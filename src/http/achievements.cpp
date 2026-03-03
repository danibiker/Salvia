#include "achievements.h"

#include <cstdio>
#include <string.h>

#include <http/httputil.h>
#include <http/badgedownloader.h>
#include <utils/Logger.h>
#include <gfx/SDL_rotozoom.h>
#include <SDL_image.h>
#include <rc_hash.h>
#include <retroachievements/CHDHashed.h>
#include <utils/langmanager.h>

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
		reset_menu();
		LOG_DEBUG("RetroAchievements: Cliente destruido.");
	}
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
    
    if (data && data->instance) {
        AchievementState msg;
        msg.title = data->title;
        msg.description = data->description;
        msg.badgeUrl = data->badgeUrl;
		int line_height, badgeW, badgeH, badgePad;
		data->instance->getBadgeSize(badgeW, badgeH, badgePad, line_height);
		data->instance->download_and_cache_image(data->badgeUrl, msg.badge, badgeW, badgeH);
		data->instance->setShouldRefresh(true);
        // Bloqueamos el mutex de SDL para añadir a la cola de forma segura
        SDL_mutexP(data->instance->messagesMutex);
        data->instance->pending_messages.push_back(msg);
        SDL_mutexV(data->instance->messagesMutex);
    }

    delete data;
    return 0;
}

DWORD WINAPI LoadGameThreadFunction(LPVOID lpParam) {
    LoadGameThreadData* data = (LoadGameThreadData*)lpParam;
	LoadContext* ctx = static_cast<LoadContext*>(data->userdata);
	
	if (ctx->romBuffer) {
		free(ctx->romBuffer); // LIBERACIÓN SEGURA: La identificación ya terminó
		ctx->romBuffer = NULL;
	}

    if (data->result == RC_OK) {
        // Llamamos a los métodos de la clase a través de la instancia
        data->instance->show_game_placard(data->client);
        data->instance->updateAchievements(data->client);
        if (ctx->messages) {
            data->instance->send_message(ctx->messages);
        }
    } else {
		std::string errorMsg = rc_error_str(data->result);
		LOG_DEBUG("Error al cargar logros del juego: %s", errorMsg.c_str());
    }
	delete ctx; // Liberamos nuestra estructura intermedia
    delete data;
    return 0;
}

void Achievements::login(const char* username, const char* password) {
	if (!g_client) initialize();
	LOG_DEBUG("RetroAchievements: Intentando login para %s...", username);
	rc_client_begin_login_with_password(g_client, username, password, login_callback, this);
}

void Achievements::load_game(const uint8_t* rom, size_t rom_size, std::string path, uint32_t console_id, std::vector<AchievementMsg>& messagesAchievement) {
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

void Achievements::event_handler(const rc_client_event_t* event, rc_client_t* client) {
    Achievements* self = Achievements::instance();

    if (event->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED) {
		// Creamos y rellenamos la estructura con copias de los strings
        AchievementEventData* data = new AchievementEventData();
        data->instance = self;
        data->title = event->achievement->title ? event->achievement->title : "";
        data->description = event->achievement->description ? event->achievement->description : "";
        data->badgeUrl = event->achievement->badge_url ? event->achievement->badge_url : "";

        // Lanzamos el hilo
        HANDLE hThread = CreateThread(NULL, 0, AchievementTriggeredThread, (LPVOID)data, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        } else {
            delete data;
        }
    } else if (event->type == RC_CLIENT_EVENT_SERVER_ERROR) {
		std::string error_message = "Error RA: " + std::string(event->server_error->error_message);
		LOG_DEBUG("RetroAchievements game event failed: %s", error_message.c_str());
    }
}

AchievementState Achievements::pop_message() {
    if (pending_messages.empty()) return AchievementState(); // Devuelve un objeto con valores por defecto (badge=NULL, etc.)
    // Obtenemos el primero, lo borramos y lo devolvemos
    AchievementState msg = pending_messages.front();
    pending_messages.erase(pending_messages.begin());
    return msg;
}

void Achievements::send_message(std::vector<AchievementMsg>* messages){
	AchievementMsg msg;
	msg.title = Achievements::instance()->getGameTitle();
	msg.scoreUnlocked = Achievements::instance()->getSummary().points_unlocked;
	msg.scoreTotal = Achievements::instance()->getSummary().points_core;
	msg.achvUnlocked = Achievements::instance()->getSummary().num_unlocked_achievements;
	msg.achvTotal = Achievements::instance()->getSummary().num_core_achievements;
	msg.img = Achievements::instance()->getGameBadgeUrl();
	msg.timeout = 5000;
	msg.ticks = SDL_GetTicks();

	int line_height, badgeW, badgeH, badgePad;
	Achievements::instance()->getBadgeSize(badgeW, badgeH, badgePad, line_height);
	Achievements::instance()->download_and_cache_image(msg.img, msg.badge, badgeW, badgeH);
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
	for (unsigned int i=0; i < self.getAchievements().size(); i++){
		self.getAchievements().at(i).clear();
	}
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

			achState.badgeUrl = (achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ? achievement->badge_url : achievement->badge_locked_url;
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
			achState.title = achievement->title;
			achState.points = achievement->points;
			achState.locked = !achievement->unlocked;
			achState.sectionType = list->buckets[i].bucket_type;
			self.achievements.push_back(achState);
		}
	}
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

void Achievements::getBadgeSize(int &w, int &h, int &badgePad, int &line_height){
	const int face_h_small = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTSMALL));
	badgePad = 2;
	line_height = face_h_small + 4;
	w = line_height * 3 - badgePad * 2;
	h = line_height * 3 - badgePad * 2;
}