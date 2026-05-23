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

/* [XBOX360] Build LightDitherMul: tex5 * light5 * 8 sin cuantizar ni clampar.
 *
 * Esta tabla sustituye las 3 muls/pixel del dither hot path por 3 loads.
 * El factor *8 alinea el rango con el shift >>7 de la formula original:
 *
 *   Original 8-bit light:  r3 = (uSrc&0x1F) * light8 + dv;  out = clamp(r3 >> 7)
 *   Nuevo    5-bit light:  r3 = LUT[(tex5 << 5) | light5] + dv;  out = clamp(r3 >> 7)
 *
 * donde LUT[x] = tex5 * light5 * 8.  Equivalente exacto al cambio light8 ->
 * light5*8 en la formula original (precision de luz baja de 8b a 5b, dither
 * mantiene su precision intacta).
 *
 * Rango maximo: 31 * 31 * 8 = 7688, comodamente dentro de s16. */
static void SetupLightDitherMul()
{
	for (int t = 0; t < 32; t++) {
		for (int l = 0; l < 32; l++) {
			gpu_unai.LightDitherMul[(t << 5) | l] = (s16)(t * l * 8);
		}
	}
}

GPU_INLINE s32 clamp_c(s32 x) {
	/* Saturate to [0, 31].
	 *
	 * En PPC (Xenos) usamos la forma ternaria explicita: cmpwi+isel del
	 * compilador genera 4 instrucciones (2 cmp + 2 isel, single-cycle
	 * cada una) por clamp.  Se invoca 3 veces (R/G/B) por pixel dentro
	 * del dither hot path, asi que el ahorro es notable (de 7 ALU ->
	 * 4 ALU por clamp, -9 ALU por pixel dither).
	 *
	 * En otras plataformas el fallback usa el truco clasico branchless
	 * con shift aritmetico y andc:
	 *   - `x >> 31` (shift aritmetico) replica el signo: -1 si x<0, 0
	 *     si x>=0.
	 *   - `x & ~(x >> 31)` -> 0 si x<0, x si x>=0  (= max(x,0)).
	 *   - Repetido sobre `(31 - x)` clampa al limite alto.
	 *  Cero branches, 7 ALU (shifts + ands + ors). */
#if defined(_XBOX) || defined(__PPC__) || defined(__powerpc__) || defined(_M_PPC)
	/* PPC isel via ternario: cmpwi cr0, x, 0 ; isel ... ; cmpwi cr0, x, 31 ; isel ... */
	s32 a = (x < 0) ? 0 : x;
	return (a > 31) ? 31 : a;
#else
	x = x & ~(x >> 31);
	s32 over = 31 - x;
	x = (x & ~(over >> 31)) | (31 & (over >> 31));
	return x;
#endif
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

/* Apply high-precision 8-bit lighting to bgr555 texture color.
 *
 * Original upstream version: 3 muls + 3 adds + 3 shifts + 3 clamps por pixel
 * (~35 ALU + 12 ciclos de mul = ~50 ciclos en Xenos).  La macro
 * `GPU_UNAI_FAST_DITHER` (default ON en Xbox 360) selecciona la version
 * rapida que sustituye las 3 muls por 3 LUT loads (LightDitherMul) y baja
 * la precision de luz a 5-bit, manteniendo el dither estructural intacto.
 *
 * Si necesitas debuggear o comparar visualmente, define
 * GPU_UNAI_FAST_DITHER=0 antes de incluir este header para usar la version
 * upstream tradicional. */
#if !defined(GPU_UNAI_FAST_DITHER)
# if defined(_XBOX) || defined(__PPC__) || defined(__powerpc__) || defined(_M_PPC)
#   define GPU_UNAI_FAST_DITHER 1
# else
#   define GPU_UNAI_FAST_DITHER 0
# endif
#endif

#if GPU_UNAI_FAST_DITHER

/* Path rapido: LUT pre-multiplicada (2 KB) + dither + clamp.
 *
 * Coste por pixel: 3 loads (~3 ciclos cada uno con L1 hit) + 3 adds + 3
 * shifts + 3 clamps con isel (4 ALU) = ~25 ciclos.  La mitad que el path
 * original.
 *
 * Toma los componentes de luz YA en 5-bit (los call-sites hacen >>3 o >>11
 * segun sea bgr0888 o gcol_t).  Esto unifica el lookup con la indexacion
 * `(tex5 << 5) | light5`. */
GPU_INLINE uint_fast16_t gpuLightingTXTDitherRGB(uint_fast16_t uSrc,
	uint_fast8_t r5, uint_fast8_t g5, uint_fast8_t b5, int_fast16_t dv)
{
	uint_fast16_t rs = uSrc & 0x001F;
	uint_fast16_t gs = (uSrc >> 5) & 0x001F;
	uint_fast16_t bs = (uSrc >> 10) & 0x001F;

	s32 r3 = gpu_unai.LightDitherMul[(rs << 5) | r5] + dv;
	s32 g3 = gpu_unai.LightDitherMul[(gs << 5) | g5] + dv;
	s32 b3 = gpu_unai.LightDitherMul[(bs << 5) | b5] + dv;

	return  clamp_c(r3 >> 7) |
	       (clamp_c(g3 >> 7) << 5) |
	       (clamp_c(b3 >> 7) << 10) |
	       (uSrc & 0x8000);
}

GPU_INLINE uint_fast16_t gpuLightingTXTDither(uint_fast16_t uSrc, u32 bgr0888, int_fast16_t dv)
{
	/* bgr0888 layout: R en byte 0, G en byte 1, B en byte 2.  Tomamos los
	 * top 5 bits de cada uno (>>3) para indexar LightDitherMul. */
	return gpuLightingTXTDitherRGB(uSrc,
		( bgr0888        & 0xff) >> 3,
		((bgr0888 >> 8)  & 0xff) >> 3,
		( bgr0888 >> 16)         >> 3,
		dv);
}

GPU_INLINE uint_fast16_t gpuLightingTXTGouraudDither(uint_fast16_t uSrc, gcol_t gCol, int_fast8_t dv)
{
	/* gCol es fixed-pt 8.8; gCol.c.r >> 11 da los 5 bits altos de luz
	 * (igual que en gpuLightingRGB y gpuLightingTXTGouraudGeneric). */
	return gpuLightingTXTDitherRGB(uSrc,
		gCol.c.r >> 11, gCol.c.g >> 11, gCol.c.b >> 11, dv);
}

#else /* !GPU_UNAI_FAST_DITHER */

/* Path original upstream (precision de luz 8-bit, 3 muls/pixel).
 * Mantenido para builds non-PPC y como referencia visual. */
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

#endif /* GPU_UNAI_FAST_DITHER */

#endif  /* _OP_LIGHT_H_ */
