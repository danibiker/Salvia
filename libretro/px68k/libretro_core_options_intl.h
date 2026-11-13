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

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* needs to be translated */
struct retro_core_option_v2_category option_cats_fr[] = {
   {
      "system",
      NULL,
      NULL
   },
   {
      "video",
      NULL,
      NULL
   },
   {
      "input",
      NULL,
      NULL
   },
   {
      "p1_joy2key",
      NULL,
      NULL
   },
   {
      "p2_joy2key",
      NULL,
      NULL
   },
   {
      "audio",
      NULL,
      NULL
   },
   {
      "media",
      NULL,
      NULL
   },
   {
      "menu",
      NULL,
      NULL
   },
   {
      "advanced",
      NULL,
      NULL
   },
   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_fr[] = {
   {
      "px68k_cpu_emulation",
      "CPU Emulation",
      NULL,
      "Select the 68000 CPU emulator. Restart the core after changing this option.",
      NULL,
      "system",
      {
         { "C68K (Fast)", NULL },
         { "Musashi (Slower)", NULL },
         { NULL, NULL },
      },
      "C68K (Fast)"
   },
   {
      "px68k_cpuspeed",
      "Vitesse du CPU",
      NULL,
      "Configurez la vitesse du processeur. Peut être utilisé pour ralentir les jeux trop rapides ou pour accélérer les temps de chargement des disquettes.",
      NULL,
      "system",
      {
         { "10Mhz", NULL },
         { "16Mhz", NULL },
         { "25Mhz", NULL },
         { "33Mhz (OC)", NULL },
         { "66Mhz (OC)", NULL },
         { "100Mhz (OC)", NULL },
         { NULL, NULL },
      },
      "10Mhz"
   },
   {
      "px68k_bg_wait",
      "Attentes de la RAM BG/sprites",
      NULL,
      "Émule l'accès lent du processeur à la RAM des sprites sans modifier la vitesse du processeur. Les modifications de la table des sprites faites au milieu de l'écran (multiplexage de sprites par raster) apparaissent quelques lignes plus tard, en un seul bloc, comme sur le matériel réel. Corrige les sprites coupés dans Puyo Puyo.",
      NULL,
      "system",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_ramsize",
      "Taille de la RAM (Redémarrage requis)",
      NULL,
      "Définit la quantité de RAM à utiliser par le système.",
      NULL,
      "system",
      {
         { "1MB", NULL },
         { "2MB", NULL },
         { "3MB", NULL },
         { "4MB", NULL },
         { "5MB", NULL },
         { "6MB", NULL },
         { "7MB", NULL },
         { "8MB", NULL },
         { "9MB", NULL },
         { "10MB", NULL },
         { "11MB", NULL },
         { "12MB", NULL },
         { NULL, NULL },
      },
      "2MB"
   },
   {
      "px68k_aspect_ratio",
      "Format d'image",
      NULL,
      "Format de l'image affichée. '4:3' étire chaque image pour remplir un écran 4:3. 'X68000' reproduit ce qu'affichent un vrai X68000 et son moniteur 4:3 : la taille de l'image est calculée à partir des timings du CRTC et de l'horloge pixel, si bien que les modes qui n'utilisent qu'une partie de la ligne ou de la hauteur de l'écran (par ex. Cotton 512*256 Hi utilise 75% de la largeur) ont des bordures noires. '1:1' affiche des pixels carrés à la résolution brute du cœur.",
      NULL,
      "video",
      {
         { "4:3", "4:3 (plein écran)" },
         { "x68000", "X68000 (timings CRT)" },
         { "1:1", "1:1 (pixels carrés)" },
         { NULL, NULL },
      },
      "4:3"
   },
   {
      "px68k_adjust_frame_rates",
      "Ajuster les fréquences d'images",
      NULL,
      "Pour la compatibilité avec les écrans modernes, ajuste légèrement les fréquences d'images signalées à l'interface afin de réduire les risques de latence audio. Désactivez pour utiliser les fréquences d'images actuelles.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_frameskip",
      "Saut d'images",
      NULL,
      "Choisissez le nombre d'images à ignorer pour améliorer les performances au détriment de la fluidité visuelle.",
      NULL,
      "video",
      {
         { "Full Frame", "Toutes les images" },
         { "1/2 Frame", "1/2 image" },
         { "1/3 Frame", "1/3 image" },
         { "1/4 Frame", "1/4 image" },
         { "1/5 Frame", "1/5 image" },
         { "1/6 Frame", "1/6 image" },
         { "1/8 Frame", "1/8 image" },
         { "1/16 Frame", "1/16 image" },
         { "1/32 Frame", "1/32 image" },
         { "1/60 Frame", "1/60 image" },
         { "Auto Frame Skip", "Saut d'image automatique" },
         { NULL, NULL },
      },
      "Full Frame"
   },
   {
      "px68k_joy_mouse",
      "Joy2Mouse",
      NULL,
      "Ajoute le déplacement de la souris avec la croix directionnelle et le stick analogique gauche du joueur 1. Attribuez Mouse Btn1 et Mouse Btn2 aux commandes P1 Joy2Key pour utiliser les clics.",
      NULL,
      "input",
      {
         { "disabled", "désactivé" },
         { "enabled", "activé" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_joy2mouse_analog_speed",
      "Joy2Mouse Analog speed",
      NULL,
      "Vitesse maximale du déplacement de la souris avec le stick analogique gauche, en unités souris par image.",
      NULL,
      "input",
      {
         { "1", NULL },
         { "2", NULL },
         { "4", NULL },
         { "6", NULL },
         { "8", NULL },
         { "10", NULL },
         { "12", NULL },
         { "16", NULL },
         { "20", NULL },
         { "24", NULL },
         { NULL, NULL },
      },
      "12"
   },
   {
      "px68k_joy2mouse_analog_deadzone",
      "Joy2Mouse Analog deadzone",
      NULL,
      "Zone morte du stick analogique en pourcentage. Augmentez cette valeur si le pointeur dérive lorsque le stick est relâché.",
      NULL,
      "input",
      {
         { "0", NULL },
         { "5", NULL },
         { "10", NULL },
         { "12.5", NULL },
         { "15", NULL },
         { "20", NULL },
         { "25", NULL },
         { "30", NULL },
         { "40", NULL },
         { "50", NULL },
         { NULL, NULL },
      },
      "12.5"
   },
   {
      "px68k_joy2mouse_digital_speed",
      "Joy2Mouse Digital speed",
      NULL,
      "Vitesse de déplacement de la souris avec la croix directionnelle, en unités souris par image.",
      NULL,
      "input",
      {
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { "4", NULL },
         { "5", NULL },
         { "6", NULL },
         { "8", NULL },
         { "10", NULL },
         { "12", NULL },
         { NULL, NULL },
      },
      "6"
   },
   {
      "px68k_osd_keyboard_theme",
      "Thème du clavier à l'écran",
      NULL,
      "Style et thème de couleurs du clavier à l'écran.",
      NULL,
      "input",
      {
         { "flat_light", "Plat clair" },
         { "flat_dark", "Plat sombre" },
         { "3D_light", "3D clair" },
         { "3D_dark", "3D sombre" },
         { NULL, NULL },
      },
      "3D_dark"
   },
   {
      "px68k_osd_keyboard_labels",
      "Libellés des touches spéciales du clavier à l'écran",
      NULL,
      "Latin : les touches sont libellées KAN, ROM, COD, KIG, TOR, HIR et ZEN. Japonais : elles affichent les inscriptions d'un vrai clavier X68000.",
      NULL,
      "input",
      {
         { "latin", "Latin" },
         { "japanese", "Japonais" },
         { NULL, NULL },
      },
      "latin"
   },
   {
      "px68k_osd_keyboard_layout",
      "Disposition du clavier à l'écran",
      NULL,
      "Choisit comment le jeu est affiché lorsque le clavier à l'écran est ouvert.",
      NULL,
      "input",
      {
         { "game_above", "Jeu au-dessus du clavier" },
         { "fullscreen", "Plein écran derrière le clavier" },
         { NULL, NULL },
      },
      "fullscreen"
   },
   {
      "px68k_osd_keyboard_opacity",
      "Opacité du clavier à l'écran",
      NULL,
      "Opacité du clavier à l'écran, de 0% à 100% par pas de 5% (droite pour l'augmenter, gauche pour la diminuer). Les valeurs basses laissent voir le jeu à travers le clavier avec la disposition 'Plein écran derrière le clavier' ; avec 'Jeu au-dessus du clavier', le clavier s'assombrit vers le noir. À 0% le clavier est invisible mais reste fonctionnel.",
      NULL,
      "input",
      {
         { "0", "0%" },
         { "5", "5%" },
         { "10", "10%" },
         { "15", "15%" },
         { "20", "20%" },
         { "25", "25%" },
         { "30", "30%" },
         { "35", "35%" },
         { "40", "40%" },
         { "45", "45%" },
         { "50", "50%" },
         { "55", "55%" },
         { "60", "60%" },
         { "65", "65%" },
         { "70", "70%" },
         { "75", "75%" },
         { "80", "80%" },
         { "85", "85%" },
         { "90", "90%" },
         { "95", "95%" },
         { "100", "100%" },
         { NULL, NULL },
      },
      "100"
   },
   {
      "px68k_osd_keyboard_cursor_tint",
      "Teinte de la touche active du clavier à l'écran",
      NULL,
      "Couleur de la face de la touche sélectionnée.",
      NULL,
      "input",
      {
         { "off", "Désactivé" },
         { "ffffff", "Blanc" },
         { "a0a0a0", "Gris" },
         { "000000", "Noir" },
         { "ff9999", "Rouge (pâle)" },
         { "ff0000", "Rouge (vif)" },
         { "8e0b0b", "Rouge (foncé)" },
         { "ffc999", "Orange (pâle)" },
         { "ff7700", "Orange (vif)" },
         { "8e480b", "Orange (foncé)" },
         { "fff199", "Jaune (pâle)" },
         { "ffdd00", "Jaune (vif)" },
         { "8e7c0b", "Jaune (foncé)" },
         { "99ff99", "Vert (pâle)" },
         { "00ff00", "Vert (vif)" },
         { "0b8e0b", "Vert (foncé)" },
         { "99ffff", "Cyan (pâle)" },
         { "00ffff", "Cyan (vif)" },
         { "0b8e8e", "Cyan (foncé)" },
         { "99b8ff", "Bleu (pâle)" },
         { "004cff", "Bleu (vif)" },
         { "0b328e", "Bleu (foncé)" },
         { "cf99ff", "Violet (pâle)" },
         { "8800ff", "Violet (vif)" },
         { "510b8e", "Violet (foncé)" },
         { "ff99e0", "Magenta (pâle)" },
         { "ff00b2", "Magenta (vif)" },
         { "8e0b67", "Magenta (foncé)" },
         { NULL, NULL },
      },
      "fff199"
   },
   {
      "px68k_osd_keyboard_cursor_frame",
      "Cadre de sélection du clavier à l'écran",
      NULL,
      "Couleur du carré dessiné autour de la touche sélectionnée.",
      NULL,
      "input",
      {
         { "off", "Désactivé" },
         { "ffffff", "Blanc" },
         { "a0a0a0", "Gris" },
         { "000000", "Noir" },
         { "ff9999", "Rouge (pâle)" },
         { "ff0000", "Rouge (vif)" },
         { "8e0b0b", "Rouge (foncé)" },
         { "ffc999", "Orange (pâle)" },
         { "ff7700", "Orange (vif)" },
         { "8e480b", "Orange (foncé)" },
         { "fff199", "Jaune (pâle)" },
         { "ffdd00", "Jaune (vif)" },
         { "8e7c0b", "Jaune (foncé)" },
         { "99ff99", "Vert (pâle)" },
         { "00ff00", "Vert (vif)" },
         { "0b8e0b", "Vert (foncé)" },
         { "99ffff", "Cyan (pâle)" },
         { "00ffff", "Cyan (vif)" },
         { "0b8e8e", "Cyan (foncé)" },
         { "99b8ff", "Bleu (pâle)" },
         { "004cff", "Bleu (vif)" },
         { "0b328e", "Bleu (foncé)" },
         { "cf99ff", "Violet (pâle)" },
         { "8800ff", "Violet (vif)" },
         { "510b8e", "Violet (foncé)" },
         { "ff99e0", "Magenta (pâle)" },
         { "ff00b2", "Magenta (vif)" },
         { "8e0b67", "Magenta (foncé)" },
         { NULL, NULL },
      },
      "ffffff"
   },
   {
      "px68k_osd_keyboard_hover",
      "Survol à la souris du clavier à l'écran",
      NULL,
      "Lorsque cette option est activée, la touche sélectionnée suit le curseur de la souris quand il passe sur le clavier à l'écran. La sélection ne change que si la souris bouge réellement : elle n'entre donc jamais en conflit avec la navigation à la manette.",
      NULL,
      "input",
      {
         { "enabled", "Activé" },
         { "disabled", "Désactivé" },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_osd_keyboard_feedback",
      "Retour visuel des appuis du clavier à l'écran",
      NULL,
      "Réaction visuelle quand une touche est touchée, cliquée ou pressée depuis la manette. Flash : la touche s'allume puis s'estompe après le relâchement, si bien que même un appui bref est visible. Flash + enfoncement : la touche s'enfonce aussi pendant l'appui.",
      NULL,
      "input",
      {
         { "off", "Désactivé" },
         { "flash", "Flash" },
         { "full", "Flash + enfoncement" },
         { NULL, NULL },
      },
      "full"
   },
   {
      "px68k_key_repeat_delay",
      "Délai de répétition des touches",
      NULL,
      "Durée pendant laquelle une touche doit être maintenue avant de se répéter. 'Machine' suit la valeur que le logiciel X68000 programme dans le clavier (500 ms par défaut).",
      NULL,
      "input",
      {
         { "machine", "Machine (réglage X68000)" },
         { "off", "Désactivé (pas de répétition)" },
         { "200", "200 ms" },
         { "300", "300 ms" },
         { "400", "400 ms" },
         { "500", "500 ms" },
         { "700", "700 ms" },
         { "1000", "1000 ms" },
         { NULL, NULL },
      },
      "machine"
   },
   {
      "px68k_key_repeat_interval",
      "Intervalle de répétition des touches",
      NULL,
      "Durée entre deux répétitions une fois qu'une touche maintenue se répète. 'Machine' suit le réglage du X68000 (110 ms par défaut).",
      NULL,
      "input",
      {
         { "machine", "Machine (réglage X68000)" },
         { "30", "30 ms" },
         { "50", "50 ms" },
         { "75", "75 ms" },
         { "100", "100 ms" },
         { "150", "150 ms" },
         { "200", "200 ms" },
         { "300", "300 ms" },
         { NULL, NULL },
      },
      "machine"
   },
   {
      "px68k_j2k_p1_up",
      "P1 Joy2Key UP",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "up"
   },
   {
      "px68k_j2k_p1_down",
      "P1 Joy2Key Down",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "down"
   },
   {
      "px68k_j2k_p1_left",
      "P1 Joy2Key Left",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "left"
   },
   {
      "px68k_j2k_p1_right",
      "P1 Joy2Key Right",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "right"
   },
   {
      "px68k_j2k_p1_select",
      "P1 Joy2Key Select",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "esc"
   },
   {
      "px68k_j2k_p1_start",
      "P1 Joy2Key Start",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "xf1"
   },
   {
      "px68k_j2k_p1_a",
      "P1 Joy2Key A",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p1_b",
      "P1 Joy2Key B",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p1_x",
      "P1 Joy2Key X",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p1_y",
      "P1 Joy2Key Y",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p1_l",
      "P1 Joy2Key L",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "opt1"
   },
   {
      "px68k_j2k_p1_r",
      "P1 Joy2Key R",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "opt2"
   },
   {
      "px68k_j2k_p1_l2",
      "P1 Joy2Key L2",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "menu"
   },
   {
      "px68k_j2k_p1_r2",
      "P1 Joy2Key R2",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "osd_keyboard"
   },
   {
      "px68k_j2k_p1_l3",
      "P1 Joy2Key L3",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "mouse_btn1"
   },
   {
      "px68k_j2k_p1_r3",
      "P1 Joy2Key R3",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "mouse_btn2"
   },
   {
      "px68k_j2k_p2_up",
      "P2 Joy2Key UP",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "up"
   },
   {
      "px68k_j2k_p2_down",
      "P2 Joy2Key Down",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "down"
   },
   {
      "px68k_j2k_p2_left",
      "P2 Joy2Key Left",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "left"
   },
   {
      "px68k_j2k_p2_right",
      "P2 Joy2Key Right",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "right"
   },
   {
      "px68k_j2k_p2_select",
      "P2 Joy2Key Select",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "esc"
   },
   {
      "px68k_j2k_p2_start",
      "P2 Joy2Key Start",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "xf1"
   },
   {
      "px68k_j2k_p2_a",
      "P2 Joy2Key A",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p2_b",
      "P2 Joy2Key B",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p2_x",
      "P2 Joy2Key X",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p2_y",
      "P2 Joy2Key Y",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p2_l",
      "P2 Joy2Key L",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "opt1"
   },
   {
      "px68k_j2k_p2_r",
      "P2 Joy2Key R",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "opt2"
   },
   {
      "px68k_j2k_p2_l2",
      "P2 Joy2Key L2",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "menu"
   },
   {
      "px68k_j2k_p2_r2",
      "P2 Joy2Key R2",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "space"
   },
   {
      "px68k_j2k_p2_l3",
      "P2 Joy2Key L3",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "touroku"
   },
   {
      "px68k_j2k_p2_r3",
      "P2 Joy2Key R3",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "shift"
   },
   {
      "px68k_midi_emulation",
      "Internal MIDI Synthesizer",
      NULL,
      "Select the internal MIDI synthesizer instead of RetroArch MIDI output. SC-55 uses Nuked-SC55 ROMs; MT-32 and CM-32L use compatible Munt ROMs in the System Directory or recognized subfolders.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "sc55", "Roland SC-55" },
         { "mt32", "Roland MT-32" },
         { "cm32l", "Roland CM-32L" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_midiemu_volume",
      "MIDI Emulation Volume",
      NULL,
      "Adjust the volume of the active internal MIDI synthesizer relative to PX68K audio. 100% is the default level.",
      NULL,
      "audio",
      {
         { "0", "0%" },
         { "5", "5%" },
         { "10", "10%" },
         { "15", "15%" },
         { "20", "20%" },
         { "25", "25%" },
         { "30", "30%" },
         { "35", "35%" },
         { "40", "40%" },
         { "45", "45%" },
         { "50", "50%" },
         { "55", "55%" },
         { "60", "60%" },
         { "65", "65%" },
         { "70", "70%" },
         { "75", "75%" },
         { "80", "80%" },
         { "85", "85%" },
         { "90", "90%" },
         { "95", "95%" },
         { "100", "100%" },
         { "105", "105%" },
         { "110", "110%" },
         { "115", "115%" },
         { "120", "120%" },
         { "125", "125%" },
         { "130", "130%" },
         { "135", "135%" },
         { "140", "140%" },
         { "145", "145%" },
         { "150", "150%" },
         { "155", "155%" },
         { "160", "160%" },
         { "165", "165%" },
         { "170", "170%" },
         { "175", "175%" },
         { "180", "180%" },
         { "185", "185%" },
         { "190", "190%" },
         { "195", "195%" },
         { "200", "200%" },
         { NULL, NULL },
      },
      "100"
   },
   {
      "px68k_midi_output",
      "MIDI Output (Restart)",
      NULL,
      NULL,
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_midi_state_restore",
      "Restauration MIDI au chargement d'état",
      NULL,
      "Quand un état est chargé, le synthétiseur MIDI conserve ce qu'on lui a envoyé en dernier. Choisissez ce qu'il faut restaurer pour que la musique sonne comme au moment de la sauvegarde. 'Configuration du synthé' renvoie le standard sonore (LA/GM/GS/XG), les paramètres SysEx et, pour chaque canal, la banque, le programme, le volume, le panoramique, les effets et le pitch bend. 'Configuration et sortie' revient aussi au synthétiseur interne ou à la sortie MIDI externe utilisés à ce moment-là, s'ils sont disponibles. Sans effet sur le Run-Ahead.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "setup", "Configuration du synthé" },
         { "setup_output", "Configuration et sortie" },
         { NULL, NULL },
      },
      "setup_output"
   },
   {
      "px68k_midi_output_type",
      "MIDI Output Type (Restart)",
      NULL,
      NULL,
      NULL,
      "audio",
      {
         { "LA", NULL },
         { "GM", NULL },
         { "GS", NULL },
         { "XG", NULL },
         { NULL, NULL },
      },
      "GM"
   },
   {
      "px68k_adpcm_lpf",
      "Filtre passe-bas ADPCM",
      NULL,
      "Active le filtre passe-bas ADPCM original. Le changement est immédiat, sans redémarrage du cœur.",
      NULL,
      "audio",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_adpcm_vol",
      "Volume ADPCM",
      NULL,
      "Règlage du volume du canal audio ADPCM.",
      NULL,
      "audio",
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
         { "9", NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { NULL, NULL },
      },
      "15"
   },
   {
      "px68k_opm_vol",
      "Volume OPM",
      NULL,
      "Règlage du volume du canal audio OPM.",
      NULL,
      "audio",
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
         { "9", NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { NULL, NULL },
      },
      "12"
   },
   {
      "px68k_mercury_vol",
      "Volume Mercury",
      NULL,
      "Règlage du volume du canal sonore Mercury.",
      NULL,
      "audio",
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
         { "9", NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { NULL, NULL },
      },
      "13"
   },
   {
      "px68k_mame_names",
      "Noms de la liste de logiciels MAME",
      NULL,
      "Affiche dans l'explorateur interne les descriptions anglaises ou les libellés japonais alt_title du fichier MAME x68k_flop.xml pour les fichiers ZIP dont le nom correspond. Nécessite MAME/hash/x68k_flop.xml dans le répertoire système.",
      NULL,
      "menu",
      {
         { "disabled", NULL },
         { "english", "Anglais" },
         { "japanese", "Japonais" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_archive_floppies",
      "Insertion automatique des disquettes archivées",
      NULL,
      "Contrôle l'insertion automatique d'une autre disquette d'une même archive ZIP au démarrage ou lors d'un changement de disquette.",
      NULL,
      "media",
      {
         { "Off", "Désactivé" },
         { "Always", "Toujours" },
         { "Boot Only", "Démarrage uniquement" },
         { "At Disk Change", "Au changement de disquette" },
         { NULL, NULL },
      },
      "Off"
   },
   {
      "px68k_disk_drive",
      "Échange de disques sur le lecteur",
      NULL,
      "Par défaut, l'interface native de RetroArch, d'échange de disque dans le menu, échange le disque dans le lecteur FDD1. Modifiez cette option pour échanger des disques dans le lecteur FDD0.",
      NULL,
      "media",
      {
         { "FDD1", NULL },
         { "FDD0", NULL },
         { NULL, NULL },
      },
      "FDD1"
   },
   {
      "px68k_save_fdd_path",
      "Enregistrer les chemins d'accès aux disquettes",
      NULL,
      "Lorsqu'elle est activée, les chemins d'accès aux disquettes précédemment chargés seront enregistrés pour chaque lecteur, puis chargés automatiquement au démarrage. Lorsqu'elle est désactivé, FDDx démarre à vide.",
      NULL,
      "media",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_save_hdd_path",
      "Enregistrer les chemins d'accès aux disques durs",
      NULL,
      "Lorsqu'elle est activée, les chemins d'accès aux disques durs précédemment chargés seront enregistrés pour chaque disque dur puis chargés automatiquement au démarrage. Lorsqu'elle est désactivée, HDDx démarre à vide.",
      NULL,
      "media",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_save_sram",
      "Sauvegarder la SRAM (NVRAM de la machine)",
      NULL,
      "Lorsque cette option est activée, la SRAM de la machine (périphérique de démarrage, horloge, répétition du clavier et autres paramètres SWITCH.X) est enregistrée dans le fichier sram.dat et rechargée au démarrage. Lorsqu'elle est désactivée, la machine démarre toujours avec une SRAM vierge, réinitialisée par l'IPL ; utile si un fichier sram.dat obsolète est à l'origine de problèmes.",
      NULL,
      "media",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_rumble_on_disk_read",
      "Faire vibrer la manette pendant la lecture des disquettes",
      NULL,
      "Produit un effet de vibration par les manettes supportées pendant la lecture des disquettes.",
      NULL,
      "media",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_menufontsize",
      "Taille de la police du menu",
      NULL,
      NULL,
      NULL,
      "menu",
      {
         { "normal", "Normale" },
         { "large", "Grande" },
         { NULL, NULL },
      },
      "normal"
   },
   {
      "px68k_menu_lines_jump",
      "Saut de lignes (gauche-droite)",
      NULL,
      "Nombre d'entrées sautées en appuyant sur Gauche ou Droite dans le menu intégré.",
      NULL,
      "menu",
      {
         { "5", NULL },
         { "10", NULL },
         { "15", NULL },
         { "20", NULL },
         { "25", NULL },
         { "30", NULL },
         { "35", NULL },
         { "40", NULL },
         { "45", NULL },
         { "50", NULL },
         { NULL, NULL },
      },
      "10"
   },
   {
      "px68k_text_off",
      "Text Off",
      NULL,
      "TODO:",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_grp_off",
      "Grp Off",
      NULL,
      "TODO:",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_sp_off",
      "SP/BG Off",
      NULL,
      "TODO:",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, { 0, 0 }, NULL },
};

struct retro_core_options_v2 options_fr = {
   option_cats_fr,
   option_defs_fr
};

/* RETRO_LANGUAGE_SPANISH */

struct retro_core_option_v2_category option_cats_es[] = {
   {
      "system",
      "Sistema",
      NULL
   },
   {
      "video",
      "Vídeo",
      NULL
   },
   {
      "input",
      "Entrada",
      NULL
   },
   {
      "p1_joy2key",
      "J1 Joy2Key",
      NULL
   },
   {
      "p2_joy2key",
      "J2 Joy2Key",
      NULL
   },
   {
      "audio",
      "Audio",
      NULL
   },
   {
      "media",
      "Soportes",
      NULL
   },
   {
      "menu",
      "Menú",
      "NULL"
   },
   {
      "advanced",
      "Avanzado",
      NULL
   },
   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_es[] = {
   {
      "px68k_cpu_emulation",
      "Emulación de CPU",
      NULL,
      "Seleccione el emulador de CPU 68000. Reinicie el núcleo después de cambiar esta opción.",
      NULL,
      "system",
      {
         { "C68K (Fast)", "C68K (Rápido)" },
         { "Musashi (Slower)", "Musashi (Lento)" },
         { NULL, NULL },
      },
      "C68K (Fast)"
   },
   {
      "px68k_cpuspeed",
      "Velocidad de la CPU",
      NULL,
      "Configura la velocidad del procesador. Puede usarse para ralentizar juegos demasiado rápidos o para acelerar los tiempos de carga de los disquetes.",
      NULL,
      "system",
      {
         { "10Mhz", NULL },
         { "16Mhz", NULL },
         { "25Mhz", NULL },
         { "33Mhz (OC)", NULL },
         { "66Mhz (OC)", NULL },
         { "100Mhz (OC)", NULL },
         { NULL, NULL },
      },
      "10Mhz"
   },
   {
      "px68k_bg_wait",
      "Estados de espera de la RAM de fondos/sprites",
      NULL,
      "Emula el acceso lento de la CPU a la memoria RAM de los sprites sin modificar la velocidad de la CPU. Los cambios en la tabla de sprites realizados a mitad de la pantalla (multiplexación de sprites por barrido) se reflejan unas líneas más abajo, como un bloque único, tal como ocurre en el hardware real. Corrige el problema de los sprites que aparecen recortados en Puyo Puyo.",
      NULL,
      "system",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_ramsize",
      "Tamaño de RAM (requiere reinicio)",
      NULL,
      "Define la cantidad de RAM que usará el sistema.",
      NULL,
      "system",
      {
         { "1MB", NULL },
         { "2MB", NULL },
         { "3MB", NULL },
         { "4MB", NULL },
         { "5MB", NULL },
         { "6MB", NULL },
         { "7MB", NULL },
         { "8MB", NULL },
         { "9MB", NULL },
         { "10MB", NULL },
         { "11MB", NULL },
         { "12MB", NULL },
         { NULL, NULL },
      },
      "2MB"
   },
   {
      "px68k_aspect_ratio",
      "Relación de aspecto",
      NULL,
      "Relación de aspecto de la imagen mostrada. '4:3' estira cada imagen para llenar una pantalla 4:3. 'X68000' reproduce lo que muestran un X68000 real y su monitor 4:3: el tamaño de la imagen se calcula a partir de los tiempos del CRTC y del reloj de píxel, así que los modos que solo usan parte de la línea o de la altura de la pantalla (p. ej. Cotton 512*256 Hi usa el 75% del ancho) tienen bordes negros. '1:1' muestra píxeles cuadrados a la resolución nativa del núcleo.",
      NULL,
      "video",
      {
         { "4:3", "4:3 (llenar pantalla)" },
         { "x68000", "X68000 (tiempos CRT)" },
         { "1:1", "1:1 (píxeles cuadrados)" },
         { NULL, NULL },
      },
      "4:3"
   },
   {
      "px68k_adjust_frame_rates",
      "Ajustar frecuencias de fotogramas",
      NULL,
      "Para compatibilidad con pantallas modernas, ajusta ligeramente las frecuencias de fotogramas comunicadas al frontend para reducir el riesgo de latencia de audio. Desactívalo para usar las frecuencias reales.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_frameskip",
      "Salto de fotogramas",
      NULL,
      "Elige cuántos fotogramas omitir para mejorar el rendimiento a costa de la fluidez visual.",
      NULL,
      "video",
      {
         { "Full Frame", "Todos los fotogramas" },
         { "1/2 Frame", "1/2 fotograma" },
         { "1/3 Frame", "1/3 fotograma" },
         { "1/4 Frame", "1/4 fotograma" },
         { "1/5 Frame", "1/5 fotograma" },
         { "1/6 Frame", "1/6 fotograma" },
         { "1/8 Frame", "1/8 fotograma" },
         { "1/16 Frame", "1/16 fotograma" },
         { "1/32 Frame", "1/32 fotograma" },
         { "1/60 Frame", "1/60 fotograma" },
         { "Auto Frame Skip", "Salto automático" },
         { NULL, NULL },
      },
      "Full Frame"
   },
   {
      "px68k_joy_mouse",
      "Joy2Mouse",
      NULL,
      "Mueve el ratón con la cruceta y el stick analógico izquierdo del jugador 1. Asigna Mouse Btn1 y Mouse Btn2 a entradas Joy2Key del jugador 1 para usarlos como clics del ratón.",
      NULL,
      "input",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_joy2mouse_analog_speed",
      "Joy2Mouse: velocidad analógica",
      NULL,
      "Velocidad máxima del ratón con el stick analógico izquierdo, en unidades de ratón por fotograma.",
      NULL,
      "input",
      {
         { "1", NULL },
         { "2", NULL },
         { "4", NULL },
         { "6", NULL },
         { "8", NULL },
         { "10", NULL },
         { "12", NULL },
         { "16", NULL },
         { "20", NULL },
         { "24", NULL },
         { NULL, NULL },
      },
      "12"
   },
   {
      "px68k_joy2mouse_analog_deadzone",
      "Joy2Mouse: zona muerta analógica",
      NULL,
      "Porcentaje de zona muerta del stick analógico. Auméntalo si el puntero se desplaza solo al soltar el stick.",
      NULL,
      "input",
      {
         { "0", NULL },
         { "5", NULL },
         { "10", NULL },
         { "12.5", NULL },
         { "15", NULL },
         { "20", NULL },
         { "25", NULL },
         { "30", NULL },
         { "40", NULL },
         { "50", NULL },
         { NULL, NULL },
      },
      "12.5"
   },
   {
      "px68k_joy2mouse_digital_speed",
      "Joy2Mouse: velocidad digital",
      NULL,
      "Velocidad del ratón con la cruceta, en unidades de ratón por fotograma.",
      NULL,
      "input",
      {
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { "4", NULL },
         { "5", NULL },
         { "6", NULL },
         { "8", NULL },
         { "10", NULL },
         { "12", NULL },
         { NULL, NULL },
      },
      "6"
   },
   {
      "px68k_osd_keyboard_theme",
      "Tema del teclado en pantalla",
      NULL,
      "Estilo y tema de color del teclado en pantalla.",
      NULL,
      "input",
      {
         { "flat_light", "Plano claro" },
         { "flat_dark", "Plano oscuro" },
         { "3D_light", "3D claro" },
         { "3D_dark", "3D oscuro" },
         { NULL, NULL },
      },
      "3D_dark"
   },
   {
      "px68k_osd_keyboard_labels",
      "Rótulos de teclas especiales del teclado en pantalla",
      NULL,
      "Latino: las teclas se rotulan KAN, ROM, COD, KIG, TOR, HIR y ZEN. Japonés: muestran las inscripciones de un teclado X68000 real.",
      NULL,
      "input",
      {
         { "latin", "Latino" },
         { "japanese", "Japonés" },
         { NULL, NULL },
      },
      "latin"
   },
   {
      "px68k_osd_keyboard_layout",
      "Disposición del teclado en pantalla",
      NULL,
      "Elige cómo se muestra el juego mientras el teclado en pantalla está abierto.",
      NULL,
      "input",
      {
         { "game_above", "Juego sobre el teclado" },
         { "fullscreen", "Pantalla completa tras el teclado" },
         { NULL, NULL },
      },
      "fullscreen"
   },
   {
      "px68k_osd_keyboard_opacity",
      "Opacidad del teclado en pantalla",
      NULL,
      "Opacidad del teclado en pantalla, del 0% al 100% en pasos de 5% (derecha la sube, izquierda la baja). Con valores bajos el juego se ve a través del teclado cuando la disposición es 'Pantalla completa tras el teclado'; con 'Juego sobre el teclado' el teclado se funde hacia negro. Al 0% el teclado es invisible pero sigue funcionando.",
      NULL,
      "input",
      {
         { "0", "0%" },
         { "5", "5%" },
         { "10", "10%" },
         { "15", "15%" },
         { "20", "20%" },
         { "25", "25%" },
         { "30", "30%" },
         { "35", "35%" },
         { "40", "40%" },
         { "45", "45%" },
         { "50", "50%" },
         { "55", "55%" },
         { "60", "60%" },
         { "65", "65%" },
         { "70", "70%" },
         { "75", "75%" },
         { "80", "80%" },
         { "85", "85%" },
         { "90", "90%" },
         { "95", "95%" },
         { "100", "100%" },
         { NULL, NULL },
      },
      "100"
   },
   {
      "px68k_osd_keyboard_cursor_tint",
      "Tinte de la tecla activa del teclado en pantalla",
      NULL,
      "Color de la cara de la tecla seleccionada.",
      NULL,
      "input",
      {
         { "off", "Desactivado" },
         { "ffffff", "Blanco" },
         { "a0a0a0", "Gris" },
         { "000000", "Negro" },
         { "ff9999", "Rojo (pálido)" },
         { "ff0000", "Rojo (vivo)" },
         { "8e0b0b", "Rojo (oscuro)" },
         { "ffc999", "Naranja (pálido)" },
         { "ff7700", "Naranja (vivo)" },
         { "8e480b", "Naranja (oscuro)" },
         { "fff199", "Amarillo (pálido)" },
         { "ffdd00", "Amarillo (vivo)" },
         { "8e7c0b", "Amarillo (oscuro)" },
         { "99ff99", "Verde (pálido)" },
         { "00ff00", "Verde (vivo)" },
         { "0b8e0b", "Verde (oscuro)" },
         { "99ffff", "Cian (pálido)" },
         { "00ffff", "Cian (vivo)" },
         { "0b8e8e", "Cian (oscuro)" },
         { "99b8ff", "Azul (pálido)" },
         { "004cff", "Azul (vivo)" },
         { "0b328e", "Azul (oscuro)" },
         { "cf99ff", "Morado (pálido)" },
         { "8800ff", "Morado (vivo)" },
         { "510b8e", "Morado (oscuro)" },
         { "ff99e0", "Magenta (pálido)" },
         { "ff00b2", "Magenta (vivo)" },
         { "8e0b67", "Magenta (oscuro)" },
         { NULL, NULL },
      },
      "fff199"
   },
   {
      "px68k_osd_keyboard_cursor_frame",
      "Marco de selección del teclado en pantalla",
      NULL,
      "Color del cuadro dibujado alrededor de la tecla seleccionada.",
      NULL,
      "input",
      {
         { "off", "Desactivado" },
         { "ffffff", "Blanco" },
         { "a0a0a0", "Gris" },
         { "000000", "Negro" },
         { "ff9999", "Rojo (pálido)" },
         { "ff0000", "Rojo (vivo)" },
         { "8e0b0b", "Rojo (oscuro)" },
         { "ffc999", "Naranja (pálido)" },
         { "ff7700", "Naranja (vivo)" },
         { "8e480b", "Naranja (oscuro)" },
         { "fff199", "Amarillo (pálido)" },
         { "ffdd00", "Amarillo (vivo)" },
         { "8e7c0b", "Amarillo (oscuro)" },
         { "99ff99", "Verde (pálido)" },
         { "00ff00", "Verde (vivo)" },
         { "0b8e0b", "Verde (oscuro)" },
         { "99ffff", "Cian (pálido)" },
         { "00ffff", "Cian (vivo)" },
         { "0b8e8e", "Cian (oscuro)" },
         { "99b8ff", "Azul (pálido)" },
         { "004cff", "Azul (vivo)" },
         { "0b328e", "Azul (oscuro)" },
         { "cf99ff", "Morado (pálido)" },
         { "8800ff", "Morado (vivo)" },
         { "510b8e", "Morado (oscuro)" },
         { "ff99e0", "Magenta (pálido)" },
         { "ff00b2", "Magenta (vivo)" },
         { "8e0b67", "Magenta (oscuro)" },
         { NULL, NULL },
      },
      "ffffff"
   },
   {
      "px68k_osd_keyboard_hover",
      "Seguimiento del ratón en el teclado en pantalla",
      NULL,
      "Si está activado, la tecla seleccionada sigue al cursor del ratón cuando pasa sobre el teclado en pantalla. La selección solo cambia cuando el ratón se mueve de verdad, así que nunca interfiere con la navegación con el mando.",
      NULL,
      "input",
      {
         { "enabled", "Activado" },
         { "disabled", "Desactivado" },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_osd_keyboard_feedback",
      "Respuesta visual al pulsar en el teclado en pantalla",
      NULL,
      "Reacción visual cuando una tecla se toca, se pulsa con el ratón o desde el mando. Destello: la tecla se ilumina y se apaga poco a poco al soltarla, así que incluso una pulsación rápida se ve. Destello + pulsación: además, la tecla se hunde mientras está pulsada.",
      NULL,
      "input",
      {
         { "off", "Desactivado" },
         { "flash", "Destello" },
         { "full", "Destello + pulsación" },
         { NULL, NULL },
      },
      "full"
   },
   {
      "px68k_key_repeat_delay",
      "Retardo de repetición de teclas",
      NULL,
      "Tiempo que hay que mantener pulsada una tecla antes de que empiece a repetirse. 'Máquina' sigue lo que el software del X68000 programa en el teclado (500 ms por defecto).",
      NULL,
      "input",
      {
         { "machine", "Máquina (ajuste del X68000)" },
         { "off", "Desactivado (sin repetición)" },
         { "200", "200 ms" },
         { "300", "300 ms" },
         { "400", "400 ms" },
         { "500", "500 ms" },
         { "700", "700 ms" },
         { "1000", "1000 ms" },
         { NULL, NULL },
      },
      "machine"
   },
   {
      "px68k_key_repeat_interval",
      "Intervalo de repetición de teclas",
      NULL,
      "Tiempo entre repeticiones mientras una tecla mantenida se repite. 'Máquina' sigue el ajuste del X68000 (110 ms por defecto).",
      NULL,
      "input",
      {
         { "machine", "Máquina (ajuste del X68000)" },
         { "30", "30 ms" },
         { "50", "50 ms" },
         { "75", "75 ms" },
         { "100", "100 ms" },
         { "150", "150 ms" },
         { "200", "200 ms" },
         { "300", "300 ms" },
         { NULL, NULL },
      },
      "machine"
   },
   {
      "px68k_j2k_p1_up",
      "J1 Joy2Key: Arriba",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "up"
   },
   {
      "px68k_j2k_p1_down",
      "J1 Joy2Key: Abajo",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "down"
   },
   {
      "px68k_j2k_p1_left",
      "J1 Joy2Key: Izquierda",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "left"
   },
   {
      "px68k_j2k_p1_right",
      "J1 Joy2Key: Derecha",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "right"
   },
   {
      "px68k_j2k_p1_select",
      "J1 Joy2Key: Select",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "esc"
   },
   {
      "px68k_j2k_p1_start",
      "J1 Joy2Key: Start",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "xf1"
   },
   {
      "px68k_j2k_p1_a",
      "J1 Joy2Key: A",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p1_b",
      "J1 Joy2Key: B",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p1_x",
      "J1 Joy2Key: X",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p1_y",
      "J1 Joy2Key: Y",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p1_l",
      "J1 Joy2Key: L",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "opt1"
   },
   {
      "px68k_j2k_p1_r",
      "J1 Joy2Key: R",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "opt2"
   },
   {
      "px68k_j2k_p1_l2",
      "J1 Joy2Key: L2",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "menu"
   },
   {
      "px68k_j2k_p1_r2",
      "J1 Joy2Key: R2",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "osd_keyboard"
   },
   {
      "px68k_j2k_p1_l3",
      "J1 Joy2Key: L3",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "mouse_btn1"
   },
   {
      "px68k_j2k_p1_r3",
      "J1 Joy2Key: R3",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "mouse_btn2"
   },
   {
      "px68k_j2k_p2_up",
      "J2 Joy2Key: Arriba",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "up"
   },
   {
      "px68k_j2k_p2_down",
      "J2 Joy2Key: Abajo",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "down"
   },
   {
      "px68k_j2k_p2_left",
      "J2 Joy2Key: Izquierda",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "left"
   },
   {
      "px68k_j2k_p2_right",
      "J2 Joy2Key: Derecha",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "right"
   },
   {
      "px68k_j2k_p2_select",
      "J2 Joy2Key: Select",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "esc"
   },
   {
      "px68k_j2k_p2_start",
      "J2 Joy2Key: Start",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "xf1"
   },
   {
      "px68k_j2k_p2_a",
      "J2 Joy2Key: A",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p2_b",
      "J2 Joy2Key: B",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p2_x",
      "J2 Joy2Key: X",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p2_y",
      "J2 Joy2Key: Y",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p2_l",
      "J2 Joy2Key: L",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "opt1"
   },
   {
      "px68k_j2k_p2_r",
      "J2 Joy2Key: R",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "opt2"
   },
   {
      "px68k_j2k_p2_l2",
      "J2 Joy2Key: L2",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "menu"
   },
   {
      "px68k_j2k_p2_r2",
      "J2 Joy2Key: R2",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "space"
   },
   {
      "px68k_j2k_p2_l3",
      "J2 Joy2Key: L3",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "touroku"
   },
   {
      "px68k_j2k_p2_r3",
      "J2 Joy2Key: R3",
      NULL,
      "Selecciona la tecla del teclado del X68000 que se envía cuando esta acción lógica de Joy2Key se activa en los Controles de RetroArch.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "shift"
   },
   {
      "px68k_midi_emulation",
      "Sintetizador MIDI interno",
      NULL,
      "Selecciona el sintetizador MIDI interno en lugar de la salida MIDI de RetroArch. El SC-55 utiliza ROMs de Nuked-SC55; el MT-32 y el CM-32L emplean ROMs compatibles de Munt, ubicadas en el directorio del sistema o en subcarpetas reconocidas.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "sc55", "Roland SC-55" },
         { "mt32", "Roland MT-32" },
         { "cm32l", "Roland CM-32L" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_midiemu_volume",
      "Volumen de la emulación MIDI",
      NULL,
      "Ajusta el volumen del sintetizador MIDI interno activo en relación con el audio del PX68K. El nivel predeterminado es 100 %.",
      NULL,
      "audio",
      {
         { "0", "0%" },
         { "5", "5%" },
         { "10", "10%" },
         { "15", "15%" },
         { "20", "20%" },
         { "25", "25%" },
         { "30", "30%" },
         { "35", "35%" },
         { "40", "40%" },
         { "45", "45%" },
         { "50", "50%" },
         { "55", "55%" },
         { "60", "60%" },
         { "65", "65%" },
         { "70", "70%" },
         { "75", "75%" },
         { "80", "80%" },
         { "85", "85%" },
         { "90", "90%" },
         { "95", "95%" },
         { "100", "100%" },
         { "105", "105%" },
         { "110", "110%" },
         { "115", "115%" },
         { "120", "120%" },
         { "125", "125%" },
         { "130", "130%" },
         { "135", "135%" },
         { "140", "140%" },
         { "145", "145%" },
         { "150", "150%" },
         { "155", "155%" },
         { "160", "160%" },
         { "165", "165%" },
         { "170", "170%" },
         { "175", "175%" },
         { "180", "180%" },
         { "185", "185%" },
         { "190", "190%" },
         { "195", "195%" },
         { "200", "200%" },
         { NULL, NULL },
      },
      "100"
   },
   {
      "px68k_midi_output",
      "Salida MIDI (reinicio)",
      NULL,
      "Emula la placa MIDI CZ-6BM1. Necesita que el frontend ofrezca una interfaz MIDI (en Salvia: un SoundFont .sf2 en el directorio system); sin salida MIDI, los juegos que detecten la placa reproducirán su música en silencio en lugar de usar el sonido FM/ADPCM interno.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_midi_state_restore",
      "Restaurar MIDI al cargar estado",
      NULL,
      "Al cargar un estado, el sintetizador MIDI conserva lo último que se le envió. Elige qué restaurar para que la música suene como cuando se guardó el estado. 'Configuración del sinte' reenvía el estándar de sonido (LA/GM/GS/XG), los parámetros SysEx y, de cada canal, el banco, el programa, el volumen, el panorama, los efectos y el pitch bend. 'Configuración y salida' además vuelve al sintetizador interno o a la salida MIDI externa que se usaba entonces, si están disponibles. No afecta al Run-Ahead.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "setup", "Configuración del sinte" },
         { "setup_output", "Configuración y salida" },
         { NULL, NULL },
      },
      "setup_output"
   },
   {
      "px68k_midi_output_type",
      "Tipo de salida MIDI (reinicio)",
      NULL,
      "Elige qué SysEx de reinicio se envía al módulo MIDI al arrancar la emulación.",
      NULL,
      "audio",
      {
         { "LA", NULL },
         { "GM", NULL },
         { "GS", NULL },
         { "XG", NULL },
         { NULL, NULL },
      },
      "GM"
   },
   {
      "px68k_adpcm_lpf",
      "Filtro paso-bajo ADPCM",
      NULL,
      "Habilita el filtro paso-bajo original de ADPCM.",
      NULL,
      "audio",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_adpcm_vol",
      "Volumen ADPCM",
      NULL,
      "Ajuste del volumen del canal de audio ADPCM.",
      NULL,
      "audio",
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
         { "9", NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { NULL, NULL },
      },
      "15"
   },
   {
      "px68k_opm_vol",
      "Volumen OPM",
      NULL,
      "Ajuste del volumen del canal de audio OPM.",
      NULL,
      "audio",
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
         { "9", NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { NULL, NULL },
      },
      "12"
   },
   {
      "px68k_mercury_vol",
      "Volumen Mercury",
      NULL,
      "Ajuste del volumen del canal de sonido Mercury.",
      NULL,
      "audio",
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
         { "9", NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { NULL, NULL },
      },
      "13"
   },
   {
      "px68k_mame_names",
      "Nombres de la lista de software MAME",
      NULL,
      "Muestra descripciones en inglés o etiquetas alt_title en japonés del archivo MAME x68k_flop.xml para nombres de archivo ZIP que coincidan en el explorador interno. Requiere MAME/hash/x68k_flop.xml en el directorio del sistema.",
      NULL,
      "menu",
      {
         { "disabled", NULL },
         { "english", "Inglés" },
         { "japanese", "Japonés" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_archive_floppies",
      "Inserción automática de disquetes desde archivo",
      NULL,
      "Controla si se inserta automáticamente otro disquete de un archivo ZIP al arrancar o durante los cambios de disco.",
      NULL,
      "media",
      {
         { "Off", "Apagado" },
         { "Always", "Siempre" },
         { "Boot Only", "Solo al arrancar" },
         { "At Disk Change", "Al cambiar de disco" },
         { NULL, NULL },
      },
      "Off"
   },
   {
      "px68k_disk_drive",
      "Cambio de disco en la unidad",
      NULL,
      "Por defecto, la interfaz nativa de cambio de disco de RetroArch cambia el disco en la unidad FDD1. Cambia esta opción para intercambiar discos en la unidad FDD0.",
      NULL,
      "media",
      {
         { "FDD1", NULL },
         { "FDD0", NULL },
         { NULL, NULL },
      },
      "FDD1"
   },
   {
      "px68k_save_fdd_path",
      "Guardar rutas de disquete",
      NULL,
      "Cuando está activado, la última ruta de disquete cargada se guarda para cada unidad y se auto-carga al iniciar. Cuando está desactivado, FDDx empieza vacío.",
      NULL,
      "media",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_save_hdd_path",
      "Guardar rutas de disco duro",
      NULL,
      "Cuando está activado, la última ruta de disco duro cargada se guarda para cada disco y se auto-carga al iniciar. Cuando está desactivado, HDDx empieza vacío.",
      NULL,
      "media",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_save_sram",
      "Guardar SRAM (NVRAM de la máquina)",
      NULL,
      "Cuando está activado, la SRAM de la máquina (dispositivo de arranque, reloj, repetición de teclado y otros ajustes de SWITCH.X) se guarda en sram.dat y se recarga al iniciar. Cuando está desactivado, la máquina siempre arranca con una SRAM nueva que el IPL reinicializa; útil si un sram.dat obsoleto da problemas.",
      NULL,
      "media",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_rumble_on_disk_read",
      "Vibración al leer disquete",
      NULL,
      "Produce un efecto de vibración en los mandos compatibles al leer de los disquetes.",
      NULL,
      "media",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_menufontsize",
      "Tamaño de fuente del menú",
      NULL,
      "Establece el tamaño de fuente del menú integrado",
      NULL,
      "menu",
      {
         { "normal", NULL },
         { "large", NULL },
         { NULL, NULL },
      },
      "normal"
   },
   {
      "px68k_menu_lines_jump",
      "Salto de líneas (con izquierda-derecha)",
      NULL,
      "Número de entradas para saltar al presionar Izquierda o Derecha en el menú integrado.",
      NULL,
      "menu",
      {
         { "5", NULL },
         { "10", NULL },
         { "15", NULL },
         { "20", NULL },
         { "25", NULL },
         { "30", NULL },
         { "35", NULL },
         { "40", NULL },
         { "45", NULL },
         { "50", NULL },
         { NULL, NULL },
      },
      "10"
   },
   {
      "px68k_text_off",
      "Desactivar capa de texto",
      NULL,
      "TODO:",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_grp_off",
      "Desactivar gráficos (GRP)",
      NULL,
      "TODO:",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "px68k_sp_off",
      "Desactivar sprites/fondo (SP/BG)",
      NULL,
      "TODO:",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, { 0, 0 }, NULL },
};

struct retro_core_options_v2 options_es = {
   option_cats_es,
   option_defs_es
};

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

/* RETRO_LANGUAGE_SLOVAK */

/* RETRO_LANGUAGE_PERSIAN */

/* RETRO_LANGUAGE_HEBREW */

/* RETRO_LANGUAGE_ASTURIAN */

/* RETRO_LANGUAGE_FINNISH */

#ifdef __cplusplus
}
#endif

#endif
