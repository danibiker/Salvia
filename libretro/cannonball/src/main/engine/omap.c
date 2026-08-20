/***************************************************************************
    Course Map Logic & Rendering. 
    
    This is the full-screen map that is displayed at the end of the game. 
    
    The logo is built from multiple sprite components.
    
    The course map itself is made up of sprites and pieced together. 
    It's not a tilemap.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "engine/oferrari.h"
#include "engine/omap.h"
#include "engine/otiles.h"
#include "engine/otraffic.h"
#include "engine/ostats.h"

static void OMap_draw_horiz_end(OMap* self, oentry*);
static void OMap_draw_vert_bottom(OMap* self, oentry*);
static void OMap_draw_vert_top(OMap* self, oentry*);
static void OMap_draw_piece(OMap* self, oentry*, uint32_t);
static void OMap_do_route_final(OMap* self);
static void OMap_end_route(OMap* self);
static void OMap_init_map_delay(OMap* self);
static void OMap_map_display(OMap* self);
static void OMap_move_mini_car(OMap* self, oentry*);

OMap omap;

/* Position of Ferrari in Jump Table */
#define MAP_FERRARI_SLOT (25)




void OMap_init(OMap* self)
{
    oferrari.car_ctrl_active = false; /* -1 */
    Video_clear_text_ram(&video);
    OSprites_disable_sprites(&osprites);
    OTraffic_disable_traffic(&otraffic);
    OSprites_clear_palette_data(&osprites);
    oinitengine.car_increment = 0;
    oferrari.car_inc_old      = 0;
    osprites.spr_cnt_main     = 0;
    osprites.spr_cnt_shadow   = 0;
    oroad.road_ctrl           = ROAD_BOTH_P0;
    oroad.horizon_base        = HORIZON_OFF;
    OTiles_fill_tilemap_color(&otiles, 0xABD); /*  Paint pinkish colour on tilemap 16 */
    self->init_sprites = true;
}

/* Process route through levels */
/* Process end position on final level */
/* Source: 0x345E */
void OMap_tick(OMap* self)
{
    /* 60 FPS Code to simply render sprites */
    if (!outrun.tick_frame)
    {
        OMap_blit(self);
        return;
    }

    /* Initialize Course Map Sprites if necessary */
    if (self->init_sprites)
    {
        OMap_load_sprites(self);
        self->init_sprites = false;
        return;
    }

    switch (self->map_state)
    {
        /* Initialise Route Info */
        case MAP_INIT:
            hwsprites_set_x_clip(video.sprite_layer, false); /* Don't clip the area in wide-screen mode */
            self->map_route  = RomLoader_read8(&(roms.rom0), MAP_ROUTE_LOOKUP + ostats.routes[1]);
            self->map_pos    = 0;
            self->map_stage1 = 0;
            self->map_stage2 = ostats.cur_stage;
            if (self->map_stage2 > 0)
                self->map_state = MAP_ROUTE;
            else
            {
                self->map_state = MAP_ROUTE_FINAL;
                OMap_do_route_final(self);
                break;
            }

        /* Do Route [Note map is displayed from this point on] */
        case MAP_ROUTE:
            if (++self->map_pos > 0x1B)
            {
                if (--self->map_stage2 <= 0)
                {   /*map_end_route */
                    self->map_pos = 0;
                    self->map_stage1++;
                    { uint16_t route_info = ostats.routes[1 + self->map_stage1];
                    if (route_info)
                    {
                        self->map_route = RomLoader_read8(&(roms.rom0), MAP_ROUTE_LOOKUP + route_info);                      
                    }
                    else
                    {
                        self->map_route = RomLoader_read8(&(roms.rom0), MAP_ROUTE_LOOKUP + ostats.routes[0 + self->map_stage1] + 0x10);
                    }

                    self->map_state = MAP_ROUTE_FINAL;
                    OMap_do_route_final(self);
                 }}
                else
                {
                    self->map_pos = 0;
                    self->map_stage1++;
                    self->map_route = RomLoader_read8(&(roms.rom0), MAP_ROUTE_LOOKUP + ostats.routes[1 + self->map_stage1]);
                }
            }
            break;
        
        /* Do Final Segment Of Route [Car still moving] */
        case MAP_ROUTE_FINAL:
            OMap_do_route_final(self);
            break;

        /* Route Concluded        */
        case MAP_ROUTE_DONE:
            OMap_end_route(self);
            break;
        
        /* Init Delay Counter For Map Display */
        case MAP_INIT_DELAY:
            OMap_init_map_delay(self);
            break;

        /* Display Map */
        case MAP_DISPLAY:
            OMap_map_display(self);
            break;

        /* Clear Course Map         */
        case MAP_CLEAR:
            Outrun_init_best_outrunners(&outrun);
            return;
    }

    OMap_draw_course_map(self);
}

/* Render sprites only. No Logic */
void OMap_blit(OMap* self)
{
    { uint8_t i; for (i = 0; i <= MAP_PIECES; i++)
    {
        oentry* sprite = &osprites.jump_table[i];
        if (sprite->control & ENABLE)
            OSprites_do_spr_order_shadows(&osprites, sprite);
    } }
}

void OMap_draw_course_map(OMap* self)
{
    oentry* sprite = osprites.jump_table;

    /* Draw Road Components */
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_vert_bottom(self, sprite++);
    OMap_draw_vert_top   (self, sprite++);
    OMap_draw_horiz_end  (self, sprite++);
    OMap_draw_horiz_end  (self, sprite++);
    OMap_draw_horiz_end  (self, sprite++);
    OMap_draw_horiz_end  (self, sprite++);
    OMap_draw_horiz_end  (self, sprite++);

    /* Draw Mini Car */
    OMap_move_mini_car   (self, sprite++);

    /* Draw Backdrop Map Pieces */
    { uint8_t i; for (i = 26; i <= MAP_PIECES; i++)
    {
        if (sprite->control & ENABLE)
            OSprites_do_spr_order_shadows(&osprites, sprite++);
    } }
}


void OMap_position_ferrari(OMap* self, uint8_t index)
{
    oentry* segment = &osprites.jump_table[index];
    osprites.jump_table[MAP_FERRARI_SLOT].x = segment->x - 8;
    osprites.jump_table[MAP_FERRARI_SLOT].y = segment->y;
}

/* Initalize Course Map Sprites */
/*  */
/* Notes: Index 26 is start of water that needs to be changed for widescreen */
/*  */
/* Source: 0x33F4 */
void OMap_load_sprites(OMap* self)
{
    /* hacks */
    /*ostats.cur_stage = 4;
    ostats.routes[0] = 4;
    ostats.routes[1] = 0x08;
    ostats.routes[2] = 0x18;
    ostats.routes[3] = 0x28;
    ostats.routes[4] = 0x38;
    oinitengine.rd_split_state = 0x16;
    oroad.road_pos = 0x192 << 16;*/
    /* end hacks */

    uint32_t adr = outrun.adr.sprite_coursemap;

    { uint8_t i; for (i = 0; i <= MAP_PIECES; i++)
    {
        oentry* sprite     = &osprites.jump_table[i];
        sprite->id         = i+1;
        sprite->control    = RomLoader_read8_a(roms.rom0p, &adr);
        sprite->draw_props = RomLoader_read8_a(roms.rom0p, &adr);
        sprite->shadow     = RomLoader_read8_a(roms.rom0p, &adr);
        sprite->zoom       = RomLoader_read8_a(roms.rom0p, &adr);
        sprite->pal_src    = (uint8_t) RomLoader_read16_a(roms.rom0p, &adr);
        sprite->priority   = sprite->road_priority = RomLoader_read16_a(roms.rom0p, &adr);
        sprite->x          = RomLoader_read16_a(roms.rom0p, &adr);
        sprite->y          = RomLoader_read16_a(roms.rom0p, &adr);
        sprite->addr       = RomLoader_read32_a(roms.rom0p, &adr);
        sprite->counter    = 0;  
        
        adr += 4; /* throw this address away */

        OSprites_map_palette(&osprites, sprite);
    } }

    /* Wide-screen hack to extend sea to edge of screen. */
    if (config.s16_x_off != 0 || config.engine.fix_bugs)
    {
        { uint8_t i; for (i = 26; i <= 30; i++)
        {
            oentry* sprite = &osprites.jump_table[i];
            sprite->addr   = osprites.jump_table[31].addr;
            sprite->x      -= 64;
            sprite->zoom   = 0x7F;
        } }
    }

    /* Minicar initalization moved here */
    self->minicar_enable = 0;
    osprites.jump_table[MAP_FERRARI_SLOT].x = -0x80;
    osprites.jump_table[MAP_FERRARI_SLOT].y = 0x78;
    self->map_state = MAP_INIT;
}

/* Source: 0x355A */static 
void OMap_do_route_final(OMap* self)
{
    int16_t pos = oroad.road_pos >> 16;
    if (oinitengine.rd_split_state)
        pos += 0x79C;   

    pos = (pos * 0x1B) / 0x94D;
    self->map_pos_final = pos;

    self->map_state = MAP_ROUTE_DONE;
    OMap_end_route(self);
}

/* Source: 0x3584 */static 
void OMap_end_route(OMap* self)
{
    self->map_pos++;

    if (self->map_pos_final < self->map_pos)
    {
        /* 359C */
        self->map_pos = self->map_pos_final;
        self->minicar_enable = 1;
        self->map_state = MAP_INIT_DELAY;
        OMap_init_map_delay(self);
    }
}

/* Source: 0x35B6 */static 
void OMap_init_map_delay(OMap* self)
{
    self->map_route = 0;
    self->map_delay = 0x80;
    self->map_state = MAP_DISPLAY;
    OMap_map_display(self);
}

/* Source: 0x35CC */static 
void OMap_map_display(OMap* self)
{
    /* Init Best OutRunners */
    if (--self->map_delay <= 0)
    {
        self->map_state = MAP_CLEAR;
        Outrun_init_best_outrunners(&outrun);
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Colour sprite based road as car moves over it on mini-map */
/* ------------------------------------------------------------------------------------------------ */

/* Source: 0x3740 */static 
void OMap_draw_vert_top(OMap* self, oentry* sprite)
{
    if (sprite->control & ENABLE)
        OMap_draw_piece(self, sprite, outrun.adr.sprite_coursemap_top);
}

/* Source: 0x3736 */static 
void OMap_draw_vert_bottom(OMap* self, oentry* sprite)
{
    if (sprite->control & ENABLE)
        OMap_draw_piece(self, sprite, outrun.adr.sprite_coursemap_bot);
}

/* Source: 0x372C */static 
void OMap_draw_horiz_end(OMap* self, oentry* sprite)
{
    if (sprite->control & ENABLE)
        OMap_draw_piece(self, sprite, outrun.adr.sprite_coursemap_end);
}

/* Source: 0x3746 */static 
void OMap_draw_piece(OMap* self, oentry* sprite, uint32_t adr)
{
    /* Update palette of background piece, to highlight route as minicar passes over it */
    if (self->map_route == sprite->id)
    {
        sprite->priority = 0x102;
        sprite->road_priority = 0x102;

        adr += (self->map_pos << 3);

        sprite->addr    = RomLoader_read32(roms.rom0p, adr);
        sprite->pal_src = RomLoader_read8(roms.rom0p, 4 + adr);
        OSprites_map_palette(&osprites, sprite);
    }

    OSprites_do_spr_order_shadows(&osprites, sprite);
}

/* Move mini car sprite on Course Map Screen */
/* Source: 0x3696 */static 
void OMap_move_mini_car(OMap* self, oentry* sprite)
{
    /* Move Mini Car */
    if (!self->minicar_enable)
    {
        /* Remember that the minimap is angled, so we still need to adjust both the x and y positions */
        uint32_t movement_table = (self->map_route & 1) ? MAP_MOVEMENT_RIGHT : MAP_MOVEMENT_LEFT;
        
        { int16_t pos = (self->map_stage1 < 4) ? self->map_pos : self->map_pos >> 1;
        pos <<= 1; /* do not try to merge with previous line */

        sprite->x += RomLoader_read16(&(roms.rom0), movement_table + pos);
        { int16_t y_change = RomLoader_read16(&(roms.rom0), movement_table + pos + 0x40);
        sprite->y -= y_change;

        if (y_change == 0)
            sprite->addr = outrun.adr.sprite_minicar_right;
        else if (y_change < 0)
            sprite->addr = outrun.adr.sprite_minicar_down;
        else
            sprite->addr = outrun.adr.sprite_minicar_up;
     } }}

    OSprites_do_spr_order_shadows(&osprites, sprite);
}