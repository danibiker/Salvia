//Evita errores al usar el min o max de windows.h al incluir el filtro "io/xbrz/xbrz.h"
#define NOMINMAX 

#include <string>
#include <map>
#include <algorithm>
#include <zlib.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_thread.h>

#include <menus/gameMenu.h>
#include <const/menuconst.h>
#include <image/icons.h>
#include <uiobjects/listmenu.h>
#include <uiobjects/tilemap.h>
#include <unzip/unziptool.h>
#include <utils/langmanager.h>
#include <io/cfgloader.h>
#include <io/dirutil.h>
#include <io/progress_bar.h>
#include <io/statesram.h>
#include <io/inputsmenu.h>
#include <io/inputscore.h>
#include <io/dischelper.h>
#include <so/launcher.h>
#include <so/soutils.h>

#include <libretro/libretro.h>
#include <libretro/vfs.h>

CfgLoader *cfgLoader;
GameMenu *gameMenu;
ListMenu *listMenu;
Logger *logger;
dirutil dir;
TileMap tileMap(9, 0, 16, 16);

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
const std::string CfgLoader::CONFIGFILE = "salvia.cfg";
const char* gameCategories[] = {"dipswitch", "cheat", "ips", "romdata"};

volatile uint32_t Constant::totalTicks = 0;
int Constant::EXEC_METHOD = launch_batch;
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

/* Tasa a la que se PIDE abrir el dispositivo de audio.  Se abre una sola vez al
 * arrancar y no se vuelve a tocar en toda la sesion: en Xbox 360 el ciclo
 * SDL_OpenAudio/SDL_CloseAudio repetido cuelga libSDLx360 (ver closeGame).  Cada
 * core entrega su audio a su propia tasa y lo adapta AudioRateControl.
 *
 * 48000 y no 44100 porque en la 360 el mastering voice de XAudio2 ya trabaja a
 * 48000, asi que pedir esa misma tasa evita un segundo remuestreo dentro de
 * XAudio2 (SDL_xboxaudio.c, XboxDX_OpenAudio crea el source voice con
 * spec->freq). */
#define AUDIO_DEVICE_RATE 48000

/* Tasa REAL con la que quedo abierto el dispositivo.  Puede no ser
 * AUDIO_DEVICE_RATE: init_sdl_audio lee el `obtained` de SDL, asi que si un
 * driver de Windows abre a otra tasa el resampler usa esta y todo sigue
 * cuadrando en vez de salir desafinado. */
static int g_audio_device_rate = AUDIO_DEVICE_RATE;
t_rom_paths romPaths;
// CRC32 de la ROM cargada (0 si desconocido). Para resolver el .cht por hash via .rdb.
uint32_t g_currentRomCrc = 0;

// CRC32 estilo No-Intro de una ROM en memoria. Salta la cabecera iNES (NES, 16B) y
// el header de copier (SNES, 512B) para que el CRC cuadre con el de No-Intro.
static inline uint32_t computeRomCrc(const uint8_t* data, std::size_t size){
	if (!data || size == 0) return 0;
	std::size_t off = 0;
	if (size >= 16 && data[0]=='N' && data[1]=='E' && data[2]=='S' && data[3]==0x1A)
		off = 16;                          // cabecera iNES
	else if ((size % 1024) == 512)
		off = 512;                         // cabecera de copier (SNES, etc.)
	if (off >= size) off = 0;
	uLong c = crc32(0L, Z_NULL, 0);
	c = crc32(c, data + off, (uInt)(size - off));
	return (uint32_t)c;
}

// CRC32 estilo No-Intro de una ROM en disco, para cores need_fullpath (memoryBuffer
// == NULL, solo hay fichero extraido). Streaming por bloques + skip de cabecera
// iNES/SNES. Devuelve 0 si no se puede abrir o si el fichero es demasiado grande:
// las imagenes de CD (cientos de MB) no se hashean -- su CRC no cuadra con No-Intro
// y evitamos el retardo en cada carga (caeria al match por nombre).
static inline uint32_t computeRomCrcFromFile(const std::string& path){
	if (path.empty()) return 0;
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return 0;
	uint32_t crc = 0;
	do {
		if (fseek(f, 0, SEEK_END) != 0) break;
		long sz = ftell(f);
		if (sz <= 0) break;
		const long CRC_FILE_MAX = 96L * 1024 * 1024;   // tope: no hashear imagenes de CD
		if (sz > CRC_FILE_MAX) break;

		if (fseek(f, 0, SEEK_SET) != 0) break;
		unsigned char head[4] = {0, 0, 0, 0};
		std::size_t nh = fread(head, 1, 4, f);
		long off = 0;
		if (nh >= 4 && head[0]=='N' && head[1]=='E' && head[2]=='S' && head[3]==0x1A)
			off = 16;                        // cabecera iNES
		else if ((sz % 1024) == 512)
			off = 512;                       // cabecera de copier (SNES, etc.)
		if (off >= sz) off = 0;

		if (fseek(f, off, SEEK_SET) != 0) break;
		uLong c = crc32(0L, Z_NULL, 0);
		unsigned char buf[32768];
		std::size_t r;
		while ((r = fread(buf, 1, sizeof(buf), f)) > 0)
			c = crc32(c, buf, (uInt)r);
		crc = (uint32_t)c;
	} while (0);
	fclose(f);
	return crc;
}

// Current ROM path (needed to persist last disc index on closeGame) and to add information
// to the exception
static std::string g_currentRompath;

//Indica si el core puede arrancar sin disco introducido
bool g_currentCoreSupportsNoGame;

double nextFrameTime;

bool g_start_from_exception = false;
static std::string g_excp_emulator_path;

/* [XBOX360] Contexto pasado al watcher thread.
 *
 * Las surfaces SDL/TTF se pre-rendereeron en el MAIN thread antes de
 * lanzar el watcher.  Esto elimina las llamadas no-thread-safe a
 * Fonts::renderUtf8Blended y SDL_CreateRGBSurface desde el watcher.
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
				SDL_SetAlpha(ctx->rawSurface, SDL_RLEACCEL, 0);
				if (gameMenu->overlay){
					if (!hangDetected){
						gameMenu->clearOverlay();
					}
					SDL_BlitSurface(ctx->rawSurface, nullptr, gameMenu->overlay, &ctx->drawRect);
				}
				//Procesamos las hotkeys
				gameMenu->joystick->pollKeys(gameMenu->getEmuStatus());
				HOTKEYS_LIST hotkey = gameMenu->joystick->findHotkey();
				if (hotkey == HK_EXIT_GAME){
					LOG_ERROR("Requested exit");
				}

				hangDetected = true;
				salviaFlip(gameMenu->gameScreen);
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
	 * una sola vez por sesion).  Fonts::renderUtf8Blended y SDL_CreateRGBSurface
	 * no son thread-safe respecto a otras llamadas SDL/TTF concurrentes, asi
	 * que las hacemos AQUI en el main thread.  El watcher solo hara
	 * SDL_FillRect/Blit/Flip sobre estas surfaces ya creadas. */
	static SDL_Color s_watcherColors[] = {Constant::colors[clWhite].sdlColor, Constant::colors[clYellow].sdlColor, Constant::colors[clBlue].sdlColor, Constant::colors[clRed].sdlColor};
	static const int  s_watcherNumColors = sizeof(s_watcherColors) / sizeof(s_watcherColors[0]);
	static SDL_Surface* s_watcherText[s_watcherNumColors] = {0};
	static SDL_Surface* s_watcherRaw = nullptr;
	static SDL_Rect     s_watcherRect = {0};
	static Uint32       s_watcherKeyBg = 0;
	static LoadingWatcherCtx s_watcherCtx = {0};

	if (s_watcherRaw == nullptr) {
		const char* msg = "waiting for the game to load";
		for (int i = 0; i < s_watcherNumColors; i++) {
			s_watcherText[i] = Fonts::renderUtf8Blended(
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

void drawLoadingProgressBar(SDL_Surface*& screen, float progress);

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

	const unsigned int MAX_BUFFER = 128;
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
					std::string description,        // Pasamos por valor para mover
					std::vector<std::string> values, // Pasamos por valor para mover
					int defaultIdx,
					std::vector<std::string> labels,
					const std::string& category = "")
	{
		std::string validKey = cleanPerGameKey(key);

		auto it = data.find(validKey);
		if (it != data.end()) {
			if (it->second->description.empty())
				it->second->description = description;

			if (it->second->values.empty())
				it->second->values = values;

			if (it->second->labels.empty() && !labels.empty())
				it->second->labels = labels;

			if (it->second->cachedValue.empty() && !it->second->values.empty())
				it->second->cachedValue = it->second->values[it->second->selected];

			it->second->isForThisGame = true;
			it->second->category = category;
			// El default lo declara el core (no se persiste); lo refrescamos
			// siempre, incluso en claves ya cargadas de config, para "restaurar".
			it->second->defaultSelected = defaultIdx;

			LOG_DEBUG("[Core Options] SET. Key already defined %s", validKey.c_str());
			return;
		}

		cfg::t_emu_props *raw = new cfg::t_emu_props();
		raw->description = std::move(description);
		raw->values      = std::move(values);
		raw->labels      = std::move(labels);
		raw->selected    = defaultIdx;
		raw->defaultSelected = defaultIdx;
		raw->isForThisGame = true;
		raw->category    = category;

		if (!raw->values.empty())
			raw->cachedValue = raw->values[defaultIdx];

		LOG_DEBUG("[Core Options] SET %s = %s", validKey.c_str(), raw->cachedValue.c_str());
		data[validKey] = std::unique_ptr<cfg::t_emu_props>(raw); 
	}

	void applyEntry(std::map<std::string, std::unique_ptr<cfg::t_emu_props> > &data,
					const std::string& key,
					std::string description,
					std::vector<std::string> values,
					int defaultIdx = 0)
	{
		std::vector<std::string> emptyLabels;
		applyEntry(data, key, description, values, defaultIdx, emptyLabels);
	}
} // anonim namespace


/**
*
*/
void initializeMenus(ListMenu &menuData, GameMenu &gameMenu, CfgLoader &cfgLoader){
    struct ListStatus menuBeforeExit;
	dirutil dir;

    int retMenu = cfgLoader.recoverGameMenuPos(menuBeforeExit);
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
		//Load the background and the title
		gameMenu.loadBgImageAndTitleEmu();
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

void drawLoadingProgressBar(SDL_Surface*& screen, float progress) {
    if (!screen) return;

    // Configuración de dimensiones
	const int face_h_big = Fonts::getLineSkip(Fonts::FONTBIG);
    const int barW = screen->w / 2;
    const int barH = 20;
    const int barX = (screen->w - barW) / 2;
    const int barY = (screen->h / 2) + 1; // Debajo del texto de "Loading..."

   
    //Dibujar fondo de la barra
    SDL_Rect bgRect = { (Sint16)barX, (Sint16)barY, (Uint16)barW, (Uint16)barH };
	//Actualizamos el area de la barra y el area del texto para que se puedan ver
	SDL_Rect bgRectFill = { bgRect.x, bgRect.y, bgRect.w, barH * 5};
	SDL_FillRect(screen, &bgRectFill, Constant::colors[clBackground].color);
	//Mostramos el fondo de la barra
    SDL_FillRect(screen, &bgRect, Constant::colors[clBG].color);

    //Dibujar el progreso real
    if (progress > 1.0f) progress = 1.0f;
    int fillW = (int)(barW * progress);
    if (fillW > 0) {
        SDL_Rect fillRect = { (Sint16)barX, (Sint16)barY, (Uint16)fillW, (Uint16)(barH / 2.0) };
        SDL_FillRect(screen, &fillRect, Constant::colors[clBkgMenuLighter].color);
		fillRect.y += (Uint16)(barH / 2);
        SDL_FillRect(screen, &fillRect, Constant::colors[clBkgMenu].color);
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
	salviaFlip(gameMenu->gameScreen);
}

/* Prepara el audio para el juego que se acaba de cargar.
 *
 * NO abre ni reabre el dispositivo: eso se hizo una sola vez al arrancar
 * (init_sdl_audio con AUDIO_DEVICE_RATE).  Aqui solo se le dice al resampler a
 * que tasa entrega este core, y se reanuda el callback que closeGame dejo
 * pausado.
 *
 * Antes esto abria el device con la tasa del PRIMER juego y se quedaba asi para
 * toda la sesion, de modo que un juego posterior con otra tasa sonaba a
 * velocidad y tono equivocados.  La valvula de escape que habia (RESET_AUDIO,
 * reabrir el device) queda eliminada porque reexponia el cuelgue de
 * SDL_OpenAudio/SDL_CloseAudio de libSDLx360 en la 360; ahora el desajuste se
 * resuelve remuestreando, sin tocar el dispositivo. */
void initGameAudio(double sampleRate){
	if (!audio_opened){
		/* Red de seguridad: si por lo que sea el arranque no pudo abrirlo,
		 * intentarlo aqui -- pero SIEMPRE a la tasa del dispositivo, nunca a la
		 * del core. */
		init_sdl_audio(AUDIO_DEVICE_RATE);
	} else {
		SDL_PauseAudio(0);
	}

	gameMenu->g_audioRate.init(BUFF_SIZE);
	gameMenu->g_audioRate.setRates(sampleRate, (double)g_audio_device_rate);
	LOG_INFO("Audio: core a %.1f Hz -> dispositivo a %d Hz (ratio %.4f)\n",
		sampleRate, g_audio_device_rate, gameMenu->g_audioRate.getBaseRatio());
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

	//Loading the rompaths
	g_currentRompath = rompath;

	//Obtain the valid extensions to be loaded from the core information received
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
	} else if (!unzipped.extractedPath.empty() || !info.need_fullpath){
		// Duplicamos el string para asegurar que la memoria no desaparezca durante la carga
		char* safePath = NULL;
		if (!unzipped.extractedPath.empty())
			safePath = strdup(unzipped.extractedPath.c_str());

		struct retro_game_info game = { safePath, unzipped.memoryBuffer, unzipped.romsize, NULL };
		// Multi-disc: si es un M3U, indicamos al core qué disco cargar de inicio
		// antes de retro_load_game (asi el savestate coincide con el disco correcto).
		findInitialImage(rompath, isM3U);
		// ******* Cargamos el juego en memoria **********
		gameLoaded = retro_load_game(&game);
		// ***********************************************
		if (safePath != NULL) 
			free(safePath);
	} else {
		LOG_ERROR("No se ha podido obtener el path del fichero extraido: %s", unzipped.originalPath.c_str());
		gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.openfileerror") + rompath, 3000);
		return false;
	}

	// CRC32 de la ROM (para resolver el .cht via .rdb por hash; fallback a nombre).
	// Con buffer en memoria lo hasheamos directo; si el core es need_fullpath y solo
	// hay fichero extraido en disco, hasheamos ese fichero (unzipped.extractedPath).
	if (unzipped.memoryBuffer && unzipped.romsize > 0)
		g_currentRomCrc = computeRomCrc((const uint8_t*)unzipped.memoryBuffer, unzipped.romsize);
	else
		g_currentRomCrc = computeRomCrcFromFile(unzipped.extractedPath);

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
/**
* Obtains all the parameters sent by RETRO_ENVIRONMENT_SET_VARIABLES and sets the parameter map in memory
* with all the values
*/
void processParameters(const retro_variable* vars, std::map<std::string, std::unique_ptr<cfg::t_emu_props> > &paramsMap){
	//cleanPrefix(gameMenu->getCfgLoader()->startupLibretroParams);
	for (int i = 0; vars[i].key != nullptr; ++i) {
		// 1. Protección contra keys vacias (basura recurrente en algunos cores)
		if (vars[i].key[0] == '\0') continue;

		const std::string key      = vars[i].key;
		// 2. Protección contra valores nulos o vacíos
		if (!vars[i].value || vars[i].value[0] == '\0') {
			LOG_DEBUG("[Core Options] SKIP: Key %s has no value string", key.c_str());
			continue;
		}

		const std::string rawValue = vars[i].value;
		const std::size_t sep = rawValue.find("; ");

		// 3. Protección de Formato: Si no hay "; ", el core está enviando algo fuera de estándar
		if (sep != std::string::npos && sep > 0) {
			std::string desc = rawValue.substr(0, sep);
			std::string optionsPart = rawValue.substr(sep + 2);

			// 4. Validación extra: ¿Hay opciones después del separador?
			if (!optionsPart.empty()) {
				std::vector<std::string> values = splitOptions(optionsPart);
                
				if (!values.empty()) {
					LOG_DEBUG("[Core Options] PARSE OK: %s", key.c_str());
					applyEntry(paramsMap, key, desc, std::move(values), 0);
				} else {
					LOG_DEBUG("[Core Options] ERROR: No split tokens in %s", key.c_str());
				}
			}
		} else {
			// DOSBox Pure a veces envía notificaciones que no son definiciones de opciones
			LOG_DEBUG("[Core Options] INFO: Key %s format not recognized (Value: %s)", key.c_str(), rawValue.c_str());
		}
	}
}


/**
* Sets a parameter value if it exists on the parameter map
*/
bool setParameter(retro_variable* var, std::map<std::string, std::unique_ptr<cfg::t_emu_props> > &paramsMap){
	auto it = paramsMap.find(var->key);
	if (it == paramsMap.end()) {
		var->value = nullptr;
		return false;
	}

	const int nVals = static_cast<int>(it->second->values.size());
	const int sel   = it->second->selected;

	if (nVals > 0 && sel >= 0 && sel < nVals)
		it->second->cachedValue = it->second->values[sel];
	else if (nVals > 0)
		it->second->cachedValue = it->second->values[0];
	else {
		var->value = nullptr;
		return false;
	}
	var->value = it->second->cachedValue.c_str();
	LOG_DEBUG("[Core Options] GET %s = %s", var->key, var->value);
	return true;
}