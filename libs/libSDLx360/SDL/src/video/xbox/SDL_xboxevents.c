/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id: SDL_xboxevents.c,v 1.1 2003/07/18 15:19:33 lantus Exp $";
#endif

/* Being a null driver, there's no event stream. We just define stubs for
   most of the API. */

#ifndef _XBOX_DONT_USE_DEVICES
#define DEBUG_KEYBOARD
#define DEBUG_MOUSE
#include <xtl.h>
#endif



#include "SDL.h"
#include "SDL_sysevents.h"
#include "SDL_events_c.h"
#include "SDL_xboxvideo.h"
#include "SDL_xboxevents_c.h"


#define VK_bracketleft	0xdb
#define VK_bracketright	0xdd
#define VK_comma	0xbc
#define VK_period	0xbe
#define VK_slash	0xbf
#define VK_semicolon	0xba
#define VK_grave	0xc0
#define VK_minus	0xbd
#define VK_equal	0xbb
#define VK_quote	0xde
#define VK_backslash	0xdc


static HANDLE g_hKeyboardDevice[4] = { 0 };
static HANDLE g_hMouseDevice[4] = { 0 };

BOOL                     g_bDevicesInitialized  = FALSE;
 
/* The translation table from a Xbox KB scancode to an SDL keysym */
 
 
void XBOX_PumpEvents(_THIS)
{
 
}

void XBOX_InitOSKeymap(_THIS)
{
 
}

CHAR XBInput_GetKeyboardInput()
{
 
}


static void keyboard_update(void)
{
 
}


static void mouse_update(void)
{
 
}

VOID Mouse_RefreshDeviceList()
{
 
}

 
 