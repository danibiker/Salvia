/***************************************************************************
                           StdAfx.h  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de

 SPU plugin port from pcsx_rearmed (notaz, 2010-2015) — cycle-driven
 model.  Adapted to MSVC 2010 XDK / Xbox 360 PowerPC big-endian.
 ***************************************************************************/

#ifndef __P_STDAFX_H__
#define __P_STDAFX_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * GCC-ism shims for MSVC 2010 (Xbox 360 XDK).
 *
 * pcsx_rearmed's SPU uses GCC-only attributes throughout.  Map them
 * to the MSVC equivalents (or no-op when there is no equivalent).
 * ============================================================ */
#if defined(_MSC_VER)
  #ifndef __attribute__
    /* Strip any leftover GCC attributes so they are no-ops in MSVC. */
    #define __attribute__(x)
  #endif
  #ifndef __restrict__
    #define __restrict__ __restrict
  #endif
  #ifndef noinline
    #define noinline __declspec(noinline)
  #endif
  #ifndef forceinline
    #define forceinline __forceinline
  #endif
  #ifndef unlikely
    #define unlikely(x) (x)
  #endif
  /* MSVC doesn't expand `static inline` portably in C; pcsx_rearmed
   * uses INLINE for "force inline if possible".  Use __forceinline. */
  #ifndef INLINE
    #define INLINE static __forceinline
  #endif
  /* pcsx_rearmed uses preload(p) → __builtin_prefetch.  Best portable
   * shim is to drop it (the dynarec hot path doesn't loop here). */
  #ifndef preload
    #define preload(p) ((void)0)
  #endif
  /* Aligned globals: GCC __attribute__((aligned(N))) → MSVC __declspec(align(N)).
   * pcsx_rearmed only aligns one global (SPUInfo spu).  Provide an
   * ALIGN_VAR(N) macro the C source can use; the original __attribute__
   * lines in spu.c will be edited to use this. */
  #define ALIGN_VAR(N) __declspec(align(N))
#else
  /* GCC fallback (not used in this build). */
  #ifndef INLINE
    #define INLINE static inline
  #endif
  #ifndef noinline
    #define noinline __attribute__((noinline))
  #endif
  #ifndef forceinline
    #define forceinline __attribute__((always_inline))
  #endif
  #ifndef unlikely
    #define unlikely(x) __builtin_expect((x), 0)
  #endif
  #if defined(__GNUC__)
    #define preload __builtin_prefetch
  #else
    #define preload(...)
  #endif
  #define ALIGN_VAR(N) __attribute__((aligned(N)))
#endif

#ifdef LOG_UNHANDLED
#define log_unhandled printf
#else
#define log_unhandled(...) ((void)0)
#endif

/* ============================================================
 * MSVC compat shims that the original pcsxr-360 had as well
 * ============================================================ */
#undef CALLBACK
#define CALLBACK
#define DWORD unsigned int
#define LOWORD(l)           ((unsigned short)(l))
#define HIWORD(l)           ((unsigned short)(((unsigned int)(l) >> 16) & 0xFFFF))

/* Xbox 360 (Xenon) is big-endian PowerPC.  PSX SPU memory is stored
 * in little-endian 16-bit words; the byte-swap helpers below map
 * pcsx_rearmed's HTOLE16/LE16TOH macros correctly. */
#if defined(_XBOX) || defined(__BIGENDIAN__) \
    || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  #if defined(_MSC_VER)
    #define HTOLE16(x) _byteswap_ushort((unsigned short)(x))
    #define LE16TOH(x) _byteswap_ushort((unsigned short)(x))
    #define SWAP16(x)  _byteswap_ushort((unsigned short)(x))
  #else
    #define HTOLE16(x) __builtin_bswap16(x)
    #define LE16TOH(x) __builtin_bswap16(x)
    #define SWAP16(x)  __builtin_bswap16(x)
  #endif
#else
  #define HTOLE16(x) (x)
  #define LE16TOH(x) (x)
  #define SWAP16(x)  (x)
#endif

#include "psemuxa.h"

/* NOTE: plugin entry-point renames (SPUinit → PEOPS_SPUinit, etc.)
 * are NOT done here.  They live in spu.h, applied AFTER plugins.h
 * has had a chance to declare its typedefs of the same names.  If
 * the renames were applied here they would also rewrite the plugin
 * function-pointer typedefs in plugins.h (e.g. `typedef long (
 * CALLBACK* SPUinit)(void)` would become `typedef long (CALLBACK*
 * PEOPS_SPUinit)(void)`), which then conflicts with the PEOPS_*
 * function declarations in xbPlugins.h. */

#endif /* __P_STDAFX_H__ */
