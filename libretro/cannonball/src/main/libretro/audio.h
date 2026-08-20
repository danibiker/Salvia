/***************************************************************************
    Libretro Audio Code.
    
    It takes the output from the PCM and YM chips, mixes them and then
    outputs appropriately.
    
    In order to achieve seamless audio, when audio is enabled the framerate
    is adjusted to essentially sync the video to the audio output.
    
    This is based upon code from the Atari800 emulator project.
    Copyright (c) 1998-2008 Atari800 development team
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "globals.h"

typedef struct wav_t {
    uint8_t loaded;
    int16_t *data;
    uint32_t pos;
    uint32_t length;
} wav_t;


#define CHANNELS (2)




typedef struct Audio
{
    bool sound_enabled;
    uint16_t custom_wav_volume;
    uint16_t* mix_buffer;
    wav_t wavfile;
} Audio;

extern Audio cannonball_audio;

void Audio_ctor(Audio* self);

void Audio_init(Audio* self);

void Audio_tick(Audio* self);

void Audio_start_audio(Audio* self);

void Audio_stop_audio(Audio* self);

void Audio_load_wav(Audio* self, const char* filename);

void Audio_clear_wav(Audio* self);

#ifdef __cplusplus
}
#endif
