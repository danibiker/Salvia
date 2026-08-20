/***************************************************************************
    Tilemap Handling Code. 

    Logic for the foreground and background tilemap layers.

    - Read and render tilemaps
    - H-Scroll & V-Scroll
    - Palette Initialization
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "../trackloader.h"
#include "engine/opalette.h"
#include "engine/otiles.h"

static void OTiles_clear_tile_info(OTiles* self);
static void OTiles_init_tilemap(OTiles* self, int16_t stage_id);
static void OTiles_init_tilemap_props(OTiles* self, uint16_t);
static void OTiles_scroll_tilemaps(OTiles* self);
static void OTiles_init_next_tilemap(OTiles* self);
static void OTiles_copy_to_palram(OTiles* self, const uint8_t, uint32_t, uint32_t);
static void OTiles_split_tilemaps(OTiles* self);
static void OTiles_loop_to_stage1(OTiles* self);
static void OTiles_clear_old_name_table(OTiles* self);
static void OTiles_h_scroll_tilemaps(OTiles* self);
static void OTiles_v_scroll_tilemaps(OTiles* self);
static void OTiles_copy_fg_tiles(OTiles* self, uint32_t);
static void OTiles_copy_bg_tiles(OTiles* self, uint32_t);
static void OTiles_update_fg_page(OTiles* self);
static void OTiles_update_bg_page(OTiles* self);
static void OTiles_update_fg_page_split(OTiles* self);
static void OTiles_update_bg_page_split(OTiles* self);

OTiles otiles;



void OTiles_init(OTiles* self)
{
    self->vswap_off   = 0;
    self->vswap_state = VSWAP_OFF;
}

void OTiles_set_vertical_swap(OTiles* self)
{
    self->vswap_off   = 0;
    self->vswap_state = VSWAP_SCROLL_OFF;
}


/* Write Tilemap Values To Hardware On Vertical Interrupt */
/*  */
/* Source Address: 0xD790 */
/* Input:          None */
/* Output:         None */

void OTiles_write_tilemap_hw(OTiles* self)
{
    Video_write_text16(&video, HW_FG_HSCROLL, self->fg_h_scroll & 0x1FF);
    Video_write_text16(&video, HW_BG_HSCROLL, self->bg_h_scroll & 0x1FF);
    Video_write_text16(&video, HW_FG_VSCROLL, self->fg_v_scroll & 0x1FF);
    Video_write_text16(&video, HW_BG_VSCROLL, self->bg_v_scroll & 0x1FF);
    Video_write_text16(&video, HW_FG_PSEL, self->fg_psel);
    Video_write_text16(&video, HW_BG_PSEL, self->bg_psel);
}

/* Setup Default Palette Settings */
/*  */
/* Source Address: 0x85EA */
/* Input:          None */
/* Output:         None */

void OTiles_setup_palette_hud(OTiles* self)
{
    uint32_t src_addr = 0x16ED8;
    uint32_t pal_addr = 0x120000;

    /* Write longs of palette data. Read from ROM. */
    { int i; for (i = 0; i <= 0x1F; i++)
    {
        Video_write_pal32_a(&video, &pal_addr, RomLoader_read32_a(&(roms.rom0), &src_addr));
    } }
}

/* Setup default palette for tilemaps for stages 1,3,5 and music select */
/*  */
/* Source Address: 0x8602 */
/* Input:          None */
/* Output:         None */

void OTiles_setup_palette_tilemap(OTiles* self)
{
    uint32_t src_addr = 0x16FD8;
    uint32_t pal_addr = S16_PALETTE_BASE + (8 * 16); /* Palette Entry 8 */
    
    { int i; for (i = 0; i < 120; i++)
    {    
        uint16_t offset = RomLoader_read8_a(&(roms.rom0), &src_addr) << 4;
        uint32_t tile_data_addr = 0x17050 + offset;
        
        /* Write 4 x longs of palette data. Read from ROM. */
        Video_write_pal32_a(&video, &pal_addr, RomLoader_read32_a(&(roms.rom0), &tile_data_addr));
        Video_write_pal32_a(&video, &pal_addr, RomLoader_read32_a(&(roms.rom0), &tile_data_addr));
        Video_write_pal32_a(&video, &pal_addr, RomLoader_read32_a(&(roms.rom0), &tile_data_addr));
        Video_write_pal32_a(&video, &pal_addr, RomLoader_read32_a(&(roms.rom0), &tile_data_addr));
    } }
}

/* Palette Patch for Widescreen Music Selection Tilemap */
void OTiles_setup_palette_widescreen(OTiles* self)
{
    /* Duplicate 10 palette entries from index 44 onwards. */
    /* This is needed due to the new tiles created for widescreen mode and the sharing of  */
    /* palette / tile indexes in the data structure. */
    uint32_t src_addr = S16_PALETTE_BASE + (44 * 16);
    uint32_t pal_addr = S16_PALETTE_BASE + ((64 + 44) * 16);

    { int i; for (i = 0; i < 10; i++)
    {
        Video_write_pal32_a(&video, &pal_addr, Video_read_pal32(&video, &src_addr));
        Video_write_pal32_a(&video, &pal_addr, Video_read_pal32(&video, &src_addr));
        Video_write_pal32_a(&video, &pal_addr, Video_read_pal32(&video, &src_addr));
        Video_write_pal32_a(&video, &pal_addr, Video_read_pal32(&video, &src_addr));
    } }

    /* Create new palette (Entry 72). */
    /* Use Palette 53 as a basis for this.  */
    /* This is needed for some of the new Widescreen tiles */
    src_addr = S16_PALETTE_BASE + (53 * 16);
    pal_addr = S16_PALETTE_BASE + (72 * 16);
    Video_write_pal32_a(&video, &pal_addr, Video_read_pal32(&video, &src_addr));
    Video_write_pal32_a(&video, &pal_addr, Video_read_pal32(&video, &src_addr));
    Video_write_pal32_a(&video, &pal_addr, Video_read_pal32(&video, &src_addr));
    Video_write_pal32_a(&video, &pal_addr, Video_read_pal32(&video, &src_addr));

    /* Copy wheel colour from Palette 50 to Palette 72, index 2. */
    src_addr = S16_PALETTE_BASE + (50 * 16) + 4;
    pal_addr = S16_PALETTE_BASE + (72 * 16) + 4;
    Video_write_pal16(&video, &pal_addr, Video_read_pal16_a(&video, &src_addr));
}

/* Reset Tiles, Palette And Road Split Data */
/* */
/* Source: 0xD7FC */

void OTiles_reset_tiles_pal(OTiles* self)
{
    self->tilemap_ctrl = TILEMAP_CLEAR;
    oinitengine.end_stage_props &= ~1; /* Denote road not splitting */
    opalette.pal_manip_ctrl = 0;
}

/* Initialize, Scroll and Update Both FG & BG Tilemaps. */
/* Source Address: 0xD812 */
/* */
/* Notes: */
/* */
/* Each level contains at FG and BG tilemap layer. */
/* Each tilemap comprises a number of name tables. */
/* */
/* There can be two FG and two BG layers loaded at once. */
/* This is because the previous and upcoming tilemaps are scrolled between on level switch. */

void OTiles_update_tilemaps(OTiles* self, int8_t p)
{
    if (outrun.service_mode) return;

    self->page = p;

    switch (self->tilemap_ctrl & 3)
    {
        /* Clear Tile Table 1 & Init Default Tilemap (Stage 1) */
        case TILEMAP_CLEAR:
            OTiles_clear_tile_info(self);
            break;

        /* Scroll Tilemap */
        case TILEMAP_SCROLL:
            OTiles_scroll_tilemaps(self);
            break;

        /* Init Next Tilemap (on level switch) */
        case TILEMAP_INIT:
            OTiles_init_next_tilemap(self);
            break;

        /* New Tilemap Initialized - Scroll both tilemaps during tilesplit */
        case TILEMAP_SPLIT:
            OTiles_split_tilemaps(self);
            break;
    }
}

/* Clear various areas related to TILE RAM and init default values for start of level */
/* Source: 0xD848 */
static void OTiles_clear_tile_info(OTiles* self)
{
    uint32_t dst_addr;
    /* 1. Clear portion of RAM containing tilemap info (60F00 - 60F1F) */
    self->fg_h_scroll = 
    self->bg_h_scroll = 
    self->fg_v_scroll = 
    self->bg_v_scroll =   /* +4 words */
    self->fg_psel = 
    self->bg_psel = 
    self->tilemap_v_scr = 
    self->tilemap_h_scr = /* +5 words */
    self->fg_v_tiles = 
    self->bg_v_tiles = 
    self->tilemap_v_off = /* +3 words */
    self->fg_addr = 
    self->bg_addr = 0;    /* +4 words */

    /* 2. Clear portion of TEXT RAM containing tilemap info (110E80 - 110FFF) */
    dst_addr = HW_FG_PSEL;
    { uint8_t i; for (i = 0; i <= 0x5F; i++)
        Video_write_text32_a(&video, &dst_addr, 0); }

    /* 3. Clear all of TILE RAM 100000 - 10FFFF */
    Video_clear_tile_ram(&video);

    /* 4. Setup new values */
    self->fg_psel = RomLoader_read16(&(roms.rom0), TILES_PAGE_FG1);
    self->bg_psel = RomLoader_read16(&(roms.rom0), TILES_PAGE_BG1);
    Video_write_text16(&video, HW_FG_PSEL, self->fg_psel);    /* Also write values to hardware */
    Video_write_text16(&video, HW_BG_PSEL, self->bg_psel);

    OTiles_init_tilemap(self, oroad.stage_lookup_off); /* Initialize Default Tilemap */
}

/* Initalize Default Tilemap (Stage 1) */
/* */
/* +0 [byte] - FG Tilemap Height */
/* +1 [byte] - BG Tilemap Height */
/* +2 [long] - FG Tilemap Address */
/* +6 [long] - BG Tilemap Address */
/* +A [word] - V-Scroll Offset */
/* */
/* Source: 0xD8B2 */
static void OTiles_init_tilemap(OTiles* self, int16_t stage_id)
{
    uint8_t offset = (RomLoader_read8(roms.rom0p, outrun.adr.tiles_def_lookup + stage_id) << 2) * 3;
    uint32_t addr = outrun.adr.tiles_table + offset;

    self->fg_v_tiles    = RomLoader_read8_a(roms.rom0p, &addr);   /* Write Default FG Tilemap Height */
    self->bg_v_tiles    = RomLoader_read8_a(roms.rom0p, &addr);   /* Write Default BG Tilemap Height */
    self->fg_addr       = RomLoader_read32_a(roms.rom0p, &addr);  /* Write Default FG Tilemap Address */
    self->bg_addr       = RomLoader_read32_a(roms.rom0p, &addr);  /* Write Default BG Tilemap Address */
    self->tilemap_v_off = RomLoader_read16_a(roms.rom0p, &addr);
    { int16_t v_off = 0x68 - self->tilemap_v_off;
    oroad.horizon_y_bak = oroad.horizon_y2;

    self->fg_v_scroll   = v_off;
    self->bg_v_scroll   = v_off;

    if (self->vswap_state == VSWAP_OFF)
    {
        Video_write_text16(&video, HW_FG_VSCROLL, v_off);           /* Also write values to hardware */
        Video_write_text16(&video, HW_BG_VSCROLL, v_off);
    }
    OTiles_copy_fg_tiles(self, 0x100F80);                            /* Copy Foreground tiles to Tile RAM */
    OTiles_copy_bg_tiles(self, 0x108F80);                            /* Copy Background tiles to Tile RAM */
    self->tilemap_ctrl = TILEMAP_SCROLL;
 }}

/* Initialize Tilemap properties for Stage (FG & BG) */
/* */
/* - Width & Height of Tilemap */
/* - ROM Address of Tiles */
/* - V-Scroll Offset */
/* Source: DC02 */
static void OTiles_init_tilemap_props(OTiles* self, uint16_t stage_id)
{
    uint8_t offset = (RomLoader_read8(roms.rom0p, outrun.adr.tiles_def_lookup + stage_id) << 2) * 3;
    uint32_t addr = outrun.adr.tiles_table + offset;

    self->fg_v_tiles    = RomLoader_read8_a(roms.rom0p, &addr);   /* Write Default FG Tilemap Height */
    self->bg_v_tiles    = RomLoader_read8_a(roms.rom0p, &addr);   /* Write Default BG Tilemap Height */
    self->fg_addr       = RomLoader_read32_a(roms.rom0p, &addr);  /* Write Default FG Tilemap Address */
    self->bg_addr       = RomLoader_read32_a(roms.rom0p, &addr);  /* Write Default BG Tilemap Address */
    self->tilemap_v_off = RomLoader_read16_a(roms.rom0p, &addr);  /* Set Tilemap v-scroll offset    */
}


/* Copy Foreground Tiles */
/*  */
/* - Initalise the foreground tilemap */
/* - Uncompress the tilemap from ROM and place into Tile RAM */
/* - The FG tilemap is defined by a 128x64 virtual name table, which is itself composed of four smaller 64x32 name tables. */
/* */
/* Source: 0xDCF2 */

static void OTiles_copy_fg_tiles(OTiles* self, uint32_t dst_addr)
{
    uint32_t src_addr = self->fg_addr;
    uint16_t offset = 0;          /* Offset into Tile RAM (e.g. the name table to use) */

    /* Each tiled background is composed of 4 smaller 64x32 name tables. This counter iterates through them. */
    { uint8_t i; for (i = 0; i < 4; i++)
    {
        /* next_name_table: */
        uint32_t tileram_addr = dst_addr + offset;
        int16_t y = self->fg_v_tiles - 1;

        /* next_tile_y: */
        do
        {
            int16_t x = 0x3F;       /* TILERAM is 0x40 Columns Wide x 8 pixels = 512 */
            /* next_tilex: */
            do
            {
                uint32_t data = RomLoader_read16_a(&(roms.rom0), &src_addr);

                /* Compression */
                if (data == 0)
                {
                    uint16_t value = RomLoader_read16_a(&(roms.rom0), &src_addr); /* tile index to copy */
                    uint16_t count = RomLoader_read16_a(&(roms.rom0), &src_addr); /* number of times to copy value */
                
                    /* copy_compressed: */
                    { uint16_t i; for (i = 0; i <= count; i++)
                    {
                        Video_write_tile16_a(&video, &tileram_addr, value);
                        if (--x < 0)
                            break; /* Break out of do/while loop to compression_done */
                    } }
                }
                /* No Compression */
                else
                {
                    /* copy_next_word: */
                    Video_write_tile16_a(&video, &tileram_addr, data);
                    --x;
                }
                /* cont: */
            }
            while (x >= 0);
            /* compression_done: */

            /* Previous row in tileram (256 pixels) */
            tileram_addr -= 0x100;
        }
        while (--y >= 0);

        offset += 0x1000; /* Bytes between each name table */
    } }
}

/* Copy Background Tiles */
/*  */
/* Note, this is virtually the same as the foreground method, */
/* aside from only copying 3 nametables, instead of 4. */
/* */
/* Source: 0xDD46 */
static void OTiles_copy_bg_tiles(OTiles* self, uint32_t dst_addr)
{
    uint32_t src_addr = self->bg_addr;
    uint16_t offset = 0;          /* Offset into Tile RAM (e.g. the name table to use) */

    /* Each tiled background is composed of 3 smaller 64x32 name tables. This counter iterates through them. */
    { uint8_t i; for (i = 0; i < 3; i++)
    {
        /* next_name_table: */
        uint32_t tileram_addr = dst_addr + offset;
        int16_t y = self->bg_v_tiles - 1;

        /* next_tile_y: */
        do
        {
            int16_t x = 0x3F;       /* TILERAM is 0x40 Columns Wide x 8 pixels = 512 */
            /* next_tilex: */
            do
            {
                uint32_t data = RomLoader_read16_a(&(roms.rom0), &src_addr);

                /* Compression */
                if (data == 0)
                {
                    uint16_t value = RomLoader_read16_a(&(roms.rom0), &src_addr); /* tile index to copy */
                    uint16_t count = RomLoader_read16_a(&(roms.rom0), &src_addr); /* number of times to copy value */
                
                    /* copy_compressed: */
                    { uint16_t i; for (i = 0; i <= count; i++)
                    {
                        Video_write_tile16_a(&video, &tileram_addr, value);
                        if (--x < 0)
                            break; /* Break out of do/while loop to compression_done */
                    } }
                }
                /* No Compression */
                else
                {
                    /* copy_next_word: */
                    Video_write_tile16_a(&video, &tileram_addr, data);
                    --x;
                }
                /* cont: */
            }
            while (x >= 0);
            /* compression_done: */

            /* Previous row in tileram (256 pixels) */
            tileram_addr -= 0x100;
        }
        while (--y >= 0);

        offset += 0x1000; /* Bytes between each name table */
    } }
}

/* Source: D910 */
static void OTiles_scroll_tilemaps(OTiles* self)
{
    /* Yes, OutRun contains a lot of crappy code. Return when car moving into start line */
    if (outrun.game_state != GS_BEST1 && 
        outrun.game_state != GS_ATTRACT && 
        outrun.game_state != GS_INGAME &&
        (outrun.game_state < GS_START1 || outrun.game_state > GS_INGAME)) /* Added GS_START1 - 3 here for view enhancement code */
    {
        if (outrun.game_state != GS_BONUS)
            return;
    }

    /* Determine if we need to loop back to Stage 1 */
    if (oinitengine.end_stage_props & BIT_3)
    {
        oinitengine.end_stage_props &= ~BIT_3;
        OTiles_loop_to_stage1(self);
        return;
    }
    /* Determine if road splitting */
    else if (oinitengine.end_stage_props & BIT_0)
    {
        OPalette_setup_sky_change(&opalette);
        self->tilemap_ctrl  = TILEMAP_INIT;
        self->tilemap_setup = SETUP_TILES;
    }

    /* Continuous Mode: Vertically scroll tilemap at end of stage */
    if (outrun.game_state != GS_BEST1) /* uses tilemap so we don't want to be adjusting it */
    {
        switch (self->vswap_state)
        {
            case VSWAP_OFF:
                break;

            case VSWAP_SCROLL_OFF:
                if (++self->vswap_off > 0x40)
                {
                    self->vswap_state = VSWAP_SCROLL_ON;
                    OTiles_clear_tile_info(self);
                    OTiles_init_tilemap_palette(self, oroad.stage_lookup_off);
                }
                break;

            case VSWAP_SCROLL_ON:
                if (--self->vswap_off == 0)
                    self->vswap_state = VSWAP_OFF;
                break;
        }
    }

    /* update_tilemap: */
    /* This block is called when we do not need to init a new tilemap */

    /* Do we need to clear the old name tables? */
    if (self->clear_name_tables)
        OTiles_clear_old_name_table(self);

    OTiles_h_scroll_tilemaps(self);        /* Set H-Scroll values for Tilemaps */

    /* Denote road not splitting (used by UpdateFGPage and UpdateBGPage)     */
    if (oinitengine.rd_split_state == SPLIT_NONE)
        self->page_split = false;

    OTiles_update_fg_page(self);           /* Update FG Pages, based on new H-Scroll */
    OTiles_update_bg_page(self);           /* Update BG Pages, based on new H-Scroll */
    OTiles_v_scroll_tilemaps(self);        /* Set V-Scroll values for Tilemaps */
}

/* Note this is called in attract mode, when we need to loop back to Stage 1, from the final stage. */
/* Source: 0xD982 */
static void OTiles_loop_to_stage1(OTiles* self)
{
    opalette.pal_manip_ctrl = 1;    /* Enable palette fade routines to transition between levels */
    OTiles_init_tilemap(self, 0);                 /* Initalize Default Tilemap (Stage 1) */
    OPalette_setup_sky_change(&opalette);    /* Setup data in RAM necessary for sky palette fade. */
}

/* Clear the name tables used by the previous stage's tilemap, which aren't needed anymore */
/* Source: 0xDC3E */
static void OTiles_clear_old_name_table(OTiles* self)
{
    self->clear_name_tables = false; /* Denote tilemaps have been cleared */

    /* Odd Stages */
    if (self->page & 1)
    {
        /* Clear FG Tiles 2 [4 pages, (each 64x32 page table)] */
        { uint32_t i; for (i = 0x104C00; i < 0x108C00; i += 2)
            Video_write_tile16(&video, i, 0); }

        /* Clear BG Tiles 2 [3 pages] */
        { uint32_t i; for (i = 0x10B700; i < 0x10E700; i += 2)
            Video_write_tile16(&video, i, 0); }
    }
    /* Even */
    else
    {
        /* Clear FG Tiles 1 [4 pages, (each 64x32 page table)] */
        { uint32_t i; for (i = 0x100C00; i < 0x104C00; i += 2)
            Video_write_tile16(&video, i, 0); }

        /* Clear BG Tiles 1 [3 pages] */
        { uint32_t i; for (i = 0x108700; i < 0x10B700; i += 2)
            Video_write_tile16(&video, i, 0); }
    }
}

/* H-Scroll Tilemap Code */
/* */
/* Scroll the tilemaps. Note these are high level routines that don't write to the hardware directly. */
/* */
/* The first routine scrolls during the road-split, using a lookup table of predefined values. */
/* The second routine scrolls during normal gameplay. */
/*  */
/* Source: 0xDAA8 */

static void OTiles_h_scroll_tilemaps(OTiles* self)
{
    /* Road Splitting */
    if (oinitengine.end_stage_props & BIT_0)
    {
        int32_t tilemap_h_target;
        /* Road position is used as an offset into the table. (Note it's reset at beginning of road split) */
        self->h_scroll_lookup = RomLoader_read16(&(roms.rom0), H_SCROLL_TABLE + ((oroad.road_pos >> 16) << 1));
        
        tilemap_h_target = self->h_scroll_lookup << 5;
        tilemap_h_target <<= 16;
        { int32_t tilemap_x = tilemap_h_target - (self->tilemap_h_scr << 5);
        if (tilemap_x != 0) 
        {
            tilemap_x >>= 8;
            if (tilemap_x == 0) 
                self->tilemap_h_scr = (self->tilemap_h_scr & 0xFFFF) | (self->h_scroll_lookup << 16);
            else
                self->tilemap_h_scr += tilemap_x;
        }
        else
        {
            /* DB1E */
            self->tilemap_h_scr += (tilemap_x >> 8);
        }
     }}
    /* Road Not Splitting */
    else
    {
        /* scroll_tilemap: */
        if (oinitengine.rd_split_state != SPLIT_NONE && 
            oinitengine.rd_split_state <= 4) return;

        { int32_t tilemap_h_target = (oroad.tilemap_h_target << 5) & 0xFFFF;
        tilemap_h_target <<= 16;
        { int32_t tilemap_x = tilemap_h_target - (self->tilemap_h_scr << 5);
        if (tilemap_x != 0) 
        {
            tilemap_x >>= 8;
            if (tilemap_x == 0) 
                self->tilemap_h_scr = (self->tilemap_h_scr & 0xFFFF) | (oroad.tilemap_h_target << 16);
            else
                self->tilemap_h_scr += tilemap_x;
        }   
        else
        {
            /* DB1E */
            self->tilemap_h_scr += (tilemap_x >> 8);
        }   
     } }}
}

/* V-Scroll Tilemap Code */
/* */
/* Scroll the tilemaps.  */
/* */
/* Inputs: */
/* a6 = 0x60F00 */
/* */
/* Source: 0xDBB8 */
static void OTiles_v_scroll_tilemaps(OTiles* self)
{
    oroad.horizon_y_bak = (oroad.horizon_y_bak + oroad.horizon_y2) >> 1;
    { int32_t d0 = (0x100 - oroad.horizon_y_bak - self->tilemap_v_off - self->vswap_off);
    self->tilemap_v_scr ^= d0;

    if (d0 < 0)
    {
        self->fg_psel = (self->fg_psel >> 8) | ((self->fg_psel & 0xFF) << 8); /* Swap */
        self->bg_psel = (self->bg_psel >> 8) | ((self->bg_psel & 0xFF) << 8); /* Swap */
    }
    self->tilemap_v_scr = d0;         /* Write d0 to master V-Scroll */
    self->fg_v_scroll = d0;           /* Write d0 to FG V-Scroll (ready for HW write) */
    self->bg_v_scroll = d0;           /* Write d0 to BG V-Scroll (ready for HW write) */
 }}

/* Update FG Page Values */
/* */
/* - Inverts H-Scroll if road splitting */
/* - Converts master H-Scroll value into values ready to be written to HW (not written here though) */
/* - Lookup Correct Page Select Of Background, based on H-Scroll */
/* */
/* This method assumes the new H-Scroll value has been set. */
/* */
/* Source: DB26 */

static void OTiles_update_fg_page(OTiles* self)
{
    int32_t rol7;
    int16_t h = self->tilemap_h_scr >> 16;
    if (oinitengine.rd_split_state == SPLIT_NONE)
        h = -h;

    self->fg_h_scroll = h;
    
    /* Choose Page 0 - 3 */
    rol7 = h << 7;
    h = ((rol7 >> 16) & 3) << 1;
    
    { uint8_t cur_stage = self->page_split ? self->page + 1 : self->page;

    cur_stage &= 1;
    cur_stage *= 8;
    h += cur_stage;
    self->fg_psel = RomLoader_read16(&(roms.rom0), TILES_PAGE_FG1 + h);
 }}

static void OTiles_update_bg_page(OTiles* self)
{
    int32_t rol7;
    int16_t h = self->tilemap_h_scr >> 16;

    if (oinitengine.rd_split_state == SPLIT_NONE)
        h = -h;

    h &= 0x7FF;
    h = (h + (h << 1)) >> 2;
    
    self->bg_h_scroll = h;

    /* Choose Page 0 - 3 */
    rol7 = h << 7;
    h = ((rol7 >> 16) & 3) << 1;

    { uint8_t cur_stage = self->page_split ? self->page + 1 : self->page;

    cur_stage &= 1;
    cur_stage = ((cur_stage * 2) + cur_stage) << 1;
    h += cur_stage;
    self->bg_psel = RomLoader_read16(&(roms.rom0), TILES_PAGE_BG1 + h);
 }}

/* Initalize Next Tilemap. On Level Switch. */
/* Source: 0xD994 */
static void OTiles_init_next_tilemap(OTiles* self)
{
    self->h_scroll_lookup = 0;
    self->clear_name_tables = false;
    self->page_split = false;
    opalette.pal_manip_ctrl = 1; /* Enable palette fade routines to transition between levels */
    
    switch (self->tilemap_setup & 1)
    {
        /* Setup New Tilemaps */
        /* Note that we copy to a location in tileram depending on the level here, so that the upcoming BG & FG */
        /* tilemap is loaded onto alternate name tables in tile ram. */
        case SETUP_TILES:
            OTiles_init_tilemap_props(self, oroad.stage_lookup_off + 8);
            OTiles_copy_fg_tiles(self, self->page & 1 ? 0x100F80 : 0x104F80);
            OTiles_copy_bg_tiles(self, self->page & 1 ? 0x108F80 : 0x10BF80);
            self->tilemap_setup = SETUP_PAL;
            break;

        /* Setup New Palettes */
        case SETUP_PAL:
            OTiles_init_tilemap_palette(self, oroad.stage_lookup_off + 8);
            self->tilemap_ctrl = TILEMAP_SPLIT;
            break;
    }
}

/* Copy relevant palette to Palette RAM, for new FG and BG layers. */
/* */
/* I've reworked this routine to function with both Jap and USA courses. */
/* Originally, both versions had a separate version of this routine. */
/* */
/* Source: 0xDD94 */
void OTiles_init_tilemap_palette(OTiles* self, uint16_t stage_id)
{
    /* Get internal level number */
    uint8_t level = trackloader.stage_data[stage_id];

    switch (level)
    {
        case 0x3C:
            return;

        /* Stage 2 */
        case 0x1E:
            OTiles_copy_to_palram(self, 2, TILEMAP_PALS + 0xC0, 0x120780);
            OTiles_copy_to_palram(self, 0, TILEMAP_PALS + 0x100, 0x1205F0);
            break;

        case 0x3B:
        case 0x25:
            OTiles_copy_to_palram(self, 0, TILEMAP_PALS + 0x60, 0x1205F0);
            OTiles_copy_to_palram(self, 1, TILEMAP_PALS + 0x20, 0x1205A0);
            break;

        /* Stage 3 */
        case 0x20:
            return;

        case 0x2F:
            OTiles_copy_to_palram(self, 3, TILEMAP_PALS + 0xE0, 0x120600);
            break;

        case 0x2A:
            return;

        /* Stage 4 */
        case 0x2D:
            return;

        case 0x35:
            OTiles_copy_to_palram(self, 3, TILEMAP_PALS, 0x1203C0);
            OTiles_copy_to_palram(self, 7, TILEMAP_PALS + 0x10, 0x1200C0);
            break;

        case 0x33:
            return;

        case 0x21:
            OTiles_copy_to_palram(self, 3, TILEMAP_PALS + 0x120, 0x120600);
            OTiles_copy_to_palram(self, 1, TILEMAP_PALS + 0x130, 0x1206C0);
            break;

        /* Stage 5: */
        case 0x32:
            OTiles_copy_to_palram(self, 1, TILEMAP_PALS + 0xF0, 0x1202A0);
            OTiles_copy_to_palram(self, 2, TILEMAP_PALS + 0x40, 0x120780);
            break;

        case 0x23:
            OTiles_copy_to_palram(self, 1, TILEMAP_PALS + 0x80, 0x1202A0);
            break;

        case 0x38:
            OTiles_copy_to_palram(self, 1, TILEMAP_PALS + 0x110, 0x1206C0);
            OTiles_copy_to_palram(self, 1, TILEMAP_PALS + 0x30, 0x120780);
            break;

        case 0x22:
            OTiles_copy_to_palram(self, 3, TILEMAP_PALS + 0x50, 0x120600);
            OTiles_copy_to_palram(self, 7, TILEMAP_PALS + 0x90, 0x1200C0);
            break;

        case 0x26:
            OTiles_copy_to_palram(self, 1, TILEMAP_PALS + 0xD0, 0x1202A0);
            OTiles_copy_to_palram(self, 1, TILEMAP_PALS + 0xB0, 0x120720);
            OTiles_copy_to_palram(self, 0, TILEMAP_PALS + 0xB0, 0x1207B0);
            break;
    }
}

static void OTiles_copy_to_palram(OTiles* self, const uint8_t blocks, uint32_t src, uint32_t dst)
{
    { uint8_t i; for (i = 0; i <= blocks; i++)
    {
        Video_write_pal32_a(&video, &dst, RomLoader_read32(&(roms.rom0), src));
        Video_write_pal32_a(&video, &dst, RomLoader_read32(&(roms.rom0), src + 0x4));
        Video_write_pal32_a(&video, &dst, RomLoader_read32(&(roms.rom0), src + 0x8));
        Video_write_pal32_a(&video, &dst, RomLoader_read32(&(roms.rom0), src + 0xc));
    } }
}

/* New Tilemap Initialized - Scroll both tilemaps during tilesplit */
/* */
/* Source: 0xDA18 */
static void OTiles_split_tilemaps(OTiles* self)
{
    /* Roads Splitting */
    if (oinitengine.rd_split_state < 6)
    {
        OTiles_h_scroll_tilemaps(self);
        OTiles_update_fg_page_split(self);
        OTiles_update_bg_page_split(self);
        OTiles_v_scroll_tilemaps(self);
    }
    /* Roads Merging */
    else
    {
        self->tilemap_ctrl = TILEMAP_SCROLL;
        self->page_split = true;
        oinitengine.end_stage_props &= ~BIT_0; /* Denote Sky Palette Change Done */
        self->h_scroll_lookup = 0;
        self->clear_name_tables = true; /* Erase old tile name tables */
    }
}

/* Setup Foreground tilemap, with relevant h-scroll and page information. Ready for forthcoming HW write. */
/* */
/* Source: 0xDA54 */
static void OTiles_update_fg_page_split(OTiles* self)
{
    self->fg_h_scroll = self->tilemap_h_scr >> 16;
    self->fg_psel = RomLoader_read16(&(roms.rom0), TILES_PAGE_FG2 + ((self->page & 1) ? 0x6 : 0xE));
}

/* Setup Background tilemap, with relevant h-scroll and page information. Ready for forthcoming HW write. */
/* */
/* Source: 0xDA78 */
static void OTiles_update_bg_page_split(OTiles* self)
{
    self->bg_h_scroll = (((self->tilemap_h_scr >> 16) & 0xFFF) * 3) >> 2;
    self->bg_psel = RomLoader_read16(&(roms.rom0), TILES_PAGE_BG2 + ((self->page & 1) ? 0x4 : 0xA));
}

/* Fill tilemap background with a solid color */
/* */
/* Source: 0xE188 */
void OTiles_fill_tilemap_color(OTiles* self, uint16_t color)
{
    uint32_t pal_addr   = 0x1204C2;
    uint32_t dst        = 0x10F000;
    const uint16_t TILE = color == 0 ? 0x20 : 0x1310;  /* Default tile value for background */

    OTiles_set_scroll(self, 0, 0); /* Reset scroll */

    Video_write_pal16(&video, &pal_addr, color);

    { uint16_t i; for (i = 0; i <= 0x7FF; i++)
        Video_write_tile16_a(&video, &dst, TILE); }
}

/* Set Tilemap Scroll. Reset Pages */
void OTiles_set_scroll(OTiles* self, int16_t h_scroll, int16_t v_scroll)
{
    self->tilemap_ctrl = TILEMAP_SCROLL; /* Use Palette */
    self->fg_h_scroll = h_scroll;
    self->bg_h_scroll = h_scroll;
    self->fg_v_scroll = v_scroll;
    self->bg_v_scroll = v_scroll;

    self->fg_psel = 0xFFFF;
    self->bg_psel = 0xFFFF;
}