/*
 *  Display.cpp - C64 graphics display, emulator window handling
 *
 *  Frodo (C) 1994-1997,2002-2005 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdeps.h"

#include "Display.h"
#include "main.h"
#include "Prefs.h"

#include "retro_video.h"

#include "C64.h"
#ifdef HAVE_SAM
#include "SAM.h"
#endif
#include "Version.h"

#include <libretro.h>
#include "libretro-core.h"
#include "graph.h"
#include "vkbd_def.h"

/* LED states */
enum
{
	LED_OFF,		   /* LED off */
	LED_ON,			/* LED on (green) */
	LED_ERROR_ON,	/* LED blinking (red), currently on */
	LED_ERROR_OFF	/* LED blinking, currently off */
};

/* Colors for speedometer/drive LEDs */
enum
{
   black        = 0,
   white        = 1,
   fill_gray    = 16,
   shine_gray   = 17,
   shadow_gray  = 18,
   red          = 19,
   green        = 20,
   PALETTE_SIZE = 21
};

// Keyboard
static bool num_locked = false;

// For LED error blinking
static C64Display *c64_disp = NULL;

retro_Surface *screen; //1bpp depth bitmap surface
retro_pal palette[PALETTE_SIZE];
retro_Rect r = {0, DISPLAY_Y, DISPLAY_X, 15}; //384*272+16

int CTRLON=-1;
int RSTOPON=-1;
static int vkx=0,vky=0;
unsigned int mpal[21];

extern int retrow; 
extern int retroh;
extern retro_input_state_t input_state_cb;
extern int CROP_WIDTH;
extern int CROP_HEIGHT;
extern int VIRTUAL_WIDTH;
extern int NPAGE;
extern int KCOL;
extern int BKGCOLOR;
extern int SHIFTON;
extern int SHOWKEY;

/* forward declarations */
int Retro_PollEvent(uint8 *key_matrix,
      uint8 *rev_matrix, uint8 *joystick);

#define USE_PEPTO_COLORS 1

#ifdef USE_PEPTO_COLORS

// C64 color palette
// Values based on measurements by Philip "Pepto" Timmermann <pepto@pepto.de>
// (see http://www.pepto.de/projects/colorvic/)
const uint8 palette_red[16] = {
	0x00, 0xff, 0x86, 0x4c, 0x88, 0x35, 0x20, 0xcf, 0x88, 0x40, 0xcb, 0x34, 0x68, 0x8b, 0x68, 0xa1
};

const uint8 palette_green[16] = {
	0x00, 0xff, 0x19, 0xc1, 0x17, 0xac, 0x07, 0xf2, 0x3e, 0x2a, 0x55, 0x34, 0x68, 0xff, 0x4a, 0xa1
};

const uint8 palette_blue[16] = {
	0x00, 0xff, 0x01, 0xe3, 0xbd, 0x0a, 0xc0, 0x2d, 0x00, 0x00, 0x37, 0x34, 0x68, 0x59, 0xff, 0xa1
};

#else

// C64 color palette (traditional Frodo colors)
const uint8 palette_red[16] = {
	0x00, 0xff, 0x99, 0x00, 0xcc, 0x44, 0x11, 0xff, 0xaa, 0x66, 0xff, 0x40, 0x80, 0x66, 0x77, 0xc0
};

const uint8 palette_green[16] = {
	0x00, 0xff, 0x00, 0xff, 0x00, 0xcc, 0x00, 0xdd, 0x55, 0x33, 0x66, 0x40, 0x80, 0xff, 0x77, 0xc0
};

const uint8 palette_blue[16] = {
	0x00, 0xff, 0x00, 0xcc, 0xcc, 0x44, 0x99, 0x00, 0x00, 0x00, 0x66, 0x40, 0x80, 0x66, 0xff, 0xc0
};

#endif


/*
 *  Update drive LED display (deferred until Update())
 */

void C64Display::UpdateLEDs(int l0, int l1, int l2, int l3)
{
	led_state[0] = l0;
	led_state[1] = l1;
	led_state[2] = l2;
	led_state[3] = l3;
}

/*
 *  Display_SDL.i - C64 graphics display, emulator window handling,
 *                  SDL specific stuff
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

/*
  C64 keyboard matrix:

    Bit 7   6   5   4   3   2   1   0
  0    CUD  F5  F3  F1  F7 CLR RET DEL
  1    SHL  E   S   Z   4   A   W   3
  2     X   T   F   C   6   D   R   5
  3     V   U   H   B   8   G   Y   7
  4     N   O   K   M   0   J   I   9
  5     ,   @   :   .   -   L   P   +
  6     /   ^   =  SHR HOM  ;   *   £
  7    R/S  Q   C= SPC  2  CTL  <-  1
*/

#define MATRIX(a,b) (((a) << 3) | (b))

void retro_Frect(retro_Surface *buffer,int x,int y,int dx,int dy,unsigned  color)
{
	int i,j,idx;

   	for(i=x;i<x+dx;i++)
   	{
      		for(j=y;j<y+dy;j++)
      		{
         		idx=i+j*buffer->pitch;
         		buffer->pixels[idx]=color;	
      		}
	} 
}

void retro_FillRect(retro_Surface * surf,retro_Rect *rect,unsigned int col)
{
	if (!rect)
      retro_Frect(surf,0,0,surf->w ,surf->h,col); 
	else
      retro_Frect(surf,rect->x,rect->y,rect->w ,rect->h,col); 
}

void Retro_BlitSurface(retro_Surface *ss)
{
   unsigned char *pout;
   unsigned char *pin;
	retro_Rect src,dst;
	int x,y,w;

	src.x = 0;
	src.y = 0;
	src.w = ss->w;
	src.h = ss->h;
	dst.x = 0;
	dst.y = 0;
	dst.w = retrow;
	dst.h = retroh;

	pout  = (unsigned char *)Retro_Screen+(dst.x*4+dst.y*retrow*4);
	pin   = (unsigned char *)ss->pixels+(src.x*1+src.y*ss->w*1);

	for(y=0;y<src.h;y++)
   {
      for(x=0;x<src.w;x++)
      {
         unsigned int mcoul=palette[*pin].r<<16|palette[*pin].g<<8|palette[*pin].b;

         for(w=0;w<4;w++)
         {
            *pout=(mcoul>>(8*w))&0xff;
            pout++;
         }
         pin++;

      }
      pin  += (ss->w-src.w)  * 1;
      pout += (retrow-src.w) * 4;
   }
}

void Retro_ClearSurface(retro_Surface *ss)
{
   memset(ss->pixels,0,ss->h*ss->pitch);
}

/*
 *  Draw string into surface using the C64 ROM font
 */

void draw_string(retro_Surface *s, int x, int y, const char *str, uint8 front_color, uint8 back_color)
{
	char c;
	uint8 *pb = (uint8 *)s->pixels + s->pitch*y + x;
	while ((c = *str++) != 0)
   {
      unsigned y;
      uint8 *q = TheC64->Char + c*8 + 0x800;
      uint8 *p = pb;
      for (y = 0; y < 8; y++)
      {
         uint8 v = *q++;
         p[0]    = (v & 0x80) ? front_color : back_color;
         p[1]    = (v & 0x40) ? front_color : back_color;
         p[2]    = (v & 0x20) ? front_color : back_color;
         p[3]    = (v & 0x10) ? front_color : back_color;
         p[4]    = (v & 0x08) ? front_color : back_color;
         p[5]    = (v & 0x04) ? front_color : back_color;
         p[6]    = (v & 0x02) ? front_color : back_color;
         p[7]    = (v & 0x01) ? front_color : back_color;
         p      += s->pitch;
      }
      pb        += 8;
   }
}

/* 0 clear emu scr , 1 clear c64 scr ,>1 clear both*/
void Screen_SetFullUpdate(int scr)
{
   if(scr==0 ||scr>1)
      memset(Retro_Screen, 0, sizeof(Retro_Screen));
   if(scr>0)
      if(screen)
         memset(screen->pixels,0,screen->h*screen->pitch);
}


//autoboot taken from frodo gp32
char kbd_feedbuf[255];
int kbd_feedbuf_pos;
bool autoboot=true;

void kbd_buf_feed(char *s)
{
   strcpy(kbd_feedbuf, s);
   kbd_feedbuf_pos=0;
}

void kbd_buf_update(C64 *TheC64)
{
   if( (kbd_feedbuf[kbd_feedbuf_pos]!=0) && TheC64->RAM[198]==0)
   {
      TheC64->RAM[631]=kbd_feedbuf[kbd_feedbuf_pos];
      TheC64->RAM[198]=1;

      kbd_feedbuf_pos++;
   }
   else if(kbd_feedbuf[kbd_feedbuf_pos]=='\0')
      autoboot=false;
}

//fautoboot

void virtual_kdb(char *buffer,int vx,int vy)
{
   int x, y;

#if defined PITCH && PITCH == 4
   unsigned *pix=(unsigned*)buffer;
#else
   unsigned short *pix=(unsigned short *)buffer;
#endif
   int page      = (NPAGE == -1) ? 0 : 50;
   unsigned coul = RGB565(28, 28, 31);
   BKGCOLOR = (KCOL>0?0xFF404040:0);

   for(x=0;x<NPLGN;x++)
   {
      for(y=0;y<NLIGN;y++)
      {
         DrawBoxBmp((char*)pix,XBASE3+x*XSIDE,YBASE3+y*YSIDE, XSIDE,YSIDE, RGB565(7, 2, 1));
         Draw_text((char*)pix,XBASE0-2+x*XSIDE ,YBASE0+YSIDE*y,coul, BKGCOLOR ,1, 1,20,
               SHIFTON==-1?MVk[(y*NPLGN)+x+page].norml:MVk[(y*NPLGN)+x+page].shift);	
      }
   }

   DrawBoxBmp((char*)pix,XBASE3+vx*XSIDE,YBASE3+vy*YSIDE, XSIDE,YSIDE, RGB565(31, 2, 1));
   Draw_text((char*)pix,XBASE0-2+vx*XSIDE ,YBASE0+YSIDE*vy,RGB565(2,31,1), BKGCOLOR ,1, 1,20,
         SHIFTON==-1?MVk[(vy*NPLGN)+vx+page].norml:MVk[(vy*NPLGN)+vx+page].shift);	

}

int check_vkey2(int x,int y)
{
   //check which key is pressed
   int page= (NPAGE==-1) ? 0 : 50;
   return MVk[y*NPLGN+x+page].val;
}

/*
 *  Open window
 */

int init_graphics(void)
{
	screen         = (retro_Surface*)malloc( sizeof(retro_Surface*) );
	screen->pixels = (unsigned char*)malloc(DISPLAY_X *( DISPLAY_Y + 16) );
	screen->h      = DISPLAY_Y+16;
	screen->w      = DISPLAY_X ;
	screen->pitch  = screen->w*1;
	
	return 1;
}

extern bool quit_requested;

/*
 *  LED error blink
 */

void C64Display::pulse_handler(...)
{
   unsigned i;
   for (i = 0; i < 4; i++)
   {
      switch (c64_disp->led_state[i])
      {
         case LED_ERROR_ON:
            c64_disp->led_state[i] = LED_ERROR_OFF;
            break;
         case LED_ERROR_OFF:
            c64_disp->led_state[i] = LED_ERROR_ON;
            break;
      }
   }
}



/*
 *  Display constructor
 */

C64Display::C64Display(C64 *the_c64) : TheC64(the_c64)
{
   unsigned i;
	quit_requested = false;

	// LEDs off
	for (i = 0; i < 4; i++)
		led_state[i] = old_led_state[i] = LED_OFF;

	// Start timer for LED error blinking
	c64_disp = this;
	libretro_pulse_handler((void (*)(int))C64Display::pulse_handler);
}

/*
 *  Display destructor
 */

C64Display::~C64Display()
{	
	if(screen)
   {
      free(screen->pixels);
      free(screen);
      screen->pixels = NULL;
      screen         = NULL;
   }
}


/*
 *  Prefs may have changed
 */

void C64Display::NewPrefs(Prefs *prefs)
{
}

/*
 *  Redraw bitmap
 */

void C64Display::Update(void)
{
	int x;
   unsigned int *pout = NULL;
   unsigned char *pin = NULL;

   if(ThePrefs.ShowLEDs)
   {
      unsigned i;
      // Draw speedometer/LEDs
      r.x   = 0;
      r.y	= DISPLAY_Y;
      r.w	= DISPLAY_X;
      r.h	= 15;

      retro_FillRect(screen, &r, fill_gray);
      r.w = DISPLAY_X; r.h = 1;
      retro_FillRect(screen, &r, shine_gray);
      r.y = DISPLAY_Y + 14;
      retro_FillRect(screen, &r, shadow_gray);
      r.w = 16;

      for (i = 2; i < 6; i++)
      {
         r.x = DISPLAY_X * i/5 - 24; r.y = DISPLAY_Y + 4;
         retro_FillRect(screen, &r, shadow_gray);
         r.y = DISPLAY_Y + 10;
         retro_FillRect(screen, &r, shine_gray);
      }
      r.y = DISPLAY_Y; r.w = 1; r.h = 15;
      for (i = 0; i < 5; i++)
      {
         r.x = DISPLAY_X * i / 5;
         retro_FillRect(screen, &r, shine_gray);
         r.x = DISPLAY_X * (i+1) / 5 - 1;
         retro_FillRect(screen, &r, shadow_gray);
      }
      r.y = DISPLAY_Y + 4; r.h = 7;
      for (i = 2; i < 6; i++)
      {
         r.x = DISPLAY_X * i/5 - 24;
         retro_FillRect(screen, &r, shadow_gray);
         r.x = DISPLAY_X * i/5 - 9;
         retro_FillRect(screen, &r, shine_gray);
      }
      r.y = DISPLAY_Y + 5; r.w = 14; r.h = 5;
      for (i = 0; i < 4; i++)
      {
         int c;
         r.x = DISPLAY_X * (i+2) / 5 - 23;
         switch (led_state[i])
         {
            case LED_ON:
               c = green;
               break;
            case LED_ERROR_ON:
               c = red;
               break;
            default:
               c = black;
               break;
         }
         retro_FillRect(screen, &r, c);
      }

      draw_string(screen, DISPLAY_X * 1/5 + 8, DISPLAY_Y + 4, "D\x12 8", black, fill_gray);
      draw_string(screen, DISPLAY_X * 2/5 + 8, DISPLAY_Y + 4, "D\x12 9", black, fill_gray);
      draw_string(screen, DISPLAY_X * 3/5 + 8, DISPLAY_Y + 4, "D\x12 10", black, fill_gray);
      draw_string(screen, DISPLAY_X * 4/5 + 8, DISPLAY_Y + 4, "D\x12 11", black, fill_gray);
   }

	// Update display
	//blit c64 scr 1bit depth to emu scr 4bit depth
	pout = (unsigned int *)Retro_Screen+((ThePrefs.ShowLEDs?0:8)*retrow);
	pin  = (unsigned char *)screen->pixels;

	for (x = 0; x < screen->w * screen->h; x++)
		*pout++ = mpal[*pin++];

	if (SHOWKEY==1)
      virtual_kdb(( char *)Retro_Screen,vkx,vky);
}

/* Return pointer to bitmap data */
uint8 *C64Display::BitmapBase(void)
{
	return (uint8 *)screen->pixels;
}

/* Return number of bytes per row */
int C64Display::BitmapXMod(void)
{
	return screen->pitch;
}

/*  Poll the keyboard */
static void translate_key(int key, bool key_up,
      uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
	int c64_key = -1;
	switch (key)
   {
      case RETROK_a: c64_key = MATRIX(1,2); break;
      case RETROK_b: c64_key = MATRIX(3,4); break;
      case RETROK_c: c64_key = MATRIX(2,4); break;
      case RETROK_d: c64_key = MATRIX(2,2); break;
      case RETROK_e: c64_key = MATRIX(1,6); break;
      case RETROK_f: c64_key = MATRIX(2,5); break;
      case RETROK_g: c64_key = MATRIX(3,2); break;
      case RETROK_h: c64_key = MATRIX(3,5); break;
      case RETROK_i: c64_key = MATRIX(4,1); break;
      case RETROK_j: c64_key = MATRIX(4,2); break;
      case RETROK_k: c64_key = MATRIX(4,5); break;
      case RETROK_l: c64_key = MATRIX(5,2); break;
      case RETROK_m: c64_key = MATRIX(4,4); break;
      case RETROK_n: c64_key = MATRIX(4,7); break;
      case RETROK_o: c64_key = MATRIX(4,6); break;
      case RETROK_p: c64_key = MATRIX(5,1); break;
      case RETROK_q: c64_key = MATRIX(7,6); break;
      case RETROK_r: c64_key = MATRIX(2,1); break;
      case RETROK_s: c64_key = MATRIX(1,5); break;
      case RETROK_t: c64_key = MATRIX(2,6); break;
      case RETROK_u: c64_key = MATRIX(3,6); break;
      case RETROK_v: c64_key = MATRIX(3,7); break;
      case RETROK_w: c64_key = MATRIX(1,1); break;
      case RETROK_x: c64_key = MATRIX(2,7); break;
      case RETROK_y: c64_key = MATRIX(3,1); break;
      case RETROK_z: c64_key = MATRIX(1,4); break;

      case RETROK_0: c64_key = MATRIX(4,3); break;
      case RETROK_1: c64_key = MATRIX(7,0); break;
      case RETROK_2: c64_key = MATRIX(7,3); break;
      case RETROK_3: c64_key = MATRIX(1,0); break;
      case RETROK_4: c64_key = MATRIX(1,3); break;
      case RETROK_5: c64_key = MATRIX(2,0); break;
      case RETROK_6: c64_key = MATRIX(2,3); break;
      case RETROK_7: c64_key = MATRIX(3,0); break;
      case RETROK_8: c64_key = MATRIX(3,3); break;
      case RETROK_9: c64_key = MATRIX(4,0); break;

      case RETROK_SPACE: c64_key = MATRIX(7,4); break;
      case RETROK_BACKQUOTE: c64_key = MATRIX(7,1); break;
      case RETROK_BACKSLASH: c64_key = MATRIX(6,6); break;
      case RETROK_COMMA: c64_key = MATRIX(5,7); break;
      case RETROK_PERIOD: c64_key = MATRIX(5,4); break;
      case RETROK_MINUS: c64_key = MATRIX(5,0); break;
      case RETROK_EQUALS: c64_key = MATRIX(5,3); break;
      case RETROK_LEFTBRACKET: c64_key = MATRIX(5,6); break;
      case RETROK_RIGHTBRACKET: c64_key = MATRIX(6,1); break;
      case RETROK_SEMICOLON: c64_key = MATRIX(5,5); break;
      case RETROK_QUOTE: c64_key = MATRIX(6,2); break;
      case RETROK_SLASH: c64_key = MATRIX(6,7); break;

      case RETROK_ESCAPE: c64_key = MATRIX(7,7); break;
      case RETROK_RETURN: c64_key = MATRIX(0,1); break;
      case RETROK_BACKSPACE: case RETROK_DELETE: c64_key = MATRIX(0,0); break;
      case RETROK_INSERT: c64_key = MATRIX(6,3); break;
      case RETROK_HOME: c64_key = MATRIX(6,3); break;
      case RETROK_END: c64_key = MATRIX(6,0); break;
      case RETROK_PAGEUP: c64_key = MATRIX(6,0); break;
      case RETROK_PAGEDOWN: c64_key = MATRIX(6,5); break;

      case RETROK_LCTRL: case RETROK_TAB: c64_key = MATRIX(7,2); break;
      case RETROK_RCTRL: c64_key = MATRIX(7,5); break;
      case RETROK_LSHIFT: c64_key = MATRIX(1,7); break;
      case RETROK_RSHIFT: c64_key = MATRIX(6,4); break;
      case RETROK_LALT: case RETROK_LMETA: c64_key = MATRIX(7,5); break;
      case RETROK_RALT: case RETROK_RMETA: c64_key = MATRIX(7,5); break;

      case RETROK_UP: c64_key = MATRIX(0,7)| 0x80; break;
      case RETROK_DOWN: c64_key = MATRIX(0,7); break;
      case RETROK_LEFT: c64_key = MATRIX(0,2) | 0x80; break;
      case RETROK_RIGHT: c64_key = MATRIX(0,2); break;

      case RETROK_F1: c64_key = MATRIX(0,4); break;
      case RETROK_F2: c64_key = MATRIX(0,4) | 0x80; break;
      case RETROK_F3: c64_key = MATRIX(0,5); break;
      case RETROK_F4: c64_key = MATRIX(0,5) | 0x80; break;
      case RETROK_F5: c64_key = MATRIX(0,6); break;
      case RETROK_F6: c64_key = MATRIX(0,6) | 0x80; break;
      case RETROK_F7: c64_key = MATRIX(0,3); break;
      case RETROK_F8: c64_key = MATRIX(0,3) | 0x80; break;

      case RETROK_KP0: case RETROK_KP5: c64_key = 0x10 | 0x40; break;
      case RETROK_KP1: c64_key = 0x06 | 0x40; break;
      case RETROK_KP2: c64_key = 0x02 | 0x40; break;
      case RETROK_KP3: c64_key = 0x0a | 0x40; break;
      case RETROK_KP4: c64_key = 0x04 | 0x40; break;
      case RETROK_KP6: c64_key = 0x08 | 0x40; break;
      case RETROK_KP7: c64_key = 0x05 | 0x40; break;
      case RETROK_KP8: c64_key = 0x01 | 0x40; break;
      case RETROK_KP9: c64_key = 0x09 | 0x40; break;

      case RETROK_KP_DIVIDE: c64_key = MATRIX(6,7); break;
      case RETROK_KP_ENTER: c64_key = MATRIX(0,1); break;
   }

	if (c64_key < 0)
		return;

	// Handle joystick emulation
	if (c64_key & 0x40)
   {
      c64_key              &= 0x1f;
      if (joystick)
      {
         if (key_up)
            *joystick         |= c64_key;
         else
            *joystick         &= ~c64_key;
      }
      return;
   }

	// Handle other keys
	bool shifted             = c64_key & 0x80;
	int c64_byte             = (c64_key >> 3) & 7;
	int c64_bit              = c64_key & 7;
	if (key_up)
   {
      if (shifted)
      {
         if (key_matrix)
            key_matrix[6]     |= 0x10;
         if (rev_matrix)
            rev_matrix[4]     |= 0x40;
      }
      if (key_matrix)
         key_matrix[c64_byte] |= (1 << c64_bit);
      if (rev_matrix)
         rev_matrix[c64_bit]  |= (1 << c64_byte);
   }
   else
   {
      if (shifted)
      {
         if (key_matrix)
            key_matrix[6]     &= 0xef;
         if (rev_matrix)
            rev_matrix[4]     &= 0xbf;
      }
      if (key_matrix)
         key_matrix[c64_byte] &= ~(1 << c64_bit);
      if (rev_matrix)
         rev_matrix[c64_bit]  &= ~(1 << c64_byte);
   }
}

static void validkey(int c64_key,int key_up,uint8 *key_matrix,
      uint8 *rev_matrix, uint8 *joystick)
{
	// Handle other keys
	bool shifted             = c64_key & 0x80;
	int c64_byte             = (c64_key >> 3) & 7;
	int c64_bit              = c64_key & 7;
	if (key_up)
   {
      if (shifted)
      {
         if (key_matrix)
            key_matrix[6]     |= 0x10;
         if (rev_matrix)
            rev_matrix[4]     |= 0x40;
      }
      if (key_matrix)
         key_matrix[c64_byte] |= (1 << c64_bit);
      if (rev_matrix)
         rev_matrix[c64_bit]  |= (1 << c64_byte);
   }
   else
   {
		if (shifted)
      {
         if (key_matrix)
            key_matrix[6]     &= 0xef;
         if (rev_matrix)
            rev_matrix[4]     &= 0xbf;
      }
      if (key_matrix)
         key_matrix[c64_byte] &= ~(1 << c64_bit);
      if (rev_matrix)
         rev_matrix[c64_bit]  &= ~(1 << c64_byte);
	}
}


void  C64Display::Keymap_KeyUp(int symkey,uint8 *key_matrix,
      uint8 *rev_matrix, uint8 *joystick)
{
	if (symkey == RETROK_NUMLOCK)
		num_locked = false;
	else
      translate_key(symkey, true, key_matrix, rev_matrix, joystick);			
}

void C64Display:: Keymap_KeyDown(int symkey,uint8 *key_matrix,
      uint8 *rev_matrix, uint8 *joystick)
{
   switch (symkey)
   {
      case RETROK_F9:	// F9: Invoke SAM
#ifdef HAVE_SAM
         SAM(TheC64);
#else
         pauseg = 1;
#endif
         break;
      case RETROK_F10:	// F10: Quit
         quit_requested = true;
         break;
      case RETROK_F11:	// F11: NMI (Restore)
         TheC64->NMI();
         break;
      case RETROK_F12:	// F12: Reset
         TheC64->Reset();
         break;
      case RETROK_NUMLOCK:
         num_locked = true;
         break;
      case RETROK_KP_PLUS:	// '+' on keypad: Increase SkipFrames
         ThePrefs.SkipFrames++;
         break;
      case RETROK_KP_MINUS:	// '-' on keypad: Decrease SkipFrames
         if (ThePrefs.SkipFrames > 1)
            ThePrefs.SkipFrames--;
         break;
      case RETROK_KP_MULTIPLY:	// '*' on keypad: Toggle speed limiter
         ThePrefs.LimitSpeed = !ThePrefs.LimitSpeed;
         break;
      case RETROK_KP_DIVIDE:	// '/' on keypad: Toggle GUI 
         pauseg=1;
         break;

      default:
         translate_key(symkey, false, key_matrix, rev_matrix, joystick);
         break;
   }
}

void C64Display::PollKeyboard(uint8 *key_matrix, uint8 *rev_matrix,
      uint8 *joystick)
{
   // VKBD
   int i;
   //   RETRO        B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
   //   INDEX        0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
   static int oldi=-1;

   if (autoboot)
      kbd_buf_update(TheC64);

   Retro_PollEvent(key_matrix,rev_matrix,joystick);

   if (oldi!=-1)
   {
      // IKBD_PressSTKey(oldi,0);
      validkey(oldi,1,key_matrix,rev_matrix,joystick);
      oldi=-1;
   }

   if(SHOWKEY==1)
   {
      static int vkflag[5]={0,0,0,0,0};		

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) && vkflag[0]==0 )
         vkflag[0]=1;
      else if (vkflag[0]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) )
      {
         vkflag[0]=0;
         vky -= 1; 
      }

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) && vkflag[1]==0 )
         vkflag[1]=1;
      else if (vkflag[1]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) )
      {
         vkflag[1]=0;
         vky += 1; 
      }

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) && vkflag[2]==0 )
         vkflag[2]=1;
      else if (vkflag[2]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) )
      {
         vkflag[2]=0;
         vkx -= 1;
      }

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) && vkflag[3]==0 )
         vkflag[3]=1;
      else if (vkflag[3]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) )
      {
         vkflag[3]=0;
         vkx += 1;
      }

      if(vkx < 0)
         vkx = 9;
      if(vkx > 9)
         vkx = 0;

      if(vky < 0)
         vky = 4;
      if(vky > 4)
         vky = 0;

      i=8;
      if(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i)  && vkflag[4]==0) 	
         vkflag[4]=1;
      else if( !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i)  && vkflag[4]==1)
      {
         vkflag[4]=0;
         i=check_vkey2(vkx,vky);

         if(i==-1)
            oldi=-1;
         if(i==-2)
         {
            NPAGE=-NPAGE;oldi=-1;
            //Clear interface zone					
            //Screen_SetFullUpdate();

         }
         else if(i==-3)
         {
            //KDB bgcolor
            //Screen_SetFullUpdate();
            KCOL=-KCOL;
            oldi=-1;
         }
         else if(i==-4)
         {
            //VKbd show/hide 			
            oldi=-1;
            Screen_SetFullUpdate(0);
            SHOWKEY=-SHOWKEY;
         }
         else if(i==-5)
         {
            //Change Joy number
            //NUMjoy=-NUMjoy;
            oldi=-1;
         }
         else
         {
            if(i==-10) //SHIFT
            {
               validkey(MATRIX(6,4),(SHIFTON == 1)?1:0,key_matrix,rev_matrix,joystick);
               SHIFTON=-SHIFTON;
               //Screen_SetFullUpdate();

               oldi=-1;
            }
            else if(i==-11) //CTRL
            {               
               validkey(MATRIX(7,2),(CTRLON == 1)?1:0,key_matrix,rev_matrix,joystick);
               CTRLON=-CTRLON;
               //Screen_SetFullUpdate();

               oldi=-1;
            }
            else if(i==-12) //RSTOP
            {               
               validkey(MATRIX(7,7),(RSTOPON == 1)?1:0,key_matrix,rev_matrix,joystick);
               RSTOPON=-RSTOPON;
               //Screen_SetFullUpdate();

               oldi=-1;
            }
            else if(i==-13) //AUTOBOOT
            {     
               kbd_buf_feed((char*)"\rLOAD\":*\",8,1:\rRUN\r\0");
               autoboot=true; 
               oldi=-1;
            }
            else if(i==-14) //GUI
            {    
               pauseg=1; 
               Screen_SetFullUpdate(0);
               SHOWKEY=-SHOWKEY;
               oldi=-1;
            }
            else
            {
               oldi=i;
               validkey(oldi,0,key_matrix,rev_matrix,joystick);               
            }
         }
      }
   }
}

/*
 *  Check if NumLock is down (for switching the joystick keyboard emulation)
 */
bool C64Display::NumLock(void)
{
	return num_locked;
}

/*
 *  Allocate C64 colors
 */

void C64Display::InitColors(uint8 *colors)
{
   int i;
	for (i=0; i<16; i++)
   {
      palette[i].r        = palette_red[i];
      palette[i].g        = palette_green[i];
      palette[i].b        = palette_blue[i];
      mpal[i]             = palette[i].r<<16|palette[i].g<<8|palette[i].b;
   }
	mpal[fill_gray]        = 0xd0d0d0;
	mpal[shine_gray]       = 0xf0f0f0;
	mpal[shadow_gray]      = 0x808080;
	mpal[red]              = 0xff0000;
	mpal[green]            = 0x00ff00;

	palette[fill_gray].r   = 0xd0;
   palette[fill_gray].g   = 0xd0;
   palette[fill_gray].b   = 0xd0;
	palette[shine_gray].r  = 0xf0;
   palette[shine_gray].g  = 0xf0;
   palette[shine_gray].b  = 0xf0;
	palette[shadow_gray].r = 0x80;
   palette[shadow_gray].g = 0x80;
   palette[shadow_gray].b = 0x80;
	palette[red].r         = 0xf0;
	palette[red].g         = 0;
   palette[red].b         = 0;
	palette[green].r       = 0;
	palette[green].g       = 0xf0;
   palette[green].b       = 0;

	for (i = 0; i < 256; i++)
		colors[i] = i & 0x0f;
}
