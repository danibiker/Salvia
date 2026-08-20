/***************************************************************************
    Bonus Points Code.
    
    This is the code that displays your bonus points on completing the game.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

/* Bonus Control */
enum
{
    BONUS_DISABLE = 0x0,    /* 0  = Bonus Mode Disabled */
    BONUS_INIT = 0x4,       /* 4  = Init Bonus Mode */
    BONUS_TICK = 0x8,       /* 8  = Tick Down Bonus Time */
    BONUS_SEQ0 = 0xC,       /* C  = End Seq Animation Stage #0 - Stages used to set Ferrari frames */
    BONUS_SEQ1 = 0x10,      /* 10 = End Seq Animation Stage #1 */
    BONUS_SEQ2 = 0x14,      /* 14 = End Seq Animation Stage #2 */
    BONUS_SEQ3 = 0x18,      /* 18 = End Seq Animation Stage #3 */
    BONUS_END = 0x1C        /* 1C = End Bonus Sequence */
};

/* Bonus State */
enum
{
    BONUS_TEXT_INIT = 0,
    BONUS_TEXT_SECONDS = 1,
    BONUS_TEXT_CLEAR = 2,
    BONUS_TEXT_DONE = 3
};

typedef struct OBonus
{
    /* Bonus Control */
    int8_t bonus_control;

    /* Bonus State */
    int8_t bonus_state;

    /* Timer used by bonus mode logic (Added from Rev. A onwards) */
    int16_t bonus_timer;

    /* Bonus seconds (for seconds countdown at bonus stage) */
    int16_t bonus_secs;

    int16_t bonus_counter;
} OBonus;

extern OBonus obonus;

void OBonus_init(OBonus* self);
void OBonus_do_bonus_text(OBonus* self);

#ifdef __cplusplus
}
#endif
