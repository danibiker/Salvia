/***************************************************************************
    Cannonball Main Entry Point.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <streams/file_stream.h>
#include <retro_dirent.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include <libretro.h>
#include "libretro_core_options.h"

#include "input.h"
#include "video.h"

#include "romloader.h"
#include "trackloader.h"
#include "main.h"
#include "../setup.h"
#include "../engine/outrun.h"
#include "../engine/oinputs.h"
#include "../engine/ooutputs.h"
#include "../engine/omusic.h"
#include "../engine/ostats.h"
#include "../frontend/config.h"
#include "roms.h"
#include "engine/audio/osoundint.h"
#include "engine/oattractai.h"
#include "../frontend/menu.h"

#include "lr_options.h"

/* Haptic Support. */
#include "ffeedback.h"

/* Initialize Shared Variables */
int    cannonball_state       = STATE_BOOT;
int    cannonball_frame       = 0;
bool   cannonball_tick_frame  = true;
int    cannonball_fps_counter = 0;

Audio cannonball_audio;

Menu* menu;


/* Pause Engine */
bool pause_engine;

static bool libretro_supports_bitmasks = false;

static void config_init(void)
{
    /* ------------------------------------------------------------------------ */
    /* Menu Settings */
    /* ------------------------------------------------------------------------ */

    config.menu.enabled           = 1;
    config.menu.road_scroll_speed = 50;

    /* ------------------------------------------------------------------------ */
    /* Video Settings */
    /* ------------------------------------------------------------------------ */

    config.video.mode       = 0; /* Video Mode: Default is Windowed */
    config.video.scale      = 1; /* Video Scale: Default is 2x */
    config.video.scanlines  = 0; /* Scanlines */
#ifdef LOW_FPS
    config.video.fps        = 0; /* Default is 30 fps */
#else
    config.video.fps        = 2; /* Default is 60 fps */
#endif
    config.video.fps_count  = 0; /* FPS Counter */
#ifdef DINGUX
    config.video.widescreen = 0; /* Enable Widescreen Mode */
#else
    config.video.widescreen = 1; /* Enable Widescreen Mode */
#endif
    config.video.hires      = 0; /* Hi-Resolution Mode */
    config.video.filtering  = 0; /* Open GL Filtering Mode */
    config.video.shadow     = 0; /* Original System 16 shadow intensity */

    /* Must be set before set_fps() initializes the audio chips. */
    config.sound.rate       = 44100;

    Config_set_fps(&config, config.video.fps);

    /* ------------------------------------------------------------------------ */
    /* Sound Settings */
    /* ------------------------------------------------------------------------ */
    config.sound.enabled     = 1;
    config.sound.advertise   = 1;
    config.sound.preview     = 1;
    config.sound.fix_samples = 1;
    config.sound.music_timer = MUSIC_TIMER;



    /* ------------------------------------------------------------------------ */
    /* Controls */
    /* ------------------------------------------------------------------------ */
    config.controls.gear          = 0;
    config.controls.steer_speed   = 3;
    config.controls.pedal_speed   = 4;
    config.controls.keyconfig[0]  = 273; /* up */
    config.controls.keyconfig[1]  = 274; /* down */
    config.controls.keyconfig[2]  = 276; /* left */
    config.controls.keyconfig[3]  = 275; /* right */
    config.controls.keyconfig[4]  = 122; /* accelerate */
    config.controls.keyconfig[5]  = 120; /* brake */
    config.controls.keyconfig[6]  = 32; /* gear1 */
    config.controls.keyconfig[7]  = 33; /* gear2 */
    config.controls.keyconfig[8]  = 49; /* start */
    config.controls.keyconfig[9]  = 53; /* coin */
    config.controls.keyconfig[10] = 286; /* menu */
    config.controls.keyconfig[11] = 304; /* view */
    config.controls.padconfig[0]  = 0; /* accelerate */
    config.controls.padconfig[1]  = 1; /* brake */
    config.controls.padconfig[2]  = 2; /* gear1 */
    config.controls.padconfig[3]  = 2; /* gear2 */
    config.controls.padconfig[4]  = 3; /* start */
    config.controls.padconfig[5]  = 4; /* coin */
    config.controls.padconfig[6]  = 5; /* padconfig menu */
    config.controls.padconfig[7]  = 6; /* padconfig view */
    config.controls.analog        = 1;
    config.controls.pad_id        = 0; /* pad_id */
    config.controls.axis[0]       = 0; /* wheel */
    config.controls.axis[1]       = 2; /* accelerate */
    config.controls.axis[2]       = 3; /* brake */
    config.controls.asettings[0]  = 75; /* wheel zone */
    config.controls.asettings[1]  = 0; /* wheel dead */
    config.controls.asettings[2]  = 0; /* pedals dead */

    config.controls.haptic        = 0;
    config.controls.max_force     = 0xFFFF;
    config.controls.min_force     = 0x1999;
    config.controls.force_duration= 500;

    /* ------------------------------------------------------------------------ */
    /* Engine Settings */
    /* ------------------------------------------------------------------------ */

    config.engine.dip_time      = 0;
    config.engine.dip_traffic   = 1;

    config.engine.freeze_timer    = config.engine.dip_time == 4;
    config.engine.disable_traffic = config.engine.dip_traffic == 4;
    config.engine.dip_time    &= 3;
    config.engine.dip_traffic &= 3;

    config.engine.freeplay      = 0;
    config.engine.jap           = 0; /* japanese tracks */
    config.engine.prototype     = 0;

    /* Additional Level Objects */
    config.engine.level_objects   = 1;
    config.engine.randomgen       = 1;
    config.engine.fix_bugs_backup = 
    config.engine.fix_bugs        = 1;
    config.engine.fix_timer       = 0;
    config.engine.layout_debug    = 0;
    config.engine.force_ai       = false;
    config.engine.hiscore_delete = true;
    config.engine.hiscore_timer  = HIGHSCORE_TIMER;
    config.engine.new_attract    = 1;
    config.engine.grippy_tyres   = false;
    config.engine.offroad        = false;
    config.engine.bumper         = false;
    config.engine.turbo          = false;
    config.engine.car_pal        = 0;

    /* ------------------------------------------------------------------------ */
    /* Time Trial Mode */
    /* ------------------------------------------------------------------------ */

    config.ttrial.laps    = 3;
    config.ttrial.traffic = 3;

    config.cont_traffic   = 3;
}


/*  libretro.cpp */

retro_log_printf_t                 log_cb;
retro_video_refresh_t              video_cb;
static retro_input_poll_t          input_poll_cb;
static retro_input_state_t         input_state_cb;
retro_environment_t                environ_cb;
static retro_audio_sample_t        audio_cb;
retro_audio_sample_batch_t         audio_batch_cb;

static bool libretro_fps_record_inhibit = false;
static int libretro_fps_prev            = 0;

char rom_path[1024];

char FILENAME_SCORES[1024];
char FILENAME_TTRIAL[1024];
char FILENAME_CONT[1024];

static bool option_visibility_set = false;
static bool sound_enable_prev     = true;
static bool analog_enable_prev    = true;

static bool update_option_visibility(void)
{
   struct retro_variable var                       = {0};
   struct retro_core_option_display option_display = {0};
   bool sound_enable                               = true;
   bool analog_enable                              = true;
   bool updated                                    = false;

   /* Check if sound is enabled */
   var.key = "cannonball_sound_enable";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) &&
       var.value &&
       (strcmp(var.value, "OFF") == 0))
      sound_enable = false;

   /* Check if analog input is enabled */
   var.key = "cannonball_analog";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) &&
       var.value &&
       (strcmp(var.value, "OFF") == 0))
      analog_enable = false;

   /* Hide auxiliary sound options, if required */
   if ((sound_enable != sound_enable_prev) ||
       (!option_visibility_set && !sound_enable))
   {
      option_display.visible = sound_enable;

      option_display.key = "cannonball_sound_advertise";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      option_display.key = "cannonball_sound_preview";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      option_display.key = "cannonball_sound_fix_samples";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
      option_display.key = "cannonball_sound_custom_wav_volume";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      sound_enable_prev = sound_enable;
      updated           = true;
   }

   /* Hide auxiliary input options, if required */
   if ((analog_enable != analog_enable_prev) ||
       (!option_visibility_set && analog_enable))
   {
      option_display.visible = !analog_enable;

      option_display.key = "cannonball_steer_speed";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      option_display.key = "cannonball_pedal_speed";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      analog_enable_prev = analog_enable;
      updated            = true;
   }

   option_visibility_set = true;
   return updated;
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_vfs_interface_info vfs_iface_info;
   struct retro_log_callback log;
   struct retro_core_options_update_display_callback update_display_cb;
   bool no_content = true;
   bool option_categories = false;

   environ_cb = cb;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   libretro_set_core_options(environ_cb, &option_categories);

   update_display_cb.callback = update_option_visibility;
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK,
         &update_display_cb);

   /* File VFS remains compatible with frontends that only expose version 1. */
   vfs_iface_info.required_interface_version = 1;
   vfs_iface_info.iface                      = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
      filestream_vfs_init(&vfs_iface_info);

   /* Directory VFS is optional. retro_dirent falls back to libretro-common's */
   /* implementation when the frontend does not expose version 3. */
   vfs_iface_info.required_interface_version = DIRENT_REQUIRED_VFS_VERSION;
   vfs_iface_info.iface                      = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
      dirent_vfs_init(&vfs_iface_info);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }

void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }

void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }

void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void update_geometry()
{
  struct retro_system_av_info av_info;
  /* Calling 'RETRO_ENVIRONMENT_SET_GEOMETRY' does not
   * update frontend timing; we must therefore inhibit
   * any record of the last set frame rate value, or
   * the next timing update may be skipped */
  libretro_fps_record_inhibit = true;
  retro_get_system_av_info(&av_info);
  libretro_fps_record_inhibit = false;
  environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &av_info);
}

static void update_variables(bool startup)
{
   bool geometry_update = false;
   bool timing_update   = false;
   struct retro_variable var;

   var.key = "cannonball_menu_enabled";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.menu.enabled = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.menu.enabled = 0;
   }

   var.key = "cannonball_menu_road_scroll_speed";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      config.menu.road_scroll_speed = atoi(var.value);
   }

   var.key = "cannonball_video_fps";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      unsigned int newval = 0;

      if (strcmp(var.value, "Ultra Smooth (120)") == 0)
         newval = 3;
      else if (strcmp(var.value, "Original (60/30)") == 0)
         newval = 1;
      else if (strcmp(var.value, "Low (30)") == 0)
         newval = 0;
      else
         newval = 2;

      if (newval != config.video.fps)
      {
         config.video.fps = newval;
         timing_update = true;
      }
   }

   var.key = "cannonball_video_widescreen";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int newval = 0;

      if (strcmp(var.value, "ON") == 0)
         newval = 1;
      else if (strcmp(var.value, "OFF") == 0)
         newval = 0;

      if (newval != config.video.widescreen)
      {
         config.video.widescreen = newval;
         geometry_update = true;
      }
   }

   var.key = "cannonball_video_hires";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int newval = 0;

      if (strcmp(var.value, "ON") == 0)
         newval = 1;
      else if (strcmp(var.value, "OFF") == 0)
         newval = 0;

      if (newval != config.video.hires)
      {
         config.video.hires = newval;
         geometry_update = true;
      }
   }

   var.key = "cannonball_sound_enable";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      unsigned int newval = 0;

      if (strcmp(var.value, "ON") == 0)
         newval = 1;
      else if (strcmp(var.value, "OFF") == 0)
         newval = 0;

      if (newval != config.sound.enabled)
      {
         config.sound.enabled = newval;
         if (config.sound.enabled)
            Audio_start_audio(&cannonball_audio);
         else
            Audio_stop_audio(&cannonball_audio);
      }
   }

   var.key = "cannonball_sound_custom_wav_volume";
   var.value = NULL;

   if (environ_cb(
           RETRO_ENVIRONMENT_GET_VARIABLE,
           &var) &&
       var.value)
   {
       int volume = atoi(var.value);

       if (volume < 0)
           volume = 0;
       else if (volume > 200)
           volume = 200;

       cannonball_audio.custom_wav_volume =
           (uint16_t)volume;
   }

   var.key = "cannonball_gear";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int gear_mode = 0;

      if (strcmp(var.value, "Manual Cabinet") == 0)
         gear_mode = 1;
      else if (strcmp(var.value, "Manual 2 Buttons") == 0)
         gear_mode = 2;
      else if (strcmp(var.value, "Automatic") == 0)
         gear_mode = 3;

      config.controls.gear = gear_mode;
   }

   var.key = "cannonball_analog";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
      {
         config.controls.analog = 1;
         input.analog = 1;
      }
      else if (strcmp(var.value, "OFF") == 0)
      {
         config.controls.analog = 0;
         input.analog = 0;
      }
   }

   var.key = "cannonball_haptic_strength";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int min_force_last = config.controls.min_force;
      int max_force_last = config.controls.max_force;
      int haptic_level   = atoi(var.value);

      haptic_level = (haptic_level < 0)  ? 0  : haptic_level;
      haptic_level = (haptic_level > 10) ? 10 : haptic_level;

      if (haptic_level == 0)
      {
         config.controls.min_force = 0;
         config.controls.max_force = 0;
         forcefeedback_deactivate_rumble();
      }
      else
      {
         config.controls.min_force = 0x3F +
               (haptic_level * ((0x1999 - 0x3F) / 10));
         config.controls.max_force = 0x5  +
               (haptic_level * (0xFFFF / 10));
      }

      if ((config.controls.min_force != min_force_last) ||
          (config.controls.max_force != max_force_last))
         forcefeedback_update_force_limits(
               config.controls.max_force,
               config.controls.min_force,
               config.controls.force_duration);
   }

   var.key = "cannonball_freeplay";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.freeplay = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.freeplay = 0;
   }

   var.key = "cannonball_force_ai";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "OFF") == 0)
         config.engine.force_ai = false;
      else
         config.engine.force_ai = true;
   }

   var.key = "cannonball_grippy_tyres";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      config.engine.grippy_tyres = strcmp(var.value, "ON") == 0;

   var.key = "cannonball_offroad_tyres";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      config.engine.offroad = strcmp(var.value, "ON") == 0;

   var.key = "cannonball_strong_bumper";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      config.engine.bumper = strcmp(var.value, "ON") == 0;

   var.key = "cannonball_faster_car";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      config.engine.turbo = strcmp(var.value, "ON") == 0;

   var.key = "cannonball_car_color";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int car_pal = 0;

      if (strcmp(var.value, "BLUE") == 0)
         car_pal = 1;
      else if (strcmp(var.value, "YELLOW") == 0)
         car_pal = 2;
      else if (strcmp(var.value, "GREEN") == 0)
         car_pal = 3;
      else if (strcmp(var.value, "CYAN") == 0)
         car_pal = 4;
      else if (strcmp(var.value, "BLACK") == 0)
         car_pal = 5;
      else if (strcmp(var.value, "WHITE") == 0)
         car_pal = 6;

      config.engine.car_pal = car_pal;
   }

   /* All of the remaining options require a core
    * restart to apply */
   if (!startup)
      goto end;

   var.key = "cannonball_sound_advertise";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.sound.advertise = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.sound.advertise = 0;
   }

   var.key = "cannonball_sound_preview";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.sound.preview = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.sound.preview = 0;
   }

   var.key = "cannonball_sound_fix_samples";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.sound.fix_samples = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.sound.fix_samples = 0;
   }

   var.key = "cannonball_steer_speed";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      config.controls.steer_speed = atoi(var.value);
   }

   var.key = "cannonball_pedal_speed";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      config.controls.pedal_speed = atoi(var.value);
   }

   var.key = "cannonball_jap";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.jap = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.jap = 0;
   }

   var.key = "cannonball_dip_time";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int diptime = 0;
      config.engine.freeze_timer = false;

      if (strcmp(var.value, "Normal (75s)") == 0)
         diptime = 1;
      else if (strcmp(var.value, "Hard (72s)") == 0)
         diptime = 2;
      else if (strcmp(var.value, "Very Hard (70s)") == 0)
         diptime = 3;
      else if (strcmp(var.value, "Infinite Time") == 0)
      {
         diptime = 4;
         config.engine.freeze_timer = true;
      }

      config.engine.dip_time = diptime;
   }

   var.key = "cannonball_dip_traffic";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int diptraffic = 0;
      config.engine.disable_traffic = false;

      if (strcmp(var.value, "Normal") == 0)
         diptraffic = 1;
      else if (strcmp(var.value, "Hard") == 0)
         diptraffic = 2;
      else if (strcmp(var.value, "Very Hard") == 0)
         diptraffic = 3;
      else if (strcmp(var.value, "No Traffic") == 0)
      {
         diptraffic = 4;
         config.engine.disable_traffic = true;
      }

      config.engine.dip_traffic = diptraffic;
   }

   var.key = "cannonball_level_objects";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.level_objects = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.level_objects = 0;
   }

   var.key = "cannonball_prototype";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.prototype = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.prototype = 0;
   }

   var.key = "cannonball_new_attract";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.new_attract = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.new_attract = 0;
   }

   var.key = "cannonball_randomgen";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "OFF") == 0)
         config.engine.randomgen = 0;
      else
         config.engine.randomgen = 1;
   }

   var.key = "cannonball_fix_bugs";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.fix_bugs = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.fix_bugs = 0;
   }

   var.key = "cannonball_fix_timer";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.fix_timer = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.fix_timer = 0;
   }

   var.key = "cannonball_ttrial_laps";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      config.ttrial.laps = atoi(var.value);
   }

   var.key = "cannonball_ttrial_traffic";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      config.ttrial.traffic = atoi(var.value);
   }

   var.key = "cannonball_cont_traffic";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      config.cont_traffic = atoi(var.value);
   }

   var.key = "cannonball_layout_debug";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.layout_debug = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.layout_debug = 0;
   }

end:
   if (geometry_update)
   {
      Video_disable(&video);
      Video_init(&video, &roms, &config.video);
      hwsprites_set_x_clip(video.sprite_layer, false);
      update_geometry();
   }

   if (timing_update)
      Config_set_fps(&config, config.video.fps);

   /* Show/hide core options */
   update_option_visibility();

   /* Update menu to reflect any option changes */
   if (!startup && (cannonball_state == STATE_MENU))
      Menu_refresh_menu(menu);
}

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name     = "Cannonball";
    info->library_version  = "git";
    info->need_fullpath    = true;
    info->valid_extensions = "game"; 
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
   unsigned scale_factor = config.video.hires ? 2 : 1;

   memset(info, 0, sizeof(*info));

   info->timing.fps            = config.fps;
   /* The chips (and mixer) emit exactly config.sound.rate / fps samples
    * per frame, an integer, so the true output rate is that count times
    * fps. This equals config.sound.rate when it divides evenly (30/60 fps)
    * and is slightly lower otherwise (e.g. 44040 at 120 fps, since
    * 44100/120 truncates to 367). Reporting the effective rate keeps the
    * frontend's A/V sync exact. */
   info->timing.sample_rate    =
      (double)((config.sound.rate / (unsigned)config.fps) * (unsigned)config.fps);

   info->geometry.max_width    = S16_WIDTH_WIDE << 1;
   info->geometry.max_height   = S16_HEIGHT     << 1;

   if (config.video.widescreen)
   {
      info->geometry.base_width   = S16_WIDTH_WIDE * scale_factor;
      info->geometry.base_height  = S16_HEIGHT     * scale_factor;
      info->geometry.aspect_ratio = 16.0 / 9.0;
   }
   else
   {
      info->geometry.base_width   = S16_WIDTH  * scale_factor;
      info->geometry.base_height  = S16_HEIGHT * scale_factor;
      info->geometry.aspect_ratio = 4.0 / 3.0;
   }

   if (!libretro_fps_record_inhibit)
      libretro_fps_prev = config.fps;
}

void update_timing(void)
{
   struct retro_system_av_info system_av_info;
   retro_get_system_av_info(&system_av_info);
   environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
    (void) port;
    (void) device;
}

size_t retro_serialize_size(void) {
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data, size_t size)
{
   return false;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void) index;
    (void) enabled;
    (void) code;
}

static void retro_osd_error_msg(const char *str)
{
   unsigned msg_interface_version = 0;

   if (string_is_empty(str))
      return;

   environ_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION,
         &msg_interface_version);

   if (msg_interface_version >= 1)
   {
      struct retro_message_ext msg;
      msg.msg      = str;
      msg.duration = 3000;
      msg.priority = 3;
      msg.level    = RETRO_LOG_ERROR;
      msg.target   = RETRO_MESSAGE_TARGET_ALL;
      msg.type     = RETRO_MESSAGE_TYPE_NOTIFICATION;
      msg.progress = -1;
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
   }
   else
   {
      struct retro_message msg;
      msg.msg    = str;
      msg.frames = 180;
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
   }
}

static void retro_build_save_paths(void)
{
   const char *save_dir = NULL;

   FILENAME_SCORES[0] = '\0';
   FILENAME_TTRIAL[0] = '\0';
   FILENAME_CONT[0] = '\0';

   /* Get frontend save directory
    * > Use game data directory as a fallback if
    *   no save directory is defined */
   if (!environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) ||
         !save_dir)
      save_dir = rom_path;

   /* Build high score save paths
    * > Note: These are not 'full' paths;
    *   suffix + extension are added elsewhere */
   fill_pathname_join(FILENAME_SCORES, save_dir,
         "hiscores", sizeof(FILENAME_SCORES));

   fill_pathname_join(FILENAME_TTRIAL, save_dir,
         "hiscores_timetrial", sizeof(FILENAME_TTRIAL));

   fill_pathname_join(FILENAME_CONT, save_dir,
         "hiscores_continuous", sizeof(FILENAME_CONT));
}

bool retro_load_game(const struct retro_game_info *info)
{
   bool loaded;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;

   struct retro_input_descriptor desc[] = {
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Gear (Low, 2 Buttons Mode)"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "Accelerate"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Gear (High, 2 Buttons Mode)"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Brake"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Adjust View"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Go Back To Menu"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "Analog Brake"},
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "Analog Accelerate"},
      {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      {0},
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "[Cannonball]: RGB565 is not supported.\n");
      return false;
   }

   if (info && !string_is_empty(info->path))
      fill_pathname_basedir(rom_path, info->path, sizeof(rom_path));
   else
   {
      const char *system_dir = NULL;
      bool path_valid        = false;

      if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir)
            && system_dir)
      {
         fill_pathname_join(rom_path, system_dir,
               "cannonball", sizeof(rom_path));
         fill_pathname_slash(rom_path, sizeof(rom_path));

         path_valid = path_is_directory(rom_path);
      }

      if (!path_valid)
      {
         retro_osd_error_msg("Cannonball game files missing from frontend system directory");
         return false;
      }
   }

   log_cb(RETRO_LOG_INFO, "Rom directory: %s\n", rom_path);
   retro_build_save_paths();

   loaded = Roms_load_revb_roms(&roms, false);

   if (!loaded)
   {
      retro_osd_error_msg("Cannonball ROM files missing from game directory");
      return false;
   }

   config_init();
   snprintf(config.data.res_path, sizeof(config.data.res_path), "%sres/", rom_path);

   update_variables(true);

   /* Load fixed PCM ROM based on config */
   if (config.sound.fix_samples)
      Roms_load_pcm_rom(&roms, true);

   /* Load patched widescreen tilemaps */
   if (!OMusic_load_widescreen_map(&omusic))
   {
      if (log_cb)
         log_cb(RETRO_LOG_WARN, "[Cannonball]: Unable to load widescreen tilemaps.\n");
   }

   if (!Video_init(&video, &roms, &config.video))
      return false;

   menu = (Menu*)calloc(1, sizeof(Menu));
   Menu_ctor(menu);

   Audio_init(&cannonball_audio);
   cannonball_state = config.menu.enabled ? STATE_INIT_MENU : STATE_INIT_GAME;

   /* Initialize controls */
   Input_init(&input, config.controls.pad_id,
         config.controls.keyconfig, config.controls.padconfig, 
         config.controls.analog,    config.controls.axis, config.controls.asettings);

   config.controls.haptic = forcefeedback_init_rumble_interface(environ_cb);
   if (config.controls.haptic) 
      config.controls.haptic = forcefeedback_init(
            config.controls.max_force,
            config.controls.min_force,
            config.controls.force_duration);


   /* Populate menus */
   Menu_populate(menu);

   return true;
}

bool retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info)
{
    (void) game_type;
    (void) info;
    (void) num_info;
    return false;
}

void retro_unload_game(void)
{
    Audio_stop_audio(&cannonball_audio);
    Input_close(&input);
    forcefeedback_close();
    Menu_dtor(menu);
    free(menu);
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void *retro_get_memory_data(unsigned id)
{
   switch (id)
   {
      case RETRO_MEMORY_SYSTEM_RAM:
      default:
         break;
   }

   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   return 0;
}

void retro_init(void)
{
   unsigned level = 2;

   TrackLoader_ctor(&trackloader);
   Outrun_ctor(&outrun);
   Audio_ctor(&cannonball_audio);
   Config_ctor(&config);
   Video_ctor(&video);
   OMusic_ctor(&omusic);
   Roms_ctor(&roms);
   OSoundInt_ctor(&osoundint);
   OAttractAI_ctor(&oattractai);
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;

   lr_options_init();

   option_visibility_set = false;
   sound_enable_prev     = true;
   analog_enable_prev    = true;
}

void retro_deinit(void)
{
   lr_options_close();

   libretro_fps_record_inhibit = false;
   libretro_fps_prev = 0;

   libretro_supports_bitmasks = false;
}

void retro_reset(void)
{
    if (log_cb)
        log_cb(
            RETRO_LOG_INFO,
            "[Cannonball]: Reset requested.\n");

    pause_engine = false;
    cannonball_frame = 0;
    cannonball_tick_frame = true;
    cannonball_fps_counter = 0;

    forcefeedback_deactivate_rumble();

    Audio_clear_wav(&cannonball_audio);

    /* A frontend may request Reset immediately after changing a Core */
    /* Option, without running another frame. Refresh the live options */
    /* before selecting the reset destination. */
    update_variables(false);

    /* Reset the active game session to its initial state. */
    /* */
    /* Time Trial stores its selected mode, course and an artificial */
    /* credit before handing control from the frontend to the engine. */
    /* These values must not survive a frontend Reset request. */
    outrun.cannonball_mode = MODE_ORIGINAL;

    outrun.ttrial.level            = 0;
    outrun.ttrial.current_lap      = 0;
    outrun.ttrial.best_lap_counter = 0;
    outrun.ttrial.best_lap[0]      = 0;
    outrun.ttrial.best_lap[1]      = 0;
    outrun.ttrial.best_lap[2]      = 0;
    outrun.ttrial.new_high_score   = false;
    outrun.ttrial.overtakes        = 0;
    outrun.ttrial.crashes          = 0;
    outrun.ttrial.vehicle_cols     = 0;

    ostats.credits = 0;

    /* Reproduce the initial state selected when the content is loaded: */
    /* main menu when enabled, otherwise the normal game boot sequence. */
    cannonball_state = config.menu.enabled
        ? STATE_INIT_MENU
        : STATE_INIT_GAME;

    if (log_cb)
        log_cb(
            RETRO_LOG_INFO,
            "[Cannonball]: Reset target: %s.\n",
            config.menu.enabled
                ? "main menu"
                : "attract mode");
}

struct button_bind
{
   unsigned id;
   unsigned joy_id;
};

static struct button_bind binds[] = { 
   {273, RETRO_DEVICE_ID_JOYPAD_UP},
   {274, RETRO_DEVICE_ID_JOYPAD_DOWN},
   {276, RETRO_DEVICE_ID_JOYPAD_LEFT},
   {275, RETRO_DEVICE_ID_JOYPAD_RIGHT},
   {122, RETRO_DEVICE_ID_JOYPAD_B},      /* Accelerate */
   {120, RETRO_DEVICE_ID_JOYPAD_Y},      /* Brake */
   {32,  RETRO_DEVICE_ID_JOYPAD_X},      /* Gear 1 */
   {33,  RETRO_DEVICE_ID_JOYPAD_A},      /* Gear 2 */
   {49,  RETRO_DEVICE_ID_JOYPAD_START},  /* Start  */
   {53,  RETRO_DEVICE_ID_JOYPAD_SELECT}, /* Coin  */
   {304, RETRO_DEVICE_ID_JOYPAD_L},      /* View  */
   {286, RETRO_DEVICE_ID_JOYPAD_R},      /* Menu  */
};

static void process_events(void)
{
   unsigned i;
   int analog_left_x, analog_r2, analog_l2;
   int16_t ret = 0;

   input_poll_cb();

   if (libretro_supports_bitmasks)
      ret = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
   else
   {
      for (i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R+1); i++)
      {
         if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, binds[i].joy_id))
            ret |= (1 << i);
      }
   }

   for (i = 0; i < (sizeof(binds) / sizeof(binds[0])); i++)
   {
      if (ret & (1 << binds[i].joy_id))
         Input_handle_key(&input, binds[i].id, true);
      else
         Input_handle_key(&input, binds[i].id, false);
   }

   analog_left_x = input_state_cb(0, RETRO_DEVICE_ANALOG,
                     RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
   analog_r2     = input_state_cb(0, RETRO_DEVICE_ANALOG,
                     RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_R2);
   analog_l2     = input_state_cb(0, RETRO_DEVICE_ANALOG,
                     RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_L2);

   /* Fallback to digital */
   if (config.controls.analog == 1)
   {
      if (analog_r2 == 0)
         analog_r2 = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_B )) ? 0x7FFF : 0;
      if (analog_l2 == 0)
         analog_l2 = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_Y )) ? 0x7FFF : 0;
      if (analog_left_x == 0)
      {
         analog_left_x += (ret & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT )) ? -0x7FFF : 0;
         analog_left_x += (ret & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT )) ? 0x7FFF : 0;
      }
   }

   Input_handle_joy_axis(&input, analog_left_x, analog_r2, analog_l2);
}

void retro_run(void)
{
    bool updated = false;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        update_variables(false);

    if (config.fps != libretro_fps_prev)
        update_timing();

    cannonball_frame++;


    switch (config.fps)
    {
        case 60:
            /* 60 fps
             * Non-standard: tick every second frame */
            cannonball_tick_frame = cannonball_frame & 1;
            break;
        case 120:
            /* 120 fps
             * Non-standard: tick every fourth frame */
            cannonball_tick_frame = (cannonball_frame & 3) == 1;
            break;
        case 30:
        default:
            /* 30 fps
             * Standard rate: tick every frame */
            cannonball_tick_frame = true;
            break;
    }

    process_events();

    if (cannonball_tick_frame)
    {
        OInputs_tick(&oinputs);           /* Do Controls */
        OInputs_do_gear(&oinputs);        /* Digital Gear */
    }

    switch (cannonball_state)
    {
        case STATE_GAME:
        {
            if (cannonball_tick_frame)
            {
                if (Input_has_pressed(&input, TIMER)) outrun.freeze_timer = !outrun.freeze_timer;
                if (Input_has_pressed(&input, PAUSE)) pause_engine = !pause_engine;
                if (Input_has_pressed(&input, MENU))  cannonball_state = STATE_INIT_MENU;
            }

            if (!pause_engine || Input_has_pressed(&input, STEP))
            {
                Outrun_tick(&outrun, cannonball_tick_frame);
                if (cannonball_tick_frame) Input_frame_done(&input);

                /* Tick audio program code */
                OSoundInt_tick(&osoundint);
                /* Tick Audio */
                Audio_tick(&cannonball_audio);
            }
            else
            {                
                if (cannonball_tick_frame) Input_frame_done(&input);
            }
        }
        break;

        case STATE_INIT_GAME:
            if (config.engine.jap && !Roms_load_japanese_roms(&roms))
            {
                cannonball_state = STATE_QUIT;
            }
            else
            {
                pause_engine = false;
                Outrun_init(&outrun);
                cannonball_state = STATE_GAME;
            }
            break;

        case STATE_MENU:
        {
            Menu_tick(menu);
            Input_frame_done(&input);
            /* Tick audio program code */
            OSoundInt_tick(&osoundint);
            /* Tick Audio */
            Audio_tick(&cannonball_audio);
        }
        break;

        case STATE_INIT_MENU:
            OInputs_init(&oinputs);
            OOutputs_init(outrun.outputs);
            Menu_init(menu);
            cannonball_state = STATE_MENU;
            break;

        case STATE_QUIT:
            environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
            return;
    }


    /* Draw Video */
    Video_prepare_frame(&video);
    Video_render_frame(&video);

    /* Stop any haptic feedback effects if */
    /* duration timer has elapsed */
    forcefeedback_update_rumble_interface();
}
