/***************************************************************************
    Traffic Routines.

    - Traffic spawning.
    - Traffic logic, lane changing & movement.
    - Collisions.
    - Traffic panning and volume control to pass to sound program.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

enum
    {
        TRAFFIC_INIT = 0x10,       /* Initalize Traffic Object */
        TRAFFIC_ENTRY = 0x11,      /* First 0x80 Positions Of Road */
        TRAFFIC_TICK = 0x12        /* Tick Normally */
    };

typedef struct OTraffic
{
    uint8_t ai_traffic;
    uint8_t bonus_lhs;
    int8_t traffic_split;
    uint16_t collision_traffic;
    uint16_t collision_mask;
    oentry* traffic_adr[9];
    uint8_t max_traffic;
    int16_t traffic_speed_total;
    int16_t traffic_speed_avg;
    uint8_t traffic_pal_cycle;
    int16_t traffic_count;
    int16_t spawn_counter;
    int16_t spawn_location;
    int16_t wheel_reset;
    int16_t wheel_counter;
} OTraffic;

extern OTraffic otraffic;

void OTraffic_init(OTraffic* self);

void OTraffic_init_stage1_traffic(OTraffic* self);

void OTraffic_tick(OTraffic* self);

void OTraffic_disable_traffic(OTraffic* self);

void OTraffic_set_max_traffic(OTraffic* self);

void OTraffic_traffic_logic(OTraffic* self);

void OTraffic_traffic_sound(OTraffic* self);

#ifdef __cplusplus
}
#endif
