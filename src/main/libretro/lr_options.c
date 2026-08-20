/***************************************************************************
   Interface for notifying Libretro frontend of configuration
   value changes.

   See license.txt for more details.
***************************************************************************/

/* Required for snprintf() on some platforms */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <array/rbuf.h>

#include "lr_options.h"
#include "frontend/config.h"

extern retro_environment_t environ_cb;

typedef bool (*lr_options_val_to_str_t)(int val, char *str, size_t len);

typedef struct
{
   char *key;
   uintptr_t addr;
   lr_options_val_to_str_t val_to_str;
} lr_options_entry_t;

static bool set_var_enabled               = false;
static lr_options_entry_t *option_entries = NULL;

static bool val_to_str_bool(int val, char *str, size_t len)
{
   if (!str || (len == 0))
      return false;

   if (val == 0)
      strlcpy(str, "OFF", len);
   else
      strlcpy(str, "ON", len);

   return true;
}

static bool val_to_str_uint(int val, char *str, size_t len)
{
   if (!str || (len == 0) || (val < 0))
      return false;

   snprintf(str, len, "%d", val);
   return true;
}

static bool val_to_str_fps(int val, char *str, size_t len)
{
   if (!str || (len == 0))
      return false;

   switch (val)
   {
      case 0:
         strlcpy(str, "Low (30)", len);
         break;
      case 1:
         strlcpy(str, "Original (60/30)", len);
         break;
      case 2:
         strlcpy(str, "Smooth (60)", len);
         break;
      case 3:
         strlcpy(str, "Ultra Smooth (120)", len);
         break;
      default:
         return false;
   }

   return true;
}

static bool val_to_str_gear(int val, char *str, size_t len)
{
   if (!str || (len == 0))
      return false;

   switch (val)
   {
      case 0:
         strlcpy(str, "Manual", len);
         break;
      case 1:
         strlcpy(str, "Manual Cabinet", len);
         break;
      case 2:
         strlcpy(str, "Manual 2 Buttons", len);
         break;
      case 3:
         strlcpy(str, "Automatic", len);
         break;
      default:
         return false;
   }

   return true;
}

static bool val_to_str_dip_time(int val, char *str, size_t len)
{
   if (!str || (len == 0))
      return false;

   switch (val)
   {
      case 0:
         strlcpy(str, "Easy (80s)", len);
         break;
      case 1:
         strlcpy(str, "Normal (75s)", len);
         break;
      case 2:
         strlcpy(str, "Hard (72s)", len);
         break;
      case 3:
         strlcpy(str, "Very Hard (70s)", len);
         break;
      case 4:
         strlcpy(str, "Infinite Time", len);
         break;
      default:
         return false;
   }

   return true;
}

static bool val_to_str_dip_traffic(int val, char *str, size_t len)
{
   if (!str || (len == 0))
      return false;

   switch (val)
   {
      case 0:
         strlcpy(str, "Easy", len);
         break;
      case 1:
         strlcpy(str, "Normal", len);
         break;
      case 2:
         strlcpy(str, "Hard", len);
         break;
      case 3:
         strlcpy(str, "Very Hard", len);
         break;
      case 4:
         strlcpy(str, "No Traffic", len);
         break;
      default:
         return false;
   }

   return true;
}

static bool val_to_str_car_color(int val, char *str, size_t len)
{
   if (!str || (len == 0))
      return false;

   switch (val)
   {
      case 0:
         strlcpy(str, "RED", len);
         break;
      case 1:
         strlcpy(str, "BLUE", len);
         break;
      case 2:
         strlcpy(str, "YELLOW", len);
         break;
      case 3:
         strlcpy(str, "GREEN", len);
         break;
      case 4:
         strlcpy(str, "CYAN", len);
         break;
      case 5:
         strlcpy(str, "BLACK", len);
         break;
      case 6:
         strlcpy(str, "WHITE", len);
         break;
      default:
         return false;
   }

   return true;
}

static void add_entry(const char *key, uintptr_t addr, lr_options_val_to_str_t val_to_str)
{
   lr_options_entry_t *entry = NULL;
   { size_t num_entries;

   if (!key ||
       (*key == '\0') ||
       !addr ||
       !val_to_str)
      return;

   /* Get current number of entries */
   num_entries = RBUF_LEN(option_entries);

   /* Attempt to allocate memory for new entry */
   if (!RBUF_TRYFIT(option_entries, num_entries + 1))
      return;

   /* Allocation successful - increment array size */
   RBUF_RESIZE(option_entries, num_entries + 1);

   /* Get handle of new entry at end of list, and
    * zero-initialise members */
   entry = &option_entries[num_entries];
   memset(entry, 0, sizeof(*entry));

   /* Assign members */
   entry->key        = strdup(key);
   entry->addr       = addr;
   entry->val_to_str = val_to_str;
 }}

void lr_options_init()
{
   uintptr_t val_addr;

   set_var_enabled = false;
   option_entries  = NULL;
      
   if (environ_cb && environ_cb(RETRO_ENVIRONMENT_SET_VARIABLE, NULL))
      set_var_enabled = true;

   if (!set_var_enabled)
      return;

   add_entry("cannonball_video_fps",
         (uintptr_t)(void*)&config.video.fps,
         val_to_str_fps);

   add_entry("cannonball_video_widescreen",
         (uintptr_t)(void*)&config.video.widescreen,
         val_to_str_bool);

   add_entry("cannonball_video_hires",
         (uintptr_t)(void*)&config.video.hires,
         val_to_str_bool);

   add_entry("cannonball_sound_enable",
         (uintptr_t)(void*)&config.sound.enabled,
         val_to_str_bool);

   add_entry("cannonball_gear",
         (uintptr_t)(void*)&config.controls.gear,
         val_to_str_gear);

   add_entry("cannonball_analog",
         (uintptr_t)(void*)&config.controls.analog,
         val_to_str_bool);

   add_entry("cannonball_freeplay",
         (uintptr_t)(void*)&config.engine.freeplay,
         val_to_str_bool);
   
   add_entry("cannonball_force_ai",
         (uintptr_t)(void*)&config.engine.force_ai,
         val_to_str_bool);

   add_entry("cannonball_grippy_tyres",
         (uintptr_t)(void*)&config.engine.grippy_tyres,
         val_to_str_bool);

   add_entry("cannonball_offroad_tyres",
         (uintptr_t)(void*)&config.engine.offroad,
         val_to_str_bool);

   add_entry("cannonball_strong_bumper",
         (uintptr_t)(void*)&config.engine.bumper,
         val_to_str_bool);

   add_entry("cannonball_faster_car",
         (uintptr_t)(void*)&config.engine.turbo,
         val_to_str_bool);

   add_entry("cannonball_car_color",
         (uintptr_t)(void*)&config.engine.car_pal,
         val_to_str_car_color);

   add_entry("cannonball_sound_advertise",
         (uintptr_t)(void*)&config.sound.advertise,
         val_to_str_bool);

   add_entry("cannonball_sound_preview",
         (uintptr_t)(void*)&config.sound.preview,
         val_to_str_bool);

   add_entry("cannonball_sound_fix_samples",
         (uintptr_t)(void*)&config.sound.fix_samples,
         val_to_str_bool);

   add_entry("cannonball_steer_speed",
         (uintptr_t)(void*)&config.controls.steer_speed,
         val_to_str_uint);

   add_entry("cannonball_pedal_speed",
         (uintptr_t)(void*)&config.controls.pedal_speed,
         val_to_str_uint);

   add_entry("cannonball_jap",
         (uintptr_t)(void*)&config.engine.jap,
         val_to_str_bool);

   add_entry("cannonball_dip_time",
         (uintptr_t)(void*)&config.engine.dip_time,
         val_to_str_dip_time);

   add_entry("cannonball_dip_traffic",
         (uintptr_t)(void*)&config.engine.dip_traffic,
         val_to_str_dip_traffic);

   add_entry("cannonball_level_objects",
         (uintptr_t)(void*)&config.engine.level_objects,
         val_to_str_bool);

   add_entry("cannonball_prototype",
         (uintptr_t)(void*)&config.engine.prototype,
         val_to_str_bool);

   add_entry("cannonball_new_attract",
         (uintptr_t)(void*)&config.engine.new_attract,
         val_to_str_bool);

   add_entry("cannonball_ttrial_laps",
         (uintptr_t)(void*)&config.ttrial.laps,
         val_to_str_uint);

   add_entry("cannonball_ttrial_traffic",
         (uintptr_t)(void*)&config.ttrial.traffic,
         val_to_str_uint);

   add_entry("cannonball_cont_traffic",
         (uintptr_t)(void*)&config.cont_traffic,
         val_to_str_uint);

   add_entry("cannonball_fix_bugs",
         (uintptr_t)(void*)&config.engine.fix_bugs,
         val_to_str_bool);
}

void lr_options_close()
{
   size_t i;

   set_var_enabled = false;

   if (!option_entries)
      return;

   for (i = 0; i < RBUF_LEN(option_entries); i++)
   {
      lr_options_entry_t *entry = &option_entries[i];

      if (entry && entry->key)
         free(entry->key);
   }

   RBUF_FREE(option_entries);
}

static void set_variable(uintptr_t addr, int val)
{
   lr_options_entry_t *entry = NULL;
   struct retro_variable var = {0};
   { char val_str[32];
   size_t i;

   val_str[0] = '\0';

   if (!environ_cb ||
       !set_var_enabled ||
       !option_entries)
      return;

   /* Find entry associated with specified address */
   for (i = 0; i < RBUF_LEN(option_entries); i++)
   {
      lr_options_entry_t *current_entry = &option_entries[i];

      if (current_entry->addr == addr)
      {
         entry = current_entry;
         break;
      }
   }

   if (!entry ||
       !entry->key ||
       (*entry->key == '\0') ||
       !entry->val_to_str)
      return;

   /* Get string representation of value */
   if (!entry->val_to_str(val, val_str, sizeof(val_str)))
      return;

   /* Notify frontend of value change */
   var.key   = entry->key;
   var.value = val_str;
   environ_cb(RETRO_ENVIRONMENT_SET_VARIABLE, &var);
 }}

void lr_options_set_frontend_variable_bool(const bool *config_var)
{
   uintptr_t addr;
   { int val;

   if (!config_var)
      return;

   addr = (uintptr_t)(void*)config_var;
   val  = *config_var ? 1 : 0;

   set_variable(addr, val);
 }}

void lr_options_set_frontend_variable_int(const int *config_var)
{
   uintptr_t addr;
   { int val;

   if (!config_var)
      return;

   addr = (uintptr_t)(void*)config_var;
   val  = *config_var;

   set_variable(addr, val);
 }}

