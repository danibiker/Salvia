/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _GFX_H_
#define _GFX_H_

#include "port.h"
#include <vector>

struct SGFX
{
	static const uint32 Pitch = sizeof(uint16) * MAX_SNES_WIDTH;
	static const uint32 RealPPL = MAX_SNES_WIDTH; // true PPL of Screen buffer
	static const uint32 ScreenSize =  MAX_SNES_WIDTH * MAX_SNES_HEIGHT;
	std::vector<uint16> ScreenBuffer;
	uint16	*Screen;
	uint16	*SubScreen;
	uint8	*ZBuffer;
	uint8	*SubZBuffer;
	uint16	*S;
	uint8	*DB;
	uint16	*ZERO;
	uint32	PPL;				// number of pixels on each of Screen buffer
	uint32	LinesPerTile;		// number of lines in 1 tile (4 or 8 due to interlace)
	uint16	*ScreenColors;		// screen colors for rendering main
	uint16	*RealScreenColors;	// screen colors, ignoring color window clipping
	uint8	Z1;					// depth for comparison
	uint8	Z2;					// depth to save
	uint32	FixedColour;
	uint8	DoInterlace;
	uint32	StartY;
	uint32	EndY;
	bool8	ClipColors;
	uint8	OBJWidths[128];
	uint8	OBJVisibleTiles[128];

	struct ClipData	*Clip;

	struct
	{
		uint8	RTOFlags;
		int16	Tiles;

		struct
		{
			int8	Sprite;
			uint8	Line;
		}	OBJ[128];
	}	OBJLines[SNES_HEIGHT_EXTENDED];

	void	(*DrawBackdropMath) (uint32, uint32, uint32);
	void	(*DrawBackdropNomath) (uint32, uint32, uint32);
	void	(*DrawTileMath) (uint32, uint32, uint32, uint32);
	void	(*DrawTileNomath) (uint32, uint32, uint32, uint32);
	void	(*DrawClippedTileMath) (uint32, uint32, uint32, uint32, uint32, uint32);
	void	(*DrawClippedTileNomath) (uint32, uint32, uint32, uint32, uint32, uint32);
	void	(*DrawMosaicPixelMath) (uint32, uint32, uint32, uint32, uint32, uint32);
	void	(*DrawMosaicPixelNomath) (uint32, uint32, uint32, uint32, uint32, uint32);
	void	(*DrawMode7BG1Math) (uint32, uint32, int);
	void	(*DrawMode7BG1Nomath) (uint32, uint32, int);
	void	(*DrawMode7BG2Math) (uint32, uint32, int);
	void	(*DrawMode7BG2Nomath) (uint32, uint32, int);

	std::string InfoString;
	uint32	InfoStringTimeout;
	char	FrameDisplayString[256];
};

struct SBG
{
	uint8	(*ConvertTile) (uint8 *, uint32, uint32);
	uint8	(*ConvertTileFlip) (uint8 *, uint32, uint32);

	uint32	TileSizeH;
	uint32	TileSizeV;
	uint32	OffsetSizeH;
	uint32	OffsetSizeV;
	uint32	TileShift;
	uint32	TileAddress;
	uint32	NameSelect;
	uint32	SCBase;

	uint32	StartPalette;
	uint32	PaletteShift;
	uint32	PaletteMask;
	uint8	EnableMath;
	uint8	InterlaceLine;

	uint8	*Buffer;
	uint8	*BufferFlip;
	uint8	*Buffered;
	uint8	*BufferedFlip;
	bool8	DirectColourMode;
};

struct SLineData
{
	struct
	{
		uint16	VOffset;
		uint16	HOffset;
	}	BG[4];
};

struct SLineMatrixData
{
	short	MatrixA;
	short	MatrixB;
	short	MatrixC;
	short	MatrixD;
	short	CentreX;
	short	CentreY;
	short	M7HOFS;
	short	M7VOFS;
};

extern uint16		BlackColourMap[256];
extern uint16		DirectColourMaps[8][256];
extern uint8		mul_brightness[16][32];
extern uint8		brightness_cap[64];
extern struct SBG	BG;
extern struct SGFX	GFX;

#define H_FLIP		0x4000
#define V_FLIP		0x8000
#define BLANK_TILE	2

struct COLOR_ADD
{
	static alwaysinline uint16 fn(uint16 C1, uint16 C2)
	{
		const int RED_MASK = 0x1F << RED_SHIFT_BITS;
		const int GREEN_MASK = 0x1F << GREEN_SHIFT_BITS;
		const int BLUE_MASK = 0x1F;

		int rb = C1 & (RED_MASK | BLUE_MASK);
		rb += C2 & (RED_MASK | BLUE_MASK);
		int rbcarry = rb & ((0x20 << RED_SHIFT_BITS) | (0x20 << 0));
		int g = (C1 & (GREEN_MASK)) + (C2 & (GREEN_MASK));
		int rgbsaturate = (((g & (0x20 << GREEN_SHIFT_BITS)) | rbcarry) >> 5) * 0x1f;
		uint16 retval = (rb & (RED_MASK | BLUE_MASK)) | (g & GREEN_MASK) | rgbsaturate;
#if GREEN_SHIFT_BITS == 6
		retval |= (retval & 0x0400) >> 5;
#endif
		return retval;
	}

	static alwaysinline uint16 fn1_2(uint16 C1, uint16 C2)
	{
		return ((((C1 & RGB_REMOVE_LOW_BITS_MASK) +
			(C2 & RGB_REMOVE_LOW_BITS_MASK)) >> 1) +
			(C1 & C2 & RGB_LOW_BITS_MASK)) | ALPHA_BITS_MASK;
	}
};

struct COLOR_ADD_BRIGHTNESS
{
	static alwaysinline uint16 fn(uint16 C1, uint16 C2)
	{
		return ((brightness_cap[ (C1 >> RED_SHIFT_BITS)           +  (C2 >> RED_SHIFT_BITS)          ] << RED_SHIFT_BITS)   |
				(brightness_cap[((C1 >> GREEN_SHIFT_BITS) & 0x1f) + ((C2 >> GREEN_SHIFT_BITS) & 0x1f)] << GREEN_SHIFT_BITS) |
	// Proper 15->16bit color conversion moves the high bit of green into the low bit.
	#if GREEN_SHIFT_BITS == 6
			   ((brightness_cap[((C1 >> 6) & 0x1f) + ((C2 >> 6) & 0x1f)] & 0x10) << 1) |
	#endif
				(brightness_cap[ (C1                      & 0x1f) +  (C2                      & 0x1f)]      ));
	}

	static alwaysinline uint16 fn1_2(uint16 C1, uint16 C2)
	{
		return COLOR_ADD::fn1_2(C1, C2);
	}
};


struct COLOR_SUB
{
	static alwaysinline uint16 fn(uint16 C1, uint16 C2)
	{
		int rb1 = (C1 & (THIRD_COLOR_MASK | FIRST_COLOR_MASK)) | ((0x20 << 0) | (0x20 << RED_SHIFT_BITS));
		int rb2 = C2 & (THIRD_COLOR_MASK | FIRST_COLOR_MASK);
		int rb = rb1 - rb2;
		int rbcarry = rb & ((0x20 << RED_SHIFT_BITS) | (0x20 << 0));
		int g = ((C1 & (SECOND_COLOR_MASK)) | (0x20 << GREEN_SHIFT_BITS)) - (C2 & (SECOND_COLOR_MASK));
		int rgbsaturate = (((g & (0x20 << GREEN_SHIFT_BITS)) | rbcarry) >> 5) * 0x1f;
		uint16 retval = ((rb & (THIRD_COLOR_MASK | FIRST_COLOR_MASK)) | (g & SECOND_COLOR_MASK)) & rgbsaturate;
#if GREEN_SHIFT_BITS == 6
		retval |= (retval & 0x0400) >> 5;
#endif
		return retval;
	}

	static alwaysinline uint16 fn1_2(uint16 C1, uint16 C2)
	{
		return GFX.ZERO[((C1 | RGB_HI_BITS_MASKx2) -
			(C2 & RGB_REMOVE_LOW_BITS_MASK)) >> 1];
	}
};

#ifdef _XBOX
#include <ppcintrinsics.h>

// Rellenar un rango de pixels uint16 con un valor constante usando stores de 128 bits.
// dst debe estar alineado a 2 bytes (uint16*), count es numero de pixels.
static alwaysinline void S9xFillPixels(uint16 * __restrict dst, uint16 color, uint32 count)
{
	// Construir un vector de 8 pixels identicos (128 bits = 8 x uint16)
	uint32 color32 = ((uint32)color << 16) | color;
	__declspec(align(16)) uint32 fillbuf[4] = { color32, color32, color32, color32 };
	__vector4 vfill = *(__vector4*)fillbuf;

	// Escribir bloques de 8 pixels (16 bytes) alineados
	uint32 aligned_start = ((uintptr_t)dst + 15) & ~15;
	uint32 pre = ((uint16*)aligned_start - dst);
	if (pre > count) pre = count;

	// Pixels previos no alineados (escalar)
	for (uint32 i = 0; i < pre; i++)
		dst[i] = color;

	uint16 * __restrict adst = dst + pre;
	uint32 remaining = count - pre;
	uint32 vec_count = remaining >> 3; // bloques de 8 pixels
	uint32 tail = remaining & 7;

	// Bloques vectoriales de 128 bits
	__vector4 *vdst = (__vector4 *)adst;
	for (uint32 i = 0; i < vec_count; i++)
		__stvx(vfill, &vdst[i], 0);

	// Pixels restantes (escalar)
	uint16 *tdst = adst + (vec_count << 3);
	for (uint32 i = 0; i < tail; i++)
		tdst[i] = color;
}

// Rellenar un rango de bytes (Z-buffer) con un valor constante usando stores de 128 bits.
static alwaysinline void S9xFillZBuffer(uint8 * __restrict dst, uint8 depth, uint32 count)
{
	uint32 d32 = depth | (depth << 8) | (depth << 16) | (depth << 24);
	__declspec(align(16)) uint32 fillbuf[4] = { d32, d32, d32, d32 };
	__vector4 vfill = *(__vector4*)fillbuf;

	uint32 aligned_start = ((uintptr_t)dst + 15) & ~15;
	uint32 pre = ((uint8*)aligned_start - dst);
	if (pre > count) pre = count;

	for (uint32 i = 0; i < pre; i++)
		dst[i] = depth;

	uint8 * __restrict adst = dst + pre;
	uint32 remaining = count - pre;
	uint32 vec_count = remaining >> 4; // bloques de 16 bytes
	uint32 tail = remaining & 15;

	__vector4 *vdst = (__vector4 *)adst;
	for (uint32 i = 0; i < vec_count; i++)
		__stvx(vfill, &vdst[i], 0);

	uint8 *tdst = adst + (vec_count << 4);
	for (uint32 i = 0; i < tail; i++)
		tdst[i] = depth;
}

// COLOR_ADD::fn1_2 batch: promedia 8 pixels de golpe (Main + Sub) / 2
// Usado en el blend mode mas comun (Add Half)
static alwaysinline void S9xColorAddHalf_Batch8(uint16 * __restrict dst,
	const uint16 * __restrict c1, const uint16 * __restrict c2)
{
	// fn1_2: ((C1 & mask) + (C2 & mask)) >> 1) + (C1 & C2 & low_bits)
	// Con VMX procesamos 8 halfwords a la vez
	__vector4 v1 = *(__vector4*)c1;
	__vector4 v2 = *(__vector4*)c2;

	// Construir mascaras como vectores
	uint16 rmask = RGB_REMOVE_LOW_BITS_MASK;
	uint16 lmask = RGB_LOW_BITS_MASK;
	uint16 amask = ALPHA_BITS_MASK;
	uint32 rm32 = ((uint32)rmask << 16) | rmask;
	uint32 lm32 = ((uint32)lmask << 16) | lmask;
	uint32 am32 = ((uint32)amask << 16) | amask;
	__declspec(align(16)) uint32 rmbuf[4] = { rm32, rm32, rm32, rm32 };
	__declspec(align(16)) uint32 lmbuf[4] = { lm32, lm32, lm32, lm32 };
	__declspec(align(16)) uint32 ambuf[4] = { am32, am32, am32, am32 };
	__vector4 vrm = *(__vector4*)rmbuf;
	__vector4 vlm = *(__vector4*)lmbuf;
	__vector4 vam = *(__vector4*)ambuf;

	// (C1 & remove_low) + (C2 & remove_low)
	__vector4 masked1 = __vand(v1, vrm);
	__vector4 masked2 = __vand(v2, vrm);
	__vector4 sum = __vadduhm(masked1, masked2);
	// >> 1 via halfword shift right (vector de shifts = 1 por cada halfword)
	__declspec(align(16)) uint16 one16[8] = { 1,1,1,1,1,1,1,1 };
	__vector4 vone = *(__vector4*)one16;
	__vector4 half = __vsrh(sum, vone);
	// C1 & C2 & low_bits
	__vector4 low = __vand(__vand(v1, v2), vlm);
	// resultado = half + low | alpha
	__vector4 result = __vor(__vadduhm(half, low), vam);

	*(__vector4*)dst = result;
}
#endif

void S9xStartScreenRefresh (void);
void S9xEndScreenRefresh (void);
void S9xBuildDirectColourMaps (void);
void RenderLine (uint8);
void S9xComputeClipWindows (void);
void S9xDisplayChar (uint16 *, uint8);
void S9xGraphicsScreenResize (void);
// called automatically unless Settings.AutoDisplayMessages is false
void S9xDisplayMessages (uint16 *, int, int, int, int);

// external port interface which must be implemented or initialised for each port
bool8 S9xGraphicsInit (void);
void S9xGraphicsDeinit (void);
bool8 S9xInitUpdate (void);
bool8 S9xDeinitUpdate (int, int);
bool8 S9xContinueUpdate (int, int);
void S9xReRefresh (void);
void S9xSyncSpeed (void);

// called instead of S9xDisplayString if set to non-NULL
extern void (*S9xCustomDisplayString) (const char *, int, int, bool, int type);
void S9xVariableDisplayString(const char* string, int linesFromBottom, int pixelsFromLeft, bool allowWrap, int type);

#endif
