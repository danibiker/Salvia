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
/*
* PSX memory functions.
*/

#if 0

#include "psxmem.h"
#include "r3000a.h"
#include "psxhw.h"

s8 *psxVM = NULL;// PSX Virtual Memory (no mirrors use mask)

s8 *psxM = NULL; // Kernel & User Memory (2 Meg)
s8 *psxP = NULL; // Parallel Port (64K)
s8 *psxR = NULL; // BIOS ROM (512K)
s8 *psxH = NULL; // Scratch Pad (1K) & Hardware Registers (8K)


/*  Playstation Memory Map (from Playstation doc by Joshua Walker)
0x0000_0000-0x0000_ffff		Kernel (64K)
0x0001_0000-0x001f_ffff		User Memory (1.9 Meg)

0x1f00_0000-0x1f00_ffff		Parallel Port (64K)

0x1f80_0000-0x1f80_03ff		Scratch Pad (1024 bytes)

0x1f80_1000-0x1f80_2fff		Hardware Registers (8K)

0x1fc0_0000-0x1fc7_ffff		BIOS (512K)

0x8000_0000-0x801f_ffff		Kernel and User Memory Mirror (2 Meg) Cached
0x9fc0_0000-0x9fc7_ffff		BIOS Mirror (512K) Cached

0xa000_0000-0xa01f_ffff		Kernel and User Memory Mirror (2 Meg) Uncached
0xbfc0_0000-0xbfc7_ffff		BIOS Mirror (512K) Uncached
*/

// also used in dr
int writeok = 1;

int psxMemInit() {

static int initialised = 0;

    writeok = 1;

if(initialised){
   return 0;
}

   // Create 0x20000000 (512Mb) virtual memory
	psxVM = (s8 *)VirtualAlloc(NULL, VM_SIZE, MEM_RESERVE, PAGE_READWRITE);

    // Create Mapping ...
	// 0x00200000 * 4 Mirrors
	psxM = (s8 *)VirtualAlloc(psxVM, 0x00800000, MEM_COMMIT, PAGE_READWRITE);
	// Parallel Port
	psxP = (s8 *)VirtualAlloc(psxVM+0x1f000000, 0x00010000, MEM_COMMIT, PAGE_READWRITE);
	// Hardware Regs + Scratch Pad
	psxH = (s8 *)VirtualAlloc(psxVM+0x1f800000, 0x00010000, MEM_COMMIT, PAGE_READWRITE);
	// Bios
	psxR = (s8 *)VirtualAlloc(psxVM+0x1fc00000, 0x00400000, MEM_COMMIT, PAGE_READWRITE);

    initialised++;

    return 0;
}

void psxMemReset() {
	int i = 0;
	FILE *f = NULL;
	char bios[1024];	

	writeok = 1;

	memset(psxM, 0, 0x00200000);
	memset(psxP, 0, 0x00010000);
//	memset(psxH, 0, 0x00010000);

	// Load BIOS
	if (strcmp(Config.Bios, "HLE") != 0) {
		sprintf(bios, "%s\\%s", Config.BiosDir, Config.Bios);
		f = fopen(bios, "rb");

		if (f == NULL) {
			SysMessage(_("Could not open BIOS:\"%s\". Enabling HLE Bios!\n"), bios);
			memset(psxR, 0, 0x80000);
			Config.HLE = TRUE;
		} else {
			fread(psxR, 1, 0x80000, f);
			// Mirror bios !
			for(i = i; i < 8; i++) {
				memcpy(psxR + i * 0x80000, psxR, 0x80000);
			}
			fclose(f);
			Config.HLE = FALSE;
		}
	} else Config.HLE = TRUE;
}

void psxMemShutdown() {

}

u8 psxMemRead8(u32 mem) {
	psxRegs.cycle += 0;

	mem = mem & VM_MASK;

	if (mem >= 0x1f801000 && mem <= 0x1f803000) {
		return psxHwRead8(mem);
	} else {
		return psxVMu8(mem);
	}
}

u16 psxMemRead16(u32 mem) {
//	psxRegs.cycle += 1;
			
	mem = mem & VM_MASK;

	if (mem >= 0x1f801000 && mem <= 0x1f803000) {
		return psxHwRead16(mem);
	} else {
		return psxVMu16(mem);
	}
}

u32 psxMemRead32(u32 mem) {
//	psxRegs.cycle += 1;
		
	mem = mem & VM_MASK;

	if (mem >= 0x1f801000 && mem <= 0x1f803000) {
		return psxHwRead32(mem);
	} else {
		return psxVMu32(mem);
	}
}

void psxMemWrite8(u32 _mem, u8 value) {	
	u32 t;
	u32 mem;
//	psxRegs.cycle += 1;

	mem = _mem & VM_MASK;

	t = mem >> 16;

	if (t == 0x1f80) {
		if ((mem & 0xffff) < 0x400)
			psxVM[mem & VM_MASK] = value;
		else
			psxHwWrite8(mem, value);
	} else {
		if (writeok && mem < PSX_MEM_MIRROR ) {
			psxVM[mem & VM_PSX_MEM_MASK | 0x00000000] = value;
			psxVM[mem & VM_PSX_MEM_MASK | 0x00200000] = value;
			psxVM[mem & VM_PSX_MEM_MASK | 0x00400000] = value;
			psxVM[mem & VM_PSX_MEM_MASK | 0x00600000] = value;
		}
		else if (mem > PSX_MEM_MIRROR) {
			psxVM[mem] = value;
		}
			
		psxCpu->Clear((_mem & (~3)), 1);
	}
}

void psxMemWrite16(u32 _mem, u16 value) {
	u32 t;	
	u32 mem;
//	psxRegs.cycle += 1;

	mem = _mem & VM_MASK;
	
	t = mem >> 16;

	if (t == 0x1f80) {
		if ((mem & 0xffff) < 0x400)
			psxVMu16ref(mem) = SWAPu16(value);
		else
			psxHwWrite16(mem, value);
	} else {
		if ( writeok && mem < PSX_MEM_MIRROR ) {
			psxVMu16ref(mem & VM_PSX_MEM_MASK | 0x00000000) = SWAPu16(value);
			psxVMu16ref(mem & VM_PSX_MEM_MASK | 0x00200000) = SWAPu16(value);
			psxVMu16ref(mem & VM_PSX_MEM_MASK | 0x00400000) = SWAPu16(value);
			psxVMu16ref(mem & VM_PSX_MEM_MASK | 0x00600000) = SWAPu16(value);
		}
		else if (mem > PSX_MEM_MIRROR) {
			(*(u16 *)&psxVM[mem]) = SWAPu16(value);
		}
			
		psxCpu->Clear((_mem & (~3)), 1);
	}
}

void psxMemWrite32(u32 mem, u32 value) {
	u32 t;
//	psxRegs.cycle += 1;

	t = mem >> 16;
	
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			psxVMu32ref(mem) = SWAPu32(value);
		else
			psxHwWrite32(mem, value);
	} else {
		if (mem != 0xfffe0130) {
			mem = mem & VM_MASK;
			if ( writeok && mem < PSX_MEM_MIRROR ) {
					psxVMu32ref(mem & VM_PSX_MEM_MASK | 0x00000000) = SWAPu32(value);
					psxVMu32ref(mem & VM_PSX_MEM_MASK | 0x00200000) = SWAPu32(value);
					psxVMu32ref(mem & VM_PSX_MEM_MASK | 0x00400000) = SWAPu32(value);
					psxVMu32ref(mem & VM_PSX_MEM_MASK | 0x00600000) = SWAPu32(value);
			} else if (mem > PSX_MEM_MIRROR) {
				(*(u32 *)&psxVM[mem]) = SWAPu32(value);
			}
			psxCpu->Clear(mem, 1);
		} else {
			// a0-44: used for cache flushing
			switch (value) {
				case 0x800: case 0x804:
					if (writeok == 0) break;
					writeok = 0;
					break;
				case 0x00: case 0x1e988:
					if (writeok == 1) break;
					writeok = 1;
					break;
				default:
					break;
			}
		}
	}
}

// is it used ?
void * psxMemPointer(u32 mem) {
	return (void *)&psxVM[mem];
}

#endif





