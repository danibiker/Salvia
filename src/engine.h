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

#ifdef WIN
	#include <video/win_d3d9.h>
#endif

#include <video/HLSLBackground.h>

/* "Flip" del frontend. En Xbox el driver SDL compone game+overlay dentro de
   SDL_Flip; en Windows lo hace WinD3D9_Present (sube la textura, dibuja el
   quad con shader y compone el overlay). En cualquier otra plataforma cae al
   SDL_Flip estandar. */
static inline void salviaFlip(SDL_Surface* s){
#if defined(WIN) && defined(SALVIA_GPU_VIDEO)
	(void)s; WinD3D9_Present();
#else
	SDL_Flip(s);
#endif
}

//Definimos un tamanyo de fuente base de 24, que va bien a una resolucion 720
const int BASE_FONT_HEIGHT = 24;

class Engine{
    public:
        Engine();
        ~Engine();
		SDL_Surface* overlay;
		SDL_Surface* gameScreen;
#ifdef WIN
		HLSLBackground hlslBkg;
#endif
		Fonts* fonts;
		// Instancia global para los callbacks
		AudioBuffer g_audioBuffer;
		AudioRateControl g_audioRate;
		Sync *sync;
		Joystick *joystick;
		struct t_keyboard *keyb;
		// Variable global para controlar la ejecuci�n
		bool running;
		int initEngine(CfgLoader* cfgLoader);
        void stopEngine();
		void initColors(SDL_Surface *srf);
    protected:
		int initFont();
		
		Fileio fileio;
    private:
};
