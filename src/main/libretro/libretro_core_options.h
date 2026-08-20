#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

/*
 ********************************
 * VERSION: 2.0
 ********************************
 *
 * - 2.0: Add support for core options v2 interface
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_v2_category option_cats_us[] = {
   {
      "menu",
      "Game Menu",
      "Configure the in-game menu."
   },
   {
      "video",
      "Video",
      "Configure frame rate, widescreen and resolution settings."
   },
   {
      "audio",
      "Audio",
      "Configure sound and music settings."
   },
   {
      "input",
      "Input",
      "Configure gear shift behaviour, analog input and haptic feedback settings."
   },
   {
      "engine",
      "Game Engine",
      "Configure settings that change or enhance the base functionality of the original game engine."
   },
   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_us[] = {
   {
      "cannonball_menu_enabled",
      "Menu > Show at Start",
      "Show Menu at Start",
      "Display main menu when launching the core. When disabled, core will boot directly into attract mode.",
      NULL,
      "menu",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_menu_road_scroll_speed",
      "Menu > Road Scroll Speed",
      "Menu Road Scroll Speed",
      "Set speed of the scrolling road animation shown behind the main menu.",
      NULL,
      "menu",
      {
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "40",  NULL },
         { "50",  NULL },
         { "60",  NULL },
         { "70",  NULL },
         { "80",  NULL },
         { "90",  NULL },
         { "100", NULL },
         { "150", NULL },
         { "200", NULL },
         { "300", NULL },
         { "400", NULL },
         { "500", NULL },
         { NULL, NULL },
      },
      "50"
   },
   {
      "cannonball_video_fps",
      "Video > Frame Rate",
      "Frame Rate",
      "Set internal frame rate. In 'Original' mode, game logic, road scrolling and sprite zooming update at 30fps, but the horizon scrolls at 60fps.",
      NULL,
      "video",
      {
         { "Low (30)",           NULL },
         { "Smooth (60)",        NULL },
         { "Ultra Smooth (120)", NULL },
         { "Original (60/30)",   NULL },
         { NULL, NULL },
      },
#ifdef LOW_FPS
      "Low (30)"
#else
      "Smooth (60)"
#endif
   },
   {
      "cannonball_video_widescreen",
      "Video > Widescreen Mode",
      "Widescreen Mode",
      "Extend the play area to a widescreen aspect. Base internal resolution is set to 398x224, increasing the screen width by 25%.",
      NULL,
      "video",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
#ifdef DINGUX
      "OFF"
#else
      "ON"
#endif
   },
   {
      "cannonball_video_hires",
      "Video > High-Resolution Mode",
      "High-Resolution Mode",
      "Double the game's internal resolution from the original 320x224 to 640x448 (768x448 in widescreen mode). Increases the fidelity of rendered sprites and the road.",
      NULL,
      "video",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_sound_enable",
      "Audio > Enable",
      "Enable Audio",
      "Allow playback of music and sound effects.",
      NULL,
      "audio",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_sound_custom_wav_volume",
      "Audio > Custom WAV Volume",
      "Custom WAV Volume",
      "Set playback volume of custom WAV music tracks. "
      "Does not affect original arcade music or external BIN music. "
      "50% matches the historical SDL attenuation; "
      "values above 100% may clip when mixed with sound effects.",
      NULL,
      "audio",
      {
         { "0", "0%" },
         { "10", "10%" },
         { "20", "20%" },
         { "30", "30%" },
         { "40", "40%" },
         { "50", "50%" },
         { "60", "60%" },
         { "70", "70%" },
         { "80", "80%" },
         { "90", "90%" },
         { "100", "100%" },
         { "110", "110%" },
         { "120", "120%" },
         { "130", "130%" },
         { "140", "140%" },
         { "150", "150%" },
         { "160", "160%" },
         { "170", "170%" },
         { "180", "180%" },
         { "190", "190%" },
         { "200", "200%" },
         { NULL, NULL },
      },
      "200"
   },
   {
      "cannonball_sound_advertise",
      "Audio > Advertise Sound (Restart)",
      "Advertise Sound (Restart)",
      "Enable audio while core is in attract mode.",
      NULL,
      "audio",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_sound_preview",
      "Audio > Preview Music (Restart)",
      "Preview Music (Restart)",
      "Play music tracks in the pre-race song selection screen. In the original game, playback of the selected track would not start until the race began.",
      NULL,
      "audio",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_sound_fix_samples",
      "Audio > Fix Samples (Restart)",
      "Fix Samples (Restart)",
      "Replace distorted audio samples present in the original game (caused by corrupt ROM data) with corrected versions. Requires the patched audio ROM 'opr-10188.71f' in the game content directory.",
      NULL,
      "audio",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_gear",
      "Input > Gear Mode",
      "Gear Mode",
      "Set gear shift behaviour. 'Manual': press Gear button to toggle low/high. 'Manual Cabinet': hold Gear button to shift low (high by default). 'Manual 2 Buttons': separate Gear buttons for low/high. 'Automatic': no gear shift required.",
      NULL,
      "input",
      {
         { "Manual",           NULL },
         { "Manual Cabinet",   NULL },
         { "Manual 2 Buttons", NULL },
         { "Automatic",        NULL },
         { NULL, NULL },
      },
      "Manual"
   },
   {
      "cannonball_analog",
      "Input > Analog Controls",
      "Analog Controls",
      "Enable analog steering, acceleration and breaking via the left stick and triggers of an analog gamepad (with D-Pad, 'Accelerate' and 'Brake' buttons producing a purely digital response). When disabled, analog input is simulated based on the duration of digital button presses.",
      NULL,
      "input",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_steer_speed",
      "Input > Digital Steer Speed (Restart)",
      "Digital Steer Speed (Restart)",
      "Set rate of change of simulated analog input when using a digital D-Pad for steering. Ignored when 'Input > Analog Controls' is enabled.",
      "Set rate of change of simulated analog input when using a digital D-Pad for steering. Ignored when 'Analog Controls' is enabled.",
      "input",
      {
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { "4", NULL },
         { "5", NULL },
         { "6", NULL },
         { "7", NULL },
         { "8", NULL },
         { "9", NULL },
         { NULL, NULL },
      },
      "3"
   },
   {
      "cannonball_pedal_speed",
      "Input > Digital Pedal Speed (Restart)",
      "Digital Pedal Speed (Restart)",
      "Set rate of change of simulated analog input when using digital buttons for acceleration/braking. Ignored when 'Input > Analog Controls' is enabled.",
      "Set rate of change of simulated analog input when using digital buttons for acceleration/braking. Ignored when 'Analog Controls' is enabled.",
      "input",
      {
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { "4", NULL },
         { "5", NULL },
         { "6", NULL },
         { "7", NULL },
         { "8", NULL },
         { "9", NULL },
         { NULL, NULL },
      },
      "4"
   },
   {
      "cannonball_haptic_strength",
      "Input > Haptic Feedback Strength",
      "Haptic Feedback Strength",
      "Set intensity of gamepad rumble effects.",
      NULL,
      "input",
      {
         { "0",  NULL },
         { "1",  NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { "7",  NULL },
         { "8",  NULL },
         { "9",  NULL },
         { "10", NULL },
         { NULL, NULL },
      },
      "10"
   },
   {
      "cannonball_jap",
      "Engine > Use Japanese Tracks (Restart)",
      "Use Japanese Tracks (Restart)",
      "Select track layouts from the Japanese version of the game instead of those from the final Western release. Increases game difficulty by introducing sharper unforgiving hills and more aggressive turns.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_freeplay",
      "Engine > Freeplay Mode",
      "Freeplay Mode",
      "Enable the 'freeplay' mode of the original arcade machine, where no coins are required to play.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_force_ai",
      "Engine > Force AI to Play",
      "Force AI to Play",
      "Enable computer control of the car during all races.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_grippy_tyres",
      "Engine > Grippy Tyres",
      "Grippy Tyres",
      "Increase tyre grip to reduce lateral sliding while cornering.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_offroad_tyres",
      "Engine > Offroad Tyres",
      "Offroad Tyres",
      "Prevent the usual speed reduction when driving away from the road surface.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_strong_bumper",
      "Engine > Strong Bumper",
      "Strong Bumper",
      "Reduce the effect of collisions with traffic vehicles.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_faster_car",
      "Engine > Faster Car",
      "Faster Car",
      "Increase engine torque and raise the maximum vehicle speed.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_car_color",
      "Engine > Car Color",
      "Car Color",
      "Select the Ferrari body colour.",
      NULL,
      "engine",
      {
         { "RED",    NULL },
         { "BLUE",   NULL },
         { "YELLOW", NULL },
         { "GREEN",  NULL },
         { "CYAN",   NULL },
         { "BLACK",  NULL },
         { "WHITE",  NULL },
         { NULL, NULL },
      },
      "RED"
   },
   {
      "cannonball_dip_time",
      "Engine > Time Limit (Restart)",
      "Time Limit (Restart)",
      "Set initial time limit when starting Stage 1. Associated difficulty rating also affects how much time is added by the 'Extend Time' feature at the end of each stage.",
      NULL,
      "engine",
      {
         { "Easy (80s)",      NULL },
         { "Normal (75s)",    NULL },
         { "Hard (72s)",      NULL },
         { "Very Hard (70s)", NULL },
         { "Infinite Time",   NULL },
         { NULL, NULL },
      },
      "Easy (80s)"
   },
   {
      "cannonball_dip_traffic",
      "Engine > Traffic Difficulty (Restart)",
      "Traffic Difficulty (Restart)",
      "Set density of on-screen traffic. Higher difficulty levels cause more traffic objects to spawn. Varies on a per-stage basis.",
      NULL,
      "engine",
      {
         { "Easy",       NULL },
         { "Normal",     NULL },
         { "Hard",       NULL },
         { "Very Hard",  NULL },
         { "No Traffic", NULL },
         { NULL, NULL },
      },
      "Normal"
   },
   {
      "cannonball_level_objects",
      "Engine > Extra Level Objects (Restart)",
      "Extra Level Objects (Restart)",
      "Remove the object slot limit present in the original game. Prevents the disappearance of random elements in 'busy' levels, e.g. archway segments in the Gateway stage.",
      NULL,
      "engine",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_prototype",
      "Engine > Use Prototype Stage 1 (Restart)",
      "Use Prototype Stage 1 (Restart)",
      "Replace Stage 1 (Coconut Beach) with an early, unused prototype contained in the original game data. Considered more difficult than the final Japanese and western versions of the track, and contains some scenery errors.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_new_attract",
      "Engine > New Attract Mode (Restart)",
      "New Attract Mode (Restart)",
      "Replace the original attract mode with an enhanced version featuring improved AI and cycling camera views.",
      NULL,
      "engine",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_randomgen",
      "Engine > Randomize Traffic Patterns (Restart)",
      "Randomize Traffic Patterns (Restart)",
      "Spawn traffic objects randomly. When disabled, the fixed patterns of the original game are used.",
      NULL,
      "engine",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_fix_bugs",
      "Engine > Fix Game Bugs (Restart)",
      "Fix Game Bugs (Restart)",
      "Correct a number of minor graphical and base engine errors present in the original game. Includes fixes for shadow/sprite rendering glitches, wrong millisecond values shown when passing checkpoints and improper centering of the steering wheel.",
      NULL,
      "engine",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "cannonball_fix_timer",
      "Engine > Fix Timing Bugs (Restart)",
      "Fix Timing Bugs (Restart)",
      "Correct a number of timing-related errors present in the original game. Includes fixes for the stage time counter decreasing ~7% slower than actual clock time, and the game over state occurring one second after the timer reaches zero.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "cannonball_ttrial_laps",
      "Engine > Time Trial Laps (Restart)",
      "Time Trial Laps (Restart)",
      "Set number of laps to race on any track in 'Time Trial' mode.",
      NULL,
      "engine",
      {
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { "4", NULL },
         { "5", NULL },
         { NULL, NULL },
      },
      "3"
   },
   {
      "cannonball_ttrial_traffic",
      "Engine > Time Trial Traffic Level (Restart)",
      "Time Trial Traffic Level (Restart)",
      "Set maximum number of traffic objects that can appear on-screen simultaneously in 'Time Trial' mode.",
      NULL,
      "engine",
      {
         { "0", NULL },
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { "4", NULL },
         { "5", NULL },
         { "6", NULL },
         { "7", NULL },
         { "8", NULL },
         { NULL, NULL },
      },
      "3"
   },
   {
      "cannonball_cont_traffic",
      "Engine > Continuous Mode Traffic Level (Restart)",
      "Continuous Mode Traffic Level (Restart)",
      "Set maximum number of traffic objects that can appear on-screen simultaneously in 'Continuous' mode.",
      NULL,
      "engine",
      {
         { "0", NULL },
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { "4", NULL },
         { "5", NULL },
         { "6", NULL },
         { "7", NULL },
         { "8", NULL },
         { NULL, NULL },
      },
      "3"
   },
   {
      "cannonball_layout_debug",
      "Engine > Display Debug Info (Restart)",
      "Display Debug Info (Restart)",
      "Show debug information in the on-screen HUD: level position, height pattern, sprite pattern.",
      NULL,
      "engine",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};

struct retro_core_options_v2 options_us = {
   option_cats_us,
   option_defs_us
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_options_v2 *options_intl[RETRO_LANGUAGE_LAST] = {
   &options_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,        /* RETRO_LANGUAGE_JAPANESE */
   NULL,        /* RETRO_LANGUAGE_FRENCH */
   NULL,        /* RETRO_LANGUAGE_SPANISH */
   NULL,        /* RETRO_LANGUAGE_GERMAN */
   NULL,        /* RETRO_LANGUAGE_ITALIAN */
   NULL,        /* RETRO_LANGUAGE_DUTCH */
   NULL,        /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,        /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,        /* RETRO_LANGUAGE_RUSSIAN */
   NULL,        /* RETRO_LANGUAGE_KOREAN */
   NULL,        /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,        /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,        /* RETRO_LANGUAGE_ESPERANTO */
   NULL,        /* RETRO_LANGUAGE_POLISH */
   NULL,        /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,        /* RETRO_LANGUAGE_ARABIC */
   NULL,        /* RETRO_LANGUAGE_GREEK */
   NULL,        /* RETRO_LANGUAGE_TURKISH */
   NULL,        /* RETRO_LANGUAGE_SLOVAK */
   NULL,        /* RETRO_LANGUAGE_PERSIAN */
   NULL,        /* RETRO_LANGUAGE_HEBREW */
   NULL,        /* RETRO_LANGUAGE_ASTURIAN */
   NULL,        /* RETRO_LANGUAGE_FINNISH */
   NULL,        /* RETRO_LANGUAGE_INDONESIAN */
   NULL,        /* RETRO_LANGUAGE_SWEDISH */
   NULL,        /* RETRO_LANGUAGE_UKRAINIAN */
};
#endif

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb,
      bool *categories_supported)
{
   unsigned version  = 0;
#ifndef HAVE_NO_LANGEXTRA
   unsigned language = 0;
#endif

   if (!environ_cb || !categories_supported)
      return;

   *categories_supported = false;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   if (version >= 2)
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_v2_intl core_options_intl;

      core_options_intl.us    = &options_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = options_intl[language];

      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL,
            &core_options_intl);
#else
      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
            &options_us);
#endif
   }
   else
   {
      size_t i, j;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_core_option_definition
            *option_v1_defs_us         = NULL;
#ifndef HAVE_NO_LANGEXTRA
      size_t num_options_intl          = 0;
      struct retro_core_option_v2_definition
            *option_defs_intl          = NULL;
      struct retro_core_option_definition
            *option_v1_defs_intl       = NULL;
      struct retro_core_options_intl
            core_options_v1_intl;
#endif
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine total number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      if (version >= 1)
      {
         /* Allocate US array */
         option_v1_defs_us = (struct retro_core_option_definition *)
               calloc(num_options + 1, sizeof(struct retro_core_option_definition));

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            struct retro_core_option_v2_definition *option_def_us = &option_defs_us[i];
            struct retro_core_option_value *option_values         = option_def_us->values;
            struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];
            struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;

            option_v1_def_us->key           = option_def_us->key;
            option_v1_def_us->desc          = option_def_us->desc;
            option_v1_def_us->info          = option_def_us->info;
            option_v1_def_us->default_value = option_def_us->default_value;

            /* Values must be copied individually... */
            while (option_values->value)
            {
               option_v1_values->value = option_values->value;
               option_v1_values->label = option_values->label;

               option_values++;
               option_v1_values++;
            }
         }

#ifndef HAVE_NO_LANGEXTRA
         if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
             (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH) &&
             options_intl[language])
            option_defs_intl = options_intl[language]->definitions;

         if (option_defs_intl)
         {
            /* Determine number of intl options */
            while (true)
            {
               if (option_defs_intl[num_options_intl].key)
                  num_options_intl++;
               else
                  break;
            }

            /* Allocate intl array */
            option_v1_defs_intl = (struct retro_core_option_definition *)
                  calloc(num_options_intl + 1, sizeof(struct retro_core_option_definition));

            /* Copy parameters from option_defs_intl array */
            for (i = 0; i < num_options_intl; i++)
            {
               struct retro_core_option_v2_definition *option_def_intl = &option_defs_intl[i];
               struct retro_core_option_value *option_values           = option_def_intl->values;
               struct retro_core_option_definition *option_v1_def_intl = &option_v1_defs_intl[i];
               struct retro_core_option_value *option_v1_values        = option_v1_def_intl->values;

               option_v1_def_intl->key           = option_def_intl->key;
               option_v1_def_intl->desc          = option_def_intl->desc;
               option_v1_def_intl->info          = option_def_intl->info;
               option_v1_def_intl->default_value = option_def_intl->default_value;

               /* Values must be copied individually... */
               while (option_values->value)
               {
                  option_v1_values->value = option_values->value;
                  option_v1_values->label = option_values->label;

                  option_values++;
                  option_v1_values++;
               }
            }
         }

         core_options_v1_intl.us    = option_v1_defs_us;
         core_options_v1_intl.local = option_v1_defs_intl;

         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_v1_intl);
#else
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);
#endif
      }
      else
      {
         /* Allocate arrays */
         variables  = (struct retro_variable *)calloc(num_options + 1,
               sizeof(struct retro_variable));
         values_buf = (char **)calloc(num_options, sizeof(char *));

         if (!variables || !values_buf)
            goto error;

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            const char *key                        = option_defs_us[i].key;
            const char *desc                       = option_defs_us[i].desc;
            const char *default_value              = option_defs_us[i].default_value;
            struct retro_core_option_value *values = option_defs_us[i].values;
            size_t buf_len                         = 3;
            size_t default_index                   = 0;

            values_buf[i] = NULL;

            if (desc)
            {
               size_t num_values = 0;

               /* Determine number of values */
               while (true)
               {
                  if (values[num_values].value)
                  {
                     /* Check if this is the default value */
                     if (default_value)
                        if (strcmp(values[num_values].value, default_value) == 0)
                           default_index = num_values;

                     buf_len += strlen(values[num_values].value);
                     num_values++;
                  }
                  else
                     break;
               }

               /* Build values string */
               if (num_values > 0)
               {
                  buf_len += num_values - 1;
                  buf_len += strlen(desc);

                  values_buf[i] = (char *)calloc(buf_len, sizeof(char));
                  if (!values_buf[i])
                     goto error;

                  strcpy(values_buf[i], desc);
                  strcat(values_buf[i], "; ");

                  /* Default value goes first */
                  strcat(values_buf[i], values[default_index].value);

                  /* Add remaining values */
                  for (j = 0; j < num_values; j++)
                  {
                     if (j != default_index)
                     {
                        strcat(values_buf[i], "|");
                        strcat(values_buf[i], values[j].value);
                     }
                  }
               }
            }

            variables[option_index].key   = key;
            variables[option_index].value = values_buf[i];
            option_index++;
         }

         /* Set variables */
         environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
      }

error:
      /* Clean up */

      if (option_v1_defs_us)
      {
         free(option_v1_defs_us);
         option_v1_defs_us = NULL;
      }

#ifndef HAVE_NO_LANGEXTRA
      if (option_v1_defs_intl)
      {
         free(option_v1_defs_intl);
         option_v1_defs_intl = NULL;
      }
#endif

      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
