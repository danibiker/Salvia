/**
 * glN64_GX - Textures.h
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifndef TEXTURES_H
#define TEXTURES_H

#ifndef __GX__
#include <GL/gl.h>
#else // !__GX__
#define __WIN32__
#include "gl.h" 
#include "glext.h"
#undef __WIN32__
#ifdef HW_RVL
#include "../gc_memory/MEM2.h"
#endif //HW_RVL
#endif // __GX__
#include "convert.h"

struct CachedTexture
{
#ifdef __GX__
	GXTexObj	GXtex;
	u16			*GXtexture;
	u8			GXtexfmt;
	u32			GXrealWidth, GXrealHeight;	// Actual dimensions of GX texture
	u8			max_level;					// Highest mip level held, 0 for a single-level texture
	/* [Xbox360] Atlas de mipmap de GLideN64 (_loadAccurate): si atlasNiveles
	 * > 0, GXtexture es un RGBA8 de atlasW x atlasH con una cabecera y los
	 * niveles en fila (ver TextureCache_LoadAtlas).  realWidth/realHeight y
	 * scaleS/T siguen siendo los del NIVEL 0 (el tile 1), asi las UV no
	 * cambian. */
	u8			atlasNiveles;
	u16			atlasW, atlasH;
	u32			VIcount;
#endif // __GX__

#ifndef __GX__
	GLuint	glName;
#endif
	u32		address;
	u32		crc;
//	float	fulS, fulT;
//	WORD	ulS, ulT, lrS, lrT;
	float	offsetS, offsetT;
	u32		maskS, maskT;
	u32		clampS, clampT;
	u32		mirrorS, mirrorT;
	/* [Xbox360] [Worms Armageddon] 1 = el espejo de ese eje lo hace la GPU
	 * (GX_MIRROR) sobre una textura del tamano de la mascara, como GLideN64
	 * con GL_MIRRORED_REPEAT; 0 = horneado en la textura (doble de ancho/alto).
	 * Ver TextureCache_Update. */
	u8		GXespejoS, GXespejoT;
	u32		line;
	u32		size;
	u32		format;
	u32		tMem;
	u32		palette;
	u32		width, height;			  // N64 width and height
	u32		clampWidth, clampHeight;  // Size to clamp to
	u32		realWidth, realHeight;	  // Actual texture size
	f32		scaleS, scaleT;			  // Scale to map to 0.0-1.0
	f32		shiftScaleS, shiftScaleT; // Scale to shift
	u32		textureBytes;

	CachedTexture	*lower, *higher;
	u32		lastDList;

	u32		frameBufferTexture;

	/* [Xbox360] ★★ SEGUNDO ENLAZADO: EL INDICE POR CRC.
	 *
	 * `lower/higher` son la lista LRU y NO SE TOCAN.  Esto es una cadena
	 * APARTE, por cubo de hash, sobre los mismos nodos.  Es el diseno de
	 * GLideN64 (`m_lruTextureLocations`, un mapa crc->nodo con la lista LRU
	 * al lado), no una invencion.
	 *
	 * POR QUE.  `TextureCache_Update` recorria la lista LRU ENTERA
	 * comparando crc + 11 campos en cada consulta.  Medido en OoT: 7.652
	 * consultas/s a **3,81 us cada una** -- unos 12.200 ciclos, que con la
	 * lista en unos 300 nodos cuadra con recorrerla casi entera.  Son 486
	 * us de cada frame, 1,46 ms del frame que dibuja.
	 *
	 * ★ `hCubo` GUARDA EL CUBO, NO SE RECALCULA AL DESENLAZAR.  Recalcularlo
	 * desde `crc` seria correcto HOY (nadie reasigna el crc de un nodo ya
	 * indexado) y una bomba manana: el dia que alguien lo reasigne, el
	 * desenlace iria al cubo equivocado y dejaria la cadena rota sin avisar.
	 * Se guarda **cubo+1**, asi que el 0 del `memset` de `TextureCache_AddTop`
	 * significa "no indexada" sin necesidad de otra bandera. */
	CachedTexture	*hnext, *hprev;
	u32		hCubo;
};


/* [Xbox360] Cubos del indice por crc.  Potencia de dos: el cubo es
 * `crc & (TEXCACHE_CUBOS-1)`, sin division.  Con ~300 texturas vivas y 512
 * cubos la cadena media queda por debajo de 1, o sea O(1) de verdad.
 * Cuesta 2 KB de punteros. */
#define TEXCACHE_CUBOS 512

/* [Xbox360] ★★ DE QUE LISTA SE BUSCA.  El indice se MANTIENE en los dos
 * casos; lo unico que cambia es por donde se recorre al consultar.
 *
 *   0  por la lista LRU, como siempre.  El indice se construye y se
 *      desmonta igual, pero no se consulta.
 *   1  por el cubo del crc.
 *
 * ★★ LA BITACORA DEL LOG 41, QUE ES LO MAS UTIL QUE HA SALIDO DE ESTO.
 *
 * Con el cubo, `tcupd` bajo de 480 a 198 us/frame -- justo lo predicho --
 * pero `DrawPrimitive` paso de 963 ns a **25.037 ns** por draw y `emu max`
 * de 23,4 a 30,1 ms.  Lo di por regresion.  ERA UNA LECTURA EQUIVOCADA:
 * `dt` -- lo unico que el usuario percibe -- se quedo en 16,6-17,9 con
 * `late` de 0-8 de 120, igual o mejor que antes.
 *
 * Si la CPU baja 282 us, `prim` sube 2.400 y el frame NO cambia, el tiempo
 * no ha aumentado: HA CAMBIADO DE SITIO.  Lo que antes esperaba en el
 * `Present` del frontend ahora espera dentro de `DrawPrimitive`.
 *
 * ★★ CONCLUSION: EN ESTA ESCENA EL MURO ES LA GPU, y lo era desde antes.
 * La CPU iba lo bastante lenta como para taparlo.  Todo microsegundo que se
 * le quite a la CPU se lo come el `DrawPrimitive` bloqueando, asi que
 * seguir optimizando CPU aqui no da frames.  (Eso invalida la conclusion de
 * tres builds antes -- "prim=963 ns contra los 740 de Rice, la GPU queda
 * descartada" --: era cierta entonces y dejo de serlo al quitar esos 282 us.)
 *
 * ★ Y DE PASO, DOS HIPOTESIS MUERTAS que no hay que volver a mirar:
 *   - "hay un cuarto camino de alta sin indexar y el cubo no lo ve":
 *     `FrameBuffer.cpp:395` si llama a `TextureCache_AddTop()`, pero NUNCA
 *     asigna `crc`, asi que esos nodos se quedan a 0 y el recorrido lineal
 *     tampoco los encontraba.  Se liberan por `TextureCache_Remove`, que
 *     desindexa y con `hCubo == 0` no hace nada.  El indice es correcto.
 *   - "el indice sirve una textura distinta": no hay tal.
 *
 * El interruptor se queda porque el A/B cuesta un digito y el dia que
 * aparezca un problema de texturas es la primera pregunta que hay que
 * contestar.  A 1 es el camino bueno. */
#define TEXCACHE_INDICE 1

struct TextureCache
{
	CachedTexture	*bottom, *top;

	/* [Xbox360] El indice.  Ver la nota de `hnext` en CachedTexture. */
	CachedTexture	*cubo[TEXCACHE_CUBOS];

	CachedTexture	*(current[2]);
	u32				maxBytes;
	u32				cachedBytes;
	u32				numCached;
	u32				hits, misses;
	GLuint			glNoiseNames[32];
	//GLuint			glDummyName;
	CachedTexture	*dummy;
	u32				enable2xSaI, bitDepth;
#ifdef __GX__
	int				VIcount;
	u32				GXprimDepthCnt,GXZTexPrimCnt,GXnoZTexPrimCnt;
#endif // __GX__
};

extern TextureCache cache;

#ifdef __GX__
# ifdef HW_RVL
# define GX_TEXTURE_CACHE_SIZE TEXCACHE_SIZE //8MB for Wii
# else //HW_RVL
#  ifdef USE_EXPANSION
#   define GX_TEXTURE_CACHE_SIZE (1*1024*1024) //1 MB for GC with expansion pak
#  else
#	define GX_TEXTURE_CACHE_SIZE (0x264000) //2.4 MB for GC
#  endif
# endif //!HW_RVL
# define GX_MAX_TEXTURES ((400*1024)/sizeof( CachedTexture )) //Allow 400kB of Texture Meta
#endif //__GX__

inline u32 pow2( u32 dim )
{
	u32 i = 1;

	while (i < dim) i <<= 1;

	return i;
}

inline u32 powof( u32 dim )
{
	u32 num = 1;
	u32 i = 0;

	while (num < dim)
	{
		num <<= 1;
		i++;
	}

	return i;
}

CachedTexture *TextureCache_AddTop();
void TextureCache_MoveToTop( CachedTexture *newtop );
void TextureCache_Remove( CachedTexture *texture );
void TextureCache_RemoveBottom();
void TextureCache_Init();
void TextureCache_Destroy();
void TextureCache_Update( u32 t );
void TextureCache_ActivateTexture( u32 t, CachedTexture *texture );
#ifdef __GX__
void TextureCache_InitAtlasTexObj( CachedTexture *texture );	// [Xbox360] atlas de mipmap: NEAR, CLAMP, sin mips
#endif // __GX__
void TextureCache_ActivateNoise( u32 t );
void TextureCache_ActivateDummy( u32 t );
BOOL TextureCache_Verify();
#ifdef __GX__
void TextureCache_FreeNextTexture();
BOOL TextureCache_FreeOneTexture();
void TextureCache_PinTexture( const CachedTexture *tex );
void TextureCache_ReleaseRetired();
#endif // __GX__

#endif
