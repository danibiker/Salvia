#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#include <string>

#include "gameMenu.h"
#include "io\video.h"
#include "io\cfgloader.h"
#include "uiobjects\listmenu.h"
#include "uiobjects\tilemap.h"



GameMenu *gameMenu;
Logger *logger;

// Ya no declaramos punteros a función, sino que usamos las funciones 
// que vendrán dentro del .lib (se resuelven al linkar)
extern "C" {
	#include "libretro.h"
    void retro_init(void);
    void retro_deinit(void);
    void retro_run(void);
    void retro_get_system_info(struct retro_system_info *info);
    void retro_get_system_av_info(struct retro_system_av_info *info);
    void retro_set_environment(retro_environment_t);
    void retro_set_video_refresh(retro_video_refresh_t);
    void retro_set_audio_sample(retro_audio_sample_t);
    void retro_set_audio_sample_batch(retro_audio_sample_batch_t);
    void retro_set_input_poll(retro_input_poll_t);
    void retro_set_input_state(retro_input_state_t);
    bool retro_load_game(const struct retro_game_info *game);
}

const char Constant::FILE_SEPARATOR_UNIX = '/';
const std::string Constant::MAME_SYS_ID = "75";
const std::string Constant::WHITESPACE = " \n\r\t";

#if defined(WIN) || defined(DOS) || defined(_XBOX)
    char Constant::FILE_SEPARATOR = 0x5C; //Separador de directorios para win32
#else
    char Constant::FILE_SEPARATOR = Constant::FILE_SEPARATOR_UNIX; //Separador de directorios para unix
#endif

char Constant::tempFileSep[2] = {Constant::FILE_SEPARATOR,'\0'};
std::string Constant::appDir = "";
volatile uint32_t Constant::totalTicks = 0;
int Constant::EXEC_METHOD = launch_batch;
const std::string CfgLoader::CONFIGFILE = "salvia.cfg";

void retro_log_printf(enum retro_log_level level, const char *fmt, ...) {
    //va_list v; va_start(v, fmt); vfprintf(stdout, fmt, v); va_end(v);
	
	if (!logger) {
		va_list v; va_start(v, fmt); vfprintf(stdout, fmt, v); va_end(v);
		return;
	}

    // 1. Mapear el nivel de Libretro a tus niveles internos
    int myLevel;
    switch (level) {
        case RETRO_LOG_DEBUG: myLevel = L_DEBUG; break;
        case RETRO_LOG_INFO:  myLevel = L_INFO;  break;
        case RETRO_LOG_ERROR: myLevel = L_ERROR; break;
        default:              myLevel = L_DEBUG;  break;
    }

    // 2. Procesar los argumentos variables (va_list) que envía el Core
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // 3. Llamar directamente al método write
    // Nota: Como no podemos obtener el archivo/línea real del Core, 
    // indicamos que el origen es "LIBRETRO_CORE"
    logger->write(myLevel, "[CORE] %s", buffer);

}

static bool retro_environment(unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            struct retro_log_callback *log = (struct retro_log_callback*)data;
            log->log = retro_log_printf;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
            // Al devolver false, el core entiende que este frontend es simple
            // y usará la estructura retro_game_info estándar.
            return false;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            // Opcional: Muchos cores preguntan si pueden usar RGB565 (1) o XRGB8888 (2)
            enum retro_pixel_format fmt = *(const enum retro_pixel_format *)data;

			std::string msgformat = "Solicitando pixelformat: " + Constant::intToString(fmt) + "\n";
			retro_log_printf(RETRO_LOG_INFO, msgformat.c_str());

            return true; 
        }
    }
    return false; // Por defecto devolver false para comandos desconocidos
}

static void retro_video_refresh(const void *data, unsigned width, unsigned height, std::size_t pitch) {
    // pitch < (width * 2) -> Asegúrate de que el core envíe al menos 16 bits
	if (!data || width == 0 || height == 0 || pitch < (width * 2)) return;

	//if (gameMenu->sync.g_sync == SYNC_NONE){
	//	fast_video_blit((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
	//} else {
		scale_software_fixed_point((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
	//}
	//scale_bilinear_fast((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);

	//scale2x_software((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
	//scale3x_software((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
	//scale4x_software((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
	//scale_generic_software((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch, 3);
	
	//scale3x_advance((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
	//scale4x_advance((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
	
	//scale_xBRZ_3x((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
	//scale4x_xbrz_software((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
	
}

//Audio Callbacks for Libretro
// Callback para una sola muestra (menos eficiente, pero requerido)
void retro_audio_sample(int16_t left, int16_t right) {
	if (gameMenu->sync.g_sync == SYNC_NONE){
		return;
	}
    int16_t samples[2] = { left, right };
    gameMenu->g_audioBuffer.Write(samples, 2);
}

// Callback para ráfagas de muestras (el que usan casi todos los cores)
std::size_t retro_audio_sample_batch(const int16_t *data, std::size_t frames) {
    if (gameMenu->sync.g_sync == SYNC_NONE){
		return 0;
	}
	
	// frames es el número de pares (izq, der), multiplicamos por 2 para el total
	// Al usar WriteBlocking, retro_run() no terminará hasta que haya
    // sitio en el buffer, sincronizando así la ejecución al audio real.
	if (gameMenu->sync.g_sync == SYNC_TO_AUDIO){
		gameMenu->g_audioBuffer.WriteBlocking(data, frames * 2);
	} else {
		gameMenu->g_audioBuffer.Write(data, frames * 2);
	}
    return frames;
}

void savestate(){
	// Lógica de guardado
	std::size_t state_size = retro_serialize_size();
	if (state_size > 0) {
		void *buffer = malloc(state_size);
		if (retro_serialize(buffer, state_size)) {
			// Aquí puedes escribir 'buffer' en un archivo usando fwrite
			FILE *f = fopen("estado.state", "wb");
			fwrite(buffer, 1, state_size, f);
			fclose(f);
		}
		free(buffer);
	}
}

void loadstate(){
	// Lógica de carga
	FILE *f = fopen("estado.state", "rb");
	if (f) {
		std::size_t state_size = retro_serialize_size();
		void *buffer = malloc(state_size);
    
		fread(buffer, 1, state_size, f);
		if (retro_unserialize(buffer, state_size)) {
			printf("Estado cargado con éxito.\n");
		}
    
		fclose(f);
		free(buffer);
	}
}

void update_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {

		if (event.type == SDL_QUIT) {
            gameMenu->running = false; // Marcamos para salir
        }

		int player = event.jbutton.which; // Índice del mando (0, 1, 2...)
        if (player >= MAX_PLAYERS) continue;

        if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP) {
            bool pressed = (event.type == SDL_JOYBUTTONDOWN);
            
            // Mapeo simple de botones SDL -> Libretro
            switch (event.jbutton.button) {
                case SDL_BUTTON_A: gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_A] = pressed; break;
                case SDL_BUTTON_B: gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_B] = pressed; break;
                case SDL_BUTTON_X: gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_X] = pressed; break;
                case SDL_BUTTON_Y: gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_Y] = pressed; break;
                case SDL_BUTTON_START:  gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_START]  = pressed; break;
                case SDL_BUTTON_SELECT: gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_SELECT] = pressed; break;
            }
        }
        
        // Manejo de la cruceta (D-PAD) mediante Ejes o Hats
        if (event.type == SDL_JOYHATMOTION) {
            gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_UP]    = (event.jhat.value & SDL_HAT_UP) > 0;
            gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_DOWN]  = (event.jhat.value & SDL_HAT_DOWN) > 0;
            gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_LEFT]  = (event.jhat.value & SDL_HAT_LEFT) > 0;
            gameMenu->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_RIGHT] = (event.jhat.value & SDL_HAT_RIGHT) > 0;
        }

		if (event.type == SDL_KEYUP) {
			if (event.key.keysym.sym == SDLK_F6){
				savestate();
			} else if (event.key.keysym.sym == SDLK_F9){
				loadstate();
			} else if (event.key.keysym.sym == SDLK_BACKSPACE){
				gameMenu->sync.g_sync = gameMenu->sync.g_sync_last;
				SDL_PauseAudio(0);
			}
		} else if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_BACKSPACE){
				//Enabling fast forward
				gameMenu->sync.g_sync_last = gameMenu->sync.g_sync;
				gameMenu->sync.g_sync = SYNC_NONE;
				SDL_PauseAudio(1);
			}
		}
    }
}

// Se llama antes de pedir el estado de los inputs
void retro_input_poll(void) {
    update_input();
}

int16_t retro_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port >= MAX_PLAYERS || device != RETRO_DEVICE_JOYPAD) 
        return 0;
    
    // Devolvemos el estado del botón 'id' para el jugador 'port'
    return gameMenu->g_joy_state[port][id] ? 1 : 0;
}



//Audio callbacks for SDL
void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
    // SDL pide bytes, pero trabajamos con muestras de 16 bits (2 bytes)
    int16_t* samples = (int16_t*)stream;
    std::size_t count = len / sizeof(int16_t);
    gameMenu->g_audioBuffer.Read(samples, count);
}

void init_sdl_audio(double sample_rate) {
    SDL_AudioSpec wanted;
    wanted.freq = (int)sample_rate;
    wanted.format = AUDIO_S16SYS;	// 16 bits nativos
    wanted.channels = 2;			// Estéreo
    wanted.samples = audio_samples; // Tamaño del bloque (latencia)
    wanted.callback = sdl_audio_callback;

    if (SDL_OpenAudio(&wanted, NULL) < 0) {
        fprintf(stderr, "Error SDL Audio: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudio(0); // Inicia el audio
}

std::string initPathAndLog(char** argv){
	//Needed to init the log subsistem
	logger = new Logger(LOG_PATH);
    dirutil dir;
	std::string appDir;

#ifdef _XBOX
	appDir = dir.getDirActual();
	Constant::setAppDir(appDir);
#else
    appDir = argv[0];
    std::size_t pos = appDir.rfind(Constant::getFileSep());
    if (pos == string::npos){
        pos = appDir.rfind(Constant::FILE_SEPARATOR_UNIX);
        #if defined(WIN) || defined(DOS)
            appDir = Constant::replaceAll(appDir, "/", "\\");
        #elif UNIX
            Constant::FILE_SEPARATOR = Constant::FILE_SEPARATOR_UNIX;
            Constant::tempFileSep[0] = Constant::FILE_SEPARATOR_UNIX;
        #endif
    }
    appDir = appDir.substr(0, pos);

    if (!dir.dirExists(appDir.c_str()) || pos == string::npos){
        appDir = dir.getDirActual();
    }
    Constant::setAppDir(appDir);
#endif
    return appDir;
}

/**
 * 
 */
void updateMenuScreen(TileMap &tileMap, ListMenu &listMenu, GameMenu* gameMenu, bool keypress){
	Uint32 bkgText = SDL_MapRGB(gameMenu->screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);

    if (listMenu.animateBkg) 
		tileMap.draw(gameMenu->video_page);
    else 
		SDL_FillRect(gameMenu->video_page, NULL, bkgText);
    

    gameMenu->refreshScreen(listMenu);

    static uint32_t lastTime = SDL_GetTicks();
    if (SDL_GetTicks() - lastTime > bkgFrameTimeTick && (lastTime = SDL_GetTicks()) > 0){
        tileMap.speed++;
    }
}

void initializeMenus(ListMenu &menuData, GameMenu &gameMenu, CfgLoader &cfgLoader){
    struct ListStatus menuBeforeExit;
    int retMenu = gameMenu.recoverGameMenuPos(menuData, menuBeforeExit);
    if (retMenu == 0){
        if (menuBeforeExit.layout != menuData.layout){
            menuData.setLayout(menuBeforeExit.layout, gameMenu.screen->w, gameMenu.screen->h);
        }
        menuData.animateBkg = menuBeforeExit.animateBkg;
    }
    gameMenu.loadEmuCfg(menuData);
    if (retMenu == 0 && menuData.maxLines == menuBeforeExit.maxLines){
        menuData.iniPos = menuBeforeExit.iniPos;
        menuData.endPos = menuBeforeExit.endPos;
        menuData.curPos = menuBeforeExit.curPos;
    }
    
    gameMenu.createMenuImages(menuData);
    menuData.keyUp = true;
}

int main(int argc, char *argv[]) {
	initPathAndLog(argv);

	CfgLoader cfgLoader;
	if (cfgLoader.isDebug()){
        logger->errorLevel = L_DEBUG;
    }

	gameMenu = new GameMenu(&cfgLoader);
	ListMenu listMenu(gameMenu->screen->w, gameMenu->screen->h);
	listMenu.setLayout(LAYBOXES, gameMenu->screen->w, gameMenu->screen->h);
	
	if (!gameMenu->initDblBuffer(cfgLoader.getWidth(), cfgLoader.getHeight())){
		LOG_ERROR("Could not create bitmap");
        return 1;
    }

	Constant::drawTextCent(gameMenu->screen, Fonts::getFont(Fonts::FONTSMALL), "Loading games...", 0,0, 
					true, true, textColor, 0);
	SDL_Flip(gameMenu->screen);

	initializeMenus(listMenu, *gameMenu, cfgLoader);
	TileMap tileMap(9, 0, 16, 16);
    tileMap.load(Constant::getAppDir() + "/assets/art/bricks2.png");
	
	//Callback de environment
	retro_set_environment(retro_environment);
	//Registrar callback de video
    retro_set_video_refresh(retro_video_refresh);
	//Registrar los callbacks de inputs
    retro_set_input_poll(retro_input_poll);
    retro_set_input_state(retro_input_state);
	// Registrar los callbacks de audio
	retro_set_audio_sample(retro_audio_sample);
	retro_set_audio_sample_batch(retro_audio_sample_batch);

	/*
    retro_init();
	std::string rompath;
	#ifdef _XBOX
		rompath = "game:\\roms\\sonicmd.gen";
	#else 
		rompath = argv[1];
	#endif

	// Carga manual mínima para que el core tenga datos que procesar
	FILE *f = fopen(rompath.c_str(), "rb");

	if (!f){
		retro_log_printf(RETRO_LOG_ERROR, "Error abriendo rom");
		return 1;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);
	void *buffer = malloc(size);
	fread(buffer, 1, size, f);
	fclose(f);

	struct retro_game_info game = { rompath.c_str(), buffer, (std::size_t)size, NULL };

    // Es importante cargar la ROM antes de retro_run
    if(!retro_load_game(&game)) {
        printf("Error cargando la ROM\n");
        return 1;
    }

	// Antes de cargar el juego, el core te dirá su frecuencia en retro_get_system_av_info
	struct retro_system_av_info av_info;
	retro_get_system_av_info(&av_info);

	// Inicializar SDL Audio con la frecuencia del core
	init_sdl_audio(av_info.timing.sample_rate);
	//Iniciando el contador de fps
	gameMenu->sync.init_fps_counter(av_info.timing.fps);
	*/

	// D. Renderizado de Texto
	SDL_Rect rect = {0, video_height - 30, 120, 30};
	Uint32 bkgText = SDL_MapRGB(gameMenu->screen->format, 40, 40, 40);
	double nextFrameTime = (double)SDL_GetTicks();

    while (gameMenu->running) {
		if (gameMenu->sync.g_sync == SYNC_TO_VIDEO){
			// El tiempo en el que DEBERÍA empezar este frame
			nextFrameTime += gameMenu->sync.frameDelay;
		}

		// retro_run hace todo: 
		// 1. Llama a input_poll() -> update_input()
		// 2. Calcula la lógica del juego
		// 3. Llama a audio_batch() -> (Aquí el audio bloquea si va muy rápido)
		// 4. Llama a video_refresh() -> (Aquí se dibuja el frame y los FPS)
        //retro_run();

		updateMenuScreen(tileMap, listMenu, gameMenu, false);

		// Actualizamos el contador de media de fps
		SDL_FillRect(gameMenu->screen, &rect, bkgText);
		gameMenu->sync.update_fps_counter();
		Constant::drawText(gameMenu->screen, Fonts::getFont(Fonts::FONTSMALL), gameMenu->sync.fpsText, 0, video_height - 30, white, 0);
		
		SDL_Flip(gameMenu->screen);
		//SDL_UpdateRect(screen, 0, 0, 0, 0);
		
		// --- LIMITADOR ---
		if (gameMenu->sync.g_sync == SYNC_TO_VIDEO){
			gameMenu->sync.limit_fps(nextFrameTime);
		}
    }

	delete logger;
    retro_deinit();
	gameMenu->stopEngine();
    return 0;
}