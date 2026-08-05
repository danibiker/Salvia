/*
 * PicoDrive
 * (C) notaz, 2009,2010
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */
#include <string.h>
#include <stddef.h>

#include "sh2.h"
#include "../debug.h"
#include "compiler.h"

#define I 0xf0

int sh2_init(SH2 *sh2, int is_slave, SH2 *other_sh2)
{
	int ret = 0;
	unsigned int mult_m68k_to_sh2 = sh2->mult_m68k_to_sh2;
	unsigned int mult_sh2_to_m68k = sh2->mult_sh2_to_m68k;

	memset(sh2, 0, sizeof(*sh2));
	sh2->is_slave = is_slave;
	sh2->other_sh2 = other_sh2;
	sh2->mult_m68k_to_sh2 = mult_m68k_to_sh2;
	sh2->mult_sh2_to_m68k = mult_sh2_to_m68k;

	pdb_register_cpu(sh2, PDBCT_SH2, is_slave ? "ssh2" : "msh2");
#ifdef DRC_SH2
	ret = sh2_drc_init(sh2);
#endif
	return ret;
}

void sh2_finish(SH2 *sh2)
{
#ifdef DRC_SH2
	sh2_drc_finish(sh2);
#endif
}

void sh2_reset(SH2 *sh2)
{
	sh2->pc = p32x_sh2_read32(0, sh2);
	sh2->r[15] = p32x_sh2_read32(4, sh2);
	sh2->sr = I;
	sh2->vbr = 0;
	sh2->pending_int_irq = 0;
}

void sh2_do_irq(SH2 *sh2, int level, int vector)
{
	sh2->sr &= 0x3f3;

	sh2->r[15] -= 4;
	p32x_sh2_write32(sh2->r[15], sh2->sr, sh2);	/* push SR onto stack */
	sh2->r[15] -= 4;
	p32x_sh2_write32(sh2->r[15], sh2->pc, sh2);	/* push PC onto stack */

	/* set I flags in SR */
	sh2->sr = (sh2->sr & ~I) | (level << 4);

	/* fetch PC */
	sh2->pc = p32x_sh2_read32(sh2->vbr + vector * 4, sh2);

	/* 13 cycles at best */
	sh2->icount -= 13;
}

int sh2_irl_irq(SH2 *sh2, int level, int nested_call)
{
	int taken;

	sh2->pending_irl = level;
	if (level < sh2->pending_int_irq)
		level = sh2->pending_int_irq;
	sh2->pending_level = level;

	taken = (level > ((sh2->sr >> 4) & 0x0f));
	if (taken) {
		if (!nested_call) {
			// not in memhandler, so handle this now (recompiler friendly)
			// do this to avoid missing irqs that other SH2 might clear
			int vector = sh2->irq_callback(sh2, level);
			sh2_do_irq(sh2, level, vector);
			sh2->m68krcycles_done += C_SH2_TO_M68K(sh2, 13);
		}
		else
			sh2->test_irq = 1;
	}
	return taken;
}

void sh2_internal_irq(SH2 *sh2, int level, int vector)
{
	// FIXME: multiple internal irqs not handled..
	// assuming internal irqs never clear until accepted
	sh2->pending_int_irq = level;
	sh2->pending_int_vector = vector;
	if (level > sh2->pending_level)
		sh2->pending_level = level;

	sh2->test_irq = 1;
}

#define SH2_REG_SIZE (offsetof(SH2, macl) + sizeof(sh2->macl))

void sh2_pack(const SH2 *sh2, unsigned char *buff)
{
	unsigned int *p;

	memcpy(buff, sh2, SH2_REG_SIZE);
	p = (void *)(buff + SH2_REG_SIZE);

	p[0] = sh2->pending_int_irq;
	p[1] = sh2->pending_int_vector;
	p[2] = sh2->m68krcycles_done;
}

void sh2_unpack(SH2 *sh2, const unsigned char *buff)
{
	unsigned int *p;

	memcpy(sh2, buff, SH2_REG_SIZE);
	p = (void *)(buff + SH2_REG_SIZE);

	sh2->pending_int_irq = p[0];
	sh2->pending_int_vector = p[1];
	sh2->test_irq = 1;
	sh2->m68krcycles_done = p[2];
}

#ifdef DRC_CMP

/* trace/compare */
#include <stdio.h>
#include <stdlib.h>
#include <pico/memory.h>
#undef _USE_CZ80 // HACK
#include <pico/pico_int.h>
#include <pico/debug.h>

/* [Salvia/Xbox360] Redirect the DRC_CMP comparator's printf to lprintf so the
 * divergence reports show up in the libretro log (RetroArch), not the debugger
 * output which is not visible on the devkit. */
#if defined(_XBOX) || defined(_XBOX360)
extern void lprintf(const char *fmt, ...);
#undef printf
#define printf lprintf
#endif

static SH2 sh2ref[2];
static unsigned int mem_val;

static unsigned int local_read32(SH2 *sh2, u32 a)
{
	const sh2_memmap *sh2_map = sh2->read16_map;
	u16 *pd;
	uptr p;

	sh2_map += (a >> 25);
	p = sh2_map->addr;
	if (!map_flag_set(p)) {
		pd = (u16 *)((p << 1) + ((a & sh2_map->mask) & ~1));
		return (pd[0] << 16) | pd[1];
	}

	if ((a & 0xfffff000) == 0xc0000000) {
		// data array
		pd = (u16 *)sh2->data_array + (a & 0xfff) / 2;
		return (pd[0] << 16) | pd[1];
	}
	if ((a & 0xdfffffc0) == 0x4000) {
		pd = &Pico32x.regs[(a & 0x3f) / 2];
		return (pd[0] << 16) | pd[1];
	}
	if ((a & 0xdffffe00) == 0x4200) {
		pd = &Pico32xMem->pal[(a & 0x1ff) / 2];
		return (pd[0] << 16) | pd[1];
	}

	return 0;
}

void do_sh2_trace(SH2 *current, int cycles)
{
	static int current_slave = -1;
	static u32 current_m68k_pc;
	SH2 *sh2o = &sh2ref[current->is_slave];
	u32 *regs_a = (void *)current;
	u32 *regs_o = (void *)sh2o;
	unsigned char v;
	u32 val;
	int i;

#ifdef SALVIA_CMP_SH2
	/* [Salvia/Xbox360] record ONLY one SH2 (0=master, 1=slave), same as
	 * do_sh2_cmp, to avoid the interleaved two-SH2 trace getting out of sync. */
	if (current->is_slave != (SALVIA_CMP_SH2))
		return;
#endif
#ifdef SALVIA_CMP_START_PC
	/* [Salvia/Xbox360] same PC gate as do_sh2_cmp: don't record until the SH2
	 * first reaches SALVIA_CMP_START_PC. Both sides stay aligned. */
	{
		static int armed_ = 0;
		if (!armed_) {
			if (current->pc != (u32)(SALVIA_CMP_START_PC)) return;
			armed_ = 1;
		}
	}
#endif
#ifdef SALVIA_CMP_START_FRAME
	/* [Salvia/Xbox360] frame gate: don't record until close to the point of
	 * interest, to shrink the tracelog and skip boot/menu. COMPARE uses the SAME
	 * threshold, so both start tracing at the same emulated frame (the DRC is
	 * correct up to the failure, so the state matches there). */
	if (Pico.m.frame_count < (unsigned)(SALVIA_CMP_START_FRAME))
		return;
#endif

	if (SekPc != current_m68k_pc) {
		current_m68k_pc = SekPc;
		tl_write_uint(CTL_M68KPC, current_m68k_pc);
	}

	if (current->is_slave != current_slave) {
		current_slave = current->is_slave;
		v = CTL_MASTERSLAVE | current->is_slave;
		tl_write(&v, sizeof(v));
	}

	for (i = 0; i < offsetof(SH2, read8_map) / 4; i++) {
		if (i == 17) // ppc
			continue;
		if (regs_a[i] != regs_o[i]) {
			tl_write_uint(CTL_SH2_R + i, regs_a[i]);
			regs_o[i] = regs_a[i];
		}
	}

	if (current->ea != sh2o->ea) {
		tl_write_uint(CTL_EA, current->ea);
		sh2o->ea = current->ea;
	}
	val = local_read32(current, current->ea);
	if (mem_val != val) {
		tl_write_uint(CTL_EAVAL, val);
		mem_val = val;
	}
	tl_write_uint(CTL_CYCLES, cycles);
}

static const char *regnames[] = {
	"r0",  "r1",  "r2",  "r3",
	"r4",  "r5",  "r6",  "r7",
	"r8",  "r9",  "r10", "r11",
	"r12", "r13", "r14", "r15",
	"pc",  "ppc", "pr",  "sr",
	"gbr", "vbr", "mach","macl",
};

static void dump_regs(SH2 *sh2)
{
	char csh2;
	int i;

	csh2 = sh2->is_slave ? 's' : 'm';
	for (i = 0; i < 16/2; i++)
		printf("%csh2 r%d: %08x r%02d: %08x\n", csh2,
			i, sh2->r[i], i+8, sh2->r[i+8]);
	printf("%csh2 PC: %08x  ,   %08x\n", csh2, sh2->pc, sh2->ppc);
	printf("%csh2 SR:      %03x  PR: %08x\n", csh2, sh2->sr, sh2->pr);
}

void REGPARM(1) do_sh2_cmp(SH2 *current)
{
	static int current_slave;
	static u32 current_val;
	SH2 *sh2o = &sh2ref[current->is_slave];
	u32 *regs_a = (void *)current;
	u32 *regs_o = (void *)sh2o;
	unsigned char code;
	int cycles_o = 666;
	u32 sr, val;
	int bad = 0;
	int cycles;
	int i, ret;
	static int cmp_stopped = 0;

	/* [Salvia/Xbox360] after the FIRST real divergence, stop: no more trace reads
	 * or comparisons -> a clean log with a single divergence. */
	if (cmp_stopped)
		return;
#ifdef SALVIA_CMP_SH2
	/* [Salvia/Xbox360] compare ONLY one SH2 (0=master, 1=slave). The interleaved
	 * two-SH2 trace drifts out of sync (cycle-count differences); comparing a
	 * single one keeps the stream self-consistent. do_sh2_trace filters the same. */
	if (current->is_slave != (SALVIA_CMP_SH2))
		return;
#endif
#ifdef SALVIA_CMP_START_PC
	/* [Salvia/Xbox360] PC gate: don't compare until the SH2 first reaches this PC
	 * (e.g. the entry of a problematic loop). This starts the comparison ALIGNED
	 * right there, so the first divergence is the exact mistranslated instruction
	 * (without the cascade from being out of sync). */
	{
		static int armed_ = 0;
		if (!armed_) {
			if (current->pc != (u32)(SALVIA_CMP_START_PC)) return;
			armed_ = 1;
		}
	}
#endif
#ifdef SALVIA_CMP_START_FRAME
	/* [Salvia/Xbox360] same gate as do_sh2_trace: don't compare until the frame
	 * threshold (the trace has no data before it either). Both sides aligned. */
	if (Pico.m.frame_count < (unsigned)(SALVIA_CMP_START_FRAME))
		return;
#endif

#if 0
	sr = current->sr;
	current->sr &= 0x3f3;
	do_sh2_trace(current, (signed int)sr >> 12);
	current->sr = sr;
	return;
#endif
	sh2ref[1].is_slave = 1;

	while (1) {
		ret = tl_read(&code, 1);
		if (ret <= 0)
			break;
		if (code == CTL_CYCLES) {
			tl_read(&cycles_o, 4);
			break;
		}

		switch (code) {
		case CTL_MASTERSLAVE:
		case CTL_MASTERSLAVE + 1:
			current_slave = code & 1;
			break;
		case CTL_EA:
			tl_read_uint(&sh2o->ea);
			break;
		case CTL_EAVAL:
			tl_read_uint(&current_val);
			break;
		case CTL_M68KPC:
			tl_read_uint(&val);
			if (SekPc != val) {
				printf("m68k: %08x %08x\n", SekPc, val);
				bad = 1;
			}
			break;
		default:
			if (CTL_SH2_R <= code && code < CTL_SH2_R +
			    offsetof(SH2, read8_map) / 4)
			{
				tl_read_uint(regs_o + code - CTL_SH2_R);
			}
			else
			{
				printf("wrong code: %02x\n", code);
				goto end;
			}
			break;
		}
	}

	if (ret <= 0) {
		printf("EOF?\n");
		goto end;
	}

	if (current->is_slave != current_slave) {
		printf("bad slave: %d %d\n", current->is_slave,
			current_slave);
		bad = 1;
	}

	for (i = 0; i < offsetof(SH2, read8_map) / 4; i++) {
		if (i == 17 || i == 19) // ppc, sr
			continue;
		if (regs_a[i] != regs_o[i]) {
			printf("bad %4s: %08x %08x\n",
				regnames[i], regs_a[i], regs_o[i]);
			bad = 1;
		}
	}

	sr = current->sr & 0x3f3;
	cycles = (signed int)current->sr >> 12;

	/* [Salvia/Xbox360] The tracelog stores the full SR (with the cycle counter
	 * packed into the high bits). Strip sh2o->sr to 0x3f3 as well so we compare
	 * only the real flags (T,S,I,M,Q); otherwise we'd get "bad SR" on every
	 * instruction where the cycle counts differ. */
	if (sr != (sh2o->sr & 0x3f3)) {
		printf("bad SR:  %03x %03x\n", sr, sh2o->sr & 0x3f3);
		bad = 1;
	}

	/* [Salvia/Xbox360] Cycles: do NOT set bad (never stop on cycles), but report
	 * the FIRST instruction where they diverge - to tell whether there is a real
	 * cycle drift (a PPC bug at that instr) or they are aligned (=> the bug is
	 * pure codegen, not timing). Only meaningful in aligned master-only mode. */
	if (cycles != cycles_o) {
		static int cyc_drift_reported = 0;
		if (!cyc_drift_reported) {
			printf("[cyc] first cycle drift at pc=%08x: drc=%d ref=%d (diff=%d)\n",
				current->pc, cycles, cycles_o, cycles - cycles_o);
			cyc_drift_reported = 1;
		}
	}

	/* [Salvia/Xbox360] val (memory at EA) check: if the master writes memory
	 * WRONG (endianness/size of a store) with correct registers, this is the only
	 * check that catches it - and it fires BEFORE the poll-loop desync. sh2o->ea
	 * is the EA recorded by the interpreter; read the current (DRC) memory there
	 * and compare with the recorded value. */
	val = local_read32(current, sh2o->ea);
	if (val != current_val) {
		printf("bad val @%08x: %08x %08x\n", sh2o->ea, val, current_val);
		bad = 1;
	}

	if (!bad) {
		sh2o->ppc = current->pc;
		return;
	}

end:
	printf("--\n");
	printf("[Salvia] DRC @ ppc=%08x pc=%08x is_slave=%d (^=diverging instruction)\n",
		current->ppc, current->pc, current->is_slave);
	dump_regs(sh2o);
	if (current->is_slave != current_slave)
		dump_regs(&sh2ref[current->is_slave ^ 1]);
#if defined(_XBOX) || defined(_XBOX360)
	/* [Salvia/Xbox360] No exit(1) (reboots the console and loses the log) nor
	 * PDebugDumpMem (nonexistent path). This is the FIRST real divergence: the
	 * dump above already shows it. Set the stop flag to avoid a cascade and
	 * return; the game keeps running (possibly hangs), but the log already has the
	 * clean divergence with its PC/register. */
	printf("=== FIRST DIVERGENCE (comparison stopped here) ===\n");
	cmp_stopped = 1;
	return;
#else
	PDebugDumpMem();
	exit(1);
#endif
}

#endif // DRC_CMP
