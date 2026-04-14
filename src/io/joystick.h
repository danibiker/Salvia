#pragma once

#include <map>
#include <vector>
#include <libretro/libretro.h>
#include <beans/structures.h>
#include <io/cursorgestor.h>
#include <io/hotkeys.h>
#include <beans/structures.h>

//El comportamiento de un hat está estandarizado por el propio API: todos los hats 
//se tratan como interruptores de posición de 8 direcciones (más la posición centrada), 
//independientemente de cómo sea físicamente el dispositivo.
#define MAX_HAT_POSITIONS 9

static const int configurablePortButtons[] = {
	RETRO_DEVICE_ID_JOYPAD_A,
	RETRO_DEVICE_ID_JOYPAD_B,
	RETRO_DEVICE_ID_JOYPAD_X,
	RETRO_DEVICE_ID_JOYPAD_Y,
	RETRO_DEVICE_ID_JOYPAD_L,
	RETRO_DEVICE_ID_JOYPAD_R,
	RETRO_DEVICE_ID_JOYPAD_SELECT,
	RETRO_DEVICE_ID_JOYPAD_START,
	RETRO_DEVICE_ID_JOYPAD_L3,
	RETRO_DEVICE_ID_JOYPAD_R3,
	RETRO_DEVICE_ID_JOYPAD_L2,
	RETRO_DEVICE_ID_JOYPAD_R2
};

static const int configurablePortHats[] = {
	RETRO_DEVICE_ID_JOYPAD_UP,
	RETRO_DEVICE_ID_JOYPAD_DOWN,
	RETRO_DEVICE_ID_JOYPAD_LEFT,
	RETRO_DEVICE_ID_JOYPAD_RIGHT
};


static const int configurableSdlHats[] = {
	RETRO_DEVICE_ID_JOYPAD_UP,    // --> SDL_HAT_UP    = 0x01
	RETRO_DEVICE_ID_JOYPAD_RIGHT, // --> SDL_HAT_RIGHT = 0x02
	RETRO_DEVICE_ID_JOYPAD_DOWN,  // --> SDL_HAT_DOWN  = 0x04
	RETRO_DEVICE_ID_JOYPAD_LEFT  // --> SDL_HAT_LEFT  = 0x08
};

static const int configurableSdlAxis[] = {
	RETRO_DEVICE_ID_JOYPAD_LEFT,   
	RETRO_DEVICE_ID_JOYPAD_RIGHT,
	RETRO_DEVICE_ID_JOYPAD_UP,
	RETRO_DEVICE_ID_JOYPAD_DOWN,
	RETRO_DEVICE_ID_JOYPAD_R2,
	RETRO_DEVICE_ID_JOYPAD_L2
};

static const int configurableFrontButtons[] = {
	JOY_BUTTON_A, JOY_BUTTON_B, JOY_BUTTON_X, JOY_BUTTON_Y, JOY_BUTTON_L, JOY_BUTTON_R, JOY_BUTTON_SELECT, JOY_BUTTON_START, JOY_BUTTON_L3, JOY_BUTTON_R3
};

static const int configurableSdlFrontHats[] = {
	JOY_BUTTON_UP,    // --> SDL_HAT_UP    = 0x01
	JOY_BUTTON_RIGHT, // --> SDL_HAT_RIGHT = 0x02
	JOY_BUTTON_DOWN,  // --> SDL_HAT_DOWN  = 0x04
	JOY_BUTTON_LEFT   // --> SDL_HAT_LEFT  = 0x08
};

static const int configurableSdlFrontAxis[] = {
	JOY_BUTTON_LEFT,   
	JOY_BUTTON_RIGHT,
	JOY_BUTTON_UP,
	JOY_BUTTON_DOWN, 
	JOY_AXIS_R2,
	JOY_AXIS_L2
};

extern t_rom_paths romPaths;

struct t_controller_port {
	int current_device_id;			// ID seleccionado actualmente (ej. RETRO_DEVICE_JOYPAD)
	std::string current_desc;       // Descripción amigable (ej. "SuperScope")
	// Lista de opciones que el core nos dio para este puerto
	std::vector<std::pair<unsigned, std::string>> available_types; 
	t_controller_port(){
		current_device_id = -1;
	}	
};

class Joystick{
    public:
        Joystick();
        ~Joystick();

		bool pollKeys(SDL_Surface* screen);
		bool init_all_joysticks();
		void close_joysticks();
		int getNumJoysticks(){return mNumJoysticks;}
		
		std::string saveButtonsRetroCore();
		std::string saveButtonsRetroGame();
		std::string saveButtonsDefaultsCore();
		std::string saveButtonsConfig(std::string, bool=true);
		bool loadButtonsRetro(std::string);
		void updateTypes();

		HOTKEYS_LIST findHotkey();

		//Array para poder detectar la pulsacion del start en xbox360
		int8_t startHoldFrames[MAX_PLAYERS];
		//Joysticks abiertos
		SDL_Joystick* g_joysticks[MAX_PLAYERS];
		//Type of the controller ports
		t_controller_port g_ports[MAX_PLAYERS];
		//Mapeo de botones para los joysticks
		t_joy_state inputs;
		//Devolvemos un objeto evento en el caso de peticion de salida de SDL
		tEvento evento;
		//Se guardan las hotkeys en una clase aparte
		Hotkeys* hotkeys;
    private:

		void configMapperRetro(t_joy_mapper& mapper, int joyId);
		void configMapperFrontend(t_joy_mapper& mapper, int joyId);


		template <size_t N>
		void cargarValoresEnArray(int8_t (&arr)[N], std::string str, int maxValues) {
			std::vector<std::string> v = Constant::splitChar(str, ',');
			for (int i=0; i < v.size() && i < maxValues; i++){
				arr[i] = Constant::strToTipo<int8_t>(v[i]);
			}
		}

		int w,h;
		int mNumJoysticks;
		bool ignoreButtonRepeats;
		int actualCursor;
		CursorGestor *gestorCursor;
		void setCursor(int cursor);
};

