/***************************************************************************
    XML Configuration File Handling.

    Load Settings.
    Load & Save Hi-Scores.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <boolean.h>

#include <stdint.h>

typedef struct data_settings_t
{
    char res_path[512];
} data_settings_t;

enum { IS_YM_INT = 0, IS_YM_EXT = 1, IS_WAV = 2 };

typedef struct music_t
{
    int type;
    int cmd;
    char title[32];
    char filename[64];
} music_t;


typedef struct ttrial_settings_t
{
    int laps;
    int traffic;
    uint16_t best_times[15];
} ttrial_settings_t;

typedef struct menu_settings_t
{
    int enabled;
    int road_scroll_speed;
} menu_settings_t;

enum { MODE_WINDOW = 0, MODE_FULL = 1, MODE_STRETCH = 2 };

typedef struct video_settings_t
{

    int mode;
    int scale;
    int scanlines;
    int widescreen;
    int fps;
    int fps_count;
    int hires;
    int filtering;
    int shadow;
} video_settings_t;

typedef struct sound_settings_t
{
    int enabled;
    int rate;
    int advertise;
    int preview;
    int fix_samples;
    int music_timer;
    music_t music[8];
    int     music_num;
} sound_settings_t;

enum { GEAR_BUTTON = 0, GEAR_PRESS = 1, GEAR_SEPARATE = 2, GEAR_AUTO = 3 };

typedef struct controls_settings_t
{

    int gear;
    int steer_speed;   /* Steering Digital Speed */
    int pedal_speed;   /* Pedal Digital Speed */
    int padconfig[8];  /* Joypad Button Config */
    int keyconfig[12]; /* Keyboard Button Config */
    int pad_id;        /* Use the N'th joystick on the system. */
    int analog;        /* Use analog controls */
    int axis[3];       /* Analog Axis */
    int asettings[3];  /* Analog Settings */

    int haptic;        /* Force Feedback Enabled */
    int max_force;
    int min_force;
    int force_duration;
} controls_settings_t;

typedef struct engine_settings_t
{
    int dip_time;
    int dip_traffic;
    bool freeplay;
    bool freeze_timer;
    bool disable_traffic;
    int jap;
    int prototype;
    int randomgen;
    int level_objects;
    bool fix_bugs;
    bool fix_bugs_backup;
    bool fix_timer;
    bool layout_debug;
    bool force_ai;
    bool hiscore_delete;
    int hiscore_timer;
    int new_attract;
    bool grippy_tyres;
    bool offroad;
    bool bumper;
    bool turbo;
    int car_pal;
    
} engine_settings_t;

typedef struct Config
{
    data_settings_t        data;
    menu_settings_t        menu;
    video_settings_t       video;
    sound_settings_t       sound;
    controls_settings_t    controls;
    engine_settings_t      engine;
    ttrial_settings_t      ttrial;
    uint16_t s16_width, s16_height;
    uint16_t s16_x_off;
    int fps;
    int tick_fps;
    int cont_traffic;
} Config;

extern Config config;

void Config_ctor(Config* self);

void Config_init(Config* self);


void Config_load_scores(Config* self, const char* filename);

void Config_save_scores(Config* self, const char* filename);

void Config_load_tiletrial_scores(Config* self);

void Config_save_tiletrial_scores(Config* self);

bool Config_clear_scores(Config* self);

void Config_set_fps(Config* self, int fps);

#ifdef __cplusplus
}
#endif
