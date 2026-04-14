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
 * PSX assembly interpreter.
 */

#include "psxcommon.h"
#include "r3000a.h"
#include "gte.h"
#include "psxhle.h"


static int branch = 0;
static int branch2 = 0;
static u32 branchPC;

// use swith instead of array func
// #define XB_OPTI

// These macros are used to assemble the repassembler functions

#ifdef PSXCPU_LOG
#define debugI() PSXCPU_LOG("%s\n", disR3000AF(psxRegs.code, psxRegs.pc)); 
#else
#define debugI()
#endif

__inline void execI();

#ifdef XB_OPTI

__inline void CallPsxBsc(int func_code);
__inline void callPsxSPC(int func_code);
__inline void CallPsxREG(int func_code);
__inline void callPsxCP0(int func_code);
__inline void callPsxCP2(int func_code);
__inline void callPsxCP2BSC(int func_code);

#else

// Subsets
void (*psxBSC[64])();
void (*psxSPC[64])();
void (*psxREG[32])();
void (*psxCP0[32])();
void (*psxCP2[64])();
void (*psxCP2BSC[32])();

#endif

static void delayRead(int reg, u32 bpc) {
	u32 rold, rnew;

//	SysPrintf("delayRead at %x!\n", psxRegs.pc);

	rold = psxRegs.GPR.r[reg];
#ifndef XB_OPTI
	psxBSC[psxRegs.code >> 26](); // branch delay load
#else
	CallPsxBsc(psxRegs.code >> 26);
#endif
	rnew = psxRegs.GPR.r[reg];

	psxRegs.pc = bpc;

	psxBranchTest();

	psxRegs.GPR.r[reg] = rold;
	execI(); // first branch opcode
	psxRegs.GPR.r[reg] = rnew;

	branch = 0;
}

static void delayWrite(int reg, u32 bpc) {

/*	SysPrintf("delayWrite at %x!\n", psxRegs.pc);

	SysPrintf("%s\n", disR3000AF(psxRegs.code, psxRegs.pc-4));
	SysPrintf("%s\n", disR3000AF(PSXMu32(bpc), bpc));*/

	// no changes from normal behavior
#ifndef XB_OPTI
	psxBSC[psxRegs.code >> 26]();
#else
	CallPsxBsc(psxRegs.code >> 26);
#endif
	branch = 0;
	psxRegs.pc = bpc;

	psxBranchTest();
}

static void delayReadWrite(int reg, u32 bpc) {

//	SysPrintf("delayReadWrite at %x!\n", psxRegs.pc);

	// the branch delay load is skipped

	branch = 0;
	psxRegs.pc = bpc;

	psxBranchTest();
}

// this defines shall be used with the tmp 
// of the next func (instead of _Funct_...)
#define _tFunct_  ((tmp      ) & 0x3F)  // The funct part of the instruction register 
#define _tRd_     ((tmp >> 11) & 0x1F)  // The rd part of the instruction register 
#define _tRt_     ((tmp >> 16) & 0x1F)  // The rt part of the instruction register 
#define _tRs_     ((tmp >> 21) & 0x1F)  // The rs part of the instruction register 
#define _tSa_     ((tmp >>  6) & 0x1F)  // The sa part of the instruction register

int psxTestLoadDelay(int reg, u32 tmp) {
	if (tmp == 0) return 0; // NOP
	switch (tmp >> 26) {
		case 0x00: // SPECIAL
			switch (_tFunct_) {
				case 0x00: // SLL
				case 0x02: case 0x03: // SRL/SRA
					if (_tRd_ == reg && _tRt_ == reg) return 1; else
					if (_tRt_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				case 0x08: // JR
					if (_tRs_ == reg) return 2;
					break;
				case 0x09: // JALR
					if (_tRd_ == reg && _tRs_ == reg) return 1; else
					if (_tRs_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				// SYSCALL/BREAK just a break;

				case 0x20: case 0x21: case 0x22: case 0x23:
				case 0x24: case 0x25: case 0x26: case 0x27: 
				case 0x2a: case 0x2b: // ADD/ADDU...
				case 0x04: case 0x06: case 0x07: // SLLV...
					if (_tRd_ == reg && (_tRt_ == reg || _tRs_ == reg)) return 1; else
					if (_tRt_ == reg || _tRs_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				case 0x10: case 0x12: // MFHI/MFLO
					if (_tRd_ == reg) return 3;
					break;
				case 0x11: case 0x13: // MTHI/MTLO
					if (_tRs_ == reg) return 2;
					break;

				case 0x18: case 0x19:
				case 0x1a: case 0x1b: // MULT/DIV...
					if (_tRt_ == reg || _tRs_ == reg) return 2;
					break;
			}
			break;

		case 0x01: // REGIMM
			switch (_tRt_) {
				case 0x00: case 0x01:
				case 0x10: case 0x11: // BLTZ/BGEZ...
					// Xenogears - lbu v0 / beq v0
					// - no load delay (fixes battle loading)
					break;

					if (_tRs_ == reg) return 2;
					break;
			}
			break;

		// J would be just a break;
		case 0x03: // JAL
			if (31 == reg) return 3;
			break;

		case 0x04: case 0x05: // BEQ/BNE
			// Xenogears - lbu v0 / beq v0
			// - no load delay (fixes battle loading)
			break;

			if (_tRs_ == reg || _tRt_ == reg) return 2;
			break;

		case 0x06: case 0x07: // BLEZ/BGTZ
			// Xenogears - lbu v0 / beq v0
			// - no load delay (fixes battle loading)
			break;

			if (_tRs_ == reg) return 2;
			break;

		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: // ADDI/ADDIU...
			if (_tRt_ == reg && _tRs_ == reg) return 1; else
			if (_tRs_ == reg) return 2; else
			if (_tRt_ == reg) return 3;
			break;

		case 0x0f: // LUI
			if (_tRt_ == reg) return 3;
			break;

		case 0x10: // COP0
			switch (_tFunct_) {
				case 0x00: // MFC0
					if (_tRt_ == reg) return 3;
					break;
				case 0x02: // CFC0
					if (_tRt_ == reg) return 3;
					break;
				case 0x04: // MTC0
					if (_tRt_ == reg) return 2;
					break;
				case 0x06: // CTC0
					if (_tRt_ == reg) return 2;
					break;
				// RFE just a break;
			}
			break;

		case 0x12: // COP2
			switch (_tFunct_) {
				case 0x00: 
					switch (_tRs_) {
						case 0x00: // MFC2
							if (_tRt_ == reg) return 3;
							break;
						case 0x02: // CFC2
							if (_tRt_ == reg) return 3;
							break;
						case 0x04: // MTC2
							if (_tRt_ == reg) return 2;
							break;
						case 0x06: // CTC2
							if (_tRt_ == reg) return 2;
							break;
					}
					break;
				// RTPS... break;
			}
			break;

		case 0x22: case 0x26: // LWL/LWR
			if (_tRt_ == reg) return 3; else
			if (_tRs_ == reg) return 2;
			break;

		case 0x20: case 0x21: case 0x23:
		case 0x24: case 0x25: // LB/LH/LW/LBU/LHU
			if (_tRt_ == reg && _tRs_ == reg) return 1; else
			if (_tRs_ == reg) return 2; else
			if (_tRt_ == reg) return 3;
			break;

		case 0x28: case 0x29: case 0x2a:
		case 0x2b: case 0x2e: // SB/SH/SWL/SW/SWR
			if (_tRt_ == reg || _tRs_ == reg) return 2;
			break;

		case 0x32: case 0x3a: // LWC2/SWC2
			if (_tRs_ == reg) return 2;
			break;
	}

	return 0;
}

void psxDelayTest(int reg, u32 bpc) {
	u32 *code;
	u32 tmp;

	// Don't execute yet - just peek
	code = (u32 *)PSXM_2(bpc);

	tmp = SWAP32(*code);
	branch = 1;

	switch (psxTestLoadDelay(reg, tmp)) {
		case 1:
			delayReadWrite(reg, bpc); return;
		case 2:
			delayRead(reg, bpc); return;
		case 3:
			delayWrite(reg, bpc); return;
	}
#ifndef XB_OPTI
	psxBSC[psxRegs.code >> 26]();
#else
	CallPsxBsc(psxRegs.code >> 26);
#endif
	branch = 0;
	psxRegs.pc = bpc;

	psxBranchTest();
}

__inline void doBranch(u32 tar) {
	u32 *code;
	u32 tmp;

	branch2 = branch = 1;
	branchPC = tar;

	// branch delay slot
	code = (u32 *)PSXM_2(psxRegs.pc);

	psxRegs.code = SWAP32(*code);

//	debugI();

	psxRegs.pc += 4;
	psxRegs.cycle += BIAS;

	// check for load delay
	tmp = psxRegs.code >> 26;
	switch (tmp) {
		case 0x10: // COP0
			switch (_Rs_) {
				case 0x00: // MFC0
				case 0x02: // CFC0
					psxDelayTest(_Rt_, branchPC);
					return;
			}
			break;
		case 0x12: // COP2
			switch (_Funct_) {
				case 0x00:
					switch (_Rs_) {
						case 0x00: // MFC2
						case 0x02: // CFC2
							psxDelayTest(_Rt_, branchPC);
							return;
					}
					break;
			}
			break;
		case 0x32: // LWC2
			psxDelayTest(_Rt_, branchPC);
			return;
		default:
			if (tmp >= 0x20 && tmp <= 0x26) { // LB/LH/LWL/LW/LBU/LHU/LWR
				psxDelayTest(_Rt_, branchPC);
				return;
			}
			break;
	}

#ifndef XB_OPTI
	psxBSC[psxRegs.code >> 26]();
#else
	CallPsxBsc(psxRegs.code >> 26);
#endif

	branch = 0;
	psxRegs.pc = branchPC;

	psxBranchTest();
}

/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

#if 0
void psxADDI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }		// Rt = Rs + Im 	(Exception on Integer Overflow)
void psxADDIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }		// Rt = Rs + Im
void psxANDI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) & _ImmU_; }		// Rt = Rs And Im
void psxORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) | _ImmU_; }		// Rt = Rs Or  Im
void psxXORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) ^ _ImmU_; }		// Rt = Rs Xor Im
void psxSLTI() 	{ if (!_Rt_) return; _rRt_ = _i32(_rRs_) < _Imm_ ; }		// Rt = Rs < Im		(Signed)
void psxSLTIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) < ((u32)_Imm_); }		// Rt = Rs < Im		(Unsigned)
#else

__declspec( naked ) int asmADDI( int x, int y ) {
	__asm 
	{
		// x is in r3
		// y is in r4
		add r3,r3,r4
		// The return value is in r3
		blr // Don’t forget the explicit ‘blr’
	}
}

// Rt = Rs + Im 	(Exception on Integer Overflow)
void psxADDI() 	{ 
	if (!_Rt_) 
		return; 
	//_rRt_ = _u32(_rRs_) + _Imm_ ; 
	_rRt_ = asmADDI(_u32(_rRs_),_Imm_);
}

// Rt = Rs + Im
void psxADDIU() { 
	if (!_Rt_) 
		return; 
	//_rRt_ = _u32(_rRs_) + _Imm_ ; 
	_rRt_ = asmADDI(_u32(_rRs_),_Imm_);
}

void psxANDI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) & _ImmU_; }		// Rt = Rs And Im
void psxORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) | _ImmU_; }		// Rt = Rs Or  Im
void psxXORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) ^ _ImmU_; }		// Rt = Rs Xor Im
void psxSLTI() 	{ if (!_Rt_) return; _rRt_ = _i32(_rRs_) < _Imm_ ; }		// Rt = Rs < Im		(Signed)
void psxSLTIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) < ((u32)_Imm_); }		// Rt = Rs < Im		(Unsigned)
#endif
/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
void psxADD()	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }	// Rd = Rs + Rt		(Exception on Integer Overflow)
void psxADDU() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }	// Rd = Rs + Rt
void psxSUB() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }	// Rd = Rs - Rt		(Exception on Integer Overflow)
void psxSUBU() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }	// Rd = Rs - Rt
void psxAND() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) & _u32(_rRt_); }	// Rd = Rs And Rt
void psxOR() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) | _u32(_rRt_); }	// Rd = Rs Or  Rt
void psxXOR() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) ^ _u32(_rRt_); }	// Rd = Rs Xor Rt
void psxNOR() 	{ if (!_Rd_) return; _rRd_ =~(_u32(_rRs_) | _u32(_rRt_)); }// Rd = Rs Nor Rt
void psxSLT() 	{ if (!_Rd_) return; _rRd_ = _i32(_rRs_) < _i32(_rRt_); }	// Rd = Rs < Rt		(Signed)
void psxSLTU() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) < _u32(_rRt_); }	// Rd = Rs < Rt		(Unsigned)

/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
void psxDIV() {
	if (_i32(_rRt_) != 0) {
		_i32(_rLo_) = _i32(_rRs_) / _i32(_rRt_);
		_i32(_rHi_) = _i32(_rRs_) % _i32(_rRt_);
	}
	else {
		_i32(_rLo_) = 0xffffffff;
		_i32(_rHi_) = _i32(_rRs_);
	}
}

void psxDIVU() {
	if (_rRt_ != 0) {
		_rLo_ = _rRs_ / _rRt_;
		_rHi_ = _rRs_ % _rRt_;
	}
	else {
		_rLo_ = 0xffffffff;
		_rHi_ = _rRs_;
	}
}

void psxMULT() {
	u64 res = (s64)((s64)_i32(_rRs_) * (s64)_i32(_rRt_));

	psxRegs.GPR.n.lo = (u32)(res & 0xffffffff);
	psxRegs.GPR.n.hi = (u32)((res >> 32) & 0xffffffff);
}

void psxMULTU() {
	u64 res = (u64)((u64)_u32(_rRs_) * (u64)_u32(_rRt_));

	psxRegs.GPR.n.lo = (u32)(res & 0xffffffff);
	psxRegs.GPR.n.hi = (u32)((res >> 32) & 0xffffffff);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/
#define RepZBranchi32(op)      if(_i32(_rRs_) op 0) doBranch(_BranchTarget_);
#define RepZBranchLinki32(op)  if(_i32(_rRs_) op 0) { _SetLink(31); doBranch(_BranchTarget_); }

void psxBGEZ()   { RepZBranchi32(>=) }      // Branch if Rs >= 0
void psxBGEZAL() { RepZBranchLinki32(>=) }  // Branch if Rs >= 0 and link
void psxBGTZ()   { RepZBranchi32(>) }       // Branch if Rs >  0
void psxBLEZ()   { RepZBranchi32(<=) }      // Branch if Rs <= 0
void psxBLTZ()   { RepZBranchi32(<) }       // Branch if Rs <  0
void psxBLTZAL() { RepZBranchLinki32(<) }   // Branch if Rs <  0 and link

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
void psxSLL() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) << _Sa_; } // Rd = Rt << sa
void psxSRA() { if (!_Rd_) return; _i32(_rRd_) = _i32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (arithmetic)
void psxSRL() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (logical)

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/
void psxSLLV() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) << _u32(_rRs_); } // Rd = Rt << rs
void psxSRAV() { if (!_Rd_) return; _i32(_rRd_) = _i32(_rRt_) >> _u32(_rRs_); } // Rd = Rt >> rs (arithmetic)
void psxSRLV() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) >> _u32(_rRs_); } // Rd = Rt >> rs (logical)

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
void psxLUI() { if (!_Rt_) return; _u32(_rRt_) = psxRegs.code << 16; } // Upper halfword of Rt = Im

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
void psxMFHI() { if (!_Rd_) return; _rRd_ = _rHi_; } // Rd = Hi
void psxMFLO() { if (!_Rd_) return; _rRd_ = _rLo_; } // Rd = Lo

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
void psxMTHI() { _rHi_ = _rRs_; } // Hi = Rs
void psxMTLO() { _rLo_ = _rRs_; } // Lo = Rs

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
void psxBREAK() {
	// Break exception - psx rom doens't handles this
}

void psxSYSCALL() {
	psxRegs.pc -= 4;
	psxException(0x20, branch);
}

void psxRFE() {
//	SysPrintf("psxRFE\n");
	psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status & 0xfffffff0) |
						  ((psxRegs.CP0.n.Status & 0x3c) >> 2);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#define RepBranchi32(op)      if(_i32(_rRs_) op _i32(_rRt_)) doBranch(_BranchTarget_);

void psxBEQ() {	RepBranchi32(==) }  // Branch if Rs == Rt
void psxBNE() {	RepBranchi32(!=) }  // Branch if Rs != Rt

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
void psxJ()   {               doBranch(_JumpTarget_); }
void psxJAL() {	_SetLink(31); doBranch(_JumpTarget_); }

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void psxJR()   {
	doBranch(_u32(_rRs_));
	psxJumpTest();
}

void psxJALR() {
	u32 temp = _u32(_rRs_);
	if (_Rd_) { _SetLink(_Rd_); }
	doBranch(temp);
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

#define _oB_ (_u32(_rRs_) + _Imm_)

void psxLB() {
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}



	if (_Rt_) {
		_i32(_rRt_) = (signed char)psxMemRead8_2(_oB_); 
	} else {
		psxMemRead8_2(_oB_); 
	}
}

void psxLBU() {
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}



	if (_Rt_) {
		_u32(_rRt_) = psxMemRead8_2(_oB_);
	} else {
		psxMemRead8_2(_oB_); 
	}
}

void psxLH() {
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}



	if (_Rt_) {
		_i32(_rRt_) = (short)psxMemRead16_2(_oB_);
	} else {
		psxMemRead16_2(_oB_);
	}
}

void psxLHU() {
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}



	if (_Rt_) {
		_u32(_rRt_) = psxMemRead16_2(_oB_);
	} else {
		psxMemRead16_2(_oB_);
	}
}

void psxLW() {
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}



	if (_Rt_) {
		_u32(_rRt_) = psxMemRead32_2(_oB_);
	} else {
		psxMemRead32_2(_oB_);
	}
}

u32 LWL_MASK[4] = { 0xffffff, 0xffff, 0xff, 0 };
u32 LWL_SHIFT[4] = { 24, 16, 8, 0 };

void psxLWL() {
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32_2(addr & ~3);


	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}


	if (!_Rt_) return;
	_u32(_rRt_) =	( _u32(_rRt_) & LWL_MASK[shift]) | 
					( mem << LWL_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   4bcd   (mem << 24) | (reg & 0x00ffffff)
	1   34cd   (mem << 16) | (reg & 0x0000ffff)
	2   234d   (mem <<  8) | (reg & 0x000000ff)
	3   1234   (mem      ) | (reg & 0x00000000)
	*/
}

u32 LWR_MASK[4] = { 0, 0xff000000, 0xffff0000, 0xffffff00 };
u32 LWR_SHIFT[4] = { 0, 8, 16, 24 };

void psxLWR() {
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32_2(addr & ~3);
		
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}



	if (!_Rt_) return;
	_u32(_rRt_) =	( _u32(_rRt_) & LWR_MASK[shift]) | 
					( mem >> LWR_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)
	*/
}

void psxSB() { psxMemWrite8_2 (_oB_, _u8 (_rRt_)); }
void psxSH() { psxMemWrite16_2(_oB_, _u16(_rRt_)); }
void psxSW() { psxMemWrite32_2(_oB_, _u32(_rRt_)); }

u32 SWL_MASK[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0 };
u32 SWL_SHIFT[4] = { 24, 16, 8, 0 };

void psxSWL() {
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32_2(addr & ~3);

	psxMemWrite32_2(addr & ~3,  (_u32(_rRt_) >> SWL_SHIFT[shift]) |
			     (  mem & SWL_MASK[shift]) );
	/*
	Mem = 1234.  Reg = abcd

	0   123a   (reg >> 24) | (mem & 0xffffff00)
	1   12ab   (reg >> 16) | (mem & 0xffff0000)
	2   1abc   (reg >>  8) | (mem & 0xff000000)
	3   abcd   (reg      ) | (mem & 0x00000000)
	*/
}

u32 SWR_MASK[4] = { 0, 0xff, 0xffff, 0xffffff };
u32 SWR_SHIFT[4] = { 0, 8, 16, 24 };

void psxSWR() {
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 dest = addr & ~3;
	u32 mem = psxMemRead32_2(dest);

	psxMemWrite32_2(dest,  (_u32(_rRt_) << SWR_SHIFT[shift]) |
			     (  mem & SWR_MASK[shift]) );

	/*
	Mem = 1234.  Reg = abcd

	0   abcd   (reg      ) | (mem & 0x00000000)
	1   bcd4   (reg <<  8) | (mem & 0x000000ff)
	2   cd34   (reg << 16) | (mem & 0x0000ffff)
	3   d234   (reg << 24) | (mem & 0x00ffffff)
	*/
}

/*********************************************************
* Moves between GPR and COPx                             *
* Format:  OP rt, fs                                     *
*********************************************************/
void psxMFC0()
{
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}


	if (!_Rt_) return;
	
	_i32(_rRt_) = (int)_rFs_;
}

void psxCFC0()
{
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}


	if (!_Rt_) 
		return;
	
	_i32(_rRt_) = (int)_rFs_;
}

void psxTestSWInts() {
	// the next code is untested, if u know please
	// tell me if it works ok or not (linuzappz)
	if (psxRegs.CP0.n.Cause & psxRegs.CP0.n.Status & 0x0300 &&
		psxRegs.CP0.n.Status & 0x1) {
		psxException(psxRegs.CP0.n.Cause, branch);
	}
}

__inline void MTC0(int reg, u32 val) {
//	SysPrintf("MTC0 %d: %x\n", reg, val);
	switch (reg) {
		case 12: // Status
			psxRegs.CP0.r[12] = val;
			psxTestSWInts();
			break;

		case 13: // Cause
			psxRegs.CP0.n.Cause = val & ~(0xfc00);
			psxTestSWInts();
			break;

		default:
			psxRegs.CP0.r[reg] = val;
			break;
	}
}

void psxMTC0() { MTC0(_Rd_, _u32(_rRt_)); }
void psxCTC0() { MTC0(_Rd_, _u32(_rRt_)); }



void psxMFC2()
{
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}

	gteMFC2();
}


void psxCFC2()
{
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}

	gteCFC2();
}


/*********************************************************
* Unknow instruction (would generate an exception)       *
* Format:  ?                                             *
*********************************************************/
void psxNULL() { 
#ifdef PSXCPU_LOG
	PSXCPU_LOG("psx: Unimplemented op %x\n", psxRegs.code);
#endif
}

void psxSPECIAL() {
	// psxSPC[_Funct_]();
#ifndef XB_OPTI
	psxSPC[_Funct_]();
#else
	callPsxSPC(_Funct_);
#endif
}

void psxREGIMM() {
#ifndef XB_OPTI
	psxREG[_Rt_](); // branch delay load
#else
	CallPsxREG(_Rt_);
#endif
}

void psxCOP0() {
#ifndef XB_OPTI
	psxCP0[_Rs_]();
#else
	callPsxCP0(_Rs_);
#endif

}

void psxCOP2() {
	if ((psxRegs.CP0.n.Status & 0x40000000) == 0 )
		return;
#ifndef XB_OPTI
	psxCP2[_Funct_]();
#else
	callPsxCP2(_Funct_);
#endif
}

void psxBASIC() {
#ifndef XB_OPTI
	psxCP2BSC[_Rs_]();
#else
	callPsxCP2BSC(_Rs_);
#endif
}

void psxHLE() {
//	psxHLEt[psxRegs.code & 0xffff]();
	psxHLEt[psxRegs.code & 0x07]();		// HDHOSHY experimental patch
}

#ifndef XB_OPTI
void (*psxBSC[64])() = {
	psxSPECIAL, psxREGIMM, psxJ   , psxJAL  , psxBEQ , psxBNE , psxBLEZ, psxBGTZ,
	psxADDI   , psxADDIU , psxSLTI, psxSLTIU, psxANDI, psxORI , psxXORI, psxLUI ,
	psxCOP0   , psxNULL  , psxCOP2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL   , psxNULL  , psxNULL, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	psxLB     , psxLH    , psxLWL , psxLW   , psxLBU , psxLHU , psxLWR , psxNULL,
	psxSB     , psxSH    , psxSWL , psxSW   , psxNULL, psxNULL, psxSWR , psxNULL, 
	psxNULL   , psxNULL  , gteLWC2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL   , psxNULL  , gteSWC2, psxHLE  , psxNULL, psxNULL, psxNULL, psxNULL 
};


void (*psxSPC[64])() = {
	psxSLL , psxNULL , psxSRL , psxSRA , psxSLLV   , psxNULL , psxSRLV, psxSRAV,
	psxJR  , psxJALR , psxNULL, psxNULL, psxSYSCALL, psxBREAK, psxNULL, psxNULL,
	psxMFHI, psxMTHI , psxMFLO, psxMTLO, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxMULT, psxMULTU, psxDIV , psxDIVU, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxADD , psxADDU , psxSUB , psxSUBU, psxAND    , psxOR   , psxXOR , psxNOR ,
	psxNULL, psxNULL , psxSLT , psxSLTU, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL
};

void (*psxREG[32])() = {
	psxBLTZ  , psxBGEZ  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxBLTZAL, psxBGEZAL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};

void (*psxCP0[32])() = {
	psxMFC0, psxNULL, psxCFC0, psxNULL, psxMTC0, psxNULL, psxCTC0, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxRFE , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};

void (*psxCP2[64])() = {
	psxBASIC, gteRTPS , psxNULL , psxNULL, psxNULL, psxNULL , gteNCLIP, psxNULL, // 00
	psxNULL , psxNULL , psxNULL , psxNULL, gteOP  , psxNULL , psxNULL , psxNULL, // 08
	gteDPCS , gteINTPL, gteMVMVA, gteNCDS, gteCDP , psxNULL , gteNCDT , psxNULL, // 10
	psxNULL , psxNULL , psxNULL , gteNCCS, gteCC  , psxNULL , gteNCS  , psxNULL, // 18
	gteNCT  , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 20
	gteSQR  , gteDCPL , gteDPCT , psxNULL, psxNULL, gteAVSZ3, gteAVSZ4, psxNULL, // 28 
	gteRTPT , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 30
	psxNULL , psxNULL , psxNULL , psxNULL, psxNULL, gteGPF  , gteGPL  , gteNCCT  // 38
};

void (*psxCP2BSC[32])() = {
	psxMFC2, psxNULL, psxCFC2, psxNULL, gteMTC2, psxNULL, gteCTC2, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};
#endif

///////////////////////////////////////////

static int intInit() {
	Config.CpuRunning = 1;
	return 0;
}

static void intReset() {
	psxRegs.ICache_valid = FALSE;
}

static void intExecute() {
	while(Config.CpuRunning) {
		execI();
  }
}

static void intExecuteBlock() {
	branch2 = 0;
	while (!branch2) execI();
}

static void intClear(u32 Addr, u32 Size) {
}

static void intShutdown() {
}

// interpreter execution
static void execI() { 
	u32 *code = (u32 *)PSXM_2(psxRegs.pc);
	psxRegs.code = SWAP32(*code);

//	debugI();

//	if (Config.Debug) ProcessDebug();

	psxRegs.pc += 4;
	psxRegs.cycle += BIAS;
#ifndef XB_OPTI
	psxBSC[psxRegs.code >> 26]();
#else
	CallPsxBsc(psxRegs.code >> 26);
#endif
}

__inline void callPsxSPC(int func_code){
	switch (func_code){
		case 0:
			psxSLL();
			return;
		case 2:
			psxSRL();
			return;
		case 3:
			psxSRA();
			return;
		case 4:
			psxSLLV();
			return;
		case 6:
			psxSRLV();
			return;
		case 7:
			psxSRAV();
			return;
		case 8:
			psxJR();
			return;
		case 9:
			psxJALR();
			return;
		case 12:
			psxSYSCALL();
			return;
		case 13:
			psxBREAK();
			return;
		case 16:
			psxMFHI();
			return;
		case 17:
			psxMTHI();
			return;
		case 18:
			psxMFLO();
			return;
		case 19:
			psxMTLO();
			return;
		case 24:
			psxMULT();
			return;
		case 25:
			psxMULTU();
			return;
		case 26:
			psxDIV();
			return;
		case 27:
			psxDIVU();
			return;
		case 32:
			psxADD();
			return;
		case 33:
			psxADDU();
			return;
		case 34:
			psxSUB();
			return;
		case 35:
			psxSUBU();
			return;
		case 36:
			psxAND();
			return;
		case 37:
			psxOR();
			return;
		case 38:
			psxXOR();
			return;
		case 39:
			psxNOR();
			return;
		case 42:
			psxSLT();
			return;
		case 43:
			psxSLTU();
			return;
		default:
			psxNULL();
			return;
	}
}

__inline void CallPsxBsc(int func_code){
	switch(func_code){
		case 0:
			psxSPECIAL();
			return;
		case 1:
			psxREGIMM();
			return;
		case 2:
			psxJ();
			return;
		case 3:
			psxJAL();
			return;
		case 4:
			psxBEQ();
			return;
		case 5:
			psxBNE();
			return;
		case 6:
			psxBLEZ();
			return;
		case 7:
			psxBGTZ();
			return;
		case 8:
			psxADDI();
			return;
		case 9:
			psxADDIU();
			return;
		case 10:
			psxSLTI();
			return;
		case 11:
			psxSLTIU();
			return;
		case 12:
			psxANDI();
			return;
		case 13:
			psxORI();
			return;
		case 14:
			psxXORI();
			return;
		case 15:
			psxLUI();
			return;
		case 16:
			psxCOP0();
			return;
		case 18:
			psxCOP2();
			return;
		case 32:
			psxLB();
			return;
		case 33:
			psxLH();
			return;
		case 34:
			psxLWL();
			return;
		case 35:
			psxLW();
			return;
		case 36:
			psxLBU();
			return;
		case 37:
			psxLHU();
			return;
		case 38:
			psxLWR();
			return;
		case 40:
			psxSB();
			return;
		case 41:
			psxSH();
			return;
		case 42:
			psxSWL();
			return;
		case 43:
			psxSW();
			return;
		case 46:
			psxSWR();
			return;
		case 50:
			gteLWC2();
			return;
		case 58:
			gteSWC2();
			return;
		case 59:
			psxHLE();
			return;
		default:
			psxNULL();
			return;
	}
}

__inline void CallPsxREG(int func_code){
	switch(func_code){
		case 0:
			psxBLTZ();
			return;
		case 1:
			psxBGEZ();
			return;
		case 16:
			psxBLTZAL();
			return;
		case 17:
			psxBGEZAL();
			return;
		default:
			psxNULL();
			return;
	}
};
__inline void callPsxCP0(int func_code){
	switch(func_code){
		case 0:
			psxMFC0();
			return;
		case 2:
			psxCFC0();
			return;
		case 4:
			psxMTC0();
			return;
		case 6:
			psxCTC0();
			return;
		case 16:
			psxRFE();
			return;
		default:
			psxNULL();
			return;
	}
};
__inline void callPsxCP2(int func_code){
	switch(func_code){
		case 0:
			psxBASIC();
			return;
		case 1:
			gteRTPS();
			return;
		case 6:
			gteNCLIP();
			return;
		case 12:
			gteOP();
			return;
		case 16:
			gteDPCS();
			return;
		case 17:
			gteINTPL();
			return;
		case 18:
			gteMVMVA();
			return;
		case 19:
			gteNCDS();
			return;
		case 20:
			gteCDP();
			return;
		case 22:
			gteNCDT();
			return;
		case 27:
			gteNCCS();
			return;
		case 28:
			gteCC();
			return;
		case 30:
			gteNCS();
			return;
		case 32:
			gteNCT();
			return;
		case 40:
			gteSQR();
			return;
		case 41:
			gteDCPL();
			return;
		case 42:
			gteDPCT();
			return;
		case 45:
			gteAVSZ3();
			return;
		case 46:
			gteAVSZ4();
			return;
		case 48:
			gteRTPT();
			return;
		case 61:
			gteGPF();
			return;
		case 62:
			gteGPL();
			return;
		case 63:
			gteNCCT();
			return;
		default:
			psxNULL();
			return;
	}
};
__inline void callPsxCP2BSC(int func_code){
	switch(func_code){
		case 0:
			psxMFC2();
			return;
		case 2:
			psxCFC2();
			return;
		case 4:
			gteMTC2();
			return;
		case 6:
			gteCTC2();
			return;
		default:
			psxNULL();
			return;
	}
};


R3000Acpu psxInt = {
	intInit,
	intReset,
	intExecute,
	intExecuteBlock,
	intClear,
	intShutdown
};
