#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include "const\Constant.h"
#include "audio\audiobuffer.h"
#include "io\joystick.h"
#include "io\sync.h"
#include "io\fileio.h"
#include "libretro.h"

class Engine{
    public:
        Engine();
        ~Engine();
        // Variable usada para la velocidad
		SDL_Surface* screen;
		SDL_Joystick* g_joysticks[MAX_PLAYERS];
		bool g_joy_state[MAX_PLAYERS][RETRO_DEVICE_ID_JOYPAD_R3 + 1];
		TTF_Font* font;
		// Instancia global para los callbacks
		AudioBuffer g_audioBuffer;
		Sync sync;
		// Variable global para controlar la ejecución
		bool running;
		
		int initEngine();
        void stopEngine();
		void init_all_joysticks();

    protected:
		int initFont();
		void close_joysticks();
		Fileio fileio;
    private:
		Constant *constant;
};
