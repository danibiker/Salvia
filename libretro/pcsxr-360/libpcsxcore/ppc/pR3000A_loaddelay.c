/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2003  Pcsx Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include <time.h>

#include "ppc.h"
#include "reguse.h"
#include "../r3000a.h"
#include "../psxhle.h"
#include "../../360/Xdk/pcsxr/bram.h"

#define malloc balloc
#define free bfree

void __declspec(naked) __icbi(int offset, const void * base)
{
	__asm {
        icbi r3,r4
        blr
    }
}

u32 *psxRecLUT;

#undef _Op_
#define _Op_     _fOp_(psxRegs.code)
#undef _Funct_
#define _Funct_  _fFunct_(psxRegs.code)
#undef _Rd_
#define _Rd_     _fRd_(psxRegs.code)
#undef _Rt_
#define _Rt_     _fRt_(psxRegs.code)
#undef _Rs_
#define _Rs_     _fRs_(psxRegs.code)
#undef _Sa_
#define _Sa_     _fSa_(psxRegs.code)
#undef _Im_
#define _Im_     _fIm_(psxRegs.code)
#undef _Target_
#define _Target_ _fTarget_(psxRegs.code)

#undef _Imm_
#define _Imm_	 _fImm_(psxRegs.code)
#undef _ImmU_
#define _ImmU_	 _fImmU_(psxRegs.code)

#undef PC_REC
#undef PC_REC8
#undef PC_REC16
#undef PC_REC32
#define PC_REC(x)	(psxRecLUT[x >> 16] + (x & 0xffff))
#define PC_REC8(x)	(*(u8 *)PC_REC(x))
#define PC_REC16(x) (*(u16*)PC_REC(x))
#define PC_REC32(x) (*(u32*)PC_REC(x))

#define OFFSET(X,Y) ((u32)(Y)-(u32)(X))

#define RECMEM_SIZE		(12*1024*1024)

static char *recMem;	/* the recompiled blocks will be here */
static char *recRAM;	/* and the ptr to the blocks here */
static char *recROM;	/* and here */

static u32 pc;			/* recompiler pc */
static u32 pcold;		/* recompiler oldpc */
static int count;		/* recompiler intruction count */
static int branch;		/* set for branch */
static u32 target;		/* branch target */
static u32 resp;

u32 cop2readypc = 0;
u32 idlecyclecount = 0;

#define NUM_REGISTERS	34
typedef struct {
	int state;
	u32 k;
	int reg;
} iRegisters;

static iRegisters iRegs[34];

#define ST_UNK      0x00
#define ST_CONST    0x01
#define ST_MAPPED   0x02

#ifdef NO_CONSTANT
#define IsConst(reg) 0
#else
#define IsConst(reg)  (iRegs[reg].state & ST_CONST)
#endif
#define IsMapped(reg) (iRegs[reg].state & ST_MAPPED)

static void (*recBSC[64])();
static void (*recSPC[64])();
static void (*recREG[32])();
static void (*recCP0[32])();
static void (*recCP2[64])();
static void (*recCP2BSC[32])();

/* ===== MIPS R3000A load-delay-slot emulation =====
 *
 * Soul Reaver / Soul Calibur (and other tight PSX code) rely on the
 * R3000A's 1-cycle load delay: the instruction immediately following
 * a `lw/lh/lb/lwc2/...` reads the OLD value of the load destination
 * register, not the just-loaded one. We use pcsx_rearmed's 2-slot
 * pending-load buffer scheme.
 *
 * Compile-time tracking:
 *   `dl_reg[i]` records, while a block is being recompiled, which MIPS
 *   GPR has a pending load queued in psxRegs.dloadVal[i]. 0 means the
 *   slot is empty. `dl_sel` mirrors pcsx_rearmed's dloadSel — it
 *   toggles every MIPS instruction to alternate which slot belongs to
 *   which instruction's delay window.
 *
 * Runtime state:
 *   psxRegs.dloadVal[2] — values waiting to be committed.
 *
 * This system handles loads correctly only INSIDE a recompiled block.
 * At block boundaries (branches, jumps, exceptions, returns) we emit
 * a flush that commits both slots, so a block always starts with a
 * clean buffer.
 */
#define DLOAD_PORT 1

#if DLOAD_PORT
static u32 dl_reg[2] = {0, 0};
static int dl_sel = 0;

/* Runtime toggle (libretro variable `pcsxr360_load_delay`). When 0 the
 * dynarec behaves identically to the pre-DLOAD_PORT path (no deferral,
 * no peephole, no per-instruction step/kill emission). When 1 the
 * peephole kicks in: loads only defer when the immediately-following
 * MIPS instruction reads the load destination. Toggling at runtime
 * REQUIRES recReset() since compiled blocks bake the choice in. */
int dload_enabled = 1;
#endif

#define REG_LO			32
#define REG_HI			33

// Hardware register usage
#define HWUSAGE_NONE     0x00

#define HWUSAGE_READ     0x01
#define HWUSAGE_WRITE    0x02
#define HWUSAGE_CONST    0x04
#define HWUSAGE_ARG      0x08	/* used as an argument for a function call */

#define HWUSAGE_RESERVED 0x10	/* won't get flushed when flushing all regs */
#define HWUSAGE_SPECIAL  0x20	/* special purpose register */
#define HWUSAGE_HARDWIRED 0x40	/* specific hardware register mapping that is never disposed */
#define HWUSAGE_INITED    0x80
#define HWUSAGE_PSXREG    0x100

// Remember to invalidate the special registers if they are modified by compiler
enum {
    ARG1 = 3,
    ARG2 = 4,
    ARG3 = 5,
    PSXREGS,	// ptr
	PSXMEM,		// ptr
    CYCLECOUNT,	// ptr
    PSXPC,	// ptr
    TARGETPTR,	// ptr
    TARGET,	// ptr
    RETVAL,
    REG_RZERO,
    REG_WZERO
};

typedef struct {
    int code;
    u32 k;
    int usage;
    int lastUsed;
    
    void (*flush)(int hwreg);
    int private;
} HWRegister;
static HWRegister HWRegisters[NUM_HW_REGISTERS];
static int HWRegUseCount;
static int DstCPUReg;
static int UniqueRegAlloc;

static int GetFreeHWReg();
static void InvalidateCPURegs();
static void DisposeHWReg(int index);
static void FlushHWReg(int index);
static void FlushAllHWReg();
static void MapPsxReg32(int reg);
static void FlushPsxReg32(int hwreg);
static int UpdateHWRegUsage(int hwreg, int usage);
static int GetHWReg32(int reg);
static int PutHWReg32(int reg);
static int GetSpecialIndexFromHWRegs(int which);
static int GetHWRegFromCPUReg(int cpureg);
static int MapRegSpecial(int which);
static void FlushRegSpecial(int hwreg);
static int GetHWRegSpecial(int which);
static int PutHWRegSpecial(int which);
static void recRecompile();
static void recError();

#pragma mark --- Generic register mapping ---

static int GetFreeHWReg()
{ 
	int i, least, index;

	if (DstCPUReg != -1) {
		index = GetHWRegFromCPUReg(DstCPUReg);
		DstCPUReg = -1;
	} else {
		// LRU algorith with a twist ;)
		for (i=0; i<NUM_HW_REGISTERS; i++) {
			if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
				break;
			}
		}

		least = HWRegisters[i].lastUsed; index = i;
		for (; i<NUM_HW_REGISTERS; i++) {
			if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
				if (HWRegisters[i].usage == HWUSAGE_NONE && HWRegisters[i].code >= 13) {
					index = i;
					break;
				}
				else if (HWRegisters[i].lastUsed < least) {
					least = HWRegisters[i].lastUsed;
					index = i;
				}
			}
		}

		// Cycle the registers
		if (HWRegisters[index].usage == HWUSAGE_NONE) {
			for (; i<NUM_HW_REGISTERS; i++) {
				if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
					if (HWRegisters[i].usage == HWUSAGE_NONE && 
						HWRegisters[i].code >= 13 && 
						HWRegisters[i].lastUsed < least) {
							least = HWRegisters[i].lastUsed;
							index = i;
							break;
					}
				}
			}
		}
	}

	if (HWRegisters[index].usage & (HWUSAGE_RESERVED | HWUSAGE_HARDWIRED)) {
		if (HWRegisters[index].usage & HWUSAGE_RESERVED) {
			SysPrintf("Error! Trying to map a new register to a reserved register (r%i)", 
				HWRegisters[index].code);
		}
		if (HWRegisters[index].usage & HWUSAGE_HARDWIRED) {
			SysPrintf("! Trying to map a new register to a hardwired register (r%i)", 
				HWRegisters[index].code);
		}
	}

	if (HWRegisters[index].lastUsed != 0) {
		UniqueRegAlloc = 0;
	}

	// Make sure the register is really flushed!
	FlushHWReg(index);
	HWRegisters[index].usage = HWUSAGE_NONE;
	HWRegisters[index].flush = NULL;

	return index;
}

static void FlushHWReg(int index)
{
	if (index < 0) return;
	if (HWRegisters[index].usage == HWUSAGE_NONE) return;

	if (HWRegisters[index].flush) {
		HWRegisters[index].usage |= HWUSAGE_RESERVED;
		HWRegisters[index].flush(index);
		HWRegisters[index].flush = NULL;
	}

	if (HWRegisters[index].usage & HWUSAGE_HARDWIRED) {
		HWRegisters[index].usage &= ~(HWUSAGE_READ | HWUSAGE_WRITE);
	} else {
		HWRegisters[index].usage = HWUSAGE_NONE;
	}
}

// get rid of a mapped register without flushing the contents to the memory
static void DisposeHWReg(int index)
{
	if (index < 0) return;
	if (HWRegisters[index].usage == HWUSAGE_NONE) return;

	HWRegisters[index].usage &= ~(HWUSAGE_READ | HWUSAGE_WRITE);
	if (HWRegisters[index].usage == HWUSAGE_NONE) {
		SysPrintf("Error! not correctly disposing register (r%i)", HWRegisters[index].code);
	}

	FlushHWReg(index);
}

// operated on cpu registers
__inline static void FlushCPURegRange(int start, int end)
{
	int i;

	if (end <= 0) end = 31;
	if (start <= 0) start = 0;

	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].code >= start && HWRegisters[i].code <= end)
			if (HWRegisters[i].flush)
				FlushHWReg(i);
	}

	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].code >= start && HWRegisters[i].code <= end)
			FlushHWReg(i);
	}
}

static void FlushAllHWReg()
{
	FlushCPURegRange(0,31);
}

static void InvalidateCPURegs()
{
	FlushCPURegRange(0,12);
}

static void MoveHWRegToCPUReg(int cpureg, int hwreg)
{
	int dstreg;

	if (HWRegisters[hwreg].code == cpureg)
		return;

	dstreg = GetHWRegFromCPUReg(cpureg);

	HWRegisters[dstreg].usage &= ~(HWUSAGE_HARDWIRED | HWUSAGE_ARG);
	if (HWRegisters[hwreg].usage & (HWUSAGE_READ | HWUSAGE_WRITE)) {
		FlushHWReg(dstreg);
		MR(HWRegisters[dstreg].code, HWRegisters[hwreg].code);
	} else {
		if (HWRegisters[dstreg].usage & (HWUSAGE_READ | HWUSAGE_WRITE)) {
			MR(HWRegisters[hwreg].code, HWRegisters[dstreg].code);
		}
		else if (HWRegisters[dstreg].usage != HWUSAGE_NONE) {
			FlushHWReg(dstreg);
		}
	}

	HWRegisters[dstreg].code = HWRegisters[hwreg].code;
	HWRegisters[hwreg].code = cpureg;
}

static int UpdateHWRegUsage(int hwreg, int usage)
{
	HWRegisters[hwreg].lastUsed = ++HWRegUseCount;    
	if (usage & HWUSAGE_WRITE) {
		HWRegisters[hwreg].usage &= ~HWUSAGE_CONST;
	}
	if (!(usage & HWUSAGE_INITED)) {
		HWRegisters[hwreg].usage &= ~HWUSAGE_INITED;
	}
	HWRegisters[hwreg].usage |= usage;

	return HWRegisters[hwreg].code;
}

static int GetHWRegFromCPUReg(int cpureg)
{
	int i;
	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].code == cpureg) {
			return i;
		}
	}

	SysPrintf("Error! Register location failure (r%i)", cpureg);
	return 0;
}

// this function operates on cpu registers
void SetDstCPUReg(int cpureg)
{
	DstCPUReg = cpureg;
}

static void ReserveArgs(int args)
{
	int index, i;

	for (i=0; i<args; i++) {
		index = GetHWRegFromCPUReg(3+i);
		HWRegisters[index].usage |= HWUSAGE_RESERVED | HWUSAGE_HARDWIRED | HWUSAGE_ARG;
	}
}

static void ReleaseArgs()
{
	int i;

	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].usage & HWUSAGE_ARG) {
			HWRegisters[i].usage &= ~(HWUSAGE_RESERVED | HWUSAGE_HARDWIRED | HWUSAGE_ARG);
			FlushHWReg(i);
		}
	}
}

static void MapPsxReg32(int reg)
{
	int hwreg = GetFreeHWReg();
	HWRegisters[hwreg].flush = FlushPsxReg32;
	HWRegisters[hwreg].private = reg;

	if (iRegs[reg].reg != -1) {
		SysPrintf("error: double mapped psx register");
	}

	iRegs[reg].reg = hwreg;
	iRegs[reg].state |= ST_MAPPED;
}

static void FlushPsxReg32(int hwreg)
{
	int reg = HWRegisters[hwreg].private;

	if (iRegs[reg].reg == -1) {
		SysPrintf("error: flushing unmapped psx register");
	}

	if (HWRegisters[hwreg].usage & HWUSAGE_WRITE) {
		if (branch) {
		{
				STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
		}
		} else {
			int reguse = nextPsxRegUse(pc-4, reg);
			if (reguse == REGUSE_NONE || (reguse & REGUSE_READ)) {
				STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
			}
		}
	}

	iRegs[reg].reg = -1;
	iRegs[reg].state = ST_UNK;
}

static int GetHWReg32(int reg)
{
	int usage = HWUSAGE_PSXREG | HWUSAGE_READ;

	if (reg == 0) {
		return GetHWRegSpecial(REG_RZERO);
	}
	if (!IsMapped(reg)) {
		usage |= HWUSAGE_INITED;
		MapPsxReg32(reg);

		HWRegisters[iRegs[reg].reg].usage |= HWUSAGE_RESERVED;
		if (IsConst(reg)) {
			LIW(HWRegisters[iRegs[reg].reg].code, iRegs[reg].k);
			usage |= HWUSAGE_WRITE | HWUSAGE_CONST;
			//iRegs[reg].state &= ~ST_CONST;
		}
		else {
			LWZ(HWRegisters[iRegs[reg].reg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
		}
		HWRegisters[iRegs[reg].reg].usage &= ~HWUSAGE_RESERVED;
	}
	else if (DstCPUReg != -1) {
		int dst = DstCPUReg;
		DstCPUReg = -1;

		if (HWRegisters[iRegs[reg].reg].code < 13) {
			MoveHWRegToCPUReg(dst, iRegs[reg].reg);
		} else {
			MR(DstCPUReg, HWRegisters[iRegs[reg].reg].code);
		}
	}

	DstCPUReg = -1;

	return UpdateHWRegUsage(iRegs[reg].reg, usage);
}

static int PutHWReg32(int reg)
{
	int usage = HWUSAGE_PSXREG | HWUSAGE_WRITE;
	if (reg == 0) {
		return PutHWRegSpecial(REG_WZERO);
	}

	if (DstCPUReg != -1 && IsMapped(reg)) {
		if (HWRegisters[iRegs[reg].reg].code != DstCPUReg) {
			int tmp = DstCPUReg;
			DstCPUReg = -1;
			DisposeHWReg(iRegs[reg].reg);
			DstCPUReg = tmp;
		}
	}
	if (!IsMapped(reg)) {
		usage |= HWUSAGE_INITED;
		MapPsxReg32(reg);
	}

	DstCPUReg = -1;
	iRegs[reg].state &= ~ST_CONST;

	return UpdateHWRegUsage(iRegs[reg].reg, usage);
}

static int GetSpecialIndexFromHWRegs(int which)
{
	int i;
	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].usage & HWUSAGE_SPECIAL) {
			if (HWRegisters[i].private == which) {
				return i;
			}
		}
	}
	return -1;
}

static int MapRegSpecial(int which)
{
	int hwreg = GetFreeHWReg();
	HWRegisters[hwreg].flush = FlushRegSpecial;
	HWRegisters[hwreg].private = which;

	return hwreg;
}

static void FlushRegSpecial(int hwreg)
{
	int which = HWRegisters[hwreg].private;

	if (!(HWRegisters[hwreg].usage & HWUSAGE_WRITE))
		return;

	switch (which) {
	case CYCLECOUNT:
		STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.cycle), GetHWRegSpecial(PSXREGS));
		break;
	case PSXPC:
		STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.pc), GetHWRegSpecial(PSXREGS));
		break;
	case TARGET:
		STW(HWRegisters[hwreg].code, 0, GetHWRegSpecial(TARGETPTR));
		break;
	}
}

static int GetHWRegSpecial(int which)
{
	int index = GetSpecialIndexFromHWRegs(which);
	int usage = HWUSAGE_READ | HWUSAGE_SPECIAL;

	if (index == -1) {
		usage |= HWUSAGE_INITED;
		index = MapRegSpecial(which);

		HWRegisters[index].usage |= HWUSAGE_RESERVED;
		switch (which) {
		case PSXREGS:
		case PSXMEM:
			SysPrintf("error! shouldn't be here!\n");
			break;
		case TARGETPTR:
			HWRegisters[index].flush = NULL;
			LIW(HWRegisters[index].code, (u32)&target);
			break;
		case REG_RZERO:
			HWRegisters[index].flush = NULL;
			LIW(HWRegisters[index].code, 0);
			break;
		case RETVAL:
			MoveHWRegToCPUReg(3, index);
			HWRegisters[index].flush = NULL;

			usage |= HWUSAGE_RESERVED;
			break;

		case CYCLECOUNT:
			LWZ(HWRegisters[index].code, OFFSET(&psxRegs, &psxRegs.cycle), GetHWRegSpecial(PSXREGS));
			break;
		case PSXPC:
			LWZ(HWRegisters[index].code, OFFSET(&psxRegs, &psxRegs.pc), GetHWRegSpecial(PSXREGS));
			break;
		case TARGET:
			LWZ(HWRegisters[index].code, 0, GetHWRegSpecial(TARGETPTR));
			break;
		default:
			SysPrintf("Error: Unknown special register in GetHWRegSpecial()\n");
			break;
		}
		HWRegisters[index].usage &= ~HWUSAGE_RESERVED;
	}
	else if (DstCPUReg != -1) {
		int dst = DstCPUReg;
		DstCPUReg = -1;

		MoveHWRegToCPUReg(dst, index);
	}

	return UpdateHWRegUsage(index, usage);
}

static int PutHWRegSpecial(int which)
{
	int index = GetSpecialIndexFromHWRegs(which);
	int usage = HWUSAGE_WRITE | HWUSAGE_SPECIAL;

	if (DstCPUReg != -1 && index != -1) {
		if (HWRegisters[index].code != DstCPUReg) {
			int tmp = DstCPUReg;
			DstCPUReg = -1;
			DisposeHWReg(index);
			DstCPUReg = tmp;
		}
	}
	switch (which) {
	case PSXREGS:
	case TARGETPTR:
		SysPrintf("Error: Read-only special register in PutHWRegSpecial()\n");
	case REG_WZERO:
		if (index >= 0) {
			if (HWRegisters[index].usage & HWUSAGE_WRITE)
				break;
		}
		index = MapRegSpecial(which);
		HWRegisters[index].flush = NULL;
		break;
	default:
		if (index == -1) {
			usage |= HWUSAGE_INITED;
			index = MapRegSpecial(which);

			HWRegisters[index].usage |= HWUSAGE_RESERVED;
			switch (which) {
			case ARG1:
			case ARG2:
			case ARG3:
				MoveHWRegToCPUReg(3+(which-ARG1), index);
				HWRegisters[index].flush = NULL;

				usage |= HWUSAGE_RESERVED | HWUSAGE_HARDWIRED | HWUSAGE_ARG;
				break;
			}
		}
		HWRegisters[index].usage &= ~HWUSAGE_RESERVED;
		break;
	}

	DstCPUReg = -1;

	return UpdateHWRegUsage(index, usage);
}

static void MapConst(int reg, u32 _const) {
	if (reg == 0)
		return;
	if (IsConst(reg) && iRegs[reg].k == _const)
		return;

	DisposeHWReg(iRegs[reg].reg);
	iRegs[reg].k = _const;
	iRegs[reg].state = ST_CONST;
}

static void MapCopy(int dst, int src)
{
	// do it the lazy way for now
	MR(PutHWReg32(dst), GetHWReg32(src));
}

static void iFlushReg(u32 nextpc, int reg) {
	if (!IsMapped(reg) && IsConst(reg)) {
		GetHWReg32(reg);
	}
	if (IsMapped(reg)) {
		if (nextpc) {
			int use = nextPsxRegUse(nextpc, reg);
			if ((use & REGUSE_RW) == REGUSE_WRITE) {
				DisposeHWReg(iRegs[reg].reg);
			} else {
				FlushHWReg(iRegs[reg].reg);
			}
		} else {
			FlushHWReg(iRegs[reg].reg);
		}
	}
}

static void iFlushRegs(u32 nextpc) {
	int i;

	for (i=1; i<NUM_REGISTERS; i++) {
		iFlushReg(nextpc, i);
	}
}

#if DLOAD_PORT
/* ===== Load-delay-slot dynarec emitters =====
 *
 * Emit PPC code that mirrors pcsx_rearmed's dloadStep / dloadFlush via
 * a compile-time-tracked 2-slot buffer. All bookkeeping pcsx_rearmed
 * does at runtime in C (sel, kill on conflict, etc.) is done here in
 * static variables; only the actual data movement (loaded value -> GPR
 * shadow) lives at runtime as PPC instructions.
 */

/* Drop the dynarec's HW-reg cache for a MIPS GPR after we (about to)
 * change psxRegs.GPR.r[reg] in memory. We can't use iFlushReg here:
 * FlushPsxReg32 consults nextPsxRegUse(pc-4, reg) and concludes "the
 * upcoming LW will overwrite reg, no need to flush" — silently
 * skipping the STW. That was fine pre-DLOAD_PORT (load committed
 * immediately); under load-delay we need the OLD value of `reg` to
 * remain in memory so the delay slot's read picks it up. We flush
 * unconditionally. */
static void dl_invalidate_iregs(int reg)
{
	if (reg <= 0) return;

	if (IsMapped(reg)) {
		int hwreg = iRegs[reg].reg;
		if (HWRegisters[hwreg].usage & HWUSAGE_WRITE) {
			STW(HWRegisters[hwreg].code,
			    OFFSET(&psxRegs, &psxRegs.GPR.r[reg]),
			    GetHWRegSpecial(PSXREGS));
		}
		HWRegisters[hwreg].usage = HWUSAGE_NONE;
		HWRegisters[hwreg].flush = NULL;
		iRegs[reg].reg = -1;
	} else if (IsConst(reg)) {
		LIW(0, iRegs[reg].k);
		STW(0, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]),
		    GetHWRegSpecial(PSXREGS));
	}
	iRegs[reg].state = ST_UNK;
}

/* Emit code that commits dl_reg[slot] to its destination GPR shadow,
 * if any. After this returns, dl_reg[slot] is cleared at compile time. */
static void emit_dload_commit_slot(int slot)
{
	u32 reg = dl_reg[slot];
	if (reg == 0) return;

	LWZ(0, OFFSET(&psxRegs, &psxRegs.dloadVal[slot]),
	    GetHWRegSpecial(PSXREGS));
	STW(0, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]),
	    GetHWRegSpecial(PSXREGS));

	dl_invalidate_iregs(reg);
	dl_reg[slot] = 0;
}

/* Mirrors pcsx_rearmed's dloadStep: commit the slot pointed to by
 * dl_sel, then toggle. Called at the start of every recompiled MIPS
 * instruction. When dload_enabled is 0 this is a no-op — dl_reg/dl_sel
 * stay zeroed because the load emitters skip dl_record_load. */
static void emit_dload_step(void)
{
	if (!dload_enabled) return;
	emit_dload_commit_slot(dl_sel);
	dl_sel ^= 1;
}

/* Commit both pending slots and reset the compile-time tracker. Used
 * at any block-exiting boundary (branch, jump, exception, return). */
static void emit_dload_flush(void)
{
	if (!dload_enabled) return;
	emit_dload_commit_slot(0);
	emit_dload_commit_slot(1);
	dl_sel = 0;
}

/* Called by load emitters to record that the value just stored to
 * psxRegs.dloadVal[slot] should be committed to GPR `reg` later.
 *
 * NOTE: pcsx_rearmed drops dl_reg[slot ^ 1] when it equals `reg`
 * (treating it as a wasted load). We DON'T do that here. On real
 * R3000A, two consecutive loads to the same register both commit:
 * the first appears at N+2 (briefly), the second at N+3 overwrites
 * it. Dropping the first means an instruction at N+2 reading the
 * register sees stale data instead of the first load's result.
 * Tomb Raider 3's GTE-heavy vertex code triggers this pattern and
 * was the second cause of the geometry corruption we saw. */
static void dl_record_load(int slot, u32 reg)
{
	if (reg == 0) {
		dl_reg[slot] = 0;
		return;
	}
	dl_reg[slot] = reg;
	dl_invalidate_iregs(reg);
}

/* Called when an instruction writes a GPR (ALU op, LUI, MFC0/MFC2,
 * LWL/LWR, etc.). If the target register has a pending load in flight,
 * kill it so the load's commit at the next step doesn't clobber the
 * write. Mirrors the kill arm of pcsx_rearmed's dloadRt. */
static void dl_kill_for_write(int reg)
{
	if (reg <= 0) return;
	if (dl_reg[0] == reg) dl_reg[0] = 0;
	if (dl_reg[1] == reg) dl_reg[1] = 0;
}

/* Compute the MIPS GPR that this instruction writes (or 0 if none).
 * Critically includes LWL (0x22) and LWR (0x26) — these write _Rt_
 * via REC_FUNC (interpreter fallback) which writes psxRegs.GPR.r[]
 * directly. Without listing them here, a pending deferred load to the
 * same register would commit at the next step and clobber the
 * LWL/LWR result. This was the root cause of the Tomb Raider 3
 * geometry corruption in the previous DLOAD_PORT attempt. */
static int dl_op_writeback_dest(u32 op)
{
	u32 mainop = op >> 26;
	u32 funct;
	u32 rs;

	switch (mainop) {
	case 0x00: /* SPECIAL */
		funct = op & 0x3f;
		switch (funct) {
		case 0x00: case 0x02: case 0x03: /* SLL, SRL, SRA */
		case 0x04: case 0x06: case 0x07: /* SLLV, SRLV, SRAV */
		case 0x09:                       /* JALR */
		case 0x10: case 0x12:            /* MFHI, MFLO */
		case 0x20: case 0x21: case 0x22: case 0x23: /* ADD ADDU SUB SUBU */
		case 0x24: case 0x25: case 0x26: case 0x27: /* AND OR XOR NOR */
		case 0x2a: case 0x2b:            /* SLT, SLTU */
			return (op >> 11) & 0x1f;    /* _Rd_ */
		}
		return 0;
	case 0x01: /* REGIMM: BLTZAL/BGEZAL write $ra */
		rs = (op >> 16) & 0x1f;
		if (rs == 0x10 || rs == 0x11) return 31;
		return 0;
	case 0x03: /* JAL: writes $ra */
		return 31;
	case 0x08: case 0x09:                /* ADDI, ADDIU */
	case 0x0a: case 0x0b:                /* SLTI, SLTIU */
	case 0x0c: case 0x0d: case 0x0e:     /* ANDI, ORI, XORI */
	case 0x0f:                           /* LUI */
		return (op >> 16) & 0x1f;        /* _Rt_ */
	case 0x10: /* COP0 */
	case 0x12: /* COP2 */
		rs = (op >> 21) & 0x1f;
		if (rs == 0x00 || rs == 0x02)    /* MFC?, CFC? — write _Rt_ */
			return (op >> 16) & 0x1f;
		return 0;
	case 0x22: case 0x26:                /* LWL, LWR — write _Rt_ via REC_FUNC */
		return (op >> 16) & 0x1f;
	default:
		return 0;
	}
}

/* Peephole: does instruction `op` READ MIPS GPR `reg`? Used by the load
 * emitters to detect a real load-use hazard at compile time. Only when
 * the very next MIPS instruction reads our load destination do we pay
 * the deferral cost; otherwise we write the result directly to the GPR
 * shadow / HW reg (path identical to the pre-DLOAD_PORT dynarec). */
static int dl_next_instr_reads(u32 op, u32 reg)
{
	u32 mainop, rs, rt, funct;

	if (reg == 0) return 0;

	mainop = op >> 26;
	rs = (op >> 21) & 0x1f;
	rt = (op >> 16) & 0x1f;

	switch (mainop) {
	case 0x00: /* SPECIAL */
		funct = op & 0x3f;
		switch (funct) {
		case 0x00: case 0x02: case 0x03: /* SLL, SRL, SRA — read rt */
			return rt == reg;
		case 0x04: case 0x06: case 0x07: /* SLLV, SRLV, SRAV */
		case 0x20: case 0x21: case 0x22: case 0x23: /* ADD, ADDU, SUB, SUBU */
		case 0x24: case 0x25: case 0x26: case 0x27: /* AND, OR, XOR, NOR */
		case 0x2a: case 0x2b:            /* SLT, SLTU */
		case 0x18: case 0x19:            /* MULT, MULTU */
		case 0x1a: case 0x1b:            /* DIV, DIVU */
			return rs == reg || rt == reg;
		case 0x08: case 0x09:            /* JR, JALR — read rs */
		case 0x11: case 0x13:            /* MTHI, MTLO — read rs */
			return rs == reg;
		}
		return 0;
	case 0x01: /* REGIMM (BLTZ/BGEZ/BLTZAL/BGEZAL) — read rs */
		return rs == reg;
	case 0x04: case 0x05: /* BEQ, BNE — read rs and rt */
		return rs == reg || rt == reg;
	case 0x06: case 0x07: /* BLEZ, BGTZ — read rs */
		return rs == reg;
	case 0x08: case 0x09: /* ADDI, ADDIU — read rs */
	case 0x0a: case 0x0b: /* SLTI, SLTIU */
	case 0x0c: case 0x0d: case 0x0e: /* ANDI, ORI, XORI */
		return rs == reg;
	case 0x10: case 0x12: /* COP0, COP2 */
		switch (rs) {
		case 0x04: /* MTC — reads rt */
		case 0x06: /* CTC — reads rt */
			return rt == reg;
		}
		return 0;
	case 0x20: case 0x21: case 0x23: /* LB, LH, LW — read rs (address) */
	case 0x24: case 0x25:            /* LBU, LHU */
		return rs == reg;
	case 0x22: case 0x26: /* LWL, LWR — read rs (addr) AND rt (merge target) */
		return rs == reg || rt == reg;
	case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2e: /* stores */
		return rs == reg || rt == reg;
	case 0x32: /* LWC2 — read rs */
	case 0x3a: /* SWC2 — read rs */
		return rs == reg;
	}
	return 0;
}
#endif /* DLOAD_PORT */


static void Return()
{
#if DLOAD_PORT
	/* Commit pending delayed loads before exiting the block — the next
	 * block must start with an empty dload buffer so the compile-time
	 * tracker matches reality. */
	emit_dload_flush();
#endif
	iFlushRegs(0);
	FlushAllHWReg();
	if (((u32)returnPC & 0x1fffffc) == (u32)returnPC) {
		BA((u32)returnPC);
	}
	else {
		LIW(0, (u32)returnPC);
		MTLR(0);
		BLR();
	}
}

static void iStoreCycle() {
	// what is idlecyclecount doing ?
	/* store cycle */
	count = (idlecyclecount + (pc - pcold) / 4) * BIAS;
	ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);
}

static void iRet() {
	iStoreCycle();
    Return();
}

static int iLoadTest() {
	u32 tmp;

	// check for load delay
	tmp = psxRegs.code >> 26;
	switch (tmp) {
		case 0x10: // COP0
			switch (_Rs_) {
				case 0x00: // MFC0
				case 0x02: // CFC0
					return 1;
			}
			break;
		case 0x12: // COP2
			switch (_Funct_) {
				case 0x00:
					switch (_Rs_) {
						case 0x00: // MFC2
						case 0x02: // CFC2
							return 1;
					}
					break;
			}
			break;
		case 0x32: // LWC2
			return 1;
		default:
			if (tmp >= 0x20 && tmp <= 0x26) { // LB/LH/LWL/LW/LBU/LHU/LWR
				return 1;
			}
			break;
	}
	return 0;
}

/* set a pending branch */
static void SetBranch() {
	int treg;
	branch = 1;
	psxRegs.code = PSXMu32_2(pc);
	pc+=4;

	if (iLoadTest() == 1) {
#if DLOAD_PORT
		/* Commit any pending deferred loads BEFORE psxDelayTest reads
		 * psxRegs.GPR.r[] from the interpreter side. Without this, a
		 * load in the previous instruction whose value still lives in
		 * dloadVal[] would be invisible to psxDelayTest, which then
		 * executes the delay-slot load against stale GPR state. This
		 * surfaces strongly in Tomb Raider 3's GTE-heavy code where
		 * JR/JALR routinely follow a memory load. */
		emit_dload_flush();
#endif
		iFlushRegs(0);
		LIW(0, psxRegs.code);
		STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));

		/* store cycle */
		iStoreCycle();

		treg = GetHWRegSpecial(TARGET);
		MR(PutHWRegSpecial(ARG2), treg);
		DisposeHWReg(GetHWRegFromCPUReg(treg));
		LIW(PutHWRegSpecial(ARG1), _Rt_);
        LIW(GetHWRegSpecial(PSXPC), pc);

		FlushAllHWReg();
		CALLFunc((u32)psxDelayTest);

		Return();
		return;
	}

//	recBSC[psxRegs.code>>26]();

	switch( psxRegs.code >> 24 )
	{
 	   // Lode Runner (jr - beq)

	   // bltz - bgez - bltzal - bgezal / beq - bne - blez - bgtz
 	   case 0x04:
	   case 0x10:
 	   case 0x14:
       case 0x18:
 	   case 0x1c:
	   break;

       default:
#if DLOAD_PORT
	   {
		   /* Same step+kill pattern as the main recompile loop applies
		    * to the delay slot — the delay slot is executed AFTER the
		    * branch decision but BEFORE the target, so it must run
		    * with the pre-branch GPR values for any pending load. */
		   int kill_dest;
		   emit_dload_step();
		   kill_dest = dl_op_writeback_dest(psxRegs.code);
		   if (kill_dest > 0)
			   dl_kill_for_write(kill_dest);
	   }
#endif
	   recBSC[psxRegs.code>>26]();
	   break;
	}

#if DLOAD_PORT
	/* Commit any load pending at the end of the delay slot before the
	 * block-exit sequence. */
	emit_dload_flush();
#endif

	iFlushRegs(0);

	// pc = target
	treg = GetHWRegSpecial(TARGET);
	MR(PutHWRegSpecial(PSXPC), GetHWRegSpecial(TARGET)); // FIXME: this line should not be needed
	DisposeHWReg(GetHWRegFromCPUReg(treg));

	FlushAllHWReg();

	/* store cycle */
	iStoreCycle();

	FlushAllHWReg();
	CALLFunc((u32)psxBranchTest);
	
	// TODO: don't return if target is compiled
	Return();
}

static void iJump(u32 branchPC) {
	u32 *b1, *b2;
	branch = 1;
	psxRegs.code = PSXMu32_2(pc);
	pc+=4;

	if (iLoadTest() == 1) {
#if DLOAD_PORT
		/* Commit pending deferred loads before psxDelayTest — see the
		 * matching comment in SetBranch. */
		emit_dload_flush();
#endif
		iFlushRegs(0);
		LIW(0, psxRegs.code);
		STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));

		/* store cycle */
		iStoreCycle();

		LIW(PutHWRegSpecial(ARG2), branchPC);
		LIW(PutHWRegSpecial(ARG1), _Rt_);
		LIW(GetHWRegSpecial(PSXPC), pc);
		FlushAllHWReg();
		CALLFunc((u32)psxDelayTest);

		Return();
		return;
	}

#if DLOAD_PORT
	{
		/* Step+kill for the unconditional-jump delay slot, mirroring
		 * SetBranch / iBranch. */
		int kill_dest;
		emit_dload_step();
		kill_dest = dl_op_writeback_dest(psxRegs.code);
		if (kill_dest > 0)
			dl_kill_for_write(kill_dest);
	}
#endif
	recBSC[psxRegs.code>>26]();

#if DLOAD_PORT
	/* Commit any load pending at the end of the delay slot. Both the
	 * Return() path AND the direct-jump-to-next-block path (BCTR via
	 * PC_REC[branchPC]) leave the block here; the next block is
	 * recompiled assuming an empty load buffer, so we must materialise
	 * dloadVal[] into GPR.r[] before transferring control. */
	emit_dload_flush();
#endif

	iFlushRegs(branchPC);
	LIW(PutHWRegSpecial(PSXPC), branchPC);
	FlushAllHWReg();

	/* store cycle */
	iStoreCycle();

	FlushAllHWReg();
	CALLFunc((u32)psxBranchTest);

	// always return for now...
	// Return();
	// maybe just happened an interruption, check so
	LIW(0, branchPC);
	CMPLW(GetHWRegSpecial(PSXPC), 0);
	BNE_L(b1);

	LIW(3, PC_REC(branchPC));
	LWZ(3, 0, 3);
	CMPLWI(3, 0);
	BNE_L(b2);

	B_DST(b1);
	Return();

	// next bit is already compiled - jump right to it
	B_DST(b2);
	MTCTR(3);
	BCTR();
}

static void iBranch(u32 branchPC, int savectx) {
	HWRegister HWRegistersS[NUM_HW_REGISTERS];
	iRegisters iRegsS[NUM_REGISTERS];
	int HWRegUseCountS = 0;
	u32 respold=0;
	u32 *b1, *b2;
#if DLOAD_PORT
	u32 dl_regS[2];
	int dl_selS = 0;
#endif

	if (savectx) {
		respold = resp;
		memcpy(iRegsS, iRegs, sizeof(iRegs));
		memcpy(HWRegistersS, HWRegisters, sizeof(HWRegisters));
		HWRegUseCountS = HWRegUseCount;
#if DLOAD_PORT
		dl_regS[0] = dl_reg[0];
		dl_regS[1] = dl_reg[1];
		dl_selS    = dl_sel;
#endif
	}

	branch = 1;
	psxRegs.code = PSXMu32_2(pc);

	// the delay test is only made when the branch is taken
	// savectx == 0 will mean that :)
	if (savectx == 0 && iLoadTest() == 1) {
#if DLOAD_PORT
		/* Commit pending deferred loads before psxDelayTest — see the
		 * matching comment in SetBranch. */
		emit_dload_flush();
#endif
		iFlushRegs(0);
		LIW(0, psxRegs.code);
		STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));

		/* store cycle */
		iStoreCycle();

		LIW(PutHWRegSpecial(ARG1), _Rt_);
		LIW(PutHWRegSpecial(ARG2), branchPC);
        LIW(GetHWRegSpecial(PSXPC), pc);

		FlushAllHWReg();
		CALLFunc((u32)psxDelayTest);

		Return();
		return;
	}

	pc+= 4;
#if DLOAD_PORT
	{
		/* Step+kill for the conditional-branch delay slot. */
		int kill_dest;
		emit_dload_step();
		kill_dest = dl_op_writeback_dest(psxRegs.code);
		if (kill_dest > 0)
			dl_kill_for_write(kill_dest);
	}
#endif
	recBSC[psxRegs.code>>26]();

#if DLOAD_PORT
	/* Commit any load pending at the end of the delay slot before
	 * transferring control out of this block. */
	emit_dload_flush();
#endif

	iFlushRegs(branchPC);
	LIW(PutHWRegSpecial(PSXPC), branchPC);
	FlushAllHWReg();

	/* store cycle */
	iStoreCycle();

	FlushAllHWReg();
	CALLFunc((u32)psxBranchTest);
	
	// always return for now...
	// Return();
	LIW(0, branchPC);
	CMPLW(GetHWRegSpecial(PSXPC), 0);
	BNE_L(b1);

	LIW(3, PC_REC(branchPC));
	LWZ(3, 0, 3);
	CMPLWI(3, 0);
	BNE_L(b2);

	B_DST(b1);
	Return();

	B_DST(b2);
	MTCTR(3);
	BCTR();

	pc-= 4;

	// Restore
	if (savectx) {
		resp = respold;
		memcpy(iRegs, iRegsS, sizeof(iRegs));
		memcpy(HWRegisters, HWRegistersS, sizeof(HWRegisters));
		HWRegUseCount = HWRegUseCountS;
#if DLOAD_PORT
		dl_reg[0] = dl_regS[0];
		dl_reg[1] = dl_regS[1];
		dl_sel    = dl_selS;
#endif
	}
}


static void iDumpRegs() {
	int i, j;

	printf("%lx %lx\n", psxRegs.pc, psxRegs.cycle);
	for (i=0; i<4; i++) {
		for (j=0; j<8; j++)
			printf("%lx ", psxRegs.GPR.r[j*i]);
		printf("\n");
	}
}

void iDumpBlock(char *ptr) {

}

#define REC_FUNC(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	LIW(PutHWRegSpecial(ARG1), (u32)psxRegs.code); \
	STW(GetHWRegSpecial(ARG1), OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
	LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
	FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
/*	branch = 2; */\
}

#define REC_SYS(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
        LIW(PutHWRegSpecial(ARG1), (u32)psxRegs.code); \
        STW(GetHWRegSpecial(ARG1), OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
        LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
        FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
	branch = 2; \
	iRet(); \
}

#define REC_BRANCH(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
        LIW(PutHWRegSpecial(ARG1), (u32)psxRegs.code); \
        STW(GetHWRegSpecial(ARG1), OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
        LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
        FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
	branch = 2; \
	iRet(); \
}

static void freeMem(int all)
{
    if (recMem) free(recMem);
    if (recRAM) free(recRAM);
    if (recROM) free(recROM);
    recMem = recRAM = recROM = 0;
    
    if (all && psxRecLUT) {
        free(psxRecLUT); psxRecLUT = NULL;
    }
}

static int allocMem() {
	int i;

	freeMem(1);
        
	if (psxRecLUT==NULL)
		psxRecLUT = (u32*) malloc(0x010000 * 4);

	recMem = (char*) malloc(RECMEM_SIZE);
	recRAM = (char*) malloc(0x200000);
	recROM = (char*) malloc(0x080000);
	if (recRAM == NULL || recROM == NULL || recMem == NULL || psxRecLUT == NULL) {
                freeMem(1);
		SysMessage("Error allocating memory"); return -1;
	}

	for (i=0; i<0x80; i++) 
		psxRecLUT[i + 0x0000] = (u32)&recRAM[(i & 0x1f) << 16];

	memcpy(psxRecLUT + 0x8000, psxRecLUT, 0x80 * 4);
	memcpy(psxRecLUT + 0xa000, psxRecLUT, 0x80 * 4);

	for (i=0; i<0x08; i++) psxRecLUT[i + 0xbfc0] = (u32)&recROM[i << 16];
	
	return 0;
}

static int recInit() {
	Config.CpuRunning = 1;

	return allocMem();
}

static void recReset() {
	memset(recRAM, 0, 0x200000);
	memset(recROM, 0, 0x080000);

	ppcInit();
	ppcSetPtr((u32 *)recMem);

	branch = 0;
	memset(iRegs, 0, sizeof(iRegs));
	iRegs[0].state = ST_CONST;
	iRegs[0].k     = 0;

#if DLOAD_PORT
	dl_reg[0] = dl_reg[1] = 0;
	dl_sel = 0;
#endif
}

#if DLOAD_PORT
/* Public toggle for the libretro `pcsxr360_load_delay` variable.
 * Compiled blocks bake the dload choice in (defer vs direct write,
 * const fast-paths gated, etc.), so flipping the flag REQUIRES the
 * recompiler cache to be invalidated. recReset() does exactly that
 * — clears recRAM/recROM (the per-PSX-address PPC code pointer table)
 * without touching psxRegs, so PSX execution state is preserved. */
void psxRec_setLoadDelay(int enabled)
{
	int new_val = enabled ? 1 : 0;
	if (new_val == dload_enabled) return;
	dload_enabled = new_val;
	recReset();
}
#endif

static void recShutdown() {
    freeMem(1);
	ppcShutdown();
}

static void recError() {
	SysReset();
	ClosePlugins();
	SysMessage("Unrecoverable error while running recompiler\n");
	SysRunGui();
}

__inline static void execute() {
    void (**recFunc)();
    char *p;

    p = (char*) PC_REC(psxRegs.pc);

    recFunc = (void (**)()) (u32) p;

    if (*recFunc == 0) {
        recRecompile();
    }
    recRun(*recFunc, (u32) & psxRegs, (u32) & psxM_2);
}

void recExecute() {
    while(Config.CpuRunning) {
        execute();
	}
}

void recExecuteBlock() {
    execute();
}

void recClear(u32 mem, u32 size) {
	u32 ptr = psxRecLUT[mem >> 16];

	if (ptr != NULL) {
		memset((void*) (ptr + (mem & 0xFFFF)), 0, size * 4);
	}
}

static void recNULL() {
    //	SysMessage("recUNK: %8.8x\n", psxRegs.code);
}

/*********************************************************
 * goes to opcodes tables...                              *
 * Format:  table[something....]                          *
 *********************************************************/

//REC_SYS(SPECIAL);

static void recSPECIAL() {
    recSPC[_Funct_]();
}

static void recREGIMM() {
    recREG[_Rt_]();
}

static void recCOP0() {
    recCP0[_Rs_]();
}

//REC_SYS(COP2);

static void recCOP2() {
    recCP2[_Funct_]();
}

static void recBASIC() {
    recCP2BSC[_Rs_]();
}

//end of Tables opcodes...

/* - Arithmetic with immediate operand - */

/*********************************************************
 * Arithmetic with immediate operand                      *
 * Format:  OP rt, rs, immediate                          *
 *********************************************************/

static void recADDIU() {
    // Rt = Rs + Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k + _Imm_);
    } else {
        if (_Imm_ == 0) {
            MapCopy(_Rt_, _Rs_);
        } else {
            ADDI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _Imm_);
        }
    }
}

static void recADDI() {
    // Rt = Rs + Im
    recADDIU();
}

//CR0:	SIGN      | POSITIVE | ZERO  | SOVERFLOW | SOVERFLOW | OVERFLOW | CARRY

static void recSLTI() {
    // Rt = Rs < Im (signed)
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, (s32) iRegs[_Rs_].k < _Imm_);
    } else {
        if (_Imm_ == 0) {
            SRWI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), 31);
        } else {
            int reg;
            CMPWI(GetHWReg32(_Rs_), _Imm_);
            reg = PutHWReg32(_Rt_);
            LI(reg, 1);
            BLT(1);
            LI(reg, 0);
        }
    }
}

static void recSLTIU() {
    // Rt = Rs < Im (unsigned)
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k < _ImmU_);
    } else {
        int reg;
        CMPLWI(GetHWReg32(_Rs_), _Imm_);
        reg = PutHWReg32(_Rt_);
        LI(reg, 1);
        BLT(1);
        LI(reg, 0);
    }
}

static void recANDI() {
    // Rt = Rs And Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k & _ImmU_);
    } else {
        ANDI_(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _ImmU_);
    }
}

static void recORI() {
    // Rt = Rs Or Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k | _ImmU_);
    } else {
        if (_Imm_ == 0) {
            MapCopy(_Rt_, _Rs_);
        } else {
            ORI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _ImmU_);
        }
    }
}

static void recXORI() {
    // Rt = Rs Xor Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k ^ _ImmU_);
    } else {
        XORI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _ImmU_);
    }
}

//end of * Arithmetic with immediate operand  

/*********************************************************
 * Load higher 16 bits of the first word in GPR with imm  *
 * Format:  OP rt, immediate                              *
 *********************************************************/

static void recLUI() {
    // Rt = Imm << 16
    if (!_Rt_) return;

    MapConst(_Rt_, _Imm_ << 16);
}

//End of Load Higher .....

/* - Register arithmetic - */

/*********************************************************
 * Register arithmetic                                    *
 * Format:  OP rd, rs, rt                                 *
 *********************************************************/

static void recADDU() {
    // Rd = Rs + Rt 
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k + iRegs[_Rt_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        if ((s32) (s16) iRegs[_Rs_].k == (s32) iRegs[_Rs_].k) {
            ADDI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), (s16) iRegs[_Rs_].k);
        } else if ((iRegs[_Rs_].k & 0xffff) == 0) {
            ADDIS(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k >> 16);
        } else {
            ADD(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((s32) (s16) iRegs[_Rt_].k == (s32) iRegs[_Rt_].k) {
            ADDI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), (s16) iRegs[_Rt_].k);
        } else if ((iRegs[_Rt_].k & 0xffff) == 0) {
            ADDIS(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k >> 16);
        } else {
            ADD(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        ADD(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recADD() {
    // Rd = Rs + Rt
    recADDU();
}

static void recSUBU() {
    // Rd = Rs - Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k - iRegs[_Rt_].k);
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((s32) (s16) (-iRegs[_Rt_].k) == (s32) (-iRegs[_Rt_].k)) {
            ADDI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), -iRegs[_Rt_].k);
        } else if (((-iRegs[_Rt_].k) & 0xffff) == 0) {
            ADDIS(PutHWReg32(_Rd_), GetHWReg32(_Rs_), (-iRegs[_Rt_].k) >> 16);
        } else {
            SUB(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        SUB(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recSUB() {
    // Rd = Rs - Rt
    recSUBU();
}

static void recAND() {
    // Rd = Rs And Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k & iRegs[_Rt_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        // TODO: implement shifted (ANDIS) versions of these
        if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
            ANDI_(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
        } else {
            AND(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
            ANDI_(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
        } else {
            AND(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        AND(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recOR() {
    // Rd = Rs Or Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k | iRegs[_Rt_].k);
    } else {
        if (_Rs_ == _Rt_) {
            MapCopy(_Rd_, _Rs_);
        } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
            if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
                ORI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else {
                OR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
            if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
                ORI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else {
                OR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else {
            OR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    }
}

static void recXOR() {
    // Rd = Rs Xor Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k ^ iRegs[_Rt_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
            XORI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
        } else {
            XOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
            XORI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
        } else {
            XOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        XOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recNOR() {
// Rd = Rs Nor Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
            MapConst(_Rd_, ~(iRegs[_Rs_].k | iRegs[_Rt_].k));
    } else if (IsConst(_Rs_)) {
            LI(0, iRegs[_Rs_].k);
            NOR(PutHWReg32(_Rd_), GetHWReg32(_Rt_), 0);
    } else if (IsConst(_Rt_)) {
            LI(0, iRegs[_Rt_].k);
            NOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), 0);
    } else {
            NOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recSLT() {
    // Rd = Rs < Rt (signed)
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, (s32) iRegs[_Rs_].k < (s32) iRegs[_Rt_].k);
    } else { // TODO: add immidiate cases
        int reg;
        CMPW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        reg = PutHWReg32(_Rd_);
        LI(reg, 1);
        BLT(1);
        LI(reg, 0);
    }
}

static void recSLTU() {
    // Rd = Rs < Rt (unsigned)
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k < iRegs[_Rt_].k);
    } else { // TODO: add immidiate cases
        SUBFC(PutHWReg32(_Rd_), GetHWReg32(_Rt_), GetHWReg32(_Rs_));
        SUBFE(PutHWReg32(_Rd_), GetHWReg32(_Rd_), GetHWReg32(_Rd_));
        NEG(PutHWReg32(_Rd_), GetHWReg32(_Rd_));
    }
}

//End of * Register arithmetic

/* - mult/div & Register trap logic - */

/*********************************************************
 * Register mult/div & Register trap logic                *
 * Format:  OP rs, rt                                     *
 *********************************************************/

int DoShift(u32 k) {
    u32 i;
    for (i = 0; i < 30; i++) {
        if (k == (1ul << i))
            return i;
    }
    return -1;
}

static void recMULT() {
    // Lo/Hi = Rs * Rt (signed)
    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        u64 res = (s64) ((s64) (s32) iRegs[_Rs_].k * (s64) (s32) iRegs[_Rt_].k);
        MapConst(REG_LO, (res & 0xffffffff));
        MapConst(REG_HI, ((res >> 32) & 0xffffffff));
        return;
    }
	MULLW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
	MULHW(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
}

static void recMULTU() {
    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        u64 res = (u64) ((u64) (u32) iRegs[_Rs_].k * (u64) (u32) iRegs[_Rt_].k);
        MapConst(REG_LO, (res & 0xffffffff));
        MapConst(REG_HI, ((res >> 32) & 0xffffffff));
        return;
    }

	MULLW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
	MULHWU(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
}

REC_FUNC(DIV);
REC_FUNC(DIVU);

//End of * Register mult/div & Register trap logic  

REC_FUNC(LWL);
REC_FUNC(LWR);
REC_FUNC(SWL);
REC_FUNC(SWR);

/* `defer_load` (DLOAD_PORT only): when non-zero, the caller will defer
 * the loaded value to psxRegs.dloadVal[] instead of writing it directly
 * to the GPR shadow. In that case _Rt_'s OLD value MUST stay in memory
 * for the delay slot's read, so we use dl_invalidate_iregs (writes back
 * unconditionally) instead of DisposeHWReg (which can skip the writeback
 * when it sees the LW will overwrite anyway). When defer_load is zero
 * (no load-use hazard, peephole path), we use the original DisposeHWReg
 * — same as pre-DLOAD_PORT, so the fast path is preserved. */
static void preMemRead(int defer_load)
{
	int rs;
	(void)defer_load;

	ReserveArgs(1);
	if (_Rs_ != _Rt_) {
#if DLOAD_PORT
		if (defer_load) dl_invalidate_iregs(_Rt_);
		else            DisposeHWReg(iRegs[_Rt_].reg);
#else
		DisposeHWReg(iRegs[_Rt_].reg);
#endif
	}
	rs = GetHWReg32(_Rs_);
	if (rs != 3 || _Imm_ != 0) {
		ADDI(PutHWRegSpecial(ARG1), rs, _Imm_);
	}
	if (_Rs_ == _Rt_) {
#if DLOAD_PORT
		if (defer_load) dl_invalidate_iregs(_Rt_);
		else            DisposeHWReg(iRegs[_Rt_].reg);
#else
		DisposeHWReg(iRegs[_Rt_].reg);
#endif
	}
	InvalidateCPURegs();

}

static void preMemWrite(int size)
{
	int rs;

	ReserveArgs(2);
	rs = GetHWReg32(_Rs_);
	if (rs != 3 || _Imm_ != 0) {
		ADDI(PutHWRegSpecial(ARG1), rs, _Imm_);
	}
	if (size == 1) {
		RLWINM(PutHWRegSpecial(ARG2), GetHWReg32(_Rt_), 0, 24, 31);
	} else if (size == 2) {
		RLWINM(PutHWRegSpecial(ARG2), GetHWReg32(_Rt_), 0, 16, 31);
	} else {
		MR(PutHWRegSpecial(ARG2), GetHWReg32(_Rt_));
	}

	InvalidateCPURegs();
	
}

#if DLOAD_PORT
/* Common tail for the simple 8/16/32-bit loads. After CALLFunc, r3 holds
 * the loaded value (already extended for LB/LH cases via the caller).
 * We park it in psxRegs.dloadVal[slot] and record the destination so
 * that the NEXT instruction's emit_dload_step commits it to the GPR
 * shadow — that's the 1-cycle MIPS load delay. */
static void dl_emit_defer_load(void)
{
	int slot;
	if (_Rt_ == 0) return;
	slot = dl_sel ^ 1;
	STW(3, OFFSET(&psxRegs, &psxRegs.dloadVal[slot]),
	    GetHWRegSpecial(PSXREGS));
	dl_record_load(slot, _Rt_);
}
#endif

#if DLOAD_PORT
/* Peephole: returns 1 iff a load to `_Rt_` should be deferred because
 * the very next MIPS instruction reads it. */
static int dl_should_defer(void)
{
	if (!dload_enabled) return 0;
	if (_Rt_ == 0) return 0;
	return dl_next_instr_reads(PSXMu32_2(pc), _Rt_);
}
#endif

static void recLB() {
	u32 func = (u32) psxMemRead8_2;
	int defer = 0;

#if DLOAD_PORT
	defer = dl_should_defer();
#endif

	preMemRead(defer);
	CALLFunc(func);
#if DLOAD_PORT
	if (defer) {
		if (_Rt_) {
			EXTSB(3, 3);             /* sign-extend in place */
			dl_emit_defer_load();
		}
		return;
	}
#endif
	if (_Rt_) {
		EXTSB(PutHWReg32(_Rt_), 3);
	}
}

static void recLBU() {
	u32 func = (u32) psxMemRead8_2;
	int defer = 0;

#if DLOAD_PORT
	defer = dl_should_defer();
#endif

	preMemRead(defer);
	CALLFunc(func);

#if DLOAD_PORT
	if (defer) {
		dl_emit_defer_load();
		return;
	}
#endif
	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
}

static void recLH() {
	u32 func = (u32) psxMemRead16_2;
	int defer = 0;

#if DLOAD_PORT
	defer = dl_should_defer();
#endif

	preMemRead(defer);
	CALLFunc(func);
#if DLOAD_PORT
	if (defer) {
		if (_Rt_) {
			EXTSH(3, 3);             /* sign-extend in place */
			dl_emit_defer_load();
		}
		return;
	}
#endif
	if (_Rt_) {
		EXTSH(PutHWReg32(_Rt_), 3);
	}
}

static void recLHU() {
	u32 func = (u32) psxMemRead16_2;
	int defer = 0;

#if DLOAD_PORT
	defer = dl_should_defer();
#endif

	preMemRead(defer);
	CALLFunc(func);
#if DLOAD_PORT
	if (defer) {
		dl_emit_defer_load();
		return;
	}
#endif
	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
}

static void recLW() {
	u32 func = (u32) psxMemRead32_2;
	int defer = 0;

#if DLOAD_PORT
	defer = dl_should_defer();
#endif

	/* Const-address inline fast paths. With the peephole, these are
	 * preserved whenever there's no load-use hazard at N+1 — which is
	 * the overwhelmingly common case (compilers schedule around load
	 * delay slots). On hazard we fall through to the slow CALLFunc
	 * path that feeds dl_emit_defer_load. */
	if (!defer && IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0xfff0) == 0xbfc0) {
			if (!_Rt_) return;
			// since bios is readonly it won't change
			MapConst(_Rt_, psxRu32_2(addr));
			return;
		}
		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			if (!_Rt_) return;

			LIW(PutHWReg32(_Rt_), (u32)&psxM_2[addr & 0x1fffff]);
			LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			if (!_Rt_) return;

			LIW(PutHWReg32(_Rt_), (u32)&psxH_2[addr & 0xfff]);
			LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			return;
		}
		if (t == 0x1f80) {
			switch (addr) {
				case 0x1f801080: case 0x1f801084: case 0x1f801088:
				case 0x1f801090: case 0x1f801094: case 0x1f801098:
				case 0x1f8010a0: case 0x1f8010a4: case 0x1f8010a8:
				case 0x1f8010b0: case 0x1f8010b4: case 0x1f8010b8:
				case 0x1f8010c0: case 0x1f8010c4: case 0x1f8010c8:
				case 0x1f8010d0: case 0x1f8010d4: case 0x1f8010d8:
				case 0x1f8010e0: case 0x1f8010e4: case 0x1f8010e8:
				case 0x1f801070: case 0x1f801074:
				case 0x1f8010f0: case 0x1f8010f4:
					if (!_Rt_) return;

					LIW(PutHWReg32(_Rt_), (u32)&psxH_2[addr & 0xffff]);
					LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
					return;

				case 0x1f801810:
					if (!_Rt_) return;

					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32)GPU_readData);

					SetDstCPUReg(3);
					PutHWReg32(_Rt_);
					return;

				case 0x1f801814:
					if (!_Rt_) return;

					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32)GPU_readStatus);

					SetDstCPUReg(3);
					PutHWReg32(_Rt_);
					return;
			}
		}
//		SysPrintf("unhandled r32 %x\n", addr);
	}

	preMemRead(defer);
	CALLFunc(func);
#if DLOAD_PORT
	if (defer) {
		dl_emit_defer_load();
		return;
	}
#endif
	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
}

void recClear32(u32 addr) {
	recClear(addr, 1);
}

static void recSB() {	
	u32 func = (u32) psxMemWrite8_2;

	preMemWrite(1);
	CALLFunc((u32) func);
}

static void recSH() {
	u32 func = (u32) psxMemWrite16_2;

	preMemWrite(2);
	CALLFunc(func);
}

static void recSW() {
	u32 func = (u32) psxMemWrite32_2;

	preMemWrite(4);
	CALLFunc(func);
}

static void recSLL() {
    // Rd = Rt << Sa
    if (!_Rd_) return;

    if (IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rt_].k << _Sa_);
    } else {
        SLWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), _Sa_);
    }
}

static void recSRL() {
    // Rd = Rt >> Sa
    if (!_Rd_) return;

    if (IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rt_].k >> _Sa_);
    } else {
        SRWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), _Sa_);
    }
}

static void recSRA() {
    // Rd = Rt >> Sa
    if (!_Rd_) return;

    if (IsConst(_Rt_)) {
        MapConst(_Rd_, (s32) iRegs[_Rt_].k >> _Sa_);
    } else {
        SRAWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), _Sa_);
    }
}

/* - shift ops - */
REC_FUNC(SLLV);
REC_FUNC(SRLV);
REC_FUNC(SRAV);

static void recSYSCALL() {
    //	dump=1;
    iFlushRegs(0);

    LIW(PutHWRegSpecial(PSXPC), pc - 4);
    LIW(3, 0x20);
    LIW(4, (branch == 1 ? 1 : 0));
    FlushAllHWReg();
    CALLFunc((u32) psxException);
    
    branch = 2;
    iRet();
}

static void recBREAK() {
}

static void recMFHI() {
    // Rd = Hi
    if (!_Rd_) return;

    if (IsConst(REG_HI)) {
        MapConst(_Rd_, iRegs[REG_HI].k);
    } else {
        MapCopy(_Rd_, REG_HI);
    }
}

static void recMTHI() {
    // Hi = Rs

    if (IsConst(_Rs_)) {
        MapConst(REG_HI, iRegs[_Rs_].k);
    } else {
        MapCopy(REG_HI, _Rs_);
    }
}

static void recMFLO() {
    // Rd = Lo
    if (!_Rd_) return;

    if (IsConst(REG_LO)) {
        MapConst(_Rd_, iRegs[REG_LO].k);
    } else {
        MapCopy(_Rd_, REG_LO);
    }
}

static void recMTLO() {
    // Lo = Rs

    if (IsConst(_Rs_)) {
        MapConst(REG_LO, iRegs[_Rs_].k);
    } else {
        MapCopy(REG_LO, _Rs_);
    }
}

/* - branch ops - */

static void recBEQ() {
    // Branch if Rs == Rt
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (_Rs_ == _Rt_) {
        iJump(bpc);
    } else {
        if (IsConst(_Rs_) && IsConst(_Rt_)) {
            if (iRegs[_Rs_].k == iRegs[_Rt_].k) {
                iJump(bpc);
                return;
            } else {
                iJump(pc + 4);
                return;
            }
        } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
            if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
                CMPLWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else if ((s32) (s16) iRegs[_Rs_].k == (s32) iRegs[_Rs_].k) {
                CMPWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else {
                CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
            if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
                CMPLWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else if ((s32) (s16) iRegs[_Rt_].k == (s32) iRegs[_Rt_].k) {
                CMPWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else {
                CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else {
            CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }

        BEQ_L(b);

        iBranch(pc + 4, 1);

        B_DST(b);

        iBranch(bpc, 0);
        pc += 4;
    }
}

static void recBNE() {
    // Branch if Rs != Rt
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (_Rs_ == _Rt_) {
        iJump(pc + 4);
    } else {
        if (IsConst(_Rs_) && IsConst(_Rt_)) {
            if (iRegs[_Rs_].k != iRegs[_Rt_].k) {
                iJump(bpc);
                return;
            } else {
                iJump(pc + 4);
                return;
            }
        } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
            if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
                CMPLWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else if ((s32) (s16) iRegs[_Rs_].k == (s32) iRegs[_Rs_].k) {
                CMPWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else {
                CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
            if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
                CMPLWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else if ((s32) (s16) iRegs[_Rt_].k == (s32) iRegs[_Rt_].k) {
                CMPWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else {
                CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else {
            CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }

        BNE_L(b);

        iBranch(pc + 4, 1);

        B_DST(b);

        iBranch(bpc, 0);
        pc += 4;
    }
}

static void recBLTZ() {
    // Branch if Rs < 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k < 0) {
            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLT_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    pc += 4;
}

static void recBGTZ() {
    // Branch if Rs > 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k > 0) {
            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGT_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    pc += 4;
}

static void recBLTZAL() {
    // Branch if Rs < 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k < 0) {
            MapConst(31, pc + 4);

            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLT_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    MapConst(31, pc + 4);

    iBranch(bpc, 0);
    pc += 4;
}

static void recBGEZAL() {
    // Branch if Rs >= 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k >= 0) {
            MapConst(31, pc + 4);

            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGE_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    MapConst(31, pc + 4);

    iBranch(bpc, 0);
    pc += 4;
}

static void recJ() {
    // j target

    iJump(_Target_ * 4 + (pc & 0xf0000000));
}

static void recJAL() {
    // jal target
    MapConst(31, pc + 4);

    iJump(_Target_ * 4 + (pc & 0xf0000000));
}

static void recJR() {
    // jr Rs

    if (IsConst(_Rs_)) {
        iJump(iRegs[_Rs_].k);
    } else {
        MR(PutHWRegSpecial(TARGET), GetHWReg32(_Rs_));
        SetBranch();
    }
}

static void recJALR() {
    // jalr Rs

    if (_Rd_) {
        MapConst(_Rd_, pc + 4);
    }

    if (IsConst(_Rs_)) {
        iJump(iRegs[_Rs_].k);
    } else {
        MR(PutHWRegSpecial(TARGET), GetHWReg32(_Rs_));
        SetBranch();
    }
}


static void recBLEZ() {
    // Branch if Rs <= 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k <= 0) {
            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLE_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    pc += 4;
}

static void recBGEZ() {
    // Branch if Rs >= 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k >= 0) {
            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGE_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    pc += 4;
}

static void recRFE() {
    
    iFlushRegs(0);
    LWZ(0, OFFSET(&psxRegs, &psxRegs.CP0.n.Status), GetHWRegSpecial(PSXREGS));
    RLWINM(11, 0, 0, 0, 27);
    RLWINM(0, 0, 30, 28, 31);
    OR(0, 0, 11);
    STW(0, OFFSET(&psxRegs, &psxRegs.CP0.n.Status), GetHWRegSpecial(PSXREGS));

    LIW(PutHWRegSpecial(PSXPC), (u32) pc);
    FlushAllHWReg();
    if (branch == 0) {
        branch = 2;
        iRet();
    }
}

static void recMFC0() {
    // Rt = Cop0->Rd
    if (!_Rt_) return;

    LWZ(PutHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
}

static void recCFC0() {
    // Rt = Cop0->Rd

    recMFC0();
}

static void recMTC0() {

    {
        switch (_Rd_) {
            case 13:
                RLWINM(0, GetHWReg32(_Rt_), 0, 22, 15); // & ~(0xfc00)
                STW(0, OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
                break;
            default://12
                STW(GetHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
                break;
        }
    }

    if (_Rd_ == 12 || _Rd_ == 13) {
        iFlushRegs(0);
        LIW(PutHWRegSpecial(PSXPC), (u32) pc);
        FlushAllHWReg();
        CALLFunc((u32) psxTestSWInts);      
      
        if (branch == 0) {
            branch = 2;
            iRet();
        }
    }
}

static void recCTC0() {
    // Cop0->Rd = Rt

    recMTC0();
}

// GTE
#define CP2_FUNC(f) \
void gte##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	LIW(0, (u32)psxRegs.code); \
	STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
	FlushAllHWReg(); \
	CALLFunc ((u32)gte##f); \
}

#define CP2_FUNCNC(f) \
void gte##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	CALLFunc ((u32)gte##f); \
/*	branch = 2; */\
}


// GTE function callers
CP2_FUNC(MFC2);
CP2_FUNC(MTC2);
CP2_FUNC(CFC2);
CP2_FUNC(CTC2);
CP2_FUNC(LWC2);
CP2_FUNC(SWC2);
CP2_FUNCNC(RTPS);
CP2_FUNC(OP);
CP2_FUNCNC(NCLIP);
CP2_FUNC(DPCS);
CP2_FUNC(INTPL);
CP2_FUNC(MVMVA);
CP2_FUNCNC(NCDS);
CP2_FUNCNC(NCDT);
CP2_FUNCNC(CDP);
CP2_FUNCNC(NCCS);
CP2_FUNCNC(CC);
CP2_FUNCNC(NCS);
CP2_FUNCNC(NCT);
CP2_FUNC(SQR);
CP2_FUNC(DCPL);
CP2_FUNCNC(DPCT);
CP2_FUNCNC(AVSZ3);
CP2_FUNCNC(AVSZ4);
CP2_FUNCNC(RTPT);
CP2_FUNC(GPF);
CP2_FUNC(GPL);
CP2_FUNCNC(NCCT);


static void recHLE() {
    CALLFunc((u32) psxHLEt[psxRegs.code & 0x7f]);
    branch = 2;
    iRet();
}

static void (*recBSC[64])() = {
    recSPECIAL, recREGIMM, recJ, recJAL, recBEQ, recBNE, recBLEZ, recBGTZ,
    recADDI, recADDIU, recSLTI, recSLTIU, recANDI, recORI, recXORI, recLUI,
    recCOP0, recNULL, recCOP2, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recLB, recLH, recLWL, recLW, recLBU, recLHU, recLWR, recNULL,
    recSB, recSH, recSWL, recSW, recNULL, recNULL, recSWR, recNULL,
    recNULL, recNULL, recLWC2, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recSWC2, recHLE, recNULL, recNULL, recNULL, recNULL
};

static void (*recSPC[64])() = {
    recSLL, recNULL, recSRL, recSRA, recSLLV, recNULL, recSRLV, recSRAV,
    recJR, recJALR, recNULL, recNULL, recSYSCALL, recBREAK, recNULL, recNULL,
    recMFHI, recMTHI, recMFLO, recMTLO, recNULL, recNULL, recNULL, recNULL,
    recMULT, recMULTU, recDIV, recDIVU, recNULL, recNULL, recNULL, recNULL,
    recADD, recADDU, recSUB, recSUBU, recAND, recOR, recXOR, recNOR,
    recNULL, recNULL, recSLT, recSLTU, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

static void (*recREG[32])() = {
    recBLTZ, recBGEZ, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recBLTZAL, recBGEZAL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

static void (*recCP0[32])() = {
    recMFC0, recNULL, recCFC0, recNULL, recMTC0, recNULL, recCTC0, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recRFE, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

static void (*recCP2[64])() = {
    recBASIC, recRTPS, recNULL, recNULL, recNULL, recNULL, recNCLIP, recNULL, // 00
    recNULL, recNULL, recNULL, recNULL, recOP, recNULL, recNULL, recNULL, // 08
    recDPCS, recINTPL, recMVMVA, recNCDS, recCDP, recNULL, recNCDT, recNULL, // 10
    recNULL, recNULL, recNULL, recNCCS, recCC, recNULL, recNCS, recNULL, // 18
    recNCT, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, // 20
    recSQR, recDCPL, recDPCT, recNULL, recNULL, recAVSZ3, recAVSZ4, recNULL, // 28 
    recRTPT, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, // 30
    recNULL, recNULL, recNULL, recNULL, recNULL, recGPF, recGPL, recNCCT // 38
};

static void (*recCP2BSC[32])() = {
    recMFC2, recNULL, recCFC2, recNULL, recMTC2, recNULL, recCTC2, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};


// try
void psxREGIMM();
void execI();

static void recRecompile() {
	//static int recCount = 0;
	char *p;
	u32 *ptr;
	u32 a;
	int i;
	
	cop2readypc = 0;
	idlecyclecount = 0;

	// initialize state variables
	UniqueRegAlloc = 1;
	HWRegUseCount = 0;
	DstCPUReg = -1;
	memset(HWRegisters, 0, sizeof(HWRegisters));
	for (i=0; i<NUM_HW_REGISTERS; i++)
		HWRegisters[i].code = cpuHWRegisters[NUM_HW_REGISTERS-i-1];
	
	// reserve the special psxReg register
	HWRegisters[0].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
	HWRegisters[0].private = PSXREGS;
	HWRegisters[0].k = (u32)&psxRegs;

	HWRegisters[1].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
	HWRegisters[1].private = PSXMEM;
	HWRegisters[1].k = (u32)&psxM_2;

	for (i=0; i<NUM_REGISTERS; i++) {
		iRegs[i].state = ST_UNK;
		iRegs[i].reg = -1;
	}
	iRegs[0].k = 0;
	iRegs[0].state = ST_CONST;
	
	/* if ppcPtr reached the mem limit reset whole mem */
	if (((u32)ppcPtr - (u32)recMem) >= (RECMEM_SIZE - 0x10000))
		recReset();

	ppcAlign(/*32*/4);
	ptr = ppcPtr;
	
	// tell the LUT where to find us
	PC_REC32(psxRegs.pc) = (u32)ppcPtr;

	pcold = pc = psxRegs.pc;

#if DLOAD_PORT
	/* Each block starts with an empty load-delay buffer. The previous
	 * block flushed before exiting (see Return()), so runtime state
	 * matches our compile-time tracker. */
	dl_reg[0] = dl_reg[1] = 0;
	dl_sel = 0;
#endif

	for (count=0; count<500;) {

		u32 adr = pc & 0x1fffff;
		u32 op;

		p = (char *)PSXM_2(pc);
		psxRegs.code = SWAP32(*(u32 *)p);

		pc+=4;
		count++;

		op = psxRegs.code>>26;

#if DLOAD_PORT
		{
			/* Mirror pcsx_rearmed's dloadStep + dloadRt's kill arm:
			 * commit the slot we toggled away from on the previous
			 * instruction, then kill any pending whose dest matches
			 * THIS instruction's GPR write target. */
			int kill_dest;
			emit_dload_step();
			kill_dest = dl_op_writeback_dest(psxRegs.code);
			if (kill_dest > 0)
				dl_kill_for_write(kill_dest);
		}
#endif

		recBSC[op]();

		if (branch) {
			branch = 0;
			goto done;
		}

	}

	iFlushRegs(pc);
	LIW(PutHWRegSpecial(PSXPC), pc);
    iRet();

done:;

	a = (u32)(u8*)ptr;
	while(a < (u32)(u8*)ppcPtr) {
		__icbi(0, a);
		__dcbst(0, a);
	  a += 4;
	}
	__emit(0x7c0004ac);//sync
	__emit(0x4C00012C);//isync

	sprintf((char *)ppcPtr, "PC=%08x", pcold);
	ppcPtr += strlen((char *)ppcPtr);

}


R3000Acpu psxRec = {
	recInit,
	recReset,
	recExecute,
	recExecuteBlock,
	recClear,
	recShutdown
};


