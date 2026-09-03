#include "hotkeys.h"
#include <const/constant.h>
#include <iostream>

/* Los defaults se declaran por boton LOGICO (JOY_BUTTON_*) y se traducen al
 * indice que entrega SDL con sdlBtnOf() (beans/structures.h, que ya llega por
 * hotkeys.h).
 *
 * Antes se pasaba el JOY_BUTTON_* directamente como indice SDL, que solo es
 * correcto en Windows: en la 360, SELECT es el 9 y no el 6, y START el 8 y no
 * el 7, asi que el MODIFICADOR caia en L3 y la salida del juego en R3. */
Hotkeys::Hotkeys(t_joy_state *inputs){
    for (int p=0; p < MAX_PLAYERS; p++){
		inputs->mapperHotkeys.setBtnFromSdl(p, sdlBtnOf(JOY_BUTTON_SELECT), HK_MODIFIER);
		inputs->mapperHotkeys.setBtnFromSdl(p, sdlBtnOf(JOY_BUTTON_A), HK_RATIO);
		inputs->mapperHotkeys.setBtnFromSdl(p, sdlBtnOf(JOY_BUTTON_B), HK_SHADER);
		inputs->mapperHotkeys.setBtnFromSdl(p, sdlBtnOf(JOY_BUTTON_X), HK_ONSCREEN_KEYB);
		inputs->mapperHotkeys.setBtnFromSdl(p, sdlBtnOf(JOY_BUTTON_Y), HK_VIEW_MENU);
		inputs->mapperHotkeys.setBtnFromSdl(p, sdlBtnOf(JOY_BUTTON_L), HK_SAVESTATE);
		inputs->mapperHotkeys.setBtnFromSdl(p, sdlBtnOf(JOY_BUTTON_R), HK_LOADSTATE);
		inputs->mapperHotkeys.setBtnFromSdl(p, sdlBtnOf(JOY_BUTTON_START), HK_EXIT_GAME);
		inputs->mapperHotkeys.setBtnFromSdl(p, sdlBtnOf(JOY_BUTTON_R3), HK_FAST_FORWARD);
		inputs->mapperHotkeys.setHatFromSdl(p, SDL_HAT_UP, HK_SLOT_UP);
		inputs->mapperHotkeys.setHatFromSdl(p, SDL_HAT_DOWN, HK_SLOT_DOWN);
    }
}

Hotkeys::~Hotkeys(){
}

/*int Hotkeys::getTriggerForAction(HOTKEYS_LIST hk){
	for (int i=0; i < g_hotkeys.size(); i++){
		if (g_hotkeys[i].action == hk)
			return g_hotkeys[i].triggerButton;
	}
	return -1;
}*/

HOTKEYS_LIST Hotkeys::procesarHotkeys(t_joy_state *inputs) {
    const Uint32 now = SDL_GetTicks();
    static Uint32 lastHotKey = 0;

	int sdlBtnModif = inputs->mapperHotkeys.getSdlBtn(0, HK_MODIFIER);
    // 1. "Early Exit": Si no hay modificador o estamos en cooldown, salimos rapido.
    if (sdlBtnModif == -1 || !inputs->getSdlBtn(0, sdlBtnModif) || (now - lastHotKey < 300)) {
        return HK_MAX;
    }

    // 2. Buscamos el Hotkey
    for (size_t i = 1; i < HK_MAX; i++) {
        // Obtenemos indices una sola vez
        int sdlBtn = inputs->mapperHotkeys.getSdlBtn(0, i);
        int sdlHat = inputs->mapperHotkeys.getSdlHat(0, i);

        // Comprobamos Boton
        if (sdlBtn > -1 && inputs->getSdlBtn(0, sdlBtn)) {
            inputs->btn_state[0][sdlBtn] = false; // Consumir evento
            lastHotKey = now;
            return static_cast<HOTKEYS_LIST>(i);
        }

        // Comprobamos Hat
        if (sdlHat > -1 && inputs->getSdlHat(0, sdlHat)) {
            inputs->hats_state[0][sdlHat] = false; // Consumir evento (Corregido indice sdlHat)
            lastHotKey = now;
            return static_cast<HOTKEYS_LIST>(i);
        }
    }

    return HK_MAX;
}