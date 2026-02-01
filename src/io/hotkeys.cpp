#include "hotkeys.h"
#include <const/constant.h>
#include <iostream>

Hotkeys::Hotkeys(t_joy_state *inputs){
    for (int p=0; p < MAX_PLAYERS; p++){
		inputs->mapperHotkeys.setBtnFromSdl(p, JOY_BUTTON_SELECT, HK_MODIFIER);
		inputs->mapperHotkeys.setBtnFromSdl(p, JOY_BUTTON_A, HK_RATIO);
		inputs->mapperHotkeys.setBtnFromSdl(p, JOY_BUTTON_B, HK_SHADER);
		inputs->mapperHotkeys.setBtnFromSdl(p, JOY_BUTTON_X, HK_SCALE);
		inputs->mapperHotkeys.setBtnFromSdl(p, JOY_BUTTON_Y, HK_VIEW_MENU);
		inputs->mapperHotkeys.setBtnFromSdl(p, JOY_BUTTON_L, HK_SAVESTATE);
		inputs->mapperHotkeys.setBtnFromSdl(p, JOY_BUTTON_R, HK_LOADSTATE);
		inputs->mapperHotkeys.setBtnFromSdl(p, JOY_BUTTON_R3, HK_EXIT_GAME);
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
    // 1. "Early Exit": Si no hay modificador o estamos en cooldown, salimos rápido.
    if (sdlBtnModif == -1 || !inputs->getSdlBtn(0, sdlBtnModif) || (now - lastHotKey < 300)) {
        return HK_MAX;
    }

    // 2. Buscamos el Hotkey
    for (size_t i = 1; i < HK_MAX; i++) {
        // Obtenemos índices una sola vez
        int sdlBtn = inputs->mapperHotkeys.getSdlBtn(0, i);
        int sdlHat = inputs->mapperHotkeys.getSdlHat(0, i);

        // Comprobamos Botón
        if (sdlBtn > -1 && inputs->getSdlBtn(0, sdlBtn)) {
            inputs->btn_state[0][sdlBtn] = false; // Consumir evento
            lastHotKey = now;
            return static_cast<HOTKEYS_LIST>(i);
        }

        // Comprobamos Hat
        if (sdlHat > -1 && inputs->getSdlHat(0, sdlHat)) {
            inputs->hats_state[0][sdlHat] = false; // Consumir evento (Corregido índice sdlHat)
            lastHotKey = now;
            return static_cast<HOTKEYS_LIST>(i);
        }
    }

    return HK_MAX;
}