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

/* [Xbox360] ★ [Castlevania/Conker, Logs Salvia (4)] 8 BYTES DE TRABAJO DEL
 * CODIGO EMITIDO, DENTRO DEL MARCO DEL TRAMPOLIN (dyna_tramp, 112 bytes):
 * ahi solo se usan el enlace hacia atras (0) y el LR (56); los DYNA_OFF_R28..
 * MYLR de Wrappers.c son del marco de dyna_enter, el de ARRIBA.  Lo usa
 * CVT_FP_W (MIPS-to-PPC.c) para el numero magico del entero a coma flotante,
 * que en GliGli iba en -8(r1): la zona roja, que en Xbox 360 no existe.
 * Alineado a 8 (el marco va a 16) y por encima del area del ABI (0..55). */
#if defined(_MSC_VER) && defined(_XBOX)
#define DYNAOFF_CVT    64
#endif

/* [Xbox360] ★★ [Castlevania con esqueletos, Logs Salvia (4)] LAS VARIANTES
 * DE TLB.  El codigo compilado cuelga de la pagina VIRTUAL (blocks[]), y
 * Castlevania remapea su ventana de overlays (0x0E/0x0F......) a OTRA pagina
 * fisica ~1.700-2.000 veces por segundo (TLBWI, `misma=0`): cada remapeo
 * tiraba el bloque entero y se recompilaba al volver, 700-850 veces por
 * segundo, 5-7 ms por VI.  Con esto, al remapear, el bloque se APARTA
 * (guardado con la fisica de la que salio) y, si la pagina vuelve a una
 * fisica ya vista, se recupera su codigo en vez de recompilarlo, si la
 * fisica sigue diciendo lo mismo (var_hash).  Ver "LAS VARIANTES DE TLB" en
 * Wrappers.c.
 *   XB_TLBVAR_N     variantes apartadas por pagina virtual, como mucho
 *   XB_TLBVAR_POOL  bloques de variante vivos en total, como mucho */
#define XB_TLBVAR_N     8u
#define XB_TLBVAR_POOL  4096u

/* [Xbox360] ★★ EL SALTO POR REGISTRO SIN VOLVER AL DESPACHADOR (JR/JALR).
 * Wii64, not64 y mupen64-360 dejan `INTERPRET_JR`/`INTERPRET_JALR` puestos
 * (Interpreter.h; aqui ya quitados): la version compilada es un
 * `// TODO: jr`, y cada salto por
 * registro -- cada RETORNO de funcion -- sale del codigo generado al
 * despachador en C (`dynarec()`, Wrappers.c).  Medido con la sonda [CPU]
 * (Logs Salvia (4)-(6)): Conker 500.000-700.000 vueltas/s, 3-4 ms por VI;
 * Castlevania ~350.000/s, ~2 ms por VI.
 *
 * Con esto, como la referencia (mupen64plus-core new_dynarec: `jump_vaddr` ->
 * `get_addr_ht`, tabla hash por direccion): el despachador apunta en
 * `xb_jr_tabla` cada destino que resuelve, y el codigo emitido de JR/JALR
 * (genJumpToRegRapido, MIPS-to-PPC.c) lo busca ahi y salta directo si:
 *   - no hay interrupcion pendiente (cr3, como los saltos enlazados),
 *   - la direccion coincide con la de la entrada (solo se apuntan KSEG0/KSEG1:
 *     una de TLB nunca coincide y va al despachador como siempre),
 *   - `invalid_code` de la pagina y de su alias KSEG0/KSEG1 estan a 0 (lo
 *     mismo que mira `update_invalid_addr` en el despachador),
 *   - `r4300.stop` esta a 0 (el `while` del despachador),
 *   - la direccion cae dentro de la funcion y esa entrada esta compilada
 *     (`code_addr[]`, que se lee en el momento: vale tras un Realloc).
 * Si algo falla, `blr` con r3 = destino: el camino de antes, tal cual.
 * Al entrar hace lo del despachador: r4300.last_pc = destino, LRU de la
 * funcion, r30 = funcion.
 *
 * ★ LA ENTRADA NO PUEDE SOBREVIVIR A SU FUNCION: free_func
 * (Recomp-Cache-Heap.c) borra las suyas (dynarec_jr_olvida_func) y los
 * vaciados globales vacian la tabla (dynarec_olvida_enlace, cpu_deinit). */
#define XB_JR_BITS      12u
#define XB_JR_N         (1u << XB_JR_BITS)   /* 4096 entradas de 8 bytes */
typedef struct {
	unsigned int  vaddr;   /* 0 = libre                       */
	PowerPC_func* func;    /* la funcion que contiene `vaddr` */
} XbJrEntrada;

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
/* [Xbox360] ★ [Logs Salvia (5): Castlevania -> Conker, Access violation al
 * arrancar] LAS VARIANTES DE TLB, FUERA ANTES DE LIBERAR LOS BLOQUES.
 * `cpu_deinit` (r4300.c) hace free() de todos los blocks[], y el registro de
 * variantes se quedaba con punteros a bloques ya liberados: el vaciado del
 * juego siguiente los recorria.  Se llama al principio de cpu_deinit; vacia
 * tambien la tabla de saltos de JR. */
void dynarec_tlbvar_vaciar(void);
/* [Xbox360] El salto por registro rapido (xb_jr_tabla, arriba).  La tabla la
 * lee el codigo emitido por su direccion; `olvida_func` la llama free_func
 * antes de liberar una funcion; `vaciar`, los vaciados globales. */
extern XbJrEntrada xb_jr_tabla[XB_JR_N];
void dynarec_jr_olvida_func(PowerPC_func* func);
void dynarec_jr_vaciar(void);
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

