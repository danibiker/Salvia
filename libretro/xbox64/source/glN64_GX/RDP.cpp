/**
 * glN64_GX - RDP.cpp
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifdef __GX__
#include <gccore.h>
extern "C" {
#include "../main/gamehacks.h"
}
#endif // __GX__

#include <string.h>
#include "N64.h"
#include "RSP.h"
#include "RDP.h"
#include "GBI.h"
#include "gDP.h"
#include "Types.h"
#include "Debug.h"

void RDP_Unknown( u32 w0, u32 w1 )
{
#ifdef DEBUG
	DebugMsg( DEBUG_UNKNOWN, "RDP_Unknown\r\n" );
	DebugMsg( DEBUG_UNKNOWN, "\tUnknown RDP opcode %02X\r\n", _SHIFTR( w0, 24, 8 ) );
#endif
}

void RDP_NoOp( u32 w0, u32 w1 )
{
	gDPNoOp();
}

void RDP_SetCImg( u32 w0, u32 w1 )
{
	gDPSetColorImage( _SHIFTR( w0, 21,  3 ),		// fmt
		              _SHIFTR( w0, 19,  2 ),		// siz
                      _SHIFTR( w0,  0, 12 ) + 1,	// width
					  w1 );							// img
}

void RDP_SetZImg( u32 w0, u32 w1 )
{
	gDPSetDepthImage( w1 );	// img
}

void RDP_SetTImg( u32 w0, u32 w1 )
{
	gDPSetTextureImage( _SHIFTR( w0, 21,  3),		// fmt
						_SHIFTR( w0, 19,  2 ),		// siz
						_SHIFTR( w0,  0, 12 ) + 1,	// width
						w1 );						// img
}

void RDP_SetCombine( u32 w0, u32 w1 )
{
	gDPSetCombine( _SHIFTR( w0, 0, 24 ),	// muxs0
				   w1 );					// muxs1
}

void RDP_SetEnvColor( u32 w0, u32 w1 )
{
	gDPSetEnvColor( _SHIFTR( w1, 24, 8 ),		// r
					_SHIFTR( w1, 16, 8 ),		// g
					_SHIFTR( w1,  8, 8 ),		// b
					_SHIFTR( w1,  0, 8 ) );		// a
}

void RDP_SetPrimColor( u32 w0, u32 w1 )
{
	/* [Xbox360] GLideN64 1ac9e4e6 ("Fix min_lod and lod_frac read in
	 * RDP_SetPrimColor"): m es el min_level de 5 bits en w0[12:8] y l el
	 * prim_lod_frac en w0[7:0].  glN64 leia m con _SHIFTL (desplazado 8 a la
	 * IZQUIERDA); nadie lo usaba hasta LOD_FRACTION (uMinLod, GXD3D_SetLod). */
	gDPSetPrimColor( _SHIFTR( w0,  8, 5 ),		// m
		             _SHIFTR( w0,  0, 8 ),		// l
					 _SHIFTR( w1, 24, 8 ),		// r
					 _SHIFTR( w1, 16, 8 ),		// g
					 _SHIFTR( w1,  8, 8 ),		// b
					 _SHIFTR( w1,  0, 8 ) );	// a

}

void RDP_SetBlendColor( u32 w0, u32 w1 )
{
	gDPSetBlendColor( _SHIFTR( w1, 24, 8 ),		// r
			 		  _SHIFTR( w1, 16, 8 ),		// g
					  _SHIFTR( w1,  8, 8 ),		// b
					  _SHIFTR( w1,  0, 8 ) );	// a
}

void RDP_SetFogColor( u32 w0, u32 w1 )
{
	gDPSetFogColor( _SHIFTR( w1, 24, 8 ),		// r
					_SHIFTR( w1, 16, 8 ),		// g
					_SHIFTR( w1,  8, 8 ),		// b
					_SHIFTR( w1,  0, 8 ) );		// a
}

void RDP_SetFillColor( u32 w0, u32 w1 )
{
	gDPSetFillColor( w1 );
}

void RDP_FillRect( u32 w0, u32 w1 )
{
	gDPFillRectangle( _SHIFTR( w1, 14, 10 ),		// ulx
					  _SHIFTR( w1,  2, 10 ),		// uly
					  _SHIFTR( w0, 14, 10 ) + 1,	// lrx
					  _SHIFTR( w0,  2, 10 ) + 1 );	// lry
}

void RDP_SetTile( u32 w0, u32 w1 )
{
	gDPSetTile( _SHIFTR( w0, 21, 3 ),	// fmt
	            _SHIFTR( w0, 19, 2 ),	// siz
	            _SHIFTR( w0,  9, 9 ),	// line
				_SHIFTR( w0,  0, 9 ),	// tmem
				_SHIFTR( w1, 24, 3 ),	// tile
				_SHIFTR( w1, 20, 4 ),	// palette
				_SHIFTR( w1, 18, 2 ),	// cmt
				_SHIFTR( w1,  8, 2 ),	// cms
				_SHIFTR( w1, 14, 4 ),	// maskt
				_SHIFTR( w1,  4, 4 ),	// masks
				_SHIFTR( w1, 10, 4 ),	// shiftt
				_SHIFTR( w1,  0, 4 ) );	// shifts
}

void RDP_LoadTile( u32 w0, u32 w1 )
{
	gDPLoadTile( _SHIFTR( w1, 24,  3 ),		// tile
		         _SHIFTR( w0, 12, 12 ),		// uls
				 _SHIFTR( w0,  0, 12 ),		// ult
				 _SHIFTR( w1, 12, 12 ),		// lrs
				 _SHIFTR( w1,  0, 12 ) );	// lrt
}

void RDP_LoadBlock( u32 w0, u32 w1 )
{
	gDPLoadBlock( _SHIFTR( w1, 24,  3 ),	// tile
		          _SHIFTR( w0, 12, 12 ),	// uls
				  _SHIFTR( w0,  0, 12 ),	// ult
				  _SHIFTR( w1, 12, 12 ),	// lrs
				  _SHIFTR( w1,  0, 12 ) );	// dxt
}

void RDP_SetTileSize( u32 w0, u32 w1 )
{
	gDPSetTileSize( _SHIFTR( w1, 24,  3 ),		// tile
		            _SHIFTR( w0, 12, 12 ),		// uls
				    _SHIFTR( w0,  0, 12 ),		// ult
				    _SHIFTR( w1, 12, 12 ),		// lrs
				    _SHIFTR( w1,  0, 12 ) );	// lrt
}

void RDP_LoadTLUT( u32 w0, u32 w1 )
{
	gDPLoadTLUT( _SHIFTR( w1, 24,  3 ),	// tile
		          _SHIFTR( w0, 12, 12 ),	// uls
				  _SHIFTR( w0,  0, 12 ),	// ult
				  _SHIFTR( w1, 12, 12 ),	// lrs
				  _SHIFTR( w1,  0, 12 ) );	// lrt
}

void RDP_SetOtherMode( u32 w0, u32 w1 )
{
	gDPSetOtherMode( _SHIFTR( w0, 0, 24 ),	// mode0
					 w1 );					// mode1
}

void RDP_SetPrimDepth( u32 w0, u32 w1 )
{
	gDPSetPrimDepth( _SHIFTR( w1, 16, 16 ),		// z
		             _SHIFTR( w1,  0, 16 ) );	// dz
}

void RDP_SetScissor( u32 w0, u32 w1 )
{
	gDPSetScissor( _SHIFTR( w1, 24, 2 ),							// mode
		           _FIXED2FLOAT( (u16)_SHIFTR( w0, 12, 12 ), 2 ),	// ulx
				   _FIXED2FLOAT( (u16)_SHIFTR( w0,  0, 12 ), 2 ),	// uly
				   _FIXED2FLOAT( (u16)_SHIFTR( w1, 12, 12 ), 2 ),	// lrx
				   _FIXED2FLOAT( (u16)_SHIFTR( w1,  0, 12 ), 2 ) );	// lry
}

void RDP_SetConvert( u32 w0, u32 w1 )
{
	gDPSetConvert( _SHIFTR( w0, 13, 9 ),	// k0
		           _SHIFTR( w0,  4, 9 ),	// k1
				   _SHIFTL( w0,  5, 4 ) | _SHIFTR( w1, 25, 5 ),	// k2
				   _SHIFTR( w1, 18, 9 ),	// k3
				   _SHIFTR( w1,  9, 9 ),	// k4
				   _SHIFTR( w1,  0, 9 ) );	// k5
}

void RDP_SetKeyR( u32 w0, u32 w1 )
{
	gDPSetKeyR( _SHIFTR( w1,  8,  8 ),		// cR
			    _SHIFTR( w1,  0,  8 ),		// sR
				_SHIFTR( w1, 16, 12 ) );	// wR
}

void RDP_SetKeyGB( u32 w0, u32 w1 )
{
	gDPSetKeyGB( _SHIFTR( w1, 24,  8 ),		// cG
		         _SHIFTR( w1, 16,  8 ),		// sG
				 _SHIFTR( w0, 12, 12 ),		// wG
				 _SHIFTR( w1,  8,  8 ),		// cB
				 _SHIFTR( w1,  0,  8 ),		// SB
				 _SHIFTR( w0,  0, 12 ) );	// wB
}

void RDP_FullSync( u32 w0, u32 w1 )
{
	gDPFullSync();
}

void RDP_TileSync( u32 w0, u32 w1 )
{
	gDPTileSync();
}

void RDP_PipeSync( u32 w0, u32 w1 )
{
	gDPPipeSync();
}

void RDP_LoadSync( u32 w0, u32 w1 )
{
	gDPLoadSync();
}

// The stripped S2DEX background code puts together texture rectangles rather than
// reading them out of the DL, so it hands the parameters over directly.
// GLideN64 does the same through RDP.w2/w3 under RSP.LLE
static BOOL RDP_texRectParamsArmed = FALSE;
static u32  RDP_texRectW2 = 0;
static u32  RDP_texRectW3 = 0;

void RDP_SetTexRectParams( u32 w2, u32 w3 )
{
	RDP_texRectParamsArmed = TRUE;
	RDP_texRectW2 = w2;
	RDP_texRectW3 = w3;
}
// Mirrors _getTexRectParams() in GLideN64's RDP.cpp
static void _getTexRectParams( u32 &w2, u32 &w3 )
{
	if (RDP_texRectParamsArmed)
	{
		w2 = RDP_texRectW2;
		w3 = RDP_texRectW3;
		RDP_texRectParamsArmed = FALSE;
		return;
	}
	/* [Xbox360] ★ [All-Star Baseball 99/2001, textos de menu como rayas o
	 * cuadrados de un color] LOS TRES MODOS DE GLideN64 (src/RDP.cpp
	 * _getTexRectParams; el medio texrect es e6fef73b "Fix texrect for
	 * AllStarBaseball 2000-2001, issue #196").  Aqui solo estaba el de 16 bytes
	 * (RDPHALF_1 + RDPHALF_2).  Con el medio -- solo un RDPHALF_2 (0xB3 en F3D,
	 * 0xF1 en F3DEX2) con dsdx/dtdy, s = t = 0 -- se tomaba dsdx/dtdy como s/t,
	 * w3 salia del comando SIGUIENTE y ese comando se saltaba.  El de WinBack
	 * sigue en RDP_TexRect.  ★RESUELTO (log 40): ASB 2001 medio con 0xF1, ASB
	 * 99 con 0xB3; Mario 64 y Paper Mario sin ningun texrect gdp.
	 *
	 * ★ [Diddy Kong Racing, textos como cuadrados de un color, 2026-10-16]
	 * PERO 0xB3 SEGUIDO DE 0xB2 ES EL TEXRECT COMPLETO DEL F3D ANTIGUO:
	 * RDPHALF_2 (s,t) + RDPHALF_CONT (dsdx,dtdy), el mismo par que reconoce el
	 * arreglo de WinBack en RDP_TexRect.  GLideN64 lo toma como medio texrect
	 * (s = t = 0 y las s,t como escala: un texel estirado), y antes de portar
	 * los tres modos aqui se leia como completo, que es lo que iba bien.  ASB
	 * manda 0xB3 SOLO, asi que no le afecta.  ★RESUELTO (log 8): DKR 92-980
	 * texrects/s por 0xB3+0xB2 y ningun medio; ASB 99 90-1488 medios y ningun
	 * 0xB3+0xB2; los dos con los textos bien. */
	enum {
		gspTexRect,
		gdpTexRect,
		halfTexRect
	} texRectMode = gdpTexRect;

	const u32 cmd1 = (*(u32*)&RDRAM[RSP.PC[RSP.PCi] + 0]) >> 24;
	const u32 cmd2 = (*(u32*)&RDRAM[RSP.PC[RSP.PCi] + 8]) >> 24;
	if (cmd1 == G_RDPHALF_1) {
		if (cmd2 == G_RDPHALF_2)
			texRectMode = gspTexRect;
	} else if (cmd1 == 0xB3) {
		if (cmd2 == 0xB2)
			texRectMode = gspTexRect;
		else
			texRectMode = halfTexRect;
	} else if (cmd1 == 0xF1)
		texRectMode = halfTexRect;

	switch (texRectMode) {
	case gspTexRect:
		w2 = *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 4];
		RSP.PC[RSP.PCi] += 8;

		w3 = *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 4];
		RSP.PC[RSP.PCi] += 8;
		break;
	case gdpTexRect:
		if (GBI.current != NULL &&
			(GBI.current->type == F5Rogue || GBI.current->type == F5Indi_Naboo))
		{
			w2 = *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 8];
			w3 = *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 12];
			RSP.PC[RSP.PCi] += 8;
			return;
		}
		w2 = *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 0];
		w3 = *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 4];
		RSP.PC[RSP.PCi] += 8;
		break;
	case halfTexRect:
		w2 = 0;
		w3 = *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 4];
		RSP.PC[RSP.PCi] += 8;
		break;
	}
}

/* [Xbox360] ★ [Mario Tennis, tira negra de la pantalla VS] RECT INVERTIDO = NO
 * SE DIBUJA, como GLideN64 (RDP.cpp:301-306, `if (lrx < ulx || lry < uly)
 * return;`), sobre las coordenadas de 12 bits SIN signo tal como llegan.  El
 * juego pinta la diagonal con rects que empiezan en y=-16: en 12 bits eso es
 * uly=1008 con lry=0.  gDPTextureRectangle le daba la vuelta (rama `lrt <= t`)
 * y salia una franja 304-367 de 1008 de alto, con la segunda textura leida de
 * la RDRAM (el inicio de la carga cae fuera del framebuffer): la tira negra.
 * Se comprueba DESPUES de _getTexRectParams, que avanza el PC igual. */
static BOOL _texRectInvertido( u32 w0, u32 w1 )
{
	const u32 ulx = _SHIFTR( w1, 12, 12 );
	const u32 uly = _SHIFTR( w1,  0, 12 );
	const u32 lrx = _SHIFTR( w0, 12, 12 );
	const u32 lry = _SHIFTR( w0,  0, 12 );
	return (lrx < ulx || lry < uly) ? TRUE : FALSE;
}

void RDP_TexRectFlip( u32 w0, u32 w1 )
{
	u32 w2, w3;

	_getTexRectParams( w2, w3 );
	if (_texRectInvertido( w0, w1 ))
		return;

	gDPTextureRectangleFlip( _FIXED2FLOAT( (u16)_SHIFTR( w1, 12, 12 ), 2 ),		// ulx
							 _FIXED2FLOAT( (u16)_SHIFTR( w1,  0, 12 ), 2 ),		// uly
							 _FIXED2FLOAT( (u16)_SHIFTR( w0, 12, 12 ), 2 ),		// lrx
							 _FIXED2FLOAT( (u16)_SHIFTR( w0,  0, 12 ), 2 ),		// lry
							 _SHIFTR( w1, 24,  3 ),								// tile
							 _FIXED2FLOAT( (s16)_SHIFTR( w2, 16, 16 ), 5 ),		// s
							 _FIXED2FLOAT( (s16)_SHIFTR( w2,  0, 16 ), 5 ),		// t
							 _FIXED2FLOAT( (s16)_SHIFTR( w3, 16, 16 ), 10 ),	// dsdx
							 _FIXED2FLOAT( (s16)_SHIFTR( w3,  0, 16 ), 10 ) );	// dsdy
}

void RDP_TexRect( u32 w0, u32 w1 )
{
	u32 cmd1 = (*(u32*)&RDRAM[RSP.PC[RSP.PCi] + 0]) >> 24;
	u32 cmd2 = (*(u32*)&RDRAM[RSP.PC[RSP.PCi] + 8]) >> 24;
	u32 w2, w3;

	_getTexRectParams( w2, w3 );
	if (_texRectInvertido( w0, w1 ))
		return;
#ifdef __GX__
	// Winback floating square hack, retrofitted version of https://github.com/gonetz/GLideN64/issues/63
	if(GetGameSpecificHack() == &hack_winback) {
		enum {
			gspTexRect,
			gdpTexRect,
			halfTexRect
		} texRectMode = gdpTexRect;
		if (cmd1 == G_RDPHALF_1) {
			if (cmd2 == G_RDPHALF_2)
				texRectMode = gspTexRect;
		} else if (cmd1 == 0xB3) {
			if (cmd2 == 0xB2)
				texRectMode = gspTexRect;
			else
				texRectMode = halfTexRect;
		} else if (cmd1 == 0xF1)
			texRectMode = halfTexRect;
			
		if(texRectMode == gdpTexRect) {
			return;
		}
	}
#endif

	gDPTextureRectangle( _FIXED2FLOAT( (u16)_SHIFTR( w1, 12, 12 ), 2 ),		// ulx
						 _FIXED2FLOAT( (u16)_SHIFTR( w1,  0, 12 ), 2 ),		// uly
						 _FIXED2FLOAT( (u16)_SHIFTR( w0, 12, 12 ), 2 ),		// lrx
						 _FIXED2FLOAT( (u16)_SHIFTR( w0,  0, 12 ), 2 ),		// lry
						 _SHIFTR( w1, 24,  3 ),								// tile
						 _FIXED2FLOAT( (s16)_SHIFTR( w2, 16, 16 ), 5 ),		// s
						 _FIXED2FLOAT( (s16)_SHIFTR( w2,  0, 16 ), 5 ),		// t
						 _FIXED2FLOAT( (s16)_SHIFTR( w3, 16, 16 ), 10 ),	// dsdx
						 _FIXED2FLOAT( (s16)_SHIFTR( w3,  0, 16 ), 10 ) );	// dsdy
}

void RDP_Init()
{
	// Initialize RDP commands to RDP_UNKNOWN
	for (int i = 0xC8; i <= 0xCF; i++)
		GBI.cmd[i] = RDP_Unknown;

	// Initialize RDP commands to RDP_UNKNOWN
	for (int i = 0xE4; i <= 0xFF; i++)
		GBI.cmd[i] = RDP_Unknown;

	// Set known GBI commands
	GBI.cmd[G_NOOP]				= RDP_NoOp;
	GBI.cmd[G_SETCIMG]			= RDP_SetCImg;
	GBI.cmd[G_SETZIMG]			= RDP_SetZImg;
	GBI.cmd[G_SETTIMG]			= RDP_SetTImg;
	GBI.cmd[G_SETCOMBINE]		= RDP_SetCombine;
	GBI.cmd[G_SETENVCOLOR]		= RDP_SetEnvColor;
	GBI.cmd[G_SETPRIMCOLOR]		= RDP_SetPrimColor;
	GBI.cmd[G_SETBLENDCOLOR]	= RDP_SetBlendColor;
	GBI.cmd[G_SETFOGCOLOR]		= RDP_SetFogColor;
	GBI.cmd[G_SETFILLCOLOR]		= RDP_SetFillColor;
	GBI.cmd[G_FILLRECT]			= RDP_FillRect;
	GBI.cmd[G_SETTILE]			= RDP_SetTile;
	GBI.cmd[G_LOADTILE]			= RDP_LoadTile;
	GBI.cmd[G_LOADBLOCK]		= RDP_LoadBlock;
	GBI.cmd[G_SETTILESIZE]		= RDP_SetTileSize;
	GBI.cmd[G_LOADTLUT]			= RDP_LoadTLUT;
	GBI.cmd[G_RDPSETOTHERMODE]	= RDP_SetOtherMode;
	GBI.cmd[G_SETPRIMDEPTH]		= RDP_SetPrimDepth;
	GBI.cmd[G_SETSCISSOR]		= RDP_SetScissor;
	GBI.cmd[G_SETCONVERT]		= RDP_SetConvert;
	GBI.cmd[G_SETKEYR]			= RDP_SetKeyR;
	GBI.cmd[G_SETKEYGB]			= RDP_SetKeyGB;
	GBI.cmd[G_RDPFULLSYNC]		= RDP_FullSync;
	GBI.cmd[G_RDPTILESYNC]		= RDP_TileSync;
	GBI.cmd[G_RDPPIPESYNC]		= RDP_PipeSync;
	GBI.cmd[G_RDPLOADSYNC]		= RDP_LoadSync;
	GBI.cmd[G_TEXRECTFLIP]		= RDP_TexRectFlip;
	GBI.cmd[G_TEXRECT]			= RDP_TexRect;
}

/* [Xbox360] ★★ [Killer Instinct, estela] LAS LISTAS QUE LA CPU MANDA DIRECTAMENTE
 * AL RDP (DPC_START/DPC_END), SIN DISPLAY LIST.  GLideN64 `RDP_ProcessRDPList`
 * (src/RDP.cpp:571), con su anillo, su `CmdLength` y su tabla `LLEcmd`.
 *
 * El `ProcessRDPList` de glN64 tenia el cuerpo comentado, o sea que estas
 * ordenes se tiraban.  Medido con una sonda en Killer Instinct Gold: ~30
 * listas/s, cada una con un RELLENO de pantalla completa (0,0-639,479, color
 * 00010001) sobre el buffer que va a dibujar -- el borrado de cada frame --, y
 * en partida tambien texrects (los fondos).  Sin ellas el EFB acumulaba los
 * frames anteriores: la estela.
 *
 * Diferencias con la referencia, las dos medidas o forzadas:
 *   - El anillo es de 16K palabras (64 KB), no de 1M: estas listas son de
 *     56-200 bytes (sonda [RDPLIST]).  Una lista mas larga que el anillo se
 *     pisaria igual que en la referencia, porque la mascara es la misma idea.
 *   - Los triangulos de bajo nivel (0x08-0x0F) van a gDPLLETriangle, el
 *     LLETriangle::draw de GLideN64.  La sonda [RDPLIST] dio `tri=0` fuera de
 *     combate; segun gonetz (GLideN64 #406) el cielo de los combates de Killer
 *     Instinct es un 0x0E de una de estas listas.  Confirmado: con ellos el
 *     cielo y las ventanas dejan de tener estela (log 6).
 * El texrect de bajo nivel ocupa 16 bytes: sus w2/w3 van en la propia orden, y
 * se le pasan con RDP_SetTexRectParams, el mismo puente que usa S2DEX.
 * La cola del anillo (+48) cabe la orden mas larga, 44 palabras, cuando da la
 * vuelta (la referencia usa +32, que se queda corto para 44). */
#define RDP_MAXCMD 0x4000
static const u32 RDP_maxCMDMask = RDP_MAXCMD - 1;
static u32 RDP_cmdData[RDP_MAXCMD + 48];
static u32 RDP_cmdPtr = 0;
static u32 RDP_cmdCur = 0;

static const u32 RDP_CmdLength[64] =
{
	8, 8, 8, 8, 8, 8, 8, 8,
	32, 32+16, 32+64, 32+64+16, 32+64, 32+64+16, 32+64+64, 32+64+64+16,
	8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 16, 16, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8
};

static void RDP_LLE_TexRect( u32 w0, u32 w1 )
{
	RDP_SetTexRectParams( RDP_cmdData[RDP_cmdCur + 2], RDP_cmdData[RDP_cmdCur + 3] );
	RDP_TexRect( w0, w1 );
}

static void RDP_LLE_TexRectFlip( u32 w0, u32 w1 )
{
	RDP_SetTexRectParams( RDP_cmdData[RDP_cmdCur + 2], RDP_cmdData[RDP_cmdCur + 3] );
	RDP_TexRectFlip( w0, w1 );
}

/* [Xbox360] [Killer Instinct, cielo de los combates] Los triangulos de bajo
 * nivel: los gDPTri* de GLideN64 (src/gDP.cpp:1538-1613), que reparten la orden
 * en el bloque de 44 palabras que espera LLETriangle::draw -- aristas (8),
 * shade (16), textura (16), z (4) -- con ceros en lo que la orden no trae. */
static void _RDP_LLETri( u32 aristas, u32 shade, u32 textura, u32 z,
                         BOOL bShade, BOOL bTex, BOOL bZ )
{
	u32 d[44];
	const u32 *src = RDP_cmdData + RDP_cmdCur;
	memset( d, 0, sizeof( d ) );
	memcpy( &d[0], src, aristas * 4 );             src += aristas;
	if (shade)   { memcpy( &d[8],  src, shade * 4 );   src += shade; }
	if (textura) { memcpy( &d[24], src, textura * 4 ); src += textura; }
	if (z)       { memcpy( &d[40], src, z * 4 ); }
	gDPLLETriangle( bShade, bTex, bZ, d );
}

static void RDP_LLE_TriFill( u32 w0, u32 w1 )         { _RDP_LLETri( 8,  0,  0, 0, FALSE, FALSE, FALSE ); }
static void RDP_LLE_TriFillZ( u32 w0, u32 w1 )        { _RDP_LLETri( 8,  0,  0, 4, FALSE, FALSE, TRUE  ); }
static void RDP_LLE_TriTxtr( u32 w0, u32 w1 )         { _RDP_LLETri( 8,  0, 16, 0, FALSE, TRUE,  FALSE ); }
static void RDP_LLE_TriTxtrZ( u32 w0, u32 w1 )        { _RDP_LLETri( 8,  0, 16, 4, FALSE, TRUE,  TRUE  ); }
static void RDP_LLE_TriShade( u32 w0, u32 w1 )        { _RDP_LLETri( 8, 16,  0, 0, TRUE,  FALSE, FALSE ); }
static void RDP_LLE_TriShadeZ( u32 w0, u32 w1 )       { _RDP_LLETri( 8, 16,  0, 4, TRUE,  FALSE, TRUE  ); }
static void RDP_LLE_TriShadeTxtr( u32 w0, u32 w1 )    { _RDP_LLETri( 8, 16, 16, 0, TRUE,  TRUE,  FALSE ); }
static void RDP_LLE_TriShadeTxtrZ( u32 w0, u32 w1 )   { _RDP_LLETri( 8, 16, 16, 4, TRUE,  TRUE,  TRUE  ); }

static GBIFunc RDP_LLEcmd[64] =
{
	/* 0x00 */
	RDP_NoOp,			RDP_Unknown,		RDP_Unknown,		RDP_Unknown,
	RDP_Unknown,		RDP_Unknown,		RDP_Unknown,		RDP_Unknown,
	RDP_LLE_TriFill,	RDP_LLE_TriFillZ,	RDP_LLE_TriTxtr,	RDP_LLE_TriTxtrZ,
	RDP_LLE_TriShade,	RDP_LLE_TriShadeZ,	RDP_LLE_TriShadeTxtr,	RDP_LLE_TriShadeTxtrZ,
	/* 0x10 */
	RDP_Unknown,		RDP_Unknown,		RDP_Unknown,		RDP_Unknown,
	RDP_Unknown,		RDP_Unknown,		RDP_Unknown,		RDP_Unknown,
	RDP_Unknown,		RDP_Unknown,		RDP_Unknown,		RDP_Unknown,
	RDP_Unknown,		RDP_Unknown,		RDP_Unknown,		RDP_Unknown,
	/* 0x20 */
	RDP_Unknown,		RDP_Unknown,		RDP_Unknown,		RDP_Unknown,
	RDP_LLE_TexRect,	RDP_LLE_TexRectFlip,	RDP_LoadSync,	RDP_PipeSync,
	RDP_TileSync,		RDP_FullSync,		RDP_SetKeyGB,		RDP_SetKeyR,
	RDP_SetConvert,		RDP_SetScissor,		RDP_SetPrimDepth,	RDP_SetOtherMode,
	/* 0x30 */
	RDP_LoadTLUT,		RDP_Unknown,		RDP_SetTileSize,	RDP_LoadBlock,
	RDP_LoadTile,		RDP_SetTile,		RDP_FillRect,		RDP_SetFillColor,
	RDP_SetFogColor,	RDP_SetBlendColor,	RDP_SetPrimColor,	RDP_SetEnvColor,
	RDP_SetCombine,		RDP_SetTImg,		RDP_SetZImg,		RDP_SetCImg
};

static u32 _RDP_ReadData( u32 address )
{
	if (*REG.DPC_STATUS & 0x1)          // XBUS_DMEM_DMA enabled
		return *(u32*)&DMEM[address & 0xFFC];
	return *(u32*)&RDRAM[(address & 0xFFFFFC) & (RDRAMSize - 1)];
}

void RDP_ProcessRDPList()
{
	const u32 dpCurrent = *REG.DPC_CURRENT;
	const u32 dpEnd     = *REG.DPC_END;

	if (dpEnd <= dpCurrent)
		return;

	const u32 length = dpEnd - dpCurrent;

	// load command data
	for (u32 i = 0; i < length; i += 4)
	{
		RDP_cmdData[RDP_cmdPtr] = _RDP_ReadData( dpCurrent + i );
		RDP_cmdPtr = (RDP_cmdPtr + 1) & RDP_maxCMDMask;
	}

	/* GLideN64 pone `RSP.cmd = cmd` antes de cada orden.  Aqui hace falta en el
	 * formato de la GBI (0xC0 | cmd): gDPTextureRectangle decide el volteo mirando
	 * `RSP.cmd == G_TEXRECTFLIP`, y sin esto veria la ultima orden de la display
	 * list.  Se devuelve al acabar. */
	const u32 rspCmdOrg = RSP.cmd;

	BOOL setZero = TRUE;
	while (RDP_cmdCur != RDP_cmdPtr)
	{
		const u32 cmd = (RDP_cmdData[RDP_cmdCur] >> 24) & 0x3F;

		if ((((RDP_cmdPtr - RDP_cmdCur) & RDP_maxCMDMask) * 4) < RDP_CmdLength[cmd])
		{
			setZero = FALSE;
			break;
		}

		if (RDP_cmdCur + RDP_CmdLength[cmd] / 4 > RDP_MAXCMD)
			memcpy( RDP_cmdData + RDP_MAXCMD, RDP_cmdData,
			        RDP_CmdLength[cmd] - (RDP_MAXCMD - RDP_cmdCur) * 4 );

		// execute the command
		RSP.cmd = 0xC0 | cmd;
		RDP_LLEcmd[cmd]( RDP_cmdData[RDP_cmdCur + 0], RDP_cmdData[RDP_cmdCur + 1] );

		RDP_cmdCur = (RDP_cmdCur + RDP_CmdLength[cmd] / 4) & RDP_maxCMDMask;
	}

	if (setZero)
	{
		RDP_cmdPtr = 0;
		RDP_cmdCur = 0;
	}

	RSP.cmd = rspCmdOrg;
	*REG.DPC_CURRENT = dpEnd;
}

