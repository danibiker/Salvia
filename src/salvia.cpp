#pragma once

#include "salvia.h"
#include <http/httputil.h>
#include <http/scrapper.h>
#include <http/gamefaqs.h>
#include <cheats/cheatmanager.h>
#include <cheats/rdbreader.h>
#define PICOJSON_USE_RVALUE_REFERENCE 0   // VS2010 no soporta noexcept (ver scrapper.cpp)
#include <http/picojson.h>
#include <io/filelist.h>
#include <cheats/cheatlocator.h>

// Puente entre los eventos SDL del frontend y el callback de teclado que
// el core ha registrado via RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK. La
// direccion del callback es: CORE -> FRONTEND - el frontend lo INVOCA al
// recibir teclas. DOSBox-Pure y otros cores que escuchan typing dependen
// de este puente (no leen retro_input_state(RETRO_DEVICE_KEYBOARD)).
extern "C" void salvia_dispatch_keyboard_event(bool down, unsigned retro_keycode,
                                               uint32_t character, uint16_t modifiers);

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

		// Rotacion solicitada por el core (juegos verticales tipo TATE).
		// Aceptamos el comando y aplicamos la rotacion al framebuffer en sw_refresh.
		case RETRO_ENVIRONMENT_SET_ROTATION: {
			const unsigned *rot = (const unsigned*)data;
			g_screen_rotation = rot ? (*rot & 3u) : 0u;
			LOG_DEBUG("SET_ROTATION solicitada por el core: %u (%u grados CCW)",
			          g_screen_rotation, g_screen_rotation * 90);
			return true;
		}

		case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
			struct retro_vfs_interface_info* vfs_info = (struct retro_vfs_interface_info*)data;
			// Si el core pide version 1, 2 o 3, le damos nuestra v3
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
				g_hasDiskControl = true;
                LOG_DEBUG("Interfaz de control de disco registrada por el core.");
            }
            return true;
        }


        // Es muy probable que en 2026 tambien te pida la version extendida (V1)
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
				g_hasDiskControl = true;
                LOG_DEBUG("Interfaz de control de disco extendida registrada.");
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
            // Al devolver false, el core entiende que este frontend es simple
            // y usara la estructura retro_game_info estandar.
            return false;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            // El core envia un puntero al formato que desea usar
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
			// Aqui le decimos al nucleo que el frontend Si puede duplicar frames.
			*(bool*)data = true; 
			return true;
		}

		case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
			return true;

		case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
			// Al devolver true, le decimos al core: 
			// "Si, puedes pedirme todos los botones de golpe".
			return true;

		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:{
			std::string currentPath = gameMenu->getCfgLoader()->configMain[cfg::libretrosystem].valueStr;

			// strncpy_s copia, asegura el \0 y devuelve 0 si todo ha ido bien.
			// Si currentPath es más grande que dirSystem, el programa fallara de forma segura en modo debug.
			if (strncpy_s(dirSystem, sizeof(dirSystem), currentPath.c_str(), _TRUNCATE) != 0) {
				// Manejo de error si la ruta era demasiado larga
				return false;
			}

			*(const char**)data = dirSystem;
			return true;
		}

		case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:{
			const struct retro_system_av_info *av_info = (const struct retro_system_av_info *)data;
			gameMenu->sync->init_fps_counter((float)av_info->timing.fps);
			gameMenu->g_audioRate.reset();
			gameMenu->g_audioRate.init(BUFF_SIZE);
			/* Un core puede cambiar su tasa a mitad de partida (p.ej. FBNeo al
			 * pasar de cart a CD).  Con el dispositivo fijo esto ya no obliga a
			 * reabrir nada: basta con recalcular el ratio del resampler. */
			if (av_info->timing.sample_rate > 0.0){
				gameMenu->g_audioRate.setRates(av_info->timing.sample_rate,
					(double)g_audio_device_rate);
				LOG_INFO("Audio: el core cambia a %.1f Hz (ratio %.4f)\n",
					av_info->timing.sample_rate, gameMenu->g_audioRate.getBaseRatio());
			}
			if (av_info->geometry.aspect_ratio > 0.0f){
				aspectRatioValues[RATIO_CORE] = av_info->geometry.aspect_ratio;
			}
			return true;
		}

		case RETRO_ENVIRONMENT_SET_GEOMETRY:{
			const struct retro_game_geometry *geom = (const struct retro_game_geometry*)data;
			if (geom && geom->aspect_ratio > 0.0f){
				aspectRatioValues[RATIO_CORE] = geom->aspect_ratio;
			}
			return true;
		}
		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
			std::string currentPath = gameMenu->getSramPath();

            // Copiamos el nuevo path al buffer fijo de forma segura
            strncpy(savePath, currentPath.c_str(), sizeof(savePath) - 1);
            savePath[sizeof(savePath) - 1] = '\0'; // Aseguramos el cierre nulo

            // Entregamos SIEMPRE la misma direccion de memoria
            *(const char**)data = savePath;
            return true;
		}

		case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
		{
			//const struct retro_subsystem_info *info = (const struct retro_subsystem_info*)data;
			//// 1. Limpiar lista de subsistemas previa
			////gameMenu->clearSubsystems();
			//// 2. Iterar y guardar
			//while (info->ident) {
			//	//gameMenu->registerSubsystem(info);
			//	LOG_DEBUG("variable - %s:, %s, %d, %d, %s, %s", info->desc, info->ident, info->id, info->num_roms, info->roms->valid_extensions, info->roms->desc);
			//	info++;
			//}
			return true;
		}
		// RETRO_ENVIRONMENT_SET_VARIABLES (formato clasico) ──────────────────
		case RETRO_ENVIRONMENT_SET_VARIABLES:
		{
			const auto* vars = static_cast<const retro_variable*>(data);
			if (!vars) return false;

			// V0 has no categories: drop any left over from a previous V2 core so the
			// menu stays flat (all these options carry an empty category anyway).
			gameMenu->getCfgLoader()->libretroCategories.clear();

			//Some cores don't publish their options until a rom is loaded, so we need to diferenciate them
			//from the game specific ones to be able to store them separately
			if (!gameMenu->romLoaded && g_currentRompath.empty()){
				processParameters(vars, gameMenu->getCfgLoader()->startupLibretroParams);
			} else {
				//We clear the previous game configuration if it existed
				gameMenu->getCfgLoader()->gameSpecificLibretroParams.clear();
				processParameters(vars, gameMenu->getCfgLoader()->gameSpecificLibretroParams);
			}
			gameMenu->configMenus->poblarCoreOptions(gameMenu->getCfgLoader());
			gameMenu->configMenus->resetIndexPos();

			return true;
		}


		// RETRO_ENVIRONMENT_GET_LANGUAGE ────────────────────────────────────
		case RETRO_ENVIRONMENT_GET_LANGUAGE:
		{
			unsigned* lang = static_cast<unsigned*>(data);
			if (!lang) return false;

			const std::string langStr = gameMenu->getCfgLoader()->configMain[cfg::mainLang].valueStr;

			if (langStr == "es")      *lang = RETRO_LANGUAGE_SPANISH;
			else if (langStr == "fr") *lang = RETRO_LANGUAGE_FRENCH;
			else if (langStr == "de") *lang = RETRO_LANGUAGE_GERMAN;
			else if (langStr == "it") *lang = RETRO_LANGUAGE_ITALIAN;
			else if (langStr == "pt") *lang = RETRO_LANGUAGE_PORTUGUESE_BRAZIL;
			else if (langStr == "ru") *lang = RETRO_LANGUAGE_RUSSIAN;
			else if (langStr == "ko") *lang = RETRO_LANGUAGE_KOREAN;
			else if (langStr == "ja") *lang = RETRO_LANGUAGE_JAPANESE;
			else if (langStr == "nl") *lang = RETRO_LANGUAGE_DUTCH;
			else if (langStr == "pl") *lang = RETRO_LANGUAGE_POLISH;
			else if (langStr == "ar") *lang = RETRO_LANGUAGE_ARABIC;
			else if (langStr == "tr") *lang = RETRO_LANGUAGE_TURKISH;
			else if (langStr == "zh") *lang = RETRO_LANGUAGE_CHINESE_SIMPLIFIED;
			else                      *lang = RETRO_LANGUAGE_ENGLISH;

			return true;
		}

		// RETRO_ENVIRONMENT_GET_VARIABLE ─────────────────────────────────────
		case RETRO_ENVIRONMENT_GET_VARIABLE:
		{
			retro_variable* var = static_cast<retro_variable*>(data);
			if (!var || !var->key) return false;

			//Search first in the core parameters list
			if (!setParameter(var, gameMenu->getCfgLoader()->startupLibretroParams)){
				//If not found, search the parameter in the game specific parameters list
				return setParameter(var, gameMenu->getCfgLoader()->gameSpecificLibretroParams);
			}
			return true;
		}



		// RETRO_ENVIRONMENT_SET_CORE_OPTIONS / _INTL (V1) ──────────────────
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
		{
			const retro_core_option_definition* usDefs = nullptr;
			const retro_core_option_definition* localDefs = nullptr;

			// V1 has no categories: drop any left over from a previous V2 core so the
			// menu stays flat (applyEntry below sets an empty category on every option).
			gameMenu->getCfgLoader()->libretroCategories.clear();

			if (cmd == RETRO_ENVIRONMENT_SET_CORE_OPTIONS) {
				usDefs = static_cast<const retro_core_option_definition*>(data);
				if (!usDefs) return false;
			} else {
				const auto* intl = static_cast<const retro_core_options_intl*>(data);
				if (!intl || !intl->us) return false;
				usDefs = intl->us;
				const std::string lang = gameMenu->getCfgLoader()->configMain[cfg::mainLang].valueStr;
				if (intl->local && lang != "en")
					localDefs = intl->local;
			}

			// Construir un mapa de local key -> index para busqueda rapida
			std::map<std::string, int> localMap;
			if (localDefs) {
				for (int i = 0; localDefs[i].key != nullptr; ++i)
					localMap[localDefs[i].key] = i;
			}

			for (int i = 0; usDefs[i].key != nullptr; ++i) {
				const std::string key = usDefs[i].key;

				// Si existe entrada local, usar su desc y labels; si no, usar us
				const retro_core_option_definition* locEntry = nullptr;
				if (localDefs && localMap.count(key))
					locEntry = &localDefs[localMap[key]];

				const std::string desc = Constant::ansiToUtf8(
					(locEntry && locEntry->desc) ? locEntry->desc :
					(usDefs[i].desc ? usDefs[i].desc : ""));

				std::vector<std::string> values, labels;
				for (int j = 0;
					 j < RETRO_NUM_CORE_OPTION_VALUES_MAX && usDefs[i].values[j].value != nullptr;
					 ++j)
				{
					values.push_back(usDefs[i].values[j].value);
					// Label: de local si existe y no esta vacio, si no de us, si no el value key
					const char* lbl = nullptr;
					if (locEntry && locEntry->values[j].value)
						lbl = locEntry->values[j].label;
					if (!lbl || !*lbl)
						lbl = usDefs[i].values[j].label;
					labels.push_back(lbl && *lbl ? Constant::ansiToUtf8(lbl) : usDefs[i].values[j].value);
				}

				// default_value siempre de us
				int defaultIdx = 0;
				if (usDefs[i].default_value) {
					const std::string defVal = usDefs[i].default_value;
					for (int j = 0; j < static_cast<int>(values.size()); ++j) {
						if (values[j] == defVal) { defaultIdx = j; break; }
					}
				}

				applyEntry(gameMenu->getCfgLoader()->startupLibretroParams, key, desc, values, defaultIdx, labels);
			}
			gameMenu->configMenus->poblarCoreOptions(gameMenu->getCfgLoader());
			gameMenu->configMenus->resetIndexPos();
			return true;
		}

		// RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION ──────────────────────────────
		case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: {
			if (data) *(unsigned*)data = 2; //Soportamos V2
			return true;
		}

		// RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2 / _V2_INTL ──────────────────
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
		{
			const retro_core_options_v2* v2 = nullptr;
			const retro_core_options_v2* usV2 = nullptr;
			//Fbneo uses lots of parameters for cheats and dips, so it is mandatory
			//to clear them before
			gameMenu->getCfgLoader()->gameSpecificLibretroParams.clear();
			gameMenu->getCfgLoader()->libretroCategories.clear();

			if (cmd == RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2) {
				v2 = static_cast<const retro_core_options_v2*>(data);
			} else {
				const auto* intl = static_cast<const retro_core_options_v2_intl*>(data);
				if (!intl || !intl->us) return false;
				usV2 = intl->us;
				v2 = intl->us;
				const std::string lang = gameMenu->getCfgLoader()->configMain[cfg::mainLang].valueStr;
				if (intl->local && lang != "en")
					v2 = intl->local;
			}

			if (!v2 || !v2->definitions) return false;

			const auto defs = v2->definitions;
			std::vector<std::string> categoriesVec(std::begin(gameCategories), std::end(gameCategories));

			// Parse the category definitions (key -> display desc), preserving the
			// order the core declares them. Used to build the per-category submenus.
			// Localized desc comes from v2 (already the local table when _V2_INTL);
			// fall back to the US table by key when the local desc is missing.
			if (v2->categories) {
				auto& cfgCats = gameMenu->getCfgLoader()->libretroCategories;
				for (int c = 0; v2->categories[c].key != nullptr; ++c) {
					const std::string catKey = v2->categories[c].key;
					const char* catDesc = v2->categories[c].desc;
					if (!catDesc && usV2 && usV2->categories) {
						for (int k = 0; usV2->categories[k].key != nullptr; ++k) {
							if (catKey == usV2->categories[k].key) { catDesc = usV2->categories[k].desc; break; }
						}
					}
					cfgCats.push_back(std::make_pair(catKey,
						Constant::ansiToUtf8(catDesc && *catDesc ? catDesc : catKey.c_str())));
				}
			}

			for (int i = 0; defs[i].key != nullptr; ++i) {
				const std::string key  = defs[i].key;

				// Ocultar Debug Dip de FBNeo (neogeo) - no aportan nada al usuario
				if (key.compare(0, 16, "fbneo-debug-dip-") == 0)
					continue;

				// Fallback a us por key (no por indice, los arrays pueden diferir en orden/cantidad)
				const char* srcDesc = defs[i].desc;
				if (!srcDesc && usV2 && usV2->definitions) {
					for (int k = 0; usV2->definitions[k].key != nullptr; ++k) {
						if (key == usV2->definitions[k].key) {
							srcDesc = usV2->definitions[k].desc;
							break;
						}
					}
				}

				// Para las opciones con categoria el core envia ademas desc_categorized:
				// el mismo nombre pero SIN el prefijo redundante (p.ej. FBNeo manda
				// "[Dipswitch] Difficulty" en desc y "Difficulty" en desc_categorized).
				// Como las mostramos dentro de un submenu de categoria, preferimos la
				// version categorizada; fallback a us por key y, si no, al desc normal.
				if (defs[i].category_key) {
					const char* catDescOpt = defs[i].desc_categorized;
					if ((!catDescOpt || !*catDescOpt) && usV2 && usV2->definitions) {
						for (int k = 0; usV2->definitions[k].key != nullptr; ++k) {
							if (key == usV2->definitions[k].key) {
								catDescOpt = usV2->definitions[k].desc_categorized;
								break;
							}
						}
					}
					if (catDescOpt && *catDescOpt)
						srcDesc = catDescOpt;
				}

				const std::string desc = Constant::ansiToUtf8(srcDesc ? srcDesc : "");

				std::vector<std::string> values, labels;
				for (int j = 0;
					 j < RETRO_NUM_CORE_OPTION_VALUES_MAX && defs[i].values[j].value != nullptr;
					 ++j)
				{
					values.push_back(defs[i].values[j].value);
					const char* lbl = defs[i].values[j].label;
					labels.push_back(lbl && *lbl ? Constant::ansiToUtf8(lbl) : defs[i].values[j].value);
				}

				int defaultIdx = 0;
				if (defs[i].default_value) {
					const std::string defVal = defs[i].default_value;
					for (int j = 0; j < static_cast<int>(values.size()); ++j) {
						if (values[j] == defVal) { defaultIdx = j; break; }
					}
				}

				const std::string catKey = defs[i].category_key ? defs[i].category_key : "";

				if (defs[i].category_key != nullptr) {
					auto it = std::find(categoriesVec.begin(), categoriesVec.end(), defs[i].category_key);
					if (it == categoriesVec.end())
						applyEntry(gameMenu->getCfgLoader()->startupLibretroParams, key, desc, values, defaultIdx, labels, catKey);
					else
						applyEntry(gameMenu->getCfgLoader()->gameSpecificLibretroParams, key, desc, values, defaultIdx, labels, catKey);
				} else {
					applyEntry(gameMenu->getCfgLoader()->startupLibretroParams, key, desc, values, defaultIdx, labels, catKey);
				}
			}

			gameMenu->configMenus->poblarCoreOptions(gameMenu->getCfgLoader());
			gameMenu->configMenus->resetIndexPos();

			return true;
		}

		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
			// Solo devolvemos true si el usuario ha tocado algo en el menu
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
            // Aqui el Core te esta diciendo que dispositivos soporta.
            for (unsigned i = 0; info[i].types && i < MAX_PLAYERS; ++i) {
				t_controller_port *port = &gameMenu->joystick->g_ports[i];
				port->available_types.clear();
				for (unsigned j = 0; j < info[i].num_types; j++) {
					unsigned id = info[i].types[j].id;
					const char* desc = info[i].types[j].desc;
					if (desc == NULL) {
						LOG_DEBUG("Puerto %d: Se recibio un descriptor NULL para el ID %u. Saltando...", i, id);
						continue; 
					}
					LOG_DEBUG("Puerto %d soporta: %s (ID: %u)", i, desc, id);
					port->available_types.push_back(std::make_pair(id, desc));
				}
				gameMenu->joystick->getCkeckedJoyTypeIndex(i);
			}
			gameMenu->configMenus->poblarJoystickTypes(gameMenu->joystick);
            return true;
        }
		
		case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: {
			const struct retro_audio_buffer_status_callback *cb = 
				(const struct retro_audio_buffer_status_callback*)data;

			if (cb) {
				audio_status_cb = cb->callback; // Guardamos la funcion que el core llamara
			}
			return true;
		}
		case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: {
			const unsigned *latency_ms = (const unsigned*)data;
			if (latency_ms) {
				unsigned requested_latency = *latency_ms;
				LOG_DEBUG("El core solicita una latencia minima de audio de: %u ms", requested_latency);
				// Aqui deberias ajustar el tamaño de tu buffer de salida de audio (ej. SDL, XAudio2, etc.)
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
		
		case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME : {
			if (data){
				bool supported = *(const bool *) data;
				g_currentCoreSupportsNoGame = supported;
				LOG_DEBUG("Core anuncia SET_SUPPORT_NO_GAME = %d\n", supported ? 1 : 0);
				gameMenu->configMenus->poblarMenuDiscos(g_currentCoreSupportsNoGame ? BOOT_NO_DISK : BOOT_WITH_DISK);
			}
			return true;
		}

		case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK: {
			// El Core te esta dando SU funcion callback. Tu solo la recibes.
			const struct retro_keyboard_callback *cb = (const struct retro_keyboard_callback *)data;
    
			if (cb) {
				core_key_callback = cb->callback;
				LOG_INFO("El Core ha registrado exitosamente su callback de teclado.");
				return true;
			}
			return false;
		}

		//Ignoramos este evento
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: 
			return false;

		case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS:
		{
			 unsigned *core_max_users_ptr = (unsigned*)data;
			 if (core_max_users_ptr)
			 {
				*core_max_users_ptr = MAX_PLAYERS;
				return true; 
			 }
			 return false;
		}

		case 30000: { // RETRO_ENVIRONMENT_GET_SERVERS_JSON
			// Fetch live server list from quakeservers.net
			static std::string cached;
			//if (cached.empty()){
				//Some default servers
				cached = gameMenu->getCfgLoader()->getCfgEmu()->network_default_servers;
				unescape_newlines(cached);
				//Obtained servers from the internet
				cached.append(Scrapper::scrapQuakeList());
			//}
			*(const char **)data = cached.c_str();
			return true;
		}

		case 30001: { // RETRO_ENVIRONMENT_DOWNLOAD_BSP
			const char *filename = *(const char **)data;
			if (!filename || !filename[0]) return false;

			const std::string romPathLaunched = listMenu->listDir.dir + listMenu->listDir.getRelativePath();
			std::string mapsDir = romPathLaunched  + Constant::getFileSep() + "maps";
			std::string localPath = mapsDir + Constant::getFileSep() + filename;

			LOG_DEBUG("Downloading map in: %s", localPath.c_str());

			dirutil d;
			if (!d.dirExists(mapsDir.c_str()))
				d.createDirRecursive(mapsDir.c_str());

			bool ok = false;
			int i=0;

			do{
				std::string url = QUAKE_MAPS_URL[i] + filename;
				CurlClient downloader;
				float progress = 0.0f;
				LOG_DEBUG("Downloading BSP: %s", url.c_str());
				ok = downloader.fetchFile(url, localPath, &progress);
				LOG_DEBUG("Download %s", ok ? "OK" : "FAILED");

				if (ok) {
					// Verify the downloaded file looks like a valid BSP (byte-level, endian-neutral)
					FILE *fp = fopen(localPath.c_str(), "rb");
					if (fp) {
						unsigned char header[4] = {0};
						if (fread(header, 1, 4, fp) == 4) {
							static const unsigned char bsp29[4]  = {0x1D, 0x00, 0x00, 0x00};
							static const unsigned char bsp2dp[4] = {'B', 'S', 'P', '2'};
							static const unsigned char bsp2rmq[4]= {'2', 'S', 'P', 'B'};
							if (memcmp(header, bsp29, 4) != 0 && memcmp(header, bsp2dp, 4) != 0 && memcmp(header, bsp2rmq, 4) != 0) {
								LOG_DEBUG("Downloaded file is not a valid BSP (0x%02x%02x%02x%02x), deleting",
									header[0], header[1], header[2], header[3]);
								ok = false;
							}
						}
						fclose(fp);
					}
					if (!ok)
						remove(localPath.c_str());
				}
				i++;
			} while (!ok && i < QUAKE_MAPS_COUNT);

			return ok;
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

// Rota un framebuffer RGB565 (16 bpp) hacia un buffer denso.
// `rotation` sigue la convencion libretro: 1=90 CCW, 2=180, 3=270 CCW.
// Las dimensiones de salida son (sh x sw) para 90/270 y (sw x sh) para 180.
// `src_pitch_bytes` es el pitch del origen en BYTES; el destino siempre queda
// denso (pitch = dst_w * 2 bytes).
static inline void rotate_buffer_16bpp(const uint16_t* src, unsigned sw, unsigned sh,
                                       std::size_t src_pitch_bytes,
                                       uint16_t* dst, unsigned rotation) {
	const std::size_t spx = src_pitch_bytes >> 1; // pitch en pixeles

	switch (rotation) {
		case 1: { // 90 CCW: dst es sh (ancho) x sw (alto)
			const unsigned dst_w = sh;
			const unsigned dst_h = sw;
			for (unsigned dy = 0; dy < dst_h; ++dy) {
				uint16_t* drow = dst + dy * dst_w;
				const unsigned sx_col = sw - 1 - dy;
				for (unsigned dx = 0; dx < dst_w; ++dx) {
					drow[dx] = src[dx * spx + sx_col];
				}
			}
			break;
		}
		case 2: { // 180
			for (unsigned dy = 0; dy < sh; ++dy) {
				const uint16_t* srow = src + (sh - 1 - dy) * spx;
				uint16_t* drow = dst + dy * sw;
				for (unsigned dx = 0; dx < sw; ++dx) {
					drow[dx] = srow[sw - 1 - dx];
				}
			}
			break;
		}
		case 3: { // 270 CCW (90 CW): dst es sh (ancho) x sw (alto)
			const unsigned dst_w = sh;
			const unsigned dst_h = sw;
			for (unsigned dy = 0; dy < dst_h; ++dy) {
				uint16_t* drow = dst + dy * dst_w;
				for (unsigned dx = 0; dx < dst_w; ++dx) {
					drow[dx] = src[(sh - 1 - dx) * spx + dy];
				}
			}
			break;
		}
		default:
			break;
	}
}

#ifndef SALVIA_GPU_VIDEO
static inline void sw_refresh(const void *data, unsigned width, unsigned height, std::size_t pitch) {
    if (!data || width == 0 || height == 0 || *gameMenu->current_scaler_mode == NO_VIDEO) 
		return;	

    void* final_src = (void*)data;

	//Hacemos la comprobacion del pitch >= width * 4, por si hemos solicitado el RETRO_PIXEL_FORMAT_RGB565
	//pero el core no lo acepta
	if (video_bpp == 16 && fmt != RETRO_PIXEL_FORMAT_RGB565){
		// 2. Gestionar buffer de conversion de forma eficiente
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

	// Rotacion solicitada por el core (juegos verticales tipo agallet, Cave, etc.).
	// Hecha aqui, tras la conversion a RGB565, para que el escalador reciba ya el
	// framebuffer en la orientacion final y trabaje con dimensiones swapped si toca.
	unsigned r_width = width;
	unsigned r_height = height;
	if (g_screen_rotation != 0) {
		const unsigned rw = (g_screen_rotation == 2) ? width : height;
		const unsigned rh = (g_screen_rotation == 2) ? height : width;
		const std::size_t needed = (std::size_t)rw * rh * sizeof(uint16_t);
		if (!rotation_buffer || rotation_buffer_size < needed) {
			uint16_t* tmp = (uint16_t*)realloc(rotation_buffer, needed);
			if (!tmp) return;
			rotation_buffer = tmp;
			rotation_buffer_size = needed;
		}
		rotate_buffer_16bpp((const uint16_t*)final_src, width, height, pitch,
		                    rotation_buffer, g_screen_rotation);
		final_src = (void*)rotation_buffer;
		r_width = rw;
		r_height = rh;
		pitch = rw * sizeof(uint16_t); // destino denso
	}

	if (action_postponed.cycles == 1 && action_postponed.action == SAVE_STATE){
		take_screenshot(final_src, r_width, r_height, pitch);
	}

    SDL_Surface* screen = gameMenu->gameScreen;

	t_scale_props scaleProps;
	scaleProps.src = (uint16_t*)final_src;
	scaleProps.dst = (uint16_t*)screen->pixels;
	scaleProps.sw = (int)r_width;
	scaleProps.sh = (int)r_height;
	scaleProps.spitch = pitch;
	scaleProps.dw = screen->w;
	scaleProps.dh = screen->h;
	scaleProps.dpitch = screen->pitch;
	scaleProps.scale = gameMenu->current_scaler_scale;
	scaleProps.ratio = aspectRatioValues[*gameMenu->current_ratio];
	scaleProps.integer_scale = *gameMenu->current_integer_scale;

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
#endif
/**
*
*/
static inline void hw_refresh(const void *data, unsigned width, 
                                unsigned height, std::size_t pitch) {
    if (!data) return;

    const int bpp = (fmt == RETRO_PIXEL_FORMAT_XRGB8888) ? 32 : 16;
    const unsigned row_bytes = width * (bpp / 8);
    SDL_Surface*& screen = gameMenu->gameScreen;
	t_scale_props &current_video_settings = gameMenu->current_video_settings;

	#ifdef SALVIA_GPU_VIDEO
    if (width  != current_video_settings.sw || height != current_video_settings.sh || bpp != current_video_settings.bpp){
        #ifdef _XBOX
        // El driver SDL de Xbox crea la textura al tamano del core.
		screen = XBOX_ResizeGameTexture(width, height, bpp);
//		screen = SDL_SetVideoMode(width, height, bpp, SDL_DOUBLEBUF);
        #else
        // Windows: recrea la textura D3D9 y su surface al tamano del core
        // SIN tocar la ventana ni el backbuffer.
        screen = WinD3D9_SetGameMode(width, height, bpp);
        #endif
        current_video_settings.sw  = width;
        current_video_settings.sh  = height;
        current_video_settings.bpp = bpp;
		SDL_FillRect(screen, NULL, Constant::colors[clBackground].color);
    }
	#endif

	gameMenu->checkDisplayOptions();

	#ifdef SALVIA_GPU_VIDEO
	// Rotacion HW: gratis en GPU (solo remap de UVs del quad).  No usamos
	// rotation_buffer en la rama GPU; la textura va a VRAM tal cual y
	// los shaders (HQx/CRT/etc.) siguen trabajando con la orientacion nativa.
	{
		static unsigned last_hw_rotation = -100;
		if (last_hw_rotation != g_screen_rotation){
			SDL_XBOX_SetRotation((int)g_screen_rotation);
			last_hw_rotation = g_screen_rotation;
		}
	}
	#endif

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
	#ifdef SALVIA_GPU_VIDEO
		// Xbox y Windows: la GPU escala la textura nativa y aplica el shader.
		hw_refresh(data, width, height, pitch);
	#else
		// Solo plataformas sin ruta GPU: escalado por software.
		sw_refresh(data, width, height, pitch);
	#endif
}

// Despacha un evento SDL_KEYDOWN/KEYUP al core libretro que haya
// registrado un callback de teclado. Se llama desde joystick.cpp en el
// loop de eventos SDL. core_key_callback puede ser NULL si el core no
// usa esta API (p.ej. cores que solo leen retro_input_state).
extern "C" void salvia_dispatch_keyboard_event(bool down, unsigned retro_keycode,
                                               uint32_t character, uint16_t modifiers) {
    if (core_key_callback) {
        core_key_callback(down, retro_keycode, character, modifiers);
    }
}

// Se llama antes de pedir el estado de los inputs
void retro_input_poll(void) {
    update_input();
    // Disparo rapido: fase on/off calculada 1 vez por frame (barato). Solo se consulta
    // dentro de retro_run() -> EMU_STARTED, asi que no tiene efecto fuera del juego.
    t_joy_state *in = &gameMenu->joystick->inputs;
    if (Achievements::instance()->isHardcoreMode()) {
        // Hardcore RetroAchievements: sin disparo rapido. Forzando la fase a "on", el
        // gate de retro_input_state nunca suprime -> el boton se comporta normal.
        in->turboPhaseOn = true;
    } else {
        static const Uint32 RF_HALF_MS[3] = {66, 44, 33}; // lento/medio/rapido (~7.5/11/15 Hz)
        int r = ((unsigned)in->rapidFireRateIdx < 3) ? in->rapidFireRateIdx : 1;
        in->turboPhaseOn = ((SDL_GetTicks() / RF_HALF_MS[r]) & 1) == 0;
    }
}

int16_t retro_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port >= MAX_PLAYERS) 
        return 0;
	
	t_joy_state *inputs = &gameMenu->joystick->inputs;

	if (device == RETRO_DEVICE_JOYPAD && !gameMenu->isOnscreenKeybEnabled()) {
		const int sdlModifier = inputs->mapperHotkeys.getSdlBtn(port, HK_MODIFIER);
		const bool modifierPressed = inputs->getSdlBtn(port, sdlModifier);

		// 1. Gestion del Latch de Start
		#ifdef _XBOX
		// 1. Cachear el valor para evitar multiples accesos al array
		const int holdFrames = gameMenu->joystick->startHoldFrames[port];
		if (holdFrames > 0) {
			// 2. Obtener el indice del boton una sola vez
			int sdlBtn = inputs->mapperCore.getSdlBtn(port, RETRO_DEVICE_ID_JOYPAD_START);
			// 3. Validacion de rango rapida
			if ((unsigned int)sdlBtn < MAX_SDL_BUTTONS) {
				// Forzamos el estado true mientras haya frames de retencion
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
						// Disparo rapido: suprime el bit durante medio ciclo
						if (inputs->rapidFire[port][i] && !inputs->turboPhaseOn) continue;
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
					if (!inputs->getCoreAny(port, id)) return 0;
					// Disparo rapido: suprime la pulsacion durante medio ciclo
					if (inputs->rapidFire[port][id] && !inputs->turboPhaseOn) return 0;
					return 1;
				}
				return (inputs->mapperCore.getSdlBtn(port, id) == sdlModifier ||
						inputs->getCoreAny(port, id)) ? 1 : 0;
			}
			return 0;
		}
	} 
	else if (device == RETRO_DEVICE_ANALOG && !gameMenu->isOnscreenKeybEnabled()) {
		/* Ya no hay numeros de eje cableados aqui. Cada direccion analogica del core
		 * es un slot de mapperCore.analogDst, reasignable desde el menu y guardado
		 * en el .joy; la diferencia entre plataformas vive en los defaults
		 * (analogSlotAxis, beans/structures.h).
		 *
		 * El valor del eje se compone restando sus dos direcciones, lo que da gratis
		 * el eje invertido, el cruzado entre sticks y el asignado a un boton.
		 * Convencion libretro: X+ es derecha e Y+ es ABAJO. */
		int slotNeg = -1, slotPos = -1;

		if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
			if (id == RETRO_DEVICE_ID_ANALOG_X) {
				slotNeg = t_joy_mapper::analogSlot(JOY_AXIS1_LEFT);
				slotPos = t_joy_mapper::analogSlot(JOY_AXIS1_RIGHT);
			} else {
				slotNeg = t_joy_mapper::analogSlot(JOY_AXIS1_UP);
				slotPos = t_joy_mapper::analogSlot(JOY_AXIS1_DOWN);
			}
		} else if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) {
			if (id == RETRO_DEVICE_ID_ANALOG_X) {
				slotNeg = t_joy_mapper::analogSlot(JOY_AXIS2_LEFT);
				slotPos = t_joy_mapper::analogSlot(JOY_AXIS2_RIGHT);
			} else {
				slotNeg = t_joy_mapper::analogSlot(JOY_AXIS2_UP);
				slotPos = t_joy_mapper::analogSlot(JOY_AXIS2_DOWN);
			}
		}
		/* RETRO_DEVICE_INDEX_ANALOG_BUTTON (gatillos analogicos) no se atiende: cae al
		 * return 0 del final.
		 *
		 * Aqui habia una rama para la 360 que leia g_analog_state[port][AXIS_LT/AXIS_RT],
		 * dos slots reservados que NO escribe nadie, asi que devolvia siempre 0 -- pero
		 * PARECIA que hacia algo. En la 360 los gatillos son botones digitales y llegan
		 * por la via JOYPAD; en Windows comparten el eje 2 y se digitalizan en pollKeys.
		 * Si algun dia se quiere el valor analogico del gatillo, hay que EMPEZAR por
		 * producirlo en pollKeys. */

		if (slotNeg != -1 && slotPos != -1) {
			/* El mapeo va de direccion fisica a destino, asi que aqui se pregunta
			 * "quien apunta a esta direccion del core": analogSlotAxis convierte el
			 * slot con nombre en el indice virtual que el core espera recibir. */
			return gameMenu->joystick->inputs.getCoreAnalog(port,
				analogSlotAxis[slotNeg], analogSlotAxis[slotPos]);
		}
	} else if (device == RETRO_DEVICE_MOUSE && !gameMenu->isOnscreenKeybEnabled()) {
		switch (id) {
			case RETRO_DEVICE_ID_MOUSE_X:      return inputs->mouse_rel_x;
			case RETRO_DEVICE_ID_MOUSE_Y:      return inputs->mouse_rel_y;
			case RETRO_DEVICE_ID_MOUSE_LEFT:   return inputs->mouse_buttons[0];
			case RETRO_DEVICE_ID_MOUSE_RIGHT:  return inputs->mouse_buttons[2];
			//case RETRO_DEVICE_ID_MOUSE_WHEELUP:   return (inputs->mouse_wheel > 0);
			//case RETRO_DEVICE_ID_MOUSE_WHEELDOWN: return (inputs->mouse_wheel < 0);
			case RETRO_DEVICE_ID_MOUSE_MIDDLE: return inputs->mouse_buttons[1];
		}
	} else if (device == RETRO_DEVICE_KEYBOARD){
		// Poll-based keyboard query (algunos cores lo usan; otros como
		// DOSBox-Pure usan el callback event-based, ver
		// salvia_dispatch_keyboard_event y RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK).
		if (id < t_joy_state::MAX_RETRO_KEYS) {
			return inputs->keyboard_state[id].keyjoydown ? 1 : 0;
		}
		return 0;
	}
	return 0;
}

//Audio Callbacks for Libretro
// Callback para una sola muestra (menos eficiente, pero requerido)
/* Acumulador de la ruta de muestra suelta (ver retro_audio_sample).  A fichero y
 * no static dentro de la funcion para que closeGame pueda vaciarlo y un juego no
 * herede las muestras a medio bloque del anterior. */
#define RETRO_SAMPLE_BLOCK 256
static int16_t     g_singleSampleBlock[RETRO_SAMPLE_BLOCK * 2];
static std::size_t g_singleSampleFrames = 0;

void retro_audio_sample(int16_t left, int16_t right) {
	/* Ruta de muestra suelta: la usan pocos cores y entregan un frame por
	 * llamada.  Se acumula un bloque antes de remuestrear por dos motivos:
	 * llamar al resampler frame a frame haria que el coste fijo de la llamada
	 * dominase (a 48 kHz son 48.000 llamadas por segundo), y ademas cada lote de
	 * un solo frame apenas deja avanzar la posicion fraccionaria.
	 *
	 * El resto por debajo de un bloque se queda para la siguiente tanda: son
	 * menos de 6 ms y evita tener que enganchar un flush al final del frame. */
	if (gameMenu->current_fast_forward && CfgLoader::configMain[cfg::fastForwardMult].valueInt > 10) return;

	g_singleSampleBlock[g_singleSampleFrames * 2]     = left;
	g_singleSampleBlock[g_singleSampleFrames * 2 + 1] = right;
	g_singleSampleFrames++;

	if (g_singleSampleFrames >= RETRO_SAMPLE_BLOCK) {
		const int mode = *gameMenu->current_sync;
		if (mode != SYNC_FAST_FORWARD) {
			gameMenu->g_audioRate.processAndWrite(gameMenu->g_audioBuffer,
				g_singleSampleBlock, g_singleSampleFrames, mode == SYNC_TO_AUDIO);
		}
		g_singleSampleFrames = 0;
	}
}

// Callback para rafagas de muestras (el que usan casi todos los cores)
std::size_t retro_audio_sample_batch(const int16_t * __restrict data, std::size_t frames) {
	if (gameMenu->current_fast_forward && CfgLoader::configMain[cfg::fastForwardMult].valueInt > 10)
		return frames;
	
	const int mode = *gameMenu->current_sync;
    switch(mode) {
        case SYNC_TO_AUDIO:
            /* El bloqueo ya sincroniza naturalmente con el reloj de audio, pero
             * hay que pasar igualmente por el resampler: el dispositivo esta
             * abierto a tasa fija y escribir crudo sonaria a destiempo. El
             * bloqueo se conserva -- processAndWrite lo propaga al buffer. */
            gameMenu->g_audioRate.processAndWrite(gameMenu->g_audioBuffer, data, frames, true);
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
        return;
    }
	
    int16_t* samples = (int16_t*)stream;
    std::size_t count = len / sizeof(int16_t);

#ifdef AUDIO_LOG
    // Trace periodico cada 2s: callback info
    {
        static DWORD lastCbTrace = 0;
        static DWORD cbCount = 0;
        cbCount++;
        DWORD now = GetTickCount();
        if (now - lastCbTrace >= 2000) {
            DWORD elapsed = now - lastCbTrace;
            lastCbTrace = now;
            float callsPerSec = (float)cbCount * 1000.0f / (float)elapsed;
            cbCount = 0;
            char buf[128];
            _snprintf(buf, sizeof(buf),
                "[SDL_CB] freq=%.1f Hz request=%lu samples (%lu frames)",
                callsPerSec, (unsigned long)count, (unsigned long)(count / 2));
            OutputDebugStringA(buf);
        }
    }
#endif

    gameMenu->g_audioBuffer.Read(samples, count);
}

/**
*
*/
/* Abre el dispositivo de audio.  Se llama UNA vez por sesion, desde el arranque,
 * con AUDIO_DEVICE_RATE: no se reabre al cargar juegos ni cuando un core pide
 * otra tasa.  De adaptar la tasa del core a la del dispositivo se encarga
 * AudioRateControl (audio/audiorate.h). */
void init_sdl_audio(double sample_rate) {
	LOG_DEBUG("init_sdl_audio %.1f\n", sample_rate);
    SDL_AudioSpec wanted, obtained;
    wanted.freq = (int)sample_rate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
	#ifdef WIN
	wanted.samples = 1024;
	#elif defined(_XBOX)
	wanted.samples = 1024; 
	#endif
    wanted.callback = sdl_audio_callback;

#ifdef AUDIO_LOG
    {
        char buf[128];
        _snprintf(buf, sizeof(buf),
            "[AUDIO_INIT] rate=%d samples=%d buffer=%d callback_ms=%.1f",
            wanted.freq, wanted.samples, BUFF_SIZE,
            (float)wanted.samples / (float)wanted.freq * 1000.0f);
        OutputDebugStringA(buf);
    }
#endif

    /* Con `obtained` en vez de NULL: SDL nos dice a que tasa quedo abierto de
     * verdad.  Es el fallback de todo el diseno -- si el driver no da la tasa
     * pedida, el resampler se ajusta a la real y el tono sigue siendo correcto,
     * en vez de sonar desafinado por un desajuste invisible. */
    memset(&obtained, 0, sizeof(obtained));
    if (SDL_OpenAudio(&wanted, &obtained) < 0) {
		string error = "Error SDL Audio: " + string(SDL_GetError());
        LOG_ERROR("%s\n", error.c_str());
        return;
    }
	audio_opened = 1;
	g_audio_device_rate = (obtained.freq > 0) ? obtained.freq : wanted.freq;
	if (g_audio_device_rate != wanted.freq) {
		LOG_INFO("Audio: se pidio %d Hz y el driver abrio %d Hz; el resampler usa la tasa real\n",
			wanted.freq, g_audio_device_rate);
	}
	LOG_INFO("Audio: dispositivo abierto a %d Hz (una sola vez por sesion)\n", g_audio_device_rate);
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
	g_excp_emulator_path = Constant::getAppExecutable();
	return Constant::getAppDir();
}

void closeGame(){
	if (gameMenu->romLoaded){
		/* IMPORTANTE: NO cerrar el dispositivo de audio entre cargas.
		 *
		 * SDL 1.2 en Xbox 360 (libSDLx360) tiene un bug en el ciclo
		 * SDL_OpenAudio / SDL_CloseAudio: tras varias iteraciones, el
		 * proximo SDL_OpenAudio se cuelga silenciosamente y arrastra
		 * todo el frontend.  Dado que PSX siempre es 44100 Hz estereo
		 * S16 y el formato no cambia entre juegos, no hace falta
		 * reabrir.  Estrategia:
		 *   1. Pausar el callback de audio (SDL_PauseAudio(1)).
		 *   2. Delay para dar tiempo a que el callback termine la
		 *      ultima iteracion (no haya races con el AudioBuffer).
		 *   3. Limpiar el AudioBuffer para que el siguiente juego no
		 *      oiga residuos del anterior.
		 *   4. Mantener audio_opened=1 — el device sigue vivo.
		 * En launchGame se reanuda con SDL_PauseAudio(0). */
		if (audio_opened) {
			audio_closing = true;
			SDL_PauseAudio(1);
			SDL_Delay(50);
			gameMenu->g_audioBuffer.Clear();
			audio_closing = false;
			/* audio_opened se mantiene a 1: el device sigue abierto,
			 * solo pausado.  En la siguiente carga se reanuda. */
		}
		/* Bloque a medias de la ruta de muestra suelta: si no se vacia, el
		 * proximo juego empieza oyendo la cola del anterior. */
		g_singleSampleFrames = 0;
		gameMenu->g_audioRate.reset();
#ifndef NO_SRAM
		saveSram(romPaths.sram.c_str());
#endif

		// Multi-disc: persistir indice del disco actual para la proxima carga
		if (!g_currentRompath.empty() && disk_control.get_num_images &&
		    disk_control.get_num_images() > 1 && disk_control.get_image_index) {
			saveLastDiscIndex(g_currentRompath, disk_control.get_image_index());
		}
		g_currentRompath.clear();

		//Liberar recursos de libretro
		 // 1. Limpieza total del juego anterior
		// Cheats: limpiar (retro_cheat_reset) ANTES de descargar el core, con este aun
		// cargado, para que no queden cheats activos al cargar el siguiente juego.
		CheatManager::instance()->clear();

		retro_unload_game();
		retro_deinit();
		gameMenu->romLoaded = false;

		/* Descartar los descriptores capturados via SET_MEMORY_MAPS:
		 * sus punteros apuntan a RAM interna del core que acaba de
		 * liberarse en retro_deinit().  Si el proximo juego/core no
		 * vuelve a emitir SET_MEMORY_MAPS (caso NeoGeo tras NeoGeo CD
		 * en FBNeo) heredariamos punteros colgantes y Achievements::
		 * read_memory haria memcpy sobre memoria invalida. */
		g_num_mem_descriptors = 0;
		memset(g_mem_descriptors, 0, sizeof(g_mem_descriptors));
	}
}

/**
*
*/
int launchGame(std::string rompath, bool tmpDelete){
	// Reset de la rotacion antes de arrancar el core: si el nuevo core no llama
	// a RETRO_ENVIRONMENT_SET_ROTATION, asumimos orientacion estandar (0 grados).
	g_screen_rotation = 0;

	/* Modo "BIOS only": el frontend nos pasa el centinela "@bios-only"
	 * para arrancar la consola sin disco.  El core debe haber anunciado
	 * RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME en retro_set_environment.
	 * En este modo saltamos toda la cadena de decompresion / lectura
	 * de fichero y llamamos retro_load_game(NULL) directamente. */
	const bool bios_only = rompath.compare(BIOS_ONLY) == 0;
	const std::string displayName = bios_only ? std::string("BIOS") : dir.getFileName(rompath);

	std::string initMsg = LanguageManager::instance()->get("msg.loading") + displayName + "...";
	const int face_h_big = Fonts::getLineSkip(Fonts::FONTBIG);
	gameMenu->fillOverlay(clBackground);
	Fonts::drawTextCentTransparent(gameMenu->overlay, Fonts::getFont(Fonts::FONTBIG), initMsg.c_str(), 0, -face_h_big / 2, true, true, Constant::colors[clWhite].sdlColor, 0);
	salviaFlip(gameMenu->gameScreen);

	// [XBOX360] Opciones de core POR JUEGO: si existe <juego>.opt junto al juego,
	// aplicarlas ANTES de extractAndLoadGame -> retro_load_game, para que el core
	// las lea en su primera pasada (incluye init-only). Si no hay, se (re)aplican
	// las generales del core.
	if (!bios_only)
		gameMenu->getCfgLoader()->loadCoreParamsForGame(rompath);

	//Cargamos el juego en memoria o lo extraemos al disco
	bool gameLoaded = extractAndLoadGame(rompath, tmpDelete);
	if(!gameLoaded) {
		LOG_ERROR("Error cargando la ROM\n");
		gameMenu->showLangSystemMessage("msg.romopenerror", 3000);
		g_currentRompath.clear();
		return 0;
	}


	// En BIOS-only usamos el centinela como rom path: setRomPaths
	// genera "@bios-only.sav" y "@bios-only.srm" coherentes para que
	// los savestates del shell de la BIOS no machaquen ningun juego.
	gameMenu->setRomPaths(rompath);

	// Cheats: importar el .cht del juego (formato RetroArch) y aplicarlo al core.
	// Solo se pasan codigos que el core decodifica (retro_cheat_set). En cores que
	// dejan esa funcion vacia (pcsxr-360, beetle-vb, 3dox/opera, dosbox...) es no-op.
	// Cargar la lista de cheats del .cht (formato RetroArch). Arrancan TODOS
	// desactivados y NO se aplica nada en la carga (como RetroArch con "Apply Cheats
	// After Load" desactivado): el usuario los activa a mano desde el menu, ya con el
	// juego arrancado, y se aplican al instante (sApplyCheats). Asi no se parchea la
	// ROM antes de su checksum de arranque, que colgaba a juegos como Sonic 1.
	CheatManager::instance()->loadFromFile(resolveCheatPath(romPaths.cht));
	gameMenu->configMenus->poblarCheats(gameMenu->getCfgLoader());

	gameMenu->joystick->updateTypes();

	//Giving a name to the window
	const std::string captionName = bios_only ? std::string("BIOS") : dir.getFileNameNoExt(rompath);
	std::string romname = (gameMenu->getCfgLoader()->getCfgEmu() != NULL ? gameMenu->getCfgLoader()->getCfgEmu()->name + " - " : "") + captionName;
	SDL_WM_SetCaption(romname.c_str(), NULL);

	// Antes de cargar el juego, el core dice su frecuencia en retro_get_system_av_info
	struct retro_system_av_info av_info = gameMenu->getAvInfo();

	//Poblamos la lista de cdroms si aplica
	if (g_hasDiskControl){
		if (!bios_only) {
			gameMenu->configMenus->poblarCdList(rompath);
		} else {
			std::string execActual = Constant::getAppExecutable();
			ConfigEmu* cfgEmu = gameMenu->getCfgLoader()->findCfgEmu(execActual);
			if (cfgEmu != NULL){
				LOG_DEBUG("Roms dir %s\n", cfgEmu->rom_directory.c_str());
				std::string cdromsPath = dirutil::getPathPrefix(cfgEmu->rom_directory + Constant::getFileSep() + BIOS_ONLY, CfgLoader::configMain[cfg::roms_path].valueStr);
				gameMenu->configMenus->poblarCdList(cdromsPath);
			}
		}
	}

	//Obtener el aspect ratio
	aspectRatioValues[RATIO_CORE] = av_info.geometry.aspect_ratio;
	//Iniciando el contador de fps
	gameMenu->sync->init_fps_counter((float)av_info.timing.fps);
	//Iniciando el sistema de audio
	initGameAudio(av_info.timing.sample_rate);
	gameMenu->romLoaded = true;
	//Deshabilitamos el fast forward si estaba a true y restauramos el vsync por defecto
	if (gameMenu->current_fast_forward){
		gameMenu->current_fast_forward = false;
		SDL_XBOX_SetVSync(true);
	}

	// Lista de partidas guardadas y SRAM solo si hay juego real cargado.
	// En BIOS-only no aplica: el shell de la BIOS solo gestiona memory
	// cards, que el core ya ha cargado via Config.Mcd1/2 en emu_setup.
	if (!bios_only) {
		gameMenu->configMenus->poblarPartidasGuardadas(gameMenu->getCfgLoader(), rompath);
		loadSram(romPaths.sram.c_str());
	}

	SDL_FillRect(gameMenu->gameScreen, NULL, Constant::colors[clBackground].color);
	gameMenu->clearOverlay();
	gameMenu->setEmuStatus(EMU_STARTED);
	//We can reload the emulator if an exception is found from this point over
	g_start_from_exception = false;

#ifdef WATCH_LOAD_STUCK
	watchForLoadingStuck();
#endif
	return 1;
}

bool loadGameAtStart(int argc, char *argv[]){
	LOG_DEBUG("argc: %d\n", argc);
	bool ret = false;
	std::string romToLaunch;

	#ifdef _XBOX
		DWORD dwLaunchDataSize = 0;    
		DWORD dwStatus = XGetLaunchDataSize( &dwLaunchDataSize );
		if( dwStatus == ERROR_SUCCESS ){
			BYTE* pLaunchData = new BYTE [ dwLaunchDataSize ];
			dwStatus = XGetLaunchData( pLaunchData, dwLaunchDataSize );
			romToLaunch = (char*)pLaunchData;
			LOG_DEBUG("Parametros recibidos: %s\n", romToLaunch.c_str());
			//If we come from an exception, don't launch anything
			if (START_FROM_EXCEPTION.compare(romToLaunch) == 0){
				LOG_DEBUG("Starting emulator from a previous exception");
				g_start_from_exception = true;
			} else {
				ret = launchGame(romToLaunch) == 1;	
			}
		} else if (dwStatus == ERROR_NOT_FOUND) {
			// El programa se lanzo normalmente (sin XSetLaunchData)
			LOG_DEBUG("No se encontraron datos de lanzamiento.\n");
		}
	#else 
		if (argc > 1){
			romToLaunch = argv[1];
			LOG_DEBUG("argv[1]: %s\n", argv[1]);
			ret = launchGame(argv[1]) == 1;
		}
	#endif	

	if (ret){
		gameMenu->setEmuStatus(EMU_STARTED);
		//Setting the romname to the Faqs downloader menu
		gameMenu->configMenus->setGameLoaded(romToLaunch);
	}
	
	return ret;
}



inline void updateGame() {
    #ifndef NO_SRAM
	const Uint32 currentTime = SDL_GetTicks();
    // Verificamos si ha pasado el intervalo desde el ultimo guardado
    if (currentTime - lastSramSaved >= INTERVAL_SRAM_SAVE) {
        saveSram(romPaths.sram.c_str());
        lastSramSaved = currentTime;
    }
	#endif
	// retro_run hace todo: 
	// 1. Llama a input_poll() -> update_input()
	// 2. Calcula la logica del juego
	// 3. Llama a audio_batch() -> (Aqui el audio bloquea si va muy rapido)
	// 4. Llama a video_refresh() -> (Aqui se dibuja el frame y los FPS)
	// Notificar al core el estado del buffer de audio (para frameskip)
	if (audio_status_cb) {
		size_t fill = gameMenu->g_audioBuffer.getUsed();
		unsigned occupancy = (unsigned)(fill * 100 / gameMenu->g_audioBuffer.getCapacity());
		bool underrun_likely = (fill < gameMenu->g_audioBuffer.getCapacity() / 4);
		audio_status_cb(true, occupancy, underrun_likely);
	}
	retro_run();
}

void processFrontendEvents(){
	// Procesamos el teclado virtual de Xbox (si hay uno pendiente)
	SOUtils::updateKeyboard();
	
	//Procesamos las hotkeys
	HOTKEYS_LIST hotkey = gameMenu->joystick->findHotkey();

	if (action_postponed.cycles == 0){
		saveState();
		action_postponed.action = SAVE_NONE;
		action_postponed.cycles = -1;
	}

	switch (hotkey) {
		case HK_SAVESTATE:
			#ifdef NO_SAVEGAMES
				gameMenu->showLangSystemMessage("msg.filesave.forbidden", 3000);
			#else
				SDL_mutexP(g_saveQueue.saveMutex);
				if (g_saveQueue.action == SAVE_STATE){
					LOG_ERROR("Savestate pending. Aborting new savestate...");
					SDL_mutexV(g_saveQueue.saveMutex);
					break;
				}
				SDL_mutexV(g_saveQueue.saveMutex);

				action_postponed.cycles = 1;
				action_postponed.action = SAVE_STATE;
			#endif
			break;

		case HK_LOADSTATE:
			// loadState debe ejecutarse en el hilo principal		
			#ifdef NO_SAVEGAMES
				gameMenu->showLangSystemMessage("msg.filesave.forbidden", 3000);
			#else
				loadState();
			#endif
			break;

		case HK_MAX:
			// No hacemos nada para el valor limite
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
			// Cualquier otro hotkey (ej. volumen, reset, menu) se delega al frontend
			gameMenu->processFrontendEvents(hotkey);
			break;
	}
}

void closeResources() {
	closeGame();
	Scrapper::ShutdownScrapper();
    if (conversion_buffer != NULL) {
        free(conversion_buffer);
        conversion_buffer = NULL; // Importante ponerlo a NULL tras liberar
        buffer_size = 0;
    }
    if (rotation_buffer != NULL) {
        free(rotation_buffer);
        rotation_buffer = NULL;
        rotation_buffer_size = 0;
    }

	deinitSaveSystem();
	Launcher::unmountAll();
	CurlClient curlClient;
	curlClient.close();

	delete logger;
	delete listMenu;
	delete gameMenu;
	delete cfgLoader;
}

static void __declspec(noinline) printAndDelay(){
	const int face_h_big = Fonts::getLineSkip(Fonts::FONTBIG);
	gameMenu->fillOverlay(clBackground);
	Fonts::drawTextCentTransparent(gameMenu->overlay, Fonts::getFont(Fonts::FONTBIG), LanguageManager::instance()->get("msg.error.fatal").c_str(), 0, -face_h_big / 2, true, true, Constant::colors[clWhite].sdlColor, 0);
	salviaFlip(gameMenu->gameScreen);
	SDL_Delay(3000);

	//We don't want the emulator restarting over and over if an exception is thrown previously, 
	//without starting a new game. The g_start_from_exception flag is cleared when the game is loaded
	if (!g_start_from_exception){
		Launcher launch;
		std::string pams = START_FROM_EXCEPTION;
		launch.launchXboxWin(Constant::getAppDir() + Constant::getFileSep() + Constant::getAppExecutable(), pams);
	}
}

/*  runGameLoop — extracted into its own function so __try does not
 *  share a stack frame with C++ objects that have destructors.
 *  MSVC error C2712 forbids SEH __try in any function that requires
 *  C++ object unwinding. */
static void __declspec(noinline) runGameLoop() {
	__try {
		while (gameMenu->running) {
			processFrontendEvents();

			switch (gameMenu->getEmuStatus()){
				case EMU_STARTED:
					updateGame();
					break;
				case EMU_MENU:
				case EMU_MENU_FILTER:
				case EMU_MENU_IMAGE_VIEWER:
					updateMenuScreen(tileMap, gameMenu, *listMenu);
					break;
				case EMU_MENU_OVERLAY:
					updateMenuOverlay(gameMenu, *listMenu);
					break;
			}

			gameMenu->processFrontendEventsAfter();

			/* Se presenta en cuanto el frame esta listo y la espera del
			 * limitador va DESPUES: asi la latencia de input es la minima.
			 * (Probado el orden contrario -esperar y luego presentar- para
			 * equiespaciar las presentaciones: sin diferencia apreciable ni con
			 * vsync ni sin el, asi que no compensa la latencia de mas.) */
			const double flipIni = Constant::getTicks();
			salviaFlip(gameMenu->gameScreen);
			const double flipEnd = Constant::getTicks();
			gameMenu->sync->note_flip(flipEnd - flipIni);
			gameMenu->sync->note_present(flipEnd);
			gameMenu->sync->limit_fps(nextFrameTime, *gameMenu->current_sync, gameMenu->gameTicks);
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		printAndDelay();
		LOG_ERROR("FATAL: Unhandled exception");
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

	/* Dispositivo de audio: se abre AQUI, una sola vez, a tasa fija, y ya no se
	 * vuelve a abrir en toda la sesion (el ciclo SDL_OpenAudio/SDL_CloseAudio
	 * cuelga libSDLx360 en la 360).  Cada core entrega a su tasa y AudioRateControl
	 * la adapta.
	 *
	 * Tiene que ir despues de crear gameMenu: sdl_audio_callback lee
	 * gameMenu->g_audioBuffer.  Se deja PAUSADO hasta que haya juego, que es el
	 * mismo ciclo de siempre (closeGame pausa, initGameAudio reanuda). */
	init_sdl_audio(AUDIO_DEVICE_RATE);
	if (audio_opened) SDL_PauseAudio(1);

	listMenu = new ListMenu(gameMenu->overlay->w, gameMenu->overlay->h);
	listMenu->setLayout(LAYBOXES, gameMenu->overlay->w, gameMenu->overlay->h);
    tileMap.load(Constant::getAppDir() + Constant::getFileSep() + "assets" + Constant::getFileSep() + "art" + Constant::getFileSep() + "bricks2.png");
	initializeMenus(*listMenu, *gameMenu, *cfgLoader);
	
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

	if (loadGameAtStart(argc, argv)){
		gameMenu->clearOverlay();
	}

	ConfigEmu *emu = cfgLoader->getCfgEmu();
	gameMenu->keyb->setKeyboardLayout(emu->keyboard_type, gameMenu->overlay->w, gameMenu->overlay->h);
	initSaveSystem();
	CurlClient curlClient;
	curlClient.init();

	nextFrameTime = Constant::getTicks();
	//Estado inicial del fondo HLSL: el constructor fija EMU_MENU directo,
	//saltandose setEmuStatus, asi que lo sincronizamos aqui una vez.
	gameMenu->applyMenuBackground();
	runGameLoop();
	closeResources();
    return 0;
}
