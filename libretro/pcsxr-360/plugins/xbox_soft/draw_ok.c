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
int *pPsxScreen = 0;
unsigned int g_pPitch;

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

/*
void BlitScreen16(unsigned char * surf,long x,long y)
{
	
 unsigned long lu;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 unsigned short LineOffset,SurfOffset;
 long lPitch= g_pPitch;

 if(PSXDisplay.RGB24)
  {
   unsigned char * pD;unsigned int startxy;

   surf+=PreviousPSXDisplay.Range.x0<<1;

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

   unsigned long * DSTPtr =
    ((unsigned long *)surf)+(PreviousPSXDisplay.Range.x0>>1);

   dx>>=1;

   LineOffset = 512 - dx;
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
 */
extern time_t tStart;

void DoBufferSwap(void)
{

	finalw = PSXDisplay.DisplayMode.x;
	finalh = PSXDisplay.DisplayMode.y;

	UpdateScrenRes(PSXDisplay.DisplayMode.x,PSXDisplay.DisplayMode.y);

  //A faire dans un thread
	BlitScreen32((unsigned char *)pPsxScreen, PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y);
//	BlitScreen16((unsigned char *)pPsxScreen, PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y);

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