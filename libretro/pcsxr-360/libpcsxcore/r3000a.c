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
* R3000A CPU functions.
*/
#include "r3000a.h"
#include "cdrom.h"
#include "mdec.h"
#include "gpu.h"
#include "gte.h"
#include "sio.h"
#include "psxdma.h"
#include "spu.h"  /* spuDelayedIrq, spuUpdate (cycle-driven SPU events) */
#if PCSXR_DIAG_INSTRUMENTATION
#include <libretro.h>  /* RETRO_LOG_DEBUG / RETRO_LOG_INFO (solo diag) */
#endif

boolean use_vm;
// extern boolean use_vm on psxcommon.h

R3000Acpu *psxCpu = NULL;
psxRegisters psxRegs;

/* ===========================================================================
 * IRQ diagnostic counters (gated por PCSXR_DIAG_INSTRUMENTATION).
 *
 * Cuando esta ON, cada dispatch de IRQ del scheduler de events y cada
 * elevacion de bit en 0x1070 (HW IRQ via setIrq) incrementan contadores.
 * Periodicamente (cada ~3 segundos emulados) se vuelcan via pcsxr_log para
 * que el usuario pueda comparar dos ventanas: una en pantalla normal con
 * juego avanzando vs otra en pantalla negra. Las diferencias indican que
 * IRQ ha dejado de dispararse cuando el juego se queda esperando un evento.
 *
 * Tipos de tracking:
 *   - diag_evt_irq_count[PSXINT_*]: count de dispatches del scheduler
 *     interno (PSXINT_SIO, PSXINT_CDR, PSXINT_CDREAD, etc.). Estos son los
 *     eventos que se programan via set_event/CDREAD_INT/etc.
 *   - diag_hw_irq_set_count[bit]: count de elevaciones de bit en
 *     psxHu32(0x1070), donde bit 0 = VBLANK, 2 = CDROM, 3 = DMA,
 *     4..6 = Timers, 9 = SPU. Estos son los IRQs reales que el CPU PSX ve.
 *   - diag_psx_exception_count: numero de psxException(0x400) tomadas.
 *   - diag_psx_branch_test_calls: numero de psxBranchTest entries
 *     (proxy de cuanto trabajo de CPU se esta haciendo).
 *
 * Coste runtime con OFF: cero. El compilador elimina las macros DIAG_INC_*. */
#if PCSXR_DIAG_INSTRUMENTATION
volatile uint32_t diag_evt_irq_count[PSXINT_COUNT];
volatile uint32_t diag_hw_irq_set_count[11];   /* 11 bits relevantes de 0x1070 */
volatile uint32_t diag_psx_exception_count = 0;
volatile uint32_t diag_psx_branch_test_calls = 0;
static   uint32_t diag_last_dump_cycle = 0;
#define  DIAG_DUMP_INTERVAL_CYCLES (100000000u)   /* ~3 segundos emulados */

/* === Histograma de PC (diagnostico de cuelgues) ===
 * Si el BIOS o un juego se queda atascado en pantalla negra mientras la CPU
 * sigue ejecutando branches (sabido via diag_psx_branch_test_calls), saber
 * EN QUE direccion esta gastando los ciclos es la pista mas directa para
 * identificar el bucle.  Sampleamos psxRegs.pc cada N entradas a
 * psxBranchTest y acumulamos en una pequena hashtable open-addressing.
 * Al volcar, ordenamos por count y mostramos top-8.
 *
 * Coste con OFF: cero (todo dentro del #if).  Coste con ON: una division
 * cada psxBranchTest y, cuando toca samplear, una busqueda lineal en una
 * tabla de 16 entradas.  Despreciable.
 *
 * Granularidad: 1 sample cada 256 branch tests.  Con ~5M branch_tests/sec
 * eso son ~20K muestras/sec.  En la ventana de 3s del dump tenemos ~60K
 * muestras totales, mas que suficiente para que la moda emerja con claridad. */
#define DIAG_PC_HIST_SLOTS 16
#define DIAG_PC_SAMPLE_PERIOD 256u
typedef struct {
    uint32_t pc;
    uint32_t count;
} diag_pc_slot_t;
static diag_pc_slot_t diag_pc_hist[DIAG_PC_HIST_SLOTS];
static uint32_t diag_pc_samples_taken = 0;
static uint32_t diag_pc_samples_evicted = 0;  /* sample tirado por tabla llena */

/* === Contadores GPU port writes ===
 * Incrementan en gpuWriteData() / gpuWriteStatus() de gpu.c (declarados
 * extern alli).  Permiten ver si el BIOS llega o no a empezar a programar
 * el GPU.  Si `push+=0` en watchdog + estos contadores a 0 -> BIOS muy
 * temprano (todavia en init de hw).  Si los contadores > 0 + `push+=0` ->
 * BIOS escribe al GPU pero el ring DMA del thread emulador no progresa. */
volatile uint32_t diag_gpu_data_writes   = 0;
volatile uint32_t diag_gpu_status_writes = 0;
volatile uint32_t diag_gpu_first_write_seen = 0;  /* 0 hasta el primer write */

/* Forward declaration: pcsxr_log esta en libretro_core.cpp.
 * Mismo patron que en gpu.c y cdriso.c (declaracion local sin header). */
extern void pcsxr_log(int level, const char *format, ...);

static void diag_dump_irq_counts(uint32_t now_cycle) {
    /* pcsxr_log trunca cerca de 115 chars, asi que partimos en lineas. */
    pcsxr_log(RETRO_LOG_INFO,
        "[IRQ] hw vbl=%u gpu=%u cdr=%u dma=%u tmr0=%u\n",
        (unsigned)diag_hw_irq_set_count[0],
        (unsigned)diag_hw_irq_set_count[1],
        (unsigned)diag_hw_irq_set_count[2],
        (unsigned)diag_hw_irq_set_count[3],
        (unsigned)diag_hw_irq_set_count[4]);
    pcsxr_log(RETRO_LOG_INFO,
        "[IRQ] hw tmr1=%u tmr2=%u sio=%u spu=%u except=%u\n",
        (unsigned)diag_hw_irq_set_count[5],
        (unsigned)diag_hw_irq_set_count[6],
        (unsigned)diag_hw_irq_set_count[7],
        (unsigned)diag_hw_irq_set_count[9],
        (unsigned)diag_psx_exception_count);
    pcsxr_log(RETRO_LOG_INFO,
        "[IRQ] evt cdr=%u cdread=%u gpudma=%u mdecout=%u spudma=%u\n",
        (unsigned)diag_evt_irq_count[PSXINT_CDR],
        (unsigned)diag_evt_irq_count[PSXINT_CDREAD],
        (unsigned)diag_evt_irq_count[PSXINT_GPUDMA],
        (unsigned)diag_evt_irq_count[PSXINT_MDECOUTDMA],
        (unsigned)diag_evt_irq_count[PSXINT_SPUDMA]);
    pcsxr_log(RETRO_LOG_INFO,
        "[IRQ] evt mdecin=%u cdrplay=%u cdrdbuf=%u cdrlid=%u cdrdma=%u\n",
        (unsigned)diag_evt_irq_count[PSXINT_MDECINDMA],
        (unsigned)diag_evt_irq_count[PSXINT_CDRPLAY],
        (unsigned)diag_evt_irq_count[PSXINT_CDRDBUF],
        (unsigned)diag_evt_irq_count[PSXINT_CDRLID],
        (unsigned)diag_evt_irq_count[PSXINT_CDRDMA]);
    pcsxr_log(RETRO_LOG_INFO,
        "[IRQ] evt sio=%u spu_irq=%u spu_upd=%u\n",
        (unsigned)diag_evt_irq_count[PSXINT_SIO],
        (unsigned)diag_evt_irq_count[PSXINT_SPU_IRQ],
        (unsigned)diag_evt_irq_count[PSXINT_SPU_UPDATE]);
    pcsxr_log(RETRO_LOG_INFO,
        "[IRQ] branch_tests=%u pc=0x%08X cyc=%u\n",
        (unsigned)diag_psx_branch_test_calls,
        (unsigned)psxRegs.pc,
        (unsigned)now_cycle);

    /* GPU write activity desde el ultimo dump.  Si los counts son 0
     * mientras branch_tests > 0, la CPU corre pero no toca el GPU
     * (probablemente atascada en bucle BIOS antes de la fase video). */
    pcsxr_log(RETRO_LOG_DEBUG,
        "[GPU-IO] data_writes=%u status_writes=%u first_seen=%u\n",
        (unsigned)diag_gpu_data_writes,
        (unsigned)diag_gpu_status_writes,
        (unsigned)diag_gpu_first_write_seen);

    /* Top-8 PCs del histograma.  Ordenado por count descendente con un
     * selection-sort in-place (la tabla es pequena, sobra).  Logueamos
     * solo los slots con count > 0 para no spamear. */
    {
        int i, j;
        uint32_t total = 0;
        for (i = 0; i < DIAG_PC_HIST_SLOTS; i++) total += diag_pc_hist[i].count;
        if (total > 0) {
            pcsxr_log(RETRO_LOG_DEBUG,
                "[PC-HIST] samples=%u evicted=%u (top by count):\n",
                (unsigned)diag_pc_samples_taken,
                (unsigned)diag_pc_samples_evicted);
            for (i = 0; i < 8 && i < DIAG_PC_HIST_SLOTS; i++) {
                int max_idx = i;
                for (j = i + 1; j < DIAG_PC_HIST_SLOTS; j++) {
                    if (diag_pc_hist[j].count > diag_pc_hist[max_idx].count)
                        max_idx = j;
                }
                if (max_idx != i) {
                    diag_pc_slot_t tmp = diag_pc_hist[i];
                    diag_pc_hist[i] = diag_pc_hist[max_idx];
                    diag_pc_hist[max_idx] = tmp;
                }
                if (diag_pc_hist[i].count == 0) break;
                pcsxr_log(RETRO_LOG_DEBUG,
                    "[PC-HIST]   pc=0x%08X count=%u (%u%%)\n",
                    (unsigned)diag_pc_hist[i].pc,
                    (unsigned)diag_pc_hist[i].count,
                    total ? (unsigned)((diag_pc_hist[i].count * 100u) / total) : 0u);
            }
        }
    }

    /* Reset todos los contadores - cada dump es un DELTA respecto al previo,
     * no un total acumulado. Asi cada ventana de ~3s es independiente. */
    {
        int i;
        for (i = 0; i < PSXINT_COUNT; i++) diag_evt_irq_count[i] = 0;
        for (i = 0; i < 11; i++) diag_hw_irq_set_count[i] = 0;
        for (i = 0; i < DIAG_PC_HIST_SLOTS; i++) {
            diag_pc_hist[i].pc    = 0;
            diag_pc_hist[i].count = 0;
        }
    }
    diag_psx_exception_count   = 0;
    diag_psx_branch_test_calls = 0;
    diag_pc_samples_taken      = 0;
    diag_pc_samples_evicted    = 0;
    diag_gpu_data_writes       = 0;
    diag_gpu_status_writes     = 0;
    /* diag_gpu_first_write_seen NO se resetea: es un latch global,
     * queremos ver "ya vimos GPU activity alguna vez" durante toda la
     * sesion para saber si el cuelgue es PRE-init-GPU o POST. */
}

/* Llamada por psxBranchTest cada DIAG_PC_SAMPLE_PERIOD entradas.
 * Inserta psxRegs.pc en el histograma; si el slot existe incrementa,
 * si no busca slot vacio, si tampoco hay incrementa diag_pc_samples_evicted. */
static void diag_pc_sample(uint32_t pc) {
    int i, free_slot = -1;
    diag_pc_samples_taken++;
    for (i = 0; i < DIAG_PC_HIST_SLOTS; i++) {
        if (diag_pc_hist[i].count != 0 && diag_pc_hist[i].pc == pc) {
            diag_pc_hist[i].count++;
            return;
        }
        if (diag_pc_hist[i].count == 0 && free_slot < 0)
            free_slot = i;
    }
    if (free_slot >= 0) {
        diag_pc_hist[free_slot].pc    = pc;
        diag_pc_hist[free_slot].count = 1;
    } else {
        /* Tabla llena con 16 PCs distintos -> el codigo "anda" mucho
         * (probablemente NO esta atascado en bucle).  Solo contabilizamos
         * el sample tirado.  Si esto domina sobre samples_taken, el
         * histograma no nos dira gran cosa y habra que recurrir a otra
         * tecnica de diagnostico. */
        diag_pc_samples_evicted++;
    }
}
#endif


int psxInit() {

	SysPrintf(_("Running PCSX Version %s (%s).\n"), PACKAGE_VERSION, __DATE__);

#ifdef PSXREC
	if (Config.Cpu == CPU_INTERPRETER) {
		psxCpu = &psxInt;
	} else psxCpu = &psxRec;
#else
	psxCpu = &psxInt;
#endif

	Log = 0;
//if (use_vm)
//{
//	if (psxMemInit() == -1) return -1;
//}
//else
//{
	if (psxMemInit_2() == -1) return -1;//teste
//}

	return psxCpu->Init();
}

void psxReset() {
	psxCpu->Reset();
//if (use_vm)
//{
//	psxMemReset();
//}
//else
//{
	psxMemReset_2();//teste
//}

	memset(&psxRegs, 0, sizeof(psxRegs));

	psxRegs.pc = 0xbfc00000; // Start in bootstrap
	psxRegs.next_interupt = 0;

	psxRegs.CP0.r[12] = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	psxRegs.CP0.r[15] = 0x00000002; // PRevID = Revision ID, same as R3000A

	psxHwReset();
//if(use_vm){
//	psxBiosInit();}
//else{
	psxBiosInit_2();//}//teste

	if (!Config.HLE)
		psxExecuteBios();

#ifdef EMU_LOG
	EMU_LOG("*BIOS END*\n");
#endif
	Log = 0;
}

void psxShutdown() {

//if(use_vm){
//	psxMemShutdown();}
//else{
	psxMemShutdown_2();//}//teste

	psxBiosShutdown();

	psxCpu->Shutdown();
}

void psxException(u32 code, u32 bd) {
	// Set the Cause
	psxRegs.CP0.n.Cause = code;

	// Set the EPC & PC
	if (bd) {
#ifdef PSXCPU_LOG
		PSXCPU_LOG("bd set!!!\n");
#endif
		SysPrintf("bd set!!!\n");
		psxRegs.CP0.n.Cause |= 0x80000000;
		psxRegs.CP0.n.EPC = (psxRegs.pc - 4);
	} else
		psxRegs.CP0.n.EPC = (psxRegs.pc);

	if (psxRegs.CP0.n.Status & 0x400000)
		psxRegs.pc = 0xbfc00180;
	else
		psxRegs.pc = 0x80000080;

	// Set the Status
	psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status &~0x3f) |
						  ((psxRegs.CP0.n.Status & 0xf) << 2);

	if (Config.HLE) psxBiosException();
}

void schedule_timeslice(void) {
	u32 i, c = psxRegs.cycle;
	u32 irqs = psxRegs.interrupt;
	s32 min, dif;

	// Start with next counter event
	min = (s32)(psxNextsCounter + psxNextCounter - c);
	if (min < 0) min = 0;

	for (i = 0; irqs != 0; i++, irqs >>= 1) {
		if (!(irqs & 1))
			continue;
		dif = (s32)(psxRegs.intCycle[i].sCycle + psxRegs.intCycle[i].cycle - c);
		if (dif < min) {
			if (dif < 0) { min = 0; break; }
			min = dif;
		}
	}
	psxRegs.next_interupt = c + min;
}

void psxBranchTest() {
#if PCSXR_DIAG_INSTRUMENTATION
	diag_psx_branch_test_calls++;
	/* Sampleo periodico del PC para el histograma.  No hace falta proteger
	 * con CritSec porque psxBranchTest siempre corre en el thread emu. */
	if ((diag_psx_branch_test_calls & (DIAG_PC_SAMPLE_PERIOD - 1u)) == 0u) {
		diag_pc_sample(psxRegs.pc);
	}
#endif
	// Event processing gated by next_interupt (fast path optimization)
	if ((s32)(psxRegs.cycle - psxRegs.next_interupt) >= 0) {
		if ((psxRegs.cycle - psxNextsCounter) >= psxNextCounter)
			psxRcntUpdate();

		if (psxRegs.interrupt) {
			u32 irq, irq_bits;
			for (irq = 0, irq_bits = psxRegs.interrupt; irq_bits != 0; irq++, irq_bits >>= 1) {
				if (!(irq_bits & 1))
					continue;
				if ((psxRegs.cycle - psxRegs.intCycle[irq].sCycle) >= psxRegs.intCycle[irq].cycle) {
					psxRegs.interrupt &= ~(1u << irq);
					/* Marcar que estamos en este IRQ handler para que el
					 * GPU watchdog pueda identificar cuelgues aqui (ver
					 * gpu.h, PCSXR_DIAG_INSTRUMENTATION).  Cuando esa
					 * macro esta off, DIAG_SET_IRQ_HANDLER expande a
					 * (void)0 sin overhead. */
					DIAG_SET_IRQ_HANDLER((int)irq);
#if PCSXR_DIAG_INSTRUMENTATION
					/* Contador por tipo de IRQ (PSXINT_*) para el dump
					 * periodico. Permite ver si algun event ha dejado de
					 * dispararse cuando el juego se queda esperando. */
					if (irq < PSXINT_COUNT) diag_evt_irq_count[irq]++;
#endif
					switch (irq) {
						case PSXINT_SIO: if (!Config.Sio) sioInterrupt(); break;
						case PSXINT_CDR: cdrInterrupt(); break;
						case PSXINT_CDREAD: cdrReadInterrupt(); break;
						case PSXINT_GPUDMA: gpuInterrupt(); break;
						case PSXINT_MDECOUTDMA: mdec1Interrupt(); break;
						case PSXINT_SPUDMA: spuInterrupt(); break;
						case PSXINT_MDECINDMA: mdec0Interrupt(); break;
						case PSXINT_GPUOTCDMA: gpuotcInterrupt(); break;
						case PSXINT_CDRDMA: cdrDmaInterrupt(); break;
						case PSXINT_CDRPLAY: cdrPlayInterrupt(); break;
						case PSXINT_CDRDBUF: cdrDecodedBufferInterrupt(); break;
						case PSXINT_CDRLID: cdrLidSeekInterrupt(); break;
						/* Cycle-driven SPU IRQ events (port from pcsx_rearmed) */
						case PSXINT_SPU_IRQ: spuDelayedIrq(); break;
						case PSXINT_SPU_UPDATE: spuUpdate(); break;
					}
					DIAG_SET_IRQ_HANDLER(PSX_IRQ_NONE);
				}
			}
		}

		schedule_timeslice();
	}

	// Hardware interrupt check - ALWAYS runs (events above may set 0x1070)
	if (psxHu32_2(0x1070) & psxHu32_2(0x1074)) {
		if ((psxRegs.CP0.n.Status & 0x401) == 0x401) {
			u32 opcode;
			u32 *code;
			code = (u32 *)PSXM_2(psxRegs.pc);
			// Crash Bandicoot 2: Don't run exceptions when GTE in pipeline
			opcode = SWAP32(*code);
			if (((opcode >> 24) & 0xfe) != 0x4a) {
#ifdef PSXCPU_LOG
				PSXCPU_LOG("Interrupt: %x %x\n", psxHu32_2(0x1070), psxHu32_2(0x1074));
#endif
#if PCSXR_DIAG_INSTRUMENTATION
				diag_psx_exception_count++;
#endif
				psxException(0x400, 0);
			}
		}
	}

#if PCSXR_DIAG_INSTRUMENTATION
	/* Periodic IRQ counter dump. Threshold por cycle delta para que dispare
	 * regular incluso si el ring del GPU esta lleno (watchdog principal solo
	 * dispara con ring vacio). 100M cycles = ~3 segundos emulados a 33 MHz. */
	{
		uint32_t cur = psxRegs.cycle;
		if ((cur - diag_last_dump_cycle) >= DIAG_DUMP_INTERVAL_CYCLES) {
			diag_dump_irq_counts(cur);
			diag_last_dump_cycle = cur;
		}
	}
#endif
}

void psxJumpTest() {
	if (!Config.HLE && Config.PsxOut) {
		u32 call = psxRegs.GPR.n.t1 & 0xff;
		switch (psxRegs.pc & 0x1fffff) {
			case 0xa0:
#ifdef PSXBIOS_LOG
				if (call != 0x28 && call != 0xe) {
					PSXBIOS_LOG("Bios call a0: %s (%x) %x,%x,%x,%x\n", biosA0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3); }
#endif
				if (biosA0[call])
					biosA0[call]();
				break;
			case 0xb0:
#ifdef PSXBIOS_LOG
				if (call != 0x17 && call != 0xb) {
					PSXBIOS_LOG("Bios call b0: %s (%x) %x,%x,%x,%x\n", biosB0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3); }
#endif
				if (biosB0[call])
					biosB0[call]();
				break;
			case 0xc0:
#ifdef PSXBIOS_LOG
				PSXBIOS_LOG("Bios call c0: %s (%x) %x,%x,%x,%x\n", biosC0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3);
#endif
				if (biosC0[call])
					biosC0[call]();
				break;
		}
	}
}

void psxExecuteBios() {
	while (psxRegs.pc != 0x80030000)
		psxCpu->ExecuteBlock();
}
