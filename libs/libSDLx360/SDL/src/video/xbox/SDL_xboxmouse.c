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
 "@(#) $Id: SDL_xboxmouse.c,v 1.1 2003/07/18 15:19:33 lantus Exp $";
#endif

#define DEBUG_MOUSE
#include <xtl.h>
#include <stdio.h>

#include "SDL_error.h"
#include "SDL_mouse.h"
#include "SDL_events_c.h"

#include "SDL_XBOXmouse_c.h"


/* The implementation dependent data for the window manager cursor */
struct WMcursor {
	int unused;
};

void stub(void)
{
	// do something
}

/* Drena el raton USB HID nativo (implementado en SDL_xboxhidmouse.cpp) y
 * despacha los eventos a SDL. Llamada desde mouse_update() en
 * SDL_xboxevents.c. Se define siempre; su cuerpo solo actua cuando
 * SDL_XBOX_HIDMOUSE esta activo (builds Xbox 360 con el toggle habilitado). */
void XBOX_MouseUpdateHID(void)
{
#ifdef SDL_XBOX_HIDMOUSE
	static Uint8 prev_buttons = 0;
	/* Boot-mouse -> boton SDL: bit0=izq(1), bit2=medio(2), bit1=der(3). */
	static const unsigned char sdl_btn_mask[3] = { 0x01, 0x04, 0x02 };
	int dx = 0, dy = 0, dwheel = 0;
	unsigned int buttons = 0;
	Uint8 changed;
	int i;

	XBOX_HIDMouse_Drain(&dx, &dy, &dwheel, &buttons);

	if (dx || dy)
		SDL_PrivateMouseMotion(0 /*buttonstate*/, 1 /*relative*/, (Sint16)dx, (Sint16)dy);

	changed = (Uint8)(buttons ^ prev_buttons);
	for (i = 0; i < 3; ++i) {
		if (changed & sdl_btn_mask[i])
			SDL_PrivateMouseButton(
				(buttons & sdl_btn_mask[i]) ? SDL_PRESSED : SDL_RELEASED,
				(Uint8)(i + 1), 0, 0);
	}
	prev_buttons = (Uint8)buttons;

	/* Rueda: un pulso press+release por muesca (convencion SDL 1.2). */
	while (dwheel > 0) {
		SDL_PrivateMouseButton(SDL_PRESSED,  SDL_BUTTON_WHEELUP, 0, 0);
		SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_WHEELUP, 0, 0);
		--dwheel;
	}
	while (dwheel < 0) {
		SDL_PrivateMouseButton(SDL_PRESSED,  SDL_BUTTON_WHEELDOWN, 0, 0);
		SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_WHEELDOWN, 0, 0);
		++dwheel;
	}
#endif
}