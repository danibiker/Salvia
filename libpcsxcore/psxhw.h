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

#ifndef __PSXHW_H__
#define __PSXHW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "r3000a.h"
#include "psxmem.h"
#include "sio.h"
#include "psxcounters.h"

#define HW_DMA0_MADR (psxHu32ref(0x1080))    // MDEC in DMA
#define HW_DMA0_MADR_2 (psxHu32ref_2(0x1080))//teste
#define HW_DMA0_BCR  (psxHu32ref(0x1084))
#define HW_DMA0_BCR_2  (psxHu32ref_2(0x1084))//teste
#define HW_DMA0_CHCR (psxHu32ref(0x1088))
#define HW_DMA0_CHCR_2 (psxHu32ref_2(0x1088))//teste


#define HW_DMA1_MADR (psxHu32ref(0x1090))    // MDEC out DMA
#define HW_DMA1_MADR_2 (psxHu32ref_2(0x1090))//teste
#define HW_DMA1_BCR  (psxHu32ref(0x1094))
#define HW_DMA1_BCR_2  (psxHu32ref_2(0x1094))//teste
#define HW_DMA1_CHCR (psxHu32ref(0x1098))
#define HW_DMA1_CHCR_2 (psxHu32ref_2(0x1098))//teste


#define HW_DMA2_MADR (psxHu32ref(0x10a0))    // GPU DMA
#define HW_DMA2_MADR_2 (psxHu32ref_2(0x10a0))//teste
#define HW_DMA2_BCR  (psxHu32ref(0x10a4))
#define HW_DMA2_BCR_2  (psxHu32ref_2(0x10a4))//teste
#define HW_DMA2_CHCR (psxHu32ref(0x10a8))
#define HW_DMA2_CHCR_2 (psxHu32ref_2(0x10a8))//teste


#define HW_DMA3_MADR (psxHu32ref(0x10b0))    // CDROM DMA
#define HW_DMA3_MADR_2 (psxHu32ref_2(0x10b0))//teste
#define HW_DMA3_BCR  (psxHu32ref(0x10b4))
#define HW_DMA3_BCR_2  (psxHu32ref_2(0x10b4))//teste
#define HW_DMA3_CHCR   (psxHu32ref(0x10b8))
#define HW_DMA3_CHCR_2 (psxHu32ref_2(0x10b8))//teste


#define HW_DMA4_MADR (psxHu32ref(0x10c0))    // SPU DMA
#define HW_DMA4_MADR_2 (psxHu32ref_2(0x10c0))//teste
#define HW_DMA4_BCR  (psxHu32ref(0x10c4))
#define HW_DMA4_BCR_2  (psxHu32ref_2(0x10c4))//teste
#define HW_DMA4_CHCR (psxHu32ref(0x10c8))
#define HW_DMA4_CHCR_2 (psxHu32ref_2(0x10c8))//teste


#define HW_DMA6_MADR (psxHu32ref(0x10e0))    // GPU DMA (OT)
#define HW_DMA6_MADR_2 (psxHu32ref_2(0x10e0))//teste
#define HW_DMA6_BCR  (psxHu32ref(0x10e4))
#define HW_DMA6_BCR_2  (psxHu32ref_2(0x10e4))//teste
#define HW_DMA6_CHCR (psxHu32ref(0x10e8))
#define HW_DMA6_CHCR_2 (psxHu32ref_2(0x10e8))//teste


#define HW_DMA_PCR   (psxHu32ref(0x10f0))
#define HW_DMA_PCR_2   (psxHu32ref_2(0x10f0))//teste

#define HW_DMA_ICR     (psxHu32ref(0x10f4))
#define HW_DMA_ICR_2   (psxHu32ref_2(0x10f4))//teste

///////////////////////////////////////////////////////
#define	DMA_INTERRUPT(n) \
	if (SWAPu32(HW_DMA_ICR) & (1 << (16 + n))) {    \
		HW_DMA_ICR |= SWAP32(1 << (24 + n));        \
		psxHu32ref(0x1070) |= SWAP32(8);            \
	}


//#define	DMA_INTERRUPT_2(n) \
//	if (SWAPu32(HW_DMA_ICR_2) & (1 << (16 + n))) {    \
//		HW_DMA_ICR_2 |= SWAP32(1 << (24 + n));        \
//		psxHu32ref_2(0x1070) |= SWAP32(8);            \
//	}

__inline void DMA_INTERRUPT_2(int n){ 
	if (SWAPu32(HW_DMA_ICR_2) & (1 << (16 + n))) {    
		HW_DMA_ICR_2 |= SWAP32(1 << (24 + n));        
		psxHu32ref_2(0x1070) |= SWAP32(8);            
	}
}

///////////////////////////////////////////////////////teste
void psxHwReset();
u8 psxHwRead8(u32 add);
u16 psxHwRead16(u32 add);
u32 psxHwRead32(u32 add);
void psxHwWrite8(u32 add, u8  value);
void psxHwWrite16(u32 add, u16 value);
void psxHwWrite32(u32 add, u32 value);
int psxHwFreeze(gzFile f, int Mode);



/******************************************************************** 
*	IO MAPPING
********************************************************************/
typedef u8	(*hw_read8_t)	(u32 add); 
typedef u16	(*hw_read16_t)	(u32 add); 
typedef u32	(*hw_read32_t)	(u32 add); 

typedef void	(*hw_write8_t)	(u32 add, u8 value); 
typedef void	(*hw_write16_t)	(u32 add, u16 value); 
typedef void	(*hw_write32_t)	(u32 add, u32 value); 

extern hw_read8_t *		hw_read8_handler;
extern hw_read16_t *	hw_read16_handler;
extern hw_read32_t *	hw_read32_handler;

extern hw_write8_t *	hw_write8_handler;
extern hw_write16_t *	hw_write16_handler;
extern hw_write32_t *	hw_write32_handler;

#ifdef __cplusplus
}
#endif
#endif
