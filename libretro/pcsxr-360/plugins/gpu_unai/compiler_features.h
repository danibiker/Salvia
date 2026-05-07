/* compiler_features.h — Xbox 360 MSVC PPC stub.
 *
 * Replacement for pcsx_rearmed/include/compiler_features.h.  Upstream
 * relies on GCC builtins (__builtin_expect/clz, __attribute__((noinline)),
 * etc.) that VS2010 doesn't have.  This shim provides MSVC-friendly
 * equivalents — most of them are already pulled in via gpu_unai_compat.h,
 * but this header makes the gpu_inner.h `#include "compiler_features.h"`
 * line resolve without dragging GCC-specific definitions.
 */
#ifndef PCSX_COMPILER_FEATURES_H_
#define PCSX_COMPILER_FEATURES_H_

#include "gpu_unai_compat.h"

/* Re-export the macros gpu_unai_compat.h already defined.  Guards mean
 * if the user builds with gpu_unai_compat.h not yet included, this
 * file alone supplies them.  Both paths converge on the same
 * definitions. */

#ifndef likely
#  define likely(x)   (x)
#endif
#ifndef unlikely
#  define unlikely(x) (x)
#endif
#ifndef preload
#  define preload(p)  ((void)0)
#endif
#ifndef noinline
#  define noinline    __declspec(noinline)
#endif
#ifndef attr_unused
#  define attr_unused
#endif
#ifndef nosanitize
#  define nosanitize(x)
#endif
#ifndef attr_weak
#  define attr_weak
#endif

/* Overflow-checked add/sub.  Upstream uses __builtin_add_overflow with
 * a fallback on the GCC code path.  MSVC has no exact equivalent for
 * 32-bit signed overflow detection without inline asm or
 * SafeIntInternal.h, but unai only uses these in geometry clipping
 * paths that we may or may not exercise on Xbox 360.  For now we
 * provide the same fallback expression the upstream falls back to,
 * wrapped to avoid the GCC statement-expression syntax `({ ... })`
 * that MSVC doesn't accept. */

static __forceinline int gpu_unai_add_overflow_s32(int a, int b, int* r)
{
    unsigned int ua = (unsigned int)a;
    unsigned int ub = (unsigned int)b;
    unsigned int ur = ua + ub;
    *r = (int)ur;
    /* Overflow if signs of a,b match but result differs. */
    return (int)((ua ^ ~ub) & (ua ^ ur) & 0x80000000u);
}

static __forceinline int gpu_unai_sub_overflow_s32(int a, int b, int* r)
{
    unsigned int ua = (unsigned int)a;
    unsigned int ub = (unsigned int)b;
    unsigned int ur = ua - ub;
    *r = (int)ur;
    return (int)((ua ^ ub) & (ua ^ ur) & 0x80000000u);
}

#define add_overflow(a, b, r) gpu_unai_add_overflow_s32((int)(a), (int)(b), (int*)&(r))
#define sub_overflow(a, b, r) gpu_unai_sub_overflow_s32((int)(a), (int)(b), (int*)&(r))

#endif /* PCSX_COMPILER_FEATURES_H_ */
