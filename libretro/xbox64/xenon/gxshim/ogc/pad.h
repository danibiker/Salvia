/*
 * gxshim/ogc/pad.h  --  vacio a proposito.
 *
 * QUIEN LO PIDE.  `source/r4300/pure_interp.c:184`, y es un resto:
 *
 *     #define DUMP_ON_BREAK
 *     #ifdef DUMP_ON_BREAK
 *     #include <ogc/pad.h>
 *     #endif
 *     static void BREAK(){
 *     #ifdef DUMP_ON_BREAK
 *     #ifdef DEBUGON
 *         _break(); return;
 *     #endif
 *         ...todo lo demas esta comentado...
 *
 * `DUMP_ON_BREAK` esta definido justo encima, o sea que el include SIEMPRE
 * entra; pero lo unico que se usaba de la cabecera es `_break()`, que a su
 * vez esta bajo `DEBUGON`, que no se define ni en el build de Wii.  O sea:
 * la cabecera entra y no se usa nada de ella.
 *
 * Por eso esto es un fichero vacio y no un shim del mando de GameCube.  El
 * mando de verdad llega por libretro en la fase 5, y no por aqui.
 *
 * ★ La alternativa era editar pure_interp.c para envolver el include.  Se
 * prefiere el fichero vacio: es una edicion menos sobre el interprete, que
 * es justo la pieza que la fase 1 quiere tener IDENTICA a Wii64 para que,
 * si una ROM no arranca, la causa no pueda ser una edicion nuestra.
 */

#ifndef _XBOX64_GXSHIM_OGC_PAD_H_
#define _XBOX64_GXSHIM_OGC_PAD_H_

#endif /* _XBOX64_GXSHIM_OGC_PAD_H_ */
