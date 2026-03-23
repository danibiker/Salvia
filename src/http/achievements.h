#pragma once

#define NO_DATABASE

#include <string>
#include <vector>
#include <deque>
#include <list>
#include <map>
#include <const/constant.h>
#include <font/fonts.h>
#include <utils/langmanager.h>
#include <gfx/gfx_utils.h>
#include <retroachievements/achievementdb.h>
#include <http/httputil.h>
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

class ScopedLock {
    SDL_mutex* m;
public:
    // Al crear el objeto en el stack, bloqueamos
    explicit ScopedLock(SDL_mutex* mutex) : m(mutex) {
        if(m) SDL_mutexP(m);
    }
    // Al salir del scope (llave de cierre), desbloqueamos
    ~ScopedLock() {
        if(m) SDL_mutexV(m);
    }
private:
    // Prohibimos copiar el lock para evitar errores lógicos
    ScopedLock(const ScopedLock&);
    ScopedLock& operator=(const ScopedLock&);
};

// Estructura para comparar (Functor)
struct AchievementComparer {
	// Funcin de apoyo para definir el peso de cada seccin
	int getSectionPriority(uint8_t sectionType) const {
		switch (sectionType) {
		case 5:  return 0; // Mxima prioridad
		case 2:  return 1; 
		case 1:  return 2;
		default: return 3; // El resto de tipos van al final
		}
	}

    bool operator()(const AchievementState& a, const AchievementState& b) const {
        // Llamamos al metodo estatico de la clase
        int prioA = getSectionPriority(a.sectionType);
        int prioB = getSectionPriority(b.sectionType);

        if (prioA != prioB) {
            return prioA < prioB;
        }

        if (a.locked != b.locked) {
            return a.locked < b.locked; // Desbloqueados (false) primero
        }
        return false;
    }
};

struct th_messages {
private:
    std::deque<AchievementState> data;
    SDL_mutex *mutex;

    // Bloqueamos la copia de la estructura
    th_messages(const th_messages&);
    th_messages& operator=(const th_messages&);

    // Método interno que ya está bajo el mutex del addSections
    void insertSectionHeaderInternal(int index, uint8_t type) {
        // Obtenemos el texto del LanguageManager (esto es seguro si el manager es de solo lectura)
        std::string label = LanguageManager::instance()->get("msg.achievement.bucket.type" + Constant::TipoToStr<int>(type));

        AchievementState header;
        header.inicializar(); // Importante limpiar punteros badge/badgeLocked
        header.isSection = true;
        header.title = label;
        header.sectionType = type;

        // Insertar en deque es eficiente, pero invalida iteradores (por eso usamos índices 'i')
        data.insert(data.begin() + index, header);
    }

	void addSections() {
        if (data.empty()) return;

        // 1. Empezamos desde el final
        uint8_t currentSection = data.back().sectionType;

        // 2. Recorremos hacia atrás
        for (int i = (int)data.size() - 1; i >= 0; i--) {
            
            // Si hay cambio de sección
            if (data[i].sectionType != currentSection) {
                // Insertamos cabecera en la posición i+1 (donde terminaba la sección anterior)
                insertSectionHeaderInternal(i + 1, currentSection);
                
                // Actualizamos la sección que estamos evaluando ahora
                currentSection = data[i].sectionType;
            }

            // Al llegar al principio, ponemos la cabecera de la primera sección
            if (i == 0) {
                insertSectionHeaderInternal(0, currentSection);
            }
        }
    }

public:
    th_messages() {
        mutex = SDL_CreateMutex();
    }

    ~th_messages() {
        SDL_DestroyMutex(mutex);
    }

    bool empty() {
        ScopedLock lock(mutex); // Bloquea al entrar
        return data.empty();
    } // Desbloquea automáticamente al salir

    void add(const AchievementState& msg) {
        ScopedLock lock(mutex);
        data.push_back(msg);
    } // Desbloquea automáticamente

    bool pop(AchievementState& out_msg) {
        ScopedLock lock(mutex);
        if (data.empty()) {
            return false; // El lock se libera solo aquí
        }
        // Copiamos el dato al exterior
        out_msg = data.front();
        // Limpiamos los punteros del elemento original para evitar doble liberación
        data.front().badge = NULL;
        data.front().badgeLocked = NULL;
        data.pop_front();
        return true; // El lock se libera solo aquí
    }

	bool pop_with_new_surfaces(AchievementState& out_msg) {
        ScopedLock lock(mutex);
        if (data.empty()) {
            return false; // El lock se libera solo aquí
        }
        // Copiamos el dato al exterior
        out_msg = data.front();

		SDL_PixelFormat* targetFormat = SDL_GetVideoSurface()->format;
		if (data.front().badge){
			out_msg.badge = SDL_ConvertSurface(data.front().badge, targetFormat, SDL_SWSURFACE);
		}
		if (data.front().badgeLocked){
			out_msg.badgeLocked = SDL_ConvertSurface(data.front().badgeLocked, targetFormat, SDL_SWSURFACE);
		}
		
        // Limpiamos los punteros del elemento original para evitar doble liberación
        data.front().badge = NULL;
        data.front().badgeLocked = NULL;
        data.pop_front();
        return true; // El lock se libera solo aquí
    }

	std::size_t size() {
		ScopedLock lock(mutex);
		return data.size();
	}

	// Devuelve una copia para que el Gestor de Menús trabaje con sus propios datos
	AchievementState get_at(std::size_t index) {
		ScopedLock lock(mutex);
		if (index >= data.size()) {
			LOG_ERROR("Requested a non existant element");
			return AchievementState(); // O manejar error
		}
		return data[index]; 
	}

	void clear() {
		ScopedLock lock(mutex);
		// Recorremos y liberamos cada superficie manualmente
		for (std::deque<AchievementState>::iterator it = data.begin(); 
			 it != data.end(); ++it) {
			 it->clearSurfaces();
		}
		data.clear();
	}

	void sortAchievements() {
		ScopedLock lock(mutex);
		// 1. ELIMINAR CABECERAS EXISTENTES antes de reordenar
		for (std::deque<AchievementState>::iterator it = data.begin(); it != data.end(); ) {
			if (it->isSection) {
				it = data.erase(it); // Borramos la cabecera vieja
			} else {
				++it;
			}
		}
		// 2. ORDENAR los logros reales
		std::sort(data.begin(), data.end(), AchievementComparer());
		// 3. GENERAR cabeceras nuevas sobre la lista limpia y ordenada
		addSections();
	}
};

struct th_cache_image {
public:
    th_cache_image() {
        mutex = SDL_CreateMutex();
    }

    ~th_cache_image() {
        // Importante: clear() antes de destruir el mutex para que sea seguro
        clear();
        SDL_DestroyMutex(mutex);
    }

    bool find(const uint32_t id, SDL_Surface*& image) {
        ScopedLock lock(mutex);
        std::map<uint32_t, SDL_Surface*>::iterator it = badgeCache.find(id);
        
        if (it != badgeCache.end()) {
            image = it->second; 
            return true;
        }
        return false;
    }

    void add(const uint32_t id, SDL_Surface* image) {
        if (!image) return;
        ScopedLock lock(mutex);
        badgeCache[id] = image;
    }

    void clear() {
        ScopedLock lock(mutex);
        if (!SDL_WasInit(SDL_INIT_VIDEO)) {
            badgeCache.clear();
            return;
        }

        std::map<uint32_t, SDL_Surface*>::iterator it;
        for (it = badgeCache.begin(); it != badgeCache.end(); ++it) {
            if (it->second != NULL) {
                SDL_FreeSurface(it->second);
            }
        }
        badgeCache.clear();
    }

private:
    std::map<uint32_t, SDL_Surface*> badgeCache;
    SDL_mutex *mutex;

    // Deshabilitar copia para evitar desastres con el mutex
    th_cache_image(const th_cache_image&);
    th_cache_image& operator=(const th_cache_image&);
};

struct tracker_data {
    uint32_t id;
    string value;
    bool active;
    SDL_Surface* cache;
    bool dirty;

    tracker_data() : id(0), active(false), cache(NULL), dirty(true) {}
	tracker_data(uint32_t _id, const char* _val) : id(_id), value(_val), active(true), cache(NULL), dirty(true) {}

    // ¡IMPORTANTE! Quitamos el SDL_FreeSurface del destructor.
    // En C++98, si el objeto se copia mucho, el destructor borrará la imagen prematuramente.
    ~tracker_data() {}

    void free_surface() {
        if (cache && SDL_WasInit(SDL_INIT_VIDEO)) {
            SDL_FreeSurface(cache);
            cache = NULL;
        }
    }
};

struct th_tracker {
public:
    th_tracker() { mutex = SDL_CreateMutex(); }
    ~th_tracker() { 
        clear(); // Limpiamos superficies antes de morir
        SDL_DestroyMutex(mutex); 
    }

    // Usamos el ID directamente (es más rápido que buscar objetos)
    bool find(uint32_t id, tracker_data& outData) {
        ScopedLock lock(mutex);
        std::map<uint32_t, tracker_data>::iterator it = active_trackers.find(id);
        if (it != active_trackers.end()) {
            outData = it->second; 
            return true;
        }
        return false;
    }

    void add(uint32_t id, const tracker_data& data) {
        ScopedLock lock(mutex);
        // Si ya existía uno con ese ID, deberíamos liberar su superficie vieja
        if (active_trackers.count(id)) {
            active_trackers[id].free_surface();
        }
        active_trackers[id] = data;
    }

	void erase(uint32_t id){
		ScopedLock lock(mutex);
		std::map<uint32_t, tracker_data>::iterator it = active_trackers.find(id);
		if (it != active_trackers.end()) {
			it->second.free_surface(); 
            active_trackers.erase(it);
        }
	}

    void clear() {
        ScopedLock lock(mutex);
        // Recorremos el mapa para liberar CADA superficie individualmente
        std::map<uint32_t, tracker_data>::iterator it;
        for (it = active_trackers.begin(); it != active_trackers.end(); ++it) {
            it->second.free_surface();
        }
        active_trackers.clear();
    }

	 bool empty() {
        ScopedLock lock(mutex);
		return active_trackers.empty();
	 }

	void update_value(uint32_t id, const std::string& newValue) {
		ScopedLock lock(mutex);
		std::map<uint32_t, tracker_data>::iterator it = active_trackers.find(id);
    
		if (it != active_trackers.end()) {
			// Solo actualizamos si el texto ha cambiado realmente
			if (it->second.value != newValue) {
				it->second.value = newValue;
            
				// Liberamos la caché vieja para no fugar memoria
				it->second.free_surface(); 
            
				// Marcamos como sucio para que el renderizador genere la nueva imagen
				it->second.dirty = true;
			}
		}
	}

	void render(SDL_Surface* dest, TTF_Font* font, int startX, int startY) {
		ScopedLock lock(mutex); // BLOQUEO TOTAL durante el dibujo
    
		if (active_trackers.empty()) return;

		int yOffset = startY;
		std::map<uint32_t, tracker_data>::iterator it;
    
		for (it = active_trackers.begin(); it != active_trackers.end(); ++it) {
			tracker_data& data = it->second;

			// 1. GENERACIÓN DE CACHÉ (Solo si es necesario)
			if (data.dirty || !data.cache) {
				// Importante: free_surface() ya limpia el puntero y libera
				data.free_surface(); 
            
				// Usamos Blended para máxima calidad o Solid para velocidad
				data.cache = TTF_RenderUTF8_Blended(font, data.value.c_str(), white);
				data.dirty = false;
			}

			// 2. DIBUJO REAL
			if (data.cache) {
				SDL_Rect txtRect;
				txtRect.x = (Sint16)(startX - data.cache->w); // Alineado a la derecha
				txtRect.y = (Sint16)yOffset;
            
				SDL_BlitSurface(data.cache, NULL, dest, &txtRect);
				yOffset += data.cache->h + 2; // Espaciado dinámico según la fuente
			}
		}
	}

private:
    std::map<uint32_t, tracker_data> active_trackers;
    SDL_mutex *mutex;
};

struct challenge_data {
    uint32_t id;
    SDL_Surface* badge; // Puntero "prestado" de la caché global
    bool active;
    SDL_Rect lastRect;
    bool pending_hide;

    challenge_data() : id(0), badge(NULL), active(false), pending_hide(false) {
        lastRect.x = lastRect.y = lastRect.w = lastRect.h = 0;
    }

    // Constructor de copia simple (Shallow copy)
    challenge_data(const challenge_data& other) {
        id = other.id;
        active = other.active;
        badge = other.badge; // Solo copiamos la dirección de memoria
        lastRect = other.lastRect;
        pending_hide = other.pending_hide;
    }

    // Operador de asignación consistente
    challenge_data& operator=(const challenge_data& other) {
        if (this != &other) {
            id = other.id;
            active = other.active;
            badge = other.badge;
            pending_hide = other.pending_hide;
            lastRect = other.lastRect;
        }
        return *this;
    }

    ~challenge_data() {
        // No liberamos badge porque es de la caché global.
    }
};

struct th_challenge {
public:
    th_challenge() { mutex = SDL_CreateMutex(); }
    ~th_challenge() { 
        clear(); // Limpiamos antes de morir
        SDL_DestroyMutex(mutex); 
    }

    // Usamos el ID directamente (es más rápido que buscar objetos)
    bool find(uint32_t id, challenge_data& outData) {
        ScopedLock lock(mutex);
        std::map<uint32_t, challenge_data>::iterator it = active_challenges.find(id);
        if (it != active_challenges.end()) {
            outData = it->second; 
            return true;
        }
        return false;
    }

    void add(uint32_t id, const challenge_data& data) {
        ScopedLock lock(mutex);
        active_challenges[id] = data;
    }

	void erase(uint32_t id){
		ScopedLock lock(mutex);
		std::map<uint32_t, challenge_data>::iterator it = active_challenges.find(id);
		if (it != active_challenges.end()) {
            active_challenges.erase(it);
        }
	}

    void clear() {
        ScopedLock lock(mutex);
        active_challenges.clear();
    }

	 bool empty() {
        ScopedLock lock(mutex);
		return active_challenges.empty();
	 }

	void update_active(uint32_t id, const bool& newValue) {
		ScopedLock lock(mutex);
		std::map<uint32_t, challenge_data>::iterator it = active_challenges.find(id);
		if (it != active_challenges.end()) {
			// Solo actualizamos si el texto ha cambiado realmente
			if (it->second.active != newValue) {
				it->second.active = newValue;
			}
		}
	}

	bool set_pending_hide(uint32_t id) {
		ScopedLock lock(mutex);
		std::map<uint32_t, challenge_data>::iterator it = active_challenges.find(id);
		if (it != active_challenges.end()) {
			it->second.active = false;
			// Solo activamos pending_hide si ya se dibujó alguna vez (tenemos coordenadas)
			if (it->second.lastRect.w > 0) {
				it->second.pending_hide = true; 
			}
			return true;
		}
		return false;
	}

	void render(SDL_Surface* dest, uint32_t bgColor) {
		ScopedLock lock(mutex);
		if (active_challenges.empty()) return;

		// Configuración de márgenes (Esquina inferior derecha)
		int margin = 20;
		int currentX = dest->w - margin; 
		int currentY = dest->h - margin;

		std::map<uint32_t, challenge_data>::iterator it;
		for (it = active_challenges.begin(); it != active_challenges.end(); ++it) {
			challenge_data& data = it->second;

			// 1. LIMPIEZA: Si el reto ya no está activo pero tiene un rastro visual
			if (!data.active && data.pending_hide) {
				if (data.lastRect.w > 0) {
					SDL_FillRect(dest, &data.lastRect, bgColor);
				}
				data.pending_hide = false; // Limpieza completada
				data.lastRect.w = 0;        // Reset dimensiones
				continue; 
			}

			// 2. DIBUJO: Solo si está activo y tiene imagen
			if (data.active && data.badge) {
				// Calculamos la posición (de derecha a izquierda para no solapar)
				int targetX = currentX - data.badge->w;
				int targetY = currentY - data.badge->h;

				// Si la posición ha cambiado respecto al frame anterior, 
				// limpiamos la posición vieja antes de dibujar en la nueva
				if (data.lastRect.w > 0 && (data.lastRect.x != targetX || data.lastRect.y != targetY)) {
					SDL_FillRect(dest, &data.lastRect, bgColor);
				}

				SDL_Rect dstRect = { (Sint16)targetX, (Sint16)targetY, (Uint16)data.badge->w, (Uint16)data.badge->h };
				SDL_BlitSurface(data.badge, NULL, dest, &dstRect);

				// Guardamos la posición actual para la limpieza del próximo frame
				data.lastRect = dstRect;

				// Desplazamos currentX hacia la izquierda para el siguiente reto
				currentX -= (data.badge->w + 10); 
			}
		}
	}
private:
    std::map<uint32_t, challenge_data> active_challenges;
    SDL_mutex *mutex;
};

class th_progress {
private:
    uint32_t id;
    SDL_Surface* badge;      // Icono (Referencia a caché o descarga)
    SDL_Surface* textCache;  // Texto renderizado (Propiedad de esta clase)
    std::string measured_progress;
    bool active;
    bool dirty;
    SDL_mutex* mutex;
    SDL_Rect lastBgRect;

public:
    th_progress() : id(0), badge(NULL), textCache(NULL), active(false), dirty(true) {
        mutex = SDL_CreateMutex();
        lastBgRect.x = lastBgRect.y = lastBgRect.w = lastBgRect.h = 0;
    }

    ~th_progress() {
        clear_surfaces();
        SDL_DestroyMutex(mutex);
    }

    // MÉTODO DE ACTUALIZACIÓN SEGURO (Llamado desde el hilo)
    void update(uint32_t _id, const std::string& _prog, SDL_Surface* _badge) {
        ScopedLock lock(mutex);
        
        // Evitamos fugas: si ya había un badge único (no de caché), libéralo aquí
        // if (this->badge && this->badge != _badge) SDL_FreeSurface(this->badge);

        this->id = _id;
        this->measured_progress = _prog;
        this->badge = _badge;
        this->active = true;
        this->dirty = true; // Forzamos regenerar el texto en el próximo render
    }

    void set_active(bool state) {
        ScopedLock lock(mutex);
        this->active = state;
    }

    void clear_surfaces() {
        ScopedLock lock(mutex);
        if (textCache) SDL_FreeSurface(textCache);
        textCache = NULL;
        // Si el badge es propiedad exclusiva, libéralo también aquí
        active = false;
    }

    // EL MÉTODO DE RENDERIZADO DENTRO DE LA CLASE
    void render(SDL_Surface* dest, uint32_t uBkgColor, const SDL_Color alphaColor) {
		ScopedLock lock(mutex);

		// 1. LIMPIEZA SI SE DESACTIVÓ
		if (!active) {
			if (lastBgRect.w > 0) {
				SDL_FillRect(dest, &lastBgRect, uBkgColor);
				lastBgRect.w = 0;
			}
			return;
		}

		// 2. RE-GENERAR CACHÉ DE TEXTO (Solo si dirty)
		if (dirty || !textCache) {
			if (textCache) SDL_FreeSurface(textCache);
			textCache = TTF_RenderUTF8_Blended(Fonts::getFont(Fonts::FONTSMALL), measured_progress.c_str(), white);
			dirty = false;
		}

		if (!textCache) return;

		int badgeW, badgeH, badgePad, line_height;
		Fonts::getBadgeSize(badgeW, badgeH, badgePad, line_height);		
		SDL_Rect newBgRect;
		
		const int margin = 4;
		newBgRect.y = dest->h - (badgeH + 20) * 2;
		newBgRect.w = badgeW + 4*margin + textCache->w;
		newBgRect.x = dest->w - newBgRect.w - margin;
		newBgRect.h = badgeH + 2*margin;

		// 4. LIMPIEZA DEL RASTRO ANTERIOR (Solo si cambió de sitio o tamaño)
		if (lastBgRect.w > 0 && (lastBgRect.x != newBgRect.x || lastBgRect.w != newBgRect.w)) {
			SDL_FillRect(dest, &lastBgRect, uBkgColor);
		}
		lastBgRect = newBgRect;

		// 5. DIBUJO DE CAPAS
		// A. Fondo con transparencia
		DrawRectAlpha(dest, newBgRect, alphaColor, 180);

		// B. Icono (Badge) - Centrado verticalmente en el contenedor
		if (badge) {
			SDL_Rect rectIcon;
			rectIcon.x = newBgRect.x + margin;
			rectIcon.y = newBgRect.y + margin;
			rectIcon.w = badgeW;
			rectIcon.h = badgeH;
			SDL_BlitSurface(badge, NULL, dest, &rectIcon);
		}

		// C. Texto - A la derecha del icono y centrado verticalmente
		SDL_Rect rectTxt;
		rectTxt.x = newBgRect.x + 2 * margin + badgeW;
		rectTxt.y = newBgRect.y + margin + (badgeH / 2) - (textCache->h / 2);
		SDL_BlitSurface(textCache, NULL, dest, &rectTxt);
	}
};


class Achievements {
public:		
    // Acceso unico a la instancia (Singleton)
    static Achievements* instance() {
        static Achievements _instance;
        return &_instance;
    }

	th_cache_image badgeCache;
	th_messages messages;
	th_messages achievements;
	th_challenge challenges;
	th_tracker trackers;
	th_progress progress;
	SDL_mutex *progressMutex;

	GameState *gameState;
	uint32_t lastGameTick;
	AchievementDB *achievementDb;

    void initialize();
    void shutdown();
	void clearAllData();
    void login(const char* username, const char* password);
	void load_game(const uint8_t* rom, std::size_t rom_size, std::string path, uint32_t console_id, th_messages& messagesAchievement);
	void reset_menu();
	bool download_and_cache_image(std::string url, uint32_t id, SDL_Surface*& image, int badgeW, int badgeH);
	void download_and_cache_image(AchievementState* achievement, int badgeW, int badgeH, bool createNew = false);
	uint32_t updatePlayTime(uint32_t game_id);

	bool refresh_achievements_menu();
	static void show_game_placard(rc_client_t* client);
	static int countUserUnlocked(rc_client_t* client);
	static void updateAchievements(rc_client_t* client);
	static void send_message_game_loaded();	

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

	void setShouldRefresh(bool ind){
		shouldRefresh = ind;
	}    
	
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
	volatile bool shouldRefresh;
	bool hardcoreMode;
	bool byte_swap_memory;  // true cuando el core emula una CPU big-endian de 16 bits
	                        // (68000) en un host big-endian y necesita XOR de direcciones

	uint32_t current_console_id;
	MemoryMapping memory_map[MAX_MEMORY_MAPPINGS];
	uint32_t memory_map_count;
	const struct retro_memory_descriptor* core_descriptors;
	unsigned core_descriptor_count;
    CurlClient curlClient;	
	
    
	// Callbacks estaticos obligatorios para la libreria C
    static uint32_t read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client);
    static void server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
    static void log_message(const char* message, const rc_client_t* client);
	// 1. La funcion que rcheevos llamara para saber la hora
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
	void insertSectionHeader(int index, uint8_t type);
	void create_tracker(uint32_t id, const char* display);
	void destroy_tracker(uint32_t id);
	void createDbAchievements();
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
    th_messages* messages;
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