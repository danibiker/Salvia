/***************************************************************************
    Heads-Up Display (HUD) Code
    
    - Score Rendering
    - Timer Rendering
    - Rev Rendering
    - Minimap Rendering
    - Text Rendering
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

enum {
        GREY  = 0x84,
        PINK  = 0x86,
        GREEN = 0x92
    };

#define DIGIT_BASE (0x30)

typedef struct OHud
{
    int colors;
} OHud;

extern OHud ohud;

void OHud_draw_main_hud(OHud* self);

void OHud_draw_fps_counter(OHud* self, int16_t);

void OHud_clear_timetrial_text(OHud* self);

void OHud_do_mini_map(OHud* self);

void OHud_draw_timer1(OHud* self, uint16_t);

void OHud_draw_timer2(OHud* self, uint16_t, uint32_t, uint16_t);

void OHud_draw_lap_timer(OHud* self, uint32_t, uint8_t*, uint8_t);

void OHud_draw_score_ingame(OHud* self, uint32_t);

void OHud_draw_score(OHud* self, uint32_t, const uint32_t, const uint8_t);

void OHud_draw_score_tile(OHud* self, uint32_t, const uint32_t, const uint8_t);

void OHud_draw_stage_number(OHud* self, uint32_t, uint8_t, uint16_t col);

void OHud_draw_rev_counter(OHud* self);

void OHud_draw_debug_info(OHud* self, uint32_t pos, uint16_t height_pat, uint8_t sprite_pat);

void OHud_blit_text1(OHud* self, uint32_t);

void OHud_blit_text1_xy(OHud* self, uint8_t x, uint8_t y, uint32_t src_addr);

void OHud_blit_text2(OHud* self, uint32_t);

void OHud_blit_text_big(OHud* self, const uint8_t Y, const char* text, bool do_notes);

void OHud_blit_text_new(OHud* self, uint16_t, uint16_t, const char* text, uint16_t col);

void OHud_blit_speed(OHud* self, uint32_t, uint16_t);

void OHud_blit_large_digit(OHud* self, uint32_t*, uint8_t);

void OHud_draw_copyright_text(OHud* self);

void OHud_draw_insert_coin(OHud* self);

void OHud_draw_credits(OHud* self);

uint32_t OHud_setup_mini_map(OHud* self);

uint32_t OHud_translate(OHud* self, const uint16_t x, const uint16_t y, const uint32_t BASE_POS);

#ifdef __cplusplus
}
#endif
