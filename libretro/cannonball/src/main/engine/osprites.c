/***************************************************************************
    Sprite Handling Routines.
    
    - Initializing Sprites from level data.
    - Mapping palettes to sprites.
    - Ordering sprites by priority.
    - Adding shadows to sprites where appropriate.
    - Clipping sprites based on priority in relation to road hardware.
    - Conversion from internal format to output format required by hardware.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "../trackloader.h"

#include "oanimseq.h"
#include "ocrash.h"
#include "oferrari.h"
#include "olevelobjs.h"
#include "osprites.h"
#include "otraffic.h"
#include "data/ozoom_lookup.h"

static void OSprites_sprite_control(OSprites* self);
static void OSprites_hide_hwsprite(OSprites* self, oentry*, osprite*);
static void OSprites_finalise_sprites(OSprites* self);

OSprites osprites;



void OSprites_init(OSprites* self)
{
    /* Set activated number of sprites based on config */
    self->no_sprites = config.engine.level_objects ? SPRITE_ENTRIES : 0x4F;

    /* Also handled by clear_palette_data() now */
    { uint16_t i; for (i = 0; i < PAL_LOOKUP_LENGTH; i++)
        self->pal_lookup[i] = 0; }

    { uint16_t i; for (i = 0; i < 0x2000; i++)
    {
        self->sprite_order[i] = 0;
        self->sprite_order2[i] = 0;
    } }

    /* Reset hardware entries */
    { uint16_t i; for (i = 0; i < JUMP_ENTRIES_TOTAL; i++)
        osprite_init(&(self->sprite_entries[i])); }

    { uint8_t i; for (i = 0; i < SPRITE_ENTRIES; i++)
        oentry_init(&self->jump_table[i], i); }

    /* Ferrari + Passenger Sprites */
    oentry_init(&self->jump_table[SPRITE_FERRARI], SPRITE_FERRARI);        /* Ferrari */
    oentry_init(&self->jump_table[SPRITE_PASS1], SPRITE_PASS1);
    oentry_init(&self->jump_table[SPRITE_PASS2], SPRITE_PASS2);
    oentry_init(&self->jump_table[SPRITE_SHADOW], SPRITE_SHADOW);          /* Shadow Settings */
    self->jump_table[SPRITE_SHADOW].shadow = 7;
    oentry_init(&self->jump_table[SPRITE_SMOKE1], SPRITE_SMOKE1);
    oentry_init(&self->jump_table[SPRITE_SMOKE2], SPRITE_SMOKE2);

    OFerrari_init(&oferrari, &self->jump_table[SPRITE_FERRARI], &self->jump_table[SPRITE_PASS1], &self->jump_table[SPRITE_PASS2], &self->jump_table[SPRITE_SHADOW]);
    
    /* ------------------------------------------------------------------------ */
    /* Traffic in Right Hand Lane At Start of Stage 1 */
    /* ------------------------------------------------------------------------ */

    { uint8_t i; for (i = SPRITE_TRAFF1; i <= SPRITE_TRAFF8; i++)
    {
        oentry_init(&self->jump_table[i], i);      
        self->jump_table[i].control |= SHADOW;
        self->jump_table[i].addr = outrun.adr.sprite_porsche; /* Initial offset of traffic sprites. Will be changed. */
    } }

    /* ------------------------------------------------------------------------ */
    /* Crash Sprites */
    /* ------------------------------------------------------------------------ */

    { uint8_t i; for (i = SPRITE_CRASH; i <= SPRITE_CRASH_PASS2_S; i++)
    {
        oentry_init(&self->jump_table[i], i);
    } }

    self->jump_table[SPRITE_CRASH_PASS1].draw_props = ANCHOR_BOTTOM;
    self->jump_table[SPRITE_CRASH_PASS2].draw_props = ANCHOR_BOTTOM;

    self->jump_table[SPRITE_CRASH_PASS1_S].shadow     = 7;
    self->jump_table[SPRITE_CRASH_PASS1_S].addr       = outrun.adr.shadow_data;
    self->jump_table[SPRITE_CRASH_PASS2_S].shadow     = 7;
    self->jump_table[SPRITE_CRASH_PASS2_S].draw_props = ANCHOR_BOTTOM;
    self->jump_table[SPRITE_CRASH_PASS2_S].addr       = outrun.adr.shadow_data;
    
    self->jump_table[SPRITE_CRASH_SHADOW].shadow     = 7;
    self->jump_table[SPRITE_CRASH_SHADOW].zoom       = 0x80;
    self->jump_table[SPRITE_CRASH_SHADOW].draw_props = ANCHOR_BOTTOM;
    self->jump_table[SPRITE_CRASH_SHADOW].addr       = outrun.adr.shadow_data;

    OCrash_init(&ocrash, 
        &self->jump_table[SPRITE_CRASH], 
        &self->jump_table[SPRITE_CRASH_SHADOW], 
        &self->jump_table[SPRITE_CRASH_PASS1],
        &self->jump_table[SPRITE_CRASH_PASS1_S],
        &self->jump_table[SPRITE_CRASH_PASS2],
        &self->jump_table[SPRITE_CRASH_PASS2_S]
    );

    /* ------------------------------------------------------------------------ */
    /* Animation Sequence Sprites */
    /* ------------------------------------------------------------------------ */

    oentry_init(&self->jump_table[SPRITE_FLAG], SPRITE_FLAG);
    OAnimSeq_init(&oanimseq, self->jump_table);
    
    self->seg_pos             = 0;
    self->seg_total_sprites   = 0;
    self->seg_sprite_freq     = 0;
    self->seg_spr_offset2     = 0;
    self->seg_spr_offset1     = 0;
    self->seg_spr_addr        = 0;

    self->do_sprite_swap      = false;
    self->sprite_scroll_speed = 0;
    self->shadow_offset       = 0;
    self->sprite_count        = 0;
    self->spr_cnt_main        = 0;
    self->spr_cnt_shadow      = 0;

    self->spr_col_pal         = 0;
    self->pal_copy_count      = 0;    
}

/* Swap Sprite RAM And Update Palette Data */
void OSprites_update_sprites(OSprites* self)
{
	if (self->do_sprite_swap)
	{
        self->do_sprite_swap = false;
        hwsprites_swap(video.sprite_layer);
        OSprites_copy_palette_data(self);
	}
}

/* Disable All Sprite Entries */
/* Source: 0x4A50 */
void OSprites_disable_sprites(OSprites* self)
{
    { uint8_t i; for (i = 0; i < SPRITE_ENTRIES; i++)
        self->jump_table[i].control &= ~ENABLE; }
}

void OSprites_tick(OSprites* self)
{
    OSprites_sprite_control(self);
}

/* Sprite Control */
/* */
/* Source: 3BEE */
/* */
/* Notes: */
/* - Setup Jump Table Entry #2, the sprite control. This in turn is used to control and setup all the sprites. */
/* - Read 4 Byte Entry From Road_Seg_Adr1 which indicates the upcoming block of sprite data for the level */
/* - This first block of data specifies the position, total number of sprites in the block we want to try rendering  */
/*   and appropriate lookup for the sprite number/frequency info. */
/* */
/* - This second table in memory specifies the frequency and number of sprites in the sequence.  */
/*   */
/* - The second table also contains the actual sprite info (x,y,palette,type). This can be multipled sprites. */
/* */
/* ---------------------------------- */
/* */
/* road_seg_addr1 Format: [4 byte boundaries] */
/* */
/* [+0] Road Position [Word] */
/* [+2] Number Of Sprites In Segment [Byte] */
/* [+3] Sprite Data Entry Number From Lookup Table * 4 [Byte] */
/* */
/* Sprite Master Table Format @ 0x1A43C: */
/* */
/* A Table Containing a series of longs. Each address in this table represents: */
/* */
/* [+0] Sprite Frequency Value Bitmask [Word] */
/* [+2] Reload Value For Sprite Info Offset [Word] */
/* [+4] Start of table with x,y,type,palette etc. */
/*      This table can appear as many times as desired in each block and follows the format below, starting +4: */
/* */
/* ------------------------------------------------- */
/* [+0] [Byte] Bit 0 = H-Flip Sprite */
/*             Bit 1 = Enable Shadows */
/*             */
/*             Bits 4-7 = Routine Draw Number */
/* [+1] [Byte] Sprite X World Position */
/* [+2] [Word] Sprite Y World Position */
/* [+5] [Byte] Sprite Type */
/* [+7] [Byte] Sprite Palette */
/*------------------------------------------------- */
/* */
/* The Frequency bitmask, for example 111000111 00111000 */
/* rotates left, and whenever a bit is set, a sprite from the sequence is rendered. */
/* */
/* When a bit is unset no draw occurs on that call. */
/* */
/* The entire sequence can repeat, until the max sprites counter expires. */
/* */
/* So the above example would draw 3 sprites in succession, then break for three attempts, then three again etc. */static 

void OSprites_sprite_control(OSprites* self)
{
    uint16_t carry;
    uint16_t pos = TrackLoader_read_scenery_pos(&trackloader);

    /* Populate next road segment */
    if (pos <= oroad.road_pos >> 16)
    {
        uint32_t a0;
        uint8_t pattern_index;
        self->seg_pos = pos;                                                          /* Position In Level Data [Word] */
        self->seg_total_sprites = TrackLoader_read_total_sprites(&trackloader);                   /* Number of Sprites In Segment */
        pattern_index = TrackLoader_read_sprite_pattern_index(&trackloader);
        trackloader.scenery_offset += 4;                                        /* Advance to next scenery point */
        
        a0 = TrackLoader_read_scenerymap_table(&trackloader, pattern_index);
        self->seg_sprite_freq = TrackLoader_read16_a(trackloader.scenerymap_data, &a0); /* Scenery Frequency */
        self->seg_spr_offset2 = TrackLoader_read16_a(trackloader.scenerymap_data, &a0); /* Reload value for scenery pattern */
        self->seg_spr_addr = a0;                                                      /* Set ROM address for sprite info lookup (x, y, type) */
                                                                                /* NOTE: Sets to value of a0 itself, not memory location */
        self->seg_spr_offset1 = 0;                                                    /* And Clear the offset into the above table */
    }

    /* Process segment */
    if (self->seg_total_sprites == 0) return;
    if (self->seg_pos > oroad.road_pos >> 16) return;

    /* ------------------------------------------------------------------------ */
    /* Sprite 1 */
    /* ------------------------------------------------------------------------ */
    /* Rotate 16 bit value left. Stick top bit in low bit. */
    carry = self->seg_sprite_freq & 0x8000;
    self->seg_sprite_freq = ((self->seg_sprite_freq << 1) | ((self->seg_sprite_freq & 0x8000) >> 15)) & 0xFFFF;

    if (carry)
    {
        self->seg_total_sprites--;
        self->seg_spr_offset1 -= 8; /*  Decrement rom address offset to point at next sprite [8 byte boundary] */
        if (self->seg_spr_offset1 < 0)
            self->seg_spr_offset1 = self->seg_spr_offset2; /* Reload sprite info offset value */
        OLevelObjs_setup_sprites(&olevelobjs, 0x10400);
    }
 
    if (self->seg_total_sprites == 0)
    {
        self->seg_pos++;
        return;
    }

    /* ------------------------------------------------------------------------ */
    /* Sprite 2 - Second Sprite is slightly set back from the first. */
    /* ------------------------------------------------------------------------ */
    carry = self->seg_sprite_freq & 0x8000;
    self->seg_sprite_freq = ((self->seg_sprite_freq << 1) | ((self->seg_sprite_freq & 0x8000) >> 15)) & 0xFFFF;

    if (carry)
    {
        self->seg_total_sprites--;
        self->seg_spr_offset1 -= 8; /*  Decrement rom address offset to point at next sprite [8 byte boundary] */
        if (self->seg_spr_offset1 < 0)
            self->seg_spr_offset1 = self->seg_spr_offset2; /* Reload sprite info offset value */
        OLevelObjs_setup_sprites(&olevelobjs, 0x10000);
    }

     self->seg_pos++;
}

/* Source: 0x76F4 */
void OSprites_clear_palette_data(OSprites* self)
{
    self->spr_col_pal = 0;
    { int16_t i; for (i = 0; i < PAL_LOOKUP_LENGTH; i++)
        self->pal_lookup[i] = 0; }
}


/* Copy Sprite Palette Data To Palette RAM On Vertical Interrupt */
/*  */
/* Source Address: 0x858E */
/* Input:          Source address in rom of data format */
/* Output:         None */
/* */

void OSprites_copy_palette_data(OSprites* self)
{
    /* Return if no palette entries to copy */
    if (self->pal_copy_count <= 0) return;

    { int16_t i; for (i = 0; i < self->pal_copy_count * 2;)
    {
        /* Palette Data Source Offset (aligned to start of 32 byte boundry, * 32) */
        uint32_t src_addr = self->pal_addresses[i++] << 3;
        uint32_t dst_addr = PAL_SPRITES + (self->pal_addresses[i++] << 5);
        { uint16_t j; for (j = 0; j < 8; j++)
            Video_write_pal32_a(&video, &dst_addr, PALETTE_EXPANSION[src_addr++]); }
    } }
    self->pal_copy_count = 0; /* All entries copied */
}

/* Map Palettes from ROM to Palette RAM for a particular sprite. */
/*  */
/* Source Address: 0x75EA */
/* Input:          Sprite */
/* Output:         None */
/* */
/* Prepares RAM for copy_palette_data routine on vint */

/* Notes: */
/* 1. Checks lookup table to determine whether relevant palette info is copied to ram.  */
/*    Return if already cached */
/* 2. Otherwise set the mapping between ROM and the HW Palette to be used */
/* 3. pal_copy_count contains the number of entries we need to copy */
/* 4. pal_addresses contains the address mapping */

void OSprites_map_palette(OSprites* self, oentry* spr)
{
    uint8_t pal = self->pal_lookup[spr->pal_src];

    /* ----------------------------------- */
    /* Entry is cached. Use entry. */
    /* ----------------------------------- */
    if (pal != 0)
    {
        spr->pal_dst = pal;
        return;
    }
    /* ----------------------------------- */
    /* Entry is not cached. Need to cache. */
    /* ----------------------------------- */

    /* Increment hw colour palette entry to use */
    if (++self->spr_col_pal > 0x7F) return;
    
    spr->pal_dst = self->spr_col_pal; /* Set next available hw sprite colour palette */
    self->pal_lookup[spr->pal_src] = self->spr_col_pal;

    self->pal_addresses[self->pal_copy_count * 2] = spr->pal_src;
    self->pal_addresses[(self->pal_copy_count * 2) + 1] = self->spr_col_pal;

    self->pal_copy_count++;
}

/* Setup Sprite Ordering Table & Shadows */
/*  */
/* Source Address: 0x77A8 */
/* Input:          Sprite To Copy */
/* Output:         None */
/* */
/* Notes: */
/* 1/ Reads Sprite-to-Sprite priority of individual sprite */
/* 2/ Creates ordered sprite table starting at 0x64000 */
/* 3/ The format of this table is detailed in the comments at 0x78B0 */
/* 4/ Optionally adds shadow to sprite if requires */
/* */
/* The end result is a table of sprite entries at 0x64000 */

void OSprites_do_spr_order_shadows(OSprites* self, oentry* input)
{
    uint32_t addr;
    int16_t x;
    uint8_t shadow;
    uint8_t pal_dst;
    uint8_t bytes_to_copy;
    uint16_t priority;
    /* LayOut specific fix to avoid memory crash on over populated scenery segments */
    if (self->spr_cnt_main + self->spr_cnt_shadow >= JUMP_ENTRIES_TOTAL)
        return;

    /* Use priority as lookup into table. Assume we're on boundaries of 0x10 */
    priority = (input->priority & 0x1FF) << 4;
    bytes_to_copy = self->sprite_order[priority];

    /* Maximum number of bytes we want to copy is 0x10 */
    if (bytes_to_copy < 0xE)
    {
        bytes_to_copy++;
        self->sprite_order[priority] = bytes_to_copy;
        self->sprite_order[priority + bytes_to_copy + 1] = input->jump_index; /* put at offset +2 */
        self->spr_cnt_main++;
    }

    /* Code to handle shadows under sprites */
    /* test_shadow:  */
    if (!(input->control & SHADOW)) return;

    /* LayOut specific fix to avoid memory crash on over populated scenery segments */
    if (self->spr_cnt_main + self->spr_cnt_shadow >= JUMP_ENTRIES_TOTAL)
        return;

    input->dst_index = self->spr_cnt_shadow;
    self->spr_cnt_shadow++;                       /* Increment total shadow count */
    
    pal_dst = input->pal_dst;
    shadow = input->shadow;
    x = input->x;
    addr = input->addr;
    input->pal_dst = 0;                     /* clear colour palette */
    input->shadow = 7;                      /* Set NEW priority & shadow settings */
    
    input->x += (input->road_priority * self->shadow_offset) >> 9; /* d0 = sprite z / distance into screen */

    if (input->control & TRAFFIC_SPRITE)
    {
        input->addr = outrun.adr.sprite_shadow_small;
        input->x = x;
    }
    else
    {
        input->addr = RomLoader_read32(roms.rom0p, outrun.adr.shadow_frames + 0x3C);
    }

    OSprites_do_sprite(self, input);           /* Create Shadowed Version Of Sprite For Hardware */
    
    input->pal_dst = pal_dst;   /* Restore Sprite Colour Palette */
    input->shadow = shadow;     /* ...and other values */
    input->x = x;
    input->addr = addr;
}

/* Sprite Copying Routine */
/*  */
/* Source Address: 0x78B0 */
/* Input:          None */
/* Output:         None */
/* */

void OSprites_sprite_copy(OSprites* self)
{
    if (self->spr_cnt_main == 0)
    {
        OSprites_finalise_sprites(self);
        return;
    }

    if (self->spr_cnt_main + self->spr_cnt_shadow > 0x7F)
    {
        self->spr_cnt_main = self->spr_cnt_shadow = 0;
        OSprites_finalise_sprites(self);
        return;
    }

    { uint32_t spr_cnt_main_copy = self->spr_cnt_main;
        uint16_t cnt_shadow_copy;

    /* look up in sprite_order */
    int32_t src_addr = -0x10;

    /* copy to sprite_entries */
    uint32_t dst_index = 0;

    /* Get next relevant entry (number of bytes to copy, followed by indexes of sprites) */
    while (spr_cnt_main_copy != 0)
    {
        src_addr += 0x10;
        { uint8_t bytes_to_copy = self->sprite_order[src_addr]; /* warning: actually reads a word here normally, so this is wrong */

        /* Copy the required number of bytes */
        if (bytes_to_copy != 0)
        {
            int32_t src_offset = src_addr + 2;

            do
            {
                /* Sprite Index To Draw */
                self->sprite_order2[dst_index++] = self->sprite_order[src_offset++];                
                if (--spr_cnt_main_copy == 0) break; /* Loop continues until this is zero. */
            }
            /* Decrement number of bytes to copy */
            while (--bytes_to_copy != 0);
        }

        /* Clear number of bytes to copy */
        self->sprite_order[src_addr] = 0; 
     }}

    /* cont2: */
    cnt_shadow_copy = self->spr_cnt_shadow;

    /* next_sprite */
    { uint16_t i; for (i = 0; i < self->spr_cnt_main; i++)
    {
        uint16_t jump_index = self->sprite_order2[i];
        oentry *entry = &self->jump_table[jump_index];
        entry->dst_index = cnt_shadow_copy;
        cnt_shadow_copy++;
        OSprites_do_sprite(self, entry);
    } }

    OSprites_finalise_sprites(self);
 }}

/* Was originally labelled set_end_marker */
/*  */
/* Source Address: 0x7942 */
/* Input:          None */
/* Output:         None */
/* */static 

void OSprites_finalise_sprites(OSprites* self)
{
    self->sprite_count = self->spr_cnt_main + self->spr_cnt_shadow;
    
    /* Set end sprite marker */
    self->sprite_entries[self->sprite_count].data[0] = 0xFFFF;
    self->sprite_entries[self->sprite_count].data[1] = 0xFFFF;

    /* TODO: Code to wait for interrupt goes here */
    OSprites_blit_sprites(self);
    OTraffic_traffic_logic(&otraffic);
    OTraffic_traffic_sound(&otraffic);
    self->spr_cnt_main = self->spr_cnt_shadow = 0;

    /* Ready to swap buffer and blit */
    self->do_sprite_swap = true;
}

/* Copy Sprite Data to Sprite RAM  */
/*  */
/* Source Address: 0x97E4 */
/* Input:          None */
/* Output:         None */
/* */

void OSprites_blit_sprites(OSprites* self)
{
    uint32_t dst_addr = SPRITE_RAM;

    { uint16_t i; for (i = 0; i <= self->sprite_count; i++)
    {
        uint16_t* data = self->sprite_entries[i].data;

        /* Write twelve bytes */
        Video_write_sprite16(&video, &dst_addr, data[0]);
        Video_write_sprite16(&video, &dst_addr, data[1]);
        Video_write_sprite16(&video, &dst_addr, data[2]);
        Video_write_sprite16(&video, &dst_addr, data[3]);
        Video_write_sprite16(&video, &dst_addr, data[4]);
        Video_write_sprite16(&video, &dst_addr, data[5]);
        Video_write_sprite16(&video, &dst_addr, data[6]);

        /* Allign on correct boundary */
        dst_addr += 2;
    } }
}

/* Convert Sprite From Internal Software Format To Hardware Format */
/*  */
/* Source Address: 0x94EC */
/* Input:          Sprite To Copy */
/* Output:         None */
/* */
/* 1. Copies Sprite Information From Jump Table Area To RAM */
/* 2. Stores In Similar Format To Sprite Hardware, but with 4 extra bytes of scratch data on end */
/* 3. Note: Mostly responsible for setting x,y,width,height,zoom,pitch,priorities etc. */
/* */
/* 0x11ED2: Table of Sprite Addresses for Hardware. Contains: */
/* */
/* 5 x 10 bytes. One block for each sprite size lookup.  */
/* The exact sprite is selected using the ozoom_lookup.hpp table. */
/* */
/* + 0 : [Byte] Unused */
/* + 1 : [Byte] Width Helper Lookup  [Offsets into 0x20000 (the width and height table)] */
/* + 2 : [Byte] Line Data Width */
/* + 3 : [Byte] Height Helper Lookup [Offsets into 0x20000 (the width and height table)] */
/* + 4 : [Byte] Line Data Height */
/* + 5 : [Byte] Sprite Pitch */
/* + 7 : [Byte] Sprite Bank */
/* + 8 : [Word] Offset Within Sprite Bank */

void OSprites_do_sprite(OSprites* self, oentry* input)
{
    uint16_t width;
    osprite* output;
    input->control |= DRAW_SPRITE; /* Display input sprite */

    /* Get Correct Output Entry */
    output = &self->sprite_entries[input->dst_index];

    /* Copy address sprite was copied from. */
    /* todo: pass pointer? */
    output->scratch = input->jump_index;

    /* Hide Sprite if zoom lookup not set */
    if (input->zoom == 0)
    {
        OSprites_hide_hwsprite(self, input, output);
        return;
    }

    /* Sprite Width/Height Settings */
    width = 0;
    { uint16_t height = 0;
        uint32_t src_offsets;
        uint16_t lookup_mask;
    
    /* Set real h/v zoom values */
    uint32_t index = (input->zoom * 4);
    osprite_set_vzoom(output, ZOOM_LOOKUP[index]); /* note we don't increment src_rom here */
    osprite_set_hzoom(output, ZOOM_LOOKUP[index++]);

    /* ------------------------------------------------------------------------- */
    /* Set width & height values using lookup */
    /* ------------------------------------------------------------------------- */
    lookup_mask = ZOOM_LOOKUP[index++];
    
    /* This is the address of the frame required for the level of zoom we're using */
    /* There are 5 unique frames that are typically used for zoomed sprites. */
    /* which correspond to different screen sizes */
    src_offsets = input->addr + ZOOM_LOOKUP[index];

    { uint16_t d0 = input->draw_props | (input->zoom << 8);
        int16_t sprite_x1;
    uint16_t top_bit = d0 & 0x8000;
    d0 &= 0x7FFF; /* Clear top bit */
    
    if (top_bit == 0)
    {
        if (ZOOM_LOOKUP[index] != SIZE1) /* Not largest sized sprite */
        {
            lookup_mask += 0x4000;
            d0 = lookup_mask;
        }

        d0 = (d0 & 0xFF00) + RomLoader_read8(roms.rom0p, src_offsets + 1);
        width = RomLoader_read8(roms.rom0p, WH_TABLE + d0);
        d0 = (d0 & 0xFF00) + RomLoader_read8(roms.rom0p, src_offsets + 3);
        height = RomLoader_read8(roms.rom0p, WH_TABLE + d0);
    }
    /* loc_9560: */
    else
    {
        d0 &= 0x7C00;
        { uint16_t h = d0;

        d0 = (d0 & 0xFF00) + RomLoader_read8(roms.rom0p, src_offsets + 1);
        width = RomLoader_read8(roms.rom0p, WH_TABLE + d0);
        d0 &= 0xFF;
        width += d0;
        
        h |= RomLoader_read8(roms.rom0p, src_offsets + 3);
        height = RomLoader_read8(roms.rom0p, WH_TABLE + h);
        h &= 0xFF;
        height += h;

     }}
    /* loc 9582: */
    input->width = width;
    
    /* ------------------------------------------------------------------------- */
    /* Set Sprite X & Y Values */
    /* ------------------------------------------------------------------------- */
    OSprites_set_sprite_xy(self, input, output, width, height);
    
    /* Here we need the entire value set by above routine, not just top 0x1FF mask! */
    sprite_x1 = osprite_get_x(output);
    { int16_t sprite_x2 = sprite_x1 + width;
        uint16_t road_y_index;
    int16_t sprite_y1 = osprite_get_y(output);
    int16_t sprite_y2 = sprite_y1 + height;

    const uint16_t x1_bounds = 512 + config.s16_x_off;
    const uint16_t x2_bounds = 192 - config.s16_x_off;

    /* Hide Sprite if off screen (note bug fix to solve shadow wrapping issue on original game) */
    /* I think this bug might be permanently fixed with the introduction of widescreen mode */
    /* as I had to change the storage size of the x-cordinate.  */
    /* Unsetting fix_bugs may no longer revert to the original behaviour. */
    if (sprite_y2 < 256 || sprite_y1 > 479 ||
        sprite_x2 < x2_bounds || (config.engine.fix_bugs ? sprite_x1 >= x1_bounds : sprite_x1 > x1_bounds))
    {
        OSprites_hide_hwsprite(self, input, output);
        return;
    }

    /* ------------------------------------------------------------------------- */
    /* Set Palette & Sprite Bank Information */
    /* ------------------------------------------------------------------------- */
    osprite_set_pal(output, input->pal_dst); /* Set Sprite Colour Palette */
    osprite_set_offset(output, RomLoader_read16(roms.rom0p, src_offsets + 8)); /* Set Offset within selected sprite bank */
    osprite_set_bank(output, RomLoader_read8(roms.rom0p, src_offsets + 7) << 1); /* Set Sprite Bank Value */

    /* ------------------------------------------------------------------------- */
    /* Set Sprite Height */
    /* ------------------------------------------------------------------------- */
    if (sprite_y1 < 256)
    {
        int16_t y_adj = -(sprite_y1 - 256);
        y_adj *= RomLoader_read16(roms.rom0p, src_offsets + 2); /* Width of line data (Unsigned multiply) */
        y_adj /= height; /* Unsigned divide */
        y_adj *= RomLoader_read16(roms.rom0p, src_offsets + 4); /* Length of line data (Unsigned multiply) */
        osprite_inc_offset(output, y_adj);
        output->data[0x0] = (output->data[0x0] & 0xFF00) | 0x100; /* Mask on negative y index */
        osprite_set_height(output, (uint8_t) sprite_y2);
    }
    else
    {
        osprite_set_height(output, (uint8_t) height);
    }
    
    /* ------------------------------------------------------------------------- */
    /* Set Sprite Height Taking Elevation Of Road Into Account For Clipping Purposes */
    /* */
    /* Word 0: Priority of section */
    /* Word 1: Height of section */
    /* */
    /* Source: 0x9602 */
    /* ------------------------------------------------------------------------- */

    /* Start of priority elevation data in road ram */
    road_y_index = oroad.road_p0 + 0x280;
    
    /* Priority List Not Populated (Flat Elevation) */
    if (oroad.road_y[road_y_index + 0] == 0 && oroad.road_y[road_y_index + 1] == 0)
    {
        /* set_spr_height: */
        if (sprite_y2 > 0x1DF)
            osprite_sub_height(output, sprite_y2 - 0x1DF);
    }
    /* Priority List Populated (Elevated Section of road) */
    else
    {
        /* Count number of height_entries */
        int16_t height_entry = 0;
        do
        {
            height_entry++;
            road_y_index += 2;
        }
        while (oroad.road_y[road_y_index + 0] != 0 && oroad.road_y[road_y_index + 1] != 0);

        height_entry--;

        do
        {
            road_y_index -= 2;
        }
        while (--height_entry > 0 && input->road_priority > oroad.road_y[road_y_index + 0]);

        /* Sprite has higher priority, draw sprite */
        if (input->road_priority > oroad.road_y[road_y_index])
        {
            /* set_spr_height: */
            if (sprite_y2 > 0x1DF)
                osprite_sub_height(output, sprite_y2 - 0x1DF);
        }
        /* Road has higher priority, clip sprite */
        else
        {
            /* 9630: */
            int16_t road_elevation = -oroad.road_y[road_y_index + 1] + 0x1DF;
            if (sprite_y1 > road_elevation)
            {
                OSprites_hide_hwsprite(self, input, output);
                return;
            }
            else if (sprite_y2 <= road_elevation)
            {
                if (sprite_y2 > 0x1DF)
                    osprite_sub_height(output, sprite_y2 - 0x1DF);
            }
            else
            {
                osprite_sub_height(output, sprite_y2 - road_elevation);
            }
        }
    }

    /* cont2: */
    OSprites_set_hrender(self, input, output, RomLoader_read16(roms.rom0p, src_offsets + 4), width);
    
    /* ------------------------------------------------------------------------- */
    /* Set Sprite Pitch & Priority */
    /* ------------------------------------------------------------------------- */
    osprite_set_pitch(output, RomLoader_read8(roms.rom0p, src_offsets + 5) << 1);
    osprite_set_priority(output, input->shadow << 4); /* todo: where does this get set? */
 } } }}

/* Hide Input And Output Entry */static 
void OSprites_hide_hwsprite(OSprites* self, oentry* input, osprite* output)
{
    input->control &= ~DRAW_SPRITE; /* Hide input sprite */
    osprite_hide(output);
}

/* Sets Sprite Render Point */
/*  */
/* Source Address: 0x967C */
/* Input:          Jump Table Entry, Output Sprite Entry, Width & Height */
/* Output:         Updated Sprite Output Entry */
/* */

void OSprites_set_sprite_xy(OSprites* self, oentry* input, osprite* output, uint16_t width, uint16_t height)
{
    int16_t x;
    uint8_t anchor = input->draw_props;

    /* ------------------------------------------------------------------------- */
    /* Set Y Render Point */
    /* ------------------------------------------------------------------------- */

    int16_t y = input->y;

    switch((anchor & 0xC) >> 2)
    {
        /* Anchor Center */
        case 0:
        case 3:
            height >>= 1;
            y -= height;
            osprite_set_y(output, y + 256);
            break;
            
        /* Anchor Left */
        case 1:
            osprite_set_y(output, y + 256);
            break;

        /* Anchor Right */
        case 2:
            y -= height;
            osprite_set_y(output, y + 256);
            break;
    }

    /* ------------------------------------------------------------------------- */
    /* Set X Render Point */
    /* ------------------------------------------------------------------------- */

    x = input->x;

    switch(anchor & 0x3)
    {
        /* Anchor Center */
        case 0:
        case 3:
            width >>= 1;
            x -= width;
            osprite_set_x(output, x + 352);
            break;
            
        /* Anchor Left */
        case 1:
            osprite_set_x(output, x + 352);
            break;

        /* Anchor Right */
        case 2:
            x -= width;
            osprite_set_x(output, x + 352);
            break;
    }
}

/* Determines whether to render sprite left-to-right or right-to-left */
/*  */
/* Source Address: 0x96E4 */
/* Input:          Jump Table Entry, Output Sprite Entry, Offset */
/* Output:         Updated Sprite Output Entry */
/* */
void OSprites_set_hrender(OSprites* self, oentry* input, osprite* output, uint16_t offset, uint16_t width)
{
    uint8_t props = 0x60;
    uint8_t anchor = input->draw_props;

    /* Anchor Top Left: Set Backwards & Left To Right Render */
    if (anchor & 1)
        props = 0x60;
    /* Anchor Bottom Right: Set Forwards & Right To Left Render */
    else if (anchor & 2 || input->x < 0)
        props = 0;

    if (input->control & HFLIP)
    {
        if (props == 0) props = 0x40; /* If H-Flip & Right To Left Render: Read data backwards */
        else props = 0x20; /* If H-Flip && Left To Right: Render left to right & Read data forwards */
    }

    /* setup_bank: */

    /* if reading forwards, increment the offset */
    if ((props & 0x40) == 0)
    {
        osprite_inc_offset(output, offset - 1);
    }

    /* if right to left, increment the x position */
    if ((props & 0x20) == 0)
    {
        osprite_inc_x(output, width);
    }

    props |= 0x80; /* Set render top to bottom */
    osprite_set_render(output, props);
}

/* Helper function to vary the move distance, based on the current frame-rate. */
void OSprites_move_sprite(OSprites* self, oentry* sprite, uint8_t shift)
{
    uint32_t addr = SPRITE_ZOOM_LOOKUP + (((sprite->z >> 16) << 2) | self->sprite_scroll_speed);
    uint32_t value = RomLoader_read32(&(roms.rom0), addr) >> shift;

    if (config.tick_fps == 60)
        value >>= 1;
    else if (config.tick_fps == 120)
        value >>= 2;

    sprite->z += value;
}
