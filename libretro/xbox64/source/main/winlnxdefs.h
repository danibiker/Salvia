/*
 * main/winlnxdefs.h  --  REENVIO.  [Xbox360]
 *
 * ESTE FICHERO SUSTITUYE AL DE Wii64.  Es la primera edicion sobre el arbol
 * copiado y conviene entender por que, porque el original no es "casi
 * portable": es activamente incompatible con el XDK.
 *
 * LO QUE HACIA EL DE Wii64 (64 lineas) Y POR QUE NO VALE EN Xbox 360:
 *
 *     #define __declspec(dllexport)     <- ★ neutraliza la PALABRA CLAVE
 *                                          __declspec.  Cualquier cabecera
 *                                          del XDK que la use despues de
 *                                          esto deja de compilar.
 *     #define __stdcall                 <- idem: en MSVC son convenciones de
 *     #define WINAPI                       llamada de verdad, y glN64.cpp:81
 *                                          (DllMain) y RSP.cpp:187
 *                                          (RSP_ThreadProc) las usan.
 *     typedef long __int32;             <- __int32 es palabra clave en MSVC.
 *     typedef unsigned int BOOL;        \
 *     typedef unsigned long DWORD;       |  los declara <xtl.h>, y
 *     typedef unsigned short WORD;       |  redeclararlos con otro tipo base
 *     typedef unsigned char BYTE;        |  es C2371.
 *     typedef unsigned int UINT;         |
 *     typedef unsigned long long DWORD64;|
 *     typedef short SHORT;               |
 *     typedef int HINSTANCE/HWND/        |
 *              WPARAM/LPARAM;            |
 *     typedef void* LPVOID;             /
 *     #define _cdecl                    <- esto SI se conserva: los headers
 *                                          zilmar lo usan como `CALL`
 *                                          (Gfx_#1.3.h:30).  Lo pone
 *                                          ../winlnxdefs.h.
 *     FALSE / TRUE                      <- los pone ../winlnxdefs.h.
 *     #include <gctypes.h>              <- se conserva aqui abajo.
 *
 * ★★ Y POR QUE UN REENVIO Y NO UNA COPIA CON EL CONTENIDO BUENO.
 *
 * En el arbol hay DOS winlnxdefs.h: este y `source/winlnxdefs.h` (el de
 * mupen64plus adaptado al XDK, que llega con el shim).  Los dos usan el
 * MISMO include guard, `WINLNXDEFS_H`.  Y los dos entran en la misma unidad
 * de compilacion de glN64:
 *
 *     OpenGL.cpp:28   #include "../main/winlnxdefs.h"      <- este
 *     OpenGL.cpp      #include <gccore.h>
 *       gccore.h:52     #include "../../source/winlnxdefs.h"  <- el otro
 *
 * Con dos ficheros distintos y el mismo guard, el segundo se salta ENTERO y
 * cual gana depende del orden de los includes.  Un reenvio deja un unico
 * fichero real y el problema desaparece.
 *
 * ★ Y no vale "poner source/ en el include path y borrar este fichero":
 * `#include "../main/winlnxdefs.h"` se resuelve PRIMERO relativo al fichero
 * que lo incluye (`source/glN64_GX/../main/`), y si ahi no hay nada falla
 * antes de mirar el include path.  18 ficheros de glN64_GX lo piden asi.
 *
 * El original esta intacto en C:\develop\proyectos\personal\Wii64.
 */

#ifndef _XBOX64_MAIN_WINLNXDEFS_FWD_H_
#define _XBOX64_MAIN_WINLNXDEFS_FWD_H_

/* Lo traia el de Wii64 en su linea 33 y hay codigo que cuenta con ello
 * (ROM-Cache.h lo pide por su cuenta, pero memory.c depende de la cadena).
 * Lo resuelve xenon/gxshim/gctypes.h; es idempotente. */
#include <gctypes.h>

#include "../winlnxdefs.h"

#endif /* _XBOX64_MAIN_WINLNXDEFS_FWD_H_ */
