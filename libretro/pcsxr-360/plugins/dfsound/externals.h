/***************************************************************************
                         externals.h  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de

 Cycle-driven port from pcsx_rearmed (notaz, 2010-2015).
 ***************************************************************************/

#ifndef __P_SOUND_EXTERNALS_H__
#define __P_SOUND_EXTERNALS_H__

#include <stdint.h>

/* All GCC-isms (noinline, forceinline, unlikely, preload, ALIGN_VAR,
 * __restrict__) are normalised in stdafx.h.  Include it first so the
 * macros are available throughout the dfsound TU. */
#include "stdafx.h"

/////////////////////////////////////////////////////////
// generic defines
/////////////////////////////////////////////////////////

#define PSE_LT_SPU                  4
#define PSE_SPU_ERR_SUCCESS         0
#define PSE_SPU_ERR                 -60
#define PSE_SPU_ERR_NOTCONFIGURED   PSE_SPU_ERR - 1
#define PSE_SPU_ERR_INIT            PSE_SPU_ERR - 2
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

////////////////////////////////////////////////////////////////////////
// spu defines
////////////////////////////////////////////////////////////////////////

// num of channels
#define MAXCHAN     24

// note: must be even due to the way reverb works now.
// pcsx_rearmed sized this to one frame (44100/50 + headroom),
// rounded down to even.  At 50 Hz PAL that is 914 samples; at NTSC
// the SPU is called more often so it never overflows.
#define NSSIZE ((44100 / 50 + 32) & ~1)

///////////////////////////////////////////////////////////
// struct defines
///////////////////////////////////////////////////////////

enum ADSR_State {
 ADSR_ATTACK = 0,
 ADSR_DECAY = 1,
 ADSR_SUSTAIN = 2,
 ADSR_RELEASE = 3,
};

// ADSR INFOS PER CHANNEL
typedef struct
{
 unsigned char  State;                /* ADSR_State (was bitfield :2) */
 unsigned char  AttackModeExp;        /* (was :1) */
 unsigned char  SustainModeExp;       /* (was :1) */
 unsigned char  SustainIncrease;      /* (was :1) */
 unsigned char  ReleaseModeExp;       /* (was :1) */
 unsigned char  AttackRate;
 unsigned char  DecayRate;
 unsigned char  SustainLevel;
 unsigned char  SustainRate;
 unsigned char  ReleaseRate;
 int            EnvelopeVol;
 int            StepCounter;          /* 0 -> 32k */
} ADSRInfoEx;

/* NOTE: pcsx_rearmed packed ADSR_State + 4 bool flags into one byte
 * via bitfields (`State:2`, `AttackModeExp:1`, ...).  MSVC accepts
 * the syntax but ABI is compiler-dependent and the freeze format
 * reads/writes them as full ints.  Expanding to plain unsigned char
 * removes any layout risk and costs only a couple of bytes per
 * channel (24 channels × 4 = 96 B). */

struct xa_decode;

///////////////////////////////////////////////////////////

// MAIN CHANNEL STRUCT
typedef struct
{
 int               iSBPos;                             // mixing stuff
 int               spos;
 int               sinc;
 int               sinc_inv;

 unsigned char *   pCurr;                              // current pos in sound mem
 unsigned char *   pLoop;                              // loop ptr in sound mem

 /* Original used unsigned-int bitfields:
  *   bReverb:1, bRVBActive:1, bNoise:1, bFMod:2, prevflags:3,
  *   bIgnoreLoop:1, bStarting:1.
  * Same packing concerns as ADSRInfoEx: expanded to plain unsigned
  * char so MSVC ABI matches the source-code intent exactly. */
 unsigned char     bReverb;            // (was :1) can we do reverb on this channel? must have ctrl reg bit, to get active
 unsigned char     bRVBActive;         // (was :1) reverb active flag
 unsigned char     bNoise;             // (was :1) noise active flag
 unsigned char     bFMod;              // (was :2) freq mod (0=off, 1=sound channel, 2=freq channel)
 unsigned char     prevflags;          // (was :3) flags from previous block
 unsigned char     bIgnoreLoop;        // (was :1) ignore loop bit, if an external loop address is used
 unsigned char     bStarting;          // (was :1) starting after keyon
 unsigned char     _pad0[1];

 /* The original source had an anonymous union with an anonymous
  * inner struct (`union { struct { int iLeftVolume, iRightVolume;
  * }; int iVolume[2]; };`).  MSVC C accepts that under /Zo (default),
  * but it's brittle across compiler versions; rewrite as a plain
  * pair plus a #define so callers reading `iVolume[0/1]` still
  * compile.  In practice only the union-member names are used. */
 int               iLeftVolume;                        // left volume
 int               iRightVolume;                       // right volume

 ADSRInfoEx        ADSRX;
 int               iRawPitch;                          // raw pitch (0...3fff)
} SPUCHAN;

/* Compatibility shim: pcsx_rearmed accesses iVolume[i] as an array.
 * Expand to either iLeftVolume or iRightVolume via this macro. */
#define s_chan_iVolume(s_chan, i) (*(((i) & 1) ? &(s_chan)->iRightVolume : &(s_chan)->iLeftVolume))

///////////////////////////////////////////////////////////

typedef struct
{
 int StartAddr;      // reverb area start addr in samples
 int CurrAddr;       // reverb area curr addr in samples

 int VolLeft;
 int VolRight;

 // directly from nocash docs (SPU2-X register names)
 int vIIR;    // 1DC4 volume  Reverb Reflection Volume 1
 int vCOMB1;  // 1DC6 volume  Reverb Comb Volume 1
 int vCOMB2;  // 1DC8 volume  Reverb Comb Volume 2
 int vCOMB3;  // 1DCA volume  Reverb Comb Volume 3
 int vCOMB4;  // 1DCC volume  Reverb Comb Volume 4
 int vWALL;   // 1DCE volume  Reverb Reflection Volume 2
 int vAPF1;   // 1DD0 volume  Reverb APF Volume 1
 int vAPF2;   // 1DD2 volume  Reverb APF Volume 2
 int mLSAME;  // 1DD4 src/dst Reverb Same Side Reflection Address 1 Left
 int mRSAME;  // 1DD6 src/dst Reverb Same Side Reflection Address 1 Right
 int mLCOMB1; // 1DD8 src     Reverb Comb Address 1 Left
 int mRCOMB1; // 1DDA src     Reverb Comb Address 1 Right
 int mLCOMB2; // 1DDC src     Reverb Comb Address 2 Left
 int mRCOMB2; // 1DDE src     Reverb Comb Address 2 Right
 int dLSAME;  // 1DE0 src     Reverb Same Side Reflection Address 2 Left
 int dRSAME;  // 1DE2 src     Reverb Same Side Reflection Address 2 Right
 int mLDIFF;  // 1DE4 src/dst Reverb Different Side Reflect Address 1 Left
 int mRDIFF;  // 1DE6 src/dst Reverb Different Side Reflect Address 1 Right
 int mLCOMB3; // 1DE8 src     Reverb Comb Address 3 Left
 int mRCOMB3; // 1DEA src     Reverb Comb Address 3 Right
 int mLCOMB4; // 1DEC src     Reverb Comb Address 4 Left
 int mRCOMB4; // 1DEE src     Reverb Comb Address 4 Right
 int dLDIFF;  // 1DF0 src     Reverb Different Side Reflect Address 2 Left
 int dRDIFF;  // 1DF2 src     Reverb Different Side Reflect Address 2 Right
 int mLAPF1;  // 1DF4 src/dst Reverb APF Address 1 Left
 int mRAPF1;  // 1DF6 src/dst Reverb APF Address 1 Right
 int mLAPF2;  // 1DF8 src/dst Reverb APF Address 2 Left
 int mRAPF2;  // 1DFA src/dst Reverb APF Address 2 Right
 int vLIN;    // 1DFC volume  Reverb Input Volume Left
 int vRIN;    // 1DFE volume  Reverb Input Volume Right

 // subtracted offsets
 int mLAPF1_dAPF1, mRAPF1_dAPF1, mLAPF2_dAPF2, mRAPF2_dAPF2;

 int dirty;   // registers changed
} REVERBInfo;

///////////////////////////////////////////////////////////

// psx buffers / addresses

typedef union {
 short buf[4+28];
 struct {
  short old[4];        // old samples for interpolation
  short decode[28];
 };
} sample_buf;

typedef struct {
 int sample[2][4*2];
} sample_buf_rvb;

typedef struct
{
 unsigned short  spuCtrl;
 unsigned short  spuStat;

 unsigned int    spuAddr;

 unsigned int    cycles_played;
 unsigned int    cycles_dma_end;
 int             decode_pos;
 int             decode_dirty_ch;
 unsigned int    bSpuInit;             /* (was :1) */
 unsigned int    bSPUIsOpen;           /* (was :1) */
 unsigned int    bMemDirty;            /* (was :1) had external write to SPU RAM */

 unsigned int    dwNoiseVal;           // global noise generator
 unsigned int    dwNoiseCount;
 unsigned int    dwNewChannel;         // flags for faster testing, if new channel starts
 unsigned int    dwChannelsAudible;    // not silent channels
 unsigned int    dwChannelDead;        // silent+not useful channels

 unsigned int    XARepeat;
 unsigned int    XALastVal;

 int             iLeftXAVol;
 int             iRightXAVol;

 int             cdClearSamples;       // extra samples to clear the capture buffers
 struct {                              // channel volume in the cd controller
  unsigned char  ll, lr, rl, rr;       // see cdr.Attenuator* in cdrom.c
 } cdv;                                // applied on spu side for easier emulation

 unsigned int    last_keyon_cycles;

 /* The original used an anonymous union here:
  *   union { unsigned char *spuMemC; unsigned short *spuMem; };
  * Same MSVC concern as in SPUCHAN; spell it out explicitly. */
 unsigned char * spuMemC;
 unsigned short *spuMem;

 unsigned char * pSpuIrq;

 unsigned char * pSpuBuffer;
 short         * pS;

 SPUCHAN       * s_chan;
 REVERBInfo    * rvb;

 int           * SSumLR;

 void (CALLBACK *irqCallback)(int);
 //void (CALLBACK *cddavCallback)(short, short);
 void (CALLBACK *scheduleCallback)(unsigned int);

 const struct xa_decode * xapGlobal;
 unsigned int  * XAFeed;
 unsigned int  * XAPlay;
 unsigned int  * XAStart;
 unsigned int  * XAEnd;

 unsigned int  * CDDAFeed;
 unsigned int  * CDDAPlay;
 unsigned int  * CDDAStart;
 unsigned int  * CDDAEnd;

 ALIGN_VAR(32)
 unsigned short  regArea[0x400];

 sample_buf      sb[MAXCHAN];
 sample_buf_rvb  sb_rvb; // for reverb filtering
 int             interpolation;

 /* Worker thread (USE_ASYNC_SPU / WANT_THREAD_CODE) is intentionally
  * disabled in this port — see SPU plugin spu.c for rationale. */
} SPUInfo;

#define regAreaRef(offset) \
  spu.regArea[((offset) - 0xc00) >> 1]
#define regAreaGet(offset) \
  regAreaRef(offset)
#define regAreaGetCh(ch, offset) \
  spu.regArea[(((ch) << 4) | (offset)) >> 1]

///////////////////////////////////////////////////////////
// SPU.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_SPU

extern SPUInfo spu;

void do_samples(unsigned int cycles_to, int force_no_thread);
void schedule_next_irq(void);
void check_irq_io(unsigned int addr);
void do_irq_io(int cycles_after);

#define do_samples_if_needed(c, no_thread, samples) \
 do { \
  if ((no_thread) || (int)((c) - spu.cycles_played) >= (samples) * 768) \
   do_samples(c, no_thread); \
 } while (0)

#endif

void FeedXA(const struct xa_decode *xap);
void FeedCDDA(unsigned char *pcm, int nBytes);

#endif /* __P_SOUND_EXTERNALS_H__ */
