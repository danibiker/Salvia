/***************************************************************************
    OutRun Engine Entry Point.

    This is the hub of the ported OutRun code.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdlib.h>
#include "../setup.h"
#include "main.h"
#include "trackloader.h"
#include "../utils.h"
#include "oattractai.h"
#include "oanimseq.h"
#include "obonus.h"
#include "ocrash.h"
#include "oferrari.h"
#include "ohiscore.h"
#include "ohud.h"
#include "oinputs.h"
#include "olevelobjs.h"
#include "ologo.h"
#include "omap.h"
#include "omusic.h"
#include "ooutputs.h"
#include "osmoke.h"
#include "outrun.h"
#include "opalette.h"
#include "ostats.h"
#include "otiles.h"
#include "otraffic.h"
#include "outils.h"

static void Outrun_jump_table(Outrun* self);
static void Outrun_init_jump_table(Outrun* self);
static void Outrun_main_switch(Outrun* self);
static void Outrun_controls(Outrun* self);
static bool Outrun_decrement_timers(Outrun* self);
static void Outrun_init_motor_calibration(Outrun* self);
static void Outrun_init_attract(Outrun* self);
static void Outrun_tick_attract(Outrun* self);
static void Outrun_check_freeplay_start(Outrun* self);

Outrun outrun;

/*
    Known Core Engine Issues:

    - Road split. Minor bug on positioning traffic on correct side of screen for one frame or so at the point of split.
      Most noticeable in 60fps mode. 
      The Dreamcast version exhibits a bug where the road renders on the wrong side of the screen for one frame at this point.
      The original version (and Cannonball) has a problem where the cars face the wrong direction for one frame. 

    Bugs Present In Original 1986 Release:

    - Millisecond displays incorrectly on Extend Time screen [fixed]
    - Erroneous values in sprite zooming table [fixed]
    - Shadow popping into position randomly. Try setting car x position to 0x1E2. (0x260050) [fixed]
    - Stage 2a: Incomplete arches due to lack of sprite slots [fixed]
    - Best OutRunners screen looks odd after Stage 2 Gateway
    - Stage 3c: Clouds overlapping trees [unable to fix easily]
    - Sometimes the Ferrari stalls on the start-line on game restart. Happens in Attract Mode too.
    - On completion screen, some of the side crowd graphics are misplaced. Japanese version only [fixed]

*/

void Outrun_ctor(Outrun* self)
{
    self->outputs = (OOutputs*)malloc(sizeof(OOutputs));
    OOutputs_ctor(self->outputs);
}

void Outrun_dtor(Outrun* self)
{
    free(self->outputs);
}

void Outrun_init(Outrun* self)
{
    self->freeze_timer = self->cannonball_mode == MODE_TTRIAL ? true : config.engine.freeze_timer;
    video.enabled = false;
    Outrun_select_course(self, config.engine.jap != 0, config.engine.prototype != 0);
    Video_clear_text_ram(&video);

    self->tick_counter = 0;

    if (config.controls.haptic)
        OOutputs_set_mode(self->outputs, MODE_FFEEDBACK);
    else
        OOutputs_set_mode(self->outputs, MODE_DISABLED);

    Outrun_boot(self);
}

void Outrun_boot(Outrun* self)
{
    self->game_state = config.engine.layout_debug ? GS_INIT_GAME : GS_INIT;
    /* Initialize default hi-score entries */
    OHiScore_init_def_scores(&ohiscore);
    /* Load saved hi-score entries */
    Config_load_scores(&config, 
        self->cannonball_mode == MODE_ORIGINAL
            ? FILENAME_SCORES
            : FILENAME_CONT);
    OStats_init(&ostats, self->cannonball_mode == MODE_TTRIAL);
    Outrun_init_jump_table(self);
    OInitEngine_init(&oinitengine, self->cannonball_mode == MODE_TTRIAL ? self->ttrial.level : 0);
    OSoundInt_init(&osoundint);
    outils_reset_random_seed(); /* Ensure we match the genuine boot up of the original game each time */
}

void Outrun_tick(Outrun* self, bool tick_frame)
{
    self->tick_frame = tick_frame;
    
    if (tick_frame)
    {
        self->tick_counter++;

        if (self->game_state >= GS_START1 && self->game_state <= GS_INGAME)
        {
            if (Input_has_pressed(&input, VIEWPOINT))
            {
                int mode = ORoad_get_view_mode(&oroad) + 1;
                if (mode > VIEW_INCAR)
                    mode = VIEW_ORIGINAL;

                ORoad_set_view_mode(&oroad, mode, false);
            }
        }
    }

    /* Only tick the road cpu twice for every time we tick the main cpu */
    /* The timing here isn't perfect, as normally the road CPU would run in parallel with the main CPU. */
    /* We can potentially hack this by calling the road CPU twice. */
    /* Most noticeable with clipping sprites on hills. */
      
    /* 30 FPS  */
    /* Updates Game Logic 1/2 frames */
    /* Updates V-Blank 1/2 frames */
    if (config.fps == 30 && config.tick_fps == 30)
    {
        Outrun_jump_table(self);
        ORoad_tick(&oroad);
        Outrun_vint(self);
        Outrun_vint(self);
    }
    /* 30/60 FPS Hybrid. (This is the same as the original game) */
    /* Updates Game Logic 1/2 frames */
    /* Updates V-Blank 1/1 frames */
    else if (config.fps == 60 && config.tick_fps == 30)
    {
        if (tick_frame)
        {
            Outrun_jump_table(self);
            ORoad_tick(&oroad);
        }
        Outrun_vint(self);
    }
    /* 60 FPS. Smooth Mode.  */
    /* Updates Game Logic 1/1 frames */
    /* Updates V-Blank 1/1 frames */
    else
    {
        Outrun_jump_table(self);
        ORoad_tick(&oroad);
        Outrun_vint(self);
    }

    /* Moved out of vertical interrupt */
    if (tick_frame)
    {
        uint8_t coin = OInputs_do_credits(&oinputs);
        OOutputs_coin_chute_out(self->outputs, &self->outputs->chute1, coin == 1);
        OOutputs_coin_chute_out(self->outputs, &self->outputs->chute2, coin == 2);
    }

    /* Draw FPS */
    if (config.video.fps_count)
        OHud_draw_fps_counter(&ohud, cannonball_fps_counter);
}

/* Vertical Interrupt */
void Outrun_vint(Outrun* self)
{
    OTiles_write_tilemap_hw(&otiles);
    OSprites_update_sprites(&osprites);
    OTiles_update_tilemaps(&otiles, self->cannonball_mode == MODE_ORIGINAL ? ostats.cur_stage : 0);
    /* The engine assumes this block runs at the hardware v-blank rate of 60Hz. */
    /* At 120 FPS Outrun_vint() is called once per frame (120Hz), so halve it here. */
    /* Running set_granular_position() at 120Hz advances oroad.pos_fine twice as */
    /* fast as OSprites_sprite_control() can pump out level objects, which silently */
    /* drops sprites - including checkpoint signs - from the tail of a scenery */
    /* segment, and makes OStats_do_timers() count at double rate. */
    if (config.fps < 120 || (cannonball_frame & 1))
    {
        OPalette_cycle_sky_palette(&opalette);
        OPalette_fade_palette(&opalette);
        OStats_do_timers(&ostats);
        if (self->cannonball_mode != MODE_TTRIAL) OHud_draw_timer1(&ohud, ostats.time_counter);
        OInitEngine_set_granular_position(&oinitengine);
    }
}

static void Outrun_jump_table(Outrun* self)
{
    if (self->tick_frame && self->game_state != GS_CALIBRATE_MOTOR)
    {
        Outrun_main_switch(self);                  /* Address #1 (0xB128) - Main Switch */
        OInputs_adjust_inputs(&oinputs);        /* Address #2 (0x74D8) - Adjust Analogue Inputs */
    }

    switch (self->game_state)
    {
        case GS_REINIT:
        case GS_CALIBRATE_MOTOR:
            break;
 
        /* ---------------------------------------------------------------------------------------- */
        /* Couse Map Specific Code */
        /* ---------------------------------------------------------------------------------------- */
        case GS_MAP:
            OMap_tick(&omap);
            break;


        case GS_MUSIC:
            if (self->tick_frame) OMusic_check_start(&omusic); /* Check for start button */
            OSprites_tick(&osprites);
            OLevelObjs_do_sprite_routine(&olevelobjs);

            if (!outrun.tick_frame)
            {
                OMusic_blit(&omusic);
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Best OutRunners Entry (EditJumpTable3 Entries) */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_BEST2:
        case GS_BEST2:
            OSprites_tick(&osprites);
            OLevelObjs_do_sprite_routine(&olevelobjs);

            if (!self->tick_frame)
            {
                /* Check for start button if credits are remaining and set state to Music Selection */
                if (ostats.credits && Input_is_pressed_clear(&input, START))
                    self->game_state = GS_INIT_MUSIC;
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Core Game Engine Routines */
        /* ---------------------------------------------------------------------------------------- */
        case GS_LOGO:
            if (!self->tick_frame)
                OLogo_blit(&ologo);

        case GS_ATTRACT:
        case GS_BEST1:
            if (self->tick_frame) Outrun_check_freeplay_start(self);
        
        default:
            if (self->tick_frame) OSprites_tick(&osprites);                /* Address #3 Jump_SetupSprites */
            OLevelObjs_do_sprite_routine(&olevelobjs);                 /* replaces calling each sprite individually */
            if (!config.engine.disable_traffic)
                OTraffic_tick(&otraffic);                            /* Spawn & Tick Traffic */
            if (self->tick_frame) OInitEngine_init_crash_bonus(&oinitengine); /* Initalize crash sequence or bonus code */
            OFerrari_tick(&oferrari);
            if (oferrari.state != FERRARI_END_SEQ)
            {
                OAnimSeq_flag_seq(&oanimseq);
                OCrash_tick(&ocrash);
                OSmoke_draw_ferrari_smoke(&osmoke, &osprites.jump_table[SPRITE_SMOKE1]); /* Do Left Hand Smoke */
                OFerrari_draw_shadow(&oferrari);                                                   /* (0xF1A2) - Draw Ferrari Shadow */
                OSmoke_draw_ferrari_smoke(&osmoke, &osprites.jump_table[SPRITE_SMOKE2]); /* Do Right Hand Smoke */
            }
            else
            {
                OSmoke_draw_ferrari_smoke(&osmoke, &osprites.jump_table[SPRITE_SMOKE1]); /* Do Left Hand Smoke */
                OSmoke_draw_ferrari_smoke(&osmoke, &osprites.jump_table[SPRITE_SMOKE2]); /* Do Right Hand Smoke */
            }
            break;
    }

    OSprites_sprite_copy(&osprites);

    /* Libretro force feedback uses the current steering position. */
    if (self->tick_frame && config.controls.haptic)
        OOutputs_tick(self->outputs, oinputs.input_steering);
}

/* Source: 0xB15E */
static void Outrun_main_switch(Outrun* self)
{
    switch (self->game_state)
    {
        case GS_INIT:  
            Outrun_init_attract(self);
            /* fall through */
            
        /* ---------------------------------------------------------------------------------------- */
        /* Attract Mode */
        /* ---------------------------------------------------------------------------------------- */
        case GS_ATTRACT:
            Outrun_tick_attract(self);
            break;

        case GS_INIT_BEST1:
            oferrari.car_ctrl_active = false;
            oinitengine.car_increment = 0;
            oferrari.car_inc_old = 0;
            ostats.time_counter = 5;
            ostats.frame_counter = frame_reset;
            OHiScore_init(&ohiscore);
            OSoundInt_queue_sound(&osoundint, SOUND_FM_RESET);
            Audio_clear_wav(&cannonball_audio);
            self->game_state = GS_BEST1;

        case GS_BEST1:
            OHud_draw_copyright_text(&ohud);
            OHiScore_display_scores(&ohiscore);
            OHud_draw_credits(&ohud);
            OHud_draw_insert_coin(&ohud);
            if (ostats.credits)
                self->game_state = GS_INIT_MUSIC;
            else if (Outrun_decrement_timers(self))
                self->game_state = GS_INIT_LOGO;
            break;

        case GS_INIT_LOGO:
            Video_clear_text_ram(&video);
            oferrari.car_ctrl_active = false;
            oinitengine.car_increment = 0;
            oferrari.car_inc_old = 0;
            ostats.time_counter = 5;
            ostats.frame_counter = frame_reset;
            OSoundInt_queue_sound(&osoundint, 0);
            OLogo_enable(&ologo, SOUND_FM_RESET);
            self->game_state = GS_LOGO;

        case GS_LOGO:
            OHud_draw_credits(&ohud);
            OHud_draw_copyright_text(&ohud);
            OHud_draw_insert_coin(&ohud);
            OLogo_tick(&ologo);

            if (ostats.credits)
                self->game_state = GS_INIT_MUSIC;
            else if (Outrun_decrement_timers(self))
            {
                OLogo_disable(&ologo);
                self->game_state = GS_INIT; /* Resume attract mode */
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Music Select Screen */
        /* ---------------------------------------------------------------------------------------- */

        case GS_INIT_MUSIC:
            OMusic_enable(&omusic);
            self->game_state = GS_MUSIC;

        case GS_MUSIC:
            OHud_draw_credits(&ohud);
            OHud_draw_insert_coin(&ohud);
            OMusic_tick(&omusic);
            if (Outrun_decrement_timers(self))
            {
                OMusic_disable(&omusic);
                self->game_state = GS_INIT_GAME;
            }
            break;
        /* ---------------------------------------------------------------------------------------- */
        /* In-Game */
        /* ---------------------------------------------------------------------------------------- */

        case GS_INIT_GAME:
            /*ROM:0000B3E8                 move.w  #-1,(ingame_active1).l              ; Denote in-game engine is active */
            /*ROM:0000B3F0                 clr.l   (prev_game_time).l                  ; Reset overall game time */
            /*ROM:0000B3F6                 move.w  #-1,(ingame_active2).l */
            Video_clear_text_ram(&video);
            oferrari.car_ctrl_active = true;
            Outrun_init_jump_table(self);
            OInitEngine_init(&oinitengine, self->cannonball_mode == MODE_TTRIAL ? self->ttrial.level : 0);
            /* Timing Hack to ensure horizon is correct */
            /* Note that the original code disables the screen, and waits for the second CPU's interrupt instead */
            ORoad_tick(&oroad);
            ORoad_tick(&oroad);
            ORoad_tick(&oroad);
            OSoundInt_queue_sound(&osoundint, SOUND_STOP_CHEERS);
            OSoundInt_queue_sound(&osoundint, SOUND_VOICE_GETREADY);
            OSoundInt_queue_sound(&osoundint, SOUND_REVS);             /* Moved from Z80 Code for extra flexibility */
            OMusic_play_music(&omusic, -1);
            
            if (!self->freeze_timer)
                ostats.time_counter = TIME[config.engine.dip_time * 40]; /* Set time to begin level with */
            else
                ostats.time_counter = 0x30;

            ostats.frame_counter = frame_reset + 50;
            ostats.credits--;                                   /* Update Credits */
            OHud_blit_text1(&ohud, TEXT1_CLEAR_START);
            OHud_blit_text1(&ohud, TEXT1_CLEAR_CREDITS);
            OSoundInt_queue_sound(&osoundint, SOUND_INIT_CHEERS);
            video.enabled = true;
            self->game_state = GS_START1;
            OHud_draw_main_hud(&ohud);
            /* fall through */

        /*  Start Game - Car Driving In */
        case GS_START1:
        case GS_START2:
            if (--ostats.frame_counter < 0)
            {
                OSoundInt_queue_sound(&osoundint, SOUND_SIGNAL1);
                ostats.frame_counter = frame_reset;
                self->game_state++;
            }
            break;

        case GS_START3:
            if (--ostats.frame_counter < 0)
            {
                if (self->cannonball_mode == MODE_TTRIAL)
                {
                    OHud_clear_timetrial_text(&ohud);
                }

                OSoundInt_queue_sound(&osoundint, SOUND_SIGNAL2);
                OSoundInt_queue_sound(&osoundint, SOUND_STOP_CHEERS);
                ostats.frame_counter = frame_reset;
                self->game_state++;
            }
            break;

        case GS_INGAME:
            if (Outrun_decrement_timers(self))
                self->game_state = GS_INIT_GAMEOVER;
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Bonus Mode */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_BONUS:
            ostats.frame_counter = frame_reset;
            obonus.bonus_control = BONUS_INIT;  /* Initialize Bonus Mode Logic */
            oroad.road_load_end   |= BIT_0;             /* Instruct CPU 1 to load end road section */
            ostats.game_completed |= BIT_0;             /* Denote game completed */
            obonus.bonus_timer = 3600;                  /* Safety Timer Added in Rev. A Roms */
            self->game_state = GS_BONUS;

        case GS_BONUS:
            if (--obonus.bonus_timer < 0)
            {
                obonus.bonus_control = BONUS_DISABLE;
                self->game_state = GS_INIT_GAMEOVER;
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Display Game Over Text */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_GAMEOVER:
            if (self->cannonball_mode != MODE_TTRIAL)
            {
                oferrari.car_ctrl_active = false; /* -1 */
                oinitengine.car_increment = 0;
                oferrari.car_inc_old = 0;
                ostats.time_counter = 3;
                ostats.frame_counter = frame_reset;
                OHud_blit_text2(&ohud, TEXT2_GAMEOVER);
            }
            else
            {
                OHud_blit_text_big(&ohud, 7, self->ttrial.new_high_score ? "NEW RECORD" : "BAD LUCK", false);

                OHud_blit_text1(&ohud, TEXT1_LAPTIME1);
                OHud_blit_text1(&ohud, TEXT1_LAPTIME2);
                OHud_draw_lap_timer(&ohud, 0x110554, self->ttrial.best_lap, self->ttrial.best_lap[2]);

                OHud_blit_text_new(&ohud, 9,  14, "OVERTAKES          - ", GREY);
                OHud_blit_text_new(&ohud, 31, 14, Utils_to_string((int) self->ttrial.overtakes), GREEN);
                OHud_blit_text_new(&ohud, 9,  16, "VEHICLE COLLISIONS - ", GREY);
                OHud_blit_text_new(&ohud, 31, 16, Utils_to_string((int) self->ttrial.vehicle_cols), GREEN);
                OHud_blit_text_new(&ohud, 9,  18, "CRASHES            - ", GREY);
                OHud_blit_text_new(&ohud, 31, 18, Utils_to_string((int) self->ttrial.crashes), GREEN);
            }
            OSoundInt_queue_sound(&osoundint, SOUND_NEW_COMMAND);
            self->game_state = GS_GAMEOVER;

        case GS_GAMEOVER:
            if (self->cannonball_mode == MODE_ORIGINAL)
            {
                if (Outrun_decrement_timers(self))
                    self->game_state = GS_INIT_MAP;
            }
            else if (self->cannonball_mode == MODE_CONT)
            {
                if (Outrun_decrement_timers(self))
                    Outrun_init_best_outrunners(self);
            }
            else if (self->cannonball_mode == MODE_TTRIAL)
            {
                if (outrun.tick_counter & BIT_4)
                    OHud_blit_text1_xy(&ohud, 10, 20, TEXT1_PRESS_START);
                else
                    OHud_blit_text1_xy(&ohud, 10, 20, TEXT1_CLEAR_START);

                if (Input_is_pressed(&input, START))
                    cannonball_state = STATE_INIT_MENU;
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Display Course Map */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_MAP:
            OMap_init(&omap);
            OHud_blit_text2(&ohud, TEXT2_COURSEMAP);
            self->game_state = GS_MAP;
            /* fall through */

        case GS_MAP:
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Best OutRunners / Score Entry */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_BEST2:
            ORoad_set_view_mode(&oroad, VIEW_ORIGINAL, true);
            /* bsr.w   EndGame */
            OSprites_disable_sprites(&osprites);
            OTraffic_disable_traffic(&otraffic);
            /* bsr.w   EditJumpTable3 */
            OSprites_clear_palette_data(&osprites);
            OLevelObjs_init_hiscore_sprites(&olevelobjs);
            ocrash.coll_count1   = 0;
            ocrash.coll_count2   = 0;
            ocrash.crash_counter = 0;
            ocrash.skid_counter  = 0;
            ocrash.spin_control1 = 0;
            oferrari.car_ctrl_active = false; /* -1 */
            oinitengine.car_increment = 0;
            oferrari.car_inc_old = 0;
            ostats.time_counter = config.engine.hiscore_timer;
            ostats.frame_counter = frame_reset;
            OHiScore_init(&ohiscore);
            OSoundInt_queue_sound(&osoundint, SOUND_NEW_COMMAND);
            OSoundInt_queue_sound(&osoundint, SOUND_FM_RESET);
            Audio_clear_wav(&cannonball_audio);
            self->game_state = GS_BEST2;
            /* fall through */

        case GS_BEST2:
            OHiScore_tick(&ohiscore); /* Do High Score Logic */
            OHud_draw_credits(&ohud);

            /* If countdown has expired */
            if (Outrun_decrement_timers(self))
            {
                /*ROM:0000B700                 bclr    #5,(ppi1_value).l                   ; Turn screen off (not activated until PPI written to) */
                oferrari.car_ctrl_active = true; /* 0 : Allow road updates */
                Outrun_init_jump_table(self);
                OInitEngine_init(&oinitengine, self->cannonball_mode == MODE_TTRIAL ? self->ttrial.level : 0);
                /*ROM:0000B716                 bclr    #0,(byte_260550).l */
                self->game_state = GS_REINIT;          /* Reinit game to attract mode */
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Reinitialize Game After High Score Entry */
        /* ---------------------------------------------------------------------------------------- */
        case GS_REINIT:
            Video_clear_text_ram(&video);
            self->game_state = GS_INIT;
            break;
    }

    OInitEngine_update_road(&oinitengine);
    OInitEngine_update_engine(&oinitengine);

    /* -------------------------------------------------------------------------------------------- */
    /* NOTE: Here used to live a "Debugging Only" road-fork chooser guarded by #ifndef NDEBUG.       */
    /* Its unconditional else branch overwrote car_x_pos / oroad.car_x_bak with road_width_bak every */
    /* frame during normal driving, wiping out the player's steering (the car sprite still leaned    */
    /* because steering_adjust was untouched, but the car never moved laterally). That only bit      */
    /* Debug builds (NDEBUG undefined), so Release/RetroArch appeared fine. Removed so gameplay is    */
    /* identical in Debug and Release. If the dev fork-selector is ever needed again, gate it behind */
    /* a dedicated opt-in macro and only touch car_x_pos when fork_chosen != 0.                      */
    /* -------------------------------------------------------------------------------------------- */
}

/* Setup Jump Table. Move from ROM to RAM. */
/* */
/* Source Address: 0x7E1C */
/* Input:          Sprite To Copy */
/* Output:         None */
/* */
/* ROM Format [0xF000 - 0xF1F5] */
/* */
/* Word 1: Number of entries [7D] */
/* Long 1: Address 1 (address of jump information) */
/* ... */
/* Long x: Address x */
/* */
/* Each address in the jump table is a pointer into ROM containing 0x1F words  */
/* of info (so info is at 0x40 boundary in bytes) */
/* */
/* RAM Format[0x61800] */
/* */
/* 0x00 byte: If high byte set, take jump */
/* 0x01 byte: Index number */
/* 0x02 long: Address to jump to */
static void Outrun_init_jump_table(Outrun* self)
{
    /* Reset value to restore car increment to during attract mode */
    self->car_inc_bak = 0;

    OSprites_init(&osprites);
    if (self->cannonball_mode != MODE_TTRIAL) 
    {
        OTraffic_init_stage1_traffic(&otraffic);      /* Hard coded traffic in right hand lane */
        if (trackloader.display_start_line)
            OLevelObjs_init_startline_sprites(&olevelobjs); /* Hard coded start line sprites (not part of level data) */
    }
    else if (trackloader.display_start_line)
        OLevelObjs_init_timetrial_sprites(&olevelobjs);

    OTraffic_init(&otraffic);
    OSmoke_init(&osmoke);
    ORoad_init(&oroad);
    OTiles_init(&otiles);
    OPalette_init(&opalette);
    OInputs_init(&oinputs);
    OBonus_init(&obonus);
    OOutputs_init(self->outputs);

    hwtiles_set_x_clamp(video.tile_layer, CLAMP_RIGHT);
    hwsprites_set_x_clip(video.sprite_layer, false);
}

/* ------------------------------------------------------------------------------- */
/* Decrement Game Time */
/*  */
/* Decrements Frame Count, and Overall Time Counter */
/* */
/* Returns true if timer expired. */
/* Source: 0xB736 */
/* ------------------------------------------------------------------------------- */
static bool Outrun_decrement_timers(Outrun* self)
{
    /* Cheat */
    if (self->freeze_timer && self->game_state == GS_INGAME)
        return false;

    /* Correct count-down timer running fast at 1/29th (3%) */
    /* Fix timer counting extra second */
    if (config.engine.fix_timer)
    {
        if (--ostats.frame_counter > 0)
            return false;

        ostats.frame_counter = frame_reset;
        ostats.time_counter  = outils_bcd_sub(1, ostats.time_counter);

        /* We need to manually refresh the HUD here to display '0' seconds */
        if (ostats.time_counter == 0)
            OHud_draw_timer1(&ohud, 0);

        return (ostats.time_counter == 0);
    }
    else
    {
        if (--ostats.frame_counter >= 0)
            return false;

        ostats.frame_counter = frame_reset;
        ostats.time_counter  = outils_bcd_sub(1, ostats.time_counter);
        return (ostats.time_counter < 0);
    }
}

/* ------------------------------------------------------------------------------- */
/* Motor calibration */
/* ------------------------------------------------------------------------------- */

static void Outrun_init_motor_calibration(Outrun* self)
{
    const static uint32_t PAL_SERVICE[] = {0xFF, 0xFF00FF, 0xFF00FF, 0xFF0000};
    uint32_t dst;
    OTiles_init(&otiles);
    OPalette_init(&opalette);
    OInputs_init(&oinputs);
    OOutputs_init(self->outputs);

    hwtiles_set_x_clamp(video.tile_layer, CLAMP_RIGHT);
    hwsprites_set_x_clip(video.sprite_layer, false);

    OTiles_fill_tilemap_color(&otiles, 0x4F60); /* Fill Tilemap Light Blue */

    video.enabled        = true;
    osoundint.has_booted = true;

    ORoad_init(&oroad);
    oroad.horizon_set    = 1;
    oroad.horizon_base   = HORIZON_OFF;
    self->game_state           = GS_CALIBRATE_MOTOR;


    /* Write Palette To RAM */
    dst = 0x120000;
    Video_write_pal32_a(&video, &dst, PAL_SERVICE[0]);
    Video_write_pal32_a(&video, &dst, PAL_SERVICE[1]);
    Video_write_pal32_a(&video, &dst, PAL_SERVICE[2]);
    Video_write_pal32_a(&video, &dst, PAL_SERVICE[3]);
}

/* ------------------------------------------------------------------------------- */
/* Attract Mode Control */
/* ------------------------------------------------------------------------------- */

static void Outrun_init_attract(Outrun* self)
{
    video.enabled             = true;
    osoundint.has_booted      = true;
    oferrari.car_ctrl_active  = true;
    oferrari.car_inc_old      = self->car_inc_bak >> 16;
    oinitengine.car_increment = self->car_inc_bak;
    ostats.time_counter       = config.engine.new_attract ? 0x80 : 0x15;
    ostats.frame_counter      = frame_reset;
    self->attract_counter           = 0;
    self->attract_view              = 0;
    OAttractAI_init(&oattractai);
    self->game_state = self->cannonball_mode == MODE_TTRIAL ? GS_INIT_MUSIC : GS_ATTRACT;
}

static void Outrun_tick_attract(Outrun* self)
{
    OHud_draw_credits(&ohud);
    OHud_draw_copyright_text(&ohud);
    OHud_draw_insert_coin(&ohud);

    /* Enhanced Attract Mode (Switch Between Views) */
    if (config.engine.new_attract)
    {
        if (++self->attract_counter > 240)
        {
            const static uint8_t VIEWS[] = {VIEW_ORIGINAL, VIEW_ELEVATED, VIEW_INCAR};

            self->attract_counter = 0;
            if (++self->attract_view > 2)
                self->attract_view = 0;
            { bool snap = VIEWS[self->attract_view] == VIEW_INCAR;
            ORoad_set_view_mode(&oroad, VIEWS[self->attract_view], snap);
         }}
    }

    if (ostats.credits)
        self->game_state = GS_INIT_MUSIC;

    else if (Outrun_decrement_timers(self))
    {
        self->car_inc_bak = oinitengine.car_increment;
        self->game_state = GS_INIT_BEST1;
    }
}

static void Outrun_check_freeplay_start(Outrun* self)
{
    if (config.engine.freeplay)
    {
        if (!ostats.credits && Input_has_pressed(&input, START))
        {
            ostats.credits = 1;
        }
    }
}

/* ------------------------------------------------------------------------------- */
/* Best OutRunners Initialization */
/* ------------------------------------------------------------------------------- */

void Outrun_init_best_outrunners(Outrun* self)
{
    video.enabled = false;
    hwsprites_set_x_clip(video.sprite_layer, false); /* Stop clipping in wide-screen mode. */
    OTiles_fill_tilemap_color(&otiles, 0); /* Fill Tilemap Black */
    OSprites_disable_sprites(&osprites);
    oroad.horizon_base = 0x154;
    OHiScore_setup_pal_best(&ohiscore);    /* Setup Palettes */
    OHiScore_setup_road_best(&ohiscore);
    self->game_state = GS_INIT_BEST2;
}

/* ------------------------------------------------------------------------------- */
/* Remap ROM addresses and select course. */
/* ------------------------------------------------------------------------------- */

void Outrun_select_course(Outrun* self, bool jap, bool prototype)
{
    if (jap)
    {
        roms.rom0p = &roms.j_rom0;
        roms.rom1p = &roms.j_rom1;

        /* Main CPU */
        self->adr.tiles_def_lookup      = TILES_DEF_LOOKUP_J;
        self->adr.tiles_table           = TILES_TABLE_J;
        self->adr.sprite_master_table   = SPRITE_MASTER_TABLE_J;
        self->adr.sprite_type_table     = SPRITE_TYPE_TABLE_J;
        self->adr.sprite_def_props1     = SPRITE_DEF_PROPS1_J;
        self->adr.sprite_def_props2     = SPRITE_DEF_PROPS2_J;
        self->adr.sprite_cloud          = SPRITE_CLOUD_FRAMES_J;
        self->adr.sprite_minitree       = SPRITE_MINITREE_FRAMES_J;
        self->adr.sprite_grass          = SPRITE_GRASS_FRAMES_J;
        self->adr.sprite_sand           = SPRITE_SAND_FRAMES_J;
        self->adr.sprite_stone          = SPRITE_STONE_FRAMES_J;
        self->adr.sprite_water          = SPRITE_WATER_FRAMES_J;
        self->adr.sprite_ferrari_frames = SPRITE_FERRARI_FRAMES_J;
        self->adr.sprite_skid_frames    = SPRITE_SKID_FRAMES_J;
        self->adr.sprite_pass_frames    = SPRITE_PASS_FRAMES_J;
        self->adr.sprite_pass1_skidl    = SPRITE_PASS1_SKIDL_J;
        self->adr.sprite_pass1_skidr    = SPRITE_PASS1_SKIDR_J;
        self->adr.sprite_pass2_skidl    = SPRITE_PASS2_SKIDL_J;
        self->adr.sprite_pass2_skidr    = SPRITE_PASS2_SKIDR_J;
        self->adr.sprite_crash_spin1    = SPRITE_CRASH_SPIN1_J;
        self->adr.sprite_crash_spin2    = SPRITE_CRASH_SPIN2_J;
        self->adr.sprite_bump_data1     = SPRITE_BUMP_DATA1_J;
        self->adr.sprite_bump_data2     = SPRITE_BUMP_DATA2_J;
        self->adr.sprite_crash_man1     = SPRITE_CRASH_MAN1_J;
        self->adr.sprite_crash_girl1    = SPRITE_CRASH_GIRL1_J;
        self->adr.sprite_crash_flip     = SPRITE_CRASH_FLIP_J;
        self->adr.sprite_crash_flip_m1  = SPRITE_CRASH_FLIP_MAN1_J;
        self->adr.sprite_crash_flip_g1  = SPRITE_CRASH_FLIP_GIRL1_J;
        self->adr.sprite_crash_flip_m2  = SPRITE_CRASH_FLIP_MAN2_J;
        self->adr.sprite_crash_flip_g2  = SPRITE_CRASH_FLIP_GIRL2_J;
        self->adr.sprite_crash_man2     = SPRITE_CRASH_MAN2_J;
        self->adr.sprite_crash_girl2    = SPRITE_CRASH_GIRL2_J;
        self->adr.smoke_data            = SMOKE_DATA_J;
        self->adr.spray_data            = SPRAY_DATA_J;
        self->adr.anim_ferrari_frames   = ANIM_FERRARI_FRAMES_J;
        self->adr.anim_endseq_obj1      = ANIM_ENDSEQ_OBJ1_J;
        self->adr.anim_endseq_obj2      = ANIM_ENDSEQ_OBJ2_J;
        self->adr.anim_endseq_obj3      = ANIM_ENDSEQ_OBJ3_J;
        self->adr.anim_endseq_obj4      = ANIM_ENDSEQ_OBJ4_J;
        self->adr.anim_endseq_obj5      = ANIM_ENDSEQ_OBJ5_J;
        self->adr.anim_endseq_obj6      = ANIM_ENDSEQ_OBJ6_J;
        self->adr.anim_endseq_obj7      = ANIM_ENDSEQ_OBJ7_J;
        self->adr.anim_endseq_obj8      = ANIM_ENDSEQ_OBJ8_J;
        self->adr.anim_endseq_objA      = ANIM_ENDSEQ_OBJA_J;
        self->adr.anim_endseq_objB      = ANIM_ENDSEQ_OBJB_J;
        self->adr.anim_end_table        = ANIM_END_TABLE_J;
        self->adr.shadow_data           = SPRITE_SHADOW_DATA_J;
        self->adr.shadow_frames         = SPRITE_SHDW_FRAMES_J;
        self->adr.sprite_shadow_small   = SPRITE_SHDW_SMALL_J;
        self->adr.sprite_logo_bg        = SPRITE_LOGO_BG_J;
        self->adr.sprite_logo_car       = SPRITE_LOGO_CAR_J;
        self->adr.sprite_logo_bird1     = SPRITE_LOGO_BIRD1_J;
        self->adr.sprite_logo_bird2     = SPRITE_LOGO_BIRD2_J;
        self->adr.sprite_logo_base      = SPRITE_LOGO_BASE_J;
        self->adr.sprite_logo_text      = SPRITE_LOGO_TEXT_J;
        self->adr.sprite_logo_palm1     = SPRITE_LOGO_PALM1_J;
        self->adr.sprite_logo_palm2     = SPRITE_LOGO_PALM2_J;
        self->adr.sprite_logo_palm3     = SPRITE_LOGO_PALM3_J;
        self->adr.sprite_fm_left        = SPRITE_FM_LEFT_J;
        self->adr.sprite_fm_centre      = SPRITE_FM_CENTRE_J;
        self->adr.sprite_fm_right       = SPRITE_FM_RIGHT_J;
        self->adr.sprite_dial_left      = SPRITE_DIAL_LEFT_J;
        self->adr.sprite_dial_centre    = SPRITE_DIAL_CENTRE_J;
        self->adr.sprite_dial_right     = SPRITE_DIAL_RIGHT_J;
        self->adr.sprite_eq             = SPRITE_EQ_J;
        self->adr.sprite_radio          = SPRITE_RADIO_J;
        self->adr.sprite_hand_left      = SPRITE_HAND_LEFT_J;
        self->adr.sprite_hand_centre    = SPRITE_HAND_CENTRE_J;
        self->adr.sprite_hand_right     = SPRITE_HAND_RIGHT_J;
        self->adr.sprite_coursemap_top  = SPRITE_COURSEMAP_TOP_J;
        self->adr.sprite_coursemap_bot  = SPRITE_COURSEMAP_BOT_J;
        self->adr.sprite_coursemap_end  = SPRITE_COURSEMAP_END_J;
        self->adr.sprite_minicar_right  = SPRITE_MINICAR_RIGHT_J;
        self->adr.sprite_minicar_up     = SPRITE_MINICAR_UP_J;
        self->adr.sprite_minicar_down   = SPRITE_MINICAR_DOWN_J;
        self->adr.anim_seq_flag         = ANIM_SEQ_FLAG_J;
        self->adr.anim_ferrari_curr     = ANIM_FERRARI_CURR_J;
        self->adr.anim_ferrari_next     = ANIM_FERRARI_NEXT_J;
        self->adr.anim_pass1_curr       = ANIM_PASS1_CURR_J;
        self->adr.anim_pass1_next       = ANIM_PASS1_NEXT_J;
        self->adr.anim_pass2_curr       = ANIM_PASS2_CURR_J;
        self->adr.anim_pass2_next       = ANIM_PASS2_NEXT_J;
        self->adr.traffic_props         = TRAFFIC_PROPS_J;
        self->adr.traffic_data          = TRAFFIC_DATA_J;
        self->adr.sprite_porsche        = SPRITE_PORSCHE_J;
        self->adr.sprite_coursemap      = SPRITE_COURSEMAP_J;
        self->adr.road_seg_table        = ROAD_SEG_TABLE_J;
        self->adr.road_seg_end          = ROAD_SEG_TABLE_END_J;
        self->adr.road_seg_split        = ROAD_SEG_TABLE_SPLIT_J;

        /* Sub CPU */
        self->adr.road_height_lookup    = ROAD_HEIGHT_LOOKUP_J;
    }
    else
    {
        roms.rom0p = &roms.rom0;
        roms.rom1p = &roms.rom1;

        /* Main CPU */
        self->adr.tiles_def_lookup      = TILES_DEF_LOOKUP;
        self->adr.tiles_table           = TILES_TABLE;
        self->adr.sprite_master_table   = SPRITE_MASTER_TABLE;
        self->adr.sprite_type_table     = SPRITE_TYPE_TABLE;
        self->adr.sprite_def_props1     = SPRITE_DEF_PROPS1;
        self->adr.sprite_def_props2     = SPRITE_DEF_PROPS2;
        self->adr.sprite_cloud          = SPRITE_CLOUD_FRAMES;
        self->adr.sprite_minitree       = SPRITE_MINITREE_FRAMES;
        self->adr.sprite_grass          = SPRITE_GRASS_FRAMES;
        self->adr.sprite_sand           = SPRITE_SAND_FRAMES;
        self->adr.sprite_stone          = SPRITE_STONE_FRAMES;
        self->adr.sprite_water          = SPRITE_WATER_FRAMES;
        self->adr.sprite_ferrari_frames = SPRITE_FERRARI_FRAMES;
        self->adr.sprite_skid_frames    = SPRITE_SKID_FRAMES;
        self->adr.sprite_pass_frames    = SPRITE_PASS_FRAMES;
        self->adr.sprite_pass1_skidl    = SPRITE_PASS1_SKIDL;
        self->adr.sprite_pass1_skidr    = SPRITE_PASS1_SKIDR;
        self->adr.sprite_pass2_skidl    = SPRITE_PASS2_SKIDL;
        self->adr.sprite_pass2_skidr    = SPRITE_PASS2_SKIDR;
        self->adr.sprite_crash_spin1    = SPRITE_CRASH_SPIN1;
        self->adr.sprite_crash_spin2    = SPRITE_CRASH_SPIN2;
        self->adr.sprite_bump_data1     = SPRITE_BUMP_DATA1;
        self->adr.sprite_bump_data2     = SPRITE_BUMP_DATA2;
        self->adr.sprite_crash_man1     = SPRITE_CRASH_MAN1;
        self->adr.sprite_crash_girl1    = SPRITE_CRASH_GIRL1;
        self->adr.sprite_crash_flip     = SPRITE_CRASH_FLIP;
        self->adr.sprite_crash_flip_m1  = SPRITE_CRASH_FLIP_MAN1;
        self->adr.sprite_crash_flip_g1  = SPRITE_CRASH_FLIP_GIRL1;
        self->adr.sprite_crash_flip_m2  = SPRITE_CRASH_FLIP_MAN2;
        self->adr.sprite_crash_flip_g2  = SPRITE_CRASH_FLIP_GIRL2;
        self->adr.sprite_crash_man2     = SPRITE_CRASH_MAN2;
        self->adr.sprite_crash_girl2    = SPRITE_CRASH_GIRL2;
        self->adr.smoke_data            = SMOKE_DATA;
        self->adr.spray_data            = SPRAY_DATA;
        self->adr.shadow_data           = SPRITE_SHADOW_DATA;
        self->adr.shadow_frames         = SPRITE_SHDW_FRAMES;
        self->adr.sprite_shadow_small   = SPRITE_SHDW_SMALL;
        self->adr.sprite_logo_bg        = SPRITE_LOGO_BG;
        self->adr.sprite_logo_car       = SPRITE_LOGO_CAR;
        self->adr.sprite_logo_bird1     = SPRITE_LOGO_BIRD1;
        self->adr.sprite_logo_bird2     = SPRITE_LOGO_BIRD2;
        self->adr.sprite_logo_base      = SPRITE_LOGO_BASE;
        self->adr.sprite_logo_text      = SPRITE_LOGO_TEXT;
        self->adr.sprite_logo_palm1     = SPRITE_LOGO_PALM1;
        self->adr.sprite_logo_palm2     = SPRITE_LOGO_PALM2;
        self->adr.sprite_logo_palm3     = SPRITE_LOGO_PALM3;
        self->adr.sprite_fm_left        = SPRITE_FM_LEFT;
        self->adr.sprite_fm_centre      = SPRITE_FM_CENTRE;
        self->adr.sprite_fm_right       = SPRITE_FM_RIGHT;
        self->adr.sprite_dial_left      = SPRITE_DIAL_LEFT;
        self->adr.sprite_dial_centre    = SPRITE_DIAL_CENTRE;
        self->adr.sprite_dial_right     = SPRITE_DIAL_RIGHT;
        self->adr.sprite_eq             = SPRITE_EQ;
        self->adr.sprite_radio          = SPRITE_RADIO;
        self->adr.sprite_hand_left      = SPRITE_HAND_LEFT;
        self->adr.sprite_hand_centre    = SPRITE_HAND_CENTRE;
        self->adr.sprite_hand_right     = SPRITE_HAND_RIGHT;
        self->adr.sprite_coursemap_top  = SPRITE_COURSEMAP_TOP;
        self->adr.sprite_coursemap_bot  = SPRITE_COURSEMAP_BOT;
        self->adr.sprite_coursemap_end  = SPRITE_COURSEMAP_END;
        self->adr.sprite_minicar_right  = SPRITE_MINICAR_RIGHT;
        self->adr.sprite_minicar_up     = SPRITE_MINICAR_UP;
        self->adr.sprite_minicar_down   = SPRITE_MINICAR_DOWN;
        self->adr.anim_seq_flag         = ANIM_SEQ_FLAG;
        self->adr.anim_ferrari_curr     = ANIM_FERRARI_CURR;
        self->adr.anim_ferrari_next     = ANIM_FERRARI_NEXT;
        self->adr.anim_pass1_curr       = ANIM_PASS1_CURR;
        self->adr.anim_pass1_next       = ANIM_PASS1_NEXT;
        self->adr.anim_pass2_curr       = ANIM_PASS2_CURR;
        self->adr.anim_pass2_next       = ANIM_PASS2_NEXT;
        self->adr.anim_ferrari_frames   = ANIM_FERRARI_FRAMES;
        self->adr.anim_endseq_obj1      = ANIM_ENDSEQ_OBJ1;
        self->adr.anim_endseq_obj2      = ANIM_ENDSEQ_OBJ2;
        self->adr.anim_endseq_obj3      = ANIM_ENDSEQ_OBJ3;
        self->adr.anim_endseq_obj4      = ANIM_ENDSEQ_OBJ4;
        self->adr.anim_endseq_obj5      = ANIM_ENDSEQ_OBJ5;
        self->adr.anim_endseq_obj6      = ANIM_ENDSEQ_OBJ6;
        self->adr.anim_endseq_obj7      = ANIM_ENDSEQ_OBJ7;
        self->adr.anim_endseq_obj8      = ANIM_ENDSEQ_OBJ8;
        self->adr.anim_endseq_objA      = ANIM_ENDSEQ_OBJA;
        self->adr.anim_endseq_objB      = ANIM_ENDSEQ_OBJB;
        self->adr.anim_end_table        = ANIM_END_TABLE;
        self->adr.shadow_data           = SPRITE_SHADOW_DATA;
        self->adr.shadow_frames         = SPRITE_SHDW_FRAMES;
        self->adr.sprite_shadow_small   = SPRITE_SHDW_SMALL;
        self->adr.traffic_props         = TRAFFIC_PROPS;
        self->adr.traffic_data          = TRAFFIC_DATA;
        self->adr.sprite_porsche        = SPRITE_PORSCHE;
        self->adr.sprite_coursemap      = SPRITE_COURSEMAP;
        self->adr.road_seg_table        = ROAD_SEG_TABLE;
        self->adr.road_seg_end          = ROAD_SEG_TABLE_END;
        self->adr.road_seg_split        = ROAD_SEG_TABLE_SPLIT;

        /* Sub CPU */
        self->adr.road_height_lookup    = ROAD_HEIGHT_LOOKUP;
    }

    TrackLoader_init(&trackloader, jap);

    /* Use Prototype Coconut Beach Track */
    trackloader.stage_data[0] = prototype ? 0x3A : 0x3C;
}
