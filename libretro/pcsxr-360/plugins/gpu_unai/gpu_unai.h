/***************************************************************************
*   Copyright (C) 2010 PCSX4ALL Team                                      *
*   Copyright (C) 2010 Unai                                               *
*   Copyright (C) 2016 Senquack (dansilsby <AT> gmail <DOT> com)          *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef GPU_UNAI_H
#define GPU_UNAI_H

/* Xbox 360 port (pcsxr-360):
 *
 * Adapted from upstream PCSX-ReARMed gpu_unai.h.  Changes from
 * upstream are flagged with `[XBOX360]` comments.  The structural
 * shape is preserved so future merges from upstream are mechanical.
 *
 * Key differences:
 *   - GCC `__attribute__((aligned(N)))` → MSVC `__declspec(align(N))`
 *     (prefix on the struct rather than suffix).
 *   - GCC `__attribute__((always_inline))` → MSVC `__forceinline` via
 *     INLINE/GPU_INLINE macros (defined in gpu_unai_compat.h).
 *   - HAVE_ARMV6 paths are ALWAYS dead on Xbox 360; the `#elif` chain
 *     for gcol_t falls into the generic-LP64 branch (unai port uses
 *     PPC32 which is __SIZEOF_SIZE_T__==4 → second branch).
 *   - LE32TOH/HTOLE32/LE16TOH/HTOLE16 are defined in gpu_unai_compat.h
 *     as byteswaps (PPC is BE; PSX VRAM is LE-stored in psxVuw).
 *   - `gpu_unai.vram` will point to pcsxr-360's psxVuw, not gpulib's
 *     own buffer.  The driver bridge sets this in renderer_init().
 *   - USE_GPULIB is NOT defined for the pcsxr-360 port; the older
 *     standalone gpu_unai variables (DisplayArea, dma, frameskip)
 *     stay in the struct for ABI compatibility but are wired to no-ops
 *     by the driver — pcsxr-360 owns DMA/display/frameskip elsewhere.
 *     Setting USE_GPULIB compiles them out entirely.
 */

#include <stdint.h>
#include <string.h>
#include "gpu_unai_compat.h"
#include "gpu.h"

/* Header shared between both standalone gpu_unai (gpu.cpp) and new
 * gpulib-compatible gpu_unai (gpulib_if.cpp)
 * -> Anything here should be for gpu_unai's private use. <- */

/* [XBOX360] Force the gpulib-style codepath since that's the modern
 * one we're bridging to.  This drops the dma/display/frameskip
 * members that pcsxr-360 doesn't need (their state lives in
 * libpcsxcore/psxdma.c and the libretro_core.cpp frame loop). */
#ifndef USE_GPULIB
#define USE_GPULIB 1
#endif

/* ===========================================================================
 *  Compile Options
 * =========================================================================*/

/* [XBOX360] Use integer math with accurate division.  Float math
 * (GPU_UNAI_USE_FLOATMATH) tends to produce identical output but pays
 * for FP→int conversions on every span; on Xenon that's not free.
 * Stick with the integer path. */
#undef GPU_UNAI_USE_FLOATMATH
#undef GPU_UNAI_USE_FLOAT_DIV_MULTINV
#undef GPU_UNAI_USE_INT_DIV_MULTINV

/* [XBOX360] Disable old standalone renderer to keep the codebase
 * minimal — only the modern (post-2016 senquack) path is ported. */
#define GPU_UNAI_NO_OLD 1

/* [XBOX360] INLINE / GPU_INLINE come from gpu_unai_compat.h and map
 * to `static __forceinline`.  Upstream defines them as static inline
 * with __attribute__((always_inline)). */

/* [XBOX360] u8/s8/.../u64 typedefs.  Upstream uses these names
 * throughout.  pcsxr-360 elsewhere uses identical typedefs (libretro,
 * libpcsxcore), so to avoid clashes wrap them in include guard. */
#ifndef PCSXR_BASIC_TYPES_DEFINED
#define PCSXR_BASIC_TYPES_DEFINED
typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint16_t u16;
typedef int16_t  s16;
typedef uint32_t u32;
typedef int32_t  s32;
typedef int64_t  s64;
typedef uint64_t u64;
#endif

/* ===========================================================================
 *  gcol_t — packed RGB color used by Gouraud interpolation.
 *
 *  Upstream has three layouts gated on HAVE_ARMV6 / SIZEOF_SIZE_T==4.
 *  On Xbox 360 PPC32 we land in the "32-bit pointer, no ARMv6"
 *  branch: c.{r,g,b,unused} as four u16, raw32[2] as the union, and
 *  the operator+= adds the two halves separately so the unused upper
 *  bits don't carry across.
 * =========================================================================*/

union gcol_t {
	struct {
		u16 r, g, b;
		u16 unused;             /* No HAVE_ARMV6 → spare slot, not a counter */
	} c;
	u32 raw32[2];               /* PPC32: SIZEOF_SIZE_T == 4 */

	inline gcol_t & operator+=(const gcol_t &rhs)
	{
		/* [XBOX360] Add halves separately.  No carry needed across
		 * 16-bit lanes because the unused slot absorbs any spill. */
		raw32[0] += rhs.raw32[0];
		raw32[1] += rhs.raw32[1];
		return *this;
	}

	inline void set_counter(int /*counter*/) { /* HAVE_ARMV6-only, no-op */ }
	inline void get_counter(int &/*counter*/) { /* HAVE_ARMV6-only, no-op */ }
};

/* ===========================================================================
 *  Little-endian wrapper types
 *
 *  Upstream uses these in debug builds (NDEBUG NOT defined) to wrap
 *  every LE-stored u16/u32 with a sentinel struct so accidental
 *  raw-typed access fails to compile.  In NDEBUG (release) builds the
 *  types collapse to plain u16/u32 and LE accessors are no-ops in LE
 *  hosts.  We always use LE16TOH/LE32TOH from gpu_unai_compat.h on the
 *  read side (byteswap on PPC).
 * =========================================================================*/

#ifndef NDEBUG

typedef struct { u32 v; } le32_t;
typedef struct { u16 v; } le16_t;
#define LExRead(v_) (v_.v)

#else

typedef u32 le32_t;
typedef u16 le16_t;
#define LExRead(v) (v)

#endif

static __forceinline u32 le32_to_u32(le32_t le)  { return LE32TOH(LExRead(le)); }
static __forceinline s32 le32_to_s32(le32_t le)  { return (s32)LE32TOH(LExRead(le)); }
static __forceinline u32 le32_raw   (le32_t le)  { return LExRead(le); }
static __forceinline le32_t u32_to_le32(u32 u)
{
	le32_t r;
#ifndef NDEBUG
	r.v = HTOLE32(u);
#else
	r = HTOLE32(u);
#endif
	return r;
}
static __forceinline u16 le16_to_u16(le16_t le)  { return LE16TOH(LExRead(le)); }
static __forceinline s16 le16_to_s16(le16_t le)  { return (s16)LE16TOH(LExRead(le)); }
static __forceinline u16 le16_raw   (le16_t le)  { return LExRead(le); }
static __forceinline le16_t u16_to_le16(u16 u)
{
	le16_t r;
#ifndef NDEBUG
	r.v = HTOLE16(u);
#else
	r = HTOLE16(u);
#endif
	return r;
}

/* Pointer puns into the GP0 packet buffer. */
union PtrUnion
{
	le32_t  *U4;
	le16_t  *U2;
	u8   *U1;
	void *ptr;
};

/* GP0 packet accumulator — up to 64 bytes (16 words). */
union GPUPacket
{
	le32_t U4[16];
	le16_t U2[32];
	u8  U1[64];
};

template<class T> static __forceinline void SwapValues(T &x, T &y)
{
	T tmp(x);  x = y;  y = tmp;
}

template<typename T> static __forceinline T Min2 (const T a, const T b)             { return (a<b)?a:b; }
template<typename T> static __forceinline T Min3 (const T a, const T b, const T c)  { return Min2(Min2(a,b),c); }
template<typename T> static __forceinline T Max2 (const T a, const T b)             { return (a>b)?a:b; }
template<typename T> static __forceinline T Max3 (const T a, const T b, const T c)  { return Max2(Max2(a,b),c); }

/* ===========================================================================
 *  GPU Raster Macros
 * =========================================================================*/

/* Convert 24bpp color parameter of GPU command to 16bpp (15bpp + mask bit). */
#define	GPU_RGB16(rgb) ((((rgb)&0xF80000)>>9)|(((rgb)&0xF800)>>6)|(((rgb)&0xF8)>>3))

/* Sign-extend 11-bit coordinate command param. */
#define GPU_EXPANDSIGN(x) (((s32)(x)<<(32-11))>>(32-11))

/* Max difference between any two X or Y primitive coordinates. */
#define CHKMAX_X 1024
#define CHKMAX_Y 512

#define	FRAME_BUFFER_SIZE     (1024*512*2)
#define	FRAME_WIDTH           1024
#define	FRAME_HEIGHT          512
#define	FRAME_OFFSET(x,y)     (((y)<<10)+(x))
#define FRAME_BYTE_STRIDE     2048
#define FRAME_BYTES_PER_PIXEL 2

static __forceinline s32 GPU_DIV(s32 rs, s32 rt)
{
	return rt ? (rs / rt) : (0);
}

/* 'Unsafe' version of above that doesn't check for div-by-zero. */
#define GPU_FAST_DIV(rs, rt) ((signed)(rs) / (signed)(rt))

/* ===========================================================================
 *  gpu_unai_inner_t — hot inner-loop parameters.
 *
 *  Upstream pins this with __attribute__((aligned(32))).  We mirror
 *  it via __declspec(align(32)) on the struct definition (MSVC syntax
 *  requires the alignment go BEFORE the struct keyword).
 *
 *  [XBOX360] The asm-friendly layout (offsets noted in upstream
 *  comments) doesn't matter on PPC because we don't load it from .S;
 *  preserved unchanged so future upstream merges stay mechanical.
 * =========================================================================*/

__declspec(align(32))
struct gpu_unai_inner_t {
	le16_t* TBA;              /* 00 Ptr to current texture in VRAM */
	le16_t* CBA;              /* 04 Ptr to current CLUT in VRAM */

	/* 22.10 Fixed-pt texture coords, mask, scanline advance.
	 * NOTE: U,V are no longer packed together into one u32, this proved to be
	 *  too imprecise, leading to pixel dropouts.  Example: NFS3's skybox. */
	u32 u, v;                 /* 08 not fractional for sprites */
	u32 mask_v00u;            /* 10 (v_mask << 24) | (u_mask & 0xff) */
	u32 unused;
	union {
	  struct {
	    s32 u_inc, v_inc;     /* 18 poly uv increment, 22.10 */
	  };
	  struct {
	    s32 y0, y1;           /* 18 sprite y range */
	  };
	};

	/* Color for flat-shaded, texture-blended prims */
	u8  r5, g5, b5, pad5;     /* 20 5-bit light for sprite asm */
	union {
	  u32 bgr0888;            /* 24 8-bit light for dithered prims */
	  struct {
	    /* [XBOX360] PPC is BE → use the BE branch from upstream. */
	    u8 pad8, b8, g8, r8;
	  };
	};

	/* Color for Gouraud-shaded prims.  Fixed-pt 8.8 rgb triplet
	 *  layout:  ccccccccXXXXXXXX for c in [r, g, b]
	 *           ^ bit 16 */
	gcol_t gCol;       /* 28 */
	gcol_t gInc;       /* 30 Increment along scanline for gCol */

	/* Color for flat-shaded, untextured prims */
	u16 PixelData;     /* 38 bgr555 color for untextured flat-shaded polys */

	u8 unused2;

	u8 ilace_mask;     /* Determines what lines to skip when rendering. */
};

/* ===========================================================================
 *  gpu_unai_t — full GPU state.
 *
 *  Upstream pins it to 32-byte alignment for ARM.  On PPC32 we keep
 *  the same alignment via __declspec(align(32)) on the type and the
 *  static instance below.
 * =========================================================================*/

__declspec(align(32))
struct gpu_unai_t {
	u32 GPU_GP1;
	GPUPacket PacketBuffer;
	le16_t *vram;

#ifdef USE_GPULIB
	le16_t *downscale_vram;
#endif
	/* ----------------------------------------------------------------------
	 * Variables used only by older standalone version of gpu_unai (gpu.cpp).
	 * GPU_UNAI_NO_OLD is set on Xbox 360 → these get compiled out. */
#ifndef USE_GPULIB
	u32  GPU_GP0;
	u32  tex_window;
	s32  PacketCount;
	s32  PacketIndex;
	bool fb_dirty;

	u16  DisplayArea[6];

	struct {
		s32  px,py;
		s32  x_end,y_end;
		le16_t* pvram;
		u32 *last_dma;
		bool FrameToRead;
		bool FrameToWrite;
	} dma;

	struct {
		int  skipCount;
		bool isSkip;
		bool skipFrame;
		bool wasSkip;
		bool skipGPU;
	} frameskip;
#endif

	u32 TextureWindowCur;
	u8  TextureWindow[4];      /* offsetX, offsetY, maskX, maskY */

	u16 DrawingArea[4];        /* topLeftX, topLeftY, bottomRightX, bottomRightY */
	s16 DrawingOffset[2];      /* X, Y (signed) */

	gpu_unai_inner_t inn;      /* hot inner-loop params (32-byte aligned) */

	u32 DitherLut32[4];        /* shifted up by 4, packed into u32 as 4 values */
	s16 DitherLut16[4][4];     /* shifted up by 4 and s16 to simplify lookup asm */

	bool prog_ilace_flag;      /* Tracks successive frames for 'prog_ilace' option */

	u8 BLEND_MODE;
	u8 TEXT_MODE;
	u8 Masking;

	u16 PixelMSB;

	gpu_unai_config_t config;

	u8  LightLUT[32*32];       /* 5-bit lighting LUT (gpu_inner_light.h) */
};

/* [XBOX360] Single instance lives in gpu_unai_driver.cpp (state owned
 * by the bridge layer).  Headers reference them via the externs.
 *
 * Both globals are declared with C linkage so the symbol names match
 * what libretro_core.cpp / xbox_soft expect (they include this from
 * inside `extern "C"` blocks).  The struct types themselves contain
 * C++ machinery (operator+= on gcol_t, templates pulled in by
 * gpu_inner.h) but that's fine — `extern "C"` controls the symbol's
 * name mangling, not the type's internal C++ features.
 *
 * `gpu_unai_config_ext` is also declared in gpu.h (the public-facing
 * config header).  Both declarations agree on C linkage so the
 * compiler doesn't emit conflicting references. */
extern "C" {
    extern __declspec(align(32)) gpu_unai_t gpu_unai;
    extern gpu_unai_config_t gpu_unai_config_ext;
}

/* ===========================================================================
 * Internal inline helpers — query option status.
 * =========================================================================*/

static __forceinline bool LightingEnabled()         { return gpu_unai.config.lighting != 0; }
static __forceinline bool FastLightingEnabled()     { return gpu_unai.config.fast_lighting != 0; }
static __forceinline bool BlendingEnabled()         { return gpu_unai.config.blending != 0; }
static __forceinline bool DitheringEnabled()        { return gpu_unai.config.dithering != 0; }
static __forceinline bool ForcedDitheringEnabled()  { return gpu_unai.config.force_dithering != 0; }

static __forceinline bool ProgressiveInterlaceEnabled()
{
#ifdef USE_GPULIB
	/* Disabled when using gpulib — adds work for negligible quality gain. */
	return false;
#else
	return gpu_unai.prog_ilace_flag;
#endif
}

#endif /* GPU_UNAI_H */
