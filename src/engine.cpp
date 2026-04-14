#include <engine.h>
#include <io/joystick.h>
#include <http/badgedownloader.h>

#ifdef _XBOX
	#include <xtl.h>
#elif defined(WIN)
	#include <windows.h>
	#include <mmsystem.h> // Necesario para timeBeginPeriod
	//#pragma comment(lib, "winmm.lib") // Necesario para timeBeginPeriod
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
		// 1. Activar la precisi�n de 1ms en el reloj de Windows
		timeBeginPeriod(1);
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	#endif

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		//fprintf(stderr, "Error SDL_Init: %s\n", SDL_GetError());
		LOG_ERROR("Error SDL_Init: %s\n", SDL_GetError());
		return 1;
    }

	#ifdef WIN
		if (video_fullscreen){
			const SDL_VideoInfo* info = SDL_GetVideoInfo();
			video_width = info->current_w;
			video_height = info->current_h;
			// Pantalla completa sin borde. Parece que pantalla completa sin borde es la forma de ejecucion mas rapida
			SDL_putenv("SDL_VIDEO_WINDOW_POS=0,0");
			video_flags = video_flags | SDL_NOFRAME;
		}
	#endif

	gameScreen = SDL_SetVideoMode(video_width, video_height, video_bpp, video_flags);

	if (!gameScreen){
		LOG_ERROR("Error SDL_SetVideoMode: %s\n", SDL_GetError());
		return 1;
	}
	
#ifdef _XBOX
	//En xbox dibujamos sobre un overlay para conseguir la maxima velocidad de renderizado
	//Asi separamos la logica de los menus de la pantalla del juego
	overlay = SDL_XBOX_GetOverlay();

	if (!overlay){
		LOG_ERROR("Error no se ha podido obtener el overlay\n");
		return 1;
	} else {
		memset(overlay->pixels, 0, overlay->pitch * overlay->h);
		SDL_XBOX_SetOverlayEnabled(1);
	}
#else
	//En pc por ahora no tenemos overlay
	overlay = gameScreen;
#endif

	SDL_WM_SetCaption("Salvia", NULL);

	if (TTF_Init() == -1) {
		LOG_ERROR("Error TTF_Init: %s\n", TTF_GetError());
	}

	initFont();
	joystick = new Joystick();

	int syncMode;
	cfgLoader->configMain[cfg::syncMode].getPropValue(syncMode);
	sync = new Sync(syncMode);
	return 0;
}

void Engine::stopEngine(){
	delete joystick;
	delete fonts;
	delete sync;
	// 3. Limpieza: Devolver el reloj del sistema a su estado normal
	#ifdef WIN
		timeEndPeriod(1);
	#endif
	SDL_FreeSurface(gameScreen);
	BadgeDownloader::instance().stop();
    SDL_Quit();
}

int Engine::initFont(){
	fonts = new Fonts();
	fonts->initFonts(24);
	return 0;
}


void Engine::initColors(){
	for (int i=0; i < clTotalColors; i++){
		colors[i].color = SDL_MapRGB(overlay->format, colors[i].sdlColor.r, colors[i].sdlColor.g, colors[i].sdlColor.b);
	}
}
