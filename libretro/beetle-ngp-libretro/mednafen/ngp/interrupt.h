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

#ifndef __INTERRUPT__
#define __INTERRUPT__

#include <boolean.h>
#include <stdint.h>

#define TIMER_HINT_RATE		515		/* CPU Ticks between horizontal interrupts */

#define TIMER_BASE_RATE		32 		/* ticks */

#define TIMER_T1_RATE		(8    * TIMER_BASE_RATE)
#define TIMER_T4_RATE		(32   * TIMER_BASE_RATE)
#define TIMER_T16_RATE		(128  * TIMER_BASE_RATE)
#define TIMER_T256_RATE		(2048 * TIMER_BASE_RATE)

#ifdef __cplusplus
extern "C" {
#endif

/* Set this value to fix problems with glitching extra lines. */
extern bool gfx_hack;

void interrupt(uint8_t index, uint8_t level);
void set_interrupt(uint8_t index, bool set);

void reset_timers(void);
void reset_int(void);

/* Call this after each instruction */
bool updateTimers(void *data, int cputicks);

/* H-INT Timer */
extern uint32_t timer_hint;

void timer_write8(uint32_t address, uint8_t data);
uint8_t timer_read8(uint32_t address);

void int_write8(uint32_t address, uint8_t data);
uint8_t int_read8(uint32_t address);
void int_check_pending(void);
void TestIntHDMA(int bios_num, int vec_num);

int int_timer_StateAction(void *data, int load, int data_only);

#ifdef __cplusplus
}
#endif

#endif
