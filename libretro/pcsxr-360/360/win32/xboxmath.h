/*
 * 360/win32/xboxmath.h  --  <xboxmath.h> del XDK, en Windows.
 *
 * [Windows, 2026-10-30] Solo la ve la build Win32 (ver xtl.h de esta carpeta).
 *
 * La incluye plugins/xbox_soft/xb_video.cpp, cuyo cuerpo entero esta detras
 * de `#if 0` (era la salida D3D propia de la epoca standalone; ahora pinta el
 * frontend via libretro).  No se usa nada de ella.
 *
 * ★ ESTA VACIA A PROPOSITO.  Si algun dia se usa algo de XNA math, el error
 * de compilacion dira que, que es mejor que un stub que compile y haga otra
 * cosa.
 */

#ifndef _PCSXR360_WIN32_XBOXMATH_H_
#define _PCSXR360_WIN32_XBOXMATH_H_

#if defined(_XBOX)
#error "360/win32/xboxmath.h es solo para la build de Windows"
#endif

#endif /* _PCSXR360_WIN32_XBOXMATH_H_ */
