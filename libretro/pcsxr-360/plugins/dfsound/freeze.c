/***************************************************************************
                          freeze.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de

 Adapted from pcsx_rearmed freeze.c.  pcsx_rearmed splits the SPU
 freeze across two buffers (SPUFreeze_t for the lightweight header,
 a separate F2 buffer for the heavy state).  pcsxr-360 still uses
 the legacy single-buffer SPUFreeze_t (SPURam[0x80000] + xa_decode_t
 embedded), so keep that layout here so misc.c doesn't need to
 change.  We append our private SPUOSSFreeze_t after the SPURam
 region inside the same buffer (the original PEOPS layout did the
 same trick using SPUInfo pointer arithmetic).
 ***************************************************************************/

#include <stddef.h>
#include <assert.h>
#include "stdafx.h"

#define _IN_FREEZE

#include "externals.h"
/* spu.h must precede registers.h: it installs the PEOPS_* renames. */
#include "spu.h"
#include "registers.h"
#include "../../libpcsxcore/decode_xa.h"   /* xa_decode_t */
#include "../../libpcsxcore/spu_freeze.h"  /* full SPUFreeze_t (PluginName, SPUPorts, SPURam, xa, SPUInfo) */

////////////////////////////////////////////////////////////////////////
// freeze structs
////////////////////////////////////////////////////////////////////////

typedef struct
{
 int            AttackModeExp;
 int            AttackTime;
 int            DecayTime;
 int            SustainLevel;
 int            SustainModeExp;
 int            SustainModeDec;
 int            SustainTime;
 int            ReleaseModeExp;
 unsigned int   ReleaseVal;
 int            ReleaseTime;
 int            ReleaseStartTime;
 int            ReleaseVol;
 int            lTime;
 int            lVolume;
} ADSRInfo;

typedef struct
{
 int            State;
 int            AttackModeExp;
 int            AttackRate;
 int            DecayRate;
 int            SustainLevel;
 int            SustainModeExp;
 int            SustainIncrease;
 int            SustainRate;
 int            ReleaseModeExp;
 int            ReleaseRate;
 int            EnvelopeVol;
 int            lVolume;
 int            StepCounter;
 int            lDummy2;
} ADSRInfoEx_orig;

typedef struct
{
 int               bNew;                               // start flag

 int               iSBPos;                             // mixing stuff
 int               spos;
 int               sinc;
 int               SB[32+32];
 int               sval;

 int               iStart;                             // start ptr into sound mem
 int               iCurr;                              // current pos in sound mem
 int               iLoop;                              // loop ptr in sound mem

 int               bOn;                                // is channel active
 int               bStop;                              // is channel stopped
 int               bReverb;
 int               iActFreq;
 int               iUsedFreq;
 int               iLeftVolume;
 int               iLeftVolRaw;
 int               bIgnoreLoop;
 int               iMute;
 int               iRightVolume;
 int               iRightVolRaw;
 int               iRawPitch;
 int               iIrqDone;
 int               s_1;
 int               s_2;
 int               bRVBActive;
 int               iRVBOffset;
 int               iRVBRepeat;
 int               bNoise;
 int               bFMod;
 int               iRVBNum;
 int               iOldNoise;
 ADSRInfo          ADSR;
 ADSRInfoEx_orig   ADSRX;
} SPUCHAN_orig;

typedef struct
{
 xa_decode_t xa;

 unsigned short  spuIrq;
 unsigned short  decode_pos;
 uint32_t   pSpuIrq;
 uint32_t   spuAddr;
 uint32_t   rvb_cur;
 uint16_t   xa_left;
 uint16_t   cdda_left;
 uint32_t   cycles_played;

 SPUCHAN_orig s_chan[MAXCHAN];

 uint32_t   cycles_dma_end;
 uint32_t   decode_dirty_ch;
 uint32_t   dwNoiseVal;
 uint32_t   dwNoiseCount;
 uint32_t   XARepeat;
 uint32_t   XALastVal;
 uint32_t   last_keyon_cycles;
 uint32_t   rvb_sb[2][4];
 int32_t    interpolation;
} SPUOSSFreeze_t;

#define SB_FORMAT_MAGIC 0x73626201

////////////////////////////////////////////////////////////////////////

static void save_channel(SPUCHAN_orig *d, const SPUCHAN *s, int ch)
{
 memset(d, 0, sizeof(*d));
 d->bNew = !!(spu.dwNewChannel & (1<<ch));
 d->iSBPos = s->iSBPos;
 d->spos = s->spos;
 d->sinc = s->sinc;
 assert(sizeof(d->SB) >= sizeof(spu.sb[ch]));
 d->SB[sizeof(d->SB) / sizeof(d->SB[0]) - 1] = SB_FORMAT_MAGIC;
 memcpy(d->SB, &spu.sb[ch], sizeof(spu.sb[ch]));
 d->iStart = (regAreaGetCh(ch, 6) & ~1) << 3;
 d->iCurr = 0; // set by caller
 d->iLoop = 0; // set by caller
 d->bOn = !!(spu.dwChannelsAudible & (1<<ch));
 d->bStop = s->ADSRX.State == ADSR_RELEASE;
 d->bReverb = s->bReverb;
 d->iActFreq = 1;
 d->iUsedFreq = 2;
 d->iLeftVolume = s->iLeftVolume;
 d->bIgnoreLoop = (s->prevflags ^ 2) << 1;
 d->iRightVolume = s->iRightVolume;
 d->iRawPitch = s->iRawPitch;
 d->s_1 = spu.sb[ch].decode[27];
 d->s_2 = spu.sb[ch].decode[26];
 d->bRVBActive = s->bRVBActive;
 d->bNoise = s->bNoise;
 d->bFMod = s->bFMod;
 d->ADSRX.State = s->ADSRX.State;
 d->ADSRX.AttackModeExp = s->ADSRX.AttackModeExp;
 d->ADSRX.AttackRate = s->ADSRX.AttackRate;
 d->ADSRX.DecayRate = s->ADSRX.DecayRate;
 d->ADSRX.SustainLevel = s->ADSRX.SustainLevel;
 d->ADSRX.SustainModeExp = s->ADSRX.SustainModeExp;
 d->ADSRX.SustainIncrease = s->ADSRX.SustainIncrease;
 d->ADSRX.SustainRate = s->ADSRX.SustainRate;
 d->ADSRX.ReleaseModeExp = s->ADSRX.ReleaseModeExp;
 d->ADSRX.ReleaseRate = s->ADSRX.ReleaseRate;
 d->ADSRX.EnvelopeVol = (uint32_t)s->ADSRX.EnvelopeVol << 16;
 d->ADSRX.StepCounter = s->ADSRX.StepCounter;
 d->ADSRX.lVolume = d->bOn;
}

static void load_channel(SPUCHAN *d, const SPUCHAN_orig *s, int ch)
{
 memset(d, 0, sizeof(*d));
 if (s->bNew) spu.dwNewChannel |= 1<<ch;
 d->iSBPos = s->iSBPos;
 if ((uint32_t)d->iSBPos >= 28) d->iSBPos = 27;
 d->spos = s->spos;
 d->sinc = s->sinc;
 d->sinc_inv = 0;
 if (s->SB[sizeof(s->SB) / sizeof(s->SB[0]) - 1] == SB_FORMAT_MAGIC)
  memcpy(&spu.sb[ch], s->SB, sizeof(spu.sb[ch]));
 else {
  memset(&spu.sb[ch], 0, sizeof(spu.sb[ch]));
  spu.sb[ch].decode[27] = (short)s->s_1;
  spu.sb[ch].decode[26] = (short)s->s_2;
 }
 d->pCurr = (unsigned char *)((uintptr_t)s->iCurr & 0x7fff0);
 d->pLoop = (unsigned char *)((uintptr_t)s->iLoop & 0x7fff0);
 d->bReverb = (unsigned char)s->bReverb;
 d->iLeftVolume = s->iLeftVolume;
 d->iRightVolume = s->iRightVolume;
 d->iRawPitch = s->iRawPitch;
 d->bRVBActive = (unsigned char)s->bRVBActive;
 d->bNoise = (unsigned char)s->bNoise;
 d->bFMod = (unsigned char)s->bFMod;
 d->prevflags = (unsigned char)((s->bIgnoreLoop >> 1) ^ 2);
 d->ADSRX.State = (unsigned char)s->ADSRX.State;
 if (s->bStop) d->ADSRX.State = ADSR_RELEASE;
 d->ADSRX.AttackModeExp = (unsigned char)s->ADSRX.AttackModeExp;
 d->ADSRX.AttackRate = (unsigned char)s->ADSRX.AttackRate;
 d->ADSRX.DecayRate = (unsigned char)s->ADSRX.DecayRate;
 d->ADSRX.SustainLevel = (unsigned char)s->ADSRX.SustainLevel;
 d->ADSRX.SustainModeExp = (unsigned char)s->ADSRX.SustainModeExp;
 d->ADSRX.SustainIncrease = (unsigned char)s->ADSRX.SustainIncrease;
 d->ADSRX.SustainRate = (unsigned char)s->ADSRX.SustainRate;
 d->ADSRX.ReleaseModeExp = (unsigned char)s->ADSRX.ReleaseModeExp;
 d->ADSRX.ReleaseRate = (unsigned char)s->ADSRX.ReleaseRate;
 d->ADSRX.EnvelopeVol = (int)((uint32_t)s->ADSRX.EnvelopeVol >> 16);
 d->ADSRX.StepCounter = s->ADSRX.StepCounter;
 if (s->bOn) spu.dwChannelsAudible |= 1<<ch;
 else d->ADSRX.EnvelopeVol = 0;
}

// force load from regArea to variables
static void load_register(unsigned long reg, unsigned int cycles)
{
 unsigned short *r = &spu.regArea[((reg & 0xfff) - 0xc00) >> 1];
 *r ^= 1;
 SPUwriteRegister(reg, *r ^ 1, cycles);
}

static void LoadStateV5(SPUOSSFreeze_t * pFO, uint32_t cycles);
static void LoadStateUnknown(uint32_t cycles);

////////////////////////////////////////////////////////////////////////
// SPUFREEZE: called by main emu on savestate load/save.
//
// Compatibility note: pcsxr-360 misc.c calls SPU_freeze(mode, pF, cycles)
// with a single SPUFreeze_t pointer that contains SPURam[0x80000] and
// xa_decode_t embedded.  We use that buffer as the storage for our
// internal SPUOSSFreeze_t state (placed after the existing fields,
// before SPURam).  The SPURam region itself is loaded/saved by misc.c
// via *ram = spu.spuMem indirectly — pcsxr-360 misc.c handles the
// 0x80000 byte SPURam region as part of psxSS_write/psxSS_read.
////////////////////////////////////////////////////////////////////////

long DoFreeze(int ulFreezeMode, SPUFreeze_t * pF, unsigned short **ram,
 void * pF2, unsigned int cycles)
{
 SPUOSSFreeze_t * pFO = NULL;
 sample_buf_rvb *sb_rvb = &spu.sb_rvb;
 int i, j;

 if (ram)
  *ram = spu.spuMem;
 if (!pF || !pF2) return 0;
 pFO = (SPUOSSFreeze_t *)pF2;

 if(ulFreezeMode)                                      // info or save?
  {
   int xa_left = 0, cdda_left = 0;
   do_samples(cycles, 1);

   if (ulFreezeMode == 1)
    memset(pF, 0, sizeof(*pF));

   strcpy((char *)pF->PluginName, "PBOSS");
   pF->PluginVersion = 5;
   pF->Size = sizeof(SPUFreeze_t);

   if(ulFreezeMode==2) return 1;                       // info mode
   regAreaGet(H_SPUctrl) = spu.spuCtrl;
   regAreaGet(H_SPUstat) = spu.spuStat;
   memcpy(pF->SPUPorts, spu.regArea, 0x200);

   if(spu.xapGlobal && spu.XAPlay!=spu.XAFeed)
    {
     xa_left = spu.XAFeed - spu.XAPlay;
     if (xa_left < 0)
      xa_left = (spu.XAEnd - spu.XAPlay) + (spu.XAFeed - spu.XAStart);
     pFO->xa = *spu.xapGlobal;
    }
   else if (spu.CDDAPlay != spu.CDDAFeed)
    {
     unsigned int *p = spu.CDDAPlay;
     cdda_left = spu.CDDAFeed - spu.CDDAPlay;
     if (cdda_left < 0)
      cdda_left = (spu.CDDAEnd - spu.CDDAPlay) + (spu.CDDAFeed - spu.CDDAStart);
     if (cdda_left > (int)(sizeof(pFO->xa.pcm) / 4))
      cdda_left = sizeof(pFO->xa.pcm) / 4;
     if (p + cdda_left <= spu.CDDAEnd)
      memcpy(pFO->xa.pcm, p, cdda_left * 4);
     else {
      memcpy(pFO->xa.pcm, p, (spu.CDDAEnd - p) * 4);
      memcpy((char *)pFO->xa.pcm + (spu.CDDAEnd - p) * 4, spu.CDDAStart,
             (cdda_left - (spu.CDDAEnd - p)) * 4);
     }
     pFO->xa.nsamples = 0;
    }
   else
    memset(&pFO->xa, 0, sizeof(xa_decode_t));

   pFO->spuIrq = spu.regArea[(0x0da4 - 0x0c00) / 2];
   if(spu.pSpuIrq) pFO->pSpuIrq = (uint32_t)(spu.pSpuIrq - spu.spuMemC);

   pFO->spuAddr = spu.spuAddr;
   if(pFO->spuAddr==0) pFO->spuAddr=0xbaadf00d;
   pFO->decode_pos = (unsigned short)spu.decode_pos;
   pFO->rvb_cur = spu.rvb->CurrAddr;
   pFO->xa_left = (uint16_t)xa_left;
   pFO->cdda_left = (uint16_t)cdda_left;
   pFO->cycles_played = spu.cycles_played;
   pFO->cycles_dma_end = spu.cycles_dma_end;
   pFO->decode_dirty_ch = spu.decode_dirty_ch;
   pFO->dwNoiseVal = spu.dwNoiseVal;
   pFO->dwNoiseCount = spu.dwNoiseCount;
   pFO->XARepeat = spu.XARepeat;
   pFO->XALastVal = spu.XALastVal;
   pFO->last_keyon_cycles = spu.last_keyon_cycles;
   for (i = 0; i < 2; i++)
    memcpy(&pFO->rvb_sb[i], sb_rvb->sample[i], sizeof(pFO->rvb_sb[i]));
   pFO->interpolation = spu.interpolation;

   for(i=0;i<MAXCHAN;i++)
    {
     save_channel(&pFO->s_chan[i],&spu.s_chan[i],i);
     if(spu.s_chan[i].pCurr)
      pFO->s_chan[i].iCurr=spu.s_chan[i].pCurr-spu.spuMemC;
     if(spu.s_chan[i].pLoop)
      pFO->s_chan[i].iLoop=spu.s_chan[i].pLoop-spu.spuMemC;
    }

   /* Also copy SPU RAM into the SPUFreeze_t.SPURam buffer for compat
    * with pcsxr-360 misc.c which writes that block as part of state. */
   memcpy(pF->SPURam, spu.spuMemC, 0x80000);

   return 1;
  }

 if(ulFreezeMode!=0) return 0;                         // bad mode

 /* Load path. */
 memcpy(spu.regArea, pF->SPUPorts, 0x200);
 memcpy(spu.spuMemC, pF->SPURam, 0x80000);
 spu.bMemDirty = 1;
 spu.spuCtrl = regAreaGet(H_SPUctrl);
 spu.spuStat = regAreaGet(H_SPUstat);

 if (!strcmp((char *)pF->PluginName, "PBOSS") && pF->PluginVersion == 5)
   LoadStateV5(pFO, cycles);
 else {
   LoadStateUnknown(cycles);
   pFO = NULL;
 }

 spu.XAPlay = spu.XAFeed = spu.XAStart;
 spu.CDDAPlay = spu.CDDAFeed = spu.CDDAStart;
 spu.cdClearSamples = 512;
 if (pFO && pFO->xa_left && pFO->xa.nsamples) {
  FeedXA(&pFO->xa);
  spu.XAPlay = spu.XAFeed - pFO->xa_left;
  if (spu.XAPlay < spu.XAStart)
   spu.XAPlay = spu.XAStart;
 }
 else if (pFO && pFO->cdda_left) {
  FeedCDDA((unsigned char *)pFO->xa.pcm, pFO->cdda_left * 4);
 }

 spu.cycles_dma_end = 0;
 spu.decode_dirty_ch = spu.dwChannelsAudible & 0x0a;
 spu.dwNoiseVal = 0;
 spu.dwNoiseCount = 0;
 spu.XARepeat = 0;
 spu.XALastVal = 0;
 spu.last_keyon_cycles = cycles - 16*786u;
 spu.interpolation = -1;
 if (pFO) {
  spu.cycles_dma_end = pFO->cycles_dma_end;
  spu.decode_dirty_ch = pFO->decode_dirty_ch;
  spu.dwNoiseVal = pFO->dwNoiseVal;
  spu.dwNoiseCount = pFO->dwNoiseCount;
  spu.XARepeat = pFO->XARepeat;
  spu.XALastVal = pFO->XALastVal;
  spu.last_keyon_cycles = pFO->last_keyon_cycles;
  for (i = 0; i < 2; i++)
   for (j = 0; j < 2; j++)
    memcpy(&sb_rvb->sample[i][j*4], pFO->rvb_sb[i], 4 * sizeof(sb_rvb->sample[i][0]));
  spu.interpolation = pFO->interpolation;
 }

 /* repair some globals via SPU register write replay */
 for(i=0;i<=62;i+=2)
  load_register(0x0dc0+i, cycles);
 load_register(0x0da2, cycles);
 load_register(0x0d84, cycles);
 load_register(0x0d86, cycles);
 load_register(0x0db0, cycles);
 load_register(0x0db2, cycles);

 spu.rvb->StartAddr = regAreaGet(0x0da2) << 2;
 if (spu.rvb->CurrAddr < spu.rvb->StartAddr)
  spu.rvb->CurrAddr = spu.rvb->StartAddr;

 ClearWorkingState();

 if (spu.spuCtrl & CTRL_IRQ)
  schedule_next_irq();

 return 1;
}

static void LoadStateV5(SPUOSSFreeze_t * pFO, uint32_t cycles)
{
 int i;

 spu.pSpuIrq = spu.spuMemC + ((spu.regArea[(0x0da4 - 0x0c00) / 2] << 3) & ~0xf);

 if(pFO->spuAddr)
  {
   if (pFO->spuAddr == 0xbaadf00d) spu.spuAddr = 0;
   else spu.spuAddr = pFO->spuAddr & 0x7fffe;
  }
 spu.decode_pos = pFO->decode_pos & 0x1ff;
 spu.rvb->CurrAddr = pFO->rvb_cur;
 spu.cycles_played = pFO->cycles_played ? pFO->cycles_played : cycles;

 spu.dwNewChannel=0;
 spu.dwChannelsAudible=0;
 spu.dwChannelDead=0;
 for(i=0;i<MAXCHAN;i++)
  {
   load_channel(&spu.s_chan[i],&pFO->s_chan[i],i);

   spu.s_chan[i].pCurr+=(uintptr_t)spu.spuMemC;
   spu.s_chan[i].pLoop+=(uintptr_t)spu.spuMemC;
  }
}

static void LoadStateUnknown(uint32_t cycles)
{
 int i;

 for(i=0;i<MAXCHAN;i++)
  {
   spu.s_chan[i].pLoop=spu.spuMemC;
  }

 spu.dwNewChannel=0;
 spu.dwChannelsAudible=0;
 spu.dwChannelDead=0;
 spu.pSpuIrq=spu.spuMemC;
 spu.cycles_played = cycles;

 for(i=0;i<0xc0;i++)
  {
   load_register(0x1f801c00 + i*2, cycles);
  }
}

/* SPUfreeze plugin entry point — pcsxr-360 misc.c invokes this with
 * the legacy 3-arg form (mode, pF, cycles).  Uses the original
 * pcsxr-360 layout: misc.c allocates Size = sizeof(SPUFreeze_t) +
 * sizeof(SPUOSSFreeze_t) bytes contiguously, with the OSS struct
 * placed immediately after the SPUFreeze_t header.  We expose that
 * via `(SPUOSSFreeze_t *)(pF + 1)`.
 *
 * Mode 0: load (pF already contains the saved blob).
 * Mode 1: save (DoFreeze fills pF + the trailing OSS region).
 * Mode 2: info — only fill PluginName/Version/Size so misc.c knows
 *         how big a buffer to allocate before calling mode 1. */
long CALLBACK SPUfreeze(uint32_t ulFreezeMode, SPUFreeze_t * pF, uint32_t cycles)
{
 SPUOSSFreeze_t *pFO;
 const uint32_t totalSize = (uint32_t)(sizeof(SPUFreeze_t) + sizeof(SPUOSSFreeze_t));

 if (!pF) return 0;

 if (ulFreezeMode == 2) {
  /* Info mode: misc.c calls this with a 16-byte stub buffer just to
   * read pF->Size.  We must report the full eventual size including
   * the trailing SPUOSSFreeze_t region. */
  strcpy((char *)pF->PluginName, "PBOSS");
  pF->PluginVersion = 5;
  pF->Size = totalSize;
  return 1;
 }

 /* For modes 0 and 1, pF was allocated with totalSize bytes by
  * misc.c, so the OSS region sits right after the SPUFreeze_t
  * header.  Same trick the original pcsxr-360 SPUfreeze used. */
 pFO = (SPUOSSFreeze_t *)(pF + 1);

 return DoFreeze((int)ulFreezeMode, pF, NULL, pFO, cycles);
}
