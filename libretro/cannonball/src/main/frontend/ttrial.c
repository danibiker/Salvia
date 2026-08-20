/***************************************************************************
    Time Trial Mode Front End.

    This file is part of Cannonball. 
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "libretro/input.h"

#include "frontend/ttrial.h"

#include "engine/ohud.h"
#include "engine/oinputs.h"
#include "engine/outils.h"
#include "engine/omap.h"
#include "engine/ostats.h"
#include "engine/otiles.h"

/* Track Selection: Ferrari Position Per Track */
/* This is a link to a sprite object that represents part of the course map. */
static const uint8_t FERRARI_POS[] = 
{
    1,5,3,11,9,7,19,17,15,13,24,23,22,21,20
};

/* Map Stage Number to Internal Lookup  */
static const uint8_t STAGE_LOOKUP[] = 
{
    0x00,
    0x09, 0x08,
    0x12, 0x11, 0x10,
    0x1B, 0x1A, 0x19, 0x18,
    0x24, 0x23, 0x22, 0x21, 0x20
};

void TTrial_ctor(TTrial* self, uint16_t* best_times)
{
    self->best_times = best_times;
}


void TTrial_init(TTrial* self)
{
    self->state = INIT_COURSEMAP;
}

int TTrial_tick(TTrial* self)
{
    switch (self->state)
    {
        case INIT_COURSEMAP:
            Outrun_select_course(&outrun, config.engine.jap != 0, config.engine.prototype != 0); /* Need to setup correct course map graphics. */
            Config_load_tiletrial_scores(&config);
            OStats_init(&ostats, true);
            OSprites_init(&osprites);
            video.enabled = true;
            hwsprites_set_x_clip(video.sprite_layer, false);
            OMap_init(&omap);
            OMap_load_sprites(&omap);
            OMap_position_ferrari(&omap, FERRARI_POS[self->level_selected = 0]);
            OHud_blit_text_big(&ohud, 1, "STEER TO SELECT TRACK", false);
            OHud_blit_text1_xy(&ohud, 2, 25, TEXT1_LAPTIME1);
            OHud_blit_text1_xy(&ohud, 2, 26, TEXT1_LAPTIME2);
            OSoundInt_queue_sound(&osoundint, SOUND_PCM_WAVE);
            outrun.ttrial.laps    = config.ttrial.laps;
            outrun.custom_traffic = config.ttrial.traffic;
            self->state = TICK_COURSEMAP;

        case TICK_COURSEMAP:
            {
                if (Input_has_pressed(&input, MENU))
                {
                    return BACK_TO_MENU;
                }
                else if (Input_has_pressed(&input, LEFT) || OInputs_is_analog_l(&oinputs))
                {
                    if (--self->level_selected < 0)
                        self->level_selected = sizeof(FERRARI_POS) - 1;
                }
                else if (Input_has_pressed(&input, RIGHT)|| OInputs_is_analog_r(&oinputs))
                {
                    if (++self->level_selected > sizeof(FERRARI_POS) - 1)
                        self->level_selected = 0;
                }
                else if (Input_has_pressed(&input, START) || Input_has_pressed(&input, ACCEL) || OInputs_is_analog_select(&oinputs))
                {
                    outils_convert_counter_to_time(self->best_times[self->level_selected], self->best_converted);

                    outrun.cannonball_mode         = MODE_TTRIAL;
                    outrun.ttrial.level            = STAGE_LOOKUP[self->level_selected];
                    outrun.ttrial.current_lap      = 0;
                    outrun.ttrial.best_lap_counter = 10000;
                    outrun.ttrial.best_lap[0]      = self->best_converted[0];
                    outrun.ttrial.best_lap[1]      = self->best_converted[1];
                    outrun.ttrial.best_lap[2]      = self->best_converted[2];
                    outrun.ttrial.best_lap_counter = self->best_times[self->level_selected];
                    outrun.ttrial.new_high_score   = false;
                    outrun.ttrial.overtakes        = 0;
                    outrun.ttrial.crashes          = 0;
                    outrun.ttrial.vehicle_cols     = 0;
                    ostats.credits = 1;
                    return INIT_GAME;
                }
                OMap_position_ferrari(&omap, FERRARI_POS[self->level_selected]);
                outils_convert_counter_to_time(self->best_times[self->level_selected], self->best_converted);
                OHud_draw_lap_timer(&ohud, OHud_translate(&ohud, 7, 26, 0x110030), self->best_converted, self->best_converted[2]);
                OMap_blit(&omap);
                ORoad_tick(&oroad);
                OSprites_sprite_copy(&osprites);
                OSprites_update_sprites(&osprites);
                OTiles_write_tilemap_hw(&otiles);
                OTiles_update_tilemaps(&otiles, 0);
            }
            break;
    }

    return CONTINUE;
}

void TTrial_update_best_time(TTrial* self)
{
    self->best_times[self->level_selected] = outrun.ttrial.best_lap_counter;
    Config_save_tiletrial_scores(&config);
}
