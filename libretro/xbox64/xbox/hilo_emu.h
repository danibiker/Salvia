/*
 * xbox/hilo_emu.h  --  el emulador en su propio hilo, al estilo de mupen.
 *
 * ★★★ ETAPA 4 DEL RENDERIZADO CON HILO.
 *
 * Es el modelo de mupen64plus-libretro-nx con `EnableThreadedRenderer`
 * (libretro/libretro.c:448-493, 2043-2099; ThreadedOpenGl/opengl_Wrapper.cpp):
 *
 *   HILO DEL EMULADOR (HW2)            retro_run (HW1, dueno del device)
 *   ---------------------------        ---------------------------------
 *   gxd3d_frame_begin()                restar el swap del frame anterior
 *   xbox64_run_frame()  -- CPU, RSP,   render target + VB + bind
 *     display list, audio: el shim     ejecutar ordenes HASTA EL SWAP
 *     GRABA ordenes D3D                soltar VB + back buffer
 *   gxd3d_frame_end()                  audio -> audio_batch_cb
 *   punto seguro (pausa, parada)       video_cb
 *   esperar si swaps > MAX_SWAP
 *   ++swaps, encolar SWAP
 *
 * El colchon de `MAX_SWAP` frames es lo que REPARTE LA CARGA: un VI con
 * display list cuesta 25-30 ms en el hilo del emulador, pero como va hasta
 * ~3 frames por delante, `retro_run` sigue presentando a su ritmo mientras
 * el emulador recupera en los VI baratos.  Mientras la MEDIA quepa en
 * 16,7 ms, ningun frame del frontend carga con toda la lista.
 *
 * DESVIACIONES DE mupen, las tres justificadas:
 *   1. El hilo es PERSISTENTE (se crea en `retro_init` y se aparca entre
 *      juegos) en vez de crearse en cada carga.  Crear y destruir hilos por
 *      carga colgaba la consola dentro de `CreateThread` en pcsxr-360
 *      (libpcsxcore/gpu.c:1523-1549), y este arbol lo tiene anotado.
 *   2. El audio se sigue entregando en `retro_run` (ver audio_n64.c): el
 *      `audio_batch_cb` de Salvia no admite otro hilo.
 *   3. Sin libco: el consumidor es una funcion que vuelve en el SWAP, y lo
 *      que mupen hace "al reanudar el cothread" (restar el swap) se hace al
 *      empezar el `retro_run` siguiente.  Mismo orden, mismo efecto.
 *
 * Todo esto solo corre con la core option `xbox64_gfx_thread` activada, y se
 * decide AL CARGAR la ROM (como `EnableThreadedRenderer` en mupen, que se
 * lee en `retro_load_game`).  Apagada, `retro_run` hace lo de la etapa 3.
 *
 * C++03 (VS2010).
 */

#ifndef _XBOX_HILO_EMU_H_
#define _XBOX_HILO_EMU_H_

/* Ciclo de vida del hilo.  `crear` en retro_init, `destruir` en
 * retro_deinit.  Entre medias el hilo esta OCIOSO salvo mientras hay una
 * partida en marcha con el modo hilo. */
int  hilo_emu_crear(void);
void hilo_emu_destruir(void);

/* Empieza a emular en el hilo.  Lo llama el PRIMER `retro_run` tras cargar,
 * igual que mupen crea su pthread en el primer `retro_run`
 * (libretro.c:2053-2060).  Idempotente. */
void hilo_emu_arrancar(void);

/* Para de emular y devuelve el hilo a ocioso.  Bombea la cola mientras
 * espera, porque el emulador puede estar esperando al consumidor (ritmo,
 * orden sincrona, pool lleno).  Al volver ya no queda nada grabado.  Lo
 * llama `retro_unload_game` ANTES de soltar nada. */
void hilo_emu_parar(void);

/* Aparca el emulador en el siguiente fin de VI -- un punto seguro -- y
 * espera a que lo este.  Para los savestates y el cambio de opciones, que
 * tocan estado del emulador desde el hilo del frontend.  Tambien bombea. */
void hilo_emu_pausar(void);
void hilo_emu_reanudar(void);

/* 1 si el emulador esta corriendo en el hilo (entre arrancar y parar). */
int  hilo_emu_corriendo(void);

/* ★ EL CONSUMIDOR.  Lo llama `retro_run` con el render target del core
 * ligado, el vertex buffer bloqueado y la sombra de estado repuesta.
 * Ejecuta ordenes hasta el SWAP del siguiente frame y vuelve. */
void hilo_emu_consumir_frame(void);

/* El informe por segundo: lineas [HILO] y [UTIL emu].  Lo llama el
 * resumen por segundo de libretro_core.cpp. */
void hilo_emu_informe(unsigned frames);

#endif /* _XBOX_HILO_EMU_H_ */
