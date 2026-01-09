#include <engine.h>
#include <io/joystick.h>

#ifdef _XBOX
	#include <xtl.h>
#elif defined(WIN)
	#include <windows.h>
	#include <mmsystem.h> // Necesario para timeBeginPeriod
#endif

Engine::Engine(){
	
}

Engine::~Engine(){
	stopEngine();
}

int Engine::initEngine(CfgLoader* cfgLoader){
	running = true;
	LOG_DEBUG("Initiating engine\n");

	#ifdef WIN
		// 1. Activar la precisión de 1ms en el reloj de Windows
		timeBeginPeriod(1);
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		// Pantalla completa sin borde. Parece que pantalla completa sin borde es la forma de ejecucion mas rapida
		//SDL_putenv("SDL_VIDEO_WINDOW_POS=0,0");
		//video_flags = video_flags | SDL_NOFRAME;
	#endif

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		//fprintf(stderr, "Error SDL_Init: %s\n", SDL_GetError());
		LOG_ERROR("Error SDL_Init: %s\n", SDL_GetError());
		return 1;
    }

	screen = SDL_SetVideoMode(video_width, video_height, video_bpp, video_flags);

	if (!screen){
		LOG_ERROR("Error SDL_SetVideoMode: %s\n", SDL_GetError());
		return 1;
	}

	if (TTF_Init() == -1) {
		LOG_ERROR("Error TTF_Init: %s\n", TTF_GetError());
	}

	initFont();
	joystick = new Joystick();
	sync = new Sync(cfgLoader->configMain.syncMode);

	return 0;
}

void Engine::stopEngine(){
	delete joystick;
	delete fonts;
	// 3. Limpieza: Devolver el reloj del sistema a su estado normal
	#ifdef WIN
		timeEndPeriod(1);
	#endif
	SDL_FreeSurface(screen);
    SDL_Quit();
}

int Engine::initFont(){
	fonts = new Fonts();
	fonts->initFonts(24);
	return 0;
}

tEvento Engine::WaitForKey(){
	return joystick->WaitForKey(screen);
}


