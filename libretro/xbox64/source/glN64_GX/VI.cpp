/**
 * glN64_GX - VI.cpp
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifdef __GX__
#include <stdio.h>
#include <string.h>	// [Xbox360] memset de VI_GX_copiaDesdeRDRAM
#include <gccore.h>
#include <malloc.h>
#include <ogc/lwp_heap.h>
#include "../libgui/IPLFont.h"
#include "../menu/MenuResources.h"
#include "../gui/DEBUG.h"
#include "../main/timers.h"
#include "../main/wii64config.h"
//#include "Textures.h"
#endif // __GX__

#include <math.h>
#include "glN64.h"
#include "Types.h"
#include "VI.h"
#include "OpenGL.h"
#include "N64.h"
#include "gSP.h"
#include "gDP.h"
#include "RSP.h"
#include "FrameBuffer.h"
#include "Debug.h"
#include "F3D.h"
#include "Config.h"	// [Xbox360] hack_copyFromRDRAM (Dr. Mario 64)
#ifdef __GX__
#include "Textures.h"

static void VI_GX_copiaDesdeRDRAM( u32 inicio, BOOL vaciar );
static u32  VI_GX_inicioCFB();
#endif // __GX__

VIInfo VI;

extern GXRModeObj *vmode, *rmode;
extern int GX_xfb_offset;

#ifdef __GX__
char printToScreen;
char showFPSonScreen;
char renderCpuFramebuffer;

/*bool updateDEBUGflag;
bool new_fb;
unsigned int* xfb[2];
int which_fb;*/
#endif // __GX__

static u32 widthPrev = 0;

void VI_UpdateSize()
{
	f32 xScale = _FIXED2FLOAT( _SHIFTR( *REG.VI_X_SCALE, 0, 12 ), 10 );
	//f32 xOffset = _FIXED2FLOAT( _SHIFTR( *REG.VI_X_SCALE, 16, 12 ), 10 );

	f32 yScale = _FIXED2FLOAT( _SHIFTR( *REG.VI_Y_SCALE, 0, 12 ), 10 );
	//f32 yOffset = _FIXED2FLOAT( _SHIFTR( *REG.VI_Y_SCALE, 16, 12 ), 10 );

	u32 hEnd = _SHIFTR( *REG.VI_H_START, 0, 10 );
	u32 hStart = _SHIFTR( *REG.VI_H_START, 16, 10 );

	u32 vEnd = _SHIFTR( *REG.VI_V_START, 0, 10 );
	u32 vStart = _SHIFTR( *REG.VI_V_START, 16, 10 );

	u32 width = _SHIFTR( *REG.VI_WIDTH, 0, 12 );
	VI.width = hEnd == hStart ? width : lrint(abs((long)(hEnd - hStart)) * xScale / 2) * 2;

	/* [Xbox360] ★ [Lode Runner 3-D] ESTA MULTIPLICACION, SOLO ENTRELAZADO, Y
	 * EL ALTO CON TOPE, como la referencia (GLideN64 de
	 * mupen64plus-libretro-nx, VI.cpp:61-85).
	 *
	 * Wii64 multiplicaba SIEMPRE la escala vertical por zancada/ancho visible.
	 * Lode Runner pinta en un framebuffer de 640 de zancada con dos imagenes de
	 * 320 una al lado de otra (ci=0036A000 y 0036A280: 0x280 bytes = 320
	 * pixeles) y no es eso: sale `VI 320x960`, el plugin escala la Y por la
	 * mitad y el juego entero queda en la esquina superior izquierda a 320x240.
	 * GLideN64 solo hace esa cuenta con el bit SERRATE (0x40) de VI_STATUS, y
	 * ademas nunca deja pasar de 480 (576 en PAL, VI_V_SYNC > 550).  Las dos
	 * cosas, igual que alli; con cualquiera de ellas aqui ya sale 480. */
	/* [Xbox360] ★ [NBA JAM 2000, imagen estirada a lo alto y recortada]
	 * REDONDEADO, como la referencia (`floorf(VI_WIDTH / fullWidth + 0.5f)`),
	 * y solo si es > 1.  Era `yScale *= width / VI.width` con los dos u32:
	 * NBA JAM entrelazado tiene zancada 576 y ancho visible 578, 576/578 = 0,
	 * la escala vertical quedaba a CERO y el `VI.height == 0` de abajo la
	 * ponia a 240 (log 33: `VI=578x240`, viewport pedido 637x824). */
	if ((*REG.VI_STATUS & 0x40) != 0 && VI.width != 0)
	{
		const u32 escZancada = (2 * width + VI.width) / (2 * VI.width);
		if (escZancada > 1)
			yScale *= escZancada;
	}
	vEnd   += vStart % 2;
	vStart += vEnd   % 2;
	vStart  = vStart / 2 - 1;
	vEnd    = vEnd   / 2 + 1;

	VI.height = lrint(abs((long)(vEnd - vStart)) * yScale / 2) * 2;
	{
		const u32 tope = ((*REG.VI_V_SYNC & 0x3ff) > 550) ? 576 : 480;
		if (VI.height > tope) VI.height = tope;
	}

	if (VI.width == 0) VI.width = 320;
	if (VI.height == 0) VI.height = 240;

	if (VI.width != widthPrev)
	{
		if (widthPrev != 0)
		{
			FrameBuffer_RemoveBuffersOfWidth( widthPrev );
			FrameBuffer_RemoveBuffersOfWidth( VI.width );
		}

		widthPrev = VI.width;
	}
}


static char using_240p = 0;
static char oldNativeOutputSetting = NATIVEOUT_DISABLE;
extern "C" void switchTo240p(bool is_pal);
extern "C" void switchToNormalVideo();
extern "C" int getEfbHeight();
extern void gfx_set_window(int x, int y, int width, int height);
extern void OGL_UpdateScale();

void GX_CheckResChange() {
	// Detect resolution change (if we'd gone back to the menu)
	int efbHeight = getEfbHeight(), dims_changed = 0;
	if((using_240p && efbHeight > 264) || (!using_240p && efbHeight <= 264)) {
		using_240p = efbHeight <= 264;
		dims_changed = 1;
	}
	// 
	// Detect internal res change or setting change.
	if (dims_changed || (oldNativeOutputSetting != nativeOutput)) {
		int new_h = _SHIFTR( *REG.VI_V_SYNC, 0, 10 );

		// Decide if we should be in 240p
		bool want_240p = (nativeOutput == NATIVEOUT_ENABLE) && (new_h % 2);

		if (want_240p && !using_240p) {
			switchTo240p(new_h > 525);
			using_240p = true;
			gfx_set_window(0, 0, 640, 240);
			OGL_UpdateScale();
		}
		else if (!want_240p && using_240p || (oldNativeOutputSetting == NATIVEOUT_ENABLE)) {
			switchToNormalVideo();
			using_240p = false;
			if (screenMode == SCREENMODE_16x9_PILLARBOX)
				gfx_set_window( 80, 0, 480, 480);
			else
				gfx_set_window( 0, 0, 640, 480);
			OGL_UpdateScale();
		}
		oldNativeOutputSetting = nativeOutput;
	}
}

void VI_UpdateScreen()
{
	GLN64_PROF_SCOPE(viupd);   /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	if (GBI.current && GBI.current->type == F3DGOLDEN) {
		F3DGOLDEN_NewFrame();
	}

#ifdef __GX__
	/* [Xbox360] [Super Smash Bros., seleccion aleatoria] La entrada NOISE del
	 * combinador: semilla nueva por frame, `& 0xFF` como GLideN64 (UNoiseSeed,
	 * glsl_CombinerProgramUniformFactoryCommon.cpp:24-28, con el contador de
	 * swaps), y la escala EFB / N64 (su uScreenScale).  Ver GXD3D_SetRuido. */
	{
		static u32 s_semillaRuido = 0;
		s_semillaRuido = (s_semillaRuido + 1) & 0xFF;
		GXD3D_SetRuido( (f32)s_semillaRuido, OGL.GXscaleX, OGL.GXscaleY );
	}
#endif // __GX__

#ifndef __GX__
	glFinish();

	if (OGL.frameBufferTextures)
	{
		FrameBuffer *current = FrameBuffer_FindBuffer( *REG.VI_ORIGIN );

		if ((*REG.VI_ORIGIN != VI.lastOrigin) || ((current) && current->changed))
		{
			if (gDP.colorImage.changed)
			{
				FrameBuffer_SaveBuffer( gDP.colorImage.address, gDP.colorImage.size, gDP.colorImage.width, gDP.colorImage.height );
				gDP.colorImage.changed = FALSE;
			}

			FrameBuffer_RenderBuffer( *REG.VI_ORIGIN );

			gDP.colorImage.changed = FALSE;
			VI.lastOrigin = *REG.VI_ORIGIN;
#ifdef DEBUG
			while (Debug.paused && !Debug.step);
			Debug.step = FALSE;
#endif
		}
	}
	else
	{
		if (gSP.changed & CHANGED_COLORBUFFER)
		{
#ifndef __LINUX__
			SwapBuffers( OGL.hDC );
#else
			OGL_SwapBuffers();
#endif
			gSP.changed &= ~CHANGED_COLORBUFFER;
#ifdef DEBUG
			while (Debug.paused && !Debug.step);
			Debug.step = FALSE;
#endif
		}
	}
	glFinish();
#else // !__GX__
	if (renderCpuFramebuffer || (RSP.DList == 0))
	{
		//Only render N64 framebuffer in RDRAM and not EFB
		VI_GX_cleanUp();
		VI_GX_renderCpuFramebuffer();
#ifdef HW_DOL
		VI_GX_showLoadIcon();
#endif
		VI_GX_showFPS();
		VI_GX_showDEBUG();
		GX_CopyDisp(VI.xfb[VI.which_fb], GX_TRUE);
		GX_SetDrawSync(VI.which_fb);
		RSP_ReiniciaOtherMode();	// [Xbox360] una vez por frame, en el swap
	}
	else if (OGL.frameBufferTextures)
	{
		FrameBuffer *current = FrameBuffer_FindBuffer( *REG.VI_ORIGIN );

		/* [Xbox360] ★ [PGA European Tour, la barra de fuerza del golpe no se
		 * ve] EL RDP HA PINTADO EN EL BUFFER QUE ESTA EN PANTALLA.  Durante el
		 * golpe el juego no cambia de buffer: manda una lista por VI sobre el
		 * que se ve (vi_origin=3A0280 fijo, 60 listas/s y 0 imagenes nuevas en
		 * logs (5)) y el VI del N64, que lee la RDRAM en cada barrido, ensena la
		 * barra.  glN64 solo presentaba al cambiar el origen o si SaveBuffer
		 * habia marcado el buffer (al cambiar de color image), y aqui no pasa
		 * ninguna de las dos: se repetia la imagen vieja.  GLideN64 presenta en
		 * CADA VI (bufferSwapMode = bsOnVerticalInterrupt, su defecto,
		 * Config.cpp; gonetz/GLideN64#325 es este medidor).  Aqui solo cuando
		 * hay algo nuevo en el buffer que se ve: un juego con doble buffer pinta
		 * en el otro y no cambia nada. */
		const BOOL pintaEnPantalla = (current != NULL && gDP.colorImage.changed &&
		                              gDP.colorImage.address == current->startAddress) ? TRUE : FALSE;

		if ((*REG.VI_ORIGIN != VI.lastOrigin) || ((current) && current->changed) || pintaEnPantalla)
		{
			/* [Xbox360] [Dr. Mario 64] Lo que necesita VI_GX_copiaDesdeRDRAM,
			 * tomado AQUI: RenderBuffer pone `changed` a FALSE, y SaveBuffer
			 * puede quitar buffers que se solapan (no se guarda el puntero). */
			const BOOL hayCFB    = (current != NULL) ? TRUE : FALSE;
			const u32  inicioCFB = hayCFB ? current->startAddress : 0;
			const BOOL rdpPinto  = (hayCFB && current->changed) ? TRUE : FALSE;

			FrameBuffer_IncrementVIcount();
			if (gDP.colorImage.changed)
			{
				FrameBuffer_SaveBuffer( gDP.colorImage.address, gDP.colorImage.size, gDP.colorImage.width, gDP.colorImage.height );
				gDP.colorImage.changed = FALSE;
			}

			FrameBuffer_RenderBuffer( *REG.VI_ORIGIN );

			//Draw DEBUG to screen
			VI_GX_cleanUp();
			if ((config.generalEmulation.hacks & hack_copyFromRDRAM) != 0 && hayCFB)
				VI_GX_copiaDesdeRDRAM( inicioCFB, rdpPinto );
#ifdef HW_DOL
			VI_GX_showLoadIcon();
#endif
			VI_GX_showFPS();
			VI_GX_showDEBUG();
			//Copy EFB->XFB
			/* [Xbox360] [Quake II, objetos a traves de las paredes] Sin borrar
			 * el Z: el VI puede caer entre dos listas del MISMO frame (escenario
			 * en una, objetos en la otra); el color lo repone RestoreBuffer de
			 * abajo, pero el Z se perdia.  Ni glN64 de PC ni GLideN64 tocan el Z
			 * en el swap: lo borra el juego.  Ver GXD3D_CopyDispConservaZ. */
			GXD3D_CopyDispConservaZ(VI.xfb[VI.which_fb]);
			GX_SetDrawSync(VI.which_fb);
			RSP_ReiniciaOtherMode();	// [Xbox360] una vez por frame, en el swap

			//Restore current EFB
			FrameBuffer_RestoreBuffer( gDP.colorImage.address, gDP.colorImage.size, gDP.colorImage.width );

			gDP.colorImage.changed = FALSE;
			VI.lastOrigin = *REG.VI_ORIGIN;
		}
	}
	else
	{
		if (gSP.changed & CHANGED_COLORBUFFER)
		{
			/* [Xbox360] [Dr. Mario 64] Sin emulacion de framebuffer el EFB es la
			 * imagen: se compone encima.  CHANGED_COLORBUFFER = hubo lista desde
			 * la ultima presentacion, o sea que el RDP acaba de pintar. */
			const BOOL componeRDRAM = ((config.generalEmulation.hacks & hack_copyFromRDRAM) != 0) ? TRUE : FALSE;
			VI_GX_cleanUp();
			if (componeRDRAM)
				VI_GX_copiaDesdeRDRAM( VI_GX_inicioCFB(), TRUE );
#ifdef HW_DOL
			VI_GX_showLoadIcon();
#endif
			VI_GX_showFPS();
			VI_GX_showDEBUG();
			/* [Xbox360] [Quake II, frame negro con solo los objetos] Sin borrar
			 * NADA: aqui el EFB es el unico sitio donde esta el frame en curso,
			 * y el VI puede caer entre dos listas del mismo frame (escenario en
			 * una, objetos en la otra).  Como GLideN64 sin emulacion de
			 * framebuffer (_renderScreenSizeBuffer: un buffer que el swap no
			 * borra); el color y el Z los borra el juego.  Con la composicion de
			 * la RDRAM encima se borra como siempre: si no, se quedaria en el
			 * frame siguiente. */
			if (componeRDRAM)
				GX_CopyDisp(VI.xfb[VI.which_fb], GX_TRUE);
			else
				GX_CopyDisp(VI.xfb[VI.which_fb], GX_FALSE);
			GX_SetDrawSync(VI.which_fb);
			RSP_ReiniciaOtherMode();	// [Xbox360] una vez por frame, en el swap
			gSP.changed &= ~CHANGED_COLORBUFFER;
		}
	}
#endif // __GX__

}

#ifdef __GX__
extern "C" {
extern int enableLoadIcon;
extern long long gettime();
extern unsigned int diff_sec(long long start,long long end);
};

void VI_GX_init() {
	//init_font();
/*	updateDEBUGflag = true;
	new_fb = false;
	which_fb = 1;*/
	VI.updateOSD = true;
	VI.EFBcleared = true;
	VI.copy_fb = false;
	VI.which_fb = 1;
}

void VI_GX_setFB(unsigned int* fb1, unsigned int* fb2){
	VI.xfb[0] = fb1;
	VI.xfb[1] = fb2;
}

unsigned int* VI_GX_getScreenPointer(){ return VI.xfb[VI.which_fb]; }

void VI_GX_clearEFB(){
	GX_SetZMode(GX_ENABLE,GX_ALWAYS,GX_TRUE);
	GX_SetCopyClear (GXCOL_BLACK, 0xFFFFFF);   /* [Xbox360] literal compuesto */
	GX_CopyDisp (VI.xfb[VI.which_fb]+GX_xfb_offset, GX_TRUE);	//clear the EFB before executing new Dlist
	GX_DrawDone(); //Wait until EFB->XFB copy is complete
}

/* [Xbox360] `extern "C"` en las dos: las define `main/timers.c`, que es C.
 * Sin esto se buscan los simbolos decorados y el enlace las toma por
 * variables distintas de las que el nucleo actualiza. */
extern "C" timers Timers;
extern "C" float VILimit;

void VI_GX_showFPS(){
	if(showFPSonScreen) {
		char caption[50];

		sprintf(caption, "%.1f VI/s (%.1fx), %.1f DL/s",Timers.vis,Timers.vis/VILimit,Timers.fps);
		
		GXColor fontColor = {150,255,150,255};
		menu::IplFont::getInstance().drawInit(fontColor);
	
		menu::IplFont::getInstance().drawString(15,35,caption, 1.0, false);

		//reset swap table from GUI/DEBUG
		GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
	}
}

#ifdef HW_DOL
void VI_GX_showLoadIcon()
{
	if (enableLoadIcon == 0) return;
	enableLoadIcon = 0;
	float x = 530;
	float y = 30;
	float width = 80;
	float height = 56;

	Mtx44 GXprojection2D;
	Mtx GXmodelView2D;

	GXTexObj texObj;
//	GX_InitTexObj(&texObj, LoadingTexture, width, height, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObj(&texObj, LoadingTexture, width, height, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);

	guMtxIdentity(GXmodelView2D);
	GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX2);
	guOrtho(GXprojection2D, 0, 480, 0, 640, 0, 1);
	GX_LoadProjectionMtx(GXprojection2D, GX_ORTHOGRAPHIC); //load current 2D projection matrix
//	GX_SetViewport((f32) 0,(f32) 0,(f32) 640,(f32) 480, 0.0f, 1.0f);
	GX_SetViewport((f32) OGL.GXorigX,(f32) OGL.GXorigY,(f32) OGL.GXwidth,(f32) OGL.GXheight, 0.0f, 1.0f);
	GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);	//Set to the same size as the viewport.

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX2);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	//set vertex attribute formats here
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	//disable textures
	GX_SetNumChans (0);
	GX_SetNumTexGens (1);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
	//set blend mode
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR); //Fix src alpha
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	GX_SetDstAlpha(GX_DISABLE, 0xFF);
	GX_SetZMode(GX_DISABLE,GX_ALWAYS,GX_FALSE);
	GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z8,0);
	GX_SetZCompLoc(GX_TRUE);	// Do Z-compare before texturing.
	//set cull mode
	GX_SetCullMode (GX_CULL_NONE);

	GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
		GX_Position2f32(x, y);
		GX_TexCoord2f32(0,0);
		GX_Position2f32(x+width, y);
		GX_TexCoord2f32(1,0);
		GX_Position2f32(x+width, y+height);
		GX_TexCoord2f32(1,1);
		GX_Position2f32(x, y+height);
		GX_TexCoord2f32(0,1);
	GX_End();
}
#endif

void VI_GX_updateDEBUG()
{
	VI.updateOSD = true;
}

extern char text[DEBUG_TEXT_HEIGHT][DEBUG_TEXT_WIDTH];

void VI_GX_showDEBUG()
{
#ifdef SHOW_DEBUG
	int i = 0;
	GXColor fontColor = {150, 255, 150, 255};
//	VI_GX_showStats();
	DEBUG_update();
	menu::IplFont::getInstance().drawInit(fontColor);
	if(printToScreen)
		for (i=0;i<DEBUG_TEXT_HEIGHT;i++)
			menu::IplFont::getInstance().drawString(10,(10*i+60),text[i], 0.5, false); 
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
#endif
}

#ifdef SHOW_DEBUG
	extern int CntTriProj, CntTriProjW, CntTriOther, CntTriNear, CntTriPolyOffset;
#endif

void VI_GX_showStats()
{
#ifdef SHOW_DEBUG
	sprintf(txtbuffer,"texCache: %d bytes in %d cached textures; %d FB textures; %d max textures",cache.cachedBytes,cache.numCached,frameBuffer.numBuffers, GX_MAX_TEXTURES);
	DEBUG_print(txtbuffer,DBG_CACHEINFO); 

	sprintf(txtbuffer,"TriMatr: %d Proj; %d ProjW; %d Other; %d ProjWnear; %d PolyOff",CntTriProj,CntTriProjW,CntTriOther,CntTriNear,CntTriPolyOffset);
	DEBUG_print(txtbuffer,DBG_CACHEINFO+1); 
	CntTriProj = 0;
	CntTriProjW = 0;
	CntTriOther = 0;
	CntTriNear = 0;
	CntTriPolyOffset = 0;

#endif
}

void VI_GX_cleanUp()
{
	GX_CheckResChange();
	
	GX_SetNumTevStages(1);
	GX_SetTevOp(GX_TEVSTAGE0,GX_MODULATE);

	GX_SetFog(GX_FOG_NONE,0,1,0,1,GXCOL_BLACK);   /* [Xbox360] literal compuesto */
	GX_SetViewport((f32) OGL.GXorigX,(f32) OGL.GXorigY,(f32) OGL.GXwidth,(f32) OGL.GXheight, 0.0f, 1.0f);
	GX_SetCoPlanar(GX_DISABLE);
	GX_SetClipMode(GX_CLIP_ENABLE);
	GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	GX_SetZCompLoc(GX_TRUE);	// Do Z-compare before texturing.
}

extern heap_cntrl* GXtexCache;

void VI_GX_renderCpuFramebuffer()
{
	//Only render N64 framebuffer in RDRAM and not EFB drawn by glN64
	if (!(*REG.VI_STATUS & 3)) //bpp != 16 or 32
	{
#ifdef SHOW_DEBUG
		sprintf(txtbuffer,"VI (CpuFramebuffer): No bits per pixel specified");
		DEBUG_print(txtbuffer,DBG_VIINFO); 
#endif
		return; 
	}
	if (!*REG.VI_H_START)
	{
#ifdef SHOW_DEBUG
		sprintf(txtbuffer,"VI (CpuFramebuffer): VI_H_START_REG is NULL");
		DEBUG_print(txtbuffer,DBG_VIINFO); 
#endif
		return; 
	}
	VI_UpdateSize();
	u32 FBtexW = (VI.width + 3) & ~3;
	u32 FBtexH = (VI.height + 3) & ~3;

	//Init texture cache heap if not yet inited
	if(!GXtexCache)
	{
		GXtexCache = (heap_cntrl*)malloc(sizeof(heap_cntrl));
#ifdef HW_RVL
		__lwp_heap_init(GXtexCache, TEXCACHE_LO,GX_TEXTURE_CACHE_SIZE, 32);
#else //HW_RVL
		__lwp_heap_init(GXtexCache, memalign(32,GX_TEXTURE_CACHE_SIZE),GX_TEXTURE_CACHE_SIZE, 32);
#endif //!HW_RVL
#ifdef SHOW_DEBUG
		DEBUG_registerHeap(GXtexCache, "TEX");
#endif
	}
	u16* FBtex = (u16*) __lwp_heap_allocate(GXtexCache,FBtexW*FBtexH*2+32);
	while (!FBtex && TextureCache_FreeOneTexture())
		FBtex = (u16*) __lwp_heap_allocate(GXtexCache,FBtexW*FBtexH*2+32);

	if (!FBtex)
	{
#ifdef SHOW_DEBUG
		sprintf(txtbuffer,"VI (CpuFramebuffer): out of texture cache");
		DEBUG_print(txtbuffer,DBG_VIINFO);
#endif
		return;
	}
	GXTexObj	FBtexObj;

	//N64 Framebuffer is in RGB5A1 format, so shift by 1 and retile.
	GX_RedirectWriteGatherPipe(FBtex);

	u32 address = RSP_SegmentToPhysical( *REG.VI_ORIGIN );
	u32 stride = *REG.VI_WIDTH * 2;

	if (*REG.VI_V_CURRENT_LINE & 1)
		address -= stride;

	u8 *src1 = &RDRAM[address - 8];
	u8 *src2 = src1 + stride;
	u8 *src3 = src2 + stride;
	u8 *src4 = src3 + stride;

	int rowpitch = stride * 4 - (FBtexW * 2);
	int rows = FBtexH >> 2;

	while (rows--) {
		int tiles = FBtexW >> 2;

		do {
			/* ★ DIVERGENCIA vs Wii64.  Ver xenon/gxshim/DIVERGENCIAS.txt punto 5.
			 *
			 * El original es inline asm de GCC escribiendo por el WRITE GATHER
			 * PIPE de la Wii, que el XDK no acepta (ni el asm de GCC ni el WGP,
			 * que no existe en Xenon).  Traducido a C conservando la semantica
			 * instruccion a instruccion:
			 *
			 *   lwzu r,8(s)  -> s += 8; r = *(u32*)s;   (pre-incremento)
			 *   lwz  r,4(s)  -> r = *(u32*)(s+4);
			 *   rotrwi r,r,1 -> (r >> 1) | (r << 31)
			 *   or r,r,0x80008000 -> pone el bit 15 de cada halfword
			 *   stw r,0(wgPipe) x8 -> 32 bytes = un tile de 4x4 a 16 bpp
			 *
			 * Lo que hace de verdad: el framebuffer del N64 es RGB5A1
			 * (rrrrrgggggbbbbba) y GX quiere RGB5A3.  La rotacion de 1 bit
			 * mueve el alfa del bit 0 al bit 15, y el OR con 0x80008000 fuerza
			 * ese bit a 1, que en RGB5A3 significa "opaco, RGB555".  O sea:
			 * convierte y retila en la misma pasada.
			 *
			 * `wgPipe` es el mismo objeto que usa upstream: en el shim sus
			 * miembros avanzan un cursor en vez de empujar a un FIFO, asi que
			 * los ocho `stw` de abajo se escriben igual que en el original. */
			u32 w0, w1, w2, w3, w4, w5, w6, w7;

			src1 += 8; w0 = *(u32*)src1; w1 = *(u32*)(src1 + 4);
			src2 += 8; w2 = *(u32*)src2; w3 = *(u32*)(src2 + 4);
			src3 += 8; w4 = *(u32*)src3; w5 = *(u32*)(src3 + 4);
			src4 += 8; w6 = *(u32*)src4; w7 = *(u32*)(src4 + 4);

			w0 = ((w0 >> 1) | (w0 << 31)) | 0x80008000u;
			w1 = ((w1 >> 1) | (w1 << 31)) | 0x80008000u;
			w2 = ((w2 >> 1) | (w2 << 31)) | 0x80008000u;
			w3 = ((w3 >> 1) | (w3 << 31)) | 0x80008000u;
			w4 = ((w4 >> 1) | (w4 << 31)) | 0x80008000u;
			w5 = ((w5 >> 1) | (w5 << 31)) | 0x80008000u;
			w6 = ((w6 >> 1) | (w6 << 31)) | 0x80008000u;
			w7 = ((w7 >> 1) | (w7 << 31)) | 0x80008000u;

			wgPipe->U32 = w0; wgPipe->U32 = w1;
			wgPipe->U32 = w2; wgPipe->U32 = w3;
			wgPipe->U32 = w4; wgPipe->U32 = w5;
			wgPipe->U32 = w6; wgPipe->U32 = w7;
		} while (--tiles);

		src1 += rowpitch;
		src2 += rowpitch;
		src3 += rowpitch;
		src4 += rowpitch;
	}

	GX_RestoreWriteGatherPipe();

	//Initialize texture
	GX_InvalidateTexAll();
	GX_InitTexObj(&FBtexObj, FBtex, FBtexW, FBtexH, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE); 
	GX_LoadTexObj(&FBtexObj, GX_TEXMAP0);

	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR); 
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	GX_SetZMode(GX_DISABLE,GX_ALWAYS,GX_FALSE);
	GX_SetCullMode (GX_CULL_NONE);
	GX_SetFog(GX_FOG_NONE,0.1,1.0,0.0,1.0,GXCOL_BLACK);   /* [Xbox360] literal compuesto */

	Mtx44 GXprojection;
	guOrtho(GXprojection, 0, 480, 0, 640, 0.0f, 1.0f);
	GX_LoadProjectionMtx(GXprojection, GX_ORTHOGRAPHIC); 
	Mtx	GXmodelViewIdent;
	guMtxIdentity(GXmodelViewIdent);
	GX_LoadPosMtxImm(GXmodelViewIdent,GX_PNMTX0);
	GX_SetViewport((f32) OGL.GXorigX,(f32) OGL.GXorigY,(f32) OGL.GXwidth,(f32) OGL.GXheight, 0.0f, 1.0f);
	GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);	//Set to the same size as the viewport.

	float u1, v1;

	u1 = (float)VI.width / (float)FBtexW;
	v1 = (float)VI.height / (float)FBtexH;

	//set vertex description
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	//set vertex attribute formats
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	//enable textures
	GX_SetNumChans (0);
	GX_SetNumTexGens (1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetNumTevStages (1);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position2f32( 0.0f, 0.0f );
		GX_TexCoord2f32( 0.0f, 0.0f );
		GX_Position2f32( 640.0f, 0.0f );
		GX_TexCoord2f32( u1, 0.0f );
		GX_Position2f32( 640.0f, 480.0f );
		GX_TexCoord2f32( u1, v1 );
		GX_Position2f32( 0.0f, 480.0f );
		GX_TexCoord2f32( 0.0f, v1 );
	GX_End();

	__lwp_heap_free(GXtexCache, FBtex);
}

/* [Xbox360] ★★ [Dr. Mario 64, pildoras] LO QUE LA CPU PINTA ENCIMA DEL FRAME.
 *
 * GLideN64 "Frame buffer emulation part II" (blog, 2014): "Co-processor
 * processes display list and then CPU draws an image over just rendered
 * frame" -- la pildora que cae en Dr. Mario, la lluvia de Jet Force Gemini --.
 * "The image must be applied with alpha blending... color buffer area in RDRAM
 * must be cleared to avoid garbage on screen."  Es la opcion copyFromRDRAM
 * (frameBufferEmulation), que su GLideN64.custom.ini pone para [DR.MARIO%2064], y aqui
 * hack_copyFromRDRAM.  Codigo de la referencia: VI.cpp:318 llama a
 * FrameBuffer_CopyFromRDRAM(VI_ORIGIN) antes de presentar, y
 * RDRAMtoColorBuffer::_copyFromRDRAM (BufferCopy/RDRAMtoColorBuffer.cpp):
 *
 *   1. lee el buffer ENTERO de la RDRAM, con el alfa del RGBA5551 (bit 0):
 *      lo que la CPU no pinto esta a 0 y queda transparente (RGBA16ToABGR32);
 *   2. si el buffer lo pinto el RDP en este frame (`m_changed`) lo pone a
 *      CERO en la RDRAM, para que el frame siguiente solo traiga lo nuevo;
 *   3. si habia algo (`summ != 0`), lo pinta encima con SRC_ALPHA /
 *      ONE_MINUS_SRC_ALPHA, sin Z.
 *
 * Es su camino SIN FBInfo, el del blog: el aviso por escritura (fBWrite en
 * gc_memory/memory.c) aqui esta a NULL (xbox/plataforma.c) y con el dynarec
 * las escrituras no pasarian por el.  glN64 no escribe los rellenos de color
 * en la RDRAM (_gDPFillRDRAM solo va en los borrados de Z), que es lo que
 * GLideN64 exige con la opcion (FrameBufferList::fillRDRAM).
 *
 * VI_GX_renderCpuFramebuffer, justo encima, NO sirve tal cual: solo entra sin
 * display lists, y fuerza el alfa a 1 (`| 0x80008000`), asi que taparia la
 * escena entera.  Esto es la misma conversion a RGB5A3 en tiles de 4x4 pero
 * con el alfa: bit 15 = 1 es RGB555 opaco y 0x0000 es transparente.  Ojo: la
 * rotacion de 32 bits de alli mezcla el alfa de los dos pixeles de la
 * palabra; aqui va por media palabra.
 *
 * Solo 16 bits por pixel (Dr. Mario): para 32 bits haria falta otra
 * conversion, que ningun juego medido pide.
 *
 * HILOS: con la lista en paralelo esto corre en el hilo de la lista, en orden
 * con las listas.  La CPU solo vuelve a pintar este buffer despues del DP_INT
 * de una lista POSTERIOR, que espera a que esa lista acabe: el vaciado de
 * aqui siempre llega antes que sus escrituras nuevas.  Y updateScreen lee la
 * RDRAM viva, no la foto (xbox/hilo_lista.cpp). */
static u32 _viCfbAlfa( u32 w )
{
	u32 hi = w >> 16;
	u32 lo = w & 0xFFFFu;
	hi = (hi & 1u) ? ((hi >> 1) | 0x8000u) : 0u;
	lo = (lo & 1u) ? ((lo >> 1) | 0x8000u) : 0u;
	return (hi << 16) | lo;
}

/* Sin emulacion de framebuffer: el buffer que mira el VI.  VI_ORIGIN suele ir
 * una linea (o unos bytes) por detras del principio del color image, y el
 * frame del RDP se dibujo desde ese principio: si cae ahi, manda el color
 * image, como GLideN64 con `m_startAddress`. */
static u32 VI_GX_inicioCFB()
{
	const u32 origen = RSP_SegmentToPhysical( *REG.VI_ORIGIN ) & 0x00FFFFFFu;
	const u32 zancada = _SHIFTR( *REG.VI_WIDTH, 0, 12 ) << 1;
	if (gDP.colorImage.address <= origen &&
	    origen < gDP.colorImage.address + zancada * 4u)
		return gDP.colorImage.address;
	return origen;
}

static void VI_GX_copiaDesdeRDRAM( u32 inicio, BOOL vaciar )
{
	if ((*REG.VI_STATUS & 3) != 2)   // solo 16 bits por pixel
		return;

	const u32 zancada = _SHIFTR( *REG.VI_WIDTH, 0, 12 ) << 1;
	const u32 ancho = VI.width, alto = VI.height;
	const u32 texW = (ancho + 3) & ~3u;
	const u32 texH = (alto + 3) & ~3u;

	inicio &= ~3u;
	if (zancada == 0 || ancho == 0 || alto == 0 || (texW << 1) > zancada)
		return;
	// RDRAMSize es una MASCARA (ultima direccion valida), no un tamano.
	if (inicio + zancada * texH > RDRAMSize + 1)
		return;
	if (!GXtexCache)
		return;

	u16 *tex = (u16*) __lwp_heap_allocate(GXtexCache, texW*texH*2+32);
	while (!tex && TextureCache_FreeOneTexture())
		tex = (u16*) __lwp_heap_allocate(GXtexCache, texW*texH*2+32);
	if (!tex)
		return;

	// RGB5A1 del N64 -> RGB5A3 de GX en tiles de 4x4, fila a fila de tile.
	BOOL hayAlgo = FALSE;
	{
		u32 *dst = (u32*) tex;
		u32 ty, tx, r;
		for (ty = 0; ty < texH; ty += 4)
			for (tx = 0; tx < texW; tx += 4)
				for (r = 0; r < 4; r++)
				{
					const u32 *src = (const u32*) &RDRAM[inicio + (ty + r) * zancada + (tx << 1)];
					const u32 w0 = _viCfbAlfa( src[0] );
					const u32 w1 = _viCfbAlfa( src[1] );
					if (w0 | w1)
						hayAlgo = TRUE;
					*dst++ = w0;
					*dst++ = w1;
				}
	}

	// Paso 2: vaciar, antes de mirar si habia algo (GLideN64 igual).  Es una
	// escritura NUESTRA sobre un buffer principal: su copia de RDRAM se pone
	// al dia, como tras _gDPFillRDRAM, o FrameBuffer_IsValid lo daria por
	// muerto si el juego lo usa luego como textura.
	if (vaciar)
	{
		memset( &RDRAM[inicio], 0, zancada * alto );
		FrameBuffer_RestampMarkers( inicio, inicio + zancada * alto - 1 );
	}

	if (!hayAlgo)
	{
		__lwp_heap_free(GXtexCache, tex);
		return;
	}

	// Paso 3: encima, con alfa y sin Z.  Mismo dibujo que VI_GX_renderCpuFramebuffer.
	GXTexObj texObj;
	GX_InvalidateTexAll();
	GX_InitTexObj(&texObj, tex, texW, texH, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);

	// Nada del dibujo de la escena: ni profundidad desde el color ni decal.
	GXD3D_SetRenderToDepth( 0 );
	GXD3D_SetDecalN64( 0 );
	{
		GXColor sinNiebla = { 0, 0, 0, 0 };
		GXD3D_SetBlenderNiebla( 0, sinNiebla );
	}

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	GX_SetZMode(GX_DISABLE,GX_ALWAYS,GX_FALSE);
	GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z8,0);
	GX_SetCullMode (GX_CULL_NONE);
	GX_SetFog(GX_FOG_NONE,0.1,1.0,0.0,1.0,GXCOL_BLACK);

	Mtx44 GXprojection;
	guOrtho(GXprojection, 0, 480, 0, 640, 0.0f, 1.0f);
	GX_LoadProjectionMtx(GXprojection, GX_ORTHOGRAPHIC);
	Mtx	GXmodelViewIdent;
	guMtxIdentity(GXmodelViewIdent);
	GX_LoadPosMtxImm(GXmodelViewIdent,GX_PNMTX0);
	GX_SetViewport((f32) OGL.GXorigX,(f32) OGL.GXorigY,(f32) OGL.GXwidth,(f32) OGL.GXheight, 0.0f, 1.0f);
	GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);

	const float u1 = (float)ancho / (float)texW;
	const float v1 = (float)alto / (float)texH;

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

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position2f32( 0.0f, 0.0f );
		GX_TexCoord2f32( 0.0f, 0.0f );
		GX_Position2f32( 640.0f, 0.0f );
		GX_TexCoord2f32( u1, 0.0f );
		GX_Position2f32( 640.0f, 480.0f );
		GX_TexCoord2f32( u1, v1 );
		GX_Position2f32( 0.0f, 480.0f );
		GX_TexCoord2f32( 0.0f, v1 );
	GX_End();

	__lwp_heap_free(GXtexCache, tex);

	// La lista siguiente vuelve a poner su estado (como tras OGL_GXclearEFB).
	OGL.GXupdateMtx = true;
	OGL.GXupdateFog = true;
	OGL_UpdateCullFace();
	OGL_UpdateViewport();
	gDP.changed |= CHANGED_SCISSOR | CHANGED_COMBINE | CHANGED_RENDERMODE;
}

void VI_GX_PreRetraceCallback(u32 retraceCnt)
{
	VI.which_fb ^= 1;
	VIDEO_SetPreRetraceCallback(NULL);
}

void VI_GX_DrawSyncCallback(u16 token)
{
	TextureCache_ReleaseRetired();

	VIDEO_SetNextFramebuffer(VI.xfb[token & 1]);
	VIDEO_Flush();
	VIDEO_SetPreRetraceCallback(VI_GX_PreRetraceCallback);
}
#endif // __GX__
