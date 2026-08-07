/* z80_ops.c: Process the next opcode
   Copyright (c) 1999-2005 Philip Kendall, Witold Filipczyk

   $Id: z80_ops.c 4624 2012-01-09 20:59:35Z pak21 $

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

#include <boolean.h>

#include "z80.h"
#include "z80_macros.h"
#include "z80_fns.h"

int32_t ngpc_soundTS = 0;

int iline = 0;

void z80_set_interrupt(int set)
{
   iline = set;
}

int z80_do_opcode( void )
{
   int ret;
   uint8_t opcode;

   if(iline)
   {
      if(z80_interrupt())
      {
         int ret = z80_tstates - last_z80_tstates;
         last_z80_tstates = z80_tstates;
         return ret;
      }
   }

   opcode = Z80_RB_MACRO( PC ); 
   z80_tstates++;

   PC++; 
   R++;

   switch(opcode) 
   {
     #include "opcodes_base.c"
   }

   ret              = z80_tstates - last_z80_tstates;
   last_z80_tstates = z80_tstates;

   return ret;
}
