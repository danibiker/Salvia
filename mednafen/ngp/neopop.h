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

#ifndef __NEOPOP__
#define __NEOPOP__

#include <stdint.h>

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

typedef enum
{
	COLOURMODE_GREYSCALE,
	COLOURMODE_COLOUR,
	COLOURMODE_AUTO
} COLOURMODE;

/* Core <--> System-Main Interface */

#ifdef __cplusplus
extern "C" {
#endif
void neopop_reset(void);
#ifdef __cplusplus
}
#endif

/* Core <--> System-Graphics Interface */

/* Physical screen dimensions */
#define SCREEN_WIDTH    160
#define SCREEN_HEIGHT   152

extern COLOURMODE system_colour;
#endif
