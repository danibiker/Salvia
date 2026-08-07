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

#include <string.h>

#include "neopop.h"
#include "dma.h"
#include "mem.h"
#include "interrupt.h"
#include "../state.h"

static uint32_t dmaS[4], dmaD[4];
static uint16_t dmaC[4];
static uint8_t dmaM[4];

void reset_dma(void)
{
	memset(dmaS, 0, sizeof(dmaS));
	memset(dmaD, 0, sizeof(dmaD));
	memset(dmaC, 0, sizeof(dmaC));
	memset(dmaM, 0, sizeof(dmaM));
}

void DMA_update(int channel)
{
	uint8_t mode = (dmaM[channel] & 0x1C) >> 2;
	uint8_t size = (dmaM[channel] & 0x03);			/* byte, word or long */

	/* Correct? */
	if (dmaC[channel] == 0)
		return;

	switch (mode)
   {
      case 0:	/* Destination INC mode, I/O to Memory transfer */
         switch(size)
         {
            case 0:
               storeB(dmaD[channel], loadB(dmaS[channel]));
               dmaD[channel] += 1; /* Byte increment */
               break;
            case 1:
               storeW(dmaD[channel], loadW(dmaS[channel]));
               dmaD[channel] += 2; /* Word increment */
               break;
            case 2:
               storeL(dmaD[channel], loadL(dmaS[channel]));
               dmaD[channel] += 4; /* Long increment */
               break;
         }
         break;

      case 1:	/* Destination DEC mode, I/O to Memory transfer */
         switch(size)
         {
            case 0:
               storeB(dmaD[channel], loadB(dmaS[channel]));
               dmaD[channel] -= 1; //Byte decrement
               break;
            case 1:
               storeW(dmaD[channel], loadW(dmaS[channel]));
               dmaD[channel] -= 2; //Word decrement
               break;
            case 2:
               storeL(dmaD[channel], loadL(dmaS[channel]));
               dmaD[channel] -= 4; //Long decrement
               break;
         }
         break;

      case 2:	/* Source INC mode, Memory to I/O transfer */
         switch(size)
         {
            case 0:
               storeB(dmaD[channel], loadB(dmaS[channel]));
               dmaS[channel] += 1; //Byte increment
               break;
            case 1:
               storeW(dmaD[channel], loadW(dmaS[channel]));
               dmaS[channel] += 2; //Word increment
               break;
            case 2:
               storeL(dmaD[channel], loadL(dmaS[channel]));
               dmaS[channel] += 4; //Long increment
               break;
         }
         break;

      case 3:	/* Source DEC mode, Memory to I/O transfer */
         switch(size)
         {
            case 0:
               storeB(dmaD[channel], loadB(dmaS[channel]));
               dmaS[channel] -= 1; //Byte decrement
               break;
            case 1:
               storeW(dmaD[channel], loadW(dmaS[channel]));
               dmaS[channel] -= 2; //Word decrement
               break;
            case 2:
               storeL(dmaD[channel], loadL(dmaS[channel]));
               dmaS[channel] -= 4; //Long decrement
               break;
         }
         break;

      case 4:	/* Fixed Address Mode */
         switch(size)
         {
            case 0:
               storeB(dmaD[channel], loadB(dmaS[channel]));
               break;
            case 1:
               storeW(dmaD[channel], loadW(dmaS[channel]));
               break;
            case 2:
               storeL(dmaD[channel], loadL(dmaS[channel]));
               break;
         }
         break;

      case 5: /* Counter Mode */
         dmaS[channel] ++;
         break;
   }

	/* Perform common counter decrement,
    * vector clearing, and interrupt handling.
    */

	dmaC[channel] --;
	if (dmaC[channel] == 0)
	{
		interrupt(14 + channel, 7);
		storeB(0x7C + channel, 0);
	}
}

void dmaStoreB(uint8_t cr, uint8_t data)
{
	switch(cr)
   {
      case 0x22:
         dmaM[0] = data;
         break;
      case 0x26:
         dmaM[1] = data;
         break;
      case 0x2A:
         dmaM[2] = data;
         break;
      case 0x2E:
         dmaM[3] = data;
         break;
   }
}

void dmaStoreW(uint8_t cr, uint16_t data)
{
   switch(cr)
   {
      case 0x20:
         dmaC[0] = data;
         break;
      case 0x24:
         dmaC[1] = data;
         break;
      case 0x28:
         dmaC[2] = data;
         break;
      case 0x2C:
         dmaC[3] = data;
         break;
   }
}

void dmaStoreL(uint8_t cr, uint32_t data)
{
   switch(cr)
   {
      case 0x00:
         dmaS[0] = data;
         break;	
      case 0x04:
         dmaS[1] = data;
         break;	
      case 0x08:
         dmaS[2] = data;
         break;	
      case 0x0C:
         dmaS[3] = data;
         break;	
      case 0x10:
         dmaD[0] = data;
         break;	
      case 0x14:
         dmaD[1] = data;
         break;	
      case 0x18:
         dmaD[2] = data;
         break;	
      case 0x1C:
         dmaD[3] = data;
         break;
   }
}

uint8_t dmaLoadB(uint8_t cr)
{

   switch(cr)
   {
      case 0x22:
         return dmaM[0];
      case 0x26:
         return dmaM[1];
      case 0x2A:
         return dmaM[2];
      case 0x2E:
         return dmaM[3];
   }

   return 0;
}

uint16_t dmaLoadW(uint8_t cr)
{
   switch(cr)
   {
      case 0x20:
         return dmaC[0];
      case 0x24:
         return dmaC[1];
      case 0x28:
         return dmaC[2];
      case 0x2C:
         return dmaC[3];
   }

   return 0;
}

uint32_t dmaLoadL(uint8_t cr)
{
   switch(cr)
   {
      case 0x00:
         return dmaS[0];
      case 0x04:
         return dmaS[1];
      case 0x08:
         return dmaS[2];
      case 0x0C:
         return dmaS[3];
      case 0x10:
         return dmaD[0];
      case 0x14:
         return dmaD[1];
      case 0x18:
         return dmaD[2];
      case 0x1C:
         return dmaD[3];
   }

   return 0;
}

int MDFNNGPCDMA_StateAction(void *data, int load, int data_only)
{
   SFORMAT StateRegs[] =
   {
      { dmaS, (uint32_t)((4) * sizeof(uint32_t)), 0x40000000, "DMAS" },
      { dmaD, (uint32_t)((4) * sizeof(uint32_t)), 0x40000000, "DMAD" },
      { dmaC, (uint32_t)((4) * sizeof(uint16_t)), 0x20000000, "DMAC" },
      { dmaM, (uint32_t)(4), 0, "DMAM" },
      { 0, 0, 0, 0 }
   };

   if(!MDFNSS_StateAction(data, load, data_only, StateRegs, "DMA", false))
      return 0;

   return 1; 
}
