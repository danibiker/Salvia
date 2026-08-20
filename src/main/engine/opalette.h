/***************************************************************************
    Palette Control

    Sky, Ground & Road Palettes. Including palette cycling code.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

typedef struct OPalette
{
    uint8_t pal_manip_ctrl;
    uint32_t pal_manip[(0x20 * 0x1F) * 2];
    uint16_t pal_fade[9 * 0x18];
    uint8_t sky_palette_init;
    uint8_t cycle_counter;
    int16_t fade_counter;
    uint16_t sky_palette_index;
    uint8_t sky_fade_offset;
} OPalette;

extern OPalette opalette;

void OPalette_init(OPalette* self);

void OPalette_setup_sky_palette(OPalette* self);

void OPalette_setup_sky_change(OPalette* self);

void OPalette_setup_sky_cycle(OPalette* self);

void OPalette_cycle_sky_palette(OPalette* self);

void OPalette_fade_palette(OPalette* self);

void OPalette_setup_ground_color(OPalette* self);

void OPalette_setup_road_centre(OPalette* self);

void OPalette_setup_road_stripes(OPalette* self);

void OPalette_setup_road_side(OPalette* self);

void OPalette_setup_road_colour(OPalette* self);

#ifdef __cplusplus
}
#endif
