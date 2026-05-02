/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#include "psxcommon.h"
#include "r3000a.h"
#include "psxbios.h"

#include "cheat.h"
#include "ppf.h"

PcsxConfig Config;
boolean NetOpened = FALSE;

/* Set by EmuUpdate at VBlank; tested by the CPU execute loops to return
 * control to retro_run for one-frame-per-call libretro semantics. */
volatile int frame_done = 0;

int Log = 0;
FILE *emuLog = NULL;

int EmuInit() {
	return psxInit();
}

void EmuReset() {
	FreeCheatSearchResults();
	FreeCheatSearchMem();

	psxReset();
}

void EmuShutdown() {
	ClearAllCheats();
	FreeCheatSearchResults();
	FreeCheatSearchMem();

	FreePPFCache();

	psxShutdown();
}

void EmuUpdate() {
	/* Signal end-of-frame to the CPU execute loop (libretro single-thread
	 * model).  Skip during HLE BIOS softcalls — historically SysUpdate
	 * was guarded against re-entrant input polling there; we keep the
	 * same gate so the frame yield only happens at "safe" VBlanks. */
	if (!Config.HLE || !hleSoftCall)
		frame_done = 1;

	ApplyCheats();
}

void __Log(char *fmt, ...) {
	va_list list;
#ifdef LOG_STDOUT
	char tmp[1024];
#endif

	va_start(list, fmt);
#ifndef LOG_STDOUT
	vfprintf(emuLog, fmt, list);
#else
	vsprintf(tmp, fmt, list);
	SysPrintf(tmp);
#endif
	va_end(list);
}
