/***************************************************************************
    Music Selection Screen.

    This is a combination of a tilemap and overlayed sprites.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <compat/msvc.h>
#include "main.h"
#include "engine/oferrari.h"
#include "engine/ohud.h"
#include "engine/oinputs.h"
#include "engine/ologo.h"
#include "engine/omusic.h"
#include "engine/tilemap_data.h"
#include "engine/otiles.h"
#include "engine/otraffic.h"
#include "engine/ostats.h"

static void OMusic_setup_sprite1(OMusic* self);
static void OMusic_setup_sprite2(OMusic* self);
static void OMusic_setup_sprite3(OMusic* self);
static void OMusic_setup_sprite4(OMusic* self);
static void OMusic_setup_sprite5(OMusic* self);
static void OMusic_tick_original(OMusic* self, oentry*, oentry*, oentry*);
static void OMusic_tick_enhanced(OMusic* self, oentry*, oentry*, oentry*);
static void OMusic_set_hand(OMusic* self, short, oentry*, oentry*, oentry*);
static void OMusic_blit_music_select(OMusic* self);

OMusic omusic;

void OMusic_ctor(OMusic* self)
{
    self->tilemap    = NULL;
    self->tile_patch = NULL;
}


void OMusic_dtor(OMusic* self)
{
    if (self->tilemap)    { RomLoader_dtor(self->tilemap); free(self->tilemap); }
    if (self->tile_patch) { RomLoader_dtor(self->tile_patch); free(self->tile_patch); }
}

/* Load Modified Widescreen version of tilemap */
bool OMusic_load_widescreen_map(OMusic* self)
{
    int status = 0;

    if (self->tilemap == NULL)
    {
        self->tilemap = (RomLoader*)calloc(1, sizeof(RomLoader));
        status += RomLoader_load_mem(self->tilemap, tilemap_bin, TILEMAP_BIN_SIZE);
    }

    if (self->tile_patch == NULL)
    {
        self->tile_patch = (RomLoader*)calloc(1, sizeof(RomLoader));
        status += RomLoader_load_mem(self->tile_patch, tilepatch_bin, TILEPATCH_BIN_SIZE);
    }

    return status == 0;
}

/* Initialize Music Selection Screen */
/* */
/* Source: 0xB342 */
void OMusic_enable(OMusic* self)
{
    oferrari.car_ctrl_active = false;
    Video_clear_text_ram(&video);
    OSprites_disable_sprites(&osprites);
    OTraffic_disable_traffic(&otraffic);
    /*edit jump table 3 */
    oinitengine.car_increment = 0;
    oferrari.car_inc_old      = 0;
    osprites.spr_cnt_main     = 0;
    osprites.spr_cnt_shadow   = 0;
    oroad.road_ctrl           = ROAD_BOTH_P0;
    oroad.horizon_base        = HORIZON_OFF;
    self->last_music_selected       = -1;
    self->preview_counter           = -20; /* Delay before playing music */
    ostats.time_counter       = config.sound.music_timer; /* Move 30 seconds to timer countdown (note on the original roms this is 15 seconds) */
    ostats.frame_counter      = frame_reset;  
     
    OMusic_blit_music_select(self);
    OHud_blit_text2(&ohud, TEXT2_SELECT_MUSIC); /* Select Music By Steering */
  
    OSoundInt_queue_sound(&osoundint, SOUND_RESET);
    if (!config.sound.preview)
        OSoundInt_queue_sound(&osoundint, SOUND_PCM_WAVE); /* Wave Noises */

    /* Enable block of sprites */
    self->entry_start = SPRITE_ENTRIES - 0x10;    
    { int i; for (i = self->entry_start; i < self->entry_start + 5; i++)
    {
        oentry_init(&osprites.jump_table[i], i);
    } }

    OMusic_setup_sprite1(self);
    OMusic_setup_sprite2(self);
    OMusic_setup_sprite3(self);
    OMusic_setup_sprite4(self);
    OMusic_setup_sprite5(self);

    /* Widescreen tiles need additional palette information copied over */
    if (self->tile_patch->loaded && config.s16_x_off > 0)
    {
        hwtiles_patch_tiles(video.tile_layer, self->tile_patch);
        OTiles_setup_palette_widescreen(&otiles);
    }

    hwtiles_set_x_clamp(video.tile_layer, CLAMP_CENTRE);
    self->cursor_pos = 1;
    self->total_tracks = (int)config.sound.music_num;
}

void OMusic_disable(OMusic* self)
{
    /* Disable block of sprites */
    { int i; for (i = self->entry_start; i < self->entry_start + 5; i++)
    {
        osprites.jump_table[i].control &= ~ENABLE;
    } }

    hwtiles_set_x_clamp(video.tile_layer, CLAMP_RIGHT);

    /* Restore original palette for widescreen tiles. */
    if (config.s16_x_off > 0)
    {
        hwtiles_restore_tiles(video.tile_layer);
        OTiles_setup_palette_tilemap(&otiles);
    }

    video.enabled = false; /* Turn screen off */
}

/* Music Selection Screen: Setup Radio Sprite */
/* Source: 0xCAF0 */
static void OMusic_setup_sprite1(OMusic* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 0];
    e->x = 28;
    e->y = 180;
    e->road_priority = 0xFF;
    e->priority = 0x1FE;
    e->zoom = 0x7F;
    e->pal_src = 0xB0;
    e->addr = outrun.adr.sprite_radio;
    OSprites_map_palette(&osprites, e);
}

/* Music Selection Screen: Setup Equalizer Sprite */
/* Source: 0xCB2A */
static void OMusic_setup_sprite2(OMusic* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 1];
    e->x = 4;
    e->y = 189;
    e->road_priority = 0xFF;
    e->priority = 0x1FE;
    e->zoom = 0x7F;
    e->pal_src = 0xA7;
    e->addr = outrun.adr.sprite_eq;
    OSprites_map_palette(&osprites, e);
}

/* Music Selection Screen: Setup FM Radio Readout */
/* Source: 0xCB64 */
static void OMusic_setup_sprite3(OMusic* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 2];
    e->x = -8;
    e->y = 176;
    e->road_priority = 0xFF;
    e->priority = 0x1FE;
    e->zoom = 0x7F;
    e->pal_src = 0x87;
    e->addr = outrun.adr.sprite_fm_left;
    OSprites_map_palette(&osprites, e);
}

/* Music Selection Screen: Setup FM Radio Dial */
/* Source: 0xCB9E */
static void OMusic_setup_sprite4(OMusic* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 3];
    e->x = 68;
    e->y = 181;
    e->road_priority = 0xFF;
    e->priority = 0x1FE;
    e->zoom = 0x7F;
    e->pal_src = 0x89;
    e->addr = outrun.adr.sprite_dial_left;
    OSprites_map_palette(&osprites, e);
}

/* Music Selection Screen: Setup Hand Sprite */
/* Source: 0xCBD8 */
static void OMusic_setup_sprite5(OMusic* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 4];
    e->x = 21;
    e->y = 196;
    e->road_priority = 0xFF;
    e->priority = 0x1FE;
    e->zoom = 0x7F;
    e->pal_src = 0xAF;
    e->addr = outrun.adr.sprite_hand_left;
    OSprites_map_palette(&osprites, e);
}

/* Check for start button during music selection screen */
/* */
/* Source: 0xB768 */
void OMusic_check_start(OMusic* self)
{
    if (ostats.credits && Input_has_pressed(&input, START))
    {
        outrun.game_state = GS_INIT_GAME;
        OLogo_disable(&ologo);
        OMusic_disable(self);
    }
}

/* Tick and Blit */
void OMusic_tick(OMusic* self)
{
    oentry * hand;
    oentry * dial;
    oentry * e;
    /* Radio Sprite */
    OSprites_do_spr_order_shadows(&osprites, &osprites.jump_table[self->entry_start + 0]);

    /* Animated EQ Sprite (Cycle the graphical equalizer on the radio) */
    e = &osprites.jump_table[self->entry_start + 1];
    e->reload++; /* Increment palette entry */
    e->pal_src = RomLoader_read8(&(roms.rom0), (e->reload & 0x3E) >> 1 | MUSIC_EQ_PAL);
    OSprites_map_palette(&osprites, e);
    OSprites_do_spr_order_shadows(&osprites, e);

    /* Draw appropriate FM station on radio, depending on steering setting */
    /* Draw Dial on radio, depending on steering setting */
    e = &osprites.jump_table[self->entry_start + 2];
    dial = &osprites.jump_table[self->entry_start + 3];
    hand = &osprites.jump_table[self->entry_start + 4];

    /* Determine Track Selection Logic */
    if (self->total_tracks < 3) OMusic_tick_original(self, e, dial, hand);
    else OMusic_tick_enhanced(self, e, dial, hand);

    OSprites_do_spr_order_shadows(&osprites, e);
    OSprites_do_spr_order_shadows(&osprites, dial);
    OSprites_do_spr_order_shadows(&osprites, hand);;

    /* Enhancement: Preview Music On Sound Selection Screen */
    if (config.sound.preview)
    {
        if (self->music_selected != self->last_music_selected)
        {
            if (self->preview_counter == 0 && self->last_music_selected != -1)
                OSoundInt_queue_sound(&osoundint, SOUND_FM_RESET);

            if (++self->preview_counter >= 10)
            {
                OMusic_play_music(self, -1);
                self->preview_counter = 0;
            }
        }
    }
}

void OMusic_play_music(OMusic* self, int index)
{
    if (index == -1) index = self->music_selected;

    self->next_track = &config.sound.music[index];

    switch (self->next_track->type)
    {
        case IS_YM_INT:
            Audio_clear_wav(&cannonball_audio);
            OSoundInt_queue_sound(&osoundint, self->next_track->cmd);
            break;

        case IS_YM_EXT:
            Audio_clear_wav(&cannonball_audio);
            { char mp[600]; snprintf(mp, sizeof(mp), "%s%s", config.data.res_path, self->next_track->filename); Roms_load_ym_data(&roms, mp); }
            OSoundInt_queue_sound(&osoundint, self->next_track->cmd);
            break;

        case IS_WAV:
            { char mp[600]; snprintf(mp, sizeof(mp), "%s%s", config.data.res_path, self->next_track->filename); Audio_load_wav(&cannonball_audio, mp); }
            break;
    }

    self->last_music_selected = index;
}

/* Cycle music in continuous mode */
void OMusic_cycle_music(OMusic* self)
{
    if (++self->music_selected > 2) self->music_selected = 0;
    OMusic_play_music(self, -1);
}

/* Original Version of Music Selection Screen With 3 Tracks. */
/* Wheel Left = Track 0, Wheel Centre = Track 1, Wheel Right = Track 2 */
static void OMusic_tick_original(OMusic* self, oentry* fm, oentry* dial, oentry* hand)
{
    /* Note tiles to append to left side of text */
    const uint32_t NOTE_TILES1 = 0x8A7A8A7B;
    const uint32_t NOTE_TILES2 = 0x8A7C8A7D;

    /* Steer Left */
    if (oinputs.steering_adjust + 0x80 <= 0x55)
    {
        OMusic_set_hand(self, HAND_LEFT, fm, dial, hand);
        OHud_blit_text2(&ohud, TEXT2_MAGICAL);
        Video_write_text32(&video, 0x1105C0, NOTE_TILES1);
        Video_write_text32(&video, 0x110640, NOTE_TILES2);
        self->music_selected = 0;
    }
    /* Centre */
    else if (oinputs.steering_adjust + 0x80 <= 0xAA)
    {
        OMusic_set_hand(self, HAND_CENTRE, fm, dial, hand);
        OHud_blit_text2(&ohud, TEXT2_BREEZE);
        Video_write_text32(&video, 0x1105C6, NOTE_TILES1);
        Video_write_text32(&video, 0x110646, NOTE_TILES2);
        self->music_selected = 1;
    }
    /* Steer Right */
    else
    {
        OMusic_set_hand(self, HAND_RIGHT, fm, dial, hand);
        OHud_blit_text2(&ohud, TEXT2_SPLASH);
        Video_write_text32(&video, 0x1105C8, NOTE_TILES1);
        Video_write_text32(&video, 0x110648, NOTE_TILES2);
        self->music_selected = 2;
    }
}

/* Enhanced Version of music selection with infinite tracks. */
static void OMusic_tick_enhanced(OMusic* self, oentry* fm, oentry* dial, oentry* hand)
{
    if (Input_has_pressed(&input, LEFT) || OInputs_is_analog_l(&oinputs))
        if (--self->cursor_pos < 0) self->cursor_pos = self->total_tracks - 1;
    if (Input_has_pressed(&input, RIGHT) || OInputs_is_analog_r(&oinputs))
        if (++self->cursor_pos >= self->total_tracks) self->cursor_pos = 0;

    if (oinputs.steering_adjust + 0x80 <= 0x70)
        OMusic_set_hand(self, HAND_LEFT, fm, dial, hand);
    else if (oinputs.steering_adjust + 0x80 <= 0x90)
        OMusic_set_hand(self, HAND_CENTRE, fm, dial, hand);
    else
        OMusic_set_hand(self, HAND_RIGHT, fm, dial, hand);

    self->music_selected = self->cursor_pos;
    OHud_blit_text_big(&ohud, 11, config.sound.music[self->music_selected].title, true);
}

static void OMusic_set_hand(OMusic* self, short direction, oentry* fm, oentry* dial, oentry* hand)
{
    if (direction == HAND_LEFT)
    {
        hand->x = 17;
        fm->addr   = outrun.adr.sprite_fm_left;
        dial->addr = outrun.adr.sprite_dial_left;
        hand->addr = outrun.adr.sprite_hand_left;
    }
    /* Centre */
    else if (direction == HAND_CENTRE)
    {
        hand->x = 21;
        fm->addr   = outrun.adr.sprite_fm_centre;
        dial->addr = outrun.adr.sprite_dial_centre;
        hand->addr = outrun.adr.sprite_hand_centre;
    }
    /* Steer Right */
    else if (direction == HAND_RIGHT)
    {
        hand->x = 21;
        fm->addr   = outrun.adr.sprite_fm_right;
        dial->addr = outrun.adr.sprite_dial_right;
        hand->addr = outrun.adr.sprite_hand_right;
    }
}

/* Blit Only: Used when frame skipping */
void OMusic_blit(OMusic* self)
{
    { int i; for (i = 0; i < 5; i++)
        OSprites_do_spr_order_shadows(&osprites, &osprites.jump_table[self->entry_start + i]); }
}

/* Blit Music Selection Tiles to text ram layer (Double Row) */
/*  */
/* Source Address: 0xE0DC */
/* Input:          Destination address into tile ram */
/* Output:         None */
/* */
/* Tilemap data is stored in the ROM as a series of words. */
/* */
/* A basic compression format is used: */
/* */
/* 1/ If a word is not '0000', copy it directly to tileram */
/* 2/ If a word is '0000' a long follows which details the compression. */
/*    The upper word of the long is the value to copy. */
/*    The lower word of the long is the number of times to copy that value. */
/* */
/* Tile structure: */
/* */
/* MSB          LSB */
/* ---nnnnnnnnnnnnn Tile index (0-8191) */
/* ---ccccccc------ Palette (0-127) */
/* p--------------- Priority flag */
/* -??------------- Unknown */

static void OMusic_blit_music_select(OMusic* self)
{
    const uint32_t TILEMAP_RAM_16 = 0x10F030;

    /* Palette Ram: 1F Long Entries For Sky Shade On Horizon, For Colour Change Effect */
    const uint32_t PAL_RAM_SKY = 0x120F00;

    uint32_t src_addr = PAL_MUSIC_SELECT;
    uint32_t dst_addr = PAL_RAM_SKY;

    /* Write 32 Palette Longs to Palette RAM */
    { int i; for (i = 0; i < 32; i++)
        Video_write_pal32_a(&video, &dst_addr, RomLoader_read32_a(&(roms.rom0), &src_addr)); }

    /* Set Tilemap Scroll */
    OTiles_set_scroll(&otiles, config.s16_x_off, 0);
    
    /* -------------------------------------------------------------------------------------------- */
    /* Blit to Tilemap 16: Widescreen Version. Uses Custom Tilemap.  */
    /* -------------------------------------------------------------------------------------------- */
    if (self->tilemap->loaded && config.s16_x_off > 0)
    {
        uint32_t tilemap16 = TILEMAP_RAM_16 - 20;
        src_addr = 0;

        { const uint16_t rows = RomLoader_read16_a(self->tilemap, &src_addr);
        const uint16_t cols = RomLoader_read16_a(self->tilemap, &src_addr);

        { int y; for (y = 0; y < rows; y++)
        {
            dst_addr = tilemap16;
            { int x; for (x = 0; x < cols; x++)
                Video_write_tile16_a(&video, &dst_addr, RomLoader_read16_a(self->tilemap, &src_addr)); }
            tilemap16 += 0x80; /* next line of tiles */
        } }
     }}
    /* -------------------------------------------------------------------------------------------- */
    /* Blit to Tilemap 16: Original 4:3 Version.  */
    /* -------------------------------------------------------------------------------------------- */
    else
    {
        uint32_t tilemap16 = TILEMAP_RAM_16;
        src_addr = TILEMAP_MUSIC_SELECT;

        { int y; for (y = 0; y < 28; y++)
        {
            dst_addr = tilemap16;
            { int x; for (x = 0; x < 40;)
            {
                /* get next tile */
                uint32_t data = RomLoader_read16_a(&(roms.rom0), &src_addr);
                /* No Compression: write tile directly to tile ram */
                if (data != 0)
                {
                    Video_write_tile16_a(&video, &dst_addr, data);    
                    x++;
                }
                /* Compression */
                else
                {
                    uint16_t value = RomLoader_read16_a(&(roms.rom0), &src_addr); /* tile index to copy */
                    uint16_t count = RomLoader_read16_a(&(roms.rom0), &src_addr); /* number of times to copy value */

                    { uint16_t i; for (i = 0; i <= count; i++)
                    {
                        Video_write_tile16_a(&video, &dst_addr, value);
                        x++;
                    } }
                }
            } }
            tilemap16 += 0x80; /* next line of tiles */
        } } /* end for */

        /* Fix Misplaced tile on music select screen (above steering wheel) */
        if (config.engine.fix_bugs)
            Video_write_tile16(&video, 0x10F730, 0x0C80);
    } 
}
