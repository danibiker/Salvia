/*
	modded for libretro-frodo
*/

/*
  Hatari - dlgJoystick.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#include "dialog.h"
#include "sdlgui.h"

#define DLGJOY_MSJY  3
#define DLGJOY_JYJY  4  
#define DLGJOY_MSMS  5
#define DLGJOY_EXIT  6

/* The joysticks dialog: */

static char sSdlStickName[20];

static SGOBJ joydlg[] =
{
	{ SGBOX, 0, 0, 0,0, 32,18, NULL },
	{ SGTEXT, 0, 0, 8,1, 15,1, (char*)"Joysticks setup" },

	{ SGBOX, 0, 0, 1,4, 30,11, NULL },
	{ SGCHECKBOX, 0, 0, 2,5, 10,1, (char*)"Enable Joy1" },
	{ SGCHECKBOX, 0, 0, 2,6, 20,1, (char*)"Enable Joy2" },
	{ SGCHECKBOX, 0, 0, 2,7, 14,1, (char*)"Swap Joy" },

	{ SGBUTTON, SG_EXIT/*SG_DEFAULT*/, 0, 6,16, 24,1, (char*)"Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};

/*-----------------------------------------------------------------------*/
/**
 * Show and process the joystick dialog.
 */
void Dialog_JoyDlg(void)
{
   int but;

   SDLGui_CenterDlg(joydlg);

   joydlg[DLGJOY_MSJY].state &= ~SG_SELECTED;
   joydlg[DLGJOY_JYJY].state &= ~SG_SELECTED;
   joydlg[DLGJOY_MSMS].state &= ~SG_SELECTED;

   if (ThePrefs.Joystick1Port) /* joy-1 */
      joydlg[DLGJOY_MSJY].state |= SG_SELECTED;
   if (ThePrefs.Joystick2Port) /* joy-2 */
      joydlg[DLGJOY_JYJY].state |= SG_SELECTED;
   if ( ThePrefs.JoystickSwap) /* swap */
      joydlg[DLGJOY_MSMS].state |= SG_SELECTED;

   do
   {
      but = SDLGui_DoDialog(joydlg, NULL);
      gui_poll_events();
   }
   while (but != DLGJOY_EXIT && but != SDLGUI_QUIT
         && but != SDLGUI_ERROR && !bQuitProgram);


   if(joydlg[DLGJOY_MSJY].state & SG_SELECTED)
   {
      if(!ThePrefs.Joystick1Port)
         prefs->	Joystick1Port =1;
   }
   else if(ThePrefs.Joystick1Port)
      prefs->	Joystick1Port =0;

   if(joydlg[DLGJOY_JYJY].state & SG_SELECTED)
   {
      if(!ThePrefs.Joystick2Port)
         prefs->	Joystick2Port =1;
   }
   else if(ThePrefs.Joystick2Port)
      prefs->	Joystick2Port =0;

   if(joydlg[DLGJOY_MSMS].state & SG_SELECTED)
   {
      if(!ThePrefs.JoystickSwap)
         prefs->	JoystickSwap = true;
   }
   else if(ThePrefs.JoystickSwap)
      prefs->	JoystickSwap =false;
}
