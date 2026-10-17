#ifdef __GX__
#include <gccore.h>
#endif // __GX__

#include "glN64.h"
#include "Debug.h"
#include "F3D.h"
#include "F3DEX.h"
#include "F3DEX2.h"
#include "F3DCBFD.h"
#include "S2DEX.h"
#include "S2DEX2.h"
#include "N64.h"
#include "RSP.h"
#include "RDP.h"
#include "gSP.h"
#include "gDP.h"
#include "GBI.h"

void F3DCBFD_MoveMem( u32 w0, u32 w1 )
{
	switch (_SHIFTR( w0, 0, 8 ))
	{
		case F3DCBFD_MV_NORMAL:
			gSPSetVertexColorBase( w1 );
			break;
		case G_MV_LIGHT:
			/* [Xbox360] ★ [Conker, habitacion de Berri] Las luces puntuales de
			 * GLideN64 (F3DEX2CBFD_MoveMem): n = desplazamiento/48, las dos
			 * primeras son LookAt y el resto `gSPLightCBFD(n - 2)`.  glN64 las
			 * cargaba con `gSPLight`, como luces DIRECCIONALES, y la
			 * habitacion salia casi negra (con Rice se veia bien).  Confirmado
			 * por el usuario con el A/B, ya retirado. */
			{
				const u32 n = _SHIFTR( w0, 5, 14 ) / 48;
				if (n < 2)
					gSPLookAt( w1, n );
				else
					gSPLightCBFD( w1, (s32)n - 2 );
			}
			break;
		default:
			F3DEX2_MoveMem( w0, w1 );
			break;
	}
}

void F3DCBFD_Vtx( u32 w0, u32 w1 )
{
	u32 n = _SHIFTR( w0, 12, 8 );

	gSPNIVertex( w1, n, _SHIFTR( w0, 1, 7 ) - n );
}

/* [Xbox360] [Conker] `F3DEX2CBFD_CoordMod` de GLideN64 tal cual: el ajuste de
 * la posicion transformada con el que se calculan las luces puntuales.  En
 * glN64 este MoveWord (0x10) caia en F3DEX2_MoveWord, que no lo conoce. */
#define F3DCBFD_MW_COORD_MOD	0x10

static void F3DCBFD_CoordMod( u32 w0, u32 w1 )
{
	if ((w0 & 8) != 0)
		return;
	const u32 idx = _SHIFTR( w0, 1, 2 );
	const u32 pos = w0 & 0x30;
	if (pos == 0)
	{
		gSP.cbfd.vertexCoordMod[0 + idx] = (f32)(s16)_SHIFTR( w1, 16, 16 );
		gSP.cbfd.vertexCoordMod[1 + idx] = (f32)(s16)_SHIFTR( w1, 0, 16 );
	}
	else if (pos == 0x10)
	{
		gSP.cbfd.vertexCoordMod[4 + idx] = _FIXED2FLOAT( _SHIFTR( w1, 16, 16 ), 16 );
		gSP.cbfd.vertexCoordMod[5 + idx] = _FIXED2FLOAT( _SHIFTR( w1, 0, 16 ), 16 );
		gSP.cbfd.vertexCoordMod[12 + idx] = gSP.cbfd.vertexCoordMod[0 + idx] + gSP.cbfd.vertexCoordMod[4 + idx];
		gSP.cbfd.vertexCoordMod[13 + idx] = gSP.cbfd.vertexCoordMod[1 + idx] + gSP.cbfd.vertexCoordMod[5 + idx];
	}
	else if (pos == 0x20)
	{
		gSP.cbfd.vertexCoordMod[8 + idx] = (f32)(s16)_SHIFTR( w1, 16, 16 );
		gSP.cbfd.vertexCoordMod[9 + idx] = (f32)(s16)_SHIFTR( w1, 0, 16 );
	}
}

/* [Xbox360] [Conker] GLideN64 (`F3DEX2CBFD_SwitchLightingMode`): en Conker
 * `G_LOAD_UCODE` no cambia de microcodigo, enciende la luz avanzada hasta el
 * final de la display list (RSP_ProcessDList la apaga al empezar otra).
 * glN64 llamaba aqui a F3DEX_Load_uCode. */
static void F3DCBFD_LoadUcode( u32 w0, u32 w1 )
{
	(void)w0; (void)w1;
	gSP.cbfd.advancedLighting = TRUE;
}

void F3DCBFD_MoveWord( u32 w0, u32 w1 )
{
	switch (_SHIFTR( w0, 16, 8 ))
	{
		case G_MW_NUMLIGHT:
			gSPNumLights( w1 / 48 );
			break;
		case F3DCBFD_MW_COORD_MOD:
			F3DCBFD_CoordMod( w0, w1 );
			break;
		default:
			F3DEX2_MoveWord( w0, w1 );
			break;
	}
}

void F3DCBFD_Tri4( u32 w0, u32 w1 )
{
	gSP4Triangles( _SHIFTR( w0, 23, 5 ), _SHIFTR( w0, 18, 5 ), (_SHIFTR( w0, 15, 3 ) << 2) | (w1 >> 30),
	               _SHIFTR( w0, 10, 5 ), _SHIFTR( w0,  5, 5 ),  _SHIFTR( w0,  0, 5 ),
	               _SHIFTR( w1, 25, 5 ), _SHIFTR( w1, 20, 5 ),  _SHIFTR( w1, 15, 5 ),
	               _SHIFTR( w1, 10, 5 ), _SHIFTR( w1,  5, 5 ),  _SHIFTR( w1,  0, 5 ) );
}

void F3DCBFD_Init()
{
	// Set GeometryMode flags
	GBI_InitFlags(F3DEX2);

	GBI.PCStackSize = 10;

	//          GBI Command				Command Value				Command Function
	GBI_SetGBI( G_RDPHALF_2,			F3DEX2_RDPHALF_2,			F3D_RDPHalf_2 );
	GBI_SetGBI( G_SETOTHERMODE_H,		F3DEX2_SETOTHERMODE_H,		F3DEX2_SetOtherMode_H );
	GBI_SetGBI( G_SETOTHERMODE_L,		F3DEX2_SETOTHERMODE_L,		F3DEX2_SetOtherMode_L );
	GBI_SetGBI( G_RDPHALF_1,			F3DEX2_RDPHALF_1,			F3D_RDPHalf_1 );
	GBI_SetGBI( G_SPNOOP,				F3DEX2_SPNOOP,				F3D_SPNoOp );
	GBI_SetGBI( G_ENDDL,				F3DEX2_ENDDL,				F3D_EndDL );
	GBI_SetGBI( G_DL,					F3DEX2_DL,					F3D_DList );
	GBI_SetGBI( G_LOAD_UCODE,			F3DEX2_LOAD_UCODE,			F3DCBFD_LoadUcode );
	GBI_SetGBI( G_MOVEMEM,				F3DEX2_MOVEMEM,				F3DCBFD_MoveMem );
	GBI_SetGBI( G_MOVEWORD,				F3DEX2_MOVEWORD,			F3DCBFD_MoveWord );
	GBI_SetGBI( G_MTX,					F3DEX2_MTX,					F3DEX2_Mtx );
	GBI_SetGBI( G_GEOMETRYMODE,			F3DEX2_GEOMETRYMODE,		F3DEX2_GeometryMode );
	GBI_SetGBI( G_POPMTX,				F3DEX2_POPMTX,				F3DEX2_PopMtx );
	GBI_SetGBI( G_TEXTURE,				F3DEX2_TEXTURE,				F3DEX2_Texture );
	GBI_SetGBI( G_DMA_IO,				F3DEX2_DMA_IO,				F3DEX2_DMAIO );
	GBI_SetGBI( G_SPECIAL_1,			F3DEX2_SPECIAL_1,			F3DEX2_Special_1 );
	GBI_SetGBI( G_SPECIAL_2,			F3DEX2_SPECIAL_2,			F3DEX2_Special_2 );
	GBI_SetGBI( G_SPECIAL_3,			F3DEX2_SPECIAL_3,			F3DEX2_Special_3 );

	GBI_SetGBI( G_VTX,					F3DEX2_VTX,					F3DCBFD_Vtx );
	GBI_SetGBI( G_MODIFYVTX,			F3DEX2_MODIFYVTX,			F3DEX_ModifyVtx );
	GBI_SetGBI( G_CULLDL,				F3DEX2_CULLDL,				F3DEX_CullDL );
	GBI_SetGBI( G_BRANCH_Z,				F3DEX2_BRANCH_Z,			F3DEX_Branch_Z );
	GBI_SetGBI( G_TRI1,					F3DEX2_TRI1,				F3DEX2_Tri1 );
	GBI_SetGBI( G_TRI2,					F3DEX2_TRI2,				F3DEX_Tri2 );
	GBI_SetGBI( G_QUAD,					F3DEX2_QUAD,				F3DEX2_Quad );
//	GBI_SetGBI( G_LINE3D,				F3DEX2_LINE3D,				F3DEX2_Line3D );

	for (int i = 0x10; i <= 0x1F; i++)
		GBI.cmd[i] = F3DCBFD_Tri4;

	GBI_SetGBI( G_BG_1CYC,				S2DEX2_BG_1CYC,				S2DEX_BG_1Cyc );
	GBI_SetGBI( G_BG_COPY,				S2DEX2_BG_COPY,				S2DEX_BG_Copy );
	GBI_SetGBI( G_OBJ_RENDERMODE,		S2DEX2_OBJ_RENDERMODE,		S2DEX_Obj_RenderMode );
}

