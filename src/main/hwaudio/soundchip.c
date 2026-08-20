/***************************************************************************
    Sound Chip  

    This is an abstract class, used by the Sega PCM and YM2151 chips.
    It facilitates writing to a buffer of sound data.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdlib.h>
#include <stdint.h>
#include "hwaudio/soundchip.h"

void SoundChip_ctor(SoundChip* self)
{
    self->volume     = 1.0;
    self->initalized = false;
}

void SoundChip_dtor(SoundChip* self)
{
    free(self->buffer);
}

void SoundChip_init(SoundChip* self, uint8_t channels, int32_t sample_freq, int32_t fps)
{
    self->fps         = fps;
    self->sample_freq = sample_freq;
    self->channels    = channels;

    self->frame_size =  sample_freq / fps;
    self->buffer_size = self->frame_size * channels;

    if (self->initalized)
        free(self->buffer);
    
    self->buffer = (int16_t*)malloc((self->buffer_size) * sizeof(int16_t));

    self->initalized = true;
}

/* Set soundchip volume (0 = Off, 10 = Loudest) */
void SoundChip_set_volume(SoundChip* self, uint8_t v)
{
    if (v > 10) 
        return;
    
    self->volume = (float) (v / 10.0);
}

void SoundChip_clear_buffer(SoundChip* self)
{
    { uint32_t i; for (i = 0; i < self->buffer_size; i++)
        self->buffer[i] = 0; }
}

void SoundChip_write_buffer(SoundChip* self, const uint8_t channel, uint32_t address, int16_t value)
{
    /*buffer[channel + (address * channels)] = (int16_t) (value * volume); // Unused for now */
    self->buffer[channel + (address * self->channels)] = value;
}

int16_t SoundChip_read_buffer(SoundChip* self, const uint8_t channel, uint32_t address)
{
    return self->buffer[channel + (address * self->channels)];
}

int16_t* SoundChip_get_buffer(SoundChip* self)
{
    return self->buffer;
}
