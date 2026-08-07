// T6W28_Snd_Emu

#ifndef SMS_APU_H
#define SMS_APU_H

#include <stdint.h>

#include "T6W28_Oscs.h"

typedef struct
{
	int32_t sq_period[3];
	int32_t sq_phase[3];
	uint32_t noise_period;
	uint32_t noise_period_extra;
	uint32_t noise_shifter;
	uint32_t noise_tap;

	int32_t delay[4];
	int32_t volume_left[4];
	int32_t volume_right[4];
	uint8_t latch_left, latch_right;
} T6W28_ApuState;

class T6W28_Apu
{
   public:
      // Set overall volume of all oscillators, where 1.0 is full volume
      void volume( double );

      // Set treble equalization
      void treble_eq( const blip_eq_t& );

      // Outputs can be assigned to a single buffer for mono output, or to three
      // buffers for stereo output (using Stereo_Buffer to do the mixing).

      // Assign all oscillator outputs to specified buffer(s). If buffer
      // is NULL, silences all oscillators.
      void output( Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right );

      // Assign single oscillator output to buffer(s). Valid indicies are 0 to 3,
      // which refer to Square 1, Square 2, Square 3, and Noise. If buffer is NULL,
      // silences oscillator.
      enum { osc_count = 4 };
      void osc_output( int index, Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right );

      // Reset oscillators and internal state
      void reset();

      // Write to data port
      void write_data_left( long, int );
      void write_data_right( long, int );

      // Run all oscillators up to specified time, end current frame, then
      // start a new frame at time 0.
      void end_frame( long );

      void save_state(T6W28_ApuState*);
      void load_state(const T6W28_ApuState*);
   public:
      T6W28_Apu();
      ~T6W28_Apu();
   private:
      // noncopyable
      T6W28_Apu( const T6W28_Apu& );
      T6W28_Apu& operator = ( const T6W28_Apu& );

      T6W28_Osc*    oscs [osc_count];
      T6W28_Square  squares [3];
      T6W28_Square::Synth square_synth; // used by squares
      long  last_time;
      int         latch_left, latch_right;
      T6W28_Noise   noise;

      void run_until( long );
};

#endif

