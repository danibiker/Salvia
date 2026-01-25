#include "hotkeys.h"
#include <const/constant.h>
#include <iostream>

Hotkeys::Hotkeys(){
	 g_modifierButton = JOY_BUTTON_SELECT;

    for (int i=0; i < HK_MAX; i++){
        HotkeyConfig configMenu; // Objeto local en el stack
        configMenu.action = static_cast<HOTKEYS_LIST>(i);
        
        switch(i){
            case HK_VIEW_MENU:  configMenu.triggerButton = JOY_BUTTON_Y; break;
            case HK_EXIT_GAME:  configMenu.triggerButton = JOY_BUTTON_START; break;
            case HK_SAVESTATE:  configMenu.triggerButton = JOY_BUTTON_L; break;
            case HK_LOADSTATE:  configMenu.triggerButton = JOY_BUTTON_R; break;
            case HK_SCALE:      configMenu.triggerButton = JOY_BUTTON_X; break;
            case HK_RATIO:      configMenu.triggerButton = JOY_BUTTON_A; break;
            case HK_SHADER:     configMenu.triggerButton = JOY_BUTTON_B; break;
            case HK_SLOT_UP:    configMenu.triggerButton = JOY_BUTTON_UP; break;
            case HK_SLOT_DOWN:  configMenu.triggerButton = JOY_BUTTON_DOWN; break;
            default:            configMenu.triggerButton = MAXJOYBUTTONS; break; 
        }
		if (configMenu.triggerButton != MAXJOYBUTTONS){
			g_hotkeys.push_back(configMenu); // El vector guarda una copia
		}
    }
}

Hotkeys::~Hotkeys(){
}

int Hotkeys::getTriggerForAction(HOTKEYS_LIST hk){
	for (int i=0; i < g_hotkeys.size(); i++){
		if (g_hotkeys[i].action == hk)
			return g_hotkeys[i].triggerButton;
	}
	return -1;
}

HOTKEYS_LIST Hotkeys::ProcesarHotkeys(bool keypresses[MAXJOYBUTTONS + 1]) {
    bool* state = keypresses;
	static Uint32 lastHotKey = 0;
	const Uint32 now = SDL_GetTicks();

	if (now - lastHotKey > 300){
		// 1. Verificamos si el modificador (BACK) está pulsado
		if (state[g_modifierButton]) {
        
			// 2. Recorremos nuestras hotkeys configuradas
			for (size_t i = 0; i < g_hotkeys.size(); ++i) {
				int btn = g_hotkeys[i].triggerButton;
            
				// 3. Detectar el flanco de subida (pulsación inicial)
				// Necesitas un array 'prev_state' para no disparar 60 veces por segundo
				if (state[btn] && !prev_state[btn]) {
					lastHotKey = now;
					return g_hotkeys[i].action;
				}
			}
		}
		// Actualizar estado previo para el próximo frame
		memcpy(prev_state, state, sizeof(bool) * (MAXJOYBUTTONS + 1));
	}
	return HK_MAX;
}