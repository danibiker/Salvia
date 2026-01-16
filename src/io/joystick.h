#pragma once

#include <map>
#include <libretro.h>
#include <beans/structures.h>
#include <io/cursorgestor.h>

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
	RETRO_DEVICE_ID_JOYPAD_R3	
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
	"RETRO_DEVICE_ID_JOYPAD_R3"	
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
		bool g_joy_state[MAX_PLAYERS][RETRO_DEVICE_ID_JOYPAD_R3 + 1];
		int16_t g_analog_state[MAX_PLAYERS][MAX_ANALOG_AXIS];

		static t_joy_retro_inputs buttonsMapperLibretro[MAX_PLAYERS];
		static t_joy_inputs buttonsMapperFrontend;

		int getNumJoysticks(){return mNumJoysticks;}
		void resetAllValues();
		Uint32 lastSelectPress;
		bool loadButtonsFrontend(std::string);
		void saveButtonsFrontend(std::string);

    private:
		void cargarValoresEnArray(int *&, std::string, int);
		SDL_Joystick* g_joysticks[MAX_PLAYERS];

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
		void loadButtonsEmupad(int);
};

