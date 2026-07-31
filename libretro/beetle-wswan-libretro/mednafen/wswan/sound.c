/* Mednafen - Multi-system Emulator
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <stdlib.h>

#include "wswan.h"
#include "sound.h"
#include "v30mz.h"
#include "wswan-memory.h"
#include "../state_inline.h"

#include "../include/blip/Blip_Buffer.h"

static Blip_Synth WaveSynth;

static Blip_Buffer sbuf[2];

static uint16 period[4];
static uint8 volume[4]; /* left volume in upper 4 bits, right in lower 4 bits */
static uint8 voice_volume;

static uint8 sweep_step, sweep_value;
static uint8 noise_control;
static uint8 control;
static uint8 output_control;

static int32 sweep_8192_divider;
static uint8 sweep_counter;
static uint8 SampleRAMPos;

static int32 sample_cache[4][2];

static int32 last_v_val;

static uint8 HyperVoice;
static int32 last_hv_val[2];
static uint8 HVoiceCtrl, HVoiceChanCtrl;

static int32 period_counter[4];
static int32 last_val[4][2]; /* Last outputted value, l&r */
static uint8 sample_pos[4];
static uint16 nreg;
static uint32 last_ts;

static Blip_Buffer_state BlipState[2];
static uint8 BlipValid;

/* Forget the DAC output levels; used whenever the Blip integrator
 * has been reset to zero so that the next synth events re-acquire
 * absolute levels instead of leaving a permanent DC offset. */
static void ResetDACLevels(void)
{
   memset(sample_cache, 0, sizeof(sample_cache));
   memset(last_val, 0, sizeof(last_val));
   memset(last_hv_val, 0, sizeof(last_hv_val));
}


#define MK_SAMPLE_CACHE	\
   {    \
    int sample = (((wsRAM[((SampleRAMPos << 6) + (sample_pos[ch] >> 1) + (ch << 4)) ] >> ((sample_pos[ch] & 1) ? 4 : 0)) & 0x0F));    \
    sample_cache[ch][0] = sample * ((volume[ch] >> 4) & 0x0F);        \
    sample_cache[ch][1] = sample * ((volume[ch] >> 0) & 0x0F);        \
   }

#define MK_SAMPLE_CACHE_NOISE	\
   {    \
    int sample = ((nreg & 1) ? 0xF : 0x0);	\
    sample_cache[ch][0] = sample * ((volume[ch] >> 4) & 0x0F);        \
    sample_cache[ch][1] = sample * ((volume[ch] >> 0) & 0x0F);        \
   }

#define MK_SAMPLE_CACHE_VOICE \
   {    \
    int sample = volume[ch]; \
    int half = sample >> 1; \
    sample_cache[ch][0] = (voice_volume & 4) ? sample : (voice_volume & 8) ? half : 0; \
    sample_cache[ch][1] = (voice_volume & 1) ? sample : (voice_volume & 2) ? half : 0; \
   }

#define SYNCSAMPLE(wt)	\
   {	\
    int32 left = sample_cache[ch][0], right = sample_cache[ch][1];	\
    Blip_Synth_offset(&WaveSynth, wt, left - last_val[ch][0], &sbuf[0]);	\
    Blip_Synth_offset(&WaveSynth, wt, right - last_val[ch][1], &sbuf[1]);	\
    last_val[ch][0] = left;	\
    last_val[ch][1] = right;	\
   }

#define SYNCSAMPLE_NOISE(wt) SYNCSAMPLE(wt)

void WSwan_SoundUpdate(void)
{
   unsigned int ch;
   int32 run_time = v30mz_timestamp - last_ts;

   for(ch = 0; ch < 4; ch++)
   {
      /* Channel is disabled? */
      if(!(control & (1 << ch)))
         continue;

      if(ch == 1 && (control & 0x20)) /* Direct D/A mode? */
      {
         MK_SAMPLE_CACHE_VOICE;
         SYNCSAMPLE(v30mz_timestamp);
      }
      else if(ch == 2 && (control & 0x40) && sweep_value) /* Sweep */
      {
         uint32 tmp_pt         = 2048 - period[ch];
         uint32 meow_timestamp = v30mz_timestamp - run_time;
         uint32 tmp_run_time   = run_time;

         while(tmp_run_time)
         {
            int32 sub_run_time = tmp_run_time;

            if(sub_run_time > sweep_8192_divider)
               sub_run_time = sweep_8192_divider;

            sweep_8192_divider -= sub_run_time;
            if(sweep_8192_divider <= 0)
            {
               sweep_8192_divider += 8192;
               sweep_counter--;
               if(sweep_counter <= 0)
               {
                  sweep_counter = sweep_step + 1;
                  period[ch] = (period[ch] + (int8)sweep_value) & 0x7FF;
               }
            }

            meow_timestamp += sub_run_time;
            if(tmp_pt > 4)
            {
               period_counter[ch] -= sub_run_time;
               while(period_counter[ch] <= 0)
               {
                  sample_pos[ch] = (sample_pos[ch] + 1) & 0x1F;

                  MK_SAMPLE_CACHE;
                  SYNCSAMPLE(meow_timestamp + period_counter[ch]);
                  period_counter[ch] += tmp_pt;
               }
            }
            tmp_run_time -= sub_run_time;
         }
      }
      else if(ch == 3 && (control & 0x80) && (noise_control & 0x10)) /* Noise */
      {
         uint32 tmp_pt = 2048 - period[ch];

         period_counter[ch] -= run_time;
         while(period_counter[ch] <= 0)
         {
            static const uint8 stab[8] = { 14, 10, 13, 4, 8, 6, 9, 11 };

            nreg = ((nreg << 1) | ((1 ^ (nreg >> 7) ^ (nreg >> stab[noise_control & 0x7])) & 1)) & 0x7FFF;

            if(control & 0x80)
            {
               MK_SAMPLE_CACHE_NOISE;
               SYNCSAMPLE_NOISE(v30mz_timestamp + period_counter[ch]);
            }
            else if(tmp_pt > 4)
            {
               sample_pos[ch] = (sample_pos[ch] + 1) & 0x1F;
               MK_SAMPLE_CACHE;
               SYNCSAMPLE(v30mz_timestamp + period_counter[ch]);
            }
            period_counter[ch] += tmp_pt;
         }
      }
      else
      {
         uint32 tmp_pt = 2048 - period[ch];

         if(tmp_pt > 4)
         {
            period_counter[ch] -= run_time;
            while(period_counter[ch] <= 0)
            {
               sample_pos[ch] = (sample_pos[ch] + 1) & 0x1F;

               MK_SAMPLE_CACHE;
               SYNCSAMPLE(v30mz_timestamp + period_counter[ch]);
               period_counter[ch] += tmp_pt;
            }
         }
      }
   }

   if(HVoiceCtrl & 0x80)
   {
      int32 left, right;
      int16 sample = (uint8)HyperVoice;

      switch(HVoiceCtrl & 0xC)
      {
         case 0x0: sample = (uint16)sample << (8 - (HVoiceCtrl & 3)); break;
         case 0x4: sample = (uint16)(sample | -0x100) << (8 - (HVoiceCtrl & 3)); break;
         case 0x8: sample = (uint16)((int8)sample) << (8 - (HVoiceCtrl & 3)); break;
         case 0xC: sample = (uint16)sample << 8; break;
      }
      /* bring back to 11bit, keeping signedness */
      sample >>= 5;

      left  = (HVoiceChanCtrl & 0x40) ? sample : 0;
      right = (HVoiceChanCtrl & 0x20) ? sample : 0;

      Blip_Synth_offset(&WaveSynth, v30mz_timestamp, left - last_hv_val[0], &sbuf[0]);
      Blip_Synth_offset(&WaveSynth, v30mz_timestamp, right - last_hv_val[1], &sbuf[1]);
      last_hv_val[0] = left;
      last_hv_val[1] = right;
   }
   last_ts = v30mz_timestamp;
}

void WSwan_SoundWrite(uint32 A, uint8 V)
{
   WSwan_SoundUpdate();

   if(A >= 0x80 && A <= 0x87)
   {
      int ch = (A - 0x80) >> 1;

      if(A & 1)
         period[ch] = (period[ch] & 0x00FF) | ((V & 0x07) << 8);
      else
         period[ch] = (period[ch] & 0x0700) | ((V & 0xFF) << 0);
   }
   else if(A >= 0x88 && A <= 0x8B)
      volume[A - 0x88] = V;
   else if(A == 0x8C)
      sweep_value = V;
   else if(A == 0x8D)
   {
      sweep_step = V;
      sweep_counter = sweep_step + 1;
      sweep_8192_divider = 8192;
   }
   else if(A == 0x8E)
   {
      if(V & 0x8)
         nreg = 0;

      noise_control = V & 0x17;
   }
   else if(A == 0x90)
   {
      int n;
      for(n = 0; n < 4; n++)
      {
         if(!(control & (1 << n)) && (V & (1 << n)))
         {
            period_counter[n] = 1;
            sample_pos[n] = 0x1F;
         }
      }
      control = V;
   }
   else if(A == 0x91)
      output_control = V & 0xF;
   else if(A == 0x92)
      nreg = (nreg & 0xFF00) | (V << 0);
   else if(A == 0x93)
      nreg = (nreg & 0x00FF) | ((V & 0x7F) << 8);  
   else if(A == 0x94)
      voice_volume = V & 0xF;
   else switch(A)
   {
      case 0x6A: HVoiceCtrl = V; break;
      case 0x6B: HVoiceChanCtrl = V & 0x6F; break;
      case 0x8F: SampleRAMPos = V; break;
      case 0x95: HyperVoice = V; break; /* Pick a port, any port?! */
   }
   WSwan_SoundUpdate();
}

uint8 WSwan_SoundRead(uint32 A)
{
   WSwan_SoundUpdate();

   if(A >= 0x80 && A <= 0x87)
   {
      int ch = (A - 0x80) >> 1;

      if(A & 1)
         return(period[ch] >> 8);
      return(period[ch]);
   }
   else if(A >= 0x88 && A <= 0x8B)
      return(volume[A - 0x88]);
   else switch(A)
   {
      case 0x6A: return(HVoiceCtrl);
      case 0x6B: return(HVoiceChanCtrl);
      case 0x8C: return(sweep_value);
      case 0x8D: return(sweep_step);
      case 0x8E: return(noise_control);
      case 0x8F: return(SampleRAMPos);
      case 0x90: return(control);
      case 0x91: return(output_control | 0x80);
      case 0x92: return((nreg >> 0) & 0xFF);
      case 0x93: return((nreg >> 8) & 0xFF);
      case 0x94: return(voice_volume);
      default:
         break;
   }

   return 0;
}

int32 WSwan_SoundFlush(int16 **SoundBuf, int32 *SoundBufSize)
{
   int32 RequiredSize = 0;
   int32 FrameCount = 0;

   WSwan_SoundUpdate();

   Blip_Buffer_end_frame(&sbuf[0], v30mz_timestamp);
   Blip_Buffer_end_frame(&sbuf[1], v30mz_timestamp);

   RequiredSize = Blip_Buffer_samples_avail(&sbuf[0]) << 1;
   RequiredSize = (RequiredSize + 1) & ~1;

   if (SoundBuf && *SoundBuf)
   {
      /* Check if sound buffer needs to be resized */
      if (*SoundBufSize < RequiredSize)
      {
         int16 *newBuf = (int16*)realloc(*SoundBuf,
               RequiredSize * sizeof(int16));

         if (newBuf)
         {
            *SoundBuf     = newBuf;
            *SoundBufSize = RequiredSize;
         }
      }

      FrameCount = Blip_Buffer_read_samples(&sbuf[0], *SoundBuf    , *SoundBufSize);
      FrameCount = Blip_Buffer_read_samples(&sbuf[1], *SoundBuf + 1, *SoundBufSize);
   }

   last_ts = 0;

   return(FrameCount);
}

/* Call before wsRAM is updated */
void WSwan_SoundCheckRAMWrite(uint32 A)
{
   if((A >> 6) == SampleRAMPos)
      WSwan_SoundUpdate();
}

static void RedoVolume(void)
{
   /* effective volume 1/4 */
   Blip_Synth_set_volume_n(&WaveSynth, 1, 4, 256);
}

void WSwan_SoundInit(void)
{
   unsigned i;
   for(i = 0; i < 2; i++)
   {
      Blip_Buffer_init(&sbuf[i]);
      Blip_Buffer_set_sample_rate(&sbuf[i], 44100, 60);
      Blip_Buffer_set_clock_rate(&sbuf[i], (long)(3072000));
      Blip_Buffer_bass_freq(&sbuf[i], 20);
   }

   RedoVolume();
}

void WSwan_SoundKill(void)
{
   int i;
   for(i = 0; i < 2; i++)
   {
      Blip_Buffer_deinit(&sbuf[i]);
   }
}

bool WSwan_SetSoundRate(uint32 rate)
{
   unsigned i;
   for(i = 0; i < 2; i++)
      Blip_Buffer_set_sample_rate(&sbuf[i], rate ? rate : 44100, 60);

   /* set_sample_rate cleared the buffers, zeroing the Blip
    * integrators; drop the tracked DAC levels with them or every
    * channel keeps a permanent DC offset until it next changes
    * amplitude. */
   ResetDACLevels();

   return(true);
}

int WSwan_SoundStateAction(StateMem *sm, int load, int data_only)
{
   SFORMAT StateRegs[] =
   {
      SFARRAY16(period, 4),
      SFARRAY(volume, 4),
      SFVAR(voice_volume),
      SFVAR(sweep_step),
      SFVAR(sweep_value),
      SFVAR(noise_control),
      SFVAR(control),
      SFVAR(output_control),
      SFVAR(HVoiceCtrl),
      SFVAR(HVoiceChanCtrl),

      SFVAR(sweep_8192_divider),
      SFVAR(sweep_counter),
      SFVAR(SampleRAMPos),
      SFARRAY32(period_counter, 4),
      SFARRAY(sample_pos, 4),
      SFVAR(nreg),

      /* Output-side DAC and resampler state. Not serialized
       * upstream, but without it audio is not sample-exact across
       * a savestate load: Blip_Buffer integrates deltas against
       * these levels, so a mismatch at load time turns into a
       * persistent DC offset on the reconstructed waveform.
       * Older savestates simply lack the fields (BlipValid stays
       * 0) and take the fallback path below. */
      SFARRAY32N(&sample_cache[0][0], 8, "sample_cache"),
      SFARRAY32N(&last_val[0][0], 8, "last_val"),
      SFARRAY32N(last_hv_val, 2, "last_hv_val"),
      SFVAR(HyperVoice),
      SFVARN(BlipState[0].offset, "Blip0.offset"),
      SFVARN(BlipState[0].reader_accum, "Blip0.accum"),
      SFVARN(BlipState[0].sample_rate, "Blip0.rate"),
      SFVARN(BlipState[0].clock_rate, "Blip0.clock"),
      SFARRAY32N(BlipState[0].tail, BLIP_STATE_TAIL_COUNT, "Blip0.tail"),
      SFVARN(BlipState[1].offset, "Blip1.offset"),
      SFVARN(BlipState[1].reader_accum, "Blip1.accum"),
      SFVARN(BlipState[1].sample_rate, "Blip1.rate"),
      SFVARN(BlipState[1].clock_rate, "Blip1.clock"),
      SFARRAY32N(BlipState[1].tail, BLIP_STATE_TAIL_COUNT, "Blip1.tail"),
      SFVARN(BlipValid, "BlipValid"),
      SFEND
   };

   if(!load)
   {
      Blip_Buffer_save_state(&sbuf[0], &BlipState[0]);
      Blip_Buffer_save_state(&sbuf[1], &BlipState[1]);
      BlipValid = 1;
   }
   else
      BlipValid = 0;

   if(!MDFNSS_StateAction(sm, load, data_only, StateRegs, "PSG", false))
      return 0;

   if(load)
   {
      unsigned ch;
      int blip_ok = 0;

      if(BlipValid)
      {
         blip_ok  = Blip_Buffer_load_state(&sbuf[0], &BlipState[0]);
         blip_ok &= Blip_Buffer_load_state(&sbuf[1], &BlipState[1]);
      }
      else
      {
         Blip_Buffer_clear(&sbuf[0], 1);
         Blip_Buffer_clear(&sbuf[1], 1);
      }

      /* If the resampler state could not be restored (old
       * savestate, or the output rate changed between sessions),
       * its integrator is now zero; drop the DAC levels with it
       * so the two stay consistent. */
      if(!blip_ok)
         ResetDACLevels();

      if(sweep_8192_divider < 1)
         sweep_8192_divider = 1;

      for(ch = 0; ch < 4; ch++)
      {
         period[ch] &= 0x7FF;

         if(period_counter[ch] < 1)
            period_counter[ch] = 1;

         sample_pos[ch] &= 0x1F;
      }
   }

   return 1;
}

void WSwan_SoundReset(void)
{
   unsigned y,ch;

   memset(period, 0, sizeof(period));
   memset(volume, 0, sizeof(volume));
   voice_volume = 0;
   sweep_step = 0;
   sweep_value = 0;
   noise_control = 0;
   control = 0;
   output_control = 0;

   sweep_8192_divider = 8192;
   sweep_counter = 0;
   SampleRAMPos = 0;

   for(ch = 0; ch < 4; ch++)
      period_counter[ch] = 1;

   memset(sample_pos, 0, sizeof(sample_pos));
   nreg = 0;

   memset(sample_cache, 0, sizeof(sample_cache));
   memset(last_val, 0, sizeof(last_val));
   last_v_val = 0;

   HyperVoice = 0;
   last_hv_val[0] = last_hv_val[1] = 0;
   HVoiceCtrl = 0;
   HVoiceChanCtrl = 0;

   for(y = 0; y < 2; y++)
      Blip_Buffer_clear(&sbuf[y], 1);
}
