#pragma once

//Evita errores al usar el min o max de windows.h al incluir el filtro "io/xbrz/xbrz.h"
#define NOMINMAX 

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
#include "const/menuconst.h"
#include <map>

GameMenu *gameMenu;
Logger *logger;

// 1. Usa un buffer persistente para evitar allocs constantes al convertir desde ARGB8888
enum retro_pixel_format fmt;
static uint16_t* conversion_buffer = NULL;
static std::size_t buffer_size = 0;

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

struct retro_core_variable {
   const char *key;    // Nombre técnico: "nestopia_region"
   const char *value;  // Nombre visual y opciones: "Region; Auto|NTSC|PAL"
};

void S9xTextMode( void)
{
}

void S9xGraphicsMode ()
{
}


std::map<std::string, std::string> defaultCoreValues;
const std::string Constant::MAME_SYS_ID = "75";
const std::string Constant::WHITESPACE = " \n\r\t";

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
            // El core envía un puntero al formato que desea usar
            enum retro_pixel_format *fmtAsked = (enum retro_pixel_format*)data;
			std::string msgformat = "Solicitando pixelformat: " + Constant::intToString(*fmtAsked) + "\n";
			LOG_DEBUG("Solicitando pixelformat %d\n", *fmtAsked);
			
			fmt = *fmtAsked;

			if (*fmtAsked == RETRO_PIXEL_FORMAT_XRGB8888){
				// Forzamos a que sea RGB565 (16 bits)
				// Esto le dice a Nestopia: "Solo acepto 16 bits, configúrate así"
				*fmtAsked = RETRO_PIXEL_FORMAT_RGB565;
			} else if (*fmtAsked != RETRO_PIXEL_FORMAT_RGB565){
				return false;
			}
            // Retornamos true para confirmar que aceptamos el formato
            return true;
        }

		case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
			// Aquí le decimos al núcleo que el frontend SÍ puede duplicar frames.
			*(bool*)data = true; 
			return true;
		}

		case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
			return true;

		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:{
			//static const char *dir = "."; // Punto (.) indica el directorio actual del ejecutable

			static string dir;
			gameMenu->getCfgLoader()->configMain[cfg::libretrosystem].getPropValue(dir);

			// O se puede usar una ruta específica de la Xbox 360 si la tienes definida, ej: "game:\\system"
			*(const char**)data = dir.c_str();
			return true;
		}

		case RETRO_ENVIRONMENT_SET_GEOMETRY:{
			/*const struct retro_game_geometry *geom = (const struct retro_game_geometry*)data;
			// 1. Calcular el nuevo tamaño necesario (asumiendo conversión a 16 bits)
			std::size_t needed = geom->max_width * geom->max_height * sizeof(uint16_t);

			// 2. Solo redimensionar si el buffer actual es pequeño o no existe
			if (!conversion_buffer || buffer_size < needed) {
				//Solo estamos en este caso, si tenemos que hacer conversion, asi que actualizamos el campo
				fmt = RETRO_PIXEL_FORMAT_XRGB8888;
				// En Xbox 360, es preferible liberar y asignar para asegurar alineación
				if (conversion_buffer) 
					free(conversion_buffer);
        
				conversion_buffer = (uint16_t*)malloc(needed);
				buffer_size = needed;

				

				// Limpiar el buffer una vez para evitar basura visual
				if (conversion_buffer) memset(conversion_buffer, 0, needed);
				LOG_DEBUG("Buffer de conversión redimensionado en SET_GEOMETRY\n");
			}*/
			return true;
		}
		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
			static std::string dir;
			gameMenu->getCfgLoader()->configMain[cfg::libretro_save].getPropValue(dir);
			*(const char**)data = dir.c_str();
			return true;
		}
		case RETRO_ENVIRONMENT_SET_VARIABLES:
		{
			const struct retro_core_variable *vars = (const struct retro_core_variable*)data;
			while (vars->key) 
			{
				std::string rawValue = vars->value;
				std::size_t semiPos = rawValue.find(';');
        
				if (semiPos != std::string::npos) 
				{
					// 1. Extraemos todo lo que hay después del ';'
					std::string optionsPart = rawValue.substr(semiPos + 1);
            
					// 2. El valor por defecto es lo que hay hasta el primer '|'
					std::size_t pipePos = optionsPart.find('|');
					std::string defaultValue;
            
					if (strcmp(vars->key, "vbanext_frameskip") == 0){
						defaultValue = "1";
					} else if (pipePos != std::string::npos)
						defaultValue = optionsPart.substr(0, pipePos);
					else
						defaultValue = optionsPart; // Solo había una opción

					// 3. Ahora puedes guardar defaultValue en tu configMain
					// para que sea el valor inicial si el usuario no tiene uno guardado.
					LOG_INFO("Opcion: %s, Por defecto: %s, rawValue: %s\n", vars->key, defaultValue.c_str(), rawValue.c_str());
					defaultCoreValues.insert(std::make_pair(vars->key, defaultValue));
				}
				vars++;
			}
			return true;
		}

		case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
		{
			const struct retro_subsystem_info *info = (const struct retro_subsystem_info*)data;
    
			// 1. Limpiar lista de subsistemas previa
			//gameMenu->clearSubsystems();

			// 2. Iterar y guardar
			while (info->ident) {
				//gameMenu->registerSubsystem(info);
				LOG_DEBUG("variable - %s:, %s, %d, %d, %s, %s \n", info->desc, info->ident, info->id, info->num_roms, info->roms->valid_extensions, info->roms->desc);
				info++;
			}
			return true;
		}
		case RETRO_ENVIRONMENT_GET_VARIABLE:{
			struct retro_variable *var = (struct retro_variable*)data;
			/**TODO: Cambiar para que se ajuste por sistema y no en el configmain*/
			//Esto claramente es una opcion de core de nes
			if (strcmp(var->key, "nestopia_favored_system") == 0){
				// Obtener el valor de tu configuración (p. ej. "pal", "ntsc" o "auto")
				// Es vital que el string sea exactamente lo que el core espera
				static std::string region;
				gameMenu->getCfgLoader()->configMain[cfg::region].getPropValue(region);
				var->value = region.c_str(); 
				return true;
			} else if (strcmp(var->key, "nestopia_nospritelimit") == 0){
				static std::string nospritelimit;
				gameMenu->getCfgLoader()->configMain[cfg::nospritelimit].getPropValue(nospritelimit);
				var->value = nospritelimit.c_str(); 
				return true;
			} else if (strcmp(var->key, "snes9x_overclock_superfx") == 0){
				static std::string superfx;
				gameMenu->getCfgLoader()->configMain[cfg::snes_fx].getPropValue(superfx);
				var->value = superfx.c_str(); 
				return true;
			} else {
				if (defaultCoreValues.find(var->key) != defaultCoreValues.end()) {
					var->value = defaultCoreValues[var->key].c_str();
					LOG_INFO("Asignando %s a %s\n", var->key, var->value);
				} else {
					LOG_INFO("Preguntando por: %s sin valor\n", var->key);
				}
			}
			return false;
		}

		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
			// Solo devolvemos true si el usuario ha tocado algo en el menú
			// de la Xbox 360 recientemente.
			bool *updated = (bool*)data;
			//*updated = gameMenu->options_changed_flag;
    
			// IMPORTANTE: Una vez que el core sabe que hubo un cambio, 
			// reseteamos el flag para que en el siguiente frame no vuelva a procesar todo.
			//gameMenu->options_changed_flag = false;
			
			*updated = false;
			return true;
		}
		default: {
			if (cmd < 65583){
				LOG_DEBUG("Comando no tratado: %s\n", Constant::TipoToStr(cmd).c_str());
			}
			// Para comandos desconocidos como 52 o 65587
			return false; 
		}
		    
    }
    return false; // Por defecto devolver false para comandos desconocidos
}

static void retro_video_refresh(const void *data, unsigned width, unsigned height, std::size_t pitch) {
    if (!data || width == 0 || height == 0) 
		return;	

    void* final_src = (void*)data;

	//Hacemos la comprobacion del pitch >= width * 4, por si hemos solicitado el RETRO_PIXEL_FORMAT_RGB565
	//pero el core no lo acepta
    //if ((fmt == RETRO_PIXEL_FORMAT_XRGB8888 || pitch >= width * 4) && conversion_buffer) {
	//if (fmt == RETRO_PIXEL_FORMAT_XRGB8888 || pitch >= width * 4) {
	if (fmt == RETRO_PIXEL_FORMAT_XRGB8888) {
		// 2. Gestionar buffer de conversión de forma eficiente
        std::size_t needed = width * height * sizeof(uint16_t);
        if (!conversion_buffer || buffer_size < needed) {
            conversion_buffer = (uint16_t*)realloc(conversion_buffer, needed);
            buffer_size = needed;
        }
		
		convertARGB8888ToRGB565((uint32_t*)data, width, height, pitch, conversion_buffer, width * 2);
		final_src = (void*)conversion_buffer;
		pitch = width * 2;
	}

    SDL_Surface* screen = gameMenu->screen;

	t_scale_props scaleProps = {0}; // Limpia todo el struct a cero antes de asignar
	// 4. Pasar el buffer correcto (ya sea el original de 16 o el convertido)
	scaleProps.src = (uint16_t*)final_src;
	scaleProps.sw = (int)width;
	scaleProps.sh = (int)height;
	scaleProps.spitch = pitch;
	scaleProps.dst = (uint16_t*)screen->pixels;
	scaleProps.dw = screen->w;
	scaleProps.dh = screen->h;
	scaleProps.dpitch = screen->pitch;
	scaleProps.scale = gameMenu->current_scaler_scale;
	scaleProps.ratio = aspectRatioValues[*gameMenu->current_ratio];
    
    //Escalamos la imagen con el escalador que hay almacenado en el puntero a funcion
    gameMenu->current_scaler(scaleProps);
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
                case SDL_BUTTON_START: gameMenu->joystick->g_joy_state[player][RETRO_DEVICE_ID_JOYPAD_START]  = pressed; break;
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
				//gameMenu->getCfgLoader()->configMain[cfg::syncMode].setPropValue(gameMenu->sync->g_sync_last);
				*gameMenu->current_sync = gameMenu->sync->g_sync_last;
				//gameMenu->processConfigChanges();
				SDL_PauseAudio(0);
			}
		} else if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_BACKSPACE){
				//Enabling fast forward
				//gameMenu->getCfgLoader()->configMain[cfg::syncMode].getPropValue(gameMenu->sync->g_sync_last);
				//gameMenu->getCfgLoader()->configMain[cfg::syncMode].setPropValue(SYNC_NONE);
				//gameMenu->processConfigChanges();
				gameMenu->sync->g_sync_last = *gameMenu->current_sync;
				*gameMenu->current_sync = SYNC_NONE;

				SDL_PauseAudio(1);
			}
		}
    }
	const Uint32 now = SDL_GetTicks();
	//LOG_DEBUG("ticks are: %d\n", SDL_GetTicks() - gameMenu->joystick->lastSelectPress);
	if (gameMenu->joystick->lastSelectPress > 0 && now - gameMenu->joystick->lastSelectPress > LONGKEYTIMEOUT){
		gameMenu->joystick->lastSelectPress = 0;
		gameMenu->setEmuStatus(EMU_MENU);
		gameMenu->joystick->resetAllValues();
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

//Audio Callbacks for Libretro
// Callback para una sola muestra (menos eficiente, pero requerido)
void retro_audio_sample(int16_t left, int16_t right) {
	// Creamos una referencia local al buffer
    // Esto evita que la CPU haga: gameMenu -> buscar g_audioBuffer -> llamar Write
    AudioBuffer& audio = gameMenu->g_audioBuffer;
    const int mode = *gameMenu->current_sync;

    if (mode == SYNC_NONE) return;

    int16_t samples[2] = { left, right };
    audio.Write(samples, 2);
}

// Callback para ráfagas de muestras (el que usan casi todos los cores)
std::size_t retro_audio_sample_batch(const int16_t * __restrict data, std::size_t frames) {
	
	AudioBuffer& audio = gameMenu->g_audioBuffer;
    const int mode = *gameMenu->current_sync;

	if (mode == SYNC_NONE) return 0;
	
	// frames es el número de pares (izq, der), multiplicamos por 2 para el total
	// Al usar WriteBlocking, retro_run() no terminará hasta que haya
    // sitio en el buffer, sincronizando así la ejecución al audio real.
	if (mode == SYNC_TO_AUDIO){
		audio.WriteBlocking(data, frames * 2);
	} else {
		audio.Write(data, frames * 2);
	}
    return frames;
}

void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
    // 1. Limpiar el buffer de salida de SDL (Opcional pero recomendado en SDL 1.2)
    // Esto garantiza que si el emulador se pausa o va lento, haya silencio en lugar de ruido.
    //memset(stream, 0, len);
    // 2. Convertir a puntero de 16 bits para trabajar con muestras
    int16_t* samples = (int16_t*)stream;
    
    // len es el tamaño en bytes. 
    // Como usamos AUDIO_S16SYS (2 bytes por muestra), count es el número de muestras totales.
    std::size_t count = len / sizeof(int16_t);

    // 3. Leer de tu buffer circular
    gameMenu->g_audioBuffer.Read(samples, count);
}

/*void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
    memset(stream, 0, len); // Silencio por defecto
    int16_t* samples = (int16_t*)stream;
    std::size_t count = len / sizeof(int16_t);

    // Comprobamos cuánto hay realmente
    std::size_t disponible = gameMenu->g_audioBuffer.get_readable_samples();
    
    if (disponible >= count) {
        gameMenu->g_audioBuffer.Read(samples, count);
    } else {
        // LOG: "Audio Underflow: %d de %d", disponible, count
        // Si entra aquí mucho, el emulador es LENTO o el buffer es muy pequeño
        gameMenu->g_audioBuffer.Read(samples, disponible);
    }
}*/

/**
*
*/
void init_sdl_audio(double sample_rate) {

	/*	
		El equilibrio de latencia y seguridad
		-------------------------------------
		Buffer Total (AudioBuffer::AUDIO_BUFFER_SIZE = 4096): Tienes un margen de maniobra de unos 92ms (a 44.1kHz). 
			Es suficiente para que Windows haga tareas en segundo plano sin que el audio sufra cortes.
		Bloque SDL (1024): Al pedir 1024 muestras (AudioBuffer::AUDIO_BUFFER_SIZE / 4), la latencia de respuesta de la tarjeta de sonido es de unos 23ms. 
			Es una latencia excelente, casi imperceptible para el oído humano.
		La Proporción (1/4): Al ser el bloque 4 veces más pequeño que el buffer total, el hilo de audio de SDL 
			llamará a tu callback 4 veces antes de vaciar el buffer por completo. Esto da al emulador mucho tiempo 
			para rellenar el head antes de que el tail lo alcance.
	*/

    SDL_AudioSpec wanted;
    wanted.freq = (int)sample_rate;
    wanted.format = AUDIO_S16SYS;	// 16 bits nativos
    wanted.channels = 2;			// Estéreo
	// --- AJUSTE PARA XBOX 360 ---
    // Si escuchas chasquidos (crackling), aumenta este valor.
    // 1024 = ~23ms (Riesgo de cortes en 360)
    // 2048 = ~46ms (Recomendado para estabilidad en emulación)
    // 4096 = ~92ms (Seguro, pero con lag perceptible)
	wanted.samples = AudioBuffer::AUDIO_BUFFER_SIZE / 4; // Tamaño del bloque (latencia)

	//wanted.samples = 1024;
    wanted.callback = sdl_audio_callback;

    if (SDL_OpenAudio(&wanted, NULL) < 0) {
		string error = "Error SDL Audio: " + string(SDL_GetError());
        LOG_ERROR("%s\n", error.c_str());
        return;
    }
    SDL_PauseAudio(0); // Inicia el audio
}

/**
*
*/
std::string initPathAndLog(char** argv){
	logger = new Logger("salviajournal.log");

	dirutil dir;

	#if defined(WIN) || defined(DOS) || defined(_XBOX)
		Constant::tempFileSep[0] = '\\';
	#else if defined(UNIX)
		Constant::tempFileSep[0] = '/';
	#endif

	#ifdef _XBOX
		Constant::setAppDir(dir.getDirActual() + string(EMU_LIB_NAME) + ".xex");
	#else
		Constant::setAppDir(argv[0]);
	#endif	

	std::size_t pos = Constant::getAppDir().rfind(Constant::getFileSep());
	if (pos != string::npos && pos < Constant::getAppDir().length()){
		Constant::setAppExecutable(Constant::getAppDir().substr(pos + 1));
	}

    Constant::setAppDir(Constant::getAppDir().substr(0, pos));
    if (!dir.dirExists(Constant::getAppDir().c_str()) || pos == string::npos){
        Constant::setAppDir(dir.getDirActual());
    }

	Constant::setExecMethod(launch_spawn);

	LOG_INFO("Directorio de app: %s\n", Constant::getAppDir().c_str());
	LOG_INFO("Ejecutable: %s\n", Constant::getAppExecutable().c_str());

	return Constant::getAppDir();
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
		ConfigEmu *emu = gameMenu->getCfgEmu();

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
			
			unzippedFileInfo unzipedFileInfo = unzipTool.descomprimirZip(rompath.c_str(), destDir.c_str(), "romToLoad"); 
			for (unsigned int i=0; i < unzipedFileInfo.files.size(); i++){
				FileProps fileprop = unzipedFileInfo.files.at(i);
				std::string ext = Constant::replaceAll(fileprop.extension, ".", "");
				if (emu->rom_extension.find(ext) != std::string::npos){
					ret.romsize = fileprop.fileSize;
					ret.rutaEscritura = fileprop.dir + Constant::getFileSep() + fileprop.filename;
					ret.errorCode = UNZ_OK;
					break;
				}
			}
		} else {
			ret = unzipTool.descomprimirZipToMem(rompath, emu->rom_extension, buffer); 
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
		ret.errorCode = UNZ_OK;
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

	if (unzipped.errorCode != UNZ_OK){
		LOG_ERROR("No se ha podido abrir el fichero o no se puede descomprimir: %s\n", rompath.c_str());
		return 0;
	}
		
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

	//Obtener el aspect ratio
	aspectRatioValues[RATIO_CORE] = av_info.geometry.aspect_ratio;

	// Inicializar SDL Audio con la frecuencia del core
	init_sdl_audio(av_info.timing.sample_rate);
	//Iniciando el contador de fps
	gameMenu->sync->init_fps_counter(av_info.timing.fps);
	gameMenu->romLoaded = true;
	gameMenu->setEmuStatus(EMU_STARTED);
	return 1;
}

/**
 * 
 */
void updateMenuScreen(TileMap &tileMap, GameMenu &gameMenu, ListMenu &listMenu, bool keypress){
	static Uint32 bkgText = SDL_MapRGB(gameMenu.screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
	tEvento askEvento;
	//Procesamos los controles de la aplicacion
	askEvento = gameMenu.WaitForKey();

	if (askEvento.isJoy){
		ConfigEmu *emu = gameMenu.getCfgEmu();
		if (listMenu.getNumGames() == 0 && emu->generalConfig){
			//Opciones para modificar el menu de configuracion
			if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_UP)){
				gameMenu.configMenus->prevPos();
			} else if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_DOWN)){
				gameMenu.configMenus->nextPos();
			} else if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_LEFT)){
				gameMenu.configMenus->cambiarValor(-1);
			} else if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_RIGHT)){
				gameMenu.configMenus->cambiarValor(1);
			} 

			if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_A)){
				gameMenu.configMenus->confirmar();
			} else if(askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_B)){
				gameMenu.configMenus->volver();
			}

			if (askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_A) || askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_LEFT) 
				|| askEvento.joy == JoyMapper::getJoyMapper(JOY_BUTTON_RIGHT)){
				gameMenu.processConfigChanges();
			}
		} else {
			//Opciones para seleccionar una rom para el emulador
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
				vector<string> launchCommand = gameMenu.launchProgram(listMenu);
				if (launchCommand.size() > 1){
					SDL_FillRect(gameMenu.screen, NULL, bkgText);
					if (launchGame(launchCommand.at(1))){
						gameMenu.setEmuStatus(EMU_STARTED);
					}
					gameMenu.joystick->resetAllValues();
					gameMenu.joystick->lastSelectPress = 0;
					return;
				}
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

		if (askEvento.longKeyPress[JoyMapper::getJoyMapper(JOY_BUTTON_SELECT)] && gameMenu.getLastStatus() == EMU_STARTED){
			LOG_DEBUG("Detectada pulsacion larga del select\n");
			gameMenu.setEmuStatus(EMU_STARTED);
			SDL_FillRect(gameMenu.video_page, NULL, gameMenu.uBkgColor);
			gameMenu.joystick->resetAllValues();
			return;
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

// En tu función de salida o desinicialización
void cerrar_emulador() {
    if (conversion_buffer != NULL) {
        free(conversion_buffer);
        conversion_buffer = NULL; // Importante ponerlo a NULL tras liberar
        buffer_size = 0;
    }

	delete logger;
    retro_deinit();
	delete gameMenu;
}

bool loadGameAtStart(int argc, char *argv[]){
	LOG_DEBUG("argc: %d\n", argc);
	bool ret = false;

	#ifdef _XBOX
		DWORD dwLaunchDataSize = 0;    
		DWORD dwStatus = XGetLaunchDataSize( &dwLaunchDataSize );
		if( dwStatus == ERROR_SUCCESS ){
			BYTE* pLaunchData = new BYTE [ dwLaunchDataSize ];
			dwStatus = XGetLaunchData( pLaunchData, dwLaunchDataSize );
			char* mensaje = (char*)pLaunchData;
			ret = launchGame(mensaje);
			LOG_DEBUG("Parametros recibidos: %s\n", mensaje);
		} else if (dwStatus == ERROR_NOT_FOUND) {
			// El programa se lanzó normalmente (sin XSetLaunchData)
			LOG_DEBUG("No se encontraron datos de lanzamiento.\n");
		}
	#else 
		if (argc > 1){
			LOG_DEBUG("argv[1]: %s\n", argv[1]);
			ret = launchGame(argv[1]);
		}
	#endif	

	if (ret){
		gameMenu->setEmuStatus(EMU_STARTED);
		gameMenu->joystick->resetAllValues();
		gameMenu->joystick->lastSelectPress = 0;
	}
	
	return ret;
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

	LOG_DEBUG("appdir: %s\n", Constant::getAppDir().c_str());
	LOG_DEBUG("appexe: %s\n", Constant::getAppExecutable().c_str());

	gameMenu = new GameMenu(&cfgLoader);
	ListMenu listMenu(gameMenu->screen->w, gameMenu->screen->h);
	listMenu.setLayout(LAYBOXES, gameMenu->screen->w, gameMenu->screen->h);
	
	if (!gameMenu->initDblBuffer(cfgLoader.getWidth(), cfgLoader.getHeight())){
		LOG_ERROR("Could not create bitmap");
        return 1;
    }

	Constant::drawTextCent(gameMenu->screen, Fonts::getFont(Fonts::FONTSMALL), "Loading games...", 0,0, true, true, textColor, 0);
	SDL_Flip(gameMenu->screen);

	TileMap tileMap(9, 0, 16, 16);
    tileMap.load(Constant::getAppDir() + Constant::getFileSep() + "assets" + Constant::getFileSep() + "art" + Constant::getFileSep() + "bricks2.png");
	initializeMenus(listMenu, *gameMenu, cfgLoader);
	
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

	if (!loadGameAtStart(argc, argv)){
		//Workaround para mostrar una primera imagen del menu con las imagenes cargadas
		listMenu.keyUp = true;
		updateMenuScreen(tileMap, *gameMenu, listMenu, false);
		SDL_Flip(gameMenu->screen);
		listMenu.keyUp = false;
	}

	double nextFrameTime = (double)SDL_GetTicks();
    while (gameMenu->running) {
		
		// Procesamos eventos como pulsaciones de hotkeys
		gameMenu->processFrontendEvents();

		if (gameMenu->getEmuStatus() == EMU_STARTED){
			// retro_run hace todo: 
			// 1. Llama a input_poll() -> update_input()
			// 2. Calcula la lógica del juego
			// 3. Llama a audio_batch() -> (Aquí el audio bloquea si va muy rápido)
			// 4. Llama a video_refresh() -> (Aquí se dibuja el frame y los FPS)
			retro_run();
		} else if (gameMenu->getEmuStatus() == EMU_MENU){
			updateMenuScreen(tileMap, *gameMenu, listMenu, false);
		}

		// DIBUJO DE INTERFAZ (OSD, FPS, Mensajes)
        gameMenu->processFrontendEventsAfter();

		// Actualizamos la pantalla
		SDL_Flip(gameMenu->screen);

		const int mode = *gameMenu->current_sync;
		// Limitamos los frames si tenemos que sincronizar con el video
		if (mode == SYNC_TO_VIDEO){
			gameMenu->sync->limit_fps(nextFrameTime);
			// El tiempo en el que DEBERÍA empezar el siguiente frame
			nextFrameTime += gameMenu->sync->frameDelay;
		}
    }
	cerrar_emulador();
    return 0;
}