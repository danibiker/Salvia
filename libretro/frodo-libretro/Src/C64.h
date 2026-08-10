/*
 *  C64.h - Put the pieces together
 *
 *  Frodo Copyright (C) Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _C64_H
#define _C64_H

#include <stdio.h>

#include <streams/file_stream.h>

/* TODO/FIXME - most if not all the file I/O should eventually be removed */

/* Sizes of memory areas */
const int C64_RAM_SIZE    = 0x10000;
const int COLOR_RAM_SIZE  = 0x400;
const int BASIC_ROM_SIZE  = 0x2000;
const int KERNAL_ROM_SIZE = 0x2000;
const int CHAR_ROM_SIZE   = 0x1000;
const int DRIVE_RAM_SIZE  = 0x800;
const int DRIVE_ROM_SIZE  = 0x4000;


// false: Frodo, true: FrodoSC
extern bool IsFrodoSC;

class Prefs;
class C64Display;
class MOS6510;
class MOS6569;
class MOS6581;
class MOS6526_1;
class MOS6526_2;
class IEC;
class REU;
class MOS6502_1541;
class Job1541;

class C64 {
public:
	C64();
	~C64();

	void Run(void);
	void Reset(void);
	void NMI(void);
	void VBlank(bool draw_frame);
	void NewPrefs(Prefs *prefs);
	void PatchKernal(bool fast_reset, bool emul_1541_proc);
	void SaveRAM(char *filename);
	void SaveSnapshot(char *filename);
	bool LoadSnapshot(char *filename);
	int SaveCPUState(RFILE *f);
	int Save1541State(RFILE *f);
	bool Save1541JobState(RFILE *f);
	bool SaveVICState(RFILE *f);
	bool SaveSIDState(RFILE *f);
	bool SaveCIAState(RFILE *f);
	bool LoadCPUState(RFILE *f);
	bool Load1541State(RFILE *f);
	bool Load1541JobState(RFILE *f);
	bool LoadVICState(RFILE *f);
	bool LoadSIDState(RFILE *f);
	bool LoadCIAState(RFILE *f);

	uint8 *RAM, *Basic, *Kernal,
		  *Char, *Color;		// C64
	uint8 *RAM1541, *ROM1541;	// 1541

	C64Display *TheDisplay;

	MOS6510 *TheCPU;			// C64
	MOS6569 *TheVIC;
	MOS6581 *TheSID;
	MOS6526_1 *TheCIA1;
	MOS6526_2 *TheCIA2;
	IEC *TheIEC;
	REU *TheREU;

	MOS6502_1541 *TheCPU1541;	// 1541
	Job1541 *TheJob1541;

	uint32 CycleCounter;  // Cycle counter for Frodo SC
#ifdef NO_LIBCO
	void thread_func(void);
#endif

private:
	void c64_ctor1(void);
	void c64_ctor2(void);
	void c64_dtor(void);
	uint8 poll_joystick(int port);
#ifndef NO_LIBCO
	void thread_func(void);
#endif
	bool quit_thyself;		// Emulation thread shall quit

	uint8 joykey;			// Joystick keyboard emulation mask value

	uint8 orig_kernal_1d84;	// Original contents of kernal locations $1d84 and $1d85
	uint8 orig_kernal_1d85;	// (for undoing the Fast Reset patch)
};


#endif
