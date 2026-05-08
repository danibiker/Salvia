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

#ifndef __SPU_H__
#define __SPU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "plugins.h"
#include "r3000a.h"
#include "psxmem.h"

//#define H_SPUirqAddr     0x0da4
//#define H_SPUaddr        0x0da6
#define H_SPUdata        0x0da8
//#define H_SPUctrl        0x0daa
#define H_SPUstat        0x0dae
#define H_SPUon1         0x0d88
#define H_SPUon2         0x0d8a
#define H_SPUoff1        0x0d8c
#define H_SPUoff2        0x0d8e

/* SPU IRQ callback (port from pcsx_rearmed cycle-driven SPU model).
 *
 * Old signature: void SPUirq(void)
 *   - The SPU plugin (running on its own thread) called this directly.
 *   - It set the IRQ pending bit in psxHu32(0x1070) immediately.
 *   - This caused cross-thread visibility issues and an inability to
 *     deliver IRQs at the correct cycle (Metal Gear Solid was the
 *     canonical victim).
 *
 * New signature: void SPUirq(int cycles_after)
 *   - Called by the SPU plugin from inside SPU_async / do_samples.
 *   - If cycles_after > 0, schedules the bit-set as a future
 *     PSXINT_SPU_IRQ event so it lands at the correct PSX cycle.
 *   - If cycles_after == 0, sets the bit immediately (the most common
 *     case: the IRQ point matches the cycle the SPU is about to mix).
 *
 * spuDelayedIrq is the PSXINT_SPU_IRQ handler invoked by psxBranchTest
 * when the scheduled cycle arrives — sets the bit then.
 *
 * spuUpdate is the PSXINT_SPU_UPDATE handler — re-enters the SPU plugin
 * via SPU_async at the cycle scheduled by schedule_next_irq. */
void CALLBACK SPUirq(int cycles_after);
void spuDelayedIrq(void);
void spuUpdate(void);

/* Schedule callback registered with the SPU plugin via
 * SPU_registerScheduleCb. The plugin calls this to ask the CPU
 * scheduler to re-enter SPU_async at exactly cycles_after future
 * cycles (used by schedule_next_irq to predict the next IRQ point). */
void CALLBACK SPUschedule(unsigned int cycles_after);

#ifdef __cplusplus
}
#endif
#endif
