/***************************************************************************
    Collision & Crash Code. 
    
    There are two types of collision: Scenery & Traffic.
    
    1/ Traffic: The Ferrari will spin after a collision.
    2/ Scenery: There are three types of scenery collision:
       - Low speed bump. Car rises slightly in the air and stalls.
       - Mid speed spin. Car spins and slides after collision.
       - High speed flip. If slightly slower, car rolls into screen.
         Otherwise, grows towards screen and vanishes
         
    Known Issues With Original Code:
    - Passenger sprites flicker if they land moving in the water on Stage 1
    
    The Ferrari sprite is used differently by the crash code.
    As there's only one of them, I've rolled the additional variables into
    this class. 
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

enum { CRASH_BUMP = 0, CRASH_SPIN = 1, CRASH_FLIP = 2 };

#define SKID_RESET (20)

#define SKID_MAX (30)

#define SKID_X_ADJ (24)

typedef struct OCrash
{
    oentry* spr_ferrari;
    oentry* spr_shadow;
    oentry* spr_pass1;
    oentry* spr_pass1s;
    oentry* spr_pass2;
    oentry* spr_pass2s;
    int8_t crash_state;
    int16_t skid_counter;
    int16_t skid_counter_bak;
    uint8_t spin_control1;
    uint8_t spin_control2;
    int16_t coll_count1;
    int16_t coll_count2;
    int16_t crash_counter;
    int16_t crash_spin_count;
    int16_t crash_z;
    int16_t spinflipcount1;
    int16_t spinflipcount2;
    int16_t slide;
    int16_t frame;
    uint32_t addr;
    int16_t camera_x_target;
    int16_t camera_xinc;
    int16_t lookup_index;
    int16_t frame_restore;
    int16_t shift;
    int16_t crash_speed;
    int16_t crash_zinc;
    int16_t crash_side;
    int16_t spin_pass_frame;
    int8_t crash_type;
    int16_t crash_delay;
    void (*function_pass1)(struct OCrash* self, oentry*);
    void (*function_pass2)(struct OCrash* self, oentry*);
} OCrash;

extern OCrash ocrash;

void OCrash_init(OCrash* self, oentry* f, oentry* s, oentry* p1, oentry* p1s, oentry* p2, oentry* p2s);

bool OCrash_is_flip(OCrash* self);

void OCrash_enable(OCrash* self);

void OCrash_clear_crash_state(OCrash* self);

void OCrash_tick(OCrash* self);

#ifdef __cplusplus
}
#endif
