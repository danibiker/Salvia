/***************************************************************************
    Interface to Ported Z80 Code.
    Handles the interface between 68000 program code and Z80.

    Also abstracted here, so the more complex OSound class isn't exposed
    to the main code directly
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdlib.h>
#include "engine/outrun.h"
#include "engine/audio/osound.h"
#include "engine/audio/osoundint.h"

static void OSoundInt_add_to_queue(OSoundInt* self, uint8_t snd);

OSoundInt osoundint;
OSound osound;

void OSoundInt_ctor(OSoundInt* self)
{
    self->pcm_ram = (uint8_t*)malloc((PCM_RAM_SIZE) * sizeof(uint8_t));
    self->has_booted = false;
}

void OSoundInt_dtor(OSoundInt* self)
{
    free(self->pcm_ram);
}

void OSoundInt_init(OSoundInt* self)
{
    if (self->pcm == NULL)
    {
        self->pcm = (SegaPCM*)malloc(sizeof(SegaPCM));
        SegaPCM_ctor(self->pcm, SOUND_CLOCK, &roms.pcm, self->pcm_ram, BANK_512);
    }

    if (self->ym == NULL)
    {
        self->ym = (YM2151*)malloc(sizeof(YM2151));
        YM2151_ctor(self->ym, 0.5f, SOUND_CLOCK);
    }

    SegaPCM_init(self->pcm, config.sound.rate, config.fps);
    YM2151_init(self->ym, config.sound.rate, config.fps);

    OSoundInt_reset(self);

    /* Clear PCM Chip RAM */
    { uint16_t i; for (i = 0; i < PCM_RAM_SIZE; i++)
        self->pcm_ram[i] = 0; }

    { uint8_t i; for (i = 0; i < 8; i++)
        self->engine_data[i] = 0; }

    OSound_init(&osound, self->ym, self->pcm_ram);
}

/* Clear sound queue */
/* Source: 0x5086 */
void OSoundInt_reset(OSoundInt* self)
{
    self->sound_counter = 0;
    self->sound_head    = 0;
    self->sound_tail    = 0;
    self->sounds_queued = 0;

    self->audio_ticks = 0;
}

void OSoundInt_tick(OSoundInt* self)
{
    int max_ticks;
    /* The audio code is updated 125 times per second */
    self->audio_ticks += (125.0 / config.fps);

    /* Ticks per frame will vary between 2 and 3 at 60fps. */
    max_ticks = (int) self->audio_ticks;

    { int i; for (i = 0; i < max_ticks; i++)
    {
        OSoundInt_play_queued_sound(self); /* Process audio commands from main program code */
        OSound_tick(&osound);       /* Tick Ported Z80 Audio Code */
    } }

    self->audio_ticks -= max_ticks;
}

/* ---------------------------------------------------------------------------- */
/* Sound Queuing Code */
/* ---------------------------------------------------------------------------- */

/* Play Queued Sounds & Send Engine Noise Commands to Z80 */
/* Was called by horizontal interrupt routine */
/* Source: 0x564E */
void OSoundInt_play_queued_sound(OSoundInt* self)
{
    if (!self->has_booted)
    {
        self->sound_head = 0;
        self->sounds_queued = 0;
        return;
    }

    /* Process the lot in one go.  */
    { int counter; for (counter = 0; counter < 8; counter++)
    {
        /* Process queued sound */
        if (counter == 0)
        {
            if (self->sounds_queued != 0)
            {
                osound.command_input = self->queue[self->sound_head];
                self->sound_head = (self->sound_head + 1) & QUEUE_LENGTH;
                self->sounds_queued--;
            }
            else
            {
                osound.command_input = SOUND_RESET;
            }
        }
        /* Process player engine sounds and passing traffic */
        else
        {
            osound.engine_data[counter] = self->engine_data[counter];
        }
    } }
}

/* Queue a sound in service mode */
/* Used to trigger both sound effects and music */
/* Source: 0x56C6 */
void OSoundInt_queue_sound_service(OSoundInt* self, uint8_t snd)
{
    if (self->has_booted)
        OSoundInt_add_to_queue(self, snd);
    else
        OSoundInt_queue_clear(self);
}

/* Queue a sound in-game */
/* Note: This version has an additional check, so that certain sounds aren't played depending on game mode */
/* Source: 0x56D4 */
void OSoundInt_queue_sound(OSoundInt* self, uint8_t snd)
{
    if (self->has_booted)
    {
        if (outrun.game_state == GS_ATTRACT)
        {
            /* Return if we are not playing sound in attract mode */
            if (!config.sound.advertise && snd != SOUND_COIN_IN) return;

            /* Do not play music in attract mode, even if attract sound enabled */
            if (snd == SOUND_MUSIC_BREEZE || snd == SOUND_MUSIC_MAGICAL ||
                snd == SOUND_MUSIC_SPLASH || snd == SOUND_MUSIC_LASTWAVE)
                return;
        }
        OSoundInt_add_to_queue(self, snd);
    }
    else
        OSoundInt_queue_clear(self);
}static 

void OSoundInt_add_to_queue(OSoundInt* self, uint8_t snd)
{
    /* Add sound to the tail end of the queue */
    self->queue[self->sound_tail] = snd;
    self->sound_tail = (self->sound_tail + 1) & QUEUE_LENGTH;
    self->sounds_queued++;
}

void OSoundInt_queue_clear(OSoundInt* self)
{
    self->sound_tail = 0;
    self->sounds_queued = 0;
}