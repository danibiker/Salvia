/* Copyright (C) 2003-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "sound.h"

#include "gba.h"
#include "globals.h"

#include "memory.h"
#include "port.h"
#include "system.h"

/* Uncomment to have Gb_Apu run at 4x normal clock rate (16777216 Hz), useful in
a Game Boy Advance emulator. */
#define GB_APU_OVERCLOCK 4

#define SGCNT0_H 0x82
#define FIFOA_L 0xa0
#define FIFOA_H 0xa2
#define FIFOB_L 0xa4
#define FIFOB_H 0xa6

#define BLIP_BUFFER_ACCURACY 16
#define BLIP_PHASE_BITS 8
#define BLIP_WIDEST_IMPULSE_ 16
#define BLIP_BUFFER_EXTRA_ 18
#define BLIP_RES 256
#define BLIP_RES_MIN_ONE 255
#define BLIP_SAMPLE_BITS 30
#define BLIP_DEFAULT_LENGTH 250		/* 1/4th of a second */

#define BUFS_SIZE 3
#define STEREO 2

#define	CLK_MUL	GB_APU_OVERCLOCK
/* Derived from CLK_MUL rather than hardcoded: the literals 8/16/24/32/60/128
 * were correct only while GB_APU_OVERCLOCK == 4.  Expressing them as products
 * keeps them in lockstep if the overclock factor is ever changed, and the
 * results are still compile-time constants so codegen is byte-identical. */
#define CLK_MUL_MUL_2 (CLK_MUL * 2)
#define CLK_MUL_MUL_4 (CLK_MUL * 4)
#define CLK_MUL_MUL_6 (CLK_MUL * 6)
#define CLK_MUL_MUL_8 (CLK_MUL * 8)
#define CLK_MUL_MUL_15 (CLK_MUL * 15)
#define CLK_MUL_MUL_32 (CLK_MUL * 32)
#define DAC_BIAS 7

#define PERIOD_MASK 0x70
#define SHIFT_MASK 0x07

#define PERIOD2_MASK 0x1FFFF

#define BANK40_MASK 0x40
#define BANK_SIZE 32
#define BANK_SIZE_MIN_ONE 31
#define BANK_SIZE_DIV_TWO 16

/* 11-bit frequency in NRx3 and NRx4 (takes the struct pointer like the others above) */
#define GB_OSC_FREQUENCY(self) ((((self)->regs[4] & 7) << 8) + (self)->regs[3])

#define	WAVE_TYPE	0x100
#define NOISE_TYPE	0x200
#define MIXED_TYPE	WAVE_TYPE | NOISE_TYPE
#define TYPE_INDEX_MASK	0xFF

/* Portable on all targets. The previous non-x86 form `(int16_t)in != in`
 * relied on implementation-defined narrowing semantics; modern gcc/clang
 * compile both forms to the same code on every supported target. */
/* BLIP_CLAMP / BLIP_CLAMP_ were folded into stereo_buffer_mixer_read_pairs,
 * the only place samples are clamped to int16_t range.  The portability note
 * above still applies to the inlined form there: the comparison-based test
 * has no implementation-defined narrowing.
 *
 * Macros below took an implicit `this` in their original C++ form; the C
 * port passes the struct pointer explicitly. */
#define GB_ENV_DAC_ENABLED(self) ((self)->regs[2] & 0xF8)	/* Non-zero if DAC is enabled*/
#define GB_NOISE_PERIOD2_INDEX(self)	((self)->regs[3] >> 4)
#define GB_NOISE_PERIOD2(self, base)	((base) << GB_NOISE_PERIOD2_INDEX(self))
#define GB_NOISE_LFSR_MASK(self)	(((self)->regs[3] & 0x08) ? ~0x4040 : ~0x4000)
#define GB_WAVE_DAC_ENABLED(self) ((self)->regs[0] & 0x80)	/* Non-zero if DAC is enabled*/

/* reload_sweep_timer was a multi-statement macro used in exactly two
 * places; one (Gb_Sweep_Square_clock_sweep) had already been hand-inlined.
 * Both are now written out inline -- a brace-less multi-statement macro
 * like this is an `if`-dangling hazard, and there is no longer a shared
 * definition to keep them consistent with. */

#define NR10 0x60
#define NR11 0x62
#define NR12 0x63
#define NR13 0x64
#define NR14 0x65
#define NR21 0x68
#define NR22 0x69
#define NR23 0x6c
#define NR24 0x6d
#define NR30 0x70
#define NR31 0x72
#define NR32 0x73
#define NR33 0x74
#define NR34 0x75
#define NR41 0x78
#define NR42 0x79
#define NR43 0x7c
#define NR44 0x7d
#define NR50 0x80
#define NR51 0x81
#define NR52 0x84

/* GBA CPU cycles per video frame.
 * 16777216 Hz / 59.7275 fps = 280896 cycles per frame.
 * (The prior "1/100th of a second" comment was wrong: 1/100 s of CPU
 * clock would be 167772 cycles, not 280896.) */
#define SOUND_CLOCK_TICKS_ 280896

/*============================================================
	STRUCT DECLS
============================================================ */

/* C-style "inheritance": each derived struct repeats its parent's field
 * list verbatim at the top, so the parent's fields occupy the same
 * leading offsets and a `Gb_Env *` (etc.) can be cast to `Gb_Osc *`.
 * The GB_*_FIELDS macros that used to factor this out have been folded
 * into the struct bodies below -- the field lists are stable and the
 * macro indirection obscured the one property that actually matters
 * here, namely that each struct is a prefix of its "derived" structs.
 * That prefix relationship is asserted at the end of this block. */

struct Blip_Buffer
{
	long clock_rate_;
	int length_;		/* Length of buffer in milliseconds*/
	long sample_rate_;	/* Current output sample rate*/
	uint32_t factor_;
	uint32_t offset_;
	int32_t * buffer_;
	int32_t buffer_size_;
	int32_t reader_accum_;
};
typedef struct Blip_Buffer Blip_Buffer;

struct Blip_Synth
{
	Blip_Buffer* buf;
	int delta_factor;
};
typedef struct Blip_Synth Blip_Synth;

/* --- Gb_Osc: base oscillator --- */
struct Gb_Osc
{
	struct Blip_Buffer* outputs[4];	/* NULL, right, left, center*/
	struct Blip_Buffer* output;	/* where to output sound*/
	uint8_t * regs;			/* osc's 5 registers*/
	int mode;			/* mode_dmg, mode_cgb, mode_agb*/
	int dac_off_amp;		/* amplitude when DAC is off*/
	int last_amp;			/* current amplitude in Blip_Buffer*/
	struct Blip_Synth const* good_synth;
	struct Blip_Synth const* med_synth;
	int delay;			/* clocks until frequency timer expires*/
	int length_ctr;			/* length counter*/
	unsigned phase;			/* waveform phase (or equivalent)*/
	bool enabled;			/* internal enabled flag*/
};
typedef struct Gb_Osc Gb_Osc;

/* --- Gb_Env: Gb_Osc + envelope --- */
struct Gb_Env
{
	/* Gb_Osc prefix */
	struct Blip_Buffer* outputs[4];
	struct Blip_Buffer* output;
	uint8_t * regs;
	int mode;
	int dac_off_amp;
	int last_amp;
	struct Blip_Synth const* good_synth;
	struct Blip_Synth const* med_synth;
	int delay;
	int length_ctr;
	unsigned phase;
	bool enabled;
	/* envelope */
	int  env_delay;
	int  volume;
	bool env_enabled;
};
typedef struct Gb_Env Gb_Env;

/* --- Gb_Square: identical layout to Gb_Env --- */
struct Gb_Square
{
	/* Gb_Osc prefix */
	struct Blip_Buffer* outputs[4];
	struct Blip_Buffer* output;
	uint8_t * regs;
	int mode;
	int dac_off_amp;
	int last_amp;
	struct Blip_Synth const* good_synth;
	struct Blip_Synth const* med_synth;
	int delay;
	int length_ctr;
	unsigned phase;
	bool enabled;
	/* envelope */
	int  env_delay;
	int  volume;
	bool env_enabled;
};
typedef struct Gb_Square Gb_Square;

/* --- Gb_Sweep_Square: Gb_Square + sweep --- */
struct Gb_Sweep_Square
{
	/* Gb_Osc prefix */
	struct Blip_Buffer* outputs[4];
	struct Blip_Buffer* output;
	uint8_t * regs;
	int mode;
	int dac_off_amp;
	int last_amp;
	struct Blip_Synth const* good_synth;
	struct Blip_Synth const* med_synth;
	int delay;
	int length_ctr;
	unsigned phase;
	bool enabled;
	/* envelope */
	int  env_delay;
	int  volume;
	bool env_enabled;
	/* sweep */
	int  sweep_freq;
	int  sweep_delay;
	bool sweep_enabled;
	bool sweep_neg;
};
typedef struct Gb_Sweep_Square Gb_Sweep_Square;

/* --- Gb_Noise: Gb_Env + divider --- */
struct Gb_Noise
{
	/* Gb_Osc prefix */
	struct Blip_Buffer* outputs[4];
	struct Blip_Buffer* output;
	uint8_t * regs;
	int mode;
	int dac_off_amp;
	int last_amp;
	struct Blip_Synth const* good_synth;
	struct Blip_Synth const* med_synth;
	int delay;
	int length_ctr;
	unsigned phase;
	bool enabled;
	/* envelope */
	int  env_delay;
	int  volume;
	bool env_enabled;
	/* noise */
	int divider;	/* noise has more complex frequency divider setup*/
};
typedef struct Gb_Noise Gb_Noise;

/* --- Gb_Wave: Gb_Osc + wave RAM --- */
struct Gb_Wave
{
	/* Gb_Osc prefix */
	struct Blip_Buffer* outputs[4];
	struct Blip_Buffer* output;
	uint8_t * regs;
	int mode;
	int dac_off_amp;
	int last_amp;
	struct Blip_Synth const* good_synth;
	struct Blip_Synth const* med_synth;
	int delay;
	int length_ctr;
	unsigned phase;
	bool enabled;
	/* wave */
	int sample_buf;		/* last wave RAM byte read*/
	int agb_mask;		/* 0xFF if AGB features enabled, 0 otherwise*/
	uint8_t* wave_ram;	/* 32 bytes (64 nybbles)*/
};
typedef struct Gb_Wave Gb_Wave;

/*============================================================
	FORWARD DECLARATIONS
============================================================ */

/* All conversions to free functions are static (file scope), matching
 * the old class methods' linkage. */
static void Gb_Osc_reset(Gb_Osc *self);
static void Gb_Osc_clock_length(Gb_Osc *self);
static int  Gb_Osc_write_trig(Gb_Osc *self, int frame_phase, int max_len, int old_data);

static void Gb_Env_clock_envelope(Gb_Env *self);
static bool Gb_Env_write_register(Gb_Env *self, int frame_phase, int reg, int old, int data);

static bool Gb_Square_write_register(Gb_Square *self, int frame_phase, int reg, int old_data, int data);
static void Gb_Square_run(Gb_Square *self, int32_t time, int32_t end_time);

static void Gb_Sweep_Square_calc_sweep(Gb_Sweep_Square *self, bool update);
static void Gb_Sweep_Square_clock_sweep(Gb_Sweep_Square *self);

static void Gb_Noise_run(Gb_Noise *self, int32_t time, int32_t end_time);

static void Gb_Wave_run(Gb_Wave *self, int32_t time, int32_t end_time);

/* Blip_Buffer_destroy / _clear / _set_sample_rate were forward-declared
 * here but every call site already follows their definitions, so the
 * declarations were dead and have been removed.  Blip_Buffer_clock_rate_
 * factor still needs its forward decl -- it has one local to the
 * BLIP BUFFER section, where it is called once before its definition. */

/*============================================================
	RESET FREE FUNCTIONS (formerly in-class inline)
============================================================ */

static INLINE void Gb_Env_reset(Gb_Env *self)
{
	self->env_delay = 0;
	self->volume    = 0;
	Gb_Osc_reset( (Gb_Osc *)self );
}

static INLINE void Gb_Square_reset(Gb_Square *self)
{
	Gb_Env_reset( (Gb_Env *)self );
	self->delay = 0x40000000; /* never clocked until first trigger */
}

static INLINE void Gb_Sweep_Square_reset(Gb_Sweep_Square *self)
{
	self->sweep_freq    = 0;
	self->sweep_delay   = 0;
	self->sweep_enabled = false;
	self->sweep_neg     = false;
	Gb_Square_reset( (Gb_Square *)self );
}

static INLINE void Gb_Noise_reset(Gb_Noise *self)
{
	self->divider = 0;
	Gb_Env_reset( (Gb_Env *)self );
	self->delay = CLK_MUL_MUL_4;
}

static INLINE void Gb_Wave_reset(Gb_Wave *self)
{
	self->sample_buf = 0;
	Gb_Osc_reset( (Gb_Osc *)self );
}

/*============================================================
	INLINE FREE FUNCS
============================================================ */

static INLINE void Blip_Synth_offset( const Blip_Synth *self, int32_t t, int delta, Blip_Buffer* buf )
{
	/* Was split across Blip_Synth_offset (this thin wrapper) and
	 * Blip_Synth_offset_resampled (the worker); folded into one since
	 * every caller goes through this entry point. */
	uint32_t time = (uint32_t)t * buf->factor_ + buf->offset_;
	int32_t left, right, phase;
	int32_t *p;

	delta *= self->delta_factor;
	p = buf->buffer_ + (time >> BLIP_BUFFER_ACCURACY);
	phase = (int) (time >> (BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS) & BLIP_RES_MIN_ONE);

	left = p [0] + delta;

	right = (delta >> BLIP_PHASE_BITS) * phase;

	left  -= right;
	right += p [1];

	p [0] = left;
	p [1] = right;
}

static INLINE void Blip_Synth_volume( Blip_Synth *self, float v )
{
	/* (double)v is deliberate: it keeps the scale multiply in double
	 * precision.  The old form spelled this `(v * 1.0)`, which looks
	 * like a no-op but is what promoted the expression to double --
	 * dropping it would compute delta_factor in float instead. */
	self->delta_factor = (int)((double)v * (1L << BLIP_SAMPLE_BITS) + 0.5);
}

static INLINE int Gb_Wave_read( const Gb_Wave *self, unsigned addr )
{
	/* Gb_Wave_access(self,addr) was just `addr & 0x0F` with self unused,
	 * and the enabled==false branch computed the identical value -- so
	 * the index is `addr & 0x0F` unconditionally, always in [0,15].  The
	 * old `index < 0 ? 0xFF :` guard was therefore dead. */
	int index = addr & 0x0F;
	unsigned char const * wave_bank =
		&self->wave_ram[(~self->regs[0] & BANK40_MASK) >> 2 & self->agb_mask];

	return wave_bank[index];
}

static INLINE void Gb_Wave_write( Gb_Wave *self, unsigned addr, int data )
{
	/* See Gb_Wave_read: index is `addr & 0x0F` unconditionally, always
	 * in [0,15], so the old `if ( index >= 0 )` guard was dead. */
	int index = addr & 0x0F;
	unsigned char * wave_bank =
		&self->wave_ram[(~self->regs[0] & BANK40_MASK) >> 2 & self->agb_mask];

	wave_bank[index] = data;
}

/* Sized to hold one GBA frame's worth of interleaved stereo int16_t samples
 * at the libretro-advertised 32 kHz sample rate.
 *
 *   GBA frame rate     = 16777216 / SOUND_CLOCK_TICKS_      = 59.7275 Hz
 *   samples per frame  = soundSampleRate / 59.7275          = 535.79 stereo pairs
 *   int16_t per frame  = 535.79 * 2  (stereo, interleaved)  = 1071.58
 *   worst-case frame   = ceil(535.79) * 2                   = 1072
 *
 * The blip pipeline advances by exactly SOUND_CLOCK_TICKS * factor_ per
 * call, so per-frame production alternates between 535 and 536 stereo
 * pairs as the fractional accumulator carries over -- not bursty.  1088
 * (= 544 stereo pairs) is the worst-case 1072 plus an 8-pair cushion for
 * any timing-jitter rounding at the soundTicks==0 boundary.
 * process_sound_tick_fn clamps avail to sizeof/sizeof of this array so a
 * larger soundSampleRate cannot overrun, though samples beyond the
 * buffer would simply defer to the next frame in that case. */
static int16_t   soundFinalWave [1088];
long  soundSampleRate    = 32000;
int   SOUND_CLOCK_TICKS  = SOUND_CLOCK_TICKS_;
int   soundTicks         = SOUND_CLOCK_TICKS_;

static int soundEnableFlag   = 0x3ff; /* emulator channels enabled*/
static float const apu_vols [4] = { -0.25f, -0.5f, -1.0f, -0.25f };

static const int table [0x40] =
{
		0xFF10,     0,0xFF11,0xFF12,0xFF13,0xFF14,     0,     0,
		0xFF16,0xFF17,     0,     0,0xFF18,0xFF19,     0,     0,
		0xFF1A,     0,0xFF1B,0xFF1C,0xFF1D,0xFF1E,     0,     0,
		0xFF20,0xFF21,     0,     0,0xFF22,0xFF23,     0,     0,
		0xFF24,0xFF25,     0,     0,0xFF26,     0,     0,     0,
		     0,     0,     0,     0,     0,     0,     0,     0,
		0xFF30,0xFF31,0xFF32,0xFF33,0xFF34,0xFF35,0xFF36,0xFF37,
		0xFF38,0xFF39,0xFF3A,0xFF3B,0xFF3C,0xFF3D,0xFF3E,0xFF3F,
};

typedef struct
{
	int last_amp;
	int last_time;
	int shift;
	Blip_Buffer* output;
} gba_pcm_t;

typedef struct
{
	bool enabled;
	uint8_t   fifo [32];
	int  count;
	int  dac;
	int  readIndex;
	int  writeIndex;
	int     which;
	int  timer;
	gba_pcm_t pcm;
} gba_pcm_fifo_t;

static gba_pcm_fifo_t   pcm [2];


static Blip_Synth pcm_synth; /* 32 kHz, 16 kHz, 8 kHz */

static Blip_Buffer bufs_buffer [BUFS_SIZE];
static int mixer_samples_read;

static void gba_pcm_init (void)
{
	pcm[0].pcm.output    = 0;
	pcm[0].pcm.last_time = 0;
	pcm[0].pcm.last_amp  = 0;
	pcm[0].pcm.shift     = 0;

	pcm[1].pcm.output    = 0;
	pcm[1].pcm.last_time = 0;
	pcm[1].pcm.last_amp  = 0;
	pcm[1].pcm.shift     = 0;
}

static void gba_pcm_apply_control( int pcm_idx, int idx )
{
	Blip_Buffer* out;
	int ch = 0;
	pcm[pcm_idx].pcm.shift = ~ioMem [SGCNT0_H] >> (2 + idx) & 1;

	if ( (ioMem [NR52] & 0x80) )
		ch = ioMem [SGCNT0_H+1] >> (idx << 2) & 3;

	out = 0;
	switch ( ch )
	{
		case 1:
			out = &bufs_buffer[1];
			break;
		case 2:
			out = &bufs_buffer[0];
			break;
		case 3:
			out = &bufs_buffer[2];
			break;
	}

	if ( pcm[pcm_idx].pcm.output != out )
	{
		if ( pcm[pcm_idx].pcm.output )
			Blip_Synth_offset(&pcm_synth,  SOUND_CLOCK_TICKS - soundTicks, -pcm[pcm_idx].pcm.last_amp, pcm[pcm_idx].pcm.output );
		pcm[pcm_idx].pcm.last_amp = 0;
		pcm[pcm_idx].pcm.output = out;
	}
}

/*============================================================
	GB APU
============================================================ */

/* 0: Square 1, 1: Square 2, 2: Wave, 3: Noise */
#define OSC_COUNT 4

/* Resets hardware to initial power on state BEFORE boot ROM runs. Mode selects*/
/* sound hardware. Additional AGB wave features are enabled separately.*/
#define MODE_AGB	2

#define START_ADDR	0xFF10
#define END_ADDR	0xFF3F

/* Reads and writes must be within the START_ADDR to END_ADDR range, inclusive.*/
/* Addresses outside this range are not mapped to the sound hardware.*/
#define REGISTER_COUNT	48
#define REGS_SIZE 64

/* Clock rate that sound hardware runs at.
 * formula: 4194304 * 4 
 * */
#define CLOCK_RATE 16777216

struct gb_apu_t
{
	bool		reduce_clicks_;
	uint8_t		regs[REGS_SIZE]; /* last values written to registers */
	int32_t		last_time;	/* time sound emulator has been run to */
	int32_t		frame_time;	/* time of next frame sequencer action */
	int32_t		frame_period;       /* clocks between each frame sequencer step */
	int32_t         frame_phase;    /* phase of next frame sequencer step */
	float		volume_;
	Gb_Osc*		oscs [OSC_COUNT];
	Gb_Sweep_Square square1;
	Gb_Square       square2;
	Gb_Wave         wave;
	Gb_Noise        noise;
	Blip_Synth	good_synth;
	Blip_Synth	med_synth;
} gb_apu;

/* Format of save state. Should be stable across versions of the library, */
/* with earlier versions properly opening later save states. Includes some */
/* room for expansion so the state size shouldn't increase. */
typedef int gb_apu_state_val_t;
enum { gb_apu_state_format0 = 0x50414247 };

struct gb_apu_state_t
{
	/* Values stored as plain int so your code can read/write them easily. */
	/* Structure can NOT be written to disk, since format is not portable. */

	gb_apu_state_val_t format;   /* format of all following data */
	gb_apu_state_val_t version;  /* later versions just add fields to end */

	unsigned char regs [0x40];
	gb_apu_state_val_t frame_time;
	gb_apu_state_val_t frame_phase;

	gb_apu_state_val_t sweep_freq;
	gb_apu_state_val_t sweep_delay;
	gb_apu_state_val_t sweep_enabled;
	gb_apu_state_val_t sweep_neg;
	gb_apu_state_val_t noise_divider;
	gb_apu_state_val_t wave_buf;

	gb_apu_state_val_t delay      [4];
	gb_apu_state_val_t length_ctr [4];
	gb_apu_state_val_t phase      [4];
	gb_apu_state_val_t enabled    [4];

	gb_apu_state_val_t env_delay   [3];
	gb_apu_state_val_t env_volume  [3];
	gb_apu_state_val_t env_enabled [3];

	gb_apu_state_val_t unused  [13]; /* for future expansion */
};
typedef struct gb_apu_state_t gb_apu_state_t;

#define VOL_REG 0xFF24
#define STEREO_REG 0xFF25
#define STATUS_REG 0xFF26
#define WAVE_RAM 0xFF30
#define POWER_MASK 0x80

#define OSC_COUNT 4

static void gb_apu_reduce_clicks( bool reduce )
{
	int dac_off_amp;
	gb_apu.reduce_clicks_ = reduce;

	/* Click reduction makes DAC off generate same output as volume 0*/
	dac_off_amp = 0;

	gb_apu.oscs[0]->dac_off_amp = dac_off_amp;
	gb_apu.oscs[1]->dac_off_amp = dac_off_amp;
	gb_apu.oscs[2]->dac_off_amp = dac_off_amp;
	gb_apu.oscs[3]->dac_off_amp = dac_off_amp;

	/* AGB always eliminates clicks on wave channel using same method*/
	gb_apu.wave.dac_off_amp = -DAC_BIAS;
}

static void gb_apu_synth_volume( int iv )
{
	float v = gb_apu.volume_ * 0.60 / OSC_COUNT / 15 /*steps*/ / 8 /*master vol range*/ * iv;
	Blip_Synth_volume(&gb_apu.good_synth, v);
	Blip_Synth_volume(&gb_apu.med_synth, v);
}

static void gb_apu_apply_volume (void)
{
	int data, left, right, vol_tmp;
	data  = gb_apu.regs [VOL_REG - START_ADDR];
	left  = data >> 4 & 7;
	right = data & 7;
	vol_tmp = left < right ? right : left;
	gb_apu_synth_volume( vol_tmp + 1 );
}

static void gb_apu_silence_osc( Gb_Osc *o )
{
	int delta;

	delta = -o->last_amp;
	if ( delta )
	{
		o->last_amp = 0;
		if ( o->output )
		{
			Blip_Synth_offset(&gb_apu.med_synth, gb_apu.last_time, delta, o->output);
		}
	}
}

static void gb_apu_run_until_( int32_t end_time )
{
	int32_t time;

	do{
		/* run oscillators*/
		time = end_time;
		if ( time > gb_apu.frame_time )
			time = gb_apu.frame_time;

		Gb_Square_run((Gb_Square *)&gb_apu.square1, gb_apu.last_time, time);
		Gb_Square_run(&gb_apu.square2, gb_apu.last_time, time);
		Gb_Wave_run(&gb_apu.wave, gb_apu.last_time, time);
		Gb_Noise_run(&gb_apu.noise, gb_apu.last_time, time);
		gb_apu.last_time = time;

		if ( time == end_time )
			break;

		/* run frame sequencer*/
		gb_apu.frame_time += gb_apu.frame_period * CLK_MUL;
		switch ( gb_apu.frame_phase++ )
		{
			case 2:
			case 6:
				/* 128 Hz*/
				Gb_Sweep_Square_clock_sweep(&gb_apu.square1);
				/* fallthrough -- 128 Hz steps also clock the 256 Hz length counters */
			case 0:
			case 4:
				/* 256 Hz*/
				Gb_Osc_clock_length((Gb_Osc *)&gb_apu.square1);
				Gb_Osc_clock_length((Gb_Osc *)&gb_apu.square2);
				Gb_Osc_clock_length((Gb_Osc *)&gb_apu.wave);
				Gb_Osc_clock_length((Gb_Osc *)&gb_apu.noise);
				break;

			case 7:
				/* 64 Hz*/
				gb_apu.frame_phase = 0;
				Gb_Env_clock_envelope((Gb_Env *)&gb_apu.square1);
				Gb_Env_clock_envelope((Gb_Env *)&gb_apu.square2);
				Gb_Env_clock_envelope((Gb_Env *)&gb_apu.noise);
		}
	}while(1);
}

static INLINE void Gb_Sweep_Square_write_register(Gb_Sweep_Square *self, int frame_phase, int reg, int old_data, int data)
{
        if ( reg == 0 && self->sweep_enabled && self->sweep_neg && !(data & 0x08) )
                self->enabled = false; /* sweep negate disabled after used */

        if ( Gb_Square_write_register( (Gb_Square *)self, frame_phase, reg, old_data, data ) )
        {
                self->sweep_freq = GB_OSC_FREQUENCY( self );
                self->sweep_neg = false;
                /* reload sweep timer (was reload_sweep_timer macro): */
                self->sweep_delay = (self->regs [0] & PERIOD_MASK) >> 4;
                if ( !self->sweep_delay )
                        self->sweep_delay = 8;
                self->sweep_enabled = (self->regs [0] & (PERIOD_MASK | SHIFT_MASK)) != 0;
                if ( self->regs [0] & SHIFT_MASK )
                        Gb_Sweep_Square_calc_sweep( self, false );
        }
}

static INLINE void Gb_Wave_write_register(Gb_Wave *self, int frame_phase, int reg, int old_data, int data)
{
        switch ( reg )
	{
		case 0:
			if ( !GB_WAVE_DAC_ENABLED( self ) )
				self->enabled = false;
			break;

		case 1:
			self->length_ctr = 256 - data;
			break;

		case 4:
			if ( Gb_Osc_write_trig( (Gb_Osc *)self, frame_phase, 256, old_data ) )
			{
				if ( !GB_WAVE_DAC_ENABLED( self ) )
					self->enabled = false;
				self->phase = 0;
				self->delay = ((2048 - GB_OSC_FREQUENCY( self )) * (CLK_MUL_MUL_2)) + CLK_MUL_MUL_6;
			}
	}
}

static INLINE void Gb_Noise_write_register(Gb_Noise *self, int frame_phase, int reg, int old_data, int data)
{
        if ( Gb_Env_write_register( (Gb_Env *)self, frame_phase, reg, old_data, data ) )
        {
                self->phase = 0x7FFF;
                self->delay += CLK_MUL_MUL_8;
        }
}

static void gb_apu_write_osc( int index, int reg, int old_data, int data )
{
        reg -= index * 5;
        switch ( index )
	{
		case 0:
			Gb_Sweep_Square_write_register( &gb_apu.square1, gb_apu.frame_phase, reg, old_data, data );
			break;
		case 1:
			Gb_Square_write_register( &gb_apu.square2, gb_apu.frame_phase, reg, old_data, data );
			break;
		case 2:
			Gb_Wave_write_register( &gb_apu.wave, gb_apu.frame_phase, reg, old_data, data );
			break;
		case 3:
			Gb_Noise_write_register( &gb_apu.noise, gb_apu.frame_phase, reg, old_data, data );
			break;
	}
}

static INLINE int gb_apu_calc_output( int osc )
{
	int bits = gb_apu.regs [STEREO_REG - START_ADDR] >> osc;
	return (bits >> 3 & 2) | (bits & 1);
}

static void gb_apu_write_register( int32_t time, unsigned addr, int data )
{
	int i;
	int reg = addr - START_ADDR;
	if ( (unsigned) reg >= REGISTER_COUNT )
		return;

	if ( addr < STATUS_REG && !(gb_apu.regs [STATUS_REG - START_ADDR] & POWER_MASK) )
		return;	/* Power is off*/

	if ( time > gb_apu.last_time )
		gb_apu_run_until_( time );

	if ( addr >= WAVE_RAM )
	{
		Gb_Wave_write(&gb_apu.wave, addr, data);
	}
	else
	{
		int old_data = gb_apu.regs [reg];
		gb_apu.regs [reg] = data;

		if ( addr < VOL_REG )
			gb_apu_write_osc( reg / 5, reg, old_data, data );	/* Oscillator*/
		else if ( addr == VOL_REG && data != old_data )
		{
			/* Master volume*/
			{
				int i;
				for ( i = OSC_COUNT; --i >= 0; )
				gb_apu_silence_osc( gb_apu.oscs [i] );
			}

			gb_apu_apply_volume();
		}
		else if ( addr == STEREO_REG )
		{
			/* Stereo panning*/
			{
				int i;
				for ( i = OSC_COUNT; --i >= 0; )
			{
				Gb_Osc *o = gb_apu.oscs [i];
				Blip_Buffer* out = o->outputs [gb_apu_calc_output( i )];
				if ( o->output != out )
				{
					gb_apu_silence_osc( o );
					o->output = out;
				}
			}
			} }
		else if ( addr == STATUS_REG && (data ^ old_data) & POWER_MASK )
		{
			/* Power control*/
			gb_apu.frame_phase = 0;
			{
				int i;
				for ( i = OSC_COUNT; --i >= 0; )
				gb_apu_silence_osc( gb_apu.oscs [i] );
			}

			for ( i = 0; i < 32; i++ )
				gb_apu.regs [i] = 0;

			Gb_Sweep_Square_reset(&gb_apu.square1);
			Gb_Square_reset(&gb_apu.square2);
			Gb_Wave_reset(&gb_apu.wave);
			Gb_Noise_reset(&gb_apu.noise);

			gb_apu_apply_volume();

			gb_apu.square1.length_ctr = 64;
			gb_apu.square2.length_ctr = 64;
			gb_apu.wave   .length_ctr = 256;
			gb_apu.noise  .length_ctr = 64;

			gb_apu.regs [STATUS_REG - START_ADDR] = data;
		}
	}
}

static void gb_apu_reset( uint32_t mode, bool agb_wave )
{
	int i;
	/* Initial wave RAM contents.  C89 forbids declarations mid-block, so
	 * keep this at the top of the function rather than just before its use. */
	static unsigned char const initial_wave [2] [16] = {
		{0x84,0x40,0x43,0xAA,0x2D,0x78,0x92,0x3C,0x60,0x59,0x59,0xB0,0x34,0xB8,0x2E,0xDA},
		{0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF},
	};
	/* Hardware mode*/
	mode = MODE_AGB; /* using AGB wave features implies AGB hardware*/
	gb_apu.wave.agb_mask = 0xFF;
	gb_apu.oscs [0]->mode = mode;
	gb_apu.oscs [1]->mode = mode;
	gb_apu.oscs [2]->mode = mode;
	gb_apu.oscs [3]->mode = mode;
	gb_apu_reduce_clicks( gb_apu.reduce_clicks_ );

	/* Reset state*/
	gb_apu.frame_time  = 0;
	gb_apu.last_time   = 0;
	gb_apu.frame_phase = 0;

	for ( i = 0; i < 32; i++ )
		gb_apu.regs [i] = 0;

	Gb_Sweep_Square_reset(&gb_apu.square1);
	Gb_Square_reset(&gb_apu.square2);
	Gb_Wave_reset(&gb_apu.wave);
	Gb_Noise_reset(&gb_apu.noise);

	gb_apu_apply_volume();

	gb_apu.square1.length_ctr = 64;
	gb_apu.square2.length_ctr = 64;
	gb_apu.wave   .length_ctr = 256;
	gb_apu.noise  .length_ctr = 64;

	/* Load initial wave RAM (initial_wave is declared at the top of the function) */
	{
		int b;
		for ( b = 2; --b >= 0; )
	{
		/* Init both banks (does nothing if not in AGB mode)*/
		gb_apu_write_register( 0, 0xFF1A, b * 0x40 );
		{
			unsigned int i_inner;
			for ( i_inner = 0; i_inner < sizeof initial_wave [0]; i_inner++ )
			gb_apu_write_register( 0, i_inner + WAVE_RAM, initial_wave [1] [i_inner] );
		}
	}
	}
}

static void gb_apu_new(void)
{
	int i;

	gb_apu.wave.wave_ram = &gb_apu.regs [WAVE_RAM - START_ADDR];

	gb_apu.oscs [0] = (Gb_Osc *)&gb_apu.square1;
	gb_apu.oscs [1] = (Gb_Osc *)&gb_apu.square2;
	gb_apu.oscs [2] = (Gb_Osc *)&gb_apu.wave;
	gb_apu.oscs [3] = (Gb_Osc *)&gb_apu.noise;

	for ( i = OSC_COUNT; --i >= 0; )
	{
		Gb_Osc *o = gb_apu.oscs [i];
		o->regs        = &gb_apu.regs [i * 5];
		o->output      = 0;
		o->outputs [0] = 0;
		o->outputs [1] = 0;
		o->outputs [2] = 0;
		o->outputs [3] = 0;
		o->good_synth  = &gb_apu.good_synth;
		o->med_synth   = &gb_apu.med_synth;
	}

	gb_apu.reduce_clicks_ = false;
	gb_apu.frame_period = 4194304 / 512; /* 512 Hz*/

	gb_apu.volume_ = 1.0;
	gb_apu_reset(MODE_AGB, false);
}



static void gb_apu_set_output( Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right, int osc )
{
	int i;

	i = osc;
	do
	{
		Gb_Osc *o = gb_apu.oscs [i];
		o->outputs [1] = right;
		o->outputs [2] = left;
		o->outputs [3] = center;
		o->output = o->outputs [gb_apu_calc_output( i )];
		++i;
	}
	while ( i < osc );
}

static void gb_apu_volume( float v )
{
	if ( gb_apu.volume_ != v )
	{
		gb_apu.volume_ = v;
		gb_apu_apply_volume();
	}
}

static void gb_apu_apply_stereo (void)
{
	int i;

	for ( i = OSC_COUNT; --i >= 0; )
	{
		Gb_Osc *o = gb_apu.oscs [i];
		Blip_Buffer* out = o->outputs [gb_apu_calc_output( i )];
		if ( o->output != out )
		{
			gb_apu_silence_osc( o );
			o->output = out;
		}
	}
}

/* C requires the ternary's branch assignments to be parenthesized;
 * in C++ this was loose enough to parse, in C the conditional
 * operator has lower precedence than assignment so the unparenthesized
 * form mis-parses. */
/* save ? store : load.  Kept as a macro rather than folded into its 17
 * call sites: it is type-generic (the fields it copies are variously
 * int, bool and arrays) and expanding it everywhere would bury
 * gb_apu_save_load under 17 hand-written ternaries.  The original
 * definition was the same logic wrapped in erratic whitespace. */
#define REFLECT( x, y )  ( save ? ((io->y) = (x)) : ((x) = (io->y)) )

static INLINE const char* gb_apu_save_load( gb_apu_state_t* io, bool save )
{
	int format, version;

	format = gb_apu_state_format0;

	REFLECT( format, format );
	if ( format != gb_apu_state_format0 )
		return "Unsupported sound save state format";

	version = 0;
	REFLECT( version, version );

	/* Registers and wave RAM*/
	if ( save )
		memcpy( io->regs, gb_apu.regs, sizeof io->regs );
	else
		memcpy( gb_apu.regs, io->regs, sizeof(gb_apu.regs) );

	/* Frame sequencer*/
	REFLECT( gb_apu.frame_time,  frame_time  );
	REFLECT( gb_apu.frame_phase, frame_phase );

	REFLECT( gb_apu.square1.sweep_freq,    sweep_freq );
	REFLECT( gb_apu.square1.sweep_delay,   sweep_delay );
	REFLECT( gb_apu.square1.sweep_enabled, sweep_enabled );
	REFLECT( gb_apu.square1.sweep_neg,     sweep_neg );

	REFLECT( gb_apu.noise.divider,         noise_divider );
	REFLECT( gb_apu.wave.sample_buf,       wave_buf );

	return 0;
}

/* second function to avoid inline limits of some compilers*/
static INLINE void gb_apu_save_load2( gb_apu_state_t* io, bool save )
{
	int i;
	for ( i = OSC_COUNT; --i >= 0; )
	{
		Gb_Osc *osc = gb_apu.oscs [i];
		REFLECT( osc->delay,      delay      [i] );
		REFLECT( osc->length_ctr, length_ctr [i] );
		REFLECT( osc->phase,      phase      [i] );
		REFLECT( osc->enabled,    enabled    [i] );

		if ( i != 2 )
		{
			int j = 2 < i ? 2 : i;
			Gb_Env *env = (Gb_Env *)osc;
			REFLECT( env->env_delay,   env_delay   [j] );
			REFLECT( env->volume,      env_volume  [j] );
			REFLECT( env->env_enabled, env_enabled [j] );
		}
	}
}

static const char * gb_apu_load_state( gb_apu_state_t const* in )
{
	const char * err = gb_apu_save_load( (gb_apu_state_t*)in, false );
	if ( err )
		return err;
	gb_apu_save_load2( (gb_apu_state_t*)in, false );

	gb_apu_apply_stereo();
	gb_apu_synth_volume( 0 );          /* suppress output for the moment*/
	gb_apu_run_until_( gb_apu.last_time );    /* get last_amp updated*/
	gb_apu_apply_volume();             /* now use correct volume*/

	return 0;
}

/*============================================================
	GB OSCS
============================================================ */

#define TRIGGER_MASK 0x80
#define LENGTH_ENABLED 0x40

#define VOLUME_SHIFT_PLUS_FOUR	6
#define SIZE20_MASK 0x20

static void Gb_Osc_reset(Gb_Osc *self)
{
        self->output   = 0;
        self->last_amp = 0;
        self->delay    = 0;
        self->phase    = 0;
        self->enabled  = false;
}

static INLINE void Gb_Osc_update_amp(Gb_Osc *self, int32_t time, int new_amp)
{
	int delta = new_amp - self->last_amp;
        if ( delta )
        {
                self->last_amp = new_amp;
                Blip_Synth_offset( self->med_synth, time, delta, self->output );
        }
}

static void Gb_Osc_clock_length(Gb_Osc *self)
{
        if ( (self->regs [4] & LENGTH_ENABLED) && self->length_ctr )
        {
                if ( --self->length_ctr <= 0 )
                        self->enabled = false;
        }
}

static INLINE int Gb_Env_reload_env_timer(Gb_Env *self)
{
        int raw = self->regs [2] & 7;
        self->env_delay = (raw ? raw : 8);
        return raw;
}

static void Gb_Env_clock_envelope(Gb_Env *self)
{
        if ( self->env_enabled && --self->env_delay <= 0 && Gb_Env_reload_env_timer( self ) )
        {
                int v = self->volume + (self->regs [2] & 0x08 ? +1 : -1);
                if ( 0 <= v && v <= 15 )
                        self->volume = v;
                else
                        self->env_enabled = false;
        }
}

static void Gb_Sweep_Square_calc_sweep(Gb_Sweep_Square *self, bool update)
{
        int const shift = self->regs [0] & SHIFT_MASK;
        int const delta = self->sweep_freq >> shift;
        int freq;
        /* hoisted above freq's initializer, which reads sweep_neg;
         * nothing between here and there depends on the ordering. */
        self->sweep_neg = (self->regs [0] & 0x08) != 0;
        freq = self->sweep_freq + (self->sweep_neg ? -delta : delta);

        if ( freq > 0x7FF )
                self->enabled = false;
        else if ( shift && update )
        {
                self->sweep_freq = freq;

                self->regs [3] = freq & 0xFF;
                self->regs [4] = (self->regs [4] & ~0x07) | (freq >> 8 & 0x07);
        }
}

static void Gb_Sweep_Square_clock_sweep(Gb_Sweep_Square *self)
{
        if ( --self->sweep_delay <= 0 )
        {
		/* reload_sweep_timer macro inlined: */
		self->sweep_delay = (self->regs [0] & PERIOD_MASK) >> 4;
		if ( !self->sweep_delay )
			self->sweep_delay = 8;
                if ( self->sweep_enabled && (self->regs [0] & PERIOD_MASK) )
                {
                        Gb_Sweep_Square_calc_sweep( self, true  );
                        Gb_Sweep_Square_calc_sweep( self, false );
                }
        }
}

/* write_register */

static int Gb_Osc_write_trig(Gb_Osc *self, int frame_phase, int max_len, int old_data)
{
        int data = self->regs [4];

        if ( (frame_phase & 1) && !(old_data & LENGTH_ENABLED) && self->length_ctr )
        {
                if ( (data & LENGTH_ENABLED))
                        self->length_ctr--;
        }

        if ( data & TRIGGER_MASK )
        {
                self->enabled = true;
                if ( !self->length_ctr )
                {
                        self->length_ctr = max_len;
                        if ( (frame_phase & 1) && (data & LENGTH_ENABLED) )
                                self->length_ctr--;
                }
        }

        if ( !self->length_ctr )
                self->enabled = false;

        return data & TRIGGER_MASK;
}

static INLINE void Gb_Env_zombie_volume(Gb_Env *self, int old, int data)
{
	int v = self->volume;

	/* CGB-05 behavior, very close to AGB behavior as well */
	if ( (old ^ data) & 8 )
	{
		if ( !(old & 8) )
		{
			v++;
			if ( old & 7 )
				v++;
		}

		v = 16 - v;
	}
	else if ( (old & 0x0F) == 8 )
		v++;
	self->volume = v & 0x0F;
}

static bool Gb_Env_write_register(Gb_Env *self, int frame_phase, int reg, int old, int data)
{
        int const max_len = 64;

        switch ( reg )
	{
		case 1:
			self->length_ctr = max_len - (data & (max_len - 1));
			break;

		case 2:
			/* GB_ENV_DAC_ENABLED() uses self->regs */
			if ( !( self->regs [2] & 0xF8 ) )
				self->enabled = false;

			Gb_Env_zombie_volume( self, old, data );

			if ( (data & 7) && self->env_delay == 8 )
			{
				self->env_delay = 1;
				Gb_Env_clock_envelope( self ); /* TODO: really happens at next length clock */
			}
			break;

		case 4:
			if ( Gb_Osc_write_trig( (Gb_Osc *)self, frame_phase, max_len, old ) )
			{
				self->volume = self->regs [2] >> 4;
				Gb_Env_reload_env_timer( self );
				self->env_enabled = true;
				if ( frame_phase == 7 )
					self->env_delay++;
				if ( !( self->regs [2] & 0xF8 ) )
					self->enabled = false;
				return true;
			}
	}
        return false;
}

static bool Gb_Square_write_register(Gb_Square *self, int frame_phase, int reg, int old_data, int data)
{
        bool result = Gb_Env_write_register( (Gb_Env *)self, frame_phase, reg, old_data, data );
        if ( result )
                self->delay = (self->delay & (CLK_MUL_MUL_4 - 1)) + ((2048 - GB_OSC_FREQUENCY( self )) * (CLK_MUL_MUL_4));
        return result;
}


static void Gb_Square_run(Gb_Square *self, int32_t time, int32_t end_time)
{
        int ph;
        int vol;
        Blip_Buffer* out;
        int32_t duty_offset, duty;
        int duty_code;
        /* regs[] is uint8_t*, so a char-typed lvalue: every store the compiler
         * cannot prove disjoint (the int32_t* blip-buffer writes inside the
         * Gb_Osc_update_amp call below) forces it to reload regs[3]/regs[4].
         * Nothing in this function writes regs, so read the 11-bit frequency
         * once into a plain int and let it stay in a register. */
        int const freq = GB_OSC_FREQUENCY( self );
        /* Calc duty and phase.  The two duty tables and the two AGB
         * adjustments that followed them (`duty_offset -= duty` and
         * `duty = 8 - duty`) are all pure functions of the 2-bit
         * duty_code, so the adjusted results are precomputed here:
         *   duty_offset = duty_offsets[c] - duties[c]
         *   duty        = 8 - duties[c]
         * That drops a subtract and a reverse-subtract per call and
         * removes the data dependency between the two values.
         * duty_offset can be negative (codes 1 and 2 yield -1), hence
         * the signed table. */
        static signed   char const duty_offsets [4] = {  0, -1, -1,  1 };
        static unsigned char const duties       [4] = {  7,  6,  4,  2 };
        duty_code = self->regs [1] >> 6;
        duty_offset = duty_offsets [duty_code];
        duty = duties [duty_code];
        ph = (self->phase + duty_offset) & 7;

        /* Determine what will be generated*/
        vol = 0;
        out = self->output;
        if ( out )
        {
                int amp = self->dac_off_amp;
                if ( GB_ENV_DAC_ENABLED( self ) )
                {
                        if ( self->enabled )
                                vol = self->volume;

			amp = -(vol >> 1);

                        /* Play inaudible frequencies as constant amplitude*/
                        if ( freq >= 0x7FA && self->delay < CLK_MUL_MUL_32 )
                        {
                                amp += (vol * duty) >> 3;
                                vol = 0;
                        }

                        if ( ph < duty )
                        {
                                amp += vol;
                                vol = -vol;
                        }
                }
                Gb_Osc_update_amp( (Gb_Osc *)self, time, amp );
        }

        /* Generate wave*/
        time += self->delay;
        if ( time < end_time )
        {
                int const per = (2048 - freq) * (CLK_MUL_MUL_4);
                if ( !vol )
                {
                        /* Maintain phase when not playing*/
                        int count = (end_time - time + per - 1) / per;
                        ph += count; /* will be masked below*/
                        time += (int32_t) count * per;
                }
                else
                {
                        /* Output amplitude transitions*/
                        int delta = vol;
                        do
                        {
                                ph = (ph + 1) & 7;
                                if ( ph == 0 || ph == duty )
                                {
                                        Blip_Synth_offset( self->good_synth, time, delta, out );
                                        delta = -delta;
                                }
                                time += per;
                        }
                        while ( time < end_time );

                        if ( delta != vol )
                                self->last_amp -= delta;
                }
                self->phase = (ph - duty_offset) & 7;
        }
        self->delay = time - end_time;
}

/* Quickly runs LFSR for a large number of clocks. For use when noise is generating*/
/* no sound.*/
static unsigned run_lfsr( unsigned s, unsigned mask, int count )
{
	/* optimization used in several places:*/
	/* ((s & (1 << b)) << n) ^ ((s & (1 << b)) << (n + 1)) = (s & (1 << b)) * (3 << n)*/

	if ( mask == 0x4000 )
	{
		if ( count >= 32767 )
			count %= 32767;

		/* Convert from Fibonacci to Galois configuration,*/
		/* shifted left 1 bit*/
		s ^= (s & 1) * 0x8000;

		/* Each iteration is equivalent to clocking LFSR 255 times*/
		while ( (count -= 255) > 0 )
			s ^= ((s & 0xE) << 12) ^ ((s & 0xE) << 11) ^ (s >> 3);
		count += 255;

		/* Each iteration is equivalent to clocking LFSR 15 times*/
		/* (interesting similarity to single clocking below)*/
		while ( (count -= 15) > 0 )
			s ^= ((s & 2) * (3 << 13)) ^ (s >> 1);
		count += 15;

		/* Remaining singles*/
		do{
			--count;
			s = ((s & 2) * (3 << 13)) ^ (s >> 1);
		}while(count >= 0);

		/* Convert back to Fibonacci configuration*/
		s &= 0x7FFF;
	}
	else if ( count < 8)
	{
		/* won't fully replace upper 8 bits, so have to do the unoptimized way*/
		do{
			--count;
			s = (s >> 1 | mask) ^ (mask & -((s - 1) & 2));
		}while(count >= 0);
	}
	else
	{
		if ( count > 127 )
		{
			count %= 127;
			if ( !count )
				count = 127; /* must run at least once*/
		}

		/* Need to keep one extra bit of history*/
		s = s << 1 & 0xFF;

		/* Convert from Fibonacci to Galois configuration,*/
		/* shifted left 2 bits*/
		s ^= (s & 2) << 7;

		/* Each iteration is equivalent to clocking LFSR 7 times*/
		/* (interesting similarity to single clocking below)*/
		while ( (count -= 7) > 0 )
			s ^= ((s & 4) * (3 << 5)) ^ (s >> 1);
		count += 7;

		/* Remaining singles*/
		while ( --count >= 0 )
			s = ((s & 4) * (3 << 5)) ^ (s >> 1);

		/* Convert back to Fibonacci configuration and*/
		/* repeat last 8 bits above significant 7*/
		s = (s << 7 & 0x7F80) | (s >> 1 & 0x7F);
	}

	return s;
}

static void Gb_Noise_run(Gb_Noise *self, int32_t time, int32_t end_time)
{
        /* Determine what will be generated*/
        int vol = 0;
        Blip_Buffer* out;
        /* hoisted decls for C89.  Table is pre-multiplied by CLK_MUL so
         * the `* CLK_MUL` that used to run on every call is gone; the
         * factor stays visible in the initializer. */
        static unsigned char const period1s [8] = {
                1*CLK_MUL, 2*CLK_MUL, 4*CLK_MUL,  6*CLK_MUL,
                8*CLK_MUL, 10*CLK_MUL, 12*CLK_MUL, 14*CLK_MUL
        };
        int period1;

        out = self->output;
        if ( out )
        {
                int amp = self->dac_off_amp;
                if ( GB_ENV_DAC_ENABLED( self ) )
                {
                        if ( self->enabled )
                                vol = self->volume;

			amp = -(vol >> 1);

                        if ( !(self->phase & 1) )
                        {
                                amp += vol;
                                vol = -vol;
                        }
                }

                /* AGB negates final output*/
		vol = -vol;
		amp    = -amp;

                Gb_Osc_update_amp( (Gb_Osc *)self, time, amp );
        }

        /* Run timer and calculate time of next LFSR clock*/
        period1 = period1s [self->regs [3] & 7];
        {
                int count;
                int extra = (end_time - time) - self->delay;
                int const per2 = GB_NOISE_PERIOD2( self, 8 );
                time += self->delay + ((self->divider ^ (per2 >> 1)) & (per2 - 1)) * period1;

                count = (extra < 0 ? 0 : (extra + period1 - 1) / period1);
                self->divider = (self->divider - count) & PERIOD2_MASK;
                self->delay = count * period1 - extra;
        }

        /* Generate wave*/
        if ( time < end_time )
        {
                unsigned const mask = GB_NOISE_LFSR_MASK( self );
                unsigned bits = self->phase;

                int per = GB_NOISE_PERIOD2( self, period1 * 8 );
                if ( GB_NOISE_PERIOD2_INDEX( self ) >= 0xE )
                {
                        time = end_time;
                }
                else if ( !vol )
                {
                        /* Maintain phase when not playing*/
                        int count = (end_time - time + per - 1) / per;
                        time += (int32_t) count * per;
                        bits = run_lfsr( bits, ~mask, count );
                }
                else
                {
                        /* Output amplitude transitions*/
                        int delta = -vol;
                        do
                        {
                                unsigned changed = bits + 1;
                                bits = bits >> 1 & mask;
                                if ( changed & 2 )
                                {
                                        bits |= ~mask;
                                        delta = -delta;
                                        Blip_Synth_offset( self->med_synth, time, delta, out );
                                }
                                time += per;
                        }
                        while ( time < end_time );

                        if ( delta == vol )
                                self->last_amp += delta;
                }
                self->phase = bits;
        }
}

static void Gb_Wave_run(Gb_Wave *self, int32_t time, int32_t end_time)
{
        /* Calc volume*/
        static unsigned char const volumes [8] = { 0, 4, 2, 1, 3, 3, 3, 3 };
        int const volume_idx = self->regs [2] >> 5 & (self->agb_mask | 3); /* 2 bits on DMG/CGB, 3 on AGB*/
        int const volume_mul = volumes [volume_idx];

        /* Determine what will be generated*/
        int playing = false;
        Blip_Buffer* out;
        /* Same aliasing reason as Gb_Square_run: hoist the regs[3]/regs[4]
         * read so the Gb_Osc_update_amp store cannot force a reload. */
        int const freq = GB_OSC_FREQUENCY( self );

        out = self->output;
        if ( out )
        {
                int amp = self->dac_off_amp;
                if ( GB_WAVE_DAC_ENABLED( self ) )
                {
                        /* Play inaudible frequencies as constant amplitude*/
                        amp = 128; /* really depends on average of all samples in wave*/

                        /* if delay is larger, constant amplitude won't start yet*/
                        if ( freq <= 0x7FB || self->delay > CLK_MUL_MUL_15 )
                        {
                                if ( volume_mul )
                                        playing = (int) self->enabled;

                                amp = (self->sample_buf << (self->phase << 2 & 4) & 0xF0) * playing;
                        }

                        amp = ((amp * volume_mul) >> VOLUME_SHIFT_PLUS_FOUR) - DAC_BIAS;
                }
                Gb_Osc_update_amp( (Gb_Osc *)self, time, amp );
        }

        /* Generate wave*/
        time += self->delay;
        if ( time < end_time )
        {
                int ph;
                unsigned char const* wave = self->wave_ram;

                /* wave size and bank*/
                int const flags = self->regs [0] & self->agb_mask;
                int const wave_mask = (flags & SIZE20_MASK) | 0x1F;
                int swap_banks = 0;
                int per;
                if ( flags & BANK40_MASK)
                {
                        swap_banks = flags & SIZE20_MASK;
                        wave += BANK_SIZE_DIV_TWO - (swap_banks >> 1);
                }

                ph = self->phase ^ swap_banks;
                ph = (ph + 1) & wave_mask; /* pre-advance*/

                per = (2048 - freq) * (CLK_MUL_MUL_2);
                if ( !playing )
                {
                        /* Maintain phase when not playing*/
                        int count = (end_time - time + per - 1) / per;
                        ph += count; /* will be masked below*/
                        time += (int32_t) count * per;
                }
                else
                {
                        /* Output amplitude transitions*/
                        int lamp = self->last_amp + DAC_BIAS;
                        do
                        {
                                int amp;
                                int delta;
                                /* Extract nybble*/
                                int nybble = wave [ph >> 1] << (ph << 2 & 4) & 0xF0;
                                ph = (ph + 1) & wave_mask;

                                /* Scale by volume*/
                                amp = (nybble * volume_mul) >> VOLUME_SHIFT_PLUS_FOUR;

                                delta = amp - lamp;
                                if ( delta )
                                {
                                        lamp = amp;
                                        Blip_Synth_offset( self->med_synth, time, delta, out );
                                }
                                time += per;
                        }
                        while ( time < end_time );
                        self->last_amp = lamp - DAC_BIAS;
                }
                ph = (ph - 1) & wave_mask; /* undo pre-advance and mask position*/

                /* Keep track of last byte read*/
                if ( self->enabled )
                        self->sample_buf = wave [ph >> 1];

                self->phase = ph ^ swap_banks; /* undo swapped banks*/
        }
        self->delay = time - end_time;
}

/*============================================================
	BLIP BUFFER
============================================================ */

/* Blip_Buffer 0.4.1. http://www.slack.net/~ant */

#define FIXED_SHIFT 12
#define SAL_FIXED_SHIFT 4096
#define TO_FIXED( f )   int ((f) * SAL_FIXED_SHIFT)
#define FROM_FIXED( f ) ((f) >> FIXED_SHIFT)

static uint32_t Blip_Buffer_clock_rate_factor(const Blip_Buffer *self, long rate);

static void Blip_Buffer_clear(Blip_Buffer *self)
{
   self->offset_       = 0;
   self->reader_accum_ = 0;
   if (self->buffer_)
      memset( self->buffer_, 0, (self->buffer_size_ + BLIP_BUFFER_EXTRA_) * sizeof (int32_t) );
}

static void Blip_Buffer_destroy(Blip_Buffer *self)
{
   if (self->buffer_)
   {
      free(self->buffer_);
      self->buffer_ = NULL;
   }
   self->buffer_size_ = 0;
}

static const char * Blip_Buffer_set_sample_rate(Blip_Buffer *self, long new_rate, int msec)
{
   /* start with maximum length that resampled time can represent*/
   long new_size = (ULONG_MAX >> BLIP_BUFFER_ACCURACY) - BLIP_BUFFER_EXTRA_ - 64;
   if ( msec != 0)
   {
      long s = (new_rate * (msec + 1) + 999) / 1000;
      if ( s < new_size )
         new_size = s;
   }

   if ( self->buffer_size_ != new_size )
   {
      void* p = realloc( self->buffer_, (new_size + BLIP_BUFFER_EXTRA_) * sizeof *self->buffer_ );
      if ( !p )
         return "Out of memory";
      self->buffer_ = (int32_t *) p;
   }

   self->buffer_size_ = new_size;

   /* update things based on the sample rate*/
   self->sample_rate_ = new_rate;
   self->length_ = new_size * 1000 / new_rate - 1;

   /* update these since they depend on sample rate*/
   if ( self->clock_rate_ )
      self->factor_ = Blip_Buffer_clock_rate_factor( self, self->clock_rate_ );

   Blip_Buffer_clear( self );

   return 0;
}

/* Sets number of source time units per second */

static uint32_t Blip_Buffer_clock_rate_factor(const Blip_Buffer *self, long rate)
{
   float ratio = (float) self->sample_rate_ / rate;
   int32_t factor = (int32_t) floor( ratio * (1L << BLIP_BUFFER_ACCURACY) + 0.5 );
   return (uint32_t) factor;
}


/*============================================================
	STEREO BUFFER
============================================================ */

/* Uses three buffers (one for center) and outputs stereo sample pairs. */

#define STEREO_BUFFER_SAMPLES_AVAILABLE() ((long)(bufs_buffer[0].offset_ -  mixer_samples_read) << 1)
#define stereo_buffer_samples_avail() ((((bufs_buffer [0].offset_ >> BLIP_BUFFER_ACCURACY) - mixer_samples_read) << 1))


static const char * stereo_buffer_set_sample_rate( long rate, int msec )
{
        mixer_samples_read = 0;
        {
        	int i;
        	for ( i = BUFS_SIZE; --i >= 0; )
        	{
        		const char * err = Blip_Buffer_set_sample_rate( &bufs_buffer [i], rate, msec );
        		if ( err )
        			return err;
        	}
        }
        return 0; 
}

static void stereo_buffer_clock_rate( long rate )
{
	bufs_buffer[2].factor_ = Blip_Buffer_clock_rate_factor( &bufs_buffer [2], rate );
	bufs_buffer[1].factor_ = Blip_Buffer_clock_rate_factor( &bufs_buffer [1], rate );
	bufs_buffer[0].factor_ = Blip_Buffer_clock_rate_factor( &bufs_buffer [0], rate );
}

static void stereo_buffer_clear (void)
{
        mixer_samples_read = 0;
	Blip_Buffer_clear( &bufs_buffer [2] );
	Blip_Buffer_clear( &bufs_buffer [1] );
	Blip_Buffer_clear( &bufs_buffer [0] );
}

/* mixers use a single index value to improve performance on register-challenged processors
 * offset goes from negative to zero*/

static INLINE void stereo_buffer_mixer_read_pairs( int16_t* out, int count )
{
	int16_t* outtemp;
	Blip_Buffer* buf;
	/* center buffer is bufs_buffer[2] for both passes; its read pointer
	 * (buffer_ + mixer_samples_read) is therefore invariant across both
	 * blocks -- mixer_samples_read is bumped once below and not touched
	 * again here -- so it is computed a single time and shared.  The
	 * center accumulator, however, must be re-seeded per block: block 1
	 * mutates its working copy and only side's accum is written back, so
	 * block 2 needs the pristine bufs_buffer[2].reader_accum_ again.
	 *
	 * Formerly the BLIP_READER_* macro family + BLIP_CLAMP; folded inline
	 * here as this is their only consumer.  The per-sample accumulator
	 * step is the classic blip one-pole DC blocker:
	 *     accum -= accum >> 9;   (9 == old BLIP_READER_DEFAULT_BASS)
	 *     accum += buf[offset];
	 * and the clamp narrows the >>14 mix result to int16_t range. */
	const int32_t* center_buf;
	/* TODO: if caller never marks buffers as modified, uses mono*/
	/* except that buffer isn't cleared, so caller can encounter*/
	/* subtle problems and not realize the cause.*/
	mixer_samples_read += count;
	outtemp = out + count * STEREO;
	center_buf = bufs_buffer[2].buffer_ + mixer_samples_read;

	/* do left + center and right + center separately to reduce register load*/
	buf = &bufs_buffer [2];
	{
		int offset;
		const int32_t* side_buf;
		int32_t side_accum;
		int32_t center_accum;
		--buf;
		--outtemp;

		side_buf     = buf->buffer_ + mixer_samples_read;
		side_accum   = buf->reader_accum_;
		center_accum = bufs_buffer[2].reader_accum_;

		offset = -count;
		do
		{
			int s = (center_accum + side_accum) >> 14;
			side_accum   += side_buf   [offset] - (side_accum   >> 9);
			center_accum += center_buf [offset] - (center_accum >> 9);
			if ( s < -0x8000 || 0x7FFF < s )
				s = (s >> 24) ^ 0x7FFF;

			++offset; /* before write since out is decremented to slightly before end*/
			outtemp [offset * STEREO] = (int16_t) s;
		}while ( offset );

		buf->reader_accum_ = side_accum;
	}
	{
		int offset;
		const int32_t* side_buf;
		int32_t side_accum;
		int32_t center_accum;
		--buf;
		--outtemp;

		side_buf     = buf->buffer_ + mixer_samples_read;
		side_accum   = buf->reader_accum_;
		center_accum = bufs_buffer[2].reader_accum_;

		offset = -count;
		do
		{
			int s = (center_accum + side_accum) >> 14;
			side_accum   += side_buf   [offset] - (side_accum   >> 9);
			center_accum += center_buf [offset] - (center_accum >> 9);
			if ( s < -0x8000 || 0x7FFF < s )
				s = (s >> 24) ^ 0x7FFF;

			++offset; /* before write since out is decremented to slightly before end*/
			outtemp [offset * STEREO] = (int16_t) s;
		}while ( offset );

		buf->reader_accum_ = side_accum;

		/* only end center once*/
		bufs_buffer[2].reader_accum_ = center_accum;
	}
}

static void blip_buffer_remove_all_samples( long count )
{
	long remain;
	uint32_t new_offset = (uint32_t)count << BLIP_BUFFER_ACCURACY;
	/* BLIP BUFFER #1 */
	bufs_buffer[0].offset_ -= new_offset;
	bufs_buffer[1].offset_ -= new_offset;
	bufs_buffer[2].offset_ -= new_offset;

	/* copy remaining samples to beginning and clear old samples*/
	remain = (bufs_buffer[0].offset_ >> BLIP_BUFFER_ACCURACY) + BLIP_BUFFER_EXTRA_;
	memmove( bufs_buffer[0].buffer_, bufs_buffer[0].buffer_ + count, remain * sizeof *bufs_buffer[0].buffer_ );
	memset( bufs_buffer[0].buffer_ + remain, 0, count * sizeof(*bufs_buffer[0].buffer_));

	remain = (bufs_buffer[1].offset_ >> BLIP_BUFFER_ACCURACY) + BLIP_BUFFER_EXTRA_;
	memmove( bufs_buffer[1].buffer_, bufs_buffer[1].buffer_ + count, remain * sizeof *bufs_buffer[1].buffer_ );
	memset( bufs_buffer[1].buffer_ + remain, 0, count * sizeof(*bufs_buffer[1].buffer_));

	remain = (bufs_buffer[2].offset_ >> BLIP_BUFFER_ACCURACY) + BLIP_BUFFER_EXTRA_;
	memmove( bufs_buffer[2].buffer_, bufs_buffer[2].buffer_ + count, remain * sizeof *bufs_buffer[2].buffer_ );
	memset( bufs_buffer[2].buffer_ + remain, 0, count * sizeof(*bufs_buffer[2].buffer_));
}

static long stereo_buffer_read_samples( int16_t * out, long out_size )
{
	int pair_count;

        out_size = (STEREO_BUFFER_SAMPLES_AVAILABLE() < out_size) ? STEREO_BUFFER_SAMPLES_AVAILABLE() : out_size;

        pair_count = (int)(out_size >> 1);
        if ( pair_count )
	{
		stereo_buffer_mixer_read_pairs( out, pair_count );
		blip_buffer_remove_all_samples( mixer_samples_read );
		mixer_samples_read = 0;
	}
        return out_size;
}

static void gba_to_gb_sound_parallel( int * addr, int * addr2 )
{
	uint32_t addr1_table = *addr - 0x60;
	uint32_t addr2_table = *addr2 - 0x60;
	*addr = table [addr1_table];
	*addr2 = table [addr2_table];
}

static void pcm_fifo_write_control( int data, int data2)
{
	pcm[0].enabled = (data & 0x0300) ? true : false;
	pcm[0].timer   = (data & 0x0400) ? 1 : 0;

	if ( data & 0x0800 )
	{
		/* Reset */
		pcm[0].writeIndex = 0;
		pcm[0].readIndex  = 0;
		pcm[0].count      = 0;
		pcm[0].dac        = 0;
		memset(pcm[0].fifo, 0, sizeof(pcm[0].fifo));
	}

	gba_pcm_apply_control( 0, pcm[0].which );

	if(pcm[0].pcm.output)
	{
		int delta;
		int time = SOUND_CLOCK_TICKS -  soundTicks;

		pcm[0].dac = (int8_t)pcm[0].dac >> pcm[0].pcm.shift;
		delta = pcm[0].dac - pcm[0].pcm.last_amp;
		if ( delta )
		{
			pcm[0].pcm.last_amp = pcm[0].dac;
			Blip_Synth_offset(&pcm_synth,  time, delta, pcm[0].pcm.output );
		}
		pcm[0].pcm.last_time = time;
	}

	pcm[1].enabled = (data2 & 0x0300) ? true : false;
	pcm[1].timer   = (data2 & 0x0400) ? 1 : 0;

	if ( data2 & 0x0800 )
	{
		/* Reset */
		pcm[1].writeIndex = 0;
		pcm[1].readIndex  = 0;
		pcm[1].count      = 0;
		pcm[1].dac        = 0;
		memset( pcm[1].fifo, 0, sizeof(pcm[1].fifo));
	}

	gba_pcm_apply_control( 1, pcm[1].which );

	if(pcm[1].pcm.output)
	{
		int delta;
		int time = SOUND_CLOCK_TICKS -  soundTicks;

		pcm[1].dac = (int8_t)pcm[1].dac >> pcm[1].pcm.shift;
		delta = pcm[1].dac - pcm[1].pcm.last_amp;
		if ( delta )
		{
			pcm[1].pcm.last_amp = pcm[1].dac;
			Blip_Synth_offset(&pcm_synth,  time, delta, pcm[1].pcm.output );
		}
		pcm[1].pcm.last_time = time;
	}
}

static void soundEvent_u16_parallel(uint32_t address[])
{
	{
		int i;
		for(i = 0; i < 8; i++)
	{
		switch ( address[i] )
		{
			case SGCNT0_H:
				/*Begin of Write SGCNT0_H */
				WRITE16LE( &ioMem [SGCNT0_H], 0 & 0x770F );
				pcm_fifo_write_control(0, 0);

				gb_apu_volume( apu_vols [ioMem [SGCNT0_H] & 3] );
				/*End of SGCNT0_H */
				break;

			case FIFOA_L:
			case FIFOA_H:
				pcm[0].fifo [pcm[0].writeIndex  ] = 0;
				pcm[0].fifo [pcm[0].writeIndex+1] = 0;
				pcm[0].count += 2;
				pcm[0].writeIndex = (pcm[0].writeIndex + 2) & 31;
				WRITE16LE( &ioMem[address[i]], 0 );
				break;

			case FIFOB_L:
			case FIFOB_H:
				pcm[1].fifo [pcm[1].writeIndex  ] = 0;
				pcm[1].fifo [pcm[1].writeIndex+1] = 0;
				pcm[1].count += 2;
				pcm[1].writeIndex = (pcm[1].writeIndex + 2) & 31;
				WRITE16LE( &ioMem[address[i]], 0 );
				break;

			case 0x88:
				WRITE16LE( &ioMem[address[i]], 0 );
				break;

			default:
				{
					int gb_addr[2]	= {(int)(address[i] & ~1), (int)(address[i] | 1)};
					uint32_t address_array[2] = {address[i] & ~ 1, address[i] | 1};
					uint8_t data_array[2] = {0};
					gba_to_gb_sound_parallel(&gb_addr[0], &gb_addr[1]);
					soundEvent_u8_parallel(gb_addr, address_array, data_array);
					break;
				}
		}
	}
	}
}

static void gba_pcm_fifo_timer_overflowed( unsigned pcm_idx )
{
	if ( pcm[pcm_idx].count <= 16 )
	{
		/* Need to fill FIFO */
		CPUCheckDMA( 3, pcm[pcm_idx].which ? 4 : 2 );

		if ( pcm[pcm_idx].count <= 16 )
		{
			/* Not filled by DMA, so fill with 16 bytes of silence */
			int reg = pcm[pcm_idx].which ? FIFOB_L : FIFOA_L;

			uint32_t address_array[8] = {(uint32_t)(reg), (uint32_t)(reg+2), (uint32_t)(reg), (uint32_t)(reg+2), (uint32_t)(reg), (uint32_t)(reg+2), (uint32_t)(reg), (uint32_t)(reg+2)};
			soundEvent_u16_parallel(address_array);
		}
	}

	/* Read next sample from FIFO */
	pcm[pcm_idx].count--;
	pcm[pcm_idx].dac = pcm[pcm_idx].fifo [pcm[pcm_idx].readIndex];
	pcm[pcm_idx].readIndex = (pcm[pcm_idx].readIndex + 1) & 31;

	if(pcm[pcm_idx].pcm.output)
	{
		int delta;
		int time = SOUND_CLOCK_TICKS -  soundTicks;

		pcm[pcm_idx].dac = (int8_t)pcm[pcm_idx].dac >> pcm[pcm_idx].pcm.shift;
		delta = pcm[pcm_idx].dac - pcm[pcm_idx].pcm.last_amp;
		if ( delta )
		{
			pcm[pcm_idx].pcm.last_amp = pcm[pcm_idx].dac;
			Blip_Synth_offset(&pcm_synth,  time, delta, pcm[pcm_idx].pcm.output );
		}
		pcm[pcm_idx].pcm.last_time = time;
	}
}

void soundEvent_u8_parallel(int gb_addr[], uint32_t address[], uint8_t data[])
{
	{
		uint32_t i;
		for(i = 0; i < 2; i++)
	{
		ioMem[address[i]] = data[i];
		gb_apu_write_register( SOUND_CLOCK_TICKS -  soundTicks, gb_addr[i], data[i] );

		if ( address[i] == NR52 )
		{
			gba_pcm_apply_control(0, 0 );
			gba_pcm_apply_control(1, 1 );
		}
		/* TODO: what about byte writes to SGCNT0_H etc.? */
	}
	}
}

void soundEvent_u8(int gb_addr, uint32_t address, uint8_t data)
{
	ioMem[address] = data;
	gb_apu_write_register( SOUND_CLOCK_TICKS -  soundTicks, gb_addr, data );

	if ( address == NR52 )
	{
		gba_pcm_apply_control(0, 0 );
		gba_pcm_apply_control(1, 1 );
	}
	/* TODO: what about byte writes to SGCNT0_H etc.? */
}


void soundEvent_u16(uint32_t address, uint16_t data)
{
	switch ( address )
	{
		case SGCNT0_H:
			/*Begin of Write SGCNT0_H */
			WRITE16LE( &ioMem [SGCNT0_H], data & 0x770F );
			pcm_fifo_write_control( data, data >> 4);

			gb_apu_volume( apu_vols [ioMem [SGCNT0_H] & 3] );
			/*End of SGCNT0_H */
			break;

		case FIFOA_L:
		case FIFOA_H:
			pcm[0].fifo [pcm[0].writeIndex  ] = data & 0xFF;
			pcm[0].fifo [pcm[0].writeIndex+1] = data >> 8;
			pcm[0].count += 2;
			pcm[0].writeIndex = (pcm[0].writeIndex + 2) & 31;
			WRITE16LE( &ioMem[address], data );
			break;

		case FIFOB_L:
		case FIFOB_H:
			pcm[1].fifo [pcm[1].writeIndex  ] = data & 0xFF;
			pcm[1].fifo [pcm[1].writeIndex+1] = data >> 8;
			pcm[1].count += 2;
			pcm[1].writeIndex = (pcm[1].writeIndex + 2) & 31;
			WRITE16LE( &ioMem[address], data );
			break;

		case 0x88:
			data &= 0xC3FF;
			WRITE16LE( &ioMem[address], data );
			break;

		default:
			{
				int gb_addr[2]	= {(int)(address & ~1), (int)(address | 1)};
				uint32_t address_array[2] = {address & ~ 1, (uint32_t)(address | 1)};
				uint8_t data_array[2] = {(uint8_t)data, (uint8_t)(data >> 8)};
				gba_to_gb_sound_parallel(&gb_addr[0], &gb_addr[1]);
				soundEvent_u8_parallel(gb_addr, address_array, data_array);
				break;
			}
	}
}

void soundTimerOverflow(int timer)
{
	if ( timer == pcm[0].timer && pcm[0].enabled )
		gba_pcm_fifo_timer_overflowed(0);
	if ( timer == pcm[1].timer && pcm[1].enabled )
		gba_pcm_fifo_timer_overflowed(1);
}

void process_sound_tick_fn (void)
{
	long avail;
	int numSamples;
	const long max_samples = (long)(sizeof(soundFinalWave) / sizeof(soundFinalWave[0]));
	/* Run sound hardware to present */
	pcm[0].pcm.last_time -= SOUND_CLOCK_TICKS;
	if ( pcm[0].pcm.last_time < -2048 )
		pcm[0].pcm.last_time = -2048;

	pcm[1].pcm.last_time -= SOUND_CLOCK_TICKS;
	if ( pcm[1].pcm.last_time < -2048 )
		pcm[1].pcm.last_time = -2048;

	/* Emulates sound hardware up to a specified time, ends current time
	frame, then starts a new frame at time 0 */

	if(SOUND_CLOCK_TICKS > gb_apu.last_time)
		gb_apu_run_until_( SOUND_CLOCK_TICKS );

	gb_apu.frame_time -= SOUND_CLOCK_TICKS;
	gb_apu.last_time -= SOUND_CLOCK_TICKS;

	bufs_buffer[2].offset_ += SOUND_CLOCK_TICKS * bufs_buffer[2].factor_;
	bufs_buffer[1].offset_ += SOUND_CLOCK_TICKS * bufs_buffer[1].factor_;
	bufs_buffer[0].offset_ += SOUND_CLOCK_TICKS * bufs_buffer[0].factor_;


	/* dump all the samples available */
	/* VBA will only ever store 1 frame worth of samples */
	avail = stereo_buffer_samples_avail();
	if (avail > max_samples)
		avail = max_samples;
	numSamples = stereo_buffer_read_samples( (int16_t*) soundFinalWave, avail);
	systemOnWriteDataToSoundBuffer(soundFinalWave, numSamples);
}

static void apply_muting (void)
{
	/* PCM */
	gba_pcm_apply_control(1, 0 );
	gba_pcm_apply_control(1, 1 );

	/* APU */
	gb_apu_set_output( &bufs_buffer[2], &bufs_buffer[0], &bufs_buffer[1], 0 );
	gb_apu_set_output( &bufs_buffer[2], &bufs_buffer[0], &bufs_buffer[1], 1 );
	gb_apu_set_output( &bufs_buffer[2], &bufs_buffer[0], &bufs_buffer[1], 2 );
	gb_apu_set_output( &bufs_buffer[2], &bufs_buffer[0], &bufs_buffer[1], 3 );
}


static void remake_stereo_buffer (void)
{
	if ( !ioMem )
		return;

	/* Clears pointers kept to old stereo_buffer */
	gba_pcm_init();

	/* Stereo_Buffer */

        mixer_samples_read = 0;
	stereo_buffer_set_sample_rate( soundSampleRate, BLIP_DEFAULT_LENGTH );
	stereo_buffer_clock_rate( CLOCK_RATE );

	/* PCM */
	pcm [0].which = 0;
	pcm [1].which = 1;

	/* APU */
	gb_apu_new();
	gb_apu_reset( MODE_AGB, true );

	stereo_buffer_clear();

	soundTicks = SOUND_CLOCK_TICKS;

	apply_muting();

	gb_apu_volume(apu_vols [ioMem [SGCNT0_H] & 3] );

	Blip_Synth_volume(&pcm_synth,  0.66 / 256 * -1);
}

void soundCleanUp (void)
{
	/* Tear down the heap-allocated blip-buffer storage owned by the
	 * 3 stereo_buffer entries.  Each holds (new_size + 18) * 4 bytes
	 * (~32 KB at 32 kHz / 250 ms), so ~96 KB total returns to the
	 * allocator.  Buffers are zeroed so a subsequent remake_stereo_buffer
	 * (via soundReset) sees a clean slate. */
	Blip_Buffer_destroy(&bufs_buffer[0]);
	Blip_Buffer_destroy(&bufs_buffer[1]);
	Blip_Buffer_destroy(&bufs_buffer[2]);
}

void soundReset (void)
{
	int gb_addr;
	remake_stereo_buffer();
	/*Begin of Reset APU */
	gb_apu_reset( MODE_AGB, true );

	stereo_buffer_clear();

	soundTicks = SOUND_CLOCK_TICKS;
	/*End of Reset APU */

	SOUND_CLOCK_TICKS = SOUND_CLOCK_TICKS_;
	soundTicks        = SOUND_CLOCK_TICKS_;

	/* Sound Event (NR52) */
	gb_addr = table[NR52 - 0x60];
	if ( gb_addr )
	{
		ioMem[NR52] = 0x80;
		gb_apu_write_register( SOUND_CLOCK_TICKS -  soundTicks, gb_addr, 0x80 );

		gba_pcm_apply_control(0, 0 );
		gba_pcm_apply_control(1, 1 );
	}

	/* TODO: what about byte writes to SGCNT0_H etc.? */
	/* End of Sound Event (NR52) */
}

void soundSetSampleRate(long sampleRate)
{
	if ( soundSampleRate != sampleRate )
	{
		soundSampleRate      = sampleRate;
		remake_stereo_buffer();
	}
}

static int dummy_state [16];

/* gba_state describes the save-state layout as an array of
 * variable_desc { void *address; int size; }.  The LOAD/SKIP macros
 * that used to build these entries have been folded into plain
 * initializers: a real field is { &field, sizeof(type) }; a reserved
 * (skipped) slot is { dummy_state, sizeof(type) }, where dummy_state
 * is scratch storage so a read or write to the slot goes nowhere.
 * The "skipped: ..." comments preserve what each reserved block is
 * held in reserve for. */

static struct {
	gb_apu_state_t apu;

	/* old state */
	int soundDSBValue;
	uint8_t soundDSAValue;
} state;

/* New state format */
static variable_desc gba_state [] =
{
	/* PCM */
	{ &pcm [0].readIndex,  sizeof(int) },
	{ &pcm [0].count,      sizeof(int) },
	{ &pcm [0].writeIndex, sizeof(int) },
	{ &pcm [0].fifo,       sizeof(uint8_t[32]) },
	{ &pcm [0].dac,        sizeof(int) },

	{ dummy_state,         sizeof(int [4]) },   /* skipped: room_for_expansion */

	{ &pcm [1].readIndex,  sizeof(int) },
	{ &pcm [1].count,      sizeof(int) },
	{ &pcm [1].writeIndex, sizeof(int) },
	{ &pcm [1].fifo,       sizeof(uint8_t[32]) },
	{ &pcm [1].dac,        sizeof(int) },

	{ dummy_state,         sizeof(int [4]) },   /* skipped: room_for_expansion */

	/* APU */
	{ &state.apu.regs,          sizeof(uint8_t [0x40]) },   /* last values written to registers and wave RAM (both banks) */
	{ &state.apu.frame_time,    sizeof(int) },              /* clocks until next frame sequencer action */
	{ &state.apu.frame_phase,   sizeof(int) },              /* next step frame sequencer will run */

	{ &state.apu.sweep_freq,    sizeof(int) },              /* sweep's internal frequency register */
	{ &state.apu.sweep_delay,   sizeof(int) },              /* clocks until next sweep action */
	{ &state.apu.sweep_enabled, sizeof(int) },
	{ &state.apu.sweep_neg,     sizeof(int) },              /* obscure internal flag */
	{ &state.apu.noise_divider, sizeof(int) },
	{ &state.apu.wave_buf,      sizeof(int) },              /* last read byte of wave RAM */

	{ &state.apu.delay,         sizeof(int [4]) },          /* clocks until next channel action */
	{ &state.apu.length_ctr,    sizeof(int [4]) },
	{ &state.apu.phase,         sizeof(int [4]) },          /* square/wave phase, noise LFSR */
	{ &state.apu.enabled,       sizeof(int [4]) },          /* internal enabled flag */

	{ &state.apu.env_delay,     sizeof(int [3]) },          /* clocks until next envelope action */
	{ &state.apu.env_volume,    sizeof(int [3]) },
	{ &state.apu.env_enabled,   sizeof(int [3]) },

	{ dummy_state,              sizeof(int [13]) },         /* skipped: room_for_expansion */

	/* Emulator */
	{ &soundEnableFlag,         sizeof(int) },
	{ &soundTicks,              sizeof(int) },

	{ dummy_state,              sizeof(int [14]) },         /* skipped: room_for_expansion */

	{ NULL, 0 }
};

void soundSaveGameMem(uint8_t **data)
{
	/* gb_apu_save_state folded in: run both halves of the save/load
	 * reflection in save mode. */
	(void) gb_apu_save_load( &state.apu, true );
	gb_apu_save_load2( &state.apu, true );
	memset(dummy_state, 0, sizeof dummy_state);
	utilWriteDataMem(data, gba_state);
}

void soundReadGameMem(const uint8_t **in_data, int version)
{
	int data;
	(void)version;
	/* Prepare APU and default state */

	/*Begin of Reset APU */
	gb_apu_reset( MODE_AGB, true );

	stereo_buffer_clear();

	soundTicks = SOUND_CLOCK_TICKS;
	/*End of Reset APU */

	/* gb_apu_save_state folded in (see soundSaveGameMem): seeds
	 * state.apu with the current APU state before it is overwritten
	 * by the data read from the save below. */
	(void) gb_apu_save_load( &state.apu, true );
	gb_apu_save_load2( &state.apu, true );

	utilReadDataMem( in_data, gba_state );

	gb_apu_load_state( &state.apu );
	/*Begin of Write SGCNT0_H */
	data = (READ16LE( &ioMem [SGCNT0_H] ) & 0x770F);
	WRITE16LE( &ioMem [SGCNT0_H], data & 0x770F );
	pcm_fifo_write_control( data, data >> 4 );

	gb_apu_volume(apu_vols [ioMem [SGCNT0_H] & 3] );
	/*End of SGCNT0_H */
}

