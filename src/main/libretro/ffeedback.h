/***************************************************************************
    Force Feedback (aka Haptic) Support
    
    Ref: http://msdn.microsoft.com/en-us/library/windows/desktop/ee417563%28v=vs.85%29.aspx
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <libretro.h>

/*----------------------------------------------------------------------------- */
/* Function prototypes  */
/*----------------------------------------------------------------------------- */
extern bool forcefeedback_init_rumble_interface(retro_environment_t environ_cb);
extern void forcefeedback_deactivate_rumble();
extern void forcefeedback_update_rumble_interface();
extern void forcefeedback_update_force_limits(int max_force, int min_force, int force_duration);

extern bool forcefeedback_init(int max_force, int min_force, int force_duration);
extern void forcefeedback_close();
extern int  forcefeedback_set(int xdirection, int force);
extern bool forcefeedback_is_supported();

#ifdef __cplusplus
}
#endif
