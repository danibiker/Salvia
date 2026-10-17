/*
 * gfx_plugin_sel.h  --  seleccion del plugin grafico.
 *
 * Lo incluyen `source/plugin/plugin.c` (que compila como C),
 * `xenon/gfx_thread.cpp` y `360/Xdk/xenon64/libretro_core.cpp`.
 *
 * ★ TIENE QUE SER VALIDO EN C.  `plugin.c` se compila como C
 * (CompileAs=Default en el vcxproj), asi que nada de referencias, `bool` ni
 * declaraciones a mitad de bloque.  Y los simbolos definidos en un `.cpp` y
 * leidos desde aqui necesitan `extern "C"` EN LA DEFINICION, no solo en la
 * declaracion: la firma del fallo es LNK2001 con el simbolo SIN DECORAR
 * (ver reference_xdk_vcxproj_compileas).
 */

#ifndef _XENON_GFX_PLUGIN_SEL_H_
#define _XENON_GFX_PLUGIN_SEL_H_

/* ------------------------------------------------------------------ */
/* ★ INTERRUPTOR MAESTRO                                               */
/* ------------------------------------------------------------------ */
/* A 0 el binario es EXACTAMENTE el de antes: ni glN64_GX ni su backend
 * entran en el enlace, `plugin.c` se queda con la rama de Rice y la core
 * option no se declara.  Es la misma disciplina que `XENON64_DIAG`
 * (ver reference_xenon64_diag_master), y sirve para dos cosas:
 *
 *   1. que el A/B contra Rice sea el MISMO ejecutable, y
 *   2. poder descartar el proyecto entero cambiando un 1 por un 0, que es
 *      la puerta de salida de la fase 2 del plan.
 *
 * Los ficheros de glN64_GX siguen en el vcxproj; a 0 compilan a nada porque
 * el toggle envuelve tambien su cableado. */
#define GFX_PLUGIN_GLN64 1

/* ------------------------------------------------------------------ */
/* Identificadores                                                     */
/* ------------------------------------------------------------------ */
#define GFX_PLUGIN_ID_RICE   0
#define GFX_PLUGIN_ID_GLN64  1

#ifdef __cplusplus
extern "C" {
#endif

/* Lo fija libretro_core.cpp desde la core option, ANTES de abrir la ROM.
 * Si nadie llama, queda Rice. */
void gfx_plugin_select(int id);

/* Lo consultan plugin.c (para cablear la tabla) y gfx_thread.cpp (para
 * despachar los puntos de entrada internos). */
int  gfx_plugin_selected(void);

/* Nombre legible, para el log. */
const char *gfx_plugin_name(void);

#if GFX_PLUGIN_GLN64
/* Arranque y parada del backend GX -> D3D9 de glN64_GX
 * (xenon/gln64_glue.cpp).
 *
 * ★ `gln64_backend_init` hay que llamarlo DESPUES de que `g_pd3dDevice`
 * apunte al device del frontend -- o sea despues de `EnsureGraphicsDevice()`
 * -- y ANTES de abrir la ROM.  Crea los vertex buffers y la declaracion de
 * vertices; sin el, el primer `GX_Begin` del plugin bloquea un vertex buffer
 * NULL y revienta. */
int  gln64_backend_init(int width, int height);
void gln64_backend_shutdown(void);

/* Opciones del plugin.  `frameBufferTextures` sale de la core option
 * `xenon64_gln64_framebuffer` y se fija ANTES de abrir la ROM, que es lo que
 * exige el orden: `Config_LoadConfig` lo copia a `OGL.frameBufferTextures`
 * desde `InitiateGFX`.  `sai2x` sigue sin opcion y va a 0. */
void gln64_set_options(int frameBufferTextures, int sai2x);

/* ★ El tamano REAL de la RDRAM, que glN64 cablea a 4 MB en tiempo de
 * compilacion (`USE_EXPANSION` no esta definido) y que con el Expansion Pak
 * son 8.  No es solo una cota: `RSP_SegmentToPhysical` la usa de MASCARA, asi
 * que quedarse corto PLIEGA las direcciones altas sobre las bajas y el parser
 * de display lists se desboca.  El porque largo esta en gln64_config.cpp.
 *
 * Lo llama main.cpp DESPUES de `gfx.romOpen()`, porque `RSP_Init()` corre
 * dentro y lo pisaria.  Tiene que ser potencia de dos. */
void gln64_apply_rdram_size(unsigned int bytes);

/* ★★ Vacia la cache de pixel shaders del shim GX->D3D9.
 *
 * La llama `xenon64_rom_close()` justo despues de `gfx.romClosed()`.  Era la
 * UNICA pieza de la tuberia grafica que sobrevivia a una carga de juego: ese
 * camino acaba en `OGL_Stop()`, que destruye combinador, framebuffer y
 * texturas, y la nuestra no se enteraba.  Se veia en el log en que `shgen`
 * iba 20 -> 40 -> 60 en tres cargas sin reiniciarse nunca.
 *
 * Inocua con Rice: esa cache es solo del camino de glN64.  El porque
 * completo, con la lista de todo lo que se descarto antes de llegar aqui,
 * esta en xenon/gx_d3d9_tev.cpp. */
void gxd3d_tev_flush_cache(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _XENON_GFX_PLUGIN_SEL_H_ */
