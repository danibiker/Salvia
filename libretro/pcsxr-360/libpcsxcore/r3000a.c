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
* R3000A CPU functions.
*/
#include "r3000a.h"
#include "cdrom.h"
#include "mdec.h"
#include "gpu.h"
#include "gte.h"
#include "sio.h"
#include "psxdma.h"

boolean use_vm;
// extern boolean use_vm on psxcommon.h

R3000Acpu *psxCpu = NULL;
psxRegisters psxRegs;


int psxInit() {

	SysPrintf(_("Running PCSX Version %s (%s).\n"), PACKAGE_VERSION, __DATE__);

#ifdef PSXREC
	if (Config.Cpu == CPU_INTERPRETER) {
		psxCpu = &psxInt;
	} else psxCpu = &psxRec;
#else
	psxCpu = &psxInt;
#endif

	Log = 0;
//if (use_vm)
//{
//	if (psxMemInit() == -1) return -1;
//}
//else
//{
	if (psxMemInit_2() == -1) return -1;//teste
//}

	return psxCpu->Init();
}

void psxReset() {
	psxCpu->Reset();
//if (use_vm)
//{
//	psxMemReset();
//}
//else
//{
	psxMemReset_2();//teste
//}

	memset(&psxRegs, 0, sizeof(psxRegs));

	psxRegs.pc = 0xbfc00000; // Start in bootstrap
	psxRegs.next_interupt = 0;

	psxRegs.CP0.r[12] = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	psxRegs.CP0.r[15] = 0x00000002; // PRevID = Revision ID, same as R3000A

	psxHwReset();
//if(use_vm){
//	psxBiosInit();}
//else{
	psxBiosInit_2();//}//teste

	if (!Config.HLE)
		psxExecuteBios();

#ifdef EMU_LOG
	EMU_LOG("*BIOS END*\n");
#endif
	Log = 0;
}

void psxShutdown() {

//if(use_vm){
//	psxMemShutdown();}
//else{
	psxMemShutdown_2();//}//teste

	psxBiosShutdown();

	psxCpu->Shutdown();
}

void psxException(u32 code, u32 bd) {
	// Set the Cause
	psxRegs.CP0.n.Cause = code;

	// Set the EPC & PC
	if (bd) {
#ifdef PSXCPU_LOG
		PSXCPU_LOG("bd set!!!\n");
#endif
		SysPrintf("bd set!!!\n");
		psxRegs.CP0.n.Cause |= 0x80000000;
		psxRegs.CP0.n.EPC = (psxRegs.pc - 4);
	} else
		psxRegs.CP0.n.EPC = (psxRegs.pc);

	if (psxRegs.CP0.n.Status & 0x400000)
		psxRegs.pc = 0xbfc00180;
	else
		psxRegs.pc = 0x80000080;

	// Set the Status
	psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status &~0x3f) |
						  ((psxRegs.CP0.n.Status & 0xf) << 2);

	if (Config.HLE) psxBiosException();
}

void schedule_timeslice(void) {
	u32 i, c = psxRegs.cycle;
	u32 irqs = psxRegs.interrupt;
	s32 min, dif;

	// Start with next counter event
	min = (s32)(psxNextsCounter + psxNextCounter - c);
	if (min < 0) min = 0;

	for (i = 0; irqs != 0; i++, irqs >>= 1) {
		if (!(irqs & 1))
			continue;
		dif = (s32)(psxRegs.intCycle[i].sCycle + psxRegs.intCycle[i].cycle - c);
		if (dif < min) {
			if (dif < 0) { min = 0; break; }
			min = dif;
		}
	}
	psxRegs.next_interupt = c + min;
}

void psxBranchTest() {
	// Event processing gated by next_interupt (fast path optimization)
	if ((s32)(psxRegs.cycle - psxRegs.next_interupt) >= 0) {
		if ((psxRegs.cycle - psxNextsCounter) >= psxNextCounter)
			psxRcntUpdate();

		if (psxRegs.interrupt) {
			u32 irq, irq_bits;
			for (irq = 0, irq_bits = psxRegs.interrupt; irq_bits != 0; irq++, irq_bits >>= 1) {
				if (!(irq_bits & 1))
					continue;
				if ((psxRegs.cycle - psxRegs.intCycle[irq].sCycle) >= psxRegs.intCycle[irq].cycle) {
					psxRegs.interrupt &= ~(1u << irq);
					switch (irq) {
						case PSXINT_SIO: if (!Config.Sio) sioInterrupt(); break;
						case PSXINT_CDR: cdrInterrupt(); break;
						case PSXINT_CDREAD: cdrReadInterrupt(); break;
						case PSXINT_GPUDMA: gpuInterrupt(); break;
						case PSXINT_MDECOUTDMA: mdec1Interrupt(); break;
						case PSXINT_SPUDMA: spuInterrupt(); break;
						case PSXINT_MDECINDMA: mdec0Interrupt(); break;
						case PSXINT_GPUOTCDMA: gpuotcInterrupt(); break;
						case PSXINT_CDRDMA: cdrDmaInterrupt(); break;
						case PSXINT_CDRPLAY: cdrPlayInterrupt(); break;
						case PSXINT_CDRDBUF: cdrDecodedBufferInterrupt(); break;
						case PSXINT_CDRLID: cdrLidSeekInterrupt(); break;
					}
				}
			}
		}

		schedule_timeslice();
	}

	// Hardware interrupt check - ALWAYS runs (events above may set 0x1070)
	if (psxHu32_2(0x1070) & psxHu32_2(0x1074)) {
		if ((psxRegs.CP0.n.Status & 0x401) == 0x401) {
			u32 opcode;
			u32 *code;
			code = (u32 *)PSXM_2(psxRegs.pc);
			// Crash Bandicoot 2: Don't run exceptions when GTE in pipeline
			opcode = SWAP32(*code);
			if (((opcode >> 24) & 0xfe) != 0x4a) {
#ifdef PSXCPU_LOG
				PSXCPU_LOG("Interrupt: %x %x\n", psxHu32_2(0x1070), psxHu32_2(0x1074));
#endif
				psxException(0x400, 0);
			}
		}
	}
}

void psxJumpTest() {
	if (!Config.HLE && Config.PsxOut) {
		u32 call = psxRegs.GPR.n.t1 & 0xff;
		switch (psxRegs.pc & 0x1fffff) {
			case 0xa0:
#ifdef PSXBIOS_LOG
				if (call != 0x28 && call != 0xe) {
					PSXBIOS_LOG("Bios call a0: %s (%x) %x,%x,%x,%x\n", biosA0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3); }
#endif
				if (biosA0[call])
					biosA0[call]();
				break;
			case 0xb0:
#ifdef PSXBIOS_LOG
				if (call != 0x17 && call != 0xb) {
					PSXBIOS_LOG("Bios call b0: %s (%x) %x,%x,%x,%x\n", biosB0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3); }
#endif
				if (biosB0[call])
					biosB0[call]();
				break;
			case 0xc0:
#ifdef PSXBIOS_LOG
				PSXBIOS_LOG("Bios call c0: %s (%x) %x,%x,%x,%x\n", biosC0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3);
#endif
				if (biosC0[call])
					biosC0[call]();
				break;
		}
	}
}

void psxExecuteBios() {
	while (psxRegs.pc != 0x80030000)
		psxCpu->ExecuteBlock();
}
