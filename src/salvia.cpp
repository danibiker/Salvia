#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <string>

#include "gameMenu.h"
#include "io/joymapper.h"
#include "io/video.h"
#include "io/cfgloader.h"
#include "uiobjects/listmenu.h"
#include "uiobjects/tilemap.h"
#include "unzip/unziptool.h"


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
	void retro_unload_game(void);
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
    va_list v; va_start(v, fmt); vfprintf(stdout, fmt, v); va_end(v);
	
	/*if (!logger) {
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
    logger->write(myLevel, "[CORE] %s", buffer);*/

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

			if (fmt = RETRO_PIXEL_FORMAT_RGB565){
				return true; 
			} else {
				return false;
			}
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
		//scale_software_fixed_point((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
		#ifdef WIN
			scale_software_fixed_point_sse2((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
		#elif defined(_XBOX)
			scale_software_fixed_point_ppc((uint16_t*)data, (uint16_t*)gameMenu->screen->pixels, width, height, pitch, gameMenu->screen->w, gameMenu->screen->h, gameMenu->screen->pitch);
		#endif

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
			LOG_ERROR("Estado cargado con éxito.\n");
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
                case SDL_BUTTON_A: gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_A] = pressed; break;
                case SDL_BUTTON_B: gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_B] = pressed; break;
                case SDL_BUTTON_X: gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_X] = pressed; break;
                case SDL_BUTTON_Y: gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_Y] = pressed; break;
                case SDL_BUTTON_START:  gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_START]  = pressed; break;
                case SDL_BUTTON_SELECT: 
					if (pressed && !gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_SELECT]){
						gameMenu->joystick->lastSelectPress = SDL_GetTicks();
						LOG_DEBUG("Setting the ticks on: %d\n", gameMenu->joystick->lastSelectPress);
					} else if (!pressed){
						gameMenu->joystick->lastSelectPress = 0;
						LOG_DEBUG("Resseting ticks\n");
					}
					gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_SELECT] = pressed; 
					break;
            }
        }
        
        // Manejo de la cruceta (D-PAD) mediante Ejes o Hats
        if (event.type == SDL_JOYHATMOTION) {
            gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_UP]    = (event.jhat.value & SDL_HAT_UP) > 0;
            gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_DOWN]  = (event.jhat.value & SDL_HAT_DOWN) > 0;
            gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_LEFT]  = (event.jhat.value & SDL_HAT_LEFT) > 0;
            gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_RIGHT] = (event.jhat.value & SDL_HAT_RIGHT) > 0;
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

	//LOG_DEBUG("ticks are: %d\n", SDL_GetTicks() - gameMenu->joystick->lastSelectPress);
	if (gameMenu->joystick->lastSelectPress > 0 && SDL_GetTicks() - gameMenu->joystick->lastSelectPress > 2000){
		gameMenu->joystick->lastSelectPress = 0;
		gameMenu->setEmuStatus(EMU_MENU);
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
    return gameMenu->joystick->g_joy_state[port][id] ? 1 : 0;
}



//Audio callbacks for SDL
void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
    // SDL pide bytes, pero trabajamos con muestras de 16 bits (2 bytes)
    int16_t* samples = (int16_t*)stream;
    std::size_t count = len / sizeof(int16_t);
    gameMenu->g_audioBuffer.Read(samples, count);
}

/**
*
*/
void init_sdl_audio(double sample_rate) {
    SDL_AudioSpec wanted;
    wanted.freq = (int)sample_rate;
    wanted.format = AUDIO_S16SYS;	// 16 bits nativos
    wanted.channels = 2;			// Estéreo
    wanted.samples = audio_samples; // Tamaño del bloque (latencia)
    wanted.callback = sdl_audio_callback;

    if (SDL_OpenAudio(&wanted, NULL) < 0) {
        LOG_ERROR("Error SDL Audio: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudio(0); // Inicia el audio
}

/**
*
*/
std::string initPathAndLog(char** argv){
	//Needed to init the log subsistem
	logger = new Logger(LOG_PATH);
    dirutil dir;
	std::string appDir;

#ifdef _XBOX
	appDir = dir.getDirActual();
#else
    appDir = argv[0];
#endif

	std::size_t pos = appDir.rfind(Constant::getFileSep());
    if (pos == string::npos){
        pos = appDir.rfind(Constant::FILE_SEPARATOR_UNIX);
        #if defined(WIN) || defined(DOS) 
            appDir = Constant::replaceAll(appDir, "/", "\\");
		#elif defined(_XBOX)
			if (pos == string::npos){
				pos = appDir.rfind(Constant::FILE_SEPARATOR);
			}
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
    return appDir;
}

/**
*
*/
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
}

/**
*
*/
unzippedFileInfo unzipOrLoadFile(std::string rompath, void*& buffer){
	std::string rompathLow = rompath;
	unzippedFileInfo ret;
	ret.errorCode = -1;
    ret.rutaEscritura = "";
    ret.romsize = 0;
    ret.nFilesInZip = 0;
    ret.nFilesWritten = 0;

	dirutil dir;
	Constant::lowerCase(&rompathLow);
	
	if (rompathLow.find(".zip") != std::string::npos){
		UnzipTool unzipTool;
		ConfigEmu emu = gameMenu->getCfgEmu();

		//Llamada al core para saber si se requiere el path completo
		struct retro_system_info info;
		memset(&info, 0, sizeof(info));
		retro_get_system_info(&info);

		if (info.need_fullpath) {
			//No nos queda otra que descomprimir a algun directorio temporal porque el core lo requiere
			std::string destDir = Constant::getAppDir() + Constant::getFileSep() + "tmp";

			if (dir.dirExists(destDir.c_str())){
				dir.borrarDir(destDir);
			}

			if (!dir.dirExists(destDir.c_str()) && dir.createDir(destDir) < 0){
				LOG_ERROR("No se ha podido crear el directorio %s\n", destDir.c_str());
				return ret;
			}
			
			unzippedFileInfo unzipedFileInfo = unzipTool.descomprimirZip(rompath.c_str(), destDir.c_str()); 
			ConfigEmu emu = gameMenu->getCfgEmu();

			for (unsigned int i=0; i < unzipedFileInfo.files.size(); i++){
				FileProps fileprop = unzipedFileInfo.files.at(i);
				std::string ext = Constant::replaceAll(fileprop.extension, ".", "");
				if (emu.rom_extension.find_first_of(ext) != std::string::npos){
					ret.romsize = fileprop.fileSize;
					ret.rutaEscritura = fileprop.dir + Constant::getFileSep() + fileprop.filename;
					break;
				}
			}
		} else {
			ret = unzipTool.descomprimirZipToMem(rompath, emu.rom_extension, buffer); 
		}

	} else {
		FILE *f = fopen(rompath.c_str(), "rb");
		if (!f){
			LOG_ERROR("Error abriendo rom %s\n", rompath.c_str());
			return ret;
		}

		fseek(f, 0, SEEK_END);
		std::size_t size = ftell(f);
		rewind(f);

		// Usar memoria alineada si es posible
		buffer = malloc(size); 
		if (!buffer) { fclose(f); return ret; }
		fread(buffer, 1, size, f);
		fclose(f);
		
		ret.romsize = size;
		ret.rutaEscritura = rompath;
	}
	return ret;
}

/**
*
*/
int launchGame(std::string rompath){
	if (gameMenu->romLoaded){
		// 1. Pausar el procesamiento de audio para detener el hilo de callback
		SDL_PauseAudio(1);
		// 2. Cerrar el dispositivo y liberar el hardware
		SDL_CloseAudio();
		SDL_Delay(10);
		//Liberar recursos de libretro
		retro_unload_game();
	}
	gameMenu->romLoaded = false;
	
	void* buffer = NULL;
	unzippedFileInfo unzipped = unzipOrLoadFile(rompath, buffer);
	// Carga manual mínima para que el core tenga datos que procesar
		
	struct retro_game_info game = { unzipped.rutaEscritura.c_str(), buffer, unzipped.romsize, NULL };
	bool success = retro_load_game(&game);
	//Liberar la memoria tras la carga exitosa
	// La mayoría de los cores de Libretro ya han copiado los datos a su propia RAM interna
	free(buffer); 

	// Es importante cargar la ROM antes de retro_run
	if(!success) {
		LOG_ERROR("Error cargando la ROM\n");
		return 0;
	}

	// Antes de cargar el juego, el core dice su frecuencia en retro_get_system_av_info
	struct retro_system_av_info av_info;
	retro_get_system_av_info(&av_info);

	// Inicializar SDL Audio con la frecuencia del core
	init_sdl_audio(av_info.timing.sample_rate);
	//Iniciando el contador de fps
	gameMenu->sync.init_fps_counter(av_info.timing.fps);
	gameMenu->romLoaded = true;
	gameMenu->setEmuStatus(EMU_STARTED);
	return 1;
}

/**
 * 
 */
void updateMenuScreen(TileMap &tileMap, GameMenu &gameMenu, ListMenu &listMenu, bool keypress){
	Uint32 bkgText = SDL_MapRGB(gameMenu.screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
	tEvento askEvento;
	//Procesamos los controles de la aplicacion
	askEvento = gameMenu.WaitForKey();

	if (askEvento.isJoy){
		if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_UP)){
			listMenu.prevPos();
		} else if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_DOWN)){
			listMenu.nextPos();
		} else if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_LEFT)){
			listMenu.prevPage();
		} else if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_RIGHT)){
			listMenu.nextPage();
		} 

		if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_A)){
			vector<string> game = gameMenu.launchProgram(listMenu);
			if (game.size() > 1){
				SDL_FillRect(gameMenu.screen, NULL, bkgText);
				if (launchGame(game.at(1))){
					gameMenu.setEmuStatus(EMU_STARTED);
				}
				gameMenu.joystick->resetAllValues();
				gameMenu.joystick->lastSelectPress = 0;
				return;
			}
		} 

		if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_R)){
            //Change to prev emulator
            //sound.play(SBTNCLICK);
            //gameMenu.showMessage("Refreshing gamelist...");
            gameMenu.getNextCfgEmu();
            gameMenu.loadEmuCfg(listMenu);
        }
        if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_L)){
            //Change to next emulator
            //sound.play(SBTNCLICK);
            //gameMenu.showMessage("Refreshing gamelist...");
            gameMenu.getPrevCfgEmu();
            gameMenu.loadEmuCfg(listMenu);
        } 
	}

	if (askEvento.quit){
		gameMenu.running = false; // Marcamos para salir
	}


    if (listMenu.animateBkg) 
		tileMap.draw(gameMenu.video_page);
    else 
		SDL_FillRect(gameMenu.video_page, NULL, bkgText);

    gameMenu.refreshScreen(listMenu);

    static uint32_t lastTime = SDL_GetTicks();
    if (SDL_GetTicks() - lastTime > bkgFrameTimeTick && (lastTime = SDL_GetTicks()) > 0){
        tileMap.speed++;
    }
}

/**
*
*/
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

	TileMap tileMap(9, 0, 16, 16);
    tileMap.load(Constant::getAppDir() + Constant::getFileSep() + "assets" + Constant::getFileSep() + "art" + Constant::getFileSep() + "bricks2.png");
	initializeMenus(listMenu, *gameMenu, cfgLoader);

	//Workaround para mostrar una primera imagen del menu con las imagenes cargadas
	listMenu.keyUp = true;
	updateMenuScreen(tileMap, *gameMenu, listMenu, false);
	SDL_Flip(gameMenu->screen);
	listMenu.keyUp = false;
	
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

	retro_init();	

	double nextFrameTime = (double)SDL_GetTicks();
    while (gameMenu->running) {
		if (gameMenu->sync.g_sync == SYNC_TO_VIDEO){
			// El tiempo en el que DEBERÍA empezar el siguiente frame
			nextFrameTime += gameMenu->sync.frameDelay;
		}

		if (gameMenu->getEmuStatus() == EMU_MENU){
			updateMenuScreen(tileMap, *gameMenu, listMenu, false);
		} else if (gameMenu->getEmuStatus() == EMU_STARTED){
			// retro_run hace todo: 
			// 1. Llama a input_poll() -> update_input()
			// 2. Calcula la lógica del juego
			// 3. Llama a audio_batch() -> (Aquí el audio bloquea si va muy rápido)
			// 4. Llama a video_refresh() -> (Aquí se dibuja el frame y los FPS)
			retro_run();
		}

		// Actualizamos el contador de media de fps
		gameMenu->updateFps();

		// Actualizamos la pantalla
		SDL_Flip(gameMenu->screen);
		
		// Limitamos los frames si tenemos que sincronizar con el video
		if (gameMenu->sync.g_sync == SYNC_TO_VIDEO){
			gameMenu->sync.limit_fps(nextFrameTime);
		}
    }

	delete logger;
    retro_deinit();
	gameMenu->stopEngine();
    return 0;
}