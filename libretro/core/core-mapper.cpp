#include <libretro.h>
#include "libretro-core.h"
#include "retroscreen.h"

#ifdef __CELLOS_LV2__
#include "sys/sys_time.h"
#include "sys/timer.h"
#else
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#endif

#include "main.h"
#include "C64.h"
#include "Display.h"
#include "Prefs.h"

/* forward declarations */
extern C64 *TheC64;

extern void Screen_SetFullUpdate(int scr);
extern bool Dialog_DoProperty(void);
#ifdef NO_LIBCO
extern void retro_run_gui(void);
#endif

//VIDEO
#ifdef  RENDER16B
uint16_t Retro_Screen[1024*1024];
#else
unsigned int Retro_Screen[1024*1024];
#endif 

//SOUND
short signed int SNDBUF[1024*2];
int snd_sampler = 44100 / 50;

//PATH
char RPATH[512];

//EMU FLAGS
int NPAGE=-1, KCOL=1, BKGCOLOR=0;
int SHOWKEY=-1;

int SHIFTON=-1,MOUSE_EMULATED=-1,PAS=4;
int SND=1; /* SOUND ON/OFF */
int pauseg=0; // enter_gui
int touch=-1; // GUI mouse button
//JOY
int al[2][2]; /* Left  analog Stick 1 */
int ar[2][2]; /* Right analog Stick 1 */
unsigned char MXjoy[2]; /* joy */
int NUMjoy=1;
int slowdown = 0;

//MOUSE
int gmx, gmy;      /* GUI mouse coordinates */

//KEYBOARD
char Key_Sate[512];
char Key_Sate2[512];

static int mbt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//STATS GUI
int BOXDEC = 32+2;
int STAT_BASEY;

retro_input_state_t input_state_cb;
static retro_input_poll_t input_poll_cb;

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

/* in milliseconds */
static long GetTicks(void)
{
#ifndef _ANDROID_
#ifdef __CELLOS_LV2__

   //#warning "GetTick PS3\n"

   unsigned long	ticks_micro;
   uint64_t secs;
   uint64_t nsecs;

   sys_time_get_current_time(&secs, &nsecs);
   ticks_micro =  secs * 1000000UL + (nsecs / 1000);

   return ticks_micro/1000;
#else
   struct timeval tv;
   gettimeofday (&tv, NULL);
   return (tv.tv_sec*1000000 + tv.tv_usec)/1000;
#endif

#else

   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (now.tv_sec*1000000 + now.tv_nsec/1000)/1000;
#endif
} 

//NO SURE FIND BETTER WAY TO COME BACK IN MAIN THREAD IN HATARI GUI
void gui_poll_events(void)
{
   static unsigned long Ktime       = 0;
   static unsigned long LastFPSTime = 0;
   Ktime                            = GetTicks();

   if(Ktime - LastFPSTime >= 1000/50)
   {
      slowdown    = 0;
      LastFPSTime = Ktime;
#ifndef NO_LIBCO		
      co_switch(mainThread);
#else
      //FIXME nolibco Gui endless loop -> no retro_run() call
      retro_run_gui();
#endif
   }
}

void enter_gui(void)
{
   Dialog_DoProperty();
   pauseg=0;
}

void pause_select(void)
{
   static int firstps = 0;
   if (pauseg == 1 && firstps == 0)
   {
      firstps = 1;
      enter_gui();
      firstps = 0;
   }
}

void texture_uninit(void)
{
}

void texture_init(void)
{
   memset(Retro_Screen, 0, sizeof(Retro_Screen));

   gmx = (retrow/2)-1;
   gmy = (retroh/2)-1;
}


void Process_key(uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
   int i;

   for(i = 0; i < 320; i++)
   {
      Key_Sate[i] = input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, i) 
	      ? 0x80: 0;

      if(Key_Sate[i] && Key_Sate2[i] == 0)
      {
         if(i == RETROK_RALT) /* Modifier pressed? */
         { 
            Key_Sate2[i]=1;
            continue;
         }

         TheC64->TheDisplay->Keymap_KeyDown(
               i,key_matrix,rev_matrix,joystick);

         /* Key down */
         Key_Sate2[i] = 1;
      }
      else if ( !Key_Sate[i] && Key_Sate2[i]==1 )
      {
         if(i == RETROK_RALT) /* Modifier pressed? */
         {
            Key_Sate2[i]=0;
            continue;
         }

         TheC64->TheDisplay->Keymap_KeyUp(
               i,key_matrix,rev_matrix,joystick);

         /* Key up */
         Key_Sate2[i] = 0;
      }
   }
}

int Retro_PollEvent(uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
   //   RETRO        B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
   //   INDEX        0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
   //   C64          BOOT VKB  M/J  R/S  UP   DWN  LEFT RGT  B1   GUI  F7   F1   F5   F3   SPC  1 
   int mouse_l;
   int mouse_r;
   int i;
   static int mmbL = 0;
   static int mmbR = 0;
   int SAVPAS      = PAS;	
   int16_t mouse_x = 0;
   int16_t mouse_y = 0;

   input_poll_cb();

   if (SHOWKEY == -1 && pauseg == 0)
      Process_key(key_matrix,rev_matrix,joystick);

   if (pauseg==0)
   {
      /*
         i=0;//Autoboot
         if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
         mbt[i]=1;
         else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
         {
         mbt[i]=0;
         kbd_buf_feed("\rLOAD\":*\",8,1:\rRUN\r\0");
         autoboot=true; 
         }
       */

      i = 1; // show vkbd toggle

      if (          mbt[i] == 0
                && input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
         mbt[i]  = 1;
      else if (     mbt[i] == 1 
                && !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
      {
         mbt[i]  = 0;
         SHOWKEY = -SHOWKEY;

         Screen_SetFullUpdate(0);  
      }
   }

   i = 2;//mouse/joy toggle
 
   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
      mbt[i]         = 1;
   else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
   {
      mbt[i]         = 0;
      MOUSE_EMULATED = -MOUSE_EMULATED;
   }

   if (MOUSE_EMULATED == 1)
   {
      if (slowdown > 0)
         return 1;

      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_RIGHT))
         mouse_x += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_LEFT))
         mouse_x -= PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_DOWN))
         mouse_y += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_UP))
         mouse_y -= PAS;
      mouse_l    = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_A);
      mouse_r    = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_B);

      PAS=SAVPAS;

      slowdown=1;
   }
   else
   {
      mouse_x    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0,
            RETRO_DEVICE_ID_MOUSE_X);
      mouse_y    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0,
            RETRO_DEVICE_ID_MOUSE_Y);
      mouse_l    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0,
            RETRO_DEVICE_ID_MOUSE_LEFT);
      mouse_r    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0,
            RETRO_DEVICE_ID_MOUSE_RIGHT);
   }

   if (mmbL == 0 && mouse_l)
   {
      mmbL  = 1;
      touch = 1;
   }
   else if (mmbL == 1 && !mouse_l)
   {
      mmbL  = 0;
      touch = -1;
   }

   if (mmbR == 0 && mouse_r)
      mmbR = 1;		
   else if (mmbR == 1 && !mouse_r)
      mmbR = 0;

   gmx += mouse_x;
   gmy += mouse_y;
   if(gmx < 0)
      gmx = 0;
   if(gmx > retrow-1)
      gmx = retrow-1;
   if(gmy < 0)
      gmy = 0;
   if(gmy > retroh-1)
      gmy = retroh-1;

   return 1;
}
