/***************************************************************************
    Best Outrunners Name Entry & Display.
    Used in attract mode, and at game end.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

typedef struct score_entry
{
    uint32_t score;
    uint8_t initial1;
    uint8_t initial2;
    uint8_t initial3;
    uint32_t maptiles;
    uint16_t time;
} score_entry;

typedef struct minicar_entry
    {
        int16_t pos;            /* [+0] Word 0: Position */
        int16_t speed;          /* [+2] Word 1: Speed (increments over time) */
        int16_t base_speed;     /* [+4] Word 2: Base Speed */
        int16_t dst_reached;    /* [+6] Word 3: Set when reached destination */
        uint16_t tile_props;    /* [+8] Word 4: Palette/Priority bits for tile */
    } minicar_entry;

enum { NO_SCORES = 20 };

enum {
        STATE_GETPOS,   /* Detect Score Position, Insert Score, Init Table */
        STATE_DISPLAY,  /* Display Basic High Score Table */
        STATE_ENTRY,    /* Init Name Entry */
        STATE_DONE      /* Score Done */
    };

enum { NO_MINICARS = 7 };

#define TILE_PROPS (0x8030)

typedef struct OHiScore
{
    score_entry scores[NO_SCORES];
    uint8_t best_or_state;
    uint8_t state;
    int8_t score_pos;
    int8_t initial_selected;
    int16_t letter_selected;
    int16_t acc_curr;
    int16_t acc_prev;
    int16_t steer;
    uint8_t flash;
    int8_t dest_total;
    int8_t score_display_pos;
    minicar_entry minicars[NO_MINICARS];
    uint16_t laptime[6];
} OHiScore;

extern OHiScore ohiscore;

void OHiScore_init(OHiScore* self);

void OHiScore_init_def_scores(OHiScore* self);

void OHiScore_tick(OHiScore* self);

void OHiScore_setup_pal_best(OHiScore* self);

void OHiScore_setup_road_best(OHiScore* self);

void OHiScore_display_scores(OHiScore* self);

#ifdef __cplusplus
}
#endif
