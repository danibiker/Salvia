/**
 * Wii64 - Interpreter.h
 * Copyright (C) 2007, 2008, 2009, 2010 Mike Slegeir
 * 
 * Defines and functions for calling the interpreter
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

#ifndef INTERPRETER_H
#define INTERPRETER_H

/* decodeNInterpret(MIPS_instr, unsigned int PC):
	1) Saves state
	2) Calls prefetch_opcode(instr)               (Decodes)
	3) Calls interp_ops[MIPS_GET_OPCODE(instr)]() (Interprets)
	4) Restores state
 */
unsigned int decodeNInterpret(MIPS_instr, unsigned int, int);

// These defines tell the recompiler to interpret
//  rather than recompile the instruction

/* [Xbox360] ★★ BISECCION TEMPORAL, TERCERA PASADA.  NO ES UN ARREGLO.
 *
 * Lo medido hasta aqui, en orden:
 *   1. memoria al interprete  -> DK64 BIEN;  devolverla -> MAL.
 *   2. solo las CARGAS al interprete -> BIEN.  Los almacenamientos
 *      recompilados son inocentes.
 *   3. FASTMEM_HOT_NOFLUSH apagado con todo recompilado -> SIGUE MAL.
 *      Tambien inocente.
 *
 * O sea que el defecto esta en las CARGAS y NO en el camino "en
 * caliente".  Desde aqui las cargas se bisecan con UNA MASCARA DE BITS,
 * un bit por opcode (justo debajo).
 *
 * ★ XBOX64_CARGAS_MASCARA = XBOX64_C_TODAS junto con
 * XBOX64_ALMACEN_AL_INTERPRETE = 1 son los mismos 23 macros de la pasada
 * 1: ni uno mas ni uno menos.  Asi "todo encendido" sigue siendo el
 * binario que se sabe bueno, y "todo apagado" el que falla. */
/* ★★ LAS CARGAS SE BISECAN CON UNA MASCARA DE BITS, UN BIT POR OPCODE.
 *
 * Bit a 1 = esa carga va al INTERPRETE.  Asi cada pasada es cambiar UN
 * numero, sin reordenar bloques, y se puede hacer busqueda binaria de
 * verdad: 13 opcodes son 4 pasadas en el peor caso.
 *
 * ★ Y NO SOLO PARTIR POR LA MITAD: si una pasada senyala un grupo, la
 * siguiente puede pedir UN SOLO BIT.  La mascara no obliga a que los
 * bits que se prueban juntos sean contiguos ni "del mismo grupo".
 *
 * El orden de los bits es el de menos a mas raro: primero las siete
 * simples, luego las cuatro desalineadas, luego las dos de coma
 * flotante. */
#define XBOX64_C_LB    0x0001
#define XBOX64_C_LBU   0x0002
#define XBOX64_C_LH    0x0004
#define XBOX64_C_LHU   0x0008
#define XBOX64_C_LW    0x0010
#define XBOX64_C_LWU   0x0020
#define XBOX64_C_LD    0x0040
#define XBOX64_C_LWL   0x0080
#define XBOX64_C_LWR   0x0100
#define XBOX64_C_LDL   0x0200
#define XBOX64_C_LDR   0x0400
#define XBOX64_C_LWC1  0x0800
#define XBOX64_C_LDC1  0x1000

/* Atajos con nombre, por si la pasada quiere un grupo entero.  No son
 * mas que sumas de los bits de arriba; la mascara puede escribirse a
 * mano igual de bien.
 *
 *   SIMPLES  una instruccion PPC alineada contra rdram, con mirilla de
 *            rafagas y camino de constante.  Es la forma MAS parecida a
 *            la de los almacenamientos, que ya sabemos que funciona.
 *   DESALIN  leen-modifican-escriben el registro destino con mascaras y
 *            desplazamientos variables; LDL/LDR ademas fuerzan
 *            isPhysical=0, o sea que SIEMPRE van por la rama lenta.
 *   COP1     ★ las unicas que escriben en la cache de FPR (`mapFPRNew`) y
 *            llaman a `genCheckFP`.  Y ojo: `INTERPRET_FP` NO las cubre,
 *            asi que en las pasadas anteriores, con la coma flotante "al
 *            interprete", estas dos SEGUIAN recompilandose. */
#define XBOX64_C_SIMPLES  (XBOX64_C_LB|XBOX64_C_LBU|XBOX64_C_LH|XBOX64_C_LHU| \
                           XBOX64_C_LW|XBOX64_C_LWU|XBOX64_C_LD)
#define XBOX64_C_DESALIN  (XBOX64_C_LWL|XBOX64_C_LWR|XBOX64_C_LDL|XBOX64_C_LDR)
#define XBOX64_C_COP1     (XBOX64_C_LWC1|XBOX64_C_LDC1)
#define XBOX64_C_TODAS    (XBOX64_C_SIMPLES|XBOX64_C_DESALIN|XBOX64_C_COP1)
#define XBOX64_C_NINGUNA  0

/* ==== PLAN DE PASADAS =================================================
 *
 * "BIEN" quiere decir que el defecto esta DENTRO de la mascara (lo que
 * se ha mandado al interprete); "MAL", que esta FUERA, en lo que sigue
 * recompilado.  Cada fila es una compilacion.
 *
 * ANCLAS (ya medidas, solo para re-comprobar si algo deja de cuadrar):
 *   0x1FFF + almacen=1  -> BIEN     0x0000 + almacen=0  -> MAL
 *
 *   #   mascara  al interprete        si BIEN   si MAL
 *   --  -------  -------------------  --------  --------
 *   1   0x1F80   desalineadas + cop1  -> 2A     -> 2B
 *   2A  0x1800   LWC1 LDC1            -> 3A     -> 3A'
 *   3A  0x1000   LDC1                 LDC1      LWC1
 *   3A' 0x0600   LDL LDR              -> 4A     -> 4A'
 *   4A  0x0200   LDL                  LDL       LDR
 *   4A' 0x0080   LWL                  LWL       LWR
 *   2B  0x0060   LWU LD               -> 3B     -> 3B'
 *   3B  0x0040   LD                   LD        LWU
 *   3B' 0x0010   LW                   LW        -> 4B
 *   4B  0x000C   LH LHU               -> 5B     -> 5B'
 *   5B  0x0004   LH                   LH        LHU
 *   5B' 0x0001   LB                   LB        LBU
 *
 * ★ EL ORDEN NO ES UNA BUSQUEDA BINARIA CIEGA, va por mecanismo:
 *   1   separa el camino simple (una instruccion contra rdram) de los
 *       dos raros.
 *   2A  COP1 primero dentro de los raros: son las unicas que tocan la
 *       cache de FPR, y son el unico trozo de coma flotante que NUNCA se
 *       ha apagado (`INTERPRET_FP` no las cubre).
 *   3A' LDL/LDR antes que LWL/LWR: fuerzan isPhysical=0, o sea que
 *       siempre van por la rama lenta -- camino distinto, no solo
 *       aritmetica distinta.
 *   2B  LWU/LD primero dentro de las simples: son las unicas que usan
 *       `mapRegister64New`, o sea pareja hi/lo de registros.
 *   3B' LW despues, por ser con diferencia la mas frecuente.
 *
 * ★ Los bits NO tienen que ir juntos ni ser contiguos: si una pasada
 * deja dos sospechosos de grupos distintos, se piden los dos y ya. */

/* ★ ESTA ES LA LINEA QUE SE TOCA CADA PASADA.
 *
 * Pasada 1 (0x1F80, desalineadas + cop1): BIEN.  El defecto esta en esos
 * seis opcodes.  Las siete simples quedan exculpadas, y con ellas todo el
 * camino de una instruccion contra rdram.
 *
 * Pasada 2A (0x1800, LWC1+LDC1): MAL.  La coma flotante tambien queda
 * exculpada -- y con ella `mapFPRNew`, `genCheckFP` y la sospecha del
 * viaje `lfs`/`stfs`.
 *
 * Pasada 3A' (0x0600, LDL+LDR): BIEN.  QUEDAN DOS.
 *
 * Pasada 4A (0x0200, solo LDL): MAL.  ★★★ EL DEFECTO ES **LDR**.
 * BISECCION TERMINADA.
 *
 * ★ LA MASCARA VUELVE A 0 A PROPOSITO: hace falta LDR RECOMPILADA para
 * que la sonda con oraculo de `dyna_mem` (Wrappers.c, case MEM_LDR)
 * pueda hablar.  El juego va a ir mal en esta pasada; es lo que toca.
 *
 * ★ LO QUE HACE GliGli, CONSULTADO: no recompila LDL/LDR EN ABSOLUTO.
 * `mupen64-360/source/r4300/ppc/Interpreter.h:57-58` define
 * INTERPRET_LDL e INTERPRET_LDR incondicionalmente, fuera de todo
 * `#if 0`, y la rama recompilada de las dos es `// TODO` ->
 * CONVERT_ERROR.  Su dynarec es el de generacion ANTIGUA y nunca las
 * implemento: el `dyna_mem` que las hace es de los que Wii64 ANYADE
 * encima.  O sea que el unico port de esta familia que corre en Xenon
 * evita justo los dos opcodes que la biseccion ha senyalado. */
/* ★★ [2026-10-01] GOEMON'S GREAT ADVENTURE (U): EL DEFECTO ES DEL DYNAREC.
 *
 * Medido (logs 40-45): negro al empezar la aventura con el recompilador,
 * avanza con el interprete.  Descartados con medida: los hilos, count_per_op,
 * la Controller Pak, instrucciones tiradas (CONVERT_ERROR) y la
 * invalidacion de TLBWR.  En caliente, interprete -> recompilador justo antes
 * de elegir personaje TAMBIEN falla: el defecto actua durante la carga.
 *
 * ★ LA BISECCION A CIEGAS SE APLAZA (peticion del usuario, con razon): la
 * de DK64 ya exculpo cargas, almacenamientos y FP, pero solo "para DK64"; lo
 * que DK64 no ejecutaba nunca se probo.  Antes de compilar pasadas, el CENSO
 * de `convert()` (MIPS-to-PPC.c) dice que clases de instruccion se compilan
 * POR PRIMERA VEZ al empezar la aventura: esas, cruzadas con las nunca
 * auditadas (64 bits, MULT/DIV, SLT, COP0, movimientos COP1, conversiones y
 * redondeos FP), son las unicas que hay que bisecar.  Hasta tenerlo, los
 * interruptores en su valor normal: el censo se toma sobre el binario que
 * FALLA. */
#define XBOX64_CARGAS_MASCARA  (XBOX64_C_NINGUNA)

#define XBOX64_ALMACEN_AL_INTERPRETE        0

#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LB
#define INTERPRET_LB
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LBU
#define INTERPRET_LBU
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LH
#define INTERPRET_LH
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LHU
#define INTERPRET_LHU
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LW
#define INTERPRET_LW
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LWU
#define INTERPRET_LWU
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LD
#define INTERPRET_LD
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LWL
#define INTERPRET_LWL
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LWR
#define INTERPRET_LWR
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LDL
#define INTERPRET_LDL
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LDR
#define INTERPRET_LDR
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LWC1
#define INTERPRET_LWC1
#endif
#if (XBOX64_CARGAS_MASCARA) & XBOX64_C_LDC1
#define INTERPRET_LDC1
#endif

#if XBOX64_ALMACEN_AL_INTERPRETE
#define INTERPRET_SB
#define INTERPRET_SH
#define INTERPRET_SW
#define INTERPRET_SD
#define INTERPRET_SWL
#define INTERPRET_SWR
#define INTERPRET_SDL
#define INTERPRET_SDR
#define INTERPRET_SWC1
#define INTERPRET_SDC1
#endif

/* ==================================================================== */
/* [Xbox360] ★★ BISECCION TEMPORAL.  NINGUNO DE LOS DOS ES UN ARREGLO.   */
/* ==================================================================== */
/* AQUI HAY **TRES** INTERRUPTORES INDEPENDIENTES:
 *
 *   XBOX64_CARGAS_AL_INTERPRETE    (arriba, :48)
 *   XBOX64_ALMACEN_AL_INTERPRETE   (arriba, :49)
 *   XBOX64_FP_AL_INTERPRETE        (mas abajo)
 *
 * Los dos primeros son las dos mitades del que antes era uno solo
 * (XBOX64_MEM_AL_INTERPRETE), y su union reproduce exactamente aquel.
 *
 * -------------------------------------------------------------------- */

/* ---- CORTE 1: LA MEMORIA, PARTIDA EN CARGAS Y ALMACENAMIENTOS --------
 *
 * ★★ LO QUE YA SABEMOS, MEDIDO.  El corte de memoria entero a 1 deja
 * bueno a Donkey Kong 64; devolverlo a 0 lo vuelve a romper tal cual
 * estaba.  O sea que el defecto ESTA en el camino de memoria del codigo
 * emitido, y la coma flotante no lo explica por si sola.
 *
 * Los dos sintomas conocidos, que pueden ser la misma raiz o dos:
 *   (a) los munyecos de la intro se mueven a tirones repitiendo un
 *       patron y el logo de Rare no gira -- CALCULA MAL;
 *   (b) a los ~4 segundos mata al FRONTEND dentro de `Blit1to4` leyendo
 *       una direccion minuscula (0x223D una vez, 0x0E0F otra: basura
 *       distinta cada vez), o sea CORRUPCION DEL MONTON DE LA CRT, que
 *       es donde SDL_ttf crea las superficies del overlay de FPS.
 * Super Mario 64 no hace ninguna de las dos.
 *
 * DESCARTADO YA, CON MEDIDA:
 *   - la ruta de desalojo de la cache: ni `release` ni `heapPush SIN
 *     SITIO` salen en el log, la cache de 7 MB no llego a llenarse.
 *   - que lo introdujera el port: el generador de accesos a memoria de
 *     `MIPS-to-PPC.c` (:4666-5236) es VERBATIM de Wii64; el diff contra
 *     C:\develop\proyectos\personal\Wii64 no toca ni una linea de ahi.
 *     Tampoco los defines: `Makefile.base:21` de Wii64 lleva los mismos
 *     -DFASTMEM -DFASTMEM_HOT_NOFLUSH que el vcxproj.
 *
 * ★ POR QUE ESTE EJE Y NO OTRO.  Las dos mitades no comparten ni un
 * mecanismo, asi que el resultado senyala una zona concreta:
 *
 *   CARGAS   -> son las UNICAS que usan FASTMEM_HOT_NOFLUSH, o sea el
 *               snapshot/reload de la cache de registros alrededor del
 *               camino rapido (MIPS-to-PPC.c:4714-4724, 5189-5225), y la
 *               mirilla de rafagas de LB/LH.  Codigo de registros fino.
 *   ALMACEN. -> son los UNICOS que pueden escribir donde no deben, y los
 *               UNICOS que emiten `check_memory_range` ->
 *               `invalidate_func_range` -> `RecompCache_Free`, que
 *               DESENLAZA funciones PARCHEANDO CODIGO YA EMITIDO en
 *               sitio (Recomp-Cache-Heap.c:204-208) y hace malloc/free
 *               de metadatos EN EL MONTON DE LA CRT.  Que es justo el
 *               monton que se corrompe.
 *
 * ★★ RESULTADO, MEDIDO.  Con CARGAS=1 y ALMACEN=0 -- o sea con los
 * almacenamientos RECOMPILADOS -- DK64 va bien 40 segundos seguidos, sin
 * matar al frontend y con la intro correcta.  Y la sonda nueva de
 * `unlink_func` no imprimio NI UNA LINEA, o sea que el parcheo de codigo
 * en sitio esta sano.  Conclusion:
 *
 *     EL CAMINO DE ALMACENAMIENTO ES INOCENTE.  EL DEFECTO ESTA EN LAS
 *     CARGAS.
 *
 * Y con eso caen de golpe todos los mecanismos que las dos mitades
 * comparten -- la mirilla de rafagas, la isla de trampolines, `pass2`,
 * los saltos especiales, el ABI de `dyna_mem`, el enmascarado a rdram --
 * porque los almacenamientos los usan igual y no fallan.  El reparto de
 * lo que queda esta en el CORTE 3, mas abajo.
 *
 * ★ LOS DOS A 0 ES EL BINARIO QUE FALLA; LOS DOS A 1, EL QUE VA. */

/* ---- CORTE 2: LA COMA FLOTANTE (XBOX64_FP_AL_INTERPRETE) -------------
 *
 * SINTOMA NUEVO, y es de CORRECCION, no de cuelgue: Donkey Kong 64 corre a
 * 60 fps pero CALCULA MAL -- los munyecos de la intro se mueven a
 * tirones repitiendo un patron y el logo de Rare no gira.  Un logo que no
 * rota y una animacion que repite son matematicas de coma flotante malas,
 * no memoria pisada.
 *
 * ★ Y LA COMA FLOTANTE ES DONDE MAS HE TOCADO EN ESTA FASE:
 *     - `round`/`roundf`, que escribi yo (VS2010 no las trae)
 *     - los casts de sobrecarga de `sqrt`/`ceil`/`floor`, que CAMBIAN QUE
 *       FUNCION SE LLAMA si elegi mal la sobrecarga
 *     - `__fixdfdi`/`__fixsfdi`, que escribi yo EN ENSAMBLADOR
 *     - las cuatro CVT con `psq_l` desviadas al interprete
 *
 * A 1, TODA la unidad de coma flotante pasa por el interprete.
 *
 * ★★ DEVUELTA A 0.  El corte de memoria esta cerrado: la causa era el
 * literal `0xFFFFFFFFFFFFFFFFLL` de LDR/SDL, arreglado con `ULL`
 * (Wrappers.c, la nota larga esta en el `case MEM_LDL`).  Esta pasada
 * mueve DOS interruptores a la vez -- este y XBOX64_SIN_HOT_NOFLUSH --
 * y es deliberado: los dos se midieron inocentes, el estado de destino
 * es upstream en los dos, y si algo se rompiera, separarlos cuesta una
 * sola compilacion mas. */
#define XBOX64_FP_AL_INTERPRETE 0

#if XBOX64_FP_AL_INTERPRETE
#define INTERPRET_FP
#endif

/* ---- CORTE 3: FASTMEM_HOT_NOFLUSH (XBOX64_SIN_HOT_NOFLUSH) -----------
 *
 * ★★ POR QUE ESTE Y NO OTRO.  La pasada 2 midio que los ALMACENAMIENTOS
 * recompilados son inocentes y las CARGAS no.  Casi todo lo que se
 * sospechaba es comun a los dos, y por tanto queda descartado de golpe:
 *
 *     la mirilla de rafagas (count>1)  la tienen SB/SH/SW/SD tambien
 *     `check_memory_range`             solo la emiten los almacenamientos
 *     la isla de trampolines, pass2,
 *       los saltos especiales,
 *       el ABI de la llamada a dyna_mem  los usan los dos igual
 *     el enmascarado a rdram
 *       (rlwinm + lwzux/stwux)           misma forma en los dos
 *
 * LO QUE QUEDA, Y ES EXCLUSIVO DE LAS CARGAS, ES **UNO**:
 * FASTMEM_HOT_NOFLUSH.  Su lista (MIPS-to-PPC.c:4716-4720) son LW, LWU,
 * LH, LHU, LB, LBU, LD y LL -- ni un almacenamiento.  Y eso explica la
 * medida entera: para un almacenamiento `hotNoFlush` es SIEMPRE falso, o
 * sea que toma el camino conservador (`flushRegisters` antes,
 * `invalidateRegisters` en la rama lenta).  Solo las cargas toman el
 * camino listo, que mantiene la cache de registros VIVA a traves de una
 * llamada a C y la reconstruye a mano con `emitFlushOf` / `GEN_OR(R3,rd,
 * rd)` / `emitReloadOf` (MIPS-to-PPC.c:5189-5225).
 *
 * ★ A 1, las cargas pasan a usar exactamente la misma forma que los
 * almacenamientos, que es la que SI funciona.
 *
 * ★★ RESULTADO, MEDIDO: DK64 SIGUE MAL.  FASTMEM_HOT_NOFLUSH ES
 * INOCENTE.  Se quedo a 1 (apagado) mientras duro el corte, para que
 * cargas y almacenamientos tuvieran la misma forma de marco y hubiera
 * un mecanismo menos vivo.
 *
 * ★★ YA DEVUELTO A 0: la optimizacion vuelve a estar encendida, que es
 * como la lleva Wii64 (`Makefile.base:21`).
 *
 * ★ ES EQUIVALENTE A QUITAR EL DEFINE DEL vcxproj, ni mas ni menos:
 * `FASTMEM_HOT_NOFLUSH` solo lo mira MIPS-to-PPC.c (censado), y esta
 * cabecera la incluye el mismo fichero en su linea 29, antes del primer
 * uso (:1063).  Se hace aqui para que los interruptores de biseccion
 * sigan estando todos en un sitio.
 *
 * ★ NO ES UN ARREGLO, y ademas es una optimizacion que Wii64 SI lleva
 * encendida (`Makefile.base:21`).  A 0 se vuelve al binario de antes. */
#define XBOX64_SIN_HOT_NOFLUSH 0

#if XBOX64_SIN_HOT_NOFLUSH
#undef FASTMEM_HOT_NOFLUSH
#endif

/* [Xbox360] XBOX64_MEM_AL_INTERPRETE ya no existe: lo sustituyen
 * XBOX64_CARGAS_MASCARA (un bit por opcode) y
 * XBOX64_ALMACEN_AL_INTERPRETE, arriba del todo, donde estan los bloques
 * de macros a los que gobiernan.  `XBOX64_C_TODAS` + almacen a 1 es este
 * de antes, exactamente. */

/* ---- CORTE 4: LAS FAMILIAS NUNCA AUDITADAS (XBOX64_GRUPOS_MASCARA) ----
 *
 * [2026-10-12] MISCHIEF MAKERS: el color de relleno de los cuatro bordes
 * sale gris y DISTINTO en cada arranque con el recompilador; con el
 * interprete sale NEGRO, como en GLideN64 (medido por el usuario).  La RDRAM
 * arranca a cero (memory.c:274), asi que es un valor mal calculado.
 *
 * La biseccion de DK64 exculpo cargas, almacenamientos y coma flotante, pero
 * solo para lo que DK64 ejecutaba.  Lo que NUNCA se ha cortado va aqui, un
 * bit por familia, con la misma regla que las cargas: bit a 1 = esa familia
 * al INTERPRETE.  Los `#if 0` de Wii64 de abajo pasan a mirar estos bits; con
 * la mascara a 0 el binario es el de siempre.
 *
 *   FP      INTERPRET_FP: toda la coma flotante y los movimientos
 *           MFC1/DMFC1/CFC1/MTC1/DMTC1/CTC1 (no LWC1/LDC1/SWC1/SDC1 ni BC1)
 *   ALU64   INTERPRET_DW: DADDIU, DADDU, DSUBU, los desplazamientos de 64
 *           bits, DMULT/DMULTU y DDIV/DDIVU
 *   MULDIV  MULT, MULTU, DIV, DIVU (y DDIV/DDIVU)
 *   HILO    MFHI, MTHI, MFLO, MTLO
 *   SLT     SLT, SLTU, SLTI, SLTIU
 *   BC1     los saltos de coma flotante (BC1F/BC1T y sus "likely")
 *   COP0    MFC0/MTC0 y el resto de COP0 (fuera de la primera pasada: toca
 *           Status, Count y Compare)
 *   LLSC    LL y SC
 *
 * ==== PLAN DE PASADAS (Mischief Makers, bordes) =========================
 * "negro" = el defecto esta DENTRO de la mascara; "gris" = FUERA.
 *
 *   #   mascara  al interprete         si negro   si gris
 *   --  -------  --------------------  ---------  ---------
 *   1   0x003F   FP ALU64 MULDIV HILO  -> 2       (memoria, COP0, ALU32)
 *                SLT BC1
 *   2   0x0021   FP BC1                -> 3A      -> 3B
 *   3A  0x0001   FP                    FP         BC1
 *   3B  0x0006   ALU64 MULDIV          -> 4B      -> 4B'
 *   4B  0x0002   ALU64                 ALU64      MULDIV
 *   4B' 0x0008   HILO                  HILO       SLT
 *   5   0x0200   DIV DIVU              DIV/DIVU   MULT/MULTU
 *
 * ★ Por mecanismo, no a ciegas: la 2 separa la unidad de coma flotante (la
 * cache de FPR y fcr31, que es lo que lee BC1) de los enteros; la 3B junta
 * ALU64 y MULDIV, que escriben parejas hi/lo de registros (mapRegister64 /
 * mapConstant64New), frente a HILO y SLT, que solo las leen o dan un bit.
 *
 * Pasada 1 (0x003F): NEGRO (usuario, 2026-10-12).  Memoria, COP0 y la ALU de
 * 32 bits quedan exculpadas.
 * Pasadas 2 (0x0021) y 3B (0x0006) segun la tabla; 4B (0x0002, ALU64): GRIS
 * => MULT/DIV.  DDIV/DDIVU tambien iban al interprete en la 4B (su guarda es
 * `INTERPRET_DW || INTERPRET_DDIV`) y DMULT/DMULTU van siempre: quedan
 * MULT, MULTU, DIV y DIVU.
 * ★ Sospecha (leyendo el codigo): DIV/DIVU con un operando `$zero` NO EMITEN
 * NADA ("Don't divide if they're using r0") con `lo`/`hi` ya mapeados como
 * NUEVOS -- basura de registro, distinta cada arranque --, y con divisor 0 en
 * ejecucion `divw` da un valor indefinido donde el interprete deja LO/HI como
 * estaban.  La autoprueba de Wrappers.c no cubre ninguno de los dos casos.
 * Pasada 5 separa division de multiplicacion; el censo [DIVCENSO]
 * (MIPS-to-PPC.c) dice si el juego compila alguna con `$zero`.
 *
 * ★ Comprobar en el log la linea `[DYNA] biseccion: ... grupos=`. */
#define XBOX64_G_FP      0x0001
#define XBOX64_G_ALU64   0x0002
#define XBOX64_G_MULDIV  0x0004	/* = las dos de abajo; se mantiene por la tabla */
#define XBOX64_G_MUL     0x0100	/* MULT, MULTU */
#define XBOX64_G_DIV     0x0200	/* DIV, DIVU (DDIV/DDIVU ya exculpadas en la 4B) */
#define XBOX64_G_HILO    0x0008
#define XBOX64_G_SLT     0x0010
#define XBOX64_G_BC1     0x0020
#define XBOX64_G_COP0    0x0040
#define XBOX64_G_LLSC    0x0080

/* ★ ESTA ES LA LINEA QUE SE TOCA CADA PASADA (corte 4). */
/* Pasada 5 (0x0200, DIV/DIVU): NEGRO, y [DIVCENSO] encontro 15 `DIV $zero,
 * $s0`.  ARREGLO en DIV/DIVU (MIPS-to-PPC.c), COMPILADO, sin interprete:
 * dividendo `$zero` -> li LO,0 / li HI,0; divisor `$zero` -> LO/HI sin tocar.
 * La mascara vuelve a 0 (todo recompilado) para probarlo solo.
 * ★ CERRADO (log 16, usuario): bordes NEGROS con grupos=0000; [DIVCENSO]
 * vio las 15 compiladas como `li` y ya esta retirada.  Los bits del corte 4
 * se quedan para la proxima biseccion. */
#define XBOX64_GRUPOS_MASCARA  0

#if ((XBOX64_GRUPOS_MASCARA) & XBOX64_G_FP) && !defined(INTERPRET_FP)
#define INTERPRET_FP
#endif
#if (XBOX64_GRUPOS_MASCARA) & XBOX64_G_ALU64
#define INTERPRET_DW
#endif
#if (XBOX64_GRUPOS_MASCARA) & XBOX64_G_HILO
#define INTERPRET_HILO
#endif
#if (XBOX64_GRUPOS_MASCARA) & XBOX64_G_BC1
#define INTERPRET_BC
#endif
#if (XBOX64_GRUPOS_MASCARA) & XBOX64_G_COP0
#define INTERPRET_COP0
#endif
#if (XBOX64_GRUPOS_MASCARA) & XBOX64_G_LLSC
#define INTERPRET_LL
#define INTERPRET_SC
#endif

//#define INTERPRET_DW

#if 0
#define INTERPRET_DADDIU
#define INTERPRET_DSLLV
#define INTERPRET_DSRLV
#define INTERPRET_DSRAV
#define INTERPRET_DADDU
#define INTERPRET_DSUBU
#define INTERPRET_DSLL
#define INTERPRET_DSRL
#define INTERPRET_DSRA
#define INTERPRET_DSLL32
#define INTERPRET_DSRL32
#define INTERPRET_DSRA32
#endif

#define INTERPRET_DMULT
#define INTERPRET_DMULTU

//#define INTERPRET_HILO

#if (XBOX64_GRUPOS_MASCARA) & (XBOX64_G_MULDIV|XBOX64_G_MUL)	/* [Xbox360] corte 4; Wii64: #if 0 */
#define INTERPRET_MULT
#define INTERPRET_MULTU
#endif
#if (XBOX64_GRUPOS_MASCARA) & (XBOX64_G_MULDIV|XBOX64_G_DIV)	/* [Xbox360] corte 4; Wii64: #if 0 */
#define INTERPRET_DIV
#define INTERPRET_DIVU
#define INTERPRET_DDIV
#define INTERPRET_DDIVU
#endif

#if (XBOX64_GRUPOS_MASCARA) & XBOX64_G_SLT	/* [Xbox360] corte 4; Wii64: #if 0 */
#define INTERPRET_SLT
#define INTERPRET_SLTU
#define INTERPRET_SLTI
#define INTERPRET_SLTIU
#endif

//#define INTERPRET_J
//#define INTERPRET_JAL
/* [Xbox360] INTERPRET_JR / INTERPRET_JALR ya no existen: su "version
 * compilada" era un `// TODO` y JR/JALR van ahora por la tabla de saltos
 * (genJumpToRegRapido, MIPS-to-PPC.c; ver Wrappers.h). */
//#define INTERPRET_BC
//#define INTERPRET_BRANCH

#define INTERPRET_SYSCALL
#define INTERPRET_BREAK
#define INTERPRET_TRAPS

//#define INTERPRET_LL
//#define INTERPRET_SC

//#define INTERPRET_COP0
//#define INTERPRET_MFC0
//#define INTERPRET_MTC0
//#define INTERPRET_TLB
//#define INTERPRET_ERET
#define INTERPRET_TLBR
#define INTERPRET_TLBWI
#define INTERPRET_TLBWR
#define INTERPRET_TLBP

//#define INTERPRET_FP

#if 0
#define INTERPRET_MFC1
#define INTERPRET_DMFC1
#define INTERPRET_CFC1
#define INTERPRET_MTC1
#define INTERPRET_DMTC1
#define INTERPRET_CTC1
#endif

#if 0
#define INTERPRET_FP_S
#define INTERPRET_FP_D
#define INTERPRET_FP_W
#define INTERPRET_FP_L
#endif

/* [Xbox360] ★★ LAS CUATRO CONVERSIONES ENTERO->COMA FLOTANTE, AL
 * INTERPRETE, PORQUE Wii64 LAS COMPILA CON `psq_l` DE GEKKO.
 *
 * SINTOMA MEDIDO: `0xC000001D Illegal Instruction` dentro de la arena.  El
 * volcado del bloque dio la instruccion exacta:
 *
 *     w19  C01C0AD8   lfs f0, 0xAD8(r28)      <- normal
 *     w20  E026F000   <- ★ OPCODE 56
 *     w21  E046D002   <- ★ OPCODE 56
 *
 * y el opcode 56 es `PPC_OPCODE_PSQ_L` (PowerPC.h:161): la carga
 * CUANTIZADA de paired-single de Gekko.  `CVT_FP_W` y `CVT_FP_L` la usan
 * con los registros QR7/QR5 para convertir el entero en coma flotante EN
 * HARDWARE, de dos mitades de 16 bits, y de ahi el `two16` = 65536.
 *
 * ★ XENON NO TIENE PAIRED SINGLES.  Ese opcode no existe en su juego de
 * instrucciones y la CPU lo rechaza.
 *
 * ★ NO DEFINIR `GEKKO` NO BASTABA, y conviene decirlo porque fue una
 * decision explicita de la fase 0: estas dos funciones NO estan bajo
 * `#ifdef GEKKO`, emiten `psq_l` siempre.
 *
 * ★ POR QUE `_W`/`_L` Y NO `_CVT_W`/`_CVT_L`: `CVT_FP_W` y `CVT_FP_L` no
 * tienen guarda de intérprete; la tienen sus despachadores `W()` y `L()`
 * (MIPS-to-PPC.c:4361 y :4404).  Y el corte es EXACTO, no aproximado: esos
 * dos despachan UNICAMENTE CVT.S/CVT.D desde W y desde L, y devuelven
 * CONVERT_ERROR para todo lo demas.  O sea que esto interpreta las cuatro
 * instrucciones que no se pueden compilar y ni una mas; el resto de la
 * coma flotante sigue recompilandose.
 *
 * ★ LO QUE HACE GliGli, consultado, por si algun dia el perfil lo pide:
 * las compila, con dos secuencias que SI son portables --
 * `mupen64-360/.../MIPS-to-PPC.c:3369` usa el truco del numero magico
 * (0x4330_0000 + sesgo de signo + `fsub`) para el entero de 32 bits, y
 * :3423 llama a `__floatdidf`/`__floatdisf` con los argumentos en r3:r4
 * para el de 64.  Las dos se pueden portar: la infraestructura de r3:r4 ya
 * existe en xbox/libgcc_ppc.c.  ★ OJO si se hace: su version de 32 bits
 * usa `-8(r1)` y `-4(r1)` como scratch, o sea LA ZONA ROJA, que en Xbox
 * 360 no existe (trampa nº2 de FASE3.txt:6, hoy medida como "no aplica"
 * precisamente porque este dynarec no la usa).
 *
 * Primero que arranque; optimizar una conversion sin un perfil que la
 * senyale seria cambiar codigo sin medida.
 *
 * ★ [Logs Salvia (4)] EL PERFIL YA LA SENALA: CVT.S.W (cop1 14.20 en [CPU4])
 * 300.000-700.000 por segundo en Castlevania con esqueletos y en Conker,
 * ~1,5-2 ms por VI de salidas al interprete.  `_W` ya se compila con el
 * numero magico de GliGli (CVT_FP_W, MIPS-to-PPC.c), sin `psq_l` ni zona
 * roja.  `_L` se queda interpretada (64 bits, poco frecuente: no sale en
 * [CPU4]). */
#if defined(_XBOX)
#define INTERPRET_FP_L
#endif

#if 0
#define INTERPRET_FP_ADD
#define INTERPRET_FP_SUB
#define INTERPRET_FP_MUL
#define INTERPRET_FP_DIV
#define INTERPRET_FP_ABS
#define INTERPRET_FP_MOV
#define INTERPRET_FP_NEG
#endif
//#define INTERPRET_FP_SQRT

#if 0
#define INTERPRET_FP_ROUND_L
#define INTERPRET_FP_TRUNC_L
#define INTERPRET_FP_FLOOR_L
#define INTERPRET_FP_CEIL_L
#endif
#if 0
#define INTERPRET_FP_ROUND_W
#define INTERPRET_FP_TRUNC_W
#define INTERPRET_FP_FLOOR_W
#define INTERPRET_FP_CEIL_W
#endif
#if 0
#define INTERPRET_FP_CVT_S
#define INTERPRET_FP_CVT_D
#define INTERPRET_FP_CVT_W
#define INTERPRET_FP_CVT_L
#endif

#if 0
#define INTERPRET_FP_C_F
#define INTERPRET_FP_C_UN
#define INTERPRET_FP_C_EQ
#define INTERPRET_FP_C_UEQ
#define INTERPRET_FP_C_OLT
#define INTERPRET_FP_C_ULT
#define INTERPRET_FP_C_OLE
#define INTERPRET_FP_C_ULE
#define INTERPRET_FP_C_SF
#define INTERPRET_FP_C_NGLE
#define INTERPRET_FP_C_SEQ
#define INTERPRET_FP_C_NGL
#define INTERPRET_FP_C_LT
#define INTERPRET_FP_C_NGE
#define INTERPRET_FP_C_LE
#define INTERPRET_FP_C_NGT
#endif

#endif

