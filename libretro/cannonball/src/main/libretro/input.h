/***************************************************************************
    Libretro Based Input Handling.

    Populates keys array with user input.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <boolean.h>

#include <stdint.h>

typedef enum presses {
        LEFT  = 0,
        RIGHT = 1,
        UP    = 2,
        DOWN  = 3,
        ACCEL = 4,
        BRAKE = 5,
        GEAR1 = 6,
        GEAR2 = 7,

        START = 8,
        COIN  = 9,
        VIEWPOINT = 10,
        
        PAUSE = 11,
        STEP  = 12,
        TIMER = 13,
        MENU  = 14
    } presses;

#define CENTRE (0x80)

#define DIGITAL_DEAD (3200)

typedef struct Input
{
    bool keys[15];
    bool keys_old[15];
    bool gamepad;
    int analog;
    int key_press;
    int16_t joy_button;
    int a_wheel;
    int a_accel;
    int a_brake;
    int* pad_config;
    int* key_config;
    int* axis;
    int wheel_zone;
    int wheel_dead;
    int pedals_dead;
} Input;

extern Input input;

void Input_init(Input* self, int, int*, int*, const int, int*, int*);

void Input_close(Input* self);

void Input_handle_joy_axis(Input* self, int, int, int);

void Input_frame_done(Input* self);

bool Input_is_pressed(Input* self, presses p);

bool Input_is_pressed_clear(Input* self, presses p);

bool Input_has_pressed(Input* self, presses p);

void Input_handle_key(Input* self, const int, const bool);

void Input_handle_joy(Input* self, const uint8_t, const bool);

#ifdef __cplusplus
}
#endif
