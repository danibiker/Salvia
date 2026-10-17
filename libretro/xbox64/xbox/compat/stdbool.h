/*
 * xbox/compat/stdbool.h  --  MSVC 10 no trae <stdbool.h> para C.
 *
 * QUIEN LO PIDE.  Dos caminos:
 *
 *   1. Directo:  source/rsp_hle/alist.h:25  #include <stdbool.h>
 *   2. Indirecto: source/gc_memory/{pif,dma,flashram}.c usan `bool`, `true`
 *      y `false` SIN incluir nada (pif.c:62, dma.c:64, flashram.c:49).  En
 *      devkitPPC les llegaba arrastrado por el <gctypes.h> de libogc, que
 *      incluye <stdbool.h>.  Nuestro gxshim/gctypes.h reproduce esa cadena
 *      incluyendo este fichero.
 *
 * ★ POR QUE `unsigned char` Y NO `int`.  Es la convencion de
 * libretro-common en este mismo repo (libretro-common/include/boolean.h, y
 * el mismo shim en libretro-atari800/.../compat/msvc/stdbool.h), pero sobre
 * todo hay una razon dura: el `bool` de C++ en MSVC ocupa 1 byte.  Las
 * estructuras de rsp_hle llevan campos `bool` y las ve tanto codigo C como
 * el wrapper, que es C++.  Con `int` los dos lados verian tamanyos distintos
 * y el desajuste seria silencioso.
 *
 * ★★ Y LA TRAMPA QUE TRAE: `bool` pasa a ser un TIPO DE 8 BITS, no el tipo
 * de C++ que normaliza a 0/1.  `b = expresion_entera;` TRUNCA.  Si la
 * expresion es, por ejemplo, `x & 0x100`, el resultado es 0 aunque el bit
 * este puesto.  Ese fallo exacto dejo la pantalla en negro en el port de
 * beetle-vb de este repo.  Al tocar codigo que asigne a un `bool`,
 * comprobar que el lado derecho ya es 0/1.
 *
 * Este directorio solo entra en el include path desde el vcxproj, asi que no
 * puede ensombrecer un <stdbool.h> de verdad.
 */

#ifndef _XBOX64_COMPAT_STDBOOL_H_
#define _XBOX64_COMPAT_STDBOOL_H_

/* ★ Sonda: confirma que `<stdbool.h>` resuelve AQUI y no a otro fichero.
 * Ni el XDK ni el CRT de VS2010 traen stdbool.h (comprobado 2026-09-23),
 * asi que no deberia haber competencia -- pero si algun dia la hay, este
 * mensaje deja de salir y se ve en el acto.
 *
 * Comentada por el mismo motivo que su gemela de xbox/xbox64_pre.h: sale
 * una vez por unidad y ensucia el log.  Descomentar las dos a la vez. */
/* #pragma message("[xbox64] compat/stdbool.h en uso") */

#ifndef __cplusplus

#if defined(_MSC_VER) && _MSC_VER < 1800

/* https://support.microsoft.com/en-us/kb/980263 : sin esto, MSVC avisa por
 * cada conversion a este `bool` de mentira. */
#pragma warning (disable:4116)
#pragma warning (disable:4805)

#ifndef bool
#define bool  unsigned char
#endif
#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif

#define __bool_true_false_are_defined 1

#endif /* _MSC_VER < 1800 */

#endif /* !__cplusplus */

#endif /* _XBOX64_COMPAT_STDBOOL_H_ */
