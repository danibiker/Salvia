/*
 * win_d3d9.h - Capa de video D3D9 para Windows (Opcion B).
 *
 * Reproduce a nivel de aplicacion lo que el driver SDL de Xbox 360
 * (SDL_xboxvideo.c) hace dentro de SDL: sube el framebuffer del juego a
 * una textura D3D9, dibuja un quad fullscreen con un pixel shader
 * (HQx/CRT/xBR/etc.) y compone un overlay ARGB para los menus.
 *
 * SDL (precompilado) sigue gestionando ventana + input + eventos; este
 * modulo solo se encarga del render. Se obtiene el HWND con SDL_GetWMInfo
 * y se crea un IDirect3D9 propio sobre esa ventana.
 *
 * Los pixel shaders son los MISMOS que en Xbox, pero ya no viven en el
 * binario: se cargan de <appDir>\assets\shaders (presets .hlslp + cuerpos
 * .hlsl, formato estilo RetroArch). La capa de aplicacion los descubre y
 * parsea en src/video/shaderpreset.cpp y publica la tabla resultante por la
 * API C de src/video/salvia_shader_api.h, que este modulo implementa. Lo
 * unico que sigue embebido es el passthrough de SDL_shaders_src.h, como
 * fallback para que jamas se enganche un pixel shader NULL.
 */
#pragma once

#ifdef WIN

#include <windows.h>
#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa D3D9 sobre la ventana SDL. bbw/bbh = tamano del backbuffer
   (normalmente el tamano de la ventana / resolucion de escritorio en
   fullscreen-borderless). Compila los shaders y crea el overlay.
   Llamar UNA vez, justo despues de SDL_SetVideoMode. Devuelve 1 OK / 0 error. */
int WinD3D9_Init(HWND hwnd, int bbw, int bbh);

/* Libera textura de juego, overlay, shaders y device. */
void WinD3D9_Shutdown(void);

/* (Re)crea la textura del juego y el surface fuente al tamano nativo del
   core (no toca la ventana ni el backbuffer). Devuelve el SDL_Surface
   donde el core escribe los frames (formato segun bpp). Equivalente a
   XBOX_SetVideoMode. El puntero devuelto pasa a ser gameScreen. */
SDL_Surface* WinD3D9_SetGameMode(int width, int height, int bpp);

/* Sube el frame del game surface a la textura, dibuja quad + shader,
   compone el overlay y hace Present. Sustituye a SDL_Flip en PC.
   Thread-safe (serializado con un critical section, igual que en Xbox). */
void WinD3D9_Present(void);

/* ---------------------------------------------------------------------
 * API con los MISMOS nombres que la rama Xbox, para compartir los
 * call-sites del frontend bajo SALVIA_GPU_VIDEO (ver engine.h).
 * ------------------------------------------------------------------- */
void         XBOX_SelectEffect(int effectID);
void         SDL_XBOX_SetDisplaySize(float aspect_ratio);
void         SDL_XBOX_SetDisplayFullscreen(int fullscreen);
void         SDL_XBOX_SetDisplayOverflow(int overflow);
void         SDL_XBOX_SetRotation(int rotation);
void         SDL_XBOX_SetVSync(int enable);
/* Sin Reset del device.  No-op en Windows: D3D9 de escritorio no tiene
 * D3DRS_PRESENTINTERVAL.  Ver el comentario en win_d3d9.cpp. */
void         SDL_XBOX_SetPresentInterval(int enable);
void         SDL_XBOX_SetAdaptiveVSync(int enable);
/* Refresco real del panel en Hz, 0 si no se conoce.  En Windows viene de
 * D3DDISPLAYMODE y es un entero, asi que 59,94 sale como 60. */
float        SDL_XBOX_GetRefreshRate(void);
/* Contadores de vblank del hardware.  En Windows devuelve ceros: no hay
 * callback de vblank en D3D9 de escritorio. */
void         SDL_XBOX_GetVBlankStats(unsigned int *count, unsigned int *missed);
/* Devuelven "no se" en Windows (0 y -1): sin callback de vblank en D3D9 de
 * escritorio, el pacing cae al camino de reloj de CPU. */
int          SDL_XBOX_WaitVBlank(unsigned int msMax);
int          SDL_XBOX_GetScanlinePercent(void);
SDL_Surface* SDL_XBOX_GetOverlay(void);
/* Rect de la imagen del juego en pixeles del overlay. */
void         SDL_XBOX_GetGameRectOnOverlay(int *x, int *y, int *w, int *h);
void         SDL_XBOX_SetOverlayEnabled(int enabled);
void         SDL_XBOX_SetOverscan(int x, int y);

/* Devuelve el IDirect3DDevice9 interno (para HLSLBackground u otros). */
struct IDirect3DDevice9;
struct IDirect3DDevice9* WinD3D9_GetDevice(void);

#ifdef __cplusplus
}
#endif

#endif /* WIN */
