/***************************************************************************
    Tilemap Handling Code. 

    Logic for the foreground and background tilemap layers.

    - Read and render tilemaps
    - H-Scroll & V-Scroll
    - Palette Initialization
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

/* Forward definition of video for cyclic dependency */
struct video;

enum { TILEMAP_CLEAR, TILEMAP_SCROLL, TILEMAP_INIT, TILEMAP_SPLIT };

enum {VSWAP_OFF, VSWAP_SCROLL_OFF, VSWAP_SCROLL_ON};

enum { SETUP_TILES, SETUP_PAL };

typedef struct OTiles
{
    uint8_t tilemap_ctrl;
    int8_t page;
    int16_t vswap_state;
    int16_t vswap_off;
    int16_t fg_h_scroll;
    int16_t bg_h_scroll;
    int16_t fg_v_scroll;
    int16_t bg_v_scroll;
    uint16_t fg_psel;
    uint16_t bg_psel;
    int16_t tilemap_v_scr;
    int32_t tilemap_h_scr;
    uint16_t fg_v_tiles;
    uint16_t bg_v_tiles;
    int16_t tilemap_v_off;
    uint32_t fg_addr;
    uint32_t bg_addr;
    uint8_t tilemap_setup;
    bool clear_name_tables;
    bool page_split;
    uint16_t h_scroll_lookup;
} OTiles;

extern OTiles otiles;

void OTiles_init(OTiles* self);

void OTiles_set_vertical_swap(OTiles* self);

void OTiles_setup_palette_tilemap(OTiles* self);

void OTiles_setup_palette_widescreen(OTiles* self);

void OTiles_setup_palette_hud(OTiles* self);

void OTiles_reset_tiles_pal(OTiles* self);

void OTiles_update_tilemaps(OTiles* self, int8_t);

void OTiles_init_tilemap_palette(OTiles* self, uint16_t);

void OTiles_fill_tilemap_color(OTiles* self, uint16_t);

void OTiles_write_tilemap_hw(OTiles* self);

void OTiles_set_scroll(OTiles* self, int16_t h_scroll, int16_t v_scroll);

extern OTiles otiles;

#ifdef __cplusplus
}
#endif
