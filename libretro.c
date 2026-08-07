#include <libretro.h>
#include <string.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#include "libretro_core_options.h"
#include "mednafen/git.h"
#include "mednafen/general.h"
#include "mednafen/file.h"
#include "mednafen/mednafen-types.h"
#include "mednafen/mempatcher.h"
#include "mednafen/settings.h"
#include "mednafen/state.h"
#include "mednafen/state_helpers.h"

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

/* Forward declarations */
void MDFN_LoadGameCheats(void);
void MDFN_FlushGameCheats(void);

/* core options */

static int RETRO_PIX_DEPTH   = 15;

static bool persistent_data  = false;

/* ==================================================== */

struct retro_perf_callback perf_cb;
retro_get_cpu_features_t perf_get_cpu_features_cb = NULL;
retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static bool overscan;

static MDFN_Surface *surf;

static bool libretro_supports_bitmasks = false;

static char retro_base_directory[1024];
static char retro_base_name[1024];
static char retro_save_directory[1024];

/*---------------------------------------------------------------------------
 * NEOPOP : Emulator as in Dreamland
 *
 * Copyright (c) 2001-2002 by neopop_uk
 *---------------------------------------------------------------------------
 */

/*---------------------------------------------------------------------------
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version. See also the license.txt file for
 *	additional informations.
 *---------------------------------------------------------------------------
 */

#include "mednafen/ngp/neopop.h"
#include "mednafen/general.h"

#include "mednafen/ngp/TLCS-900h/TLCS900h_interpret.h"
#include "mednafen/ngp/TLCS-900h/TLCS900h_registers.h"
#include "mednafen/ngp/Z80_interface.h"
#include "mednafen/ngp/interrupt.h"
#include "mednafen/ngp/mem.h"
#include "mednafen/ngp/rom.h"
#include "mednafen/ngp/gfx.h"
#include "mednafen/ngp/sound.h"
#include "mednafen/ngp/dma.h"
#include "mednafen/ngp/bios.h"
#include "mednafen/ngp/flash.h"
#include "mednafen/ngp/system.h"

extern uint8 CPUExRAM[16384];

ngpgfx_t *NGPGfx;

COLOURMODE system_colour = COLOURMODE_AUTO;

static uint8 *chee;

static int32 z80_runtime;

extern int32_t ngpc_soundTS;

static void Emulate(EmulateSpecStruct *espec, int16_t *sound_buf)
{
   bool MeowMeow        = false;

   if(espec->VideoFormatChanged)
      ngpgfx_set_pixel_format(NGPGfx, espec->surface->depth);

   storeB(0x6F82, *chee);

   MDFNMP_ApplyPeriodicCheats();

   ngpc_soundTS         = 0;

   do
   {
      int32 timetime = (uint8)TLCS900h_interpret();
      MeowMeow |= updateTimers(espec->surface, timetime);
      z80_runtime += timetime;

      while(z80_runtime > 0)
      {
         int z80rantime = Z80_RunOP();

         if (z80rantime < 0) /* Z80 inactive, so take up all run time! */
         {
            z80_runtime = 0;
            break;
         }

         z80_runtime -= z80rantime << 1;

      }
   }while(!MeowMeow);

   espec->SoundBufSize = MDFNNGPCSOUND_Flush(sound_buf,
         espec->SoundBufMaxSize);
}

void neopop_reset(void)
{
   ngpgfx_power(NGPGfx);
   Z80_reset();
   reset_int();
   reset_timers();

   reset_memory();
   BIOSHLE_Reset();
   reset_registers();	/* TLCS900H registers */
   reset_dma();
}

static void extract_basename(char *buf, const char *path, size_t size)
{
   char *ext        = NULL;
   const char *base = strrchr(path, '/');
   if (!base)
      base = strrchr(path, '\\');
   if (!base)
      base = path;

   if (*base == '\\' || *base == '/')
      base++;

   strncpy(buf, base, size - 1);
   buf[size - 1] = '\0';

   ext = strrchr(buf, '.');
   if (ext)
      *ext = '\0';
}

static struct MDFNFILE *file_open(const char *path)
{
   int64_t size          = 0;
   const char        *ld = NULL;
   struct MDFNFILE *file = (struct MDFNFILE*)calloc(1, sizeof(*file));

   if (!file)
      return NULL;

   if (!filestream_read_file(path, (void**)&file->data, &size))
   {
      free(file);
      return NULL;
   }

   ld         = (const char*)strrchr(path, '.');
   file->size = size;
   file->ext  = strdup(ld ? ld + 1 : "");

   return file;
}

static int file_close(struct MDFNFILE *file)
{
   if (!file)
      return 0;

   if (file->ext)
      free(file->ext);
   file->ext = NULL;

   if (file->data)
      free(file->data);
   file->data = NULL;

   free(file);

   return 1;
}

static int Load(const char *path,
      const uint8_t *data, size_t size)
{
   struct retro_game_info_ext *info_ext = NULL;
   const char *rom_path = NULL;
   const uint8_t *rom_data = NULL;
   size_t rom_size = 0;

   /* Attempt to fetch extended game info */
   if (environ_cb(RETRO_ENVIRONMENT_GET_GAME_INFO_EXT, &info_ext))
   {
      rom_path = info_ext->full_path;
      rom_data = (const uint8_t *)info_ext->data;
      rom_size = info_ext->size;
      persistent_data = info_ext->persistent_data;
      /* Use canonical content name for save files */
      strlcpy(retro_base_name, info_ext->name, sizeof(retro_base_name));
   }
   else
   {
      rom_path = path;
      rom_data = data;
      rom_size = size;
      persistent_data = false;
      if (rom_path)
         extract_basename(retro_base_name, rom_path, sizeof(retro_base_name));
   }

   /* Use existing ROM data if available */
   if (rom_data && rom_size)
   {
      if (persistent_data)
      {
         ngpc_rom.orig_data = (uint8_t*)rom_data;
         ngpc_rom.length    = rom_size;
      }
      else
      {
         if (!(ngpc_rom.orig_data = (uint8 *)malloc(rom_size)))
            return 0;
         ngpc_rom.length = rom_size;
         memcpy(ngpc_rom.orig_data, rom_data, rom_size);
      }
   }
   /* Load ROM data from file */
   else
   {
      struct MDFNFILE *rom_file = NULL;

      if (rom_path)
         rom_file = file_open(rom_path);

      if (!rom_file)
         return 0;

      if (!(ngpc_rom.orig_data = (uint8 *)malloc(rom_file->size)))
      {
         file_close(rom_file);
         return 0;
      }

      ngpc_rom.length = rom_file->size;
      memcpy(ngpc_rom.orig_data, rom_file->data, rom_file->size);
      file_close(rom_file);
   }

   rom_loaded(ngpc_rom.orig_data, ngpc_rom.length);

   MDFNMP_Init(1024, 1024 * 1024 * 16 / 1024);

   NGPGfx = (ngpgfx_t*)calloc(1, sizeof(*NGPGfx));
   NGPGfx->layer_enable = 1 | 2 | 4;

   MDFNNGPCSOUND_Init();

   MDFNMP_AddRAM(16384, 0x4000, CPUExRAM);

   SetFRM(); /* Set up fast read memory mapping */

   bios_install();

   z80_runtime = 0;

   neopop_reset();

   return 1;
}

void StateAction(StateMem *sm, int load, int data_only)
{
   SFORMAT StateRegs[] =
   {
      SFVARN(z80_runtime, "z80_runtime"),
      SFARRAY(CPUExRAM, 16384),
      SFVARN_BOOL(FlashStatusEnable, "FlashStatusEnable"),
      SFEND
   };

   SFORMAT TLCS_StateRegs[] =
   {
      { &pc, (uint32_t)sizeof(pc), MDFNSTATE_RLSB, "PC" },
      { &sr, (uint32_t)sizeof(sr), MDFNSTATE_RLSB, "SR" },
      { &f_dash, (uint32_t)sizeof(f_dash), MDFNSTATE_RLSB, "F_DASH" },
      { gpr, (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPR" },
      { gprBank[0], (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPRB0" },
      { gprBank[1], (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPRB1" },
      { gprBank[2], (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPRB2" },
      { gprBank[3], (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPRB3" },
      { 0, 0, 0, 0 }
   };

   MDFNSS_StateAction(sm, load, data_only, StateRegs, "MAIN", false);
   MDFNSS_StateAction(sm, load, data_only, TLCS_StateRegs, "TLCS", false);
   MDFNNGPCDMA_StateAction(sm, load, data_only);
   MDFNNGPCSOUND_StateAction(sm, load, data_only);
   ngpgfx_StateAction(NGPGfx, sm, load, data_only);
   MDFNNGPCZ80_StateAction(sm, load, data_only);
   int_timer_StateAction(sm, load, data_only);
   BIOSHLE_StateAction(sm, load, data_only);
   FLASH_StateAction(sm, load, data_only);

   if(load)
   {
      RecacheFRM();
      changedSP();
   }
}

static bool update_video = false;

#define MEDNAFEN_CORE_NAME_MODULE "ngp"
#define MEDNAFEN_CORE_NAME "Beetle NeoPop"
/* TODO/FIXME - only thing missing is flash/RTC refactors */
#define MEDNAFEN_CORE_VERSION "v1.29.0.0"
#define MEDNAFEN_CORE_EXTENSIONS "ngp|ngc|ngpc|npc"
#define MEDNAFEN_CORE_TIMING_FPS 60.25
#define MEDNAFEN_CORE_GEOMETRY_BASE_W 160 
#define MEDNAFEN_CORE_GEOMETRY_BASE_H 152
#define MEDNAFEN_CORE_GEOMETRY_MAX_W 160
#define MEDNAFEN_CORE_GEOMETRY_MAX_H 152
#define MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO (20.0 / 19.0)
#define FB_WIDTH 160
#define FB_HEIGHT 152
#define FB_MAX_HEIGHT FB_HEIGHT

static void check_system_specs(void)
{
   unsigned level = 0;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

static void check_color_depth(void)
{
#if defined(FRONTEND_SUPPORTS_RGB565)
      enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;

      if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565))
      {
         if(log_cb) log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of 0RGB1555.\n");

         RETRO_PIX_DEPTH = 16;
      }
#else
      enum retro_pixel_format rgb555 = RETRO_PIXEL_FORMAT_0RGB1555;

      if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb555))
      {
         if(log_cb) log_cb(RETRO_LOG_INFO, "Using default 0RGB1555 pixel format.\n");

         RETRO_PIX_DEPTH = 15;
      }
#endif
}

static void check_variables(void)
{
   struct retro_variable var = {0};

   var.key   = "ngp_language";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      /* user must manually restart core for change to happen */
      if (!strcmp(var.value, "japanese"))
         setting_ngp_language = 0;
      else if (!strcmp(var.value, "english"))
         setting_ngp_language = 1;
   }
}

void retro_init(void)
{
   struct retro_log_callback log;
   char *dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else 
      log_cb = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
      strcpy(retro_base_directory, dir);
   else
   {
      /* TODO: Add proper fallback */
      if (log_cb)
         log_cb(RETRO_LOG_WARN, "System directory is not defined. Fallback on using same dir as ROM for system directory later ...\n");
   }
   
   /* If save directory is defined use it, otherwise use system directory */
   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir)
      strcpy(retro_save_directory, dir);
   else
   {
      /* TODO: Add proper fallback */
      if (log_cb)
         log_cb(RETRO_LOG_WARN, "Save directory is not defined. Fallback on using SYSTEM directory ...\n");
      strcpy(retro_save_directory, retro_base_directory);
   }      

   perf_get_cpu_features_cb = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb))
      perf_get_cpu_features_cb = perf_cb.get_cpu_features;

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;

   check_system_specs();
}

void retro_reset(void)
{
   neopop_reset();
}

bool retro_load_game_special(unsigned a, const struct retro_game_info *b, size_t c)
{
   return false;
}

#define MAX_PLAYERS 1
#define MAX_BUTTONS 7
static uint8_t input_buf;

bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Option" },

      { 0 },
   };

   if (!info)
      return false;

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   overscan = false;
   environ_cb(RETRO_ENVIRONMENT_GET_OVERSCAN, &overscan);

   check_variables();
   check_color_depth();

   if (Load(info->path, (const uint8_t*)info->data, info->size) <= 0)
      return false;

   MDFN_LoadGameCheats();
   MDFNMP_InstallReadPatches();

   surf = (MDFN_Surface*)calloc(1, sizeof(*surf));

   if (!surf)
      return false;

   surf->width  = FB_WIDTH;
   surf->height = FB_HEIGHT;
   surf->pitch  = FB_WIDTH;
   surf->depth  = RETRO_PIX_DEPTH;

   surf->pixels = (uint16_t*)calloc(1, FB_WIDTH * FB_HEIGHT * sizeof(uint32_t));

   if (!surf->pixels)
   {
      free(surf);
      return false;
   }

   chee = (uint8 *)&input_buf;

   ngpgfx_set_pixel_format(NGPGfx, RETRO_PIX_DEPTH);
   MDFNNGPC_SetSoundRate();

   update_video = false;

   return true;
}

void retro_unload_game(void)
{
   MDFN_FlushGameCheats();

   rom_unload(persistent_data);
   if (NGPGfx)
      free(NGPGfx);
   NGPGfx = NULL;

   MDFNMP_Kill();

   if (surf)
   {
      if (surf->pixels)
         free(surf->pixels);
      free(surf);
   }
   surf            = NULL;

   persistent_data = false;
}

static void update_input(void)
{
   static unsigned map[] = {
      RETRO_DEVICE_ID_JOYPAD_UP,    /* X Cursor horizontal-layout games */
      RETRO_DEVICE_ID_JOYPAD_DOWN,  /* X Cursor horizontal-layout games */
      RETRO_DEVICE_ID_JOYPAD_LEFT,  /* X Cursor horizontal-layout games */
      RETRO_DEVICE_ID_JOYPAD_RIGHT, /* X Cursor horizontal-layout games */
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_START,
   };
   unsigned i, j;
   int16_t ret = 0;
   input_buf   = 0;

   if (libretro_supports_bitmasks)
      ret = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
   else
   {
      for (j = 0; j < (RETRO_DEVICE_ID_JOYPAD_R3+1); j++)
         if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, j))
            ret |= (1 << j);
   }

   for (i = 0; i < MAX_BUTTONS; i++)
      if ((map[i] != -1u) && (ret & (1 << map[i])))
         input_buf |= (1 << i);
}

void retro_run(void)
{
   int total = 0;
   unsigned width, height;
   static int16_t sound_buf[0x10000];
   EmulateSpecStruct spec;
   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();

   input_poll_cb();

   update_input();

   spec.surface            = surf;
   spec.VideoFormatChanged = update_video;
   spec.DisplayRect.w      = 160;
   spec.DisplayRect.h      = 152;
   spec.SoundBufMaxSize    = sizeof(sound_buf) / 2;
   spec.SoundBufSize       = 0;

   if (update_video)
   {
      struct retro_system_av_info system_av_info;

      if (update_video)
      {
         memset(&system_av_info, 0, sizeof(system_av_info));
         environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
      }

      retro_get_system_av_info(&system_av_info);
      environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);

      surf->depth = RETRO_PIX_DEPTH;

      update_video = false;
   }

   Emulate(&spec, sound_buf);

   width  = spec.DisplayRect.w;
   height = spec.DisplayRect.h;

   video_cb(surf->pixels, width, height, FB_WIDTH * 2);

   for (total = 0; total < spec.SoundBufSize; )
      total += audio_batch_cb(sound_buf + total*2, spec.SoundBufSize - total);

}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = MEDNAFEN_CORE_NAME;
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif

   info->need_fullpath    = true;
   info->library_version  = MEDNAFEN_CORE_VERSION GIT_VERSION;
   info->valid_extensions = MEDNAFEN_CORE_EXTENSIONS;
   info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = MEDNAFEN_CORE_TIMING_FPS;
   info->timing.sample_rate    = 44100;
   info->geometry.base_width   = MEDNAFEN_CORE_GEOMETRY_BASE_W;
   info->geometry.base_height  = MEDNAFEN_CORE_GEOMETRY_BASE_H;
   info->geometry.max_width    = MEDNAFEN_CORE_GEOMETRY_MAX_W;
   info->geometry.max_height   = MEDNAFEN_CORE_GEOMETRY_MAX_H;
   info->geometry.aspect_ratio = MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO;

   check_color_depth();
}

void retro_deinit(void)
{
   if (surf)
   {
      if (surf->pixels)
         free(surf->pixels);
      free(surf);
   }
   surf = NULL;

   libretro_supports_bitmasks = false;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_vfs_interface_info vfs_iface_info;
   static const struct retro_system_content_info_override content_overrides[] = {
      {
         MEDNAFEN_CORE_EXTENSIONS, /* extensions */
#ifdef LOAD_FROM_MEMORY
         false, /* need_fullpath */
         true   /* persistent_data */
#else
         true,  /* need_fullpath */
         false  /* persistent_data */
#endif
      },
      { NULL, false, false }
   };
   environ_cb = cb;

   libretro_set_core_options(environ_cb);

   vfs_iface_info.required_interface_version = 1;
   vfs_iface_info.iface                      = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
	   filestream_vfs_init(&vfs_iface_info);
   /* Request a persistent content data buffer */
   environ_cb(RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE,
         (void*)content_overrides);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
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

   st.data           = (uint8_t*)data;
   st.loc            = 0;
   st.len            = size;
   st.malloced       = 0;
   st.initial_malloc = 0;

   MDFNSS_LoadSM(&st, 0, 0);

   return true;
}

void *retro_get_memory_data(unsigned type)
{
   if(type == RETRO_MEMORY_SYSTEM_RAM)
      return CPUExRAM;
   return NULL;
}

size_t retro_get_memory_size(unsigned type)
{
   if(type == RETRO_MEMORY_SYSTEM_RAM)
      return 16384;
   return 0;
}

void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned a, bool b, const char *c) { }

/* Use a simpler approach to make sure that things go right for libretro. */
void MDFN_MakeFName(uint8_t type, char *s, size_t len,
      int id1, const char *cd1)
{
#ifdef _WIN32
   char slash = '\\';
#else
   char slash = '/';
#endif
   switch (type)
   {
      case MDFNMKF_SAV:
         snprintf(s, len, "%s%c%s%s%s", 
               retro_save_directory, slash, retro_base_name, ".",
               cd1);
	 if (log_cb)
	       log_cb(RETRO_LOG_INFO, "MDFN_MakeFName: %s\n", s);
         break;
      default:	  
         break;
   }
}
