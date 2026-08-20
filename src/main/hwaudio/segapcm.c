/***************************************************************************
    Sega 8-Bit PCM Driver.
    
    This driver is based upon the MAME source code, with some minor 
    modifications to integrate it into the Cannonball framework.

    Note, that I've altered this driver to output at a fixed 44,100Hz.
    This is to avoid the need for downsampling.  
    
    See http://mamedev.org/source/docs/license.txt for more details.
***************************************************************************/

/**
 * RAM DESCRIPTION ===============
 * 
 * 0x00 - 0x07, 0x80 - 0x87 : CHANNEL #1  
 * 0x08 - 0x0F, 0x88 - 0x8F : CHANNEL #2
 * 0x10 - 0x17, 0x90 - 0x97 : CHANNEL #3  
 * 0x18 - 0x1F, 0x98 - 0x9F : CHANNEL #4
 * 0x20 - 0x27, 0xA0 - 0xA7 : CHANNEL #5  
 * 0x28 - 0x2F, 0xA8 - 0xAF : CHANNEL #6
 * 0x30 - 0x37, 0xB0 - 0xB7 : CHANNEL #7  
 * 0x38 - 0x3F, 0xB8 - 0xBF : CHANNEL #8
 * 0x40 - 0x47, 0xC0 - 0xC7 : CHANNEL #9  
 * 0x48 - 0x4F, 0xC8 - 0xCF : CHANNEL #10
 * 0x50 - 0x57, 0xD0 - 0xD7 : CHANNEL #11 
 * 0x58 - 0x5F, 0xD8 - 0xDF : CHANNEL #12
 * 0x60 - 0x67, 0xE0 - 0xE7 : CHANNEL #13 
 * 0x68 - 0x6F, 0xE8 - 0xEF : CHANNEL #14
 * 0x70 - 0x77, 0xF0 - 0xF7 : CHANNEL #15 
 * 0x78 - 0x7F, 0xF8 - 0xFF : CHANNEL #16
 * 
 * 
 * CHANNEL DESCRIPTION ===================
 * 
 * OFFS | BITS     | DESCRIPTION 
 * -----+----------+---------------------------------
 * 0x00 | -------- | (unknown) 
 * 0x01 | -------- | (unknown) 
 * 0x02 | vvvvvvvv | Volume LEFT 
 * 0x03 | vvvvvvvv | Volume RIGHT 
 * 0x04 | aaaaaaaa | Wave Start Address LOW 8 bits 
 * 0x05 | aaaaaaaa | Wave Start Address HIGH 8 bits 
 * 0x06 | eeeeeeee | Wave End Address HIGH 8 bits 
 * 0x07 | dddddddd | Delta (pitch) 
 * 0x80 | -------- | (unknown) 
 * 0x81 | -------- | (unknown) 
 * 0x82 | -------- | (unknown)
 * 0x83 | -------- | (unknown) 
 * 0x84 | llllllll | Wave Loop Address LOW 8 bits
 * 0x85 | llllllll | Wave Loop Address HIGH 8 bits 
 * 0x86 | ------la | Flags: a = active (0 = active,  1 = inactive) 
 *      |          |        l = loop   (0 = enabled, 1 = disabled)
 * 
 */

#include <stdlib.h>
#include "hwaudio/segapcm.h"

void SegaPCM_ctor(SegaPCM* self, uint32_t clock, RomLoader* rom, uint8_t* ram, int32_t bank)
{
    SoundChip_ctor(&self->sc);
    self->ram = ram;
    self->pcm_rom = rom->rom;  
    self->low = (uint8_t*)malloc((16) * sizeof(uint8_t));
    self->max_addr = rom->length;
    self->bankshift = bank & 0xFF;
    self->rgnmask = self->max_addr - 1;

    { int32_t mask = bank >> 16;
    if (mask == 0)
        mask = BANK_MASK7 >> 16;

    { int32_t rom_mask;
    for (rom_mask = 1; rom_mask < self->max_addr; rom_mask *= 2);
    rom_mask--;

    self->bankmask = mask & (rom_mask >> self->bankshift);

    { int32_t i; for (i = 0; i < 0x100; i++)
        ram[i] = 0xff; }
 } }}

void SegaPCM_dtor(SegaPCM* self)
{
    SoundChip_dtor(&self->sc);
    free(self->low);
}

void SegaPCM_init(SegaPCM* self, int32_t rate, int32_t fps)
{
    self->sc.sample_freq = rate;
    self->downsample = (32000.0 / (double) rate);
    SoundChip_init(&(self->sc), SNDCHIP_STEREO, rate, fps);
}

void SegaPCM_stream_update(SegaPCM* self)
{
    SoundChip_clear_buffer(&(self->sc));

    /* loop over channels */
    { int ch; for (ch = 0; ch < 16; ch++)
    {
        uint8_t *regs = self->ram + 8 * ch;

        /* only process active channels */
        if ((regs[0x86] & 1) == 0) 
        {             
            uint8_t *rom  = self->pcm_rom + ((regs[0x86] & self->bankmask) << self->bankshift);

            uint32_t addr = (regs[0x85] << 16) | (regs[0x84] << 8) | self->low[ch];
            uint32_t loop = (regs[0x05] << 16) | (regs[0x04] << 8);
            uint8_t end   =  regs[0x06] + 1;

            uint32_t i;

            /* loop over samples on this channel */
            for (i = 0; i < self->sc.frame_size; i++) 
            {
                double increment;
                int8_t v = 0;

                /* handle looping if we've hit the end */
                if ((addr >> 16) == end) 
                {
                    if ((regs[0x86] & 2) == 0) 
                    {
                        addr = loop;
                    } 
                    else 
                    {
                        regs[0x86] |= 1;
                        break;
                    }
                }

                /* fetch the sample */
                v = rom[(addr >> 8) & self->rgnmask] - 0x80;

                /* apply panning */
                SoundChip_write_buffer(&(self->sc), SNDCHIP_LEFT,  i, SoundChip_read_buffer(&(self->sc), SNDCHIP_LEFT,  i) + (v * regs[2]));
                SoundChip_write_buffer(&(self->sc), SNDCHIP_RIGHT, i, SoundChip_read_buffer(&(self->sc), SNDCHIP_RIGHT, i) + (v * regs[3]));

                /* Advance. */
                /* Cannonball Change: Output at a fixed 44,100Hz.  */
                increment = ((double)regs[7]) * self->downsample;
                addr = (addr + (int) increment) & 0xffffff;
            }

            /* store back the updated address and info */
            regs[0x84] = addr >> 8;
            regs[0x85] = addr >> 16;
            self->low[ch] = regs[0x86] & 1 ? 0 : addr;
        }
    } }
}