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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

/*
* PSX memory functions.
*/

#include "psxmem.h"
#include "r3000a.h"
#include "psxhw.h"

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

__declspec(align(32))u8* psxMemWLUT[0x10000];
__declspec(align(32))u8* psxMemRLUT[0x10000];
__declspec(align(32))s8 psxM_2[0x00220000];
__declspec(align(32))s8 psxR_2[0x00080000];

s8 *psxP_2 = NULL;
s8 *psxH_2 = NULL; 


int psxMemInit_2() {
	int i;

//	psxMemRLUT = (u8 **)malloc(0x10000 * sizeof(void *));
//	psxMemWLUT = (u8 **)malloc(0x10000 * sizeof(void *));
	memset(psxMemRLUT, 0, 0x10000 * sizeof(void *));
	memset(psxMemWLUT, 0, 0x10000 * sizeof(void *));

//	psxM_2 = (s8 *)malloc(0x00220000);

	psxP_2 = &psxM_2[0x200000];
	psxH_2 = &psxM_2[0x210000];

//	psxR_2 = (s8 *)malloc(0x00080000);
/*
	if (psxMemRLUT == NULL || psxMemWLUT == NULL || 
		psxM_2 == NULL || psxP_2 == NULL || psxH_2 == NULL) {
		SysMessage(_("Error allocating memory!"));
		return -1;
	}
*/
// MemR
	for (i = 0; i < 0x80; i++) psxMemRLUT[i + 0x0000] = (u8 *)&psxM_2[(i & 0x1f) << 16];

	memcpy(psxMemRLUT + 0x8000, psxMemRLUT, 0x80 * sizeof(void *));
	memcpy(psxMemRLUT + 0xa000, psxMemRLUT, 0x80 * sizeof(void *));

	psxMemRLUT[0x1f00] = (u8 *)psxP_2;
	psxMemRLUT[0x1f80] = (u8 *)psxH_2;

	for (i = 0; i < 0x08; i++) psxMemRLUT[i + 0x1fc0] = (u8 *)&psxR_2[i << 16];

	memcpy(psxMemRLUT + 0x9fc0, psxMemRLUT + 0x1fc0, 0x08 * sizeof(void *));
	memcpy(psxMemRLUT + 0xbfc0, psxMemRLUT + 0x1fc0, 0x08 * sizeof(void *));

// MemW
	for (i = 0; i < 0x80; i++) psxMemWLUT[i + 0x0000] = (u8 *)&psxM_2[(i & 0x1f) << 16];

	memcpy(psxMemWLUT + 0x8000, psxMemWLUT, 0x80 * sizeof(void *));
	memcpy(psxMemWLUT + 0xa000, psxMemWLUT, 0x80 * sizeof(void *));

	psxMemWLUT[0x1f00] = (u8 *)psxP_2;
	psxMemWLUT[0x1f80] = (u8 *)psxH_2;

	return 0;
}

extern void pcsxr_lr_notify_user(const char *msg, unsigned frames);

void psxMemReset_2() {
	FILE *f = NULL;
	char bios[1024];

	memset(psxM_2, 0, 0x00200000);
	memset(psxP_2, 0, 0x00010000);

	// Load BIOS
	//There was a previous support for HLE bios, but it's changed now for and
	//embedded openbios
	Config.HLE = FALSE;
	if (strcmp(Config.Bios, OPENBIOS) != 0) {
		sprintf(bios, "%s\\%s", Config.BiosDir, Config.Bios);
		f = fopen(bios, "rb");

		if (f == NULL) {
			SysMessage(_("Could not open BIOS:\"%s\". Enabling Openbios!\n"), bios);
			memcpy(psxR_2, openbios_bin, 0x80000);
			pcsxr_lr_notify_user("Enabling Openbios", 300);
		} else {
			fread(psxR_2, 1, 0x80000, f);
			fclose(f);
		}
	} else {
		memcpy(psxR_2, openbios_bin, 0x80000);
	}
}

void psxMemShutdown_2() {
/*
	free(psxM_2);
    free(psxR_2);
	free(psxMemRLUT);
	free(psxMemWLUT);

	psxP_2 = NULL;
	psxH_2 = NULL;
*/
}

static int writeok = 1;
static int s_sr_isc = 0;   /* ultimo bit IsC (16) de Status visto, ver psxMemSyncIsolation */

/* Aislar la cache (ok = 0): las escrituras de la CPU a la RAM van a la
 * I-cache y no a la memoria, asi que se quitan los bancos de RAM de la tabla
 * de escritura y caen al camino lento (que con el dynarec invalida el bloque).
 * ok = 1 los vuelve a poner. */
static void psxMemSetWriteOk(int ok) {
	int i;
	if (!ok) {
		if (writeok == 0) return;
		writeok = 0;
		memset(psxMemWLUT + 0x0000, 0, 0x80 * sizeof(void *));
		memset(psxMemWLUT + 0x8000, 0, 0x80 * sizeof(void *));
		memset(psxMemWLUT + 0xa000, 0, 0x80 * sizeof(void *));

		psxRegs.ICache_valid = FALSE;
		psxIcacheClear();	/* flush real de la I-cache */
	} else {
		if (writeok == 1) return;
		writeok = 1;
		for (i = 0; i < 0x80; i++) psxMemWLUT[i + 0x0000] = (void *)&psxM_2[(i & 0x1f) << 16];
		memcpy(psxMemWLUT + 0x8000, psxMemWLUT, 0x80 * sizeof(void *));
		memcpy(psxMemWLUT + 0xa000, psxMemWLUT, 0x80 * sizeof(void *));
	}
}

/* [2026-11-11] Aislamiento de la cache por el bit IsC (16) de Status, como
 * pcsx_rearmed (psxMemOnIsolate desde MTC0).  Antes solo se reconocian los
 * valores que escribe la BIOS de Sony en 0xFFFE0130 (0x800/0x804 y
 * 0/0x1E988); OpenBIOS escribe 0x1E90C, asi que su vaciado de la I-cache
 * (sw $zero cada 16 bytes de 0 a 0xFFF con IsC puesto) llegaba a la RAM y
 * borraba sus vectores A0/B0/C0: el "li t0" de 0xA0 desaparecia y el "jr t0"
 * saltaba a basura (0x40000000).  Solo en los CAMBIOS del bit: asi no pisa
 * a la deteccion por 0xFFFE0130, que aisla antes de poner IsC. */
void psxMemSyncIsolation(void) {
	const int isc = (int)((psxRegs.CP0.n.Status >> 16) & 1);
	if (isc == s_sr_isc)
		return;
	s_sr_isc = isc;
	psxMemSetWriteOk(!isc);
}

u8 psxMemRead8_2(u32 mem) {
	char *p;
	u32 t;

	//	psxRegs.cycle += 0;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			return psxHu8_2(mem);
		else
			return psxHwRead8(mem);
	} else {
		p = (char *)(psxMemRLUT[t]);
		if (p != NULL) {
			return *(u8 *)(p + (mem & 0xffff));
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err lb %8.8lx\n", mem);
#endif
			return 0;
		}
	}
}

u16 psxMemRead16_2(u32 mem) {
	char *p;
	u32 t;

	//	psxRegs.cycle += 1;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			return psxHu16_2(mem);
		else
			return psxHwRead16(mem);
	} else {
		p = (char *)(psxMemRLUT[t]);
		if (p != NULL) {
			return SWAPu16(*(u16 *)(p + (mem & 0xffff)));
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err lh %8.8lx\n", mem);
#endif
			return 0;
		}
	}
}

u32 psxMemRead32_2(u32 mem) {
	char *p;
	u32 t;

//	psxRegs.cycle += 1;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			return psxHu32_2(mem);
		else
			return psxHwRead32(mem);
	} else {
		p = (char *)(psxMemRLUT[t]);
		if (p != NULL) {
			return SWAPu32(*(u32 *)(p + (mem & 0xffff)));
		} else {
#ifdef PSXMEM_LOG
			if (writeok) { PSXMEM_LOG("err lw %8.8lx\n", mem); }
#endif
			return 0;
		}
	}
}

void psxMemWrite8_2(u32 mem, u8 value) {
	char *p;
	u32 t;

//	psxRegs.cycle += 1;
	
	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			psxHu8_2(mem) = value;
		else
			psxHwWrite8(mem, value);
	} else {
		p = (char *)(psxMemWLUT[t]);
		if (p != NULL) {
#ifdef PSXREC
			/* [2026-10-31] Solo invalidar el codigo recompilado si el byte
			 * CAMBIA: reescribir lo mismo no cambia ninguna instruccion. */
			const u8 old = *(u8 *)(p + (mem & 0xffff));
			*(u8 *)(p + (mem & 0xffff)) = value;
			if (old != value)
				psxCpu->Clear((mem & (~3)), 1);
#else
			*(u8 *)(p + (mem & 0xffff)) = value;
#endif
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err sb %8.8lx\n", mem);
#endif
		}
	}
}

void psxMemWrite16_2(u32 mem, u16 value) {
	char *p;
	u32 t;

//	psxRegs.cycle += 1;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			psxHu16ref_2(mem) = SWAPu16(value);
		else
			psxHwWrite16(mem, value);
	} else {
		p = (char *)(psxMemWLUT[t]);
		if (p != NULL) {
#ifdef PSXREC
			const u16 old = *(u16 *)(p + (mem & 0xffff));
			*(u16 *)(p + (mem & 0xffff)) = SWAPu16(value);
			if (old != (u16)SWAPu16(value))   /* ver psxMemWrite8_2 */
				psxCpu->Clear((mem & (~3)), 1);
#else
			*(u16 *)(p + (mem & 0xffff)) = SWAPu16(value);
#endif
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err sh %8.8lx\n", mem);
#endif
		}
	}
}

void psxMemWrite32_2(u32 mem, u32 value) {
	char *p;
	u32 t;

//	psxRegs.cycle += 1;

//	if ((mem&0x1fffff) == 0x71E18 || value == 0x48088800) SysPrintf("t2fix!!\n");
	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			psxHu32ref_2(mem) = SWAPu32(value);
		else
			psxHwWrite32(mem, value);
	} else {
		p = (char *)(psxMemWLUT[t]);
		if (p != NULL) {
#ifdef PSXREC
			const u32 old = *(u32 *)(p + (mem & 0xffff));
			*(u32 *)(p + (mem & 0xffff)) = SWAPu32(value);
			if (old != (u32)SWAPu32(value))   /* ver psxMemWrite8_2 */
				psxCpu->Clear(mem, 1);
#else
			*(u32 *)(p + (mem & 0xffff)) = SWAPu32(value);
#endif
		} else {
			if (mem != 0xfffe0130) {
#ifdef PSXREC
				if (!writeok) {
					//printf("Clear32 %08x\n", mem);
					psxCpu->Clear(mem, 1);
				}
#endif

#ifdef PSXMEM_LOG
				if (writeok) { PSXMEM_LOG("err sw %8.8lx\n", mem); }
#endif
			} else {
				// a0-44: used for cache flushing
				switch (value) {
					case 0x800: case 0x804:
						psxMemSetWriteOk(0);
						break;
					case 0x00: case 0x1e988:
						psxMemSetWriteOk(1);
						break;
					default:
#ifdef PSXMEM_LOG
						PSXMEM_LOG("unk %8.8lx = %x\n", mem, value);
#endif
						break;
				}
			}
		}
	}
}


