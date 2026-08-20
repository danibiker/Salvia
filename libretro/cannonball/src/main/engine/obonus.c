/***************************************************************************
    Bonus Points Code.
    
    This is the code that displays your bonus points on completing the game.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "ostats.h"
#include "ohud.h"
#include "outils.h"
#include "obonus.h"

static void OBonus_init_bonus_text(OBonus* self);
static void OBonus_decrement_bonus_secs(OBonus* self);
static void OBonus_blit_bonus_secs(OBonus* self);

OBonus obonus;

void OBonus_init(OBonus* self)
{
    self->bonus_control = BONUS_DISABLE;
    self->bonus_state   = BONUS_TEXT_INIT;

    self->bonus_counter = 0;
}

/* Display Text and countdown time for bonus mode */
/* Source: 0x99E0 */
void OBonus_do_bonus_text(OBonus* self)
{
    switch (self->bonus_state)
    {
        case BONUS_TEXT_INIT:
            OBonus_init_bonus_text(self);
            break;

        case BONUS_TEXT_SECONDS:
            OBonus_decrement_bonus_secs(self);
            break;

        case BONUS_TEXT_CLEAR:
        case BONUS_TEXT_DONE:
            if (self->bonus_counter < 60)
                self->bonus_counter++;
            else
            {
                OHud_blit_text2(&ohud, TEXT2_BONUS_CLEAR1);
                OHud_blit_text2(&ohud, TEXT2_BONUS_CLEAR2);
                OHud_blit_text2(&ohud, TEXT2_BONUS_CLEAR3);
            }
            break;
    }
}

/* Calculate Bonus Seconds. This uses the seconds remaining for every lap raced. */
/* Print stuff to text layer for bonus mode. */
/* */
/* Source: 0x9A9C */
static void OBonus_init_bonus_text(OBonus* self)
{
    self->bonus_state = BONUS_TEXT_SECONDS;
    
    { int16_t time_counter_bak = ostats.time_counter << 8;
    ostats.time_counter = 0x30;

    { uint16_t total_time = 0;
        uint16_t digit_mid;

    if (outrun.cannonball_mode == MODE_ORIGINAL)
    {
        /* Add milliseconds remaining from previous stage times */
        { int i; for (i = 0; i < 5; i++)
        {
            total_time = outils_bcd_add(DEC_TO_HEX[ostats.stage_times[i][2]], total_time);
        } }
    }

    /* Mask on top digit of lap milliseconds */
    total_time &= 0xF0;

    if (total_time)
    {
        time_counter_bak |= (10 - (total_time >> 4));
    }
    /* So 60 seconds remaining on the clock and 3 from lap_seconds would be 0x0603 */

    /* Extract individual 3 digits */
    digit_mid = (time_counter_bak >> 8  & 0x0F) * 10;
    { uint16_t digit_top = (time_counter_bak >> 12 & 0x0F) * 100;
        int8_t count;
        uint32_t dst_addr;
        uint32_t src_addr;
    uint16_t digit_bot = (time_counter_bak & 0x0F);

    /* Write them back to final bonus seconds value */
    self->bonus_secs = digit_bot + digit_mid + digit_top;

    /* Write to text layer */
    OHud_blit_text2(&ohud, TEXT2_BONUS_POINTS); /* Print "BONUS POINTS" */
    OHud_blit_text1(&ohud, TEXT1_BONUS_STOP);   /* Print full stop after Bonus Points text */
    OHud_blit_text1(&ohud, TEXT1_BONUS_SEC);    /* Print "SEC" */
    OHud_blit_text1(&ohud, TEXT1_BONUS_X);      /* Print 'X' symbol after SEC */
    OHud_blit_text1(&ohud, TEXT1_BONUS_PTS);    /* Print "PTS" */

    /* Blit big 100K number */
    src_addr = TEXT1_BONUS_100K;
    dst_addr = 0x11065A;
    count = RomLoader_read8_a(&(roms.rom0), &src_addr);

    { int8_t i; for (i = 0; i <= count; i++)
        OHud_blit_large_digit(&ohud, &dst_addr, (RomLoader_read8_a(&(roms.rom0), &src_addr) - 0x30) << 1); }

    OBonus_blit_bonus_secs(self);
 } } }}

/* Decrement bonus seconds. Blit seconds remaining. */
/* Source: 9A08 */
static void OBonus_decrement_bonus_secs(OBonus* self)
{
    if (self->bonus_counter < 60)
    {
        self->bonus_counter++;
        return;
    }

    /* Play Signal 1 Sound In A Steady Fashion */
    if ((((self->bonus_counter - 1) ^ self->bonus_counter) & BIT_2) == 0)
        OSoundInt_queue_sound(&osoundint, SOUND_SIGNAL1);

    /* Increment Score by 100K points */
    OStats_update_score(&ostats, 0x100000);
    
    /* Blit bonus seconds remaining */
    OBonus_blit_bonus_secs(self);
    
    if (--self->bonus_secs < 0)
    {
        self->bonus_counter = -1;
        self->bonus_state = BONUS_TEXT_CLEAR;
    }
    else
        self->bonus_counter++;

}

/* Blit large yellow second remaining value e.g.: 23.3 */
/* Source: 0x9B7C */
static void OBonus_blit_bonus_secs(OBonus* self)
{
    const uint8_t COL2 = 0x80;
    const uint16_t TILE_DOT = 0x8C2E;
    const uint16_t TILE_ZERO = 0x8420;

    uint32_t d1 = (self->bonus_secs / 100) << 8;
    uint32_t d4 = (self->bonus_secs / 100) * 100;

    uint32_t d2 = (self->bonus_secs - d4) / 10;
    uint32_t d3 = self->bonus_secs - d4;

    d4 = d2;
    d2 <<= 4;
    d4 *= 10;
    d3 -= d4;
    d1 += d2;
    d1 += d3;

    d3 = (d1 & 0xF)   << 1;
    d2 = (d1 & 0xF0)  >> 3;
    d1 = (d1 & 0xF00) >> 7;

    { uint32_t text_addr = 0x110644;

    /* Blit Digit 1 */
    if (d1)
    {
        OHud_blit_large_digit(&ohud, &text_addr, d1);
    }
    else
    {
        Video_write_text16(&video, text_addr, TILE_ZERO);
        Video_write_text16(&video, text_addr | COL2, TILE_ZERO);
        text_addr += 2;        
    }

    /* Blit Digit 2 */
    OHud_blit_large_digit(&ohud, &text_addr, d2);
    /* Blit Dot */
    Video_write_text16(&video, text_addr | COL2, TILE_DOT);
    text_addr += 2;
    /* Blit Digit 3 */
    OHud_blit_large_digit(&ohud, &text_addr, d3);
 }}
