/***************************************************************************
                            xa.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

#include "stdafx.h"
#include "spu.h"
#define _IN_XA
#include <stdint.h>
#include "../../libpcsxcore/decode_xa.h"  /* xa_decode_t */

// will be included from spu.c
#ifdef _IN_SPU

////////////////////////////////////////////////////////////////////////
// XA GLOBALS
////////////////////////////////////////////////////////////////////////

static int gauss_ptr = 0;
static int gauss_window[8] = {0, 0, 0, 0, 0, 0, 0, 0};

#define gvall0 gauss_window[gauss_ptr]
#define gvall(x) gauss_window[(gauss_ptr+x)&3]
#define gvalr0 gauss_window[4+gauss_ptr]
#define gvalr(x) gauss_window[4+((gauss_ptr+x)&3)]

////////////////////////////////////////////////////////////////////////
// MIX XA & CDDA
////////////////////////////////////////////////////////////////////////

INLINE void SkipCD(int ns_to, int decode_pos)
{
 int cursor = decode_pos;
 int ns;

 if(spu.XAPlay != spu.XAFeed)
 {
  for(ns = 0; ns < ns_to*2; ns += 2)
   {
    if(spu.XAPlay != spu.XAFeed) spu.XAPlay++;
    if(spu.XAPlay == spu.XAEnd) spu.XAPlay=spu.XAStart;

    spu.spuMem[cursor] = 0;
    spu.spuMem[cursor + 0x400/2] = 0;
    cursor = (cursor + 1) & 0x1ff;
   }
 }
 else if(spu.CDDAPlay != spu.CDDAFeed)
 {
  for(ns = 0; ns < ns_to*2; ns += 2)
   {
    if(spu.CDDAPlay != spu.CDDAFeed) spu.CDDAPlay++;
    if(spu.CDDAPlay == spu.CDDAEnd) spu.CDDAPlay=spu.CDDAStart;

    spu.spuMem[cursor] = 0;
    spu.spuMem[cursor + 0x400/2] = 0;
    cursor = (cursor + 1) & 0x1ff;
   }
 }
 spu.XALastVal = 0;
}

INLINE void MixCD(int *SSumLR, int *RVB, int ns_to, int decode_pos)
{
 int vll = spu.iLeftXAVol * spu.cdv.ll >> 7;
 int vrl = spu.iLeftXAVol * spu.cdv.rl >> 7;
 int vlr = spu.iRightXAVol * spu.cdv.lr >> 7;
 int vrr = spu.iRightXAVol * spu.cdv.rr >> 7;
 int cursor = decode_pos;
 int l1, r1, l, r;
 int ns;
 uint32_t v = spu.XALastVal;

 // note: spu volume doesn't affect cd capture
 if ((spu.cdv.ll | spu.cdv.lr | spu.cdv.rl | spu.cdv.rr) == 0)
 {
  SkipCD(ns_to, decode_pos);
  return;
 }

 /* Endian-safe sample extraction for both XA and CDDA buffers.
  *
  * The original pcsx_rearmed code did `v = *spu.XAPlay++; l1 = (short)v;
  * r1 = (short)(v >> 16);` which silently relies on host being
  * little-endian (so v's low 16 bits == first sample in memory == L,
  * high 16 bits == second sample == R).  On big-endian Xenon (Xbox
  * 360) that read is byte-reversed and L/R end up swapped at best,
  * or — for CDDA where the buffer holds raw LE-byte PCM straight
  * from the disc — every sample comes out byte-swapped, which sounds
  * exactly like the constant gaussian-noise-over-the-music symptom
  * the user reported in TR2/Dino Crisis.
  *
  * Fixes:
  *  - XA: read as 2 host shorts (the buffer was filled by FeedXA
  *    via memcpy of host shorts, so direct short reads recover L/R
  *    in the right order regardless of host endianness).
  *  - CDDA: the buffer is filled by memcpy of LE PCM bytes from the
  *    CD reader, so we read the shorts and apply LE16TOH to convert
  *    LE byte order to host short value.
  *  - spuMem stores: write HTOLE16(host_short) so PSX SPU RAM ends
  *    up with bytes in LE order (PSX RAM is LE), regardless of host. */
 if(spu.XAPlay != spu.XAFeed || spu.XARepeat > 0)
 {
  if(spu.XAPlay == spu.XAFeed)
   spu.XARepeat--;

  for(ns = 0; ns < ns_to*2; ns += 2)
   {
    if(spu.XAPlay != spu.XAFeed) {
     /* XA: buffer holds host shorts laid out as [L0, R0, L1, R1, ...]. */
     l1 = (short)((short *)spu.XAPlay)[0];
     r1 = (short)((short *)spu.XAPlay)[1];
     v = ((uint32_t)(unsigned short)r1 << 16) | (unsigned short)l1; /* keep XALastVal semantics */
     spu.XAPlay++;
    } else {
     l1 = (short)v;
     r1 = (short)(v >> 16);
    }
    if(spu.XAPlay == spu.XAEnd) spu.XAPlay=spu.XAStart;

    l = (l1 * vll + r1 * vrl) >> 15;
    r = (r1 * vrr + l1 * vlr) >> 15;
    ssat32_to_16(l);
    ssat32_to_16(r);
    if (spu.spuCtrl & CTRL_CD)
    {
     SSumLR[ns+0] += l;
     SSumLR[ns+1] += r;
    }
    if (unlikely(spu.spuCtrl & CTRL_CDREVERB))
    {
     RVB[ns+0] += l;
     RVB[ns+1] += r;
    }

    spu.spuMem[cursor] = HTOLE16((unsigned short)l1);
    spu.spuMem[cursor + 0x400/2] = HTOLE16((unsigned short)r1);
    cursor = (cursor + 1) & 0x1ff;
   }
  spu.XALastVal = v;
 }
 // occasionally CDDAFeed underflows by a few samples due to poor timing,
 // hence this 'ns_to < 8'
 else if(spu.CDDAPlay != spu.CDDAFeed || ns_to < 8)
 {
  for(ns = 0; ns < ns_to*2; ns += 2)
   {
    if(spu.CDDAPlay != spu.CDDAFeed) {
     /* CDDA: buffer holds raw LE-byte PCM straight from the disc.
      * LE16TOH converts the bytes to a host short value correctly
      * on both host endianness conventions. */
     unsigned short *p = (unsigned short *)spu.CDDAPlay;
     l1 = (short)LE16TOH(p[0]);
     r1 = (short)LE16TOH(p[1]);
     v = ((uint32_t)(unsigned short)r1 << 16) | (unsigned short)l1;
     spu.CDDAPlay++;
    } else {
     l1 = (short)v;
     r1 = (short)(v >> 16);
    }
    if(spu.CDDAPlay == spu.CDDAEnd) spu.CDDAPlay=spu.CDDAStart;

    l = (l1 * vll + r1 * vrl) >> 15;
    r = (r1 * vrr + l1 * vlr) >> 15;
    ssat32_to_16(l);
    ssat32_to_16(r);
    if (spu.spuCtrl & CTRL_CD)
    {
     SSumLR[ns+0] += l;
     SSumLR[ns+1] += r;
    }
    if (unlikely(spu.spuCtrl & CTRL_CDREVERB))
    {
     RVB[ns+0] += l;
     RVB[ns+1] += r;
    }

    spu.spuMem[cursor] = HTOLE16((unsigned short)l1);
    spu.spuMem[cursor + 0x400/2] = HTOLE16((unsigned short)r1);
    cursor = (cursor + 1) & 0x1ff;
   }
  spu.XALastVal = v;
 }
 else if (spu.cdClearSamples > 0)
 {
  for(ns = 0; ns < ns_to; ns++)
   {
    spu.spuMem[cursor] = spu.spuMem[cursor + 0x400/2] = 0;
    cursor = (cursor + 1) & 0x1ff;
   }
  spu.cdClearSamples -= ns_to;
  spu.XALastVal = 0;
 }
}

////////////////////////////////////////////////////////////////////////
// small linux time helper... only used for watchdog
////////////////////////////////////////////////////////////////////////

#if 0
static unsigned long timeGetTime_spu()
{
#if defined(NO_OS)
 return 0;
#elif defined(_WIN32)
 return GetTickCount();
#else
 struct timeval tv;
 gettimeofday(&tv, 0);                                 // well, maybe there are better ways
 return tv.tv_sec * 1000 + tv.tv_usec/1000;            // to do that, but at least it works
#endif
}
#endif

////////////////////////////////////////////////////////////////////////
// FEED XA 
////////////////////////////////////////////////////////////////////////

void FeedXA(const xa_decode_t *xap)
{
 int sinc,spos,i,iSize,iPlace,vl,vr;

 if(!spu.bSPUIsOpen) return;

 spu.XARepeat  = 3;                                    // set up repeat

#if 0//def XA_HACK
 iSize=((45500*xap->nsamples)/xap->freq);              // get size
#else
 iSize=((44100*xap->nsamples)/xap->freq);              // get size
#endif
 if(!iSize) return;                                    // none? bye

 if(spu.XAFeed<spu.XAPlay) iPlace=spu.XAPlay-spu.XAFeed; // how much space in my buf?
 else              iPlace=(spu.XAEnd-spu.XAFeed) + (spu.XAPlay-spu.XAStart);

 if(iPlace==0) return;                                 // no place at all

 //----------------------------------------------------//
#if 0
 if(spu_config.iXAPitch)                               // pitch change option?
  {
   static DWORD dwLT=0;
   static DWORD dwFPS=0;
   static int   iFPSCnt=0;
   static int   iLastSize=0;
   static DWORD dwL1=0;
   DWORD dw=timeGetTime_spu(),dw1,dw2;

   iPlace=iSize;

   dwFPS+=dw-dwLT;iFPSCnt++;

   dwLT=dw;
                                       
   if(iFPSCnt>=10)
    {
     if(!dwFPS) dwFPS=1;
     dw1=1000000/dwFPS; 
     if(dw1>=(dwL1-100) && dw1<=(dwL1+100)) dw1=dwL1;
     else dwL1=dw1;
     dw2=(xap->freq*100/xap->nsamples);
     if((!dw1)||((dw2+100)>=dw1)) iLastSize=0;
     else
      {
       iLastSize=iSize*dw2/dw1;
       if(iLastSize>iPlace) iLastSize=iPlace;
       iSize=iLastSize;
      }
     iFPSCnt=0;dwFPS=0;
    }
   else
    {
     if(iLastSize) iSize=iLastSize;
    }
  }
#endif
 //----------------------------------------------------//

 spos=0x10000L;
 sinc = (xap->nsamples << 16) / iSize;                 // calc freq by num / size

 if(xap->stereo)
{
   /* xap->pcm is a host-short array filled by libpcsxcore::xa_decode.
    * On big-endian Xenon, reading it through (uint32_t *) — as the
    * original pcsx_rearmed code did — and splitting with LOWORD()/
    * HIWORD() extracts R into "low 16" and L into "high 16" because
    * the BE word layout is (pcm[0]<<16)|pcm[1].  That made the
    * gauss_window get filled with L/R swapped and the resulting
    * write to XAFeed produced an XA stream that MixCD then read as
    * R-as-L, giving the unpleasant audio in Dino Crisis intro and
    * any FMV with audio resampling through gauss.
    *
    * Fix: keep `pS` as uint32_t* for the non-interpolation path
    * (where `l = *pS++` already produces the correct two-host-shorts
    * layout in XAFeed regardless of host endianness), but use a
    * parallel short-pointer view for filling gauss_window. */
   uint32_t * pS=(uint32_t *)xap->pcm;
   short    * pSs=(short *)xap->pcm;
   uint32_t l=0;

#if 0
   if(spu_config.iXAPitch)
    {
     int32_t l1,l2;short s;
     for(i=0;i<iSize;i++)
      {
       if(spu_config.iUseInterpolation==2)
        {
         while(spos>=0x10000L)
          {
           l = *pS++;
           gauss_window[gauss_ptr] = (short)LOWORD(l);
           gauss_window[4+gauss_ptr] = (short)HIWORD(l);
           gauss_ptr = (gauss_ptr+1) & 3;
           spos -= 0x10000L;
          }
         vl = (spos >> 6) & ~3;
         vr=(gauss[vl]*gvall0) >> 15;
         vr+=(gauss[vl+1]*gvall(1)) >> 15;
         vr+=(gauss[vl+2]*gvall(2)) >> 15;
         vr+=(gauss[vl+3]*gvall(3)) >> 15;
         l= vr & 0xffff;
         vr=(gauss[vl]*gvalr0) >> 15;
         vr+=(gauss[vl+1]*gvalr(1)) >> 15;
         vr+=(gauss[vl+2]*gvalr(2)) >> 15;
         vr+=(gauss[vl+3]*gvalr(3)) >> 15;
         l |= vr << 16;
        }
       else
        {
         while(spos>=0x10000L)
          {
           l = *pS++;
           spos -= 0x10000L;
          }
        }

       s=(short)LOWORD(l);
       l1=s;
       l1=(l1*iPlace)/iSize;
       ssat32_to_16(l1);
       s=(short)HIWORD(l);
       l2=s;
       l2=(l2*iPlace)/iSize;
       ssat32_to_16(l2);
       l=(l1&0xffff)|(l2<<16);

       *spu.XAFeed++=l;

       if(spu.XAFeed==spu.XAEnd) spu.XAFeed=spu.XAStart;
       if(spu.XAFeed==spu.XAPlay)
        {
         if(spu.XAPlay!=spu.XAStart) spu.XAFeed=spu.XAPlay-1;
         break;
        }

       spos += sinc;
      }
    }
   else
#endif
    {
     for(i=0;i<iSize;i++)
      {
       if(spu_config.iUseInterpolation==2)
        {
         /* Endian-safe: read L/R as separate host shorts (pSs view)
          * to fill gauss_window correctly.  Using LOWORD/HIWORD on
          * a uint32_t value would put R into the L slot on big-
          * endian Xenon, swapping channels in the gauss-interpolated
          * XA stream — that was the unpleasant audio in Dino Crisis
          * intro / FMVs that go through the gauss path. */
         int vL, vR;
         while(spos>=0x10000L)
          {
           gauss_window[gauss_ptr]   = pSs[0];   /* L = pcm[2k]   */
           gauss_window[4+gauss_ptr] = pSs[1];   /* R = pcm[2k+1] */
           pSs += 2;
           pS  += 1;                              /* keep both views in sync */
           gauss_ptr = (gauss_ptr+1) & 3;
           spos -= 0x10000L;
          }
         vl = (spos >> 6) & ~3;
         vL  = (gauss[vl]  *gvall0)   >> 15;
         vL += (gauss[vl+1]*gvall(1)) >> 15;
         vL += (gauss[vl+2]*gvall(2)) >> 15;
         vL += (gauss[vl+3]*gvall(3)) >> 15;
         vR  = (gauss[vl]  *gvalr0)   >> 15;
         vR += (gauss[vl+1]*gvalr(1)) >> 15;
         vR += (gauss[vl+2]*gvalr(2)) >> 15;
         vR += (gauss[vl+3]*gvalr(3)) >> 15;
         /* Write the L/R pair as two adjacent host shorts.  MixCD
          * reads `((short *)spu.XAPlay)[0/1]` regardless of host
          * endianness, so this layout is endian-correct.  Don't
          * use uint32 packing (l = (vR << 16) | vL) because BE
          * stores that with bytes in the wrong order for our reads. */
         ((short *)spu.XAFeed)[0] = (short)vL;
         ((short *)spu.XAFeed)[1] = (short)vR;
         /* Reconstruct `l` only if any later code observes it; in
          * practice it isn't read after this point inside the loop. */
         l = ((uint32_t)(unsigned short)vR << 16) | (unsigned short)vL;
        }
       else
        {
         while(spos>=0x10000L)
          {
           l = *pS++;
           pSs += 2;                              /* keep short view in sync */
           spos -= 0x10000L;
          }
         /* Non-interp: a uint32 read of two host shorts and an
          * uint32 write of the same value preserves the two-host-
          * shorts-adjacent layout on any host endianness, so MixCD's
          * short-pair read still recovers L/R correctly. */
         *(uint32_t *)spu.XAFeed = l;
        }

       spu.XAFeed++;

       if(spu.XAFeed==spu.XAEnd) spu.XAFeed=spu.XAStart;
       if(spu.XAFeed==spu.XAPlay)
        {
         if(spu.XAPlay!=spu.XAStart) spu.XAFeed=spu.XAPlay-1;
         break;
        }

       spos += sinc;
      }
    }
  }
 else
  {
   unsigned short * pS=(unsigned short *)xap->pcm;
   uint32_t l;short s=0;

#if 0
   if(spu_config.iXAPitch)
    {
     int32_t l1;
     for(i=0;i<iSize;i++)
      {
       if(spu_config.iUseInterpolation==2)
        {
         while(spos>=0x10000L)
          {
           gauss_window[gauss_ptr] = (short)*pS++;
           gauss_ptr = (gauss_ptr+1) & 3;
           spos -= 0x10000L;
          }
         vl = (spos >> 6) & ~3;
         vr=(gauss[vl]*gvall0) >> 15;
         vr+=(gauss[vl+1]*gvall(1)) >> 15;
         vr+=(gauss[vl+2]*gvall(2)) >> 15;
         vr+=(gauss[vl+3]*gvall(3)) >> 15;
         l1=s= vr;
         l1 &= 0xffff;
        }
       else
        {
         while(spos>=0x10000L)
          {
           s = *pS++;
           spos -= 0x10000L;
          }
         l1=s;
        }

       l1=(l1*iPlace)/iSize;
       ssat32_to_16(l1);
       l=(l1&0xffff)|(l1<<16);
       *spu.XAFeed++=l;

       if(spu.XAFeed==spu.XAEnd) spu.XAFeed=spu.XAStart;
       if(spu.XAFeed==spu.XAPlay)
        {
         if(spu.XAPlay!=spu.XAStart) spu.XAFeed=spu.XAPlay-1;
         break;
        }

       spos += sinc;
      }
    }
   else
#endif
    {
     for(i=0;i<iSize;i++)
      {
       if(spu_config.iUseInterpolation==2)
        {
         while(spos>=0x10000L)
          {
           gauss_window[gauss_ptr] = (short)*pS++;
           gauss_ptr = (gauss_ptr+1) & 3;
           spos -= 0x10000L;
          }
         vl = (spos >> 6) & ~3;
         vr=(gauss[vl]*gvall0) >> 15;
         vr+=(gauss[vl+1]*gvall(1)) >> 15;
         vr+=(gauss[vl+2]*gvall(2)) >> 15;
         vr+=(gauss[vl+3]*gvall(3)) >> 15;
         l=s= vr;
        }
       else
        {
         while(spos>=0x10000L)
          {
           s = *pS++;
           spos -= 0x10000L;
          }
         l=s;
        }

       l &= 0xffff;
       *spu.XAFeed++=(l|(l<<16));

       if(spu.XAFeed==spu.XAEnd) spu.XAFeed=spu.XAStart;
       if(spu.XAFeed==spu.XAPlay)
        {
         if(spu.XAPlay!=spu.XAStart) spu.XAFeed=spu.XAPlay-1;
         break;
        }

       spos += sinc;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////
// FEED CDDA
////////////////////////////////////////////////////////////////////////

void FeedCDDA(unsigned char *pcm, int nBytes)
{
 int space;
 space=(spu.CDDAPlay-spu.CDDAFeed-1)*4 & (CDDA_BUFFER_SIZE - 1);
 if (space < nBytes) {
  log_unhandled("FeedCDDA: %d/%d\n", nBytes, space);
  return;
 }

 while(nBytes>0)
  {
   if(spu.CDDAFeed==spu.CDDAEnd) spu.CDDAFeed=spu.CDDAStart;
   space=(spu.CDDAPlay-spu.CDDAFeed-1)*4 & (CDDA_BUFFER_SIZE - 1);
   if(spu.CDDAFeed+space/4>spu.CDDAEnd)
    space=(spu.CDDAEnd-spu.CDDAFeed)*4;
   if(space>nBytes)
    space=nBytes;

   memcpy(spu.CDDAFeed,pcm,space);
   spu.CDDAFeed+=space/4;
   nBytes-=space;
   pcm+=space;
  }
}

#endif
// vim:shiftwidth=1:expandtab
