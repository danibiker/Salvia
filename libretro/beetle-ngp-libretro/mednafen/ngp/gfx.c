//---------------------------------------------------------------------------
// NEOPOP : Emulator as in Dreamland
//
// Copyright (c) 2001-2002 by neopop_uk
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version. See also the license.txt file for
//	additional informations.
//---------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "neopop.h"
#include "mem.h"
#include "gfx.h"
#include "interrupt.h"
#include "dma.h"
#include "TLCS-900h/TLCS900h_registers.h"
#include "../state.h"
#include "../video.h"
#ifdef MSB_FIRST
#include "../masmem.h"
#endif

static const unsigned char mirrored[] = {
    0x00, 0x40, 0x80, 0xc0, 0x10, 0x50, 0x90, 0xd0,
    0x20, 0x60, 0xa0, 0xe0, 0x30, 0x70, 0xb0, 0xf0,
    0x04, 0x44, 0x84, 0xc4, 0x14, 0x54, 0x94, 0xd4,
    0x24, 0x64, 0xa4, 0xe4, 0x34, 0x74, 0xb4, 0xf4,
    0x08, 0x48, 0x88, 0xc8, 0x18, 0x58, 0x98, 0xd8,
    0x28, 0x68, 0xa8, 0xe8, 0x38, 0x78, 0xb8, 0xf8,
    0x0c, 0x4c, 0x8c, 0xcc, 0x1c, 0x5c, 0x9c, 0xdc,
    0x2c, 0x6c, 0xac, 0xec, 0x3c, 0x7c, 0xbc, 0xfc,
    0x01, 0x41, 0x81, 0xc1, 0x11, 0x51, 0x91, 0xd1,
    0x21, 0x61, 0xa1, 0xe1, 0x31, 0x71, 0xb1, 0xf1,
    0x05, 0x45, 0x85, 0xc5, 0x15, 0x55, 0x95, 0xd5,
    0x25, 0x65, 0xa5, 0xe5, 0x35, 0x75, 0xb5, 0xf5,
    0x09, 0x49, 0x89, 0xc9, 0x19, 0x59, 0x99, 0xd9,
    0x29, 0x69, 0xa9, 0xe9, 0x39, 0x79, 0xb9, 0xf9,
    0x0d, 0x4d, 0x8d, 0xcd, 0x1d, 0x5d, 0x9d, 0xdd,
    0x2d, 0x6d, 0xad, 0xed, 0x3d, 0x7d, 0xbd, 0xfd,
    0x02, 0x42, 0x82, 0xc2, 0x12, 0x52, 0x92, 0xd2,
    0x22, 0x62, 0xa2, 0xe2, 0x32, 0x72, 0xb2, 0xf2,
    0x06, 0x46, 0x86, 0xc6, 0x16, 0x56, 0x96, 0xd6,
    0x26, 0x66, 0xa6, 0xe6, 0x36, 0x76, 0xb6, 0xf6,
    0x0a, 0x4a, 0x8a, 0xca, 0x1a, 0x5a, 0x9a, 0xda,
    0x2a, 0x6a, 0xaa, 0xea, 0x3a, 0x7a, 0xba, 0xfa,
    0x0e, 0x4e, 0x8e, 0xce, 0x1e, 0x5e, 0x9e, 0xde,
    0x2e, 0x6e, 0xae, 0xee, 0x3e, 0x7e, 0xbe, 0xfe,
    0x03, 0x43, 0x83, 0xc3, 0x13, 0x53, 0x93, 0xd3,
    0x23, 0x63, 0xa3, 0xe3, 0x33, 0x73, 0xb3, 0xf3,
    0x07, 0x47, 0x87, 0xc7, 0x17, 0x57, 0x97, 0xd7,
    0x27, 0x67, 0xa7, 0xe7, 0x37, 0x77, 0xb7, 0xf7,
    0x0b, 0x4b, 0x8b, 0xcb, 0x1b, 0x5b, 0x9b, 0xdb,
    0x2b, 0x6b, 0xab, 0xeb, 0x3b, 0x7b, 0xbb, 0xfb,
    0x0f, 0x4f, 0x8f, 0xcf, 0x1f, 0x5f, 0x9f, 0xdf,
    0x2f, 0x6f, 0xaf, 0xef, 0x3f, 0x7f, 0xbf, 0xff
};

static void drawColourPattern(ngpgfx_t *gfx, uint16_t *cfb_scanline, uint8_t *zbuffer,
      uint8 screenx, uint16 tile, uint8 tiley, uint16 mirror, 
      uint16* palette_ptr, uint8 pal, uint8 depth)
{
   uint16_t *ptr16;
	int index, left, right, highmark, xx;
	int x = screenx;
	if (x > 0xf8)
		x -= 256;
	if (x >= SCREEN_WIDTH)
		return;

	/* Get the data for the "tiley'th" line of "tile". */
   ptr16 = (uint16_t*)(gfx->CharacterRAM + (tile * 16) + (tiley * 2));
#ifdef MSB_FIRST
	index = LoadU16_RBO(ptr16);
#else
   index = *ptr16;
#endif

	/* Horizontal Flip */
	if (mirror)
		index = mirrored[(index & 0xff00)>>8] | (mirrored[(index & 0xff)] << 8);

	palette_ptr += pal << 2;
	left = MAX(MAX(x, gfx->winx), 0);
	right = x+7;

	highmark = MIN(gfx->winw + gfx->winx, SCREEN_WIDTH)-1;

	if (right > highmark)
   {
      index >>= (right - highmark)*2;
      right = highmark;
   }

	for (xx = right; xx>=left; --xx,index>>=2)
   {
      uint16_t data16;
      uint16_t *scan = &cfb_scanline[xx];
      uint8_t *zbuf = &zbuffer[xx];
		if (depth <= *zbuf || (index&3)==0) 
			continue;
		*zbuf = depth;

		/* Get the colour of the pixel */
      ptr16 = (uint16_t*)&palette_ptr[index&3];

#ifdef MSB_FIRST
		data16 = LoadU16_RBO(ptr16);
#else
		data16 = *ptr16;
#endif
		if (gfx->negative)
         data16 = ~data16;

      *scan = data16;
	}
}

static void draw_colour_scroll1(ngpgfx_t *gfx,
      uint16_t *cfb_scanline,
      uint8_t *zbuffer, uint8 depth, int ngpc_scanline)
{
	unsigned i;
	uint8_t line = ngpc_scanline + gfx->scroll1y;
	uint8_t row = line & 7;	/* Which row? */

	/* Draw Foreground scroll plane (Scroll 1) */
	for (i = 0; i < 32; i++)
	{
      uint16_t *ptr16 = (uint16_t*)(gfx->ScrollVRAM + ((i + ((line >> 3) << 5)) << 1));
#ifdef MSB_FIRST
		uint16_t data16 = LoadU16_RBO(ptr16);
#else
      uint16_t data16 = *ptr16;
#endif
		
		/* Draw the line of the tile */
		drawColourPattern(gfx, cfb_scanline, zbuffer, (i << 3) - gfx->scroll1x, data16 & 0x01FF, 
			(data16 & 0x4000) ? (7 - row) : row, data16 & 0x8000, (uint16_t*)(gfx->ColorPaletteRAM + 0x0080),
			(data16 & 0x1E00) >> 9, depth);
	}
}

static void draw_colour_scroll2(ngpgfx_t *gfx, uint16_t *cfb_scanline,
      uint8_t *zbuffer, uint8 depth, int ngpc_scanline)
{
   unsigned i;
	uint8_t line = ngpc_scanline + gfx->scroll2y;
	uint8_t row = line & 7;	/* Which row? */

	/* Draw Background scroll plane (Scroll 2) */
	for (i = 0; i < 32; i++)
	{
      uint16_t *ptr16 = (uint16_t*)(gfx->ScrollVRAM + 0x0800 + ((i + ((line >> 3) << 5)) << 1));
#ifdef MSB_FIRST
		uint16_t data16 = LoadU16_RBO(ptr16);
#else
		uint16_t data16 = *ptr16;
#endif
		
		/* Draw the line of the tile */
		drawColourPattern(gfx, cfb_scanline, zbuffer, (i << 3) - gfx->scroll2x, data16 & 0x01FF, 
			(data16 & 0x4000) ? (7 - row) : row, data16 & 0x8000, (uint16_t*)(gfx->ColorPaletteRAM + 0x0100),
			(data16 & 0x1E00) >> 9, depth);
	}
}

static void draw_scanline_colour(ngpgfx_t *gfx, uint16_t *cfb_scanline,
      int layer_enable, int ngpc_scanline)
{
	int16_t lastSpriteX;
	int16_t lastSpriteY;
	int spr;
   uint16_t *scan;
   int x = 0;
   uint8_t zbuffer[256] = {0};
	/* Window colour */
   uint16_t *ptr16 = (uint16_t*)(gfx->ColorPaletteRAM + 0x01F0 + (gfx->oowc << 1));
#ifdef MSB_FIRST
	uint16_t data16 = LoadU16_RBO(ptr16);
#else
	uint16_t data16 = *ptr16;
#endif
	if (gfx->negative)
      data16 = ~data16;

   scan = &cfb_scanline[0];

   /* Middle */
   if (!(ngpc_scanline < gfx->winy) && ngpc_scanline < gfx->winy + gfx->winh)
   {
      for (x = 0; x < MIN(gfx->winx, SCREEN_WIDTH); x++)
         *scan++ = data16;

      x = MIN(gfx->winx + gfx->winw, SCREEN_WIDTH);
      scan = &cfb_scanline[x];
   }

   /*Bottom and Top */
   for (; x < SCREEN_WIDTH; x++)
      *scan++ = data16;

	/* Ignore above and below the window's top and bottom */

	if (ngpc_scanline >= gfx->winy && ngpc_scanline < gfx->winy + gfx->winh)
	{
      int x;
      uint16_t *scan;
      uint16_t *ptr16 = (uint16_t*)(uint8*)(gfx->ColorPaletteRAM + 0x01E0 + ((gfx->bgc & 7) << 1));
#ifdef MSB_FIRST
      data16 = LoadU16_RBO(ptr16);
#else
      data16 = *ptr16;
#endif

		if (gfx->negative)
         data16 = ~data16;

      x    = gfx->winx;
      scan = &cfb_scanline[x];

		/*Draw background */
		for (; x < MIN(gfx->winx + gfx->winw, SCREEN_WIDTH); x++)	
			*scan++ = data16;

		/*Swap Front/Back scroll planes? */
		if (gfx->planeSwap)
		{
			if(layer_enable & 1)
			 draw_colour_scroll1(gfx, cfb_scanline, zbuffer, ZDEPTH_BACKGROUND_SCROLL, ngpc_scanline);		/* Swap */
			if(layer_enable & 2)
			 draw_colour_scroll2(gfx, cfb_scanline, zbuffer, ZDEPTH_FOREGROUND_SCROLL, ngpc_scanline);
		}
		else
		{
			if(layer_enable & 1)
			 draw_colour_scroll2(gfx, cfb_scanline, zbuffer, ZDEPTH_BACKGROUND_SCROLL, ngpc_scanline);		/* Normal */
			if(layer_enable & 2)
			 draw_colour_scroll1(gfx, cfb_scanline, zbuffer, ZDEPTH_FOREGROUND_SCROLL, ngpc_scanline);
		}

		/* Draw Sprites */
		/*Last sprite position, (defaults to top-left, sure?) */
		lastSpriteX = 0;
		lastSpriteY = 0;

		if(layer_enable & 4)
		for (spr = 0; spr < 64; spr++)
		{
			uint8_t sx       = gfx->SpriteVRAM[(spr * 4) + 2];	/* X position */
			uint8_t sy       = gfx->SpriteVRAM[(spr * 4) + 3];	/* Y position */
			int16_t x        = sx;
			int16_t y        = sy;
         uint16_t *ptr16  = (uint16_t*)(gfx->SpriteVRAM + (spr * 4));
#ifdef MSB_FIRST
			uint16_t data16  = LoadU16_RBO(ptr16);
#else
         uint16_t data16  = *ptr16;
#endif
			uint8_t priority = (data16 & 0x1800) >> 11;

			if (data16 & 0x0400)
            x = lastSpriteX + sx;	/* Horizontal chain? */
			if (data16 & 0x0200)
            y = lastSpriteY + sy;	/* Vertical chain? */

			/* Store the position for chaining */
			lastSpriteX = x;
			lastSpriteY = y;
			
			/* Visible? */
			if (priority == 0)
            continue;

			/* Scroll the sprite */
			x += gfx->scrollsprx; 
			y += gfx->scrollspry;

			/* Off-screen? */
			if (x > 248 && x < 256)
            x = x - 256;
         else
            x &= 0xFF;
			if (y > 248 && y < 256)
            y = y - 256;
         else
            y &= 0xFF;

			/* In range? */
			if (ngpc_scanline >= y && ngpc_scanline <= y + 7)
			{
				uint8_t row = (ngpc_scanline - y) & 7;	/* Which row? */
				drawColourPattern(gfx, cfb_scanline, zbuffer, (uint8)x, data16 & 0x01FF, 
					(data16 & 0x4000) ? 7 - row : row, data16 & 0x8000,
					(uint16_t*)gfx->ColorPaletteRAM, gfx->SpriteVRAMColor[spr] & 0xF, priority << 1); 
			}
		}
	}
}

static void MonoPlot(ngpgfx_t *gfx, uint16_t *cfb_scanline, uint8_t *zbuffer,
      uint8 x, uint8* palette_ptr, uint16 pal_hi, uint8 index, uint8 depth)
{
   uint16 r, g, b;
	uint8_t data8, *zbuf;
   uint16_t *scan, data16;

	/* Clip */
	if (index == 0 || x < gfx->winx || x >= (gfx->winw + gfx->winx) || x >= SCREEN_WIDTH)
		return;

   zbuf = &zbuffer[x];

	/*Depth check, <= to stop later sprites overwriting pixels! */
	if (depth <= *zbuf)
      return;
	*zbuf = depth;

	/*Get the colour of the pixel */
   data8 = palette_ptr[(pal_hi ? 3 : 0) + index - 1];

	r     = (data8 & 7) << 1;
	g     = (data8 & 7) << 5;
	b     = (data8 & 7) << 9;

   scan = &cfb_scanline[x];
   data16 = ~(r | g | b);
	if (gfx->negative)
      data16 = ~(data16);

   *scan++ = data16;
}

static void drawMonoPattern(ngpgfx_t *gfx, 
      uint16_t *cfb_scanline, uint8_t *zbuffer, uint8 screenx, uint16 tile, uint8 tiley, uint16 mirror, 
      uint8* palette_ptr, uint16 pal, uint8 depth)
{
	/* Get the data for th e "tiley'th" line of "tile". */
   uint16_t *ptr16 = (uint16_t*)(gfx->CharacterRAM + (tile * 16) + (tiley * 2));
#ifdef MSB_FIRST
	uint16_t   data = LoadU16_RBO(ptr16);
#else
   uint16_t   data = *ptr16;
#endif

	if (mirror)
	{
      /* Horizontal Flip */
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 7, palette_ptr, pal, (data & 0xC000) >> 0xE, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 6, palette_ptr, pal, (data & 0x3000) >> 0xC, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 5, palette_ptr, pal, (data & 0x0C00) >> 0xA, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 4, palette_ptr, pal, (data & 0x0300) >> 0x8, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 3, palette_ptr, pal, (data & 0x00C0) >> 0x6, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 2, palette_ptr, pal, (data & 0x0030) >> 0x4, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 1, palette_ptr, pal, (data & 0x000C) >> 0x2, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 0, palette_ptr, pal, (data & 0x0003) >> 0x0, depth);
	}
	else
	{
      /*Normal */
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 0, palette_ptr, pal, (data & 0xC000) >> 0xE, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 1, palette_ptr, pal, (data & 0x3000) >> 0xC, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 2, palette_ptr, pal, (data & 0x0C00) >> 0xA, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 3, palette_ptr, pal, (data & 0x0300) >> 0x8, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 4, palette_ptr, pal, (data & 0x00C0) >> 0x6, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 5, palette_ptr, pal, (data & 0x0030) >> 0x4, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 6, palette_ptr, pal, (data & 0x000C) >> 0x2, depth);
		MonoPlot(gfx, cfb_scanline, zbuffer, screenx + 7, palette_ptr, pal, (data & 0x0003) >> 0x0, depth);
	}
}

static void draw_mono_scroll1(ngpgfx_t *gfx, uint16_t *cfb_scanline,
      uint8_t *zbuffer, uint8 depth, int ngpc_scanline)
{
	unsigned i;
	uint8_t line = ngpc_scanline + gfx->scroll1y;
	uint8_t row = line & 7;	/* Which row? */

	/* Draw Foreground scroll plane (Scroll 1) */
	for (i = 0; i < 32; i++)
	{
      uint16_t *ptr16 = (uint16_t*)(gfx->ScrollVRAM + ((i + ((line >> 3) << 5)) << 1));
#ifdef MSB_FIRST
		uint16_t data16 = LoadU16_RBO(ptr16);
#else
      uint16_t data16 = *ptr16;
#endif
		
		//Draw the line of the tile
		drawMonoPattern(gfx, cfb_scanline, zbuffer, (i << 3) - gfx->scroll1x, data16 & 0x01FF, 
			(data16 & 0x4000) ? 7 - row : row, data16 & 0x8000, gfx->SCRP1PLT,
			data16 & 0x2000, depth);
	}
}

static void draw_mono_scroll2(ngpgfx_t *gfx, uint16_t *cfb_scanline, uint8_t *zbuffer, uint8 depth, int ngpc_scanline)
{
   unsigned i;
	uint8_t line = ngpc_scanline + gfx->scroll2y;
	uint8_t row = line & 7;	//Which row?

	/* Draw Background scroll plane (Scroll 2) */
	for (i = 0; i < 32; i++)
	{
      uint16_t *ptr16 = (uint16_t*)(gfx->ScrollVRAM + 0x0800 + ((i + ((line >> 3) << 5)) << 1));
#ifdef MSB_FIRST
		uint16_t data16 = LoadU16_RBO(ptr16);
#else
      uint16_t data16 = *ptr16;
#endif
		
		/* Draw the line of the tile */
		drawMonoPattern(gfx, cfb_scanline, zbuffer, (i << 3) - gfx->scroll2x, data16 & 0x01FF, 
			(data16 & 0x4000) ? 7 - row : row, data16 & 0x8000, gfx->SCRP2PLT,
			data16 & 0x2000, depth);
	}
}

static void draw_scanline_mono(ngpgfx_t *gfx,
      uint16_t *cfb_scanline, int layer_enable, int ngpc_scanline)
{
   int x;
   uint16_t *scan;
	int16_t lastSpriteX;
	int16_t lastSpriteY;
	int spr;
   uint8_t zbuffer[256] = {0};

	/* Window colour */
	uint16_t r = (uint16)gfx->oowc << 1;
	uint16_t g = (uint16)gfx->oowc << 5;
	uint16_t b = (uint16)gfx->oowc << 9;
   uint16_t data16 = ~(r | g | b);

	if (gfx->negative)
		data16 = ~data16;

   x = 0;
   scan = &cfb_scanline[x];

   /* Middle */
   if (!(ngpc_scanline < gfx->winy) && ngpc_scanline < gfx->winy + gfx->winh)
   {
      for (; x < MIN(gfx->winx, SCREEN_WIDTH); x++)
         *scan++ = data16;

      x = MIN(gfx->winx + gfx->winw, SCREEN_WIDTH);
   }

   /* Bottom and Top */
   for (; x < SCREEN_WIDTH; x++)
      *scan++ = data16;

	/* Ignore above and below the window's top and bottom */
	if (ngpc_scanline >= gfx->winy && ngpc_scanline < gfx->winy + gfx->winh)
	{
      int x;
      uint16_t *scan;

      data16 = 0x0FFF;

		/* Background colour Enabled? */
		if ((gfx->bgc & 0xC0) == 0x80)
		{
			r = (uint16)(gfx->bgc & 7) << 1;
			g = (uint16)(gfx->bgc & 7) << 5;
			b = (uint16)(gfx->bgc & 7) << 9;
			data16 = ~(r | g | b);
		}

		if (gfx->negative)
         data16 = ~data16;

      x    = gfx->winx;
      scan = &cfb_scanline[x];

		/* Draw background */
		for (; x < MIN(gfx->winx + gfx->winw, SCREEN_WIDTH); x++)	
			*scan++ = data16;

		/* Swap Front/Back scroll planes? */
		if (gfx->planeSwap)
		{
			if(layer_enable & 1)
			 draw_mono_scroll1(gfx, cfb_scanline, zbuffer, ZDEPTH_BACKGROUND_SCROLL, ngpc_scanline);		//Swap
			if(layer_enable & 2)
			 draw_mono_scroll2(gfx, cfb_scanline, zbuffer, ZDEPTH_FOREGROUND_SCROLL, ngpc_scanline);
		}
		else
		{
			if(layer_enable & 1)
			 draw_mono_scroll2(gfx, cfb_scanline, zbuffer, ZDEPTH_BACKGROUND_SCROLL, ngpc_scanline);		//Normal
			if(layer_enable & 2)
			 draw_mono_scroll1(gfx, cfb_scanline, zbuffer, ZDEPTH_FOREGROUND_SCROLL, ngpc_scanline);
		}

		/* Draw Sprites */
		/* Last sprite position, (defaults to top-left, sure?) */
		lastSpriteX = 0;
		lastSpriteY = 0;
		if(layer_enable & 4)
		for (spr = 0; spr < 64; spr++)
		{
			uint8_t priority, row;
			uint8_t sx = gfx->SpriteVRAM[(spr * 4) + 2];	//X position
			uint8_t sy = gfx->SpriteVRAM[(spr * 4) + 3];	//Y position
			int16_t x = sx;
			int16_t y = sy;
         uint16_t *ptr16 = (uint16_t*)(gfx->SpriteVRAM + (spr * 4));

#ifdef MSB_FIRST
			data16 = LoadU16_RBO(ptr16);
#else
         data16 = *ptr16;
#endif
			priority = (data16 & 0x1800) >> 11;

			if (data16 & 0x0400)
            x = lastSpriteX + sx;	//Horizontal chain?
			if (data16 & 0x0200)
            y = lastSpriteY + sy;	//Vertical chain?

			//Store the position for chaining
			lastSpriteX = x;
			lastSpriteY = y;
			
			//Visible?
			if (priority == 0)
            continue;

			//Scroll the sprite
			x += gfx->scrollsprx;
			y += gfx->scrollspry;

			//Off-screen?
			if (x > 248 && x < 256)	x = x - 256; else x &= 0xFF;
			if (y > 248 && y < 256)	y = y - 256; else y &= 0xFF;

			//In range?
			if (ngpc_scanline >= y && ngpc_scanline <= y + 7)
			{
				row = (ngpc_scanline - y) & 7;	//Which row?
				drawMonoPattern(gfx, cfb_scanline, zbuffer, (uint8)x, data16 & 0x01FF, 
					(data16 & 0x4000) ? 7 - row : row, data16 & 0x8000,
					gfx->SPPLT, data16 & 0x2000, priority << 1); 
			}
		}

	}
}

static void ngpgfx_delayed_settings(ngpgfx_t *gfx)
{
	//Window dimensions
	gfx->winx = gfx->WBA_H;
	gfx->winy = gfx->WBA_V;
	gfx->winw = gfx->WSI_H;
	gfx->winh = gfx->WSI_V;

	/* Scroll Planes (Confirmed delayed) */
	gfx->scroll1x = gfx->S1SO_H;
	gfx->scroll1y = gfx->S1SO_V;

	gfx->scroll2x = gfx->S2SO_H;
	gfx->scroll2y = gfx->S2SO_V;

	/* Sprite offset (Confirmed delayed) */
	gfx->scrollsprx = gfx->PO_H;
	gfx->scrollspry = gfx->PO_V;

	/* Plane Priority (Confirmed delayed) */
	gfx->planeSwap = gfx->P_F & 0x80;

	/* Background colour register (Confirmed delayed) */
	gfx->bgc = gfx->BG_COL;

	/* 2D Control register (Confirmed delayed) */
	gfx->oowc     = gfx->CONTROL_2D & 7;
	gfx->negative = gfx->CONTROL_2D & 0x80;
}

void ngpgfx_reset(ngpgfx_t *gfx)
{
   memset(gfx->SPPLT,    0x7, sizeof(gfx->SPPLT));
   memset(gfx->SCRP1PLT, 0x7, sizeof(gfx->SCRP1PLT));
   memset(gfx->SCRP2PLT, 0x7, sizeof(gfx->SCRP2PLT));

   gfx->raster_line    = 0;
   gfx->S1SO_H         = 0;
   gfx->S1SO_V         = 0;
   gfx->S2SO_H         = 0;
   gfx->S2SO_V         = 0;
   gfx->WBA_H          = 0;
   gfx->WBA_V          = 0;

   gfx->WSI_H          = 0xFF;
   gfx->WSI_V          = 0xFF;

   gfx->C_OVR          = 0;
   gfx->BLNK           = 0;

   gfx->PO_H           = 0;
   gfx->PO_V           = 0;
   gfx->P_F            = 0;

   gfx->BG_COL         = 0x7;
   gfx->CONTROL_2D     = 0;
   gfx->CONTROL_INT    = 0;
   gfx->SCREEN_PERIOD  = 0xC6;
   gfx->K2GE_MODE      = 0;

   ngpgfx_delayed_settings(gfx);
}

void ngpgfx_power(ngpgfx_t *gfx)
{
   ngpgfx_reset(gfx);

   memset(gfx->ScrollVRAM, 0, sizeof(gfx->ScrollVRAM));
   memset(gfx->CharacterRAM, 0, sizeof(gfx->CharacterRAM));
   memset(gfx->SpriteVRAM, 0, sizeof(gfx->SpriteVRAM));
   memset(gfx->SpriteVRAMColor, 0, sizeof(gfx->SpriteVRAMColor));
   memset(gfx->ColorPaletteRAM, 0, sizeof(gfx->ColorPaletteRAM));
}


bool ngpgfx_hint(ngpgfx_t *gfx)
{
   /* H_Int / Delayed settings */
   if ((gfx->raster_line < SCREEN_HEIGHT-1 || gfx->raster_line == gfx->SCREEN_PERIOD))
   {
      /* Get delayed settings */
      ngpgfx_delayed_settings(gfx);

      /* Allowed? */
      if (gfx->CONTROL_INT & 0x40)
         return 1;
   }

   return 0;
}

void ngpgfx_set_pixel_format(ngpgfx_t *gfx, int depth)
{
   unsigned i;

   for(i = 0; i < 4096; i++)
   {
      int r = (i & 0xF) * 17;
      int g = ((i >> 4) & 0xF) * 17;
      int b = ((i >> 8) & 0xF) * 17;

      switch(depth)
      {
         case 15: gfx->ColorMap[i] = MAKECOLOR_15(r, g, b, 0); break;
         case 16:
	 default: gfx->ColorMap[i] = MAKECOLOR_16(r, g, b, 0); break;
      }
   }
}

bool ngpgfx_draw(ngpgfx_t *gfx, void *data, bool skip)
{
   unsigned x;
   bool ret = 0;
   MDFN_Surface *surface = (MDFN_Surface*)data;

   /* Draw the scanline */
   if (gfx->raster_line < SCREEN_HEIGHT && !skip)
   {
      uint16_t cfb_scanline[256];

      if (!gfx->K2GE_MODE)
         draw_scanline_colour(gfx, cfb_scanline, gfx->layer_enable, gfx->raster_line);
      else
         draw_scanline_mono(gfx, cfb_scanline, gfx->layer_enable, gfx->raster_line);

      switch (surface->depth)
      {
         case 15:
         case 16: {
            uint16_t *dest = surface->pixels + surface->pitch * gfx->raster_line;

            for (x = 0; x < SCREEN_WIDTH; x++)
               dest[x] = gfx->ColorMap[cfb_scanline[x] & 4095];
         } break;

         case 24: {
            uint32_t *dest = ((uint32_t *) surface->pixels) + surface->pitch * gfx->raster_line;

            for (x = 0; x < SCREEN_WIDTH; x++)
               dest[x] = gfx->ColorMap[cfb_scanline[x] & 4095];
         } break;
      }
   }
   gfx->raster_line++;

   /* V_Int? */
   if (gfx->raster_line == SCREEN_HEIGHT)
   {
      gfx->BLNK = 1;
      ret = 1;

      if(gfx->CONTROL_INT & 0x80) /* (statusIFF() <= 4 */
         TestIntHDMA(5, 0x0B);
   }

   /* End of V_Int */
   if(gfx->raster_line == gfx->SCREEN_PERIOD + 1)
   {
      /* Last scanline + 1 */
      gfx->raster_line = 0;
      gfx->C_OVR = 0;
      gfx->BLNK = 0;
   }

   return ret;
}

int ngpgfx_StateAction(ngpgfx_t *gfx, void *data, int load, int data_only)
{
   StateMem *sm = (StateMem*)data;
   SFORMAT StateRegs[] =
   {
      { &(gfx->raster_line), sizeof(gfx->raster_line), 0x80000000, "raster_line" },
      { &(gfx->S1SO_H), (uint32_t)(sizeof(gfx->S1SO_H)), 0x80000000, "S1SO_H" },
      { &(gfx->S1SO_V), (uint32_t)(sizeof(gfx->S1SO_V)), 0x80000000, "S1SO_V" },
      { &(gfx->S2SO_H), (uint32_t)(sizeof(gfx->S2SO_H)), 0x80000000, "S2SO_H" },
      { &(gfx->S2SO_V), (uint32_t)(sizeof(gfx->S2SO_V)), 0x80000000, "S2SO_V" },
      { &(gfx->WBA_H), (uint32_t)(sizeof(gfx->WBA_H)),   0x80000000, "WBA_H" },
      { &(gfx->WBA_V), (uint32_t)(sizeof(gfx->WBA_V)),   0x80000000, "WBA_V" },
      { &(gfx->WSI_H), (uint32_t)(sizeof(gfx->WSI_H)),   0x80000000, "WSI_H" },
      { &(gfx->WSI_V), (uint32_t)(sizeof(gfx->WSI_V)),   0x80000000, "WSI_V" },
      { &(gfx->C_OVR), (uint32_t)1, 0x80000000 | 0x08000000, "C_OVR" },
      { &(gfx->BLNK), (uint32_t)1,  0x80000000 | 0x08000000, "BLNK" },
      { &(gfx->PO_H), (uint32_t)(sizeof(gfx->PO_H)), 0x80000000, "PO_H" },
      { &(gfx->PO_V), (uint32_t)(sizeof(gfx->PO_V)), 0x80000000, "PO_V" },
      { &(gfx->P_F), (uint32_t)(sizeof(gfx->P_F)),   0x80000000, "P_F" },
      { &(gfx->BG_COL), (uint32_t)(sizeof(gfx->BG_COL)), 0x80000000, "BG_COL" },
      { &(gfx->CONTROL_2D), (uint32_t)(sizeof(gfx->CONTROL_2D)), 0x80000000, "CONTROL_2D" },
      { &(gfx->CONTROL_INT), (uint32_t)(sizeof(gfx->CONTROL_INT)), 0x80000000, "CONTROL_INT" },
      { &(gfx->SCREEN_PERIOD), (uint32_t)(sizeof(gfx->SCREEN_PERIOD)), 0x80000000, "SCREEN_PERIOD" },
      { &(gfx->K2GE_MODE), (uint32_t)(sizeof(gfx->K2GE_MODE)), 0x80000000, "K2GE_MODE" },

      { (gfx->SPPLT), (uint32_t)(6), 0, "SPPLT" },
      { (gfx->SCRP1PLT), (uint32_t)(6), 0, "SCRP1PLT" },
      { (gfx->SCRP2PLT), (uint32_t)(6), 0, "SCRP2PLT" },

      { &(gfx->winx), (uint32_t)(sizeof(gfx->winx)), 0x80000000, "winx" },
      { &(gfx->winw), (uint32_t)(sizeof(gfx->winw)), 0x80000000, "winw" },
      { &(gfx->winy), (uint32_t)(sizeof(gfx->winy)), 0x80000000, "winy" },
      { &(gfx->winh), (uint32_t)(sizeof(gfx->winh)), 0x80000000, "winh" },
      { &(gfx->scroll1x), (uint32_t)(sizeof(gfx->scroll1x)), 0x80000000, "scroll1x" },
      { &(gfx->scroll1y), (uint32_t)(sizeof(gfx->scroll1y)), 0x80000000, "scroll1y" },
      { &(gfx->scroll2x), (uint32_t)(sizeof(gfx->scroll2x)), 0x80000000, "scroll2x" },
      { &(gfx->scroll2y), (uint32_t)(sizeof(gfx->scroll2y)), 0x80000000, "scroll2y" },
      { &(gfx->scrollsprx), (uint32_t)(sizeof(gfx->scrollsprx)), 0x80000000, "scrollsprx" },
      { &(gfx->scrollspry), (uint32_t)(sizeof(gfx->scrollspry)), 0x80000000, "scrollspry" },
      { &(gfx->planeSwap), (uint32_t)(sizeof(gfx->planeSwap)), 0x80000000, "planeSwap" },
      { &(gfx->bgc), (uint32_t)(sizeof(gfx->bgc)), 0x80000000, "bgc" },
      { &(gfx->oowc), (uint32_t)(sizeof(gfx->oowc)), 0x80000000, "oowc" },

      { &(gfx->negative), (uint32_t)(sizeof(gfx->negative)), 0x80000000, "negative" },

      { (gfx->ScrollVRAM), (uint32_t)(4096), 0, "ScrollVRAM" },
      { (gfx->CharacterRAM), (uint32_t)(8192), 0, "CharacterRAM" },
      { (gfx->SpriteVRAM), (uint32_t)(256), 0, "SpriteVRAM" },
      { (gfx->SpriteVRAMColor), (uint32_t)(0x40), 0, "SpriteVRAMColor" },
      { (gfx->ColorPaletteRAM), (uint32_t)(0x200), 0, "ColorPaletteRAM" },

      { 0, 0, 0, 0 }
   };

   if(!MDFNSS_StateAction(sm, load, data_only, StateRegs, "GFX", false))
      return(0);

   return(1);
}

void ngpgfx_SetLayerEnableMask(ngpgfx_t *gfx, uint64_t mask)
{
   gfx->layer_enable = mask;
}

void ngpgfx_write8(ngpgfx_t *gfx, uint32 address, uint8 data)
{
   if(address >= 0x9000 && address <= 0x9fff)
      gfx->ScrollVRAM[address - 0x9000] = data;
   else if(address >= 0xa000 && address <= 0xbfff)
      gfx->CharacterRAM[address - 0xa000] = data;
   else if(address >= 0x8800 && address <= 0x88ff)
      gfx->SpriteVRAM[address - 0x8800] = data;
   else if(address >= 0x8c00 && address <= 0x8c3f)
      gfx->SpriteVRAMColor[address - 0x8c00] = data & 0x0f;
   else if(address >= 0x8200 && address <= 0x83ff)
      gfx->ColorPaletteRAM[address - 0x8200] = data;
   else switch(address)
   {
      case 0x8000:
         gfx->CONTROL_INT = data & 0xC0;
         break;
      case 0x8002:
         gfx->WBA_H = data;
         break;
      case 0x8003:
         gfx->WBA_V = data;
         break;
      case 0x8004:
         gfx->WSI_H = data;
         break;
      case 0x8005:
         gfx->WSI_V = data;
         break;
      case 0x8006:
         gfx->SCREEN_PERIOD = data;
         break;
      case 0x8012:
         gfx->CONTROL_2D = data & 0x87;
         break;
      case 0x8020:
         gfx->PO_H = data;
         break;
      case 0x8021:
         gfx->PO_V = data;
         break;
      case 0x8030:
         gfx->P_F = data & 0x80;
         break;
      case 0x8032:
         gfx->S1SO_H = data;
         break;
      case 0x8033:
         gfx->S1SO_V = data;
         break;
      case 0x8034:
         gfx->S2SO_H = data;
         break;
      case 0x8035:
         gfx->S2SO_V = data;
         break;
      case 0x8101:
         gfx->SPPLT[0] = data & 0x7;
         break;
      case 0x8102:
         gfx->SPPLT[1] = data & 0x7;
         break;
      case 0x8103:
         gfx->SPPLT[2] = data & 0x7;
         break;
      case 0x8105:
         gfx->SPPLT[3] = data & 0x7;
         break;
      case 0x8106:
         gfx->SPPLT[4] = data & 0x7;
         break;
      case 0x8107:
         gfx->SPPLT[5] = data & 0x7;
         break;
      case 0x8109:
         gfx->SCRP1PLT[0] = data & 0x7;
         break;
      case 0x810a:
         gfx->SCRP1PLT[1] = data & 0x7;
         break;
      case 0x810b:
         gfx->SCRP1PLT[2] = data & 0x7;
         break;
      case 0x810d:
         gfx->SCRP1PLT[3] = data & 0x7;
         break;
      case 0x810e:
         gfx->SCRP1PLT[4] = data & 0x7;
         break;
      case 0x810f:
         gfx->SCRP1PLT[5] = data & 0x7;
         break;
      case 0x8111:
         gfx->SCRP2PLT[0] = data & 0x7;
         break;
      case 0x8112:
         gfx->SCRP2PLT[1] = data & 0x7;
         break;
      case 0x8113:
         gfx->SCRP2PLT[2] = data & 0x7;
         break;

      case 0x8115:
         gfx->SCRP2PLT[3] = data & 0x7;
         break;
      case 0x8116:
         gfx->SCRP2PLT[4] = data & 0x7;
         break;
      case 0x8117:
         gfx->SCRP2PLT[5] = data & 0x7;
         break;

      case 0x8118:
         gfx->BG_COL = data & 0xC7;
         break;

      case 0x87e0:
         if(data == 0x52) 
         {
            /* GEreset */
            neopop_reset(); 
         }
         break;
      case 0x87e2:
         gfx->K2GE_MODE = data & 0x80;
         break;
   }
}

void ngpgfx_write16(ngpgfx_t *gfx, uint32 address, uint16 data)
{
   ngpgfx_write8(gfx, address, data & 0xFF);
   ngpgfx_write8(gfx, address + 1, data >> 8);
}

uint8_t ngpgfx_read8(ngpgfx_t *gfx, uint32_t address)
{
   if(address >= 0x9000 && address <= 0x9fff)
      return(gfx->ScrollVRAM[address - 0x9000]);
   else if(address >= 0xa000 && address <= 0xbfff)
      return(gfx->CharacterRAM[address - 0xa000]);
   else if(address >= 0x8800 && address <= 0x88ff)
      return(gfx->SpriteVRAM[address - 0x8800]);
   else if(address >= 0x8c00 && address <= 0x8c3f)
      return(gfx->SpriteVRAMColor[address - 0x8c00]);
   else if(address >= 0x8200 && address <= 0x83ff)
      return(gfx->ColorPaletteRAM[address - 0x8200]);
   else switch(address)
   {
      case 0x8000:
         return gfx->CONTROL_INT;
      case 0x8002:
         return gfx->WBA_H;
      case 0x8003:
         return gfx->WBA_V;
      case 0x8004:
         return gfx->WSI_H;
      case 0x8005:
         return gfx->WSI_V;
      case 0x8006:
         return gfx->SCREEN_PERIOD;
      case 0x8008:
         return( (uint8)((abs(TIMER_HINT_RATE - (int)timer_hint)) >> 2) ); //RAS.H read (Simulated horizontal raster position)
      case 0x8009:
         return gfx->raster_line;
      case 0x8010:
         return((gfx->C_OVR ? 0x80 : 0x00) | (gfx->BLNK ? 0x40 : 0x00));
      case 0x8012:
         return gfx->CONTROL_2D;
      case 0x8020:
         return gfx->PO_H;
      case 0x8021:
         return gfx->PO_V;
      case 0x8030:
         return gfx->P_F;
      case 0x8032:
         return gfx->S1SO_H;
      case 0x8033:
         return gfx->S1SO_V;
      case 0x8034:
         return gfx->S2SO_H;
      case 0x8035:
         return gfx->S2SO_V;
      case 0x8101:
         return gfx->SPPLT[0];
      case 0x8102:
         return gfx->SPPLT[1];
      case 0x8103:
         return gfx->SPPLT[2];
      case 0x8105:
         return gfx->SPPLT[3];
      case 0x8106:
         return gfx->SPPLT[4];
      case 0x8107:
         return gfx->SPPLT[5];
      case 0x8108:
         return gfx->SCRP1PLT[0];
      case 0x8109:
         return gfx->SCRP1PLT[1];
      case 0x810a:
         return gfx->SCRP1PLT[2];
      case 0x810d:
         return gfx->SCRP1PLT[3];
      case 0x810e:
         return gfx->SCRP1PLT[4];
      case 0x810f:
         return gfx->SCRP1PLT[5];
      case 0x8111:
         return gfx->SCRP2PLT[0];
      case 0x8112:
         return gfx->SCRP2PLT[1];
      case 0x8113:
         return gfx->SCRP2PLT[2];
      case 0x8115:
         return gfx->SCRP2PLT[3];
      case 0x8116:
         return gfx->SCRP2PLT[4];
      case 0x8117:
         return gfx->SCRP2PLT[5];
      case 0x8118:
         return(gfx->BG_COL);
      case 0x87e2:
         return(gfx->K2GE_MODE);
   }

   return 0;
}

uint16_t ngpgfx_read16(ngpgfx_t *gfx, uint32_t address)
{
   uint16_t ret;

   ret = ngpgfx_read8(gfx, address);
   ret |= ngpgfx_read8(gfx, address + 1) << 8;

   return(ret);
}

