
#include "libretro.h"

#include "retroscreen.h"
#include "libretro-core.h"
#include "vkbd_def.h"
#include "graph.h"

extern int NPAGE;
extern int KCOL;
extern int BKGCOLOR;
extern int SHIFTON;
extern int CTRLON;
extern int retrow, retroh;

/* Compact 12x6 keyboard, centered horizontally and anchored to the bottom of
   the screen so it takes as little space as possible. Labels are <=3 chars
   (~21px), so 28px-wide keys fit them. Local geometry replaces the old
   XSIDE/YSIDE/XBASE YBASE macros (which made a big, upper-centered board). */
#define VK_KW 28   /* column pitch / key width  */
#define VK_KH 13   /* row pitch / key height    */

void virtual_kdb(char *buffer,int vx,int vy)
{

   int x, y, page;
   unsigned coul;
   int x0 = (retrow - NPLGN * VK_KW) / 2;   /* center horizontally */
   int y0 = retroh - NLIGN * VK_KH - 3;     /* anchor to the bottom */

#if defined PITCH && PITCH == 4
unsigned *pix=(unsigned*)buffer;
#else
unsigned short *pix=(unsigned short *)buffer;
#endif

   page = (NPAGE == -1) ? 0 : NLIGN*NPLGN;
   coul = RGB565(28, 28, 31);
   BKGCOLOR = (KCOL>0?0xFF808080:0);


   for(x=0;x<NPLGN;x++)
   {
      for(y=0;y<NLIGN;y++)
      {
         DrawBoxBmp((char*)pix, x0 + x*VK_KW, y0 + y*VK_KH, VK_KW-1, VK_KH-1, RGB565(7, 2, 1));
		 if (SHIFTON==1)
		 {
			Draw_text((char*)pix, x0 + x*VK_KW + 2, y0 + y*VK_KH + 3, coul, BKGCOLOR ,1, 1,3,MVk[(y*NPLGN)+x+page].shift);
		 }
		 else if (CTRLON==1)
		 {
			Draw_text((char*)pix, x0 + x*VK_KW + 2, y0 + y*VK_KH + 3, coul, BKGCOLOR ,1, 1,3,MVk[(y*NPLGN)+x+page].ctrl);
		 }
		 else
		 {
			Draw_text((char*)pix, x0 + x*VK_KW + 2, y0 + y*VK_KH + 3, coul, BKGCOLOR ,1, 1,3,MVk[(y*NPLGN)+x+page].norml);
		 }
      }
   }

   // draw Shift and Control keys status
   // Shift - position 0,4
   if (SHIFTON==1)
   {
	   Draw_text((char*)pix, x0 + 0*VK_KW + 2, y0 + 4*VK_KH + 3, RGB565(2,2,31), BKGCOLOR ,1, 1,3,MVk[(4*NPLGN)+0+page].shift);
   }
   // Control - position 0,3
   if (CTRLON==1)
   {
	   Draw_text((char*)pix, x0 + 0*VK_KW + 2, y0 + 3*VK_KH + 3, RGB565(2,2,31), BKGCOLOR ,1, 1,3,MVk[(3*NPLGN)+0+page].ctrl);
   }

   DrawBoxBmp((char*)pix, x0 + vx*VK_KW, y0 + vy*VK_KH, VK_KW-1, VK_KH-1, RGB565(31, 2, 1));
	if (SHIFTON==1)
	{
		Draw_text((char*)pix, x0 + vx*VK_KW + 2, y0 + vy*VK_KH + 3, RGB565(2,31,1), BKGCOLOR ,1, 1,3,MVk[(vy*NPLGN)+vx+page].shift);
	}
	else if (CTRLON==1)
	{
		Draw_text((char*)pix, x0 + vx*VK_KW + 2, y0 + vy*VK_KH + 3, RGB565(2,31,1), BKGCOLOR ,1, 1,3,MVk[(vy*NPLGN)+vx+page].ctrl);
	}
	else
	{
		Draw_text((char*)pix, x0 + vx*VK_KW + 2, y0 + vy*VK_KH + 3, RGB565(2,31,1), BKGCOLOR ,1, 1,3,MVk[(vy*NPLGN)+vx+page].norml);
	}
	if (vx==0 && vy==4 && SHIFTON==1) // diferent Shift color if Shift is ON - position 0,4
	{
		Draw_text((char*)pix, x0 + vx*VK_KW + 2, y0 + vy*VK_KH + 3, RGB565(2,31,21), BKGCOLOR ,1, 1,3,MVk[(vy*NPLGN)+vx+page].shift);
	}
	if (vx==0 && vy==3 && CTRLON==1) // diferent Conrol color if Control is ON - position 0,3
	{
		Draw_text((char*)pix, x0 + vx*VK_KW + 2, y0 + vy*VK_KH + 3, RGB565(2,31,21), BKGCOLOR ,1, 1,3,MVk[(vy*NPLGN)+vx+page].ctrl);
	}

}

int check_vkey2(int x,int y)
{
   int page;
   //check which key is press
   page= (NPAGE==-1) ? 0 : 5*NPLGN;
   return MVk[y*NPLGN+x+page].val;
}

