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

#define LYNX_ROT_SCREEN_LABEL_AR NULL
#define LYNX_ROT_SCREEN_INFO_0_AR NULL
#define OPTION_VAL_AUTO_AR "تلقائي"
#define OPTION_VAL_MANUAL_AR "يدوي"
#define LYNX_PIX_FORMAT_LABEL_AR NULL
#define OPTION_VAL_16_AR NULL
#define OPTION_VAL_32_AR NULL
#define LYNX_FORCE_60HZ_LABEL_AR NULL
#define LYNX_FORCE_60HZ_INFO_0_AR NULL

struct retro_core_option_v2_category option_cats_ar[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ar[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_AR,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_AR,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_AR },
         { "manual", OPTION_VAL_MANUAL_AR },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_AR,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_AR },
         { "32", OPTION_VAL_32_AR },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_AR,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_AR,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_ar = {
   option_cats_ar,
   option_defs_ar
};

/* RETRO_LANGUAGE_AST */

#define LYNX_ROT_SCREEN_LABEL_AST "Voltiar automáticamente la pantalla"
#define LYNX_ROT_SCREEN_INFO_0_AST NULL
#define OPTION_VAL_AUTO_AST NULL
#define OPTION_VAL_MANUAL_AST NULL
#define LYNX_PIX_FORMAT_LABEL_AST NULL
#define OPTION_VAL_16_AST NULL
#define OPTION_VAL_32_AST NULL
#define LYNX_FORCE_60HZ_LABEL_AST NULL
#define LYNX_FORCE_60HZ_INFO_0_AST NULL

struct retro_core_option_v2_category option_cats_ast[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ast[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_AST,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_AST,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_AST },
         { "manual", OPTION_VAL_MANUAL_AST },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_AST,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_AST },
         { "32", OPTION_VAL_32_AST },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_AST,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_AST,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_ast = {
   option_cats_ast,
   option_defs_ast
};

/* RETRO_LANGUAGE_BE */

#define LYNX_ROT_SCREEN_LABEL_BE NULL
#define LYNX_ROT_SCREEN_INFO_0_BE NULL
#define OPTION_VAL_AUTO_BE "Аўта"
#define OPTION_VAL_MANUAL_BE NULL
#define LYNX_PIX_FORMAT_LABEL_BE NULL
#define OPTION_VAL_16_BE NULL
#define OPTION_VAL_32_BE NULL
#define LYNX_FORCE_60HZ_LABEL_BE NULL
#define LYNX_FORCE_60HZ_INFO_0_BE NULL

struct retro_core_option_v2_category option_cats_be[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_be[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_BE,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_BE,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_BE },
         { "manual", OPTION_VAL_MANUAL_BE },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_BE,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_BE },
         { "32", OPTION_VAL_32_BE },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_BE,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_BE,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_be = {
   option_cats_be,
   option_defs_be
};

/* RETRO_LANGUAGE_CA */

#define LYNX_ROT_SCREEN_LABEL_CA "Gir de Pantalla Automàtic"
#define LYNX_ROT_SCREEN_INFO_0_CA "Gira virtualment l'orientació de la pantalla i els mapes de botons per a jocs coneguts. Quan s'estableix a \"Manual\", la rotació de la pantalla s'ajusta prement el botó SELECT; en cas contrari, es pot establir una rotació fixa a 0, 90, 180 o 270 graus en sentit contrari a les agulles del rellotge."
#define OPTION_VAL_AUTO_CA "Automàtic"
#define OPTION_VAL_MANUAL_CA NULL
#define LYNX_PIX_FORMAT_LABEL_CA "Format de Color (Reinici Necessari)"
#define OPTION_VAL_16_CA NULL
#define OPTION_VAL_32_CA NULL
#define LYNX_FORCE_60HZ_LABEL_CA NULL
#define LYNX_FORCE_60HZ_INFO_0_CA NULL

struct retro_core_option_v2_category option_cats_ca[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ca[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_CA,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_CA,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_CA },
         { "manual", OPTION_VAL_MANUAL_CA },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_CA,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_CA },
         { "32", OPTION_VAL_32_CA },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_CA,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_CA,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_ca = {
   option_cats_ca,
   option_defs_ca
};

/* RETRO_LANGUAGE_CHS */

#define LYNX_ROT_SCREEN_LABEL_CHS "自动旋转屏幕"
#define LYNX_ROT_SCREEN_INFO_0_CHS "为已知游戏自动旋转虚拟屏幕方向和按键映射。当设置为“手动”时，按选择按钮调整屏幕旋转，否则可以设置 逆时针旋转0°、90°、180° 或 270 °为固定旋转角度。"
#define OPTION_VAL_AUTO_CHS "自动"
#define OPTION_VAL_MANUAL_CHS "手动"
#define LYNX_PIX_FORMAT_LABEL_CHS "色彩格式 (需要重启)"
#define OPTION_VAL_16_CHS "16位格式 (RGB565格式)"
#define OPTION_VAL_32_CHS "32位 格式(RGB8888格式)"
#define LYNX_FORCE_60HZ_LABEL_CHS NULL
#define LYNX_FORCE_60HZ_INFO_0_CHS NULL

struct retro_core_option_v2_category option_cats_chs[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_chs[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_CHS,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_CHS,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_CHS },
         { "manual", OPTION_VAL_MANUAL_CHS },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_CHS,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_CHS },
         { "32", OPTION_VAL_32_CHS },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_CHS,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_CHS,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_chs = {
   option_cats_chs,
   option_defs_chs
};

/* RETRO_LANGUAGE_CHT */

#define LYNX_ROT_SCREEN_LABEL_CHT "自動旋轉螢幕"
#define LYNX_ROT_SCREEN_INFO_0_CHT "設定旋轉螢幕的模式, 設定<手動>時按[選擇鍵]逆時針旋轉, 90度、180度、270度和0度的順序重複切換。"
#define OPTION_VAL_AUTO_CHT "自動"
#define OPTION_VAL_MANUAL_CHT "手動"
#define LYNX_PIX_FORMAT_LABEL_CHT "色彩格式 (需要重新啟動)"
#define OPTION_VAL_16_CHT "16位元 (RGB565)"
#define OPTION_VAL_32_CHT "32位元 (RGB8888)"
#define LYNX_FORCE_60HZ_LABEL_CHT NULL
#define LYNX_FORCE_60HZ_INFO_0_CHT NULL

struct retro_core_option_v2_category option_cats_cht[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_cht[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_CHT,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_CHT,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_CHT },
         { "manual", OPTION_VAL_MANUAL_CHT },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_CHT,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_CHT },
         { "32", OPTION_VAL_32_CHT },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_CHT,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_CHT,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_cht = {
   option_cats_cht,
   option_defs_cht
};

/* RETRO_LANGUAGE_CS */

#define LYNX_ROT_SCREEN_LABEL_CS "Automatické otáčení obrazovky"
#define LYNX_ROT_SCREEN_INFO_0_CS "Prakticky automaticky otáčí orientaci obrazovky a mapování tlačítek pro známé hry. Pokud je nastaveno \"Ruční\", otáčení obrazovky se upravuje stisknutím tlačítka SELECT, jinak lze nastavit pevné otáčení buď na 0, 90, 180 nebo 270 stupňů proti směru hodinových ručiček."
#define OPTION_VAL_AUTO_CS "Automatické"
#define OPTION_VAL_MANUAL_CS "Manuální"
#define LYNX_PIX_FORMAT_LABEL_CS "Formát barev (nutný restart)"
#define OPTION_VAL_16_CS NULL
#define OPTION_VAL_32_CS NULL
#define LYNX_FORCE_60HZ_LABEL_CS "Vynutit 60Hz"
#define LYNX_FORCE_60HZ_INFO_0_CS "Vynucení 60 Hz namísto původní obnovovací frekvence 75 Hz pro dokonale plynulý pohyb na 60Hz displejích"

struct retro_core_option_v2_category option_cats_cs[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_cs[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_CS,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_CS,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_CS },
         { "manual", OPTION_VAL_MANUAL_CS },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_CS,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_CS },
         { "32", OPTION_VAL_32_CS },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_CS,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_CS,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_cs = {
   option_cats_cs,
   option_defs_cs
};

/* RETRO_LANGUAGE_CY */

#define LYNX_ROT_SCREEN_LABEL_CY NULL
#define LYNX_ROT_SCREEN_INFO_0_CY NULL
#define OPTION_VAL_AUTO_CY NULL
#define OPTION_VAL_MANUAL_CY NULL
#define LYNX_PIX_FORMAT_LABEL_CY NULL
#define OPTION_VAL_16_CY NULL
#define OPTION_VAL_32_CY NULL
#define LYNX_FORCE_60HZ_LABEL_CY NULL
#define LYNX_FORCE_60HZ_INFO_0_CY NULL

struct retro_core_option_v2_category option_cats_cy[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_cy[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_CY,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_CY,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_CY },
         { "manual", OPTION_VAL_MANUAL_CY },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_CY,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_CY },
         { "32", OPTION_VAL_32_CY },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_CY,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_CY,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_cy = {
   option_cats_cy,
   option_defs_cy
};

/* RETRO_LANGUAGE_DA */

#define LYNX_ROT_SCREEN_LABEL_DA NULL
#define LYNX_ROT_SCREEN_INFO_0_DA NULL
#define OPTION_VAL_AUTO_DA NULL
#define OPTION_VAL_MANUAL_DA NULL
#define LYNX_PIX_FORMAT_LABEL_DA NULL
#define OPTION_VAL_16_DA NULL
#define OPTION_VAL_32_DA NULL
#define LYNX_FORCE_60HZ_LABEL_DA NULL
#define LYNX_FORCE_60HZ_INFO_0_DA NULL

struct retro_core_option_v2_category option_cats_da[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_da[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_DA,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_DA,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_DA },
         { "manual", OPTION_VAL_MANUAL_DA },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_DA,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_DA },
         { "32", OPTION_VAL_32_DA },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_DA,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_DA,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_da = {
   option_cats_da,
   option_defs_da
};

/* RETRO_LANGUAGE_DE */

#define LYNX_ROT_SCREEN_LABEL_DE "Bildschirm automatisch drehen"
#define LYNX_ROT_SCREEN_INFO_0_DE "Bildschirmausrichtung und Tastenzuordnung für bekannte Spiele automatisch virtuell drehen. Bei der Einstellung „Manuell“ wird die Bildschirmdrehung durch Drücken der SELECT-Taste angepasst, andernfalls kann eine feste Drehung auf 0, 90, 180 oder 270 Grad gegen den Uhrzeigersinn eingestellt werden."
#define OPTION_VAL_AUTO_DE "Automatisch"
#define OPTION_VAL_MANUAL_DE "Manuell"
#define LYNX_PIX_FORMAT_LABEL_DE "Farbformat (Neustart erforderlich)"
#define OPTION_VAL_16_DE NULL
#define OPTION_VAL_32_DE NULL
#define LYNX_FORCE_60HZ_LABEL_DE "60 Hz erzwingen"
#define LYNX_FORCE_60HZ_INFO_0_DE "60 Hz statt der originalen 75 Hz Bildwiederholrate erzwingen, um perfekt flüssige Bewegungen auf 60-Hz-Bildschirmen zu ermöglichen"

struct retro_core_option_v2_category option_cats_de[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_de[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_DE,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_DE,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_DE },
         { "manual", OPTION_VAL_MANUAL_DE },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_DE,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_DE },
         { "32", OPTION_VAL_32_DE },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_DE,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_DE,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_de = {
   option_cats_de,
   option_defs_de
};

/* RETRO_LANGUAGE_EL */

#define LYNX_ROT_SCREEN_LABEL_EL "Αυτόματη Περιστροφή Οθόνης"
#define LYNX_ROT_SCREEN_INFO_0_EL NULL
#define OPTION_VAL_AUTO_EL "Αυτόματο"
#define OPTION_VAL_MANUAL_EL "Χειροκίνητο"
#define LYNX_PIX_FORMAT_LABEL_EL "Μορφή Χρώματος (Απαιτείται Επανεκκίνηση)"
#define OPTION_VAL_16_EL NULL
#define OPTION_VAL_32_EL NULL
#define LYNX_FORCE_60HZ_LABEL_EL NULL
#define LYNX_FORCE_60HZ_INFO_0_EL NULL

struct retro_core_option_v2_category option_cats_el[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_el[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_EL,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_EL,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_EL },
         { "manual", OPTION_VAL_MANUAL_EL },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_EL,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_EL },
         { "32", OPTION_VAL_32_EL },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_EL,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_EL,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_el = {
   option_cats_el,
   option_defs_el
};

/* RETRO_LANGUAGE_EN */

#define LYNX_ROT_SCREEN_LABEL_EN NULL
#define LYNX_ROT_SCREEN_INFO_0_EN NULL
#define OPTION_VAL_AUTO_EN NULL
#define OPTION_VAL_MANUAL_EN NULL
#define LYNX_PIX_FORMAT_LABEL_EN "Colour Format (Restart Required)"
#define OPTION_VAL_16_EN NULL
#define OPTION_VAL_32_EN NULL
#define LYNX_FORCE_60HZ_LABEL_EN NULL
#define LYNX_FORCE_60HZ_INFO_0_EN NULL

struct retro_core_option_v2_category option_cats_en[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_en[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_EN,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_EN,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_EN },
         { "manual", OPTION_VAL_MANUAL_EN },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_EN,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_EN },
         { "32", OPTION_VAL_32_EN },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_EN,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_EN,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_en = {
   option_cats_en,
   option_defs_en
};

/* RETRO_LANGUAGE_EO */

#define LYNX_ROT_SCREEN_LABEL_EO NULL
#define LYNX_ROT_SCREEN_INFO_0_EO NULL
#define OPTION_VAL_AUTO_EO NULL
#define OPTION_VAL_MANUAL_EO NULL
#define LYNX_PIX_FORMAT_LABEL_EO NULL
#define OPTION_VAL_16_EO NULL
#define OPTION_VAL_32_EO NULL
#define LYNX_FORCE_60HZ_LABEL_EO NULL
#define LYNX_FORCE_60HZ_INFO_0_EO NULL

struct retro_core_option_v2_category option_cats_eo[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_eo[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_EO,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_EO,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_EO },
         { "manual", OPTION_VAL_MANUAL_EO },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_EO,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_EO },
         { "32", OPTION_VAL_32_EO },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_EO,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_EO,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_eo = {
   option_cats_eo,
   option_defs_eo
};

/* RETRO_LANGUAGE_ES */

#define LYNX_ROT_SCREEN_LABEL_ES "Rotación automática de pantalla"
#define LYNX_ROT_SCREEN_INFO_0_ES "Rota automáticamente la orientación de la pantalla y las asignaciones de controles en aquellos juegos que lo necesiten. Al seleccionar la opción «Manual», la rotación de pantalla se podrá ajustar al pulsar el botón SELECT, en caso contrario, se puede asignar una rotación fija de 0º, 90º, 180º o 270º (en el sentido contrario al de las agujas del reloj)."
#define OPTION_VAL_AUTO_ES "Selección automática"
#define OPTION_VAL_MANUAL_ES "Selección manual"
#define LYNX_PIX_FORMAT_LABEL_ES "Formato de color (es necesario reiniciar)"
#define OPTION_VAL_16_ES "16 bits (RGB565)"
#define OPTION_VAL_32_ES "32 bits (RGB8888)"
#define LYNX_FORCE_60HZ_LABEL_ES "Forzar 60Hz"
#define LYNX_FORCE_60HZ_INFO_0_ES "Fuerza una frecuencia de actualización de 60 Hz en vez de utilizar la original de 75 Hz. Así, el movimiento será perfectamente suave en pantallas que funcionen a 60 Hz."

struct retro_core_option_v2_category option_cats_es[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_es[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_ES,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_ES,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_ES },
         { "manual", OPTION_VAL_MANUAL_ES },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_ES,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_ES },
         { "32", OPTION_VAL_32_ES },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_ES,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_ES,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_es = {
   option_cats_es,
   option_defs_es
};

/* RETRO_LANGUAGE_FA */

#define LYNX_ROT_SCREEN_LABEL_FA "چرخش خودکار نمایشگر"
#define LYNX_ROT_SCREEN_INFO_0_FA "جهت‌گیری نمایشگر و نگاشت دکمه‌ها را به طور خودکار برای بازی‌های شناخته شده بچرخانید. هنگامی که روی «دستی» تنظیم می‌شود، چرخش صفحه با فشاردادن دکمه SELECT تنظیم می‌شود، در غیر این صورت یک چرخش ثابت را می‌توان روی ۰، ۹۰، ۱۸۰ یا ۲۷۰ درجه پادساعتگرد تنظیم کرد."
#define OPTION_VAL_AUTO_FA "خودکار"
#define OPTION_VAL_MANUAL_FA "دستی"
#define LYNX_PIX_FORMAT_LABEL_FA "فرمت رنگ (نیازمند بازآغاز)"
#define OPTION_VAL_16_FA "۱۶-بیتی (RGB۵۶۵)"
#define OPTION_VAL_32_FA "۳۲-بیتی (RGB۸۸۸۸)"
#define LYNX_FORCE_60HZ_LABEL_FA NULL
#define LYNX_FORCE_60HZ_INFO_0_FA NULL

struct retro_core_option_v2_category option_cats_fa[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_fa[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_FA,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_FA,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_FA },
         { "manual", OPTION_VAL_MANUAL_FA },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_FA,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_FA },
         { "32", OPTION_VAL_32_FA },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_FA,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_FA,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_fa = {
   option_cats_fa,
   option_defs_fa
};

/* RETRO_LANGUAGE_FI */

#define LYNX_ROT_SCREEN_LABEL_FI "Näytön kierto"
#define LYNX_ROT_SCREEN_INFO_0_FI "Virtuaalisesti kierrä näytön ja painikkeiden suuntaa tunnetuille peleille. Kun asetus on \"Manuaalinen\", näytön kierto säädetään painamalla SELECT-painiketta, muuten kiinteä kierto voidaan asettaa joko 0, 90, 180, tai 270 asteen vastapäivään."
#define OPTION_VAL_AUTO_FI "Automaattinen"
#define OPTION_VAL_MANUAL_FI "Manuaalinen"
#define LYNX_PIX_FORMAT_LABEL_FI "Väriformaatti (Uudelleenkäynnistys vaaditaan)"
#define OPTION_VAL_16_FI NULL
#define OPTION_VAL_32_FI NULL
#define LYNX_FORCE_60HZ_LABEL_FI "Pakota 60 Hz"
#define LYNX_FORCE_60HZ_INFO_0_FI "Pakota 60 hertsiä alkuperäisen 75 hertsin virkistystaajuuden sijaan, tekee liikkumisesta tasaista 60 hertsin näytöillä"

struct retro_core_option_v2_category option_cats_fi[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_fi[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_FI,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_FI,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_FI },
         { "manual", OPTION_VAL_MANUAL_FI },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_FI,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_FI },
         { "32", OPTION_VAL_32_FI },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_FI,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_FI,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_fi = {
   option_cats_fi,
   option_defs_fi
};

/* RETRO_LANGUAGE_FR */

#define LYNX_ROT_SCREEN_LABEL_FR "Rotation automatique de l'écran"
#define LYNX_ROT_SCREEN_INFO_0_FR "Faire pivoter automatiquement l'orientation de l'écran et le mappage des touches pour les jeux connus. Lorsque définie sur 'Manuelle', la rotation de l'écran est réglée en appuyant sur la touche SELECT, sinon une rotation fixe peut être réglée à 0, 90, 180 ou 270 degrés dans le sens anti-horaire."
#define OPTION_VAL_AUTO_FR NULL
#define OPTION_VAL_MANUAL_FR "Manuelle"
#define LYNX_PIX_FORMAT_LABEL_FR "Format des couleurs (Redémarrage requis)"
#define OPTION_VAL_16_FR "16 bits (RVB565)"
#define OPTION_VAL_32_FR "32 bits (RVB8888)"
#define LYNX_FORCE_60HZ_LABEL_FR "Forcer 60 Hz"
#define LYNX_FORCE_60HZ_INFO_0_FR "Forcer une fréquence de rafraîchissement de 60 Hz au lieu des 75 Hz d'origine, pour un mouvement parfaitement fluide sur les écrans à 60 Hz"

struct retro_core_option_v2_category option_cats_fr[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_fr[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_FR,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_FR,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_FR },
         { "manual", OPTION_VAL_MANUAL_FR },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_FR,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_FR },
         { "32", OPTION_VAL_32_FR },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_FR,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_FR,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_fr = {
   option_cats_fr,
   option_defs_fr
};

/* RETRO_LANGUAGE_GL */

#define LYNX_ROT_SCREEN_LABEL_GL NULL
#define LYNX_ROT_SCREEN_INFO_0_GL NULL
#define OPTION_VAL_AUTO_GL NULL
#define OPTION_VAL_MANUAL_GL NULL
#define LYNX_PIX_FORMAT_LABEL_GL NULL
#define OPTION_VAL_16_GL NULL
#define OPTION_VAL_32_GL NULL
#define LYNX_FORCE_60HZ_LABEL_GL NULL
#define LYNX_FORCE_60HZ_INFO_0_GL NULL

struct retro_core_option_v2_category option_cats_gl[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_gl[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_GL,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_GL,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_GL },
         { "manual", OPTION_VAL_MANUAL_GL },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_GL,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_GL },
         { "32", OPTION_VAL_32_GL },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_GL,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_GL,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_gl = {
   option_cats_gl,
   option_defs_gl
};

/* RETRO_LANGUAGE_HE */

#define LYNX_ROT_SCREEN_LABEL_HE NULL
#define LYNX_ROT_SCREEN_INFO_0_HE NULL
#define OPTION_VAL_AUTO_HE NULL
#define OPTION_VAL_MANUAL_HE NULL
#define LYNX_PIX_FORMAT_LABEL_HE NULL
#define OPTION_VAL_16_HE NULL
#define OPTION_VAL_32_HE NULL
#define LYNX_FORCE_60HZ_LABEL_HE NULL
#define LYNX_FORCE_60HZ_INFO_0_HE NULL

struct retro_core_option_v2_category option_cats_he[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_he[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_HE,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_HE,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_HE },
         { "manual", OPTION_VAL_MANUAL_HE },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_HE,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_HE },
         { "32", OPTION_VAL_32_HE },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_HE,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_HE,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_he = {
   option_cats_he,
   option_defs_he
};

/* RETRO_LANGUAGE_HR */

#define LYNX_ROT_SCREEN_LABEL_HR NULL
#define LYNX_ROT_SCREEN_INFO_0_HR NULL
#define OPTION_VAL_AUTO_HR NULL
#define OPTION_VAL_MANUAL_HR NULL
#define LYNX_PIX_FORMAT_LABEL_HR NULL
#define OPTION_VAL_16_HR NULL
#define OPTION_VAL_32_HR NULL
#define LYNX_FORCE_60HZ_LABEL_HR NULL
#define LYNX_FORCE_60HZ_INFO_0_HR NULL

struct retro_core_option_v2_category option_cats_hr[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_hr[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_HR,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_HR,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_HR },
         { "manual", OPTION_VAL_MANUAL_HR },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_HR,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_HR },
         { "32", OPTION_VAL_32_HR },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_HR,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_HR,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_hr = {
   option_cats_hr,
   option_defs_hr
};

/* RETRO_LANGUAGE_HU */

#define LYNX_ROT_SCREEN_LABEL_HU "Automatikus képernyőforgatás"
#define LYNX_ROT_SCREEN_INFO_0_HU "A képernyőirány és gombkiosztás virtuális elforgatása az ismert játékokhoz automatikusan. \"Kézi\" esetben a Select gomb lenyomásával forgatható, vagy fix elforgatás választható 0, 90, 180, vagy 270 fokban az óramutató járásával ellentétesen."
#define OPTION_VAL_AUTO_HU "Automatikus"
#define OPTION_VAL_MANUAL_HU "Kézi"
#define LYNX_PIX_FORMAT_LABEL_HU "Színformátum (újraindítás szükséges)"
#define OPTION_VAL_16_HU "16 bites (RGB565)"
#define OPTION_VAL_32_HU "32 bites (RGB8888)"
#define LYNX_FORCE_60HZ_LABEL_HU "Mindenképpen 60 Hz"
#define LYNX_FORCE_60HZ_INFO_0_HU "Az eredeti 75 Hz-es képfrissítés helyett 60 Hz használata, a teljesen sima mozgásért 60 Hz-es kijelzőkön"

struct retro_core_option_v2_category option_cats_hu[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_hu[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_HU,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_HU,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_HU },
         { "manual", OPTION_VAL_MANUAL_HU },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_HU,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_HU },
         { "32", OPTION_VAL_32_HU },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_HU,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_HU,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_hu = {
   option_cats_hu,
   option_defs_hu
};

/* RETRO_LANGUAGE_ID */

#define LYNX_ROT_SCREEN_LABEL_ID "Putar-layar Otomatis"
#define LYNX_ROT_SCREEN_INFO_0_ID "Putar orientasi layar dan pemetaan tombol secara virtual secara otomatis untuk game yang dikenal. Saat disetel ke 'Manual', rotasi layar disesuaikan dengan menekan tombol PILIH, jika tidak, rotasi tetap dapat disetel ke 0, 90, 180, atau 270 derajat berlawanan arah jarum jam."
#define OPTION_VAL_AUTO_ID "Otomatis"
#define OPTION_VAL_MANUAL_ID NULL
#define LYNX_PIX_FORMAT_LABEL_ID "Format Warna (Diperlukan Mulai Ulang)"
#define OPTION_VAL_16_ID NULL
#define OPTION_VAL_32_ID NULL
#define LYNX_FORCE_60HZ_LABEL_ID NULL
#define LYNX_FORCE_60HZ_INFO_0_ID NULL

struct retro_core_option_v2_category option_cats_id[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_id[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_ID,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_ID,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_ID },
         { "manual", OPTION_VAL_MANUAL_ID },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_ID,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_ID },
         { "32", OPTION_VAL_32_ID },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_ID,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_ID,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_id = {
   option_cats_id,
   option_defs_id
};

/* RETRO_LANGUAGE_IT */

#define LYNX_ROT_SCREEN_LABEL_IT "Rotazione Automatica Schermo"
#define LYNX_ROT_SCREEN_INFO_0_IT "Ruota virtualmente l'orientamento dello schermo e le mappature dei pulsanti automaticamente per i giochi conosciuti. Quando impostato su 'Manuale', la rotazione dello schermo viene regolata premendo il pulsante SELECT, altrimenti una rotazione fissa può essere impostata a 0, 90, 180 o 270 gradi in senso antiorario."
#define OPTION_VAL_AUTO_IT "Automatico"
#define OPTION_VAL_MANUAL_IT "Manuale"
#define LYNX_PIX_FORMAT_LABEL_IT "Formato Colore (Riavvio Richiesto)"
#define OPTION_VAL_16_IT NULL
#define OPTION_VAL_32_IT NULL
#define LYNX_FORCE_60HZ_LABEL_IT "Forza 60Hz"
#define LYNX_FORCE_60HZ_INFO_0_IT "Forza 60Hz invece della frequenza di aggiornamento 75Hz originale, per un movimento perfettamente fluido sui display a 60Hz"

struct retro_core_option_v2_category option_cats_it[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_it[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_IT,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_IT,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_IT },
         { "manual", OPTION_VAL_MANUAL_IT },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_IT,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_IT },
         { "32", OPTION_VAL_32_IT },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_IT,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_IT,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_it = {
   option_cats_it,
   option_defs_it
};

/* RETRO_LANGUAGE_JA */

#define LYNX_ROT_SCREEN_LABEL_JA "画面の自動回転"
#define LYNX_ROT_SCREEN_INFO_0_JA "既知のゲームの画面の向きとボタンのマッピングを実質的に自動的に回転させます。 「手動」に設定すると、SELECTボタンを押して画面の回転を調整します。 それ以外の場合、固定された回転は反時計回りに0、90、180、または270度に設定できます。"
#define OPTION_VAL_AUTO_JA "自動"
#define OPTION_VAL_MANUAL_JA "手動"
#define LYNX_PIX_FORMAT_LABEL_JA "色深度 (再起動が必要)"
#define OPTION_VAL_16_JA NULL
#define OPTION_VAL_32_JA NULL
#define LYNX_FORCE_60HZ_LABEL_JA NULL
#define LYNX_FORCE_60HZ_INFO_0_JA NULL

struct retro_core_option_v2_category option_cats_ja[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ja[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_JA,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_JA,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_JA },
         { "manual", OPTION_VAL_MANUAL_JA },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_JA,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_JA },
         { "32", OPTION_VAL_32_JA },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_JA,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_JA,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_ja = {
   option_cats_ja,
   option_defs_ja
};

/* RETRO_LANGUAGE_KO */

#define LYNX_ROT_SCREEN_LABEL_KO "화면 자동 회전"
#define LYNX_ROT_SCREEN_INFO_0_KO "알려진 게임에 대해 화면과 버튼 매핑 방향을 회전시킵니다. '수동'으로 설정할 경우 선택 버튼을 눌러 화면을 회전할 수 있으며, 또는 0, 90, 180, 270도의 고정된 반시계방향 회전을 선택할 수도 있습니다."
#define OPTION_VAL_AUTO_KO "자동"
#define OPTION_VAL_MANUAL_KO "수동"
#define LYNX_PIX_FORMAT_LABEL_KO "색 포맷 (재시작 필요)"
#define OPTION_VAL_16_KO "16비트 (RGB565)"
#define OPTION_VAL_32_KO "32비트 (RGB8888)"
#define LYNX_FORCE_60HZ_LABEL_KO "강제 60Hz"
#define LYNX_FORCE_60HZ_INFO_0_KO "60Hz 디스플레이에서 완벽하게 부드러운 움직임을 위해 원래 75Hz 재생률 대신 60Hz를 강제 적용"

struct retro_core_option_v2_category option_cats_ko[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ko[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_KO,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_KO,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_KO },
         { "manual", OPTION_VAL_MANUAL_KO },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_KO,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_KO },
         { "32", OPTION_VAL_32_KO },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_KO,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_KO,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_ko = {
   option_cats_ko,
   option_defs_ko
};

/* RETRO_LANGUAGE_NL */

#define LYNX_ROT_SCREEN_LABEL_NL "Scherm automatisch draaien"
#define LYNX_ROT_SCREEN_INFO_0_NL NULL
#define OPTION_VAL_AUTO_NL "Automatisch"
#define OPTION_VAL_MANUAL_NL NULL
#define LYNX_PIX_FORMAT_LABEL_NL NULL
#define OPTION_VAL_16_NL NULL
#define OPTION_VAL_32_NL NULL
#define LYNX_FORCE_60HZ_LABEL_NL NULL
#define LYNX_FORCE_60HZ_INFO_0_NL NULL

struct retro_core_option_v2_category option_cats_nl[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_nl[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_NL,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_NL,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_NL },
         { "manual", OPTION_VAL_MANUAL_NL },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_NL,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_NL },
         { "32", OPTION_VAL_32_NL },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_NL,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_NL,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_nl = {
   option_cats_nl,
   option_defs_nl
};

/* RETRO_LANGUAGE_NO */

#define LYNX_ROT_SCREEN_LABEL_NO NULL
#define LYNX_ROT_SCREEN_INFO_0_NO NULL
#define OPTION_VAL_AUTO_NO NULL
#define OPTION_VAL_MANUAL_NO NULL
#define LYNX_PIX_FORMAT_LABEL_NO NULL
#define OPTION_VAL_16_NO NULL
#define OPTION_VAL_32_NO NULL
#define LYNX_FORCE_60HZ_LABEL_NO NULL
#define LYNX_FORCE_60HZ_INFO_0_NO NULL

struct retro_core_option_v2_category option_cats_no[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_no[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_NO,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_NO,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_NO },
         { "manual", OPTION_VAL_MANUAL_NO },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_NO,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_NO },
         { "32", OPTION_VAL_32_NO },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_NO,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_NO,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_no = {
   option_cats_no,
   option_defs_no
};

/* RETRO_LANGUAGE_OR */

#define LYNX_ROT_SCREEN_LABEL_OR NULL
#define LYNX_ROT_SCREEN_INFO_0_OR NULL
#define OPTION_VAL_AUTO_OR NULL
#define OPTION_VAL_MANUAL_OR NULL
#define LYNX_PIX_FORMAT_LABEL_OR NULL
#define OPTION_VAL_16_OR NULL
#define OPTION_VAL_32_OR NULL
#define LYNX_FORCE_60HZ_LABEL_OR NULL
#define LYNX_FORCE_60HZ_INFO_0_OR NULL

struct retro_core_option_v2_category option_cats_or[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_or[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_OR,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_OR,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_OR },
         { "manual", OPTION_VAL_MANUAL_OR },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_OR,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_OR },
         { "32", OPTION_VAL_32_OR },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_OR,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_OR,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_or = {
   option_cats_or,
   option_defs_or
};

/* RETRO_LANGUAGE_PL */

#define LYNX_ROT_SCREEN_LABEL_PL "Autoobracanie ekranu"
#define LYNX_ROT_SCREEN_INFO_0_PL "Automatycznie obracaj orietancję ekranu i przypisania klawiszy dla znanych gier. Po ustawieniu na 'Ręcznie', obrót ekranu jest regulowany poprzez naciśnięcie przycisku SELECT, w przeciwnym wypadku stałą wartość obrótu można ustawić na 0, 90, 180 lub 270 stopni w kierunku przeciwnym do ruchu wskazówek zegara."
#define OPTION_VAL_AUTO_PL NULL
#define OPTION_VAL_MANUAL_PL "Ręcznie"
#define LYNX_PIX_FORMAT_LABEL_PL "Format kolorów (wymagany restart)"
#define OPTION_VAL_16_PL NULL
#define OPTION_VAL_32_PL NULL
#define LYNX_FORCE_60HZ_LABEL_PL "Wymuś 60Hz"
#define LYNX_FORCE_60HZ_INFO_0_PL "Wymuś 60Hz zamiast oryginalnej szybkości odświeżania 75Hz, dla doskonale płynnego ruchu na wyświetlaczach 60Hz"

struct retro_core_option_v2_category option_cats_pl[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_pl[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_PL,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_PL,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_PL },
         { "manual", OPTION_VAL_MANUAL_PL },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_PL,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_PL },
         { "32", OPTION_VAL_32_PL },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_PL,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_PL,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_pl = {
   option_cats_pl,
   option_defs_pl
};

/* RETRO_LANGUAGE_PT_BR */

#define LYNX_ROT_SCREEN_LABEL_PT_BR "Giro automático da tela"
#define LYNX_ROT_SCREEN_INFO_0_PT_BR "Praticamente gira automaticamente a orientação de tela e o mapeamento de botões para jogos conhecidos. Ao definir com \"Manual\", a rotação de tela é reajustada ao pressionar o botão SELECT, por outro lado, uma rotação fixa pode ser definida entre 0, 90, 180 ou 270 graus em sentido anti-horário."
#define OPTION_VAL_AUTO_PT_BR "Automático"
#define OPTION_VAL_MANUAL_PT_BR NULL
#define LYNX_PIX_FORMAT_LABEL_PT_BR "Formato de cor (requer reinício)"
#define OPTION_VAL_16_PT_BR "16 bits (RGB565)"
#define OPTION_VAL_32_PT_BR "32 bits (RGB8888)"
#define LYNX_FORCE_60HZ_LABEL_PT_BR NULL
#define LYNX_FORCE_60HZ_INFO_0_PT_BR NULL

struct retro_core_option_v2_category option_cats_pt_br[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_pt_br[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_PT_BR,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_PT_BR,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_PT_BR },
         { "manual", OPTION_VAL_MANUAL_PT_BR },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_PT_BR,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_PT_BR },
         { "32", OPTION_VAL_32_PT_BR },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_PT_BR,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_PT_BR,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_pt_br = {
   option_cats_pt_br,
   option_defs_pt_br
};

/* RETRO_LANGUAGE_PT_PT */

#define LYNX_ROT_SCREEN_LABEL_PT_PT "Rodar automaticamente o ecrã"
#define LYNX_ROT_SCREEN_INFO_0_PT_PT "Virtualmente girar a orientação da tela e mapeamentos de botões automaticamente para jogos conhecidos. Quando definido como 'Manual', a rotação da tela é ajustada pressionando o botão SELECT, caso contrário, uma rotação fixa pode ser definida para 0, 90, 180 ou 270 graus no sentido anti-horário."
#define OPTION_VAL_AUTO_PT_PT "Automático"
#define OPTION_VAL_MANUAL_PT_PT NULL
#define LYNX_PIX_FORMAT_LABEL_PT_PT "Formato de cores (reinicialização necessária)"
#define OPTION_VAL_16_PT_PT NULL
#define OPTION_VAL_32_PT_PT NULL
#define LYNX_FORCE_60HZ_LABEL_PT_PT NULL
#define LYNX_FORCE_60HZ_INFO_0_PT_PT NULL

struct retro_core_option_v2_category option_cats_pt_pt[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_pt_pt[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_PT_PT,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_PT_PT,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_PT_PT },
         { "manual", OPTION_VAL_MANUAL_PT_PT },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_PT_PT,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_PT_PT },
         { "32", OPTION_VAL_32_PT_PT },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_PT_PT,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_PT_PT,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_pt_pt = {
   option_cats_pt_pt,
   option_defs_pt_pt
};

/* RETRO_LANGUAGE_RU */

#define LYNX_ROT_SCREEN_LABEL_RU "Автоповорот экрана"
#define LYNX_ROT_SCREEN_INFO_0_RU "Автоматический поворот экрана и раскладки кнопок для определённых игр. Поворот экрана настраивается кнопкой SELECT в режиме 'Вручную' или устанавливается на фиксированное значение в 0, 90, 180 или 270 градусов против часовой стрелки."
#define OPTION_VAL_AUTO_RU "Авто"
#define OPTION_VAL_MANUAL_RU "Вручную"
#define LYNX_PIX_FORMAT_LABEL_RU "Глубина цвета (требуется перезапуск)"
#define OPTION_VAL_16_RU "16-битный (RGB565)"
#define OPTION_VAL_32_RU "32-битный (RGB8888)"
#define LYNX_FORCE_60HZ_LABEL_RU "Форсировать 60 Гц"
#define LYNX_FORCE_60HZ_INFO_0_RU "Принудительно использовать частоту обновления 60 Гц вместо оригинальной 75 Гц для идеально плавного перемещения на 60 Гц дисплеях."

struct retro_core_option_v2_category option_cats_ru[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ru[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_RU,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_RU,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_RU },
         { "manual", OPTION_VAL_MANUAL_RU },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_RU,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_RU },
         { "32", OPTION_VAL_32_RU },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_RU,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_RU,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_ru = {
   option_cats_ru,
   option_defs_ru
};

/* RETRO_LANGUAGE_SK */

#define LYNX_ROT_SCREEN_LABEL_SK NULL
#define LYNX_ROT_SCREEN_INFO_0_SK NULL
#define OPTION_VAL_AUTO_SK NULL
#define OPTION_VAL_MANUAL_SK "Ručne"
#define LYNX_PIX_FORMAT_LABEL_SK NULL
#define OPTION_VAL_16_SK NULL
#define OPTION_VAL_32_SK NULL
#define LYNX_FORCE_60HZ_LABEL_SK NULL
#define LYNX_FORCE_60HZ_INFO_0_SK NULL

struct retro_core_option_v2_category option_cats_sk[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_sk[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_SK,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_SK,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_SK },
         { "manual", OPTION_VAL_MANUAL_SK },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_SK,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_SK },
         { "32", OPTION_VAL_32_SK },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_SK,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_SK,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_sk = {
   option_cats_sk,
   option_defs_sk
};

/* RETRO_LANGUAGE_SR */

#define LYNX_ROT_SCREEN_LABEL_SR NULL
#define LYNX_ROT_SCREEN_INFO_0_SR NULL
#define OPTION_VAL_AUTO_SR NULL
#define OPTION_VAL_MANUAL_SR NULL
#define LYNX_PIX_FORMAT_LABEL_SR NULL
#define OPTION_VAL_16_SR NULL
#define OPTION_VAL_32_SR NULL
#define LYNX_FORCE_60HZ_LABEL_SR NULL
#define LYNX_FORCE_60HZ_INFO_0_SR NULL

struct retro_core_option_v2_category option_cats_sr[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_sr[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_SR,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_SR,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_SR },
         { "manual", OPTION_VAL_MANUAL_SR },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_SR,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_SR },
         { "32", OPTION_VAL_32_SR },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_SR,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_SR,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_sr = {
   option_cats_sr,
   option_defs_sr
};

/* RETRO_LANGUAGE_SV */

#define LYNX_ROT_SCREEN_LABEL_SV "Rotera skärmen automatiskt"
#define LYNX_ROT_SCREEN_INFO_0_SV "Rotera skärmens orientering och knappmappningar automatiskt virtuellt för välkända spel. När det är inställt på 'Manuell' justeras skärmens rotation genom att trycka på 'SELECT'-knappen, annars kan en fast rotation ställas in till antingen 0, 90, 180 eller 270 grader moturs."
#define OPTION_VAL_AUTO_SV "Automatiskt"
#define OPTION_VAL_MANUAL_SV "Manuell"
#define LYNX_PIX_FORMAT_LABEL_SV "Färgformat (Omstart Krävs)"
#define OPTION_VAL_16_SV "16-Bitar (RGB565)"
#define OPTION_VAL_32_SV "32-Bitar (RGB8888)"
#define LYNX_FORCE_60HZ_LABEL_SV "Tvinga 60Hz"
#define LYNX_FORCE_60HZ_INFO_0_SV "Tvinga 60Hz istället för original 75Hz uppdateringsfrekvens, för perfekt jämn rörelse på 60Hz-skärmar"

struct retro_core_option_v2_category option_cats_sv[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_sv[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_SV,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_SV,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_SV },
         { "manual", OPTION_VAL_MANUAL_SV },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_SV,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_SV },
         { "32", OPTION_VAL_32_SV },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_SV,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_SV,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_sv = {
   option_cats_sv,
   option_defs_sv
};

/* RETRO_LANGUAGE_TR */

#define LYNX_ROT_SCREEN_LABEL_TR "Ekranı Otomatik Döndür"
#define LYNX_ROT_SCREEN_INFO_0_TR "Bilinen oyunlar için ekran yönünü ve düğme eşlemelerini sanal olarak döndürün. 'El ile' olarak ayarlandığında, SELECT düğmesine basılarak ekran dönüşü ayarlanır, öyle değilse sabit bir dönüş saat yönünün tersine 0, 90, 180 veya 270 dereceye ayarlanabilir."
#define OPTION_VAL_AUTO_TR "Otomatik"
#define OPTION_VAL_MANUAL_TR "El İle"
#define LYNX_PIX_FORMAT_LABEL_TR "Renk Biçimi (Yeniden Başlatılmalı)"
#define OPTION_VAL_16_TR NULL
#define OPTION_VAL_32_TR NULL
#define LYNX_FORCE_60HZ_LABEL_TR "60Hz Olarak Zorla"
#define LYNX_FORCE_60HZ_INFO_0_TR "60 Hz ekranlarda kusursuz hareket için orijinal 75 Hz yenileme hızı yerine 60 Hz olarak zorlayın"

struct retro_core_option_v2_category option_cats_tr[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_tr[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_TR,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_TR,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_TR },
         { "manual", OPTION_VAL_MANUAL_TR },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_TR,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_TR },
         { "32", OPTION_VAL_32_TR },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_TR,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_TR,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_tr = {
   option_cats_tr,
   option_defs_tr
};

/* RETRO_LANGUAGE_UK */

#define LYNX_ROT_SCREEN_LABEL_UK "Автоповорот екрана"
#define LYNX_ROT_SCREEN_INFO_0_UK "По суті, поворот орієнтації екрану та розташування кнопок автоматично для відомих ігор. Якщо встановлено \"Вручну\", обертання екрана визначається за допомогою кнопки SELECT, інакше фіксований поворот можна встановити на 0, 90, 180 або 270 градусів проти годинникової стрілки."
#define OPTION_VAL_AUTO_UK "Авто"
#define OPTION_VAL_MANUAL_UK "Вручну"
#define LYNX_PIX_FORMAT_LABEL_UK "Формат кольору (потрібний перезапуск)"
#define OPTION_VAL_16_UK NULL
#define OPTION_VAL_32_UK NULL
#define LYNX_FORCE_60HZ_LABEL_UK NULL
#define LYNX_FORCE_60HZ_INFO_0_UK NULL

struct retro_core_option_v2_category option_cats_uk[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_uk[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_UK,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_UK,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_UK },
         { "manual", OPTION_VAL_MANUAL_UK },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_UK,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_UK },
         { "32", OPTION_VAL_32_UK },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_UK,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_UK,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_uk = {
   option_cats_uk,
   option_defs_uk
};

/* RETRO_LANGUAGE_VAL */

#define LYNX_ROT_SCREEN_LABEL_VAL "Rotació automàtica de la pantalla"
#define LYNX_ROT_SCREEN_INFO_0_VAL "Gira virtualment l'orientació de la pantalla i el mapejat dels botons automàticament per a jocs coneguts. Quan s'ajusta com a \"Manual\", la rotació de la pantalla canvia prement el botó SELECT, en cas contrari, es pot definir una rotació fixa a 0, 90, 180 o 270 graus en sentit contrari a les agulles del rellotge."
#define OPTION_VAL_AUTO_VAL "Selecció automàtica"
#define OPTION_VAL_MANUAL_VAL "Selecció manual"
#define LYNX_PIX_FORMAT_LABEL_VAL "Format de color (requerix reiniciar)"
#define OPTION_VAL_16_VAL "16 bits (RGB565)"
#define OPTION_VAL_32_VAL "32 bits (RGB8888)"
#define LYNX_FORCE_60HZ_LABEL_VAL NULL
#define LYNX_FORCE_60HZ_INFO_0_VAL NULL

struct retro_core_option_v2_category option_cats_val[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_val[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_VAL,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_VAL,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_VAL },
         { "manual", OPTION_VAL_MANUAL_VAL },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_VAL,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_VAL },
         { "32", OPTION_VAL_32_VAL },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_VAL,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_VAL,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};
struct retro_core_options_v2 options_val = {
   option_cats_val,
   option_defs_val
};

/* RETRO_LANGUAGE_VN */

#define LYNX_ROT_SCREEN_LABEL_VN "Tự động xoay màn hình"
#define LYNX_ROT_SCREEN_INFO_0_VN "Tự động xoay hướng màn hình và chọn nút cho các trò chơi đã biết. Khi được đặt thành 'Thủ công', xoay màn hình được điều chỉnh bằng cách nhấn nút CHỌN, nếu không, xoay cố định có thể được đặt thành 0, 90, 180 hoặc 270 độ ngược chiều kim đồng hồ."
#define OPTION_VAL_AUTO_VN "Tự động"
#define OPTION_VAL_MANUAL_VN "Thủ công"
#define LYNX_PIX_FORMAT_LABEL_VN "Định dạng màu (Yêu cầu khởi động lại)"
#define OPTION_VAL_16_VN "16-Bit màu (RGB565)"
#define OPTION_VAL_32_VN "32-Bit màu (RGB8888)"
#define LYNX_FORCE_60HZ_LABEL_VN NULL
#define LYNX_FORCE_60HZ_INFO_0_VN NULL

struct retro_core_option_v2_category option_cats_vn[] = {
   { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_vn[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "lynx_rot_screen",
      LYNX_ROT_SCREEN_LABEL_VN,
      NULL,
      LYNX_ROT_SCREEN_INFO_0_VN,
      NULL,
      NULL,
      {
         { "auto", OPTION_VAL_AUTO_VN },
         { "manual", OPTION_VAL_MANUAL_VN },
         { "0",    NULL },
         { "90",   NULL },
         { "180",  NULL },
         { "270",  NULL },
         { NULL, NULL},
      },
      "auto",
   },

   {
      "lynx_pix_format",
      LYNX_PIX_FORMAT_LABEL_VN,
      NULL,
      "",
      NULL,
      NULL,
      {
         { "16", OPTION_VAL_16_VN },
         { "32", OPTION_VAL_32_VN },
         { NULL, NULL},
      },
      "16",
   },

   {
	"lynx_force_60hz",
	LYNX_FORCE_60HZ_LABEL_VN,
	NULL,
	LYNX_FORCE_60HZ_INFO_0_VN,
	NULL,
	NULL,
	{
		{ "disabled", NULL },
		{ "enabled", NULL },
		{ NULL, NULL },
	},
	"disabled"
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