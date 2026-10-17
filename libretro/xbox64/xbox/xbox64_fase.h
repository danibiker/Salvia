/*
 * xbox/xbox64_fase.h  --  los interruptores maestros del port.
 *
 * Misma disciplina que `XENON64_DIAG` y `GFX_PLUGIN_GLN64` en el arbol
 * aparcado: una constante por pieza grande, de modo que cada fase se pueda
 * apagar entera cambiando un 1 por un 0 y volver EXACTAMENTE al binario
 * anterior.  Es lo que convierte "no arranca" en un A/B en vez de en una
 * arqueologia.
 *
 * ★ TIENE QUE SER VALIDO EN C.
 */

#ifndef _XBOX64_FASE_H_
#define _XBOX64_FASE_H_

/* ------------------------------------------------------------------ */
/* FASE 1: sin plugin grafico                                          */
/* ------------------------------------------------------------------ */
/* A 1, `xbox/plugins_fase1.c` aporta los puntos de entrada graficos que el
 * nucleo llama (updateScreen, processDList, processRDPList,
 * viStatusChanged, viWidthChanged, showCFB) como funciones vacias, y
 * glN64_GX NO entra en el enlace.
 *
 * A 0 desaparecen de este fichero y los aporta glN64_GX por el renombre de
 * macro de `glN64_GX/GFXPlugin.h:29` (`#define UpdateScreen updateScreen`).
 * Los dos a la vez serian LNK2005: por eso es un interruptor y no dos
 * ficheros sueltos.
 *
 * La puerta de salida de la fase 1 se comprueba con esto a 1: si la ROM no
 * arranca al interprete SIN video, el problema no esta en el video. */
#define XBOX64_GFX_NULO 0

/* ------------------------------------------------------------------ */
/* FASE 3: el dynarec                                                  */
/* ------------------------------------------------------------------ */
/* A 0, `r4300/ppc/*.c` no entra en el vcxproj, `xbox/dynarec_ausente.c`
 * repone los cinco simbolos que el nucleo referencia, y `dynacore` se
 * fuerza a DYNACORE_PURE_INTERP en xbox64_init.  El nucleo lo consulta en
 * r4300.c:196 y elige `pure_interpreter()`.
 *
 * ★ A 1 (fase 3) hay que cambiar TRES cosas a la vez, y este define solo
 * cubre dos.  La tercera es del vcxproj y no se entera de este fichero:
 *
 *   1. aqui: dynarec_ausente.c se vacia y xbox_mem.h expone RECOMPMETA_LO
 *      (4 MB) y RECOMP_CACHE_HEAP_SCRATCH_LO (64 KB); plataforma.c pone
 *      `dynacore = DYNACORE_DYNAREC`.
 *   2. vcxproj: los cinco .c de ppc/ + Recomp-Cache-Heap.c, los seis como
 *      C++ (son C99).
 *   3. vcxproj: los defines USE_RECOMP_CACHE, FASTMEM y FASTMEM_HOT_NOFLUSH,
 *      que el build de referencia de Wii64 lleva y que NO dan error de
 *      enlace si faltan: dan un binario distinto, en silencio.
 *
 * Volver a 0 exige deshacer las tres.  Esta escrito aqui para que el A/B
 * contra la fase 2 siga siendo posible en un minuto.
 *
 * ★★ ESTO NO ES LO MISMO QUE `PPC_DYNAREC`, Y CONFUNDIRLOS CUESTA UN
 * BUILD ENTERO (ya lo costo).
 *
 *   PPC_DYNAREC      lo define el vcxproj, SIEMPRE, en las cuatro
 *                    configuraciones.  No dice "hay dynarec": dice que el
 *                    arbol se compila en su UNICA configuracion valida.
 *                    La rama `#ifndef PPC_DYNAREC` de Wii64 usa el tipo
 *                    `precomp_block`, que en este arbol NO SE DECLARA EN
 *                    NINGUN SITIO -- aparece solo en las tres lineas que
 *                    lo usan (r4300.h:94, r4300.c:70, r4300.c:183).
 *                    Wii64 borro el recompilador x86 de mupen64 y con el
 *                    su typedef.  Es codigo muerto que nunca ha
 *                    compilado, y sin PPC_DYNAREC salen 20 errores:
 *                    C2143 en r4300.h:94, C2373 contra Recompile.h:106
 *                    (porque r4300.c:40 incluye ppc/Wrappers.h SIN
 *                    guarda) y una cascada de C2223 sobre `->block`.
 *
 *   XBOX64_DYNAREC   esto.  Dice si los .c de ppc/ estan EN EL ENLACE.
 *
 * Lo que decide si el dynarec se EJECUTA no es ninguno de los dos: es
 * `dynacore` en tiempo de ejecucion.
 *
 * ★ Y hay una consecuencia en el mapa de memoria: `BLOCKS_LO` cuelga de
 * PPC_DYNAREC (r4300.c:64), no de esto, asi que sus 4 MB se reservan
 * desde la fase 1.  Ver xbox/xbox_mem.h. */
#define XBOX64_DYNAREC 1

#endif /* _XBOX64_FASE_H_ */
