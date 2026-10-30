/*
 * 360/win32/pcsxr_win32_pre.h  --  se fuerza al principio de CADA unidad de
 * compilacion de la build Win32 (ForcedIncludeFiles de los seis .vcxproj).
 * En Xbox 360 no existe.
 *
 * [Windows, 2026-10-30]
 *
 *   PCSXR360_WIN32   "esta es la build de Windows de este port".  El arbol usa
 *                    _XBOX con dos sentidos: "hardware Xbox" (big-endian,
 *                    intrinsecos PPC, hilos HW) y "este port libretro" (nombres
 *                    PEOPS_*, GPUsetframelimit, tipos de Windows en vez de los
 *                    de X11).  Las guardas del segundo tipo pasan a ser
 *                    `defined(_XBOX) || defined(PCSXR360_WIN32)`; las del
 *                    primero no se tocan.
 *
 *   inline           el compilador C del XDK acepta `inline` en C; el de VS2010
 *                    para x86 no (C89: solo __inline).  En C++ es palabra clave
 *                    y no se toca.
 */

#ifndef _PCSXR360_WIN32_PRE_H_
#define _PCSXR360_WIN32_PRE_H_

#if defined(_XBOX)
#error "pcsxr_win32_pre.h es solo para la build de Windows"
#endif

#define PCSXR360_WIN32 1

#if !defined(__cplusplus) && !defined(inline)
#define inline __inline
#endif

#endif /* _PCSXR360_WIN32_PRE_H_ */
