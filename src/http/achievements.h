#pragma once
#include <string>
#include <vector>
#include <rc_client.h>
#include <const/constant.h>
#include <font/fonts.h>

class Achievements {
public:
    // Acceso único a la instancia (Singleton)
    static Achievements* instance() {
        static Achievements _instance;
        return &_instance;
    }

	std::vector<AchievementState> pending_messages;
	SDL_mutex* messagesMutex;

    void initialize();
    void shutdown();
    void login(const char* username, const char* password);
	void load_game(const uint8_t* rom, std::size_t rom_size, std::string path, uint32_t console_id, std::vector<AchievementMsg>& messagesAchievement);
	void reset_menu();
	void download_and_cache_image(std::string url, SDL_Surface*& image, int badgeW, int badgeH);
	void getBadgeSize(int &w, int &h, int &badgePad, int &line_height);
	bool refresh_achievements_menu();
	AchievementState pop_message(); 
	static void show_game_placard(rc_client_t* client);
	static void show_achievements_menu(rc_client_t* client);
	static void send_message(std::vector<AchievementMsg>* messages);	

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
	std::vector<AchievementState>& getAchievements(){return achievements;}
	void setShouldRefresh(bool ind){shouldRefresh = ind;}    
	bool has_pending_messages() const { return !pending_messages.empty(); }

private:
    Achievements() : g_client(NULL), ra_score(0), shouldRefresh(false) {} // Constructor privado
    
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
	std::vector<AchievementState> achievements;
	uint8_t* wram_ptr;     // System RAM (128KB)
    std::size_t wram_size;
    uint8_t* sram_ptr;     // Save RAM / SuperFX RAM
    std::size_t sram_size;
	bool shouldRefresh;

    // Callbacks estáticos obligatorios para la librería C
    static uint32_t read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client);
    static void server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
    static void log_message(const char* message, const rc_client_t* client);
    static void login_callback(int result, const char* error_message, rc_client_t* client, void* userdata);
    static void load_game_callback(int result, const char* error_message, rc_client_t* client, void* userdata);
	static void event_handler(const rc_client_event_t* event, rc_client_t* client);
};

struct ServerCallData {
    std::string url;
    std::string post_data;
    rc_client_server_callback_t callback;
    void* callback_data;
};

struct LoadGameThreadData {
    Achievements* instance; // Guardamos el puntero 'this'
    int result;
    std::string error_message;
    rc_client_t* client;
    void* userdata;
};

struct LoadContext {
    std::vector<AchievementMsg>* messages;
    void* romBuffer;
};

struct AchievementEventData {
    Achievements* instance;
    std::string title;
    std::string description;
    std::string badgeUrl;
};