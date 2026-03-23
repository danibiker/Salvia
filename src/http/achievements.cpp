#include "achievements.h"

#include <cstdio>
#include <string.h>

#include <http/badgedownloader.h>
#include <utils/Logger.h>
#include <io/dirutil.h>
#include <gfx/SDL_rotozoom.h>
#include <SDL_image.h>
#include <retroachievements/CHDHashed.h>
#include <rc_consoles.h>
#include <libretro/libretro.h>

/* ---------- Deteccion de endianness del host en tiempo de compilacion ----------
 * Se usa para decidir si el core necesita byte-swap en read_memory().
 * En hosts little-endian (PC), los cores con CPU 68000 (Genesis, Sega CD, 32X)
 * ya hacen byte-swap de cada word de 16 bits en work_ram por rendimiento, y ese
 * es el layout contra el que RetroAchievements escribe sus condiciones.
 * En hosts big-endian (Xbox 360 PowerPC), el core almacena work_ram en orden
 * nativo 68000 (BE), por lo que debemos hacer XOR de cada direccion con 1 para
 * replicar el layout LE que esperan los logros. */
#if defined(_XBOX) 
#define SALVIA_HOST_IS_BIG_ENDIAN 1
#else
#define SALVIA_HOST_IS_BIG_ENDIAN 0
#endif

/* ---------- Debug trace for rcheevos conditions ----------
 * These variables live in rc_client.c.  Set g_rc_debug_achievement_id
 * to the numeric achievement ID you want to trace (0 = disabled).
 * g_rc_debug_frame_interval controls how often the trace fires
 * (1 = every frame, 60 = ~once/sec at 60 fps).  Traces also fire
 * immediately on any state change or measured-value change. */
extern "C" {
  extern uint32_t g_rc_debug_achievement_id;
  extern uint32_t g_rc_debug_frame_interval;
}

void clearBadgeCache();
static const std::string SALVIA_USER_AGENT = "Salvia/1.0";

void Achievements::initialize() {
	if (g_client) return;
	createDbAchievements();
	progressMutex = SDL_CreateMutex();

	// Creamos el cliente pasando la funcion de lectura de memoria
	g_client = rc_client_create(read_memory, server_call);
	// Habilitar logs detallados para debug
	rc_client_enable_logging(g_client, RC_CLIENT_LOG_LEVEL_VERBOSE, log_message);

	/* -------- TRAZA DE CONDICIONES DE UN LOGRO --------
	 * Cambia el ID para trazar otro logro. Pon 0 para desactivar.
	 * g_rc_debug_frame_interval controla cada cuantos frames se imprime
	 * (siempre se imprime inmediatamente ante cambios de estado o progreso). */
	g_rc_debug_achievement_id  = 0;   /* <-- PON AQUI EL ID DEL LOGRO (ej. 260) */
	g_rc_debug_frame_interval  = 60;  /* 60 = ~1 vez/seg a 60fps, 1 = cada frame */

	#ifdef _XBOX
	// REGISTRA LA FUNCION DE TIEMPO (Crucial para Triggers/Challenges)
    //rc_client_set_get_time_millisecs_function(g_client, get_xbox_clock_millis);
	#endif
	// Provide an event handler
	rc_client_set_event_handler(g_client, event_handler);
	// Hardcore 0 para evitar baneos accidentales durante el desarrollo
	rc_client_set_hardcore_enabled(g_client, hardcoreMode ? 1 : 0);

	LOG_DEBUG("RetroAchievements: Cliente inicializado.");
}

void Achievements::shutdown() {
	if (g_client) {

		if (gameState != NULL){
			updatePlayTime(gameState->id);
			delete gameState;
			gameState = NULL;
		}

		rc_client_destroy(g_client);
		g_client = NULL;
		#ifndef NO_DATABASE
		delete achievementDb;
		#endif
		clearAllData();
		SDL_DestroyMutex(progressMutex);
		LOG_DEBUG("RetroAchievements: Cliente destruido.");
	}
}
void Achievements::createDbAchievements() {
	#ifndef NO_DATABASE
	dirutil dir;
	std::string rutadb = Constant::getAppDir() + Constant::getFileSep() + "data" + Constant::getFileSep() + "db";
	if (!dir.dirExists(rutadb.c_str())){
		dir.createDirRecursive(rutadb.c_str());
	}
	rutadb += Constant::getFileSep() + "achievements.db";
	//rutadb = "cache:\\achievements.db";
	achievementDb = new AchievementDB(rutadb.c_str());
	achievementDb->createTable();
	#endif
}

void Achievements::clearAllData(){
	reset_menu();
	challenges.clear();
	trackers.clear();
	clearBadgeCache();
}

// --- Threaded methods ---
DWORD WINAPI ServerThreadFunction(LPVOID lpParam) {
	ServerCallData* data = (ServerCallData*)lpParam;

	CurlClient curlClient;
	float progress = 0;

	// Ejecutamos la peticion
	bool success = false;
	if (!data->post_data.empty()) {
		success = curlClient.postUrl(data->url, data->post_data, SALVIA_USER_AGENT, data->response, &progress);
	} else {
		success = curlClient.fetchUrl(data->url, data->response, &progress);
	}

	// Preparamos la respuesta para la libreria
	rc_api_server_response_t server_response;
	memset(&server_response, 0, sizeof(server_response));
	server_response.body = data->response.c_str();
	server_response.body_length = data->response.length();
	server_response.http_status_code = success ? 200 : 500;

	// Ejecutamos el callback
	data->callback(&server_response, data->callback_data);

	// Limpiamos la memoria de la estructura y cerramos
	delete data;
	return 0;
}

DWORD WINAPI AchievementTriggeredThread(LPVOID lpParam) {
	AchievementEventData* data = (AchievementEventData*)lpParam;
	Achievements& self = *Achievements::instance();

	if (data) {
		AchievementState msg;
		msg.title = data->title;
		msg.description = data->description;
		msg.badgeUrl = data->badgeUrl;
		msg.type = data->type;
		msg.badgeName = data->badgeName;
		msg.id = data->id;
		LOG_DEBUG("AchievementTriggeredThread: %s - %s", msg.title.c_str(), msg.description.c_str());
		int line_height, badgeW, badgeH, badgePad;
		Fonts::getBadgeSize(badgeW, badgeH, badgePad, line_height);
		self.download_and_cache_image(&msg, badgeW, badgeH);
		self.setShouldRefresh(true);
		self.messages.add(msg); // Anyadimos a la cola de forma "thread safe" con el add
		//Aunque el destructor de AchievementState, ahora mismo, no libera memoria, para curarnos 
		//en salud por si en un futuro esto cambia, ponemos a null los campos de la variable local msg
		msg.badge = NULL;       // Crucial para evitar que el destructor local
		msg.badgeLocked = NULL; // de 'msg' toque la memoria que ahora es del vector.
	}
	delete data;
	LOG_DEBUG("AchievementTriggeredThread");
	return 0;
}

void clearBadgeCache() {
	LOG_DEBUG("Limpiando badges");
    Achievements* ach = Achievements::instance();
    if (!ach) return;
	ach->badgeCache.clear();
    LOG_DEBUG("Cache de badges limpiada con éxito");
}

DWORD WINAPI LoadGameThreadFunction(LPVOID lpParam) {
	LoadGameThreadData* data = (LoadGameThreadData*)lpParam;
	LoadContext* ctx = static_cast<LoadContext*>(data->userdata);
	Achievements& self = *Achievements::instance();

	LOG_DEBUG("Start of LoadGameThreadFunction");
	if (ctx->romBuffer) {
		free(ctx->romBuffer); // LIBERACION SEGURA: La identificacion ya termino
		ctx->romBuffer = NULL;
	}

	int procReq = rc_client_is_processing_required(data->client);
	if (procReq <= 0 && self.isHardcoreMode()){
		self.setHardcoreMode(false);
	}

	if (data->result == RC_OK) {
		//Se liberan todas las superficies
		self.clearAllData();

		bool prevGameLoaded = self.gameState != NULL;
		if (prevGameLoaded){
			//Si ya habiamos cargado un juego, actualizamos el tiempo de juego 
			//del anterior antes de cargar el nuevo
			self.updatePlayTime(self.getGameId());
			//Borramos la memoria
			delete self.gameState;
			self.gameState = NULL;
		}

		// Llamamos a los metodos de la clase a traves de la instancia
		self.show_game_placard(data->client);
		self.updateAchievements(data->client);
		if (ctx->messages) {
			self.send_message_game_loaded();
		}

	} else {
		std::string errorMsg = rc_error_str(data->result);
		LOG_DEBUG("Error al cargar logros del juego: %s", errorMsg.c_str());
	}
	delete ctx; // Liberamos nuestra estructura intermedia
	delete data;
	return 0;
}

DWORD WINAPI ChallengeIndicatorThread(LPVOID lpParam) {
    ChallengeThreadData* data = (ChallengeThreadData*)lpParam;
    Achievements& self = *Achievements::instance();

    LOG_DEBUG("Downloading challenge: %d", data->id);

    // 1. Descarga de la imagen (fuera del mutex para no bloquear el juego)
    SDL_Surface* temp_badge = NULL;
    int badgeW, badgeH, badgePad, line_height;
    Fonts::getBadgeSize(badgeW, badgeH, badgePad, line_height);
    
    // Asumimos que esta función devuelve la superficie ya procesada
    self.download_and_cache_image(data->badgeUrl, data->id, temp_badge, badgeW, badgeH);

    // 2. Integración en la estructura th_challenge (Thread-Safe)
    challenge_data existing;
    if (self.challenges.find(data->id, existing)) {
        // Si ya existía (porque alguien llamó a hide() antes de que terminara la descarga)
        if (existing.pending_hide) {
            existing.active = false;
            existing.pending_hide = false; // Ya hemos procesado la cancelación
        } else {
            existing.active = true;
        }
        existing.badge = temp_badge;
        self.challenges.add(data->id, existing);
    } else {
        // Es un reto nuevo y no se ha cancelado durante la descarga
        challenge_data c_data;
        c_data.id = data->id;
        c_data.active = true;
        c_data.badge = temp_badge;
        c_data.pending_hide = false;
        self.challenges.add(data->id, c_data);
    }

    LOG_DEBUG("Challenge ready: %d", data->id);
    delete data;
    return 0;
}

DWORD WINAPI ProgressIndicatorThread(LPVOID lpParam) {
    ProgressThreadData* data = (ProgressThreadData*)lpParam;
    Achievements& self = *Achievements::instance();
    
    SDL_Surface* tempBadge = NULL;
    int badgeW, badgeH, badgePad, lh;
    Fonts::getBadgeSize(badgeW, badgeH, badgePad, lh);

    // 1. DESCARGA (Fuera del mutex para no bloquear el renderizado)
    self.download_and_cache_image(data->badgeUrl, data->id, tempBadge, badgeW, badgeH);

    // 2. ACTUALIZACIÓN ATÓMICA
    // El método update se encarga del ScopedLock internamente
    self.progress.update(data->id, data->measured_progress, tempBadge);

    delete data;
    LOG_DEBUG("Progress indicator updated thread-safely");
    return 0;
}

void Achievements::login(const char* username, const char* password) {
	if (!g_client) initialize();
	LOG_DEBUG("RetroAchievements: Intentando login para %s...", username);
	rc_client_begin_login_with_password(g_client, username, password, login_callback, this);
}

void Achievements::load_game(const uint8_t* rom, size_t rom_size, std::string path, uint32_t console_id, th_messages& messagesAchievement) {
	LoadContext* ctx = new LoadContext();
	ctx->messages = &messagesAchievement;
	ctx->romBuffer = (void*)rom;
	char romHash[33] = {0};
	shouldRefresh = true;

	current_console_id = console_id;

	/* --- Deteccion automatica de byte-swap ---
	 * Cores que emulan CPUs big-endian de 16 bits (Motorola 68000) almacenan
	 * work_ram en orden nativo cuando el host tambien es BE.  Los logros de
	 * RetroAchievements estan escritos contra el layout LE (byte-swapped) que
	 * se usa en PC, asi que en hosts BE necesitamos XOR cada direccion con 1
	 * para intercambiar bytes dentro de cada word de 16 bits.
	 * En hosts LE esto NO es necesario porque el core ya hace el swap. */
#if SALVIA_HOST_IS_BIG_ENDIAN
	switch (console_id) {
		case RC_CONSOLE_MEGA_DRIVE:  /* Genesis / Mega Drive (68000) */
		case RC_CONSOLE_SEGA_CD:     /* Sega CD / Mega CD (68000) */
		case RC_CONSOLE_SEGA_32X:    /* 32X (68000 + SH-2) */
			byte_swap_memory = true;
			break;
		default:
			byte_swap_memory = false;
			break;
	}
	LOG_DEBUG("RetroAchievements: console_id=%u, byte_swap_memory=%s",
	          console_id, byte_swap_memory ? "YES" : "NO");
#else
	byte_swap_memory = false;
#endif

	/* Construir mapa de memoria basado en las regiones de la consola */
	build_memory_map(console_id);

#ifdef HAVE_CHD
	std::string hashStr;
	std::string lowPath = path;
	Constant::lowerCase(&lowPath);

	if (lowPath.find(".chd") != std::string::npos) {
		CHDHashed chdhashed;
		hashStr = chdhashed.GetHash(path, console_id);

		if (!hashStr.empty()) {
			// Copiamos el string al array char[33]
			// strncpy asegura que no nos pasemos del tamao del buffer
			strncpy(romHash, hashStr.c_str(), sizeof(romHash) - 1);
			romHash[32] = '\0'; // Aseguramos el terminador nulo

			// IMPORTANTE: Ahora inicias la carga con el hash generado del CHD
			rc_client_begin_load_game(g_client, romHash, load_game_callback, (void*)ctx);
		} else {
			// Manejar error: No se pudo generar hash del CHD
			delete ctx;
		}
	} else 
#endif		
		// CASO NORMAL: ROM en memoria
		if (rom != NULL && rom_size > 0) {
			rc_client_begin_identify_and_load_game(g_client, console_id, NULL, rom, rom_size, load_game_callback, (void*)ctx);
		} 
		// CASO: Archivo plano
		else if (!path.empty()) {
			if (rc_hash_generate_from_file(romHash, console_id, path.c_str())) {
				rc_client_begin_load_game(g_client, romHash, load_game_callback, (void*)ctx);
			}
		}
}

// --- CALLBACKS ---

void Achievements::login_callback(int result, const char* error_message, rc_client_t* client, void* userdata) {
	Achievements* self = (Achievements*)userdata;

	if (result != RC_OK) {
		LOG_DEBUG("RetroAchievements Error: %s", error_message ? error_message : "Unknown error");
		return;
	}

	const rc_client_user_t* user = rc_client_get_user_info(client);
	if (user) {
		self->ra_user = user->username;
		self->ra_token = user->token;
		self->ra_score = user->score;
		LOG_DEBUG("RetroAchievements: Login exitoso. Usuario: %s | Puntos: %u", user->display_name, user->score);
	}
}

void Achievements::load_game_callback(int result, const char* error_message, rc_client_t* client, void* userdata)
{
	LOG_DEBUG("load_game_callback");
	// Creamos el paquete de datos
	LoadGameThreadData* data = new LoadGameThreadData();
	data->result = result;
	data->error_message = error_message ? error_message : "";
	data->client = client;
	data->userdata = userdata;

	// Lanzamos el hilo
	HANDLE hThread = CreateThread(NULL, 0, LoadGameThreadFunction, (LPVOID)data, CREATE_SUSPENDED, NULL);

	if (hThread) {
		Constant::setup_and_run_thread(hThread, CPU_THREAD);
	} else {
		delete data; // Limpieza en caso de error
	}
}

/* ---------- build_memory_map ----------
 * Construye el mapa de traduccion de direcciones usando las definiciones
 * de regiones de rcheevos (rc_console_memory_regions).  Cada region de
 * tipo SYSTEM_RAM se mapea secuencialmente al buffer wram_ptr, y cada
 * region SAVE_RAM al buffer sram_ptr.
 *
 * Esto hace que read_memory funcione automaticamente para TODAS las
 * consolas: Game Boy (WRAM en 0xC000, HRAM en 0xFF80), NES (WRAM en
 * 0x0000, SRAM en 0x6000), Genesis (flat), SNES, GBA, etc. */
void Achievements::build_memory_map(uint32_t console_id) {
    memory_map_count = 0;
    memset(memory_map, 0, sizeof(memory_map));

    const rc_memory_regions_t* regions = rc_console_memory_regions(console_id);
    if (!regions || regions->num_regions == 0) {
        LOG_DEBUG("RetroAchievements: No memory regions for console %u", console_id);
        return;
    }

    uint32_t system_ram_used = 0;  /* bytes acumulados de SYSTEM_RAM */
    uint32_t save_ram_used = 0;    /* bytes acumulados de SAVE_RAM */
    uint32_t i;

    for (i = 0; i < regions->num_regions && memory_map_count < MAX_MEMORY_MAPPINGS; i++) {
        const rc_memory_region_t* rgn = &regions->region[i];
        uint32_t region_size = rgn->end_address - rgn->start_address + 1;

        if (rgn->type == RC_MEMORY_TYPE_SYSTEM_RAM) {
            if (wram_ptr && (system_ram_used + region_size) <= wram_size) {
                memory_map[memory_map_count].start  = rgn->start_address;
                memory_map[memory_map_count].end    = rgn->end_address;
                memory_map[memory_map_count].buffer = wram_ptr;
                memory_map[memory_map_count].offset = system_ram_used;
                memory_map_count++;
                LOG_DEBUG("  MEM MAP[%u]: 0x%05X-0x%05X -> WRAM+0x%X (%s)",
                          memory_map_count - 1, rgn->start_address,
                          rgn->end_address, system_ram_used, rgn->description);
            } else {
                /* FALLBACK: buscar en los descriptores del core (SET_MEMORY_MAPS)
                 * un descriptor cuyo rango contenga esta region.
                 * Esto resuelve regiones como HRAM en Game Boy que no caben en
                 * el buffer de retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM). */
                bool found = false;
                if (core_descriptors && core_descriptor_count > 0) {
                    for (unsigned d = 0; d < core_descriptor_count; d++) {
                        const struct retro_memory_descriptor* desc = &core_descriptors[d];
                        if (!desc->ptr) continue;
                        /* Comprobar si el descriptor cubre la direccion de inicio
                         * de esta region.  Para descriptores simples (select=0),
                         * start+len define el rango.  Para descriptores con select,
                         * se compara con mascara. */
                        if (desc->select == 0) {
                            /* Descriptor simple: start..start+len */
                            if (desc->len > 0 &&
                                rgn->start_address >= desc->start &&
                                rgn->end_address < desc->start + desc->len) {
                                uint32_t desc_offset = (uint32_t)(desc->offset +
                                    (rgn->start_address - desc->start));
                                memory_map[memory_map_count].start  = rgn->start_address;
                                memory_map[memory_map_count].end    = rgn->end_address;
                                memory_map[memory_map_count].buffer = (uint8_t*)desc->ptr;
                                memory_map[memory_map_count].offset = desc_offset;
                                memory_map_count++;
                                found = true;
                                LOG_DEBUG("  MEM MAP[%u]: 0x%05X-0x%05X -> CORE_DESC[%u] ptr=%p+0x%X (%s)",
                                          memory_map_count - 1, rgn->start_address,
                                          rgn->end_address, d, desc->ptr, desc_offset,
                                          rgn->description);
                                break;
                            }
                        } else {
                            /* Descriptor con select: la direccion coincide si
                             * (addr & select) == (desc->start & select) */
                            if ((rgn->start_address & desc->select) ==
                                (desc->start & desc->select)) {
                                uint32_t desc_offset = (uint32_t)(desc->offset +
                                    (rgn->start_address - desc->start));
                                memory_map[memory_map_count].start  = rgn->start_address;
                                memory_map[memory_map_count].end    = rgn->end_address;
                                memory_map[memory_map_count].buffer = (uint8_t*)desc->ptr;
                                memory_map[memory_map_count].offset = desc_offset;
                                memory_map_count++;
                                found = true;
                                LOG_DEBUG("  MEM MAP[%u]: 0x%05X-0x%05X -> CORE_DESC[%u] ptr=%p+0x%X (select) (%s)",
                                          memory_map_count - 1, rgn->start_address,
                                          rgn->end_address, d, desc->ptr, desc_offset,
                                          rgn->description);
                                break;
                            }
                        }
                    }
                }
                if (!found) {
                    LOG_DEBUG("  MEM MAP: SKIP 0x%05X-0x%05X (WRAM buffer too small: need %u, have %u, no core descriptor found) (%s)",
                              rgn->start_address, rgn->end_address,
                              system_ram_used + region_size, (uint32_t)wram_size, rgn->description);
                }
            }
            system_ram_used += region_size;

        } else if (rgn->type == RC_MEMORY_TYPE_SAVE_RAM) {
            if (sram_ptr && (save_ram_used + region_size) <= sram_size) {
                memory_map[memory_map_count].start  = rgn->start_address;
                memory_map[memory_map_count].end    = rgn->end_address;
                memory_map[memory_map_count].buffer = sram_ptr;
                memory_map[memory_map_count].offset = save_ram_used;
                memory_map_count++;
                LOG_DEBUG("  MEM MAP[%u]: 0x%05X-0x%05X -> SRAM+0x%X (%s)",
                          memory_map_count - 1, rgn->start_address,
                          rgn->end_address, save_ram_used, rgn->description);
            } else {
                LOG_DEBUG("  MEM MAP: SKIP 0x%05X-0x%05X (SRAM buffer too small or null) (%s)",
                          rgn->start_address, rgn->end_address, rgn->description);
            }
            save_ram_used += region_size;

        } else if (rgn->type == RC_MEMORY_TYPE_VIRTUAL_RAM) {
            /* Echo RAM: real_address indica a que direccion real apunta.
             * Buscamos la region ya mapeada que contiene real_address. */
            uint32_t j;
            for (j = 0; j < memory_map_count; j++) {
                if (memory_map[j].start <= rgn->real_address &&
                    rgn->real_address <= memory_map[j].end) {
                    memory_map[memory_map_count].start  = rgn->start_address;
                    memory_map[memory_map_count].end    = rgn->end_address;
                    memory_map[memory_map_count].buffer = memory_map[j].buffer;
                    memory_map[memory_map_count].offset = memory_map[j].offset +
                        (rgn->real_address - memory_map[j].start);
                    memory_map_count++;
                    LOG_DEBUG("  MEM MAP[%u]: 0x%05X-0x%05X -> ECHO of 0x%05X (%s)",
                              memory_map_count - 1, rgn->start_address,
                              rgn->end_address, rgn->real_address, rgn->description);
                    break;
                }
            }
        }
        /* Ignoramos RC_MEMORY_TYPE_READONLY, HARDWARE_CONTROLLER, VIDEO_RAM, UNUSED */
    }

    LOG_DEBUG("RetroAchievements: Memory map built for console %u: %u regions mapped", console_id, memory_map_count);
}

uint32_t Achievements::read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client) {
    Achievements& self = *Achievements::instance();
    uint32_t i;

    /* Buscar la region que contiene esta direccion */
    for (i = 0; i < self.memory_map_count; i++) {
        if (address >= self.memory_map[i].start && address <= self.memory_map[i].end) {
            uint32_t local_addr = self.memory_map[i].offset + (address - self.memory_map[i].start);

            /* Verificar que no leemos mas alla del final de la region */
            if (num_bytes == 0 || num_bytes > self.memory_map[i].end - address + 1)
                return 0;

            uint8_t* base_ptr = self.memory_map[i].buffer;

            if (self.byte_swap_memory) {
                /* XOR con 1 para intercambiar bytes dentro de cada word
                 * de 16 bits (cores 68000 en hosts big-endian) */
                uint32_t j;
                for (j = 0; j < num_bytes; j++)
                    buffer[j] = base_ptr[(local_addr + j) ^ 1];
            } else {
                memcpy(buffer, base_ptr + local_addr, num_bytes);
            }

            return num_bytes;
        }
    }

    return 0;
}

void Achievements::log_message(const char* message, const rc_client_t* client) {
	LOG_DEBUG("[rcheevos] %s", message);
}

uint64_t Achievements::get_xbox_clock_millis(const rc_client_t* client) {
    return (uint64_t)SDL_GetTicks();
}

void Achievements::leaderboard_started(const rc_client_leaderboard_t* leaderboard)
{
	LOG_DEBUG("Leaderboard attempt started: %s - %s", leaderboard->title, leaderboard->description);
}

void Achievements::leaderboard_failed(const rc_client_leaderboard_t* leaderboard)
{
	LOG_DEBUG("Leaderboard attempt failed: %s", leaderboard->title);
}

void Achievements::leaderboard_submitted(const rc_client_leaderboard_t* leaderboard)
{
	LOG_DEBUG("Submitted %s for %s", leaderboard->tracker_value, leaderboard->title);
	Achievements& self = *Achievements::instance();
	AchievementState msg;
	msg.title = leaderboard->title;
	msg.description = leaderboard->tracker_value;
	// Bloqueamos el mutex de SDL para anyadir a la cola de forma segura
	self.messages.add(msg); // Anyadimos a la cola de forma "thread safe" con el add
	//Aunque el destructor de AchievementState, ahora mismo, no libera memoria, para curarnos 
	//en salud por si en un futuro esto cambia, ponemos a null los campos de la variable local msg
	msg.badge = NULL;       // Crucial para evitar que el destructor local
	msg.badgeLocked = NULL; // de 'msg' toque la memoria que ahora es del vector.
}

void Achievements::leaderboard_tracker_update(const rc_client_leaderboard_tracker_t* tracker)
{
	Achievements& self = *Achievements::instance();
	// Find the currently visible tracker by ID and update what's being displayed.
	self.trackers.update_value(tracker->id, tracker->display);
}

void Achievements::leaderboard_tracker_show(const rc_client_leaderboard_tracker_t* tracker)
{
	Achievements& self = *Achievements::instance();
	// The actual implementation of converting an rc_client_leaderboard_tracker_t to
	// an on-screen widget is going to be client-specific. The provided tracker object
	// has a unique identifier for the tracker and a string to be displayed on-screen.
	// The string should be displayed using a fixed-width font to eliminate jittering
	// when timers are updated several times a second.
	self.create_tracker(tracker->id, tracker->display);
}

void Achievements::leaderboard_tracker_hide(const rc_client_leaderboard_tracker_t* tracker)
{
	Achievements& self = *Achievements::instance();
	// This tracker is no longer needed
	self.destroy_tracker(tracker->id);
	
}

void Achievements::create_tracker(uint32_t id, const char* display) {
    LOG_DEBUG("Creando tracker: %d - %s", id, display);
    // Accedemos directamente a la referencia en el mapa. 
    // Si no existe, std::map la crea automáticamente.
    tracker_data data(id, display);
	trackers.add(id, data);
    LOG_DEBUG("Tracker creado: %d - %s", id, display);
}

void Achievements::destroy_tracker(uint32_t id) {
	trackers.erase(id);
	LOG_DEBUG("Tracker eliminado: %d", id);
}

void Achievements::challenge_indicator_show(const rc_client_achievement_t* achievement)
{
	LOG_DEBUG("Showing challenge: %d - %s", achievement->id, achievement->title);
	if (!achievement) return;

	Achievements& self = *Achievements::instance();
    // Reservamos memoria para los datos que usara el hilo
    ChallengeThreadData* data = new ChallengeThreadData();
    data->id = achievement->id;
	data->badgeName = achievement->badge_name; 
    data->badgeUrl = achievement->badge_url;

    // Lanzamos el hilo de Windows
    HANDLE hThread = CreateThread(NULL, 0, ChallengeIndicatorThread, (LPVOID)data, CREATE_SUSPENDED, NULL);
    
    if (hThread) {
		Constant::setup_and_run_thread(hThread, CPU_THREAD);
    } else {
        delete data; // Si falla el hilo, limpiamos para evitar leak
    }
}

void Achievements::challenge_indicator_hide(const rc_client_achievement_t* achievement)
{
    if (!achievement) return;
    LOG_DEBUG("challenge_indicator_hide: %u", achievement->id);
    
    Achievements* self = Achievements::instance();
    
    // Intentamos marcar el reto para ocultar y limpiar
    if (!self->challenges.set_pending_hide(achievement->id)) {
        // Si no existía (estaba descargando), creamos la entrada preventiva
        challenge_data c_data;
        c_data.id = achievement->id;
        c_data.active = false;
        c_data.pending_hide = true; // El hilo de descarga verá esto y no lo activará
        c_data.badge = NULL;
        self->challenges.add(achievement->id, c_data);
    }
}

void Achievements::progress_indicator_show(const rc_client_achievement_t* achievement)
{
  // The SHOW event tells us the indicator was not visible, but should be now.
  // To reduce duplicate code, we just update the non-visible indicator, then show it.
  progress_indicator_update(achievement);
}

void Achievements::progress_indicator_hide(void)
{
	LOG_DEBUG("progress_indicator_hide");
	Achievements* self = Achievements::instance();
	self->progress.set_active(false);
}

void Achievements::progress_indicator_update(const rc_client_achievement_t* achievement) {
    if (!achievement) return;
    Achievements* self = Achievements::instance();

    // 1. COMPROBACIÓN RÁPIDA: ¿Ya tenemos la imagen en la caché global?
    SDL_Surface* cachedBadge = NULL;
    // Asumo que tienes un método que busca en tu th_cache_image por nombre o ID
	if (self->badgeCache.find(achievement->id, cachedBadge)) {
        LOG_DEBUG("Progress update (Cache): %s", achievement->measured_progress);
        
        // Si la imagen ya está, actualizamos directamente sin hilos
        self->progress.update(achievement->id, achievement->measured_progress, cachedBadge);
        return; 
    }

    // 2. Si no está en caché, lanzamos el hilo (solo la primera vez o si falló antes)
    LOG_DEBUG("Progress update (Thread needed): %s", achievement->badge_url);
    ProgressThreadData* data = new ProgressThreadData();
    data->id = achievement->id;
    data->measured_progress = achievement->measured_progress;
    data->badgeName = achievement->badge_name;
    data->badgeUrl = achievement->badge_url;

    HANDLE hThread = CreateThread(NULL, 0, ProgressIndicatorThread, (LPVOID)data, CREATE_SUSPENDED, NULL);
    if (hThread) {
        Constant::setup_and_run_thread(hThread, CPU_THREAD);
    } else {
        delete data;
    }
}

std::string Achievements::format_total_playtime(){
	//Not yet implemented
	return "";
}

void Achievements::game_mastered(void)
{
	char message[128], submessage[128];
	Achievements* self = Achievements::instance();
	const rc_client_game_t* game = rc_client_get_game_info(self->g_client);

	snprintf(message, sizeof(message), "%s %s", 
      rc_client_get_hardcore_enabled(self->g_client) ? LanguageManager::instance()->get("menu.achievement.mastered") : LanguageManager::instance()->get("menu.achievement.completed"),
      game->title);

	snprintf(submessage, sizeof(submessage), "%s (%s)",
      rc_client_get_user_info(self->g_client)->display_name,
	  Constant::formatPlayTime(self->updatePlayTime(self->game_id)).c_str());
  
    // Creamos y rellenamos la estructura con copias de los strings
	AchievementEventData* data = new AchievementEventData();
	data->title = message;
	data->description = submessage;
	data->badgeUrl = game->badge_url;
	data->badgeName = game->badge_name;
	data->id = game->id;

	// Lanzamos el hilo
	HANDLE hThread = CreateThread(NULL, 0, AchievementTriggeredThread, (LPVOID)data, CREATE_SUSPENDED, NULL);
	if (hThread) {
		Constant::setup_and_run_thread(hThread, CPU_THREAD);
	} else {
		delete data;
	}
}

void Achievements::subset_completed(const rc_client_subset_t* subset)
{
	char message[128], submessage[128];
	Achievements* self = Achievements::instance();

	snprintf(message, sizeof(message), "%s %s", 
      rc_client_get_hardcore_enabled(self->g_client) ? LanguageManager::instance()->get("menu.achievement.mastered") : LanguageManager::instance()->get("menu.achievement.completed"),
      subset->title);

	snprintf(submessage, sizeof(submessage), "%s (%s)",
      rc_client_get_user_info(self->g_client)->display_name,
	  Constant::formatPlayTime(self->updatePlayTime(self->game_id)).c_str());
  
    // Creamos y rellenamos la estructura con copias de los strings
	AchievementEventData* data = new AchievementEventData();
	data->title = message;
	data->description = submessage;
	data->badgeUrl = subset->badge_url;
	data->badgeName = subset->badge_name;
	data->id = subset->id;

	// Lanzamos el hilo
	HANDLE hThread = CreateThread(NULL, 0, AchievementTriggeredThread, (LPVOID)data, CREATE_SUSPENDED, NULL);
	if (hThread) {
		Constant::setup_and_run_thread(hThread, CPU_THREAD);
	} else {
		delete data;
	}
}

void Achievements::server_error(const rc_client_server_error_t* error)
{
	Achievements* self = Achievements::instance();
	char buffer[128];
	// _TRUNCATE garantiza el terminador nulo \0 si el texto es muy largo
	_snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%s: %s", error->api, error->error_message);
	LOG_DEBUG(buffer);
  
	AchievementState msg;
	msg.title = error->api;
	msg.description = error->error_message;
	self->messages.add(msg); // Anyadimos a la cola de forma "thread safe" con el add
	//Aunque el destructor de AchievementState, ahora mismo, no libera memoria, para curarnos 
	//en salud por si en un futuro esto cambia, ponemos a null los campos de la variable local msg
	msg.badge = NULL;       // Crucial para evitar que el destructor local
	msg.badgeLocked = NULL; // de 'msg' toque la memoria que ahora es del vector.

}

void Achievements::achievement_update(rc_client_achievement_t* achievement){
	// Creamos y rellenamos la estructura con copias de los strings
	AchievementEventData* data = new AchievementEventData();
	data->title = achievement->title ? achievement->title : "";
	data->description = achievement->description ? achievement->description : "";
	data->badgeUrl = achievement->badge_url ? achievement->badge_url : "";
	data->badgeName = achievement->badge_name ? achievement->badge_name : "";
	data->type = achievement->id >= 100000000 ? ACH_WARNING : ACH_UNLOCKED;
	data->id = achievement->id;

	LOG_DEBUG("Trigering achievement %s", data->title.c_str());
	// Lanzamos el hilo
	HANDLE hThread = CreateThread(NULL, 0, AchievementTriggeredThread, (LPVOID)data, CREATE_SUSPENDED, NULL);
	if (hThread) {
		Constant::setup_and_run_thread(hThread, CPU_THREAD);
	} else {
		delete data;
	}
}


void Achievements::event_handler(const rc_client_event_t* event, rc_client_t* client) {
	Achievements* self = Achievements::instance();

	switch (event->type){
		case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
			achievement_update(event->achievement);
			break;								 
		case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
			Achievements::leaderboard_started(event->leaderboard);
			break;
		case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
			Achievements::leaderboard_failed(event->leaderboard);
			break;
		case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
			Achievements::leaderboard_submitted(event->leaderboard);
			break;
		case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
			leaderboard_tracker_update(event->leaderboard_tracker);
			break;
		case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
			leaderboard_tracker_show(event->leaderboard_tracker);
			break;
		case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
			leaderboard_tracker_hide(event->leaderboard_tracker);
			break;
		case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
			challenge_indicator_show(event->achievement);
			break;
		case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
			challenge_indicator_hide(event->achievement);
			break;
		case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
			progress_indicator_show(event->achievement);
			break;
		case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
			progress_indicator_update(event->achievement);
			break;
		case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE:
			progress_indicator_hide();
			break;
		case RC_CLIENT_EVENT_GAME_COMPLETED:
			game_mastered();
			break;
		case RC_CLIENT_EVENT_SUBSET_COMPLETED:
			subset_completed(event->subset);
			break;
		case RC_CLIENT_EVENT_RESET:
			self->doReset();
			break;
		case RC_CLIENT_EVENT_SERVER_ERROR:
			server_error(event->server_error);
			break;
		default:
			LOG_DEBUG("achievement event not processed: %d", event->type);
			break;
	}
}

void Achievements::send_message_game_loaded(){
	int line_height, badgeW, badgeH, badgePad;
	Achievements& self = *Achievements::instance();
	Fonts::getBadgeSize(badgeW, badgeH, badgePad, line_height);

	AchievementState msg;
	msg.type = ACH_LOAD_GAME;
	msg.gameId = self.getGameId();
	msg.title = self.getGameTitle();
	msg.scoreUnlocked = self.getSummary().points_unlocked;
	msg.scoreTotal = self.getSummary().points_core;
	msg.achvUnlocked = self.getSummary().num_unlocked_achievements;
	msg.achvTotal = self.getSummary().num_core_achievements;
	msg.badgeName = self.getGameBadge();
	msg.badgeUrl = self.getGameBadgeUrl();
	msg.timeout = 5000;
	msg.ticks = SDL_GetTicks();
	self.download_and_cache_image(&msg, badgeW, badgeH);

	#ifndef NO_DATABASE
	//Si encontramos el juego ya en bdd, actualizamos la variable
	GameState* gameStateDb = self.achievementDb->getGameInfo(self.getGameId());
	if (self.gameState == NULL){
		if (gameStateDb != NULL){
			self.gameState = gameStateDb;
		//Si no se encontro el juego, lo guardamos en bdd y actualizamos la variable
		} else if (self.achievementDb->saveGameInfo(self.getGameId(), self.getGameTitle(), self.getGameBadge(), msg.badge)){
			self.gameState = self.achievementDb->getGameInfo(self.getGameId());
		}
	} 
	//actualizamos contador de ticks
	self.lastGameTick = SDL_GetTicks();
	#endif

	self.messages.add(msg);
	msg.badge = NULL;       // Crucial para evitar que el destructor local
	msg.badgeLocked = NULL; // de 'msg' toque la memoria que ahora es del vector.
	LOG_DEBUG("send_message_game_loaded: %s", msg.title.c_str());
}

uint32_t Achievements::updatePlayTime(uint32_t game_id){
	uint32_t elapsed_game_time = 0;
	#ifndef NO_DATABASE
	if (gameState != NULL){
		elapsed_game_time = (SDL_GetTicks() - lastGameTick) / 1000 + gameState->playTime;
		achievementDb->updatePlayTime(game_id, elapsed_game_time);
		lastGameTick = SDL_GetTicks();
	} else {
		elapsed_game_time = (SDL_GetTicks() - lastGameTick) / 1000;
	}
	#endif
	return elapsed_game_time;
}

void Achievements::show_game_placard(rc_client_t* client)
{
	// 1. Obtener la instancia del Singleton
	Achievements& self = *Achievements::instance();

	// 2. Obtener info de la libreria
	const rc_client_game_t* game = rc_client_get_game_info(client);

	// 3. Guardar en los miembros de la clase
	rc_client_get_user_game_summary(client, &self.game_summary);

	if (game) {
		self.game_title = game->title;
		self.game_badge = game->badge_name;
		self.game_badge_url = game->badge_url;
		self.game_id = game->id;
	}

	LOG_DEBUG("show_game_placard: %s", game->title);

	// 4. Lgica de logs
	if (self.game_summary.num_core_achievements == 0){
		LOG_DEBUG("This game has no achievements.");
	} else {
		const int numUnlockedAch = countUserUnlocked(client);
		#ifndef NO_DATABASE
		if (numUnlockedAch == 0 && game){
			LOG_DEBUG("El usuario no ha desbloqueado logros. Reseteamos contadores");
			self.achievementDb->updatePlayTime(game->id, 0);
			self.lastGameTick = SDL_GetTicks();
		}
		#endif

		LOG_DEBUG("RA: %s - %u/%u desbloqueados.", 
			self.game_title.c_str(),
			numUnlockedAch, 
			self.game_summary.num_core_achievements);
	}
}

int Achievements::countUserUnlocked(rc_client_t* client) {
    int realCount = 0;
    rc_client_achievement_list_t* list = rc_client_create_achievement_list(client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
    
    if (list) {
        for (unsigned int i = 0; i < list->num_buckets; i++){
			for (unsigned int j = 0; j < list->buckets[i].num_achievements; j++){
				const rc_client_achievement_t* achievement = list->buckets[i].achievements[j];
				// Ignoramos el ID de aviso 101000001 y superiores de sistema
				if (achievement->id < 101000000 && 
					achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) {
					realCount++;
				}
			}
        }
        rc_client_destroy_achievement_list(list);
    }
    return realCount;
}

void Achievements::reset_menu(){
	Achievements& self = *Achievements::instance();
	BadgeDownloader::instance().stop();
	//La lista de logros debe limpiarse siempre, puesto que crean nuevas superficies
	self.achievements.clear();
}

bool Achievements::refresh_achievements_menu(){
	if (shouldRefresh){
		updateAchievements(g_client);
		shouldRefresh = false;
		return true;
	}
	return false;
}

void Achievements::updateAchievements(rc_client_t* client)
{
	// This will return a list of lists. Each top-level item is an achievement category
	// (Active Challenge, Unlocked, etc). Empty categories are not returned, so we can
	// just display everything that is returned.
	rc_client_achievement_list_t* list = rc_client_create_achievement_list(client,
		RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
		RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);

	Achievements& self = *Achievements::instance();

	// Clear any previously loaded menu items
	self.reset_menu();

	for (unsigned int i = 0; i < list->num_buckets; i++){
		// Create a header item for the achievement category
		//std::string bucketTypeLabel = LanguageManager::instance()->get("msg.achievement.bucket.type" + Constant::TipoToStr<int>(list->buckets[i].bucket_type));
		//self.achievements.push_back(AchievementState(bucketTypeLabel, list->buckets[i].bucket_type));
		//LOG_DEBUG("buckets[i]: %s", list->buckets[i].label);

		for (unsigned int j = 0; j < list->buckets[i].num_achievements; j++){
			const rc_client_achievement_t* achievement = list->buckets[i].achievements[j];
			AchievementState achState;
			// Generate a local filename to store the downloaded image.
			//snprintf(achievement_badge, sizeof(achievement_badge), "ach_%s%s.png", achievement->badge_name, 
			//         (state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ? "" : "_lock");
			//achState.badgeUrl = (achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ? achievement->badge_url : achievement->badge_locked_url;
			achState.badgeUrl = achievement->badge_url;
			// Determine the "progress" of the achievement. This can also be used to show
			// locked/unlocked icons and progress bars.
			if (list->buckets[i].bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED)
				achState.progress = LanguageManager::instance()->get("msg.achievement.unsopported");
			else if (achievement->unlocked)
				achState.progress = LanguageManager::instance()->get("msg.achievement.unlocked");
			else if (achievement->measured_percent)
				achState.progress = achievement->measured_progress;
			else
				achState.progress = LanguageManager::instance()->get("msg.achievement.locked");

			achState.description = achievement->description;
			achState.gameId = self.game_id;
			achState.id = achievement->id;
			achState.title = achievement->title;
			achState.badgeName = achievement->badge_name;
			achState.points = achievement->points;
			achState.locked = !achievement->unlocked;
			achState.sectionType = list->buckets[i].bucket_type;
			self.achievements.add(achState);
			achState.badge = NULL;       // Crucial para evitar que el destructor local
			achState.badgeLocked = NULL; // de 'msg' toque la memoria que ahora es del vector.
			LOG_DEBUG("updateAchievements: id = %d, name = %s", achState.id, achievement->title);
			/**
				//For debug purposes
				if (j==1){
				rc_client_achievement_t achievementTmp = *(rc_client_achievement_t*) achievement;
				strcpy(achievementTmp.measured_progress, "2/5");
				progress_indicator_show(&achievementTmp);
			}*/
		}
	}
	#ifndef NO_DATABASE
	self.achievementDb->updateAchievementsStatus(self.achievements);
	#endif
	rc_client_destroy_achievement_list(list);
	self.achievements.sortAchievements();
}

void Achievements::server_call(const rc_api_request_t* request,
	rc_client_server_callback_t callback, void* callback_data, rc_client_t* client) {

		// Reservamos memoria para los datos que usar el hilo
		ServerCallData* data = new ServerCallData();
		data->url = request->url ? request->url : "";
		data->post_data = request->post_data ? request->post_data : "";
		data->callback = callback;
		data->callback_data = callback_data;

		// Creamos el hilo
		HANDLE hThread = CreateThread(NULL, 0, ServerThreadFunction, (LPVOID)data, CREATE_SUSPENDED, NULL);

		if (hThread) {
			Constant::setup_and_run_thread(hThread, CPU_THREAD);
		} else {
			delete data;
		}
}

void Achievements::download_and_cache_image(AchievementState* achievement, int badgeW, int badgeH, bool createNew) {
    if (achievement->badgeName.empty() || achievement->badge != NULL) {
		achievement->isDownloading = false;
		return;
	}

    AchievementState* fromDb = NULL;
	SDL_Surface *imageObtained = NULL;
	SDL_Surface* tmpSurface = NULL;
	bool needResize = false;
	// Obtenemos el formato de la pantalla (asumiendo que es 16 bits)
	SDL_PixelFormat* targetFormat = SDL_GetVideoSurface()->format;

	if (!download_and_cache_image(achievement->badgeUrl, achievement->id, imageObtained, badgeW, badgeH)){
		LOG_ERROR("Error downloading image %s", achievement->badgeUrl.c_str());
		return;
	}

	if (createNew){
		SDL_Surface* surfaceToResize = SDL_ConvertSurface(imageObtained, targetFormat, SDL_SWSURFACE);
		//Si creamos una nueva superficie, comprobamos si necesita redimensionado
		needResize = imageObtained->w != badgeW || imageObtained->h != badgeH;
		if (needResize && surfaceToResize && imageObtained->w > 0 && imageObtained->h > 0){
			//Si necesita redimensionado
			double zoomX = (double)badgeW / imageObtained->w;
			double zoomY = (double)badgeH / imageObtained->h;
			tmpSurface = rotozoomSurfaceXY(surfaceToResize, 0, zoomX, zoomY, SMOOTHING_ON);
			SDL_FreeSurface(surfaceToResize);
		} else if (surfaceToResize){
			//No hizo falta redimensionarla, pero devolvemos la nueva que hemos creado
			tmpSurface = surfaceToResize;
		} else {
			LOG_ERROR("Error resizing image %s", achievement->badgeUrl.c_str());
		}
	} else {
		//Si no la creamos nueva, solo asignamos su referencia
		tmpSurface = imageObtained;
	}

	if (achievement->badgeLocked == NULL && achievement->locked && tmpSurface) {
		achievement->badgeLocked = SDL_ConvertSurface(tmpSurface, targetFormat, SDL_SWSURFACE);
		if (achievement->badgeLocked){
			Image::convertirGrises16Bits(achievement->badgeLocked);
		}
	}
	//Finalmente, asignamos la imagen definitiva, despues de hacer la conversion a niveles de grises
	achievement->badge = tmpSurface;
    achievement->isDownloading = false;
}

/**
*
*/
bool Achievements::download_and_cache_image(std::string url, uint32_t idImage, SDL_Surface*& image, int badgeW, int badgeH) {
    std::string response;
    float progress = 0;
	const char *c_url = url.c_str();

	// 1. CONSULTA CACHÉ (Retornamos una referencia, no una copia nueva)
	if (badgeCache.find(idImage, image)){
		return true;
	}

    if (curlClient.fetchUrl(url, response, &progress)) {
        SDL_RWops* rw = SDL_RWFromMem((void*)response.data(), (int)response.size());
        SDL_Surface *rawImg = IMG_Load_RW(rw, 1);

        if (!rawImg) {
			LOG_ERROR("Error reading image from mem: %s", c_url);
			return false;
		}
        SDL_Surface* finalSurface = NULL;
		SDL_PixelFormat* targetFormat = SDL_GetVideoSurface()->format;

        // 3. ESCALADO OPTIMIZADO
        if (rawImg->w == badgeW && rawImg->h == badgeH) {
			// En lugar de DisplayFormat, usamos ConvertSurface (Seguro en hilos)
			finalSurface = SDL_ConvertSurface(rawImg, targetFormat, SDL_SWSURFACE);
        } else if (rawImg->w > 0 && rawImg->h > 0){
            // Solo usamos rotozoom si el tamaño difiere
            double zoomX = (double)badgeW / rawImg->w;
            double zoomY = (double)badgeH / rawImg->h;
            SDL_Surface* zoomed = rotozoomSurfaceXY(rawImg, 0, zoomX, zoomY, SMOOTHING_ON);
            if (zoomed) {
                //finalSurface = SDL_DisplayFormat(zoomed);
				finalSurface = SDL_ConvertSurface(zoomed, targetFormat, SDL_SWSURFACE);
                SDL_FreeSurface(zoomed);
            }
			/*SDL_Surface* zoomed = SDL_CreateRGBSurface(SDL_SWSURFACE, badgeW, badgeH, 
                         rawImg->format->BitsPerPixel,
                         rawImg->format->Rmask, rawImg->format->Gmask, 
                         rawImg->format->Bmask, rawImg->format->Amask);
			if (zoomed) {
				SDL_Rect destRect = {0, 0, badgeW, badgeH};
				// Esta función es órdenes de magnitud más rápida que rotozoom
				SDL_SoftStretch(rawImg, NULL, zoomed, &destRect);
				finalSurface = SDL_DisplayFormat(zoomed);
				SDL_FreeSurface(zoomed);
			}*/
        }
        SDL_FreeSurface(rawImg);

        // 4. GUARDADO EN CACHÉ
        if (finalSurface != NULL) {
			badgeCache.add(idImage, finalSurface);
            image = finalSurface;
            LOG_DEBUG("Image id: %u cached: %s", idImage, c_url);
            return true;
        }
    }
    return false;
}