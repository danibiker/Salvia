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

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

struct tracker_data {
    uint32_t id;
    string value;
    bool active;

	tracker_data(){
		id = 0;
		active = false;
	}

	~tracker_data(){
	}
};

struct challenge_data {
    uint32_t id;
    SDL_Surface* badge; // La imagen ya cargada
    bool active;

	challenge_data(){
		id = 0;
		badge = NULL;
		active = false;
	}

	~challenge_data(){
		if (badge != NULL){
			SDL_FreeSurface(badge);
			badge = NULL; // Inicializar por seguridad
		}
	}

	// Constructor de copia
	challenge_data(const challenge_data& other) {
		id = other.id;
		active = other.active;
		badge = (other.badge != NULL) ? SDL_DisplayFormat(other.badge) : NULL;
	}

	// Operador de asignación
	challenge_data& operator=(const challenge_data& other) {
		if (this != &other) {
			if (badge != NULL) SDL_FreeSurface(badge); // Limpiar lo actual
			id = other.id;
			active = other.active;
			badge = (other.badge != NULL) ? SDL_DisplayFormat(other.badge) : NULL;
		}
		return *this;
	}
};

struct progress_data {
    uint32_t id;
    SDL_Surface* badge; 
    std::string measured_progress;
    bool active;
    
    // Constructor por defecto
    progress_data() {
        id = 0;
        badge = NULL;
        active = false;
        measured_progress = "";
    }

    // 1. CONSTRUCTOR DE COPIA (Ej: al hacer push_back en un vector)
    progress_data(const progress_data& other) {
        copiarDesde(other);
    }

    // 2. OPERADOR DE ASIGNACIÓN (Ej: al hacer data1 = data2)
    progress_data& operator=(const progress_data& other) {
        if (this != &other) { // Evitar auto-asignación
            liberar();        // Limpiar la superficie actual antes de copiar la nueva
            copiarDesde(other);
        }
        return *this;
    }

    // Destructor
    ~progress_data() {
        liberar();
    }

private:
    // Función auxiliar para liberar memoria
    void liberar() {
        if (badge != NULL) {
            SDL_FreeSurface(badge);
            badge = NULL;
        }
    }

    // Función auxiliar para realizar la clonación de datos
    void copiarDesde(const progress_data& other) {
        id = other.id;
        active = other.active;
        measured_progress = other.measured_progress;

        // CLONACIÓN DE LA SUPERFICIE (Copia profunda)
        if (other.badge != NULL) {
            // SDL_DisplayFormat crea una copia exacta optimizada para la pantalla
            badge = SDL_DisplayFormat(other.badge);
        } else {
            badge = NULL;
        }
    }
};

class Achievements {
public:
    // Acceso único a la instancia (Singleton)
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
	void download_and_cache_image(AchievementState* achievement, int badgeW, int badgeH);
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
		rc_client_reset(g_client);
	}

	bool canPause(){
		return rc_client_can_pause(g_client, NULL) > 0;
	}

	void set_memory_sources(uint8_t* wram, std::size_t w_size, uint8_t* sram, std::size_t s_size) {
        wram_ptr = wram; wram_size = w_size;
        sram_ptr = sram; sram_size = s_size;
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
	void setShouldRefresh(bool ind){shouldRefresh = ind;}    
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
    Achievements() : g_client(NULL), ra_score(0), shouldRefresh(false), hardcoreMode(true), gameState(NULL), lastGameTick(0) {} // Constructor privado
    
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
	
	
	std::vector<AchievementState> achievements;

	std::map<std::string, SDL_Surface *> badgeCache;
    // Callbacks estáticos obligatorios para la librería C
    static uint32_t read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client);
    static void server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
    static void log_message(const char* message, const rc_client_t* client);
	// 1. La función que rcheevos llamará para saber la hora
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
        // Llamamos al método estático de la clase
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