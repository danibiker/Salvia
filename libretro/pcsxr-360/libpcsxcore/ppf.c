/*  PPF/SBI Support for PCSX-Reloaded
 *  Copyright (c) 2009, Wei Mingzhi <whistler_wmz@users.sf.net>.
 *  Copyright (c) 2010, shalma.
 *
 *  PPF code based on P.E.Op.S CDR Plugin by Pete Bernert.
 *  Copyright (c) 2002, Pete Bernert.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include "psxcommon.h"
#include "ppf.h"
#include "cdrom.h"
#include "plugins.h"

/* Implemented in the libretro frontend (libretro_core.cpp): surfaces a
 * user-visible message through RETRO_ENVIRONMENT_SET_MESSAGE. */
extern void pcsxr_lr_notify_user(const char *msg, unsigned frames);

typedef struct tagPPF_DATA {
	s32					addr;
	s32					pos;
	s32					anz;
	struct tagPPF_DATA	*pNext;
} PPF_DATA;

typedef struct tagPPF_CACHE {
	s32					addr;
	struct tagPPF_DATA	*pNext;
} PPF_CACHE;

static PPF_CACHE		*ppfCache = NULL;
static PPF_DATA			*ppfHead = NULL, *ppfLast = NULL;
static int				iPPFNum = 0;

// using a linked data list, and address array
static void FillPPFCache() {
	PPF_DATA		*p;
	PPF_CACHE		*pc;
	s32				lastaddr;

	p = ppfHead;
	lastaddr = -1;
	iPPFNum = 0;

	while (p != NULL) {
		if (p->addr != lastaddr) iPPFNum++;
		lastaddr = p->addr;
		p = p->pNext;
	}

	if (iPPFNum <= 0) return;

	pc = ppfCache = (PPF_CACHE *)malloc(iPPFNum * sizeof(PPF_CACHE));

	iPPFNum--;
	p = ppfHead;
	lastaddr = -1;

	while (p != NULL) {
		if (p->addr != lastaddr) {
			pc->addr = p->addr;
			pc->pNext = p;
			pc++;
		}
		lastaddr = p->addr;
		p = p->pNext;
	}
}

void FreePPFCache() {
	PPF_DATA *p = ppfHead;
	void *pn;

	while (p != NULL) {
		pn = p->pNext;
		free(p);
		p = (PPF_DATA *)pn;
	}
	ppfHead = NULL;
	ppfLast = NULL;

	if (ppfCache != NULL) free(ppfCache);
	ppfCache = NULL;
}

void CheckPPFCache(unsigned char *pB, unsigned char m, unsigned char s, unsigned char f) {
	PPF_CACHE *pcstart, *pcend, *pcpos;
	int addr = MSF2SECT(btoi(m), btoi(s), btoi(f)), pos, anz, start;

	if (ppfCache == NULL) return;

	pcstart = ppfCache;
	if (addr < pcstart->addr) return;
	pcend = ppfCache + iPPFNum;
	if (addr > pcend->addr) return;

	while (1) {
		if (addr == pcend->addr) { pcpos = pcend; break; }

		pcpos = pcstart + (pcend - pcstart) / 2;
		if (pcpos == pcstart) break;
		if (addr < pcpos->addr) {
			pcend = pcpos;
			continue;
		}
		if (addr > pcpos->addr) {
			pcstart = pcpos;
			continue;
		}
		break;
	}

	if (addr == pcpos->addr) {
		PPF_DATA *p = pcpos->pNext;
		while (p != NULL && p->addr == addr) {
			pos = p->pos - (CD_FRAMESIZE_RAW - DATA_SIZE);
			anz = p->anz;
			if (pos < 0) { start = -pos; pos = 0; anz -= start; }
			else start = 0;
			memcpy(pB + pos, (unsigned char *)(p + 1) + start, anz);
			p = p->pNext;
		}
	}
}

static void AddToPPF(s32 ladr, s32 pos, s32 anz, unsigned char *ppfmem) {
	if (ppfHead == NULL) {
		ppfHead = (PPF_DATA *)malloc(sizeof(PPF_DATA) + anz);
		ppfHead->addr = ladr;
		ppfHead->pNext = NULL;
		ppfHead->pos = pos;
		ppfHead->anz = anz;
		memcpy(ppfHead + 1, ppfmem, anz);
		iPPFNum = 1;
		ppfLast = ppfHead;
	} else {
		PPF_DATA *p = ppfHead;
		PPF_DATA *plast = NULL;
		PPF_DATA *padd;

		if (ladr > ppfLast->addr || (ladr == ppfLast->addr && pos > ppfLast->pos)) {
			p = NULL;
			plast = ppfLast;
		} else {
			while (p != NULL) {
				if (ladr < p->addr) break;
				if (ladr == p->addr) {
					while (p && ladr == p->addr && pos > p->pos) {
						plast = p;
						p = p->pNext;
					}
					break;
				}
				plast = p;
				p = p->pNext;
			}
		}

		padd = (PPF_DATA *)malloc(sizeof(PPF_DATA) + anz);
		padd->addr = ladr;
		padd->pNext = p;
		padd->pos = pos;
		padd->anz = anz;
		memcpy(padd + 1, ppfmem, anz);
		iPPFNum++;
		if (plast == NULL) ppfHead = padd;
		else plast->pNext = padd;

		if (padd->pNext == NULL) ppfLast = padd;
	}
}

void BuildPPFCache() {
	FILE			*ppffile;
	char			buffer[12];
	char			method, undo = 0, blockcheck = 0;
	int				dizlen, dizyn;
	unsigned char	ppfmem[512];
	char			szPPF[MAXPATHLEN];
	int				count, seekpos, pos;
	u32				anz; // use 32-bit to avoid stupid overflows
	s32				ladr, off, anx;

	FreePPFCache();

	// Generate filename in the format of SLUS_123.45
	buffer[0] = toupper(CdromId[0]);
	buffer[1] = toupper(CdromId[1]);
	buffer[2] = toupper(CdromId[2]);
	buffer[3] = toupper(CdromId[3]);
	buffer[4] = '_';
	buffer[5] = CdromId[4];
	buffer[6] = CdromId[5];
	buffer[7] = CdromId[6];
	buffer[8] = '.';
	buffer[9] = CdromId[7];
	buffer[10] = CdromId[8];
	buffer[11] = '\0';

	sprintf(szPPF, "%s%s", Config.PatchesDir, buffer);

	ppffile = fopen(szPPF, "rb");
	if (ppffile == NULL) return;

	memset(buffer, 0, 5);
	fread(buffer, 3, 1, ppffile);

	if (strcmp(buffer, "PPF") != 0) {
		SysPrintf(_("Invalid PPF patch: %s.\n"), szPPF);
		fclose(ppffile);
		return;
	}

	fseek(ppffile, 5, SEEK_SET);
	method = fgetc(ppffile);

	switch (method) {
		case 0: // ppf1
			fseek(ppffile, 0, SEEK_END);
			count = ftell(ppffile);
			count -= 56;
			seekpos = 56;
			break;

		case 1: // ppf2
			fseek(ppffile, -8, SEEK_END);

			memset(buffer, 0, 5);
			fread(buffer, 4, 1, ppffile);

			if (strcmp(".DIZ", buffer) != 0) {
				dizyn = 0;
			} else {
				fread(&dizlen, 4, 1, ppffile);
				dizlen = SWAP32(dizlen);
				dizyn = 1;
			}

			fseek(ppffile, 0, SEEK_END);
			count = ftell(ppffile);

			if (dizyn == 0) {
				count -= 1084;
				seekpos = 1084;
			} else {
				count -= 1084;
				count -= 38;
				count -= dizlen;
				seekpos = 1084;
			}
			break;

		case 2: // ppf3
			fseek(ppffile, 57, SEEK_SET);
			blockcheck = fgetc(ppffile);
			undo = fgetc(ppffile);

			fseek(ppffile, -6, SEEK_END);
			memset(buffer, 0, 5);
			fread(buffer, 4, 1, ppffile);
			dizlen = 0;

			if (strcmp(".DIZ", buffer) == 0) {
				fseek(ppffile, -2, SEEK_END);
				fread(&dizlen, 2, 1, ppffile);
				dizlen = SWAP32(dizlen);
				dizlen += 36;
			}

			fseek(ppffile, 0, SEEK_END);
			count = ftell(ppffile);
			count -= dizlen;

			if (blockcheck) {
				seekpos = 1084;
				count -= 1084;
			} else {
				seekpos = 60;
				count -= 60;
			}
			break;

		default:
			fclose(ppffile);
			SysPrintf(_("Unsupported PPF version (%d).\n"), method + 1);
			return;
	}

	// now do the data reading
	do {                                                
		fseek(ppffile, seekpos, SEEK_SET);
		fread(&pos, 4, 1, ppffile);
		pos = SWAP32(pos);

		if (method == 2) fread(buffer, 4, 1, ppffile); // skip 4 bytes on ppf3 (no int64 support here)

		anz = fgetc(ppffile);
		fread(ppfmem, anz, 1, ppffile);   

		ladr = pos / CD_FRAMESIZE_RAW;
		off = pos % CD_FRAMESIZE_RAW;

		if (off + anz > CD_FRAMESIZE_RAW) {
			anx = off + anz - CD_FRAMESIZE_RAW;
			anz -= (unsigned char)anx;
			AddToPPF(ladr + 1, 0, anx, &ppfmem[anz]);
		}

		AddToPPF(ladr, off, anz, ppfmem); // add to link list

		if (method == 2) {
			if (undo) anz += anz;
			anz += 4;
		}

		seekpos = seekpos + 5 + anz;
		count = count - 5 - anz;
	} while (count != 0); // loop til end

	fclose(ppffile);

	FillPPFCache(); // build address array

	SysPrintf(_("Loaded PPF %d.0 patch: %s.\n"), method + 1, szPPF);
}

// LibCrypt-protected game serials (PAL only).
// Source: alex-free/libcrypt-patcher (lcp.c), cross-checked against redump.org.
// Used to emit a clear warning when a known libcrypt title is booted without
// a matching .sbi file, so the user knows exactly what is missing.
static const char libcrypt_serials[][10] = {
	"SCES00311", "SCES01431", "SCES01444", "SCES01492", "SCES01493",
	"SCES01494", "SCES01495", "SCES01516", "SCES01517", "SCES01518",
	"SCES01519", "SCES01564", "SCES01695", "SCES01700", "SCES01701",
	"SCES01702", "SCES01703", "SCES01704", "SCES01763", "SCES01882",
	"SCES01909", "SCES01979", "SCES02004", "SCES02005", "SCES02006",
	"SCES02007", "SCES02028", "SCES02029", "SCES02030", "SCES02031",
	"SCES02104", "SCES02105", "SCES02181", "SCES02182", "SCES02184",
	"SCES02185", "SCES02222", "SCES02264", "SCES02269", "SCES02290",
	"SCES02365", "SCES02366", "SCES02367", "SCES02368", "SCES02369",
	"SCES02430", "SCES02431", "SCES02432", "SCES02433", "SCES02487",
	"SCES02488", "SCES02489", "SCES02490", "SCES02491", "SCES02544",
	"SCES02545", "SCES02546", "SCES02834", "SCES02835",
	"SLES00017", "SLES00995", "SLES01041", "SLES01226", "SLES01241",
	"SLES01301", "SLES01362", "SLES01545", "SLES01715", "SLES01733",
	"SLES01879", "SLES01880", "SLES01906", "SLES01907", "SLES01953",
	"SLES02024", "SLES02025", "SLES02026", "SLES02027", "SLES02061",
	"SLES02071", "SLES02080", "SLES02081", "SLES02082", "SLES02083",
	"SLES02084", "SLES02086", "SLES02112", "SLES02113", "SLES02118",
	"SLES02207", "SLES02208", "SLES02209", "SLES02210", "SLES02211",
	"SLES02292", "SLES02293", "SLES02328", "SLES02329", "SLES02330",
	"SLES02354", "SLES02355", "SLES02395", "SLES02396", "SLES02402",
	"SLES02529", "SLES02530", "SLES02531", "SLES02532", "SLES02533",
	"SLES02538", "SLES02558", "SLES02559", "SLES02560", "SLES02561",
	"SLES02562", "SLES02563", "SLES02572", "SLES02573", "SLES02681",
	"SLES02688", "SLES02689", "SLES02698", "SLES02700", "SLES02704",
	"SLES02705", "SLES02706", "SLES02707", "SLES02708", "SLES02722",
	"SLES02723", "SLES02724", "SLES02733", "SLES02754", "SLES02755",
	"SLES02756", "SLES02763", "SLES02766", "SLES02767", "SLES02768",
	"SLES02769", "SLES02824", "SLES02830", "SLES02831", "SLES02839",
	"SLES02857", "SLES02858", "SLES02859", "SLES02860", "SLES02861",
	"SLES02862", "SLES02965", "SLES02966", "SLES02967", "SLES02968",
	"SLES02969", "SLES02975", "SLES02976", "SLES02977", "SLES02978",
	"SLES02979", "SLES03061", "SLES03062", "SLES03189", "SLES03190",
	"SLES03191", "SLES03241", "SLES03242", "SLES03243", "SLES03244",
	"SLES03245", "SLES03324", "SLES03489", "SLES03519", "SLES03520",
	"SLES03521", "SLES03522", "SLES03523", "SLES03530", "SLES03603",
	"SLES03604", "SLES03605", "SLES03606", "SLES03607", "SLES03626",
	"SLES03648",
	"SLES11879", "SLES11880", "SLES12080", "SLES12081", "SLES12082",
	"SLES12083", "SLES12084", "SLES12328", "SLES12329", "SLES12330",
	"SLES12558", "SLES12559", "SLES12560", "SLES12561", "SLES12562",
	"SLES12965", "SLES12966", "SLES12967", "SLES12968", "SLES12969",
	"SLES22080", "SLES22081", "SLES22082", "SLES22083", "SLES22328",
	"SLES22329", "SLES22330", "SLES22965", "SLES22966", "SLES22967",
	"SLES22968", "SLES22969",
	"SLES32080", "SLES32081", "SLES32082", "SLES32083", "SLES32084",
	"SLES32965", "SLES32966", "SLES32967", "SLES32968", "SLES32969",
};

static int is_libcrypt_serial(const char *cdrom_id) {
	size_t i, n = sizeof(libcrypt_serials) / sizeof(libcrypt_serials[0]);
	char id[10];

	for (i = 0; i < 9 && cdrom_id[i] != '\0'; i++)
		id[i] = (char)toupper((unsigned char)cdrom_id[i]);
	id[i] = '\0';
	if (i < 9) return 0;

	for (i = 0; i < n; i++) {
		if (strcmp(id, libcrypt_serials[i]) == 0)
			return 1;
	}
	return 0;
}

// redump.org SBI files
static u8 sbitime[256][3], sbicount;

void LoadSBI() {
	FILE *sbihandle = NULL;
	char buffer[16], sbifile[MAXPATHLEN];
	const char *isoFile;

	// init
	sbicount = 0;

	// Build canonical <CdromId>.sbi name (SLES_012.26.sbi form). Used both
	// for the PatchesDir fallback and for the libcrypt warning message.
	buffer[0] = toupper(CdromId[0]);
	buffer[1] = toupper(CdromId[1]);
	buffer[2] = toupper(CdromId[2]);
	buffer[3] = toupper(CdromId[3]);
	buffer[4] = '_';
	buffer[5] = CdromId[4];
	buffer[6] = CdromId[5];
	buffer[7] = CdromId[6];
	buffer[8] = '.';
	buffer[9] = CdromId[7];
	buffer[10] = CdromId[8];
	buffer[11] = '.';
	buffer[12] = 's';
	buffer[13] = 'b';
	buffer[14] = 'i';
	buffer[15] = '\0';

	// 1) Try <image_path_without_ext>.sbi next to the loaded image,
	//    so any dump works without renaming or copying to PatchesDir.
	isoFile = GetIsoFile();
	if (isoFile != NULL && isoFile[0] != '\0') {
		const char *slash1, *slash2, *sep, *dot;
		size_t base_len;

		strncpy(sbifile, isoFile, MAXPATHLEN - 1);
		sbifile[MAXPATHLEN - 1] = '\0';

		slash1 = strrchr(sbifile, '/');
		slash2 = strrchr(sbifile, '\\');
		sep = slash1;
		if (slash2 != NULL && (sep == NULL || slash2 > sep)) sep = slash2;

		dot = strrchr(sbifile, '.');
		if (dot != NULL && (sep == NULL || dot > sep))
			base_len = dot - sbifile;
		else
			base_len = strlen(sbifile);

		if (base_len + 5 <= MAXPATHLEN) {
			strcpy(sbifile + base_len, ".sbi");
			sbihandle = fopen(sbifile, "rb");
		}
	}

	// 2) Fallback: <PatchesDir><CdromId>.sbi (format SLES_012.26.sbi).
	if (sbihandle == NULL) {
		sprintf(sbifile, "%s%s", Config.PatchesDir, buffer);
		sbihandle = fopen(sbifile, "rb");
	}

	if (sbihandle == NULL) {
		if (is_libcrypt_serial(CdromId)) {
			char warn[256];
			_snprintf(warn, sizeof(warn),
			         "LibCrypt: %.9s requires %s. Place it next to the image "
			         "or in the patches dir %s",
			         CdromId, buffer, Config.PatchesDir);
			pcsxr_lr_notify_user(warn, 600);
		}
		return;
	}

	// 4-byte SBI header
	fread(buffer, 1, 4, sbihandle);
	while (!feof(sbihandle)) {
		fread(sbitime[sbicount++], 1, 3, sbihandle);
		fread(buffer, 1, 11, sbihandle);
	}

	fclose(sbihandle);

	SysPrintf(_("Loaded SBI file: %s.\n"), sbifile);
}

boolean CheckSBI(const u8 *time) {
	int lcv;

	// both BCD format
	for (lcv = 0; lcv < sbicount; lcv++) {
		if (time[0] == sbitime[lcv][0] && 
				time[1] == sbitime[lcv][1] && 
				time[2] == sbitime[lcv][2])
			return TRUE;
	}

	return FALSE;
}
