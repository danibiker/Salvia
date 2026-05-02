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
 *   PCSXR_NO_THREADING: si el define esta a 1, NO se crea thread; las
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

static u32 gpuDmaChainSize(u32 addr) {
	u32 size;
	u32 DMACommandCounter = 0;

	// initial linked list ptr (word)
	size = 1;

	do {
		addr &= 0x1ffffc;

		if (DMACommandCounter++ > 2000000) break;


		// # 32-bit blocks to transfer
		size += psxMu8_2( addr + 3 );


		// next 32-bit pointer
		addr = psxMu32_2( addr & ~0x3 ) & 0xffffff;
		size += 1;
	} while (!(addr & 0x800000));

	
	return size;
}

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
 * coge el resto). */
static void gpu_thread_proc(void)
{
    while (s_thread_state != GPU_THREAD_STOPPING) {
        uint32_t wpos, rpos, used, ridx, chunk;

        wpos = s_ring_wpos;
        /* Acquire: ver el payload publicado antes de wpos. */
        __lwsync();
        rpos = s_ring_rpos;
        used = wpos - rpos;

        if (used == 0) {
            /* Ring vacio — yield para no quemar CPU. */
            YieldProcessor();
            continue;
        }

        ridx = rpos & RING_MASK;
        /* Limitar el chunk al tramo contiguo (no cruzar wrap). */
        chunk = (RING_SIZE - ridx < used) ? (RING_SIZE - ridx) : used;

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


/* ===== Soul-Reaver collapsed-quad workaround =====
 *
 * Some game(s), most visibly Soul Reaver, render small textured sprites as
 * 0x2E primitives (QuadFlatTexBlend SemiTrans, 9 words: cmd+color, V0, UV0,
 * V1, UV1, V2, UV2, V3, UV3). pcsxr-360's CPU/GTE emulation produces these
 * primitives with all four vertex words equal to the same 32-bit packed XY,
 * so the quad has zero area and nothing is drawn. The cmd, color and UVs are
 * correct — only the 4 vertices collapse.
 *
 * Workaround: scan every GP0 chunk on its way to the GPU plugin, find any
 * 0x2E primitive whose 4 vertex words are identical, and expand the corners
 * around that (correct) center using a fixed half-size of (11, 8). This is
 * the sprite size observed in no$psx for the soul effect. Other games that
 * happen to use 0x2E correctly are unaffected because their 4 vertices
 * differ.
 *
 * Toggled at runtime by `soul_reaver_quad_fix` (set from the libretro core
 * variable `pcsxr360_fix_soul_reaver_quads`). Default off so other games
 * are not perturbed.
 */
int collapsed_quad_fix = 0;

/* Per-call scratch — chunks come from gpuDmaChain in nodes of <=255 words,
 * but the GP0 port path can pass larger blocks. 4096 words is the same cap
 * PEOPS uses for prim assembly. */
#define SOUL_FIX_BUF_WORDS 4096
static u32 soul_fix_buf[SOUL_FIX_BUF_WORDS];

/* PSX primitive length lookup. Returns 0 for cmds we don't recognise so the
 * caller can fall back to advancing one word at a time. */
static int soul_fix_prim_len(u8 cmd)
{
	switch (cmd >> 5) {
	case 1: { /* polygon 0x20-0x3F */
		int len = (cmd & 0x08) ? ((cmd & 0x04) ? 9 : 5)
		                       : ((cmd & 0x04) ? 7 : 4);
		if (cmd & 0x10) len += (cmd & 0x08) ? 3 : 2;
		return len;
	}
	case 2: return (cmd & 0x10) ? 4 : 3;     /* line */
	case 3: return (cmd & 0x18) ? ((cmd & 0x04) ? 3 : 2)
	                            : ((cmd & 0x04) ? 4 : 3); /* sprite/rect */
	default: return 0;
	}
}

/* Walk `buf` (host-native u32s, i.e. PSX-LE bytes byte-swapped on BE) and
 * rewrite collapsed 0x2E quads. Returns the number of primitives fixed. */
static int soul_fix_chunk(u32 *buf, int size)
{
	int fixed = 0;
	int i = 0;
	while (i < size) {
		u8 cmd = ((u8 *)&buf[i])[3];   /* PSX cmd byte = byte+3 of native u32 */
		int len = soul_fix_prim_len(cmd);
		if (len == 0) { i++; continue; }
		if (i + len > size) break;

		if (cmd == 0x2E
		    && buf[i+1] == buf[i+3]
		    && buf[i+3] == buf[i+5]
		    && buf[i+5] == buf[i+7]) {
			u32 psx_v = SWAP32(buf[i+1]);
			s16 cx = (s16)(psx_v & 0xFFFF);
			s16 cy = (s16)((psx_v >> 16) & 0xFFFF);
			const s16 hw = 11;
			const s16 hh = 8;
			/* V0=BR, V1=BL, V2=TR, V3=TL — matches the UV layout no$psx
			 * shows for the soul sprite (UV0 right-top, UV3 left-bottom). */
			#define PACK_XY(x, y) \
				(((u32)((u16)((s16)(y))) << 16) | ((u32)((u16)((s16)(x)))))
			buf[i+1] = SWAP32(PACK_XY(cx + hw, cy + hh));
			buf[i+3] = SWAP32(PACK_XY(cx - hw, cy + hh));
			buf[i+5] = SWAP32(PACK_XY(cx + hw, cy - hh));
			buf[i+7] = SWAP32(PACK_XY(cx - hw, cy - hh));
			#undef PACK_XY
			fixed++;
		}
		i += len;
	}
	return fixed;
}

/* ===========================================================================
 * DMA chain enqueue: aplica el fix de Soul Reaver y encola al ring.
 * En modo NO_THREADING o si el thread no esta corriendo, ejecuta inline.
 * =========================================================================== */

static void chain_enqueue(uint32_t *pMem, int size)
{
    if (size <= 0) return;

    /* Soul-Reaver collapsed-quad workaround: scan el chunk en busca de
     * primitivas 0x2E con los 4 vertices iguales, y expandirlas a un
     * rectangulo razonable.  Solo activo si el toggle libretro lo pide.
     * Copiamos al scratch para no mutar la PSX RAM. */
    if (collapsed_quad_fix && size <= SOUL_FIX_BUF_WORDS) {
        int j;
        for (j = 0; j < size; j++) soul_fix_buf[j] = pMem[j];
        if (soul_fix_chunk(soul_fix_buf, size) > 0) {
            pMem = soul_fix_buf;
        }
    }

#if PCSXR_NO_THREADING
    GPU_writeDataMem(pMem, size);
#else
    if (s_thread_state == GPU_THREAD_RUNNING) {
        ring_push((const uint32_t *)pMem, (uint32_t)size);
    } else {
        /* Pre-init, post-shutdown, o NO_THREADING runtime: directo. */
        GPU_writeDataMem(pMem, size);
    }
#endif
}

////////////////////////////////////////////////////////////gpu.c

/* PSX DMA linked-list parser. Cada nodo tiene `count` words a procesar
 * y un puntero al siguiente.  Termina cuando un puntero tiene bit 23. */
void gpuDmaChain(uint32_t addr)
{
    uint32_t dmaMem;
    uint32_t * baseAddrL;
    unsigned char * baseAddrB;
    short count;
    unsigned int DMACommandCounter = 0;

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

        addr = psxMu32_2(addr & ~0x3) & 0xffffff;
    }
    while (!(addr & 0x800000));
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
    GPU_readDataMem(addr, size);
}

void gpuWriteDataMem(uint32_t * pMem, int size)
{
    /* Esta entrada se usa para writes desde psxhw.c (game escribiendo a
     * la GP0 register palabra a palabra) o desde psxDma2 mem2vram (block).
     * No encolamos: el ring esta reservado para el DMA chain (donde la
     * latencia importa porque son 600+ comandos por frame). */
    ring_drain();
    GPU_writeDataMem(pMem, size);
}

u32 gpuReadData(void)
{
    ring_drain();
    return GPU_readData();
}

void gpuWriteData(u32 data)
{
    ring_drain();
    GPU_writeData(data);
}

void gpuWriteStatus(u32 data)
{
    /* Status writes cambian register state (display mode, drawing area,
     * etc).  Drain antes para que los draws encolados respeten el state
     * anterior y los siguientes vean el nuevo. */
    ring_drain();
    GPU_writeStatus(data);
}

void gpuUpdateLace(void)
{
    /* VBlank: BlitScreen32 (dentro de GPU_updateLace) lee psxVuw que el
     * thread modifica.  Drain primero para garantizar que el frame esta
     * completo antes de presentarlo. */
    ring_drain();
    GPU_updateLace();
}

/* ===========================================================================
 * Lifecycle del GPU helper thread.
 * =========================================================================== */

void gpuDmaThreadShutdown(void)
{
#if PCSXR_NO_THREADING
    /* Modo single-thread: no hay nada que parar. */
    s_thread_state = GPU_THREAD_STOPPED;
    return;
#else
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
#endif
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

#if PCSXR_NO_THREADING
    /* Modo single-thread: no creamos thread.  chain_enqueue / ring_drain
     * tomaran el path direct. */
    s_thread_state = GPU_THREAD_STOPPED;
    return;
#else
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
#endif
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

			size = gpuDmaChainSize(madr);

	        gpuDmaChain(madr & 0x1fffff);

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

