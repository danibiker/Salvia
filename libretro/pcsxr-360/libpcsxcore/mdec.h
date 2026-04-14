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

#ifndef __MDEC_H__
#define __MDEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "r3000a.h"
#include "psxhw.h"
#include "psxdma.h"

#define	MDEC_END_OF_DATA	0xfe00

// mdec0: command register
#define MDEC0_STP			0x02000000
#define MDEC0_RGB24			0x08000000
#define MDEC0_SIZE_MASK		0x0000FFFF

// mdec1: status register
#define MDEC1_BUSY			0x20000000
#define MDEC1_DREQ			0x18000000
#define MDEC1_FIFO			0xc0000000
#define MDEC1_RGB24			0x02000000
#define MDEC1_STP			0x00800000
#define MDEC1_RESET			0x80000000

struct _pending_dma1 {
	u32 adr;
	u32 bcr;
	u32 chcr;
};

static struct {
	u32 reg0;
	u32 reg1;
	u16 * rl;
	u16 * rl_end;
	u8 * block_buffer_pos;
	u8 block_buffer[16*16*3];
	struct _pending_dma1 pending_dma1;
} mdec;

#define mdec1func() \
mdec.reg1 &= ~MDEC1_STP; \
HW_DMA0_CHCR_2 &= SWAP32(~0x01000000); \
DMA_INTERRUPT_2(0); \
mdec.reg1 &= ~MDEC1_BUSY; 


void mdecInit();
void mdecWrite0(u32 data);
void mdecWrite1(u32 data);
u32 mdecRead0();
u32 mdecRead1();
void psxDma0(u32 madr, u32 bcr, u32 chcr);
void psxDma1(u32 madr, u32 bcr, u32 chcr);
void mdec1Interrupt();
int mdecFreeze(gzFile f, int Mode);


#ifdef __cplusplus
}
#endif
#endif
