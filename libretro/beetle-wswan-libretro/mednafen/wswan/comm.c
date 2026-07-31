/* Mednafen - Multi-system Emulator
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* WonderSwan serial port emulation, ported from upstream comm.cpp.
 *
 * No external link is emulated (upstream's optional WonderFence
 * child-process bridge is intentionally left out; it is unix-only,
 * non-deterministic, and unusable on statically linked console
 * builds). Behavior matches upstream when no link peer is present:
 * transmitted bytes complete immediately and raise the serial-send
 * interrupt; nothing is ever received. */

#include <string.h>

#include "wswan.h"
#include "interrupt.h"
#include "comm.h"

#include "../state_inline.h"

static uint8 Control;
static uint8 SendBuf, RecvBuf;
static bool SendLatched, RecvLatched;

void Comm_Reset(void)
{
   SendBuf = 0x00;
   RecvBuf = 0x00;

   SendLatched = false;
   RecvLatched = false;

   Control = 0x00;

   WSwan_InterruptAssert(WSINT_SERIAL_RECV, RecvLatched);
}

/* Called once per scanline from wsExecuteLine(). */
void Comm_Process(void)
{
   if(SendLatched && (Control & 0x80))
   {
      /* No link peer; the byte leaves the shift register
       * immediately. */
      SendLatched = false;
      WSwan_Interrupt(WSINT_SERIAL_SEND);
   }
}

uint8 Comm_Read(uint8 A)
{
   if(A == 0xB1)
   {
      RecvLatched = false;
      WSwan_InterruptAssert(WSINT_SERIAL_RECV, RecvLatched);

      return(RecvBuf);
   }
   else if(A == 0xB3)
   {
      uint8 ret = Control & 0xF0;

      if((Control & 0x80) && !SendLatched)
         ret |= 0x4;

      if((Control & 0x20) && RecvLatched)
         ret |= 0x1;

      return(ret);
   }

   return(0x00);
}

void Comm_Write(uint8 A, uint8 V)
{
   if(A == 0xB1)
   {
      if(Control & 0x80)
      {
         SendBuf = V;
         SendLatched = true;
      }
   }
   else if(A == 0xB3)
      Control = V & 0xF0;
}

int Comm_StateAction(StateMem *sm, int load, int data_only)
{
   SFORMAT StateRegs[] =
   {
      SFVARN(SendBuf, "SendBuf"),
      SFVARN(RecvBuf, "RecvBuf"),

      SFVARN_BOOL(SendLatched, "SendLatched"),
      SFVARN_BOOL(RecvLatched, "RecvLatched"),

      SFVARN(Control, "Control"),
      { 0, 0, 0, 0 }
   };

   if(!MDFNSS_StateAction(sm, load, data_only, StateRegs, "COMM", false))
      return 0;

   if(load)
      WSwan_InterruptAssert(WSINT_SERIAL_RECV, RecvLatched);

   return 1;
}
