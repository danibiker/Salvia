/***************************************************************************
 *   Copyright (C) 2010 PCSX4ALL Team                                      *
 *   Copyright (C) 2010 Unai                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef FIXED_H
#define FIXED_H

/* Xbox 360 port (pcsxr-360):
 *
 * Direct port from upstream gpu_unai/gpu_fixedpoint.h.  Differences:
 *
 *   - GCC inline asm for `clz` (Log2) replaced with MSVC PPC intrinsic
 *     `_CountLeadingZeros` (or fallback C loop).  Path only used when
 *     GPU_UNAI_USE_INT_DIV_MULTINV is defined; we don't enable that
 *     optimization on Xbox 360 (gpu_unai.h `#undef`s it).  Keeping
 *     the code anyway in case a future build flips it on.
 *
 *   - GCC SMULL inline asm in xInvMulx replaced with native s64 mul.
 *     MSVC PPC compiles `(s64)a * (s64)b >> N` to `mullw + mulhw +
 *     srawi` which is the canonical sequence (3 instructions, same
 *     latency as ARM SMULL).
 *
 *   - INLINE comes from gpu_unai_compat.h (= `static __forceinline`).
 */

#include "gpu_unai_compat.h"

typedef s32 fixed;

/* senquack: 22.10 fixed-point (was 16.16 in original Unai).
 * gpu_drhell-derived poly routines need higher integer range, hence
 * the change. */
#define FIXED_BITS 10

#define fixed_ZERO ((fixed)0)
#define fixed_ONE  ((fixed)1<<FIXED_BITS)
#define fixed_TWO  ((fixed)2<<FIXED_BITS)
#define fixed_HALF ((fixed)((1<<FIXED_BITS)>>1))

#define fixed_LOMASK ((fixed)((1<<FIXED_BITS)-1))
#define fixed_HIMASK ((fixed)(~fixed_LOMASK))

/* int<->fixed conversions */
#define i2x(x) ((x)<<FIXED_BITS)
#define x2i(x) ((x)>>FIXED_BITS)

INLINE fixed FixedCeil(const fixed x)
{
	return (x + (fixed_ONE - 1)) & fixed_HIMASK;
}

INLINE s32 FixedCeilToInt(const fixed x)
{
	return (x + (fixed_ONE - 1)) >> FIXED_BITS;
}

/* float<->fixed conversions */
#define f2x(x) ((s32)((x) * (float)(1<<FIXED_BITS)))
#define x2f(x) ((float)(x) / (float)(1<<FIXED_BITS))

/* ===========================================================================
 * Inverse-table-based division.
 *
 * Upstream uses these only when GPU_UNAI_USE_INT_DIV_MULTINV is defined,
 * which we keep OFF on Xbox 360 (the standard signed-int division
 * pipeline of the Xenon is fast enough; the inverse table introduces
 * its own L1 misses).  Code preserved for completeness.
 * =========================================================================*/

#if defined(GPU_UNAI_USE_INT_DIV_MULTINV) || (!defined(GPU_UNAI_NO_OLD) && !defined(GPU_UNAI_USE_FLOATMATH))

/* Big-precision inverse table. */
#define TABLE_BITS 16
extern s32 s_invTable[(1<<TABLE_BITS)];

#endif

#ifdef GPU_UNAI_USE_INT_DIV_MULTINV

/* PowerPC has cntlzw (count leading zeros word) — same semantics as
 * ARM CLZ.  Implemented via MSVC intrinsic _CountLeadingZeros. */
#if defined(_M_PPC) || defined(_XBOX)
INLINE u32 Log2(u32 x)
{
	/* _CountLeadingZeros returns 0..32; matching upstream's `32-clz`. */
	extern unsigned long _CountLeadingZeros(unsigned long);
	return 32 - _CountLeadingZeros((unsigned long)x);
}
#else
INLINE u32 Log2(u32 x)
{
	u32 i = 0;
	for ( ; x > 0; ++i, x >>= 1)
		;
	return i - 1;
}
#endif

INLINE void xInv(const fixed _b, s32 &iFactor_, s32 &iShift_)
{
	u32 uD = (_b<0) ? -_b : _b;
	if (uD>1) {
		u32 uLog = Log2(uD);
		uLog = uLog>(TABLE_BITS-1) ? uLog-(TABLE_BITS-1) : 0;
		u32 uDen = (uD>>uLog);
		iFactor_ = s_invTable[uDen];
		iFactor_ = (_b<0) ? -iFactor_ : iFactor_;
		/* 22.10 fixed-pt (originally 16.16). */
		iShift_  = 21+uLog;
	} else {
		iFactor_ = _b;
		iShift_  = 0;
	}
}

INLINE fixed xInvMulx(const fixed _a, const s32 _iFact, const s32 _iShift)
{
	/* PPC32: native s64 = s32*s32 → mullw+mulhw, then srawi.  Same
	 * sequence GCC would emit for ARM via SMULL on Cortex-A8/A9. */
	return (fixed)(((s64)_a * (s64)_iFact) >> _iShift);
}

INLINE fixed xLoDivx(const fixed _a, const fixed _b)
{
	s32 iFact, iShift;
	xInv(_b, iFact, iShift);
	return xInvMulx(_a, iFact, iShift);
}

#endif /* GPU_UNAI_USE_INT_DIV_MULTINV */

#endif /* FIXED_H */
