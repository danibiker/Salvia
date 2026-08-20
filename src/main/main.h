#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "globals.h"

#include "libretro/audio.h"

extern Audio cannonball_audio;

/* Frame counter */
extern int cannonball_frame;

/* Tick Logic. Used when running at non-standard > 30 fps */
extern bool cannonball_tick_frame;

/* FPS Counter */
extern int cannonball_fps_counter;

/* Engine Master State */
extern int cannonball_state;

/* Video geometry/timing refresh (defined in libretro/main.cpp) */
void update_geometry(void);
void update_timing(void);

enum
{
    STATE_BOOT,
    STATE_INIT_MENU,
    STATE_MENU,
    STATE_INIT_GAME,
    STATE_GAME,
    STATE_QUIT
};

#ifdef __cplusplus
}
#endif
