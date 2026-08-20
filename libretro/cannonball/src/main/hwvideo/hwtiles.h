#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

struct RomLoader;

enum {
        CLAMP_LEFT,
        CLAMP_RIGHT,
        CLAMP_CENTRE
    };

enum { TILES_LENGTH = 0x10000 };

#define NUM_TILES (0x2000)

#define TILEMAP_COLOUR_OFFSET (0x1c00)

typedef struct hwtiles
{
    uint8_t text_ram[0x1000];
    uint8_t tile_ram[0x10000];
    int16_t x_clamp;
    uint16_t s16_width_noscale;
    uint32_t tiles[TILES_LENGTH];
    uint32_t tiles_backup[TILES_LENGTH];
    uint16_t page[4];
    uint16_t scroll_x[4];
    uint16_t scroll_y[4];
    uint8_t tile_banks[2];
    void (*render8x8_tile_mask)(struct hwtiles* self, 
        uint16_t *buf,
        uint16_t nTileNumber, 
        uint16_t StartX, 
        uint16_t StartY, 
        uint16_t nTilePalette, 
        uint16_t nColourDepth, 
        uint16_t nMaskColour, 
        uint16_t nPaletteOffset);
    void (*render8x8_tile_mask_clip)(struct hwtiles* self, 
        uint16_t *buf,
        uint16_t nTileNumber, 
        int16_t StartX, 
        int16_t StartY, 
        uint16_t nTilePalette, 
        uint16_t nColourDepth, 
        uint16_t nMaskColour, 
        uint16_t nPaletteOffset);
} hwtiles;

void hwtiles_ctor(struct hwtiles* self);

void hwtiles_init(struct hwtiles* self, uint8_t* src_tiles, const bool hires);

void hwtiles_patch_tiles(struct hwtiles* self, RomLoader* patch);

void hwtiles_restore_tiles(struct hwtiles* self);

void hwtiles_set_x_clamp(struct hwtiles* self, const uint16_t);

void hwtiles_update_tile_values(struct hwtiles* self);

void hwtiles_render_tile_layer(struct hwtiles* self, uint16_t*, uint8_t, uint8_t);

void hwtiles_render_text_layer(struct hwtiles* self, uint16_t*, uint8_t);

void hwtiles_render_all_tiles(struct hwtiles* self, uint16_t*);

#ifdef __cplusplus
}
#endif
