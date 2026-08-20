/***************************************************************************
    Best Outrunners Name Entry & Display.
    Used in attract mode, and at game end.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "../setup.h"
#include "main.h"
#include "ohud.h"
#include "oinputs.h"
#include "ostats.h"
#include "outils.h"
#include "ohiscore.h"

static void OHiScore_get_score_pos(OHiScore* self);
static void OHiScore_insert_score(OHiScore* self);
static void OHiScore_set_display_pos(OHiScore* self);
static void OHiScore_check_name_entry(OHiScore* self);
static uint32_t OHiScore_get_score_adr(OHiScore* self);
static void OHiScore_blit_alphabet(OHiScore* self);
static void OHiScore_flash_entry(OHiScore* self, uint32_t adr);
static void OHiScore_do_input(OHiScore* self, uint32_t adr);
static int8_t OHiScore_read_controls(OHiScore* self);
static void OHiScore_setup_minicars(OHiScore* self);
static void OHiScore_tick_minicars(OHiScore* self);
static void OHiScore_setup_minicars_pal(OHiScore* self, minicar_entry*);
static void OHiScore_blit_score_table(OHiScore* self);
static void OHiScore_blit_scores(OHiScore* self);
static void OHiScore_blit_digit(OHiScore* self);
static void OHiScore_blit_initials(OHiScore* self);
static void OHiScore_blit_route_map(OHiScore* self);
static void OHiScore_blit_lap_time(OHiScore* self);
static void OHiScore_convert_lap_time(OHiScore* self, uint16_t);

OHiScore ohiscore;



/* Clear score variables (not scores themselves) */
/* */
/* Source: 0xCE74 */
void OHiScore_init(OHiScore* self)
{
    /*ostats.score = 0x04100000; // hack */

    self->best_or_state     = 0;
    self->state             = STATE_GETPOS;
    self->score_pos         = -1;
    self->initial_selected  = 0;
    self->letter_selected   = 0;
    self->acc_curr          = 0;
    self->acc_prev          = 0;
    self->flash             = 0;
    self->score_display_pos = 0;
    self->dest_total        = 0;
}

/* Setup palette for Best Outrunners High Score Entry */
/* This is the shaded red background for the hi-score entry */
/* Source: 0x360C */
void OHiScore_setup_pal_best(OHiScore* self)
{
    uint32_t src = PAL_BESTOR;
    uint32_t dst = 0x120F00;

    { int i; for (i = 0; i <= 0x1F; i++)
        Video_write_pal32_a(&video, &dst, RomLoader_read32_a(&(roms.rom0), &src)); }
}

/* Setup road colour for Best Outrunners High Score Entry */
/* This is a pure black road for the hi-score entry */
/* Source: 0x3624 */
void OHiScore_setup_road_best(OHiScore* self)
{
    uint32_t dst = 0x120800;

    { int i; for (i = 0; i <= 0x1F; i++)
        Video_write_pal32_a(&video, &dst, 0); }
}

/* Initalize Default Score Table */
/* Source: 0xD17A */
void OHiScore_init_def_scores(OHiScore* self)
{
    uint32_t adr = DEFAULT_SCORES;

    { int i; for (i = 0; i < NO_SCORES; i++)
    {
        uint32_t initials;
        /* Read default score */
        self->scores[i].score = RomLoader_read32_a(&(roms.rom0), &adr);

        /* Read initials */
        initials = RomLoader_read32_a(&(roms.rom0), &adr);
        self->scores[i].initial1 = (initials >> 24) & 0xFF;
        self->scores[i].initial2 = (initials >> 16) & 0xFF;
        self->scores[i].initial3 = (initials >> 8) & 0xFF;

        /* Read default time */
        self->scores[i].time = RomLoader_read16_a(&(roms.rom0), &adr);
        /*scores[i].time = (i & 1) ? 0x4321 : 0x1234; // hack to display 4m 43 51 or 1m 16 56 */
        /* Read map tiles */
        self->scores[i].maptiles = RomLoader_read32_a(&(roms.rom0), &adr);
        /*scores[i].maptiles = 0xe5c8c2d1; // hack to populate map tiles for testing */
    } }
}

/* Hi Score Processing Logic */
/* */
/* Source: 0xD1C4 */
void OHiScore_tick(OHiScore* self)
{
    switch (self->state & 3)
    {
        /* Detect Score Position, Insert Score, Init Table */
        case STATE_GETPOS:   
            OHiScore_get_score_pos(self);

            /* New High Score */
            if (self->score_pos != -1)
            {
                OSoundInt_queue_sound(&osoundint, SOUND_PCM_WAVE);
                OSoundInt_queue_sound(&osoundint, SOUND_MUSIC_LASTWAVE);
                OHiScore_insert_score(self);               
            }
            /* Not a High Score */
            else
            {
                ostats.time_counter = 5;
            }
            OHiScore_set_display_pos(self);
            self->acc_prev = -1;
            self->state = STATE_DISPLAY;
            video.enabled = true;
            break;

        /* Display Basic High Score Table */
        case STATE_DISPLAY: 
            OHiScore_display_scores(self);
            if (self->best_or_state >= 2)
                self->state = STATE_ENTRY; /* Only allow name entry when minicars have animation finished */
            break;

        /* Init Name Entry */
        case STATE_ENTRY:
            OHiScore_check_name_entry(self);
            break;

        /* Score Done */
        case STATE_DONE:
            return;
    }
}

/* Calculate high score position. */
/* Source: D318 */static 
void OHiScore_get_score_pos(OHiScore* self)
{
    { int i; for (i = 0; i < NO_SCORES; i++)
    {
        if (ostats.score > self->scores[i].score)
        {
            self->score_pos = i;
            OHiScore_set_display_pos(self);
            return;
        }
    } }

    self->score_pos = -1; /* Not a new high-score */
}

/* - Insert Score Entry */
/* - Move Other Entries Down In Memory */
/* - Calculate Completed Time */
/* - Setup Appropriate Minimap Tiles */
/* */
/* Source: 0xD2C0 */static 
void OHiScore_insert_score(OHiScore* self)
{
    /* Move entries down in memory */
    { int i; for (i = NO_SCORES - 1; i > self->score_pos; i--)
    {
        self->scores[i] = self->scores[i-1];
    } }

    self->scores[self->score_pos].score    = ostats.score;
    self->scores[self->score_pos].initial1 = 0x20;
    self->scores[self->score_pos].initial2 = 0x20;
    self->scores[self->score_pos].initial3 = 0x20;

    /* Calculate total time if game completed. Store result in $20 */
    if (ostats.game_completed)
    {
        const uint8_t entries = outrun.cannonball_mode == MODE_ORIGINAL ? 5 : 15;

        self->scores[self->score_pos].time = 0;

        { int i; for (i = 0; i < entries; i++)
            self->scores[self->score_pos].time += ostats.stage_counters[i]; }
    }
    else
    {
        self->scores[self->score_pos].time = 0;
        ostats.game_completed = false;
    }

    /* Setup Appropriate Minimap Tiles */
    self->scores[self->score_pos].maptiles = RomLoader_read32(&(roms.rom0), OHud_setup_mini_map(&ohud));
}

/* Set Table Position To Display Score From. Store Result in $26 */
/* Source: 0xD298 */static 
void OHiScore_set_display_pos(OHiScore* self)
{
    if (self->score_pos < 0)
    {
        self->score_display_pos = 13;
    }
    else
    {
        self->score_display_pos = self->score_pos - 3;

        if (self->score_display_pos < 0)
            self->score_display_pos = 0;
        else if (self->score_display_pos > 13)
            self->score_display_pos = 13;
    } 
    /*score_display_pos = 0; // HACK! */
}

/* Check whether to perform name entry. */
/* Print alphabet and other stuff if necessary. */
/* Source: 0xD252 */static 
void OHiScore_check_name_entry(OHiScore* self)
{
    /* No High Score */
    if (self->score_pos == -1)
    {
        OHud_blit_text1(&ohud, TEXT1_YOURSCORE);
        OHud_draw_score(&ohud, 0x110BDA, ostats.score, 3); /* Select font 3 and print score */
        self->state = STATE_DONE;
    }
    else
    {
        uint16_t BIG_RED_FONT;
        /* Get text ram address of score to blit */
        uint32_t score_adr = OHiScore_get_score_adr(self);
        /* Blit Alphabet. Highlight selected letter red. */
        OHiScore_blit_alphabet(self);
        /* Flash current initial that is being entered */
        OHiScore_flash_entry(self, score_adr);   
        /* Draw big red countdown timer */
        BIG_RED_FONT = 0x8080;
        OHud_draw_timer2(&ohud, ostats.time_counter, 0x1101EC, BIG_RED_FONT);
        /* Input from controls */
        OHiScore_do_input(self, score_adr);
        
        /* Save new score info */
        if (self->state == STATE_DONE)
            Config_save_scores(&config, 
                outrun.cannonball_mode == MODE_ORIGINAL
                    ? FILENAME_SCORES
                    : FILENAME_CONT);
    }
}

/* Get Address in text ram at which to output score */
/* Source: 0xD542 */static 
uint32_t OHiScore_get_score_adr(OHiScore* self)
{
    if (self->score_pos < 3)
        return 0x110452 + (self->score_pos << 8); /* top 3 positions */
    if (self->score_pos >= 17)
        return 0x110A52 + ((self->score_pos - 19) << 8); /* last 3 positions */

    return 0x110752; /* middle positions */
}

/* Blit Alphabet. Highlight selected letter red. */
/* Source: 0xD45A */static 
void OHiScore_blit_alphabet(OHiScore* self)
{
    uint16_t RED;
    uint32_t adr;
    /* Print Text: "ABCDEFGHIJK..." */
    OHud_blit_text2(&ohud, TEXT2_ALPHABET); 

    /* Address in text ram for characters */
    adr = 0x110BF0;

    Video_write_text16_a(&video, &adr,       0x8D00); /* Full Stop */
    Video_write_text16(&video, adr + 0x7E, 0x8D01);
    Video_write_text16_a(&video, &adr,       0x8D04); /* Arrow */
    Video_write_text16(&video, adr + 0x7E, 0x8D05);
    Video_write_text16_a(&video, &adr,       0x8D02); /* ED */
    Video_write_text16(&video, adr + 0x7E, 0x8D03);

    /* Colour selected tile red */
    RED = 0x80;
    adr = 0x110BBC + (self->letter_selected << 1);
    Video_write_text8(&video, adr,        (Video_read_text8(&video, adr) & 1) | RED);
    Video_write_text8(&video, adr + 0x80, (Video_read_text8(&video, adr + 0x80) & 1) | RED);
}

/* Flash current initial that is being entered */
/* */
/* Takes address of score entry as input */
/* */
/* Source: 0xD42C */static 
void OHiScore_flash_entry(OHiScore* self, uint32_t adr)
{
    uint16_t tile = 0x20; /* Default blank tile */
    self->flash++; /* Increment flashing counter */

    if (self->flash & BIT_3)
    {
        tile = (RomLoader_read8(&(roms.rom0), self->letter_selected + TILES_ALPHABET) & 0xFF) | 0x8600;
    }

    Video_write_text16(&video, adr + (self->initial_selected << 1), tile);
}

/* High Score Input */
/* */
/* Source: 0xD33A */static 
void OHiScore_do_input(OHiScore* self, uint32_t adr)
{
    /* Read Steering Left / Right & Denote Letter To Be Highlighted */

    const static uint8_t ENTRIES = 28; /* 28 Possible entries we can select from */
    const static uint8_t DELETE = ENTRIES - 1;
    
    int16_t position = OHiScore_read_controls(self) + self->letter_selected;

    if (position > ENTRIES)
        self->letter_selected = position = 0;
    else if (position < (self->initial_selected == 3 ? DELETE : 0))
        self->letter_selected = position = ENTRIES;
    else
        self->letter_selected = position;

    /* Check accelerator for press and depress */
    if (!self->acc_curr || !(self->acc_prev ^ self->acc_curr)) return;

    /* End option selected */
    if (self->letter_selected == ENTRIES)
    {
        Video_write_text16(&video, adr + (self->initial_selected << 1), 0x20); /* Write blank tile to ram */
        ostats.frame_counter = 0;
        ostats.time_counter = 0;
        self->state = STATE_DONE;
    }
    /* Delete option selected */
    else if (self->letter_selected == DELETE)
    {
        /* Delete if not at first position */
        if (self->initial_selected != 0)
        {
            if (self->initial_selected == 1)
                self->scores[self->score_pos].initial2 = 0x20;
            else if (self->initial_selected == 2)
                self->scores[self->score_pos].initial3 = 0x20;

            Video_write_text16(&video, adr + (self->initial_selected << 1), 0x20); /* Write blank tile to ram */

            self->initial_selected--;
        }
    }
    /* Normal character selected */
    else
    {
        uint8_t tile = RomLoader_read8(&(roms.rom0), TILES_ALPHABET + self->letter_selected);
        
        /* Store initial to score structure */
        if (self->initial_selected == 0)
            self->scores[self->score_pos].initial1 = tile;
        else if (self->initial_selected == 1)
            self->scores[self->score_pos].initial2 = tile;
        else if (self->initial_selected == 2)
        {
            self->scores[self->score_pos].initial3 = tile;
            self->letter_selected = ENTRIES;
        }

        Video_write_text16(&video, adr + (self->initial_selected << 1), tile | 0x8600); /* Write initial tile to ram */

        /* Final Initial */
        /* Note we have optional functionality to delete the final entry here */
        if (++self->initial_selected >= (config.engine.hiscore_delete ? 4 : 3))
        {
            self->state = STATE_DONE;
            ostats.frame_counter = frame_reset;
            ostats.time_counter  = 2;
            /* code to enable easter egg if YU. is inputted goes here. */
        }
    }
}

/* Read controls for high score input screen */
/* */
/* Output: */
/*  0 = No Movement */
/* -1 = Left */
/*  1 = Right */
/* */
/* Source: 0xD4DA */static 
int8_t OHiScore_read_controls(OHiScore* self)
{
    int16_t steering;
    int8_t movement;
    /* Determine when accelerator has been pressed then depressed */
    if (oinputs.input_acc < 0x30)
    {
        self->acc_prev = self->acc_curr;
        self->acc_curr = 0;
    }
    else if (oinputs.input_acc < 0x60)
    {
        self->acc_curr = self->acc_prev;
    }
    else
    {
        self->acc_prev = self->acc_curr;
        self->acc_curr = -1;
    }

    /* Check Steering Wheel */
    movement = 1;
    steering = (oinputs.input_steering & 0xFF) - 0x80;
    if (steering < 0)
    {
        steering = -steering;
        movement = -1; /* left */
    }

    /* Set increment to potentially advance to next letter. */
    /* This depends on how far the steering wheel is turned. */
    if (steering >= 0x30)
        self->steer += 5;
    else if (steering >= 0x10)
        self->steer += 1;

    if (self->steer >= 0x14)
        self->steer = 0;
    else
        movement = 0; /* no movement */
   
    return movement;
}

/* Display Best Outrunners in attract mode and name entry screen */
/* */
/* Source: 0xCE84 */
void OHiScore_display_scores(OHiScore* self)
{
    switch (self->best_or_state)
    {
        /* Init */
        case 0:
            Video_clear_text_ram(&video);
            OHiScore_setup_minicars(self);
            OHiScore_blit_score_table(self);
            self->best_or_state = 1; /* Set State to TICK */
            break;

        /* Tick */
        case 1:
            OHiScore_tick_minicars(self);
            /* Have all mini-cars reached their destination? */
            if (self->dest_total >= 7)
                 self->best_or_state = 2; /* Set State to DONE  */
            break;

        /* Return */
        case 2:
            return;
    }
}

/* ------------------------------------------------------------------------------------------------ */
/*                                       Mini car Movement */
/* ------------------------------------------------------------------------------------------------ */

/* Setup minicars before they move across screen */
/* Source: 0xCED2 */static 
void OHiScore_setup_minicars(OHiScore* self)
{
    { int i; for (i = 0; i < NO_MINICARS; i++)
    {
        self->minicars[i].pos         = 0x100;
        self->minicars[i].dst_reached = 0;
        self->minicars[i].speed       = (outils_random() & 0x180) | 0xF0;
        self->minicars[i].base_speed  = (outils_random() & 0x7) | 0x01;
    } }
}

/* Move minicars across screen on text ram layer */
/* Source: 0xCF0E */static 
void OHiScore_tick_minicars(OHiScore* self)
{
    /* Destination in text ram */
    uint32_t dst = 0x11047C;

    /* Source tile data */
    uint32_t tiles_adr = TILES_MINICARS1;

    /* There are seven lines / entries to blit */
    { int i; for (i = 0; i < NO_MINICARS; i++)
    {
        minicar_entry* minicar = &self->minicars[i];
        
        /* Minicar is on-screen */
        if (!(minicar->dst_reached & BIT_0))
        {
            uint16_t tile_bits;
            uint32_t tiles_smoke_adr;
            uint32_t textram_adr;
            int16_t pos;
            /* Minicar has reached destination position (off-screen) */
            if ((minicar->pos >> 8) >= 0x5A)
            {
                minicar->dst_reached |= BIT_0;
                self->dest_total++; /* Increment total minicars that have reached destination */
            }

            minicar->speed += minicar->base_speed;

            if (minicar->speed >= 0x200)
                minicar->speed = 0x180;

            minicar->pos += minicar->speed;

            OHiScore_setup_minicars_pal(self, minicar);

            /* Masked off the lower bit */
            pos = (minicar->pos >> 8) & 0xFFFE;

            /* Get final address in text ram for minicar based on position */
            textram_adr = dst - pos;

            /* Address for following smoke tiles */
            tiles_smoke_adr = TILES_MINICARS2;

            /* The minicar is two tiles wide. */
            /* Two versions of routine, one that only blits the car in two tiles */
            if ((minicar->pos >> 8) & BIT_0)
            {
                Video_write_text32_a(&video, &textram_adr, RomLoader_read32(&(roms.rom0), tiles_adr)); /* blit car in 2 tiles */
                Video_write_text32_a(&video, &textram_adr, RomLoader_read32_a(&(roms.rom0), &tiles_smoke_adr)); /* smoke trail tile 1 */
                Video_write_text16_a(&video, &textram_adr, RomLoader_read16_a(&(roms.rom0), &tiles_smoke_adr)); /* smoke trail tile 2 */
            }
            /* Blit at an offset */
            /* The second blits the mini-car at an offset halfway into the tile (and hence takes 3 tiles) */
            else
            {
                Video_write_text32_a(&video, &textram_adr, RomLoader_read32(&(roms.rom0), 4 + tiles_adr)); /* blit car in 3 tiles */
                Video_write_text16_a(&video, &textram_adr, RomLoader_read16(&(roms.rom0), 8 + tiles_adr)); /* blit car in 3 tiles */
                Video_write_text32_a(&video, &textram_adr, RomLoader_read32_a(&(roms.rom0), &tiles_smoke_adr)); /* smoke trail tile 1 */
                Video_write_text16_a(&video, &textram_adr, RomLoader_read16_a(&(roms.rom0), &tiles_smoke_adr)); /* smoke trail tile 2 */
            }

            /* Erase Minicar tiles (0xCFB2) */
            /* Reveal info from tile ram by copying to text ram */

            /* Bottom Line */
            tile_bits = Video_read_tile8(&video, textram_adr - 0x2000 + 1) | minicar->tile_props;
            Video_write_text16(&video, textram_adr, tile_bits);
            /* Top Line */
            tile_bits = Video_read_tile8(&video, textram_adr - 0x2000 - 0x7F) | minicar->tile_props;
            Video_write_text16(&video, textram_adr - 0x80, tile_bits);
        }

        dst += 0x100; /* Advance to next row in text ram */
        tiles_adr += 0x0A; /* Advance to next block of minicar data */
    } }
}

/* Setup palette and priority data for the copied tiles behind the minicar. */
/* The palette & priority used for the text depends on the position. */
/* Source: 0xCFCC */static 
void OHiScore_setup_minicars_pal(OHiScore* self, minicar_entry* minicar)
{
    uint8_t pos = minicar->pos >> 8;

    /* Lap Time Tile Properties */
    minicar->tile_props = 0x8400;
    if (pos <= 0x20) return; /* Was 0x1F in original: Changed to handle longer times */

    /* Route Tile Properties */
    minicar->tile_props = 0x8B00;
    if (pos <= 0x2D) return;

    /* Initial Tile Properties */
    minicar->tile_props = 0x8200;
    if (pos <= 0x39) return;

    /* Score Tile Properties */
    minicar->tile_props = 0x8400;
    if (pos <= 0x4A) return;

    /* 1.2.3. Tile Properties */
    minicar->tile_props = 0x8600;
}

/* ------------------------------------------------------------------------------------------------ */
/*                                     Score Table Rendering */
/* ------------------------------------------------------------------------------------------------ */

/* Source: 0xD00C */static 
void OHiScore_blit_score_table(OHiScore* self)
{
    /* Clear tile table ready for High Score Display */
    uint32_t tile_addr = 0x10E000; /* Tile Table 15 */
    { int i; for (i = 0; i <= 0x3FF; i++)
        Video_write_tile32_a(&video, &tile_addr, 0x200020); }

    OHud_blit_text2(&ohud, TEXT2_BEST_OR);   /* Print "BEST OUTRUNNERS" */
    OHud_blit_text1(&ohud, TEXT1_SCORE_ETC); /* Print Score, Name, Route, Record */
    OHiScore_blit_digit(self);                     /* Blit 1. 2. 3. etc. */
    OHiScore_blit_scores(self);                    /* Blit list of scores */
    OHiScore_blit_initials(self);                  /* Blit initials attached to those scores */
    if (outrun.cannonball_mode != MODE_CONT)
        OHiScore_blit_route_map(self);            /* Blit Mini Route Map */
    OHiScore_blit_lap_time(self);
}

/* Blit 7x single digit at start of score table (1. 2. 3. 4. 5. 6. 7.) */
/* Source: 0xD03A */static 
void OHiScore_blit_digit(OHiScore* self)
{
    /* Destination in tile ram for digit */
    uint32_t dst = 0x10E438;

    /* Starting display position */
    int16_t pos = self->score_display_pos + 1;

    /* Display numbers 1 to 7 */
    { int i; for (i = 0; i < 7; i++)
    {
        int32_t tile = (pos / 10) | ((pos % 10) << 16);

        /* Draw blank */
        if (!(tile & 0xFFFF))
        {
            tile = (tile & 0xFFFF0000) | 0x20;
            outils_swap32(&tile);
            tile |= 0x30;
        }
        /* Draw tile */
        else
        {
            outils_swap32(&tile);
            tile |= 0x300030;
        }

        Video_write_tile32(&video, dst, tile);      /* Output number digit */
        Video_write_tile16(&video, 4 + dst, 0x5B);  /* Output full stop following digit */

        dst += 0x100; /* Advance to next text row */
        pos++;
    } }
}

/* Blit High Scores */
/* */
/* Source: 0xD078 */static 
void OHiScore_blit_scores(OHiScore* self)
{
    /* Destination in tile ram for digit */
    uint32_t dst = 0x10E43E;

    /* Starting display position */
    int16_t pos = self->score_display_pos;

    /* Display scores 1 to 7 */
    { int i; for (i = 0; i < 7; i++)
    {
        OHud_draw_score_tile(&ohud, dst, self->scores[pos++].score, 0);
        dst += 0x100; /* Advance to next text row */
    } }
}

/* Blit Initials */
/* */
/* Source: 0xD0A4 */static 
void OHiScore_blit_initials(OHiScore* self)
{
    /* Destination in tile ram for digit */
    uint32_t dst = 0x10E452;

    /* Starting display position */
    int16_t pos = self->score_display_pos;

    /* Write 3 initials for entries 1 to 7 */
    { int i; for (i = 0; i < 7; i++)
    {
        Video_write_tile8(&video, dst + 1, self->scores[pos].initial1);
        Video_write_tile8(&video, dst + 3, self->scores[pos].initial2);
        Video_write_tile8(&video, dst + 5, self->scores[pos].initial3);
        pos++;
        dst += 0x100; /* Advance to next text row */
    } }
}

/* Blit mini route map */
/* */
/* Source: 0xD0D8 */static 
void OHiScore_blit_route_map(OHiScore* self)
{
    /* Destination in tile ram for digit */
    uint32_t dst = 0x10E45E;

    /* Starting display position */
    int16_t pos = self->score_display_pos;

    /* Write 7 map entries */
    { int i; for (i = 0; i < 7; i++)
    {
        uint32_t tiles = self->scores[pos++].maptiles;

        /* eg e5 c8 c2 d1 (4 tile indexes of route map) */
        Video_write_tile8(&video, dst - 0x7F, (tiles >> 24) & 0xFF);
        Video_write_tile8(&video, dst - 0x7D, (tiles >> 16) & 0xFF);
        Video_write_tile8(&video, dst + 0x01, (tiles >> 8) & 0xFF);
        Video_write_tile8(&video, dst + 0x03, tiles & 0xFF);

        dst += 0x100; /* Advance to next text row */
    } }
}

/* Blit laptime */
/* */
/* Source: 0xD112 */static 
void OHiScore_blit_lap_time(OHiScore* self)
{
    /* Destination in tile ram for digit */
    uint32_t dst = 0x10E46A;

    /* Starting display position */
    int16_t pos = self->score_display_pos;

    /* Write 7 lap entries */
    { int i; for (i = 0; i < 7; i++)
    {
        uint16_t time = self->scores[pos++].time;

        if (time)
        {
            OHiScore_convert_lap_time(self, time);

            /* Write laptime */
            if (self->laptime[0] != TILE_PROPS)
            {
                Video_write_tile16(&video, dst - 0x2, self->laptime[0]); /* Minutes Digit 1 */
            }
            
            Video_write_tile16(&video, 0x0 + dst, self->laptime[1]); /* Minutes Digit 2 */
            Video_write_tile16(&video, 0x2 + dst, 0x5E);       /* ' */
            Video_write_tile16(&video, 0x4 + dst, self->laptime[2]); /* Seconds Digit 1 */
            Video_write_tile16(&video, 0x6 + dst, self->laptime[3]); /* Seconds Digit 2 */
            Video_write_tile16(&video, 0x8 + dst, 0x5F);       /* ' */
            Video_write_tile16(&video, 0xA + dst, self->laptime[4]); /* Milliseconds Digit 1 */
            Video_write_tile16(&video, 0xC + dst, self->laptime[5]); /* Milliseconds Digit 2 */
        }

        dst += 0x100; /* Advance to next text row */
    } }
}

/* Convert laptime to tile data and store in laptime array. */
/* Enhanced routine to handle minutes > 9 */
/* */
/* Source: 0x806C */static 
void OHiScore_convert_lap_time(OHiScore* self, uint16_t time)
{
    uint16_t s2;
    uint16_t s1;
    uint16_t seconds;
    uint16_t ms_lookup;
    const uint16_t MINUTE = 3600;

    int32_t src_time = time; /* laptime copy [d0]  */
    int16_t minutes = -1;     /* Store number of minutes */

    /* Calculate Minutes */
    do
    {
        src_time -= MINUTE;
        minutes++;
    }
    while (src_time >= 0);
    
    src_time += MINUTE;
    minutes = outils_convert16_dechex(minutes);

    /* Store Millisecond Lookup */
    ms_lookup = src_time & 0x3F;
    
    /* Calculate Seconds */
    seconds = src_time >> 6;

    s1 = seconds & 0xF;
    s2 = seconds >> 4;

    if (s1 > 9)
        seconds += 6;

    s2 = outils_bcd_add(s2, s2);
    { int16_t d3 = s2;
    s2 = outils_bcd_add(s2, s2);
    s2 = outils_bcd_add(s2, d3);
    seconds = outils_bcd_add(s2, seconds);

    /* Output Milliseconds */
    self->laptime[5] = (ostats.lap_ms[ms_lookup] & 0xF) | TILE_PROPS;
    self->laptime[4] = ((ostats.lap_ms[ms_lookup] & 0xF0) >> 4) | TILE_PROPS;

    /* Output Seconds */
    self->laptime[3] = (seconds & 0xF) | TILE_PROPS;
    self->laptime[2] = ((seconds & 0xF0) >> 4) | TILE_PROPS;

    /* Output Minutes */
    self->laptime[1] = (minutes & 0xF) | TILE_PROPS;
    self->laptime[0] = ((minutes & 0xF0) >> 4) | TILE_PROPS;
 }}
