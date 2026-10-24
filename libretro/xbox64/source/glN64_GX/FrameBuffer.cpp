/**
 * glN64_GX - FrameBuffer.cpp
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009, 2010 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifdef __GX__
#include <gccore.h>
#include <ogc/lwp_heap.h>
#include <string.h>
#endif // __GX__

#ifndef __LINUX__
# include <windows.h>
#else
# include "../main/winlnxdefs.h"
# include <stdlib.h>
#endif // __LINUX__
#include "OpenGL.h"
#include "FrameBuffer.h"
#include "RSP.h"
#include "RDP.h"
#include "Textures.h"
#include "VI.h"
#include "Combiner.h"
#include "gDP.h"
#include "Types.h"
#include "../gui/DEBUG.h"
#include "Config.h"
#include "../xenon64_diag.h"   /* XBOX64_TRAZAS */

FrameBufferInfo frameBuffer;

extern GXRModeObj *vmode, *rmode;

// The buffer FrameBuffer_SaveBuffer() is currently building
static FrameBuffer *fbUnderConstruction = NULL;

void glN64_FBRead( unsigned long addr )
{
	(void)addr;
}

#ifdef __GX__
extern heap_cntrl* GXtexCache;

/* [Xbox360] El buffer que FrameBuffer_RestoreBuffer encontro NUEVO, para
 * FrameBuffer_GX_CargaNuevoDesdeRDRAM.  0 = ninguno. */
static u32 s_fbNuevoDir = 0, s_fbNuevoSize = 0, s_fbNuevoAncho = 0;

/* [Xbox360] ★ [Mario Tennis, los digitos que GOTEAN] El buffer que se cargo
 * desde la RDRAM y CUANTAS FILAS se pintaron, para que FrameBuffer_SaveBuffer
 * le de a su TEXTURA ese alto aunque el juego haya dibujado menos.
 *
 * Medido con [MTDIG] (log 18): el medidor 00264430 (48 de ancho) recibe los
 * digitos con texrects de y=16 a y=28 y glN64 le da al buffer el alto
 * DIBUJADO (gDPBufferChanged: 28).  El panel 3D del estadio lo muestrea mas
 * alto, y por debajo de la fila 28 el clamp repetia la ultima fila guardada
 * -- la base de los digitos --: el goteo sobre el marco.
 *
 * GLideN64 separa las dos cosas: el RANGO del buffer (m_endAddress, el que
 * decide que direcciones son framebuffer) sale del alto dibujado, pero la
 * TEXTURA mide siempre VI_GetMaxBufferHeight (FrameBuffer::init,
 * _initTexture(_width, maxHeight...)), asi que por debajo lee lo que hay: el
 * marco cargado de la RDRAM.  Aqui igual: `current->height` (el rango) no se
 * toca -- con 224 filas el medidor taparia las texturas de letras que viven
 * detras en la RAM --, solo la textura crece.  Tope VI.height: es lo que cabe
 * en el EFB. */
static u32 s_fbCargadoDir = 0, s_fbCargadoAncho = 0, s_fbCargadoAlto = 0;
/* ★ Y el MISMO tamano de texel: el aux de la sombra de Mario Tennis se carga en
 * 16 bits y luego, en la misma direccion y ancho, se usa en 8 bits y se lee
 * con un tile con espejo a 64 filas (log 22).  Esa textura de 8 bits heredaba
 * el alto de la de 16 y el espejo del sampler se iba a 224 filas. */
static u32 s_fbCargadoSize = 0;
#endif //__GX__

/* `forzar`: la escritura en la RDRAM es NUESTRA (borrados, copias a RDRAM),
 * asi que la copia se rehace aunque ya exista.  Ver la nota de la copia unica
 * en el cuerpo. */
static void _fbCopiaRdram( FrameBuffer *buffer, BOOL forzar );

// Re-stamp the fingerprint of every buffer starting in [start,end].
// GLideN64 does this after copyWhiteToRDRAM().
void FrameBuffer_RestampMarkers( u32 start, u32 end )
{
	FrameBuffer *current = frameBuffer.top;

	while (current != NULL)
	{
		if (current->startAddress >= start && (current->startAddress + 3) <= end)
			_fbCopiaRdram( current, TRUE );
		// [Xbox360] A main buffer keeps a copy of ALL its RDRAM, not four words at
		// its start, so any write of ours that overlaps it has to refresh the copy.
		// Conker's clears start at x=2 ([FILL-GX] 2,0-291,217), past startAddress.
		else if (current->rdramCopy != NULL &&
		         start <= current->endAddress && end >= current->startAddress)
			_fbCopiaRdram( current, TRUE );
		current = current->lower;
	}
}

// GLideN64 uses this, has it named fingerprint instead.
static const u32 fbFingerprint[4] = { 2, 6, 4, 3 };

u32 FB_frame = 0;

// GLideN64's FrameBuffer::updateEndAddress()
static void _fbUpdateEndAddress( FrameBuffer *buffer )
{
	const u32 height = buffer->height ? buffer->height : 1;
	const u32 end = buffer->startAddress + (((buffer->width * height) << buffer->size >> 1) - 1);

	buffer->endAddress = (end < RDRAMSize) ? end : (RDRAMSize - 1);
}

/* [Xbox360] ★ [Mario Tennis, "GAME MARIO" con una tira de escenario encima,
 * solo al acabar el primer juego] EL ANCHO DEL VI DE GLideN64 PARA DECIDIR SI
 * UN BUFFER ES AUXILIAR.  GLideN64 (VI_UpdateSize, VI.cpp:58-69) toma
 * `VI.width = *REG.VI_WIDTH` -- la zancada del registro, dividida en
 * entrelazado si pasa de 640 --, y FrameBuffer::isAuxiliary compara con eso.
 * glN64 calcula VI.width con H_START y la escala X (el ancho VISIBLE): en
 * Mario Tennis 388 con buffers de 384.  Asi sus buffers de 384 contaban como
 * AUXILIARES, que solo se validan por la huella de las 4 primeras palabras, y
 * no como principales, que guardan su RDRAM entera (_fbCopiaRdram).  El
 * buffer abandonado 00238040 (w=384) seguia "valido" con el texto del
 * letrero cargado en mitad de su memoria por la CPU, y la tira de 128x16 que
 * cae en una fila exacta (0023DF40 + 2*0x800 = +37 filas) se servia como
 * textura de framebuffer: un trozo de escena.  MEDIDO con un censo de
 * dibujos (logs (6) del 2026-10-24: `1 de 7 servidas como FB`; ya
 * retirado).  Solo para
 * esta decision: VI.width se queda como esta (escala, viewport...). */
static u32 _fbAnchoVIReferencia()
{
	u32 ancho;
	if (REG.VI_WIDTH == NULL || (*REG.VI_WIDTH & 0xFFF) == 0)
		return VI.width;
	ancho = *REG.VI_WIDTH & 0xFFF;
	if (REG.VI_STATUS != NULL && (*REG.VI_STATUS & 0x40) != 0)
	{
		f32 lleno = 640.0f;
		if (REG.VI_X_SCALE != NULL && (*REG.VI_X_SCALE % 512) == 0)
			lleno *= (f32)(*REG.VI_X_SCALE & 0xFFF) / 1024.0f;
		if ((f32)ancho > lleno && lleno > 0.0f)
		{
			const u32 esc = (u32)((f32)ancho / lleno + 0.5f);
			if (esc > 1)
				ancho /= esc;
		}
	}
	return ancho;
}

static BOOL _fbIsAuxiliary( const FrameBuffer *buffer )
{
	const u32 anchoVI = _fbAnchoVIReferencia();
	return (buffer->width != anchoVI || buffer->size < G_IM_SIZ_16b) ? TRUE : FALSE;
}

// [Xbox360] GLideN64's m_RdramCopy.clear()
static void _fbFreeRdramCopy( FrameBuffer *buffer )
{
	if (buffer->rdramCopy != NULL)
		free( buffer->rdramCopy );
	buffer->rdramCopy = NULL;
	buffer->rdramCopySize = 0;
}

void FrameBuffer_CopyRdram( FrameBuffer *buffer )
{
	_fbCopiaRdram( buffer, FALSE );
}

static void _fbCopiaRdram( FrameBuffer *buffer, BOOL forzar )
{
	if (buffer == NULL)
		return;

	buffer->fingerprint = FALSE;

	const u32 stride = buffer->width << buffer->size >> 1;
	if (stride == 0 || buffer->startAddress >= RDRAMSize)
		return;

	u32 height = buffer->height ? buffer->height : 1;
	if ((buffer->startAddress + stride * height) > RDRAMSize)
		height = (RDRAMSize - buffer->startAddress) / stride;
	if (height == 0)
		return;

	// height <= 1 means nothing has been rendered into it yet
	if (height <= 1)
		return;

	const u32 dataSize = stride * height;

	if (buffer->isDepthBuffer)
	{
		_fbFreeRdramCopy( buffer );
		return;
	}

	/* [Xbox360] ★ [Conker, zapatillas y parches negros en la pared] LOS BUFFERS
	 * PRINCIPALES TAMBIEN GUARDAN SU RDRAM, COMO EN GLideN64.
	 *
	 * GLideN64 `FrameBuffer::copyRdram()` (src/FrameBuffer.cpp:216-247): un
	 * auxiliar recibe la huella; uno principal, una COPIA ENTERA de su RDRAM
	 * (`m_RdramCopy`), y `isValid()` lo da por muerto si mas del 1% de las
	 * palabras ya no coincide.  Wii64 (glN64_GX/FrameBuffer.cpp:109) se queda
	 * solo con la huella y sale aqui sin guardar nada, con lo que un buffer
	 * principal era valido PARA SIEMPRE.
	 *
	 * Conker usa buffers de 292x216 que abandona (001503E0, 001E5270,
	 * 001A4170...) y luego reutiliza esa memoria para texturas.  Una carga que
	 * cayera ahi se servia como textura de framebuffer -- la copia vieja, negra
	 * -- en vez de la textura: la parte de arriba de las zapatillas en la intro
	 * y parches negros en la pared.  Con la emulacion de framebuffer apagada
	 * salen bien (comprobado por el usuario).
	 *
	 * Los borrados que el plugin escribe en la RDRAM (`_gDPFillRDRAM`), que
	 * GLideN64 valida aparte con `m_cleared`, rehacen la copia por
	 * FrameBuffer_RestampMarkers (`forzar`). */
	if (!_fbIsAuxiliary( buffer ))
	{
		/* [Conker, sprites del suelo, 2026-10-16] ★★ LA COPIA UNA SOLA VEZ, como
		 * GLideN64 (src/FrameBuffer.cpp:803-810: `copyRdram()` solo si
		 * `m_RdramCopy.empty()`).  Refrescarla en cada guardado se traga lo que
		 * el JUEGO escriba en esa RDRAM entre guardados: Conker carga ahi sus
		 * sprites, la copia los absorbe, el buffer sigue "valido" y las cargas
		 * que por casualidad empiezan en una fila (log 5: ~90/s, 0x15A578 =
		 * 0x14CCC0 + 95 filas) salen con un trozo de la escena.  MEDIDO (log 6):
		 * con la copia unica, tras rehacerse el buffer, 0 cargas de sprites
		 * servidas (solo la sombra, 0x0DE080) en varias entradas a la cueva;
		 * controles de framebuffer confirmados por el usuario.  Lo que
		 * escribimos NOSOTROS sigue rehaciendo la copia (`forzar`, desde
		 * FrameBuffer_RestampMarkers). */
		if (!forzar && buffer->rdramCopy != NULL && buffer->rdramCopySize == dataSize)
			return;

		if (buffer->rdramCopySize != dataSize)
		{
			_fbFreeRdramCopy( buffer );
			buffer->rdramCopy = (u8*)malloc( dataSize );
			buffer->rdramCopySize = (buffer->rdramCopy != NULL) ? dataSize : 0;
		}
		if (buffer->rdramCopy != NULL)
			memcpy( buffer->rdramCopy, &RDRAM[buffer->startAddress], dataSize );
		return;
	}

	_fbFreeRdramCopy( buffer );

	const u32 twoPercent = (dataSize / 200 > 4) ? (dataSize / 200) : 4;
	u32 *dst = (u32*)&RDRAM[buffer->startAddress];

	for (u32 i = 0; i < twoPercent; i++)
		dst[i] = (i < 4) ? fbFingerprint[i] : 0;

	buffer->fingerprint = TRUE;
}

BOOL FrameBuffer_IsValid( FrameBuffer *buffer )
{
	if (buffer == NULL)
		return FALSE;

	if (buffer->validityChecked == FB_frame)
		return TRUE;
	buffer->validityChecked = FB_frame;

	// [Xbox360] GLideN64's FrameBuffer::isValid(), the m_RdramCopy branch: the buffer
	// is dead once 1% of its dwords no longer match. See FrameBuffer_CopyRdram().
	if (buffer->rdramCopy != NULL)
	{
		const u32 *pData = (const u32*)&RDRAM[buffer->startAddress];
		const u32 *pCopy = (const u32*)buffer->rdramCopy;
		const u32 sizeDwords = buffer->rdramCopySize >> 2;
		u32 wrongPixels = 0;

		for (u32 i = 0; i < sizeDwords; i++)
			if ((pData[i] & 0xFFFEFFFE) != (pCopy[i] & 0xFFFEFFFE))
				++wrongPixels;

		return (wrongPixels < buffer->rdramCopySize / 400) ? TRUE : FALSE;
	}

	if (!buffer->fingerprint)
		return TRUE;

	const u32 *src = (const u32*)&RDRAM[buffer->startAddress];
	for (u32 i = 0; i < 4; i++)
		if ((src[i] & 0xFFFEFFFE) != (fbFingerprint[i] & 0xFFFEFFFE))
			return FALSE;

	return TRUE;
}

void FrameBuffer_Init()
{
	frameBuffer.current = NULL;
	frameBuffer.top = NULL;
	frameBuffer.bottom = NULL;
	frameBuffer.numBuffers = 0;
	fbUnderConstruction = NULL;
}

static BOOL _fbIsLive( const FrameBuffer *buffer );

// Eviction for the texture cache: the least recently used buffer that is neither being
// built, scanned out nor rendered into.
void FrameBuffer_RemoveBottom()
{
	FrameBuffer *victim = frameBuffer.bottom;

	while (victim != NULL && (victim == fbUnderConstruction || _fbIsLive( victim )))
		victim = victim->higher;

	if (victim != NULL)
		FrameBuffer_Remove( victim );
}

void FrameBuffer_Remove( FrameBuffer *buffer )
{
	if (buffer == NULL)
		return;

	if ((buffer == frameBuffer.bottom) &&
		(buffer == frameBuffer.top))
	{
		frameBuffer.top = NULL;
		frameBuffer.bottom = NULL;
	}
	else if (buffer == frameBuffer.bottom)
	{
		frameBuffer.bottom = buffer->higher;

		if (frameBuffer.bottom)
			frameBuffer.bottom->lower = NULL;
	}
	else if (buffer == frameBuffer.top)
	{
		frameBuffer.top = buffer->lower;

		if (frameBuffer.top)
			frameBuffer.top->higher = NULL;
	}
	else
	{
		buffer->higher->lower = buffer->lower;
		buffer->lower->higher = buffer->higher;
	}

	if (buffer->texture)
	{
		CachedTexture *texture = buffer->texture;

		// Clearing buffer->texture, rather than the frameBufferTexture flag, is what
		// stops TextureCache_Remove() coming back around through
		// FrameBuffer_RemoveBufferForTexture(). It looks this buffer up by texture and
		// no longer finds it. The flag has to stay around, because it is what tells
		// _texHeapFree() the GP may still be writing to the block. (Vigilante 8 menu crash)
		buffer->texture = NULL;
		TextureCache_Remove( texture );
	}

	_fbFreeRdramCopy( buffer );
	free( buffer );

	frameBuffer.numBuffers--;
}

void FrameBuffer_InvalidateBuffer( u32 address )
{
	FrameBuffer *current = frameBuffer.bottom;

	while (current != NULL)
	{
		if (current->startAddress == address)
		{
			FrameBuffer_Remove( current );
			return;
		}
		current = current->higher;
	}
}

void FrameBuffer_RemoveBufferForTexture( CachedTexture *texture )
{
	FrameBuffer *current = frameBuffer.bottom;

	while (current != NULL)
	{
		if (current->texture == texture)
		{
			current->texture = NULL;
			FrameBuffer_Remove( current );
			return;
		}
		current = current->higher;
	}
}

// GLideN64's isOverlapping()
static BOOL _fbIsOverlapping( const FrameBuffer *b1, const FrameBuffer *b2 )
{
	if (b1->endAddress < b2->endAddress && b1->width == b2->width && b1->size == b2->size &&
	    b1->startAddress < b2->startAddress)
	{
		const u32 stride = b1->width << b1->size >> 1;
		const u32 diff = b1->endAddress - b2->startAddress + 1;

		if (stride != 0 && (diff % stride) == 0)
			return TRUE;
	}

	return FALSE;
}

static BOOL _fbIsLive( const FrameBuffer *buffer )
{
	if (REG.VI_ORIGIN != NULL)
	{
		const u32 origin = *REG.VI_ORIGIN & 0x00FFFFFF;

		if (buffer->startAddress <= origin && buffer->endAddress >= origin)
			return TRUE;	// on screen right now
	}

	if (buffer->startAddress == gDP.colorImage.address)
		return TRUE;		// the current render target

	return FALSE;
}

BOOL FrameBuffer_IsTextureLive( const CachedTexture *texture )
{
	FrameBuffer *current = frameBuffer.top;

	while (current != NULL)
	{
		if (current->texture == texture)
			return _fbIsLive( current );
		current = current->lower;
	}

	return FALSE;
}

// GLideN64's isSubBuffer()
static FrameBuffer *_fbFindSubBuffer( u32 address, u16 size, u16 width )
{
	FrameBuffer *current = frameBuffer.top;
	const u32 stride = (u32)width << size >> 1;

	if (stride == 0)
		return NULL;

	while (current != NULL)
	{
		if (current->width == width && current->size == size &&
		    address > current->startAddress && address <= current->endAddress)
		{
			const u32 diff = address - current->startAddress;

			if ((diff % stride) != 0 && (diff / stride) < 5)
				return current;
		}

		current = current->lower;
	}

	return NULL;
}

static FrameBuffer *_fbFindTarget( u32 address, u16 size, u16 width )
{
	FrameBuffer *current = frameBuffer.top;

	while (current != NULL)
	{
		if (current->startAddress == address &&
		    current->width == width && current->size == size)
			return current;

		current = current->lower;
	}

	return _fbFindSubBuffer( address, size, width );
}

static BOOL _fbMayEvict( const FrameBuffer *victim, const FrameBuffer *current )
{
	if (_fbIsLive( victim ))
		return FALSE;

	if (!_fbIsAuxiliary( current ))
		return TRUE;

	return _fbIsAuxiliary( victim ) ? TRUE : FALSE;
}

void FrameBuffer_RemoveIntersections( FrameBuffer *current )
{
	if (current == NULL)
		return;

	FrameBuffer *iter = frameBuffer.bottom;
	while (iter != NULL)
	{
		FrameBuffer *next = iter->higher;

		if (iter != current)
		{
			if (iter->startAddress <= current->startAddress &&
			    iter->endAddress >= current->startAddress)
			{
				if (_fbIsOverlapping( iter, current ))
				{
					iter->endAddress = current->startAddress - 1;
				}
				else if (_fbMayEvict( iter, current ))
					FrameBuffer_Remove( iter );
			}
			else if (current->startAddress <= iter->startAddress &&
			         current->endAddress >= iter->startAddress)
			{
				if (_fbIsOverlapping( current, iter ))
				{
					current->endAddress = iter->startAddress - 1;
				}
				else if (_fbMayEvict( iter, current ))
					FrameBuffer_Remove( iter );
			}
		}

		iter = next;
	}

}

FrameBuffer *FrameBuffer_AddTop()
{
	FrameBuffer *newtop = (FrameBuffer*)malloc( sizeof( FrameBuffer ) );

	if (newtop == NULL)
	{
		return NULL;
	}

	memset( newtop, 0, sizeof( FrameBuffer ) );

	newtop->texture = TextureCache_AddTop();
	if (newtop->texture == NULL)
	{
		free( newtop );
		return NULL;
	}

#ifdef __GX__
	newtop->texture->VIcount = 0;
#endif //__GX__

	newtop->lower = frameBuffer.top;
	newtop->higher = NULL;

	if (frameBuffer.top)
		frameBuffer.top->higher = newtop;

	if (!frameBuffer.bottom)
		frameBuffer.bottom = newtop;

    frameBuffer.top = newtop;

	frameBuffer.numBuffers++;

	return newtop;
}

void FrameBuffer_MoveToTop( FrameBuffer *newtop )
{
#ifdef __GX__
	newtop->texture->VIcount = 0;
#endif //__GX__

	if (newtop == frameBuffer.top)
		return;

	if (newtop == frameBuffer.bottom)
	{
		frameBuffer.bottom = newtop->higher;
		frameBuffer.bottom->lower = NULL;
	}
	else
	{
		newtop->higher->lower = newtop->lower;
		newtop->lower->higher = newtop->higher;
	}

	newtop->higher = NULL;
	newtop->lower = frameBuffer.top;
	frameBuffer.top->higher = newtop;
	frameBuffer.top = newtop;

	TextureCache_MoveToTop( newtop->texture );	// VIcount already reset above
}

// GLideN64's FrameBufferList::removeBuffers()
void FrameBuffer_RemoveBuffersOfWidth( u32 width )
{
	FrameBuffer *current = frameBuffer.bottom;

	while (current != NULL)
	{
		FrameBuffer *next = current->higher;

		if (current->width == width)
		{
			FrameBuffer_Remove( current );
		}

		current = next;
	}
}

void FrameBuffer_Destroy()
{
	fbUnderConstruction = NULL;
	while (frameBuffer.bottom)
		FrameBuffer_Remove( frameBuffer.bottom );
}

void FrameBuffer_RefreshCurrent( FrameBuffer *buffer )
{
	if (buffer == NULL || buffer->texture == NULL || buffer->texture->GXtexture == NULL)
		return;

	/* [Xbox360] ★ MEDIDO, NO SE TOCA: GLideN64 copia el buffer actual en CADA
	 * lectura y Wii64 como mucho una vez por frame.  Se probo la de la
	 * referencia para la estela de la bola de Mario Tennis (log 12): no cambio
	 * nada en pantalla -- en ese momento la puerta solo se habia saltado 4
	 * copias -- y costo rendimiento visible (2705 copias, 2381 de ellas
	 * repetidas en el mismo frame).  Se queda la de Wii64. */
	if (buffer->startAddress != gDP.colorImage.address || buffer->refreshedFrame == FB_frame)
		return;

	buffer->refreshedFrame = FB_frame;

	OGL_ApplyPendingClears();

	GX_SetTexCopySrc(OGL.GXorigX, OGL.GXorigY,
	                 (u16) buffer->texture->realWidth, (u16) buffer->texture->realHeight);
	GX_SetTexCopyDst((u16) buffer->texture->realWidth, (u16) buffer->texture->realHeight,
	                 buffer->texture->GXtexfmt, GX_COPY_PROGRESSIVE);
	GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
	DCInvalidateRange(buffer->texture->GXtexture, buffer->texture->textureBytes);
	GX_CopyTex(buffer->texture->GXtexture, GX_FALSE);
	GXD3D_SetCopyMono(buffer->texture->GXtexture, (buffer->size == G_IM_SIZ_8b) ? 1 : 0);
	GX_InvalidateTexAll();
	GX_PixModeSync();
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
}

void FrameBuffer_SaveBuffer( u32 address, u16 size, u16 width, u16 height )
{
	// The EFB copy below has to see the game's clear, not what preceded it.
	OGL_ApplyPendingClears();

	// height is deliberately not part of the key
	FrameBuffer *current = _fbFindTarget( address, size, width );

	if (current != NULL)
	{
#ifndef __GX__
		const BOOL stale = ((current->scaleX != OGL.scaleX) ||
		                    (current->scaleY != OGL.scaleY) ||
		                    (current->height < height)) ? TRUE : FALSE;
#else //!__GX__
		const BOOL stale = ((current->scaleX != OGL.GXscaleX) ||
		                    (current->scaleY != OGL.GXscaleY) ||
		                    (current->height < height)) ? TRUE : FALSE;
#endif //__GX__

		// The buffer no longer describes what we would render now, so drop it and build
		// a replacement below.
		if (stale)
		{
			FrameBuffer_Remove( current );
			current = NULL;
		}
	}

	if (current != NULL)
	{
		{
#ifndef __GX__
			glBindTexture( GL_TEXTURE_2D, current->texture->glName );
			glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, OGL.height - current->texture->height + OGL.heightOffset, current->texture->width, current->texture->height );
#else // !__GX__
			//Note: texture realWidth and realHeight should be multiple of 2!
			GX_SetTexCopySrc(OGL.GXorigX, OGL.GXorigY,(u16) current->texture->realWidth,(u16) current->texture->realHeight);
			GX_SetTexCopyDst((u16) current->texture->realWidth,(u16) current->texture->realHeight, current->texture->GXtexfmt, GX_COPY_PROGRESSIVE);
			GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
			if (current->texture->GXtexture)
			{
				DCInvalidateRange(current->texture->GXtexture, current->texture->textureBytes);
				GX_CopyTex(current->texture->GXtexture, GX_FALSE);
				GXD3D_SetCopyMono(current->texture->GXtexture, (current->size == G_IM_SIZ_8b) ? 1 : 0);
				GX_InvalidateTexAll();
			}
			GX_PixModeSync();
			GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
#endif // __GX__

			current->isDepthBuffer = (address == gDP.depthImageAddress) ? TRUE : FALSE;
			_fbUpdateEndAddress( current );
			FrameBuffer_RemoveIntersections( current );
			FrameBuffer_CopyRdram( current );

			current->changed = TRUE;

			FrameBuffer_MoveToTop( current );

			gSP.changed |= CHANGED_TEXTURE;
			return;
		}
	}

	FrameBuffer *stale = frameBuffer.bottom;

	while (stale != NULL)
	{
		FrameBuffer *next = stale->higher;

		if (stale->startAddress == address &&
		    (stale->width != width || stale->size != size))
		{
			FrameBuffer_Remove( stale );
		}

		stale = next;
	}

	// Wasn't found, create a new one
	current = FrameBuffer_AddTop();
	if (current == NULL)
	{
		return;
	}

#ifdef __GX__
	fbUnderConstruction = current;
#endif // __GX__

	current->startAddress = address;
	current->width = width;
	current->height = height;
	current->size = size;
	current->isDepthBuffer = (address == gDP.depthImageAddress) ? TRUE : FALSE;
	_fbUpdateEndAddress( current );

	// Before allocating, so an overlapping stale buffer gives its texture back first.
	FrameBuffer_RemoveIntersections( current );
#ifndef __GX__
	current->scaleX = OGL.scaleX;
	current->scaleY = OGL.scaleY;
#else //!__GX__
	current->scaleX = OGL.GXscaleX;
	current->scaleY = OGL.GXscaleY;
#endif //__GX__

#ifndef __GX__
	current->texture->width = (unsigned long)(current->width * OGL.scaleX);
	current->texture->height = (unsigned long)(current->height * OGL.scaleY);
#else //!__GX__
	{
		/* [Xbox360] La TEXTURA con el alto cargado de la RDRAM, el rango con el
		 * dibujado.  Ver s_fbCargadoDir. */
		u32 altoTex = current->height;
		if (address == s_fbCargadoDir && width == s_fbCargadoAncho &&
		    size == s_fbCargadoSize && s_fbCargadoAlto > altoTex)
			altoTex = s_fbCargadoAlto;
		s_fbCargadoDir = 0;
		current->texture->width = (unsigned long)(current->width * OGL.GXscaleX);
		current->texture->height = (unsigned long)(altoTex * OGL.GXscaleY);
	}
	if (current->texture->width  > OGL.GXwidth)  current->texture->width  = OGL.GXwidth;
	if (current->texture->height > OGL.GXheight) current->texture->height = OGL.GXheight;
#endif //__GX__
	current->texture->clampS = 1;
	current->texture->clampT = 1;
	current->texture->address = current->startAddress;
	current->texture->clampWidth = current->width;
	current->texture->clampHeight = current->height;
	current->texture->frameBufferTexture = TRUE;
	current->texture->maskS = 0;
	current->texture->maskT = 0;
	current->texture->mirrorS = 0;
	current->texture->mirrorT = 0;
#ifndef __GX__
	current->texture->realWidth = pow2( (unsigned long)(current->width * OGL.scaleX) );
	current->texture->realHeight = pow2( (unsigned long)(current->height * OGL.scaleY) );
	current->texture->textureBytes = current->texture->realWidth * current->texture->realHeight * 4;
#else //!__GX__
	//realWidth & realHeight should be multiple of 2 for EFB->Texture Copy
	if(current->texture->width & 0x1)	current->texture->realWidth = current->texture->width + 1;
	else								current->texture->realWidth = current->texture->width;
	if(!current->texture->realWidth)	current->texture->realWidth = 2;

	if(current->texture->height & 0x1)	current->texture->realHeight = current->texture->height + 1;
	else								current->texture->realHeight = current->texture->height;
	if(!current->texture->realHeight)	current->texture->realHeight = 2;

/*	if(current->texture->width % 2 || current->texture->width == 0)
		current->texture->realWidth = current->texture->width + 2 - (current->texture->width % 2);
	else
		current->texture->realWidth = current->texture->width;
	if(current->texture->height % 2 || current->texture->height == 0)
		current->texture->realHeight = current->texture->height + 2 - (current->texture->height % 2);
	else
		current->texture->realHeight = current->texture->height;*/

	//GXrealWidth and GXrealHeight should be multiple of 4 for GXtexture
	if(current->texture->realWidth & 0x2)	current->texture->GXrealWidth = current->texture->realWidth + 2;
	else									current->texture->GXrealWidth = current->texture->realWidth;
	if(current->texture->realHeight & 0x2)	current->texture->GXrealHeight = current->texture->realHeight + 2;
	else									current->texture->GXrealHeight = current->texture->realHeight;

/*	if(current->texture->realWidth % 4)
		current->texture->GXrealWidth = current->texture->realWidth + 4 - (current->texture->realWidth % 4);
	else
		current->texture->GXrealWidth = current->texture->realWidth;
	if(current->texture->realHeight % 4)
		current->texture->GXrealHeight = current->texture->realHeight + 4 - (current->texture->realHeight % 4);
	else
		current->texture->GXrealHeight = current->texture->realHeight;*/

#ifdef HW_RVL
	current->texture->textureBytes = (current->texture->GXrealWidth * current->texture->GXrealHeight) * 4;
	current->texture->GXtexfmt = GX_TF_RGBA8;
#else //HW_RVL
	current->texture->textureBytes = (current->texture->GXrealWidth * current->texture->GXrealHeight) * 2;
	current->texture->GXtexfmt = GX_TF_RGB565;
#endif //!HW_RVL

	current->texture->GXtexture = (u16*) __lwp_heap_allocate(GXtexCache,current->texture->textureBytes);
	TextureCache_PinTexture( current->texture );
	while (!current->texture->GXtexture && TextureCache_FreeOneTexture())
		current->texture->GXtexture = (u16*) __lwp_heap_allocate(GXtexCache,current->texture->textureBytes);
	TextureCache_PinTexture( NULL );

	if (current->texture->GXtexture == NULL)
	{
		current->texture->textureBytes = 0;
		fbUnderConstruction = NULL;
		FrameBuffer_Remove( current );
		return;
	}

	fbUnderConstruction = NULL;
#endif //__GX__
	cache.cachedBytes += current->texture->textureBytes;

#ifndef __GX__
	glBindTexture( GL_TEXTURE_2D, current->texture->glName );
	glCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 0, OGL.height - current->texture->height + OGL.heightOffset, current->texture->realWidth, current->texture->realHeight, 0 );
#else // !__GX__
	//Note: texture realWidth and realHeight should be multiple of 2!
	GX_SetTexCopySrc((u16) OGL.GXorigX, (u16) OGL.GXorigY,(u16) current->texture->realWidth,(u16) current->texture->realHeight);
	GX_SetTexCopyDst((u16) current->texture->realWidth,(u16) current->texture->realHeight, current->texture->GXtexfmt, GX_COPY_PROGRESSIVE);
	GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
	if (current->texture->GXtexture)
	{
		DCInvalidateRange(current->texture->GXtexture, current->texture->textureBytes);
		GX_CopyTex(current->texture->GXtexture, GX_FALSE);
		GXD3D_SetCopyMono(current->texture->GXtexture, (current->size == G_IM_SIZ_8b) ? 1 : 0);
		GX_InvalidateTexAll();
	}
	GX_PixModeSync();
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
#endif // __GX__

	FrameBuffer_CopyRdram( current );

	current->changed = TRUE;

	gSP.changed |= CHANGED_TEXTURE;
}

void FrameBuffer_RenderBuffer( u32 address )
{
	OGL_ApplyPendingClears();

	FrameBuffer *displayed = FrameBuffer_FindBuffer( address );
	FrameBuffer *current = frameBuffer.top;

	while (current != NULL)
	{
		if (current == displayed)
		{
#ifndef __GX__
			glPushAttrib( GL_ENABLE_BIT | GL_VIEWPORT_BIT );
#endif // __GX__

			Combiner_BeginTextureUpdate();
			TextureCache_ActivateTexture( 0, current->texture );
			Combiner_SetCombine( EncodeCombineMode( 0, 0, 0, TEXEL0, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1 ) );
/*			if (OGL.ARB_multitexture)
			{
				for (int i = 0; i < OGL.maxTextureUnits; i++)
				{
					glActiveTextureARB( GL_TEXTURE0_ARB + i );
					glDisable( GL_TEXTURE_2D );
				}

				glActiveTextureARB( GL_TEXTURE0_ARB );
			}

			TextureCache_ActivateTexture( 0, current->texture );
			glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
			glEnable( GL_TEXTURE_2D );*/

#ifndef __GX__
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glDisable( GL_DEPTH_TEST );
			glDisable( GL_CULL_FACE );
			glDisable( GL_POLYGON_OFFSET_FILL );
//			glDisable( GL_REGISTER_COMBINERS_NV );
			glDisable( GL_FOG );

			glMatrixMode( GL_PROJECTION );
			glLoadIdentity();
 			glOrtho( 0, OGL.width, 0, OGL.height, -1.0f, 1.0f );
			glViewport( 0, OGL.heightOffset, OGL.width, OGL.height );
			glDisable( GL_SCISSOR_TEST );

			float u1, v1;

			u1 = (float)current->texture->width / (float)current->texture->realWidth;
			v1 = (float)current->texture->height / (float)current->texture->realHeight;

			glDrawBuffer( GL_FRONT );
			glBegin(GL_QUADS);
 				glTexCoord2f( 0.0f, 0.0f );
				glVertex2f( 0.0f, OGL.height - current->texture->height );

				glTexCoord2f( 0.0f, v1 );
				glVertex2f( 0.0f, OGL.height );

 				glTexCoord2f( u1,  v1 );
				glVertex2f( current->texture->width, OGL.height );

 				glTexCoord2f( u1, 0.0f );
				glVertex2f( current->texture->width, OGL.height - current->texture->height );
			glEnd();
			glDrawBuffer( GL_BACK );

/*			glEnable( GL_TEXTURE_2D );
			glActiveTextureARB( GL_TEXTURE0_ARB );
			glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );*/
			glPopAttrib();
#else // !__GX__

			GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR); 
			GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
			GX_SetZMode(GX_DISABLE,GX_ALWAYS,GX_FALSE);
			GX_SetCullMode (GX_CULL_NONE);
			GX_SetFog(GX_FOG_NONE,0.1,1.0,0.0,1.0,GXCOL_BLACK);   /* [Xbox360] literal compuesto: MSVC no lo acepta */

			Mtx44 GXprojection;
			guOrtho(GXprojection, 0, OGL.height, 0, OGL.width, 0.0f, 1.0f);
			GX_LoadProjectionMtx(GXprojection, GX_ORTHOGRAPHIC); 
			GX_LoadPosMtxImm(OGL.GXmodelViewIdent,GX_PNMTX0);
			GX_SetViewport((f32) 0,(f32) 0,(f32) OGL.width,(f32) OGL.height, 0.0f, 1.0f);
			GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);	//Set to the same size as the viewport.

			float u1, v1;

			u1 = (float)current->texture->width / (float)current->texture->realWidth;
			v1 = (float)current->texture->height / (float)current->texture->realHeight;

			/* [Xbox360] [Beetle Adventure Racing, transicion entre menus] El
			 * juego deja DOS color buffers seguidos en la RDRAM y va moviendo
			 * el VI_ORIGIN de uno al otro: la pantalla es el final del primero
			 * y, debajo, el principio del segundo.  Wii64 pintaba siempre el
			 * buffer entero desde su fila 0.  GLideN64 FrameBufferList::
			 * renderBuffer (FrameBuffer.cpp:1508-1550 y 1637-1668; buffers
			 * encadenados 21d64691, deteccion b18ae00b): fila de origen srcY0
			 * por la distancia del origen al inicio del buffer; si la ultima
			 * fila barrida cae en OTRO buffer, se pinta el primero desde srcY0
			 * arriba y el siguiente, filas 0..srcY0, debajo.  Sin siguiente
			 * buffer queda como estaba (de la fila 0). */
			FrameBuffer *siguiente = NULL;
			u32 srcY0 = 0, srcAlto = 0;
			{
				const u32 desplaz = ((address - current->startAddress) << 1) >> current->size;
				const u32 zancada = (current->width << current->size) >> 1;

				srcY0 = (current->width != 0) ? desplaz / current->width : 0;
				if (desplaz != 0 && current->width == desplaz * 2)
					srcY0 = 1;
				srcAlto = VI.height;
				if (srcAlto > current->height) srcAlto = current->height;

				/* ★ [DK64, la intro temblaba, 2026-10-23] SOLO CON
				 * hack_viDosBuffers (Beetle).  Medido con [VIPART]: la intro de
				 * DK64 (640x480 entrelazado) tiene el origen 2 filas dentro del
				 * buffer en TODAS las pantallas, y solo en 15 de cada 60 habia
				 * otro buffer pegado: esas se pintaban desde la fila 2 y las
				 * demas desde la 0 -> la imagen saltaba 2 filas.  GLideN64 no
				 * tiembla porque aplica SIEMPRE el desplazamiento, con su ajuste
				 * de campos entrelazados (`!vi_lowerfield -> --srcY0`); eso no
				 * esta portado, y sin ello el reparto solo es seguro donde se
				 * midio. */
				if ((config.generalEmulation.hacks & hack_viDosBuffers) != 0 &&
				    srcY0 > 0 && srcAlto > 0 && zancada != 0)
				{
					siguiente = FrameBuffer_FindBuffer( address + zancada * (srcAlto - 1) - 1 );
					if (siguiente == current || (siguiente != NULL && siguiente->texture == NULL))
						siguiente = NULL;
				}
			}
			float yTex0 = 0.0f, yTex1 = (float)current->texture->height;
			float v0 = 0.0f;
			if (siguiente != NULL)
			{
				yTex0 = (float)srcY0 * current->scaleY;
				yTex1 = (float)srcAlto * current->scaleY;
				if (yTex1 > (float)current->texture->height) yTex1 = (float)current->texture->height;
				if (yTex0 >= yTex1)
					siguiente = NULL;
				else
				{
					v0 = yTex0 / (float)current->texture->realHeight;
					v1 = yTex1 / (float)current->texture->realHeight;
				}
			}
			if (siguiente == NULL)
			{
				yTex0 = 0.0f;
				yTex1 = (float)current->texture->height;
			}
			const float altoParte1 = yTex1 - yTex0;

//			glDrawBuffer( GL_FRONT );

			//set vertex description here
			GX_ClearVtxDesc();
			GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
			GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
			GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
			GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
			//set vertex attribute formats here
			GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
			GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
			/* [Xbox360] Repintado INTERNO del plugin, no un dibujo del juego: sin
			 * el marcado de xbox64_marcatex.  Con `combinador` este quad salia de
			 * un color plano y tapaba la pantalla entera (Perfect Dark, todo
			 * verde).  El shim corta el lote al cambiar el modo
			 * (gxd3d_tev_difiere). */
			const int marcaGuardada = gln64_marcaluz;
			gln64_marcaluz = 0;
			GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
				GX_Position2f32( (f32) OGL.GXorigX, (f32) OGL.GXorigY );
				GX_TexCoord2f32( 0.0f, v0 );
				GX_Position2f32( (f32) OGL.GXorigX + current->texture->width, (f32) OGL.GXorigY );
				GX_TexCoord2f32( u1, v0 );
				GX_Position2f32( (f32) OGL.GXorigX + current->texture->width, (f32) OGL.GXorigY + altoParte1 );
				GX_TexCoord2f32( u1, v1 );
				GX_Position2f32( (f32) OGL.GXorigX, (f32) OGL.GXorigY + altoParte1 );
				GX_TexCoord2f32( 0.0f, v1 );
			GX_End();
			/* [Xbox360] La segunda parte: el principio del buffer siguiente,
			 * srcY0 filas, justo debajo de la primera (GLideN64 1637-1668). */
			if (siguiente != NULL)
			{
				float nyTex1 = (float)srcY0 * siguiente->scaleY;
				if (nyTex1 > (float)siguiente->texture->height) nyTex1 = (float)siguiente->texture->height;
				const float nu1 = (float)siguiente->texture->width / (float)siguiente->texture->realWidth;
				const float nv1 = nyTex1 / (float)siguiente->texture->realHeight;
				const float y0 = (f32) OGL.GXorigY + altoParte1;
				const float y1 = y0 + (float)srcY0 * current->scaleY;

				TextureCache_ActivateTexture( 0, siguiente->texture );
				GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
					GX_Position2f32( (f32) OGL.GXorigX, y0 );
					GX_TexCoord2f32( 0.0f, 0.0f );
					GX_Position2f32( (f32) OGL.GXorigX + siguiente->texture->width, y0 );
					GX_TexCoord2f32( nu1, 0.0f );
					GX_Position2f32( (f32) OGL.GXorigX + siguiente->texture->width, y1 );
					GX_TexCoord2f32( nu1, nv1 );
					GX_Position2f32( (f32) OGL.GXorigX, y1 );
					GX_TexCoord2f32( 0.0f, nv1 );
				GX_End();
			}
			gln64_marcaluz = marcaGuardada;

//			glDrawBuffer( GL_BACK );

			OGL.GXupdateMtx = true;

#endif // __GX__

			current->changed = FALSE;

#ifdef __GX__
			if (siguiente != NULL)
				FrameBuffer_MoveToTop( siguiente );
#endif // __GX__
			FrameBuffer_MoveToTop( current );
#ifndef __GX__
			gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT;
			gDP.changed |= CHANGED_COMBINE;
#else //!__GX__
			gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT | CHANGED_GEOMETRYMODE;
			gDP.changed |= CHANGED_COMBINE | CHANGED_SCISSOR | CHANGED_RENDERMODE;
#endif //__GX__
			return;
		}
		current = current->lower;
	}
}

void FrameBuffer_RestoreBuffer( u32 address, u16 size, u16 width )
{
	OGL_ApplyPendingClears();

	FrameBuffer *target = _fbFindTarget( address, size, width );
	FrameBuffer *current = frameBuffer.top;

	/* [Xbox360] sondas temporales [REST-GX] / [RDRAM-GX]; ver gln64_prof.h. */
#if XBOX64_TRAZAS
	gln64_restore_nota( address, width, size, target != NULL );
	if (target != NULL)
		gln64_rdram_nota( address, target->width, target->height, target->size );
#endif

	while (current != NULL)
	{
		if (current == target)
		{
#ifndef __GX__
			glPushAttrib( GL_ENABLE_BIT | GL_VIEWPORT_BIT );
#endif // __GX__

/*			if (OGL.ARB_multitexture)
			{
				for (int i = 0; i < OGL.maxTextureUnits; i++)
				{
					glActiveTextureARB( GL_TEXTURE0_ARB + i );
					glDisable( GL_TEXTURE_2D );
				}

				glActiveTextureARB( GL_TEXTURE0_ARB );
			}

			TextureCache_ActivateTexture( 0, current->texture );
			glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
			glEnable( GL_TEXTURE_2D );*/
			Combiner_BeginTextureUpdate();
			TextureCache_ActivateTexture( 0, current->texture );
			Combiner_SetCombine( EncodeCombineMode( 0, 0, 0, TEXEL0, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1 ) );

#ifndef __GX__
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glDisable( GL_DEPTH_TEST );
			glDisable( GL_SCISSOR_TEST );
			glDisable( GL_CULL_FACE );
			glDisable( GL_POLYGON_OFFSET_FILL );
			//glDisable( GL_REGISTER_COMBINERS_NV );
			glDisable( GL_FOG );
//			glDepthMask( FALSE );

			glMatrixMode( GL_PROJECTION );
			glLoadIdentity();
 			glOrtho( 0, OGL.width, 0, OGL.height, -1.0f, 1.0f );
//			glOrtho( 0, RDP.width, RDP.height, 0, -1.0f, 1.0f );
			glViewport( 0, OGL.heightOffset, OGL.width, OGL.height );

			float u1, v1;

			u1 = (float)current->texture->width / (float)current->texture->realWidth;
			v1 = (float)current->texture->height / (float)current->texture->realHeight;

			glBegin(GL_QUADS); 
 				glTexCoord2f( 0.0f, 0.0f );
				glVertex2f( 0.0f, OGL.height - current->texture->height );

				glTexCoord2f( 0.0f, v1 );
				glVertex2f( 0.0f, OGL.height );

 				glTexCoord2f( u1,  v1 );
				glVertex2f( current->texture->width, OGL.height );

 				glTexCoord2f( u1, 0.0f );
				glVertex2f( current->texture->width, OGL.height - current->texture->height );
			glEnd();

			glLoadIdentity();
			glPopAttrib();
#else //!__GX__

			GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR); 
			GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
			GX_SetZMode(GX_DISABLE,GX_ALWAYS,GX_FALSE);
			GX_SetCullMode (GX_CULL_NONE);
			GX_SetFog(GX_FOG_NONE,0.1,1.0,0.0,1.0,GXCOL_BLACK);   /* [Xbox360] literal compuesto: MSVC no lo acepta */

			Mtx44 GXprojection;
			guOrtho(GXprojection, 0, OGL.height, 0, OGL.width, 0.0f, 1.0f);
			GX_LoadProjectionMtx(GXprojection, GX_ORTHOGRAPHIC); 
			GX_LoadPosMtxImm(OGL.GXmodelViewIdent,GX_PNMTX0);
			GX_SetViewport((f32) 0,(f32) 0,(f32) OGL.width,(f32) OGL.height, 0.0f, 1.0f);
			GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);	//Set to the same size as the viewport.

			float u1, v1;

			u1 = (float)current->texture->width / (float)current->texture->realWidth;
			v1 = (float)current->texture->height / (float)current->texture->realHeight;

			//set vertex description here
			GX_ClearVtxDesc();
			GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
			GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
			GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
			GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
			//set vertex attribute formats here
			GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
			GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
			/* [Xbox360] Repintado INTERNO del plugin, no un dibujo del juego: sin
			 * el marcado de xbox64_marcatex.  Con `combinador` este quad salia de
			 * un color plano y tapaba la pantalla entera (Perfect Dark, todo
			 * verde).  El shim corta el lote al cambiar el modo
			 * (gxd3d_tev_difiere). */
			const int marcaGuardada = gln64_marcaluz;
			gln64_marcaluz = 0;
			GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
				GX_Position2f32( (f32) OGL.GXorigX, (f32) OGL.GXorigY );
				GX_TexCoord2f32( 0.0f, 0.0f );
				GX_Position2f32( (f32) OGL.GXorigX + current->texture->width, (f32) OGL.GXorigY );
				GX_TexCoord2f32( u1, 0.0f );
				GX_Position2f32( (f32) OGL.GXorigX + current->texture->width, (f32) OGL.GXorigY + current->texture->height );
				GX_TexCoord2f32( u1, v1 );
				GX_Position2f32( (f32) OGL.GXorigX, (f32) OGL.GXorigY + current->texture->height );
				GX_TexCoord2f32( 0.0f, v1 );
			GX_End();
			gln64_marcaluz = marcaGuardada;

			OGL.GXupdateMtx = true;

#endif // __GX__

			FrameBuffer_MoveToTop( current );

#ifndef __GX__
			gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT;
			gDP.changed |= CHANGED_COMBINE;
#else //!__GX__
			gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT | CHANGED_GEOMETRYMODE;
			gDP.changed |= CHANGED_COMBINE | CHANGED_SCISSOR | CHANGED_RENDERMODE;
#endif //__GX__
			return;
		}
		current = current->lower;
	}
	float color[4] = {0.0f,0.0f,0.0f,0.0f};
	OGL_ClearColorBuffer( color );
#ifdef __GX__
	/* Buffer NUEVO: se carga desde la RDRAM al final de gDPSetColorImage,
	 * cuando ya rige su escala.  Ver FrameBuffer_GX_CargaNuevoDesdeRDRAM. */
	s_fbNuevoDir   = address;
	s_fbNuevoSize  = size;
	s_fbNuevoAncho = width;
#endif // __GX__
}

#ifdef __GX__
/* [Xbox360] ★★ [Mario Tennis, el marco del medidor de velocidad] UN
 * FRAMEBUFFER NUEVO EMPIEZA CON LO QUE HAY EN SU RDRAM, como GLideN64
 * 8b359761 ("Always render RDRAM content into newly created frame buffer.
 * Remove ugly hack for Mario Tennis scoreboard"): FrameBufferList::saveBuffer
 * llama a RDRAMtoColorBuffer::copyFromRDRAM(m_pCurrent) al crear el buffer, en
 * vez del clearColorBuffer(0,0,0,0) que tenia -- el mismo borrado a negro que
 * hace aqui arriba FrameBuffer_RestoreBuffer.
 *
 * El medidor de velocidad (y el marcador) es un buffer auxiliar que la CPU
 * deja YA DIBUJADO en la RDRAM -- el marco azul con "Nintendo" y "mph" -- y
 * sobre el que el RDP solo pinta los digitos.  Sin esto salian los digitos
 * sobre negro (foto del usuario).  Y como la CPU reescribe esa RDRAM, el buffer
 * deja de ser valido (FrameBuffer_IsValid), se quita y se vuelve a crear: la
 * carga se repite sola cuando cambia.
 *
 * Como la referencia (BufferCopy/RDRAMtoColorBuffer.cpp, _copyFromRDRAM con
 * `_fullAlpha` = true):
 *   - alto = VI_GetMaxBufferHeight(ancho) (240/480, 290/580 en PAL), cortado
 *     al final de la RDRAM (cutHeight);
 *   - alfa: la referencia lo pone SIEMPRE a 1 (RGBA16ToABGR32 con _fullAlpha);
 *     aqui se usa el bit de alfa del pixel (DIVERGENCIA, ver la conversion);
 *   - si la suma de los pixeles es 0 no se pinta nada (`summ != 0`);
 *   - solo 16 y 32 bits (`m_size < G_IM_SIZ_16b` -> nada).  Aqui solo 16: el
 *     de 32 necesita otra conversion y no hay juego medido que lo pida.
 * Mismo dibujo que VI_GX_copiaDesdeRDRAM (Dr. Mario) pero en las coordenadas
 * del EFB del buffer que entra, como FrameBuffer_RestoreBuffer. */
extern "C" volatile unsigned int gln64_dbg_fbrdram, gln64_dbg_fbrdram_px;

/* [Xbox360] ★ [Perfect Dark, la lista esperando ~9 ms en cada frame] TRES
 * BUFERES FIJOS, POR TURNO, no uno nuevo del monton en cada carga.  Antes se
 * pedia, se subia y se liberaba: el shim registra las texturas por el puntero
 * de los texels, asi que cada carga era una textura de D3D NUEVA, y crearla es
 * SINCRONO (el hilo de la lista espera al consumidor, hasta un frame).
 * Medido: [CREATEX] subida=22-40 de 320x240 por segundo (log 14), una por
 * lista, y [HILO2] sinc=16 con 147 ms de espera (log 13).  Con el bufer fijo
 * se avisa con GXD3D_TexturaCambiada y el shim vuelve a subir sobre la misma
 * textura.  Tres por turno: la GPU puede ir hasta un par de frames detras, y
 * asi nunca se sube sobre la que todavia esta leyendo.  640x580 es el mayor
 * alto de VI_GetMaxBufferHeight con 640 de ancho; lo que no quepa va por el
 * monton como antes. */
/* [Xbox360] ★ [Paper Mario, escena de Bowser: 216 cargas de 320x240 por
 * segundo] LA CARGA, APLAZADA AL PRIMER DIBUJO.  Medido con la sonda [PMFB]
 * (log 21): el buffer que se recargaba era siempre el mismo (00159880 de 320,
 * 256 de cada 256 cargas) y NADIE quitaba buffers: nunca se llegaba a
 * guardar, porque glN64 solo guarda al salir si se dibujo algo
 * (`colorImage.changed`), y en ese color image el juego solo hace los
 * texrects de la paleta del "alumbrado 2D", que _gDPTexrectPaletteMod escribe
 * en la RDRAM SIN dibujar.  Cada vuelta a el (una por sprite iluminado, ~27
 * por lista) era un buffer "nuevo" y una carga entera.  GLideN64 crea el
 * buffer al poner el color image, una vez, y no tiene el problema.
 * Aplazada, la carga se hace justo antes del primer dibujo de verdad
 * (OGL_UpdateStates, que va antes de todos; y el buffer nuevo siempre llega
 * con un borrado que marca gDP.changed), y si no se dibuja nada no se hace.
 * Un borrado del color antes del primer dibujo la cancela: la taparia
 * entera, que es lo que pasaria en GLideN64 (carga y luego borrado).  Solo
 * con hack_texrectPaletteMod (gDPSetColorImage): es lo unico que deja un
 * color image sin dibujar.  Confirmado (log 22): de 216 cargas/s a 3 como
 * mucho, y la escena de Bowser ya no se ralentiza. */
void FrameBuffer_GX_CargaPendiente()
{
	if (s_fbNuevoDir != 0)
		FrameBuffer_GX_CargaNuevoDesdeRDRAM();
}

void FrameBuffer_GX_CancelaCargaPendiente()
{
	s_fbNuevoDir = 0;
}

#define FB_CARGA_BUFERES 3
#define FB_CARGA_TEXELS  (640u * 580u)
static __declspec(align(128)) u16 s_fbCargaMem[FB_CARGA_BUFERES][FB_CARGA_TEXELS];
static u32 s_fbCargaTurno = 0;

void FrameBuffer_GX_CargaNuevoDesdeRDRAM()
{
	const u32 dir = s_fbNuevoDir, ancho = s_fbNuevoAncho;
	const u32 size = s_fbNuevoSize;
	s_fbNuevoDir = 0;
	// Direccion impar: GLideN64 tampoco copia (_copyBufferFromRdram).
	if (dir == 0 || (dir & 1) != 0 || ancho == 0 || size != G_IM_SIZ_16b || !GXtexCache)
		return;
	// Solo si sigue siendo el color image que se acaba de poner.
	if (gDP.colorImage.address != dir || gDP.colorImage.width != ancho)
		return;

	// VI_GetMaxBufferHeight de GLideN64 (VI.cpp:26), con su VI.PAL y
	// VI.interlaced (VI.cpp:45, :59), que la VIInfo de glN64 no tiene.
	const BOOL pal = (REG.VI_V_SYNC && (*REG.VI_V_SYNC & 0x3ff) > 550) ? TRUE : FALSE;
	const BOOL entrelazado = (REG.VI_STATUS && (*REG.VI_STATUS & 0x40) != 0) ? TRUE : FALSE;
	u32 alto = (ancho > 320 || entrelazado) ? (pal ? 580 : 480) : (pal ? 290 : 240);
	// cutHeight: RDRAMSize es una MASCARA (ultima direccion valida).
	const u32 zancada = ancho << 1;
	if (dir + zancada > RDRAMSize + 1)
		return;
	if (dir + zancada * alto > RDRAMSize + 1)
		alto = (RDRAMSize + 1 - dir) / zancada;
	if (alto == 0)
		return;

	const u32 texW = (ancho + 3) & ~3u;
	const u32 texH = (alto + 3) & ~3u;
	const BOOL fijo = (texW * texH <= FB_CARGA_TEXELS) ? TRUE : FALSE;
	u16 *tex;
	if (fijo)
		tex = s_fbCargaMem[s_fbCargaTurno % FB_CARGA_BUFERES];
	else
	{
		tex = (u16*) __lwp_heap_allocate(GXtexCache, texW*texH*2+32);
		while (!tex && TextureCache_FreeOneTexture())
			tex = (u16*) __lwp_heap_allocate(GXtexCache, texW*texH*2+32);
		if (!tex)
			return;
	}

	// RGBA5551 del N64 -> RGB5A3 en tiles de 4x4.  Fuera del buffer (relleno de
	// los tiles) va 0.
	/* ★ DIVERGENCIA vs GLideN64 [Mario Tennis, triangulo negro en las sombras de
	 * las bob-ombs]: CON EL BIT DE ALFA DEL PIXEL, no con alfa 1 siempre
	 * (`_fullAlpha`).  Medido: sin esta carga no sale el triangulo (A/B, log 20);
	 * sin hacer crecer la textura (log 24) o escribiendo los rellenos en la RDRAM
	 * como GLideN64 (log 25: `fillram` 90/s, la RAM de la sombra ya trae el
	 * borrado en su mitad de arriba; retirado despues por no arreglar nada y
	 * costar en Killer Instinct y Paper Mario) SIGUE.  Lo unico que la carga mete y antes
	 * no existia es NEGRO OPACO donde la RAM vale 0.  Antes de 8b359761 un
	 * buffer nuevo empezaba en (0,0,0,0) y las sombras estaban bien.  Con el bit
	 * de alfa: 0x0000 queda transparente, como entonces, y lo que la CPU pinto
	 * con alfa (el marco del medidor) sale opaco.  RGB5A3 con el bit 15 a 0 y
	 * todo a cero = alfa 0. */
	u32 summ = 0;
	{
		u16 *dst = tex;
		u32 ty, tx, r, c;
		for (ty = 0; ty < texH; ty += 4)
			for (tx = 0; tx < texW; tx += 4)
				for (r = 0; r < 4; r++)
					for (c = 0; c < 4; c++)
					{
						const u32 y = ty + r, x = tx + c;
						u16 px = 0;
						if (y < alto && x < ancho)
						{
							const u16 n64 = *(const u16*) &RDRAM[dir + (y * ancho + x) * 2];
							summ += n64;
							px = (n64 & 1u) ? (u16)((n64 >> 1) | 0x8000u) : (u16)0;
						}
						*dst++ = px;
					}
	}

#if XBOX64_TRAZAS
	gln64_dbg_fbrdram++;
#endif

	if (summ == 0)
	{
		if (!fijo)
			__lwp_heap_free(GXtexCache, tex);
		return;
	}
#if XBOX64_TRAZAS
	gln64_dbg_fbrdram_px += ancho * alto;
#endif

	/* La textura de este buffer tendra este alto (tope VI.height) aunque el
	 * juego dibuje menos.  Ver s_fbCargadoDir. */
	s_fbCargadoDir   = dir;
	s_fbCargadoAncho = ancho;
	s_fbCargadoSize  = size;
	s_fbCargadoAlto  = (VI.height != 0 && alto > VI.height) ? VI.height : alto;

	// El borrado a negro de FrameBuffer_RestoreBuffer es DIFERIDO
	// (OGL_ClearColorBuffer solo levanta GXclearColorBuffer): se aplica ya, o
	// se comeria lo que se pinta aqui.
	OGL_ApplyPendingClears();

	GXTexObj texObj;
	GX_InvalidateTexAll();
	if (fijo)
	{
		GXD3D_TexturaCambiada( tex );   // texels nuevos en el mismo bufer
		s_fbCargaTurno++;
	}
	GX_InitTexObj(&texObj, tex, texW, texH, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);

	// Nada del dibujo de la escena: ni profundidad desde el color ni decal.
	GXD3D_SetRenderToDepth( 0 );
	GXD3D_SetDecalN64( 0 );
	{
		GXColor sinNiebla = { 0, 0, 0, 0 };
		GXD3D_SetBlenderNiebla( 0, sinNiebla );
	}

	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	GX_SetZMode(GX_DISABLE,GX_ALWAYS,GX_FALSE);
	GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z8,0);
	GX_SetCullMode (GX_CULL_NONE);
	GX_SetFog(GX_FOG_NONE,0.1,1.0,0.0,1.0,GXCOL_BLACK);

	// Coordenadas del EFB, como FrameBuffer_RestoreBuffer.
	Mtx44 GXprojection;
	guOrtho(GXprojection, 0, OGL.height, 0, OGL.width, 0.0f, 1.0f);
	GX_LoadProjectionMtx(GXprojection, GX_ORTHOGRAPHIC);
	GX_LoadPosMtxImm(OGL.GXmodelViewIdent,GX_PNMTX0);
	GX_SetViewport((f32) 0,(f32) 0,(f32) OGL.width,(f32) OGL.height, 0.0f, 1.0f);
	GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);

	const f32 x0 = (f32) OGL.GXorigX;
	const f32 y0 = (f32) OGL.GXorigY;
	const f32 x1 = x0 + (f32)ancho * OGL.GXscaleX;
	const f32 y1 = y0 + (f32)alto  * OGL.GXscaleY;
	const f32 u1 = (f32)ancho / (f32)texW;
	const f32 v1 = (f32)alto  / (f32)texH;

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetNumChans (0);
	GX_SetNumTexGens (1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetNumTevStages (1);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);

	// Repintado interno: sin el marcado de xbox64_marcatex (ver RestoreBuffer).
	const int marcaGuardada = gln64_marcaluz;
	gln64_marcaluz = 0;
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position2f32( x0, y0 );
		GX_TexCoord2f32( 0.0f, 0.0f );
		GX_Position2f32( x1, y0 );
		GX_TexCoord2f32( u1, 0.0f );
		GX_Position2f32( x1, y1 );
		GX_TexCoord2f32( u1, v1 );
		GX_Position2f32( x0, y1 );
		GX_TexCoord2f32( 0.0f, v1 );
	GX_End();
	gln64_marcaluz = marcaGuardada;

	if (!fijo)
		__lwp_heap_free(GXtexCache, tex);

	// El dibujo siguiente vuelve a poner su estado (como VI_GX_copiaDesdeRDRAM
	// y FrameBuffer_RestoreBuffer).
	OGL.GXupdateMtx = true;
	OGL.GXupdateFog = true;
	OGL_UpdateCullFace();
	OGL_UpdateViewport();
	gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT | CHANGED_GEOMETRYMODE;
	gDP.changed |= CHANGED_COMBINE | CHANGED_SCISSOR | CHANGED_RENDERMODE;
}
#endif // __GX__

// GLideN64's getBuffer(): exact start-address match.
FrameBuffer *FrameBuffer_GetBuffer( u32 startAddress )
{
	FrameBuffer *current = frameBuffer.top;

	while (current)
	{
		if (current->startAddress == startAddress)
			return current;
		current = current->lower;
	}

	return NULL;
}

FrameBuffer *FrameBuffer_FindBuffer( u32 address )
{
	FrameBuffer *current = frameBuffer.top;
	FrameBuffer *best = NULL;

	while (current)
	{
		if ((current->startAddress <= address) &&
			(current->endAddress >= address) &&
			(best == NULL || current->startAddress > best->startAddress))
			best = current;
		current = current->lower;
	}

	return best;
}

// GLideN64's FrameBuffer::getTextureBG(). A BG rect covers the buffer as a whole and
// takes its origin from gSP.bgImage, so none of the load tile shift and offset work
// that FrameBuffer_ActivateBufferTexture() does applies here.
void FrameBuffer_ActivateBufferTextureBG( s16 t, FrameBuffer *buffer )
{
	if (buffer == NULL || buffer->texture == NULL)
		return;

	buffer->texture->scaleS = OGL.GXscaleX / (float)buffer->texture->realWidth;
	buffer->texture->scaleT = OGL.GXscaleY / (float)buffer->texture->realHeight;

	buffer->texture->shiftScaleS = 1.0f;
	buffer->texture->shiftScaleT = 1.0f;

	buffer->texture->offsetS = gSP.bgImage.imageX;
	buffer->texture->offsetT = gSP.bgImage.imageY;

	FrameBuffer_MoveToTop( buffer );
	TextureCache_ActivateTexture( t, buffer->texture );
}

void FrameBuffer_ActivateBufferTexture( s16 t, FrameBuffer *buffer )
{
	/* [Xbox360] sonda temporal: ¿esta REALMENTE resuelta esta textura de
	 * framebuffer?  Si no lo esta, GX_LoadTexObj se ira por el camino normal
	 * y destilara un buffer que nadie escribio -> negro o basura.  Ver
	 * gxd3d_dbg_fbmiss en xenon/gx_d3d9.cpp. */
#if XBOX64_TRAZAS
	gxd3d_fb_activa( buffer->texture->GXtexture );
#endif

#ifndef __GX__
    buffer->texture->scaleS = OGL.scaleX / (float)buffer->texture->realWidth;
    buffer->texture->scaleT = OGL.scaleY / (float)buffer->texture->realHeight;
#else //!__GX__
    /* [Xbox360] LA ESCALA DEL BUFFER, NO LA DE AHORA.
     *
     * Upstream pone aqui `OGL.GXscaleX` porque en glN64 la escala es una sola
     * para toda la sesion: la del VI.  Aqui ya no lo es -- gln64_cimg_escala()
     * la baja cuando un color image no cabe en el EFB (ver gDPSetColorImage)
     * --, asi que usar la vigente mezcla dos buffers distintos: la textura se
     * CAPTURO con la escala de su buffer y se MUESTREARIA con la del que este
     * activo al dibujarla, que normalmente es el principal.
     *
     * Medido en Pokemon Stadium: el panel de 538x100 se captura a 1.190 y se
     * activaria a 2.000, o sea 1,68 veces mas grande de lo que toca.
     *
     * `current->scaleX/scaleY` ya se guardan en FrameBuffer_SaveBuffer justo
     * para esto (la comprobacion de invalidacion de :510 los usa), asi que no
     * hay que anadir estado: solo leer el que corresponde.
     *
     * Revisado contra Wii64 1b5041a: la reforma del framebuffer de esa
     * version adopta las reglas de GLideN64 (fbFingerprint, _fbIsAuxiliary,
     * FrameBuffer_CopyRdram...) pero NO toca la escala -- `OGL_UpdateScale`
     * sigue siendo `GXwidth / VI.width` y nada mas --, asi que la divergencia
     * sigue haciendo falta tal cual. */
    buffer->texture->scaleS = buffer->scaleX / (float)buffer->texture->realWidth;
    buffer->texture->scaleT = buffer->scaleY / (float)buffer->texture->realHeight;
#endif //__GX__

	if (gSP.textureTile[t]->shifts > 10)
		buffer->texture->shiftScaleS = (float)(1 << (16 - gSP.textureTile[t]->shifts));
	else if (gSP.textureTile[t]->shifts > 0)
		buffer->texture->shiftScaleS = 1.0f / (float)(1 << gSP.textureTile[t]->shifts);
	else
		buffer->texture->shiftScaleS = 1.0f;

	if (gSP.textureTile[t]->shiftt > 10)
		buffer->texture->shiftScaleT = (float)(1 << (16 - gSP.textureTile[t]->shiftt));
	else if (gSP.textureTile[t]->shiftt > 0)
		buffer->texture->shiftScaleT = 1.0f / (float)(1 << gSP.textureTile[t]->shiftt);
	else
		buffer->texture->shiftScaleT = 1.0f;

	/* ★ DIVERGENCIA vs Wii64: el desplazamiento en LOS DOS EJES, portado de
	 * GLideN64 (`FrameBuffer::getTexture`, src/FrameBuffer.cpp:485-493):
	 *
	 *     shift   = (imageAddress - startAddress) >> (size - 1);   // texels
	 *     offsetS = uls + (shift % width);
	 *     offsetT = ult + (shift / width);
	 *
	 * Upstream solo calcula el termino de T y deja `offsetS` en `uls` (o 0),
	 * que solo vale si la carga empieza al principio de una fila -- y por eso
	 * lleva el guarda de alineacion de `_gDPCheckForFrameBufferTexture`.  Los
	 * dos van juntos; al estrechar aquel, este hace falta.
	 *
	 * ★ HONESTIDAD SOBRE EL MERITO: en todo lo medido hasta hoy esta formula
	 * devuelve EXACTAMENTE lo mismo que upstream, porque `shift % width` sale
	 * 0 en las 231 activaciones del log 40.  Lo que arreglo los paneles de
	 * Pokemon Stadium fue estrechar el guarda, no esto.  Se queda por
	 * fidelidad a la referencia y porque cubre el caso que upstream no sabe
	 * expresar, no porque haya arreglado nada todavia.
	 *
	 * [Xbox360] ★★ [Mario Tennis] Y CON LO QUE SE GUARDO AL CARGAR, como la
	 * referencia: la direccion de imagen DEL TILE (`imageAddress`) y el origen
	 * y tipo de carga DEL BUFFER (`m_loadTileOrigin`, `m_loadType`).  Aqui se
	 * usaban `gDP.textureImage.address`, `gDP.loadType` y `gDP.loadTile`, que
	 * son los de la ULTIMA carga: si el juego carga el tile 1 de un buffer y
	 * luego apunta SetTImg a otra cosa, el desplazamiento salia negativo
	 * (medido con [FBACT], log 7: `+4294954608`) y se muestreaba fuera. */
	{
		const u32 bpp1   = buffer->size ? (buffer->size - 1) : 0;
		const u32 factor = buffer->width ? buffer->width : 1;
		const u32 shift  = (gSP.textureTile[t]->imageAddress - buffer->startAddress) >> bpp1;
		const f32 offS   = (f32)(shift % factor);
		const f32 offT   = (f32)(shift / factor);

		if (buffer->loadType == LOADTYPE_TILE)
		{
			buffer->texture->offsetS = (f32)buffer->loadUls + offS;
			buffer->texture->offsetT = (f32)buffer->loadUlt + offT;
		}
		else
		{
			buffer->texture->offsetS = offS;
			buffer->texture->offsetT = offT;
		}
	}

	// GLideN64 hack_fbTextureOffset (their #519 and #2112)
	if ((config.generalEmulation.hacks & hack_fbTextureOffset) != 0 &&
	    gDP.otherMode.textureFilter != G_TF_POINT)
	{
		buffer->texture->offsetS -= 1.0f;
		buffer->texture->offsetT -= 1.0f;
	}

	FrameBuffer_MoveToTop( buffer );
	TextureCache_ActivateTexture( t, buffer->texture );
}

#ifdef __GX__

void FrameBuffer_CopyToRDRAM( u32 sourceAddress, u32 address, u32 width, u32 height )
{
	if (width == 0 || height == 0)
		return;

	if (address >= RDRAMSize || (address + width * height * 2) > RDRAMSize)
		return;

	FrameBuffer *source = FrameBuffer_GetBuffer( sourceAddress );

	if (source == NULL || source->texture == NULL ||
	    source->texture->GXtexture == NULL ||
	    source->texture->GXtexfmt != GX_TF_RGBA8)
		return;

	const u32 texW = source->texture->width;
	const u32 texH = source->texture->height;
	const u32 pitch = source->texture->realWidth;

	if (texW == 0 || texH == 0 || pitch < 4)
		return;

	const u8 *tex = (const u8*)source->texture->GXtexture;
	DCInvalidateRange( source->texture->GXtexture, source->texture->textureBytes );

	for (u32 oy = 0; oy < height; oy++)
	{
		u32 sy = (oy * texH) / height;
		if (sy >= texH)
			sy = texH - 1;

		const u8 *tileRow = tex + (sy >> 2) * (pitch >> 2) * 64;
		const u32 i = ((sy & 3) << 2) << 1;

		u16 *dst = (u16*)&RDRAM[address + oy * width * 2];

		for (u32 ox = 0; ox < width; ox++)
		{
			u32 sx = (ox * texW) / width;
			if (sx >= texW)
				sx = texW - 1;

			const u8 *tile = tileRow + (sx >> 2) * 64;
			const u32 t = i + ((sx & 3) << 1);

			const u8 r = tile[t + 1];
			const u8 g = tile[32 + t];
			const u8 b = tile[32 + t + 1];

			dst[ox] = (u16)(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 1);
		}
	}

	DCFlushRange( &RDRAM[address], width * height * 2 );

	FrameBuffer_RestampMarkers( address, address + width * height * 2 - 1 );

	FrameBuffer *written = FrameBuffer_GetBuffer( address );
	if (written != NULL && written->isDepthBuffer)
		written->changed = FALSE;
}

void FrameBuffer_IncrementVIcount()
{
	FrameBuffer *buffer = frameBuffer.top;
	while(buffer)
	{
		buffer->texture->VIcount++;
		buffer = buffer->lower;
	}
}
#endif //__GX__
