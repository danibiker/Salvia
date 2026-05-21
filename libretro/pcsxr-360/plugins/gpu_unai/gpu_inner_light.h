/***************************************************************************
*   Copyright (C) 2016 PCSX4ALL Team                                      *
*   Copyright (C) 2010 Unai                                               *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

/* Xbox 360 port (pcsxr-360):
 *
 * Direct port from upstream gpu_inner_light.h.  Changes:
 *   - Compound-literal syntax `(gcol_t){{ ... }}` is C99/GCC; replaced
 *     with explicit local construction so MSVC C++03 accepts it.
 *   - Added uint_fast8_t / int_fast8_t / int_fast16_t / uint_fast32_t
 *     fallbacks via gpu_unai_compat.h pull (typedefs to plain int).
 */

#ifndef _OP_LIGHT_H_
#define _OP_LIGHT_H_

#include "gpu_unai_compat.h"

/* GPU color operations for lighting calculations */

static void SetupLightLUT()
{
	/* 1024-entry lookup table that modulates 5-bit texture + 5-bit light value.
	 * A light value of 15 does not modify the incoming texture color. */
	for (int j=0; j < 32; ++j) {
		for (int i=0; i < 32; ++i) {
			int val = i * j / 16;
			if (val > 31) val = 31;
			gpu_unai.LightLUT[(j*32) + i] = val;
		}
	}
}

GPU_INLINE s32 clamp_c(s32 x) {
	/* Branchless saturate to [0, 31].
	 *
	 * El equivalente con branches:
	 *     if (x < 0)  return 0;
	 *     if (x > 31) return 31;
	 *     return x;
	 * tiene DOS branches por llamada y se invoca 3 veces (R/G/B) por pixel
	 * dentro del dither hot path. Sobre valores aleatorios eso son 6 branches
	 * por pixel impredecibles -> ~16 ciclos de mispredict cada uno en el
	 * Xenos (PPC in-order). Catastrofico para fillrate.
	 *
	 * Esta version usa dos trucos clasicos de saturacion entera signed:
	 *   - `x >> 31` con shift aritmetico replica el bit de signo en todos
	 *     los bits: -1 (0xFFFFFFFF) si x<0, 0 si x>=0.
	 *   - `x & ~(x >> 31)` -> 0 si x<0, x si x>=0.  Equivale a max(x, 0).
	 *   - Aplicado de nuevo sobre `(31 - x)` para clampar al limite alto:
	 *       sign(31-x) = -1 si x>31, 0 si x<=31.
	 *       resultado = (x & ~sign) | (31 & sign).
	 * Cero branches, 7 instrucciones ALU (shifts + ands + ors). */
	x = x & ~(x >> 31);
	s32 over = 31 - x;
	x = (x & ~(over >> 31)) | (31 & (over >> 31));
	return x;
}

/* Create packed Gouraud fixed-pt 8.8 rgb triplet.
 * Upstream uses GCC compound literal `(gcol_t){{ ... }}`; we use an
 * explicit local-variable construction for MSVC compatibility. */
GPU_INLINE gcol_t gpuPackGouraudCol(u32 r, u32 g, u32 b)
{
	gcol_t out;
	out.c.r = (u16)(r >> 2);
	out.c.g = (u16)(g >> 2);
	out.c.b = (u16)(b >> 2);
	out.c.unused = 0;
	return out;
}

/* Create packed increment for Gouraud fixed-pt 8.8 rgb triplet. */
GPU_INLINE gcol_t gpuPackGouraudColInc(s32 dr, s32 dg, s32 db)
{
	gcol_t out;
	out.c.r = (u16)((dr >> 2) + (dr < 0 ? 1 : 0));
	out.c.g = (u16)((dg >> 2) + (dg < 0 ? 1 : 0));
	out.c.b = (u16)((db >> 2) + (db < 0 ? 1 : 0));
	out.c.unused = 0;
	return out;
}

/* Extract bgr555 color from Gouraud u32 fixed-pt 8.8 rgb triplet. */
GPU_INLINE uint_fast16_t gpuLightingRGB(gcol_t gCol)
{
	return (gCol.c.r >> 11) |
		((gCol.c.g >> 6) & 0x3e0) |
		((gCol.c.b >> 1) & 0x7c00);
}

GPU_INLINE uint_fast16_t gpuLightingRGBDither(gcol_t gCol, int_fast16_t dt)
{
	dt <<= 4;
	return  clamp_c(((s32)gCol.c.r + dt) >> 11) |
	       (clamp_c(((s32)gCol.c.g + dt) >> 11) << 5) |
	       (clamp_c(((s32)gCol.c.b + dt) >> 11) << 10);
}

/* Apply fast (low-precision) 5-bit lighting to bgr555 texture color. */
GPU_INLINE uint_fast16_t gpuLightingTXTGeneric(uint_fast16_t uSrc, u32 bgr0888)
{
	/* The compiler can move this out of the loop if it wants to */
	uint_fast32_t b5 = (bgr0888 >> 19);
	uint_fast32_t g5 = (bgr0888 >> 11) & 0x1f;
	uint_fast32_t r5 = (bgr0888 >>  3) & 0x1f;

	return (gpu_unai.LightLUT[((uSrc&0x7C00)>>5) | b5] << 10) |
	       (gpu_unai.LightLUT[ (uSrc&0x03E0)     | g5] <<  5) |
	       (gpu_unai.LightLUT[((uSrc&0x001F)<<5) | r5]      ) |
	       (uSrc & 0x8000);
}


/* Apply fast (low-precision) 5-bit Gouraud lighting to bgr555 texture color. */
GPU_INLINE uint_fast16_t gpuLightingTXTGouraudGeneric(uint_fast16_t uSrc, gcol_t gCol)
{
	return (gpu_unai.LightLUT[((uSrc&0x7C00)>>5) | (gCol.c.b >> 11)] << 10) |
	       (gpu_unai.LightLUT[ (uSrc&0x03E0)     | (gCol.c.g >> 11)] << 5) |
	       (gpu_unai.LightLUT[((uSrc&0x001F)<<5) | (gCol.c.r >> 11)]) |
	       (uSrc & 0x8000);
}

/* Apply high-precision 8-bit lighting to bgr555 texture color. */
GPU_INLINE uint_fast16_t gpuLightingTXTDitherRGB(uint_fast16_t uSrc,
	uint_fast8_t r, uint_fast8_t g, uint_fast8_t b, int_fast16_t dv)
{
	uint_fast16_t rs = uSrc & 0x001F;
	uint_fast16_t gs = uSrc & 0x03E0;
	uint_fast16_t bs = uSrc & 0x7C00;
	s32 r3 = rs * r +  dv;
	s32 g3 = gs * g + (dv << 5);
	s32 b3 = bs * b + (dv << 10);
	return  clamp_c(r3 >> 7) |
	       (clamp_c(g3 >> 12) << 5) |
	       (clamp_c(b3 >> 17) << 10) |
	       (uSrc & 0x8000);
}

GPU_INLINE uint_fast16_t gpuLightingTXTDither(uint_fast16_t uSrc, u32 bgr0888, int_fast16_t dv)
{
	return gpuLightingTXTDitherRGB(uSrc, bgr0888 & 0xff,
			(bgr0888 >> 8) & 0xff, bgr0888 >> 16, dv);
}

GPU_INLINE uint_fast16_t gpuLightingTXTGouraudDither(uint_fast16_t uSrc, gcol_t gCol, int_fast8_t dv)
{
	return gpuLightingTXTDitherRGB(uSrc, gCol.c.r >> 8, gCol.c.g >> 8, gCol.c.b >> 8, dv);
}

#endif  /* _OP_LIGHT_H_ */
