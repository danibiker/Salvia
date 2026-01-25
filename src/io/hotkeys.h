#pragma once

#include <const\constant.h>

typedef enum{ HK_SAVESTATE = 0, 
	HK_LOADSTATE, 
	HK_SCALE, 
	HK_RATIO, 
	HK_SHADER,
	HK_SLOT_UP,
    HK_SLOT_DOWN,
	HK_EXIT_GAME,
	HK_VIEW_MENU,
	HK_MAX 
} HOTKEYS_LIST;

const static char *HOTKEYS_STR[] = {"Guardar estado", 
	"Cargar estado", 
	"Cambiar escalador de video", 
	"Cambiar ratio de video", 
	"Cambiar shader", 
	"Aumentar slot de estado", 
	"Disminuir slot de estado", 
	"Salir del juego",
	"Mostrar Menú",
	"No implementado"};

const static int MAX_COMBINATIONS = 3;

struct HotkeyConfig {
    HOTKEYS_LIST action;
    int triggerButton; // ID del botón de SDL (ej: A, B, X, Y)
};


class Hotkeys{
	public:
		Hotkeys();
		~Hotkeys();
		HOTKEYS_LIST ProcesarHotkeys(bool keypresses[MAXJOYBUTTONS + 1]);
		// Mapa de configuración (puedes cargarlo desde un .cfg)
		std::vector<HotkeyConfig> g_hotkeys;
		int g_modifierButton; // Por ejemplo, el botón BACK (Select)
		int getTriggerForAction(HOTKEYS_LIST);
	private:
		bool prev_state[MAXJOYBUTTONS + 1];
};