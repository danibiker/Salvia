/***************************************************************************
    Libretro Force Feedback (aka Haptic) Support
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <string.h>

#include "ffeedback.h"
#include "frontend/config.h"

static struct retro_rumble_interface rumble = {0};
static uint16_t rumble_strength_last        = 0;
static bool rumble_active                   = false;
static uint16_t rumble_min                  = 0;
static uint16_t rumble_step                 = 0;
static uint16_t rumble_duration             = 0;
static uint16_t rumble_counter              = 0;

bool forcefeedback_init_rumble_interface(retro_environment_t environ_cb)
{
    memset(&rumble, 0, sizeof(struct retro_rumble_interface));

    rumble_strength_last = 0;
    rumble_active        = false;

    rumble_min           = 0x1999;
    rumble_step          = (0xFFFF - 0x1999) / 5;
    rumble_duration      = 500;
    rumble_counter       = 0;

    if (environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble) &&
                rumble.set_rumble_state)
        return true;

    return false;
}

void forcefeedback_deactivate_rumble()
{
    if (!rumble.set_rumble_state || !rumble_active)
        return;

    rumble.set_rumble_state(0, RETRO_RUMBLE_WEAK, 0);
    rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, 0);

    rumble_counter       = 0;
    rumble_strength_last = 0;
    rumble_active        = false;
}

void forcefeedback_update_rumble_interface()
{
    uint16_t frame_ticks = (config.fps == 120) ? 8 : 16;

    if (!rumble.set_rumble_state || !rumble_active)
        return;

    if (rumble_counter > frame_ticks)
        rumble_counter -= frame_ticks;
    else
    {
        rumble.set_rumble_state(0, RETRO_RUMBLE_WEAK, 0);
        rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, 0);

        rumble_counter       = 0;
        rumble_strength_last = 0;
        rumble_active        = false;
    }
}

void forcefeedback_update_force_limits(int max_force, int min_force, int force_duration)
{
    max_force = (max_force < 0)      ? 0 : max_force;
    max_force = (max_force > 0xFFFF) ? 0xFFFF : max_force;

    min_force = (min_force < 0)         ? 0 : min_force;
    min_force = (min_force > max_force) ? max_force : min_force;

    force_duration = (force_duration < 0) ? 0 : force_duration;

    rumble_min      = min_force;
    rumble_step     = (max_force - min_force) / 5;
    rumble_duration = force_duration;
}

bool forcefeedback_init(int max_force, int min_force, int force_duration)
{
    if (!rumble.set_rumble_state)
        return false;

    forcefeedback_update_force_limits(max_force, min_force, force_duration);
    return true;
}

void forcefeedback_close()
{
    if (rumble.set_rumble_state && rumble_active)
    {
        rumble.set_rumble_state(0, RETRO_RUMBLE_WEAK, 0);
        rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, 0);
    }

    rumble_counter       = 0;
    rumble_strength_last = 0;
    rumble_active        = false;

    memset(&rumble, 0, sizeof(struct retro_rumble_interface));
}

int forcefeedback_set(int xdirection, int force)
{
    uint16_t rumble_strength = 0;

    if (!rumble.set_rumble_state || (rumble_step == 0))
        return 0;

    /* > xdirection is ignored (used to control
     *   real motors in a physical cabinet)
     * > force is a value in the range [0, 5]
     *   (0: strongest, 5: weakest) */
    force = (force < 0) ? 0 : force;
    force = (force > 5) ? 5 : force;

    rumble_strength = rumble_min + ((5 - force) * rumble_step);
    rumble_strength = (rumble_strength > 0xFFFF) ?
            0xFFFF : rumble_strength;

    if (rumble_strength != rumble_strength_last)
    {
        rumble.set_rumble_state(0, RETRO_RUMBLE_WEAK, rumble_strength);
        rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, rumble_strength);
        rumble_strength_last = rumble_strength;
    }

    if (rumble_strength > 0)
    {
        rumble_counter = rumble_duration;
        rumble_active  = true;
    }
    else
    {
        rumble_counter = 0;
        rumble_active  = false;
    }

    return 1;
}

bool forcefeedback_is_supported()
{
    return !!rumble.set_rumble_state;
}

