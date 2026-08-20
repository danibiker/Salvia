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

#include <stdio.h>
#include <compat/msvc.h>
#include <stdlib.h>
#include <string.h>

#include "../utils.h"
#include "engine/oferrari.h"
#include "engine/outils.h"
#include "engine/ohud.h"
#include "engine/ooutputs.h"
#include "engine/ostats.h"

static void OHud_draw_mini_map(OHud* self, uint32_t);

OHud ohud;




/* Draw Text Labels For HUD */
/*  */
/* Source: 0xB462 */
void OHud_draw_main_hud(OHud* self)
{
    OHud_blit_text1(self, HUD_LAP1);
    OHud_blit_text1(self, HUD_LAP2);

    if (outrun.cannonball_mode == MODE_ORIGINAL)
    {
        OHud_blit_text1(self, HUD_TIME1);
        OHud_blit_text1(self, HUD_TIME2);
        OHud_blit_text1(self, HUD_SCORE1);
        OHud_blit_text1(self, HUD_SCORE2);
        OHud_blit_text1(self, HUD_STAGE1);
        OHud_blit_text1(self, HUD_STAGE2);
        OHud_blit_text1(self, HUD_ONE);
        OHud_do_mini_map(self);
    }
    else if (outrun.cannonball_mode == MODE_TTRIAL)
    {
        OHud_draw_score(self, OHud_translate(self, 3, 2, 0x110030), 0, 2);
        OHud_blit_text1_xy(self, 2, 1, HUD_SCORE1);
        OHud_blit_text1_xy(self, 2, 2, HUD_SCORE2);
        OHud_blit_text_big(self, 4, "TIME TO BEAT", false);
        OHud_draw_lap_timer(self, OHud_translate(self, 16, 7, 0x110030), outrun.ttrial.best_lap, outrun.ttrial.best_lap[2]);
    }
    else if (outrun.cannonball_mode == MODE_CONT)
    {
        OHud_blit_text1(self, HUD_TIME1);
        OHud_blit_text1(self, HUD_TIME2);
        OHud_blit_text1(self, HUD_SCORE1);
        OHud_blit_text1(self, HUD_SCORE2);
        OHud_blit_text1(self, HUD_STAGE1);
        OHud_blit_text1(self, HUD_STAGE2);
        OHud_blit_text1(self, HUD_ONE);
    }
}

void OHud_clear_timetrial_text(OHud* self)
{
    OHud_blit_text_big(self, 4,     "            ", false);
    OHud_blit_text_new(self, 16, 7, "            ", GREY);
}

void OHud_draw_fps_counter(OHud* self, int16_t fps)
{
    char str[24]; snprintf(str, sizeof(str), "FPS %d", fps);
    OHud_blit_text_new(self, 30, 0, str, GREY);
}


/* Routine to setup and draw mini-map (bottom RHS of HUD) */
/* */
/* Source: 0x8B52 */
void OHud_do_mini_map(OHud* self)
{
    if (outrun.game_state == GS_ATTRACT)
        return;
    
    { uint32_t tile_addr = OHud_setup_mini_map(self);
    OHud_draw_mini_map(self, tile_addr);
 }}

/* Setup Appropriate Tile Address For Minimap. */
/* */
/* Returns start address of block of 4 tiles. Represents square of route on mini-map screen. */
/*  */
/* Source: 0x8B68 */
uint32_t OHud_setup_mini_map(OHud* self)
{
    const uint8_t ROUTE_MAPPING[] =
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06,
        0x07, 0x07, 0x08, 0x08, 0x09, 0x09, 0x0A, 0x0A, 0x0B, 0x0B, 0x0C, 0x0C, 0x0D, 0x0D, 0x0E, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
    };
    if (ostats.route_info > 0x4F)
        ostats.route_info = 0x4F;

    /* Map Route to appropriate tile */

    return TILES_MINIMAP + (ROUTE_MAPPING[ostats.route_info] << 2);
}static  

void OHud_draw_mini_map(OHud* self, uint32_t tile_addr)
{
    /* Address in text ram to copy this to */
    uint32_t dst = 0x110CFA;

    /* Base Tile to use */
    const uint16_t BASE = 0x8B00;

    uint16_t tile = (BASE | RomLoader_read8_a(&(roms.rom0), &tile_addr));
    Video_write_text16(&video, dst, tile);

    tile = BASE | RomLoader_read8_a(&(roms.rom0), &tile_addr);
    Video_write_text16(&video, 2 + dst, tile);

    tile = BASE | RomLoader_read8_a(&(roms.rom0), &tile_addr);
    Video_write_text16(&video, 0x80 + dst, tile);

    tile = BASE | RomLoader_read8_a(&(roms.rom0), &tile_addr);
    Video_write_text16(&video, 0x82 + dst, tile);
}

/* Print Timer To Top Left Hand Corner Of Screen */
/* */
/* Source: 0x8216 */
void OHud_draw_timer1(OHud* self, uint16_t time)
{
    if (outrun.game_state < GS_START1 || outrun.game_state > GS_INGAME)
        return;

    if (!outrun.freeze_timer)
    {
        const uint16_t BASE_TILE = 0x8C80;
        OHud_draw_timer2(self, time > 0x99 ? 0x99 : time, 0x1100BE, BASE_TILE);

        /* Blank out the OFF text area */
        Video_write_text16(&video, 0x110C2, 0);
        Video_write_text16(&video, 0x110C2 + 0x80, 0);
    }
    else
    {
        uint32_t dst_addr = OHud_translate(self, 7, 1, 0x110030);
        const uint16_t PAL = 0x8AA0;
        const uint16_t O = (('O' - 0x41) * 2) + PAL; /* Convert character to real index (D0-0x41) so A is 0x01 */
        const uint16_t F = (('F' - 0x41) * 2) + PAL;
        
        Video_write_text16_a(&video, &dst_addr,       O);     /* Write first row to text ram */
        Video_write_text16(&video, 0x7E + dst_addr, O + 1); /* Write second row to text ram */
        Video_write_text16_a(&video, &dst_addr,       F);     /* Write first row to text ram */
        Video_write_text16(&video, 0x7E + dst_addr, F + 1); /* Write second row to text ram */
        Video_write_text16_a(&video, &dst_addr,       F);     /* Write first row to text ram */
        Video_write_text16(&video, 0x7E + dst_addr, F + 1); /* Write second row to text ram */
    }
}

/* Called directly by High Score Table */
/* */
/* Source: 0x8234 */
void OHud_draw_timer2(OHud* self, uint16_t time_counter, uint32_t addr, uint16_t base_tile)
{
    uint16_t digit1 = time_counter & 0xF;
    uint16_t digit2 = (time_counter & 0xF0) >> 4;

    /* Low Digit */
    uint16_t value = (digit1 << 1) + base_tile;
    Video_write_text16(&video, 0x02 + addr, value);
    Video_write_text16(&video, 0x82 + addr, value+1);

    /* High Digit */
    value = (digit2 << 1);

    if (value)
    {
        value += base_tile;
        Video_write_text16(&video, 0x00 + addr, value);
        Video_write_text16(&video, 0x80 + addr, value+1);
    }
    else
    {
        Video_write_text16(&video, 0x00 + addr, 0);
        Video_write_text16(&video, 0x80 + addr, 0);
    }
}

void OHud_draw_lap_timer(OHud* self, uint32_t addr, uint8_t* digits, uint8_t ms_value)
{
    const uint16_t BASE = 0x8230;
    const uint16_t APOSTROPHE1 = 0x835E;
    const uint16_t APOSTROPHE2 = 0x835F;

    /* Write Minute Digit */
    Video_write_text16_a(&video, &addr, BASE | digits[0]);
    Video_write_text16_a(&video, &addr, APOSTROPHE1);

    /* Write Seconds */
    Video_write_text16_a(&video, &addr, BASE | (digits[1] & 0xF0) >> 4);
    Video_write_text16_a(&video, &addr, BASE | (digits[1] & 0xF));
    Video_write_text16_a(&video, &addr, APOSTROPHE2);

    /* Write Milliseconds */
    Video_write_text16_a(&video, &addr, BASE | (ms_value & 0xF0) >> 4);
    Video_write_text16_a(&video, &addr, BASE | (ms_value & 0xF));
}

/* Draw Score (In-Game HUD) */
/* */
/* Source: 0x7382 */
void OHud_draw_score_ingame(OHud* self, uint32_t score)
{
    if (outrun.game_state < GS_START1 || outrun.game_state > GS_BONUS)
        return;

    OHud_draw_score(self, 0x110150, score, 2);
}

/* Draw Score */
/* */
/* + 8530 = Digit 0 */
/* + 8531 = Digit 1 */
/* */
/* Source: 0x7146 */
void OHud_draw_score(OHud* self, uint32_t addr, const uint32_t score, uint8_t font)
{
    /* Base address of Digit 0 setup here */
    const uint16_t BASE = 0x30 | (font << 9) | 0x8100;

    /* Blank tile for comparison purposes */
    const uint16_t BLANK = 0x8020;

    uint8_t* digits = (uint8_t*)malloc((8) * sizeof(uint8_t));

    /* Topmost digit */
    digits[0] = ((score >> 16) & 0xF000) >> 12;
    digits[1] = ((score >> 16) & 0xF00) >> 8;
    digits[2] = ((score >> 16) & 0xF0) >> 4;
    digits[3] = ((score >> 16) & 0xF);
    digits[4] = (score & 0xF000) >> 12;
    digits[5] = (score & 0xF00) >> 8;
    digits[6] = (score & 0xF0) >> 4;
    digits[7] = (score & 0xF);

    { bool found = false;

    /* Draw blank digits until we find first digit */
    /* Then use zero for blank digits */
    { uint8_t i; for (i = 0; i < 7; i++)
    {
        if (!found && !digits[i])
            Video_write_text16_a(&video, &addr, BLANK);
        else
        {
            Video_write_text16_a(&video, &addr, digits[i] + BASE);
            found = true;
        }
    } }

    Video_write_text16_a(&video, &addr, digits[7] + BASE); /* Always draw last digit */
    free(digits);
 }}

/* Same as above function but writes to tile ram instead. */
/* Not an ideal solution, really a workaround because text and tile ram are not one big lump in my implementation */
void OHud_draw_score_tile(OHud* self, uint32_t addr, const uint32_t score, uint8_t font)
{
    /* Base address of Digit 0 setup here */
    const uint16_t BASE = 0x30 | (font << 9) | 0x8100;

    /* Blank tile for comparison purposes */
    const uint16_t BLANK = 0x8020;

    uint8_t* digits = (uint8_t*)malloc((8) * sizeof(uint8_t));

    /* Topmost digit */
    digits[0] = ((score >> 16) & 0xF000) >> 12;
    digits[1] = ((score >> 16) & 0xF00) >> 8;
    digits[2] = ((score >> 16) & 0xF0) >> 4;
    digits[3] = ((score >> 16) & 0xF);
    digits[4] = (score & 0xF000) >> 12;
    digits[5] = (score & 0xF00) >> 8;
    digits[6] = (score & 0xF0) >> 4;
    digits[7] = (score & 0xF);

    { bool found = false;

    /* Draw blank digits until we find first digit */
    /* Then use zero for blank digits */
    { uint8_t i; for (i = 0; i < 7; i++)
    {
        if (!found && !digits[i])
            Video_write_tile16_a(&video, &addr, BLANK);
        else
        {
            Video_write_tile16_a(&video, &addr, digits[i] + BASE);
            found = true;
        }
    } }

    Video_write_tile16_a(&video, &addr, digits[7] + BASE); /* Always draw last digit */
    free(digits);
 }}

/* Modified Version Of Draw Digits */
/* */
/* Source: C3A0 */
void OHud_draw_stage_number(OHud* self, uint32_t addr, uint8_t digit, uint16_t col)
{
    if (digit < 10)
    {
        Video_write_text16(&video, addr, digit + (col << 8) + DIGIT_BASE);
    }
    else
    {
        int hex = outils_convert16_dechex(digit);

        Video_write_text16(&video, addr + 2, (hex & 0xF) + (col << 8) + DIGIT_BASE);
        Video_write_text16(&video, addr    , (hex >> 4)  + (col << 8) + DIGIT_BASE);
    }
}

/* Draw Rev Counter */
/* */
/* Source: 0x6B08 */
void OHud_draw_rev_counter(OHud* self)
{
    uint16_t revs;
    /* Return in attract mode and don't draw rev counter */
    if (outrun.game_state <= GS_INIT_GAME) return;
    revs = oferrari.rev_stop_flag ? oferrari.revs_post_stop : oferrari.revs >> 16;
    
    /* Boost revs during countdown phase, so the bar goes further into the red */
    if (oinitengine.car_increment >> 16 == 0)
        revs += (revs >> 2);

    revs >>= 4;

    { uint32_t addr = 0x110DB4; /* Address of rev counter */
        
    const uint16_t REV_OFF = 0x8120; /* Rev counter: Off (Blank Tile) */
    const uint16_t REV_ON1 = 0x81FE; /* Rev counter: On (Single Digit) */
    const uint16_t REV_ON2 = 0x81FD; /* Rev counter: On (Double Digit) */
    const uint16_t GREEN = 0x200;
    const uint16_t WHITE = 0x400;
    const uint16_t RED   = 0x600;

    { int8_t i; for (i = 0; i <= 0x13; i++)
    {
        uint16_t tile = 0;
        
        if (revs > i)
        {
            tile = REV_ON2; 
            if (i >= 0xE) tile |= RED;
            else if (i <= 9) tile |= WHITE;
            else tile |= GREEN;
        }
        else if (revs != i)
        {
            tile = REV_OFF | WHITE; 
        }
        else
        {
            tile = REV_ON1;
            if (i >= 0xE) tile |= RED;
            else if (i <= 9) tile |= WHITE;
            else tile |= GREEN;
        }
        
        Video_write_text16(&video, addr, tile);

        /* On odd indexes, we don't increment to next word - to effectively shorten the length of the rev counter */
        /* It would be twice as long otherwise */
        if (i & 1)
            addr += 2;
    } }
    oferrari.rev_pitch2 = oferrari.rev_pitch1;
 }}

/* Convert & Blit car speed to screen */
/* */
/* Source: 0xBB72 */
void OHud_blit_speed(OHud* self, uint32_t dst_addr, uint16_t speed)
{
    const uint16_t TILE_BASE = 0x8C60; /* Base tile number */

    /* Convert to human readable speed */
    speed = outils_convert16_dechex(speed);

    { uint16_t digit1 = speed & 0xF;
    uint16_t digit2 = (speed & 0xF0) >> 4;
    uint16_t digit3 = (speed & 0xF00) >> 8;

    digit3 <<= 1;
    if (digit3 == 0)
    {
        digit2 <<= 1;
        if (digit2 != 0)
            digit2 += TILE_BASE;
    }
    else
    {
        digit3 += TILE_BASE;
        digit2 <<= 1;
        digit2 += TILE_BASE;
    }
    /* Blit Top Line Of Tiles */
    digit1 <<= 1;
    digit1 += TILE_BASE;
    
    Video_write_text16_a(&video, &dst_addr, digit3);
    Video_write_text16_a(&video, &dst_addr, digit2);
    Video_write_text16(&video, dst_addr, digit1);
    dst_addr += 0x7C; /* Set to next horizontal line of number tiles */

    /* Blit Bottom Line Of Tiles */
    if (digit3 != 0) digit3++;
    if (digit2 != 0) digit2++;
    digit1++;
    Video_write_text16_a(&video, &dst_addr, digit3);
    Video_write_text16_a(&video, &dst_addr, digit2);
    Video_write_text16(&video, dst_addr, digit1);
 }}

/* Blit large digit spanning two rows. */
/* */
/* Source: 0x9BF2 */
void OHud_blit_large_digit(OHud* self, uint32_t* addr, uint8_t digit)
{
    Video_write_text16(&video, *addr,        (digit + 0x80) | 0x8C00);
    Video_write_text16(&video, *addr + 0x80, (digit + 0x81) | 0x8C00);

    *addr += 2;
}

/* Draw Copyright Text to text ram */
/*  */
/* Source Address: 0xB844 */
/* Input:          None */
/* Output:         None */

void OHud_draw_copyright_text(OHud* self)
{
    OHud_blit_text1(self, TEXT1_1986_SEGA);
    OHud_blit_text1(self, TEXT1_COPYRIGHT);
}

/* Draw Insert Coin text */
/* */
/* Source: 0xB7D0 */
void OHud_draw_insert_coin(OHud* self)
{
    /* Update text */
    if ((outrun.tick_counter ^ (outrun.tick_counter - 1)) & BIT_4)
    {
        /* Flash Press Start */
        if (ostats.credits)
        {
            if (outrun.tick_counter & BIT_4)
            {
                OHud_blit_text1(self, TEXT1_PRESS_START);
                OOutputs_set_digital(outrun.outputs, D_START_LAMP);
            }
            else
            {
                OHud_blit_text1(self, TEXT1_CLEAR_START);
                OOutputs_clear_digital(outrun.outputs, D_START_LAMP);
            }
        }
        /* Flash Insert Coins / Freeplay Press Start */
        else
        {
            if (config.engine.freeplay)
            {
                uint32_t dst_addr = 0x110ACC;
                const static uint8_t PRESS_START[] = {0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x28};
                
                if (outrun.tick_counter & BIT_4)
                {                
                    /* Blit each tile */
                    { uint16_t i; for (i = 0; i < sizeof(PRESS_START); i++)
                        Video_write_text16_a(&video, &dst_addr, (0x8700 | PRESS_START[i])); }
                }
                else
                {
                    { uint16_t i; for (i = 0; i < sizeof(PRESS_START); i++)
                        Video_write_text16_a(&video, &dst_addr, (0x8700 | 0x20)); }
                }
            }
            else
            {
                OHud_blit_text1(self, (outrun.tick_counter & BIT_4) ? TEXT1_INSERT_COINS : TEXT1_CLEAR_COINS);
            }
        }
    }
}

/* Source: 0x6CDE */
void OHud_draw_credits(OHud* self)
{
    if (config.engine.freeplay)
    {
        OHud_blit_text1(self, TEXT1_FREEPLAY);
    }
    else
    {
        Video_write_text16(&video, 0x110D44, ostats.credits | 0x8630); /* blit digit */
        OHud_blit_text1(self, ostats.credits >= 2 ? TEXT1_CREDITS : TEXT1_CREDIT);
    }
}

/* Blit Tiles to text ram layer (Single Row) */
/*  */
/* Source Address: 0xB844 */
/* Input:          Source address in rom of data format */
/* Output:         None */
/* */
/* Format of the input data is as follows: */
/* */
/* Long 1: Destination address to move data to. [e.g. 0x00110D34 which would go to the text ram] */
/* Word 1: Number of tiles to draw / counter */
/* Byte 1: High byte to apply to every copy, containing priority info [86] */
/* Byte 2: Not used */
/* */
/* Byte 3: Second byte to copy containing tile number (low) */
/* Byte 4: Third byte to copy containing tile number (low) */
/* Byte 5: etc. */
/* */
/* Text layer name table format: */
/* */
/* MSB          LSB */
/* p???cccnnnnnnnnn */
/*  */
/* p : Priority. If 0, sprites with priority level 3 are shown over the text. */
/*               If 1, the text layer is shown over sprites regardless of priority. */
/* c : Color palette */
/* n : Tile index to use */
/* ? : Unknown */

void OHud_blit_text1(OHud* self, uint32_t src_addr)
{
    uint32_t dst_addr = RomLoader_read32_a(&(roms.rom0), &src_addr); /* Text RAM destination address */
    uint16_t counter = RomLoader_read16_a(&(roms.rom0), &src_addr);  /* Number of tiles to blit */
    uint16_t data = RomLoader_read16_a(&(roms.rom0), &src_addr);     /* Tile data to blit */
    
    /* Blit each tile */
    { uint16_t i; for (i = 0; i <= counter; i++)
    {
        data = (data & 0xFF00) | RomLoader_read8_a(&(roms.rom0), &src_addr);
        Video_write_text16_a(&video, &dst_addr, data);
    } }
}

void OHud_blit_text1_xy(OHud* self, uint8_t x, uint8_t y, uint32_t src_addr)
{
    uint32_t dst_addr = OHud_translate(self, x, y, 0x110030);
    src_addr += 4;
    { uint16_t counter = RomLoader_read16_a(&(roms.rom0), &src_addr);  /* Number of tiles to blit */
    uint16_t data = RomLoader_read16_a(&(roms.rom0), &src_addr);     /* Tile data to blit */

    /* Blit each tile */
    { uint16_t i; for (i = 0; i <= counter; i++)
    {
        data = (data & 0xFF00) | RomLoader_read8_a(&(roms.rom0), &src_addr);
        Video_write_text16_a(&video, &dst_addr, data);
    } }
 }}

/* Blit Tiles to text ram layer (Double Row) */
/*  */
/* Source Address: 0xB844 */
/* Input:          Source address in rom of data format */
/* Output:         None */
/* */
/* Format of the input data is as follows: */
/* */
/* Byte 0: Offset into text RAM [First 2 bytes] */
/* Byte 1: Offset into text RAM [First 2 bytes] */
/* Byte 2: Palette to use */
/* Byte 3: Number of characters to display (used as a counter) */
/* Byte 4: Start of text to display */


void OHud_blit_text2(OHud* self, uint32_t src_addr)
{
    uint16_t counter;
    uint32_t dst_addr = 0x110000 + RomLoader_read16_a(&(roms.rom0), &src_addr); /* Text RAM destination address */

    uint16_t pal = RomLoader_read8_a(&(roms.rom0), &src_addr); 
    pal = 0x80A0 | ((pal << 9) | ((pal >> 7) & 1));
    /* same as ror 7 and extending to word */
    counter = RomLoader_read8_a(&(roms.rom0), &src_addr);

    /* Blit each tile */
    { uint16_t i; for (i = 0; i <= counter; i++)
    {
        uint16_t data = RomLoader_read8_a(&(roms.rom0), &src_addr); /* Tile data to blit */
        
        /* Blank space */
        if (data == 0x20)
        {
            data = 0;
            Video_write_text16_a(&video, &dst_addr, data); /* Write blank space to text ram */
        }
        /* Normal character */
        else
        {
            /* Convert character to real index (D0-0x41) so A is 0x01 */
            data -= 0x41;
            data = (data * 2) + pal;
            Video_write_text16_a(&video, &dst_addr, data); /* Write first row to text ram */
            data++;
        }
        Video_write_text16(&video, 0x7E + dst_addr, data); /* Write second row to text ram */
    } }
}

/* ------------------------------------------------------------------------------------------------ */
/* Enhanced Cannonball Routines Below */
/* ------------------------------------------------------------------------------------------------ */

void OHud_draw_debug_info(OHud* self, uint32_t pos, uint16_t height_pat, uint8_t sprite_pat)
{
    OHud_blit_text_new(&ohud, 0,  4, "LEVEL POS", GREEN);
    OHud_blit_text_new(&ohud, 16, 4, "    ", GREY);
    OHud_blit_text_new(&ohud, 16, 4, Utils_to_string((int)(pos >> 16)), PINK);
    OHud_blit_text_new(&ohud, 0,  5, "HEIGHT PATTERN", GREEN);
    OHud_blit_text_new(&ohud, 16, 5, "    ", GREY);
    OHud_blit_text_new(&ohud, 16, 5, Utils_to_string((int)height_pat), PINK);
    OHud_blit_text_new(&ohud, 0,  6, "SPRITE PATTERN", GREEN);
    OHud_blit_text_new(&ohud, 16, 6, "    ", GREY);
    OHud_blit_text_new(&ohud, 16, 6, Utils_to_string((int)sprite_pat), PINK);
}

/* Big Yellow Text. Always Centered.  */
void OHud_blit_text_big(OHud* self, const uint8_t Y, const char* text, bool do_notes)
{
    uint32_t dst_addr;
    uint16_t length = (uint16_t) strlen(text);

    const uint16_t X = 20 - (length >> 1);

    /* Clear complete row in text ram before blitting */
    { uint8_t x; for (x = 0; x < 40; x++)
    {
        Video_write_text16(&video, OHud_translate(self, x, Y, 0x110030) + 0x110000, 0); /* Write blank space to text ram */
        Video_write_text16(&video, OHud_translate(self, x, Y, 0x110030) + 0x11007E, 0); /* Write blank space to text ram */
    } }

    /* Draw Notes */
    if (do_notes)
    {
        /* Note tiles to append to left side of text */
        const uint32_t NOTE_TILES1 = 0x8A7A8A7B;
        const uint32_t NOTE_TILES2 = 0x8A7C8A7D;

        Video_write_text32(&video, OHud_translate(self, X - 2, Y, 0x110030) + 0x110000, NOTE_TILES1);
        Video_write_text32(&video, OHud_translate(self, X - 2, Y, 0x110030) + 0x110080, NOTE_TILES2);
    }

    dst_addr = OHud_translate(self, X, Y, 0x110030) + 0x110000;

    /* Blit each tile */
    { uint16_t i; for (i = 0; i < length; i++)
    {
        char c = *text++;
        /* Convert lowercase characters to uppercase */
        if (c >= 'a' && c <= 'z')
            c -= 0x20;
        /* Numerals: Use different palette for numbers so they display more nicely */
        else if (c >= '0' && c <= '9')
        {
            const uint16_t pal = 0x8CA0;
            c -= 0x40;
            c = (c * 2);
            Video_write_text16_a(&video, &dst_addr,       c + pal);     /* Write first row to text ram */
            Video_write_text16(&video, 0x7E + dst_addr, c + pal + 1); /* Write second row to text ram */
        }
        /* Blank space */
        else if (c == ' ')
        {
            c = 0;
            Video_write_text16_a(&video, &dst_addr, c); /* Write blank space to text ram */
            Video_write_text16(&video, 0x7E + dst_addr, c); /* Write blank space to text ram */
        }
        /* Normal character */
        if (c >= 'A' && c <= 'Z')
        {           
            const uint16_t pal = do_notes ? 0x8AA0 : 0x8CA0;
            /* Convert character to real index (D0-0x41) so A is 0x01 */
            c -= 0x41;
            c = (c * 2);
            Video_write_text16_a(&video, &dst_addr,       c + pal);     /* Write first row to text ram */
            Video_write_text16(&video, 0x7E + dst_addr, c + pal + 1); /* Write second row to text ram */
        }       
    } }
}

/* Custom Routine To Blit Text Easily */
/* */
/* The name table is 64x28, but only 40x28 is shown. The viewable portion of */
/* the name table starts at column 24 and goes to column 63, which maps to */
/* screen columns 0 through 39. */
/* */
/* Normal font: 41 onwards */
void OHud_blit_text_new(OHud* self, uint16_t x, uint16_t y, const char* text, uint16_t pal)
{
    uint32_t dst_addr = OHud_translate(self, x, y, 0x110030); 
    uint16_t length = (uint16_t) strlen(text);

    { uint16_t i; for (i = 0; i < length; i++)
    {
        char c = *text++;

        /* Convert lowercase characters to uppercase */
        if (c >= 'a' && c <= 'z')
            c -= 0x20;
        else if ((unsigned char)(c) == 0xA9)
            c = 0x10;
        else if (c == '-')
            c = 0x2d;
        else if (c == '.')
            c = 0x5b;

        Video_write_text16_a(&video, &dst_addr, (pal << 8) | c);
    } }
}

/* Translate x, y column position to tilemap address */
/* Base Position defaults to 0,0 */
uint32_t OHud_translate(OHud* self, uint16_t x, uint16_t y, const uint32_t BASE_POS)
{
    if (x > 63) x = 63;
    if (y > 27) y = 27;

    /* Calculate destination address based on x, y position */
    return BASE_POS + ((x + (y * 64)) << 1);
}