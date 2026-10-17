/**
 * glN64_GX - L3D.cpp
 * Copyright (C) 2003 Orkin
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 *
**/

#ifdef __GX__
#include <gccore.h>
#endif // __GX__

#include "glN64.h"
#include "Debug.h"
#include "F3D.h"
#include "L3D.h"
#include "N64.h"
#include "RSP.h"	
#include "RDP.h"
#include "gSP.h"
#include "gDP.h"
#include "GBI.h"

void L3D_Line3D( u32 w0, u32 w1 )
{
	u32 wd = _SHIFTR( w1, 0, 8 );

	if (wd == 0)
		gSPLine3D( _SHIFTR( w1, 16, 8 ) / 10, _SHIFTR( w1, 8, 8 ) / 10, _SHIFTR( w1, 24, 8 ) );
	else
		gSPLineW3D( _SHIFTR( w1, 16, 8 ) / 10, _SHIFTR( w1, 8, 8 ) / 10, wd, _SHIFTR( w1, 24, 8 ) );
}

/* [Xbox360] GLideN64 2be03555: TRI1 del microcodigo de lineas = los tres
 * lados como lineas, sin ancho; `flag` elige el vertice de sombreado plano.
 * Ver la nota de L3DEX2.cpp. */
void L3D_Tri1( u32 w0, u32 w1 )
{
	u32 v0 = _SHIFTR( w1, 16, 8 ) / 10;
	u32 v1 = _SHIFTR( w1,  8, 8 ) / 10;
	u32 v2 = _SHIFTR( w0,  0, 8 ) / 10;
	u32 flag = _SHIFTR( w1, 24, 8 );
	u32 flatShadeVtx = v0;

	if (flag == 0x01)
		flatShadeVtx = v1;
	else if (flag == 0x02)
		flatShadeVtx = v2;

	if (v0 != v1)
		gSPLine3D( v0, v1, flatShadeVtx );
	if (v1 != v2)
		gSPLine3D( v1, v2, flatShadeVtx );
	if (v2 != v0)
		gSPLine3D( v2, v0, flatShadeVtx );
}

void L3D_Init()
{
	// Set GeometryMode flags
	GBI_InitFlags( F3D );

	GBI.PCStackSize = 10;

	//          GBI Command             Command Value			Command Function
	GBI_SetGBI( G_SPNOOP,				F3D_SPNOOP,				F3D_SPNoOp );
	GBI_SetGBI( G_MTX,					F3D_MTX,				F3D_Mtx );
	GBI_SetGBI( G_RESERVED0,			F3D_RESERVED0,			F3D_Reserved0 );
	GBI_SetGBI( G_MOVEMEM,				F3D_MOVEMEM,			F3D_MoveMem );
	GBI_SetGBI( G_VTX,					F3D_VTX,				F3D_Vtx );
	GBI_SetGBI( G_RESERVED1,			F3D_RESERVED1,			F3D_Reserved1 );
	GBI_SetGBI( G_DL,					F3D_DL,					F3D_DList );
	GBI_SetGBI( G_RESERVED2,			F3D_RESERVED2,			F3D_Reserved2 );
	GBI_SetGBI( G_RESERVED3,			F3D_RESERVED3,			F3D_Reserved3 );
	GBI_SetGBI( G_SPRITE2D_BASE,		F3D_SPRITE2D_BASE,		F3D_Sprite2D_Base );

	GBI_SetGBI( G_TRI1,					L3D_TRI1,				L3D_Tri1 );   /* GLideN64 2be03555 */
	GBI_SetGBI( G_CULLDL,				F3D_CULLDL,				F3D_CullDL );
	GBI_SetGBI( G_POPMTX,				F3D_POPMTX,				F3D_PopMtx );
	GBI_SetGBI( G_MOVEWORD,				F3D_MOVEWORD,			F3D_MoveWord );
	GBI_SetGBI( G_TEXTURE,				F3D_TEXTURE,			F3D_Texture );
	GBI_SetGBI( G_SETOTHERMODE_H,		F3D_SETOTHERMODE_H,		F3D_SetOtherMode_H );
	GBI_SetGBI( G_SETOTHERMODE_L,		F3D_SETOTHERMODE_L,		F3D_SetOtherMode_L );
	GBI_SetGBI( G_ENDDL,				F3D_ENDDL,				F3D_EndDL );
	GBI_SetGBI( G_SETGEOMETRYMODE,		F3D_SETGEOMETRYMODE,	F3D_SetGeometryMode );
	GBI_SetGBI( G_CLEARGEOMETRYMODE,	F3D_CLEARGEOMETRYMODE,	F3D_ClearGeometryMode );
	GBI_SetGBI( G_LINE3D,				L3D_LINE3D,				L3D_Line3D );
	GBI_SetGBI( G_RDPHALF_1,			F3D_RDPHALF_1,			F3D_RDPHalf_1 );
	GBI_SetGBI( G_RDPHALF_2,			F3D_RDPHALF_2,			F3D_RDPHalf_2 );
	GBI_SetGBI( G_RDPHALF_CONT,			F3D_RDPHALF_CONT,		F3D_RDPHalf_Cont );
//	GBI_SetGBI( G_TRI4,					F3D_TRI4,				F3D_Tri4 );
}

