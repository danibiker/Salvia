#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include "globals.h"
#include "roms.h"

#include "hwaudio/ym2151.h"

#include "engine/audio/commands.h"
#include "engine/audio/osoundadr.h"

/* PCM Sample Indexes */
enum
{
    PCM_SAMPLE_CRASH1  = 0xD0, /* 0xD0 - Crash 1 */
    PCM_SAMPLE_GURGLE  = 0xD1, /* 0xD1 - Gurgle */
    PCM_SAMPLE_SLIP    = 0xD2, /* 0xD2 - Slip */
    PCM_SAMPLE_CRASH2  = 0xD3, /* 0xD3 - Crash 2 */
    PCM_SAMPLE_CRASH3  = 0xD4, /* 0xD4 - Crash 3 */
    PCM_SAMPLE_SKID    = 0xD5, /* 0xD5 - Skid */
    PCM_SAMPLE_REBOUND = 0xD6, /* 0xD6 - Rebound */
    PCM_SAMPLE_HORN    = 0xD7, /* 0xD7 - Horn */
    PCM_SAMPLE_TYRES   = 0xD8, /* 0xD8 - Tyre Squeal */
    PCM_SAMPLE_SAFETY  = 0xD9, /* 0xD9 - Safety Zone */
    PCM_SAMPLE_LOSKID  = 0xDA, /* 0xDA - Lofi skid (is this used) */
    PCM_SAMPLE_CHEERS  = 0xDB, /* 0xDB - Cheers */
    PCM_SAMPLE_VOICE1  = 0xDC, /* 0xDC - Voice 1, Checkpoint */
    PCM_SAMPLE_VOICE2  = 0xDD, /* 0xDD - Voice 2, Congratulations */
    PCM_SAMPLE_VOICE3  = 0xDE, /* 0xDE - Voice 3, Get Ready */
    PCM_SAMPLE_VOICE4  = 0xDF, /* 0xDF - Voice 4, You're doing great (unused, plays at wrong pitch) */
    PCM_SAMPLE_WAVE    = 0xE0, /* 0xE0 - Wave */
    PCM_SAMPLE_CRASH4  = 0xE1  /* 0xE1 - Crash 4 */
};

/* Internal Channel Offsets in RAM */
/* Channels 0-7: YM Channels */
#define CHANNEL_YM1 (0x020) /* f820 */
#define CHANNEL_YM2 (0x040)
#define CHANNEL_YM3 (0x060)
#define CHANNEL_YM4 (0x080)
#define CHANNEL_YM5 (0x0A0)
#define CHANNEL_YM6 (0x0C0)
#define CHANNEL_YM7 (0x0E0)
#define CHANNEL_YM8 (0x100) /* f900 */

/* Channels 8-13: PCM Drum Channels for music */
#define CHANNEL_PCM_DRUM1 (0x120)
#define CHANNEL_PCM_DRUM2 (0x140)
#define CHANNEL_PCM_DRUM3 (0x160)
#define CHANNEL_PCM_DRUM4 (0x180)
#define CHANNEL_PCM_DRUM5 (0x1A0)
#define CHANNEL_PCM_DRUM6 (0x1C0)

/* Channels 14-21: PCM Sound Effects */
#define CHANNEL_PCM_FX1 (0x1E0) /* f9e0: Crowd, Cheers, Wave */
#define CHANNEL_PCM_FX2 (0x200)
#define CHANNEL_PCM_FX3 (0x220) /* fa20: Slip, Safety Zone */
#define CHANNEL_PCM_FX4 (0x240)
#define CHANNEL_PCM_FX5 (0x260) /* fa60: Crash 1, Rebound, Crash2 */
#define CHANNEL_PCM_FX6 (0x280)
#define CHANNEL_PCM_FX7 (0x2A0) /* faa0: Voices */
#define CHANNEL_PCM_FX8 (0x2C0)

/* Channel Mapping Info. Used to play sound effects and music at the same time.  */
#define CHANNEL_MAP1 (0x2E0)
#define CHANNEL_MAP2 (0x300)
#define CHANNEL_MAP3 (0x320)
#define CHANNEL_MAP4 (0x340)
#define CHANNEL_MAP5 (0x360)
#define CHANNEL_MAP6 (0x380)
#define CHANNEL_MAP7 (0x3A0)

/* Channels 22-23: YM Sound Effects */
#define CHANNEL_YM_FX1 (0x3C0) /* fbc0: Signal 1, Signal 2 */
#define CHANNEL_YM_FX2 (0x3E0)

/* Engine Commands in RAM */
#define CHANNEL_ENGINE_CH1 (0x400) /* 0xFC00: Engine Channel - Player's Car */
#define CHANNEL_ENGINE_CH2 (0x420) /* 0xFC20: Engine Channel - Traffic 1 */
#define CHANNEL_ENGINE_CH3 (0x440) /* 0xFC40: Engine Channel - Traffic 2 */
#define CHANNEL_ENGINE_CH4 (0x460) /* 0xFC60: Engine Channel - Traffic 3 */
#define CHANNEL_ENGINE_CH5 (0x480) /* 0xFC80: Engine Channel - Traffic 4 */

/* ------------------------------------------------------------------------------------------------ */
/* Internal Format of Sound Data in RAM before sending to hardware */
/* ------------------------------------------------------------------------------------------------ */
/* */
/*0x20 byte chunks of information per channel in memory. Format is as follows: */
/* */
/*+0x00: [Byte] Flags e65--cn- */
/*              n = FM noise channel (1 = yes, 0 = no) */
/*              c = Corresponding music channel is enabled */
/*              5 = Pitch Bend (Only used by Step On Beat track, not the standard music) */
/*              6 = ??? */
/*              e = channel enable (1 = active, 0 = disabled).  */
/*+0x01: [Byte] Flags -m---ccc */
/*	            c = YM Channel Number */
/*              m = possibly a channel mute?  */
/*                  Counters and positions still tick.  (1 = active, 0 = disabled). */
/*+0x02: [Byte] Used as end marker when bit 1 of 0x0D is set */
/*+0x03: [Word] Position in sequence */
/*+0x05: [Word] Sequence End Marker */
/*+0x07: [Word] Address of next command (see 0x2E7) */
/*              This is essentially within the same block of sound information */
/*+0x09: [Byte] Note Offset: From lowest note. Essentially an index into a table. */
/*+0x0A: [Byte] Offset into 0x20 block of memory. Used to store positioning info. */
/*+0x0B: [Byte] Use Phase and Amplitude Modulation Sensitivity Table */
/*+0x0C: [Byte] FM: Select FM Data Block To Send. 0 = No Block. */
/*+0x0D: [Byte] FM: End Marker Flags */
/*              Flags ------10 */
/*              0 = Set high byte of end marker from data. */
/*              1 = Do not calculate end marker. Use value from data. */
/*+0x0E: [Byte] Sample Index / Command */
/*+0x0F: [Word] ? */
/*+0x10: [Byte] Offset into Phase and Amplitude Modulation Sensitivity Table (see 0x1DF) */
/*+0x11: [Byte] Volume: Left Channel */
/*+0x12: [Byte] Volume: Right Channel */
/*+0x13: [Word] PCM: Wave Start Address / Loop Address  */
/*              FM:  Note & Octave Info (top bit denotes noise channel?) */
/*+0x14: [Byte] FM Channels only. Phase and Amplitude Modulation Sensitivity */
/*+0x15: [Byte] Wave End Address HIGH 8 bits */
/*+0x16: [Byte] PCM Pitch */
/*+0x17: [Byte] Flags m-bbccla */
/*              a = active (0 = active,  1 = inactive)  */
/*              l = loop   (0 = enabled, 1 = disabled) */
/*              c = channel pair select */
/*              b = bank */
/*              m = Music Sample (Drums etc.) */
/*+0x18: [Byte] FM Loop Counter. Specifies number of times to trigger command sequence.  */
/*              Counter used at 0x45f */
/* */
/*+0x1C: [Word] Sequence Address #1 */
/*+0x1E: [Word] Sequence Address #2 */

enum
{
    CH_FLAGS       = 0x00,
    CH_FM_FLAGS    = 0x01,
    CH_END_MARKER  = 0x02,
    CH_SEQ_POS     = 0x03,
    CH_SEQ_END     = 0x05,
    CH_SEQ_CMD     = 0x07,
    CH_NOTE_OFFSET = 0x09,
    CH_MEM_OFFSET  = 0x0A,
    CH_FM_PHASETBL = 0x0B,
    CH_FM_BLOCK    = 0x0C,
    CH_FM_MARKER   = 0x0D,
    CH_COMMAND     = 0x0E,
    CH_UNKNOWN     = 0x0F,
    CH_FM_PHASEOFF = 0x10,
    CH_VOL_L       = 0x11,
    CH_VOL_R       = 0x12,
    CH_PCM_ADR1L   = 0x13,
    CH_FM_NOTE     = 0x13,
    CH_PCM_ADR1H   = 0x14,
    CH_FM_PHASE_AMP= 0x14,
    CH_PCM_ADR2    = 0x15,
    CH_PCM_PITCH   = 0x16,
    CH_CTRL        = 0x17,
    CH_FM_LOOP     = 0x18,
    CH_SEQ_ADR1    = 0x1C,
    CH_SEQ_ADR2    = 0x1E
};

/* +0x00: [Byte] Engine Volume */
/* +0x01: [Byte] Engine Volume (seems same as 0x00) */
/* +0x02: [Byte] Flags -6543210 */
/*               6 =  */
/*               5 = Set mutes channel completely */
/*               4 =  */
/*               3 = Set denotes loop address has been set */
/*               2 = Set denotes loop disabled */
/*               1 = Denote engine volume set */
/*               0 = Set denotes start address / end address has been set */
/* +0x03: [Byte] Engine Sample Loop counter (0 - 8)  */
/*               Used as offset into separate 0x20 block to store data at (e.g. Engine Pitch1, Pitch2, Vol) */
/* +0x04: [Byte] Engine Pitch Low */
/* +0x05: [Byte] Engine Pitch High */
/* +0x06: [Byte] Volume adjusted by 0x76D7 routine */
/* +0x07: [Byte] */
/* +0x08: [Byte] 0 = Channel Muted, 1 = Channel Active */

enum
{
    CH_ENGINES_VOL0    = 0x00,
    CH_ENGINES_VOL1    = 0x01,
    CH_ENGINES_FLAGS   = 0x02,
    CH_ENGINES_OFFSET  = 0x03,
    CH_ENGINES_LOOP    = 0x03,
    CH_ENGINES_PITCH_L = 0x04,
    CH_ENGINES_PITCH_H = 0x05,
    CH_ENGINES_VOL6    = 0x06,
    CH_ENGINES_ACTIVE  = 0x08
};

/* MML Command Languge Defines */
enum
{
    MML_TEMPO            = 0x01, /* change tempo of track(only possible for non - MML_FIXED_TEMPO tracks) */
    MML_SAMPLE_LEVEL	 = 0x02, /* set sample left / right volume */
    MML_SEAMLESS		 = 0x03, /* make use of unused command $83 for 'seamless' note transitions */
    MML_END_FM_TRACK	 = 0x04, /* end playback of this FM track(DO NOT USE ON PCM TRACKS) */
    MML_NOISE_ON		 = 0x05, /* set noise bit(only works on FM track 7) */
    MML_SET_TL		     = 0x06, /* only partially working(can't use bits 0 and 1) */
    MML_KEY_FRACTIONS	 = 0x07, /* little to no audible difference */
    MML_CALL		     = 0x08, /* call a subroutine */
    MML_RET			     = 0x09, /* return from a subroutine */
    MML_LOOP_FOREVER	 = 0x0a, /* self explanatory */
    MML_TRANSPOSE		 = 0x0b, /* transpose subsequent notes up or down */
    MML_LOOP		     = 0x0c, /* loop n number of times(allows nested loops via second parameter) */
    MML_PITCH_BEND_START = 0x0d, /* pitch bend start */
    MML_PITCH_BEND_END	 = 0x0e, /* pitch bend AND 'seamless' end */
    MML_LOAD_PATCH		 = 0x11, /* load new FM patch data into the sound chip registers */
    MML_NOISE_OFF		 = 0x12, /* clear noise bit(set by cmd $85) */
    MML_VOICE_PITCH		 = 0x13, /* set pitch of PCM voice like 'Checkpoint' or 'Get Ready' */
    MML_FIXED_TEMPO		 = 0x14, /* note duration is read directly from track rather than being computed */
    MML_LONG		     = 0x15, /* used for 'long' notes, restsand percussion samples */
    MML_RIGHT_CH_ONLY	 = 0x16, /* send FM output to right channel / speaker only */
    MML_LEFT_CH_ONLY	 = 0x17, /* send FM output to left channel / speaker only */
    MML_BOTH_CH		     = 0x18 /* send FM output to both channels / speakers */
};

enum { CHAN_RAM_SIZE = 0x800 };

static const uint8_t CHAN_SIZE = 0x20;

#define CH09_CMDS1 (0x570)

#define CH09_CMDS2 (0x578)

#define CH11_CMDS1 (0x580)

#define CH11_CMDS2 (0x588)

#define PAN_LEFT (0x40)

#define PAN_RIGHT (0x80)

#define PAN_CENTRE (PAN_LEFT | PAN_RIGHT)

typedef struct OSound
{
    uint8_t command_input;
    uint8_t engine_data[8];
    uint8_t chan_ram[CHAN_RAM_SIZE];
    uint8_t* pcm_ram;
    YM2151*  ym;
    uint8_t sound_props;
    uint8_t command_index;
    uint8_t counter1, counter2, counter3, counter4;
    uint16_t pos;
    uint8_t cmd_prev;
    uint16_t chanid_prev;
    uint8_t engine_counter;
    uint8_t engine_channel;
} OSound;

extern OSound osound;

void OSound_init(OSound* self, YM2151* ym, uint8_t* pcm_ram);

void OSound_init_fm_chip(OSound* self);

void OSound_tick(OSound* self);

#ifdef __cplusplus
}
#endif
