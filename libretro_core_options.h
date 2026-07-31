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
   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_us[] = {
   {
      "wswan_language",
      "Language (Restart Required)",
      NULL,
      "Set the console language stored in the internal EEPROM. The only game known to check it is 'Digimon Tamers - Battle Spirit'. Takes effect when content is loaded.",
      NULL,
      NULL,
      {
         { "english",  "English" },
         { "japanese", "Japanese" },
         { NULL, NULL },
      },
      "english",
   },
   {
      "wswan_name_source",
      "Owner Name Source (Restart Required)",
      NULL,
      "Choose the owner name written into the internal EEPROM. Several games greet the player with it. 'Frontend Username' uses the name configured in the frontend when one is available. Takes effect when content is loaded.",
      NULL,
      NULL,
      {
         { "frontend", "Frontend Username" },
         { "default",  "Default (Mednafen)" },
         { NULL, NULL },
      },
      "frontend",
   },
   {
      "wswan_birth_year",
      "Owner Birth Year (Restart Required)",
      NULL,
      "Set the owner birth year stored in the internal EEPROM. Some games trigger events on the owner's birthday. Takes effect when content is loaded.",
      NULL,
      NULL,
      {
         { "1920", NULL },
         { "1921", NULL },
         { "1922", NULL },
         { "1923", NULL },
         { "1924", NULL },
         { "1925", NULL },
         { "1926", NULL },
         { "1927", NULL },
         { "1928", NULL },
         { "1929", NULL },
         { "1930", NULL },
         { "1931", NULL },
         { "1932", NULL },
         { "1933", NULL },
         { "1934", NULL },
         { "1935", NULL },
         { "1936", NULL },
         { "1937", NULL },
         { "1938", NULL },
         { "1939", NULL },
         { "1940", NULL },
         { "1941", NULL },
         { "1942", NULL },
         { "1943", NULL },
         { "1944", NULL },
         { "1945", NULL },
         { "1946", NULL },
         { "1947", NULL },
         { "1948", NULL },
         { "1949", NULL },
         { "1950", NULL },
         { "1951", NULL },
         { "1952", NULL },
         { "1953", NULL },
         { "1954", NULL },
         { "1955", NULL },
         { "1956", NULL },
         { "1957", NULL },
         { "1958", NULL },
         { "1959", NULL },
         { "1960", NULL },
         { "1961", NULL },
         { "1962", NULL },
         { "1963", NULL },
         { "1964", NULL },
         { "1965", NULL },
         { "1966", NULL },
         { "1967", NULL },
         { "1968", NULL },
         { "1969", NULL },
         { "1970", NULL },
         { "1971", NULL },
         { "1972", NULL },
         { "1973", NULL },
         { "1974", NULL },
         { "1975", NULL },
         { "1976", NULL },
         { "1977", NULL },
         { "1978", NULL },
         { "1979", NULL },
         { "1980", NULL },
         { "1981", NULL },
         { "1982", NULL },
         { "1983", NULL },
         { "1984", NULL },
         { "1985", NULL },
         { "1986", NULL },
         { "1987", NULL },
         { "1988", NULL },
         { "1989", NULL },
         { "1990", NULL },
         { "1991", NULL },
         { "1992", NULL },
         { "1993", NULL },
         { "1994", NULL },
         { "1995", NULL },
         { "1996", NULL },
         { "1997", NULL },
         { "1998", NULL },
         { "1999", NULL },
         { "2000", NULL },
         { "2001", NULL },
         { "2002", NULL },
         { "2003", NULL },
         { "2004", NULL },
         { "2005", NULL },
         { "2006", NULL },
         { "2007", NULL },
         { "2008", NULL },
         { "2009", NULL },
         { "2010", NULL },
         { "2011", NULL },
         { "2012", NULL },
         { "2013", NULL },
         { "2014", NULL },
         { "2015", NULL },
         { "2016", NULL },
         { "2017", NULL },
         { "2018", NULL },
         { "2019", NULL },
         { "2020", NULL },
         { "2021", NULL },
         { "2022", NULL },
         { "2023", NULL },
         { "2024", NULL },
         { "2025", NULL },
         { "2026", NULL },
         { "2027", NULL },
         { "2028", NULL },
         { "2029", NULL },
         { NULL, NULL },
      },
      "1989",
   },
   {
      "wswan_birth_month",
      "Owner Birth Month (Restart Required)",
      NULL,
      "Set the owner birth month stored in the internal EEPROM. Takes effect when content is loaded.",
      NULL,
      NULL,
      {
         { "1", "January" },
         { "2", "February" },
         { "3", "March" },
         { "4", "April" },
         { "5", "May" },
         { "6", "June" },
         { "7", "July" },
         { "8", "August" },
         { "9", "September" },
         { "10", "October" },
         { "11", "November" },
         { "12", "December" },
         { NULL, NULL },
      },
      "6",
   },
   {
      "wswan_birth_day",
      "Owner Birth Day (Restart Required)",
      NULL,
      "Set the owner birth day stored in the internal EEPROM. Takes effect when content is loaded.",
      NULL,
      NULL,
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
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { "16", NULL },
         { "17", NULL },
         { "18", NULL },
         { "19", NULL },
         { "20", NULL },
         { "21", NULL },
         { "22", NULL },
         { "23", NULL },
         { "24", NULL },
         { "25", NULL },
         { "26", NULL },
         { "27", NULL },
         { "28", NULL },
         { "29", NULL },
         { "30", NULL },
         { "31", NULL },
         { NULL, NULL },
      },
      "23",
   },
   {
      "wswan_sex",
      "Owner Sex (Restart Required)",
      NULL,
      "Set the owner sex stored in the internal EEPROM. Takes effect when content is loaded.",
      NULL,
      NULL,
      {
         { "female", "Female" },
         { "male",   "Male" },
         { NULL, NULL },
      },
      "female",
   },
   {
      "wswan_blood",
      "Owner Blood Type (Restart Required)",
      NULL,
      "Set the owner blood type stored in the internal EEPROM. Takes effect when content is loaded.",
      NULL,
      NULL,
      {
         { "o",  "O" },
         { "a",  "A" },
         { "b",  "B" },
         { "ab", "AB" },
         { NULL, NULL },
      },
      "o",
   },
   {
      "wswan_rotate_display",
      "Display Rotation",
      NULL,
      "Choose the orientation of the console screen. 'Portrait' can be used to achieve the correct layout of portrait oriented games on a conventional (landscape) display. 'Manual' enables rotation via the controller (default button: SELECT).",
      NULL,
      NULL,
      {
         { "manual",    "Manual" },
         { "landscape", "Landscape" },
         { "portrait",  "Portrait" },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      "Rotate Button Mappings",
      NULL,
      "Choose the preferred button mapping. 'Auto' adjusts button mapping to match the current display rotation.",
      NULL,
      NULL,
      {
         { "auto",     "Auto" },
         { "disabled", "Landscape" },
         { "enabled",  "Portrait" },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      "Color Palette",
      NULL,
      "Choose the colorization of WonderSwan (Mono) games. 'WS' palettes mimic the display of the original WonderSwan hardware.",
      NULL,
      NULL,
      {
         { "default",                "Grayscale" },
         { "wonderswan",             "WS - WonderSwan" },
         { "wondeswan_color",        "WS - WonderSwan Color" },
         { "swancrystal",            "WS - SwanCrystal" },
         { "gb_dmg",                 "Game Boy DMG" },
         { "gb_pocket",              "Game Boy Pocket" },
         { "gb_light",               "Game Boy Light" },
         { "blossom_pink",           "Blossom Pink" },
         { "bubbles_blue",           "Bubbles Blue" },
         { "buttercup_green",        "Buttercup Green" },
         { "digivice",               "Digivice" },
         { "game_com",               "Game.com" },
         { "gameking",               "GameKing" },
         { "game_master",            "Game Master" },
         { "golden_wild",            "Golden Wild" },
         { "greenscale",             "Greenscale" },
         { "hokage_orange",          "Hokage Orange" },
         { "labo_fawn",              "Labo Fawn" },
         { "legendary_super_saiyan", "Legendary Super Saiyan" },
         { "microvision",            "Microvision" },
         { "million_live_gold",      "Million Live Gold" },
         { "odyssey_gold",           "Odyssey Gold" },
         { "shiny_sky_blue",         "Shiny Sky Blue" },
         { "slime_blue",             "Slime Blue" },
         { "ti_83",                  "TI-83" },
         { "travel_wood",            "Travel Wood" },
         { "virtual_boy",            "Virtual Boy" },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      "Color Depth (Restart Required)",
      NULL,
      "24-bit is slower and not available on all platforms.",
      NULL,
      NULL,
      {
         { "16bit", "Thousands (16-bit)" },
         { "24bit", "Millions (24-bit)" },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      "Frameskip",
      NULL,
      "Skip frames to avoid audio buffer under-run (crackling). Improves performance at the expense of visual smoothness. 'Auto' skips frames when advised by the frontend. 'Manual' utilizes the 'Frameskip Threshold (%)' setting.",
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     "Auto" },
         { "manual",   "Manual" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      "Frameskip Threshold (%)",
      NULL,
      "When 'Frameskip' is set to 'Manual', this specifies the audio buffer occupancy threshold (percentage) below which frames will be skipped. Higher values reduce the risk of crackling by causing frames to be dropped more frequently.",
      NULL,
      NULL,
      {
         { "15", NULL },
         { "18", NULL },
         { "21", NULL },
         { "24", NULL },
         { "27", NULL },
         { "30", NULL },
         { "33", NULL },
         { "36", NULL },
         { "39", NULL },
         { "42", NULL },
         { "45", NULL },
         { "48", NULL },
         { "51", NULL },
         { "54", NULL },
         { "57", NULL },
         { "60", NULL },
         { NULL, NULL },
      },
      "33"
   },
   {
      "wswan_60hz_mode",
      "60Hz Mode",
      NULL,
      "Update the display at 60Hz instead of the native 75Hz WonderSwan refresh rate by dropping every fifth frame. Reduces video smoothness, but avoids screen tearing on displays that do not support native operation above 60Hz.",
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      "wswan_sound_sample_rate",
      "Sound Output Sample Rate",
      NULL,
      "Higher values improve sound quality slightly at the expense of performance.",
      NULL,
      NULL,
      {
         { "11025",  NULL },
         { "22050",  NULL },
         { "44100",  NULL },
         { "48000",  NULL },
         { NULL, NULL },
      },
      "44100",
   },
   {
      "wswan_sound_low_pass",
      "Audio Filter",
      NULL,
      "Apply a low pass audio filter to 'soften' the sometimes harsh chiptunes produced by the WonderSwan.",
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
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
   &options_ja,      /* RETRO_LANGUAGE_JAPANESE */
   &options_fr,      /* RETRO_LANGUAGE_FRENCH */
   &options_es,      /* RETRO_LANGUAGE_SPANISH */
   &options_de,      /* RETRO_LANGUAGE_GERMAN */
   &options_it,      /* RETRO_LANGUAGE_ITALIAN */
   &options_nl,      /* RETRO_LANGUAGE_DUTCH */
   &options_pt_br,   /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   &options_pt_pt,   /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   &options_ru,      /* RETRO_LANGUAGE_RUSSIAN */
   &options_ko,      /* RETRO_LANGUAGE_KOREAN */
   &options_cht,     /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   &options_chs,     /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   &options_eo,      /* RETRO_LANGUAGE_ESPERANTO */
   &options_pl,      /* RETRO_LANGUAGE_POLISH */
   &options_vn,      /* RETRO_LANGUAGE_VIETNAMESE */
   &options_ar,      /* RETRO_LANGUAGE_ARABIC */
   &options_el,      /* RETRO_LANGUAGE_GREEK */
   &options_tr,      /* RETRO_LANGUAGE_TURKISH */
   &options_sk,      /* RETRO_LANGUAGE_SLOVAK */
   &options_fa,      /* RETRO_LANGUAGE_PERSIAN */
   &options_he,      /* RETRO_LANGUAGE_HEBREW */
   &options_ast,     /* RETRO_LANGUAGE_ASTURIAN */
   &options_fi,      /* RETRO_LANGUAGE_FINNISH */
   &options_id,      /* RETRO_LANGUAGE_INDONESIAN */
   &options_sv,      /* RETRO_LANGUAGE_SWEDISH */
   &options_uk,      /* RETRO_LANGUAGE_UKRAINIAN */
   &options_cs,      /* RETRO_LANGUAGE_CZECH */
   &options_val,     /* RETRO_LANGUAGE_CATALAN_VALENCIA */
   &options_ca,      /* RETRO_LANGUAGE_CATALAN */
   &options_en,      /* RETRO_LANGUAGE_BRITISH_ENGLISH */
   &options_hu,      /* RETRO_LANGUAGE_HUNGARIAN */
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
