/**
 * glN64_GX - gDP.h
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifndef GDP_H
#define GDP_H

#include "Types.h"
#include "FrameBuffer.h"

#define CHANGED_RENDERMODE		0x001
#define CHANGED_CYCLETYPE		0x002
#define CHANGED_SCISSOR			0x004
#define CHANGED_TMEM			0x008
#define CHANGED_TILE			0x010
#define CHANGED_COMBINE_COLORS	0x020
#define CHANGED_COMBINE			0x040
#define CHANGED_ALPHACOMPARE	0x080
#define CHANGED_FOGCOLOR		0x100

#define TEXTUREMODE_NORMAL		0
#define TEXTUREMODE_TEXRECT		1
#define TEXTUREMODE_BGIMAGE		2
#define TEXTUREMODE_FRAMEBUFFER	3
#define TEXTUREMODE_FRAMEBUFFER_BG	4

#define LOADTYPE_BLOCK			0
#define LOADTYPE_TILE			1

struct gDPCombine
{
	union
	{
		struct
		{
#ifndef _BIG_ENDIAN
			// muxs1
			unsigned	aA1		: 3;
			unsigned	sbA1	: 3;
			unsigned	aRGB1	: 3;
			unsigned	aA0		: 3;
			unsigned	sbA0	: 3;
			unsigned	aRGB0	: 3;
			unsigned	mA1		: 3;
			unsigned	saA1	: 3;
			unsigned	sbRGB1	: 4;
			unsigned	sbRGB0	: 4;

			// muxs0
			unsigned	mRGB1	: 5;
			unsigned	saRGB1	: 4;
			unsigned	mA0		: 3;
			unsigned	saA0	: 3;
			unsigned	mRGB0	: 5;
			unsigned	saRGB0	: 4;
			unsigned	pad0	: 8;
#else // !_BIG_ENDIAN -> This should fix for BE.
			// muxs0
			unsigned	pad0	: 8;
			unsigned	saRGB0	: 4;
			unsigned	mRGB0	: 5;
			unsigned	saA0	: 3;
			unsigned	mA0		: 3;
			unsigned	saRGB1	: 4;
			unsigned	mRGB1	: 5;

			// muxs1
			unsigned	sbRGB0	: 4;
			unsigned	sbRGB1	: 4;
			unsigned	saA1	: 3;
			unsigned	mA1		: 3;
			unsigned	aRGB0	: 3;
			unsigned	sbA0	: 3;
			unsigned	aA0		: 3;
			unsigned	aRGB1	: 3;
			unsigned	sbA1	: 3;
			unsigned	aA1		: 3;
#endif // _BIG_ENDIAN
		};

		struct
		{
#ifndef _BIG_ENDIAN
			u32			muxs1, muxs0;
#else // !_BIG_ENDIAN -> This should fix for BE.
			u32			muxs0, muxs1;
#endif // _BIG_ENDIAN
		};

		u64				mux;
	};
};

struct gDPTile
{
	u32 format, size, line, tmem, palette;

	union
	{
		struct
		{
#ifndef _BIG_ENDIAN
			unsigned	mirrort	: 1;
			unsigned	clampt	: 1;
			unsigned	pad0	: 30;

			unsigned	mirrors	: 1;
			unsigned	clamps	: 1;
			unsigned	pad1	: 30;
#else // !_BIG_ENDIAN -> Big Endian fix.
			unsigned	pad1	: 30;
			unsigned	clamps	: 1;
			unsigned	mirrors	: 1;

			unsigned	pad0	: 30;
			unsigned	clampt	: 1;
			unsigned	mirrort	: 1;
#endif // _BIG_ENDIAN
		};

		struct
		{
#ifndef _BIG_ENDIAN
			u32 cmt, cms;
#else // !_BIG_ENDIAN -> Big Endian fix.
			u32 cms, cmt;
#endif // _BIG_ENDIAN
		};
	};

	u32 frameBufferAddress;	// resolved through FrameBuffer_GetBuffer()
	u32 maskt, masks;
	u32 originalMaskS, originalMaskT;
	u32 shiftt, shifts;
	f32 fuls, fult, flrs, flrt;
	u32 uls, ult, lrs, lrt;
	u32 loadWidth, loadHeight;

	// [Xbox360] GLideN64's per-tile gDPTile::textureMode / loadType / imageAddress.
	// glN64 kept one global gDP.textureMode, so a tile loaded from RDRAM and a tile
	// loaded from a frame buffer could not coexist (Mario Tennis). Only the frame
	// buffer case is tracked per tile; the other modes stay global.
	u32 textureMode;
	u32 loadType;
	u32 imageAddress;	// gDP.textureImage.address when this tile was loaded
};

// Tracks what a LoadTile actually copied into a given TMEM address
struct gDPLoadTileInfo
{
	u16 width, height;
	u16 texWidth;
	u32 texAddress;	// RDRAM address this load read from
	u32 loadType;
	u8  size;
};

struct gDPInfo
{
	struct
	{
		union
		{
			struct
			{
#ifndef _BIG_ENDIAN
				unsigned int alphaCompare : 2;
				unsigned int depthSource : 1;

//				struct
//				{
					unsigned int AAEnable : 1;
					unsigned int depthCompare : 1;
					unsigned int depthUpdate : 1;
					unsigned int imageRead : 1;
					unsigned int clearOnCvg : 1;

					unsigned int cvgDest : 2;
					unsigned int depthMode : 2;

					unsigned int cvgXAlpha : 1;
					unsigned int alphaCvgSel : 1;
					unsigned int forceBlender : 1;
					unsigned int textureEdge : 1;
//				} renderMode;

//				struct
//				{
					unsigned int c2_m2b : 2;
					unsigned int c1_m2b : 2;
					unsigned int c2_m2a : 2;
					unsigned int c1_m2a : 2;
					unsigned int c2_m1b : 2;
					unsigned int c1_m1b : 2;
					unsigned int c2_m1a : 2;
					unsigned int c1_m1a : 2;
//				} blender;

				unsigned int blendMask : 4;
				unsigned int alphaDither : 2;
				unsigned int colorDither : 2;
				
				unsigned int combineKey : 1;
				unsigned int textureConvert : 3;
				unsigned int textureFilter : 2;
				unsigned int textureLUT : 2;

				unsigned int textureLOD : 1;
				unsigned int textureDetail : 2;
				unsigned int texturePersp : 1;
				unsigned int cycleType : 2;
				unsigned int unusedColorDither : 1; // unsupported
				unsigned int pipelineMode : 1;

				unsigned int pad : 8;
#else // !_BIG_ENDIAN - Big Endian fix.
				unsigned int pad : 8;

				unsigned int pipelineMode : 1;
				unsigned int unusedColorDither : 1; // unsupported
				unsigned int cycleType : 2;
				unsigned int texturePersp : 1;
				unsigned int textureDetail : 2;
				unsigned int textureLOD : 1;

				unsigned int textureLUT : 2;
				unsigned int textureFilter : 2;
				unsigned int textureConvert : 3;
				unsigned int combineKey : 1;

				unsigned int colorDither : 2;
				unsigned int alphaDither : 2;
				unsigned int blendMask : 4;
				
//				struct
//				{
					unsigned int c1_m1a : 2;
					unsigned int c2_m1a : 2;
					unsigned int c1_m1b : 2;
					unsigned int c2_m1b : 2;
					unsigned int c1_m2a : 2;
					unsigned int c2_m2a : 2;
					unsigned int c1_m2b : 2;
					unsigned int c2_m2b : 2;
//				} blender;

//				struct
//				{
					unsigned int textureEdge : 1;
					unsigned int forceBlender : 1;
					unsigned int alphaCvgSel : 1;
					unsigned int cvgXAlpha : 1;

					unsigned int depthMode : 2;
					unsigned int cvgDest : 2;

					unsigned int clearOnCvg : 1;
					unsigned int imageRead : 1;
					unsigned int depthUpdate : 1;
					unsigned int depthCompare : 1;
					unsigned int AAEnable : 1;
//				} renderMode;

				unsigned int depthSource : 1;
				unsigned int alphaCompare : 2;
#endif // _BIG_ENDIAN
			};

			u64			_u64;

			struct
			{
#ifndef _BIG_ENDIAN
				u32			l, h;
#else // !_BIG_ENDIAN - Big Endian fix.
				u32			h, l;
#endif // _BIG_ENDIAN
			};
		};
	} otherMode;

	gDPCombine combine;

	gDPTile tiles[8], *loadTile;
	gDPLoadTileInfo loadInfo[512];

	struct
	{
		f32 r, g, b, a;
	} fogColor,  blendColor, envColor;

	struct
	{
		f32 r, g, b, a;
		f32 z, dz;
		u32 color;
	} fillColor;

	struct
	{
		u32 m;
		f32 l, r, g, b, a;
	} primColor;

	struct
	{
		f32 z, deltaZ;
	} primDepth;

	struct
	{
		u32 format, size, width, bpl;
		u32 address;
	} textureImage;

	struct
	{
		u32 format, size, width, height, bpl;
		u32 address, changed;
		u32 depthImage;
	} colorImage;

	u32	depthImageAddress;

	struct
	{
		u32 mode;
		f32 ulx, uly, lrx, lry;
		// The same bounds in raw s10.2, as the command delivered them.
		s16 xh, yh, xl, yl;
	} scissor;

	struct
	{
		s32 k0, k1, k2, k3, k4, k5;
	} convert;

	struct
	{
		struct
		{
			f32 r, g, b, a;
		} center, scale, width;
	} key;

	struct
	{
		u32 width, height;
		f32 dsdx, dtdy;
	} texRect;

	u32 changed;

	/* [Xbox360] ★★ CRC DE LA PALETA, PRECALCULADO AL CARGARLA.
	 *
	 * Portado de GLideN64 (src/gDP.h:266-267, src/gDP.cpp:758-762,
	 * src/Textures.cpp:1862-1867).  NO es invencion: `gDPLoadTLUT` de este
	 * arbol ya calcula e incrementa `pal` y NO LO USA PARA NADA -- es el
	 * residuo exacto de estas dos lineas, que glN64 no llego a tener.
	 *
	 * POR QUE.  `TextureCache_CalculateCRC` rehasheaba la paleta ENTERA
	 * (2.048 bytes para 8b/16b, 128 para 4b) en CADA consulta de textura.
	 * Medido en OoT: 7.648 consultas/s, 17,3 MB/s hasheados a 336 MB/s, o sea
	 * 0,84 ms de cada frame -- y la inmensa mayoria de esos bytes son LOS
	 * MISMOS 2 KB una y otra vez.
	 *
	 * ★ Y NO SE PODIA MEMOIZAR SIN MAS: `Hash_Calculate(crc, buf, len)` es
	 * `XXH32(buf, len, crc)`, o sea que el CRC que se lleva acumulado es la
	 * SEMILLA.  El hash de la paleta depende del de los texels y no se puede
	 * precalcular tal cual.  La solucion de la referencia -- y la unica que
	 * hay -- es hashear la paleta APARTE, con semilla fija, y meter ESE valor
	 * en la cadena.  Eso CAMBIA el valor del CRC, y es inocuo porque el CRC
	 * solo se compara con otros calculados igual y la cache se reconstruye en
	 * cada sesion.
	 *
	 * ★ SON DIECISEIS, NO UNO.  Para CI4 el CRC lleva SOLO la sub-paleta que
	 * toca (`palette << 4`).  Colapsarlas en un unico valor daria el mismo CRC
	 * a dos texturas con sub-paleta distinta = textura equivocada, que es
	 * justo el defecto de [[project_xenon64_papermario_siluetas]].
	 *
	 * ★ LIMITE CONOCIDO Y ACEPTADO, igual que en la referencia: esto se
	 * recalcula SOLO en `gDPLoadTLUT`.  La mitad alta de la TMEM no es una
	 * particion de hardware, y `gDPLoadTile`/`gDPLoadBlock` pueden escribir en
	 * ella (su guarda solo mira los 4 KB totales).  Si un juego hace eso, el
	 * CRC de la paleta se queda rancio.  GLideN64 acepta ese riesgo y no pone
	 * el gancho ahi; anadirlo "por simetria" seria
	 * [[feedback_no_anadir_lo_que_la_referencia_no_tiene]].  Queda escrito
	 * para que, si algun dia aparece una textura equivocada en un juego con
	 * paletas, este sea el primer sitio donde mirar. */
	u32 paletteCRC16[16];
	u32 paletteCRC256;

	u32 half_1, half_2;
	u32 textureMode;
	u32 loadType;

	bool m_subscreen;	// for GLideN64 hack_subscreen
	u32 m_fbCopyPending;
	u32 m_fbCopySource;
};

extern gDPInfo gDP;

// [Xbox360] [Mario Golf] Depth buffer whose saved Z the texrect being drawn copies
// into the depth image (0 = none). Set by gDPTextureRectangle around the draw and
// read by OGL_UpdateStates (render to depth, mode 3). See gDP.cpp.
extern u32 gln64_zCopiaDesde;

// [Xbox360] [Beetle Adventure Racing] TRUE while the texrect being drawn is
// GLideN64's texturedRectShadowMap (depth based fog). Set by gDPTextureRectangle
// around the draw and read by OGL_UpdateStates (its blend). See gDP.cpp.
extern BOOL gln64_nieblaZ;

extern u32 DepthClearColor;
void gDPSetDepthClearColor();

void gDPSetOtherMode( u32 mode0, u32 mode1 );
void gDPSetPrimDepth( u16 z, u16 dz );
void gDPPipelineMode( u32 mode );
void gDPSetCycleType( u32 type );
void gDPSetTexturePersp( u32 enable );
void gDPSetTextureDetail( u32 type );
void gDPSetTextureLOD( u32 mode );
void gDPSetTextureLUT( u32 mode );
void gDPSetTextureFilter( u32 type );
void gDPSetTextureConvert( u32 type );
void gDPSetCombineKey( u32 type );
void gDPSetColorDither( u32 type );
void gDPSetAlphaDither( u32 type );
void gDPSetAlphaCompare( u32 mode );
void gDPSetDepthSource( u32 source );
void gDPSetRenderMode( u32 mode1, u32 mode2 );
void gDPSetCombine( s32 muxs0, s32 muxs1 );
void gDPSetColorImage( u32 format, u32 size, u32 width, u32 address );
void gDPSetTextureImage( u32 format, u32 size, u32 width, u32 address );
void gDPSetDepthImage( u32 address );
void gDPSetEnvColor( u32 r, u32 g, u32 b, u32 a );
void gDPSetBlendColor( u32 r, u32 g, u32 b, u32 a );
void gDPSetFogColor( u32 r, u32 g, u32 b, u32 a );
void gDPSetFillColor( u32 c );
void gDPSetPrimColor( u32 m, u32 l, u32 r, u32 g, u32 b, u32 a );
void gDPSetTile( u32 format, u32 size, u32 line, u32 tmem, u32 tile, u32 palette, u32 cmt, u32 cms, u32 maskt, u32 masks, u32 shiftt, u32 shifts );
void gDPSetTileSize( u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt );
void gDPLoadTile( u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt );
void gDPLoadBlock( u32 tile, u32 uls, u32 ult, u32 lrs, u32 dxt );
void gDPLoadTLUT( u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt );
void gDPSetScissor( u32 mode, f32 ulx, f32 uly, f32 lrx, f32 lry );
void gDPBufferChanged( f32 maxY );
void gDPFillRectangle( s32 ulx, s32 uly, s32 lrx, s32 lry );
void gDPSetConvert( s32 k0, s32 k1, s32 k2, s32 k3, s32 k4, s32 k5 );
void gDPSetKeyR( u32 cR, u32 sR, u32 wR );
void gDPSetKeyGB(u32 cG, u32 sG, u32 wG, u32 cB, u32 sB, u32 wB );
void gDPTextureRectangle( f32 ulx, f32 uly, f32 lrx, f32 lry, s32 tile, f32 s, f32 t, f32 dsdx, f32 dtdy, const f32 *colorOverride = NULL );
void gDPTextureRectangleFlip( f32 ulx, f32 uly, f32 lrx, f32 lry, s32 tile, f32 s, f32 t, f32 dsdx, f32 dtdy );
// [Xbox360] GLideN64's LLETriangle::draw(): RDP edge-walker triangles (0x08-0x0F)
// coming through RDP_ProcessRDPList.  _pData is the 44-word command, zero padded.
void gDPLLETriangle( BOOL _shade, BOOL _texture, BOOL _zbuffer, const u32 *_pData );
void gDPFullSync();
void gDPTileSync();
void gDPPipeSync();
void gDPLoadSync();
void gDPNoOp();

#endif

