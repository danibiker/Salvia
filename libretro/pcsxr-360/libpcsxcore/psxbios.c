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
 * Internal simulated HLE BIOS.
 */

// TODO: implement all system calls, count the exact CPU cycles of system calls.

#include "psxbios.h"
#include "psxhw.h"

#define a0 (psxRegs.GPR.n.a0)
#define a1 (psxRegs.GPR.n.a1)
#define a2 (psxRegs.GPR.n.a2)
#define a3 (psxRegs.GPR.n.a3)
#define sp (psxRegs.GPR.n.sp)
#define ra (psxRegs.GPR.n.ra)

#define pc0 (psxRegs.pc)

#define Ra0 ((char *)PSXM_2(a0))

char *biosA0n[256];
char *biosB0n[256];
char *biosC0n[256];

typedef struct {
	char name[32];
	u32  mode;
	u32  offset;
	u32  size;
	u32  mcfile;
} FileDesc;

typedef struct {
	s32 status;
	s32 mode;
	u32 reg[32];
	u32 func;
} TCB;

static int CardState = -1;
static TCB Thread[8];
static int CurThread = 0;
static FileDesc FDesc[32];
static u32 SysIntRP[8];
static u32 regs[35];
static u32 *heap_addr = NULL;
static u32 *jmp_int = NULL;
static int *pad_buf = NULL;
static char *pad_buf1 = NULL, *pad_buf2 = NULL;
static int pad_buf1len, pad_buf2len;

boolean hleSoftCall = FALSE;

void psxBios_printf() { // 0x3f
	char tmp[1024];
	char tmp2[1024];
	u32 save[4];
	char *ptmp = tmp;
	int n=1, i=0, j;

	memcpy(save, (char*)PSXM_2(sp), 4 * 4);

	psxMu32ref_2(sp) = SWAP32((u32)a0);
	psxMu32ref_2(sp + 4) = SWAP32((u32)a1);
	psxMu32ref_2(sp + 8) = SWAP32((u32)a2);
	psxMu32ref_2(sp + 12) = SWAP32((u32)a3);

	while (Ra0[i]) {
		switch (Ra0[i]) {
			case '%':
				j = 0;
				tmp2[j++] = '%';
_start:
				switch (Ra0[++i]) {
					case '.':
					case 'l':
						tmp2[j++] = Ra0[i]; goto _start;
					default:
						if (Ra0[i] >= '0' && Ra0[i] <= '9') {
							tmp2[j++] = Ra0[i];
							goto _start;
						}
						break;
				}
				tmp2[j++] = Ra0[i];
				tmp2[j] = 0;

				switch (Ra0[i]) {
					case 'f': case 'F':
						ptmp += sprintf(ptmp, tmp2, (float)psxMu32_2(sp + n * 4)); n++; break;
					case 'a': case 'A':
					case 'e': case 'E':
					case 'g': case 'G':
						ptmp += sprintf(ptmp, tmp2, (double)psxMu32_2(sp + n * 4)); n++; break;
					case 'p':
					case 'i':
					case 'd': case 'D':
					case 'o': case 'O':
					case 'x': case 'X':
						ptmp += sprintf(ptmp, tmp2, (unsigned int)psxMu32_2(sp + n * 4)); n++; break;
					case 'c':
						ptmp += sprintf(ptmp, tmp2, (unsigned char)psxMu32_2(sp + n * 4)); n++; break;
					case 's':
						ptmp += sprintf(ptmp, tmp2, (char*)PSXM_2(psxMu32_2(sp + n * 4))); n++; break;
                    case '%':
						*ptmp++ = Ra0[i]; break;
				}
				i++;
				break;
			default:
				*ptmp++ = Ra0[i++];
		}
	}
	*ptmp = 0;

	memcpy((char*)PSXM_2(sp), save, 4 * 4);

	SysPrintf(tmp);

	pc0 = ra;
}

void psxBios_putchar() { // 3d
	SysPrintf("%c", (char)a0);
	pc0 = ra;
}

void psxBios_puts() { // 3e/3f
	SysPrintf(Ra0);
	pc0 = ra;
}

void (*biosA0[256])();
void (*biosB0[256])();
void (*biosC0[256])();

void psxBiosInit_2() {
	 
	int i;

	for(i = 0; i < 256; i++) {
		biosA0[i] = NULL;
		biosB0[i] = NULL;
		biosC0[i] = NULL;
	}
	biosA0[0x3e] = psxBios_puts;
	biosA0[0x3f] = psxBios_printf;

	biosB0[0x3d] = psxBios_putchar;
	biosB0[0x3f] = psxBios_puts;

	if (!Config.HLE) return;

}

void psxBiosException() {

}

void psxBiosShutdown() {
}


#define bfreeze(ptr, size) { \
	if (Mode == 1) memcpy(&psxR_2[base], ptr, size); \
	if (Mode == 0) memcpy(ptr, &psxR_2[base], size); \
	base += size; \
}

#define bfreezes(ptr) bfreeze(ptr, sizeof(ptr))
#define bfreezel(ptr) bfreeze(ptr, sizeof(*ptr))

#define bfreezepsxMptr(ptr, type) { \
	if (Mode == 1) { \
		if (ptr) psxRu32ref_2(base) = SWAPu32((s8 *)(ptr) - psxM_2); \
		else psxRu32ref_2(base) = 0; \
	} else { \
		if (psxRu32_2(base) != 0) ptr = (type *)(psxM_2 + psxRu32_2(base)); \
		else (ptr) = NULL; \
	} \
	base += sizeof(u32); \
}

void psxBiosFreeze(int Mode) {
	u32 base = 0x40000;

	bfreezepsxMptr(jmp_int, u32);
	bfreezepsxMptr(pad_buf, int);
	bfreezepsxMptr(pad_buf1, char);
	bfreezepsxMptr(pad_buf2, char);
	bfreezepsxMptr(heap_addr, u32);
	bfreezel(&pad_buf1len);
	bfreezel(&pad_buf2len);
	bfreezes(regs);
	bfreezes(SysIntRP);
	bfreezel(&CardState);
	bfreezes(Thread);
	bfreezel(&CurThread);
	bfreezes(FDesc);
}
