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

#ifdef _MSC_VER_
#pragma warning(disable:4244)
#pragma warning(disable:4761)
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
//#include "bram.h"
void * __cdecl balloc(_In_ size_t _Size);
void   __cdecl bfree(_Inout_opt_ void * _Memory);
#include "../psxcommon.h"
#include "ppc.h"
#include "reguse.h"
#include "../r3000a.h"
#include "../psxhle.h"
#include "../psxhw.h"
#include "ppcAlloc.h"

#define malloc balloc
#define free bfree

static void recRecompile();

static void MOVI2R(int dest, unsigned int imm) {
	LIS(dest, imm>>16);
	if ((imm & 0xFFFF) != 0) {
		// LO 16bit
		ORI(dest, dest, imm & 0xFFFF);
	}
}

void __declspec(naked) __icbi(int offset, const void * base)
{
	__asm {
        icbi r3,r4
        blr
    }
}

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

u32 *psxRecLUT;

static char *recMem;	/* the recompiled blocks will be here */
static char *recRAM;	/* and the ptr to the blocks here */
static char *recROM;	/* and here */

u32 cop2readypc = 0;
u32 idlecyclecount = 0;

static void (*recBSC[64])();
static void (*recSPC[64])();
static void (*recREG[32])();
static void (*recCP0[32])();
static void (*recCP2[64])();
static void (*recCP2BSC[32])();

static void Return()
{
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
	ppcRec.count = (idlecyclecount + (ppcRec.pc - ppcRec.pcold) / 4) * BIAS;
	ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), ppcRec.count);
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
	ppcRec.branch = 1;
if(use_vm){
	psxRegs.code = PSXMu32(ppcRec.pc);}
else{
	psxRegs.code = PSXMu32_2(ppcRec.pc);}//teste
	ppcRec.pc+=4;

	if (iLoadTest() == 1) {
		iFlushRegs(0);
		LIW(0, psxRegs.code);
		STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));

		/* store cycle */
		iStoreCycle();
		
		treg = GetHWRegSpecial(TARGET);
		MR(R4, treg);
		DisposeHWReg(GetHWRegFromCPUReg(treg));
		LIW(R3, _Rt_);
        LIW(GetHWRegSpecial(PSXPC), ppcRec.pc);

		FlushAllHWReg();
		CALLFunc((u32)psxDelayTest);

		Return();
		return;
	}

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
	recBSC[psxRegs.code>>26]();
	break; 
	}
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
	ppcRec.branch = 1;
if(use_vm){
	psxRegs.code = PSXMu32(ppcRec.pc);}
else{
	psxRegs.code = PSXMu32_2(ppcRec.pc);}//teste
	ppcRec.pc+=4;

	if (iLoadTest() == 1) {
		iFlushRegs(0);
		LIW(0, psxRegs.code);
		STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));

		/* store cycle */
		iStoreCycle();

		LIW(R4, branchPC);
		LIW(R3, _Rt_);
		LIW(GetHWRegSpecial(PSXPC), ppcRec.pc);
		FlushAllHWReg();
		CALLFunc((u32)psxDelayTest);
                
		Return();
		return;
	}

	recBSC[psxRegs.code>>26]();

	iFlushRegs(branchPC);
	LIW(PutHWRegSpecial(PSXPC), branchPC);
	FlushAllHWReg();

	/* store cycle */
	iStoreCycle();

	FlushAllHWReg();
	CALLFunc((u32)psxBranchTest);

	/*
	if (!Config.HLE && Config.PsxOut &&
	    ((branchPC & 0x1fffff) == 0xa0 ||
	     (branchPC & 0x1fffff) == 0xb0 ||
	     (branchPC & 0x1fffff) == 0xc0))
	  CALLFunc((u32)psxJumpTest);
	*/
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

	if (savectx) {
		memcpy(iRegsS, iRegs, sizeof(iRegs));
		memcpy(HWRegistersS, HWRegisters, sizeof(HWRegisters));
		HWRegUseCountS = HWRegUseCount;
	}
	
	ppcRec.branch = 1;
if(use_vm){
	psxRegs.code = PSXMu32(ppcRec.pc);}
else{
	psxRegs.code = PSXMu32_2(ppcRec.pc);}//teste

	// the delay test is only made when the branch is taken
	// savectx == 0 will mean that :)
	if (savectx == 0 && iLoadTest() == 1) {
		iFlushRegs(0);
		LIW(0, psxRegs.code);
		STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));

		/* store cycle */
		iStoreCycle();
		
		LIW(R3, _Rt_);
		LIW(R4, branchPC);
        LIW(GetHWRegSpecial(PSXPC), ppcRec.pc);

		FlushAllHWReg();
		CALLFunc((u32)psxDelayTest);

		Return();
		return;
	}

	ppcRec.pc+= 4;
	recBSC[psxRegs.code>>26]();
	
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

	ppcRec.pc-= 4;

	// Restore 
	if (savectx) {
		memcpy(iRegs, iRegsS, sizeof(iRegs));
		memcpy(HWRegisters, HWRegistersS, sizeof(HWRegisters));
		HWRegUseCount = HWRegUseCountS;
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
/*	FILE *f;
	u32 i;

	SysPrintf("dump1 %x:%x, %x\n", psxRegs.pc, pc, psxCurrentCycle);

	for (i = psxRegs.pc; i < pc; i+=4)
		SysPrintf("%s\n", disR3000AF(PSXMu32(i), i));

	fflush(stdout);
	f = fopen("dump1", "w");
	fwrite(ptr, 1, (u32)x86Ptr - (u32)ptr, f);
	fclose(f);
	system("ndisasmw -u dump1");
	fflush(stdout);*/
}

#define REC_FUNC(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	LIW(R3, (u32)psxRegs.code); \
	STW(R3, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
	LIW(PutHWRegSpecial(PSXPC), (u32)ppcRec.pc); \
	FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
/*	branch = 2; */\
}

#define REC_SYS(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
        LIW(R3, (u32)psxRegs.code); \
        STW(R3, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
        LIW(PutHWRegSpecial(PSXPC), (u32)ppcRec.pc); \
        FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
	branch = 2; \
	iRet(); \
}

#define REC_BRANCH(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
        LIW(R3, (u32)psxRegs.code); \
        STW(R3, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
        LIW(PutHWRegSpecial(PSXPC), (u32)ppcRec.pc); \
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

	ppcRec.branch = 0;
	memset(iRegs, 0, sizeof(iRegs));
	iRegs[0].state = ST_CONST;
	iRegs[0].k     = 0;
}

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
    if(use_vm)
    recRun(*recFunc, (u32)&psxRegs, (u32)&psxM);
	else
    recRun(*recFunc, (u32)&psxRegs, (u32)&psxM_2);
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

	if (ptr != 0) {
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


#if 0
static void recDIV() {
    // Lo/Hi = Rs / Rt (signed)
    int usehi;

    if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
        MapConst(REG_LO, 0);
        MapConst(REG_HI, 0);
        return;
    }
    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(REG_LO, (s32) iRegs[_Rs_].k / (s32) iRegs[_Rt_].k);
        MapConst(REG_HI, (s32) iRegs[_Rs_].k % (s32) iRegs[_Rt_].k);
        return;
    }

    usehi = isPsxRegUsed(pc, REG_HI);

    if (IsConst(_Rt_)) {
        int shift = DoShift(iRegs[_Rt_].k);
        if (shift != -1) {
            SRAWI(PutHWReg32(REG_LO), GetHWReg32(_Rs_), shift);
            ADDZE(PutHWReg32(REG_LO), GetHWReg32(REG_LO));
            if (usehi) {
                RLWINM(PutHWReg32(REG_HI), GetHWReg32(_Rs_), 0, (31 - shift), 31);
            }
        } else if (iRegs[_Rt_].k == 3) {
            // http://the.wall.riscom.net/books/proc/ppc/cwg/code2.html
            LIS(PutHWReg32(REG_HI), 0x5555);
            ADDI(PutHWReg32(REG_HI), GetHWReg32(REG_HI), 0x5556);
            MULHW(PutHWReg32(REG_LO), GetHWReg32(REG_HI), GetHWReg32(_Rs_));
            SRWI(PutHWReg32(REG_HI), GetHWReg32(_Rs_), 31);
            ADD(PutHWReg32(REG_LO), GetHWReg32(REG_LO), GetHWReg32(REG_HI));
            if (usehi) {
                MULLI(PutHWReg32(REG_HI), GetHWReg32(REG_LO), 3);
                SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
            }
        } else {
            DIVW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            if (usehi) {
                if ((iRegs[_Rt_].k & 0x7fff) == iRegs[_Rt_].k) {
                    MULLI(PutHWReg32(REG_HI), GetHWReg32(REG_LO), iRegs[_Rt_].k);
                } else {
                    MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
                }
                SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
            }
        }
    } else {
        DIVW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        if (usehi) {
            MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
            SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
        }
    }
}

static void recDIVU() {
    // Lo/Hi = Rs / Rt (unsigned)
    int usehi;

    if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
        MapConst(REG_LO, 0);
        MapConst(REG_HI, 0);
        return;
    }
    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(REG_LO, (u32) iRegs[_Rs_].k / (u32) iRegs[_Rt_].k);
        MapConst(REG_HI, (u32) iRegs[_Rs_].k % (u32) iRegs[_Rt_].k);
        return;
    }

    usehi = isPsxRegUsed(pc, REG_HI);

    if (IsConst(_Rt_)) {
        int shift = DoShift(iRegs[_Rt_].k);
        if (shift != -1) {
            SRWI(PutHWReg32(REG_LO), GetHWReg32(_Rs_), shift);
            if (usehi) {
                RLWINM(PutHWReg32(REG_HI), GetHWReg32(_Rs_), 0, (31 - shift), 31);
            }
        } else {
            DIVWU(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            if (usehi) {
                MULLW(PutHWReg32(REG_HI), GetHWReg32(_Rt_), GetHWReg32(REG_LO));
                SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
            }
        }
    } else {
        DIVWU(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        if (usehi) {
            MULLW(PutHWReg32(REG_HI), GetHWReg32(_Rt_), GetHWReg32(REG_LO));
            SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
        }
    }
}
#elif 0 // + 2 fps in castlevania opening
static void recDIV() {
    // Lo/Hi = Rs / Rt (signed)
    int usehi;

    if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
        MapConst(REG_LO, 0);
        MapConst(REG_HI, 0);
        return;
    }
    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(REG_LO, (s32) iRegs[_Rs_].k / (s32) iRegs[_Rt_].k);
        MapConst(REG_HI, (s32) iRegs[_Rs_].k % (s32) iRegs[_Rt_].k);
        return;
    }
    
    usehi = isPsxRegUsed(pc, REG_HI);
    
    DIVW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    if (usehi) {
		MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
		SUBF(PutHWReg32(REG_HI), GetHWReg32(REG_HI), GetHWReg32(_Rs_));
	}
}

static void recDIVU() {
    // Lo/Hi = Rs / Rt (unsigned)
    int usehi;
     
    if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
        MapConst(REG_LO, 0);
        MapConst(REG_HI, 0);
        return;
    }
    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(REG_LO, (u32) iRegs[_Rs_].k / (u32) iRegs[_Rt_].k);
        MapConst(REG_HI, (u32) iRegs[_Rs_].k % (u32) iRegs[_Rt_].k);
        return;
    }
    
    usehi = isPsxRegUsed(pc, REG_HI);
    
	DIVWU(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
	if (usehi) {
		MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
		SUBF(PutHWReg32(REG_HI), GetHWReg32(REG_HI), GetHWReg32(_Rs_));
	}
}
#else
REC_FUNC(DIV);
REC_FUNC(DIVU);
#endif
//End of * Register mult/div & Register trap logic  

static u32 sioRead32() {
	u32 hard;
	hard = sioRead8();
	hard |= sioRead8() << 8;
	hard |= sioRead8() << 16;
	hard |= sioRead8() << 24;
	return hard;
}

static u16 sioRead16() {
	u16 hard;
	hard = sioRead8();
	hard |= sioRead8() << 8;
	return hard;
}

REC_FUNC(LWL);
REC_FUNC(LWR);
REC_FUNC(SWL);
REC_FUNC(SWR);

static void preMemRead()
{
	int rs;
	if (_Rs_ != _Rt_) {
		DisposeHWReg(iRegs[_Rt_].reg);
	}
	rs = GetHWReg32(_Rs_);
	if (rs != 3 || _Imm_ != 0) {
		ADDI(R3, rs, _Imm_);
	}
	if (_Rs_ == _Rt_) {
		DisposeHWReg(iRegs[_Rt_].reg);
	}
	InvalidateCPURegs();

	FlushAllHWReg();
}

static void preMemWrite(int size)
{
	int rs;
	rs = GetHWReg32(_Rs_);
	if (rs != 3 || _Imm_ != 0) {
		ADDI(R3, rs, _Imm_);
	}
	if (size == 1) {
		RLWINM(R4, GetHWReg32(_Rt_), 0, 24, 31);
		//ANDI_(R4, GetHWReg32(_Rt_), 0xff);
	} else if (size == 2) {
		RLWINM(R4, GetHWReg32(_Rt_), 0, 16, 31);
		//ANDI_(R4, GetHWReg32(_Rt_), 0xffff);
	} else {
		MR(R4, GetHWReg32(_Rt_));
	}

	InvalidateCPURegs();
	
	//FlushAllHWReg();
}

#define DR_REWRITE_CALL 1
#define DR_RECOMPILE_LOAD 1
#define DR_REWRITE_WRITE 0

enum LOAD_STORE_OPERATION {
	// Load
	REC_LB,
	REC_LBU,
	REC_LH,
	REC_LHU,
	REC_LW,
	// Store
	REC_SB,
	REC_SH,
	REC_SW
};

static u32 vmRead32Unchecked(u32 addr) {
	return (SWAP32(*(u32 *)&psxVM[addr]));
}

static void recompileLoad(enum LOAD_STORE_OPERATION operation) {
#if 1 // Not complete
	u32 * ptr;
	u32 * ptr_end;
	u32 RS;

	RS = GetHWReg32(_Rs_);
	
	// Add offset
	ADDI(R3, RS, _Imm_);

	// R3 = R3 & 0x1FFF.FFFF
	RLWINM(R3, R3, 0, 3, 31);

	// R4 = R3 >> 24
	SRWI(R4, R3, 24); 
	// Compare ...
	CMPLWI(R4, 0x1F);
	
#if 1 // this par fail
	// if R4 == 0x1F (Direct func call ...) go to ptr
	BEQ_L(ptr);
	
	//Break();

	// Really needed ? psxRegs.cycle += 1;
	// ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), 1);
	// ppcRec.count++;
#if 1
	// Direct memory access
	if (_Rt_) { // Load only if rt != r0
		switch(operation) {
		case REC_LB:
			LBZX(PutHWReg32(_Rt_), R14, R3);
			// rt = sign(rt)
			EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
			break;
		case REC_LBU:	
			LBZX(PutHWReg32(_Rt_), R14, R3);
			break;
		case REC_LH:
			LHBRX(PutHWReg32(_Rt_), R14, R3);
			// rt = sign(rt)
			EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
			break;
		case REC_LHU:
			LHBRX(PutHWReg32(_Rt_), R14, R3);
			break;
		case REC_LW:
			LWBRX(PutHWReg32(_Rt_), R14, R3);
			break;

		default:
			// Not made yet
			DebugBreak();
		}
	}
#endif

#if 0 // working ... only for rec lw
	//CALLFunc((u32)vmRead32Unchecked);
	//MR(PutHWReg32(_Rt_), R3);
	
	LWBRX(PutHWReg32(_Rt_), R14, R3);
	//MR(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
#endif
	// Jump to end
	B_L(ptr_end);

	// Set direct call jump address
	B_DST(ptr);
#endif

	// Direct call func ... todo optimise ...
	//preMemRead();
	//FlushAllHWReg();

	switch(operation) {
		case REC_LB:
		case REC_LBU:		
			CALLFunc((u32)psxMemRead8);
			break;
		case REC_LH:
		case REC_LHU:
			CALLFunc((u32)psxMemRead16);
			break;
		case REC_LW:
			CALLFunc((u32)psxMemRead32);
			break;
		default:
			// Not made yet
			DebugBreak();
	}

	// rt = (signed)rt;
	if (_Rt_) {
		switch(operation) {
		// need to sign extend
		case REC_LB:
			EXTSB(PutHWReg32(_Rt_), R3);
			break;
		case REC_LH:			
			EXTSH(PutHWReg32(_Rt_), R3);
			break;
		// Copy reg
		default:
			MR(PutHWReg32(_Rt_), R3);
			break;
		}
	}
#if 1
	// Set func end call jump adress
	B_DST(ptr_end);
#endif
	return;
#else
	preMemRead();
	switch(operation) {
		case REC_LB:
		case REC_LBU:		
			CALLFunc((u32)psxMemRead8);
			break;
		case REC_LH:
		case REC_LHU:
			CALLFunc((u32)psxMemRead16);
			break;
		case REC_LW:
			CALLFunc((u32)psxMemRead32);
			break;
		default:
			// Not made yet
			DebugBreak();
	}

	// rt = (signed)rt;
	if (_Rt_) {
		switch(operation) {
		// need to sign extend
		case REC_LB:
			EXTSB(PutHWReg32(_Rt_), R3);
			break;
		case REC_LH:			
			EXTSH(PutHWReg32(_Rt_), R3);
			break;
		// Copy reg
		default:
			MR(PutHWReg32(_Rt_), R3);
			break;
		}
	}
#endif
}


static void recLB() {
	u32 func;
if(use_vm){
	func = (u32) psxMemRead8;}
else{
	func = (u32) psxMemRead8_2;}//teste

if (use_vm){
	if (IsConst(_Rs_)  && 0) {
		u32 addr = iRegs[_Rs_].k + _Imm_;

		if (addr >= 0x1f801000 && addr <= 0x1f803000) {
			// direct call ?
			func = (u32)hw_read8_handler[addr&0xFFFF];
		} else {
			if (!_Rt_) {
				return;
			}
			LIW(PutHWReg32(_Rt_), (u32) & psxVM[addr & VM_MASK]);
			LBZ(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
			return;
		}
	} 
#if DR_RECOMPILE_LOAD
	else {
		recompileLoad(REC_LB);// crash
		return;
	}
#endif

	preMemRead();
	CALLFunc(func);
	if (_Rt_) {
		EXTSB(PutHWReg32(_Rt_), 3);
	}
  }
else{

	preMemRead();
	CALLFunc(func);
	if (_Rt_) {
		EXTSB(PutHWReg32(_Rt_), 3);
	}
  }
}

static void recLBU() {
	u32 func;
if(use_vm){
	func = (u32) psxMemRead8;}
else{
	func = (u32) psxMemRead8_2;}//teste

if(use_vm){

	if (IsConst(_Rs_)  && 0) {
		u32 addr = iRegs[_Rs_].k + _Imm_;

		if (addr >= 0x1f801000 && addr <= 0x1f803000) {
			// direct call ?
			func = (u32)hw_read8_handler[addr&0xFFFF];
		} else {
			if (!_Rt_) {
				return;
			}
			LIW(PutHWReg32(_Rt_), (u32) & psxVM[addr & VM_MASK]);
			LBZ(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			return;
		}
	}
#if DR_RECOMPILE_LOAD
	else {
		recompileLoad(REC_LBU);
		return;
	}
#endif

	preMemRead();
	CALLFunc(func);

	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
 }
else{
	preMemRead();
	CALLFunc(func);

	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
  }
}

static void recLH() {
	u32 func;
if(use_vm){
	func = (u32) psxMemRead16;}
else{
	func = (u32) psxMemRead16_2;}//teste

if(use_vm){
	if (IsConst(_Rs_)  && 0) {
		u32 addr = iRegs[_Rs_].k + _Imm_;

		if (addr >= 0x1f801000 && addr <= 0x1f803000) {
			// direct call ?
			func = (u32)hw_read16_handler[addr&0xFFFF];
		} else {
			if (!_Rt_) {
				return;
			}

			LIW(PutHWReg32(_Rt_), (u32) & psxVM[addr & VM_MASK]);
			LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
			return;
		}
	} 
#if DR_RECOMPILE_LOAD
	else {
		recompileLoad(REC_LH);
		return;
	}
#endif

	preMemRead();
	CALLFunc(func);
	if (_Rt_) {
		EXTSH(PutHWReg32(_Rt_), 3);
	}
  }
else{
		preMemRead();
	CALLFunc(func);
	if (_Rt_) {
		EXTSH(PutHWReg32(_Rt_), 3);
	}
  }
}

static void recLHU() {
	u32 func;
if(use_vm){
	func = (u32) psxMemRead16;}
else{
	func = (u32) psxMemRead16_2;}//teste

if(use_vm){
	if (IsConst(_Rs_)  && 0) {
		u32 addr = iRegs[_Rs_].k + _Imm_;

		if (addr >= 0x1f801000 && addr <= 0x1f803000) {			
			func = (u32)hw_read16_handler[addr&0xFFFF];
		} else {
			if (!_Rt_) {
				return;
			}
			LIW(PutHWReg32(_Rt_), (u32) & psxVM[addr & VM_MASK]);
			LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			return;
		}
	} 
#if DR_RECOMPILE_LOAD
	else {
		recompileLoad(REC_LHU);
		return;
	}
#endif

	preMemRead();
	CALLFunc(func);
	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
  }
else{
	preMemRead();
	CALLFunc(func);
	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
  }
}

static void recLW() {
	u32 func;
if(use_vm){
	func = (u32) psxMemRead32;}
else{
	func = (u32) psxMemRead32_2;}//teste

if(use_vm){
	if (IsConst(_Rs_)  && 0) {
		u32 addr = iRegs[_Rs_].k + _Imm_;

		if (addr >= 0x1f801000 && addr <= 0x1f803000) {
			func = (u32)hw_read32_handler[addr&0xFFFF];
		} else {
			if (!_Rt_) {
				return;
			}
			LIW(PutHWReg32(_Rt_), (u32) & psxVM[addr & VM_MASK]);
			LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			return;
		}
	} 
#if DR_RECOMPILE_LOAD
	else {
		recompileLoad(REC_LW);
		return;
	}
#endif

	preMemRead();
	CALLFunc(func);
	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
  }
else{
	preMemRead();
	CALLFunc(func);
	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
  }
}

// From psxvm.c
int writeok;

void recClear32(u32 addr) {
	recClear(addr, 1);
}

static void recSB() {	
	u32 func;
if(use_vm){
	func = (u32) psxMemWrite8;}
else{
	func = (u32) psxMemWrite8_2;}//teste

#if DR_REWRITE_WRITE
#ifdef _USE_VM
	if (IsConst(_Rs_)) {
		if (IsConst(_Rt_)) {
			u32 addr = iRegs[_Rs_].k + _Imm_;
			u16 t = addr >> 16;
			if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
				func = (u32)hw_write8_handler[addr&0xFFFF];
			} else if(writeok){
				//STB((u32) & psxVM[addr & VM_MASK], 0, GetHWReg32(_Rt_));
				//recClear((addr & (~3)), 1);
				//return;
			}
		}
	}
#endif
#endif
	preMemWrite(1);
	CALLFunc((u32) func);
}

static void recSH() {
	u32 func;
if(use_vm){
	func = (u32) psxMemWrite16;}
else{
	func = (u32) psxMemWrite16_2;}//teste

#if DR_REWRITE_WRITE
#ifdef _USE_VM
	if (IsConst(_Rs_)) {
		if (IsConst(_Rt_)) {
			u32 addr = iRegs[_Rs_].k + _Imm_;
			u16 t = addr >> 16;
			if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
				func = (u32)hw_write16_handler[addr&0xFFFF];
			} else if(writeok) {
				//STHBRX((u32) & psxVM[addr & VM_MASK], 0, GetHWReg32(_Rt_));
				//recClear((addr & (~3)), 1);
				//return;
			}
		}
	}
#endif
#endif
	preMemWrite(2);
	CALLFunc(func);
}

static void recSW() {
	u32 func;
if(use_vm){
	func = (u32) psxMemWrite32;}
else{
	func = (u32) psxMemWrite32_2;}//teste

#if DR_REWRITE_WRITE
#ifdef _USE_VM
	if (IsConst(_Rs_)) {
		if (IsConst(_Rt_)) {
			u32 addr = iRegs[_Rs_].k + _Imm_;hghghg
			u16 t = addr >> 16;
			
			if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
				/*
				int reg = PutHWReg32(_Rt_);

				LIW(reg, (u32) & psxVM[addr & VM_MASK]);
				STWBRX(PutHWReg32(_Rt_), 0, reg);

				preMemRead();
				CALLFunc((u32)recClear32);
				return;
				*/
			}
			
			if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
				func = (u32)hw_write32_handler[addr&0xFFFF];
			} else if(writeok ) {
				if (addr != 0xfffe0130) {
					//STWBRX((u32) & psxVM[addr & VM_MASK], 0, GetHWReg32(_Rt_));
					//recClear(addr, 1);
					//return;
				}
			}
		}
	}
#endif
#endif
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

#if 1
static void recSYSCALL() {
    //	dump=1;
    iFlushRegs(0);

    LIW(PutHWRegSpecial(PSXPC), ppcRec.pc - 4);
    LIW(3, 0x20);
    LIW(4, (ppcRec.branch == 1 ? 1 : 0));
    FlushAllHWReg();
    CALLFunc((u32) psxException);
    
    ppcRec.branch = 2;
    iRet();
}

static void recBREAK() {
}
#else
REC_SYS(SYSCALL);
REC_SYS(BREAK);
#endif

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

#if 0
REC_BRANCH(BEQ);     // *FIXME
REC_BRANCH(BNE);     // *FIXME
#else
static void recBEQ() {
    // Branch if Rs == Rt
    u32 bpc = _Imm_ * 4 + ppcRec.pc;
    u32 *b;
	/*
    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    */
    if (_Rs_ == _Rt_) {
        iJump(bpc);
    } else {
        if (IsConst(_Rs_) && IsConst(_Rt_)) {
            if (iRegs[_Rs_].k == iRegs[_Rt_].k) {
                iJump(bpc);
                return;
            } else {
                iJump(ppcRec.pc + 4);
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

        iBranch(ppcRec.pc + 4, 1);

        B_DST(b);

        iBranch(bpc, 0);
        ppcRec.pc += 4;
    }
}

static void recBNE() {
    // Branch if Rs != Rt
    u32 bpc = _Imm_ * 4 + ppcRec.pc;
    u32 *b;
	/*
    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    */
    if (_Rs_ == _Rt_) {
        iJump(ppcRec.pc + 4);
    } else {
        if (IsConst(_Rs_) && IsConst(_Rt_)) {
            if (iRegs[_Rs_].k != iRegs[_Rt_].k) {
                iJump(bpc);
                return;
            } else {
                iJump(ppcRec.pc + 4);
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

        iBranch(ppcRec.pc + 4, 1);

        B_DST(b);

        iBranch(bpc, 0);
        ppcRec.pc += 4;
    }
}
#endif

static void recBLTZ() {
    // Branch if Rs < 0
    u32 bpc = _Imm_ * 4 + ppcRec.pc;
    u32 *b;
	/*
    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    */
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k < 0) {
            iJump(bpc);
            return;
        } else {
            iJump(ppcRec.pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLT_L(b);

    iBranch(ppcRec.pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    ppcRec.pc += 4;
}

static void recBGTZ() {
    // Branch if Rs > 0
    u32 bpc = _Imm_ * 4 + ppcRec.pc;
    u32 *b;
	/*
    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    */
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k > 0) {
            iJump(bpc);
            return;
        } else {
            iJump(ppcRec.pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGT_L(b);

    iBranch(ppcRec.pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    ppcRec.pc += 4;
}

static void recBLTZAL() {
    // Branch if Rs < 0
    u32 bpc = _Imm_ * 4 + ppcRec.pc;
    u32 *b;
	/*
    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    */
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k < 0) {
            MapConst(31, ppcRec.pc + 4);

            iJump(bpc);
            return;
        } else {
            iJump(ppcRec.pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLT_L(b);

    iBranch(ppcRec.pc + 4, 1);

    B_DST(b);

    MapConst(31, ppcRec.pc + 4);

    iBranch(bpc, 0);
    ppcRec.pc += 4;
}

static void recBGEZAL() {
    // Branch if Rs >= 0
    u32 bpc = _Imm_ * 4 + ppcRec.pc;
    u32 *b;
	/*
    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    */
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k >= 0) {
            MapConst(31, ppcRec.pc + 4);

            iJump(bpc);
            return;
        } else {
            iJump(ppcRec.pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGE_L(b);

    iBranch(ppcRec.pc + 4, 1);

    B_DST(b);

    MapConst(31, ppcRec.pc + 4);

    iBranch(bpc, 0);
    ppcRec.pc += 4;
}

static void recJ() {
    // j target

    iJump(_Target_ * 4 + (ppcRec.pc & 0xf0000000));
}

static void recJAL() {
    // jal target
    MapConst(31, ppcRec.pc + 4);

    iJump(_Target_ * 4 + (ppcRec.pc & 0xf0000000));
}

static void recJR() {
    // jr Rs

    if (IsConst(_Rs_)) {
        iJump(iRegs[_Rs_].k);
        //LIW(PutHWRegSpecial(TARGET), iRegs[_Rs_].k);
    } else {
        MR(PutHWRegSpecial(TARGET), GetHWReg32(_Rs_));
        SetBranch();
    }
}

static void recJALR() {
    // jalr Rs

    if (_Rd_) {
        MapConst(_Rd_, ppcRec.pc + 4);
    }

    if (IsConst(_Rs_)) {
        iJump(iRegs[_Rs_].k);
        //LIW(PutHWRegSpecial(TARGET), iRegs[_Rs_].k);
    } else {
        MR(PutHWRegSpecial(TARGET), GetHWReg32(_Rs_));
        SetBranch();
    }
}


static void recBLEZ() {
    // Branch if Rs <= 0
    u32 bpc = _Imm_ * 4 + ppcRec.pc;
    u32 *b;
	/*
    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    */
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k <= 0) {
            iJump(bpc);
            return;
        } else {
            iJump(ppcRec.pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLE_L(b);

    iBranch(ppcRec.pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    ppcRec.pc += 4;
}

static void recBGEZ() {
    // Branch if Rs >= 0
    u32 bpc = _Imm_ * 4 + ppcRec.pc;
    u32 *b;
	/*
    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    */
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k >= 0) {
            iJump(bpc);
            return;
        } else {
            iJump(ppcRec.pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGE_L(b);

    iBranch(ppcRec.pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    ppcRec.pc += 4;
}

// Return from exception
static void recRFE() {
    
    iFlushRegs(0);
    LWZ(0, OFFSET(&psxRegs, &psxRegs.CP0.n.Status), GetHWRegSpecial(PSXREGS));
    RLWINM(11, 0, 0, 0, 27);
    RLWINM(0, 0, 30, 28, 31);
    OR(0, 0, 11);
    STW(0, OFFSET(&psxRegs, &psxRegs.CP0.n.Status), GetHWRegSpecial(PSXREGS));

#if 1 // not needed ?
    //LIW(PutHWRegSpecial(PSXPC), (u32)pc);
    LIW(PutHWRegSpecial(PSXPC), (u32) ppcRec.pc);
    FlushAllHWReg();
    if (ppcRec.branch == 0) {
        ppcRec.branch = 2;
        iRet();
    }
#endif
}

static void recMFC0() {
	/** todo ... load delay here **/


    // Rt = Cop0->Rd
    if (!_Rt_) return;

    LWZ(PutHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
}

static void recCFC0() {
    // Rt = Cop0->Rd

    recMFC0();
}

/** todo recompile this function **/
static void recTestSWInts() {
#if 1
	iFlushRegs(0);
    LIW(PutHWRegSpecial(PSXPC), (u32) ppcRec.pc);
    FlushAllHWReg();
    CALLFunc((u32) psxTestSWInts);

    if (ppcRec.branch == 0) {
        ppcRec.branch = 2;
        iRet();
    }
#else
	// Port that ...
	if (psxRegs.CP0.n.Cause & psxRegs.CP0.n.Status & 0x0300 &&
		psxRegs.CP0.n.Status & 0x1) {
		psxException(psxRegs.CP0.n.Cause, branch);
	}
#endif
}


static void recMTC0() {
    switch (_Rd_) {
        case 12: // Status
            STW(GetHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
			recTestSWInts();
            break;
        case 13:
            RLWINM(0, GetHWReg32(_Rt_), 0, 22, 15); // & ~(0xfc00)
            STW(0, OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
			recTestSWInts();
            break;
		default:
			STW(GetHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
            break;
    }
}

static void recCTC0() {
    recMTC0();
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

    //CALLFunc((u32) psxHLEt[psxRegs.code & 0xffff]);
    CALLFunc((u32) psxHLEt[psxRegs.code & 0x7f]);
    ppcRec.branch = 2;
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
	HWRegUseCount = 0;
	SetDstCPUReg(-1);
	memset(HWRegisters, 0, sizeof(HWRegisters));
	for (i=0; i<NUM_HW_REGISTERS; i++)
		HWRegisters[i].code = cpuHWRegisters[NUM_HW_REGISTERS-i-1];
	
	// reserve the special psxReg register
	HWRegisters[0].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
	HWRegisters[0].reg = PSXREGS;
	HWRegisters[0].k = (u32)&psxRegs;

	HWRegisters[1].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
	HWRegisters[1].reg = PSXMEM;
	HWRegisters[1].k = (u32)&psxM;

	// reserve the special psxRegs.cycle register
	//HWRegisters[1].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
	//HWRegisters[1].private = CYCLECOUNT;
	
	//memset(iRegs, 0, sizeof(iRegs));
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

	ppcRec.pcold = ppcRec.pc = psxRegs.pc;

    if(use_vm)// Save vm ptr
	MOVI2R(R14, (u32)psxVM);
	
	// Build dynarec cache
	for (ppcRec.count=0; ppcRec.count<500;) {
		u32 adr;
		u32 op;
		if(use_vm)
		adr = ppcRec.pc & VM_MASK;
		else
        adr = ppcRec.pc & 0x1fffff;

		if(use_vm)
		op = (char*)PSXM(ppcRec.pc);
		else
        op = (char*)PSXM_2(ppcRec.pc);

		psxRegs.code = SWAP32(*(u32 *)p);

		ppcRec.pc+=4; 
		ppcRec.count++;

		op = psxRegs.code>>26;

		recBSC[op]();

		// branch hit finish
		if (ppcRec.branch) {
			ppcRec.branch = 0;
			goto done;
		}
	}

	iFlushRegs(ppcRec.pc);
	
	LIW(PutHWRegSpecial(PSXPC), ppcRec.pc);

	iRet();

done:;
#if 0
	MakeDataExecutable(ptr, ((u8*)ppcPtr)-((u8*)ptr));
#else

	// Flush data to be executable
	a = (u32)(u8*)ptr;
	while(a < (u32)(u8*)ppcPtr) {
		__icbi(0, (void*)a);
		__dcbst(0, (void*)a);
	  a += 4;
	}
	__emit(0x7c0004ac);//sync
	__emit(0x4C00012C);//isync
#endif
	
#if 1
	sprintf((char *)ppcPtr, "PC=%08x", ppcRec.pcold);
	ppcPtr += strlen((char *)ppcPtr);
#endif
}


R3000Acpu psxRec = {
	recInit,
	recReset,
	recExecute,
	recExecuteBlock,
	recClear,
	recShutdown
};

