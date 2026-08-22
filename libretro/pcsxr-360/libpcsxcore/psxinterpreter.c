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
 * PSX assembly interpreter.
 */

#include "psxcommon.h"
#include "r3000a.h"
#include "gpu.h"	/* PCSXR_DIAG_INSTRUMENTATION */
#include "gte.h"
#include "psxhle.h"
#include <process.h>

BOOL frontmission3fix = 0;

static int branch = 0;
static int branch2 = 0;
static u32 branchPC;

// These macros are used to assemble the repassembler functions

#ifdef PSXCPU_LOG
#define debugI() PSXCPU_LOG("%s\n", disR3000AF(psxRegs.code, psxRegs.pc)); 
#else
#define debugI()
#endif

/* [XBOX360] Fetch de instruccion CON GUARDA, y destino de salto enmascarado.
 * Alineado con upstream pcsx_rearmed: fetchNoCache() comprueba el LUT y, si
 * el PC no esta mapeado, reporta "game crash @pc, ra=..." y ejecuta un NOP;
 * doBranchReg() enmascara el destino con ~3.
 *
 * Aqui los TRES sitios de fetch hacian *(u32 *)PSXM_2(pc) a pelo.  Con un PC
 * basura PSXM_2 devuelve NULL y el ANFITRION revienta con una access
 * violation leyendo la direccion 0 -- sin decir nada del juego.  Y jr/jalr
 * pasaban el registro crudo, asi que el PC podia quedar DESALINEADO (visto:
 * ra=0x00430035, pc=0x004300D5), algo imposible en un R3000A real. */
extern void pcsxr_log(int level, const char *format, ...);

/* [XBOX360] DIAGNOSTICO: lo que este fichero usa de fuera, agrupado aqui
 * arriba para que este visible en todo el fichero.  Las definiciones viven
 * en r3000a.c. */
#if PCSXR_DIAG_INSTRUMENTATION
extern void diag_sr_note(char src, u32 pc, u32 val);	/* anillo CP0.Status */
extern void diag_rdidx_note(u32 addr, u32 val);		/* vigia del indice de drain */
#else
/* Sin volcados que las lean, recoger es coste puro (diag_rdidx_note esta en
 * psxSW: cada store de 32 bits del interprete). */
#define diag_sr_note(s, p, v)      ((void)0)
#define diag_rdidx_note(a, v)      ((void)0)
#endif

static int s_fetch_crash_logged = 0;
static int s_jump_align_logged = 0;

/* [XBOX360] I-CACHE del R3000A.  Portado de pcsx_rearmed (fetchICache), cuyo
 * comentario es literalmente "Formula One 2001: use old CPU cache code when the
 * RAM location is updated with new code", y cuya core option dice "Required for
 * Formula One 2001, Formula One Arcade and Formula One 99".
 *
 * 4 KB, 256 lineas de 16 bytes, mapeo directo: el indice de linea es
 * (pc >> 4) & 0xFF.  Lo confirma el propio juego: la funcion 0x801F5808
 * recorre las 0x60 lineas del descompresor (desde 0x80013A00) comparando
 * ((a0>>4)+i) & 0xFF contra (a2>>4) & 0xFF y empuja a2 16 bytes por cada
 * colision, hasta dar con una linea libre.  Sale a2 = 0x80023000 (linea 0x00,
 * fuera del rango 0xA0..0xFF del descompresor), copia ahi 16 bytes desde
 * 0x801F57EC, los llama para calentar la cache, y luego el descompresor
 * escribe 0x197000 bytes (medido: end=0x801B9F00, ultimo byte 0x801B9EFF)
 * pisando esa RAM.  Al volver, el jalr sobre 0x80023000 tiene que ejecutar el
 * stub CACHEADO, no lo que quedo en memoria.
 *
 * NO se invalida en las escrituras a RAM: eso es justo lo que el juego explota.
 * Se limpia en reset y en el flush explicito de 0xFFFE0130 (psxmem.c). */
struct pcsxr_icache_line {
	u32 tag;
	u32 data[4];
};
static struct pcsxr_icache_line s_icache[256];
static int s_icache_on = 0;	/* la usa el fetch del INTERPRETE */
static int s_icache_dyn = 0;	/* la usa el compilador del DYNAREC */

/* Divergencia real: veces que el compilador leyo de la cache un valor DISTINTO
 * al que hay en RAM.  Es la medida que valida el modo dynarec: si en Ace Combat
 * 2 / NFS3 / GT2 / ISS Pro esto se queda a 0, el cambio es demostrablemente
 * inerte para ellos; si en Formula One 99 es >0 y el juego va, la emulacion
 * esta haciendo su trabajo. */
static u32 s_icdiv_n = 0;
static u32 s_icdiv_pc = 0;
static u32 s_icdiv_cached = 0;
static u32 s_icdiv_ram = 0;
/* Siguiente hito a reportar.  El volcado periodico de r3000a.c vive bajo
 * PCSXR_DIAG_INSTRUMENTATION, que esta a 0, asi que [ICDIV] no salia NUNCA y la
 * medicion no se hacia.  Esto se auto-reporta: la primera divergencia y luego
 * cada x100.  Son 3-4 lineas por sesion como mucho. */
static u32 s_icdiv_next = 1;

void psxIcacheClear(void)
{
	memset(s_icache, 0xff, sizeof(s_icache));
}

void psxIcacheStats(u32 *n, u32 *pc, u32 *cached, u32 *ram)
{
	if (n)      *n = s_icdiv_n;
	if (pc)     *pc = s_icdiv_pc;
	if (cached) *cached = s_icdiv_cached;
	if (ram)    *ram = s_icdiv_ram;
}

/* Un solo punto de configuracion, llamado desde psxReset() y desde intReset().
 * En modo dynarec el INTERPRETE tambien usa la cache: el recompilador cae al
 * interprete de vez en cuando y los dos tienen que ver los mismos bytes. */
void psxIcacheConfigure(void)
{
	if (Config.Cpu == CPU_INTERPRETER) {
		s_icache_dyn = 0;
		s_icache_on  = Config.IcacheEmulation ? 1 : 0;
	} else {
		s_icache_dyn = Config.IcacheDynarec ? 1 : 0;
		s_icache_on  = s_icache_dyn;
	}
	s_icdiv_n = 0;
	s_icdiv_pc = 0;
	s_icdiv_cached = 0;
	s_icdiv_ram = 0;
	s_icdiv_next = 1;
	psxIcacheClear();
	pcsxr_log(1, "[ICACHE] interprete=%s dynarec=%s\n",
		s_icache_on ? "si" : "no", s_icache_dyn ? "si" : "no");
}

/* Fetch de instruccion DEL COMPILADOR del dynarec (ppc/pR3000A.c).  Con la
 * opcion apagada se comporta exactamente como el codigo de antes. */
u32 psxIcacheFetchCompile(u32 pc)
{
	struct pcsxr_icache_line *line;
	const u32 *code;
	u32 got, ram;

	if (!s_icache_dyn || pc >= 0xa0000000u) {
		code = (const u32 *)PSXM_2(pc & ~3u);
		return code ? SWAP32(*code) : 0u;
	}

	line = &s_icache[(pc & 0xff0u) >> 4];
	if (((line->tag ^ pc) & 0xfffffff0u) != 0 || pc < line->tag) {
		code = (const u32 *)PSXM_2(pc & ~0x0fu);
		if (code == NULL) {
			code = (const u32 *)PSXM_2(pc & ~3u);
			return code ? SWAP32(*code) : 0u;
		}
		line->tag = pc;
		/* CAIDA A PROPOSITO: relleno hacia delante hasta el fin de linea. */
		switch (pc & 0x0cu) {
		case 0x00: line->data[0] = SWAP32(code[0]);
		case 0x04: line->data[1] = SWAP32(code[1]);
		case 0x08: line->data[2] = SWAP32(code[2]);
		case 0x0c: line->data[3] = SWAP32(code[3]);
		}
		return line->data[(pc & 0x0fu) >> 2];
	}

	/* ACIERTO de linea: aqui es donde el modelo se separa de la RAM.  Se
	 * compara con la RAM SOLO para contarlo; lo que se devuelve es siempre el
	 * valor cacheado, que es lo que ejecutaria el hardware. */
	got  = line->data[(pc & 0x0fu) >> 2];
	code = (const u32 *)PSXM_2(pc & ~3u);
	ram  = code ? SWAP32(*code) : 0u;
	if (ram != got) {
		if (s_icdiv_n == 0u) {
			s_icdiv_pc     = pc;
			s_icdiv_cached = got;
			s_icdiv_ram    = ram;
		}
		s_icdiv_n++;
		if (s_icdiv_n >= s_icdiv_next) {
			pcsxr_log(1, "[ICDIV] n=%u pc=%08X cache=%08X ram=%08X"
				" (1a: pc=%08X cache=%08X ram=%08X)\n",
				(unsigned)s_icdiv_n, (unsigned)pc,
				(unsigned)got, (unsigned)ram,
				(unsigned)s_icdiv_pc, (unsigned)s_icdiv_cached,
				(unsigned)s_icdiv_ram);
			s_icdiv_next = s_icdiv_n * 100u;
		}
	}
	return got;
}

static u32 diagFetchUncached(u32 pc);

static u32 diagFetchCached(u32 pc)
{
	struct pcsxr_icache_line *line = &s_icache[(pc & 0xff0u) >> 4];

	/* Fallo de linea, o salto HACIA ATRAS dentro de la linea: el R3000A
	 * rellena desde la palabra que falla hasta el final de la linea, asi
	 * que retroceder dentro de la misma linea obliga a recargarla. */
	if (((line->tag ^ pc) & 0xfffffff0u) != 0 || pc < line->tag) {
		const u32 *code = (const u32 *)PSXM_2(pc & ~0x0fu);
		if (code == NULL)
			return diagFetchUncached(pc);	/* que reporte el de siempre */
		line->tag = pc;
		/* CAIDA A PROPOSITO entre casos: rellena de la palabra que fallo
		 * hasta el final de la linea, como el relleno hacia delante del
		 * hardware.  NO poner breaks aqui. */
		switch (pc & 0x0cu) {
		case 0x00: line->data[0] = SWAP32(code[0]);
		case 0x04: line->data[1] = SWAP32(code[1]);
		case 0x08: line->data[2] = SWAP32(code[2]);
		case 0x0c: line->data[3] = SWAP32(code[3]);
		}
	}
	return line->data[(pc & 0x0fu) >> 2];
}

static u32 diagFetch(u32 pc)
{
	const u32 *code = (const u32 *)PSXM_2(pc & ~3u);

	if (code == NULL) {
		if (!s_fetch_crash_logged) {
			s_fetch_crash_logged = 1;
			pcsxr_log(1, "[CRASH] fetch sin mapear @%08X ra=%08X sp=%08X cyc=%u\n",
				(unsigned)pc, (unsigned)psxRegs.GPR.n.ra,
				(unsigned)psxRegs.GPR.n.sp, (unsigned)psxRegs.cycle);
		}
		return 0;	/* se ejecuta como NOP, igual que upstream */
	}
	/* KSEG1 (>= 0xA0000000, la BIOS de 0xBFC00000 incluida) no se cachea. */
	if (s_icache_on && pc < 0xa0000000u)
		return diagFetchCached(pc);
	return SWAP32(*code);
}

/* Camino sin cache, para cuando el relleno de linea se sale del mapa. */
static u32 diagFetchUncached(u32 pc)
{
	const u32 *code = (const u32 *)PSXM_2(pc & ~3u);
	if (code == NULL) {
		if (!s_fetch_crash_logged) {
			s_fetch_crash_logged = 1;
			pcsxr_log(1, "[CRASH] fetch sin mapear @%08X ra=%08X sp=%08X cyc=%u\n",
				(unsigned)pc, (unsigned)psxRegs.GPR.n.ra,
				(unsigned)psxRegs.GPR.n.sp, (unsigned)psxRegs.cycle);
		}
		return 0;
	}
	return SWAP32(*code);
}

/* Destino de jr/jalr: enmascara los 2 bits bajos (upstream doBranchReg) y
 * delata el PRIMER salto desalineado, que es donde empieza de verdad el
 * descarrilamiento (el fetch fallido ocurre decenas de instrucciones despues). */
static u32 diagBranchTarget(u32 tar)
{
	if (tar & 3u) {
		if (!s_jump_align_logged) {
			s_jump_align_logged = 1;
			pcsxr_log(1, "[CRASH] salto desalineado: jump@%08X -> %08X ra=%08X sp=%08X cyc=%u\n",
				(unsigned)(psxRegs.pc - 4), (unsigned)tar,
				(unsigned)psxRegs.GPR.n.ra,
				(unsigned)psxRegs.GPR.n.sp, (unsigned)psxRegs.cycle);
		}
	}
	return tar & ~3u;
}

#define execI(){ \
psxRegs.code = diagFetch(psxRegs.pc); \
psxRegs.pc += 4; \
psxRegs.cycle += BIAS; \
psxBSC[psxRegs.code >> 26](); \
}

// Subsets
void (*psxBSC[64])();
void (*psxSPC[64])();
void (*psxREG[32])();
void (*psxCP0[32])();
void (*psxCP2[64])();
void (*psxCP2BSC[32])();

u32 rold,rnew;

#define delayRead(reg, bpc){ \
rold = psxRegs.GPR.r[reg]; \
psxBSC[psxRegs.code >> 26](); \
rnew = psxRegs.GPR.r[reg]; \
psxRegs.pc = bpc; \
branch = 0; \
psxRegs.GPR.r[reg] = rold; \
execI(); \
psxRegs.GPR.r[reg] = rnew; \
psxBranchTest(); \
}

#define delayWrite(reg, bpc){ \
psxBSC[psxRegs.code >> 26](); \
branch = 0; \
psxRegs.pc = bpc; \
psxBranchTest(); \
}

#define delayReadWrite(reg,bpc){ \
branch = 0; \
psxRegs.pc = bpc; \
psxBranchTest(); \
}

// this defines shall be used with the tmp 
// of the next func (instead of _Funct_...)
#define _tFunct_  ((tmp      ) & 0x3F)  // The funct part of the instruction register 
#define _tRd_     ((tmp >> 11) & 0x1F)  // The rd part of the instruction register 
#define _tRt_     ((tmp >> 16) & 0x1F)  // The rt part of the instruction register 
#define _tRs_     ((tmp >> 21) & 0x1F)  // The rs part of the instruction register 
#define _tSa_     ((tmp >>  6) & 0x1F)  // The sa part of the instruction register

int psxTestLoadDelay(int reg, u32 tmp) {
	if (tmp == 0) return 0; // NOP
	switch (tmp >> 26) {
		case 0x00: // SPECIAL
			switch (_tFunct_) {
				case 0x00: // SLL
				case 0x02: case 0x03: // SRL/SRA
					if (_tRd_ == reg && _tRt_ == reg) return 1; else
					if (_tRt_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				case 0x08: // JR
					if (_tRs_ == reg) return 2;
					break;
				case 0x09: // JALR
					if (_tRd_ == reg && _tRs_ == reg) return 1; else
					if (_tRs_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				// SYSCALL/BREAK just a break;

				case 0x20: case 0x21: case 0x22: case 0x23:
				case 0x24: case 0x25: case 0x26: case 0x27: 
				case 0x2a: case 0x2b: // ADD/ADDU...
				case 0x04: case 0x06: case 0x07: // SLLV...
					if (_tRd_ == reg && (_tRt_ == reg || _tRs_ == reg)) return 1; else
					if (_tRt_ == reg || _tRs_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				case 0x10: case 0x12: // MFHI/MFLO
					if (_tRd_ == reg) return 3;
					break;
				case 0x11: case 0x13: // MTHI/MTLO
					if (_tRs_ == reg) return 2;
					break;

				case 0x18: case 0x19:
				case 0x1a: case 0x1b: // MULT/DIV...
					if (_tRt_ == reg || _tRs_ == reg) return 2;
					break;
			}
			break;

		case 0x01: // REGIMM
			switch (_tRt_) {
				case 0x00: case 0x01:
				case 0x10: case 0x11: // BLTZ/BGEZ...
					// Xenogears - lbu v0 / beq v0
					// - no load delay (fixes battle loading)
					break;

					if (_tRs_ == reg) return 2;
					break;
			}
			break;

		// J would be just a break;
		case 0x03: // JAL
			if (31 == reg) return 3;
			break;

		case 0x04: case 0x05: // BEQ/BNE
			// Xenogears - lbu v0 / beq v0
			// - no load delay (fixes battle loading)
			break;

			if (_tRs_ == reg || _tRt_ == reg) return 2;
			break;

		case 0x06: case 0x07: // BLEZ/BGTZ
			// Xenogears - lbu v0 / beq v0
			// - no load delay (fixes battle loading)
			break;

			if (_tRs_ == reg) return 2;
			break;

		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: // ADDI/ADDIU...
			if (_tRt_ == reg && _tRs_ == reg) return 1; else
			if (_tRs_ == reg) return 2; else
			if (_tRt_ == reg) return 3;
			break;

		case 0x0f: // LUI
			if (_tRt_ == reg) return 3;
			break;

		case 0x10: // COP0
			switch (_tFunct_) {
				case 0x00: // MFC0
					if (_tRt_ == reg) return 3;
					break;
				case 0x02: // CFC0
					if (_tRt_ == reg) return 3;
					break;
				case 0x04: // MTC0
					if (_tRt_ == reg) return 2;
					break;
				case 0x06: // CTC0
					if (_tRt_ == reg) return 2;
					break;
				// RFE just a break;
			}
			break;

		case 0x12: // COP2
			switch (_tFunct_) {
				case 0x00: 
					switch (_tRs_) {
						case 0x00: // MFC2
							if (_tRt_ == reg) return 3;
							break;
						case 0x02: // CFC2
							if (_tRt_ == reg) return 3;
							break;
						case 0x04: // MTC2
							if (_tRt_ == reg) return 2;
							break;
						case 0x06: // CTC2
							if (_tRt_ == reg) return 2;
							break;
					}
					break;
				// RTPS... break;
			}
			break;

		case 0x22: case 0x26: // LWL/LWR
			if (_tRt_ == reg) return 3; else
			if (_tRs_ == reg) return 2;
			break;

		case 0x20: case 0x21: case 0x23:
		case 0x24: case 0x25: // LB/LH/LW/LBU/LHU
			if (_tRt_ == reg && _tRs_ == reg) return 1; else
			if (_tRs_ == reg) return 2; else
			if (_tRt_ == reg) return 3;
			break;

		case 0x28: case 0x29: case 0x2a:
		case 0x2b: case 0x2e: // SB/SH/SWL/SW/SWR
			if (_tRt_ == reg || _tRs_ == reg) return 2;
			break;

		case 0x32: case 0x3a: // LWC2/SWC2
			if (_tRs_ == reg) return 2;
			break;
	}

	return 0;
}

__forceinline void psxDelayTest(int reg, u32 bpc) {
	u32 *code;
	u32 tmp;

	// Don't execute yet - just peek

	code = (u32 *)PSXM_2(bpc);

	tmp = SWAP32(*code);
	branch = 1;

	switch (psxTestLoadDelay(reg, tmp)) {

		case 1:
			delayReadWrite(reg, bpc); return;
		case 2:
			delayRead(reg, bpc); return;
		case 3:
			delayWrite(reg, bpc); return;
	}
	psxBSC[psxRegs.code >> 26]();

	branch = 0;
	psxRegs.pc = bpc;

    psxBranchTest();

}

static u32 psxBranchNoDelay(void) {
	u32 temp;

	psxRegs.code = diagFetch(psxRegs.pc);

	switch (_Op_) {
		case 0x00: // SPECIAL
			switch (_Funct_) {
				case 0x08: // JR
					return diagBranchTarget(_u32(_rRs_));
				case 0x09: // JALR
					temp = diagBranchTarget(_u32(_rRs_));
					if (_Rd_) { _SetLink(_Rd_); }
					return temp;
			}
			break;
		case 0x01: // REGIMM
			switch (_Rt_) {
				case 0x00: // BLTZ
					if (_i32(_rRs_) < 0)
						return _BranchTarget_;
					break;
				case 0x01: // BGEZ
					if (_i32(_rRs_) >= 0)
						return _BranchTarget_;
					break;
				case 0x08: // BLTZAL
					if (_i32(_rRs_) < 0) {
						_SetLink(31);
						return _BranchTarget_;
					}
					break;
				case 0x09: // BGEZAL
					if (_i32(_rRs_) >= 0) {
						_SetLink(31);
						return _BranchTarget_;
					}
					break;
			}
			break;
		case 0x02: // J
			return _JumpTarget_;
		case 0x03: // JAL
			_SetLink(31);
			return _JumpTarget_;
		case 0x04: // BEQ
			if (_i32(_rRs_) == _i32(_rRt_))
				return _BranchTarget_;
			break;
		case 0x05: // BNE
			if (_i32(_rRs_) != _i32(_rRt_))
				return _BranchTarget_;
			break;
		case 0x06: // BLEZ
			if (_i32(_rRs_) <= 0)
				return _BranchTarget_;
			break;
		case 0x07: // BGTZ
			if (_i32(_rRs_) > 0)
				return _BranchTarget_;
			break;
	}

	return (u32)-1;
}

static __forceinline int psxDelayBranchExec(u32 tar) {

	execI();

	branch = 0;
	psxRegs.pc = tar;
	psxRegs.cycle += BIAS;

    psxBranchTest();

	return 1;
}

static int psxDelayBranchTest(u32 tar1) {
	u32 tar2, tmp1, tmp2;

	tar2 = psxBranchNoDelay();
	if (tar2 == (u32)-1)
		return 0;

	debugI();

	/*
	 * Branch in delay slot:
	 * - execute 1 instruction at tar1
	 * - jump to tar2 (target of branch in delay slot; this branch
	 *   has no normal delay slot, instruction at tar1 was fetched instead)
	 */
	psxRegs.pc = tar1;
	tmp1 = psxBranchNoDelay();
	if (tmp1 == (u32)-1) {
		return psxDelayBranchExec(tar2);
	}
	debugI();
	psxRegs.cycle += BIAS;

	/*
	 * Got a branch at tar1:
	 * - execute 1 instruction at tar2
	 * - jump to target of that branch (tmp1)
	 */
	psxRegs.pc = tar2;
	tmp2 = psxBranchNoDelay();
	if (tmp2 == (u32)-1) {
		return psxDelayBranchExec(tmp1);
	}
	debugI();
	psxRegs.cycle += BIAS;

	/*
	 * Got a branch at tar2:
	 * - execute 1 instruction at tmp1
	 * - jump to target of that branch (tmp2)
	 */
	psxRegs.pc = tmp1;
	return psxDelayBranchExec(tmp2);
}

__inline void doBranch(u32 tar) {
	u32 tmp;

	branch2 = branch = 1;
	branchPC = tar;

	// notaz: check for branch in delay slot
	if (psxDelayBranchTest(tar))
		return;

	// branch delay slot
	psxRegs.code = diagFetch(psxRegs.pc);

	debugI();

	psxRegs.pc += 4;
	psxRegs.cycle += BIAS;

	// check for load delay
	tmp = psxRegs.code >> 26;
	switch (tmp) {
		case 0x10: // COP0
			switch (_Rs_) {
				case 0x00: // MFC0
				case 0x02: // CFC0
					psxDelayTest(_Rt_, branchPC);
					return;
			}
			break;
		case 0x12: // COP2
			switch (_Funct_) {
				case 0x00:
					switch (_Rs_) {
						case 0x00: // MFC2
						case 0x02: // CFC2
							psxDelayTest(_Rt_, branchPC);
							return;
					}
					break;
			}
			break;
		case 0x32: // LWC2
			psxDelayTest(_Rt_, branchPC);
			return;
		default:
			if (tmp >= 0x20 && tmp <= 0x26) { // LB/LH/LWL/LW/LBU/LHU/LWR
				psxDelayTest(_Rt_, branchPC);
				return;
			}
			break;
	}

	psxBSC[psxRegs.code >> 26]();

	branch = 0;
	psxRegs.pc = branchPC;

    psxBranchTest();

}

/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/
void psxADDI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }		// Rt = Rs + Im 	(Exception on Integer Overflow)
void psxADDIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }		// Rt = Rs + Im
void psxANDI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) & _ImmU_; }		// Rt = Rs And Im
void psxORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) | _ImmU_; }		// Rt = Rs Or  Im
void psxXORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) ^ _ImmU_; }		// Rt = Rs Xor Im
void psxSLTI() 	{ if (!_Rt_) return; _rRt_ = _i32(_rRs_) < _Imm_ ; }		// Rt = Rs < Im		(Signed)
void psxSLTIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) < ((u32)_Imm_); }		// Rt = Rs < Im		(Unsigned)

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
void psxADD()	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }	// Rd = Rs + Rt		(Exception on Integer Overflow)
void psxADDU() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }	// Rd = Rs + Rt
void psxSUB() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }	// Rd = Rs - Rt		(Exception on Integer Overflow)
void psxSUBU() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }	// Rd = Rs - Rt
void psxAND() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) & _u32(_rRt_); }	// Rd = Rs And Rt
void psxOR() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) | _u32(_rRt_); }	// Rd = Rs Or  Rt
void psxXOR() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) ^ _u32(_rRt_); }	// Rd = Rs Xor Rt
void psxNOR() 	{ if (!_Rd_) return; _rRd_ =~(_u32(_rRs_) | _u32(_rRt_)); }// Rd = Rs Nor Rt
void psxSLT() 	{ if (!_Rd_) return; _rRd_ = _i32(_rRs_) < _i32(_rRt_); }	// Rd = Rs < Rt		(Signed)
void psxSLTU() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) < _u32(_rRt_); }	// Rd = Rs < Rt		(Unsigned)

/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
void psxDIV() {
	const s32 Rt = _i32(_rRt_);
    const s32 Rs = _i32(_rRs_);

    if( Rt == 0 )
    {
		_i32(_rHi_) = Rs;
		_i32(_rLo_) = (Rs >= 0) ? -1 : 1;
		return;
    }
    if( Rs == 0x80000000 && Rt == 0xffffffff )
    {
		_i32(_rHi_) = 0;
		_i32(_rLo_) = Rs;
		return;
    }

    _i32(_rHi_) = Rs % Rt;
    _i32(_rLo_) = Rs / Rt;
}

void psxDIVU() {
    if( _rRt_ == 0 )
    {
		_rHi_ = _rRs_;
		_rLo_ = 0xffffffff;
		return;
    }

    _rHi_ = _rRs_ % _rRt_;
    _rLo_ = _rRs_ / _rRt_;
}

void psxMULT() {
	u64 res = (s64)((s64)_i32(_rRs_) * (s64)_i32(_rRt_));

	psxRegs.GPR.n.lo = (u32)(res & 0xffffffff);
	psxRegs.GPR.n.hi = (u32)((res >> 32) & 0xffffffff);
}

void psxMULTU() {
	u64 res = (u64)((u64)_u32(_rRs_) * (u64)_u32(_rRt_));

	psxRegs.GPR.n.lo = (u32)(res & 0xffffffff);
	psxRegs.GPR.n.hi = (u32)((res >> 32) & 0xffffffff);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/
#define RepZBranchi32(op)      if(_i32(_rRs_) op 0) doBranch(_BranchTarget_);
#define RepZBranchLinki32(op)  if(_i32(_rRs_) op 0) { _SetLink(31); doBranch(_BranchTarget_); }

void psxBGEZ()   { RepZBranchi32(>=) }      // Branch if Rs >= 0
void psxBGEZAL() { RepZBranchLinki32(>=) }  // Branch if Rs >= 0 and link
void psxBGTZ()   { RepZBranchi32(>) }       // Branch if Rs >  0
void psxBLEZ()   { RepZBranchi32(<=) }      // Branch if Rs <= 0
void psxBLTZ()   { RepZBranchi32(<) }       // Branch if Rs <  0
void psxBLTZAL() { RepZBranchLinki32(<) }   // Branch if Rs <  0 and link

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
void psxSLL() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) << _Sa_; } // Rd = Rt << sa
void psxSRA() { if (!_Rd_) return; _i32(_rRd_) = _i32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (arithmetic)
void psxSRL() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (logical)

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/
__inline u32 Shamt() {
	int shamt = (_u32(_rRs_) & 0x1f);
	if(shamt >= 0 && shamt < 32) return shamt;
	return 0;
}

void psxSLLV() { if (!_Rd_) return; _u32(_rRd_)  =  _u32(_rRt_)  << Shamt(); } // Rd = Rt << rs
void psxSRAV() { if (!_Rd_) return; _i32(_rRd_)  =  _i32(_rRt_)  >> Shamt(); } // Rd = Rt >> rs (arithmetic)
void psxSRLV() { if (!_Rd_) return; _u32(_rRd_)  =  _u32(_rRt_)  >> Shamt(); } // Rd = Rt >> rs (logical)

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
void psxLUI() { if (!_Rt_) return; _u32(_rRt_) = psxRegs.code << 16; } // Upper halfword of Rt = Im

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
void psxMFHI() { if (!_Rd_) return; _rRd_ = _rHi_; } // Rd = Hi
void psxMFLO() { if (!_Rd_) return; _rRd_ = _rLo_; } // Rd = Lo

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
void psxMTHI() { _rHi_ = _rRs_; } // Hi = Rs
void psxMTLO() { _rLo_ = _rRs_; } // Lo = Rs

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
void psxBREAK() {
	// Break exception - psx rom doens't handles this
}

void psxSYSCALL() {
	psxRegs.pc -= 4;
	psxException(0x20, branch);
}


void psxRFE() {
//	SysPrintf("psxRFE\n");
	psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status & 0xfffffff0) |
						  ((psxRegs.CP0.n.Status & 0x3c) >> 2);
	diag_sr_note('R', psxRegs.pc, psxRegs.CP0.n.Status);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#define RepBranchi32(op)      if(_i32(_rRs_) op _i32(_rRt_)) doBranch(_BranchTarget_);

void psxBEQ() {	RepBranchi32(==) }  // Branch if Rs == Rt
void psxBNE() {	RepBranchi32(!=) }  // Branch if Rs != Rt

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
void psxJ()   {               doBranch(_JumpTarget_); }
void psxJAL() {	_SetLink(31); doBranch(_JumpTarget_); }

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void psxJR()   {
	doBranch(diagBranchTarget(_u32(_rRs_)));

    psxJumpTest();

}

void psxJALR() {
	u32 temp = diagBranchTarget(_u32(_rRs_));
	if (_Rd_) { _SetLink(_Rd_); }
	doBranch(temp);
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

#define _oB_ (_u32(_rRs_) + _Imm_)

void psxLB() {
#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif

	if (_Rt_) {
	_i32(_rRt_) = (signed char)psxMemRead8_2(_oB_);
	} else {
	psxMemRead8_2(_oB_);
	}
}

void psxLBU() {
#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif

	if (_Rt_) {
	_u32(_rRt_) = psxMemRead8_2(_oB_);
	} else {
	psxMemRead8_2(_oB_);
	}
}

void psxLH() {
#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif

	if (_Rt_) {
	_i32(_rRt_) = (short)psxMemRead16_2(_oB_);
	} else {
	psxMemRead16_2(_oB_);
	}
}

void psxLHU() {
#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif

	if (_Rt_) {
	_u32(_rRt_) = psxMemRead16_2(_oB_);
	} else {
	psxMemRead16_2(_oB_);
	}
}

void psxLW() {
#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif

	if (_Rt_) {
	_u32(_rRt_) = psxMemRead32_2(_oB_);
	} else {
	psxMemRead32_2(_oB_);
	}
}

u32 LWL_MASK[4] = { 0xffffff, 0xffff, 0xff, 0 };
u32 LWL_SHIFT[4] = { 24, 16, 8, 0 };

void psxLWL() {
	u32 mem ;

	u32 addr = _oB_;
	u32 shift = addr & 3;

	mem = psxMemRead32_2(addr & ~3);

#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif

	if (!_Rt_) return;
	_u32(_rRt_) =	( _u32(_rRt_) & LWL_MASK[shift]) | 
					( mem << LWL_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   4bcd   (mem << 24) | (reg & 0x00ffffff)
	1   34cd   (mem << 16) | (reg & 0x0000ffff)
	2   234d   (mem <<  8) | (reg & 0x000000ff)
	3   1234   (mem      ) | (reg & 0x00000000)
	*/
}

u32 LWR_MASK[4] = { 0, 0xff000000, 0xffff0000, 0xffffff00 };
u32 LWR_SHIFT[4] = { 0, 8, 16, 24 };

void psxLWR() {
	u32 mem; 

	u32 addr = _oB_;
	u32 shift = addr & 3;

	mem = psxMemRead32_2(addr & ~3);

#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif

	if (!_Rt_) return;
	_u32(_rRt_) =	( _u32(_rRt_) & LWR_MASK[shift]) | 
					( mem >> LWR_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)
	*/
}

void psxSB() {
	u32 a = _oB_, v = _u8(_rRt_);
	psxMemWrite8_2(a, (u8)v);
}

void psxSH() {
	u32 a = _oB_, v = _u16(_rRt_);
	psxMemWrite16_2(a, (u16)v);
}

void psxSW() {
	u32 a = _oB_, v = _u32(_rRt_);
	diag_rdidx_note(a, v);
	psxMemWrite32_2(a, v);
}


u32 SWL_MASK[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0 };
u32 SWL_SHIFT[4] = { 24, 16, 8, 0 };

void psxSWL() {
	u32 mem;

	u32 addr = _oB_;
	u32 shift = addr & 3;

	mem = psxMemRead32_2(addr & ~3);

	psxMemWrite32_2(addr & ~3,  (_u32(_rRt_) >> SWL_SHIFT[shift]) |(  mem & SWL_MASK[shift]) );//}//teste
	/*
	Mem = 1234.  Reg = abcd

	0   123a   (reg >> 24) | (mem & 0xffffff00)
	1   12ab   (reg >> 16) | (mem & 0xffff0000)
	2   1abc   (reg >>  8) | (mem & 0xff000000)
	3   abcd   (reg      ) | (mem & 0x00000000)
	*/
}

u32 SWR_MASK[4] = { 0, 0xff, 0xffff, 0xffffff };
u32 SWR_SHIFT[4] = { 0, 8, 16, 24 };

void psxSWR() {
	u32 mem;

	u32 addr = _oB_;
	u32 shift = addr & 3;

	mem = psxMemRead32_2(addr & ~3);

	psxMemWrite32_2(addr & ~3,  (_u32(_rRt_) << SWR_SHIFT[shift]) | (  mem & SWR_MASK[shift]) );//}//teste

	/*
	Mem = 1234.  Reg = abcd

	0   abcd   (reg      ) | (mem & 0x00000000)
	1   bcd4   (reg <<  8) | (mem & 0x000000ff)
	2   cd34   (reg << 16) | (mem & 0x0000ffff)
	3   d234   (reg << 24) | (mem & 0x00ffffff)
	*/
}

/*********************************************************
* Moves between GPR and COPx                             *
* Format:  OP rt, fs                                     *
*********************************************************/
void psxMFC0()
{
#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif

	if (!_Rt_) return;
	
	_i32(_rRt_) = (int)_rFs_;
}

void psxCFC0()
{
#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif

	if (!_Rt_) return;
	
	_i32(_rRt_) = (int)_rFs_;
}

void psxTestSWInts() {
	// the next code is untested, if u know please
	// tell me if it works ok or not (linuzappz)
	if (psxRegs.CP0.n.Cause & psxRegs.CP0.n.Status & 0x0300 &&
		psxRegs.CP0.n.Status & 0x1) {
		psxException(psxRegs.CP0.n.Cause, branch);
	}
}

__inline void MTC0(int reg, u32 val) {
//	SysPrintf("MTC0 %d: %x\n", reg, val);
	switch (reg) {
		case 12: // Status
			psxRegs.CP0.r[12] = val;
			diag_sr_note('M', psxRegs.pc, val);
			psxTestSWInts();
			break;

		case 13: // Cause
			psxRegs.CP0.n.Cause = val & ~(0xfc00);
			psxTestSWInts();
			break;

		default:
			psxRegs.CP0.r[reg] = val;
			break;
	}
}

void psxMTC0() { MTC0(_Rd_, _u32(_rRt_)); }
void psxCTC0() { MTC0(_Rd_, _u32(_rRt_)); }



void psxMFC2()
{
 if(frontmission3fix)
{
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
 }
	gteMFC2();
}


void psxCFC2()
{
#if TEST_LOAD_DELAY
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc -= 4;
		doBranch( psxRegs.pc + 4 );

		return;
	}
#endif
	gteCFC2();
}


/*********************************************************
* Unknow instruction (would generate an exception)       *
* Format:  ?                                             *
*********************************************************/
void psxNULL() { 
#ifdef PSXCPU_LOG
	PSXCPU_LOG("psx: Unimplemented op %x\n", psxRegs.code);
#endif
}

void psxSPECIAL() {
	psxSPC[_Funct_]();
}

void psxREGIMM() {
	psxREG[_Rt_]();
}

void psxCOP0() {
	psxCP0[_Rs_]();
}

void psxCOP2() {
	if ((psxRegs.CP0.n.Status & 0x40000000) == 0 )
		return;

	psxCP2[_Funct_]();
}

void psxBASIC() {
	psxCP2BSC[_Rs_]();
}

/* Entradas realmente inicializadas de psxHLEt (psxhle.c): la tabla se
 * declara [256] pero solo tiene 8 punteros; el resto son NULL. */
#define PSXHLE_HANDLERS 8u

/* [XBOX360] Latch de un solo disparo para el opcode 0x3b con BIOS real.
 * Ejecutar 0x3b es la FIRMA de que la CPU se ha ido a interpretar datos:
 * el opcode esta reservado en MIPS I y aqui no hay ninguna trampa HLE
 * plantada (psxBiosInit_2 sale si !Config.HLE).  Antes esto caia en
 * hleBootstrap, que rearrancaba el juego desde el CD y pisaba el PC, con
 * lo que el descarrilamiento se convertia en un bucle de reboots y no
 * quedaba rastro de donde habia empezado.  Ahora se delata en el sitio. */
static int s_hle_trap_logged = 0;

void psxHLE() {
	u32 hleCode;
	/* [XBOX360] Alineado con upstream pcsx_rearmed (OP(psxHLE) en su
	 * psxinterpreter.c).  El opcode 0x3b esta RESERVADO en MIPS I;
	 * PCSX-R lo secuestra como puerta al HLE del BIOS.  Antes se hacia
	 * psxHLEt[code & 0x07] a pelo, sin mirar Config.HLE ni el rango, asi
	 * que CUALQUIER palabra basura con los 6 bits altos = 0x3b caia en
	 * uno de los 8 handlers.  El indice 4 es hleBootstrap, que rearranca
	 * el juego desde el CD y pisa el PC: un descarrilamiento de la CPU se
	 * convertia en un bucle infinito de reboots que ademas borraba la
	 * evidencia de donde se habia ido.  Con BIOS real (Config.HLE == 0)
	 * psxBiosInit_2 no planta ninguna trampa, asi que 0x3b NO debe
	 * disparar nada y se trata como opcode no implementado. */
	if (!Config.HLE) {
		if (!s_hle_trap_logged) {
			s_hle_trap_logged = 1;
			pcsxr_log(1, "[HLETRAP] opcode 0x3b con BIOS real:"
				" pc=%08X code=%08X ra=%08X sp=%08X cyc=%u\n",
				(unsigned)(psxRegs.pc - 4), (unsigned)psxRegs.code,
				(unsigned)psxRegs.GPR.n.ra, (unsigned)psxRegs.GPR.n.sp,
				(unsigned)psxRegs.cycle);
		}
		psxNULL();
		return;
	}
	hleCode = psxRegs.code & 0x03ffffffu;
	if (hleCode >= PSXHLE_HANDLERS || psxHLEt[hleCode] == NULL) {
		psxNULL();
		return;
	}
	psxHLEt[hleCode]();
}

void (*psxBSC[64])() = {
	psxSPECIAL, psxREGIMM, psxJ   , psxJAL  , psxBEQ , psxBNE , psxBLEZ, psxBGTZ,
	psxADDI   , psxADDIU , psxSLTI, psxSLTIU, psxANDI, psxORI , psxXORI, psxLUI ,
	psxCOP0   , psxNULL  , psxCOP2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL   , psxNULL  , psxNULL, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	psxLB     , psxLH    , psxLWL , psxLW   , psxLBU , psxLHU , psxLWR , psxNULL,
	psxSB     , psxSH    , psxSWL , psxSW   , psxNULL, psxNULL, psxSWR , psxNULL, 
	psxNULL   , psxNULL  , gteLWC2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL   , psxNULL  , gteSWC2, psxHLE  , psxNULL, psxNULL, psxNULL, psxNULL 
};


void (*psxSPC[64])() = {
	psxSLL , psxNULL , psxSRL , psxSRA , psxSLLV   , psxNULL , psxSRLV, psxSRAV,
	psxJR  , psxJALR , psxNULL, psxNULL, psxSYSCALL, psxBREAK, psxNULL, psxNULL,
	psxMFHI, psxMTHI , psxMFLO, psxMTLO, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxMULT, psxMULTU, psxDIV , psxDIVU, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxADD , psxADDU , psxSUB , psxSUBU, psxAND    , psxOR   , psxXOR , psxNOR ,
	psxNULL, psxNULL , psxSLT , psxSLTU, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL
};

void (*psxREG[32])() = {
	psxBLTZ  , psxBGEZ  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxBLTZAL, psxBGEZAL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};

void (*psxCP0[32])() = {
	psxMFC0, psxNULL, psxCFC0, psxNULL, psxMTC0, psxNULL, psxCTC0, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxRFE , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};

void (*psxCP2[64])() = {
	psxBASIC, gteRTPS , psxNULL , psxNULL, psxNULL, psxNULL , gteNCLIP, psxNULL, // 00
	psxNULL , psxNULL , psxNULL , psxNULL, gteOP  , psxNULL , psxNULL , psxNULL, // 08
	gteDPCS , gteINTPL, gteMVMVA, gteNCDS, gteCDP , psxNULL , gteNCDT , psxNULL, // 10
	psxNULL , psxNULL , psxNULL , gteNCCS, gteCC  , psxNULL , gteNCS  , psxNULL, // 18
	gteNCT  , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 20
	gteSQR  , gteDCPL , gteDPCT , psxNULL, psxNULL, gteAVSZ3, gteAVSZ4, psxNULL, // 28 
	gteRTPT , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 30
	psxNULL , psxNULL , psxNULL , psxNULL, psxNULL, gteGPF  , gteGPL  , gteNCCT  // 38
};

void (*psxCP2BSC[32])() = {
	psxMFC2, psxNULL, psxCFC2, psxNULL, gteMTC2, psxNULL, gteCTC2, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};

static int intInit() {

	Config.CpuRunning = 1;
    return 0;
}

static void intReset() {
	psxRegs.ICache_valid = FALSE;
	psxIcacheConfigure();
}

/* [JIT DEBUG] Sonda: ¿ejecuta el INTERPRETE el bloque a0017cc8 (donde el
 * recompilador salta a pc=1)? Compara si llegar ahi es correcto y con que
 * estado de registros (sp/ra). Poner PCSXR_INT_PROBE a 0 para quitarla.
 *   - Nunca loguea + juego avanza  => el interp NO pasa por 0x17cc8 => el
 *     recompilador DIVERGE antes (salto erroneo aguas arriba).
 *   - Loguea con sp valido         => ambos llegan, pero el interp tiene el
 *     estado bueno => corrupcion de registros aguas arriba en el recompilador.
 *   - Loguea con sp=0 (igual)      => ambos llegan igual => el bug es el
 *     CODEGEN del propio bloque (su JR terminal / su fuente). */
#ifndef PCSXR_INT_PROBE
/* 0 = desactivada.  Era la sonda del crash "pc=1" de Ace Combat 2: vigila
 * direcciones FIJAS de aquella investigacion, asi que en cualquier otro juego
 * solo produce coincidencias sin significado (p.ej. 0x174f8 en Formula One 99,
 * que despisto durante el diagnostico).  Poner a 1 solo para re-investigar
 * aquel caso concreto. */
#define PCSXR_INT_PROBE 0
#endif
#if PCSXR_INT_PROBE
static u32 _intLast = 0;
/* Direcciones de la cadena del recompilador (offset fisico en RAM), en orden
 * cronologico hacia el crash. Registramos la PRIMERA vez (cyc>150M) que el
 * interprete pisa cada una. Las que alcance vs las que no localizan el punto
 * exacto donde el recompilador se desvia del camino correcto. */
static const u32 _intWatch[9] = {
	0x10ec0, 0x10ec8, 0x10ed0, 0x10ed8,   /* pre-cadena */
	0x15a14, 0x161e4,                     /* -> syscall */
	0x16878, 0x174f8, 0x17cc8             /* retorno syscall -> data -> pc=1 */
};
static unsigned char _intSeen[9] = {0,0,0,0,0,0,0,0,0};
#endif

static void intExecute() {
	while (Config.CpuRunning && !frame_done) {
#if PCSXR_INT_PROBE
		{
			u32 _pc  = psxRegs.pc;
			u32 _seg = _pc & 0xff000000u;
			u32 _ph  = _pc & 0x1fffff;
			int _ram = (_seg == 0x00000000u || _seg == 0x80000000u || _seg == 0xa0000000u);
			if (_ram && psxRegs.cycle > 150000000u) {
				int _i;
				for (_i = 0; _i < 9; _i++) {
					if (_ph == _intWatch[_i] && !_intSeen[_i]) {
						_intSeen[_i] = 1;
						pcsxr_log(1, "[INT] visto %05x  pc=%08x <- %08x  cyc=%u sp=%08x ra=%08x\n",
							(unsigned)_intWatch[_i], (unsigned)_pc, (unsigned)_intLast,
							(unsigned)psxRegs.cycle,
							(unsigned)psxRegs.GPR.n.sp, (unsigned)psxRegs.GPR.n.ra);
					}
				}
			}
			_intLast = _pc;
		}
#endif
		execI();
	}
}

static void intExecuteBlock() {
	branch2 = 0;
	while (!branch2){
		execI();
	}
}

static void intClear(u32 Addr, u32 Size) {
}

static void intShutdown() {
}

R3000Acpu psxInt = {
	intInit,
	intReset,
	intExecute,
	intExecuteBlock,
	intClear,
	intShutdown
};