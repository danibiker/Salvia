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

	TLCS900h_registers.c

//=========================================================================
//---------------------------------------------------------------------------

  History of changes:
  ===================

20 JUL 2002 - neopop_uk
=======================================
- Cleaned and tidied up for the source release

10 AUG 2002 - neopop_uk
=======================================
- Moved default PC setup to the 'reset_registers' function.

//---------------------------------------------------------------------------
*/

#include <string.h>
#include "TLCS900h_registers.h"
#include "../neopop.h"
#include "../interrupt.h"
#include "../rom.h"

#ifdef MSB_FIRST
#define BYTE0	3
#define BYTE1	2
#define BYTE2	1
#define BYTE3	0
#define WORD0	2
#define WORD1	0
#else
#define BYTE0	0
#define BYTE1	1
#define BYTE2	2
#define BYTE3	3
#define WORD0	0
#define WORD1	2
#endif

//=============================================================================

uint32 pc, gprBank[4][4], gpr[4];
uint16 sr;
uint8 f_dash;

//=============================================================================

//Bank Data
uint8* gprMapB[4][8] =
{
   //Bank 0
   {
      (uint8*)&(gprBank[0][0]) + BYTE1,
      (uint8*)&(gprBank[0][0]) + BYTE0,
      (uint8*)&(gprBank[0][1]) + BYTE1,
      (uint8*)&(gprBank[0][1]) + BYTE0,
      (uint8*)&(gprBank[0][2]) + BYTE1,
      (uint8*)&(gprBank[0][2]) + BYTE0,
      (uint8*)&(gprBank[0][3]) + BYTE1,
      (uint8*)&(gprBank[0][3]) + BYTE0,
   },

   //Bank 1
   {
      (uint8*)&(gprBank[1][0]) + BYTE1,
      (uint8*)&(gprBank[1][0]) + BYTE0,
      (uint8*)&(gprBank[1][1]) + BYTE1,
      (uint8*)&(gprBank[1][1]) + BYTE0,
      (uint8*)&(gprBank[1][2]) + BYTE1,
      (uint8*)&(gprBank[1][2]) + BYTE0,
      (uint8*)&(gprBank[1][3]) + BYTE1,
      (uint8*)&(gprBank[1][3]) + BYTE0,
   },

   //Bank 2
   {
      (uint8*)&(gprBank[2][0]) + BYTE1,
      (uint8*)&(gprBank[2][0]) + BYTE0,
      (uint8*)&(gprBank[2][1]) + BYTE1,
      (uint8*)&(gprBank[2][1]) + BYTE0,
      (uint8*)&(gprBank[2][2]) + BYTE1,
      (uint8*)&(gprBank[2][2]) + BYTE0,
      (uint8*)&(gprBank[2][3]) + BYTE1,
      (uint8*)&(gprBank[2][3]) + BYTE0,
   },

   //Bank 3
   {
      (uint8*)&(gprBank[3][0]) + BYTE1,
      (uint8*)&(gprBank[3][0]) + BYTE0,
      (uint8*)&(gprBank[3][1]) + BYTE1,
      (uint8*)&(gprBank[3][1]) + BYTE0,
      (uint8*)&(gprBank[3][2]) + BYTE1,
      (uint8*)&(gprBank[3][2]) + BYTE0,
      (uint8*)&(gprBank[3][3]) + BYTE1,
      (uint8*)&(gprBank[3][3]) + BYTE0,
   }
};

uint16* gprMapW[4][8] =
{
   //Bank 0
   {
      (uint16*)(((uint8*)&gprBank[0][0]) + WORD0),
      (uint16*)(((uint8*)&gprBank[0][1]) + WORD0),
      (uint16*)(((uint8*)&gprBank[0][2]) + WORD0),
      (uint16*)(((uint8*)&gprBank[0][3]) + WORD0),
      (uint16*)(((uint8*)&gpr[0]) + WORD0),
      (uint16*)(((uint8*)&gpr[1]) + WORD0),
      (uint16*)(((uint8*)&gpr[2]) + WORD0),
      (uint16*)(((uint8*)&gpr[3]) + WORD0),
   },

   //Bank 1
   {
      (uint16*)(((uint8*)&gprBank[1][0]) + WORD0),
      (uint16*)(((uint8*)&gprBank[1][1]) + WORD0),
      (uint16*)(((uint8*)&gprBank[1][2]) + WORD0),
      (uint16*)(((uint8*)&gprBank[1][3]) + WORD0),
      (uint16*)(((uint8*)&gpr[0]) + WORD0),
      (uint16*)(((uint8*)&gpr[1]) + WORD0),
      (uint16*)(((uint8*)&gpr[2]) + WORD0),
      (uint16*)(((uint8*)&gpr[3]) + WORD0),
   },

   //Bank 2
   {
      (uint16*)(((uint8*)&gprBank[2][0]) + WORD0),
      (uint16*)(((uint8*)&gprBank[2][1]) + WORD0),
      (uint16*)(((uint8*)&gprBank[2][2]) + WORD0),
      (uint16*)(((uint8*)&gprBank[2][3]) + WORD0),
      (uint16*)(((uint8*)&gpr[0]) + WORD0),
      (uint16*)(((uint8*)&gpr[1]) + WORD0),
      (uint16*)(((uint8*)&gpr[2]) + WORD0),
      (uint16*)(((uint8*)&gpr[3]) + WORD0),
   },

   //Bank 3
   {
      (uint16*)(((uint8*)&gprBank[3][0]) + WORD0),
      (uint16*)(((uint8*)&gprBank[3][1]) + WORD0),
      (uint16*)(((uint8*)&gprBank[3][2]) + WORD0),
      (uint16*)(((uint8*)&gprBank[3][3]) + WORD0),
      (uint16*)(((uint8*)&gpr[0]) + WORD0),
      (uint16*)(((uint8*)&gpr[1]) + WORD0),
      (uint16*)(((uint8*)&gpr[2]) + WORD0),
      (uint16*)(((uint8*)&gpr[3]) + WORD0),
   },
};

uint32* gprMapL[4][8] =
{
   //Bank 0
   {
      (uint32*)&(gprBank[0][0]),
      (uint32*)&(gprBank[0][1]),
      (uint32*)&(gprBank[0][2]),
      (uint32*)&(gprBank[0][3]),
      (uint32*)&(gpr[0]),
      (uint32*)&(gpr[1]),
      (uint32*)&(gpr[2]),
      (uint32*)&(gpr[3]),
   },

   //Bank 1
   {
      (uint32*)&(gprBank[1][0]),
      (uint32*)&(gprBank[1][1]),
      (uint32*)&(gprBank[1][2]),
      (uint32*)&(gprBank[1][3]),
      (uint32*)&(gpr[0]),
      (uint32*)&(gpr[1]),
      (uint32*)&(gpr[2]),
      (uint32*)&(gpr[3]),
   },

   //Bank 2
   {
      (uint32*)&(gprBank[2][0]),
      (uint32*)&(gprBank[2][1]),
      (uint32*)&(gprBank[2][2]),
      (uint32*)&(gprBank[2][3]),
      (uint32*)&(gpr[0]),
      (uint32*)&(gpr[1]),
      (uint32*)&(gpr[2]),
      (uint32*)&(gpr[3]),
   },

   //Bank 3
   {
      (uint32*)&(gprBank[3][0]),
      (uint32*)&(gprBank[3][1]),
      (uint32*)&(gprBank[3][2]),
      (uint32*)&(gprBank[3][3]),
      (uint32*)&(gpr[0]),
      (uint32*)&(gpr[1]),
      (uint32*)&(gpr[2]),
      (uint32*)&(gpr[3]),
   },
};

//=============================================================================

uint32 rErr;

uint8* regCodeMapB[4][256] =
{
   {

      ((uint8*)&gprBank[0][0]) + BYTE0,((uint8*)&gprBank[0][0]) + BYTE1,			//BANK 0
      ((uint8*)&gprBank[0][0]) + BYTE2, ((uint8*)&gprBank[0][0]) + BYTE3,
      ((uint8*)&gprBank[0][1]) + BYTE0,((uint8*)&gprBank[0][1]) + BYTE1,
      ((uint8*)&gprBank[0][1]) + BYTE2, ((uint8*)&gprBank[0][1]) + BYTE3,
      ((uint8*)&gprBank[0][2]) + BYTE0,((uint8*)&gprBank[0][2]) + BYTE1,
      ((uint8*)&gprBank[0][2]) + BYTE2, ((uint8*)&gprBank[0][2]) + BYTE3,
      ((uint8*)&gprBank[0][3]) + BYTE0,((uint8*)&gprBank[0][3]) + BYTE1,
      ((uint8*)&gprBank[0][3]) + BYTE2, ((uint8*)&gprBank[0][3]) + BYTE3,

      ((uint8*)&gprBank[1][0]) + BYTE0,((uint8*)&gprBank[1][0]) + BYTE1,			//BANK 1
      ((uint8*)&gprBank[1][0]) + BYTE2, ((uint8*)&gprBank[1][0]) + BYTE3,
      ((uint8*)&gprBank[1][1]) + BYTE0,((uint8*)&gprBank[1][1]) + BYTE1,
      ((uint8*)&gprBank[1][1]) + BYTE2, ((uint8*)&gprBank[1][1]) + BYTE3,
      ((uint8*)&gprBank[1][2]) + BYTE0,((uint8*)&gprBank[1][2]) + BYTE1,
      ((uint8*)&gprBank[1][2]) + BYTE2, ((uint8*)&gprBank[1][2]) + BYTE3,
      ((uint8*)&gprBank[1][3]) + BYTE0,((uint8*)&gprBank[1][3]) + BYTE1,
      ((uint8*)&gprBank[1][3]) + BYTE2, ((uint8*)&gprBank[1][3]) + BYTE3,

      ((uint8*)&gprBank[2][0]) + BYTE0,((uint8*)&gprBank[2][0]) + BYTE1,			//BANK 2
      ((uint8*)&gprBank[2][0]) + BYTE2, ((uint8*)&gprBank[2][0]) + BYTE3,
      ((uint8*)&gprBank[2][1]) + BYTE0,((uint8*)&gprBank[2][1]) + BYTE1,
      ((uint8*)&gprBank[2][1]) + BYTE2, ((uint8*)&gprBank[2][1]) + BYTE3,
      ((uint8*)&gprBank[2][2]) + BYTE0,((uint8*)&gprBank[2][2]) + BYTE1,
      ((uint8*)&gprBank[2][2]) + BYTE2, ((uint8*)&gprBank[2][2]) + BYTE3,
      ((uint8*)&gprBank[2][3]) + BYTE0,((uint8*)&gprBank[2][3]) + BYTE1,
      ((uint8*)&gprBank[2][3]) + BYTE2, ((uint8*)&gprBank[2][3]) + BYTE3,

      ((uint8*)&gprBank[3][0]) + BYTE0,((uint8*)&gprBank[3][0]) + BYTE1,			//BANK 3
      ((uint8*)&gprBank[3][0]) + BYTE2, ((uint8*)&gprBank[3][0]) + BYTE3,
      ((uint8*)&gprBank[3][1]) + BYTE0,((uint8*)&gprBank[3][1]) + BYTE1,
      ((uint8*)&gprBank[3][1]) + BYTE2, ((uint8*)&gprBank[3][1]) + BYTE3,
      ((uint8*)&gprBank[3][2]) + BYTE0,((uint8*)&gprBank[3][2]) + BYTE1,
      ((uint8*)&gprBank[3][2]) + BYTE2, ((uint8*)&gprBank[3][2]) + BYTE3,
      ((uint8*)&gprBank[3][3]) + BYTE0,((uint8*)&gprBank[3][3]) + BYTE1,
      ((uint8*)&gprBank[3][3]) + BYTE2, ((uint8*)&gprBank[3][3]) + BYTE3,

      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,

      //Previous Bank
      (uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,

      //Current Bank
      ((uint8*)&gprBank[0][0]) + BYTE0,((uint8*)&gprBank[0][0]) + BYTE1,
      ((uint8*)&gprBank[0][0]) + BYTE2, ((uint8*)&gprBank[0][0]) + BYTE3,
      ((uint8*)&gprBank[0][1]) + BYTE0,((uint8*)&gprBank[0][1]) + BYTE1,
      ((uint8*)&gprBank[0][1]) + BYTE2, ((uint8*)&gprBank[0][1]) + BYTE3,
      ((uint8*)&gprBank[0][2]) + BYTE0,((uint8*)&gprBank[0][2]) + BYTE1,
      ((uint8*)&gprBank[0][2]) + BYTE2, ((uint8*)&gprBank[0][2]) + BYTE3,
      ((uint8*)&gprBank[0][3]) + BYTE0,((uint8*)&gprBank[0][3]) + BYTE1,
      ((uint8*)&gprBank[0][3]) + BYTE2, ((uint8*)&gprBank[0][3]) + BYTE3,

      ((uint8*)&gpr[0]) + BYTE0, ((uint8*)&gpr[0]) + BYTE1, 
      ((uint8*)&gpr[0]) + BYTE2, ((uint8*)&gpr[0]) + BYTE3,
      ((uint8*)&gpr[1]) + BYTE0, ((uint8*)&gpr[1]) + BYTE1, 
      ((uint8*)&gpr[1]) + BYTE2, ((uint8*)&gpr[1]) + BYTE3,
      ((uint8*)&gpr[2]) + BYTE0, ((uint8*)&gpr[2]) + BYTE1, 
      ((uint8*)&gpr[2]) + BYTE2, ((uint8*)&gpr[2]) + BYTE3,
      ((uint8*)&gpr[3]) + BYTE0, ((uint8*)&gpr[3]) + BYTE1,
      ((uint8*)&gpr[3]) + BYTE2, ((uint8*)&gpr[3]) + BYTE3
   },

   {

      ((uint8*)&gprBank[0][0]) + BYTE0,((uint8*)&gprBank[0][0]) + BYTE1,			//BANK 0
      ((uint8*)&gprBank[0][0]) + BYTE2, ((uint8*)&gprBank[0][0]) + BYTE3,
      ((uint8*)&gprBank[0][1]) + BYTE0,((uint8*)&gprBank[0][1]) + BYTE1,
      ((uint8*)&gprBank[0][1]) + BYTE2, ((uint8*)&gprBank[0][1]) + BYTE3,
      ((uint8*)&gprBank[0][2]) + BYTE0,((uint8*)&gprBank[0][2]) + BYTE1,
      ((uint8*)&gprBank[0][2]) + BYTE2, ((uint8*)&gprBank[0][2]) + BYTE3,
      ((uint8*)&gprBank[0][3]) + BYTE0,((uint8*)&gprBank[0][3]) + BYTE1,
      ((uint8*)&gprBank[0][3]) + BYTE2, ((uint8*)&gprBank[0][3]) + BYTE3,

      ((uint8*)&gprBank[1][0]) + BYTE0,((uint8*)&gprBank[1][0]) + BYTE1,			//BANK 1
      ((uint8*)&gprBank[1][0]) + BYTE2, ((uint8*)&gprBank[1][0]) + BYTE3,
      ((uint8*)&gprBank[1][1]) + BYTE0,((uint8*)&gprBank[1][1]) + BYTE1,
      ((uint8*)&gprBank[1][1]) + BYTE2, ((uint8*)&gprBank[1][1]) + BYTE3,
      ((uint8*)&gprBank[1][2]) + BYTE0,((uint8*)&gprBank[1][2]) + BYTE1,
      ((uint8*)&gprBank[1][2]) + BYTE2, ((uint8*)&gprBank[1][2]) + BYTE3,
      ((uint8*)&gprBank[1][3]) + BYTE0,((uint8*)&gprBank[1][3]) + BYTE1,
      ((uint8*)&gprBank[1][3]) + BYTE2, ((uint8*)&gprBank[1][3]) + BYTE3,

      ((uint8*)&gprBank[2][0]) + BYTE0,((uint8*)&gprBank[2][0]) + BYTE1,			//BANK 2
      ((uint8*)&gprBank[2][0]) + BYTE2, ((uint8*)&gprBank[2][0]) + BYTE3,
      ((uint8*)&gprBank[2][1]) + BYTE0,((uint8*)&gprBank[2][1]) + BYTE1,
      ((uint8*)&gprBank[2][1]) + BYTE2, ((uint8*)&gprBank[2][1]) + BYTE3,
      ((uint8*)&gprBank[2][2]) + BYTE0,((uint8*)&gprBank[2][2]) + BYTE1,
      ((uint8*)&gprBank[2][2]) + BYTE2, ((uint8*)&gprBank[2][2]) + BYTE3,
      ((uint8*)&gprBank[2][3]) + BYTE0,((uint8*)&gprBank[2][3]) + BYTE1,
      ((uint8*)&gprBank[2][3]) + BYTE2, ((uint8*)&gprBank[2][3]) + BYTE3,

      ((uint8*)&gprBank[3][0]) + BYTE0,((uint8*)&gprBank[3][0]) + BYTE1,			//BANK 3
      ((uint8*)&gprBank[3][0]) + BYTE2, ((uint8*)&gprBank[3][0]) + BYTE3,
      ((uint8*)&gprBank[3][1]) + BYTE0,((uint8*)&gprBank[3][1]) + BYTE1,
      ((uint8*)&gprBank[3][1]) + BYTE2, ((uint8*)&gprBank[3][1]) + BYTE3,
      ((uint8*)&gprBank[3][2]) + BYTE0,((uint8*)&gprBank[3][2]) + BYTE1,
      ((uint8*)&gprBank[3][2]) + BYTE2, ((uint8*)&gprBank[3][2]) + BYTE3,
      ((uint8*)&gprBank[3][3]) + BYTE0,((uint8*)&gprBank[3][3]) + BYTE1,
      ((uint8*)&gprBank[3][3]) + BYTE2, ((uint8*)&gprBank[3][3]) + BYTE3,

      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,

      //Previous Bank
      ((uint8*)&gprBank[0][0]) + BYTE0,((uint8*)&gprBank[0][0]) + BYTE1,
      ((uint8*)&gprBank[0][0]) + BYTE2, ((uint8*)&gprBank[0][0]) + BYTE3,
      ((uint8*)&gprBank[0][1]) + BYTE0,((uint8*)&gprBank[0][1]) + BYTE1,
      ((uint8*)&gprBank[0][1]) + BYTE2, ((uint8*)&gprBank[0][1]) + BYTE3,
      ((uint8*)&gprBank[0][2]) + BYTE0,((uint8*)&gprBank[0][2]) + BYTE1,
      ((uint8*)&gprBank[0][2]) + BYTE2, ((uint8*)&gprBank[0][2]) + BYTE3,
      ((uint8*)&gprBank[0][3]) + BYTE0,((uint8*)&gprBank[0][3]) + BYTE1,
      ((uint8*)&gprBank[0][3]) + BYTE2, ((uint8*)&gprBank[0][3]) + BYTE3,

      //Current Bank
      ((uint8*)&gprBank[1][0]) + BYTE0,((uint8*)&gprBank[1][0]) + BYTE1,
      ((uint8*)&gprBank[1][0]) + BYTE2, ((uint8*)&gprBank[1][0]) + BYTE3,
      ((uint8*)&gprBank[1][1]) + BYTE0,((uint8*)&gprBank[1][1]) + BYTE1,
      ((uint8*)&gprBank[1][1]) + BYTE2, ((uint8*)&gprBank[1][1]) + BYTE3,
      ((uint8*)&gprBank[1][2]) + BYTE0,((uint8*)&gprBank[1][2]) + BYTE1,
      ((uint8*)&gprBank[1][2]) + BYTE2, ((uint8*)&gprBank[1][2]) + BYTE3,
      ((uint8*)&gprBank[1][3]) + BYTE0,((uint8*)&gprBank[1][3]) + BYTE1,
      ((uint8*)&gprBank[1][3]) + BYTE2, ((uint8*)&gprBank[1][3]) + BYTE3,

      ((uint8*)&gpr[0]) + BYTE0, ((uint8*)&gpr[0]) + BYTE1, 
      ((uint8*)&gpr[0]) + BYTE2, ((uint8*)&gpr[0]) + BYTE3,
      ((uint8*)&gpr[1]) + BYTE0, ((uint8*)&gpr[1]) + BYTE1, 
      ((uint8*)&gpr[1]) + BYTE2, ((uint8*)&gpr[1]) + BYTE3,
      ((uint8*)&gpr[2]) + BYTE0, ((uint8*)&gpr[2]) + BYTE1, 
      ((uint8*)&gpr[2]) + BYTE2, ((uint8*)&gpr[2]) + BYTE3,
      ((uint8*)&gpr[3]) + BYTE0, ((uint8*)&gpr[3]) + BYTE1,
      ((uint8*)&gpr[3]) + BYTE2, ((uint8*)&gpr[3]) + BYTE3
   },

   {

      ((uint8*)&gprBank[0][0]) + BYTE0,((uint8*)&gprBank[0][0]) + BYTE1,			//BANK 0
      ((uint8*)&gprBank[0][0]) + BYTE2, ((uint8*)&gprBank[0][0]) + BYTE3,
      ((uint8*)&gprBank[0][1]) + BYTE0,((uint8*)&gprBank[0][1]) + BYTE1,
      ((uint8*)&gprBank[0][1]) + BYTE2, ((uint8*)&gprBank[0][1]) + BYTE3,
      ((uint8*)&gprBank[0][2]) + BYTE0,((uint8*)&gprBank[0][2]) + BYTE1,
      ((uint8*)&gprBank[0][2]) + BYTE2, ((uint8*)&gprBank[0][2]) + BYTE3,
      ((uint8*)&gprBank[0][3]) + BYTE0,((uint8*)&gprBank[0][3]) + BYTE1,
      ((uint8*)&gprBank[0][3]) + BYTE2, ((uint8*)&gprBank[0][3]) + BYTE3,

      ((uint8*)&gprBank[1][0]) + BYTE0,((uint8*)&gprBank[1][0]) + BYTE1,			//BANK 1
      ((uint8*)&gprBank[1][0]) + BYTE2, ((uint8*)&gprBank[1][0]) + BYTE3,
      ((uint8*)&gprBank[1][1]) + BYTE0,((uint8*)&gprBank[1][1]) + BYTE1,
      ((uint8*)&gprBank[1][1]) + BYTE2, ((uint8*)&gprBank[1][1]) + BYTE3,
      ((uint8*)&gprBank[1][2]) + BYTE0,((uint8*)&gprBank[1][2]) + BYTE1,
      ((uint8*)&gprBank[1][2]) + BYTE2, ((uint8*)&gprBank[1][2]) + BYTE3,
      ((uint8*)&gprBank[1][3]) + BYTE0,((uint8*)&gprBank[1][3]) + BYTE1,
      ((uint8*)&gprBank[1][3]) + BYTE2, ((uint8*)&gprBank[1][3]) + BYTE3,

      ((uint8*)&gprBank[2][0]) + BYTE0,((uint8*)&gprBank[2][0]) + BYTE1,			//BANK 2
      ((uint8*)&gprBank[2][0]) + BYTE2, ((uint8*)&gprBank[2][0]) + BYTE3,
      ((uint8*)&gprBank[2][1]) + BYTE0,((uint8*)&gprBank[2][1]) + BYTE1,
      ((uint8*)&gprBank[2][1]) + BYTE2, ((uint8*)&gprBank[2][1]) + BYTE3,
      ((uint8*)&gprBank[2][2]) + BYTE0,((uint8*)&gprBank[2][2]) + BYTE1,
      ((uint8*)&gprBank[2][2]) + BYTE2, ((uint8*)&gprBank[2][2]) + BYTE3,
      ((uint8*)&gprBank[2][3]) + BYTE0,((uint8*)&gprBank[2][3]) + BYTE1,
      ((uint8*)&gprBank[2][3]) + BYTE2, ((uint8*)&gprBank[2][3]) + BYTE3,

      ((uint8*)&gprBank[3][0]) + BYTE0,((uint8*)&gprBank[3][0]) + BYTE1,			//BANK 3
      ((uint8*)&gprBank[3][0]) + BYTE2, ((uint8*)&gprBank[3][0]) + BYTE3,
      ((uint8*)&gprBank[3][1]) + BYTE0,((uint8*)&gprBank[3][1]) + BYTE1,
      ((uint8*)&gprBank[3][1]) + BYTE2, ((uint8*)&gprBank[3][1]) + BYTE3,
      ((uint8*)&gprBank[3][2]) + BYTE0,((uint8*)&gprBank[3][2]) + BYTE1,
      ((uint8*)&gprBank[3][2]) + BYTE2, ((uint8*)&gprBank[3][2]) + BYTE3,
      ((uint8*)&gprBank[3][3]) + BYTE0,((uint8*)&gprBank[3][3]) + BYTE1,
      ((uint8*)&gprBank[3][3]) + BYTE2, ((uint8*)&gprBank[3][3]) + BYTE3,

      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,

      //Previous Bank
      ((uint8*)&gprBank[1][0]) + BYTE0,((uint8*)&gprBank[1][0]) + BYTE1,
      ((uint8*)&gprBank[1][0]) + BYTE2, ((uint8*)&gprBank[1][0]) + BYTE3,
      ((uint8*)&gprBank[1][1]) + BYTE0,((uint8*)&gprBank[1][1]) + BYTE1,
      ((uint8*)&gprBank[1][1]) + BYTE2, ((uint8*)&gprBank[1][1]) + BYTE3,
      ((uint8*)&gprBank[1][2]) + BYTE0,((uint8*)&gprBank[1][2]) + BYTE1,
      ((uint8*)&gprBank[1][2]) + BYTE2, ((uint8*)&gprBank[1][2]) + BYTE3,
      ((uint8*)&gprBank[1][3]) + BYTE0,((uint8*)&gprBank[1][3]) + BYTE1,
      ((uint8*)&gprBank[1][3]) + BYTE2, ((uint8*)&gprBank[1][3]) + BYTE3,

      //Current Bank
      ((uint8*)&gprBank[2][0]) + BYTE0,((uint8*)&gprBank[2][0]) + BYTE1,
      ((uint8*)&gprBank[2][0]) + BYTE2, ((uint8*)&gprBank[2][0]) + BYTE3,
      ((uint8*)&gprBank[2][1]) + BYTE0,((uint8*)&gprBank[2][1]) + BYTE1,
      ((uint8*)&gprBank[2][1]) + BYTE2, ((uint8*)&gprBank[2][1]) + BYTE3,
      ((uint8*)&gprBank[2][2]) + BYTE0,((uint8*)&gprBank[2][2]) + BYTE1,
      ((uint8*)&gprBank[2][2]) + BYTE2, ((uint8*)&gprBank[2][2]) + BYTE3,
      ((uint8*)&gprBank[2][3]) + BYTE0,((uint8*)&gprBank[2][3]) + BYTE1,
      ((uint8*)&gprBank[2][3]) + BYTE2, ((uint8*)&gprBank[2][3]) + BYTE3,

      ((uint8*)&gpr[0]) + BYTE0, ((uint8*)&gpr[0]) + BYTE1, 
      ((uint8*)&gpr[0]) + BYTE2, ((uint8*)&gpr[0]) + BYTE3,
      ((uint8*)&gpr[1]) + BYTE0, ((uint8*)&gpr[1]) + BYTE1, 
      ((uint8*)&gpr[1]) + BYTE2, ((uint8*)&gpr[1]) + BYTE3,
      ((uint8*)&gpr[2]) + BYTE0, ((uint8*)&gpr[2]) + BYTE1, 
      ((uint8*)&gpr[2]) + BYTE2, ((uint8*)&gpr[2]) + BYTE3,
      ((uint8*)&gpr[3]) + BYTE0, ((uint8*)&gpr[3]) + BYTE1,
      ((uint8*)&gpr[3]) + BYTE2, ((uint8*)&gpr[3]) + BYTE3
   },

   {

      ((uint8*)&gprBank[0][0]) + BYTE0,((uint8*)&gprBank[0][0]) + BYTE1,			//BANK 0
      ((uint8*)&gprBank[0][0]) + BYTE2, ((uint8*)&gprBank[0][0]) + BYTE3,
      ((uint8*)&gprBank[0][1]) + BYTE0,((uint8*)&gprBank[0][1]) + BYTE1,
      ((uint8*)&gprBank[0][1]) + BYTE2, ((uint8*)&gprBank[0][1]) + BYTE3,
      ((uint8*)&gprBank[0][2]) + BYTE0,((uint8*)&gprBank[0][2]) + BYTE1,
      ((uint8*)&gprBank[0][2]) + BYTE2, ((uint8*)&gprBank[0][2]) + BYTE3,
      ((uint8*)&gprBank[0][3]) + BYTE0,((uint8*)&gprBank[0][3]) + BYTE1,
      ((uint8*)&gprBank[0][3]) + BYTE2, ((uint8*)&gprBank[0][3]) + BYTE3,

      ((uint8*)&gprBank[1][0]) + BYTE0,((uint8*)&gprBank[1][0]) + BYTE1,			//BANK 1
      ((uint8*)&gprBank[1][0]) + BYTE2, ((uint8*)&gprBank[1][0]) + BYTE3,
      ((uint8*)&gprBank[1][1]) + BYTE0,((uint8*)&gprBank[1][1]) + BYTE1,
      ((uint8*)&gprBank[1][1]) + BYTE2, ((uint8*)&gprBank[1][1]) + BYTE3,
      ((uint8*)&gprBank[1][2]) + BYTE0,((uint8*)&gprBank[1][2]) + BYTE1,
      ((uint8*)&gprBank[1][2]) + BYTE2, ((uint8*)&gprBank[1][2]) + BYTE3,
      ((uint8*)&gprBank[1][3]) + BYTE0,((uint8*)&gprBank[1][3]) + BYTE1,
      ((uint8*)&gprBank[1][3]) + BYTE2, ((uint8*)&gprBank[1][3]) + BYTE3,

      ((uint8*)&gprBank[2][0]) + BYTE0,((uint8*)&gprBank[2][0]) + BYTE1,			//BANK 2
      ((uint8*)&gprBank[2][0]) + BYTE2, ((uint8*)&gprBank[2][0]) + BYTE3,
      ((uint8*)&gprBank[2][1]) + BYTE0,((uint8*)&gprBank[2][1]) + BYTE1,
      ((uint8*)&gprBank[2][1]) + BYTE2, ((uint8*)&gprBank[2][1]) + BYTE3,
      ((uint8*)&gprBank[2][2]) + BYTE0,((uint8*)&gprBank[2][2]) + BYTE1,
      ((uint8*)&gprBank[2][2]) + BYTE2, ((uint8*)&gprBank[2][2]) + BYTE3,
      ((uint8*)&gprBank[2][3]) + BYTE0,((uint8*)&gprBank[2][3]) + BYTE1,
      ((uint8*)&gprBank[2][3]) + BYTE2, ((uint8*)&gprBank[2][3]) + BYTE3,

      ((uint8*)&gprBank[3][0]) + BYTE0,((uint8*)&gprBank[3][0]) + BYTE1,			//BANK 3
      ((uint8*)&gprBank[3][0]) + BYTE2, ((uint8*)&gprBank[3][0]) + BYTE3,
      ((uint8*)&gprBank[3][1]) + BYTE0,((uint8*)&gprBank[3][1]) + BYTE1,
      ((uint8*)&gprBank[3][1]) + BYTE2, ((uint8*)&gprBank[3][1]) + BYTE3,
      ((uint8*)&gprBank[3][2]) + BYTE0,((uint8*)&gprBank[3][2]) + BYTE1,
      ((uint8*)&gprBank[3][2]) + BYTE2, ((uint8*)&gprBank[3][2]) + BYTE3,
      ((uint8*)&gprBank[3][3]) + BYTE0,((uint8*)&gprBank[3][3]) + BYTE1,
      ((uint8*)&gprBank[3][3]) + BYTE2, ((uint8*)&gprBank[3][3]) + BYTE3,

      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,
      (uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,(uint8*)&rErr,

      //Previous Bank
      ((uint8*)&gprBank[2][0]) + BYTE0,((uint8*)&gprBank[2][0]) + BYTE1,
      ((uint8*)&gprBank[2][0]) + BYTE2, ((uint8*)&gprBank[2][0]) + BYTE3,
      ((uint8*)&gprBank[2][1]) + BYTE0,((uint8*)&gprBank[2][1]) + BYTE1,
      ((uint8*)&gprBank[2][1]) + BYTE2, ((uint8*)&gprBank[2][1]) + BYTE3,
      ((uint8*)&gprBank[2][2]) + BYTE0,((uint8*)&gprBank[2][2]) + BYTE1,
      ((uint8*)&gprBank[2][2]) + BYTE2, ((uint8*)&gprBank[2][2]) + BYTE3,
      ((uint8*)&gprBank[2][3]) + BYTE0,((uint8*)&gprBank[2][3]) + BYTE1,
      ((uint8*)&gprBank[2][3]) + BYTE2, ((uint8*)&gprBank[2][3]) + BYTE3,

      //Current Bank
      ((uint8*)&gprBank[3][0]) + BYTE0,((uint8*)&gprBank[3][0]) + BYTE1,
      ((uint8*)&gprBank[3][0]) + BYTE2, ((uint8*)&gprBank[3][0]) + BYTE3,
      ((uint8*)&gprBank[3][1]) + BYTE0,((uint8*)&gprBank[3][1]) + BYTE1,
      ((uint8*)&gprBank[3][1]) + BYTE2, ((uint8*)&gprBank[3][1]) + BYTE3,
      ((uint8*)&gprBank[3][2]) + BYTE0,((uint8*)&gprBank[3][2]) + BYTE1,
      ((uint8*)&gprBank[3][2]) + BYTE2, ((uint8*)&gprBank[3][2]) + BYTE3,
      ((uint8*)&gprBank[3][3]) + BYTE0,((uint8*)&gprBank[3][3]) + BYTE1,
      ((uint8*)&gprBank[3][3]) + BYTE2, ((uint8*)&gprBank[3][3]) + BYTE3,

      ((uint8*)&gpr[0]) + BYTE0, ((uint8*)&gpr[0]) + BYTE1, 
      ((uint8*)&gpr[0]) + BYTE2, ((uint8*)&gpr[0]) + BYTE3,
      ((uint8*)&gpr[1]) + BYTE0, ((uint8*)&gpr[1]) + BYTE1, 
      ((uint8*)&gpr[1]) + BYTE2, ((uint8*)&gpr[1]) + BYTE3,
      ((uint8*)&gpr[2]) + BYTE0, ((uint8*)&gpr[2]) + BYTE1, 
      ((uint8*)&gpr[2]) + BYTE2, ((uint8*)&gpr[2]) + BYTE3,
      ((uint8*)&gpr[3]) + BYTE0, ((uint8*)&gpr[3]) + BYTE1,
      ((uint8*)&gpr[3]) + BYTE2, ((uint8*)&gpr[3]) + BYTE3
   }
};

uint16* regCodeMapW[4][128] =
{
   {
      /* MAP CODE W0 */

      //BANK 0
      (uint16*)(((uint8*)&gprBank[0][0]) + WORD0), (uint16*)(((uint8*)&gprBank[0][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][1]) + WORD0), (uint16*)(((uint8*)&gprBank[0][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][2]) + WORD0), (uint16*)(((uint8*)&gprBank[0][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][3]) + WORD0), (uint16*)(((uint8*)&gprBank[0][3]) + WORD1),

      //BANK 1
      (uint16*)(((uint8*)&gprBank[1][0]) + WORD0), (uint16*)(((uint8*)&gprBank[1][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][1]) + WORD0), (uint16*)(((uint8*)&gprBank[1][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][2]) + WORD0), (uint16*)(((uint8*)&gprBank[1][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][3]) + WORD0), (uint16*)(((uint8*)&gprBank[1][3]) + WORD1),

      //BANK 2
      (uint16*)(((uint8*)&gprBank[2][0]) + WORD0), (uint16*)(((uint8*)&gprBank[2][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][1]) + WORD0), (uint16*)(((uint8*)&gprBank[2][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][2]) + WORD0), (uint16*)(((uint8*)&gprBank[2][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][3]) + WORD0), (uint16*)(((uint8*)&gprBank[2][3]) + WORD1),

      //BANK 3
      (uint16*)(((uint8*)&gprBank[3][0]) + WORD0), (uint16*)(((uint8*)&gprBank[3][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][1]) + WORD0), (uint16*)(((uint8*)&gprBank[3][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][2]) + WORD0), (uint16*)(((uint8*)&gprBank[3][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][3]) + WORD0), (uint16*)(((uint8*)&gprBank[3][3]) + WORD1),

      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,

      //Previous Bank
      (uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,

      //Current Bank
      (uint16*)(((uint8*)&gprBank[0][0]) + WORD0), (uint16*)(((uint8*)&gprBank[0][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][1]) + WORD0), (uint16*)(((uint8*)&gprBank[0][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][2]) + WORD0), (uint16*)(((uint8*)&gprBank[0][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][3]) + WORD0), (uint16*)(((uint8*)&gprBank[0][3]) + WORD1),

      (uint16*)((uint8*)&(gpr[0]) + WORD0),	(uint16*)((uint8*)&(gpr[0]) + WORD1),
      (uint16*)((uint8*)&(gpr[1]) + WORD0),	(uint16*)((uint8*)&(gpr[1]) + WORD1),
      (uint16*)((uint8*)&(gpr[2]) + WORD0),	(uint16*)((uint8*)&(gpr[2]) + WORD1),
      (uint16*)((uint8*)&(gpr[3]) + WORD0),	(uint16*)((uint8*)&(gpr[3]) + WORD1),
   },

   {
      /* MAP CODE W1 */

      //BANK 0
      (uint16*)(((uint8*)&gprBank[0][0]) + WORD0), (uint16*)(((uint8*)&gprBank[0][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][1]) + WORD0), (uint16*)(((uint8*)&gprBank[0][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][2]) + WORD0), (uint16*)(((uint8*)&gprBank[0][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][3]) + WORD0), (uint16*)(((uint8*)&gprBank[0][3]) + WORD1),

      //BANK 1
      (uint16*)(((uint8*)&gprBank[1][0]) + WORD0), (uint16*)(((uint8*)&gprBank[1][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][1]) + WORD0), (uint16*)(((uint8*)&gprBank[1][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][2]) + WORD0), (uint16*)(((uint8*)&gprBank[1][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][3]) + WORD0), (uint16*)(((uint8*)&gprBank[1][3]) + WORD1),

      //BANK 2
      (uint16*)(((uint8*)&gprBank[2][0]) + WORD0), (uint16*)(((uint8*)&gprBank[2][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][1]) + WORD0), (uint16*)(((uint8*)&gprBank[2][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][2]) + WORD0), (uint16*)(((uint8*)&gprBank[2][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][3]) + WORD0), (uint16*)(((uint8*)&gprBank[2][3]) + WORD1),

      //BANK 3
      (uint16*)(((uint8*)&gprBank[3][0]) + WORD0), (uint16*)(((uint8*)&gprBank[3][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][1]) + WORD0), (uint16*)(((uint8*)&gprBank[3][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][2]) + WORD0), (uint16*)(((uint8*)&gprBank[3][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][3]) + WORD0), (uint16*)(((uint8*)&gprBank[3][3]) + WORD1),

      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,

      //Previous Bank
      (uint16*)(((uint8*)&gprBank[0][0]) + WORD0), (uint16*)(((uint8*)&gprBank[0][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][1]) + WORD0), (uint16*)(((uint8*)&gprBank[0][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][2]) + WORD0), (uint16*)(((uint8*)&gprBank[0][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][3]) + WORD0), (uint16*)(((uint8*)&gprBank[0][3]) + WORD1),

      //Current Bank
      (uint16*)(((uint8*)&gprBank[1][0]) + WORD0), (uint16*)(((uint8*)&gprBank[1][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][1]) + WORD0), (uint16*)(((uint8*)&gprBank[1][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][2]) + WORD0), (uint16*)(((uint8*)&gprBank[1][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][3]) + WORD0), (uint16*)(((uint8*)&gprBank[1][3]) + WORD1),

      (uint16*)((uint8*)&(gpr[0]) + WORD0),	(uint16*)((uint8*)&(gpr[0]) + WORD1),
      (uint16*)((uint8*)&(gpr[1]) + WORD0),	(uint16*)((uint8*)&(gpr[1]) + WORD1),
      (uint16*)((uint8*)&(gpr[2]) + WORD0),	(uint16*)((uint8*)&(gpr[2]) + WORD1),
      (uint16*)((uint8*)&(gpr[3]) + WORD0),	(uint16*)((uint8*)&(gpr[3]) + WORD1),
   },

   {
      /* MAP CODE W2 */
      //BANK 0
      (uint16*)(((uint8*)&gprBank[0][0]) + WORD0), (uint16*)(((uint8*)&gprBank[0][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][1]) + WORD0), (uint16*)(((uint8*)&gprBank[0][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][2]) + WORD0), (uint16*)(((uint8*)&gprBank[0][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][3]) + WORD0), (uint16*)(((uint8*)&gprBank[0][3]) + WORD1),

      //BANK 1
      (uint16*)(((uint8*)&gprBank[1][0]) + WORD0), (uint16*)(((uint8*)&gprBank[1][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][1]) + WORD0), (uint16*)(((uint8*)&gprBank[1][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][2]) + WORD0), (uint16*)(((uint8*)&gprBank[1][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][3]) + WORD0), (uint16*)(((uint8*)&gprBank[1][3]) + WORD1),

      //BANK 2
      (uint16*)(((uint8*)&gprBank[2][0]) + WORD0), (uint16*)(((uint8*)&gprBank[2][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][1]) + WORD0), (uint16*)(((uint8*)&gprBank[2][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][2]) + WORD0), (uint16*)(((uint8*)&gprBank[2][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][3]) + WORD0), (uint16*)(((uint8*)&gprBank[2][3]) + WORD1),

      //BANK 3
      (uint16*)(((uint8*)&gprBank[3][0]) + WORD0), (uint16*)(((uint8*)&gprBank[3][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][1]) + WORD0), (uint16*)(((uint8*)&gprBank[3][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][2]) + WORD0), (uint16*)(((uint8*)&gprBank[3][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][3]) + WORD0), (uint16*)(((uint8*)&gprBank[3][3]) + WORD1),

      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,

      //Previous Bank
      (uint16*)(((uint8*)&gprBank[1][0]) + WORD0), (uint16*)(((uint8*)&gprBank[1][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][1]) + WORD0), (uint16*)(((uint8*)&gprBank[1][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][2]) + WORD0), (uint16*)(((uint8*)&gprBank[1][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][3]) + WORD0), (uint16*)(((uint8*)&gprBank[1][3]) + WORD1),

      //Current Bank
      (uint16*)(((uint8*)&gprBank[2][0]) + WORD0), (uint16*)(((uint8*)&gprBank[2][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][1]) + WORD0), (uint16*)(((uint8*)&gprBank[2][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][2]) + WORD0), (uint16*)(((uint8*)&gprBank[2][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][3]) + WORD0), (uint16*)(((uint8*)&gprBank[2][3]) + WORD1),

      (uint16*)((uint8*)&(gpr[0]) + WORD0),	(uint16*)((uint8*)&(gpr[0]) + WORD1),
      (uint16*)((uint8*)&(gpr[1]) + WORD0),	(uint16*)((uint8*)&(gpr[1]) + WORD1),
      (uint16*)((uint8*)&(gpr[2]) + WORD0),	(uint16*)((uint8*)&(gpr[2]) + WORD1),
      (uint16*)((uint8*)&(gpr[3]) + WORD0),	(uint16*)((uint8*)&(gpr[3]) + WORD1),
   },

   {
      /* MAP CODE W3 */

      //BANK 0
      (uint16*)(((uint8*)&gprBank[0][0]) + WORD0), (uint16*)(((uint8*)&gprBank[0][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][1]) + WORD0), (uint16*)(((uint8*)&gprBank[0][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][2]) + WORD0), (uint16*)(((uint8*)&gprBank[0][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[0][3]) + WORD0), (uint16*)(((uint8*)&gprBank[0][3]) + WORD1),

      //BANK 1
      (uint16*)(((uint8*)&gprBank[1][0]) + WORD0), (uint16*)(((uint8*)&gprBank[1][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][1]) + WORD0), (uint16*)(((uint8*)&gprBank[1][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][2]) + WORD0), (uint16*)(((uint8*)&gprBank[1][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[1][3]) + WORD0), (uint16*)(((uint8*)&gprBank[1][3]) + WORD1),

      //BANK 2
      (uint16*)(((uint8*)&gprBank[2][0]) + WORD0), (uint16*)(((uint8*)&gprBank[2][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][1]) + WORD0), (uint16*)(((uint8*)&gprBank[2][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][2]) + WORD0), (uint16*)(((uint8*)&gprBank[2][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][3]) + WORD0), (uint16*)(((uint8*)&gprBank[2][3]) + WORD1),

      //BANK 3
      (uint16*)(((uint8*)&gprBank[3][0]) + WORD0), (uint16*)(((uint8*)&gprBank[3][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][1]) + WORD0), (uint16*)(((uint8*)&gprBank[3][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][2]) + WORD0), (uint16*)(((uint8*)&gprBank[3][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][3]) + WORD0), (uint16*)(((uint8*)&gprBank[3][3]) + WORD1),

      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,
      (uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,(uint16*)&rErr,

      //Previous Bank
      (uint16*)(((uint8*)&gprBank[2][0]) + WORD0), (uint16*)(((uint8*)&gprBank[2][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][1]) + WORD0), (uint16*)(((uint8*)&gprBank[2][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][2]) + WORD0), (uint16*)(((uint8*)&gprBank[2][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[2][3]) + WORD0), (uint16*)(((uint8*)&gprBank[2][3]) + WORD1),

      //Current Bank
      (uint16*)(((uint8*)&gprBank[3][0]) + WORD0), (uint16*)(((uint8*)&gprBank[3][0]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][1]) + WORD0), (uint16*)(((uint8*)&gprBank[3][1]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][2]) + WORD0), (uint16*)(((uint8*)&gprBank[3][2]) + WORD1),
      (uint16*)(((uint8*)&gprBank[3][3]) + WORD0), (uint16*)(((uint8*)&gprBank[3][3]) + WORD1),

      (uint16*)((uint8*)&(gpr[0]) + WORD0),	(uint16*)((uint8*)&(gpr[0]) + WORD1),
      (uint16*)((uint8*)&(gpr[1]) + WORD0),	(uint16*)((uint8*)&(gpr[1]) + WORD1),
      (uint16*)((uint8*)&(gpr[2]) + WORD0),	(uint16*)((uint8*)&(gpr[2]) + WORD1),
      (uint16*)((uint8*)&(gpr[3]) + WORD0),	(uint16*)((uint8*)&(gpr[3]) + WORD1),
   }
};

uint32* regCodeMapL[4][64] =
{
   {
      /* MAP CODE L0 */

      //BANK 0
      &(gprBank[0][0]), &(gprBank[0][1]),	&(gprBank[0][2]), &(gprBank[0][3]),

      //BANK 1
      &(gprBank[1][0]), &(gprBank[1][1]), &(gprBank[1][2]), &(gprBank[1][3]),

      //BANK 2
      &(gprBank[2][0]), &(gprBank[2][1]),	&(gprBank[2][2]), &(gprBank[2][3]),

      //BANK 3
      &(gprBank[3][0]), &(gprBank[3][1]),	&(gprBank[3][2]), &(gprBank[3][3]),

      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,

      //Previous Bank
      &rErr,&rErr,&rErr,&rErr,

      //Current Bank
      &(gprBank[0][0]), &(gprBank[0][1]),	&(gprBank[0][2]), &(gprBank[0][3]),

      &(gpr[0]), &(gpr[1]), &(gpr[2]), &(gpr[3])
   },

   {
      /* MAP CODE L1 */

      //BANK 0
      &(gprBank[0][0]), &(gprBank[0][1]),	&(gprBank[0][2]), &(gprBank[0][3]),

      //BANK 1
      &(gprBank[1][0]), &(gprBank[1][1]),	&(gprBank[1][2]), &(gprBank[1][3]),

      //BANK 2
      &(gprBank[2][0]), &(gprBank[2][1]),	&(gprBank[2][2]), &(gprBank[2][3]),

      //BANK 3
      &(gprBank[3][0]), &(gprBank[3][1]),	&(gprBank[3][2]), &(gprBank[3][3]),

      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,

      //Previous Bank
      &(gprBank[0][0]), &(gprBank[0][1]),	&(gprBank[0][2]), &(gprBank[0][3]),

      //Current Bank
      &(gprBank[1][0]), &(gprBank[1][1]), &(gprBank[1][2]), &(gprBank[1][3]),

      &(gpr[0]), &(gpr[1]), &(gpr[2]), &(gpr[3])
   },

   {
      /* MAP CODE L2 */

      //BANK 0
      &(gprBank[0][0]), &(gprBank[0][1]),	&(gprBank[0][2]), &(gprBank[0][3]),

      //BANK 1
      &(gprBank[1][0]), &(gprBank[1][1]),	&(gprBank[1][2]), &(gprBank[1][3]),

      //BANK 2
      &(gprBank[2][0]), &(gprBank[2][1]),	&(gprBank[2][2]), &(gprBank[2][3]),

      //BANK 3
      &(gprBank[3][0]), &(gprBank[3][1]),	&(gprBank[3][2]), &(gprBank[3][3]),

      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,

      //Previous Bank
      &(gprBank[1][0]), &(gprBank[1][1]),	&(gprBank[1][2]), &(gprBank[1][3]),

      //Current Bank
      &(gprBank[2][0]), &(gprBank[2][1]), &(gprBank[2][2]), &(gprBank[2][3]),

      &(gpr[0]), &(gpr[1]), &(gpr[2]), &(gpr[3])
   },

   {
      /* MAP CODE L3 */

      //BANK 0
      &(gprBank[0][0]), &(gprBank[0][1]),	&(gprBank[0][2]), &(gprBank[0][3]),

      //BANK 1
      &(gprBank[1][0]), &(gprBank[1][1]),	&(gprBank[1][2]), &(gprBank[1][3]),

      //BANK 2
      &(gprBank[2][0]), &(gprBank[2][1]),	&(gprBank[2][2]), &(gprBank[2][3]),

      //BANK 3
      &(gprBank[3][0]), &(gprBank[3][1]),	&(gprBank[3][2]), &(gprBank[3][3]),

      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,&rErr,
      &rErr,&rErr,&rErr,&rErr,

      //Previous Bank
      &(gprBank[2][0]), &(gprBank[2][1]), &(gprBank[2][2]), &(gprBank[2][3]),

      //Current Bank
      &(gprBank[3][0]), &(gprBank[3][1]), &(gprBank[3][2]), &(gprBank[3][3]),

      &(gpr[0]), &(gpr[1]), &(gpr[2]), &(gpr[3])
   }
};

//=============================================================================

uint8 statusIFF(void)	
{
	uint8 iff = (sr & 0x7000) >> 12;

	if (iff == 1)
		return 0;
   return iff;
}

void setStatusIFF(uint8 iff)
{
	sr = (sr & 0x8FFF) | ((iff & 0x7) << 12);
}

//=============================================================================

uint8 statusRFP;

void setStatusRFP(uint8 rfp)
{
	sr = (sr & 0xF8FF) | ((rfp & 0x3) << 8);
	changedSP();
}

void changedSP(void)
{
	//Store global RFP for optimisation. 
	statusRFP = ((sr & 0x300) >> 8);
	int_check_pending();
}

//=============================================================================
#ifndef le32toh
#ifdef MSB_FIRST
#define le32toh(l)      ((((l)>>24) & 0xff) | (((l)>>8) & 0xff00) \
  | (((l)<<8) & 0xff0000) | (((l)<<24) & 0xff000000))
#else
#define le32toh(x) (x)
#endif
#endif

void reset_registers(void)
{
	memset(gprBank, 0, sizeof(gprBank));
	memset(gpr, 0, sizeof(gpr));

	if (ngpc_rom.data)
		pc = le32toh(rom_header->startPC) & 0xFFFFFF;
	else
		pc = 0xFFFFFE;

	sr = 0xF800;		// = %11111000???????? (?) are undefined in the manual)
	changedSP();
	
	f_dash = 00;

	rErr = RERR_VALUE;

	gpr[0] = 0xff23c3;
	gpr[1] = 0xff23df;
	gpr[2] = 0x006480;
	REGXSP = 0x00006C00; //Confirmed from BIOS, 
						//immediately changes value from default of 0x100
}

//=============================================================================
