#pragma once

#include "salvia.h"
#include "dischelper.h"

retro_audio_buffer_status_callback_t audio_status_cb;

struct retro_core_variable {
   const char *key;    // Nombre técnico: "nestopia_region"
   const char *value;  // Nombre visual y opciones: "Region; Auto|NTSC|PAL"
};

void drawLoadingProgressBar(SDL_Surface* screen, float progress);
struct t_progress_load{
	float loading_progress;
	int total_rom_files;
	int current_rom_file;

	t_progress_load(){
		reset();
	}

	void reset(){
		loading_progress = 0.0f;
		total_rom_files = 10; // Valor estimado o calculado abriendo el zip
		current_rom_file = 0;
	}

} progress_loader;


void retro_log_printf(enum retro_log_level level, const char *fmt, ...) {
    #ifndef DEBUG_LOG
    if (level != RETRO_LOG_ERROR && gameMenu->romLoaded) {
        return;
    }
    #endif

	const unsigned int MAX_BUFFER = 128;
	char buffer[MAX_BUFFER] = {0}; 
    va_list args;
    va_start(args, fmt);
    int len = _vsnprintf_s(buffer, MAX_BUFFER, _TRUNCATE, fmt, args);
    va_end(args);
    buffer[MAX_BUFFER - 1] = '\0';

	//This code is intended to detect loading process for fbanext and mame
    if (!gameMenu->romLoaded && progress_loader.total_rom_files > 0) {
        if (strstr(buffer, "Opening ROM file:")) {
            progress_loader.current_rom_file++;
            float progress = (float)progress_loader.current_rom_file / (float)progress_loader.total_rom_files;
            drawLoadingProgressBar(gameMenu->overlay, (progress > 1.0f) ? 1.0f : progress);
        }
    }

	//Log the output of the core
	#ifdef DEBUG_LOG
		OutputDebugStringA(buffer);
	#endif
}

// ─────────────────────────────────────────────
// Helper: split "opt1|opt2|opt3" → vector
// ─────────────────────────────────────────────
namespace {

std::vector<std::string> splitOptions(const std::string& raw) {
    std::vector<std::string> out;
    std::istringstream ss(raw);
    std::string token;
    while (std::getline(ss, token, '|')) {
        if (!token.empty()) out.push_back(token);
    }
    return out;
}

// Crea o actualiza una entrada preservando `selected` si ya existía.
void applyEntry(std::map<std::string, std::unique_ptr<cfg::t_emu_props> > &data,
                const std::string& key,
                std::string description,       // Pasamos por valor para mover
                std::vector<std::string> values, // Pasamos por valor para mover
                int defaultIdx = 0)
{
    auto it = data.find(key);
    if (it != data.end()) {
		if (it->second->description.empty())
			it->second->description = description;

		if (it->second->values.empty())
			it->second->values = values;
        
		//Si ya estaba en el mapa, no lo tocamos
        //if (it->second.selected < 0 || it->second.selected >= (int)it->second.values.size())
        //    it->second.selected = defaultIdx;
        
		if (it->second->cachedValue.empty() && !it->second->values.empty())
            it->second->cachedValue = it->second->values[it->second->selected];

		LOG_DEBUG("[Core Options] SET. Key already defined %s", key.c_str());
        return;
    } 

    cfg::t_emu_props *raw = new cfg::t_emu_props();
    raw->description = std::move(description);
    raw->values      = std::move(values);
    raw->selected    = defaultIdx;

    if (!raw->values.empty())
        raw->cachedValue = raw->values[defaultIdx];

    LOG_DEBUG("[Core Options] SET %s = %s", key.c_str(), raw->cachedValue.c_str());
    // Ahora data[key] usará el operator=(t_emu_props&&) que definimos arriba
    data[key] = std::unique_ptr<cfg::t_emu_props>(raw); 
}

} // namespace anónimo


static bool retro_environment(unsigned cmd, void *data) {
	static char dirSystem[MAX_PATH] = {0};
	static char savePath[MAX_PATH] = {0};

    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
			struct retro_log_callback *log = (struct retro_log_callback*)data;
			log->log = retro_log_printf;
            return true;
        }

		case RETRO_ENVIRONMENT_SET_MESSAGE: {
			const struct retro_message *msg = (const struct retro_message*)data;
			if (msg && msg->msg){
				//LOG_DEBUG("NOTIFICACION DEL CORE: %s (Duracion: %u frames)", msg->msg, msg->frames);
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
                // Caso simetrico: si el core SOLO registra la basic (cores antiguos
                // o builds sin EXT), replicamos los 7 callbacks comunes en
                // disk_control_ext para que el codigo que lo consuma los vea
                // poblados. Los 3 callbacks exclusivos de EXT (set_initial_image,
                // get_image_path, get_image_label) quedan a NULL y DEBEN
                // consultarse siempre con guard "if (disk_control_ext.xxx)".
                disk_control_ext.set_eject_state     = cb->set_eject_state;
                disk_control_ext.get_eject_state     = cb->get_eject_state;
                disk_control_ext.get_image_index     = cb->get_image_index;
                disk_control_ext.set_image_index     = cb->set_image_index;
                disk_control_ext.get_num_images      = cb->get_num_images;
                disk_control_ext.replace_image_index = cb->replace_image_index;
                disk_control_ext.add_image_index     = cb->add_image_index;
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
                // Si el core registra SOLO la EXT (comportamiento estandar:
                // no llamara a SET_DISK_CONTROL_INTERFACE si esta retorna true),
                // replicamos los 7 callbacks comunes en la estructura basic
                // para que swapDisc/swapToNewDisc los vean poblados.
                disk_control.set_eject_state     = cb->set_eject_state;
                disk_control.get_eject_state     = cb->get_eject_state;
                disk_control.get_image_index     = cb->get_image_index;
                disk_control.set_image_index     = cb->set_image_index;
                disk_control.get_num_images      = cb->get_num_images;
                disk_control.replace_image_index = cb->replace_image_index;
                disk_control.add_image_index     = cb->add_image_index;
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
            enum retro_pixel_format requested = *(enum retro_pixel_format*)data;
			LOG_DEBUG("Solicitando pixelformat %d", (int)requested);
			fmt = requested; // Guarda esto en tu 
			// MAME suele requerir 0RGB1555 o XRGB8888. 
			// Aceptamos lo que pida
			if (requested == RETRO_PIXEL_FORMAT_0RGB1555 || 
				requested == RETRO_PIXEL_FORMAT_RGB565   || 
				requested == RETRO_PIXEL_FORMAT_XRGB8888) {
				LOG_INFO("Formato de pixel aceptado: %d", requested);
				return true; 
			}
			LOG_ERROR("Core solicita un formato totalmente incompatible");
			return false;
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
			std::string currentPath = gameMenu->getCfgLoader()->configMain[cfg::libretrosystem].valueStr;

			 // Copiamos el nuevo path al buffer fijo de forma segura
            strncpy(dirSystem, currentPath.c_str(), sizeof(dirSystem) - 1);
            savePath[sizeof(dirSystem) - 1] = '\0'; // Aseguramos el cierre nulo

            // Entregamos SIEMPRE la misma dirección de memoria
            *(const char**)data = dirSystem;
            return true;
		}

		case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:{
			const struct retro_system_av_info *av_info = (const struct retro_system_av_info *)data;
			gameMenu->sync->init_fps_counter((float)av_info->timing.fps);
			gameMenu->g_audioRate.reset();
			gameMenu->g_audioRate.init(BUFF_SIZE);
			return true;
		}

		case RETRO_ENVIRONMENT_SET_GEOMETRY:{
			const struct retro_game_geometry *geom = (const struct retro_game_geometry*)data;
			return true;
		}
		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
			std::string currentPath = gameMenu->getSramPath();

            // Copiamos el nuevo path al buffer fijo de forma segura
            strncpy(savePath, currentPath.c_str(), sizeof(savePath) - 1);
            savePath[sizeof(savePath) - 1] = '\0'; // Aseguramos el cierre nulo

            // Entregamos SIEMPRE la misma dirección de memoria
            *(const char**)data = savePath;
            return true;
		}

		case RETRO_ENVIRONMENT_GET_LANGUAGE:{
			// El núcleo nos pregunta: "¿En qué idioma quieres las descripciones?"
            unsigned *lang = (unsigned*)data;
			static int g_current_language = gameMenu->getCfgLoader()->configMain[cfg::libretro_save].valueInt;
			*lang = g_current_language;
            LOG_INFO("Core solicitó idioma: Enviando (%d)", g_current_language);
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
		// ── 1. RETRO_ENVIRONMENT_SET_VARIABLES (formato clásico) ──────────────────
		case RETRO_ENVIRONMENT_SET_VARIABLES:
		{
			const auto* vars = static_cast<const retro_variable*>(data);
			if (!vars) return false;

			for (int i = 0; vars[i].key != nullptr; ++i) {
				// 1. Protección contra keys vacias (basura recurrente en algunos cores)
				if (vars[i].key[0] == '\0') continue;

				const std::string key      = vars[i].key;
				// 2. Protección contra valores nulos o vacíos
				if (!vars[i].value || vars[i].value[0] == '\0') {
					LOG_DEBUG("[Core Options] SKIP: Key %s has no value string", key.c_str());
					continue;
				}

				const std::string rawValue = vars[i].value;
				const std::size_t sep = rawValue.find("; ");

				// 3. Protección de Formato: Si no hay "; ", el core está enviando algo fuera de estándar
				if (sep != std::string::npos && sep > 0) {
					std::string desc = rawValue.substr(0, sep);
					std::string optionsPart = rawValue.substr(sep + 2);

					// 4. Validación extra: ¿Hay opciones después del separador?
					if (!optionsPart.empty()) {
						std::vector<std::string> values = splitOptions(optionsPart);
                
						if (!values.empty()) {
							LOG_DEBUG("[Core Options] PARSE OK: %s", key.c_str());
							applyEntry(gameMenu->getCfgLoader()->startupLibretroParams, key, desc, std::move(values), 0);
						} else {
							LOG_DEBUG("[Core Options] ERROR: No split tokens in %s", key.c_str());
						}
					}
				} else {
					// DOSBox Pure a veces envía notificaciones que no son definiciones de opciones
					LOG_DEBUG("[Core Options] INFO: Key %s format not recognized (Value: %s)", key.c_str(), rawValue.c_str());
				}
			}

			gameMenu->configMenus->poblarCoreOptions(gameMenu->getCfgLoader());
			return true;
		}


		// ── 2. RETRO_ENVIRONMENT_GET_VARIABLE ─────────────────────────────────────
		case RETRO_ENVIRONMENT_GET_VARIABLE:
		{
			retro_variable* var = static_cast<retro_variable*>(data);
			if (!var || !var->key) return false;

			auto it = gameMenu->getCfgLoader()->startupLibretroParams.find(var->key);
			if (it == gameMenu->getCfgLoader()->startupLibretroParams.end()) {
				var->value = nullptr;
				return false;
			}

			const int nVals = static_cast<int>(it->second->values.size());
			const int sel   = it->second->selected;

			if (nVals > 0 && sel >= 0 && sel < nVals)
				it->second->cachedValue = it->second->values[sel];
			else if (nVals > 0)
				it->second->cachedValue = it->second->values[0];
			else {
				var->value = nullptr;
				return false;
			}
			var->value = it->second->cachedValue.c_str();
			LOG_DEBUG("[Core Options] GET %s = %s", var->key, var->value);
			return true;
		}


		// ── 3. RETRO_ENVIRONMENT_SET_CORE_OPTIONS (V1) ────────────────────────────
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
		{
			const auto* defs = static_cast<const retro_core_option_definition*>(data);
			if (!defs) return false;

			for (int i = 0; defs[i].key != nullptr; ++i) {
				const std::string key  = defs[i].key;
				const std::string desc = defs[i].desc   ? defs[i].desc   : "";

				std::vector<std::string> values;
				for (int j = 0;
					 j < RETRO_NUM_CORE_OPTION_VALUES_MAX && defs[i].values[j].value != nullptr;
					 ++j)
				{
					values.push_back(defs[i].values[j].value);
				}

				// Buscar índice del valor por defecto
				int defaultIdx = 0;
				if (defs[i].default_value) {
					const std::string defVal = defs[i].default_value;
					for (int j = 0; j < static_cast<int>(values.size()); ++j) {
						if (values[j] == defVal) { defaultIdx = j; break; }
					}
				}

				applyEntry(gameMenu->getCfgLoader()->startupLibretroParams, key, desc, values, defaultIdx);
			}
			return true;
		}


		// ── 4. RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2 ──────────────────────────────
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
		{
			const auto* v2 = static_cast<const retro_core_options_v2*>(data);
			if (!v2 || !v2->definitions) return false;

			const retro_core_option_v2_definition* defs = v2->definitions;

			for (int i = 0; defs[i].key != nullptr; ++i) {
				const std::string key  = defs[i].key;
				// V2 tiene desc y desc_categorized; usamos desc como descripción principal
				const std::string desc = defs[i].desc ? defs[i].desc : "";

				std::vector<std::string> values;
				for (int j = 0;
					 j < RETRO_NUM_CORE_OPTION_VALUES_MAX && defs[i].values[j].value != nullptr;
					 ++j)
				{
					values.push_back(defs[i].values[j].value);
				}

				int defaultIdx = 0;
				if (defs[i].default_value) {
					const std::string defVal = defs[i].default_value;
					for (int j = 0; j < static_cast<int>(values.size()); ++j) {
						if (values[j] == defVal) { defaultIdx = j; break; }
					}
				}

				applyEntry(gameMenu->getCfgLoader()->startupLibretroParams, key, desc, values, defaultIdx);
			}
			return true;
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
				t_controller_port *port = &gameMenu->joystick->g_ports[i];
				port->available_types.clear();
				for (unsigned j = 0; j < info[i].num_types; j++) {
					unsigned id = info[i].types[j].id;
					const char* desc = info[i].types[j].desc;

					if (desc == NULL) {
						LOG_DEBUG("Puerto %d: Se recibió un descriptor NULL para el ID %u. Saltando...", i, id);
						continue; 
					}

					LOG_DEBUG("Puerto %d soporta: %s (ID: %u)", i, desc, id);
					port->available_types.push_back(std::make_pair(id, desc));
				}

				if (port->available_types.size() > 0){
					port->current_device_id = port->available_types[0].first;
				}
			}
			gameMenu->configMenus->poblarJoystickTypes(gameMenu->joystick);
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
		case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: {
			/* El core envia su mapa de memoria completo.  Copiamos los
			 * descriptores para poder acceder a regiones como HRAM en
			 * Game Boy que no estan disponibles via retro_get_memory_data. */
			const struct retro_memory_map *map = (const struct retro_memory_map*)data;
			if (map && map->descriptors) {
				g_num_mem_descriptors = map->num_descriptors;
				if (g_num_mem_descriptors > MAX_LIBRETRO_MEM_DESCRIPTORS)
					g_num_mem_descriptors = MAX_LIBRETRO_MEM_DESCRIPTORS;
				memcpy(g_mem_descriptors, map->descriptors,
				       g_num_mem_descriptors * sizeof(struct retro_memory_descriptor));
				LOG_DEBUG("SET_MEMORY_MAPS: captured %u descriptors from core", g_num_mem_descriptors);
				for (unsigned i = 0; i < g_num_mem_descriptors; i++) {
					LOG_DEBUG("  desc[%u]: ptr=%p start=0x%05X len=0x%X select=0x%X offset=0x%X flags=0x%X",
					          i, g_mem_descriptors[i].ptr,
					          (unsigned)g_mem_descriptors[i].start,
					          (unsigned)g_mem_descriptors[i].len,
					          (unsigned)g_mem_descriptors[i].select,
					          (unsigned)g_mem_descriptors[i].offset,
					          (unsigned)g_mem_descriptors[i].flags);
				}
			}
			return true;
		}
		case RETRO_ENVIRONMENT_SHUTDOWN : {
			gameMenu->setEmuStatus(EMU_MENU);
			return true;
		}
		default: {
			if (cmd < 65572){
				LOG_DEBUG("Comando no tratado: %s", Constant::TipoToStr(cmd).c_str());
			}
			// Para comandos desconocidos como 52 o 65587
			return false;
		}
		    
    }
    return false; // Por defecto devolver false para comandos desconocidos
}

static inline void take_screenshot(void* final_src, unsigned width, unsigned height, std::size_t pitch, int bpp = 16){
    if (action_postponed.screenshot) {
        delete[] action_postponed.screenshot;
        action_postponed.screenshot = NULL;
    }

	const std::size_t bytes_per_pixel = (bpp == 32) ? 4 : 2;
	const std::size_t total_bytes = (std::size_t)width * height * bytes_per_pixel;
    action_postponed.screenshot = new uint8_t[total_bytes];
    action_postponed.width = width;
    action_postponed.height = height;
	action_postponed.bpp = bpp;

    uint8_t* src_ptr = (uint8_t*)final_src;
    uint8_t* dst_ptr = action_postponed.screenshot;
    std::size_t row_size = width * bytes_per_pixel;
    for (unsigned y = 0; y < height; y++) {
        memcpy(dst_ptr, src_ptr, row_size);
        src_ptr += pitch;
        dst_ptr += row_size;
    }

    action_postponed.cycles = 0;
}

static inline void sw_refresh(const void *data, unsigned width, unsigned height, std::size_t pitch) {
    if (!data || width == 0 || height == 0 || *gameMenu->current_scaler_mode == NO_VIDEO) 
		return;	

    void* final_src = (void*)data;

	//Hacemos la comprobacion del pitch >= width * 4, por si hemos solicitado el RETRO_PIXEL_FORMAT_RGB565
	//pero el core no lo acepta
	if (video_bpp == 16 && fmt != RETRO_PIXEL_FORMAT_RGB565){
		// 2. Gestionar buffer de conversión de forma eficiente
		std::size_t needed = width * height * sizeof(uint16_t);
		if (!conversion_buffer || buffer_size < needed) {
			uint16_t* temp = (uint16_t*)realloc(conversion_buffer, needed);
			if (!temp) return;
			conversion_buffer = temp;
			buffer_size = needed;
		}

		switch (fmt){
			case RETRO_PIXEL_FORMAT_XRGB8888: { 
				convertARGB8888ToRGB565_Fast((uint32_t*)data, width, height, pitch, conversion_buffer, width * 2);
				break;
			}
			case RETRO_PIXEL_FORMAT_0RGB1555: {
				convert0RGB1555ToRGB565_Fast2((uint16_t*)data, width, height, pitch, conversion_buffer);
				break;
			}
		}
		pitch = width * 2;
		final_src = (void*)conversion_buffer;
	}

	if (action_postponed.cycles == 1 && action_postponed.action == SAVE_STATE){
		take_screenshot(final_src, width, height, pitch);
	}
	
    SDL_Surface* screen = gameMenu->gameScreen;
	
	t_scale_props scaleProps;
	scaleProps.src = (uint16_t*)final_src;
	scaleProps.dst = (uint16_t*)screen->pixels;
	scaleProps.sw = (int)width;
	scaleProps.sh = (int)height;
	scaleProps.spitch = pitch;
	scaleProps.dw = screen->w;
	scaleProps.dh = screen->h;
	scaleProps.dpitch = screen->pitch;
	scaleProps.scale = gameMenu->current_scaler_scale;
	scaleProps.ratio = aspectRatioValues[*gameMenu->current_ratio];
	scaleProps.force_fs = *gameMenu->current_force_fs;

	// 4. Pasar el buffer correcto (ya sea el original de 16 o el convertido)
#ifdef _XBOX
	if (video_bpp == 32){
		scale_software_32bit_xbox(scaleProps, (uint32_t*)final_src, (uint32_t*)screen->pixels);
		return;
	}
#endif

	//Escalamos la imagen con el escalador que hay almacenado en el puntero a funcion
	gameMenu->current_scaler(scaleProps);
}

static inline void hw_refresh(const void *data, unsigned width, 
                                unsigned height, std::size_t pitch) {
    if (!data) return;

    const int bpp = (fmt == RETRO_PIXEL_FORMAT_XRGB8888) ? 32 : 16;
    const float current_ratio = aspectRatioValues[*gameMenu->current_ratio];
    unsigned row_bytes = width * (bpp / 8);

    SDL_Surface*& screen = gameMenu->gameScreen;

	#ifdef _XBOX
    if (width  != current_video_settings.sw || height != current_video_settings.sh || bpp != current_video_settings.bpp){
        screen = SDL_SetVideoMode(width, height, bpp, SDL_DOUBLEBUF);
        current_video_settings.sw  = width;
        current_video_settings.sh  = height;
        current_video_settings.bpp = bpp;
    }
	
	if (current_video_settings.filter != *gameMenu->current_shader){
		XBOX_SelectEffect(*gameMenu->current_shader);
		current_video_settings.filter = *gameMenu->current_shader;
	}

	if (current_video_settings.force_fs != *gameMenu->current_force_fs){
		SDL_XBOX_SetDisplayFullscreen(*gameMenu->current_force_fs == 1);
		current_video_settings.force_fs = *gameMenu->current_force_fs;
	}
	#endif

	if (current_video_settings.ratio != current_ratio){
		LOG_DEBUG("SetDisplaySize: ratio=%.3f, tex=%dx%d", current_ratio, current_video_settings.sw, current_video_settings.sh);
		#ifdef _XBOX
		SDL_XBOX_SetDisplaySize(current_ratio);
		#endif
		current_video_settings.ratio = current_ratio;
	}

    // ── Determinar el puntero fuente definitivo ──────────────────────────────
    const void* final_src = data;
    std::size_t final_pitch = pitch;

    if (fmt == RETRO_PIXEL_FORMAT_0RGB1555){
        // El surface SDL de 16 bpp espera RGB565: hay que convertir
        std::size_t needed = (std::size_t)width * height * sizeof(uint16_t);

        if (!conversion_buffer || buffer_size < needed){
            uint16_t* tmp = (uint16_t*)realloc(conversion_buffer, needed);
            if (!tmp) return;          // sin memoria, abortar
            conversion_buffer = tmp;
            buffer_size       = needed;
        }

        convert_0RGB1555_to_RGB565((const uint16_t*)data,conversion_buffer,width, height,pitch);

        final_src   = conversion_buffer;
        final_pitch = width * sizeof(uint16_t); // pitch denso, sin padding
    }

    // ── Screenshot (usa la fuente ya convertida) ─────────────────────────────
    if (action_postponed.cycles == 1 && action_postponed.action  == SAVE_STATE){
        take_screenshot((void *)final_src, width, height, final_pitch, bpp);
    }

    // ── Copiar al surface SDL ─────────────────────────────────────────────────
    if (SDL_LockSurface(screen) != 0) return;
    uint8_t*       dst     = (uint8_t*)screen->pixels;
    const uint8_t* src     = (const uint8_t*)final_src;

    if (final_pitch == (std::size_t)screen->pitch && final_pitch == row_bytes){
        memcpy(dst, src, row_bytes * height);       // copia directa
    } else {
        for (unsigned y = 0; y < height; ++y)       // fila por fila
        {
            memcpy(dst, src, row_bytes);
            dst += screen->pitch;
            src += final_pitch;
        }
    }
    SDL_UnlockSurface(screen);
}


static void retro_video_refresh(const void *data, unsigned width, 
                                unsigned height, std::size_t pitch) {
	#ifdef _XBOX
		hw_refresh(data, width, height, pitch);
	#else
		sw_refresh(data, width, height, pitch);
	#endif
}

// Se llama antes de pedir el estado de los inputs
void retro_input_poll(void) {
    update_input();
}

int16_t retro_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port >= MAX_PLAYERS) 
        return 0;
	
	t_joy_state *inputs = &gameMenu->joystick->inputs;

	if (device == RETRO_DEVICE_JOYPAD) {
		const int sdlModifier = inputs->mapperHotkeys.getSdlBtn(port, HK_MODIFIER);
		const bool modifierPressed = inputs->getSdlBtn(port, sdlModifier);

		// 1. Gestión del Latch de Start
		#ifdef _XBOX
		// 1. Cachear el valor para evitar múltiples accesos al array
		const int holdFrames = gameMenu->joystick->startHoldFrames[port];
		if (holdFrames > 0) {
			// 2. Obtener el índice del botón una sola vez
			int sdlBtn = inputs->mapperCore.getSdlBtn(port, RETRO_DEVICE_ID_JOYPAD_START);
			// 3. Validación de rango rápida
			if ((unsigned int)sdlBtn < MAX_BUTTONS) { 
				// Forzamos el estado true mientras haya frames de retención
				inputs->btn_state[port][sdlBtn] = true;
			}
		}
		#endif

		// 2. Respuesta al Core
		if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
			int16_t mask = 0;
			if (!modifierPressed) {
				// Fast path: sin modifier, lectura directa sin getSdlBtn por iteracion
				for (int i = 0; i < maxJoyTargets; i++) {
					if (inputs->getCoreAny(port, i)) {
						mask |= (int16_t)(1 << i);
					}
				}
			} else {
				// Slow path: con modifier activo, filtramos botones
				for (int i = 0; i < maxJoyTargets; i++) {
					if (inputs->mapperCore.getSdlBtn(port, i) != sdlModifier)
						continue;
					if (inputs->getCoreAny(port, i)) {
						mask |= (int16_t)(1 << i);
					}
				}
			}
			return mask;
		} else {
			if (id < maxJoyTargets) {
				if (!modifierPressed) {
					return inputs->getCoreAny(port, id) ? 1 : 0;
				}
				return (inputs->mapperCore.getSdlBtn(port, id) == sdlModifier ||
						inputs->getCoreAny(port, id)) ? 1 : 0;
			}
			return 0;
		}
	} 
	else if (device == RETRO_DEVICE_ANALOG) {
		int sdl_axis = -1;

		if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
			sdl_axis = (id == RETRO_DEVICE_ID_ANALOG_X) ? 0 : 1;
			//LOG_INFO("Analog Left: %u %u", id, sdl_axis);
		} else if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) {
			#ifdef _XBOX
			sdl_axis = (id == RETRO_DEVICE_ID_ANALOG_X) ? 2 : 3;
			#else
			sdl_axis = (id == RETRO_DEVICE_ID_ANALOG_X) ? 4 : 3;
			#endif
			//LOG_INFO("Analog Right: %u %u", id, sdl_axis);
		} else if (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON) {
			//LOG_INFO("Analog button: %u ", index);
			// Gatillos: el core pide id = RETRO_DEVICE_ID_JOYPAD_L2 / R2
			#ifdef _XBOX
			if (id == RETRO_DEVICE_ID_JOYPAD_L2)
				sdl_axis = AXIS_LT;
			else if (id == RETRO_DEVICE_ID_JOYPAD_R2)
				sdl_axis = AXIS_RT;
			#endif
		} else {
			LOG_INFO("Indice analogico: %u", index);
		}

		if (sdl_axis != -1) {
			return gameMenu->joystick->inputs.g_analog_state[port][sdl_axis];
		}
	} else if (device == RETRO_DEVICE_MOUSE) {
		switch (id) {
			case RETRO_DEVICE_ID_MOUSE_X:      return inputs->mouse_rel_x;
			case RETRO_DEVICE_ID_MOUSE_Y:      return inputs->mouse_rel_y;
			case RETRO_DEVICE_ID_MOUSE_LEFT:   return inputs->mouse_buttons[0];
			case RETRO_DEVICE_ID_MOUSE_RIGHT:  return inputs->mouse_buttons[2];
			//case RETRO_DEVICE_ID_MOUSE_WHEELUP:   return (inputs->mouse_wheel > 0);
			//case RETRO_DEVICE_ID_MOUSE_WHEELDOWN: return (inputs->mouse_wheel < 0);
			case RETRO_DEVICE_ID_MOUSE_MIDDLE: return inputs->mouse_buttons[1];
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
    const int mode = *gameMenu->current_sync;

    switch(mode) {
        case SYNC_TO_AUDIO:
            // El bloqueo ya sincroniza naturalmente con el reloj de audio
            gameMenu->g_audioBuffer.WriteBlocking(data, frames * 2);
            break;
        case SYNC_FAST_FORWARD:
            return frames;
        default:
            // SYNC_TO_VIDEO / SYNC_NONE: DRC ajusta la tasa para evitar drift
            gameMenu->g_audioRate.processAndWrite(gameMenu->g_audioBuffer, data, frames, false);
            break;
    }
    return frames;
}

void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
    if (audio_closing) {
        memset(stream, 0, len);
        return;   // salir limpio sin tocar el estado del core
    }
	
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
	#ifdef WIN
	wanted.samples = 1024; // Tamaño del bloque (latencia)
	#elif defined(_XBOX)
	wanted.samples = 2048; // Tamaño del bloque (latencia)
	#endif
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
            menuData.setLayout(menuBeforeExit.layout, gameMenu.overlay->w, gameMenu.overlay->h);
        }
        menuData.animateBkg = menuBeforeExit.animateBkg;
    }

    gameMenu.loadEmuCfg(menuData);
    if (retMenu == 0 && menuData.maxLines == menuBeforeExit.maxLines 
		&& menuBeforeExit.iniPos >= 0 && menuBeforeExit.iniPos < menuData.listSize
		&& menuBeforeExit.endPos > 0 && menuBeforeExit.endPos <= menuData.listSize
		&& menuBeforeExit.curPos > 0 && menuBeforeExit.curPos < menuData.listSize){
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
			audio_closing = true;
			// 1. Pausar el procesamiento de audio para detener el hilo de callback
			SDL_PauseAudio(1);
			SDL_Delay(50);
			SDL_CloseAudio();
			audio_opened = 0;
			audio_closing = false;
		}
		gameMenu->g_audioRate.reset();
		saveSram(romPaths.sram.c_str());

		// Multi-disc: persistir indice del disco actual para la proxima carga
		if (!g_currentRompath.empty() && disk_control.get_num_images &&
		    disk_control.get_num_images() > 1 && disk_control.get_image_index) {
			saveLastDiscIndex(g_currentRompath, disk_control.get_image_index());
		}
		g_currentRompath.clear();

		//Liberar recursos de libretro
		 // 1. Limpieza total del juego anterior
		retro_unload_game();
		retro_deinit();
		gameMenu->romLoaded = false;
	}
}

void drawLoadingProgressBar(SDL_Surface* screen, float progress) {
    if (!screen) return;

    // Configuración de dimensiones
    int barW = screen->w / 2;
    int barH = 20;
    int barX = (screen->w - barW) / 2;
    int barY = (screen->h / 2) + 40; // Debajo del texto de "Loading..."

    // Colores (Ajusta según tu paleta)
    Uint32 colorBorder		= SDL_MapRGBA(screen->format, 200, 200, 200, 0xFF);
    Uint32 colorFill		= SDL_MapRGBA(screen->format, bkgMenu.r, bkgMenu.g, bkgMenu.b, 0xFF);
	Uint32 colorFillLighter = SDL_MapRGBA(screen->format, bkgMenuLighter.r, bkgMenuLighter.g, bkgMenuLighter.b, 0xFF);
    Uint32 colorBG			= SDL_MapRGBA(screen->format, 40, 40, 40, 0xFF);
	
    //Dibujar fondo de la barra
    SDL_Rect bgRect = { (Sint16)barX, (Sint16)barY, (Uint16)barW, (Uint16)barH };
	//Actualizamos el area de la barra y el area del texto para que se puedan ver
	SDL_Rect bgRectFill = { bgRect.x, bgRect.y, bgRect.w, barH * 5};
	SDL_FillRect(screen, &bgRectFill, colors[clBackground].color);
	//Mostramos el fondo de la barra
    SDL_FillRect(screen, &bgRect, colorBG);

    //Dibujar el progreso real
    if (progress > 1.0f) progress = 1.0f;
    int fillW = (int)(barW * progress);
    if (fillW > 0) {
        SDL_Rect fillRect = { (Sint16)barX, (Sint16)barY, (Uint16)fillW, (Uint16)(barH / 2.0) };
        SDL_FillRect(screen, &fillRect, colorFillLighter);
		fillRect.y += (Uint16)(barH / 2);
        SDL_FillRect(screen, &fillRect, colorFill);
    }

	const int txtW = 40;
	SDL_Rect percentRect = { barX + barW / 2 - txtW, barY + barH, 80, barH * 4 };
	percentRect.x += txtW;
	percentRect.y += 15;
	PB_drawPercent(screen, (int)(progress * 100.0), percentRect.x, percentRect.y, 3, PBUtil::rgb(screen, 100, 210, 255));

    //Dibujar borde (opcional, 1px)
    //SDL_FillRect no tiene "drawRect" vacío, así que usamos 4 líneas si quieres borde fino
    //Actualizar solo la región de la barra para ganar rendimiento
    //SDL_UpdateRect(screen, barX, barY, barW, 3*barH);
	SDL_FillRect(gameMenu->gameScreen, NULL, colors[clBackground].color);
	SDL_Flip(gameMenu->gameScreen);
}

/**
*
*/
int launchGame(std::string rompath){
	static Uint32 bkgText = SDL_MapRGB(gameMenu->overlay->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
	const bool loadAchievement = gameMenu->getCfgLoader()->configMain[cfg::enableAchievements].valueBool;
	std::string tempDir = Constant::getAppDir() + Constant::getFileSep() + "tmp";
	unzippedFileInfo unzipped;
	struct retro_system_info info;
	memset(&info, 0, sizeof(info));

	std::string initMsg = LanguageManager::instance()->get("msg.loading") + dir.getFileName(rompath) + "...";
	const int face_h_big = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTBIG));

	gameMenu->fillOverlay(clBackground);
	Constant::drawTextCentTransparent(gameMenu->overlay, Fonts::getFont(Fonts::FONTBIG), initMsg.c_str(), 0, face_h_big / 2, true, true, textColor, 0);
	SDL_Flip(gameMenu->gameScreen);

	romPaths.rompath.clear();
	closeGame();
	retro_get_system_info(&info);
	std::string allowedExtensions = Constant::replaceAll(info.valid_extensions, "|", " ");
	LOG_DEBUG("Extensiones: %s\n", info.valid_extensions);

	bool container = false;
	bool isM3U = false;
	detectContainer(rompath, isM3U, container);

	const bool noUncompress = gameMenu->getCfgLoader()->getCfgEmu()->no_uncompress || container;
	if (noUncompress){
		LOG_DEBUG("Loading rom directly %s", rompath.c_str());
		progress_loader.reset();
		progress_loader.total_rom_files = getZipFileCountFiltered(rompath);

		if (!container){
			// Dibujamos la barra inicial al 0%
			drawLoadingProgressBar(gameMenu->overlay, 0.0f);
		}
		unzipped.errorCode = 0;
		unzipped.extractedPath = rompath;
		unzipped.originalPath  = rompath;
	} else {
		if (dir.dirExists(tempDir.c_str())){
			dir.borrarDir(tempDir);
		}
		if (dir.createDir(tempDir) <= 0){
			LOG_ERROR("Error creating the temporary directory %s\n", tempDir.c_str());
			gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.direrror") + tempDir, 3000);
			return 0;
		}
		LOG_DEBUG("Unzipping or loading rom %s", rompath.c_str());
		unzipped = unzipOrLoad(rompath, allowedExtensions, !info.need_fullpath, tempDir);
	}

	if (unzipped.errorCode != 0){
		LOG_ERROR("No se ha podido abrir el fichero o no se puede descomprimir: %s", rompath.c_str());
		gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.openfileerror") + rompath, 3000);
		return 0;
	}

	struct retro_game_info game = { unzipped.extractedPath.c_str(), unzipped.memoryBuffer, unzipped.romsize, NULL };
	retro_init();
	// Multi-disc: si es un M3U, indicamos al core qué disco cargar de inicio
	// antes de retro_load_game (asi el savestate coincide con el disco correcto).
	findInitialImage(rompath, isM3U);
	bool success = retro_load_game(&game);
	
	//Liberar la memoria tras la carga exitosa
	//La mayoría de los cores de Libretro ya han copiado los datos a su propia RAM interna
	//Si hay logros habilitados, ya se encarga de liberarse posteriormente
	if (unzipped.memoryBuffer && !loadAchievement){
		free(unzipped.memoryBuffer);
		unzipped.memoryBuffer = NULL;
	}
	
	if(!success) {
		LOG_ERROR("Error cargando la ROM\n");
		gameMenu->showLangSystemMessage("msg.romopenerror", 3000);
		return 0;
	}

	if (success && loadAchievement && !noUncompress){
		//After the loading of the game, we load the achievements
		gameMenu->loadGameAchievements(unzipped);
	}

	//Loading the rompaths and setting the control device
	g_currentRompath = rompath;
	gameMenu->setRomPaths(rompath);
	gameMenu->joystick->updateTypes();

	//Giving a name to the window 
	string romname = (gameMenu->getCfgLoader()->getCfgEmu() != NULL ? gameMenu->getCfgLoader()->getCfgEmu()->name + " - " : "") + dir.getFileNameNoExt(unzipped.originalPath);
	SDL_WM_SetCaption(romname.c_str(), NULL);

	// Antes de cargar el juego, el core dice su frecuencia en retro_get_system_av_info
	struct retro_system_av_info av_info;
	retro_get_system_av_info(&av_info);

	//Poblamos la lista de cdroms si aplica
	gameMenu->configMenus->poblarCdList(unzipped.originalPath);

	//Obtener el aspect ratio
	aspectRatioValues[RATIO_CORE] = av_info.geometry.aspect_ratio;
	// Inicializar SDL Audio con la frecuencia del core
	if (!audio_opened){
		init_sdl_audio(av_info.timing.sample_rate);
	}
	gameMenu->g_audioRate.init(BUFF_SIZE);
	//Iniciando el contador de fps
	gameMenu->sync->init_fps_counter((float)av_info.timing.fps);
	gameMenu->romLoaded = true;
	gameMenu->configMenus->poblarPartidasGuardadas(gameMenu->getCfgLoader(), rompath);
	loadSram(romPaths.sram.c_str());

	SDL_FillRect(gameMenu->gameScreen, NULL, colors[clBackground].color);
	gameMenu->setEmuStatus(EMU_STARTED);
	gameMenu->clearOverlay();
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
			ret = launchGame(mensaje) == 1;
			LOG_DEBUG("Parametros recibidos: %s\n", mensaje);
		} else if (dwStatus == ERROR_NOT_FOUND) {
			// El programa se lanzó normalmente (sin XSetLaunchData)
			LOG_DEBUG("No se encontraron datos de lanzamiento.\n");
		}
	#else 
		if (argc > 1){
			LOG_DEBUG("argv[1]: %s\n", argv[1]);
			ret = launchGame(argv[1]) == 1;
		}
	#endif	

	if (ret){
		gameMenu->setEmuStatus(EMU_STARTED);
	}
	
	return ret;
}

void closeResources() {
	closeGame();
	Scrapper::ShutdownScrapper();
    if (conversion_buffer != NULL) {
        free(conversion_buffer);
        conversion_buffer = NULL; // Importante ponerlo a NULL tras liberar
        buffer_size = 0;
    }

	deinitSaveSystem();
	Launcher::unmountAll();
	CurlClient curlClient;
	curlClient.close();

	delete logger;
	delete gameMenu;
	delete cfgLoader;
}

inline void updateGame() {
    const Uint32 currentTime = SDL_GetTicks();
    // Verificamos si ha pasado el intervalo desde el último guardado
    if (currentTime - lastSramSaved >= INTERVAL_SRAM_SAVE) {
        saveSram(romPaths.sram.c_str());
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

	if (action_postponed.cycles == 0){
		saveState();
		action_postponed.action = SAVE_NONE;
		action_postponed.cycles = -1;
	}

	switch (hotkey) {
		case HK_SAVESTATE:
			SDL_mutexP(g_saveQueue.saveMutex);
			if (g_saveQueue.action == SAVE_STATE){
				LOG_ERROR("Savestate pending. Aborting new savestate...");
				SDL_mutexV(g_saveQueue.saveMutex);
				break;
			}
			SDL_mutexV(g_saveQueue.saveMutex);

			action_postponed.cycles = 1;
			action_postponed.action = SAVE_STATE;
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
			gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.selectslot") + Constant::intToString(g_currentSlot), 2000);
			break;

		case HK_SLOT_DOWN:
			g_currentSlot = (g_currentSlot - 1 < 0) ? MAX_SAVESTATES - 1 : g_currentSlot - 1;
			gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.selectslot") + Constant::intToString(g_currentSlot), 2000);
			break;

		// ─── Multi-disc (PSX M3U) ────────────────────────────────────────────
		/*case HK_DISC_NEXT:
			if (disk_control.get_num_images && disk_control.get_num_images() > 1) {
				unsigned n   = disk_control.get_num_images();
				unsigned cur = disk_control.get_image_index ? disk_control.get_image_index() : 0;
				swapDisc((cur + 1) % n);
			}
			break;

		case HK_DISC_PREV:
			if (disk_control.get_num_images && disk_control.get_num_images() > 1) {
				unsigned n   = disk_control.get_num_images();
				unsigned cur = disk_control.get_image_index ? disk_control.get_image_index() : 0;
				swapDisc(cur == 0 ? (n - 1) : (cur - 1));
			}
			break;

		case HK_DISC_EJECT:
			if (disk_control.set_eject_state && disk_control.get_eject_state) {
				bool now = !disk_control.get_eject_state();
				disk_control.set_eject_state(now);
				gameMenu->showSystemMessage(now ? "Tray open" : "Tray closed", 1500);
			}
			break;
		*/
		default:
			//LOG_DEBUG("Sending Hotkey %d\n", hotkey);
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
	cfgLoader = new CfgLoader();

	if (cfgLoader->isDebug()){
		#ifndef DEBUG_LOG
		#define DEBUG_LOG
		#endif
        logger->errorLevel = L_DEBUG;
    }

	LOG_DEBUG("appdir: %s\n", Constant::getAppDir().c_str());
	LOG_DEBUG("appexe: %s\n", Constant::getAppExecutable().c_str());

	// Se cargan los textos
	const std::string mainLang = cfgLoader->configMain[cfg::mainLang].valueStr;
	LanguageManager::instance()->loadLanguage(Constant::getAppDir() + "\\assets\\i18n\\" + mainLang + ".ini");
	gameMenu = new GameMenu(cfgLoader);
	ListMenu listMenu(gameMenu->overlay->w, gameMenu->overlay->h);
	listMenu.setLayout(LAYBOXES, gameMenu->overlay->w, gameMenu->overlay->h);

	TileMap tileMap(9, 0, 16, 16);
    tileMap.load(Constant::getAppDir() + Constant::getFileSep() + "assets" + Constant::getFileSep() + "art" + Constant::getFileSep() + "bricks2.png");
	initializeMenus(listMenu, *gameMenu, *cfgLoader);
	
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
		gameMenu->refreshScreen(listMenu);
		listMenu.keyUp = false;
	}

	initSaveSystem();
	//retro_set_controller_port_device(puerto, id_guardado);
	//retro_set_controller_port_device(0, 1);
	
	CurlClient curlClient;
	curlClient.init();
	//scrapper->scrapSystem(*cfgLoader.getCfgEmu(), config);

	double nextFrameTime = Constant::getTicks();
	while (gameMenu->running) {
		// Procesamos eventos como pulsaciones de hotkeys
		processFrontendEvents();

		switch (gameMenu->getEmuStatus()){
			case EMU_STARTED:
				updateGame();
				break;
			case EMU_MENU:
				updateMenuScreen(tileMap, gameMenu, listMenu);
				break;
			case EMU_MENU_OVERLAY:
				updateMenuOverlay(gameMenu, listMenu);
				break;
		}

		// DIBUJO DE INTERFAZ (OSD, FPS, Mensajes)
		gameMenu->processFrontendEventsAfter();

		// Actualizamos la pantalla
		//double before = Constant::getTicks();
		//SDL_SetAlpha(gameMenu->overlay, 0, 0);
		//SDL_BlitSurface(gameMenu->overlay, NULL, SDL_XBOX_GetOverlay(), NULL);
		SDL_Flip(gameMenu->gameScreen);
		//LOG_DEBUG("time ms %.3f\n", Constant::getTicks() - before);
		// Limitamos los frames si tenemos que sincronizar con el video
		if (*gameMenu->current_sync == SYNC_TO_VIDEO){
			gameMenu->sync->limit_fps(nextFrameTime);
		}
	}
	closeResources();
    return 0;
}