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
/* [XBOX360] Anillo de transiciones de CP0.Status.  El sintoma de F1'99 es
 * que el hilo principal corre con IEc=0 (bit 0) y por tanto ninguna IRQ se
 * entrega: espera por siempre una variable que solo actualiza un handler.
 * psxException / psxRFE / MTC0 son los UNICOS que tocan Status, asi que con
 * este anillo (quien, desde que PC, y el valor resultante) se identifica al
 * responsable de dejar IEc a 0.  src: E=exception R=rfe M=mtc0. */
volatile u32  diag_sr_ring_pc[8];
volatile u32  diag_sr_ring_val[8];
volatile char diag_sr_ring_src[8];
volatile u32  diag_sr_ring_idx = 0;
/* [XBOX360] CP0.Cause y el opcode EN el EPC de cada evento.  Cause es lo que
 * separa las tres hipotesis del cuelgue de F1'99: 0x00=Interrupt (handler
 * colgado), 0x20=Syscall (funcion del BIOS girando), y cualquier otra
 * (0x10=CpU, 0x24/0x28=AdEL/AdES, 0x0A=instruccion reservada) = fallo
 * ESPURIO por divergencia de emulacion, que es la unica que explica que
 * interprete y dynarec fallen igual.  El opcode identifica la instruccion. */
volatile u32  diag_sr_ring_cause[8];
volatile u32  diag_sr_ring_op[8];
/* Histograma ACUMULADO de codigos de excepcion (ExcCode de Cause).
 * El anillo de 8 puede perder el fallo si hubo trafico despues; un
 * contador no.  Indices MIPS-I: 0=Int 4=AdEL 5=AdES 6=IBE 7=DBE
 * 8=Sys 9=Bp 10=RI 11=CpU 12=Ovf.  Cualquier cosa que no sea 0 u 8
 * en este juego = fallo espurio de emulacion. */
volatile u32  diag_exc_count[16];
volatile u32  diag_exc_first_pc[16];

#endif

/* pcsxr_log vive en libretro_core.cpp; se declara aqui porque las sondas de
 * mas abajo lo usan MUCHO antes de la declaracion que hay junto al volcado
 * periodico (mismo patron que gpu.c / cdriso.c: decl local sin header). */
extern void pcsxr_log(int level, const char *format, ...);

static int s_drain_dumped = 0;

#if PCSXR_DIAG_INSTRUMENTATION
/* Watchpoint de escritura sobre el indice de LECTURA de la cola de libgpu
 * (0x8001A11C = gp+0x6C).  El consumidor avanza a la MITAD del ritmo del
 * productor (~1 vs ~2 por frame), asi que lo que hace falta saber es QUIEN
 * lo avanza y cada cuanto: si todas las escrituras vienen del handler de
 * VBlank, el consumidor esta gobernado por VBlank en vez de por la
 * complecion de la DMA, y ahi esta el factor 2 exacto. */
#define DIAG_RDIDX_ADDR 0x8001A11Cu
volatile u32 diag_rdidx_pc[16];
volatile u32 diag_rdidx_val[16];
volatile u32 diag_rdidx_cyc[16];
volatile u32 diag_rdidx_idx = 0;

void diag_rdidx_note(u32 addr, u32 val)
{
	u32 i;
	if (addr != DIAG_RDIDX_ADDR)
		return;
	i = diag_rdidx_idx & 15u;
	diag_rdidx_pc[i]  = psxRegs.pc;
	diag_rdidx_val[i] = val;
	diag_rdidx_cyc[i] = psxRegs.cycle;
	diag_rdidx_idx++;
}

void diag_sr_note(char src, u32 pc, u32 val)
{
	u32 i = diag_sr_ring_idx & 7u;
	const u32 *op;
	u32 e;
	diag_sr_ring_pc[i]  = pc;
	diag_sr_ring_val[i] = val;
	diag_sr_ring_src[i] = src;
	diag_sr_ring_cause[i] = psxRegs.CP0.n.Cause;
	op = (const u32 *)PSXM_2(pc);
	diag_sr_ring_op[i] = op ? SWAP32(*op) : 0xFFFFFFFFu;
	diag_sr_ring_idx++;
	if (src == 'E') {  /* solo excepciones reales */
		e = (psxRegs.CP0.n.Cause >> 2) & 0xfu;
		if (diag_exc_count[e] == 0)
			diag_exc_first_pc[e] = pc;
		diag_exc_count[e]++;
	}
}

#else
#define diag_rdidx_note(a, v)      ((void)0)
#define diag_sr_note(s, p, v)      ((void)0)
#endif

/* El diferimiento de la IRQ por opcode GTE en el pipeline (el viejo hack de
 * "Crash Bandicoot 2") se ELIMINO: era una aproximacion con umbral que
 * podia bloquear la IRQ para siempre.  El mecanismo real -- ejecutar el
 * opcode GTE al entrar en la excepcion, porque el BIOS lo salta al volver --
 * esta ahora en psxException(), igual que upstream. */
extern void (*psxCP2[64])();          /* tabla GTE del interprete */
extern u32 psxHwUpdateCauseIp(void);  /* psxhw.c: Cause bit 10 <- linea HW */

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
    /* [XBOX360] Los tres terminos que deciden si se entrega la IRQ (ver
     * psxBranchTest).  Cuando `except` se queda a 0 mientras las IRQs se
     * siguen levantando, esto dice CUAL de ellos falla:
     *   stat = I_STAT (0x1070), bits pendientes
     *   mask = I_MASK (0x1074), bits habilitados
     *   pend = stat & mask -> si es 0, el juego tiene la IRQ enmascarada
     *   sr   = CP0.Status; hace falta (sr & 0x401) == 0x401 */
    pcsxr_log(RETRO_LOG_INFO,
        "[IRQ] stat=%08X mask=%08X pend=%08X sr=%08X sr_ok=%d\n",
        (unsigned)psxHu32_2(0x1070), (unsigned)psxHu32_2(0x1074),
        (unsigned)(psxHu32_2(0x1070) & psxHu32_2(0x1074)),
        (unsigned)psxRegs.CP0.n.Status,
        (int)((psxRegs.CP0.n.Status & 0x401) == 0x401));
    /* [XBOX360] Ultimas 8 direcciones de registro HW leidas (0x1f80xxxx) y
     * estado del controlador de CD.  Con el juego colgado dentro de su
     * handler de IRQ (IEc=0), esto dice QUE registro sondea el bucle y si el
     * CD se quedo en un estado del que no sale. */
    {
        extern volatile u32 diag_hwread_ring[8];
        extern volatile u32 diag_hwread_idx;
        extern volatile u32 diag_hwwrite_ring[8];
        extern void cdrDiagDump(void);
        pcsxr_log(RETRO_LOG_INFO,
            "[HWRD] %04X %04X %04X %04X %04X %04X %04X %04X\n",
            (unsigned)diag_hwread_ring[0], (unsigned)diag_hwread_ring[1],
            (unsigned)diag_hwread_ring[2], (unsigned)diag_hwread_ring[3],
            (unsigned)diag_hwread_ring[4], (unsigned)diag_hwread_ring[5],
            (unsigned)diag_hwread_ring[6], (unsigned)diag_hwread_ring[7]);
        pcsxr_log(RETRO_LOG_INFO,
            "[HWWR] %08X %08X %08X %08X %08X %08X %08X %08X\n",
            (unsigned)diag_hwwrite_ring[0], (unsigned)diag_hwwrite_ring[1],
            (unsigned)diag_hwwrite_ring[2], (unsigned)diag_hwwrite_ring[3],
            (unsigned)diag_hwwrite_ring[4], (unsigned)diag_hwwrite_ring[5],
            (unsigned)diag_hwwrite_ring[6], (unsigned)diag_hwwrite_ring[7]);
        /* [XBOX360] Volcado de las instrucciones MIPS alrededor del PC.
         * Con el juego girando en un bucle cerrado, esto permite DECODIFICAR
         * el bucle y saber exactamente que condicion espera, en vez de
         * inferirlo.  8 words centrados en pc. */
        {
            u32 base = (psxRegs.pc - 16) & ~3u;
            u32 *ip  = (u32 *)PSXM_2(base);
            if (ip != NULL) {
                pcsxr_log(RETRO_LOG_INFO,
                    "[DIS] @%08X: %08X %08X %08X %08X\n",
                    (unsigned)base, (unsigned)SWAP32(ip[0]),
                    (unsigned)SWAP32(ip[1]), (unsigned)SWAP32(ip[2]),
                    (unsigned)SWAP32(ip[3]));
                pcsxr_log(RETRO_LOG_INFO,
                    "[DIS] @%08X: %08X %08X %08X %08X  (pc=%08X)\n",
                    (unsigned)(base + 16), (unsigned)SWAP32(ip[4]),
                    (unsigned)SWAP32(ip[5]), (unsigned)SWAP32(ip[6]),
                    (unsigned)SWAP32(ip[7]), (unsigned)psxRegs.pc);
            }
        }
        {
            u32 k;
            /* idx: imprescindible para reconstruir el orden cronologico.
             * El ultimo evento escrito es el slot (idx-1)&7. */
            pcsxr_log(RETRO_LOG_INFO, "[SR] idx=%u last_slot=%u\n",
                (unsigned)diag_sr_ring_idx,
                (unsigned)((diag_sr_ring_idx - 1u) & 7u));
            for (k = 0; k < 8; k++) {
                pcsxr_log(RETRO_LOG_INFO,
                    "[SR%u] %c pc=%08X sr=%08X iec=%u cause=%08X exc=%02X op=%08X\n",
                    (unsigned)k, diag_sr_ring_src[k] ? diag_sr_ring_src[k] : 63,
                    (unsigned)diag_sr_ring_pc[k], (unsigned)diag_sr_ring_val[k],
                    (unsigned)(diag_sr_ring_val[k] & 1u),
                    (unsigned)diag_sr_ring_cause[k],
                    (unsigned)((diag_sr_ring_cause[k] >> 2) & 0x1fu),
                    (unsigned)diag_sr_ring_op[k]);
            }
        }
        {
            extern volatile u32  diag_dicr_pc[16];
            extern volatile u32  diag_dicr_val[16];
            extern volatile u32  diag_dicr_after[16];
            extern volatile char diag_dicr_addr[16];
            extern volatile char diag_dicr_w[16];
            extern volatile u32  diag_dicr_idx;
            u32 n;
            pcsxr_log(RETRO_LOG_INFO, "[DICR] idx=%u last_slot=%u\n",
                (unsigned)diag_dicr_idx,
                (unsigned)((diag_dicr_idx - 1u) & 15u));
            for (n = 0; n < 16; n++)
                pcsxr_log(RETRO_LOG_INFO,
                    "[DICR%02u] %c%02X val=%08X -> %08X en2=%u pc=%08X\n",
                    (unsigned)n, diag_dicr_w[n] ? diag_dicr_w[n] : 63,
                    (unsigned)(unsigned char)diag_dicr_addr[n],
                    (unsigned)diag_dicr_val[n],
                    (unsigned)diag_dicr_after[n],
                    (unsigned)((diag_dicr_after[n] >> 18) & 1u),
                    (unsigned)diag_dicr_pc[n]);
        }
        /* Cabecera del bucle de drenado de la cola de callbacks del BIOS.
         * El cuerpo (0x800176B0-0x800176E4) saca {funcion, arg} de las tablas
         * 0x8001B590/0x8001B598, hace jalr, avanza el indice de lectura y
         * salta ATRAS a 0x800175C8: es un while que vacia la cola.  Medido:
         * solo da UNA vuelta por VBlank, asi que la condicion de salida esta
         * en 0x800175C8 y es lo que hay que leer.  One-shot: 92 words. */
        if (!s_drain_dumped) {
            u32 base = 0x80017580u;
            const u32 *ip = (const u32 *)PSXM_2(base);
            u32 k;
            if (ip != NULL) {
                s_drain_dumped = 1;
                for (k = 0; k < 92; k += 4)
                    pcsxr_log(RETRO_LOG_INFO,
                        "[DRAIN] @%08X: %08X %08X %08X %08X\n",
                        (unsigned)(base + k * 4),
                        (unsigned)SWAP32(ip[k+0]), (unsigned)SWAP32(ip[k+1]),
                        (unsigned)SWAP32(ip[k+2]), (unsigned)SWAP32(ip[k+3]));
            }
        }
        {
            u32 n;
            pcsxr_log(RETRO_LOG_INFO, "[RDIDX] idx=%u last_slot=%u\n",
                (unsigned)diag_rdidx_idx,
                (unsigned)((diag_rdidx_idx - 1u) & 15u));
            for (n = 0; n < 16; n++)
                pcsxr_log(RETRO_LOG_INFO,
                    "[RDIDX%02u] pc=%08X val=%u cyc=%u\n",
                    (unsigned)n, (unsigned)diag_rdidx_pc[n],
                    (unsigned)diag_rdidx_val[n],
                    (unsigned)diag_rdidx_cyc[n]);
        }
        /* DICR (0x10F4) y los registros del canal 2 (GPU).  DMA_INTERRUPT_2
         * solo eleva I_STAT bit 3 si DICR tiene el bit (16+n) del canal; con
         * n=2 eso es el bit 18.  Si dma=0 con el bit 18 PUESTO, el gate
         * nuestro esta roto.  Si el bit 18 esta a 0, el juego no pidio esa
         * IRQ y la cola de libgpu se drena por polling, no por ISR. */
        {
            u32 dicr = psxHu32_2(0x10f4);
            pcsxr_log(RETRO_LOG_INFO,
                "[DMA2] dicr=%08X en2=%u glob=%u sent=%u dpcr=%08X"
                " chcr=%08X madr=%08X bcr=%08X\n",
                (unsigned)dicr,
                (unsigned)((dicr >> 18) & 1u),
                (unsigned)((dicr >> 23) & 1u),
                (unsigned)((dicr >> 31) & 1u),
                (unsigned)psxHu32_2(0x10f0),
                (unsigned)psxHu32_2(0x10a8),
                (unsigned)psxHu32_2(0x10a0),
                (unsigned)psxHu32_2(0x10a4));
        }
        /* Ventana de 16 words alrededor de [gp+0x6C] (el indice de LECTURA de
         * la cola de 64 entradas del BIOS).  El indice de ESCRITURA vive
         * cerca, en esta misma estructura.  Volcandolo en las ventanas SANAS
         * se ve la progresion: si el de escritura sube sin que el de lectura
         * lo siga, el consumidor no corre nunca y se sabe cual de los dos se
         * desmadra ANTES de que sea tarde. */
        {
            u32 gp = psxRegs.GPR.n.gp;
            const u32 *w = (const u32 *)PSXM_2(gp + 0x60);
            u32 k;
            if (w != NULL) {
                for (k = 0; k < 16; k += 8) {
                    pcsxr_log(RETRO_LOG_INFO,
                        "[RING] +%02X: %08X %08X %08X %08X %08X %08X %08X %08X\n",
                        (unsigned)(0x60 + k * 4),
                        (unsigned)SWAP32(w[k+0]), (unsigned)SWAP32(w[k+1]),
                        (unsigned)SWAP32(w[k+2]), (unsigned)SWAP32(w[k+3]),
                        (unsigned)SWAP32(w[k+4]), (unsigned)SWAP32(w[k+5]),
                        (unsigned)SWAP32(w[k+6]), (unsigned)SWAP32(w[k+7]));
                }
            }
        }
        /* Las dos variables que consulta el bucle que devuelve -1:
         * [gp+0xF4] (puntero de cola, lo avanza otra funcion en pasos de
         * 0xF0) contra [0x8001A154], y el byte [gp+0xC9] que sondea el
         * otro bucle (0x80015DA4).  Si el puntero y el limite no cuadran,
         * se ve aqui directamente. */
        {
            u32 gp = psxRegs.GPR.n.gp;
            const u32 *pf4 = (const u32 *)PSXM_2(gp + 0xF4);
            const u32 *pa154 = (const u32 *)PSXM_2(0x8001A154);
            const unsigned char *pc9 = (const unsigned char *)PSXM_2(gp + 0xC9);
            pcsxr_log(RETRO_LOG_INFO,
                "[WAIT] gp=%08X [gp+F4]=%08X [8001A154]=%08X [gp+C9]=%02X\n",
                (unsigned)gp,
                (unsigned)(pf4 ? SWAP32(*pf4) : 0xFFFFFFFFu),
                (unsigned)(pa154 ? SWAP32(*pa154) : 0xFFFFFFFFu),
                (unsigned)(pc9 ? *pc9 : 0xFFu));
        }
        {
            u32 e;
            for (e = 0; e < 16; e++) {
                if (diag_exc_count[e] == 0)
                    continue;
                pcsxr_log(RETRO_LOG_INFO,
                    "[EXC] code=%u n=%u first_pc=%08X\n",
                    (unsigned)e, (unsigned)diag_exc_count[e],
                    (unsigned)diag_exc_first_pc[e]);
            }
        }
        /* Anillo dedicado del CD (0x1800-0x1803): los 16 ultimos accesos.
         * Sobrevive al spin del juego en 1070/1074, asi que muestra que hizo
         * el callback ANTES de quedarse esperando.  Si aqui no aparece nada
         * del tramo del cuelgue, el callback NUNCA toco el CD. */
        {
            extern volatile u32  diag_cd_pc[64];
            extern volatile u32  diag_cd_addr[64];
            extern volatile u32  diag_cd_val[64];
            extern volatile u32  diag_cd_cyc[64];
            extern volatile char diag_cd_rw[64];
            extern volatile u32  diag_cd_idx;
            u32 n;
            pcsxr_log(RETRO_LOG_INFO, "[CDIO] idx=%u\n", (unsigned)diag_cd_idx);
            /* 2 por linea para que quepa el ciclo: sin el no se distingue lo que
             * paso en la ventana del cargador de lo de la ronda anterior. */
            for (n = 0; n < 64; n += 2) {
                pcsxr_log(RETRO_LOG_INFO,
                    "[CDIO%02u] %c%04X=%02X@%08X c=%u   %c%04X=%02X@%08X c=%u\n",
                    (unsigned)n,
                    diag_cd_rw[n+0] ? diag_cd_rw[n+0] : 63,
                    (unsigned)diag_cd_addr[n+0], (unsigned)diag_cd_val[n+0],
                    (unsigned)diag_cd_pc[n+0], (unsigned)diag_cd_cyc[n+0],
                    diag_cd_rw[n+1] ? diag_cd_rw[n+1] : 63,
                    (unsigned)diag_cd_addr[n+1], (unsigned)diag_cd_val[n+1],
                    (unsigned)diag_cd_pc[n+1], (unsigned)diag_cd_cyc[n+1]);
            }
        }
        cdrDiagDump();
    }
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

    /* === Coste de la espera del juego a la GPU, en CICLOS EMULADOS ===
     * DrawSync()/GPU_cw() son bucles cerrados sondeando GPUSTAT bit 26.
     * Cada ciclo emulado que se va ahi es un ciclo que el juego NO usa para
     * su logica, y el VBlank sigue llegando puntual (va por ciclos) -> el
     * frontend marca 60 fps mientras el juego se mueve a medio gas.
     *
     * Lectura de la linea:
     *   pct = ciclos_ocupada / ventana.  La ventana son 100M ciclos
     *   (~177 frames), asi que pct es directamente el % del presupuesto de
     *   cada frame que se pierde esperando al hilo GPU del host.
     *     < 3%   -> la espera NO es el problema; mirar CPU (cycle_multiplier)
     *     10-25% -> suficiente para tirar un juego de 60 a 30 fps de logica
     *     > 30%  -> es LA causa
     * Con pcsxr360_gpu_busy=never estos contadores quedan a 0 por
     * definicion: sirve de referencia superior para comparar velocidad. */
    {
        extern volatile u32 diag_gpu_busy_cycles;
        extern volatile u32 diag_gpu_busy_episodes;
        extern volatile u32 diag_gpu_status_reads;
        extern volatile u32 diag_gpu_status_busy;
        extern int g_gpu_busy_model;
        pcsxr_log(RETRO_LOG_INFO,
            "[GPU-BUSY] model=%d polls=%u busy=%u waits=%u cycles=%u (%u%%)\n",
            g_gpu_busy_model,
            (unsigned)diag_gpu_status_reads,
            (unsigned)diag_gpu_status_busy,
            (unsigned)diag_gpu_busy_episodes,
            (unsigned)diag_gpu_busy_cycles,
            (unsigned)(diag_gpu_busy_cycles / (DIAG_DUMP_INTERVAL_CYCLES / 100u)));
        diag_gpu_busy_cycles   = 0;
        diag_gpu_busy_episodes = 0;
        diag_gpu_status_reads  = 0;
        diag_gpu_status_busy   = 0;
    }

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
	/* Despues del reset de la CPU, porque recReset() tambien se llama por
	 * capacidad del code cache desde recRecompile() y ahi NO hay que limpiar
	 * la I-cache: el hardware no tiene ese evento. */
	psxIcacheConfigure();
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
	/* [XBOX360] El handler de excepcion del BIOS NO vuelve a una
	 * instruccion GTE: la SALTA (asume que ya estaba planificada).  Si
	 * la excepcion cae sobre un opcode COP2 y no lo ejecutamos aqui, esa
	 * operacion GTE se PIERDE.  Es el mecanismo real del hack conocido
	 * como "Crash Bandicoot 2 / Hokuto no Ken".
	 *
	 * Antes lo aproximabamos DIFIRIENDO la IRQ mientras hubiera un GTE
	 * en el pipeline, lo que colgaba juegos (el PC podia quedarse parado
	 * sobre el GTE y la IRQ no llegaba nunca).  Al poner el diferimiento
	 * a 0 se quito el cuelgue pero se empezo a PERDER la operacion GTE.
	 * Esto es lo que hace upstream (pcsx_rearmed r3000a.c:111-121) y
	 * cubre los dos casos sin heuristica ni umbrales. */
	if (!Config.HLE) {
		const u32 *ip = (const u32 *)PSXM_2(psxRegs.pc);
		if (ip != NULL) {
			u32 opcode = SWAP32(*ip);
			if ((opcode >> 25) == 0x25 &&
			    (psxRegs.CP0.n.Status & 0x40000000)) {
				u32 saved = psxRegs.code;
				psxRegs.code = opcode;
				psxCP2[opcode & 0x3f]();
				psxRegs.code = saved;
			}
		}
	}

	/* Set the Cause.  Preservar los bits IP (0x700): el bit 10 refleja la
	 * linea de IRQ del hardware y lo mantiene psxHwUpdateCauseIp(), no la
	 * excepcion.  Antes se hacia `Cause = code`, que lo borraba en cada
	 * syscall y lo dejaba pegado a 1 tras cada IRQ. */
	psxRegs.CP0.n.Cause = (psxRegs.CP0.n.Cause & 0x700u) | code;

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

	diag_sr_note('E', psxRegs.CP0.n.EPC, psxRegs.CP0.n.Status);

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

/* ---- Diagnostico TEMPORAL de eventos (autocontenido; NO usa la
 * instrumentacion global de gpu.h, asi que enlaza sin g_xbox_soft_*).
 * Vuelca ~1 linea por segundo emulado (o si branch_tests explota, senal
 * de spin) con los dispatches de cada PSXINT y el avance del reloj (dcyc).
 * Lectura:
 *   - bt (branch_tests) enorme + dcyc pequeno  -> spin / timeslices diminutos.
 *   - algun evento con count enorme            -> ese es el que tormentea.
 *   - bt y counts normales + dcyc ~= PSXCLK    -> el core NO es el cuello
 *                                                 (mirar render/audio).
 * Poner PCSXR_EVT_DIAG a 0 para quitarlo. */
#define PCSXR_EVT_DIAG 0
#if PCSXR_EVT_DIAG
static unsigned int g_evt_cnt[PSXINT_COUNT];
static unsigned int g_evt_bt, g_evt_lastcyc;
static void evt_diag_tick(void) {
	unsigned int dcyc = psxRegs.cycle - g_evt_lastcyc;
	g_evt_bt++;
	if (dcyc >= (unsigned int)PSXCLK || g_evt_bt >= 3000000u) {
		SysPrintf("[EVTDIAG] bt=%u cdr=%u cdrd=%u cddma=%u cdlid=%u cdplay=%u cddbuf=%u spuirq=%u spuupd=%u gpudma=%u spudma=%u dcyc=%u\n",
			g_evt_bt, g_evt_cnt[PSXINT_CDR], g_evt_cnt[PSXINT_CDREAD],
			g_evt_cnt[PSXINT_CDRDMA], g_evt_cnt[PSXINT_CDRLID], g_evt_cnt[PSXINT_CDRPLAY],
			g_evt_cnt[PSXINT_CDRDBUF], g_evt_cnt[PSXINT_SPU_IRQ], g_evt_cnt[PSXINT_SPU_UPDATE],
			g_evt_cnt[PSXINT_GPUDMA], g_evt_cnt[PSXINT_SPUDMA], dcyc);
		{ int k; for (k = 0; k < PSXINT_COUNT; k++) g_evt_cnt[k] = 0; }
		g_evt_bt = 0;
		g_evt_lastcyc = psxRegs.cycle;
	}
}
#endif

void psxBranchTest() {
#if PCSXR_EVT_DIAG
	evt_diag_tick();
#endif
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
				if ((s32)(psxRegs.cycle - psxRegs.intCycle[irq].sCycle - psxRegs.intCycle[irq].cycle) >= 0) { /* wrap-safe s32: tolera sCycle futuro (modelo acumulado CDRPLAYREAD_INT) */
					psxRegs.interrupt &= ~(1u << irq);
#if PCSXR_EVT_DIAG
					if (irq < PSXINT_COUNT) g_evt_cnt[irq]++;
#endif
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
						case PSXINT_CDREAD: cdrPlayReadInterrupt(); break;
						case PSXINT_GPUDMA: gpuInterrupt(); break;
						case PSXINT_MDECOUTDMA: mdec1Interrupt(); break;
						case PSXINT_SPUDMA: spuInterrupt(); break;
						case PSXINT_MDECINDMA: mdec0Interrupt(); break;
						case PSXINT_GPUOTCDMA: gpuotcInterrupt(); break;
						case PSXINT_CDRDMA: cdrDmaInterrupt(); break;
						case PSXINT_CDRPLAY: break; /* n/a: cdrom.c moderno fusiona el play en cdrPlayReadInterrupt (PSXINT_CDREAD) */
						case PSXINT_CDRDBUF: break; /* n/a: cdrom.c moderno no usa decoded-buffer IRQ */
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
	/* Cause bit 10 debe reflejar la linea ANTES de decidir (upstream:
	 * irq_test en psxevents.c:80).  Los eventos de arriba pueden haberla
	 * levantado o bajado. */
	if (psxHwUpdateCauseIp()) {
		if ((psxRegs.CP0.n.Status & 0x401) == 0x401) {
#ifdef PSXCPU_LOG
			PSXCPU_LOG("Interrupt: %x %x\n", psxHu32_2(0x1070), psxHu32_2(0x1074));
#endif
#if PCSXR_DIAG_INSTRUMENTATION
			diag_psx_exception_count++;
#endif
			/* code 0: el bit IP2 ya esta en Cause (psxHwUpdateCauseIp). */
			psxException(0, 0);
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
