/***************************************************************************
    Process Outputs.
    
    - Only the Deluxe Moving Motor Code is ported for now.
    - This is used by the force-feedback haptic system.

    One thing to note is that this code was originally intended to drive
    a moving hydraulic cabinet, not to be mapped to a haptic device.

    Therefore, it's not perfect when used in this way, but the results
    aren't bad :)
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


enum {
    MODE_DISABLED = 0,
    MODE_FFEEDBACK = 2,
    MODE_RUMBLE = 3,
    OO_STATE_INIT = 0,
    OO_STATE_DELAY = 1,
    OO_STATE_LEFT = 2,
    OO_STATE_RIGHT = 3,
    OO_STATE_CENTRE = 4,
    OO_STATE_DONE = 5,
    OO_STATE_EXIT = 6,
    COUNTER_RESET = 300,
    MOTOR_OFF = 0,
    MOTOR_RIGHT = 0x5,
    MOTOR_CENTRE = 0x8,
    MOTOR_LEFT = 0xB,
    CENTRE_POS = 0x80,
    LEFT_LIMIT = 0xC1,
    RIGHT_LIMIT = 0x3C
};


#include <stdint.h>

typedef struct CoinChute
{
    /* Coin Chute Counters */
    uint8_t counter[3];
    /* Output bit */
    uint8_t output_bit;
} CoinChute;

enum {
        D_EXT_MUTE   = 0x01, /* bit 0 = External Amplifier Mute Control */
        D_BRAKE_LAMP = 0x02, /* bit 1 = brake lamp */
        D_START_LAMP = 0x04, /* bit 2 = start lamp */
        D_COIN1_SUCC = 0x08, /* bit 3 = Coin successfully inserted - Chute 2 */
        D_COIN2_SUCC = 0x10, /* bit 4 = Coin successfully inserted - Chute 1 */
        D_MOTOR      = 0x20, /* bit 5 = steering wheel central vibration */
        D_UNUSED     = 0x40, /* bit 6 = ? */
        D_SOUND      = 0x80 /* bit 7 = sound enable */
    };

typedef struct OOutputs
{
    uint8_t hw_motor_control, hw_motor_control_old;
    CoinChute chute1, chute2;
    int mode;
    uint8_t dig_out, dig_out_old;
    int16_t limit_left;
    int16_t limit_right;
    int16_t motor_centre_pos;
    int16_t motor_x_change;
    uint16_t motor_state;
    bool motor_enabled;
    int8_t motor_control;
    int8_t motor_movement;
    bool is_centered;
    int16_t motor_change_latch;
    int16_t speed;
    int16_t curve;
    int16_t vibrate_counter;
    bool was_small_change;
    int16_t movement_adjust1;
    int16_t movement_adjust2;
    int16_t movement_adjust3;
    int16_t counter;
    uint16_t col1, col2;
} OOutputs;

void OOutputs_ctor(OOutputs* self);

void OOutputs_init(OOutputs* self);

void OOutputs_set_mode(OOutputs* self, int);

bool OOutputs_diag_motor(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit);

bool OOutputs_calibrate_motor(OOutputs* self, int16_t input_motor, uint8_t hw_motor_limit);

void OOutputs_tick(OOutputs* self, int16_t input_motor);

void OOutputs_writeDigitalToConsole(OOutputs* self);

void OOutputs_set_digital(OOutputs* self, uint8_t);

void OOutputs_clear_digital(OOutputs* self, uint8_t);

int OOutputs_is_set(OOutputs* self, uint8_t);

void OOutputs_coin_chute_out(OOutputs* self, CoinChute* chute, bool insert);

#ifdef __cplusplus
}
#endif
