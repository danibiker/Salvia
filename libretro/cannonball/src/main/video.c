/***************************************************************************
    Video Rendering. 
    
    - Renders the System 16 Video Layers
    - Handles Reads and Writes to these layers from the main game code
    - Interfaces with platform specific rendering code

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdlib.h>
#include "video.h"
#include "globals.h"
#include "frontend/config.h"
#include "engine/oroad.h"

#include <libretro.h>
#include "setup.h"

static void Video_refresh_palette(Video* self, uint32_t);

extern retro_video_refresh_t video_cb;

#define Rshift 11
#define Gshift 6
#define Bshift 0
#define CURRENT_RGB() ((r << Rshift) | (g << Gshift) | (b << Bshift))

Video video;

void Video_ctor(Video* self)
{
    self->pixels       = NULL;
    self->sprite_layer = (hwsprites*)calloc(1, sizeof(hwsprites));
    self->tile_layer = (hwtiles*)malloc(sizeof(hwtiles));
    hwtiles_ctor(self->tile_layer);

    Video_set_shadow_intensity(self, SHADOW_ORIGINAL);
    self->enabled      = false;
}

void Video_dtor(Video* self)
{
    free(self->sprite_layer);
    free(self->tile_layer);
    if (self->pixels) free(self->pixels);
}

int Video_init(Video* self, Roms* roms, video_settings_t* settings)
{
    if (!Video_set_video_mode(self, settings))
        return 0;

    /* Internal pixel array. The size of this is always constant */
    if (self->pixels) free(self->pixels);
    self->pixels = (uint16_t*)malloc((config.s16_width * config.s16_height) * sizeof(uint16_t));

    /* Convert S16 tiles to a more useable format */
    hwtiles_init(self->tile_layer, roms->tiles.rom, config.video.hires != 0);
    
    Video_clear_tile_ram(self);
    Video_clear_text_ram(self);
    if (roms->tiles.rom)
    {
        free(roms->tiles.rom);
        roms->tiles.rom = NULL;
    }

    /* Convert S16 sprites */
    hwsprites_init(self->sprite_layer, roms->sprites.rom);
    if (roms->sprites.rom)
    {
        free(roms->sprites.rom);
        roms->sprites.rom = NULL;
    }

    /* Convert S16 Road Stuff */
    HWRoad_init(&hwroad, roms->road.rom, config.video.hires != 0);
    if (roms->road.rom)
    {
        free(roms->road.rom);
        roms->road.rom = NULL;
    }

    self->enabled = true;
    return 1;
}

void Video_disable(Video* self)
{
    self->enabled = false;
}

/* ------------------------------------------------------------------------------------------------ */
/* Configure video settings from config file */
/* ------------------------------------------------------------------------------------------------ */

int Video_set_video_mode(Video* self, video_settings_t* settings)
{
    if (settings->widescreen)
    {
        config.s16_width  = S16_WIDTH_WIDE;
        config.s16_x_off = (S16_WIDTH_WIDE - S16_WIDTH) / 2;
    }
    else
    {
        config.s16_width = S16_WIDTH;
        config.s16_x_off = 0;
    }

    config.s16_height = S16_HEIGHT;

    /* Internal video buffer is doubled in hi-res mode. */
    if (settings->hires)
    {
        config.s16_width  <<= 1;
        config.s16_height <<= 1;
    }

    if (settings->scanlines < 0) settings->scanlines = 0;
    else if (settings->scanlines > 100) settings->scanlines = 100;

    if (settings->scale < 1)
        settings->scale = 1;

    Video_set_shadow_intensity(self, settings->shadow == 0 ? SHADOW_ORIGINAL : SHADOW_MAME);

    return 1;
}

/* -------------------------------------------------------------------------------------------- */
/* Shadow Colours. */
/* 63% Intensity is the correct value derived from hardware as follows: */
/* */
/* 1/ Shadows are just an extra 220 ohm resistor that goes to ground when enabled. */
/* 2/ This is in parallel with the resistor-"DAC" (3.9k, 2k, 1k, 0.5k, 0.25k), */
/*    and otherwise left floating. */
/* */
/* Static calculation example: */
/* */
/* const float rDAC   = 1.f / (1.f/3900.f + 1.f/2000.f + 1.f/1000.f + 1.f/500.f + 1.f/250.f); */
/* const float rShade = 220.f; */
/* const float shadeAttenuation = rShade / (rShade + rDAC); // 0.63f */
/* */
/* (MAME uses an incorrect value which is closer to 78% Intensity) */
/* -------------------------------------------------------------------------------------------- */

void Video_set_shadow_intensity(Video* self, float f)
{
    self->shadow_multi = (uint32_t) (255.0f * f + 0.5f);
}

void Video_prepare_frame(Video* self)
{
    if (!self->pixels)
        return;

    if (!self->enabled)
    {
        /* Fill with black pixels */
        { int i; for (i = 0; i < config.s16_width * config.s16_height; i++)
            self->pixels[i] = 0; }
    }
    else
    {
        /* OutRun Hardware Video Emulation */
        hwtiles_update_tile_values(self->tile_layer);

        (hwroad.render_background)(&hwroad, self->pixels);
        hwtiles_render_tile_layer(self->tile_layer, self->pixels, 1, 0);      /* background layer */
        hwtiles_render_tile_layer(self->tile_layer, self->pixels, 0, 0);      /* foreground layer */

        if (!config.engine.fix_bugs || oroad.horizon_base != HORIZON_OFF)
            (hwroad.render_foreground)(&hwroad, self->pixels);
        hwsprites_render(self->sprite_layer, 8);
        hwtiles_render_text_layer(self->tile_layer, self->pixels, 1);
     }
}

void Video_render_frame(Video* self)
{
    uint16_t* output = self->pixels;

    { int i; for (i = 0;
         i < config.s16_width * config.s16_height;
         i++)
        output[i] = (uint16_t)
            self->rgb[output[i] % (S16_PALETTE_ENTRIES * 3)]; }

    video_cb(
        self->pixels,
        config.s16_width,
        config.s16_height,
        config.s16_width << 1
    );
}

/* --------------------------------------------------------------------------- */
/* Text Handling Code */
/* --------------------------------------------------------------------------- */

void Video_clear_text_ram(Video* self)
{
    { uint32_t i; for (i = 0; i <= 0xFFF; i++)
        self->tile_layer->text_ram[i] = 0; }
}

void Video_write_text8(Video* self, uint32_t addr, const uint8_t data)
{
    self->tile_layer->text_ram[addr & 0xFFF] = data;
}

void Video_write_text16_a(Video* self, uint32_t* addr, const uint16_t data)
{
    self->tile_layer->text_ram[*addr & 0xFFF] = (data >> 8) & 0xFF;
    self->tile_layer->text_ram[(*addr+1) & 0xFFF] = data & 0xFF;

    *addr += 2;
}

void Video_write_text16(Video* self, uint32_t addr, const uint16_t data)
{
    self->tile_layer->text_ram[addr & 0xFFF] = (data >> 8) & 0xFF;
    self->tile_layer->text_ram[(addr+1) & 0xFFF] = data & 0xFF;
}

void Video_write_text32_a(Video* self, uint32_t* addr, const uint32_t data)
{
    self->tile_layer->text_ram[*addr & 0xFFF] = (data >> 24) & 0xFF;
    self->tile_layer->text_ram[(*addr+1) & 0xFFF] = (data >> 16) & 0xFF;
    self->tile_layer->text_ram[(*addr+2) & 0xFFF] = (data >> 8) & 0xFF;
    self->tile_layer->text_ram[(*addr+3) & 0xFFF] = data & 0xFF;

    *addr += 4;
}

void Video_write_text32(Video* self, uint32_t addr, const uint32_t data)
{
    self->tile_layer->text_ram[addr & 0xFFF] = (data >> 24) & 0xFF;
    self->tile_layer->text_ram[(addr+1) & 0xFFF] = (data >> 16) & 0xFF;
    self->tile_layer->text_ram[(addr+2) & 0xFFF] = (data >> 8) & 0xFF;
    self->tile_layer->text_ram[(addr+3) & 0xFFF] = data & 0xFF;
}

uint8_t Video_read_text8(Video* self, uint32_t addr)
{
    return self->tile_layer->text_ram[addr & 0xFFF];
}

/* --------------------------------------------------------------------------- */
/* Tile Handling Code */
/* --------------------------------------------------------------------------- */

void Video_clear_tile_ram(Video* self)
{
    { uint32_t i; for (i = 0; i <= 0xFFFF; i++)
        self->tile_layer->tile_ram[i] = 0; }
}

void Video_write_tile8(Video* self, uint32_t addr, const uint8_t data)
{
    self->tile_layer->tile_ram[addr & 0xFFFF] = data;
} 

void Video_write_tile16_a(Video* self, uint32_t* addr, const uint16_t data)
{
    self->tile_layer->tile_ram[*addr & 0xFFFF] = (data >> 8) & 0xFF;
    self->tile_layer->tile_ram[(*addr+1) & 0xFFFF] = data & 0xFF;

    *addr += 2;
}

void Video_write_tile16(Video* self, uint32_t addr, const uint16_t data)
{
    self->tile_layer->tile_ram[addr & 0xFFFF] = (data >> 8) & 0xFF;
    self->tile_layer->tile_ram[(addr+1) & 0xFFFF] = data & 0xFF;
}   

void Video_write_tile32_a(Video* self, uint32_t* addr, const uint32_t data)
{
    self->tile_layer->tile_ram[*addr & 0xFFFF] = (data >> 24) & 0xFF;
    self->tile_layer->tile_ram[(*addr+1) & 0xFFFF] = (data >> 16) & 0xFF;
    self->tile_layer->tile_ram[(*addr+2) & 0xFFFF] = (data >> 8) & 0xFF;
    self->tile_layer->tile_ram[(*addr+3) & 0xFFFF] = data & 0xFF;

    *addr += 4;
}

void Video_write_tile32(Video* self, uint32_t addr, const uint32_t data)
{
    self->tile_layer->tile_ram[addr & 0xFFFF] = (data >> 24) & 0xFF;
    self->tile_layer->tile_ram[(addr+1) & 0xFFFF] = (data >> 16) & 0xFF;
    self->tile_layer->tile_ram[(addr+2) & 0xFFFF] = (data >> 8) & 0xFF;
    self->tile_layer->tile_ram[(addr+3) & 0xFFFF] = data & 0xFF;
}

uint8_t Video_read_tile8(Video* self, uint32_t addr)
{
    return self->tile_layer->tile_ram[addr & 0xFFFF];
}


/* --------------------------------------------------------------------------- */
/* Sprite Handling Code */
/* --------------------------------------------------------------------------- */

void Video_write_sprite16(Video* self, uint32_t* addr, const uint16_t data)
{
    hwsprites_write(self->sprite_layer, *addr & 0xfff, data);
    *addr += 2;
}

/* --------------------------------------------------------------------------- */
/* Palette Handling Code */
/* --------------------------------------------------------------------------- */

void Video_write_pal8(Video* self, uint32_t* palAddr, const uint8_t data)
{
    self->palette[*palAddr & 0x1fff] = data;
    Video_refresh_palette(self, *palAddr & 0x1fff);
    *palAddr += 1;
}

void Video_write_pal16(Video* self, uint32_t* palAddr, const uint16_t data)
{    
    uint32_t adr = *palAddr & 0x1fff;
    self->palette[adr]   = (data >> 8) & 0xFF;
    self->palette[adr+1] = data & 0xFF;
    Video_refresh_palette(self, adr);
    *palAddr += 2;
}

void Video_write_pal32_a(Video* self, uint32_t* palAddr, const uint32_t data)
{    
    uint32_t adr = *palAddr & 0x1fff;

    self->palette[adr]   = (data >> 24) & 0xFF;
    self->palette[adr+1] = (data >> 16) & 0xFF;
    self->palette[adr+2] = (data >> 8) & 0xFF;
    self->palette[adr+3] = data & 0xFF;

    Video_refresh_palette(self, adr);
    Video_refresh_palette(self, adr+2);

    *palAddr += 4;
}

void Video_write_pal32(Video* self, uint32_t adr, const uint32_t data)
{    
    adr &= 0x1fff;

    self->palette[adr]   = (data >> 24) & 0xFF;
    self->palette[adr+1] = (data >> 16) & 0xFF;
    self->palette[adr+2] = (data >> 8) & 0xFF;
    self->palette[adr+3] = data & 0xFF;
    Video_refresh_palette(self, adr);
    Video_refresh_palette(self, adr+2);
}

uint8_t Video_read_pal8(Video* self, uint32_t palAddr)
{
    return self->palette[palAddr & 0x1fff];
}

uint16_t Video_read_pal16(Video* self, uint32_t palAddr)
{
    uint32_t adr = palAddr & 0x1fff;
    return (self->palette[adr] << 8) | self->palette[adr+1];
}

uint16_t Video_read_pal16_a(Video* self, uint32_t* palAddr)
{
    uint32_t adr = *palAddr & 0x1fff;
    *palAddr += 2;
    return (self->palette[adr] << 8)| self->palette[adr+1];
}

uint32_t Video_read_pal32(Video* self, uint32_t* palAddr)
{
    uint32_t adr = *palAddr & 0x1fff;
    *palAddr += 4;
    return (self->palette[adr] << 24) | (self->palette[adr+1] << 16) | (self->palette[adr+2] << 8) | self->palette[adr+3];
}

/* Convert internal System 16 RRRR GGGG BBBB format palette to renderer output format */static 
void Video_refresh_palette(Video* self, uint32_t palAddr)
{
    palAddr &= ~1;
    { uint32_t a = (self->palette[palAddr] << 8) | self->palette[palAddr + 1];
    uint32_t r = (a & 0x000f) << 1; /* r rrr0 */
    uint32_t g = (a & 0x00f0) >> 3; /* g ggg0 */
    uint32_t b = (a & 0x0f00) >> 7; /* b bbb0 */
    if ((a & 0x1000) != 0)
        r |= 1; /* r rrrr */
    if ((a & 0x2000) != 0)
        g |= 1; /* g gggg */
    if ((a & 0x4000) != 0)
        b |= 1; /* b bbbb */

    palAddr >>= 1;

    self->rgb[palAddr] = CURRENT_RGB();

    r = r * self->shadow_multi / 255;
    g = g * self->shadow_multi / 255;
    b = b * self->shadow_multi / 255;

    self->rgb[palAddr + S16_PALETTE_ENTRIES] =
        CURRENT_RGB();

    /* Conserva il comportamento del core precedente */
    /* qualora venga usato il terzo banco della palette. */
    self->rgb[palAddr + (S16_PALETTE_ENTRIES * 2)] =
        CURRENT_RGB();
 }}
