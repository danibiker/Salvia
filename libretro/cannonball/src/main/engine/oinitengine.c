/***************************************************************************
    Core Game Engine Routines.
    
    - The main loop which advances the level onto the next segment.
    - Code to directly control the road hardware. For example, the road
      split and bonus points routines.
    - Code to determine whether to initialize certain game modes
      (Crash state, Bonus points, road split state) 
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "trackloader.h"

#include "engine/oanimseq.h"
#include "engine/obonus.h"
#include "engine/ocrash.h"
#include "engine/oferrari.h"
#include "engine/ohud.h"
#include "engine/oinputs.h"
#include "engine/olevelobjs.h"
#include "engine/omusic.h"
#include "engine/ooutputs.h"
#include "engine/ostats.h"
#include "engine/outils.h"
#include "engine/opalette.h"
#include "engine/osmoke.h"
#include "engine/otiles.h"
#include "engine/otraffic.h"
#include "engine/oinitengine.h"

static void OInitEngine_setup_stage1(OInitEngine* self);
static void OInitEngine_check_road_split(OInitEngine* self);
static void OInitEngine_check_stage(OInitEngine* self);
static void OInitEngine_init_split1(OInitEngine* self);
static void OInitEngine_init_split2(OInitEngine* self);
static void OInitEngine_init_split3(OInitEngine* self);
static void OInitEngine_init_split4(OInitEngine* self);
static void OInitEngine_init_split5(OInitEngine* self);
static void OInitEngine_init_split6(OInitEngine* self);
static void OInitEngine_init_split7(OInitEngine* self);
static void OInitEngine_init_split9(OInitEngine* self);
static void OInitEngine_init_split10(OInitEngine* self);
static void OInitEngine_bonus1(OInitEngine* self);
static void OInitEngine_bonus2(OInitEngine* self);
static void OInitEngine_bonus3(OInitEngine* self);
static void OInitEngine_bonus4(OInitEngine* self);
static void OInitEngine_bonus5(OInitEngine* self);
static void OInitEngine_bonus6(OInitEngine* self);
static void OInitEngine_reload_stage1(OInitEngine* self);
static void OInitEngine_init_split_next_level(OInitEngine* self);
static void OInitEngine_test_bonus_mode(OInitEngine* self, bool);

OInitEngine oinitengine;

/* Continuous Mode Level Ordering */
const static uint8_t CONTINUOUS_LEVELS[] = {0, 0x8, 0x9, 0x10, 0x11, 0x12, 0x18, 0x19, 0x1A, 0x1B, 0x20, 0x21, 0x22, 0x23, 0x24};

/* Set to 0 to 4 to test bonus sequence, -1 disables */
#define DEBUG_BONUS (-1)




/* Source: 0x8360 */
void OInitEngine_init(OInitEngine* self, int8_t level)
{
    ostats.game_completed  = 0;

    self->ingame_engine          = false;
    self->ingame_counter         = 0;
    ostats.cur_stage       = 0;
    oroad.stage_lookup_off = level ? level : 0;
    self->rd_split_state         = SPLIT_NONE;
    self->road_type              = ROAD_NOCHANGE;
    self->road_type_next         = ROAD_NOCHANGE;
    self->end_stage_props        = 0;
    self->car_increment          = 0;
    self->car_x_pos              = 0;
    self->car_x_old              = 0;
    self->checkpoint_marker      = 0;
    self->road_curve             = 0;
    self->road_curve_next        = 0;
    self->road_remove_split      = 0;
    self->route_selected         = 0;
    
    self->road_width_next        = 0;
    self->road_width_adj         = 0;
    self->change_width           = 0;
    self->granular_rem           = 0;
    self->pos_fine_old           = 0;
    self->road_width_orig        = 0;
    self->road_width_merge       = 0;
    self->route_updated          = 0;

	OInitEngine_init_road_seg_master(self);

    /* Road Renderer: Setup correct stage address  */
    if (level)
        TrackLoader_init_path(&trackloader, oroad.stage_lookup_off);

    OPalette_setup_sky_palette(&opalette);
	OPalette_setup_ground_color(&opalette);
	OPalette_setup_road_centre(&opalette);
	OPalette_setup_road_stripes(&opalette);
	OPalette_setup_road_side(&opalette);
	OPalette_setup_road_colour(&opalette);   
    OTiles_setup_palette_hud(&otiles);                     /* Init Default Palette for HUD */
    OSprites_copy_palette_data(&osprites);                   /* Copy Palette Data to RAM */
    OTiles_setup_palette_tilemap(&otiles);                 /* Setup Palette For Tilemap */
    OInitEngine_setup_stage1(self);                                 /* Setup Misc stuff relating to Stage 1 */
    OTiles_reset_tiles_pal(&otiles);                       /* Reset Tiles, Palette And Road Split Data */
    OCrash_clear_crash_state(&ocrash);

    /* The following is set up specifically for time trial mode */
    if (level)
    {  
        OTiles_init_tilemap_palette(&otiles, level);
        oroad.road_ctrl  = ROAD_BOTH_P0;
        oroad.road_width = RD_WIDTH_MERGE;        /* Setup a default road width */
    }

    OSoundInt_reset(&osoundint);
}

/* Source: 0x8402 */static 
void OInitEngine_setup_stage1(OInitEngine* self)
{
    oroad.road_width = 0x1C2 << 16;     /* Force display of two roads at start */
    ostats.score = 0;
    OStats_clear_stage_times(&ostats);
    OFerrari_reset_car(&oferrari);               /* Reset Car Speed/Rev Values */
    OOutputs_set_digital(outrun.outputs, D_EXT_MUTE);
    OOutputs_set_digital(outrun.outputs, D_SOUND);
    osoundint.engine_data[SOUND_ENGINE_VOL] = 0x3F;
    ostats.extend_play_timer = 0;
    self->checkpoint_marker = 0;              /* Denote not past checkpoint marker */
    OTraffic_set_max_traffic(&otraffic);         /* Set Number Of Enemy Cars Based On Dip Switches */
    OStats_clear_route_info(&ostats);
    OSmoke_setup_smoke_sprite(&osmoke, true);
}

/* Initialise Master Segment Address For Stage */
/* */
/* 1. Read Internal Stage Number from Stage Data Table (Using the lookup offset) */
/* 2. Load the master address, using the stage number as an index. */
/* */
/* Source: 0x8C80 */

void OInitEngine_init_road_seg_master(OInitEngine* self)
{
    TrackLoader_init_track(&trackloader, oroad.stage_lookup_off);
}

/* */
/* Check Road Width */
/* Source: B85A */
/* */
/* Potentially Update Width Of Road */
/* */
/* ADDRESS 2 - Road Segment Data [8 byte boundaries]: */
/* */
/* Word 1 [+0]: Segment Position */
/* Word 2 [+2]: Set = Denotes Road Height Info. Unset = Denotes Road Width */
/* Word 3 [+4]: Segment Road Width / Segment Road Height Index */
/* Word 4 [+6]: Segment Width Adjustment SIGNED (Speed at which width is adjusted) */

void OInitEngine_update_road(OInitEngine* self)
{
    uint32_t addr;
    OInitEngine_check_road_split(self); /* Check/Process road split if necessary */
    addr = 0;
    { uint16_t d0 = TrackLoader_read_width_height(&trackloader, &addr);
        int16_t segment_pos;
    /* Update next road section */
    if (d0 <= oroad.road_pos >> 16)
    {
        /* Skip road width adjustment if set and adjust height */
        if (TrackLoader_read_width_height(&trackloader, &addr) == 0)
        {
            /* ROM:0000B8A6 skip_next_width */
            if (oroad.height_lookup == 0)
                 oroad.height_lookup = TrackLoader_read_width_height(&trackloader, &addr); /* Set new height lookup section */
        }
        else
        {
            /* ROM:0000B87A */
            int16_t width  = TrackLoader_read_width_height(&trackloader, &addr); /* Segment road width */
            int16_t change = TrackLoader_read_width_height(&trackloader, &addr); /* Segment adjustment speed */

            if (width != (int16_t) (oroad.road_width >> 16))
            {
                if (width <= (int16_t) (oroad.road_width >> 16))
                    change = -change;

                self->road_width_next = width;
                self->road_width_adj  = change;
                self->change_width = -1; /* Denote road width is changing */
            }
        }
        trackloader.wh_offset += 8;
    }

    /* ROM:0000B8BC set_road_width     */
    /* Width of road is changing & car is moving */
    if (self->change_width != 0 && self->car_increment >> 16 != 0)
    {
        int32_t d0 = ((self->car_increment >> 16) * self->road_width_adj) << 4;
        oroad.road_width += d0; /* add long here */
        if (d0 > 0)
        {    
            if (self->road_width_next < (int16_t) (oroad.road_width >> 16))
            {
                self->change_width = 0;
                oroad.road_width = self->road_width_next << 16;
            }
        }
        else
        {
            if (self->road_width_next >= (int16_t) (oroad.road_width >> 16))
            {
                self->change_width = 0;
                oroad.road_width = self->road_width_next << 16;
            }
        }
    }
    /* ------------------------------------------------------------------------------------------------ */
    /* ROAD SEGMENT FORMAT */
    /* */
    /* Each segment of road is 6 bytes in memory, consisting of 3 words */
    /* Each road segment is a significant length of road btw :) */
    /* */
    /* ADDRESS 3 - Road Segment Data [6 byte boundaries] */
    /* */
    /* Word 1 [+0]: Segment Position (used with 0x260006 car position) */
    /* Word 2 [+2]: Segment Road Curve */
    /* Word 3 [+4]: Segment Road type (1 = Straight, 2 = Right Bend, 3 = Left Bend) */
    /* */
    /* 60a08 = address of next road segment? (e.g. A0 = 0x0001DD86) */
    /* ------------------------------------------------------------------------------------------------ */

    /* ROM:0000B91C set_road_type:  */

    segment_pos = TrackLoader_read_curve(&trackloader, 0);

    if (segment_pos != -1)
    {
        int16_t d1 = segment_pos - 0x3C;

        if (d1 <= (int16_t) (oroad.road_pos >> 16))
        {
            self->road_curve_next = TrackLoader_read_curve(&trackloader, 2);
            self->road_type_next  = TrackLoader_read_curve(&trackloader, 4);
        }

        if (segment_pos <= (int16_t) (oroad.road_pos >> 16))
        {
            self->road_curve = TrackLoader_read_curve(&trackloader, 2);
            self->road_type  = TrackLoader_read_curve(&trackloader, 4);
            trackloader.curve_offset += 6;
            self->road_type_next = 0;
            self->road_curve_next = 0;
        }
    }
 }}

/* Carries on from the above in the original code */
void OInitEngine_update_engine(OInitEngine* self)
{   
    /* ------------------------------------------------------------------------ */
    /* TILE MAP OFFSETS */
    /* ROM:0000B986 setup_shadow_offset: */
    /* Setup the shadow offset based on how much we've scrolled left/right. Lovely and subtle! */
    /* ------------------------------------------------------------------------ */

    OInitEngine_update_shadow_offset(self);

    /* ------------------------------------------------------------------------ */
    /* Main Car Logic Block */
    /* ------------------------------------------------------------------------ */

    OFerrari_move(&oferrari);

    if (oferrari.car_ctrl_active)
    {
        OFerrari_set_curve_adjust(&oferrari);
        OFerrari_set_ferrari_x(&oferrari);
        OFerrari_do_skid(&oferrari);
        OFerrari_check_wheels(&oferrari);
        OFerrari_set_ferrari_bounds(&oferrari);
    }

    OFerrari_do_sound_score_slip(&oferrari);

    /* ------------------------------------------------------------------------ */
    /* Setup New Sprite Scroll Speed. Based On Granular Difference. */
    /* ------------------------------------------------------------------------ */
    OInitEngine_set_granular_position(self);
    OInitEngine_set_fine_position(self);

    /* Draw Speed & Hud Stuff */
    if (outrun.game_state >= GS_START1 && outrun.game_state <= GS_BONUS)
    {
        /* Convert & Blit Car Speed */
        OHud_blit_speed(&ohud, 0x110CB6, self->car_increment >> 16);
        OHud_blit_text1(&ohud, HUD_KPH1);
        OHud_blit_text1(&ohud, HUD_KPH2);

        /* Blit High/Low Gear. */
        /* Libretro displays the current transmission state in every gear mode, */
        /* including automatic transmission. */
        if (oinputs.gear)
            OHud_blit_text_new(&ohud, 9, 26, "H", GREEN);
        else
            OHud_blit_text_new(&ohud, 9, 26, "L", GREY);

        if (config.engine.layout_debug)
            OHud_draw_debug_info(&ohud, oroad.road_pos, oroad.height_lookup_wrk, TrackLoader_read_sprite_pattern_index(&trackloader));
    }

    if (olevelobjs.spray_counter > 0)
        olevelobjs.spray_counter--;

    if (olevelobjs.sprite_collision_counter > 0)
        olevelobjs.sprite_collision_counter--;

    OPalette_setup_sky_cycle(&opalette);
}

void OInitEngine_update_shadow_offset(OInitEngine* self)
{
    int16_t shadow_off = oroad.tilemap_h_target & 0x3FF;
    if (shadow_off > 0x1FF)
        shadow_off = -shadow_off + 0x3FF;
    shadow_off >>= 2;
    if (oroad.tilemap_h_target & BIT_A)
        shadow_off = -shadow_off; /* reverse direction of shadow */
    osprites.shadow_offset = shadow_off;
}

/* Check for Road Split */
/* */
/* - Checks position in level and determine whether to init road split */
/* - Processes road split if initialized */
/* */
/* Source: 868E */static 
void OInitEngine_check_road_split(OInitEngine* self)
{
    /* Check whether to initialize the next level */
    OStats_init_next_level(&ostats);

    switch (self->rd_split_state)
    {
        /* State 0: No road split. Check current road position with ROAD_END. */
        case SPLIT_NONE:
            if (DEBUG_BONUS == -1)
            {
                if (oroad.road_pos >> 16 <= ROAD_END) return;
                OInitEngine_check_stage(self); /* Do Split - Split behaviour depends on stage */
            }
            else
            {
                if (oroad.road_pos >> 16 <= 0x100) return;
                OInitEngine_init_bonus(self, DEBUG_BONUS);
            }
            break;

        /* State 1: (Does this ever need to be called directly?) */
        case SPLIT_INIT:
            OInitEngine_init_split1(self);
            break;
        
        /* State 2: Road Split */
        case SPLIT_CHOICE1:
            if (oroad.road_pos >> 16 >= 0x3F)  
                OInitEngine_init_split2(self);            
            break;

        /* State 3: Beginning of split. User must choose. */
        case SPLIT_CHOICE2:
            OInitEngine_init_split2(self);
            break;

        /* State 4: Road physically splits into two individual roads */
        case 4:
            OInitEngine_init_split3(self);
            break;

        /* State 5: Road fully split. Init remove other road */
        case 5:
            if (!self->road_curve)
                self->rd_split_state = 6; /* and fall through */
            else
                break;
        
        /* State 6: Road split. Only one road drawn. */
        case 6:
            OInitEngine_init_split5(self);
            break;

        /* Stage 7 */
        case 7:
            OInitEngine_init_split6(self);
            break;

        /* State 8: Init Road Merge before checkpoint sign */
        case 8:
            otraffic.traffic_split = -1;
            self->rd_split_state = 9;
            break;

        /* State 9: Road Merge before checkpoint sign */
        case 9:
            otraffic.traffic_split = 0;
        case 0x0A:
            OInitEngine_init_split9(self);
            break;

        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            OInitEngine_init_split10(self);
            break;
        
        /* Init Bonus Sequence */
        case 0x10:
            OInitEngine_init_bonus(self, oroad.stage_lookup_off - 0x20);
            break;

        case 0x11:
            OInitEngine_bonus1(self);
            break;

        case 0x12:
            OInitEngine_bonus2(self);
            break;

        case 0x13:
            OInitEngine_bonus3(self);
            break;

        case 0x14:
            OInitEngine_bonus4(self);
            break;

        case 0x15:
            OInitEngine_bonus5(self);
            break;

        case 0x16:
        case 0x17:
        case 0x18:
            OInitEngine_bonus6(self);
            break;
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Check Stage Info To Determine What To Do With Road */
/* */
/* Stage 1-4: Init Road Split */
/* Stage 5: Init Bonus */
/* Stage 5 ATTRACT: Loop to Stage 1 */
/* ------------------------------------------------------------------------------------------------ */static 
void OInitEngine_check_stage(OInitEngine* self)
{
    /* Time Trial Mode */
    if (outrun.cannonball_mode == MODE_TTRIAL)
    {
        int16_t counter;
        /* Store laptime and reset */
        uint8_t* laptimes = outrun.ttrial.laptimes[outrun.ttrial.current_lap];

        laptimes[0] = ostats.stage_times[0][0];
        laptimes[1] = ostats.stage_times[0][1];
        laptimes[2] = ostats.stage_times[0][2];
        ostats.stage_times[0][0] = 
        ostats.stage_times[0][1] = 
        ostats.stage_times[0][2] = 0;

        /* Check for new best laptime */
        counter = ostats.stage_counters[outrun.ttrial.current_lap];
        if (counter < outrun.ttrial.best_lap_counter)
        {
            outrun.ttrial.best_lap_counter = counter;
            outrun.ttrial.best_lap[0] = laptimes[0];
            outrun.ttrial.best_lap[1] = laptimes[1];
            outrun.ttrial.best_lap[2] = ostats.lap_ms[laptimes[2]];

            /* Draw best laptime */
            ostats.extend_play_timer = 0x80;
            OHud_blit_text1(&ohud, TEXT1_LAPTIME1);
            OHud_blit_text1(&ohud, TEXT1_LAPTIME2);
            OHud_draw_lap_timer(&ohud, 0x110554, laptimes, ostats.lap_ms[laptimes[2]]);

            outrun.ttrial.new_high_score = true;
        }

        if (outrun.game_state == GS_INGAME)
        {
            /* More laps to go, loop the course */
            if (++outrun.ttrial.current_lap < outrun.ttrial.laps)
            {
                /* Update lap number */
                oroad.road_pos = 0;
                oroad.tilemap_h_target = 0;
                OInitEngine_init_road_seg_master(self);
            }
            else 
            {
                /* Set correct finish segment for final 5 stages, otherwise just default to first one. */
                oroad.stage_lookup_off = oroad.stage_lookup_off < 0x20 ? 0x20 : oroad.stage_lookup_off;
                ostats.time_counter = 1;
                OInitEngine_init_bonus(self, oroad.stage_lookup_off - 0x20);
            }
        }
    }
    else if (outrun.cannonball_mode == MODE_CONT)
    {
        oroad.road_pos         = 0;
        oroad.tilemap_h_target = 0;
        
        if ((ostats.cur_stage + 1) == 15)
        {
            if (outrun.game_state == GS_INGAME)
                OInitEngine_init_bonus(self, outils_random() % 5);
            else
                OInitEngine_reload_stage1(self);
        }
        else
        {
            oroad.stage_lookup_off = CONTINUOUS_LEVELS[++ostats.cur_stage];
            OInitEngine_init_road_seg_master(self);
            OSprites_clear_palette_data(&osprites);

            /* Init next tilemap */
            OTiles_set_vertical_swap(&otiles); /* Tell tilemap to v-scroll off/on */

            /* Reload smoke data */
            OSmoke_setup_smoke_sprite(&osmoke, true);

            /* Update palette */
            oinitengine.end_stage_props |= BIT_1; /* Don't bump stage offset when fetching next palette */
            oinitengine.end_stage_props |= BIT_2;
            opalette.pal_manip_ctrl = 1;
            OPalette_setup_sky_change(&opalette);
            
            /* Denote Checkpoint Passed */
            self->checkpoint_marker = -1;

            /* Cycle Music every 5 stages */
            if (outrun.game_state == GS_INGAME)
            {
                if (ostats.cur_stage == 5 || ostats.cur_stage == 10)
                    OMusic_cycle_music(&omusic);
            }              
        }
    }

    /* Stages 0-4, do road split */
    else if (ostats.cur_stage <= 3)
    {
        self->rd_split_state = SPLIT_INIT;
        OInitEngine_init_split1(self);
    }
    /* Stage 5: Init Bonus */
    else if (outrun.game_state == GS_INGAME)
    {
        OInitEngine_init_bonus(self, oroad.stage_lookup_off - 0x20);
    }
    /* Stage 5 Attract Mode: Reload Stage 1 */
    else
    {
        OInitEngine_reload_stage1(self);
    }
}static 

void OInitEngine_reload_stage1(OInitEngine* self)
{
    oroad.road_pos         = 0;
    oroad.tilemap_h_target = 0;
    ostats.cur_stage       = -1;
    oroad.stage_lookup_off = -8;

    OStats_clear_route_info(&ostats);

    self->end_stage_props |= BIT_1; /* Loop back to stage 1 (Used by tilemap code) */
    self->end_stage_props |= BIT_2;
    self->end_stage_props |= BIT_3;
    OSmoke_setup_smoke_sprite(&osmoke, true);
    OInitEngine_init_split_next_level(self);
}

/* ------------------------------------------------------------------------------------------------ */
/* Road Split 1 */
/* Init Road Split & Begin Road Split */
/* Called When We're Not On The Final Stage */
/* ------------------------------------------------------------------------------------------------ */static 
void OInitEngine_init_split1(OInitEngine* self)
{
    self->rd_split_state = SPLIT_CHOICE1;

    oroad.road_load_split  = -1;
    oroad.road_ctrl        = ROAD_BOTH_P0_INV; /* Both Roads (Road 0 Priority) (Road Split. Invert Road 0) */
    self->road_width_orig        = oroad.road_width >> 16;
    oroad.road_pos         = 0;
    oroad.tilemap_h_target = 0;
    TrackLoader_init_track_split(&trackloader);
}

/* ------------------------------------------------------------------------------------------------ */
/* Road Split 2: Beginning of split. User must choose. */
/* ------------------------------------------------------------------------------------------------ */static 
void OInitEngine_init_split2(OInitEngine* self)
{
    int16_t pos;
    self->rd_split_state = SPLIT_CHOICE2;

    /* Manual adjustments to the road width, based on the current position */
    pos = (((oroad.road_pos >> 16) - 0x3F) << 3) + self->road_width_orig;
    oroad.road_width = (pos << 16) | (oroad.road_width & 0xFFFF);
    if (pos > 0xFF)
    {
        self->route_updated &= ~BIT_0;
        OInitEngine_init_split3(self);
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Road Split 3: Road physically splits into two individual roads */
/* ------------------------------------------------------------------------------------------------ */static 
void OInitEngine_init_split3(OInitEngine* self)
{
    self->rd_split_state = 4;

    { int16_t pos = (((oroad.road_pos >> 16) - 0x3F) << 3) + self->road_width_orig;
    oroad.road_width = (pos << 16) | (oroad.road_width & 0xFFFF);

    if (self->route_updated & BIT_0 || pos <= 0x168)
    {
        if (oroad.road_width >> 16 > 0x300)
            OInitEngine_init_split4(self);
        return;
    }

    self->route_updated |= BIT_0; /* Denote route info updated */
    self->route_selected = 0;

    /* Go Left */
    if (self->car_x_pos > 0)
    {
        self->route_selected = -1;
        { uint8_t inc = 1 << (3 - ostats.cur_stage);

        /* One of the following increment values */

        /* Stage 1 = +8 (1 << 3 - 0) */
        /* Stage 2 = +4 (1 << 3 - 1) */
        /* Stage 3 = +2 (1 << 3 - 2) */
        /* Stage 4 = +1 (1 << 3 - 3) */
        /* Stage 5 = Road doesn't split on this stage */

        ostats.route_info += inc;
        oroad.stage_lookup_off++;
     }}
    /* Go Right / Continue */

    self->end_stage_props |= BIT_0;                                 /* Set end of stage property (road splitting) */
    osmoke.load_smoke_data |= BIT_0;                          /* Denote we should load smoke sprite data */
    ostats.routes[0]++;                                       /* Set upcoming stage number to store route info */
    ostats.routes[ostats.routes[0]] = ostats.route_info;      /* Store route info for course map screen */

    if (oroad.road_width >> 16 > 0x300)
        OInitEngine_init_split4(self);
 }}

/* ------------------------------------------------------------------------------------------------ */
/* Road Split 4: Road Fully Split, Remove Other Road */
/* ------------------------------------------------------------------------------------------------ */static 

void OInitEngine_init_split4(OInitEngine* self)
{
    self->rd_split_state = 5; /* init_split4 */

     /* Set Appropriate Road Control Value, Dependent On Route Chosen */
    if (self->route_selected != 0)
        oroad.road_ctrl = ROAD_R0_SPLIT;
    else
        oroad.road_ctrl = ROAD_R1_SPLIT;

    /* Denote road split has been removed (for enemy traFfic logic) */
    self->road_remove_split |= BIT_0;
       
    if (!self->road_curve)
        OInitEngine_init_split5(self);
}

/* ------------------------------------------------------------------------------------------------ */
/* Road Split 5: Only Draw One Section Of Road - Wait For Final Curve */
/* ------------------------------------------------------------------------------------------------ */static 

void OInitEngine_init_split5(OInitEngine* self)
{
    self->rd_split_state = 6;
    if (self->road_curve)
        OInitEngine_init_split6(self);
}

/* ------------------------------------------------------------------------------------------------ */
/* Road Split 6 - Car On Final Curve Of Split */
/* ------------------------------------------------------------------------------------------------ */static 
void OInitEngine_init_split6(OInitEngine* self)
{
    self->rd_split_state = 7;
    if (!self->road_curve)
        OInitEngine_init_split7(self);
}

/* ------------------------------------------------------------------------------------------------ */
/* Road Split 7: Init Road Merge Before Checkpoint (From Normal Section Of Road) */
/* ------------------------------------------------------------------------------------------------ */static 

void OInitEngine_init_split7(OInitEngine* self)
{
    int16_t width2;
    self->rd_split_state = 8;

    oroad.road_ctrl = ROAD_BOTH_P0;
    self->route_selected = ~self->route_selected; /* invert bits */
    width2 = (oroad.road_width >> 16) << 1;
    if (self->route_selected == 0) 
        width2 = -width2;
    self->car_x_pos += width2;
    self->car_x_old += width2;
    self->road_width_orig = oroad.road_pos >> 16;
    self->road_width_merge = oroad.road_width >> 19; /* (>> 16 and then >> 3) */
    self->road_remove_split &= ~BIT_0; /* Denote we're back to normal road handling for enemy traffic logic */
}

/* ------------------------------------------------------------------------------------------------ */
/* Road Split 9 - Do Road Merger. Road Gets Narrower Again. */
/* ------------------------------------------------------------------------------------------------ */static 
void OInitEngine_init_split9(OInitEngine* self)
{
    uint16_t d0;
    self->rd_split_state = 10;

    /* Calculate narrower road width to merge roads */
    d0 = (self->road_width_merge - ((oroad.road_pos >> 16) - self->road_width_orig)) << 3;

    if (d0 <= RD_WIDTH_MERGE)
    {
        oroad.road_width = (RD_WIDTH_MERGE << 16) | (oroad.road_width & 0xFFFF);
        OInitEngine_init_split10(self);
    }
    else
        oroad.road_width = (d0 << 16) | (oroad.road_width & 0xFFFF);
}


/* ------------------------------------------------------------------------------------------------ */
/* Road Split A: Checkpoint Sign */
/* ------------------------------------------------------------------------------------------------ */static 
void OInitEngine_init_split10(OInitEngine* self)
{
    self->rd_split_state = 11;

    if (oroad.road_pos >> 16 > 0x180)
    {
        self->rd_split_state = 0;
        OInitEngine_init_split_next_level(self);
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Road Split B: Init Next Level */
/* ------------------------------------------------------------------------------------------------ */static 
void OInitEngine_init_split_next_level(OInitEngine* self)
{
    oroad.road_pos = 0;
    oroad.tilemap_h_target = 0;
    ostats.cur_stage++;
    oroad.stage_lookup_off += 8;    /* Increment lookup to next block of stages */
    ostats.route_info += 0x10;      /* Route Info increments by 10 at each stage */
    OHud_do_mini_map(&ohud);
    OInitEngine_init_road_seg_master(self);

    /* Clear sprite palette lookup */
    if (ostats.cur_stage)
        OSprites_clear_palette_data(&osprites);
}

/* ------------------------------------------------------------------------------------------------ */
/* Bonus Road Mode Control */
/* ------------------------------------------------------------------------------------------------ */

/* Initialize new segment of road data for bonus sequence */
/* Source: 0x8A04 */
void OInitEngine_init_bonus(OInitEngine* self, int16_t seq)
{
    oroad.road_ctrl = ROAD_BOTH_P0_INV;
    oroad.road_pos  = 0;
    oroad.tilemap_h_target = 0;
    oanimseq.end_seq = (uint8_t) seq; /* Set End Sequence (0 - 4) */
    TrackLoader_init_track_bonus(&trackloader, oanimseq.end_seq);
    outrun.game_state = GS_INIT_BONUS;
    self->rd_split_state = 0x11;
    OInitEngine_bonus1(self);
}static 

void OInitEngine_bonus1(OInitEngine* self)
{
    if (oroad.road_pos >> 16 >= 0x5B)
    {
        otraffic.bonus_lhs = 1; /* Force traffic spawn on LHS during bonus mode */
        self->rd_split_state = 0x12;
        OInitEngine_bonus2(self);
    }
}static 

void OInitEngine_bonus2(OInitEngine* self)
{
    if (oroad.road_pos >> 16 >= 0xB6)
    {
        otraffic.bonus_lhs = 0; /* Disable forced traffic spawn */
        self->road_width_orig = oroad.road_width >> 16;
        self->rd_split_state = 0x13;
        OInitEngine_bonus3(self);
    }
}

/* Stretch the road to a wider width. It does this based on the car's current position. */static 
void OInitEngine_bonus3(OInitEngine* self)
{
    /* Manual adjustments to the road width, based on the current position */
    int16_t pos = (((oroad.road_pos >> 16) - 0xB6) << 3) + self->road_width_orig;
    oroad.road_width = (pos << 16) | (oroad.road_width & 0xFFFF);
    if (pos > 0xFF)
    {
        self->route_selected = 0;
        if (self->car_x_pos > 0)
            self->route_selected = ~self->route_selected; /* invert bits */
        self->rd_split_state = 0x14;
        OInitEngine_bonus4(self);
    }
}static 

void OInitEngine_bonus4(OInitEngine* self)
{
    /* Manual adjustments to the road width, based on the current position */
    int16_t pos = (((oroad.road_pos >> 16) - 0xB6) << 3) + self->road_width_orig;
    oroad.road_width = (pos << 16) | (oroad.road_width & 0xFFFF);
    if (pos > 0x300)
    {
         /* Set Appropriate Road Control Value, Dependent On Route Chosen */
        if (self->route_selected != 0)
            oroad.road_ctrl = ROAD_R0_SPLIT;
        else
            oroad.road_ctrl = ROAD_R1_SPLIT;

        /* Denote road split has been removed (for enemy traFfic logic) */
        self->road_remove_split |= BIT_0;
        self->rd_split_state = 0x15;
        OInitEngine_bonus5(self);
    }
}

/* Check for end of curve. Init next state when ended. */static 
void OInitEngine_bonus5(OInitEngine* self)
{
    if (!self->road_curve)
    {
        self->rd_split_state = 0x16;
        OInitEngine_bonus6(self);
    }
}

/* This state simply checks for the end of bonus mode */static 
void OInitEngine_bonus6(OInitEngine* self)
{
    if (obonus.bonus_control >= BONUS_END)
        self->rd_split_state = 0;
}

/* SetGranularPosition */
/* */
/* Source: BD3E */
/* */
/* Uses the car increment value to set the granular position. */
/* The granular position is used to finely scroll the road by CPU 1 and smooth zooming of scenery. */
/* */
/* pos_fine is the (road_pos >> 16) * 10 */
/* */
/* Notes: */
/* Disable with - bpset bd3e,1,{pc = bd76; g} */

void OInitEngine_set_granular_position(OInitEngine* self)
{
    uint16_t car_inc16 = self->car_increment >> 16;

    uint16_t result = car_inc16 / 0x40;
    uint16_t rem = car_inc16 % 0x40;

    self->granular_rem += rem;
    /* When the overall counter overflows past 0x40, we must carry a 1 to the unsigned divide :) */
    if (self->granular_rem >= 0x40)
    {
        self->granular_rem -= 0x40;
        result++;
    }
    oroad.pos_fine += result;
}

void OInitEngine_set_fine_position(OInitEngine* self)
{
    uint16_t d0 = oroad.pos_fine - self->pos_fine_old;
    if (d0 > 0xF)
        d0 = 0xF;

    d0 <<= 0xB;
    osprites.sprite_scroll_speed = d0;

    self->pos_fine_old = oroad.pos_fine;
}

/* Check whether to initalize crash or bonus sequence code */
/* */
/* Source: 0x984E */
void OInitEngine_init_crash_bonus(OInitEngine* self)
{
    if (outrun.game_state == GS_MUSIC) return;

    if (ocrash.skid_counter > 6 || ocrash.skid_counter < -6)
    {
        /* do_skid: */
        if (otraffic.collision_traffic == 1)
        {   
            otraffic.collision_traffic = 2;
            { uint8_t rnd = outils_random() & otraffic.collision_mask;
            if (rnd == otraffic.collision_mask)
            {
                /* Try to launch crash code and perform a spin */
                if (ocrash.coll_count1 == ocrash.coll_count2)
                {
                    if (!ocrash.spin_control1)
                    {
                        ocrash.spin_control1 = 1;
                        ocrash.skid_counter_bak = ocrash.skid_counter;
                        OInitEngine_test_bonus_mode(self, true); /* 9924 fall through */
                        return;
                    }
                    OInitEngine_test_bonus_mode(self, false); /* finalise skid                */
                    return;
                }
                else
                {
                    OInitEngine_test_bonus_mode(self, true); /* test_bonus_mode */
                    return;
                }
            }
         }}
    }
    else if (ocrash.spin_control2 == 1)
    {
        /* 9894 */
        ocrash.spin_control2 = 2;
        if (ocrash.coll_count1 == ocrash.coll_count2)
        {
            OCrash_enable(&ocrash);
        }
        OInitEngine_test_bonus_mode(self, false); /* finalise skid */
        return;
    }
    else if (ocrash.spin_control1 == 1)
    {
        /* 98c0 */
        ocrash.spin_control1 = 2;
        OCrash_enable(&ocrash);
        OInitEngine_test_bonus_mode(self, false); /* finalise skid */
        return;
    }

    /* 0x9924: Section Of Code */
    if (ocrash.coll_count1 == ocrash.coll_count2) 
    {
        OInitEngine_test_bonus_mode(self, true);  /* test_bonus_mode */
    }
    else
    {
        OCrash_enable(&ocrash);
        OInitEngine_test_bonus_mode(self, false); /* finalise skid */
    }
}

/* Source: 0x993C */static 
void OInitEngine_test_bonus_mode(OInitEngine* self, bool do_bonus_check)
{
    /* Bonus checking code  */
    if (do_bonus_check && obonus.bonus_control)
    {
        /* Do Bonus Text Display */
        if (outrun.cannonball_mode != MODE_TTRIAL && obonus.bonus_state < 3)
            OBonus_do_bonus_text(&obonus);

        /* End Seq Animation Stage #0 */
        if (obonus.bonus_control == BONUS_SEQ0)
        {
            if (outrun.cannonball_mode == MODE_TTRIAL)
                outrun.game_state = GS_INIT_GAMEOVER;
            else
                OAnimSeq_init_end_seq(&oanimseq);
        }
    }

   /* finalise_skid: */
   if (!ocrash.skid_counter)
       otraffic.collision_traffic = 0;
}