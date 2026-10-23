/**
 * glN64_GX - F3DEX2.cpp
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
#include "F3DEX.h"
#include "F3DEX2.h"
#include "N64.h"
#include "RSP.h"	
#include "RDP.h"
#include "gSP.h"
#include "gDP.h"
#include "GBI.h"

void F3DEX2_Mtx( u32 w0, u32 w1 )
{
	gSPMatrix( w1, _SHIFTR( w0, 0, 8 ) ^ G_MTX_PUSH );
}

void F3DEX2_MoveMem( u32 w0, u32 w1 )
{
	switch (_SHIFTR( w0, 0, 8 ))
	{
		case F3DEX2_MV_VIEWPORT:
			gSPViewport( w1 );
			break;
		case G_MV_MATRIX:
			gSPForceMatrix( w1 );

			// force matrix takes two commands
			RSP.PC[RSP.PCi] += 8;
			break;
		case G_MV_LIGHT:
			u32 offset = _SHIFTR( w0, 8, 8 ) << 3;

			if (offset >= 48)
			{
				gSPLight( w1, (offset - 24) / 24);
			}
			else
			{
				gSPLookAt( w1, offset / 24 );
			}
			break;
	}
}

void F3DEX2_Vtx( u32 w0, u32 w1 )
{
	u32 n = _SHIFTR( w0, 12, 8 );

	gSPVertex( w1, n, _SHIFTR( w0, 1, 7 ) - n );
}

void F3DEX2_Reserved1( u32 w0, u32 w1 )
{
}

void F3DEX2_Tri1( u32 w0, u32 w1 )
{
	gSP1Triangle( _SHIFTR( w0, 17, 7 ), 
		          _SHIFTR( w0, 9, 7 ), 
				  _SHIFTR( w0, 1, 7 ) );
}

void F3DEX2_PopMtx( u32 w0, u32 w1 )
{
	gSPPopMatrixN( 0, w1 >> 6 );
}

void F3DEX2_MoveWord( u32 w0, u32 w1 )
{
	switch (_SHIFTR( w0, 16, 8 ))
	{
		case G_MW_FORCEMTX:
			if (w1 == 0)
				gSP.changed |= CHANGED_MATRIX;
			else
				gSP.changed &= ~CHANGED_MATRIX;
			break;
		case G_MW_MATRIX:
			gSPInsertMatrix( _SHIFTR( w0, 0, 16 ), w1 );
			break;
		case G_MW_NUMLIGHT:
			gSPNumLights( w1 / 24 );
			break;
		case G_MW_CLIP:
			gSPClipRatio( w1 );
			break;
		case G_MW_SEGMENT:
			gSPSegment( _SHIFTR( w0, 0, 16 ) >> 2, w1 & 0x00FFFFFF );
			break;
		case G_MW_FOG:
/*			s16 fm, fo, min, max;

			fm = _SHIFTR( w1, 16, 16 );
			fo = _SHIFTR( w1, 0, 16 );

			min = 500 - (fo * (128000 / fm)) / 256;
			max = (128000 / fm) + min;*/

			gSPFogFactor( (s16)_SHIFTR( w1, 16, 16 ), (s16)_SHIFTR( w1, 0, 16 ) );
			break;
		case G_MW_LIGHTCOL:
			switch (_SHIFTR( w0, 0, 16 ))
			{
				case F3DEX2_MWO_aLIGHT_1:
					gSPLightColor( LIGHT_1, w1 );
					break;
				case F3DEX2_MWO_aLIGHT_2:
					gSPLightColor( LIGHT_2, w1 );
					break;
				case F3DEX2_MWO_aLIGHT_3:
					gSPLightColor( LIGHT_3, w1 );
					break;
				case F3DEX2_MWO_aLIGHT_4:
					gSPLightColor( LIGHT_4, w1 );
					break;
				case F3DEX2_MWO_aLIGHT_5:
					gSPLightColor( LIGHT_5, w1 );
					break;
				case F3DEX2_MWO_aLIGHT_6:
					gSPLightColor( LIGHT_6, w1 );
					break;
				case F3DEX2_MWO_aLIGHT_7:
					gSPLightColor( LIGHT_7, w1 );
					break;
				case F3DEX2_MWO_aLIGHT_8:
					gSPLightColor( LIGHT_8, w1 );
					break;
			}
			break;
		case G_MW_PERSPNORM:
			gSPPerspNormalize( w1 );
			break;
	}
}

void F3DEX2_Texture( u32 w0, u32 w1 )
{
	gSPTexture( GXcastu16f32( _SHIFTR( w1, 16, 16 ) ), 
		        GXcastu16f32( _SHIFTR( w1, 0, 16 ) ), 
		        _SHIFTR( w0, 11, 3 ), 
				_SHIFTR( w0, 8, 3 ), 
				_SHIFTR( w0, 1, 7 ) );
}

/* [Xbox360] ★★ Mascara siempre, como GLideN64 (uCodes/F3DEX2.cpp:119): ver
 * gSPSetOtherMode_H en gSP.cpp (Paper Mario, la TLUT que se quedaba puesta). */
void F3DEX2_SetOtherMode_H( u32 w0, u32 w1 )
{
	const u32 length = _SHIFTR( w0, 0, 8 ) + 1;
	const s32 shiftS = (s32)(32 - _SHIFTR( w0, 8, 8 ) - length);

	gSPSetOtherMode_H( length, (shiftS > 0) ? (u32)shiftS : 0, w1 );
}

/* [Xbox360] ★★ [Mario Tennis, menus opacos] MASCARA SIEMPRE, COMO GLideN64.
 *
 * glN64 repartia por el desplazamiento: `case G_MDSFT_ALPHACOMPARE` (0),
 * `ZSRCSEL` (2) y `RENDERMODE` (3).  Pero el desplazamiento no dice CUANTOS
 * bits se escriben: una escritura ENTERA de otherMode.l (`E200001F`,
 * desplazamiento 0 y 32 bits) caia en `case 0` y solo tocaba alphaCompare.
 * Medido con [MTMODO] (log 3): los circulos del menu llegan con
 * `E200001C:00552078` (OPA_SURF) y JUSTO DESPUES `E200001F:00504244`
 * (FORCE_BL, blender 0050 = IN*A + MEM*(1-A)); el dibujo salia con el
 * primero -- opaco, el blanco y el magenta a la vista --.
 *
 * GLideN64 (uCodes/F3DEX2.cpp:126 + gSP.cpp:2323, gSPSetOtherMode_L) no
 * reparte: mascara y dato, siempre.  Se porta tal cual, con el `(u64)` que
 * evita el desplazamiento de 32 y el `max(0, ...)` del desplazamiento.  Los
 * dos flags siempre: glN64 reaplica el alpha compare y el z de primitiva en el
 * bloque de CHANGED_RENDERMODE/ALPHACOMPARE, y gDPSetDepthSource marcaba
 * CHANGED_RENDERMODE. */
void F3DEX2_SetOtherMode_L( u32 w0, u32 w1 )
{
	const u32 length = _SHIFTR( w0, 0, 8 ) + 1;
	const s32 shiftS = (s32)(32 - _SHIFTR( w0, 8, 8 ) - length);
	const u32 shift = (shiftS > 0) ? (u32)shiftS : 0;
	const u32 mask = (u32)((((u64)1 << length) - 1) << shift);

	gDP.otherMode.l = (gDP.otherMode.l & ~mask) | (w1 & mask);

	gDP.changed |= CHANGED_RENDERMODE | CHANGED_ALPHACOMPARE;
}

void F3DEX2_GeometryMode( u32 w0, u32 w1 )
{
	gSPGeometryMode( ~_SHIFTR( w0, 0, 24 ), w1 );
}

void F3DEX2_DMAIO( u32 w0, u32 w1 )
{
	gSP.DMAIO_address = RSP_SegmentToPhysical( w1 );
}

void F3DEX2_Special_1( u32 w0, u32 w1 )
{
	u32 param = _SHIFTR( w0, 0, 8 );
	if (GBI.current->combineMatrices)
		gSPCombineMatrices( param );
	else
		gSPDlistCount( param, w1 );
}

void F3DEX2_Special_2( u32 w0, u32 w1 )
{
}

void F3DEX2_Special_3( u32 w0, u32 w1 )
{
}

void F3DEX2_Quad( u32 w0, u32 w1 )
{
	gSP2Triangles( _SHIFTR( w0, 17, 7 ), 
		           _SHIFTR( w0, 9, 7 ), 
				   _SHIFTR( w0, 1, 7 ), 
				   _SHIFTR( w1, 17, 7 ), 
		           _SHIFTR( w1, 9, 7 ), 
				   _SHIFTR( w1, 1, 7 ) );
}

void F3DEX2_Init()
{
	// Set GeometryMode flags
	GBI_InitFlags( F3DEX2 );

	GBI.PCStackSize = 18;

	// GBI Command						Command Value				Command Function
	GBI_SetGBI( G_RDPHALF_2,			F3DEX2_RDPHALF_2,			F3D_RDPHalf_2 );
	GBI_SetGBI( G_SETOTHERMODE_H,		F3DEX2_SETOTHERMODE_H,		F3DEX2_SetOtherMode_H );
	GBI_SetGBI( G_SETOTHERMODE_L,		F3DEX2_SETOTHERMODE_L,		F3DEX2_SetOtherMode_L );
	GBI_SetGBI( G_RDPHALF_1,			F3DEX2_RDPHALF_1,			F3D_RDPHalf_1 );
	GBI_SetGBI( G_SPNOOP,				F3DEX2_SPNOOP,				F3D_SPNoOp );
	GBI_SetGBI( G_ENDDL,				F3DEX2_ENDDL,				F3D_EndDL );
	GBI_SetGBI( G_DL,					F3DEX2_DL,					F3D_DList );
	GBI_SetGBI( G_LOAD_UCODE,			F3DEX2_LOAD_UCODE,			F3DEX_Load_uCode );
	GBI_SetGBI( G_MOVEMEM,				F3DEX2_MOVEMEM,				F3DEX2_MoveMem );
	GBI_SetGBI( G_MOVEWORD,				F3DEX2_MOVEWORD,			F3DEX2_MoveWord );
	GBI_SetGBI( G_MTX,					F3DEX2_MTX,					F3DEX2_Mtx );
	GBI_SetGBI( G_GEOMETRYMODE,			F3DEX2_GEOMETRYMODE,		F3DEX2_GeometryMode );
	GBI_SetGBI( G_POPMTX,				F3DEX2_POPMTX,				F3DEX2_PopMtx );
	GBI_SetGBI( G_TEXTURE,				F3DEX2_TEXTURE,				F3DEX2_Texture );
	GBI_SetGBI( G_DMA_IO,				F3DEX2_DMA_IO,				F3DEX2_DMAIO );
	GBI_SetGBI( G_SPECIAL_1,			F3DEX2_SPECIAL_1,			F3DEX2_Special_1 );
	GBI_SetGBI( G_SPECIAL_2,			F3DEX2_SPECIAL_2,			F3DEX2_Special_2 );
	GBI_SetGBI( G_SPECIAL_3,			F3DEX2_SPECIAL_3,			F3DEX2_Special_3 );

	GBI_SetGBI( G_VTX,					F3DEX2_VTX,					F3DEX2_Vtx );
	GBI_SetGBI( G_MODIFYVTX,			F3DEX2_MODIFYVTX,			F3DEX_ModifyVtx );
	GBI_SetGBI(	G_CULLDL,				F3DEX2_CULLDL,				F3DEX_CullDL );
	GBI_SetGBI( G_BRANCH_Z,				F3DEX2_BRANCH_Z,			F3DEX_Branch_Z );
	GBI_SetGBI( G_TRI1,					F3DEX2_TRI1,				F3DEX2_Tri1 );
	GBI_SetGBI( G_TRI2,					F3DEX2_TRI2,				F3DEX_Tri2 );
	GBI_SetGBI( G_QUAD,					F3DEX2_QUAD,				F3DEX2_Quad );
//	GBI_SetGBI( G_LINE3D,				F3DEX2_LINE3D,				F3DEX2_Line3D );
}

