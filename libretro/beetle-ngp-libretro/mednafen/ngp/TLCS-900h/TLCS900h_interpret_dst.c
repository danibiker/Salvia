//---------------------------------------------------------------------------
// NEOPOP : Emulator as in Dreamland
//
// Copyright (c) 2001-2002 by neopop_uk
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version. See also the license.txt file for
//	additional informations.
//---------------------------------------------------------------------------

/*
//---------------------------------------------------------------------------
//=========================================================================

	TLCS900h_interpret_dst.c

//=========================================================================
//---------------------------------------------------------------------------

  History of changes:
  ===================

20 JUL 2002 - neopop_uk
=======================================
- Cleaned and tidied up for the source release

22 JUL 2002 - neopop_uk
=======================================
- Added ANDCF,ORCF and XORCF in # and A modes. These were being used
	by one of the obscure pachinko "games".

23 JUL 2002 - neopop_uk
=======================================
- Added cycle count for TSET.

16 AUG 2002 - neopop_uk
=======================================
- Replaced 'second & 7' with 'R', clearer, faster - and for some reason
	more accurate... oh well!

21 AUG 2002 - neopop_uk
=======================================
- Added TSET.

//---------------------------------------------------------------------------
*/

#include "TLCS900h_interpret.h"
#include "TLCS900h_registers.h"
#include "../mem.h"

//=========================================================================

//===== LD (mem),#
void DST_dstLDBi(void)
{
	storeB(mem, FETCH8);
	cycles = 5;
}

//===== LD (mem),#
void DST_dstLDWi(void)
{
	storeW(mem, fetch16());
	cycles = 6;
}

//===== POP (mem)
void DST_dstPOPB(void)
{
	storeB(mem, pop8());
	cycles = 6;
}

//===== POP (mem)
void DST_dstPOPW(void)
{
	storeW(mem, pop16());
	cycles = 6;
}

//===== LD (mem),(nn)
void DST_dstLDBm16(void)
{
	storeB(mem, loadB(fetch16()));
	cycles = 8;
}

//===== LD (mem),(nn)
void DST_dstLDWm16(void)
{
	storeW(mem, loadW(fetch16()));
	cycles = 8;
}

//===== LDA R,mem
void DST_dstLDAW(void)
{
	regW(R) = (uint16)mem;
	cycles = 4;
}

//===== LDA R,mem
void DST_dstLDAL(void)
{
	regL(R) = (uint32)mem;
	cycles = 4;
}

//===== ANDCF A,(mem)
void DST_dstANDCFA(void)
{
	uint8 bit = REGA & 0xF;
	uint8 mbit = (loadB(mem) >> bit) & 1;
	if (bit < 8) SETFLAG_C(mbit & FLAG_C);
	cycles = 8;
}

//===== ORCF A,(mem)
void DST_dstORCFA(void)
{
	uint8 bit = REGA & 0xF;
	uint8 mbit = (loadB(mem) >> bit) & 1;
	if (bit < 8) SETFLAG_C(mbit | FLAG_C);
	cycles = 8;
}

//===== XORCF A,(mem)
void DST_dstXORCFA(void)
{
	uint8 bit = REGA & 0xF;
	uint8 mbit = (loadB(mem) >> bit) & 1;
	if (bit < 8) SETFLAG_C(mbit ^ FLAG_C);
	cycles = 8;
}

//===== LDCF A,(mem)
void DST_dstLDCFA(void)
{
	uint8 bit = REGA & 0xF;
	uint8 mask = (1 << bit);
	if (bit < 8) SETFLAG_C(loadB(mem) & mask);
	cycles = 8;
}

//===== STCF A,(mem)
void DST_dstSTCFA(void)
{
	uint8 bit = REGA & 0xF;
	uint8 cmask = ~(1 << bit);
	uint8 set = FLAG_C << bit;
	if (bit < 8) storeB(mem, (loadB(mem) & cmask) | set); 
	cycles = 8;
}

//===== LD (mem),R
void DST_dstLDBR(void)
{
	storeB(mem, regB(R));
	cycles = 4;
}

//===== LD (mem),R
void DST_dstLDWR(void)
{
	storeW(mem, regW(R));
	cycles = 4;
}

//===== LD (mem),R
void DST_dstLDLR(void)
{
	storeL(mem, regL(R));
	cycles = 6;
}

//===== ANDCF #3,(mem)
void DST_dstANDCF(void)
{
	uint8 bit = R;
	uint8 mbit = (loadB(mem) >> bit) & 1;
	SETFLAG_C(mbit & FLAG_C);
	cycles = 8;
}

//===== ORCF #3,(mem)
void DST_dstORCF(void)
{
	uint8 bit = R;
	uint8 mbit = (loadB(mem) >> bit) & 1;
	SETFLAG_C(mbit | FLAG_C);
	cycles = 8;
}

//===== XORCF #3,(mem)
void DST_dstXORCF(void)
{
	uint8 bit = R;
	uint8 mbit = (loadB(mem) >> bit) & 1;
	SETFLAG_C(mbit ^ FLAG_C);
	cycles = 8;
}

//===== LDCF #3,(mem)
void DST_dstLDCF(void)
{
	uint8 bit = R;
	uint32 mask = (1 << bit);
	SETFLAG_C(loadB(mem) & mask);
	cycles = 8;
}

//===== STCF #3,(mem)
void DST_dstSTCF(void)
{
	uint8 bit = R;
	uint8 cmask = ~(1 << bit);
	uint8 set = FLAG_C << bit;
	storeB(mem, (loadB(mem) & cmask) | set); 
	cycles = 8;
}

//===== TSET #3,(mem)
void DST_dstTSET(void)
{
	SETFLAG_Z(! (loadB(mem) & (1 << R)) );
	storeB(mem, loadB(mem) | (1 << R));

	SETFLAG_H1
	SETFLAG_N0
	cycles = 10;
}

//===== RES #3,(mem)
void DST_dstRES(void)
{
	storeB(mem, loadB(mem) & (~(1 << R)));
	cycles = 8;
}

//===== SET #3,(mem)
void DST_dstSET(void)
{
	storeB(mem, loadB(mem) | (1 << R));
	cycles = 8;
}

//===== CHG #3,(mem)
void DST_dstCHG(void)
{
	storeB(mem, loadB(mem) ^ (1 << R));
	cycles = 8;
}

//===== BIT #3,(mem)
void DST_dstBIT(void)
{
   SETFLAG_Z(! (loadB(mem) & (1 << R)) );
   SETFLAG_H1;
   SETFLAG_N0;
   cycles = 8;
}

//===== JP cc,mem
void DST_dstJP(void)
{
   cycles = 6;
   if (conditionCode(second & 0xF))
   {
      pc = mem;
      cycles += 3;
   }
}

//===== CALL cc,mem
void DST_dstCALL(void)
{
   cycles = 6;
   if (conditionCode(second & 0xF))
   {
      push32(pc);
      pc = mem;
      cycles += 6;
   }
}

//===== RET cc
void DST_dstRET(void)
{
   cycles = 6;
   if (conditionCode(second & 0xF))
   {
      pc = pop32();
      cycles += 6;
   }
}

//=============================================================================
