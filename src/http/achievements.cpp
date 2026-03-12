#include "achievements.h"

#include <cstdio>
#include <string.h>

#include <http/httputil.h>
#include <http/badgedownloader.h>
#include <utils/Logger.h>
#include <io/dirutil.h>
#include <gfx/SDL_rotozoom.h>
#include <SDL_image.h>
#include <retroachievements/CHDHashed.h>
#include <utils/langmanager.h>
#include <rc_consoles.h>

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

	messagesMutex = SDL_CreateMutex();
	trackerMutex = SDL_CreateMutex();
	challengeMutex = SDL_CreateMutex();
	progressMutex = SDL_CreateMutex();
	achievementMutex = SDL_CreateMutex();

	createDbAchievements();
	active_progress.badge = NULL;
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
    rc_client_set_get_time_millisecs_function(g_client, get_xbox_clock_millis);
	// Forzamos el modo de lectura Little Endian para evitar que la libreria
	// reconstruya los bytes al reves en la arquitectura Big-Endian de la Xbox.
	//rc_client_set_legacy_peek(g_client, RC_CLIENT_LEGACY_PEEK_LITTLE_ENDIAN_READS);
	//rc_client_set_legacy_peek(g_client, RC_CLIENT_LEGACY_PEEK_CONSTRUCTED);
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
	pending_messages.clear();
	active_challenges.clear();
	active_trackers.clear();
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
		self.getBadgeSize(badgeW, badgeH, badgePad, line_height);
		self.download_and_cache_image(&msg, badgeW, badgeH);
		self.setShouldRefresh(true);
		// Bloqueamos el mutex de SDL para anyadir a la cola de forma segura
		SDL_mutexP(self.messagesMutex);
		self.pending_messages.push_back(msg);
		SDL_mutexV(self.messagesMutex);
	}
	delete data;
	LOG_DEBUG("AchievementTriggeredThread");
	return 0;
}

void clearBadgeCache(){
	LOG_DEBUG("Limpiando cache de badges");
	// 1. Obtener la instancia del Singleton
	Achievements& self = *Achievements::instance();
	// Definimos el iterador para el mapa
    std::map<std::string, SDL_Surface*>::iterator it;

	for (it = self.getBadgeCache().begin(); it != self.getBadgeCache().end(); ++it) {
        if (it->second != NULL) {
            SDL_FreeSurface(it->second); // Libera la superficie de la memoria
        }
    }
    // 2. Limpiar el mapa (elimina los IDs y punteros ahora invalidos)
    self.getBadgeCache().clear();
}

DWORD WINAPI LoadGameThreadFunction(LPVOID lpParam) {
	LoadGameThreadData* data = (LoadGameThreadData*)lpParam;
	LoadContext* ctx = static_cast<LoadContext*>(data->userdata);
	Achievements& self = *Achievements::instance();

	if (ctx->romBuffer) {
		free(ctx->romBuffer); // LIBERACION SEGURA: La identificacion ya termino
		ctx->romBuffer = NULL;
	}

	int procReq = rc_client_is_processing_required(data->client);
	if (procReq <= 0 && self.isHardcoreMode()){
		self.setHardcoreMode(false);
	}

	if (data->result == RC_OK) {
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
			self.send_message_game_loaded(ctx->messages);
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
	LOG_DEBUG("Downloading challenge");
    ChallengeThreadData* data = (ChallengeThreadData*)lpParam;
    Achievements& self = *Achievements::instance();

    int badgeW, badgeH, badgePad, line_height;
    self.getBadgeSize(badgeW, badgeH, badgePad, line_height);

    challenge_data c_data;
    c_data.id = data->id;
    c_data.active = true;
    c_data.badge = NULL; // Inicializar por seguridad

    // Operacion pesada de red/disco
    self.download_and_cache_image(data->badgeUrl, data->badgeName, c_data.badge, badgeW, badgeH);

	SDL_mutexP(self.challengeMutex);
    // Guardar en el mapa (Ojo: si tu mapa no es thread-safe, considera usar un Mutex aqui)
    self.active_challenges[data->id] = c_data;
	SDL_mutexV(self.challengeMutex);
    delete data; // Limpiar memoria
	LOG_DEBUG("challenge downloaded");
    return 0;
}

DWORD WINAPI ProgressIndicatorThread(LPVOID lpParam) {
    ProgressThreadData* data = (ProgressThreadData*)lpParam;
    Achievements& self = *Achievements::instance();
    
    int badgeW, badgeH, badgePad, line_height;
    self.getBadgeSize(badgeW, badgeH, badgePad, line_height);

    // 1. DESCARGA FUERA DEL MUTEX
    SDL_Surface* tempBadge = NULL;
    self.download_and_cache_image(data->badgeUrl, data->badgeName, tempBadge, badgeW, badgeH);
    SDL_mutexP(self.progressMutex);

	// IMPORTANTE: Como el struct gestiona su propia memoria, 
    // simplemente asignamos los valores. Si ya habia un badge, 
    // nuestro operador de asignacion se encargara de liberarlo.
    self.active_progress.id = data->id;
    self.active_progress.measured_progress = data->measured_progress;
    
    // Si NO quieres copia profunda aqui (porque tempBadge ya es nuevo):
    if (self.active_progress.badge != NULL) SDL_FreeSurface(self.active_progress.badge);
    self.active_progress.badge = tempBadge; 
    self.active_progress.active = true;
    SDL_mutexV(self.progressMutex);
    delete data;
	LOG_DEBUG("Progress downloaded");
    return 0;
}

void Achievements::login(const char* username, const char* password) {
	if (!g_client) initialize();
	LOG_DEBUG("RetroAchievements: Intentando login para %s...", username);
	rc_client_begin_login_with_password(g_client, username, password, login_callback, this);
}

void Achievements::load_game(const uint8_t* rom, size_t rom_size, std::string path, uint32_t console_id, std::list<AchievementState>& messagesAchievement) {
	LoadContext* ctx = new LoadContext();
	ctx->messages = &messagesAchievement;
	ctx->romBuffer = (void*)rom;
	char romHash[33] = {0};
	shouldRefresh = true;

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
	// Creamos el paquete de datos
	LoadGameThreadData* data = new LoadGameThreadData();
	data->result = result;
	data->error_message = error_message ? error_message : "";
	data->client = client;
	data->userdata = userdata;

	// Lanzamos el hilo
	HANDLE hThread = CreateThread(NULL, 0, LoadGameThreadFunction, (LPVOID)data, 0, NULL);

	if (hThread) {
		#ifdef _XBOX
		// En Xbox 360 es buena practica asignar el hilo a un hardware thread especifico
		XSetThreadProcessor(hThread, 4); 
		#endif
		CloseHandle(hThread);
	} else {
		delete data; // Limpieza en caso de error
	}
}

uint32_t Achievements::read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client) {
    Achievements& self = *Achievements::instance();
    uint8_t* base_ptr = NULL;
    uint32_t local_addr = 0;

    if (address < 0x020000) {
        if (self.wram_ptr && (address + num_bytes) <= self.wram_size) {
            base_ptr = self.wram_ptr;
            local_addr = address;
        }
    } else {
        uint32_t sram_address = address - 0x020000;
        if (self.sram_ptr && (sram_address + num_bytes) <= self.sram_size) {
            base_ptr = self.sram_ptr;
            local_addr = sram_address;
        }
    }

    if (!base_ptr) return 0;

    if (self.byte_swap_memory) {
        /* El core emula una CPU big-endian de 16 bits (68000) y el host
         * tambien es big-endian, asi que work_ram esta en orden nativo BE.
         * Los logros esperan el layout LE (byte-swapped) de PC.
         * XOR con 1 intercambia bytes dentro de cada word de 16 bits:
         *   addr_even -> lee addr+1  (LSB en vez de MSB)
         *   addr_odd  -> lee addr-1  (MSB en vez de LSB) */
        uint32_t i;
        for (i = 0; i < num_bytes; i++)
            buffer[i] = base_ptr[(local_addr + i) ^ 1];
    } else {
        memcpy(buffer, base_ptr + local_addr, num_bytes);
    }

    return num_bytes;
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
	SDL_mutexP(self.messagesMutex);
	self.pending_messages.push_back(msg);
	msg.clear();
	SDL_mutexV(self.messagesMutex);
}

void Achievements::leaderboard_tracker_update(const rc_client_leaderboard_tracker_t* tracker)
{
	Achievements& self = *Achievements::instance();
	SDL_mutexP(self.trackerMutex);
	// Find the currently visible tracker by ID and update what's being displayed.
	tracker_data* data = self.find_tracker(tracker->id);
	if (data){
		// The display text buffer is guaranteed to live for as long as the game is loaded,
		// but it may be updated in a non-thread safe manner within rc_client_do_frame, so
		// we create a copy for the rendering code to read.
		data->value = tracker->display;
	}
	SDL_mutexV(self.trackerMutex);
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
	tracker_data data;
	data.id = id;
	data.value = display;
	data.active = true;
	SDL_mutexP(trackerMutex);
	active_trackers[id] = data;
	SDL_mutexV(trackerMutex);
	LOG_DEBUG("Tracker creado: %d - %s", id, display);
}

void Achievements::destroy_tracker(uint32_t id) {
	SDL_mutexP(trackerMutex);
	active_trackers.erase(id);
	SDL_mutexV(trackerMutex);
	LOG_DEBUG("Tracker eliminado: %d", id);
}

tracker_data* Achievements::find_tracker(uint32_t id) {
    // NOTA: Esta funcion debe llamarse siempre desde dentro de un bloque bloqueado por trackerMutex
    // o bloquear ella misma si devuelve una COPIA del objeto, no un puntero.
    std::map<uint32_t, tracker_data>::iterator it = active_trackers.find(id);
    if (it != active_trackers.end())
        return &(it->second);
    return NULL;
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
    HANDLE hThread = CreateThread(NULL, 0, ChallengeIndicatorThread, (LPVOID)data, 0, NULL);
    
    if (hThread) {
		#ifdef _XBOX
		// En Xbox 360 es buena practica asignar el hilo a un hardware thread especifico
		XSetThreadProcessor(hThread, 4); 
		#endif
        CloseHandle(hThread); // No necesitamos trackearlo, se cerrara al terminar
    } else {
        delete data; // Si falla el hilo, limpiamos para evitar leak
    }
}

void Achievements::challenge_indicator_hide(const rc_client_achievement_t* achievement)
{
	LOG_DEBUG("challenge_indicator_hide");
  // This indicator is no longer needed
	Achievements* self = Achievements::instance();
	self->active_challenges[achievement->id].active = false;
}

void Achievements::progress_indicator_update(const rc_client_achievement_t* achievement)
{
	LOG_DEBUG("Updating progress: %d - %s", achievement->id, achievement->title);
	if (!achievement) return;
	// Reservamos memoria para los datos que usara el hilo
    ProgressThreadData* data = new ProgressThreadData();
    data->id = achievement->id;
	data->measured_progress = achievement->measured_progress;
	data->badgeName = achievement->badge_name;
	data->badgeUrl = achievement->badge_url;

    // Lanzamos el hilo de Windows
    HANDLE hThread = CreateThread(NULL, 0, ProgressIndicatorThread, (LPVOID)data, 0, NULL);
    
    if (hThread) {
		#ifdef _XBOX
		// En Xbox 360 es buena practica asignar el hilo a un hardware thread especifico
		XSetThreadProcessor(hThread, 4); 
		#endif
        CloseHandle(hThread); // No necesitamos trackearlo, se cerrara al terminar
    } else {
        delete data; // Si falla el hilo, limpiamos para evitar leak
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
	self->active_progress.active = false;
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
	HANDLE hThread = CreateThread(NULL, 0, AchievementTriggeredThread, (LPVOID)data, 0, NULL);
	if (hThread) {
		#ifdef _XBOX
		// En Xbox 360 es buena practica asignar el hilo a un hardware thread especifico
		XSetThreadProcessor(hThread, 4); 
		#endif
		CloseHandle(hThread);
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
	HANDLE hThread = CreateThread(NULL, 0, AchievementTriggeredThread, (LPVOID)data, 0, NULL);
	if (hThread) {
		#ifdef _XBOX
		// En Xbox 360 es buena practica asignar el hilo a un hardware thread especifico
		XSetThreadProcessor(hThread, 4); 
		#endif
		CloseHandle(hThread);
	} else {
		delete data;
	}
}

void Achievements::server_error(const rc_client_server_error_t* error)
{
	char buffer[256];
	Achievements* self = Achievements::instance();

	snprintf(buffer, sizeof(buffer), "%s: %s", error->api, error->error_message);
	LOG_DEBUG("RetroAchievements game event failed: %s", buffer);
  
	AchievementState msg;
	msg.title = error->api;
	msg.description = error->error_message;
	// Bloqueamos el mutex de SDL para anyadir a la cola de forma segura
	SDL_mutexP(self->messagesMutex);
	self->pending_messages.push_back(msg);
	msg.clear();
	SDL_mutexV(self->messagesMutex);

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
	HANDLE hThread = CreateThread(NULL, 0, AchievementTriggeredThread, (LPVOID)data, 0, NULL);
	if (hThread) {
		#ifdef _XBOX
		// En Xbox 360 es buena practica asignar el hilo a un hardware thread especifico
		XSetThreadProcessor(hThread, 4); 
		#endif
		CloseHandle(hThread);
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

AchievementState Achievements::pop_message() {
    AchievementState msg;
    if (!pending_messages.empty()) {
        // 1. Obtenemos el primero
        msg = pending_messages.front(); 
        
        // 2. CRUCIAL!: "Vaciamos" el objeto que sigue en el vector
        // para que cuando hagamos .erase(), su destructor NO borre el badge.
        pending_messages.front().badge = NULL;
        pending_messages.front().badgeLocked = NULL;
        
        // 3. Lo sacamos del vector
        pending_messages.erase(pending_messages.begin());
    }
    return msg; // Retornamos 'msg', que ahora es el unico duenyo del badge
}

void Achievements::send_message_game_loaded(std::list<AchievementState>* messages){
	int line_height, badgeW, badgeH, badgePad;
	Achievements& self = *Achievements::instance();
	self.getBadgeSize(badgeW, badgeH, badgePad, line_height);

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

	messages->push_back(msg);
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
	self.getAchievements().clear();
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

	SDL_mutexP(self.achievementMutex);
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
			self.achievements.push_back(achState);
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
	self.sortAchievements();
	self.addSections();
	SDL_mutexV(self.achievementMutex);
}

// Funcin de apoyo para definir el peso de cada seccin
int Achievements::getSectionPriority(uint8_t sectionType) {
	switch (sectionType) {
	case 5:  return 0; // Mxima prioridad
	case 2:  return 1; 
	case 1:  return 2;
	default: return 3; // El resto de tipos van al final
	}
}

void Achievements::sortAchievements() {
	// Usamos el objeto comparador en lugar de la lambda
	std::sort(achievements.begin(), achievements.end(), AchievementComparer());
}

void Achievements::addSections(){
	if (achievements.empty()) return;

	// Empezamos desde el final hacia el principio
	uint8_t currentSection = achievements.back().sectionType;

	for (int i = (int)achievements.size() - 1; i >= 0; i--) {
		// Si detectamos un cambio de seccin o es el primer elemento del vector
		if (achievements[i].sectionType != currentSection) {
			// Insertamos la cabecera para la seccin que acabamos de terminar de recorrer
			insertSectionHeader(i + 1, currentSection);

			// Actualizamos la seccin actual
			currentSection = achievements[i].sectionType;
		}

		// Caso especial: Si llegamos al ndice 0, siempre hay que poner la cabecera de la primera seccin
		if (i == 0) {
			insertSectionHeader(0, currentSection);
		}
	}
}

// Mtodo auxiliar para no repetir cdigo de insercin
void Achievements::insertSectionHeader(int index, uint8_t type) {
	std::string label = LanguageManager::instance()->get("msg.achievement.bucket.type" + Constant::TipoToStr<int>(type));

	// Creamos un objeto que represente la cabecera (isSection = true)
	AchievementState header;
	header.isSection = true;
	header.title = label;
	header.sectionType = type;

	achievements.insert(achievements.begin() + index, header);
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
		HANDLE hThread = CreateThread(NULL, 0, ServerThreadFunction, (LPVOID)data, 0, NULL);

		if (hThread == NULL) {
			// Si falla la creacin, limpiar y llamar al callback con error
			delete data;
		} else {
			#ifdef _XBOX
			// En Xbox 360 es buena prctica asignar el hilo a un hardware thread especfico
			XSetThreadProcessor(hThread, 4); 
			#endif
			// Cerramos el handle porque no necesitamos esperar por l (el hilo se limpia solo)
			CloseHandle(hThread);
		}
}

void Achievements::download_and_cache_image(AchievementState* achievement, int badgeW, int badgeH) {
    if (achievement->badgeName.empty()) return;

    AchievementState* fromDb = NULL;

	//Buscamos en base de datos
	#ifndef NO_DATABASE
    fromDb = achievementDb->getAchievement(achievement->id);
    if (fromDb && fromDb->badge && achievement->type != ACH_LOAD_GAME) {
		LOG_DEBUG("Achievement %d found in cache", achievement->id);
        achievement->badge = SDL_DisplayFormat(fromDb->badge);
    } else {
		LOG_DEBUG("Achievement %d NOT found in cache", achievement->id);
        // Descarga real si no existe en ningn lado
        download_and_cache_image(achievement->badgeUrl, achievement->badgeName, achievement->badge, badgeW, badgeH);
    }
	#else 
		download_and_cache_image(achievement->badgeUrl, achievement->badgeName, achievement->badge, badgeW, badgeH);
	#endif

    // REDIMENSIONAR
    if (achievement->badge != NULL && (achievement->badge->w != badgeW || achievement->badge->h != badgeH)) {
        double zoomX = (double)badgeW / achievement->badge->w;
        double zoomY = (double)badgeH / achievement->badge->h;
        SDL_Surface* zoomed = rotozoomSurfaceXY(achievement->badge, 0, zoomX, zoomY, SMOOTHING_ON);
        if (zoomed) {
            SDL_FreeSurface(achievement->badge);
            achievement->badge = zoomed; // No uses DisplayFormat aqu, rotozoom ya optimiza
        }
    }

	#ifndef NO_DATABASE
    //GUARDAR EN DB (Solo si no vena de all y tenemos imagen vlida)
    if (fromDb == NULL && achievement->badge != NULL && achievement->type != ACH_LOAD_GAME) {
        LOG_DEBUG("Guardando logro completo en DB: %s", achievement->badgeName.c_str());
        achievementDb->saveAchievement(*achievement);
    }

    // Limpieza
    if (fromDb) {
        fromDb->clear();
        delete fromDb;
    }
	#endif
    achievement->isDownloading = false;
}

/**
*
*/
bool Achievements::download_and_cache_image(std::string url, std::string name, SDL_Surface*& image, int badgeW, int badgeH) {
    // 1. Verificar si el nombre es vlido
    if (name.empty()) {
		LOG_DEBUG("download_and_cache_image. Name not valid: %s", name.c_str());
		return false;
	}

	if (badgeCache.find(name) != badgeCache.end()){
		LOG_DEBUG("Achievement %s found in cache", name.c_str());
		image = SDL_DisplayFormat(badgeCache[name]);
		return true;
	} 

    // 3. Si no est en cach, descargar
    CurlClient curlClient;
    std::string response;
    float progress = 0;

    if (curlClient.fetchUrl(url, response, &progress)) {
        SDL_RWops* rw = SDL_RWFromMem((void*)response.data(), (int)response.size());
        if (!rw) {
			LOG_DEBUG("download_and_cache_image. Couldn't load image from mem: %s", url.c_str());
			return false;
		}

        // IMG_Load_RW con 1 cierra el rw automticamente
        SDL_Surface *rawImg = IMG_Load_RW(rw, 1); 
        if (!rawImg){
			LOG_DEBUG("download_and_cache_image. Couldn't load RW image from mem: %s", url.c_str());
			return false;
		}

        SDL_Surface* finalSurface = NULL;

        // 4. Procesar: Escalar o convertir formato
        if (badgeW == 0 || badgeH == 0) {
            finalSurface = SDL_DisplayFormat(rawImg);
        } else {
            double zoomX = (double)badgeW / rawImg->w;
            double zoomY = (double)badgeH / rawImg->h;
            SDL_Surface* zoomed = rotozoomSurfaceXY(rawImg, 0, zoomX, zoomY, SMOOTHING_ON);
            if (zoomed) {
                finalSurface = SDL_DisplayFormat(zoomed);
                SDL_FreeSurface(zoomed);
            }
        }

        // 5. Limpieza de la imagen original
        SDL_FreeSurface(rawImg);

        // 6. GUARDADO CRTICO EN CACH
        if (finalSurface != NULL) {
            // Guardamos en el mapa para futuras llamadas
            badgeCache.insert(std::make_pair(name, finalSurface));
            LOG_DEBUG("download_and_cache_image. Inserting image in cache: %s", name.c_str());
			if (image != NULL){
				SDL_FreeSurface(image);
				image = NULL;
			}
            // Actualizamos el puntero de salida
            image = SDL_DisplayFormat(finalSurface);
        } else {
			LOG_DEBUG("download_and_cache_image. Surface error: %s", name.c_str());
		}
    }
	return true;
}

void Achievements::getBadgeSize(int &w, int &h, int &badgePad, int &line_height){
	const int face_h_small = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTSMALL));
	badgePad = 2;
	line_height = face_h_small + 4;
	w = line_height * 3 - badgePad * 2;
	h = line_height * 3 - badgePad * 2;
}