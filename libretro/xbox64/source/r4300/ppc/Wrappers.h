/**
 * Wii64 - Wrappers.h
 * Copyright (C) 2007, 2008, 2009, 2010 Mike Slegeir
 * 
 * Interface between emulator code and recompiled code
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: tehpola@gmail.com
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

#ifndef WRAPPERS_H
#define WRAPPERS_H

#include "Recompile.h"
#include "../r4300.h"

#define DYNAREG_R4300   28
#define DYNAREG_RDRAM   29
#define DYNAREG_FUNC    30
#define DYNAREG_ZERO    31

/* [Xbox360] 12 -> 56.  Es el desplazamiento, DENTRO del marco del
 * trampolin, donde vive la direccion de retorno: el codigo emitido la lee
 * con `lwz r0,DYNAOFF_LR(r1)` en 16 sitios de MIPS-to-PPC.c.
 *
 * ★ EN XBOX 360 LOS 56 PRIMEROS BYTES DEL MARCO NO SON TUYOS.  El ABI
 * reserva ahi el area de parametros y los saves de LR/CR/FP del LLAMADO, o
 * sea que la primera llamada a C desde codigo recompilado (`dyna_mem`,
 * `decodeNInterpret`, `dyna_update_count`) machacaria el +12.  El numero 56
 * no es un margen inventado: es el `STACK_EXTRA = (8+6)*PTR_SIZE` de
 * `libretro/picodrive/cpu/drc/emit_ppc.c:1857`, que llego a el pagando este
 * mismo cuelgue.  La nota larga esta en Wrappers.c, junto al trampolin.
 *
 * ★ VA EMPAREJADO CON `DYNA_OFF_LR` DE Wrappers.c Y CON EL MARCO DE
 * `gen_checkfp` (MIPS-to-PPC.c:291).  Los tres se mueven a la vez o no se
 * mueve ninguno. */
#if defined(_MSC_VER) && defined(_XBOX)
#define DYNAOFF_LR     56
#else
#define DYNAOFF_LR     12
#endif

/* El marco del ayudante `gen_checkfp` (MIPS-to-PPC.c:291).  Es el unico
 * marco que se apila el propio codigo emitido -- censado: no hay mas
 * GEN_STWU sobre R1 en los cinco .c de ppc/ -- y desde el se llama a C, asi
 * que sufre exactamente lo mismo que DYNAOFF_LR.
 *
 * 64 y no 60: la pila va alineada a 16.  El LR en 56, que es la primera
 * ranura libre por encima del area reservada del ABI. */
#if defined(_MSC_VER) && defined(_XBOX)
#define CHECKFP_FRAME  64
#define CHECKFP_LR     56
#else
#define CHECKFP_FRAME  32
#define CHECKFP_LR     24
#endif

#define REG_LOCALRS    34


typedef enum { MEM_LW,   MEM_LH,   MEM_LB,   MEM_LD,
               MEM_LWU,  MEM_LHU,  MEM_LBU,
               MEM_LWC1, MEM_LDC1, MEM_LL,
               MEM_LWL,  MEM_LWR,  MEM_LDL,  MEM_LDR,
               MEM_SW,   MEM_SH,   MEM_SB,   MEM_SD,
               MEM_SWC1, MEM_SDC1, MEM_SC,
               MEM_SWL,  MEM_SWR,  MEM_SDL,  MEM_SDR } memType;

/* [Xbox360] `dynarec` la llama `r4300.c:209` (C) y la define Wrappers.c,
 * que se compila como C++.  Ver la nota de Recompile.h sobre por que esto
 * va simbolo a simbolo y no envolviendo la cabecera. */
#ifdef __cplusplus
extern "C" {
#endif
void dynarec(unsigned int address);
/* [Xbox360] [log 38] Olvida el enlace pendiente (`link_branch`/`last_func`).
 * Hay que llamarla siempre que se tire TODO el codigo recompilado: al
 * arrancar otro juego (`go`, r4300.c) y al cargar un estado
 * (xbox/savestate_n64.c).  Ver la nota en Wrappers.c. */
void dynarec_olvida_enlace(void);
/* [Xbox360] ★ LA AUTOPRUEBA DEL RECOMPILADOR CONTRA EL INTERPRETE.  Compila y
 * ejecuta cada instruccion sin memoria ni saltos con valores limite, ejecuta
 * la misma con `decodeNInterpret` y compara registros, HI/LO, FPR y fcr31.
 * Una vez por proceso, desde `xbox64_load_rom` justo despues de `cpu_init`
 * y antes del primer `go()`, solo con el recompilador.  Escribe `[AUTO]`.
 * A 0, la funcion queda vacia.  [2026-10-02] Apagada a peticion del usuario
 * tras cerrar Goemon: ponerla a 1 vuelve a pasar los ~22.000 casos al
 * cargar el primer juego de la sesion (unos 240 ms).
 * [2026-10-12] Encendida UNA pasada para estrenar los casos de `$zero` y
 * divisor 0 (Mischief Makers); apagarla otra vez despues del log.
 * [2026-10-13] Apagada: Logs Salvia (10), 293 instrucciones y 28224 casos;
 * solo fallan los `.div0` (DIV/DIVU/DDIV/DDIVU: el resultado de dividir por
 * cero es indefinido y el recompilador no lo imita a proposito) y los
 * `.viejo` de siempre.  Los `$zero` de DDIV/DDIVU, ya limpios. */
#define XBOX64_AUTOPRUEBA 0
void xbox64_autoprueba_dynarec(void);
#ifdef __cplusplus
}
#endif
unsigned int decodeNInterpret(MIPS_instr, unsigned int, int);
unsigned int dyna_cop0_status(unsigned int, unsigned int, unsigned int, int);
#ifdef COMPARE_CORE
int dyna_update_count(unsigned int pc, int isDelaySlot);
#else
int dyna_update_count(unsigned int pc);
#endif
unsigned int dyna_check_cop1_unusable(unsigned int pc, int isDelaySlot);
/* [Xbox360] `invalidate_func` la llaman `pure_interp.c:70` (macro
 * check_memory) y `dma.c:182-183`, los dos en C. */
#ifdef __cplusplus
extern "C" {
#endif
void invalidate_func(unsigned int addr);
#ifdef __cplusplus
}
#endif
void invalidate_func_range(unsigned int addr, unsigned int bytes);
unsigned int dyna_mem(unsigned int addr, unsigned int value, int count,
                      memType type, unsigned int pc, int isDelaySlot,
                      int reverse);

//cop0 macros
#define Index r4300.reg_cop0[0]
#define Random r4300.reg_cop0[1]
#define EntryLo0 r4300.reg_cop0[2]
#define EntryLo1 r4300.reg_cop0[3]
#define Context r4300.reg_cop0[4]
#define PageMask r4300.reg_cop0[5]
#define Wired r4300.reg_cop0[6]
#define BadVAddr r4300.reg_cop0[8]
#define Count r4300.reg_cop0[9]
#define EntryHi r4300.reg_cop0[10]
#define Compare r4300.reg_cop0[11]
#define Status r4300.reg_cop0[12]
#define Cause r4300.reg_cop0[13]
#define EPC r4300.reg_cop0[14]
#define PRevID r4300.reg_cop0[15]
#define Config r4300.reg_cop0[16]
#define LLAddr r4300.reg_cop0[17]
#define WatchLo r4300.reg_cop0[18]
#define WatchHi r4300.reg_cop0[19]
#define XContext r4300.reg_cop0[20]
#define PErr r4300.reg_cop0[26]
#define CacheErr r4300.reg_cop0[27]
#define TagLo r4300.reg_cop0[28]
#define TagHi r4300.reg_cop0[29]
#define ErrorEPC r4300.reg_cop0[30]

#endif

