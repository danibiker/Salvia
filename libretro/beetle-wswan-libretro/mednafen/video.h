#ifndef __MDFN_VIDEO_H
#define __MDFN_VIDEO_H

#include <stdint.h>


#define RED_SHIFT_24 16
#define GREEN_SHIFT_24 8
#define BLUE_SHIFT_24 0
#define ALPHA_SHIFT_24 24
#define MAKECOLOR_24(r, g, b, a) ((r << RED_SHIFT_24) | (g << GREEN_SHIFT_24) | (b << BLUE_SHIFT_24) | (a << ALPHA_SHIFT_24))

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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
 int32_t w, h;
} MDFN_Rect;

typedef struct
{
   unsigned int colorspace;
   uint8_t r_shift;
   uint8_t g_shift;
   uint8_t b_shift;
   uint8_t a_shift;
} MDFN_PixelFormat;

typedef struct
{
   uint16_t *pixels;
   int32_t width;
   int32_t height;
   int32_t pitch;
   int32_t depth;
} MDFN_Surface;

#ifdef __cplusplus
}
#endif

#endif
