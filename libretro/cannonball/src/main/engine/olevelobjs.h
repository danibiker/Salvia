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

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

#define DEF_SPRITE_ENTRIES (0x44)

#define HISCORE_SPRITE_ENTRIES (0x40)

#define COLLISION_RESET (4)

#define SPRAY_RESET (0xC)

typedef struct OLevelObjs
{
    uint16_t spray_counter;
    uint16_t spray_type;
    uint8_t collision_sprite;
    int16_t sprite_collision_counter;
} OLevelObjs;

extern OLevelObjs olevelobjs;

void OLevelObjs_init_startline_sprites(OLevelObjs* self);

void OLevelObjs_init_timetrial_sprites(OLevelObjs* self);

void OLevelObjs_init_hiscore_sprites(OLevelObjs* self);

void OLevelObjs_setup_sprites(OLevelObjs* self, uint32_t);

void OLevelObjs_do_sprite_routine(OLevelObjs* self);

void OLevelObjs_hide_sprite(OLevelObjs* self, oentry*);

#ifdef __cplusplus
}
#endif
