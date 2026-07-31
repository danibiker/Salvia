#ifndef __WSWAN_SOUND_H
#define __WSWAN_SOUND_H

#include "../state.h"

#ifdef __cplusplus
extern "C" {
#endif

void WSwan_SoundInit(void);

void WSwan_SoundReset(void);

bool WSwan_SetSoundRate(uint32 rate);

int32 WSwan_SoundFlush(int16 **SoundBuf, int32 *SoundBufSize);

void WSwan_SoundKill(void);

int WSwan_SoundStateAction(StateMem *sm, int load, int data_only);

void WSwan_SetSoundMultiplier(double multiplier);

void WSwan_SoundWrite(uint32, uint8);
uint8 WSwan_SoundRead(uint32);
void WSwan_SoundCheckRAMWrite(uint32 A);

#ifdef __cplusplus
}
#endif

#endif
