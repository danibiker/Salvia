#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <audio\audiobuffer.h>
#include <audio\audiorate.h>
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
		SDL_Surface* overlay;
		SDL_Surface* gameScreen;
		Fonts* fonts;
		// Instancia global para los callbacks
		AudioBuffer g_audioBuffer;
		AudioRateControl g_audioRate;
		Sync *sync;
		Joystick *joystick;
		struct t_keyboard *keyb;
		// Variable global para controlar la ejecución
		bool running;
		int initEngine(CfgLoader* cfgLoader);
        void stopEngine();
		void initColors(SDL_Surface *srf);
    protected:
		int initFont();
		
		Fileio fileio;
    private:
};
