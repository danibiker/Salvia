/***************************************************************************
    Ferrari Rendering & Handling Code.
       
    Much of the handling code is very messy. As such, the translated code 
    isn't great as I tried to focus on accuracy rather than refactoring.
    
    A good example of the randomness is a routine I've named
      do_sound_score_slip()
    which performs everything from updating the score, setting the audio
    engine tone, triggering smoke effects etc. in an interwoven fashion.
    
    The Ferrari sprite has different properties to other game objects
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

enum {
        /* Initialise Intro Animation Sequences */
        FERRARI_SEQ1 = 0,

        /* Tick Intro Animation Sequences */
        FERRARI_SEQ2 = 1,

        /* Initialize In-Game Logic */
        FERRARI_INIT = 2,

        /* Tick In-Game Logic */
        FERRARI_LOGIC = 3,
        
        /* Ferrari End Sequence Logic */
        FERRARI_END_SEQ = 4
    };

enum { CAR_ANIM_SEQ = -1, CAR_NORMAL = 0, CAR_SMOKE = 1};

enum {
        WHEELS_ON = 0,
        WHEELS_LEFT_OFF = 1,
        WHEELS_RIGHT_OFF = 2,
        WHEELS_OFF = 3
    };

enum {
        TRACTION_ON = 0,
        TRACTION_HALF = 1,
        TRACTION_OFF = 2
    };

#define PAL_RED 2

#define PAL_BLUE 256

#define PAL_YELLOW 261

#define PAL_GREEN 266

#define PAL_CYAN 271

#define PAL_BLACK 276

#define PAL_WHITE 281

#define MAX_SPEED (0x1260000)

#define CAR_BASE_INC (0x12F)

#define OFFROAD_BOUNDS (0x1F4)

extern uint16_t torque_lookup[];

extern const uint8_t rev_inc_lookup[];

typedef struct OFerrari
{
    oentry *spr_ferrari;
    oentry *spr_pass1;
    oentry *spr_pass2;
    oentry *spr_shadow;
    uint16_t ferrari_pal;
    uint8_t state;
    uint16_t counter;
    int16_t steering_old;
    bool car_ctrl_active;
    int8_t car_state;
    bool auto_brake;
    uint8_t torque_index;
    int16_t torque;
    int32_t revs;
    uint8_t rev_shift;
    uint8_t wheel_state;
    uint8_t wheel_traction;
    uint16_t is_slipping;
    uint8_t slip_sound;
    uint16_t car_inc_old;
    int16_t car_x_diff;
    int16_t rev_stop_flag;
    int16_t revs_post_stop;
    int16_t acc_post_stop;
    uint16_t rev_pitch1;
    uint16_t rev_pitch2;
    int16_t sprite_ai_counter;
    int16_t sprite_ai_curve;
    int16_t sprite_ai_x;
    int16_t sprite_ai_steer;
    int16_t sprite_car_x_bak;
    int16_t sprite_wheel_state;
    int16_t sprite_slip_copy;
    int8_t wheel_pal;
    int16_t sprite_pass_y;
    int16_t wheel_frame_reset;
    int16_t wheel_counter;
    int16_t road_width_old;
    int16_t accel_value;
    int16_t accel_value_bak;
    int16_t brake_value;
    bool gear_value;
    bool gear_bak;
    int16_t acc_adjust1;
    int16_t acc_adjust2;
    int16_t acc_adjust3;
    int16_t brake_adjust1;
    int16_t brake_adjust2;
    int16_t brake_adjust3;
    int32_t brake_subtract;
    int8_t gear_counter;
    int32_t rev_adjust;
    int16_t gear_smoke;
    int16_t gfx_smoke;
    int8_t cornering;
    int8_t cornering_old;
} OFerrari;

extern OFerrari oferrari;

void OFerrari_init(OFerrari* self, oentry*, oentry*, oentry*, oentry*);

void OFerrari_reset_car(OFerrari* self);

void OFerrari_init_ingame(OFerrari* self);

void OFerrari_tick(OFerrari* self);

void OFerrari_set_ferrari_x(OFerrari* self);

void OFerrari_set_ferrari_bounds(OFerrari* self);

void OFerrari_check_wheels(OFerrari* self);

void OFerrari_set_curve_adjust(OFerrari* self);

void OFerrari_draw_shadow(OFerrari* self);

void OFerrari_move(OFerrari* self);

void OFerrari_do_sound_score_slip(OFerrari* self);

void OFerrari_shake(OFerrari* self);

void OFerrari_do_skid(OFerrari* self);

#ifdef __cplusplus
}
#endif
