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
    const Uint32 now = SDL_GetTicks();
    static Uint32 lastHotKey = 0;
    HOTKEYS_LIST result = HK_MAX;

    if (keypresses[g_modifierButton]) {
        for (size_t i = 0; i < g_hotkeys.size(); ++i) {
            int btn = g_hotkeys[i].triggerButton;
            
            // Detectamos pulsación inicial y respetamos el cooldown
            if (keypresses[btn] && !prev_state[btn]) {
                if (now - lastHotKey > 300) {
                    lastHotKey = now;
                    result = g_hotkeys[i].action;
                    // RESET: Limpiamos el array para que nadie más procese estos botones
                    memset(keypresses, 0, sizeof(bool) * (MAXJOYBUTTONS + 1));
					//Pero no soltamos el boton de seleccion de hotkeys
                    keypresses[g_modifierButton] = true;
                    // Salimos del bucle para no procesar múltiples hotkeys a la vez
                    break; 
                }
            }
        }
    }

    // Actualizamos el estado previo con lo que quede en keypresses 
    // (Si hubo reset, prev_state guardará todo en 'false')
    memcpy(prev_state, keypresses, sizeof(bool) * (MAXJOYBUTTONS + 1));
    
    return result;
}