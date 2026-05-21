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

/*
 * Sound (SPU) functions — cycle-driven model (port from pcsx_rearmed).
 *
 * The SPU plugin no longer runs in its own thread.  Instead the CPU
 * scheduler (psxBranchTest, set_event) calls back into the plugin at
 * specific cycles via SPUasync / spuUpdate.  This guarantees IRQ
 * delivery at the correct cycle and removes the wait/handshake logic
 * that broke games like Metal Gear Solid.
 */

#include "spu.h"

void CALLBACK SPUirq(int cycles_after) {
	if (cycles_after > 0) {
		/* Schedule the bit-set as a future event.  The PSXINT_SPU_IRQ
		 * handler is spuDelayedIrq() in psxBranchTest. */
		set_event(PSXINT_SPU_IRQ, cycles_after);
		return;
	}

	/* Immediate: SPU is mixing right at the IRQ point. */
#if PCSXR_DIAG_INSTRUMENTATION
	{
		extern volatile uint32_t diag_hw_irq_set_count[11];
		diag_hw_irq_set_count[9]++;  /* bit 9 = SPU IRQ (immediate path) */
	}
#endif
	psxHu32ref_2(0x1070) |= SWAPu32(0x200);
}

void spuDelayedIrq(void) {
	/* PSXINT_SPU_IRQ handler - fires at the cycle scheduled by SPUirq. */
#if PCSXR_DIAG_INSTRUMENTATION
	{
		extern volatile uint32_t diag_hw_irq_set_count[11];
		diag_hw_irq_set_count[9]++;  /* bit 9 = SPU IRQ (delayed path) */
	}
#endif
	psxHu32ref_2(0x1070) |= SWAPu32(0x200);
}

/* PSXINT_SPU_UPDATE handler.  Re-enters the SPU plugin at the current
 * CPU cycle so the plugin can do_samples up to here and re-schedule
 * itself for the next predicted IRQ point.  Called from psxBranchTest
 * when the scheduled cycle arrives. */
void spuUpdate(void) {
	if (SPU_async)
		SPU_async(psxRegs.cycle, 0);
}

/* Registered with the SPU plugin via SPU_registerScheduleCb at boot.
 * The plugin invokes this from schedule_next_irq when it has predicted
 * the next IRQ point N cycles in the future. */
void CALLBACK SPUschedule(unsigned int cycles_after) {
	set_event(PSXINT_SPU_UPDATE, cycles_after);
}
