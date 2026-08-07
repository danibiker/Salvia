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

	TLCS900h_interpret_dst.h

//=========================================================================
//---------------------------------------------------------------------------

  History of changes:
  ===================

20 JUL 2002 - neopop_uk
=======================================
- Cleaned and tidied up for the source release

//---------------------------------------------------------------------------
*/

#ifndef __TLCS900H_DST__
#define __TLCS900H_DST__

//=========================================================================

#ifdef __cplusplus
extern "C" {
#endif

//===== LD (mem),#
void DST_dstLDBi(void);

//===== LD (mem),#
void DST_dstLDWi(void);

//===== POP (mem)
void DST_dstPOPB(void);

//===== POP (mem)
void DST_dstPOPW(void);

//===== LD (mem),(nn)
void DST_dstLDBm16(void);

//===== LD (mem),(nn)
void DST_dstLDWm16(void);

//===== LDA R,mem
void DST_dstLDAW(void);

//===== LDA R,mem
void DST_dstLDAL(void);

//===== ANDCF A,(mem)
void DST_dstANDCFA(void);

//===== ORCF A,(mem)
void DST_dstORCFA(void);

//===== XORCF A,(mem)
void DST_dstXORCFA(void);

//===== LDCF A,(mem)
void DST_dstLDCFA(void);

//===== STCF A,(mem)
void DST_dstSTCFA(void);

//===== LD (mem),R
void DST_dstLDBR(void);

//===== LD (mem),R
void DST_dstLDWR(void);

//===== LD (mem),R
void DST_dstLDLR(void);

//===== ANDCF #3,(mem)
void DST_dstANDCF(void);

//===== ORCF #3,(mem)
void DST_dstORCF(void);

//===== XORCF #3,(mem)
void DST_dstXORCF(void);

//===== LDCF #3,(mem)
void DST_dstLDCF(void);

//===== STCF #3,(mem)
void DST_dstSTCF(void);

//===== TSET #3,(mem)
void DST_dstTSET(void);

//===== RES #3,(mem)
void DST_dstRES(void);

//===== SET #3,(mem)
void DST_dstSET(void);

//===== CHG #3,(mem)
void DST_dstCHG(void);

//===== BIT #3,(mem)
void DST_dstBIT(void);

//===== JP cc,mem
void DST_dstJP(void);

//===== CALL cc,mem
void DST_dstCALL(void);

//===== RET cc
void DST_dstRET(void);

#ifdef __cplusplus
}
#endif

//=========================================================================
#endif
