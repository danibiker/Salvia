#pragma once

#define NO_DATABASE

#include <string>
#include <vector>
#include <list>
#include <map>
#include <const/constant.h>
#include <font/fonts.h>
#include <retroachievements/achievementdb.h>
#include <rc_client.h>
#include <rc_hash.h>
#include <rc_client_internal.h>
#include <rc_consoles.h>

/* Forward declaration definido en libretro.h */
struct retro_memory_descriptor;

/* Entrada del mapa de memoria: traduce una direccion RA (address space
 * de la consola) al buffer correcto (WRAM o SRAM) + offset. */
#define MAX_MEMORY_MAPPINGS 16
struct MemoryMapping {
    uint32_t start;     // Direccion RA de inicio (inclusive)
    uint32_t end;       // Direccion RA de fin (inclusive)
    uint8_t* buffer;    // Puntero al buffer de memoria
    uint32_t offset;    // Offset dentro del buffer para 'start'
};

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

struct tracker_data {
    uint32_t id;
    string value;
    bool active;
	SDL_Surface* cache; // Puntero para la superficie renderizada
    bool dirty;         // Flag para saber si el texto cambió

	tracker_data(){
		id = 0;
		active = false;
		cache = NULL;
		dirty = true;
	}

	~tracker_data(){
		if (cache) {
            SDL_FreeSurface(cache);
            cache = NULL;
        }
	}
};

struct challenge_data {
    uint32_t id;
    SDL_Surface* badge; // La imagen ya cargada. La tenemos guardada en la cache de badges, 
						// por lo que no hace falta liberarla en el destructor
    bool active;
	SDL_Rect lastRect;
	bool pending_hide;

	challenge_data(){
		id = 0;
		badge = NULL;
		active = false;
		lastRect.x = lastRect.y = lastRect.w = lastRect.h = 0;
		pending_hide = false;
	}

	~challenge_data(){
		//No necesitamos borrar, todo queda en memoria. El badge es solo un puntero a una posicion del array badgeCache
		/*if (badge != NULL){
			SDL_FreeSurface(badge);
			badge = NULL; // Inicializar por seguridad
		}*/
	}

	// Constructor de copia
	challenge_data(const challenge_data& other) {
		id = other.id;
		active = other.active;
		badge = (other.badge != NULL) ? SDL_DisplayFormat(other.badge) : NULL;
	}

	// Operador de asignacion
	challenge_data& operator=(const challenge_data& other) {
		if (this != &other) {
			//if (badge != NULL) SDL_FreeSurface(badge); // Limpiar lo actual
			id = other.id;
			active = other.active;
			//badge = (other.badge != NULL) ? SDL_DisplayFormat(other.badge) : NULL;
			badge = other.badge;
			pending_hide = other.pending_hide;
			lastRect = other.lastRect;
		}
		return *this;
	}
};

struct progress_data {
    uint32_t id;
    SDL_Surface* badge;      // Icono (Referencia, no copia profunda)
    SDL_Surface* textCache;  // Caché del texto renderizado
    std::string measured_progress;
    bool active;
    bool dirty;              // Flag para refrescar el texto

    progress_data() : id(0), badge(NULL), textCache(NULL), active(false), dirty(true) {}

    // 1. CONSTRUCTOR DE COPIA (Optimizado: No duplica píxeles innecesariamente)
    progress_data(const progress_data& other) {
        copiarDesde(other);
    }

    // 2. OPERADOR DE ASIGNACIÓN
    progress_data& operator=(const progress_data& other) {
        if (this != &other) {
            liberar(); 
            copiarDesde(other);
        }
        return *this;
    }

    ~progress_data() {
        liberar();
    }

private:
    void liberar() {
        // IMPORTANTE: Solo liberamos lo que este objeto "posee" de forma única
        if (textCache) {
            SDL_FreeSurface(textCache);
            textCache = NULL;
        }
        // Si el 'badge' es compartido (viene de un pool), NO lo liberes aquí.
        // Si es único por cada progreso, descomenta la siguiente línea:
        // if (badge) { SDL_FreeSurface(badge); badge = NULL; }
    }

    void copiarDesde(const progress_data& other) {
        id = other.id;
        active = other.active;
        measured_progress = other.measured_progress;
        dirty = true; // Forzamos regenerar la caché de texto tras una copia
        
        // NO usamos SDL_DisplayFormat aquí. 
        // En Xbox 360 es mejor copiar el puntero del badge 
        // y gestionar su vida útil en un Singleton de Logros (Texture Pool).
        badge = other.badge; 
        textCache = NULL; // La caché de texto se regenerará en el render
    }
};

class Achievements {
public:
    // Acceso �nico a la instancia (Singleton)
    static Achievements* instance() {
        static Achievements _instance;
        return &_instance;
    }

	std::vector<AchievementState> pending_messages;
	std::map<uint32_t, tracker_data> active_trackers;
	std::map<uint32_t, challenge_data> active_challenges;
	progress_data active_progress;
	SDL_mutex* messagesMutex;
	SDL_mutex* trackerMutex;
	SDL_mutex* challengeMutex;
	SDL_mutex* progressMutex;
	SDL_mutex* achievementMutex;
	SDL_mutex* badgeCacheMutex;

	GameState *gameState;
	uint32_t lastGameTick;
	AchievementDB *achievementDb;


    void initialize();
    void shutdown();
	void clearAllData();
    void login(const char* username, const char* password);
	void load_game(const uint8_t* rom, std::size_t rom_size, std::string path, uint32_t console_id, std::list<AchievementState>& messagesAchievement);
	void reset_menu();
	bool download_and_cache_image(std::string url, std::string name, SDL_Surface*& image, int badgeW, int badgeH);
	void download_and_cache_image(AchievementState* achievement, int badgeW, int badgeH, bool createNew = false);
	uint32_t updatePlayTime(uint32_t game_id);

	void getBadgeSize(int &w, int &h, int &badgePad, int &line_height);
	bool refresh_achievements_menu();
	AchievementState pop_message(); 
	static void show_game_placard(rc_client_t* client);
	static int countUserUnlocked(rc_client_t* client);
	static void updateAchievements(rc_client_t* client);
	static void send_message_game_loaded(std::list<AchievementState>* messages);	
	static int getSectionPriority(uint8_t sectionType);

	void doUnload(){
		rc_client_unload_game(g_client);
	}

	void doFrame(){
		rc_client_do_frame(g_client);
	}

	void doIdle(){
		rc_client_idle(g_client);
	}

	void doReset(){
		//rc_client_reset(g_client);
	}

	bool canPause(){
		return rc_client_can_pause(g_client, NULL) > 0;
	}

	void set_memory_sources(uint8_t* wram, std::size_t w_size, uint8_t* sram, std::size_t s_size) {
        wram_ptr = wram; wram_size = w_size;
        sram_ptr = sram; sram_size = s_size;

        LOG_DEBUG("[DIAG] set_memory_sources: wram=%p size=%u  sram=%p size=%u",
                  (void*)wram, (unsigned)w_size, (void*)sram, (unsigned)s_size);
        /* Volcamos los primeros bytes de wram para comprobar si est viva */
        if (wram && w_size > 16) {
            LOG_DEBUG("[DIAG] wram first 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X "
                      "%02X %02X %02X %02X %02X %02X %02X %02X",
                      wram[0],wram[1],wram[2],wram[3],wram[4],wram[5],wram[6],wram[7],
                      wram[8],wram[9],wram[10],wram[11],wram[12],wram[13],wram[14],wram[15]);
        }
    }

	/* Recibe los descriptores de memoria del core (de SET_MEMORY_MAPS).
	 * Se usan como fallback en build_memory_map para regiones no cubiertas
	 * por wram_ptr/sram_ptr (ej. HRAM en Game Boy). */
	void set_core_descriptors(const struct retro_memory_descriptor* descs, unsigned count) {
		core_descriptors = descs;
		core_descriptor_count = count;
	}

    // Getters
    const std::string& getUser() const { return ra_user; }
    uint32_t getScore() const { return ra_score; }
    rc_client_t* getClient() { return g_client; }
	const std::string& getGameTitle() const { return game_title; }
	const std::string& getGameBadgeUrl() const { return game_badge_url; }
	rc_client_user_game_summary_t& getSummary(){return game_summary;}
	const std::string& getGameBadge() const{ return game_badge;}
	uint32_t getGameId(){return game_id;}


	std::vector<AchievementState>& getAchievements(){return achievements;}
	std::map<std::string, SDL_Surface *>& getBadgeCache(){return badgeCache;}
	void setShouldRefresh(bool ind){
		shouldRefresh = ind;
	}    
	bool has_pending_messages() const { return !pending_messages.empty(); }
	void setHardcoreMode(bool mode){
		hardcoreMode = mode;
		if (g_client != NULL){
			rc_client_set_hardcore_enabled(g_client, hardcoreMode ? 1 : 0);
		}
	}

	bool isHardcoreMode(){
		return hardcoreMode;
	}

private:
    Achievements() : g_client(NULL), ra_score(0), shouldRefresh(false), hardcoreMode(true), gameState(NULL), lastGameTick(0), byte_swap_memory(false), current_console_id(0), memory_map_count(0), core_descriptors(NULL), core_descriptor_count(0) {
		memset(memory_map, 0, sizeof(memory_map));
	}
    
    rc_client_t* g_client;
    std::string ra_user;
    std::string ra_token;
    uint32_t ra_score;
    // Variables para persistir los datos
    rc_client_user_game_summary_t game_summary;
    std::string game_title;
    std::string game_badge;
	std::string game_badge_url;
	uint32_t game_id;
	uint8_t* wram_ptr;     // System RAM (128KB)
    std::size_t wram_size;
    uint8_t* sram_ptr;     // Save RAM / SuperFX RAM
    std::size_t sram_size;
	bool shouldRefresh;
	bool hardcoreMode;
	bool byte_swap_memory;  // true cuando el core emula una CPU big-endian de 16 bits
	                        // (68000) en un host big-endian y necesita XOR de direcciones
	uint32_t current_console_id;
	MemoryMapping memory_map[MAX_MEMORY_MAPPINGS];
	uint32_t memory_map_count;
	const struct retro_memory_descriptor* core_descriptors;
	unsigned core_descriptor_count;
	
	
	std::vector<AchievementState> achievements;

	std::map<std::string, SDL_Surface *> badgeCache;
    // Callbacks est�ticos obligatorios para la librer�a C
    static uint32_t read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client);
    static void server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
    static void log_message(const char* message, const rc_client_t* client);
	// 1. La funci�n que rcheevos llamar� para saber la hora
	static uint64_t get_xbox_clock_millis(const rc_client_t* client);
    static void login_callback(int result, const char* error_message, rc_client_t* client, void* userdata);
    static void load_game_callback(int result, const char* error_message, rc_client_t* client, void* userdata);
	static void event_handler(const rc_client_event_t* event, rc_client_t* client);

	static void achievement_update(rc_client_achievement_t* achievement);
	static void leaderboard_started(const rc_client_leaderboard_t* leaderboard);
	static void leaderboard_failed(const rc_client_leaderboard_t* leaderboard);
	static void leaderboard_submitted(const rc_client_leaderboard_t* leaderboard);
	static void leaderboard_tracker_update(const rc_client_leaderboard_tracker_t* tracker);
	static void leaderboard_tracker_show(const rc_client_leaderboard_tracker_t* tracker);
	static void leaderboard_tracker_hide(const rc_client_leaderboard_tracker_t* tracker);
	static void challenge_indicator_hide(const rc_client_achievement_t* achievement);
	static void challenge_indicator_show(const rc_client_achievement_t* achievement);
	static void progress_indicator_update(const rc_client_achievement_t* achievement);
	static void progress_indicator_show(const rc_client_achievement_t* achievement);
	static void progress_indicator_hide(void);
	static void game_mastered(void);
	static void subset_completed(const rc_client_subset_t* subset);
	static void server_error(const rc_client_server_error_t* error);

	static std::string format_total_playtime();
	
	void sortAchievements();
	void addSections();
	void insertSectionHeader(int index, uint8_t type);
	void create_tracker(uint32_t id, const char* display);
	void destroy_tracker(uint32_t id);
	void createDbAchievements();
	
	


	tracker_data* find_tracker(uint32_t id) ;
	void build_memory_map(uint32_t console_id);
};



struct ServerCallData {
    std::string url;
    std::string post_data;
	std::string response;
    rc_client_server_callback_t callback;
    void* callback_data;
};

struct LoadGameThreadData {
    int result;
    std::string error_message;
    rc_client_t* client;
    void* userdata;
};

struct LoadContext {
    std::list<AchievementState>* messages;
    void* romBuffer;
};

struct AchievementEventData {
    std::string title;
    std::string description;
    std::string badgeUrl;
	std::string badgeName;
	uint32_t id;
	ACH_TYPE type;
};

struct ChallengeThreadData {
    uint32_t id;
    std::string badgeUrl;
	std::string badgeName;
};

struct ProgressThreadData {
    uint32_t id;
    std::string badgeUrl;
	std::string badgeName;
	std::string measured_progress;
};

// Estructura para comparar (Functor)
struct AchievementComparer {
    bool operator()(const AchievementState& a, const AchievementState& b) const {
        // Llamamos al m�todo est�tico de la clase
        int prioA = Achievements::getSectionPriority(a.sectionType);
        int prioB = Achievements::getSectionPriority(b.sectionType);

        if (prioA != prioB) {
            return prioA < prioB;
        }

        if (a.locked != b.locked) {
            return a.locked < b.locked; // Desbloqueados (false) primero
        }
        return false;
    }
};