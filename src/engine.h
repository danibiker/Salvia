#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <audio\audiobuffer.h>
#include <const\Constant.h>
#include <io\cfgloader.h>
#include <io\joystick.h>
#include <io\fileio.h>
#include <io\sync.h>
#include <font\fonts.h>
#include <map>

class Engine{
    public:
        Engine();
        ~Engine();
        // Variable usada para la velocidad
		SDL_Surface* screen;

		Fonts* fonts;
		// Instancia global para los callbacks
		AudioBuffer g_audioBuffer;
		Sync *sync;
		Joystick *joystick;

		// Variable global para controlar la ejecución
		bool running;
		int initEngine(CfgLoader* cfgLoader);
        void stopEngine();
    protected:
		int initFont();
		Fileio fileio;
    private:
};
