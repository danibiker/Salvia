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

#include <stdlib.h>
#include <libretro.h>

#include "trackloader.h"
#include "roms.h"
#include "engine/outrun.h"
#include "engine/oaddresses.h"

static void TrackLoader_setup_level(TrackLoader* self, Level* l, RomLoader* data, const int STAGE_ADR);
static void TrackLoader_setup_section(TrackLoader* self, Level* l, RomLoader* data, const int STAGE_ADR);

int32_t TrackLoader_read32_a(uint8_t* data, uint32_t* addr)
{    
    int32_t value = (data[*addr] << 24) | (data[*addr+1] << 16) | (data[*addr+2] << 8) | (data[*addr+3]);
    *addr += 4;
    return value;
}

int16_t TrackLoader_read16_a(uint8_t* data, uint32_t* addr)
{
    int16_t value = (data[*addr] << 8) | (data[*addr+1]);
    *addr += 2;
    return value;
}

int8_t TrackLoader_read8_a(uint8_t* data, uint32_t* addr)
{
    return data[(*addr)++]; 
}

int32_t TrackLoader_read32(uint8_t* data, uint32_t addr)
{    
    return (data[addr] << 24) | (data[addr+1] << 16) | (data[addr+2] << 8) | data[addr+3];
}

int16_t TrackLoader_read16(uint8_t* data, uint32_t addr)
{
    return (data[addr] << 8) | data[addr+1];
}

int8_t TrackLoader_read8(uint8_t* data, uint32_t addr)
{
    return data[addr];
}


extern retro_log_printf_t                 log_cb;

/* ------------------------------------------------------------------------------------------------ */
/* Stage Mapping Data: This is the master table that controls the order of the stages. */
/* */
/* You can change the stage order by editing this table. */
/* Bear in mind that the double lanes are hard coded in Stage 1. */
/* */
/* For USA there are unused tracks: */
/* 0x3A = Unused Coconut Beach */
/* 0x25 = Original Gateway track from Japanese edition */
/* 0x19 = Devils Canyon Variant */
/* ------------------------------------------------------------------------------------------------ */

static uint8_t STAGE_MAPPING_USA[] = 
{ 
    0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* Stage 1 */
    0x1E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* Stage 2 */
    0x20, 0x2F, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00,  /* Stage 3 */
    0x2D, 0x35, 0x33, 0x21, 0x00, 0x00, 0x00, 0x00,  /* Stage 4 */
    0x32, 0x23, 0x38, 0x22, 0x26, 0x00, 0x00, 0x00,  /* Stage 5 */
};

static uint8_t STAGE_MAPPING_JAP[] = 
{ 
    0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* Stage 1 */
    0x20, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* Stage 2 */
    0x1E, 0x2F, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00,  /* Stage 3 */
    0x2D, 0x25, 0x33, 0x21, 0x00, 0x00, 0x00, 0x00,  /* Stage 4 */
    0x32, 0x23, 0x38, 0x22, 0x26, 0x00, 0x00, 0x00,  /* Stage 5 */
};

TrackLoader trackloader;

void TrackLoader_ctor(TrackLoader* self)
{
    self->layout        = NULL;
    self->levels        = (Level*)malloc((STAGES) * sizeof(Level));
    self->levels_end    = (Level*)malloc((5) * sizeof(Level));
    self->level_split   = (Level*)calloc(1, sizeof(Level));
    self->current_level = &self->levels[0];

    self->mode       = TL_MODE_ORIGINAL;
}

void TrackLoader_dtor(TrackLoader* self)
{
    if (self->layout != NULL)
        free(self->layout);

    free(self->levels_end);
    free(self->levels);
    free(self->level_split);
}

void TrackLoader_init(TrackLoader* self, bool jap)
{
    if (self->mode == TL_MODE_ORIGINAL)
        TrackLoader_init_original_tracks(self, jap);
    else
        TrackLoader_init_layout_tracks(self, jap);
}

void TrackLoader_init_original_tracks(TrackLoader* self, bool jap)
{
    static const uint32_t STAGE_ORDER[] = { 0,
                                            0x8, 0x9, 
                                            0x10, 0x11, 0x12, 
                                            0x18, 0x19, 0x1A, 0x1B, 
                                            0x20, 0x21, 0x22, 0x23, 0x24};
    self->stage_data = jap ? STAGE_MAPPING_JAP : STAGE_MAPPING_USA;

    self->display_start_line = true;

    /* -------------------------------------------------------------------------------------------- */
    /* Setup Shared Data */
    /* -------------------------------------------------------------------------------------------- */

    /* Height Map Entries */
    self->heightmap_offset  = outrun.adr.road_height_lookup;
    self->heightmap_data    = &roms.rom1p->rom[0];  

    /* Scenery Map Entries */
    self->scenerymap_offset = outrun.adr.sprite_master_table;
    self->scenerymap_data   = &roms.rom0p->rom[0]; 

    /* Palette Entries */
    self->pal_sky_offset    = PAL_SKY_TABLE;
    self->pal_sky_data      = &roms.rom0.rom[0];

    self->pal_gnd_offset    = PAL_GND_TABLE;
    self->pal_gnd_data      = &roms.rom0.rom[0];

    /* -------------------------------------------------------------------------------------------- */
    /* Iterate and setup 15 stages */
    /* -------------------------------------------------------------------------------------------- */


    { int i; for (i = 0; i < STAGES; i++)
    {
        uint32_t PATH_ADR;
        const uint16_t STAGE_OFFSET = self->stage_data[STAGE_ORDER[i]] << 2;

        /* CPU 0 Data */
        const uint32_t STAGE_ADR = RomLoader_read32(roms.rom0p, outrun.adr.road_seg_table + STAGE_OFFSET);
        TrackLoader_setup_level(self, &self->levels[i], roms.rom0p, STAGE_ADR);

        /* CPU 1 Data */
        PATH_ADR = RomLoader_read32(roms.rom1p, ROAD_DATA_LOOKUP + STAGE_OFFSET);
        self->levels[i].path = &roms.rom1p->rom[PATH_ADR];        
    } }

    /* -------------------------------------------------------------------------------------------- */
    /* Setup End Sections & Split Stages */
    /* -------------------------------------------------------------------------------------------- */

    /* Split stages don't contain palette information */
    TrackLoader_setup_section(self, self->level_split, roms.rom0p, outrun.adr.road_seg_split);
    self->level_split->path         = &roms.rom1p->rom[ROAD_DATA_SPLIT];

    { int i; for (i = 0; i < 5; i++)
    {
        const uint32_t STAGE_ADR = RomLoader_read32(roms.rom0p, outrun.adr.road_seg_end + (i << 2));
        TrackLoader_setup_section(self, &self->levels_end[i], roms.rom0p, STAGE_ADR);
        self->levels_end[i].path  = &roms.rom1p->rom[ROAD_DATA_BONUS];
    } }
}

void TrackLoader_init_layout_tracks(TrackLoader* self, bool jap)
{
    uint8_t* end_path;
    self->stage_data = STAGE_MAPPING_USA;

    /* -------------------------------------------------------------------------------------------- */
    /* Check Version is Correct */
    /* -------------------------------------------------------------------------------------------- */
    if (RomLoader_read32(self->layout,  HEADER) !=  EXPECTED_VERSION)
    {
        log_cb(RETRO_LOG_WARN, "Incompatible LayOut Version Detected. Try upgrading CannonBall to the latest version\n");
        TrackLoader_init_original_tracks(self, jap);
        return;
    }

    self->display_start_line = RomLoader_read8(self->layout, (uint32_t) HEADER + (uint32_t)sizeof(uint32_t));

    /* -------------------------------------------------------------------------------------------- */
    /* Setup Shared Data */
    /* -------------------------------------------------------------------------------------------- */

    /* Height Map Entries */
    self->heightmap_offset  = RomLoader_read32(self->layout,  HEIGHT_MAPS);
    self->heightmap_data    = &self->layout->rom[0];  

    /* Scenery Map Entries */
    self->scenerymap_offset = RomLoader_read32(self->layout,  SPRITE_MAPS);
    self->scenerymap_data   = &self->layout->rom[0]; 

    /* Palette Entries */
    self->pal_sky_offset    = RomLoader_read32(self->layout,  PAL_SKY);
    self->pal_sky_data      = &self->layout->rom[0];

    self->pal_gnd_offset    = RomLoader_read32(self->layout,  PAL_GND);
    self->pal_gnd_data      = &self->layout->rom[0];

    /* -------------------------------------------------------------------------------------------- */
    /* Iterate and setup 15 stages */
    /* -------------------------------------------------------------------------------------------- */
    { int i; for (i = 0; i < STAGES; i++)
    {
        uint32_t PATH_ADR;
        /* CPU 0 Data */
        const uint32_t STAGE_ADR = RomLoader_read32(self->layout,  LEVELS + (i * sizeof(uint32_t)));
        TrackLoader_setup_level(self, &self->levels[i], self->layout, STAGE_ADR);

        /* CPU 1 Data */
        PATH_ADR = RomLoader_read32(self->layout,  PATH);
        self->levels[i].path = &self->layout->rom[ PATH_ADR + ((ROAD_END_CPU1 * sizeof(uint32_t)) * i) ];
    } }

    /* -------------------------------------------------------------------------------------------- */
    /* Setup End Sections & Split Stages */
    /* -------------------------------------------------------------------------------------------- */

    /* Split stages don't contain palette information */
    TrackLoader_setup_section(self, self->level_split, self->layout, RomLoader_read32(self->layout,  SPLIT_LEVEL));
    self->level_split->path = &self->layout->rom[ RomLoader_read32(self->layout,  SPLIT_PATH) ];

    /* End sections don't contain palette information. Shared path. */
    end_path = &self->layout->rom[ RomLoader_read32(self->layout,  END_PATH) ];
    { int i; for (i = 0; i < 5; i++)
    {
        const uint32_t STAGE_ADR = RomLoader_read32(self->layout,  END_LEVELS + (i * sizeof(uint32_t)));
        TrackLoader_setup_section(self, &self->levels_end[i], self->layout, STAGE_ADR);
        self->levels_end[i].path = end_path;
    } }
}

/* Setup a normal level */
static void TrackLoader_setup_level(TrackLoader* self, Level* l, RomLoader* data, const int STAGE_ADR)
{
    /* Sky Palette */
    uint32_t adr = RomLoader_read32(data, STAGE_ADR + 0);
    l->pal_sky   = RomLoader_read16(data, adr);

    /* Load Road Pallete */
    adr = RomLoader_read32(data, STAGE_ADR + 4);
    l->palr1.stripe_centre = RomLoader_read32_a(data, &adr);
    l->palr2.stripe_centre = RomLoader_read32(data, adr);

    adr = RomLoader_read32(data, STAGE_ADR + 8);
    l->palr1.stripe = RomLoader_read32_a(data, &adr);
    l->palr2.stripe = RomLoader_read32(data, adr);

    adr = RomLoader_read32(data, STAGE_ADR + 12);
    l->palr1.side = RomLoader_read32_a(data, &adr);
    l->palr2.side = RomLoader_read32(data, adr);

    adr = RomLoader_read32(data, STAGE_ADR + 16);
    l->palr1.road = RomLoader_read32_a(data, &adr);
    l->palr2.road = RomLoader_read32(data, adr);

    /* Ground Palette */
    adr = RomLoader_read32(data, STAGE_ADR + 20);
    l->pal_gnd = RomLoader_read16(data, adr);

    /* Curve Data */
    self->curve_offset = RomLoader_read32(data, STAGE_ADR + 24);
    l->curve = &data->rom[self->curve_offset];

    /* Width / Height Lookup */
    self->wh_offset = RomLoader_read32(data, STAGE_ADR + 28);
    l->width_height = &data->rom[self->wh_offset];

    /* Sprite Information */
    self->scenery_offset = RomLoader_read32(data, STAGE_ADR + 32);
    l->scenery = &data->rom[self->scenery_offset];
}

/* Setup a special section of track (end section or level split) */
/* Special sections do not contain palette information */
static void TrackLoader_setup_section(TrackLoader* self, Level* l, RomLoader* data, const int STAGE_ADR)
{
    /* Curve Data */
    self->curve_offset = RomLoader_read32(data, STAGE_ADR + 0);
    l->curve = &data->rom[self->curve_offset];

    /* Width / Height Lookup */
    self->wh_offset = RomLoader_read32(data, STAGE_ADR + 4);
    l->width_height = &data->rom[self->wh_offset];

    /* Sprite Information */
    self->scenery_offset = RomLoader_read32(data, STAGE_ADR + 8);
    l->scenery = &data->rom[self->scenery_offset];
}

/* ------------------------------------------------------------------------------------------------ */
/*                                 CPU 0: Track Data (Scenery, Width, Height) */
/* ------------------------------------------------------------------------------------------------ */

void TrackLoader_init_track(TrackLoader* self, const uint32_t offset)
{
    self->curve_offset   = 0;
    self->wh_offset      = 0;
    self->scenery_offset = 0;
    self->current_level  = &self->levels[TrackLoader_stage_offset_to_level(self, offset)];
}

int8_t TrackLoader_stage_offset_to_level(TrackLoader* self, uint32_t id)
{
    static const uint8_t ID_OFFSET[] = {0, 1, 3, 6, 10};
    return (ID_OFFSET[id / 8]) + (id & 7);
}


void TrackLoader_init_track_split(TrackLoader* self)
{
    self->curve_offset   = 0;
    self->wh_offset      = 0;
    self->scenery_offset = 0;
    self->current_level  = self->level_split;
}

void TrackLoader_init_track_bonus(TrackLoader* self, const uint32_t id)
{
    self->curve_offset   = 0;
    self->wh_offset      = 0;
    self->scenery_offset = 0;
    self->current_level  = &self->levels_end[id];
}

/* ------------------------------------------------------------------------------------------------ */
/*                                    CPU 1: Road Path Functions */
/* ------------------------------------------------------------------------------------------------ */

void TrackLoader_init_path(TrackLoader* self, const uint32_t offset)
{
    self->current_path = self->levels[TrackLoader_stage_offset_to_level(self, offset)].path;
}

void TrackLoader_init_path_split(TrackLoader* self)
{
    self->current_path = self->level_split->path;
}

void TrackLoader_init_path_end(TrackLoader* self)
{
    self->current_path = self->levels_end[0].path; /* Path is shared for end sections */
}

/* ------------------------------------------------------------------------------------------------ */
/*                                        HELPER FUNCTIONS TO READ DATA */
/* ------------------------------------------------------------------------------------------------ */

int16_t TrackLoader_readPath(TrackLoader* self, uint32_t addr)
{
    return (self->current_path[addr] << 8) | self->current_path[addr+1];
}

int16_t TrackLoader_readPath_a(TrackLoader* self, uint32_t* addr)
{
    int16_t value = (self->current_path[*addr] << 8) | (self->current_path[*addr+1]);
    *addr += 2;
    return value;
}

int16_t TrackLoader_read_width_height(TrackLoader* self, uint32_t* addr)
{
    int16_t value = (self->current_level->width_height[*addr + self->wh_offset] << 8) | (self->current_level->width_height[*addr+1 + self->wh_offset]);
    *addr += 2;
    return value;
}

int16_t TrackLoader_read_curve(TrackLoader* self, uint32_t addr)
{
    return (self->current_level->curve[addr + self->curve_offset] << 8) | self->current_level->curve[addr+1 + self->curve_offset];
}

uint16_t TrackLoader_read_scenery_pos(TrackLoader* self)
{
    return (self->current_level->scenery[self->scenery_offset] << 8) | self->current_level->scenery[self->scenery_offset + 1];
}

uint8_t TrackLoader_read_total_sprites(TrackLoader* self)
{
    return self->current_level->scenery[self->scenery_offset + 2];
}

uint8_t TrackLoader_read_sprite_pattern_index(TrackLoader* self)
{
    return self->current_level->scenery[self->scenery_offset + 3];
}

Level* TrackLoader_get_level(TrackLoader* self, uint32_t id)
{
    return &self->levels[TrackLoader_stage_offset_to_level(self, id)];
}

uint32_t TrackLoader_read_pal_sky_table(TrackLoader* self, uint16_t entry)
{
    return TrackLoader_read32(self->pal_sky_data, self->pal_sky_offset + (entry * 4));
}

uint32_t TrackLoader_read_pal_gnd_table(TrackLoader* self, uint16_t entry)
{
    return TrackLoader_read32(self->pal_gnd_data, self->pal_gnd_offset + (entry * 4));
}

uint32_t TrackLoader_read_heightmap_table(TrackLoader* self, uint16_t entry)
{
    return TrackLoader_read32(self->heightmap_data, self->heightmap_offset + (entry * 4));
}

uint32_t TrackLoader_read_scenerymap_table(TrackLoader* self, uint16_t entry)
{
    return TrackLoader_read32(self->scenerymap_data, self->scenerymap_offset + (entry * 4));
}
