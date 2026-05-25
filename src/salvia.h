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
int launchGame(std::string);
void closeGame();
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

DWORD WINAPI th_printLoading(LPVOID data) {
	LoadingWatcherCtx* ctx = (LoadingWatcherCtx*)data;
	uint32_t cycles = 0;
	uint8_t  colors = 0;

	const uint16_t updateCycle = 10;
	const uint16_t updateDelay = 1000 / (float) updateCycle;
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
