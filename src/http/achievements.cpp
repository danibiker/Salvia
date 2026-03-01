#include "achievements.h"

#include <cstdio>
#include <string.h>

#include <http/httputil.h> // Tu clase de CURL
#include <http/badgedownloader.h>
#include <utils/Logger.h>    // Tu sistema de log
#include <gfx/SDL_rotozoom.h>
#include <SDL_image.h>
#include <rc_consoles.h>


void Achievements::initialize() {
	if (g_client) return;

	// Creamos el cliente pasando la función de lectura de memoria y de red
	g_client = rc_client_create(read_memory, server_call);

	// Habilitar logs detallados para debug
	rc_client_enable_logging(g_client, RC_CLIENT_LOG_LEVEL_VERBOSE, log_message);

	// Provide an event handler
	rc_client_set_event_handler(g_client, event_handler);

	// Hardcore 0 para evitar baneos accidentales durante el desarrollo
	rc_client_set_hardcore_enabled(g_client, 0);

	LOG_DEBUG("RetroAchievements: Cliente inicializado.");
}

void Achievements::shutdown() {
	if (g_client) {
		rc_client_destroy(g_client);
		g_client = NULL;
		LOG_DEBUG("RetroAchievements: Cliente destruido.");
	}
}

void Achievements::login(const char* username, const char* password) {
	if (!g_client) initialize();

	LOG_DEBUG("RetroAchievements: Intentando login para %s...", username);
	rc_client_begin_login_with_password(g_client, username, password, login_callback, this);
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
		LOG_DEBUG("RetroAchievements: Login exitoso. Usuario: %s | Puntos: %u", user->display_name, user->score);
	}
}

void Achievements::server_call(const rc_api_request_t* request,
	rc_client_server_callback_t callback, void* callback_data, rc_client_t* client) {

		CurlClient curlClient;
		std::string response;
		float progress = 0;
		bool success = false;

		// Ejecutamos la petición (POST o GET según indique rcheevos)
		if (request->post_data) {
			success = curlClient.postUrl(request->url, request->post_data, response, &progress);
		} else {
			success = curlClient.fetchUrl(request->url, response, &progress);
		}

		// Preparamos la respuesta para la librería
		rc_api_server_response_t server_response;
		memset(&server_response, 0, sizeof(server_response));
		server_response.body = response.c_str();
		server_response.body_length = response.length();

		// Si curl devolvió false, informamos un error de red (0 o 500)
		server_response.http_status_code = success ? 200 : RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;

		// Enviamos la respuesta de vuelta a rc_client
		callback(&server_response, callback_data);
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

void Achievements::event_handler(const rc_client_event_t* event, rc_client_t* client) {
    Achievements& self = *Achievements::instance();

    if (event->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED) {
		self.shouldRefresh = true;
		AchievementState msg;
		msg.title = event->achievement->title;
		msg.description = event->achievement->description;   
		msg.badgeUrl = event->achievement->badge_url;
		// 1. Añadimos el mensaje a la cola de la UI (con Mutex)
        SDL_mutexP(self.messagesMutex);
        // Guardamos el mensaje en nuestra lista interna
        self.pending_messages.push_back(msg);
		SDL_mutexV(self.messagesMutex);
    }
    else if (event->type == RC_CLIENT_EVENT_SERVER_ERROR) {
		std::string error_message = "Error RA: " + std::string(event->server_error->error_message);
		LOG_DEBUG("RetroAchievements game load failed: %s", error_message.c_str());
    }
}

void Achievements::getBadgeSize(int &w, int &h, int &badgePad, int &line_height){
	const int face_h_small = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTSMALL));
	badgePad = 2;
	line_height = face_h_small + 4;
	w = line_height * 3 - badgePad * 2;
	h = line_height * 3 - badgePad * 2;
}

AchievementState Achievements::pop_message() {
    if (pending_messages.empty()) return AchievementState(); // Devuelve un objeto con valores por defecto (badge=NULL, etc.)
    
    // Obtenemos el primero, lo borramos y lo devolvemos
    AchievementState msg = pending_messages.front();
    pending_messages.erase(pending_messages.begin());
    return msg;
}

void Achievements::load_game(const uint8_t* rom, size_t rom_size)
{
	// this example is hard-coded to identify a Super Nintendo game already loaded in memory.
	// it will use the rhash library to generate a hash, then make a server call to resolve
	// the hash to a game_id. If found, it will then fetch the game data and start a session
	// for the user. By the time load_game_callback is called, the achievements for the game are
	// ready to be processed (unless an error occurs, like not being able to identify the game).
	rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_SUPER_NINTENDO, NULL, rom, rom_size, load_game_callback, NULL);
}

void Achievements::load_game_callback(int result, const char* error_message, rc_client_t* client, void* userdata)
{
	if (result != RC_OK){
		LOG_DEBUG("RetroAchievements game load failed: %s", error_message);
		return;
	}

	// announce that the game is ready
	show_game_placard(client);
	//Retrieve the achievements
	show_achievements_menu(client);
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

	// 4. Lógica de logs (la que ya tenías)
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
	for (int i=0; i < self.getAchievements().size(); i++){
		self.getAchievements().at(i).clear();
	}
	self.getAchievements().clear();
}

bool Achievements::refresh_achievements_menu(){
	if (shouldRefresh){
		show_achievements_menu(g_client);
		shouldRefresh = false;
		return true;
	}
	return false;
}

void Achievements::show_achievements_menu(rc_client_t* client)
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

	for (int i = 0; i < list->num_buckets; i++){
		// Create a header item for the achievement category
		self.achievements.push_back(AchievementState(list->buckets[i].label, true));
		LOG_DEBUG("buckets[i]: %s", list->buckets[i].label, true);

		for (int j = 0; j < list->buckets[i].num_achievements; j++){
			const rc_client_achievement_t* achievement = list->buckets[i].achievements[j];
			//async_image_data* image_data = NULL;
			char achievement_badge[64];
			AchievementState achState;
			// Generate a local filename to store the downloaded image.
			//snprintf(achievement_badge, sizeof(achievement_badge), "ach_%s%s.png", achievement->badge_name, 
			//         (state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ? "" : "_lock");

			achState.badgeUrl = (achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ? achievement->badge_url : achievement->badge_locked_url;
			// Determine the "progress" of the achievement. This can also be used to show
			// locked/unlocked icons and progress bars.
			if (list->buckets[i].bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED)
				achState.progress = "Unsupported";
			else if (achievement->unlocked)
				achState.progress = "Unlocked";
			else if (achievement->measured_percent)
				achState.progress = achievement->measured_progress;
			else
				achState.progress = "Locked";

			achState.description = achievement->description;
			achState.title = achievement->title;
			achState.points = achievement->points;
			self.achievements.push_back(achState);
		}
	}
	rc_client_destroy_achievement_list(list);
}

/**
*
*/
void Achievements::download_and_cache_image(std::string url, SDL_Surface*& image, int badgeW, int badgeH){
	CurlClient curlClient;
	std::string response;
	float progress = 0;

	if (curlClient.fetchUrl(url, response, &progress)){
		// Creamos un SDL_RWops que apunte a la memoria del string
		SDL_RWops* rw = SDL_RWFromMem((void*)response.data(), (int)response.size());
		if (rw) {
			if (image != NULL){
				SDL_FreeSurface(image);
				image = NULL;
			}
			// Cargamos la superficie (esto detecta si es PNG o JPG automáticamente)
			SDL_Surface *rawImg = IMG_Load_RW(rw, 0); // 1 = cierra el rw automáticamente

			if (badgeW == 0 || badgeH == 0){
				image = SDL_DisplayFormat(rawImg);
				SDL_FreeSurface(rawImg);
			} else {
				int line_height, badgePad;
				// Escalar
				double zoomX = (double)badgeW / rawImg->w;
				double zoomY = (double)badgeH / rawImg->h;
				SDL_Surface* zoomedSurface = rotozoomSurfaceXY(rawImg, 0, zoomX, zoomY, SMOOTHING_ON);
				SDL_FreeSurface(rawImg);
				image = SDL_DisplayFormat(zoomedSurface);
				SDL_FreeSurface(zoomedSurface);
			}
		}
	}
}