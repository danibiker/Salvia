/***************************************************************************
    Process Outputs.

    - Cabinet Vibration & Hydraulic Movement
    - Brake & Start Lamps
    - Coin Chute Outputs
    
    The Deluxe Motor code is also used by the force-feedback haptic system.

    One thing to note is that this code was originally intended to drive
    a moving hydraulic cabinet, not to be mapped to a haptic device.

    Therefore, it's not perfect when used in this way, but the results
    aren't bad :)
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdlib.h> /* abs */

#include "utils.h"

#include "engine/outrun.h"
#include "engine/ocrash.h"
#include "engine/oferrari.h"
#include "engine/ohud.h"
#include "engine/oinputs.h"
#include "engine/ooutputs.h"
#include "libretro/ffeedback.h"

static void OOutputs_diag_left(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit);
static void OOutputs_diag_right(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit);
static void OOutputs_diag_centre(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit);
static void OOutputs_diag_done(OOutputs* self);
static void OOutputs_calibrate_left(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit);
static void OOutputs_calibrate_right(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit);
static void OOutputs_calibrate_centre(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit);
static void OOutputs_calibrate_done(OOutputs* self);
static void OOutputs_do_motors(OOutputs* self, const int MODE, int16_t input_motor);
static void OOutputs_car_moving(OOutputs* self, const int MODE);
static void OOutputs_car_stationary(OOutputs* self);
static void OOutputs_adjust_motor(OOutputs* self);
static void OOutputs_do_motor_crash(OOutputs* self);
static void OOutputs_do_motor_offroad(OOutputs* self);
static void OOutputs_set_value(OOutputs* self, const uint8_t*, uint8_t);
static void OOutputs_done(OOutputs* self);
static void OOutputs_motor_output(OOutputs* self, uint8_t cmd);
static void OOutputs_do_vibrate_upright(OOutputs* self);
static void OOutputs_do_vibrate_mini(OOutputs* self);

void OOutputs_ctor(OOutputs* self)
{
    self->mode = MODE_DISABLED;

    self->chute1.output_bit  = D_COIN1_SUCC;
    self->chute2.output_bit  = D_COIN2_SUCC;

    self->col1               = 0;
    self->col2               = 0;
    self->limit_left         = 0;
    self->limit_right        = 0;
    self->motor_enabled      = true;
}



/* Initalize Moving Cabinet Motor */
/* Source: 0xECE8 */
void OOutputs_init(OOutputs* self)
{
    self->motor_state        = OO_STATE_INIT;
    self->hw_motor_control   = MOTOR_OFF;
    self->hw_motor_control_old = MOTOR_OFF;
    self->dig_out            = 0;
    self->dig_out_old        = -1;
    self->motor_control      = 0;
    self->motor_movement     = 0;
    self->is_centered        = false;
    self->motor_change_latch = 0;
    self->speed              = 0;
    self->curve              = 0;
    self->counter            = 0;
    self->vibrate_counter    = 0;
    self->was_small_change   = false;
    self->movement_adjust1   = 0;
    self->movement_adjust2   = 0;
    self->movement_adjust3   = 0;
    self->chute1.counter[0]  = 0;
    self->chute1.counter[1]  = 0;
    self->chute1.counter[2]  = 0;
    self->chute2.counter[0]  = 0;
    self->chute2.counter[1]  = 0;
    self->chute2.counter[2]  = 0;
}

void OOutputs_set_mode(OOutputs* self, int m)
{
    self->mode = m;
}

void OOutputs_tick(OOutputs* self, int16_t input_motor)
{
    switch (self->mode)
    {
        case MODE_DISABLED:
            break;

        /* Force Feedback Steering Wheels */
        case MODE_FFEEDBACK:
            OOutputs_do_motors(self, self->mode, input_motor);   /* Use X-Position of wheel instead of motor position */
            OOutputs_motor_output(self, self->hw_motor_control); /* Force Feedback Handling */
            break;

        /* GamePad: Basic Rumble */
        case MODE_RUMBLE:
            OOutputs_do_vibrate_upright(self);
            break;
    }
}

void OOutputs_writeDigitalToConsole(OOutputs* self)
{
    /* Not used by the Libretro core. */
}

/* ------------------------------------------------------------------------------------------------ */
/* Digital Outputs */
/* ------------------------------------------------------------------------------------------------ */

void OOutputs_set_digital(OOutputs* self, uint8_t output)
{
    self->dig_out |= output;   
}

void OOutputs_clear_digital(OOutputs* self, uint8_t output)
{
    self->dig_out &= ~output;
}

int OOutputs_is_set(OOutputs* self, uint8_t output)
{
    return (self->dig_out & output) ? 1 : 0;
}

/* ------------------------------------------------------------------------------------------------ */
/* Motor Diagnostics */
/* Source: 0x1885E */
/* ------------------------------------------------------------------------------------------------ */

bool OOutputs_diag_motor(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit)
{
    switch (self->motor_state)
    {
        /* Initalize */
        case OO_STATE_INIT:
            self->col1 = 10;
            self->col2 = 27;
            OHud_blit_text_new(&ohud, self->col1, 9, "LEFT LIMIT", GREY);
            OHud_blit_text_new(&ohud, self->col1, 11, "RIGHT LIMIT", GREY);
            OHud_blit_text_new(&ohud, self->col1, 13, "CENTRE", GREY);
            OHud_blit_text_new(&ohud, self->col1, 16, "MOTOR POSITION", GREY);
            OHud_blit_text_new(&ohud, self->col1, 18, "LIMIT B3 LEFT", GREY);
            OHud_blit_text_new(&ohud, self->col1, 19, "LIMIT B4 CENTRE", GREY);
            OHud_blit_text_new(&ohud, self->col1, 20, "LIMIT B5 RIGHT", GREY);
            self->counter          = COUNTER_RESET;
            self->motor_centre_pos = 0;
            self->motor_enabled    = true;
            self->motor_state = OO_STATE_LEFT;
            break;

        case OO_STATE_LEFT:
            OOutputs_diag_left(self, input_motor, hw_motor_limit);
            break;

        case OO_STATE_RIGHT:
            OOutputs_diag_right(self, input_motor, hw_motor_limit);
            break;

        case OO_STATE_CENTRE:
            OOutputs_diag_centre(self, input_motor, hw_motor_limit);
            break;

        case OO_STATE_DONE:
            OOutputs_diag_done(self);
            break;
    }

    /* Print Motor Position & Limit Switch */
    OHud_blit_text_new(&ohud, self->col2, 16, "  H", 0x80);
    OHud_blit_text_new(&ohud, self->col2, 16, Utils_to_hex_string(input_motor), 0x80);
    OHud_blit_text_new(&ohud, self->col2, 18, (hw_motor_limit & BIT_5) ? "OFF" : "ON ", 0x80);
    OHud_blit_text_new(&ohud, self->col2, 19, (hw_motor_limit & BIT_4) ? "OFF" : "ON ", 0x80);
    OHud_blit_text_new(&ohud, self->col2, 20, (hw_motor_limit & BIT_3) ? "OFF" : "ON ", 0x80);
    return self->motor_state == OO_STATE_DONE;
}

static void OOutputs_diag_left(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit)
{
    /* If Right Limit Reached, Move Left */
    if (hw_motor_limit & BIT_5)
    {
        if (--self->counter >= 0)
        {
            self->hw_motor_control = MOTOR_LEFT;
            return;
        }
        /* Counter Expired, Left Limit Still Not Reached */
        else
            OHud_blit_text_new(&ohud, self->col2, 9, "FAIL 1", 0x80);
    }
    /* Left Limit Reached */
    else if (hw_motor_limit & BIT_3)
    {
        OHud_blit_text_new(&ohud, self->col2, 9, "  H", 0x80);
        OHud_blit_text_new(&ohud, self->col2, 9, Utils_to_hex_string(input_motor), 0x80);
    }
    else
        OHud_blit_text_new(&ohud, self->col2, 9, "FAIL 2", 0x80);

    self->counter          = COUNTER_RESET;
    self->motor_state      = OO_STATE_RIGHT;
}


static void OOutputs_diag_right(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit)
{
    if (self->motor_centre_pos == 0 && (hw_motor_limit & BIT_4) == 0)
        self->motor_centre_pos = input_motor;
   
    /* If Left Limit Set, Move Right */
    if (hw_motor_limit & BIT_3)
    {
        if (--self->counter >= 0)
        {
            self->hw_motor_control = MOTOR_RIGHT; /* Move Right */
            return;
        }
        /* Counter Expired, Right Limit Still Not Reached */
        else
            OHud_blit_text_new(&ohud, self->col2, 11, "FAIL 1", 0x80);
    }
    /* Right Limit Reached */
    else if (hw_motor_limit & BIT_5)
    {
        OHud_blit_text_new(&ohud, self->col2, 11, "  H", 0x80);
        OHud_blit_text_new(&ohud, self->col2, 11, Utils_to_hex_string(input_motor), 0x80);
    }
    else
    {
        OHud_blit_text_new(&ohud, self->col2, 11, "FAIL 2", 0x80);
        self->motor_enabled = false;
        self->motor_state   = OO_STATE_DONE;
        return;
    }

    self->motor_state  = OO_STATE_CENTRE;
    self->counter      = COUNTER_RESET;
}


static void OOutputs_diag_centre(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit)
{
    if (hw_motor_limit & BIT_4)
    {
        if (--self->counter >= 0)
        {
            self->hw_motor_control = (self->counter <= COUNTER_RESET/2) ? MOTOR_RIGHT : MOTOR_LEFT; /* Move Right */
            return;
        }
        else
        {
            OHud_blit_text_new(&ohud, self->col2, 13, "FAIL", 0x80);
        }  
    }
    else
    {
        OHud_blit_text_new(&ohud, self->col2, 13, "  H", 0x80);
        OHud_blit_text_new(&ohud, self->col2, 13, Utils_to_hex_string((input_motor + self->motor_centre_pos) >> 1), 0x86);
        self->hw_motor_control = MOTOR_OFF; /* switch off */
        self->counter          = 32;
        self->motor_state      = OO_STATE_DONE;
    }
}

static void OOutputs_diag_done(OOutputs* self)
{
    if (self->counter > 0)
        self->counter--;

    if (self->counter == 0)
        self->hw_motor_control = MOTOR_CENTRE;
}

/* ------------------------------------------------------------------------------------------------ */
/* Calibrate Motors */
/* ------------------------------------------------------------------------------------------------ */

bool OOutputs_calibrate_motor(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit)
{
    switch (self->motor_state)
    {
        /* Initalize */
        case OO_STATE_INIT:
            self->col1 = 11;
            self->col2 = 25;
            OHud_blit_text_big(&ohud,       2,  "MOTOR CALIBRATION", false);
            OHud_blit_text_new(&ohud, self->col1, 10, "MOVE LEFT   -", GREY);
            OHud_blit_text_new(&ohud, self->col1, 12, "MOVE RIGHT  -", GREY);
            OHud_blit_text_new(&ohud, self->col1, 14, "MOVE CENTRE -", GREY);
            self->counter          = 25;
            self->motor_centre_pos = 0;
            self->motor_enabled    = true;
            self->motor_state++;
            break;

        /* Just a delay to wait for the serial for safety */
        case OO_STATE_DELAY:
            if (--self->counter == 0)
            {
                self->counter = COUNTER_RESET;
                self->motor_state++;
            }
            break;

        /* Calibrate Left Limit */
        case OO_STATE_LEFT:
            OOutputs_calibrate_left(self, input_motor, hw_motor_limit);
            break;

        /* Calibrate Right Limit */
        case OO_STATE_RIGHT:
            OOutputs_calibrate_right(self, input_motor, hw_motor_limit);
            break;

        /* Return to Centre */
        case OO_STATE_CENTRE:
            OOutputs_calibrate_centre(self, input_motor, hw_motor_limit);
            break;

        /* Clear Screen & Exit Calibration */
        case OO_STATE_DONE:
            OOutputs_calibrate_done(self);
            break;

        case OO_STATE_EXIT:
            return true;
    }

    return false;
}

static void OOutputs_calibrate_left(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit)
{
    /* If Right Limit Set, Move Left */
    if (hw_motor_limit & BIT_5)
    {
        if (--self->counter >= 0)
        {
            self->hw_motor_control = MOTOR_LEFT;
            return;
        }
        /* Counter Expired, Left Limit Still Not Reached */
        else
        {
            OHud_blit_text_new(&ohud, self->col2, 10, "FAIL 1", GREY);
            self->motor_centre_pos = 0;
            self->limit_left       = input_motor; /* Set Left Limit */
            self->hw_motor_control = MOTOR_LEFT;  /* Move Left */
            self->counter          = COUNTER_RESET;
            self->motor_state      = OO_STATE_RIGHT;
        }
    }
    /* Left Limit Reached */
    else if (hw_motor_limit & BIT_3)
    {
        OHud_blit_text_new(&ohud, self->col2, 10, Utils_to_hex_string(input_motor), 0x80);
        self->motor_centre_pos = 0;
        self->limit_left       = input_motor; /* Set Left Limit */
        self->hw_motor_control = MOTOR_LEFT;  /* Move Left */
        self->counter          = COUNTER_RESET; 
        self->motor_state      = OO_STATE_RIGHT;
    }
    else
    {
        OHud_blit_text_new(&ohud, self->col2, 10, "FAIL 2", GREY);
        OHud_blit_text_new(&ohud, self->col2, 12, "FAIL 2", GREY);
        self->motor_enabled = false; 
        self->counter       = COUNTER_RESET;
        self->motor_state   = OO_STATE_CENTRE;
    }
}

static void OOutputs_calibrate_right(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit)
{
    if (self->motor_centre_pos == 0 && ((hw_motor_limit & BIT_4) == 0))
    {
        self->motor_centre_pos = input_motor;
    }
   
    /* If Left Limit Set, Move Right */
    if (hw_motor_limit & BIT_3)
    {
        if (--self->counter >= 0)
        {
            self->hw_motor_control = MOTOR_RIGHT; /* Move Right */
            return;
        }
        /* Counter Expired, Right Limit Still Not Reached */
        else
        {
            OHud_blit_text_new(&ohud, self->col2, 12, "FAIL 1", GREY);
            self->limit_right  = input_motor;
            self->motor_state  = OO_STATE_CENTRE;
            self->counter      = COUNTER_RESET;
        }
    }
    /* Right Limit Reached */
    else if (hw_motor_limit & BIT_5)
    {
        OHud_blit_text_new(&ohud, self->col2, 12, Utils_to_hex_string(input_motor), 0x80);
        self->limit_right   = input_motor; /* Set Right Limit */
        self->motor_state   = OO_STATE_CENTRE;
        self->counter       = COUNTER_RESET;
    }
    else
    {
        OHud_blit_text_new(&ohud, self->col2, 12, "FAIL 2", GREY);
        self->motor_enabled = false;
        self->motor_state   = OO_STATE_CENTRE;
        self->counter       = COUNTER_RESET;
    }
}

static void OOutputs_calibrate_centre(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit)
{
    bool fail = false;

    if (hw_motor_limit & BIT_4)
    {
        if (--self->counter >= 0)
        {
            self->hw_motor_control = (self->counter <= COUNTER_RESET/2) ? MOTOR_RIGHT : MOTOR_LEFT; /* Move Right */
            return;
        }
        else
        {
            OHud_blit_text_new(&ohud, self->col2, 14, "FAIL SW", GREY);
            fail = true;
            /* Fall through to EEB6 */
        }  
    }
  
    /* 0xEEB6: */
    self->motor_centre_pos = (input_motor + self->motor_centre_pos) >> 1;
  
    { int16_t d0 = self->limit_right - self->motor_centre_pos;
    int16_t d1 = self->motor_centre_pos  - self->limit_left;
  
    /* set both to left limit */
    if (d0 > d1)
        d1 = d0;

    d0 = d1;
  
    self->limit_left  = d0 - 6;
    self->limit_right = -d1 + 6;
    
    if (abs(self->motor_centre_pos - 0x80) > 0x20)
    {
        OHud_blit_text_new(&ohud, self->col2, 14, "FAIL DIST", GREY);
        self->motor_enabled = false;
    }
    else if (!fail)
    {
        OHud_blit_text_new(&ohud, self->col2, 14, Utils_to_hex_string(self->motor_centre_pos), 0x80);
    }

    OHud_blit_text_new(&ohud, 13, 17, "TESTS COMPLETE!", 0x82);

    self->hw_motor_control = MOTOR_OFF; /* switch off */
    self->counter          = 90;
    self->motor_state      = OO_STATE_DONE;
 }}

static void OOutputs_calibrate_done(OOutputs* self)
{
    if (self->counter > 0)
        self->counter--;
    else
        self->motor_state = OO_STATE_EXIT;
}

/* ------------------------------------------------------------------------------------------------ */
/* Moving Cabinet Code */
/* ------------------------------------------------------------------------------------------------ */

const static uint8_t MOTOR_VALUES[] = 
{
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3,
    2, 2, 3, 3, 4, 4, 5, 5, 2, 3, 4, 5, 6, 7, 7, 7
};

const static uint8_t MOTOR_VALUES_STATIONARY[] = 
{
    2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4
};

const static uint8_t MOTOR_VALUES_OFFROAD1[] = 
{
    0x8, 0x8, 0x5, 0x5, 0x8, 0x8, 0xB, 0xB, 0x8, 0x8, 0x4, 0x4, 0x8, 0x8, 0xC, 0xC, 
    0x8, 0x8, 0x3, 0x3, 0x8, 0x8, 0xD, 0xD, 0x8, 0x8, 0x2, 0x2, 0x8, 0x8, 0xE, 0xE,
};

const static uint8_t MOTOR_VALUES_OFFROAD2[] = 
{
    0x8, 0x5, 0x8, 0xB, 0x8, 0x5, 0x8, 0xB, 0x8, 0x4, 0x8, 0xC, 0x8, 0x4, 0x8, 0xC,
    0x8, 0x3, 0x8, 0xD, 0x8, 0x3, 0x8, 0xD, 0x8, 0x2, 0x8, 0xE, 0x8, 0x2, 0x8, 0xE,
};

const static uint8_t MOTOR_VALUES_OFFROAD3[] = 
{
    0x8, 0x5, 0x5, 0x8, 0xB, 0x8, 0x0, 0x8, 0x8, 0x4, 0x4, 0x8, 0xC, 0x8, 0x0, 0x8,
    0x8, 0x3, 0x3, 0x8, 0xD, 0x8, 0x0, 0x8, 0x8, 0x2, 0x2, 0x8, 0xE, 0x8, 0x0, 0x8,
};

const static uint8_t MOTOR_VALUES_OFFROAD4[] = 
{
    0x8, 0xB, 0xB, 0x8, 0x5, 0x8, 0x0, 0x8, 0x8, 0xC, 0xC, 0x8, 0x4, 0x8, 0x0, 0x8,
    0x8, 0xD, 0xD, 0x8, 0x3, 0x8, 0x0, 0x8, 0x8, 0xE, 0xE, 0x8, 0x2, 0x8, 0x0, 0x8,
};


/* Process Motor Code. */
/* Note, that only the Deluxe Moving Motor Code is ported for now. */
/* Source: 0xE644 */
static void OOutputs_do_motors(OOutputs* self, int MODE, int16_t input_motor)
{
    self->motor_x_change = -(input_motor - (MODE == MODE_FFEEDBACK ? CENTRE_POS : self->motor_centre_pos));

    if (!self->motor_enabled)
    {
        OOutputs_done(self);
        return;
    }

    /* In-Game: Test for crash, skidding, whether car is moving */
    if (outrun.game_state == GS_INGAME)
    {
        if (ocrash.crash_counter)
        {
            if ((oinitengine.car_increment >> 16) <= 0x14)
                OOutputs_car_stationary(self);
            else
                OOutputs_do_motor_crash(self);
        }
        else if (ocrash.skid_counter)
        {
            OOutputs_do_motor_crash(self);
        }
        else
        {
            if ((oinitengine.car_increment >> 16) <= 0x14)
            {
                if (!self->was_small_change)
                    OOutputs_done(self);
                else
                    OOutputs_car_stationary(self);
            }
            else
                OOutputs_car_moving(self, MODE);
        }
    }
    /* Not In-Game: Act as though car is stationary / moving slow */
    else
    {
        OOutputs_car_stationary(self);
    }
}

/* Source: 0xE6DA */
static void OOutputs_car_moving(OOutputs* self, const int MODE)
{
    /* Motor is currently moving */
    if (self->motor_movement)
    {
        self->hw_motor_control = self->motor_control;
        OOutputs_adjust_motor(self);
        return;
    }
    
    /* Motor is not currently moving. Setup new movement as necessary. */
    if (oferrari.wheel_state != WHEELS_ON)
    {
        OOutputs_do_motor_offroad(self);
        return;
    }

    { const uint16_t car_inc = oinitengine.car_increment >> 16;
    if (car_inc <= 0x64)                    self->speed = 0;
    else if (car_inc <= 0xA0)               self->speed = 1 << 3;
    else if (car_inc <= 0xDC)               self->speed = 2 << 3;
    else                                    self->speed = 3 << 3;

    if (oinitengine.road_curve == 0)         self->curve = 0;
    else if (oinitengine.road_curve <= 0x3C) self->curve = 2; /* sharp self->curve */
    else if (oinitengine.road_curve <= 0x5A) self->curve = 1; /* gentle self->curve */
    else                                     self->curve = 0;

    { int16_t steering = oinputs.steering_adjust;
    steering += (self->movement_adjust1 + self->movement_adjust2 + self->movement_adjust3);
    steering >>= 2;
    self->movement_adjust3 = self->movement_adjust2;                   /* Trickle down values */
    self->movement_adjust2 = self->movement_adjust1;
    self->movement_adjust1 = oinputs.steering_adjust;

    /* Veer Left */
    if (steering >= 0)
    {
        steering = (steering >> 4) - 1;
        if (steering < 0)
        {
            OOutputs_car_stationary(self);
            return;
        }
                
        if (steering > 0)
            steering--;

        { uint8_t motor_value = MOTOR_VALUES[self->speed + self->curve];

        if (MODE == MODE_FFEEDBACK)
        {
            self->hw_motor_control = motor_value + 8;
        }
        else
        {
            int16_t change = self->motor_x_change + (motor_value << 1);
            /* Latch left movement */
            if (change >= self->limit_left)
            {
                self->hw_motor_control   = MOTOR_CENTRE;
                self->motor_movement     = 1;
                self->motor_control      = 7;
                self->motor_change_latch = self->motor_x_change;
            }
            else
            {
                self->hw_motor_control = motor_value + 8;
            }
        }
        
        OOutputs_done(self);
     }}
    /* Veer Right */
    else
    {
        steering = -steering;
        steering = (steering >> 4) - 1;
        if (steering < 0)
        {
            OOutputs_car_stationary(self);
            return;
        }

        if (steering > 0)
            steering--;

        { uint8_t motor_value = MOTOR_VALUES[self->speed + self->curve];

        if (MODE == MODE_FFEEDBACK)
        {
            self->hw_motor_control = -motor_value + 8;
        }
        else
        {
            int16_t change = self->motor_x_change - (motor_value << 1);
            /* Latch right movement */
            if (change <= self->limit_right)
            {
                self->hw_motor_control   = MOTOR_CENTRE;
                self->motor_movement     = -1;
                self->motor_control      = 9;
                self->motor_change_latch = self->motor_x_change;
            }
            else
            {
                self->hw_motor_control = -motor_value + 8;
            }
        }
        
        OOutputs_done(self);
     }}
 } }}

/* Source: 0xE822 */
static void OOutputs_car_stationary(OOutputs* self)
{
    int16_t change = abs(self->motor_x_change);

    if (change <= 8)
    {
        if (!self->is_centered)
        {
            self->hw_motor_control = MOTOR_CENTRE;
            self->is_centered      = true;
        }
        else
        {
            self->hw_motor_control = MOTOR_OFF;
            self->is_centered      = false;
            OOutputs_done(self);
        }
    }
    else
    {
        int8_t motor_value = MOTOR_VALUES_STATIONARY[change >> 3];

        if (self->motor_x_change >= 0)
            motor_value = -motor_value;

        self->hw_motor_control = motor_value + 8;

        OOutputs_done(self);
    }
}

/* Source: 0xE8DA */
static void OOutputs_adjust_motor(OOutputs* self)
{
    int16_t change = self->motor_change_latch; /* d1 */
    self->motor_change_latch = self->motor_x_change;
    change -= self->motor_x_change;
    if (change < 0) 
        change = -change;

    /* no movement */
    if (change <= 2)
    {
        self->motor_movement = 0;
        self->is_centered    = true;
    }
    /* moving right */
    else if (self->motor_movement < 0)
    {
        if (++self->motor_control > 10)
            self->motor_control = 10;
    }
    /* moving left */
    else 
    {
        if (--self->motor_control < 6)
            self->motor_control = 6;
    }

    OOutputs_done(self);
}

/* Adjust motor during crash/skid state */
/* Source: 0xE994 */
static void OOutputs_do_motor_crash(OOutputs* self)
{
    if (oferrari.car_x_diff == 0)
        OOutputs_set_value(self, MOTOR_VALUES_OFFROAD1, 3);
    else if (oferrari.car_x_diff < 0)
        OOutputs_set_value(self, MOTOR_VALUES_OFFROAD4, 3);
    else
        OOutputs_set_value(self, MOTOR_VALUES_OFFROAD3, 3);
}

/* Adjust motor when wheels are off-road */
/* Source: 0xE9BE */
static void OOutputs_do_motor_offroad(OOutputs* self)
{
    const uint8_t* table = (oferrari.wheel_state != WHEELS_OFF) ? MOTOR_VALUES_OFFROAD2 : MOTOR_VALUES_OFFROAD1;

    { const uint16_t car_inc = oinitengine.car_increment >> 16;
    uint8_t index;
    if (car_inc <= 0x32)      index = 0;
    else if (car_inc <= 0x50) index = 1;
    else if (car_inc <= 0x6E) index = 2;
    else                      index = 3;

    OOutputs_set_value(self, table, index);
 }}

static void OOutputs_set_value(OOutputs* self, const uint8_t* table, uint8_t index)
{
    self->hw_motor_control = table[(index << 3) + (self->counter & 7)];
    self->counter++;
    OOutputs_done(self);
}

/* Source: 0xE94E */
static void OOutputs_done(OOutputs* self)
{
    if (abs(self->motor_x_change) <= 8)
    {
        self->was_small_change = true;
        self->motor_control    = MOTOR_CENTRE;
    }
    else
    {
        self->was_small_change = false;
    }
}

/* Send output commands to motor hardware */
/* This is the equivalent to writing to register 0x140003 */
static void OOutputs_motor_output(OOutputs* self, uint8_t cmd)
{
    if (cmd == MOTOR_OFF || cmd == MOTOR_CENTRE)
        return;

    { int8_t force = 0;

    if (cmd < MOTOR_CENTRE)      /* left */
        force = cmd - 1;
    else if (cmd > MOTOR_CENTRE) /* right */
        force = 15 - cmd;

    forcefeedback_set(cmd, force);
 }}

/* ------------------------------------------------------------------------------------------------ */
/* Deluxe Upright: Steering Wheel Movement */
/* ------------------------------------------------------------------------------------------------ */

/* Deluxe Upright: Vibration Enable Table. 4 Groups of vibration values. */
const static uint8_t VIBRATE_LOOKUP[] = 
{
    /* SLOW SPEED --------   // MEDIUM SPEED ------ */
    1, 0, 0, 0, 1, 0, 0, 0,  1, 1, 0, 0, 1, 1, 0, 0,
    /* FAST SPEED --------   // VERY FAST SPEED --- */
    1, 1, 1, 0, 1, 1, 1, 0,  1, 1, 1, 1, 1, 1, 1, 1,
};

/* Source: 0xEAAA */
static void OOutputs_do_vibrate_upright(OOutputs* self)
{
    if (outrun.game_state != GS_INGAME)
    {
        OOutputs_clear_digital(self, D_MOTOR);
        return;
    }

    { const uint16_t speed = oinitengine.car_increment >> 16;
    uint16_t index = 0;

    /* Car Crashing: Diable Motor once speed below 10 */
    if (ocrash.crash_counter)
    {
        if (speed <= 10)
        {
            OOutputs_clear_digital(self, D_MOTOR);
            return;
        }
    }
    /* Car Normal */
    else if (!ocrash.skid_counter)
    {
        /* 0xEAE2: Disable Vibration once speed below 30 or wheels on-road */
        if (speed < 30 || oferrari.wheel_state == WHEELS_ON)
        {
            OOutputs_clear_digital(self, D_MOTOR);
            return;
        }

        /* 0xEAFC: Both wheels off-road. Faster the car speed, greater the chance of vibrating */
        if (oferrari.wheel_state == WHEELS_OFF)
        {
            if (speed > 220)      index = 3;
            else if (speed > 170) index = 2;
            else if (speed > 120) index = 1;
        }
        /* 0xEB38: One wheel off-road. Faster the car speed, greater the chance of vibrating */
        else
        {
            if (speed > 270)      index = 3;
            else if (speed > 210) index = 2;
            else if (speed > 150) index = 1;
        }

        if (VIBRATE_LOOKUP[ (self->vibrate_counter & 7) + (index << 3) ])
            OOutputs_set_digital(self, D_MOTOR);
        else
            OOutputs_clear_digital(self, D_MOTOR);

        self->vibrate_counter++;
        return;
    }
    /* 0xEB68: Car Crashing or Skidding */
    if (speed > 140)      index = 3;
    else if (speed > 100) index = 2;
    else if (speed > 60)  index = 1;

    if (VIBRATE_LOOKUP[ (self->vibrate_counter & 7) + (index << 3) ])
        OOutputs_set_digital(self, D_MOTOR);
    else
        OOutputs_clear_digital(self, D_MOTOR);

    self->vibrate_counter++;
 }}

/* ------------------------------------------------------------------------------------------------ */
/* Mini Upright: Steering Wheel Movement */
/* ------------------------------------------------------------------------------------------------ */

static void OOutputs_do_vibrate_mini(OOutputs* self)
{
    if (outrun.game_state != GS_INGAME)
    {
        OOutputs_clear_digital(self, D_MOTOR);
        return;
    }

    { const uint16_t speed = oinitengine.car_increment >> 16;
    uint16_t index = 0;

    /* Car Crashing: Disable Motor once speed below 10 */
    if (ocrash.crash_counter)
    {
        if (speed <= 10)
        {
            OOutputs_clear_digital(self, D_MOTOR);
            return;
        }
    }
    /* Car Normal */
    else if (!ocrash.skid_counter)
    {
        if (speed < 10 || oferrari.wheel_state == WHEELS_ON)
        {
            OOutputs_clear_digital(self, D_MOTOR);
            return;
        }  

        if (speed > 140)      index = 5;
        else if (speed > 100) index = 4;
        else if (speed > 60)  index = 3;
        else if (speed > 20)  index = 2;
        else                  index = 1;

        if (index > self->vibrate_counter)
        {
            self->vibrate_counter = 0;
            OOutputs_clear_digital(self, D_MOTOR);
        }
        else
        {
            self->vibrate_counter++;
            OOutputs_set_digital(self, D_MOTOR);
        }
        return;
    }

    /* 0xEC7A calc_crash_skid: */
    if (speed > 90)      index = 4;
    else if (speed > 70) index = 3;
    else if (speed > 50) index = 2;
    else if (speed > 30) index = 1;
    if (index > self->vibrate_counter)
    {
        self->vibrate_counter = 0;
        OOutputs_clear_digital(self, D_MOTOR);
    }
    else
    {
        self->vibrate_counter++;
        OOutputs_set_digital(self, D_MOTOR);
    }
 }}

/* ------------------------------------------------------------------------------------------------ */
/* Coin Chute Output */
/* Source: 0x6F8C */
/* ------------------------------------------------------------------------------------------------ */

void OOutputs_coin_chute_out(OOutputs* self, CoinChute* chute, bool insert)
{
    /* Initalize counter if coin inserted */
    chute->counter[2] = insert ? 1 : 0;

    if (chute->counter[0])
    {
        if (--chute->counter[0] != 0)
            return;
        chute->counter[1] = 6;
        OOutputs_clear_digital(self, chute->output_bit);
    }
    else if (chute->counter[1])
    {
        chute->counter[1]--;
    }
    /* Coin first inserted. Called Once.  */
    else if (chute->counter[2])
    {
        chute->counter[2]--;
        chute->counter[0] = 6;
        OOutputs_set_digital(self, chute->output_bit);
    }
}
