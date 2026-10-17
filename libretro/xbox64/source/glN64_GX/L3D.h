/**
 * glN64_GX - L3D.h
 * Copyright (C) 2003 Orkin
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 *
**/

#ifndef L3D_H
#define L3D_H
#include "Types.h"

#define L3D_LINE3D				0xB5
#define L3D_TRI1				0xBF	/* GLideN64 2be03555 */

void L3D_Line3D( u32 w0, u32 w1 );
void L3D_Tri1( u32 w0, u32 w1 );
void L3D_Init();
#endif

