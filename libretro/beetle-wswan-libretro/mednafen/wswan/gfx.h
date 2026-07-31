#ifndef __WSWAN_GFX_H
#define __WSWAN_GFX_H

#include "../state.h"
#include "../video.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint8		wsTCache[512*64];		  //tiles cache
extern uint8		wsTCacheFlipped[512*64];  	  //tiles cache (H flip)
extern uint8		wsTCacheUpdate[512];	  //tiles cache flags
extern uint8		wsTCache2[512*64];		  //tiles cache
extern uint8		wsTCacheFlipped2[512*64];  	  //tiles cache (H flip)
extern uint8		wsTCacheUpdate2[512];	  //tiles cache flags


extern uint8		wsTileRow[8];		  //extracted 8 pixels (tile row)
extern int		wsVMode;			  //Video Mode	

void wsMakeTiles(void);
void wsGetTile(uint32,uint32,int,int,int);
void wsSetVideo(int, bool);
void WSWan_TCacheInvalidByAddr(uint32);

bool wsExecuteLine(MDFN_Surface *surface, bool skip);

void WSwan_SetMonoPalette(int depth, uint32 mono_start, uint32 mono_end);

void WSwan_SetPixelFormat(int depth, uint32 mono_start, uint32 mono_end);

void WSwan_GfxInit(void);

void WSwan_GfxReset(void);

uint8 WSwan_GfxRead(uint32 A);

int WSwan_GfxStateAction(StateMem *sm, int load, int data_only);

void wsScanline(uint16 *target, int depth);

extern uint32		dx_r,dx_g,dx_b,dx_sr,dx_sg,dx_sb;
extern uint32		dx_bits,dx_pitch,cmov,dx_linewidth_blit,dx_buffer_line;

void WSwan_GfxWrite(uint32 A, uint8 V);
void WSwan_GfxWSCPaletteRAMWrite(uint32 ws_offset, uint8 data);

void WSwan_SetLayerEnableMask(uint64 mask);

#ifdef __cplusplus
}
#endif


#endif
