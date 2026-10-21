/**
 * Wii64 - Wrappers.c
 * Copyright (C) 2008, 2009, 2010 Mike Slegeir
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

#include <stdlib.h>
#include <string.h>             /* memset: dynarec_jr_vaciar */
#include "../../gui/DEBUG.h"
#include "../../xenon64_diag.h"   /* XBOX64_TRAZAS */
#include "../Invalid_Code.h"
#include "../../gc_memory/memory.h"
#include "../../main/ROM-Cache.h"
#include "../interupt.h"
#include "../exception.h"
#include "../r4300.h"
#include "../Recomp-Cache.h"
#include "Recompile.h"
#include "Wrappers.h"

/* [Xbox360] ★ Estas TRES son de C, declaradas a mano aqui porque ninguna
 * cabecera las trae, y este fichero se compila como C++.  Sin la guarda
 * piden el simbolo decorado:
 *     unresolved external symbol "void (__cdecl** interp_ops)(void)"
 *     unresolved external symbol "unsigned long __cdecl update_invalid_addr(unsigned long)"
 *
 * ★ Y LAS DOS DE ABAJO SE QUEDAN FUERA A PROPOSITO: `dyna_check_cop1_unusable`
 * y `dyna_mem` las define ESTE fichero, que es C++, y las declara
 * Wrappers.h sin `extern "C"`.  Meterlas aqui dentro las dejaria con dos
 * enlaces distintos en la misma unidad. */
#ifdef __cplusplus
extern "C" {
#endif
extern unsigned long instructionCount;
extern void (*interp_ops[64])(void);
extern unsigned long update_invalid_addr(unsigned long addr);
/* [Xbox360] La tabla de manejadores de la RDRAM normal (memory.c), para el
 * atajo de TLB de dyna_mem (`dm_tlb_rdram`). */
extern void (*rw_rdram[8])();
#ifdef __cplusplus
}
#endif

unsigned int dyna_check_cop1_unusable(unsigned int, int);
unsigned int dyna_mem(unsigned int, unsigned int, int, memType, unsigned int, int, int);

static PowerPC_instr* link_branch = NULL;
static PowerPC_func* last_func;

/* [Xbox360] El registro de LAS VARIANTES DE TLB (ver la nota larga junto a
 * xb_tlbvar_cambia, mas abajo): todo bloque que ha sido variante, activo o
 * apartado. */
static PowerPC_block* s_var[XB_TLBVAR_POOL];
static unsigned int   s_varN = 0;
static void xb_tlbvar_vaciar(void);

/* [Xbox360] ★★ El salto por registro rapido: ver xb_jr_tabla en Wrappers.h. */
XbJrEntrada  xb_jr_tabla[XB_JR_N];

extern "C" void dynarec_jr_vaciar(void){
	memset(xb_jr_tabla, 0, sizeof(xb_jr_tabla));
}

/* Las entradas de `func`.  Solo se apuntan direcciones de DENTRO de la
 * funcion, asi que basta con mirar su rango; si es mas largo que la tabla,
 * la tabla entera. */
extern "C" void dynarec_jr_olvida_func(PowerPC_func* func){
	unsigned int n, k, a;
	if(!func) return;
	if((func->start_addr & 0xC0000000u) != 0x80000000u) return;   /* nunca se apunta */
	n = ((func->end_addr - func->start_addr) >> 2) + 1u;
	if(func->end_addr < func->start_addr || n >= XB_JR_N){
		for(k = 0; k < XB_JR_N; k++)
			if(xb_jr_tabla[k].func == func){
				xb_jr_tabla[k].vaddr = 0;
				xb_jr_tabla[k].func  = NULL;
			}
		return;
	}
	for(k = 0, a = func->start_addr; k < n; k++, a += 4){
		XbJrEntrada* const e = &xb_jr_tabla[(a >> 2) & (XB_JR_N - 1u)];
		if(e->func == func){
			e->vaddr = 0;
			e->func  = NULL;
		}
	}
}

/* [Xbox360] ★★ [log 38] EL ENLACE PENDIENTE NO PUEDE CRUZAR UN VACIADO.
 *
 * `dynarec()` enlaza el bloque que se acaba de ejecutar (`last_func`, su
 * salida `link_branch`) con el siguiente, y la unica guarda es
 * `func_was_freed(last_func)`.  Pero `freed_funcs` solo lo llena
 * `handle_overlap` (Recompile.c): el vaciado ENTERO -- `cpu_deinit` al
 * descargar, `RecompCache_Init` al arrancar otro juego, los bloques tirados
 * al cargar un estado -- libera las funciones con `free_func` sin apuntarlas.
 *
 * En Wii64 el bucle solo se para para entrar al menu.  Aqui se para en CADA
 * fin de frame (`xbox64_frame_boundary` pone `r4300.stop` dentro de
 * `gen_interupt`), y si esa interrupcion llega desde un ayudante
 * (`dyna_cop0_status`, `decodeNInterpret`, `dyna_mem`) el bucle sale con
 * `link_branch` todavia puesto.  Si despues se vacia el codigo, la primera
 * vuelta del juego siguiente llama a `RecompCache_Link` con un
 * `PowerPC_func` LIBERADO: escribe en el monton y parchea once palabras en
 * medio del codigo nuevo.  Log 38: Banjo-Kazooie -> Banjo-Tooie, cae en esta
 * linea en la primera vuelta del segundo juego.
 *
 * Dentro de una partida no cambia nada: solo se llama en los vaciados. */
static void olvida_fis_virt(void);   /* [log 50] el puente fisica->virtual, mas abajo */
void dynarec_olvida_enlace(void)
{
	link_branch = NULL;
	last_func   = NULL;
	/* [log 50] Con todo el codigo tirado, las parejas fisica->virtual ya no
	 * apuntan a nada; las de otro juego solo harian liberar de mas. */
	olvida_fis_virt();
	/* [Castlevania] Y las variantes apartadas: el vaciado recorre blocks[] y
	 * a ellas no llega.  Ver xb_tlbvar_vaciar. */
	xb_tlbvar_vaciar();
	/* [JR rapido] Con todo el codigo tirado, ninguna entrada vale. */
	dynarec_jr_vaciar();
}

/* Recompiled code stack frame:
 *  $sp+12  |
 *  $sp+8   | old cr
 *  $sp+4   | old lr
 *  $sp	    | old sp
 */

#if defined(_MSC_VER) && defined(_XBOX)
/* ===========================================================================
 * [Xbox360] EL TRAMPOLIN, REESCRITO PARA MSVC.  Es la UNICA funcion del
 * dynarec que llevaba ensamblador, y la unica que define el contrato con el
 * codigo emitido.  La version de arriba (GCC) se conserva intacta debajo.
 * ===========================================================================
 *
 * QUE TIENE QUE HACER, y es lo mismo que la de GCC:
 *
 *   entrar con  r28 = &r4300   r29 = rdram   r30 = func   r31 = 0
 *               ctr = code
 *   dejar en el marco, en DYNAOFF_LR, la direccion de retorno -- el codigo
 *   emitido vuelve con `lwz r0,DYNAOFF_LR(r1) / mtlr r0 / blr` (16 sitios)
 *   salir con   r3 = siguiente direccion MIPS
 *               r30 = la funcion que acaba de correr
 *               LR = por donde salio, para poder ENLAZAR el salto
 *
 * ★★ EL MARCO CRECE DE 8 A 112 BYTES, Y DYNAOFF_LR DE 12 A 56.  NO ES
 * PRUDENCIA: ES EL ABI DE XBOX 360, Y ESTA MEDIDO EN ESTE REPOSITORIO.
 *
 * En Wii cualquier cosa por encima de $sp es tuya.  En Xbox 360 NO: el que
 * llama reserva un area donde el LLAMADO vuelca sus argumentos y sus saves
 * de ABI.  La referencia que FUNCIONA lo tiene medido y comentado en
 * `libretro/picodrive/cpu/drc/emit_ppc.c:1732-1741`, que se comio
 * exactamente este fallo:
 *
 *     "the PPC/Xbox 360 ABI DOES have one [parameter save area], at
 *      SP+8..SP+8+8*PTR_SIZE, and the called C function may spill its
 *      arguments there, OVERWRITING the saved LR -> emith_pop_and_ret
 *      restores a garbage LR and returns to invalid code -> hang"
 *
 * y fija la primera ranura segura en `STACK_EXTRA = (8+6)*PTR_SIZE` = 56
 * ("Param, ABI (LR,CR,FP etc) save areas").  Con DYNAOFF_LR en 12, la
 * PRIMERA llamada a C desde codigo recompilado -- `dyna_mem`,
 * `decodeNInterpret`, `dyna_update_count`-- machaca la direccion de retorno.
 * SINTOMA si esto estuviera mal: vuelta a una direccion basura justo
 * despues de una llamada a C, no en el salto.
 *
 * ★ POR QUE DOS FUNCIONES Y NO UNA.  El `__asm` de PowerPC del XDK es
 * limitado; no doy por hecho que acepte etiquetas locales, que es lo que
 * usa la version de GCC (`bl 1f` ... `1:`).  Partiendolo en dos, el salto
 * es un `bl <simbolo>`, que es la forma mas basica que existe.  El efecto
 * es identico: `bl` deja en LR la vuelta -- que ES la direccion de retorno
 * que el codigo emitido necesita-- y el trampolin la guarda en el marco.
 *
 * ★ `dyna_tramp` NO ES `static`: la unica referencia que tiene esta DENTRO
 * de un bloque `__asm`, que el compilador no mira.  Con enlace interno
 * podria desaparecer por codigo muerto.
 *
 * ★ HAY QUE GUARDAR CR.  r28-r31 son no volatiles y se salvan aqui, como es
 * obvio; lo que es facil olvidar es que en Xbox 360 **CR2, CR3 y CR4
 * tambien lo son**, y el codigo emitido los usa -- `MIPS-to-PPC.c:276`
 * escribe CR3 en `update_count_check`.  La lista de clobber de la version
 * de GCC ya los declaraba; alli lo resolvia el compilador.
 *
 * ★ LO QUE **NO** HAY QUE ARREGLAR, y esta comprobado antes de escribir
 * nada, no despues del primer cuelgue:
 *   - r2 y r13 (TLS): `Register-Cache.c:33` los marca NO asignables
 *     ("gp: leave alone!"), asi que el dynarec nunca los reparte.
 *   - red zone: no hay ni un solo acceso a desplazamiento NEGATIVO de r1 en
 *     el codigo emitido (censado sobre los cinco .c de ppc/).
 * Las dos son trampas del port ANTERIOR, que llevaba el dynarec viejo de
 * Wii64.  Este es el nuevo y viene mas limpio.
 */

#define DYNA_FRAME      112   /* multiplo de 16 */
#define DYNA_OFF_LR     56    /* == DYNAOFF_LR de Wrappers.h. Los dos a la vez */
#define DYNA_OFF_OUT    60
#define DYNA_OFF_R28    64
#define DYNA_OFF_R29    68
#define DYNA_OFF_R30    72
#define DYNA_OFF_R31    76
#define DYNA_OFF_CR     80
#define DYNA_OFF_MYLR   84

/* Lo que el trampolin devuelve APARTE del r3.  Va por puntero y no por
 * variables de fichero porque desde un bloque `__asm` no se puede cargar la
 * direccion de un simbolo sin reubicaciones; un puntero llega en un registro
 * y ya esta. */
typedef struct {
	void* func;   /* r30 al salir                                  */
	void* ret;    /* la direccion de retorno que se dejo en el marco */
	void* lr;     /* LR al salir: por donde se fue el codigo         */
} DynaExit;

/* ★ El prototipo va SIN `__declspec(naked)`: MSVC solo lo acepta en la
 * DEFINICION (`error C2488: 'naked' can only be applied to non-member
 * function definitions`).  El prototipo sigue haciendo falta para que el
 * simbolo sea visible aunque su unica referencia este dentro de un `__asm`,
 * que el compilador no mira. */
void dyna_tramp(void);
void __declspec(naked) dyna_tramp(void)
{
	__asm {
		/* El marco sobre el que corre el codigo recompilado.  De aqui para
		 * abajo, los 56 primeros bytes son del ABI y no se tocan. */
		stwu	r1, -DYNA_FRAME(r1)
		/* LR viene del `bl` de dyna_enter: es la direccion de retorno. */
		mflr	r0
		stw		r0, DYNA_OFF_LR(r1)
		bctr
	}
}

static unsigned int __declspec(naked) dyna_enter(void* func, void* code,
                                                 void* r4300p, void* rdramp,
                                                 DynaExit* out)
{
	__asm {
		/* r3=func r4=code r5=&r4300 r6=rdram r7=out */
		mflr	r0
		stwu	r1, -DYNA_FRAME(r1)
		stw		r0,  DYNA_OFF_MYLR(r1)
		mfcr	r0
		stw		r0,  DYNA_OFF_CR(r1)
		stw		r28, DYNA_OFF_R28(r1)
		stw		r29, DYNA_OFF_R29(r1)
		stw		r30, DYNA_OFF_R30(r1)
		stw		r31, DYNA_OFF_R31(r1)
		stw		r7,  DYNA_OFF_OUT(r1)

		mtctr	r4
		mr		r30, r3
		mr		r28, r5
		mr		r29, r6
		li		r31, 0

		bl		dyna_tramp
		/* Vuelve aqui.  r1 sigue apuntando al marco del TRAMPOLIN: nadie lo
		 * ha desapilado todavia, igual que en la version de GCC. */
		mflr	r11						/* r11 = LR al salir = link_branch */
		lwz		r12, DYNA_OFF_LR(r1)	/* r12 = la direccion de retorno   */
		mr		r10, r30				/* r10 = func al salir             */
		addi	r1, r1, DYNA_FRAME		/* desapilar el marco del trampolin*/

		/* Ya estamos de vuelta en NUESTRO marco. */
		lwz		r9,  DYNA_OFF_OUT(r1)
		stw		r10, 0(r9)
		stw		r12, 4(r9)
		stw		r11, 8(r9)

		lwz		r28, DYNA_OFF_R28(r1)
		lwz		r29, DYNA_OFF_R29(r1)
		lwz		r30, DYNA_OFF_R30(r1)
		lwz		r31, DYNA_OFF_R31(r1)
		lwz		r0,  DYNA_OFF_CR(r1)
		mtcr	r0
		lwz		r0,  DYNA_OFF_MYLR(r1)
		addi	r1, r1, DYNA_FRAME
		mtlr	r0
		blr
	}
}

/* [Xbox360] ★ SONDA DE LA PRIMERA ENTRADA AL CODIGO RECOMPILADO.
 *
 * Sintoma que la motiva: el primer `dyna_run` acaba en
 *     Access violation reading location 0x4331DA40
 *     !0x4331DA40()  <-  dyna_enter() line 198
 * o sea que se salto a 0x4331DA40, que no es `code`.
 *
 * ★ HAY DOS CULPABLES POSIBLES Y ESTA SONDA LOS SEPARA, que es la unica
 * razon de que exista:
 *
 *   A) el trampolin: `code` es bueno pero el `mtctr`/`bctr` no lo usa.
 *   B) el recompilador o la cache: `code` ya viene mal de
 *      `func->code_addr[index]`.
 *
 * El depurador NO lo contesta: en un build optimizado los parametros que
 * ensenya son basura -- decia `func=0x00000001`, que no puede ser.
 *
 * COMO SE LEE, y por eso son DOS lineas y no una:
 *   sale 1 y no sale 2  -> `code` apunta a memoria ilegible: es (B).
 *   salen las dos, con palabras que parecen PowerPC -> el bufer esta bien
 *                          y es (A) -- o el permiso de ejecucion.
 *   salen las dos y las palabras son ceros o basura -> (B) tambien, pero
 *                          con el bufer reservado y sin escribir.
 *
 * ★ NO TIENE TOPE DE "las primeras N": se apaga sola en cuanto UNA entrada
 * vuelve viva, asi que o imprime dos lineas o imprime las de la que mata
 * el proceso.  Si el dynarec llegase a funcionar, son dos lineas en toda
 * la sesion. */
/* ★ DOS INTERRUPTORES, Y LA SEPARACION IMPORTA.
 *
 *   XBOX64_DYNA_PROBE        TRES lineas en toda la sesion: la primera
 *                            entrada al codigo recompilado y su vuelta.
 *                            Es el CONTROL POSITIVO de que el dynarec
 *                            ejecuta y regresa vivo, y no cuesta nada.
 *                            Se queda encendido.
 *   XBOX64_DYNA_PROBE_TODAS  una linea POR ENTRADA.  Caza cuelgues dentro
 *                            de la arena, pero son ~78.000 lineas en el
 *                            arranque y hunden el ritmo a 4-9 fps: con
 *                            esto a 1 NINGUNA medida de rendimiento vale.
 */
#define XBOX64_DYNA_PROBE       (XBOX64_TRAZAS && 1)
#define XBOX64_DYNA_PROBE_TODAS 0

/* [Xbox360] ★ Sonda de LDR con oraculo.  La nota larga esta en el propio
 * `case MEM_LDR` de `dyna_mem`.  El valor previo del registro se guarda
 * aqui porque la comparacion ocurre DESPUES de que el `case` ya lo haya
 * pisado. */
/* ★ APAGADA: cumplio.  Hablo con el defecto vivo (addr=8043F854 k=4
 * sh=24: got=0000008000002552 ref=0000C48000002552) y enmudecio con el
 * `ULL` puesto, en DK64 y en Mario 64, con todos los interruptores ya en
 * upstream.  Se deja el codigo: es el oraculo de estas cuatro
 * instrucciones si algun dia vuelven a sospechar. */
#define XBOX64_SONDA_LDR        0

#if XBOX64_SONDA_LDR
static unsigned long long xbox64_ldr_previo = 0;
#endif

#if XBOX64_DYNA_PROBE
static int s_dyna_probe_ok = 0;
#endif

u32 dyna_run(PowerPC_func* func, PowerPC_instr *code){
	DynaExit out;
	u32 next;

#if XBOX64_DYNA_PROBE
	if(!s_dyna_probe_ok){
		char msg[192];
		sprintf(msg, "[DYNA] 1/2 func=%08X code=%08X r4300=%08X rdram=%08X",
		        (unsigned int)func, (unsigned int)code,
		        (unsigned int)&r4300, (unsigned int)rdram);
		DEBUG_print(msg, DBG_USBGECKO);
		/* Leer de `code` va en su PROPIA linea: si esto revienta, la de
		 * arriba ya esta en el log y eso ES el resultado. */
		if(code){
			const unsigned int* w = (const unsigned int*)code;
			sprintf(msg, "[DYNA] 2/2 code[0..3]= %08X %08X %08X %08X",
			        w[0], w[1], w[2], w[3]);
			DEBUG_print(msg, DBG_USBGECKO);
		}
	}
#endif

#if XBOX64_DYNA_PROBE_TODAS
	/* ★ TODAS LAS ENTRADAS, SIN TOPE.  Sirvio para cazar la instruccion
	 * ilegal DENTRO de la arena, que no ocurria en la primera entrada y
	 * que la sonda de arriba -- que se apaga en cuanto una vuelve viva --
	 * no veia.
	 *
	 * ★★ APAGADA, Y EL MOTIVO ES UNA MEDIDA: con esto encendido el log del
	 * arranque son 78.285 lineas, de las cuales 78.172 (el 99,86%) son
	 * esta sonda.  El emulador bajaba a 4-9 fps, y eso NO era el dynarec:
	 * era el coste de escribirlas.  Cualquier cifra de rendimiento tomada
	 * con esto a 1 no significa nada.
	 *
	 * Se deja el codigo entero porque es la herramienta que resolvio dos
	 * cuelgues seguidos; encenderla nunca es caro EN DIAGNOSTICO, solo en
	 * medida.
	 *
	 * ★ QUE CONTESTA: el bloque que falla se entra por `code`, y la
	 * excepcion cae en otra direccion de la arena.  Con la lista de `code`
	 * delante se ve si la direccion del fallo pertenece a un bloque que se
	 * entro alguna vez (salto entre bloques mal enlazado) o a un hueco en
	 * blanco que nadie compilo (0x00000000 = opcode ilegal en PowerPC).
	 *
	 * `ini` es el PC de N64 del bloque: permite cruzarlo con lo que el
	 * juego estaba ejecutando. */
	{
		char msg[128];
		/* ★ `z` ES EL CAMPO QUE DECIDE.  Es el indice de la primera palabra
		 * a CERO en las 64 que siguen al punto de entrada (-1 si no hay
		 * ninguna).  0x00000000 no es una instruccion valida en PowerPC:
		 * es justo lo que la CPU rechazo.
		 *
		 * Con el, las dos causas que quedan se separan sin ambiguedad,
		 * comparando `z` con la distancia del fallo al punto de entrada:
		 *
		 *   z == esa distancia  -> la memoria SI tiene ceros ahi.  El
		 *        bloque se escribio corto: el problema esta en la
		 *        generacion o en la copia, no en las caches.
		 *   z == -1             -> desde C se leen instrucciones buenas y
		 *        la CPU ejecuto otra cosa: I-CACHE RANCIA.  El
		 *        `ICInvalidateRange` no cubre lo que deberia.
		 *
		 * Leerlo desde C es lo que lo hace concluyente: la CPU y el
		 * procesador de datos ven la MISMA memoria pero por caches
		 * distintas, y esa es exactamente la pregunta. */
		int z = -1;
		if(code){
			const unsigned int* w = (const unsigned int*)code;
			int k;
			for(k = 0; k < 64; k++)
				if(w[k] == 0){ z = k; break; }
		}
		sprintf(msg, "[DYNA] e code=%08X ini=%08X fin=%08X z=%d",
		        (unsigned int)code,
		        func ? (unsigned int)func->start_addr : 0,
		        func ? (unsigned int)func->end_addr   : 0,
		        z);
		DEBUG_print(msg, DBG_USBGECKO);

		/* ★★ VOLCADO DEL BLOQUE QUE MATA, Y SOLO DE ESE.
		 *
		 * `z=-1` ya descarto que la memoria tenga ceros, asi que el bloque
		 * NO esta escrito corto.  Quedan dos causas y las separa ver QUE
		 * hay en la palabra 20, que es donde la CPU se nego a ejecutar
		 * (0x84E31E0C = punto de entrada + 0x50):
		 *
		 *   palabra 20 es una instruccion PowerPC razonable
		 *       -> la memoria esta bien y la CPU ejecuto OTRA COSA:
		 *          I-CACHE RANCIA.  El `ICInvalidateRange` no cubre lo que
		 *          deberia, o no llega a tiempo.
		 *   palabra 20 es basura (opcode inexistente)
		 *       -> lo emitio mal el recompilador: es GENERACION DE CODIGO,
		 *          y hay que mirar que instruccion MIPS cae ahi.
		 *
		 * ★ EL FILTRO ES LA FUNCION MIPS, NO LA DIRECCION DE LA ARENA.
		 * Las direcciones salen iguales en las dos ejecuciones, pero
		 * dependen de como reparta el monton; `start_addr` es del juego y
		 * no cambia.  Son 4 lineas por entrada y se entra dos veces. */
		if(func && func->start_addr == 0x80325970 && code){
			const unsigned int* w = (const unsigned int*)code;
			int b;
			for(b = 0; b < 32; b += 8){
				sprintf(msg, "[DYNA] w%02d %08X %08X %08X %08X %08X %08X %08X %08X",
				        b, w[b+0], w[b+1], w[b+2], w[b+3],
				           w[b+4], w[b+5], w[b+6], w[b+7]);
				DEBUG_print(msg, DBG_USBGECKO);
			}
		}
	}
#endif

	end_section(TRAMP_SECTION);

	next = dyna_enter(func, code, &r4300, rdram, &out);

#if XBOX64_DYNA_PROBE
	if(!s_dyna_probe_ok){
		char msg[192];
		s_dyna_probe_ok = 1;
		sprintf(msg, "[DYNA] vuelta next=%08X func=%08X ret=%08X lr=%08X",
		        (unsigned int)next, (unsigned int)out.func,
		        (unsigned int)out.ret, (unsigned int)out.lr);
		DEBUG_print(msg, DBG_USBGECKO);
	}
#endif

	last_func = (PowerPC_func*)out.func;
	link_branch = (out.lr == out.ret) ? NULL
	                                  : ((PowerPC_instr*)out.lr) - 1;

	return next;
}

#else  /* la version de Wii64, intacta */

inline u32 dyna_run(PowerPC_func* func, PowerPC_instr *code){

	PowerPC_instr* return_addr;

	register void* r3  __asm__("r3");
	register void* r28 __asm__("r28") = &r4300;
	register void* r29 __asm__("r29") = rdram;
	register void* r30 __asm__("r30") = func;
	register void* r31 __asm__("r31") = NULL;

	end_section(TRAMP_SECTION);

	__asm__ volatile(
        "mtctr  %4        \n"
        "bl     1f        \n"
        "mflr   %3        \n"
        "lwz    %2, 12(1) \n"
        "addi   1, 1, 8   \n"
        "b      2f        \n"
        "1:               \n"
		"stwu   1, -8(1)  \n"
        "mflr   0         \n"
        "stw    0, 12(1)  \n"
		"bctr             \n"
        "2:               \n"
        : "=r" (r3), "=r" (r30), "=r" (return_addr), "=r" (link_branch)
		: "r" (code), "r" (r28), "r" (r29), "r" (r30), "r" (r31)
        : "r0", "r2", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12",
		  "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
          "lr", "ctr", "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7", "ca");

	last_func = r30;
	link_branch = link_branch == return_addr ? NULL : link_branch - 1;

	return (u32)r3;
}

#endif  /* _MSC_VER && _XBOX */

/* [Xbox360] ★★★ EL PUENTE FISICA -> VIRTUAL (CAUSA DE GOEMON, logs 40-57).
 *
 * SINTOMA: Goemon's Great Adventure se quedaba en negro al elegir personaje
 * con el recompilador; con el interprete puro avanzaba.  Seguia en negro
 * incluso mandando TODAS las instrucciones al interprete desde el dynarec,
 * porque cada una se compilaba con su palabra MIPS incrustada: se
 * ejecutaba codigo VIEJO.
 *
 * MEDIDO (log 50, con un verificador de huellas por funcion ya retirado):
 * el overlay de 0x08000000-0x0800A3EC (virtual, por TLB) cambia de
 * contenido en la MISMA pagina fisica (0x00409000-0x00413FFF) sin que nadie
 * invalide lo compilado.  Lo reescribe la CPU por el alias KSEG0
 * (0x8040xxxx) con almacenamientos normales, que si llaman a
 * `invalidate_func`... con la direccion KSEG0, y los bloques van por pagina
 * VIRTUAL: las funciones de 0x0800xxxx cuelgan de otro bloque y no las tocaba
 * nadie.  Lo mismo valdria para un DMA (dma.c invalida solo KSEG0/KSEG1).
 *
 * ARREGLO, como la referencia (mupen64plus new_dynarec.c:2841-2848, que
 * apunta el codigo por TLB en la lista de su pagina FISICA): al compilar bajo
 * una direccion de TLB se apunta que pagina virtual sale de que pagina
 * fisica, e `invalidate_func` de una direccion KSEG0/KSEG1 libera tambien las
 * funciones de esas virtuales que la cubren -- con sus enlaces, asi que ningun
 * salto enlazado llega al codigo viejo.  Log 56: libera 128+ funciones y
 * avanza; log 57, el control con el puente APAGADO: negro otra vez.  Desde
 * el log 59 va siempre activo, sin opcion: fuera de las paginas con codigo
 * de TLB cuesta una lectura por invalidacion.
 *
 * Hasta XB_FV_HUECOS virtuales por pagina fisica; las entradas no se
 * borran (una vieja solo cuesta una busqueda que no encuentra nada).  Si
 * se llena, se escribe y se sustituye en rueda. */
#define XB_FV_PAGINAS 2048u             /* 8 MB de RDRAM en paginas de 4 KB */
#define XB_FV_HUECOS  8u
static unsigned int s_fisVirt[XB_FV_PAGINAS][XB_FV_HUECOS];   /* pagina virtual + 1; 0 = libre */
static unsigned int s_fvLleno = 0, s_fvLiberadas = 0;
/* [Xbox360] [GoldenEye, ralentizaciones] Parejas quitadas porque su pagina
 * virtual ya apunta a OTRA fisica en el TLB (tlb_LUT_r).  Ver invalidate_func;
 * sale en la misma linea que `s_fvLiberadas`. */
static unsigned int s_fvRancias = 0;

static void olvida_fis_virt(void)
{
	unsigned int p, i;
	for (p = 0; p < XB_FV_PAGINAS; p++)
		for (i = 0; i < XB_FV_HUECOS; i++)
			s_fisVirt[p][i] = 0u;
}

static void apunta_fis_virt(unsigned int vaddr, unsigned long paddr)
{
	unsigned int pf, v, i;
	if (vaddr >= 0x80000000u && vaddr < 0xC0000000u) return;   /* KSEG0/1: ya van por fisica */
	pf = (unsigned int)((paddr & 0x1FFFFFFFu) >> 12);
	if (pf >= XB_FV_PAGINAS) return;
	v = (vaddr >> 12) + 1u;
	for (i = 0; i < XB_FV_HUECOS; i++) {
		if (s_fisVirt[pf][i] == v) return;
		if (s_fisVirt[pf][i] == 0u) { s_fisVirt[pf][i] = v; return; }
	}
	s_fvLleno++;   /* funcional: elige el hueco en rueda */
#if XBOX64_TRAZAS
	if ((s_fvLleno & (s_fvLleno - 1u)) == 0u) {
		sprintf(txtbuffer, "[DYNA] * puente fisica->virtual LLENO #%u: fisica %08X, virtual %08X sustituye a otra",
		        s_fvLleno, (unsigned int)(pf << 12), vaddr & ~0xFFFu);
		DEBUG_print(txtbuffer, DBG_USBGECKO);
	}
#endif
	s_fisVirt[pf][s_fvLleno % XB_FV_HUECOS] = v;
}

/* [Xbox360] ★★ LAS VARIANTES DE TLB (Castlevania con esqueletos, Logs Salvia
 * (4); el interruptor, en Wrappers.h).
 *
 * El codigo compilado cuelga de la pagina VIRTUAL: `blocks[pagina]` con su
 * arbol de funciones.  Cuando TLBWI desmapea una pagina pone su
 * `invalid_code`, y el despachador, al volver a entrar, la tiraba entera
 * (`invalidate_block`) y la recompilaba desde la fisica de ahora.  Bien si la
 * fisica cambio -- el codigo es otro --, pero Castlevania cambia su ventana de
 * overlays ENTRE UNAS POCAS fisicas sin parar: se recompilaba lo mismo una y
 * otra vez.
 *
 * Ahora cada bloque de una pagina de TLB apunta de que fisica salio
 * (`xb_phys`), y al remapear a OTRA fisica el bloque se APARTA en vez de
 * tirarse: se deshacen los enlaces que ENTRAN en sus funciones (ningun salto
 * enlazado puede llegar a su codigo mientras la pagina apunte a otra cosa) y
 * se guarda en la cadena `xb_var` del bloque que pasa a ser el activo.  Si la
 * fisica de ahora ya tiene un apartado, ese vuelve, con su codigo; si no,
 * bloque nuevo y vacio, que es lo de antes.
 *
 * ★ POR QUE LO APARTADO SIGUE SIENDO VALIDO.  Solo cambia si alguien escribe
 * en su fisica, y eso pasa por `invalidate_func` (almacenes, DMA): su puente
 * fisica->virtual libera las funciones afectadas de los apartados igual que
 * las del activo, y una pareja no se quita por "rancia" mientras quede un
 * bloque que salio de esa fisica.  Los vaciados globales (cargar un estado,
 * otro juego) tiran tambien lo apartado (`xb_tlbvar_vaciar`).
 *
 * ★ LO QUE NO CAMBIA.  Si el bloque no sabe de donde salio (`xb_phys` 0:
 * codigo mezclado de dos fisicas, ver el despachador) o el registro esta
 * lleno, se tira como siempre.
 *
 * Las funciones apartadas siguen en la cache del codigo: el LRU puede
 * liberarlas (free_func las quita del arbol de su dueno, `xb_owner`), y sus
 * enlaces de SALIDA se quedan: si el destino se libera o se aparta, su
 * desenlace parchea este codigo. */
static int var_registra(PowerPC_block* b){
	if (b->xb_pool) return 1;
	if (s_varN >= XB_TLBVAR_POOL) return 0;
	s_var[s_varN] = b;
	s_varN++;
	b->xb_pool = s_varN;
	return 1;
}

static void var_quita(PowerPC_block* b){
	unsigned int k;
	PowerPC_block* ult;
	if (!b->xb_pool) return;
	k = b->xb_pool - 1u;
	s_varN--;
	ult = s_var[s_varN];
	s_var[k] = ult;
	ult->xb_pool = k + 1u;
	b->xb_pool = 0;
}

/* Los enlaces que ENTRAN en todas las funciones de un arbol. */
static void var_desenlaza(PowerPC_func_node* n){
	if (!n) return;
	var_desenlaza(n->left);
	var_desenlaza(n->right);
	RecompCache_UnlinkIn(n->function);
}

/* Un apartado fuera: sus funciones a la cache libre y el struct a la basura. */
static void var_tira(PowerPC_block* b){
	invalidate_block(b, 1);
	var_quita(b);
	free(b);
}

/* El apartado de la pagina del activo `act` que salio de la fisica `ph`
 * (pagina + 1), o NULL. */
static PowerPC_block* var_busca(PowerPC_block* act, unsigned int ph){
	PowerPC_block* b;
	if (!act) return NULL;
	for (b = act->xb_var; b; b = b->xb_var)
		if (b->xb_phys == ph) return b;
	return NULL;
}

/* [Xbox360] ★★ [Bomberman 64 2nd Attack, Logs Salvia (9)-(11)] LO GUARDADO
 * SOLO SE REUSA SI LA FISICA SIGUE DICIENDO LO MISMO.  Con las variantes el
 * juego petaba al acabar los creditos (saltaba a una direccion sin memoria:
 * pass0 con mips_code NULL); sin variantes, bien.  Bomberman
 * tiene mucho codigo por TLB (0x60000000 -> fisica 0x80250000, 0x10000000 ->
 * 0x801D0000, 0x4000xxxx...) y cambia sus overlays.  Antes de las variantes,
 * cualquier remapeo tiraba la pagina virtual entera: eso tapaba TODO camino
 * que escriba codigo sin invalidarlo (almacenes con base >= $k0, que
 * genCallDynaMem da por seguros; DMA del RSP; parejas perdidas del puente
 * fisica->virtual cuando se llena).  Al quedarse con la misma fisica o
 * recuperar un apartado, ese codigo viejo volvia a ejecutarse.
 *
 * Ahora cada bloque de variante guarda la huella de los 4 KB de su fisica al
 * compilar (`xb_hash`, despachador) y solo se reusa si la huella de AHORA es
 * la misma: codigo identico = compilado identico (una funcion no pasa de su
 * pagina: pass0 pone un salto al final).  Si no, se tira como siempre.  Fuera
 * de la RDRAM (8 MB) no hay huella (0) y nunca se reusa.  1024 palabras por
 * cambio, mucho menos que recompilar.  Medido (logs (1)): Bomberman la tira
 * ~16 veces (6000xxxx, misma fisica) y ya no peta; Castlevania, cientos en
 * 0E005000 (pagina que el juego reescribe), y sigue a 60. */
#define XB_VAR_RDRAM_PAGINAS 0x800u
static unsigned int var_hash(unsigned int ph){
	const unsigned int* w;
	unsigned int k, h = 2166136261u;
	if (!ph || ph - 1u >= XB_VAR_RDRAM_PAGINAS) return 0;
	w = (const unsigned int*)fast_mem_access(0x80000000u | ((ph - 1u) << 12));
	if (!w) return 0;
	for (k = 0; k < 1024u; k++)
		h = (h ^ w[k]) * 16777619u;   /* FNV-1a por palabras */
	return h | 1u;                    /* nunca 0: 0 = sin huella */
}

/* El despachador, en una pagina de TLB con `invalid_code` puesto y bloque.
 * 1 = resuelto aqui (la pagina queda lista); 0 = tirarla como siempre. */
static int xb_tlbvar_cambia(unsigned int pag, unsigned long paddr){
	PowerPC_block* act = blocks[pag];
	PowerPC_block* b;
	PowerPC_block* prev;
	unsigned int ph, n;

	if (!act || !paddr) return 0;
	ph = (unsigned int)((paddr & 0x1FFFFFFFu) >> 12) + 1u;
	if (!act->xb_phys) return 0;   /* no se sabe de donde salio: como siempre */
	/* ★ [Logs Salvia (5)] LA MISMA FISICA: SE QUEDA, NO SE TIRA.  Con
	 * esqueletos `reusa` y `nueva` salian 0 con ~450 paginas tiradas por
	 * segundo: el juego remapea la ventana P1 -> P2 -> P1 ENTRE dos visitas
	 * del despachador, que al llegar la ve otra vez en la fisica de la que
	 * salio su codigo -- y la tiraba "por si acaso" y la recompilaba.  Su
	 * codigo sigue siendo el de esa fisica: lo que se escriba en ella lo
	 * libera el puente fisica->virtual, como en todo lo demas.  Es lo que hace
	 * la referencia (mupen64plus `tlb_unmap` no invalida nada) y lo que este
	 * arbol ya hace con TLBWR (pure_interp.c, Conker).  Solo se ponen a 0 sus
	 * `invalid_code`.
	 * [Logs Salvia (11)] Si la huella de la fisica no cuadra, ha cambiado
	 * por un camino que no invalido: se tira como antes de las variantes. */
	if (act->xb_phys == ph) {
		if (!act->xb_hash || act->xb_hash != var_hash(ph)) return 0;
		init_block(act);
		return 1;
	}
	if (s_varN + 2u > XB_TLBVAR_POOL) return 0;   /* sin sitio: como siempre */

	/* El apartado de esta fisica, si lo hay, fuera de la cadena. */
	prev = act;
	for (b = act->xb_var; b && b->xb_phys != ph; b = b->xb_var)
		prev = b;
	if (b) {
		prev->xb_var = b->xb_var;
		/* [Logs Salvia (11)] La fisica cambio desde que se aparto: su codigo
		 * no vale.  El struct se queda (ya esta en el registro), vacio. */
		if (!b->xb_hash || b->xb_hash != var_hash(ph)) {
			invalidate_block(b, 1);
			b->xb_hash = 0;
		}
	} else {
		b = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
		if (!b) return 0;
		b->start_address = pag << 12;
		b->xb_phys = ph;
	}
	var_registra(act);
	var_registra(b);

	/* Apartar el activo: nada enlazado entra ya en su codigo. */
	var_desenlaza(act->funcs);
	b->xb_var = act;          /* el apartado mas reciente, el primero */
	blocks[pag] = b;
	init_block(b);            /* invalid_code de la pagina y de su fisica a 0 */

	/* Como mucho XB_TLBVAR_N apartados: los del final son los mas viejos. */
	n = 0;
	prev = b;
	while (prev->xb_var) {
		PowerPC_block* x = prev->xb_var;
		if (++n > XB_TLBVAR_N) {
			prev->xb_var = x->xb_var;
			var_tira(x);
		} else
			prev = x;
	}
	return 1;
}

/* Los vaciados (dynarec_olvida_enlace): lo apartado no esta en blocks[].
 * Primero el codigo que quede en cualquiera (los activos ya vienen vacios);
 * despues los structs: los que siguen en blocks[] se quedan, sin cadena, y
 * el resto fuera. */
static void xb_tlbvar_vaciar(void){
	unsigned int k;
	for (k = 0; k < s_varN; k++)
		invalidate_block(s_var[k], 1);
	for (k = 0; k < s_varN; k++) {
		PowerPC_block* b = s_var[k];
		b->xb_pool = 0;
		if (blocks[b->start_address >> 12] == b)
			b->xb_var = NULL;
		else
			free(b);
	}
	s_varN = 0;
}

/* Ver Wrappers.h: desde cpu_deinit, ANTES de que libere los blocks[].  Con
 * el registro vaciado aqui, los activos quedan como bloques normales (y
 * cpu_deinit los libera) y los apartados ya no existen. */
extern "C" void dynarec_tlbvar_vaciar(void){
	xb_tlbvar_vaciar();
	/* [JR rapido] cpu_deinit va a liberar todos los bloques. */
	dynarec_jr_vaciar();
}

void dynarec(unsigned int address){
	while(!r4300.stop){
		//print_gecko("PC: %08X cop0[9]: %08X\r\n",address,r4300.reg_cop0[9]);
#ifdef PROFILE
		refresh_stat();
		
		start_section(TRAMP_SECTION);
#endif
		unsigned long paddr = update_invalid_addr(address);
		/*
		sprintf(txtbuffer, "trampolining to 0x%08x\n", address);
		DEBUG_print(txtbuffer, DBG_USBGECKO);
		*/
		if(!paddr){ 
			link_branch = NULL;
			address = r4300.pc;
			paddr = update_invalid_addr(address);
		}
		
		if(!blocks[address>>12]){
			/* [Xbox360] casts: fichero compilado como C++ */
			blocks[address>>12] = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
			if(!blocks[address>>12]) {
				release(1);
				blocks[address>>12] = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
			}
			blocks[address>>12]->start_address = address & ~0xFFF;
			init_block(blocks[address>>12]);
		} else if(invalid_code_get(address>>12)){
			/* [Xbox360] [Castlevania] Pagina de TLB remapeada: si es a la
			 * misma fisica (y con la misma huella) se queda; si es a OTRA, se
			 * aparta y se recupera la variante de la fisica de ahora, si la
			 * hay.  Si no se puede (origen desconocido, huella distinta,
			 * registro lleno), se tira como siempre.  Ver xb_tlbvar_cambia. */
			if ((address & 0xC0000000u) == 0x80000000u ||
			    !xb_tlbvar_cambia(address>>12, paddr))
				invalidate_block(blocks[address>>12], 0);
		}

		PowerPC_func* func = find_func(&blocks[address>>12]->funcs, address);

		if(!func || !func->code_addr[(address-func->start_addr)>>2]){
			blocks[address>>12]->mips_code = (MIPS_instr*)fast_mem_access(paddr & ~0xFFF);
			/* [Xbox360] [Castlevania] La fisica de la que sale lo que se va a
			 * compilar, en el bloque.  Vacio: la de ahora.  Con codigo de OTRA
			 * fisica (el TLB cambio sin invalidar: TLBWR, cargar un estado),
			 * el bloque mezcla dos y pasa a "no se sabe" (0), que es tratarlo
			 * como antes de las variantes. */
			if ((address & 0xC0000000u) != 0x80000000u) {
				PowerPC_block* const bb = blocks[address>>12];
				const unsigned int ph = (unsigned int)((paddr & 0x1FFFFFFFu) >> 12) + 1u;
				if (bb->xb_phys != ph)
					bb->xb_phys = bb->funcs ? 0u : ph;
			}
#ifdef PROFILE
			start_section(COMPILER_SECTION);
#endif
			//print_gecko("Recompile %08X\r\n",address);
			func = recompile_block(blocks[address>>12], address);
#ifdef PROFILE
			end_section(COMPILER_SECTION);
#endif
			/* [Xbox360] [Bomberman, Logs Salvia (11)] La huella de la fisica de
			 * la que acaba de salir este codigo: ver var_hash. */
			if ((address & 0xC0000000u) != 0x80000000u) {
				PowerPC_block* const bb = blocks[address>>12];
				bb->xb_hash = bb->xb_phys ? var_hash(bb->xb_phys) : 0u;
			}
			/* [Xbox360] [log 50] El puente fisica -> virtual. */
			apunta_fis_virt(address, paddr);
		} else {
#ifdef USE_RECOMP_CACHE
			RecompCache_Update(func);
#endif
		}

		int index = (address - func->start_addr)>>2;

		// Recompute the block offset
		PowerPC_instr *code = (PowerPC_instr *)func->code_addr[index];

		/* [Xbox360] [JR rapido] El destino resuelto, para el proximo JR/JALR
		 * que salte aqui (Wrappers.h).  Solo KSEG0/KSEG1: en TLB la misma
		 * virtual puede apuntar a otra fisica sin que nadie lo marque. */
		if((address & 0xC0000000u) == 0x80000000u && code){
			XbJrEntrada* const e = &xb_jr_tabla[(address >> 2) & (XB_JR_N - 1u)];
			e->vaddr = address;
			e->func  = func;
		}

		// Create a link if possible
		/* [Xbox360] ★ [Bomberman, Logs Salvia (11)] Y NUNCA DESDE UNA VARIANTE
		 * APARTADA.  Si la funcion de la que se sale es de un bloque que ya no
		 * es el de su pagina (se acaba de apartar en esta misma vuelta: su
		 * pagina cambio de fisica), el enlace quedaria en su codigo apuntando
		 * a la variante de AHORA; al recuperarla, saltaria a codigo de otra
		 * fisica.  Antes de las variantes esa funcion estaba liberada y
		 * `func_was_freed` lo evitaba. */
		if(link_branch && !func_was_freed(last_func) &&
		   !(last_func->xb_owner &&
		     last_func->xb_owner != blocks[last_func->start_addr >> 12]))
			RecompCache_Link(last_func, link_branch, func, code);
		else if(link_branch){
			/* [Xbox360] [Conker, log 12] Enlace pendiente desde una funcion
			 * liberada o RECOMPILADA (Recompile.c, rama de RecompCache_Realloc):
			 * enlazarlo parchearia codigo que ya no es suyo.  Se cuenta;
			 * escribe la 1, 2, 4...  `last_func` NO se desreferencia: en los
			 * casos de `handle_overlap` puede estar liberada con `free`. */
#if XBOX64_TRAZAS
			static unsigned int s_enlDescartados = 0;
			s_enlDescartados++;
			if((s_enlDescartados & (s_enlDescartados - 1u)) == 0u){
				sprintf(txtbuffer, "[DYNA] enlace descartado: la funcion de origen %08X se libero o se recompilo; destino %08X (%u)",
				        (unsigned int)last_func, address, s_enlDescartados);
				DEBUG_print(txtbuffer, DBG_USBGECKO);
			}
#endif
		}
		clear_freed_funcs();

		r4300.pc = address = dyna_run(func, code);
		
		if(!r4300.noCheckInterrupt){
			r4300.last_pc = r4300.pc;
			// Check for interrupts
			if(cp0_cycle_count >= 0){
				link_branch = NULL;
				gen_interupt();
				address = r4300.pc;
			}
		}
		r4300.noCheckInterrupt = 0;
	}
	r4300.pc = address;
}

unsigned int dyna_cop0_status(unsigned int pc, unsigned int oldStatus,
                              unsigned int newStatus, int isDelaySlot) {
	r4300.pc = pc;
	r4300.delay_slot = isDelaySlot;

	newStatus &= ~0x080000;
	if ((newStatus & 0x04000000) != (oldStatus & 0x04000000)) {
	  set_fpr_pointers(newStatus);
	}
	Status = newStatus;
	r4300.pc+=4;
	update_count();
	check_interupt();
	interrupt_unsafe_state |= INTR_UNSAFE_R4300;
	if (cp0_cycle_count >= 0)  {
		gen_interupt();
	}
	interrupt_unsafe_state &= ~INTR_UNSAFE_R4300;
	r4300.delay_slot = 0;

	if(r4300.pc != pc + 4) r4300.noCheckInterrupt = 1;

	return r4300.pc != pc + 4 ? r4300.pc : 0;
}

unsigned int decodeNInterpret(MIPS_instr mips, unsigned int pc,
                              int isDelaySlot){
	r4300.delay_slot = isDelaySlot; // Make sure we set r4300.delay_slot properly
	r4300.pc = pc;
#ifdef PROFILE
	start_section(INTERP_SECTION);
#endif
	prefetch_opcode(mips);
	interp_ops[MIPS_GET_OPCODE(mips)]();
#ifdef PROFILE
	end_section(INTERP_SECTION);
#endif
	r4300.delay_slot = 0;

	if(r4300.pc != pc + 4) r4300.noCheckInterrupt = 1;

	return r4300.pc != pc + 4 ? r4300.pc : 0;
}

#ifdef COMPARE_CORE
int dyna_update_count(unsigned int pc, int isDelaySlot){
#else
int dyna_update_count(unsigned int pc){
#endif
	Count += ((pc - r4300.last_pc) >> 2) * count_per_op;
	r4300.last_pc = pc;

#ifdef COMPARE_CORE
	if(isDelaySlot){
		r4300.pc = pc;
		compare_core();
	}
#endif

	return cp0_cycle_count;
}

unsigned int dyna_check_cop1_unusable(unsigned int pc, int isDelaySlot){
	// Set state so it can be recovered after exception
	r4300.delay_slot = isDelaySlot;
	r4300.pc = pc;
	// Take a FP unavailable exception
	Cause = (11 << 2) | 0x10000000;
	exception_general();
	// Reset state
	r4300.delay_slot = 0;
	r4300.noCheckInterrupt = 1;
	// Return the address to trampoline to
	return r4300.pc;
}

void invalidate_func(unsigned int addr){
	if(!invalid_code_get(addr>>12)){
		PowerPC_func* func = find_func(&blocks[addr>>12]->funcs, addr);
		if(func)
			RecompCache_FreeFunc(func);   /* [Xbox360] por puntero (Recomp-Cache.h) */
	}
	/* [Xbox360] [log 50] ★ Y las funciones compiladas bajo una direccion de
	 * TLB que sale de esta pagina fisica: ver `apunta_fis_virt`.  Sin mirar
	 * `invalid_code` de la virtual: aunque este marcada, sus funciones
	 * siguen vivas y ENLAZADAS hasta que alguien entre por `dynarec()`. */
	if(addr >= 0x80000000u && addr < 0xC0000000u){
		const unsigned int pf = (addr & 0x1FFFFFFFu) >> 12;
		unsigned int i;
		if(pf < XB_FV_PAGINAS){
			i = 0;
			while(i < XB_FV_HUECOS){
				const unsigned int v = s_fisVirt[pf][i];
				PowerPC_func* f;
				/* [log 58] Los huecos se llenan en orden y solo se vacian
				 * todos a la vez (olvida_fis_virt): el primero libre acaba la
				 * busqueda.  Asi una pagina sin codigo de TLB, que es casi
				 * cualquier almacenamiento, cuesta una sola lectura. */
				if(!v) break;
				/* [Xbox360] [Castlevania] Las variantes de TLB: un apartado que
				 * salio de ESTA fisica tiene el codigo que se esta pisando,
				 * aunque la virtual apunte hoy a otra.  Se libera lo afectado
				 * y la pareja no es "rancia" mientras quede un bloque de aqui.
				 * Y el activo solo si salio de aqui (o no se sabe de donde).
				 *
				 * ★ [GoldenEye, ralentizaciones] LA PAREJA RANCIA SE QUITA Y NO
				 * LIBERA NADA.  Si la virtual apunta HOY a OTRA pagina fisica (y
				 * ningun bloque salio de esta), lo que esta compilado ahi salio
				 * de esa otra: escribir en esta no le afecta.  Medido con una
				 * sonda (log 17): de 16384 funciones liberadas por el puente,
				 * 9735 eran de parejas asi -- codigo valido tirado y
				 * recompilado, ~300/s.  Solo con la virtual MAPEADA a otra: sin
				 * mapear (`lut` 0) se libera como antes, que es el patron de un
				 * overlay (desmapear, escribir, volver a mapear la misma).  Al
				 * quitarla, las de detras suben un hueco, para que el primero
				 * libre siga acabando la busqueda. */
				{
					PowerPC_block* const act = blocks[v - 1u];
					PowerPC_block* const var = var_busca(act, pf + 1u);
					const int actDeAqui = (act && act->xb_phys == pf + 1u);
					if (var) {
						PowerPC_func* const fv = find_func(&var->funcs, ((v - 1u) << 12) | (addr & 0xFFFu));
						if (fv) {
							if (fv == last_func) link_branch = NULL;
							RecompCache_FreeFunc(fv);
						}
					}
#ifndef USE_TLB_CACHE
					if (!var && !actDeAqui) {
						const unsigned long lut = tlb_LUT_r[v - 1u];
						if(lut && ((lut & 0x1FFFFFFFu) >> 12) != pf){
							unsigned int j;
							for(j = i; j + 1u < XB_FV_HUECOS; j++)
								s_fisVirt[pf][j] = s_fisVirt[pf][j + 1u];
							s_fisVirt[pf][XB_FV_HUECOS - 1u] = 0u;
#if XBOX64_TRAZAS
							s_fvRancias++;
#endif
							continue;   /* el mismo hueco, que ahora es la siguiente */
						}
					}
#endif
					/* El activo salio de OTRA fisica: escribir aqui no le toca. */
					if (act && act->xb_phys && !actDeAqui) { i++; continue; }
				}
				if(!blocks[v - 1u]) { i++; continue; }
				f = find_func(&blocks[v - 1u]->funcs, ((v - 1u) << 12) | (addr & 0xFFFu));
				if(!f) { i++; continue; }
#if XBOX64_TRAZAS
				s_fvLiberadas++;
				if((s_fvLiberadas & (s_fvLiberadas - 1u)) == 0u){
					sprintf(txtbuffer, "[DYNA] puente fisica->virtual: %u funciones liberadas, %u parejas rancias quitadas (ultima %08X-%08X por la fisica %08X)",
					        s_fvLiberadas, s_fvRancias, f->start_addr, f->end_addr, addr);
					DEBUG_print(txtbuffer, DBG_USBGECKO);
				}
#endif
				/* La de la que se acaba de salir: sin enlace pendiente
				 * desde ella (`freed_funcs` no la apuntaria; log 38). */
				if(f == last_func) link_branch = NULL;
				RecompCache_FreeFunc(f);   /* [Xbox360] por puntero */
				i++;
			}
		}
	}
}

void invalidate_func_range(unsigned int addr, unsigned int bytes){
	unsigned int i;
	for(i = 0; i < bytes; i += 4) invalidate_func(addr + i);
}

/* [Xbox360] ★★ [Gauntlet Legends, logs 21-24] EL ATAJO DE TLB DEL CAMINO LENTO.
 *
 * Gauntlet tiene sus datos en direcciones mapeadas por TLB (su codigo corre en
 * E000xxxx), asi que el codigo generado no puede usar su acceso rapido (solo
 * cubre una base en 0x80/0xA0) y llama aqui.  MEDIDO (sonda [DMEM], ya
 * retirada): ~79.000 accesos por VI, ~23 ms por VI solo en las llamadas a TLB, unos 290 ns
 * cada uno, con 0 recargas de TLB y 0 accesos a framebuffer.  O sea que el
 * coste es la cadena en si: rwmem[] (salto indirecto) -> read_nomem ->
 * virtual_to_physical_address -> rwmem[] otra vez -> read_rdram, todo a traves
 * de las globales `address`/`word`, que se escriben y se releen al instante
 * (load-hit-store en Xenon).
 *
 * El atajo hace la MISMA traduccion que virtual_to_physical_address (tlb.c,
 * rama sin USE_TLB_CACHE) con una sola lectura de la tabla, y lee o escribe la
 * RDRAM como read_rdram, read_rdramh... y write_rdram... (memory.c).  Solo si las tres cosas se
 * cumplen; si no, NULL y se sigue por el camino de siempre:
 *   - la direccion no es KSEG0/KSEG1 (esas no son TLB);
 *   - la pagina tiene entrada en la tabla (sin ella, el camino de siempre
 *     dispara la excepcion de recarga); escrituras con tlb_LUT_w, que solo
 *     tiene las paginas con permiso de escritura;
 *   - la pagina fisica es RDRAM NORMAL (`rw_rdram`): ni framebuffer
 *     protegido (rw_rdramFB), ni registros, ni nada mas.
 * Las escrituras siguen invalidando el codigo compilado igual que antes. */
static __inline unsigned char *dm_tlb_rdram(unsigned int a, int escribe)
{
#ifndef USE_TLB_CACHE
	unsigned long lut;
	unsigned int pa;
	if ((a & 0xC0000000u) == 0x80000000u)
		return NULL;
	lut = escribe ? tlb_LUT_w[a >> 12] : tlb_LUT_r[a >> 12];
	if (!lut)
		return NULL;
	pa = (unsigned int)((lut & 0xFFFFF000u) | (a & 0xFFFu));
	if (rwmem[pa >> 16] != rw_rdram)
		return NULL;
	return rdramb + (pa & 0x7FFFFFu);
#else
	(void)a; (void)escribe;
	return NULL;
#endif
}

unsigned int dyna_mem(unsigned int addr, unsigned int value, int count,
                      memType type, unsigned int pc, int isDelaySlot,
                      int reverse){
	int i;
	const int istart = reverse ? count - 1 : 0;
	const int iend   = reverse ? -1        : count;
	const int istep  = reverse ? -1        : 1;

	//start_section(DYNAMEM_SECTION);
	r4300.pc = pc;
	r4300.delay_slot = isDelaySlot;
	
	uint32_t phys = addr & 0x1FFFFFFF;
	int needsCheck = 1;
	if (type >= MEM_SW && phys >= 0x03F00000 && phys <= 0x04900000) {
		needsCheck = 0;
	}

	switch(type){
		/* [Xbox360] En cada carga, primero el atajo de TLB (dm_tlb_rdram). */
		case MEM_LW:
			for(i = istart; i != iend; i += istep){
				const unsigned char *p = dm_tlb_rdram(addr + i*4, 0);
				if(p){ r4300.gpr[value + i] = (signed long)*(const unsigned long *)p; continue; }
				address = addr + i*4;
				read_word_in_memory();
				if(!address) break;
				r4300.gpr[value + i] = (signed long)word;
			}
			break;
		case MEM_LWU:
			for(i = istart; i != iend; i += istep){
				const unsigned char *p = dm_tlb_rdram(addr + i*4, 0);
				if(p){ r4300.gpr[value + i] = *(const unsigned long *)p; continue; }
				address = addr + i*4;
				read_word_in_memory();
				if(!address) break;
				r4300.gpr[value + i] = word;
			}
			break;
		case MEM_LH:
			for(i = istart; i != iend; i += istep){
				const unsigned char *p = dm_tlb_rdram(addr + i*2, 0);
				if(p){ r4300.gpr[value + i] = (signed short)*(const unsigned short *)p; continue; }
				address = addr + i*2;
				read_hword_in_memory();
				if(!address) break;
				r4300.gpr[value + i] = (signed short)hword;
			}
			break;
		case MEM_LHU:
			for(i = istart; i != iend; i += istep){
				const unsigned char *p = dm_tlb_rdram(addr + i*2, 0);
				if(p){ r4300.gpr[value + i] = *(const unsigned short *)p; continue; }
				address = addr + i*2;
				read_hword_in_memory();
				if(!address) break;
				r4300.gpr[value + i] = hword;
			}
			break;
		case MEM_LB:
			for(i = istart; i != iend; i += istep){
				const unsigned char *p = dm_tlb_rdram(addr + i, 0);
				if(p){ r4300.gpr[value + i] = (signed char)*p; continue; }
				address = addr + i;
				read_byte_in_memory();
				if(!address) break;
				r4300.gpr[value + i] = (signed char)byte;
			}
			break;
		case MEM_LBU:
			for(i = istart; i != iend; i += istep){
				const unsigned char *p = dm_tlb_rdram(addr + i, 0);
				if(p){ r4300.gpr[value + i] = *p; continue; }
				address = addr + i;
				read_byte_in_memory();
				if(!address) break;
				r4300.gpr[value + i] = byte;
			}
			break;
		case MEM_LD:
			for(i = istart; i != iend; i += istep){
				const unsigned char *p = dm_tlb_rdram(addr + i*8, 0);
				if(p){
					r4300.gpr[value + i] = ((unsigned long long)((const unsigned long *)p)[0] << 32) |
					                       ((const unsigned long *)p)[1];
					continue;
				}
				address = addr + i*8;
				read_dword_in_memory();
				if(!address) break;
				r4300.gpr[value + i] = dword;
			}
			break;
		case MEM_LWC1:
			for(i = istart; i != iend; i += istep){
				const unsigned char *p = dm_tlb_rdram(addr + i*4, 0);
				if(p){ *((long*)r4300.fpr_single[value + i*2]) = *(const long *)p; continue; }
				address = addr + i*4;
				read_word_in_memory();
				if(!address) break;
				*((long*)r4300.fpr_single[value + i*2]) = word;
			}
			break;
		case MEM_LDC1:
			for(i = istart; i != iend; i += istep){
				const unsigned char *p = dm_tlb_rdram(addr + i*8, 0);
				if(p){
					*((long long*)r4300.fpr_double[value + i*2]) =
						(long long)(((unsigned long long)((const unsigned long *)p)[0] << 32) |
						            ((const unsigned long *)p)[1]);
					continue;
				}
				address = addr + i*8;
				read_dword_in_memory();
				if(!address) break;
				*((long long*)r4300.fpr_double[value + i*2]) = dword;
			}
			break;
		case MEM_LL:
			address = addr;
			read_word_in_memory();
			if(!address) break;
			r4300.gpr[value] = (signed long)word;
			r4300.llbit = 1;
			break;
		case MEM_LWL:
			address = addr & ~3;
			read_word_in_memory();
			if(!address) break;
			if((addr & 3) == 0){
				r4300.gpr[value] = (signed long)word;
			} else {
				u32 shift = (addr & 3) * 8;
				u32 mask = 0xFFFFFFFF << shift;
				r4300.gpr[value] = (r4300.gpr[value] & ~mask) | ((word << shift) & mask);
			}
			break;
		case MEM_LWR:
			address = addr & ~3;
			read_word_in_memory();
			if(!address) break;
			if((~addr & 3) == 0){
				r4300.gpr[value] = (signed long)word;
			} else {
				u32 shift = (~addr & 3) * 8;
				u32 mask = 0xFFFFFFFF >> shift;
				r4300.gpr[value] = (r4300.gpr[value] & ~mask) | ((word >> shift) & mask);
			}
			break;
		/* [Xbox360] ★★★ CAUSA RAIZ DE LA FASE, Y ES UNA `U`.
		 *
		 * Las cuatro desalineadas de 64 bits construyen su mascara con el
		 * literal `0xFFFFFFFFFFFFFFFF`.  Upstream lo escribe con sufijo
		 * `LL`, y GCC -- siguiendo la norma -- le da tipo `unsigned long
		 * long`, porque el valor no cabe en el con signo.  ESTE
		 * COMPILADOR NO: le da `long long`, o sea el valor -1.  MEDIDO,
		 * no deducido:
		 *
		 *     [DYNA] literal: (0xFF..FFLL >> 56) = FFFFFFFFFFFFFFFF
		 *                                  (sin signo daria 000000FF)
		 *
		 * Con el literal a -1, el `>>` pasa a ser ARITMETICO y la mascara
		 * sale TODO UNOS para cualquier desplazamiento.  Entonces `~mask`
		 * es cero, el termino `(gpr & ~mask)` desaparece y el valor
		 * previo del registro SE PIERDE ENTERO.  La sonda lo pillo en
		 * vivo:
		 *
		 *     LDR addr=8043F854 k=4 sh=24
		 *       prev=0000C40000000002  dw=8000002552A7E1BA
		 *       got =0000008000002552  <- los bytes altos, borrados
		 *       ref =0000C48000002552
		 *
		 * ★ POR QUE SOLO SE NOTABA EN LDR.  De las cuatro, LDR y SDL
		 * desplazan a la DERECHA y LDL y SDR a la IZQUIERDA.  Un `<<` de
		 * -1 produce el mismo patron de bits que sin signo, asi que LDL y
		 * SDR funcionaban.  Eso es exactamente lo que dijo la biseccion:
		 * LDL inocente, LDR culpable.  (SDL tenia el mismo defecto y no
		 * dio la cara porque DK64 no la usa.)
		 *
		 * ★ EL ARREGLO ES `ULL` EN LOS CUATRO SITIOS, y no solo en los
		 * dos que fallan: el literal es el mismo y con `LL` los otros dos
		 * quedan desplazando a la izquierda un valor negativo, que
		 * ademas es comportamiento indefinido.  En los dos de `<<` no
		 * cambia ni un bit; es consistencia, no un arreglo distinto.
		 *
		 * ★ Esto NO es tocar la referencia: `ULL` es el tipo que GCC le
		 * da a ese mismo literal.  Lo que se corrige es que MSVC no lo
		 * hace, no lo que Wii64 quiso decir.
		 *
		 * ★ GliGli esquiva el problema sin verlo: su dynarec es el de
		 * generacion antigua y manda LDL/LDR/SDL/SDR al interprete
		 * siempre (mupen64-360/.../Interpreter.h:57-61, con `// TODO` en
		 * la rama recompilada).  Aqui no hace falta renunciar a
		 * recompilarlas. */
		case MEM_LDL:
			address = addr & ~7;
			read_dword_in_memory();
			if(!address) break;
			if((addr & 7) == 0){
				r4300.gpr[value] = dword;
			} else {
				u32 shift = (addr & 7) * 8;
				u64 mask = 0xFFFFFFFFFFFFFFFFULL << shift;
				r4300.gpr[value] = (r4300.gpr[value] & ~mask) | ((dword << shift) & mask);
			}
			break;
		case MEM_LDR:
			address = addr & ~7;
			read_dword_in_memory();
			if(!address) break;
#if XBOX64_SONDA_LDR
			xbox64_ldr_previo = (unsigned long long)r4300.gpr[value];
#endif
			if((~addr & 7) == 0){
				r4300.gpr[value] = dword;
			} else {
				u32 shift = (~addr & 7) * 8;
				u64 mask = 0xFFFFFFFFFFFFFFFFULL >> shift;
				r4300.gpr[value] = (r4300.gpr[value] & ~mask) | ((dword >> shift) & mask);
			}
#if XBOX64_SONDA_LDR
			/* [Xbox360] ★★ SONDA DE LDR, CON ORACULO.
			 *
			 * Recalcula el resultado con una referencia INDEPENDIENTE --
			 * la formulacion del interprete (pure_interp.c:2789), que es
			 * el camino que SI funciona -- y lo compara con lo que acaba
			 * de escribir el codigo de arriba.
			 *
			 * ★ CON EL ARREGLO PUESTO, ESTA SONDA ES LA VERIFICACION: si
			 * enmudece, LDR ya calcula lo mismo que el interprete.  Antes
			 * del arreglo hablaba, y esa fue la prueba en vivo:
			 *
			 *     addr=8043F854 k=4 sh=24
			 *     prev=0000C40000000002  dw=8000002552A7E1BA
			 *     got =0000008000002552  ref=0000C48000002552
			 *
			 * ★ Y QUE ENMUDEZCA NO PUEDE CONFUNDIRSE CON QUE NO ESTE
			 * COMPILADA, que es la trampa de siempre.  La linea
			 * `[DYNA] literal:` de `start_new_block` se imprime SIEMPRE y
			 * dice que para este compilador `LL` sigue siendo con signo.
			 * O sea: si el arreglo NO estuviera puesto, esta sonda
			 * hablaria.  Literal con signo + sonda callada = arreglo
			 * puesto Y funcionando.
			 *
			 * Sin tope: en condiciones sanas no imprime nada. */
			{
				unsigned int  sh  = (~addr & 7) * 8;
				/* ★ LA MASCARA DE REFERENCIA SE CONSTRUYE BYTE A BYTE, a
				 * proposito: si se escribiera desplazando el mismo literal
				 * ancho que el codigo de arriba, las dos expresiones serian
				 * identicas y la sonda no podria detectar nada.  Asi sigue
				 * siendo un oraculo INDEPENDIENTE despues del arreglo.
				 * Guarda los (8 - sh/8) bytes bajos, que es lo que LDR
				 * escribe. */
				unsigned long long mref = 0;
				unsigned int b;
				unsigned long long ref;
				for(b = 0; b < (8u - sh/8u); ++b) mref |= 0xFFULL << (b*8);
				ref = (xbox64_ldr_previo & ~mref) | (dword >> sh);
				unsigned long long got  = (unsigned long long)r4300.gpr[value];
				if(got != ref){
					char msg[224];
					sprintf(msg,
					        "[DYNA] * LDR DISCREPA addr=%08X k=%u sh=%u | prev=%08X%08X "
					        "dw=%08X%08X | got=%08X%08X ref=%08X%08X",
					        addr, (unsigned int)(addr & 7), sh,
					        (unsigned int)(xbox64_ldr_previo >> 32), (unsigned int)xbox64_ldr_previo,
					        (unsigned int)(dword >> 32), (unsigned int)dword,
					        (unsigned int)(got >> 32),  (unsigned int)got,
					        (unsigned int)(ref >> 32),  (unsigned int)ref);
					DEBUG_print(msg, DBG_USBGECKO);
				}
			}
#endif
			break;
		/* [Xbox360] En cada escritura, primero el atajo de TLB (dm_tlb_rdram);
		 * la invalidacion de codigo de despues no cambia. */
		case MEM_SW:
			for(i = istart; i != iend; i += istep){
				unsigned char *p = dm_tlb_rdram(addr + i*4, 1);
				if(p){ *(unsigned long *)p = (unsigned long)r4300.gpr[value + i]; continue; }
				address = addr + i*4;
				word = r4300.gpr[value + i];
				write_word_in_memory();
			}
			if(needsCheck) invalidate_func_range(addr, count*4);
			break;
		case MEM_SH:
			for(i = istart; i != iend; i += istep){
				unsigned char *p = dm_tlb_rdram(addr + i*2, 1);
				if(p){ *(unsigned short *)p = (unsigned short)r4300.gpr[value + i]; continue; }
				address = addr + i*2;
				hword = r4300.gpr[value + i];
				write_hword_in_memory();
			}
			if(needsCheck) invalidate_func_range(addr, count*2);
			break;
		case MEM_SB:
			for(i = istart; i != iend; i += istep){
				unsigned char *p = dm_tlb_rdram(addr + i, 1);
				if(p){ *p = (unsigned char)r4300.gpr[value + i]; continue; }
				address = addr + i;
				byte = r4300.gpr[value + i];
				write_byte_in_memory();
			}
			if(needsCheck) invalidate_func_range(addr, count*1);
			break;
		case MEM_SD:
			for(i = istart; i != iend; i += istep){
				unsigned char *p = dm_tlb_rdram(addr + i*8, 1);
				if(p){
					const unsigned long long d = (unsigned long long)r4300.gpr[value + i];
					((unsigned long *)p)[0] = (unsigned long)(d >> 32);
					((unsigned long *)p)[1] = (unsigned long)(d & 0xFFFFFFFFu);
					continue;
				}
				address = addr + i*8;
				dword = r4300.gpr[value + i];
				write_dword_in_memory();
			}
			if(needsCheck) invalidate_func_range(addr, count*8);
			break;
		case MEM_SWC1:
			for(i = istart; i != iend; i += istep){
				unsigned char *p = dm_tlb_rdram(addr + i*4, 1);
				if(p){ *(unsigned long *)p = (unsigned long)*((long*)r4300.fpr_single[value + i*2]); continue; }
				address = addr + i*4;
				word = *((long*)r4300.fpr_single[value + i*2]);
				write_word_in_memory();
			}
			if(needsCheck) invalidate_func_range(addr, count*4);
			break;
		case MEM_SDC1:
			for(i = istart; i != iend; i += istep){
				unsigned char *p = dm_tlb_rdram(addr + i*8, 1);
				if(p){
					const unsigned long long d = (unsigned long long)*((long long*)r4300.fpr_double[value + i*2]);
					((unsigned long *)p)[0] = (unsigned long)(d >> 32);
					((unsigned long *)p)[1] = (unsigned long)(d & 0xFFFFFFFFu);
					continue;
				}
				address = addr + i*8;
				dword = *((long long*)r4300.fpr_double[value + i*2]);
				write_dword_in_memory();
			}
			if(needsCheck) invalidate_func_range(addr, count*8);
			break;
		case MEM_SC:
			if(r4300.llbit){
				address = addr;
				word = r4300.gpr[value];
				write_word_in_memory();
				invalidate_func(addr);
			}
			r4300.gpr[value] = !!r4300.llbit;
			r4300.llbit = 0;
			break;
		case MEM_SWL:
			address = addr & ~3;
			read_word_in_memory();
			if((addr & 3) == 0){
				word = r4300.gpr[value];
			} else {
				u32 shift = (addr & 3) * 8;
				u32 mask = 0xFFFFFFFF >> shift;
				word = (word & ~mask) | ((r4300.gpr[value] >> shift) & mask);
			}
			write_word_in_memory();
			if(needsCheck) invalidate_func(addr);
			break;
		case MEM_SWR:
			address = addr & ~3;
			read_word_in_memory();
			if((~addr & 3) == 0){
				word = r4300.gpr[value];
			} else {
				u32 shift = (~addr & 3) * 8;
				u32 mask = 0xFFFFFFFF << shift;
				word = (word & ~mask) | ((r4300.gpr[value] << shift) & mask);
			}
			write_word_in_memory();
			if(needsCheck) invalidate_func(addr);
			break;
		case MEM_SDL:
			address = addr & ~7;
			read_dword_in_memory();
			if((addr & 7) == 0){
				dword = r4300.gpr[value];
			} else {
				u32 shift = (addr & 7) * 8;
				u64 mask = 0xFFFFFFFFFFFFFFFFULL >> shift;
				dword = (dword & ~mask) | ((r4300.gpr[value] >> shift) & mask);
			}
			write_dword_in_memory();
			if(needsCheck) invalidate_func(addr);
			break;
		case MEM_SDR:
			address = addr & ~7;
			read_dword_in_memory();
			if((~addr & 7) == 0){
				dword = r4300.gpr[value];
			} else {
				u32 shift = (~addr & 7) * 8;
				u64 mask = 0xFFFFFFFFFFFFFFFFULL << shift;
				dword = (dword & ~mask) | ((r4300.gpr[value] << shift) & mask);
			}
			write_dword_in_memory();
			if(needsCheck) invalidate_func(addr);
			break;
		default:
			r4300.stop = 1;
			break;
	}
	r4300.delay_slot = 0;

	if(r4300.pc != pc) r4300.noCheckInterrupt = 1;
	//end_section(DYNAMEM_SECTION);
	return r4300.pc != pc ? r4300.pc : 0;
}

/* ==========================================================================
 * [Xbox360] ★★ LA AUTOPRUEBA DEL RECOMPILADOR CONTRA EL INTERPRETE.
 *
 * POR QUE (2026-10-01).  Goemon's Great Adventure se queda en negro al
 * empezar la aventura con el recompilador y avanza con el interprete.  La
 * biseccion de la fase 3 (DK64) exculpo cargas, almacenamientos y FP -- pero
 * "para DK64": lo que DK64 no ejecutaba no se probo nunca.  En vez de
 * bisecar juego a juego, esto prueba CADA instruccion sin memoria ni saltos
 * contra el ORACULO que ya funciona, el interprete, y escribe las que
 * difieren.  Sirve para cualquier juego que falle despues.
 *
 * COMO, por caso:
 *   1. Un bloque minimo en RDRAM (AP_DIR, KSEG0): [CTC1 de modo] instr,
 *      J AP_SALIDA, NOP.  Se recompila con `recompile_block` y se ejecuta UNA
 *      vez con `dyna_run`: exactamente como lo ejecuta `dynarec()`.
 *   2. Mismo estado de partida, y las mismas instrucciones con
 *      `decodeNInterpret`, el respaldo que el propio dynarec usa.
 *   3. Se comparan gpr[1..31], HI, LO, fpr_data[0..31] y fcr31.
 *
 * ★ LAS ENTRADAS NO SON CUALQUIERA.  Las operaciones de 32 bits (ADDU, SLL,
 * MULT...) solo reciben valores EXTENDIDOS EN SIGNO: con otros, el propio
 * R4300 no define el resultado y una diferencia no seria un fallo.  Las de 64
 * bits y las logicas (AND/OR/XOR/NOR/SLT, que en MIPS III son de 64) reciben
 * ademas mitades altas "raras".  Las divisiones no prueban divisor 0 ni
 * MIN/-1: el interprete las protege (o MSVC no), y medir eso seria ruido o
 * un cuelgue del propio oraculo (trampa 4 de FASE3.txt).
 *
 * ★ EN COMA FLOTANTE, FR=1 (Status 0x34000000, el de libultra) y la
 * palabra simple en la MITAD BAJA (`fpr_single` = +1 en big-endian).  Las
 * conversiones a entero se prueban en los cuatro modos de redondeo con un
 * CTC1 delante; la aritmetica y las comparaciones, en el modo 0 (en PowerPC
 * el interprete no aplica el modo: `set_rounding()` es (void)0,
 * macros.h:93).  Los destinos simple/palabra se comparan solo en sus 32 bits.
 *
 * ★ NO DEJA RASTRO: corre antes del primer `go()`, que de todas formas
 * reinicia la cache y los bloques (r4300.c:211-215); aun asi restaura r4300,
 * pone a cero la pagina, libera sus bloques, olvida el enlace pendiente y
 * devuelve el modo de redondeo del hilo.  El censo de `convert()` se
 * silencia mientras dura.
 * ========================================================================== */
#if defined(_XBOX)
#include <string.h>
#include <float.h>              /* _controlfp_s */
#include <ppcintrinsics.h>      /* __mftb */
#include "Register-Cache.h"     /* xbox64_mapeo_viejo */

#if XBOX64_AUTOPRUEBA

#define AP_DIR        0x80700000u          /* pagina de prueba, KSEG0 */
#define AP_SALIDA     (AP_DIR + 0x800u)
#define AP_RS   8                           /* t0 */
#define AP_RT   9                           /* t1 */
#define AP_RD  10                           /* t2 */
#define AP_RX  11                           /* t3: el modo para CTC1 */
#define AP_FS   2
#define AP_FT   4
#define AP_FD   6
#define AP_FALLOS_POR_INSTR  3              /* ejemplos escritos por instruccion */
#define AP_N(a) ((unsigned int)(sizeof(a) / sizeof((a)[0])))

#define AP_R(rs,rt,rd,sa,fn)  (((unsigned int)(rs)<<21)|((unsigned int)(rt)<<16)| \
                               ((unsigned int)(rd)<<11)|((unsigned int)(sa)<<6)|(unsigned int)(fn))
#define AP_I(op,rs,rt,imm)    (((unsigned int)(op)<<26)|((unsigned int)(rs)<<21)| \
                               ((unsigned int)(rt)<<16)|((unsigned int)(imm)&0xFFFFu))
#define AP_F(fmt,ft,fs,fd,fn) ((17u<<26)|((unsigned int)(fmt)<<21)|((unsigned int)(ft)<<16)| \
                               ((unsigned int)(fs)<<11)|((unsigned int)(fd)<<6)|(unsigned int)(fn))
#define AP_M(sub,rt,fs)       ((17u<<26)|((unsigned int)(sub)<<21)|((unsigned int)(rt)<<16)| \
                               ((unsigned int)(fs)<<11))
#define AP_J(dst)             ((2u<<26)|(((unsigned int)(dst)>>2)&0x03FFFFFFu))
#define AP_HI(x)              ((unsigned int)((unsigned long long)(x) >> 32))
#define AP_LO(x)              ((unsigned int)(unsigned long long)(x))

typedef struct { unsigned long long a, b; int modo; } ApEnt;

static R4300        s_apOrig, s_apBase, s_apIni, s_apDyna;
static const char  *s_apNombre;
static unsigned int s_apCasos, s_apFallos;
static unsigned int s_apTotCasos, s_apTotFallos, s_apInstr, s_apInstrMal;
static char         s_apMalas[640];

/* Extendidos en signo (validos para las de 32 bits). */
static const unsigned long long s_apC32[] = {
	0ULL, 1ULL, 0xFFFFFFFFFFFFFFFFULL, 0x000000007FFFFFFFULL,
	0xFFFFFFFF80000000ULL, 0x0000000012345678ULL, 0xFFFFFFFF9ABCDEF0ULL,
	31ULL, 32ULL, 0x0000000000007FFFULL, 0xFFFFFFFFFFFF8000ULL,
	0xFFFFFFFFFFFFFFFEULL };
/* Los de arriba y mitades altas que no son extension de signo. */
static const unsigned long long s_apX64[] = {
	0ULL, 1ULL, 0xFFFFFFFFFFFFFFFFULL, 0x000000007FFFFFFFULL,
	0xFFFFFFFF80000000ULL, 0x0000000012345678ULL, 0xFFFFFFFF9ABCDEF0ULL,
	31ULL, 32ULL, 0x0000000000007FFFULL, 0xFFFFFFFFFFFF8000ULL,
	0xFFFFFFFFFFFFFFFEULL,
	0x0000000080000000ULL, 0x00000000FFFFFFFFULL, 0x123456789ABCDEF0ULL,
	0x8000000000000000ULL, 0x7FFFFFFFFFFFFFFFULL, 0x0000000100000000ULL,
	0xFFFFFFFF00000000ULL, 0x00000000FFFF0000ULL, 0x00000001FFFFFFFFULL };
/* Desplazamientos variables: solo cuentan los 5/6 bits bajos. */
static const unsigned long long s_apShv[] = {
	0ULL, 1ULL, 4ULL, 31ULL, 32ULL, 33ULL, 63ULL, 64ULL, 0xFFFFFFFFFFFFFFFFULL };
static const unsigned int s_apImm[] = { 0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x1234, 0x00FF };
static const unsigned int s_apSa[]  = { 0, 1, 4, 15, 16, 31 };

/* Coma flotante: valores normales; los especiales (+-0, +-inf, NaN,
 * desnormal) se ponen por BITS en `ap_fp_init`, sin fiarse de literales. */
static const double s_apFv[] = {
	1.0, -1.0, 0.5, -0.5, 1.5, -1.5, 2.5, -2.5, 3.7, -3.7,
	123456.789, -98765.4321, 2147483647.0, 2147483648.0, -2147483648.0,
	-2147483649.0, 4294967296.5, 1.0e20, -1.0e20, 9.2233720368547758e18 };
static unsigned long long s_apFD[32];       /* dobles, en bits */
static unsigned int       s_apFS[32];       /* simples, en bits */
static unsigned int       s_apNF;
/* Los primeros AP_NFB se usan en binarias y comparaciones (el cuadrado). */
#define AP_NFB 12

static void ap_fp_init(void)
{
	unsigned int i, n = 0;
	union { double d; unsigned long long u; } cd;
	union { float f;  unsigned int u; } cs;
	/* Primero los especiales: entran en el subconjunto de las binarias. */
	s_apFD[n] = 0x0000000000000000ULL; s_apFS[n++] = 0x00000000u;   /* +0     */
	s_apFD[n] = 0x8000000000000000ULL; s_apFS[n++] = 0x80000000u;   /* -0     */
	s_apFD[n] = 0x7FF0000000000000ULL; s_apFS[n++] = 0x7F800000u;   /* +inf   */
	s_apFD[n] = 0xFFF0000000000000ULL; s_apFS[n++] = 0xFF800000u;   /* -inf   */
	s_apFD[n] = 0x7FF8000000000000ULL; s_apFS[n++] = 0x7FC00000u;   /* NaN    */
	s_apFD[n] = 0x0000000000000001ULL; s_apFS[n++] = 0x00000001u;   /* desnormal */
	for (i = 0; i < AP_N(s_apFv) && n < 32; i++) {
		cd.d = s_apFv[i];
		cs.f = (float)s_apFv[i];
		s_apFD[n] = cd.u;
		s_apFS[n++] = cs.u;
	}
	s_apNF = n;
}

static void ap_inicio(const char *nombre)
{
	s_apNombre = nombre;
	s_apCasos = s_apFallos = 0;
	/* ANTES de probarla: si una instruccion tumba la consola, esta es la
	 * ultima linea del log y dice cual. */
	sprintf(txtbuffer, "[AUTO] probando %s", nombre);
	DEBUG_print(txtbuffer, DBG_USBGECKO);
}

static void ap_fin(void)
{
	s_apInstr++;
	s_apTotCasos += s_apCasos;
	if (s_apFallos) {
		s_apInstrMal++;
		s_apTotFallos += s_apFallos;
		sprintf(txtbuffer, "[AUTO] * %s: %u de %u casos FALLAN", s_apNombre,
		        s_apFallos, s_apCasos);
		DEBUG_print(txtbuffer, DBG_USBGECKO);
		if (strlen(s_apMalas) + strlen(s_apNombre) + 2 < sizeof(s_apMalas)) {
			strcat(s_apMalas, " ");
			strcat(s_apMalas, s_apNombre);
		}
	}
}

/* Compila y ejecuta `cod[0..n-1]` + J AP_SALIDA + NOP con el recompilador.
 * Devuelve la direccion por la que salio el bloque. */
static unsigned int ap_dynarec(const unsigned int *cod, int n)
{
	PowerPC_block *b;
	PowerPC_func  *f;
	PowerPC_instr *code;
	unsigned long *mem = fast_mem_access(AP_DIR);
	int i;

	for (i = 0; i < n; i++) mem[i] = cod[i];
	mem[n]     = AP_J(AP_SALIDA);
	mem[n + 1] = 0;                          /* NOP del hueco de retardo */

	b = blocks[AP_DIR >> 12];
	if (!b) {
		b = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
		b->start_address = AP_DIR & ~0xFFFu;
		blocks[AP_DIR >> 12] = b;
		init_block(b);
	} else {
		invalidate_block(b, 0);              /* tira la funcion del caso anterior */
	}
	b->mips_code = (MIPS_instr*)fast_mem_access(AP_DIR & ~0xFFFu);
	f = recompile_block(b, AP_DIR);
	code = f->code_addr[(AP_DIR - f->start_addr) >> 2];
	return dyna_run(f, code);
}

static int ap_difiere(int palabra, char *d, int nd)
{
	int i;
	/* [Xbox360] `$zero` TAMBIEN SE COMPARA (casos con destino $zero, ver
	 * ap_r3z).  El oraculo se normaliza a la arquitectura: el interprete
	 * puro repone r0 en su bucle, pero `decodeNInterpret` no, asi que tras un
	 * `rd = $zero` deja ahi el resultado.  Del recompilador se exige 0: un
	 * valor en gpr[0] lo leeria luego cualquier instruccion interpretada. */
	r4300.gpr[0] = 0;
	for (i = 0; i < 32; i++)
		if (s_apDyna.gpr[i] != r4300.gpr[i]) {
			_snprintf(d, nd, "gpr%d recomp=%08X%08X interp=%08X%08X", i,
			          AP_HI(s_apDyna.gpr[i]), AP_LO(s_apDyna.gpr[i]),
			          AP_HI(r4300.gpr[i]), AP_LO(r4300.gpr[i]));
			return 1;
		}
	if (s_apDyna.hi != r4300.hi) {
		_snprintf(d, nd, "HI recomp=%08X%08X interp=%08X%08X",
		          AP_HI(s_apDyna.hi), AP_LO(s_apDyna.hi), AP_HI(r4300.hi), AP_LO(r4300.hi));
		return 1;
	}
	if (s_apDyna.lo != r4300.lo) {
		_snprintf(d, nd, "LO recomp=%08X%08X interp=%08X%08X",
		          AP_HI(s_apDyna.lo), AP_LO(s_apDyna.lo), AP_HI(r4300.lo), AP_LO(r4300.lo));
		return 1;
	}
	for (i = 0; i < 32; i++) {
		if (i == palabra) {
			/* destino simple/palabra: solo sus 32 bits (la mitad baja) */
			const unsigned int a = ((unsigned int*)&s_apDyna.fpr_data[i])[1];
			const unsigned int b = ((unsigned int*)&r4300.fpr_data[i])[1];
			if (a != b) {
				_snprintf(d, nd, "f%d recomp=%08X interp=%08X", i, a, b);
				return 1;
			}
		} else if (s_apDyna.fpr_data[i] != r4300.fpr_data[i]) {
			_snprintf(d, nd, "f%d recomp=%08X%08X interp=%08X%08X", i,
			          AP_HI(s_apDyna.fpr_data[i]), AP_LO(s_apDyna.fpr_data[i]),
			          AP_HI(r4300.fpr_data[i]), AP_LO(r4300.fpr_data[i]));
			return 1;
		}
	}
	if (s_apDyna.fcr31 != r4300.fcr31) {
		_snprintf(d, nd, "fcr31 recomp=%08X interp=%08X",
		          (unsigned int)s_apDyna.fcr31, (unsigned int)r4300.fcr31);
		return 1;
	}
	return 0;
}

/* Un caso: `s_apIni` ya preparado por quien llama.  `palabra` = FPR cuyo
 * destino es de 32 bits (o -1). */
static void ap_caso(const unsigned int *cod, int n, int palabra, const ApEnt *e)
{
	char d[128];
	unsigned int salida;
	int i, mal = 0;

	memcpy(&r4300, &s_apIni, sizeof(R4300));
	salida = ap_dynarec(cod, n);
	memcpy(&s_apDyna, &r4300, sizeof(R4300));

	memcpy(&r4300, &s_apIni, sizeof(R4300));
	for (i = 0; i < n; i++)
		decodeNInterpret(cod[i], AP_DIR + 4u * (unsigned int)i, 0);

	if (salida != AP_SALIDA) {
		_snprintf(d, sizeof(d), "el bloque salio por %08X, no por %08X", salida, AP_SALIDA);
		mal = 1;
	} else {
		mal = ap_difiere(palabra, d, sizeof(d));
	}
	d[sizeof(d) - 1] = 0;
	s_apCasos++;
	if (mal) {
		if (s_apFallos < AP_FALLOS_POR_INSTR) {
			sprintf(txtbuffer, "[AUTO]   %s instr=%08X a=%08X%08X b=%08X%08X modo=%d | %s",
			        s_apNombre, cod[n - 1], AP_HI(e->a), AP_LO(e->a),
			        AP_HI(e->b), AP_LO(e->b), e->modo, d);
			DEBUG_print(txtbuffer, DBG_USBGECKO);
		}
		s_apFallos++;
	}
}

static void ap_ini(void) { memcpy(&s_apIni, &s_apBase, sizeof(R4300)); }

/* ---- Enteros ------------------------------------------------------------ */

/* rd = rs op rt */
static void ap_r3(const char *nom, unsigned int fn, int x64)
{
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, j;
	ApEnt e;
	ap_inicio(nom);
	cod[0] = AP_R(AP_RS, AP_RT, AP_RD, 0, fn);
	for (i = 0; i < nv; i++)
		for (j = 0; j < nv; j++) {
			ap_ini();
			s_apIni.gpr[AP_RS] = (long long)v[i];
			s_apIni.gpr[AP_RT] = (long long)v[j];
			e.a = v[i]; e.b = v[j]; e.modo = -1;
			ap_caso(cod, 1, -1, &e);
		}
	ap_fin();
}

/* rd = rt << rs (desplazamiento variable) */
static void ap_shv(const char *nom, unsigned int fn, int x64)
{
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, j;
	ApEnt e;
	ap_inicio(nom);
	cod[0] = AP_R(AP_RS, AP_RT, AP_RD, 0, fn);
	for (i = 0; i < AP_N(s_apShv); i++)
		for (j = 0; j < nv; j++) {
			ap_ini();
			s_apIni.gpr[AP_RS] = (long long)s_apShv[i];
			s_apIni.gpr[AP_RT] = (long long)v[j];
			e.a = s_apShv[i]; e.b = v[j]; e.modo = -1;
			ap_caso(cod, 1, -1, &e);
		}
	ap_fin();
}

/* rd = rt << sa (desplazamiento inmediato) */
static void ap_sh(const char *nom, unsigned int fn, int x64)
{
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, j;
	ApEnt e;
	ap_inicio(nom);
	for (i = 0; i < AP_N(s_apSa); i++) {
		cod[0] = AP_R(0, AP_RT, AP_RD, s_apSa[i], fn);
		for (j = 0; j < nv; j++) {
			ap_ini();
			s_apIni.gpr[AP_RT] = (long long)v[j];
			e.a = s_apSa[i]; e.b = v[j]; e.modo = -1;
			ap_caso(cod, 1, -1, &e);
		}
	}
	ap_fin();
}

/* HI/LO = rs op rt.  `div`: 1 = de 32 bits, 2 = de 64 (sin divisor 0 ni
 * MIN/-1). */
static void ap_md(const char *nom, unsigned int fn, int x64, int div)
{
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, j;
	ApEnt e;
	ap_inicio(nom);
	cod[0] = AP_R(AP_RS, AP_RT, 0, 0, fn);
	for (i = 0; i < nv; i++)
		for (j = 0; j < nv; j++) {
			if (div == 1 && ((unsigned int)v[j] == 0 ||
			    ((unsigned int)v[i] == 0x80000000u && (unsigned int)v[j] == 0xFFFFFFFFu)))
				continue;
			if (div == 2 && (v[j] == 0 ||
			    (v[i] == 0x8000000000000000ULL && v[j] == 0xFFFFFFFFFFFFFFFFULL)))
				continue;
			ap_ini();
			s_apIni.gpr[AP_RS] = (long long)v[i];
			s_apIni.gpr[AP_RT] = (long long)v[j];
			e.a = v[i]; e.b = v[j]; e.modo = -1;
			ap_caso(cod, 1, -1, &e);
		}
	ap_fin();
}

/* rt = rs op imm */
static void ap_imm(const char *nom, unsigned int op, int x64)
{
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, j;
	ApEnt e;
	ap_inicio(nom);
	for (i = 0; i < AP_N(s_apImm); i++) {
		cod[0] = AP_I(op, AP_RS, AP_RT, s_apImm[i]);
		for (j = 0; j < nv; j++) {
			ap_ini();
			s_apIni.gpr[AP_RS] = (long long)v[j];
			e.a = v[j]; e.b = s_apImm[i]; e.modo = -1;
			ap_caso(cod, 1, -1, &e);
		}
	}
	ap_fin();
}

static void ap_lui(void)
{
	unsigned int cod[1], i;
	ApEnt e;
	ap_inicio("LUI");
	for (i = 0; i < AP_N(s_apImm); i++) {
		cod[0] = AP_I(15, 0, AP_RT, s_apImm[i]);
		ap_ini();
		s_apIni.gpr[AP_RT] = (long long)0x123456789ABCDEF0ULL;
		e.a = s_apImm[i]; e.b = 0; e.modo = -1;
		ap_caso(cod, 1, -1, &e);
	}
	ap_fin();
}

/* MFHI/MFLO (desde = 1) y MTHI/MTLO (desde = 0) */
static void ap_hilo(const char *nom, unsigned int fn, int desde)
{
	unsigned int cod[1], i;
	ApEnt e;
	ap_inicio(nom);
	cod[0] = desde ? AP_R(0, 0, AP_RD, 0, fn) : AP_R(AP_RS, 0, 0, 0, fn);
	for (i = 0; i < AP_N(s_apX64); i++) {
		ap_ini();
		s_apIni.hi = (long long)s_apX64[i];
		s_apIni.lo = (long long)~s_apX64[i];
		s_apIni.gpr[AP_RS] = (long long)s_apX64[i];
		e.a = s_apX64[i]; e.b = 0; e.modo = -1;
		ap_caso(cod, 1, -1, &e);
	}
	ap_fin();
}

/* ---- `$zero` como operando o destino, y divisor 0 (2026-10-12) ---------
 *
 * POR QUE.  Mischief Makers sacaba los bordes grises con el recompilador: sus
 * `DIV $zero, $s0` no emitian NADA ("Don't divide if they're using r0") con
 * LO/HI ya mapeados como nuevos, y quedaban con basura de registro PPC.  Las
 * pruebas de arriba usan SIEMPRE t0/t1/t2, asi que nunca pasan por esas ramas
 * especiales de `$zero`, que el recompilador tiene en muchas instrucciones.
 * Aqui cada familia se prueba con `$zero` en cada operando y como destino.
 *
 * ★ HI/LO y el destino NO empiezan a 0 (AP_HI_Z, AP_LO_Z, AP_RD_Z): un "lo
 * deja como estaba" contra "lo pone a 0" o contra basura tiene que verse.
 *
 * ★ EL DIVISOR 0 va aparte (".div0"), con el divisor en un registro que vale
 * 0 en ejecucion: el interprete deja LO/HI como estaban (pure_interp.c:265-
 * 388, las cuatro protegidas) y el R4300 no lo define, asi que un fallo ahi
 * es una diferencia con el oraculo, no por fuerza un error del juego.  En las
 * variantes con `$zero` se saltan los divisores que valen 0 por eso mismo.
 * MIN/-1 sigue fuera: el interprete NO lo protege y MSVC lanzaria (trampa 4
 * de FASE3.txt). */
#define AP_HI_Z  0x1111111122222222ULL
#define AP_LO_Z  0x3333333344444444ULL
#define AP_RD_Z  0x5555555566666666ULL

static char s_apNomZ[32];

static void ap_inicio_z(const char *nom, const char *suf)
{
	_snprintf(s_apNomZ, sizeof(s_apNomZ), "%s%s", nom, suf);
	s_apNomZ[sizeof(s_apNomZ) - 1] = 0;
	ap_inicio(s_apNomZ);
}

static void ap_ini_z(void)
{
	ap_ini();
	s_apIni.hi = (long long)AP_HI_Z;
	s_apIni.lo = (long long)AP_LO_Z;
	s_apIni.gpr[AP_RD] = (long long)AP_RD_Z;
}

/* rd = rs op rt: k = 0 rs=$zero, 1 rt=$zero, 2 los dos, 3 rd=$zero */
static void ap_r3z(const char *nom, unsigned int fn, int x64)
{
	static const char *const suf[4] = { ".rs0", ".rt0", ".rs0rt0", ".rd0" };
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, k;
	ApEnt e;
	for (k = 0; k < 4; k++) {
		const unsigned int rs = (k == 0 || k == 2) ? 0u : (unsigned int)AP_RS;
		const unsigned int rt = (k == 1 || k == 2) ? 0u : (unsigned int)AP_RT;
		const unsigned int rd = (k == 3) ? 0u : (unsigned int)AP_RD;
		ap_inicio_z(nom, suf[k]);
		cod[0] = AP_R(rs, rt, rd, 0, fn);
		for (i = 0; i < nv; i++) {
			ap_ini_z();
			s_apIni.gpr[AP_RS] = (long long)v[i];
			s_apIni.gpr[AP_RT] = (long long)v[nv - 1 - i];
			e.a = v[i]; e.b = v[nv - 1 - i]; e.modo = -1;
			ap_caso(cod, 1, -1, &e);
		}
		ap_fin();
	}
}

/* rd = rt << sa: k = 0 rt=$zero, 1 rd=$zero */
static void ap_shz(const char *nom, unsigned int fn, int x64)
{
	static const char *const suf[2] = { ".rt0", ".rd0" };
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, j, k;
	ApEnt e;
	for (k = 0; k < 2; k++) {
		ap_inicio_z(nom, suf[k]);
		for (i = 0; i < AP_N(s_apSa); i++) {
			cod[0] = AP_R(0, (k == 0) ? 0u : (unsigned int)AP_RT,
			              (k == 1) ? 0u : (unsigned int)AP_RD, s_apSa[i], fn);
			for (j = 0; j < nv; j++) {
				ap_ini_z();
				s_apIni.gpr[AP_RT] = (long long)v[j];
				e.a = s_apSa[i]; e.b = v[j]; e.modo = -1;
				ap_caso(cod, 1, -1, &e);
			}
		}
		ap_fin();
	}
}

/* rd = rt << rs: k = 0 rs=$zero, 1 rt=$zero, 2 rd=$zero */
static void ap_shvz(const char *nom, unsigned int fn, int x64)
{
	static const char *const suf[3] = { ".rs0", ".rt0", ".rd0" };
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, k;
	ApEnt e;
	for (k = 0; k < 3; k++) {
		ap_inicio_z(nom, suf[k]);
		cod[0] = AP_R((k == 0) ? 0u : (unsigned int)AP_RS, (k == 1) ? 0u : (unsigned int)AP_RT,
		              (k == 2) ? 0u : (unsigned int)AP_RD, 0, fn);
		for (i = 0; i < nv; i++) {
			ap_ini_z();
			s_apIni.gpr[AP_RS] = (long long)s_apShv[i % AP_N(s_apShv)];
			s_apIni.gpr[AP_RT] = (long long)v[i];
			e.a = s_apShv[i % AP_N(s_apShv)]; e.b = v[i]; e.modo = -1;
			ap_caso(cod, 1, -1, &e);
		}
		ap_fin();
	}
}

/* rt = rs op imm: k = 0 rs=$zero (el `li` de toda la vida), 1 rt=$zero */
static void ap_immz(const char *nom, unsigned int op, int x64)
{
	static const char *const suf[2] = { ".rs0", ".rt0" };
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, j, k;
	ApEnt e;
	for (k = 0; k < 2; k++) {
		ap_inicio_z(nom, suf[k]);
		for (i = 0; i < AP_N(s_apImm); i++) {
			cod[0] = AP_I(op, (k == 0) ? 0u : (unsigned int)AP_RS,
			              (k == 1) ? 0u : (unsigned int)AP_RT, s_apImm[i]);
			for (j = 0; j < nv; j++) {
				ap_ini_z();
				s_apIni.gpr[AP_RS] = (long long)v[j];
				s_apIni.gpr[AP_RT] = (long long)AP_RD_Z;
				e.a = v[j]; e.b = s_apImm[i]; e.modo = -1;
				ap_caso(cod, 1, -1, &e);
			}
		}
		ap_fin();
	}
}

static void ap_luiz(void)
{
	unsigned int cod[1], i;
	ApEnt e;
	ap_inicio("LUI.rt0");
	for (i = 0; i < AP_N(s_apImm); i++) {
		cod[0] = AP_I(15, 0, 0, s_apImm[i]);
		ap_ini_z();
		e.a = s_apImm[i]; e.b = 0; e.modo = -1;
		ap_caso(cod, 1, -1, &e);
	}
	ap_fin();
}

/* HI/LO = rs op rt con `$zero`: k = 0 rs, 1 rt, 2 los dos.  `div` como en
 * ap_md (1 = de 32 bits, 2 = de 64); los divisores que valen 0 se saltan
 * (son de ".div0"). */
static void ap_mdz(const char *nom, unsigned int fn, int x64, int div)
{
	static const char *const suf[3] = { ".rs0", ".rt0", ".rs0rt0" };
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i, k;
	ApEnt e;
	for (k = 0; k < 3; k++) {
		const unsigned int rs = (k == 0 || k == 2) ? 0u : (unsigned int)AP_RS;
		const unsigned int rt = (k == 1 || k == 2) ? 0u : (unsigned int)AP_RT;
		ap_inicio_z(nom, suf[k]);
		cod[0] = AP_R(rs, rt, 0, 0, fn);
		for (i = 0; i < nv; i++) {
			const unsigned long long b = v[nv - 1 - i];
			if (div == 1 && rt != 0 && (unsigned int)b == 0)
				continue;
			if (div == 2 && rt != 0 && b == 0)
				continue;
			ap_ini_z();
			s_apIni.gpr[AP_RS] = (long long)v[i];
			s_apIni.gpr[AP_RT] = (long long)b;
			e.a = v[i]; e.b = b; e.modo = -1;
			ap_caso(cod, 1, -1, &e);
		}
		ap_fin();
	}
}

/* Divisor 0 EN EJECUCION (registro t1 = 0), dividendo de la tabla. */
static void ap_md0(const char *nom, unsigned int fn, int x64)
{
	const unsigned long long *v = x64 ? s_apX64 : s_apC32;
	const unsigned int nv = x64 ? AP_N(s_apX64) : AP_N(s_apC32);
	unsigned int cod[1], i;
	ApEnt e;
	ap_inicio_z(nom, ".div0");
	cod[0] = AP_R(AP_RS, AP_RT, 0, 0, fn);
	for (i = 0; i < nv; i++) {
		ap_ini_z();
		s_apIni.gpr[AP_RS] = (long long)v[i];
		s_apIni.gpr[AP_RT] = 0;
		e.a = v[i]; e.b = 0; e.modo = -1;
		ap_caso(cod, 1, -1, &e);
	}
	ap_fin();
}

/* MFHI/MFLO a `$zero` (desde = 1) y MTHI/MTLO desde `$zero` (desde = 0). */
static void ap_hiloz(const char *nom, unsigned int fn, int desde)
{
	unsigned int cod[1], i;
	ApEnt e;
	ap_inicio_z(nom, desde ? ".rd0" : ".rs0");
	cod[0] = AP_R(0, 0, 0, 0, fn);
	for (i = 0; i < AP_N(s_apX64); i++) {
		ap_ini_z();
		s_apIni.hi = (long long)s_apX64[i];
		s_apIni.lo = (long long)~s_apX64[i];
		e.a = s_apX64[i]; e.b = 0; e.modo = -1;
		ap_caso(cod, 1, -1, &e);
	}
	ap_fin();
}

/* ---- El paso de 32 a 64 bits del asignador (log 46) ------------------- */

/* `mapRegister64` sobre un registro que ya estaba mapeado a 32 bits.  CFC1
 * t0,$0 lo mapea con `mapRegisterNewUnsigned` (sign = 0) y NADA reinicia
 * ese `sign` (ni flushRegisters ni invalidateRegisters): pasa al bloque
 * siguiente.  Alli ADDU carga t0 a 32 bits, limpio, y la segunda
 * instruccion lo pide a 64.  Con t0 negativo la mitad alta tiene que salir
 * de memoria; si se inventa con el `sign` viejo sale 0.  Cada valor son DOS
 * casos: el CFC1 de preparacion y la pareja.
 *
 * `viejo` = 1 compila con el comportamiento de antes (xbox64_mapeo_viejo):
 * esa pasada TIENE que fallar con los valores negativos, o la prueba no
 * distingue nada y el arreglo de Register-Cache.c no esta medido. */
static void ap_mapeo(const char *nom, unsigned int fn2, int viejo)
{
	unsigned int prep[1], cod[2], i;
	ApEnt e;
	xbox64_mapeo_viejo = viejo;
	ap_inicio(nom);
	prep[0] = AP_M(2, AP_RS, 0);                     /* CFC1  t0, $0      */
	cod[0]  = AP_R(AP_RS, AP_RT, AP_RD, 0, 33);      /* ADDU  t2, t0, t1  */
	cod[1]  = AP_R(AP_RS, AP_RT, 12, 0, fn2);        /* fn2   t4, t0, t1  */
	for (i = 0; i < AP_N(s_apC32); i++) {
		ap_ini();
		e.a = 0; e.b = 0; e.modo = -1;
		ap_caso(prep, 1, -1, &e);
		ap_ini();
		s_apIni.gpr[AP_RS] = (long long)s_apC32[i];
		s_apIni.gpr[AP_RT] = 1;
		e.a = s_apC32[i]; e.b = 1; e.modo = -1;
		ap_caso(cod, 2, -1, &e);
	}
	ap_fin();
	xbox64_mapeo_viejo = 0;
}

/* ---- Saltos condicionales (log 46) ------------------------------------- */

#define AP_TOMADO  (AP_DIR + 0x900u)        /* destino de los saltos, fuera del bloque */

/* Bxx con t0/t1 cargados A 32 BITS por un ADDU anterior del bloque (es lo
 * que activaba la trampa de genCmp64).  Aqui no hay oraculo de interprete
 * -- el salto ejecuta su hueco y mueve el PC --: la direccion por la que
 * TIENE que salir el bloque se calcula en C, a 64 bits.  Tomado = sale por
 * AP_TOMADO; no tomado = por la J a AP_SALIDA (o por AP_DIR+0xC si el
 * salto se interpreta, que devuelve pc+8).
 * tipo: 0 ==, 1 !=, 2 <=0, 3 >0, 4 <0, 5 >=0.  `viejo` = la trampa. */
static void ap_salto(const char *nom, unsigned int instr, int tipo, int viejo)
{
	unsigned int cod[3], i, j, salida, esperada;
	const unsigned int n1 = AP_N(s_apX64);
	const unsigned int n2 = (tipo < 2) ? AP_N(s_apX64) : 1u;
	long long a, b;
	int tomado, bien;
	xbox64_cmp_viejo = viejo;
	ap_inicio(nom);
	cod[0] = AP_R(AP_RS, AP_RT, AP_RD, 0, 33);                 /* ADDU t2, t0, t1 */
	cod[1] = instr | (((AP_TOMADO - (AP_DIR + 8u)) >> 2) & 0xFFFFu);
	cod[2] = 0;                                                /* NOP del hueco   */
	for (i = 0; i < n1; i++)
		for (j = 0; j < n2; j++) {
			a = (long long)s_apX64[i];
			b = (long long)s_apX64[j];
			switch (tipo) {
			case 0:  tomado = (a == b); break;
			case 1:  tomado = (a != b); break;
			case 2:  tomado = (a <= 0); break;
			case 3:  tomado = (a > 0);  break;
			case 4:  tomado = (a < 0);  break;
			default: tomado = (a >= 0); break;
			}
			ap_ini();
			s_apIni.gpr[AP_RS] = a;
			s_apIni.gpr[AP_RT] = b;
			memcpy(&r4300, &s_apIni, sizeof(R4300));
			salida = ap_dynarec(cod, 3);
			esperada = tomado ? AP_TOMADO : AP_SALIDA;
			bien = (salida == esperada) || (!tomado && salida == AP_DIR + 0xCu);
			s_apCasos++;
			if (!bien) {
				if (s_apFallos < AP_FALLOS_POR_INSTR) {
					sprintf(txtbuffer, "[AUTO]   %s instr=%08X a=%08X%08X b=%08X%08X | salio por %08X, tocaba %08X",
					        s_apNombre, cod[1], AP_HI(a), AP_LO(a), AP_HI(b), AP_LO(b),
					        salida, esperada);
					DEBUG_print(txtbuffer, DBG_USBGECKO);
				}
				s_apFallos++;
			}
		}
	ap_fin();
	xbox64_cmp_viejo = 0;
}

/* ---- Movimientos del COP1 --------------------------------------------- */

static void ap_cop1mov(const char *nom, unsigned int sub, unsigned int fs, int palabra)
{
	static const unsigned int fcr[] = { 0u, 1u, 2u, 3u, 0x01000000u, 0x00800003u, 0xFFFFFFFFu };
	unsigned int cod[1], i;
	ApEnt e;
	ap_inicio(nom);
	cod[0] = AP_M(sub, AP_RT, fs);
	for (i = 0; i < AP_N(s_apX64); i++) {
		ap_ini();
		s_apIni.gpr[AP_RT]       = (long long)s_apX64[i];
		s_apIni.fpr_data[AP_FS]  = (long long)s_apX64[i];
		s_apIni.fcr31            = (long)fcr[i % AP_N(fcr)];
		e.a = s_apX64[i]; e.b = fcr[i % AP_N(fcr)]; e.modo = -1;
		ap_caso(cod, 1, palabra, &e);
	}
	ap_fin();
}

/* ---- Coma flotante ----------------------------------------------------- */

static void ap_pon_fp(int reg, int dbl, unsigned int k)
{
	if (dbl) {
		s_apIni.fpr_data[reg] = (long long)s_apFD[k];
	} else {
		((unsigned int*)&s_apIni.fpr_data[reg])[0] = 0;
		((unsigned int*)&s_apIni.fpr_data[reg])[1] = s_apFS[k];
	}
}

/* fd = op(fs), en los cuatro modos de redondeo.  `res`: 0 = 64 bits
 * (D/L), 1 = 32 bits (S/W). */
static void ap_fp1(const char *nom, unsigned int fmt, unsigned int fn, int res)
{
	unsigned int cod[2], i, modo;
	const int dbl = (fmt == 17);
	ApEnt e;
	ap_inicio(nom);
	cod[0] = AP_M(6, AP_RX, 31);             /* CTC1 t3, $31 */
	cod[1] = AP_F(fmt, 0, AP_FS, AP_FD, fn);
	for (modo = 0; modo < 4; modo++)
		for (i = 0; i < s_apNF; i++) {
			ap_ini();
			s_apIni.gpr[AP_RX] = (long long)modo;
			ap_pon_fp(AP_FS, dbl, i);
			e.a = dbl ? s_apFD[i] : s_apFS[i]; e.b = 0; e.modo = (int)modo;
			ap_caso(cod, 2, res ? AP_FD : -1, &e);
		}
	ap_fin();
}

/* fd = fs op ft (modo 0) */
static void ap_fp2(const char *nom, unsigned int fmt, unsigned int fn)
{
	unsigned int cod[1], i, j;
	const int dbl = (fmt == 17);
	ApEnt e;
	ap_inicio(nom);
	cod[0] = AP_F(fmt, AP_FT, AP_FS, AP_FD, fn);
	for (i = 0; i < AP_NFB && i < s_apNF; i++)
		for (j = 0; j < AP_NFB && j < s_apNF; j++) {
			ap_ini();
			ap_pon_fp(AP_FS, dbl, i);
			ap_pon_fp(AP_FT, dbl, j);
			e.a = dbl ? s_apFD[i] : s_apFS[i];
			e.b = dbl ? s_apFD[j] : s_apFS[j]; e.modo = 0;
			ap_caso(cod, 1, dbl ? -1 : AP_FD, &e);
		}
	ap_fin();
}

/* C.cond fs, ft: el resultado es el bit C de fcr31. */
static void ap_fpc(const char *nom, unsigned int fmt, unsigned int fn)
{
	unsigned int cod[1], i, j;
	const int dbl = (fmt == 17);
	ApEnt e;
	ap_inicio(nom);
	cod[0] = AP_F(fmt, AP_FT, AP_FS, 0, fn);
	for (i = 0; i < AP_NFB && i < s_apNF; i++)
		for (j = 0; j < AP_NFB && j < s_apNF; j++) {
			ap_ini();
			ap_pon_fp(AP_FS, dbl, i);
			ap_pon_fp(AP_FT, dbl, j);
			s_apIni.fcr31 = (long)(((i + j) & 1) ? 0x00800000u : 0u); /* C de partida */
			e.a = dbl ? s_apFD[i] : s_apFS[i];
			e.b = dbl ? s_apFD[j] : s_apFS[j]; e.modo = 0;
			ap_caso(cod, 1, -1, &e);
		}
	ap_fin();
}

/* CVT.S/D desde W (fmt 20) o L (fmt 21): el origen es un entero en bits. */
static void ap_cvti(const char *nom, unsigned int fmt, unsigned int fn, int res)
{
	unsigned int cod[1], i;
	ApEnt e;
	ap_inicio(nom);
	cod[0] = AP_F(fmt, 0, AP_FS, AP_FD, fn);
	for (i = 0; i < AP_N(s_apX64); i++) {
		ap_ini();
		if (fmt == 20) {
			((unsigned int*)&s_apIni.fpr_data[AP_FS])[0] = 0;
			((unsigned int*)&s_apIni.fpr_data[AP_FS])[1] = AP_LO(s_apX64[i]);
		} else {
			s_apIni.fpr_data[AP_FS] = (long long)s_apX64[i];
		}
		e.a = s_apX64[i]; e.b = 0; e.modo = 0;
		ap_caso(cod, 1, res ? AP_FD : -1, &e);
	}
	ap_fin();
}

static void ap_coma_flotante(unsigned int fmt)
{
	const int s = (fmt == 16);
	/* res: 1 = destino de 32 bits */
	ap_fp1(s ? "ABS.S"     : "ABS.D",     fmt, 5,  s);
	ap_fp1(s ? "MOV.S"     : "MOV.D",     fmt, 6,  s);
	ap_fp1(s ? "NEG.S"     : "NEG.D",     fmt, 7,  s);
	ap_fp1(s ? "SQRT.S"    : "SQRT.D",    fmt, 4,  s);
	ap_fp1(s ? "ROUND.L.S" : "ROUND.L.D", fmt, 8,  0);
	ap_fp1(s ? "TRUNC.L.S" : "TRUNC.L.D", fmt, 9,  0);
	ap_fp1(s ? "CEIL.L.S"  : "CEIL.L.D",  fmt, 10, 0);
	ap_fp1(s ? "FLOOR.L.S" : "FLOOR.L.D", fmt, 11, 0);
	ap_fp1(s ? "ROUND.W.S" : "ROUND.W.D", fmt, 12, 1);
	ap_fp1(s ? "TRUNC.W.S" : "TRUNC.W.D", fmt, 13, 1);
	ap_fp1(s ? "CEIL.W.S"  : "CEIL.W.D",  fmt, 14, 1);
	ap_fp1(s ? "FLOOR.W.S" : "FLOOR.W.D", fmt, 15, 1);
	if (s) ap_fp1("CVT.D.S", fmt, 33, 0);
	else   ap_fp1("CVT.S.D", fmt, 32, 1);
	ap_fp1(s ? "CVT.W.S"   : "CVT.W.D",   fmt, 36, 1);
	ap_fp1(s ? "CVT.L.S"   : "CVT.L.D",   fmt, 37, 0);
	ap_fp2(s ? "ADD.S" : "ADD.D", fmt, 0);
	ap_fp2(s ? "SUB.S" : "SUB.D", fmt, 1);
	ap_fp2(s ? "MUL.S" : "MUL.D", fmt, 2);
	ap_fp2(s ? "DIV.S" : "DIV.D", fmt, 3);
	{
		static const char *const cs[16] = {
			"C.F", "C.UN", "C.EQ", "C.UEQ", "C.OLT", "C.ULT", "C.OLE", "C.ULE",
			"C.SF", "C.NGLE", "C.SEQ", "C.NGL", "C.LT", "C.NGE", "C.LE", "C.NGT" };
		static char nombres[2][16][12];
		unsigned int c;
		for (c = 0; c < 16; c++) {
			char *n = nombres[s ? 0 : 1][c];
			_snprintf(n, 12, "%s.%s", cs[c], s ? "S" : "D");
			n[11] = 0;
			ap_fpc(n, fmt, 48 + c);
		}
	}
}

void xbox64_autoprueba_dynarec(void)
{
	static int s_hecha = 0;
	unsigned int cw = 0, basura = 0;
	unsigned long long t0;

	if (s_hecha) return;                     /* una vez por proceso */
	s_hecha = 1;

	t0 = __mftb();
	sprintf(txtbuffer, "[AUTO] autoprueba del recompilador contra el interprete: empieza");
	DEBUG_print(txtbuffer, DBG_USBGECKO);

	_controlfp_s(&cw, 0, 0);
	xbox64_censo_silencio = 1;
	memcpy(&s_apOrig, &r4300, sizeof(R4300));
	RecompCache_Init();                      /* el monton de la cache, si aun no existe */
	ap_fp_init();
	s_apInstr = s_apInstrMal = s_apTotCasos = s_apTotFallos = 0;
	s_apMalas[0] = 0;

	/* El estado limpio de partida de cada caso. */
	memset(r4300.gpr, 0, sizeof(r4300.gpr));
	memset(r4300.fpr_data, 0, sizeof(r4300.fpr_data));
	r4300.hi = r4300.lo = 0;
	r4300.reg_cop0[12] = 0x34000000;         /* Status: CU1 | CU0 | FR */
	set_fpr_pointers(r4300.reg_cop0[12]);
	r4300.fcr31 = 0;
	r4300.pc = r4300.last_pc = AP_DIR;
	r4300.delay_slot = 0;
	r4300.stop = 0;
	r4300.noCheckInterrupt = 0;
	r4300.next_interrupt = (unsigned int)r4300.reg_cop0[9] + 0x40000000u;
	memcpy(&s_apBase, &r4300, sizeof(R4300));

	/* ---- enteros de 32 bits (entradas extendidas en signo) ---- */
	ap_r3("ADD",  32, 0);  ap_r3("ADDU", 33, 0);
	ap_r3("SUB",  34, 0);  ap_r3("SUBU", 35, 0);
	ap_sh("SLL", 0, 0);    ap_sh("SRL", 2, 0);    ap_sh("SRA", 3, 0);
	ap_shv("SLLV", 4, 0);  ap_shv("SRLV", 6, 0);  ap_shv("SRAV", 7, 0);
	ap_md("MULT", 24, 0, 0); ap_md("MULTU", 25, 0, 0);
	ap_md("DIV",  26, 0, 1); ap_md("DIVU",  27, 0, 1);
	ap_imm("ADDI", 8, 0);  ap_imm("ADDIU", 9, 0);
	ap_lui();
	/* ---- de 64 bits y logicas (entradas con mitad alta rara) ---- */
	ap_r3("AND", 36, 1);   ap_r3("OR",  37, 1);
	ap_r3("XOR", 38, 1);   ap_r3("NOR", 39, 1);
	ap_r3("SLT", 42, 1);   ap_r3("SLTU", 43, 1);
	ap_r3("DADD", 44, 1);  ap_r3("DADDU", 45, 1);
	ap_r3("DSUB", 46, 1);  ap_r3("DSUBU", 47, 1);
	ap_shv("DSLLV", 20, 1); ap_shv("DSRLV", 22, 1); ap_shv("DSRAV", 23, 1);
	ap_sh("DSLL", 56, 1);  ap_sh("DSRL", 58, 1);  ap_sh("DSRA", 59, 1);
	ap_sh("DSLL32", 60, 1); ap_sh("DSRL32", 62, 1); ap_sh("DSRA32", 63, 1);
	ap_md("DMULT", 28, 1, 0); ap_md("DMULTU", 29, 1, 0);
	ap_md("DDIV",  30, 1, 2); ap_md("DDIVU",  31, 1, 2);
	ap_imm("SLTI", 10, 1); ap_imm("SLTIU", 11, 1);
	ap_imm("ANDI", 12, 1); ap_imm("ORI", 13, 1); ap_imm("XORI", 14, 1);
	ap_imm("DADDI", 24, 1); ap_imm("DADDIU", 25, 1);
	ap_hilo("MFHI", 16, 1); ap_hilo("MFLO", 18, 1);
	ap_hilo("MTHI", 17, 0); ap_hilo("MTLO", 19, 0);
	/* ---- `$zero` como operando o destino (Mischief Makers, DIV $zero) ---- */
	ap_r3z("ADD",  32, 0);  ap_r3z("ADDU", 33, 0);
	ap_r3z("SUB",  34, 0);  ap_r3z("SUBU", 35, 0);
	ap_r3z("AND", 36, 1);   ap_r3z("OR",  37, 1);
	ap_r3z("XOR", 38, 1);   ap_r3z("NOR", 39, 1);
	ap_r3z("SLT", 42, 1);   ap_r3z("SLTU", 43, 1);
	ap_r3z("DADD", 44, 1);  ap_r3z("DADDU", 45, 1);
	ap_r3z("DSUB", 46, 1);  ap_r3z("DSUBU", 47, 1);
	ap_shz("SLL", 0, 0);    ap_shz("SRL", 2, 0);    ap_shz("SRA", 3, 0);
	ap_shz("DSLL", 56, 1);  ap_shz("DSRL", 58, 1);  ap_shz("DSRA", 59, 1);
	ap_shz("DSLL32", 60, 1); ap_shz("DSRL32", 62, 1); ap_shz("DSRA32", 63, 1);
	ap_shvz("SLLV", 4, 0);  ap_shvz("SRLV", 6, 0);  ap_shvz("SRAV", 7, 0);
	ap_shvz("DSLLV", 20, 1); ap_shvz("DSRLV", 22, 1); ap_shvz("DSRAV", 23, 1);
	ap_immz("ADDI", 8, 0);  ap_immz("ADDIU", 9, 0);
	ap_immz("SLTI", 10, 1); ap_immz("SLTIU", 11, 1);
	ap_immz("ANDI", 12, 1); ap_immz("ORI", 13, 1); ap_immz("XORI", 14, 1);
	ap_immz("DADDI", 24, 1); ap_immz("DADDIU", 25, 1);
	ap_luiz();
	ap_mdz("MULT", 24, 0, 0); ap_mdz("MULTU", 25, 0, 0);
	ap_mdz("DIV",  26, 0, 1); ap_mdz("DIVU",  27, 0, 1);
	ap_mdz("DMULT", 28, 1, 0); ap_mdz("DMULTU", 29, 1, 0);
	ap_mdz("DDIV",  30, 1, 2); ap_mdz("DDIVU",  31, 1, 2);
	ap_hiloz("MFHI", 16, 1); ap_hiloz("MFLO", 18, 1);
	ap_hiloz("MTHI", 17, 0); ap_hiloz("MTLO", 19, 0);
	/* ---- divisor 0 en ejecucion: diferencia con el ORACULO (el R4300 no lo
	 * define); si sale, se decide aparte si importa ---- */
	ap_md0("DIV",  26, 0); ap_md0("DIVU",  27, 0);
	ap_md0("DDIV", 30, 1); ap_md0("DDIVU", 31, 1);
	/* ---- el paso de 32 a 64 bits del asignador ---- */
	/* ★ las dos ".viejo" TIENEN que salir en FALLAN; las otras dos no */
	ap_mapeo("MAPEO.DADDU.viejo", 45, 1);
	ap_mapeo("MAPEO.SLT.viejo",   42, 1);
	ap_mapeo("MAPEO.DADDU", 45, 0);
	ap_mapeo("MAPEO.SLT",   42, 0);
	/* ---- saltos condicionales; las ".viejo" TIENEN que fallar ---- */
	ap_salto("BEQ.viejo",  AP_I(4, AP_RS, AP_RT, 0), 0, 1);
	ap_salto("BLTZ.viejo", AP_I(1, AP_RS, 0, 0),     4, 1);
	ap_salto("BEQ",   AP_I(4,  AP_RS, AP_RT, 0), 0, 0);
	ap_salto("BNE",   AP_I(5,  AP_RS, AP_RT, 0), 1, 0);
	ap_salto("BLEZ",  AP_I(6,  AP_RS, 0, 0),     2, 0);
	ap_salto("BGTZ",  AP_I(7,  AP_RS, 0, 0),     3, 0);
	ap_salto("BLTZ",  AP_I(1,  AP_RS, 0, 0),     4, 0);
	ap_salto("BGEZ",  AP_I(1,  AP_RS, 1, 0),     5, 0);
	ap_salto("BEQL",  AP_I(20, AP_RS, AP_RT, 0), 0, 0);
	ap_salto("BNEL",  AP_I(21, AP_RS, AP_RT, 0), 1, 0);
	ap_salto("BLEZL", AP_I(22, AP_RS, 0, 0),     2, 0);
	ap_salto("BGTZL", AP_I(23, AP_RS, 0, 0),     3, 0);
	ap_salto("BLTZL", AP_I(1,  AP_RS, 2, 0),     4, 0);
	ap_salto("BGEZL", AP_I(1,  AP_RS, 3, 0),     5, 0);
	/* ---- movimientos del COP1 ---- */
	ap_cop1mov("MFC1",  0, AP_FS, -1);
	ap_cop1mov("DMFC1", 1, AP_FS, -1);
	ap_cop1mov("CFC1.31", 2, 31, -1);
	ap_cop1mov("CFC1.0",  2, 0,  -1);
	ap_cop1mov("MTC1",  4, AP_FS, AP_FS);
	ap_cop1mov("DMTC1", 5, AP_FS, -1);
	ap_cop1mov("CTC1",  6, 31, -1);
	/* ---- coma flotante ---- */
	ap_coma_flotante(16);
	ap_coma_flotante(17);
	ap_cvti("CVT.S.W", 20, 32, 1); ap_cvti("CVT.D.W", 20, 33, 0);
	ap_cvti("CVT.S.L", 21, 32, 1); ap_cvti("CVT.D.L", 21, 33, 0);

	/* ---- y dejarlo todo como estaba ---- */
	{
		const unsigned int pag = AP_DIR >> 12;
		const unsigned int esp = (AP_DIR ^ 0x20000000u) >> 12;   /* el espejo que crea init_block */
		if (blocks[pag]) { deinit_block(blocks[pag]); free(blocks[pag]); blocks[pag] = NULL; }
		if (blocks[esp]) { deinit_block(blocks[esp]); free(blocks[esp]); blocks[esp] = NULL; }
		memset(fast_mem_access(AP_DIR & ~0xFFFu), 0, 0x1000);
	}
	clear_freed_funcs();
	dynarec_olvida_enlace();
	memcpy(&r4300, &s_apOrig, sizeof(R4300));
	set_fpr_pointers(r4300.reg_cop0[12]);
	xbox64_censo_silencio = 0;
	_controlfp_s(&basura, cw, _MCW_RC);

	sprintf(txtbuffer, "[AUTO] FIN: %u instrucciones, %u casos, %u con fallos (%u casos) en %u ms",
	        s_apInstr, s_apTotCasos, s_apInstrMal, s_apTotFallos,
	        (unsigned int)((__mftb() - t0) / 50000ULL));
	DEBUG_print(txtbuffer, DBG_USBGECKO);
	if (s_apInstrMal) {
		_snprintf(txtbuffer, 256, "[AUTO] FALLAN:%s", s_apMalas);
		txtbuffer[255] = 0;
		DEBUG_print(txtbuffer, DBG_USBGECKO);
	}
}

#else  /* !XBOX64_AUTOPRUEBA */
void xbox64_autoprueba_dynarec(void) { }
#endif /* XBOX64_AUTOPRUEBA */
#endif /* _XBOX */

