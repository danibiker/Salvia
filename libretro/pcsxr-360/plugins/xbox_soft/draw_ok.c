/***************************************************************************
draw.c  -  description
-------------------
begin                : Sun Oct 28 2001
copyright            : (C) 2001 by Pete Bernert
email                : BlackDove@addcom.de
***************************************************************************/
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version. See also the license.txt file for *
*   additional informations.                                              *
*                                                                         *
***************************************************************************/

#define _IN_DRAW

#include "externals.h"
#include "gpu.h"
#include "draw.h"
#include "prim.h"
#include "menu.h"
#include "interp.h"
#include "swap.h"
#include <xtl.h>
#include "xb_video.h"
#include "../../libpcsxcore/psxcommon.h"


// misc globals
int            iResX;
int            iResY;
long           lLowerpart;
BOOL           bIsFirstFrame = TRUE;
BOOL           bCheckMask = FALSE;
unsigned short sSetMask = 0;
unsigned long  lSetMask = 0;
int            iDesktopCol = 16;
int            iShowFPS = 0;
int            iWinSize;
int            iMaintainAspect = 0;
int            iUseNoStretchBlt = 0;
int            iFastFwd = 0;
int            iDebugMode = 0;
int            iFVDisplay = 0;
PSXPoint_t     ptCursorPoint[8];
unsigned short usCursorActive = 0;

unsigned char *pBackBuffer = 0;

#ifdef LIBRETRO
/* Defined in video_stub.cpp (main project) */
extern unsigned char *pPsxScreen;
extern unsigned int g_pPitch;
extern int g_useRGB565;
#else
int *pPsxScreen = 0;
unsigned int g_pPitch;
int g_useRGB565 = 0;
#endif

int finalw,finalh;
int iFPSEInterface =0;

////////////////////////////////


#include <time.h>


////////////////////////////////////////////////////////////////////////

#ifndef MAX
#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))
#endif


////////////////////////////////////////////////////////////////////////
// X STUFF :)
////////////////////////////////////////////////////////////////////////
char *               Xpixels;
char *               pCaptionText;

static int fx=0;

// close display
void DestroyDisplay(void)
{

}

static int depth=0;
int root_window_id=0;


// Create display
void CreateDisplay(void){

}


// Memset fail :s => clear to black
__inline void TexZero(void * buf, int len) {
	int i;
	unsigned char * s = (unsigned char*)buf;
	for(i = 0; i < len; i++) {
		s[i] = 0;
	}
}

/* True Color (24-bit) shadow framebuffer.  Definidos en gpu_duck_driver.cpp.
 * Cuando g_pcsxr_true_color_active != 0 Y g_psxVuw24 != NULL, esta funcion
 * sirve los pixels desde el shadow en vez de hacer la expansion 5->8 bit
 * desde psxVuw.  Se mantiene un sanity check por pixel para detectar
 * inconsistencias (write externo a psxVuw que no toco el shadow): si la
 * cuantizacion del shadow no coincide con el valor de psxVuw, ese pixel se
 * reexpande desde psxVuw via bit-replication y se sincroniza el shadow. */
extern unsigned int* g_psxVuw24;
extern int           g_pcsxr_true_color_active;

static void BlitScreen32(unsigned char * surf, int32_t x, int32_t y)
{
	uint32_t * destpix;
	uint32_t * __restrict rdest;

	unsigned char *pD;
	unsigned int startxy;
	uint32_t lu;
	
	unsigned short s, s1, s2, s3, s4, s5, s6, s7;
	uint32_t d, d1, d2, d3, d4, d5, d6, d7;
	unsigned short row, column;
	unsigned short dx = PreviousPSXDisplay.Range.x1;
	unsigned short dy = PreviousPSXDisplay.DisplayMode.y;

	int loop = 0;
	int offset = 0;
	int32_t lPitch = g_pPitch;
	const int true_color = (g_pcsxr_true_color_active && g_psxVuw24 != 0);

	for(loop=0; loop < 1024; loop += 128)
		__dcbt(loop, psxVuw);

	if (PreviousPSXDisplay.Range.y0) // centering needed?
	{
		TexZero(surf, (PreviousPSXDisplay.Range.y0 >> 1) * lPitch);

		dy -= PreviousPSXDisplay.Range.y0;
		surf += (PreviousPSXDisplay.Range.y0 >> 1) * lPitch;

		TexZero(surf + dy * lPitch,
			((PreviousPSXDisplay.Range.y0 + 1) >> 1) * lPitch);
	}

	if (PreviousPSXDisplay.Range.x0)
	{
		for (column = 0; column < dy; column++)
		{
			destpix = (uint32_t *)(surf + (column * lPitch));
			TexZero(destpix, PreviousPSXDisplay.Range.x0 << 2);
		}
		surf += PreviousPSXDisplay.Range.x0 << 2;
	}

	if (PSXDisplay.RGB24)
	{
		for (column = 0; column < dy; column++)
		{
			startxy = ((1024) * (column + y)) + x;
			pD = (unsigned char *)&psxVuw[startxy];
			destpix = (uint32_t *)(surf + (column * lPitch));
			
			for (row = 0; row < dx; row++)
			{
				rdest = &destpix[row];
				
				lu = *((uint32_t *)pD);

				d = 0xff000000 | (RED(lu) << 16) | (GREEN(lu) << 8) | (BLUE(lu));
				
				// destpix[row] = d;
				rdest[0] = d;

				pD += 3;
			}
		}
	}
	else if (true_color)
	{
		/* === True-color path ===
		 *
		 * Sirve los pixels desde g_psxVuw24 (24-bit shadow) en vez de
		 * expandir 5->8 bit desde psxVuw.  Por cada pixel hace un sanity
		 * check (quantize(shadow_8) == vram_5).  Si NO coincide significa
		 * que algo (primLoadImage, MDEC, save-state restore...) escribio
		 * a psxVuw sin actualizar el shadow.  En ese caso re-expandimos
		 * desde psxVuw via bit-replication y sincronizamos el shadow para
		 * el proximo frame.
		 *
		 * Coste: ~4 ops por pixel + 1 branch.  A 1920x1080 ~ 2M pixels,
		 * el tiempo extra es marginal.  Se mantiene loop simple (de a 1
		 * pixel) en vez de unrolling 8x del path 5-bit porque el coste
		 * de la rama de resync no merece la complejidad. */
		for (column = 0; column < dy; column++)
		{
			startxy = (1024 * (column + y)) + x;
			destpix = (uint32_t *)(surf + (column * lPitch));

			for (row = 0; row < dx; row++)
			{
				unsigned short s_vram;
				uint32_t s24;
				unsigned int r5_vram, g5_vram, b5_vram;
				unsigned int r5_sh, g5_sh, b5_sh;
				int dr, dg, db;

				s_vram = __loadshortbytereverse(0, &psxVuw[startxy]);
				s24 = g_psxVuw24[startxy];
				/* Sanity check con tolerancia de +-1 LSB.
				 *
				 * Por que tolerancia y no comparacion exacta:
				 * ShadePixel escribe DOS valores diferentes -- el 5-bit
				 * "PSX-accurate" a psxVuw (LUT + dither matrix + 5-bit
				 * blend de transparencia) y el 8-bit true-color a g_psxVuw24
				 * (formula directa + 8-bit blend de transparencia).  Ambos
				 * operan sobre los mismos inputs pero las dos formulas no
				 * dan resultados que roundtrip exactamente: el blend 5-bit
				 * cuantiza fg y bg ANTES de mezclar, perdiendo precision
				 * que el blend 8-bit conserva.  Resultado: hasta +-1 LSB
				 * de drift por canal en pixels transparentes -- que en
				 * Silent Hill (niebla = primitivas semi-transparentes) son
				 * la mayoria.
				 *
				 * El sanity check original (comparacion exacta) rechazaba
				 * todos esos pixels y reexpandia desde el 5-bit bit-replicado,
				 * tirando la precision 8-bit.  Era equivalente a NO tener
				 * true color en la niebla.
				 *
				 * Tolerancia +-1 LSB acepta el drift de cuantizacion y
				 * sigue detectando escrituras externas grandes (loadImage,
				 * MDEC, save-state restore) que no pasaron por gpu_duck.
				 * Falsos negativos solo si la escritura externa cae justo
				 * en una ventana de +-1 LSB de la stale shadow -- raro y
				 * visualmente equivalente. */
				r5_vram = (s_vram >>  0) & 0x1F;
				g5_vram = (s_vram >>  5) & 0x1F;
				b5_vram = (s_vram >> 10) & 0x1F;
				r5_sh   = (s24 >> 19) & 0x1F;
				g5_sh   = (s24 >> 11) & 0x1F;
				b5_sh   = (s24 >>  3) & 0x1F;
				dr = (int)r5_sh - (int)r5_vram; if (dr < 0) dr = -dr;
				dg = (int)g5_sh - (int)g5_vram; if (dg < 0) dg = -dg;
				db = (int)b5_sh - (int)b5_vram; if (db < 0) db = -db;
				if (dr > 1 || dg > 1 || db > 1)
				{
					/* Out of sync.  External write to psxVuw.  Re-expand. */
					const unsigned int r8 = (r5_vram << 3) | (r5_vram >> 2);
					const unsigned int g8 = (g5_vram << 3) | (g5_vram >> 2);
					const unsigned int b8 = (b5_vram << 3) | (b5_vram >> 2);
					s24 = (r8 << 16) | (g8 << 8) | b8;
					g_psxVuw24[startxy] = s24;
				}
				destpix[row] = s24 | 0xff000000;
				startxy++;
			}
		}
	}
	else
	{
		for (column = 0;column<dy;column++)
		{
			startxy = (1024 * (column + y)) + x;
			destpix = (uint32_t *)(surf + (column * lPitch));			

			for (row = 0; row < dx; row+=8)
			{					
				rdest = &destpix[row];

				s =    __loadshortbytereverse(0,&psxVuw[startxy++]);
				s1 =   __loadshortbytereverse(0,&psxVuw[startxy++]);
				s2 =   __loadshortbytereverse(0,&psxVuw[startxy++]);
				s3 =   __loadshortbytereverse(0,&psxVuw[startxy++]);
				s4 =   __loadshortbytereverse(0,&psxVuw[startxy++]);
				s5 =   __loadshortbytereverse(0,&psxVuw[startxy++]);
				s6 =   __loadshortbytereverse(0,&psxVuw[startxy++]);
				s7 =   __loadshortbytereverse(0,&psxVuw[startxy++]);

			
				d =  (((s << 19) & 0xf80000) | ((s << 6) & 0xf800) | ((s >> 7) & 0xf8)) | 0xff000000;
				d1 = (((s1 << 19) & 0xf80000) | ((s1 << 6) & 0xf800) | ((s1 >> 7) & 0xf8)) | 0xff000000;
				d2 = (((s2 << 19) & 0xf80000) | ((s2 << 6) & 0xf800) | ((s2 >> 7) & 0xf8)) | 0xff000000;
				d3 = (((s3 << 19) & 0xf80000) | ((s3 << 6) & 0xf800) | ((s3 >> 7) & 0xf8)) | 0xff000000;
				d4 = (((s4 << 19) & 0xf80000) | ((s4 << 6) & 0xf800) | ((s4 >> 7) & 0xf8)) | 0xff000000;
				d5 = (((s5 << 19) & 0xf80000) | ((s5 << 6) & 0xf800) | ((s5 >> 7) & 0xf8)) | 0xff000000;
				d6 = (((s6 << 19) & 0xf80000) | ((s6 << 6) & 0xf800) | ((s6 >> 7) & 0xf8)) | 0xff000000;
				d7 = (((s7 << 19) & 0xf80000) | ((s7 << 6) & 0xf800) | ((s7 >> 7) & 0xf8)) | 0xff000000; 

				rdest[0] = d;
				rdest[1] = d1;
				rdest[2] = d2;
				rdest[3] = d3;
				rdest[4] = d4;
				rdest[5] = d5;
				rdest[6] = d6;
				rdest[7] = d7;

			}
		}
	}
}


void BlitScreen16(unsigned char * surf,long x,long y)
{

 unsigned long lu;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 unsigned short LineOffset,SurfOffset;
 long lPitch= g_pPitch;

 /* Vertical letterbox bars.  BlitScreen32 already does this; the
  * 16-bit path was missing it, which is why Descent (and any other
  * game with non-zero Range.y0) showed a band of random/garbage
  * pixels at the bottom of the screen in RGB565 mode while looking
  * fine in XRGB8888.  The garbage is whatever was previously left in
  * pPsxScreen for that region — random RetroArch backbuffer
  * contents, residue from the previous frame, etc.
  *
  * Same arithmetic as BlitScreen32 but with 16-bit (2 byte) pixels
  * instead of 32-bit, hence no <<2 shift on the byte counts. */
 if (PreviousPSXDisplay.Range.y0)                       // centering needed?
 {
   TexZero(surf, (PreviousPSXDisplay.Range.y0 >> 1) * lPitch);

   dy -= PreviousPSXDisplay.Range.y0;
   surf += (PreviousPSXDisplay.Range.y0 >> 1) * lPitch;

   TexZero(surf + dy * lPitch,
           ((PreviousPSXDisplay.Range.y0 + 1) >> 1) * lPitch);
 }

 /* Horizontal letterbox columns.  Match BlitScreen32: zero the left
  * margin column-by-column, then advance surf past it.  Bytes per
  * pixel is 2 here (vs 4 in 32-bit), so the shift is <<1. */
 if (PreviousPSXDisplay.Range.x0)
 {
   unsigned char *p;
   for (column = 0; column < dy; column++)
   {
     p = surf + (column * lPitch);
     TexZero(p, PreviousPSXDisplay.Range.x0 << 1);
   }
   surf += PreviousPSXDisplay.Range.x0 << 1;
 }

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   for(column=0;column<dy;column++)
    {
     startxy=((1024)*(column+y))+x;

     pD=(unsigned char *)&psxVuw[startxy];

     for(row=0;row<dx;row++)
      {
       lu=*((unsigned long *)pD);
       *((unsigned short *)((surf)+(column*lPitch)+(row<<1)))=
         ((RED(lu)<<8)&0xf800)|((GREEN(lu)<<3)&0x7e0)|(BLUE(lu)>>3);
       pD+=3;
      }
    }
  }
 else
  {
   unsigned long * SRCPtr = (unsigned long *)(psxVuw +
                             (y<<10) + x);

   /* surf was already advanced past the horizontal margin above
    * (when Range.x0 != 0), so DSTPtr starts right at the content
    * area.  The original code did `surf + (Range.x0 >> 1)` here as
    * a 32-bit pointer offset, which was equivalent only when no
    * horizontal centering was active. */
   unsigned long * DSTPtr = (unsigned long *)surf;

   dx>>=1;

   LineOffset = 512 - dx;

   //if pitch mismatch would wrap SurfOffset; skip blit
   if((lPitch>>2) < (long)dx) {
	   OutputDebugStringA("pitch mismatch\n");
	   return;
   }


   SurfOffset = (lPitch>>2) - dx;

   for(column=0;column<dy;column++)
    {
     for(row=0;row<dx;row++)
      {
		 lu=GETLE16D(SRCPtr++);

       *DSTPtr++=
        ((lu<<11)&0xf800f800)|((lu<<1)&0x7c007c0)|((lu>>10)&0x1f001f);
      }
     SRCPtr += LineOffset;
     DSTPtr += SurfOffset;

   }
  }
 }
 
extern time_t tStart;

void DoBufferSwap(void)
{

	finalw = PSXDisplay.DisplayMode.x;
	finalh = PSXDisplay.DisplayMode.y;

	UpdateScrenRes(PSXDisplay.DisplayMode.x,PSXDisplay.DisplayMode.y);

	/* When bSkipNextFrame is set (by libretro frameskip or the GPU plugin),
	 * skip the expensive pixel conversion blit.  The previous frame's
	 * content remains in pPsxScreen for the frontend to reuse. */
	if (bSkipNextFrame)
		return;

	if (g_useRGB565)
		BlitScreen16((unsigned char *)pPsxScreen, PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y);
	else
		BlitScreen32((unsigned char *)pPsxScreen, PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y);

    DisplayUpdate();
}

void DoClearScreenBuffer(void)                         // CLEAR DX BUFFER
{
}

void DoClearFrontBuffer(void)                          // CLEAR DX BUFFER
{
}

int Xinitialize()
{
	VideoInit();

	iDesktopCol=16;
	
	bUsingTWin=FALSE;

	InitMenu();

	bIsFirstFrame = FALSE;                                // done

	if(iShowFPS)
	{
		iShowFPS=0;
		ulKeybits|=KEY_SHOWFPS;
		szDispBuf[0]=0;
		BuildDispMenu(0);
	}

	return 0;
}

void Xcleanup()                                        // X CLEANUP
{
	CloseMenu();
}

unsigned long ulInitDisplay(void)
{
	CreateDisplay();                                      // x stuff
	Xinitialize();                                        // init x
	return 1;
}

void CloseDisplay(void)
{
	Xcleanup();                                           // cleanup dx
	DestroyDisplay();
}

void CreatePic(unsigned char * pMem)
{
}

void DestroyPic(void)
{
}

void DisplayPic(void)
{
}

void ShowGpuPic(void)
{
}

void ShowTextGpuPic(void)
{
}