/*  Copyright (c) 2010, shalma.
 *  Portions Copyright (c) 2002, Pete Bernert.
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

#include "psxhw.h"
#include "gpu.h"
#include "psxdma.h"
/* PCSXR_PERF_ENABLED gates the gpu_wait_ticks sonda below.  See
 * plugins/xbox_soft/peops_prof.h for the rationale on locating the flag
 * at the bottom of the include graph. */
#include "../plugins/xbox_soft/peops_prof.h"

#include <libretro.h>

extern unsigned int hSyncCount;

#define GPUSTATUS_ODDLINES            0x80000000
#define GPUSTATUS_DMABITS             0x60000000 // Two bits
#define GPUSTATUS_READYFORCOMMANDS    0x10000000
#define GPUSTATUS_READYFORVRAM        0x08000000
#define GPUSTATUS_IDLE                0x04000000

#define GPUSTATUS_DISPLAYDISABLED     0x00800000
#define GPUSTATUS_INTERLACED          0x00400000
#define GPUSTATUS_RGB24               0x00200000
#define GPUSTATUS_PAL                 0x00100000
#define GPUSTATUS_DOUBLEHEIGHT        0x00080000
#define GPUSTATUS_WIDTHBITS           0x00070000 // Three bits
#define GPUSTATUS_MASKENABLED         0x00001000
#define GPUSTATUS_MASKDRAWN           0x00000800
#define GPUSTATUS_DRAWINGALLOWED      0x00000400
#define GPUSTATUS_DITHER              0x00000200

/* ===========================================================================
 * GPU THREADING SUBSYSTEM (rediseño 2026-05)
 * ===========================================================================
 *
 *  Reemplaza la implementacion previa, que sufria de varios bugs:
 *   - Vars `gpu_thread_running` y `gpu_thread_exit` con ownership confusa
 *     y sin barriers explicitos en lecturas cruzadas entre threads.
 *   - `gpuThreadEnable(1)` reseteaba `gpu_thread_exit=0` despues de
 *     `gpuDmaThreadInit`, contradiciendo decisiones tomadas en init.
 *   - `psxDma2` mem2vram/vram2mem hacian bypass de la sincronizacion con
 *     el thread, abriendo races sobre `psxVuw`.
 *
 *  Modelo nuevo:
 *
 *   Producer/Consumer SPSC ring de power-of-two (RING_SIZE = 128K u32).
 *
 *   Producer (CPU emulada via gpuDmaChain) llama `chain_enqueue` que
 *   copia al ring y publica `s_ring_wpos` con barrera lwsync (release).
 *
 *   Consumer (GPU helper thread, core 4) lee `s_ring_wpos` con lwsync
 *   (acquire), procesa el chunk via `GPU_writeDataMem`, y publica
 *   `s_ring_rpos` con lwsync (release).
 *
 *   Sincronizacion main↔helper: cualquier acceso direct a `GPU_*` desde
 *   el thread principal (lectures, lace, status) llama `ring_drain()`
 *   primero, que espera a que el thread vacie el ring antes de tocar
 *   estado del GPU.  Eso garantiza orden de operaciones: todos los
 *   comandos encolados se procesan antes que los direct calls que
 *   siguen.
 *
 *   Lifecycle: el estado del thread es un enum claro
 *   (`STOPPED|RUNNING|STOPPING`) que sirve como source-of-truth.  Init
 *   crea thread → state=RUNNING.  Shutdown drena ring, marca
 *   state=STOPPING, espera al thread (con timeout), cierra handle.
 *   Idempotente y safe contra double-init/double-shutdown.
 *
 *   g_pcsxr_threading_enabled: si esta a 0, NO se crea thread; las
 *   funciones encolan via direct call.  Modo single-thread completo,
 *   util como fallback / diagnostico.
 *
 * ========================================================================= */

#define RING_SIZE  (128u * 1024u)
#define RING_MASK  (RING_SIZE - 1u)

/* Ring storage.  Aligned a 128B (cache line de Xenon) para evitar false
 * sharing con cualquier var contigua. */
static __declspec(align(128)) uint32_t s_ring_data[RING_SIZE];

/* Cursor del producer (escrito SOLO por la CPU emulada que llama push).
 * Read-only para el consumer.  En su propia cache line. */
static __declspec(align(128)) volatile uint32_t s_ring_wpos = 0;

/* Cursor del consumer (escrito SOLO por el GPU thread).
 * Read-only para el producer.  En su propia cache line. */
static __declspec(align(128)) volatile uint32_t s_ring_rpos = 0;

/* Estado del thread.  Source-of-truth unica de si hay thread vivo y si
 * debe parar.  Solo el main thread escribe (en init/shutdown).  El
 * thread la lee. */
enum {
    GPU_THREAD_STOPPED  = 0,
    GPU_THREAD_RUNNING  = 1,
    GPU_THREAD_STOPPING = 2
};
static volatile uint32_t s_thread_state = GPU_THREAD_STOPPED;
static HANDLE            s_thread_handle = NULL;

/* QueryPerformanceCounter ticks acumulados durante ring_drain (CPU
 * emulada bloqueada esperando al GPU thread).  retro_run lo resetea a 0
 * antes de psxCpu->Execute() y lo lee para:
 *  (a) Auto-frameskip: si gpu_wait domina el exceso sobre el budget,
 *      skipear da speedup real.  Si el cuello es el dynarec (gpu_wait
 *      bajo), skipear solo introduce flicker sin ganancia y se evita.
 *  (b) Dump [PERF]: desglose "CPU real" vs "esperando al GPU".
 * Coste cero cuando no hay espera (ring_drain check rapido y retorna). */
volatile uint64_t gpu_wait_ticks = 0;

/* ===========================================================================
 * Diagnostic instrumentation (gated by PCSXR_DIAG_INSTRUMENTATION in gpu.h).
 *
 * Cuando ON, estas variables forman la "telemetria" que el watchdog
 * del consumer thread imprime cuando detecta que el cycle counter del
 * main no avanza.  Permite responder a "donde se queda main cuando
 * el juego se cuelga" sin attaching debugger, leyendo solo el log de
 * pcsxr_log.
 *
 * Cuando OFF (default), nada de esto se compila; las macros DIAG_SET_*
 * en gpu.h expanden a (void)0.
 * =========================================================================== */
#if PCSXR_DIAG_INSTRUMENTATION

/* Section tracker para diagnostico: el main thread lo actualiza en
 * cada fase de retro_run via DIAG_SET_RR_SEC().  El watchdog lo lee
 * para identificar donde se quedo el main cuando deja de avanzar. */
volatile int retro_run_section = 0;

/* Hardware register access tracker: psxhw.c dispatcher setea la
 * direccion (0x1f80xxxx) que esta siendo leida/escrita.  Bit 16
 * distingue read/write.  0 = idle. */
volatile uint32_t s_psxhw_active = 0;

/* Interrupt handler tracker: psxBranchTest setea cual interrupt esta
 * procesando.  Valores PSXINT_* del enum en r3000a.h.  -1 = idle. */
volatile int s_psx_irq_handler = -1;

/* Plugin call tracker: cada wrapper GPU marca su valor antes de llamar
 * al puntero del plugin (GPU_*) y lo limpia despues.  Si watchdog ve
 * cycle FROZEN con un valor != 0, main esta atrapado en ese plugin
 * call (PEOPS o gpu_duck). */
volatile int s_gpu_plugin_call = 0;

/* Contadores de spin del producer/consumer del ring.  Si el cycle
 * counter esta FROZEN y estos crecen rapido entre dos logs del
 * watchdog, main esta atascado en uno de nuestros propios spin loops
 * (no en una funcion externa del dynarec).  Solo informacional —
 * los lee el watchdog y los muestra en delta. */
static volatile uint64_t s_ring_drain_spin_count = 0;
static volatile uint64_t s_ring_push_spin_count  = 0;

#endif /* PCSXR_DIAG_INSTRUMENTATION */

#define GPUDMA_INT(eCycle) set_event(PSXINT_GPUDMA, eCycle)

/*
 * Historical note on two upstream fixes merged into this file:
 *
 *  1. Removed the legacy PEOPS-SOFTGPU `CheckForEndlessLoop` /
 *     `lUsedAddr[3]` heuristic that used to guard the DMA-chain parser.
 *     It tracked only two effective recent addresses and false-positived
 *     on legitimate OT access patterns — notably Soul Reaver, which
 *     splices particle-effect chunks into the ordering table post-sort,
 *     producing non-monotonic revisits that tripped the heuristic and
 *     caused every subsequent chain node to be silently dropped. The
 *     plain DMACommandCounter safety net (also used by PCSX-ReARMed) is
 *     sufficient.
 *
 *  2. End-of-linked-list terminator corrected from `addr == 0xffffff` to
 *     `addr & 0x800000`. Contrary to some documentation, the PSX GPU
 *     DMA-chain terminator is ANY pointer with bit 23 set, not the
 *     specific value 0xFF'FFFF. Soul Reaver emits terminators like
 *     0x800000 / 0x810000 / etc., which the old equality check walked
 *     straight past — reading 32-bit words of RAM past the real end
 *     into the GPU FIFO as if they were GP0 commands. Stray control
 *     commands (E3 set-drawing-area, E4, E5 set-draw-offset, E1
 *     texpage) landed there and corrupted GPU state for the rest of
 *     the frame, silently scissoring out the next chain's soul
 *     primitives. Matches PCSX-ReARMed behaviour.
 */

/* Eliminada: gpuDmaChainSize hacia un walk separado de la misma linked
 * list que gpuDmaChain ya recorre, duplicando lecturas a psxM_2.  En
 * BR2 batalla pesada cada chain tiene cientos de nodos × 60 chains/seg
 * = decenas de miles de lecturas redundantes/seg, pagando L1 miss
 * frecuente (psxM_2 es 2MB, no cabe en L1 32KB del Xenon).  Ahora
 * gpuDmaChain acumula y retorna el size durante su unico walk.
 *
 * Mantenemos esta nota como anclaje historico — la funcion antigua
 * estaba en este punto del archivo. */

/* ===========================================================================
 * Ring API: producer (CPU emulada) y consumer (GPU thread)
 * =========================================================================== */

/* PRODUCER: copiar `size` palabras del buffer `data` al ring, esperando
 * espacio si esta lleno.  Solo llamado desde la CPU emulada en main thread.
 *
 * Memory ordering:
 *   1. Read s_ring_rpos para calcular espacio libre (acquire-like, pero
 *      es ok consumir un valor stale — solo nos hace dormir mas).
 *   2. Copiar payload al ring.
 *   3. lwsync (release): payload visible antes de publicar wpos.
 *   4. Publicar s_ring_wpos. */
static void ring_push(const uint32_t *data, uint32_t size)
{
    uint32_t wpos = s_ring_wpos;   /* producer es dueño exclusivo */
    uint32_t widx, first_chunk;

    /* Esperar espacio.  Capacity - used >= size.  En SPSC con cursores
     * uint32 sin wrap explicito (unsigned arithmetic wrap-safe),
     * used = wpos - rpos.  Spin con yield si no cabe. */
    for (;;) {
        uint32_t rpos = s_ring_rpos;
        uint32_t used = wpos - rpos;
        if (RING_SIZE - used >= size) break;
#if PCSXR_DIAG_INSTRUMENTATION
        s_ring_push_spin_count++;
#endif
        YieldProcessor();
    }

    /* Copia con memcpy (orden de magnitud mas rapido que loop word-a-word
     * para chunks grandes; PEOPS DMA chain envia hasta 255 words por nodo). */
    widx = wpos & RING_MASK;
    first_chunk = RING_SIZE - widx;
    if (first_chunk >= size) {
        /* Cabe contiguo, sin wrap. */
        memcpy(&s_ring_data[widx], data, size * sizeof(uint32_t));
    } else {
        /* Wrap: dos memcpy. */
        memcpy(&s_ring_data[widx], data, first_chunk * sizeof(uint32_t));
        memcpy(&s_ring_data[0],
               data + first_chunk,
               (size - first_chunk) * sizeof(uint32_t));
    }

    /* Release barrier: payload completo antes de publicar wpos.  El
     * consumer hace lwsync acquire y ve datos consistentes. */
    __lwsync();
    s_ring_wpos = wpos + size;
}

/* MAIN-THREAD SYNC: esperar a que el ring este vacio.  Llamado antes
 * de cualquier acceso direct a `GPU_*` desde main para garantizar que
 * todos los comandos encolados se procesan primero (y que las escrituras
 * del thread a psxVuw son visibles para el main).
 *
 * Memory ordering:
 *   1. Spin hasta wpos == rpos (visto desde main).
 *   2. lwsync acquire: las escrituras a VRAM/state hechas por el thread
 *      antes de publicar rpos son ahora visibles aqui.
 */
static void ring_drain(void)
{
    LARGE_INTEGER t0, t1;

    /* Si no hay thread (modo NO_THREADING o pre-init/post-shutdown), no
     * hay nada que drenar.  Y el ring nunca se uso. */
    if (s_thread_state != GPU_THREAD_RUNNING)
        return;

    /* Fast path: si esta vacio ya, no QPC ni spin.  Coste cero comun. */
    if (s_ring_wpos == s_ring_rpos)
        return;

    QueryPerformanceCounter(&t0);
    while (s_ring_wpos != s_ring_rpos) {
#if PCSXR_DIAG_INSTRUMENTATION
        s_ring_drain_spin_count++;
#endif
        YieldProcessor();
    }
    /* Acquire: ver psxVuw/state writes hechos por el thread antes de rpos. */
    __lwsync();
    QueryPerformanceCounter(&t1);
    gpu_wait_ticks += (uint64_t)(t1.QuadPart - t0.QuadPart);
}

/* CONSUMER LOOP: GPU helper thread.  Ejecuta hasta state=STOPPING.  Lee
 * chunks contiguos del ring (sin wrap-split en una sola llamada — si hay
 * wrap, procesamos hasta el final del array y la siguiente iteracion
 * coge el resto).
 *
 * Cuando PCSXR_DIAG_INSTRUMENTATION=1 incluye un WATCHDOG: si el ring
 * se queda vacio durante muchas iteraciones consecutivas (=> el main no
 * esta produciendo trabajo) Y el cycle counter del main no avanza, el
 * sistema esta colgado.  Logueamos el state del main thread para
 * diagnostico. */
static void gpu_thread_proc(void)
{
#if PCSXR_DIAG_INSTRUMENTATION
    uint32_t s_idle_iters = 0;
    uint32_t s_last_cycle = 0;
    uint64_t s_last_drain_spin = 0;
    uint64_t s_last_push_spin  = 0;
#endif

    while (s_thread_state != GPU_THREAD_STOPPING) {
        uint32_t wpos, rpos, used, ridx, chunk;

        wpos = s_ring_wpos;
        /* Acquire: ver el payload publicado antes de wpos. */
        __lwsync();
        rpos = s_ring_rpos;
        used = wpos - rpos;

        if (used == 0) {
            /* Ring vacio — yield para no quemar CPU. */
#if PCSXR_DIAG_INSTRUMENTATION
            /* Watchdog: si llevamos mucho tiempo idle Y el main no
             * avanza cycles, el sistema esta colgado.  Logear state
             * del main thread (best-effort, sin sincronizacion — solo
             * lectura para diagnostico).  Thresholds geometricos para
             * no spamear el log: ~10M / 50M / 250M iters de
             * YieldProcessor.  En idle ese rate es del orden de 100s
             * de millones por segundo, asi que estos thresholds
             * representan ~0.1s / 0.5s / 2.5s aproximadamente. */
            s_idle_iters++;
            if (s_idle_iters == 10000000u  ||
                s_idle_iters == 50000000u  ||
                s_idle_iters == 250000000u)
            {
                /* C89 (VS2010 + Xbox 360 SDK): TODAS las declaraciones
                 * tienen que ir al principio del bloque, antes de
                 * cualquier statement. */
                uint32_t cur_cycle;
                uint32_t delta;
                int sec;
                uint64_t cur_drain_spin;
                uint64_t cur_push_spin;
                uint64_t drain_spin_delta;
                uint64_t push_spin_delta;
                int plugin_call;
                uint32_t hw_active;
                int irq_handler;
                const char *irq_name;

                cur_cycle = psxRegs.cycle;
                delta = cur_cycle - s_last_cycle;
                sec = retro_run_section;

                /* Snapshot de los contadores de spin del main thread.
                 * Si crecen rapido entre dos logs Y cycles estan FROZEN,
                 * main esta atascado en uno de nuestros propios spin
                 * loops (no en una funcion externa del dynarec). */
                cur_drain_spin = s_ring_drain_spin_count;
                cur_push_spin  = s_ring_push_spin_count;
                drain_spin_delta = cur_drain_spin - s_last_drain_spin;
                push_spin_delta  = cur_push_spin  - s_last_push_spin;

                /* Plugin call activo (si != 0 con FROZEN, main esta en
                 * el plugin GPU). */
                plugin_call = s_gpu_plugin_call;

                /* Hardware register access activo (psxhw.c dispatcher) y
                 * IRQ handler activo (psxBranchTest).  Si plug=0 y main
                 * sigue FROZEN, miramos `hw=0xXXXXX` o `irq=N(NAME)`
                 * para identificar donde se quedo. */
                hw_active = s_psxhw_active;
                irq_handler = s_psx_irq_handler;
                switch (irq_handler) {
                    case -1:                  irq_name = "none";       break;
                    case PSXINT_SIO:          irq_name = "sio";        break;
                    case PSXINT_CDR:          irq_name = "cdr";        break;
                    case PSXINT_CDREAD:       irq_name = "cdread";     break;
                    case PSXINT_GPUDMA:       irq_name = "gpudma";     break;
                    case PSXINT_MDECOUTDMA:   irq_name = "mdecout";    break;
                    case PSXINT_SPUDMA:       irq_name = "spudma";     break;
                    case PSXINT_MDECINDMA:    irq_name = "mdecin";     break;
                    case PSXINT_GPUOTCDMA:    irq_name = "gpuotc";     break;
                    case PSXINT_CDRDMA:       irq_name = "cdrdma";     break;
                    case PSXINT_CDRPLAY:      irq_name = "cdrplay";    break;
                    case PSXINT_CDRDBUF:      irq_name = "cdrdbuf";    break;
                    case PSXINT_CDRLID:       irq_name = "cdrlid";     break;
                    default:                  irq_name = "?";          break;
                }

                /* Log en DOS lineas porque el buffer de pcsxr_log
                 * trunca cerca de los ~115 caracteres.  Linea 1: estado
                 * del main thread (PC, cycles, plugin/irq actual).
                 * Linea 2: ring state + spin counts + hw register. */
                pcsxr_log(RETRO_LOG_INFO,
                    "[WD] sec=%d pc=0x%08X cyc=%u dlt=%u %s plug=%d irq=%d(%s)\n",
                    sec,
                    (unsigned)psxRegs.pc,
                    (unsigned)cur_cycle,
                    (unsigned)delta,
                    (delta == 0) ? "FROZEN"
                                 : (delta < 1000) ? "TIGHT-LOOP"
                                                  : "running",
                    plugin_call, irq_handler, irq_name);
                pcsxr_log(RETRO_LOG_INFO,
                    "[WD] ring=%u/%u drain+=%u push+=%u hw=0x%05X\n",
                    (unsigned)wpos,
                    (unsigned)rpos,
                    (unsigned)drain_spin_delta,
                    (unsigned)push_spin_delta,
                    (unsigned)hw_active);

                s_last_cycle      = cur_cycle;
                s_last_drain_spin = cur_drain_spin;
                s_last_push_spin  = cur_push_spin;
            }
#endif
            YieldProcessor();
            continue;
        }

#if PCSXR_DIAG_INSTRUMENTATION
        /* Ring tiene trabajo, resetear watchdog. */
        s_idle_iters = 0;
        s_last_cycle = psxRegs.cycle;
#endif

        ridx = rpos & RING_MASK;
        /* Limitar el chunk al tramo contiguo (no cruzar wrap). */
        chunk = (RING_SIZE - ridx < used) ? (RING_SIZE - ridx) : used;

        /* No actualizamos s_gpu_plugin_call aqui aunque tecnicamente
         * estamos en GPU_writeDataMem desde el consumer.  La intencion
         * de ese tracker es identificar cuelgues del MAIN thread en
         * plugin calls; si lo escribe tambien el consumer, falsea el
         * diagnostico (main puede no estar dentro del plugin pero
         * tracker lo dice).  GPU_CALL_THREAD_PROC se mantiene en gpu.h
         * por si alguna vez quisieramos un tracker dual. */
        GPU_writeDataMem(&s_ring_data[ridx], chunk);

        /* Release: writes del GPU (psxVuw, state) visibles antes de
         * publicar rpos.  El main thread en ring_drain hace lwsync
         * acquire para verlas. */
        __lwsync();
        s_ring_rpos = rpos + chunk;
    }

    /* Marcamos que terminamos.  Shutdown lee STOPPED para confirmar. */
    s_thread_state = GPU_THREAD_STOPPED;
    ExitThread(0);
}



/* ===========================================================================
 * DMA chain enqueue: aplica el fix de Soul Reaver y encola al ring.
 * En modo NO_THREADING o si el thread no esta corriendo, ejecuta inline.
 * =========================================================================== */

static void chain_enqueue(uint32_t *pMem, int size)
{
    if (size <= 0) return;

   	if (!g_pcsxr_threading_enabled){
		GPU_writeDataMem(pMem, size);
	} else {
		if (s_thread_state == GPU_THREAD_RUNNING) {
			ring_push((const uint32_t *)pMem, (uint32_t)size);
		} else {
			/* Pre-init, post-shutdown, o NO_THREADING runtime: directo. */
			GPU_writeDataMem(pMem, size);
		}
	}
}

////////////////////////////////////////////////////////////gpu.c

/* PSX DMA linked-list parser. Cada nodo tiene `count` words a procesar
 * y un puntero al siguiente.  Termina cuando un puntero tiene bit 23.
 *
 * Retorna el numero total de words "vistos" en el walk (initial ptr +
 * suma de counts + 1 next-ptr por nodo).  Ese valor lo usa psxDma2
 * para programar el GPUDMA_INT proporcional al trabajo, sustituyendo
 * a la antigua `gpuDmaChainSize` que recorria la misma linked list
 * por separado.  Walk unico = ~50% menos lecturas a psxM_2 por chain. */
uint32_t gpuDmaChain(uint32_t addr)
{
    uint32_t dmaMem;
    uint32_t * baseAddrL;
    unsigned char * baseAddrB;
    short count;
    unsigned int DMACommandCounter = 0;
    uint32_t size = 1;   /* contar el initial linked list ptr word */

    baseAddrL = (u32 *)psxM_2;
    baseAddrB = (unsigned char*) baseAddrL;

    do
    {
        addr &= 0x1FFFFC;
        if (DMACommandCounter++ > 2000000) break;

        count  = baseAddrB[addr + 3];
        dmaMem = addr + 4;

        if (count > 0) {
            chain_enqueue(&baseAddrL[dmaMem >> 2], count);
        }

        /* Acumular size: count words del nodo + 1 word del next ptr
         * (mismo calculo que hacia gpuDmaChainSize en su loop separado). */
        size += (uint32_t)(unsigned char)count + 1u;

        addr = psxMu32_2(addr & ~0x3) & 0xffffff;
    }
    while (!(addr & 0x800000));

    return size;
}

/* ===========================================================================
 * Wrappers libretro-facing.  Cualquier acceso direct desde main al GPU
 * pasa por aqui: drain primero, luego direct call.  Eso garantiza que el
 * helper thread ya proceso todo lo encolado antes de que el main lea o
 * cambie estado.
 * =========================================================================== */

void gpuReadDataMem(uint32_t * addr, int size)
{
    ring_drain();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_READ_DATA_MEM);
    GPU_readDataMem(addr, size);
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);
}

void gpuWriteDataMem(uint32_t * pMem, int size)
{
    /* Esta entrada se usa para writes desde psxhw.c (game escribiendo a
     * la GP0 register palabra a palabra) o desde psxDma2 mem2vram (block).
     * No encolamos: el ring esta reservado para el DMA chain (donde la
     * latencia importa porque son 600+ comandos por frame). */
    ring_drain();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_WRITE_DATA_MEM);
    GPU_writeDataMem(pMem, size);
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);
}

u32 gpuReadData(void)
{
    u32 r;
    ring_drain();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_READ_DATA);
    r = GPU_readData();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);
    return r;
}

void gpuWriteData(u32 data)
{
    ring_drain();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_WRITE_DATA);
    GPU_writeData(data);
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);
}

void gpuWriteStatus(u32 data)
{
    /* Status writes cambian register state (display mode, drawing area,
     * etc).  Drain antes para que los draws encolados respeten el state
     * anterior y los siguientes vean el nuevo. */
    ring_drain();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_WRITE_STATUS);
    GPU_writeStatus(data);
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);
}

void gpuUpdateLace(void)
{
    /* VBlank: BlitScreen32 (dentro de GPU_updateLace) lee psxVuw que el
     * thread modifica.  Drain primero para garantizar que el frame esta
     * completo antes de presentarlo. */
    ring_drain();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_UPDATE_LACE);
    GPU_updateLace();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);
}

/* ===========================================================================
 * Lifecycle del GPU helper thread.
 * =========================================================================== */

void gpuDmaThreadShutdown(void)
{
	if (!g_pcsxr_threading_enabled){
		/* Modo single-thread: no hay nada que parar. */
		s_thread_state = GPU_THREAD_STOPPED;
		return;
	} else {
		DWORD wait_result;

		if (s_thread_state == GPU_THREAD_STOPPED || s_thread_handle == NULL) {
			/* Idempotente: ya cerrado o nunca abierto. */
			s_thread_state  = GPU_THREAD_STOPPED;
			s_thread_handle = NULL;
			return;
		}

		/* Drenar ring antes de pedir stop, asi no perdemos comandos
		 * pendientes que el juego (o el BIOS) ya envio. */
		while (s_ring_wpos != s_ring_rpos) {
			YieldProcessor();
		}
		__lwsync();

		/* Pedir parada y esperar al thread.  Timeout defensivo: si el thread
		 * quedase atascado (ej. dentro de un GPU_writeDataMem patologicamente
		 * largo), no colgamos el unload — soltamos el handle y seguimos.
		 * TerminateThread no esta disponible en XDK Xbox 360. */
		s_thread_state = GPU_THREAD_STOPPING;

		wait_result = WaitForSingleObject(s_thread_handle, 5000);
		if (wait_result == WAIT_TIMEOUT) {
			pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] WARNING: GPU helper thread did not exit in 5s, leaving handle\n");
			/* Marcamos como stopped para que la proxima init no reuse el
			 * handle viejo.  El thread fugado eventualmente saldra solo
			 * cuando vea STOPPING. */
			s_thread_handle = NULL;
			s_thread_state  = GPU_THREAD_STOPPED;
			return;
		}

		CloseHandle(s_thread_handle);
		s_thread_handle = NULL;
		s_thread_state  = GPU_THREAD_STOPPED;
	}
}

void gpuDmaThreadInit(void)
{
    /* Idempotente: si ya hay thread, lo apagamos primero. */
    if (s_thread_handle != NULL || s_thread_state != GPU_THREAD_STOPPED) {
        gpuDmaThreadShutdown();
    }

    /* Reset cursors del ring.  Los hacemos ANTES de crear el thread
     * para que el thread vea valores limpios al arrancar. */
    s_ring_wpos = 0;
    s_ring_rpos = 0;

	if (!g_pcsxr_threading_enabled){
		/* Modo single-thread: no creamos thread.  chain_enqueue / ring_drain
		 * tomaran el path direct. */
		s_thread_state = GPU_THREAD_STOPPED;
		return;
	} else {
		/* Marcar RUNNING antes de ResumeThread.  El consumer hace check de
		 * STOPPING para parar; mientras este RUNNING corre normal. */
		s_thread_state = GPU_THREAD_RUNNING;

		s_thread_handle = CreateThread(NULL, 0,
									   (LPTHREAD_START_ROUTINE)gpu_thread_proc,
									   NULL, CREATE_SUSPENDED, NULL);
		if (!s_thread_handle) {
			/* CreateThread fallo (heap exhausted, kernel handles, etc.).
			 * Caer a modo single-thread runtime: chain_enqueue / ring_drain
			 * detectan state != RUNNING y van direct. */
			pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] WARNING: CreateThread for GPU helper failed, falling back to inline mode\n");
			s_thread_state = GPU_THREAD_STOPPED;
			return;
		}

		SetThreadPriority(s_thread_handle, THREAD_PRIORITY_ABOVE_NORMAL);
		XSetThreadProcessor(s_thread_handle, 4);
		ResumeThread(s_thread_handle);
	}
}

/* gpuThreadEnable() solia existir y reseteaba gpu_thread_exit, anulando
 * decisiones tomadas en gpuDmaThreadInit.  Eliminada por ser fuente de
 * bugs.  El lifecycle se controla SOLO via init/shutdown.  Compatibilidad
 * con callers viejos: stub no-op para no romper builds intermedios. */
void gpuThreadEnable(int enable) { (void)enable; /* deprecated, no-op */ }

void psxDma2(u32 madr, u32 bcr, u32 chcr) { // GPU
	u32 *ptr;
	u32 size;

	switch (chcr) {
		case 0x01000200: // vram2mem
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA2 GPU - vram2mem *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif
			ptr = (u32 *)PSXM_2(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA2 GPU - vram2mem *** NULL Pointer!!!\n");
#endif
				break;
			}
			// BA blocks * BS words (word = 32-bits)
			size = (bcr >> 16) * (bcr & 0xffff);
			/* Pasamos por el wrapper para que ring_drain garantice que
			 * el thread ya escribio toda la VRAM antes de leerla.  Antes
			 * iba direct a GPU_readDataMem, era una race latente. */
			gpuReadDataMem(ptr, size);

			psxCpu->Clear(madr, size);

			// already 32-bit word size ((size * 4) / 4)
			GPUDMA_INT(size);
			return;

		case 0x01000201: // mem2vram
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA 2 - GPU mem2vram *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif

			ptr = (u32 *)PSXM_2(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA2 GPU - mem2vram *** NULL Pointer!!!\n");
#endif
				break;
			}
			// BA blocks * BS words (word = 32-bits)
			size = (bcr >> 16) * (bcr & 0xffff);
			/* Idem: wrapper en lugar de direct.  Drain primero, luego
			 * GPU_writeDataMem.  Garantiza orden frente a draws encolados. */
			gpuWriteDataMem(ptr, size);


			// already 32-bit word size ((size * 4) / 4)
			GPUDMA_INT(size);
			return;

		case 0x01000401: // dma chain
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA 2 - GPU dma chain *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif

			/* Walk unico: gpuDmaChain procesa la chain (encola al ring
			 * SPSC) y retorna el size acumulado.  Antes hacia dos walks
			 * separados (gpuDmaChainSize + gpuDmaChain) sobre la misma
			 * linked list — la primera pasada solo era para calcular
			 * `size` y programar GPUDMA_INT.  Eliminado.  En BR2/SR/FFVII
			 * con cientos de nodos por chain ahorra decenas de miles de
			 * lecturas a psxM_2 por segundo. */
			size = gpuDmaChain(madr & 0x1fffff);

			// Tekken 3 = use 1.0 only (not 1.5x)

			// Einhander = parse linked list in pieces (todo)
			// Final Fantasy 4 = internal vram time (todo)
			// Rebel Assault 2 = parse linked list in pieces (todo)
			// Vampire Hunter D = allow edits to linked list (todo)
			GPUDMA_INT(size);
			return;

#ifdef PSXDMA_LOG
		default:
			PSXDMA_LOG("*** DMA 2 - GPU unknown *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
			break;
#endif
	}
	HW_DMA2_CHCR_2 &= SWAP32(~0x01000000);
	DMA_INTERRUPT_2(2);
}

