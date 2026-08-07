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

#ifndef __GFX__
#define __GFX__

#include <stdint.h>

#define ZDEPTH_BACK_SPRITE          2
#define ZDEPTH_BACKGROUND_SCROLL    3
#define ZDEPTH_MIDDLE_SPRITE        4
#define ZDEPTH_FOREGROUND_SCROLL    5
#define ZDEPTH_FRONT_SPRITE			6

#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t cfb_scanline[256];	// __attribute__ ((aligned (8)));

typedef struct ngpgfx
{
   uint8_t winx, winw;
   uint8_t winy, winh;
   uint8_t scroll1x, scroll1y;
   uint8_t scroll2x, scroll2y;
   uint8_t scrollsprx, scrollspry;
   uint8_t planeSwap;
   uint8_t bgc, oowc, negative;

   uint8_t ScrollVRAM[4096];       /* 9000-9fff */
   uint8_t CharacterRAM[8192];     /* a000-bfff */
   uint8_t SpriteVRAM[256];        /* 8800-88ff */
   uint8_t SpriteVRAMColor[0x40];  /* 8C00-8C3F */
   uint8_t ColorPaletteRAM[0x200]; /* 8200-83ff */

   uint8_t SPPLT[6];
   uint8_t SCRP1PLT[6];
   uint8_t SCRP2PLT[6];

   uint8_t raster_line;
   uint8_t S1SO_H, S1SO_V, S2SO_H, S2SO_V;
   uint8_t WBA_H, WBA_V, WSI_H, WSI_V;
   bool C_OVR, BLNK;
   uint8_t PO_H, PO_V;
   uint8_t P_F;
   uint8_t BG_COL;
   uint8_t CONTROL_2D;
   uint8_t CONTROL_INT;
   uint8_t SCREEN_PERIOD;
   uint8_t K2GE_MODE;

   uint32_t ColorMap[4096];

   int layer_enable;
} ngpgfx_t;

void ngpgfx_set_pixel_format(ngpgfx_t *fx, int depth);

void ngpgfx_SetLayerEnableMask(ngpgfx_t *gfx, uint64_t mask);

int ngpgfx_StateAction(ngpgfx_t *gfx, void *data, int load, int data_only);

void ngpgfx_power(ngpgfx_t *gfx);

bool ngpgfx_hint(ngpgfx_t *gfx);

bool ngpgfx_draw(ngpgfx_t *gfx, void *data, bool skip);

uint8_t ngpgfx_read8(ngpgfx_t *gfx, uint32_t address);

uint16_t ngpgfx_read16(ngpgfx_t *gfx, uint32_t address);

void ngpgfx_write16(ngpgfx_t *gfx, uint32_t address, uint16_t data);

void ngpgfx_write8(ngpgfx_t *gfx, uint32_t address, uint8_t data);

extern ngpgfx_t *NGPGfx;

#ifdef __cplusplus
}
#endif

#endif

