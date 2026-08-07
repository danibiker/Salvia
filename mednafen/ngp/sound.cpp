#include <stdlib.h>

#include "neopop.h"
#include "sound.h"

#include "../include/blip/Blip_Buffer.h"
#include "../include/blip/Stereo_Buffer.h"
#include "T6W28_Apu.h"
#include "../state.h"
#include "../state_helpers.h"

static T6W28_Apu apu;

static Stereo_Buffer buf;

static uint8_t LastDACLeft = 0, LastDACRight = 0;
static uint8_t CurrentDACLeft = 0, CurrentDACRight = 0;

static Blip_Synth<blip_good_quality, 0xFF> synth;
extern "C" int32_t ngpc_soundTS;
static bool schipenable = 0;

extern "C" void MDFNNGPCSOUND_SetEnable(bool set)
{
   schipenable = set;
   if(!set)
      apu.reset();
}

extern "C" void Write_SoundChipLeft(uint8_t data)
{
   if(schipenable)
      apu.write_data_left(ngpc_soundTS >> 1, data);
}

extern "C" void Write_SoundChipRight(uint8_t data)
{
   if(schipenable)
      apu.write_data_right(ngpc_soundTS >> 1, data);
}

extern "C" void dac_write_left(uint8_t data)
{
   CurrentDACLeft = data;

   synth.offset_inline(ngpc_soundTS >> 1, CurrentDACLeft - LastDACLeft, buf.left());

   LastDACLeft = data;
}

extern "C" void dac_write_right(uint8_t data)
{
   CurrentDACRight = data;

   synth.offset_inline(ngpc_soundTS >> 1, CurrentDACRight - LastDACRight, buf.right());

   LastDACRight = data;
}

extern "C" int32_t MDFNNGPCSOUND_Flush(int16_t *SoundBuf, const int32_t MaxSoundFrames)
{
   int32_t FrameCount = 0;

   apu.end_frame(ngpc_soundTS >> 1);
   buf.end_frame(ngpc_soundTS >> 1);

   if(SoundBuf)
      FrameCount = buf.read_samples(SoundBuf, MaxSoundFrames * 2) / 2;
   else
      buf.clear();

   return(FrameCount);
}

extern "C" void MDFNNGPCSOUND_Init(void)
{
   MDFNNGPC_SetSoundRate();
   buf.clock_rate((long)(3072000));

   apu.output(buf.center(), buf.left(), buf.right());
   apu.volume(0.30);
   synth.volume(0.40);
   buf.bass_freq(20);
}

extern "C" void MDFNNGPC_SetSoundRate(void)
{
   buf.set_sample_rate(44100, 60);
}

extern "C" int MDFNNGPCSOUND_StateAction(void *data, int load, int data_only)
{
   T6W28_ApuState sn_state;

   apu.save_state(&sn_state);

   SFORMAT StateRegs[] =
   {
      SFVARN(CurrentDACLeft, "CurrentDACLeft"),
      SFVARN(CurrentDACRight, "CurrentDACRight"),

      SFVARN_BOOL(schipenable, "schipenable"),

      { sn_state.delay, (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "Delay" },
      { sn_state.volume_left, (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "VolumeLeft" },
      { sn_state.volume_right, (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "VolumeRight" },
      { sn_state.sq_period, (uint32_t)(3 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "SQPeriod" },
      { sn_state.sq_phase, (uint32_t)(3 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "SQPhase" },
      { &(sn_state.noise_period), (uint32_t)sizeof(sn_state.noise_period), MDFNSTATE_RLSB, "NPeriod" },
      { &(sn_state.noise_shifter), (uint32_t)sizeof(sn_state.noise_shifter), MDFNSTATE_RLSB, "NShifter" },
      { &(sn_state.noise_tap), (uint32_t)sizeof(sn_state.noise_tap), MDFNSTATE_RLSB, "NTap" },
      { &(sn_state.noise_period_extra), (uint32_t)sizeof(sn_state.noise_period_extra), MDFNSTATE_RLSB, "NPeriodExtra" },
      { &(sn_state.latch_left), (uint32_t)sizeof(sn_state.latch_left), MDFNSTATE_RLSB, "LatchLeft" },
      { &(sn_state.latch_right), (uint32_t)sizeof(sn_state.latch_right), MDFNSTATE_RLSB, "LatchRight" },
      { 0, 0, 0, 0 }
   };

   if(!MDFNSS_StateAction(data, load, data_only, StateRegs, "SND", false))
      return 0;

   if(load)
   {
      buf.clear();
      apu.load_state(&sn_state);

      LastDACLeft = CurrentDACLeft;
      LastDACRight = CurrentDACRight;
   }

   return 1;
}
