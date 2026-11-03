// Optional startup RAM diagnostic bypass for three verified CV1000 sets.
// The program is decompressed into RAM by the game before this can be applied.
// Leave the loader, RAM initialization, flash loading and service tests intact.
#ifndef FBNEO_CV1K_FASTBOOT_H
#define FBNEO_CV1K_FASTBOOT_H

struct Cv1kBootPatch {
	const char *name;
	UINT32 crc, entry, caller, literal;
	int layout; // 0: DFK context, 1: SDOJ context
};

static const Cv1kBootPatch cv1k_boot_patches[] = {
	{"dfkbl",   0x8092ca9d, 0x2314f0, 0x231350, 0x2314a0, 0},
	{"ddpdfk",  0x9976d699, 0x22fe20, 0x22fc80, 0x22fdd0, 0},
	{"ddpsdoj", 0xe2a4411c, 0x21e850, 0x21e6a0, 0x21e804, 1}
};
static const Cv1kBootPatch *cv1k_boot_pending;
static unsigned cv1k_boot_checks;

static void cv1k_boot_prepare()
{
	cv1k_boot_pending = NULL; cv1k_boot_checks = 0;
	if (DrvDips[3] & 0x40) return;
	BurnRomInfo rom; BurnDrvGetRomInfo(&rom, 0);
	for (unsigned i = 0; i < sizeof(cv1k_boot_patches) / sizeof(cv1k_boot_patches[0]); ++i) {
		const Cv1kBootPatch &p = cv1k_boot_patches[i];
		if (rom.nCrc == p.crc && !strcmp(BurnDrvGetTextA(DRV_NAME), p.name)) {
			cv1k_boot_pending = &p; break;
		}
	}
}

static bool cv1k_boot_words(UINT32 offset, const UINT16 *words, unsigned count)
{
	const UINT16 *ram = (const UINT16 *)(DrvMainRAM + offset);
	for (unsigned i = 0; i < count; ++i) if (ram[i] != words[i]) return false;
	return true;
}

static void cv1k_boot_step()
{
	if (!cv1k_boot_pending) return;
	if ((DrvDips[3] & 0x40) || ++cv1k_boot_checks > 960) { cv1k_boot_pending = NULL; return; }
	const Cv1kBootPatch &p = *cv1k_boot_pending;
	static const UINT16 entry[] = {0x2f86,0x2f96,0x2fa6,0xe900,0x2fb6,0x6a43,0x2fc6,0x2fd6,0x2fe6};
	static const UINT16 caller[2][16] = {
		{0xd853,0x480b,0x54e6,0x2008,0x8bea,0x53e6,0x5036,0x2008,0x8be6,0x5237,0x2228,0x8be3,0x5834,0x2888,0x8908,0x78ff},
		{0xdb58,0x4b0b,0x64d3,0x2008,0x8bd8,0x5cd8,0x2cc8,0x8bd5,0x50d9,0x2008,0x8bd2,0x58d6,0x2888,0x8909,0x78ff,0xafcd}
	};
	UINT16 target[] = {(UINT16)((0x0c000000 + p.entry) >> 16), (UINT16)p.entry};
	if (!cv1k_boot_words(p.entry, entry, sizeof(entry) / sizeof(entry[0])) ||
		!cv1k_boot_words(p.caller, caller[p.layout], 16) || !cv1k_boot_words(p.literal, target, 2)) return;
	// Return "finished", mark the diagnostic phase complete, clear only its
	// post-test display timer. This callback's writes otherwise only test and
	// restore RAM; it performs no required memory allocation or initialization.
	UINT16 *code = (UINT16 *)(DrvMainRAM + p.entry);
	code[0] = 0xe003;                         // mov #3,r0
	code[1] = p.layout ? 0x1405 : 0x1403;      // mov.l r0,@(phase,r4)
	code[2] = 0xe000;                         // mov #0,r0 (finished)
	code[3] = p.layout ? 0x1406 : 0x1404;      // mov.l r0,@(timer,r4)
	code[4] = 0x000b; code[5] = 0x0009;      // rts; nop
	bprintf(0, _T("CV1000: %s startup RAM test skipped (verified %06x)\n"), p.name, p.entry);
	cv1k_boot_pending = NULL;
}
#endif
