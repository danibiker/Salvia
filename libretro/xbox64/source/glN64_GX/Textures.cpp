/**
 * glN64_GX - Textures.cpp
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
#include <ogc/lwp_heap.h>
#include <ogc/machine/processor.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "../gui/DEBUG.h"
#endif // __GX__

#ifndef __LINUX__
# include <windows.h>
#else
# include "../main/winlnxdefs.h"
# include <time.h>
# include <stdlib.h>
# ifndef min
#  define min(a,b) ((a) < (b) ? (a) : (b))
# endif
# ifndef max
#  define max(a,b) ((a) > (b) ? (a) : (b))
# endif
# define timeGetTime() time(NULL)
#endif
#ifndef __GX__
#include <memory.h>
#endif // !__GX__
#include "../xenon64_diag.h"   /* XBOX64_TRAZAS */
#include "OpenGL.h"
#include "Textures.h"
#include "GBI.h"
#include "RSP.h"
#include "gDP.h"
#include "gSP.h"
#include "N64.h"
#include "CRC.h"
#include "convert.h"
#include "2xSAI.h"
#include "FrameBuffer.h"
#include "Config.h"	// [Xbox360] hack_fbTextureOffset (filtro de las texturas de framebuffer)
#include "Combiner.h"	// [Xbox360] combiner.usesLOD (atlas de mipmap)

TextureCache	cache;
#ifdef __GX__
heap_cntrl* GXtexCache;
/* [Xbox360] La opcion `xbox64_fb_bilineal` (xenon/gln64_config.cpp): 0 auto,
 * 1 activado, 2 desactivado.  Ver TextureCache_ActivateTexture. */
extern "C" volatile int gln64_fb_bilineal;
#endif //__GX__

typedef u32 (*GetTexelFunc)( u64 *src, u16 x, u16 i, u8 palette );

inline u32 GetNone( u64 *src, u16 x, u16 i, u8 palette )
{
	return 0x00000000;
}

inline u32 GXGetIA31_IA4( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
	color4B = (x & 1) ? (color4B & 0x0F) : (color4B >> 4);
	u8 i4 = Three2Four[color4B >> 1];
	u8 a4 = One2Four[color4B & 0x01];
	u8 ia = ((a4 << 4) | i4);
	return (u32) ia;
}

inline u32 GXGetI4_IA4( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
	color4B = (x & 1) ? (color4B & 0x0F) : (color4B >> 4);
	u8 ia = ((color4B << 4) | color4B);
	return (u32) ia;
}

inline u32 GXGetIA44_IA4( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color = ((u8*)src)[x^(i<<1)];
	color = ((color & 0xf0) >> 4) | ((color & 0x0f) << 4);
	return (u32) color;
}

inline u32 GXGetCI4IA_IA8( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 ind4B = ((u8*)src)[(x>>1)^(i<<1)];
	u16 color = (x & 1) ?	*(u16*)(void*)&TMEM[256 + (palette << 4) + (ind4B & 0x0F)] :
							*(u16*)(void*)&TMEM[256 + (palette << 4) + (ind4B >> 4)];
	color = ((color & 0xff00) >> 8) | ((color & 0x00ff) << 8);
	return (u32) color;
}

inline u32 GXGetCI8IA_IA8( u64 *src, u16 x, u16 i, u8 palette )
{
	u16 color = *(u16*)(void*)&TMEM[256 + ((u8*)src)[x^(i<<1)]];
	color = ((color & 0xff00) >> 8) | ((color & 0x00ff) << 8);
	return (u32) color;
}

inline u32 GXGetCI16IA_IA8( u64 *src, u16 x, u16 i, u8 palette )
{
	u16 color = *(u16*)(void*)&TMEM[256 + (u8)((u16*)src)[x^i]];
	color = ((color & 0xff00) >> 8) | ((color & 0x00ff) << 8);
	return (u32) color;
}

inline u32 GXGetCI4RGBA_RGB5A3( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 ind4B = ((u8*)src)[(x>>1)^(i<<1)];
	u16 c = (x & 1) ?	*(u16*)(void*)&TMEM[256 + (palette << 4) + (ind4B & 0x0F)] :
						*(u16*)(void*)&TMEM[256 + (palette << 4) + (ind4B >> 4)];
	if ((c&1) != 0)		c = 0x8000|(((c>>11)&0x1F)<<10)|(((c>>6)&0x1F)<<5)|((c>>1)&0x1F);   //opaque texel
	else				c = 0x0000|(((c>>12)&0xF)<<8)|(((c>>7)&0xF)<<4)|((c>>2)&0xF);   //transparent texel
	return (u32) c;
}

inline u32 GXGetCI8RGBA_RGB5A3( u64 *src, u16 x, u16 i, u8 palette )
{
	u16 c = *(u16*)(void*)&TMEM[256 + ((u8*)src)[x^(i<<1)]];
	if ((c&1) != 0)	c = 0x8000|(((c>>11)&0x1F)<<10)|(((c>>6)&0x1F)<<5)|((c>>1)&0x1F);   //opaque texel
	else			c = 0x0000|(((c>>12)&0xF)<<8)|(((c>>7)&0xF)<<4)|((c>>2)&0xF);   //transparent texel
	return (u32) c;
}

inline u32 GXGetCI16RGBA_RGB5A3( u64 *src, u16 x, u16 i, u8 palette )
{
	u16 c = *(u16*)(void*)&TMEM[256 + (u8)((u16*)src)[x^i]];
	if ((c&1) != 0)	c = 0x8000|(((c>>11)&0x1F)<<10)|(((c>>6)&0x1F)<<5)|((c>>1)&0x1F);   //opaque texel
	else			c = 0x0000|(((c>>12)&0xF)<<8)|(((c>>7)&0xF)<<4)|((c>>2)&0xF);   //transparent texel
	return (u32) c;
}
 
inline u32 GXGetI8_IA8( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color = ((u8*)src)[x^(i<<1)];
	u16 ia = (color << 8) | color;
	return (u32) ia;
}

inline u32 GXGetIA88_IA8( u64 *src, u16 x, u16 i, u8 palette )
{
	u16 color = ((u16*)src)[x^i];
	color = ((color & 0xff00) >> 8) | ((color & 0x00ff) << 8);
	return (u32) color;
}

inline u32 GXGetRGBA5551_RGB5A3( u64 *src, u16 x, u16 i, u8 palette )
{
	u16 c = ((u16*)src)[x^i];
	if ((c&1) != 0)		c = 0x8000|(((c>>11)&0x1F)<<10)|(((c>>6)&0x1F)<<5)|((c>>1)&0x1F);   //opaque texel
	else				c = 0x0000|(((c>>12)&0xF)<<8)|(((c>>7)&0xF)<<4)|((c>>2)&0xF);   //transparent texel
	return (u32) c;
}

inline u32 GXGetUYVY8888_RGBA8( u64 *src, u16 x, u16 i, u8 palette )
{
	u16 color = ((u16*)src)[x^i];
	s32 y, u, v;
	s32 r, g, b;

	if (x & 1)
	{
		y = color & 0xFF;
		v = (color >> 8) - 0x80;
		color = ((u16*)src)[x^i^1];
		u = (color >> 8) - 0x80;
	}
	else
	{
		y = color & 0xFF;
		u = (color >> 8) - 0x80;
		color = ((u16*)src)[x^i^1];
		v = (color >> 8) - 0x80;
	}

	if (palette & 1)
	{
		g = y + ((gDP.convert.k1 * u + gDP.convert.k2 * v + 0x80) >> 8);
		if (g & ~0xFF)
			g = ~g >> 31 & 0xFF;
		b = y + ((gDP.convert.k3 * u + 0x80) >> 8);
		if (b & ~0xFF)
			b = ~b >> 31 & 0xFF;
		color = (g << 8) | b;
	}
	else
	{
		r = y + ((gDP.convert.k0 * v + 0x80) >> 8);
		if (r & ~0xFF)
			r = ~r >> 31 & 0xFF;
		color = (y << 8) | r;
	}

	return (u32) color;
}

inline u32 GXGetRGBA8888_RGBA8( u64 *src, u16 x, u16 i, u8 palette )
{
//set palette = 0 for AR texels and palette = 1 for GB texels
	u32 c = ((u32*)src)[x^i]; // 0xRRGGBBAA
	u16 color = (palette & 1) ? /* GGBB */ (u16) ((c >> 8) & 0x0000FFFF) : /* AARR */ (u16) (((c & 0x000000FF) << 8) | (c >> 24));
	return (u32) color;
}

/* [Xbox360] ★ [Resident Evil 2] UNA TEXTURA DE PROFUNDIDAD: LA PALABRA DE Z DEL
 * N64 ENTERA.  GLideN64 5e51b386 (hack_LoadDepthTextures, _loadDepthTexture):
 * lo que se dibuja sobre el depth image se carga como R16F con el texel crudo
 * (`_pDest[i] / 65535.0f`) y el shader lo escribe como profundidad.  Aqui en
 * RGBA8: A = FF, R = byte alto, G = byte bajo, B = 0 (las dos mitades de GX,
 * como GXGetRGBA8888_RGBA8: palette 0 = AARR, 1 = GGBB), y el render to depth
 * modo 5 la decodifica (gx_d3d9_tev.cpp).  El alfa a FF para que ningun alpha
 * compare tire las Z cercanas, que tienen el byte alto a 0. */
inline u32 GXGetRE2Z_RGBA8( u64 *src, u16 x, u16 i, u8 palette )
{
	const u16 z = ((u16*)src)[x^i];
	return (palette & 1) ? /* GGBB */ (u32)((z & 0x00FF) << 8) : /* AARR */ (u32)(0xFF00 | (z >> 8));
}

/* Si la textura que se carga es una de esas: hack_RE2, el color image es el
 * depth image y el texel es de 16 bits (GLideN64 no mira el tamano, pero su
 * _loadDepthTexture lee el destino como u16). */
static BOOL _texZRE2( u32 size )
{
	return ((config.generalEmulation.hacks & hack_RE2) != 0 &&
	        gDP.colorImage.address == gDP.depthImageAddress &&
	        size == G_IM_SIZ_16b) ? TRUE : FALSE;
}

/* Lo que se mezcla en el crc de una textura de Z de RE2: la misma imagen
 * cargada como color no puede servir para la Z, ni al reves. */
#define TEXZ_RE2_CRC	0x52453245u	/* "RE2E" */

inline u32 GetCI4IA_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	if (x & 1)
		return IA88_RGBA4444( *(u16*)(void*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
	else
		return IA88_RGBA4444( *(u16*)(void*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

inline u32 GetCI4IA_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	if (x & 1)
		return IA88_RGBA8888( *(u16*)(void*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
	else
		return IA88_RGBA8888( *(u16*)(void*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

inline u32 GetCI4RGBA_RGBA5551( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	if (x & 1)
		return RGBA5551_RGBA5551( *(u16*)(void*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
	else
		return RGBA5551_RGBA5551( *(u16*)(void*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

inline u32 GetCI4RGBA_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	if (x & 1)
		return RGBA5551_RGBA8888( *(u16*)(void*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
	else
		return RGBA5551_RGBA8888( *(u16*)(void*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

inline u32 GetIA31_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	return IA31_RGBA8888( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

inline u32 GetIA31_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	return IA31_RGBA4444( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

inline u32 GetI4_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	return I4_RGBA8888( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

inline u32 GetI4_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	return I4_RGBA4444( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

inline u32 GetCI8IA_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA88_RGBA4444( *(u16*)(void*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

inline u32 GetCI8IA_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA88_RGBA8888( *(u16*)(void*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

inline u32 GetCI8RGBA_RGBA5551( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA5551_RGBA5551( *(u16*)(void*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

inline u32 GetCI8RGBA_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA5551_RGBA8888( *(u16*)(void*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

inline u32 GetIA44_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA44_RGBA8888(((u8*)src)[x^(i<<1)]);
}

inline u32 GetIA44_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA44_RGBA4444(((u8*)src)[x^(i<<1)]);
}

inline u32 GetI8_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return I8_RGBA8888(((u8*)src)[x^(i<<1)]);
}

inline u32 GetI8_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return I8_RGBA4444(((u8*)src)[x^(i<<1)]);
}

inline u32 GetRGBA5551_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA5551_RGBA8888( ((u16*)src)[x^i] );
}

inline u32 GetRGBA5551_RGBA5551( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA5551_RGBA5551( ((u16*)src)[x^i] );
}

inline u32 GetIA88_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA88_RGBA8888(((u16*)src)[x^i]);
}

inline u32 GetIA88_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA88_RGBA4444(((u16*)src)[x^i]);
}

inline u32 GetRGBA8888_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return ((u32*)src)[x^i];
}

inline u32 GetRGBA8888_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA8888_RGBA4444(((u32*)src)[x^i]);
}

const struct
{
#ifdef __GX__
	GetTexelFunc	GetGX;
	u8				GXtexfmt;
	u32				GXsize;
#else // __GX__
	GetTexelFunc	Get16;
	GLenum			glType16;
	GLint			glInternalFormat16;
	GetTexelFunc	Get32;
	GLenum			glType32;
	GLint			glInternalFormat32;
	u32				autoFormat; 
#endif // !__GX__
	u32				lineShift, maxTexels;
} imageFormat[4][4][5] =
#ifdef __GX__
{ //			GetGX					GXtexfmt		GXsize	lineShift	maxTexels
	{ // G_TT_NONE
		{ // 4-bit
			{	GXGetI4_IA4,			GX_TF_IA4,		1,		4,			8192 }, // RGBA as I
			{	GetNone,				GX_TF_I4,		0,		4,			8192 }, // YUV
			{	GXGetCI4RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		4,			4096 }, // CI as I (FIXME)
			{	GXGetIA31_IA4,			GX_TF_IA4,		1,		4,			8192 }, // IA
			{	GXGetI4_IA4,			GX_TF_IA4,		1,		4,			8192 }, // I
		},
		{ // 8-bit
			{	GXGetI8_IA8,			GX_TF_IA8,		2,		3,			4096 }, // RGBA as I
			{	GetNone,				GX_TF_I4,		0,		3,			4096 }, // YUV
			/* [Xbox360] [Ogre Battle 64, siluetas de la colina al reves] CI de 8
			 * bits SIN TLUT = intensidad (I = A = el indice), como GLideN64
			 * (Textures.cpp imageFormat[G_TT_NONE][8-bit][CI]: GetI8, "CI without
			 * palette", 4096 texels).  Wii64 lo pasaba por la paleta y el alfa
			 * salia de ella: el combinador del juego (TEXEL0*PRIM, alfa TEXEL0)
			 * pintaba negro lo que era transparente.  La fila de 4 bits sigue
			 * como estaba: GLideN64 da ahi un valor constante sacado del numero
			 * de paleta, y no hay ningun juego medido que lo use. */
			{	GXGetI8_IA8,			GX_TF_IA8,		2,		3,			4096 }, // CI without palette = I
			{	GXGetIA44_IA4,			GX_TF_IA4,		1,		3,			4096 }, // IA
			{	GXGetI8_IA8,			GX_TF_IA8,		2,		3,			4096 }, // I
		},
		{ // 16-bit
			{	GXGetRGBA5551_RGB5A3,	GX_TF_RGB5A3,	2,		2,			2048 }, // RGBA
			{	GXGetUYVY8888_RGBA8,	GX_TF_RGBA8,	4,		2,			2048 }, // YUV
			{	GXGetIA88_IA8,			GX_TF_IA8,		2,		2,			2048 }, // CI as IA
			{	GXGetIA88_IA8,			GX_TF_IA8,		2,		2,			2048 }, // IA
			{	GetNone,				GX_TF_I4,		0,		2,			2048 }, // I
		},
		{ // 32-bit
			{	GXGetRGBA8888_RGBA8,	GX_TF_RGBA8,	4,		2,			1024 }, // RGBA
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // YUV
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // CI
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // IA
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // I
		}
	},
	{ // DUMMY
		{ // 4-bit
			{	GXGetCI4RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		4,			4096 }, // RGBA as CI
			{	GetNone,				GX_TF_I4,		0,		4,			4096 }, // YUV as CI
			{	GXGetCI4RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		4,			4096 }, // CI
			{	GXGetCI4RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		4,			4096 }, // IA as CI
			{	GXGetCI4RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		4,			4096 }, // I as CI
		},
		{ // 8-bit
			{	GXGetCI8RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		3,			2048 }, // RGBA as CI
			{	GetNone,				GX_TF_I4,		0,		3,			2048 }, // YUV as CI
			{	GXGetCI8RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		3,			2048 }, // CI
			{	GXGetCI8RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		3,			2048 }, // IA as CI
			{	GXGetCI8RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		3,			2048 }, // I as CI
		},
		{ // 16-bit
			{	GXGetCI16RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		2,			1024 }, // RGBA as CI
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // YUV as CI
			{	GXGetCI16RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		2,			1024 }, // CI
			{	GXGetCI16RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		2,			1024 }, // IA as CI
			{	GXGetCI16RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		2,			1024 }, // I as CI
		},
		{ // 32-bit
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // RGBA
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // YUV
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // CI
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // IA
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // I
		}
	},
	{ // G_TT_RGBA16
		{ // 4-bit
			{	GXGetCI4RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		4,			4096 }, // RGBA as CI
			{	GetNone,				GX_TF_I4,		0,		4,			4096 }, // YUV as CI
			{	GXGetCI4RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		4,			4096 }, // CI
			{	GXGetCI4RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		4,			4096 }, // IA as CI
			{	GXGetCI4RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		4,			4096 }, // I as CI
		},
		{ // 8-bit
			{	GXGetCI8RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		3,			2048 }, // RGBA as CI
			{	GetNone,				GX_TF_I4,		0,		3,			2048 }, // YUV as CI
			{	GXGetCI8RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		3,			2048 }, // CI
			{	GXGetCI8RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		3,			2048 }, // IA as CI
			{	GXGetCI8RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		3,			2048 }, // I as CI
		},
		{ // 16-bit
			{	GXGetCI16RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		2,			1024 }, // RGBA as CI
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // YUV as CI
			{	GXGetCI16RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		2,			1024 }, // CI
			{	GXGetCI16RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		2,			1024 }, // IA as CI
			{	GXGetCI16RGBA_RGB5A3,	GX_TF_RGB5A3,	2,		2,			1024 }, // I as CI
		},
		{ // 32-bit
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // RGBA
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // YUV
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // CI
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // IA
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // I
		}
	},
	{ // G_TT_IA16
		{ // 4-bit
			{	GXGetCI4IA_IA8,			GX_TF_IA8,		2,		4,			4096 }, // RGBA as CI
			{	GetNone,				GX_TF_I4,		0,		4,			4096 }, // YUV as CI
			{	GXGetCI4IA_IA8,			GX_TF_IA8,		2,		4,			4096 }, // CI
			{	GXGetCI4IA_IA8,			GX_TF_IA8,		2,		4,			4096 }, // IA as CI
			{	GXGetCI4IA_IA8,			GX_TF_IA8,		2,		4,			4096 }, // I as CI
		},
		{ // 8-bit
			{	GXGetCI8IA_IA8,			GX_TF_IA8,		2,		3,			2048 }, // RGBA as CI
			{	GetNone,				GX_TF_I4,		0,		3,			2048 }, // YUV as CI
			{	GXGetCI8IA_IA8,			GX_TF_IA8,		2,		3,			2048 }, // CI
			{	GXGetCI8IA_IA8,			GX_TF_IA8,		2,		3,			2048 }, // IA as CI
			{	GXGetCI8IA_IA8,			GX_TF_IA8,		2,		3,			2048 }, // I as CI
		},
		{ // 16-bit
			{	GXGetCI16IA_IA8,		GX_TF_IA8,		2,		2,			1024 }, // RGBA as CI
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // YUV as CI
			{	GXGetCI16IA_IA8,		GX_TF_IA8,		2,		2,			1024 }, // CI
			{	GXGetCI16IA_IA8,		GX_TF_IA8,		2,		2,			1024 }, // IA as CI
			{	GXGetCI16IA_IA8,		GX_TF_IA8,		2,		2,			1024 }, // I as CI
		},
		{ // 32-bit
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // RGBA
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // YUV
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // CI
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // IA
			{	GetNone,				GX_TF_I4,		0,		2,			1024 }, // I
		}
	}
};
#else // __GX__
{ //		Get16					glType16						glInternalFormat16	Get32					glType32						glInternalFormat32	autoFormat
	{ // 4-bit
		{	GetCI4RGBA_RGBA5551,	GL_UNSIGNED_SHORT_5_5_5_1_EXT,	GL_RGB5_A1,			GetCI4RGBA_RGBA8888,	GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGB5_A1, 4, 4096 }, // CI (Banjo-Kazooie uses this, doesn't make sense, but it works...)
		{	GetNone,				GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetNone,				GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 4, 8192 }, // YUV
		{	GetCI4RGBA_RGBA5551,	GL_UNSIGNED_SHORT_5_5_5_1_EXT,	GL_RGB5_A1,			GetCI4RGBA_RGBA8888,	GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGB5_A1, 4, 4096 }, // CI
		{	GetIA31_RGBA4444,		GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetIA31_RGBA8888,		GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 4, 8192 }, // IA
		{	GetI4_RGBA4444,			GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetI4_RGBA8888,			GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 4, 8192 }, // I
	},
	{ // 8-bit
		{	GetCI8RGBA_RGBA5551,	GL_UNSIGNED_SHORT_5_5_5_1_EXT,	GL_RGB5_A1,			GetCI8RGBA_RGBA8888,	GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGB5_A1, 3, 2048 }, // RGBA
		{	GetNone,				GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetNone,				GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 0, 4096 }, // YUV
		{	GetCI8RGBA_RGBA5551,	GL_UNSIGNED_SHORT_5_5_5_1_EXT,	GL_RGB5_A1,			GetCI8RGBA_RGBA8888,	GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGB5_A1, 3, 2048 }, // CI
		{	GetIA44_RGBA4444,		GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetIA44_RGBA8888,		GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 3, 4096 }, // IA
		{	GetI8_RGBA4444,			GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetI8_RGBA8888,			GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA8, 3, 4096 }, // I
	},
	{ // 16-bit
		{	GetRGBA5551_RGBA5551,	GL_UNSIGNED_SHORT_5_5_5_1_EXT,	GL_RGB5_A1,			GetRGBA5551_RGBA8888,	GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGB5_A1, 2, 2048 }, // RGBA
		{	GetNone,				GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetNone,				GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 2, 2048 }, // YUV
		{	GetNone,				GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetNone,				GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 0, 2048 }, // CI
		{	GetIA88_RGBA4444,		GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetIA88_RGBA8888,		GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA8, 2, 2048 }, // IA
		{	GetNone,				GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetNone,				GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 0, 2048 }, // I
	},
	{ // 32-bit
		{	GetRGBA8888_RGBA4444,	GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetRGBA8888_RGBA8888,	GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA8, 2, 1024 }, // RGBA
		{	GetNone,				GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetNone,				GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 0, 1024 }, // YUV
		{	GetNone,				GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetNone,				GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 0, 1024 }, // CI
		{	GetNone,				GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetNone,				GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 0, 1024 }, // IA
		{	GetNone,				GL_UNSIGNED_SHORT_4_4_4_4_EXT,	GL_RGBA4,			GetNone,				GL_UNSIGNED_BYTE,				GL_RGBA8,			GL_RGBA4, 0, 1024 }, // I
	}
};
#endif // !__GX__


static CachedTexture  textureSlots[GX_MAX_TEXTURES];

static void *GXtexCacheBase = NULL;
static CachedTexture *freeTextureSlots = NULL;

static void TextureCache_InitSlots()
{
	freeTextureSlots = NULL;
	for (u32 i = 0; i < (u32)GX_MAX_TEXTURES; i++)
	{
		textureSlots[i].lower = freeTextureSlots;
		freeTextureSlots = &textureSlots[i];
	}
}

static CachedTexture *TextureCache_AllocSlot()
{
	CachedTexture *slot = freeTextureSlots;
	if (slot != NULL)
		freeTextureSlots = slot->lower;
	return slot;
}

static void TextureCache_FreeSlot( CachedTexture *slot )
{
	slot->lower = freeTextureSlots;
	freeTextureSlots = slot;
}

#define BG_KEEP 4
static CachedTexture *bgKeep[BG_KEEP] = { NULL, NULL, NULL, NULL };
static u32 bgKeepNext = 0;

static void TextureCache_KeepBackground( CachedTexture *tex )
{
	for (u32 i = 0; i < BG_KEEP; i++)
		if (bgKeep[i] == tex)
			return;

	bgKeep[bgKeepNext] = tex;
	bgKeepNext = (bgKeepNext + 1) % BG_KEEP;
}

static bool TextureCache_IsKeptBackground( const CachedTexture *tex )
{
	for (u32 i = 0; i < BG_KEEP; i++)
		if (bgKeep[i] == tex)
			return true;

	return false;
}

static void TextureCache_ForgetBackground( const CachedTexture *tex )
{
	for (u32 i = 0; i < BG_KEEP; i++)
		if (bgKeep[i] == tex)
			bgKeep[i] = NULL;
}

static void TextureCache_ForgetCurrent( const CachedTexture *tex )
{
	for (u32 i = 0; i < 2; i++)
		if (cache.current[i] == tex)
			cache.current[i] = NULL;
}

static const CachedTexture *pinnedTexture = NULL;

void TextureCache_PinTexture( const CachedTexture *tex )
{
	pinnedTexture = tex;
}

static bool TextureCache_IsInUse( const CachedTexture *tex )
{
	return (tex == cache.current[0]) || (tex == cache.current[1]) ||
	       (tex != NULL && tex == pinnedTexture);
}

static void TextureCache_ForgetAllBackgrounds()
{
	for (u32 i = 0; i < BG_KEEP; i++)
		bgKeep[i] = NULL;

	bgKeepNext = 0;
}

#ifdef __GX__
static bool TextureCache_HasKeptBackgrounds()
{
	for (u32 i = 0; i < BG_KEEP; i++)
		if (bgKeep[i] != NULL)
			return true;

	return false;
}

BOOL TextureCache_FreeOneTexture()
{
	const u32 numBefore = cache.numCached;

	TextureCache_FreeNextTexture();
	if (cache.numCached < numBefore)
		return TRUE;

	if (TextureCache_HasKeptBackgrounds())
	{
		TextureCache_ForgetAllBackgrounds();
		TextureCache_FreeNextTexture();
		if (cache.numCached < numBefore)
			return TRUE;
	}

	CachedTexture *victim = cache.bottom;
	while (victim != NULL)
	{
		if (victim != cache.dummy && !TextureCache_IsInUse( victim ) &&
		    !(victim->frameBufferTexture && FrameBuffer_IsTextureLive( victim )))
		{
			TextureCache_Remove( victim );
			break;
		}
		victim = victim->higher;
	}

	return (cache.numCached < numBefore) ? TRUE : FALSE;
}
#endif // __GX__

static void _texHeapDropRetired();

void TextureCache_Init()
{
	TextureCache_InitSlots();
	TextureCache_ForgetAllBackgrounds();
	cache.current[0] = NULL;
	cache.current[1] = NULL;
	cache.top = NULL;
	cache.bottom = NULL;
	/* [Xbox360] El indice se vacia CON la lista, en el mismo sitio y con el
	 * mismo criterio.  Si sobreviviera a un `Init` quedarian punteros a
	 * nodos ya liberados -- el fallo de recarga de juego de siempre. */
	memset( cache.cubo, 0, sizeof( cache.cubo ) );
	cache.numCached = 0;
	cache.cachedBytes = 0;
	cache.enable2xSaI = OGL.enable2xSaI;
	cache.bitDepth = OGL.textureBitDepth;

#ifdef __GX__
	//Init texture cache heap if not yet inited
	if(!GXtexCache)
	{
		GXtexCache = (heap_cntrl*)malloc(sizeof(heap_cntrl));
#ifdef HW_RVL
		GXtexCacheBase = TEXCACHE_LO;
#else //HW_RVL
		GXtexCacheBase = memalign(32,GX_TEXTURE_CACHE_SIZE);
#endif //!HW_RVL
		__lwp_heap_init(GXtexCache, GXtexCacheBase, GX_TEXTURE_CACHE_SIZE, 32);
#ifdef SHOW_DEBUG
		DEBUG_registerHeap(GXtexCache, "TEX");
#endif
	}
	else if (GXtexCacheBase != NULL)
	{
		_texHeapDropRetired();
		__lwp_heap_init(GXtexCache, GXtexCacheBase, GX_TEXTURE_CACHE_SIZE, 32);
	}
#endif //__GX__

#ifndef __GX__
	u32 dummyTexture[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	glGenTextures( 32, cache.glNoiseNames );

	u8 noise[64*64*4];
	for (s16 i = 0; i < 32; i++)
	{
		glBindTexture( GL_TEXTURE_2D, cache.glNoiseNames[i] );

		srand( timeGetTime() );

		for (s16 y = 0; y < 64; y++)
		{
			for (s16 x = 0; x < 64; x++)
			{
				u8 random = rand();
				noise[y*64*4+x*4] = random;
				noise[y*64*4+x*4+1] = random;
				noise[y*64*4+x*4+2] = random;
				noise[y*64*4+x*4+3] = random;
			}
		}
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, noise );
	}
#else // !__GX__
	//Noise textures don't seem to be necessary and would take 512kB of memory, so don't use for now.
	//TODO: Implement if needed?
#endif // __GX__

	cache.dummy = TextureCache_AddTop();

	cache.dummy->address = 0;
	cache.dummy->clampS = 1;
	cache.dummy->clampT = 1;
	cache.dummy->clampWidth = 2;
	cache.dummy->clampHeight = 2;
	cache.dummy->crc = 0;
	cache.dummy->format = 0;
	cache.dummy->size = 0;
	cache.dummy->frameBufferTexture = FALSE;
	cache.dummy->width = 2;
	cache.dummy->height = 2;
	cache.dummy->realWidth = 0;
	cache.dummy->realHeight = 0;
	cache.dummy->maskS = 0;
	cache.dummy->maskT = 0;
	cache.dummy->scaleS = 0.5f;
	cache.dummy->scaleT = 0.5f;
	cache.dummy->shiftScaleS = 1.0f;
	cache.dummy->shiftScaleT = 1.0f;
	cache.dummy->textureBytes = 64;
	cache.dummy->tMem = 0;

#ifndef __GX__
	glBindTexture( GL_TEXTURE_2D, cache.dummy->glName );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummyTexture );
#else // !__GX__
	//Dummy texture doesn't seem to be needed, so don't load into GX for now.
//	cache.dummy->GXtexture = (u16*) memalign(32,cache.dummy->textureBytes);
	cache.dummy->GXtexture = (u16*) __lwp_heap_allocate(GXtexCache,cache.dummy->textureBytes);
	cache.dummy->GXtexfmt = GX_TF_RGBA8;
	DCZeroRange(cache.dummy->GXtexture, cache.dummy->textureBytes);
	DCFlushRange(cache.dummy->GXtexture, cache.dummy->textureBytes);

#endif // __GX__

	cache.cachedBytes = cache.dummy->textureBytes;

	TextureCache_ActivateDummy( 0 );
	TextureCache_ActivateDummy( 1 );
	TextureCache_ActivateDummy( 2 );
}

BOOL TextureCache_Verify()
{
	s16 i = 0;
	CachedTexture *current;
	
	current = cache.top;

	while (current)
	{
		i++;
		current = current->lower;
	}
	if ((unsigned short)i != cache.numCached) return FALSE;

	i = 0;
	current = cache.bottom;
	while (current)
	{
		i++;
		current = current->higher;
	}
	if ((unsigned short)i != cache.numCached) return FALSE;

	return TRUE;
}

// Special case handling because a CachedTexture slot was re-used while a FrameBuffer still referenced it.
static BOOL _texHeapOwns(const void *ptr)
{
	if (ptr == NULL || GXtexCache == NULL)
		return FALSE;

	return ((const char*)ptr > (const char*)GXtexCache->start &&
	        (const char*)ptr < (const char*)GXtexCache->final) ? TRUE : FALSE;
}

// GX_CopyTex() only queues the copy, and we run into timing issues in some games
// like Vigilante 8 where it'll write into a FB tex after we've replaced it.
#define TEX_RETIRE_MAX 64

static void *texRetire[TEX_RETIRE_MAX];
static u32 texRetireCount = 0;

static void _texHeapDropRetired()
{
	texRetireCount = 0;
}

void TextureCache_ReleaseRetired()
{
	while (texRetireCount > 0)
		__lwp_heap_free( GXtexCache, texRetire[--texRetireCount] );
}

static void _texHeapRetire( void *ptr )
{
	if (texRetireCount >= TEX_RETIRE_MAX)
	{
		GX_DrawDone();
		TextureCache_ReleaseRetired();
	}

	texRetire[texRetireCount++] = ptr;
}

static void _texHeapFree(CachedTexture *texture)
{
	if (texture->GXtexture == NULL)
		return;

	if (!_texHeapOwns(texture->GXtexture))
	{
		texture->GXtexture = NULL;
		return;
	}

	if (texture->frameBufferTexture)
		_texHeapRetire( texture->GXtexture );
	else
		__lwp_heap_free(GXtexCache, texture->GXtexture);

	texture->GXtexture = NULL;
}

/* [Xbox360] ★★ EL INDICE POR CRC.  Ver la nota larga en Textures.h.
 *
 * Las dos reglas, y son las unicas que hay que respetar:
 *
 *   1. Se INDEXA en cuanto el nodo tiene su crc definitivo, o sea justo
 *      detras de `nodo->crc = crc` (Textures.cpp, dos sitios).  NO se puede
 *      hacer dentro de `TextureCache_AddTop`: alli el nodo sale recien
 *      puesto a cero y todavia no tiene crc.
 *   2. Se DESINDEXA antes de liberar el nodo, en los DOS unicos sitios que
 *      lo liberan: `TextureCache_RemoveBottom` y `TextureCache_Remove`.
 *
 * Si alguna vez aparece un tercer sitio que libere un CachedTexture, tiene
 * que desindexar tambien o el cubo se queda con un puntero colgando -- que
 * es exactamente la clase de fallo de [[project_xenon64_compartir_vs_cacheada]].
 * Los dos sitios son los unicos que llaman a `TextureCache_FreeSlot`. */
static void TextureCache_Indexar( CachedTexture *t )
{
	u32 b = t->crc & (TEXCACHE_CUBOS - 1);

	t->hprev = NULL;
	t->hnext = cache.cubo[b];
	if (cache.cubo[b])
		cache.cubo[b]->hprev = t;
	cache.cubo[b] = t;
	t->hCubo = b + 1;          /* +1: el 0 del memset significa "no indexada" */
}

static void TextureCache_Desindexar( CachedTexture *t )
{
	u32 b;

	if (!t->hCubo)             /* nunca se indexo (p.ej. `cache.dummy`) */
		return;
	b = t->hCubo - 1;

	if (t->hprev)
		t->hprev->hnext = t->hnext;
	else
		cache.cubo[b] = t->hnext;

	if (t->hnext)
		t->hnext->hprev = t->hprev;

	t->hnext = t->hprev = NULL;
	t->hCubo = 0;
}

void TextureCache_RemoveBottom()
{
	TextureCache_Desindexar( cache.bottom );

	TextureCache_ForgetBackground( cache.bottom );
	TextureCache_ForgetCurrent( cache.bottom );

	CachedTexture *newBottom = cache.bottom->higher;

#ifndef __GX__
	glDeleteTextures( 1, &cache.bottom->glName );
#endif // !__GX__

	cache.cachedBytes -= cache.bottom->textureBytes;

	if (cache.bottom->frameBufferTexture)
		FrameBuffer_RemoveBufferForTexture( cache.bottom );

	if (cache.bottom == cache.top)
		cache.top = NULL;

#ifdef __GX__
	_texHeapFree(cache.bottom);

	TextureCache_FreeSlot( cache.bottom );
#else
	free( cache.bottom );
#endif // __GX__
    cache.bottom = newBottom;
	
	if (cache.bottom)
		cache.bottom->lower = NULL;

	cache.numCached--;
}

void TextureCache_Remove( CachedTexture *texture )
{
	TextureCache_Desindexar( texture );

	TextureCache_ForgetBackground( texture );
	TextureCache_ForgetCurrent(texture);

#ifdef __GX__
	if (texture->frameBufferTexture)
		FrameBuffer_RemoveBufferForTexture(texture);
#endif //__GX__

	if ((texture == cache.bottom) &&
		(texture == cache.top))
	{
		cache.top = NULL;
		cache.bottom = NULL;
	}
	else if (texture == cache.bottom)
	{
		cache.bottom = texture->higher;

		if (cache.bottom)
			cache.bottom->lower = NULL;
	}
	else if (texture == cache.top)
	{
		cache.top = texture->lower;

		if (cache.top)
			cache.top->higher = NULL;
	}
	else
	{
		texture->higher->lower = texture->lower;
		texture->lower->higher = texture->higher;
	}

#ifndef __GX__
	glDeleteTextures( 1, &texture->glName );
#endif // !__GX__

	cache.cachedBytes -= texture->textureBytes;
#ifdef __GX__
	_texHeapFree(texture);
	TextureCache_FreeSlot( texture );
#else // !__GX__
	free( texture );
#endif // __GX__

	cache.numCached--;
}

CachedTexture *TextureCache_AddTop()
{
#ifndef __GX__
	while (cache.cachedBytes > cache.maxBytes)
//	while (cache.cachedBytes > 64) //cache.dummy->textureBytes)
	{
		if (cache.bottom != cache.dummy)
			TextureCache_RemoveBottom();
		else if (cache.dummy->higher)
			TextureCache_Remove( cache.dummy->higher );
	}
#else //!__GX__
	// Evict through TextureCache_FreeOneTexture(), which honours the framebuffer
	// and background protections but still guarantees progress.
	// TextureCache_RemoveBottom() takes the LRU entry whatever it is, and the
	// backgrounds sit at that end by the time the byte budget forces a free.
	while ((cache.cachedBytes > cache.maxBytes) || cache.numCached >= GX_MAX_TEXTURES)
	{
		if (!TextureCache_FreeOneTexture())
			break;	// nothing evictable left
	}
#endif //__GX__

#ifdef __GX__
	CachedTexture *newtop = TextureCache_AllocSlot();
#else // !__GX__
	CachedTexture *newtop = (CachedTexture*)malloc( sizeof( CachedTexture ) );
#endif // __GX__
	while(!newtop) {
#ifdef __GX__
		if (!TextureCache_FreeOneTexture())
			break;
#else // !__GX__
		if (cache.bottom != cache.dummy)
			TextureCache_RemoveBottom();
		else if (cache.dummy->higher)
			TextureCache_Remove( cache.dummy->higher );
#endif // __GX__
		//print_gecko("Out of memory %d/%d, next %d/%d\r\n", cache.cachedBytes, cache.maxBytes, cache.numCached, GX_MAX_TEXTURES);
#ifdef __GX__
		newtop = TextureCache_AllocSlot();
#else // !__GX__
		newtop = (CachedTexture*)malloc( sizeof( CachedTexture ) );
#endif // __GX__
	}
	//print_gecko("Memory %d/%d, next %d/%d\r\n", cache.cachedBytes, cache.maxBytes, cache.numCached, GX_MAX_TEXTURES);
	if (newtop == NULL)
		return NULL;

	memset( newtop, 0x00, sizeof( CachedTexture ) );

#ifndef __GX__
	glGenTextures( 1, &newtop->glName );
#else // !__GX__
	//This should be taken care of later when we call GX_InitTexObj()
	newtop->GXtexture = NULL;
#endif // __GX__

	newtop->lower = cache.top;
	newtop->higher = NULL;

	if (cache.top)
		cache.top->higher = newtop;

	if (!cache.bottom)
		cache.bottom = newtop;

    cache.top = newtop;

	cache.numCached++;

	return newtop;
}

void TextureCache_MoveToTop( CachedTexture *newtop )
{
	if (newtop == cache.top) return;

	if (newtop == cache.bottom)
	{
		cache.bottom = newtop->higher;
		cache.bottom->lower = NULL;
	}
	else
	{
		newtop->higher->lower = newtop->lower;
		newtop->lower->higher = newtop->higher;
	}

	newtop->higher = NULL;
	newtop->lower = cache.top;
	cache.top->higher = newtop;
	cache.top = newtop;
}

void TextureCache_Destroy()
{
	while (cache.bottom)
		TextureCache_RemoveBottom();

	/* [Xbox360] `RemoveBottom` ya desindexa uno a uno, asi que los cubos
	 * quedan vacios solos.  Esto es el cinturon: si algun dia un nodo se
	 * libera por otro camino, aqui se corta igual en vez de dejar el
	 * puntero para la siguiente ROM. */
	memset( cache.cubo, 0, sizeof( cache.cubo ) );

	cache.cachedBytes = 0;
	cache.numCached = 0;
	cache.current[0] = NULL;
	cache.current[1] = NULL;
	cache.dummy = NULL;
	TextureCache_ForgetAllBackgrounds();
	TextureCache_PinTexture( NULL );
#ifndef __GX__
	glDeleteTextures( 32, cache.glNoiseNames );
//	glDeleteTextures( 1, &cache.glDummyName );
#else // !__GX__
	//For now we're not using Noise textures.

#endif // __GX__

	cache.top = NULL;
	cache.bottom = NULL;
}

void TextureCache_LoadBackground( CachedTexture *texInfo )
{
	u8 *dest = NULL, *scaledDest;
#ifndef __GX__
	u8 *swapped;
	GLuint			glInternalFormat;
	GLenum			glType;
#else // !__GX__
	u32	GXsize, k, l;
#endif // __GX__

	u8 *src;
	u32 numBytes, bpl;
	u32 x, y, /*i,*/ j, tx, ty;
	u16 clampSClamp;
	u16 clampTClamp;
	GetTexelFunc	GetTexel;

#ifndef __GX__
	if (((imageFormat[texInfo->size][texInfo->format].autoFormat == GL_RGBA8) || 
		((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16)) || (cache.bitDepth == 2)) && (cache.bitDepth != 0))
	{
	 	texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) << 2;
		if ((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16))
		{
			if (texInfo->size == G_IM_SIZ_4b)
				GetTexel = GetCI4IA_RGBA8888;
			else
				GetTexel = GetCI8IA_RGBA8888;

			glInternalFormat = GL_RGBA8;
			glType = GL_UNSIGNED_BYTE;
		}
		else
		{
			GetTexel = imageFormat[texInfo->size][texInfo->format].Get32;
			glInternalFormat = imageFormat[texInfo->size][texInfo->format].glInternalFormat32;
			glType = imageFormat[texInfo->size][texInfo->format].glType32;
		}
	}
	else
	{
	 	texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) << 1;
		if ((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16))
		{
			if (texInfo->size == G_IM_SIZ_4b)
				GetTexel = GetCI4IA_RGBA4444;
			else
				GetTexel = GetCI8IA_RGBA4444;

			glInternalFormat = GL_RGBA4;
			glType = GL_UNSIGNED_SHORT_4_4_4_4_EXT;
		}
		else
		{
			GetTexel = imageFormat[texInfo->size][texInfo->format].Get16;
			glInternalFormat = imageFormat[texInfo->size][texInfo->format].glInternalFormat16;
			glType = imageFormat[texInfo->size][texInfo->format].glType16;
		}
	}

	bpl = gSP.bgImage.width << gSP.bgImage.size >> 1;
	numBytes = bpl * gSP.bgImage.height;
	swapped = (u8*)malloc( numBytes );
#ifndef _BIG_ENDIAN
	UnswapCopy( &RDRAM[gSP.bgImage.address], swapped, numBytes );
#else // !_BIG_ENDIAN
	memcpy( swapped, &RDRAM[gSP.bgImage.address], numBytes );
#endif // _BIG_ENDIAN
	dest = (u32*)malloc( texInfo->textureBytes );

	clampSClamp = texInfo->width - 1;
	clampTClamp = texInfo->height - 1;

	j = 0;
	for (y = 0; y < texInfo->realHeight; y++)
	{
		ty = min(y, clampTClamp);

		src = &swapped[bpl * ty];

		for (x = 0; x < texInfo->realWidth; x++)
		{
			tx = min(x, clampSClamp);

			if (glInternalFormat == GL_RGBA8)
				((u32*)dest)[j++] = GetTexel( (u64*)src, tx, 0, texInfo->palette );
			else
				((u16*)dest)[j++] = GetTexel( (u64*)src, tx, 0, texInfo->palette );
		}
	}
#else // !__GX__
	if (texInfo->format == G_IM_FMT_CI)
	{
		GetTexel = imageFormat[G_TT_RGBA16][texInfo->size][texInfo->format].GetGX;
		texInfo->GXtexfmt = imageFormat[G_TT_RGBA16][texInfo->size][texInfo->format].GXtexfmt;
		GXsize = imageFormat[G_TT_RGBA16][texInfo->size][texInfo->format].GXsize;
	}
	else
	{
		GetTexel = imageFormat[G_TT_NONE][texInfo->size][texInfo->format].GetGX;
		texInfo->GXtexfmt = imageFormat[G_TT_NONE][texInfo->size][texInfo->format].GXtexfmt;
		GXsize = imageFormat[G_TT_NONE][texInfo->size][texInfo->format].GXsize;
	}
	/* [Xbox360] [Resident Evil 2] El fondo que se dibuja sobre el depth image:
	 * la Z del N64 tal cual.  Ver GXGetRE2Z_RGBA8. */
	if (_texZRE2( texInfo->size ))
	{
		GetTexel = GXGetRE2Z_RGBA8;
		texInfo->GXtexfmt = GX_TF_RGBA8;
		GXsize = 4;
	}

	//If realWidth or realHeight do not match the GX texture block size, then specify a compatible blocksize
	int blockWidth = (GXsize == 1) ? 8 : 4;
	int blockHeight = 4;
	if(texInfo->realWidth % blockWidth || texInfo->realWidth == 0)
	{
		texInfo->GXrealWidth = texInfo->realWidth + blockWidth - (texInfo->realWidth % blockWidth);
//		sprintf(txtbuffer,"texture:GXrealWidth = %d, realWidth = %d",texInfo->GXrealWidth,texInfo->realWidth);
//		DEBUG_print(txtbuffer,DBG_TXINFO1);
	}
	else
		texInfo->GXrealWidth = texInfo->realWidth;
	if(texInfo->realHeight % blockHeight || texInfo->realHeight == 0)
	{
		texInfo->GXrealHeight = texInfo->realHeight + blockHeight - (texInfo->realHeight % blockHeight);
//		sprintf(txtbuffer,"texture:GXrealHeight = %d, realHeight = %d",texInfo->GXrealHeight,texInfo->realHeight);
//		DEBUG_print(txtbuffer,DBG_TXINFO1);
	}
	else
		texInfo->GXrealHeight = texInfo->realHeight;

	texInfo->textureBytes = (texInfo->GXrealWidth * texInfo->GXrealHeight) * GXsize;
//	texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) * GXsize;

	bpl = gSP.bgImage.width << gSP.bgImage.size >> 1;
	numBytes = bpl * gSP.bgImage.height;
//	swapped = (u8*)malloc( numBytes );
//	UnswapCopy( &RDRAM[gSP.bgImage.address], swapped, numBytes );
//	dest = (u32*)malloc( texInfo->textureBytes );

	if (texInfo->textureBytes > 0)
	{
		dest = (u8*) __lwp_heap_allocate(GXtexCache,texInfo->textureBytes);
		while (!dest && TextureCache_FreeOneTexture())
			dest = (u8*) __lwp_heap_allocate(GXtexCache,texInfo->textureBytes);
	}
#ifdef SHOW_DEBUG
	else
		DEBUG_print((char*)"Textures: Trying to malloc a 0 byte GX texture",DBG_TXINFO);
#endif

	if (texInfo->textureBytes > 0 && dest == NULL)
	{
		texInfo->GXtexture = NULL;
		texInfo->textureBytes = 0;
		return;
	}

	const u32 wrapS = (texInfo->width  > 0) ? texInfo->width  : 1;
	const u32 wrapT = (texInfo->height > 0) ? texInfo->height : 1;
	clampSClamp = texInfo->width - 1;
	clampTClamp = texInfo->height - 1;

	if (!cache.enable2xSaI)
	{
		texInfo->GXtexture = (u16*) dest;

		switch(GXsize)
		{
		case 1:	// 1 byte per GX texel -> GXGetIA31_IA4, GXGetI4_IA4, GXGetIA44_IA4
			for (y = 0; y < texInfo->realHeight; y+=4)
			{
				for (x = 0; x < texInfo->realWidth; x+=8)
				{
					j = 0;
					GX_DCBZ32(dest);	/* [Xbox360] asm de GCC */
					for (k = 0; k < 4; k++)
					{
						ty = (y+k) % wrapT;
						src = &RDRAM[gSP.bgImage.address + (bpl * ty)];
						for (l = 0; l < 8; l++)
						{
							tx = (x+l) % wrapS;
							((u8*)dest)[j++] = (u8) GetTexel( (u64*)src, tx, 0, texInfo->palette );
						}
					}
					GX_DCBF32(dest);	/* [Xbox360] asm de GCC */
					dest += 32;
				}
			}
			_sync();
			break;
		case 2: // 2 bytes per GX texel -> GXGetCI4RGBA_RGB5A3, GXGetCI8RGBA_RGB5A3, GXGetI8_IA8, GXGetRGBA5551_RGB5A3, GXGetIA88_IA8, GXGetCI4IA_IA8, GXGetCI8IA_IA8
			for (y = 0; y < texInfo->realHeight; y+=4)
			{
				for (x = 0; x < texInfo->realWidth; x+=4)
				{
					j = 0;
					GX_DCBZ32(dest);	/* [Xbox360] asm de GCC */
					for (k = 0; k < 4; k++)
					{
						ty = (y+k) % wrapT;
						src = &RDRAM[gSP.bgImage.address + (bpl * ty)];
						for (l = 0; l < 4; l++)
						{
							tx = (x+l) % wrapS;
							((u16*)dest)[j++] = (u16) GetTexel( (u64*)src, tx, 0, texInfo->palette );
						}
					}
					GX_DCBF32(dest);	/* [Xbox360] asm de GCC */
					dest += 32;
				}
			}
			_sync();
			break;
		case 4: // 4 bytes per GX texel -> GXGetRGBA8888_RGBA8
			for (y = 0; y < texInfo->realHeight; y+=4)
			{
				for (x = 0; x < texInfo->realWidth; x+=4)
				{
					j = 0;
					GX_DCBZ32(&dest[0]);	/* [Xbox360] asm de GCC */
					GX_DCBZ32(&dest[32]);	/* [Xbox360] asm de GCC */
					for (k = 0; k < 4; k++)
					{
						ty = (y+k) % wrapT;
						src = &RDRAM[gSP.bgImage.address + (bpl * ty)];
						for (l = 0; l < 4; l++)
						{
							tx = (x+l) % wrapS;
							((u16*)dest)[j] =		(u16) GetTexel( (u64*)src, tx, 0, 0 );	// AARR texels
							((u16*)dest)[j+16] =	(u16) GetTexel( (u64*)src, tx, 0, 1 );	// GGBB texels -> next 32B block
							j++;
						}
					}
					GX_DCBF32(&dest[0]);	/* [Xbox360] asm de GCC */
					GX_DCBF32(&dest[32]);	/* [Xbox360] asm de GCC */
					dest += 64;
				}
			}
			_sync();
			break;
#ifdef SHOW_DEBUG
		default:
			DEBUG_print((char*)"Textures: Converting Invalid Texture Format",DBG_TXINFO);
#endif
		}
	}
	else //	!cache.enable2xSaI
	{
		j = 0;
		for (y = 0; y < texInfo->realHeight; y++)
		{
			ty = y % wrapT;
			src = &RDRAM[gSP.bgImage.address + (bpl * ty)];
	
			for (x = 0; x < texInfo->realWidth; x++)
			{
				tx = x % wrapS;

				if (GXsize == 1)		// 1 byte per GX texel -> GXGetIA31_IA4, GXGetI4_IA4, GXGetIA44_IA4
					((u8*)dest)[j++] = (u8) GetTexel( (u64*)src, tx, 0, texInfo->palette );
				else if (GXsize == 2)	// 2 bytes per GX texel -> GXGetCI4RGBA_RGB5A3, GXGetCI8RGBA_RGB5A3, GXGetI8_IA8, GXGetRGBA5551_RGB5A3, GXGetIA88_IA8, GXGetCI4IA_IA8, GXGetCI8IA_IA8
					((u16*)dest)[j++] = (u16) GetTexel( (u64*)src, tx, 0, texInfo->palette );
				else if (GXsize == 4)	// 4 bytes per GX texel -> GXGetRGBA8888_RGBA8
				{
					((u16*)dest)[j++] =	(u16) GetTexel( (u64*)src, tx, 0, 0 );	// AARR texels
					((u16*)dest)[j++] =	(u16) GetTexel( (u64*)src, tx, 0, 1 );	// GGBB texels -> next 32B block
				}
			}
		}

		static InterpolatorGXIA4	iGXIA4;
		static InterpolatorGXIA8	iGXIA8;
		static InterpolatorGXRGB5A3	iGXRGB5A3;
		static InterpolatorGXRGBA8	iGXRGBA8;
		
		texInfo->textureBytes <<= 2;

		scaledDest = (u8*) __lwp_heap_allocate(GXtexCache,texInfo->textureBytes);
		while (!scaledDest && TextureCache_FreeOneTexture())
			scaledDest = (u8*) __lwp_heap_allocate(GXtexCache,texInfo->textureBytes);

		if (!scaledDest)
		{
			__lwp_heap_free(GXtexCache, dest);
			texInfo->GXtexture = NULL;
			texInfo->textureBytes = 0;
			return;
		}

		Interpolator* interpolator;
		if (texInfo->GXtexfmt == GX_TF_IA4)
			interpolator = &iGXIA4;
		else if (texInfo->GXtexfmt == GX_TF_IA8)
			interpolator = &iGXIA8;
		else if (texInfo->GXtexfmt == GX_TF_RGB5A3)
			interpolator = &iGXRGB5A3;
		else //(texInfo->GXtexfmt == GX_TF_RGBA8)
			interpolator = &iGXRGBA8;
		
		_2xSaI( dest, scaledDest,
		        texInfo->realWidth, texInfo->realHeight,
		        texInfo->clampS, texInfo->clampT,
		        interpolator );

		texInfo->GXtexture = (u16*) scaledDest;
		__lwp_heap_free(GXtexCache, dest);
		DCFlushRange(scaledDest, texInfo->textureBytes);
	}	//	cache.enable2xSaI
#endif // __GX__

#ifndef __GX__
	if (cache.enable2xSaI)
	{
		static Interpolator8888 i8888;
		static Interpolator4444 i4444;
		static Interpolator5551 i5551;
		
		texInfo->textureBytes <<= 2;

		scaledDest = (u32*)malloc( texInfo->textureBytes );

		Interpolator* interpolator;
		if (glInternalFormat == GL_RGBA8)
			interpolator = &i8888;
		else if (glInternalFormat == GL_RGBA4)
			interpolator = &i4444;
		else
			interpolator = &i5551;
		
		_2xSaI( dest, scaledDest,
		        texInfo->realWidth, texInfo->realHeight,
		        texInfo->clampS, texInfo->clampT,
		        interpolator );

		glTexImage2D( GL_TEXTURE_2D, 0, glInternalFormat, texInfo->realWidth << 1, texInfo->realHeight << 1, 0, GL_RGBA, glType, scaledDest );

		free( swapped );
		free( dest );
		free( scaledDest );
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, glInternalFormat, texInfo->realWidth, texInfo->realHeight, 0, GL_RGBA, glType, dest );

		free (swapped );
		free( dest );
	}
#endif // __GX__
}

// Fixes for ZSortBOSS (WDC/Stunt Racer) 2D screens
bool TextureCache_UseMirrorFixes()
{
	return (GBI.current != NULL) && (GBI.current->type == ZSortBOSS);
}

void TextureCache_Load( CachedTexture *texInfo )
{
	GLN64_PROF_SCOPE(tcload);  /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	u8 *dest = NULL, *scaledDest;
#ifndef __GX__
	GLuint			glInternalFormat;
	GLenum			glType;
#else // !__GX__
	u32	GXsize, k, l;
#endif // __GX__

	u64 *src;
	u16 x, y, i, j, tx, ty, line;
	u16 mirrorSBit, maskSMask, clampSClamp, postClampS;
	u16 mirrorTBit, maskTMask, clampTClamp, postClampT;
	GetTexelFunc	GetTexel;

#ifndef __GX__
	if (((imageFormat[texInfo->size][texInfo->format].autoFormat == GL_RGBA8) || 
		((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16)) || (cache.bitDepth == 2)) && (cache.bitDepth != 0))
	{
	 	texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) << 2;
		if ((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16))
		{
			if (texInfo->size == G_IM_SIZ_4b)
				GetTexel = GetCI4IA_RGBA8888;
			else
				GetTexel = GetCI8IA_RGBA8888;

			glInternalFormat = GL_RGBA8;
			glType = GL_UNSIGNED_BYTE;
		}
		else
		{
			GetTexel = imageFormat[texInfo->size][texInfo->format].Get32;
			glInternalFormat = imageFormat[texInfo->size][texInfo->format].glInternalFormat32;
			glType = imageFormat[texInfo->size][texInfo->format].glType32;
		}
	}
	else
	{
	 	texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) << 1;
		if ((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16))
		{
			if (texInfo->size == G_IM_SIZ_4b)
				GetTexel = GetCI4IA_RGBA4444;
			else
				GetTexel = GetCI8IA_RGBA4444;

			glInternalFormat = GL_RGBA4;
			glType = GL_UNSIGNED_SHORT_4_4_4_4_EXT;
		}
		else
		{
			GetTexel = imageFormat[texInfo->size][texInfo->format].Get16;
			glInternalFormat = imageFormat[texInfo->size][texInfo->format].glInternalFormat16;
			glType = imageFormat[texInfo->size][texInfo->format].glType16;
		}
	}
	dest = (u32*)malloc( texInfo->textureBytes );
#else // !__GX__
	GetTexel = imageFormat[gDP.otherMode.textureLUT][texInfo->size][texInfo->format].GetGX;
	texInfo->GXtexfmt = imageFormat[gDP.otherMode.textureLUT][texInfo->size][texInfo->format].GXtexfmt;
	GXsize = imageFormat[gDP.otherMode.textureLUT][texInfo->size][texInfo->format].GXsize;
	/* [Xbox360] [Resident Evil 2] Lo que se dibuja sobre el depth image: la Z
	 * del N64 tal cual (GLideN64 TextureCache::_load, hack_LoadDepthTextures).
	 * Ver GXGetRE2Z_RGBA8. */
	if (_texZRE2( texInfo->size ))
	{
		GetTexel = GXGetRE2Z_RGBA8;
		texInfo->GXtexfmt = GX_TF_RGBA8;
		GXsize = 4;
	}

	//If realWidth or realHeight do not match the GX texture block size, then specify a compatible blocksize
	int blockWidth = (GXsize == 1) ? 8 : 4;
	int blockHeight = 4;
	if(texInfo->realWidth % blockWidth || texInfo->realWidth == 0)
	{
		texInfo->GXrealWidth = texInfo->realWidth + blockWidth - (texInfo->realWidth % blockWidth);
//		sprintf(txtbuffer,"texture:GXrealWidth = %d, realWidth = %d",texInfo->GXrealWidth,texInfo->realWidth);
//		DEBUG_print(txtbuffer,DBG_TXINFO1); 
	}
	else
		texInfo->GXrealWidth = texInfo->realWidth;
	if(texInfo->realHeight % blockHeight || texInfo->realHeight == 0)
	{
		texInfo->GXrealHeight = texInfo->realHeight + blockHeight - (texInfo->realHeight % blockHeight);
//		sprintf(txtbuffer,"texture:GXrealHeight = %d, realHeight = %d",texInfo->GXrealHeight,texInfo->realHeight);
//		DEBUG_print(txtbuffer,DBG_TXINFO1); 
	}
	else
		texInfo->GXrealHeight = texInfo->realHeight;

	texInfo->textureBytes = (texInfo->GXrealWidth * texInfo->GXrealHeight) * GXsize;
	if (texInfo->textureBytes > 0)
	{
		dest = (u8*) __lwp_heap_allocate(GXtexCache,texInfo->textureBytes);
		while (!dest && TextureCache_FreeOneTexture())
			dest = (u8*) __lwp_heap_allocate(GXtexCache,texInfo->textureBytes);
	}
#ifdef SHOW_DEBUG
	else
		DEBUG_print((char*)"Textures: Trying to malloc a 0 byte GX texture",DBG_TXINFO);
#endif

	if (texInfo->textureBytes > 0 && dest == NULL)
	{
		texInfo->GXtexture = NULL;
		texInfo->textureBytes = 0;
		return;
	}

#endif // __GX__

	line = texInfo->line;

	if (((texInfo->format == G_IM_FMT_YUV) && (texInfo->size == G_IM_SIZ_16b)) || (texInfo->size == G_IM_SIZ_32b))
		line <<= 1;

	const bool mirrorFix = TextureCache_UseMirrorFixes();
	const u16 dataClampS = (u16)(texInfo->clampS ? texInfo->clampWidth - 1 : (texInfo->width << 1) - 1);
	const u16 dataClampT = (u16)(texInfo->clampT ? texInfo->clampHeight - 1 : (texInfo->height << 1) - 1);

	if (texInfo->maskS)
	{
		if (mirrorFix && texInfo->mirrorS)
		{
			clampSClamp = 0xFFFF;
			postClampS = dataClampS;
		}
		/* [Xbox360] ★ [Killer Instinct, nariz del hombre lobo] TAMBIEN CON
		 * ESPEJO.  En la consola, con mascara y sin clamp no hay clamp: la
		 * coordenada se enmascara (y se refleja) y lee la TMEM tal cual.  La
		 * rama de Wii64 lo hacia solo sin espejo; con espejo se quedaba en
		 * `clampSClamp = 2*width - 1`, y cuando la mascara es mayor que el
		 * ancho todo lo que pasa de ahi dentro del periodo (2 * mascara) repite
		 * una columna.  Medido con [TRFUERA] (log 8): `tex 43x42 real 128x64
		 * mask 6 cl 0` con el texrect en s 212.5-256.4, o sea x 84-128 del
		 * periodo de 128; de 86 a 127 salia la columna 42 -- el tercio derecho
		 * de tex_69, la barra encima de la nariz.  La consola ahi lee 41..0.
		 * Solo el eje S: es el medido.  ZSortBOSS sigue por la rama de arriba. */
		else if (!texInfo->clampS &&
				 (u32)(1 << texInfo->maskS) > texInfo->width)
		{
			clampSClamp = 0xFFFF;
			postClampS = 0xFFFF;
		}
		else
		{
			clampSClamp = texInfo->clampS ? texInfo->clampWidth - 1
				: (texInfo->mirrorS ? (u16)((texInfo->width << 1) - 1) : (u16)(texInfo->width - 1));
			postClampS = 0xFFFF;
		}
		maskSMask = (1 << texInfo->maskS) - 1;
		mirrorSBit = texInfo->mirrorS ? 1 << texInfo->maskS : 0;
	}
	else
	{
		clampSClamp = texInfo->clampS ? texInfo->clampWidth - 1 : texInfo->width - 1;
		postClampS = 0xFFFF;
		maskSMask = 0xFFFF;
		mirrorSBit = 0x0000;
	}

	if (texInfo->maskT)
	{
		if (mirrorFix && texInfo->mirrorT)
		{
			clampTClamp = 0xFFFF;
			postClampT = dataClampT;
		}
		else if (!texInfo->clampT && !texInfo->mirrorT &&
				 (u32)(1 << texInfo->maskT) > texInfo->height)
		{
			// Same as the S axis above.
			clampTClamp = 0xFFFF;
			postClampT = 0xFFFF;
		}
		else
		{
			clampTClamp = texInfo->clampT ? texInfo->clampHeight - 1
				: (texInfo->mirrorT ? (u16)((texInfo->height << 1) - 1) : (u16)(texInfo->height - 1));
			postClampT = 0xFFFF;
		}
		maskTMask = (1 << texInfo->maskT) - 1;
		mirrorTBit = texInfo->mirrorT ?	1 << texInfo->maskT : 0;
	}
	else
	{
		clampTClamp = texInfo->clampT ? texInfo->clampHeight - 1 : texInfo->height - 1;
		postClampT = 0xFFFF;
		maskTMask = 0xFFFF;
		mirrorTBit = 0x0000;
	}

#ifdef __GX__
	texInfo->GXtexture = (u16*) dest;

	if (!cache.enable2xSaI)
	{
		switch(GXsize)
		{
		case 1:	// 1 byte per GX texel -> GXGetIA31_IA4, GXGetI4_IA4, GXGetIA44_IA4
			for (y = 0; y < texInfo->realHeight; y+=4)
			{
				for (x = 0; x < texInfo->realWidth; x+=8)
				{
					j = 0;
					GX_DCBZ32(dest);	/* [Xbox360] asm de GCC */
					for (k = 0; k < 4; k++)
					{
						ty = min(y+k, clampTClamp) & maskTMask;
						if ((y+k) & mirrorTBit)
							ty ^= maskTMask;
						ty = min(ty, postClampT);
						src = &TMEM[(texInfo->tMem + line * ty) & 0x1FF];
						i = (ty & 1) << 1;
						for (l = 0; l < 8; l++)
						{
							tx = min(x+l, clampSClamp) & maskSMask;
							if ((x+l) & mirrorSBit)
								tx ^= maskSMask;
							tx = min(tx, postClampS);
							((u8*)dest)[j++] = (u8) GetTexel( src, tx, i, texInfo->palette );
						}
					}
					GX_DCBF32(dest);	/* [Xbox360] asm de GCC */
					dest += 32;
				}
			}
			_sync();
			break;
		case 2: // 2 bytes per GX texel -> GXGetCI4RGBA_RGB5A3, GXGetCI8RGBA_RGB5A3, GXGetI8_IA8, GXGetRGBA5551_RGB5A3, GXGetIA88_IA8, GXGetCI4IA_IA8, GXGetCI8IA_IA8
			for (y = 0; y < texInfo->realHeight; y+=4)
			{
				for (x = 0; x < texInfo->realWidth; x+=4)
				{
					j = 0;
					GX_DCBZ32(dest);	/* [Xbox360] asm de GCC */
					for (k = 0; k < 4; k++)
					{
						ty = min(y+k, clampTClamp) & maskTMask;
						if ((y+k) & mirrorTBit)
							ty ^= maskTMask;
						ty = min(ty, postClampT);
						src = &TMEM[(texInfo->tMem + line * ty) & 0x1FF];
						i = (ty & 1) << 1;
						for (l = 0; l < 4; l++)
						{
							tx = min(x+l, clampSClamp) & maskSMask;
							if ((x+l) & mirrorSBit)
								tx ^= maskSMask;
							tx = min(tx, postClampS);
							((u16*)dest)[j++] = (u16) GetTexel( src, tx, i, texInfo->palette );
						}
					}
					GX_DCBF32(dest);	/* [Xbox360] asm de GCC */
					dest += 32;
				}
			}
			_sync();
			break;
		case 4: // 4 bytes per GX texel -> GXGetRGBA8888_RGBA8
			for (y = 0; y < texInfo->realHeight; y+=4)
			{
				for (x = 0; x < texInfo->realWidth; x+=4)
				{
					j = 0;
					GX_DCBZ32(&dest[0]);	/* [Xbox360] asm de GCC */
					GX_DCBZ32(&dest[32]);	/* [Xbox360] asm de GCC */
					for (k = 0; k < 4; k++)
					{
						ty = min(y+k, clampTClamp) & maskTMask;
						if ((y+k) & mirrorTBit)
							ty ^= maskTMask;
						ty = min(ty, postClampT);
						src = &TMEM[(texInfo->tMem + line * ty) & 0x1FF];
						i = (ty & 1) << 1;
						for (l = 0; l < 4; l++)
						{
							tx = min(x+l, clampSClamp) & maskSMask;
							if ((x+l) & mirrorSBit)
								tx ^= maskSMask;
							tx = min(tx, postClampS);
							((u16*)dest)[j] =		(u16) GetTexel( src, tx, i, 0 );	// AARR texels
							((u16*)dest)[j+16] =	(u16) GetTexel( src, tx, i, 1 );	// GGBB texels -> next 32B block
							j++;
						}
					}
					GX_DCBF32(&dest[0]);	/* [Xbox360] asm de GCC */
					GX_DCBF32(&dest[32]);	/* [Xbox360] asm de GCC */
					dest += 64;
				}
			}
			_sync();
			break;
#ifdef SHOW_DEBUG
		default:
			DEBUG_print((char*)"Textures: Converting Invalid Texture Format",DBG_TXINFO);
#endif
		}
	}
	else if (texInfo->textureBytes > 0) //	!cache.enable2xSaI
	{
		j = 0;
		for (y = 0; y < texInfo->realHeight; y++)
		{
			ty = min(y, clampTClamp) & maskTMask;
	
			if (y & mirrorTBit)
				ty ^= maskTMask;
			ty = min(ty, postClampT);
	
			src = &TMEM[(texInfo->tMem + line * ty) & 0x1FF];
	
			i = (ty & 1) << 1;
			for (x = 0; x < texInfo->realWidth; x++)
			{
				tx = min(x, clampSClamp) & maskSMask;
	
				if (x & mirrorSBit)
					tx ^= maskSMask;
				tx = min(tx, postClampS);

				if (GXsize == 1)		// 1 byte per GX texel -> GXGetIA31_IA4, GXGetI4_IA4, GXGetIA44_IA4
					((u8*)dest)[j++] = GetTexel( src, tx, i, texInfo->palette );
				else if (GXsize == 2)	// 2 bytes per GX texel -> GXGetCI4RGBA_RGB5A3, GXGetCI8RGBA_RGB5A3, GXGetI8_IA8, GXGetRGBA5551_RGB5A3, GXGetIA88_IA8, GXGetCI4IA_IA8, GXGetCI8IA_IA8
					((u16*)dest)[j++] = GetTexel( src, tx, i, texInfo->palette );
				else if (GXsize == 4)	// 4 bytes per GX texel -> GXGetRGBA8888_RGBA8
				{
					((u16*)dest)[j++] =	(u16) GetTexel( src, tx, i, 0 );	// AARR texels
					((u16*)dest)[j++] =	(u16) GetTexel( src, tx, i, 1 );	// GGBB texels -> next 32B block
				}
			}
		}

		static InterpolatorGXIA4	iGXIA4;
		static InterpolatorGXIA8	iGXIA8;
		static InterpolatorGXRGB5A3	iGXRGB5A3;
		static InterpolatorGXRGBA8	iGXRGBA8;
		
		texInfo->textureBytes <<= 2;

		scaledDest = (u8*) __lwp_heap_allocate(GXtexCache,texInfo->textureBytes);
		while (!scaledDest && TextureCache_FreeOneTexture())
			scaledDest = (u8*) __lwp_heap_allocate(GXtexCache,texInfo->textureBytes);

		if (!scaledDest)
		{
			__lwp_heap_free(GXtexCache, dest);
			texInfo->GXtexture = NULL;
			texInfo->textureBytes = 0;
			return;
		}

		Interpolator* interpolator;
		if (texInfo->GXtexfmt == GX_TF_IA4)
			interpolator = &iGXIA4;
		else if (texInfo->GXtexfmt == GX_TF_IA8)
			interpolator = &iGXIA8;
		else if (texInfo->GXtexfmt == GX_TF_RGB5A3)
			interpolator = &iGXRGB5A3;
		else // (texInfo->GXtexfmt == GX_TF_RGBA8)
			interpolator = &iGXRGBA8;
		
		_2xSaI( dest, scaledDest,
		        texInfo->realWidth, texInfo->realHeight,
		        1, 1,
		        interpolator );

		texInfo->GXtexture = (u16*) scaledDest;
		__lwp_heap_free(GXtexCache, dest);
		DCFlushRange(scaledDest, texInfo->textureBytes);
	} 	//	cache.enable2xSaI
#else // __GX__
	j = 0;
	for (y = 0; y < texInfo->realHeight; y++)
	{
		ty = min(y, clampTClamp) & maskTMask;

		if (y & mirrorTBit)
			ty ^= maskTMask;
		ty = min(ty, postClampT);

		src = &TMEM[texInfo->tMem] + line * ty;

		i = (ty & 1) << 1;
		for (x = 0; x < texInfo->realWidth; x++)
		{
			tx = min(x, clampSClamp) & maskSMask;

			if (x & mirrorSBit)
				tx ^= maskSMask;
			tx = min(tx, postClampS);

			if (glInternalFormat == GL_RGBA8)
				((u32*)dest)[j++] = GetTexel( src, tx, i, texInfo->palette );
			else
				((u16*)dest)[j++] = GetTexel( src, tx, i, texInfo->palette );
		}
	}
	if (cache.enable2xSaI)
	{
		static Interpolator8888 i8888;
		static Interpolator4444 i4444;
		static Interpolator5551 i5551;
		
		texInfo->textureBytes <<= 2;

		scaledDest = (u32*)malloc( texInfo->textureBytes );

		Interpolator* interpolator;
		if (glInternalFormat == GL_RGBA8)
			interpolator = &i8888;
		else if (glInternalFormat == GL_RGBA4)
			interpolator = &i4444;
		else
			interpolator = &i5551;
		
		_2xSaI( dest, scaledDest,
		        texInfo->realWidth, texInfo->realHeight,
		        1, 1,
		        interpolator );

		glTexImage2D( GL_TEXTURE_2D, 0, glInternalFormat, texInfo->realWidth << 1, texInfo->realHeight << 1, 0, GL_RGBA, glType, scaledDest );

		free( dest );
		free( scaledDest );
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, glInternalFormat, texInfo->realWidth, texInfo->realHeight, 0, GL_RGBA, glType, dest );

		free( dest );
	}
#endif // !__GX__
}

u32 TextureCache_CalculateCRC( u32 t, u32 width, u32 height )
{
	GLN64_PROF_SCOPE(tccrc);   /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	u32 crc;
	u32 y, bpl, line;
	u64 *src;

	bpl = width << gSP.textureTile[t]->size >> 1;

	line = gSP.textureTile[t]->line;
 	if (gSP.textureTile[t]->size == G_IM_SIZ_32b)
		line <<= 1;

	crc = 0xFFFFFFFF;

	/* [Xbox360] ★★ UNA LLAMADA EN VEZ DE UNA POR FILA, CUANDO LAS FILAS SON
	 * CONTIGUAS.
	 *
	 * El bucle de abajo llama a `Hash_Calculate` UNA VEZ POR FILA.  Una
	 * textura de 32 filas son 32 invocaciones de XXH32 de ~40 bytes, cada una
	 * pagando inicializacion y avalancha.  Medido en OoT tras cachear la
	 * paleta: 9.459 KB/s hasheados en 32.580 us/s, o sea 3,44 us/KB cuando
	 * XXH32 deberia ir por encima de 800 MB/s (~1,2 us/KB).  Los otros dos
	 * tercios son coste POR LLAMADA.
	 *
	 * ★ LA REFERENCIA NO TIENE EL BUCLE: GLideN64 calcula
	 * `_bytes = height * (line << 3)` y hashea de una sola vez
	 * (src/Textures.cpp:1841-1855).
	 *
	 * ★★ PERO NO SE PORTA SU VERSION, Y ESA ES LA DECISION IMPORTANTE.
	 * GLideN64 hashea `height * line*8` bytes SEGUIDOS, o sea que incluye el
	 * relleno entre filas cuando `bpl < line*8`, y resuelve la vuelta de la
	 * TMEM recortando en vez de envolviendo.  Son CONJUNTOS DE BYTES
	 * DISTINTOS de los que mira este arbol, y el CRC es la CLAVE DE LA CACHE:
	 * cambiar que bytes entran es cambiar a que textura se parece cada
	 * textura.  Eso no se hace a ciegas.
	 *
	 * Asi que el camino rapido solo se toma cuando es DEMOSTRABLEMENTE la
	 * misma entrada byte a byte:
	 *
	 *   lineBytes == bpl   las filas son contiguas: lo que se salta el bucle
	 *                      entre fila y fila es exactamente cero.
	 *   sin vuelta         la ultima fila termina dentro de los 4 KB, asi que
	 *                      ningun `& 0x1FF` llega a envolver.
	 *   bpl != 0           una textura degenerada se va por el camino viejo y
	 *                      se comporta igual que siempre.
	 *
	 * Con esas tres, los bytes que entran al hash son LOS MISMOS.
	 *
	 * ★ El VALOR del crc si cambia -- XXH32 no es asociativo, hashear A||B de
	 * una vez no da lo mismo que encadenar A y luego B.  Es inocuo por el
	 * mismo motivo que en la paleta: el crc solo se compara con otros
	 * calculados igual y la cache se reconstruye en cada sesion.  Lo que hay
	 * que conservar es la INYECTIVIDAD (misma textura -> mismo crc, textura
	 * distinta -> crc distinto), y eso lo da el conjunto de bytes.
	 *
	 * ★ Y una textura NO puede alternar de camino: la condicion depende solo
	 * de `tmem`, `line`, `bpl` y `height`, que son el descriptor del tile.
	 * Con el mismo descriptor se toma siempre el mismo camino. */
	{
		const u32 tmem0     = gSP.textureTile[t]->tmem & 0x1FF;
		const u32 lineBytes = line << 3;
		const u32 total     = bpl * height;

		if (bpl != 0 && lineBytes == bpl && ((tmem0 << 3) + total) <= 4096)
		{
			crc = Hash_Calculate( crc, &TMEM[tmem0], total );
			GLN64_PROF_BYTES(crcbytes, total);
			GLN64_PROF_CNT(crccalls);
		}
		else
		{
			GLN64_PROF_CNT(crclento);
			for (y = 0; y < height; y++)
			{
				const u32 offset = (gSP.textureTile[t]->tmem + line * y) & 0x1FF;
				const u32 avail = (512 - offset) << 3;
				src = (u64*)&TMEM[offset];
				crc = Hash_Calculate( crc, src, (bpl < avail) ? bpl : avail );
				GLN64_PROF_BYTES(crcbytes, (bpl < avail) ? bpl : avail);
				GLN64_PROF_CNT(crccalls);
			}
		}
	}

	/* [Xbox360] ★★ LA PALETA ENTRA POR SU CRC PRECALCULADO, NO POR SUS BYTES.
	 *
	 * Antes se rehasheaban 2.048 bytes (8b/16b) o 128 (4b) EN CADA CONSULTA.
	 * Medido en OoT: 0,84 ms de cada frame, y casi todo eran los mismos 2 KB
	 * una y otra vez.  Ahora son 4.  El calculo vive en `gDPLoadTLUT`, que es
	 * donde la paleta cambia; el porque largo y el limite conocido estan en
	 * `gDP.paletteCRC16` (gDP.h).  Portado de GLideN64 src/Textures.cpp:1862.
	 *
	 * ★ `crcbytes` sigue contando lo que se hashea DE VERDAD, que es lo que
	 * hace comparable el MB/s de `[GLN64H]` entre el antes y el despues. */
   	if ((gDP.otherMode.textureLUT != G_TT_NONE) || (gSP.textureTile[t]->format == G_IM_FMT_CI))
	{
		if (gSP.textureTile[t]->size == G_IM_SIZ_4b)
		{
			crc = Hash_Calculate( crc, &gDP.paletteCRC16[gSP.textureTile[t]->palette], sizeof(u32) );
			GLN64_PROF_BYTES(crcbytes, sizeof(u32));
			GLN64_PROF_CNT(crccalls);
			GLN64_PROF_CNT(pal4);
		}
		else if (gSP.textureTile[t]->size == G_IM_SIZ_8b)
		{
			crc = Hash_Calculate( crc, &gDP.paletteCRC256, sizeof(u32) );
			GLN64_PROF_BYTES(crcbytes, sizeof(u32));
			GLN64_PROF_CNT(crccalls);
			GLN64_PROF_CNT(pal8);
		}
		/* ★ LA RAMA DE 16b ES DE glN64/Wii64: GLideN64 NO LA TIENE (solo 4b y
		 * 8b, src/Textures.cpp:1863-1866).  Aqui se CONSERVA -- la paleta
		 * sigue entrando en el CRC, que es el comportamiento de este arbol--
		 * y solo cambia COMO se calcula, igual que en 8b.  Eso es decision
		 * mia, no de la referencia, y por eso lleva su propio contador: si
		 * `pal16` sale a cero, la rama no se usa y se puede quitar en una
		 * pasada aparte; si sale alta, hay que mirarla con cuidado antes de
		 * tocarla ([[reference_wii64_sobra_vs_gliden64]]). */
		else if (gSP.textureTile[t]->size == G_IM_SIZ_16b)
		{
			crc = Hash_Calculate( crc, &gDP.paletteCRC256, sizeof(u32) );
			GLN64_PROF_BYTES(crcbytes, sizeof(u32));
			GLN64_PROF_CNT(crccalls);
			GLN64_PROF_CNT(pal16);
		}
	}
	return crc;
}

#if defined(__GX__) && XBOX64_TRAZAS
/* [Xbox360] ★ EL DETALLE DEL VOLCADO DE TEXTURAS (L3+R3), parte fija de la
 * herramienta TEXBMP (xenon/gx_d3d9.cpp), solo con XBOX64_TRAZAS.  En el frame
 * del volcado (gxd3d_tbmp_activo), una vez por textura y con el MISMO puntero
 * que el nombre del BMP (`tex_NN_fF_WxH_dPUNTERO`):
 *   [TEXPAR]   direccion, formato/tamano, WxH, TMEM, linea, paleta, crc, fb;
 *   [TEXPAR2]  mascaras, clamp/mirror, tamanos (clamp, real, GX) y formato GX;
 *   [TEXPAR3]  el tile con que se activa (TMEM, linea, rect, mascaras, shift,
 *              cm, tipo de carga, imagen);
 *   [TEXPAR4]  la ultima carga que la TMEM recuerda en esa posicion, la TLUT y
 *              el CRC de la sub-paleta;
 *   [TEXPAR5]  el estado de dibujo: combinador, othermode H/L, geometry mode,
 *              PRIM y ENV;
 * y tres .bin de 4 KB en el mismo directorio: tmem_ (la TMEM), img_ (RDRAM
 * desde la imagen del ultimo load del tile) y dir_ (RDRAM desde la direccion
 * de la textura de la cache), para reconstruir a mano RDRAM -> TMEM -> texels.
 * Origen: Paper Mario, ventana negra fuera de la casa (2026-10-23): [TEXPAR5]
 * enseno la TLUT que SetOtherMode_H dejaba puesta (ver gSPSetOtherMode_H). */
extern "C" int  gxd3d_tbmp_activo(void);
extern "C" const char *gxd3d_tbmp_dir(void);
extern "C" void DebugOutputString( const char *s );

/* Un .bin crudo en el directorio del volcado: `tmem_dPUNTERO.bin` (la TMEM,
 * 4 KB), `img_dPUNTERO.bin` (4 KB de RDRAM desde la imagen del ultimo load
 * del tile) y `dir_dPUNTERO.bin` (4 KB desde la direccion de la textura de la
 * cache).  Con eso se reconstruye a mano RDRAM -> TMEM -> texels. */
static void _texSondaBin( const char *pref, const void *tex, const void *datos, u32 bytes )
{
	const char *dir = gxd3d_tbmp_dir();
	char ruta[192];
	FILE *f;
	if (dir == NULL || datos == NULL)
		return;
	sprintf( ruta, "%s%s_d%08X.bin", dir, pref, (u32)(size_t)tex );
	f = fopen( ruta, "wb" );
	if (f == NULL)
		return;
	fwrite( datos, 1, bytes, f );
	fclose( f );
}

static const void *_texSondaRdram( u32 dir )
{
	return (dir != 0 && dir + 4096 <= RDRAMSize + 1) ? (const void*)&RDRAM[dir] : NULL;
}
static void _texSondaPar( u32 t, const CachedTexture *tx )
{
	static const void *s_vistas[256];
	static u32 s_n = 0;
	static int s_antes = 0;
	const int activo = gxd3d_tbmp_activo();
	char l[200];
	u32 i;

	if (!activo) { s_antes = 0; return; }
	if (!s_antes) { s_n = 0; s_antes = 1; }   /* un volcado nuevo */
	if (tx == NULL || tx->GXtexture == NULL) return;
	for (i = 0; i < s_n; i++)
		if (s_vistas[i] == tx->GXtexture) return;
	if (s_n >= 256) return;
	s_vistas[s_n++] = tx->GXtexture;

	sprintf( l, "[TEXPAR] d%08X t%u dir=%08X fmt=%u siz=%u %ux%u tmem=%u line=%u pal=%u crc=%08X fb=%u",
	         (u32)(size_t)tx->GXtexture, t, tx->address, tx->format, tx->size, tx->width, tx->height,
	         tx->tMem, tx->line, tx->palette, tx->crc, tx->frameBufferTexture );
	DebugOutputString( l );
	sprintf( l, "[TEXPAR2] d%08X mask=%u,%u clamp=%u,%u mir=%u,%u clampWH=%ux%u real=%ux%u gx=%ux%u fmt%u",
	         (u32)(size_t)tx->GXtexture, tx->maskS, tx->maskT, tx->clampS, tx->clampT,
	         tx->mirrorS, tx->mirrorT, tx->clampWidth, tx->clampHeight,
	         tx->realWidth, tx->realHeight, tx->GXrealWidth, tx->GXrealHeight, tx->GXtexfmt );
	DebugOutputString( l );
	{
		const gDPTile *tl = gSP.textureTile[t];
		if (tl != NULL)
		{
			sprintf( l, "[TEXPAR3] d%08X tile tmem=%u line=%u s%u,%u-%u,%u mask=%u,%u shift=%u,%u cm=%u,%u ld=%u img=%08X",
			         (u32)(size_t)tx->GXtexture, tl->tmem, tl->line, tl->uls, tl->ult, tl->lrs, tl->lrt,
			         tl->masks, tl->maskt, tl->shifts, tl->shiftt, tl->cms, tl->cmt,
			         tl->loadType, tl->imageAddress );
			DebugOutputString( l );
			{
				/* La ultima carga que la TMEM recuerda en esa posicion. */
				const gDPLoadTileInfo &li = gDP.loadInfo[tl->tmem & 0x1FF];
				sprintf( l, "[TEXPAR4] d%08X carga %ux%u texW=%u dir=%08X tipo=%u siz=%u | lut=%u palCRC=%08X",
				         (u32)(size_t)tx->GXtexture, li.width, li.height, li.texWidth, li.texAddress,
				         li.loadType, li.size, gDP.otherMode.textureLUT,
				         gDP.paletteCRC16[tl->palette & 15] );
				DebugOutputString( l );
			}
			/* El estado de dibujo con que se activa: combinador, othermode,
			 * geometry mode y los colores PRIM/ENV (RGBA de 8 bits). */
			sprintf( l, "[TEXPAR5] d%08X mux=%08X:%08X H=%08X L=%08X geo=%08X prim=%02X%02X%02X%02X env=%02X%02X%02X%02X",
			         (u32)(size_t)tx->GXtexture, gDP.combine.muxs0, gDP.combine.muxs1,
			         gDP.otherMode.h, gDP.otherMode.l, gSP.geometryMode,
			         (u32)(gDP.primColor.r * 255.0f), (u32)(gDP.primColor.g * 255.0f),
			         (u32)(gDP.primColor.b * 255.0f), (u32)(gDP.primColor.a * 255.0f),
			         (u32)(gDP.envColor.r * 255.0f), (u32)(gDP.envColor.g * 255.0f),
			         (u32)(gDP.envColor.b * 255.0f), (u32)(gDP.envColor.a * 255.0f) );
			DebugOutputString( l );
			_texSondaBin( "tmem", tx->GXtexture, TMEM, 4096 );
			_texSondaBin( "img",  tx->GXtexture, _texSondaRdram( tl->imageAddress ), 4096 );
			_texSondaBin( "dir",  tx->GXtexture, _texSondaRdram( tx->address ), 4096 );
		}
	}
}
#endif

void TextureCache_ActivateTexture( u32 t, CachedTexture *texture )
{
#if defined(__GX__) && XBOX64_TRAZAS
	_texSondaPar( t, texture );
#endif
#ifndef __GX__
	// If multitexturing, set the appropriate texture
	if (OGL.ARB_multitexture)
		glActiveTextureARB( GL_TEXTURE0_ARB + t );

	// Bind the cached texture
	glBindTexture( GL_TEXTURE_2D, texture->glName );

	// Set filter mode. Almost always bilinear, but check anyways
	if ((gDP.otherMode.textureFilter == G_TF_BILERP) || (gDP.otherMode.textureFilter == G_TF_AVERAGE) || (OGL.forceBilinear))
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}

	// Set clamping modes
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture->clampS ? GL_CLAMP_TO_EDGE : GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture->clampT ? GL_CLAMP_TO_EDGE : GL_REPEAT );
#else // !__GX__
	if (!((gDP.otherMode.textureFilter == G_TF_BILERP) || (gDP.otherMode.textureFilter == G_TF_AVERAGE) || (OGL.forceBilinear)))
		OGL.GXuseMinMagNearest = true;

	if (texture->GXtexture != NULL && !OGL.GXrenderTexRect) 
	{
		if (cache.enable2xSaI && !texture->frameBufferTexture)
			GX_InitTexObj(&texture->GXtex, texture->GXtexture, (u16) texture->realWidth << 1, (u16) texture->realHeight << 1, texture->GXtexfmt, 
				texture->clampS ? GX_CLAMP : GX_REPEAT, 
				texture->clampT ? GX_CLAMP : GX_REPEAT, GX_FALSE); 
		else
			/* [Xbox360] [Worms Armageddon] Espejo por la GPU: ver GXespejoS en
			 * TextureCache_Update. */
			GX_InitTexObj(&texture->GXtex, texture->GXtexture, (u16) texture->realWidth, (u16) texture->realHeight, texture->GXtexfmt,
				texture->clampS ? GX_CLAMP : (texture->GXespejoS ? GX_MIRROR : GX_REPEAT),
				texture->clampT ? GX_CLAMP : (texture->GXespejoT ? GX_MIRROR : GX_REPEAT), GX_FALSE);
		/* [Xbox360] [Conker, sombra pixelada] UNA TEXTURA DE FRAMEBUFFER SE FILTRA
		 * COMO PIDA EL JUEGO, COMO EN GLideN64.
		 *
		 * Wii64 la muestreaba SIEMPRE con GX_NEAR.  GLideN64 la trata como a
		 * cualquier otra: bilineal si `gDP.otherMode.textureFilter` no es POINT
		 * (TextureCache::activateTexture, src/Textures.cpp:1894).  Y su
		 * `hack_fbTextureOffset` -- que ya esta portado, FrameBuffer.cpp, y que
		 * lleva Conker -- solo actua cuando el filtro NO es POINT: esta hecho
		 * para este muestreo.  Con GX_NEAR la silueta de 64x64 se estiraba
		 * sobre el suelo en bloques.
		 *
		 * Sin anisotropia ni LOD: el buffer no tiene mipmaps.  Este es el camino
		 * de los triangulos; el de los texrect (OpenGL.cpp, OGL_DrawTexturedRect)
		 * se queda en GX_NEAR, que es por donde van los fondos y los efectos de
		 * pantalla completa que se guardan y repintan cada frame.
		 *
		 * ★ [Pokemon Stadium, texto del menu borroso] LO GOBIERNA LA OPCION
		 * `xbox64_fb_bilineal` (gln64_fb_bilineal, xenon/gln64_config.cpp).  El
		 * texto sale de un buffer pintado como textura a escala 1:1 y en
		 * bilineal se emborronaba.  En `auto` (por defecto) solo filtran los
		 * juegos con hack_fbTextureOffset -- Conker y Mario Tennis, los mismos
		 * a los que GLideN64 corrige este muestreo --; el resto vuelve a GX_NEAR,
		 * como Wii64.  `activado` = GLideN64 (bilineal si no es POINT) y
		 * `desactivado` = Wii64 siempre. */
		const BOOL fbBilineal = (gln64_fb_bilineal == 1 ||
		                         (gln64_fb_bilineal == 0 &&
		                          (config.generalEmulation.hacks & hack_fbTextureOffset) != 0)) ? TRUE : FALSE;
		if (OGL.GXuseMinMagNearest || (texture->frameBufferTexture && !fbBilineal))
			GX_InitTexObjLOD(&texture->GXtex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
		else if (texture->frameBufferTexture)
			GX_InitTexObjLOD(&texture->GXtex, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
		else
			GX_InitTexObjLOD(&texture->GXtex, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_TRUE, GX_TRUE, GX_ANISO_4);
		if (texture->atlasNiveles)
			TextureCache_InitAtlasTexObj( texture );	// [Xbox360] el atlas lo filtra el shader
		GX_LoadTexObj(&texture->GXtex, t); // t = 0 is GX_TEXMAP0 and t = 1 is GX_TEXMAP1
		OGL.GXuseMinMagNearest = false;
#ifdef GLN64_SDLOG
	sprintf(txtbuffer,"Texture_ActivateTex: MAP%d, GXtexfmt %d, wd %d, ht %d, GXwd %d, GXht %d, clampS %d, clampT %d, fmt %d, size %d\n", t, texture->GXtexfmt, texture->realWidth, texture->realHeight, texture->GXrealWidth, texture->GXrealHeight, texture->clampS, texture->clampT, texture->format, texture->size);
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
#endif // GLN64_SDLOG
	}
#endif // __GX__

	texture->lastDList = RSP.DList;

	TextureCache_MoveToTop( texture );

	cache.current[t] = texture;
}

void TextureCache_ActivateDummy( u32 t )
{
	if (cache.dummy == NULL)
		return;

#ifndef __GX__
//TextureCache_ActivateTexture( t, cache.dummy );
	if (OGL.ARB_multitexture)
		glActiveTextureARB( GL_TEXTURE0_ARB + t );

	glBindTexture( GL_TEXTURE_2D, cache.dummy->glName );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
#else // !__GX__
	if (cache.dummy->GXtexture != NULL) 
	{
		GX_InitTexObj(&cache.dummy->GXtex, cache.dummy->GXtexture, (u16) 4, (u16) 4, cache.dummy->GXtexfmt, 
			cache.dummy->clampS ? GX_CLAMP : GX_REPEAT, cache.dummy->clampT ? GX_CLAMP : GX_REPEAT, GX_FALSE); 
		GX_LoadTexObj(&cache.dummy->GXtex, t); // t = 0 is GX_TEXMAP0 and t = 1 is GX_TEXMAP1
	}
#ifdef GLN64_SDLOG
	sprintf(txtbuffer,"Texture_ActivateDummy: %d\n", t);
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
#endif // GLN64_SDLOG
#endif // __GX__
}

void TextureCache_UpdateBackground()
{
	u32 numBytes = gSP.bgImage.width * gSP.bgImage.height << gSP.bgImage.size >> 1;
	u32 crc;

	crc = Hash_Calculate( 0xFFFFFFFF, &RDRAM[gSP.bgImage.address], numBytes );

   	if (gSP.bgImage.format == G_IM_FMT_CI)
	{
		if (gSP.bgImage.size == G_IM_SIZ_4b)
			crc = Hash_Calculate( crc, &TMEM[0x100 + (gSP.bgImage.palette << 4)], 128 );
		else if (gSP.bgImage.size == G_IM_SIZ_8b)
			crc = Hash_Calculate( crc, &TMEM[0x100], 2048 );
	}
	/* [Xbox360] [Resident Evil 2] Una textura de Z es otra entrada: ver
	 * GXGetRE2Z_RGBA8. */
	if (_texZRE2( gSP.bgImage.size ))
		crc ^= TEXZ_RE2_CRC;

	/* [Xbox360] Por el cubo del crc, igual que `TextureCache_Update`.  Ver
	 * TEXCACHE_INDICE en Textures.h. */
#if TEXCACHE_INDICE
	CachedTexture *current = cache.cubo[crc & (TEXCACHE_CUBOS - 1)];
#else
	CachedTexture *current = cache.top;
#endif

 	while (current)
  	{
		if ((current->crc == crc) &&
			(current->width == gSP.bgImage.width) &&
			(current->height == gSP.bgImage.height) &&
			(current->format == gSP.bgImage.format) &&
			(current->size == gSP.bgImage.size))
		{
			TextureCache_ActivateTexture( 0, current );
//			TextureCache_ActivateDummy( 0 );

			TextureCache_KeepBackground( current );
#if XBOX64_TRAZAS   /* solo lo lee el log */
			cache.hits++;
#endif
			return;
		}

		current = current->hnext;
	}

#if XBOX64_TRAZAS   /* solo lo lee el log */
	cache.misses++;
#endif

#ifndef __GX__
	// If multitexturing, set the appropriate texture
	if (OGL.ARB_multitexture)
		glActiveTextureARB( GL_TEXTURE0_ARB );
#endif // !__GX__

	cache.current[0] = TextureCache_AddTop();

	if (cache.current[0] == NULL)
	{
		TextureCache_ActivateDummy( 0 );
		return;
	}

#ifndef __GX__
	glBindTexture( GL_TEXTURE_2D, cache.current[0]->glName );
#endif // !__GX__

	cache.current[0]->address = gSP.bgImage.address;
	cache.current[0]->crc = crc;
	TextureCache_Indexar( cache.current[0] );   /* [Xbox360] regla 1 */

	cache.current[0]->format = gSP.bgImage.format;
	cache.current[0]->size = gSP.bgImage.size;

	cache.current[0]->width = gSP.bgImage.width;
	cache.current[0]->height = gSP.bgImage.height;

	cache.current[0]->clampWidth = gSP.bgImage.width;
	cache.current[0]->clampHeight = gSP.bgImage.height;
	cache.current[0]->palette = gSP.bgImage.palette;
	cache.current[0]->maskS = 0;
	cache.current[0]->maskT = 0;
	cache.current[0]->mirrorS = 0;
	cache.current[0]->mirrorT = 0;
	cache.current[0]->clampS = 1;
	cache.current[0]->clampT = 1;
	TextureCache_KeepBackground( cache.current[0] );
	cache.current[0]->line = 0;
	cache.current[0]->tMem = 0;
	cache.current[0]->lastDList = RSP.DList;
	cache.current[0]->frameBufferTexture = FALSE;

	cache.current[0]->realWidth = pow2( gSP.bgImage.width );
	cache.current[0]->realHeight = pow2( gSP.bgImage.height );

	cache.current[0]->scaleS = 1.0f / (f32)(cache.current[0]->realWidth);
	cache.current[0]->scaleT = 1.0f / (f32)(cache.current[0]->realHeight);

	cache.current[0]->shiftScaleS = 1.0f;
	cache.current[0]->shiftScaleT = 1.0f;

	TextureCache_LoadBackground( cache.current[0] );
	TextureCache_ActivateTexture( 0, cache.current[0] );
//	TextureCache_ActivateDummy( 0 );

	cache.cachedBytes += cache.current[0]->textureBytes;
}

static u32 _TextureCache_MipLevelCap( const CachedTexture *texInfo, u32 GXsize )
{
	const u32 blockWidth  = (GXsize == 1) ? 8 : 4;
	const u32 blockHeight = 4;

	u32 level = 0;
	u32 w = texInfo->realWidth;
	u32 h = texInfo->realHeight;

	while ((w >> 1) >= blockWidth && (h >> 1) >= blockHeight)
	{
		w >>= 1;
		h >>= 1;
		level++;
	}

	return level;
}

static void _TextureCache_DescribeMipLevel( CachedTexture *dst, const CachedTexture *base, u32 tileIdx )
{
	const gDPTile *tile = &gDP.tiles[tileIdx & 7];

	*dst = *base;

	dst->tMem    = tile->tmem;
	dst->palette = tile->palette;
	dst->maskS   = tile->masks;
	dst->maskT   = tile->maskt;
	dst->format  = tile->format;
	dst->size    = tile->size;
	dst->line    = tile->line;
	dst->clampS  = tile->clamps;
	dst->clampT  = tile->clampt;
	dst->mirrorS = tile->mirrors;
	dst->mirrorT = tile->mirrort;

	dst->max_level  = 0;
	dst->GXtexture  = NULL;
	dst->textureBytes = 0;
}

static void TextureCache_LoadMipChain( CachedTexture *texInfo, u32 baseTile )
{
	const u32 GXsize = imageFormat[gDP.otherMode.textureLUT][texInfo->size][texInfo->format].GXsize;
	const u32 cap = _TextureCache_MipLevelCap( texInfo, GXsize );

	if (texInfo->max_level > cap)
		texInfo->max_level = (u8)cap;

	if (texInfo->max_level == 0)
	{
		TextureCache_Load( texInfo );
		return;
	}

	const u32 levels = texInfo->max_level + 1;

	CachedTexture level[8];
	u32 levelBytes[8];
	u32 totalBytes = 0;
	u32 i;

	for (i = 0; i < levels; i++)
	{
		if (i == 0)
		{
			level[0] = *texInfo;
			level[0].max_level = 0;
			level[0].GXtexture = NULL;
		}
		else
		{
			_TextureCache_DescribeMipLevel( &level[i], &level[0], baseTile + i );

			level[i].width  = (level[i - 1].width  > 1) ? (level[i - 1].width  >> 1) : 1;
			level[i].height = (level[i - 1].height > 1) ? (level[i - 1].height >> 1) : 1;
			level[i].realWidth  = level[i - 1].realWidth  >> 1;
			level[i].realHeight = level[i - 1].realHeight >> 1;
			level[i].clampWidth  = level[i].width;
			level[i].clampHeight = level[i].height;
		}

		TextureCache_Load( &level[i] );

		if (level[i].GXtexture == NULL)
		{
			for (u32 j = 0; j < i; j++)
				if (level[j].GXtexture != NULL)
					__lwp_heap_free( GXtexCache, level[j].GXtexture );

			texInfo->max_level = 0;
			TextureCache_Load( texInfo );
			return;
		}

		levelBytes[i] = level[i].textureBytes;
		totalBytes += levelBytes[i];
	}

	u8 *chain = (u8*)__lwp_heap_allocate( GXtexCache, totalBytes );
	while (!chain && TextureCache_FreeOneTexture())
		chain = (u8*)__lwp_heap_allocate( GXtexCache, totalBytes );

	if (chain == NULL)
	{
		for (i = 0; i < levels; i++)
			if (level[i].GXtexture != NULL)
				__lwp_heap_free( GXtexCache, level[i].GXtexture );

		texInfo->max_level = 0;
		TextureCache_Load( texInfo );
		return;
	}

	u32 offset = 0;
	for (i = 0; i < levels; i++)
	{
		memcpy( chain + offset, level[i].GXtexture, levelBytes[i] );
		offset += levelBytes[i];
		__lwp_heap_free( GXtexCache, level[i].GXtexture );
	}

	DCFlushRange( chain, totalBytes );

	texInfo->GXtexture    = (u16*)chain;
	texInfo->GXtexfmt     = level[0].GXtexfmt;
	texInfo->GXrealWidth  = level[0].GXrealWidth;
	texInfo->GXrealHeight = level[0].GXrealHeight;
	texInfo->textureBytes = totalBytes;
}

/* [Xbox360] ★ ATLAS DE MIPMAP, COMO GLideN64 (TextureCache::_loadAccurate,
 * src/Textures.cpp:1616-1734, y TextureMipMap1 en
 * glsl_CombinerProgramBuilderAccurate.cpp; blog "Mip-mapping emulation",
 * 2014/10).  Solo la unidad 1, con un combinador que usa LOD_FRACTION y
 * gSP.texture.level > 1: el nivel 0 es el tile 1 (gSP.texture.tile + 1) y el
 * nivel L, el tile gSP.texture.tile + L + 1.  El shader elige los dos niveles
 * con el LOD y mezcla con LOD_FRACTION (gx_d3d9_tev.cpp, `atlasNivel`).
 *
 * Formato: un RGBA8 de ATLAS_ANCHO de ancho, leido como texels ARGB en fila:
 *   texel L (0..7), cabecera del nivel L:
 *     R = desplazamiento & 0xFF, G = desplazamiento >> 8   (en texels)
 *     B = log2(ancho) << 4 | log2(alto)                    (del nivel)
 *     A = ratioS << 4 | ratioT    (shift + 5, modulo 16, del tile: GLideN64)
 *   desde el texel 8, los niveles seguidos, ancho x alto cada uno.
 *
 * Diferencias con GLideN64, las dos por el modelo de texturas de glN64:
 *  - cada nivel se guarda a su realWidth x realHeight, con la mascara, el
 *    espejo y el clamp ya horneados por TextureCache_Load; el shader da la
 *    vuelta o hace clamp sobre el nivel 0 (el GPU lo hacia sobre realWidth);
 *  - las medidas del nivel L >= 1 son las del anterior entre dos, como
 *    TextureCache_LoadMipChain (GLideN64 las saca de _calcTileSizes). */
#define ATLAS_ANCHO 256

static u32 _atlasLog2( u32 v )
{
	u32 n = 0;
	while ((1u << n) < v)
		n++;
	return n;
}

static u32 _atlasRatio( u32 shift )
{
	u32 r = shift + 5;
	if (r >= 16)
		r -= 16;
	return r;
}

void TextureCache_InitAtlasTexObj( CachedTexture *texture )
{
	GX_InitTexObj(&texture->GXtex, texture->GXtexture, texture->atlasW, texture->atlasH,
		GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&texture->GXtex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
}

static BOOL TextureCache_LoadAtlas( CachedTexture *texInfo, u32 niveles )
{
	u32 dimW[8], dimH[8], offs[8];
	u32 total = 8, i, x, y;

	for (i = 0; i < niveles; i++)
	{
		if (i == 0)
		{
			dimW[0] = texInfo->realWidth;
			dimH[0] = texInfo->realHeight;
		}
		else
		{
			dimW[i] = (dimW[i - 1] > 1) ? (dimW[i - 1] >> 1) : 1;
			dimH[i] = (dimH[i - 1] > 1) ? (dimH[i - 1] >> 1) : 1;
		}
		offs[i] = total;
		total += dimW[i] * dimH[i];
	}
	if (total > 0xFFFF)	// el desplazamiento de la cabecera tiene 16 bits
		return FALSE;

	const u32 atlasH = (((total + ATLAS_ANCHO - 1) / ATLAS_ANCHO) + 3) & ~3u;
	u32 *lin = (u32*)malloc( ATLAS_ANCHO * atlasH * 4 );
	if (lin == NULL)
		return FALSE;
	memset( lin, 0, ATLAS_ANCHO * atlasH * 4 );

	CachedTexture level;
	u32 nW = texInfo->width, nH = texInfo->height;
	for (i = 0; i < niveles; i++)
	{
		const gDPTile *tile = &gDP.tiles[(gSP.texture.tile + 1 + i) & 7];

		if (i == 0)
		{
			level = *texInfo;
			level.max_level = 0;
			level.GXtexture = NULL;
			level.textureBytes = 0;
		}
		else
		{
			_TextureCache_DescribeMipLevel( &level, texInfo, gSP.texture.tile + 1 + i );
			nW = (nW > 1) ? (nW >> 1) : 1;
			nH = (nH > 1) ? (nH >> 1) : 1;
			level.width  = nW;
			level.height = nH;
			level.realWidth  = dimW[i];
			level.realHeight = dimH[i];
			level.clampWidth  = nW;
			level.clampHeight = nH;
		}
		level.atlasNiveles = 0;

		TextureCache_Load( &level );
		if (level.GXtexture == NULL)
		{
			free( lin );
			return FALSE;
		}

		u32 *tmp = (u32*)malloc( level.GXrealWidth * level.GXrealHeight * 4 );
		if (tmp == NULL)
		{
			__lwp_heap_free( GXtexCache, level.GXtexture );
			free( lin );
			return FALSE;
		}
		GXD3D_DecodificaLineal( level.GXtexture, level.GXtexfmt,
			(int)level.GXrealWidth, (int)level.GXrealHeight, tmp );
		for (y = 0; y < dimH[i]; y++)
			memcpy( &lin[offs[i] + y * dimW[i]], &tmp[y * level.GXrealWidth], dimW[i] * 4 );
		free( tmp );
		__lwp_heap_free( GXtexCache, level.GXtexture );

		const u32 r = offs[i] & 0xFF;
		const u32 g = (offs[i] >> 8) & 0xFF;
		const u32 b = (_atlasLog2( dimW[i] ) << 4) | _atlasLog2( dimH[i] );
		const u32 a = (_atlasRatio( tile->shifts ) << 4) | _atlasRatio( tile->shiftt );
		lin[i] = (a << 24) | (r << 16) | (g << 8) | b;
	}

	const u32 bytes = ATLAS_ANCHO * atlasH * 4;
	u8 *atlas = (u8*)__lwp_heap_allocate( GXtexCache, bytes );
	while (!atlas && TextureCache_FreeOneTexture())
		atlas = (u8*)__lwp_heap_allocate( GXtexCache, bytes );
	if (atlas == NULL)
	{
		free( lin );
		return FALSE;
	}

	// RGBA8 de GX: bloques de 4x4, 32 bytes de pares AR y luego 32 de GB.
	u8 *dst = atlas;
	for (y = 0; y < atlasH; y += 4)
	{
		for (x = 0; x < ATLAS_ANCHO; x += 4)
		{
			u32 k, l;
			for (k = 0; k < 4; k++)
			{
				for (l = 0; l < 4; l++)
				{
					const u32 c = lin[(y + k) * ATLAS_ANCHO + x + l];
					const u32 j = (k * 4 + l) * 2;
					dst[j]      = (u8)(c >> 24);
					dst[j + 1]  = (u8)(c >> 16);
					dst[32 + j]     = (u8)(c >> 8);
					dst[32 + j + 1] = (u8)c;
				}
			}
			dst += 64;
		}
	}
	free( lin );
	DCFlushRange( atlas, bytes );

	texInfo->GXtexture    = (u16*)atlas;
	texInfo->GXtexfmt     = GX_TF_RGBA8;
	texInfo->GXrealWidth  = ATLAS_ANCHO;
	texInfo->GXrealHeight = atlasH;
	texInfo->textureBytes = bytes;
	texInfo->atlasW       = ATLAS_ANCHO;
	texInfo->atlasH       = (u16)atlasH;
	texInfo->atlasNiveles = (u8)niveles;
	return TRUE;
}

/* [Xbox360] ★ [Mario Golf, parches del cesped] GLideN64 7ca20f8a ("Ground
 * incorrect at some place during mario golf intro #78"): el shift del tile
 * NO entra en la comparacion de la cache, asi que la misma textura vuelve con
 * otro shift (la base del cesped: shift 3,3 en unos trozos y 0,0 en otros,
 * [MGOLF] tex #3 / #8) y el acierto se quedaba con el shiftScale de cuando se
 * creo -> la base muestreada a 8x la frecuencia, un parche de patron
 * repetido.  GLideN64 lo recalcula en CADA activacion, acierto incluido. */
static void _TextureCache_UpdateShiftScale( u32 t, CachedTexture *texture )
{
	texture->shiftScaleS = 1.0f;
	texture->shiftScaleT = 1.0f;

	if (gSP.textureTile[t]->shifts > 10)
		texture->shiftScaleS = (f32)(1 << (16 - gSP.textureTile[t]->shifts));
	else if (gSP.textureTile[t]->shifts > 0)
		texture->shiftScaleS /= (f32)(1 << gSP.textureTile[t]->shifts);

	if (gSP.textureTile[t]->shiftt > 10)
		texture->shiftScaleT = (f32)(1 << (16 - gSP.textureTile[t]->shiftt));
	else if (gSP.textureTile[t]->shiftt > 0)
		texture->shiftScaleT /= (f32)(1 << gSP.textureTile[t]->shiftt);
}

void TextureCache_Update( u32 t )
{
	GLN64_PROF_SCOPE(tcupd);   /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	CachedTexture *current;
	//s32 i, j, k;
	u32 crc, /*bpl, cacheNum,*/ maxTexels;
	u32 tileWidth, maskWidth, loadWidth, lineWidth, clampWidth, height;
	u32 tileHeight, maskHeight, loadHeight, lineHeight, clampHeight, width;

	if (cache.enable2xSaI != OGL.enable2xSaI)
	{
		TextureCache_Destroy();
		TextureCache_Init();
	}

	if (cache.bitDepth != (unsigned int)OGL.textureBitDepth)
	{
		TextureCache_Destroy();
		TextureCache_Init();
	}

	if (gDP.textureMode == TEXTUREMODE_BGIMAGE)
	{
		TextureCache_UpdateBackground();
		return;
	}
	else if (gDP.textureMode == TEXTUREMODE_FRAMEBUFFER_BG)
	{
		FrameBuffer *buffer = FrameBuffer_GetBuffer( gSP.textureTile[t]->frameBufferAddress );
		if (buffer != NULL)
			FrameBuffer_ActivateBufferTextureBG( t, buffer );
		return;
	}
	/* [Xbox360] [Mario Tennis] El modo DEL TILE, como GLideN64
	 * (Textures.cpp:2066, `switch (pTile->textureMode)`).  Con el global, un
	 * tile cargado de RDRAM al lado de otro cargado de un framebuffer tomaba
	 * tambien este camino y activaba el framebuffer que le hubiera quedado en
	 * `frameBufferAddress`.  Ver _gDPPropagaCargaFB en gDP.cpp.  Va DETRAS de
	 * los dos fondos, que siguen siendo modos globales: el fondo pone
	 * `frameBufferAddress` en el tile 0 sin tocar su modo propio. */
	else if (gSP.textureTile[t]->textureMode == TEXTUREMODE_FRAMEBUFFER)
	{
		// The tile outlives the FrameBuffer, so a stored ptr would dangle once the buffer is removed, use the tile addr
		FrameBuffer *buffer = FrameBuffer_GetBuffer( gSP.textureTile[t]->frameBufferAddress );
		if (buffer != NULL)
			FrameBuffer_ActivateBufferTexture( t, buffer );
		return;
	}

	gSP.textureTile[t]->masks = gSP.textureTile[t]->originalMaskS;
	gSP.textureTile[t]->maskt = gSP.textureTile[t]->originalMaskT;

	maxTexels = imageFormat[gDP.otherMode.textureLUT][gSP.textureTile[t]->size][gSP.textureTile[t]->format].maxTexels;

	// Here comes a bunch of code that just calculates the texture size...I wish there was an easier way...

	// taken from GLideN64's _calcTileSizes()
	tileWidth = ((gSP.textureTile[t]->lrs - gSP.textureTile[t]->uls) & 0x03FF) + 1;
	tileHeight = ((gSP.textureTile[t]->lrt - gSP.textureTile[t]->ult) & 0x03FF) + 1;

	const bool mirrorFix = TextureCache_UseMirrorFixes();

	maskWidth = 1 << gSP.textureTile[t]->masks;
	maskHeight = 1 << gSP.textureTile[t]->maskt;

	loadWidth = ((gDP.loadTile->lrs - gDP.loadTile->uls) & 0x03FF) + 1;
	loadHeight = ((gDP.loadTile->lrt - gDP.loadTile->ult) & 0x03FF) + 1;

	lineWidth = gSP.textureTile[t]->line << imageFormat[gDP.otherMode.textureLUT][gSP.textureTile[t]->size][gSP.textureTile[t]->format].lineShift;

	if (lineWidth) // Don't allow division by zero
		lineHeight = min( maxTexels / lineWidth, tileHeight );
	else
		lineHeight = 0;


	gDPLoadTileInfo &loadedInfo = gDP.loadInfo[gSP.textureTile[t]->tmem & 0x1FF];
	const bool useLoadedInfo = (gDP.textureMode == TEXTUREMODE_TEXRECT) && (loadedInfo.loadType == LOADTYPE_TILE);

	if (gSP.textureTile[t]->tmem == gDP.loadTile->tmem)
	{
		if ((gDP.loadTile->loadWidth != 0) && (gDP.loadTile->masks == 0))
			loadedInfo.width = (u16)gDP.loadTile->loadWidth;
		if ((gDP.loadTile->loadHeight != 0) && (gDP.loadTile->maskt == 0))
			loadedInfo.height = (u16)gDP.loadTile->loadHeight;
	}
	gDP.loadTile->loadWidth = gDP.loadTile->loadHeight = 0;

	if (useLoadedInfo)
	{
		height = loadedInfo.height;
		if (height == 0)
			height = tileHeight;

		width = (loadedInfo.texWidth != 0) ? min( (u32)loadedInfo.width, (u32)loadedInfo.texWidth ) : loadedInfo.width;
		if (width == 0)
			width = tileWidth;
		
		if (loadedInfo.size > gSP.textureTile[t]->size)
			width <<= loadedInfo.size - gSP.textureTile[t]->size;

		if (mirrorFix && (lineWidth > width) && ((lineWidth * height) <= maxTexels))
			width = lineWidth;
	}
	else if (gDP.textureMode == TEXTUREMODE_TEXRECT)
	{
		u16 texRectWidth = gDP.texRect.width - gSP.textureTile[t]->uls;
		u16 texRectHeight = gDP.texRect.height - gSP.textureTile[t]->ult;

//		if ((tileWidth == (maskWidth + 1)) && (gDP.loadType == LOADTYPE_TILE) && (gDP.loadTile->lrs - gDP.loadTile->uls + 1 == tileWidth))
//			gSP.textureTile[t]->masks = 0;

		if (gSP.textureTile[t]->masks && ((maskWidth * maskHeight) <= maxTexels))
			width = maskWidth;
		else if ((tileWidth * tileHeight) <= maxTexels)
			width = tileWidth;
		else if ((tileWidth * texRectHeight) <= maxTexels)
			width = tileWidth;
		else if ((texRectWidth * tileHeight) <= maxTexels)
			width = gDP.texRect.width;
		else if ((unsigned int)(texRectWidth * texRectHeight) <= maxTexels)
			width = gDP.texRect.width;
		else if (gDP.loadType == LOADTYPE_TILE)
			width = loadWidth;
		else
			width = lineWidth;

//		if ((tileHeight == (maskHeight + 1)) && (gDP.loadType == LOADTYPE_TILE) && (gDP.loadTile->lrt - gDP.loadTile->ult + 1 == tileHeight))
//			gSP.textureTile[t]->maskt = 0;

		if (gSP.textureTile[t]->maskt && ((maskWidth * maskHeight) <= maxTexels))
			height = maskHeight;
		else if ((tileWidth * tileHeight) <= maxTexels)
			height = tileHeight;
		else if ((tileWidth * texRectHeight) <= maxTexels)
			height = gDP.texRect.height;
		else if ((texRectWidth * tileHeight) <= maxTexels)
			height = tileHeight;
		else if ((unsigned int)(texRectWidth * texRectHeight) <= maxTexels)
			height = gDP.texRect.height;
		else if (gDP.loadType == LOADTYPE_TILE)
			height = loadHeight;
		else
			height = lineHeight;

//		gSP.textureTile[t]->masks = 0;
//		gSP.textureTile[t]->maskt = 0;
	}
	else
	{
		const u32 clampMaskWidth  = (gSP.textureTile[t]->masks == 0) ? tileWidth  : maskWidth;
		const u32 clampMaskHeight = (gSP.textureTile[t]->maskt == 0) ? tileHeight : maskHeight;

		if (gSP.textureTile[t]->clamps)
			width = min( clampMaskWidth, tileWidth );
		else if (gSP.textureTile[t]->masks && ((maskWidth * maskHeight) <= maxTexels))
			width = maskWidth; // Use mask width if set and valid
		else if ((tileWidth * tileHeight) <= maxTexels)
			width = tileWidth; // else use tile width if valid
		else if (gDP.loadType == LOADTYPE_TILE)
			width = loadWidth; // else use load width if load done with LoadTile
		else
			width = lineWidth; // else use line-based width

		if (gSP.textureTile[t]->clampt)
			height = min( clampMaskHeight, tileHeight );
		else if (gSP.textureTile[t]->maskt && ((maskWidth * maskHeight) <= maxTexels))
			height = maskHeight;
		else if ((tileWidth * tileHeight) <= maxTexels)
			height = tileHeight;
		else if (gDP.loadType == LOADTYPE_TILE)
			height = loadHeight;
		else
			height = lineHeight;
	}

/*	if (gDP.loadTile->frameBuffer)
	{
		FrameBuffer_ActivateBufferTexture( t, gDP.loadTile->frameBuffer );
		return;
	}*/

 	clampWidth = (gSP.textureTile[t]->clamps && (gDP.otherMode.cycleType != G_CYC_COPY)) ? tileWidth : width;
	clampHeight = (gSP.textureTile[t]->clampt && (gDP.otherMode.cycleType != G_CYC_COPY)) ? tileHeight : height;

	if (useLoadedInfo)
	{
		if (gSP.textureTile[t]->clamps && (gSP.textureTile[t]->masks == 0))
			width = clampWidth;
		if (gSP.textureTile[t]->clampt && (gSP.textureTile[t]->maskt == 0))
			height = clampHeight;
	}

	if (clampWidth > 256)
		gSP.textureTile[t]->clamps = 0;
	if (clampHeight > 256)
		gSP.textureTile[t]->clampt = 0;

	// Make sure masking is valid.

	if ((maskWidth > width) && !(mirrorFix && gSP.textureTile[t]->mirrors && (maskWidth <= 128)))
	{
		gSP.textureTile[t]->masks = powof( width );
		maskWidth = 1 << gSP.textureTile[t]->masks;
	}

	if ((maskHeight > height) && !(mirrorFix && gSP.textureTile[t]->mirrort && (maskHeight <= 128)))
	{
		gSP.textureTile[t]->maskt = powof( height );
		maskHeight = 1 << gSP.textureTile[t]->maskt;
	}

	crc = TextureCache_CalculateCRC( t, width, height );

	/* [Xbox360] ★ ATLAS DE MIPMAP (TextureCache_LoadAtlas): la condicion de
	 * GLideN64 _loadAccurate (`usesLOD() && gSP.texture.level > 1 && _tile >
	 * 0`) y su max_level (level con DETAIL, level - 1 si no).  Ademas: el LOD
	 * encendido (sin el, su shader lee el tile 1 como textura normal,
	 * uNoAtlasTex), el tile 1 de verdad en la unidad 1, sin 2xSaI y sin pasar
	 * del tile 7.  Como en GLideN64 (_calculateCRC), gSP.texture.level entra
	 * en el crc; y el atlas, en la comparacion: la misma textura sin atlas es
	 * otra entrada. */
	u32 nivAtlas = 0;
	if (t == 1 && combiner.usesLOD && (gSP.texture.level > 1) &&
	    (gDP.otherMode.textureLOD == G_TL_LOD) && !cache.enable2xSaI &&
	    (gSP.textureTile[1] == &gDP.tiles[(gSP.texture.tile + 1) & 7]))
	{
		const u32 tile0 = (u32)gSP.texture.tile & 7;
		u32 maxLevel = (gDP.otherMode.textureDetail == G_TD_DETAIL) ? (u32)gSP.texture.level : (u32)gSP.texture.level - 1;
		if (tile0 + 1 + maxLevel > 7)
			maxLevel = (tile0 < 6) ? (6 - tile0) : 0;
		if (maxLevel > 0)
		{
			nivAtlas = maxLevel + 1;
			crc = Hash_Calculate( crc, &gSP.texture.level, sizeof(u32) );
			crc = Hash_Calculate( crc, &nivAtlas, sizeof(u32) );
		}
	}
	/* [Xbox360] [Resident Evil 2] Una textura de Z es otra entrada: ver
	 * GXGetRE2Z_RGBA8. */
	if (_texZRE2( gSP.textureTile[t]->size ))
		crc ^= TEXZ_RE2_CRC;

//	if (!TextureCache_Verify())
//		current = cache.top;

	/* [Xbox360] ★★ POR EL CUBO DEL CRC, NO POR LA LISTA LRU ENTERA.
	 *
	 * Antes esto era `current = cache.top; ... current = current->lower;`,
	 * o sea recorrer las ~300 entradas comparando doce campos en cada una:
	 * 3,81 us por consulta y 7.652 consultas/s.  Ahora solo se recorre la
	 * cadena del cubo, que de media tiene menos de un nodo.
	 *
	 * ★ ENCUENTRA EL MISMO NODO.  Los dos recorridos difieren en el ORDEN,
	 * y daria igual aunque hubiera varios candidatos... pero no puede
	 * haberlos: un nodo solo se anade en el camino de FALLO, o sea cuando
	 * ninguno casaba.  Dos entradas con el mismo crc Y los mismos once
	 * campos no pueden coexistir por construccion.
	 *
	 * La comparacion de campos se queda TAL CUAL: el cubo reparte por crc,
	 * no sustituye a la comprobacion. */
#if TEXCACHE_INDICE
	current = cache.cubo[crc & (TEXCACHE_CUBOS - 1)];
#else
	current = cache.top;
#endif
 	while (current)
  	{
		if ((current->crc == crc) &&
//			(current->address == gDP.textureImage.address) &&
//			(current->palette == gSP.textureTile[t]->palette) &&
			(current->width == width) &&
			(current->height == height) &&
			(current->clampWidth == clampWidth) &&
			(current->clampHeight == clampHeight) &&
			(current->maskS == gSP.textureTile[t]->masks) &&
			(current->maskT == gSP.textureTile[t]->maskt) &&
			(current->mirrorS == gSP.textureTile[t]->mirrors) &&
			(current->mirrorT == gSP.textureTile[t]->mirrort) &&
			(current->clampS == gSP.textureTile[t]->clamps) &&
			(current->clampT == gSP.textureTile[t]->clampt) &&
//			(current->tMem == gSP.textureTile[t]->tMem) &&
/*			(current->ulS == gSP.textureTile[t]->ulS) &&
			(current->ulT == gSP.textureTile[t]->ulT) &&
			(current->lrS == gSP.textureTile[t]->lrS) &&
			(current->lrT == gSP.textureTile[t]->lrT) &&*/
			(current->format == gSP.textureTile[t]->format) &&
			(current->size == gSP.textureTile[t]->size) &&
			(current->atlasNiveles == nivAtlas))
		{
			_TextureCache_UpdateShiftScale( t, current );   /* GLideN64 7ca20f8a */
			TextureCache_ActivateTexture( t, current );
//			TextureCache_ActivateDummy( t );

#if XBOX64_TRAZAS   /* solo lo lee el log */
			cache.hits++;
#endif
			return;
		}

#if TEXCACHE_INDICE
		current = current->hnext;
#else
		current = current->lower;
#endif
	}

#if XBOX64_TRAZAS   /* solo lo lee el log */
	cache.misses++;
#endif

#ifndef __GX__
	// If multitexturing, set the appropriate texture
	if (OGL.ARB_multitexture)
		glActiveTextureARB( GL_TEXTURE0_ARB + t );
#endif // !__GX__

	cache.current[t] = TextureCache_AddTop();

	if (cache.current[t] == NULL)
	{
		TextureCache_ActivateDummy( t );
		return;
	}

#ifndef __GX__
	glBindTexture( GL_TEXTURE_2D, cache.current[t]->glName );
#endif // !__GX__

	cache.current[t]->address = gDP.textureImage.address;
	cache.current[t]->crc = crc;
	TextureCache_Indexar( cache.current[t] );   /* [Xbox360] regla 1 */

	cache.current[t]->format = gSP.textureTile[t]->format;
	cache.current[t]->size = gSP.textureTile[t]->size;

	cache.current[t]->width = width;
	cache.current[t]->height = height;

	cache.current[t]->clampWidth = clampWidth;
	cache.current[t]->clampHeight = clampHeight;

	cache.current[t]->palette = gSP.textureTile[t]->palette;
/*	cache.current[t]->fulS = gSP.textureTile[t]->fulS;
	cache.current[t]->fulT = gSP.textureTile[t]->fulT;
	cache.current[t]->ulS = gSP.textureTile[t]->ulS;
	cache.current[t]->ulT = gSP.textureTile[t]->ulT;
	cache.current[t]->lrS = gSP.textureTile[t]->lrS;
	cache.current[t]->lrT = gSP.textureTile[t]->lrT;*/
	cache.current[t]->maskS = gSP.textureTile[t]->masks;
	cache.current[t]->maskT = gSP.textureTile[t]->maskt;
	cache.current[t]->mirrorS = gSP.textureTile[t]->mirrors;
	cache.current[t]->mirrorT = gSP.textureTile[t]->mirrort;
 	cache.current[t]->clampS = gSP.textureTile[t]->clamps;
	cache.current[t]->clampT = gSP.textureTile[t]->clampt;
	cache.current[t]->line = gSP.textureTile[t]->line;
	cache.current[t]->tMem = gSP.textureTile[t]->tmem;
	cache.current[t]->lastDList = RSP.DList;
	cache.current[t]->frameBufferTexture = FALSE;

/*	if (cache.current[t]->clampS)
		cache.current[t]->realWidth = pow2( clampWidth );
	else if (cache.current[t]->mirrorS)
		cache.current[t]->realWidth = maskWidth << 1;
	else
		cache.current[t]->realWidth = pow2( width );

	if (cache.current[t]->clampT)
		cache.current[t]->realHeight = pow2( clampHeight );
	else if (cache.current[t]->mirrorT)
		cache.current[t]->realHeight = maskHeight << 1;
	else
		cache.current[t]->realHeight = pow2( height );*/

	/* [Xbox360] ★★ [Worms Armageddon, la camara a 14 img/s, logs (3)-(4)
	 * 2026-10-24] EL ESPEJO LO HACE LA GPU, COMO EN GLideN64.  glN64 lo hornea:
	 * la textura mide el DOBLE de la mascara en cada eje con espejo y los texels
	 * se copian reflejados (mirrorSBit/mirrorTBit en TextureCache_Load).  El
	 * fondo de Worms (S2DEX BG_1CYC por tiras) son tiras de 330x6 en CI8 con
	 * mascara 9,3 y espejo en los dos ejes: [TEXPAR2] `mir=1,1 real=1024x16`,
	 * 16384 texels para dibujar ~2000.  Al mover la camara cada tira es una
	 * textura nueva (680/s): 7,3 ms/VI en tcload + 8,0 ms/VI en la conversion
	 * del shim, 44 MB/s.  GLideN64 la crea del tamano de la mascara y pone
	 * GL_MIRRORED_REPEAT; aqui GX_MIRROR (D3DTADDRESS_MIRROR en el shim).
	 * ★ El modo lo ponen DOS sitios: TextureCache_ActivateTexture y el
	 * GX_InitTexObj de los texrect (OpenGL.cpp); sin el segundo, las tiras del
	 * retrato del lobo de Killer Instinct (s 212..256) salian rotas.
	 * Con mascara, espejo y sin clamp la mitad reflejada horneada es la primera
	 * mitad al reves (la coordenada solo se enmascara y se refleja; el clamp de
	 * S ya se quito por Killer Instinct), asi que la GPU da lo mismo con la
	 * cuarta parte de texels en una textura de dos ejes.  Fuera: ZSortBOSS
	 * (mirrorFix, su propio recorte), el atlas de LOD, los mipmaps y 2xSaI,
	 * que construyen la textura a su manera. */
	const BOOL espejoGPU = (!mirrorFix && nivAtlas == 0 && !cache.enable2xSaI &&
	                        !((gDP.otherMode.textureLOD == G_TL_LOD) &&
	                          (gDP.otherMode.textureDetail == G_TD_DETAIL) &&
	                          (gSP.texture.level > 1))) ? TRUE : FALSE;
	cache.current[t]->GXespejoS = (espejoGPU && !cache.current[t]->clampS &&
	                               cache.current[t]->mirrorS && cache.current[t]->maskS != 0) ? 1 : 0;
	cache.current[t]->GXespejoT = (espejoGPU && !cache.current[t]->clampT &&
	                               cache.current[t]->mirrorT && cache.current[t]->maskT != 0) ? 1 : 0;

	if (cache.current[t]->clampS)
		cache.current[t]->realWidth = pow2( clampWidth );
	else if (cache.current[t]->GXespejoS)
		cache.current[t]->realWidth = maskWidth;
	else if (cache.current[t]->mirrorS && (!mirrorFix || (maskWidth <= 128)))
		cache.current[t]->realWidth = maskWidth << 1;
	else
		cache.current[t]->realWidth = pow2( width );

	if (cache.current[t]->clampT)
		cache.current[t]->realHeight = pow2( clampHeight );
	else if (cache.current[t]->GXespejoT)
		cache.current[t]->realHeight = maskHeight;
	else if (cache.current[t]->mirrorT && (!mirrorFix || (maskHeight <= 128)))
		cache.current[t]->realHeight = maskHeight << 1;
	else
		cache.current[t]->realHeight = pow2( height );

	cache.current[t]->scaleS = 1.0f / (f32)(cache.current[t]->realWidth);
	cache.current[t]->scaleT = 1.0f / (f32)(cache.current[t]->realHeight);

	cache.current[t]->offsetS = OGL.enable2xSaI ? 0.25f : 0.5f;
	cache.current[t]->offsetT = OGL.enable2xSaI ? 0.25f : 0.5f;

	_TextureCache_UpdateShiftScale( t, cache.current[t] );   /* GLideN64 7ca20f8a */

	cache.current[t]->max_level = 0;

	/* [Xbox360] ★ LA SONDA DEL PUNTO K/M, EN SU PROPIO SITIO.
	 *
	 * El censo `[GLN64RM]` mira `gDP.otherMode.l`, pero la puerta de aqui
	 * abajo depende de `textureLOD` y `textureDetail`, que viven en
	 * `otherMode.H`.  O sea que el censo NO puede ver este defecto por
	 * construccion, y los quesitos del marcador POWER de Mario 64 -- que es
	 * lo que arreglaron los puntos K y M-- se deciden justo aqui.
	 *
	 * Diferencia MEDIDA entre carga limpia (quesitos OK) y segunda carga
	 * (quesitos ausentes, aro visible): el conjunto de modos de render es
	 * IDENTICO en los dos.  Se dibuja lo mismo; lo que cambia es con que
	 * textura.  Asi que lo que hay que ver es esta decision. */
#if GLN64_CAZA_MARIO64
	gln64_tex_nota( (unsigned int)gDP.otherMode.textureLOD,
	                (unsigned int)gDP.otherMode.textureDetail,
	                (unsigned int)gSP.texture.level,
	                (unsigned int)gSP.texture.tile,
	                (unsigned int)t );
#endif

	if (nivAtlas > 0)
	{
		if (!TextureCache_LoadAtlas( cache.current[t], nivAtlas ))
		{
			cache.current[t]->atlasNiveles = 0;
			TextureCache_Load( cache.current[t] );
		}
	}
	else if ((gDP.otherMode.textureLOD == G_TL_LOD) &&
	    (gDP.otherMode.textureDetail == G_TD_DETAIL) &&
	    (gSP.texture.level > 1))
	{
		u32 levels = gSP.texture.level - 1;
		if (levels > 7)
			levels = 7;

		cache.current[t]->max_level = (u8)levels;
		/* ★ DIVERGENCIA vs Wii64, Y ES CONSECUENCIA DEL PUNTO K.
		 *
		 * `LoadMipChain` usa `baseTile + i` para el nivel i (>=1), asi que
		 * `baseTile` tiene que ser el tile del NIVEL 0 para que la cadena
		 * salga consecutiva.  Upstream pasa `tile + 1` porque su
		 * `_gSPDetailBaseTile` ya habia corrido el nivel 0 a `tile + 1`.
		 * Aqui ese desplazamiento esta quitado (punto K), asi que el nivel 0
		 * vuelve a ser `tile` y pasar `tile + 1` se SALTARIA el tile+1:
		 * nivel 0 en `tile` y nivel 1 en `tile+2`.
		 *
		 * Los dos cambios van juntos: si algun dia se devuelve el
		 * desplazamiento del punto K, esto vuelve a `+ 1`.
		 *
		 * [Xbox360] PENDIENTE DE MEDIR (Mario Golf, 2026-10-06): GLideN64
		 * (Textures.cpp:1410-1427, 1471) solo da cadena a la UNIDAD 1 y saca su
		 * nivel n de `gSP.texture.tile + n + 1`; aqui la llevan las dos y la
		 * unidad 1 repite el tile 1 como nivel 1.  Probado como arreglo de los
		 * parches de Mario Golf (log 9): SIN efecto, asi que se devolvio a esto.
		 * Los parches resultaron ser el shiftScale del acierto de cache
		 * (GLideN64 7ca20f8a); esta divergencia sigue sin medir en otro juego. */
		TextureCache_LoadMipChain( cache.current[t], gSP.texture.tile );
	}
	else
		TextureCache_Load( cache.current[t] );

	TextureCache_ActivateTexture( t, cache.current[t] );
//	TextureCache_ActivateDummy( t );

	cache.cachedBytes += cache.current[t]->textureBytes;
}

void TextureCache_ActivateNoise( u32 t )
{
#ifndef __GX__
	if (OGL.ARB_multitexture)
		glActiveTextureARB( GL_TEXTURE0_ARB + t );

	glBindTexture( GL_TEXTURE_2D, cache.glNoiseNames[RSP.DList & 0x1F] );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
#else // !__GX__
	//Noise is not being implemented for now.
#endif // __GX__
}

#ifdef __GX__
void TextureCache_FreeNextTexture()
{
	CachedTexture *current = cache.bottom;

	while (current)
	{
		if (current == cache.dummy ||
		    TextureCache_IsInUse( current ) ||
		    TextureCache_IsKeptBackground( current ) ||
		    (OGL.frameBufferTextures && current->frameBufferTexture && current->VIcount < 2))
		{
			current = current->higher;
			continue;
		}

		TextureCache_Remove( current );
		return;
	}

	// Nothing evictable left; give up a framebuffer if there is one to give.
	if (OGL.frameBufferTextures)
		FrameBuffer_RemoveBottom();
}

#endif // __GX__
