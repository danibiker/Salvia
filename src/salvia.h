//Evita errores al usar el min o max de windows.h al incluir el filtro "io/xbrz/xbrz.h"
#define NOMINMAX 

#include <SDL.h>
#include <SDL_ttf.h>
#include "SDL_thread.h"

#include <string>
#include <map>
#include <algorithm>
#include <zlib.h>

#include "gameMenu.h"
#include "io/cfgloader.h"
#include "io/dirutil.h"
#include <io/progress_bar.h>
#include "uiobjects/listmenu.h"
#include "uiobjects/tilemap.h"
#include "unzip/unziptool.h"
#include "const/menuconst.h"
#include "statesram.h"
#include "io/inputsmenu.h"
#include "io/inputscore.h"
#include "image/icons.h"
#include "utils/langmanager.h"
#include "so/launcher.h"
#include "dischelper.h"
#include <so/soutils.h>


CfgLoader *cfgLoader;
GameMenu *gameMenu;
ListMenu *listMenu;
Logger *logger;
dirutil dir;

#ifdef __cplusplus
extern "C" {
#endif
	#include "libretro/libretro.h"
	#include "libretro/vfs.h"

    void retro_init(void);
    void retro_deinit(void);
    void retro_run(void);
    void retro_get_system_info(struct retro_system_info *info);
    void retro_get_system_av_info(struct retro_system_av_info *info);
    void retro_set_environment(retro_environment_t);
    void retro_set_video_refresh(retro_video_refresh_t);
    void retro_set_audio_sample(retro_audio_sample_t);
    void retro_set_audio_sample_batch(retro_audio_sample_batch_t);
    void retro_set_input_poll(retro_input_poll_t);
    void retro_set_input_state(retro_input_state_t);
    bool retro_load_game(const struct retro_game_info *game);
	void retro_unload_game(void);
	void retro_set_controller_port_device(unsigned port, unsigned device);
	
#ifdef __cplusplus
}
#endif

/* ---------- Memory map descriptors from the core ----------
 * Capturados cuando el core llama RETRO_ENVIRONMENT_SET_MEMORY_MAPS.
 * Se usan para obtener punteros a regiones de memoria que no estan
 * disponibles via retro_get_memory_data (ej. HRAM en Game Boy). */
#define MAX_LIBRETRO_MEM_DESCRIPTORS 32
static struct retro_memory_descriptor g_mem_descriptors[MAX_LIBRETRO_MEM_DESCRIPTORS];
static unsigned g_num_mem_descriptors = 0;



/* Funciones de acceso para que otros modulos puedan consultar los descriptores */
const struct retro_memory_descriptor* get_core_memory_descriptors(unsigned* out_count) {
    if (out_count) *out_count = g_num_mem_descriptors;
    return g_mem_descriptors;
}

// 1. Usa un buffer persistente para evitar allocs constantes al convertir desde ARGB8888
enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
void closeGame();
void init_sdl_audio(double sample_rate);

//Maximo de 30 MB. Los CHD que son grandes no debemos cargarlos en memoria. Ya se encarga
//la implementacion de vfs
const int MAX_FILE_LOAD_MEMORY = 1024 * 2014 * 30; 
const int maxJoyTargets = RETRO_DEVICE_ID_JOYPAD_R3 + 1;
const std::string Constant::MAME_SYS_ID = "75";
const std::string Constant::WHITESPACE = " \n\r\t";
volatile uint32_t Constant::totalTicks = 0;
int Constant::EXEC_METHOD = launch_batch;
const std::string CfgLoader::CONFIGFILE = "salvia.cfg";
static uint16_t* conversion_buffer = NULL;
static std::size_t buffer_size = 0;

// Rotacion de pantalla solicitada por el core via RETRO_ENVIRONMENT_SET_ROTATION.
// Valores 0..3 = 0/90/180/270 grados en sentido antihorario (CCW),
// segun la convencion libretro. Cores como FBNeo lo emiten automaticamente
// para juegos verticales (Cave, agallet, ddonpach, etc.).
static unsigned g_screen_rotation = 0;
static uint16_t* rotation_buffer = NULL;
static std::size_t rotation_buffer_size = 0;

int audio_opened = 0;
// En tu clase/global:
volatile bool audio_closing = false;
t_rom_paths romPaths;
t_scale_props current_video_settings;

// Current ROM path (needed to persist last disc index on closeGame)
static std::string g_currentRompath;

//Indica si el core puede arrancar sin disco introducido
bool g_currentCoreSupportsNoGame;

double nextFrameTime;

/* [Xbox 360] Tasa actualmente abierta del dispositivo SDL audio.  Permite
 * a launchGame detectar cambios de sample_rate entre cargas y, si la macro
 * RESET_AUDIO esta definida, reabrir el device a la nueva tasa.  Sin
 * RESET_AUDIO se mantiene el comportamiento por defecto (no reabrir nunca,
 * porque en 360 SDL_OpenAudio/CloseAudio repetidos colgaban). */
static int g_audio_opened_rate = 0;

/* [XBOX360] Contexto pasado al watcher thread.
 *
 * Las surfaces SDL/TTF se pre-rendereeron en el MAIN thread antes de
 * lanzar el watcher.  Esto elimina las llamadas no-thread-safe a
 * TTF_RenderUTF8_Blended y SDL_CreateRGBSurface desde el watcher.
 *
 * El watcher solo hace operaciones "seguras" sobre surfaces ajenas:
 * SDL_FillRect, SDL_BlitSurface, SDL_Flip.  Estas operan sobre buffers
 * de pixels concretos sin tocar estado global de SDL/TTF.  Combinado
 * con D3DCREATE_MULTITHREADED en el device, SDL_Flip queda thread-safe.
 *
 * `exitRequested` permite al main thread senalizar fin antes de que
 * el watcher salga por su propio bucle (no se usa actualmente porque
 * el watcher se autoextingue tras watchPeriod, pero queda preparado). */
struct LoadingWatcherCtx {
	double*       nextFrameTime;
	SDL_Surface** preRenderedText;
	int           numColors;
	SDL_Surface*  rawSurface;
	Uint32        keyBg;
	SDL_Rect      drawRect;
	volatile LONG exitRequested;
};

static void unescape_newlines(std::string& str) {
    std::string::size_type pos = 0;
    // Busca "\\n" (representado en código como "\\\\n")
    while ((pos = str.find("\\n", pos)) != std::string::npos) {
        str.replace(pos, 2, "\n"); // Reemplaza 2 caracteres por 1 salto de línea
        ++pos; // Avanza para evitar bucles infinitos
    }
}

DWORD WINAPI th_printLoading(LPVOID data) {
	LoadingWatcherCtx* ctx = (LoadingWatcherCtx*)data;
	uint32_t cycles = 0;
	uint8_t  colors = 0;

	const uint16_t updateCycle = 10;
	const uint16_t updateDelay = (uint16_t)(1000.0f / updateCycle);
	const uint16_t watchPeriod = 10 * updateCycle; //10 segundos de comprobacion
	bool salir = false;
	bool hangDetected = false;

	while (!salir){
		if (InterlockedExchangeAdd(&ctx->exitRequested, 0) != 0) break;
		cycles++;
		//if (cycles > watchPeriod){
			if (*ctx->nextFrameTime + 1000.0 < Constant::getTicks()){
				/* Hang detectado.  Rotamos color cada segundo para
				 * dar feedback visual. */
				if (cycles % updateCycle == 0) {
					colors = (colors + 1) % ctx->numColors;
				}
				SDL_Surface* line = ctx->preRenderedText[colors];

				SDL_FillRect(ctx->rawSurface, nullptr, ctx->keyBg);
				SDL_BlitSurface(line, nullptr, ctx->rawSurface, nullptr);
				SDL_SetAlpha(ctx->rawSurface, 0, 0);
				if (gameMenu->overlay){
					if (!hangDetected){
						gameMenu->clearOverlay();
					}
					SDL_BlitSurface(ctx->rawSurface, nullptr, gameMenu->overlay, &ctx->drawRect);
				}
				//Procesamos las hotkeys
				gameMenu->joystick->pollKeys(gameMenu->overlay);
				HOTKEYS_LIST hotkey = gameMenu->joystick->findHotkey();
				if (hotkey == HK_EXIT_GAME){
					LOG_ERROR("Requested exit");
				}

				hangDetected = true;
				SDL_Flip(gameMenu->gameScreen);
			} else if (hangDetected){
				//Se ha restablecido el sistema. Salimos del loop
				salir = true;
			} else if (cycles > watchPeriod && !hangDetected){
			 //else {
				//El sistema funcionaba bien
				salir = true;
			}
		//}
		SDL_Delay(updateDelay); // 10fps = 1000/10
	}

	if (gameMenu->overlay)
		gameMenu->clearOverlayRect(ctx->drawRect);

	return 0;
}

void watchForLoadingStuck(){
	/* [XBOX360] Pre-render de surfaces para el watcher de carga (lazy init
	 * una sola vez por sesion).  TTF_RenderUTF8_Blended y SDL_CreateRGBSurface
	 * no son thread-safe respecto a otras llamadas SDL/TTF concurrentes, asi
	 * que las hacemos AQUI en el main thread.  El watcher solo hara
	 * SDL_FillRect/Blit/Flip sobre estas surfaces ya creadas. */
	static SDL_Color s_watcherColors[] = {white, yellow, blue, red};
	static const int  s_watcherNumColors = sizeof(s_watcherColors) / sizeof(s_watcherColors[0]);
	static SDL_Surface* s_watcherText[s_watcherNumColors] = {0};
	static SDL_Surface* s_watcherRaw = nullptr;
	static SDL_Rect     s_watcherRect = {0};
	static Uint32       s_watcherKeyBg = 0;
	static LoadingWatcherCtx s_watcherCtx = {0};

	if (s_watcherRaw == nullptr) {
		const char* msg = "waiting for the game to load";
		for (int i = 0; i < s_watcherNumColors; i++) {
			s_watcherText[i] = TTF_RenderUTF8_Blended(
				Fonts::getFont(Fonts::FONTBIG), msg, s_watcherColors[i]);
		}
		if (s_watcherText[0] != nullptr) {
			s_watcherRaw = SDL_CreateRGBSurface(SDL_SWSURFACE,
				s_watcherText[0]->w, s_watcherText[0]->h,
				gameMenu->overlay->format->BitsPerPixel,
				gameMenu->overlay->format->Rmask,
				gameMenu->overlay->format->Gmask,
				gameMenu->overlay->format->Bmask,
				gameMenu->overlay->format->Amask);
			if (s_watcherRaw != nullptr) {
				const Uint8 KEY_ALPHA = 180;
				s_watcherKeyBg = SDL_MapRGBA(s_watcherRaw->format,
					Constant::colors[clBackground].sdlColor.r,
					Constant::colors[clBackground].sdlColor.g,
					Constant::colors[clBackground].sdlColor.b,
					KEY_ALPHA);
				s_watcherRect.x = 20;
				s_watcherRect.y = 20;
				s_watcherRect.w = s_watcherText[0]->w;
				s_watcherRect.h = s_watcherText[0]->h;
			}
		}
	}

	/* Solo lanzar el watcher si las surfaces se inicializaron correctamente. */
	if (s_watcherRaw != nullptr) {
		s_watcherCtx.nextFrameTime   = &nextFrameTime;
		s_watcherCtx.preRenderedText = s_watcherText;
		s_watcherCtx.numColors       = s_watcherNumColors;
		s_watcherCtx.rawSurface      = s_watcherRaw;
		s_watcherCtx.keyBg           = s_watcherKeyBg;
		s_watcherCtx.drawRect        = s_watcherRect;
		InterlockedExchange(&s_watcherCtx.exitRequested, 0);

		HANDLE hThread = CreateThread(NULL, 0, th_printLoading, &s_watcherCtx, CREATE_SUSPENDED, NULL);
		nextFrameTime = Constant::getTicks();
		Constant::setup_and_run_thread(hThread, IO_THREAD, true);
	}
}


retro_audio_buffer_status_callback_t audio_status_cb;
// 1. Declara una variable global o estática para guardar el callback del core
retro_keyboard_event_t core_key_callback = nullptr;

struct retro_core_variable {
   const char *key;    // Nombre técnico: "nestopia_region"
   const char *value;  // Nombre visual y opciones: "Region; Auto|NTSC|PAL"
};

void drawLoadingProgressBar(SDL_Surface* screen, float progress);
struct t_progress_load{
	float loading_progress;
	int total_rom_files;
	int current_rom_file;

	t_progress_load(){
		reset();
	}

	void reset(){
		loading_progress = 0.0f;
		total_rom_files = 10; // Valor estimado o calculado abriendo el zip
		current_rom_file = 0;
	}

} progress_loader;



void retro_log_printf(enum retro_log_level level, const char *fmt, ...) {
    #ifndef DEBUG_LOG
    if (level != RETRO_LOG_ERROR && gameMenu->romLoaded) {
        return;
    }
    #endif

	const unsigned int MAX_BUFFER = 512;
	char buffer[MAX_BUFFER] = {0}; 
    va_list args;
    va_start(args, fmt);
    int len = _vsnprintf_s(buffer, MAX_BUFFER, _TRUNCATE, fmt, args);
    va_end(args);
    buffer[MAX_BUFFER - 1] = '\0';

	//This code is intended to detect loading process for fbanext and mame
    if (!gameMenu->romLoaded && progress_loader.total_rom_files > 0) {
        if (strstr(buffer, "Opening ROM file:")) {
            progress_loader.current_rom_file++;
            float progress = (float)progress_loader.current_rom_file / (float)progress_loader.total_rom_files;
            drawLoadingProgressBar(gameMenu->overlay, (progress > 1.0f) ? 1.0f : progress);
        }
    }

	//Log the output of the core
	#ifdef DEBUG_LOG
		OutputDebugStringA(buffer);
	#else 
		if (level == RETRO_LOG_ERROR) {
			OutputDebugStringA(buffer);
		}
	#endif
}

// ─────────────────────────────────────────────
// Helper: split "opt1|opt2|opt3" → vector
// ─────────────────────────────────────────────
namespace {
	std::vector<std::string> splitOptions(const std::string& raw) {
		std::vector<std::string> out;
		std::istringstream ss(raw);
		std::string token;
		while (std::getline(ss, token, '|')) {
			if (!token.empty()) out.push_back(token);
		}
		return out;
	}

	/** Necesitamos borrar ciertos elementos porque sino da la sensacion de que tenemos 
	*   opciones del core duplicadas. Por ejemplo:
	*
	* Key: fbneo-dipswitch-msx_lic2kill-BIOS_-_NOTE__Changes_require_re-start!, Selected: 0
	* Key: fbneo-dipswitch-msx_007tld-BIOS_-_NOTE__Changes_require_re-start!, Selected: 0
	* Key: fbneo-dipswitch-msx_10yard-BIOS_-_NOTE__Changes_require_re-start!, Selected: 0
	*/
	void cleanPrefix(std::map<std::string, std::unique_ptr<cfg::t_emu_props> > &data) {
		const std::string prefijo = "fbneo-dipswitch";
    
		auto it = data.begin();
		while (it != data.end()) {
			// Comprobar si la clave empieza por el prefijo
			if (it->first.compare(0, prefijo.length(), prefijo) == 0) {
				// Guardar el iterador actual y avanzarlo antes de borrar
				auto it_borrar = it++; 
				data.erase(it_borrar);
			} else {
				// Avanzar normalmente si no cumple la condición
				++it;
			}
		}
	}
	/** Necesitamos borrar ciertos elementos porque sino da la sensacion de que tenemos 
	*   opciones del core duplicadas. Por ejemplo:
	*
	* Key: fbneo-dipswitch-msx_lic2kill-BIOS_-_NOTE__Changes_require_re-start!, Selected: 0
	* Key: fbneo-dipswitch-msx_007tld-BIOS_-_NOTE__Changes_require_re-start!, Selected: 0
	* Key: fbneo-dipswitch-msx_10yard-BIOS_-_NOTE__Changes_require_re-start!, Selected: 0
	*
	* Este metodo devolvera la key sin incluir el juego en particular: 
	*      fbneo-dipswitch-msx-BIOS_-_NOTE__Changes_require_re-start!
	*/
	std::string cleanPerGameKey(std::string key){
		std::string validKey = key;
		const std::string prefijo = "fbneo-dipswitch";
		if (validKey.compare(0, prefijo.length(), prefijo) == 0) {
			std::size_t posUnderscore = validKey.find_first_of("_");
			if (posUnderscore != string::npos){
				std::size_t nextMinus = validKey.substr(posUnderscore).find_first_of("-");
				if (nextMinus != string::npos){
					validKey = validKey.substr(0, posUnderscore) + 
						validKey.substr(posUnderscore + nextMinus);
				}
			}
		}
		return validKey;
	}

	// Crea o actualiza una entrada preservando `selected` si ya existía.
	void applyEntry(std::map<std::string, std::unique_ptr<cfg::t_emu_props> > &data,
					const std::string& key,
					std::string description,       // Pasamos por valor para mover
					std::vector<std::string> values, // Pasamos por valor para mover
					int defaultIdx = 0)
	{
		std::string validKey = cleanPerGameKey(key);
    
		auto it = data.find(validKey);
		if (it != data.end()) {
			if (it->second->description.empty())
				it->second->description = description;

			if (it->second->values.empty())
				it->second->values = values;
        
			if (it->second->cachedValue.empty() && !it->second->values.empty())
				it->second->cachedValue = it->second->values[it->second->selected];

			LOG_DEBUG("[Core Options] SET. Key already defined %s", validKey.c_str());
			return;
		} 

		cfg::t_emu_props *raw = new cfg::t_emu_props();
		raw->description = std::move(description);
		raw->values      = std::move(values);
		raw->selected    = defaultIdx;

		if (!raw->values.empty())
			raw->cachedValue = raw->values[defaultIdx];

		LOG_DEBUG("[Core Options] SET %s = %s", validKey.c_str(), raw->cachedValue.c_str());
		data[validKey] = std::unique_ptr<cfg::t_emu_props>(raw); 
	}
} // namespace anónimo


/**
*
*/
void initializeMenus(ListMenu &menuData, GameMenu &gameMenu, CfgLoader &cfgLoader){
    struct ListStatus menuBeforeExit;
	dirutil dir;

    int retMenu = gameMenu.recoverGameMenuPos(menuData, menuBeforeExit);
    if (retMenu == 0){
        if (menuBeforeExit.layout != menuData.layout){
            menuData.setLayout(menuBeforeExit.layout, gameMenu.overlay->w, gameMenu.overlay->h);
        }
        menuData.animateBkg = menuBeforeExit.animateBkg;
    }

	if (retMenu == 0 && menuBeforeExit.zipname[0] != '\0' && menuBeforeExit.zippedPath[0] != '\0'){
		LOG_DEBUG("Loading zip %s...", menuBeforeExit.zipname);
		LOG_DEBUG("...Internal path %s", menuBeforeExit.zippedPath);
		menuData.listZipped.setInternalDir(menuBeforeExit.zippedPath);
		menuData.listZipped.dir = dir.getFolder(menuBeforeExit.zipname);
		menuData.listZipped.file = dir.getFileName(menuBeforeExit.zipname);
		FILE_STATUS fs = gameMenu.listableZip(menuData, FS_ZIP_CD);
	} else {
		if (menuBeforeExit.relativePath[0] != '\0'){
			menuData.listDir.setRelativePath(menuBeforeExit.relativePath);
		}
		gameMenu.loadEmuCfg(menuData);
	}

    if (retMenu == 0 && menuData.maxLines == menuBeforeExit.maxLines 
		&& menuBeforeExit.iniPos >= 0 && menuBeforeExit.iniPos < menuData.listSize
		&& menuBeforeExit.endPos > 0 && menuBeforeExit.endPos <= menuData.listSize
		&& menuBeforeExit.curPos > 0 && menuBeforeExit.curPos < menuData.listSize){
		//Setting the filter if it was set
		menuData.gameDataFields.onlyParents = menuBeforeExit.onlyParents;
		menuData.gameDataFields.posManufacturer = menuBeforeExit.posManufacturer;
		menuData.gameDataFields.posSystem = menuBeforeExit.posSystem;
		menuData.gameDataFields.posYear = menuBeforeExit.posYear;
		menuData.checkFilter();
		//Seting the menu to the proper position and the selected element
		menuData.iniPos = menuBeforeExit.iniPos;
        menuData.endPos = menuBeforeExit.endPos;
        menuData.curPos = menuBeforeExit.curPos;
    } else {
		//Algun dato no es correcto segun el tamanyo de la lista
		menuData.resetIndexPos();
	}
    gameMenu.createMenuImages(menuData);
}

void drawLoadingProgressBar(SDL_Surface* screen, float progress) {
    if (!screen) return;

    // Configuración de dimensiones
    int barW = screen->w / 2;
    int barH = 20;
    int barX = (screen->w - barW) / 2;
    int barY = (screen->h / 2) + 40; // Debajo del texto de "Loading..."

    // Colores (Ajusta según tu paleta)
    Uint32 colorBorder		= SDL_MapRGBA(screen->format, 200, 200, 200, 0xFF);
    Uint32 colorFill		= SDL_MapRGBA(screen->format, bkgMenu.r, bkgMenu.g, bkgMenu.b, 0xFF);
	Uint32 colorFillLighter = SDL_MapRGBA(screen->format, bkgMenuLighter.r, bkgMenuLighter.g, bkgMenuLighter.b, 0xFF);
    Uint32 colorBG			= SDL_MapRGBA(screen->format, 40, 40, 40, 0xFF);
	
    //Dibujar fondo de la barra
    SDL_Rect bgRect = { (Sint16)barX, (Sint16)barY, (Uint16)barW, (Uint16)barH };
	//Actualizamos el area de la barra y el area del texto para que se puedan ver
	SDL_Rect bgRectFill = { bgRect.x, bgRect.y, bgRect.w, barH * 5};
	SDL_FillRect(screen, &bgRectFill, Constant::colors[clBackground].color);
	//Mostramos el fondo de la barra
    SDL_FillRect(screen, &bgRect, colorBG);

    //Dibujar el progreso real
    if (progress > 1.0f) progress = 1.0f;
    int fillW = (int)(barW * progress);
    if (fillW > 0) {
        SDL_Rect fillRect = { (Sint16)barX, (Sint16)barY, (Uint16)fillW, (Uint16)(barH / 2.0) };
        SDL_FillRect(screen, &fillRect, colorFillLighter);
		fillRect.y += (Uint16)(barH / 2);
        SDL_FillRect(screen, &fillRect, colorFill);
    }

	const int txtW = 40;
	SDL_Rect percentRect = { barX + barW / 2 - txtW, barY + barH, 80, barH * 4 };
	percentRect.x += txtW;
	percentRect.y += 15;
	PB_drawPercent(screen, (int)(progress * 100.0), percentRect.x, percentRect.y, 3, PBUtil::rgb(screen, 100, 210, 255));

    //Dibujar borde (opcional, 1px)
    //SDL_FillRect no tiene "drawRect" vacío, así que usamos 4 líneas si quieres borde fino
    //Actualizar solo la región de la barra para ganar rendimiento
    //SDL_UpdateRect(screen, barX, barY, barW, 3*barH);
	SDL_FillRect(gameMenu->gameScreen, NULL, Constant::colors[clBackground].color);
	SDL_Flip(gameMenu->gameScreen);
}

void initGameAudio(double sampleRate){
	/* Inicializar SDL Audio con la frecuencia del core.
	 *
	 * El device se abre UNA SOLA VEZ por sesion (primer juego cargado)
	 * y se mantiene abierto hasta cierre de la app — abrirlo/cerrarlo
	 * en cada carga colgaba SDL_OpenAudio en Xbox 360 tras varias
	 * iteraciones (ver closeGame para el rationale).
	 *
	 * En cargas posteriores el device sigue abierto (audio_opened==1)
	 * pero el callback esta pausado.  Reanudamos con PauseAudio(0).
	 *
	 * RESET_AUDIO (opt-in): si el sample_rate cambia entre cargas
	 * (p.ej. FBNeo Neo Geo cart 48011 Hz -> Neo Geo CD 48000 Hz), el
	 * consumidor SDL queda desincronizado y aparecen pops.  Con
	 * RESET_AUDIO definido reabrimos el device a la nueva tasa.  Es
	 * el camino "C": resuelve el desajuste a cambio de exponer el bug
	 * historico de SDL_OpenAudio/CloseAudio en 360.  Sin la macro se
	 * mantiene el comportamiento estable de no reabrir nunca. */
	if (!audio_opened){
		init_sdl_audio(sampleRate);
	} else {
#ifdef RESET_AUDIO
		int new_rate = (int)sampleRate;
		if (new_rate > 0 && new_rate != g_audio_opened_rate) {
			LOG_DEBUG("RESET_AUDIO: rate change %d -> %d, reopening SDL audio\n",
				g_audio_opened_rate, new_rate);
			audio_closing = true;
			SDL_PauseAudio(1);
			SDL_Delay(50);              // dejar terminar el callback en vuelo
			gameMenu->g_audioBuffer.Clear();
			SDL_CloseAudio();
			audio_opened = 0;
			g_audio_opened_rate = 0;
			audio_closing = false;
			init_sdl_audio(sampleRate);
		} else {
			SDL_PauseAudio(0);
		}
#else
		SDL_PauseAudio(0);
#endif
	}
	gameMenu->g_audioRate.init(BUFF_SIZE);
}

bool extractAndLoadGame(std::string rompath, bool tmpDelete = true){
	bool container = false;
	bool isM3U = false;
	struct retro_system_info info;
	memset(&info, 0, sizeof(info));
	bool gameLoaded;
	unzippedFileInfo unzipped;
	const bool loadAchievement = gameMenu->getCfgLoader()->configMain[cfg::enableAchievements].valueBool;
	const std::string tempDir = Constant::getTmpDir();
	const bool bios_only = rompath.compare(BIOS_ONLY) == 0;
	dirutil dir;

	// Don't delete the tmp dir if the rom we pretend to load is inside it.
	// That is for the new functionality to be able to load a romfile that is inside the file structure
	// of a zip file, and has previously extracted to the tmp dir to be loaded next. 
	// See GameMenu::listableZip and ZipBrowser class
	tmpDelete = tmpDelete && !dir.isChild(tempDir, rompath);

	romPaths.rompath.clear();
	closeGame();

	//Obtaint the valid extensions to be loaded from the core information received
	retro_get_system_info(&info);
	std::string allowedExtensions = Constant::replaceAll(info.valid_extensions, "|", " ");
	LOG_DEBUG("Extensiones: %s\n", info.valid_extensions);

	if (bios_only) {
		LOG_DEBUG("BIOS-only mode: skipping container detection / unzip\n");
		/* Dejar unzipped en estado "vacio coherente" — los siguientes
		 * pasos del flujo comun lo veran como "no hay fichero". */
		unzipped.errorCode = 0;
		unzipped.extractedPath = "";
		unzipped.originalPath  = "";
		unzipped.memoryBuffer  = NULL;
		unzipped.romsize       = 0;
	} else {
		detectContainer(rompath, isM3U, container);
		const bool noUncompress = gameMenu->getCfgLoader()->getCfgEmu()->no_uncompress || container;
		if (noUncompress){
			LOG_DEBUG("Loading rom directly %s", rompath.c_str());
			progress_loader.reset();
			progress_loader.total_rom_files = getZipFileCountFiltered(rompath);

			if (!container){
				// Dibujamos la barra inicial al 0%
				drawLoadingProgressBar(gameMenu->overlay, 0.0f);
			}
			unzipped.errorCode = 0;
			unzipped.extractedPath = rompath;
			unzipped.originalPath  = rompath;
		} else {
			if (tmpDelete && dir.dirExists(tempDir.c_str())){
				dir.borrarDir(tempDir);
			}
			if (dir.createDir(tempDir) <= 0){
				LOG_ERROR("Error creating the temporary directory %s\n", tempDir.c_str());
				gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.direrror") + tempDir, 3000);
				return false;
			}
			LOG_DEBUG("Unzipping or loading rom %s", rompath.c_str());
			unzipped = unzipOrLoad(rompath, allowedExtensions, !info.need_fullpath, tempDir);
		}

		if (unzipped.errorCode != 0){
			LOG_ERROR("No se ha podido abrir el fichero o no se puede descomprimir: %s", rompath.c_str());
			gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.openfileerror") + rompath, 3000);
			return false;
		}
	}

	// Llamada para iniciar el core
	retro_init();
	
	if (bios_only) {
		// Pasar NULL al core: el core debe haber anunciado SET_SUPPORT_NO_GAME.
		gameLoaded = retro_load_game(NULL);
	} else {
		struct retro_game_info game = { unzipped.extractedPath.c_str(), unzipped.memoryBuffer, unzipped.romsize, NULL };
		// Multi-disc: si es un M3U, indicamos al core qué disco cargar de inicio
		// antes de retro_load_game (asi el savestate coincide con el disco correcto).
		findInitialImage(rompath, isM3U);
		// ******* Cargamos el juego en memoria **********
		gameLoaded = retro_load_game(&game);
		// ***********************************************
	}

	//Liberar la memoria tras la carga exitosa
	//La mayoría de los cores de Libretro ya han copiado los datos a su propia RAM interna
	//Si hay logros habilitados, ya se encarga de liberarse posteriormente
	if (unzipped.memoryBuffer && !loadAchievement){
		free(unzipped.memoryBuffer);
		unzipped.memoryBuffer = NULL;
	}

	//After the loading of the game, we load the achievements
	Achievements::instance()->clearAllData();
	if (!bios_only && gameLoaded && loadAchievement){
		gameMenu->loadGameAchievements(unzipped);
	}

	return gameLoaded;
}

