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
 * Los pixel shaders son los MISMOS que en Xbox: el HLSL vive en
 * libs/libSDLx360/SDL/src/video/xbox/SDL_shaders_src.h (header neutro
 * compartido por ambas plataformas).
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
SDL_Surface* SDL_XBOX_GetOverlay(void);
void         SDL_XBOX_SetOverlayEnabled(int enabled);

#ifdef __cplusplus
}
#endif

#endif /* WIN */
