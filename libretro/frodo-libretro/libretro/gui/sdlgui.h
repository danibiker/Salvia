/*
	modded for libretro-frodo
*/

/*
  Hatari - sdlgui.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Header for the tiny graphical user interface for Hatari.
*/
//
#ifndef HATARI_SDLGUI_H
#define HATARI_SDLGUI_H

#include <boolean.h>

#include "sysdeps.h"

#define SNAP_BMP 1

#include "main.h"
#include "C64.h"
#include "Display.h"
#include "Prefs.h"

extern C64 *TheC64;
extern Prefs ThePrefs,*prefs;

extern void gui_poll_events(void);

enum
{
  SGBOX,
  SGTEXT,
  SGEDITFIELD,
  SGBUTTON,
  SGRADIOBUT,
  SGCHECKBOX,
  SGPOPUP,
  SGSCROLLBAR
};


/* Object flags: */
#define SG_TOUCHEXIT   1   /* Exit immediately when mouse button is pressed down */
#define SG_EXIT        2   /* Exit when mouse button has been pressed (and released) */
#define SG_DEFAULT     4   /* Marks a default button, selectable with return key */
#define SG_CANCEL      8   /* Marks a cancel button, selectable with ESC key */

/* Object states: */
#define SG_SELECTED    1
#define SG_MOUSEDOWN   16
#define SG_MOUSEUP     (((int)-1) - SG_MOUSEDOWN)

#define SGRADIOBUTTON_NORMAL '.'//12
#define SGRADIOBUTTON_SELECTED '*'//13
#define SGCHECKBOX_NORMAL 'o'//14
#define SGCHECKBOX_SELECTED 'X'//15
#define SGARROWUP "^"// 1
#define SGARROWDOWN "v"// 2
#define SGARROWLEFT "<"// 4
#define SGARROWRIGHT ">"// 3
#define SGFOLDER     '~'

/* Return codes: */
#define SDLGUI_ERROR         -1
#define SDLGUI_QUIT          -2
#define SDLGUI_UNKNOWNEVENT  -3

typedef struct
{
  int type;             /* What type of object */
  int flags;            /* Object flags */
  int state;            /* Object state */
  int x, y;             /* The offset to the upper left corner */
  int w, h;             /* Width and height (for scrollbar : height and position) */
  char *txt;            /* Text string */
}  SGOBJ;

extern int sdlgui_fontwidth;	/* Width of the actual font */
extern int sdlgui_fontheight;	/* Height of the actual font */

extern int SDLGui_Init(void);
extern int SDLGui_UnInit(void);
extern int SDLGui_SetScreen();
extern int SDLGui_DoDialog(SGOBJ *dlg, int *pEventOut);
extern void SDLGui_CenterDlg(SGOBJ *dlg);
extern char* SDLGui_FileSelect(const char *path_and_name, char **zip_path, bool bAllowNew);

#endif
