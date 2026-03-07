#include "achievements.h"

#include <cstdio>
#include <string.h>

#include <http/httputil.h>
#include <http/badgedownloader.h>
#include <utils/Logger.h>
#include <io/dirutil.h>
#include <gfx/SDL_rotozoom.h>
#include <SDL_image.h>
#include <rc_hash.h>
#include <retroachievements/CHDHashed.h>
#include <utils/langmanager.h>

void clearBadgeCache();

void Achievements::initialize() {
	if (g_client) return;

	createDbAchievements();

	active_progress.badge = NULL;

	// Creamos el cliente pasando la función de lectura de memoria y de red
	g_client = rc_client_create(read_memory, server_call);

	// Habilitar logs detallados para debug
	rc_client_enable_logging(g_client, RC_CLIENT_LOG_LEVEL_VERBOSE, log_message);

	// Provide an event handler
	rc_client_set_event_handler(g_client, event_handler);

	// Hardcore 0 para evitar baneos accidentales durante el desarrollo
	rc_client_set_hardcore_enabled(g_client, hardcoreMode ? 1 : 0);

	LOG_DEBUG("RetroAchievements: Cliente inicializado.");
}

void Achievements::shutdown() {
	if (g_client) {
		rc_client_destroy(g_client);
		g_client = NULL;
		delete achievementDb;
		clearAllData();
		LOG_DEBUG("RetroAchievements: Cliente destruido.");
	}
}

void Achievements::createDbAchievements() {
	dirutil dir;
	std::string rutadb = Constant::getAppDir() + Constant::getFileSep() + "data" + Constant::getFileSep() + "db";
	if (!dir.dirExists(rutadb.c_str())){
		dir.createDirRecursive(rutadb.c_str());
	}
	rutadb += Constant::getFileSep() + "achievements.db";
	achievementDb = new AchievementDB(rutadb.c_str());
	achievementDb->createTable();
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
	std::string response;
	float progress = 0;

	// Ejecutamos la petición
	bool success = false;
	if (!data->post_data.empty()) {
		success = curlClient.postUrl(data->url.c_str(), data->post_data.c_str(), response, &progress);
	} else {
		success = curlClient.fetchUrl(data->url.c_str(), response, &progress);
	}

	// Preparamos la respuesta para la librería
	rc_api_server_response_t server_response;
	memset(&server_response, 0, sizeof(server_response));
	server_response.body = response.c_str();
	server_response.body_length = response.length();
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
		// Bloqueamos el mutex de SDL para añadir a la cola de forma segura
		SDL_mutexP(self.messagesMutex);
		self.pending_messages.push_back(msg);
		// IMPORTANTE: Evitamos que el destructor de 'msg' borre la imagen
		// porque ahora la "propiedad" es del vector pending_messages
		msg.badge = NULL; 
		msg.badgeLocked = NULL;
		SDL_mutexV(self.messagesMutex);
	}

	delete data;
	return 0;
}

void clearBadgeCache(){
	// 1. Obtener la instancia del Singleton
	Achievements& self = *Achievements::instance();
	// Definimos el iterador para el mapa
    std::map<std::string, SDL_Surface*>::iterator it;
	LOG_DEBUG("Limpiando cache de badges");

    // Recorremos desde el inicio hasta el final
/*    for (it = self.getBadgeCache().begin(); it != self.getBadgeCache().end(); ++it) {
        // it->first es la clave (string)
        // it->second es el valor (SDL_Surface*)
        if (it->second != NULL) {
            SDL_FreeSurface(it->second);
			it->second = NULL;
        }
    }

    // ¡Muy importante! Vaciar el mapa después de liberar las superficies
    self.getBadgeCache().clear();
*/
}

DWORD WINAPI LoadGameThreadFunction(LPVOID lpParam) {
	LoadGameThreadData* data = (LoadGameThreadData*)lpParam;
	LoadContext* ctx = static_cast<LoadContext*>(data->userdata);
	Achievements& self = *Achievements::instance();

	if (ctx->romBuffer) {
		free(ctx->romBuffer); // LIBERACIÓN SEGURA: La identificación ya terminó
		ctx->romBuffer = NULL;
	}

	int procReq = rc_client_is_processing_required(data->client);
	if (procReq <= 0 && self.isHardcoreMode()){
		self.setHardcoreMode(false);
	}

	if (data->result == RC_OK) {
		self.clearAllData();
		// Llamamos a los métodos de la clase a través de la instancia
		self.show_game_placard(data->client);
		self.updateAchievements(data->client);
		if (ctx->messages) {
			self.send_message(ctx->messages);
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

    int badgeW, badgeH, badgePad, line_height;
    self.getBadgeSize(badgeW, badgeH, badgePad, line_height);

    challenge_data c_data;
    c_data.id = data->id;
    c_data.active = true;
    c_data.badge = NULL; // Inicializar por seguridad

    // Operación pesada de red/disco
    self.download_and_cache_image(data->badgeUrl, data->badgeName, c_data.badge, badgeW, badgeH);

	SDL_mutexP(self.challengeMutex);
    // Guardar en el mapa (Ojo: si tu mapa no es thread-safe, considera usar un Mutex aquí)
    self.active_challenges[data->id] = c_data;
	// Ponemos a NULL el original para que el destructor de c_data 
    // (que se ejecuta al salir de esta función) no borre la imagen del mapa.
    c_data.badge = NULL; 
	SDL_mutexV(self.challengeMutex);
    delete data; // Limpiar memoria
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

    // 2. SECCIÓN CRÍTICA RÁPIDA
    SDL_mutexP(self.progressMutex);

	// IMPORTANTE: Como el struct gestiona su propia memoria, 
    // simplemente asignamos los valores. Si ya había un badge, 
    // nuestro operador de asignación se encargará de liberarlo.
    self.active_progress.id = data->id;
    self.active_progress.measured_progress = data->measured_progress;
    
    // Si NO quieres copia profunda aquí (porque tempBadge ya es nuevo):
    if (self.active_progress.badge != NULL) SDL_FreeSurface(self.active_progress.badge);
    self.active_progress.badge = tempBadge; 
    
    self.active_progress.active = true;
    SDL_mutexV(self.progressMutex);

    delete data;
    return 0;
}

void Achievements::login(const char* username, const char* password) {
	if (!g_client) initialize();
	LOG_DEBUG("RetroAchievements: Intentando login para %s...", username);
	rc_client_begin_login_with_password(g_client, username, password, login_callback, this);
}

void Achievements::load_game(const uint8_t* rom, size_t rom_size, std::string path, uint32_t console_id, std::list<AchievementMsg>& messagesAchievement) {
	LoadContext* ctx = new LoadContext();
	ctx->messages = &messagesAchievement;
	ctx->romBuffer = (void*)rom; 
	char romHash[33] = {0};
	shouldRefresh = true;

#ifdef HAVE_CHD
	std::string hashStr;
	std::string lowPath = path;
	Constant::lowerCase(&lowPath);

	if (lowPath.find(".chd") != std::string::npos) {
		CHDHashed chdhashed;
		hashStr = chdhashed.GetHash(path, console_id);

		if (!hashStr.empty()) {
			// Copiamos el string al array char[33]
			// strncpy asegura que no nos pasemos del tamaño del buffer
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
		self->messagesMutex = SDL_CreateMutex();
		self->trackerMutex = SDL_CreateMutex();
		self->challengeMutex = SDL_CreateMutex();
		self->progressMutex = SDL_CreateMutex();
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
		// En Xbox 360 es buena práctica asignar el hilo a un hardware thread específico
		//XSetThreadProcessor(hThread, 4); 
		CloseHandle(hThread);
	} else {
		delete data; // Limpieza en caso de error
	}
}

uint32_t Achievements::read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client) {
	Achievements& self = *Achievements::instance();

	// 1. Rango WRAM (0x000000 - 0x01FFFF)
	if (address < 0x020000) {
		if (self.wram_ptr && (address + num_bytes) <= self.wram_size) {
			memcpy(buffer, self.wram_ptr + address, num_bytes);
			return num_bytes;
		}
	}
	// 2. Rango SRAM / Chips especiales (0x020000 en adelante)
	// RA mapea la SRAM justo después de la WRAM en SNES
	else if (address >= 0x020000) {
		uint32_t sram_address = address - 0x020000;
		if (self.sram_ptr && (sram_address + num_bytes) <= self.sram_size) {
			memcpy(buffer, self.sram_ptr + sram_address, num_bytes);
			return num_bytes;
		}
	}
	return 0; 
}

void Achievements::log_message(const char* message, const rc_client_t* client) {
	LOG_DEBUG("[rcheevos] %s", message);
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
	// Bloqueamos el mutex de SDL para añadir a la cola de forma segura
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
    // NOTA: Esta función debe llamarse siempre desde dentro de un bloque bloqueado por trackerMutex
    // o bloquear ella misma si devuelve una COPIA del objeto, no un puntero.
    std::map<uint32_t, tracker_data>::iterator it = active_trackers.find(id);
    if (it != active_trackers.end())
        return &(it->second);
    return NULL;
}

void Achievements::challenge_indicator_show(const rc_client_achievement_t* achievement)
{
	if (!achievement) return;

	Achievements& self = *Achievements::instance();
	LOG_DEBUG("Showing challenge: %d - %s", achievement->id, achievement->title);
    // Reservamos memoria para los datos que usará el hilo
    ChallengeThreadData* data = new ChallengeThreadData();
    data->id = achievement->id;
	data->badgeName = achievement->badge_name; 
    data->badgeUrl = achievement->badge_url;

    // Lanzamos el hilo de Windows
    HANDLE hThread = CreateThread(NULL, 0, ChallengeIndicatorThread, (LPVOID)data, 0, NULL);
    
    if (hThread) {
        CloseHandle(hThread); // No necesitamos trackearlo, se cerrará al terminar
    } else {
        delete data; // Si falla el hilo, limpiamos para evitar leak
    }
}

void Achievements::challenge_indicator_hide(const rc_client_achievement_t* achievement)
{
  // This indicator is no longer needed
	Achievements* self = Achievements::instance();
	self->active_challenges[achievement->id].active = false;
}

void Achievements::progress_indicator_update(const rc_client_achievement_t* achievement)
{
	if (!achievement) return;

	LOG_DEBUG("Updating progress: %d - %s", achievement->id, achievement->title);
    // Reservamos memoria para los datos que usará el hilo
    ProgressThreadData* data = new ProgressThreadData();
    data->id = achievement->id;
	data->measured_progress = achievement->measured_progress;
	data->badgeName = achievement->badge_name;
	data->badgeUrl = achievement->badge_url;

    // Lanzamos el hilo de Windows
    HANDLE hThread = CreateThread(NULL, 0, ProgressIndicatorThread, (LPVOID)data, 0, NULL);
    
    if (hThread) {
        CloseHandle(hThread); // No necesitamos trackearlo, se cerrará al terminar
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

	/*snprintf(submessage, sizeof(submessage), "%s (%s)",
      rc_client_get_user_info(self->g_client)->display_name,
      format_total_playtime().c_str());
	  */
	snprintf(submessage, sizeof(submessage), "%s",
      rc_client_get_user_info(self->g_client)->display_name);
  
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

	/*snprintf(submessage, sizeof(submessage), "%s (%s)",
      rc_client_get_user_info(self->g_client)->display_name,
      format_total_playtime().c_str());
	  */
	snprintf(submessage, sizeof(submessage), "%s",
      rc_client_get_user_info(self->g_client)->display_name);
  
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
	// Bloqueamos el mutex de SDL para añadir a la cola de forma segura
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

	// Lanzamos el hilo
	HANDLE hThread = CreateThread(NULL, 0, AchievementTriggeredThread, (LPVOID)data, 0, NULL);
	if (hThread) {
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
        
        // 2. ¡CRUCIAL!: "Vaciamos" el objeto que sigue en el vector
        // para que cuando hagamos .erase(), su destructor NO borre el badge.
        pending_messages.front().badge = NULL;
        pending_messages.front().badgeLocked = NULL;
        
        // 3. Lo sacamos del vector
        pending_messages.erase(pending_messages.begin());
    }
    return msg; // Retornamos 'msg', que ahora es el único dueño del badge
}

void Achievements::send_message(std::list<AchievementMsg>* messages){
	AchievementMsg msg;
	msg.title = Achievements::instance()->getGameTitle();
	msg.scoreUnlocked = Achievements::instance()->getSummary().points_unlocked;
	msg.scoreTotal = Achievements::instance()->getSummary().points_core;
	msg.achvUnlocked = Achievements::instance()->getSummary().num_unlocked_achievements;
	msg.achvTotal = Achievements::instance()->getSummary().num_core_achievements;
	msg.img = Achievements::instance()->getGameBadgeUrl();
	msg.timeout = 5000;
	msg.ticks = SDL_GetTicks();

	LOG_DEBUG("Sending message: %s", msg.title.c_str());

	int line_height, badgeW, badgeH, badgePad;
	Achievements::instance()->getBadgeSize(badgeW, badgeH, badgePad, line_height);
	Achievements::instance()->download_and_cache_image(msg.img, Achievements::instance()->getGameBadge(), msg.badge, badgeW, badgeH);
	messages->push_back(msg);
}

void Achievements::show_game_placard(rc_client_t* client)
{
	// 1. Obtener la instancia del Singleton
	Achievements& self = *Achievements::instance();

	// 2. Obtener info de la librería
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

	// 4. Lógica de logs
	if (self.game_summary.num_core_achievements == 0){
		LOG_DEBUG("This game has no achievements.");
	} else {
		LOG_DEBUG("RA: %s - %u/%u desbloqueados.", 
			self.game_title.c_str(),
			self.game_summary.num_unlocked_achievements, 
			self.game_summary.num_core_achievements);
	}
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

	for (unsigned int i = 0; i < list->num_buckets; i++){
		// Create a header item for the achievement category
		//std::string bucketTypeLabel = LanguageManager::instance()->get("msg.achievement.bucket.type" + Constant::TipoToStr<int>(list->buckets[i].bucket_type));
		//self.achievements.push_back(AchievementState(bucketTypeLabel, list->buckets[i].bucket_type));
		//LOG_DEBUG("buckets[i]: %s", list->buckets[i].label);

		for (unsigned int j = 0; j < list->buckets[i].num_achievements; j++){
			const rc_client_achievement_t* achievement = list->buckets[i].achievements[j];
			//async_image_data* image_data = NULL;
			//char achievement_badge[64];
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

			//Actializamos la base de datos
			//self.achievementDb->updateStatus(achState.id, achState.locked, achState.sectionType);
			LOG_DEBUG("updateAchievements: id = %d", achState.id);

			/**
				//For debug purposes
				if (j==1){
				rc_client_achievement_t achievementTmp = *(rc_client_achievement_t*) achievement;
				strcpy(achievementTmp.measured_progress, "2/5");
				progress_indicator_show(&achievementTmp);
			}*/
		}
	}
	self.achievementDb->updateAchievementsStatus(self.achievements);

	rc_client_destroy_achievement_list(list);
	self.sortAchievements();
	self.addSections();
}

// Función de apoyo para definir el peso de cada sección
int Achievements::getSectionPriority(uint8_t sectionType) {
	switch (sectionType) {
	case 5:  return 0; // Máxima prioridad
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
		// Si detectamos un cambio de sección o es el primer elemento del vector
		if (achievements[i].sectionType != currentSection) {
			// Insertamos la cabecera para la sección que acabamos de terminar de recorrer
			insertSectionHeader(i + 1, currentSection);

			// Actualizamos la sección actual
			currentSection = achievements[i].sectionType;
		}

		// Caso especial: Si llegamos al índice 0, siempre hay que poner la cabecera de la primera sección
		if (i == 0) {
			insertSectionHeader(0, currentSection);
		}
	}
}

// Método auxiliar para no repetir código de inserción
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

		// Reservamos memoria para los datos que usará el hilo
		ServerCallData* data = new ServerCallData();
		data->url = request->url ? request->url : "";
		data->post_data = request->post_data ? request->post_data : "";
		data->callback = callback;
		data->callback_data = callback_data;

		// Creamos el hilo
		HANDLE hThread = CreateThread(NULL, 0, ServerThreadFunction, (LPVOID)data, 0, NULL);

		if (hThread == NULL) {
			// Si falla la creación, limpiar y llamar al callback con error
			delete data;
		} else {
			// Cerramos el handle porque no necesitamos esperar por él (el hilo se limpia solo)
			CloseHandle(hThread);
		}
}

void Achievements::download_and_cache_image(AchievementState* achievement, int badgeW, int badgeH) {
    if (achievement->badgeName.empty()) return;

    AchievementState* fromDb = NULL;
    //std::map<std::string, SDL_Surface*>::iterator it = badgeCache.find(achievement->badgeName);

    // 1. INTENTAR CARGAR (Prioridad: Cache -> DB -> Descarga)
    //if (it != badgeCache.end()) {
    //    achievement->badge = it->second; 
    //} else {
        fromDb = achievementDb->getAchievement(achievement->id);
        if (fromDb && fromDb->badge) {
            achievement->badge = SDL_DisplayFormat(fromDb->badge);
            //badgeCache[achievement->badgeName] = achievement->badge;
        } else {
            // Descarga real si no existe en ningún lado
            download_and_cache_image(achievement->badgeUrl, achievement->badgeName, achievement->badge, badgeW, badgeH);
        }
    //}

    // 2. REDIMENSIONAR (Crucial hacerlo ANTES de guardar en DB para que el BLOB sea correcto)
    if (achievement->badge != NULL && (achievement->badge->w != badgeW || achievement->badge->h != badgeH)) {
        double zoomX = (double)badgeW / achievement->badge->w;
        double zoomY = (double)badgeH / achievement->badge->h;
        SDL_Surface* zoomed = rotozoomSurfaceXY(achievement->badge, 0, zoomX, zoomY, SMOOTHING_ON);
        if (zoomed) {
            SDL_FreeSurface(achievement->badge);
            achievement->badge = zoomed; // No uses DisplayFormat aquí, rotozoom ya optimiza
            // Actualizamos la cache con la versión de tamaño correcto
            //badgeCache[achievement->badgeName] = achievement->badge;
        }
    }

    // 3. GUARDAR EN DB (Solo si no venía de allí y tenemos imagen válida)
    if (fromDb == NULL && achievement->badge != NULL) {
        LOG_DEBUG("Guardando logro completo en DB: %s", achievement->badgeName.c_str());
        achievementDb->saveAchievement(*achievement);
    }

    // Limpieza
    if (fromDb) {
        fromDb->clear();
        delete fromDb;
    }
    achievement->isDownloading = false;
}

/**
*
*/
bool Achievements::download_and_cache_image(std::string url, std::string name, SDL_Surface*& image, int badgeW, int badgeH) {
    // 1. Verificar si el nombre es válido
    if (name.empty()) {
		LOG_DEBUG("download_and_cache_image. Name not valid: %s", name.c_str());
		return false;
	}

    // 2. Buscar en caché
/*    std::map<std::string, SDL_Surface*>::iterator it = badgeCache.find(name);
    if (it != badgeCache.end()) {
		if (image != NULL){
			SDL_FreeSurface(image);
			image = NULL;
		}
        image = SDL_DisplayFormat(it->second);
		LOG_DEBUG("download_and_cache_image. Image found in cache: %s", name.c_str());
        return true;
    }
*/
    // 3. Si no está en caché, descargar
    CurlClient curlClient;
    std::string response;
    float progress = 0;

    if (curlClient.fetchUrl(url, response, &progress)) {
        SDL_RWops* rw = SDL_RWFromMem((void*)response.data(), (int)response.size());
        if (!rw) {
			LOG_DEBUG("download_and_cache_image. Couldn't load image from mem: %s", url.c_str());
			return false;
		}

        // IMG_Load_RW con 1 cierra el rw automáticamente
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

        // 6. GUARDADO CRÍTICO EN CACHÉ
        if (finalSurface != NULL) {
            // Guardamos en el mapa para futuras llamadas
            //badgeCache.insert(std::make_pair(name, finalSurface));
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