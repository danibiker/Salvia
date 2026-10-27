/*
 * xbox/dynarec_ausente.c  --  los simbolos del dynarec sin dynarec (fase 1
 * en Xbox; Windows, que no lo tiene).  Eran cinco; la lista entera esta al
 * final del fichero.
 *
 * `r4300/ppc/*.c` no entra en el enlace hasta la fase 3, pero el nucleo
 * REFERENCIA cinco de sus simbolos.  Censado sobre los .c de la fase 1:
 *
 *     dynarec            r4300.c:209   rama `else` de go()
 *     RecompCache_Init   r4300.c:205   idem
 *     init_block         r4300.c:183   dentro de init_blocks()
 *     deinit_block       r4300.c:465   dentro de cpu_deinit(), rama
 *                                      PPC_DYNAREC
 *     invalidate_func    pure_interp.c:70 (macro check_memory)
 *                                      y dma.c:182-183
 *
 * ★ OJO: PPC_DYNAREC SI ESTA DEFINIDO EN ESTE PROYECTO, Y NO ES UNA
 * CONTRADICCION.
 *
 * La rama `#ifndef PPC_DYNAREC` de Wii64 usa el tipo `precomp_block`, que
 * en este arbol NO SE DECLARA EN NINGUN SITIO: aparece solo en las tres
 * lineas que lo usan (r4300.h:94, r4300.c:70, r4300.c:183).  Wii64 borro el
 * recompilador x86 de mupen64 y con el su typedef, asi que esa rama es
 * codigo muerto que nunca ha compilado.  Lo confirma que `r4300.c:40`
 * incluya `ppc/Wrappers.h` SIN guarda: el arbol ya da PPC_DYNAREC por
 * hecho.
 *
 * Lo que decide si el dynarec SE EJECUTA no es el define, es `dynacore`
 * (r4300.c:196), que xbox64_init pone a DYNACORE_PURE_INTERP.
 *
 * ★ NINGUNO SE LLEGA A LLAMAR con dynacore == DYNACORE_PURE_INTERP:
 *
 *   - `dynarec` y `RecompCache_Init` viven en la rama de go() que solo se
 *     toma cuando dynacore != 2 (r4300.c:196).
 *   - `init_blocks()`, que llama a `init_block`, solo se llama desde esa
 *     misma rama.
 *   - `deinit_block` esta detras de `if(dynacore != 2 && !cpu_inited)`
 *     (r4300.c:462).
 *   - `invalidate_func` esta detras de `if(dynacore)` en pure_interp.c:70 y
 *     de `if(!interpcore)` en dma.c:176.  Y go() pone dynacore a 0 e
 *     interpcore a 1 mientras corre el interprete puro (r4300.c:197-199),
 *     asi que las dos condiciones son falsas.
 *
 * O sea que son referencias de ENLACE, no de ejecucion.  Por eso no se
 * guardan con `#if` dentro de Wii64 -- que serian cinco ediciones sobre
 * codigo que en la fase 3 hay que dejar como estaba -- sino con este
 * fichero, que en la fase 3 se apaga entero.
 *
 * ★ Y POR ESO CADA UNO GRITA.  Si alguno se ejecuta, el razonamiento de
 * arriba es falso y hay que saberlo en el acto: un stub mudo convertiria un
 * error de razonamiento en un cuelgue sin causa.
 *
 * C89.
 */

#include "xbox64_fase.h"

#if !XBOX64_DYNAREC

/* Las firmas salen de las cabeceras de verdad, no se escriben a mano: con
 * PPC_DYNAREC definido, r4300.h arrastra ppc/Recompile.h, que declara
 * init_block y deinit_block con `PowerPC_block*`.  Un stub con `void*`
 * enlazaria igual (C no decora por tipos) pero mentiria en la fase 3, que
 * es cuando alguien lo leera. */
#include "../source/r4300/r4300.h"          /* -> ppc/Recompile.h        */
#include "../source/r4300/ppc/Wrappers.h"   /* dynarec, invalidate_func  */
#include "../source/r4300/Recomp-Cache.h"   /* RecompCache_Init          */
#include "../source/gui/DEBUG.h"

void dynarec(unsigned int address)
{
	(void)address;
	DEBUG_print("dynarec_ausente: se ha LLAMADO a dynarec()", DBG_USBGECKO);
}

void RecompCache_Init(void)
{
	DEBUG_print("dynarec_ausente: se ha LLAMADO a RecompCache_Init()", DBG_USBGECKO);
}

void init_block(PowerPC_block *ppc_block)
{
	(void)ppc_block;
	DEBUG_print("dynarec_ausente: se ha LLAMADO a init_block()", DBG_USBGECKO);
}

void deinit_block(PowerPC_block *ppc_block)
{
	(void)ppc_block;
	DEBUG_print("dynarec_ausente: se ha LLAMADO a deinit_block()", DBG_USBGECKO);
}

void invalidate_func(unsigned int addr)
{
	(void)addr;
	DEBUG_print("dynarec_ausente: se ha LLAMADO a invalidate_func()", DBG_USBGECKO);
}

/* [Xbox360] Las variantes de TLB (ppc/Wrappers.h): cpu_deinit la llama en la
 * misma rama que deinit_block. */
void dynarec_tlbvar_vaciar(void)
{
	DEBUG_print("dynarec_ausente: se ha LLAMADO a dynarec_tlbvar_vaciar()", DBG_USBGECKO);
}

/* [Windows, logs (6)] Los dos que faltaban del censo, de la fase 3 en adelante
 * (en la Xbox nunca se echaron en falta: alli ppc/ esta en el enlace).
 *   dynarec_olvida_enlace  r4300.c:217 (rama `else` de go(), la del dynarec) y
 *                          savestate_n64.c:341 (tras `if (dynacore != 1) return;`)
 *   invalidate_block       savestate_n64.c:334 (idem)
 * Con el interprete puro ninguno se alcanza, igual que los de arriba. */
void dynarec_olvida_enlace(void)
{
	DEBUG_print("dynarec_ausente: se ha LLAMADO a dynarec_olvida_enlace()", DBG_USBGECKO);
}

void invalidate_block(PowerPC_block *ppc_block, int is_deinit)
{
	(void)ppc_block;
	(void)is_deinit;
	DEBUG_print("dynarec_ausente: se ha LLAMADO a invalidate_block()", DBG_USBGECKO);
}

#endif /* !XBOX64_DYNAREC */
