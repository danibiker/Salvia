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

#ifndef __DMA__
#define __DMA__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void reset_dma(void);

void DMA_update(int channel);

uint8_t  dmaLoadB(uint8_t cr);
uint16_t dmaLoadW(uint8_t cr);
uint32_t dmaLoadL(uint8_t cr);

void dmaStoreB(uint8_t cr, uint8_t data);
void dmaStoreW(uint8_t cr, uint16_t data);
void dmaStoreL(uint8_t cr, uint32_t data);

int MDFNNGPCDMA_StateAction(void *data, int load, int data_only);

#ifdef __cplusplus
}
#endif

#endif
