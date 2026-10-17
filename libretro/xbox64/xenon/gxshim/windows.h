/*
 * gxshim/windows.h  --  reenvio a <xtl.h>.
 *
 * QUIEN LO PIDE.  `Rice_GX/liblinux/BMGImage.h:29`:
 *
 *     #ifdef _WIN32
 *     #include <windows.h>
 *     #else
 *     #include "../winlnxdefs.h"
 *     #endif
 *
 * ★ Y SE METE POR LA RAMA DE ARRIBA AUNQUE ESTEMOS EN UNA CONSOLA.  El
 * compilador del XDK define `_WIN32` (es MSVC, y Xbox 360 es Windows por
 * dentro), asi que la condicion es cierta.  Lo que NO hay en el XDK es
 * `windows.h`: su cabecera raiz es `<xtl.h>`, que trae el subconjunto de
 * Win32 que la consola implementa.  Sin este fichero, el build muere con
 *
 *     BMGImage.h(30): fatal error C1083: Cannot open include file:
 *         'windows.h': No such file or directory
 *
 * en TRES unidades (OGLGraphicsContext.cpp, Render.cpp y TextureFilters.cpp
 * incluyen liblinux/BMGLibPNG.h, y este sin guarda de __GX__).
 *
 * ★ POR QUE UN REENVIO Y NO TOCAR EL `#ifdef _WIN32` DE Rice.  Porque
 * `liblinux/` es codigo de terceros copiado verbatim y el reenvio vale para
 * cualquier otro fichero que herede la misma suposicion.  Es ademas el
 * apanyo estandar en los ports al XDK.
 *
 * ★ NO ES UN `windows.h` DE VERDAD y no pretende serlo: si alguien llama a
 * una API de Win32 que la consola no tiene, el fallo sera un LNK2019 con el
 * nombre de la funcion, que es exactamente la informacion que hace falta.
 *
 * ★ Es idempotente y no molesta al resto del arbol: nadie mas incluye
 * <windows.h>, y si algun dia alguien lo hace, aterriza aqui en vez de
 * fallar.
 */

#ifndef _XBOX64_GXSHIM_WINDOWS_H_
#define _XBOX64_GXSHIM_WINDOWS_H_

#include <xtl.h>

#endif /* _XBOX64_GXSHIM_WINDOWS_H_ */
