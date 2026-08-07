/* z80.c: z80 supplementary functions
   Copyright (c) 1999-2003 Philip Kendall

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include "z80.h"
#include "z80_macros.h"

/* Set up the z80 emulation */
void z80_init( void )
{
  int i;

  /* Initialise the tables used to set flags */
  for(i = 0; i < 0x100; i++)
  {
     int j        = i;
     int k        = 0;
     uint8_t parity = 0;

     sz53_table[i]= i & ( Z80_FLAG_3 | Z80_FLAG_5 | Z80_FLAG_S );

     for(; k < 8; k++)
     {
        parity ^= j & 1;
        j >>=1;
     }

     parity_table[i] = ( parity ? 0 : Z80_FLAG_P );
     sz53p_table[i]  = sz53_table[i] | parity_table[i];
  }

  sz53_table[0]  |= Z80_FLAG_Z;
  sz53p_table[0] |= Z80_FLAG_Z;
}

/* Reset the z80 */
void z80_reset( void )
{
  AF =BC =DE =HL =0;
  AF_=BC_=DE_=HL_=0;
  IX=IY=0;
  I=R=R7=0;
  SP=PC=0;
  IFF1=IFF2=IM=0;
  z80.halted=0;

  z80.interrupts_enabled_at = 0;
  z80_tstates = last_z80_tstates = 0;
}

/* Process a z80 maskable interrupt */
int z80_interrupt( void )
{
   if (!IFF1)
      return 0;			/* Did not accept an interrupt */

   /* If interrupts have just been enabled, don't accept the interrupt now,
      but check after the next instruction has been executed */
   if(z80_tstates == z80.interrupts_enabled_at)
      return 0;

   if(z80.halted)
   {
      PC++;
      z80.halted = 0;
   }

   IFF1 = IFF2 = 0;

   Z80_WB_MACRO( --SP, PCH );
   Z80_WB_MACRO( --SP, PCL );

   R++;

   switch(IM)
   {
      case 0:
         PC = 0x0038;
         z80_tstates += 7;
         break;
      case 1:
         PC = 0x0038;
         z80_tstates += 7;
         break;
      case 2: 
         {
            uint16_t inttemp=(0x100*I)+0xff;
            PCL = Z80_RB_MACRO(inttemp++); PCH = Z80_RB_MACRO(inttemp);
            z80_tstates += 7;
            break;
         }
   }

   return 1;			/* Accepted an interrupt */
}

/* Process a z80 non-maskable interrupt */
void z80_nmi(void)
{
  if(z80.halted)
  {
     PC++;
     z80.halted = 0;
  }

  IFF1 = 0;

  Z80_WB_MACRO( --SP, PCH );
  Z80_WB_MACRO( --SP, PCL );

  /* FIXME: how is R affected? */

  /* FIXME: how does contention apply here? */
  z80_tstates += 11;
  PC = 0x0066;
}
