// ============================================================================
// [Salvia] Puente core -> Direct3D para la GPU de CV1000 (epic12) en Xbox 360
// ============================================================================
//
// El compositor Xenos de liujiny (src/burn/devices/epic12_gpu_xbox.h) usaba
// siete funciones SDL_XBOX_*CoreGpu* que el anadio a libSDLx360, junto con un
// modo de presentacion asincrona en el frontend.  Salvia no mete en el frontend
// nada especifico de un core, asi que aqui se resuelve todo dentro de FBNeo:
//
//   * El device se pide por el canal generico que ya usan xbox64/pcsxr-360:
//     RETRO_ENVIRONMENT_SALVIA_GET_D3D_DEVICE (30002), en el hilo de emulacion
//     durante retro_load_game.  Si el frontend no lo da, el compositor se
//     desactiva solo y CV1000 dibuja por CPU como siempre.
//   * El lock lo pone un CRITICAL_SECTION propio del core: serializa el hilo
//     del blitter de epic12 con el hilo de emulacion.  El frontend no necesita
//     lock porque el core deja la GPU sin trabajo antes de que retro_run()
//     devuelva el control (ver la espera al final de DrvFrame en d_cv1k.cpp).
//   * La presentacion asincrona y los diagnosticos de pausa no existen aqui:
//     el compositor trabaja siempre en modo 0 (readback al bitmap del core y
//     entrega normal por video_cb), que es su ruta por defecto.
//
// Tambien sirve de buzon para los avisos de estado de la GPU, para que
// libretro.cpp no dependa de epic12.cpp (excluido en algunas configuraciones).
#ifndef RETRO_CORE_GPU_H
#define RETRO_CORE_GPU_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _XBOX
// libretro.cpp, hilo de emulacion.  NULL al descargar el juego.
void  RetroCoreGpuSetDevice(void *device);
// Cualquier hilo del core.  Si devuelve no-NULL hay que llamar a Release.
// generation cambia cuando cambia el device: el core recrea sus recursos.
void *RetroCoreGpuAcquire(unsigned *generation);
void  RetroCoreGpuRelease(void);
// Avisos de estado ("CV1000 GPU: ...") que libretro.cpp muestra con
// RETRO_ENVIRONMENT_SET_MESSAGE.  El driver registra su funcion al iniciar.
void  RetroCoreGpuSetMessageSource(const char *(*source)(void));
const char *RetroCoreGpuTakeMessage(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
