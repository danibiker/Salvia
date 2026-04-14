#include "ppc.h"
#include <setjmp.h>
/**
Lorsque le compiler copile une function il ajoute un prologue pour securité
naked permet de gerer le prologue soit meme
mci [cOz]

Register R3-R10: Parameters
Register R3-R4: Results 
*/
#if 1
void __declspec(naked) recRun(register void (*func)(), register u32 hw1, register u32 hw2)
{
	/* prologue code */
	__asm{
		mflr	r12
		std     r13, -0x110(sp)
		std     r14, -0x98(sp)
		std     r15, -0x90(sp)			// save regs to stack frame
		std     r16, -0x88(sp)
		std     r17, -0x80(sp)
		std     r18, -0x78(sp)
		std     r19, -0x70(sp)
		std     r20, -0x68(sp)
		std     r21, -0x60(sp)
		std     r22, -0x58(sp)
		std     r23, -0x50(sp)
		std     r24, -0x48(sp)
		std     r25, -0x40(sp)
		std     r26, -0x38(sp)
		std     r27, -0x30(sp)
		std     r28, -0x28(sp)
		std     r29, -0x20(sp)
		std     r30, -0x18(sp)
		std     r31, -0x10(sp)
		std     r12, -0x8(sp)
		stwu	r1, -0x118(r1)			// increments stack frame

		/* execute code */
		mtctr   r3                      // load Count Register with address of func
		mr      r31, r4
		mr      r30, r5
		bctrl                           //branch to contents of Count Register
	}
}


void __declspec(naked) returnPC()
{
	__asm{
		addi	r1, r1, 0x118		//Free stack frame
		ld		r13, -0x110(sp)
		ld		r14, -0x98(sp)
		ld		r15, -0x90(sp)		// restore regs from stack frame
		ld		r16, -0x88(sp)
		ld		r17, -0x80(sp)
		ld		r18, -0x78(sp)
		ld		r19, -0x70(sp)
		ld		r20, -0x68(sp)
		ld		r21, -0x60(sp)
		ld		r22, -0x58(sp)
		ld		r23, -0x50(sp)
		ld		r24, -0x48(sp)
		ld		r25, -0x40(sp)
		ld		r26, -0x38(sp)
		ld		r27, -0x30(sp)
		ld		r28, -0x28(sp)
		ld		r29, -0x20(sp)
		ld		r30, -0x18(sp)
		ld		r31, -0x10(sp)
		ld      r12, -0x8(sp)		//recover and branch to lik register
		mtlr	r12
		blr
	}
}

#elif 1

/*
Register R3-R10: Parameters

Register R3-R4: Results 
*/
__inline void __declspec(naked) recRun(register void (*func)(), register u32 hw1, register u32 hw2)
{
	__asm{
		/* prologue code */
		mflr	r0
		stmw	r13, -76(r1) // -(32-14)*4
		stw		r0, 4(r1)
		stwu	r1, -84(r1) // -((32-14)*4+8)

		/* execute code */
		mtctr	r3			// load Count Register with address of func
		mr		r31, r4
		mr		r30, r5
		bctrl				//branch to contents of Count Register
	}
}

__inline void __declspec(naked) returnPC()
{
	__asm{
		lwz		r0, 88(r1) // (32-14)*4+8+4
		addi	r1, r1, 84 //(32-14)*4+8
		mtlr	r0
		lmw		r13, -76(r1) // -(32-14)*4
		blr
	}
}

#else
__inline void __declspec(naked) recRun(register void (*func)(), register u32 hw1, register u32 hw2)
{
	__asm{
		/* prologue code */
		mflr	r0
		stmw	r14, -72(r1) // -(32-14)*4
		stw		r0, 4(r1)
		stwu	r1, -80(r1) // -((32-14)*4+8)

		/* execute code */
		mtctr	r3			// load Count Register with address of func
		mr		r31, r4
		mr		r30, r5
		bctrl				//branch to contents of Count Register
	}
}

__inline void __declspec(naked) returnPC()
{
	__asm{
		lwz		r0, 84(r1) // (32-14)*4+8+4
		addi	r1, r1, 80 //(32-14)*4+8
		mtlr	r0
		lmw		r14, -72(r1) // -(32-14)*4
		blr
	}
}
#endif