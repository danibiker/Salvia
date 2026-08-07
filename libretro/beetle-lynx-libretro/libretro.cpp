#include <stdarg.h>
#include "mednafen/mednafen.h"
#include "mednafen/mednafen-endian.h"
#include "mednafen/mempatcher.h"
#include "mednafen/git.h"
#include "mednafen/general.h"
#include <libretro.h>
#include <streams/file_stream.h>
#include <algorithm>
#include "mednafen/lynx/system.h"
#include "libretro_core_options.h"

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#ifdef _WIN32
#define SLASH "\\"
#else
#define SLASH "/"
#endif

struct retro_perf_callback perf_cb;
retro_get_cpu_features_t perf_get_cpu_features_cb = NULL;
retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static bool overscan;
static bool force_60hz;
static MDFN_PixelFormat last_pixel_format;

static unsigned rotate_mode;
static unsigned rotate_fixed;
static unsigned rotate_screen;
static unsigned rotate_screen_last_frame;
static unsigned select_pressed_last_frame;

static MDFN_Surface *surf;

static bool libretro_supports_option_categories = false;

static char retro_system_directory[4096];

static bool libretro_supports_input_bitmasks;
static int system_color_depth = 16;

extern MDFNGI EmulatedLynx;
MDFNGI *MDFNGameInfo = &EmulatedLynx;

#define MEDNAFEN_CORE_NAME_MODULE "lynx"
#define MEDNAFEN_CORE_NAME "Beetle Lynx"
#define MEDNAFEN_CORE_VERSION "v1.24.0"
#define MEDNAFEN_CORE_EXTENSIONS "lnx|lyx|bll|o"
#define MEDNAFEN_CORE_TIMING_FPS 75.0
#define MEDNAFEN_CORE_GEOMETRY_BASE_W 160
#define MEDNAFEN_CORE_GEOMETRY_BASE_H 102
#define MEDNAFEN_CORE_GEOMETRY_MAX_W 160
#define MEDNAFEN_CORE_GEOMETRY_MAX_H 102
#define MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO (80.0 / 51.0)
#define FB_WIDTH 160
#define FB_HEIGHT 102

#define FB_MAX_HEIGHT FB_HEIGHT

static void check_system_specs(void)
{
   unsigned level = 0;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

void retro_init(void)
{
   const char *dir = NULL;
   struct retro_log_callback log;
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
      snprintf(retro_system_directory, sizeof(retro_system_directory), "%s", dir);

   if (environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb))
      perf_get_cpu_features_cb = perf_cb.get_cpu_features;
   else
      perf_get_cpu_features_cb = NULL;

   check_system_specs();

   libretro_set_core_options(environ_cb,
		   &libretro_supports_option_categories);

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_input_bitmasks = true;
}

void retro_reset(void)
{
   DoSimpleCommand(MDFN_MSC_RESET);
}

bool retro_load_game_special(unsigned, const struct retro_game_info *, size_t)
{
   return false;
}

static void check_variables(void)
{
   struct retro_variable var = {0};

   var.key = "lynx_rot_screen";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      unsigned last_val1 = rotate_mode;
      unsigned last_val2 = rotate_fixed;
      if (strcmp(var.value, "auto") == 0)
      {
         rotate_mode   = 1;
         rotate_fixed  = 0;
      }
      else if (strcmp(var.value, "manual") == 0)
      {
         rotate_mode   = 2;
         rotate_fixed  = 0;
      }
      else if (strcmp(var.value, "0") == 0)
      {
         rotate_mode   = 0;
         rotate_fixed  = 0;
      }
      else if (strcmp(var.value, "90") == 0)
      {
         rotate_mode   = 0;
         rotate_fixed  = 1;
      }
      else if (strcmp(var.value, "180") == 0)
      {
         rotate_mode   = 0;
         rotate_fixed  = 2;
      }
      else if (strcmp(var.value, "270") == 0)
      {
         rotate_mode   = 0;
         rotate_fixed  = 3;
      }
   }

   var.key = "lynx_force_60hz";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "enabled") == 0)
	 force_60hz = true;
      else
         force_60hz = false;
   }
}

#define MAX_PLAYERS 1
#define MAX_BUTTONS 9
static uint8_t input_buf[2];

static bool MDFNI_LoadGame(const uint8_t *data, size_t size)
{
	if (!data || !size)
		MDFNGameInfo = NULL;

	MDFNFILE *GameFile = file_open_mem(data, size);

	if (!GameFile)
		return false;

	MDFNGameInfo = &EmulatedLynx;

	//
	// Load per-game settings
	//
	// Maybe we should make a "pgcfg" subdir, 
	// and automatically load all files in it?
	// End load per-game settings
	//
	char bios_path[2048];
	snprintf(bios_path, sizeof(bios_path),"%s" SLASH "%s", retro_system_directory, "lynxboot.img");

	Load(GameFile, bios_path);

	MDFN_LoadGameCheats();
	MDFNMP_InstallReadPatches();

	return true;
}

static bool init_pix_format(void)
{
   struct retro_variable var = { "lynx_pix_format", NULL };
   bool ret = true;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      system_color_depth = atoi(var.value);
   }

   if (system_color_depth == 32)
   {
      enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
      if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      {
         if (log_cb)
            log_cb(RETRO_LOG_ERROR, "Pixel format XRGB8888 not supported by platform.\n");
         ret = false;
      }
   }

   if (!ret || system_color_depth == 16)
   {
      enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;
      if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565))
      {
         log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");
         system_color_depth = 16;
      }
      else
         ret = false;
   }
   return ret;
}


bool retro_load_game(const struct retro_game_info *info)
{
   if (!info)
      return false;

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Opt 1" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Opt 2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Option" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Rotate Screen and D-Pad" },
      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   overscan = false;
   environ_cb(RETRO_ENVIRONMENT_GET_OVERSCAN, &overscan);

   if (!init_pix_format())
      log_cb(RETRO_LOG_ERROR, "Unable to initialize pixel format. Aborting.\n");

   if (!MDFNI_LoadGame((const uint8_t *)info->data, info->size))
      return false;

   surf = (MDFN_Surface*)calloc(1, sizeof(*surf));
   
   if (!surf)
      return false;
   
   surf->width  = FB_WIDTH;
   surf->height = FB_HEIGHT;
   surf->pitch  = FB_WIDTH;
   surf->bpp    = system_color_depth;
   surf->pixels = (uint16_t*)calloc(4, FB_WIDTH * FB_HEIGHT);

   if (!surf->pixels)
   {
      free(surf);
      surf = NULL;
      return false;
   }

   lynxie->DisplaySetAttributes(surf->bpp);

   SetInput(0, "gamepad", (uint8_t*)&input_buf);

   rotate_mode = 0;
   rotate_fixed = 0;
   rotate_screen = 0;
   select_pressed_last_frame = 0;
   rotate_screen_last_frame = 0;

   check_variables();

   return true;
}

static void MDFNI_CloseGame(void)
{
   if(!MDFNGameInfo)
      return;

   MDFN_FlushGameCheats();

   CloseGame();

   MDFNMP_Kill();

   MDFNGameInfo = NULL;
}


void retro_unload_game(void)
{
   MDFNI_CloseGame();
}

static void update_input(void)
{
   static unsigned map[4][9] = {
      {
         RETRO_DEVICE_ID_JOYPAD_A,
         RETRO_DEVICE_ID_JOYPAD_B,
         RETRO_DEVICE_ID_JOYPAD_L,
         RETRO_DEVICE_ID_JOYPAD_R,
         RETRO_DEVICE_ID_JOYPAD_LEFT,
         RETRO_DEVICE_ID_JOYPAD_RIGHT,
         RETRO_DEVICE_ID_JOYPAD_UP,
         RETRO_DEVICE_ID_JOYPAD_DOWN,
         RETRO_DEVICE_ID_JOYPAD_START
      },
      {
         RETRO_DEVICE_ID_JOYPAD_A,
         RETRO_DEVICE_ID_JOYPAD_B,
         RETRO_DEVICE_ID_JOYPAD_L,
         RETRO_DEVICE_ID_JOYPAD_R,
         RETRO_DEVICE_ID_JOYPAD_DOWN,
         RETRO_DEVICE_ID_JOYPAD_UP,
         RETRO_DEVICE_ID_JOYPAD_LEFT,
         RETRO_DEVICE_ID_JOYPAD_RIGHT,
         RETRO_DEVICE_ID_JOYPAD_START
      },
      {
         RETRO_DEVICE_ID_JOYPAD_A,
         RETRO_DEVICE_ID_JOYPAD_B,
         RETRO_DEVICE_ID_JOYPAD_L,
         RETRO_DEVICE_ID_JOYPAD_R,
         RETRO_DEVICE_ID_JOYPAD_RIGHT,
         RETRO_DEVICE_ID_JOYPAD_LEFT,
         RETRO_DEVICE_ID_JOYPAD_DOWN,
         RETRO_DEVICE_ID_JOYPAD_UP,
         RETRO_DEVICE_ID_JOYPAD_START
      },
      {
         RETRO_DEVICE_ID_JOYPAD_A,
         RETRO_DEVICE_ID_JOYPAD_B,
         RETRO_DEVICE_ID_JOYPAD_L,
         RETRO_DEVICE_ID_JOYPAD_R,
         RETRO_DEVICE_ID_JOYPAD_UP,
         RETRO_DEVICE_ID_JOYPAD_DOWN,
         RETRO_DEVICE_ID_JOYPAD_RIGHT,
         RETRO_DEVICE_ID_JOYPAD_LEFT,
         RETRO_DEVICE_ID_JOYPAD_START
      },
   };

   unsigned select_button = 0;
   uint16_t input_state = 0;

   if (libretro_supports_input_bitmasks)
   {
      int16_t ret = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
      for (unsigned i = 0; i < MAX_BUTTONS; i++)
         input_state |= ret & (1 << map[rotate_screen][i]) ? (1 << i) : 0;
      select_button = ret & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
   }
   else
   {
      for (unsigned i = 0; i < MAX_BUTTONS; i++)
         input_state |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, map[rotate_screen][i]) ? (1 << i) : 0;
      select_button = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
   }

   // Input data must be little endian.
   input_buf[0] = (input_state >> 0) & 0xff;
   input_buf[1] = (input_state >> 8) & 0xff;

   switch (rotate_mode)
   {
   case 1: /* auto rotation */
      switch (lynxie->CartGetRotate())
      {
      case CART_ROTATE_RIGHT: rotate_screen = 3; break;
      case CART_ROTATE_LEFT:  rotate_screen = 1;  break;
      default: rotate_screen = 0; break;
      }
      break;
   case 2: /* manual rotation */
      if (select_button && !select_pressed_last_frame)
         rotate_screen++;
      break;
   case 0: /* fixed rotation */
      rotate_screen = rotate_fixed;
      break;
   default: /* just set to normal */
      rotate_screen = 0;
      break;
   }

   if (rotate_screen != rotate_screen_last_frame)
   {
      if (rotate_screen > 3)
         rotate_screen = 0;
      rotate_screen_last_frame = rotate_screen;

      const float aspect[2] = { (80.0 / 51.0), (51.0 / 80.0) };
      const unsigned rot_angle[4] = { 0, 1, 2, 3 };
      struct retro_game_geometry new_geom = { FB_WIDTH, FB_HEIGHT, FB_WIDTH, FB_HEIGHT, aspect[rotate_screen & 1] };
      environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, (void*)&new_geom);
      environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, (void*)&rot_angle[rotate_screen]);
   }

   select_pressed_last_frame = select_button;
   rotate_screen_last_frame  = rotate_screen;
}

void retro_run(void)
{
   static int16_t sound_buf[0x10000];
   static double last_sound_rate;
   EmulateSpecStruct spec = {0};
   bool updated           = false;

   input_poll_cb();
   update_input();

   spec.surface            = surf;
   spec.SoundRate          = 44100;
   spec.SoundBuf           = sound_buf;
   spec.SoundBufMaxSize    = sizeof(sound_buf) / 2;
   spec.SoundBufSize       = 0;
   spec.SoundFormatChanged = false;

   if (spec.SoundRate != last_sound_rate)
   {
      spec.SoundFormatChanged = true;
      last_sound_rate         = spec.SoundRate;
   }

   Emulate(&spec);

   unsigned width  = spec.DisplayRect.w;
   unsigned height = spec.DisplayRect.h;
   unsigned pitch  = FB_WIDTH << (system_color_depth >> 4);

   video_cb(surf->pixels, width, height, pitch);

   audio_batch_cb(spec.SoundBuf, spec.SoundBufSize);

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,
			   &updated) && updated)
      check_variables();
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = MEDNAFEN_CORE_NAME;
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   info->library_version  = MEDNAFEN_CORE_VERSION GIT_VERSION;
   info->need_fullpath    = false;
   info->valid_extensions = MEDNAFEN_CORE_EXTENSIONS;
   info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   if (!force_60hz)
      info->timing.fps            = MEDNAFEN_CORE_TIMING_FPS;
   else
      info->timing.fps            = 60.0;
   info->timing.sample_rate    = 44100.0;
   info->geometry.base_width   = MEDNAFEN_CORE_GEOMETRY_BASE_W;
   info->geometry.base_height  = MEDNAFEN_CORE_GEOMETRY_BASE_H;
   info->geometry.max_width    = MEDNAFEN_CORE_GEOMETRY_MAX_W;
   info->geometry.max_height   = MEDNAFEN_CORE_GEOMETRY_MAX_H;
   info->geometry.aspect_ratio = MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO;
}

void retro_deinit(void)
{
   if (surf)
   {
      if (surf->pixels)
         free(surf->pixels);
      surf->pixels = NULL;

      free (surf);
      surf = NULL;
   }

   libretro_supports_input_bitmasks = false;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC; // FIXME: Regions for other cores.
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned in_port, unsigned device) {}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_vfs_interface_info vfs_iface_info = {
      2,
      NULL
   };

   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
      filestream_vfs_init(&vfs_iface_info);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

size_t retro_serialize_size(void)
{
   StateMem st;

   st.data           = NULL;
   st.loc            = 0;
   st.len            = 0;
   st.malloced       = 0;
   st.initial_malloc = 0;

   if (!MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL))
      return 0;

   free(st.data);

   return st.len;
}

bool retro_serialize(void *data, size_t size)
{
   StateMem st;
   bool ret          = false;
   uint8_t *_dat     = (uint8_t*)malloc(size);

   if (!_dat)
      return false;

   /* Mednafen can realloc the buffer so we need to ensure this is safe. */
   st.data           = _dat;
   st.loc            = 0;
   st.len            = 0;
   st.malloced       = size;
   st.initial_malloc = 0;

   ret = MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL);

   memcpy(data, st.data, size);
   free(st.data);

   return ret;
}

bool retro_unserialize(const void *data, size_t size)
{
   StateMem st;
   memset(&st, 0, sizeof(st));
   st.data = (uint8_t*)data;
   st.len  = size;

   return MDFNSS_LoadSM(&st, 0, 0);
}

void *retro_get_memory_data(unsigned type)
{
   if (lynxie && type == RETRO_MEMORY_SYSTEM_RAM)
      return lynxie->GetRamPointer();
   return NULL;
}

size_t retro_get_memory_size(unsigned type)
{
   if (type == RETRO_MEMORY_SYSTEM_RAM)
      return 1024 * 64;
   return 0;
}

void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned, bool, const char *) { }
