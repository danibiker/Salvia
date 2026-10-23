/**
 * Wii64 - MIPS-to-PPC.c
 * Copyright (C) 2007, 2008, 2009, 2010 Mike Slegeir
 * 
 * Convert MIPS code into PPC (take 2 1/2)
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
 * See the GNU General Public Licence for  more details.
 *
**/

/* TODO: Optimize idle branches (generate a call to gen_interrupt)
         Optimize instruction scheduling & reduce branch instructions
 */
#include <string.h>
/* [Xbox360] `sprintf`, que este fichero no necesitaba hasta la sonda de
 * biseccion de `start_new_block`.  DEBUG.h declara `txtbuffer` pero no
 * arrastra <stdio.h>. */
#include <stdio.h>
#include "MIPS-to-PPC.h"
#include "Register-Cache.h"
#include "Interpreter.h"
#include "Wrappers.h"
#include "../Recomp-Cache.h"
#include "../../gc_memory/memory.h"
#include "../../gui/DEBUG.h"
#include <math.h>
#include "../Invalid_Code.h"
#include "../../xenon64_diag.h"   /* XBOX64_TRAZAS */

#include <assert.h>

#ifdef USE_EXPANSION
#define RAM_TOP 0x80800000
#else
#define RAM_TOP 0x80400000
#endif

// Prototypes for functions used and defined in this file
static void genCallInterp(MIPS_instr);
#define JUMPTO_OFF  1
#define JUMPTO_ADDR 2
#define JUMPTO_OFF_SIZE  11
#define JUMPTO_ADDR_SIZE 11
static void genJumpTo(unsigned int loc, unsigned int type);
static void genJumpToRegRapido(void);   /* [Xbox360] JR/JALR: ver Wrappers.h */
static void genUpdateCount(int checkCount);
static void genCheckFP(void);
static void genCallDynaMem(memType type, int count, int reverse, int _rs, int _rt, short immed);
/* [Xbox360] ★★ [Gauntlet Legends, logs 21-25] TRADUCCION POR TLB EN EL CODIGO
 * GENERADO.  Gauntlet tiene sus datos en direcciones de TLB y cada acceso salia
 * por `dyna_mem` (~80.000 por VI; ~13,7 ms por VI aun con el atajo en C de
 * Wrappers.c, `dm_tlb_rdram`).  Con esto, en el camino lento de
 * `genCallDynaMem`, ANTES de llamar a dyna_mem, el codigo generado hace la misma
 * consulta que `dm_tlb_rdram` y, si acierta, no llama.  Por partes:
 *   1. LW y LWU sueltas (count 1)                          <- hecha
 *   2. resto de cargas y rafagas                           <- hecha
 *   3. escrituras (con la invalidacion de codigo)          <- hecha
 *   4. sin volcar los registros en el camino de TLB        <- hecha, EN PRUEBA
 *      (cargas de FASTMEM_HOT_NOFLUSH: saltan al acceso rapido de RDRAM)
 * MEDIDO (logs 24-29, sonda [DMEM] ya retirada): Gauntlet de 18-19 a 29-30
 * VI/s; llamadas a dyna_mem de ~1,9 M/s a ~0,3 M/s.  Controles sin cambios:
 * GoldenEye, Goemon (overlays sobre codigo) y Mario 64.
 * A 0 vuelve el comportamiento de antes.  Solo sin USE_TLB_CACHE (la tabla
 * plana tlb_LUT_r de tlb.c). */
#ifndef USE_TLB_CACHE
#define XBOX64_TLB_EN_LINEA 1
#else
#define XBOX64_TLB_EN_LINEA 0
#endif
static void emit_update_count_lazy(int checkCount);
static void emit_genCheckFp_lazy(void);
void RecompCache_Update(PowerPC_func*);
static int inline mips_is_jump(MIPS_instr);
/* [Xbox360] Las tres son de C (`r4300.c`, `interupt.c`) y este fichero se
 * compila como C++.  `jump_to` y `count_per_op` ya llegan con enlace C por
 * r4300.h (via Wrappers.h), asi que repetirlo aqui es consistente y no
 * redundante: `check_interupt` NO llega por ninguna cabecera. */
#ifdef __cplusplus
extern "C" {
#endif
void jump_to(unsigned int);
void check_interupt();
extern unsigned long count_per_op;
/* [Xbox360] La tabla de manejadores de la RDRAM normal (memory.c), para que
 * las escrituras de TLB en linea (genCallDynaMem, parte 3) no salten la
 * proteccion de framebuffer. */
extern void (*rw_rdram[8])();
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/* [Xbox360] ★ `extern "C"` OBLIGATORIO.  Este fichero se compila como C++
 * (es C99), asi que sin esto `&__fixdfdi` pide el simbolo DECORADO y no lo
 * define nadie.  Es la misma frontera C/C++ que en la fase 2 dejo dos
 * copias de `enablegDPUpdateColorImage`; aqui al menos avisa con LNK2001
 * en vez de fallar en silencio. */
extern "C" {
#endif
unsigned long long __udivmoddi4(unsigned long long, unsigned long long, unsigned long long*);
double __ieee754_sqrt(double);
float __ieee754_sqrtf(float);
long long __fixdfdi(double);
long long __fixsfdi(float);

/* [Xbox360] `round` y `roundf` son de C99 y VS2010 (_MSC_VER 1600) no las
 * trae: llegaron en VS2013.  Las implementa xbox/compat_posix.c.
 *
 * ★ NO PUEDEN SER UNA MACRO, que es como este arbol resuelve el resto de
 * huecos de C99 (isnan, localtime_r): el dynarec toma su DIRECCION para
 * emitir la llamada (`&round` en ROUND_L_FP), y una macro no tiene. */
#if defined(_MSC_VER) && _MSC_VER < 1800
double round(double);
float  roundf(float);
#endif
#ifdef __cplusplus
}
#endif

/* [Xbox360] [AUTO, log 46] ★ ROUND.L redondea al PAR en los empates (el
 * modo 0 del R4300, el mismo que hace `fctiw` en ROUND.W), y `round` de C
 * los aleja del cero: ROUND.L(0.5) daba 1, ROUND.L(-0.5) -1 y ROUND.L(2.5)
 * 3; los buenos son 0, 0 y 2.  `round` no se toca (compat_posix.c es C y
 * para C esta bien).  `x - floor(x)` es exacto; por encima de 2^52 x ya es
 * entero y el resto es 0.  NaN sale NaN. */
static double xbox64_redondeo_par(double x)
{
	double f = floor(x);
	double r = x - f;
	if (r > 0.5) return f + 1.0;
	if (r < 0.5) return f;
	return (fmod(f, 2.0) == 0.0) ? f : f + 1.0;
}
static float xbox64_redondeo_parf(float x)
{
	return (float)xbox64_redondeo_par((double)x);
}
/* [Xbox360] [AUTO, log 46] CVT.L usa el modo de fcr31 (mupen64plus-core,
 * fpu.h:477-480).  El original llamaba directamente a __fixdfdi, que trunca
 * siempre ("FIXME: I'm fairly certain this will always trunc").  La
 * autoprueba no lo veia porque el interprete truncaba igual.  Modo 1 =
 * devolverlo tal cual: el truncado lo hace __fixdfdi. */
static double xbox64_redondeo_modo(double x)
{
	switch (r4300.fcr31 & 3) {
	case 0:  return xbox64_redondeo_par(x);
	case 1:  return x;
	case 2:  return ceil(x);
	default: return floor(x);
	}
}
static float xbox64_redondeo_modof(float x)
{
	return (float)xbox64_redondeo_modo((double)x);
}

#define CANT_COMPILE_DELAY() \
	((get_src_pc()&0xFFF) == 0xFFC && \
	 (get_src_pc() <  0x80000000 || \
	  get_src_pc() >= 0xC0000000))

static inline unsigned short extractUpper16(unsigned int address){
	unsigned int addr = (unsigned int)address;
	return (addr>>16) + ((addr>>15)&1);
}

static inline short extractLower16(unsigned int address){
	unsigned int addr = (unsigned int)address;
	return addr&0x8000 ? (addr&0xffff)-0x10000 : addr&0xffff;
}

static int FP_need_check;

#ifdef LAZY_GEN_CALLS
// For lazy single function generation per block.
#define MAX_HELPER_CALLS 512
static PowerPC_instr* update_count_slots[MAX_HELPER_CALLS];
static int update_count_slot_count = 0;
static PowerPC_instr* update_count_addr = NULL;

static PowerPC_instr* update_count_check_slots[MAX_HELPER_CALLS];
static int update_count_check_slot_count = 0;
static PowerPC_instr* update_count_check_addr = NULL;

static PowerPC_instr* gen_checkfp_slots[MAX_HELPER_CALLS];
static int gen_checkfp_slot_count = 0;
static PowerPC_instr* gen_checkfp_addr = NULL;
#endif

// Variable to indicate whether the next recompiled instruction
//   is a delay slot (which needs to have its registers flushed)
//   and the current instruction
static int delaySlotNext, isDelaySlot;
// This should be called before the jump is recompiled
static inline int check_delaySlot(void){
	if(peek_next_src() == 0){ // MIPS uses 0 as a NOP
		get_next_src();   // Get rid of the NOP
		return 0;
	} else {
		if(mips_is_jump(peek_next_src())) return CONVERT_WARNING;
		delaySlotNext = 1;
		convert(); // This just moves the delay slot instruction ahead of the branch
		return 1;
	}
}

#define MIPS_REG_HI 32
#define MIPS_REG_LO 33

// Initialize register mappings
#if defined(_XBOX)
/* [Xbox360] `FASTMEM_HOT_NOFLUSH` es un define sin valor, asi que no se
 * puede meter en un `%d`: se convierte aqui en texto.  Aparte para no
 * partir la llamada a `sprintf` con directivas, que compila pero se lee
 * fatal y se rompe al primer retoque. */
#ifdef FASTMEM_HOT_NOFLUSH
#define XBOX64_HOT_NOFLUSH_TXT "ON"
#else
#define XBOX64_HOT_NOFLUSH_TXT "OFF"
#endif

/* [Xbox360] Sonda de las cuatro desalineadas, al recompilarlas.  Apagada:
 * ver la nota en `genCallDynaMem`. */
#define XBOX64_SONDA_DESALINEADAS 0

/* [Xbox360] Sonda del tipo del literal `0x...LL`.  Apagada: era el control
 * positivo de la sonda de LDR, y esa ya esta apagada tambien.  Lo que
 * midio esta escrito donde importa -- en el `case MEM_LDL` de
 * `dyna_mem`, junto al arreglo. */
#define XBOX64_SONDA_LITERAL 0

/* ★ LA LINEA `[DYNA] biseccion:` SE QUEDA ENCENDIDA, y no es un descuido.
 * Cuesta UNA linea por sesion y dice que se compilo de verdad.  No
 * tenerla costo una pasada entera: "la hipotesis era falsa" y "el
 * interruptor no llego al compilador" se ven igual en el log. */
#define XBOX64_SONDA_BISECCION (XBOX64_TRAZAS && 1)
#endif

void start_new_block(void){
#if defined(_XBOX) && XBOX64_SONDA_BISECCION
	/* [Xbox360] ★ QUE SE COMPILO DE VERDAD.  UNA LINEA POR SESION.
	 *
	 * Sin esto no se puede distinguir "la hipotesis era falsa" de "el
	 * interruptor no llego al compilador", y las dos cosas se ven igual
	 * en el log: el juego sigue fallando.  Ya paso una vez con el
	 * `#undef FASTMEM_HOT_NOFLUSH`, que hubo que creerse.
	 *
	 * Va en `start_new_block` porque es lo primero que corre del
	 * recompilador y aqui no cuesta nada: un `if` sobre un estatico, una
	 * vez por bloque. */
	static int xbox64_biseccion_dicha = 0;
	if(!xbox64_biseccion_dicha){
		xbox64_biseccion_dicha = 1;
		sprintf(txtbuffer,
		        "[DYNA] biseccion: cargas=%04X almacen=%d fp=%d hot_noflush=%s grupos=%04X",
		        (unsigned int)(XBOX64_CARGAS_MASCARA),
		        (int)(XBOX64_ALMACEN_AL_INTERPRETE),
		        (int)(XBOX64_FP_AL_INTERPRETE),
		        XBOX64_HOT_NOFLUSH_TXT,
		        (unsigned int)(XBOX64_GRUPOS_MASCARA));
		DEBUG_print(txtbuffer, DBG_USBGECKO);

		/* [Xbox360] ★★ DE QUE TIPO ES `0xFFFFFFFFFFFFFFFFLL` PARA ESTE
		 * COMPILADOR.  Una linea, y decide la hipotesis de LDR.
		 *
		 * `dyna_mem` usa ese literal para las cuatro desalineadas
		 * (Wrappers.c:739,751,846,859).  Si el compilador le da tipo CON
		 * SIGNO -- o sea el valor -1 -- entonces `>>` es un
		 * desplazamiento ARITMETICO y la mascara sale TODO UNOS para
		 * cualquier desplazamiento.  Con `~mask` a cero, el valor previo
		 * del registro se descarta entero, que es exactamente una LDR
		 * rota.  El `<<` no lo nota, porque el patron de bits que
		 * produce es el mismo con signo que sin el.
		 *
		 * Y eso explica la medida ENTERA: LDL usa `<<` (sana), LDR usa
		 * `>>` (rota).  Lo mismo predice SDL rota (usa `>>`) y SDR sana
		 * (usa `<<`), que cuadra con que el paso de almacenamientos
		 * saliera bien si DK64 no usa `sdl`.
		 *
		 * ★★ MEDIDO: da FFFFFFFFFFFFFFFF.  CON SIGNO.  Hipotesis
		 * CONFIRMADA y arreglada con `ULL` en Wrappers.c (los cuatro
		 * sitios; la nota larga esta alli, en el `case MEM_LDL`).
		 *
		 * ★ SE QUEDA ENCENDIDA aunque el defecto este arreglado, y no es
		 * por nostalgia: es lo que impide confundir "la sonda de LDR
		 * enmudecio porque el arreglo funciona" con "enmudecio porque no
		 * se compilo".  Mientras esta linea diga FFFFFFFF, un LDR sin
		 * `ULL` TIENE que discrepar. */
#if XBOX64_SONDA_LITERAL
		{
			unsigned long long ff56 = (unsigned long long)(0xFFFFFFFFFFFFFFFFLL >> 56);
			sprintf(txtbuffer,
			        "[DYNA] literal: (0xFF..FFLL >> 56) = %08X%08X   "
			        "(000000FF = sin signo, FFFFFFFF = CON SIGNO)",
			        (unsigned int)(ff56 >> 32), (unsigned int)ff56);
			DEBUG_print(txtbuffer, DBG_USBGECKO);
		}
#endif
	}
#endif
	invalidateRegisters();
#ifdef LAZY_GEN_CALLS
	update_count_slot_count = 0;
    update_count_addr = NULL;
	update_count_check_slot_count = 0;
	update_count_check_addr = NULL;
	gen_checkfp_slot_count = 0;
	gen_checkfp_addr = NULL;
#endif
	// Check if the previous instruction was a branch
	//   and thus whether this block begins with a delay slot
	unget_last_src();
	if(mips_is_jump(get_next_src())) delaySlotNext = 2;
	else delaySlotNext = 0;
}
void start_new_mapping(void){
	flushRegisters();
	FP_need_check = 1;
	reset_code_addr();
}

#ifdef LAZY_GEN_CALLS
static void patch_helper_slot(PowerPC_instr* slot, PowerPC_instr* helper_addr)
{
	unsigned long off = (unsigned long)((helper_addr - slot));
	*slot &= ~(PPC_LI_MASK << PPC_LI_SHIFT);
	PPC_SET_LI(*slot, off);
}


static PowerPC_instr* reserve_helper_slot(void)
{
    PowerPC_instr* slot = get_curr_dst();

    PowerPC_instr ppc = NEW_PPC_INSTR();
    PPC_SET_OPCODE(ppc, PPC_OPCODE_B);
    PPC_SET_LI(ppc, 0);     // placeholder
    PPC_SET_AA(ppc, 0);     // relative
    PPC_SET_LK(ppc, 1);     // link
    set_next_dst(ppc);

    return slot;
}

/* LAZY_GEN_CALLS is an attempt at making less generated code by only 
	placing a single version of certain generated functions into a block and then jumping to them.
	It hasn't proven to be faster.
*/
void end_block()
{
    if(update_count_slot_count > 0)
    {
        int pc_reg = R3;
        int tmp    = R4;

        update_count_addr = get_curr_dst();

        // r0 = last_pc
        GEN_LWZ(R0, offsetof(R4300,last_pc), DYNAREG_R4300);

        // tmp = pc_reg (current PC)
        GEN_OR(tmp, pc_reg, pc_reg);  // tmp = pc_reg

        // r4300.last_pc = pc_reg
        GEN_STW(pc_reg, offsetof(R4300,last_pc), DYNAREG_R4300);

        // r0 = pc - last_pc
        GEN_SUBF(R0, R0, tmp);

        // tmp = Count
        GEN_LWZ(tmp, (9*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);

		if(count_per_op == 1) {
			// r0 = (pc - last_pc) >> 2
			GEN_SRWI(R0, R0, 2);
		} 
		else if(count_per_op == 2) {
			// r0 = (pc - last_pc) >> 2
			GEN_SRWI(R0, R0, 1);
		} 
		else {
			// r0 = (pc - last_pc) >> 2
			GEN_SRWI(R0, R0, 2);
			// r0 *= count_per_op
			GEN_MULLI(R0, R0, count_per_op);
		} 

        // r0 += Count
        GEN_ADD(R0, R0, tmp);

        // Count = r0
        GEN_STW(R0, (9*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		
		GEN_BLR(0)
		//print_gecko("Generated update_count at addr %08X\r\n", update_count_addr);
		//print_gecko("current dst is %08X\r\n", get_curr_dst());
		for(int i = 0; i < update_count_slot_count; i++) {
			//print_gecko("Patch helper slot at %08X\r\n", update_count_slots[i]);
			patch_helper_slot(update_count_slots[i], update_count_addr);
		}
    }
	if(update_count_check_slot_count > 0)
    {
        int pc_reg = R3;
        int tmp    = R4;

        update_count_check_addr = get_curr_dst();

        // r0 = last_pc
        GEN_LWZ(R0, offsetof(R4300,last_pc), DYNAREG_R4300);

        // tmp = pc_reg (current PC)
        GEN_OR(tmp, pc_reg, pc_reg);  // tmp = pc_reg

        // r4300.last_pc = pc_reg
        GEN_STW(pc_reg, offsetof(R4300,last_pc), DYNAREG_R4300);

        // r0 = pc - last_pc
        GEN_SUBF(R0, R0, tmp);

        // tmp = Count
        GEN_LWZ(tmp, (9*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);

		if(count_per_op == 1) {
			// r0 = (pc - last_pc) >> 2
			GEN_SRWI(R0, R0, 2);
		} 
		else if(count_per_op == 2) {
			// r0 = (pc - last_pc) >> 2
			GEN_SRWI(R0, R0, 1);
		} 
		else {
			// r0 = (pc - last_pc) >> 2
			GEN_SRWI(R0, R0, 2);
			// r0 *= count_per_op
			GEN_MULLI(R0, R0, count_per_op);
		} 

        // r0 += Count
        GEN_ADD(R0, R0, tmp);

		// lwz    tmp, 0(&r4300.next_interrupt) // tmp = r4300.next_interrupt
		GEN_LWZ(tmp, 0+offsetof(R4300,next_interrupt), DYNAREG_R4300);
		
        // Count = r0
        GEN_STW(R0, (9*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);

		// subf   tmp, tmp, r0  // tmp = Count - next_interrupt (cycle_count)
		GEN_SUBF(tmp, tmp, R0);
		// cmpwi  cr3,  tmp, 0  // cr3 = signed cycle_count ? 0
		GEN_CMPI(CR3, tmp, 0);
		
		GEN_BLR(0)
		//print_gecko("Generated update_count_check at addr %08X\r\n", update_count_check_addr);
		//print_gecko("current dst is %08X\r\n", get_curr_dst());
		for(int i = 0; i < update_count_check_slot_count; i++) {
			//print_gecko("Patch helper slot at %08X\r\n", update_count_check_slots[i]);
			patch_helper_slot(update_count_check_slots[i], update_count_check_addr);
		}
    }
	if(gen_checkfp_slot_count > 0)
    {
        gen_checkfp_addr = get_curr_dst();

		// Save LR since we BL'd to here.
		/* [Xbox360] 32 -> CHECKFP_FRAME y 24 -> CHECKFP_LR.  Este ayudante
		 * es el UNICO sitio donde el codigo emitido se apila un marco
		 * propio (censado: no hay mas GEN_STWU sobre R1 en todo ppc/), y
		 * desde el se llama a `dyna_check_cop1_unusable`, que es C.
		 *
		 * ★ Con el marco de 32 y el LR en 24, esa llamada a C vuelca sus
		 * argumentos justo encima -- el area de parametros del ABI de Xbox
		 * 360 llega hasta +40, y los saves de ABI hasta +56.  El mismo
		 * motivo, y el mismo numero, que DYNAOFF_LR: ver Wrappers.h. */
		GEN_STWU(R1, -CHECKFP_FRAME, R1);
		GEN_MFLR(R0);
		GEN_STW(R0, CHECKFP_LR, R1);   // LR

		// lwz r0, 12*4(r4300.reg_cop0)
		GEN_LWZ(R0, (12*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		// andis. r0, r0, 0x2000
		GEN_ANDIS(R0, R0, 0x2000);
		// bne cr0, end
		GEN_BNE(CR0, 6, 0, 0);
		
		// Call dyna_check_cop1_unusable (r3 is addr, r4 is isDelaySlot)
		GEN_B(add_jump((unsigned long)(&dyna_check_cop1_unusable), 1, 1), 0, 1);
		
		GEN_ADDI(R1, R1, CHECKFP_FRAME);
		// Load the old LR
		GEN_LWZ(R0, DYNAOFF_LR, R1);
		// Restore the LR
		GEN_MTLR(R0);
		// Return to trampoline
		GEN_BLR(0);

		// Restore LR
		GEN_LWZ(R0, CHECKFP_LR, R1);
		GEN_ADDI(R1, R1, CHECKFP_FRAME);
		GEN_MTLR(R0);
		GEN_BLR(0);
		//print_gecko("Generated update_count_check at addr %08X\r\n", update_count_check_addr);
		//print_gecko("current dst is %08X\r\n", get_curr_dst());
		for(int i = 0; i < gen_checkfp_slot_count; i++) {
			//print_gecko("Patch helper slot at %08X\r\n", update_count_check_slots[i]);
			patch_helper_slot(gen_checkfp_slots[i], gen_checkfp_addr);
		}
    }
}
#endif

static inline int signExtend(int value, int size){
	int signMask = 1 << (size-1);
	int negMask = 0xffffffff << (size-1);
	if(value & signMask) value |= negMask;
	return value;
}

/* [Xbox360] [log 46] ★ SIN LA TRAMPA DE 32 BITS.  El original comparaba
 * solo la mitad baja en cuanto uno de los dos estaba mapeado a 32 bits:
 * "cheat a little bit".  Es el mismo defecto que la autoprueba midio en
 * SLT/SLTI (186 y 46 casos), aqui en BEQ/BNE/BLEZ/BGTZ/BLTZ/BGEZ y sus
 * "likely": 0x00000001_00000000 == 0 salia CIERTO si un ADDU anterior del
 * bloque habia cargado el registro a 32.  El interprete (y el R4300)
 * comparan 64 siempre.  Ahora siempre a 64: `mapRegister64` de un
 * registro mapeado a 32 trae la mitad alta de memoria si esta limpio y la
 * extiende en signo si lo escribio una operacion de 32 bits, que es
 * exactamente su valor (Register-Cache.c).  Cuesta 2-4 instrucciones por
 * salto.  `xbox64_cmp_viejo` (solo la autoprueba) devuelve la trampa.
 *
 * [log 58] ★ PERO EL CORTO SE QUEDA DONDE ES EXACTO, que es lo comun: si
 * los DOS operandos son la extension de signo de su mitad baja
 * (`isRegisterSext32`: mapeados a 32 y escritos por LW/ADDU/ADDIU...), sus
 * valores de 64 bits son esas extensiones y comparar 32 da lo mismo que
 * comparar 64.  Solo los registros de los que no se sabe nada (limpios,
 * LWU/LBU, ya de 64) pagan el camino largo. */
static void genCmp64(int cr, int _ra, int _rb){

	if((xbox64_cmp_viejo &&
	    (getRegisterMapping(_ra) == MAPPING_32 ||
	     getRegisterMapping(_rb) == MAPPING_32)) ||
	   (isRegisterSext32(_ra) && isRegisterSext32(_rb))){
		// Here we cheat a little bit: if either of the registers are mapped
		// as 32-bit, only compare the 32-bit values
		int ra = mapRegister(_ra), rb = mapRegister(_rb);
		
		GEN_CMP(cr, ra, rb);
	} else {
		RegMapping ra = mapRegister64(_ra), rb = mapRegister64(_rb);
		
		GEN_CMP(cr, ra.hi, rb.hi);
		// Skip low word comparison if high words are mismatched
		GEN_BNE(cr, 2, 0, 0);
		// Compare low words if r4300.hi words don't match
		GEN_CMPL(cr, ra.lo, rb.lo);
	}
}

/* [Xbox360] [log 46] Ver genCmp64: la misma trampa, para BLEZ/BGTZ/BLTZ/
 * BGEZ contra 0.  0x00000000_80000000 es POSITIVO a 64 bits. */
static void genCmpi64(int cr, int _ra, short immed){

	/* [Xbox360] [log 58] El corto tambien cuando es EXACTO: ver genCmp64. */
	if((xbox64_cmp_viejo && getRegisterMapping(_ra) == MAPPING_32) ||
	   isRegisterSext32(_ra)){
		// If we've mapped this register as 32-bit, don't bother with 64-bit
		int ra = mapRegister(_ra);
		
		GEN_CMPI(cr, ra, immed);
	} else {
		RegMapping ra = mapRegister64(_ra);
		
		GEN_CMPI(cr, ra.hi, immed < 0 ? ~0 : 0);
		// Skip low word comparison if high words are mismatched
		GEN_BNE(cr, 2, 0, 0);
		// Compare low words if r4300.hi words don't match
		GEN_CMPLI(cr, ra.lo, immed);
	}
}

typedef enum { NONE=0, EQ, NE, LT, GT, LE, GE } condition;
// Branch a certain offset (possibly conditionally, linking, or likely)
//   offset: N64 instructions from current N64 instruction to branch
//   cond: type of branch to execute depending on cr 7
//   link: if nonzero, branch and link
//   likely: if nonzero, the delay slot will only be executed when cond is true
static int branch(short offset, condition cond, int link, int likely){

	int likely_id;
	// Condition codes for bc (and their negations) 
	int bo, bi, nbo;
	switch(cond){
		case EQ:
			bo = 0xc, nbo = 0x4, bi = 18;
			break;
		case NE:
			bo = 0x4, nbo = 0xc, bi = 18;
			break;
		case LT:
			bo = 0xc, nbo = 0x4, bi = 16;
			break;
		case GE:
			bo = 0x4, nbo = 0xc, bi = 16;
			break;
		case GT:
			bo = 0xc, nbo = 0x4, bi = 17;
			break;
		case LE:
			bo = 0x4, nbo = 0xc, bi = 17;
			break;
		default:
			bo = 0x14; nbo = 0x4; bi = 19;
			break;
	}

	flushRegisters();

	if(link){
		// Set LR to next instruction
		int lr = mapRegisterNew(MIPS_REG_LR);
		// lis	lr, pc@ha(0)
		GEN_LIS(lr, extractUpper16(get_src_pc()+8));
		// la	lr, pc@l(lr)
		GEN_ADDI(lr, lr, extractLower16(get_src_pc()+8));
		flushRegisters();
	}

	if(likely){
		// b[!cond] <past delay to update_count>
		likely_id = add_jump_special(0);
		GEN_BC(likely_id, 0, 0, nbo, bi);
	}

	// Check the delay slot, and note how big it is
	PowerPC_instr* preDelay = get_curr_dst();
	check_delaySlot();
	int delaySlot = get_curr_dst() - preDelay;

#ifdef COMPARE_CORE
	GEN_LI(R4, 1);
	if(likely){
		GEN_B(2, 0, 0);
		GEN_LI(R4, 0);

		set_jump_special(likely_id, delaySlot+2+1);
	}
#else
	if(likely) set_jump_special(likely_id, delaySlot+1);
#endif

	genUpdateCount(1); // Sets cr3 to (cycle_count ? 0)

#ifndef INTERPRET_BRANCH
	// If we're jumping out, we need to trampoline using genJumpTo
	if(is_j_out(offset, 0)){
#endif // INTEPRET_BRANCH

		// b[!cond] <past jumpto & delay>
		//   Note: if there's a delay slot, I will branch to the branch over it
		GEN_BC(JUMPTO_OFF_SIZE+1, 0, 0, nbo, bi);

		genJumpTo(offset, JUMPTO_OFF);

		// The branch isn't taken, but we need to check interrupts
		// Load the address of the next instruction
		GEN_LIS(R3, extractUpper16(get_src_pc()+4));
		GEN_ADDI(R3, R3, extractLower16(get_src_pc()+4));
		// If taking the interrupt, return to the trampoline
		GEN_BGELR(CR3, 0);

#ifndef INTERPRET_BRANCH
	} else {
		// r4300.last_pc = naddr
		if(cond != NONE){
			GEN_BC(4, 0, 0, bo, bi);
			GEN_LIS(R3, extractUpper16(get_src_pc()+4));
			GEN_ADDI(R3, R3, extractLower16(get_src_pc()+4));
			GEN_B(3, 0, 0);
		}
		GEN_LIS(R3, extractUpper16(get_src_pc() + (offset<<2)));
		GEN_ADDI(R3, R3, extractLower16(get_src_pc() + (offset<<2)));
		GEN_STW(R3, offsetof(R4300,last_pc), DYNAREG_R4300);

		// If taking the interrupt, return to the trampoline
		GEN_BGELR(CR3, 0);
		// The actual branch
#if 0
		// FIXME: Reenable this when blocks are small enough to BC within
		//          Make sure that pass2 uses BD/LI as appropriate
		GEN_BC(add_jump(offset, 0, 0), 0, 0, bo, bi);
#else
		GEN_BC(2, 0, 0, nbo, bi);
		GEN_B(add_jump(offset, 0, 0), 0, 0);
#endif

	}
#endif // INTERPRET_BRANCH

	// Let's still recompile the delay slot in place in case its branched to
	// Unless the delay slot is in the next block, in which case there's nothing to skip
	//   Testing is_j_out with an offset of 0 checks whether the delay slot is out
	if(delaySlot){
		if(is_j_dst(0) && !is_j_out(0, 0)){
			// Step over the already executed delay slot if the branch isn't taken
			// b delaySlot+1
			GEN_B(delaySlot+1, 0, 0);
			unget_last_src();
			delaySlotNext = 2;
		}
	} else nop_ignored();

#ifdef INTERPRET_BRANCH
	return INTERPRETED;
#else // INTERPRET_BRANCH
	return CONVERT_SUCCESS;
#endif
}

#define check_memory_range(nbytes) \
{ \
invalidateRegisters(); \
GEN_LI(R4, (nbytes)); \
GEN_B(add_jump((unsigned long)&invalidate_func_range, 1, 1), 0, 1); \
GEN_LWZ(R0, DYNAOFF_LR, R1); \
GEN_MTLR(R0); \
}

/* [Xbox360] ★ DEFINICION TENTATIVA DE C, QUE C++ NO TIENE.
 *
 * En C esta linea no define nada: anuncia la tabla para que `convert()`
 * (mas abajo, :630) pueda usarla, y la definicion de verdad -- con su
 * inicializador de 64 entradas -- llega al final del fichero.  Es legal en
 * C y se llama "definicion tentativa".
 *
 * C++ no las tiene: aqui esto YA ES una definicion (inicializada a cero), y
 * la del final es una SEGUNDA.  De ahi el `error C2086: 'gen_ops' :
 * redefinition`.
 *
 * ★ EL PRECIO ES QUE LA TABLA PASA A TENER ENLACE EXTERNO, y hay que
 * decirlo: en C++ la unica forma de declarar sin definir en ambito de
 * fichero es `extern`, y `extern` primero con `static` despues es
 * ilegal.  Comprobado que no hay colision: `gen_ops` aparece en TRES
 * lineas de todo el arbol, las dos de aqui y el uso de :630.
 *
 * Las otras seis tablas del fichero (gen_special, gen_tlb, gen_cop0,
 * gen_cop1, gen_cop1_fp) se definen UNA sola vez con su inicializador, asi
 * que no les pasa nada.  Censadas. */
#ifdef __cplusplus
extern int (*gen_ops[64])(MIPS_instr);
#else
static int (*gen_ops[64])(MIPS_instr);
#endif

#if defined(_XBOX)
int xbox64_censo_silencio = 0;   /* ver MIPS-to-PPC.h */
#endif
int xbox64_cmp_viejo = 0;        /* ver MIPS-to-PPC.h y genCmp64 */

int convert(void){
	int needFlush = delaySlotNext;
	isDelaySlot = (delaySlotNext == 1);
	delaySlotNext = 0;

	MIPS_instr mips = get_next_src();
#if 0
	// This probably isn't even worth it...
	if (!isDelaySlot && !is_j_dst(1) &&
        MIPS_GET_OPCODE(mips) == MIPS_OPCODE_LW &&
        has_next_src())
    {
        MIPS_instr next = peek_next_src();

        if (MIPS_GET_OPCODE(next) == MIPS_OPCODE_SW)
        {
            int lw_rs = MIPS_GET_RS(mips);
            int lw_rt = MIPS_GET_RT(mips);

            int sw_rs = MIPS_GET_RS(next);
            int sw_rt = MIPS_GET_RT(next);

            short lw_off = MIPS_GET_IMMED(mips);
            short sw_off = MIPS_GET_IMMED(next);

			if (lw_rt == sw_rt && lw_rt != lw_rs)
			{
				if (isRegisterConstant(lw_rs) && isRegisterConstant(sw_rs))
				{
					unsigned long base = getRegisterConstant(lw_rs);
					unsigned long base_sw = getRegisterConstant(sw_rs);

					// Only handle RDRAM range explicitly
					if (base >= 0x80000000 && base < 0x80800000 && base_sw >= 0x80000000 && base_sw < 0x80800000)
					{
						unsigned long vaddr_lw = base + lw_off;
						unsigned long vaddr_sw = base_sw + sw_off;

						unsigned long rdram_lw = (vaddr_lw - 0x80000000) & 0x7FFFFF;
						unsigned long rdram_sw = (vaddr_sw - 0x80000000) & 0x7FFFFF;

						int tmp = mapRegisterTemp();
						int rt = mapRegisterNew(lw_rt);

						// Build RDRAM address for LW
						GEN_LIS(tmp, extractUpper16(rdram_lw));
						GEN_ADDI(tmp, tmp, extractLower16(rdram_lw));
						GEN_ADD(tmp, tmp, DYNAREG_RDRAM);
						GEN_LWZ(rt, 0, tmp);

						// Build RDRAM address for SW
						GEN_LIS(tmp, extractUpper16(rdram_sw));
						GEN_ADDI(tmp, tmp, extractLower16(rdram_sw));
						GEN_ADD(tmp, tmp, DYNAREG_RDRAM);
						GEN_STW(rt, 0, tmp);

						// Unmap temp before flush
						unmapRegisterTemp(tmp);
						flushRegisters();

						// Set R3 = virtual address for invalidate_func
						GEN_LIS(R3, vaddr_sw >> 16);
						GEN_ORI(R3, R3, vaddr_sw & 0xFFFF);
						check_memory_range(4);

						// Consume SW
						get_next_src();
						return CONVERT_SUCCESS;
					}
				}
			}
			if(lw_rs == sw_rs && lw_off == sw_off) {
				print_gecko("dead load/store\r\n");
			}
        }
    }
#endif
#if defined(_XBOX) && XBOX64_CENSO
	/* [Xbox360] [log 42] El PC ANTES de generar: los saltos consumen su
	 * hueco de retardo y mueven `src`. */
	const unsigned int xbox64_pc_sonda = get_src_pc();
#endif
	int result = gen_ops[MIPS_GET_OPCODE(mips)](mips);

#if defined(_XBOX) && XBOX64_CENSO
	/* [Xbox360] ★★ [log 45] EL CENSO DE CLASES DE INSTRUCCION.
	 *
	 * Goemon's Great Adventure se queda en negro al empezar la aventura con
	 * el recompilador y AVANZA con el interprete.  La biseccion de DK64 ya
	 * exculpo cargas, almacenamientos y FP -- pero solo para DK64: lo que DK64
	 * no ejecutaba no se probo nunca.  En vez de bisecar a ciegas, esto dice
	 * QUE se compila: escribe cada clase de instruccion la PRIMERA vez que se
	 * convierte, con su numero de orden, su PC y como se trato.  El arranque
	 * y el menu (que funcionan) dejan su lista; las clases que salen por
	 * primera vez AL ELEGIR PERSONAJE, justo antes del negro, son las unicas
	 * sospechosas y las unicas que hay que bisecar.
	 *
	 * Absorbe la sonda de TIRADA (log 42-43): una clase que acaba en
	 * CONVERT_ERROR sale marcada asi, porque `recompile_block` ignora el
	 * retorno y esa instruccion no emite nada
	 * (reference_wii64_convert_error_no_emite).
	 *
	 * CLASE = opcode + su subcampo (func en SPECIAL, rt en REGIMM y CACHE, rs
	 * en COPz), y ademas el campo de funcion en COP1 S/D/W/L (ahi se
	 * distinguen TRUNC, ROUND, CVT, C.cond...) y en COP0 CO (TLBWI, ERET...). */
	if(!xbox64_censo_silencio){
		static unsigned char xbox64_censo[(64 * 64 + 5 * 64) / 8];
		static unsigned int  xbox64_censo_n = 0;
		const unsigned int op = MIPS_GET_OPCODE(mips);
		const unsigned int fn = mips & 0x3F;
		unsigned int sub = 0;
		unsigned int k;
		if(op == 0)                    sub = fn;                   /* SPECIAL: func */
		else if(op == 1)               sub = (mips >> 16) & 0x1F;  /* REGIMM: rt    */
		else if(op >= 16 && op <= 19)  sub = (mips >> 21) & 0x1F;  /* COPz: rs      */
		else if(op == 47)              sub = (mips >> 16) & 0x1F;  /* CACHE: op     */
		k = op * 64 + sub;
		if(op == 17 && (sub == 16 || sub == 17 || sub == 20 || sub == 21))
			k = 64 * 64 + ((sub == 16) ? 0 : (sub == 17) ? 1 : (sub == 20) ? 2 : 3) * 64 + fn;
		else if(op == 16 && sub >= 16)
			k = 64 * 64 + 4 * 64 + fn;
		if(!(xbox64_censo[k >> 3] & (1 << (k & 7)))){
			xbox64_censo[k >> 3] |= (unsigned char)(1 << (k & 7));
			xbox64_censo_n++;
			sprintf(txtbuffer, "[DYNA] censo #%u %s: instr=%08X op=%u sub=%u fn=%u pc=%08X",
			        xbox64_censo_n,
			        (result == CONVERT_ERROR) ? "* TIRADA (no se emite nada)" :
			        (result == INTERPRETED)   ? "INTERPRETA" : "recompila",
			        (unsigned int)mips, op, sub, fn, xbox64_pc_sonda);
			DEBUG_print(txtbuffer, DBG_USBGECKO);
		}
	}
#endif

	if(needFlush) flushRegisters();
	return result;
}

static int NI(MIPS_instr mips){
	return CONVERT_ERROR;
}

static int NI_COP(MIPS_instr mips, int _){
	return CONVERT_ERROR;
}

/* [Xbox360] ★★ [Resident Evil 2, 10 VI/s] EL SALTO A SI MISMO SOLO ES "OCIOSO"
 * CON UN NOP EN EL HUECO.
 *
 * Wii64 (y not64) mandan al interprete TODO salto a si mismo (J, BEQ r,r y
 * REGIMM con desplazamiento -1) para que se salte el tiempo hasta la siguiente
 * interrupcion.  Pero el interprete solo se lo salta si el hueco de retardo es
 * un NOP (pure_interp.c, `probe_nop(pc+4)`, como mupen64plus); si no, ejecuta
 * UNA vuelta y vuelve.  Con el dynarec eso es una ida y vuelta al despachador
 * por vuelta.  MEDIDO (logs 8, sondas [DYNAD]/[DYNAI]): el hilo ocioso de RE2
 * en 80000518 pasaba 1.780.000 veces/s por decodeNInterpret, el ~45% de la CPU,
 * y el juego iba a 10 VI/s.
 *
 * Con otra instruccion en el hueco el interprete no se salta nada, asi que
 * compilar el bucle hace lo MISMO dentro del codigo generado: el hueco y el
 * salto, contando ciclos, hasta que vence la interrupcion (GEN_BGELR).  El
 * caso del NOP sigue igual.  `peek_next_src` solo si el hueco esta en la
 * misma pagina (CANT_COMPILE_DELAY va antes).  A 0 vuelve lo de Wii64.
 * Resultado (log 9): RE2 de 10 a 60 VI/s; su hueco era 64840001 (addiu). */
#define XBOX64_OCIOSO_SOLO_CON_NOP 1
#if XBOX64_OCIOSO_SOLO_CON_NOP
#define OCIOSO_HUECO_NOP() (peek_next_src() == 0)
#else
#define OCIOSO_HUECO_NOP() 1
#endif

// -- Primary Opcodes --

static int J(MIPS_instr mips){
	unsigned int naddr = (MIPS_GET_LI(mips)<<2)|((get_src_pc()+4)&0xf0000000);

	if(CANT_COMPILE_DELAY() ||
	   (naddr == get_src_pc() && OCIOSO_HUECO_NOP())){
		// J_IDLE || virtual delay
		genCallInterp(mips);
		return INTERPRETED;
	}

	flushRegisters();
	reset_code_addr();

	// Check the delay slot, and note how big it is
	PowerPC_instr* preDelay = get_curr_dst();
	check_delaySlot();
	int delaySlot = get_curr_dst() - preDelay;

#ifdef COMPARE_CORE
	GEN_LI(R4, 1);
#endif
	// Sets cr3 to (cycle_count ? 0)
	genUpdateCount(1);

#ifdef INTERPRET_J
	genJumpTo(MIPS_GET_LI(mips), JUMPTO_ADDR);
#else // INTERPRET_J
	// If we're jumping out, we can't just use a branch instruction
	if(is_j_out(MIPS_GET_LI(mips), 1)){
		genJumpTo(MIPS_GET_LI(mips), JUMPTO_ADDR);
	} else {
		// r4300.last_pc = naddr
		GEN_LIS(R3, extractUpper16(naddr));
		GEN_ADDI(R3, R3, extractLower16(naddr));
		GEN_STW(R3, offsetof(R4300,last_pc), DYNAREG_R4300);

		// if(cycle_count >= 0) return;
		GEN_BGELR(CR3, 0);

		// Even though this is an absolute branch
		//   in pass 2, we generate a relative branch
		GEN_B(add_jump(MIPS_GET_LI(mips), 1, 0), 0, 0);
	}
#endif

	// Let's still recompile the delay slot in place in case its branched to
	if(delaySlot){ if(is_j_dst(0)){ unget_last_src(); delaySlotNext = 2; } }
	else nop_ignored();

#ifdef INTERPRET_J
	return INTERPRETED;
#else // INTERPRET_J
	return CONVERT_SUCCESS;
#endif
}

static int JAL(MIPS_instr mips){
	unsigned int naddr = (MIPS_GET_LI(mips)<<2)|((get_src_pc()+4)&0xf0000000);

	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	flushRegisters();
	reset_code_addr();

	// Check the delay slot, and note how big it is
	PowerPC_instr* preDelay = get_curr_dst();
	check_delaySlot();
	int delaySlot = get_curr_dst() - preDelay;

#ifdef COMPARE_CORE
	GEN_LI(R4, 1);
#endif
	// Sets cr3 to (cycle_count ? 0)
	genUpdateCount(1);

	// Set LR to next instruction
	int lr = mapRegisterNew(MIPS_REG_LR);
	// lis	lr, pc@ha(0)
	GEN_LIS(lr, extractUpper16(get_src_pc()+4));
	// la	lr, pc@l(lr)
	GEN_ADDI(lr, lr, extractLower16(get_src_pc()+4));

	flushRegisters();

#ifdef INTERPRET_JAL
	genJumpTo(MIPS_GET_LI(mips), JUMPTO_ADDR);
#else // INTERPRET_JAL
	// If we're jumping out, we can't just use a branch instruction
	if(is_j_out(MIPS_GET_LI(mips), 1)){
		genJumpTo(MIPS_GET_LI(mips), JUMPTO_ADDR);
	} else {
		// r4300.last_pc = naddr
		GEN_LIS(R3, extractUpper16(naddr));
		GEN_ADDI(R3, R3, extractLower16(naddr));
		GEN_STW(R3, offsetof(R4300,last_pc), DYNAREG_R4300);

		/// if(cycle_count >= 0) return;
		GEN_BGELR(CR3, 0);

		// Even though this is an absolute branch
		//   in pass 2, we generate a relative branch
		GEN_B(add_jump(MIPS_GET_LI(mips), 1, 0), 0, 0);
	}
#endif

	// Let's still recompile the delay slot in place in case its branched to
	if(delaySlot){ if(is_j_dst(0)){ unget_last_src(); delaySlotNext = 2; } }
	else nop_ignored();

#ifdef INTERPRET_JAL
	return INTERPRETED;
#else // INTERPRET_JAL
	return CONVERT_SUCCESS;
#endif
}

static int BEQ(MIPS_instr mips){

	if(CANT_COMPILE_DELAY() ||
	   (MIPS_GET_IMMED(mips) == 0xffff &&
	    MIPS_GET_RA(mips) == MIPS_GET_RB(mips) && OCIOSO_HUECO_NOP())){
		// BEQ_IDLE || virtual delay
		genCallInterp(mips);
		return INTERPRETED;
	}
	
	genCmp64(CR4, MIPS_GET_RA(mips), MIPS_GET_RB(mips));

	return branch(MIPS_GET_IMMED(mips), EQ, 0, 0);
}

static int BNE(MIPS_instr mips){

	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	genCmp64(CR4, MIPS_GET_RA(mips), MIPS_GET_RB(mips));

	return branch(MIPS_GET_IMMED(mips), NE, 0, 0);
}

static int BLEZ(MIPS_instr mips){

	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	genCmpi64(CR4, MIPS_GET_RA(mips), 0);

	return branch(MIPS_GET_IMMED(mips), LE, 0, 0);
}

static int BGTZ(MIPS_instr mips){

	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	genCmpi64(CR4, MIPS_GET_RA(mips), 0);

	return branch(MIPS_GET_IMMED(mips), GT, 0, 0);
}

static int ADDIU(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);
	
	int rs = mapRegister(_rs);
	int rt = mapConstantNew(_rt, isRegisterConstant(_rs), 1);
	setRegisterConstant(_rt, getRegisterConstant(_rs) + immed);
	
	GEN_ADDI(rt, rs, immed);
	return CONVERT_SUCCESS;
}

static int ADDI(MIPS_instr mips){
	return ADDIU(mips);
}

static int SLTI(MIPS_instr mips){
#ifdef INTERPRET_SLTI
	genCallInterp(mips);
	return INTERPRETED;
#else
	/* [Xbox360] [AUTO, log 46] ★ A 64 BITS: ver SLT.  El inmediato se
	 * extiende en signo a 64: su mitad alta es 0 o -1 (cabe en CMPI) y la
	 * baja va a un temporal, porque comparada SIN signo no cabe en CMPLI
	 * cuando es negativa.  El original daba 1 para 0x00000000_80000000 < 0.
	 * La temporal se pide ANTES que rt (rt == rs suelta la mitad alta). */
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);
	short alto = (short)(immed < 0 ? -1 : 0);   /* mitad alta del inmediato */

	/* [Xbox360] [log 58] El corto de Wii64 donde es exacto: ver genCmp64. */
	if(isRegisterSext32(_rs)){
		int rs = mapRegister(_rs);
		int rt = mapConstantNew(_rt, isRegisterConstant(_rs), 0);
		int sa;
		setRegisterConstant(_rt, getRegisterConstant(_rs) < immed ? 1 : 0);
		sa = mapRegisterTemp();
		GEN_CMPI(CR0, rs, immed);
		GEN_MFCR(sa);
		GEN_RLWINM(rt, sa, 1, 31, 31);
		unmapRegisterTemp(sa);
		return CONVERT_SUCCESS;
	}

	RegMapping rs = mapRegister64(_rs);
	int sa = mapRegisterTemp();
	int rt = mapConstantNew(_rt, isRegisterConstant(_rs), 0);
	setRegisterConstant(_rt, getRegisterConstant64(_rs) < (long long)immed ? 1 : 0);

	GEN_LI(sa, immed);
	GEN_CMPI(CR0, rs.hi, alto);
	GEN_BNE(CR0, 2, 0, 0);
	GEN_CMPL(CR0, rs.lo, sa);
	GEN_MFCR(sa);
	GEN_RLWINM(rt, sa, 1, 31, 31);
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int SLTIU(MIPS_instr mips){
#ifdef INTERPRET_SLTIU
	genCallInterp(mips);
	return INTERPRETED;
#else
	/* [Xbox360] [AUTO, log 46] ★ A 64 BITS: ver SLT.  El inmediato se
	 * extiende en signo a 64 y DESPUES se compara sin signo (pure_interp.c:
	 * `(unsigned long long)((long long)iimmediate)`).  El original daba 1
	 * para 0x80000000_00000000 < 1.  Su constante, ademas, comparaba con el
	 * inmediato SIN extender: SLTIU x, y, -1 se apuntaba como `y < 0xFFFF`.
	 * La mitad alta del inmediato (0 o 0xFFFFFFFF) tampoco cabe en CMPLI:
	 * tambien va por el temporal, y LI no toca CR0. */
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);
	short alto = (short)(immed < 0 ? -1 : 0);   /* mitad alta del inmediato */

	/* [Xbox360] [log 58] El corto de Wii64 donde es exacto: ver genCmp64.
	 * ADDIC extiende el inmediato en signo: rs <u (u32)(s32)immed, que con
	 * rs extendido en signo es lo mismo que a 64 bits. */
	if(isRegisterSext32(_rs)){
		int rs = mapRegister(_rs);
		int rt = mapConstantNew(_rt, isRegisterConstant(_rs), 0);
		setRegisterConstant(_rt, (unsigned long long)getRegisterConstant64(_rs) <
		                         (unsigned long long)(long long)immed ? 1 : 0);
		GEN_NOT(R0, rs);
		GEN_ADDIC(R0, R0, immed);
		GEN_ADDZE(rt, DYNAREG_ZERO);
		return CONVERT_SUCCESS;
	}

	RegMapping rs = mapRegister64(_rs);
	int sa = mapRegisterTemp();
	int rt = mapConstantNew(_rt, isRegisterConstant(_rs), 0);
	setRegisterConstant(_rt, (unsigned long long)getRegisterConstant64(_rs) <
	                         (unsigned long long)(long long)immed ? 1 : 0);

	GEN_LI(sa, alto);
	GEN_CMPL(CR0, rs.hi, sa);
	GEN_LI(sa, immed);
	GEN_BNE(CR0, 2, 0, 0);
	GEN_CMPL(CR0, rs.lo, sa);
	GEN_MFCR(sa);
	GEN_RLWINM(rt, sa, 1, 31, 31);
	unmapRegisterTemp(sa);

	return CONVERT_SUCCESS;
#endif
}

static int ANDI(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	unsigned short immed = MIPS_GET_IMMED(mips);
	
	int rs = mapRegister(_rs);
	int rt = mapConstantNew(_rt, isRegisterConstant(_rs), 0);
	setRegisterConstant(_rt, getRegisterConstant(_rs) & immed);
	
	GEN_ANDI(rt, rs, immed);
	return CONVERT_SUCCESS;
}

static int ORI(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	unsigned short immed = MIPS_GET_IMMED(mips);
	
	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapConstant64New(_rt, isRegisterConstant(_rs));
	setRegisterConstant64(_rt, getRegisterConstant64(_rs) | immed);

	GEN_MR(rt.hi, rs.hi);
	GEN_ORI(rt.lo, rs.lo, immed);

  	return CONVERT_SUCCESS;
}

static int XORI(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	unsigned short immed = MIPS_GET_IMMED(mips);
	
	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapConstant64New(_rt, isRegisterConstant(_rs));
	setRegisterConstant64(_rt, getRegisterConstant64(_rs) ^ immed);
  
	GEN_MR(rt.hi, rs.hi);
	GEN_XORI(rt.lo, rs.lo, immed);  
	return CONVERT_SUCCESS;
}

static int LUI(MIPS_instr mips){
	int _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

	int rt = mapConstantNew(_rt, 1, 1);
	setRegisterConstant(_rt, immed << 16);
	GEN_LIS(rt, immed);
	return CONVERT_SUCCESS;
}

static int BEQL(MIPS_instr mips){

	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	genCmp64(CR4, MIPS_GET_RA(mips), MIPS_GET_RB(mips));

	return branch(MIPS_GET_IMMED(mips), EQ, 0, 1);
}

static int BNEL(MIPS_instr mips){

	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	genCmp64(CR4, MIPS_GET_RA(mips), MIPS_GET_RB(mips));

	return branch(MIPS_GET_IMMED(mips), NE, 0, 1);
}

static int BLEZL(MIPS_instr mips){

	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	genCmpi64(CR4, MIPS_GET_RA(mips), 0);

	return branch(MIPS_GET_IMMED(mips), LE, 0, 1);
}

static int BGTZL(MIPS_instr mips){

	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	genCmpi64(CR4, MIPS_GET_RA(mips), 0);

	return branch(MIPS_GET_IMMED(mips), GT, 0, 1);
}

static int DADDIU(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DADDIU)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DADDIU

	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapConstant64New(_rt, isRegisterConstant(_rs));
	setRegisterConstant64(_rt, getRegisterConstant64(_rs) + immed);

	// Add the immediate to the LSW
	GEN_ADDIC(rt.lo, rs.lo, immed);
	// Add the MSW with the sign-extension and the carry
	if(immed < 0){
		GEN_ADDME(rt.hi, rs.hi);
	} else {
		GEN_ADDZE(rt.hi, rs.hi);
	}
	return CONVERT_SUCCESS;
#endif
}

static int DADDI(MIPS_instr mips){
	return DADDIU(mips);
}

static int LDL(MIPS_instr mips){
#ifdef INTERPRET_LDL
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LDL
	genCallDynaMem(MEM_LDL, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

static int LDR(MIPS_instr mips){
#ifdef INTERPRET_LDR
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LDR
	genCallDynaMem(MEM_LDR, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

static int LB(MIPS_instr mips){
#ifdef INTERPRET_LB
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LB
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_LB && MIPS_GET_RS(peek) == _rs && MIPS_GET_RS(peek) != _rt){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 1){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LB)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 1)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 1){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LB)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 1)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_LB, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int LH(MIPS_instr mips){
#ifdef INTERPRET_LH
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LH
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_LH && MIPS_GET_RS(peek) == _rs && MIPS_GET_RS(peek) != _rt){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 2){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LH)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 2)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 2){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LH)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 2)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_LH, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int LWL(MIPS_instr mips){
#ifdef INTERPRET_LWL
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LWL
	genCallDynaMem(MEM_LWL, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

static int LW(MIPS_instr mips){
#ifdef INTERPRET_LW
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LW
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_LW && MIPS_GET_RS(peek) == _rs && MIPS_GET_RS(peek) != _rt){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LW)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 4)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LW)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 4)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_LW, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int LBU(MIPS_instr mips){
#ifdef INTERPRET_LBU
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LBU
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_LBU && MIPS_GET_RS(peek) == _rs && MIPS_GET_RS(peek) != _rt){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 1){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LBU)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 1)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 1){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LBU)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 1)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_LBU, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int LHU(MIPS_instr mips){
#ifdef INTERPRET_LHU
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LHU
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_LHU && MIPS_GET_RS(peek) == _rs && MIPS_GET_RS(peek) != _rt){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 2){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LHU)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 2)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 2){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LHU)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 2)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_LHU, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int LWR(MIPS_instr mips){
#ifdef INTERPRET_LWR
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LWR
	genCallDynaMem(MEM_LWR, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

static int LWU(MIPS_instr mips){
#ifdef INTERPRET_LWU
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LWU
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_LWU && MIPS_GET_RS(peek) == _rs && MIPS_GET_RS(peek) != _rt){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LWU)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 4)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LWU)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 4)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_LWU, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int SB(MIPS_instr mips){
#ifdef INTERPRET_SB
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SB
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_SB && MIPS_GET_RS(peek) == _rs){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 1){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SB)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 1)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 1){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SB)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 1)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_SB, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int SH(MIPS_instr mips){
#ifdef INTERPRET_SH
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SH
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_SH && MIPS_GET_RS(peek) == _rs){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 2){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SH)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 2)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 2){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SH)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 2)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_SH, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int SWL(MIPS_instr mips){
#ifdef INTERPRET_SWL
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SWL
	genCallDynaMem(MEM_SWL, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

static int SW(MIPS_instr mips){
#ifdef INTERPRET_SW
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SW
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_SW && MIPS_GET_RS(peek) == _rs){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SW)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 4)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SW)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 4)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_SW, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int SDL(MIPS_instr mips){
#ifdef INTERPRET_SDL
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SDL
	genCallDynaMem(MEM_SDL, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

static int SDR(MIPS_instr mips){
#ifdef INTERPRET_SDR
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SDR
	genCallDynaMem(MEM_SDR, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

static int SWR(MIPS_instr mips){
#ifdef INTERPRET_SWR
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SWR
	genCallDynaMem(MEM_SWR, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

static int LD(MIPS_instr mips){
#ifdef INTERPRET_LD
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LD
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_LD && MIPS_GET_RS(peek) == _rs && MIPS_GET_RS(peek) != _rt){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 8){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LD)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 8)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 8){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LD)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RS(peek) == MIPS_GET_RT(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 8)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_LD, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int SD(MIPS_instr mips){
#ifdef INTERPRET_SD
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SD
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_SD && MIPS_GET_RS(peek) == _rs){
			if(MIPS_GET_RT(peek) == _rt + 1 && MIPS_GET_IMMED(peek) == immed + 8){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SD)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 8)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 1 && MIPS_GET_IMMED(peek) == immed - 8){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SD)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 1)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 8)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_SD, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int LWC1(MIPS_instr mips){
#ifdef INTERPRET_LWC1
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LWC1
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_LWC1 && MIPS_GET_RS(peek) == _rs){
			if(MIPS_GET_RT(peek) == _rt + 2 && MIPS_GET_IMMED(peek) == immed + 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LWC1)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 2)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 4)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 2 && MIPS_GET_IMMED(peek) == immed - 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LWC1)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 2)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 4)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_LWC1, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int LDC1(MIPS_instr mips){
#ifdef INTERPRET_LDC1
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LDC1
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_LDC1 && MIPS_GET_RS(peek) == _rs){
			if(MIPS_GET_RT(peek) == _rt + 2 && MIPS_GET_IMMED(peek) == immed + 8){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LDC1)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 2)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 8)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 2 && MIPS_GET_IMMED(peek) == immed - 8){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_LDC1)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 2)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 8)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_LDC1, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int SWC1(MIPS_instr mips){
#ifdef INTERPRET_SWC1
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SWC1
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_SWC1 && MIPS_GET_RS(peek) == _rs){
			if(MIPS_GET_RT(peek) == _rt + 2 && MIPS_GET_IMMED(peek) == immed + 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SWC1)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 2)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 4)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 2 && MIPS_GET_IMMED(peek) == immed - 4){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SWC1)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 2)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 4)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_SWC1, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int SDC1(MIPS_instr mips){
#ifdef INTERPRET_SDC1
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SDC1
	int count = 1;
	int reverse = 0;
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);

#ifdef FASTMEM
	if(!isDelaySlot && !is_j_dst(1)){
		MIPS_instr peek = peek_next_src();
		if(MIPS_GET_OPCODE(peek) == MIPS_OPCODE_SDC1 && MIPS_GET_RS(peek) == _rs){
			if(MIPS_GET_RT(peek) == _rt + 2 && MIPS_GET_IMMED(peek) == immed + 8){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SDC1)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) + 2)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) + 8)
						break;
					mips = get_next_src();
					count++;
				}
			} else if(MIPS_GET_RT(peek) == _rt - 2 && MIPS_GET_IMMED(peek) == immed - 8){
				mips = get_next_src();
				count++;
				while(has_next_src() && !is_j_dst(1)){
					peek = peek_next_src();
					if(MIPS_GET_OPCODE(peek) != MIPS_OPCODE_SDC1)
						break;
					if(MIPS_GET_RS(peek) != MIPS_GET_RS(mips))
						break;
					if(MIPS_GET_RT(peek) != MIPS_GET_RT(mips) - 2)
						break;
					if(MIPS_GET_IMMED(peek) != MIPS_GET_IMMED(mips) - 8)
						break;
					mips = get_next_src();
					count++;
				}
				reverse = 1;
				_rt = MIPS_GET_RT(mips);
				immed = MIPS_GET_IMMED(mips);
			}
		}
	}
#endif

	genCallDynaMem(MEM_SDC1, count, reverse, _rs, _rt, immed);
	return CONVERT_SUCCESS;
#endif
}

static int CACHE(MIPS_instr mips){
#if !defined(_XBOX)
	return CONVERT_ERROR;
#else
	/* [Xbox360] ★★★ CACHE NO SE EMITIA, Y NO ES UN HUECO DE ESTE PORT:
	 * Wii64 upstream tiene el mismo `return CONVERT_ERROR` (su
	 * MIPS-to-PPC.c:1961).  Portado de **not64** (r4300/ppc/MIPS-to-PPC.c
	 * :2375), que es de la misma estirpe y si la implementa.
	 *
	 * ★ Y `CONVERT_ERROR` NO MANDA AL INTERPRETE.  `recompile_block`
	 * llama a `convert()` IGNORANDO el retorno (Recompile.c:296), asi que
	 * la instruccion no generaba NADA: cada CACHE del invitado se tiraba
	 * en silencio.  Eso es lo que lo hace peligroso y no solo incompleto.
	 *
	 * ★ POR QUE IMPORTA, Y A QUE JUEGOS.  Un juego que carga overlays de
	 * codigo por DMA llama despues a `osInvalICache`, que emite CACHE con
	 * la direccion VIRTUAL.  Es la UNICA via que alcanza al codigo que
	 * vive en KUSEG por TLB: la invalidacion que hace la DMA conoce solo
	 * la pagina FISICA, y las funciones del codigo mapeado cuelgan de la
	 * pagina virtual.  Sin esto se sigue ejecutando codigo recompilado de
	 * un overlay que ya no esta.
	 *
	 * ★ `flushRegisters` Y NO `invalidateRegisters`, que es el error
	 * facil: el idiom de al lado (`check_memory_range`) usa
	 * `invalidateRegisters` porque `genCallDynaMem` YA ha hecho el flush
	 * antes.  Aqui no hay flush previo, e `invalidateRegisters` DESCARTA
	 * los registros sucios en vez de escribirlos.
	 *
	 * ★ `invalidate_func` de ESTE arbol lleva su propia guarda
	 * (`if(!invalid_code_get(addr>>12))`, Wrappers.c:596), asi que se
	 * puede llamar directa.  La de GliGli no la llevaba y por eso alli
	 * hubo que pasar por `check_invalidate_memory`; comprobado que aqui
	 * no hace falta -- `r4300.c` inicializa `invalid_code[i]=1` y
	 * `blocks[i]=NULL` a la vez, asi que la guarda garantiza el bloque. */
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);
	short immed = MIPS_GET_IMMED(mips);   /* not64 usa MIPS_GET_SIMMED, que
	                                       * este arbol no tiene; el `short`
	                                       * hace la extension de signo */

	switch(_rt){
	case 0:   /* Index Invalidate */
		/* [Xbox360] ★★ [Castlevania con esqueletos, Logs Salvia (3)] INDEX
		 * INVALIDATE NO LLEVA UNA DIRECCION DE CODIGO: LLEVA UN INDICE DE
		 * LINEA DE LA I-CACHE.  `osInvalICache` de libultra, con un tamano de
		 * 16 KB o mas, vacia la I-cache ENTERA recorriendola por indice desde
		 * K0BASE: 0x80000000, 0x80000020 ... 0x80003FE0.  Tratarlo como Hit
		 * Invalidate (lo que hacia not64, de donde se porto) liberaba la
		 * funcion que vive en ESA direccion -- el vector de excepciones y el
		 * codigo del sistema, que nadie ha tocado -- y se recompilaba en cada
		 * frame.  MEDIDO (sonda [CPU4]): con esqueletos ~1.400 funciones/s
		 * liberadas, todas por debajo de 0x80004000 (80000180, 8000053C,
		 * 80000694, 800008D0, 800023F8...), con la escritura en saltos de
		 * linea de cache; ~2.500 recompilaciones/s, 8-9 ms por VI, y el juego
		 * de 60 a 41-46 VI/s.
		 *
		 * La referencia no hace nada con CACHE: mupen64plus-core
		 * (mips_instructions.def, CACHE = ADD_TO_PC(1)) y Wii64 HEAD
		 * (`return CONVERT_ERROR`).  Aqui el codigo que cambia ya lo cubren
		 * los almacenes, la DMA y el puente fisica->virtual (ppc/Wrappers.c,
		 * `invalidate_func`), que no existia cuando se porto esto.  Se queda
		 * el Hit Invalidate, que si lleva la direccion VIRTUAL de lo que
		 * cambio (el overlay por TLB).
		 *
		 * [Bomberman 64 2nd Attack, Logs Salvia (10)] Descartado como causa de
		 * su cuelgue: con el Hit Invalidate de antes petaba igual (eran las
		 * variantes de TLB, ver var_hash en Wrappers.c). */
#if XBOX64_TRAZAS
		{
			static int dicho0 = 0;
			if(!dicho0){
				dicho0 = 1;
				sprintf(txtbuffer, "[DYNA] CACHE Index Invalidate (pc=%08X): "
				        "no se invalida nada (indice de I-cache, no direccion)",
				        get_src_pc());
				DEBUG_print(txtbuffer, DBG_USBGECKO);
			}
		}
#endif
		return CONVERT_SUCCESS;
	case 16:  /* Hit Invalidate   */
	{
		int rs;

		/* ★ UNA LINEA LA PRIMERA VEZ, y decide la teoria entera.
		 *
		 * Si esta linea NO sale, el juego no ejecuta CACHE y todo el
		 * razonamiento de arriba es irrelevante PARA ESE JUEGO -- sin
		 * gastar una compilacion en averiguarlo por otro lado.  Si sale,
		 * la via existe y antes se estaba tirando. */
#if XBOX64_TRAZAS
		{
			static int dicho = 0;
			if(!dicho){
				dicho = 1;
				sprintf(txtbuffer, "[DYNA] CACHE emitido (op=%d pc=%08X): "
				        "el juego invalida su I-cache", _rt, get_src_pc());
				DEBUG_print(txtbuffer, DBG_USBGECKO);
			}
		}
#endif

		rs = mapRegister(_rs);
		flushRegisters();

		/* R3 = rs + immed, que es la direccion virtual a invalidar. */
		if(immed || rs != R3){
			GEN_ADDI(R3, rs, immed);
		}

		GEN_B(add_jump((unsigned long)&invalidate_func, 1, 1), 0, 1);
		GEN_LWZ(R0, DYNAOFF_LR, R1);
		GEN_MTLR(R0);

		return CONVERT_SUCCESS;
	}
	default:
		/* Las demas operaciones de CACHE son de datos o de escritura
		 * diferida y no afectan al codigo recompilado.  Se dejan como
		 * estaban: sin emitir nada. */
		return CONVERT_ERROR;
	}
#endif
}

static int LL(MIPS_instr mips){
#ifdef INTERPRET_LL
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_LL
	genCallDynaMem(MEM_LL, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

static int SC(MIPS_instr mips){
#ifdef INTERPRET_SC
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SC
	genCallDynaMem(MEM_SC, 1, 0, MIPS_GET_RS(mips), MIPS_GET_RT(mips), MIPS_GET_IMMED(mips));
	return CONVERT_SUCCESS;
#endif
}

// -- Special Functions --

static int SLL(MIPS_instr mips){
	int _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);
	int sa = MIPS_GET_SA(mips);

	int rt = mapRegister(_rt);
	int rd = mapConstantNew(_rd, isRegisterConstant(_rt), 1);
	setRegisterConstant(_rd, (unsigned long)getRegisterConstant(_rt) << sa);

	GEN_SLWI(rd, rt, sa);
	return CONVERT_SUCCESS;
}

static int SRL(MIPS_instr mips){
	int _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);
	int sa = MIPS_GET_SA(mips);

	int rt = mapRegister(_rt);
	int rd = mapConstantNew(_rd, isRegisterConstant(_rt), 1);
	setRegisterConstant(_rd, (unsigned long)getRegisterConstant(_rt) >> sa);

	GEN_SRWI(rd, rt, sa);
	return CONVERT_SUCCESS;
}

static int SRA(MIPS_instr mips){
	int _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);
	int sa = MIPS_GET_SA(mips);

	int rt = mapRegister(_rt);
	int rd = mapConstantNew(_rd, isRegisterConstant(_rt), 1);
	setRegisterConstant(_rd, getRegisterConstant(_rt) >> sa);

	GEN_SRAWI(rd, rt, sa);
	return CONVERT_SUCCESS;
}

static int SLLV(MIPS_instr mips){
	int _rt = MIPS_GET_RT(mips), _rs = MIPS_GET_RS(mips), _rd = MIPS_GET_RD(mips);

	int rt = mapRegister(_rt);
	int rs = mapRegister(_rs);
	int rd = mapConstantNew(_rd, isRegisterConstant(_rt) && isRegisterConstant(_rs), 1);
	setRegisterConstant(_rd, (unsigned long)getRegisterConstant(_rt) << (getRegisterConstant(_rs) & 0x1F));

	GEN_CLRLWI(R0, rs, 27); // Mask the lower 5-bits of rs
	GEN_SLW(rd, rt, R0);
	return CONVERT_SUCCESS;
}

static int SRLV(MIPS_instr mips){
	int _rt = MIPS_GET_RT(mips), _rs = MIPS_GET_RS(mips), _rd = MIPS_GET_RD(mips);

	int rt = mapRegister(_rt);
	int rs = mapRegister(_rs);
	int rd = mapConstantNew(_rd, isRegisterConstant(_rt) && isRegisterConstant(_rs), 1);
	setRegisterConstant(_rd, (unsigned long)getRegisterConstant(_rt) >> (getRegisterConstant(_rs) & 0x1F));

	GEN_CLRLWI(R0, rs, 27); // Mask the lower 5-bits of rs
	GEN_SRW(rd, rt, R0);
	return CONVERT_SUCCESS;
}

static int SRAV(MIPS_instr mips){
	int _rt = MIPS_GET_RT(mips), _rs = MIPS_GET_RS(mips), _rd = MIPS_GET_RD(mips);

	int rt = mapRegister(_rt);
	int rs = mapRegister(_rs);
	int rd = mapConstantNew(_rd, isRegisterConstant(_rt) && isRegisterConstant(_rs), 1);
	setRegisterConstant(_rd, getRegisterConstant(_rt) >> (getRegisterConstant(_rs) & 0x1F));

	GEN_CLRLWI(R0, rs, 27); // Mask the lower 5-bits of rs
	GEN_SRAW(rd, rt, R0);
	return CONVERT_SUCCESS;
}

static int JR(MIPS_instr mips){
	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}
	
	flushRegisters();
	reset_code_addr();
	
	GEN_STW(mapRegister(MIPS_GET_RS(mips)), offsetof(R4300,local_gpr)+4, DYNAREG_R4300);

	// Check the delay slot, and note how big it is
	PowerPC_instr* preDelay = get_curr_dst();
	check_delaySlot();
	int delaySlot = get_curr_dst() - preDelay;

#ifdef COMPARE_CORE
	GEN_LI(R4, 1);
#endif
	/* [Xbox360] Con la comprobacion de la interrupcion en cr3, como los saltos
	 * enlazados: la via rapida no entra si hay una pendiente. */
	genUpdateCount(1);
	invalidateRegisters();

	/* [Xbox360] Wii64 dejaba aqui `#ifdef INTERPRET_JR` con la version
	 * compilada en `// TODO: jr`: el salto volvia siempre al despachador.
	 * Ahora, la tabla de saltos (genJumpToRegRapido, Wrappers.h). */
	genJumpToRegRapido();

	// Let's still recompile the delay slot in place in case its branched to
	if(delaySlot){ if(is_j_dst(0)){ unget_last_src(); delaySlotNext = 2; } }
	else nop_ignored();

	return INTERPRETED;
}

static int JALR(MIPS_instr mips){
	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	flushRegisters();
	reset_code_addr();
	
	GEN_STW(mapRegister(MIPS_GET_RS(mips)), offsetof(R4300,local_gpr)+4, DYNAREG_R4300);

	// Check the delay slot, and note how big it is
	PowerPC_instr* preDelay = get_curr_dst();
	check_delaySlot();
	int delaySlot = get_curr_dst() - preDelay;

#ifdef COMPARE_CORE
	GEN_LI(R4, 1);
#endif
	genUpdateCount(1);   /* [Xbox360] cr3 para la via rapida: ver JR */

	// Set LR to next instruction
	int rd = mapRegisterNew(MIPS_GET_RD(mips));
	// lis	lr, pc@ha(0)
	GEN_LIS(rd, extractUpper16(get_src_pc()+4));
	// la	lr, pc@l(lr)
	GEN_ADDI(rd, rd, extractLower16(get_src_pc()+4));

	flushRegisters();

	/* [Xbox360] La via rapida usa r3-r7 a pelo: que el reparto de registros
	 * no crea que `rd` sigue en uno de ellos (ya esta escrito: flush).  El
	 * `// TODO: jalr` de Wii64 se fue con INTERPRET_JALR: ver JR. */
	invalidateRegisters();
	genJumpToRegRapido();

	// Let's still recompile the delay slot in place in case its branched to
	if(delaySlot){ if(is_j_dst(0)){ unget_last_src(); delaySlotNext = 2; } }
	else nop_ignored();

	return INTERPRETED;
}

static int SYSCALL(MIPS_instr mips){
#ifdef INTERPRET_SYSCALL
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SYSCALL
	return CONVERT_ERROR;
#endif
}

static int BREAK(MIPS_instr mips){
#ifdef INTERPRET_BREAK
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_BREAK
	return CONVERT_ERROR;
#endif
}

static int SYNC(MIPS_instr mips){
#ifdef INTERPRET_SYNC
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_SYNC
	return CONVERT_SUCCESS;
#endif
}

static int MFHI(MIPS_instr mips){
#ifdef INTERPRET_HILO
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_HILO

	int _rd = MIPS_GET_RD(mips);

	RegMapping hi = mapRegister64(MIPS_REG_HI);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(MIPS_REG_HI));
	setRegisterConstant64(_rd, getRegisterConstant64(MIPS_REG_HI));

	// mr rd, hi
	GEN_MR(rd.lo, hi.lo);
	GEN_MR(rd.hi, hi.hi);
	return CONVERT_SUCCESS;
#endif
}

static int MTHI(MIPS_instr mips){
#ifdef INTERPRET_HILO
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_HILO

	int _rs = MIPS_GET_RS(mips);

	RegMapping rs = mapRegister64(_rs);
	RegMapping hi = mapConstant64New(MIPS_REG_HI, isRegisterConstant(_rs));
	setRegisterConstant64(MIPS_REG_HI, getRegisterConstant64(_rs));

	// mr hi, rs
	GEN_MR(hi.lo, rs.lo);
	GEN_MR(hi.hi, rs.hi);
	return CONVERT_SUCCESS;
#endif
}

static int MFLO(MIPS_instr mips){
#ifdef INTERPRET_HILO
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_HILO

	int _rd = MIPS_GET_RD(mips);

	RegMapping lo = mapRegister64(MIPS_REG_LO);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(MIPS_REG_LO));
	setRegisterConstant64(_rd, getRegisterConstant64(MIPS_REG_LO));

	// mr rd, lo
	GEN_MR(rd.lo, lo.lo);
	GEN_MR(rd.hi, lo.hi);
	return CONVERT_SUCCESS;
#endif
}

static int MTLO(MIPS_instr mips){
#ifdef INTERPRET_HILO
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_HILO

	int _rs = MIPS_GET_RS(mips);

	RegMapping rs = mapRegister64(_rs);
	RegMapping lo = mapConstant64New(MIPS_REG_LO, isRegisterConstant(_rs));
	setRegisterConstant64(MIPS_REG_LO, getRegisterConstant64(_rs));

	// mr lo, rs
	GEN_MR(lo.lo, rs.lo);
	GEN_MR(lo.hi, rs.hi);
	return CONVERT_SUCCESS;
#endif
}

static int MULT(MIPS_instr mips){
#ifdef INTERPRET_MULT
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_MULT
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);

	int rs = mapRegister(_rs);
	int rt = mapRegister(_rt);
	int lo = mapConstantNew(MIPS_REG_LO, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	int hi = mapConstantNew(MIPS_REG_HI, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	setRegisterConstant(MIPS_REG_LO, ((long long)getRegisterConstant(_rs) * getRegisterConstant(_rt)));
	setRegisterConstant(MIPS_REG_HI, ((long long)getRegisterConstant(_rs) * getRegisterConstant(_rt)) >> 32);

	// Don't multiply if they're using r0
	if(_rs && _rt){
		// mullw lo, rs, rt
		GEN_MULLW(lo, rs, rt);
		// mulhw hi, rs, rt
		GEN_MULHW(hi, rs, rt);
	} else {
		// li lo, 0
		GEN_LI(lo, 0);
		// li hi, 0
		GEN_LI(hi, 0);
	}
	return CONVERT_SUCCESS;
#endif
}

static int MULTU(MIPS_instr mips){
#ifdef INTERPRET_MULTU
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_MULTU
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);

	int rs = mapRegister(_rs);
	int rt = mapRegister(_rt);
	int lo = mapConstantNew(MIPS_REG_LO, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	int hi = mapConstantNew(MIPS_REG_HI, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	setRegisterConstant(MIPS_REG_LO, ((unsigned long long)getRegisterConstant(_rs) * getRegisterConstant(_rt)));
	setRegisterConstant(MIPS_REG_HI, ((unsigned long long)getRegisterConstant(_rs) * getRegisterConstant(_rt)) >> 32);

	// Don't multiply if they're using r0
	if(_rs && _rt){
		// mullw lo, rs, rt
		GEN_MULLW(lo, rs, rt);
		// mulhwu hi, rs, rt
		GEN_MULHWU( hi, rs, rt);
	} else {
		// li lo, 0
		GEN_LI(lo, 0);
		// li hi, 0
		GEN_LI(hi, 0);
	}
	return CONVERT_SUCCESS;
#endif
}

static int DIV(MIPS_instr mips){
#ifdef INTERPRET_DIV
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DIV
	// This instruction computes the quotient and remainder
	//   and stores the results in lo and hi respectively
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);

	/* [Xbox360] ★★ [Mischief Makers, bordes grises] LOS OPERANDOS `$zero`.
	 * Wii64 no emitia NADA cuando uno de los dos era `$zero` ("Don't divide if
	 * they're using r0", abajo), pero `lo` y `hi` ya estaban mapeados como
	 * NUEVOS: LO/HI se quedaban con lo que tuvieran esos registros PPC, basura
	 * distinta en cada arranque.  MEDIDO (biseccion, corte 4 de Interpreter.h,
	 * y [DIVCENSO]): el juego tiene 15 `DIV $zero, $s0` (instr 0010001A,
	 * 80021698..80021FA8) y con ellas calcula el color de relleno de los
	 * cuatro bordes; con el interprete sale negro, como en GLideN64.
	 *
	 * Lo mismo que el interprete (pure_interp.c:265), sin pasar por el:
	 *   - divisor `$zero`: el interprete deja LO/HI COMO ESTABAN.  Aqui, salir
	 *     ANTES de mapearlos: ni codigo ni registros nuevos, y su estado de
	 *     constante (si lo tenian) sigue valiendo.
	 *   - dividendo `$zero`: 0/rt -> LO = 0, HI = 0; el `else` de abajo emite
	 *     los dos `li 0`, como MULT.  (Con rt = 0 en ejecucion el R4300 no
	 *     define el resultado, igual que el `divw` del caso general.) */
	if(_rt == 0)
		return CONVERT_SUCCESS;

	int rs = mapRegister(_rs);
	int rt = mapRegister(_rt);
	int lo = mapConstantNew(MIPS_REG_LO, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	int hi = mapConstantNew(MIPS_REG_HI, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	/* [Xbox360] ★★ MISMO CODIGO, DISTINTO COMPILADOR: MSVC ATRAPA LA
	 * DIVISION ENTERA POR CERO Y GCC EN PowerPC NO.
	 *
	 * Este plegado de constantes se ejecuta SIEMPRE, no solo cuando los dos
	 * registros son constantes: la condicion `isRegisterConstant(...) &&
	 * ...` solo se le pasa a `mapConstantNew` (dos lineas arriba) para
	 * decidir si el resultado SE USA.  Cuando no lo son,
	 * `getRegisterConstant` devuelve 0 y esto divide por cero.
	 *
	 * En Wii eso es inofensivo: `divw` de PowerPC NO lanza excepcion, deja
	 * un resultado indefinido que ademas nadie lee.  MSVC, en cambio, emite
	 * una comprobacion explicita para darle a ese comportamiento indefinido
	 * de C una excepcion de Windows:
	 *
	 *     0xC0000094: Integer division by zero
	 *     DIV() line 2389  <-  convert()  <-  recompile_block()
	 *
	 * ★ Y OJO CON DONDE PASA: en el RECOMPILADOR, no en el codigo
	 * generado.  La pila lo dice, pero es facil leer "division por cero" y
	 * buscarla en el juego.
	 *
	 * ★ ARREGLO MINIMO: no se toca el caso con divisor distinto de cero --
	 * ahi el resultado es identico al de Wii64.  Solo se sustituye la
	 * basura indefinida por un 0 cuando el divisor es cero, que ademas es
	 * lo honesto: DIV por cero en MIPS es UNPREDICTABLE, asi que ese valor
	 * no significa nada en ninguna de las dos maquinas.
	 *
	 * ★★ Y HAY UN SEGUNDO DESBORDAMIENTO QUE LA GUARDA DEL CERO NO CUBRE,
	 * PORQUE NO ES UNA DIVISION POR CERO.  Killer Instinct Gold lo tira:
	 *
	 *     0xC0000095: Integer overflow
	 *     DIV() line 2589  <-  convert()  <-  recompile_block()
	 *
	 * Es `0x80000000 / -1`.  El cociente seria 2^31, que no cabe en `long`
	 * con signo, asi que MSVC lanza igual que con el cero.  Y el `%` de la
	 * linea siguiente desborda por lo mismo.
	 *
	 * ★ EL VALOR QUE SE PONE NO ES UNA ELECCION, ES EL QUE TIENEN QUE DAR
	 * LAS DOS MAQUINAS -- si no, el plegado de constantes y el codigo
	 * generado dirian cosas distintas:
	 *
	 *   R4300     DIV de -2^31 entre -1 deja LO = -2^31 y HI = 0.
	 *   PowerPC   `divw` de 0x80000000 entre -1 devuelve 0x80000000 (y
	 *             activa OV, que aqui nadie mira).  Y el HI que emite el
	 *             codigo de abajo es `rs - lo*rt`: en 32 bits
	 *             0x80000000 * -1 vuelve a ser 0x80000000, asi que
	 *             0x80000000 - 0x80000000 = 0.
	 *
	 * O sea LO = el dividendo, HI = 0, que es justo lo que se escribe. */
	{
		long dividendo = getRegisterConstant(_rs);
		long divisor   = getRegisterConstant(_rt);

		if (divisor == 0)
		{
			setRegisterConstant(MIPS_REG_LO, 0);
			setRegisterConstant(MIPS_REG_HI, 0);
		}
		else if (divisor == -1 && dividendo == (long)0x80000000)
		{
			setRegisterConstant(MIPS_REG_LO, dividendo);
			setRegisterConstant(MIPS_REG_HI, 0);
		}
		else
		{
			setRegisterConstant(MIPS_REG_LO, dividendo / divisor);
			setRegisterConstant(MIPS_REG_HI, dividendo % divisor);
		}
	}

	// Don't divide if they're using r0
	if(_rs && _rt){
		// divw lo, rs, rt
		GEN_DIVW(lo, rs, rt);
		// This is how you perform a mod in PPC
		// divw lo, rs, rt
		// NOTE: We already did that
		// mullw hi, lo, rt
		GEN_MULLW(hi, lo, rt);
		// subf hi, hi, rs
		GEN_SUBF(hi, hi, rs);
	} else {
		/* [Xbox360] Dividendo `$zero` (el divisor `$zero` ya salio arriba):
		 * 0/rt.  Ver la nota de arriba. */
		// li lo, 0
		GEN_LI(lo, 0);
		// li hi, 0
		GEN_LI(hi, 0);
	}

	return CONVERT_SUCCESS;
#endif
}

static int DIVU(MIPS_instr mips){
#ifdef INTERPRET_DIVU
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DIVU
	// This instruction computes the quotient and remainder
	//   and stores the results in lo and hi respectively
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips);

	/* [Xbox360] ★★ Los operandos `$zero`, el mismo hueco que en DIV (la nota
	 * larga esta alli): divisor `$zero` -> LO/HI como estaban. */
	if(_rt == 0)
		return CONVERT_SUCCESS;

	int rs = mapRegister(_rs);
	int rt = mapRegister(_rt);
	int lo = mapConstantNew(MIPS_REG_LO, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	int hi = mapConstantNew(MIPS_REG_HI, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	/* [Xbox360] Mismo caso que en DIV: el plegado corre siempre y MSVC
	 * atrapa la division por cero.  La nota larga esta en DIV.
	 * ★ El divisor se toma como `unsigned long` para reproducir EXACTAMENTE
	 * la conversion de la expresion original, donde el cast del dividendo
	 * arrastraba la operacion entera a sin signo. */
	{
		unsigned long divisor = (unsigned long)getRegisterConstant(_rt);
		setRegisterConstant(MIPS_REG_LO, divisor ? (unsigned long)getRegisterConstant(_rs) / divisor : 0);
		setRegisterConstant(MIPS_REG_HI, divisor ? (unsigned long)getRegisterConstant(_rs) % divisor : 0);
	}

	// Don't divide if they're using r0
	if(MIPS_GET_RS(mips) && MIPS_GET_RT(mips)){
		// divwu lo, rs, rt
		GEN_DIVWU(lo, rs, rt);
		// This is how you perform a mod in PPC
		// divw lo, rs, rt
		// NOTE: We already did that
		// mullw hi, lo, rt
		GEN_MULLW(hi, lo, rt);
		// subf hi, hi, rs
		GEN_SUBF(hi, hi, rs);
	} else {
		/* [Xbox360] Dividendo `$zero`: 0/rt.  Ver la nota de DIV. */
		// li lo, 0
		GEN_LI(lo, 0);
		// li hi, 0
		GEN_LI(hi, 0);
	}
	return CONVERT_SUCCESS;
#endif
}

static int DSLLV(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSLLV)
	genCallInterp(mips);
	return INTERPRETED;
#else  // INTERPRET_DW || INTERPRET_DSLLV

	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	int rs = mapRegister(_rs);
	int sa = mapRegisterTemp();
	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rt) && isRegisterConstant(_rs));
	setRegisterConstant64(_rd, (unsigned long long)getRegisterConstant64(_rt) << (getRegisterConstant(_rs) & 0x3F));

	GEN_CLRLWI(sa, rs, 26);
	GEN_SUBIC_(R0, sa, 32);
	GEN_BLT(CR0, 4, 0, 0);
	GEN_SLW(rd.hi, rt.lo, R0);
	GEN_LI(rd.lo, 0);
	GEN_B(6, 0, 0);
	GEN_SUBFIC(R0, sa, 32);
	GEN_SLW(rd.hi, rt.hi, sa);
	GEN_SRW(R0, rt.lo, R0);
	GEN_SLW(rd.lo, rt.lo, sa);
	GEN_OR(rd.hi, rd.hi, R0);

	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int DSRLV(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSRLV)
	genCallInterp(mips);
	return INTERPRETED;
#else  // INTERPRET_DW || INTERPRET_DSRLV

	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	int rs = mapRegister(_rs);
	int sa = mapRegisterTemp();
	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rt) && isRegisterConstant(_rs));
	setRegisterConstant64(_rd, (unsigned long long)getRegisterConstant64(_rt) >> (getRegisterConstant(_rs) & 0x3F));

	GEN_CLRLWI(sa, rs, 26);
	GEN_SUBIC_(R0, sa, 32);
	GEN_BLT(CR0, 4, 0, 0);
	GEN_SRW(rd.lo, rt.hi, R0);
	GEN_LI(rd.hi, 0);
	GEN_B(6, 0, 0);
	GEN_SUBFIC(R0, sa, 32);
	GEN_SRW(rd.lo, rt.lo, sa);
	GEN_SLW(R0, rt.hi, R0);
	GEN_SRW(rd.hi, rt.hi, sa);
	GEN_OR(rd.lo, rd.lo, R0);
	
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int DSRAV(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSRAV)
	genCallInterp(mips);
	return INTERPRETED;
#else  // INTERPRET_DW || INTERPRET_DSRAV

	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	int rs = mapRegister(_rs);
	int sa = mapRegisterTemp();
	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rt) && isRegisterConstant(_rs));
	setRegisterConstant64(_rd, getRegisterConstant64(_rt) >> (getRegisterConstant(_rs) & 0x3F));

	GEN_CLRLWI(sa, rs, 26);
	GEN_SUBIC_(R0, sa, 32);
	GEN_BLT(CR0, 4, 0, 0);
	GEN_SRAW(rd.lo, rt.hi, R0);
	GEN_SRAWI(rd.hi, rt.hi, 31);
	GEN_B(6, 0, 0);
	GEN_SUBFIC(R0, sa, 32);
	GEN_SRW(rd.lo, rt.lo, sa);
	GEN_SLW(R0, rt.hi, R0);
	GEN_SRAW(rd.hi, rt.hi, sa);
	GEN_OR(rd.lo, rd.lo, R0);
	
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int DMULT(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DMULT)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DMULT
	return CONVERT_ERROR;
#endif
}

static int DMULTU(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DMULTU)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DMULTU
	return CONVERT_ERROR;
#endif
}

static int DDIV(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DDIV)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DDIV
	/* [Xbox360] ★★ [AUTO, 2026-10-12] LOS OPERANDOS `$zero`, como en DIV (la
	 * nota larga esta alli).  Aqui era peor: el codigo de abajo cuenta con
	 * que, tras `flushRegisters`, el dividendo caiga en r3:r4 y el divisor en
	 * r5:r6, que es donde los espera `__udivmoddi4`; pero `mapRegister64(0)`
	 * devuelve DYNAREG_ZERO y no ocupa pareja, asi que el otro operando se
	 * corria a r3:r4 y la division salia con los argumentos cambiados.
	 * MEDIDO (autoprueba, DDIV.rs0/.rt0/.rs0rt0: 62 de 62 casos): 0/b daba
	 * HI = b, y a/$zero tocaba LO/HI.  Lo mismo que el interprete:
	 *   - divisor `$zero`: LO/HI COMO ESTABAN -> no se emite nada;
	 *   - dividendo `$zero`: 0/rt -> LO = HI = 0. */
	if(MIPS_GET_RT(mips) == 0)
		return CONVERT_SUCCESS;
	if(MIPS_GET_RS(mips) == 0){
		RegMapping lo0 = mapRegister64New( MIPS_REG_LO );
		RegMapping hi0 = mapRegister64New( MIPS_REG_HI );
		GEN_LI(lo0.lo, 0);
		GEN_LI(lo0.hi, 0);
		GEN_LI(hi0.lo, 0);
		GEN_LI(hi0.hi, 0);
		return CONVERT_SUCCESS;
	}

	flushRegisters();
	RegMapping rs = mapRegister64( MIPS_GET_RS(mips) );
	RegMapping rt = mapRegister64( MIPS_GET_RT(mips) );
	invalidateRegisters();

	GEN_CMPI(CR2, rs.hi, 0);
	GEN_CMPI(CR1, rt.hi, 0);
	GEN_BGE(CR2, 3, 0, 0);
	GEN_SUBFIC(rs.lo, rs.lo, 0); 
	GEN_SUBFZE(rs.hi, rs.hi);
	GEN_BGE(CR1, 3, 0, 0);
	GEN_SUBFIC(rt.lo, rt.lo, 0);
	GEN_SUBFZE(rt.hi, rt.hi);
	GEN_CRXOR(CR2*4+2, CR1*4+0, CR2*4+0);

	// r7 = &reg[hi]
	GEN_ADDI(R7, DYNAREG_R4300, offsetof(R4300,hi));

	// divide
	GEN_B(add_jump((unsigned long)&__udivmoddi4, 1, 1), 0, 1);

	RegMapping lo = mapRegister64New( MIPS_REG_LO );
	/* [Xbox360] [AUTO, log 46] `mapRegister64` sola carga el resto que
	 * dejo __udivmoddi4 pero lo mapea LIMPIO: la negacion de abajo se hacia
	 * en el registro y nunca llegaba a r4300.hi.  DDIV -1/7FFFFFFF daba
	 * HI=+1 (el interprete -1, que es lo que manda C y el R4300).  Volver a
	 * pedirlo con `mapRegister64New` devuelve el MISMO par (ya esta mapeado,
	 * Register-Cache.c:143) y lo marca sucio. */
	mapRegister64( MIPS_REG_HI );
	RegMapping hi = mapRegister64New( MIPS_REG_HI );

	GEN_BNE(CR2, 3, 0, 0);
	GEN_SUBFIC(lo.lo, lo.lo, 0);
	GEN_SUBFZE(lo.hi, lo.hi);
	GEN_BGE(CR2, 3, 0, 0);
	GEN_SUBFIC(hi.lo, hi.lo, 0);
	GEN_SUBFZE(hi.hi, hi.hi);

	// Load old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// Restore LR
	GEN_MTLR(R0);

	return CONVERT_SUCCESS;
#endif
}

static int DDIVU(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DDIVU)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DDIVU
	/* [Xbox360] ★★ Los operandos `$zero`: el mismo hueco que en DDIV (la nota
	 * esta alli; DDIVU.rs0/.rt0/.rs0rt0 fallaban 62 de 62 en la autoprueba). */
	if(MIPS_GET_RT(mips) == 0)
		return CONVERT_SUCCESS;
	if(MIPS_GET_RS(mips) == 0){
		RegMapping lo0 = mapRegister64New( MIPS_REG_LO );
		RegMapping hi0 = mapRegister64New( MIPS_REG_HI );
		GEN_LI(lo0.lo, 0);
		GEN_LI(lo0.hi, 0);
		GEN_LI(hi0.lo, 0);
		GEN_LI(hi0.hi, 0);
		return CONVERT_SUCCESS;
	}

	flushRegisters();
	mapRegister64( MIPS_GET_RS(mips) );
	mapRegister64( MIPS_GET_RT(mips) );
	invalidateRegisters();

	// r7 = &reg[hi]
	GEN_ADDI(R7, DYNAREG_R4300, offsetof(R4300,hi));

	// divide
	GEN_B(add_jump((unsigned long)&__udivmoddi4, 1, 1), 0, 1);

	mapRegister64New( MIPS_REG_LO );

	// Load old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// Restore LR
	GEN_MTLR(R0);

	return CONVERT_SUCCESS;
#endif
}

static int DADDU(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DADDU)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DADDU

	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt));
	setRegisterConstant64(_rd, getRegisterConstant64(_rs) + getRegisterConstant64(_rt));

	GEN_ADDC(rd.lo, rs.lo, rt.lo);
	GEN_ADDE(rd.hi, rs.hi, rt.hi);
	return CONVERT_SUCCESS;
#endif
}

static int DADD(MIPS_instr mips){
	return DADDU(mips);
}

static int DSUBU(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSUBU)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DSUBU

	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt));
	setRegisterConstant64(_rd, getRegisterConstant64(_rs) - getRegisterConstant64(_rt));

	GEN_SUBFC(rd.lo, rt.lo, rs.lo);
	GEN_SUBFE(rd.hi, rt.hi, rs.hi);
	return CONVERT_SUCCESS;
#endif
}

static int DSUB(MIPS_instr mips){
	return DSUBU(mips);
}

static int DSLL(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSLL)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DSLL

	int _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);
	int sa = MIPS_GET_SA(mips);

	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rt));
	setRegisterConstant64(_rd, (unsigned long long)getRegisterConstant64(_rt) << sa);

	if(sa){
		// Shift MSW left by SA
		GEN_SLWI(rd.hi, rt.hi, sa);
		// Extract the bits shifted out of the LSW
		// Insert those bits into the MSW
		GEN_RLWIMI(rd.hi, rt.lo, sa, 32-sa, 31);
		// Shift LSW left by SA
		GEN_SLWI(rd.lo, rt.lo, sa);
	} else {
		// Copy over the register
		GEN_MR(rd.hi, rt.hi);
		GEN_MR(rd.lo, rt.lo);
	}
	return CONVERT_SUCCESS;
#endif
}

static int DSRL(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSRL)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DSRL

	int _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);
	int sa = MIPS_GET_SA(mips);

	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rt));
	setRegisterConstant64(_rd, (unsigned long long)getRegisterConstant64(_rt) >> sa);

	if(sa){
		// Shift LSW right by SA
		GEN_SRWI(rd.lo, rt.lo, sa);
		// Extract the bits shifted out of the MSW
		// Insert those bits into the LSW
		GEN_RLWIMI(rd.lo, rt.hi, 32-sa, 0, sa-1);
		// Shift MSW right by SA
		GEN_SRWI(rd.hi, rt.hi, sa);
	} else {
		// Copy over the register
		GEN_MR(rd.hi, rt.hi);
		GEN_MR(rd.lo, rt.lo);
	}
	return CONVERT_SUCCESS;
#endif
}

static int DSRA(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSRA)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DSRA

	int _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);
	int sa = MIPS_GET_SA(mips);

	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rt));
	setRegisterConstant64(_rd, getRegisterConstant64(_rt) << sa);

	if(sa){
		// Shift LSW right by SA
		GEN_SRWI(rd.lo, rt.lo, sa);
		// Extract the bits shifted out of the MSW
		// Insert those bits into the LSW
		GEN_RLWIMI(rd.lo, rt.hi, 32-sa, 0, sa-1);
		// Shift (arithmetically) MSW right by SA
		GEN_SRAWI(rd.hi, rt.hi, sa);
	} else {
		// Copy over the register
		GEN_MR(rd.hi, rt.hi);
		GEN_MR(rd.lo, rt.lo);
	}
	return CONVERT_SUCCESS;
#endif
}

static int DSLL32(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSLL32)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DSLL32

	RegMapping rt = mapRegister64( MIPS_GET_RT(mips) );
	RegMapping rd = mapRegister64New( MIPS_GET_RD(mips) );
	int sa = MIPS_GET_SA(mips);

	// Shift LSW into MSW and by SA
	GEN_SLWI(rd.hi, rt.lo, sa);
	// Clear out LSW
	GEN_LI(rd.lo, 0);
	return CONVERT_SUCCESS;
#endif
}

static int DSRL32(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSRL32)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DSRL32

	RegMapping rt = mapRegister64( MIPS_GET_RT(mips) );
	RegMapping rd = mapRegister64New( MIPS_GET_RD(mips) );
	int sa = MIPS_GET_SA(mips);

	// Shift MSW into LSW and by SA
	GEN_SRWI(rd.lo, rt.hi, sa);
	// Clear out MSW
	GEN_LI(rd.hi, 0);
	return CONVERT_SUCCESS;
#endif
}

static int DSRA32(MIPS_instr mips){
#if defined(INTERPRET_DW) || defined(INTERPRET_DSRA32)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_DW || INTERPRET_DSRA32

	RegMapping rt = mapRegister64( MIPS_GET_RT(mips) );
	RegMapping rd = mapRegister64New( MIPS_GET_RD(mips) );
	int sa = MIPS_GET_SA(mips);

	// Shift (arithmetically) MSW into LSW and by SA
	GEN_SRAWI(rd.lo, rt.hi, sa);
	// Fill MSW with sign of MSW
	GEN_SRAWI(rd.hi, rt.hi, 31);
	return CONVERT_SUCCESS;
#endif
}

static int ADDU(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	int rs = mapRegister(_rs);
	int rt = mapRegister(_rt);
	int rd = mapConstantNew(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	setRegisterConstant(_rd, getRegisterConstant(_rs) + getRegisterConstant(_rt));

	GEN_ADD(rd, rs, rt);
	return CONVERT_SUCCESS;
}

static int ADD(MIPS_instr mips){
	return ADDU(mips);
}

static int SUBU(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	int rs = mapRegister(_rs);
	int rt = mapRegister(_rt);
	int rd = mapConstantNew(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt), 1);
	setRegisterConstant(_rd, getRegisterConstant(_rs) - getRegisterConstant(_rt));

	GEN_SUB(rd, rs, rt);
	return CONVERT_SUCCESS;
}

static int SUB(MIPS_instr mips){
	return SUBU(mips);
}

static int AND(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt));
	setRegisterConstant64(_rd, getRegisterConstant64(_rs) & getRegisterConstant64(_rt));

	GEN_AND(rd.hi, rs.hi, rt.hi);
	GEN_AND(rd.lo, rs.lo, rt.lo);
	return CONVERT_SUCCESS;
}

static int OR(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt));
	setRegisterConstant64(_rd, getRegisterConstant64(_rs) | getRegisterConstant64(_rt));

	GEN_OR(rd.hi, rs.hi, rt.hi);
	GEN_OR(rd.lo, rs.lo, rt.lo);
	return CONVERT_SUCCESS;
}

static int XOR(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt));
	setRegisterConstant64(_rd, getRegisterConstant64(_rs) ^ getRegisterConstant64(_rt));

	GEN_XOR(rd.hi, rs.hi, rt.hi);
	GEN_XOR(rd.lo, rs.lo, rt.lo);
	return CONVERT_SUCCESS;
}

static int NOR(MIPS_instr mips){
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapRegister64(_rt);
	RegMapping rd = mapConstant64New(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt));
	setRegisterConstant64(_rd, ~(getRegisterConstant64(_rs) | getRegisterConstant64(_rt)));

	GEN_NOR(rd.hi, rs.hi, rt.hi);
	GEN_NOR(rd.lo, rs.lo, rt.lo);
	return CONVERT_SUCCESS;
}

static int SLT(MIPS_instr mips){
#ifdef INTERPRET_SLT
	genCallInterp(mips);
	return INTERPRETED;
#else
	/* [Xbox360] [AUTO, log 46] ★ A 64 BITS, como el interprete
	 * (pure_interp.c: `rrs < rrt` en long long) y el R4300.  El original
	 * (con su "FIXME: Do I need to worry about 64-bit values?") comparaba
	 * solo la mitad baja: 0 < 0x00000000_80000000 daba 0, y 0 <
	 * 0x12345678_9ABCDEF0 tambien.  186 de 441 casos.  Las mitades altas
	 * con signo; si son iguales manda la baja SIN signo.  El orden de los
	 * mapeos importa: si rd == rs, `mapConstantNew` suelta la mitad alta de
	 * rs, y despues ya no se pide ningun registro. */
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	/* [Xbox360] [log 58] El corto de Wii64 donde es exacto: ver genCmp64. */
	if(isRegisterSext32(_rs) && isRegisterSext32(_rt)){
		int sa = mapRegisterTemp();
		int rs = mapRegister(_rs);
		int rt = mapRegister(_rt);
		int rd = mapConstantNew(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt), 0);
		setRegisterConstant(_rd, getRegisterConstant(_rs) < getRegisterConstant(_rt));
		GEN_CMP(CR0, rs, rt);
		GEN_MFCR(sa);
		GEN_RLWINM(rd, sa, 1, 31, 31);
		unmapRegisterTemp(sa);
		return CONVERT_SUCCESS;
	}

	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapRegister64(_rt);
	int sa = mapRegisterTemp();
	int rd = mapConstantNew(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt), 0);
	setRegisterConstant(_rd, getRegisterConstant64(_rs) < getRegisterConstant64(_rt));

	GEN_CMP(CR0, rs.hi, rt.hi);
	GEN_BNE(CR0, 2, 0, 0);
	GEN_CMPL(CR0, rs.lo, rt.lo);
	GEN_MFCR(sa);
	GEN_RLWINM(rd, sa, 1, 31, 31);

	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int SLTU(MIPS_instr mips){
#ifdef INTERPRET_SLTU
	genCallInterp(mips);
	return INTERPRETED;
#else
	/* [Xbox360] [AUTO, log 46] ★ A 64 BITS: ver SLT.  Las dos mitades sin
	 * signo.  El original (truco del acarreo sobre la mitad baja) daba 0
	 * para 0 < 0x80000000_00000000.  116 de 441 casos. */
	int _rs = MIPS_GET_RS(mips), _rt = MIPS_GET_RT(mips), _rd = MIPS_GET_RD(mips);

	/* [Xbox360] [log 58] El corto de Wii64 donde es exacto: ver genCmp64.
	 * Con los dos extendidos en signo, el orden sin signo de 64 bits y el
	 * de 32 coinciden. */
	if(isRegisterSext32(_rs) && isRegisterSext32(_rt)){
		int rs = mapRegister(_rs);
		int rt = mapRegister(_rt);
		int rd = mapConstantNew(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt), 0);
		setRegisterConstant(_rd, (unsigned long)getRegisterConstant(_rs) <
		                         (unsigned long)getRegisterConstant(_rt));
		GEN_NOT(R0, rs);
		GEN_ADDC(R0, R0, rt);
		GEN_ADDZE(rd, DYNAREG_ZERO);
		return CONVERT_SUCCESS;
	}

	RegMapping rs = mapRegister64(_rs);
	RegMapping rt = mapRegister64(_rt);
	int sa = mapRegisterTemp();
	int rd = mapConstantNew(_rd, isRegisterConstant(_rs) && isRegisterConstant(_rt), 0);
	setRegisterConstant(_rd, (unsigned long long)getRegisterConstant64(_rs) <
	                         (unsigned long long)getRegisterConstant64(_rt));

	GEN_CMPL(CR0, rs.hi, rt.hi);
	GEN_BNE(CR0, 2, 0, 0);
	GEN_CMPL(CR0, rs.lo, rt.lo);
	GEN_MFCR(sa);
	GEN_RLWINM(rd, sa, 1, 31, 31);

	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int TEQ(MIPS_instr mips){
#ifdef INTERPRET_TRAPS
	genCallInterp(mips);
	return INTERPRETED;
#else
	return CONVERT_ERROR;
#endif
}

static int (*gen_special[64])(MIPS_instr) =
{
   SLL , NI   , SRL , SRA , SLLV   , NI    , SRLV  , SRAV  ,
   JR  , JALR , NI  , NI  , SYSCALL, BREAK , NI    , SYNC  ,
   MFHI, MTHI , MFLO, MTLO, DSLLV  , NI    , DSRLV , DSRAV ,
   MULT, MULTU, DIV , DIVU, DMULT  , DMULTU, DDIV  , DDIVU ,
   ADD , ADDU , SUB , SUBU, AND    , OR    , XOR   , NOR   ,
   NI  , NI   , SLT , SLTU, DADD   , DADDU , DSUB  , DSUBU ,
   NI  , NI   , NI  , NI  , TEQ    , NI    , NI    , NI    ,
   DSLL, NI   , DSRL, DSRA, DSLL32 , NI    , DSRL32, DSRA32
};

static int SPECIAL(MIPS_instr mips){
	return gen_special[MIPS_GET_FUNC(mips)](mips);
}

// -- RegImmed Instructions --

// Since the RegImmed instructions are very similar:
//   BLTZ, BGEZ, BLTZL, BGEZL, BLZAL, BGEZAL, BLTZALL, BGEZALL
//   It's less work to handle them all in one function
static int REGIMM(MIPS_instr mips){
	int which = MIPS_GET_RT(mips);
	int cond   = which & 1; // t = GE, f = LT
	int likely = which & 2;
	int link   = which & 16;

	if(CANT_COMPILE_DELAY() ||
	   (MIPS_GET_IMMED(mips) == 0xffff && OCIOSO_HUECO_NOP())){
		// REGIMM_IDLE || virtual delay
		genCallInterp(mips);
		return INTERPRETED;
	}

	genCmpi64(CR4, MIPS_GET_RA(mips), 0);

	return branch(MIPS_GET_IMMED(mips), cond ? GE : LT, link, likely);
}

// -- COP0 Instructions --

static int TLBR(MIPS_instr mips){
#ifdef INTERPRET_TLBR
	genCallInterp(mips);
	return INTERPRETED;
#else
	return CONVERT_ERROR;
#endif
}

static int TLBWI(MIPS_instr mips){
#ifdef INTERPRET_TLBWI
	genCallInterp(mips);
	return INTERPRETED;
#else
	return CONVERT_ERROR;
#endif
}

static int TLBWR(MIPS_instr mips){
#ifdef INTERPRET_TLBWR
	genCallInterp(mips);
	return INTERPRETED;
#else
	return CONVERT_ERROR;
#endif
}

static int TLBP(MIPS_instr mips){
#ifdef INTERPRET_TLBP
	genCallInterp(mips);
	return INTERPRETED;
#else
	return CONVERT_ERROR;
#endif
}

static int ERET(MIPS_instr mips){
#ifdef INTERPRET_ERET
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_ERET

	flushRegisters();

 #ifdef COMPARE_CORE
 	GEN_LI(4, 0);
	// Hack: we count an extra instruction here; it's not worth working around
	unget_last_src();
 #endif
	genUpdateCount(0);
#ifdef COMPARE_CORE
	get_next_src();
#endif
	// Load Status
	GEN_LWZ(R3, (12*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
	// Status & 0xFFFFFFFD
	GEN_RLWINM(R3, R3, 0, 31, 29);
	// llbit = 0
	GEN_STW(DYNAREG_ZERO, offsetof(R4300, llbit), DYNAREG_R4300);
	// Store updated Status 
	GEN_STW(R3, (12*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
	// check_interupt()
	GEN_B(add_jump((unsigned long)(&check_interupt), 1, 1), 0, 1);
	// Load the old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// r4300.pc = EPC
	GEN_LWZ(R3, (14*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
	// Restore the LR
	GEN_MTLR(R0);
	// Return to trampoline with EPC
	GEN_BLR(0);
	return CONVERT_SUCCESS;
#endif
}

static int (*gen_tlb[64])(MIPS_instr) =
{
   NI  , TLBR, TLBWI, NI, NI, NI, TLBWR, NI,
   TLBP, NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   ERET, NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI
};

static int MFC0(MIPS_instr mips){
#ifdef INTERPRET_MFC0
	genCallInterp(mips);
	return INTERPRETED;
#else
	int rt = mapRegisterNew(MIPS_GET_RT(mips));
	// *rt = r4300.reg_cop0[rd]
	GEN_LWZ(rt, (MIPS_GET_RD(mips)*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
	return CONVERT_SUCCESS;
#endif
}

static int MTC0(MIPS_instr mips){
#ifdef INTERPRET_MTC0
	genCallInterp(mips);
	return INTERPRETED;
#else
	
	int rt = MIPS_GET_RT(mips), rrt;
	int rd = MIPS_GET_RD(mips);
	
	switch(rd){
	case 0: // Index
		rrt = mapRegister(rt);
		// r0 = rt & 0x8000003F
		GEN_RLWINM(R0, rrt, 0, 26, 0);
		// r4300.reg_cop0[rd] = r0
		GEN_STW(R0, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		return CONVERT_SUCCESS;
	
	case 2: // EntryLo0
	case 3: // EntryLo1
		rrt = mapRegister(rt);
		// r0 = rt & 0x3FFFFFFF
		GEN_RLWINM(R0, rrt, 0, 2, 31);
		// r4300.reg_cop0[rd] = r0
		GEN_STW(R0, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		return CONVERT_SUCCESS;
	
	case 4: // Context
		rrt = mapRegister(rt);
		// r0 = r4300.reg_cop0[rd]
		GEN_LWZ(R0, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		// r0 = (rt & 0xFF800000) | (r0 & 0x007FFFFF)
		GEN_RLWIMI(R0, rrt, 0, 0, 8);
		// r4300.reg_cop0[rd] = r0
		GEN_STW(R0, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		return CONVERT_SUCCESS;
	
	case 5: // PageMask
		rrt = mapRegister(rt);
		// r0 = rt & 0x01FFE000
		GEN_RLWINM(R0, rrt, 0, 7, 18);
		// r4300.reg_cop0[rd] = r0
		GEN_STW(R0, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		return CONVERT_SUCCESS;
	
	case 6: // Wired
		rrt = mapRegister(rt);
		// r0 = 31
		GEN_LI(R0, 31);
		// r4300.reg_cop0[rd] = rt
		GEN_STW(rrt, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		// r4300.reg_cop0[1] = r0
		GEN_STW(R0, (1*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		return CONVERT_SUCCESS;
	
	case 10: // EntryHi
		rrt = mapRegister(rt);
		// r0 = rt & 0xFFFFE0FF
		GEN_RLWINM(R0, rrt, 0, 24, 18);
		// r4300.reg_cop0[rd] = r0
		GEN_STW(R0, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		return CONVERT_SUCCESS;
	
	case 13: // Cause
	case 14: // EPC
	case 16: // Config
	case 18: // WatchLo
	case 19: // WatchHi
		rrt = mapRegister(rt);
		// r4300.reg_cop0[rd] = rt
		GEN_STW(rrt, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		return CONVERT_SUCCESS;
	
	case 28: // TagLo
		rrt = mapRegister(rt);
		// r0 = rt & 0x0FFFFFC0
		GEN_RLWINM(R0, rrt, 0, 4, 25);
		// r4300.reg_cop0[rd] = r0
		GEN_STW(R0, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		return CONVERT_SUCCESS;
	
	case 29: // TagHi
		// r4300.reg_cop0[rd] = 0
		GEN_STW(DYNAREG_ZERO, (rd*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		return CONVERT_SUCCESS;
	
	case 1: // Random
	case 8: // BadVAddr
	case 15: // PRevID
	case 27: // CacheErr
		// Do nothing
		return CONVERT_SUCCESS;
	case 12: { // Status
		RegCacheState preCall;
		snapshotRegisters(&preCall);
		emitFlushOf(&preCall);

		// r3 = pc
		GEN_LIS(R3, extractUpper16(get_src_pc()));
		// r5 = new status value
		GEN_LWZ(R5, (rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
		// r4 = old Status
		GEN_LWZ(R4, (12*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		GEN_ADDI(R3, R3, extractLower16(get_src_pc()));
		// Status = newStatus (in r4300.reg_cop0)
		GEN_STW(R5, (12*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		// isDelaySlot
		GEN_LI(R6, isDelaySlot);
		GEN_B(add_jump((unsigned long)(&dyna_cop0_status), 1, 1), 0, 1);
		// Load the old LR
		GEN_LWZ(R0, DYNAOFF_LR, R1);
		// Check if the PC changed
		GEN_CMPI(CR5, R3, 0);
		// Restore the LR
		GEN_MTLR(R0);
		// If dyna_cop0_status returned a new PC, return to
		// the trampoline with it. We're leaving this function for good, so
		// there's no need to reload the register cache first.
		GEN_BNELR(CR5, 0);

		// No interrupt, the call clobbered the volatiles.
		emitReloadOf(&preCall);

		if(mips_is_jump(mips)) delaySlotNext = 2;
		return CONVERT_SUCCESS;
	}
	case 9: // Count
	case 11: // Compare

	default:
		genCallInterp(mips);
		return INTERPRETED;
	}
	
#endif
}

static int TLB(MIPS_instr mips){
#ifdef INTERPRET_TLB
	genCallInterp(mips);
	return INTERPRETED;
#else
	return gen_tlb[mips&0x3f](mips);
#endif
}

static int (*gen_cop0[32])(MIPS_instr) =
{
   MFC0, NI, NI, NI, MTC0, NI, NI, NI,
   NI  , NI, NI, NI, NI  , NI, NI, NI,
   TLB , NI, NI, NI, NI  , NI, NI, NI,
   NI  , NI, NI, NI, NI  , NI, NI, NI
};

static int COP0(MIPS_instr mips){
#ifdef INTERPRET_COP0
	genCallInterp(mips);
	return INTERPRETED;
#else
	return gen_cop0[MIPS_GET_RS(mips)](mips);
#endif
}

// -- COP1 Instructions --

static int MFC1(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_MFC1)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP

	genCheckFP();

	int fs = MIPS_GET_FS(mips);
	int rt = mapRegisterNew( MIPS_GET_RT(mips) );
	flushFPR(fs);

	// rt = r4300.fpr_single[fs]
	GEN_LWZ(rt, (fs*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
	// rt = *rt
	GEN_LWZ(rt, 0, rt);

	return CONVERT_SUCCESS;
#endif
}

static int DMFC1(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_DMFC1)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP

	genCheckFP();

	int fs = MIPS_GET_FS(mips);
	RegMapping rt = mapRegister64New( MIPS_GET_RT(mips) );
	int addr = mapRegisterTemp();
	flushFPR(fs);

	// addr = r4300.fpr_double[fs]
	GEN_LWZ(addr, (fs*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
	// rt[r4300.hi] = *addr
	GEN_LWZ(rt.hi, 0, addr);
	// rt[r4300.lo] = *(addr+4)
	GEN_LWZ(rt.lo, 4, addr);

	unmapRegisterTemp(addr);

	return CONVERT_SUCCESS;
#endif
}

static int CFC1(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_CFC1)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_CFC1

	genCheckFP();

	if(MIPS_GET_FS(mips) == 31){
		int rt = mapRegisterNew( MIPS_GET_RT(mips) );

		GEN_LWZ(rt, offsetof(R4300,fcr31), DYNAREG_R4300);
	} else if(MIPS_GET_FS(mips) == 0){
		int rt = mapRegisterNewUnsigned( MIPS_GET_RT(mips) );

		GEN_LI(rt, 0x511);
	}

	return CONVERT_SUCCESS;
#endif
}

static int MTC1(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_MTC1)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP

	genCheckFP();

	int rt = mapRegister( MIPS_GET_RT(mips) );
	int fs = MIPS_GET_FS(mips);
	int addr = mapRegisterTemp();
	invalidateFPR(fs);

	// addr = r4300.fpr_single[fs]
	GEN_LWZ(addr, (fs*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
	// *addr = rt
	GEN_STW(rt, 0, addr);

	unmapRegisterTemp(addr);

	return CONVERT_SUCCESS;
#endif
}

static int DMTC1(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_DMTC1)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP

	genCheckFP();

	RegMapping rt = mapRegister64( MIPS_GET_RT(mips) );
	int fs = MIPS_GET_FS(mips);
	int addr = mapRegisterTemp();
	invalidateFPR(fs);

	GEN_LWZ(addr, (fs*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
	GEN_STW(rt.hi, 0, addr);
	GEN_STW(rt.lo, 4, addr);

	unmapRegisterTemp(addr);

	return CONVERT_SUCCESS;
#endif
}

static int CTC1(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_CTC1)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_CTC1

	genCheckFP();

	if(MIPS_GET_FS(mips) == 31){
		int rt = mapRegister( MIPS_GET_RT(mips) );
		GEN_STW(rt, offsetof(R4300,fcr31), DYNAREG_R4300);
	}
	return CONVERT_SUCCESS;
#endif
}

static int BC(MIPS_instr mips){
#if defined(INTERPRET_BC)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_BC

	if(CANT_COMPILE_DELAY()){
		genCallInterp(mips);
		return INTERPRETED;
	}

	genCheckFP();

	int cond   = mips & 0x00010000;
	int likely = mips & 0x00020000;
	
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_RLWINM(R0, R0, 9, 31, 31);
	GEN_CMPI(CR4, R0, 0);
	
	return branch(MIPS_GET_IMMED(mips), cond?NE:EQ, 0, likely);
#endif
}

// -- Floating Point Arithmetic --
static int ADD_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_ADD)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_ADD

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );

	GEN_FADD(fd, fs, ft, dbl);
	return CONVERT_SUCCESS;
#endif
}

static int SUB_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_SUB)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_SUB

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );

	GEN_FSUB(fd, fs, ft, dbl);
	return CONVERT_SUCCESS;
#endif
}

static int MUL_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_MUL)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_MUL

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );

	GEN_FMUL(fd, fs, ft, dbl);
	return CONVERT_SUCCESS;
#endif
}

static int DIV_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_DIV)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_DIV

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );

	GEN_FDIV(fd, fs, ft, dbl);
	return CONVERT_SUCCESS;
#endif
}

static int SQRT_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_SQRT)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_SQRT

	genCheckFP();

	flushRegisters();
	mapFPR( MIPS_GET_FS(mips), dbl ); // maps to f1 (FP argument)
	invalidateRegisters();

	// call sqrt
	/* [Xbox360] Los casts de tipo de funcion NO son decorativos: este
	 * fichero se compila como C++, donde `sqrt` es un CONJUNTO DE
	 * SOBRECARGAS (double/float/long double) y `&sqrt` a secas es
	 * ambiguo -- `C2440: cannot convert from 'overloaded-function'`.
	 * El cast a `double(*)(double)` es lo que elige una de ellas. */
	GEN_B(add_jump(dbl ? (unsigned long)(double(*)(double))sqrt
	                   : (unsigned long)(float(*)(float))sqrtf, 1, 1), 0, 1);

	mapFPRNew( MIPS_GET_FD(mips), dbl ); // maps to f1 (FP return)

	// Load old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// Restore LR
	GEN_MTLR(R0);
	return CONVERT_SUCCESS;
#endif
}

static int ABS_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_ABS)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_ABS

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );

	GEN_FABS(fd, fs);
	return CONVERT_SUCCESS;
#endif
}

static int MOV_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_MOV)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_MOV

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );

	GEN_FMR(fd, fs);
	return CONVERT_SUCCESS;
#endif
}

static int NEG_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_NEG)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_NEG

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );

	GEN_FNEG(fd, fs);
	return CONVERT_SUCCESS;
#endif
}

// -- Floating Point Rounding/Conversion --
#define PPC_ROUNDING_NEAREST 0
#define PPC_ROUNDING_TRUNC   1
#define PPC_ROUNDING_CEIL    2
#define PPC_ROUNDING_FLOOR   3
static void set_rounding(int rounding_mode){
	GEN_MTFSFI(R7, rounding_mode);
}

static void set_rounding_reg(int fs){
	GEN_MTFSF(1, fs);
}

static int ROUND_L_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_ROUND_L)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_ROUND_L
	
	genCheckFP();

	flushRegisters();
	int fd = MIPS_GET_FD(mips);
	mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR( MIPS_GET_FS(mips) );

	// round
	/* [Xbox360] [AUTO, log 46] al par, no `round`: ver xbox64_redondeo_par. */
	GEN_B(add_jump(dbl ? (unsigned long)(&xbox64_redondeo_par)
	                   : (unsigned long)(&xbox64_redondeo_parf), 1, 1), 0, 1);
	// convert
	GEN_B(add_jump(dbl ? (unsigned long)(&__fixdfdi) : (unsigned long)(&__fixsfdi), 1, 1), 0, 1);
	//TODO Tehpola check if we should use tmp register?
	int addr = 5; // Use r5 for the addr (to not clobber r3/r4)
	// addr = r4300.fpr_double[fd]
	GEN_LWZ(addr, (fd*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
	// Load old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// stw r3, 0(addr)
	GEN_STW(R3, 0, addr);
	// Restore LR
	GEN_MTLR(R0);
	// stw r4, 4(addr)
	GEN_STW(R4, 4, addr);
	return CONVERT_SUCCESS;
#endif
}

static int TRUNC_L_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_TRUNC_L)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_TRUNC_L

	genCheckFP();

	flushRegisters();
	int fd = MIPS_GET_FD(mips);
	mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR( MIPS_GET_FS(mips) );

	// convert
	GEN_B(add_jump(dbl ? (unsigned long)(&__fixdfdi) : (unsigned long)(&__fixsfdi), 1, 1), 0, 1);
	// TODO Tehpola again
	int addr = 5; // Use r5 for the addr (to not clobber r3/r4)
	// addr = r4300.fpr_double[fd]
	GEN_LWZ(addr, (fd*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
	// Load old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// stw r3, 0(addr)
	GEN_STW(R3, 0, addr);
	// Restore LR
	GEN_MTLR(R0);
	// stw r4, 4(addr)
	GEN_STW(R4, 4, addr);
	return CONVERT_SUCCESS;
#endif
}

static int CEIL_L_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_CEIL_L)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_CEIL_L

	genCheckFP();

	flushRegisters();
	int fd = MIPS_GET_FD(mips);
	mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR( MIPS_GET_FS(mips) );

	// ceil
	/* [Xbox360] Sobrecargas de C++: ver la nota de `sqrt` en SQRT_FP. */
	GEN_B(add_jump(dbl ? (unsigned long)(double(*)(double))ceil
	                   : (unsigned long)(float(*)(float))ceilf, 1, 1), 0, 1);
	// convert
	GEN_B(add_jump(dbl ? (unsigned long)(&__fixdfdi) : (unsigned long)(&__fixsfdi), 1, 1), 0, 1);
	//TODO tehpola again
	int addr = 5; // Use r5 for the addr (to not clobber r3/r4)
	// addr = r4300.fpr_double[fd]
	GEN_LWZ(addr, (fd*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
	// Load old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// stw r3, 0(addr)
	GEN_STW(R3, 0, addr);
	// Restore LR
	GEN_MTLR(R0);
	// stw r4, 4(addr)
	GEN_STW(R4, 4, addr);
	return CONVERT_SUCCESS;
#endif
}

static int FLOOR_L_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_FLOOR_L)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_FLOOR_L

	genCheckFP();

	flushRegisters();
	int fd = MIPS_GET_FD(mips);
	mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR( MIPS_GET_FS(mips) );

	// round
	/* [Xbox360] Sobrecargas de C++: ver la nota de `sqrt` en SQRT_FP. */
	GEN_B(add_jump(dbl ? (unsigned long)(double(*)(double))floor
	                   : (unsigned long)(float(*)(float))floorf, 1, 1), 0, 1);
	// convert
	GEN_B(add_jump(dbl ? (unsigned long)(&__fixdfdi) : (unsigned long)(&__fixsfdi), 1, 1), 0, 1);
	//TODO tehpola again
	int addr = 5; // Use r5 for the addr (to not clobber r3/r4)
	// addr = r4300.fpr_double[fd]
	GEN_LWZ(addr, (fd*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
	// Load old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// stw r3, 0(addr)
	GEN_STW(R3, 0, addr);
	// Restore LR
	GEN_MTLR(R0);
	// stw r4, 4(addr)
	GEN_STW(R4, 4, addr);
	return CONVERT_SUCCESS;
#endif
}

static int ROUND_W_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_ROUND_W)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_ROUND_W

	genCheckFP();

	int fd = MIPS_GET_FD(mips);
	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR(fd);

	// fctiw f0, fs
	GEN_FCTIW(F0, fs);
	// r0 = r4300.fpr_single[fd]
	GEN_LWZ(R0, (fd*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
	// stfiwx f0, 0, r0
	GEN_STFIWX(F0, 0, R0);

	return CONVERT_SUCCESS;
#endif
}

static int TRUNC_W_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_TRUNC_W)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_TRUNC_W

	genCheckFP();

	int fd = MIPS_GET_FD(mips);
	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR(fd);

	// fctiwz f0, fs
	GEN_FCTIWZ(F0, fs);
	// r0 = r4300.fpr_single[fd]
	GEN_LWZ(R0, (fd*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
	// stfiwx f0, 0, r0
	GEN_STFIWX(F0, 0, R0);
	
	return CONVERT_SUCCESS;
#endif
}

static int CEIL_W_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_CEIL_W)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_CEIL_W

	genCheckFP();

	set_rounding(PPC_ROUNDING_CEIL);

	int fd = MIPS_GET_FD(mips);
	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR(fd);

	// fctiw f0, fs
	GEN_FCTIW(F0, fs);
	// r0 = r4300.fpr_single[fd]
	GEN_LWZ(R0, (fd*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
	// stfiwx f0, 0, r0
	GEN_STFIWX(F0, 0, R0);

	set_rounding(PPC_ROUNDING_NEAREST);
	return CONVERT_SUCCESS;
#endif
}

static int FLOOR_W_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_FLOOR_W)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_FLOOR_W

	genCheckFP();

	set_rounding(PPC_ROUNDING_FLOOR);

	int fd = MIPS_GET_FD(mips);
	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR(fd);

	// fctiw f0, fs
	GEN_FCTIW(F0, fs);
	// r0 = r4300.fpr_single[fd]
	GEN_LWZ(R0, (fd*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
	// stfiwx f0, 0, r0
	GEN_STFIWX(F0, 0, R0);
	
	set_rounding(PPC_ROUNDING_NEAREST);
	return CONVERT_SUCCESS;
#endif
}

static int CVT_S_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_CVT_S)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_CVT_S

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int fd = mapFPRNew( MIPS_GET_FD(mips), 0 );

	GEN_FRSP(fd, fs);
	return CONVERT_SUCCESS;
#endif
}

static int CVT_D_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_CVT_D)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_CVT_D

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int fd = mapFPRNew( MIPS_GET_FD(mips), 1 );

	GEN_FMR(fd, fs);
	return CONVERT_SUCCESS;
#endif
}

static int CVT_W_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_CVT_W)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_CVT_W

	genCheckFP();

	// Set rounding mode according to r4300.fcr31
	GEN_LFD(F0, -4+offsetof(R4300,fcr31), DYNAREG_R4300);

	// FIXME: Here I have the potential to disable IEEE mode
	//          and enable inexact exceptions
	set_rounding_reg(F0);

	int fd = MIPS_GET_FD(mips);
	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR(fd);
	int addr = mapRegisterTemp();

	// fctiw f0, fs
	GEN_FCTIW(F0, fs);
	// addr = r4300.fpr_single[fd]
	GEN_LWZ(addr, (fd*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
	// stfiwx f0, 0, addr
	GEN_STFIWX(F0, 0, addr);

	unmapRegisterTemp(addr);
	
	set_rounding(PPC_ROUNDING_NEAREST);
	return CONVERT_SUCCESS;
#endif
}

static int CVT_L_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_CVT_L)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_CVT_L

	genCheckFP();

	flushRegisters();
	int fd = MIPS_GET_FD(mips);
	mapFPR( MIPS_GET_FS(mips), dbl );
	invalidateFPR( MIPS_GET_FS(mips) );

	// FIXME: I'm fairly certain this will always trunc
	/* [Xbox360] [AUTO, log 46] Ya no: primero se redondea segun fcr31, como
	 * CEIL_L_FP hace con ceil.  Ver xbox64_redondeo_modo. */
	GEN_B(add_jump(dbl ? (unsigned long)(&xbox64_redondeo_modo)
	                   : (unsigned long)(&xbox64_redondeo_modof), 1, 1), 0, 1);
	// convert
	GEN_B(add_jump(dbl ? (unsigned long)(&__fixdfdi) : (unsigned long)(&__fixsfdi), 1, 1), 0, 1);
	// TODO Tehpola again
	int addr = 5; // Use r5 for the addr (to not clobber r3/r4)
	// addr = r4300.fpr_double[fd]
	GEN_LWZ(addr, (fd*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
	// Load old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// stw r3, 0(addr)
	GEN_STW(R3, 0, addr);
	// Restore LR
	GEN_MTLR(R0);
	// stw r4, 4(addr)
	GEN_STW(R4, 4, addr);
	return CONVERT_SUCCESS;
#endif
}

// -- Floating Point Comparisons --
static int C_F_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_F)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_F

	genCheckFP();

	// lwz r0, 0(&fcr31)
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	// and r0, r0, 0xff7fffff (clear cond)
	GEN_RLWINM(R0, R0, 0, 9, 7);
	// stw r0, 0(&fcr31)
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	return CONVERT_SUCCESS;
#endif
}

static int C_UN_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_UN)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_UN

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();
	
	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 31, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int C_EQ_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_EQ)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_EQ

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();

	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 30, 8, 8);
	GEN_STW(R0, 0+offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);

	return CONVERT_SUCCESS;
#endif
}

static int C_UEQ_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_UEQ)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_UEQ

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();
	
	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_CROR(CR1*4+2, CR1*4+3, CR1*4+2);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 30, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	
	return CONVERT_SUCCESS;
#endif
}

static int C_OLT_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_OLT)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_OLT

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();
	
	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 28, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	
	return CONVERT_SUCCESS;
#endif
}

static int C_ULT_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_ULT)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_ULT

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();
	
	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_CROR(CR1*4+0, CR1*4+3, CR1*4+0);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 28, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	
	return CONVERT_SUCCESS;
#endif
}

static int C_OLE_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_OLE)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_OLE

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();
	
	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_CRNOR(CR1*4+2, CR1*4+3, CR1*4+1);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 30, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	
	return CONVERT_SUCCESS;
#endif
}

static int C_ULE_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_ULE)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_ULE

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();
	
	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_CRORC(CR1*4+2, CR1*4+3, CR1*4+1);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 30, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	
	return CONVERT_SUCCESS;
#endif
}

static int C_SF_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_SF)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_SF

	genCheckFP();

	// lwz r0, 0(&fcr31)
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	// and r0, r0, 0xff7fffff (clear cond)
	GEN_RLWINM(R0, R0, 0, 9, 7);
	// stw r0, 0(&fcr31)
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	return CONVERT_SUCCESS;
#endif
}

static int C_NGLE_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_NGLE)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_NGLE

	genCheckFP();

	/* [Xbox360] [AUTO, log 46] "ni menor, ni igual, ni mayor" = desordenado:
	 * el codigo de C.UN.  Era una copia de C.SF (borrar siempre): con NaN
	 * daba 0 y tiene que dar 1 (fpu.h, c_ngle). */
	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();

	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 31, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);

	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int C_SEQ_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_SEQ)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_SEQ

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();
	
	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 30, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int C_NGL_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_NGL)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_NGL

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();

	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	/* [Xbox360] [AUTO, log 46] "ni menor ni mayor" = desordenado o ==, como
	 * C.UEQ.  Era una copia de C.SEQ: con NaN daba 0. */
	GEN_CROR(CR1*4+2, CR1*4+3, CR1*4+2);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 30, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int C_LT_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_LT)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_LT

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();
	
	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 28, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int C_NGE_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_NGE)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_NGE

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();

	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	/* [Xbox360] [AUTO, log 46] "no mayor o igual" = desordenado o <, como
	 * C.ULT (fpu.h:695).  Era una copia de C.LT: con NaN daba 0. */
	GEN_CROR(CR1*4+0, CR1*4+3, CR1*4+0);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 28, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int C_LE_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_LE)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_LE

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();
	
	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	GEN_CRNOR(CR1*4+2, CR1*4+3, CR1*4+1);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 30, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int C_NGT_FP(MIPS_instr mips, int dbl){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_C_NGT)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_C_NGT

	genCheckFP();

	int fs = mapFPR( MIPS_GET_FS(mips), dbl );
	int ft = mapFPR( MIPS_GET_FT(mips), dbl );
	int sa = mapRegisterTemp();

	GEN_FCMPU(CR1, fs, ft);
	GEN_LWZ(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	/* [Xbox360] [AUTO, log 46] "no mayor" = desordenado o <=, como C.ULE
	 * (mupen64plus-core fpu.h:728: con NaN pone el bit C).  Era una copia
	 * de C.LE (CRNOR = ordenado y <=): con NaN daba 0. */
	GEN_CRORC(CR1*4+2, CR1*4+3, CR1*4+1);
	GEN_MFCR(sa);
	GEN_RLWIMI(R0, sa, 30, 8, 8);
	GEN_STW(R0, offsetof(R4300,fcr31), DYNAREG_R4300);
	unmapRegisterTemp(sa);
	return CONVERT_SUCCESS;
#endif
}

static int (*gen_cop1_fp[64])(MIPS_instr, int) =
{
   ADD_FP    ,SUB_FP    ,MUL_FP   ,DIV_FP    ,SQRT_FP   ,ABS_FP    ,MOV_FP   ,NEG_FP    ,
   ROUND_L_FP,TRUNC_L_FP,CEIL_L_FP,FLOOR_L_FP,ROUND_W_FP,TRUNC_W_FP,CEIL_W_FP,FLOOR_W_FP,
   NI_COP   ,NI_COP   ,NI_COP  ,NI_COP   ,NI_COP   ,NI_COP   ,NI_COP  ,NI_COP   ,
   NI_COP   ,NI_COP   ,NI_COP  ,NI_COP   ,NI_COP   ,NI_COP   ,NI_COP  ,NI_COP   ,
   CVT_S_FP  ,CVT_D_FP  ,NI_COP  ,NI_COP   ,CVT_W_FP  ,CVT_L_FP  ,NI_COP  ,NI_COP   ,
   NI_COP   ,NI_COP   ,NI_COP  ,NI_COP   ,NI_COP   ,NI_COP   ,NI_COP  ,NI_COP   ,
   C_F_FP    ,C_UN_FP   ,C_EQ_FP  ,C_UEQ_FP  ,C_OLT_FP  ,C_ULT_FP  ,C_OLE_FP ,C_ULE_FP  ,
   C_SF_FP   ,C_NGLE_FP ,C_SEQ_FP ,C_NGL_FP  ,C_LT_FP   ,C_NGE_FP  ,C_LE_FP  ,C_NGT_FP
};

static int S(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_S)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_S
	return gen_cop1_fp[ MIPS_GET_FUNC(mips) ](mips, 0);
#endif
}

static int D(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_D)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_D
	return gen_cop1_fp[ MIPS_GET_FUNC(mips) ](mips, 1);
#endif
}

/* [Xbox360] `0x1p16` es un literal de coma flotante HEXADECIMAL, de C99, y
 * VS2010 no lo entiende: `error C2059: syntax error : 'bad suffix on
 * number'` -- lee `0x1` y se encuentra una `p` pegada.
 *
 * Vale exactamente 1 x 2^16 = 65536.  Es una potencia de dos, o sea
 * REPRESENTABLE EXACTAMENTE en float: escribirlo en decimal no pierde nada.
 * (Esa es justo la razon de ser de la notacion hexadecimal, y por eso aqui
 * se puede sustituir sin pensarlo; con un valor que no fuese potencia de
 * dos habria que mirarlo dos veces.)
 *
 * Lo usa CVT_FP_W para reconstruir un entero de 32 bits sin signo a partir
 * de dos mitades de 16 bits. */
static const float two16 = 65536.0f;   /* 0x1p16 */

static int CVT_FP_W(MIPS_instr mips, int dbl){
#if defined(_MSC_VER) && defined(_XBOX)
	/* [Xbox360] ★★ [Castlevania y Conker, Logs Salvia (4)] CVT.S.W / CVT.D.W
	 * COMPILADAS, SIN `psq_l`.  Iban al interprete (INTERPRET_FP_W en
	 * Interpreter.h) porque la version de Wii64, la de abajo, usa la carga
	 * cuantizada de Gekko, que Xenon no tiene.  MEDIDO (sonda [CPU4], cop1
	 * 14.20): 300.000-700.000 CVT.S.W por segundo con esqueletos y en las
	 * secuencias de Conker, ~1,5-2 ms por VI solo en salir al interprete.
	 *
	 * La secuencia es la de GliGli (mupen64-360, MIPS-to-PPC.c:3369), el
	 * numero magico: el double 0x43300000_80000000 vale 2^52 + 2^31; con el
	 * entero, con el signo cambiado, en la palabra baja vale 2^52 + 2^31 +
	 * entero, y la resta deja el entero EXACTO (cabe en 52 bits).  `fsubs`
	 * en sencilla redondea una sola vez, igual que (float)entero.  Lo unico
	 * distinto de GliGli es DONDE: alli -8(r1), aqui DYNAOFF_CVT (Wrappers.h),
	 * 8 bytes del marco del trampolin que no usa nadie.  Entre las escrituras
	 * y las lecturas no se llama a nada, asi que nadie mas los toca. */
	genCheckFP();

	int fs = MIPS_GET_FS(mips);
	flushFPR(fs);
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );
	int r2 = mapRegisterTemp();

	// r2 = r4300.fpr_single[fs]; r2 = *r2 (el entero)
	GEN_LWZ(r2, (fs*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
	GEN_LWZ(r2, 0, r2);
	// f0 = 2^52 + 2^31
	GEN_LIS(R0, 0x4330);
	GEN_STW(R0, DYNAOFF_CVT, R1);
	GEN_LIS(R0, 0x8000);
	GEN_STW(R0, DYNAOFF_CVT + 4, R1);
	GEN_LFD(F0, DYNAOFF_CVT, R1);
	// fd = 2^52 + 2^31 + entero
	GEN_XORIS(R0, r2, 0x8000);
	GEN_STW(R0, DYNAOFF_CVT + 4, R1);
	GEN_LFD(fd, DYNAOFF_CVT, R1);
	// fd = entero
	GEN_FSUB(fd, fd, F0, dbl);

	unmapRegisterTemp(r2);
	return CONVERT_SUCCESS;
#else
	r4300.two16 = two16;
	genCheckFP();

	int fs = MIPS_GET_FS(mips);
	flushFPR(fs);
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );
	int r2 = mapRegisterTemp();
	int tmp = mapFPRTemp();

	// Get the integer value into a GPR
	// r2 = r4300.fpr_single[fs]
	GEN_LWZ(r2, (fs*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
	GEN_LFS(F0, offsetof(R4300,two16), DYNAREG_R4300);

	GEN_PSQ_L(fd, 0, r2, QR7);
	GEN_PSQ_L(tmp, 2, r2, QR5);
	GEN_FMADD(fd, fd, F0, tmp, dbl);

	unmapFPRTemp(tmp);
	unmapRegisterTemp(r2);
	return CONVERT_SUCCESS;
#endif
}

static int W(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_W)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_W

	int func = MIPS_GET_FUNC(mips);

	if(func == MIPS_FUNC_CVT_S_) return CVT_FP_W(mips, 0);
	if(func == MIPS_FUNC_CVT_D_) return CVT_FP_W(mips, 1);
	else return CONVERT_ERROR;
#endif
}

static int CVT_FP_L(MIPS_instr mips, int dbl){
	r4300.two16 = two16;
	genCheckFP();

	int fs = MIPS_GET_FS(mips);
	flushFPR(fs);
	mapFPRNew( MIPS_GET_FD(mips), dbl ); // f1
	int fd = mapFPRNew( MIPS_GET_FD(mips), dbl );
	int r2 = mapRegisterTemp();
	int tmp = mapFPRTemp();

	// Get the long value into GPRs
	// r2 = r4300.fpr_double[fs]
	GEN_LWZ(r2, (fs*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
	GEN_LFS(F0, offsetof(R4300,two16), DYNAREG_R4300);
	GEN_PSQ_L(fd, 0, r2, QR7);
	GEN_PSQ_L(tmp, 2, r2, QR5);
	GEN_FMADD(fd, fd, F0, tmp, dbl);
	GEN_PSQ_L(tmp, 4, r2, QR5);
	GEN_FMADD(fd, fd, F0, tmp, dbl);
	GEN_PSQ_L(tmp, 6, r2, QR5);
	GEN_FMADD(fd, fd, F0, tmp, dbl);
	
	unmapRegisterTemp(r2);
	unmapFPRTemp(tmp);
	
	return CONVERT_SUCCESS;
}

static int L(MIPS_instr mips){
#if defined(INTERPRET_FP) || defined(INTERPRET_FP_L)
	genCallInterp(mips);
	return INTERPRETED;
#else // INTERPRET_FP || INTERPRET_FP_L

	int func = MIPS_GET_FUNC(mips);

	if(func == MIPS_FUNC_CVT_S_) return CVT_FP_L(mips, 0);
	if(func == MIPS_FUNC_CVT_D_) return CVT_FP_L(mips, 1);
	else return CONVERT_ERROR;
#endif
}

static int (*gen_cop1[32])(MIPS_instr) =
{
   MFC1, DMFC1, CFC1, NI, MTC1, DMTC1, CTC1, NI,
   BC  , NI   , NI  , NI, NI  , NI   , NI  , NI,
   S   , D    , NI  , NI, W   , L    , NI  , NI,
   NI  , NI   , NI  , NI, NI  , NI   , NI  , NI
};

static int COP1(MIPS_instr mips){
	return gen_cop1[MIPS_GET_RS(mips)](mips);
}

/* [Xbox360] Sin `static` en C++: ver la nota de la declaracion (:551). */
#ifdef __cplusplus
int (*gen_ops[64])(MIPS_instr) =
#else
static int (*gen_ops[64])(MIPS_instr) =
#endif
{
   SPECIAL, REGIMM, J   , JAL  , BEQ , BNE , BLEZ , BGTZ ,
   ADDI   , ADDIU , SLTI, SLTIU, ANDI, ORI , XORI , LUI  ,
   COP0   , COP1  , NI  , NI   , BEQL, BNEL, BLEZL, BGTZL,
   DADDI  , DADDIU, LDL , LDR  , NI  , NI  , NI   , NI   ,
   LB     , LH    , LWL , LW   , LBU , LHU , LWR  , LWU  ,
   SB     , SH    , SWL , SW   , SDL , SDR , SWR  , CACHE,
   LL     , LWC1  , NI  , NI   , NI  , LDC1, NI   , LD   ,
   SC     , SWC1  , NI  , NI   , NI  , SDC1, NI   , SD
};



static void genCallInterp(MIPS_instr mips){
	flushRegisters();
	reset_code_addr();
	// Pass in whether this instruction is in the delay slot
	GEN_LI(R5, isDelaySlot);
	// Load our argument into r3 (mips)
	GEN_LIS(R3, extractUpper16(mips));
	// Load the current PC as the second arg
	GEN_LIS(R4, extractUpper16(get_src_pc()));
	// Load the lower halves of mips and PC
	GEN_ADDI(R3, R3, extractLower16(mips));
	GEN_ADDI(R4, R4, extractLower16(get_src_pc()));
	// Branch to decodeNInterpret
	GEN_B(add_jump((unsigned long)(&decodeNInterpret), 1, 1), 0, 1);
	// Load the old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// Check if the PC changed
	GEN_CMPI(CR5, R3, 0);
	// Restore the LR 
	GEN_MTLR(R0);
	// if decodeNInterpret returned an address
	//   jumpTo it
	GEN_BNELR(CR5, 0);

	if(mips_is_jump(mips)) delaySlotNext = 2;
}

/* [Xbox360] Solo JUMPTO_OFF y JUMPTO_ADDR: la rama JUMPTO_REG (cargar el
 * registro y `blr`) era de JR/JALR, que van por genJumpToRegRapido. */
static void genJumpTo(unsigned int loc, unsigned int type){
	// Calculate the destination address
	loc <<= 2;
	if(type == JUMPTO_OFF) loc += get_src_pc();
	else loc |= get_src_pc() & 0xf0000000;
	// Create space to load destination func*
	set_next_dst(PPC_NOP);
	set_next_dst(PPC_NOP);
	// Make this function the LRU
	GEN_LWZ(R3, offsetof(R4300,nextLRU), DYNAREG_R4300);
	GEN_STW(R3, offsetof(PowerPC_func, lru), DYNAREG_FUNC);
	GEN_ADDI(R0, R3, 1);
	GEN_STW(R0, offsetof(R4300,nextLRU), DYNAREG_R4300);
	// Load the address as the return value
	GEN_LIS(R3, extractUpper16(loc));
	GEN_ADDI(R3, R3, extractLower16(loc));
	// Since we could be linking, return on interrupt
	GEN_BGELR(CR3, 0);
	// Store r4300.last_pc for linking
	GEN_STW(R3, offsetof(R4300,last_pc), DYNAREG_R4300);
	GEN_BLR(1);
}

/* [Xbox360] ★★ JR/JALR SIN VOLVER AL DESPACHADOR: el porque y las
 * condiciones, en `xb_jr_tabla` (Wrappers.h).  Sustituye a la rama
 * JUMPTO_REG de genJumpTo, que era solo las dos primeras instrucciones de
 * aqui con un `blr` detras: cada salida de abajo es ESE `blr` (r3 = destino,
 * r30 = esta funcion, LR sin tocar), asi que si algo no cuadra el despachador
 * lo resuelve exactamente como antes.  Medido (logs (2), 2026-10-21): Mario
 * 64 ~7 saltos directos por vuelta al despachador; V-Rally y Turok, 15-35.
 *
 * Llega con los registros volcados (invalidateRegisters) y cr3 = la
 * comprobacion de la interrupcion (genUpdateCount(1)).  Usa r0 y r3-r7, cr5 y
 * ctr, que a la salida de una funcion no lleva nadie.  ~35 instrucciones. */
static void genJumpToRegRapido(void){
	const unsigned int tabla = (unsigned int)&xb_jr_tabla[0];
	const unsigned int inval = (unsigned int)invalid_code;

	// r3 = destino (lo mismo que deja genJumpTo para el despachador)
	GEN_LWZ(R3, (REG_LOCALRS*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
	// Interrupcion pendiente: al despachador, como siempre
	GEN_BGELR(CR3, 0);

	// r4 = &xb_jr_tabla[(r3 >> 2) & (XB_JR_N-1)]; r5 = su direccion
	GEN_LIS(R4, extractUpper16(tabla));
	GEN_ADDI(R4, R4, extractLower16(tabla));
	// r6 = ((r3 >> 2) & (N-1)) * 8 = (r3 << 1) & ((N-1) << 3)
	GEN_RLWINM(R6, R3, 1, 32 - (XB_JR_BITS + 3), 28);
	GEN_LWZUX(R5, R4, R6);
	GEN_CMPL(CR5, R5, R3);
	GEN_BNELR(CR5, 0);                       // otra direccion (o de TLB)
	// r5 = la funcion; 0 = entrada libre
	GEN_LWZ(R5, 4, R4);
	GEN_CMPLI(CR5, R5, 0);
	GEN_BCLR(0, 0xC, (CR5<<2)+2);            // beqlr

	// invalid_code de la pagina y de su alias (update_invalid_addr)
	GEN_LIS(R4, extractUpper16(inval));
	GEN_ADDI(R4, R4, extractLower16(inval));
	GEN_RLWINM(R6, R3, 20, 12, 31);          // r6 = r3 >> 12
	GEN_LBZX(R0, R4, R6);
	GEN_CMPLI(CR5, R0, 0);
	GEN_BNELR(CR5, 0);
	GEN_XORIS(R6, R6, 0x0002);               // (r3 ^ 0x20000000) >> 12
	GEN_LBZX(R0, R4, R6);
	GEN_CMPLI(CR5, R0, 0);
	GEN_BNELR(CR5, 0);

	// r4300.stop: el `while` del despachador
	GEN_LWZ(R0, offsetof(R4300,stop), DYNAREG_R4300);
	GEN_CMPLI(CR5, R0, 0);
	GEN_BNELR(CR5, 0);

	// start_addr <= r3 < end_addr (el mismo rango que find_func)
	GEN_LWZ(R6, offsetof(PowerPC_func,start_addr), R5);
	GEN_CMPL(CR5, R3, R6);
	GEN_BCLR(0, 0xC, (CR5<<2)+0);            // bltlr
	GEN_LWZ(R7, offsetof(PowerPC_func,end_addr), R5);
	GEN_CMPL(CR5, R3, R7);
	GEN_BGELR(CR5, 0);
	// r7 = func->code_addr[(r3 - start) >> 2]; 0 = sin compilar
	GEN_SUBF(R6, R6, R3);                    // r6 = r3 - start
	GEN_RLWINM(R6, R6, 0, 0, 29);            // palabras -> punteros de 4 bytes
	GEN_LWZ(R7, offsetof(PowerPC_func,code_addr), R5);
	GEN_LWZX(R7, R7, R6);
	GEN_CMPLI(CR5, R7, 0);
	GEN_BCLR(0, 0xC, (CR5<<2)+2);            // beqlr

	// Lo que haria el despachador: last_pc, LRU (como JUMPTO_ADDR), r30
	/* ★★ [logs (1), 2026-10-21] NUNCA `addi rX, r0, n`: CON r0 DE FUENTE,
	 * addi NO LEE EL REGISTRO, USA UN 0 LITERAL (igual que addis y la base de
	 * las cargas).  La primera version hacia `addi r0,r0,1` y dejaba
	 * nextLRU = 1 en cada salto rapido (el LRU de la cache de codigo hecho
	 * trizas).  Por eso r6, libre aqui.  JUMPTO_ADDR hace lo mismo con r3 de
	 * fuente. */
	GEN_STW(R3, offsetof(R4300,last_pc), DYNAREG_R4300);
	GEN_LWZ(R6, offsetof(R4300,nextLRU), DYNAREG_R4300);
	GEN_STW(R6, offsetof(PowerPC_func,lru), R5);
	GEN_ADDI(R6, R6, 1);
	GEN_STW(R6, offsetof(R4300,nextLRU), DYNAREG_R4300);
	GEN_MR(DYNAREG_FUNC, R5);
	GEN_MTCTR(R7);
	GEN_BCTR(0);
}

#ifdef LAZY_GEN_CALLS
static void emit_update_count_lazy(int checkCount)
{
	
	// r3 = PC (get_src_pc()+4)
	GEN_LIS(R3, extractUpper16(get_src_pc()+4));
	GEN_ADDI(R3, R3, extractLower16(get_src_pc()+4));
	// Reserve a placeholder for this call site
    PowerPC_instr* slot = reserve_helper_slot();
	// Load old LR
	GEN_LWZ(R0, DYNAOFF_LR, R1);
	// Restore LR
	GEN_MTLR(R0);
    
    // Store it for patching later
	if(checkCount) {
		update_count_check_slots[update_count_check_slot_count++] = slot;
	} else {
		update_count_slots[update_count_slot_count++] = slot;
	}
	//print_gecko("reserved update_count bl at %08X\r\n", slot);
}

static void emit_genCheckFp_lazy(void)
{
	if(FP_need_check || isDelaySlot){
		//print_gecko("genCheckFP\r\n");
		flushRegisters();
		if(!isDelaySlot) reset_code_addr(); // Calling this makes no sense in the context of the delay slot
		
		// Load the current PC as arg 1 (upper half)
		GEN_LIS(R3, extractUpper16(get_src_pc()));
		// Pass in whether this instruction is in the delay slot as arg 2
		GEN_LI(R4, isDelaySlot);
		// Current PC (lower half)
		GEN_ADDI(R3, R3, extractLower16(get_src_pc()));
		
		// Reserve a placeholder for this call site
		PowerPC_instr* slot = reserve_helper_slot();
		
		// Load the old LR
		GEN_LWZ(R0, DYNAOFF_LR, R1);
		// Restore the LR
		GEN_MTLR(R0);
		
		// Don't check for the rest of this mapping
		// Unless this instruction is in a delay slot
		FP_need_check = isDelaySlot;
		
		// Store it for patching later
		gen_checkfp_slots[gen_checkfp_slot_count++] = slot;
		//print_gecko("reserved update_count bl at %08X\r\n", slot);
	}
}
#endif

// Updates Count, and sets cr3 to (cycle_count ? 0)
static void genUpdateCount(int checkCount){
#ifdef LAZY_GEN_CALLS
	emit_update_count_lazy(checkCount);
#else
#ifndef COMPARE_CORE
	// Dynarec inlined code equivalent:
	int tmp = mapRegisterTemp();
	// lis    tmp, pc >> 16
	GEN_LIS(tmp, extractUpper16(get_src_pc()+4));
	// lwz    r0,  0(&r4300.last_pc)     // r0 = r4300.last_pc
	GEN_LWZ(R0, offsetof(R4300,last_pc), DYNAREG_R4300);
	// addi    tmp, tmp, pc & 0xffff  // tmp = pc
	GEN_ADDI(tmp, tmp, extractLower16(get_src_pc()+4));
	// stw    tmp, 0(&r4300.last_pc)     // r4300.last_pc = pc
	GEN_STW(tmp, offsetof(R4300,last_pc), DYNAREG_R4300);
	// subf   r0,  r0, tmp           // r0 = pc - r4300.last_pc
	GEN_SUBF(R0, R0, tmp);
	// lwz    tmp, 9*4(r4300.reg_cop0)     // tmp = Count
	GEN_LWZ(tmp, (9*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
	
	if(count_per_op == 1) {
		// srwi r0, r0, 2                // r0 = (pc - r4300.last_pc)>>2
		GEN_SRWI(R0, R0, 2);
	}
	else if(count_per_op == 2) {
		// srwi r0, r0, 1                // r0 = ((pc - r4300.last_pc)>>2) * 2
		GEN_SRWI(R0, R0, 1);
	}
	else {
		// srwi r0, r0, 2                // r0 = (pc - r4300.last_pc)>>2
		GEN_SRWI(R0, R0, 2);
		// mulli r0, r0, count_per_op    // r0 *= count_per_op
		GEN_MULLI(R0, R0, count_per_op);
	}
	// add    r0,  r0, tmp           // r0 += Count
	GEN_ADD(R0, R0, tmp);
	
	if(checkCount){
		// lwz    tmp, 0(&r4300.next_interrupt) // tmp = r4300.next_interrupt
		GEN_LWZ(tmp, 0+offsetof(R4300,next_interrupt), DYNAREG_R4300);
	}
	// stw    r0,  9*4(r4300.reg_cop0)    // Count = r0
	GEN_STW(R0, (9*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
	if(checkCount){
		// subf   tmp, tmp, r0  // tmp = Count - next_interrupt (cycle_count)
		GEN_SUBF(tmp, tmp, R0);
		// cmpwi  cr3,  tmp, 0  // cr3 = signed cycle_count ? 0
		GEN_CMPI(CR3, tmp, 0);
	}
	// Free tmp register
	unmapRegisterTemp(tmp);
#else
	// Load the current PC as the argument
	GEN_LIS(R3, extractUpper16(get_src_pc()+4));
	GEN_ADDI(R3, R3, extractLower16(get_src_pc()+4));
	// Call dyna_update_count
	GEN_B(add_jump((unsigned long)(&dyna_update_count), 1, 1), 0, 1);
	// Load the lr
	GEN_LWZ(R0, DYNAOFF_LR, 1);
	GEN_MTLR(R0);
	if(checkCount){
		// If cycle_count >= 0 (cr3)
		GEN_CMPI(CR3, R3, 0);
	}
#endif
#endif
}

// Check whether we need to take a FP unavailable exception
static void genCheckFP(void){
#ifdef LAZY_GEN_CALLS
	emit_genCheckFp_lazy();
#else
	if(FP_need_check || isDelaySlot){
		flushRegisters();
		if(!isDelaySlot) reset_code_addr(); // Calling this makes no sense in the context of the delay slot
		// lwz r0, 12*4(r4300.reg_cop0)
		GEN_LWZ(R0, (12*4)+offsetof(R4300,reg_cop0), DYNAREG_R4300);
		// andis. r0, r0, 0x2000
		GEN_ANDIS(R0, R0, 0x2000);
		// bne cr0, end
		GEN_BNE(CR0, 8, 0, 0);
		// Load the current PC as arg 1 (upper half)
		GEN_LIS(R3, extractUpper16(get_src_pc()));
		// Pass in whether this instruction is in the delay slot as arg 2
		GEN_LI(R4, isDelaySlot);
		// Current PC (lower half)
		GEN_ADDI(R3, R3, extractLower16(get_src_pc()));
		// Call dyna_check_cop1_unusable
		GEN_B(add_jump((unsigned long)(&dyna_check_cop1_unusable), 1, 1), 0, 1);
		// Load the old LR
		GEN_LWZ(R0, DYNAOFF_LR, R1);
		// Restore the LR
		GEN_MTLR(R0);
		// Return to trampoline
		GEN_BLR(0);
		// Don't check for the rest of this mapping
		// Unless this instruction is in a delay slot
		FP_need_check = isDelaySlot;
	}
#endif
}

static void genCallDynaMem(memType type, int count, int reverse, int _rs, int _rt, short immed){
	int isPhysical = 1, isVirtual = 1, isUnsafe = 1;
	int isConstant = isRegisterConstant(_rs);
	int constant = getRegisterConstant(_rs) + immed;
	int i;
	
	if(_rs >= MIPS_REG_K0){
		isUnsafe = 0;
		if(!isConstant){
			isConstant = 2;
			constant = r4300.gpr[_rs];	// This feels broken.
		}
	}

#if defined(_XBOX)
	/* [Xbox360] ★ SONDA: LAS CUATRO DESALINEADAS, AL COMPILARLAS.
	 *
	 * ★★ LA VERSION ANTERIOR DE ESTA SONDA ESTABA MAL FILTRADA y hay que
	 * dejarlo escrito: buscaba `_rt == 0` para cazar una carga con destino
	 * $zero, pero para LWC1/LDC1 `_rt` es un registro de COMA FLOTANTE, y
	 * $f0 es un registro normal y corriente.  Resultado: cientos de lineas
	 * de type=7 y type=8 que no eran ningun defecto.  En las cuatro que
	 * importaban (10..13) no salto NI UNA, asi que de paso quedo
	 * descartado que DK64 haga `lwl/lwr $zero`.
	 *
	 * Ahora imprime, AL RECOMPILAR (no al ejecutar), cada sitio con una de
	 * las cuatro desalineadas.  Esta acotada por construccion -- hay
	 * tantas lineas como instrucciones distintas en el juego -- y contesta
	 * algo que hace falta saber: si DK64 usa LDL y LDR, o solo una.  Si
	 * solo aparece una de las dos, la biseccion se acaba aqui.
	 *
	 *     10=LWL  11=LWR  12=LDL  13=LDR   (ppc/Wrappers.h:73-79) */
	/* ★ APAGADA: ya contesto lo que se le pidio -- dio los seis sitios de
	 * LDR de DK64 (80619CD0, 80619CD8, 80619DE0, 80619DE4, 80619EA8,
	 * 80619EAC) y confirmo que van en parejas de `uld`.  Se deja el
	 * codigo porque vuelve a hacer falta en cuanto aparezca otro juego
	 * con un defecto en estas cuatro. */
#if XBOX64_SONDA_DESALINEADAS
	if(type >= MEM_LWL && type <= MEM_LDR){
		sprintf(txtbuffer, "[DYNA] desalineada type=%d pc=%08X rs=%d rt=%d",
		        (int)type, get_src_pc(), _rs, _rt);
		DEBUG_print(txtbuffer, DBG_USBGECKO);
	}
#endif
#endif

#ifdef FASTMEM	
	if(isConstant){
	#ifdef USE_EXPANSION
		if(constant >= 0x80000000 && constant < 0x80800000)
			isVirtual = 0;
		else if(constant >= 0xA0000000 && constant < 0xA0800000)
			isVirtual = 0;
	#else
		if(constant >= 0x80000000 && constant < 0x80400000)
			isVirtual = 0;
		else if(constant >= 0xA0000000 && constant < 0xA0400000)
			isVirtual = 0;
	#endif
		else
			isPhysical = 0;
		
		uint32_t phys = constant & 0x1FFFFFFF;
		if (type >= MEM_SW && phys >= 0x03F00000 && phys <= 0x04900000) {
			isUnsafe = 0;
		}
	}
#endif
	
	if(type == MEM_LDL || type == MEM_LDR || type == MEM_SDL || type == MEM_SDR){
		isPhysical = 0;
		isVirtual = 1;
	}
	
	if(type < MEM_SW || type > MEM_SD)
		isUnsafe = 0;

	if(type == MEM_LWC1 || type == MEM_LDC1 || type == MEM_SWC1 || type == MEM_SDC1)
		genCheckFP();

#if defined(FASTMEM_HOT_NOFLUSH) && defined(FASTMEM)
	// Keep the register cache live across the rdram fast path
	int hotNoFlush = isPhysical && isVirtual &&
	                 (type == MEM_LW  || type == MEM_LWU ||
	                  type == MEM_LH  || type == MEM_LHU ||
	                  type == MEM_LB  || type == MEM_LBU ||
	                  type == MEM_LD  || type == MEM_LL);
	RegCacheState hot_split, hot_out;
#else
	const int hotNoFlush = 0;
#endif

	if(isVirtual || isUnsafe){
		if(!hotNoFlush){
			flushRegisters();
			reset_code_addr_burst(count);
		}
	}

	int rd = mapRegisterTemp();
	int tmp = mapRegisterTemp();
	int rs = mapRegister(_rs);
	
	// addr = rs + immed
	GEN_ADDI(rd, rs, immed);

#if defined(FASTMEM_HOT_NOFLUSH) && defined(FASTMEM)
	// Snapshot the cache
	if(hotNoFlush) snapshotRegisters(&hot_split);
#endif

#ifdef FASTMEM
	int to_slow_id;
	PowerPC_instr* ts_preCall;
	if(isPhysical && isVirtual){
		// If base in physical memory
		GEN_XORIS(R0, rs, 0x8000);
	#ifdef USE_EXPANSION
		GEN_ANDIS(R0, R0, 0xDF80);
	#else
		GEN_ANDIS(R0, R0, 0xDFC0);
	#endif
		to_slow_id = add_jump_special(0);
		GEN_BNE(CR0, to_slow_id, 0, 0);
		ts_preCall = get_curr_dst();
	}

	if(isPhysical){
		switch(type){
			case MEM_LW:
			{
				if(isConstant==1 && (((constant) & 0x7FFFFC) + (count*4) < 32767)) {
					// Save an instruction for lowmem stuff if we can.
					for(i = 0; i < count; i++){
						unsigned int phys = (i*4) + (constant & 0x7FFFFC);
						int rt = mapRegisterNew(_rt + i);
						GEN_LWZ(rt, phys, DYNAREG_RDRAM);
					}
				}
				else {
					for(i = 0; i < count; i++){
						int rt = mapRegisterNew(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 29);
							GEN_LWZUX(rt, tmp, DYNAREG_RDRAM);
						} else {
							GEN_LWZ(rt, i*4, tmp);
						}
					}
				}
				break;
			}
			case MEM_LWU:
			{
				if(isConstant==1 && (((constant) & 0x7FFFFC) + (count*4) < 32767)) {
					for(i = 0; i < count; i++){
						unsigned int phys = (i*4) + (constant & 0x7FFFFC);
						int rt = mapRegisterNewUnsigned(_rt + i);
						GEN_LWZ(rt, phys, DYNAREG_RDRAM);
					}
				}
				else {
					for(i = 0; i < count; i++){
						int rt = mapRegisterNewUnsigned(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 29);
							GEN_LWZUX(rt, tmp, DYNAREG_RDRAM);
						} else {
							GEN_LWZ(rt, i*4, tmp);
						}
					}
				}
				break;
			}
			case MEM_LH:
			{
				if(isConstant==1 && (((constant) & 0x7FFFFE) + (count*2) < 32767)) {
					for(i = 0; i < count; i++){
						unsigned int phys = (i*2) + (constant & 0x7FFFFE);
						int rt = mapRegisterNew(_rt + i);
						GEN_LHA(rt, phys, DYNAREG_RDRAM);
					}
				}
				else {
					for(i = 0; i < count; i++){
						int rt = mapRegisterNew(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 30);
							GEN_LHAUX(rt, tmp, DYNAREG_RDRAM);
						} else {
							GEN_LHA(rt, i*2, tmp);
						}
					}
				}
				break;
			}
			case MEM_LHU:
			{
				if(isConstant==1 && (((constant) & 0x7FFFFE) + (count*2) < 32767)) {
					for(i = 0; i < count; i++){
						unsigned int phys = (i*2) + (constant & 0x7FFFFE);
						int rt = mapRegisterNewUnsigned(_rt + i);
						GEN_LHZ(rt, phys, DYNAREG_RDRAM);
					}
				}
				else {
					for(i = 0; i < count; i++){
						int rt = mapRegisterNewUnsigned(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 30);
							GEN_LHZUX(rt, tmp, DYNAREG_RDRAM);
						} else {
							GEN_LHZ(rt, i*2, tmp);
						}
					}
				}
				break;
			}
			case MEM_LB:
			{
				if(isConstant==1 && (((constant) & 0x7FFFFF) + (count) < 32767)) {
					for(i = 0; i < count; i++){
						unsigned int phys = (i) + (constant & 0x7FFFFF);
						int rt = mapRegisterNew(_rt + i);
						GEN_LBZ(rt, phys, DYNAREG_RDRAM);
						GEN_EXTSB(rt, rt);
					}
				}
				else {
					for(i = 0; i < count; i++){
						int rt = mapRegisterNew(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 31);
							GEN_LBZUX(rt, tmp, DYNAREG_RDRAM);
						} else {
							GEN_LBZ(rt, i, tmp);
						}
						GEN_EXTSB(rt, rt);
					}
				}
				break;
			}
			case MEM_LBU:
			{
				if(isConstant==1 && (((constant) & 0x7FFFFF) + (count) < 32767)) {
					for(i = 0; i < count; i++){
						unsigned int phys = (i) + (constant & 0x7FFFFF);
						int rt = mapRegisterNewUnsigned(_rt + i);
						GEN_LBZ(rt, phys, DYNAREG_RDRAM);
					}
				}
				else {
					for(i = 0; i < count; i++){
						int rt = mapRegisterNewUnsigned(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 31);
							GEN_LBZUX(rt, tmp, DYNAREG_RDRAM);
						} else {
							GEN_LBZ(rt, i, tmp);
						}
					}
				}
				break;
			}
			case MEM_LD:
			{
				if(isConstant==1 && (((constant) & 0x7FFFF8) + (count*8) < 32767)) {
					for(i = 0; i < count; i++){
						unsigned int phys = (i*8) + (constant & 0x7FFFF8);
						RegMapping rt = mapRegister64New(_rt + i);
						GEN_LWZ(rt.hi, phys, DYNAREG_RDRAM);
						GEN_LWZ(rt.lo, phys+4, DYNAREG_RDRAM);
					}
				}
				else {
					for(i = 0; i < count; i++){
						RegMapping rt = mapRegister64New(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 28);
							GEN_LWZUX(rt.hi, tmp, DYNAREG_RDRAM);
						} else {
							GEN_LWZ(rt.hi, i*8, tmp);
						}
						GEN_LWZ(rt.lo, i*8+4, tmp);
					}
				}
				break;
			}
			case MEM_LWC1:
			{
				for(i = 0; i < count; i++){
					int rt = mapFPRNew(_rt + i*2, 0);
					if(i == 0){
						GEN_RLWINM(tmp, rd, 0, 8, 29);
						GEN_LFSUX(rt, tmp, DYNAREG_RDRAM);
					} else {
						GEN_LFS(rt, i*4, tmp);
					}
				}
				break;
			}
			case MEM_LDC1:
			{
				for(i = 0; i < count; i++){
					int rt = mapFPRNew(_rt + i*2, 1);
					if(i == 0){
						GEN_RLWINM(tmp, rd, 0, 8, 28);
						GEN_LFDUX(rt, tmp, DYNAREG_RDRAM);
					} else {
						GEN_LFD(rt, i*8, tmp);
					}
				}
				break;
			}
			case MEM_LL:
			{
				int rt = mapRegisterNew(_rt);
				GEN_RLWINM(tmp, rd, 0, 8, 29);
				GEN_LI(R0, 1);
				GEN_LWZUX(rt, tmp, DYNAREG_RDRAM);
				GEN_STW(R0, offsetof(R4300,llbit), DYNAREG_R4300);
				break;
			}
			case MEM_LWL:
			{
				int rt = mapRegister(_rt);
				int word = mapRegisterTemp();
				int mask = mapRegisterTemp();
				GEN_RLWINM(tmp, rd, 0, 8, 29);
				GEN_CLRLSLWI(R0, rd, 30, 3);
				GEN_LWZUX(word, tmp, DYNAREG_RDRAM);
				GEN_LI(mask, ~0);
				GEN_SLW(mask, mask, R0);
				GEN_ANDC(rt, rt, mask);
				GEN_SLW(word, word, R0);
				GEN_OR(rt, word, rt);
				mapRegisterNew(_rt);
				unmapRegisterTemp(word);
				unmapRegisterTemp(mask);
				break;
			}
			case MEM_LWR:
			{
				int rt = mapRegister(_rt);
				int word = mapRegisterTemp();
				int mask = mapRegisterTemp();
				GEN_NOT(R0, rd);
				GEN_RLWINM(tmp, rd, 0, 8, 29);
				GEN_CLRLSLWI(R0, R0, 30, 3);
				GEN_LWZUX(word, tmp, DYNAREG_RDRAM);
				GEN_LI(mask, ~0);
				GEN_SRW(mask, mask, R0);
				GEN_ANDC(rt, rt, mask);
				GEN_SRW(word, word, R0);
				GEN_OR(rt, word, rt);
				mapRegisterNew(_rt);				
				unmapRegisterTemp(word);
				unmapRegisterTemp(mask);
				break;
			}
			case MEM_LDL:
				break;
			case MEM_LDR:
				break;
			case MEM_SW:
			{
				if(isConstant==1 && (((constant) & 0x7FFFFC) + (count*4) < 32767)) {
					for(i = 0; i < count; i++){
						unsigned int phys = (i*4) + (constant & 0x7FFFFC);
						int rt = mapRegister(_rt + i);
						GEN_STW(rt, phys, DYNAREG_RDRAM);
					}
				}
				else {
					for(i = 0; i < count; i++){
						int rt = mapRegister(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 29);
							GEN_STWUX(rt, tmp, DYNAREG_RDRAM);
						} else {
							GEN_STW(rt, i*4, tmp);
						}
					}
				}
				if(isUnsafe) check_memory_range(count*4);
				break;
			}
			case MEM_SH:
			{
				if(isConstant==1 && (((constant) & 0x7FFFFE) + (count*2) < 32767)) {
					for(i = 0; i < count; i++){
						unsigned int phys = (i*2) + (constant & 0x7FFFFE);
						int rt = mapRegister(_rt + i);
						GEN_STH(rt, phys, DYNAREG_RDRAM);
					}
				}
				else {
					for(i = 0; i < count; i++){
						int rt = mapRegister(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 30);
							GEN_STHUX(rt, tmp, DYNAREG_RDRAM);
						} else {
							GEN_STH(rt, i*2, tmp);
						}
					}
				}
				if(isUnsafe) check_memory_range(count*2);
				break;
			}
			case MEM_SB:
			{
				if(isConstant==1 && (((constant) & 0x7FFFFF) + (count) < 32767)) {
					for(i = 0; i < count; i++){
						unsigned int phys = i + (constant & 0x7FFFFF);
						int rt = mapRegister(_rt + i);
						GEN_STB(rt, phys, DYNAREG_RDRAM);
					}
				}
				else {
					for(i = 0; i < count; i++){
						int rt = mapRegister(_rt + i);
						if(i == 0){
							GEN_RLWINM(tmp, rd, 0, 8, 31);
							GEN_STBUX(rt, tmp, DYNAREG_RDRAM);
						} else {
							GEN_STB(rt, i, tmp);
						}
					}
				}
				if(isUnsafe) check_memory_range(count*1);
				break;
			}
			case MEM_SD:
			{
				for(i = 0; i < count; i++){
					RegMapping rt = mapRegister64(_rt + i);
					if(i == 0){
						GEN_RLWINM(tmp, rd, 0, 8, 28);
						GEN_STWUX(rt.hi, tmp, DYNAREG_RDRAM);
					} else {
						GEN_STW(rt.hi, i*8, tmp);
					}
					GEN_STW(rt.lo, i*8+4, tmp);
				}
				if(isUnsafe) check_memory_range(count*8);
				break;
			}
			case MEM_SWC1:
			{
				for(i = 0; i < count; i++){
					int rt = mapFPR(_rt + i*2, 0);
					if(i == 0){
						GEN_RLWINM(tmp, rd, 0, 8, 29);
						GEN_STFSUX(rt, tmp, DYNAREG_RDRAM);
					} else {
						GEN_STFS(rt, i*4, tmp);
					}
				}
				if(isUnsafe) check_memory_range(count*4);
				break;
			}
			case MEM_SDC1:
			{
				for(i = 0; i < count; i++){
					int rt = mapFPR(_rt + i*2, 1);
					if(i == 0){
						GEN_RLWINM(tmp, rd, 0, 8, 28);
						GEN_STFDUX(rt, tmp, DYNAREG_RDRAM);
					} else {
						GEN_STFD(rt, i*8, tmp);
					}
				}
				if(isUnsafe) check_memory_range(count*8);
				break;
			}
			case MEM_SC:
			{
				int rt = mapRegister(_rt);
				GEN_LWZ(R0, offsetof(R4300,llbit), DYNAREG_R4300);
				GEN_CMPLI(CR2, R0, 0);
				GEN_BEQ(CR2, 6, 0, 0);
				GEN_RLWINM(tmp, rd, 0, 8, 29);
				GEN_STWUX(rt, tmp, DYNAREG_RDRAM);
				if(isUnsafe) check_memory_range(4);
				GEN_MFCR(tmp);
				GEN_RLWINM(mapRegisterNewUnsigned(_rt), tmp, 10, 31, 31);
				GEN_STW(DYNAREG_ZERO, offsetof(R4300,llbit), DYNAREG_R4300);
				break;
			}
			case MEM_SWL:
			{
				int rt = mapRegister(_rt);
				int word = mapRegisterTemp();
				int mask = mapRegisterTemp();
				GEN_RLWINM(tmp, rd, 0, 8, 29);
				GEN_CLRLSLWI(R0, rd, 30, 3);
				GEN_LWZUX(word, tmp, DYNAREG_RDRAM);
				GEN_LI(mask, ~0);
				GEN_SRW(mask, mask, R0);
				GEN_ANDC(word, word, mask);
				GEN_SRW(R0, rt, R0);
				GEN_OR(R0, word, R0);
				GEN_STW(R0, 0, tmp);
				if(isUnsafe) check_memory_range(4);
				unmapRegisterTemp(word);
				unmapRegisterTemp(mask);
				break;
			}
			case MEM_SWR:
			{
				int rt = mapRegister(_rt);
				int word = mapRegisterTemp();
				int mask = mapRegisterTemp();
				GEN_NOT(R0, rd);
				GEN_RLWINM(tmp, rd, 0, 8, 29);
				GEN_CLRLSLWI(R0, R0, 30, 3);
				GEN_LWZUX(word, tmp, DYNAREG_RDRAM);
				GEN_LI(mask, ~0);
				GEN_SLW(mask, mask, R0);
				GEN_ANDC(word, word, mask);
				GEN_SLW(R0, rt, R0);
				GEN_OR(R0, word, R0);
				GEN_STW(R0, 0, tmp);
				if(isUnsafe) check_memory_range(4);
				unmapRegisterTemp(word);
				unmapRegisterTemp(mask);
				break;
			}
			case MEM_SDL:
				break;
			case MEM_SDR:
				break;
		}
	}

	int not_fastmem_id = 0;
	PowerPC_instr* preCall = 0;
	if(isPhysical && isVirtual){
#ifdef FASTMEM_HOT_NOFLUSH
		if(hotNoFlush) snapshotRegisters(&hot_out);
		else
#endif
			flushRegisters();
		// Skip over else
		not_fastmem_id = add_jump_special(1);
		GEN_B(not_fastmem_id, 0, 0);
		preCall = get_curr_dst();

		int ts_callSize = get_curr_dst() - ts_preCall;
		set_jump_special(to_slow_id, ts_callSize+1);
	}
#endif // FASTMEM

	if(isVirtual){
#if XBOX64_TLB_EN_LINEA
		PowerPC_instr *tlbFallo1 = NULL, *tlbFallo2 = NULL, *tlbBien = NULL;
		const int tlbCarga =
			((type == MEM_LW || type == MEM_LWU) && _rt != 0 && _rt + count <= 32) ||
			(count == 1 && _rt != 0 &&
			 (type == MEM_LH || type == MEM_LHU || type == MEM_LB ||
			  type == MEM_LBU || type == MEM_LD)) ||
			(count == 1 && (type == MEM_LWC1 || type == MEM_LDC1));
		int tlbRapida = 0;
#if defined(FASTMEM_HOT_NOFLUSH) && defined(FASTMEM)
		/* [Xbox360] ★★ PARTE 4: LAS CARGAS SIN VOLCAR LOS REGISTROS.  En las
		 * cargas de FASTMEM_HOT_NOFLUSH (base no constante: LW, LWU, LH, LHU,
		 * LB, LBU, LD) la cache de registros sigue VIVA al entrar aqui: el
		 * volcado de `hot_split` va justo debajo.  Asi que la consulta del TLB
		 * se hace ANTES de volcar, y si acierta, `rd` pasa a ser la direccion
		 * FISICA y se salta hacia atras al acceso rapido de RDRAM de arriba
		 * (`ts_preCall`), que ya esta generado: carga directa al registro del
		 * host, su `snapshotRegisters(&hot_out)` y su salto por encima del
		 * camino lento.  Ni volcado, ni el resultado por memoria, ni la recarga
		 * de toda la cache (emitReloadOf).
		 *
		 * ★ Vale porque el estado del host al entrar aqui es EXACTAMENTE el de
		 * `ts_preCall`: el salto al camino lento sale de justo antes, y hasta
		 * decidir solo se tocan R0, `tmp` (temporal, el acceso rapido lo pisa)
		 * y `rd` (solo en el acierto).  El acceso rapido enmascara `rd` con
		 * 0x00FFFFFF, y la fisica es < 8 MB.  Si falla, `rd` sigue intacto y
		 * se sigue por el volcado y dyna_mem de siempre. */
		tlbRapida = hotNoFlush && tlbCarga;
		if(tlbRapida){
			const unsigned int lut = (unsigned int)tlb_LUT_r;
			PowerPC_instr *rFallo0 = NULL, *rFallo1, *rFallo2, *rVuelta;
#if XBOX64_TRAZAS
			/* SONDA de que se compila: los sitios, en cada potencia de 2. */
			static unsigned int sitios4 = 0;
			sitios4++;
			if((sitios4 & (sitios4 - 1)) == 0){
				sprintf(txtbuffer, "[DYNA] TLB parte 4: %u sitios compilados (ultimo pc=%08X)",
				        sitios4, get_src_pc());
				DEBUG_print(txtbuffer, DBG_USBGECKO);
			}
#endif
			if(count > 1){
				// La rafaga entera en la misma pagina: (rd & 0xFFF) + count*4 <= 0x1000
				GEN_RLWINM(R0, rd, 0, 20, 31);
				GEN_CMPLI(CR0, R0, 0x1000 - count*4);
				rFallo0 = get_curr_dst();
				GEN_BGT(CR0, 0, 0, 0);
			}
			GEN_RLWINM(tmp, rd, 22, 10, 29);              // tmp = (rd >> 12) << 2
			GEN_ADDIS(tmp, tmp, extractUpper16(lut));
			GEN_LWZ(R0, extractLower16(lut), tmp);        // r0 = tlb_LUT_r[rd >> 12]
			GEN_CMPLI(CR0, R0, 0);
			rFallo1 = get_curr_dst();
			GEN_BEQ(CR0, 0, 0, 0);                        // sin entrada: dyna_mem
#ifdef USE_EXPANSION
			GEN_ANDIS(tmp, R0, 0x1F80);                   // fisica >= 8 MB
#else
			GEN_ANDIS(tmp, R0, 0x1FC0);                   // fisica >= 4 MB
#endif
			rFallo2 = get_curr_dst();
			GEN_BNE(CR0, 0, 0, 0);
			GEN_RLWINM(R0, R0, 0, 9, 19);                 // r0 &= 0x007FF000
			GEN_RLWIMI(R0, rd, 0, 20, 31);                // r0 |= rd & 0xFFF
			GEN_OR(rd, R0, R0);                           // rd = fisica
			rVuelta = get_curr_dst();
			GEN_B(0, 0, 0);                               // al acceso rapido
			PPC_SET_LI(*rVuelta, ts_preCall - rVuelta);
			if(rFallo0)
				PPC_SET_BD(*rFallo0, get_curr_dst() - rFallo0);
			PPC_SET_BD(*rFallo1, get_curr_dst() - rFallo1);
			PPC_SET_BD(*rFallo2, get_curr_dst() - rFallo2);
		}
#endif
#endif
#if defined(FASTMEM_HOT_NOFLUSH) && defined(FASTMEM)
		if(hotNoFlush){
			// Write the snapshot
			emitFlushOf(&hot_split);
			// Without the flush, we need to put r3 back potentially
			GEN_OR(R3, rd, rd);
		} else
#endif
			invalidateRegisters();
#if XBOX64_TLB_EN_LINEA
		/* [Xbox360] ★★ [Gauntlet Legends] TRADUCCION POR TLB EN LINEA, PARTES 1
		 * Y 2: las cargas.  Ver XBOX64_TLB_EN_LINEA arriba.  Los registros ya
		 * estan volcados (igual que para dyna_mem): si la pagina esta en
		 * tlb_LUT_r y su fisica es RDRAM, se lee aqui y se deja en r4300 (gpr,
		 * o el FPR por fpr_single/fpr_double) COMO LO DEJARIA dyna_mem, y se
		 * salta a la recarga de despues de la llamada.  Si no, la llamada de
		 * siempre.  Hasta decidir solo usa `tmp` y R0, asi que `rd` (la
		 * direccion) llega intacto a dyna_mem; con el acceso ya decidido, `rd`
		 * se reutiliza como puntero.  Los saltos se rellenan aqui mismo
		 * (PPC_SET_BD/LI), sin entradas en la tabla de saltos del bloque.
		 *   parte 1: LW y LWU sueltas
		 *   parte 2: rafagas de LW/LWU sin cruzar de pagina, y LH, LHU, LB,
		 *            LBU, LD, LWC1 y LDC1 sueltas
		 * Las rafagas de los demas tipos, LL y las desalineadas siguen por
		 * dyna_mem.  Las de FASTMEM_HOT_NOFLUSH ya las cubre la parte 4 (arriba,
		 * antes del volcado); aqui quedan LWC1, LDC1 y las de base constante. */
		if(tlbCarga && !tlbRapida){
			const unsigned int lut = (unsigned int)tlb_LUT_r;
			PowerPC_instr *tlbFallo0 = NULL;
			if(count > 1){
				// La rafaga entera en la misma pagina: (rd & 0xFFF) + count*4 <= 0x1000
				GEN_RLWINM(R0, rd, 0, 20, 31);
				GEN_CMPLI(CR0, R0, 0x1000 - count*4);
				tlbFallo0 = get_curr_dst();
				GEN_BGT(CR0, 0, 0, 0);
			}
			GEN_RLWINM(tmp, rd, 22, 10, 29);              // tmp = (rd >> 12) << 2
			GEN_ADDIS(tmp, tmp, extractUpper16(lut));
			GEN_LWZ(R0, extractLower16(lut), tmp);        // r0 = tlb_LUT_r[rd >> 12]
			GEN_CMPLI(CR0, R0, 0);
			tlbFallo1 = get_curr_dst();
			GEN_BEQ(CR0, 0, 0, 0);                        // sin entrada: dyna_mem
			/* Fisica fuera de la RDRAM: dyna_mem.  Como el acceso rapido de
			 * KSEG0 de arriba, no mira la proteccion de framebuffer (rwmem). */
#ifdef USE_EXPANSION
			GEN_ANDIS(tmp, R0, 0x1F80);                   // fisica >= 8 MB
#else
			GEN_ANDIS(tmp, R0, 0x1FC0);                   // fisica >= 4 MB
#endif
			tlbFallo2 = get_curr_dst();
			GEN_BNE(CR0, 0, 0, 0);
			GEN_RLWINM(R0, R0, 0, 9, 19);                 // r0 &= 0x007FF000
			GEN_RLWIMI(R0, rd, 0, 20, 31);                // r0 |= rd & 0xFFF
			if((type == MEM_LW || type == MEM_LWU) && count == 1){
				GEN_LWZX(tmp, DYNAREG_RDRAM, R0);
				GEN_STW(tmp, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
				if(type == MEM_LW){
					GEN_SRAWI(R0, tmp, 31);               // signo, como (signed long)word
				} else {
					GEN_LI(R0, 0);
				}
				GEN_STW(R0, (_rt*8)+offsetof(R4300,gpr), DYNAREG_R4300);
			} else {
				/* Decidido el acceso: `rd` pasa a ser el puntero en la RDRAM. */
				GEN_ADD(rd, DYNAREG_RDRAM, R0);
				switch(type){
				case MEM_LW:
				case MEM_LWU:                             // rafaga: gpr[rt+i] <- addr+i*4
					for(i = 0; i < count; i++){
						GEN_LWZ(tmp, i*4, rd);
						GEN_STW(tmp, ((_rt+i)*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
						if(type == MEM_LW){
							GEN_SRAWI(R0, tmp, 31);
						} else {
							GEN_LI(R0, 0);
						}
						GEN_STW(R0, ((_rt+i)*8)+offsetof(R4300,gpr), DYNAREG_R4300);
					}
					break;
				case MEM_LH:                              // (signed short)hword
					GEN_LHZ(tmp, 0, rd);
					GEN_EXTSH(tmp, tmp);
					GEN_STW(tmp, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
					GEN_SRAWI(R0, tmp, 31);
					GEN_STW(R0, (_rt*8)+offsetof(R4300,gpr), DYNAREG_R4300);
					break;
				case MEM_LHU:                             // hword
					GEN_LHZ(tmp, 0, rd);
					GEN_STW(tmp, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
					GEN_STW(DYNAREG_ZERO, (_rt*8)+offsetof(R4300,gpr), DYNAREG_R4300);
					break;
				case MEM_LB:                              // (signed char)byte
					GEN_LBZ(tmp, 0, rd);
					GEN_EXTSB(tmp, tmp);
					GEN_STW(tmp, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
					GEN_SRAWI(R0, tmp, 31);
					GEN_STW(R0, (_rt*8)+offsetof(R4300,gpr), DYNAREG_R4300);
					break;
				case MEM_LBU:                             // byte
					GEN_LBZ(tmp, 0, rd);
					GEN_STW(tmp, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
					GEN_STW(DYNAREG_ZERO, (_rt*8)+offsetof(R4300,gpr), DYNAREG_R4300);
					break;
				case MEM_LD:                              // dword: alta en +0, baja en +4
					GEN_LWZ(tmp, 0, rd);
					GEN_LWZ(R0, 4, rd);
					GEN_STW(tmp, (_rt*8)+offsetof(R4300,gpr), DYNAREG_R4300);
					GEN_STW(R0, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
					break;
				case MEM_LWC1:                            // *(long*)fpr_single[rt] = word
					GEN_LWZ(tmp, 0, rd);
					GEN_LWZ(rd, (_rt*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
					GEN_STW(tmp, 0, rd);
					break;
				case MEM_LDC1:                            // *(long long*)fpr_double[rt] = dword
					GEN_LWZ(tmp, 0, rd);
					GEN_LWZ(R0, 4, rd);
					GEN_LWZ(rd, (_rt*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
					GEN_STW(tmp, 0, rd);
					GEN_STW(R0, 4, rd);
					break;
				default:
					break;
				}
			}
			tlbBien = get_curr_dst();
			GEN_B(0, 0, 0);                               // a la recarga
			if(tlbFallo0)
				PPC_SET_BD(*tlbFallo0, get_curr_dst() - tlbFallo0);
			PPC_SET_BD(*tlbFallo1, get_curr_dst() - tlbFallo1);
			PPC_SET_BD(*tlbFallo2, get_curr_dst() - tlbFallo2);
#if defined(FASTMEM_HOT_NOFLUSH) && defined(FASTMEM)
			/* Sin volcado previo `tmp` puede ser R3, donde ya se copio la
			 * direccion para dyna_mem: se vuelve a copiar.  Los dos saltos de
			 * fallo aterrizan aqui. */
			if(hotNoFlush)
				GEN_OR(R3, rd, rd);
#endif
		}
		/* [Xbox360] ★★ PARTE 3: LAS ESCRITURAS (SW, SH, SB, SD, SWC1, SDC1
		 * sueltas).  Mismo esquema que las cargas, con tlb_LUT_w (solo paginas
		 * con permiso de escritura), y una condicion mas: que la pagina VIRTUAL
		 * no tenga codigo compilado (`invalid_code[addr >> 12] != 0`).
		 *
		 * ★ ES EXACTAMENTE LO QUE HARIA dyna_mem.  Para una direccion de TLB,
		 * `invalidate_func` (Wrappers.c) solo mira `invalid_code` de la pagina
		 * virtual: el puente fisica->virtual es solo para KSEG0/KSEG1.  Con la
		 * pagina sin codigo no invalidaria nada; con codigo, aqui no se escribe
		 * y va a dyna_mem, que escribe e invalida como siempre.  La escritura en
		 * caliente sobre codigo propio (overlays de Goemon) sigue por dyna_mem.
		 * El valor sale de r4300 en memoria: antes de una escritura los
		 * registros siempre se vuelcan (no es de las de FASTMEM_HOT_NOFLUSH). */
		if(count == 1 &&
		   (type == MEM_SW || type == MEM_SH || type == MEM_SB || type == MEM_SD ||
		    type == MEM_SWC1 || type == MEM_SDC1)){
			const unsigned int lutw = (unsigned int)tlb_LUT_w;
			const unsigned int invc = (unsigned int)invalid_code;
			const unsigned int rwm  = (unsigned int)rwmem;
			const unsigned int rwr  = (unsigned int)rw_rdram;
			PowerPC_instr *tlbFallo3, *tlbFallo4;
			GEN_RLWINM(tmp, rd, 22, 10, 29);              // tmp = (rd >> 12) << 2
			GEN_ADDIS(tmp, tmp, extractUpper16(lutw));
			GEN_LWZ(R0, extractLower16(lutw), tmp);       // r0 = tlb_LUT_w[rd >> 12]
			GEN_CMPLI(CR0, R0, 0);
			tlbFallo1 = get_curr_dst();
			GEN_BEQ(CR0, 0, 0, 0);                        // sin entrada (o sin escritura): dyna_mem
#ifdef USE_EXPANSION
			GEN_ANDIS(tmp, R0, 0x1F80);                   // fisica >= 8 MB
#else
			GEN_ANDIS(tmp, R0, 0x1FC0);                   // fisica >= 4 MB
#endif
			tlbFallo2 = get_curr_dst();
			GEN_BNE(CR0, 0, 0, 0);
			/* La fisica tiene que ser RDRAM NORMAL: si es un framebuffer
			 * protegido (rw_rdramFB), dyna_mem avisa al plugin (fBWrite) y aqui
			 * no.  Mismo criterio que dm_tlb_rdram (Wrappers.c). */
			GEN_RLWINM(tmp, R0, 18, 14, 29);              // tmp = (r0 >> 16) << 2
			GEN_ADDIS(tmp, tmp, extractUpper16(rwm));
			GEN_LWZ(tmp, extractLower16(rwm), tmp);       // tmp = rwmem[r0 >> 16]
			GEN_XORIS(tmp, tmp, (rwr >> 16) & 0xFFFF);
			GEN_XORI(tmp, tmp, rwr & 0xFFFF);             // 0 si es rw_rdram
			GEN_CMPLI(CR0, tmp, 0);
			tlbFallo4 = get_curr_dst();
			GEN_BNE(CR0, 0, 0, 0);
			GEN_RLWINM(tmp, rd, 20, 12, 31);              // tmp = rd >> 12
			GEN_ADDIS(tmp, tmp, extractUpper16(invc));
			GEN_LBZ(tmp, extractLower16(invc), tmp);      // tmp = invalid_code[rd >> 12]
			GEN_CMPLI(CR0, tmp, 0);
			tlbFallo3 = get_curr_dst();
			GEN_BEQ(CR0, 0, 0, 0);                        // pagina con codigo: dyna_mem
			GEN_RLWINM(R0, R0, 0, 9, 19);                 // r0 &= 0x007FF000
			GEN_RLWIMI(R0, rd, 0, 20, 31);                // r0 |= rd & 0xFFF
			/* Decidido el acceso: `rd` pasa a ser el puntero en la RDRAM. */
			GEN_ADD(rd, DYNAREG_RDRAM, R0);
			switch(type){
			case MEM_SW:                                  // word = gpr[rt] (32 bajos)
				GEN_LWZ(tmp, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
				GEN_STW(tmp, 0, rd);
				break;
			case MEM_SH:                                  // hword = gpr[rt]
				GEN_LWZ(tmp, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
				GEN_STH(tmp, 0, rd);
				break;
			case MEM_SB:                                  // byte = gpr[rt]
				GEN_LWZ(tmp, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
				GEN_STB(tmp, 0, rd);
				break;
			case MEM_SD:                                  // dword: alta en +0, baja en +4
				GEN_LWZ(tmp, (_rt*8)+offsetof(R4300,gpr), DYNAREG_R4300);
				GEN_LWZ(R0, (_rt*8+4)+offsetof(R4300,gpr), DYNAREG_R4300);
				GEN_STW(tmp, 0, rd);
				GEN_STW(R0, 4, rd);
				break;
			case MEM_SWC1:                                // word = *(long*)fpr_single[rt]
				GEN_LWZ(tmp, (_rt*4)+offsetof(R4300,fpr_single), DYNAREG_R4300);
				GEN_LWZ(tmp, 0, tmp);
				GEN_STW(tmp, 0, rd);
				break;
			case MEM_SDC1:                                // dword = *(long long*)fpr_double[rt]
				GEN_LWZ(tmp, (_rt*4)+offsetof(R4300,fpr_double), DYNAREG_R4300);
				GEN_LWZ(R0, 4, tmp);
				GEN_LWZ(tmp, 0, tmp);
				GEN_STW(tmp, 0, rd);
				GEN_STW(R0, 4, rd);
				break;
			default:
				break;
			}
			tlbBien = get_curr_dst();
			GEN_B(0, 0, 0);                               // a la vuelta de dyna_mem
			PPC_SET_BD(*tlbFallo1, get_curr_dst() - tlbFallo1);
			PPC_SET_BD(*tlbFallo2, get_curr_dst() - tlbFallo2);
			PPC_SET_BD(*tlbFallo3, get_curr_dst() - tlbFallo3);
			PPC_SET_BD(*tlbFallo4, get_curr_dst() - tlbFallo4);
		}
#endif
		GEN_LI(R4, _rt);
		GEN_LI(R5, count);
		// Pass PC as arg 5 (upper half)
		GEN_LIS(R7, (get_src_pc()+4)>>16);
		// type passed as arg 4
		GEN_LI(R6, type);
		// Lower half of PC
		GEN_ORI(R7, R7, get_src_pc()+4);
		// isDelaySlot as arg 6
		GEN_LI(R8, isDelaySlot);
		// reverse as arg 7
		GEN_LI(R9, reverse);
		// call dyna_mem
		GEN_B(add_jump((unsigned long)&dyna_mem, 1, 1), 0, 1);
		// Load old LR
		GEN_LWZ(R0, DYNAOFF_LR, R1);
		// Check whether we need to take an interrupt
		GEN_CMPI(CR5, R3, 0);
		// Restore LR
		GEN_MTLR(R0);
		// If so, return to trampoline
		GEN_BNELR(CR5, 0);
#if XBOX64_TLB_EN_LINEA
		/* [Xbox360] El atajo de TLB en linea (arriba) aterriza aqui, en la
		 * misma recarga que la vuelta sin interrupcion de dyna_mem. */
		if(tlbBien)
			PPC_SET_LI(*tlbBien, get_curr_dst() - tlbBien);
#endif
#if defined(FASTMEM_HOT_NOFLUSH) && defined(FASTMEM)
		if(hotNoFlush){
			// No interrupt
			emitReloadOf(&hot_out);
		}
#endif
	}

#ifdef FASTMEM
	if(isPhysical && isVirtual){
		int callSize = get_curr_dst() - preCall;
		set_jump_special(not_fastmem_id, callSize+1);
	}
#endif
	unmapRegisterTemp(rd);
	unmapRegisterTemp(tmp);
}

static int mips_is_jump(MIPS_instr instr){
	int opcode = MIPS_GET_OPCODE(instr);
	int format = MIPS_GET_RS    (instr);
	int func   = MIPS_GET_FUNC  (instr);
	return (opcode == MIPS_OPCODE_J     ||
                opcode == MIPS_OPCODE_JAL   ||
                opcode == MIPS_OPCODE_BEQ   ||
                opcode == MIPS_OPCODE_BNE   ||
                opcode == MIPS_OPCODE_BLEZ  ||
                opcode == MIPS_OPCODE_BGTZ  ||
                opcode == MIPS_OPCODE_BEQL  ||
                opcode == MIPS_OPCODE_BNEL  ||
                opcode == MIPS_OPCODE_BLEZL ||
                opcode == MIPS_OPCODE_BGTZL ||
                opcode == MIPS_OPCODE_B     ||
                (opcode == MIPS_OPCODE_R    &&
                 (func  == MIPS_FUNC_JR     ||
                  func  == MIPS_FUNC_JALR)) ||
                (opcode == MIPS_OPCODE_COP1 &&
                 format == MIPS_FRMT_BC)    );
}
