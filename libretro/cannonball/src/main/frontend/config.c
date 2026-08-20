/***************************************************************************
    XML Configuration File Handling.

    Load & Save Hi-Scores.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdio.h>
#include <compat/msvc.h>
#include <string.h> /* remove() */

#include <streams/file_stream.h>

#include <libretro.h>

#include "../main.h"
#include "config.h"
#include "../globals.h"
#include "../setup.h"
#include "../utils.h"

#include "../engine/ohiscore.h"

enum { COUNTER_1M_15 = 0x11D0 };
#include "../engine/audio/osoundint.h"

extern retro_log_printf_t                 log_cb;

Config config;

void Config_ctor(Config* self)
{
    /* Safe defaults required before Config_set_fps(Config* self) initializes audio. */
    self->sound.rate        = 44100;
    self->sound.music_timer = MUSIC_TIMER;
    self->video.shadow      = 0;
    self->engine.hiscore_delete = true;
    self->engine.hiscore_timer  = HIGHSCORE_TIMER;
    self->engine.grippy_tyres   = false;
    self->engine.offroad        = false;
    self->engine.bumper         = false;
    self->engine.turbo          = false;
    self->engine.car_pal        = 0;

    self->sound.music[0].type = IS_YM_INT;
    self->sound.music[0].cmd  = SOUND_MUSIC_MAGICAL;
    strcpy(self->sound.music[0].title, "MAGICAL SOUND SHOWER");
    self->sound.music[0].filename[0] = 0;

    self->sound.music[1].type = IS_YM_INT;
    self->sound.music[1].cmd  = SOUND_MUSIC_BREEZE;
    strcpy(self->sound.music[1].title, "PASSING BREEZE");
    self->sound.music[1].filename[0] = 0;

    self->sound.music[2].type = IS_YM_INT;
    self->sound.music[2].cmd  = SOUND_MUSIC_SPLASH;
    strcpy(self->sound.music[2].title, "SPLASH WAVE");
    self->sound.music[2].filename[0] = 0;

    self->sound.music_num = 3;
}



static RFILE* config_open_read(const char* filename)
{
    char path[600];
    snprintf(path, sizeof(path), "%s.sav", filename);
    return filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
                           RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static RFILE* config_open_write(const char* filename)
{
    char path[600];
    snprintf(path, sizeof(path), "%s.sav", filename);
    return filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
                           RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

/* High scores are persisted as a small binary blob in the save */
/* directory. Every field is stored little-endian behind a 4-byte magic */
/* and 1-byte version, so a .sav is portable across platforms regardless */
/* of host endianness or struct padding. Settings themselves come from */
/* the libretro core options, so no XML configuration is loaded/written. */

#define CONFIG_SAV_VERSION 1

static void config_write_u8(RFILE* f, uint8_t v)
{
    filestream_write(f, &v, 1);
}

static void config_write_u16(RFILE* f, uint16_t v)
{
    uint8_t b[2];
    b[0] = (uint8_t)(v & 0xff);
    b[1] = (uint8_t)((v >> 8) & 0xff);
    filestream_write(f, b, 2);
}

static void config_write_u32(RFILE* f, uint32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)(v & 0xff);
    b[1] = (uint8_t)((v >> 8) & 0xff);
    b[2] = (uint8_t)((v >> 16) & 0xff);
    b[3] = (uint8_t)((v >> 24) & 0xff);
    filestream_write(f, b, 4);
}

static uint8_t config_read_u8(RFILE* f)
{
    uint8_t v = 0;
    filestream_read(f, &v, 1);
    return v;
}

static uint16_t config_read_u16(RFILE* f)
{
    uint8_t b[2];
    b[0] = 0; b[1] = 0;
    filestream_read(f, b, 2);
    return (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}

static uint32_t config_read_u32(RFILE* f)
{
    uint8_t b[4];
    b[0] = 0; b[1] = 0; b[2] = 0; b[3] = 0;
    filestream_read(f, b, 4);
    return  (uint32_t)b[0]        | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static void config_write_header(RFILE* f, char m0, char m1, char m2, char m3)
{
    config_write_u8(f, (uint8_t)m0);
    config_write_u8(f, (uint8_t)m1);
    config_write_u8(f, (uint8_t)m2);
    config_write_u8(f, (uint8_t)m3);
    config_write_u8(f, CONFIG_SAV_VERSION);
}

static bool config_check_header(RFILE* f, char m0, char m1, char m2, char m3)
{
    uint8_t h[5];
    h[0] = 0; h[1] = 0; h[2] = 0; h[3] = 0; h[4] = 0;
    filestream_read(f, h, 5);
    return h[0] == (uint8_t)m0 && h[1] == (uint8_t)m1 &&
           h[2] == (uint8_t)m2 && h[3] == (uint8_t)m3 &&
           h[4] == CONFIG_SAV_VERSION;
}

void Config_load_scores(Config* self, const char* filename)
{
    int    i;
    RFILE* f = config_open_read(filename);
    if (!f)
        return;
    if (!config_check_header(f, 'C', 'B', 'H', 'S'))
    {
        filestream_close(f);
        return;
    }
    for (i = 0; i < NO_SCORES; i++)
    {
        score_entry* e = &ohiscore.scores[i];
        e->score    = config_read_u32(f);
        e->initial1 = config_read_u8(f);
        e->initial2 = config_read_u8(f);
        e->initial3 = config_read_u8(f);
        e->maptiles = config_read_u32(f);
        e->time     = config_read_u16(f);
    }
    filestream_close(f);
}

void Config_save_scores(Config* self, const char* filename)
{
    int    i;
    RFILE* f = config_open_write(filename);
    if (!f)
        return;
    config_write_header(f, 'C', 'B', 'H', 'S');
    for (i = 0; i < NO_SCORES; i++)
    {
        score_entry* e = &ohiscore.scores[i];
        config_write_u32(f, e->score);
        config_write_u8(f, e->initial1);
        config_write_u8(f, e->initial2);
        config_write_u8(f, e->initial3);
        config_write_u32(f, e->maptiles);
        config_write_u16(f, e->time);
    }
    filestream_close(f);
}

void Config_load_tiletrial_scores(Config* self)
{
    int    i;
    RFILE* f = config_open_read(FILENAME_TTRIAL);
    if (f && config_check_header(f, 'C', 'B', 'T', 'T'))
    {
        for (i = 0; i < 15; i++)
            self->ttrial.best_times[i] = config_read_u16(f);
        filestream_close(f);
        return;
    }
    if (f)
        filestream_close(f);
    for (i = 0; i < 15; i++)
        self->ttrial.best_times[i] = COUNTER_1M_15;
}

void Config_save_tiletrial_scores(Config* self)
{
    int    i;
    RFILE* f = config_open_write(FILENAME_TTRIAL);
    if (!f)
        return;
    config_write_header(f, 'C', 'B', 'T', 'T');
    for (i = 0; i < 15; i++)
        config_write_u16(f, self->ttrial.best_times[i]);
    filestream_close(f);
}

bool Config_clear_scores(Config* self)
{
    char path[600];
    bool files_removed = false;

    OHiScore_init_def_scores(&ohiscore);

    /* remove() returns 0 on success */
    snprintf(path, sizeof(path), "%s.sav", FILENAME_SCORES);
    if (!remove(path))
        files_removed = true;
    snprintf(path, sizeof(path), "%s.sav", FILENAME_TTRIAL);
    if (!remove(path))
        files_removed = true;
    snprintf(path, sizeof(path), "%s.sav", FILENAME_CONT);
    if (!remove(path))
        files_removed = true;

    return files_removed;
}

void Config_set_fps(Config* self, int fps)
{
    self->video.fps = fps;
    /* Set core FPS to 30fps, 60fps or 120fps */
    if (self->video.fps == 0)
        self->fps = 30;
    else if (self->video.fps == 3)
        self->fps = 120;
    else
        self->fps = 60;
    
    /* Original game ticks sprites at 30fps but background scroll at 60fps */
    if (self->video.fps == 3)
        self->tick_fps = 120;
    else if (self->video.fps < 2)
        self->tick_fps = 30;
    else
        self->tick_fps = 60;

    if (config.sound.enabled)
        Audio_stop_audio(&cannonball_audio);
    OSoundInt_init(&osoundint);
    if (config.sound.enabled)
        Audio_start_audio(&cannonball_audio);
}
