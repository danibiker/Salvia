/*
	modded for libretro-frodo
*/

/*
  Hatari - sdlgui.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  A tiny graphical user interface for Hatari.
*/
#include <stdlib.h>
#include <boolean.h>
#include <ctype.h>
#include <string.h>

#include "utype.h"

#include "dialog.h"
#include "sdlgui.h"

#include "graph.h"
#include "libretro-core.h"

typedef struct{
     Sint16 x, y;
     Uint16 w, h;
} SDL_Rect;


Prefs *prefs;

extern int retroh,retrow;

#ifdef SNAP_BMP 
extern unsigned int emubkg[96*72];
#endif

#define TEXTURE_WIDTH retrow
#define TEXTURE_HEIGHT retroh

#define B ((rgba>> 8)&0xff)>>3 
#define G ((rgba>>16)&0xff)>>3
#define R ((rgba>>24)&0xff)>>3

#define RGB565(r, g, b)  (((r) << (5+16)) | ((g) << (5+8)) | (b<<5))

extern int Retro_PollEvent(uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick);

static const char *cross[] = {
  "X                               ",
  "XX                              ",
  "X.X                             ",
  "X..X                            ",
  "X...X                           ",
  "X....X                          ",
  "X.....X                         ",
  "X......X                        ",
  "X.......X                       ",
  "X........X                      ",
  "X.....XXXXX                     ",
  "X..X..X                         ",
  "X.X X..X                        ",
  "XX  X..X                        ",
  "X    X..X                       ",
  "     X..X                       ",
  "      X..X                      ",
  "      X..X                      ",
  "       XX                       ",
  "                                ",
};

void draw_cross(int x,int y)
{
	int i,j,idx;
	uint16_t color;
	int dx = 32, dy = 20;

	for(j=y;j<y+dy;j++)
   {
      idx=0;
      for(i=x;i<x+dx;i++)
      {
         if(cross[j-y][idx]=='.')DrawPointBmp((char*)Retro_Screen,i,j,0xffffffff);
         else if(cross[j-y][idx]=='X')DrawPointBmp((char*)Retro_Screen,i,j,0);
         idx++;			
      }
   }
}

#define DrawBoxF( x,  y,  z,  dx,  dy, rgba) \
DrawFBoxBmp((char*)Retro_Screen, x, y, dx, dy,rgba)

static int current_object = 0;				/* Current selected object */

#define FontW 6
#define FontH 8

int sdlgui_fontwidth  = FontW;			/* Width of the actual font */
int sdlgui_fontheight = FontH;			/* Height of the actual font */

#define fontwidth sdlgui_fontwidth
#define fontheight sdlgui_fontheight

/* Initialize the GUI. */
int SDLGui_Init(void)
{
	return 0;
}

/* Uninitialize the GUI. */
int SDLGui_UnInit(void)
{
   return 0;
}

/*-----------------------------------------------------------------------*/
/**
 * Inform the SDL-GUI about the actual SDL_Surface screen pointer and
 * prepare the font to suit the actual resolution.
 */

int SDLGui_SetScreen(void)
{
	memset(Retro_Screen, 0, sizeof(Retro_Screen));

	sdlgui_fontwidth  = FontW;
	sdlgui_fontheight = FontH;
	return 0;
}

/*-----------------------------------------------------------------------*/
/**
 * Center a dialog so that it appears in the middle of the screen.
 * Note: We only store the coordinates in the root box of the dialog,
 * all other objects in the dialog are positioned relatively to this one.
 */
void SDLGui_CenterDlg(SGOBJ *dlg)
{
	dlg[0].x = (TEXTURE_WIDTH  / fontwidth  - dlg[0].w) / 2;
	dlg[0].y = (TEXTURE_HEIGHT / fontheight - dlg[0].h) / 2;
}

/* Draw a text string. */
static void SDLGui_Text(int x, int y, const char *txt)
{       
   Draw_text((char*)Retro_Screen,x,y,1,0,1,1,40,(char *)txt);
}


/*-----------------------------------------------------------------------*/
/**
 * Draw a dialog text object.
 */
static void SDLGui_DrawText(const SGOBJ *tdlg, int objnum)
{
	int x = (tdlg[0].x+tdlg[objnum].x)*sdlgui_fontwidth;
	int y = (tdlg[0].y+tdlg[objnum].y)*sdlgui_fontheight;
	SDLGui_Text(x, y, tdlg[objnum].txt);
}


/*-----------------------------------------------------------------------*/
/**
 * Draw a edit field object.
 */
static void SDLGui_DrawEditField(const SGOBJ *edlg, int objnum)
{
	SDL_Rect rect;
	int x = (edlg[0].x+edlg[objnum].x)*sdlgui_fontwidth;
	int y = (edlg[0].y+edlg[objnum].y)*sdlgui_fontheight;
	SDLGui_Text(x, y, edlg[objnum].txt);
   DrawBoxF(x,y+ edlg[objnum].h * fontheight,0,
         edlg[objnum].w * fontwidth,1,0xffA0A0A0);
}


/*-----------------------------------------------------------------------*/
/**
 * Draw a dialog box object.
 */
static void SDLGui_DrawBox(const SGOBJ *bdlg, int objnum)
{
	int w, h, offset  = 0;
	Uint32 upleftc    = 0xFFFFFFFF;
   Uint32 downrightc = 0xff808080;
	Uint32 grey       = 0xffC0C0C0;
	int x             = bdlg[objnum].x*sdlgui_fontwidth;
	int y             = bdlg[objnum].y*sdlgui_fontheight;

   /* Since the root object is a box, too, */
	if (objnum > 0) 
   {
      /* we have to look for it now here and only
       * add its absolute coordinates if we need to */
      x += bdlg[0].x*sdlgui_fontwidth;
      y += bdlg[0].y*sdlgui_fontheight;
   }
	w = bdlg[objnum].w*sdlgui_fontwidth;
	h = bdlg[objnum].h*sdlgui_fontheight;

	if (bdlg[objnum].state & SG_SELECTED)
	{
		upleftc    = 0xff808080;
		downrightc = 0xFFFFFFFF;
	}

	/* The root box should be bigger than the screen, so we disable the offset there: */
	if (objnum != 0)
		offset = 1;

	/* Draw background: */
   DrawBoxF(x,y,0,w ,h,grey);
	/* Draw upper border: */
   DrawBoxF(x,y - offset,0,w ,1,upleftc);
	/* Draw left border: */
   DrawBoxF(x-offset,y,0,1 ,h,upleftc);
	/* Draw bottom border: */
   DrawBoxF(x,y + h - 1 + offset,0,w ,1,downrightc);
	/* Draw right border: */
   DrawBoxF(x + w - 1 + offset,y,0,1,h,downrightc);
}


/*-----------------------------------------------------------------------*/
/**
 * Draw a normal button.
 */
static void SDLGui_DrawButton(const SGOBJ *bdlg, int objnum)
{
	int x,y;

	SDLGui_DrawBox(bdlg, objnum);

	x = (bdlg[0].x + bdlg[objnum].x + (bdlg[objnum].w-strlen(bdlg[objnum].txt))/2) * sdlgui_fontwidth;
	y = (bdlg[0].y + bdlg[objnum].y + (bdlg[objnum].h-1)/2) * sdlgui_fontheight;

	if (bdlg[objnum].state & SG_SELECTED)
	{
		x++;
		y++;
	}
	SDLGui_Text(x, y, bdlg[objnum].txt);
}


/*-----------------------------------------------------------------------*/
/**
 * Draw a dialog radio button object.
 */
static void SDLGui_DrawRadioButton(const SGOBJ *rdlg, int objnum)
{
	char str[80];
	int x     = (rdlg[0].x + rdlg[objnum].x) * sdlgui_fontwidth;
	int y     = (rdlg[0].y + rdlg[objnum].y) * sdlgui_fontheight;

	if (rdlg[objnum].state & SG_SELECTED)
		str[0] = SGRADIOBUTTON_SELECTED;
	else
		str[0] = SGRADIOBUTTON_NORMAL;
	str[1]    = ' ';
	strcpy(&str[2], rdlg[objnum].txt);

	SDLGui_Text(x, y, str);
}


/*-----------------------------------------------------------------------*/
/**
 * Draw a dialog check box object.
 */
static void SDLGui_DrawCheckBox(const SGOBJ *cdlg, int objnum)
{
	char str[80];
	int x     = (cdlg[0].x + cdlg[objnum].x) * sdlgui_fontwidth;
	int y     = (cdlg[0].y + cdlg[objnum].y) * sdlgui_fontheight;

	if ( cdlg[objnum].state&SG_SELECTED )
		str[0] = SGCHECKBOX_SELECTED;
	else
		str[0] = SGCHECKBOX_NORMAL;
	str[1]    =' ';
	strcpy(&str[2], cdlg[objnum].txt);

	SDLGui_Text(x, y, str);
}


/*-----------------------------------------------------------------------*/
/**
 * Draw a scrollbar button.
 */
static void SDLGui_DrawScrollbar(const SGOBJ *bdlg, int objnum)
{
	SDL_Rect rect;
	int w, h;
   int offset   = 0;

	Uint32 grey0 = 0xff808080;//SDL_MapRGB(pSdlGuiScrn->format,128,128,128);
	Uint32 grey1 = 0xffC4C4C4;//SDL_MapRGB(pSdlGuiScrn->format,196,196,196);
	Uint32 grey2 = 0xff404040;//SDL_MapRGB(pSdlGuiScrn->format, 64, 64, 64);

	int x        = bdlg[objnum].x * sdlgui_fontwidth;
	int y        = bdlg[objnum].y * sdlgui_fontheight + bdlg[objnum].h;

   /* add mainbox absolute coordinates */
	x           += bdlg[0].x*sdlgui_fontwidth;
   /* add mainbox absolute coordinates */
	y           += bdlg[0].y*sdlgui_fontheight;
	
	w            = 1 * sdlgui_fontwidth;
	h            = bdlg[objnum].w;

	/* Draw background: */
	DrawBoxF(x,y,0,w ,h,grey0);
	/* Draw upper border: */
	DrawBoxF(x,y - offset,0,w ,1,grey1);
	/* Draw bottom border: */
	DrawBoxF(x,y + h - 1 + offset,0,w ,1,grey2);
}

/*-----------------------------------------------------------------------*/
/**
 *  Draw a dialog popup button object.
 */
static void SDLGui_DrawPopupButton(const SGOBJ *pdlg, int objnum)
{
	int x, y, w;
	const char *downstr = "\x02";

	SDLGui_DrawBox(pdlg, objnum);

	x = (pdlg[0].x + pdlg[objnum].x) * sdlgui_fontwidth;
	y = (pdlg[0].y + pdlg[objnum].y) * sdlgui_fontheight;
	w = pdlg[objnum].w * sdlgui_fontwidth;

	SDLGui_Text(x, y, pdlg[objnum].txt);
	SDLGui_Text(x+w-sdlgui_fontwidth, y, downstr);
}

/*-----------------------------------------------------------------------*/
/**
 * Draw a whole dialog.
 */
static void SDLGui_DrawDialog(const SGOBJ *dlg)
{
	int i;
#ifdef SNAP_BMP 
	int j,idx = 0;
#endif

	memset(Retro_Screen, 0, sizeof(Retro_Screen));

#ifdef SNAP_BMP 
   for(j=0;j<72;j++)
   {
      for(i=152;i<248;i++)
      {
         DrawPointBmp((char *)Retro_Screen,i,j,emubkg[idx]);
         idx++;			
      }
   }
#endif

	for (i = 0; dlg[i].type != -1; i++)
	{
		switch (dlg[i].type)
		{
		 case SGBOX:
			SDLGui_DrawBox(dlg, i);
			break;
		 case SGTEXT:
			SDLGui_DrawText(dlg, i);
			break;
		 case SGEDITFIELD:
			SDLGui_DrawEditField(dlg, i);
			break;
		 case SGBUTTON:
			SDLGui_DrawButton(dlg, i);
			break;
		 case SGRADIOBUT:
			SDLGui_DrawRadioButton(dlg, i);
			break;
		 case SGCHECKBOX:
			SDLGui_DrawCheckBox(dlg, i);
			break;
		 case SGPOPUP:
			SDLGui_DrawPopupButton(dlg, i);
			break;
		 case SGSCROLLBAR:
			SDLGui_DrawScrollbar(dlg, i);
			break;
		}
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Search an object at a certain position.
 */
static int SDLGui_FindObj(const SGOBJ *dlg, int fx, int fy)
{
	int i;
	int xpos, ypos;
	int ob  = -1;
	int len = 0;
	while (dlg[len].type != -1)
      len++;

	xpos = fx / sdlgui_fontwidth;
	ypos = fy / sdlgui_fontheight;
	/* Now search for the object: */
	for (i = len; i >= 0; i--)
	{
		/* clicked on a scrollbar ? */
		if (dlg[i].type == SGSCROLLBAR)
      {
         if (     xpos >= dlg[0].x + dlg[i].x 
               && xpos  < dlg[0].x + dlg[i].x + 1)
         {
            ypos = dlg[i].y * sdlgui_fontheight + dlg[i].h + dlg[0].y * sdlgui_fontheight;
            if (fy >= ypos && fy < ypos + dlg[i].w)
            {
               ob = i;
               break;
            }
         }
      }
		/* clicked on another object ? */
		else if (xpos >= dlg[0].x+dlg[i].x 
            && ypos >= dlg[0].y+dlg[i].y
		      && xpos  < dlg[0].x+dlg[i].x  
            + dlg[i].w 
            && ypos  < dlg[0].y+dlg[i].y 
            + dlg[i].h)
		{
			ob = i;
			break;
		}
	}

	return ob;
}

extern int touch;
extern int gmx,gmy;
int okold=0,boutc=0;

/*-----------------------------------------------------------------------*/
/**SDLGui_DoDialog
 * Show and process a dialog. Returns the button number that has been
 * pressed or SDLGUI_UNKNOWNEVENT if an unsupported event occured (will be
 * stored in parameter pEventOut).
 */
int SDLGui_DoDialog(SGOBJ *dlg, int *pEventOut)
{
	int sdlEvent;
	SDL_Rect rct;
	SDL_Rect dlgrect, bgrect;
	int obj       = 0;
	int oldbutton = 0;
	int retbutton = 0;
	int i, j, b   = 0;
   Uint32 grey   = 0xffC0C0C0;

	dlgrect.x     = dlg[0].x * sdlgui_fontwidth;
	dlgrect.y     = dlg[0].y * sdlgui_fontheight;
	dlgrect.w     = dlg[0].w * sdlgui_fontwidth;
	dlgrect.h     = dlg[0].h * sdlgui_fontheight;

	bgrect.x      = 0;
   bgrect.y      = 0;
	bgrect.w      = dlgrect.w;
	bgrect.h      = dlgrect.h;

	/* (Re-)draw the dialog */
	SDLGui_DrawDialog(dlg);

	Retro_PollEvent(NULL,NULL,NULL);

   if(touch!=-1)
   {
      b     = 1;
      boutc = 1;
   }
	else
      boutc = 0;

	i        = gmx;
   j        = gmy;

	/* If current object is the scrollbar, 
      and mouse is still down, we can scroll it */
	/* also if the mouse pointer has left the scrollbar */
	if (dlg[current_object].type == SGSCROLLBAR)
   {
      if (b == 1)
      {
         obj             = current_object;
         dlg[obj].state |= SG_MOUSEDOWN;
         oldbutton       = obj;
         retbutton       = obj;

      }
      else
      {
         obj             = current_object;
         current_object  = 0;
         dlg[obj].state &= SG_MOUSEUP;
         okold           = 1;
      }
   }
	else
   {
      obj            = SDLGui_FindObj(dlg, i, j);
      current_object = obj;
      if (obj > 0 && (dlg[obj].flags&SG_TOUCHEXIT) )
      {
         oldbutton = obj;
         if (b ==1)
         {
            dlg[obj].state |= SG_SELECTED;
            retbutton       = obj;
         }
         else
            dlg[obj].state &= ~SG_SELECTED;				
      }
   }

        /* The main loop */
	while (retbutton == 0 && !bQuitProgram)
   {
      Retro_PollEvent(NULL,NULL,NULL);

      draw_cross(gmx,gmy);

      if (touch != -1 && okold == 0)
      {
         okold = 1;
         obj   = SDLGui_FindObj(dlg, gmx, gmy);

         if (obj>0)
         {
            if (dlg[obj].type==SGBUTTON)
            {
               dlg[obj].state |= SG_SELECTED;
               SDLGui_DrawButton(dlg, obj);

               oldbutton       = obj;
            }
            if (dlg[obj].type==SGSCROLLBAR)
            {
               dlg[obj].state |= SG_MOUSEDOWN;
               oldbutton       = obj;
            }
            if ( dlg[obj].flags&SG_TOUCHEXIT )
            {
               dlg[obj].state |= SG_SELECTED;
               retbutton       = obj;
            }
         }

      }
      else if(touch==-1 && okold==1)
      {
         okold=0;

         // It was the left button: Find the object under the mouse cursor /
         obj = SDLGui_FindObj(dlg, gmx, gmy);
         if (obj>0)
         {
            switch (dlg[obj].type)
            {
               case SGBUTTON:
                  if (oldbutton==obj)
                     retbutton=obj;
                  break;

               case SGSCROLLBAR:
                  dlg[obj].state &= SG_MOUSEUP;

                  if (oldbutton==obj)
                     retbutton=obj;
                  break;
               case SGEDITFIELD:
                  break;
               case SGRADIOBUT:
                  for (i = obj-1; i > 0 && dlg[i].type == SGRADIOBUT; i--)
                  {
                     // Deselect all radio buttons in this group /
                     dlg[i].state &= ~SG_SELECTED;

                     DrawBoxF((dlg[0].x+dlg[i].x)*fontwidth,(dlg[0].y+dlg[i].y)*fontheight,0,fontwidth ,fontheight,grey);
                     SDLGui_DrawRadioButton(dlg, i);

                  }
                  for (i = obj+1; dlg[i].type == SGRADIOBUT; i++)
                  {
                     // Deselect all radio buttons in this group /
                     dlg[i].state &= ~SG_SELECTED;

                     DrawBoxF((dlg[0].x+dlg[i].x)*fontwidth,(dlg[0].y+dlg[i].y)*fontheight,0,fontwidth ,fontheight,grey);
                     SDLGui_DrawRadioButton(dlg, i);

                  }
                  // Select this radio button
                  dlg[obj].state |= SG_SELECTED; 

                  DrawBoxF((dlg[0].x+dlg[obj].x)*fontwidth,(dlg[0].y+dlg[obj].y)*fontheight,0,fontwidth ,fontheight,grey);
                  SDLGui_DrawRadioButton(dlg, obj);

                  break;
               case SGCHECKBOX:
                  dlg[obj].state ^= SG_SELECTED;

                  DrawBoxF((dlg[0].x+dlg[obj].x)*fontwidth,(dlg[0].y+dlg[obj].y)*fontheight,0,fontwidth ,fontheight,grey);
                  SDLGui_DrawCheckBox(dlg, obj);

                  break;
               case SGPOPUP:
                  dlg[obj].state |= SG_SELECTED;
                  SDLGui_DrawPopupButton(dlg, obj);

                  retbutton=obj;
                  break;
            }
         }
         if (oldbutton > 0)
         {	
            dlg[oldbutton].state &= ~SG_SELECTED;
            SDLGui_DrawButton(dlg, oldbutton);
            oldbutton = 0;
         }
         if (obj >= 0 && (dlg[obj].flags&SG_EXIT))
         {
            if(dlg[obj].type==SGBUTTON)
               dlg[obj].state &= ~SG_SELECTED;
            retbutton = obj;
         }
      }

      if(retbutton ==0)
         retbutton = 1;
   }


	if (retbutton == SDLGUI_QUIT)
		bQuitProgram = true;

	return retbutton;
}
