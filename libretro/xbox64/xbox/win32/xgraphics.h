/*
 * xbox/win32/xgraphics.h  --  <xgraphics.h> del XDK, en Windows.
 *
 * [Windows, 2026-10-26] Solo la ve la build Win32 (ver xbox/win32/xtl.h).
 *
 * <xgraphics.h> es la biblioteca de la Xbox para el formato de memoria de su
 * GPU: tamanyo de superficies en la EDRAM (XGSurfaceSize), cabeceras de
 * textura sobre memoria propia (XGSetTextureHeader), y el tileado
 * (XGAddress2DTiledOffset, XGUntileSurface).  Nada de eso existe ni hace falta
 * en PC: las superficies no viven en una EDRAM y las texturas son lineales.
 *
 * ★ ESTA VACIA A PROPOSITO.  Los usos de esas funciones van detras de
 * `#if defined(_XBOX)` en el shim, con su alternativa de PC al lado.  Si
 * alguno se queda sin cubrir, el error de compilacion dice cual, que es
 * mejor que un stub que compile y haga otra cosa.
 */

#ifndef _XBOX64_WIN32_XGRAPHICS_H_
#define _XBOX64_WIN32_XGRAPHICS_H_

#include <d3d9.h>

#endif /* _XBOX64_WIN32_XGRAPHICS_H_ */
