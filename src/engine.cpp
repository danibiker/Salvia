#include <engine.h>
#include <io/joystick.h>
#include <io/keyboard.h>
#include <http/badgedownloader.h>
#include <image/icons.h>
#include <video/shaderpreset.h>

#ifdef _XBOX
	#include <xtl.h>
#elif defined(WIN)
	#include <windows.h>
	#include <mmsystem.h> // Necesario para timeBeginPeriod
	#include <SDL_syswm.h> // Para obtener el HWND de la ventana SDL
	#pragma comment(lib, "winmm.lib") // Necesario para timeBeginPeriod
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
		// 1. Activar la precision de 1ms en el reloj de Windows.
		// El limitador de frames (Sync::limit_fps) duerme el grueso de la espera
		// con SDL_Delay y solo clava el instante final con espera activa; con la
		// granularidad por defecto (15.6 ms) el sleep se pasaria y el limitador
		// tendria que gastar en espera activa TODO el frame.  No dependemos de
		// que la SDL lo suba por dentro en SDL_SYS_TimerInit: lo pedimos aqui.
		timeBeginPeriod(1);
		//SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		// Forzar el driver GDI: SDL solo gestiona la ventana/eventos; el
		// render lo hace nuestra capa D3D9 (no llamamos a SDL_Flip en PC).
		SDL_putenv("SDL_VIDEODRIVER=windib");
	#elif defined(_XBOX)
		HANDLE currrentThread = GetCurrentThread();
		SetThreadPriority(currrentThread, THREAD_PRIORITY_NORMAL);
		// Pinear el main thread (Salvia + retro_run + dynarec PSX del core libretro) a HW thread 0.
		// Razones:
		//  - SMT partner (HW thread 1) idle: pipeline del core fisico 0 enteramente para el dynarec.
		//  - No comparte L1/L2 con SPU (HW thread 3), GPU helper (HW thread 4) ni IO/HTTP (HW thread 5).
		//  - Salvia ya arranca en HW thread 0 por defecto; el pin solo garantiza que el
		//    dispatcher no migre el thread bajo presion, no cambia el patron de ejecucion.
		XSetThreadProcessor(currrentThread, CPU_THREAD);

		video_width = cfgLoader->configMain[cfg::resolution_width].valueInt;   // 0 = Auto
		video_height = cfgLoader->configMain[cfg::resolution_height].valueInt;
		SDL_XBOX_SetScreenResolution(video_width, video_height);              // Auto (<=0) -> XGetVideoMode dentro
		//Releer las dims REALES del backbuffer (imprescindible cuando se pidio "Auto":
		//la config es 0/0 y SDL_SetVideoMode + el escalado de fuentes necesitan dims reales)
		SDL_XBOX_GetScreenResolution(&video_width, &video_height);
	#endif

	/* AUDIO va aqui explicitamente porque el dispositivo se abre en el arranque
	 * (ver init_sdl_audio) y no al cargar el primer juego.  Antes funcionaba sin
	 * pedirlo porque SDL_OpenAudio auto-inicializa el subsistema, pero conviene
	 * que la inicializacion sea la que se declara. */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
		//fprintf(stderr, "Error SDL_Init: %s\n", SDL_GetError());
		LOG_ERROR("Error SDL_Init: %s\n", SDL_GetError());
		return 1;
    }

	SDL_ShowCursor(SDL_DISABLE);

	// Habilitar traduccion a UNICODE en eventos de teclado. Sin esto,
	// event.key.keysym.unicode siempre vale 0 y los cores libretro que
	// dependen del campo `character` del callback (DOSBox-Pure, ScummVM
	// para typing en menus, etc.) no reciben la tecla traducida.
	SDL_EnableUNICODE(1);

	#ifdef WIN
		if (cfgLoader->configMain[cfg::fullscreen].valueBool){
			const SDL_VideoInfo* info = SDL_GetVideoInfo();
			video_width = info->current_w;
			video_height = info->current_h;
			// Pantalla sin borde. Parece que pantalla completa sin borde es la forma de ejecucion mas rapida
			// Ademas, si especificamos SDL_FULLSCREEN habria que implementar una deteccion de perdida del foco
			// porque parece que windows le asigna todos los inputs del teclado y raton y no podemos volver 
			// al SO comodamente
			SDL_putenv("SDL_VIDEO_WINDOW_POS=0,0");
			video_flags = video_flags | SDL_NOFRAME;
		} else {
			//Modo ventana: cualquier resolucion del fichero de config (sin cap ni allow-list).
			video_width = cfgLoader->configMain[cfg::resolution_width].valueInt;
			video_height = cfgLoader->configMain[cfg::resolution_height].valueInt;
			//Centinela "Auto" (0/0): no hay XGetVideoMode en Windows -> default 1280x720.
			if (video_width <= 0 || video_height <= 0) { video_width = 1280; video_height = 720; }
		}
	#endif

#if defined(_XBOX) || defined(SALVIA_GPU_VIDEO)
	/* La tabla de shaders tiene que estar publicada ANTES de que el backend de
	 * video se inicialice: initShaders() se llama desde dentro de
	 * SDL_SetVideoMode (Xbox) y de WinD3D9_Init (Windows). Si llegase tarde,
	 * el backend arrancaria solo con el passthrough integrado. */
	ShaderRegistry::instance()->publish();
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
#elif defined(WIN) && defined(SALVIA_GPU_VIDEO)
	// SDL_SetVideoMode solo nos ha servido para crear la ventana (driver
	// windib). Inicializamos D3D9 sobre su HWND y montamos el mismo modelo
	// que en Xbox: textura de juego escalada por GPU + overlay ARGB.
	{
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		if (SDL_GetWMInfo(&wmInfo) <= 0){
			LOG_ERROR("Error SDL_GetWMInfo: %s\n", SDL_GetError());
			return 1;
		}
		if (!WinD3D9_Init(wmInfo.window, video_width, video_height)){
			LOG_ERROR("Error inicializando D3D9 (ps_3_0 no soportado?)\n");
			return 1;
		}
		// gameScreen pasa a ser el surface del juego (origen de la textura
		// D3D9), NO la surface de ventana de SDL. Se recrea al tamano nativo
		// del core en hw_refresh.
		gameScreen = WinD3D9_SetGameMode(video_width, video_height, video_bpp);
		if (!gameScreen){
			LOG_ERROR("Error WinD3D9_SetGameMode\n");
			return 1;
		}

		overlay = SDL_XBOX_GetOverlay();
		if (!overlay){
			LOG_ERROR("Error no se ha podido obtener el overlay\n");
			return 1;
		}
		memset(overlay->pixels, 0, overlay->pitch * overlay->h);
		SDL_XBOX_SetOverlayEnabled(1);
	}
#else
	overlay = gameScreen;
#endif

#if defined(_XBOX) || defined(SALVIA_GPU_VIDEO)
	/* Las LUT ya estan subidas a la GPU y sus texturas sobreviven al device
	 * lost/reset por su cuenta, asi que soltamos los pixeles en RAM (hasta
	 * ~470 KB con las tres tablas de HQx). */
	ShaderRegistry::instance()->freeTransientBuffers();
#endif

	//Actualizar overscan en windows y xbox
	SDL_XBOX_SetOverscan(cfgLoader->configMain[cfg::overscan_x].valueInt, cfgLoader->configMain[cfg::overscan_y].valueInt);
	SDL_WM_SetCaption("Salvia", NULL);
	initFont();
	joystick = new Joystick();
	keyb = new t_keyboard();

	int syncMode;
	cfgLoader->configMain[cfg::syncMode].getPropValue(syncMode);
	sync = new Sync(syncMode);

	initColors(overlay);
	return 0;
}

void Engine::stopEngine(){
	delete joystick;
	delete keyb;
	delete fonts;
	delete sync;
	// 3. Limpieza: Devolver el reloj del sistema a su estado normal
	#ifdef WIN
		timeEndPeriod(1);
	#endif

#if defined(WIN) && defined(SALVIA_GPU_VIDEO)
	// gameScreen (textura de juego) y overlay son propiedad de WinD3D9;
	// los libera WinD3D9_Shutdown junto con el device D3D9.
	WinD3D9_Shutdown();
	gameScreen = NULL;
	overlay = NULL;
#else
	if (gameScreen){
		SDL_FreeSurface(gameScreen);
		gameScreen = NULL;
	}

	if (overlay){
		SDL_FreeSurface(overlay);
		overlay = NULL;
	}
#endif
	BadgeDownloader::instance().stop();
    SDL_Quit();
}

int Engine::initFont(){
	fonts = new Fonts();
	//Establecemos 720 como la resolucion estandar, y en base a ella, escalamos la fuente
	//dependiendo de la resolucion de la pantalla
	fonts->initFonts((int)(BASE_FONT_HEIGHT * video_height / (float)720));
	return 0;
}


void Engine::initColors(SDL_Surface *srf){
	for (int i=0; i < clTotalColors; i++){
		Constant::colors[i].color = SDL_MapRGBA(srf->format, Constant::colors[i].sdlColor.r, Constant::colors[i].sdlColor.g, Constant::colors[i].sdlColor.b, 0xFF);
		Constant::colors[i].colorRaw = (Constant::colors[i].sdlColor.r << 24) | (Constant::colors[i].sdlColor.g << 16) | (Constant::colors[i].sdlColor.b << 8) | 0xFF; // RRGGBBAA
	}
}
