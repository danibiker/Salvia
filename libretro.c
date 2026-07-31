#include <sys/types.h>

#include <libretro.h>
#include <retro_math.h>

#include "libretro_core_options.h"

#include "mednafen/settings.h"
#include "mednafen/git.h"
#include "mednafen/wswan/wswan.h"
#include "mednafen/mempatcher.h"
#include "mednafen/mempatcher-driver.h"
#include "mednafen/wswan/gfx.h"
#include "mednafen/wswan/interrupt.h"
#include "mednafen/wswan/wswan-memory.h"
#include "mednafen/wswan/start.inc"
#include "mednafen/wswan/sound.h"
#include "mednafen/wswan/v30mz.h"
#include "mednafen/wswan/rtc.h"
#include "mednafen/wswan/comm.h"
#include "mednafen/state_inline.h"
#include "mednafen/wswan/eeprom.h"

#if defined(_3DS)
void* linearMemAlign(size_t size, size_t alignment);
void linearFree(void* mem);
#endif

#define MEDNAFEN_CORE_NAME_MODULE "wswan"
#define MEDNAFEN_CORE_NAME "Beetle WonderSwan"
#define MEDNAFEN_CORE_VERSION "v0.9.35.1"
#define MEDNAFEN_CORE_EXTENSIONS "ws|wsc|pc2"
#define MEDNAFEN_CORE_TIMING_FPS (3072000.0 / (159.0 * 256.0))
#define MEDNAFEN_CORE_GEOMETRY_BASE_W 224
#define MEDNAFEN_CORE_GEOMETRY_BASE_H 144
#define MEDNAFEN_CORE_GEOMETRY_MAX_W 224
#define MEDNAFEN_CORE_GEOMETRY_MAX_H 144
#define MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO (14.0 / 9.0)
#define FB_WIDTH 224
#define FB_HEIGHT 144
#define FB_MAX_HEIGHT FB_HEIGHT

/* Forward declarations */
void MDFN_FlushGameCheats(int nosave);

/* core options */
static int RETRO_SAMPLE_RATE = 44100;

static int RETRO_PIX_BYTES = 2;
static int RETRO_PIX_DEPTH = 15;

struct retro_perf_callback perf_cb;
retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static bool libretro_supports_bitmasks = false;

typedef enum
{
   DISPLAY_ROTATION_MANUAL = 0,
   DISPLAY_ROTATION_PORTRAIT,
   DISPLAY_ROTATION_LANDSCAPE
} retro_display_rotation_t;

static retro_display_rotation_t retro_display_rotation = DISPLAY_ROTATION_MANUAL;

static bool rotate_tall               = false;
static bool select_pressed_last_frame = false;
static bool hw_rotate_enabled         = false;

static unsigned rotate_joymap = 0;

static MDFN_Surface *surf   = NULL;
static uint16_t *rotate_buf = NULL;

#define ROTATE_PIXEL_BUF(typename_t, src, width, height, dst)                            \
   {                                                                                     \
      typename_t *in_ptr  = (typename_t*)src;                                            \
      typename_t *out_ptr = (typename_t*)dst;                                            \
      size_t x, y;                                                                       \
      for (x = 0; x < width; x++)                                                        \
         for (y = 0; y < height; y++)                                                    \
            *(out_ptr + y + (((width - 1) - x) * height)) = *(in_ptr + x + (y * width)); \
   }

static int16_t *audio_samples_buf     = NULL;
static int32_t audio_samples_buf_size = 0;

#define RETRO_60HZ_FPS         ((4.0 * MEDNAFEN_CORE_TIMING_FPS) / 5.0)
#define RETRO_60HZ_CYCLE_INDEX 4

typedef struct
{
   int16_t *samples_buf;
   int32_t samples_buf_size;
   int32_t samples_buf_pos;
   int32_t samples_per_frame;
} retro_60hz_audio_t;

static bool retro_60hz_enabled             = false;
static uint16_t retro_60hz_counter         = 0;
static retro_60hz_audio_t retro_60hz_audio = {0};

/* Forward declarations for lrav_reset() */
static int64_t audio_low_pass_acc_left;
static int64_t audio_low_pass_acc_right;

/* Reset the presentation-side A/V state: the low-pass filter's
 * IIR accumulators and the 60Hz pulldown cadence/cache. Called at
 * game load and console reset so a statically linked core cannot
 * carry this state across sessions, and so reset -> N frames is
 * reproducible. */
static void lrav_reset(void)
{
   audio_low_pass_acc_left          = 0;
   audio_low_pass_acc_right         = 0;
   retro_60hz_counter               = 0;
   retro_60hz_audio.samples_buf_pos = 0;
}

static void retro_60hz_deinit(void)
{
   if (retro_60hz_audio.samples_buf)
      free(retro_60hz_audio.samples_buf);

   retro_60hz_audio.samples_buf       = NULL;
   retro_60hz_audio.samples_buf_size  = 0;
   retro_60hz_audio.samples_buf_pos   = 0;
   retro_60hz_audio.samples_per_frame = 0;

   retro_60hz_counter = 0;
}

static void retro_60hz_init(void)
{
   retro_60hz_deinit();

   if (retro_60hz_enabled)
   {
      /* Get expected number of samples per 60Hz
       * frame
       * > Round down - any excess samples will be read
       *   out at the end of each 4/5 frame cycle */
      /* rate / (4/5 * 3072000/(159*256)), floored, in exact
       * integer arithmetic (feeds the audio cadence, so it must
       * be identical on every platform) */
      retro_60hz_audio.samples_per_frame = (int32_t)
            (((int64_t)RETRO_SAMPLE_RATE * 159 * 256 * 5) / (3072000LL * 4));

      /* Initial buffer size should be *twice* that of
       * the expected number of samples per 75Hz frame */
      retro_60hz_audio.samples_buf_size  = ((int32_t)
            (((int64_t)RETRO_SAMPLE_RATE * 159 * 256) / 3072000LL) + 1) << 2;
      retro_60hz_audio.samples_buf       = (int16_t*)malloc(
            retro_60hz_audio.samples_buf_size * sizeof(int16_t));

      if (!retro_60hz_audio.samples_buf)
      {
         retro_60hz_deinit();
         retro_60hz_enabled = false;
      }
   }
}

static void retro_60hz_cache_audio_samples(int16_t *samples, int32_t frames)
{
   int32_t buffer_capacity  = retro_60hz_audio.samples_buf_size -
         retro_60hz_audio.samples_buf_pos;
   int32_t samples_to_write = frames << 1;

   /* Resize buffer if necessary */
   if (buffer_capacity < samples_to_write)
   {
      int16_t *tmp_buffer = NULL;
      int32_t tmp_buffer_size;

      tmp_buffer_size = retro_60hz_audio.samples_buf_size +
            (samples_to_write - buffer_capacity);
      tmp_buffer_size = (tmp_buffer_size << 1) - (tmp_buffer_size >> 1);
      tmp_buffer      = (int16_t *)malloc(tmp_buffer_size * sizeof(int16_t));

      memcpy(tmp_buffer, retro_60hz_audio.samples_buf,
            retro_60hz_audio.samples_buf_pos * sizeof(int16_t));

      free(retro_60hz_audio.samples_buf);
      retro_60hz_audio.samples_buf      = tmp_buffer;
      retro_60hz_audio.samples_buf_size = tmp_buffer_size;
   }

   /* Copy samples */
   memcpy(retro_60hz_audio.samples_buf +
               retro_60hz_audio.samples_buf_pos,
         samples, samples_to_write * sizeof(int16_t));
   retro_60hz_audio.samples_buf_pos += samples_to_write;
}

static bool audio_low_pass_enabled      = false;

static void audio_low_pass_apply(int16_t *samples, int32_t frames)
{
   while (frames-- > 0)
   {
      int64_t drop_current_left;
      int64_t drop_current_right;

      /* Left channel */
      drop_current_left        = ((int64_t)*samples * 65536 - audio_low_pass_acc_left) >> 3;
      audio_low_pass_acc_left += drop_current_left;
      *samples++               = (int16_t)((audio_low_pass_acc_left >> 16) & 0xFFFF);

      /* Right channel */
      drop_current_right        = ((int64_t)*samples * 65536 - audio_low_pass_acc_right) >> 3;
      audio_low_pass_acc_right += drop_current_right;
      *samples++                = (int16_t)((audio_low_pass_acc_right >> 16) & 0xFFFF);
   }
}

/* Mono palettes */

struct ws_mono_palette
{
   const char *name;
   uint32 start;
   uint32 end;
};

static struct ws_mono_palette ws_mono_palettes[] = {
   { "default",                0x000000, 0xFFFFFF },
   { "wonderswan",             0x3E3D20, 0x9B9D66 },
   { "wondeswan_color",        0x1B201E, 0xD7D49D },
   { "swancrystal",            0x151108, 0xFFFCCA },
   { "gb_dmg",                 0x00420C, 0x578200 },
   { "gb_pocket",              0x2A3325, 0xA7B19A },
   { "gb_light",               0x00778D, 0x01CBDF },
   { "blossom_pink",           0x180F0F, 0xF09898 },
   { "bubbles_blue",           0x0D1418, 0x88D0F0 },
   { "buttercup_green",        0x12160D, 0xB8E088 },
   { "digivice",               0x000000, 0x8C8C73 },
   { "game_com",               0x000000, 0xA7BF6B },
   { "gameking",               0x184221, 0x8CCE94 },
   { "game_master",            0x2D2D2B, 0x829FA6 },
   { "golden_wild",            0x120F0A, 0xB99F65 },
   { "greenscale",             0x0C360C, 0x9CBE0C },
   { "hokage_orange",          0x170D08, 0xEA8352 },
   { "labo_fawn",              0x15110B, 0xD7AA73 },
   { "legendary_super_saiyan", 0x101509, 0xA5DB5A },
   { "microvision",            0x303030, 0xA0A0A0 },
   { "million_live_gold",      0x141109, 0xCDB261 },
   { "odyssey_gold",           0x131000, 0xC2A000 },
   { "shiny_sky_blue",         0x0E1216, 0x8CB6DF },
   { "slime_blue",             0x040E14, 0x2F8CCC },
   { "ti_83",                  0x181810, 0x9CA684 },
   { "travel_wood",            0x482810, 0xF8D8B0 },
   { "virtual_boy",            0x000000, 0xE30000 },
   { NULL, 0, 0 },
};

static uint32 mono_pal_start = 0x000000;
static uint32 mono_pal_end   = 0xFFFFFF;

static bool find_mono_palette(const char* name,
      uint32 *start, uint32 *end)
{
   bool palette_found = false;
   unsigned i;

   for (i = 0; ws_mono_palettes[i].name; i++)
   {
      if (!strcmp(ws_mono_palettes[i].name, name))
      {
         *start        = ws_mono_palettes[i].start;
         *end          = ws_mono_palettes[i].end;
         palette_found = true;
         break;
      }
   }

   if (!palette_found)
   {
      *start = 0x000000;
      *end   = 0xFFFFFF;
   }

   return palette_found;
}

/* Frameskipping */

static unsigned frameskip_type             = 0;
static unsigned frameskip_threshold        = 0;
static uint16_t frameskip_counter          = 0;

static bool retro_audio_buff_active        = false;
static unsigned retro_audio_buff_occupancy = 0;
static bool retro_audio_buff_underrun      = false;
/* Maximum number of consecutive frames that
 * can be skipped */
#define FRAMESKIP_MAX 30

static unsigned audio_latency              = 0;
static bool update_audio_latency           = false;

static void retro_audio_buff_status_cb(
      bool active, unsigned occupancy, bool underrun_likely)
{
   retro_audio_buff_active    = active;
   retro_audio_buff_occupancy = occupancy;
   retro_audio_buff_underrun  = underrun_likely;
}

static void init_frameskip(void)
{
   if (frameskip_type > 0)
   {
      struct retro_audio_buffer_status_callback buf_status_cb;

      buf_status_cb.callback = retro_audio_buff_status_cb;
      if (!environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK,
            &buf_status_cb))
      {
         if (log_cb)
            log_cb(RETRO_LOG_WARN, "Frameskip disabled - frontend does not support audio buffer status monitoring.\n");

         retro_audio_buff_active    = false;
         retro_audio_buff_occupancy = 0;
         retro_audio_buff_underrun  = false;
         audio_latency              = 0;
      }
      else
      {
         float fps;
         float latency_factor;
         float frame_time_msec;

         if (retro_60hz_enabled)
         {
            fps            = (float)RETRO_60HZ_FPS;
            latency_factor = 6.0f;
         }
         else
         {
            fps            = (float)MEDNAFEN_CORE_TIMING_FPS;
            latency_factor = 8.0f;
         }

         /* Frameskip is enabled - increase frontend
          * audio latency to minimise potential
          * buffer underruns */
         frame_time_msec = 1000.0f / fps;

         /* Set latency to an integer multiple of
          * the current frame time...
          * > At 60Hz we normally use a 6x multiplier
          * > The native (unusually high) 75Hz of the
          *   WonderSwan seems to place greater demands
          *   on the frontend, so we increase the
          *   multiplier to 8x; this gives the frontend
          *   more room to manoeuvre, and improves the
          *   efficacy of the 'Auto' frameskip setting */
         audio_latency = (unsigned)((latency_factor * frame_time_msec) + 0.5f);

         /* ...then round up to nearest multiple of 32 */
         audio_latency = (audio_latency + 0x1F) & ~0x1F;
      }
   }
   else
   {
      environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK, NULL);
      audio_latency = 0;
   }

   update_audio_latency = true;
}

/* Cygne
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Dox dox@space.pl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

int 		wsc = 1;			/*color/mono*/
uint32		rom_size;

uint16 WSButtonStatus;

static void Reset(void)
{
   int u0;

   v30mz_reset();				/* Reset CPU */
   WSwan_MemoryReset();
   Comm_Reset();
   WSwan_GfxReset();
   WSwan_SoundReset();
   WSwan_InterruptReset();
   WSwan_RTCReset();
   WSwan_EEPROMReset();

   for(u0=0;u0<0xc8;u0++)
   {
      if(u0 != 0xC4 && u0 != 0xC5 && u0 != 0xBA && u0 != 0xBB)
         WSwan_writeport(u0,startio[u0]);
   }

   v30mz_set_reg(NEC_SS,0);
   v30mz_set_reg(NEC_SP,0x2000);
}

static uint8 *chee = NULL;

static void Emulate(EmulateSpecStruct *espec,
      int skip_frame, int update_sample_rate)
{
   espec->surface          = surf;
   espec->DisplayRect.w    = 224;
   espec->DisplayRect.h    = 144;
   espec->skip             = skip_frame;
   espec->SoundBufSize     = 0;

   if (update_sample_rate)
      WSwan_SetSoundRate(RETRO_SAMPLE_RATE);

   WSButtonStatus          = chee[0] | (chee[1] << 8);

   MDFNMP_ApplyPeriodicCheats();

   while (!wsExecuteLine(espec->surface, espec->skip));

   espec->SoundBufSize = WSwan_SoundFlush(&audio_samples_buf,
         &audio_samples_buf_size);

   if (audio_low_pass_enabled)
      audio_low_pass_apply(audio_samples_buf,
            espec->SoundBufSize);

   v30mz_timestamp = 0;
}

#if 0
typedef struct
{
 const uint8 id;
 const char *name;
} DLEntry;

static const DLEntry Developers[] =
{
 { 0x01, "Bandai" },
 { 0x02, "Taito" },
 { 0x03, "Tomy" },
 { 0x04, "Koei" },
 { 0x05, "Data East" },
 { 0x06, "Asmik" }, /* Asmik Ace? */
 { 0x07, "Media Entertainment" },
 { 0x08, "Nichibutsu" },
 { 0x0A, "Coconuts Japan" },
 { 0x0B, "Sammy" },
 { 0x0C, "Sunsoft" },
 { 0x0D, "Mebius" },
 { 0x0E, "Banpresto" },
 { 0x10, "Jaleco" },
 { 0x11, "Imagineer" },
 { 0x12, "Konami" },
 { 0x16, "Kobunsha" },
 { 0x17, "Bottom Up" },
 { 0x18, "Kaga Tech" },
 { 0x19, "Sunrise" },
 { 0x1A, "Cyber Front" },
 { 0x1B, "Mega House" },
 { 0x1D, "Interbec" },
 { 0x1E, "Nihon Application" },
 { 0x1F, "Bandai Visual" },
 { 0x20, "Athena" },
 { 0x21, "KID" },
 { 0x22, "HAL Corporation" },
 { 0x23, "Yuki Enterprise" },
 { 0x24, "Omega Micott" },
 { 0x25, "Layup" },
 { 0x26, "Kadokawa Shoten" },
 { 0x27, "Shall Luck" },
 { 0x28, "Squaresoft" },
 { 0x2B, "Tom Create" },
 { 0x2D, "Namco" },
 { 0x2E, "Movic" }, /* ???? */
 { 0x2F, "E3 Staff" }, /* ???? */
 { 0x31, "Vanguard" },
 { 0x32, "Megatron" },
 { 0x33, "Wiz" },
 { 0x36, "Capcom" }
};
#endif

static uint32 SRAMSize;

static bool wswan_is_ww = false;

/* Standard CRC-32 (IEEE 802.3), as used by upstream for the
 * WonderWitch firmware fingerprint. */
static uint32 lr_crc32(uint32 crc, const uint8 *data, size_t len)
{
   static uint32 tab[256];
   static bool tab_init = false;
   size_t i;

   if(!tab_init)
   {
      uint32 c;
      unsigned n, k;

      for(n = 0; n < 256; n++)
      {
         c = (uint32)n;
         for(k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
         tab[n] = c;
      }
      tab_init = true;
   }

   crc = ~crc;
   for(i = 0; i < len; i++)
      crc = tab[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
   return ~crc;
}

static int Load(const uint8_t *data, size_t size)
{
   uint32 pow_size      = 0;
   uint32 real_rom_size = 0;
   uint8 header[10];

   if(size < 65536)
      return 0;

   real_rom_size = (size + 0xFFFF) & ~0xFFFF;
   pow_size      = next_pow2(real_rom_size);
   rom_size      = pow_size + (pow_size == 0);

   wsCartROM     = (uint8 *)calloc(1, rom_size);

   /* This real_rom_size vs rom_size funny business 
    * is intended primarily for handling
    * WSR files. */
   if(real_rom_size < rom_size)
      memset(wsCartROM, 0xFF, rom_size - real_rom_size);

   memcpy(wsCartROM + (rom_size - real_rom_size), data, size);

   memcpy(header, wsCartROM + rom_size - 10, 10);

   /* WonderWitch firmware detection, as upstream: 512KB image,
    * "ELISA" font signature, known footer checksum, minus a
    * blocklist of retail images that happen to match. */
   wswan_is_ww = false;
   if(rom_size == 524288 &&
      !memcmp(&wsCartROM[0x70000], "ELISA", 5) &&
      lr_crc32(0, &wsCartROM[0x7FFF0], 0x10) == 0x0d05ed64)
   {
      static const uint32 ww_blocklist[3] =
      {
         0x63f00316, 0x60fd569b, 0xe11538f8
      };
      uint32 crc32_sans_l64k = lr_crc32(0, wsCartROM, 0x70000);
      bool blisted = false;
      unsigned i;

      for(i = 0; i < 3; i++)
      {
         if(crc32_sans_l64k == ww_blocklist[i])
         {
            blisted = true;
            break;
         }
      }

      wswan_is_ww = !blisted;
   }

   SRAMSize = 0;
   eeprom_size = 0;

   switch(header[5])
   {
      case 0x01: SRAMSize =   8 * 1024; break;
      case 0x02: SRAMSize =  32 * 1024; break;
      case 0x03: SRAMSize = 128 * 1024; break;
      case 0x04: SRAMSize = 256 * 1024; break; /* Dicing Knight!, Judgement Silver */
      case 0x05: SRAMSize = 512 * 1024; break; /* Wonder Gate */

      case 0x10: eeprom_size = 128; break;
      case 0x20: eeprom_size = 2 *1024; break;
      case 0x50: eeprom_size = 1024; break;
   }

   if((header[8] | (header[9] << 8)) == 0x8de1 && (header[0]==0x01)&&(header[2]==0x27)) /* Detective Conan */
   {
      /* WS cpu is using cache/pipeline or there's protected ROM bank where pointing CS */
      wsCartROM[0xfffe8]=0xea;
      wsCartROM[0xfffe9]=0x00;
      wsCartROM[0xfffea]=0x00;
      wsCartROM[0xfffeb]=0x00;
      wsCartROM[0xfffec]=0x20;
   }

#if 0
   if(header[6] & 0x1)
      EmulatedWSwan.rotated = MDFN_ROTATE90;
#endif

   MDFNMP_Init(16384, (1 << 20) / 1024);

   if(wswan_is_ww)
      v30mz_init(WSwan_readmem20_WW, WSwan_writemem20_WW, WSwan_readport_WW, WSwan_writeport_WW);
   else
      v30mz_init(WSwan_readmem20, WSwan_writemem20, WSwan_readport, WSwan_writeport);
   WSwan_MemoryInit(MDFN_GetSettingB("wswan.language"), wsc, SRAMSize, wswan_is_ww); /* EEPROM and SRAM are loaded in this func. */
   WSwan_GfxInit();

   WSwan_SoundInit();

   WSwan_RTCInit();

   lrav_reset();

   wsMakeTiles();

   Reset();

   return 1;
}

static void CloseGame(void)
{
   WSwan_MemoryKill();

   WSwan_SoundKill();

   if(wsCartROM)
   {
      free(wsCartROM);
      wsCartROM = NULL;
   }
}

int StateAction(StateMem *sm, int load, int data_only)
{
   if(!v30mz_StateAction(sm, load, data_only))
      return 0;
   /* Call MemoryStateAction before others StateActions... */
   if(!WSwan_MemoryStateAction(sm, load, data_only))
      return 0;
   if(!Comm_StateAction(sm, load, data_only))
      return 0;
   if(!WSwan_GfxStateAction(sm, load, data_only))
      return 0;
   if(!WSwan_RTCStateAction(sm, load, data_only))
      return 0;
   if(!WSwan_InterruptStateAction(sm, load, data_only))
      return 0;
   if(!WSwan_SoundStateAction(sm, load, data_only))
      return 0;
   if(!WSwan_EEPROMStateAction(sm, load, data_only))
      return 0;

   /* Presentation-chain state that affects reproducibility:
    * the audio low-pass filter's IIR accumulators, and the 60Hz
    * pulldown cadence plus its cached-audio backlog. The 60Hz
    * counter schedules an extra emulated frame every fifth
    * retro_run, so leaving it out desyncs runahead and netplay
    * resimulation whenever 60Hz mode is enabled; the accumulators
    * and cache keep audio sample-exact across a state load.
    * Section is optional: states from older cores simply reset
    * this block. */
   {
      static int16_t lrav_60hz_stage[8192];
      static int32_t lrav_60hz_pos;

      SFORMAT LRAVRegs[] =
      {
         SFVARN(audio_low_pass_acc_left, "LPAccL"),
         SFVARN(audio_low_pass_acc_right, "LPAccR"),
         SFVARN(retro_60hz_counter, "60HzCounter"),
         SFVARN(lrav_60hz_pos, "60HzCachePos"),
         SFARRAY16N(lrav_60hz_stage, 8192, "60HzCache"),
         { 0, 0, 0, 0 }
      };

      if(!load)
      {
         lrav_60hz_pos = retro_60hz_audio.samples_buf_pos;
         if(lrav_60hz_pos > 8192)
            lrav_60hz_pos = 8192;
         if(lrav_60hz_pos > 0)
            memcpy(lrav_60hz_stage, retro_60hz_audio.samples_buf,
                  lrav_60hz_pos * sizeof(int16_t));
      }
      else
      {
         /* Defaults if the section is absent from the state */
         audio_low_pass_acc_left  = 0;
         audio_low_pass_acc_right = 0;
         retro_60hz_counter       = 0;
         lrav_60hz_pos            = 0;
      }

      if(!MDFNSS_StateAction(sm, load, data_only, LRAVRegs, "LRAV", true))
         return 0;

      if(load)
      {
         if(lrav_60hz_pos < 0 || lrav_60hz_pos > 8192)
            lrav_60hz_pos = 0;
         lrav_60hz_pos &= ~1;
         retro_60hz_counter %= (RETRO_60HZ_CYCLE_INDEX + 1);

         if(lrav_60hz_pos > 0 &&
            retro_60hz_audio.samples_buf_size < lrav_60hz_pos)
         {
            int16_t *new_buf = (int16_t*)realloc(retro_60hz_audio.samples_buf,
                  lrav_60hz_pos * sizeof(int16_t));
            if (new_buf)
            {
               retro_60hz_audio.samples_buf      = new_buf;
               retro_60hz_audio.samples_buf_size = lrav_60hz_pos;
            }
            else
               lrav_60hz_pos = 0;
         }
         if(lrav_60hz_pos > 0)
            memcpy(retro_60hz_audio.samples_buf, lrav_60hz_stage,
                  lrav_60hz_pos * sizeof(int16_t));
         retro_60hz_audio.samples_buf_pos = lrav_60hz_pos;
      }
   }

   return 1;
}

static void DoSimpleCommand(int cmd)
{
   switch(cmd)
   {
      case MDFN_MSC_POWER:
      case MDFN_MSC_RESET:
         lrav_reset();
         Reset();
         break;
   }
}

static bool update_audio = false;
static bool update_video = false;

static bool MDFNI_LoadGame(
      const char *force_module, const uint8_t *data,
      size_t size)
{
   if(Load(data, size) <= 0)
      return false;


   return true;
}

static void MDFNI_CloseGame(void)
{
   MDFN_FlushGameCheats(0);

   CloseGame();

   MDFNMP_Kill();
}

static void check_system_specs(void)
{
   /* TODO - should theoretically be level 4, but apparently
    * doesn't run at fullspeed on PSP (yet) */
   unsigned level = 4;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

static void check_depth(void)
{
   if (RETRO_PIX_DEPTH == 24)
   {
      enum retro_pixel_format rgb888 = RETRO_PIXEL_FORMAT_XRGB8888;

      if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb888))
      {
         if (log_cb) log_cb(RETRO_LOG_ERROR, "Pixel format XRGB8888 not supported by platform.\n");

         RETRO_PIX_BYTES = 2;
         RETRO_PIX_DEPTH = 15;
      }
   }

#if defined(FRONTEND_SUPPORTS_RGB565)
   if (RETRO_PIX_BYTES == 2)
   {
      enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;

      if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565))
      {
         if (log_cb) log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");

         RETRO_PIX_DEPTH = 16;
      }
   }
#endif
}

static void rotate_display(void)
{
   if (hw_rotate_enabled)
   {
      struct retro_game_geometry new_geom = {
            FB_WIDTH,
            FB_HEIGHT,
            FB_WIDTH,
            FB_HEIGHT,
            rotate_tall ? (9.0 / 14.0) : (14.0 / 9.0)
      };
      unsigned rot_angle = rotate_tall ?
            1 : /* 90 degrees */
            0;  /* 0 degrees  */

      environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, (void*)&new_geom);
      environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, (void*)&rot_angle);
   }
   else
   {
      struct retro_game_geometry new_geom = {
            rotate_tall ? FB_HEIGHT : FB_WIDTH,
            rotate_tall ? FB_WIDTH  : FB_HEIGHT,
            FB_WIDTH,
            FB_WIDTH,
            rotate_tall ? (9.0 / 14.0) : (14.0 / 9.0)
      };

      environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, (void*)&new_geom);
   }
}

static void check_variables(int startup)
{
   struct retro_variable var = {0};
   uint32 prev_mono_pal_start;
   uint32 prev_mono_pal_end;
   unsigned prev_frameskip_type;
   bool update_60hz_mode = false;

   /* Owner profile options are consumed once, at content load,
    * when WSwan_EEPROMInit() writes the internal EEPROM. */
   if (startup)
   {
      char frontend_name[17];

      var.key   = "wswan_language";
      var.value = NULL;
      setting_wswan_language = 1;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
         if (!strcmp(var.value, "japanese"))
            setting_wswan_language = 0;

      var.key   = "wswan_birth_year";
      var.value = NULL;
      setting_wswan_byear = 1989;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         uint32 v = (uint32)atoi(var.value);
         if (v >= 1920 && v <= 2029)
            setting_wswan_byear = v;
      }

      var.key   = "wswan_birth_month";
      var.value = NULL;
      setting_wswan_bmonth = 6;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         uint32 v = (uint32)atoi(var.value);
         if (v >= 1 && v <= 12)
            setting_wswan_bmonth = v;
      }

      var.key   = "wswan_birth_day";
      var.value = NULL;
      setting_wswan_bday = 23;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         uint32 v = (uint32)atoi(var.value);
         if (v >= 1 && v <= 31)
            setting_wswan_bday = v;
      }

      var.key   = "wswan_sex";
      var.value = NULL;
      setting_wswan_sex = 2;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
         if (!strcmp(var.value, "male"))
            setting_wswan_sex = 1;

      var.key   = "wswan_blood";
      var.value = NULL;
      setting_wswan_blood = 3;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         if (!strcmp(var.value, "a"))
            setting_wswan_blood = 1;
         else if (!strcmp(var.value, "b"))
            setting_wswan_blood = 2;
         else if (!strcmp(var.value, "ab"))
            setting_wswan_blood = 4;
      }

      strcpy(setting_wswan_name, "Mednafen");
      var.key   = "wswan_name_source";
      var.value = NULL;
      if (!(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value &&
            !strcmp(var.value, "default")))
      {
         const char *username = NULL;
         if (environ_cb(RETRO_ENVIRONMENT_GET_USERNAME, &username) &&
             username && username[0])
         {
            size_t i;
            /* Internal EEPROM name is 16 chars; EEPROMInit maps
             * the charset (A-Z, a-z, 0-9, space) itself. */
            for (i = 0; i < 16 && username[i]; i++)
               frontend_name[i] = username[i];
            frontend_name[i] = '\0';
            strcpy(setting_wswan_name, frontend_name);
         }
      }
   }

   var.key = "wswan_rotate_display",
   var.value = NULL;

   retro_display_rotation = DISPLAY_ROTATION_MANUAL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "portrait"))
         retro_display_rotation = DISPLAY_ROTATION_PORTRAIT;
      else if (!strcmp(var.value, "landscape"))
         retro_display_rotation = DISPLAY_ROTATION_LANDSCAPE;
   }

   switch (retro_display_rotation)
   {
      case DISPLAY_ROTATION_PORTRAIT:
         if (!rotate_tall)
         {
            rotate_tall = true;
            rotate_display();
         }
         break;
      case DISPLAY_ROTATION_LANDSCAPE:
         if (rotate_tall)
         {
            rotate_tall = false;
            rotate_display();
         }
         break;
      default:
         break;
   }

   var.key = "wswan_rotate_keymap",
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         rotate_joymap = 0;
      else if (!strcmp(var.value, "enabled"))
         rotate_joymap = 1;
      else if (!strcmp(var.value, "auto"))
         rotate_joymap = 2;
   }

   var.key = "wswan_mono_palette",
   var.value = NULL;

   prev_mono_pal_start = mono_pal_start;
   prev_mono_pal_end   = mono_pal_end;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      find_mono_palette(var.value, &mono_pal_start, &mono_pal_end);

   if ((mono_pal_start != prev_mono_pal_start) ||
       (mono_pal_end   != prev_mono_pal_end))
      WSwan_SetMonoPalette(RETRO_PIX_DEPTH, mono_pal_start, mono_pal_end);

   var.key = "wswan_frameskip";
   var.value = NULL;

   prev_frameskip_type = frameskip_type;
   frameskip_type      = 0;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "auto") == 0)
         frameskip_type = 1;
      else if (strcmp(var.value, "manual") == 0)
         frameskip_type = 2;
   }

   var.key = "wswan_frameskip_threshold";
   var.value = NULL;

   frameskip_threshold = 33;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      frameskip_threshold = strtol(var.value, NULL, 10);

   var.key = "wswan_60hz_mode";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      bool old_value = retro_60hz_enabled;

      if (!strcmp(var.value, "disabled"))
         retro_60hz_enabled = false;
      else if (!strcmp(var.value, "enabled"))
         retro_60hz_enabled = true;

      if (!startup && (old_value != retro_60hz_enabled))
      {
         update_video = true;
         update_60hz_mode = true;
      }
   }

   var.key = "wswan_sound_sample_rate";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int old_value = RETRO_SAMPLE_RATE;

      RETRO_SAMPLE_RATE = atoi(var.value);
      RETRO_SAMPLE_RATE = (RETRO_SAMPLE_RATE < 11025) ?
            11025 : RETRO_SAMPLE_RATE;
      RETRO_SAMPLE_RATE = (RETRO_SAMPLE_RATE > 48000) ?
            48000 : RETRO_SAMPLE_RATE;

      if (!startup && (old_value != RETRO_SAMPLE_RATE))
      {
         update_audio = true;
         /* If audio sample rate changes, must reinitialise
          * 60Hz mode (will be a no-op if 60Hz mode is
          * currently inactive) */
         update_60hz_mode = true;
      }
   }

   var.key = "wswan_sound_low_pass";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         audio_low_pass_enabled = false;
      else if (!strcmp(var.value, "enabled"))
         audio_low_pass_enabled = true;
   }

   if (startup)
   {
      var.key = "wswan_gfx_colors";
      var.value = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         if (!strcmp(var.value, "16bit"))
         {
            RETRO_PIX_BYTES = 2;
            RETRO_PIX_DEPTH = 16;
         }
         else if (!strcmp(var.value, "24bit"))
         {
            RETRO_PIX_BYTES = 4;
            RETRO_PIX_DEPTH = 24;
         }
      }
   }

   if (update_60hz_mode)
      retro_60hz_init();

   /* (Re)Initialise frameskipping, if required */
   if (startup ||
       (frameskip_type != prev_frameskip_type) ||
       update_60hz_mode)
      init_frameskip();
}

void retro_init(void)
{
   struct retro_log_callback log;
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else 
      log_cb = NULL;

   frameskip_type             = 0;
   frameskip_threshold        = 0;
   frameskip_counter          = 0;
   retro_audio_buff_active    = false;
   retro_audio_buff_occupancy = 0;
   retro_audio_buff_underrun  = false;
   audio_latency              = 0;
   update_audio_latency       = false;
   audio_low_pass_enabled     = false;
   audio_low_pass_acc_left    = 0;
   audio_low_pass_acc_right   = 0;

   check_system_specs();
   check_variables(true);
   check_depth();

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;
}

void retro_reset(void)
{
   DoSimpleCommand(MDFN_MSC_RESET);
}

bool retro_load_game_special(unsigned a, const struct retro_game_info *b, size_t c)
{
   return false;
}

#define MAX_PLAYERS 1
#define MAX_BUTTONS 11
static uint16_t input_buf;

bool retro_load_game(const struct retro_game_info *info)
{
   const unsigned rot_angle = 0;

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "X Cursor Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "X Cursor Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "X Cursor Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "X Cursor Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Y Cursor Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Y Cursor Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Y Cursor Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Y Cursor Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Rotate screen + active D-Pad" },

      { 0 },
   };

   if (!info)
      goto error;

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (!MDFNI_LoadGame(MEDNAFEN_CORE_NAME_MODULE,
         (const uint8_t*)info->data, info->size))
      goto error;

   chee = (uint8 *)&input_buf;
   surf = (MDFN_Surface*)calloc(1, sizeof(*surf));
   
   if (!surf)
      goto error;
   
   surf->width  = FB_WIDTH;
   surf->height = FB_HEIGHT;
   surf->pitch  = FB_WIDTH;
   surf->depth  = RETRO_PIX_DEPTH;

#if defined(_3DS)
   surf->pixels = (uint16_t*)linearMemAlign(FB_WIDTH * FB_HEIGHT * sizeof(uint32_t), 128);
#else
   surf->pixels = (uint16_t*)calloc(1, FB_WIDTH * FB_HEIGHT * sizeof(uint32_t));
#endif

   if (!surf->pixels)
      goto error;

   /* Check whether 'hardware' rotation (via frontend
    * gfx driver) is supported */
   hw_rotate_enabled = environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, (void*)&rot_angle);

   if (!hw_rotate_enabled && !rotate_buf)
   {
#if defined(_3DS)
      rotate_buf = (uint16_t*)linearMemAlign(FB_WIDTH * FB_HEIGHT * sizeof(uint32_t), 128);
#else
      rotate_buf = (uint16_t*)calloc(1, FB_WIDTH * FB_HEIGHT * sizeof(uint32_t));
#endif

      if (!rotate_buf)
         goto error;
   }

   rotate_tall = false;
   select_pressed_last_frame = false;
   rotate_joymap = 0;

   check_variables(false);

   /* Allocate an audio buffer of sufficient size
    * for the expected number of samples per frame
    * (size will be increased automatically if
    * configuration changes) */
   audio_samples_buf_size = ((int32_t)(RETRO_SAMPLE_RATE /
         MEDNAFEN_CORE_TIMING_FPS) + 1) << 1;
   audio_samples_buf      = (int16_t*)malloc(audio_samples_buf_size * sizeof(int16_t));

   if (!audio_samples_buf)
      goto error;

   retro_60hz_init();

   WSwan_SetPixelFormat(RETRO_PIX_DEPTH,
         mono_pal_start, mono_pal_end);
   WSwan_SetSoundRate(RETRO_SAMPLE_RATE);

   return true;

error:
   if (surf)
   {
      if (surf->pixels)
#if defined(_3DS)
         linearFree(surf->pixels);
#else
         free(surf->pixels);
#endif
      free(surf);
   }
   surf = NULL;

   if (rotate_buf)
#if defined(_3DS)
      linearFree(rotate_buf);
#else
      free(rotate_buf);
#endif
   rotate_buf = NULL;

   if (audio_samples_buf)
      free(audio_samples_buf);
   audio_samples_buf      = NULL;
   audio_samples_buf_size = 0;

   return false;
}

void retro_unload_game(void)
{
   MDFNI_CloseGame();

   if (surf)
   {
      if (surf->pixels)
#if defined(_3DS)
         linearFree(surf->pixels);
#else
         free(surf->pixels);
#endif
      free(surf);
   }
   surf = NULL;

   if (rotate_buf)
#if defined(_3DS)
      linearFree(rotate_buf);
#else
      free(rotate_buf);
#endif
   rotate_buf = NULL;

   if (audio_samples_buf)
      free(audio_samples_buf);
   audio_samples_buf      = NULL;
   audio_samples_buf_size = 0;

   retro_60hz_deinit();
}

static void update_input(void)
{
   static unsigned map[2][11] = {
      {
         RETRO_DEVICE_ID_JOYPAD_UP,    /* X Cursor horizontal-layout games */
         RETRO_DEVICE_ID_JOYPAD_RIGHT, /* X Cursor horizontal-layout games */
         RETRO_DEVICE_ID_JOYPAD_DOWN,  /* X Cursor horizontal-layout games */
         RETRO_DEVICE_ID_JOYPAD_LEFT,  /* X Cursor horizontal-layout games */
         RETRO_DEVICE_ID_JOYPAD_R2,    /* Y Cursor UP vertical-layout games */
         RETRO_DEVICE_ID_JOYPAD_R,     /* Y Cursor RIGHT vertical-layout games */
         RETRO_DEVICE_ID_JOYPAD_L2,    /* Y Cursor DOWN vertical-layout games */
         RETRO_DEVICE_ID_JOYPAD_L,     /* Y Cursor LEFT vertical-layout games */
         RETRO_DEVICE_ID_JOYPAD_START,
         RETRO_DEVICE_ID_JOYPAD_A,
         RETRO_DEVICE_ID_JOYPAD_B,
      },
      {
         RETRO_DEVICE_ID_JOYPAD_Y,     /* X Cursor horizontal-layout games */
         RETRO_DEVICE_ID_JOYPAD_X,     /* X Cursor horizontal-layout games */
         RETRO_DEVICE_ID_JOYPAD_A,     /* X Cursor horizontal-layout games */
         RETRO_DEVICE_ID_JOYPAD_B,     /* X Cursor horizontal-layout games */
         RETRO_DEVICE_ID_JOYPAD_LEFT,  /* Y Cursor UP vertical-layout games */
         RETRO_DEVICE_ID_JOYPAD_UP,    /* Y Cursor RIGHT vertical-layout games */
         RETRO_DEVICE_ID_JOYPAD_RIGHT, /* Y Cursor DOWN vertical-layout games */
         RETRO_DEVICE_ID_JOYPAD_DOWN,  /* Y Cursor LEFT vertical-layout games */
         RETRO_DEVICE_ID_JOYPAD_START,
         RETRO_DEVICE_ID_JOYPAD_L,
         RETRO_DEVICE_ID_JOYPAD_R,
      }
   };
   unsigned i;
   int16_t bitmask = 0;
   bool select_button;
   bool joymap;
#ifdef MSB_FIRST
   union {
      uint8_t b[2];
      uint16_t s;
   } u;
#endif

   input_buf = 0;

   if (libretro_supports_bitmasks)
      bitmask = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
   else
   {
      for (i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R3+1); i++)
         bitmask |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) ? 1 << i : 0;
   }

   select_button = bitmask & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
   if ((retro_display_rotation == DISPLAY_ROTATION_MANUAL) &&
       select_button &&
       !select_pressed_last_frame)
   {
      rotate_tall = !rotate_tall;
      rotate_display();
   }
   select_pressed_last_frame = select_button;

   joymap = (rotate_joymap == 2) ? rotate_tall : (rotate_joymap ? true : false);

   for (i = 0; i < MAX_BUTTONS; i++)
      input_buf |= map[joymap][i] != -1u && ((1 << map[joymap][i]) & bitmask) ? (1 << i) : 0;

#ifdef MSB_FIRST
   u.s = input_buf;
   input_buf = u.b[0] | u.b[1] << 8;
#endif
}

void retro_run(void)
{
   int total;
   unsigned width, height;
   int32 SoundBufSize;
   EmulateSpecStruct spec;
   bool updated   = false;
   int skip_frame = 0;
   int update_sample_rate;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables(false);
   update_sample_rate = update_audio;

   input_poll_cb();

   update_input();

   /* Check whether current frame should
    * be skipped */
   if ((frameskip_type > 0) && retro_audio_buff_active)
   {
      switch (frameskip_type)
      {
         case 1: /* auto */
            skip_frame = retro_audio_buff_underrun ? 1 : 0;
            break;
         case 2: /* manual */
            skip_frame = (retro_audio_buff_occupancy < frameskip_threshold) ? 1 : 0;
            break;
         default:
            skip_frame = 0;
            break;
      }

      if (!skip_frame ||
          (frameskip_counter >= FRAMESKIP_MAX))
      {
         skip_frame        = 0;
         frameskip_counter = 0;
      }
      else
         frameskip_counter++;
   }

   /* If frameskip settings have changed, update
    * frontend audio latency */
   if (update_audio_latency)
   {
      environ_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY,
            &audio_latency);
      update_audio_latency = false;
   }

   if (retro_60hz_enabled)
   {
      /* If we are running in 60Hz mode, then:
       * - Audio data must be buffered
       * - On every 4th call of retro_run(), an
       *   extra frame must be emulated */

      /* Emulate 'force skipped frame' */
      if (retro_60hz_counter == 0)
      {
         Emulate(&spec, 1, update_sample_rate);
         update_sample_rate = 0;
         retro_60hz_cache_audio_samples(audio_samples_buf,
               spec.SoundBufSize);
      }

      /* Run 'regular' frame */
      Emulate(&spec, skip_frame, update_sample_rate);
      retro_60hz_cache_audio_samples(audio_samples_buf,
            spec.SoundBufSize);

      retro_60hz_counter++;
   }
   else
      Emulate(&spec, skip_frame, update_sample_rate);

   if (update_audio || update_video)
   {
      struct retro_system_av_info system_av_info;
      retro_get_system_av_info(&system_av_info);
      environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
      update_audio = false;
      update_video = false;
   }

   width  = spec.DisplayRect.w;
   height = spec.DisplayRect.h;

   if (hw_rotate_enabled || !rotate_tall)
   {
      if (!skip_frame)
         video_cb(surf->pixels, width, height, FB_WIDTH * RETRO_PIX_BYTES);
      else
         video_cb(NULL, width, height, FB_WIDTH * RETRO_PIX_BYTES);
   }
   else
   {
      if (!skip_frame)
      {
         /* Perform software-based display rotation */
         if (RETRO_PIX_BYTES == 4)
            ROTATE_PIXEL_BUF(
                  uint32_t, surf->pixels, width, height, rotate_buf)
         else
            ROTATE_PIXEL_BUF(
                  uint16_t, surf->pixels, width, height, rotate_buf)

         video_cb(rotate_buf, height, width, FB_HEIGHT * RETRO_PIX_BYTES);
      }
      else
         video_cb(NULL, height, width, FB_HEIGHT * RETRO_PIX_BYTES);
   }

   if (retro_60hz_enabled)
   {
      /* If we are running in 60Hz mode, then
       * read out the expected number of samples
       * on each frame plus empty the buffer at
       * the end of each 4/5 frame cycle */
      int32_t frames_available = retro_60hz_audio.samples_buf_pos >> 1;
      int32_t frames_to_read   = retro_60hz_audio.samples_per_frame;
      int32_t samples_to_read;

      frames_to_read = (frames_to_read > frames_available) ?
            frames_available : frames_to_read;

      if (retro_60hz_counter >= RETRO_60HZ_CYCLE_INDEX)
      {
         frames_to_read     = frames_available;
         retro_60hz_counter = 0;
      }
      samples_to_read  = frames_to_read << 1;

      for (total = 0; total < frames_to_read; )
         total += audio_batch_cb(
               retro_60hz_audio.samples_buf + (total << 1),
               frames_to_read - total);

      /* Remove uploaded samples from the buffer */
      if (frames_to_read < frames_available)
         memmove(retro_60hz_audio.samples_buf,
               retro_60hz_audio.samples_buf + samples_to_read,
               (retro_60hz_audio.samples_buf_pos - samples_to_read) *
                     sizeof(int16_t));

      retro_60hz_audio.samples_buf_pos -= samples_to_read;
   }
   else
      for (total = 0; total < spec.SoundBufSize; )
         total += audio_batch_cb(
               audio_samples_buf + (total << 1),
               spec.SoundBufSize - total);
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

   info->timing.fps               = retro_60hz_enabled ?
         RETRO_60HZ_FPS : MEDNAFEN_CORE_TIMING_FPS;
   info->timing.sample_rate       = RETRO_SAMPLE_RATE;

   if (hw_rotate_enabled || !rotate_tall)
   {
      info->geometry.base_width   = MEDNAFEN_CORE_GEOMETRY_BASE_W;
      info->geometry.base_height  = MEDNAFEN_CORE_GEOMETRY_BASE_H;
   }
   else
   {
      info->geometry.base_width   = MEDNAFEN_CORE_GEOMETRY_BASE_H;
      info->geometry.base_height  = MEDNAFEN_CORE_GEOMETRY_BASE_W;
   }

   info->geometry.max_width       = MEDNAFEN_CORE_GEOMETRY_MAX_W;
   if (hw_rotate_enabled)
      info->geometry.max_height   = MEDNAFEN_CORE_GEOMETRY_MAX_H;
   else
      info->geometry.max_height   = MEDNAFEN_CORE_GEOMETRY_MAX_W;

   if (rotate_tall)
      info->geometry.aspect_ratio = 1.0f / MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO;
   else
      info->geometry.aspect_ratio = MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO;
}

void retro_deinit(void)
{
   if (surf)
   {
      if (surf->pixels)
#if defined(_3DS)
         linearFree(surf->pixels);
#else
         free(surf->pixels);
#endif
      free(surf);
   }
   surf = NULL;

   if (rotate_buf)
#if defined(_3DS)
      linearFree(rotate_buf);
#else
      free(rotate_buf);
#endif
   rotate_buf = NULL;

   if (audio_samples_buf)
      free(audio_samples_buf);
   audio_samples_buf      = NULL;
   audio_samples_buf_size = 0;

   retro_60hz_deinit();
   retro_60hz_enabled = false;

   libretro_supports_bitmasks = false;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC; /* No real regions of sorts for this handheld, so just set to NTSC. */
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned in_port, unsigned device) { }

void retro_set_environment(retro_environment_t cb)
{
   bool option_cats_supported = false;

   environ_cb = cb;

   libretro_set_core_options(environ_cb,
      &option_cats_supported);
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

   return MDFNSS_LoadSM(&st, 0, 0);
}

void *retro_get_memory_data(unsigned type)
{
   switch (type)
   {
      case RETRO_MEMORY_SAVE_RAM:
         if (wswan_is_ww)
            return WSwan_GetWWSaveBlock(NULL);
         else if (eeprom_size)
            return (uint8_t*)wsEEPROM;
         else if (SRAMSize)
            return wsSRAM;
         break;
      case RETRO_MEMORY_SYSTEM_RAM:
         return (uint8_t*)wsRAM;
      default:
         break;
   }

   return NULL;
}

size_t retro_get_memory_size(unsigned type)
{
   switch (type)
   {
      case RETRO_MEMORY_SAVE_RAM:
         if (wswan_is_ww)
         {
            uint32 ww_size = 0;
            WSwan_GetWWSaveBlock(&ww_size);
            return ww_size;
         }
         else if (eeprom_size)
            return eeprom_size;
         else if (SRAMSize)
            return SRAMSize;
         break;
      case RETRO_MEMORY_SYSTEM_RAM:
         return wsRAMSize;
      default:
         break;
   }

   return 0;
}

void retro_cheat_reset(void)
{
   MDFN_FlushGameCheats(0);
}

/* Accepts multi-part codes separated by any of "+,;._ ", each
 * part being a raw RAM patch "AAAA:VV" / "AAAAA:VV" (hex address,
 * hex byte), matching the convention used by the other Beetle
 * cores. Addresses follow the cheat-engine mapping established in
 * WSwan_MemoryInit(): internal RAM at 0x0000-0xFFFF, cartridge
 * SRAM linearly at 0x10000 onward.
 *
 * Parsed in a single pass with no library calls and no bounded
 * intermediate copy, so codes of any length work (the previous
 * strtok-based version silently truncated multi-part codes at
 * 256 characters). */

static INLINE int cheat_hex_digit(char c)
{
   if (c >= '0' && c <= '9')
      return c - '0';
   if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
   if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
   return -1;
}

static INLINE int cheat_is_sep(char c)
{
   return (c == '+' || c == ',' || c == ';' || c == '.' ||
           c == '_' || c == ' ');
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   const char *p = code;

   (void)index;

   if (!code || !enabled)
      return;

   while (*p)
   {
      uint32 a       = 0;
      uint32 v       = 0;
      int addr_chars = 0;
      int val_chars  = 0;
      int valid      = 1;
      const char *part_start;
      int d;

      while (cheat_is_sep(*p))
         p++;
      if (!*p)
         break;
      part_start = p;

      /* hex address, 1-6 digits */
      while ((d = cheat_hex_digit(*p)) >= 0)
      {
         if (addr_chars >= 6)
         {
            valid = 0;
            break;
         }
         a = (a << 4) | (uint32)d;
         addr_chars++;
         p++;
      }
      if (!addr_chars || addr_chars > 5)
         valid = 0;

      if (valid && *p == ':')
         p++;
      else
         valid = 0;

      /* value: exactly two hex digits */
      if (valid)
      {
         while ((d = cheat_hex_digit(*p)) >= 0)
         {
            if (val_chars >= 2)
            {
               valid = 0;
               break;
            }
            v = (v << 4) | (uint32)d;
            val_chars++;
            p++;
         }
         if (val_chars != 2)
            valid = 0;
      }

      /* the part must end exactly at a separator or the end */
      if (valid && *p && !cheat_is_sep(*p))
         valid = 0;

      if (valid)
      {
         /* RAM, or SRAM when the cart has any */
         if (a < 0x10000 || (SRAMSize && a >= 0x10000 &&
               a < 0x10000 + SRAMSize))
         {
            if (!MDFNI_AddCheat("N/A", a, v, 0, 'R', 1, false))
            {
               if (log_cb)
                  log_cb(RETRO_LOG_WARN, "Failed to set cheat: %05X:%02X\n", a, v);
            }
         }
         else if (log_cb)
            log_cb(RETRO_LOG_WARN, "Cheat address out of range: %05X:%02X\n", a, v);
      }
      else if (log_cb)
         log_cb(RETRO_LOG_WARN, "Invalid cheat code part: '%.16s'\n", part_start);

      /* advance to the next separator (recovers from junk parts) */
      while (*p && !cheat_is_sep(*p))
         p++;
   }
}
