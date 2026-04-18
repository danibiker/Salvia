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

CfgLoader *cfgLoader;
GameMenu *gameMenu;
Logger *logger;
dirutil dir;

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