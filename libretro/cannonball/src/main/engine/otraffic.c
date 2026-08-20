/***************************************************************************
    Traffic Routines.

    - Traffic spawning.
    - Traffic logic, lane changing & movement.
    - Collisions.
    - Traffic panning and volume control to pass to sound program.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "engine/obonus.h"
#include "engine/ocrash.h"
#include "engine/oferrari.h"
#include "engine/ohud.h"
#include "engine/olevelobjs.h"
#include "engine/outils.h"
#include "engine/ostats.h"
#include "engine/otraffic.h"

static void OTraffic_spawn_car(OTraffic* self, oentry* sprite);
static void OTraffic_spawn_traffic(OTraffic* self);
static void OTraffic_tick_spawned_sprite(OTraffic* self, oentry* sprite);
static void OTraffic_move_spawned_sprite(OTraffic* self, oentry* sprite);
static void OTraffic_update_props(OTraffic* self, oentry* sprite);
static void OTraffic_set_zoom_lookup(OTraffic* self, oentry* sprite);
static void OTraffic_calculate_avg_speed(OTraffic* self, uint16_t);
static void OTraffic_check_collision(OTraffic* self, oentry* sprite);

OTraffic otraffic;




void OTraffic_init(OTraffic* self)
{
    self->ai_traffic        = 0;
    self->bonus_lhs         = 0;
    self->traffic_split     = 0;
    self->collision_traffic = 0;
    self->collision_mask    = 0;

    self->traffic_speed_total = 0;
    self->traffic_speed_avg   = 0;
    self->traffic_pal_cycle   = 0;
    self->traffic_count       = 0;
    self->spawn_counter       = 0;
    self->spawn_location      = 0;
    /* Set wheel animation reset value across all traffic (moved from spawn traffic routine) */
    self->wheel_counter = self->wheel_reset = 12;
}

/* Initalize traffic in right land lane for Stage 1 */
void OTraffic_init_stage1_traffic(OTraffic* self)
{
    const uint8_t flags = TRAFFIC_SPRITE | TRAFFIC_RHS | ENABLE;

    oentry* t = &osprites.jump_table[SPRITE_TRAFF1];
    t->function_holder = TRAFFIC_INIT;
    t->control        |= flags;
    t->draw_props     |= ANCHOR_BOTTOM;
    t->z               = 0x140F520;

    t = &osprites.jump_table[SPRITE_TRAFF2];
    t->function_holder = TRAFFIC_INIT;
    t->control        |= flags;
    t->draw_props     |= ANCHOR_BOTTOM;
    t->xw1             = 0x70;
    t->z               = 0x14004E0;
    t->type            = 0x18;
    t->xw2             = 0x70;

    t = &osprites.jump_table[SPRITE_TRAFF3];
    t->function_holder = TRAFFIC_INIT;
    t->control        |= flags;
    t->draw_props     |= ANCHOR_BOTTOM;
    t->xw1             = -0x70;
    t->z               = 0x14004E0;
    t->type            = 0x20; 
    t->xw2             = -0x70;

    t = &osprites.jump_table[SPRITE_TRAFF4];
    t->function_holder = TRAFFIC_INIT;
    t->control        |= flags;
    t->draw_props     |= ANCHOR_BOTTOM;
    t->xw1             = 0x70;
    t->z               = 0x1D004E0;
    t->type            = 0x28; 
    t->xw2             = 0x70;

    t = &osprites.jump_table[SPRITE_TRAFF5];
    t->function_holder = TRAFFIC_INIT;
    t->control        |= flags;
    t->draw_props     |= ANCHOR_BOTTOM;
    t->xw1             = -0x70;
    t->z               = 0x1D004E0;
    t->type            = 0x30; 
    t->xw2             = -0x70;
}

/* Tick Spawned Traffic Objects */
/* */
/* Source: 0x521A */
void OTraffic_tick(OTraffic* self)
{
    /* Lock traffic spawning to 30fps frame rate. */
    if (outrun.tick_frame) 
        OTraffic_spawn_traffic(self);

    { uint8_t i; for (i = SPRITE_TRAFF1; i <= SPRITE_TRAFF8; i++)
    {
        oentry* sprite = &osprites.jump_table[i];

        if (sprite->function_holder == TRAFFIC_INIT)
        {
            if (outrun.game_state != GS_INGAME && outrun.game_state != GS_ATTRACT)
            {
                sprite->traffic_proximity = 0;
                OTraffic_move_spawned_sprite(self, sprite); /* Skip collision code */
                continue;
            }
            sprite->traffic_orig_speed = 0xD4;
            sprite->function_holder = TRAFFIC_ENTRY;
        }

        /* Skip collision code in first section of level */
        if (sprite->function_holder == TRAFFIC_ENTRY)
        {
            if (oroad.road_pos >> 16 >= 0x80)
                sprite->function_holder = TRAFFIC_TICK;
            else
                OTraffic_move_spawned_sprite(self, sprite); /* Skip collision code */
        }

        if (sprite->function_holder == TRAFFIC_TICK)
            OTraffic_tick_spawned_sprite(self, sprite);
    } }
}

/* Disable Traffic Routines */
/* Source: 0x4A78 */
void OTraffic_disable_traffic(OTraffic* self)
{
    { uint8_t i; for (i = SPRITE_TRAFF1; i <= SPRITE_TRAFF8; i++)
        osprites.jump_table[i].control &= ~ENABLE; }
}

/* Master Function to determine when to spawn traffic */
/* */
/* 1. Toggle animation frame to control wheels of traffic */
/* 2. Spawn traffic when appropriate */
/* */
/* Source: 0x4AC8 */static 
void OTraffic_spawn_traffic(OTraffic* self)
{
    if (obonus.bonus_control || 
        outrun.game_state == GS_MAP || outrun.game_state == GS_MUSIC || outrun.game_state == GS_BEST2) 
        return;
    
    self->spawn_counter++;
    self->ai_traffic = 0; /* Clear AI Traffic Marker */
    
    /* Use average speed of traffic as new counter reset value to control speed of wheel animations */
    if (self->traffic_speed_avg)
    {
        self->wheel_reset = -((self->traffic_speed_avg >> 5) - 11);
        
        if (--self->wheel_counter == 0)
        {
            self->wheel_counter = self->wheel_reset;
            self->traffic_pal_cycle = 0;
        }
        else if ((self->wheel_reset >> 1) == self->wheel_counter)
        {
            self->traffic_pal_cycle = 1;
        }
    }
    /* check_traffic_count */
    if (self->traffic_count >= self->max_traffic)
        return;

    /* Use counter as a spawning delay */
    if (! (((self->spawn_counter - 1) ^ self->spawn_counter) & BIT_5) )
        return;

    /* Spawn Traffic if possible in one of the eight slots */
    { uint8_t i; for (i = SPRITE_TRAFF1; i <= SPRITE_TRAFF8; i++)
    {
        oentry* sprite = &osprites.jump_table[i];
        
        if (!(sprite->control & ENABLE))
        {
            OTraffic_spawn_car(self, sprite);
            return;
        }
    } }
}

/* Spawn individual vehicle. Called by master function. */
/* Cars are spawned on the horizon */
/* */
/* Source: 0x4BAC */static 
void OTraffic_spawn_car(OTraffic* self, oentry* sprite)
{
    static const int8_t TYPE[] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x01, 0x02, 0x03, 0x06, 0x07, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0A, 0x0B, 0x08, 0x09, 0x0A, 0x0B, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0C, 0x0D, 0x0C, 0x0D,
        0x0C, 0x0D, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0F, 0x0E, 0x0F, 0x0E, 0x0F, 0x0E, 0x0F, 0x0E, 0x0F,
        0x11, 0x11, 0x11, 0x10, 0x10, 0x10, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x0F, 0x0F, 0x0F, 0x0F
    };
    uint8_t spawn_index;
    int16_t rnd;
    sprite->control |= ENABLE | TRAFFIC_SPRITE;
    sprite->draw_props = ANCHOR_BOTTOM;
    sprite->shadow = 7;     /* Used as priority */
    sprite->width = 0;
    sprite->traffic_proximity = 0;
    sprite->traffic_fx = 0;
    sprite->z = 0x10000;    /* Traffic starts on horizon in the distance */
    rnd = outils_random();
    self->spawn_location++;
    
    /* Spawn On Left Hand Side Of Road */
    if (self->spawn_location & 1)
    {
        const int8_t TABLE[] = {0, -0x70, -0x70, 0x70};
        sprite->control &= ~TRAFFIC_RHS;
        /* note we use (rnd & 6) >> 1 rather than (rnd & 3) to match original random number generation */
        sprite->xw1 = sprite->xw2 = TABLE[(rnd & 6) >> 1];  
        sprite->control |= HFLIP;   
    }
    /* Spawn On Right Hand Side Of Road */
    else
    {
        const int8_t TABLE[] = {0, -0x70, 0x70, 0x70};
        sprite->control |= TRAFFIC_RHS;
        sprite->xw1 = sprite->xw2 = TABLE[(rnd & 6) >> 1];
        sprite->control &= ~HFLIP;
    }
    
    rnd = (int8_t) rnd; /* ext.w */

    sprite->traffic_orig_speed = (rnd >> 2) + 200;

    /* hack//////////////////////////////////////////////////////////////////////////// */
    /*sprite->traffic_orig_speed = 1; */
    /*traffic_speed_avg = 0; */
    /* hack//////////////////////////////////////////////////////////////////////////// */

    sprite->traffic_speed = self->traffic_speed_avg;

    /* Randomize Type of traffic to spawn */
    spawn_index = (rnd >> 2) + 0x20;


    sprite->type = TYPE[spawn_index] << 3;
    sprite->function_holder = TRAFFIC_TICK;
}

/* Check Traffic Collision */
/* */
/* Source: 0x4DAA */static 
void OTraffic_tick_spawned_sprite(OTraffic* self, oentry* sprite)
{
    if (outrun.tick_frame)
    {
        /* Force side of road when in bonus mode, or road splitting */
        if (self->bonus_lhs)
            sprite->control |= TRAFFIC_RHS;
        else if (self->traffic_split)
            sprite->control ^= TRAFFIC_RHS;
    
        /* Check for collision with player's car */
        OTraffic_check_collision(self, sprite);

        /* Denote collisions for new attract mode */
        if (config.engine.new_attract)
        {
            if (sprite->z >> 16 >= 0x90)
            {
                const int PAD = 48;
                int16_t w  = (sprite->width >> 1) + (sprite->width >> 3) + (sprite->width >> 4) + PAD;
                int16_t x1 = sprite->x - w; /* d2 */
                int16_t x2 = sprite->x + w; /* d1 */

                /* Check traffic is directly in front of player's car */
                if (x1 < 0 && x2 > 0)
                {
                    otraffic.ai_traffic = 1;
                }
            }
        }

        /* Calculate X Difference Between Player Car & Traffic. */
        /* Set Relevant Bits To Denote which side player's car is on in relation to traffic */

        if (sprite->z >> 16 <= 0x100) /* Value was 0xA0 on original romset and changed for Rev. A */
        {
            OTraffic_move_spawned_sprite(self, sprite);
            return;
        }

        { int16_t x_diff = sprite->xw1 + oinitengine.car_x_pos - (oroad.road_width >> 16); /* d1 */
        int16_t x_diff_abs = x_diff < 0 ? -x_diff : x_diff; /* d0 */

        if (x_diff_abs >= 0xA0)
        {
            OTraffic_move_spawned_sprite(self, sprite);
            return;
        }
        if (x_diff >= 0)
            sprite->traffic_proximity |= BIT_1;
        else
            sprite->traffic_proximity |= BIT_0;

        /* Code in block below was added in Rev A. Romset */
        if (sprite->xw1 == 0x70)
            sprite->traffic_proximity |= BIT_0;
        else if (sprite->xw1 == -0x70)
            sprite->traffic_proximity |= BIT_1;
        /* End Added block */
    
        if (!config.engine.new_attract)
            self->ai_traffic |= sprite->traffic_proximity;
     }}

    OTraffic_move_spawned_sprite(self, sprite);
}

/* 0x4E3E */static 
void OTraffic_move_spawned_sprite(OTraffic* self, oentry* sprite)
{
    /* Road Splitting: Return if enemy on opposite side of road to split */
    if (oinitengine.road_remove_split)
    {
        if (((oinitengine.route_selected ^ sprite->control) & TRAFFIC_RHS) == 0) 
            return;
    }

    if (outrun.game_state != GS_INGAME && outrun.game_state != GS_BONUS && outrun.game_state != GS_ATTRACT)
    {
        OSprites_do_spr_order_shadows(&osprites, sprite);
        return;
    }

    if (outrun.tick_frame)
    {
        int16_t x_diff;
        /* Check closeness bits and setup approproiate lane movement plan for traffic. */
        uint8_t traffic_proximity = sprite->traffic_proximity & 3;

        /* Other Traffic Close */
        if (traffic_proximity)
        {
            /* Value transformed as follows: */
            /* 3 = 0, 2 = 1, 1 = 2 */
            traffic_proximity ^= 3;
        
            /* use_traffic_speed: */
            /* Sprite hemmed in on left + right. Resort to average traffic speed. */
            if (!traffic_proximity)
            {
                sprite->traffic_speed = sprite->traffic_near_speed < 0x70 ? 0x70 : sprite->traffic_near_speed;
                OTraffic_update_props(self, sprite);
                return;
            }
            /* try_move_right */
            else if (traffic_proximity & BIT_0)
            {
                if (sprite->xw2 <= 0)
                    sprite->xw2 += 0x70;
            }
            /* Try Moving Sprite Left */
            else
            {
                if (sprite->xw2 >= 0)
                    sprite->xw2 -= 0x70;
            }
        }
        /* not_close: */
        /* Gradually restore traffic back to original speed. (Routine from 0x50BC rolled in) */
        else
        {
            int16_t speed = sprite->traffic_orig_speed - sprite->traffic_speed;
            if (speed > 2) speed = 2;
            else if (speed < -2) speed = -2;
            sprite->traffic_speed += speed;
        }

        /* try_lane_change: */
        x_diff = sprite->xw2 - sprite->xw1;

        if (x_diff)
        {
            if (x_diff > 0)
            {
                if ((sprite->traffic_proximity & BIT_0) == 0) /* Move Left if no traffic on LHS */
                    sprite->xw1 += 4;
            }
            else if (x_diff < 0)
            {
                if ((sprite->traffic_proximity & BIT_1) == 0) /* Move Right if no traffic on RHS */
                    sprite->xw1 -= 4;
            }
        }
    }
    /* skip_lane_change: */
    OTraffic_update_props(self, sprite);
}

/* skip_lane_change: */
/* Source: 0x4F0C */static 
void OTraffic_update_props(OTraffic* self, oentry* sprite)
{
    int32_t z_adjust = (((oinitengine.car_increment >> 16) - sprite->traffic_speed) * (sprite->z >> 16)) << 5;

    if (config.tick_fps == 60)
        z_adjust >>= 1;
    else if (config.tick_fps == 120)
        z_adjust >>= 2;

    sprite->z += z_adjust;
    
    { int16_t z16 = sprite->z >> 16;
        int16_t* road_x;

    /* Disable Traffic Object */
    if (z16 <= 0)
    {
        OLevelObjs_hide_sprite(&olevelobjs, sprite);
        return;
    }
    /* Overtake Traffic Object */
    if (z16 >= 0x200)
    {
        OSoundInt_queue_sound(&osoundint, SOUND_RESET);
        if (outrun.game_state == GS_INGAME)
        {
            /* Update score on overtake */
            if (outrun.cannonball_mode != MODE_TTRIAL)
                OStats_update_score(&ostats, 0x20000);
            else
            {
                OHud_draw_score(&ohud, OHud_translate(&ohud, 3, 2, 0x110030), outils_convert16_dechex(++outrun.ttrial.overtakes), 2);
                OHud_blit_text1_xy(&ohud, 2, 1, HUD_SCORE1);
                OHud_blit_text1_xy(&ohud, 2, 2, HUD_SCORE2);
            }
        }

        OLevelObjs_hide_sprite(&olevelobjs, sprite);
        return;
    }

    sprite->priority = sprite->road_priority = z16;

    /* Set Screen Y */
    sprite->y = -(oroad.road_y[oroad.road_p0 + z16] >> 4) + 223;
    OTraffic_set_zoom_lookup(self, sprite);

    /* Set Screen X */
    road_x = (sprite->control & TRAFFIC_RHS) ? oroad.road1_h : oroad.road0_h;
    { int32_t x = (sprite->xw1 * z16) >> 9;
        int8_t incline;
        int16_t y;
    sprite->x = x + road_x[z16];

    if (z16 <= 8)
    {
        OSprites_map_palette(&osprites, sprite);
        self->traffic_speed_total += sprite->traffic_speed;
        OSprites_do_spr_order_shadows(&osprites, sprite);
        return;
    }

    /* Calculate change in road y, so we can determine incline frame for traffic */
    y = oroad.road_y[oroad.road_p0 - (0x10 / 2)] - oroad.road_y[oroad.road_p0];

    /* 0 = No Incline, 10 = Flat Road/Incline */
    incline = (y < 0x12) ? 0x10 : 0;

    /* ------------------------------------------------------------------------ */
    /* Cap Player X Position  */
    /* Set Horizontal Flip Depending On Position Of Car In Relation To Player */
    /* ------------------------------------------------------------------------ */

    x = oinitengine.car_x_pos - (oroad.road_width >> 16);

    if (sprite->control & TRAFFIC_RHS)
    {
        x += (oroad.road_width >> 16) << 1;
    }

    x += (oroad.road_x[z16] - oroad.road_x[z16 - (0x10 / 2)]);

    if (x > 0x190) 
        x = 0x190;
    else if (x < -0x190) 
        x = -0x190;

    x = (x >> 2) + (sprite->xw1 >> 2);
    
    { int8_t traffic_frame = 0;
    int32_t xabs = x < 0 ? -x : x;

    if (xabs < 0x10)
        traffic_frame = 1;
    else if (xabs < 0x30)
        traffic_frame = 2;
    else
        traffic_frame = 3;

    if (x < 0)
    {
        sprite->control &= ~HFLIP;
    }
    else
    {
        sprite->control |= HFLIP;
    }

    /* ------------------------------------------------------------------------ */
    /* Set palette, sprite data etc. based on: */
    /* 1/ Traffic Type */
    /* 2/ Uphill/Straight Road Section */
    /* 3/ Position of Car in relation to player on x axis */
    /* ------------------------------------------------------------------------ */
    
    sprite->pal_src = RomLoader_read8(roms.rom0p, outrun.adr.traffic_props + sprite->type + 4) + self->traffic_pal_cycle;

    { int16_t traffic_type = (RomLoader_read8(roms.rom0p, outrun.adr.traffic_props + sprite->type + 7) << 5) + (traffic_frame << 2) + incline;
    sprite->addr = RomLoader_read32(roms.rom0p, outrun.adr.traffic_data + traffic_type);

    OSprites_map_palette(&osprites, sprite);
    self->traffic_speed_total += sprite->traffic_speed;
    OSprites_do_spr_order_shadows(&osprites, sprite);
 } } } }}static 

void OTraffic_set_zoom_lookup(OTraffic* self, oentry* sprite)
{
    uint8_t zoom_lookup;
    uint16_t road_priority = (sprite->road_priority >> 2) + 4;
    if (road_priority > 0x7F)
        road_priority = 0x7F;

    /* Traffic Properties Table */
    /* */
    /* +0 [Long] Sprite data address */
    /* +4 [Byte] Palette */
    /* +5 [Byte] Collision Mask. Probably to do with the strength/impact of the collision */
    /* +6 [Byte] Zoom Lookup Value for Width/Height */
    /* +7 [Byte] Traffic Type */

    zoom_lookup = RomLoader_read8(roms.rom0p, outrun.adr.traffic_props + sprite->type + 6);

    switch (zoom_lookup)
    {
        case 0:
            road_priority += (road_priority >> 3);
            break;
        case 2:
            road_priority += (road_priority >> 2);
            break;
        case 4:
            road_priority += (road_priority >> 1);
            break;
        case 6:
            road_priority += road_priority;
            break;
    }

    sprite->zoom = (uint8_t) road_priority;
}

/* Set Maximum number of traffic objects to spawn.  */
/* Based on difficulty selected and stage number. */
/* */
/* Maximum Traffic Per Level */
/*  */
/*         | Easy | Norm | Hard | VHar | */
/*         '------'------'------'------' */
/*Stage 1  |   2      3      4      5  | */
/*         '---------------------------' */
/*Stage 2  |   2      4      5      6  | */
/*         '---------------------------' */
/*Stage 3  |   3      5      6      7  | */
/*         '---------------------------' */
/*Stage 4  |   4      6      7      8  | */
/*         '---------------------------' */
/*Stage 5  |   5      7      8      8  | */
/*         '---------------------------' */
/* Source: 0x846E */
void OTraffic_set_max_traffic(OTraffic* self)
{
    if (outrun.cannonball_mode == MODE_ORIGINAL)
    {
        const static uint8_t MAX_TRAFFIC[] =
        {
        /* S1 S2 S3 S4 S5 */
            2, 2, 3, 4, 5, /* Easy Traffic */
            3, 4, 5, 6, 7, /* Normal Traffic */
            4, 5, 6, 7, 8, /* Hard Traffic */
            5, 6, 7, 8, 8, /* Very Hard Traffic */
        };

        { int dip_traffic = config.engine.disable_traffic ? 3 : config.engine.dip_traffic;
        { uint8_t index = (dip_traffic * 5) + (oroad.stage_lookup_off / 8);
        self->max_traffic = MAX_TRAFFIC[index];
     } }}
    else
    {
        self->max_traffic = outrun.custom_traffic;
    }
}

/* ------------- */
/* Traffic Logic */
/* ------------- */
/* */
/* 1/ Handles Traffic to Traffic behaviour */
/* 2/ Adjusts speed of cars to avoid running into each other */
/* 3/ Sets various sprite bits to denote where traffic is in relation to each other */
/* 4/ Calculates average speed of all traffic */
/* */
/* Notes: */
/* Processes sprite in hardware ready format and extracts original addresses where necessary. */
/* */
/* In use: */
/* */
/* d5 = Count of number of traffic sprites spawned */
/* d7 = Loop counter */
/* */
/* a2 = Address of sprite in jump table */
/* a4 = Address of sprite ready for HW */
/* */
/* Source: 0x7990 */
void OTraffic_traffic_logic(OTraffic* self)
{
    oentry* first;
    uint16_t sprite_count = osprites.sprite_count - osprites.spr_cnt_shadow;
    uint16_t spawned = 0; /* d5 */
    
    if (!sprite_count)
    {
        OTraffic_calculate_avg_speed(self, 0);
        return;
    }
       
    first = 0;
    { uint8_t index = 0;
        oentry* next;
    uint16_t spr_index = osprites.spr_cnt_shadow;

    /* Find First Traffic Entry. Note we use the hardware sprite list here to extract the original object. */
    for (index = 0; index < sprite_count; index++)
    {
        uint16_t src_index = osprites.sprite_entries[spr_index++].scratch;

        first = &osprites.jump_table[src_index];
        if (first->control & TRAFFIC_SPRITE)
        {
            self->traffic_adr[spawned++] = first;
            break;
        }
    }

    /* No Traffic Found, get out of there */
    if (!spawned)
    {
        OTraffic_calculate_avg_speed(self, 0);
        return;
    }

    next = 0;

    /* Compare Current Traffic Entry With Previous One */
    { uint8_t index2; for (index2 = index + 1; index2 < sprite_count; index2++)
    {
        uint16_t src_index = osprites.sprite_entries[spr_index++].scratch;
        next = &osprites.jump_table[src_index];
        if (next->control & TRAFFIC_SPRITE)
        {
            self->traffic_adr[spawned++] = next;
            next->traffic_proximity = 0;

            { uint16_t z16 = first->z >> 16;
                int16_t x_diff_abs;
                int16_t x_diff;

            if (z16 < 0x40)
            {
                first = next;
                continue;
            }

            z16 += (z16 >> 1) + (z16 >> 2); /* [x1.75 original value] */

            if (z16 <= next->z >> 16)   
            {
                first = next;
                continue;
            }

            next->traffic_proximity |= BIT_2; /* Denote entry2 is close to other traffic (z axis) */

            x_diff = first->xw1 - next->xw1;
            x_diff_abs = x_diff < 0 ? -x_diff : x_diff;

            if (x_diff_abs - 0x80 >= 0)
            {
                first = next;
                continue;
            }

            if (x_diff >= 0)
            {
                first->traffic_proximity |= BIT_1; /* Entry 1: Denote traffic on RHS */
                next->traffic_proximity |= BIT_0;  /* Entry 2: Denote traffic on LHS [remember x scale is reversed on outrun] */
            }
            else
            {
                first->traffic_proximity |= BIT_0; /* Entry 1: Denote traffic on LHS */
                next->traffic_proximity |= BIT_1;  /* Entry 2: Denote traffic on RHS */
            }

            /* Copy car speed into entry 2 to avoid collision */
            next->traffic_near_speed = first->traffic_speed;
            first = next;
         }}
    } }

    OTraffic_calculate_avg_speed(self, spawned);
 }}

/* Source: 7A6A */static 
void OTraffic_calculate_avg_speed(OTraffic* self, uint16_t c)
{
    self->traffic_count = c;
    if (self->traffic_count != 0)
        self->traffic_speed_avg = self->traffic_speed_total / self->traffic_count;
    self->traffic_speed_total = 0;
}

/* Check For Traffic Collisions */
/* */
/* - Check for collision between traffic sprite and player car */
/* - Setup the skid counter for the player's car */
/* - Adjust player's speed */
/* */
/* Source: 0x50DE */static 

void OTraffic_check_collision(OTraffic* self, oentry* sprite)
{
    uint8_t traffic_fx_old;
    int16_t d0 = 0;

    /* Check for collision */
    if (sprite->z >> 16 >= 0x1D8)
    {
        int16_t w  = (sprite->width >> 1) + (sprite->width >> 3) + (sprite->width >> 4);
        int16_t x1 = sprite->x - w; /* d2 */
        int16_t x2 = sprite->x + w; /* d1 */

        /* Check traffic is directly in front of player's car */
        if (x1 < 0 && x2 > 0)
        {
            /* Set collision settings from property table */
            self->collision_mask = RomLoader_read8(roms.rom0p, outrun.adr.traffic_props + sprite->type + 5);
            d0 = (sprite->x < 0) ? -SKID_RESET : SKID_RESET;

            /* Enhancement: Bumper enabled, reduce skid distance */
            if (config.engine.bumper)
                d0 /= 3;

            d0 += ocrash.skid_counter;

            if (d0 <= SKID_MAX && d0 >= -SKID_MAX)
                ocrash.skid_counter = d0;

            /* Set Ferrari speed based on collision speed */
            if (outrun.game_state == GS_ATTRACT || outrun.game_state == GS_INGAME)
            {
                int16_t traffic_speed = sprite->traffic_speed - 80;
                if (traffic_speed < 0) traffic_speed = 0;
                oinitengine.car_increment = (traffic_speed << 16) | (oinitengine.car_increment & 0xFFFF);
                oferrari.car_inc_old = traffic_speed;
                d0 = SOUND_REBOUND; /* rebound sound effect */
                self->collision_traffic++; /* denote collision with traffic */
                outrun.ttrial.vehicle_cols++;
            }
        }
    }

    /* try_sound: */
    traffic_fx_old = sprite->traffic_fx;
    sprite->traffic_fx = d0 & 0xFF;

    /* New sound effect triggered */
    if (!traffic_fx_old && sprite->traffic_fx)
    {
        OSoundInt_queue_sound(&osoundint, sprite->traffic_fx);
        /* Set all proximity bits on */
        if (outils_random() & 1)
            sprite->traffic_proximity = 0xFF;
    }
}

/* Passing Traffic Sound Effects */
/* Handle up to four cars passing simulataneously */
/* Source: 0x7A8C */
void OTraffic_traffic_sound(OTraffic* self)
{
    int16_t sounds;
    /* Clear traffic data */
    osoundint.engine_data[SOUND_TRAFFIC1] = 0;
    osoundint.engine_data[SOUND_TRAFFIC2] = 0;
    osoundint.engine_data[SOUND_TRAFFIC3] = 0;
    osoundint.engine_data[SOUND_TRAFFIC4] = 0;

    if (outrun.game_state != GS_INGAME && outrun.game_state != GS_ATTRACT)
        return;

    /* Return if we have chosen not to play sound in attract mode */
    if (outrun.game_state == GS_ATTRACT && !config.sound.advertise)
        return;

    /* Return if we haven't spawned any traffic */
    if (!self->traffic_count)
        return;

    /* Max number of sounds we can do is 4 */
    sounds = self->traffic_count <= 4 ? self->traffic_count : 4;

    /* Loop through traffic objects that are on screen */
    { int16_t i; for (i = 0; i < sounds; i++)
    {
        uint8_t vol;
        oentry* t = self->traffic_adr[self->traffic_count - i - 1];
        /* Used to set panning of sound as car moves left and right in front of the player */
        int16_t pan = t->x >> 5; 
        if (pan < -3) pan = -3;
        if (pan >  3) pan =  3;
        pan &= 7;
        /* Position into screen is used to set volume */
        vol = (t->road_priority & 0x1F0) >> 1;
        osoundint.engine_data[SOUND_TRAFFIC1 + i] = pan | vol;
    } }
}
