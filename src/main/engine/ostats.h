/***************************************************************************
    In-Game Statistics.
    - Stage Timers
    - Route Info
    - Speed to Score Conversion
    - Bonus Time Increment
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

static const int16_t frame_reset = 30;

extern const uint8_t TIME[];

typedef struct OStats
{
    int8_t cur_stage;
    uint32_t score;
    uint16_t route_info;
    uint16_t routes[0x8];
    int16_t frame_counter;
    int16_t time_counter;
    int16_t extend_play_timer;
    int16_t stage_counters[15];
    bool game_completed;
    const uint8_t* lap_ms;
    uint8_t credits;
    uint8_t stage_times[15][3];
    uint8_t ms_value;
} OStats;

extern OStats ostats;

void OStats_init(OStats* self, bool);

void OStats_clear_stage_times(OStats* self);

void OStats_clear_route_info(OStats* self);

void OStats_do_mini_map(OStats* self);

void OStats_do_timers(OStats* self);

void OStats_convert_speed_score(OStats* self, uint16_t);

void OStats_update_score(OStats* self, uint32_t);

void OStats_init_next_level(OStats* self);

#ifdef __cplusplus
}
#endif
