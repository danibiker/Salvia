/***************************************************************************
    Process Inputs.
    
    - Read & Process inputs and controls.
    - Note, this class does not contain platform specific code.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "libretro/input.h"

#include "outrun.h"

#define BRAKE_THRESHOLD1 (0x80)

#define BRAKE_THRESHOLD2 (0xA0)

#define BRAKE_THRESHOLD3 (0xC0)

#define BRAKE_THRESHOLD4 (0xE0)

#define DELAY_RESET (40)

#define STEERING_MIN (0x48)

#define STEERING_MAX (0xB8)

#define STEERING_CENTRE (0x80)

#define PEDAL_MIN (0x30)

#define PEDAL_MAX (0x90)

typedef struct OInputs
{
    int8_t crash_input;
    int16_t input_acc;
    int16_t input_steering;
    int16_t steering_adjust;
    int16_t acc_adjust;
    int16_t brake_adjust;
    bool gear;
    uint8_t steering_inc;
    uint8_t acc_inc;
    uint8_t brake_inc;
    int delay1, delay2, delay3;
    int16_t steering_old;
    int16_t steering_change;
    int16_t input_brake;
} OInputs;

extern OInputs oinputs;

void OInputs_init(OInputs* self);

void OInputs_tick(OInputs* self);

void OInputs_adjust_inputs(OInputs* self);

void OInputs_do_gear(OInputs* self);

uint8_t OInputs_do_credits(OInputs* self);

bool OInputs_is_analog_l(OInputs* self);

bool OInputs_is_analog_r(OInputs* self);

bool OInputs_is_analog_select(OInputs* self);

#ifdef __cplusplus
}
#endif
