/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#ifndef __PSXDMA_H__
#define __PSXDMA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "r3000a.h"
#include "psxhw.h"
#include "psxmem.h"

/*
DMA5 = N/A (PIO)
*/

void psxDma3(u32 madr, u32 bcr, u32 chcr);
void psxDma4(u32 madr, u32 bcr, u32 chcr);
void psxDma6(u32 madr, u32 bcr, u32 chcr);

//void spuInterrupt();
//void mdec0Interrupt();
//void gpuotcInterrupt();
//void cdrDmaInterrupt();

#define mdec0Interrupt() \
HW_DMA0_CHCR_2 &= SWAP32(~0x01000000); \
DMA_INTERRUPT_2(0);

#define gpuotcInterrupt() \
HW_DMA6_CHCR_2 &= SWAP32(~0x01000000); \
DMA_INTERRUPT_2(6);

#define cdrDmaInterrupt() \
HW_DMA3_CHCR_2 &= SWAP32(~0x01000000); \
DMA_INTERRUPT_2(3);

#define spuInterrupt() \
HW_DMA4_CHCR_2 &= SWAP32(~0x01000000); \
DMA_INTERRUPT_2(4);

#ifdef __cplusplus
}
#endif
#endif
