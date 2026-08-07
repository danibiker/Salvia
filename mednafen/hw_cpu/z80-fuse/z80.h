/* z80.h: z80 emulation core
   Copyright (c) 1999-2003 Philip Kendall

   $Id: z80.h 4640 2012-01-21 13:26:35Z pak21 $

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

#ifndef FUSE_Z80_H
#define FUSE_Z80_H

#include <stdint.h>
#include "z80_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void z80_init( void );
void z80_reset( void );

void z80_nmi( void );

void z80_set_interrupt(int set);

int z80_interrupt( void );

int z80_do_opcode(void);

extern struct processor z80;
extern const uint8_t overflow_add_table[];
extern const uint8_t overflow_sub_table[];
extern const uint8_t halfcarry_add_table[];
extern const uint8_t halfcarry_sub_table[];
extern uint64_t last_z80_tstates;
extern uint8_t sz53_table[0x100]; /* The S, Z, 5 and 3 bits of the index */
extern uint8_t parity_table[0x100]; /* The parity of the lookup value */
extern uint8_t sz53p_table[0x100]; /* OR the above two tables together */
extern uint64_t z80_tstates;
extern void (*z80_writebyte)(uint16_t a, uint8_t b);
extern uint8_t (*z80_readbyte)(uint16_t a);
extern void (*z80_writeport)(uint16_t a, uint8_t b);
extern uint8_t (*z80_readport)(uint16_t a);

void z80_enable_interrupts( void );

static INLINE uint16_t z80_getpc(void) { return z80.pc.w; }

// Ok, I lied, not a macro!

//Write mem
static INLINE void Z80_WB_MACRO(uint16_t A, uint8_t V)
{ 
 z80_tstates += 3; 
 z80_writebyte(A, V); 
}

// Write port
static INLINE void Z80_WP_MACRO(uint16_t A, uint8_t V)
{ 
 z80_tstates += 4; 
 z80_writeport(A, V); 
}

// Read mem
static INLINE uint8_t Z80_RB_MACRO(uint16_t A)
{ 
 z80_tstates += 3; 
 return(z80_readbyte(A));
}

// Read port
static INLINE uint8_t Z80_RP_MACRO(uint16_t A)
{ 
 z80_tstates += 4; 
 return(z80_readport(A));
}

#ifdef __cplusplus
}
#endif

#endif			/* #ifndef FUSE_Z80_H */
