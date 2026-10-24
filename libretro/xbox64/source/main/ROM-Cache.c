/**
 * Wii64 - ROM-Cache.c (Wii/GC ROM Cache)
 * Copyright (C) 2007, 2008, 2009 Mike Slegeir
 * Copyright (C) 2007, 2008, 2009, 2012 emu_kidid
 * Copyright (C) 2013 sepp256
 * 
 * This is how the ROM should be accessed, this way the ROM doesn't waste RAM
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: tehpola@gmail.com
 *                emukidid@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include "../fileBrowser/fileBrowser.h"
#include "../r4300/r4300.h"
#include "../gui/DEBUG.h"
#include "ROM-Cache.h"
#include "../xenon64_diag.h"   /* XBOX64_TRAZAS */

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#if defined(_XBOX)
/* [Xbox360] ★ LA TERCERA RAMA.
 *
 * Este fichero estaba partido en HW_RVL (MEM2 + memoria virtual paginada de
 * vm/wii_vm.c) y HW_DOL (cache de bloques de 64 KB en la ARAM).  En Xbox 360
 * no se define ninguno de los dos, asi que SIN esta rama el fichero compila
 * a casi nada: ROMCache_load devolveria sin leer, ROMCache_read no copiaria
 * nada, y el emulador arrancaria con la RDRAM a cero y sin un solo error.
 *
 * La rama de Xbox es la simple que las otras dos no podian permitirse: la
 * consola tiene 512 MB y el cartucho mas grande de N64 son 64, asi que la
 * ROM ENTERA cabe en RAM.  Ni paginacion, ni LRU, ni bloques.  Por eso
 * ROMTooBig nunca es cierto aqui y todas las ramas `if(ROMTooBig)` de abajo
 * se quedan en la parte simple. */
#include <stdlib.h>
#include "../../xbox/plataforma.h"     /* xbox64_log_line */
#define ROMCACHE_SIZE (64*1024*1024)   /* el cartucho mas grande de N64 */
#define LOAD_SIZE     (64*1024)        /* trozo de lectura, como HW_DOL  */
static char* ROMBase = NULL;
/* [Xbox360] LO QUE CAE FUERA DE LA ROM SE LEE COMO CEROS, como mupen64plus
 * (cart_rom.c, read_cart_rom: `addr < rom_size` o 0).  ROMCache_pointer
 * devolvia ROMBase + desplazamiento sin mirar el tamanyo, y fast_mem_access
 * acepta cualquier fisica desde 0x10000000: compilar (o leer) mas alla del
 * final del cartucho era leer fuera del malloc.  Se sospecho de esto por la
 * excepcion de Worms Armageddon (2026-10-24) y NO era la causa -- la sonda no
 * salto nunca: era el desborde de code_buffer, ver ppc/Recompile.c --, pero se
 * deja por ser lo de la referencia y no costar nada dentro de la ROM.  Fuera de la
 * ROM se da una pagina de ceros (0 = NOP, lo que ve mupen); `+ 16` para la
 * lectura de 8 bytes del final de la pagina.  Y la reserva lleva una pagina
 * mas a cero, para la pagina del final si la ROM no acaba en 4 KB (el dynarec
 * lee la pagina de 4 KB entera).  La DMA ya se recorta en dma.c. */
#define ROM_RELLENO   0x1000u
static char s_romCeros[0x1000 + 16];
#elif defined(HW_RVL)
#include "../gc_memory/MEM2.h"
#include "../vm/wii_vm.h"
static char* ROMBase = ROMCACHE_LO;
#else
#include "../gc_memory/ARAM.h"
#define BLOCK_SIZE  (64*1024)
#define BLOCK_MASK  (BLOCK_SIZE-1)
#define OFFSET_MASK (0xFFFFFFFF-BLOCK_MASK)
#define BLOCK_SHIFT (16)	//only change ME and BLOCK_SIZE
#define MAX_ROMSIZE (64*1024*1024)
#define NUM_BLOCKS  (MAX_ROMSIZE/BLOCK_SIZE)
#define LOAD_SIZE   (64*1024)
static char* ROMBlocks[NUM_BLOCKS];
static int   ROMBlocksLRU[NUM_BLOCKS];
static void ensure_block(u32 block);
int enableLoadIcon = 0;
#endif

void LoadingBar_showBar(float percent, const char* string);

static u32   ROMSize;
static int   ROMTooBig;
static fileBrowser_file ROMFile;
static char readBefore = 0;

static int byte_swap_type = 0;

extern void pauseAudio(void);
extern void resumeAudio(void);
extern BOOL hasLoadedROM;


void ROMCache_init(fileBrowser_file* f){
	readBefore = 0; //de-init byteswapping
	memcpy(&ROMFile, f, sizeof(fileBrowser_file));
	ROMSize = f->size;
	ROMTooBig = ROMSize > ROMCACHE_SIZE;

	romFile_seekFile(f, 0, FILE_BROWSER_SEEK_SET);	// Lets be nice and keep the file at 0.
}

void ROMCache_deinit(){
#ifdef _XBOX
	/* [Xbox360] Lo llama rom_read cuando ROMCache_load falla, y
	 * xbox/plataforma.c en cada descarga.  Tiene que ser reentrante: se
	 * puede llamar dos veces seguidas. */
	if (ROMBase) {
		free(ROMBase);
		ROMBase = NULL;
	}
#endif
#ifdef HW_RVL
	if (ROMTooBig) {
		ROMBase = ROMCACHE_LO;
		VM_Deinit();
	}
#endif
}

void* ROMCache_pointer(u32 rom_offset){
#ifdef _XBOX
	if (rom_offset >= ROMSize)
		return s_romCeros + (rom_offset & 0xFFFu);
	return ROMBase + rom_offset;
#endif
#ifdef HW_RVL
	return ROMBase + rom_offset;
#endif
#ifdef HW_DOL
#ifdef PROFILE
	start_section(ROMREAD_SECTION);
#endif
	if(ROMTooBig){
		u32 block = rom_offset >> BLOCK_SHIFT;
		u32 block_offset = rom_offset & BLOCK_MASK;
		
		ensure_block(block);
#ifdef PROFILE
		end_section(ROMREAD_SECTION);
#endif
		return ROMBlocks[block] + block_offset;
	} else {
#ifdef PROFILE
		end_section(ROMREAD_SECTION);
#endif
		return (void*)(ROMCACHE_LO + rom_offset);
	}
#endif
}

#ifdef HW_DOL
static void ROMCache_load_block(char* dst, u32 rom_offset) {
	enableLoadIcon = 1;
	u32 offset = 0, bytes_read, loads_til_update = 0;
	romFile_seekFile(&ROMFile, rom_offset, FILE_BROWSER_SEEK_SET);
	while(offset < BLOCK_SIZE){
		bytes_read = romFile_readFile(&ROMFile, dst + offset, LOAD_SIZE);
		byte_swap(dst + offset, bytes_read, byte_swap_type);
		offset += bytes_read;
		
		if(!loads_til_update--){
			loads_til_update = 32;
		}
	}
}

static void ensure_block(u32 block){
	if (block >= NUM_BLOCKS)
        return;
	if(!ROMBlocks[block]){
		// The block we're trying to read isn't in the cache
		// Find the Least Recently Used Block
		int i, max_i = 0, max_lru = 0;
		for(i=0; i<NUM_BLOCKS; ++i)
			if(ROMBlocks[i] && ROMBlocksLRU[i] > max_lru)
				max_i = i, max_lru = ROMBlocksLRU[i];
		ROMBlocks[block] = ROMBlocks[max_i]; // Take its place
		ROMCache_load_block(ROMBlocks[block], block << BLOCK_SHIFT);
		ROMBlocks[max_i] = 0; // Evict the LRU block
	}
}
#endif


void ROMCache_read(u8* ram_dest, u32 rom_offset, u32 length){
#ifdef _XBOX
	/* [Xbox360] La ROM entera esta en RAM: una copia y ya.  El guardia
	 * existe porque gc_memory/memory.c lee la ROM en el arranque, ANTES de
	 * que nadie haya comprobado que la carga fue bien. */
	if (ROMBase)
		memcpy(ram_dest, ROMBase + rom_offset, length);
#endif
#ifdef HW_RVL
	memcpy(ram_dest, ROMBase + rom_offset, length);
#endif
#ifdef HW_DOL
#ifdef PROFILE
  start_section(ROMREAD_SECTION);
#endif
  if(ROMTooBig){
		u32 block = rom_offset>>BLOCK_SHIFT;
		u32 length2 = length;
		u32 offset2 = rom_offset&BLOCK_MASK;
		
		while(length2){
			if (block >= NUM_BLOCKS) {
				return;
			}
			ensure_block(block);
			
			// Set length to the length for this block
			if(length2 > BLOCK_SIZE - offset2)
				length = BLOCK_SIZE - offset2;
			else length = length2;
		
			// Increment LRU's; set this one to 0
			int i;
			for(i=0; i<NUM_BLOCKS; ++i) ++ROMBlocksLRU[i];
			ROMBlocksLRU[block] = 0;
			
			// Actually read for this block
			memcpy(ram_dest, ROMBlocks[block] + offset2, length);
			
			// In case the read spans multiple blocks, increment state
			++block; length2 -= length; offset2 = 0; ram_dest += length; rom_offset += length;
		}
	} else {
		memcpy(ram_dest, (void*)(ROMCACHE_LO + rom_offset), length);
	}
#ifdef PROFILE
	end_section(ROMREAD_SECTION);
#endif
#endif
}

int ROMCache_load(fileBrowser_file* file){
	char txt[128];
#if defined(_XBOX)
	sprintf(txt, "Loading ROM fully into RAM");
#elif defined(HW_RVL)
	sprintf(txt, "Loading ROM %s into MEM2",ROMTooBig ? "partially" : "fully");
#else
	sprintf(txt, "Loading ROM %s into ARAM",ROMTooBig ? "partially" : "fully");
#endif

#ifdef _XBOX
	/* [Xbox360] La rama simple: reservar ROMSize y leer de una tacada.
	 *
	 * El orden lo marca el original y no es libre: el tipo de byteswap se
	 * decide con la PRIMERA palabra leida (init_byte_swap sobre ROMBase),
	 * asi que hay que leer el primer trozo ANTES de voltear nada, y
	 * despues voltear ese mismo trozo.  Si se invierte, el magic de la ROM
	 * se lee ya volteado y init_byte_swap contesta BYTE_SWAP_BAD sobre una
	 * ROM perfectamente valida. */
	{
		u32 offset = 0;
		int bytes_read;

		if (!ROMSize)
			return ROM_CACHE_ERROR_READ;

		if (ROMBase) { free(ROMBase); ROMBase = NULL; }
		/* [Worms Armageddon] La pagina de despues, a cero: ver ROM_RELLENO. */
		ROMBase = (char*)malloc(((ROMSize + 0xFFFu) & ~0xFFFu) + ROM_RELLENO);
		if (!ROMBase)
			return ROM_CACHE_ERROR_READ;
		memset(ROMBase + ROMSize, 0, (((ROMSize + 0xFFFu) & ~0xFFFu) + ROM_RELLENO) - ROMSize);

		romFile_seekFile(&ROMFile, 0, FILE_BROWSER_SEEK_SET);

		while (offset < ROMSize) {
			u32 pedir = MIN(LOAD_SIZE, ROMSize - offset);
			bytes_read = romFile_readFile(&ROMFile, ROMBase + offset, pedir);

			if (bytes_read < 0) {
				free(ROMBase); ROMBase = NULL;
				return ROM_CACHE_ERROR_READ;
			}
			if (bytes_read == 0)
				break;   /* fichero mas corto de lo que decia su tamanyo */

			if (!readBefore) {
				byte_swap_type = init_byte_swap(*(unsigned int*)ROMBase);
				if (byte_swap_type == BYTE_SWAP_BAD) {
					romFile_deinit(&ROMFile);
					free(ROMBase); ROMBase = NULL;
					return ROM_CACHE_INVALID_ROM;
				}
				readBefore = 1;
			}

			byte_swap(ROMBase + offset, bytes_read, byte_swap_type);
			offset += bytes_read;
		}

		/* [Xbox360] ★★★ Y SE DICE CUANTO SE LEYO DE VERDAD.
		 *
		 * El `break` de arriba sale del bucle en SILENCIO cuando la lectura
		 * se queda corta, y entonces el resto de `ROMBase` conserva lo que
		 * devolvio `malloc`: ni ceros siquiera, basura.  El emulador arranca
		 * igual, porque la cabecera y el codigo de arranque viven en los
		 * primeros kilobytes; lo que falla es lo que este mas arriba en el
		 * cartucho -- y en un juego por fases, eso son las fases del final.
		 *
		 * Un fallo asi no puede ser mudo.  Esta linea es el A/B entero: si
		 * `leidos` != `ROMSize`, la ROM en memoria esta truncada y no hay mas
		 * que buscar.  Si coinciden, la ROM esta entera y la causa es otra.
		 *
		 * ★ DE DONDE SALE `ROMSize`.  `need_fullpath` esta a `false`
		 * (libretro_core.cpp), asi que el frontend entrega la ROM ya en RAM
		 * y el tamanyo es el que el declara (`f->size`, que pone
		 * `xbox64_fb_set_rom_mem`).  Por el camino de ruta, que sigue
		 * existiendo, lo pone `xbox_romFile_init` con `tam_de`.  La
		 * comparacion vale igual para los dos. */
#if XBOX64_TRAZAS
		{
			char aviso[160];
			sprintf(aviso, "[xbox64] ROM: leidos %u de %u bytes%s",
			        (unsigned)offset, (unsigned)ROMSize,
			        (offset == ROMSize) ? "" : "   <<< TRUNCADA");
			xbox64_log_line(aviso);
		}
#endif

		/* [Xbox360] ★★ SE CIERRA EL FICHERO AQUI, Y NO ES UNA LIMPIEZA
		 * COSMETICA.
		 *
		 * En esta rama la ROM entera esta YA en `ROMBase`: `ROMCache_read` y
		 * `ROMCache_pointer` sirven de RAM y NADIE vuelve a tocar el fichero
		 * (el unico `romFile_readFile` del camino _XBOX es el de aqui arriba;
		 * la lectura por bloques con `ensure_block` es de HW_DOL).  O sea que
		 * a partir de este punto el descriptor es peso muerto.
		 *
		 * ★ Y PESO MUERTO QUE ESTORBA.  Con libretro la ROM llega descomprimida
		 * en un temporal del frontend (`game:\tmp\extractedRom.z64`).  Mientras
		 * el descriptor siga abierto, el XDK no deja SOBREESCRIBIR ese fichero,
		 * asi que la SIGUIENTE carga de juego falla al extraer el .zip.  Ese es
		 * el sintoma que lo destapo.
		 *
		 * ★★ Y DESDE `need_fullpath = false` ESTA LLAMADA HACE ALGO MAS, que
		 * es lo que la vuelve obligatoria en vez de higienica: por el camino
		 * de memoria es la que SUELTA el puntero al bufer del frontend.  Ese
		 * bufer muere en cuanto `retro_load_game` devuelve (Salvia lo libera
		 * al volver), y aqui arriba ya esta volcado entero en `ROMBase`.  Si
		 * este `deinit` no corriera, `filebrowser_xbox.c` se quedaria con un
		 * puntero colgando que leeria memoria liberada sin quejarse.  Ver la
		 * nota 4 de ese fichero.
		 *
		 * ★ Por que no bastaba con cerrarlo al descargar: `ROMCache_deinit()`
		 * solo libera `ROMBase`, y `romFile_deinit` NO SE LLAMA en ningun
		 * camino de exito -- solo en los tres de error (BYTE_SWAP_BAD).
		 * Upstream tiene la llamada COMENTADA en la rama HW_RVL (mas abajo),
		 * y con razon: alli el caso ROMTooBig SI sigue leyendo del fichero.
		 * Aqui no, asi que cerrar es correcto.
		 *
		 * Es idempotente: `romFile_deinit` comprueba el descriptor. */
		romFile_deinit(&ROMFile);

		return 0;
	}
#endif

#ifdef HW_RVL
	unsigned i = 0, loads_til_update = 0;
	int bytes_read;
	if (ROMTooBig) {
		void* VMBase = VM_Init(rom_length, ROMCACHE_SIZE);
		if (VMBase == NULL)
			return ROM_CACHE_ERROR_READ;
		
		ROMBase = VMBase;
	}
	do {
		bytes_read = romFile_readFile(file, ROMBase + i, 32*KB);
		if (bytes_read < 0)
			return ROM_CACHE_ERROR_READ;
		
		//initialize byteswapping if it isn't already
		if(!readBefore)
		{
			byte_swap_type = init_byte_swap(*(unsigned int*)ROMBase);
 			if(byte_swap_type == BYTE_SWAP_BAD) {
 			  romFile_deinit(&ROMFile);
 			  return ROM_CACHE_INVALID_ROM;
		  }
 			readBefore = 1;
		}
		
		byte_swap(ROMBase + i, bytes_read, byte_swap_type);
		i += bytes_read;
		
		if (!loads_til_update--) {
			LoadingBar_showBar((float)i / rom_length, txt);
			loads_til_update = 16;
		}
	} while (bytes_read > 0);
	//romFile_deinit(file);
	return 0;
#endif
#ifdef HW_DOL
	romFile_seekFile(&ROMFile, 0, FILE_BROWSER_SEEK_SET);
	
	u32 offset = 0,loads_til_update = 0;
	int bytes_read;
	u32 sizeToLoad = MIN(ROMCACHE_SIZE, ROMSize);
	while(offset < sizeToLoad){
		bytes_read = romFile_readFile(&ROMFile, (void*)(ROMCACHE_LO + offset), LOAD_SIZE);
		
		if(bytes_read < 0){		// Read fail!
			return ROM_CACHE_ERROR_READ;
		}
		//initialize byteswapping if it isn't already
		if(!readBefore)
		{
			byte_swap_type = init_byte_swap(*(unsigned int*)ROMCACHE_LO);
 			if(byte_swap_type == BYTE_SWAP_BAD) {
 			  romFile_deinit(&ROMFile);
 			  return ROM_CACHE_INVALID_ROM;
		  }
 			readBefore = 1;
		}
		//byteswap
		byte_swap((char*)(ROMCACHE_LO + offset), bytes_read, byte_swap_type);
		
		offset += bytes_read;
		
		if(!loads_til_update--){
			LoadingBar_showBar((float)offset/sizeToLoad, txt);
			loads_til_update = 16;
		}
	}
	
	if(ROMTooBig){ // Set block pointers if we need to
		int i;
		for(i=0; i<ROMCACHE_SIZE/BLOCK_SIZE; ++i)
			ROMBlocks[i] = (void*)(ROMCACHE_LO + (i*BLOCK_SIZE));
		for(; i<ROMSize/BLOCK_SIZE; ++i)
			ROMBlocks[i] = 0;
		for(i=0; i<ROMSize/BLOCK_SIZE; ++i)
			ROMBlocksLRU[i] = i;
	}
	return 0;
#endif
}



