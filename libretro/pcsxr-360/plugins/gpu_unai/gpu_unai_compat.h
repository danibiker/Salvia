/* gpu_unai_compat.h
 *
 * Adapter layer for porting gpu_unai (PCSX-ReARMed) to Xbox 360 / pcsxr-360.
 *
 * Upstream gpu_unai targets ARM/MIPS/x86 with GCC/clang.  This shim
 * maps GCC-isms and platform helpers to their MSVC PPC equivalents so
 * the unai sources compile under the Xbox 360 XDK (VS2010) without
 * touching the algorithmic code.
 *
 * Scope:
 *   - GCC attribute syntax → MSVC __declspec/__forceinline.
 *   - GCC builtins (likely/unlikely/prefetch/clz) → no-op or intrinsic.
 *   - LE host helpers (LE16TOH/HTOLE16/LE32TOH/HTOLE32) → byteswap on
 *     PPC BE.
 *   - Suppress arm_features.h / compiler_features.h pulls — provide
 *     local stubs in this directory.
 *
 * Convention: this header MUST be included BEFORE any unai source so
 * the macros are in scope when those files reference them.
 */
#ifndef GPU_UNAI_COMPAT_H
#define GPU_UNAI_COMPAT_H

#include <stdint.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Endianness
 * -----------------------------------------------------------------------*/

/* Xbox 360 PowerPC is big-endian.  PSX VRAM (psxVuw, shared with
 * xbox_soft) is stored in PSX little-endian byte order — same
 * convention upstream unai uses.  Conversion macros below mirror those
 * unai expects (LE16TOH = "little-endian-16-to-host"). */

#if defined(_XBOX) || (defined(_MSC_VER) && defined(_M_PPC))
#  define UNAI_HOST_BIG_ENDIAN 1
#else
#  define UNAI_HOST_BIG_ENDIAN 0
#endif

#if UNAI_HOST_BIG_ENDIAN
#  if defined(_MSC_VER)
#    include <stdlib.h>     /* _byteswap_ushort/_ulong */
#    define UNAI_BSWAP16(x) _byteswap_ushort((unsigned short)(x))
#    define UNAI_BSWAP32(x) _byteswap_ulong((unsigned long)(x))
#  else
#    define UNAI_BSWAP16(x) ((uint16_t)(((((uint16_t)(x)) & 0x00ff) << 8) | \
                                       ((((uint16_t)(x)) & 0xff00) >> 8)))
#    define UNAI_BSWAP32(x) ((uint32_t)(((((uint32_t)(x)) & 0x000000ffu) << 24) | \
                                       ((((uint32_t)(x)) & 0x0000ff00u) <<  8) | \
                                       ((((uint32_t)(x)) & 0x00ff0000u) >>  8) | \
                                       ((((uint32_t)(x)) & 0xff000000u) >> 24)))
#  endif
#  define LE16TOH(x) UNAI_BSWAP16(x)
#  define HTOLE16(x) UNAI_BSWAP16(x)
#  define LE32TOH(x) UNAI_BSWAP32(x)
#  define HTOLE32(x) UNAI_BSWAP32(x)
#else
#  define LE16TOH(x) ((uint16_t)(x))
#  define HTOLE16(x) ((uint16_t)(x))
#  define LE32TOH(x) ((uint32_t)(x))
#  define HTOLE32(x) ((uint32_t)(x))
#endif

/* -------------------------------------------------------------------------
 * Compiler attribute mapping
 *
 * Upstream uses GCC-style __attribute__((aligned/always_inline/noinline)).
 * MSVC PPC uses __declspec(align)/__forceinline/__declspec(noinline).
 * -----------------------------------------------------------------------*/

#if defined(_MSC_VER)
   /* __attribute__((aligned(N))) on a struct/union → __declspec(align(N))
    * on the type.  We can't fully emulate the GCC suffix form, so the
    * unai struct definitions need an inline edit (see gpu_unai.h port).
    * For local variables, MSVC equivalent is __declspec(align(N)) prefix.
    */
#  ifndef __forceinline
   /* MSVC defines __forceinline as a keyword, but some XDK config knobs
    * disable it; provide a fallback to inline. */
#    define __forceinline __inline
#  endif

#  ifndef noinline
#    define noinline   __declspec(noinline)
#  endif
#  ifndef attr_unused
#    define attr_unused
#  endif
#  ifndef nosanitize
#    define nosanitize(x)
#  endif
#  ifndef likely
#    define likely(x)   (x)
#  endif
#  ifndef unlikely
#    define unlikely(x) (x)
#  endif
#  ifndef preload
#    define preload(p)  ((void)0)
#  endif
#  ifndef attr_weak
#    define attr_weak
#  endif
#endif

/* unai uses bare `inline` and assumes "always inline" semantics.  MSVC
 * respects bare `inline` only with /O2 and not always for templates;
 * unai's INLINE macro pins it explicitly. */
#ifndef GPU_INLINE
#  define GPU_INLINE static __forceinline
#endif
#ifndef INLINE
#  define INLINE     static __forceinline
#endif

/* -------------------------------------------------------------------------
 * Platform feature flags
 *
 * Xbox 360 is PPC32 big-endian, NOT ARM and NOT MIPS.  Make sure none
 * of the ARM-specific code paths get pulled in.  The original unai
 * sources gate ARM asm behind __arm__ / HAVE_ARMV6 / HAVE_NEON; on
 * MSVC/PPC none of these are defined, so the generic-C paths are used.
 * -----------------------------------------------------------------------*/

#ifdef __arm__
#  error "gpu_unai port targets PowerPC, not ARM."
#endif

/* Some files unai includes wrap their code with `#ifdef HAVE_ARMV6` or
 * similar; make sure those macros are NOT defined inadvertently. */
#undef HAVE_ARMV5
#undef HAVE_ARMV5E
#undef HAVE_ARMV6
#undef HAVE_ARMV7
#undef HAVE_NEON32
#undef HAVE_PRE_ARMV7

/* -------------------------------------------------------------------------
 * MSVC namespace cleanup
 *
 * Some XDK headers (winnt.h, etc.) define `min`/`max` as macros that
 * conflict with std::min/max and unai's Min2/Min3/Max2/Max3 templates.
 * We do NOT undef them here because that's the SDK's responsibility;
 * but DO advise that callers either define NOMINMAX project-wide or
 * use the explicit Min2/Max2 helpers. */

#endif /* GPU_UNAI_COMPAT_H */
