/*
 * gxshim/gctypes.h  --  los tipos escalares de libogc, sueltos.
 *
 * POR QUE EXISTE ESTE FICHERO.
 *
 * Cuatro ficheros del arbol de Wii64 piden `<gctypes.h>` DIRECTAMENTE, sin
 * pasar por `<gccore.h>`:
 *
 *     source/gc_memory/tlb.c:30
 *     source/main/ROM-Cache.h:29
 *     source/main/winlnxdefs.h:33
 *     source/r4300/pure_interp.c:30
 *
 * En libogc `gctypes.h` es una cabecera de verdad y `gccore.h` la arrastra.
 * Nuestro shim invirtio eso: metio los typedefs DENTRO de gccore.h.  Para los
 * cuatro de arriba eso no vale -- ninguno quiere la API de GX, solo los
 * tipos -- y arrastrarles gccore.h entero seria meterles `<xtl.h>`,
 * `gln64_prof.h` y 883 lineas de enums GX_* en el interprete y en la TLB.
 *
 * Asi que se separan los tipos aqui, y no se toca ni un fichero de Wii64.
 *
 * ★ LA GUARDA ES LA DE libogc A PROPOSITO.  `gccore.h:82` envuelve su bloque
 * de typedefs en el mismo `#ifndef _GCTYPES_H_`, asi que los dos ordenes
 * funcionan y ninguno duplica:
 *
 *     gctypes.h primero -> define _GCTYPES_H_ -> gccore.h se salta su bloque
 *     gccore.h  primero -> define _GCTYPES_H_ -> este fichero no aporta nada
 *
 * Los typedefs son BYTE A BYTE los de gccore.h:84-107.  Si alguna vez hay que
 * tocarlos, tocar LOS DOS SITIOS: una redefinicion identica de typedef es
 * legal en C++ y una divergente es `error C2371`, que es justo el fallo que
 * este esquema esta evitando.  El porque de cada tipo base (coincidir con
 * source/xetypes.h y con hle.h) esta documentado en gccore.h:57-81.
 *
 * ★ BOOL NO SE DEFINE AQUI.  En libogc `gctypes.h` trae `typedef s32 BOOL;`,
 * pero en Xbox 360 lo define `<xtl.h>` y las dos declaraciones divergen.  Lo
 * mismo hizo `source/libogc_lwp_heap.h:79`.  Quien necesite BOOL que incluya
 * `<xtl.h>` o `winlnxdefs.h`.
 *
 * Toolchain: VS2010 / XDK.  Tiene que ser valido en C89: lo incluyen .c.
 */

#ifndef _XENON_GXSHIM_GCTYPES_H_
#define _XENON_GXSHIM_GCTYPES_H_

/* ★ ESTO NO ES DECORACION: reproduce una cadena de la que hay codigo
 * colgando.  El `gctypes.h` de libogc incluye `<stdbool.h>`, y por eso
 * `gc_memory/pif.c:62`, `dma.c:64` y `flashram.c:49` pueden escribir
 * `bool eepromWritten = false;` sin incluir nada.  Los tres llegan aqui por
 * `../main/winlnxdefs.h`.  Si se quita este include, esos tres ficheros dan
 * C2065 en el primer `bool`.
 *
 * Resuelve a xbox/compat/stdbool.h, que es donde esta explicado el
 * `unsigned char` y su trampa de truncacion. */
#include <stdbool.h>

#ifndef _GCTYPES_H_
#define _GCTYPES_H_

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

typedef signed char         s8;
typedef short               s16;
typedef int                 s32;
typedef long long           s64;

typedef volatile unsigned char       vu8;
typedef volatile unsigned short      vu16;
typedef volatile unsigned int        vu32;
typedef volatile unsigned long long  vu64;

typedef volatile signed char         vs8;
typedef volatile short               vs16;
typedef volatile int                 vs32;
typedef volatile long long           vs64;

typedef float               f32;
typedef double              f64;

#endif /* _GCTYPES_H_ */

/* TRUE/FALSE/NULL: en libogc tambien salen por esta via, y gccore.h:110-120
 * los pone con estas mismas guardas.  Van FUERA de _GCTYPES_H_ por lo mismo
 * que alli: son `#define`, no typedefs, y `#ifndef` ya evita el choque. */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL  0
#endif

#endif /* _XENON_GXSHIM_GCTYPES_H_ */
