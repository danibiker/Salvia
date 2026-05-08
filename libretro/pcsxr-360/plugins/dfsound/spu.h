/***************************************************************************
                            spu.h  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/

#ifndef __P_SPU_H__
#define __P_SPU_H__

/* Cycle-driven SPU plugin entry points (port from pcsx_rearmed).
 *
 * All entry points that can trigger SPU IRQs take an extra `cycles`
 * argument so the plugin can do_samples up to that exact CPU cycle
 * before applying the side effect (register write, DMA, XA feed).
 * That is what enables cycle-correct IRQ delivery — the wait/handshake
 * mechanism of the old PEOPS-thread model is completely gone.
 *
 * IMPORTANT: This header MUST NOT include libpcsxcore/plugins.h.
 * That header declares typedefs `SPUinit`, `SPUwriteRegister`, ...
 * (function-pointer types for the plugin loader) that share names
 * with the actual function declarations below.  Pulling them into
 * the same TU produces "redefinition" errors throughout the plugin.
 * pcsx_rearmed avoids this by simply never #including plugins.h
 * inside dfsound/.  We do the same and forward-declare the only two
 * types from plugins.h that the plugin's API references in its
 * signatures (struct SPUFreeze and xa_decode_t).
 *
 * The PEOPS_* renames below come straight after the forward decls so
 * that the function declarations that follow get translated to
 * PEOPS_* before they reach the compiler.  xbPlugins.h's
 * SPU_PEOPS_PLUGIN table maps the unprefixed names back via the
 * plugin loader. */

#include <stdint.h>

/* Forward declarations — full definitions live in libpcsxcore. */
struct SPUFreeze;
struct xa_decode;
typedef struct SPUFreeze SPUFreeze_t;
typedef struct xa_decode xa_decode_t;

#ifdef _XBOX
#define SPUreadDMA              PEOPS_SPUreadDMA
#define SPUreadDMAMem           PEOPS_SPUreadDMAMem
#define SPUwriteDMA             PEOPS_SPUwriteDMA
#define SPUwriteDMAMem          PEOPS_SPUwriteDMAMem
#define SPUasync                PEOPS_SPUasync
#define SPUupdate               PEOPS_SPUupdate
#define SPUplayADPCMchannel     PEOPS_SPUplayADPCMchannel
#define SPUinit                 PEOPS_SPUinit
#define SPUopen                 PEOPS_SPUopen
#define SPUsetConfigFile        PEOPS_SPUsetConfigFile
#define SPUclose                PEOPS_SPUclose
#define SPUshutdown             PEOPS_SPUshutdown
#define SPUtest                 PEOPS_SPUtest
#define SPUconfigure            PEOPS_SPUconfigure
#define SPUabout                PEOPS_SPUabout
#define SPUregisterCallback     PEOPS_SPUregisterCallback
#define SPUregisterScheduleCb   PEOPS_SPUregisterScheduleCb
#define SPUregisterCDDAVolume   PEOPS_SPUregisterCDDAVolume
#define SPUplayCDDAchannel      PEOPS_SPUplayCDDAchannel
#define SPUsetCDvol             PEOPS_SPUsetCDvol
#define SPUwriteRegister        PEOPS_SPUwriteRegister
#define SPUreadRegister         PEOPS_SPUreadRegister
#define SPUfreeze               PEOPS_SPUfreeze
#endif

#ifndef CALLBACK
#define CALLBACK
#endif

long CALLBACK SPUopen(void);
long CALLBACK SPUinit(void);
long CALLBACK SPUshutdown(void);
long CALLBACK SPUclose(void);
void CALLBACK SPUconfigure(void);
void CALLBACK SPUwriteRegister(unsigned long, unsigned short, unsigned int);
unsigned short CALLBACK SPUreadRegister(unsigned long, unsigned int);
void CALLBACK SPUregisterCallback(void (CALLBACK *cb)(int));
void CALLBACK SPUregisterScheduleCb(void (CALLBACK *cb)(unsigned int));
long CALLBACK SPUfreeze(uint32_t ulFreezeMode, SPUFreeze_t * pF, uint32_t cycles);
void CALLBACK SPUasync(unsigned int, unsigned int);
void CALLBACK SPUupdate(void);

void CALLBACK SPUreadDMAMem(unsigned short * pusPSXMem, int iSize, unsigned int cycles);
void CALLBACK SPUwriteDMAMem(unsigned short * pusPSXMem, int iSize, unsigned int cycles);

void CALLBACK SPUplayADPCMchannel(xa_decode_t *xap, unsigned int cycle, int is_start);
int  CALLBACK SPUplayCDDAchannel(short *pcm, int bytes, unsigned int cycle, int is_start);
void CALLBACK SPUsetCDvol(unsigned char ll, unsigned char lr,
		unsigned char rl, unsigned char rr, unsigned int cycle);

// internal — used by freeze.c
void ClearWorkingState(void);
long DoFreeze(int ulFreezeMode, SPUFreeze_t * pF, unsigned short **ram,
		void * pF2, unsigned int cycles);

#endif /* __P_SPU_H__ */
