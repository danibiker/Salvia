#ifndef PORT_H
#define PORT_H

#ifdef __PS3__
/* PlayStation3 */
#include <ppu_intrinsics.h>
#endif

#ifdef _XBOX360
/* XBox 360 */
#include <ppcintrinsics.h>
#endif

#include "types.h"

/* Inside short spin-wait loops, hint the core to back off (lower SMT priority
 * on PPE/Cell, micro-throttle on x86, yield on ARM). Crucial on in-order
 * PPC where a tight spin starves the sibling SMT thread that's producing
 * the value the spinner is waiting on. */
#if defined(__PS3__) || defined(__POWERPC__) || defined(__powerpc__) || defined(__ppc__) || defined(_XBOX360)
/* `or 27,27,27` is the PPC low-priority SMT hint (Power ISA Book II) */
#define SPIN_HINT() __asm__ volatile("or 27,27,27" ::: "memory")
#elif defined(_M_IX86) || defined(_M_X64)
/* MSVC on x86/x64: no GCC-style __asm__ syntax; use the SSE2 PAUSE
 * intrinsic from <intrin.h>.  Available since MSVC 2005. */
#include <intrin.h>
#define SPIN_HINT() _mm_pause()
#elif defined(__x86_64__) || defined(__i386__)
#define SPIN_HINT() __asm__ volatile("pause" ::: "memory")
#elif defined(__aarch64__) || defined(__arm__)
#define SPIN_HINT() __asm__ volatile("yield" ::: "memory")
#else
#define SPIN_HINT() ((void)0)
#endif

/* if a >= 0 return x else y*/
#define isel(a, x, y) ((x & (~(a >> 31))) + (y & (a >> 31)))

#ifdef FRONTEND_SUPPORTS_RGB565
/* 16bit color - RGB565 */
#define RED_MASK  0xf800
#define GREEN_MASK 0x7e0
#define BLUE_MASK 0x1f
#define RED_EXPAND 3
#define GREEN_EXPAND 2
#define BLUE_EXPAND 3
#define RED_SHIFT 11
#define GREEN_SHIFT 5
#define BLUE_SHIFT 0
#define CONVERT_COLOR(color) (((color & 0x001f) << 11) | ((color & 0x03e0) << 1) | ((color & 0x0200) >> 4) | ((color & 0x7c00) >> 10))
#else
/* 16bit color - RGB555 */
#define RED_MASK  0x7c00
#define GREEN_MASK 0x3e0
#define BLUE_MASK 0x1f
#define RED_EXPAND 3
#define GREEN_EXPAND 3
#define BLUE_EXPAND 3
#define RED_SHIFT 10
#define GREEN_SHIFT 5
#define BLUE_SHIFT 0
#define CONVERT_COLOR(color) ((((color & 0x1f) << 10) | (((color & 0x3e0) >> 5) << 5) | (((color & 0x7c00) >> 10))) & 0x7fff)
#endif

#ifdef _MSC_VER
#include <stdlib.h>
#define strcasecmp _stricmp
#endif

#ifdef USE_CACHE_PREFETCH
#if defined(__ANDROID__)
/* Android: arg is the lvalue to prefetch. __builtin_prefetch is gcc/clang.
 * The previous form expanded to "prefetch(&prefetch)" (a function call on
 * the parameter itself) which never compiled. */
#define CACHE_PREFETCH(prefetch) __builtin_prefetch(&(prefetch));
#elif defined(_XBOX)
#define CACHE_PREFETCH(prefetch) __dcbt(0, &prefetch);
#else
#define CACHE_PREFETCH(prefetch) __dcbt(&prefetch);
#endif
#else
#define CACHE_PREFETCH(prefetch)
#endif

#ifdef MSB_FIRST
#if defined(__SNC__)
#define READ16LE( base )        (__builtin_lhbrx((base), 0))
#define READ32LE( base )        (__builtin_lwbrx((base), 0))
#define WRITE16LE( base, value )    (__builtin_sthbrx((value), (base), 0))
#define WRITE32LE( base, value )    (__builtin_stwbrx((value), (base), 0))
#elif defined(__GNUC__) && defined(__ppc__)
#define READ16LE( base )        ({unsigned ppc_lhbrx_; asm( "lhbrx %0,0,%1" : "=r" (ppc_lhbrx_) : "r" (base), "0" (ppc_lhbrx_) ); ppc_lhbrx_;})
#define READ32LE( base )        ({unsigned ppc_lwbrx_; asm( "lwbrx %0,0,%1" : "=r" (ppc_lwbrx_) : "r" (base), "0" (ppc_lwbrx_) ); ppc_lwbrx_;})
#define WRITE16LE( base, value)    ({asm( "sthbrx %0,0,%1" : : "r" (value), "r" (base) );})
#define WRITE32LE( base, value)    ({asm( "stwbrx %0,0,%1" : : "r" (value), "r" (base) );})
#elif defined(_XBOX360)
#define READ16LE( base)	_byteswap_ushort(*((uint16_t *)(base)))
#define READ32LE( base) _byteswap_ulong(*((uint32_t *)(base)))
#define WRITE16LE(base, value) *((uint16_t *)(base)) = _byteswap_ushort((value))
#define WRITE32LE(base, value) *((uint32_t *)(base)) = _byteswap_ulong((value))
#else
/* Generic portable fallback: byte-swap via masked shifts.
 * The original generic fallback had a syntax error in READ32LE and was missing
 * byte masks. Reach this only on a BE target without ppc/xbox360 intrinsics. */
#define READ16LE(x) ((((*((uint16_t *)(x))) >> 8) & 0x00FF) | (((*((uint16_t *)(x))) << 8) & 0xFF00))
#define READ32LE(x) ((((*((uint32_t *)(x))) >> 24) & 0x000000FF) | \
                     (((*((uint32_t *)(x))) >>  8) & 0x0000FF00) | \
                     (((*((uint32_t *)(x))) <<  8) & 0x00FF0000) | \
                     (((*((uint32_t *)(x))) << 24) & 0xFF000000))
#define WRITE16LE(x,v) (*((uint16_t *)(x)) = (uint16_t)((((v) >> 8) & 0x00FF) | (((v) << 8) & 0xFF00)))
#define WRITE32LE(x,v) (*((uint32_t *)(x)) = ((((uint32_t)(v)) >> 24) & 0x000000FF) | \
                                        ((((uint32_t)(v)) >>  8) & 0x0000FF00) | \
                                        ((((uint32_t)(v)) <<  8) & 0x00FF0000) | \
                                        ((((uint32_t)(v)) << 24) & 0xFF000000))
#endif
#else
#define READ16LE(x) *((uint16_t *)(x))
#define READ32LE(x) *((uint32_t *)(x))
#define WRITE16LE(x,v) *((uint16_t *)(x)) = (v)
#define WRITE32LE(x,v) *((uint32_t *)(x)) = (v)
#endif

#ifdef INLINE
 #if defined(_MSC_VER)
  #define FORCE_INLINE __forceinline
 #elif defined(__GNUC__)
  #define FORCE_INLINE __inline__ __attribute__((always_inline))
 #else
  #define FORCE_INLINE INLINE
 #endif
#else
 #define FORCE_INLINE
#endif

#endif /* PORT_H */
