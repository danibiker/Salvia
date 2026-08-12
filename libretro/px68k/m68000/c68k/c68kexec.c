/*  Copyright 2003-2004 Stephane Dallongeville

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*! \file c68kexec.c
    \brief C68K emulation execution and instruction functions.
*/

#include "core.h"
#include "c68k.h"

/* exception cycle table (taken from musashi core) */
static const int32_t c68k_exception_cycle_table[256] =
{
	  4, /*  0: Reset - Initial Stack Pointer */
	  4, /*  1: Reset - Initial Program Counter */
	 50, /*  2: Bus Error */
	 50, /*  3: Address Error */
	 34, /*  4: Illegal Instruction */
	 38, /*  5: Divide by Zero */
	 40, /*  6: CHK */
	 34, /*  7: TRAPV */
	 34, /*  8: Privilege Violation */
	 34, /*  9: Trace */
	  4, /* 10: */
	  4, /* 11: */
	  4, /* 12: RESERVED */
	  4, /* 13: Coprocessor Protocol Violation */
	  4, /* 14: Format Error */
	 44, /* 15: Uninitialized Interrupt */
	  4, /* 16: RESERVED */
	  4, /* 17: RESERVED */
	  4, /* 18: RESERVED */
	  4, /* 19: RESERVED */
	  4, /* 20: RESERVED */
	  4, /* 21: RESERVED */
	  4, /* 22: RESERVED */
	  4, /* 23: RESERVED */
	 44, /* 24: Spurious Interrupt */
	 44, /* 25: Level 1 Interrupt Autovector */
	 44, /* 26: Level 2 Interrupt Autovector */
	 44, /* 27: Level 3 Interrupt Autovector */
	 44, /* 28: Level 4 Interrupt Autovector */
	 44, /* 29: Level 5 Interrupt Autovector */
	 44, /* 30: Level 6 Interrupt Autovector */
	 44, /* 31: Level 7 Interrupt Autovector */
	 34, /* 32: TRAP #0 */
	 34, /* 33: TRAP #1 */
	 34, /* 34: TRAP #2 */
	 34, /* 35: TRAP #3 */
	 34, /* 36: TRAP #4 */
	 34, /* 37: TRAP #5 */
	 34, /* 38: TRAP #6 */
	 34, /* 39: TRAP #7 */
	 34, /* 40: TRAP #8 */
	 34, /* 41: TRAP #9 */
	 34, /* 42: TRAP #10 */
	 34, /* 43: TRAP #11 */
	 34, /* 44: TRAP #12 */
	 34, /* 45: TRAP #13 */
	 34, /* 46: TRAP #14 */
	 34, /* 47: TRAP #15 */
	  4, /* 48: FP Branch or Set on Unknown Condition */
	  4, /* 49: FP Inexact Result */
	  4, /* 50: FP Divide by Zero */
	  4, /* 51: FP Underflow */
	  4, /* 52: FP Operand Error */
	  4, /* 53: FP Overflow */
	  4, /* 54: FP Signaling NAN */
	  4, /* 55: FP Unimplemented Data Type */
	  4, /* 56: MMU Configuration Error */
	  4, /* 57: MMU Illegal Operation Error */
	  4, /* 58: MMU Access Level Violation Error */
	  4, /* 59: RESERVED */
	  4, /* 60: RESERVED */
	  4, /* 61: RESERVED */
	  4, /* 62: RESERVED */
	  4, /* 63: RESERVED */
	     /* 64-255: User Defined */
	  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
};

/* global variable */

#ifndef C68K_NO_JUMP_TABLE
static void *JumpTable[0x10000];
#endif

static uint32_t C68k_Initialised = 0;

/* internals core macros */

#define LSL(A, C)       ((A) << (C))
#define LSR(A, C)       ((A) >> (C))

#define LSR_32(A, C)    ((C) < 32 ? (A) >> (C) : 0)
#define LSL_32(A, C)    ((C) < 32 ? (A) << (C) : 0)

#define ROL_8(A, C)     (LSL(A, C) | LSR(A, 8-(C)))
#define ROL_9(A, C)     (LSL(A, C) | LSR(A, 9-(C)))
#define ROL_16(A, C)    (LSL(A, C) | LSR(A, 16-(C)))
#define ROL_17(A, C)    (LSL(A, C) | LSR(A, 17-(C)))
#define ROL_32(A, C)    (LSL_32(A, C) | LSR_32(A, 32-(C)))
#define ROL_33(A, C)    (LSL_32(A, C) | LSR_32(A, 33-(C)))

#define ROR_8(A, C)     (LSR(A, C) | LSL(A, 8-(C)))
#define ROR_9(A, C)     (LSR(A, C) | LSL(A, 9-(C)))
#define ROR_16(A, C)    (LSR(A, C) | LSL(A, 16-(C)))
#define ROR_17(A, C)    (LSR(A, C) | LSL(A, 17-(C)))
#define ROR_32(A, C)    (LSR_32(A, C) | LSL_32(A, 32-(C)))
#define ROR_33(A, C)    (LSR_32(A, C) | LSL_32(A, 33-(C)))

#ifndef C68K_NO_JUMP_TABLE
#define NEXT                    \
    CPU->CycleIO = CCnt;        \
    Opcode = FETCH_WORD;        \
    PC += 2;                    \
    goto *JumpTable[Opcode];
#else
#define NEXT                    \
    CPU->CycleIO = CCnt;        \
    Opcode = FETCH_WORD;        \
    PC += 2;                    \
    goto SwitchTable;
#endif

#define RET(A)                  \
    CCnt -= (A);                \
    if (CCnt <= 0) goto C68k_Exec_End; \
    NEXT

#define SET_PC(A)               \
    CPU->BasePC = CPU->Fetch[((A) >> C68K_FETCH_SFT) & C68K_FETCH_MASK];    \
    CPU->BasePC -= (A) & 0xFF000000;    \
    PC = (A) + CPU->BasePC;

#define PRE_IO  CPU->CycleIO = CCnt
#define POST_IO CCnt = CPU->CycleIO

#define READ_BYTE_F(A, D)           \
    D = CPU->Read_Byte(A) & 0xFF;

#define READ_WORD_F(A, D)           \
    D = CPU->Read_Word(A) & 0xFFFF;

#ifdef MSB_FIRST
    #define READ_LONG_F(A, D)           \
    D = CPU->Read_Word((A)) << 16;   \
    D |= CPU->Read_Word((A) + 2) & 0xFFFF;

    #define READ_LONG_DEC_F(A, D)       \
    D = CPU->Read_Word((A) + 2) & 0xFFFF;  \
    D |= CPU->Read_Word((A)) << 16;
#else
    #define READ_LONG_F(A, D)               \
    D = CPU->Read_Word((A)) << 16;          \
    D |= CPU->Read_Word((A) + 2) & 0xFFFF;

    #define READ_LONG_DEC_F(A, D)           \
    D = CPU->Read_Word((A) + 2) & 0xFFFF;   \
    D |= CPU->Read_Word((A)) << 16;
#endif

#define READSX_BYTE_F(A, D)             \
    D = (int32_t)(int8_t)CPU->Read_Byte(A);

#define READSX_WORD_F(A, D)             \
    D = (int32_t)(int16_t)CPU->Read_Word(A);
    
#ifdef MSB_FIRST
    #define READSX_LONG_F(A, D)         \
    D = CPU->Read_Word((A)) << 16;   \
    D |= CPU->Read_Word((A) + 2) & 0xFFFF;

    #define READSX_LONG_DEC_F(A, D)     \
    D = CPU->Read_Word((A) + 2) & 0xFFFF;  \
    D |= CPU->Read_Word((A)) << 16;
#else
    #define READSX_LONG_F(A, D)             \
    D = CPU->Read_Word((A)) << 16;          \
    D |= CPU->Read_Word((A) + 2) & 0xFFFF;

    #define READSX_LONG_DEC_F(A, D)         \
    D = CPU->Read_Word((A) + 2) & 0xFFFF;   \
    D |= CPU->Read_Word((A)) << 16;
#endif

#define WRITE_BYTE_F(A, D) CPU->Write_Byte(A, D);
#define WRITE_WORD_F(A, D) CPU->Write_Word(A, D);

#ifdef MSB_FIRST
    #define WRITE_LONG_F(A, D)              \
    CPU->Write_Word((A), (D) >> 16);     \
    CPU->Write_Word((A) + 2, (D) & 0xFFFF);

    #define WRITE_LONG_DEC_F(A, D)          \
    CPU->Write_Word((A) + 2, (D) & 0xFFFF);    \
    CPU->Write_Word((A), (D) >> 16);
#else
    #define WRITE_LONG_F(A, D)              \
    CPU->Write_Word((A), (D) >> 16);        \
    CPU->Write_Word((A) + 2, (D) & 0xFFFF);

    #define WRITE_LONG_DEC_F(A, D)          \
    CPU->Write_Word((A) + 2, (D) & 0xFFFF); \
    CPU->Write_Word((A), (D) >> 16);
#endif

#define PUSH_16_F(D)                    \
    CPU->Write_Word(CPU->A[7] -= 2, D); \

#define POP_16_F(D)                     \
    D = (uint16_t)CPU->Read_Word(CPU->A[7]); \
    CPU->A[7] += 2;

#ifdef MSB_FIRST
    #define PUSH_32_F(D)                        \
    CPU->A[7] -= 4;                             \
    CPU->Write_Word(CPU->A[7] + 2, (D) & 0xFFFF);  \
    CPU->Write_Word(CPU->A[7], (D) >> 16);
    
    #define POP_32_F(D)                         \
    D = CPU->Read_Word(CPU->A[7]) << 16;     \
    D |= CPU->Read_Word(CPU->A[7] + 2) & 0xFFFF;   \
    CPU->A[7] += 4;
#else
    #define PUSH_32_F(D)                            \
    CPU->A[7] -= 4;                                 \
    CPU->Write_Word(CPU->A[7] + 2, (D) & 0xFFFF);   \
    CPU->Write_Word(CPU->A[7], (D) >> 16);

    #define POP_32_F(D)                             \
    D = CPU->Read_Word(CPU->A[7]) << 16;            \
    D |= CPU->Read_Word(CPU->A[7] + 2) & 0xFFFF;    \
    CPU->A[7] += 4;
#endif

#define FETCH_BYTE ((*(uint16_t*)PC) & 0xFF)
#define FETCH_WORD (*(uint16_t*)PC)
#define FETCH_LONG (*(uint32_t*)PC)

#define DECODE_EXT_WORD     \
{                           \
    uint32_t ext = (*(uint16_t*)PC); \
    PC += 2; \
    adr += (int32_t)((int8_t)(ext)); \
    if (ext & 0x0800) \
       adr += (int32_t) CPU->D[ext >> 12]; \
    else \
       adr += (int32_t)((int16_t)(CPU->D[ext >> 12]));        \
}

#ifndef MSB_FIRST
#ifdef C68K_BYTE_SWAP_OPT
    #undef FETCH_LONG
    #define FETCH_LONG          ((((uint32_t)(*(uint16_t*)PC)) << 16) | ((uint32_t)(*(uint16_t*)(PC + 2))))
    
#else
    #undef FETCH_BYTE
    #define FETCH_BYTE          (*(uint16_t*)PC) >> 8)
    

    #undef FETCH_WORD
    #define FETCH_WORD          ((((uint16_t)(*(uint8_t*)PC)) << 8) | ((uint16_t)(*(uint8_t*)(PC + 1))))
    

    #undef FETCH_LONG
    #define FETCH_LONG          ((((uint32_t)(*(uint8_t*)PC)) << 24) | (((uint32_t)(*(uint8_t*)(PC + 1))) << 16) | (((uint32_t)(*(uint8_t*)(PC + 2))) << 8) | ((uint32_t)(*(uint8_t*)(PC + 3))))
    

    #undef DECODE_EXT_WORD
    #define DECODE_EXT_WORD     \
    {                           \
        uint32_t ext = (*(uint16_t*)PC);      \
        PC += 2;                \
                                \
        adr += (int32_t)((int8_t)(ext >> 8));                                   \
        if (ext & 0x0008) adr += (int32_t) CPU->D[(ext >> 4) & 0x000F];     \
        else adr += (int32_t)((int16_t)(CPU->D[(ext >> 4) & 0x000F]));          \
    }
#endif
#endif

#define GET_CCR                                     \
    (((CPU->flag_C >> (C68K_SR_C_SFT - 0)) & 1) |   \
     ((CPU->flag_V >> (C68K_SR_V_SFT - 1)) & 2) |   \
     (((!CPU->flag_notZ) & 1) << 2) |               \
     ((CPU->flag_N >> (C68K_SR_N_SFT - 3)) & 8) |   \
     ((CPU->flag_X >> (C68K_SR_X_SFT - 4)) & 0x10))

#define GET_SR                  \
    ((CPU->flag_S << 0)  |      \
     (CPU->flag_I << 8)  |      \
     GET_CCR)

#define SET_CCR(A)                              \
    CPU->flag_C = (A) << (C68K_SR_C_SFT - 0);   \
    CPU->flag_V = (A) << (C68K_SR_V_SFT - 1);   \
    CPU->flag_notZ = ~(A) & 4;                  \
    CPU->flag_N = (A) << (C68K_SR_N_SFT - 3);   \
    CPU->flag_X = (A) << (C68K_SR_X_SFT - 4);

#define SET_SR(A)                   \
    SET_CCR(A)                      \
    CPU->flag_I = ((A) >> 8) & 7;   \
    CPU->flag_S = (A) & C68K_SR_S;

#define CHECK_INT                                     \
    {                                                 \
        int32_t vect;                               \
        int32_t line = CPU->IRQLine;                          \
                                                      \
        if ((line == 7) || (line > (int32_t)CPU->flag_I)) \
        {                                             \
            /* get vector */                                        \
            CPU->IRQLine = 0;                                       \
            vect = CPU->Interrupt_CallBack(line);                   \
            if (vect == C68K_INT_ACK_AUTOVECTOR)                    \
                vect = C68K_INTERRUPT_AUTOVECTOR_EX + (line & 7);   \
                                                                    \
            /* adjust CCnt */                                       \
            CCnt -= c68k_exception_cycle_table[vect];               \
                                                                    \
            /* swap A7 and USP */              \
            if (!CPU->flag_S)                  \
            {                                  \
                uint32_t tmpSP = CPU->USP;     \
                CPU->USP = CPU->A[7];          \
                CPU->A[7] = tmpSP;             \
            }                                  \
                                               \
    	    CPU->CycleIO = CCnt;               \
                                               \
            /* push PC and SR */               \
            PUSH_32_F((uint32_t)(PC - CPU->BasePC)) \
            PUSH_16_F(GET_SR)                  \
                                               \
            /* adjust SR */                    \
            CPU->flag_S = C68K_SR_S;           \
            CPU->flag_I = line;                \
                                               \
            /* fetch new PC */                 \
            READ_LONG_F(vect * 4, PC)          \
            SET_PC(PC)                         \
                                               \
	    CCnt = CPU->CycleIO;               \
        }                                      \
    }

/* main exec function */

int32_t FASTCALL C68k_Exec(c68k_struc *cpu, int32_t cycle)
{
    c68k_struc *CPU;
    uintptr_t PC;
    int32_t CCnt;
    uint32_t Opcode;

#ifdef C68K_NO_JUMP_TABLE
    C68k_Initialised = 1;
#endif

    CPU = cpu;
    PC = CPU->PC;

    if (CPU->Status & (C68K_RUNNING | C68K_DISABLE | C68K_FAULTED))
    {
#ifndef C68K_NO_JUMP_TABLE
        if (!C68k_Initialised)
           goto C68k_Init;
#endif
        return (CPU->Status | 0x80000000);
    }

    if (cycle <= 0)
       return -cycle;
    
    CPU->CycleToDo = CCnt = cycle;

    CHECK_INT

    if (CPU->Status & (C68K_HALTED | C68K_WAITING)) return CPU->CycleToDo;

    CPU->CycleSup = 0;
    CPU->Status |= C68K_RUNNING;

    NEXT

#ifdef C68K_NO_JUMP_TABLE
SwitchTable:
    switch(Opcode)
    {
#endif
    #include "c68k_op0.inc"
    #include "c68k_op1.inc"
    #include "c68k_op2.inc"
    #include "c68k_op3.inc"
    #include "c68k_op4.inc"
    #include "c68k_op5.inc"
    #include "c68k_op6.inc"
    #include "c68k_op7.inc"
    #include "c68k_op8.inc"
    #include "c68k_op9.inc"
    #include "c68k_opA.inc"
    #include "c68k_opB.inc"
    #include "c68k_opC.inc"
    #include "c68k_opD.inc"
    #include "c68k_opE.inc"
    #include "c68k_opF.inc"
#ifdef C68K_NO_JUMP_TABLE
       /* fallthrough and loop to SwitchTable */
	default:
		if (Opcode <= 0xFFFF)
		{
			const uint32_t JmpTable[] = {
				0x4AFC, 0x4AFC, 0x4AFC, 0x4AFC, 0x4AFC, 0x4AFC, 0x4AFC, 0x4AFC,
				0x4AFC, 0x4AFC, 0xA000, 0x4AFC, 0x4AFC, 0x4AFC, 0x4AFC, 0xF000
			};
			Opcode = JmpTable[(Opcode & 0xF000) >> 12];
			goto SwitchTable;
		}
	}
#endif

C68k_Exec_End:
    CHECK_INT
    if ((CCnt += CPU->CycleSup) > 0)
    {
        CPU->CycleSup = 0;
        NEXT;
    }

C68k_Exec_Really_End:
    CPU->Status &= ~C68K_RUNNING;
    CPU->PC = PC;
    
    return (CPU->CycleToDo - CCnt);

#ifndef C68K_NO_JUMP_TABLE
C68k_Init:
    C68k_Initialised = 1;
    
    return 0;
#endif
}

