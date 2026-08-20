/***************************************************************************
    Front End Menu System.

    This file is part of Cannonball. 
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdio.h>
#include <compat/msvc.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "menu.h"
#include "../setup.h"
#include "../utils.h"

#include "../engine/ohud.h"
#include "../engine/oinputs.h"
#include "../engine/osprites.h"
#include "../engine/ologo.h"
#include "../engine/opalette.h"
#include "../engine/otiles.h"

#include "../frontend/ttrial.h"

#include "lr_options.h"

static void Menu_tick_ui(Menu* self);
static void Menu_draw_menu_options(Menu* self);
static void Menu_draw_text(Menu* self, const char* s);
static void Menu_tick_menu(Menu* self);
static void Menu_set_menu(Menu* self, const char** menu, uint8_t num);
static void Menu_set_menu_text(Menu* self, const char* s1, const char* s2);
static void Menu_redefine_keyboard(Menu* self);
static void Menu_redefine_joystick(Menu* self);
static void Menu_display_message(Menu* self, const char* s);
static bool Menu_check_jap_roms(Menu* self);
static void Menu_restart_video(Menu* self);
static void Menu_start_game(Menu* self, int mode, int settings);


/* Logo Y Position */
#define LOGO_Y (-60)

/* Columns and rows available */
#define COLS (40)
#define ROWS (28)

/* Horizon Destination Position */
#define HORIZON_DEST (0x3A0)

/* ------------------------------------------------------------------------------------------------ */
/* Text Labels for menus */
/* ------------------------------------------------------------------------------------------------ */

/* Back Labels */
const static char* ENTRY_BACK       = "BACK";

/* Main Menu */
const static char* ENTRY_PLAYGAME   = "PLAY GAME";
const static char* ENTRY_GAMEMODES  = "GAME MODES";
const static char* ENTRY_SETTINGS   = "SETTINGS";
const static char* ENTRY_ABOUT      = "ABOUT";
const static char* ENTRY_EXIT       = "EXIT";

/* Game Modes Menu */
const static char* ENTRY_ENHANCED   = "SET ENHANCED MODE";
const static char* ENTRY_ORIGINAL   = "SET ORIGINAL MODE";
const static char* ENTRY_CONT       = "CONTINUOUS MODE";
const static char* ENTRY_TIMETRIAL  = "TIME TRIAL MODE";

/* Time Trial Menu */
const static char* ENTRY_START      =  "START TIME TRIAL";
const static char* ENTRY_LAPS       =  "NO OF LAPS ";

/* Continuous Menu */
const static char* ENTRY_START_CONT = "START CONTINUOUS MODE";

/* Settings Menu */
const static char* ENTRY_VIDEO      = "VIDEO";
const static char* ENTRY_SOUND      = "SOUND";
const static char* ENTRY_CONTROLS   = "CONTROLS";
const static char* ENTRY_ENGINE     = "GAME ENGINE";
const static char* ENTRY_SCORES     = "CLEAR HISCORES";
const static char* ENTRY_SAVE       = "SAVE AND RETURN";

/* Video Menu */
const static char* ENTRY_FPS        = "FRAME RATE ";
const static char* ENTRY_FULLSCREEN = "FULL SCREEN ";
const static char* ENTRY_WIDESCREEN = "WIDESCREEN ";
const static char* ENTRY_HIRES      = "HIRES ";
const static char* ENTRY_SCALE      = "WINDOW SCALE ";
const static char* ENTRY_SCANLINES  = "SCANLINES ";

/* Sound Menu */
const static char* ENTRY_MUTE       = "SOUND ";
const static char* ENTRY_BGM        = "BGM VOL ";
const static char* ENTRY_SFX        = "SFX VOL ";
const static char* ENTRY_ADVERTISE  = "ADVERTISE SOUND ";
const static char* ENTRY_PREVIEWSND = "PREVIEW MUSIC ";
const static char* ENTRY_FIXSAMPLES = "FIX SAMPLES ";
const static char* ENTRY_MUSICTEST  = "MUSIC TEST";

/* Controls Menu */
const static char* ENTRY_GEAR       = "GEAR ";
const static char* ENTRY_ANALOG     = "ANALOG ";
const static char* ENTRY_REDEFJOY   = "REDEFINE GAMEPAD";
const static char* ENTRY_REDEFKEY   = "REDEFINE KEYS";
const static char* ENTRY_DSTEER     = "DIGITAL STEER SPEED ";
const static char* ENTRY_DPEDAL     = "DIGITAL PEDAL SPEED ";

/* Game Engine Menu */
const static char* ENTRY_FREEPLAY   = "FREE PLAY ";
const static char* ENTRY_FORCE_AI   = "FORCE AI TO PLAY ";
const static char* ENTRY_TRACKS     = "TRACKS ";
const static char* ENTRY_TIME       = "TIME ";
const static char* ENTRY_TRAFFIC    = "TRAFFIC ";
const static char* ENTRY_OBJECTS    = "OBJECTS ";
const static char* ENTRY_PROTOTYPE  = "PROTOTYPE STAGE 1 ";
const static char* ENTRY_ATTRACT    = "NEW ATTRACT ";
const static char* ENTRY_TIMER      = "TIMING FIXES ";
const static char* ENTRY_SUB_ENHANCEMENTS = "ENHANCEMENTS";
const static char* ENTRY_SUB_HANDLING     = "CAR SETUP";
const static char* ENTRY_GRIP       = "GRIPPY TYRES ";
const static char* ENTRY_OFFROAD    = "OFFROAD TYRES ";
const static char* ENTRY_BUMPER     = "STRONG BUMPER ";
const static char* ENTRY_TURBO      = "FASTER CAR ";
const static char* ENTRY_COLOR      = "CAR COLOR ";

const static char* COLOR_LABELS[7]  = { "RED", "BLUE", "YELLOW", "GREEN", "CYAN", "BLACK", "WHITE" };

/* Music Test Menu */
const static char* ENTRY_MUSIC1     = "MAGICAL SOUND SHOWER";
const static char* ENTRY_MUSIC2     = "PASSING BREEZE";
const static char* ENTRY_MUSIC3     = "SPLASH WAVE";
const static char* ENTRY_MUSIC4     = "LAST WAVE";

void Menu_ctor(Menu* self)
{
    self->ttrial = (TTrial*)malloc(sizeof(TTrial));
    TTrial_ctor(self->ttrial, config.ttrial.best_times);
}


void Menu_dtor(Menu* self)
{
    free(self->ttrial);
}

void Menu_populate(Menu* self)
{
    self->menu_main_num = 0;
    self->menu_gamemodes_num = 0;
    self->menu_cont_num = 0;
    self->menu_timetrial_num = 0;
    self->menu_about_num = 0;
    self->menu_settings_num = 0;
    self->menu_video_num = 0;
    self->menu_sound_num = 0;
    self->menu_controls_num = 0;
    self->menu_engine_num = 0;
    self->menu_enhancements_num = 0;
    self->menu_handling_num = 0;
    self->menu_musictest_num = 0;
    self->text_redefine_num = 0;
    /* Create Menus */
    self->menu_main[self->menu_main_num++] = ENTRY_PLAYGAME;
    self->menu_main[self->menu_main_num++] = ENTRY_GAMEMODES;
    self->menu_main[self->menu_main_num++] = ENTRY_SETTINGS;
    self->menu_main[self->menu_main_num++] = ENTRY_ABOUT;
    self->menu_main[self->menu_main_num++] = ENTRY_EXIT;

    self->menu_gamemodes[self->menu_gamemodes_num++] = ENTRY_ENHANCED;
    self->menu_gamemodes[self->menu_gamemodes_num++] = ENTRY_ORIGINAL;
    self->menu_gamemodes[self->menu_gamemodes_num++] = ENTRY_CONT;
    self->menu_gamemodes[self->menu_gamemodes_num++] = ENTRY_TIMETRIAL;
    self->menu_gamemodes[self->menu_gamemodes_num++] = ENTRY_BACK;

    self->menu_cont[self->menu_cont_num++] = ENTRY_START_CONT;
    self->menu_cont[self->menu_cont_num++] = ENTRY_TRAFFIC;
    self->menu_cont[self->menu_cont_num++] = ENTRY_BACK;

    self->menu_timetrial[self->menu_timetrial_num++] = ENTRY_START;
    self->menu_timetrial[self->menu_timetrial_num++] = ENTRY_LAPS;
    self->menu_timetrial[self->menu_timetrial_num++] = ENTRY_TRAFFIC;
    self->menu_timetrial[self->menu_timetrial_num++] = ENTRY_BACK;

    self->menu_settings[self->menu_settings_num++] = ENTRY_VIDEO;
    self->menu_settings[self->menu_settings_num++] = ENTRY_SOUND;
    self->menu_settings[self->menu_settings_num++] = ENTRY_CONTROLS;
    self->menu_settings[self->menu_settings_num++] = ENTRY_ENGINE;
    self->menu_settings[self->menu_settings_num++] = ENTRY_SCORES;
    self->menu_settings[self->menu_settings_num++] = ENTRY_SAVE;

    self->menu_video[self->menu_video_num++] = ENTRY_FPS;
    self->menu_video[self->menu_video_num++] = ENTRY_WIDESCREEN;
    self->menu_video[self->menu_video_num++] = ENTRY_HIRES;
    self->menu_video[self->menu_video_num++] = ENTRY_BACK;

    self->menu_sound[self->menu_sound_num++] = ENTRY_MUTE;
    /*menu_sound.push_back(ENTRY_BGM); */
    /*menu_sound.push_back(ENTRY_SFX); */
    self->menu_sound[self->menu_sound_num++] = ENTRY_ADVERTISE;
    self->menu_sound[self->menu_sound_num++] = ENTRY_PREVIEWSND;
    self->menu_sound[self->menu_sound_num++] = ENTRY_FIXSAMPLES;
    self->menu_sound[self->menu_sound_num++] = ENTRY_MUSICTEST;
    self->menu_sound[self->menu_sound_num++] = ENTRY_BACK;

    self->menu_controls[self->menu_controls_num++] = ENTRY_GEAR;
    if (input.gamepad) self->menu_controls[self->menu_controls_num++] = ENTRY_ANALOG;
    self->menu_controls[self->menu_controls_num++] = ENTRY_DSTEER;
    self->menu_controls[self->menu_controls_num++] = ENTRY_DPEDAL;
    self->menu_controls[self->menu_controls_num++] = ENTRY_BACK;

    self->menu_engine[self->menu_engine_num++] = ENTRY_TIME;
    self->menu_engine[self->menu_engine_num++] = ENTRY_TRAFFIC;
    self->menu_engine[self->menu_engine_num++] = ENTRY_TRACKS;
    self->menu_engine[self->menu_engine_num++] = ENTRY_FREEPLAY;
    self->menu_engine[self->menu_engine_num++] = ENTRY_FORCE_AI;
    self->menu_engine[self->menu_engine_num++] = ENTRY_SUB_ENHANCEMENTS;
    self->menu_engine[self->menu_engine_num++] = ENTRY_SUB_HANDLING;
    self->menu_engine[self->menu_engine_num++] = ENTRY_BACK;

    self->menu_enhancements[self->menu_enhancements_num++] = ENTRY_TIMER;
    self->menu_enhancements[self->menu_enhancements_num++] = ENTRY_ATTRACT;
    self->menu_enhancements[self->menu_enhancements_num++] = ENTRY_OBJECTS;
    self->menu_enhancements[self->menu_enhancements_num++] = ENTRY_PROTOTYPE;
    self->menu_enhancements[self->menu_enhancements_num++] = ENTRY_BACK;

    self->menu_handling[self->menu_handling_num++] = ENTRY_GRIP;
    self->menu_handling[self->menu_handling_num++] = ENTRY_OFFROAD;
    self->menu_handling[self->menu_handling_num++] = ENTRY_BUMPER;
    self->menu_handling[self->menu_handling_num++] = ENTRY_TURBO;
    self->menu_handling[self->menu_handling_num++] = ENTRY_COLOR;
    self->menu_handling[self->menu_handling_num++] = ENTRY_BACK;

    self->menu_musictest[self->menu_musictest_num++] = ENTRY_MUSIC1;
    self->menu_musictest[self->menu_musictest_num++] = ENTRY_MUSIC2;
    self->menu_musictest[self->menu_musictest_num++] = ENTRY_MUSIC3;
    self->menu_musictest[self->menu_musictest_num++] = ENTRY_MUSIC4;
    self->menu_musictest[self->menu_musictest_num++] = ENTRY_BACK;

    self->menu_about[self->menu_about_num++] = "CANNONBALL 0.3 \xA9 CHRIS WHITE 2014";
    self->menu_about[self->menu_about_num++] = "REASSEMBLER.BLOGSPOT.COM";
    self->menu_about[self->menu_about_num++] = " ";
    self->menu_about[self->menu_about_num++] = "CANNONBALL IS FREE AND MAY NOT BE SOLD.";

    /* Redefine menu text */
    self->text_redefine[self->text_redefine_num++] = "PRESS UP";
    self->text_redefine[self->text_redefine_num++] = "PRESS DOWN";
    self->text_redefine[self->text_redefine_num++] = "PRESS LEFT";
    self->text_redefine[self->text_redefine_num++] = "PRESS RIGHT";
    self->text_redefine[self->text_redefine_num++] = "PRESS ACCELERATE";
    self->text_redefine[self->text_redefine_num++] = "PRESS BRAKE";
    self->text_redefine[self->text_redefine_num++] = "PRESS GEAR";
    self->text_redefine[self->text_redefine_num++] = "PRESS GEAR HIGH";
    self->text_redefine[self->text_redefine_num++] = "PRESS START";
    self->text_redefine[self->text_redefine_num++] = "PRESS COIN IN";
    self->text_redefine[self->text_redefine_num++] = "PRESS MENU";
    self->text_redefine[self->text_redefine_num++] = "PRESS VIEW CHANGE";
}

void Menu_init(Menu* self)
{   
    /* If we got a new high score on previous time trial, then save it! */
    if (outrun.ttrial.new_high_score)
    {
        outrun.ttrial.new_high_score = false;
        TTrial_update_best_time(self->ttrial);
    }

    Outrun_select_course(&outrun, false, config.engine.prototype != 0);
    video.enabled = true;
    hwsprites_set_x_clip(video.sprite_layer, false); /* Stop clipping in wide-screen mode. */
    hwsprites_reset(video.sprite_layer);
    Video_clear_text_ram(&video);
    hwtiles_restore_tiles(video.tile_layer);
    OLogo_enable(&ologo, LOGO_Y);

    /* Setup palette, road and colours for background */
    oroad.stage_lookup_off = 9;
    OInitEngine_init_road_seg_master(&oinitengine);
    OPalette_setup_sky_palette(&opalette);
    OPalette_setup_ground_color(&opalette);
    OPalette_setup_road_centre(&opalette);
    OPalette_setup_road_stripes(&opalette);
    OPalette_setup_road_side(&opalette);
    OPalette_setup_road_colour(&opalette);
    OTiles_setup_palette_hud(&otiles);

    ORoad_init(&oroad);
    oroad.road_ctrl = ROAD_R0;
    oroad.horizon_set = 1;
    oroad.horizon_base = HORIZON_DEST + 0x100;
    oinitengine.rd_split_state = SPLIT_NONE;
    oinitengine.car_increment = 0;
    oinitengine.change_width = 0;

    outrun.game_state = GS_INIT;

    Menu_set_menu(self, self->menu_main, self->menu_main_num);
    Menu_refresh_menu(self);

    /* Reset audio, so we can play tones */
    osoundint.has_booted = true;
    OSoundInt_init(&osoundint);
    Audio_clear_wav(&cannonball_audio);

    self->frame = 0;
    self->message_counter = 0;

    self->state = MENU_STATE_MENU;
}

void Menu_tick(Menu* self)
{
    switch (self->state)
    {
        case MENU_STATE_MENU:
        case MENU_STATE_REDEFINE_KEYS:
        case MENU_STATE_REDEFINE_JOY:
            Menu_tick_ui(self);
            break;

        case MENU_STATE_TTRIAL:
            {
                int ttrial_state = TTrial_tick(self->ttrial);

                if (ttrial_state == INIT_GAME)
                {
                    cannonball_state = STATE_INIT_GAME;
                    OSoundInt_queue_clear(&osoundint);
                }
                else if (ttrial_state == BACK_TO_MENU)
                {
                    Menu_init(self);
                }
            }
            break;
    }
}

static void Menu_tick_ui(Menu* self)
{
    /* Skip odd frames at 60fps */
    self->frame++;

    Video_clear_text_ram(&video);

    if (self->state == MENU_STATE_MENU)
    {
        Menu_tick_menu(self);
        Menu_draw_menu_options(self);
    }
    else if (self->state == MENU_STATE_REDEFINE_KEYS)
    {
        Menu_redefine_keyboard(self);
    }
    else if (self->state == MENU_STATE_REDEFINE_JOY)
    {
        Menu_redefine_joystick(self);
    }

    /* Show messages */
    if (self->message_counter > 0)
    {
        self->message_counter--;
        OHud_blit_text_new(&ohud, 0, 1, self->msg, GREY);
    }
     
    /* Shift horizon */
    if (oroad.horizon_base > HORIZON_DEST)
    {
        oroad.horizon_base -= 60 / (config.fps < 60 ? config.fps : 60);
        if (oroad.horizon_base < HORIZON_DEST)
            oroad.horizon_base = HORIZON_DEST;
    }
    /* Advance road */
    else
    {
        uint32_t scroll_speed = (config.fps >= 60) ? config.menu.road_scroll_speed : config.menu.road_scroll_speed << 1;

        if (oinitengine.car_increment < scroll_speed << 16)
            oinitengine.car_increment += (1 << 14);
        if (oinitengine.car_increment > scroll_speed << 16)
            oinitengine.car_increment = scroll_speed << 16;
        { uint32_t result = 0x12F * (oinitengine.car_increment >> 16);
        oroad.road_pos_change = result;
        oroad.road_pos += result;
        if (oroad.road_pos >> 16 > ROAD_END) /* loop to beginning of track data */
            oroad.road_pos = 0;
        OInitEngine_update_road(&oinitengine);
        OInitEngine_set_granular_position(&oinitengine);
        oroad.road_width_bak = oroad.road_width >> 16; 
        oroad.car_x_bak = -oroad.road_width_bak; 
        oinitengine.car_x_pos = oroad.car_x_bak;
     }}

    /* Do Animations at 30 fps */
    if (config.fps == 30
        || (config.fps == 60 && (self->frame & 1) == 0)
        || (config.fps == 120 && (self->frame & 3) == 1))
    {
        OLogo_tick(&ologo);
        OSprites_sprite_copy(&osprites);
        OSprites_update_sprites(&osprites);
    }

    /* Draw FPS */
    if (config.video.fps_count)
        OHud_draw_fps_counter(&ohud, cannonball_fps_counter);

    ORoad_tick(&oroad);
}

static void Menu_draw_menu_options(Menu* self)
{
    int8_t x = 0;

    /* Find central column in screen.  */
    int8_t y = 13 + ((ROWS - 13) >> 1) - ((self->menu_selected_num * 2) >> 1);

    { int i; for (i = 0; i < (int) self->menu_selected_num; i++)
    {
        const char* s = self->menu_selected[i];

        /* Centre the menu option */
        x = 20 - (strlen(s) >> 1);
        OHud_blit_text_new(&ohud, x, y, s, GREEN);

        if (!self->is_text_menu)
        {
            /* Draw minicar */
            if (i == self->cursor)
                Video_write_text32(&video, OHud_translate(&ohud, x - 3, y, 0x110030), RomLoader_read32(&(roms.rom0), TILES_MINICARS1));
            /* Erase minicar from this position */
            else
                Video_write_text32(&video, OHud_translate(&ohud, x - 3, y, 0x110030), 0x20202020);
        }
        y += 2;
    } }
}

/* Draw a single line of text */
static void Menu_draw_text(Menu* self, const char* s)
{
    /* Centre text */
    int8_t x = 20 - (strlen(s) >> 1);

    /* Find central column in screen.  */
    int8_t y = 13 + ((ROWS - 13) >> 1) - 1;

    OHud_blit_text_new(&ohud, x, y, s, GREEN);
}

static bool menu_starts_with(
    const char* value,
    const char* prefix)
{
    return strncmp(value, prefix, strlen(prefix)) == 0;
}


#define SELECTED(string) menu_starts_with(OPTION, string)

static void Menu_tick_menu(Menu* self)
{
    /* Tick Controls */
    if (Input_has_pressed(&input, DOWN) || OInputs_is_analog_r(&oinputs))
    {
        OSoundInt_queue_sound(&osoundint, SOUND_BEEP1);

        if (++self->cursor >= (int16_t) self->menu_selected_num)
            self->cursor = 0;
    }
    else if (Input_has_pressed(&input, UP) || OInputs_is_analog_l(&oinputs))
    {
        OSoundInt_queue_sound(&osoundint, SOUND_BEEP1);

        if (--self->cursor < 0)
            self->cursor = self->menu_selected_num - 1;
    }
    else if (Input_has_pressed(&input, ACCEL) || Input_has_pressed(&input, START) || OInputs_is_analog_select(&oinputs))
    {
        /* Get option that was selected */
        const char* OPTION = self->menu_selected[self->cursor];

        if (self->menu_selected == self->menu_main)
        {
            if (SELECTED(ENTRY_PLAYGAME))
            {
                Menu_start_game(self, MODE_ORIGINAL, 0);
                return;
            }
            else if (SELECTED(ENTRY_GAMEMODES))
                Menu_set_menu(self, self->menu_gamemodes, self->menu_gamemodes_num);
            else if (SELECTED(ENTRY_SETTINGS))
                Menu_set_menu(self, self->menu_settings, self->menu_settings_num);
            else if (SELECTED(ENTRY_ABOUT))
                Menu_set_menu(self, self->menu_about, self->menu_about_num);
            else if (SELECTED(ENTRY_EXIT))
            {
                cannonball_state = STATE_QUIT;
            }
        }
        else if (self->menu_selected == self->menu_gamemodes)
        {
            if (SELECTED(ENTRY_ENHANCED))
                Menu_start_game(self, MODE_ORIGINAL, 1);
            else if (SELECTED(ENTRY_ORIGINAL))
                Menu_start_game(self, MODE_ORIGINAL, 2);
            else if (SELECTED(ENTRY_CONT))
                Menu_set_menu(self, self->menu_cont, self->menu_cont_num);
            else if (SELECTED(ENTRY_TIMETRIAL))
                Menu_set_menu(self, self->menu_timetrial, self->menu_timetrial_num);
            else if (SELECTED(ENTRY_BACK))
                Menu_set_menu(self, self->menu_main, self->menu_main_num);
        }
        else if (self->menu_selected == self->menu_cont)
        {
            if (SELECTED(ENTRY_START_CONT))
            {
                outrun.custom_traffic = config.cont_traffic;
                Menu_start_game(self, MODE_CONT, 0);
            }
            else if (SELECTED(ENTRY_TRAFFIC))
            {
                if (++config.cont_traffic > MAX_TRAFFIC)
                    config.cont_traffic = 0;
                lr_options_set_frontend_variable_int(&config.cont_traffic);
            }
            else if (SELECTED(ENTRY_BACK))
                Menu_set_menu(self, self->menu_gamemodes, self->menu_gamemodes_num);
        }
        else if (self->menu_selected == self->menu_timetrial)
        {
            if (SELECTED(ENTRY_START))
            {
                if (Menu_check_jap_roms(self))
                {
                    self->state = MENU_STATE_TTRIAL;
                    TTrial_init(self->ttrial);
                }
            }
            else if (SELECTED(ENTRY_LAPS))
            {
                if (++config.ttrial.laps > MAX_LAPS)
                    config.ttrial.laps = 1;
                lr_options_set_frontend_variable_int(&config.ttrial.laps);
            }
            else if (SELECTED(ENTRY_TRAFFIC))
            {
                if (++config.ttrial.traffic > MAX_TRAFFIC)
                    config.ttrial.traffic = 0;
                lr_options_set_frontend_variable_int(&config.ttrial.traffic);
            }
            else if (SELECTED(ENTRY_BACK))
                Menu_set_menu(self, self->menu_gamemodes, self->menu_gamemodes_num);
        }
        else if (self->menu_selected == self->menu_settings)
        {
            if (SELECTED(ENTRY_VIDEO))
                Menu_set_menu(self, self->menu_video, self->menu_video_num);
            else if (SELECTED(ENTRY_SOUND))
                Menu_set_menu(self, self->menu_sound, self->menu_sound_num);
            else if (SELECTED(ENTRY_CONTROLS))
            {
                if (input.gamepad)
                    Menu_display_message(self, "GAMEPAD FOUND");
                Menu_set_menu(self, self->menu_controls, self->menu_controls_num);
            }
            else if (SELECTED(ENTRY_ENGINE))
                Menu_set_menu(self, self->menu_engine, self->menu_engine_num);
            else if (SELECTED(ENTRY_SCORES))
            {
                if (Config_clear_scores(&config))
                    Menu_display_message(self, "SCORES CLEARED");
                else
                    Menu_display_message(self, "NO SAVED SCORES FOUND!");
            }
            else if (SELECTED(ENTRY_SAVE))
            {
                Menu_set_menu(self, self->menu_main, self->menu_main_num);
            }
        }
        else if (self->menu_selected == self->menu_about)
        {
            Menu_set_menu(self, self->menu_main, self->menu_main_num);
        }
        else if (self->menu_selected == self->menu_video)
        {
            if (SELECTED(ENTRY_FULLSCREEN))
            {
                if (++config.video.mode > MODE_STRETCH)
                    config.video.mode = MODE_WINDOW;
                Menu_restart_video(self);
            }
            else if (SELECTED(ENTRY_WIDESCREEN))
            {
                config.video.widescreen = !config.video.widescreen;
                Menu_restart_video(self);
                update_geometry();
                lr_options_set_frontend_variable_int(&config.video.widescreen);
            }
            else if (SELECTED(ENTRY_HIRES))
            {
                config.video.hires = !config.video.hires;
                if (config.video.hires)
                {
                    if (config.video.scale > 1)
                        config.video.scale >>= 1;
                }
                else
                {
                    config.video.scale <<= 1;
                }

                Menu_restart_video(self);
                hwsprites_set_x_clip(video.sprite_layer, false);
                update_geometry();
                lr_options_set_frontend_variable_int(&config.video.hires);
            }
            else if (SELECTED(ENTRY_SCALE))
            {
                if (++config.video.scale > (config.video.hires ? 2 : 4))
                    config.video.scale = 1;
                Menu_restart_video(self);
            }
            else if (SELECTED(ENTRY_SCANLINES))
            {
                config.video.scanlines += 10;
                if (config.video.scanlines > 100)
                    config.video.scanlines = 0;
                Menu_restart_video(self);
            }
            else if (SELECTED(ENTRY_FPS))
            {
                int fps_prev = config.fps;
                if (++config.video.fps > 3)
                {
                    config.video.fps = 0;
                }
                Config_set_fps(&config, config.video.fps);
                if (config.fps != fps_prev)
                    update_timing();
                lr_options_set_frontend_variable_int(&config.video.fps);
            }
            else if (SELECTED(ENTRY_BACK))
                Menu_set_menu(self, self->menu_settings, self->menu_settings_num);
        }
        else if (self->menu_selected == self->menu_sound)
        {
            if (SELECTED(ENTRY_MUTE))
            {
                config.sound.enabled = !config.sound.enabled;
                if (config.sound.enabled)
                    Audio_start_audio(&cannonball_audio);
                else
                    Audio_stop_audio(&cannonball_audio);              
                lr_options_set_frontend_variable_int(&config.sound.enabled);
            }
            else if (SELECTED(ENTRY_ADVERTISE))
            {
                config.sound.advertise = !config.sound.advertise;
                lr_options_set_frontend_variable_int(&config.sound.advertise);
            }
            else if (SELECTED(ENTRY_PREVIEWSND))
            {
                config.sound.preview = !config.sound.preview;
                lr_options_set_frontend_variable_int(&config.sound.preview);
            }
            else if (SELECTED(ENTRY_FIXSAMPLES))
            {
                int rom_type = !config.sound.fix_samples;
                
                if (Roms_load_pcm_rom(&roms, rom_type == 1))
                {
                    config.sound.fix_samples = rom_type;
                    Menu_display_message(self, rom_type == 1 ? "FIXED SAMPLES LOADED" : "ORIGINAL SAMPLES LOADED");
                }
                else
                {
                    Menu_display_message(self, rom_type == 1 ? "CANT LOAD FIXED SAMPLES" : "CANT LOAD ORIGINAL SAMPLES");
                }
                lr_options_set_frontend_variable_int(&config.sound.fix_samples);
            }
            else if (SELECTED(ENTRY_MUSICTEST))
                Menu_set_menu(self, self->menu_musictest, self->menu_musictest_num);
            else if (SELECTED(ENTRY_BACK))
                Menu_set_menu(self, self->menu_settings, self->menu_settings_num);
        }
        else if (self->menu_selected == self->menu_controls)
        {
            if (SELECTED(ENTRY_GEAR))
            {
                if (++config.controls.gear > GEAR_AUTO)
                    config.controls.gear = GEAR_BUTTON;
                lr_options_set_frontend_variable_int(&config.controls.gear);
            }
            else if (SELECTED(ENTRY_ANALOG))
            {
                if (++config.controls.analog >= 2)
                    config.controls.analog = 0;
                input.analog = config.controls.analog;
                lr_options_set_frontend_variable_int(&config.controls.analog);
            }
            else if (SELECTED(ENTRY_REDEFKEY))
            {
                Menu_display_message(self, "PRESS MENU TO END AT ANY STAGE");
                self->state = MENU_STATE_REDEFINE_KEYS;
                self->redef_state = 0;
                input.key_press = -1;
            }
            else if (SELECTED(ENTRY_REDEFJOY))
            {
                Menu_display_message(self, "PRESS MENU TO END AT ANY STAGE");
                self->state = MENU_STATE_REDEFINE_JOY;
                self->redef_state = config.controls.analog == 1 ? 2 : 0; /* Ignore pedals when redefining analog */
                input.joy_button = -1;
            }
            else if (SELECTED(ENTRY_DSTEER))
            {
                if (++config.controls.steer_speed > 9)
                    config.controls.steer_speed = 1;
                lr_options_set_frontend_variable_int(&config.controls.steer_speed);
            }
            else if (SELECTED(ENTRY_DPEDAL))
            {
                if (++config.controls.pedal_speed > 9)
                    config.controls.pedal_speed = 1;
                lr_options_set_frontend_variable_int(&config.controls.pedal_speed);
            }
            else if (SELECTED(ENTRY_BACK))
                Menu_set_menu(self, self->menu_settings, self->menu_settings_num);
        }
        else if (self->menu_selected == self->menu_engine)
        {
            if (SELECTED(ENTRY_TRACKS))
            {
                config.engine.jap = !config.engine.jap;
                lr_options_set_frontend_variable_int(&config.engine.jap);
            }
            else if (SELECTED(ENTRY_FREEPLAY))
            {
               config.engine.freeplay = !config.engine.freeplay;
	       lr_options_set_frontend_variable_bool(&config.engine.freeplay);
            }
            else if (SELECTED(ENTRY_FORCE_AI))
            {
               config.engine.force_ai = !config.engine.force_ai;
	       lr_options_set_frontend_variable_bool(&config.engine.force_ai);
            }
            else if (SELECTED(ENTRY_TIME))
            {
                if (config.engine.dip_time < 4)
                    config.engine.dip_time++;
                else
                    config.engine.dip_time = 0;

                if (config.engine.dip_time == 4)
                    config.engine.freeze_timer = 1;
                else
                    config.engine.freeze_timer = 0;

                lr_options_set_frontend_variable_int(&config.engine.dip_time);
            }
            else if (SELECTED(ENTRY_TRAFFIC))
            {
                if (config.engine.dip_traffic < 4)
                    config.engine.dip_traffic++;
                else
                    config.engine.dip_traffic = 0;

                if (config.engine.dip_traffic == 4)
                    config.engine.disable_traffic = 1;
                else
                    config.engine.disable_traffic = 0;

                lr_options_set_frontend_variable_int(&config.engine.dip_traffic);
            }
            else if (SELECTED(ENTRY_SUB_ENHANCEMENTS))
                Menu_set_menu(self, self->menu_enhancements, self->menu_enhancements_num);
            else if (SELECTED(ENTRY_SUB_HANDLING))
                Menu_set_menu(self, self->menu_handling, self->menu_handling_num);
            else if (SELECTED(ENTRY_BACK))
                Menu_set_menu(self, self->menu_settings, self->menu_settings_num);
        }
        else if (self->menu_selected == self->menu_enhancements)
        {
            if (SELECTED(ENTRY_TIMER))
            {
                config.engine.fix_timer = !config.engine.fix_timer;
                lr_options_set_frontend_variable_bool(&config.engine.fix_timer);
            }
            else if (SELECTED(ENTRY_OBJECTS))
            {
                config.engine.level_objects = !config.engine.level_objects;
                lr_options_set_frontend_variable_int(&config.engine.level_objects);
            }
            else if (SELECTED(ENTRY_PROTOTYPE))
            {
                config.engine.prototype = !config.engine.prototype;
                lr_options_set_frontend_variable_int(&config.engine.prototype);
            }
            else if (SELECTED(ENTRY_ATTRACT))
            {
                config.engine.new_attract ^= 1;
                lr_options_set_frontend_variable_int(&config.engine.new_attract);
            }
            else if (SELECTED(ENTRY_BACK))
                Menu_set_menu(self, self->menu_engine, self->menu_engine_num);
        }
        else if (self->menu_selected == self->menu_handling)
        {
            if (SELECTED(ENTRY_GRIP))
            {
                config.engine.grippy_tyres = !config.engine.grippy_tyres;
                lr_options_set_frontend_variable_bool(&config.engine.grippy_tyres);
            }
            else if (SELECTED(ENTRY_OFFROAD))
            {
                config.engine.offroad = !config.engine.offroad;
                lr_options_set_frontend_variable_bool(&config.engine.offroad);
            }
            else if (SELECTED(ENTRY_BUMPER))
            {
                config.engine.bumper = !config.engine.bumper;
                lr_options_set_frontend_variable_bool(&config.engine.bumper);
            }
            else if (SELECTED(ENTRY_TURBO))
            {
                config.engine.turbo = !config.engine.turbo;
                lr_options_set_frontend_variable_bool(&config.engine.turbo);
            }
            else if (SELECTED(ENTRY_COLOR))
            {
                if (++config.engine.car_pal > 6)
                    config.engine.car_pal = 0;
                lr_options_set_frontend_variable_int(&config.engine.car_pal);
            }
            else if (SELECTED(ENTRY_BACK))
                Menu_set_menu(self, self->menu_engine, self->menu_engine_num);
        }
        else if (self->menu_selected == self->menu_musictest)
        {
            if (SELECTED(ENTRY_MUSIC1))
                OSoundInt_queue_sound(&osoundint, SOUND_MUSIC_MAGICAL);
            else if (SELECTED(ENTRY_MUSIC2))
                OSoundInt_queue_sound(&osoundint, SOUND_MUSIC_BREEZE);
            else if (SELECTED(ENTRY_MUSIC3))
                OSoundInt_queue_sound(&osoundint, SOUND_MUSIC_SPLASH);
            else if (SELECTED(ENTRY_MUSIC4))
                OSoundInt_queue_sound(&osoundint, SOUND_MUSIC_LASTWAVE);

            else if (SELECTED(ENTRY_BACK))
            {
                OSoundInt_queue_sound(&osoundint, SOUND_FM_RESET);
                Menu_set_menu(self, self->menu_sound, self->menu_sound_num);
            }
        }
        else
            Menu_set_menu(self, self->menu_main, self->menu_main_num);

        OSoundInt_queue_sound(&osoundint, SOUND_BEEP1);
        Menu_refresh_menu(self);
    }
}

/* Set Current Menu */
static void Menu_set_menu(Menu* self, const char** menu, uint8_t num)
{
    self->menu_selected     = menu;
    self->menu_selected_num = num;
    self->cursor            = 0;
    self->is_text_menu      = (menu == self->menu_about);
}

/* Refresh menu options with latest config data */
void Menu_refresh_menu(Menu* self)
{
    int16_t cursor_backup = self->cursor;
    const char* s = "";

    for (self->cursor = 0; self->cursor < (int) self->menu_selected_num; self->cursor++)
    {
        /* Get option that was selected */
        const char* OPTION = self->menu_selected[self->cursor];

        if (self->menu_selected == self->menu_timetrial)
        {
            if (SELECTED(ENTRY_LAPS))
                Menu_set_menu_text(self, ENTRY_LAPS, Utils_to_string(config.ttrial.laps));
            else if (SELECTED(ENTRY_TRAFFIC))
                Menu_set_menu_text(self, ENTRY_TRAFFIC, config.ttrial.traffic == 0 ? "DISABLED" : Utils_to_string(config.ttrial.traffic));
        }
        else if (self->menu_selected == self->menu_cont)
        {
            if (SELECTED(ENTRY_TRAFFIC))
                Menu_set_menu_text(self, ENTRY_TRAFFIC, config.cont_traffic == 0 ? "DISABLED" : Utils_to_string(config.cont_traffic));
        }
        else if (self->menu_selected == self->menu_video)
        {
            if (SELECTED(ENTRY_FULLSCREEN))
            {
                if (config.video.mode == MODE_WINDOW)       s = "OFF";
                else if (config.video.mode == MODE_FULL)    s = "ON";
                else if (config.video.mode == MODE_STRETCH) s = "STRETCH";
                Menu_set_menu_text(self, ENTRY_FULLSCREEN, s);
            }
            else if (SELECTED(ENTRY_WIDESCREEN))
                Menu_set_menu_text(self, ENTRY_WIDESCREEN, config.video.widescreen ? "ON" : "OFF");
            else if (SELECTED(ENTRY_SCALE))
                { char vb[16]; snprintf(vb, sizeof(vb), "%dX", config.video.scale); Menu_set_menu_text(self, ENTRY_SCALE, vb); }
            else if (SELECTED(ENTRY_HIRES))
                Menu_set_menu_text(self, ENTRY_HIRES, config.video.hires ? "ON" : "OFF");
            else if (SELECTED(ENTRY_FPS))
            {
                if (config.video.fps == 0)      s = "30 FPS";
                else if (config.video.fps == 1) s = "ORIGINAL";
                else if (config.video.fps == 2) s = "60 FPS";
                else if (config.video.fps == 3) s = "120 FPS";
                Menu_set_menu_text(self, ENTRY_FPS, s);
            }
            else if (SELECTED(ENTRY_SCANLINES))
                { char vb[16]; if (config.video.scanlines) snprintf(vb, sizeof(vb), "%d%%", config.video.scanlines); else strcpy(vb, "OFF"); Menu_set_menu_text(self, ENTRY_SCANLINES, vb); }
        }
        else if (self->menu_selected == self->menu_sound)
        {
            if (SELECTED(ENTRY_MUTE))
                Menu_set_menu_text(self, ENTRY_MUTE, config.sound.enabled ? "ON" : "OFF");
            else if (SELECTED(ENTRY_ADVERTISE))
                Menu_set_menu_text(self, ENTRY_ADVERTISE, config.sound.advertise ? "ON" : "OFF");
            else if (SELECTED(ENTRY_PREVIEWSND))
                Menu_set_menu_text(self, ENTRY_PREVIEWSND, config.sound.preview ? "ON" : "OFF");
            else if (SELECTED(ENTRY_FIXSAMPLES))
                Menu_set_menu_text(self, ENTRY_FIXSAMPLES, config.sound.fix_samples ? "ON" : "OFF");
        }
        else if (self->menu_selected == self->menu_controls)
        {
            if (SELECTED(ENTRY_GEAR))
            {
                if (config.controls.gear == GEAR_BUTTON)        s = "MANUAL";
                else if (config.controls.gear == GEAR_PRESS)    s = "MANUAL CABINET";
                else if (config.controls.gear == GEAR_SEPARATE) s = "MANUAL 2 BUTTONS";
                else if (config.controls.gear == GEAR_AUTO)     s = "AUTOMATIC";
                Menu_set_menu_text(self, ENTRY_GEAR, s);
            }
            else if (SELECTED(ENTRY_ANALOG))
            {
                if (config.controls.analog == 0)      s = "OFF";
                else if (config.controls.analog == 1) s = "ON";
                else if (config.controls.analog == 2) s = "ON WHEEL ONLY";
                Menu_set_menu_text(self, ENTRY_ANALOG, s);
            }
            else if (SELECTED(ENTRY_DSTEER))
                Menu_set_menu_text(self, ENTRY_DSTEER, Utils_to_string(config.controls.steer_speed));
            else if (SELECTED(ENTRY_DPEDAL))
                Menu_set_menu_text(self, ENTRY_DPEDAL, Utils_to_string(config.controls.pedal_speed));
        }
        else if (self->menu_selected == self->menu_engine)
        {
            if (SELECTED(ENTRY_TRACKS))
                Menu_set_menu_text(self, ENTRY_TRACKS, config.engine.jap ? "JAPAN" : "WORLD");
            else if (SELECTED(ENTRY_FREEPLAY))
                Menu_set_menu_text(self, ENTRY_FREEPLAY, config.engine.freeplay ? "ON" : "OFF");
            else if (SELECTED(ENTRY_FORCE_AI))
                Menu_set_menu_text(self, ENTRY_FORCE_AI, config.engine.force_ai ? "ON" : "OFF");
            else if (SELECTED(ENTRY_TIME))
            {
                if (config.engine.freeze_timer)       s = "INFINITE";
                else if (config.engine.dip_time == 0) s = "EASY";
                else if (config.engine.dip_time == 1) s = "NORMAL";
                else if (config.engine.dip_time == 2) s = "HARD";
                else if (config.engine.dip_time == 3) s = "HARDEST";          
                Menu_set_menu_text(self, ENTRY_TIME, s);
            }
            else if (SELECTED(ENTRY_TRAFFIC))
            {
                if (config.engine.disable_traffic)       s = "DISABLED";
                else if (config.engine.dip_traffic == 0) s = "EASY";
                else if (config.engine.dip_traffic == 1) s = "NORMAL";
                else if (config.engine.dip_traffic == 2) s = "HARD";
                else if (config.engine.dip_traffic == 3) s = "HARDEST";          
                Menu_set_menu_text(self, ENTRY_TRAFFIC, s);
            }
        }
        else if (self->menu_selected == self->menu_enhancements)
        {
            if (SELECTED(ENTRY_TIMER))
                Menu_set_menu_text(self, ENTRY_TIMER, config.engine.fix_timer ? "ON" : "OFF");
            else if (SELECTED(ENTRY_OBJECTS))
                Menu_set_menu_text(self, ENTRY_OBJECTS, config.engine.level_objects ? "ENHANCED" : "ORIGINAL");
            else if (SELECTED(ENTRY_PROTOTYPE))
                Menu_set_menu_text(self, ENTRY_PROTOTYPE, config.engine.prototype ? "ON" : "OFF");
            else if (SELECTED(ENTRY_ATTRACT))
                Menu_set_menu_text(self, ENTRY_ATTRACT, config.engine.new_attract ? "ON" : "OFF");
        }
        else if (self->menu_selected == self->menu_handling)
        {
            if (SELECTED(ENTRY_GRIP))
                Menu_set_menu_text(self, ENTRY_GRIP, config.engine.grippy_tyres ? "ON" : "OFF");
            else if (SELECTED(ENTRY_OFFROAD))
                Menu_set_menu_text(self, ENTRY_OFFROAD, config.engine.offroad ? "ON" : "OFF");
            else if (SELECTED(ENTRY_BUMPER))
                Menu_set_menu_text(self, ENTRY_BUMPER, config.engine.bumper ? "ON" : "OFF");
            else if (SELECTED(ENTRY_TURBO))
                Menu_set_menu_text(self, ENTRY_TURBO, config.engine.turbo ? "ON" : "OFF");
            else if (SELECTED(ENTRY_COLOR))
                Menu_set_menu_text(self, ENTRY_COLOR, COLOR_LABELS[config.engine.car_pal]);
        }
    }
    self->cursor = cursor_backup;
}

/* Append Menu Text For A Particular Menu Entry */
static void Menu_set_menu_text(Menu* self, const char* s1, const char* s2)
{
    snprintf(self->menu_text[self->cursor], sizeof(self->menu_text[self->cursor]), "%s%s", s1, s2);
    self->menu_text[self->cursor][sizeof(self->menu_text[self->cursor]) - 1] = 0;
    self->menu_selected[self->cursor] = self->menu_text[self->cursor];
}

static void Menu_redefine_keyboard(Menu* self)
{
    if (self->redef_state == 7 && config.controls.gear != GEAR_SEPARATE) /* Skip redefine of second gear press */
        self->redef_state++;

    switch (self->redef_state)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
            if (Input_has_pressed(&input, MENU))
            {
                self->message_counter = 0;
                self->state = MENU_STATE_MENU;
            }
            else
            {
                Menu_draw_text(self, self->text_redefine[self->redef_state]);
                if (input.key_press != -1)
                {
                    config.controls.keyconfig[self->redef_state] = input.key_press;
                    self->redef_state++;
                    input.key_press = -1;
                }
            }
            break;

        case 12:
            self->state = MENU_STATE_MENU;
            break;
    }
}

static void Menu_redefine_joystick(Menu* self)
{
    if (self->redef_state == 3 && config.controls.gear != GEAR_SEPARATE) /* Skip redefine of second gear press */
        self->redef_state++;

    switch (self->redef_state)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            if (Input_has_pressed(&input, MENU))
            {
                self->message_counter = 0;
                self->state = MENU_STATE_MENU;
            }
            else
            {
                Menu_draw_text(self, self->text_redefine[self->redef_state + 4]);
                if (input.joy_button != -1)
                {
                    config.controls.padconfig[self->redef_state] = input.joy_button;
                    self->redef_state++;
                    input.joy_button = -1;
                }
            }
            break;

        case 8:
            self->state = MENU_STATE_MENU;
            break;
    }
}

/* Display a contextual message in the top left of the screen */
static void Menu_display_message(Menu* self, const char* s)
{
    strncpy(self->msg, s, sizeof(self->msg) - 1);
    self->msg[sizeof(self->msg) - 1] = 0;
    self->message_counter = MESSAGE_TIME * config.fps;
}

static bool Menu_check_jap_roms(Menu* self)
{
    if (config.engine.jap && !Roms_load_japanese_roms(&roms))
    {
        Menu_display_message(self, "JAPANESE ROMSET NOT FOUND");
        return false;
    }
    return true;
}

/* Reinitalize Video, and stop audio to avoid crackles */
static void Menu_restart_video(Menu* self)
{
    if (config.sound.enabled)
        Audio_stop_audio(&cannonball_audio);
    Video_disable(&video);
    Video_init(&video, &roms, &config.video);
    OSoundInt_init(&osoundint);
    if (config.sound.enabled)
        Audio_start_audio(&cannonball_audio);
}

static void Menu_start_game(Menu* self, int mode, int settings)
{
    int fps_prev = config.fps;

    /* Enhanced Settings */
    if (settings == 1)
    {
        if (!config.video.hires)
        {
            if (config.video.scale > 1)
                config.video.scale >>= 1;
        }

        if (!config.sound.fix_samples)
        {
            if (Roms_load_pcm_rom(&roms, true))
                config.sound.fix_samples = 1;
        }

        Config_set_fps(&config, config.video.fps = 2);
        config.video.widescreen     = 1;
        config.video.hires          = 1;
        config.engine.level_objects = 1;
        config.engine.new_attract   = 1;
        config.engine.fix_bugs      = 1;
        config.sound.preview        = 1;

        Menu_restart_video(self);
        update_geometry();
        if (config.fps != fps_prev)
            update_timing();

        lr_options_set_frontend_variable_int(&config.sound.fix_samples);
        lr_options_set_frontend_variable_int(&config.video.fps);
        lr_options_set_frontend_variable_int(&config.video.widescreen);
        lr_options_set_frontend_variable_int(&config.video.hires);
        lr_options_set_frontend_variable_int(&config.engine.level_objects);
        lr_options_set_frontend_variable_int(&config.engine.new_attract);
        lr_options_set_frontend_variable_bool(&config.engine.fix_bugs);
        lr_options_set_frontend_variable_int(&config.sound.preview);
    }
    /* Original Settings */
    else if (settings == 2)
    {
        if (config.video.hires)
        {
            config.video.scale <<= 1;
        }

        if (config.sound.fix_samples)
        {
            if (Roms_load_pcm_rom(&roms, false))
                config.sound.fix_samples = 0;
        }

        Config_set_fps(&config, config.video.fps = 1);
        config.video.widescreen     = 0;
        config.video.hires          = 0;
        config.engine.level_objects = 0;
        config.engine.new_attract   = 0;
        config.engine.fix_bugs      = 0;
        config.sound.preview        = 0;

        Menu_restart_video(self);
        update_geometry();
        if (config.fps != fps_prev)
            update_timing();

        lr_options_set_frontend_variable_int(&config.sound.fix_samples);
        lr_options_set_frontend_variable_int(&config.video.fps);
        lr_options_set_frontend_variable_int(&config.video.widescreen);
        lr_options_set_frontend_variable_int(&config.video.hires);
        lr_options_set_frontend_variable_int(&config.engine.level_objects);
        lr_options_set_frontend_variable_int(&config.engine.new_attract);
        lr_options_set_frontend_variable_bool(&config.engine.fix_bugs);
        lr_options_set_frontend_variable_int(&config.sound.preview);
    }
    /* Otherwise, use whatever is already setup... */
    else
    {
        config.engine.fix_bugs = config.engine.fix_bugs_backup;
    }

    if (Menu_check_jap_roms(self))
    {
        outrun.cannonball_mode = mode;
        cannonball_state = STATE_INIT_GAME;
        OSoundInt_queue_clear(&osoundint);
    }
}
