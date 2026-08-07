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

#include "neopop.h"
#include "mem.h"
#include "sound.h"
#include "Z80_interface.h"
#include "TLCS-900h/TLCS900h_registers.h"
#include "interrupt.h"
#include "dma.h"
#include "../hw_cpu/z80-fuse/z80.h"
#include "../hw_cpu/z80-fuse/z80_macros.h"

#include "../state.h"

static uint8_t CommByte;
static bool Z80Enabled;

int z80_state_action(void *data, int load, int data_only, const char *section_name)
{
   uint8_t r_register;

   SFORMAT StateRegs[] =
   {
      { &(z80.af.w), sizeof(z80.af.w), 0x80000000, "AF" },
      { &(z80.bc.w), sizeof(z80.bc.w), 0x80000000, "BC" },
      { &(z80.de.w), sizeof(z80.de.w), 0x80000000, "DE" },
      { &(z80.hl.w), sizeof(z80.hl.w), 0x80000000, "HL" },
      { &(z80.af_.w), sizeof(z80.af_.w), 0x80000000, "AF_" },
      { &(z80.bc_.w), sizeof(z80.bc_.w), 0x80000000, "BC_" },
      { &(z80.de_.w), sizeof(z80.de_.w), 0x80000000, "DE_" },
      { &(z80.hl_.w), sizeof(z80.hl_.w), 0x80000000, "HL_" },
      { &(z80.ix.w), sizeof(z80.ix.w), 0x80000000, "IX" },
      { &(z80.iy.w), sizeof(z80.iy.w), 0x80000000, "IY" },
      { &(z80.i), sizeof(z80.i), 0x80000000, "I" },
      { &(z80.sp.w), sizeof(z80.sp.w), 0x80000000, "SP" },
      { &(z80.pc.w), sizeof(z80.pc.w), 0x80000000, "PC" },
      { &(z80.iff1), sizeof(z80.iff1), 0x80000000, "IFF1" },
      { &(z80.iff2), sizeof(z80.iff2), 0x80000000, "IFF2" },
      { &(z80.im), sizeof(z80.im), 0x80000000, "IM" },
      { &(r_register), sizeof(r_register), 0x80000000, "R" },

      { &(z80.interrupts_enabled_at), sizeof(z80.interrupts_enabled_at), 0x80000000, "interrupts_enabled_at" },
      { &(z80.halted), sizeof(z80.halted), 0x80000000, "halted" },

      { &((z80_tstates)), sizeof((z80_tstates)), 0x80000000, "z80_tstates" },
      { &((last_z80_tstates)), sizeof((last_z80_tstates)), 0x80000000, "last_z80_tstates" },

      { 0, 0, 0, 0 }
   };

   if(!load)
      r_register = (z80.r7 & 0x80) | (z80.r & 0x7f);

   if(!MDFNSS_StateAction(data, load, data_only, StateRegs, section_name, false))
      return 0;

   if(load)
   {
      z80.r7 = r_register & 0x80;
      z80.r = r_register & 0x7F;
   }

   return 1;
}

uint8_t Z80_ReadComm(void)
{
   return CommByte;
}

void Z80_WriteComm(uint8_t data)
{
   CommByte = data;
}

static uint8_t NGP_z80_readbyte(uint16_t address)
{
   if (address <= 0xFFF)
      return loadB(0x7000 + address);
   else if (address == 0x8000)
         return CommByte;
   return 0;
}

static void NGP_z80_writebyte(uint16_t address, uint8_t value)
{
   if (address <= 0x0FFF)
   {
      storeB(0x7000 + address, value);
      return;
   }

   switch (address)
   {
      case 0x8000:
         CommByte = value;
         break;
      case 0x4001:
         Write_SoundChipLeft(value);
         break;
      case 0x4000:
         Write_SoundChipRight(value);
         break;
      case 0xC000:
         TestIntHDMA(6, 0x0C);
         break;
   }

}

static void NGP_z80_writeport(uint16_t port, uint8_t value)
{
	z80_set_interrupt(0);
}

static uint8_t NGP_z80_readport(uint16_t port)
{
	return 0;
}

void Z80_nmi(void)
{
	z80_nmi();
}

void Z80_irq(void)
{
	z80_set_interrupt(1);
}

void Z80_reset(void)
{
	Z80Enabled    = 0;

	z80_writebyte = NGP_z80_writebyte;
	z80_readbyte  = NGP_z80_readbyte;
	z80_writeport = NGP_z80_writeport;
	z80_readport  = NGP_z80_readport;

	z80_init();
	z80_reset();
}

void Z80_SetEnable(bool set)
{
   Z80Enabled = set;
   if(!set)
      z80_reset();
}

bool Z80_IsEnabled(void)
{
   return(Z80Enabled);
}

int Z80_RunOP(void)
{
   if(!Z80Enabled)
      return -1;

   return(z80_do_opcode());
}

void MDFNNGPCZ80_StateAction(void *data, int load, int data_only)
{
   SFORMAT StateRegs[] =
   {
      { &(CommByte), (uint32_t)sizeof(CommByte), MDFNSTATE_RLSB, "CommByte" },
      { &(Z80Enabled), 1, MDFNSTATE_RLSB | MDFNSTATE_BOOL, "Z80Enabled" },
      { 0, 0, 0, 0 }
   };

   MDFNSS_StateAction(data, load, data_only, StateRegs, "Z80X", false);

   z80_state_action(data, load, data_only, "Z80");
}
