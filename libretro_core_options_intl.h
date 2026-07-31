#ifndef LIBRETRO_CORE_OPTIONS_INTL_H__
#define LIBRETRO_CORE_OPTIONS_INTL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning(disable:4566)
#endif

#include <libretro.h>

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
/* RETRO_LANGUAGE_AR */

#define WSWAN_ROTATE_DISPLAY_LABEL_AR NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_AR NULL
#define OPTION_VAL_MANUAL_AR "يدوي"
#define OPTION_VAL_LANDSCAPE_AR NULL
#define OPTION_VAL_PORTRAIT_AR NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_AR NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_AR NULL
#define OPTION_VAL_AUTO_AR "تلقائي"
#define WSWAN_MONO_PALETTE_LABEL_AR NULL
#define WSWAN_MONO_PALETTE_INFO_0_AR NULL
#define OPTION_VAL_DEFAULT_AR NULL
#define OPTION_VAL_WONDERSWAN_AR NULL
#define OPTION_VAL_WONDESWAN_COLOR_AR NULL
#define OPTION_VAL_SWANCRYSTAL_AR NULL
#define OPTION_VAL_GB_DMG_AR NULL
#define OPTION_VAL_GB_POCKET_AR NULL
#define OPTION_VAL_GB_LIGHT_AR NULL
#define OPTION_VAL_BLOSSOM_PINK_AR NULL
#define OPTION_VAL_BUBBLES_BLUE_AR NULL
#define OPTION_VAL_BUTTERCUP_GREEN_AR NULL
#define OPTION_VAL_DIGIVICE_AR NULL
#define OPTION_VAL_GAME_COM_AR NULL
#define OPTION_VAL_GAMEKING_AR NULL
#define OPTION_VAL_GAME_MASTER_AR NULL
#define OPTION_VAL_GOLDEN_WILD_AR NULL
#define OPTION_VAL_GREENSCALE_AR NULL
#define OPTION_VAL_HOKAGE_ORANGE_AR NULL
#define OPTION_VAL_LABO_FAWN_AR NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_AR NULL
#define OPTION_VAL_MICROVISION_AR NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_AR NULL
#define OPTION_VAL_ODYSSEY_GOLD_AR NULL
#define OPTION_VAL_SHINY_SKY_BLUE_AR NULL
#define OPTION_VAL_SLIME_BLUE_AR NULL
#define OPTION_VAL_TI_83_AR NULL
#define OPTION_VAL_TRAVEL_WOOD_AR NULL
#define OPTION_VAL_VIRTUAL_BOY_AR NULL
#define WSWAN_GFX_COLORS_LABEL_AR NULL
#define WSWAN_GFX_COLORS_INFO_0_AR NULL
#define OPTION_VAL_16BIT_AR NULL
#define OPTION_VAL_24BIT_AR NULL
#define WSWAN_FRAMESKIP_LABEL_AR NULL
#define WSWAN_FRAMESKIP_INFO_0_AR NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_AR NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_AR NULL
#define WSWAN_60HZ_MODE_LABEL_AR NULL
#define WSWAN_60HZ_MODE_INFO_0_AR NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_AR NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_AR NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_AR "تصفية الصوت"
#define WSWAN_SOUND_LOW_PASS_INFO_0_AR NULL

struct retro_core_option_v2_category option_cats_ar[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ar[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_AR,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_AR,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_AR },
         { "landscape", OPTION_VAL_LANDSCAPE_AR },
         { "portrait",  OPTION_VAL_PORTRAIT_AR },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_AR,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_AR,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_AR },
         { "disabled", OPTION_VAL_LANDSCAPE_AR },
         { "enabled",  OPTION_VAL_PORTRAIT_AR },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_AR,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_AR,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_AR },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_AR },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_AR },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_AR },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_AR },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_AR },
         { "gb_light",               OPTION_VAL_GB_LIGHT_AR },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_AR },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_AR },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_AR },
         { "digivice",               OPTION_VAL_DIGIVICE_AR },
         { "game_com",               OPTION_VAL_GAME_COM_AR },
         { "gameking",               OPTION_VAL_GAMEKING_AR },
         { "game_master",            OPTION_VAL_GAME_MASTER_AR },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_AR },
         { "greenscale",             OPTION_VAL_GREENSCALE_AR },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_AR },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_AR },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_AR },
         { "microvision",            OPTION_VAL_MICROVISION_AR },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_AR },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_AR },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_AR },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_AR },
         { "ti_83",                  OPTION_VAL_TI_83_AR },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_AR },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_AR },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_AR,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_AR,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_AR },
         { "24bit", OPTION_VAL_24BIT_AR },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_AR,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_AR,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_AR },
         { "manual",   OPTION_VAL_MANUAL_AR },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_AR,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_AR,
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
      WSWAN_60HZ_MODE_LABEL_AR,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_AR,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_AR,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_AR,
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
      WSWAN_SOUND_LOW_PASS_LABEL_AR,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_AR,
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
struct retro_core_options_v2 options_ar = {
   option_cats_ar,
   option_defs_ar
};

/* RETRO_LANGUAGE_AST */

#define WSWAN_ROTATE_DISPLAY_LABEL_AST NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_AST NULL
#define OPTION_VAL_MANUAL_AST NULL
#define OPTION_VAL_LANDSCAPE_AST NULL
#define OPTION_VAL_PORTRAIT_AST NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_AST NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_AST NULL
#define OPTION_VAL_AUTO_AST NULL
#define WSWAN_MONO_PALETTE_LABEL_AST NULL
#define WSWAN_MONO_PALETTE_INFO_0_AST NULL
#define OPTION_VAL_DEFAULT_AST NULL
#define OPTION_VAL_WONDERSWAN_AST NULL
#define OPTION_VAL_WONDESWAN_COLOR_AST NULL
#define OPTION_VAL_SWANCRYSTAL_AST NULL
#define OPTION_VAL_GB_DMG_AST NULL
#define OPTION_VAL_GB_POCKET_AST NULL
#define OPTION_VAL_GB_LIGHT_AST NULL
#define OPTION_VAL_BLOSSOM_PINK_AST NULL
#define OPTION_VAL_BUBBLES_BLUE_AST NULL
#define OPTION_VAL_BUTTERCUP_GREEN_AST NULL
#define OPTION_VAL_DIGIVICE_AST NULL
#define OPTION_VAL_GAME_COM_AST NULL
#define OPTION_VAL_GAMEKING_AST NULL
#define OPTION_VAL_GAME_MASTER_AST NULL
#define OPTION_VAL_GOLDEN_WILD_AST NULL
#define OPTION_VAL_GREENSCALE_AST NULL
#define OPTION_VAL_HOKAGE_ORANGE_AST NULL
#define OPTION_VAL_LABO_FAWN_AST NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_AST NULL
#define OPTION_VAL_MICROVISION_AST NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_AST NULL
#define OPTION_VAL_ODYSSEY_GOLD_AST NULL
#define OPTION_VAL_SHINY_SKY_BLUE_AST NULL
#define OPTION_VAL_SLIME_BLUE_AST NULL
#define OPTION_VAL_TI_83_AST NULL
#define OPTION_VAL_TRAVEL_WOOD_AST NULL
#define OPTION_VAL_VIRTUAL_BOY_AST NULL
#define WSWAN_GFX_COLORS_LABEL_AST NULL
#define WSWAN_GFX_COLORS_INFO_0_AST NULL
#define OPTION_VAL_16BIT_AST "Miles (16 bits)"
#define OPTION_VAL_24BIT_AST "Millones (24 bits)"
#define WSWAN_FRAMESKIP_LABEL_AST NULL
#define WSWAN_FRAMESKIP_INFO_0_AST NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_AST NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_AST NULL
#define WSWAN_60HZ_MODE_LABEL_AST NULL
#define WSWAN_60HZ_MODE_INFO_0_AST NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_AST NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_AST NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_AST NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_AST NULL

struct retro_core_option_v2_category option_cats_ast[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ast[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_AST,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_AST,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_AST },
         { "landscape", OPTION_VAL_LANDSCAPE_AST },
         { "portrait",  OPTION_VAL_PORTRAIT_AST },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_AST,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_AST,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_AST },
         { "disabled", OPTION_VAL_LANDSCAPE_AST },
         { "enabled",  OPTION_VAL_PORTRAIT_AST },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_AST,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_AST,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_AST },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_AST },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_AST },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_AST },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_AST },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_AST },
         { "gb_light",               OPTION_VAL_GB_LIGHT_AST },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_AST },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_AST },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_AST },
         { "digivice",               OPTION_VAL_DIGIVICE_AST },
         { "game_com",               OPTION_VAL_GAME_COM_AST },
         { "gameking",               OPTION_VAL_GAMEKING_AST },
         { "game_master",            OPTION_VAL_GAME_MASTER_AST },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_AST },
         { "greenscale",             OPTION_VAL_GREENSCALE_AST },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_AST },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_AST },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_AST },
         { "microvision",            OPTION_VAL_MICROVISION_AST },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_AST },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_AST },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_AST },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_AST },
         { "ti_83",                  OPTION_VAL_TI_83_AST },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_AST },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_AST },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_AST,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_AST,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_AST },
         { "24bit", OPTION_VAL_24BIT_AST },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_AST,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_AST,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_AST },
         { "manual",   OPTION_VAL_MANUAL_AST },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_AST,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_AST,
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
      WSWAN_60HZ_MODE_LABEL_AST,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_AST,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_AST,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_AST,
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
      WSWAN_SOUND_LOW_PASS_LABEL_AST,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_AST,
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
struct retro_core_options_v2 options_ast = {
   option_cats_ast,
   option_defs_ast
};

/* RETRO_LANGUAGE_BE */

#define WSWAN_ROTATE_DISPLAY_LABEL_BE NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_BE NULL
#define OPTION_VAL_MANUAL_BE NULL
#define OPTION_VAL_LANDSCAPE_BE NULL
#define OPTION_VAL_PORTRAIT_BE NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_BE NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_BE NULL
#define OPTION_VAL_AUTO_BE "Аўта"
#define WSWAN_MONO_PALETTE_LABEL_BE NULL
#define WSWAN_MONO_PALETTE_INFO_0_BE NULL
#define OPTION_VAL_DEFAULT_BE NULL
#define OPTION_VAL_WONDERSWAN_BE NULL
#define OPTION_VAL_WONDESWAN_COLOR_BE NULL
#define OPTION_VAL_SWANCRYSTAL_BE NULL
#define OPTION_VAL_GB_DMG_BE NULL
#define OPTION_VAL_GB_POCKET_BE NULL
#define OPTION_VAL_GB_LIGHT_BE NULL
#define OPTION_VAL_BLOSSOM_PINK_BE NULL
#define OPTION_VAL_BUBBLES_BLUE_BE NULL
#define OPTION_VAL_BUTTERCUP_GREEN_BE NULL
#define OPTION_VAL_DIGIVICE_BE NULL
#define OPTION_VAL_GAME_COM_BE NULL
#define OPTION_VAL_GAMEKING_BE NULL
#define OPTION_VAL_GAME_MASTER_BE NULL
#define OPTION_VAL_GOLDEN_WILD_BE NULL
#define OPTION_VAL_GREENSCALE_BE NULL
#define OPTION_VAL_HOKAGE_ORANGE_BE NULL
#define OPTION_VAL_LABO_FAWN_BE NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_BE NULL
#define OPTION_VAL_MICROVISION_BE NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_BE NULL
#define OPTION_VAL_ODYSSEY_GOLD_BE NULL
#define OPTION_VAL_SHINY_SKY_BLUE_BE NULL
#define OPTION_VAL_SLIME_BLUE_BE NULL
#define OPTION_VAL_TI_83_BE NULL
#define OPTION_VAL_TRAVEL_WOOD_BE NULL
#define OPTION_VAL_VIRTUAL_BOY_BE NULL
#define WSWAN_GFX_COLORS_LABEL_BE NULL
#define WSWAN_GFX_COLORS_INFO_0_BE NULL
#define OPTION_VAL_16BIT_BE NULL
#define OPTION_VAL_24BIT_BE NULL
#define WSWAN_FRAMESKIP_LABEL_BE NULL
#define WSWAN_FRAMESKIP_INFO_0_BE NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_BE NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_BE NULL
#define WSWAN_60HZ_MODE_LABEL_BE NULL
#define WSWAN_60HZ_MODE_INFO_0_BE NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_BE NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_BE NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_BE NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_BE NULL

struct retro_core_option_v2_category option_cats_be[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_be[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_BE,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_BE,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_BE },
         { "landscape", OPTION_VAL_LANDSCAPE_BE },
         { "portrait",  OPTION_VAL_PORTRAIT_BE },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_BE,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_BE,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_BE },
         { "disabled", OPTION_VAL_LANDSCAPE_BE },
         { "enabled",  OPTION_VAL_PORTRAIT_BE },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_BE,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_BE,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_BE },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_BE },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_BE },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_BE },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_BE },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_BE },
         { "gb_light",               OPTION_VAL_GB_LIGHT_BE },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_BE },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_BE },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_BE },
         { "digivice",               OPTION_VAL_DIGIVICE_BE },
         { "game_com",               OPTION_VAL_GAME_COM_BE },
         { "gameking",               OPTION_VAL_GAMEKING_BE },
         { "game_master",            OPTION_VAL_GAME_MASTER_BE },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_BE },
         { "greenscale",             OPTION_VAL_GREENSCALE_BE },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_BE },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_BE },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_BE },
         { "microvision",            OPTION_VAL_MICROVISION_BE },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_BE },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_BE },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_BE },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_BE },
         { "ti_83",                  OPTION_VAL_TI_83_BE },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_BE },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_BE },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_BE,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_BE,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_BE },
         { "24bit", OPTION_VAL_24BIT_BE },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_BE,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_BE,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_BE },
         { "manual",   OPTION_VAL_MANUAL_BE },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_BE,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_BE,
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
      WSWAN_60HZ_MODE_LABEL_BE,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_BE,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_BE,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_BE,
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
      WSWAN_SOUND_LOW_PASS_LABEL_BE,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_BE,
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
struct retro_core_options_v2 options_be = {
   option_cats_be,
   option_defs_be
};

/* RETRO_LANGUAGE_CA */

#define WSWAN_ROTATE_DISPLAY_LABEL_CA NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_CA NULL
#define OPTION_VAL_MANUAL_CA NULL
#define OPTION_VAL_LANDSCAPE_CA NULL
#define OPTION_VAL_PORTRAIT_CA NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_CA NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_CA NULL
#define OPTION_VAL_AUTO_CA "Automàtic"
#define WSWAN_MONO_PALETTE_LABEL_CA NULL
#define WSWAN_MONO_PALETTE_INFO_0_CA NULL
#define OPTION_VAL_DEFAULT_CA NULL
#define OPTION_VAL_WONDERSWAN_CA NULL
#define OPTION_VAL_WONDESWAN_COLOR_CA NULL
#define OPTION_VAL_SWANCRYSTAL_CA NULL
#define OPTION_VAL_GB_DMG_CA NULL
#define OPTION_VAL_GB_POCKET_CA NULL
#define OPTION_VAL_GB_LIGHT_CA NULL
#define OPTION_VAL_BLOSSOM_PINK_CA NULL
#define OPTION_VAL_BUBBLES_BLUE_CA NULL
#define OPTION_VAL_BUTTERCUP_GREEN_CA NULL
#define OPTION_VAL_DIGIVICE_CA NULL
#define OPTION_VAL_GAME_COM_CA NULL
#define OPTION_VAL_GAMEKING_CA NULL
#define OPTION_VAL_GAME_MASTER_CA NULL
#define OPTION_VAL_GOLDEN_WILD_CA NULL
#define OPTION_VAL_GREENSCALE_CA NULL
#define OPTION_VAL_HOKAGE_ORANGE_CA NULL
#define OPTION_VAL_LABO_FAWN_CA NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CA NULL
#define OPTION_VAL_MICROVISION_CA NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_CA NULL
#define OPTION_VAL_ODYSSEY_GOLD_CA NULL
#define OPTION_VAL_SHINY_SKY_BLUE_CA NULL
#define OPTION_VAL_SLIME_BLUE_CA NULL
#define OPTION_VAL_TI_83_CA NULL
#define OPTION_VAL_TRAVEL_WOOD_CA NULL
#define OPTION_VAL_VIRTUAL_BOY_CA NULL
#define WSWAN_GFX_COLORS_LABEL_CA NULL
#define WSWAN_GFX_COLORS_INFO_0_CA NULL
#define OPTION_VAL_16BIT_CA NULL
#define OPTION_VAL_24BIT_CA NULL
#define WSWAN_FRAMESKIP_LABEL_CA NULL
#define WSWAN_FRAMESKIP_INFO_0_CA NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_CA NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CA NULL
#define WSWAN_60HZ_MODE_LABEL_CA NULL
#define WSWAN_60HZ_MODE_INFO_0_CA NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_CA NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_CA NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_CA NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_CA NULL

struct retro_core_option_v2_category option_cats_ca[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ca[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_CA,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_CA,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_CA },
         { "landscape", OPTION_VAL_LANDSCAPE_CA },
         { "portrait",  OPTION_VAL_PORTRAIT_CA },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_CA,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_CA,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_CA },
         { "disabled", OPTION_VAL_LANDSCAPE_CA },
         { "enabled",  OPTION_VAL_PORTRAIT_CA },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_CA,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_CA,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_CA },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_CA },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_CA },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_CA },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_CA },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_CA },
         { "gb_light",               OPTION_VAL_GB_LIGHT_CA },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_CA },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_CA },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_CA },
         { "digivice",               OPTION_VAL_DIGIVICE_CA },
         { "game_com",               OPTION_VAL_GAME_COM_CA },
         { "gameking",               OPTION_VAL_GAMEKING_CA },
         { "game_master",            OPTION_VAL_GAME_MASTER_CA },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_CA },
         { "greenscale",             OPTION_VAL_GREENSCALE_CA },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_CA },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_CA },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CA },
         { "microvision",            OPTION_VAL_MICROVISION_CA },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_CA },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_CA },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_CA },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_CA },
         { "ti_83",                  OPTION_VAL_TI_83_CA },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_CA },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_CA },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_CA,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_CA,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_CA },
         { "24bit", OPTION_VAL_24BIT_CA },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_CA,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_CA,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_CA },
         { "manual",   OPTION_VAL_MANUAL_CA },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_CA,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CA,
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
      WSWAN_60HZ_MODE_LABEL_CA,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_CA,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_CA,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_CA,
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
      WSWAN_SOUND_LOW_PASS_LABEL_CA,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_CA,
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
struct retro_core_options_v2 options_ca = {
   option_cats_ca,
   option_defs_ca
};

/* RETRO_LANGUAGE_CHS */

#define WSWAN_ROTATE_DISPLAY_LABEL_CHS "显示旋转"
#define WSWAN_ROTATE_DISPLAY_INFO_0_CHS NULL
#define OPTION_VAL_MANUAL_CHS "手工"
#define OPTION_VAL_LANDSCAPE_CHS "横向"
#define OPTION_VAL_PORTRAIT_CHS "纵向"
#define WSWAN_ROTATE_KEYMAP_LABEL_CHS NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_CHS "选择首选按钮映射。“自动”将调整按钮映射以匹配当前显示旋转。"
#define OPTION_VAL_AUTO_CHS "自动"
#define WSWAN_MONO_PALETTE_LABEL_CHS "色板"
#define WSWAN_MONO_PALETTE_INFO_0_CHS "选择WonderSwan (Mono) 游戏的颜色化。'WS' 调色板将模仿原始WonderSwan 硬件的显示。"
#define OPTION_VAL_DEFAULT_CHS "灰阶"
#define OPTION_VAL_WONDERSWAN_CHS NULL
#define OPTION_VAL_WONDESWAN_COLOR_CHS NULL
#define OPTION_VAL_SWANCRYSTAL_CHS NULL
#define OPTION_VAL_GB_DMG_CHS "Game Boy DMG（元祖GB）"
#define OPTION_VAL_GB_POCKET_CHS NULL
#define OPTION_VAL_GB_LIGHT_CHS NULL
#define OPTION_VAL_BLOSSOM_PINK_CHS NULL
#define OPTION_VAL_BUBBLES_BLUE_CHS NULL
#define OPTION_VAL_BUTTERCUP_GREEN_CHS NULL
#define OPTION_VAL_DIGIVICE_CHS NULL
#define OPTION_VAL_GAME_COM_CHS NULL
#define OPTION_VAL_GAMEKING_CHS NULL
#define OPTION_VAL_GAME_MASTER_CHS NULL
#define OPTION_VAL_GOLDEN_WILD_CHS NULL
#define OPTION_VAL_GREENSCALE_CHS NULL
#define OPTION_VAL_HOKAGE_ORANGE_CHS NULL
#define OPTION_VAL_LABO_FAWN_CHS NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CHS NULL
#define OPTION_VAL_MICROVISION_CHS NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_CHS NULL
#define OPTION_VAL_ODYSSEY_GOLD_CHS NULL
#define OPTION_VAL_SHINY_SKY_BLUE_CHS NULL
#define OPTION_VAL_SLIME_BLUE_CHS NULL
#define OPTION_VAL_TI_83_CHS NULL
#define OPTION_VAL_TRAVEL_WOOD_CHS NULL
#define OPTION_VAL_VIRTUAL_BOY_CHS NULL
#define WSWAN_GFX_COLORS_LABEL_CHS "色深(需要重启)"
#define WSWAN_GFX_COLORS_INFO_0_CHS "24-bit 较慢且无法在所有平台上使用。"
#define OPTION_VAL_16BIT_CHS "千位级(16-bit)"
#define OPTION_VAL_24BIT_CHS "百万位级(24-bit)"
#define WSWAN_FRAMESKIP_LABEL_CHS "跳帧"
#define WSWAN_FRAMESKIP_INFO_0_CHS "跳过帧以避免运行下的音频缓冲区(崩溃)。提高性能以牺牲视觉性能。 “自动”跳板帧在前端建议时。“手动”使用了“Frameskip 阈值(%)”设置。"
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_CHS "跳帧阈值(%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CHS "当'Frameskip'设置为“手动”时，这将指定音频缓冲区占用阈值(百分比)，低于此值的帧将被跳过。 较高的值会导致帧更频繁地丢失，从而降低破碎的风险。"
#define WSWAN_60HZ_MODE_LABEL_CHS "60Hz 模式"
#define WSWAN_60HZ_MODE_INFO_0_CHS NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_CHS "声音输出采样率"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_CHS "较高的数值略微提高了音质，牺牲了性能。"
#define WSWAN_SOUND_LOW_PASS_LABEL_CHS "音频过滤器"
#define WSWAN_SOUND_LOW_PASS_INFO_0_CHS NULL

struct retro_core_option_v2_category option_cats_chs[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_chs[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_CHS,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_CHS,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_CHS },
         { "landscape", OPTION_VAL_LANDSCAPE_CHS },
         { "portrait",  OPTION_VAL_PORTRAIT_CHS },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_CHS,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_CHS,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_CHS },
         { "disabled", OPTION_VAL_LANDSCAPE_CHS },
         { "enabled",  OPTION_VAL_PORTRAIT_CHS },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_CHS,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_CHS,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_CHS },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_CHS },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_CHS },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_CHS },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_CHS },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_CHS },
         { "gb_light",               OPTION_VAL_GB_LIGHT_CHS },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_CHS },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_CHS },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_CHS },
         { "digivice",               OPTION_VAL_DIGIVICE_CHS },
         { "game_com",               OPTION_VAL_GAME_COM_CHS },
         { "gameking",               OPTION_VAL_GAMEKING_CHS },
         { "game_master",            OPTION_VAL_GAME_MASTER_CHS },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_CHS },
         { "greenscale",             OPTION_VAL_GREENSCALE_CHS },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_CHS },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_CHS },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CHS },
         { "microvision",            OPTION_VAL_MICROVISION_CHS },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_CHS },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_CHS },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_CHS },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_CHS },
         { "ti_83",                  OPTION_VAL_TI_83_CHS },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_CHS },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_CHS },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_CHS,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_CHS,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_CHS },
         { "24bit", OPTION_VAL_24BIT_CHS },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_CHS,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_CHS,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_CHS },
         { "manual",   OPTION_VAL_MANUAL_CHS },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_CHS,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CHS,
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
      WSWAN_60HZ_MODE_LABEL_CHS,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_CHS,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_CHS,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_CHS,
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
      WSWAN_SOUND_LOW_PASS_LABEL_CHS,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_CHS,
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
struct retro_core_options_v2 options_chs = {
   option_cats_chs,
   option_defs_chs
};

/* RETRO_LANGUAGE_CHT */

#define WSWAN_ROTATE_DISPLAY_LABEL_CHT "螢幕旋轉"
#define WSWAN_ROTATE_DISPLAY_INFO_0_CHT "設定螢幕旋轉方向, 設定<直向>可用於橫式顯示器並正確旋轉角度, 設定<手動>可用「控制器」按鍵隨時切換方向。"
#define OPTION_VAL_MANUAL_CHT "手動"
#define OPTION_VAL_LANDSCAPE_CHT "橫向"
#define OPTION_VAL_PORTRAIT_CHT "直向"
#define WSWAN_ROTATE_KEYMAP_LABEL_CHT "按鍵映射"
#define WSWAN_ROTATE_KEYMAP_INFO_0_CHT "設定按鍵映射方向, 設定<自動>時依畫面的方向自動調整按鍵映射。"
#define OPTION_VAL_AUTO_CHT "自動"
#define WSWAN_MONO_PALETTE_LABEL_CHT "影像配色"
#define WSWAN_MONO_PALETTE_INFO_0_CHT "設定WonderSwan主機單色遊戲的著色模式, 選項WS類別為WonderSwan主機的擬真顯示。"
#define OPTION_VAL_DEFAULT_CHT "灰階"
#define OPTION_VAL_WONDERSWAN_CHT "WS - WonderSwan 主機"
#define OPTION_VAL_WONDESWAN_COLOR_CHT "WS - WonderSwan Color 主機"
#define OPTION_VAL_SWANCRYSTAL_CHT "WS - SwanCrystal 主機"
#define OPTION_VAL_GB_DMG_CHT "Game Boy 主機"
#define OPTION_VAL_GB_POCKET_CHT "Game Boy Pocket 主機"
#define OPTION_VAL_GB_LIGHT_CHT "Game Boy Light 主機"
#define OPTION_VAL_BLOSSOM_PINK_CHT "飛天小女警花花粉"
#define OPTION_VAL_BUBBLES_BLUE_CHT "飛天小女警泡泡藍"
#define OPTION_VAL_BUTTERCUP_GREEN_CHT "飛天小女警毛毛綠"
#define OPTION_VAL_DIGIVICE_CHT "怪獸對打機D2"
#define OPTION_VAL_GAME_COM_CHT "Game.com 主機"
#define OPTION_VAL_GAMEKING_CHT "GameKing 主機"
#define OPTION_VAL_GAME_MASTER_CHT "Game Master 主機"
#define OPTION_VAL_GOLDEN_WILD_CHT "曠野金"
#define OPTION_VAL_GREENSCALE_CHT "綠色階"
#define OPTION_VAL_HOKAGE_ORANGE_CHT "火影橘"
#define OPTION_VAL_LABO_FAWN_CHT "瓦楞棕"
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CHT "傳說中的超級賽亞人 (綠光)"
#define OPTION_VAL_MICROVISION_CHT "Microvision 主機"
#define OPTION_VAL_MILLION_LIVE_GOLD_CHT "偶像大師百萬人演唱會 (金色)"
#define OPTION_VAL_ODYSSEY_GOLD_CHT "奧德賽金幣 (金黃)"
#define OPTION_VAL_SHINY_SKY_BLUE_CHT "閃亮藍"
#define OPTION_VAL_SLIME_BLUE_CHT "史萊姆藍"
#define OPTION_VAL_TI_83_CHT "TI-83 工程計算機"
#define OPTION_VAL_TRAVEL_WOOD_CHT "織夢島DX旅照模式 (粉撲色)"
#define OPTION_VAL_VIRTUAL_BOY_CHT "Virtual Boy 主機 (紅黑)"
#define WSWAN_GFX_COLORS_LABEL_CHT "色彩深度 (需要重新啟動)"
#define WSWAN_GFX_COLORS_INFO_0_CHT "全彩(24位元)影響效能且不是所有平台都支援。"
#define OPTION_VAL_16BIT_CHT "高彩 (16位元)"
#define OPTION_VAL_24BIT_CHT "全彩 (24位元)"
#define WSWAN_FRAMESKIP_LABEL_CHT "跳幀"
#define WSWAN_FRAMESKIP_INFO_0_CHT "設定跳幀用以避免音訊緩衝區不足(產生破音), 用影像流暢度為代價提高效能。\n設定<自動>由前端系統控制跳幀, 設定<手動>由「跳幀上限(%)」控制跳幀。"
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_CHT "跳幀上限 (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CHT "設定「跳幀」為<手動>時音頻緩衝區使用上限(%)。\n低於上限的影格將被跳過, 設定過高會導致頻繁丟失影格, 過低則是出現破音。"
#define WSWAN_60HZ_MODE_LABEL_CHT "60Hz模式"
#define WSWAN_60HZ_MODE_INFO_0_CHT "不使用WonderSwan預設的75Hz更新率, 使用60Hz更新率模式, 更新率每五幀下降一次。\n適用於不支援60Hz以上的裝置, 降低影像流暢度避免出現畫面撕裂。"
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_CHT "音訊輸出取樣率"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_CHT "較高的值會些許提高音質但會降低效能。"
#define WSWAN_SOUND_LOW_PASS_LABEL_CHT "音訊濾波器"
#define WSWAN_SOUND_LOW_PASS_INFO_0_CHT "WonderSwan音樂晶片有時會產生刺耳聲, 使用低通音訊濾波器進行柔和處理。"

struct retro_core_option_v2_category option_cats_cht[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_cht[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_CHT,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_CHT,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_CHT },
         { "landscape", OPTION_VAL_LANDSCAPE_CHT },
         { "portrait",  OPTION_VAL_PORTRAIT_CHT },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_CHT,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_CHT,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_CHT },
         { "disabled", OPTION_VAL_LANDSCAPE_CHT },
         { "enabled",  OPTION_VAL_PORTRAIT_CHT },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_CHT,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_CHT,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_CHT },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_CHT },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_CHT },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_CHT },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_CHT },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_CHT },
         { "gb_light",               OPTION_VAL_GB_LIGHT_CHT },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_CHT },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_CHT },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_CHT },
         { "digivice",               OPTION_VAL_DIGIVICE_CHT },
         { "game_com",               OPTION_VAL_GAME_COM_CHT },
         { "gameking",               OPTION_VAL_GAMEKING_CHT },
         { "game_master",            OPTION_VAL_GAME_MASTER_CHT },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_CHT },
         { "greenscale",             OPTION_VAL_GREENSCALE_CHT },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_CHT },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_CHT },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CHT },
         { "microvision",            OPTION_VAL_MICROVISION_CHT },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_CHT },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_CHT },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_CHT },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_CHT },
         { "ti_83",                  OPTION_VAL_TI_83_CHT },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_CHT },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_CHT },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_CHT,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_CHT,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_CHT },
         { "24bit", OPTION_VAL_24BIT_CHT },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_CHT,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_CHT,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_CHT },
         { "manual",   OPTION_VAL_MANUAL_CHT },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_CHT,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CHT,
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
      WSWAN_60HZ_MODE_LABEL_CHT,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_CHT,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_CHT,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_CHT,
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
      WSWAN_SOUND_LOW_PASS_LABEL_CHT,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_CHT,
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
struct retro_core_options_v2 options_cht = {
   option_cats_cht,
   option_defs_cht
};

/* RETRO_LANGUAGE_CS */

#define WSWAN_ROTATE_DISPLAY_LABEL_CS "Otáčení Displeje"
#define WSWAN_ROTATE_DISPLAY_INFO_0_CS "Vyberte orientaci konzole. 'Portrét' lze použít k dosažení správného rozložení her orientovaných na výšku na klasickém displeji (Krajina). 'Ruční' umožňuje otáčení přes ovladač (výchozí tlačítko: SELECT)."
#define OPTION_VAL_MANUAL_CS "Ruční"
#define OPTION_VAL_LANDSCAPE_CS "Krajina"
#define OPTION_VAL_PORTRAIT_CS "Portrét"
#define WSWAN_ROTATE_KEYMAP_LABEL_CS "Mapování Otočných Tlačítek"
#define WSWAN_ROTATE_KEYMAP_INFO_0_CS "Vyberte preferované mapování tlačítek. 'Auto' upraví mapování tlačítek tak, aby odpovídalo aktuálnímu natočení displeje."
#define OPTION_VAL_AUTO_CS NULL
#define WSWAN_MONO_PALETTE_LABEL_CS "Paleta Barev"
#define WSWAN_MONO_PALETTE_INFO_0_CS "Výběr barevného odlišení her WonderSwan (Mono). Palety 'WS' napodobují zobrazení původního hardwaru WonderSwan."
#define OPTION_VAL_DEFAULT_CS NULL
#define OPTION_VAL_WONDERSWAN_CS NULL
#define OPTION_VAL_WONDESWAN_COLOR_CS NULL
#define OPTION_VAL_SWANCRYSTAL_CS "WS – SwanCrystal"
#define OPTION_VAL_GB_DMG_CS NULL
#define OPTION_VAL_GB_POCKET_CS NULL
#define OPTION_VAL_GB_LIGHT_CS NULL
#define OPTION_VAL_BLOSSOM_PINK_CS NULL
#define OPTION_VAL_BUBBLES_BLUE_CS NULL
#define OPTION_VAL_BUTTERCUP_GREEN_CS NULL
#define OPTION_VAL_DIGIVICE_CS NULL
#define OPTION_VAL_GAME_COM_CS NULL
#define OPTION_VAL_GAMEKING_CS NULL
#define OPTION_VAL_GAME_MASTER_CS NULL
#define OPTION_VAL_GOLDEN_WILD_CS NULL
#define OPTION_VAL_GREENSCALE_CS NULL
#define OPTION_VAL_HOKAGE_ORANGE_CS NULL
#define OPTION_VAL_LABO_FAWN_CS NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CS NULL
#define OPTION_VAL_MICROVISION_CS NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_CS NULL
#define OPTION_VAL_ODYSSEY_GOLD_CS NULL
#define OPTION_VAL_SHINY_SKY_BLUE_CS NULL
#define OPTION_VAL_SLIME_BLUE_CS NULL
#define OPTION_VAL_TI_83_CS NULL
#define OPTION_VAL_TRAVEL_WOOD_CS NULL
#define OPTION_VAL_VIRTUAL_BOY_CS "Virtuální Chlapec"
#define WSWAN_GFX_COLORS_LABEL_CS "Hloubka Barev (Vyžaduje Restart)"
#define WSWAN_GFX_COLORS_INFO_0_CS "24 bitů je pomalejších a nejsou k dispozici na všech platformách."
#define OPTION_VAL_16BIT_CS "Tisíce (16 bitů)"
#define OPTION_VAL_24BIT_CS "Miliony (24 bitů)"
#define WSWAN_FRAMESKIP_LABEL_CS "Přeskočení Snímku"
#define WSWAN_FRAMESKIP_INFO_0_CS "Přeskočení snímků, aby se zabránilo nedostatečnému využití vyrovnávací paměti zvuku (praskání). Zlepšuje výkon na úkor vizuální plynulosti. Funkce 'Auto' přeskakuje snímky, pokud to doporučí frontend. 'Ruční' využívá nastavení 'Práh přeskakování snímků (%)'."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_CS "Hraniční Hodnota Přeskočení Snímku (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CS "Pokud je položka 'Přeskočení Snímku' nastavena na hodnotu 'Ruční', určuje práh obsazení vyrovnávací paměti zvuku (v procentech), pod kterým budou snímky přeskočeny. Vyšší hodnoty snižují riziko praskání tím, že způsobují častější vynechávání snímků."
#define WSWAN_60HZ_MODE_LABEL_CS "60Hz režim"
#define WSWAN_60HZ_MODE_INFO_0_CS "Aktualizujte displej na 60 Hz místo nativní obnovovací frekvence 75 Hz zařízení WonderSwan tak, že budete ignorovat každý pátý snímek. Snižuje plynulost videa, ale zabraňuje trhání na displejích, které nativně nepodporují více než 60 Hz."
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_CS "Výstupní Zvuková Vzorkovací Frekvence"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_CS "Vyšší hodnoty mírně zlepšují kvalitu zvuku na úkor výkonu."
#define WSWAN_SOUND_LOW_PASS_LABEL_CS "Audio Filtr"
#define WSWAN_SOUND_LOW_PASS_INFO_0_CS "Použijte zvukový filtr s dolní propustí, abyste \"zjemnili\" někdy ostré chiptuny produkované zařízením WonderSwan."

struct retro_core_option_v2_category option_cats_cs[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_cs[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_CS,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_CS,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_CS },
         { "landscape", OPTION_VAL_LANDSCAPE_CS },
         { "portrait",  OPTION_VAL_PORTRAIT_CS },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_CS,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_CS,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_CS },
         { "disabled", OPTION_VAL_LANDSCAPE_CS },
         { "enabled",  OPTION_VAL_PORTRAIT_CS },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_CS,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_CS,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_CS },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_CS },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_CS },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_CS },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_CS },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_CS },
         { "gb_light",               OPTION_VAL_GB_LIGHT_CS },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_CS },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_CS },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_CS },
         { "digivice",               OPTION_VAL_DIGIVICE_CS },
         { "game_com",               OPTION_VAL_GAME_COM_CS },
         { "gameking",               OPTION_VAL_GAMEKING_CS },
         { "game_master",            OPTION_VAL_GAME_MASTER_CS },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_CS },
         { "greenscale",             OPTION_VAL_GREENSCALE_CS },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_CS },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_CS },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CS },
         { "microvision",            OPTION_VAL_MICROVISION_CS },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_CS },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_CS },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_CS },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_CS },
         { "ti_83",                  OPTION_VAL_TI_83_CS },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_CS },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_CS },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_CS,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_CS,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_CS },
         { "24bit", OPTION_VAL_24BIT_CS },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_CS,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_CS,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_CS },
         { "manual",   OPTION_VAL_MANUAL_CS },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_CS,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CS,
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
      WSWAN_60HZ_MODE_LABEL_CS,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_CS,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_CS,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_CS,
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
      WSWAN_SOUND_LOW_PASS_LABEL_CS,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_CS,
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
struct retro_core_options_v2 options_cs = {
   option_cats_cs,
   option_defs_cs
};

/* RETRO_LANGUAGE_CY */

#define WSWAN_ROTATE_DISPLAY_LABEL_CY NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_CY NULL
#define OPTION_VAL_MANUAL_CY NULL
#define OPTION_VAL_LANDSCAPE_CY NULL
#define OPTION_VAL_PORTRAIT_CY NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_CY NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_CY NULL
#define OPTION_VAL_AUTO_CY NULL
#define WSWAN_MONO_PALETTE_LABEL_CY NULL
#define WSWAN_MONO_PALETTE_INFO_0_CY NULL
#define OPTION_VAL_DEFAULT_CY NULL
#define OPTION_VAL_WONDERSWAN_CY NULL
#define OPTION_VAL_WONDESWAN_COLOR_CY NULL
#define OPTION_VAL_SWANCRYSTAL_CY NULL
#define OPTION_VAL_GB_DMG_CY NULL
#define OPTION_VAL_GB_POCKET_CY NULL
#define OPTION_VAL_GB_LIGHT_CY NULL
#define OPTION_VAL_BLOSSOM_PINK_CY NULL
#define OPTION_VAL_BUBBLES_BLUE_CY NULL
#define OPTION_VAL_BUTTERCUP_GREEN_CY NULL
#define OPTION_VAL_DIGIVICE_CY NULL
#define OPTION_VAL_GAME_COM_CY NULL
#define OPTION_VAL_GAMEKING_CY NULL
#define OPTION_VAL_GAME_MASTER_CY NULL
#define OPTION_VAL_GOLDEN_WILD_CY NULL
#define OPTION_VAL_GREENSCALE_CY NULL
#define OPTION_VAL_HOKAGE_ORANGE_CY NULL
#define OPTION_VAL_LABO_FAWN_CY NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CY NULL
#define OPTION_VAL_MICROVISION_CY NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_CY NULL
#define OPTION_VAL_ODYSSEY_GOLD_CY NULL
#define OPTION_VAL_SHINY_SKY_BLUE_CY NULL
#define OPTION_VAL_SLIME_BLUE_CY NULL
#define OPTION_VAL_TI_83_CY NULL
#define OPTION_VAL_TRAVEL_WOOD_CY NULL
#define OPTION_VAL_VIRTUAL_BOY_CY NULL
#define WSWAN_GFX_COLORS_LABEL_CY NULL
#define WSWAN_GFX_COLORS_INFO_0_CY NULL
#define OPTION_VAL_16BIT_CY NULL
#define OPTION_VAL_24BIT_CY NULL
#define WSWAN_FRAMESKIP_LABEL_CY NULL
#define WSWAN_FRAMESKIP_INFO_0_CY NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_CY NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CY NULL
#define WSWAN_60HZ_MODE_LABEL_CY NULL
#define WSWAN_60HZ_MODE_INFO_0_CY NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_CY NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_CY NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_CY NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_CY NULL

struct retro_core_option_v2_category option_cats_cy[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_cy[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_CY,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_CY,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_CY },
         { "landscape", OPTION_VAL_LANDSCAPE_CY },
         { "portrait",  OPTION_VAL_PORTRAIT_CY },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_CY,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_CY,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_CY },
         { "disabled", OPTION_VAL_LANDSCAPE_CY },
         { "enabled",  OPTION_VAL_PORTRAIT_CY },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_CY,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_CY,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_CY },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_CY },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_CY },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_CY },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_CY },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_CY },
         { "gb_light",               OPTION_VAL_GB_LIGHT_CY },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_CY },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_CY },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_CY },
         { "digivice",               OPTION_VAL_DIGIVICE_CY },
         { "game_com",               OPTION_VAL_GAME_COM_CY },
         { "gameking",               OPTION_VAL_GAMEKING_CY },
         { "game_master",            OPTION_VAL_GAME_MASTER_CY },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_CY },
         { "greenscale",             OPTION_VAL_GREENSCALE_CY },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_CY },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_CY },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_CY },
         { "microvision",            OPTION_VAL_MICROVISION_CY },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_CY },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_CY },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_CY },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_CY },
         { "ti_83",                  OPTION_VAL_TI_83_CY },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_CY },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_CY },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_CY,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_CY,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_CY },
         { "24bit", OPTION_VAL_24BIT_CY },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_CY,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_CY,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_CY },
         { "manual",   OPTION_VAL_MANUAL_CY },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_CY,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_CY,
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
      WSWAN_60HZ_MODE_LABEL_CY,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_CY,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_CY,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_CY,
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
      WSWAN_SOUND_LOW_PASS_LABEL_CY,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_CY,
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
struct retro_core_options_v2 options_cy = {
   option_cats_cy,
   option_defs_cy
};

/* RETRO_LANGUAGE_DA */

#define WSWAN_ROTATE_DISPLAY_LABEL_DA NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_DA NULL
#define OPTION_VAL_MANUAL_DA NULL
#define OPTION_VAL_LANDSCAPE_DA NULL
#define OPTION_VAL_PORTRAIT_DA NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_DA NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_DA NULL
#define OPTION_VAL_AUTO_DA NULL
#define WSWAN_MONO_PALETTE_LABEL_DA NULL
#define WSWAN_MONO_PALETTE_INFO_0_DA NULL
#define OPTION_VAL_DEFAULT_DA NULL
#define OPTION_VAL_WONDERSWAN_DA NULL
#define OPTION_VAL_WONDESWAN_COLOR_DA NULL
#define OPTION_VAL_SWANCRYSTAL_DA NULL
#define OPTION_VAL_GB_DMG_DA NULL
#define OPTION_VAL_GB_POCKET_DA NULL
#define OPTION_VAL_GB_LIGHT_DA NULL
#define OPTION_VAL_BLOSSOM_PINK_DA NULL
#define OPTION_VAL_BUBBLES_BLUE_DA NULL
#define OPTION_VAL_BUTTERCUP_GREEN_DA NULL
#define OPTION_VAL_DIGIVICE_DA NULL
#define OPTION_VAL_GAME_COM_DA NULL
#define OPTION_VAL_GAMEKING_DA NULL
#define OPTION_VAL_GAME_MASTER_DA NULL
#define OPTION_VAL_GOLDEN_WILD_DA NULL
#define OPTION_VAL_GREENSCALE_DA NULL
#define OPTION_VAL_HOKAGE_ORANGE_DA NULL
#define OPTION_VAL_LABO_FAWN_DA NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_DA NULL
#define OPTION_VAL_MICROVISION_DA NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_DA NULL
#define OPTION_VAL_ODYSSEY_GOLD_DA NULL
#define OPTION_VAL_SHINY_SKY_BLUE_DA NULL
#define OPTION_VAL_SLIME_BLUE_DA NULL
#define OPTION_VAL_TI_83_DA NULL
#define OPTION_VAL_TRAVEL_WOOD_DA NULL
#define OPTION_VAL_VIRTUAL_BOY_DA NULL
#define WSWAN_GFX_COLORS_LABEL_DA NULL
#define WSWAN_GFX_COLORS_INFO_0_DA NULL
#define OPTION_VAL_16BIT_DA NULL
#define OPTION_VAL_24BIT_DA NULL
#define WSWAN_FRAMESKIP_LABEL_DA NULL
#define WSWAN_FRAMESKIP_INFO_0_DA NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_DA NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_DA NULL
#define WSWAN_60HZ_MODE_LABEL_DA NULL
#define WSWAN_60HZ_MODE_INFO_0_DA NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_DA NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_DA NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_DA NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_DA NULL

struct retro_core_option_v2_category option_cats_da[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_da[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_DA,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_DA,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_DA },
         { "landscape", OPTION_VAL_LANDSCAPE_DA },
         { "portrait",  OPTION_VAL_PORTRAIT_DA },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_DA,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_DA,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_DA },
         { "disabled", OPTION_VAL_LANDSCAPE_DA },
         { "enabled",  OPTION_VAL_PORTRAIT_DA },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_DA,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_DA,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_DA },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_DA },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_DA },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_DA },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_DA },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_DA },
         { "gb_light",               OPTION_VAL_GB_LIGHT_DA },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_DA },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_DA },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_DA },
         { "digivice",               OPTION_VAL_DIGIVICE_DA },
         { "game_com",               OPTION_VAL_GAME_COM_DA },
         { "gameking",               OPTION_VAL_GAMEKING_DA },
         { "game_master",            OPTION_VAL_GAME_MASTER_DA },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_DA },
         { "greenscale",             OPTION_VAL_GREENSCALE_DA },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_DA },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_DA },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_DA },
         { "microvision",            OPTION_VAL_MICROVISION_DA },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_DA },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_DA },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_DA },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_DA },
         { "ti_83",                  OPTION_VAL_TI_83_DA },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_DA },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_DA },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_DA,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_DA,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_DA },
         { "24bit", OPTION_VAL_24BIT_DA },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_DA,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_DA,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_DA },
         { "manual",   OPTION_VAL_MANUAL_DA },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_DA,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_DA,
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
      WSWAN_60HZ_MODE_LABEL_DA,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_DA,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_DA,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_DA,
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
      WSWAN_SOUND_LOW_PASS_LABEL_DA,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_DA,
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
struct retro_core_options_v2 options_da = {
   option_cats_da,
   option_defs_da
};

/* RETRO_LANGUAGE_DE */

#define WSWAN_ROTATE_DISPLAY_LABEL_DE "Bildschirmausrichtung"
#define WSWAN_ROTATE_DISPLAY_INFO_0_DE "Die Ausrichtung des Konsolenbildschirms auswählen. \"Hochformat\" kann verwendet werden, um das korrekte Layout von Hochformat-orientierten Spielen auf einer herkömmlichen (Querformats-) Anzeige zu erreichen. \"Manuel\" ermöglicht die Rotation über den Controller (Standard-Taste: SELECT)."
#define OPTION_VAL_MANUAL_DE "Manuell"
#define OPTION_VAL_LANDSCAPE_DE "Querformat"
#define OPTION_VAL_PORTRAIT_DE "Hochformat"
#define WSWAN_ROTATE_KEYMAP_LABEL_DE "Tastenzuordnungen rotieren"
#define WSWAN_ROTATE_KEYMAP_INFO_0_DE "Bevorzugte Tastenzuordnung wählen. \"Automatisch\" passt die Tastenzuordnung an die aktuelle Bildschirmausrichtung an."
#define OPTION_VAL_AUTO_DE "Automatisch"
#define WSWAN_MONO_PALETTE_LABEL_DE "Farbpalette"
#define WSWAN_MONO_PALETTE_INFO_0_DE "Farbgebung von WonderSwan (Mono) Spielen wählen. \"WS\"-Paletten immitieren die original WonderSwan-Hardware."
#define OPTION_VAL_DEFAULT_DE "Graustufen"
#define OPTION_VAL_WONDERSWAN_DE NULL
#define OPTION_VAL_WONDESWAN_COLOR_DE NULL
#define OPTION_VAL_SWANCRYSTAL_DE NULL
#define OPTION_VAL_GB_DMG_DE "Game Boy original"
#define OPTION_VAL_GB_POCKET_DE NULL
#define OPTION_VAL_GB_LIGHT_DE NULL
#define OPTION_VAL_BLOSSOM_PINK_DE "Blossom-Rosa"
#define OPTION_VAL_BUBBLES_BLUE_DE "Bubbles-Blau"
#define OPTION_VAL_BUTTERCUP_GREEN_DE "Buttercup-Grün"
#define OPTION_VAL_DIGIVICE_DE NULL
#define OPTION_VAL_GAME_COM_DE NULL
#define OPTION_VAL_GAMEKING_DE NULL
#define OPTION_VAL_GAME_MASTER_DE NULL
#define OPTION_VAL_GOLDEN_WILD_DE NULL
#define OPTION_VAL_GREENSCALE_DE "Grüne Farbgebung"
#define OPTION_VAL_HOKAGE_ORANGE_DE "Hokage-Orange"
#define OPTION_VAL_LABO_FAWN_DE "Labo-Kitz"
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_DE "Legendärer Super-Saiyajin"
#define OPTION_VAL_MICROVISION_DE NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_DE "Million-Live-Gold"
#define OPTION_VAL_ODYSSEY_GOLD_DE "Odyssey-Gold"
#define OPTION_VAL_SHINY_SKY_BLUE_DE "Shiny-Sky-Blau"
#define OPTION_VAL_SLIME_BLUE_DE "Schleim-Blau"
#define OPTION_VAL_TI_83_DE NULL
#define OPTION_VAL_TRAVEL_WOOD_DE "Reiseholz"
#define OPTION_VAL_VIRTUAL_BOY_DE NULL
#define WSWAN_GFX_COLORS_LABEL_DE "Farbtiefe (Neustart erforderlich)"
#define WSWAN_GFX_COLORS_INFO_0_DE "24 Bit ist langsamer und nicht auf allen Plattformen verfügbar."
#define OPTION_VAL_16BIT_DE "Tausende (16 Bit)"
#define OPTION_VAL_24BIT_DE "Millionen (24 Bit)"
#define WSWAN_FRAMESKIP_LABEL_DE NULL
#define WSWAN_FRAMESKIP_INFO_0_DE "Frames überspringen, um ein Unterlaufen des Audiopuffers (Knistern) zu vermeiden. Verbessert die Leistung auf Kosten der visuellen Glätte. 'Auto' überspringt Frames, wenn vom Frontend empfohlen. 'Manual' verwendet die Einstellung 'Frameskip Grenzwert (%)'."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_DE "Frameskip Grenzwert (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_DE "Gibt den Audiopuffer-Belegungsschwellenwert (Prozent) an, unter dem Frames übersprungen werden, wenn \"Frameskip\" auf \"Manuell\" eingestellt ist. Höhere Werte verringern das Risiko von Knistern, da Frames häufiger weggelassen werden."
#define WSWAN_60HZ_MODE_LABEL_DE "60Hz Modus"
#define WSWAN_60HZ_MODE_INFO_0_DE "Aktualisiert die Anzeige um 60Hz anstelle der nativen 75Hz WonderSwan Aktualisierungsrate, während jedes fünfte Frame verworfen wird. Reduziert die Videoglättung, aber vermeidet Tearing bei Bildschirmen, die den nativen Betrieb über 60Hz nicht unterstützen."
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_DE "Audioausgabe-Abtastrate"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_DE "Höhere Werte verbessern die Klangqualität leicht auf Kosten der Leistung."
#define WSWAN_SOUND_LOW_PASS_LABEL_DE "Audiofilter"
#define WSWAN_SOUND_LOW_PASS_INFO_0_DE "Fügt einen Tiefpass-Audiofilter hinzu, um die manchmal harten Chiptunes des WonderSwan zu 'weichen'."

struct retro_core_option_v2_category option_cats_de[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_de[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_DE,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_DE,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_DE },
         { "landscape", OPTION_VAL_LANDSCAPE_DE },
         { "portrait",  OPTION_VAL_PORTRAIT_DE },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_DE,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_DE,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_DE },
         { "disabled", OPTION_VAL_LANDSCAPE_DE },
         { "enabled",  OPTION_VAL_PORTRAIT_DE },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_DE,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_DE,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_DE },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_DE },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_DE },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_DE },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_DE },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_DE },
         { "gb_light",               OPTION_VAL_GB_LIGHT_DE },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_DE },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_DE },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_DE },
         { "digivice",               OPTION_VAL_DIGIVICE_DE },
         { "game_com",               OPTION_VAL_GAME_COM_DE },
         { "gameking",               OPTION_VAL_GAMEKING_DE },
         { "game_master",            OPTION_VAL_GAME_MASTER_DE },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_DE },
         { "greenscale",             OPTION_VAL_GREENSCALE_DE },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_DE },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_DE },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_DE },
         { "microvision",            OPTION_VAL_MICROVISION_DE },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_DE },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_DE },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_DE },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_DE },
         { "ti_83",                  OPTION_VAL_TI_83_DE },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_DE },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_DE },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_DE,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_DE,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_DE },
         { "24bit", OPTION_VAL_24BIT_DE },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_DE,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_DE,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_DE },
         { "manual",   OPTION_VAL_MANUAL_DE },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_DE,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_DE,
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
      WSWAN_60HZ_MODE_LABEL_DE,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_DE,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_DE,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_DE,
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
      WSWAN_SOUND_LOW_PASS_LABEL_DE,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_DE,
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
struct retro_core_options_v2 options_de = {
   option_cats_de,
   option_defs_de
};

/* RETRO_LANGUAGE_EL */

#define WSWAN_ROTATE_DISPLAY_LABEL_EL "Περιστροφή Οθόνης"
#define WSWAN_ROTATE_DISPLAY_INFO_0_EL NULL
#define OPTION_VAL_MANUAL_EL "Χειροκίνητο"
#define OPTION_VAL_LANDSCAPE_EL "Οριζόντιος Προσανατολισμός"
#define OPTION_VAL_PORTRAIT_EL "Κατακόρυφος Προσανατολισμός"
#define WSWAN_ROTATE_KEYMAP_LABEL_EL "Περιστροφή Πλήκτρων"
#define WSWAN_ROTATE_KEYMAP_INFO_0_EL NULL
#define OPTION_VAL_AUTO_EL "Αυτόματο"
#define WSWAN_MONO_PALETTE_LABEL_EL "Παλέτα Χρωμάτων"
#define WSWAN_MONO_PALETTE_INFO_0_EL NULL
#define OPTION_VAL_DEFAULT_EL "Γκρι απόχρωση"
#define OPTION_VAL_WONDERSWAN_EL NULL
#define OPTION_VAL_WONDESWAN_COLOR_EL NULL
#define OPTION_VAL_SWANCRYSTAL_EL NULL
#define OPTION_VAL_GB_DMG_EL NULL
#define OPTION_VAL_GB_POCKET_EL NULL
#define OPTION_VAL_GB_LIGHT_EL NULL
#define OPTION_VAL_BLOSSOM_PINK_EL NULL
#define OPTION_VAL_BUBBLES_BLUE_EL NULL
#define OPTION_VAL_BUTTERCUP_GREEN_EL NULL
#define OPTION_VAL_DIGIVICE_EL NULL
#define OPTION_VAL_GAME_COM_EL NULL
#define OPTION_VAL_GAMEKING_EL NULL
#define OPTION_VAL_GAME_MASTER_EL NULL
#define OPTION_VAL_GOLDEN_WILD_EL NULL
#define OPTION_VAL_GREENSCALE_EL NULL
#define OPTION_VAL_HOKAGE_ORANGE_EL NULL
#define OPTION_VAL_LABO_FAWN_EL NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_EL NULL
#define OPTION_VAL_MICROVISION_EL NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_EL NULL
#define OPTION_VAL_ODYSSEY_GOLD_EL NULL
#define OPTION_VAL_SHINY_SKY_BLUE_EL NULL
#define OPTION_VAL_SLIME_BLUE_EL NULL
#define OPTION_VAL_TI_83_EL NULL
#define OPTION_VAL_TRAVEL_WOOD_EL NULL
#define OPTION_VAL_VIRTUAL_BOY_EL NULL
#define WSWAN_GFX_COLORS_LABEL_EL "Βάθος Χρώματος (Απαιτείται Επανεκκίνηση)"
#define WSWAN_GFX_COLORS_INFO_0_EL NULL
#define OPTION_VAL_16BIT_EL "Χιλιάδες (16-bit)"
#define OPTION_VAL_24BIT_EL "Εκατομμύρια (24-bit)"
#define WSWAN_FRAMESKIP_LABEL_EL NULL
#define WSWAN_FRAMESKIP_INFO_0_EL NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_EL NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_EL NULL
#define WSWAN_60HZ_MODE_LABEL_EL NULL
#define WSWAN_60HZ_MODE_INFO_0_EL NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_EL NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_EL NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_EL "Φίλτρα Ήχου"
#define WSWAN_SOUND_LOW_PASS_INFO_0_EL NULL

struct retro_core_option_v2_category option_cats_el[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_el[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_EL,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_EL,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_EL },
         { "landscape", OPTION_VAL_LANDSCAPE_EL },
         { "portrait",  OPTION_VAL_PORTRAIT_EL },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_EL,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_EL,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_EL },
         { "disabled", OPTION_VAL_LANDSCAPE_EL },
         { "enabled",  OPTION_VAL_PORTRAIT_EL },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_EL,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_EL,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_EL },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_EL },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_EL },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_EL },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_EL },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_EL },
         { "gb_light",               OPTION_VAL_GB_LIGHT_EL },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_EL },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_EL },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_EL },
         { "digivice",               OPTION_VAL_DIGIVICE_EL },
         { "game_com",               OPTION_VAL_GAME_COM_EL },
         { "gameking",               OPTION_VAL_GAMEKING_EL },
         { "game_master",            OPTION_VAL_GAME_MASTER_EL },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_EL },
         { "greenscale",             OPTION_VAL_GREENSCALE_EL },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_EL },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_EL },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_EL },
         { "microvision",            OPTION_VAL_MICROVISION_EL },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_EL },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_EL },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_EL },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_EL },
         { "ti_83",                  OPTION_VAL_TI_83_EL },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_EL },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_EL },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_EL,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_EL,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_EL },
         { "24bit", OPTION_VAL_24BIT_EL },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_EL,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_EL,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_EL },
         { "manual",   OPTION_VAL_MANUAL_EL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_EL,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_EL,
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
      WSWAN_60HZ_MODE_LABEL_EL,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_EL,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_EL,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_EL,
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
      WSWAN_SOUND_LOW_PASS_LABEL_EL,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_EL,
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
struct retro_core_options_v2 options_el = {
   option_cats_el,
   option_defs_el
};

/* RETRO_LANGUAGE_EN */

#define WSWAN_ROTATE_DISPLAY_LABEL_EN NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_EN "Choose the orientation of the console screen. 'Portrait' can be used to achieve the correct layout of portrait oriented games on a conventional (landscape) display. 'Manual' enables rotation with the controller (default button: SELECT)."
#define OPTION_VAL_MANUAL_EN NULL
#define OPTION_VAL_LANDSCAPE_EN NULL
#define OPTION_VAL_PORTRAIT_EN NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_EN NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_EN NULL
#define OPTION_VAL_AUTO_EN NULL
#define WSWAN_MONO_PALETTE_LABEL_EN "Colour Palette"
#define WSWAN_MONO_PALETTE_INFO_0_EN "Choose the colurisation of WonderSwan (Mono) games. 'WS' palettes mimic the display of the original WonderSwan hardware."
#define OPTION_VAL_DEFAULT_EN "Greyscale"
#define OPTION_VAL_WONDERSWAN_EN NULL
#define OPTION_VAL_WONDESWAN_COLOR_EN "WS - WonderSwan Colour"
#define OPTION_VAL_SWANCRYSTAL_EN NULL
#define OPTION_VAL_GB_DMG_EN NULL
#define OPTION_VAL_GB_POCKET_EN NULL
#define OPTION_VAL_GB_LIGHT_EN NULL
#define OPTION_VAL_BLOSSOM_PINK_EN NULL
#define OPTION_VAL_BUBBLES_BLUE_EN NULL
#define OPTION_VAL_BUTTERCUP_GREEN_EN NULL
#define OPTION_VAL_DIGIVICE_EN NULL
#define OPTION_VAL_GAME_COM_EN NULL
#define OPTION_VAL_GAMEKING_EN NULL
#define OPTION_VAL_GAME_MASTER_EN NULL
#define OPTION_VAL_GOLDEN_WILD_EN NULL
#define OPTION_VAL_GREENSCALE_EN NULL
#define OPTION_VAL_HOKAGE_ORANGE_EN NULL
#define OPTION_VAL_LABO_FAWN_EN NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_EN NULL
#define OPTION_VAL_MICROVISION_EN NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_EN NULL
#define OPTION_VAL_ODYSSEY_GOLD_EN NULL
#define OPTION_VAL_SHINY_SKY_BLUE_EN NULL
#define OPTION_VAL_SLIME_BLUE_EN NULL
#define OPTION_VAL_TI_83_EN NULL
#define OPTION_VAL_TRAVEL_WOOD_EN NULL
#define OPTION_VAL_VIRTUAL_BOY_EN NULL
#define WSWAN_GFX_COLORS_LABEL_EN "Colour Depth (Restart Required)"
#define WSWAN_GFX_COLORS_INFO_0_EN NULL
#define OPTION_VAL_16BIT_EN NULL
#define OPTION_VAL_24BIT_EN NULL
#define WSWAN_FRAMESKIP_LABEL_EN "Frame-skip"
#define WSWAN_FRAMESKIP_INFO_0_EN "Skip frames to avoid audio buffer under-run (crackling). Improves performance at the expense of visual smoothness. 'Auto' skips frames when advised by the front-end. 'Manual' utilizes the 'Frame-skip Threshold (%)' setting."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_EN "Frame-skip Threshold (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_EN "When 'Frame-skip' is set to 'Manual', this specifies the audio buffer occupancy threshold (percentage) below which frames will be skipped. Higher values reduce the risk of crackling by causing frames to be dropped more frequently."
#define WSWAN_60HZ_MODE_LABEL_EN NULL
#define WSWAN_60HZ_MODE_INFO_0_EN NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_EN NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_EN NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_EN NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_EN NULL

struct retro_core_option_v2_category option_cats_en[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_en[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_EN,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_EN,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_EN },
         { "landscape", OPTION_VAL_LANDSCAPE_EN },
         { "portrait",  OPTION_VAL_PORTRAIT_EN },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_EN,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_EN,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_EN },
         { "disabled", OPTION_VAL_LANDSCAPE_EN },
         { "enabled",  OPTION_VAL_PORTRAIT_EN },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_EN,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_EN,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_EN },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_EN },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_EN },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_EN },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_EN },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_EN },
         { "gb_light",               OPTION_VAL_GB_LIGHT_EN },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_EN },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_EN },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_EN },
         { "digivice",               OPTION_VAL_DIGIVICE_EN },
         { "game_com",               OPTION_VAL_GAME_COM_EN },
         { "gameking",               OPTION_VAL_GAMEKING_EN },
         { "game_master",            OPTION_VAL_GAME_MASTER_EN },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_EN },
         { "greenscale",             OPTION_VAL_GREENSCALE_EN },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_EN },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_EN },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_EN },
         { "microvision",            OPTION_VAL_MICROVISION_EN },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_EN },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_EN },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_EN },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_EN },
         { "ti_83",                  OPTION_VAL_TI_83_EN },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_EN },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_EN },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_EN,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_EN,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_EN },
         { "24bit", OPTION_VAL_24BIT_EN },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_EN,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_EN,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_EN },
         { "manual",   OPTION_VAL_MANUAL_EN },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_EN,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_EN,
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
      WSWAN_60HZ_MODE_LABEL_EN,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_EN,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_EN,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_EN,
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
      WSWAN_SOUND_LOW_PASS_LABEL_EN,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_EN,
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
struct retro_core_options_v2 options_en = {
   option_cats_en,
   option_defs_en
};

/* RETRO_LANGUAGE_EO */

#define WSWAN_ROTATE_DISPLAY_LABEL_EO NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_EO NULL
#define OPTION_VAL_MANUAL_EO NULL
#define OPTION_VAL_LANDSCAPE_EO NULL
#define OPTION_VAL_PORTRAIT_EO NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_EO NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_EO NULL
#define OPTION_VAL_AUTO_EO NULL
#define WSWAN_MONO_PALETTE_LABEL_EO NULL
#define WSWAN_MONO_PALETTE_INFO_0_EO NULL
#define OPTION_VAL_DEFAULT_EO NULL
#define OPTION_VAL_WONDERSWAN_EO NULL
#define OPTION_VAL_WONDESWAN_COLOR_EO NULL
#define OPTION_VAL_SWANCRYSTAL_EO NULL
#define OPTION_VAL_GB_DMG_EO NULL
#define OPTION_VAL_GB_POCKET_EO NULL
#define OPTION_VAL_GB_LIGHT_EO NULL
#define OPTION_VAL_BLOSSOM_PINK_EO NULL
#define OPTION_VAL_BUBBLES_BLUE_EO NULL
#define OPTION_VAL_BUTTERCUP_GREEN_EO NULL
#define OPTION_VAL_DIGIVICE_EO NULL
#define OPTION_VAL_GAME_COM_EO NULL
#define OPTION_VAL_GAMEKING_EO NULL
#define OPTION_VAL_GAME_MASTER_EO NULL
#define OPTION_VAL_GOLDEN_WILD_EO NULL
#define OPTION_VAL_GREENSCALE_EO NULL
#define OPTION_VAL_HOKAGE_ORANGE_EO NULL
#define OPTION_VAL_LABO_FAWN_EO NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_EO NULL
#define OPTION_VAL_MICROVISION_EO NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_EO NULL
#define OPTION_VAL_ODYSSEY_GOLD_EO NULL
#define OPTION_VAL_SHINY_SKY_BLUE_EO NULL
#define OPTION_VAL_SLIME_BLUE_EO NULL
#define OPTION_VAL_TI_83_EO NULL
#define OPTION_VAL_TRAVEL_WOOD_EO NULL
#define OPTION_VAL_VIRTUAL_BOY_EO NULL
#define WSWAN_GFX_COLORS_LABEL_EO NULL
#define WSWAN_GFX_COLORS_INFO_0_EO NULL
#define OPTION_VAL_16BIT_EO NULL
#define OPTION_VAL_24BIT_EO NULL
#define WSWAN_FRAMESKIP_LABEL_EO NULL
#define WSWAN_FRAMESKIP_INFO_0_EO NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_EO NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_EO NULL
#define WSWAN_60HZ_MODE_LABEL_EO NULL
#define WSWAN_60HZ_MODE_INFO_0_EO NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_EO NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_EO NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_EO NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_EO NULL

struct retro_core_option_v2_category option_cats_eo[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_eo[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_EO,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_EO,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_EO },
         { "landscape", OPTION_VAL_LANDSCAPE_EO },
         { "portrait",  OPTION_VAL_PORTRAIT_EO },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_EO,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_EO,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_EO },
         { "disabled", OPTION_VAL_LANDSCAPE_EO },
         { "enabled",  OPTION_VAL_PORTRAIT_EO },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_EO,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_EO,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_EO },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_EO },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_EO },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_EO },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_EO },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_EO },
         { "gb_light",               OPTION_VAL_GB_LIGHT_EO },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_EO },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_EO },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_EO },
         { "digivice",               OPTION_VAL_DIGIVICE_EO },
         { "game_com",               OPTION_VAL_GAME_COM_EO },
         { "gameking",               OPTION_VAL_GAMEKING_EO },
         { "game_master",            OPTION_VAL_GAME_MASTER_EO },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_EO },
         { "greenscale",             OPTION_VAL_GREENSCALE_EO },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_EO },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_EO },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_EO },
         { "microvision",            OPTION_VAL_MICROVISION_EO },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_EO },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_EO },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_EO },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_EO },
         { "ti_83",                  OPTION_VAL_TI_83_EO },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_EO },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_EO },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_EO,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_EO,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_EO },
         { "24bit", OPTION_VAL_24BIT_EO },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_EO,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_EO,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_EO },
         { "manual",   OPTION_VAL_MANUAL_EO },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_EO,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_EO,
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
      WSWAN_60HZ_MODE_LABEL_EO,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_EO,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_EO,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_EO,
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
      WSWAN_SOUND_LOW_PASS_LABEL_EO,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_EO,
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
struct retro_core_options_v2 options_eo = {
   option_cats_eo,
   option_defs_eo
};

/* RETRO_LANGUAGE_ES */

#define WSWAN_ROTATE_DISPLAY_LABEL_ES "Rotación de imagen"
#define WSWAN_ROTATE_DISPLAY_INFO_0_ES "Selecciona la orientación de la pantalla de la consola. «Vertical» ofrece la distribución correcta para los juegos con orientación vertical en una pantalla convencional. «Selección manual» permite rotar la pantalla con un botón del mando (botón predeterminado: SELECT)."
#define OPTION_VAL_MANUAL_ES "Selección manual"
#define OPTION_VAL_LANDSCAPE_ES "Horizontal"
#define OPTION_VAL_PORTRAIT_ES "Vertical"
#define WSWAN_ROTATE_KEYMAP_LABEL_ES "Girar la asignación de botones"
#define WSWAN_ROTATE_KEYMAP_INFO_0_ES "Selecciona la asignación de botones preferida. «Selección automática» ajustará la asignación de los botones según la rotación actual de la imagen."
#define OPTION_VAL_AUTO_ES "Selección automática"
#define WSWAN_MONO_PALETTE_LABEL_ES "Paleta de colores"
#define WSWAN_MONO_PALETTE_INFO_0_ES "Selecciona la coloración de los juegos para WonderSwan (Mono). Las opciones «WS» imitan la pantalla de las WonderSwan originales."
#define OPTION_VAL_DEFAULT_ES "Escala de grises"
#define OPTION_VAL_WONDERSWAN_ES "WS (WonderSwan)"
#define OPTION_VAL_WONDESWAN_COLOR_ES "WS (WonderSwan Color)"
#define OPTION_VAL_SWANCRYSTAL_ES "WS (SwanCrystal)"
#define OPTION_VAL_GB_DMG_ES NULL
#define OPTION_VAL_GB_POCKET_ES NULL
#define OPTION_VAL_GB_LIGHT_ES NULL
#define OPTION_VAL_BLOSSOM_PINK_ES "Rosa Pétalo"
#define OPTION_VAL_BUBBLES_BLUE_ES "Azul Burbuja"
#define OPTION_VAL_BUTTERCUP_GREEN_ES "Verde Cactus"
#define OPTION_VAL_DIGIVICE_ES NULL
#define OPTION_VAL_GAME_COM_ES NULL
#define OPTION_VAL_GAMEKING_ES NULL
#define OPTION_VAL_GAME_MASTER_ES NULL
#define OPTION_VAL_GOLDEN_WILD_ES "Dorado Wild"
#define OPTION_VAL_GREENSCALE_ES "Escala de verdes"
#define OPTION_VAL_HOKAGE_ORANGE_ES "Naranja Hokage"
#define OPTION_VAL_LABO_FAWN_ES "Beige Labo"
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_ES "Súper Saiyan legendario"
#define OPTION_VAL_MICROVISION_ES NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_ES "Dorado Million Live"
#define OPTION_VAL_ODYSSEY_GOLD_ES "Dorado Odyssey"
#define OPTION_VAL_SHINY_SKY_BLUE_ES "Azul cielo SHINY"
#define OPTION_VAL_SLIME_BLUE_ES "Azul limo"
#define OPTION_VAL_TI_83_ES NULL
#define OPTION_VAL_TRAVEL_WOOD_ES "Madera de álbum de fotos"
#define OPTION_VAL_VIRTUAL_BOY_ES NULL
#define WSWAN_GFX_COLORS_LABEL_ES "Profundidad de color (es necesario reiniciar)"
#define WSWAN_GFX_COLORS_INFO_0_ES "El modo a 24 bits es más lento y no se encuentra disponible en todas las plataformas."
#define OPTION_VAL_16BIT_ES "Miles (16 bits)"
#define OPTION_VAL_24BIT_ES "Millones (24 bits)"
#define WSWAN_FRAMESKIP_LABEL_ES "Omisión de fotogramas"
#define WSWAN_FRAMESKIP_INFO_0_ES "Omite fotogramas para no saturar el búfer de audio (chasquidos en el sonido). Mejora el rendimiento a costa de perder fluidez visual. Selección automática omite fotogramas según lo aconseje el front-end. El valor Manual utiliza el ajuste Umbral de omisión de fotogramas (%)."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_ES "Umbral de omisión de fotogramas (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_ES "Cuando la omisión de fotogramas esté configurada como Manual, este ajuste especifica el umbral de ocupación del búfer de audio (en porcentaje) por debajo del cual se omitirán fotogramas si el valor. Un valor más elevado reduce el riesgo de chasquidos omitiendo fotogramas con una mayor frecuencia."
#define WSWAN_60HZ_MODE_LABEL_ES "Modo a 60 Hz"
#define WSWAN_60HZ_MODE_INFO_0_ES "Actualiza la imagen a 60 Hz en lugar de a la frecuencia de actualización nativa de WonderSwan (75 Hz) omitiendo un fotograma de cada cinco. Reducirá la fluidez de vídeo, pero evitará fragmentaciones de imagen en aquellos monitores que no sean compatibles con frecuencias superiores a 60 Hz."
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_ES "Frecuencia de muestreo de salida de audio"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_ES "Un valor más alto mejorará levemente la calidad del sonido, a costa del rendimiento."
#define WSWAN_SOUND_LOW_PASS_LABEL_ES "Filtro de audio"
#define WSWAN_SOUND_LOW_PASS_INFO_0_ES "Aplica un filtro de audio de paso bajo para «suavizar» el a veces molesto sonido que produce la WonderSwan."

struct retro_core_option_v2_category option_cats_es[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_es[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_ES,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_ES,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_ES },
         { "landscape", OPTION_VAL_LANDSCAPE_ES },
         { "portrait",  OPTION_VAL_PORTRAIT_ES },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_ES,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_ES,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_ES },
         { "disabled", OPTION_VAL_LANDSCAPE_ES },
         { "enabled",  OPTION_VAL_PORTRAIT_ES },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_ES,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_ES,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_ES },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_ES },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_ES },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_ES },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_ES },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_ES },
         { "gb_light",               OPTION_VAL_GB_LIGHT_ES },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_ES },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_ES },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_ES },
         { "digivice",               OPTION_VAL_DIGIVICE_ES },
         { "game_com",               OPTION_VAL_GAME_COM_ES },
         { "gameking",               OPTION_VAL_GAMEKING_ES },
         { "game_master",            OPTION_VAL_GAME_MASTER_ES },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_ES },
         { "greenscale",             OPTION_VAL_GREENSCALE_ES },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_ES },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_ES },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_ES },
         { "microvision",            OPTION_VAL_MICROVISION_ES },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_ES },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_ES },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_ES },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_ES },
         { "ti_83",                  OPTION_VAL_TI_83_ES },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_ES },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_ES },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_ES,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_ES,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_ES },
         { "24bit", OPTION_VAL_24BIT_ES },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_ES,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_ES,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_ES },
         { "manual",   OPTION_VAL_MANUAL_ES },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_ES,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_ES,
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
      WSWAN_60HZ_MODE_LABEL_ES,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_ES,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_ES,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_ES,
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
      WSWAN_SOUND_LOW_PASS_LABEL_ES,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_ES,
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
struct retro_core_options_v2 options_es = {
   option_cats_es,
   option_defs_es
};

/* RETRO_LANGUAGE_FA */

#define WSWAN_ROTATE_DISPLAY_LABEL_FA "چرخش نمایشگر"
#define WSWAN_ROTATE_DISPLAY_INFO_0_FA NULL
#define OPTION_VAL_MANUAL_FA "دستی"
#define OPTION_VAL_LANDSCAPE_FA NULL
#define OPTION_VAL_PORTRAIT_FA NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_FA NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_FA NULL
#define OPTION_VAL_AUTO_FA "خودکار"
#define WSWAN_MONO_PALETTE_LABEL_FA NULL
#define WSWAN_MONO_PALETTE_INFO_0_FA NULL
#define OPTION_VAL_DEFAULT_FA NULL
#define OPTION_VAL_WONDERSWAN_FA NULL
#define OPTION_VAL_WONDESWAN_COLOR_FA NULL
#define OPTION_VAL_SWANCRYSTAL_FA NULL
#define OPTION_VAL_GB_DMG_FA NULL
#define OPTION_VAL_GB_POCKET_FA NULL
#define OPTION_VAL_GB_LIGHT_FA NULL
#define OPTION_VAL_BLOSSOM_PINK_FA NULL
#define OPTION_VAL_BUBBLES_BLUE_FA NULL
#define OPTION_VAL_BUTTERCUP_GREEN_FA NULL
#define OPTION_VAL_DIGIVICE_FA NULL
#define OPTION_VAL_GAME_COM_FA NULL
#define OPTION_VAL_GAMEKING_FA NULL
#define OPTION_VAL_GAME_MASTER_FA NULL
#define OPTION_VAL_GOLDEN_WILD_FA NULL
#define OPTION_VAL_GREENSCALE_FA NULL
#define OPTION_VAL_HOKAGE_ORANGE_FA NULL
#define OPTION_VAL_LABO_FAWN_FA NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_FA NULL
#define OPTION_VAL_MICROVISION_FA NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_FA NULL
#define OPTION_VAL_ODYSSEY_GOLD_FA NULL
#define OPTION_VAL_SHINY_SKY_BLUE_FA NULL
#define OPTION_VAL_SLIME_BLUE_FA NULL
#define OPTION_VAL_TI_83_FA NULL
#define OPTION_VAL_TRAVEL_WOOD_FA NULL
#define OPTION_VAL_VIRTUAL_BOY_FA NULL
#define WSWAN_GFX_COLORS_LABEL_FA "عمق رنگ (نیازمند بازآغاز)"
#define WSWAN_GFX_COLORS_INFO_0_FA NULL
#define OPTION_VAL_16BIT_FA "هزار (۱۶-بیتی)"
#define OPTION_VAL_24BIT_FA "میلیون (۲۴-بیتی)"
#define WSWAN_FRAMESKIP_LABEL_FA "پرش فریم"
#define WSWAN_FRAMESKIP_INFO_0_FA "برای جلوگیری از اجرا شدن بافر صوتی (ترق زدن) از فریم‌ها پرش کنید. عملکرد را در ازای صاف‌بودن تصویر بهبود می‌بخشد. حالت «خودکار»هنگام توصیهٔ پیشانه از روی فریم‌ها می‌پرد. حالت «دستی» تنظیم «آستانهٔ پرش فریم (٪)» را فعال می‌کند."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_FA "آستانهٔ پرش فریم (٪)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_FA NULL
#define WSWAN_60HZ_MODE_LABEL_FA NULL
#define WSWAN_60HZ_MODE_INFO_0_FA NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_FA NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_FA NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_FA "پالایهٔ صدا"
#define WSWAN_SOUND_LOW_PASS_INFO_0_FA NULL

struct retro_core_option_v2_category option_cats_fa[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_fa[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_FA,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_FA,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_FA },
         { "landscape", OPTION_VAL_LANDSCAPE_FA },
         { "portrait",  OPTION_VAL_PORTRAIT_FA },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_FA,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_FA,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_FA },
         { "disabled", OPTION_VAL_LANDSCAPE_FA },
         { "enabled",  OPTION_VAL_PORTRAIT_FA },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_FA,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_FA,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_FA },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_FA },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_FA },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_FA },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_FA },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_FA },
         { "gb_light",               OPTION_VAL_GB_LIGHT_FA },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_FA },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_FA },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_FA },
         { "digivice",               OPTION_VAL_DIGIVICE_FA },
         { "game_com",               OPTION_VAL_GAME_COM_FA },
         { "gameking",               OPTION_VAL_GAMEKING_FA },
         { "game_master",            OPTION_VAL_GAME_MASTER_FA },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_FA },
         { "greenscale",             OPTION_VAL_GREENSCALE_FA },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_FA },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_FA },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_FA },
         { "microvision",            OPTION_VAL_MICROVISION_FA },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_FA },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_FA },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_FA },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_FA },
         { "ti_83",                  OPTION_VAL_TI_83_FA },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_FA },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_FA },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_FA,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_FA,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_FA },
         { "24bit", OPTION_VAL_24BIT_FA },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_FA,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_FA,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_FA },
         { "manual",   OPTION_VAL_MANUAL_FA },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_FA,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_FA,
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
      WSWAN_60HZ_MODE_LABEL_FA,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_FA,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_FA,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_FA,
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
      WSWAN_SOUND_LOW_PASS_LABEL_FA,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_FA,
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
struct retro_core_options_v2 options_fa = {
   option_cats_fa,
   option_defs_fa
};

/* RETRO_LANGUAGE_FI */

#define WSWAN_ROTATE_DISPLAY_LABEL_FI "Näytön kierto"
#define WSWAN_ROTATE_DISPLAY_INFO_0_FI NULL
#define OPTION_VAL_MANUAL_FI "Manuaalinen"
#define OPTION_VAL_LANDSCAPE_FI "Vaaka-asento"
#define OPTION_VAL_PORTRAIT_FI "Pystyasento"
#define WSWAN_ROTATE_KEYMAP_LABEL_FI NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_FI NULL
#define OPTION_VAL_AUTO_FI "Automaattinen"
#define WSWAN_MONO_PALETTE_LABEL_FI "Väripaletti"
#define WSWAN_MONO_PALETTE_INFO_0_FI NULL
#define OPTION_VAL_DEFAULT_FI "Harmaasävy"
#define OPTION_VAL_WONDERSWAN_FI NULL
#define OPTION_VAL_WONDESWAN_COLOR_FI NULL
#define OPTION_VAL_SWANCRYSTAL_FI NULL
#define OPTION_VAL_GB_DMG_FI NULL
#define OPTION_VAL_GB_POCKET_FI NULL
#define OPTION_VAL_GB_LIGHT_FI NULL
#define OPTION_VAL_BLOSSOM_PINK_FI NULL
#define OPTION_VAL_BUBBLES_BLUE_FI NULL
#define OPTION_VAL_BUTTERCUP_GREEN_FI NULL
#define OPTION_VAL_DIGIVICE_FI NULL
#define OPTION_VAL_GAME_COM_FI NULL
#define OPTION_VAL_GAMEKING_FI NULL
#define OPTION_VAL_GAME_MASTER_FI NULL
#define OPTION_VAL_GOLDEN_WILD_FI NULL
#define OPTION_VAL_GREENSCALE_FI NULL
#define OPTION_VAL_HOKAGE_ORANGE_FI NULL
#define OPTION_VAL_LABO_FAWN_FI NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_FI NULL
#define OPTION_VAL_MICROVISION_FI NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_FI NULL
#define OPTION_VAL_ODYSSEY_GOLD_FI NULL
#define OPTION_VAL_SHINY_SKY_BLUE_FI NULL
#define OPTION_VAL_SLIME_BLUE_FI NULL
#define OPTION_VAL_TI_83_FI NULL
#define OPTION_VAL_TRAVEL_WOOD_FI NULL
#define OPTION_VAL_VIRTUAL_BOY_FI NULL
#define WSWAN_GFX_COLORS_LABEL_FI "Värisyvyys (Uudelleenkäynnistys vaaditaan)"
#define WSWAN_GFX_COLORS_INFO_0_FI "24-bittinen on hitaampi eikä se ole saatavilla kaikilla alustoilla."
#define OPTION_VAL_16BIT_FI "Tuhansissa (16-bit)"
#define OPTION_VAL_24BIT_FI "Miljoonissa (24-bittinen)"
#define WSWAN_FRAMESKIP_LABEL_FI "Kuvanohitus"
#define WSWAN_FRAMESKIP_INFO_0_FI "Ohita kuvia välttääksesi äänipuskurin ali ajon (säröily). Parantaa suorituskykyä visuaalisen tasaisuuden kustannuksella. \"Automaatti\" ohittaa kehykset käyttöliittymän asetusten mukaan. \"Manuaalinen\" käyttää \"Kuvienohituksen arvo (%)\" -asetusta."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_FI "Kuvienohituksen arvo (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_FI NULL
#define WSWAN_60HZ_MODE_LABEL_FI "60 Hz:n tila"
#define WSWAN_60HZ_MODE_INFO_0_FI NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_FI NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_FI NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_FI "Äänisuodatin"
#define WSWAN_SOUND_LOW_PASS_INFO_0_FI NULL

struct retro_core_option_v2_category option_cats_fi[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_fi[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_FI,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_FI,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_FI },
         { "landscape", OPTION_VAL_LANDSCAPE_FI },
         { "portrait",  OPTION_VAL_PORTRAIT_FI },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_FI,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_FI,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_FI },
         { "disabled", OPTION_VAL_LANDSCAPE_FI },
         { "enabled",  OPTION_VAL_PORTRAIT_FI },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_FI,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_FI,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_FI },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_FI },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_FI },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_FI },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_FI },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_FI },
         { "gb_light",               OPTION_VAL_GB_LIGHT_FI },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_FI },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_FI },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_FI },
         { "digivice",               OPTION_VAL_DIGIVICE_FI },
         { "game_com",               OPTION_VAL_GAME_COM_FI },
         { "gameking",               OPTION_VAL_GAMEKING_FI },
         { "game_master",            OPTION_VAL_GAME_MASTER_FI },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_FI },
         { "greenscale",             OPTION_VAL_GREENSCALE_FI },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_FI },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_FI },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_FI },
         { "microvision",            OPTION_VAL_MICROVISION_FI },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_FI },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_FI },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_FI },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_FI },
         { "ti_83",                  OPTION_VAL_TI_83_FI },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_FI },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_FI },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_FI,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_FI,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_FI },
         { "24bit", OPTION_VAL_24BIT_FI },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_FI,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_FI,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_FI },
         { "manual",   OPTION_VAL_MANUAL_FI },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_FI,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_FI,
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
      WSWAN_60HZ_MODE_LABEL_FI,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_FI,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_FI,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_FI,
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
      WSWAN_SOUND_LOW_PASS_LABEL_FI,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_FI,
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
struct retro_core_options_v2 options_fi = {
   option_cats_fi,
   option_defs_fi
};

/* RETRO_LANGUAGE_FR */

#define WSWAN_ROTATE_DISPLAY_LABEL_FR "Rotation d'écran"
#define WSWAN_ROTATE_DISPLAY_INFO_0_FR "Choisir l'orientation d'écran de la console. 'Portrait' peut être utilisé pour obtenir la bonne mise en page pour les jeux en mode portrait sur un affichage conventionnel (en mode paysage). 'Manuel' active la rotation via la manette (touche par défaut : SELECT)."
#define OPTION_VAL_MANUAL_FR "Manuelle"
#define OPTION_VAL_LANDSCAPE_FR "Paysage"
#define OPTION_VAL_PORTRAIT_FR NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_FR "Faire pivoter le mappage des touches"
#define WSWAN_ROTATE_KEYMAP_INFO_0_FR "Choisir le mappage des touches préféré. 'Automatiquement' ajuste le mappage des touches pour correspondre à la rotation actuelle de l'affichage."
#define OPTION_VAL_AUTO_FR "Automatiquement"
#define WSWAN_MONO_PALETTE_LABEL_FR "Palette de couleurs"
#define WSWAN_MONO_PALETTE_INFO_0_FR "Choisir la colorisation des jeux WonderSwan (Mono), les palettes 'WS' imitent l'affichage du matériel original de la WonderSwan."
#define OPTION_VAL_DEFAULT_FR "Niveaux de gris"
#define OPTION_VAL_WONDERSWAN_FR NULL
#define OPTION_VAL_WONDESWAN_COLOR_FR NULL
#define OPTION_VAL_SWANCRYSTAL_FR NULL
#define OPTION_VAL_GB_DMG_FR "DMG Game Boy"
#define OPTION_VAL_GB_POCKET_FR NULL
#define OPTION_VAL_GB_LIGHT_FR NULL
#define OPTION_VAL_BLOSSOM_PINK_FR "Rose Belle"
#define OPTION_VAL_BUBBLES_BLUE_FR "Bleu Bulle"
#define OPTION_VAL_BUTTERCUP_GREEN_FR "Vert Rebelle"
#define OPTION_VAL_DIGIVICE_FR NULL
#define OPTION_VAL_GAME_COM_FR NULL
#define OPTION_VAL_GAMEKING_FR NULL
#define OPTION_VAL_GAME_MASTER_FR NULL
#define OPTION_VAL_GOLDEN_WILD_FR "Doré Wild"
#define OPTION_VAL_GREENSCALE_FR "Niveaux de vert"
#define OPTION_VAL_HOKAGE_ORANGE_FR "Orange Hokage"
#define OPTION_VAL_LABO_FAWN_FR "Faon Labo"
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_FR "Super Saiyan Légendaire"
#define OPTION_VAL_MICROVISION_FR NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_FR "Or Million Live"
#define OPTION_VAL_ODYSSEY_GOLD_FR "Or Odyssey"
#define OPTION_VAL_SHINY_SKY_BLUE_FR "Bleu Shiny Sky"
#define OPTION_VAL_SLIME_BLUE_FR "Bleu Slime"
#define OPTION_VAL_TI_83_FR NULL
#define OPTION_VAL_TRAVEL_WOOD_FR "Bois des voyages"
#define OPTION_VAL_VIRTUAL_BOY_FR NULL
#define WSWAN_GFX_COLORS_LABEL_FR "Profondeur des couleurs (Redémarrage requis)"
#define WSWAN_GFX_COLORS_INFO_0_FR "24 bits est plus lent et n'est pas disponibles sur toutes les plateformes."
#define OPTION_VAL_16BIT_FR "Milliers (16 bits)"
#define OPTION_VAL_24BIT_FR "Millions (24 bits)"
#define WSWAN_FRAMESKIP_LABEL_FR "Saut d'images"
#define WSWAN_FRAMESKIP_INFO_0_FR "Sauter des images pour éviter que le tampon audio ne soit sous-exécuté (crépitements). Améliore les performances au détriment de la fluidité visuelle. 'Auto' saute des images lorsque l'interface le conseille. 'Manuel' utilise le paramètre 'Seuil de saut d'images (%)'."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_FR "Seuil de saut d'images (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_FR "Lorsque 'Saut d'images' est réglé sur 'Manuel', cette option spécifie le seuil d'occupation du tampon audio (pourcentage) en dessous duquel des images seront sautées. Des valeurs plus élevées réduisent le risque de crépitements en faisant sauter des images plus fréquemment."
#define WSWAN_60HZ_MODE_LABEL_FR "Mode 60 Hz"
#define WSWAN_60HZ_MODE_INFO_0_FR "Mettre à jour l'affichage à 60 Hz au lieu de la fréquence de rafraîchissement native de la WonderSwan à 75 Hz en ignorant chaque cinquième image. Réduit la fluidité de la vidéo, mais évite les saccades sur les écrans qui ne prennent pas en charge plus de 60 Hz nativement."
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_FR "Fréquence d'échantillonnage de sortie audio"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_FR "Des valeurs plus élevées améliorent légèrement la qualité sonore au détriment des performances."
#define WSWAN_SOUND_LOW_PASS_LABEL_FR "Filtre audio"
#define WSWAN_SOUND_LOW_PASS_INFO_0_FR "Appliquer un filtre audio passe-bas pour 'adoucir' les chiptunes parfois durs produits par la WonderSwan."

struct retro_core_option_v2_category option_cats_fr[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_fr[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_FR,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_FR,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_FR },
         { "landscape", OPTION_VAL_LANDSCAPE_FR },
         { "portrait",  OPTION_VAL_PORTRAIT_FR },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_FR,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_FR,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_FR },
         { "disabled", OPTION_VAL_LANDSCAPE_FR },
         { "enabled",  OPTION_VAL_PORTRAIT_FR },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_FR,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_FR,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_FR },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_FR },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_FR },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_FR },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_FR },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_FR },
         { "gb_light",               OPTION_VAL_GB_LIGHT_FR },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_FR },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_FR },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_FR },
         { "digivice",               OPTION_VAL_DIGIVICE_FR },
         { "game_com",               OPTION_VAL_GAME_COM_FR },
         { "gameking",               OPTION_VAL_GAMEKING_FR },
         { "game_master",            OPTION_VAL_GAME_MASTER_FR },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_FR },
         { "greenscale",             OPTION_VAL_GREENSCALE_FR },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_FR },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_FR },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_FR },
         { "microvision",            OPTION_VAL_MICROVISION_FR },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_FR },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_FR },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_FR },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_FR },
         { "ti_83",                  OPTION_VAL_TI_83_FR },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_FR },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_FR },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_FR,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_FR,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_FR },
         { "24bit", OPTION_VAL_24BIT_FR },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_FR,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_FR,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_FR },
         { "manual",   OPTION_VAL_MANUAL_FR },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_FR,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_FR,
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
      WSWAN_60HZ_MODE_LABEL_FR,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_FR,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_FR,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_FR,
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
      WSWAN_SOUND_LOW_PASS_LABEL_FR,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_FR,
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
struct retro_core_options_v2 options_fr = {
   option_cats_fr,
   option_defs_fr
};

/* RETRO_LANGUAGE_GL */

#define WSWAN_ROTATE_DISPLAY_LABEL_GL NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_GL NULL
#define OPTION_VAL_MANUAL_GL NULL
#define OPTION_VAL_LANDSCAPE_GL NULL
#define OPTION_VAL_PORTRAIT_GL NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_GL NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_GL NULL
#define OPTION_VAL_AUTO_GL NULL
#define WSWAN_MONO_PALETTE_LABEL_GL NULL
#define WSWAN_MONO_PALETTE_INFO_0_GL NULL
#define OPTION_VAL_DEFAULT_GL NULL
#define OPTION_VAL_WONDERSWAN_GL NULL
#define OPTION_VAL_WONDESWAN_COLOR_GL NULL
#define OPTION_VAL_SWANCRYSTAL_GL NULL
#define OPTION_VAL_GB_DMG_GL NULL
#define OPTION_VAL_GB_POCKET_GL NULL
#define OPTION_VAL_GB_LIGHT_GL NULL
#define OPTION_VAL_BLOSSOM_PINK_GL NULL
#define OPTION_VAL_BUBBLES_BLUE_GL NULL
#define OPTION_VAL_BUTTERCUP_GREEN_GL NULL
#define OPTION_VAL_DIGIVICE_GL NULL
#define OPTION_VAL_GAME_COM_GL NULL
#define OPTION_VAL_GAMEKING_GL NULL
#define OPTION_VAL_GAME_MASTER_GL NULL
#define OPTION_VAL_GOLDEN_WILD_GL NULL
#define OPTION_VAL_GREENSCALE_GL NULL
#define OPTION_VAL_HOKAGE_ORANGE_GL NULL
#define OPTION_VAL_LABO_FAWN_GL NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_GL NULL
#define OPTION_VAL_MICROVISION_GL NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_GL NULL
#define OPTION_VAL_ODYSSEY_GOLD_GL NULL
#define OPTION_VAL_SHINY_SKY_BLUE_GL NULL
#define OPTION_VAL_SLIME_BLUE_GL NULL
#define OPTION_VAL_TI_83_GL NULL
#define OPTION_VAL_TRAVEL_WOOD_GL NULL
#define OPTION_VAL_VIRTUAL_BOY_GL NULL
#define WSWAN_GFX_COLORS_LABEL_GL NULL
#define WSWAN_GFX_COLORS_INFO_0_GL NULL
#define OPTION_VAL_16BIT_GL NULL
#define OPTION_VAL_24BIT_GL NULL
#define WSWAN_FRAMESKIP_LABEL_GL NULL
#define WSWAN_FRAMESKIP_INFO_0_GL NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_GL NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_GL NULL
#define WSWAN_60HZ_MODE_LABEL_GL NULL
#define WSWAN_60HZ_MODE_INFO_0_GL NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_GL NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_GL NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_GL NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_GL NULL

struct retro_core_option_v2_category option_cats_gl[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_gl[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_GL,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_GL,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_GL },
         { "landscape", OPTION_VAL_LANDSCAPE_GL },
         { "portrait",  OPTION_VAL_PORTRAIT_GL },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_GL,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_GL,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_GL },
         { "disabled", OPTION_VAL_LANDSCAPE_GL },
         { "enabled",  OPTION_VAL_PORTRAIT_GL },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_GL,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_GL,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_GL },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_GL },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_GL },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_GL },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_GL },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_GL },
         { "gb_light",               OPTION_VAL_GB_LIGHT_GL },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_GL },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_GL },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_GL },
         { "digivice",               OPTION_VAL_DIGIVICE_GL },
         { "game_com",               OPTION_VAL_GAME_COM_GL },
         { "gameking",               OPTION_VAL_GAMEKING_GL },
         { "game_master",            OPTION_VAL_GAME_MASTER_GL },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_GL },
         { "greenscale",             OPTION_VAL_GREENSCALE_GL },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_GL },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_GL },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_GL },
         { "microvision",            OPTION_VAL_MICROVISION_GL },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_GL },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_GL },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_GL },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_GL },
         { "ti_83",                  OPTION_VAL_TI_83_GL },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_GL },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_GL },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_GL,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_GL,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_GL },
         { "24bit", OPTION_VAL_24BIT_GL },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_GL,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_GL,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_GL },
         { "manual",   OPTION_VAL_MANUAL_GL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_GL,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_GL,
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
      WSWAN_60HZ_MODE_LABEL_GL,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_GL,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_GL,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_GL,
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
      WSWAN_SOUND_LOW_PASS_LABEL_GL,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_GL,
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
struct retro_core_options_v2 options_gl = {
   option_cats_gl,
   option_defs_gl
};

/* RETRO_LANGUAGE_HE */

#define WSWAN_ROTATE_DISPLAY_LABEL_HE NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_HE NULL
#define OPTION_VAL_MANUAL_HE NULL
#define OPTION_VAL_LANDSCAPE_HE NULL
#define OPTION_VAL_PORTRAIT_HE NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_HE NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_HE NULL
#define OPTION_VAL_AUTO_HE NULL
#define WSWAN_MONO_PALETTE_LABEL_HE NULL
#define WSWAN_MONO_PALETTE_INFO_0_HE NULL
#define OPTION_VAL_DEFAULT_HE NULL
#define OPTION_VAL_WONDERSWAN_HE NULL
#define OPTION_VAL_WONDESWAN_COLOR_HE NULL
#define OPTION_VAL_SWANCRYSTAL_HE NULL
#define OPTION_VAL_GB_DMG_HE NULL
#define OPTION_VAL_GB_POCKET_HE NULL
#define OPTION_VAL_GB_LIGHT_HE NULL
#define OPTION_VAL_BLOSSOM_PINK_HE NULL
#define OPTION_VAL_BUBBLES_BLUE_HE NULL
#define OPTION_VAL_BUTTERCUP_GREEN_HE NULL
#define OPTION_VAL_DIGIVICE_HE NULL
#define OPTION_VAL_GAME_COM_HE NULL
#define OPTION_VAL_GAMEKING_HE NULL
#define OPTION_VAL_GAME_MASTER_HE NULL
#define OPTION_VAL_GOLDEN_WILD_HE NULL
#define OPTION_VAL_GREENSCALE_HE NULL
#define OPTION_VAL_HOKAGE_ORANGE_HE NULL
#define OPTION_VAL_LABO_FAWN_HE NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_HE NULL
#define OPTION_VAL_MICROVISION_HE NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_HE NULL
#define OPTION_VAL_ODYSSEY_GOLD_HE NULL
#define OPTION_VAL_SHINY_SKY_BLUE_HE NULL
#define OPTION_VAL_SLIME_BLUE_HE NULL
#define OPTION_VAL_TI_83_HE NULL
#define OPTION_VAL_TRAVEL_WOOD_HE NULL
#define OPTION_VAL_VIRTUAL_BOY_HE NULL
#define WSWAN_GFX_COLORS_LABEL_HE NULL
#define WSWAN_GFX_COLORS_INFO_0_HE NULL
#define OPTION_VAL_16BIT_HE NULL
#define OPTION_VAL_24BIT_HE NULL
#define WSWAN_FRAMESKIP_LABEL_HE NULL
#define WSWAN_FRAMESKIP_INFO_0_HE NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_HE NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_HE NULL
#define WSWAN_60HZ_MODE_LABEL_HE NULL
#define WSWAN_60HZ_MODE_INFO_0_HE NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_HE NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_HE NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_HE NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_HE NULL

struct retro_core_option_v2_category option_cats_he[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_he[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_HE,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_HE,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_HE },
         { "landscape", OPTION_VAL_LANDSCAPE_HE },
         { "portrait",  OPTION_VAL_PORTRAIT_HE },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_HE,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_HE,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_HE },
         { "disabled", OPTION_VAL_LANDSCAPE_HE },
         { "enabled",  OPTION_VAL_PORTRAIT_HE },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_HE,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_HE,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_HE },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_HE },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_HE },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_HE },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_HE },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_HE },
         { "gb_light",               OPTION_VAL_GB_LIGHT_HE },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_HE },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_HE },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_HE },
         { "digivice",               OPTION_VAL_DIGIVICE_HE },
         { "game_com",               OPTION_VAL_GAME_COM_HE },
         { "gameking",               OPTION_VAL_GAMEKING_HE },
         { "game_master",            OPTION_VAL_GAME_MASTER_HE },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_HE },
         { "greenscale",             OPTION_VAL_GREENSCALE_HE },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_HE },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_HE },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_HE },
         { "microvision",            OPTION_VAL_MICROVISION_HE },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_HE },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_HE },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_HE },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_HE },
         { "ti_83",                  OPTION_VAL_TI_83_HE },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_HE },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_HE },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_HE,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_HE,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_HE },
         { "24bit", OPTION_VAL_24BIT_HE },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_HE,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_HE,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_HE },
         { "manual",   OPTION_VAL_MANUAL_HE },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_HE,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_HE,
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
      WSWAN_60HZ_MODE_LABEL_HE,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_HE,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_HE,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_HE,
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
      WSWAN_SOUND_LOW_PASS_LABEL_HE,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_HE,
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
struct retro_core_options_v2 options_he = {
   option_cats_he,
   option_defs_he
};

/* RETRO_LANGUAGE_HR */

#define WSWAN_ROTATE_DISPLAY_LABEL_HR NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_HR NULL
#define OPTION_VAL_MANUAL_HR NULL
#define OPTION_VAL_LANDSCAPE_HR NULL
#define OPTION_VAL_PORTRAIT_HR NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_HR NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_HR NULL
#define OPTION_VAL_AUTO_HR NULL
#define WSWAN_MONO_PALETTE_LABEL_HR NULL
#define WSWAN_MONO_PALETTE_INFO_0_HR NULL
#define OPTION_VAL_DEFAULT_HR NULL
#define OPTION_VAL_WONDERSWAN_HR NULL
#define OPTION_VAL_WONDESWAN_COLOR_HR NULL
#define OPTION_VAL_SWANCRYSTAL_HR NULL
#define OPTION_VAL_GB_DMG_HR NULL
#define OPTION_VAL_GB_POCKET_HR NULL
#define OPTION_VAL_GB_LIGHT_HR NULL
#define OPTION_VAL_BLOSSOM_PINK_HR NULL
#define OPTION_VAL_BUBBLES_BLUE_HR NULL
#define OPTION_VAL_BUTTERCUP_GREEN_HR NULL
#define OPTION_VAL_DIGIVICE_HR NULL
#define OPTION_VAL_GAME_COM_HR NULL
#define OPTION_VAL_GAMEKING_HR NULL
#define OPTION_VAL_GAME_MASTER_HR NULL
#define OPTION_VAL_GOLDEN_WILD_HR NULL
#define OPTION_VAL_GREENSCALE_HR NULL
#define OPTION_VAL_HOKAGE_ORANGE_HR NULL
#define OPTION_VAL_LABO_FAWN_HR NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_HR NULL
#define OPTION_VAL_MICROVISION_HR NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_HR NULL
#define OPTION_VAL_ODYSSEY_GOLD_HR NULL
#define OPTION_VAL_SHINY_SKY_BLUE_HR NULL
#define OPTION_VAL_SLIME_BLUE_HR NULL
#define OPTION_VAL_TI_83_HR NULL
#define OPTION_VAL_TRAVEL_WOOD_HR NULL
#define OPTION_VAL_VIRTUAL_BOY_HR NULL
#define WSWAN_GFX_COLORS_LABEL_HR NULL
#define WSWAN_GFX_COLORS_INFO_0_HR NULL
#define OPTION_VAL_16BIT_HR NULL
#define OPTION_VAL_24BIT_HR NULL
#define WSWAN_FRAMESKIP_LABEL_HR NULL
#define WSWAN_FRAMESKIP_INFO_0_HR NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_HR NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_HR NULL
#define WSWAN_60HZ_MODE_LABEL_HR NULL
#define WSWAN_60HZ_MODE_INFO_0_HR NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_HR NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_HR NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_HR NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_HR NULL

struct retro_core_option_v2_category option_cats_hr[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_hr[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_HR,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_HR,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_HR },
         { "landscape", OPTION_VAL_LANDSCAPE_HR },
         { "portrait",  OPTION_VAL_PORTRAIT_HR },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_HR,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_HR,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_HR },
         { "disabled", OPTION_VAL_LANDSCAPE_HR },
         { "enabled",  OPTION_VAL_PORTRAIT_HR },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_HR,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_HR,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_HR },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_HR },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_HR },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_HR },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_HR },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_HR },
         { "gb_light",               OPTION_VAL_GB_LIGHT_HR },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_HR },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_HR },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_HR },
         { "digivice",               OPTION_VAL_DIGIVICE_HR },
         { "game_com",               OPTION_VAL_GAME_COM_HR },
         { "gameking",               OPTION_VAL_GAMEKING_HR },
         { "game_master",            OPTION_VAL_GAME_MASTER_HR },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_HR },
         { "greenscale",             OPTION_VAL_GREENSCALE_HR },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_HR },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_HR },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_HR },
         { "microvision",            OPTION_VAL_MICROVISION_HR },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_HR },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_HR },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_HR },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_HR },
         { "ti_83",                  OPTION_VAL_TI_83_HR },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_HR },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_HR },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_HR,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_HR,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_HR },
         { "24bit", OPTION_VAL_24BIT_HR },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_HR,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_HR,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_HR },
         { "manual",   OPTION_VAL_MANUAL_HR },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_HR,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_HR,
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
      WSWAN_60HZ_MODE_LABEL_HR,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_HR,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_HR,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_HR,
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
      WSWAN_SOUND_LOW_PASS_LABEL_HR,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_HR,
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
struct retro_core_options_v2 options_hr = {
   option_cats_hr,
   option_defs_hr
};

/* RETRO_LANGUAGE_HU */

#define WSWAN_ROTATE_DISPLAY_LABEL_HU "Kijelző elforgatás"
#define WSWAN_ROTATE_DISPLAY_INFO_0_HU "A konzol képernyőjének tájolása. Az Álló móddal érhető el az álló módú játékok helyes tájolása egy hagyományos (fekvő) kijelzőn. A Kézi mód engedélyezi a forgatást a kontrolleren (alapértelmezett gomb: Select)."
#define OPTION_VAL_MANUAL_HU "Kézi"
#define OPTION_VAL_LANDSCAPE_HU "Fekvő"
#define OPTION_VAL_PORTRAIT_HU "Álló"
#define WSWAN_ROTATE_KEYMAP_LABEL_HU "Gombkiosztás elforgatása"
#define WSWAN_ROTATE_KEYMAP_INFO_0_HU "A kívánt gombkiosztás. Az automatikus a kijelző aktuális elforgatásához igazodik."
#define OPTION_VAL_AUTO_HU "Automatikus"
#define WSWAN_MONO_PALETTE_LABEL_HU "Színpaletta"
#define WSWAN_MONO_PALETTE_INFO_0_HU "A WonderSwan (Mono) játékok színezése. A \"WS\" paletták az eredeti WonderSwan hardver kijelzőjét utánozzák."
#define OPTION_VAL_DEFAULT_HU "Szürkeárnyalatos"
#define OPTION_VAL_WONDERSWAN_HU NULL
#define OPTION_VAL_WONDESWAN_COLOR_HU NULL
#define OPTION_VAL_SWANCRYSTAL_HU NULL
#define OPTION_VAL_GB_DMG_HU NULL
#define OPTION_VAL_GB_POCKET_HU NULL
#define OPTION_VAL_GB_LIGHT_HU NULL
#define OPTION_VAL_BLOSSOM_PINK_HU NULL
#define OPTION_VAL_BUBBLES_BLUE_HU NULL
#define OPTION_VAL_BUTTERCUP_GREEN_HU NULL
#define OPTION_VAL_DIGIVICE_HU NULL
#define OPTION_VAL_GAME_COM_HU NULL
#define OPTION_VAL_GAMEKING_HU NULL
#define OPTION_VAL_GAME_MASTER_HU NULL
#define OPTION_VAL_GOLDEN_WILD_HU NULL
#define OPTION_VAL_GREENSCALE_HU NULL
#define OPTION_VAL_HOKAGE_ORANGE_HU NULL
#define OPTION_VAL_LABO_FAWN_HU NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_HU NULL
#define OPTION_VAL_MICROVISION_HU NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_HU NULL
#define OPTION_VAL_ODYSSEY_GOLD_HU NULL
#define OPTION_VAL_SHINY_SKY_BLUE_HU NULL
#define OPTION_VAL_SLIME_BLUE_HU NULL
#define OPTION_VAL_TI_83_HU NULL
#define OPTION_VAL_TRAVEL_WOOD_HU NULL
#define OPTION_VAL_VIRTUAL_BOY_HU NULL
#define WSWAN_GFX_COLORS_LABEL_HU "Színmélység (újraindítás szükséges)"
#define WSWAN_GFX_COLORS_INFO_0_HU "A 24 bites lassabb, és nem elérhető minden platformon."
#define OPTION_VAL_16BIT_HU "Több ezer (16 bites)"
#define OPTION_VAL_24BIT_HU "Több millió (24 bites)"
#define WSWAN_FRAMESKIP_LABEL_HU "Képkocka kihagyás"
#define WSWAN_FRAMESKIP_INFO_0_HU "A hangbuffer kiürülése (recsegés) elkerülésére képkockák eldobása. Javít a teljesítményen a vizuális folyamatosság kárára. Az \"Automatikus\" beállítás akkor dob képkockát, ha a frontend azt javasolja. A \"Kézi\" a \"Képkocka kihagyás határértéke (%)\" beállítást használja."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_HU "Képkocka kihagyás határértéke (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_HU "Ha a \"Képkocka kihagyás\" \"Kézi\" értékre van állítva, ez határozza meg a hangpuffer telítettségi határát (százalékban), amely alatt képkockákat dob el. Nagyobb érték csökkenti a recsegés kockázatát azáltal, hogy gyakrabban dob el képkockát."
#define WSWAN_60HZ_MODE_LABEL_HU "60 Hz-es mód"
#define WSWAN_60HZ_MODE_INFO_0_HU "A kijelző frissítése 60 Hz-el a WonderSwan eredeti 75 Hz-es gyakorisága helyett, minden ötödik képkocka eldobásával. Csökkenti a kép folyamatosságát, de elkerüli a képszakadást olyan kijelzőkön, amelyek nem támoganak 60 Hz-nél nagyobb frissítést."
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_HU "Hangkimenet mintavételi frekvenciája"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_HU "Nagyobb értékek enyhén javítják a hangminőséget a teljesítmény rovására."
#define WSWAN_SOUND_LOW_PASS_LABEL_HU "Hangszűrő"
#define WSWAN_SOUND_LOW_PASS_INFO_0_HU "Aluláteresztő hangszűrő engedélyezése, a WonderSwan játékok néha \"nyers\" chiptune hangjának tompításához."

struct retro_core_option_v2_category option_cats_hu[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_hu[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_HU,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_HU,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_HU },
         { "landscape", OPTION_VAL_LANDSCAPE_HU },
         { "portrait",  OPTION_VAL_PORTRAIT_HU },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_HU,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_HU,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_HU },
         { "disabled", OPTION_VAL_LANDSCAPE_HU },
         { "enabled",  OPTION_VAL_PORTRAIT_HU },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_HU,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_HU,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_HU },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_HU },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_HU },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_HU },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_HU },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_HU },
         { "gb_light",               OPTION_VAL_GB_LIGHT_HU },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_HU },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_HU },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_HU },
         { "digivice",               OPTION_VAL_DIGIVICE_HU },
         { "game_com",               OPTION_VAL_GAME_COM_HU },
         { "gameking",               OPTION_VAL_GAMEKING_HU },
         { "game_master",            OPTION_VAL_GAME_MASTER_HU },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_HU },
         { "greenscale",             OPTION_VAL_GREENSCALE_HU },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_HU },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_HU },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_HU },
         { "microvision",            OPTION_VAL_MICROVISION_HU },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_HU },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_HU },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_HU },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_HU },
         { "ti_83",                  OPTION_VAL_TI_83_HU },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_HU },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_HU },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_HU,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_HU,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_HU },
         { "24bit", OPTION_VAL_24BIT_HU },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_HU,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_HU,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_HU },
         { "manual",   OPTION_VAL_MANUAL_HU },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_HU,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_HU,
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
      WSWAN_60HZ_MODE_LABEL_HU,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_HU,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_HU,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_HU,
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
      WSWAN_SOUND_LOW_PASS_LABEL_HU,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_HU,
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
struct retro_core_options_v2 options_hu = {
   option_cats_hu,
   option_defs_hu
};

/* RETRO_LANGUAGE_ID */

#define WSWAN_ROTATE_DISPLAY_LABEL_ID NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_ID "Pilih orientasi layar konsol. 'Menegak' bisa untuk membetulkan tata letak permainan yang berlayar menegak pada tampilan layar mendatar (landscape). 'Manual' mengakifkan perputaran lewat stik kendali (tombol bawaan: SELECT)."
#define OPTION_VAL_MANUAL_ID NULL
#define OPTION_VAL_LANDSCAPE_ID "Mendatar"
#define OPTION_VAL_PORTRAIT_ID "Menegak"
#define WSWAN_ROTATE_KEYMAP_LABEL_ID NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_ID NULL
#define OPTION_VAL_AUTO_ID "Otomatis"
#define WSWAN_MONO_PALETTE_LABEL_ID NULL
#define WSWAN_MONO_PALETTE_INFO_0_ID NULL
#define OPTION_VAL_DEFAULT_ID NULL
#define OPTION_VAL_WONDERSWAN_ID NULL
#define OPTION_VAL_WONDESWAN_COLOR_ID NULL
#define OPTION_VAL_SWANCRYSTAL_ID NULL
#define OPTION_VAL_GB_DMG_ID NULL
#define OPTION_VAL_GB_POCKET_ID NULL
#define OPTION_VAL_GB_LIGHT_ID NULL
#define OPTION_VAL_BLOSSOM_PINK_ID NULL
#define OPTION_VAL_BUBBLES_BLUE_ID NULL
#define OPTION_VAL_BUTTERCUP_GREEN_ID NULL
#define OPTION_VAL_DIGIVICE_ID NULL
#define OPTION_VAL_GAME_COM_ID NULL
#define OPTION_VAL_GAMEKING_ID NULL
#define OPTION_VAL_GAME_MASTER_ID NULL
#define OPTION_VAL_GOLDEN_WILD_ID NULL
#define OPTION_VAL_GREENSCALE_ID NULL
#define OPTION_VAL_HOKAGE_ORANGE_ID NULL
#define OPTION_VAL_LABO_FAWN_ID NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_ID NULL
#define OPTION_VAL_MICROVISION_ID NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_ID NULL
#define OPTION_VAL_ODYSSEY_GOLD_ID NULL
#define OPTION_VAL_SHINY_SKY_BLUE_ID NULL
#define OPTION_VAL_SLIME_BLUE_ID NULL
#define OPTION_VAL_TI_83_ID NULL
#define OPTION_VAL_TRAVEL_WOOD_ID NULL
#define OPTION_VAL_VIRTUAL_BOY_ID NULL
#define WSWAN_GFX_COLORS_LABEL_ID NULL
#define WSWAN_GFX_COLORS_INFO_0_ID NULL
#define OPTION_VAL_16BIT_ID NULL
#define OPTION_VAL_24BIT_ID NULL
#define WSWAN_FRAMESKIP_LABEL_ID NULL
#define WSWAN_FRAMESKIP_INFO_0_ID NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_ID NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_ID "Saat 'Lewati Bingkai' diatur ke 'Manual', (persentase) ambang batas hunian penyangga audio di bawah akan menentukan bingkai mana yang dilewati. Nilai yang lebih tinggi mengurangi risiko keretakan dengan cara membuat laju bingkai sering anjlok."
#define WSWAN_60HZ_MODE_LABEL_ID NULL
#define WSWAN_60HZ_MODE_INFO_0_ID NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_ID NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_ID NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_ID NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_ID NULL

struct retro_core_option_v2_category option_cats_id[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_id[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_ID,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_ID,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_ID },
         { "landscape", OPTION_VAL_LANDSCAPE_ID },
         { "portrait",  OPTION_VAL_PORTRAIT_ID },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_ID,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_ID,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_ID },
         { "disabled", OPTION_VAL_LANDSCAPE_ID },
         { "enabled",  OPTION_VAL_PORTRAIT_ID },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_ID,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_ID,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_ID },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_ID },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_ID },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_ID },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_ID },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_ID },
         { "gb_light",               OPTION_VAL_GB_LIGHT_ID },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_ID },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_ID },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_ID },
         { "digivice",               OPTION_VAL_DIGIVICE_ID },
         { "game_com",               OPTION_VAL_GAME_COM_ID },
         { "gameking",               OPTION_VAL_GAMEKING_ID },
         { "game_master",            OPTION_VAL_GAME_MASTER_ID },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_ID },
         { "greenscale",             OPTION_VAL_GREENSCALE_ID },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_ID },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_ID },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_ID },
         { "microvision",            OPTION_VAL_MICROVISION_ID },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_ID },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_ID },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_ID },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_ID },
         { "ti_83",                  OPTION_VAL_TI_83_ID },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_ID },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_ID },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_ID,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_ID,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_ID },
         { "24bit", OPTION_VAL_24BIT_ID },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_ID,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_ID,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_ID },
         { "manual",   OPTION_VAL_MANUAL_ID },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_ID,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_ID,
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
      WSWAN_60HZ_MODE_LABEL_ID,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_ID,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_ID,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_ID,
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
      WSWAN_SOUND_LOW_PASS_LABEL_ID,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_ID,
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
struct retro_core_options_v2 options_id = {
   option_cats_id,
   option_defs_id
};

/* RETRO_LANGUAGE_IT */

#define WSWAN_ROTATE_DISPLAY_LABEL_IT "Rotazione Display"
#define WSWAN_ROTATE_DISPLAY_INFO_0_IT "Scegli l'orientamento dello schermo della console. 'Portrait' può essere utilizzato per ottenere il corretto layout dei giochi orientati al ritratto su un display convenzionale (orizzontale). 'Manuale' abilita la rotazione tramite il controller (pulsante predefinito: SELECT)."
#define OPTION_VAL_MANUAL_IT "Manuale"
#define OPTION_VAL_LANDSCAPE_IT "Orizzontale"
#define OPTION_VAL_PORTRAIT_IT "Verticale"
#define WSWAN_ROTATE_KEYMAP_LABEL_IT "Mapping Pulsante Di Rotazione"
#define WSWAN_ROTATE_KEYMAP_INFO_0_IT "Scegli la mappatura dei pulsanti preferiti. 'Auto' regola la mappatura dei pulsanti in base alla rotazione corrente del display."
#define OPTION_VAL_AUTO_IT NULL
#define WSWAN_MONO_PALETTE_LABEL_IT "Tavolozza Colore"
#define WSWAN_MONO_PALETTE_INFO_0_IT "Scegli la colorazione dei giochi WonderSwan (Mono). Le tavolozze 'WS' imitano il display dell'hardware originale WonderSwan."
#define OPTION_VAL_DEFAULT_IT "Scala di grigi"
#define OPTION_VAL_WONDERSWAN_IT NULL
#define OPTION_VAL_WONDESWAN_COLOR_IT NULL
#define OPTION_VAL_SWANCRYSTAL_IT NULL
#define OPTION_VAL_GB_DMG_IT NULL
#define OPTION_VAL_GB_POCKET_IT NULL
#define OPTION_VAL_GB_LIGHT_IT NULL
#define OPTION_VAL_BLOSSOM_PINK_IT NULL
#define OPTION_VAL_BUBBLES_BLUE_IT "Blu Bolle"
#define OPTION_VAL_BUTTERCUP_GREEN_IT "Buttercup Verde"
#define OPTION_VAL_DIGIVICE_IT NULL
#define OPTION_VAL_GAME_COM_IT NULL
#define OPTION_VAL_GAMEKING_IT NULL
#define OPTION_VAL_GAME_MASTER_IT NULL
#define OPTION_VAL_GOLDEN_WILD_IT NULL
#define OPTION_VAL_GREENSCALE_IT NULL
#define OPTION_VAL_HOKAGE_ORANGE_IT NULL
#define OPTION_VAL_LABO_FAWN_IT NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_IT "Super Saiyan Leggendario"
#define OPTION_VAL_MICROVISION_IT NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_IT NULL
#define OPTION_VAL_ODYSSEY_GOLD_IT NULL
#define OPTION_VAL_SHINY_SKY_BLUE_IT NULL
#define OPTION_VAL_SLIME_BLUE_IT NULL
#define OPTION_VAL_TI_83_IT NULL
#define OPTION_VAL_TRAVEL_WOOD_IT NULL
#define OPTION_VAL_VIRTUAL_BOY_IT NULL
#define WSWAN_GFX_COLORS_LABEL_IT "Profondità Colore (Riavvio Richiesto)"
#define WSWAN_GFX_COLORS_INFO_0_IT "24 bit è più lento e non disponibile su tutte le piattaforme."
#define OPTION_VAL_16BIT_IT "Migliaia (16-bit)"
#define OPTION_VAL_24BIT_IT "Milioni (24-bit)"
#define WSWAN_FRAMESKIP_LABEL_IT NULL
#define WSWAN_FRAMESKIP_INFO_0_IT "Salta i fotogrammi per evitare la sottoesecuzione del buffer audio (crackling). Migliora le prestazioni a scapito della fluidità visiva. 'Auto' salta i quadri quando consigliato dal frontend. 'Manuale' utilizza l'impostazione 'Frameskip Threshold (%)'."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_IT "Soglia salto fotogrammi (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_IT "Quando 'Frameskip' è impostato a 'Manuale', specifica la soglia di occupazione del buffer audio (percentuale) al di sotto della quale i quadri verranno saltati. Valori più elevati riducono il rischio di rompere causando un calo più frequente dei fotogrammi."
#define WSWAN_60HZ_MODE_LABEL_IT "Modalità 60Hz"
#define WSWAN_60HZ_MODE_INFO_0_IT "Aggiornare il display a 60Hz invece della frequenza di aggiornamento nativa di 75Hz WonderSwan cadendo ogni quinto fotogramma. Riduce la fluidità del video, ma evita la lacerazione dello schermo su display che non supportano l'operazione nativa sopra i 60Hz."
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_IT "Campionatura Uscita Suono"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_IT "Valori più elevati migliorano leggermente la qualità del suono a scapito delle prestazioni."
#define WSWAN_SOUND_LOW_PASS_LABEL_IT "Filtro Audio"
#define WSWAN_SOUND_LOW_PASS_INFO_0_IT "Applica un filtro audio passa-basso per ammorbidire i chiptunes talvolta duri prodotti dal WonderSwan."

struct retro_core_option_v2_category option_cats_it[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_it[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_IT,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_IT,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_IT },
         { "landscape", OPTION_VAL_LANDSCAPE_IT },
         { "portrait",  OPTION_VAL_PORTRAIT_IT },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_IT,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_IT,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_IT },
         { "disabled", OPTION_VAL_LANDSCAPE_IT },
         { "enabled",  OPTION_VAL_PORTRAIT_IT },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_IT,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_IT,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_IT },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_IT },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_IT },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_IT },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_IT },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_IT },
         { "gb_light",               OPTION_VAL_GB_LIGHT_IT },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_IT },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_IT },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_IT },
         { "digivice",               OPTION_VAL_DIGIVICE_IT },
         { "game_com",               OPTION_VAL_GAME_COM_IT },
         { "gameking",               OPTION_VAL_GAMEKING_IT },
         { "game_master",            OPTION_VAL_GAME_MASTER_IT },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_IT },
         { "greenscale",             OPTION_VAL_GREENSCALE_IT },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_IT },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_IT },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_IT },
         { "microvision",            OPTION_VAL_MICROVISION_IT },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_IT },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_IT },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_IT },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_IT },
         { "ti_83",                  OPTION_VAL_TI_83_IT },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_IT },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_IT },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_IT,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_IT,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_IT },
         { "24bit", OPTION_VAL_24BIT_IT },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_IT,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_IT,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_IT },
         { "manual",   OPTION_VAL_MANUAL_IT },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_IT,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_IT,
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
      WSWAN_60HZ_MODE_LABEL_IT,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_IT,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_IT,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_IT,
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
      WSWAN_SOUND_LOW_PASS_LABEL_IT,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_IT,
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
struct retro_core_options_v2 options_it = {
   option_cats_it,
   option_defs_it
};

/* RETRO_LANGUAGE_JA */

#define WSWAN_ROTATE_DISPLAY_LABEL_JA "ディスプレイの回転"
#define WSWAN_ROTATE_DISPLAY_INFO_0_JA NULL
#define OPTION_VAL_MANUAL_JA "手動"
#define OPTION_VAL_LANDSCAPE_JA NULL
#define OPTION_VAL_PORTRAIT_JA NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_JA NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_JA NULL
#define OPTION_VAL_AUTO_JA "自動"
#define WSWAN_MONO_PALETTE_LABEL_JA "カラーパレット"
#define WSWAN_MONO_PALETTE_INFO_0_JA NULL
#define OPTION_VAL_DEFAULT_JA "グレースケール"
#define OPTION_VAL_WONDERSWAN_JA NULL
#define OPTION_VAL_WONDESWAN_COLOR_JA NULL
#define OPTION_VAL_SWANCRYSTAL_JA NULL
#define OPTION_VAL_GB_DMG_JA NULL
#define OPTION_VAL_GB_POCKET_JA NULL
#define OPTION_VAL_GB_LIGHT_JA NULL
#define OPTION_VAL_BLOSSOM_PINK_JA NULL
#define OPTION_VAL_BUBBLES_BLUE_JA NULL
#define OPTION_VAL_BUTTERCUP_GREEN_JA NULL
#define OPTION_VAL_DIGIVICE_JA NULL
#define OPTION_VAL_GAME_COM_JA NULL
#define OPTION_VAL_GAMEKING_JA NULL
#define OPTION_VAL_GAME_MASTER_JA NULL
#define OPTION_VAL_GOLDEN_WILD_JA NULL
#define OPTION_VAL_GREENSCALE_JA NULL
#define OPTION_VAL_HOKAGE_ORANGE_JA NULL
#define OPTION_VAL_LABO_FAWN_JA NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_JA NULL
#define OPTION_VAL_MICROVISION_JA NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_JA NULL
#define OPTION_VAL_ODYSSEY_GOLD_JA NULL
#define OPTION_VAL_SHINY_SKY_BLUE_JA NULL
#define OPTION_VAL_SLIME_BLUE_JA NULL
#define OPTION_VAL_TI_83_JA NULL
#define OPTION_VAL_TRAVEL_WOOD_JA NULL
#define OPTION_VAL_VIRTUAL_BOY_JA NULL
#define WSWAN_GFX_COLORS_LABEL_JA "色深度 (再起動が必要)"
#define WSWAN_GFX_COLORS_INFO_0_JA NULL
#define OPTION_VAL_16BIT_JA "数千 (16ビット)"
#define OPTION_VAL_24BIT_JA "数百万 (24ビット)"
#define WSWAN_FRAMESKIP_LABEL_JA "フレームスキップ"
#define WSWAN_FRAMESKIP_INFO_0_JA "フレームをスキップしてオーディオバッファアンダーラン (音割れ) を回避します。視覚的な滑らかさを犠牲にしてパフォーマンスを向上させます。 「自動」は、フロントエンドにアドバイスされたときにフレームをスキップします。「マニュアル」は、「フレームスキップ閾値 (%)」設定を使用します。"
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_JA "フレームスキップ閾値 (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_JA NULL
#define WSWAN_60HZ_MODE_LABEL_JA NULL
#define WSWAN_60HZ_MODE_INFO_0_JA NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_JA NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_JA NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_JA "オーディオフィルタ"
#define WSWAN_SOUND_LOW_PASS_INFO_0_JA NULL

struct retro_core_option_v2_category option_cats_ja[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ja[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_JA,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_JA,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_JA },
         { "landscape", OPTION_VAL_LANDSCAPE_JA },
         { "portrait",  OPTION_VAL_PORTRAIT_JA },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_JA,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_JA,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_JA },
         { "disabled", OPTION_VAL_LANDSCAPE_JA },
         { "enabled",  OPTION_VAL_PORTRAIT_JA },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_JA,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_JA,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_JA },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_JA },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_JA },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_JA },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_JA },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_JA },
         { "gb_light",               OPTION_VAL_GB_LIGHT_JA },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_JA },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_JA },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_JA },
         { "digivice",               OPTION_VAL_DIGIVICE_JA },
         { "game_com",               OPTION_VAL_GAME_COM_JA },
         { "gameking",               OPTION_VAL_GAMEKING_JA },
         { "game_master",            OPTION_VAL_GAME_MASTER_JA },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_JA },
         { "greenscale",             OPTION_VAL_GREENSCALE_JA },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_JA },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_JA },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_JA },
         { "microvision",            OPTION_VAL_MICROVISION_JA },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_JA },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_JA },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_JA },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_JA },
         { "ti_83",                  OPTION_VAL_TI_83_JA },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_JA },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_JA },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_JA,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_JA,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_JA },
         { "24bit", OPTION_VAL_24BIT_JA },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_JA,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_JA,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_JA },
         { "manual",   OPTION_VAL_MANUAL_JA },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_JA,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_JA,
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
      WSWAN_60HZ_MODE_LABEL_JA,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_JA,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_JA,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_JA,
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
      WSWAN_SOUND_LOW_PASS_LABEL_JA,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_JA,
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
struct retro_core_options_v2 options_ja = {
   option_cats_ja,
   option_defs_ja
};

/* RETRO_LANGUAGE_KO */

#define WSWAN_ROTATE_DISPLAY_LABEL_KO "화면 회전"
#define WSWAN_ROTATE_DISPLAY_INFO_0_KO "콘솔 화면의 방향을 선택합니다. '세로'를 선택하면 일반적인(가로) 화면에서 세로 방향을 사용하는 게임을 올바르게 표시할 수 있습니다. '수동'을 선택하면 컨트롤러로 방향을 전환할 수 있습니다(기본 버튼: 선택)."
#define OPTION_VAL_MANUAL_KO "수동"
#define OPTION_VAL_LANDSCAPE_KO "가로"
#define OPTION_VAL_PORTRAIT_KO "세로"
#define WSWAN_ROTATE_KEYMAP_LABEL_KO "버튼 매핑 회전"
#define WSWAN_ROTATE_KEYMAP_INFO_0_KO "선호하는 버튼 매핑 방식을 선택합니다. '자동' 선택 시 버튼 매핑을 화면 회전 방향에 따라 조정합니다."
#define OPTION_VAL_AUTO_KO "자동"
#define WSWAN_MONO_PALETTE_LABEL_KO "색상 팔레트"
#define WSWAN_MONO_PALETTE_INFO_0_KO "WonderSwan(흑백) 게임들의 색상화 방식을 선택합니다. 'WS' 팔레트들은 원본 WonderSwan 하드웨어의 색상을 흉내냅니다."
#define OPTION_VAL_DEFAULT_KO "흑백"
#define OPTION_VAL_WONDERSWAN_KO NULL
#define OPTION_VAL_WONDESWAN_COLOR_KO NULL
#define OPTION_VAL_SWANCRYSTAL_KO NULL
#define OPTION_VAL_GB_DMG_KO NULL
#define OPTION_VAL_GB_POCKET_KO NULL
#define OPTION_VAL_GB_LIGHT_KO NULL
#define OPTION_VAL_BLOSSOM_PINK_KO "블로섬 핑크"
#define OPTION_VAL_BUBBLES_BLUE_KO "버블 블루"
#define OPTION_VAL_BUTTERCUP_GREEN_KO "버터컵 그린"
#define OPTION_VAL_DIGIVICE_KO "디지바이스"
#define OPTION_VAL_GAME_COM_KO NULL
#define OPTION_VAL_GAMEKING_KO NULL
#define OPTION_VAL_GAME_MASTER_KO NULL
#define OPTION_VAL_GOLDEN_WILD_KO "골든 와일드"
#define OPTION_VAL_GREENSCALE_KO "초록 단색"
#define OPTION_VAL_HOKAGE_ORANGE_KO "호카게 오렌지"
#define OPTION_VAL_LABO_FAWN_KO "라보 황갈색"
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_KO "전설의 슈퍼 사이어인"
#define OPTION_VAL_MICROVISION_KO NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_KO "밀리언 라이브 골드"
#define OPTION_VAL_ODYSSEY_GOLD_KO "오디세이 골드"
#define OPTION_VAL_SHINY_SKY_BLUE_KO "샤이니 스카이 블루"
#define OPTION_VAL_SLIME_BLUE_KO "슬라임 블루"
#define OPTION_VAL_TI_83_KO NULL
#define OPTION_VAL_TRAVEL_WOOD_KO "낡은 여행 사진"
#define OPTION_VAL_VIRTUAL_BOY_KO "버추얼 보이"
#define WSWAN_GFX_COLORS_LABEL_KO "색 깊이 (재시작 필요)"
#define WSWAN_GFX_COLORS_INFO_0_KO "24비트는 느리며 일부 플랫폼에서는 지원되지 않습니다."
#define OPTION_VAL_16BIT_KO "수천가지 (16비트)"
#define OPTION_VAL_24BIT_KO "수백만가지 (24비트)"
#define WSWAN_FRAMESKIP_LABEL_KO "프레임 스킵"
#define WSWAN_FRAMESKIP_INFO_0_KO "오디오 버퍼 언더런(소리깨짐) 을 줄이기 위해 프레임 건너뛰기를 합니다. 시각적인 부드러움을 포기하는 대신 성능이 향상됩니다. '자동'은 프론트엔드의 추천값으로 실행되고 '사용자 설정'은 '프레임 스킵 임계값(%)' 설정을 이용해 실행됩니다."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_KO "프레임 스킵 임계값 (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_KO "'프레임 스킵'이 '수동'일 경우 프레임 스킵을 수행할 오디오 버퍼 점유 임계점(퍼센트)을 설정합니다. 값이 높을수록 프레임을 자주 스킵하여 소리 깨짐 현상을 줄일 수 있습니다."
#define WSWAN_60HZ_MODE_LABEL_KO "60Hz 모드"
#define WSWAN_60HZ_MODE_INFO_0_KO "매 5번째 프레임을 건너뛰어 화면을 WonderSwan의 기본 주사율인 75Hz가 아닌 60Hz로 표시합니다. 영상이 덜 부드럽게 표시되지만, 60Hz 이상의 주사율을 지원하지 않는 디스플레이에서의 화면 티어링 증상을 해결합니다."
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_KO "소리 출력 샘플 레이트"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_KO "높은 값은 음질을 약간 향상시키지만 높은 성능을 필요로 합니다."
#define WSWAN_SOUND_LOW_PASS_LABEL_KO "오디오 필터"
#define WSWAN_SOUND_LOW_PASS_INFO_0_KO "오디오에 로우패스 필터를 적용하여 WonderSwan이 간혹 만들어내는 거친 소리를 부드럽게 변환합니다."

struct retro_core_option_v2_category option_cats_ko[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ko[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_KO,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_KO,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_KO },
         { "landscape", OPTION_VAL_LANDSCAPE_KO },
         { "portrait",  OPTION_VAL_PORTRAIT_KO },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_KO,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_KO,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_KO },
         { "disabled", OPTION_VAL_LANDSCAPE_KO },
         { "enabled",  OPTION_VAL_PORTRAIT_KO },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_KO,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_KO,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_KO },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_KO },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_KO },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_KO },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_KO },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_KO },
         { "gb_light",               OPTION_VAL_GB_LIGHT_KO },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_KO },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_KO },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_KO },
         { "digivice",               OPTION_VAL_DIGIVICE_KO },
         { "game_com",               OPTION_VAL_GAME_COM_KO },
         { "gameking",               OPTION_VAL_GAMEKING_KO },
         { "game_master",            OPTION_VAL_GAME_MASTER_KO },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_KO },
         { "greenscale",             OPTION_VAL_GREENSCALE_KO },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_KO },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_KO },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_KO },
         { "microvision",            OPTION_VAL_MICROVISION_KO },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_KO },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_KO },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_KO },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_KO },
         { "ti_83",                  OPTION_VAL_TI_83_KO },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_KO },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_KO },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_KO,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_KO,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_KO },
         { "24bit", OPTION_VAL_24BIT_KO },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_KO,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_KO,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_KO },
         { "manual",   OPTION_VAL_MANUAL_KO },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_KO,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_KO,
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
      WSWAN_60HZ_MODE_LABEL_KO,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_KO,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_KO,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_KO,
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
      WSWAN_SOUND_LOW_PASS_LABEL_KO,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_KO,
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
struct retro_core_options_v2 options_ko = {
   option_cats_ko,
   option_defs_ko
};

/* RETRO_LANGUAGE_NL */

#define WSWAN_ROTATE_DISPLAY_LABEL_NL NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_NL NULL
#define OPTION_VAL_MANUAL_NL NULL
#define OPTION_VAL_LANDSCAPE_NL NULL
#define OPTION_VAL_PORTRAIT_NL NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_NL NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_NL NULL
#define OPTION_VAL_AUTO_NL "Automatisch"
#define WSWAN_MONO_PALETTE_LABEL_NL NULL
#define WSWAN_MONO_PALETTE_INFO_0_NL NULL
#define OPTION_VAL_DEFAULT_NL NULL
#define OPTION_VAL_WONDERSWAN_NL NULL
#define OPTION_VAL_WONDESWAN_COLOR_NL NULL
#define OPTION_VAL_SWANCRYSTAL_NL NULL
#define OPTION_VAL_GB_DMG_NL NULL
#define OPTION_VAL_GB_POCKET_NL NULL
#define OPTION_VAL_GB_LIGHT_NL NULL
#define OPTION_VAL_BLOSSOM_PINK_NL NULL
#define OPTION_VAL_BUBBLES_BLUE_NL NULL
#define OPTION_VAL_BUTTERCUP_GREEN_NL NULL
#define OPTION_VAL_DIGIVICE_NL NULL
#define OPTION_VAL_GAME_COM_NL NULL
#define OPTION_VAL_GAMEKING_NL NULL
#define OPTION_VAL_GAME_MASTER_NL NULL
#define OPTION_VAL_GOLDEN_WILD_NL NULL
#define OPTION_VAL_GREENSCALE_NL NULL
#define OPTION_VAL_HOKAGE_ORANGE_NL NULL
#define OPTION_VAL_LABO_FAWN_NL NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_NL NULL
#define OPTION_VAL_MICROVISION_NL NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_NL NULL
#define OPTION_VAL_ODYSSEY_GOLD_NL NULL
#define OPTION_VAL_SHINY_SKY_BLUE_NL NULL
#define OPTION_VAL_SLIME_BLUE_NL NULL
#define OPTION_VAL_TI_83_NL NULL
#define OPTION_VAL_TRAVEL_WOOD_NL NULL
#define OPTION_VAL_VIRTUAL_BOY_NL NULL
#define WSWAN_GFX_COLORS_LABEL_NL NULL
#define WSWAN_GFX_COLORS_INFO_0_NL NULL
#define OPTION_VAL_16BIT_NL NULL
#define OPTION_VAL_24BIT_NL NULL
#define WSWAN_FRAMESKIP_LABEL_NL NULL
#define WSWAN_FRAMESKIP_INFO_0_NL NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_NL NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_NL NULL
#define WSWAN_60HZ_MODE_LABEL_NL NULL
#define WSWAN_60HZ_MODE_INFO_0_NL NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_NL NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_NL NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_NL NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_NL NULL

struct retro_core_option_v2_category option_cats_nl[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_nl[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_NL,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_NL,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_NL },
         { "landscape", OPTION_VAL_LANDSCAPE_NL },
         { "portrait",  OPTION_VAL_PORTRAIT_NL },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_NL,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_NL,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_NL },
         { "disabled", OPTION_VAL_LANDSCAPE_NL },
         { "enabled",  OPTION_VAL_PORTRAIT_NL },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_NL,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_NL,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_NL },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_NL },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_NL },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_NL },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_NL },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_NL },
         { "gb_light",               OPTION_VAL_GB_LIGHT_NL },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_NL },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_NL },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_NL },
         { "digivice",               OPTION_VAL_DIGIVICE_NL },
         { "game_com",               OPTION_VAL_GAME_COM_NL },
         { "gameking",               OPTION_VAL_GAMEKING_NL },
         { "game_master",            OPTION_VAL_GAME_MASTER_NL },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_NL },
         { "greenscale",             OPTION_VAL_GREENSCALE_NL },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_NL },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_NL },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_NL },
         { "microvision",            OPTION_VAL_MICROVISION_NL },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_NL },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_NL },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_NL },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_NL },
         { "ti_83",                  OPTION_VAL_TI_83_NL },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_NL },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_NL },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_NL,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_NL,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_NL },
         { "24bit", OPTION_VAL_24BIT_NL },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_NL,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_NL,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_NL },
         { "manual",   OPTION_VAL_MANUAL_NL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_NL,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_NL,
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
      WSWAN_60HZ_MODE_LABEL_NL,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_NL,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_NL,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_NL,
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
      WSWAN_SOUND_LOW_PASS_LABEL_NL,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_NL,
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
struct retro_core_options_v2 options_nl = {
   option_cats_nl,
   option_defs_nl
};

/* RETRO_LANGUAGE_NO */

#define WSWAN_ROTATE_DISPLAY_LABEL_NO NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_NO NULL
#define OPTION_VAL_MANUAL_NO NULL
#define OPTION_VAL_LANDSCAPE_NO NULL
#define OPTION_VAL_PORTRAIT_NO NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_NO NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_NO NULL
#define OPTION_VAL_AUTO_NO NULL
#define WSWAN_MONO_PALETTE_LABEL_NO NULL
#define WSWAN_MONO_PALETTE_INFO_0_NO NULL
#define OPTION_VAL_DEFAULT_NO NULL
#define OPTION_VAL_WONDERSWAN_NO NULL
#define OPTION_VAL_WONDESWAN_COLOR_NO NULL
#define OPTION_VAL_SWANCRYSTAL_NO NULL
#define OPTION_VAL_GB_DMG_NO NULL
#define OPTION_VAL_GB_POCKET_NO NULL
#define OPTION_VAL_GB_LIGHT_NO NULL
#define OPTION_VAL_BLOSSOM_PINK_NO NULL
#define OPTION_VAL_BUBBLES_BLUE_NO NULL
#define OPTION_VAL_BUTTERCUP_GREEN_NO NULL
#define OPTION_VAL_DIGIVICE_NO NULL
#define OPTION_VAL_GAME_COM_NO NULL
#define OPTION_VAL_GAMEKING_NO NULL
#define OPTION_VAL_GAME_MASTER_NO NULL
#define OPTION_VAL_GOLDEN_WILD_NO NULL
#define OPTION_VAL_GREENSCALE_NO NULL
#define OPTION_VAL_HOKAGE_ORANGE_NO NULL
#define OPTION_VAL_LABO_FAWN_NO NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_NO NULL
#define OPTION_VAL_MICROVISION_NO NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_NO NULL
#define OPTION_VAL_ODYSSEY_GOLD_NO NULL
#define OPTION_VAL_SHINY_SKY_BLUE_NO NULL
#define OPTION_VAL_SLIME_BLUE_NO NULL
#define OPTION_VAL_TI_83_NO NULL
#define OPTION_VAL_TRAVEL_WOOD_NO NULL
#define OPTION_VAL_VIRTUAL_BOY_NO NULL
#define WSWAN_GFX_COLORS_LABEL_NO NULL
#define WSWAN_GFX_COLORS_INFO_0_NO NULL
#define OPTION_VAL_16BIT_NO NULL
#define OPTION_VAL_24BIT_NO NULL
#define WSWAN_FRAMESKIP_LABEL_NO NULL
#define WSWAN_FRAMESKIP_INFO_0_NO NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_NO NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_NO NULL
#define WSWAN_60HZ_MODE_LABEL_NO NULL
#define WSWAN_60HZ_MODE_INFO_0_NO NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_NO NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_NO NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_NO NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_NO NULL

struct retro_core_option_v2_category option_cats_no[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_no[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_NO,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_NO,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_NO },
         { "landscape", OPTION_VAL_LANDSCAPE_NO },
         { "portrait",  OPTION_VAL_PORTRAIT_NO },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_NO,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_NO,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_NO },
         { "disabled", OPTION_VAL_LANDSCAPE_NO },
         { "enabled",  OPTION_VAL_PORTRAIT_NO },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_NO,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_NO,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_NO },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_NO },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_NO },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_NO },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_NO },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_NO },
         { "gb_light",               OPTION_VAL_GB_LIGHT_NO },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_NO },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_NO },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_NO },
         { "digivice",               OPTION_VAL_DIGIVICE_NO },
         { "game_com",               OPTION_VAL_GAME_COM_NO },
         { "gameking",               OPTION_VAL_GAMEKING_NO },
         { "game_master",            OPTION_VAL_GAME_MASTER_NO },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_NO },
         { "greenscale",             OPTION_VAL_GREENSCALE_NO },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_NO },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_NO },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_NO },
         { "microvision",            OPTION_VAL_MICROVISION_NO },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_NO },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_NO },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_NO },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_NO },
         { "ti_83",                  OPTION_VAL_TI_83_NO },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_NO },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_NO },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_NO,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_NO,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_NO },
         { "24bit", OPTION_VAL_24BIT_NO },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_NO,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_NO,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_NO },
         { "manual",   OPTION_VAL_MANUAL_NO },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_NO,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_NO,
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
      WSWAN_60HZ_MODE_LABEL_NO,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_NO,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_NO,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_NO,
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
      WSWAN_SOUND_LOW_PASS_LABEL_NO,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_NO,
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
struct retro_core_options_v2 options_no = {
   option_cats_no,
   option_defs_no
};

/* RETRO_LANGUAGE_OR */

#define WSWAN_ROTATE_DISPLAY_LABEL_OR NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_OR NULL
#define OPTION_VAL_MANUAL_OR NULL
#define OPTION_VAL_LANDSCAPE_OR NULL
#define OPTION_VAL_PORTRAIT_OR NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_OR NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_OR NULL
#define OPTION_VAL_AUTO_OR NULL
#define WSWAN_MONO_PALETTE_LABEL_OR NULL
#define WSWAN_MONO_PALETTE_INFO_0_OR NULL
#define OPTION_VAL_DEFAULT_OR NULL
#define OPTION_VAL_WONDERSWAN_OR NULL
#define OPTION_VAL_WONDESWAN_COLOR_OR NULL
#define OPTION_VAL_SWANCRYSTAL_OR NULL
#define OPTION_VAL_GB_DMG_OR NULL
#define OPTION_VAL_GB_POCKET_OR NULL
#define OPTION_VAL_GB_LIGHT_OR NULL
#define OPTION_VAL_BLOSSOM_PINK_OR NULL
#define OPTION_VAL_BUBBLES_BLUE_OR NULL
#define OPTION_VAL_BUTTERCUP_GREEN_OR NULL
#define OPTION_VAL_DIGIVICE_OR NULL
#define OPTION_VAL_GAME_COM_OR NULL
#define OPTION_VAL_GAMEKING_OR NULL
#define OPTION_VAL_GAME_MASTER_OR NULL
#define OPTION_VAL_GOLDEN_WILD_OR NULL
#define OPTION_VAL_GREENSCALE_OR NULL
#define OPTION_VAL_HOKAGE_ORANGE_OR NULL
#define OPTION_VAL_LABO_FAWN_OR NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_OR NULL
#define OPTION_VAL_MICROVISION_OR NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_OR NULL
#define OPTION_VAL_ODYSSEY_GOLD_OR NULL
#define OPTION_VAL_SHINY_SKY_BLUE_OR NULL
#define OPTION_VAL_SLIME_BLUE_OR NULL
#define OPTION_VAL_TI_83_OR NULL
#define OPTION_VAL_TRAVEL_WOOD_OR NULL
#define OPTION_VAL_VIRTUAL_BOY_OR NULL
#define WSWAN_GFX_COLORS_LABEL_OR NULL
#define WSWAN_GFX_COLORS_INFO_0_OR NULL
#define OPTION_VAL_16BIT_OR NULL
#define OPTION_VAL_24BIT_OR NULL
#define WSWAN_FRAMESKIP_LABEL_OR NULL
#define WSWAN_FRAMESKIP_INFO_0_OR NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_OR NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_OR NULL
#define WSWAN_60HZ_MODE_LABEL_OR NULL
#define WSWAN_60HZ_MODE_INFO_0_OR NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_OR NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_OR NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_OR NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_OR NULL

struct retro_core_option_v2_category option_cats_or[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_or[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_OR,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_OR,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_OR },
         { "landscape", OPTION_VAL_LANDSCAPE_OR },
         { "portrait",  OPTION_VAL_PORTRAIT_OR },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_OR,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_OR,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_OR },
         { "disabled", OPTION_VAL_LANDSCAPE_OR },
         { "enabled",  OPTION_VAL_PORTRAIT_OR },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_OR,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_OR,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_OR },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_OR },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_OR },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_OR },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_OR },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_OR },
         { "gb_light",               OPTION_VAL_GB_LIGHT_OR },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_OR },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_OR },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_OR },
         { "digivice",               OPTION_VAL_DIGIVICE_OR },
         { "game_com",               OPTION_VAL_GAME_COM_OR },
         { "gameking",               OPTION_VAL_GAMEKING_OR },
         { "game_master",            OPTION_VAL_GAME_MASTER_OR },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_OR },
         { "greenscale",             OPTION_VAL_GREENSCALE_OR },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_OR },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_OR },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_OR },
         { "microvision",            OPTION_VAL_MICROVISION_OR },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_OR },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_OR },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_OR },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_OR },
         { "ti_83",                  OPTION_VAL_TI_83_OR },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_OR },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_OR },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_OR,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_OR,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_OR },
         { "24bit", OPTION_VAL_24BIT_OR },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_OR,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_OR,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_OR },
         { "manual",   OPTION_VAL_MANUAL_OR },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_OR,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_OR,
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
      WSWAN_60HZ_MODE_LABEL_OR,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_OR,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_OR,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_OR,
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
      WSWAN_SOUND_LOW_PASS_LABEL_OR,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_OR,
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
struct retro_core_options_v2 options_or = {
   option_cats_or,
   option_defs_or
};

/* RETRO_LANGUAGE_PL */

#define WSWAN_ROTATE_DISPLAY_LABEL_PL "Obrót ekranu"
#define WSWAN_ROTATE_DISPLAY_INFO_0_PL NULL
#define OPTION_VAL_MANUAL_PL "Ręcznie"
#define OPTION_VAL_LANDSCAPE_PL NULL
#define OPTION_VAL_PORTRAIT_PL NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_PL NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_PL NULL
#define OPTION_VAL_AUTO_PL NULL
#define WSWAN_MONO_PALETTE_LABEL_PL "Paleta kolorów"
#define WSWAN_MONO_PALETTE_INFO_0_PL NULL
#define OPTION_VAL_DEFAULT_PL "Odcienie szarości"
#define OPTION_VAL_WONDERSWAN_PL NULL
#define OPTION_VAL_WONDESWAN_COLOR_PL NULL
#define OPTION_VAL_SWANCRYSTAL_PL NULL
#define OPTION_VAL_GB_DMG_PL NULL
#define OPTION_VAL_GB_POCKET_PL NULL
#define OPTION_VAL_GB_LIGHT_PL NULL
#define OPTION_VAL_BLOSSOM_PINK_PL NULL
#define OPTION_VAL_BUBBLES_BLUE_PL NULL
#define OPTION_VAL_BUTTERCUP_GREEN_PL NULL
#define OPTION_VAL_DIGIVICE_PL NULL
#define OPTION_VAL_GAME_COM_PL NULL
#define OPTION_VAL_GAMEKING_PL NULL
#define OPTION_VAL_GAME_MASTER_PL NULL
#define OPTION_VAL_GOLDEN_WILD_PL NULL
#define OPTION_VAL_GREENSCALE_PL NULL
#define OPTION_VAL_HOKAGE_ORANGE_PL NULL
#define OPTION_VAL_LABO_FAWN_PL NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_PL NULL
#define OPTION_VAL_MICROVISION_PL NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_PL NULL
#define OPTION_VAL_ODYSSEY_GOLD_PL NULL
#define OPTION_VAL_SHINY_SKY_BLUE_PL NULL
#define OPTION_VAL_SLIME_BLUE_PL NULL
#define OPTION_VAL_TI_83_PL NULL
#define OPTION_VAL_TRAVEL_WOOD_PL NULL
#define OPTION_VAL_VIRTUAL_BOY_PL NULL
#define WSWAN_GFX_COLORS_LABEL_PL NULL
#define WSWAN_GFX_COLORS_INFO_0_PL NULL
#define OPTION_VAL_16BIT_PL NULL
#define OPTION_VAL_24BIT_PL NULL
#define WSWAN_FRAMESKIP_LABEL_PL "Pomijanie klatek"
#define WSWAN_FRAMESKIP_INFO_0_PL NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_PL "Próg pominięcia ramki (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_PL NULL
#define WSWAN_60HZ_MODE_LABEL_PL "Tryb 60Hz"
#define WSWAN_60HZ_MODE_INFO_0_PL "Uaktualnij wyświetlacz przy 60Hz zamiast natywnej częstotliwości odświeżania 75Hz WonderSwan poprzez upuszczanie co piątą klatkę. Zmniejsza płynność wideo, ale unika rozrywania ekranu na wyświetlaczach, które nie obsługują pracy natywnej powyżej 60 Hz."
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_PL NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_PL NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_PL "Filtr audio"
#define WSWAN_SOUND_LOW_PASS_INFO_0_PL "Zastosuj filtr dźwiękowy o niskim współczynniku przenikania do \"łagodnych\" czasami surowych dźwięków wytwarzanych przez WonderSwan."

struct retro_core_option_v2_category option_cats_pl[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_pl[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_PL,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_PL,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_PL },
         { "landscape", OPTION_VAL_LANDSCAPE_PL },
         { "portrait",  OPTION_VAL_PORTRAIT_PL },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_PL,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_PL,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_PL },
         { "disabled", OPTION_VAL_LANDSCAPE_PL },
         { "enabled",  OPTION_VAL_PORTRAIT_PL },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_PL,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_PL,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_PL },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_PL },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_PL },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_PL },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_PL },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_PL },
         { "gb_light",               OPTION_VAL_GB_LIGHT_PL },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_PL },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_PL },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_PL },
         { "digivice",               OPTION_VAL_DIGIVICE_PL },
         { "game_com",               OPTION_VAL_GAME_COM_PL },
         { "gameking",               OPTION_VAL_GAMEKING_PL },
         { "game_master",            OPTION_VAL_GAME_MASTER_PL },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_PL },
         { "greenscale",             OPTION_VAL_GREENSCALE_PL },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_PL },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_PL },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_PL },
         { "microvision",            OPTION_VAL_MICROVISION_PL },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_PL },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_PL },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_PL },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_PL },
         { "ti_83",                  OPTION_VAL_TI_83_PL },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_PL },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_PL },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_PL,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_PL,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_PL },
         { "24bit", OPTION_VAL_24BIT_PL },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_PL,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_PL,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_PL },
         { "manual",   OPTION_VAL_MANUAL_PL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_PL,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_PL,
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
      WSWAN_60HZ_MODE_LABEL_PL,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_PL,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_PL,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_PL,
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
      WSWAN_SOUND_LOW_PASS_LABEL_PL,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_PL,
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
struct retro_core_options_v2 options_pl = {
   option_cats_pl,
   option_defs_pl
};

/* RETRO_LANGUAGE_PT_BR */

#define WSWAN_ROTATE_DISPLAY_LABEL_PT_BR "Rotação de tela"
#define WSWAN_ROTATE_DISPLAY_INFO_0_PT_BR "Seleciona a orientação da tela do console. \"Modo retrato\" pode ser usado para assegurar o layout correto de jogos voltados a modo retrato em uma exibição tradicional (modo paisagem). \"Manual\" ativa a rotação via controle (botão padrão: SELECT)."
#define OPTION_VAL_MANUAL_PT_BR NULL
#define OPTION_VAL_LANDSCAPE_PT_BR "Modo paisagem"
#define OPTION_VAL_PORTRAIT_PT_BR "Modo retrato"
#define WSWAN_ROTATE_KEYMAP_LABEL_PT_BR "Rotação dos mapeamentos de botão"
#define WSWAN_ROTATE_KEYMAP_INFO_0_PT_BR "Seleciona o mapeamento de botão desejado. \"Automática\" ajusta o mapeamento de botão para combinar com a rotação da tela atual."
#define OPTION_VAL_AUTO_PT_BR "Automática"
#define WSWAN_MONO_PALETTE_LABEL_PT_BR "Paleta de cores"
#define WSWAN_MONO_PALETTE_INFO_0_PT_BR "Seleciona a coloração dos jogos de WonderSwan (Mono). As paletas \"WS\" imitam a exibição do hardware original do WonderSwan."
#define OPTION_VAL_DEFAULT_PT_BR "Nível de cinza"
#define OPTION_VAL_WONDERSWAN_PT_BR "WS — WonderSwan"
#define OPTION_VAL_WONDESWAN_COLOR_PT_BR "WS — WonderSwan Color"
#define OPTION_VAL_SWANCRYSTAL_PT_BR "WS — SwanCrystal"
#define OPTION_VAL_GB_DMG_PT_BR NULL
#define OPTION_VAL_GB_POCKET_PT_BR NULL
#define OPTION_VAL_GB_LIGHT_PT_BR NULL
#define OPTION_VAL_BLOSSOM_PINK_PT_BR "Rosa — Florzinha"
#define OPTION_VAL_BUBBLES_BLUE_PT_BR "Azul — Lindinha"
#define OPTION_VAL_BUTTERCUP_GREEN_PT_BR "Verde — Docinho"
#define OPTION_VAL_DIGIVICE_PT_BR NULL
#define OPTION_VAL_GAME_COM_PT_BR NULL
#define OPTION_VAL_GAMEKING_PT_BR NULL
#define OPTION_VAL_GAME_MASTER_PT_BR NULL
#define OPTION_VAL_GOLDEN_WILD_PT_BR "Natureza Dourada"
#define OPTION_VAL_GREENSCALE_PT_BR "Verdejante"
#define OPTION_VAL_HOKAGE_ORANGE_PT_BR "Laranja — Hokage"
#define OPTION_VAL_LABO_FAWN_PT_BR "Castanho — Labo"
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_PT_BR "Lendário Super Saiyajin"
#define OPTION_VAL_MICROVISION_PT_BR NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_PT_BR "Dourado — Million Live"
#define OPTION_VAL_ODYSSEY_GOLD_PT_BR "Dourado — Odyssey"
#define OPTION_VAL_SHINY_SKY_BLUE_PT_BR "Azul — Shiny Sky"
#define OPTION_VAL_SLIME_BLUE_PT_BR "Azul — Slime"
#define OPTION_VAL_TI_83_PT_BR NULL
#define OPTION_VAL_TRAVEL_WOOD_PT_BR "Madeira — Fotos de Viagem"
#define OPTION_VAL_VIRTUAL_BOY_PT_BR NULL
#define WSWAN_GFX_COLORS_LABEL_PT_BR "Profundidade de cor (requer reinício)"
#define WSWAN_GFX_COLORS_INFO_0_PT_BR "24 bits é mais lento e não está disponível em todas as plataformas."
#define OPTION_VAL_16BIT_PT_BR "Milhares (16 bits)"
#define OPTION_VAL_24BIT_PT_BR "Milhões (24 bits)"
#define WSWAN_FRAMESKIP_LABEL_PT_BR "Pulo de quadro"
#define WSWAN_FRAMESKIP_INFO_0_PT_BR "Ignora quadros para evitar o esvaziamento do buffer do áudio (pipocos no áudio). Melhora o desempenho ao custo da suavidade visual. A opção 'Auto' ignora os quadros quando for aconselhado pela interface. Já 'Manual' utiliza a configuração 'Limite do salto de quadros (%)'."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_PT_BR "Limite de pulo de quadro (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_PT_BR "Ao configurar \"Pulo de quadro\" como \"Manual\", especifica o limite do buffer de áudio em porcentagem, os quadros fora da porcentagem serão ignorados. Valores maiores reduzem o risco de engasgo por descartar quadros com mais frequência."
#define WSWAN_60HZ_MODE_LABEL_PT_BR NULL
#define WSWAN_60HZ_MODE_INFO_0_PT_BR NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_PT_BR "Taxa de amostragem da saída de som"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_PT_BR "Valores maiores melhoram ligeiramente a qualidade do som em detrimento do desempenho."
#define WSWAN_SOUND_LOW_PASS_LABEL_PT_BR "Filtro de áudio"
#define WSWAN_SOUND_LOW_PASS_INFO_0_PT_BR NULL

struct retro_core_option_v2_category option_cats_pt_br[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_pt_br[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_PT_BR,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_PT_BR,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_PT_BR },
         { "landscape", OPTION_VAL_LANDSCAPE_PT_BR },
         { "portrait",  OPTION_VAL_PORTRAIT_PT_BR },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_PT_BR,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_PT_BR,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_PT_BR },
         { "disabled", OPTION_VAL_LANDSCAPE_PT_BR },
         { "enabled",  OPTION_VAL_PORTRAIT_PT_BR },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_PT_BR,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_PT_BR,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_PT_BR },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_PT_BR },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_PT_BR },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_PT_BR },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_PT_BR },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_PT_BR },
         { "gb_light",               OPTION_VAL_GB_LIGHT_PT_BR },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_PT_BR },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_PT_BR },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_PT_BR },
         { "digivice",               OPTION_VAL_DIGIVICE_PT_BR },
         { "game_com",               OPTION_VAL_GAME_COM_PT_BR },
         { "gameking",               OPTION_VAL_GAMEKING_PT_BR },
         { "game_master",            OPTION_VAL_GAME_MASTER_PT_BR },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_PT_BR },
         { "greenscale",             OPTION_VAL_GREENSCALE_PT_BR },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_PT_BR },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_PT_BR },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_PT_BR },
         { "microvision",            OPTION_VAL_MICROVISION_PT_BR },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_PT_BR },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_PT_BR },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_PT_BR },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_PT_BR },
         { "ti_83",                  OPTION_VAL_TI_83_PT_BR },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_PT_BR },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_PT_BR },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_PT_BR,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_PT_BR,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_PT_BR },
         { "24bit", OPTION_VAL_24BIT_PT_BR },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_PT_BR,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_PT_BR,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_PT_BR },
         { "manual",   OPTION_VAL_MANUAL_PT_BR },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_PT_BR,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_PT_BR,
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
      WSWAN_60HZ_MODE_LABEL_PT_BR,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_PT_BR,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_PT_BR,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_PT_BR,
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
      WSWAN_SOUND_LOW_PASS_LABEL_PT_BR,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_PT_BR,
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
struct retro_core_options_v2 options_pt_br = {
   option_cats_pt_br,
   option_defs_pt_br
};

/* RETRO_LANGUAGE_PT_PT */

#define WSWAN_ROTATE_DISPLAY_LABEL_PT_PT NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_PT_PT NULL
#define OPTION_VAL_MANUAL_PT_PT NULL
#define OPTION_VAL_LANDSCAPE_PT_PT NULL
#define OPTION_VAL_PORTRAIT_PT_PT NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_PT_PT NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_PT_PT NULL
#define OPTION_VAL_AUTO_PT_PT "Automático"
#define WSWAN_MONO_PALETTE_LABEL_PT_PT NULL
#define WSWAN_MONO_PALETTE_INFO_0_PT_PT NULL
#define OPTION_VAL_DEFAULT_PT_PT NULL
#define OPTION_VAL_WONDERSWAN_PT_PT NULL
#define OPTION_VAL_WONDESWAN_COLOR_PT_PT NULL
#define OPTION_VAL_SWANCRYSTAL_PT_PT NULL
#define OPTION_VAL_GB_DMG_PT_PT NULL
#define OPTION_VAL_GB_POCKET_PT_PT NULL
#define OPTION_VAL_GB_LIGHT_PT_PT NULL
#define OPTION_VAL_BLOSSOM_PINK_PT_PT NULL
#define OPTION_VAL_BUBBLES_BLUE_PT_PT NULL
#define OPTION_VAL_BUTTERCUP_GREEN_PT_PT NULL
#define OPTION_VAL_DIGIVICE_PT_PT NULL
#define OPTION_VAL_GAME_COM_PT_PT NULL
#define OPTION_VAL_GAMEKING_PT_PT NULL
#define OPTION_VAL_GAME_MASTER_PT_PT NULL
#define OPTION_VAL_GOLDEN_WILD_PT_PT NULL
#define OPTION_VAL_GREENSCALE_PT_PT NULL
#define OPTION_VAL_HOKAGE_ORANGE_PT_PT NULL
#define OPTION_VAL_LABO_FAWN_PT_PT NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_PT_PT NULL
#define OPTION_VAL_MICROVISION_PT_PT NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_PT_PT NULL
#define OPTION_VAL_ODYSSEY_GOLD_PT_PT NULL
#define OPTION_VAL_SHINY_SKY_BLUE_PT_PT NULL
#define OPTION_VAL_SLIME_BLUE_PT_PT NULL
#define OPTION_VAL_TI_83_PT_PT NULL
#define OPTION_VAL_TRAVEL_WOOD_PT_PT NULL
#define OPTION_VAL_VIRTUAL_BOY_PT_PT NULL
#define WSWAN_GFX_COLORS_LABEL_PT_PT NULL
#define WSWAN_GFX_COLORS_INFO_0_PT_PT NULL
#define OPTION_VAL_16BIT_PT_PT NULL
#define OPTION_VAL_24BIT_PT_PT NULL
#define WSWAN_FRAMESKIP_LABEL_PT_PT NULL
#define WSWAN_FRAMESKIP_INFO_0_PT_PT NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_PT_PT NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_PT_PT NULL
#define WSWAN_60HZ_MODE_LABEL_PT_PT NULL
#define WSWAN_60HZ_MODE_INFO_0_PT_PT NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_PT_PT NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_PT_PT NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_PT_PT "Filtro de som"
#define WSWAN_SOUND_LOW_PASS_INFO_0_PT_PT NULL

struct retro_core_option_v2_category option_cats_pt_pt[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_pt_pt[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_PT_PT,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_PT_PT,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_PT_PT },
         { "landscape", OPTION_VAL_LANDSCAPE_PT_PT },
         { "portrait",  OPTION_VAL_PORTRAIT_PT_PT },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_PT_PT,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_PT_PT,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_PT_PT },
         { "disabled", OPTION_VAL_LANDSCAPE_PT_PT },
         { "enabled",  OPTION_VAL_PORTRAIT_PT_PT },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_PT_PT,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_PT_PT,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_PT_PT },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_PT_PT },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_PT_PT },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_PT_PT },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_PT_PT },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_PT_PT },
         { "gb_light",               OPTION_VAL_GB_LIGHT_PT_PT },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_PT_PT },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_PT_PT },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_PT_PT },
         { "digivice",               OPTION_VAL_DIGIVICE_PT_PT },
         { "game_com",               OPTION_VAL_GAME_COM_PT_PT },
         { "gameking",               OPTION_VAL_GAMEKING_PT_PT },
         { "game_master",            OPTION_VAL_GAME_MASTER_PT_PT },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_PT_PT },
         { "greenscale",             OPTION_VAL_GREENSCALE_PT_PT },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_PT_PT },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_PT_PT },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_PT_PT },
         { "microvision",            OPTION_VAL_MICROVISION_PT_PT },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_PT_PT },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_PT_PT },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_PT_PT },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_PT_PT },
         { "ti_83",                  OPTION_VAL_TI_83_PT_PT },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_PT_PT },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_PT_PT },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_PT_PT,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_PT_PT,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_PT_PT },
         { "24bit", OPTION_VAL_24BIT_PT_PT },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_PT_PT,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_PT_PT,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_PT_PT },
         { "manual",   OPTION_VAL_MANUAL_PT_PT },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_PT_PT,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_PT_PT,
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
      WSWAN_60HZ_MODE_LABEL_PT_PT,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_PT_PT,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_PT_PT,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_PT_PT,
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
      WSWAN_SOUND_LOW_PASS_LABEL_PT_PT,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_PT_PT,
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
struct retro_core_options_v2 options_pt_pt = {
   option_cats_pt_pt,
   option_defs_pt_pt
};

/* RETRO_LANGUAGE_RU */

#define WSWAN_ROTATE_DISPLAY_LABEL_RU "Вращение экрана"
#define WSWAN_ROTATE_DISPLAY_INFO_0_RU "Выбор ориентации экрана консоли. 'Портретная' может быть использована для правильного отображения вертикально ориентированных игр в ландшафтном режиме. 'Вручную' активирует вращение с помощью контроллера (кнопка по умолчанию: SELECT)."
#define OPTION_VAL_MANUAL_RU "Вручную"
#define OPTION_VAL_LANDSCAPE_RU "Ландшафтная"
#define OPTION_VAL_PORTRAIT_RU "Портретная"
#define WSWAN_ROTATE_KEYMAP_LABEL_RU NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_RU "Выбор предпочитаемой раскладки кнопок. 'Авто' подстраивает раскладку кнопок согласно текущему повороту экрана."
#define OPTION_VAL_AUTO_RU "Авто"
#define WSWAN_MONO_PALETTE_LABEL_RU "Цветовая палитра"
#define WSWAN_MONO_PALETTE_INFO_0_RU NULL
#define OPTION_VAL_DEFAULT_RU "Оттенки серого"
#define OPTION_VAL_WONDERSWAN_RU NULL
#define OPTION_VAL_WONDESWAN_COLOR_RU NULL
#define OPTION_VAL_SWANCRYSTAL_RU NULL
#define OPTION_VAL_GB_DMG_RU NULL
#define OPTION_VAL_GB_POCKET_RU NULL
#define OPTION_VAL_GB_LIGHT_RU NULL
#define OPTION_VAL_BLOSSOM_PINK_RU NULL
#define OPTION_VAL_BUBBLES_BLUE_RU NULL
#define OPTION_VAL_BUTTERCUP_GREEN_RU NULL
#define OPTION_VAL_DIGIVICE_RU NULL
#define OPTION_VAL_GAME_COM_RU NULL
#define OPTION_VAL_GAMEKING_RU NULL
#define OPTION_VAL_GAME_MASTER_RU NULL
#define OPTION_VAL_GOLDEN_WILD_RU NULL
#define OPTION_VAL_GREENSCALE_RU NULL
#define OPTION_VAL_HOKAGE_ORANGE_RU NULL
#define OPTION_VAL_LABO_FAWN_RU NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_RU NULL
#define OPTION_VAL_MICROVISION_RU NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_RU NULL
#define OPTION_VAL_ODYSSEY_GOLD_RU NULL
#define OPTION_VAL_SHINY_SKY_BLUE_RU NULL
#define OPTION_VAL_SLIME_BLUE_RU NULL
#define OPTION_VAL_TI_83_RU NULL
#define OPTION_VAL_TRAVEL_WOOD_RU NULL
#define OPTION_VAL_VIRTUAL_BOY_RU NULL
#define WSWAN_GFX_COLORS_LABEL_RU "Глубина цвета (перезапуск)"
#define WSWAN_GFX_COLORS_INFO_0_RU NULL
#define OPTION_VAL_16BIT_RU "Тысячи (16 бит)"
#define OPTION_VAL_24BIT_RU "Миллионы (24 бита)"
#define WSWAN_FRAMESKIP_LABEL_RU "Пропуск кадров"
#define WSWAN_FRAMESKIP_INFO_0_RU "Пропускать кадры, чтобы избежать опустошения аудиобуфера (треск). Улучшает производительность, но снижает плавность изображения. В режиме 'Авто' пропуск кадров регулируется фронтендом. В режиме 'Вручную' используется настройка 'Граница пропуска кадров (%)'."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_RU "Граница пропуска кадров (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_RU NULL
#define WSWAN_60HZ_MODE_LABEL_RU NULL
#define WSWAN_60HZ_MODE_INFO_0_RU NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_RU NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_RU NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_RU "Аудиофильтр"
#define WSWAN_SOUND_LOW_PASS_INFO_0_RU NULL

struct retro_core_option_v2_category option_cats_ru[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ru[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_RU,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_RU,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_RU },
         { "landscape", OPTION_VAL_LANDSCAPE_RU },
         { "portrait",  OPTION_VAL_PORTRAIT_RU },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_RU,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_RU,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_RU },
         { "disabled", OPTION_VAL_LANDSCAPE_RU },
         { "enabled",  OPTION_VAL_PORTRAIT_RU },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_RU,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_RU,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_RU },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_RU },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_RU },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_RU },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_RU },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_RU },
         { "gb_light",               OPTION_VAL_GB_LIGHT_RU },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_RU },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_RU },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_RU },
         { "digivice",               OPTION_VAL_DIGIVICE_RU },
         { "game_com",               OPTION_VAL_GAME_COM_RU },
         { "gameking",               OPTION_VAL_GAMEKING_RU },
         { "game_master",            OPTION_VAL_GAME_MASTER_RU },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_RU },
         { "greenscale",             OPTION_VAL_GREENSCALE_RU },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_RU },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_RU },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_RU },
         { "microvision",            OPTION_VAL_MICROVISION_RU },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_RU },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_RU },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_RU },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_RU },
         { "ti_83",                  OPTION_VAL_TI_83_RU },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_RU },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_RU },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_RU,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_RU,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_RU },
         { "24bit", OPTION_VAL_24BIT_RU },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_RU,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_RU,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_RU },
         { "manual",   OPTION_VAL_MANUAL_RU },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_RU,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_RU,
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
      WSWAN_60HZ_MODE_LABEL_RU,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_RU,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_RU,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_RU,
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
      WSWAN_SOUND_LOW_PASS_LABEL_RU,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_RU,
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
struct retro_core_options_v2 options_ru = {
   option_cats_ru,
   option_defs_ru
};

/* RETRO_LANGUAGE_SK */

#define WSWAN_ROTATE_DISPLAY_LABEL_SK "Rotácia displeja"
#define WSWAN_ROTATE_DISPLAY_INFO_0_SK NULL
#define OPTION_VAL_MANUAL_SK "Ručne"
#define OPTION_VAL_LANDSCAPE_SK "Na šírku"
#define OPTION_VAL_PORTRAIT_SK "Portrét"
#define WSWAN_ROTATE_KEYMAP_LABEL_SK NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_SK NULL
#define OPTION_VAL_AUTO_SK NULL
#define WSWAN_MONO_PALETTE_LABEL_SK "Farebná paleta"
#define WSWAN_MONO_PALETTE_INFO_0_SK NULL
#define OPTION_VAL_DEFAULT_SK "Odtiene šedej"
#define OPTION_VAL_WONDERSWAN_SK NULL
#define OPTION_VAL_WONDESWAN_COLOR_SK NULL
#define OPTION_VAL_SWANCRYSTAL_SK NULL
#define OPTION_VAL_GB_DMG_SK NULL
#define OPTION_VAL_GB_POCKET_SK NULL
#define OPTION_VAL_GB_LIGHT_SK NULL
#define OPTION_VAL_BLOSSOM_PINK_SK NULL
#define OPTION_VAL_BUBBLES_BLUE_SK NULL
#define OPTION_VAL_BUTTERCUP_GREEN_SK NULL
#define OPTION_VAL_DIGIVICE_SK NULL
#define OPTION_VAL_GAME_COM_SK NULL
#define OPTION_VAL_GAMEKING_SK NULL
#define OPTION_VAL_GAME_MASTER_SK NULL
#define OPTION_VAL_GOLDEN_WILD_SK NULL
#define OPTION_VAL_GREENSCALE_SK "Odtiene zelenej"
#define OPTION_VAL_HOKAGE_ORANGE_SK NULL
#define OPTION_VAL_LABO_FAWN_SK NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_SK NULL
#define OPTION_VAL_MICROVISION_SK NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_SK NULL
#define OPTION_VAL_ODYSSEY_GOLD_SK NULL
#define OPTION_VAL_SHINY_SKY_BLUE_SK NULL
#define OPTION_VAL_SLIME_BLUE_SK NULL
#define OPTION_VAL_TI_83_SK NULL
#define OPTION_VAL_TRAVEL_WOOD_SK NULL
#define OPTION_VAL_VIRTUAL_BOY_SK NULL
#define WSWAN_GFX_COLORS_LABEL_SK NULL
#define WSWAN_GFX_COLORS_INFO_0_SK NULL
#define OPTION_VAL_16BIT_SK "Tisíce (16-bit)"
#define OPTION_VAL_24BIT_SK "Milióny (24-bit)"
#define WSWAN_FRAMESKIP_LABEL_SK "Preskočenie snímky"
#define WSWAN_FRAMESKIP_INFO_0_SK NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_SK "Prah preskočenia snímky (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_SK NULL
#define WSWAN_60HZ_MODE_LABEL_SK "60 Hz režim"
#define WSWAN_60HZ_MODE_INFO_0_SK NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_SK NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_SK NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_SK "Zvukový filter"
#define WSWAN_SOUND_LOW_PASS_INFO_0_SK NULL

struct retro_core_option_v2_category option_cats_sk[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_sk[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_SK,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_SK,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_SK },
         { "landscape", OPTION_VAL_LANDSCAPE_SK },
         { "portrait",  OPTION_VAL_PORTRAIT_SK },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_SK,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_SK,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_SK },
         { "disabled", OPTION_VAL_LANDSCAPE_SK },
         { "enabled",  OPTION_VAL_PORTRAIT_SK },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_SK,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_SK,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_SK },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_SK },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_SK },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_SK },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_SK },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_SK },
         { "gb_light",               OPTION_VAL_GB_LIGHT_SK },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_SK },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_SK },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_SK },
         { "digivice",               OPTION_VAL_DIGIVICE_SK },
         { "game_com",               OPTION_VAL_GAME_COM_SK },
         { "gameking",               OPTION_VAL_GAMEKING_SK },
         { "game_master",            OPTION_VAL_GAME_MASTER_SK },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_SK },
         { "greenscale",             OPTION_VAL_GREENSCALE_SK },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_SK },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_SK },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_SK },
         { "microvision",            OPTION_VAL_MICROVISION_SK },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_SK },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_SK },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_SK },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_SK },
         { "ti_83",                  OPTION_VAL_TI_83_SK },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_SK },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_SK },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_SK,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_SK,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_SK },
         { "24bit", OPTION_VAL_24BIT_SK },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_SK,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_SK,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_SK },
         { "manual",   OPTION_VAL_MANUAL_SK },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_SK,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_SK,
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
      WSWAN_60HZ_MODE_LABEL_SK,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_SK,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_SK,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_SK,
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
      WSWAN_SOUND_LOW_PASS_LABEL_SK,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_SK,
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
struct retro_core_options_v2 options_sk = {
   option_cats_sk,
   option_defs_sk
};

/* RETRO_LANGUAGE_SR */

#define WSWAN_ROTATE_DISPLAY_LABEL_SR NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_SR NULL
#define OPTION_VAL_MANUAL_SR NULL
#define OPTION_VAL_LANDSCAPE_SR NULL
#define OPTION_VAL_PORTRAIT_SR NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_SR NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_SR NULL
#define OPTION_VAL_AUTO_SR NULL
#define WSWAN_MONO_PALETTE_LABEL_SR NULL
#define WSWAN_MONO_PALETTE_INFO_0_SR NULL
#define OPTION_VAL_DEFAULT_SR NULL
#define OPTION_VAL_WONDERSWAN_SR NULL
#define OPTION_VAL_WONDESWAN_COLOR_SR NULL
#define OPTION_VAL_SWANCRYSTAL_SR NULL
#define OPTION_VAL_GB_DMG_SR NULL
#define OPTION_VAL_GB_POCKET_SR NULL
#define OPTION_VAL_GB_LIGHT_SR NULL
#define OPTION_VAL_BLOSSOM_PINK_SR NULL
#define OPTION_VAL_BUBBLES_BLUE_SR NULL
#define OPTION_VAL_BUTTERCUP_GREEN_SR NULL
#define OPTION_VAL_DIGIVICE_SR NULL
#define OPTION_VAL_GAME_COM_SR NULL
#define OPTION_VAL_GAMEKING_SR NULL
#define OPTION_VAL_GAME_MASTER_SR NULL
#define OPTION_VAL_GOLDEN_WILD_SR NULL
#define OPTION_VAL_GREENSCALE_SR NULL
#define OPTION_VAL_HOKAGE_ORANGE_SR NULL
#define OPTION_VAL_LABO_FAWN_SR NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_SR NULL
#define OPTION_VAL_MICROVISION_SR NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_SR NULL
#define OPTION_VAL_ODYSSEY_GOLD_SR NULL
#define OPTION_VAL_SHINY_SKY_BLUE_SR NULL
#define OPTION_VAL_SLIME_BLUE_SR NULL
#define OPTION_VAL_TI_83_SR NULL
#define OPTION_VAL_TRAVEL_WOOD_SR NULL
#define OPTION_VAL_VIRTUAL_BOY_SR NULL
#define WSWAN_GFX_COLORS_LABEL_SR NULL
#define WSWAN_GFX_COLORS_INFO_0_SR NULL
#define OPTION_VAL_16BIT_SR NULL
#define OPTION_VAL_24BIT_SR NULL
#define WSWAN_FRAMESKIP_LABEL_SR NULL
#define WSWAN_FRAMESKIP_INFO_0_SR NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_SR NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_SR NULL
#define WSWAN_60HZ_MODE_LABEL_SR NULL
#define WSWAN_60HZ_MODE_INFO_0_SR NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_SR NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_SR NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_SR NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_SR NULL

struct retro_core_option_v2_category option_cats_sr[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_sr[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_SR,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_SR,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_SR },
         { "landscape", OPTION_VAL_LANDSCAPE_SR },
         { "portrait",  OPTION_VAL_PORTRAIT_SR },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_SR,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_SR,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_SR },
         { "disabled", OPTION_VAL_LANDSCAPE_SR },
         { "enabled",  OPTION_VAL_PORTRAIT_SR },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_SR,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_SR,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_SR },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_SR },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_SR },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_SR },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_SR },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_SR },
         { "gb_light",               OPTION_VAL_GB_LIGHT_SR },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_SR },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_SR },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_SR },
         { "digivice",               OPTION_VAL_DIGIVICE_SR },
         { "game_com",               OPTION_VAL_GAME_COM_SR },
         { "gameking",               OPTION_VAL_GAMEKING_SR },
         { "game_master",            OPTION_VAL_GAME_MASTER_SR },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_SR },
         { "greenscale",             OPTION_VAL_GREENSCALE_SR },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_SR },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_SR },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_SR },
         { "microvision",            OPTION_VAL_MICROVISION_SR },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_SR },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_SR },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_SR },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_SR },
         { "ti_83",                  OPTION_VAL_TI_83_SR },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_SR },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_SR },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_SR,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_SR,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_SR },
         { "24bit", OPTION_VAL_24BIT_SR },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_SR,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_SR,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_SR },
         { "manual",   OPTION_VAL_MANUAL_SR },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_SR,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_SR,
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
      WSWAN_60HZ_MODE_LABEL_SR,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_SR,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_SR,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_SR,
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
      WSWAN_SOUND_LOW_PASS_LABEL_SR,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_SR,
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
struct retro_core_options_v2 options_sr = {
   option_cats_sr,
   option_defs_sr
};

/* RETRO_LANGUAGE_SV */

#define WSWAN_ROTATE_DISPLAY_LABEL_SV NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_SV NULL
#define OPTION_VAL_MANUAL_SV NULL
#define OPTION_VAL_LANDSCAPE_SV "Landskap"
#define OPTION_VAL_PORTRAIT_SV "Porträtt"
#define WSWAN_ROTATE_KEYMAP_LABEL_SV NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_SV NULL
#define OPTION_VAL_AUTO_SV "Automatiskt"
#define WSWAN_MONO_PALETTE_LABEL_SV "Färgpalett"
#define WSWAN_MONO_PALETTE_INFO_0_SV NULL
#define OPTION_VAL_DEFAULT_SV "Gråskala"
#define OPTION_VAL_WONDERSWAN_SV NULL
#define OPTION_VAL_WONDESWAN_COLOR_SV NULL
#define OPTION_VAL_SWANCRYSTAL_SV NULL
#define OPTION_VAL_GB_DMG_SV NULL
#define OPTION_VAL_GB_POCKET_SV NULL
#define OPTION_VAL_GB_LIGHT_SV NULL
#define OPTION_VAL_BLOSSOM_PINK_SV NULL
#define OPTION_VAL_BUBBLES_BLUE_SV NULL
#define OPTION_VAL_BUTTERCUP_GREEN_SV NULL
#define OPTION_VAL_DIGIVICE_SV NULL
#define OPTION_VAL_GAME_COM_SV NULL
#define OPTION_VAL_GAMEKING_SV NULL
#define OPTION_VAL_GAME_MASTER_SV NULL
#define OPTION_VAL_GOLDEN_WILD_SV NULL
#define OPTION_VAL_GREENSCALE_SV NULL
#define OPTION_VAL_HOKAGE_ORANGE_SV NULL
#define OPTION_VAL_LABO_FAWN_SV NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_SV NULL
#define OPTION_VAL_MICROVISION_SV NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_SV NULL
#define OPTION_VAL_ODYSSEY_GOLD_SV NULL
#define OPTION_VAL_SHINY_SKY_BLUE_SV NULL
#define OPTION_VAL_SLIME_BLUE_SV NULL
#define OPTION_VAL_TI_83_SV NULL
#define OPTION_VAL_TRAVEL_WOOD_SV NULL
#define OPTION_VAL_VIRTUAL_BOY_SV NULL
#define WSWAN_GFX_COLORS_LABEL_SV NULL
#define WSWAN_GFX_COLORS_INFO_0_SV NULL
#define OPTION_VAL_16BIT_SV "Tusentals (16-bitar)"
#define OPTION_VAL_24BIT_SV "Miljontals (24-bitars)"
#define WSWAN_FRAMESKIP_LABEL_SV NULL
#define WSWAN_FRAMESKIP_INFO_0_SV NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_SV NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_SV NULL
#define WSWAN_60HZ_MODE_LABEL_SV NULL
#define WSWAN_60HZ_MODE_INFO_0_SV NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_SV NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_SV NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_SV "Ljudfilter"
#define WSWAN_SOUND_LOW_PASS_INFO_0_SV NULL

struct retro_core_option_v2_category option_cats_sv[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_sv[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_SV,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_SV,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_SV },
         { "landscape", OPTION_VAL_LANDSCAPE_SV },
         { "portrait",  OPTION_VAL_PORTRAIT_SV },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_SV,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_SV,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_SV },
         { "disabled", OPTION_VAL_LANDSCAPE_SV },
         { "enabled",  OPTION_VAL_PORTRAIT_SV },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_SV,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_SV,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_SV },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_SV },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_SV },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_SV },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_SV },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_SV },
         { "gb_light",               OPTION_VAL_GB_LIGHT_SV },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_SV },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_SV },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_SV },
         { "digivice",               OPTION_VAL_DIGIVICE_SV },
         { "game_com",               OPTION_VAL_GAME_COM_SV },
         { "gameking",               OPTION_VAL_GAMEKING_SV },
         { "game_master",            OPTION_VAL_GAME_MASTER_SV },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_SV },
         { "greenscale",             OPTION_VAL_GREENSCALE_SV },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_SV },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_SV },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_SV },
         { "microvision",            OPTION_VAL_MICROVISION_SV },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_SV },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_SV },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_SV },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_SV },
         { "ti_83",                  OPTION_VAL_TI_83_SV },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_SV },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_SV },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_SV,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_SV,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_SV },
         { "24bit", OPTION_VAL_24BIT_SV },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_SV,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_SV,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_SV },
         { "manual",   OPTION_VAL_MANUAL_SV },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_SV,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_SV,
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
      WSWAN_60HZ_MODE_LABEL_SV,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_SV,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_SV,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_SV,
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
      WSWAN_SOUND_LOW_PASS_LABEL_SV,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_SV,
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
struct retro_core_options_v2 options_sv = {
   option_cats_sv,
   option_defs_sv
};

/* RETRO_LANGUAGE_TR */

#define WSWAN_ROTATE_DISPLAY_LABEL_TR "Ekran Döndürme"
#define WSWAN_ROTATE_DISPLAY_INFO_0_TR "Konsol ekranının yönünü seçin. 'Portre', geleneksel (manzara) bir ekranda portre odaklı oyunların doğru düzenini elde etmek için kullanılabilir. 'El İle', kontrolcü aracılığıyla dönüşü etkinleştirir (varsayılan düğme: SEÇ)."
#define OPTION_VAL_MANUAL_TR "El İle"
#define OPTION_VAL_LANDSCAPE_TR "Yatay"
#define OPTION_VAL_PORTRAIT_TR "Dikey"
#define WSWAN_ROTATE_KEYMAP_LABEL_TR "Düğme Eşlemelerini Döndür"
#define WSWAN_ROTATE_KEYMAP_INFO_0_TR "Tercih edilen düğme eşlemesini seçin. 'Otomatik', düğme eşlemesini mevcut ekran dönüşüyle ​​eşleşecek şekilde ayarlar."
#define OPTION_VAL_AUTO_TR "Otomatik"
#define WSWAN_MONO_PALETTE_LABEL_TR "Renk Paleti"
#define WSWAN_MONO_PALETTE_INFO_0_TR "WonderSwan (Mono) oyunlarının renklendirmesini seçin. 'WS' paletleri, orijinal WonderSwan donanımının görüntüsünü taklit eder."
#define OPTION_VAL_DEFAULT_TR "Gri Tonlama"
#define OPTION_VAL_WONDERSWAN_TR NULL
#define OPTION_VAL_WONDESWAN_COLOR_TR "WS - WonderSwan Renk"
#define OPTION_VAL_SWANCRYSTAL_TR NULL
#define OPTION_VAL_GB_DMG_TR NULL
#define OPTION_VAL_GB_POCKET_TR NULL
#define OPTION_VAL_GB_LIGHT_TR NULL
#define OPTION_VAL_BLOSSOM_PINK_TR "Blossom Pembe"
#define OPTION_VAL_BUBBLES_BLUE_TR "Kabarcıklı Mavi"
#define OPTION_VAL_BUTTERCUP_GREEN_TR "Düğün Çiçeği Yeşil"
#define OPTION_VAL_DIGIVICE_TR "Dijital"
#define OPTION_VAL_GAME_COM_TR NULL
#define OPTION_VAL_GAMEKING_TR NULL
#define OPTION_VAL_GAME_MASTER_TR NULL
#define OPTION_VAL_GOLDEN_WILD_TR "Vahşi Altın"
#define OPTION_VAL_GREENSCALE_TR "Yeşil Ton"
#define OPTION_VAL_HOKAGE_ORANGE_TR "Hokage Turuncu"
#define OPTION_VAL_LABO_FAWN_TR NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_TR "Efsanevi Süper Saiyan"
#define OPTION_VAL_MICROVISION_TR NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_TR "Milyon Canlı Altın"
#define OPTION_VAL_ODYSSEY_GOLD_TR "Odyssey Altın"
#define OPTION_VAL_SHINY_SKY_BLUE_TR "Parlak Gök Mavisi"
#define OPTION_VAL_SLIME_BLUE_TR "Balçık Mavisi"
#define OPTION_VAL_TI_83_TR NULL
#define OPTION_VAL_TRAVEL_WOOD_TR "Seyahat Ahşap"
#define OPTION_VAL_VIRTUAL_BOY_TR NULL
#define WSWAN_GFX_COLORS_LABEL_TR "Renk Derinliği (Yeniden Başlatılmalı)"
#define WSWAN_GFX_COLORS_INFO_0_TR "24 bit daha yavaştır ve tüm platformlarda mevcut değildir."
#define OPTION_VAL_16BIT_TR "Binlerce (16-bit)"
#define OPTION_VAL_24BIT_TR "Milyonlarca (24-bit)"
#define WSWAN_FRAMESKIP_LABEL_TR "Kare Atlama"
#define WSWAN_FRAMESKIP_INFO_0_TR "Ses ara belleğinin yetersiz çalışmasını (çatırtı) önlemek için kareleri atlayın. Görsel pürüzsüzlük pahasına performansı artırır. 'Otomatik', ön uç tarafından önerildiğinde çerçeveleri atlar. 'El ile', 'Kare Atlama Eşiği (%)' ayarını kullanır."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_TR "Kare Atlama Eşiği (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_TR "'Kare Atlama', 'El İle' olarak ayarlandığında, altındaki karelerin atlanacağı ses arabelleği doluluk eşiğini (yüzde) belirtir. Daha yüksek değerler, karelerin daha sık yavaşlamasına neden olarak bozulma riskini azaltır."
#define WSWAN_60HZ_MODE_LABEL_TR "60Hz Kipi"
#define WSWAN_60HZ_MODE_INFO_0_TR "Her beş karede bir düşürerek ekranı yerel 75Hz WonderSwan yenileme hızı yerine 60Hz olarak güncelleyin. Video düzgünlüğünü azaltır, ancak 60Hz üzerinde doğal çalışmayı desteklemeyen ekranlarda ekran yırtılmasını önler."
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_TR "Ses Çıkışı Örnek Oranı"
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_TR "Daha yüksek değerler, performans pahasına ses kalitesini biraz artırır."
#define WSWAN_SOUND_LOW_PASS_LABEL_TR "Ses Filtresi"
#define WSWAN_SOUND_LOW_PASS_INFO_0_TR "WonderSwan ile üretilen bazen sert olan ince melodileri 'yumuşatmak' için düşük geçişli bir ses filtresi uygulayın."

struct retro_core_option_v2_category option_cats_tr[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_tr[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_TR,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_TR,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_TR },
         { "landscape", OPTION_VAL_LANDSCAPE_TR },
         { "portrait",  OPTION_VAL_PORTRAIT_TR },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_TR,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_TR,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_TR },
         { "disabled", OPTION_VAL_LANDSCAPE_TR },
         { "enabled",  OPTION_VAL_PORTRAIT_TR },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_TR,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_TR,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_TR },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_TR },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_TR },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_TR },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_TR },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_TR },
         { "gb_light",               OPTION_VAL_GB_LIGHT_TR },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_TR },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_TR },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_TR },
         { "digivice",               OPTION_VAL_DIGIVICE_TR },
         { "game_com",               OPTION_VAL_GAME_COM_TR },
         { "gameking",               OPTION_VAL_GAMEKING_TR },
         { "game_master",            OPTION_VAL_GAME_MASTER_TR },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_TR },
         { "greenscale",             OPTION_VAL_GREENSCALE_TR },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_TR },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_TR },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_TR },
         { "microvision",            OPTION_VAL_MICROVISION_TR },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_TR },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_TR },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_TR },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_TR },
         { "ti_83",                  OPTION_VAL_TI_83_TR },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_TR },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_TR },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_TR,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_TR,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_TR },
         { "24bit", OPTION_VAL_24BIT_TR },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_TR,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_TR,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_TR },
         { "manual",   OPTION_VAL_MANUAL_TR },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_TR,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_TR,
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
      WSWAN_60HZ_MODE_LABEL_TR,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_TR,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_TR,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_TR,
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
      WSWAN_SOUND_LOW_PASS_LABEL_TR,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_TR,
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
struct retro_core_options_v2 options_tr = {
   option_cats_tr,
   option_defs_tr
};

/* RETRO_LANGUAGE_UK */

#define WSWAN_ROTATE_DISPLAY_LABEL_UK "Обертання екрану"
#define WSWAN_ROTATE_DISPLAY_INFO_0_UK NULL
#define OPTION_VAL_MANUAL_UK "Ручний"
#define OPTION_VAL_LANDSCAPE_UK NULL
#define OPTION_VAL_PORTRAIT_UK NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_UK NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_UK NULL
#define OPTION_VAL_AUTO_UK "Авто"
#define WSWAN_MONO_PALETTE_LABEL_UK "Кольорова палітра"
#define WSWAN_MONO_PALETTE_INFO_0_UK NULL
#define OPTION_VAL_DEFAULT_UK NULL
#define OPTION_VAL_WONDERSWAN_UK NULL
#define OPTION_VAL_WONDESWAN_COLOR_UK NULL
#define OPTION_VAL_SWANCRYSTAL_UK NULL
#define OPTION_VAL_GB_DMG_UK NULL
#define OPTION_VAL_GB_POCKET_UK NULL
#define OPTION_VAL_GB_LIGHT_UK NULL
#define OPTION_VAL_BLOSSOM_PINK_UK NULL
#define OPTION_VAL_BUBBLES_BLUE_UK NULL
#define OPTION_VAL_BUTTERCUP_GREEN_UK NULL
#define OPTION_VAL_DIGIVICE_UK NULL
#define OPTION_VAL_GAME_COM_UK NULL
#define OPTION_VAL_GAMEKING_UK NULL
#define OPTION_VAL_GAME_MASTER_UK NULL
#define OPTION_VAL_GOLDEN_WILD_UK NULL
#define OPTION_VAL_GREENSCALE_UK NULL
#define OPTION_VAL_HOKAGE_ORANGE_UK NULL
#define OPTION_VAL_LABO_FAWN_UK NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_UK NULL
#define OPTION_VAL_MICROVISION_UK NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_UK NULL
#define OPTION_VAL_ODYSSEY_GOLD_UK NULL
#define OPTION_VAL_SHINY_SKY_BLUE_UK NULL
#define OPTION_VAL_SLIME_BLUE_UK NULL
#define OPTION_VAL_TI_83_UK NULL
#define OPTION_VAL_TRAVEL_WOOD_UK NULL
#define OPTION_VAL_VIRTUAL_BOY_UK NULL
#define WSWAN_GFX_COLORS_LABEL_UK "Глибина кольору (потрібен перезапуск)"
#define WSWAN_GFX_COLORS_INFO_0_UK NULL
#define OPTION_VAL_16BIT_UK NULL
#define OPTION_VAL_24BIT_UK NULL
#define WSWAN_FRAMESKIP_LABEL_UK "Пропуск кадру"
#define WSWAN_FRAMESKIP_INFO_0_UK "Пропускати кадри, щоб уникнути спустошення буферу (тріскіт). Покращує швидкодію, погіршуючи плавність зображення. 'Авто' пропускає кадри, керуючись командами зовнішньої оболонки. 'Ручний' використовує налаштування 'Межа пропуску кадрів (%)'."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_UK "Межа пропуску кадрів (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_UK NULL
#define WSWAN_60HZ_MODE_LABEL_UK NULL
#define WSWAN_60HZ_MODE_INFO_0_UK NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_UK NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_UK NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_UK "Аудіофільтр"
#define WSWAN_SOUND_LOW_PASS_INFO_0_UK NULL

struct retro_core_option_v2_category option_cats_uk[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_uk[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_UK,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_UK,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_UK },
         { "landscape", OPTION_VAL_LANDSCAPE_UK },
         { "portrait",  OPTION_VAL_PORTRAIT_UK },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_UK,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_UK,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_UK },
         { "disabled", OPTION_VAL_LANDSCAPE_UK },
         { "enabled",  OPTION_VAL_PORTRAIT_UK },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_UK,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_UK,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_UK },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_UK },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_UK },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_UK },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_UK },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_UK },
         { "gb_light",               OPTION_VAL_GB_LIGHT_UK },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_UK },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_UK },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_UK },
         { "digivice",               OPTION_VAL_DIGIVICE_UK },
         { "game_com",               OPTION_VAL_GAME_COM_UK },
         { "gameking",               OPTION_VAL_GAMEKING_UK },
         { "game_master",            OPTION_VAL_GAME_MASTER_UK },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_UK },
         { "greenscale",             OPTION_VAL_GREENSCALE_UK },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_UK },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_UK },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_UK },
         { "microvision",            OPTION_VAL_MICROVISION_UK },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_UK },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_UK },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_UK },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_UK },
         { "ti_83",                  OPTION_VAL_TI_83_UK },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_UK },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_UK },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_UK,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_UK,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_UK },
         { "24bit", OPTION_VAL_24BIT_UK },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_UK,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_UK,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_UK },
         { "manual",   OPTION_VAL_MANUAL_UK },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_UK,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_UK,
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
      WSWAN_60HZ_MODE_LABEL_UK,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_UK,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_UK,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_UK,
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
      WSWAN_SOUND_LOW_PASS_LABEL_UK,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_UK,
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
struct retro_core_options_v2 options_uk = {
   option_cats_uk,
   option_defs_uk
};

/* RETRO_LANGUAGE_VAL */

#define WSWAN_ROTATE_DISPLAY_LABEL_VAL NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_VAL NULL
#define OPTION_VAL_MANUAL_VAL "Selecció manual"
#define OPTION_VAL_LANDSCAPE_VAL NULL
#define OPTION_VAL_PORTRAIT_VAL NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_VAL NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_VAL NULL
#define OPTION_VAL_AUTO_VAL "Selecció automàtica"
#define WSWAN_MONO_PALETTE_LABEL_VAL NULL
#define WSWAN_MONO_PALETTE_INFO_0_VAL NULL
#define OPTION_VAL_DEFAULT_VAL NULL
#define OPTION_VAL_WONDERSWAN_VAL NULL
#define OPTION_VAL_WONDESWAN_COLOR_VAL NULL
#define OPTION_VAL_SWANCRYSTAL_VAL NULL
#define OPTION_VAL_GB_DMG_VAL NULL
#define OPTION_VAL_GB_POCKET_VAL NULL
#define OPTION_VAL_GB_LIGHT_VAL NULL
#define OPTION_VAL_BLOSSOM_PINK_VAL NULL
#define OPTION_VAL_BUBBLES_BLUE_VAL NULL
#define OPTION_VAL_BUTTERCUP_GREEN_VAL NULL
#define OPTION_VAL_DIGIVICE_VAL NULL
#define OPTION_VAL_GAME_COM_VAL NULL
#define OPTION_VAL_GAMEKING_VAL NULL
#define OPTION_VAL_GAME_MASTER_VAL NULL
#define OPTION_VAL_GOLDEN_WILD_VAL NULL
#define OPTION_VAL_GREENSCALE_VAL NULL
#define OPTION_VAL_HOKAGE_ORANGE_VAL NULL
#define OPTION_VAL_LABO_FAWN_VAL NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_VAL NULL
#define OPTION_VAL_MICROVISION_VAL NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_VAL NULL
#define OPTION_VAL_ODYSSEY_GOLD_VAL NULL
#define OPTION_VAL_SHINY_SKY_BLUE_VAL NULL
#define OPTION_VAL_SLIME_BLUE_VAL NULL
#define OPTION_VAL_TI_83_VAL NULL
#define OPTION_VAL_TRAVEL_WOOD_VAL NULL
#define OPTION_VAL_VIRTUAL_BOY_VAL NULL
#define WSWAN_GFX_COLORS_LABEL_VAL NULL
#define WSWAN_GFX_COLORS_INFO_0_VAL NULL
#define OPTION_VAL_16BIT_VAL NULL
#define OPTION_VAL_24BIT_VAL NULL
#define WSWAN_FRAMESKIP_LABEL_VAL NULL
#define WSWAN_FRAMESKIP_INFO_0_VAL NULL
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_VAL NULL
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_VAL NULL
#define WSWAN_60HZ_MODE_LABEL_VAL NULL
#define WSWAN_60HZ_MODE_INFO_0_VAL NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_VAL NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_VAL NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_VAL NULL
#define WSWAN_SOUND_LOW_PASS_INFO_0_VAL NULL

struct retro_core_option_v2_category option_cats_val[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_val[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_VAL,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_VAL,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_VAL },
         { "landscape", OPTION_VAL_LANDSCAPE_VAL },
         { "portrait",  OPTION_VAL_PORTRAIT_VAL },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_VAL,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_VAL,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_VAL },
         { "disabled", OPTION_VAL_LANDSCAPE_VAL },
         { "enabled",  OPTION_VAL_PORTRAIT_VAL },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_VAL,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_VAL,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_VAL },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_VAL },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_VAL },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_VAL },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_VAL },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_VAL },
         { "gb_light",               OPTION_VAL_GB_LIGHT_VAL },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_VAL },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_VAL },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_VAL },
         { "digivice",               OPTION_VAL_DIGIVICE_VAL },
         { "game_com",               OPTION_VAL_GAME_COM_VAL },
         { "gameking",               OPTION_VAL_GAMEKING_VAL },
         { "game_master",            OPTION_VAL_GAME_MASTER_VAL },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_VAL },
         { "greenscale",             OPTION_VAL_GREENSCALE_VAL },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_VAL },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_VAL },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_VAL },
         { "microvision",            OPTION_VAL_MICROVISION_VAL },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_VAL },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_VAL },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_VAL },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_VAL },
         { "ti_83",                  OPTION_VAL_TI_83_VAL },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_VAL },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_VAL },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_VAL,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_VAL,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_VAL },
         { "24bit", OPTION_VAL_24BIT_VAL },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_VAL,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_VAL,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_VAL },
         { "manual",   OPTION_VAL_MANUAL_VAL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_VAL,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_VAL,
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
      WSWAN_60HZ_MODE_LABEL_VAL,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_VAL,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_VAL,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_VAL,
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
      WSWAN_SOUND_LOW_PASS_LABEL_VAL,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_VAL,
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
struct retro_core_options_v2 options_val = {
   option_cats_val,
   option_defs_val
};

/* RETRO_LANGUAGE_VN */

#define WSWAN_ROTATE_DISPLAY_LABEL_VN NULL
#define WSWAN_ROTATE_DISPLAY_INFO_0_VN NULL
#define OPTION_VAL_MANUAL_VN "Thủ công"
#define OPTION_VAL_LANDSCAPE_VN NULL
#define OPTION_VAL_PORTRAIT_VN NULL
#define WSWAN_ROTATE_KEYMAP_LABEL_VN NULL
#define WSWAN_ROTATE_KEYMAP_INFO_0_VN NULL
#define OPTION_VAL_AUTO_VN "Tự động"
#define WSWAN_MONO_PALETTE_LABEL_VN "Bảng màu"
#define WSWAN_MONO_PALETTE_INFO_0_VN NULL
#define OPTION_VAL_DEFAULT_VN NULL
#define OPTION_VAL_WONDERSWAN_VN NULL
#define OPTION_VAL_WONDESWAN_COLOR_VN NULL
#define OPTION_VAL_SWANCRYSTAL_VN NULL
#define OPTION_VAL_GB_DMG_VN NULL
#define OPTION_VAL_GB_POCKET_VN NULL
#define OPTION_VAL_GB_LIGHT_VN NULL
#define OPTION_VAL_BLOSSOM_PINK_VN NULL
#define OPTION_VAL_BUBBLES_BLUE_VN NULL
#define OPTION_VAL_BUTTERCUP_GREEN_VN NULL
#define OPTION_VAL_DIGIVICE_VN NULL
#define OPTION_VAL_GAME_COM_VN NULL
#define OPTION_VAL_GAMEKING_VN NULL
#define OPTION_VAL_GAME_MASTER_VN NULL
#define OPTION_VAL_GOLDEN_WILD_VN NULL
#define OPTION_VAL_GREENSCALE_VN NULL
#define OPTION_VAL_HOKAGE_ORANGE_VN NULL
#define OPTION_VAL_LABO_FAWN_VN NULL
#define OPTION_VAL_LEGENDARY_SUPER_SAIYAN_VN NULL
#define OPTION_VAL_MICROVISION_VN NULL
#define OPTION_VAL_MILLION_LIVE_GOLD_VN NULL
#define OPTION_VAL_ODYSSEY_GOLD_VN NULL
#define OPTION_VAL_SHINY_SKY_BLUE_VN NULL
#define OPTION_VAL_SLIME_BLUE_VN NULL
#define OPTION_VAL_TI_83_VN NULL
#define OPTION_VAL_TRAVEL_WOOD_VN NULL
#define OPTION_VAL_VIRTUAL_BOY_VN NULL
#define WSWAN_GFX_COLORS_LABEL_VN NULL
#define WSWAN_GFX_COLORS_INFO_0_VN NULL
#define OPTION_VAL_16BIT_VN NULL
#define OPTION_VAL_24BIT_VN NULL
#define WSWAN_FRAMESKIP_LABEL_VN "Bỏ qua khung hình"
#define WSWAN_FRAMESKIP_INFO_0_VN "Bỏ qua khung hình để tránh bộ đệm âm thanh chạy dưới mức (tiếng kêu lách tách). Cải thiện hiệu suất với độ mượt mà của hình ảnh. 'Tự động' bỏ qua khung khi giao diện người dùng thông báo. 'Thủ công' sử dụng cài đặt 'Ngưỡng khung hình (%)'."
#define WSWAN_FRAMESKIP_THRESHOLD_LABEL_VN "Ngưỡng bỏ qua khung hình (%)"
#define WSWAN_FRAMESKIP_THRESHOLD_INFO_0_VN NULL
#define WSWAN_60HZ_MODE_LABEL_VN NULL
#define WSWAN_60HZ_MODE_INFO_0_VN NULL
#define WSWAN_SOUND_SAMPLE_RATE_LABEL_VN NULL
#define WSWAN_SOUND_SAMPLE_RATE_INFO_0_VN NULL
#define WSWAN_SOUND_LOW_PASS_LABEL_VN "Âm thanh Filter Danh mục"
#define WSWAN_SOUND_LOW_PASS_INFO_0_VN NULL

struct retro_core_option_v2_category option_cats_vn[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_vn[] = {
   {
      "wswan_rotate_display",
      WSWAN_ROTATE_DISPLAY_LABEL_VN,
      NULL,
      WSWAN_ROTATE_DISPLAY_INFO_0_VN,
      NULL,
      NULL,
      {
         { "manual",    OPTION_VAL_MANUAL_VN },
         { "landscape", OPTION_VAL_LANDSCAPE_VN },
         { "portrait",  OPTION_VAL_PORTRAIT_VN },
         { NULL, NULL },
      },
      "manual",
   },
   {
      "wswan_rotate_keymap",
      WSWAN_ROTATE_KEYMAP_LABEL_VN,
      NULL,
      WSWAN_ROTATE_KEYMAP_INFO_0_VN,
      NULL,
      NULL,
      {
         { "auto",     OPTION_VAL_AUTO_VN },
         { "disabled", OPTION_VAL_LANDSCAPE_VN },
         { "enabled",  OPTION_VAL_PORTRAIT_VN },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "wswan_mono_palette",
      WSWAN_MONO_PALETTE_LABEL_VN,
      NULL,
      WSWAN_MONO_PALETTE_INFO_0_VN,
      NULL,
      NULL,
      {
         { "default",                OPTION_VAL_DEFAULT_VN },
         { "wonderswan",             OPTION_VAL_WONDERSWAN_VN },
         { "wondeswan_color",        OPTION_VAL_WONDESWAN_COLOR_VN },
         { "swancrystal",            OPTION_VAL_SWANCRYSTAL_VN },
         { "gb_dmg",                 OPTION_VAL_GB_DMG_VN },
         { "gb_pocket",              OPTION_VAL_GB_POCKET_VN },
         { "gb_light",               OPTION_VAL_GB_LIGHT_VN },
         { "blossom_pink",           OPTION_VAL_BLOSSOM_PINK_VN },
         { "bubbles_blue",           OPTION_VAL_BUBBLES_BLUE_VN },
         { "buttercup_green",        OPTION_VAL_BUTTERCUP_GREEN_VN },
         { "digivice",               OPTION_VAL_DIGIVICE_VN },
         { "game_com",               OPTION_VAL_GAME_COM_VN },
         { "gameking",               OPTION_VAL_GAMEKING_VN },
         { "game_master",            OPTION_VAL_GAME_MASTER_VN },
         { "golden_wild",            OPTION_VAL_GOLDEN_WILD_VN },
         { "greenscale",             OPTION_VAL_GREENSCALE_VN },
         { "hokage_orange",          OPTION_VAL_HOKAGE_ORANGE_VN },
         { "labo_fawn",              OPTION_VAL_LABO_FAWN_VN },
         { "legendary_super_saiyan", OPTION_VAL_LEGENDARY_SUPER_SAIYAN_VN },
         { "microvision",            OPTION_VAL_MICROVISION_VN },
         { "million_live_gold",      OPTION_VAL_MILLION_LIVE_GOLD_VN },
         { "odyssey_gold",           OPTION_VAL_ODYSSEY_GOLD_VN },
         { "shiny_sky_blue",         OPTION_VAL_SHINY_SKY_BLUE_VN },
         { "slime_blue",             OPTION_VAL_SLIME_BLUE_VN },
         { "ti_83",                  OPTION_VAL_TI_83_VN },
         { "travel_wood",            OPTION_VAL_TRAVEL_WOOD_VN },
         { "virtual_boy",            OPTION_VAL_VIRTUAL_BOY_VN },
         { NULL, NULL },
      },
      "default"
   },
   {
      "wswan_gfx_colors",
      WSWAN_GFX_COLORS_LABEL_VN,
      NULL,
      WSWAN_GFX_COLORS_INFO_0_VN,
      NULL,
      NULL,
      {
         { "16bit", OPTION_VAL_16BIT_VN },
         { "24bit", OPTION_VAL_24BIT_VN },
         { NULL, NULL },
      },
      "16bit"
   },
   {
      "wswan_frameskip",
      WSWAN_FRAMESKIP_LABEL_VN,
      NULL,
      WSWAN_FRAMESKIP_INFO_0_VN,
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "auto",     OPTION_VAL_AUTO_VN },
         { "manual",   OPTION_VAL_MANUAL_VN },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "wswan_frameskip_threshold",
      WSWAN_FRAMESKIP_THRESHOLD_LABEL_VN,
      NULL,
      WSWAN_FRAMESKIP_THRESHOLD_INFO_0_VN,
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
      WSWAN_60HZ_MODE_LABEL_VN,
      NULL,
      WSWAN_60HZ_MODE_INFO_0_VN,
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
      WSWAN_SOUND_SAMPLE_RATE_LABEL_VN,
      NULL,
      WSWAN_SOUND_SAMPLE_RATE_INFO_0_VN,
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
      WSWAN_SOUND_LOW_PASS_LABEL_VN,
      NULL,
      WSWAN_SOUND_LOW_PASS_INFO_0_VN,
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
struct retro_core_options_v2 options_vn = {
   option_cats_vn,
   option_defs_vn
};


#ifdef __cplusplus
}
#endif

#endif