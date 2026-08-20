/***************************************************************************
    Process Inputs.
    
    - Read & Process inputs and controls.
    - Note, this class does not contain platform specific code.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "ocrash.h"
#include "oinputs.h"
#include "ostats.h"

static void OInputs_digital_steering(OInputs* self);
static void OInputs_digital_pedals(OInputs* self);

OInputs oinputs;



void OInputs_init(OInputs* self)
{
    self->input_steering  = STEERING_CENTRE;
    self->steering_old    = STEERING_CENTRE;
    self->steering_adjust = 0;
    self->acc_adjust      = 0;
    self->brake_adjust    = 0;
    self->steering_change = 0;

    self->steering_inc = config.controls.steer_speed;
    self->acc_inc      = config.controls.pedal_speed * 4;
    self->brake_inc    = config.controls.pedal_speed * 4;

    self->input_acc   = 0;
    self->input_brake = 0;
    self->gear        = false;
    self->crash_input = 0;
    self->delay1      = 0;
    self->delay2      = 0;
    self->delay3      = 0;
}

void OInputs_tick(OInputs* self)
{
    /* Digital Controls: Simulate Analog */
    if (!input.analog || !input.gamepad)
    {
        OInputs_digital_steering(self);
        OInputs_digital_pedals(self);
    }
    /* Analog Controls */
    else
    {
        self->input_steering = input.a_wheel;

        /* Analog Pedals */
        if (input.analog == 1)
        {
            self->input_acc   = input.a_accel;
            self->input_brake = input.a_brake;
        }
        /* Digital Pedals */
        else
        {
            OInputs_digital_pedals(self);
        }
    }
}
/* DIGITAL CONTROLS: Digital Simulation of analog steering */static 
void OInputs_digital_steering(OInputs* self)
{
    /* ------------------------------------------------------------------------ */
    /* STEERING */
    /* ------------------------------------------------------------------------ */
    if (Input_is_pressed(&input, LEFT))
    {
        /* Recentre wheel immediately if facing other way */
        if (self->input_steering > STEERING_CENTRE) self->input_steering = STEERING_CENTRE;

        self->input_steering -= self->steering_inc;
        if (self->input_steering < STEERING_MIN) self->input_steering = STEERING_MIN;
    }
    else if (Input_is_pressed(&input, RIGHT))
    {
        /* Recentre wheel immediately if facing other way */
        if (self->input_steering < STEERING_CENTRE) self->input_steering = STEERING_CENTRE;

        self->input_steering += self->steering_inc;
        if (self->input_steering > STEERING_MAX) self->input_steering = STEERING_MAX;
    }
    /* Return steering to centre if nothing pressed */
    else
    {
        if (self->input_steering < STEERING_CENTRE)
        {
            self->input_steering += self->steering_inc;
            if (self->input_steering > STEERING_CENTRE)
                self->input_steering = STEERING_CENTRE;
        }
        else if (self->input_steering > STEERING_CENTRE)
        {
            self->input_steering -= self->steering_inc;
            if (self->input_steering < STEERING_CENTRE)
                self->input_steering = STEERING_CENTRE;
        }
    }
}

/* DIGITAL CONTROLS: Digital Simulation of analog pedals */static 
void OInputs_digital_pedals(OInputs* self)
{
    /* ------------------------------------------------------------------------ */
    /* ACCELERATION */
    /* ------------------------------------------------------------------------ */

    if (Input_is_pressed(&input, ACCEL))
    {
        self->input_acc += self->acc_inc;
        if (self->input_acc > 0xFF) self->input_acc = 0xFF;
    }
    else
    {
        self->input_acc -= self->acc_inc;
        if (self->input_acc < 0) self->input_acc = 0;
    }

    /* ------------------------------------------------------------------------ */
    /* BRAKE */
    /* ------------------------------------------------------------------------ */

    if (Input_is_pressed(&input, BRAKE))
    {
        self->input_brake += self->brake_inc;
        if (self->input_brake > 0xFF) self->input_brake = 0xFF;
    }
    else
    {
        self->input_brake -= self->brake_inc;
        if (self->input_brake < 0) self->input_brake = 0;
    }
}

void OInputs_do_gear(OInputs* self)
{
    /* ------------------------------------------------------------------------ */
    /* GEAR SHIFT */
    /* ------------------------------------------------------------------------ */

    /* Automatic Gears: Don't do anything */
    if (config.controls.gear == GEAR_AUTO)
        return;

    else
    {
        /* Manual: Cabinet Shifter */
        if (config.controls.gear == GEAR_PRESS)
            self->gear = !(Input_is_pressed(&input, GEAR1) || Input_is_pressed(&input, GEAR2));

        /* Manual: Two Separate Buttons for gears */
        else if (config.controls.gear == GEAR_SEPARATE)
        {
            if (Input_has_pressed(&input, GEAR1))
                self->gear = false;
            else if (Input_has_pressed(&input, GEAR2))
                self->gear = true;
        }

        /* Manual: Keyboard/Digital Button */
        else
        {
            if (Input_has_pressed(&input, GEAR1) || Input_has_pressed(&input, GEAR2))
                self->gear = !self->gear;
        }
    }
}

/* Adjust Analogue Inputs */
/* */
/* Read, Adjust & Write Analogue Inputs */
/* In the original, these values are set during a H-Blank routine */
/* */
/* Source: 74E2 */

void OInputs_adjust_inputs(OInputs* self)
{
    int16_t brake;
    int16_t acc;
    /* Cap Steering Value */
    if (self->input_steering < STEERING_MIN) self->input_steering = STEERING_MIN;
    else if (self->input_steering > STEERING_MAX) self->input_steering = STEERING_MAX;

    if (self->crash_input)
    {
        self->crash_input--;
        { int16_t d0 = ((self->input_steering - 0x80) * 0x100) / 0x70;
        if (d0 > 0x7F) d0 = 0x7F;
        else if (d0 < -0x7F) d0 = -0x7F;
        self->steering_adjust = ocrash.crash_counter ? 0 : d0;
     }}
    else
    {
        /* no_crash1: */
        int16_t d0 = self->input_steering - self->steering_old;
        self->steering_old = self->input_steering;
        self->steering_change += d0;
        d0 = self->steering_change < 0 ? -self->steering_change : self->steering_change;

        /* Note the below line "if (d0 > 2)" causes a bug in the original game */
        /* whereby if you hold the wheel to the left whilst stationary, then accelerate the car will veer left even */
        /* when the wheel has been centered */
        if (config.engine.fix_bugs || d0 > 2)
        {
            self->steering_change = 0;
            /* Convert input steering value to internal value */
            d0 = ((self->input_steering - 0x80) * 0x100) / 0x70;
            if (d0 > 0x7F) d0 = 0x7F;
            else if (d0 < -0x7F) d0 = -0x7F;
            self->steering_adjust = ocrash.crash_counter ? 0 : d0;
        }
    }

    /* Cap & Adjust Acceleration Value */
    acc = self->input_acc;
    if (acc > PEDAL_MAX) acc = PEDAL_MAX;
    else if (acc < PEDAL_MIN) acc = PEDAL_MIN;
    self->acc_adjust = ((acc - 0x30) * 0x100) / 0x61;

    /* Cap & Adjust Brake Value */
    brake = self->input_brake;
    if (brake > PEDAL_MAX) brake = PEDAL_MAX;
    else if (brake < PEDAL_MIN) brake = PEDAL_MIN;
    self->brake_adjust = ((brake - 0x30) * 0x100) / 0x61;
}

/* Simplified version of do credits routine.  */
/* I have not ported the coin chute handling code, or dip switch routines. */
/* */
/* Returns: 0 (No Coin Inserted) */
/*          1 (Coin Chute 1 Used) */
/*          2 (Coin Chute 2 Used) */
/*          3 (Key Pressed / Service Button) */
/* */
/* Source: 0x6DE0 */
uint8_t OInputs_do_credits(OInputs* self)
{
    if (Input_has_pressed(&input, COIN))
    {
        if (!config.engine.freeplay && ostats.credits < 9)
        {
            ostats.credits++;
            /* todo: Increment credits total for bookkeeping */
            OSoundInt_queue_sound(&osoundint, SOUND_COIN_IN);
        }
        return 3;
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------ */
/* Menu Selection Controls */
/* ------------------------------------------------------------------------------------------------ */

bool OInputs_is_analog_l(OInputs* self)
{
    if (self->input_steering < STEERING_CENTRE - 0x10)
    {
        if (--self->delay1 < 0)
        {
            self->delay1 = DELAY_RESET;
            return true;
        }
    }
    else
        self->delay1 = DELAY_RESET;
    return false;
}

bool OInputs_is_analog_r(OInputs* self)
{
    if (self->input_steering > STEERING_CENTRE + 0x10)
    {
        if (--self->delay2 < 0)
        {
            self->delay2 = DELAY_RESET;
            return true;
        }
    }
    else
        self->delay2 = DELAY_RESET;
    return false;
}

bool OInputs_is_analog_select(OInputs* self)
{
    if (self->input_acc > 0x90)
    {
        if (--self->delay3 < 0)
        {
            self->delay3 = DELAY_RESET;
            return true;
        }
    }
    else
        self->delay3 = DELAY_RESET;
    return false;
}
