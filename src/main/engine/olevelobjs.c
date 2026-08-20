/***************************************************************************
    Level Object Logic
    
    This class handles rendering most of the objects that comprise a typical
    level. 
    
    - Configures rendering properties (co-ordinates, zoom etc.) 
    - Object specific logic, including collision checks & start lights etc.

    The original codebase contains a large amount of code duplication,
    much of which is duplicated here.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "../trackloader.h"
#include "engine/outils.h"
#include "engine/olevelobjs.h"
#include "engine/ostats.h"

static void OLevelObjs_init_entries(OLevelObjs* self, uint32_t, const uint8_t start_index, const uint8_t);
static void OLevelObjs_setup_sprite(OLevelObjs* self, oentry*, uint32_t);
static void OLevelObjs_setup_sprite_routine(OLevelObjs* self, oentry*);
static void OLevelObjs_sprite_collision_z1c(OLevelObjs* self, oentry*);
static void OLevelObjs_sprite_lights(OLevelObjs* self, oentry*);
static void OLevelObjs_sprite_lights_countdown(OLevelObjs* self, oentry*);
static void OLevelObjs_sprite_grass(OLevelObjs* self, oentry* sprite);
static void OLevelObjs_sprite_water(OLevelObjs* self, oentry* sprite);
static void OLevelObjs_sprite_rocks(OLevelObjs* self, oentry* sprite);
static void OLevelObjs_sprite_debris(OLevelObjs* self, oentry* sprite);
static void OLevelObjs_sprite_minitree(OLevelObjs* self, oentry* sprite);
static void OLevelObjs_do_thickness_sprite(OLevelObjs* self, oentry* sprite, const uint32_t);
static void OLevelObjs_sprite_clouds(OLevelObjs* self, oentry* sprite);
static void OLevelObjs_sprite_normal(OLevelObjs* self, oentry*, uint8_t);
static void OLevelObjs_set_spr_zoom_priority(OLevelObjs* self, oentry*, uint8_t);
static void OLevelObjs_set_spr_zoom_priority2(OLevelObjs* self, oentry*, uint8_t);
static void OLevelObjs_set_spr_zoom_priority_rocks(OLevelObjs* self, oentry*, uint8_t);

OLevelObjs olevelobjs;



/* SetupSpriteEntries */
/* Initialize default sprite entries for game engine. */
/* Called when title initalizes. */
/* */
/* Once setup, this routine is replaced with SpriteControl */
/* */
/* Source: 0x3B48 */
void OLevelObjs_init_startline_sprites(OLevelObjs* self)
{
    /* Return if Music Selection Screen */
    if (outrun.game_state == GS_MUSIC) return; 
    OLevelObjs_init_entries(self, outrun.adr.sprite_def_props1, 0, DEF_SPRITE_ENTRIES);
}

void OLevelObjs_init_timetrial_sprites(OLevelObjs* self)
{
    const uint8_t start_index = outrun.ttrial.level == 0 ? 0 : 18;
    OLevelObjs_init_entries(self, outrun.adr.sprite_def_props1 + (start_index * 0x10), start_index, DEF_SPRITE_ENTRIES);
}

/* Setup hi-score screen sprite entries */
void OLevelObjs_init_hiscore_sprites(OLevelObjs* self)
{
    OLevelObjs_init_entries(self, outrun.adr.sprite_def_props2, 0, HISCORE_SPRITE_ENTRIES);
}static 

void OLevelObjs_init_entries(OLevelObjs* self, uint32_t a4, const uint8_t start_index, const uint8_t no_entries)
{
    /* next_sprite: */
    { uint8_t i; for (i = start_index; i <= no_entries; i++)
    {
        oentry *sprite = &osprites.jump_table[i];

        sprite->control    = RomLoader_read8_a(roms.rom0p, &a4);
        sprite->draw_props = RomLoader_read8_a(roms.rom0p, &a4);
        sprite->shadow     = RomLoader_read8_a(roms.rom0p, &a4);
        sprite->pal_src    = RomLoader_read8_a(roms.rom0p, &a4);
        sprite->type       = RomLoader_read16_a(roms.rom0p, &a4);
        sprite->addr       = RomLoader_read32(roms.rom0p, outrun.adr.sprite_type_table + sprite->type);
        sprite->xw1        = 
        sprite->xw2        = RomLoader_read16_a(roms.rom0p, &a4);
        sprite->yw         = RomLoader_read16_a(roms.rom0p, &a4);

        { uint16_t z_orig    = RomLoader_read16_a(roms.rom0p, &a4);

        uint32_t z = z_orig;
        outils_swap32u(&z);
        sprite->z = z;

        { int16_t road_x = oroad.road0_h[z_orig];
            int16_t multiply;
        int16_t xw1 = sprite->xw1;

        if (xw1 >= 0 && (sprite->control & WIDE_ROAD) == 0)
        {
            xw1 += (oroad.road_width << 1) << 16;
        }

        /* on_lhs */
        multiply = (xw1 * z_orig) >> 9;
        sprite->x = road_x + multiply;

        a4 += 4; /* Throw away */

        /* Hack to choose correct routine, and not use lookup table from ROM */
        if (i >= 0 && i <= 27)
            sprite->function_holder = 0; /* SpriteCollisionZ1 */
        else if (i >= 28 && i <= 43)
            sprite->function_holder = 7; /* SpriteCollisionZ1C */
        else if (no_entries == HISCORE_SPRITE_ENTRIES)
            sprite->function_holder = 8; /* SpriteNoCollisionZ2; */
        /* Vertical Sign on LHS with Lights */
        else if (i == 44)
            sprite->function_holder = 4; /* Lights */
        /* Vertical Sign on RHS */
        else if (i == 45)
            sprite->function_holder = 0; /* SpriteCollisionZ1 */
        /* Two halves of start sign */
        else if (i == 46 || i == 47)
            sprite->function_holder = 0; /* SpriteNoCollisionZ1 */
        /* Crowd of people */
        else if (i >= 48 && i <= 67)
            sprite->function_holder = 8; /* SpriteNoCollisionZ2; */

        OSprites_map_palette(&osprites, sprite);    
     } }} }
}

/* Setup Sprites */
/* */
/* Source: 3CB2 */
/* */
/* Input: Default Zoom Value */
void OLevelObjs_setup_sprites(OLevelObjs* self, uint32_t z)
{
    /* Setup entries that have not yet been enabled */
    { uint8_t i; for (i = 0; i < osprites.no_sprites; i++)
    {
        if ((osprites.jump_table[i].control & ENABLE) == 0)
        {
            OLevelObjs_setup_sprite(self, &osprites.jump_table[i], z);
            return;
        }
    } }
}

/* Setup Sprite from ROM format for use in game */
/* */
/* Source: 3CDE */
/* */
/* ROM Setup: */
/* */
/* +0: [Byte] Bit 0 = H-Flip Sprite */
/*            Bit 1 = Enable Shadows */
/*             */
/*            Bits 4-7 = Routine Draw Number */
/* +1: [Byte] Sprite X World Position */
/* +2: [Word] Sprite Y World Position */
/* +5: [Byte] Sprite Type */
/* +7: [Byte] Sprite Palette */static 
void OLevelObjs_setup_sprite(OLevelObjs* self, oentry* sprite, uint32_t z)
{
    uint32_t addr;
    #define READ8(x)  TrackLoader_read8(trackloader.scenerymap_data, x)
    #define READ16(x) TrackLoader_read16(trackloader.scenerymap_data, x)
    #define READ32(x) TrackLoader_read32(trackloader.scenerymap_data, x)

    sprite->control |= ENABLE; /* Turn sprite on */
    addr = osprites.seg_spr_addr + osprites.seg_spr_offset1;

    /* Set sprite x,y (world coordinates) */
    sprite->xw1     = 
    sprite->xw2     = READ8(addr + 1) << 4;
    sprite->yw      = READ16(addr + 2) << 7;
    sprite->type    = ((uint8_t) READ8 (addr + 5)) << 2;
    sprite->addr    = RomLoader_read32(roms.rom0p, outrun.adr.sprite_type_table + sprite->type);
    sprite->pal_src = (uint8_t) (READ8 (addr + 7));

    OSprites_map_palette(&osprites, sprite);

    sprite->width = 0;
    sprite->reload = 0;
    sprite->z = z; /* Set default zoom */
    
    if (READ8(addr + 0) & 1)
        sprite->control |= HFLIP;
    else
        sprite->control &=~ HFLIP;

    if (READ8(addr + 0) & 2)
        sprite->control |= SHADOW;
    else
        sprite->control &=~ SHADOW;

    if ((int16_t) (oroad.road_width >> 16) > 0x118)
        sprite->control |= WIDE_ROAD;
    else
        sprite->control &=~ WIDE_ROAD;

    sprite->draw_props = READ8(addr + 0) & 0xF0;
    sprite->function_holder = sprite->draw_props >> 4; /* set sprite type */

    OLevelObjs_setup_sprite_routine(self, sprite);
}static 

void OLevelObjs_setup_sprite_routine(OLevelObjs* self, oentry* sprite)
{
    switch (sprite->function_holder)
    {
        /* Normal Sprite: (Possible With/Without Collision) */
        case 0:
            sprite->shadow = 7;
            sprite->draw_props |= 8; /* x = anchor centre, y = anchor bottom */
            break;
        
        case 1: /* Grass Sprite */
        case 11:  /* Stone Strips */
            sprite->shadow = 7;
            if (sprite->control & HFLIP)
                sprite->draw_props |= 2;
            else
                sprite->draw_props |= 1;
            break;

        /* Overhead Clouds */
        case 2: 
            sprite->shadow = 3;
            if (sprite->control & HFLIP)
                sprite->draw_props |= 0xA;
            else
                sprite->draw_props |= 9;

            if (sprite->draw_props & BIT_0)
                sprite->reload = -0x20;
            else
                sprite->reload = 0x20;

            sprite->xw2 = 0;
            break;

        /* Water Sprite */
        case 3:
            sprite->shadow = 3;
            if (sprite->control & HFLIP)
                sprite->draw_props |= 2; /* anchor x right */
            else
                sprite->draw_props |= 1; /* anchor x left */
            break;
        
        case 4: /* Sprite Lights */
        case 5: /* Checkpoint Bottom */
        case 6: /* Checkpoint Top */
        case 8: /* No Collision Check */
        case 9: /* Wide Rocks (Stage 2) */
            sprite->shadow = 7;
            sprite->draw_props |= 8; /* x = anchor centre, y = anchor bottom */
            break;
        
        /* Draw From Top Left Collision Check */
        case 7:
            sprite->shadow = 7;
            if (sprite->control & HFLIP)
                sprite->draw_props |= 9;
            else
                sprite->draw_props |= 0xA;
            break;

        /* Sand Strips */
        case 10:
        case 14: /* version for wider road widths */
            sprite->shadow = 3;
            if (sprite->control & HFLIP)
                sprite->draw_props |= 2;
            else
                sprite->draw_props |= 1;
            break;
    
        /* Mini Tree */
        case 12:
            sprite->shadow = 7;
            if (sprite->control & HFLIP)
                sprite->draw_props |= 0xA;
            else
                sprite->draw_props |= 9;
            break;

        /* Spray */
        case 13:
            sprite->shadow = 3;
            sprite->draw_props |= 0;
            break;
    }
}

void OLevelObjs_do_sprite_routine(OLevelObjs* self)
{
    { uint8_t i; for (i = 0; i < osprites.no_sprites; i++)
    {
        oentry* sprite = &osprites.jump_table[i];

        if (sprite->control & ENABLE)
        {
            switch (sprite->function_holder)
            {
                /* Normal Sprite: (Possible With/Without Collision, Zoom 1) */
                case 0:
                    if (sprite->yw == 0)
                       OLevelObjs_sprite_normal(self, sprite, 1);
                    else
                       OLevelObjs_set_spr_zoom_priority(self, sprite, 1);
                    break;

                /* Grass Sprite */
                case 1:
                    OLevelObjs_sprite_grass(self, sprite);
                    break;

                /* Sprite based clouds that span entire sky */
                case 2:
                    OLevelObjs_sprite_clouds(self, sprite);
                    break;

                /* Water on LHS of Stage 1 */
                case 3:
                    OLevelObjs_sprite_water(self, sprite);
                    break;

                /* Start Lights & Base Pillar of Checkpoint Sign */
                case 4:
                    OLevelObjs_sprite_lights(self, sprite);
                    break;
                
                /* 5 - Checkpoint (Bottom Of Sign) */
                case 5:
                    OLevelObjs_set_spr_zoom_priority(self, sprite, 1);
                    break;

                /* 6 - Checkpoint (Top Of Sign) */
                case 6:
                    OLevelObjs_set_spr_zoom_priority(self, sprite, 1);
                    /* Have we passed the checkpoint? */
                    if (!(sprite->control & ENABLE))
                        oinitengine.checkpoint_marker = -1;
                    break;

                /* Draw From Centre Collision Check */
                case 7:
                    OLevelObjs_sprite_collision_z1c(self, sprite);
                    break;

                /* Normal Sprite: (Collision, Zoom 2) */
                case 8:
                    OLevelObjs_sprite_normal(self, sprite, 2);
                    break;

                /* Wide Rocks on Stage 2 */
                case 9:
                    OLevelObjs_sprite_rocks(self, sprite);
                    break;

                /* Sand Strips */
                case 10:
                    OLevelObjs_do_thickness_sprite(self, sprite, outrun.adr.sprite_sand);
                    break;

                /* Stone Strips */
                case 11:
                    OLevelObjs_do_thickness_sprite(self, sprite, outrun.adr.sprite_stone);
                    break;

                /* Mini-Tree (Stage 5, Level ID: 0x24) */
                case 12:
                    OLevelObjs_sprite_minitree(self, sprite);
                    break;
                
                /* Track Debris on Stage 3a */
                case 13:
                    OLevelObjs_sprite_debris(self, sprite);
                    break;

                /* Sand (Again) - Used in end sequence #2 */
                case 14:
                    OLevelObjs_do_thickness_sprite(self, sprite, outrun.adr.sprite_sand);
                    break;
            }
        }
    } }
}

/* Source: 4048 */static 
void OLevelObjs_sprite_normal(OLevelObjs* self, oentry *sprite, uint8_t zoom)
{
    int16_t x2;
    int16_t x1;
    uint32_t offset_addr;
    /* Omit collision check if we're already colliding with something */
    if (self->sprite_collision_counter != 0 || (sprite->z >> 16) < 0x1B0)
    {
        OLevelObjs_set_spr_zoom_priority(self, sprite, zoom);
        return;
    }
    /* Check Collision with sprite */

    /* First read collision offsets from table */
    offset_addr = SPRITE_X_OFFS + sprite->type;

    /* H-Flip - swap x co-ordinates */
    if (sprite->control & HFLIP)
    {
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = -x2;
        x1 = -x1;
    }
    else
    {
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
    }

    /* If off left hand side or off right hand side of screen */
    if (sprite->x + x1 > 0 || sprite->x + x2 < 0)
    {
        OLevelObjs_set_spr_zoom_priority(self, sprite, zoom);
        return;
    }

    self->collision_sprite++; /* Denote collision with sprite */
    self->sprite_collision_counter = COLLISION_RESET; /* Reset sprite collision counter */
    OLevelObjs_set_spr_zoom_priority(self, sprite, zoom);
}

/* Identical to sprite_normal, but calls the countdown routine at the end.  */
/* */
/* Source: 0x4658 */static 
void OLevelObjs_sprite_lights(OLevelObjs* self, oentry *sprite)
{
    int16_t x2;
    int16_t x1;
    uint32_t offset_addr;
    /* Omit collision check if we're already colliding with something */
    if (self->sprite_collision_counter != 0 || (sprite->z >> 16) < 0x1B0)
    {
        OLevelObjs_sprite_lights_countdown(self, sprite);
        return;
    }
    /* Check Collision with sprite */

    /* First read collision offsets from table */
    offset_addr = SPRITE_X_OFFS + sprite->type;

    /* H-Flip - swap x co-ordinates */
    if (sprite->control & HFLIP)
    {
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = -x2;
        x1 = -x1;
    }
    else
    {
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
    }

    /* If off left hand side or off right hand side of screen */
    if (sprite->x + x1 > 0 || sprite->x + x2 < 0)
    {
        OLevelObjs_sprite_lights_countdown(self, sprite);
        return;
    }

    self->collision_sprite++; /* Denote collision with sprite */
    self->sprite_collision_counter = COLLISION_RESET; /* Reset sprite collision counter */
    OLevelObjs_sprite_lights_countdown(self, sprite);
}static 

void OLevelObjs_sprite_lights_countdown(OLevelObjs* self, oentry *sprite)
{
    /* Yes, here we use the game_state to control the countdown palette :-s */
    int countdown_pal = outrun.game_state - 9;

    /* We're in countdown mode, let's adjust the palette */
    if (countdown_pal >= 0 && countdown_pal <= 3)
    {
        countdown_pal += 0x7A + ostats.cur_stage;
        sprite->pal_src = (uint8_t) countdown_pal;
        OSprites_map_palette(&osprites, sprite);
    }
    
    OLevelObjs_set_spr_zoom_priority(self, sprite, 1); /* WIDE_ROAD must be set for this to work. */
}

/* Set Sprite Priority To Other Sprites */
/* Set Sprite Priority To Road */
/* Set Index To Lookup Sprite Settings (Width/Height) From Zoom Value */
/* */
/* Source Address: 0x404A */static 
void OLevelObjs_set_spr_zoom_priority(OLevelObjs* self, oentry *sprite, uint8_t zoom)
{
    OSprites_move_sprite(&osprites, sprite, 0);
    { uint16_t z16 = sprite->z >> 16;
        int16_t road_x;
        int32_t road_y;

    if (z16 < 4) return;
    if (z16 >= 0x200)
    {
        OLevelObjs_hide_sprite(self, sprite);
        return;
    }
    sprite->road_priority = z16;
    sprite->priority = z16;        
    sprite->zoom = z16 >> zoom;

    /* Set Sprite Y Position [SCREEN] */
    /* 1/ Use Y Offset From Road Position [Screen] */
    /* 2/ Use Sprite Y World Data if not 0, converted to a screen position [World] */

    road_y = -(oroad.road_y[oroad.road_p0 + z16] >> 4) + 223;

    if (sprite->yw != 0)
    {
        uint32_t yw = sprite->yw * z16; /* Note the final product is a LONG, not a word here */
        outils_swap32u(&yw);
        outils_sub16(yw, &road_y);
    }
    sprite->y = road_y;

    /*    Set Sprite X Position [SCREEN] */
    /*    1/ Use X Offset From Road Position [Screen] */
    /*    2/ Use Sprite X World Data */
    
    road_x = oroad.road0_h[z16];
    { int16_t xw1 = sprite->xw1;
   
    if (xw1 >= 0)
    {
        /* Bit of a hack here to avoid code duplication */
        if (sprite->function_holder >= 4 && sprite->function_holder <= 6) 
            xw1 += (oroad.road_width >> 16) << 1;
        else
        {
            if ((sprite->control & WIDE_ROAD) == 0)
                xw1 += (oroad.road_width >> 16) << 1;
        }
    }

    { int32_t multiply = (xw1 * z16) >> 9; /* only used as a 16 bit value so truncate here */
    sprite->x = road_x + multiply;

    OSprites_do_spr_order_shadows(&osprites, sprite);
 } } }}

/* Seems to be identical to sprite_normal */
/* */
/* Source: 0x4828 */
/*  */
/* - Zoom Shift: 1 */
/* - Test For Collision */static 
void OLevelObjs_sprite_collision_z1c(OLevelObjs* self, oentry* sprite)
{
    int16_t x2;
    int16_t x1;
    uint32_t offset_addr;
    /* Omit collision check if we're already colliding with something */
    if (self->sprite_collision_counter != 0 || (sprite->z >> 16) < 0x1B0)
    {
        OLevelObjs_set_spr_zoom_priority2(self, sprite, 1);
        return;
    }
    /* Check Collision with sprite */

    /* First read collision offsets from table */
    offset_addr = SPRITE_X_OFFS + sprite->type;

    /* H-Flip - swap x co-ordinates */
    if (sprite-> control & HFLIP)
    {
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = -x2;
        x1 = -x1;
    }
    else
    {
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
    }

    { int16_t centre = (x2 - x1) >> 1; /* d0 */
    x1 -= centre;
    x2 -= centre;

    /* If off left hand side or off right hand side of screen */
    if (sprite->x + x1 > 0 || sprite->x + x2 < 0)
    {
        OLevelObjs_set_spr_zoom_priority2(self, sprite, 1);
        return;
    }

    self->collision_sprite++; /* Denote collision with sprite */
    self->sprite_collision_counter = COLLISION_RESET; /* Reset sprite collision counter */
    OLevelObjs_set_spr_zoom_priority2(self, sprite, 1);
 }}

/* Almost identical to set_spr_zoom_priority */
/*  */
/* Differences highlighted below.  */
/*  */
/* Set Sprite Priority To Other Sprites */
/* Set Sprite Priority To Road */
/* Set Index To Lookup Sprite Settings (Width/Height) From Zoom Value */
/* */
/* Source Address: 0x404A */static 
void OLevelObjs_set_spr_zoom_priority2(OLevelObjs* self, oentry *sprite, uint8_t zoom)
{
    OSprites_move_sprite(&osprites, sprite, 0);
    { uint16_t z16 = sprite->z >> 16;
        int16_t road_x;

    if (z16 < 4) return;
    if (z16 >= 0x200)
    {
        OLevelObjs_hide_sprite(self, sprite);
        return;
    }
    sprite->road_priority = z16;
    sprite->priority = z16;    
    sprite->zoom = z16 >> zoom;

    /* Code differs from below */

    /* Set Sprite X Position [SCREEN] */
    /* 1/ Use X Offset From Road Position [Screen] */
    /* 2/ Use Sprite X World Data */

    road_x = oroad.road0_h[z16];
    { int16_t xw1 = sprite->xw1;

    if (xw1 >= 0 && (sprite->control & WIDE_ROAD) == 0)
    {
        xw1 +=  ((int16_t) (oroad.road_width >> 16)) << 1;
    }

    { int32_t multiply = (xw1 * z16) >> 9; /* only used as a 16 bit value so truncate here */
    road_x += multiply;
    if (road_x > (160 + config.s16_x_off) || road_x < -(160 + config.s16_x_off)) return; /* NEW LINE compared with original routine (added for ROM REV. A) */
    sprite->x = road_x;

    /* Set Sprite Y Position [SCREEN] */
    /* 1/ Use Y Offset From Road Position [Screen] */
    /* 2/ Use Sprite Y World Data if not 0, converted to a screen position [World] */
    sprite->y = -(oroad.road_y[oroad.road_p0 + z16] >> 4) + 223;

    OSprites_do_spr_order_shadows(&osprites, sprite);
 } } }}

/* Water, as used on LHS of Stage 1 */
/* */
/* Source: 0x4408 */static 
void OLevelObjs_sprite_water(OLevelObjs* self, oentry* sprite)
{
    /* check_spray */
    if (self->spray_counter == 0)
    {
        if ((sprite->z >> 16) < 0x1B0)
        {

        }
        /* Check whether to initialise spray */
        else if (((sprite->control & HFLIP) == 0               && sprite->x < 0) ||
                 ((sprite->control & HFLIP) == HFLIP && sprite->x > 0))
        {
            self->spray_counter = SPRAY_RESET;
            self->spray_type = 0;
        }
    }
    OLevelObjs_do_thickness_sprite(self, sprite, outrun.adr.sprite_water);
}

/* Grass Sprites */
/* */
/* Source: 0x4416 */static 
void OLevelObjs_sprite_grass(OLevelObjs* self, oentry* sprite)
{
    /* check_spray */
    if (self->spray_counter == 0)
    {
        if ((sprite->z >> 16) < 0x1B0)
        {

        }
        /* Check whether to initialise spray */
        else if (((sprite->control & HFLIP) == 0               && sprite->x < 0) ||
                 ((sprite->control & HFLIP) == HFLIP && sprite->x > 0))
        {
            self->spray_counter = SPRAY_RESET;
            self->spray_type = 4; /* Set Spray Type = Yellow */
            if (sprite->pal_src != 0x49) /* Check whether palette is set to green/grass palette */
                self->spray_type = 8; /* Set Spray Type = Green */
        }
    }
    OLevelObjs_do_thickness_sprite(self, sprite, outrun.adr.sprite_grass);
}

/* Sprite: MiniTrees  */
/* */
/* - Rows of short tree/shrubs found on Stage 5 */
/* - Frame Changes Based On Current Y Position Of Sprite */
/*  */
/* Note: This routine is very similar to do_thickness_sprites and can probably be refactored */
/* */
/* Source: 0x428A */static 

void OLevelObjs_sprite_minitree(OLevelObjs* self, oentry* sprite)
{
    OSprites_move_sprite(&osprites, sprite, 0);
    { uint16_t z16 = sprite->z >> 16;
        int16_t road_x;

    if (z16 < 4) return;
    if (z16 >= 0x200)
    {
        OLevelObjs_hide_sprite(self, sprite);
        return;
    }
     
    sprite->road_priority = z16;
    sprite->priority = z16;    

    /* 44c4 */
    road_x = oroad.road0_h[z16];
    { int16_t xw1 = sprite->xw1;

    if (xw1 >= 0)
    {
        xw1 += ((int16_t) (oroad.road_width >> 16)) << 1;
    }

    { int32_t multiply = (xw1 * z16) >> 9;
        uint16_t z;
        int32_t road_y;
    road_x += multiply;

    if (road_x >= (160 + config.s16_x_off) || road_x < -(160 + config.s16_x_off)) return;
    sprite->x = road_x;

    /* 44fc */
    road_y = -(oroad.road_y[oroad.road_p0 + z16] >> 4) + 223;
    sprite->y = road_y; /* Set Sprite Y (Screen/Camera) */

    z = z16 >> 1;

    if (z >= 0x80) 
    {
        /* don't choose a custom frame */
        sprite->zoom = (uint8_t) z; /* Set Entry Number For Zoom Lookup Table */
        sprite->addr = RomLoader_read32(roms.rom0p, outrun.adr.sprite_minitree); /* Set to first frame in table */
    }
    /* Use Table to alter sprite based on its y position. */
    /* */
    /* Input = Y Position */
    /* */
    /* Format: */
    /* +0: Frame Number To Use */
    /* +2: Entry In Zoom Lookup Table */
    else
    {
        uint8_t offset;
        z <<= 1; /* Note we can't use original z16, so don't try to optimize this */
        offset = RomLoader_read8(&(roms.rom0), MAP_Y_TO_FRAME + z);
        sprite->zoom = RomLoader_read8(&(roms.rom0), MAP_Y_TO_FRAME + z + 1);
        sprite->addr = RomLoader_read32(roms.rom0p, outrun.adr.sprite_minitree + offset);
    }
    /* order_sprites */
    OSprites_do_spr_order_shadows(&osprites, sprite);
 } } }}

/* Source:  */static 
void OLevelObjs_sprite_debris(OLevelObjs* self, oentry* sprite)
{
    uint32_t offset_addr;
    const uint8_t zoom = 2;
    if (self->spray_counter != 0 || (sprite->z >> 16) < 0x1B0)
    {
        OLevelObjs_set_spr_zoom_priority(self, sprite, zoom);
        return;
    }
    /* Check Collision with sprite */

    /* First read collision offsets from table */
    offset_addr = SPRITE_X_OFFS + sprite->type;
    { int16_t x1;
    int16_t x2; 

    /* H-Flip - swap x co-ordinates */
    if (sprite->control & HFLIP)
    {
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = -x2;
        x1 = -x1;
    }
    else
    {
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
    }

    /* If off left hand side or off right hand side of screen */
    if (sprite->x + x1 > 0 || sprite->x + x2 < 0)
    {
        OLevelObjs_set_spr_zoom_priority(self, sprite, zoom);
        return;
    }

    /* Passing through spray! */
    self->spray_counter = SPRAY_RESET;
    self->spray_type = 0xC;
    OLevelObjs_set_spr_zoom_priority(self, sprite, zoom);
 }}

/* Sprite based clouds that span entire sky. */
/* */
/* Appear on Stage 3 - Rightmost Route. */
/* */
/* Source: 0x4144 */static 
void OLevelObjs_sprite_clouds(OLevelObjs* self, oentry* sprite)
{
    OSprites_move_sprite(&osprites, sprite, 1);
    { uint16_t z16 = sprite->z >> 16;
        int16_t road_x;
        uint16_t type;

    if (z16 < 4)
    {
        int32_t road_x = oroad.road0_h[z16];
        sprite->type = road_x;
        return;
    }
    if (z16 >= 0x200)
    {
        OLevelObjs_hide_sprite(self, sprite);
        return;
    }
     
    sprite->road_priority = z16;
    sprite->priority      = z16;
    sprite->y = -((z16 * oroad.horizon_y2) >> 9) + oroad.horizon_y2; /* muls (two 16 bit values, 32 bit result) */

    type = sprite->type;
    road_x = oroad.road0_h[z16];
    sprite->type = road_x;
    road_x -= type;
    type = (z16 >> 2);
    
    if (type != 0)
    {
        /*41b6 */
        road_x += sprite->xw2;
        if (road_x >= 0)
        {
            /* 41be */
            do
            {
                road_x -= type;
            }
            while (road_x >= 0);
            road_x += type;
        }
        else
        {
            /* 41c8 */
            do
            {
                road_x += type;
            }
            while (road_x < 0);
            road_x -= type;
        }
        /* 41ce set_sprite_x_world */
        sprite->xw2 = road_x;
    }

    /* 41d2 - reload is used as an x-offset here, not as a reload value */
    sprite->x = sprite->xw2 + sprite->reload;
    sprite->pal_src = 0xCD;

    { uint16_t z = z16 >> 1;

    if (z >= 0x80)
    {
        /* 421c */
        sprite->zoom = (uint8_t) z; /* Set Entry Number For Zoom Lookup Table */
        sprite->addr = RomLoader_read32(roms.rom0p, outrun.adr.sprite_cloud);
    }
    else
    {
        uint8_t lookup_z;
        /* 41f8 */
        z <<= 1;
        lookup_z = RomLoader_read8(&(roms.rom0), MOVEMENT_LOOKUP_Z + z);
        sprite->addr = RomLoader_read32(roms.rom0p, outrun.adr.sprite_cloud + lookup_z);
        sprite->zoom = RomLoader_read8(&(roms.rom0), MOVEMENT_LOOKUP_Z + z + 1);
    }
    /* end */
    OSprites_map_palette(&osprites, sprite);
    OSprites_do_spr_order_shadows(&osprites, sprite);  
 } }}static 

void OLevelObjs_do_thickness_sprite(OLevelObjs* self, oentry* sprite, const uint32_t sprite_table_address)
{
    OSprites_move_sprite(&osprites, sprite, 0);
    { uint16_t z16 = sprite->z >> 16;
        int16_t road_x;

    if (z16 < 4) return;
    if (z16 >= 0x200)
    {
        OLevelObjs_hide_sprite(self, sprite);
        return;
    }
     
    sprite->road_priority = z16;
    sprite->priority = z16;    

    /* 44c4 */
    road_x = oroad.road0_h[z16];
    { int16_t xw1 = sprite->xw1;

    if (xw1 >= 0)
    {
        if (sprite->id != 14 || (sprite->control & WIDE_ROAD) == 0) /* Rolled in separate routine with this one line. (Sand 2 used in bonus sequence) */
            xw1 += ((int16_t) (oroad.road_width >> 16)) << 1;
    }

    { int32_t multiply = (xw1 * z16) >> 9;
        uint16_t z;
        int32_t road_y;
    road_x += multiply;

    if (road_x >= (160 + config.s16_x_off) || road_x < -(160 + config.s16_x_off)) return;
    sprite->x = road_x;

    /* 44fc */
    road_y = -(oroad.road_y[oroad.road_p0 + z16] >> 4) + 223;
    sprite->y = road_y; /* Set Sprite Y (Screen/Camera) */
    /* 4518 - Sprite thickness code */

    z = z16 >> 1;

    if (z >= 0x80) 
    {
        /*use_large_frame (don't choose a custom frame) */
        sprite->zoom = (uint8_t) z; /* Set Entry Number For Zoom Lookup Table */
        sprite->addr = RomLoader_read32(roms.rom0p, 0x3C + sprite_table_address); /* Set default frame for larger sprite */
    }
    else
    {
        /* use custom frame for sprite */
        sprite->zoom = 0x80; /* cap sprite_z minimum to 0x80 */
        z = (z >> 1) & 0x3C; /* Mask over lower 2 bits, so the frame aligns to a word */
        sprite->addr = RomLoader_read32(roms.rom0p, z + sprite_table_address); /* Set Frame Data Based On Zoom Value */
    }
    /* order_sprites */
    OSprites_do_spr_order_shadows(&osprites, sprite);
 } } }}

/* --------------- */
/* SpriteWideRocks */
/* --------------- */
/* */
/* Note doesn't do same adjustment of y position using z  */
/* Used for rocks on Stage 2 (rightmost route) */
/* */
/* Similar routine to SpriteCollisionZ1 (0x4048) but with additional width checks */
/*  */
/* TODO: Merge with sprite_normal routine and then pass in the set_spr_zoom_priority_rocks routine as an argument */
/* */
/* Source: 0x492A */static 

void OLevelObjs_sprite_rocks(OLevelObjs* self, oentry *sprite)
{
    int16_t x2;
    int16_t x1;
    uint32_t offset_addr;
    uint8_t zoom = 1;
    /* Omit collision check if we're already colliding with something */
    if (self->sprite_collision_counter != 0 || (sprite->z >> 16) < 0x1B0)
    {
        OLevelObjs_set_spr_zoom_priority_rocks(self, sprite, zoom);
        return;
    }
    /* Check Collision with sprite */

    /* First read collision offsets from table */
    offset_addr = SPRITE_X_OFFS + sprite->type;

    /* H-Flip - swap x co-ordinates */
    if (sprite->control & HFLIP)
    {
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = -x2;
        x1 = -x1;
    }
    else
    {
        x1 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
        x2 = (int16_t) RomLoader_read16_a(&(roms.rom0), &offset_addr);
    }

    /* If off left hand side or off right hand side of screen */
    if (sprite->x + x1 > 0 || sprite->x + x2 < 0)
    {
        OLevelObjs_set_spr_zoom_priority_rocks(self, sprite, zoom);
        return;
    }

    self->collision_sprite++; /* Denote collision with sprite */
    self->sprite_collision_counter = COLLISION_RESET; /* Reset sprite collision counter */
    OLevelObjs_set_spr_zoom_priority_rocks(self, sprite, zoom);
}

/* Set Sprite Priority To Other Sprites */
/* Set Sprite Priority To Road */
/* Set Index To Lookup Sprite Settings (Width/Height) From Zoom Value */
/* */
/* Source Address: 0x404A */static 
void OLevelObjs_set_spr_zoom_priority_rocks(OLevelObjs* self, oentry *sprite, uint8_t zoom)
{
    OSprites_move_sprite(&osprites, sprite, 0);
    { uint16_t z16 = sprite->z >> 16;
        int16_t road_x;
        int32_t road_y;

    if (z16 < 4) return;
    if (z16 >= 0x200)
    {
        OLevelObjs_hide_sprite(self, sprite);
        return;
    }
    sprite->road_priority = z16;
    sprite->priority = z16;        
    sprite->zoom = z16 >> zoom;

    /* Set Sprite Y Position [SCREEN] */
    /* 1/ Use Y Offset From Road Position [Screen] */
    /* 2/ Use Sprite Y World Data if not 0, converted to a screen position [World] */

    road_y = -(oroad.road_y[oroad.road_p0 + z16] >> 4) + 223;
    sprite->y = road_y;

    /*    Set Sprite X Position [SCREEN] */
    /*    1/ Use X Offset From Road Position [Screen] */
    /*    2/ Use Sprite X World Data */
    
    road_x = oroad.road0_h[z16];
    { int16_t xw1 = sprite->xw1;
   
    if (xw1 >= 0 && (sprite->control & WIDE_ROAD) == 0)
    {
        xw1 +=  ((int16_t) (oroad.road_width >> 16)) << 1;
    }

    { int32_t multiply = (xw1 * z16) >> 9; /* only used as a 16 bit value so truncate here */
        uint16_t width;
    sprite->x = road_x + multiply;

    /* Additional Draw Check Here, to prevent sprite from wrapping around screen and redrawing on left */
    /* Remember this routine is setup to draw from centre x */

    width = (sprite->width >> 1) + 160 + config.s16_x_off;
    if (sprite->x >= width || sprite->x + width < 0) return;

    OSprites_do_spr_order_shadows(&osprites, sprite);
 } } }}

/* Source Address: 0x4648 */
void OLevelObjs_hide_sprite(OLevelObjs* self, oentry *sprite)
{
    sprite->z = 0;
    sprite->zoom = 0; /* Hide the sprite */
    sprite->control &= ~ENABLE; /* Disable entry in jump table */
}
