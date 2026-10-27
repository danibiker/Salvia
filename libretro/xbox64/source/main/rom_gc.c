/**
 * Mupen64 - rom_gc.c
 * Copyright (C) 2002 Hacktarux,
 * Wii/GC Additional code by tehpola, emu_kidid
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 *
 * If you want to contribute to the project please contact
 * me first (maybe someone is already making what you are
 * planning to do).
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
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ogc/machine/processor.h>
#include "rom.h"
#include "ROM-Cache.h"
#include "wii64config.h"
#include "../gc_memory/memory.h"
#include "../fileBrowser/fileBrowser.h"
#include "gamehacks.h"

#define PRINT GUI_print

static fileBrowser_file* rom_file;
int rom_length;
rom_header ROM_HEADER;
rom_settings ROM_SETTINGS;

extern void init_vi_clock_refresh_rate();

void stripInvalidChars(char *str) {
	int i = 0;
	for(i = 0; i < strlen(str); i++) {
		if(str[i] == '\\' || str[i] == '/' || str[i] == ':'|| str[i] == '*'
		|| str[i] == '?'|| str[i] == '"'|| str[i] == '<'|| str[i] == '>'|| str[i] == '|') {
			str[i] = '_';
		}
	}
}

int init_byte_swap(unsigned int magicWord){

#ifndef _BIG_ENDIAN
	/* [Windows, 2026-10-26] La primera palabra se lee como u32 del ANFITRION
	 * (ROM-Cache.c), y aqui la ROM tiene que acabar en palabras del anfitrion
	 * (el modelo de memoria de mupen64 en little-endian):
	 *   .z64  80 37 12 40 -> 0x40123780: voltear cada palabra (HALF, __lwbrx)
	 *   .n64  40 12 37 80 -> 0x80371240: ya esta (NONE)
	 *   .v64  37 80 40 12 -> 0x12408037: rotar 16 cada palabra (WORD16)
	 * La tabla de abajo es la de big-endian, que en PC dejaba fuera las .v64. */
	switch(magicWord){
		case 0x40123780:
			return BYTE_SWAP_HALF;
		case 0x80371240:
			return BYTE_SWAP_NONE;
		case 0x12408037:
			return BYTE_SWAP_WORD16;
	}
	return BYTE_SWAP_BAD;
#else
	switch(magicWord){
		case 0x37804012:					//37804012 aka byteswapped
			return BYTE_SWAP_BYTE;
		case 0x40123780:					//40123780 aka little endian, aka halfswapped
			return BYTE_SWAP_HALF;
		case 0x80371240:
			return BYTE_SWAP_NONE;
	}
	return BYTE_SWAP_BAD;
#endif
}

void byte_swap(char* buffer, unsigned int length, int byte_swap_type) {
	int i = 0;   /* [Xbox360] hoisteada: C89 */

	if(byte_swap_type == BYTE_SWAP_NONE || byte_swap_type == BYTE_SWAP_BAD)
		return;

	if(byte_swap_type == BYTE_SWAP_HALF){	//aka little endian (40123780) vs (80371240)
		for (i = 0; i < (length & ~3); i += 4)
			*(u32 *)(buffer + i) = __lwbrx(buffer, i);
	} else if(byte_swap_type == BYTE_SWAP_BYTE){	// (37804012) vs (80371240)
		for (i = 0; i < (length & ~3); i += 2)
			*(u16 *)(buffer + i) = __lhbrx(buffer, i);
	}
#ifndef _BIG_ENDIAN
	else if(byte_swap_type == BYTE_SWAP_WORD16){	/* [Windows] .v64: ver init_byte_swap */
		for (i = 0; i < (length & ~3); i += 4) {
			const u32 w = *(u32 *)(buffer + i);
			*(u32 *)(buffer + i) = (w << 16) | (w >> 16);
		}
	}
#endif
}

static struct {
	int Cartridge_ID;
	char* Country_codes;
} ROM_TABLE[] = {
	{ 'D3', "J",   }, // Akumajou Dracula Mokushiroku - Real Action Adventure
	{ 'D4', "J",   }, // Akumajou Dracula Mokushiroku Gaiden - Legend of Cornell
	{ 'B7', "EJPU" }, // Banjo-Tooie
	{ 'GT', "J",   }, // City Tour Grandprix - Zennihon GT Senshuken
	{ 'FU', "EP",  }, // Conker's Bad Fur Day
	{ 'CW', "EP"   }, // Cruis'n World
	{ 'CZ', "J",   }, // Custom Robo V2
	{ 'D6', "J",   }, // Densha de GO! 64
	{ 'DO', "EJP"  }, // Donkey Kong 64
	{ 'D2', "J",   }, // Doraemon 2 - Nobita to Hikari no Shinden
	{ '3D', "J",   }, // Doraemon 3 - Nobita no Machi SOS!
	{ 'MX', "EJP"  }, // Excitebike 64
	{ 'X7', "E"    }, // GoldenEye X 5d
	{ 'GC', "EP"   }, // GT 64 - Championship Edition
	{ 'IM', "J",   }, // Ide Yousuke no Mahjong Juku
	{ 'K4', "EJP"  }, // Kirby 64 - The Crystal Shards
	{ 'NB', "EP"   }, // Kobe Bryant in NBA Courtside
	{ 'MV', "EJP"  }, // Mario Party 3
	{ 'M8', "EJP"  }, // Mario Tennis
	{ 'EV', "J"    }, // Neon Genesis Evangelion
	{ 'PP', "J"    }, // Parlor! Pro 64 - Pachinko Jikki Simulation Game
	{ 'UB', "J"    }, // PD Ultraman Battle Collection 64
	{ 'PD', "EJP"  }, // Perfect Dark
	{ 'R7', "J"    }, // Robot Ponkottsu 64 - 7tsu no Umi no Caramel
	{ 'RZ', "EP"   }, // RR64 - Ridge Racer 64
	{ 'EP', "EJP"  }, // Star Wars Episode I - Racer
	{ 'YS', "EJP"  }, // Yoshi's Story
};

// Checks if the current game is in the ID list for 16kbit eeprom save type
// cause it's cheaper to have a ID list than an entire .ini file :)
bool isEEPROM16k()
{
	int i;

	for (i = 0; i < sizeof(ROM_TABLE) / sizeof(ROM_TABLE[0]); i++)
	{
		if (ROM_TABLE[i].Cartridge_ID == ROM_HEADER.Cartridge_ID &&
			strchr(ROM_TABLE[i].Country_codes, ROM_HEADER.Country_code))
			return true;
	}

	return false;
}

#ifndef _BIG_ENDIAN
/* [Windows, 2026-10-26] LA CABECERA EN LITTLE-ENDIAN.  La ROM esta en palabras
 * de 32 bits del orden del anfitrion (ROM-Cache.c), asi que el ROMCache_read
 * de rom_read deja ROM_HEADER con los campos de 32 bits bien y los de byte y
 * media palabra (el nombre, Cartridge_ID, Country_code) revueltos.  En
 * big-endian los dos ordenes coinciden y esto no existe.
 *   - Los plugins reciben la cabecera COMO LA ROM (`rom_cabecera_plugins`),
 *     que es lo que les da mupen64 en little-endian: Rice la voltea el mismo
 *     (Video.cpp, `#ifndef _BIG_ENDIAN`) y GLideN64 lee el nombre con ^3.
 *   - El nucleo usa ROM_HEADER como struct de verdad: se rehace leyendo los
 *     bytes del cartucho en su orden (k ^ 3) y montando cada campo. */
unsigned char rom_cabecera_plugins[sizeof(rom_header)];

static unsigned int cabecera_u32(const unsigned char *b, int o)
{
	return ((unsigned int)b[o] << 24) | ((unsigned int)b[o + 1] << 16) |
	       ((unsigned int)b[o + 2] << 8) | (unsigned int)b[o + 3];
}

static void rom_cabecera_a_struct(void)
{
	unsigned char b[sizeof(rom_header)];
	unsigned int k;

	memcpy(rom_cabecera_plugins, &ROM_HEADER, sizeof(rom_header));
	for (k = 0; k < sizeof(rom_header); k++)
		b[k] = rom_cabecera_plugins[k ^ 3];

	ROM_HEADER.init_PI_BSB_DOM1_LAT_REG  = b[0];
	ROM_HEADER.init_PI_BSB_DOM1_PGS_REG  = b[1];
	ROM_HEADER.init_PI_BSB_DOM1_PWD_REG  = b[2];
	ROM_HEADER.init_PI_BSB_DOM1_PGS_REG2 = b[3];
	ROM_HEADER.ClockRate  = cabecera_u32(b, 4);
	ROM_HEADER.PC         = cabecera_u32(b, 8);
	ROM_HEADER.Release    = cabecera_u32(b, 12);
	ROM_HEADER.CRC1       = cabecera_u32(b, 16);
	ROM_HEADER.CRC2       = cabecera_u32(b, 20);
	ROM_HEADER.Unknown[0] = cabecera_u32(b, 24);
	ROM_HEADER.Unknown[1] = cabecera_u32(b, 28);
	memcpy(ROM_HEADER.Name, b + 32, 20);
	ROM_HEADER.unknown         = cabecera_u32(b, 52);
	ROM_HEADER.Manufacturer_ID = cabecera_u32(b, 56);
	ROM_HEADER.Cartridge_ID    = (unsigned short)(((unsigned int)b[60] << 8) | b[61]);
	ROM_HEADER.Country_code    = b[62];
	ROM_HEADER.Version         = b[63];
}
#endif

/* Loads the ROM into the ROM cache */
int rom_read(fileBrowser_file* file){

   char buffer[1024];
   int i;
   int ret;   /* [Xbox360] hoisteada: C89 */

   romFile_readFile(file, buffer, 32);	// dummy read to set up the file/size.
   rom_file = file;
   rom_length = file->size;

   ROMCache_init(rom_file);
   ret = ROMCache_load(rom_file);
   if(ret) {
     ROMCache_deinit();
     return ret;
   }
   ROMCache_read((u8*)&ROM_HEADER, 0, sizeof(rom_header));
#ifndef _BIG_ENDIAN
   rom_cabecera_a_struct();   /* [Windows] ver arriba */
#endif

  //Copy header name as Goodname (in the .ini we can use CRC to identify ROMS)
  memset((char*)buffer,0,1024);
  strncpy(buffer, (char*)ROM_HEADER.Name,20);
  //Maximum ROM name is 20 bytes. Lets make sure we cut off trailing spaces
  for(i = strlen(buffer); i>0; i--)
  {
    if(buffer[i-1] !=  ' ') {
  		strncpy(&ROM_SETTINGS.goodname[0],&buffer[0],i);
  		ROM_SETTINGS.goodname[i] = 0; //terminate it too
  		break;
    }
  }
  // Replace any non file system complaint chars with underscores
  stripInvalidChars((char*)&ROM_SETTINGS.goodname[0]);
  // Fix save type for certain special sized (16kbit) eeprom games
  ROM_SETTINGS.eeprom_16kb = isEEPROM16k();
  // Apply game specific hacks
  GameSpecificHackSetup();
#ifdef GLN64_GX
/* [Xbox360] La guarda original es `#ifdef HW_RVL`, o sea que este arreglo
 * solo esta activo en el build de Wii -- que es EL BUILD QUE FUNCIONA y del
 * que se porta.  En Xbox 360 HW_RVL no se define, asi que una copia literal
 * lo perderia en silencio: no habria error, solo la pantalla del cartel de
 * Mario Kart 64 en negro, y a saber cuando se ata eso con esta linea.
 *
 * Se anyade _XBOX a la condicion en vez de quitarla para que el build de
 * GameCube siga comportandose igual que en el original.
 *
 * ================================================================
 * ★★★ LO DE ARRIBA ESTABA MAL, Y COSTO UN CUELGUE.  (fase 4)
 * ================================================================
 * Mario Kart 64 reventaba nada mas arrancar:
 *
 *     Access violation reading location 0x00000000
 *     gDPUpdateColorImage()  gDP.cpp:374
 *       <- gDPSetColorImage  <- RSP_ProcessDList  <- do_SP_task
 *
 * El razonamiento de arriba mira solo la MITAD: acerte en que no hay
 * que perder el arreglo por juego en silencio, y no comprobe que la
 * IMPLEMENTACION que ese interruptor enciende se pueda ejecutar aqui.
 *
 *     gDP.cpp:374   u16 *xfb = (u16*)VI.xfb[VI.which_fb];
 *
 * `VI.xfb[]` es el framebuffer EXTERNO de la Wii, en YUYV, y lo rellena
 * `VI_GX_setFB` <- `gfx_set_fb` <- el menu de Wii64.  Ese camino NO esta
 * portado (censado: `VI_GX_setFB` no lo llama nadie en todo el arbol,
 * y `gln64_glue.cpp:336` ya lo daba por ausente).  O sea que los dos
 * punteros valen NULL y la funcion los desreferencia sin mirar.
 *
 * ★ LA LECCION, que es la regla de la lista entera vista del otro lado:
 * encender un arreglo por juego cuyo CODIGO no esta portado es PEOR que
 * no encenderlo.  Convierte un efecto que falta -- cosmetico, visible,
 * anotable -- en un cuelgue en el arranque.  Antes de activar un
 * `#if` por plataforma hay que seguir la cadena hasta el codigo que
 * ejecuta, no solo hasta la variable.
 *
 * ★ QUE SE PIERDE: el cartel animado del circuito de Mario Kart 64 (la
 * pantalla que muestra la carrera en directo).  Se queda estatico.  Lo
 * que haria falta para tenerlo esta anotado en FASE4.txt: leer de vuelta
 * nuestro render target de D3D en vez del XFB de la Wii.
 *
 * Se vuelve a la condicion de upstream.  Con ella, `enablegDPUpdateColorImage`
 * se queda en su valor inicial (gDP.cpp:62 lo define a 0), que es
 * exactamente lo que hace cualquier build que no sea el de Wii. */
#if defined(HW_RVL)
	// Makes the Mario Kart 64 in-game billboard screen work
  if(ROM_HEADER.CRC1 == 0xC3B6DE9D ||
    ROM_HEADER.CRC1 == 0x2577C7D4 || 
    ROM_HEADER.CRC1 == 0x6BFF4758 || 
    ROM_HEADER.CRC1 == 0xC9C3A987 || 
    ROM_HEADER.CRC1 == 0x3E5055B6) {
      enablegDPUpdateColorImage = 1;
  }
  else {
    enablegDPUpdateColorImage = 0;	
  }
#endif
#endif
  //Set VI limit based on ROM header
  InitTimer();
  init_vi_clock_refresh_rate();
	
  return ret;
}

#define tr 
void countrycodestring(unsigned short countrycode, char *string)
{
    switch (countrycode)
    {
    case 0:    /* Demo */
        strcpy(string, ("Demo"));
        break;

    case '7':  /* Beta */
        strcpy(string, ("Beta"));
        break;

    case 0x41: /* Japan / USA */
        strcpy(string, ("USA/Japan"));
        break;

    case 0x44: /* Germany */
        strcpy(string, ("Germany"));
        break;

    case 0x45: /* USA */
        strcpy(string, ("USA"));
        break;

    case 0x46: /* France */
        strcpy(string, ("France"));
        break;

    case 'I':  /* Italy */
        strcpy(string, ("Italy"));
        break;

    case 0x4A: /* Japan */
        strcpy(string, ("Japan"));
        break;

    case 'S':  /* Spain */
        strcpy(string, ("Spain"));
        break;

    case 0x55: case 0x59:  /* Australia */
        sprintf(string, ("Australia (0x%2.2X)"), countrycode);
        break;

    case 0x50: case 0x58: case 0x20:
    case 0x21: case 0x38: case 0x70:
        sprintf(string, ("Europe (0x%02X)"), countrycode);
        break;

    default:
        sprintf(string, ("Unknown (0x%02X)"), countrycode);
        break;
    }
}

char *saveregionstr()
{
    switch (ROM_HEADER.Country_code&0xFF)
    {
    case 0:    /* Demo */
        return "(Demo)";
        break;
    case '7':  /* Beta */
        return "(Beta)";
        break;
    case 0x41: /* Japan / USA */
        return "(JU)";
        break;
    case 0x44: /* Germany */
        return "(G)";
        break;
    case 0x45: /* USA */
        return "(U)";
        break;
    case 0x46: /* France */
        return "(F)";
        break;
    case 'I':  /* Italy */
        return "(I)";
        break;
    case 0x4A: /* Japan */
        return "(J)";
        break;
    case 'S':  /* Spain */
        return "(S)";
        break;
    case 0x55: case 0x59:  /* Australia */
        return "(A)";
        break;
    case 0x50: case 0x58: case 0x20:
    case 0x21: case 0x38: case 0x70:
        return "(E)";
        break;
    default:
        return "(Unk)";
        break;
    }
}

