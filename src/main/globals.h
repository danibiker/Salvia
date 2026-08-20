#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <boolean.h>

/* ------------------------------------------------------------------------------------------------ */
/* General useful stuff */
/* ------------------------------------------------------------------------------------------------ */

/* Internal Sega OutRun Screen Properties */
enum { S16_WIDTH = 320 };
enum { S16_HEIGHT = 224 };

/* Internal Widescreen Width */
enum { S16_WIDTH_WIDE = 398 };

/* Palette Address in Memory */
enum { S16_PALETTE_BASE = 0x120000 };

/* Number of Palette Entries */
enum { S16_PALETTE_ENTRIES = 0x1000 };

/* Number of stages */
enum { STAGES = 15 };

/* Hard Coded End Point of every level */
#define ROAD_END (0x79C)

/* End Point of level for CPU1, including horizon */
#define ROAD_END_CPU1 (0x904)

/* Default timer used for hi-score entry */
#define HIGHSCORE_TIMER (0x30)

/* Default timer used for music selection (was 15 seconds on original/old romset) */
#define MUSIC_TIMER (0x30)

enum
{
    BIT_0 = 0x01,
    BIT_1 = 0x02,
    BIT_2 = 0x04,
    BIT_3 = 0x08,
    BIT_4 = 0x10,
    BIT_5 = 0x20,
    BIT_6 = 0x40,
    BIT_7 = 0x80,
    BIT_8 = 0x100,
    BIT_9 = 0x200,
    BIT_A = 0x400
};

#ifdef __cplusplus
}
#endif
