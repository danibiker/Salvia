#ifndef __MDFN_SURFACE_H
#define __MDFN_SURFACE_H

#include "mednafen-types.h"

/* 32bit color - XRGB8888 */
#define RED_SHIFT_32 16
#define GREEN_SHIFT_32 8
#define BLUE_SHIFT_32 0
#define ALPHA_SHIFT_32 24
#define MAKECOLOR_32(r, g, b, a) ((r << RED_SHIFT_32) | (g << GREEN_SHIFT_32) | (b << BLUE_SHIFT_32) | (a << ALPHA_SHIFT_32))

/* 16bit color - RGB565 */
#define RED_EXPAND_16 3
#define GREEN_EXPAND_16 2
#define BLUE_EXPAND_16 3
#define RED_SHIFT_16 11
#define GREEN_SHIFT_16 5
#define BLUE_SHIFT_16 0
#define MAKECOLOR_16(r, g, b, a) (((r >> RED_EXPAND_16) << RED_SHIFT_16) | ((g >> GREEN_EXPAND_16) << GREEN_SHIFT_16) | ((b >> BLUE_EXPAND_16) << BLUE_SHIFT_16))

/* 16bit color - RGB555 */
#define RED_EXPAND_15 3
#define GREEN_EXPAND_15 3
#define BLUE_EXPAND_15 3
#define RED_SHIFT_15 10
#define GREEN_SHIFT_15 5
#define BLUE_SHIFT_15 0
#define MAKECOLOR_15(r, g, b, a) (((r >> RED_EXPAND_15) << RED_SHIFT_15) | ((g >> GREEN_EXPAND_15) << GREEN_SHIFT_15) | ((b >> BLUE_EXPAND_15) << BLUE_SHIFT_15))

/* 16bit color - BGR555 */
#define BLUE_EXPAND_15_1 3
#define GREEN_EXPAND_15_1 3
#define RED_EXPAND_15_1 3
#define BLUE_SHIFT_15_1 10
#define GREEN_SHIFT_15_1 5
#define RED_SHIFT_15_1 0
#define MAKECOLOR_15_1(r, g, b, a) (((r >> RED_EXPAND_15_1) << RED_SHIFT_15_1) | ((g >> GREEN_EXPAND_15_1) << GREEN_SHIFT_15_1) | ((b >> BLUE_EXPAND_15_1) << BLUE_SHIFT_15_1))

typedef struct
{
 int32 x, y, w, h;
} MDFN_Rect;

typedef struct
{
   unsigned int colorspace;
   uint8 r_shift;
   uint8 g_shift;
   uint8 b_shift;
   uint8 a_shift;
} MDFN_PixelFormat;

typedef struct
{
   uint16 *pixels;
   int32 width;
   int32 height;
   int32 pitch;
   int32 bpp;
} MDFN_Surface;

#endif
