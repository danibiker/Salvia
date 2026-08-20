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

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

enum {SPLIT_NONE, SPLIT_INIT, SPLIT_CHOICE1, SPLIT_CHOICE2};

enum {ROAD_NOCHANGE, ROAD_STRAIGHT, ROAD_RIGHT, ROAD_LEFT};

#define RD_WIDTH_MERGE (0xD4)

typedef struct OInitEngine
{
    int16_t camera_x_off;
    bool ingame_engine;
    int16_t ingame_counter;
    uint16_t rd_split_state;
    int16_t road_type;
    int16_t road_type_next;
    uint8_t end_stage_props;
    uint32_t car_increment;
    int16_t car_x_pos;
    int16_t car_x_old;
    int8_t checkpoint_marker;
    int16_t road_curve;
    int16_t road_curve_next;
    int8_t road_remove_split;
    int8_t route_selected;
    int16_t change_width;
    int16_t road_width_next;
    int16_t road_width_adj;
    int16_t granular_rem;
    uint16_t pos_fine_old;
    int16_t road_width_orig;
    int16_t road_width_merge;
    int8_t route_updated;
} OInitEngine;

extern OInitEngine oinitengine;

void OInitEngine_init(OInitEngine* self, int8_t debug_level);

void OInitEngine_init_road_seg_master(OInitEngine* self);

void OInitEngine_init_crash_bonus(OInitEngine* self);

void OInitEngine_update_road(OInitEngine* self);

void OInitEngine_update_engine(OInitEngine* self);

void OInitEngine_update_shadow_offset(OInitEngine* self);

void OInitEngine_set_granular_position(OInitEngine* self);

void OInitEngine_set_fine_position(OInitEngine* self);

void OInitEngine_init_bonus(OInitEngine* self, int16_t);

#ifdef __cplusplus
}
#endif
