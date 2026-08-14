//
// Made by iMoD1998
// V3.1
//
// Detour PPC/Xenon: reescribe el prologo de la funcion destino con un salto
// absoluto (lis/ori/mtctr/bctr) y construye un trampolin en una seccion .text de
// confianza del hypervisor. Copia usada por el plugin residente hidmouse.xex.
//

#ifndef DETOUR_H
#define DETOUR_H

#include <xtl.h>
#include <stdint.h>

#define MASK_N_BITS(N) ( ( 1 << ( N ) ) - 1 )

#define POWERPC_HI(X) ( ( X >> 16 ) & 0xFFFF )
#define POWERPC_LO(X) ( X & 0xFFFF )

#define POWERPC_BIT32(N) ( 31 - N )

#define POWERPC_OPCODE(OP)       ( OP << 26 )
#define POWERPC_OPCODE_ADDI      POWERPC_OPCODE( 14 )
#define POWERPC_OPCODE_ADDIS     POWERPC_OPCODE( 15 )
#define POWERPC_OPCODE_BC        POWERPC_OPCODE( 16 )
#define POWERPC_OPCODE_B         POWERPC_OPCODE( 18 )
#define POWERPC_OPCODE_BCCTR     POWERPC_OPCODE( 19 )
#define POWERPC_OPCODE_ORI       POWERPC_OPCODE( 24 )
#define POWERPC_OPCODE_EXTENDED  POWERPC_OPCODE( 31 )
#define POWERPC_OPCODE_STW       POWERPC_OPCODE( 36 )
#define POWERPC_OPCODE_LWZ       POWERPC_OPCODE( 32 )
#define POWERPC_OPCODE_LD        POWERPC_OPCODE( 58 )
#define POWERPC_OPCODE_STD       POWERPC_OPCODE( 62 )
#define POWERPC_OPCODE_MASK      POWERPC_OPCODE( 63 )

#define POWERPC_EXOPCODE(OP)     ( OP << 1 )
#define POWERPC_EXOPCODE_BCCTR   POWERPC_EXOPCODE( 528 )
#define POWERPC_EXOPCODE_MTSPR   POWERPC_EXOPCODE( 467 )

#define POWERPC_SPR(SPR) (UINT32)( ( ( SPR & 0x1F ) << 5 ) | ( ( SPR >> 5 ) & 0x1F ) )

#define POWERPC_ADDI(rD, rA, SIMM)  (UINT32)( POWERPC_OPCODE_ADDI | ( rD << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | SIMM )
#define POWERPC_ADDIS(rD, rA, SIMM) (UINT32)( POWERPC_OPCODE_ADDIS | ( rD << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | SIMM )
#define POWERPC_LIS(rD, SIMM)       POWERPC_ADDIS( rD, 0, SIMM )
#define POWERPC_LI(rD, SIMM)        POWERPC_ADDI( rD, 0, SIMM )
#define POWERPC_MTSPR(SPR, rS)      (UINT32)( POWERPC_OPCODE_EXTENDED | ( rS << POWERPC_BIT32( 10 ) ) | ( POWERPC_SPR( SPR ) << POWERPC_BIT32( 20 ) ) | POWERPC_EXOPCODE_MTSPR )
#define POWERPC_MTCTR(rS)           POWERPC_MTSPR( 9, rS )
#define POWERPC_ORI(rS, rA, UIMM)   (UINT32)( POWERPC_OPCODE_ORI | ( rS << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | UIMM )
#define POWERPC_BCCTR(BO, BI, LK)   (UINT32)( POWERPC_OPCODE_BCCTR | ( BO << POWERPC_BIT32( 10 ) ) | ( BI << POWERPC_BIT32( 15 ) ) | ( LK & 1 ) | POWERPC_EXOPCODE_BCCTR )
#define POWERPC_STD(rS, DS, rA)     (UINT32)( POWERPC_OPCODE_STD | ( rS << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | ( (INT16)DS & 0xFFFF ) )
#define POWERPC_LD(rS, DS, rA)      (UINT32)( POWERPC_OPCODE_LD | ( rS << POWERPC_BIT32( 10 ) ) | ( rA << POWERPC_BIT32( 15 ) ) | ( (INT16)DS & 0xFFFF ) )

#define POWERPC_BRANCH_LINKED    1
#define POWERPC_BRANCH_ABSOLUTE  2
#define POWERPC_BRANCH_TYPE_MASK ( POWERPC_BRANCH_LINKED | POWERPC_BRANCH_ABSOLUTE )

#define POWERPC_BRANCH_OPTIONS_ALWAYS ( 20 )

class Detour
{
public:
	Detour() {}

	Detour(
		_Inout_ void*       HookSource,
		_In_    const void* HookTarget
	) :
		HookSource(HookSource),
		HookTarget(HookTarget),
		TrampolineAddress(NULL),
		OriginalLength(0)
	{
	}

	~Detour()
	{
		this->Remove();
	}

	static SIZE_T WriteFarBranch(
		_Out_   void*       Destination,
		_In_    const void* BranchTarget,
		_In_    bool        Linked = true,
		_In_    bool        PreserveRegister = false
	)
	{
		return Detour::WriteFarBranchEx(Destination, BranchTarget, Linked, PreserveRegister);
	}

	static SIZE_T WriteFarBranchEx(
		_Out_ void*       Destination,
		_In_  const void* BranchTarget,
		_In_  bool        Linked = false,
		_In_  bool        PreserveRegister = false,
		_In_  UINT32      BranchOptions = POWERPC_BRANCH_OPTIONS_ALWAYS,
		_In_  BYTE        ConditionRegisterBit = 0,
		_In_  BYTE        RegisterIndex = 0
	)
	{
		const UINT32 BranchFarAsm[] = {
			POWERPC_LIS(RegisterIndex, POWERPC_HI((UINT32)BranchTarget)),
			POWERPC_ORI(RegisterIndex, RegisterIndex, POWERPC_LO((UINT32)BranchTarget)),
			POWERPC_MTCTR(RegisterIndex),
			POWERPC_BCCTR(BranchOptions, ConditionRegisterBit, Linked)
		};

		const UINT32 BranchFarAsmPreserve[] = {
			POWERPC_STD(RegisterIndex, -0x30, 1),
			POWERPC_LIS(RegisterIndex, POWERPC_HI((UINT32)BranchTarget)),
			POWERPC_ORI(RegisterIndex, RegisterIndex, POWERPC_LO((UINT32)BranchTarget)),
			POWERPC_MTCTR(RegisterIndex),
			POWERPC_LD(RegisterIndex, -0x30, 1),
			POWERPC_BCCTR(BranchOptions, ConditionRegisterBit, Linked)
		};

		const auto BranchAsm = PreserveRegister ? BranchFarAsmPreserve : BranchFarAsm;
		const auto BranchAsmSize = PreserveRegister ? sizeof(BranchFarAsmPreserve) : sizeof(BranchFarAsm);

		if (Destination)
			memcpy(Destination, BranchAsm, BranchAsmSize);

		return BranchAsmSize;
	}

	static SIZE_T RelocateBranch(
		_Out_ UINT32*       Destination,
		_In_  const UINT32* Source
	)
	{
		const auto Instruction = *Source;
		const auto InstructionAddress = (UINT32)Source;

		if (Instruction & POWERPC_BRANCH_ABSOLUTE)
		{
			*Destination = Instruction;
			return 4;
		}

		INT32  BranchOffsetBitSize;
		INT32  BranchOffsetBitBase;
		UINT32 BranchOptions;
		BYTE   ConditionRegisterBit;

		switch (Instruction & POWERPC_OPCODE_MASK)
		{
		case POWERPC_OPCODE_B:
			BranchOffsetBitSize = 24;
			BranchOffsetBitBase = 2;
			BranchOptions = POWERPC_BRANCH_OPTIONS_ALWAYS;
			ConditionRegisterBit = 0;
			break;

		case POWERPC_OPCODE_BC:
			BranchOffsetBitSize = 14;
			BranchOffsetBitBase = 2;
			BranchOptions = (Instruction >> POWERPC_BIT32(10)) & MASK_N_BITS(5);
			ConditionRegisterBit = (Instruction >> POWERPC_BIT32(15)) & MASK_N_BITS(5);
			break;
		}

		INT32 BranchOffset = Instruction & (MASK_N_BITS(BranchOffsetBitSize) << BranchOffsetBitBase);

		if (BranchOffset >> ((BranchOffsetBitSize + BranchOffsetBitBase) - 1))
		{
			BranchOffset |= ~MASK_N_BITS(BranchOffsetBitSize + BranchOffsetBitBase);
		}

		const auto BranchAddress = (void*)(INT32)(InstructionAddress + BranchOffset);

		return Detour::WriteFarBranchEx(Destination, BranchAddress, Instruction & POWERPC_BRANCH_LINKED, true, BranchOptions, ConditionRegisterBit);
	}

	static SIZE_T CopyInstruction(
		_Out_ UINT32*       Destination,
		_In_  const UINT32* Source
	)
	{
		const auto Instruction = *Source;

		switch (Instruction & POWERPC_OPCODE_MASK)
		{
		case POWERPC_OPCODE_B:
		case POWERPC_OPCODE_BC:
			return Detour::RelocateBranch(Destination, Source);
		default:
			*Destination = Instruction;
			return 4;
		}
	}

	bool Install()
	{
		if (this->OriginalLength != 0)
			return false;

		const auto HookSize = Detour::WriteFarBranch(NULL, this->HookTarget, false, false);

		memcpy(this->OriginalInstructions, this->HookSource, HookSize);

		this->OriginalLength = HookSize;

		this->TrampolineAddress = &Detour::TrampolineBuffer[Detour::TrampolineSize];

		for (SIZE_T i = 0; i < (HookSize / 4); i++)
		{
			const auto InstructionPtr = (UINT32*)((UINT32)this->HookSource + (i * 4));

			Detour::TrampolineSize += Detour::CopyInstruction((UINT32*)&Detour::TrampolineBuffer[Detour::TrampolineSize], InstructionPtr);
		}

		const auto AfterBranchAddress = (void*)((UINT32)this->HookSource + HookSize);

		Detour::TrampolineSize += Detour::WriteFarBranch(&Detour::TrampolineBuffer[Detour::TrampolineSize], AfterBranchAddress, false, true);

		Detour::WriteFarBranch(this->HookSource, this->HookTarget, false, false);

		return true;
	}

	bool Remove()
	{
		if (this->HookSource && this->OriginalLength)
		{
			memcpy(this->HookSource, this->OriginalInstructions, this->OriginalLength);

			this->OriginalLength = 0;
			this->HookSource = NULL;

			return true;
		}

		return false;
	}

	template<typename T>
	T GetOriginal() const
	{
		return T(this->TrampolineAddress);
	}

private:
	const void* HookTarget;
	void*       HookSource;
	BYTE*       TrampolineAddress;
	BYTE        OriginalInstructions[30];
	SIZE_T      OriginalLength;

	#pragma section(".text", read, execute)
	__declspec(allocate(".text")) static BYTE TrampolineBuffer[200 * 20];
	static SIZE_T TrampolineSize;
};

#endif // !DETOUR_H
