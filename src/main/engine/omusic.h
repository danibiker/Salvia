/***************************************************************************
    Music Selection Screen.

    This is a combination of a tilemap and overlayed sprites.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

struct RomLoader;

enum { HAND_LEFT = 0, HAND_CENTRE = 1, HAND_RIGHT = 2 };

typedef struct OMusic
{
    RomLoader* tilemap;
    RomLoader* tile_patch;
    music_t* next_track;
    uint8_t music_selected;
    int total_tracks;
    int cursor_pos;
    uint16_t entry_start;
    int16_t last_music_selected;
    int8_t preview_counter;
} OMusic;

extern OMusic omusic;

void OMusic_ctor(OMusic* self);

void OMusic_dtor(OMusic* self);

bool OMusic_load_widescreen_map(OMusic* self);

void OMusic_enable(OMusic* self);

void OMusic_disable(OMusic* self);

void OMusic_tick(OMusic* self);

void OMusic_blit(OMusic* self);

void OMusic_check_start(OMusic* self);

void OMusic_play_music(OMusic* self, int index);

void OMusic_cycle_music(OMusic* self);

extern OMusic omusic;

#ifdef __cplusplus
}
#endif
