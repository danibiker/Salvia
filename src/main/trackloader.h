/***************************************************************************
    Track Loading Code.

    Abstracts the level format, so that the original ROMs as well as
    in conjunction with track data from the LayOut editor.

    - Handles levels (path, width, height, scenery)
    - Handles additional level sections (road split, end section)
    - Handles road/level related palettes
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "globals.h"

/* Road Generator Palette Representation */
typedef struct RoadPalette
{
    uint32_t stripe_centre;   /* Centre Stripe Colour */
    uint32_t stripe;          /* Stripe Colour */
    uint32_t side;            /* Side Colour */
    uint32_t road;            /* Main Road Colour */
} RoadPalette;

/* OutRun Level Representation */
typedef struct Level
{
    uint8_t* path;            /* CPU 1 Path Data */
    uint8_t* curve;           /* Track Curve Information (Derived From Path) */
    uint8_t* width_height;    /* Track Width & Height Lookups */
    uint8_t* scenery;         /* Track Scenery Lookups */

    uint16_t pal_sky;         /* Index into Sky Palettes */
    uint16_t pal_gnd;         /* Index into Ground Palettes */

    RoadPalette palr1;        /* Road 1 Generator Palette */
    RoadPalette palr2;        /* Road 2 Generator Palette */
} Level;

/* LayOut Binary Header Format (offsets) */
enum {
    EXPECTED_VERSION = 1,
    HEADER = 0,
    PATH = HEADER      + sizeof(uint32_t) + sizeof(uint8_t),
    LEVELS = PATH        + sizeof(uint32_t),
    END_PATH = LEVELS      + (STAGES * sizeof(uint32_t)),
    END_LEVELS = END_PATH    + sizeof(uint32_t),
    SPLIT_PATH = END_LEVELS  + (5 * sizeof(uint32_t)),
    SPLIT_LEVEL = SPLIT_PATH  + sizeof(uint32_t),
    PAL_SKY = SPLIT_LEVEL + sizeof(uint32_t),
    PAL_GND = PAL_SKY     + sizeof(uint32_t),
    SPRITE_MAPS = PAL_GND     + sizeof(uint32_t),
    HEIGHT_MAPS = SPRITE_MAPS + sizeof(uint32_t)
};


struct RomLoader;

int32_t TrackLoader_read32_a(uint8_t* data, uint32_t* addr);
int32_t TrackLoader_read32(uint8_t* data, uint32_t addr);
int16_t TrackLoader_read16_a(uint8_t* data, uint32_t* addr);
int16_t TrackLoader_read16(uint8_t* data, uint32_t addr);
int8_t TrackLoader_read8_a(uint8_t* data, uint32_t* addr);
int8_t TrackLoader_read8(uint8_t* data, uint32_t addr);

#define TL_MODE_ORIGINAL (0)

#define TL_MODE_LAYOUT (1)

typedef struct TrackLoader
{
    uint8_t* stage_data;
    Level* current_level;
    uint8_t display_start_line;
    uint32_t curve_offset;
    uint32_t wh_offset;
    uint32_t scenery_offset;
    uint8_t* pal_sky_data;
    uint8_t* pal_gnd_data;
    uint8_t* heightmap_data;
    uint8_t* scenerymap_data;
    uint32_t pal_sky_offset;
    uint32_t pal_gnd_offset;
    uint32_t heightmap_offset;
    uint32_t scenerymap_offset;
    struct RomLoader* layout;
    int mode;
    Level* levels;
    Level* level_split;
    Level* levels_end;
    uint8_t* current_path;
} TrackLoader;

extern TrackLoader trackloader;

void TrackLoader_ctor(TrackLoader* self);

void TrackLoader_dtor(TrackLoader* self);

void TrackLoader_init(TrackLoader* self, bool jap);

void TrackLoader_init_original_tracks(TrackLoader* self, bool jap);

void TrackLoader_init_layout_tracks(TrackLoader* self, bool jap);

void TrackLoader_init_track(TrackLoader* self, const uint32_t);

void TrackLoader_init_track_split(TrackLoader* self);

void TrackLoader_init_track_bonus(TrackLoader* self, const uint32_t);

void TrackLoader_init_path(TrackLoader* self, const uint32_t);

void TrackLoader_init_path_split(TrackLoader* self);

void TrackLoader_init_path_end(TrackLoader* self);

uint32_t TrackLoader_read_pal_sky_table(TrackLoader* self, uint16_t entry);

uint32_t TrackLoader_read_pal_gnd_table(TrackLoader* self, uint16_t entry);

uint32_t TrackLoader_read_heightmap_table(TrackLoader* self, uint16_t entry);

uint32_t TrackLoader_read_scenerymap_table(TrackLoader* self, uint16_t entry);

int16_t TrackLoader_readPath(TrackLoader* self, uint32_t addr);

int16_t TrackLoader_readPath_a(TrackLoader* self, uint32_t* addr);

int16_t TrackLoader_read_width_height(TrackLoader* self, uint32_t* addr);

int16_t TrackLoader_read_curve(TrackLoader* self, uint32_t addr);

uint16_t TrackLoader_read_scenery_pos(TrackLoader* self);

uint8_t TrackLoader_read_total_sprites(TrackLoader* self);

uint8_t TrackLoader_read_sprite_pattern_index(TrackLoader* self);

int8_t TrackLoader_stage_offset_to_level(TrackLoader* self, uint32_t);

Level* TrackLoader_get_level(TrackLoader* self, uint32_t);

extern TrackLoader trackloader;

#ifdef __cplusplus
}
#endif
