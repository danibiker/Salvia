/*
 * gln64_hacks.h  --  los arreglos por juego de glN64 como core options.
 *
 * [Xbox360] ★ [2026-10-30] LA CATEGORIA "GL" DE LAS OPCIONES.
 *
 * Los 28 `hack_*` de source/glN64_GX/Config.h los enciende RSP.cpp
 * (_RSP_SetGameHacks) por el nombre interno de la ROM.  Cada uno tiene
 * ademas una core option con tres valores:
 *
 *   auto         lo que diga RSP.cpp para este juego (por defecto)
 *   activado     encendido aunque el juego no lo tenga
 *   desactivado  apagado aunque el juego lo tenga
 *
 * Sirve para probar un arreglo en un juego que no esta identificado (otra
 * region, una traduccion, un hack de ROM con otro nombre interno) y para
 * quitarlo si molesta.  Se aplica EN CALIENTE: el valor efectivo es
 * (los del juego & ~desactivados) | activados, y se recalcula al cambiar
 * una opcion y al cargar cada ROM.
 *
 * La tabla (clave, bit y textos en ingles y espanol) vive en
 * xenon/gln64_config.cpp, donde se ven los `hack_*`; libretro_core.cpp la
 * recorre para publicar y leer las opciones.
 */

#ifndef _XENON_GLN64_HACKS_H_
#define _XENON_GLN64_HACKS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Gln64HackOpcion
{
	const char  *clave;      /* clave de la core option ("xbox64_gl_...") */
	unsigned int bit;        /* el hack_* de glN64_GX/Config.h            */
	const char  *nombreEn;   /* etiqueta, ingles                          */
	const char  *nombreEs;   /* etiqueta, espanol (UTF-8 por escapes)     */
	const char  *infoEn;     /* descripcion, ingles                       */
	const char  *infoEs;     /* descripcion, espanol                      */
} Gln64HackOpcion;

/* La tabla y cuantas entradas tiene. */
const Gln64HackOpcion *gln64_hacks_tabla(int *n);

/* Los que el usuario fuerza encendidos y apagados (mascaras de bits
 * `hack_*`).  Si un bit esta en los dos, gana `activar`. */
void gln64_hacks_forzados(unsigned int activar, unsigned int quitar);

#ifdef __cplusplus
}
#endif

#endif /* _XENON_GLN64_HACKS_H_ */
