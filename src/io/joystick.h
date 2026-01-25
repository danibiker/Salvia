#pragma once

#include <map>
#include <vector>
#include <libretro/libretro.h>
#include <beans/structures.h>
#include <io/cursorgestor.h>
#include <io/hotkeys.h>

//El comportamiento de un hat está estandarizado por el propio API: todos los hats 
//se tratan como interruptores de posición de 8 direcciones (más la posición centrada), 
//independientemente de cómo sea físicamente el dispositivo.
#define MAX_HAT_POSITIONS 9
#define MAX_ANALOG_AXIS 16

static int configurablePortButtons[] = {
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

static char * configurablePortButtonsStr[] = {
	"RETRO_DEVICE_ID_JOYPAD_A",
	"RETRO_DEVICE_ID_JOYPAD_B",
	"RETRO_DEVICE_ID_JOYPAD_X",
	"RETRO_DEVICE_ID_JOYPAD_Y",
	"RETRO_DEVICE_ID_JOYPAD_L",
	"RETRO_DEVICE_ID_JOYPAD_R",
	"RETRO_DEVICE_ID_JOYPAD_SELECT",
	"RETRO_DEVICE_ID_JOYPAD_START",
	"RETRO_DEVICE_ID_JOYPAD_L3",
	"RETRO_DEVICE_ID_JOYPAD_R3",
	"RETRO_DEVICE_ID_JOYPAD_L2",
	"RETRO_DEVICE_ID_JOYPAD_R2"
};

static int configurablePortHats[] = {
	RETRO_DEVICE_ID_JOYPAD_UP,
	RETRO_DEVICE_ID_JOYPAD_DOWN,
	RETRO_DEVICE_ID_JOYPAD_LEFT,
	RETRO_DEVICE_ID_JOYPAD_RIGHT
};

static char * configurablePortHatsStr[] = {
	"RETRO_DEVICE_ID_JOYPAD_UP",
	"RETRO_DEVICE_ID_JOYPAD_DOWN",
	"RETRO_DEVICE_ID_JOYPAD_LEFT",
	"RETRO_DEVICE_ID_JOYPAD_RIGHT"
};

class Joystick{
    public:
        Joystick();
        ~Joystick();
		void clearEvento(tEvento *evento);
		tEvento WaitForKey(SDL_Surface* screen);
		bool init_all_joysticks();
		void close_joysticks();
		int8_t startHoldFrames[MAX_PLAYERS];

		//This two arrays are used mainly to know the state of the buttons while the core is running
		//They will be sent to the core
		bool g_joy_state[MAX_PLAYERS][RETRO_DEVICE_ID_JOYPAD_R3 + 1];
		// Sticks analógicos como botones digitales
		bool g_axis_state[MAX_PLAYERS][RETRO_DEVICE_ID_JOYPAD_R3 + 1];     
		//To store the positions of the analog axis, but is not used by any core actually
		int16_t g_analog_state[MAX_PLAYERS][MAX_ANALOG_AXIS];
		//Mapping of buttons for the frontend navigation
		static t_joy_inputs buttonsMapperFrontend;
		//Mapping of buttons for the core
		static t_joy_retro_inputs buttonsMapperLibretro[MAX_PLAYERS];
		//This array is used mainly to detect hotkeys while the core is running
		bool g_joy_frontend_state[1][MAXJOYBUTTONS + 1];

		int getNumJoysticks(){return mNumJoysticks;}
		void resetAllValues();
		//Uint32 lastSelectPress;
		bool loadButtonsFrontend(std::string);
		void saveButtonsFrontend(std::string);
		void resetButtonsFrontend();
		void resetButtonsCore();

		SDL_Joystick* g_joysticks[MAX_PLAYERS];
		std::string saveButtonsRetro();
		Hotkeys hotkeys;
		HOTKEYS_LIST findHotkey();

    private:
		void cargarValoresEnArray(int*&, std::string, int);

		template <size_t N>
		void cargarValoresEnArray(int8_t (&arr)[N], std::string str, int maxValues) {
			std::vector<std::string> v = Constant::splitChar(str, ',');
			for (int i=0; i < v.size() && i < maxValues; i++){
				arr[i] = Constant::strToTipo<int8_t>(v[i]);
			}
		}
		
		int cargarValoresEnArray(t_retro_input *&, std::string);
		void addJoyToList(std::vector<std::string>&, t_joy_retro_inputs&);
		std::string searchNewName(std::map<std::string, t_joy_retro_inputs>&, std::string);
		std::vector<std::string> loadControllerPorts();
		std::map<std::string, t_joy_retro_inputs> loadButtonsRetroList();
		void loadButtonsEmupad(int, std::vector<std::string>& , std::map<std::string, t_joy_retro_inputs>&);

		

		tEvento evento;
        tEvento lastEvento;
		int w,h;
		std::map<int, int>* mPrevAxisValues; //Almacena los valores de los ejes de cada joystick
		std::map<int, int>* mPrevHatValues; //Almacena los valores de las crucetas de cada joystick
		int mNumJoysticks;
		bool ignoreButtonRepeats;
		int actualCursor;
		CursorGestor *gestorCursor;
		void setCursor(int cursor);
		
};

