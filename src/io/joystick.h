#pragma once

#include <map>
#include <libretro.h>
#include <beans/structures.h>
#include <io/cursorgestor.h>


#define MAX_PLAYERS 4

// Mapeo conceptual (esto depende de tu mando, aquí un estándar tipo Xbox/PlayStation)
#ifdef _XBOX
enum {
    SDL_BUTTON_A = 0,
    SDL_BUTTON_B = 1,
    SDL_BUTTON_X = 2,
    SDL_BUTTON_Y = 3,
    SDL_BUTTON_L = 4,
    SDL_BUTTON_R = 5,
    SDL_BUTTON_START = 8,
	SDL_BUTTON_SELECT = 9,
	SDL_LEFT_STICK = 10,
	SDL_RIGHT_STICK = 11
};
#elif  defined(WIN)
enum {
    SDL_BUTTON_A = 0,
    SDL_BUTTON_B = 1,
    SDL_BUTTON_X = 2,
    SDL_BUTTON_Y = 3,
    SDL_BUTTON_L = 4,
    SDL_BUTTON_R = 5,
    SDL_BUTTON_SELECT = 6,
    SDL_BUTTON_START = 7,
	SDL_LEFT_STICK = 10,
	SDL_RIGHT_STICK = 11
};
#endif

class Joystick{
    public:
        Joystick();
        ~Joystick();
		void clearEvento(tEvento *evento);
		tEvento WaitForKey(SDL_Surface* screen);
		bool init_all_joysticks();
		void close_joysticks();
		bool g_joy_state[MAX_PLAYERS][RETRO_DEVICE_ID_JOYPAD_R3 + 1];

		int getNumJoysticks(){return mNumJoysticks;}
		void resetAllValues();
		Uint32 lastSelectPress;

    private:
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
};

