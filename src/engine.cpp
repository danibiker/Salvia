#include "engine.h"

#ifdef _XBOX
	#include <xtl.h>
	
	#ifdef DEBUG
		#ifdef __cplusplus
		extern "C" {
		#endif
			//Parche para error de enlazado en Xbox 360.
			//Muchos cores de Libretro esperan que esta función exista en la CRT,
			//pero el XDK requiere una definición manual si se usan ciertas 
			//funciones de string/file_stream.
			void _chvalidator(void) {
				// Se deja vacío. El core simplemente busca la dirección del símbolo.
			}
		#ifdef __cplusplus
		}
		#endif
	#endif

#elif defined(WIN)
	#include <windows.h>
	#include <mmsystem.h> // Necesario para timeBeginPeriod
#endif

Engine::Engine(){
	constant = new Constant();

	for (int i=0; i < MAX_PLAYERS; i++){
		g_joysticks[i] = NULL;
		for (int j=0; j < RETRO_DEVICE_ID_JOYPAD_R3 + 1; j++){
			g_joy_state[i][j] = false;
		}
	}
}

Engine::~Engine(){
	delete constant;
}

int Engine::initEngine(CfgLoader &cfgLoader){
	running = true;
	LOG_DEBUG("Initiating engine\n");

	#ifdef WIN
		// 1. Activar la precisión de 1ms en el reloj de Windows
		timeBeginPeriod(1);
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		sync.g_sync = SYNC_TO_VIDEO;
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

	initFont();
	init_all_joysticks();

	return 0;
}

void Engine::stopEngine(){
	close_joysticks();
	// 3. Limpieza: Devolver el reloj del sistema a su estado normal
	#ifdef WIN
		timeEndPeriod(1);
	#endif
    SDL_Quit();
}

int Engine::initFont(){
	fonts->init();
	fonts->initFonts(24);
	return 0;
}

void Engine::init_all_joysticks() {
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    int num_joy = SDL_NumJoysticks();
    
	if (num_joy <= 0){
		return;
	}

    // Abrir todos los mandos disponibles hasta el límite de jugadores
    for (int i = 0; i < num_joy && i < MAX_PLAYERS; i++) {
        g_joysticks[i] = SDL_JoystickOpen(i);
        if (g_joysticks[i]) {
            LOG_DEBUG("Mando %d abierto: %s\n", i, SDL_JoystickName(i));
        }
    }
}

void Engine::close_joysticks() {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (g_joysticks[i]) {
			SDL_JoystickClose(g_joysticks[i]);
			g_joysticks[i] = NULL;
		}
	}
}