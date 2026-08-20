/***************************************************************************
    In-Game Statistics.
    - Stage Timers
    - Route Info
    - Speed to Score Conversion
    - Bonus Time Increment
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "engine/ohud.h"
#include "engine/omusic.h"
#include "engine/outils.h"
#include "engine/ostats.h"
#include "engine/otraffic.h"

static void OStats_inc_lap_timer(OStats* self);

OStats ostats;

/* Original buggy millisecond lookup table (Used when 64 frames = 1 second) */
/* Conversion table from 0 to 64 -> Millisecond value */
const static uint8_t LAP_MS_64[] = 
{
    0x00, 0x01, 0x03, 0x04, 0x06, 0x07, 0x09, 0x10, 0x12, 0x14, 0x15, 0x17, 0x18, 0x20, 0x21, 0x23,
    0x25, 0x26, 0x28, 0x29, 0x31, 0x32, 0x34, 0x35, 0x37, 0x39, 0x40, 0x42, 0x43, 0x45, 0x46, 0x48,
    0x50, 0x51, 0x53, 0x54, 0x56, 0x57, 0x59, 0x60, 0x62, 0x64, 0x65, 0x67, 0x68, 0x70, 0x71, 0x73,
    0x75, 0x76, 0x78, 0x79, 0x81, 0x82, 0x84, 0x85, 0x87, 0x89, 0x90, 0x92, 0x93, 0x95, 0x96, 0x98,
};

/* Bug fixed millisecond lookup table  (Used when 60 frames = 1 second) */
/* Conversion table from 0 to 60 -> Millisecond value */
const static uint8_t LAP_MS_60[] = 
{
    0x00, 0x01, 0x03, 0x05, 0x06, 0x08, 0x10, 0x11, 0x13, 0x15, 0x16, 0x18, 0x20, 0x21, 0x23, 0x25,
    0x26, 0x28, 0x30, 0x31, 0x33, 0x35, 0x36, 0x38, 0x40, 0x41, 0x43, 0x45, 0x46, 0x48,
    0x50, 0x51, 0x53, 0x55, 0x56, 0x58, 0x60, 0x61, 0x63, 0x65, 0x66, 0x68, 0x70, 0x71, 0x73, 0x75,
    0x76, 0x78, 0x80, 0x81, 0x83, 0x85, 0x86, 0x88, 0x90, 0x91, 0x93, 0x95, 0x96, 0x98
};



void OStats_init(OStats* self, bool ttrial)
{
    self->credits = ttrial ? 1 : 0;
    /* Choose correct lookup table if timing bugs fixed */
    self->lap_ms = config.engine.fix_timer ? LAP_MS_60 : LAP_MS_64;
}

void OStats_clear_stage_times(OStats* self)
{
    { int i; for (i = 0; i < 15; i++)
    {
        self->stage_counters[i] = 0;

        { int j; for (j = 0; j < 3; j++)
            self->stage_times[i][j] = 0; }
    } }
}

void OStats_clear_route_info(OStats* self)
{
    self->route_info = 0;
    self->routes[0] = self->routes[1] = self->routes[2] = self->routes[3] = 
    self->routes[4] = self->routes[5] = self->routes[6] = self->routes[7] = 0;
}

/* Increment Counters, Stage Timers & Print Stage Timers */
/* */
/* Source: 0x7F12 */
void OStats_do_timers(OStats* self)
{
    if (outrun.game_state != GS_INGAME) return;

    OStats_inc_lap_timer(self);

    if (outrun.cannonball_mode == MODE_ORIGINAL || outrun.cannonball_mode == MODE_CONT)
    {
        /* Each stage has a standard counter that just increments. Do this here. */
        self->stage_counters[self->cur_stage]++;
        OHud_draw_lap_timer(&ohud, 0x11016C, self->stage_times[self->cur_stage], self->ms_value);
    }

    else if (outrun.cannonball_mode == MODE_TTRIAL)
    {
        self->stage_counters[outrun.ttrial.current_lap]++;
        OHud_draw_stage_number(&ohud, OHud_translate(&ohud, 30, 2 + outrun.ttrial.current_lap, 0x110030), (outrun.ttrial.current_lap + 1), GREY);
        OHud_draw_lap_timer(&ohud, OHud_translate(&ohud, 32, 2 + outrun.ttrial.current_lap, 0x110030), self->stage_times[self->cur_stage], self->ms_value);
    }
}

/* Increment and store lap timer for each stage. */
/* */
/* Source: 0x7F4C */static 
void OStats_inc_lap_timer(OStats* self)
{
    /* Add MS (Not actual milliseconds, as these are looked up from the table below) */
    if (++self->stage_times[self->cur_stage][2] >= (config.engine.fix_timer ? 0x3C : 0x40))
    {
        /* Looped MS, so add a second */
        self->stage_times[self->cur_stage][2] = 0;
        self->stage_times[self->cur_stage][1] = outils_bcd_add(self->stage_times[self->cur_stage][1], 1);

        /* Loop seconds, so add a minute */
        if (self->stage_times[self->cur_stage][1] == 0x60)
        {
            self->stage_times[self->cur_stage][1] = 0;
            self->stage_times[self->cur_stage][0] = outils_bcd_add(self->stage_times[self->cur_stage][0], 1);
        }
    }

    /* Get MS Value */
    self->ms_value = self->lap_ms[self->stage_times[self->cur_stage][2]];
}

/* Source: 0xBE4E */
void OStats_convert_speed_score(OStats* self, uint16_t speed)
{
    /* 0x960 is the last value in this table to be actively used */
    static const uint16_t CONVERT[] = 
    {
        0x0,   0x10,  0x20,  0x30,  0x40,  0x50,  0x60,  0x80,  0x110, 0x150,
        0x200, 0x260, 0x330, 0x410, 0x500, 0x600, 0x710, 0x830, 0x960, 0x1100,
        0x1250,
    };

    { uint16_t score = CONVERT[(speed >> 4)];
    OStats_update_score(self, score);
 }}

/* Update In-Game Score. Adds Value To Overall Score. */
/* */
/* Source: 0x7340 */
void OStats_update_score(OStats* self, uint32_t value)
{
    if (outrun.cannonball_mode == MODE_TTRIAL)
        return;

    self->score = outils_bcd_add(value, self->score);

    if (self->score > 0x99999999)
        self->score = 0x99999999;

    OHud_draw_score_ingame(&ohud, self->score);
}

/* Initialize Next Level */
/* */
/* In-Game Only: */
/* */
/* 1/ Show Extend Play Timer */
/* 2/ Add correct time extend for time adjustment setting from dips */
/* 3/ Setup next level with relevant number of enemies */
/* 4/ Blit some info to the screen */
/* */
/* Source: 0x8FAC */

void OStats_init_next_level(OStats* self)
{
    if (self->extend_play_timer)
    {
        /* End Extend Play: Clear Text From Screen */
        if (--self->extend_play_timer <= 0)
        {
            OHud_blit_text1(&ohud, TEXT1_EXTEND_CLEAR1);
            OHud_blit_text1(&ohud, TEXT1_EXTEND_CLEAR2);
            OHud_blit_text1(&ohud, TEXT1_LAPTIME_CLEAR1);
            OHud_blit_text1(&ohud, TEXT1_LAPTIME_CLEAR2);
        }
        /* Extend Play: Flash Text */
        else
        {
            int16_t do_blit = ((self->extend_play_timer - 1) ^ self->extend_play_timer) & BIT_3;

            if (do_blit)
            {
                if (self->extend_play_timer & BIT_3)
                {
                    if (outrun.cannonball_mode == MODE_TTRIAL)
                        OHud_blit_text_new(&ohud, 15, 8, "BEST LAP!", PINK);
                    else
                    {
                        OHud_blit_text1(&ohud, TEXT1_EXTEND1);
                        OHud_blit_text1(&ohud, TEXT1_EXTEND2);
                    }
                }
                else
                {
                    OHud_blit_text1(&ohud, TEXT1_EXTEND_CLEAR1);
                    OHud_blit_text1(&ohud, TEXT1_EXTEND_CLEAR2);
                }
            }
        }
    }
    else if (outrun.game_state == GS_INGAME && oinitengine.checkpoint_marker)
    {
        uint16_t time_lookup;
        oinitengine.checkpoint_marker = 0;
        self->extend_play_timer             = 0x80;
        
        /* Calculate Time To Add */
        time_lookup = (config.engine.dip_time * 40) + oroad.stage_lookup_off;
        if (!outrun.freeze_timer)
        {
            if (outrun.cannonball_mode == MODE_ORIGINAL)
                self->time_counter = outils_bcd_add(self->time_counter, TIME[time_lookup]);
            else if (outrun.cannonball_mode == MODE_CONT)
                self->time_counter = outils_bcd_add(self->time_counter, 0x55);

            if (self->time_counter > 0x99) self->time_counter = 0x99;
        }

        /* Draw last laptime */
        /* Note there is a bug in the original code here, where the current ms value is displayed, instead of the ms value from the last lap time */
        OHud_blit_text1(&ohud, TEXT1_LAPTIME1);
        OHud_blit_text1(&ohud, TEXT1_LAPTIME2);
        OHud_draw_lap_timer(&ohud, 0x110554, self->stage_times[self->cur_stage-1], config.engine.fix_bugs ? self->lap_ms[self->stage_times[self->cur_stage-1][2]] : self->ms_value);

        OTraffic_set_max_traffic(&otraffic);
        OSoundInt_queue_sound(&osoundint, SOUND_YM_CHECKPOINT);
        OSoundInt_queue_sound(&osoundint, SOUND_VOICE_CHECKPOINT);
        
        /* Update Stage Number on HUD */
        OHud_draw_stage_number(&ohud, 0x110d76, self->cur_stage+1, GREEN);
        /* No need to redraw the stage info as that was a bug in the original game */
    }
}

/* Time Tables */
/* */
/* - Show how much time will be incremented to the counter at each stage */
/* - Rightmost routes first */
/* - Note there appears to be an error with the Stage 3a Normal entry */
/* */
/*         | Easy | Norm | Hard | VHar | */
/*         '------'------'------'------' */
/*Stage 1  |  80     75     72     70  | */
/*         '---------------------------' */
/*Stage 2a |  65     65     65     65  | */
/*Stage 2b |  62     62     62     62  | */
/*         '---------------------------' */
/*Stage 3a |  57     55     57     57  | */
/*Stage 3b |  62     60     60     60  | */
/*Stage 3c |  60     60     59     58  | */
/*         '---------------------------' */
/*Stage 4a |  66     65     64     62  | */
/*Stage 4b |  63     62     60     60  | */
/*Stage 4c |  61     60     58     58  | */
/*Stage 4d |  65     65     63     63  | */
/*         '---------------------------' */
/*Stage 5a |  58     56     54     54  | */
/*Stage 5b |  55     56     54     54  | */
/*Stage 5c |  56     56     54     54  | */
/*Stage 5d |  58     56     54     54  | */
/*Stage 5e |  56     56     56     56  | */
/*         '---------------------------' */


const uint8_t TIME[] =
{
    /* Easy */
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x65, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x57, 0x62, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x66, 0x63, 0x61, 0x65, 0x00, 0x00, 0x00, 0x00, 
    0x58, 0x55, 0x56, 0x58, 0x56, 0x00, 0x00, 0x00,

    /* Normal  */
    0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x65, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x65, 0x62, 0x60, 0x65, 0x00, 0x00, 0x00, 0x00,
    0x56, 0x56, 0x56, 0x56, 0x56, 0x00, 0x00, 0x00, 

    /* Hard */
    0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x65, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x57, 0x60, 0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x64, 0x60, 0x58, 0x63, 0x00, 0x00, 0x00, 0x00,
    0x54, 0x54, 0x54, 0x54, 0x56, 0x00, 0x00, 0x00, 

    /* Hardest */
    0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x65, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x57, 0x60, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x62, 0x60, 0x58, 0x63, 0x00, 0x00, 0x00, 0x00, 
    0x54, 0x54, 0x54, 0x54, 0x56, 0x00, 0x00, 0x00,
};
