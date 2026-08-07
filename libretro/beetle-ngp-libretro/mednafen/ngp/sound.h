//---------------------------------------------------------------------------
// NEOPOP : Emulator as in Dreamland
//
// Copyright (c) 2001-2002 by neopop_uk
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version. See also the license.txt file for
//	additional informations.
//---------------------------------------------------------------------------

#ifndef __SOUND__
#define __SOUND__

#include <stdint.h>

/* Speed of DAC playback (in Hz) */
#define DAC_FREQUENCY		8000

#ifdef __cplusplus
extern "C" {
#endif
void Write_SoundChipLeft(uint8_t data);
void Write_SoundChipRight(uint8_t data);
void dac_write_left(uint8_t);
void dac_write_right(uint8_t);
void MDFNNGPCSOUND_SetEnable(bool set);
void MDFNNGPCSOUND_Init(void);
int MDFNNGPCSOUND_StateAction(void *data, int load, int data_only);
void MDFNNGPC_SetSoundRate(void);
int32_t MDFNNGPCSOUND_Flush(int16_t *SoundBuf, const int32_t MaxSoundFrames);

#ifdef __cplusplus
}
#endif

#endif
