/***************************************************************************
    Interface to Ported Z80 Code.
    Handles the interface between 68000 program code and Z80.

    Also abstracted here, so the more complex OSound class isn't exposed
    to the main code directly
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "hwaudio/segapcm.h"
#include "hwaudio/ym2151.h"
#include "engine/audio/commands.h"

enum { QUEUE_LENGTH = 0x1F };

#define PCM_RAM_SIZE (0x100)

#define SOUND_CLOCK (4000000)

typedef struct OSoundInt
{
    SegaPCM* pcm;
    YM2151*  ym;
    bool has_booted;
    uint8_t engine_data[8];
    double audio_ticks;
    uint8_t* pcm_ram;
    uint8_t sound_counter;
    uint8_t queue[QUEUE_LENGTH + 1];
    uint8_t sounds_queued;
    uint8_t sound_head, sound_tail;
} OSoundInt;

extern OSoundInt osoundint;

void OSoundInt_ctor(OSoundInt* self);

void OSoundInt_dtor(OSoundInt* self);

void OSoundInt_init(OSoundInt* self);

void OSoundInt_reset(OSoundInt* self);

void OSoundInt_tick(OSoundInt* self);

void OSoundInt_play_queued_sound(OSoundInt* self);

void OSoundInt_queue_sound_service(OSoundInt* self, uint8_t snd);

void OSoundInt_queue_sound(OSoundInt* self, uint8_t snd);

void OSoundInt_queue_clear(OSoundInt* self);

#ifdef __cplusplus
}
#endif
