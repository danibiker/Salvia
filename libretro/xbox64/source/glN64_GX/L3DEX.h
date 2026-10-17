/**
 * glN64_GX - L3DEX.h
 * Copyright (C) 2003 Orkin
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 *
**/

#ifndef L3DEX_H
#define L3DEX_H
#include "Types.h"

#define L3DEX_TRI1				0xBF	/* GLideN64 2be03555 */
#define L3DEX_TRI2				0xB1

void L3DEX_Line3D( u32 w0, u32 w1 );
void L3DEX_Tri1( u32 w0, u32 w1 );
void L3DEX_Tri2( u32 w0, u32 w1 );
void L3DEX_Init();
#endif

