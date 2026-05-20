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

/* USB keyboard backend for Xbox 360 SDL.
 *
 * Antes este fichero era un stub vacío: keyboard_update() no hacía nada y
 * por eso, en Xbox 360, ningún teclado USB generaba eventos SDL_KEYDOWN/
 * KEYUP. Eso impedía que Salvia recibiera teclas y, en consecuencia, que
 * cores libretro como DOSBox-Pure recibieran nada del usuario.
 *
 * Esta implementación es un port del backend que sí funciona en proyectos
 * como XeDOSBox y SDLx360 (Wolf3s): se polea el teclado vía
 * XInputGetKeystroke con XINPUT_FLAG_KEYBOARD, se traduce el
 * VirtualKey de Windows (VK_*) a SDLK_* con DIK_keymap[256], y se
 * dispatcha vía SDL_PrivateKeyboard. Los modifiers (Shift/Ctrl/Alt) se
 * remapean desde el HidCode porque XInput devuelve códigos genéricos para
 * la versión izquierda/derecha; sin el remap perdíamos la distinción.
 */

#ifndef _XBOX_DONT_USE_DEVICES
#define DEBUG_KEYBOARD
#define DEBUG_MOUSE
#include <xtl.h>
/* Xbox Debug Manager: provee DmGetMouseChanges para leer ratón USB en
 * builds JTAG/RGH/Devkit. En retail Xbox Live no existe, así que el
 * build solo enlaza esta lib si el target es homebrew. Si tu setup no
 * la tiene, no se define SDL_XBOX_MOUSE en las opciones del proyecto y la
 * función mouse_update() quedará no-op. */
#ifdef SDL_XBOX_MOUSE
#include <xbdm.h>
#pragma comment(lib, "xbdm.lib")
#endif
#endif



#include "SDL.h"
#include "SDL_sysevents.h"
#include "SDL_events_c.h"
#include "SDL_xboxvideo.h"
#include "SDL_xboxevents_c.h"


static SDLKey DIK_keymap[256];
static SDL_keysym *TranslateKey(UINT scancode, SDL_keysym *keysym, int pressed);

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

static void keyboard_update(void);
static void mouse_update(void);

/* Constantes para DmGetMouseChanges: bitmask de botones devuelta por XBDM. */
#define DM_MOUSE_LBUTTON 0x01   /* Left button */
#define DM_MOUSE_RBUTTON 0x02   /* Right button */
#define DM_MOUSE_MBUTTON 0x04   /* Middle button */

static HANDLE g_hKeyboardDevice[4] = { 0 };
static HANDLE g_hMouseDevice[4] = { 0 };

BOOL                     g_bDevicesInitialized  = FALSE;

/* The translation table from a Xbox KB scancode to an SDL keysym */


void XBOX_PumpEvents(_THIS)
{
	/* Polear el teclado USB cada vez que SDL nos pida bombear eventos.
	 * SDL llama esto desde SDL_PumpEvents y, transitivamente, desde
	 * SDL_PollEvent — que es lo que Salvia usa en pollKeys(). */
	keyboard_update();
	mouse_update();
}

void XBOX_InitOSKeymap(_THIS)
{
	DWORD i;
#ifndef _XBOX_DONT_USE_DEVICES
	/* Map the Xbox VK_* virtual key codes to SDL keysyms. Inicializa todo
	 * a 0 (SDLK_UNKNOWN) y luego rellena las entradas conocidas. */
	for (i = 0; i < SDL_TABLESIZE(DIK_keymap); ++i)
		DIK_keymap[i] = 0;

	DIK_keymap[VK_ESCAPE]        = SDLK_ESCAPE;
	DIK_keymap['1']              = SDLK_1;
	DIK_keymap['2']              = SDLK_2;
	DIK_keymap['3']              = SDLK_3;
	DIK_keymap['4']              = SDLK_4;
	DIK_keymap['5']              = SDLK_5;
	DIK_keymap['6']              = SDLK_6;
	DIK_keymap['7']              = SDLK_7;
	DIK_keymap['8']              = SDLK_8;
	DIK_keymap['9']              = SDLK_9;
	DIK_keymap['0']              = SDLK_0;
	DIK_keymap[VK_SUBTRACT]      = SDLK_MINUS;
	DIK_keymap[VK_equal]         = SDLK_EQUALS;
	DIK_keymap[VK_BACK]          = SDLK_BACKSPACE;
	DIK_keymap[VK_TAB]           = SDLK_TAB;
	DIK_keymap['Q']              = SDLK_q;
	DIK_keymap['W']              = SDLK_w;
	DIK_keymap['E']              = SDLK_e;
	DIK_keymap['R']              = SDLK_r;
	DIK_keymap['T']              = SDLK_t;
	DIK_keymap['Y']              = SDLK_y;
	DIK_keymap['U']              = SDLK_u;
	DIK_keymap['I']              = SDLK_i;
	DIK_keymap['O']              = SDLK_o;
	DIK_keymap['P']              = SDLK_p;
	DIK_keymap[VK_bracketleft]   = SDLK_LEFTBRACKET;
	DIK_keymap[VK_bracketright]  = SDLK_RIGHTBRACKET;
	DIK_keymap[VK_RETURN]        = SDLK_RETURN;
	DIK_keymap[VK_LCONTROL]      = SDLK_LCTRL;
	DIK_keymap['A']              = SDLK_a;
	DIK_keymap['S']              = SDLK_s;
	DIK_keymap['D']              = SDLK_d;
	DIK_keymap['F']              = SDLK_f;
	DIK_keymap['G']              = SDLK_g;
	DIK_keymap['H']              = SDLK_h;
	DIK_keymap['J']              = SDLK_j;
	DIK_keymap['K']              = SDLK_k;
	DIK_keymap['L']              = SDLK_l;
	DIK_keymap[VK_OEM_1]         = SDLK_SEMICOLON;
	DIK_keymap[VK_OEM_7]         = SDLK_QUOTE;
	DIK_keymap[VK_OEM_3]         = SDLK_BACKQUOTE;
	DIK_keymap[VK_LSHIFT]        = SDLK_LSHIFT;
	DIK_keymap[VK_backslash]     = SDLK_BACKSLASH;
	DIK_keymap['Z']              = SDLK_z;
	DIK_keymap['X']              = SDLK_x;
	DIK_keymap['C']              = SDLK_c;
	DIK_keymap['V']              = SDLK_v;
	DIK_keymap['B']              = SDLK_b;
	DIK_keymap['N']              = SDLK_n;
	DIK_keymap['M']              = SDLK_m;
	DIK_keymap[VK_OEM_COMMA]     = SDLK_COMMA;
	DIK_keymap[VK_OEM_PERIOD]    = SDLK_PERIOD;
	DIK_keymap[VK_OEM_PLUS]      = SDLK_PLUS;
	DIK_keymap[VK_OEM_MINUS]     = SDLK_MINUS;
	DIK_keymap[VK_slash]         = SDLK_SLASH;
	DIK_keymap[VK_RSHIFT]        = SDLK_RSHIFT;
	DIK_keymap[VK_MULTIPLY]      = SDLK_KP_MULTIPLY;
	DIK_keymap[VK_LMENU]         = SDLK_LALT;
	DIK_keymap[VK_SPACE]         = SDLK_SPACE;
	DIK_keymap[VK_CAPITAL]       = SDLK_CAPSLOCK;
	DIK_keymap[VK_F1]            = SDLK_F1;
	DIK_keymap[VK_F2]            = SDLK_F2;
	DIK_keymap[VK_F3]            = SDLK_F3;
	DIK_keymap[VK_F4]            = SDLK_F4;
	DIK_keymap[VK_F5]            = SDLK_F5;
	DIK_keymap[VK_F6]            = SDLK_F6;
	DIK_keymap[VK_F7]            = SDLK_F7;
	DIK_keymap[VK_F8]            = SDLK_F8;
	DIK_keymap[VK_F9]            = SDLK_F9;
	DIK_keymap[VK_F10]           = SDLK_F10;
	DIK_keymap[VK_NUMLOCK]       = SDLK_NUMLOCK;
	DIK_keymap[VK_SCROLL]        = SDLK_SCROLLOCK;
	DIK_keymap[VK_NUMPAD7]       = SDLK_KP7;
	DIK_keymap[VK_NUMPAD8]       = SDLK_KP8;
	DIK_keymap[VK_NUMPAD9]       = SDLK_KP9;
	DIK_keymap[VK_ADD]           = SDLK_KP_PLUS;
	DIK_keymap[VK_SUBTRACT]      = SDLK_KP_MINUS;
	DIK_keymap[VK_NUMPAD4]       = SDLK_KP4;
	DIK_keymap[VK_NUMPAD5]       = SDLK_KP5;
	DIK_keymap[VK_NUMPAD6]       = SDLK_KP6;
	DIK_keymap[VK_NUMPAD1]       = SDLK_KP1;
	DIK_keymap[VK_NUMPAD2]       = SDLK_KP2;
	DIK_keymap[VK_NUMPAD3]       = SDLK_KP3;
	DIK_keymap[VK_NUMPAD0]       = SDLK_KP0;
	DIK_keymap[VK_DECIMAL]       = SDLK_KP_PERIOD;
	DIK_keymap[VK_F11]           = SDLK_F11;
	DIK_keymap[VK_F12]           = SDLK_F12;
	DIK_keymap[VK_F13]           = SDLK_F13;
	DIK_keymap[VK_F14]           = SDLK_F14;
	DIK_keymap[VK_F15]           = SDLK_F15;

	DIK_keymap[VK_equal]         = SDLK_EQUALS;
	DIK_keymap[VK_RCONTROL]      = SDLK_RCTRL;
	DIK_keymap[VK_DIVIDE]        = SDLK_KP_DIVIDE;
	DIK_keymap[VK_RMENU]         = SDLK_RALT;
	DIK_keymap[VK_PAUSE]         = SDLK_PAUSE;
	DIK_keymap[VK_HOME]          = SDLK_HOME;
	DIK_keymap[VK_UP]            = SDLK_UP;
	DIK_keymap[VK_PRIOR]         = SDLK_PAGEUP;
	DIK_keymap[VK_LEFT]          = SDLK_LEFT;
	DIK_keymap[VK_RIGHT]         = SDLK_RIGHT;
	DIK_keymap[VK_END]           = SDLK_END;
	DIK_keymap[VK_DOWN]          = SDLK_DOWN;
	DIK_keymap[VK_NEXT]          = SDLK_PAGEDOWN;
	DIK_keymap[VK_INSERT]        = SDLK_INSERT;
	DIK_keymap[VK_DELETE]        = SDLK_DELETE;
	DIK_keymap[VK_APPS]          = SDLK_MENU;
#endif
}

CHAR XBInput_GetKeyboardInput()
{
	/* Legacy stub conservado por si algún callsite externo lo referencia.
	 * El flujo real va vía XBOX_PumpEvents → keyboard_update → SDL_PrivateKeyboard. */
	return 0;
}


static void keyboard_update(void)
{
#ifndef _XBOX_DONT_USE_DEVICES
	SDL_keysym keysym;
	XINPUT_KEYSTROKE m_Key;

	/* XInputGetKeystroke entrega de a una sola pulsación por llamada en
	 * una cola interna del runtime de Xbox. Devuelve ERROR_EMPTY (no
	 * ERROR_SUCCESS) cuando no hay nada pendiente — lo tratamos como
	 * "sin novedades" y volvemos. */
	HRESULT hr = XInputGetKeystroke(XUSER_INDEX_ANY, XINPUT_FLAG_KEYBOARD, &m_Key);
	if (hr != ERROR_SUCCESS)
		return;

	if (m_Key.VirtualKey == 0)   /* No key */
		return;

	/* XInput entrega VirtualKey genérico para los modificadores (un solo
	 * VK_SHIFT, un solo VK_CONTROL...) pero SÍ distingue izquierdo/derecho
	 * en HidCode. Remapeamos para que SDL reciba LSHIFT/RSHIFT/LCTRL/...
	 * correctamente — así juegos que diferencian (DOSBox + Wolfenstein con
	 * "Right Alt = strafe", etc.) funcionan. */
	switch (m_Key.HidCode)
	{
		case 0xE1: m_Key.VirtualKey = VK_LSHIFT;   break;
		case 0xE5: m_Key.VirtualKey = VK_RSHIFT;   break;
		case 0xE0: m_Key.VirtualKey = VK_LCONTROL; break;
		case 0xE4: m_Key.VirtualKey = VK_RCONTROL; break;
		case 0xE2: m_Key.VirtualKey = VK_LMENU;    break;
		case 0xE6: m_Key.VirtualKey = VK_RMENU;    break;
	}

	if (m_Key.Flags & XINPUT_KEYSTROKE_KEYUP)
		SDL_PrivateKeyboard(SDL_RELEASED, TranslateKey(m_Key.VirtualKey, &keysym, 1));
	else
		SDL_PrivateKeyboard(SDL_PRESSED,  TranslateKey(m_Key.VirtualKey, &keysym, 0));
#endif
}

static SDL_keysym *TranslateKey(UINT scancode, SDL_keysym *keysym, int pressed)
{
	/* Rellena el SDL_keysym a partir del VirtualKey. Los modifiers
	 * (Shift/Ctrl/Caps) los gestiona la capa SDL_keyboard mirando los
	 * eventos previos de LSHIFT/LCTRL/CAPSLOCK — aquí solo reportamos la
	 * tecla individual con mod=KMOD_NONE. */
	keysym->mod      = KMOD_NONE;
	keysym->scancode = (unsigned char)scancode;
	keysym->sym      = DIK_keymap[scancode];
	keysym->unicode  = 0;
	return keysym;
}


static void mouse_update(void)
{
#if !defined(_XBOX_DONT_USE_DEVICES) && defined(SDL_XBOX_MOUSE)
	/* Lectura de ratón USB vía Xbox Debug Manager. DmGetMouseChanges
	 * devuelve estado RELATIVO (deltas) — la mayoría de ratones USB se
	 * mueven en delta así que esto encaja bien con la convención de
	 * SDL_PrivateMouseMotion(relative=1). El wheel está disponible si
	 * algún día queremos exponerlo vía SDL_PrivateMouseButton con los
	 * eventos SDL_BUTTON_WHEELUP/DOWN (no implementado aquí). */
	static UCHAR mouseButtons = 0, changed;
	static UCHAR prev_buttons = 0;
	static short lastmouseX = 0, lastmouseY = 0;
	short nX = 0, nY = 0;
	short wheel = 0;
	short mouseX, mouseY;
	int i;

	const static char sdl_mousebtn[] = {
		DM_MOUSE_LBUTTON,
		DM_MOUSE_MBUTTON,
		DM_MOUSE_RBUTTON,
	};

	DmGetMouseChanges(&mouseButtons, &nX, &nY, &wheel);

	/* DmGetMouseChanges acumula la posición — si no cambió, no movemos.
	 * (Algunos drivers entregan posición absoluta entre llamadas en vez
	 * de delta real; comparamos con la lectura anterior y mandamos sólo
	 * el delta efectivo a SDL.) */
	if ((lastmouseX == nX) && (lastmouseY == nY)) {
		mouseX = 0;
		mouseY = 0;
	} else {
		mouseX = nX;
		mouseY = nY;
	}

	if (mouseX || mouseY)
		SDL_PrivateMouseMotion(0 /*buttonstate*/, 1 /*relative*/, mouseX, mouseY);

	changed = mouseButtons ^ prev_buttons;
	for (i = 0; i < (int)sizeof(sdl_mousebtn); ++i) {
		if (changed & sdl_mousebtn[i]) {
			SDL_PrivateMouseButton(
				(mouseButtons & sdl_mousebtn[i]) ? SDL_PRESSED : SDL_RELEASED,
				i + 1, 0, 0);
		}
	}

	prev_buttons = mouseButtons;
	lastmouseX   = nX;
	lastmouseY   = nY;
#endif
}

VOID Mouse_RefreshDeviceList()
{
	/* Stub. */
}


