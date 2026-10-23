/**
 * Wii64 - gamehacks.c
 * Copyright (C) 2012 emu_kidid
 * 
 * Game specific hacks to workaround core inaccuracy
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: emukidid@gmail.com
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
#include "winlnxdefs.h"
#include "rom.h"
#include "../gc_memory/memory.h"
#include "ROM-Cache.h"
#include "../r4300/r4300.h"
#include "wii64config.h"
#include "../../xbox/plataforma.h"
#include "../../xbox/hilo_lista.h"   /* [Cruis'n USA] hilo_lista_sp_db */

/* ★★ `RICE_GFX` ERA UN DEFINE DE COMPILACION, Y AQUI NO PUEDE SERLO.
 *
 * Wii64 construye un binario por plugin (`Makefile.Rice_wii` anyade
 * `-DRICE_GFX`), asi que podia decidir con el preprocesador.  En este core
 * los dos plugins estan enlazados y la eleccion se hace al cargar la ROM, asi
 * que la misma decision tiene que tomarse en ejecucion.
 *
 * Esta funcion corre UNA VEZ por carga de ROM, asi que consultar el plugin
 * activo aqui no cuesta nada. */
#define RICE_ACTIVO (xbox64_gfx_plugin_activo() == XBOX64_GFX_RICE)

static void *game_specific_hack = 0;
static u32 zelda_subscreen_address = 0;

void compare_hword_write_hword(u32 compAddr, u32 destAddr, u16 compVal, u16 destVal) {
	address = compAddr;
	read_hword_in_memory();
	if(hword == compVal) {
		address = destAddr;
		hword = destVal;
		write_hword_in_memory();
	}
}

void compare_hword_write_byte(u32 compAddr, u32 destAddr, u16 compVal, u8 destVal) {
	address = compAddr;
	read_hword_in_memory();
	if(hword == compVal) {
		address = destAddr;
		byte = destVal;
		write_byte_in_memory();
	}
}

void compare_byte_write_byte(u32 addr, u8 compVal, u8 destVal) {
	address = addr;
	read_byte_in_memory();
	if(byte == compVal) {
		byte = destVal;
		write_byte_in_memory();
	}
}

// Pokemon Snap (U)
void hack_pkm_snap_u() {
	// Pass 1st Level and Controller Fix
	compare_hword_write_byte(0x80382D1C, 0x80382D0F, 0x802C, 0);

	//Make Picture selectable
	compare_hword_write_hword(0x801E3184, 0x801E3184, 0x2881, 0x2001);
	compare_hword_write_hword(0x801E3186, 0x801E3186, 0x0098, 0x0001);
}


// Pokemon Snap (A)
void hack_pkm_snap_a() {
	// Pass 1st Level and Controller Fix
	compare_hword_write_byte(0x80382D1C, 0x80382D0F, 0x802C, 0);

	//Make Picture selectable
	compare_hword_write_hword(0x801E3C44, 0x801E3C44, 0x2881, 0x2001);
	compare_hword_write_hword(0x801E3C46, 0x801E3C46, 0x0098, 0x0001);
}

// Pokemon Snap (E)
void hack_pkm_snap_e() {
	// Pass 1st Level and Controller Fix
	compare_hword_write_byte(0x80381BFC, 0x80381BEF, 0x802C, 0);

	//Make Picture selectable
	compare_hword_write_hword(0x801E3824, 0x801E3824, 0x2881, 0x2001);
	compare_hword_write_hword(0x801E3826, 0x801E3826, 0x0098, 0x0001);
}

// Pocket Monsters Snap (J)
void hack_pkm_snap_j() {
	// Pass 1st Level and Controller Fix
	compare_hword_write_byte(0x8036D22C, 0x8036D21F, 0x802A, 0);

	//Make Picture selectable
	compare_hword_write_hword(0x801E1EC4, 0x801E1EC4, 0x2881, 0x2001);
	compare_hword_write_hword(0x801E1EC6, 0x801E1EC6, 0x0098, 0x0001);
}

// Top Gear Hyper-Bike (E)
void hack_topgear_hb_e() {
	//Game Playable Fix (Gent)
	compare_hword_write_byte(0x800021EE, 0x800021EE, 0x0001, 0);
}

// Top Gear Hyper-Bike (J)
void hack_topgear_hb_j() {
	//Game Playable Fix (Gent)
	compare_hword_write_byte(0x8000225A, 0x8000225A, 0x0001, 0);
}

// Top Gear Hyper-Bike (U)
void hack_topgear_hb_u() {
	//Game Playable Fix (Gent)
	compare_hword_write_hword(0x800021EA, 0x800021EA, 0x0001, 0);
}

// Top Gear Overdrive (E)
void hack_topgear_od_e() {
	//Game Playable Fix (Gent)
	compare_hword_write_hword(0x80001AB2, 0x80001AB2, 0x0001, 0);
}

// Top Gear Overdrive (J)
void hack_topgear_od_j() {
	//Game Playable Fix (Gent)
	hword = 0;
	address = 0x800F7BD4;
	write_hword_in_memory();
	address = 0x800F7BD6;
	write_hword_in_memory();
	address = 0x80001B4E;
	write_hword_in_memory();
}

// Top Gear Overdrive (U)
void hack_topgear_od_u() {
	//Game Playable Fix (Gent)
	compare_hword_write_hword(0x80001B4E, 0x80001B4E, 0x0001, 0);
}


void hack_paperboy() {
	// NOP
}

void hack_winback() {
	//NOP
}

void hack_dukenukem() {
	// NOP
}

void hack_scars() {
	// NOP
}

void hack_ogrebattle() {
	// NOP
}

void hack_yoshistory() {
	// NOP
}

/* [Xbox360] ★ [Pilotwings 64] SIN PARCHE.  Aqui estaban hack_pilotwings_u/e/j
 * de Wii64 ("Removes annoying shadow that appears below planes"), que
 * borraban de la RAM el modelo de la sombra de los aparatos (y que en su
 * forma original colgaban el juego al terminar la fase).  La sombra ya se
 * emula: el juego dibuja la silueta CON el color image = depth image, y
 * glN64_GX lo trata ahora como GLideN64 ccff245f "Implement render to depth
 * buffer. Fixed shadows in Pilot Wings" (final de OGL_UpdateStates y
 * GXD3D_SetRenderToDepth del shim).  GLideN64 retiro tambien su
 * hack_pilotWings (81cc0854).  Confirmado por el usuario (2026-10-05). */

void hack_zelda_oot() {
	if(zelda_subscreen_address) {
		rdramb[zelda_subscreen_address] = 2;
	}
}


/* [Xbox360] ★ INTERRUPTOR DE LOS PARCHES POR JUEGO.
 *
 * `timers.c` llama al parche del juego UNA VEZ POR FRAME, y los parches
 * escriben en direcciones fijas de la RAM del invitado.  Eso los hace utiles
 * pero tambien opacos: mientras estan puestos no se puede ver el defecto que
 * tapan, ni saber si siguen haciendo falta.
 *
 * El caso que lo pidio fue Pilotwings 64: su parche quitaba la sombra de los
 * aviones, asi que cualquier prueba SOBRE esa sombra daba igual con el parche
 * activo.  Sin poder apagarlo no se pudo ni caracterizar el defecto (el
 * parche ya no existe: ver la nota de Pilotwings mas arriba).
 *
 * Por defecto ENCENDIDO: el comportamiento no cambia salvo que el usuario lo
 * apague a proposito. */
static int s_hacks_habilitados = 1;

void xbox64_gamehacks_habilitar(int on) {
	s_hacks_habilitados = on ? 1 : 0;
}

// Return a pointer to the game specific hack or 0 if there isn't any
void *GetGameSpecificHack() {
	return s_hacks_habilitados ? game_specific_hack : 0;
}

static int old_count_per_op = -1;

void restore_count_per_op() {
	if(old_count_per_op != -1) {
		count_per_op = old_count_per_op;
	}
}

// Game specific hack detection via CRC
void GameSpecificHackSetup() {
	unsigned int curCRC[2];
	ROMCache_read((u8*)&curCRC[0], 0x10, sizeof(unsigned int)*2);
	//print_gecko("CRC values %08X %08X\r\n", curCRC[0], curCRC[1]);

	/* [Xbox360] ★ [Conker, logs 13-16] VELOCIDAD DE CPU 1 PARA CONKER.
	 *
	 * Con la de Wii64 (count_per_op = 2) Conker se queda parado para siempre:
	 * la foto `[LISTA3]` del log 16 (sonda ya retirada) dio el COMPARE_INT VENCIDO
	 * (Compare=4DDB953B, Count=53D89932), esperando al giro de Count (~91 s).
	 * Medido con una sonda en el MTC0 Compare: el juego escribe temporizadores
	 * ya vencidos por ~260.000 ciclos, porque entre leer Count y escribir
	 * Compare hace mucho trabajo, y con 2 cada instruccion avanza Count el
	 * doble.  Con 1 no se paro en ninguna prueba (logs 14 y 15) y va mas
	 * fluido.  La referencia (mupen64plus.ini) no le pone CountPerOp.
	 *
	 * Va por `xbox64_db_count_per_op` y no escribiendo `count_per_op`: asi
	 * sobrevive a un cambio de opciones en partida, y solo manda si el
	 * usuario dejo la opcion por defecto.  Para todos los demas juegos se
	 * pone a 0 aqui mismo, en cada carga.
	 *
	 * [Fighting Force 64, log 44] Este SI lo trae la referencia:
	 * mupen64plus.ini le pone `CountPerOp=1` en (E) y (U).  Con 2 el juego iba
	 * a 3-10 img/s con 60 VI/s y la mitad del frame libre: cada VI le daba la
	 * mitad de instrucciones.
	 *
	 * [Resident Evil 2, logs 9-13] Tambien lo trae la referencia: mupen64plus.ini
	 * `CountPerOp=1` en las cuatro (J, E, U 1.0 y U 1.1).  Con 2, a 60 VI/s y
	 * con la imagen ya bien, el juego se quedaba en negro tras la intro: la
	 * sonda [HILOS] dio todos los hilos esperando con normalidad (planificador,
	 * VI, PI, cargador) sin ningun fallo y sin nadie que lanzara trabajo nuevo,
	 * la huella de Conker. */
	if((curCRC[0] == 0x30C7AC50 && curCRC[1] == 0x7704072D) ||   // Conker's Bad Fur Day (U)
	   (curCRC[0] == 0x373F5889 && curCRC[1] == 0x9A6CA80A) ||   // Conker's Bad Fur Day (E)
	   (curCRC[0] == 0x66CF0FFE && curCRC[1] == 0xAD697F9C) ||   // Fighting Force 64 (E)
	   (curCRC[0] == 0x32EFC7CB && curCRC[1] == 0xC3EA3F20) ||   // Fighting Force 64 (U)
	   (curCRC[0] == 0x7EAE2488 && curCRC[1] == 0x9D40A35A) ||   // Biohazard 2 (J)
	   (curCRC[0] == 0x9B500E8E && curCRC[1] == 0xE90550B3) ||   // Resident Evil 2 (E)
	   (curCRC[0] == 0x2F493DD0 && curCRC[1] == 0x2E64DFD9) ||   // Resident Evil 2 (U) (V1.0)
	   (curCRC[0] == 0xAA18B1A5 && curCRC[1] == 0x07DB6AEB))     // Resident Evil 2 (U) (V1.1)
		xbox64_db_count_per_op(COUNT_PER_OP_1);
	else
		xbox64_db_count_per_op(0);

	/* [Xbox360] ★ [Cruis'n USA, logs 12, 19 y 20] "ESPERAR AL RSP" EN "FOTO".
	 *
	 * Con la lista en paralelo el juego reescribe vertices y matrices en
	 * cuanto recibe el SP_INT, antes de que el hilo de la lista los lea:
	 * destellos y poligonos que faltan.  "activado" lo arregla esperando a la
	 * lista entera, y el juego pierde su cadencia de 30 (`img` 15-29, log 12).
	 * "foto" lo arregla copiando la RDRAM al empezar la lista: sin destellos
	 * (confirmado por el usuario, log 19) y `img` 30 estable con el DP sin
	 * esperar a la lista (log 20).  Las cuatro revisiones, con los CRC de
	 * mupen64plus.ini.  Solo manda si la opcion sigue en "desactivado"; para
	 * todos los demas juegos se pone a 0 aqui mismo, en cada carga.
	 *
	 * [Blast Corps, logs 15-18] "ESPERAR AL RSP" EN "ACTIVADO".  En "en el
	 * acto" el juego reescribe la memoria de la lista tras el SP_INT: errores
	 * graficos y se cuelga (pc 80244908 parado, log 15).  "activado" (plazo
	 * del SP = el de la lista, xbox/hilo_lista.cpp) va a ~30 imagenes/s (log
	 * 17), mas que sin lista en paralelo (25-28).  Medido con (U) (V1.1,
	 * Rev A); las demas revisiones, con los CRC de mupen64plus.ini.
	 *
	 * [1080 Snowboarding, 2026-10-22] YA NO VA EN "FOTO", a peticion del
	 * usuario (iba desde el 2026-10-16): necesita la copia de Z y la del color
	 * a la RDRAM (RSP.cpp), y con foto nada de lo que escribe el plugin llega
	 * al juego.  Earthworm Jim 3D (2026-10-16) sigue en "foto", a peticion
	 * del usuario; los volcados [!].
	 *
	 * [Quake 64, Quake II, Rayman 2, Turok - Dinosaur Hunter, Turok 2,
	 * Turok 3 y Donald Duck, 2026-10-16] "ACTIVADO", a peticion del usuario,
	 * como Blast Corps.  CRC de mupen64plus.ini, los volcados [!]. */
	if((curCRC[0] == 0x503EA760 && curCRC[1] == 0xE1300E96) ||   // Cruis'n USA (E)
	   (curCRC[0] == 0xFF2F2FB4 && curCRC[1] == 0xD161149A) ||   // Cruis'n USA (U) (V1.0)
	   (curCRC[0] == 0x5306CF45 && curCRC[1] == 0xCBC49250) ||   // Cruis'n USA (U) (V1.1)
	   (curCRC[0] == 0xB3402554 && curCRC[1] == 0x7340C004) ||   // Cruis'n USA (U) (V1.2, Rev B)
	   (curCRC[0] == 0x492B9DE8 && curCRC[1] == 0xC6CCC81C) ||   // Earthworm Jim 3D (E) (M6)
	   (curCRC[0] == 0xDF574191 && curCRC[1] == 0x9EB5123D))     // Earthworm Jim 3D (U)
		hilo_lista_sp_db(2);
	else if((curCRC[0] == 0x7C64E6DB && curCRC[1] == 0x55B924DB) ||   // Blast Corps (E) (M2)
	        (curCRC[0] == 0x7C647C25 && curCRC[1] == 0xD9D901E6) ||   // Blast Corps (U) (V1.0)
	        (curCRC[0] == 0x7C647E65 && curCRC[1] == 0x1948D305) ||   // Blast Corps (U) (V1.1, Rev A)
	        (curCRC[0] == 0x65234451 && curCRC[1] == 0xEBD3346F) ||   // Blast Dozer (J)
	        (curCRC[0] == 0x16931D74 && curCRC[1] == 0x65DC6D34) ||   // Quake 64 (E)
	        (curCRC[0] == 0x9F5BF79C && curCRC[1] == 0xD2FE08A0) ||   // Quake 64 (U)
	        (curCRC[0] == 0x7433D9D7 && curCRC[1] == 0x2C4322D0) ||   // Quake II (E)
	        (curCRC[0] == 0xBDA8F143 && curCRC[1] == 0xB1AF2D62) ||   // Quake II (U)
	        (curCRC[0] == 0x60D5E10B && curCRC[1] == 0x8BEDED46) ||   // Rayman 2 (E) (M5)
	        (curCRC[0] == 0xF3C5BF9B && curCRC[1] == 0x160F33E2) ||   // Rayman 2 (U) (M5)
	        (curCRC[0] == 0x2F7009DD && curCRC[1] == 0xFC3BAC53) ||   // Turok - Dinosaur Hunter (E) (V1.0)
	        (curCRC[0] == 0x2F700DCD && curCRC[1] == 0x176CC5C9) ||   // Turok - Dinosaur Hunter (E/U) (V1.1, V1.2)
	        (curCRC[0] == 0x2F70F10D && curCRC[1] == 0x5C4187FF) ||   // Turok - Dinosaur Hunter (U) (V1.0)
	        (curCRC[0] == 0x665FD963 && curCRC[1] == 0xB5CC6612) ||   // Turok - Dinosaur Hunter (G) (V1.0)
	        (curCRC[0] == 0x665FC793 && curCRC[1] == 0x6934A73B) ||   // Turok - Dinosaur Hunter (G) (V1.1, V1.2)
	        (curCRC[0] == 0xE0B92B94 && curCRC[1] == 0x80E87CBD) ||   // Turok 2 - Seeds of Evil (E)
	        (curCRC[0] == 0x2E0E7749 && curCRC[1] == 0xB8B49D59) ||   // Turok 2 - Seeds of Evil (E) (M4)
	        (curCRC[0] == 0x49088A11 && curCRC[1] == 0x6494957E) ||   // Turok 2 - Seeds of Evil (U) (V1.0)
	        (curCRC[0] == 0xE0B92B94 && curCRC[1] == 0xB9A7E025) ||   // Turok 2 - Seeds of Evil (U) (V1.1)
	        (curCRC[0] == 0xFE05840B && curCRC[1] == 0x9393320C) ||   // Turok 2 - Seeds of Evil (G)
	        (curCRC[0] == 0xE8C95AFC && curCRC[1] == 0x35D121DA) ||   // Turok 2 - Seeds of Evil (E/U) (Kiosk Demo)
	        (curCRC[0] == 0x6A162FF2 && curCRC[1] == 0x2093704C) ||   // Turok 3 - Shadow of Oblivion (E)
	        (curCRC[0] == 0x89A579F1 && curCRC[1] == 0x667E97EF) ||   // Turok 3 - Shadow of Oblivion (U)
	        (curCRC[0] == 0xC16C421B && curCRC[1] == 0xA21580F7) ||   // Donald Duck - Goin' Quackers (U)
	        (curCRC[0] == 0x3DF17480 && curCRC[1] == 0x193DED5A))     // Donald Duck - Quack Attack (E) (M5)
		hilo_lista_sp_db(1);
	else
		hilo_lista_sp_db(0);

	/* [Xbox360] [Jet Force Gemini] EMULACION DE FRAMEBUFFER.  Sin ella la
	 * silueta de la sombra, que el juego pinta en un buffer auxiliar, se queda
	 * en la esquina superior izquierda y la pantalla parpadea a negro; con ella
	 * va bien (comprobado por el usuario).  GLideN64 tambien lo trata por juego
	 * en su ini.  Solo enciende si la opcion sigue en "desactivado", que es su
	 * valor por defecto; ver xbox64_db_framebuffer en xbox/plataforma.c.  CRC
	 * de mupen64plus.ini, volcados buenos. */
	if((curCRC[0] == 0x68D7A1DE && curCRC[1] == 0x0079834A) ||   // Jet Force Gemini (E) (M4)
	   (curCRC[0] == 0x8A6009B6 && curCRC[1] == 0x94ACE150) ||   // Jet Force Gemini (U)
	   (curCRC[0] == 0xDFD8AB47 && curCRC[1] == 0x3CDBEB89) ||   // Jet Force Gemini (U) (Kiosk Demo)
	   (curCRC[0] == 0xF163A242 && curCRC[1] == 0xF2449B3B))     // Star Twins (J)
		xbox64_db_framebuffer(1);
	else
		xbox64_db_framebuffer(0);

	/* [Xbox360] ★ A CERO EN CADA CARGA.  Con glN64, un juego sin parche cae en
	 * la rama vacia `else if (!RICE_ACTIVO) { }` de abajo y nunca llegaba al
	 * `else` final que lo limpia: el parche del juego ANTERIOR seguia puesto.
	 * Cada rama que quiere parche lo pone explicitamente. */
	game_specific_hack = 0;

	if(curCRC[0] == 0xCA12B547 && curCRC[1] == 0x71FA4EE4) {
		game_specific_hack = &hack_pkm_snap_u;
		restore_count_per_op();
	}
	else if(curCRC[0] == 0xEC0F690D && curCRC[1] == 0x32A7438C) {
		game_specific_hack = &hack_pkm_snap_j;
		restore_count_per_op();
	}
	else if(curCRC[0] == 0x7BB18D40 && curCRC[1] == 0x83138559) {
		game_specific_hack = &hack_pkm_snap_a;
		restore_count_per_op();
	}
	else if(curCRC[0] == 0x4FF5976F && curCRC[1] == 0xACF559D8) {
		game_specific_hack = &hack_pkm_snap_e;
		restore_count_per_op();
	}
	else if(curCRC[0] == 0x5F3F49C6 && curCRC[1] == 0x0DC714B0) {
		game_specific_hack = &hack_topgear_hb_e;
		restore_count_per_op();
	}
	else if(curCRC[0] == 0x845B0269 && curCRC[1] == 0x57DE9502) {
		game_specific_hack = &hack_topgear_hb_j;
		restore_count_per_op();
	}
	else if(curCRC[0] == 0x8ECC02F0 && curCRC[1] == 0x7F8BDE81) {
		game_specific_hack = &hack_topgear_hb_u;
		restore_count_per_op();
	}
	else if(curCRC[0] == 0xD09BA538 && curCRC[1] == 0x1C1A5489) {
		game_specific_hack = &hack_topgear_od_e;
		restore_count_per_op();
	}
	else if(curCRC[0] == 0x0578F24F && curCRC[1] == 0x9175BF17) {
		game_specific_hack = &hack_topgear_od_j;
		restore_count_per_op();
	}
	else if(curCRC[0] == 0xD741CD80 && curCRC[1] == 0xACA9B912) {
		game_specific_hack = &hack_topgear_od_u;
		restore_count_per_op();
	}
	else if(
			(curCRC[0] == 0xC16C421B && curCRC[1] == 0xA21580F7) /*||	// Donald Duck Quackers
			(curCRC[0] == 0x72611D7D && curCRC[1] == 0x9919BDD2) || // HSV Adventure Racing (A)
			(curCRC[0] == 0xA1B64A61 && curCRC[1] == 0xD014940B) || // Beetle Adventure Racing! (E) (M3)
			(curCRC[0] == 0x9C7318D2 && curCRC[1] == 0x24AE0DC1) || // Beetle Adventure Racing (J)
			(curCRC[0] == 0x3DF17480 && curCRC[1] == 0x193DED5A) || // Donald Duck - Quack Attack (E) (M5)
			(curCRC[0] == 0xF478D8B3 && curCRC[1] == 0x9716DD6D) || // LEGO Racers (E) (M10)
			(curCRC[0] == 0x096A40EA && curCRC[1] == 0x8ABE0A10) || // LEGO Racers (U) (M10)
			(curCRC[0] == 0xD3D806FC && curCRC[1] == 0xB43AA2A8) || // Monster Truck Madness 64 (E) (M5)
			(curCRC[0] == 0xB19AD999 && curCRC[1] == 0x7E585118) || // Monster Truck Madness 64 (U)
			(curCRC[0] == 0x02D8366A && curCRC[1] == 0x6CABEF9C) || // Road Rash 64 (E)
			(curCRC[0] == 0xF050746C && curCRC[1] == 0x247B820B) || // Road Rash 64 (U)
			(curCRC[0] == 0x1AA05AD5 && curCRC[1] == 0x46F52D80) || // Pilotwings 64 (E) (M3)
			(curCRC[0] == 0x09CC4801 && curCRC[1] == 0xE42EE491) || // Pilotwings 64 (J)
			(curCRC[0] == 0xC851961C && curCRC[1] == 0x78FCAAFA) || // Pilotwings 64 (U)
			(curCRC[0] == 0x535DF3E2 && curCRC[1] == 0x609789F1) || // Wave Race 64 - Shindou Edition (J) (V1.2)
			(curCRC[0] == 0xD4C45A1A && curCRC[1] == 0xF425B25E)  // WCW Nitro (U)
			(curCRC[0] == 0xCEA8B54F && curCRC[1] == 0x7F21D503) || // Wetrix (E) (M6)
			(curCRC[0] == 0xDCB6EAFA && curCRC[1] == 0xC6BBCFA3) || // Wetrix (J)
			*/)
	{ // Wetrix (U) (M6)
				old_count_per_op = count_per_op;
				count_per_op = COUNT_PER_OP_3;
	}
	/* ★ Con Rice activo, esta rama vacia CORTA la cadena: los parches que
	 * vienen debajo son remiendos de glN64 y Wii64 los excluia con
	 * `#ifndef RICE_GFX`.  Interponer un `else if` cuesta dos lineas y deja
	 * la cadena original intacta; la alternativa era anyadir la condicion a
	 * las siete comparaciones de abajo, o reindentarlas todas. */
	else if (RICE_ACTIVO) { }
	else if((curCRC[0] == 0xED98957E && curCRC[1] == 0x8242DCAC) ||
			(curCRC[0] == 0xD5898CAF && curCRC[1] == 0x6007B65B) ||
			(curCRC[0] == 0x1FA056E0 && curCRC[1] == 0xA4B9946A)) {
		game_specific_hack = &hack_winback;
		restore_count_per_op();
	}
	else if((curCRC[0] == 0x57BFF74D && curCRC[1] == 0xDE747743) ||
			(curCRC[0] == 0xA273AB56 && curCRC[1] == 0xDA33DB9A)) {
		game_specific_hack = &hack_dukenukem;
		restore_count_per_op();
	}
	else if((curCRC[0] == 0xE6419BC5 && curCRC[1] == 0x69011DE3) ||	// Ogre Battle 64 (U)
			(curCRC[0] == 0x0375CF67 && curCRC[1] == 0x56A93FAA) || // Ogre Battle 64 (J) v1.1
			(curCRC[0] == 0x0ADAECA7 && curCRC[1] == 0xB17F9795)) {	// Ogre Battle 64 (U) v1.1
		game_specific_hack = &hack_ogrebattle;
		restore_count_per_op();
	}
	else if((curCRC[0] == 0x2337D8E8 && curCRC[1] == 0x6B8E7CEC) ||	// Yoshi's Story (U)
			(curCRC[0] == 0xD3F97D49 && curCRC[1] == 0x6924135B) ||	// Yoshi's Story (E)
			(curCRC[0] == 0x2DCFCA60 && curCRC[1] == 0x8354B147)) {	// Yoshi's Story (J)
		game_specific_hack = &hack_yoshistory;
		restore_count_per_op();
	}
	/* (Aqui iban las tres ramas de Pilotwings 64; ver la nota donde estaban
	 * sus parches.) */
	else if((curCRC[0] == 0x918E2D60 && curCRC[1] == 0xF865683E) ||
			(curCRC[0] == 0x769147F3 && curCRC[1] == 0x2033C10E)) {
		game_specific_hack = &hack_scars;
		restore_count_per_op();
	}
	/* ★ Y el simetrico: el parche de la subpantalla de Zelda es de Rice
	 * (`#ifdef RICE_GFX` en Wii64), asi que con glN64 activo esta rama vacia
	 * lo salta. */
	else if (!RICE_ACTIVO) { }
	else if(strncmp((char *)ROM_HEADER.Name, "THE LEGEND OF ZELDA", 19) == 0) {
		zelda_subscreen_address = 0;
		game_specific_hack = 0;
        if (curCRC[0] == 0xEC7011B7 && curCRC[1] == 0x7616D72B) {
            // Legend of Zelda, The - Ocarina of Time (U) + (J) (V1.0)
            zelda_subscreen_address = 0x1DA5CB;
        } else if (curCRC[0] == 0xD43DA81F && curCRC[1] == 0x021E1E19) {
            // Legend of Zelda, The - Ocarina of Time (U) + (J) (V1.1)
            zelda_subscreen_address = 0x1DA78B;
        } else if (curCRC[0] == 0x693BA2AE && curCRC[1] == 0xB7F14E9F) {
            // Legend of Zelda, The - Ocarina of Time (U) + (J) (V1.2)
            zelda_subscreen_address = 0x1DAE8B;
        } else if (curCRC[0] == 0xB044B569 && curCRC[1] == 0x373C1985) {
            // Legend of Zelda, The - Ocarina of Time (E) (V1.0)
            zelda_subscreen_address = 0x1D860B;
         } else if (curCRC[0] == 0xB2055FBD && curCRC[1] == 0x0BAB4E0C) {
            // Legend of Zelda, The - Ocarina of Time (E) (V1.1)
            zelda_subscreen_address = 0x1D864B;
        // GC Versions
        } else if (curCRC[0] == 0x1D4136F3 && curCRC[1] == 0xAF63EEA9) {
            // Legend of Zelda, The - Ocarina of Time - Master Quest (E) (GC Version)
            zelda_subscreen_address = 0x1D8F4B;
        } else if (curCRC[0] == 0x09465AC3 && curCRC[1] == 0xF8CB501B) {
            // Legend of Zelda, The - Ocarina of Time (E) (GC Version)
            zelda_subscreen_address = 0x1D8F8B;
        } else if (curCRC[0] == 0xF3DD35BA && curCRC[1] == 0x4152E075) {
            // Legend of Zelda, The - Ocarina of Time (U) (GC Version)
            zelda_subscreen_address = 0x1DB78B;
        } else if (curCRC[0] == 0xF034001A && curCRC[1] == 0xAE47ED06) {
            // Legend of Zelda, The - Ocarina of Time - Master Quest (U) (GC Version)
            zelda_subscreen_address = 0x1DB74B;
        } else if (curCRC[0] == 0xF7F52DB8 && curCRC[1] == 0x2195E636) {
            // Zelda no Densetsu - Toki no Ocarina - Zelda Collection Version (J) (GC Version)
            zelda_subscreen_address = 0x1DB78B;
        } else if (curCRC[0] == 0xF611F4BA && curCRC[1] == 0xC584135C) {
            // Zelda no Densetsu - Toki no Ocarina GC (J) (GC Version)
            zelda_subscreen_address = 0x1DB78B;
        } else if (curCRC[0] == 0xF43B45BA && curCRC[1] == 0x2F0E9B6F) {
            // Zelda no Densetsu - Toki no Ocarina GC Ura (J) (GC Version)
            zelda_subscreen_address = 0x1DB78B;
		}
		if(zelda_subscreen_address != 0) {
			game_specific_hack = &hack_zelda_oot;
		}
	}
	else {
		game_specific_hack = 0;
	}

	//print_gecko("Applied a hack? %s\r\n", game_specific_hack ? "yes" : "no");
}
