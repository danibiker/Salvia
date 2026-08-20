/***************************************************************************
    Libretro Based Input Handling.

    Populates keys array with user input.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdlib.h> /* abs */
#include <string.h>
#include "input.h"

Input input;



void Input_init(Input* self, int pad_id, int* key_config, int* pad_config, int analog, int* axis, int* analog_settings)
{
    self->key_config  = key_config;
    self->pad_config  = pad_config;
    self->analog      = analog;
    self->axis        = axis;
    self->wheel_zone  = analog_settings[0];
    self->wheel_dead  = analog_settings[1];
    self->pedals_dead = analog_settings[2];
    self->gamepad     = analog;

    self->a_wheel = CENTRE;
}

void Input_close(Input* self)
{
}

/* Detect whether a key press change has occurred */
bool Input_has_pressed(Input* self, presses p)
{
    return self->keys[p] && !self->keys_old[p];
}

/* Detect whether key is still pressed */
bool Input_is_pressed(Input* self, presses p)
{
    return self->keys[p];
}

/* Detect whether pressed and clear the press */
bool Input_is_pressed_clear(Input* self, presses p)
{
    bool pressed = self->keys[p];
    self->keys[p] = false;
    return pressed;
}

/* Denote that a frame has been done by copying key presses into previous array */
void Input_frame_done(Input* self)
{
    memcpy(&self->keys_old, &self->keys, sizeof(self->keys));
}

void Input_handle_key(Input* self, const int key, const bool is_pressed)
{
    /* Redefinable Key Input */
    if (key == self->key_config[0])
        self->keys[UP] = is_pressed;

    else if (key == self->key_config[1])
        self->keys[DOWN] = is_pressed;

    else if (key == self->key_config[2])
        self->keys[LEFT] = is_pressed;

    else if (key == self->key_config[3])
        self->keys[RIGHT] = is_pressed;

    if (key == self->key_config[4])
        self->keys[ACCEL] = is_pressed;

    if (key == self->key_config[5])
        self->keys[BRAKE] = is_pressed;

    if (key == self->key_config[6])
        self->keys[GEAR1] = is_pressed;

    if (key == self->key_config[7])
        self->keys[GEAR2] = is_pressed;

    if (key == self->key_config[8])
        self->keys[START] = is_pressed;

    if (key == self->key_config[9])
        self->keys[COIN] = is_pressed;

    if (key == self->key_config[10])
        self->keys[MENU] = is_pressed;

    if (key == self->key_config[11])
        self->keys[VIEWPOINT] = is_pressed;
}


void Input_handle_joy_axis(Input* self, int wheel_axis, int accel_axis, int brake_axis)
{
   /* Analog Controls */

   int adjusteda;
   int adjustedb;

   /* Steering */
   /* OutRun requires values between 0x48 and 0xb8. */
   int percentage_adjust = ((self->wheel_zone) << 8) / 100;         
   int adjustedw = wheel_axis + ((wheel_axis * percentage_adjust) >> 8);

   /* Make 0 hard left, and 0x80 centre value. */
   adjustedw = ((adjustedw + (1 << 15)) >> 9);
   adjustedw += 0x40; /* Centre */

   if (adjustedw < 0x40)
       adjustedw = 0x40;
   else if (adjustedw > 0xC0)
       adjustedw = 0xC0;

   /* Remove Dead Zone */
   if (self->wheel_dead)
   {
       if (abs(CENTRE - adjustedw) <= self->wheel_dead)
           adjustedw = CENTRE;
   }
   self->a_wheel = adjustedw;

   /* Accelerator [Single Axis] */
   /* Scale input to be in the range of 0 to 0x7F */
   adjusteda = accel_axis/256;          
   adjusteda += (adjusteda >> 2);
   self->a_accel = adjusteda;

   /* Brake [Single Axis] */
   /* Scale input to be in the range of 0 to 0x7F */
   adjustedb = 0x7F - ((-brake_axis + (1 << 15)) >> 9);
   adjustedb += (adjustedb >> 2);
   self->a_brake = adjustedb;
}

void Input_handle_joy(Input* self, const uint8_t button, const bool is_pressed)
{
    if (button == self->pad_config[0])
        self->keys[ACCEL] = is_pressed;

    if (button == self->pad_config[1])
        self->keys[BRAKE] = is_pressed;

    if (button == self->pad_config[2])
        self->keys[GEAR1] = is_pressed;

    if (button == self->pad_config[3])
        self->keys[GEAR2] = is_pressed;

    if (button == self->pad_config[4])
        self->keys[START] = is_pressed;

    if (button == self->pad_config[5])
        self->keys[COIN] = is_pressed;

    if (button == self->pad_config[6])
        self->keys[MENU] = is_pressed;

    if (button == self->pad_config[7])
        self->keys[VIEWPOINT] = is_pressed;
}
