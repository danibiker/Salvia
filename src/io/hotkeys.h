#pragma once

#include <const/constant.h>
#include <beans/structures.h>

typedef enum{ 
	HK_MODIFIER = 0,
    HK_SAVESTATE, 
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

const static char *HOTKEYS_STR[] = {
	"Tecla para activar teclas rápidas",
	"Guardar estado", 
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
		Hotkeys(t_joy_state *inputs);
		~Hotkeys();
		HOTKEYS_LIST procesarHotkeys(t_joy_state *inputs);
		//std::vector<HotkeyConfig> g_hotkeys;
		//int g_modifierButton; // Por ejemplo, el botón BACK (Select)
		//int getTriggerForAction(HOTKEYS_LIST);
	private:
};