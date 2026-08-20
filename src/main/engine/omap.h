/***************************************************************************
    Course Map Logic & Rendering. 
    
    This is the full-screen map that is displayed at the end of the game. 
    
    The logo is built from multiple sprite components.
    
    The course map itself is made up of sprites and pieced together. 
    It's not a tilemap.
    
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
        MAP_INIT  = 0,
        /* Do Route [Note map is displayed from this point on] */
        MAP_ROUTE = 0x4,
        /* Do Final Segment Of Route [Car still moving] */
        MAP_ROUTE_FINAL = 0x08,
        /* Route Concluded */
        MAP_ROUTE_DONE = 0x0C,
        /* Init Delay Counter For Map Display */
        MAP_INIT_DELAY = 0x10,
        /* Display Map */
        MAP_DISPLAY = 0x14,
        /* Clear Course Map */
        MAP_CLEAR = 0x18
    };

#define MAP_PIECES (0x3C)

typedef struct OMap
{
    bool init_sprites;
    uint8_t map_state;
    uint8_t map_route;
    int16_t map_pos;
    int16_t map_pos_final;
    int16_t map_delay;
    int16_t map_stage1;
    int16_t map_stage2;
    uint8_t minicar_enable;
} OMap;

extern OMap omap;

void OMap_init(OMap* self);

void OMap_tick(OMap* self);

void OMap_blit(OMap* self);

void OMap_load_sprites(OMap* self);

void OMap_draw_course_map(OMap* self);

void OMap_position_ferrari(OMap* self, uint8_t index);

#ifdef __cplusplus
}
#endif
