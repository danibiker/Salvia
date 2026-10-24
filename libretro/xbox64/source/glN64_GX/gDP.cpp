/**
 * glN64_GX - gDP.cpp
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
#include <string.h>
#include <stdio.h>
#include "../gui/DEBUG.h"
extern "C" {
#include "../main/gamehacks.h"
}
#endif // __GX__

#include "glN64.h"
#include "N64.h"
#include "GBI.h"
#include "RSP.h"
#include "gDP.h"
#include "Config.h"
#include "gSP.h"
#include "Types.h"
#include "Debug.h"
#include "../xenon64_diag.h"   /* XBOX64_TRAZAS */
#include "OpenGL.h"
#include "CRC.h"
#include "FrameBuffer.h"
#include "DepthBuffer.h"
#include "DepthCopy.h"
#include "VI.h"
#ifdef __GX__
#include "Textures.h"
#endif // __GX__

#include <math.h>

#ifdef __LINUX__
#include <stdlib.h>
#endif

gDPInfo gDP;
/* [Xbox360] ★ `extern "C"` ANYADIDO, Y NO ES COSMETICO.
 *
 * Quien PONE esta bandera es `main/rom_gc.c:183`, por CRC de la ROM (el
 * arreglo del cartel de Mario Kart 64).  `rom_gc.c` es C y define el
 * simbolo PELADO; este fichero es C++ y sin `extern "C"` definia el
 * simbolo DECORADO.  O sea DOS VARIABLES con el mismo nombre: rom_gc.c
 * escribia en una y las lineas 448 y 453 de aqui leian la otra, que se
 * queda a 0 para siempre.
 *
 * El fallo no da ni un aviso del enlazador -- son simbolos distintos, no
 * duplicados -- y el sintoma seria "el arreglo por CRC no hace nada".
 *
 * Lo declara `main/wii64config.h:198`, que tampoco lleva extern "C"; se
 * arregla en la definicion para no tocar una cabecera que ve medio
 * arbol. */
extern "C" char enablegDPUpdateColorImage = 0;

void gDPSetOtherMode( u32 mode0, u32 mode1 )
{
	gDP.otherMode.h = mode0;
	gDP.otherMode.l = mode1;
	gSPUpdateTextureTiles();
	gSP.changed |= CHANGED_TEXTURE;


	gDP.changed |= CHANGED_RENDERMODE | CHANGED_CYCLETYPE | CHANGED_ALPHACOMPARE;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetOtherMode( %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s, %s | %s | %s%s%s%s%s | %s | %s%s%s );\n",
		AlphaDitherText[gDP.otherMode.alphaDither],
		ColorDitherText[gDP.otherMode.colorDither],
		CombineKeyText[gDP.otherMode.combineKey],
		TextureConvertText[gDP.otherMode.textureConvert],
		TextureFilterText[gDP.otherMode.textureFilter],
		TextureLUTText[gDP.otherMode.textureLUT],
		TextureLODText[gDP.otherMode.textureLOD],
		TextureDetailText[gDP.otherMode.textureDetail],
		TexturePerspText[gDP.otherMode.texturePersp],
		CycleTypeText[gDP.otherMode.cycleType],
		PipelineModeText[gDP.otherMode.pipelineMode],
		AlphaCompareText[gDP.otherMode.alphaCompare],
		DepthSourceText[gDP.otherMode.depthSource],
		gDP.otherMode.AAEnable ? "AA_EN | " : "",
		gDP.otherMode.depthCompare ? "Z_CMP | " : "",
		gDP.otherMode.depthUpdate ? "Z_UPD | " : "",
		gDP.otherMode.imageRead ? "IM_RD | " : "",
		CvgDestText[gDP.otherMode.cvgDest],
		DepthModeText[gDP.otherMode.depthMode],
		gDP.otherMode.cvgXAlpha ? "CVG_X_ALPHA | " : "",
		gDP.otherMode.alphaCvgSel ? "ALPHA_CVG_SEL | " : "",
		gDP.otherMode.forceBlender ? "FORCE_BL" : "" );
#endif
}

void gDPSetPrimDepth( u16 z, u16 dz )
{
	gDP.primDepth.z = _FIXED2FLOAT( _SHIFTR( z, 0, 15 ), 15 );
	gDP.primDepth.deltaZ = _FIXED2FLOAT( _SHIFTR( dz, 0, 15 ), 15 );

#ifdef __GX__
	gDP.changed |= CHANGED_RENDERMODE;

#ifdef SHOW_DEBUG
	sprintf(txtbuffer,"gDP: Setting PrimDepth to %f", gDP.primDepth.z);
	DEBUG_print(txtbuffer,DBG_RSPINFO1);
#endif
#endif //__GX__

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetPrimDepth( %f, %f );\n",
		gDP.primDepth.z,
		gDP.primDepth.deltaZ);
#endif
}

void gDPPipelineMode( u32 mode )
{
	gDP.otherMode.pipelineMode = mode;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPPipelineMode( %s );\n",
		PipelineModeText[gDP.otherMode.pipelineMode] );
#endif
}

void gDPSetCycleType( u32 type )
{
	gDP.otherMode.cycleType = type;

	gDP.changed |= CHANGED_CYCLETYPE;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetCycleType( %s );\n",
		CycleTypeText[gDP.otherMode.cycleType] );
#endif
}

void gDPSetTexturePersp( u32 enable )
{
	gDP.otherMode.texturePersp = enable;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPSetTexturePersp( %s );\n",
		TexturePerspText[gDP.otherMode.texturePersp] );
#endif
}

void gDPSetTextureDetail( u32 type )
{
	gDP.otherMode.textureDetail = type;
	gSPUpdateTextureTiles();
	gSP.changed |= CHANGED_TEXTURE;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPSetTextureDetail( %s );\n",
		TextureDetailText[gDP.otherMode.textureDetail] );
#endif
}

void gDPSetTextureLOD( u32 mode )
{
	gDP.otherMode.textureLOD = mode;
	gSPUpdateTextureTiles();
	gSP.changed |= CHANGED_TEXTURE;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPSetTextureLOD( %s );\n",
		TextureLODText[gDP.otherMode.textureLOD] );
#endif
}

void gDPSetTextureLUT( u32 mode )
{
	gDP.otherMode.textureLUT = mode;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPSetTextureLUT( %s );\n",
		TextureLUTText[gDP.otherMode.textureLUT] );
#endif
}

void gDPSetTextureFilter( u32 type )
{
	gDP.otherMode.textureFilter = type;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPSetTextureFilter( %s );\n",
		TextureFilterText[gDP.otherMode.textureFilter] );
#endif
}

void gDPSetTextureConvert( u32 type )
{
	gDP.otherMode.textureConvert = type;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPSetTextureConvert( %s );\n",
		TextureConvertText[gDP.otherMode.textureConvert] );
#endif
}

void gDPSetCombineKey( u32 type )
{
	gDP.otherMode.combineKey = type;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_COMBINE, "gDPSetCombineKey( %s );\n",
		CombineKeyText[gDP.otherMode.combineKey] );
#endif
}

void gDPSetColorDither( u32 type )
{
	gDP.otherMode.colorDither = type;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetColorDither( %s );\n",
		ColorDitherText[gDP.otherMode.colorDither] );
#endif
}

void gDPSetAlphaDither( u32 type )
{
	gDP.otherMode.alphaDither = type;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetAlphaDither( %s );\n",
		AlphaDitherText[gDP.otherMode.alphaDither] );
#endif
}

void gDPSetAlphaCompare( u32 mode )
{
	gDP.otherMode.alphaCompare = mode;

	gDP.changed |= CHANGED_ALPHACOMPARE;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetAlphaCompare( %s );\n",
		AlphaCompareText[gDP.otherMode.alphaCompare] );
#endif
}

void gDPSetDepthSource( u32 source )
{
	gDP.otherMode.depthSource = source;

#ifdef __GX__
	gDP.changed |= CHANGED_RENDERMODE;
#endif // __GX__

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetDepthSource( %s );\n",
		DepthSourceText[gDP.otherMode.depthSource] );
#endif
}

void gDPSetRenderMode( u32 mode1, u32 mode2 )
{
	gDP.otherMode.l &= 0x00000007;
	gDP.otherMode.l |= mode1 | mode2;

	gDP.changed |= CHANGED_RENDERMODE;

#ifdef DEBUG
	// THIS IS INCOMPLETE!!!
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetRenderMode( %s%s%s%s%s | %s | %s%s%s );\n",
		gDP.otherMode.AAEnable ? "AA_EN | " : "",
		gDP.otherMode.depthCompare ? "Z_CMP | " : "",
		gDP.otherMode.depthUpdate ? "Z_UPD | " : "",
		gDP.otherMode.imageRead ? "IM_RD | " : "",
		CvgDestText[gDP.otherMode.cvgDest],
		DepthModeText[gDP.otherMode.depthMode],
		gDP.otherMode.cvgXAlpha ? "CVG_X_ALPHA | " : "",
		gDP.otherMode.alphaCvgSel ? "ALPHA_CVG_SEL | " : "",
		gDP.otherMode.forceBlender ? "FORCE_BL" : "" );
#endif
}

void gDPSetCombine( s32 muxs0, s32 muxs1 )
{
	gDP.combine.muxs0 = muxs0;
	gDP.combine.muxs1 = muxs1;

	gDP.changed |= CHANGED_COMBINE;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_COMBINE, "gDPSetCombine( %s, %s, %s, %s, %s, %s, %s, %s,\n",
		saRGBText[gDP.combine.saRGB0],
		sbRGBText[gDP.combine.sbRGB0],
		mRGBText[gDP.combine.mRGB0],
		aRGBText[gDP.combine.aRGB0],
		saAText[gDP.combine.saA0],
		sbAText[gDP.combine.sbA0],
		mAText[gDP.combine.mA0],
		aAText[gDP.combine.aA0] );

	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_COMBINE, "               %s, %s, %s, %s, %s, %s, %s, %s );\n",
		saRGBText[gDP.combine.saRGB1],
		sbRGBText[gDP.combine.sbRGB1],
		mRGBText[gDP.combine.mRGB1],
		aRGBText[gDP.combine.aRGB1],
		saAText[gDP.combine.saA1],
		sbAText[gDP.combine.sbA1],
		mAText[gDP.combine.mA1],
		aAText[gDP.combine.aA1] );

#endif
}

/*void RSP_UpdateColorImage()
{
	WORD *colorBuffer = (WORD*)&RDRAM[RDP.colorImage.address];
	BYTE *frameBuffer = (BYTE*)malloc( OGL.width * OGL.height * 3 );
	BYTE *framePixel;

	int x, y, frameX, frameY, i;

	glReadBuffer( GL_BACK );
	glReadPixels( 0, 0, OGL.width - 1, OGL.height - 1, GL_RGB, GL_UNSIGNED_BYTE, frameBuffer );
	
	i = 0;
	for (y = 0; y < RDP.height; y++)
	{
		frameY = OGL.height - (y * OGL.scaleY);
		for (x = 0; x < RDP.width; x++)
		{
			frameX = x * OGL.scaleX;
			framePixel = &frameBuffer[(OGL.width * frameY + frameX) * 3];
			colorBuffer[i^1] =	((framePixel[0] >> 3) << 11) |
								((framePixel[1] >> 3) <<  6) |
								((framePixel[2] >> 3) <<  1);
			i++;
		}
	}
	free( frameBuffer );
}*/

static inline u16 YUYV_to_RGBA5551(u8 y, u8 u, u8 v)
{
    int C = y - 16;
    int D = u - 128;
    int E = v - 128;

    int R = (298 * C + 409 * E + 128) >> 8;
    int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
    int B = (298 * C + 516 * D + 128) >> 8;

    if (R < 0) R = 0; else if (R > 255) R = 255;
    if (G < 0) G = 0; else if (G > 255) G = 255;
    if (B < 0) B = 0; else if (B > 255) B = 255;

    u16 r5 = R >> 3;
    u16 g5 = G >> 3;
    u16 b5 = B >> 3;

    return (r5 << 11) | (g5 << 6) | (b5 << 1) | 1;
}

extern VIInfo VI;
void gDPUpdateColorImage(void)
{

	// This is hacky (uses xfb, makes many assumptions, no depth buffer) so lets only enable it for Mario Kart 64.
    const u32 w = gDP.colorImage.width  * OGL.scaleX;
    //const u32 h = gDP.colorImage.height * OGL.scaleY;

    u16 *xfb = (u16*)VI.xfb[VI.which_fb];

#if defined(_XBOX)
	/* [Xbox360] ★ ESTA FUNCION NO SE PUEDE EJECUTAR AQUI, Y ESTO LO DICE
	 * EN VEZ DE REVENTAR.
	 *
	 * Lee `VI.xfb[]`, el framebuffer EXTERNO de la Wii en YUYV, que
	 * rellena `VI_GX_setFB` <- `gfx_set_fb` <- el menu de Wii64.  Ese
	 * camino no esta portado, asi que los dos punteros son NULL.
	 *
	 * Hoy no deberia llegarse: `enablegDPUpdateColorImage` se queda a 0
	 * porque `rom_gc.c` volvio a la condicion de upstream.  La guarda
	 * esta para el dia en que alguien lo reencienda -- entonces sale UNA
	 * linea que nombra el problema, en vez de un
	 * `Access violation reading location 0x00000000` en mitad de una
	 * lista de display, que es como se presento la primera vez y no
	 * apunta a este fichero ni de lejos.
	 *
	 * UNA SOLA VEZ: si se reactiva, se llama por cada cambio de color
	 * image, o sea varias veces por frame. */
	if (!xfb)
	{
#if XBOX64_TRAZAS
		static int dicho = 0;
		if (!dicho)
		{
			dicho = 1;
			DEBUG_print("[GLN64] gDPUpdateColorImage SIN XFB: es el framebuffer "
			            "externo de la Wii y aqui no existe.  Sin efecto.",
			            DBG_USBGECKO);
		}
#endif
		return;
	}
#endif
    u16 *dst = (u16*)&RDRAM[gDP.colorImage.address];

    u32 i = 0;

    for (u32 y = 0; y < gDP.colorImage.height; y++)
    {
        u32 frameY = OGL.GXheight < 480 ? y : (y * OGL.scaleY);

        for (u32 x = 0; x < gDP.colorImage.width; x++)
        {
           u32 frameX = (u32)(x * OGL.scaleX);

			u32 px = frameY * w + frameX;
			u32 yuyv = ((u32*)xfb)[px >> 1];


			u8 y0 = (yuyv >> 24) & 0xFF;
			u8 u  = (yuyv >> 16) & 0xFF;
			u8 y1 = (yuyv >>  8) & 0xFF;
			u8 v  = (yuyv >>  0) & 0xFF;
			u8 yval = (px & 1) ? y1 : y0;

			u16 out = YUYV_to_RGBA5551(yval, u, v);

			dst[i ^ 1] = out;
			i++;
        }
    }
}

#if 0
void gDPUpdateColorImage()
{
	return;
	if ((gDP.colorImage.size == G_IM_SIZ_16b) && (gDP.colorImage.format == G_IM_FMT_RGBA))
	{
		u16 *frameBuffer = (u16*)malloc( (unsigned int)(gDP.colorImage.width * OGL.scaleX * gDP.colorImage.height * OGL.scaleY * 2) );
		u16 *colorImage = (u16*)&RDRAM[gDP.colorImage.address];
		u32 frameX, frameY;
		u32 i = 0;

		glReadBuffer( GL_BACK );
		glReadPixels( 0, (int)(OGL.height - gDP.colorImage.height * OGL.scaleY + OGL.heightOffset), (int)(gDP.colorImage.width * OGL.scaleX), (int)(gDP.colorImage.height * OGL.scaleY), GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1_EXT, frameBuffer );

		for (u32 y = 0; y < gDP.colorImage.height; y++)
		{
			frameY = (unsigned long)((gDP.colorImage.height - 1) * OGL.scaleY - y * OGL.scaleY);
			for (u32 x = 0; x < gDP.colorImage.width; x++)
			{
				frameX = (unsigned long)(x * OGL.scaleX);
				colorImage[i^1] = frameBuffer[(u32)(gDP.colorImage.width * OGL.scaleX) * frameY + frameX];

				i++;
			}
		}

		free( frameBuffer );
	}
	else if ((gDP.colorImage.size == G_IM_SIZ_8b) && (gDP.colorImage.format == G_IM_FMT_I))
	{
		u8 *frameBuffer = (u8*)malloc( (unsigned int)(gDP.colorImage.width * OGL.scaleX * gDP.colorImage.height * OGL.scaleY) );
		u8 *colorImage = (u8*)&RDRAM[gDP.colorImage.address];
		u32 frameX, frameY;
		u32 i = 0;

		glReadPixels( 0, (int)(OGL.height - gDP.colorImage.height * OGL.scaleY + OGL.heightOffset), (int)(gDP.colorImage.width * OGL.scaleX), (int)(gDP.colorImage.height * OGL.scaleY), GL_LUMINANCE, GL_UNSIGNED_BYTE, frameBuffer );

		for (u32 y = 0; y < gDP.colorImage.height; y++)
		{
			frameY = (unsigned long)((gDP.colorImage.height - 1) * OGL.scaleY - y * OGL.scaleY);
			for (u32 x = 0; x < gDP.colorImage.width; x++)
			{
				frameX = (unsigned long)(x * OGL.scaleX);
				colorImage[i^3] = frameBuffer[(u32)(gDP.colorImage.width * OGL.scaleX) * frameY + frameX];

				i++;
			}
		}

		free( frameBuffer );
	}
}
#endif

static void _gDPWriteRDRAM( u32 dst, const u8 *src, u32 fill, u32 numBytes );

/* [Xbox360] ★ [Quake II: fondo AZUL en la intro y en el menu] EL COLOR DE
 * RELLENO SE INTERPRETA AL USARLO, segun el tamano de la color image, como
 * GLideN64 (getFillColor, gDP.cpp:268-281): RGBA5551 con 16 bits o menos,
 * RGBA8888 con 32.  glN64 lo convertia SIEMPRE como 5551 en gDPSetFillColor.
 * MEDIDO (log 38, [FILL-GX]/[CIMG-GX]): Quake II dibuja en color images de 32
 * bits (size=3) y las borra con fill=000000FF, negro opaco en 8888; como 5551
 * la mitad baja 00FF es R=0 G=3 B=31: azul.  Con 16 bits devuelve lo mismo que
 * antes (las componentes que guarda gDPSetFillColor). */
static f32 *_gDPFillColorActual( void )
{
	static f32 s_c[4];
	if (gDP.colorImage.size < G_IM_SIZ_32b)
		return &gDP.fillColor.r;
	s_c[0] = (f32)_SHIFTR( gDP.fillColor.color, 24, 8 ) * (1.0f / 255.0f);
	s_c[1] = (f32)_SHIFTR( gDP.fillColor.color, 16, 8 ) * (1.0f / 255.0f);
	s_c[2] = (f32)_SHIFTR( gDP.fillColor.color,  8, 8 ) * (1.0f / 255.0f);
	s_c[3] = (f32)_SHIFTR( gDP.fillColor.color,  0, 8 ) * (1.0f / 255.0f);
	return s_c;
}

void gDPSetColorImage( u32 format, u32 size, u32 width, u32 address )
{
	/* [Xbox360] [Resident Evil 2] GLideN64 5e51b386, FrameBufferList::saveBuffer:
	 *     if (_width == 512 && (config.generalEmulation.hacks & hack_RE2) != 0)
	 *         _width = *REG.VI_WIDTH;
	 * El juego declara sus buffers con 512 de ancho y los usa con el del VI (ver
	 * _gSPFondoRE2 en gSP.cpp).  Alli es el ancho del buffer; aqui el buffer es
	 * gDP.colorImage (FrameBuffer_SaveBuffer/RestoreBuffer, la escala del EFB de
	 * gln64_cimg_escala), asi que se cambia a la entrada. */
	if ((config.generalEmulation.hacks & hack_RE2) != 0 && width == 512 && *REG.VI_WIDTH != 0)
		width = *REG.VI_WIDTH;

#ifdef __GX__
	/* [Xbox360] ★ [Pilotwings] Fuera del dibujo sobre el depth image
	 * (GXD3D_SetRenderToDepth, ver el final de OGL_UpdateStates) ANTES de
	 * guardar o reponer nada: FrameBuffer_SaveBuffer/RestoreBuffer dibujan
	 * sin pasar por OGL_UpdateStates y heredarian el modo -- reponer el buffer
	 * principal escribiria su color como profundidad en toda la pantalla --.
	 * Y la copia de Z deja de valer, como en GLideN64 al enganchar otro
	 * buffer (DepthBuffer::setDepthAttachment, `m_copied = false`). */
	if (gDP.colorImage.address != RSP_SegmentToPhysical( address ))
	{
		GXD3D_SetRenderToDepth( 0 );
		GXD3D_SetDecalN64( 0 );   /* [ISS64] idem: el decal por shader */
		GXD3D_InvalidaCopiaZ();
	}
#endif // __GX__

	if (enablegDPUpdateColorImage &&
		gDP.colorImage.changed &&
		(gDP.colorImage.address != gDP.depthImageAddress) &&
		(gDP.colorImage.address != RSP_SegmentToPhysical( address )))
	{
		if(enablegDPUpdateColorImage) {
			gDPUpdateColorImage();
		}
		//OGL_ClearDepthBuffer();
		gDP.colorImage.changed = FALSE;
		gDP.colorImage.height = 1;
	}
/*
	for (u32 i = 0; i < (gDP.colorImage.width * gDP.colorImage.height ) << gDP.colorImage.size >> 1; i++)
	{
		sprintf(txtbuffer, "gDP: RDRAM clear %i", (gDP.colorImage.width * gDP.colorImage.height ) << gDP.colorImage.size >> 1);
		DEBUG_print(txtbuffer, DBG_CCINFO);
		RDRAM[gDP.colorImage.address + i] = 0;
	}
*/
	address = RSP_SegmentToPhysical( address );

	/* [Xbox360] sonda temporal [CIMG-GX]; ver xenon/gln64_prof.h. */
#if XBOX64_TRAZAS
	gln64_cimg_nota( address, width, size );
#endif

	if (gDP.colorImage.address != address)
	{
		if (gDP.m_fbCopyPending != 0 && gDP.m_fbCopyPending == gDP.colorImage.address)
		{
			if (gDP.colorImage.size == G_IM_SIZ_16b && gDP.colorImage.width == VI.width)
				FrameBuffer_CopyToRDRAM( gDP.m_fbCopySource, gDP.colorImage.address,
				                         gDP.colorImage.width, VI.height );

			gDP.m_fbCopyPending = 0;
			gDP.m_fbCopySource = 0;
		}

		if (OGL.frameBufferTextures)
		{
			if (gDP.colorImage.changed)
				FrameBuffer_SaveBuffer( gDP.colorImage.address, gDP.colorImage.size, gDP.colorImage.width, gDP.colorImage.height );

			if (address != gDP.depthImageAddress)
				FrameBuffer_RestoreBuffer( address, size, width );

			//OGL_ClearDepthBuffer();
		}

		gDP.colorImage.changed = FALSE;

		if (width == VI.width)
			gDP.colorImage.height = VI.height;
 		else
			gDP.colorImage.height = 1;
	}
	/* [Xbox360] ★ [Mario Tennis, sombra de los jugadores] MISMA DIRECCION Y
	 * ANCHO, TEXEL MAS PEQUENO: se borra el buffer entero con el fill color,
	 * como GLideN64 (FrameBuffer.cpp:870-891, rama `m_size != _size` de
	 * saveBuffer).  El juego pone el aux 000D4A80 en 16 bits, lo rellena de
	 * 0,0-64,32 (4096 bytes = el aux ENTERO de 64x64 en 8 bits) y lo vuelve a
	 * poner en 8 bits para pintar la silueta.  En el EFB ese relleno solo
	 * cubria las 32 primeras filas y las otras 32 se quedaban con lo que
	 * hubiera: el rectangulo a los pies de la sombra.  8 -> 16 GLideN64 lo
	 * recrea; eso no se toca. */
	else if (OGL.frameBufferTextures &&
	         address != gDP.depthImageAddress &&
	         width == gDP.colorImage.width &&
	         size < gDP.colorImage.size)
	{
		OGL_ClearColorBuffer( _gDPFillColorActual() );
	}

	if ((config.generalEmulation.hacks & hack_fbCopyToRDRAM) != 0 &&
	    format == G_IM_FMT_I && size == G_IM_SIZ_8b && width == VI.width)
	{
		const u32 address32 = RSP_SegmentToPhysical( address );
		const u32 numBytes = width * VI.height;

		if (numBytes != 0 && (address32 + numBytes) <= RDRAMSize)
		{
			_gDPWriteRDRAM( address32, NULL, 0xFFFFFFFF, numBytes );
			FrameBuffer_RestampMarkers( address32, address32 + numBytes - 1 );
		}
	}

	if ((config.generalEmulation.hacks & hack_subscreen) != 0 &&
	    format == G_IM_FMT_I && size == G_IM_SIZ_8b)
		gDP.m_subscreen = (gDP.otherMode._u64 == 0x00000cf00f0a0004ULL);

	gDP.colorImage.format = format;
	gDP.colorImage.size = size;
	gDP.colorImage.width = width;
	gDP.colorImage.address = RSP_SegmentToPhysical( address );

	/* [Xbox360] La escala del color image que entra, para que quepa en el
	 * EFB.  NO es una sonda; ver gln64_cimg_escala en gln64_config.cpp.
	 *
	 * ★ EL SITIO IMPORTA, Y HA COSTADO DOS INTENTOS:
	 *
	 *   - ANTES del SaveBuffer, NO: `FrameBuffer_SaveBuffer` guarda el buffer
	 *     que SALE y dimensiona su textura con la escala vigente, asi que
	 *     cada buffer se guardaba con la escala del SIGUIENTE.  En el log
	 *     salia desfasado una posicion -- un panel de 128 con esc=1.190, que
	 *     era la del buffer de 538 que venia detras.
	 *
	 *   - Entre el SaveBuffer y estas asignaciones, TAMPOCO: `gDP.colorImage`
	 *     todavia describe el buffer VIEJO, y la escala se calcula junto con
	 *     `gln64_rect_alto()`, que lo lee.  Los dos tienen que ver el mismo
	 *     color image o el ancho y el alto salen de buffers distintos.
	 *
	 * Aqui abajo las dos cosas son ciertas. */
	gln64_cimg_escala( width );

#ifdef __GX__
	/* [Xbox360] [Mario Tennis, marco del medidor] Si FrameBuffer_RestoreBuffer
	 * lo encontro NUEVO, se carga su RDRAM (GLideN64 8b359761).  Aqui y no alli:
	 * hace falta la escala del buffer que entra, que se acaba de fijar.
	 * [Paper Mario] Con hack_texrectPaletteMod se APLAZA al primer dibujo: ver
	 * FrameBuffer_GX_CargaPendiente (FrameBuffer.cpp). */
	if ((config.generalEmulation.hacks & hack_texrectPaletteMod) == 0)
		FrameBuffer_GX_CargaNuevoDesdeRDRAM();
#endif // __GX__

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetColorImage( %s, %s, %i, 0x%08X );\n",
		ImageFormatText[gDP.colorImage.format],
		ImageSizeText[gDP.colorImage.size],
		gDP.colorImage.width,
		gDP.colorImage.address );
#endif
}

void gDPSetTextureImage( u32 format, u32 size, u32 width, u32 address )
{
	gDP.textureImage.format = format;
	gDP.textureImage.size = size;
	gDP.textureImage.width = width;
	gDP.textureImage.address = RSP_SegmentToPhysical( address );
	gDP.textureImage.bpl = gDP.textureImage.width << gDP.textureImage.size >> 1;

	/* [Xbox360] ★ [Diddy Kong Racing, poligonos sin textura] GLideN64
	 * 268278a6 ("Implement load texture by dma offset"), tal cual salvo el
	 * indice: alli `t[tex_count ^ 1]` deshace el intercambio de una RDRAM
	 * little-endian; aqui la RDRAM ya esta en el orden del N64.  Ver
	 * gSPSetDMATexOffset. */
	if (gSP.DMAOffsets.tex_offset != 0)
	{
		if (format == G_IM_FMT_RGBA)
		{
			const u16 *tabla = (const u16*)&RDRAM[gSP.DMAOffsets.tex_offset];
			gSP.DMAOffsets.tex_shift = tabla[gSP.DMAOffsets.tex_count];
			gDP.textureImage.address += gSP.DMAOffsets.tex_shift;
		}
		else
		{
			gSP.DMAOffsets.tex_offset = 0;
			gSP.DMAOffsets.tex_shift = 0;
			gSP.DMAOffsets.tex_count = 0;
		}
	}

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPSetTextureImage( %s, %s, %i, 0x%08X );\n",
		ImageFormatText[gDP.textureImage.format],
		ImageSizeText[gDP.textureImage.size],
		gDP.textureImage.width,
		gDP.textureImage.address );
#endif
}

/* [Xbox360] ★★ [Mario Golf, el lago encima del cesped] LA COPIA DE UN Z
 * GUARDADO AL Z CON TEXRECTS, como GLideN64 7dca7f8f ("Implement depth buffer
 * copy for Mario Golf", issue 312).
 *
 * Con la camara quieta el juego no vuelve a dibujar el campo: lo usa de fondo
 * 2D y COPIA al Z principal el Z de cuando se dibujo, que tiene en otro depth
 * buffer, con texrects sobre el depth image.  GLideN64 sirve en esos texrects
 * la textura de PROFUNDIDAD del buffer de origen (FrameBuffer::getTexture,
 * `getDepthTexture = m_isDepthBuffer && ci == zi`) y, con el render to depth,
 * fragDepth = esa profundidad.  Sin esto el Z queda borrado y el agua, que se
 * pinta encima con Z_CMP, tapa el cesped (el defecto del articulo).
 *
 * Medido con [MGOLF] (log 5): `zi 0400 -> 002EA000` (la escena se dibuja con
 * el Z en 002EA000), `002EA000 -> 0400`, y 38 texrects COPY de 304x6 sobre
 * ci = zi = 0400 que leen 002EA000 + 0xF00 por tira (6 filas de 320) con
 * `st` = su propia posicion: una copia pixel a pixel.
 *
 * Aqui hay UN solo Z (el de la EDRAM), asi que:
 *   - al pasar el depth image a OTRA direccion, si se escribio Z mientras era
 *     el actual, su Z se GUARDA (GXD3D_GuardaZ, 4 huecos por LRU);
 *   - el texrect de la copia (sobre el depth image, 16 bits, lee de OTRO depth
 *     buffer y cada texel cae en su mismo pixel) se dibuja en el modo 3 del
 *     render to depth: profundidad = el Z guardado, sin color.
 * Un texrect que no cumple todo eso se dibuja como siempre.
 *
 * ★ Antes solo se guardaba el Z de un depth buffer ya APRENDIDO como origen
 * en una copia anterior.  Al empezar el hoyo con la camara quieta el juego
 * pinta el Z del temporal UNA vez, antes de la primera copia: no se guardaba
 * y el lago salia encima del cesped hasta mover la camara (lo que vio el
 * usuario).  GLideN64 no aprende nada: tiene siempre el Z de cada buffer.
 * Coste: un Resolve por cambio de depth image con Z escrito; un juego con un
 * solo depth buffer no paga nada (`zg guarda=` de [RTZ]). */
u32 gln64_zCopiaDesde = 0;
static u32 s_zEscriturasAlEntrar = 0;

static u32 _gDPCopiaDeZGuardada( f32 ulx, f32 uly, f32 s, f32 t, f32 dsdx, f32 dtdy )
{
	const gDPTile *t0 = gSP.textureTile[0];
	const u32 texDir = t0->imageAddress;
	const u32 ancho = gDP.colorImage.width;
	DepthBuffer *d;
	u32 off;

	if (t0->size != G_IM_SIZ_16b || ancho == 0 || dsdx != 1.0f || dtdy != 1.0f)
		return 0;

	// El depth buffer de ORIGEN: otro que el actual, y que contenga la textura.
	for (d = depthBuffer.top; d != NULL; d = d->lower)
		if (d->address != gDP.depthImageAddress && texDir >= d->address &&
		    texDir - d->address < ancho * 2 * 480)
			break;
	if (d == NULL)
		return 0;

	// Pixel a pixel: el texel (s,t) del origen es el pixel (ulx,uly) del destino.
	off = (texDir - d->address) >> 1;
	if (fabsf( s + (f32)(off % ancho) - ulx ) > 0.5f ||
	    fabsf( t + (f32)(off / ancho) - uly ) > 0.5f)
		return 0;

	return GXD3D_UsaZGuardada( d->address ) ? d->address : 0;
}

void gDPSetDepthImage( u32 address )
{
	//if (address != gDP.depthImageAddress)
	//	OGL_ClearDepthBuffer();

	const u32 depthAddress = RSP_SegmentToPhysical( address );

	DepthBuffer_SetBuffer( depthAddress );

	if (depthBuffer.current->cleared)
		OGL_ClearDepthBuffer();
	else if ((config.generalEmulation.hacks & hack_clearAloneDepthBuffer) != 0 &&
	         FrameBuffer_FindBuffer( depthAddress ) == NULL)
	{
		OGL_ClearDepthBuffer();
	}

	/* [Xbox360] [Mario Golf] El Z del depth image que se deja, si se escribio
	 * Z mientras era el actual.  Ver _gDPCopiaDeZGuardada. */
	if (depthAddress != gDP.depthImageAddress)
	{
		if (gDP.depthImageAddress != 0 &&
		    GXD3D_ZEscrituras() != s_zEscriturasAlEntrar)
			GXD3D_GuardaZ( gDP.depthImageAddress );
		s_zEscriturasAlEntrar = GXD3D_ZEscrituras();
	}

	gDP.depthImageAddress = depthAddress;

	/* [Xbox360] Historial de direcciones de depth image, para poder hacer la
	 * pregunta de Rice `IsUsedAsDI()`.  Ver gln64_ci_es_zi en
	 * xenon/gln64_config.cpp.  No cambia nada: solo anota. */
#if XBOX64_SONDAS   /* solo lo leen las sondas de OGL_AddTriangle/OGL_DrawRect */
	gln64_zi_nota( depthAddress, RSP.DList );
#endif

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetDepthImage( 0x%08X );\n", gDP.depthImageAddress );
#endif
}

void gDPSetEnvColor( u32 r, u32 g, u32 b, u32 a )
{
	gDP.envColor.r = GXcastu8f32( r );
	gDP.envColor.g = GXcastu8f32( g );
	gDP.envColor.b = GXcastu8f32( b );
	gDP.envColor.a = GXcastu8f32( a );

	gDP.changed |= CHANGED_COMBINE_COLORS;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_COMBINE, "gDPSetEnvColor( %i, %i, %i, %i );\n",
		r, g, b, a );
#endif
}

void gDPSetBlendColor( u32 r, u32 g, u32 b, u32 a )
{
	gDP.blendColor.r = GXcastu8f32( r );
	gDP.blendColor.g = GXcastu8f32( g );
	gDP.blendColor.b = GXcastu8f32( b );
	gDP.blendColor.a = GXcastu8f32( a );

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetBlendColor( %i, %i, %i, %i );\n",
		r, g, b, a );
#endif
}

void gDPSetFogColor( u32 r, u32 g, u32 b, u32 a )
{
	gDP.fogColor.r = GXcastu8f32( r );
	gDP.fogColor.g = GXcastu8f32( g );
	gDP.fogColor.b = GXcastu8f32( b );
	gDP.fogColor.a = GXcastu8f32( a );

	gDP.changed |= CHANGED_FOGCOLOR;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetFogColor( %i, %i, %i, %i );\n",
		r, g, b, a );
#endif
}

void gDPSetFillColor( u32 c )
{
	gDP.fillColor.r = _FIXED2FLOAT( (u8)_SHIFTR( c, 11, 5 ), 5);
	gDP.fillColor.g = _FIXED2FLOAT( (u8)_SHIFTR( c,  6, 5 ), 5);
	gDP.fillColor.b = _FIXED2FLOAT( (u8)_SHIFTR( c,  1, 5 ), 5);
	gDP.fillColor.a = (u8)_SHIFTR( c, 0, 1 );

	gDP.fillColor.z = (u16)_SHIFTR( c, 2, 14 );
	gDP.fillColor.dz = (u8)_SHIFTR( c, 0, 2 );

	gDP.fillColor.color = c;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPSetFillColor( 0x%08X );\n", c );
#endif
}

void gDPSetPrimColor( u32 m, u32 l, u32 r, u32 g, u32 b, u32 a )
{
	gDP.primColor.m = m;
	gDP.primColor.l = GXcastu8f32( l );
	gDP.primColor.r = GXcastu8f32( r );
	gDP.primColor.g = GXcastu8f32( g );
	gDP.primColor.b = GXcastu8f32( b );
	gDP.primColor.a = GXcastu8f32( a );

	gDP.changed |= CHANGED_COMBINE_COLORS;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_COMBINE, "gDPSetPrimColor( %i, %i, %i, %i, %i, %i );\n",
		m, l, r, g, b, a );
#endif
}

void gDPSetTile( u32 format, u32 size, u32 line, u32 tmem, u32 tile, u32 palette, u32 cmt, u32 cms, u32 maskt, u32 masks, u32 shiftt, u32 shifts )
{
	if (((size == G_IM_SIZ_4b) || (size == G_IM_SIZ_8b)) && (format == G_IM_FMT_RGBA))
		format = G_IM_FMT_CI;

	gDP.tiles[tile].format = format;
	gDP.tiles[tile].size = size;
	gDP.tiles[tile].line = line;
	gDP.tiles[tile].tmem = tmem;
	gDP.tiles[tile].palette = palette;
	gDP.tiles[tile].cmt = cmt;
	gDP.tiles[tile].cms = cms;
	gDP.tiles[tile].maskt = gDP.tiles[tile].originalMaskT = maskt;
	gDP.tiles[tile].masks = gDP.tiles[tile].originalMaskS = masks;
	gDP.tiles[tile].shiftt = shiftt;
	gDP.tiles[tile].shifts = shifts;

	if (!gDP.tiles[tile].masks) gDP.tiles[tile].clamps = 1;
    if (!gDP.tiles[tile].maskt) gDP.tiles[tile].clampt = 1;

	/* [Xbox360] GLideN64 gDP.cpp:318-328: el tile de render que se configura
	 * DESPUES de la carga (SetTile 7, Load, SetTile 0 -- el orden normal) hereda
	 * el estado de framebuffer del tile de carga con su misma TMEM.  Sin esto el
	 * tile 0 conservaba el framebuffer de una carga anterior.  Ver
	 * _gDPPropagaCargaFB. */
	if ((s32)tile == gSP.texture.tile || (s32)tile == gSP.texture.tile + 1)
	{
		s32 nTile = gDP.loadTile ? (s32)(gDP.loadTile - gDP.tiles) : 0;
		while (gDP.tiles[nTile].tmem != tmem && nTile > gSP.texture.tile + 1)
			--nTile;
		if (nTile > gSP.texture.tile + 1)
		{
			gDP.tiles[tile].textureMode        = gDP.tiles[nTile].textureMode;
			gDP.tiles[tile].loadType           = gDP.tiles[nTile].loadType;
			gDP.tiles[tile].frameBufferAddress = gDP.tiles[nTile].frameBufferAddress;
			gDP.tiles[tile].imageAddress       = gDP.tiles[nTile].imageAddress;
		}
	}

	/* [Xbox360] ★ [Mystical Ninja Starring Goemon: pared, suelo y ojos] COMO
	 * GLideN64 13d53a0d ("Set CHANGED_TILE flag in gDPSetTile", GLideN64 1.1;
	 * issue #423 es este juego: fondos corruptos y un ojo que no sale).  glN64
	 * no marcaba nada aqui -- solo gDPSetTileSize --, asi que un SetTile que
	 * cambia mascara o clamp SIN carga nueva no recalculaba la textura y se
	 * seguia atando la anterior.  MEDIDO (log 35, [RDPTEX]): tras la carga de
	 * la pared el juego alterna el tile 0 entre `mask 5,6` y `mask 5,5` sin
	 * cargar; con 5,5 sus t (32..46) dan la vuelta a las filas 0-14, las
	 * piedras, y nosotros seguiamos con la textura de 64 filas. */
	gDP.changed |= CHANGED_TILE;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPSetTile( %s, %s, %i, %i, %i, %i, %s%s, %s%s, %i, %i, %i, %i );\n",
		ImageFormatText[format],
		ImageSizeText[size],
		line,
		tmem,
		tile,
		palette,
		cmt & G_TX_MIRROR ? "G_TX_MIRROR" : "G_TX_NOMIRROR",
		cmt & G_TX_CLAMP ? " | G_TX_CLAMP" : "",
		cms & G_TX_MIRROR ? "G_TX_MIRROR" : "G_TX_NOMIRROR",
		cms & G_TX_CLAMP ? " | G_TX_CLAMP" : "",
		maskt,
		masks,
		shiftt,
		shifts );
#endif
}


void gDPSetTileSize( u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt )
{
	gDP.tiles[tile].uls = _SHIFTR( uls, 2, 10 );
	gDP.tiles[tile].ult = _SHIFTR( ult, 2, 10 );
	gDP.tiles[tile].lrs = _SHIFTR( lrs, 2, 10 );
	gDP.tiles[tile].lrt = _SHIFTR( lrt, 2, 10 );

	gDP.tiles[tile].fuls = _FIXED2FLOAT( uls, 2 );
	gDP.tiles[tile].fult = _FIXED2FLOAT( ult, 2 );
	gDP.tiles[tile].flrs = _FIXED2FLOAT( lrs, 2 );
	gDP.tiles[tile].flrt = _FIXED2FLOAT( lrt, 2 );

	gDP.changed |= CHANGED_TILE;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPSetTileSize( %i, %.2f, %.2f, %.2f, %.2f );\n",
		tile,
		gDP.tiles[tile].fuls,
		gDP.tiles[tile].fult,
		gDP.tiles[tile].flrs,
		gDP.tiles[tile].flrt );
#endif
}

/* [Xbox360] ★★ [Mario Tennis en partida] El estado de "esto es un framebuffer"
 * POR TILE, como GLideN64 (gDP.cpp:377-457): al terminar la comprobacion, haya
 * salido bien o mal, todos los tiles que comparten la TMEM de la carga se quedan
 * con su modo, su tipo de carga, su framebuffer y su direccion de imagen.
 *
 * glN64 solo ponia `frameBufferAddress` en esos tiles cuando SALIA BIEN, y el
 * modo era uno global.  Medido con [FBACT] (log 7): un tile se quedaba con el
 * framebuffer de una carga ANTERIOR (`buf 003D6000` activado para una carga de
 * `000D7C10`), y en cuanto otra carga ponia el modo global en FRAMEBUFFER se
 * activaba ese buffer equivocado. */
static void _gDPPropagaCargaFB( void )
{
	u32 i;
	for (i = 0; i < 8; i++)
	{
		gDPTile *t = &gDP.tiles[i];
		if (t != gDP.loadTile && t->tmem == gDP.loadTile->tmem)
		{
			t->textureMode        = gDP.loadTile->textureMode;
			t->loadType           = gDP.loadTile->loadType;
			t->frameBufferAddress = gDP.loadTile->frameBufferAddress;
			t->imageAddress       = gDP.loadTile->imageAddress;
		}
	}
}

/* El cuerpo de la comprobacion; sus `return` son los de siempre.  La envoltura
 * de abajo es la que llaman las cargas y la que propaga, en todas las salidas. */
static BOOL _gDPCheckForFrameBufferTextureCuerpo( u32 address, u32 width, u32 bytes, u32 loadType );

// GLideN64's CheckForFrameBufferTexture()
static BOOL _gDPCheckForFrameBufferTexture( u32 address, u32 width, u32 bytes, u32 loadType )
{
	BOOL res = _gDPCheckForFrameBufferTextureCuerpo( address, width, bytes, loadType );
	_gDPPropagaCargaFB();
	return res;
}

static BOOL _gDPCheckForFrameBufferTextureCuerpo( u32 address, u32 width, u32 bytes, u32 loadType )
{
	gDP.loadTile->frameBufferAddress = 0;
	gDP.loadTile->textureMode = TEXTUREMODE_NORMAL;
	gDP.loadTile->loadType = loadType;
	gDP.textureMode = TEXTUREMODE_NORMAL;
	gDP.changed |= CHANGED_TMEM;

	if (!OGL.frameBufferTextures)
	{
		return FALSE;
	}

	FrameBuffer *buffer = FrameBuffer_FindBuffer( address );
	if (buffer == NULL)
	{
		return FALSE;
	}

	const u32 texEndAddress = address + bytes - 1;
	if (address > buffer->startAddress &&
	    (u32)abs( (s32)buffer->width - (s32)width ) > 1 &&
	    texEndAddress > (buffer->endAddress + (buffer->width << buffer->size >> 1)))
	{
		return FALSE;
	}

	const u32 bufferStride = buffer->width << buffer->size >> 1;

	/* ★★ DIVERGENCIA vs Wii64.  Ver DIVERGENCIAS.txt punto I.
	 *
	 * El guarda de alineacion a fila de aqui abajo NO ES DE LA REFERENCIA:
	 * `CheckForFrameBufferTexture` de GLideN64 (src/gDP.cpp:418-433) tiene
	 * solo dos filtros -- texEndAddress y LOADTYPE_TILE -- y ninguno mira la
	 * alineacion.  Lo anadio Wii64 porque su `ActivateBufferTexture` no
	 * calculaba el desplazamiento en S y no sabria donde muestrear una carga
	 * que no empiece en una fila.
	 *
	 * Rechaza de mas: Pokemon Stadium carga la parte DERECHA de sus paneles
	 * con un tile de 72 texels a 200 bytes de un buffer de 172 de ancho
	 * (200 % 344 != 0, o sea el texel 100), esas cargas caian a TMEM -- que
	 * glN64 nunca escribe, porque pinta en el EFB -- y salia ruido en la
	 * mitad derecha de cada panel.
	 *
	 * ★ No se puede quitar entero: probado, y deja pasar texturas NORMALES
	 * que solo por casualidad viven dentro del rango de un framebuffer.  Se
	 * llevo por delante el texto del HUD de combate.
	 *
	 * Asi que se ESTRECHA: no se aplica cuando la imagen de textura ES el
	 * framebuffer -- mismo ancho, mismo tamano y carga por tile.  Las tres
	 * condiciones salen del log, no de una intuicion:
	 *
	 *     paneles del menu   ti=172x(size 2)  bufW=172 bufSize=2   -> pasa
	 *     banda inferior     ti=538x(size 2)  bufW=538 bufSize=2   -> pasa
	 *     texto del HUD      la imagen es la fuente, no el buffer  -> corta
	 *
	 * y lo de TILE esta medido: de las 93 cargas que dependen de esta
	 * excepcion, las 93 son TILE y ninguna BLOCK -- que es por donde un
	 * juego se lee el framebuffer ENTERO.
	 *
	 * Es ademas el mismo criterio que usa el filtro de LOADTYPE_TILE de aqui
	 * abajo (`gDP.textureImage.width != buffer->width`). */
	const BOOL imagenEsElBuffer = (loadType == LOADTYPE_TILE &&
	                               gDP.textureImage.width == buffer->width &&
	                               gDP.textureImage.size  == buffer->size) ? TRUE : FALSE;

	/* [Xbox360] ★★ [Conker, sprites del suelo, 2026-10-16] EL FILTRO VALE
	 * TAMBIEN PARA LOS BUFFERS PRINCIPALES.  Para Mario Golf se le puso una
	 * excepcion (`validezFuerte`: no aplicarlo si el buffer tiene `rdramCopy`)
	 * que alli no arreglo nada (log 8; los parches eran el shiftScale de
	 * 7ca20f8a) y se quedo puesta.  MEDIDO (log 5): con ella, ~420 cargas/s de
	 * sprites de Conker (0x153768..0x15A578, bloques de 1760 texels) se
	 * servian desde el buffer principal 0x14CCC0 -- setas, hierba y agua con
	 * trozos de la escena --; sin ella, 0.  Las que caen en una fila por
	 * casualidad las para la copia unica de la RDRAM (FrameBuffer.cpp,
	 * _fbCopiaRdram). */
	/* [Xbox360] Wii64 2e1c6fb "fix BK pause screen size calc": en una carga
	 * por tile el ancho de la fila es el de la imagen de textura, no el del
	 * tile ("Banjo Kazooie pause screen needs gDP.textureImage.width"). */
	const u32 rowWidth = (loadType == LOADTYPE_TILE) ? gDP.textureImage.width : width;
	if (!imagenEsElBuffer &&
	    address > buffer->startAddress && bufferStride != 0 &&
	    (u32)abs( (s32)buffer->width - (s32)rowWidth ) > 1 &&
	    ((address - buffer->startAddress) % bufferStride) != 0)
	{
		return FALSE;
	}

	if (loadType == LOADTYPE_TILE &&
	    gDP.textureImage.width != buffer->width &&
	    gDP.textureImage.size != buffer->size)
	{
		return FALSE;
	}

	if (!FrameBuffer_IsValid( buffer ))
	{
		if (buffer->startAddress != gDP.colorImage.address)
			FrameBuffer_Remove( buffer );

		return FALSE;
	}

	FrameBuffer_RefreshCurrent( buffer );

	/* GLideN64 gDP.cpp:441-445: el origen de la carga va al BUFFER, y el modo
	 * al tile.  La propagacion a los tiles que comparten TMEM la hace la
	 * envoltura. */
	buffer->loadType = loadType;
	buffer->loadUls  = gDP.loadTile->uls;
	buffer->loadUlt  = gDP.loadTile->ult;
	gDP.loadTile->frameBufferAddress = buffer->startAddress;
	gDP.loadTile->textureMode = TEXTUREMODE_FRAMEBUFFER;
	FrameBuffer_MoveToTop( buffer );
	gDP.textureMode = TEXTUREMODE_FRAMEBUFFER;
	gDP.loadType = loadType;

	return TRUE;
}

/* [Xbox360] LAS CARGAS ENVUELVEN LA TMEM, como en la consola y como GLideN64
 * (UnswapCopyWrap con mascara 0xFFF y DWordInterleaveWrap con 0x3FF,
 * gDP.cpp:579-590 y :705-730).  glN64 DESCARTABA la carga entera si
 * `tmem*8 + bytes` pasaba de 4096, y la TMEM se quedaba con lo anterior.
 * Portado durante el caso de Goemon, pero NO era su causa: alli ninguna carga
 * se sale (log 34, cero envueltas); la causa fue CHANGED_TILE en gDPSetTile.
 * Se deja por ser lo de la referencia.  Solo el camino big-endian (el
 * nuestro); si la carga cabe, el codigo de siempre. */
static void _gDPCopiaTMEMEnvuelta( u32 tmemQW, const u8 *src, u32 bytes )
{
	u32 d = (tmemQW << 3) & 0xFFF;
	while (bytes > 0)
	{
		u32 trozo = 4096 - d;
		if (trozo > bytes)
			trozo = bytes;
		memcpy( (u8*)TMEM + d, src, trozo );
		src += trozo;
		bytes -= trozo;
		d = 0;
	}
}

/* DWordInterleave (es32 = FALSE: las dos palabras de cada qword) y
 * QWordInterleave (es32 = TRUE: los qwords por parejas) con la direccion de la
 * TMEM envuelta.  `qwords` como el segundo argumento de esas dos. */
static void _gDPInterleaveEnvuelto( u32 tmemQW, u32 qwords, BOOL es32 )
{
	if (!es32)
	{
		while (qwords--)
		{
			u32 *p = (u32*)&TMEM[tmemQW & 0x1FF];
			const u32 tmp = p[0];
			p[0] = p[1];
			p[1] = tmp;
			tmemQW++;
		}
	}
	else
	{
		qwords >>= 1;
		while (qwords--)
		{
			u64 *a = &TMEM[tmemQW & 0x1FF];
			u64 *b = &TMEM[(tmemQW + 1) & 0x1FF];
			const u64 tmp = *a;
			*a = *b;
			*b = tmp;
			tmemQW += 2;
		}
	}
}

void gDPLoadTile( u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt )
{
	GLN64_PROF_SCOPE(ldtile);  /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	u32 address, height, bpl, line, y;
	u64 *dest;
	u8 *src;

	gDPSetTileSize( tile, uls, ult, lrs, lrt );
	gDP.loadTile = &gDP.tiles[tile];
	gDP.loadTile->imageAddress = gDP.textureImage.address;	// [Xbox360] GLideN64 gDP.cpp:500

	if (gDP.loadTile->line == 0)
		return;

	void (*Interleave)( void *mem, u32 numDWords );

	if (gDP.loadTile->size == G_IM_SIZ_32b)
	{
		line = gDP.loadTile->line << 1;
		Interleave = QWordInterleave;
	}
	else
	{
		line = gDP.loadTile->line;
		Interleave = DWordInterleave;
	}

	address = gDP.textureImage.address + gDP.loadTile->ult * gDP.textureImage.bpl + (gDP.loadTile->uls << gDP.textureImage.size >> 1);
	dest = &TMEM[gDP.loadTile->tmem];
	bpl = line << 3;
	height = gDP.loadTile->lrt - gDP.loadTile->ult + 1;
	src = &RDRAM[address];

	// Record what this load actually put at this TMEM address
	{
		const u32 loadedWidth = (gDP.loadTile->lrs - gDP.loadTile->uls + 1) & 0x03FF;
		gDPLoadTileInfo &info = gDP.loadInfo[gDP.loadTile->tmem & 0x1FF];
		info.width = (u16)((gDP.loadTile->masks != 0) ? MIN( loadedWidth, (u32)1 << gDP.loadTile->masks ) : loadedWidth);
		info.height = (u16)((gDP.loadTile->maskt != 0) ? MIN( height, (u32)1 << gDP.loadTile->maskt ) : height);
		info.texAddress = address;
		info.texWidth = (u16)gDP.textureImage.width;
		info.size = (u8)gDP.textureImage.size;
		info.loadType = LOADTYPE_TILE;

		if (gDP.loadTile->masks == 0)
			gDP.loadTile->loadWidth = MAX( gDP.loadTile->loadWidth, (u32)info.width );

		if (gDP.loadTile->maskt == 0)
		{
			if ((gDP.otherMode.cycleType != G_CYC_2CYCLE) && (line != 0) && ((gDP.loadTile->tmem % line) == 0))
			{
				const u32 theight = (u32)info.height + gDP.loadTile->tmem / line;
				gDP.loadTile->loadHeight = MAX( gDP.loadTile->loadHeight, theight );
			}
			else
				gDP.loadTile->loadHeight = MAX( gDP.loadTile->loadHeight, (u32)info.height );
		}
	}

	/* [Xbox360] ★ [Mario Tennis] PRIMERO SI ES UN FRAMEBUFFER, LUEGO EL RANGO,
	 * como GLideN64 (gDP.cpp:565-571).  Aqui iba al reves: una carga que no cabe
	 * en la TMEM salia antes de la comprobacion y el tile se quedaba con la
	 * direccion de imagen NUEVA y el framebuffer de la carga ANTERIOR.  Medido
	 * con [FBACT] (log 9): `buf 00000400` activado con la imagen en `003AC000`,
	 * desplazamiento +3849216.  Si es framebuffer no hace falta copiar nada a la
	 * TMEM, asi que el rango solo importa para el camino normal. */
	if (_gDPCheckForFrameBufferTexture( address,
	        gDP.loadInfo[gDP.loadTile->tmem & 0x1FF].width, bpl * height, LOADTYPE_TILE ))
		return;

	/* [Xbox360] La TMEM se envuelve (ver _gDPCopiaTMEMEnvuelta); solo el rango
	 * de RDRAM descarta la carga. */
	const BOOL envuelve = (((gDP.loadTile->tmem << 3) + bpl * height) > 4096) ? TRUE : FALSE;
	if (((address + height * bpl) > RDRAMSize)
#ifndef _BIG_ENDIAN
		|| envuelve // Stay within TMEM
#endif // _BIG_ENDIAN
		)
	{
#ifdef DEBUG
		DebugMsg( DEBUG_HIGH | DEBUG_ERROR | DEBUG_TEXTURE, "// Attempting to load texture tile out of range\n" );
		DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPLoadTile( %i, %i, %i, %i, %i );\n",
			tile, gDP.loadTile->uls, gDP.loadTile->ult, gDP.loadTile->lrs, gDP.loadTile->lrt );
#endif
		return;
	}

	if (!envuelve)
	{
		for (y = 0; y < height; y++)
		{
#ifndef _BIG_ENDIAN
			UnswapCopy( src, dest, bpl );
#else // !_BIG_ENDIAN
			memcpy( dest, src, bpl );
#endif // _BIG_ENDIAN
			if (y & 1) Interleave( dest, line );

			src += gDP.textureImage.bpl;
	 		dest += line;
		}
	}
	else
	{
		u32 tq = gDP.loadTile->tmem;
		for (y = 0; y < height; y++)
		{
			_gDPCopiaTMEMEnvuelta( tq, src, bpl );
			if (y & 1)
				_gDPInterleaveEnvuelto( tq, line, (gDP.loadTile->size == G_IM_SIZ_32b) ? TRUE : FALSE );

			src += gDP.textureImage.bpl;
			tq += line;
		}
	}

	gDP.textureMode = TEXTUREMODE_NORMAL;
	gDP.loadType = LOADTYPE_TILE;
	gDP.changed |= CHANGED_TMEM;

#ifdef DEBUG
		DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPLoadTile( %i, %i, %i, %i, %i );\n",
			tile, gDP.loadTile->uls, gDP.loadTile->ult, gDP.loadTile->lrs, gDP.loadTile->lrt );
#endif
}

void gDPLoadBlock( u32 tile, u32 uls, u32 ult, u32 lrs, u32 dxt )
{
	GLN64_PROF_SCOPE(ldtile);  /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	gDPSetTileSize( tile, uls, ult, lrs, dxt );
	gDP.loadTile = &gDP.tiles[tile];

	/* [Xbox360] GLideN64 268278a6, en el mismo sitio (antes de fijar
	 * imageAddress): si el desplazamiento no casa con el tamano del bloque se
	 * deshace y se apaga; si casa, la siguiente textura toma la siguiente
	 * entrada de la tabla.  Ver gSPSetDMATexOffset. */
	if (gSP.DMAOffsets.tex_offset != 0)
	{
		if (gSP.DMAOffsets.tex_shift % (((lrs >> 2) + 1) << 3))
		{
			gDP.textureImage.address -= gSP.DMAOffsets.tex_shift;
			gSP.DMAOffsets.tex_offset = 0;
			gSP.DMAOffsets.tex_shift = 0;
			gSP.DMAOffsets.tex_count = 0;
		}
		else
			++gSP.DMAOffsets.tex_count;
	}

	gDP.loadTile->imageAddress = gDP.textureImage.address;	// [Xbox360] GLideN64 gDP.cpp:660

	gDP.loadInfo[gDP.loadTile->tmem & 0x1FF].loadType = LOADTYPE_BLOCK;

 	u32 bytes = (((lrs - uls + 1) & 0x0FFF) << gDP.loadTile->size) >> 1;
	if ((bytes & 7) != 0)
		bytes = (bytes & ~7U) + 8;
	u32 address = gDP.textureImage.address + ult * gDP.textureImage.bpl + (uls << gDP.textureImage.size >> 1);

	/* [Xbox360] [Mario Tennis] Mismo orden que GLideN64 (gDP.cpp:681-688): el
	 * rango de RDRAM antes y la comprobacion de framebuffer despues.  El limite
	 * de TMEM que habia al final ya no descarta: se envuelve, como la
	 * referencia (Goemon; ver _gDPCopiaTMEMEnvuelta).  Ver la nota gemela en
	 * gDPLoadTile. */
	if ((bytes == 0) ||
		((address + bytes) > RDRAMSize))
	{
#ifdef DEBUG
		DebugMsg( DEBUG_HIGH | DEBUG_ERROR | DEBUG_TEXTURE, "// Attempting to load texture block out of range\n" );
		DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPLoadBlock( %i, %i, %i, %i, %i );\n",
			tile, uls, ult, lrs, dxt );
#endif
//		bytes = min( bytes, min( RDRAMSize - gDP.textureImage.address, 4096 - (gDP.loadTile->tmem << 3) ) );
		return;
	}

	/* [Xbox360] [Paper Mario] GLideN64 (gDP.cpp:688) carga la TMEM AUNQUE la
	 * textura sea un framebuffer: "Load data to TMEM even if FB texture is
	 * found".  glN64 sale aqui.  La textura de oscuridad de Paper Mario cae en
	 * un framebuffer (0x400) y texturedRectPaletteMod la quiere de la TMEM
	 * (_gDPTexrectPaletteMod), asi que con ese hack se carga tambien, como en
	 * la referencia, sin tocar el modo que acaba de poner la comprobacion. */
	const BOOL esFB = _gDPCheckForFrameBufferTexture( address, (lrs - uls + 1) & 0x0FFF, bytes,
	                                                  LOADTYPE_BLOCK );
	if (esFB && (config.generalEmulation.hacks & hack_texrectPaletteMod) == 0)
		return;

	/* [Xbox360] La TMEM se envuelve (ver _gDPCopiaTMEMEnvuelta). */
	const BOOL envuelve = (((gDP.loadTile->tmem << 3) + bytes) > 4096) ? TRUE : FALSE;
#ifndef _BIG_ENDIAN
	if (envuelve)
		return;
#endif // _BIG_ENDIAN

	gDP.loadInfo[gDP.loadTile->tmem & 0x1FF].texAddress = address;

	u64* src = (u64*)&RDRAM[address];
	u64* dest = &TMEM[gDP.loadTile->tmem];

#ifndef _BIG_ENDIAN
	UnswapCopy( src, dest, bytes );
#else // !_BIG_ENDIAN
	if (!envuelve)
		memcpy( dest, src, bytes );
	else
	{
		_gDPCopiaTMEMEnvuelta( gDP.loadTile->tmem, (const u8*)src, bytes );
	}
#endif // _BIG_ENDIAN

	if (dxt != 0)
	{
		void (*Interleave)( void *mem, u32 numDWords );

		if (gDP.loadTile->size == G_IM_SIZ_32b)
			Interleave = QWordInterleave;
		else
			Interleave = DWordInterleave;

		u32 tmemAddr = gDP.loadTile->tmem;
		u32 qwords = bytes >> 3;
		u32 dxtCounter = 0;
		u32 line = 0;
		while (true)
		{
			do
			{
				++tmemAddr;
				if (--qwords == 0)
					goto end_dxt;
				dxtCounter += dxt;
			} while ((dxtCounter & 0x800) == 0);
			do
			{
				++line;
				if (--qwords == 0)
					goto end_dxt;
				dxtCounter += dxt;
			} while ((dxtCounter & 0x800) != 0);
			if (envuelve)
				_gDPInterleaveEnvuelto( tmemAddr, line, (gDP.loadTile->size == G_IM_SIZ_32b) ? TRUE : FALSE );
			else
				Interleave( &TMEM[tmemAddr], line );
			tmemAddr += line;
			line = 0;
		}
end_dxt:
		if (line > 0)
		{
			if (envuelve)
				_gDPInterleaveEnvuelto( tmemAddr, line, (gDP.loadTile->size == G_IM_SIZ_32b) ? TRUE : FALSE );
			else
				Interleave( &TMEM[tmemAddr], line );
		}
	}

	if (!esFB)   // [Paper Mario] el framebuffer conserva su modo; ver arriba
	{
		gDP.textureMode = TEXTUREMODE_NORMAL;
		gDP.loadType = LOADTYPE_BLOCK;
	}
	gDP.changed |= CHANGED_TMEM;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPLoadBlock( %i, %i, %i, %i, %i );\n",
		tile, uls, ult, lrs, dxt );
#endif
}

void gDPLoadTLUT( u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt )
{
	gDPSetTileSize( tile, uls, ult, lrs, lrt );

	if (gDP.tiles[tile].tmem < 256) {
#ifdef DEBUG
		DebugMsg(DEBUG_NORMAL | DEBUG_ERROR, "gDPLoadTLUT wrong tile tmem addr: tile[%d].tmem=%04x;\n", tile, gDP.tiles[tile].tmem);
#endif
		return;
	}

	u16 count = (u16)((gDP.tiles[tile].lrs - gDP.tiles[tile].uls + 1) * (gDP.tiles[tile].lrt - gDP.tiles[tile].ult + 1));
	u32 address = gDP.textureImage.address + gDP.tiles[tile].ult * gDP.textureImage.bpl + (gDP.tiles[tile].uls << gDP.textureImage.size >> 1);
	u16 pal = (u16)((gDP.tiles[tile].tmem - 256) >> 4);
	u32 destIdx = gDP.tiles[tile].tmem << 2;

	/* [Xbox360] ★ [NBA Live 99, piel y caras con colores de otra paleta]
	 * Aqui habia `if (pal != 0) count = 16;`: el parche de Banjo-Tooie
	 * (GLideN64 8fbd1d03, issue 1250) que trunca a 16 colores cualquier TLUT
	 * que no empiece en la entrada 256.  GLideN64 lo QUITO en 83e1ccb7
	 * ("Fixed NBA Live 99 messed up textures #1826"): la escritura ya va
	 * enmascarada a la mitad alta (`(destIdx | 0x0400) & 0x07FF`, que este
	 * arbol ya tiene), asi que no puede salirse de la TMEM.  Con el recorte,
	 * las sub-paletas de despues de la primera se quedaban con lo que hubiera
	 * cargado otro (las camisetas).  ★RESUELTO, log 33: NBA Live carga
	 * `tmem=414 pal=9 count=98` y ya salen la piel y las caras. */

	int i = 0;
	while (i < count) {
		for (u16 j = 0; (j < 16) && (i < count); ++j, ++i) {
			u64 *dest = &TMEM[((destIdx | 0x0400) & 0x07FF) >> 2];
#ifndef _BIG_ENDIAN
			*dest = (u64) swapword( *(u16*)(RDRAM + (address ^ 2)) );
#else // !_BIG_ENDIAN
			*dest = (u64) *(u16*)(RDRAM + address) << 48;
#endif // _BIG_ENDIAN
			address += 2;
			destIdx += 4;
		}

		/* [Xbox360] ★ EL CRC DE ESTA SUB-PALETA, AQUI Y NO EN CADA CONSULTA.
		 *
		 * Las dos lineas son de GLideN64 (src/gDP.cpp:758 y :762) y encajan
		 * en el hueco que dejaron: `pal` ya se calculaba y se incrementaba en
		 * este bucle SIN USARSE PARA NADA.  El porque completo esta en
		 * `gDP.paletteCRC16` (gDP.h).
		 *
		 * Semilla FIJA (0xFFFFFFFF, la misma con la que arranca
		 * `TextureCache_CalculateCRC`): tiene que ser independiente del CRC
		 * de los texels, que es justo lo que no se podia hacer antes.
		 *
		 * 128 bytes = las 16 entradas de 64 bits de esta sub-paleta, o sea
		 * EXACTAMENTE los mismos bytes que hasheaba la rama de 4b. */
		gDP.paletteCRC16[pal] =
			Hash_Calculate( 0xFFFFFFFF, &TMEM[0x100 + (pal << 4)], 128 );

		pal = (pal + 1) & 0x0F;
	}

	/* El de 256 entradas es el CRC DE LOS DIECISEIS, no el de los 2.048 bytes
	 * (GLideN64 src/gDP.cpp:762).  No es el mismo valor que antes y da igual:
	 * como clave vale lo mismo, y cuesta 64 bytes en vez de 2.048. */
	gDP.paletteCRC256 =
		Hash_Calculate( 0xFFFFFFFF, gDP.paletteCRC16, sizeof(gDP.paletteCRC16) );

	gDP.changed |= CHANGED_TMEM;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED | DEBUG_TEXTURE, "gDPLoadTLUT( %i, %i, %i, %i, %i );\n",
		tile, gDP.tiles[tile].uls, gDP.tiles[tile].ult, gDP.tiles[tile].lrs, gDP.tiles[tile].lrt );
#endif
}

void gDPBufferChanged( f32 maxY )
{
	if (gDP.colorImage.address == gDP.depthImageAddress)
		DepthCopy_NoteDepthImageWritten();

	gDP.colorImage.changed = TRUE;
	gDP.colorImage.height = MAX( gDP.colorImage.height, (u32)maxY );
	/* [Xbox360] ★★ [Worms Armageddon, a los ~3,5 min en la misma pantalla:
	 * corrupciones y 13 img/s, logs (5) 2026-10-24] EL ALTO SE ACOTA AL
	 * SCISSOR, como GLideN64 (FrameBufferList::setBufferChanged,
	 * FrameBuffer.cpp:662-663).  glN64 solo hacia el MAX: un dibujo con `lry`
	 * enorme -- o negativo, que en (u32) sale 0x80000000 -- dejaba el alto del
	 * color image por las nubes, SaveBuffer rehacia el buffer con ese alto, y
	 * _fbCopiaRdram lo recorta al final de la RDRAM: cada buffer principal pasaba
	 * a cubrir megas.  MEDIDO: desde ese segundo cada FILLRECT costaba 13 ms en
	 * _gDPFillRDRAM (la copia de RDRAM de los dos buffers, desde
	 * FrameBuffer_RestampMarkers) en vez de 0,8, y cada LoadTile ~640 us en vez
	 * de 3,5 (FrameBuffer_IsValid comparando esa copia); las texturas que viven
	 * en esa memoria se servian como textura de framebuffer.  Confirmado con
	 * [FBALTO] (logs (6), sonda retirada): llegaban maxY = -1, -2, -3, o sea
	 * alto 4294967295 -> 240 con el scissor en 240.  Con el scissor
	 * aun sin poner (0 hasta el primer SetScissor, ver OGL_DrawRect) no se
	 * acota: dejaria el alto a 0. */
	if (gDP.scissor.lry > 0.0f && gDP.colorImage.height > (u32)gDP.scissor.lry)
		gDP.colorImage.height = (u32)gDP.scissor.lry;
}

void gDPSetScissor( u32 mode, f32 ulx, f32 uly, f32 lrx, f32 lry )
{
	gDP.scissor.mode = mode;
	gDP.scissor.ulx = ulx;
	gDP.scissor.uly = uly;
	gDP.scissor.lrx = lrx;
	gDP.scissor.lry = lry;

	// Recover the raw s10.2 values
	gDP.scissor.xh = (s16)(ulx * 4.0f);
	gDP.scissor.yh = (s16)(uly * 4.0f);
	gDP.scissor.xl = (s16)(lrx * 4.0f);
	gDP.scissor.yl = (s16)(lry * 4.0f);

	gDP.changed |= CHANGED_SCISSOR;

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_IGNORED, "gDPSetScissor( %s, %.2f, %.2f, %.2f, %.2f );\n",
		ScissorModeText[gDP.scissor.mode],
		gDP.scissor.ulx,
		gDP.scissor.uly,
		gDP.scissor.lrx,
		gDP.scissor.lry );
#endif
}

// GLideN64's DepthClearColor
u32 DepthClearColor = 0xFFFCFFFC;

void gDPSetDepthClearColor()
{
	if (strstr( RSP.romname, "Elmo's" ) != NULL)
		DepthClearColor = 0xFFFFFFFF;
	else if (strstr( RSP.romname, "Taz Express" ) != NULL)
		DepthClearColor = 0xFFBCFFBC;
	else if (strstr( RSP.romname, "NFL QBC 2000" ) != NULL ||
	         strstr( RSP.romname, "NFL Quarterback Club" ) != NULL ||
	         strstr( RSP.romname, "Jeremy McGrath Super" ) != NULL)
		DepthClearColor = 0xFFFDFFFC;
	else
		DepthClearColor = 0xFFFCFFFC;
}

// GLideN64's FrameBufferList::fillRDRAM()
static void _gDPFillRDRAM( s32 ulx, s32 uly, s32 lrx, s32 lry )
{
	if (gDP.otherMode.cycleType != G_CYC_FILL || gDP.colorImage.size != G_IM_SIZ_16b)
		return;

	const s32 width = (s32)gDP.colorImage.width;

	if (ulx < (s32)gDP.scissor.ulx) ulx = (s32)gDP.scissor.ulx;
	if (uly < (s32)gDP.scissor.uly) uly = (s32)gDP.scissor.uly;
	if (lrx > (s32)gDP.scissor.lrx) lrx = (s32)gDP.scissor.lrx;
	if (lry > (s32)gDP.scissor.lry) lry = (s32)gDP.scissor.lry;
	if (ulx < 0) ulx = 0;
	if (uly < 0) uly = 0;
	if (lrx > width) lrx = width;

	if (width == 0 || lrx <= ulx || lry <= uly)
		return;

	const u32 stride = (u32)width << 1;
	const u32 rowBytes = (u32)(lrx - ulx) << 1;
	const u32 start = gDP.colorImage.address + (u32)uly * stride + ((u32)ulx << 1);

	if (start >= RDRAMSize)
		return;

	if ((u32)lry > (RDRAMSize - gDP.colorImage.address) / stride)
		lry = (s32)((RDRAMSize - gDP.colorImage.address) / stride);

	if (lry <= uly)
		return;

	if (rowBytes == stride)
		_gDPWriteRDRAM( start, NULL, gDP.fillColor.color, (u32)(lry - uly) * stride );
	else
		for (s32 y = uly; y < lry; y++)
			_gDPWriteRDRAM( start + (u32)(y - uly) * stride, NULL, gDP.fillColor.color, rowBytes );

	FrameBuffer_RestampMarkers( start, start + (u32)(lry - uly - 1) * stride + rowBytes - 1 );
}

/* [Xbox360] ★ La profundidad que escribe un relleno sobre el depth image: el
 * rojo de 5 bits entre 31, como `getFillColor` de GLideN64 (gDP.cpp:271,
 * _FIXED2FLOATCOLOR) y su `gl_FragDepth = fragColor.r`.  NO gDP.fillColor.r:
 * glN64 lo calcula con _FIXED2FLOAT(r, 5), o sea entre 32, y FFFC daba 0,969
 * en vez de 1,0 -- un "borrado" que dejaba el Z por delante de casi toda la
 * escena.  Super Smash Bros. (borra con un inset 10,10-310,230) salia en negro
 * y las bob-ombs de Mario Tennis desaparecian. */
static f32 _gDPProfundidadDeRelleno( void )
{
	return (f32)_SHIFTR( gDP.fillColor.color, 11, 5 ) / 31.0f;
}

void gDPFillRectangle( s32 ulx, s32 uly, s32 lrx, s32 lry )
{
	DepthBuffer *buffer = DepthBuffer_FindBuffer( gDP.colorImage.address );

	if (buffer)
		buffer->cleared = TRUE;

	/* ★★ DIVERGENCIA vs Wii64.  Ver DIVERGENCIAS.txt punto L.
	 *
	 * Upstream fusiona con `||` dos casos que la referencia tiene SEPARADOS
	 * (`gDPFillRectangle` de GLideN64, src/gDP.cpp:914-948):
	 *
	 *     if (depthImageAddress == colorImage.address) {
	 *         // El juego puede usar la textura de profundidad como color
	 *         // auxiliar (ejemplo suyo: Mario Tennis).  Si el color NO es
	 *         // el de borrado de profundidad, es casi seguro ese caso.
	 *         if (fillColor == DepthClearColor) { ...borrar Z...; dbCleared; }
	 *     } else if (fillColor == DepthClearColor && cycleType == FILL) {
	 *         ...borrar Z...; dbCleared;
	 *     }
	 *     if (depthBuffer != dbCleared) { ...pintar el rect de COLOR... }
	 *
	 * Wii64 se dejo el `if` de dentro: cuando el color image coincide con el
	 * de profundidad, borra Z y VUELVE, pinte lo que pinte el juego.  Asi que
	 * un relleno de COLOR sobre ese buffer se lo traga el camino de
	 * profundidad y no se pinta nada.
	 *
	 * MEDIDO en Killer Instinct Gold: `clrz=8669` contra `clrcol=8` en todo
	 * el log -- casi ningun borrado de color -- y en pantalla todo deja
	 * estela, que es exactamente lo que pasa si el fondo no se repinta.
	 *
	 * Aqui se porta la estructura de la referencia: el borrado de
	 * profundidad solo se come el rect cuando de verdad lo ha borrado. */
	{
		BOOL zBorrado = FALSE;

		if (gDP.depthImageAddress == gDP.colorImage.address)
		{
			if (gDP.fillColor.color == DepthClearColor)
			{
				/* [Xbox360] ★★ [Mario Tennis, pantalla VS] SOLO EL RELLENO QUE CUBRE
				 * EL BUFFER ENTERO ES UN BORRADO.  GLideN64 669337f5 ("Fixed Mario
				 * Tennis 64's Versus Screen Issue #771") lo restringio a
				 * `ulx == 0 && lrx == width`, y el GLideN64 actual, con
				 * enableFragmentDepthWrite (su defecto), ni siquiera borra: dibuja el
				 * rect con profundidad = rojo del fill color (gDP.cpp:914-948).
				 * glN64 borraba el Z ENTERO con cualquier relleno FFFC sobre el depth
				 * image, y el juego hace rellenos parciales asi (`0,96-304,224
				 * fill=FFFC`): entre las dos escenas de la VS se llevaba por delante
				 * la mascara o la profundidad del rival.  Parcial: solo ese rect, a
				 * la profundidad del rojo (FFFC -> 1, lo mas lejano).
				 *
				 * ★ SOLO EL ANCHO, como 669337f5: pedir ademas toda la altura hizo
				 * parciales los borrados de los aux de 64x64 (bob-ombs, tortugas),
				 * que cubren su ancho pero no 224 filas.  Con un solo EFB el resto
				 * del Z conservaba la pista, y el pegado al marcador (Z_CMP) fallaba
				 * contra ella: desaparecian.  GLideN64 tiene un depth buffer por
				 * ancho y no lo sufre. */
				const BOOL entero = (ulx <= 0 &&
				                     lrx >= (s32)gDP.colorImage.width) ? TRUE : FALSE;
				if (entero || gDP.otherMode.cycleType != G_CYC_FILL)
					OGL_ClearDepthBuffer();
				else
					OGL_DrawDepthFill( ulx, uly, lrx, lry, _gDPProfundidadDeRelleno() );
				zBorrado = TRUE;
			}
			else
			{
				/* [Xbox360] ★★ [Mario Tennis, pantalla VS] El relleno de COLOR
				 * sobre el depth image tambien escribe PROFUNDIDAD (= rojo del
				 * fill color), como GLideN64 con enableFragmentDepthWrite.  El
				 * color lo sigue pintando el OGL_DrawRect de abajo.  Ver
				 * OGL_DrawDepthFill. */
				if (gDP.otherMode.cycleType == G_CYC_FILL)
					OGL_DrawDepthFill( ulx, uly, lrx, lry, _gDPProfundidadDeRelleno() );
			}
		}
		else if (gDP.fillColor.color == DepthClearColor &&
		         gDP.otherMode.cycleType == G_CYC_FILL)
		{
			OGL_ClearDepthBuffer();
			zBorrado = TRUE;
		}

		if (zBorrado)
		{
			/* [Xbox360] Wii64 443f68b, tal cual, dentro de la rama de borrado
			 * de profundidad de la divergencia L.  Sustituye a nuestro relleno
			 * de DK64 (opcion `xbox64_depth_rdram`, una vez por buffer), que
			 * se quito al portar este commit: tiene prioridad el de Wii64.
			 *
			 * Con la copia de profundidad activa (OoT, Majora, DK64: rama de
			 * la Wii, ver DepthCopy.cpp y xenon/gx_d3d9.cpp) el depth image
			 * guarda la profundidad del frame anterior y el borrado no se
			 * escribe; en los demas juegos se escribe en la RDRAM en cada
			 * frame, que es lo que hace el RDP de verdad. */
			const BOOL depthImage = (gDP.depthImageAddress == gDP.colorImage.address);
			if (depthImage)
				DepthCopy_NoteDepthClear();

			// With the depth copy running, the depth image holds the previous frame's depth
			// for the game to read after this list, so its clear must not land there.
			if (!depthImage || !DepthCopy_Active())
				_gDPFillRDRAM( ulx, uly, lrx, lry );

			/* [Xbox360] sonda temporal [FILL-GX]; ver xenon/gln64_prof.h. */
#if XBOX64_TRAZAS
			gln64_fill_nota( ulx, uly, lrx, lry, gDP.otherMode.cycleType,
			                 gDP.fillColor.color, gDP.colorImage.address,
			                 gDP.depthImageAddress, 1 );
#endif
			return;
		}
	}

	if (gDP.otherMode.cycleType == G_CYC_FILL)
	{
		//if (gDP.fillColor.a == 0.0f)
		//	return;

		if ((ulx == 0) && (uly == 0) &&
		    ((unsigned int)lrx >= VI.width) && ((unsigned int)lry >= VI.height))
		{
			/* [Xbox360] sonda temporal [FILL-GX]; ver xenon/gln64_prof.h. */
#if XBOX64_TRAZAS
			gln64_fill_nota( ulx, uly, lrx, lry, gDP.otherMode.cycleType,
			                 gDP.fillColor.color, gDP.colorImage.address,
			                 gDP.depthImageAddress, 2 );
#endif
			OGL_ClearColorBuffer( _gDPFillColorActual() );
			/* [Xbox360] ★ [Star Fox 64, ruido en el borde de arriba] EL BORRADO
			 * ENTERO TAMBIEN VA A LA RDRAM, como todo relleno en GLideN64
			 * (gDPFillRectangle -> FrameBufferList::fillRDRAM, salvo con
			 * copyFromRDRAM: aqui hack_copyFromRDRAM).  Star Fox borra al arrancar
			 * 0,0-320,720 = sus TRES buffers seguidos (0038F800, 003B5000,
			 * 003DA800) de una vez, y despues solo el interior 8,8-312,233.  Sin
			 * esto la RDRAM de 0038F800 seguia con datos MIO0 ([RDRAM-GX]:
			 * 4D494F30...) y al cargarse el buffer de la RDRAM salian en el
			 * marco, que el juego no vuelve a pintar (scissor 8,8-312,232): ruido
			 * en un buffer de cada tres, parpadeando con negro. */
			/* [Xbox360] ★★ [Banjo-Kazooie, piezas del puzzle NEGRAS, 2026-10-24]
			 * PERO NO SOBRE EL BUFFER QUE RECIBE LA COPIA DEL COLOR.  En GLideN64
			 * (copyToRDRAM=1, ctSync) este relleno tambien cae en la RDRAM, pero su
			 * copia del fullsync lo tapa con el frame entero antes del DP.  La nuestra
			 * (_gDPCopiaColorARDRAM) llega 1 frame tarde: Banjo borra cada frame
			 * 0,0-292,216 con 0001 ([FILL-GX] "borra COLOR"), el borrado machacaba la
			 * copia anterior del mismo buffer, y al pulsar START la CPU sacaba las
			 * piezas de una pantalla de 0001 (volcado: filas enteras de 0x0001).
			 * Sin el borrado, la RDRAM del buffer guarda la ultima copia entregada:
			 * la escena de 2 frames antes.  Mismas condiciones que la copia. */
			const BOOL bufferDeCopia =
				((config.generalEmulation.hacks & hack_copyToRDRAM) != 0 &&
				 gDP.colorImage.size == G_IM_SIZ_16b &&
				 gDP.colorImage.width == VI.width &&
				 gDP.colorImage.address != gDP.depthImageAddress) ? TRUE : FALSE;
			if ((config.generalEmulation.hacks & hack_copyFromRDRAM) == 0 && !bufferDeCopia)
				_gDPFillRDRAM( ulx, uly, lrx, lry );
			/* [Xbox360] ★★ [Rally Challenge 2000: cielo y segundos del HUD con
			 * estela, y el reflejo de la luna trasera tambien arriba a la
			 * izquierda] EL BORRADO ENTERO TAMBIEN CAMBIA EL BUFFER, como en
			 * GLideN64, que llama a setBufferChanged(lry) tras CUALQUIER relleno.
			 * glN64 volvia antes y `colorImage.changed` seguia a FALSE.  MEDIDO
			 * con [SECFB] (Logs Salvia (13)): el juego borra el principal con el
			 * cielo (5C37), cambia al aux de 64x32 del retrovisor SIN guardar el
			 * principal (CI ... changed=0), y al volver REST repone la copia
			 * VIEJA: el cielo no se repinta (es el color del borrado) y queda lo
			 * de hace tres frames, retrovisor incluido.  Confirmado (log 14, sonda
			 * retirada): changed=1 y el cielo bien; arregla tambien la estela de
			 * la intro de Hexen.  Alto acotado al scissor
			 * (como setBufferChanged) y al VI: Star Fox borra 0,0-320,720 de una
			 * vez y un alto de 720 haria crecer el buffer sobre los otros dos. */
			{
				f32 alto = (f32)lry;
				if (gDP.scissor.lry > gDP.scissor.uly && alto > gDP.scissor.lry)
					alto = gDP.scissor.lry;
				if (VI.height != 0 && alto > (f32)VI.height)
					alto = (f32)VI.height;
				gDPBufferChanged( alto );
			}
			return;
		}

	}

	/* [Xbox360] sonda temporal [FILL-GX]; ver xenon/gln64_prof.h. */
#if XBOX64_TRAZAS
	gln64_fill_nota( ulx, uly, lrx, lry, gDP.otherMode.cycleType,
	                 gDP.fillColor.color, gDP.colorImage.address,
	                 gDP.depthImageAddress, 3 );
#endif

	OGL_DrawRect( ulx, uly, lrx, lry, (gDP.otherMode.cycleType == G_CYC_FILL) ? _gDPFillColorActual() : &gDP.blendColor.r );

	if (depthBuffer.current) depthBuffer.current->cleared = FALSE;
	if (gDP.otherMode.cycleType == G_CYC_FILL)
		gDPBufferChanged( (f32)((lry > (s32)VI.height) ? lry - 1 : lry) );
	else
		gDPBufferChanged( gDP.scissor.lry );

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPFillRectangle( %i, %i, %i, %i );\n",
		ulx, uly, lrx, lry );
#endif
}

void gDPSetConvert( s32 k0, s32 k1, s32 k2, s32 k3, s32 k4, s32 k5 )
{
	gDP.convert.k0 = (k0 << (32 - 9) >> (32 - 10)) + 1;
	gDP.convert.k1 = (k1 << (32 - 9) >> (32 - 10)) + 1;
	gDP.convert.k2 = (k2 << (32 - 9) >> (32 - 10)) + 1;
	gDP.convert.k3 = (k3 << (32 - 9) >> (32 - 10)) + 1;
	gDP.convert.k4 = k4;
	gDP.convert.k5 = k5;
}

void gDPSetKeyR( u32 cR, u32 sR, u32 wR )
{
	gDP.key.center.r = GXcastu8f32( cR );
	gDP.key.scale.r = GXcastu8f32( sR );
	gDP.key.width.r = GXcastu8f32( wR );
}

void gDPSetKeyGB(u32 cG, u32 sG, u32 wG, u32 cB, u32 sB, u32 wB )
{
	gDP.key.center.g = GXcastu8f32( cG );
	gDP.key.scale.g = GXcastu8f32( sG );
	gDP.key.width.g = GXcastu8f32( wG );
	gDP.key.center.b = GXcastu8f32( cB );
	gDP.key.scale.b = GXcastu8f32( sB );
	gDP.key.width.b = GXcastu8f32( wB );
}


static inline u8 _gDPFillByte( u32 fill, u32 address )
{
	return (u8)(fill >> ((3 - (address & 3)) << 3));
}

/* [Xbox360] ★★ [Cruis'n USA, fin de carrera en negro, logs (3)-(6),
 * 2026-10-21] CON LA "FOTO", LO QUE ESCRIBE EL PLUGIN SE QUEDA EN LA FOTO.
 * Con xbox64_lista_sp = foto el `RDRAM` del plugin apunta a una copia
 * (xbox/hilo_lista.cpp, s_foto) y el DP_INT sale con la lista a medias
 * (`dpantes`).  Hasta aqui estas escrituras (fillRDRAM, copyWhiteToRDRAM,
 * texturedRectBGCopy, el artefacto de PD, la paleta de Paper Mario, el color
 * a RDRAM) se copiaban TAMBIEN a la real -- se anadio por Star Fox 64, que
 * entonces iba en foto (Logs Salvia (4) del 2026-10-13) --.  Pero llegaban
 * TARDE: el juego, con su DP ya recibido, reutilizaba esa memoria (Cruis'n
 * carga ahi la carrera siguiente) y el relleno del plugin caia encima.  La
 * lista siguiente leia basura (imagenes de color 007CFFFC w=4093, 0000000D
 * w=289...), 6 VI/s y pantalla negra.  MEDIDO: con el DP esperando a la lista
 * no hay negro (logs (5)), y tampoco sin copiar nada a la real con el DP
 * adelantado (logs (6)), que es lo que se queda: el juego no ve nada de lo
 * que el plugin escribe durante una lista con foto.  Sin foto, RDRAM es la
 * real y todo se ve como siempre. */
extern "C" unsigned char *const rdramb;

static void _gDPWriteRDRAM( u32 dst, const u8 *src, u32 fill, u32 numBytes )
{
	const u32 end     = dst + numBytes;
	const u32 wgStart = (dst + 31) & ~31;
	const u32 wgEnd   = end & ~31;

	if (wgEnd <= wgStart)
	{
		if (src)
			memcpy( &RDRAM[dst], src, numBytes );
		else
			for (u32 a = dst; a < end; a++)
				RDRAM[a] = _gDPFillByte( fill, a );
		return;
	}

	const u32 head = wgStart - dst;
	const u32 span = wgEnd - wgStart;

	DCFlushRange( &RDRAM[wgStart], span );

	GX_RedirectWriteGatherPipe( &RDRAM[wgStart] );
	if (src)
	{
		const u8 *s = src + head;
		for (u32 n = span >> 3; n; --n, s += 8)
		{
			f64 quad;
			memcpy( &quad, s, 8 );
			wgPipe->F64 = quad;
		}
	}
	else
	{
		for (u32 n = span >> 2; n; --n)
			wgPipe->U32 = fill;
	}
	GX_RestoreWriteGatherPipe();

	DCInvalidateRange( &RDRAM[wgStart], span );

	if (src)
	{
		memcpy( &RDRAM[dst], src, head );
		memcpy( &RDRAM[wgEnd], src + head + span, end - wgEnd );
	}
	else
	{
		for (u32 a = dst; a < wgStart; a++)
			RDRAM[a] = _gDPFillByte( fill, a );
		for (u32 a = wgEnd; a < end; a++)
			RDRAM[a] = _gDPFillByte( fill, a );
	}
}

// GLideN64's texturedRectBGCopy(). Yoshi's Story draws some backgrounds as a texrect
// into an 8-bit colour image and then reads that image back as a texture, so copy the
// source texels straight into the colour image in RDRAM and skip the draw.
static BOOL _gDPTextureRectangleBGCopy( f32 ulx, f32 uly, f32 lrx, f32 lry, f32 s, f32 t,
                                        f32 dsdx, f32 dtdy )
{
	if (!OGL.frameBufferTextures)
		return FALSE;

	if (gDP.colorImage.size != G_IM_SIZ_8b)
		return FALSE;

	if (gDP.otherMode.cycleType == G_CYC_COPY)
		return FALSE;

	if (dsdx != 1.0f || dtdy != 1.0f)
		return FALSE;

	const f32 flry     = (lry > gDP.scissor.lry) ? gDP.scissor.lry : lry;
	const s32 texWidth = (s32)(gSP.textureTile[0]->line << 3);
	const s32 ciWidth  = (s32)gDP.colorImage.width;
	const u32 texAddr  = gDP.loadInfo[gSP.textureTile[0]->tmem & 0x1FF].texAddress;

	const s32 dstX0 = ((s32)ulx > 0) ? (s32)ulx : 0;
	const s32 dstX1 = ((s32)lrx < ciWidth) ? (s32)lrx : ciWidth;
	const s32 dstY0 = ((s32)uly > 0) ? (s32)uly : 0;
	const s32 dstY1 = (s32)flry;
	const s32 width = dstX1 - dstX0;
	const s32 rows  = dstY1 - dstY0;


	if (texAddr == 0 || texWidth <= 0 || ciWidth <= 0 || width <= 0 || rows <= 0)
	{
		return FALSE;
	}

	// Clipping moves the source start by the same amount.  s and t are signed.
	const s32 srcX = (s32)s + (dstX0 - (s32)ulx);
	const s32 srcY = (s32)t + (dstY0 - (s32)uly);

	const s32 srcBase = (s32)texAddr + srcY * texWidth + srcX;
	const s32 dstBase = (s32)gDP.colorImage.address + dstX0;

	// Check the whole extent up front; a partial copy leaves the rest of the image
	// holding the previous frame, which is worse than not copying at all.
	if (srcBase < 0 || dstBase < 0 ||
	    (u32)(srcBase + (rows - 1) * texWidth + width) > RDRAMSize ||
	    (u32)(dstBase + (dstY1 - 1) * ciWidth + width) > RDRAMSize)
	{
		return FALSE;
	}

	if (width == ciWidth && texWidth == ciWidth)
		_gDPWriteRDRAM( (u32)(dstBase + dstY0 * ciWidth), &RDRAM[srcBase], 0,
		                (u32)(rows * ciWidth) );
	else
		for (s32 y = 0; y < rows; y++)
			memcpy( &RDRAM[dstBase + (dstY0 + y) * ciWidth],
			        &RDRAM[srcBase + y * texWidth], (size_t)width );

	FrameBuffer_InvalidateBuffer( gDP.colorImage.address );
	return TRUE;
}

#ifdef __GX__
// GLideN64 does hack_rectDepthBufferCopyPD with texturedRectDepthBufferCopy(),
// which copies the whole depth buffer back to RDRAM.
//
// We can do similar via GX_PeekZ.
extern "C" int getVmodeAA();

static u32 _gDPDecodeZ16Mid( u16 z16 )
{
	const u32 e     = z16 >> 13;
	const u32 m     = z16 & 0x1FFF;
	const u32 shift = (e < 7) ? 10 - e : 4;
	const u32 base  = 0x1000000 - (0x1000000 >> e);

	return base + (m << shift) + ((1u << shift) >> 1);
}

// [Xbox360] 512, no 128: ahora se recuerda cada texel del texrect (4 en Perfect
// Dark) y el log 6 da 32 texrects por frame, o sea 128 justos.
#define GDP_MAX_ARTIFACT_SAMPLES 512

static struct
{
	u32 rowAddress;	// the z buffer row the game read - write back relative to this
	u16 x, y;		// screen pixel it sampled
} gDPArtifactSamples[GDP_MAX_ARTIFACT_SAMPLES];

static u32 gDPArtifactSampleCount = 0;
static BOOL gDPArtifactSynced = FALSE;

// The N64's depth word: 3 bit exponent, 11 bit mantissa, 2 dz bits, we need to encode to it.
// I think this covers it enough for PD.
static u16 _gDPEncodeN64Depth( f32 ndc01 )
{
	if (ndc01 < 0.0f) ndc01 = 0.0f;
	if (ndc01 > 1.0f) ndc01 = 1.0f;

	const u32 value = (u32)(ndc01 * 261632.0f);
	u32 exponent, mantissa;

	if      (value > 0x3F800) { exponent = 7; mantissa = value;      }
	else if (value > 0x3F000) { exponent = 6; mantissa = value;      }
	else if (value > 0x3E000) { exponent = 5; mantissa = value >> 1; }
	else if (value > 0x3C000) { exponent = 4; mantissa = value >> 2; }
	else if (value > 0x38000) { exponent = 3; mantissa = value >> 3; }
	else if (value > 0x30000) { exponent = 2; mantissa = value >> 4; }
	else if (value > 0x20000) { exponent = 1; mantissa = value >> 5; }
	else                      { exponent = 0; mantissa = value >> 6; }

	return (u16)((exponent << 13) | ((mantissa & 0x7FF) << 2));
}

/* [Xbox360] [Perfect Dark, coronas] La Z que lee la CPU.  Con la "foto" se
 * queda en la copia, como todo lo que escribe el plugin (ver _gDPWriteRDRAM,
 * Cruis'n USA): Perfect Dark necesita "en el acto" o "activado", no "foto". */
static void _gDPArtifactEscribe( u32 address, u16 value )
{
	if ((address + 1) >= RDRAMSize)
		return;
	*(u16*)&RDRAM[address] = value;
}

static u16 _gDPPeekDepth( u16 x, u16 y )
{
	const u16 efbX = (u16)(OGL.GXorigX + (f32)x * OGL.GXscaleX);
	const u16 efbY = (u16)(OGL.GXorigY + (f32)y * OGL.GXscaleY);
	u32 z = 0;

	GX_PeekZ( efbX, efbY, &z );

	if (getVmodeAA())
		z = _gDPDecodeZ16Mid( (u16)(z & 0xFFFF) );

	/* [Xbox360] ★ [Perfect Dark, coronas: no salia ninguna] EL PLANO LEJANO ES
	 * LA PALABRA DE BORRADO, como en la copia de Z de Wii64 (DepthCopy.cpp,
	 * _dcBuildLut: "A bucket that reaches the far plane is a clear ... It has to
	 * come out as the clear word exactly").  Aqui no: el codificador de abajo
	 * convierte 1.0 en F800, que para el juego es una Z por DELANTE de lo
	 * borrado.  Medido con la sonda [PDCORONA] (log 4): las 1920 lecturas/s
	 * recibian F800 y en la RDRAM habia FFFC.  Y GX_PeekZ en Xenos es un stub
	 * que siempre devuelve el lejano (xenon/gx_d3d9.cpp), asi que hoy TODAS
	 * las lecturas son esto.  (Ahora GX_PeekZ lee la Z de verdad; esto queda
 * para cuando no hay copia todavia.) */
	if (z >= 0x00FFFFFF)
		return (u16)(DepthClearColor >> 16);

	// Undo the viewport depth range the scene was drawn with
	f32 ndc01 = (f32)z / 16777215.0f;
	const f32 nearz = gSP.viewport.nearz;
	const f32 farz  = gSP.viewport.farz;

	if (farz != nearz)
		ndc01 = (ndc01 - nearz) / (farz - nearz);

	const u16 encoded = _gDPEncodeN64Depth( ndc01 );

	return encoded;
}

// TRUE means it was handled and nothing should be drawn
static BOOL _gDPArtifactDepthCopy( f32 ulx, f32 uly, f32 lrx, f32 lry, f32 s )
{
	if ((config.generalEmulation.hacks & hack_rectDepthBufferCopyPD) == 0)
		return FALSE;

	if (gDP.otherMode.cycleType != G_CYC_COPY || uly != 0.0f || (lry - uly) > 1.0f)
		return FALSE;

	if (gDP.textureImage.size != G_IM_SIZ_16b)
		return FALSE;

	if (gDP.loadInfo[gSP.textureTile[0]->tmem & 0x1FF].loadType != LOADTYPE_BLOCK)
		return FALSE;

	// What is being read has to be a whole row of the current depth image.
	const u32 stride = (u32)VI.width << 1;

	if (stride == 0 || gDP.depthImageAddress == 0 ||
	    gDP.textureImage.address < gDP.depthImageAddress)
		return FALSE;

	const u32 rowOffset = gDP.textureImage.address - gDP.depthImageAddress;

	if (rowOffset >= stride * (u32)VI.height || (rowOffset % stride) != 0)
		return FALSE;

	const u32 x = (u32)s;
	const u32 y = rowOffset / stride;
	const u32 dstIndex = (u32)ulx;

	if (x >= (u32)VI.width || dstIndex > 0x0FFF)
		return FALSE;

	if (!gDPArtifactSynced)
	{
		GX_DrawDone();
		gDPArtifactSynced = TRUE;

		/* [Xbox360] ★ [Perfect Dark, coronas] La copia de la Z del EFB que
		 * leera GX_PeekZ: aqui la escena ya esta dibujada y su Z todavia no se
		 * ha borrado para el arma (n64decomp/perfect_dark, artifact.c).  Sin
		 * esperas: este frame lee la copia terminada mas nueva, de 1-3 frames
		 * antes.  Ver "GX_PeekZ" en xenon/gx_d3d9.cpp.  Confirmado en el log
		 * 11: las coronas salen. */
		GXD3D_PreparaPeekZ( (u16)OGL.GXorigX, (u16)OGL.GXorigY,
		                    (u16)((f32)VI.width * OGL.GXscaleX + 0.5f),
		                    (u16)((f32)VI.height * OGL.GXscaleY + 0.5f) );
	}

	/* [Xbox360] ★ [Perfect Dark, coronas] TODA LA ANCHURA DEL TEXRECT, como
	 * GLideN64 (texturedRectDepthBufferCopy, GraphicsDrawer.cpp:1239-1244):
	 *     width = lrx - ulx;   // lrx ya lleva el +1 de COPY (gDP.cpp:995)
	 *     for (x = 0; x < width; ++x) pDst[ulx + x] = pSrc[s/32 + x];
	 * Wii64 escribia UN texel ("One texel is enough").  Medido con la sonda
	 * [PDCORONA] (log 6): todos los texrects miden lrx - ulx = 3, o sea 4
	 * texels, y en el buffer de las coronas quedaba FFFC en 0..31 y CERO a
	 * partir del 32: lo que el juego lee de ahi es "algo pegado a la camara" y
	 * da la corona por tapada.  Aqui lrx todavia NO lleva el +1 (se suma mas
	 * abajo en gDPTextureRectangle). */
	u32 n = (lrx > ulx) ? (u32)(lrx - ulx) + 1u : 1u;
	if (n > 16)
		n = 16;
	for (u32 i = 0; i < n; i++)
	{
		if (x + i >= (u32)VI.width || dstIndex + i > 0x0FFF)
			break;

		const u16 depth = _gDPPeekDepth( (u16)(x + i), (u16)y );

		const u32 dstAddress = gDP.colorImage.address + ((dstIndex + i) << 1);

		_gDPArtifactEscribe( dstAddress, depth );

		// Remembered so _gDPResolveArtifactDepths() can answer the second read.
		if (gDPArtifactSampleCount < GDP_MAX_ARTIFACT_SAMPLES)
		{
			gDPArtifactSamples[gDPArtifactSampleCount].rowAddress = gDP.textureImage.address;
			gDPArtifactSamples[gDPArtifactSampleCount].x = (u16)(x + i);
			gDPArtifactSamples[gDPArtifactSampleCount].y = (u16)y;
			++gDPArtifactSampleCount;
		}
	}

	return TRUE;
}

// The second of the two reads
static void _gDPResolveArtifactDepths()
{
	if (gDPArtifactSampleCount == 0)
		return;

	GX_DrawDone();

	for (u32 i = 0; i < gDPArtifactSampleCount; i++)
	{
		// rowAddress is the row the game itself read, so this lands on its
		// artifact->zbufptr whether or not the depth image and g_ZbufPtr1 agree.
		const u32 address = gDPArtifactSamples[i].rowAddress + ((u32)gDPArtifactSamples[i].x << 1);

		_gDPArtifactEscribe( address, _gDPPeekDepth( gDPArtifactSamples[i].x, gDPArtifactSamples[i].y ) );
	}

	gDPArtifactSampleCount = 0;
}
#endif // __GX__

#ifdef __GX__
/* [Xbox360] ★ [Paper Mario / Mario Story] texturedRectPaletteMod DE GLideN64
 * (GraphicsDrawer.cpp:1289-1327; c49df83a, ce0fc1f0, 2cc29816), TAL CUAL.
 * El juego prepara con texrects cosas que la CPU lee de la RDRAM, y que
 * nosotros dibujabamos en la GPU, donde no las ve nadie:
 *   - el "alumbrado 2D" (las velas del castillo): un texrect de 16x1 con un
 *     scissor de 16x1 cuyo resultado es una PALETA de 16 colores; cada
 *     entrada sale del color PRIM o del ENV segun el bit 0x100 de la entrada
 *     de la TLUT cargada en la TMEM;
 *   - la textura de "oscuridad" en 0x400: GLideN64 lo llama "dirty hacks" --
 *     copia 4096 bytes de RDRAM+0x14d500 a 0x400 en vez del dibujo de 64 de
 *     ancho sobre si mismo, y la textura de 64 de ancho de 0x400 se lee como
 *     textura normal (de la TMEM), no del framebuffer.
 * Diferencias de plataforma, nada mas: GLideN64 escribe `dst[i ^ 1]` porque
 * su RDRAM esta en palabras de 32 bits cambiadas (anfitrion little-endian);
 * aqui la RDRAM va en el orden del N64.  La entrada `i` de la TLUT es
 * `*(u16*)&TMEM[256 + i]` (gDPLoadTLUT la guarda en los 16 bits altos de cada
 * palabra de 64; es su `src[i << 2]`).  ★ Y SU `& 0x100` ES NUESTRO `& 1`:
 * GLideN64 guarda la TLUT en la TMEM con los bytes CAMBIADOS (`swapword` en
 * su gDP.cpp:753; PaletteTexture.cpp:91 la deshace al leer), asi que su bit 8
 * es el bit 0 del valor del N64, el de alfa del 5551.  Aqui la TMEM lleva el
 * valor tal cual: con 0x100 se miraba un bit de VERDE (Paper Mario: guantes
 * de Peach sin luz, mancha roja en la mano de Mario).  Escribe con
 * _gDPArtifactEscribe (con "foto" se queda en la copia, ver _gDPWriteRDRAM).
 * Solo en las ordenes de texrect, como su `_params.texrectCmd`.  TRUE = no se
 * dibuja. */
static BOOL _gDPTexrectPaletteMod( f32 lrx, f32 lry )
{
	if ((config.generalEmulation.hacks & hack_texrectPaletteMod) == 0)
		return FALSE;
	if (RSP.cmd != G_TEXRECT && RSP.cmd != G_TEXRECTFLIP)
		return FALSE;

	if (gDP.textureImage.address == 0x400)
	{
		if (gDP.colorImage.address == 0x400 && gDP.colorImage.width == 64)
		{
			if (0x14D500 + 4096 - 1 <= RDRAMSize)
			{
				memcpy( &RDRAM[0x400], &RDRAM[0x14D500], 4096 );
			}
			return TRUE;
		}

		if (gDP.textureImage.width == 64)
		{
			// GLideN64: curTile.frameBufferAddress = 0; textureMode = NORMAL;
			// textureCache().update(0); currentCombiner()->update(false);
			gDP.tiles[0].frameBufferAddress = 0;
			gDP.tiles[0].textureMode = TEXTUREMODE_NORMAL;
			gDP.changed |= CHANGED_TMEM | CHANGED_COMBINE;
		}
		return FALSE;
	}

	// Modify palette for Paper Mario "2D lighting" effect
	if (gDP.scissor.lrx != 16.0f || gDP.scissor.lry != 1.0f || lrx != 16.0f || lry != 1.0f)
		return FALSE;

	const u8 envr = (u8)(gDP.envColor.r * 31.0f);
	const u8 envg = (u8)(gDP.envColor.g * 31.0f);
	const u8 envb = (u8)(gDP.envColor.b * 31.0f);
	const u16 env16 = (u16)((envr << 11) | (envg << 6) | (envb << 1) | 1);
	const u8 prmr = (u8)(gDP.primColor.r * 31.0f);
	const u8 prmg = (u8)(gDP.primColor.g * 31.0f);
	const u8 prmb = (u8)(gDP.primColor.b * 31.0f);
	const u16 prim16 = (u16)((prmr << 11) | (prmg << 6) | (prmb << 1) | 1);
	for (u32 i = 0; i < 16; ++i)
	{
		const u16 entrada = *(const u16*)&TMEM[256 + i];
		_gDPArtifactEscribe( gDP.colorImage.address + (i << 1), (entrada & 0x0001) ? prim16 : env16 );
	}
	return TRUE;
}

/* [Xbox360] ★ [Beetle Adventure Racing, la niebla] texturedRectShadowMap DE
 * GLideN64 (GraphicsDrawer.cpp:1189-1208), con su shader ShadowMapShader
 * (glsl_SpecialShadersFactory.cpp:59-134 y 618-664).  El juego hace la niebla
 * con un texrect que lee el DEPTH IMAGE como textura de 16 bits y la TLUT
 * cargada en la TMEM como curva: en el N64 cada pixel sale del color de
 * niebla con alfa = TLUT[byte alto de su Z].  Aqui (como en GLideN64) la
 * RDRAM no tiene ese Z al dia, asi que el texrect se dibuja con un shader
 * especial del shim (GXD3D_SetNieblaZ): lee el Z del EFB en su pixel, lo pasa
 * al formato del N64 (la zlut de GLideN64, DepthBuffer.cpp:353-367), y saca
 * fogColor con alfa = TLUT[indice] >> 8, mezclado SRCALPHA / INVSRCALPHA
 * (el blend lo pone OGL_UpdateStates con gln64_nieblaZ).
 * Las condiciones, las suyas: hay framebuffer (su pCurrentBuffer) y la
 * textura es de 16 bits y empieza dentro del depth image, con su mismo limite
 * `ancho * ancho * 6 / 4`.  Solo en las ordenes de texrect (su texrectCmd).
 * La TLUT es `*(u16*)&TMEM[256 + i]`, el valor del N64 tal cual (su
 * PaletteTexture deshace el swapword: ver _gDPTexrectPaletteMod). */
BOOL gln64_nieblaZ = FALSE;

static BOOL _gDPTexrectNieblaZ( void )
{
	if ((config.generalEmulation.hacks & hack_texrectShadowMap) == 0)
		return FALSE;
	if (RSP.cmd != G_TEXRECT && RSP.cmd != G_TEXRECTFLIP)
		return FALSE;
	if (!OGL.frameBufferTextures)
		return FALSE;
	return (gDP.textureImage.size == G_IM_SIZ_16b &&
	        gDP.textureImage.address >= gDP.depthImageAddress &&
	        gDP.textureImage.address < (gDP.depthImageAddress +
	                                    gDP.colorImage.width * gDP.colorImage.width * 6 / 4)) ? TRUE : FALSE;
}
#endif // __GX__

void gDPTextureRectangle( f32 ulx, f32 uly, f32 lrx, f32 lry, s32 tile, f32 s, f32 t, f32 dsdx, f32 dtdy, const f32 *colorOverride )
{
	if (_gDPArtifactDepthCopy( ulx, uly, lrx, lry, s ))
		return;

 	if (gDP.otherMode.cycleType == G_CYC_COPY)
	{
		dsdx = 1.0f;
		lrx += 1.0f;
		lry += 1.0f;
	}
	else if (lry - uly < 1.0f)
	{
		lry = ceilf( lry );
	}

	// Save and restore the pointers rather than recomputing them: gSPUpdateTextureTiles()
	// can shift the base tile (G_TL_LOD + G_TD_DETAIL), and a texrect must not be the
	// thing that decides where they point once it is done.
	gDPTile *textureTileOrg[2];
	textureTileOrg[0] = gSP.textureTile[0];
	textureTileOrg[1] = gSP.textureTile[1];
	gSP.textureTile[0] = &gDP.tiles[tile];
	gSP.textureTile[1] = needReplaceTex1ByTex0() ? &gDP.tiles[tile] : &gDP.tiles[(tile + 1) & 7];

	/* [Xbox360] ★ [Conker, sombra cuadrada] `texturedRectCopyToItself` de
	 * GLideN64 (GraphicsDrawer.cpp:1252), tal cual: si el color image activo es
	 * de 8 bits y la textura del rect sale de ESE MISMO framebuffer, el juego
	 * esta copiando el buffer sobre si mismo (el difuminado de la silueta de la
	 * sombra) y el rect NO se dibuja.  Dibujado, rellena el buffer de la silueta
	 * y la sombra sale como un cuadrado.  Solo con el hack, o sea solo Conker.
	 * Medido (log 32): 60 saltados por segundo, dos por frame, y la sombra sale
	 * con su forma; el A/B y la sonda ya se retiraron. */
	if ((config.generalEmulation.hacks & hack_texrectCopyToItself) != 0 &&
	    gDP.colorImage.size == G_IM_SIZ_8b &&
	    gSP.textureTile[0]->frameBufferAddress == gDP.colorImage.address)
	{
		gSP.textureTile[0] = textureTileOrg[0];
		gSP.textureTile[1] = textureTileOrg[1];
		return;
	}

#ifdef __GX__
	/* [Paper Mario] El otro texrect especial de GLideN64, con lrx/lry ya
	 * ajustados como los suyos (el +1 de COPY va arriba). */
	if (_gDPTexrectPaletteMod( lrx, lry ))
	{
		gSP.textureTile[0] = textureTileOrg[0];
		gSP.textureTile[1] = textureTileOrg[1];
		return;
	}
#endif // __GX__

	/* [Xbox360] [Mario Golf] Si es la copia de un Z guardado al Z.  Ver
	 * _gDPCopiaDeZGuardada; lo lee OGL_UpdateStates durante este dibujo. */
	gln64_zCopiaDesde = (gDP.colorImage.address == gDP.depthImageAddress)
	                  ? _gDPCopiaDeZGuardada( ulx, uly, s, t, dsdx, dtdy ) : 0;
	if (gln64_zCopiaDesde != 0)
		gDP.changed |= CHANGED_RENDERMODE;

#ifdef __GX__
	/* [Beetle Adventure Racing] La niebla por Z: ver _gDPTexrectNieblaZ.  La
	 * curva y el color se le dan al shim ANTES del dibujo (vacia lo pendiente
	 * con su modo), y el blend lo fuerza OGL_UpdateStates. */
	if (_gDPTexrectNieblaZ())
	{
		u8 alfa[256];
		GXColor color;
		u32 i;
		for (i = 0; i < 256; i++)
			alfa[i] = (u8)((*(const u16*)&TMEM[256 + i]) >> 8);
		color.r = GXcastf32u8( gDP.fogColor.r );
		color.g = GXcastf32u8( gDP.fogColor.g );
		color.b = GXcastf32u8( gDP.fogColor.b );
		color.a = 0xFF;
		/* El Z del EFB va referido al viewport: el mismo paso al del N64 que
		 * DepthCopy (DepthCopy_EndDList: nearz/farz y la palabra de borrado). */
		gln64_nieblaZ = GXD3D_SetNieblaZ( 1, alfa, color, gSP.viewport.nearz, gSP.viewport.farz,
		                                  (u16)(DepthClearColor >> 16) ) ? TRUE : FALSE;
		if (gln64_nieblaZ)
			gDP.changed |= CHANGED_RENDERMODE;
	}
#endif // __GX__

	f32 lrs = s + (lrx - ulx - 1) * dsdx;
	f32 lrt = t + (lry - uly - 1) * dtdy;

	if (gDP.textureMode == TEXTUREMODE_NORMAL)
		gDP.textureMode = TEXTUREMODE_TEXRECT;

	f32 texRectWidthF = MAX( lrs, s ) + dsdx;
	f32 texRectHeightF = MAX( lrt, t ) + dtdy;
	gDP.texRect.width = (texRectWidthF > 0.0f) ? (unsigned long)texRectWidthF : 0;
	gDP.texRect.height = (texRectHeightF > 0.0f) ? (unsigned long)texRectHeightF : 0;
	gDP.texRect.dsdx = (dsdx < 0.0f) ? -dsdx : dsdx;
	gDP.texRect.dtdy = (dtdy < 0.0f) ? -dtdy : dtdy;

	BOOL blitted = FALSE;
	if ((RSP.cmd == G_TEXRECT || RSP.cmd == G_TEXRECTFLIP) &&
	    GetGameSpecificHack() == &hack_yoshistory)
		blitted = _gDPTextureRectangleBGCopy( ulx, uly, lrx, lry, s, t, dsdx, dtdy );

	if (blitted)
	{
		// The rect went straight into RDRAM above.
	}
	else if (lrs > s)
	{
		if (lrt > t)
			OGL_DrawTexturedRect( ulx, uly, lrx, lry, s, t, lrs, lrt, (RSP.cmd == G_TEXRECTFLIP), colorOverride );
		else
			OGL_DrawTexturedRect( ulx, lry, lrx, uly, s, lrt, lrs, t, (RSP.cmd == G_TEXRECTFLIP), colorOverride );
	}
	else
	{
		if (lrt > t)
			OGL_DrawTexturedRect( lrx, uly, ulx, lry, lrs, t, s, lrt, (RSP.cmd == G_TEXRECTFLIP), colorOverride );
		else
			OGL_DrawTexturedRect( lrx, lry, ulx, uly, lrs, lrt, s, t, (RSP.cmd == G_TEXRECTFLIP), colorOverride );
	}

	if (gln64_zCopiaDesde != 0)
	{
		gln64_zCopiaDesde = 0;               // el siguiente dibujo, normal
		gDP.changed |= CHANGED_RENDERMODE;
	}
#ifdef __GX__
	if (gln64_nieblaZ)
	{
		GXColor sinColor;
		sinColor.r = sinColor.g = sinColor.b = sinColor.a = 0;
		gln64_nieblaZ = FALSE;               // el siguiente dibujo, normal
		GXD3D_SetNieblaZ( 0, NULL, sinColor, 0.0f, 1.0f, 0 );
		gDP.changed |= CHANGED_RENDERMODE;
	}
#endif // __GX__

	gSP.textureTile[0] = textureTileOrg[0];
	gSP.textureTile[1] = textureTileOrg[1];

	if (depthBuffer.current) depthBuffer.current->cleared = FALSE;
	/* [Xbox360] ★★ [Mario Tennis, bob-ombs] LA ALTURA CRECE HASTA LO DIBUJADO,
	 * NO HASTA EL RECORTE, como GLideN64 (gDP.cpp:1049 `setBufferChanged(lry)`
	 * + FrameBuffer.cpp:662-663: max con lo dibujado, min con el recorte).
	 * Con `scissor.lry` un buffer auxiliar de 32 de ancho donde solo se dibujan
	 * unas filas arriba contaba como 32x224 (medido con [AUXDIB], log 18:
	 * recorte 0,0-384,224), y la lectura en I4 de las bob-ombs desde la fila
	 * 130 (`+8320`) caia DENTRO del buffer y se servia lo que hubiera en el EFB
	 * ahi -- opaco: el cuadrado negro --.  En la RDRAM esas filas estan a cero
	 * ([FBACT-RAM], log 16), que leido como I4 es transparente. */
	gDPBufferChanged( (lry < gDP.scissor.lry) ? lry : gDP.scissor.lry );

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPTextureRectangle( %f, %f, %f, %f, %i, %i, %f, %f, %f, %f );\n",
		ulx, uly, lrx, lry, tile, s, t, dsdx, dtdy );
#endif
}

/* [Xbox360] ★★ [Killer Instinct, el cielo de los combates] LOS TRIANGULOS DE BAJO
 * NIVEL DEL RDP (ordenes 0x08-0x0F de las listas mandadas directamente por DPC).
 * GLideN64 `LLETriangle::draw` (src/gDP.cpp:1179-1536), portado tal cual: de los
 * datos del recorredor de aristas (yh/ym/yl, xh/xm/xl y pendientes) saca un
 * abanico de hasta 8 vertices en coordenadas de PANTALLA, con color, textura y
 * profundidad, y los dibuja como triangulos de pantalla.
 *
 * En KI Gold los cielos llegan asi: lo dice gonetz en GLideN64 #406 ("RDP_Tri-
 * ShadeTxtr command 0x0E is called from that dlist... pure LLE issue").  glN64
 * no tenia estas ordenes y el cielo no se pintaba nunca: la estela.  GLideN64
 * reconoce que su version no es perfecta para esos cielos; aun asi es la
 * referencia que hay.
 *
 * Diferencias:
 *   - Se dibuja cada triangulo en el acto, sin el lote de LLETriangle (que solo
 *     ahorra llamadas); por eso start()/flush() van juntos aqui dentro.
 *   - Sin shade, GLideN64 deja el color sin inicializar (std::array de POD);
 *     aqui va a blanco, el neutro del combinador.
 *   - La z se calcula como en la referencia, pero el camino de dibujo de
 *     pantalla de glN64 (el de ZSortBOSS) usa una profundidad fija.  KI no tiene
 *     depth image (zi=0). */
#define LLE_SIGN( x, numb )	(((x) & ((1 << (numb)) - 1)) | -((x) & (1 << ((numb) - 1))))

struct LLEInterp
{
	BOOL shade, texture, zbuffer;
	f32 rf, gf, bf, af, wf, zf, sf, tf;
	f32 drdef, dgdef, dbdef, dadef, dwdef, dzdef, dsdef, dtdef;
	f32 drdxf, dgdxf, dbdxf, dadxf, dwdxf, dzdxf, dsdxf, dtdxf;
};

static f32 _lleColorScale( int c )
{
	const int v = (c > 0x3ff0000) ? 0x3ff0000 : ((c < 0) ? 0 : c);
	return (f32)(v >> 18) / 255.0f;
}

static f32 _lleColorClamp( f32 c )
{
	return (c < 0.0f) ? 0.0f : ((c > 1.0f) ? 1.0f : c);
}

static void _lleUpdateVtx( const LLEInterp &p, SPVertex *vtx, f32 diffY, f32 diffx )
{
	if (p.shade)
	{
		vtx->r = _lleColorClamp( p.rf + p.drdef * diffY + p.drdxf * diffx );
		vtx->g = _lleColorClamp( p.gf + p.dgdef * diffY + p.dgdxf * diffx );
		vtx->b = _lleColorClamp( p.bf + p.dbdef * diffY + p.dbdxf * diffx );
		vtx->a = _lleColorClamp( p.af + p.dadef * diffY + p.dadxf * diffx );
	}

	if (p.zbuffer)
		vtx->z = (gDP.otherMode.depthSource == G_ZS_PRIM) ?
		         gDP.primDepth.z :
		         (p.zf + p.dzdef * diffY + p.dzdxf * diffx * 4.0f) * 2.0f;
	else
		vtx->z = 0.0f;

	if (p.texture)
	{
		if (gDP.otherMode.texturePersp != 0)
		{
			const f32 vw = p.wf + p.dwdef * diffY + p.dwdxf * diffx * 4.0f;
			vtx->w = 1.0f / (vw > 0.0f ? vw : (1.0f + vw - ceilf( vw )));
			if (vw <= 0.0f)
			{
				// TODO fix with proper coords (GLideN64)
				vtx->s = (f32)(1 << gSP.textureTile[0]->masks);
				vtx->t = (f32)(1 << gSP.textureTile[0]->maskt);
			}
			else
			{
				vtx->s = (p.sf + p.dsdef * diffY + p.dsdxf * diffx) / vw * 0.0625f;
				vtx->t = (p.tf + p.dtdef * diffY + p.dtdxf * diffx) / vw * 0.0625f;
			}
		}
		else
		{
			vtx->w = 1.0f;
			vtx->s = (p.sf + p.dsdef * diffY + p.dsdxf * diffx) * 0.125f;
			vtx->t = (p.tf + p.dtdef * diffY + p.dtdxf * diffx) * 0.125f;
		}
	}
	else
		vtx->w = 1.0f;
}

void gDPLLETriangle( BOOL _shade, BOOL _texture, BOOL _zbuffer, const u32 *_pData )
{
	const s32 *pDataSigned = (const s32*)_pData;
	LLEInterp p;
	SPVertex vertices[8];
	u32 vtxCount = 0;
	u32 i;

	// LLETriangle::start()
	gDPTile *textureTileOrg[2];
	textureTileOrg[0] = gSP.textureTile[0];
	textureTileOrg[1] = gSP.textureTile[1];
	const f32 scalesOrg = gSP.texture.scales;
	const f32 scaletOrg = gSP.texture.scalet;

	gSP.texture.level = _SHIFTR( _pData[0], 19, 3 );
	const u32 tile = _SHIFTR( _pData[0], 16, 3 );
	gSP.texture.tile = tile;
	gSP.textureTile[0] = &gDP.tiles[tile];
	gSP.textureTile[1] = needReplaceTex1ByTex0() ? &gDP.tiles[tile] : &gDP.tiles[(tile + 1) & 7];
	gSP.texture.scales = 1.0f;
	gSP.texture.scalet = 1.0f;

	int yl = LLE_SIGN( pDataSigned[0], 14 );
	int ym = pDataSigned[1] >> 16;
	ym = LLE_SIGN( ym, 14 );
	int yh = LLE_SIGN( pDataSigned[1], 14 );

	const int xl = LLE_SIGN( pDataSigned[2], 28 );
	const int xh = LLE_SIGN( pDataSigned[4], 28 );
	const int xm = LLE_SIGN( pDataSigned[6], 28 );

	const int dxldy = LLE_SIGN( pDataSigned[3], 30 );
	const int dxhdy = LLE_SIGN( pDataSigned[5], 30 );
	const int dxmdy = LLE_SIGN( pDataSigned[7], 30 );

	yh &= ~3;

	int r = 0xff, g = 0xff, b = 0xff, a = 0xff;
	int drdx = 0, dgdx = 0, dbdx = 0, dadx = 0;
	int drde = 0, dgde = 0, dbde = 0, dade = 0;

	if (_shade)
	{
		r    = (int)((_pData[8] & 0xffff0000) | ((_pData[12] >> 16) & 0x0000ffff));
		g    = (int)(((_pData[8] << 16) & 0xffff0000) | (_pData[12] & 0x0000ffff));
		b    = (int)((_pData[9] & 0xffff0000) | ((_pData[13] >> 16) & 0x0000ffff));
		a    = (int)(((_pData[9] << 16) & 0xffff0000) | (_pData[13] & 0x0000ffff));
		drdx = (int)((_pData[10] & 0xffff0000) | ((_pData[14] >> 16) & 0x0000ffff));
		dgdx = (int)(((_pData[10] << 16) & 0xffff0000) | (_pData[14] & 0x0000ffff));
		dbdx = (int)((_pData[11] & 0xffff0000) | ((_pData[15] >> 16) & 0x0000ffff));
		dadx = (int)(((_pData[11] << 16) & 0xffff0000) | (_pData[15] & 0x0000ffff));
		drde = (int)((_pData[16] & 0xffff0000) | ((_pData[20] >> 16) & 0x0000ffff));
		dgde = (int)(((_pData[16] << 16) & 0xffff0000) | (_pData[20] & 0x0000ffff));
		dbde = (int)((_pData[17] & 0xffff0000) | ((_pData[21] >> 16) & 0x0000ffff));
		dade = (int)(((_pData[17] << 16) & 0xffff0000) | (_pData[21] & 0x0000ffff));
	}

	int s = 0, t = 0, w = 0x30000;
	int dsdx = 0, dtdx = 0, dwdx = 0;
	int dsde = 0, dtde = 0, dwde = 0;
	if (_texture)
	{
		s    = (int)((_pData[24] & 0xffff0000) | ((_pData[28] >> 16) & 0x0000ffff));
		t    = (int)(((_pData[24] << 16) & 0xffff0000) | (_pData[28] & 0x0000ffff));
		w    = (int)((_pData[25] & 0xffff0000) | ((_pData[29] >> 16) & 0x0000ffff));
		dsdx = (int)((_pData[26] & 0xffff0000) | ((_pData[30] >> 16) & 0x0000ffff));
		dtdx = (int)(((_pData[26] << 16) & 0xffff0000) | (_pData[30] & 0x0000ffff));
		dwdx = (int)((_pData[27] & 0xffff0000) | ((_pData[31] >> 16) & 0x0000ffff));
		dsde = (int)((_pData[32] & 0xffff0000) | ((_pData[36] >> 16) & 0x0000ffff));
		dtde = (int)(((_pData[32] << 16) & 0xffff0000) | (_pData[36] & 0x0000ffff));
		dwde = (int)((_pData[33] & 0xffff0000) | ((_pData[37] >> 16) & 0x0000ffff));
	}

	int z = (int)0xffff0000;
	int dzdx = 0, dzde = 0;
	if (_zbuffer)
	{
		z    = pDataSigned[40];
		dzdx = pDataSigned[41];
		dzde = pDataSigned[42];
	}

	p.shade = _shade;  p.texture = _texture;  p.zbuffer = _zbuffer;
	p.rf = _lleColorScale( r << 2 );
	p.gf = _lleColorScale( g << 2 );
	p.bf = _lleColorScale( b << 2 );
	p.af = _lleColorScale( a << 2 );
	p.wf = (f32)w / (f32)0xffff0000u;
	p.zf = (f32)z / (f32)0xffff0000u;
	p.sf = (f32)s / (f32)(1 << 18);
	p.tf = (f32)t / (f32)(1 << 18);

	p.drdef = _FIXED2FLOAT( ((drde >> 2) & ~1), 16 ) / 255.0f;
	p.dgdef = _FIXED2FLOAT( ((dgde >> 2) & ~1), 16 ) / 255.0f;
	p.dbdef = _FIXED2FLOAT( ((dbde >> 2) & ~1), 16 ) / 255.0f;
	p.dadef = _FIXED2FLOAT( ((dade >> 2) & ~1), 16 ) / 255.0f;
	p.dwdef = (f32)(dwde >> 2) / (f32)0xffff0000u;
	p.dzdef = (f32)(dzde >> 2) / (f32)0xffff0000u;
	p.dsdef = (f32)(dsde >> 2) / (f32)(1 << 18);
	p.dtdef = (f32)(dtde >> 2) / (f32)(1 << 18);

	p.drdxf = _FIXED2FLOAT( drdx, 16 ) / 255.0f;
	p.dgdxf = _FIXED2FLOAT( dgdx, 16 ) / 255.0f;
	p.dbdxf = _FIXED2FLOAT( dbdx, 16 ) / 255.0f;
	p.dadxf = _FIXED2FLOAT( dadx, 16 ) / 255.0f;
	p.dwdxf = (f32)(dwdx >> 2) / (f32)0xffff0000u;
	p.dzdxf = (f32)(dzdx >> 2) / (f32)0xffff0000u;
	p.dsdxf = _FIXED2FLOAT( ((dsdx >> 2) & ~1), 16 );
	p.dtdxf = _FIXED2FLOAT( ((dtdx >> 2) & ~1), 16 );

	const f32 xhf = _FIXED2FLOAT( (xh & ~0x1), 16 );
	const f32 xmf = _FIXED2FLOAT( (xm & ~0x1), 16 );
	const f32 yhf = (f32)yh;
	const f32 ymf = (f32)ym;
	const f32 ylf = (f32)yl;
	const f32 hk = _FIXED2FLOAT( ((dxhdy >> 2) & ~0x1), 16 );
	const f32 mk = _FIXED2FLOAT( ((dxmdy >> 2) & ~0x1), 16 );
	const f32 hc = xhf - hk * yhf;
	const f32 mc = xmf - mk * yhf;

	// Sin shade, el color es el neutro: ver la nota de arriba.
	for (i = 0; i < 8; i++)
	{
		memset( &vertices[i], 0, sizeof( SPVertex ) );
		vertices[i].r = vertices[i].g = vertices[i].b = vertices[i].a = 1.0f;
		vertices[i].w = 1.0f;
	}

	SPVertex *vtx;
	if (fabsf( hk - mk ) < 0.00000001f)
	{
		vtx = &vertices[vtxCount++];
		vtx->x = hk * yhf + hc;
		vtx->y = yhf * 0.25f;
		_lleUpdateVtx( p, vtx, 0.0f, 0.0f );

		if (mc != hc)
		{
			vtx = &vertices[vtxCount++];
			vtx->x = mk * yhf + mc;
			vtx->y = yhf * 0.25f;
			_lleUpdateVtx( p, vtx, 0.0f, (mc - hc) );
		}

		const f32 diffym = (ymf - yhf);
		const f32 xhym = (hk * ymf + hc);
		const f32 xmym = (mk * ymf + mc);
		const f32 diffxm = (xmym - xhym);

		f32 vw = p.wf + p.dwdef * diffym + p.dwdxf * diffxm * 4.0f;
		if (vw <= 0.0f)
		{
			f32 xhyf = xhym, xmyf = xmym, diffyf = diffym, diffxf = diffxm;
			f32 yf = ymf;
			do {
				yf -= 1.0f;
				diffyf = (yf - yhf);
				xhyf = hk * yf + hc;
				xmyf = mk * yf + mc;
				diffxf = xmyf - xhyf;
				vw = p.wf + p.dwdef * diffyf + p.dwdxf * diffxf * 4.0f;
			} while (vw <= 0.0f && yf > yhf);

			vtx = &vertices[vtxCount++];
			vtx->x = xhyf;
			vtx->y = yf * 0.25f;
			_lleUpdateVtx( p, vtx, diffyf, 0.0f );

			vtx = &vertices[vtxCount++];
			vtx->x = xmyf;
			vtx->y = yf * 0.25f;
			_lleUpdateVtx( p, vtx, diffyf, diffxf );
		}

		vtx = &vertices[vtxCount++];
		vtx->x = xhym;
		vtx->y = ymf * 0.25f;
		_lleUpdateVtx( p, vtx, diffym, 0.0f );

		vtx = &vertices[vtxCount++];
		vtx->x = xmym;
		vtx->y = ymf * 0.25f;
		_lleUpdateVtx( p, vtx, diffym, diffxm );

		if (dxldy != dxmdy && ym < yl)
		{
			const f32 xlf = _FIXED2FLOAT( (xl & ~1), 16 );
			const f32 lk = _FIXED2FLOAT( ((dxldy >> 2) & ~1), 16 );
			const f32 lc = xlf - lk * ym;
			const f32 y4f = (lc - hc) / (hk - lk);
			vtx = &vertices[vtxCount++];
			vtx->x = hk * y4f + hc;
			vtx->y = y4f * 0.25f;
			_lleUpdateVtx( p, vtx, (y4f - yhf), 0.0f );
		}
	}
	else
	{
		const f32 y0f = (mc - hc) / (hk - mk);

		vtx = &vertices[vtxCount++];
		vtx->x = hk * y0f + hc;
		vtx->y = y0f * 0.25f;
		_lleUpdateVtx( p, vtx, (y0f - yhf), 0.0f );

		const f32 y1f = ymf;
		const f32 xlf = _FIXED2FLOAT( (xl & ~1), 16 );
		const f32 lk = _FIXED2FLOAT( ((dxldy >> 2) & ~1), 16 );
		const f32 lc = xlf - lk * y1f;

		vtx = &vertices[vtxCount++];
		vtx->x = xlf;
		vtx->y = y1f * 0.25f;

		const f32 x1f = hk * y1f + hc;
		const f32 diffx1 = xlf - x1f;
		const f32 diffy1 = (y1f - yhf);
		_lleUpdateVtx( p, vtx, diffy1, diffx1 );

		const f32 vw1 = p.wf + p.dwdef * diffy1 + p.dwdxf * diffx1 * 4.0f;
		if (vw1 <= 0.0f)
		{
			f32 y1_1f = y1f;
			f32 vw1_1 = vw1;
			f32 x1_1f = x1f;
			f32 x1_2f = xlf;
			f32 diffy1_1 = diffy1;
			f32 diffx1_1 = diffx1;
			do {
				y1_1f += 1.0f;
				diffy1_1 = (y1_1f - yhf);
				x1_1f = hk * y1_1f + hc;
				x1_2f = lk * y1_1f + lc;
				diffx1_1 = x1_2f - x1_1f;
				vw1_1 = p.wf + p.dwdef * diffy1_1 + p.dwdxf * diffx1_1 * 4.0f;
			} while (vw1_1 <= 0.0f && y1_1f < ylf);

			vtx = &vertices[vtxCount++];
			vtx->x = x1_1f;
			vtx->y = y1_1f * 0.25f;
			_lleUpdateVtx( p, vtx, diffy1_1, 0.0f );

			vtx = &vertices[vtxCount++];
			vtx->x = x1_2f;
			vtx->y = y1_1f * 0.25f;
			_lleUpdateVtx( p, vtx, diffy1_1, diffx1_1 );
		}

		if (hk == lk)
		{
			const f32 lrx = lk * ylf + lc;
			vtx = &vertices[vtxCount++];
			vtx->x = lrx;
			vtx->y = ylf * 0.25f - (vertices[1].y - vertices[0].y);
			f32 ydiff = (vtx->y * 4.0f - yhf);
			_lleUpdateVtx( p, vtx, ydiff, diffx1 );

			vtx = &vertices[vtxCount++];
			vtx->x = lrx;
			vtx->y = ylf * 0.25f;
			ydiff = (ylf - yhf);

			const f32 x2f = hk * ylf + hc;
			const f32 diffx2 = vtx->x - x2f;
			_lleUpdateVtx( p, vtx, ydiff, diffx2 );
		}
		else if (mk == lk)
		{
			vtx = &vertices[vtxCount++];
			vtx->x = hk * ylf + hc;
			vtx->y = ylf * 0.25f;
			_lleUpdateVtx( p, vtx, (ylf - yhf), 0.0f );
		}
		else
		{
			f32 y2f;
			if (yl == ym)
				y2f = (lc - mc) / (mk - lk);
			else
				y2f = (lc - hc) / (hk - lk);

			vtx = &vertices[vtxCount++];
			vtx->x = hk * y2f + hc;
			vtx->y = y2f * 0.25f;
			_lleUpdateVtx( p, vtx, (y2f - yhf), 0.0f );
		}
	}

	if (_texture)
	{
		gDP.changed |= CHANGED_TILE;
		gSP.changed |= CHANGED_TEXTURE;
	}
	if (_zbuffer)
	{
		gSP.geometryMode |= G_ZBUFFER;
		gSP.changed |= CHANGED_GEOMETRYMODE;
	}

	if (vtxCount >= 3)
		OGL_DrawScreenSpaceTriangles( vertices, vtxCount, _texture );

	// LLETriangle::flush()
	gSP.textureTile[0] = textureTileOrg[0];
	gSP.textureTile[1] = textureTileOrg[1];
	gSP.texture.scales = scalesOrg;
	gSP.texture.scalet = scaletOrg;
	gSP.changed |= CHANGED_TEXTURE;
	gDP.changed |= CHANGED_TILE;
}

void gDPTextureRectangleFlip( f32 ulx, f32 uly, f32 lrx, f32 lry, s32 tile, f32 s, f32 t, f32 dsdx, f32 dtdy )
{
	gDPTextureRectangle( ulx, uly, lrx, lry, tile, s + (lrx - ulx) * dsdx, t + (lry - uly) * dtdy, -dsdx, -dtdy );

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPTextureRectangleFlip( %f, %f, %f, %f, %i, %i, %f, %f, %f, %f );\n",
		ulx, uly, lrx, lry, tile, s, t, dsdx, dtdy );
#endif
}

// GLideN64's copyWhiteToRDRAM(). Zelda OoT reads the frame buffer back out of RDRAM
// and checks it before opening the subscreen; a real copy does not satisfy it, but a
// buffer full of white does, and without it the menu takes 7-10 seconds to appear
// (GLideN64 issue #327).
static void _gDPCopyWhiteToRDRAM()
{
	const u32 numBytes = (VI.width * VI.height) << 1;

	if (numBytes == 0 || (gDP.colorImage.address + numBytes) > RDRAMSize)
		return;

	/* [Xbox360] Wii64 2e1c6fb "Fix pictobox after depth changes": la nota
	 * ANTES de escribir el blanco ("Do this after to not break pictograph in
	 * ZeldaMM"): DepthCopy_NoteDepthImageWritten puede volcar ahi el borrado
	 * pendiente del Z, y el blanco tiene que quedar encima. */
	if (gDP.colorImage.address == gDP.depthImageAddress)
		DepthCopy_NoteDepthImageWritten();
	_gDPWriteRDRAM( gDP.colorImage.address, NULL, 0xFFFFFFFF, numBytes );

	FrameBuffer_RestampMarkers( gDP.colorImage.address, gDP.colorImage.address + numBytes - 1 );
}

#ifdef __GX__
/* [Xbox360] La RDRAM REAL del emulador (gc_memory/memory.h).  El `RDRAM` del
 * plugin apunta a la foto mientras dura una lista con "foto" (Cruis'n, ver
 * xbox/hilo_lista.cpp); distinta de `rdramb` = hay foto. */

/* [Xbox360] ★★ [Mario Kart 64 TV, puzzle de Banjo-Kazooie] EL COLOR BUFFER A
 * LA RDRAM, como GLideN64 en su gDPFullSync (gDP.cpp:1091-1095):
 *
 *     if (copyToRDRAM != ctDisable && !FBInfo::fbInfo.isSupported() &&
 *         pCurrentBuffer != nullptr && !pCurrentBuffer->isAuxiliary())
 *         FrameBuffer_CopyToRDRAM(gDP.colorImage.address, copyToRDRAM == ctSync);
 *
 * La CPU del juego lee de la RDRAM lo que el RDP pinto: Mario Kart copia un
 * trozo para la TV del circuito y Banjo la imagen para las piezas del puzzle
 * (blog de GLideN64, "Frame buffer emulation part II").  glN64 nunca la
 * escribia: dibuja en la GPU.  Y FrameBuffer_CopyToRDRAM de Wii64 lee los
 * texels GX de la textura guardada, que el shim no rellena (GX_CopyTex los
 * resuelve a una textura de D3D), asi que aqui no sirve.
 *
 * Sin esperar a la GPU (GXD3D_PideColorARDRAM / GXD3D_EntregaColorARDRAM): lo
 * que se escribe es la copia terminada mas nueva, de 1-3 frames antes.  Va
 * ANTES del DP_INT, para que el juego la tenga al leer despues de su lista.
 *
 * Buffer principal (no auxiliar), como la referencia: ancho del VI y 16 bits
 * (lo que escribe la conversion), y no el depth image.  El rect del EFB es el
 * del buffer principal: origen y escala del plugin, igual que la lectura de
 * profundidad de _gDPPeekDepth. */
static void _gDPCopiaColorARDRAM()
{
	u32 dir = 0, bytes = 0;

	if (GXD3D_EntregaColorARDRAM( &dir, &bytes ) && bytes != 0)
		FrameBuffer_RestampMarkers( dir, dir + bytes - 1 );   // escritura nuestra

	if (gDP.colorImage.size != G_IM_SIZ_16b ||
	    gDP.colorImage.width != VI.width ||
	    gDP.colorImage.address == gDP.depthImageAddress ||
	    VI.width == 0 || VI.height == 0)
		return;

	const u32 n64w = gDP.colorImage.width;
	const u32 n64h = VI.height;
	const u32 dirCI = gDP.colorImage.address;
	// RDRAMSize es una MASCARA (ultima direccion valida), no un tamano.
	if (dirCI + n64w * n64h * 2 > RDRAMSize + 1)
		return;

	const u32 x = OGL.GXorigX;
	const u32 y = OGL.GXorigY;
	const u32 w = (u32)((f32)n64w * OGL.GXscaleX + 0.5f);
	const u32 h = (u32)((f32)n64h * OGL.GXscaleY + 0.5f);
	if (w == 0 || h == 0)
		return;
	/* [Cruis'n USA] Con foto, tampoco el color: llegaria a la real despues del
	 * DP (ver _gDPWriteRDRAM). */
	if (rdramb != RDRAM)
		return;

	// [Paper Mario] GLideN64 1f4d04f4: el ajuste del azul en esta copia.
	GXD3D_ColorARDRAMAzulPM( (config.generalEmulation.hacks & hack_paperMarioSubscreen) != 0 ? 1 : 0 );
	/* [Xbox360] ★ [Banjo-Kazooie] GLideN64 ctSync: la de ESTE frame, esperando
	 * a la GPU (ver GXD3D_CopiaColorARDRAMSincrona).  Si no llega a tiempo
	 * queda pedida como la de siempre. */
	if ((config.generalEmulation.hacks & hack_copyToRDRAMSync) != 0)
	{
		if (GXD3D_CopiaColorARDRAMSincrona( (u16*)&rdramb[dirCI], dirCI, (u16)x, (u16)y,
		                                    (u16)w, (u16)h, (u16)n64w, (u16)n64h, &bytes ) &&
		    bytes != 0)
			FrameBuffer_RestampMarkers( dirCI, dirCI + bytes - 1 );   // escritura nuestra
		return;
	}
	GXD3D_PideColorARDRAM( (u16*)&rdramb[dirCI], dirCI, (u16)x, (u16)y, (u16)w, (u16)h,
	                       (u16)n64w, (u16)n64h );
}
#endif // __GX__

void gDPFullSync()
{
	FB_frame++;

#ifdef __GX__
	if ((config.generalEmulation.hacks & hack_copyToRDRAM) != 0)
		_gDPCopiaColorARDRAM();
#endif // __GX__

	if (gDP.m_subscreen &&
	    gDP.colorImage.size == G_IM_SIZ_16b &&
	    gDP.colorImage.width == VI.width)
	{
		_gDPCopyWhiteToRDRAM();
		gDP.m_subscreen = false;
	}

	_gDPResolveArtifactDepths();
	gDPArtifactSynced = FALSE;

	GLN64_PROF_CNT(fsync);   /* [Xbox360] paso 0 del hilo; xenon/gln64_prof.h */
	*REG.MI_INTR |= MI_INTR_DP;

	CheckInterrupts();

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gDPFullSync();\n" );
#endif
}

void gDPTileSync()
{
#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_IGNORED | DEBUG_TEXTURE, "gDPTileSync();\n" );
#endif
}

void gDPPipeSync()
{
#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_IGNORED, "gDPPipeSync();\n" );
#endif
}

void gDPLoadSync()
{
#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_IGNORED, "gDPLoadSync();\n" );
#endif
}

void gDPNoOp()
{
#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_IGNORED, "gDPNoOp();\n" );
#endif
}

