#pragma once

//Evita errores al usar el min o max de windows.h al incluir el filtro "io/xbrz/xbrz.h"
#define NOMINMAX 

#include <SDL.h>
#include <SDL_ttf.h>
#include "SDL_thread.h"

#include <string>
#include <map>

#include <zlib.h>

#include "gameMenu.h"
#include "io/joymapper.h"
#include "io/cfgloader.h"
#include "io/dirutil.h"
#include "uiobjects/listmenu.h"
#include "uiobjects/tilemap.h"
#include "unzip/unziptool.h"
#include "const/menuconst.h"
#include "libretro/libretro.h"
#include "libretro/vfs.h"
#include "statesram.h"
#include "io/inputsmenu.h"
#include "io/inputscore.h"


GameMenu *gameMenu;
Logger *logger;
dirutil dir;

// 1. Usa un buffer persistente para evitar allocs constantes al convertir desde ARGB8888
enum retro_pixel_format fmt;
int launchGame(std::string);
//Maximo de 30 MB. Los CHD que son grandes no debemos cargarlos en memoria. Ya se encarga
//la implementacion de vfs
const int MAX_FILE_LOAD_MEMORY = 1024 * 2014 * 30; 
const int maxJoyTargets = RETRO_DEVICE_ID_JOYPAD_R3 + 1;
const std::string Constant::MAME_SYS_ID = "75";
const std::string Constant::WHITESPACE = " \n\r\t";
volatile uint32_t Constant::totalTicks = 0;
int Constant::EXEC_METHOD = launch_batch;
const std::string CfgLoader::CONFIGFILE = "salvia.cfg";

static uint16_t* conversion_buffer = NULL;
static std::size_t buffer_size = 0;
int audio_opened = 0;

struct retro_core_variable {
   const char *key;    // Nombre técnico: "nestopia_region"
   const char *value;  // Nombre visual y opciones: "Region; Auto|NTSC|PAL"
};

// Ya no declaramos punteros a función, sino que usamos las funciones 
// que vendrán dentro del .lib (se resuelven al linkar)
extern "C" {	
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
	void retro_set_controller_port_device(unsigned port, unsigned device);
	retro_audio_buffer_status_callback_t audio_status_cb;
}

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

		case RETRO_ENVIRONMENT_SET_MESSAGE: {
			// 1. Convertir el puntero 'data' a la estructura de mensaje
			const struct retro_message *msg = (const struct retro_message*)data;
			if (msg && msg->msg){
				LOG_DEBUG("NOTIFICACIÓN DEL CORE: %s (Duración: %u frames)", msg->msg, msg->frames);
				gameMenu->showSystemMessage(msg->msg, (unsigned)((msg->frames * 1000) / 60));
			}
			return true;
        }

		case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
			struct retro_vfs_interface_info* vfs_info = (struct retro_vfs_interface_info*)data;
			// Si el core pide versión 1, 2 o 3, le damos nuestra v3
			if (vfs_info->required_interface_version <= 3) {
				vfs_info->iface = &vfs_interface;
				return true;
			}
			return false;
		}

		case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: {
			const struct retro_disk_control_callback *cb = 
				(const struct retro_disk_control_callback*)data;
    
			if (cb) {
				disk_control = *cb; // Copiamos las funciones que nos da el core
				LOG_DEBUG("Interfaz de control de disco registrada por el core.");
			}
			return true;
		}

		// Es muy probable que en 2026 también te pida la versión extendida (V1)
		case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: {
			const struct retro_disk_control_ext_callback *cb = 
				(const struct retro_disk_control_ext_callback*)data;
    
			if (cb) {
				disk_control_ext = *cb;
				LOG_DEBUG("Interfaz de control de disco extendida registrada.");
			}
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
			LOG_DEBUG("Solicitando pixelformat %d", *fmtAsked);
			
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

		case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
			// Al devolver true, le decimos al core: 
			// "Sí, puedes pedirme todos los botones de golpe".
			return true;

		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:{
			//static const char *dir = "."; // Punto (.) indica el directorio actual del ejecutable

			static string dir;
			gameMenu->getCfgLoader()->configMain[cfg::libretrosystem].getPropValue(dir);

			// O se puede usar una ruta específica de la Xbox 360 si la tienes definida, ej: "game:\\system"
			*(const char**)data = dir.c_str();
			return true;
		}

		case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:{
			const struct retro_system_av_info *av_info = (const struct retro_system_av_info *)data;
			gameMenu->sync->init_fps_counter(av_info->timing.fps);
			return true;
		}

		case RETRO_ENVIRONMENT_SET_GEOMETRY:{
			const struct retro_game_geometry *geom = (const struct retro_game_geometry*)data;
			/*
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
			*(const char**)data = gameMenu->getRomPaths()->saves.c_str();
			return true;
		}

		case RETRO_ENVIRONMENT_GET_LANGUAGE:{
			// El núcleo nos pregunta: "¿En qué idioma quieres las descripciones?"
            unsigned *lang = (unsigned*)data;
			static int g_current_language;
			gameMenu->getCfgLoader()->configMain[cfg::libretro_save].getPropValue(g_current_language);
			*lang = g_current_language;
            LOG_INFO("Core solicitó idioma: Enviando (%d)", g_current_language);
            return true;
		}

		case RETRO_ENVIRONMENT_SET_VARIABLES:
		{
			const struct retro_core_variable *vars = (const struct retro_core_variable*)data;
			// Asumiendo que getLibretroParams() devuelve std::map<std::string, std::unique_ptr<cfg::t_emu_props>>&
			auto& params = gameMenu->getCfgLoader()->getLibretroParams();

			while (vars && vars->key) 
			{
				std::string rawValue = vars->value ? vars->value : "";
				std::size_t semiPos = rawValue.find(';');

				if (semiPos != std::string::npos)
				{
					std::string keyStr = vars->key;
					cfg::t_emu_props* ptr = NULL;

					// 1. Buscar si ya existe
					std::map<std::string, std::unique_ptr<cfg::t_emu_props> >::iterator it = params.find(keyStr);
            
					if (it == params.end()) {
						// Si no existe, crear uno nuevo e insertarlo
						ptr = new cfg::t_emu_props();
						ptr->selected = 0;
						// En VS2010 insertamos usando std::move o pasándolo directamente al constructor del map
						params.insert(std::make_pair(keyStr, std::unique_ptr<cfg::t_emu_props>(ptr)));
					} else {
						// Si existe, usamos el puntero que ya está en el mapa
						ptr = it->second.get();
						ptr->values.clear(); // Limpiar opciones anteriores para no duplicar
					}

					// 2. Parsear descripción y opciones
					ptr->description = rawValue.substr(0, semiPos);
					std::string optionsPart = rawValue.substr(semiPos + 1);
            
					std::vector<std::string> valuesList = Constant::splitChar(optionsPart, '|');
					for (std::size_t i = 0; i < valuesList.size(); ++i) {
						ptr->values.push_back(Constant::Trim(valuesList[i]));
					}

					LOG_INFO("Opcion: %s = %s", keyStr.c_str(), ptr->values[0].c_str());

					// 3. Sincronizar con startupLibretroParams (Copia de CONTENIDO)
					// IMPORTANTE: Creamos un objeto nuevo copiando los datos del original
					cfg::t_emu_props* copyPtr = new cfg::t_emu_props(*ptr); 
					gameMenu->getCfgLoader()->startupLibretroParams[keyStr] = std::unique_ptr<cfg::t_emu_props>(copyPtr);
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
				LOG_DEBUG("variable - %s:, %s, %d, %d, %s, %s", info->desc, info->ident, info->id, info->num_roms, info->roms->valid_extensions, info->roms->desc);
				info++;
			}
			return true;
		}
		case RETRO_ENVIRONMENT_GET_VARIABLE: {
			struct retro_variable *var = (struct retro_variable*)data;
			if (!var || !var->key) return false;

			// 1. Buscamos el elemento una sola vez usando un iterador
			//auto& params = gameMenu->getLibretroParams();
			auto& params = gameMenu->getCfgLoader()->startupLibretroParams;
			auto it = params.find(var->key);

			if (it != params.end()) {
				// Obtenemos el puntero crudo del unique_ptr
				// 'it->second' es el unique_ptr<cfg::t_emu_props>
				cfg::t_emu_props* param = it->second.get();
				var->value = param->values.at(param->selected).c_str();
				LOG_INFO("Asignando %s a %s", var->key, var->value);
				return true; // Retornar true indica que encontramos la variable
			} else {
				LOG_DEBUG("Preguntando por: %s sin valor", var->key);
				var->value = NULL;
			}
			return false;
		}

		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
			// Solo devolvemos true si el usuario ha tocado algo en el menú
			// de la Xbox 360 recientemente.
			bool *updated = (bool*)data;
			*updated = gameMenu->configMenus->options_changed_flag;
			if (*updated){
				LOG_DEBUG("Core options changed");
			}
			// IMPORTANTE: Una vez que el core sabe que hubo un cambio, 
			// reseteamos el flag para que en el siguiente frame no vuelva a procesar todo.
			gameMenu->configMenus->options_changed_flag = false;
			return true;
		}

		case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: {
            const struct retro_controller_info *info = (const struct retro_controller_info *)data;
            // Aquí el Core te está diciendo qué dispositivos soporta.
            for (unsigned i = 0; info[i].types && i < MAX_PLAYERS; ++i) {
				//gameMenu->getCfgLoader()->g_ports[i].available_types.clear();
				for (unsigned j = 0; j < info[i].num_types; ++j) {
					unsigned id = info[i].types[j].id;
					const char* desc = info[i].types[j].desc;
					LOG_DEBUG("Puerto %d soporta: %s (ID: %u)", i, desc, id);
					gameMenu->getCfgLoader()->g_ports[i].available_types.push_back(std::make_pair(id, desc));
				}
			}
            return true;
        }
		
		case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: {
			const struct retro_audio_buffer_status_callback *cb = 
				(const struct retro_audio_buffer_status_callback*)data;

			if (cb) {
				audio_status_cb = cb->callback; // Guardamos la función que el core llamará
			}
			return true;
		}
		case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: {
			const unsigned *latency_ms = (const unsigned*)data;
			if (latency_ms) {
				unsigned requested_latency = *latency_ms;
				LOG_DEBUG("El core solicita una latencia mínima de audio de: %u ms", requested_latency);
				// Aquí deberías ajustar el tamaño de tu buffer de salida de audio (ej. SDL, XAudio2, etc.)
				// para que sea al menos de ese tamaño.
				//audio_system->set_minimum_latency(requested_latency);
			}
			return true;
		}
		default: {
			if (cmd < 65583){
				LOG_DEBUG("Comando no tratado: %s", Constant::TipoToStr(cmd).c_str());
			}
			// Para comandos desconocidos como 52 o 65587
			return false; 
		}
		    
    }
    return false; // Por defecto devolver false para comandos desconocidos
}

static inline void take_screenshot(void* final_src, unsigned width, unsigned height, std::size_t pitch){
	// 1. Liberar memoria previa correctamente (si es array usa delete[])
	if (action_postponed.screenshot) delete[] action_postponed.screenshot;

	// 2. Calcular tamaño real
	std::size_t num_pixels = width * height;
	action_postponed.screenshot = new uint16_t[num_pixels];
	action_postponed.width = width;
	action_postponed.height = height;

	// 3. Copia segura considerando el pitch
	if (pitch == width * sizeof(uint16_t)) {
		// Pitch lineal, copia rápida
		memcpy(action_postponed.screenshot, final_src, num_pixels * sizeof(uint16_t));
	} else {
		// Pitch con padding, copiar fila por fila
		uint8_t* src_ptr = (uint8_t*)final_src;
		uint8_t* dst_ptr = (uint8_t*)action_postponed.screenshot;
		std::size_t row_size = width * sizeof(uint16_t);
        
		for (unsigned y = 0; y < height; y++) {
			memcpy(dst_ptr, src_ptr, row_size);
			src_ptr += pitch;
			dst_ptr += row_size;
		}
	}
	action_postponed.cycles = 0;
}

static void retro_video_refresh(const void *data, unsigned width, unsigned height, std::size_t pitch) {
    if (!data || width == 0 || height == 0) 
		return;	

    void* final_src = (void*)data;

	//Hacemos la comprobacion del pitch >= width * 4, por si hemos solicitado el RETRO_PIXEL_FORMAT_RGB565
	//pero el core no lo acepta
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

	if (action_postponed.cycles == 1 && action_postponed.action == HK_SAVESTATE){
		take_screenshot(final_src, width, height, pitch);
	}
	
    SDL_Surface* screen = gameMenu->screen;
	
	t_scale_props scaleProps = {0}; // Limpia todo el struct a cero antes de asignar
	// 4. Pasar el buffer correcto (ya sea el original de 16 o el convertido)
	scaleProps.src = (uint16_t*)final_src;
	scaleProps.sw = (int)width;
	scaleProps.sh = (int)height;
	scaleProps.spitch = pitch;
	scaleProps.dw = screen->w;
	scaleProps.dh = screen->h;
	scaleProps.dpitch = screen->pitch;
	scaleProps.scale = gameMenu->current_scaler_scale;
	scaleProps.ratio = aspectRatioValues[*gameMenu->current_ratio];
	scaleProps.force_fs = *gameMenu->current_force_fs;
    

	//if (SDL_LockSurface(screen) == 0) { // SDL_LockSurface devuelve 0 si tiene éxito
		scaleProps.dst = (uint16_t*)screen->pixels;
		//Escalamos la imagen con el escalador que hay almacenado en el puntero a funcion
		gameMenu->current_scaler(scaleProps);
		//SDL_UnlockSurface(screen);
	//}
}

// Se llama antes de pedir el estado de los inputs
void retro_input_poll(void) {
    update_input();
}

int16_t retro_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {

	if (port >= MAX_PLAYERS) 
        return 0;

	if (device == RETRO_DEVICE_JOYPAD) {
		// 1. Gestión del Latch de Start
		if (gameMenu->joystick->startHoldFrames[port] > 0) {
			gameMenu->joystick->startHoldFrames[port]--;
			gameMenu->joystick->g_joy_state[port][RETRO_DEVICE_ID_JOYPAD_START] = true; // Forzamos true en el array

			if (gameMenu->joystick->startHoldFrames[port] == 0) {
				gameMenu->joystick->g_joy_state[port][RETRO_DEVICE_ID_JOYPAD_START] = false;
			}
		}

		// 2. Respuesta al Core
		if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
			int16_t mask = 0;
			const bool* portState = gameMenu->joystick->g_joy_state[port];
			const bool* axisState = gameMenu->joystick->g_axis_state[port];

			for (int i = 0; i < maxJoyTargets; i++) {
				if (portState[i] || axisState[i]) mask |= (1 << i);
			}

			/*if (port == 0){
				char bitStr[17]; // 16 bits + fin de cadena
				bitStr[16] = '\0';
				for (int i = 0; i < 16; i++) {
					bitStr[15 - i] = (mask & (1 << i)) ? '1' : '0';
				}
				LOG_DEBUG("Joypad Mask: %s (Value: %d)", bitStr, mask);
			}*/
			
			return mask;
		} else {
			if (id < maxJoyTargets) {
				return gameMenu->joystick->g_joy_state[port][id] || gameMenu->joystick->g_axis_state[port][id] ? 1 : 0;
			}
			return 0;
		}
	} else if (device == RETRO_DEVICE_ANALOG) {
		int sdl_axis = -1;
        if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
            sdl_axis = (id == RETRO_DEVICE_ID_ANALOG_X) ? 0 : 1;
        } else if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) {
            sdl_axis = (id == RETRO_DEVICE_ID_ANALOG_X) ? 2 : 3;
        }

		if (sdl_axis != -1) {
			int16_t val = gameMenu->joystick->g_analog_state[port][sdl_axis];
			// LOG_DEBUG("port: %d, Valor: %d -> sdl_axis: %d", port, val, sdl_axis);
            // Aplicar zona muerta (Deadzone) para evitar "drift"
            if (abs(val) < DEADZONE) return 0;
            return val;
		}
	}
    
	return 0;
}

//Audio Callbacks for Libretro
// Callback para una sola muestra (menos eficiente, pero requerido)
void retro_audio_sample(int16_t left, int16_t right) {
	// Creamos una referencia local al buffer
    // Esto evita que la CPU haga: gameMenu -> buscar g_audioBuffer -> llamar Write
    AudioBuffer& audio = gameMenu->g_audioBuffer;
    const int mode = *gameMenu->current_sync;

    if (mode == SYNC_FAST_FORWARD) return;

    int16_t samples[2] = { left, right };
    audio.Write(samples, 2);
}

// Callback para ráfagas de muestras (el que usan casi todos los cores)
std::size_t retro_audio_sample_batch(const int16_t * __restrict data, std::size_t frames) {
	
	AudioBuffer& audio = gameMenu->g_audioBuffer;
    const int mode = *gameMenu->current_sync;

	if (mode == SYNC_FAST_FORWARD) return 0;
	
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
    wanted.callback = sdl_audio_callback;

    if (SDL_OpenAudio(&wanted, NULL) < 0) {
		string error = "Error SDL Audio: " + string(SDL_GetError());
        LOG_ERROR("%s\n", error.c_str());
        return;
    }
	audio_opened = 1;
    SDL_PauseAudio(0); // Inicia el audio
}

/**
*
*/
std::string initPathAndLog(char** argv){
	logger = new Logger(LOG_PATH);

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
    } else {
		menuData.resetIndexPos();
	}
    
    gameMenu.createMenuImages(menuData);
}

void closeGame(){
	if (gameMenu->romLoaded){
		// 2. Cerrar el dispositivo y liberar el hardware
		if (audio_opened) {
			// 1. Pausar el procesamiento de audio para detener el hilo de callback
			SDL_PauseAudio(1);
			SDL_CloseAudio();
			audio_opened = 0;
			SDL_Delay(10);
		}
		saveSram(gameMenu->getRomPaths()->sram.c_str());
		//Liberar recursos de libretro
		 // 1. Limpieza total del juego anterior
		retro_unload_game();
		retro_deinit();
		gameMenu->romLoaded = false;
	}
}

/**
*
*/
int launchGame(std::string rompath){
	std::string tempDir = Constant::getAppDir() + Constant::getFileSep() + "tmp";
	closeGame();

	struct retro_system_info info;
	memset(&info, 0, sizeof(info));
	retro_get_system_info(&info);
	std::string allowedExtensions = Constant::replaceAll(info.valid_extensions, "|", " ");
	LOG_DEBUG("Extensiones: %s\n", info.valid_extensions);

	if (dir.dirExists(tempDir.c_str())){
		dir.borrarDir(tempDir);
	}

	if (!dir.dirExists(tempDir.c_str()) && dir.createDir(tempDir) < 0){
		LOG_ERROR("No se ha podido crear el directorio %s\n", tempDir.c_str());
		gameMenu->showSystemMessage("No se ha podido crear el directorio " + tempDir, 3000);
		return 0;
	}

	LOG_DEBUG("Unzipping rom %s", rompath.c_str());
	unzippedFileInfo unzipped = unzipOrLoad(rompath, allowedExtensions, !info.need_fullpath, tempDir);

	if (unzipped.errorCode != 0){
		LOG_ERROR("No se ha podido abrir el fichero o no se puede descomprimir: %s\n", rompath.c_str());
		gameMenu->showSystemMessage("No se ha podido abrir el fichero o no se puede descomprimir " + rompath, 3000);
		return 0;
	}
	
	struct retro_game_info game = { unzipped.extractedPath.c_str(), unzipped.memoryBuffer, unzipped.romsize, NULL };
	retro_init();	
	bool success = retro_load_game(&game);

	//Para el jugador 1
	//retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
	// Para el jugador 2
	//retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);

	//Liberar la memoria tras la carga exitosa
	// La mayoría de los cores de Libretro ya han copiado los datos a su propia RAM interna
	if (unzipped.memoryBuffer){
		free(unzipped.memoryBuffer);
		unzipped.memoryBuffer = NULL;
	} 

	// Es importante cargar la ROM antes de retro_run
	if(!success) {
		LOG_ERROR("Error cargando la ROM\n");
		gameMenu->showSystemMessage("Error cargando la rom", 3000);
		return 0;
	}

	
	string romname = (gameMenu->getCfgLoader()->getCfgEmu() != NULL ? gameMenu->getCfgLoader()->getCfgEmu()->name + " - " : "") + dir.getFileNameNoExt(unzipped.originalPath);
	SDL_WM_SetCaption(romname.c_str(), NULL);

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
	gameMenu->setRomPaths(rompath);
	loadSram(gameMenu->getRomPaths()->sram.c_str());
	gameMenu->setEmuStatus(EMU_STARTED);
	return 1;
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
		//gameMenu->joystick->lastSelectPress = 0;
	}
	
	return ret;
}

// En tu función de salida o desinicialización
void closeResources() {
	closeGame();
    if (conversion_buffer != NULL) {
        free(conversion_buffer);
        conversion_buffer = NULL; // Importante ponerlo a NULL tras liberar
        buffer_size = 0;
    }

    g_saveQueue.running = false;
    
    // 1. Despertamos al hilo una última vez para que vea que 'running' es false
    SDL_SemPost(g_saveQueue.semaphore);
    
    // 2. Esperamos a que el hilo termine (opcional pero recomendado)
    // En SDL 1.2 no hay SDL_WaitThread, pero puedes usar un pequeño delay
    SDL_Delay(100); 

    // 3. Liberamos los recursos de SDL
    if (g_saveQueue.semaphore) SDL_DestroySemaphore(g_saveQueue.semaphore);
    if (g_saveQueue.saveMutex) SDL_DestroyMutex(g_saveQueue.saveMutex);
    
    // 4. Liberamos la memoria del buffer de SRAM
    if (g_sram_data_last) {
        free(g_sram_data_last);
        g_sram_data_last = NULL;
    }

	delete logger;
	delete gameMenu;
}

inline void updateGame() {
    const Uint32 currentTime = SDL_GetTicks();
    // Verificamos si ha pasado el intervalo desde el último guardado
    if (currentTime - lastSramSaved >= INTERVAL_SRAM_SAVE) {
        saveSram(gameMenu->getRomPaths()->sram.c_str());
        lastSramSaved = currentTime;
    }

	// retro_run hace todo: 
	// 1. Llama a input_poll() -> update_input()
	// 2. Calcula la lógica del juego
	// 3. Llama a audio_batch() -> (Aquí el audio bloquea si va muy rápido)
	// 4. Llama a video_refresh() -> (Aquí se dibuja el frame y los FPS)
	retro_run();
}

void processFrontendEvents(){
	HOTKEYS_LIST hotkey = gameMenu->joystick->findHotkey();

	if (action_postponed.action == HK_SAVESTATE && hotkey != HK_SAVESTATE && action_postponed.cycles == 0){
		hotkey = HK_SAVESTATE;
	}

	switch (hotkey) {
		case HK_SAVESTATE:
			//saveState();
			if (action_postponed.cycles == 0){
				saveState();
				action_postponed.action = -1;
				action_postponed.cycles = -1;
			} else {
				action_postponed.cycles = 1;
				action_postponed.action = HK_SAVESTATE;
			}
			break;

		case HK_LOADSTATE:
			// loadState debe ejecutarse en el hilo principal		
			loadState();
			break;

		case HK_MAX:
			// No hacemos nada para el valor límite
			break;

		case HK_SLOT_UP:
			g_currentSlot = (g_currentSlot + 1) % MAX_SAVESTATES;
			gameMenu->showSystemMessage("Slot seleccionado: " + Constant::intToString(g_currentSlot), 2000);
			break;

		case HK_SLOT_DOWN:
			g_currentSlot = (g_currentSlot - 1 < 0) ? MAX_SAVESTATES - 1 : g_currentSlot - 1;
			gameMenu->showSystemMessage("Slot seleccionado: " + Constant::intToString(g_currentSlot), 2000);
			break;

		default:
			LOG_DEBUG("Sending Hotkey %d\n", hotkey);
			// Cualquier otro hotkey (ej. volumen, reset, menú) se delega al frontend
			gameMenu->processFrontendEvents(hotkey);
			break;
	}
}

/**
*
*/
int main(int argc, char *argv[]) {
	initPathAndLog(argv);
	CfgLoader cfgLoader;
	if (cfgLoader.isDebug()){
		#ifndef DEBUG_LOG
		#define DEBUG_LOG
		#endif
        logger->errorLevel = L_DEBUG;
    }

	LOG_DEBUG("appdir: %s\n", Constant::getAppDir().c_str());
	LOG_DEBUG("appexe: %s\n", Constant::getAppExecutable().c_str());

	gameMenu = new GameMenu(&cfgLoader);

	ListMenu listMenu(gameMenu->screen->w, gameMenu->screen->h);
	listMenu.setLayout(LAYBOXES, gameMenu->screen->w, gameMenu->screen->h);
	
	if (!gameMenu->initDblBuffer(cfgLoader.getWidth(), cfgLoader.getHeight())){
		LOG_ERROR("No se pudo crear el buffer doble");
		gameMenu->showSystemMessage("No se pudo crear el buffer doble", 3000);
        return 1;
    }

	std::string initMsg = "Loading " + Constant::getAppExecutable() + "...";
	Constant::drawTextCent(gameMenu->screen, Fonts::getFont(Fonts::FONTSMALL), initMsg.c_str(), 0,0, true, true, textColor, 0);
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

	if (!loadGameAtStart(argc, argv)){
		//Workaround para mostrar una primera imagen del menu con las imagenes cargadas
		listMenu.keyUp = true;
		updateMenuScreen(tileMap, *gameMenu, listMenu, false);
		SDL_Flip(gameMenu->screen);
		listMenu.keyUp = false;
	}

	InitSaveSystem();

	//Poblamos las opciones del core. Lo tenemos que hacer aquí porque ya se deberian 
	//haber obtenido.
	gameMenu->configMenus->poblarCoreOptions(&cfgLoader);
	//retro_set_controller_port_device(puerto, id_guardado);
	//retro_set_controller_port_device(0, 1);

	double nextFrameTime = SDL_GetTicks();
	while (gameMenu->running) {
		// Procesamos eventos como pulsaciones de hotkeys
		processFrontendEvents();

		switch (gameMenu->getEmuStatus()){
			case EMU_STARTED:
				updateGame();
				break;
			case EMU_MENU:
				updateMenuScreen(tileMap, *gameMenu, listMenu, false);
				break;
			case EMU_MENU_OVERLAY:
				updateMenuOverlay(*gameMenu, listMenu);
				break;
		}

		// DIBUJO DE INTERFAZ (OSD, FPS, Mensajes)
		gameMenu->processFrontendEventsAfter();

		// Actualizamos la pantalla
		SDL_Flip(gameMenu->screen);
		//SDL_UpdateRect(gameMenu->screen);

		// Limitamos los frames si tenemos que sincronizar con el video
		if (*gameMenu->current_sync == SYNC_TO_VIDEO){
			gameMenu->sync->limit_fps(nextFrameTime);
			
		}
	}
	closeResources();
    return 0;
}