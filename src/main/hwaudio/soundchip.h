/***************************************************************************
    Sound Chip  

    This is an abstract class, used by the Sega PCM and YM2151 chips.
    It facilitates writing to a buffer of sound data.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <boolean.h>

#define SNDCHIP_MONO (1)

#define SNDCHIP_STEREO (2)

#define SNDCHIP_LEFT (0)

#define SNDCHIP_RIGHT (1)

typedef struct SoundChip
{
    bool initalized;
    uint32_t sample_freq;
    uint8_t channels;
    uint32_t buffer_size;
    uint32_t frame_size;
    float volume;
    int16_t* buffer;
    uint32_t fps;
} SoundChip;

void SoundChip_ctor(SoundChip* self);

void SoundChip_dtor(SoundChip* self);

void SoundChip_init(SoundChip* self, uint8_t, int32_t, int32_t);

int16_t* SoundChip_get_buffer(SoundChip* self);

void SoundChip_set_volume(SoundChip* self, uint8_t);

void SoundChip_clear_buffer(SoundChip* self);

void SoundChip_write_buffer(SoundChip* self, const uint8_t, uint32_t, int16_t);

int16_t SoundChip_read_buffer(SoundChip* self, const uint8_t, uint32_t);

#ifdef __cplusplus
}
#endif
