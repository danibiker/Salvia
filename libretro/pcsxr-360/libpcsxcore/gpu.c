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
 *   Consumer (GPU helper thread, HW thread 2 via XSetThreadProcessor) lee `s_ring_wpos` con lwsync
 *   (acquire), procesa el chunk via `GPU_writeDataMem`, y publica
 *   `s_ring_rpos` con lwsync (release).
 *
 *   Sincronizacion main↔helper: los accesos directos a `GPU_*` desde el
 *   thread principal que LEEN O MODIFICAN estado (datos, lace, writes de
 *   status) llaman `ring_drain()` primero, de modo que el thread ya
 *   proceso todo lo encolado antes.  Eso garantiza el orden: los comandos
 *   encolados se procesan antes que los direct calls que siguen.
 *
 *   EXCEPCION: la LECTURA del registro de status (gpuReadStatus) NO drena
 *   -- seria un sync point en cada sondeo del juego y serializaria el
 *   pipeline.  En su lugar el status es propiedad exclusiva del main
 *   thread y "GPU ocupada" se deriva de la ocupacion del ring, con una
 *   barrera acquire.  Ver el comentario de gpuReadStatus.
 *
 *   Lifecycle: HILO PERSISTENTE.  El consumidor se crea UNA vez y vive
 *   entre cargas de juego; Init/Shutdown solo lo REACTIVAN/APARCAN via
 *   s_thread_state (RUNNING = productores usan el ring, STOPPED =
 *   productores inline y consumidor idle).  Solo gpuDmaThreadDestroy
 *   (retro_deinit) lo termina de verdad.  Ver el bloque "MODELO DE CICLO
 *   DE VIDA" mas abajo: crear/destruir el hilo en cada carga colgaba la
 *   consola dentro de CreateThread de forma intermitente.
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
 * MODELO DE "GPU OCUPADA" (bits IDLE / READYFORCOMMANDS de GPUSTAT)
 *
 * El problema que resuelve esta opcion: los juegos hacen DrawSync() /
 * GPU_cw(), que es un bucle CERRADO sondeando GPUSTAT bit 26 hasta que la
 * GPU dice "idle".  Si derivamos "ocupada" de la ocupacion del ring (lo que
 * haciamos siempre), la duracion de esa espera la marca el HILO GPU del
 * HOST: la CPU emulada se queda girando en ese bucle y se GASTA su
 * presupuesto de ciclos del frame (565045 ciclos NTSC) sin hacer trabajo
 * util.  El frontend sigue viendo 60 fps (el VBlank va por ciclos, no por
 * trabajo) pero la LOGICA del juego avanza a menos de 60 Hz -> "se ve a
 * medio gas aunque el contador marque 60".
 *
 * upstream pcsx_rearmed NO hace esto: deriva "ocupada" de una fecha limite
 * en CICLOS EMULADOS (psxRegs.gpuIdleAfter, ver libpcsxcore/gpu.c y
 * psxhw.c:psxHwReadGpuSR de upstream), y encima la ACOTA — su comentario es
 * literalmente "limit because gpulib delays things with it's buffering"
 * (cap de 512 ciclos para primitivas directas).  Es decir: upstream se
 * asegura de que la velocidad real del renderer NUNCA se filtre al reloj
 * emulado.  Aqui si se filtraba.
 *
 * Modelos disponibles (core option pcsxr360_gpu_busy, hot-reload):
 *   RING    (0) comportamiento historico: ocupada mientras el ring no este
 *               vacio.  Exacto respecto al trabajo pendiente, pero sin
 *               acotar y dependiente de la velocidad del host.
 *   BOUNDED (1) ocupada solo durante GPU_BUSY_MAX_CYCLES ciclos emulados
 *               desde el ultimo encolado (y solo si queda trabajo).  Es el
 *               equivalente al gpuIdleAfter acotado de upstream.
 *   NEVER   (2) siempre idle/ready, como el GPUreadStatus pelado de gpulib
 *               (que nunca toca esos bits).  Referencia superior para medir.
 * =========================================================================== */
#define GPU_BUSY_MODEL_RING     0
#define GPU_BUSY_MODEL_BOUNDED  1
#define GPU_BUSY_MODEL_NEVER    2

/* Ventana maxima que la GPU puede figurar ocupada en el modelo BOUNDED.
 * 512 = el mismo cap que usa upstream en PGS_PRIMITIVE_START. */
#define GPU_BUSY_MAX_CYCLES     512u

/* Escrito por libretro_core.cpp (check_gpu_busy_model), leido por
 * gpuReadStatus.
 *
 * Default BOUNDED, medido en Guilty Gear (dump [GPU-BUSY], ventanas de 100M
 * ciclos): con RING el juego se dejaba entre el 16 % y el 63 % del
 * presupuesto de ciclos de cada frame girando en su DrawSync (1,5 M de
 * sondeos a GPUSTAT por ventana, el 99,8 % devolviendo "ocupada"); con
 * BOUNDED los sondeos caen a ~4.600 y el coste a 0 -- identico a NEVER,
 * porque la ventana de 512 ciclos expira antes de que el juego llegue a
 * sondear, asi que el bucle de espera no llega a formarse.
 *
 * BOUNDED en vez de NEVER porque sigue publicando el bit durante esos 512
 * ciclos: es el modelo de upstream, no una mentira permanente. */
int g_gpu_busy_model = GPU_BUSY_MODEL_BOUNDED;

/* Las dos mitades del GP1 0x04.  La del stream (DataWriteMode/DataReadMode) la
 * aplica el hilo CONSUMIDOR en orden de stream via la cola gp1q_*; la del
 * registro de status la aplica el hilo PRINCIPAL, que es su propietario.  Ver
 * el comentario largo de PEOPS_GPUsetStreamMode en plugins/xbox_soft/gpu.c. */
void PEOPS_GPUsetStreamMode(unsigned long gdata);
void PEOPS_GPUsetDMABits(unsigned long gdata);
void PEOPS_GPUsetDisplayState(unsigned long gdata);
void PEOPS_GPUsetDisplayStatusBits(unsigned long gdata);
int  PEOPS_GPUdisplayDeferrable(void);
/* Diagnostico fase 1: decide si merece la pena la espera parcial del vblank. */
unsigned long PEOPS_GPUdiagDisplayOrigin(void);

/* Ciclo emulado hasta el que la GPU figura ocupada en el modelo BOUNDED.
 * Se re-arma en cada encolado al ring (chain_enqueue). */
static u32 s_gpu_busy_until = 0;

/* === Contabilidad del coste real de la espera ===
 * Mide, en CICLOS EMULADOS, cuanto tiempo del presupuesto del frame se le
 * va a la CPU emulada girando con la GPU reportada como ocupada.  Es la
 * medida que decide si este mecanismo es o no la causa de la lentitud:
 * el dump [GPU-BUSY] de r3000a.c lo saca como % de la ventana.
 *
 * Gateado tras PCSXR_DIAG_INSTRUMENTATION: GPUSTAT se sondea en bucles
 * cerrados (cientos de miles de lecturas por segundo) y no queremos 4
 * RMW sobre volatiles por lectura en release. */
#if PCSXR_DIAG_INSTRUMENTATION
volatile u32 diag_gpu_busy_cycles   = 0;  /* ciclos emulados con busy=1 */
volatile u32 diag_gpu_busy_episodes = 0;  /* nº de esperas distintas */
volatile u32 diag_gpu_status_reads  = 0;  /* lecturas totales de 0x1814 */
volatile u32 diag_gpu_status_busy   = 0;  /* de las cuales devolvieron busy */

/* Tiempo de HOST que el hilo consumidor pasa DENTRO de GPU_writeDataMem
 * (rasterizando de verdad), y numero de veces que el productor se tuvo que
 * bloquear en ring_drain.  Con estos dos y gpu_wait_ticks se distingue:
 *   thread_busy ~= budget  -> el rasterizador ES el techo; no hay overlap
 *                             posible que lo salve (solo frameskip o un
 *                             renderer mas rapido).
 *   thread_busy << gpu_wait -> el techo es la SERIALIZACION: el productor se
 *                             bloquea en puntos de sync que podrian evitarse
 *                             (drains de GP1) y el overlap se esta perdiendo. */
volatile uint64_t diag_gpu_thread_busy_ticks = 0;   /* escribe el hilo GPU */
volatile u32      diag_gpu_drain_waits       = 0;   /* escribe el main */

/* --- Sondas del diagnostico "por que no solapaban los dos hilos" ----------
 * RESUELTO: la causa era el ring_drain() de GP1 0x04 en gpuWriteStatus, que
 * paraba el hilo emu 7,7 ms de cada frame de 18 esperando un rasterizado ya
 * en marcha (ver la cola gp1q_* mas abajo).  Con el arreglo, el menu de NFS3
 * mide exec 10,0 ms con gpu_thr 8,15 EN PARALELO: 60 fps.
 *
 * Se conservan porque son las que lo encontraron y valen para volver a
 * medirlo.  Como leerlas:
 *
 *  dexec ~= gpu_thr          -> el consumidor rasteriza mientras el emu
 *                               ejecuta: SOLAPAN.  Es el estado esperado.
 *  dexec ~= 0 con gpu_thr>0  -> el consumidor no corre durante Execute:
 *                               scheduling/afinidad de hilos HW.
 *  pend_words ~= una lista   -> el juego envia TODO al final del frame, asi
 *                               que no hay nada que solapar (medido FALSO en
 *                               NFS3: lo envia en los primeros 66 us).
 *  push_us > 0               -> el emu se bloquea en ring_push (ring lleno);
 *                               ese tiempo NO se contaba en gpu_wait.
 *
 * OJO con dexec: solo prueba solapamiento si el emu NO se bloquea dentro de
 * Execute.  Mientras existio el drain de GP1, la espera ocurria en CPU_EXEC y
 * dexec == gpu_thr sin que hubiera solapamiento ninguno.  Ese error me costo
 * varias iteraciones: para descartarlo hace falta la TRAZA temporal, no los
 * agregados. */
volatile uint64_t diag_gpu_busy_exec_ticks = 0;  /* hilo GPU, solo si el emu esta en CPU_EXEC */
volatile uint64_t diag_push_spin_ticks     = 0;  /* main, bloqueado en ring_push */
volatile u32      diag_lace_pend_words     = 0;  /* MAXIMO de palabras pendientes en el ring al llegar al vblank */
/* Histograma de drains BLOQUEANTES por comando GP1: el ring no estaba vacio
 * al llegar la escritura, asi que el emu se para a esperar al rasterizador.
 * Sirve para saber QUE comando cuesta los 8 ms en vez de deducirlo. */
volatile u32      diag_gp1_drain_cmd[32];
volatile u32      diag_gp1_04_defer = 0;   /* diferidos en orden de stream */

/* Clasificacion del vblank, tomada ANTES del drain.
 *
 *   free     -> ring vacio: no habia nada que esperar de todos modos.
 *   disp_alt -> veces que cambio el origen de display respecto al vblank
 *               anterior.  Alternar cada dos vblanks = doble bufer en VRAM y
 *               logica del juego a 30 Hz (medido asi en F1'99: 30 de 60).
 *
 * Hubo aqui un `partial`/`full` que comparaba el area de dibujo con el
 * rectangulo visible.  Era INVALIDO: ver el comentario en gpuUpdateLace(). */
volatile u32      diag_scanout_free    = 0;
volatile u32      diag_disp_alt        = 0;

/* --- TRAZA TEMPORAL (one-shot) --------------------------------------------
 * Los agregados por ventana han demostrado ser insuficientes: permiten
 * construir una explicacion plausible y equivocada (paso 4 veces). Esto
 * registra CUANDO ocurre cada cosa en los dos hilos y luego lo mezcla por
 * timestamp, para ver el hueco real en vez de deducirlo.
 *
 * Cada hilo escribe en SU PROPIO buffer: un solo escritor por buffer, asi que
 * no hacen falta atomicos ni barreras. Se mezclan al volcar, cuando ya nadie
 * escribe. One-shot: se arma en el primer frame que se pasa del presupuesto
 * (= la escena lenta) y se desarma tras volcar, para no inundar el log. */
#define TR_MAX 256
#define TR_RUN_IN  1
#define TR_RUN_OUT 2
#define TR_PUSH    3
#define TR_LACE    4
#define TR_CHUNK0  5
#define TR_CHUNK1  6
#define TR_DRAIN0  7
#define TR_DRAIN1  8

typedef struct { uint64_t t; unsigned int k; unsigned int a; } tr_ev_t;

static tr_ev_t s_tr_emu[TR_MAX];
static tr_ev_t s_tr_gpu[TR_MAX];
static uint32_t s_tr_emu_n = 0;   /* solo escribe el hilo emu */
static uint32_t s_tr_gpu_n = 0;   /* solo escribe el hilo GPU */
static volatile int s_tr_on   = 0;
static uint32_t     s_tr_laces = 0;

static const char *tr_name(unsigned int k)
{
    switch (k) {
        case TR_RUN_IN:  return "RUN_IN ";
        case TR_RUN_OUT: return "RUN_OUT";
        case TR_PUSH:    return "PUSHx  ";
        case TR_LACE:    return "VBLANK ";
        case TR_CHUNK0:  return "  raster>";
        case TR_CHUNK1:  return "  raster<";
        case TR_DRAIN0:  return "drain> ";
        case TR_DRAIN1:  return "drain< ";
    }
    return "?";
}

static void tr_emu(unsigned int k, unsigned int a)
{
    LARGE_INTEGER q;
    if (!s_tr_on || s_tr_emu_n >= TR_MAX) return;
    /* COALESCER los PUSH consecutivos: el juego envia su display list en una
     * rafaga de ~250 pushes en 66 us, y eso llenaba el buffer entero dejando
     * fuera el VBLANK y los wait>.  Un solo evento acumulando el
     * total de palabras dice lo mismo y deja sitio a lo que importa. */
    if (k == TR_PUSH && s_tr_emu_n > 0 &&
        s_tr_emu[s_tr_emu_n - 1].k == TR_PUSH) {
        s_tr_emu[s_tr_emu_n - 1].a += a;   /* palabras acumuladas */
        return;
    }
    QueryPerformanceCounter(&q);
    s_tr_emu[s_tr_emu_n].t = (uint64_t)q.QuadPart;
    s_tr_emu[s_tr_emu_n].k = k;
    s_tr_emu[s_tr_emu_n].a = a;
    s_tr_emu_n++;
}

static void tr_gpu(unsigned int k, unsigned int a)
{
    LARGE_INTEGER q;
    if (!s_tr_on || s_tr_gpu_n >= TR_MAX) return;
    QueryPerformanceCounter(&q);
    s_tr_gpu[s_tr_gpu_n].t = (uint64_t)q.QuadPart;
    s_tr_gpu[s_tr_gpu_n].k = k;
    s_tr_gpu[s_tr_gpu_n].a = a;
    s_tr_gpu_n++;
}

/* Arma la traza (la llama retro_run en el primer frame fuera de presupuesto). */
void diag_trace_arm(void)
{
    if (s_tr_on || s_tr_laces) return;   /* ya activa, o ya volcada */
    s_tr_emu_n = 0;
    s_tr_gpu_n = 0;
    s_tr_on    = 1;
}

/* Vuelca la traza mezclada por timestamp.  Se llama desde el hilo emu con la
 * captura ya detenida, asi que leer los dos buffers es seguro. */
static void tr_dump(void)
{
    LARGE_INTEGER freq;
    uint64_t t0;
    uint32_t i = 0, j = 0;
    int n = 0;

    s_tr_on = 0;   /* parar la captura ANTES de leer */
    QueryPerformanceFrequency(&freq);
    if (freq.QuadPart == 0 || (s_tr_emu_n == 0 && s_tr_gpu_n == 0)) return;

    t0 = (s_tr_emu_n && (!s_tr_gpu_n || s_tr_emu[0].t < s_tr_gpu[0].t))
       ? s_tr_emu[0].t : s_tr_gpu[0].t;

    pcsxr_log(RETRO_LOG_INFO,
        "[TRACE] inicio (emu=%u ev, gpu=%u ev). us relativos; "
        "EMU sin sangrar, GPU sangrado.\n",
        (unsigned)s_tr_emu_n, (unsigned)s_tr_gpu_n);

    while ((i < s_tr_emu_n || j < s_tr_gpu_n) && n < 2 * TR_MAX) {
        int take_emu = (i < s_tr_emu_n) &&
                       (j >= s_tr_gpu_n || s_tr_emu[i].t <= s_tr_gpu[j].t);
        const tr_ev_t *e = take_emu ? &s_tr_emu[i] : &s_tr_gpu[j];
        uint64_t us = (e->t - t0) * 1000000ULL / (uint64_t)freq.QuadPart;
        pcsxr_log(RETRO_LOG_INFO, "[TRACE] %6u %s %u\n",
                  (unsigned)us, tr_name(e->k), (unsigned)e->a);
        if (take_emu) i++; else j++;
        n++;
    }
    pcsxr_log(RETRO_LOG_INFO, "[TRACE] fin\n");
}

/* Marca de retro_run (la llama libretro_core.cpp). */
void diag_trace_mark(int kind)
{
    tr_emu((unsigned int)kind, 0);
}
static   u32 s_busy_episode_start   = 0;
static   int s_busy_episode_active  = 0;
#endif

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


/* --- Cola de GP1 diferidos, aplicados EN ORDEN DE STREAM -------------------
 * El problema: GP1 0x04 cambia DataWriteMode/DataReadMode, que gobiernan como
 * interpreta el CONSUMIDOR las palabras del ring (datos de VRAM o comandos).
 * Aplicarlo desde el hilo principal exige que el consumidor haya terminado
 * TODO lo pendiente, o le cambiamos el modo a mitad de la cola.  Eso era el
 * ring_drain(), y medido en el menu de NFS3 costaba 7,7 ms de cada frame de
 * 18: el juego hace GP1(04)=2 -> DMA de bloque -> GP1(04)=0, y el `=0`
 * posterior esperaba a que se rasterizara el frame entero.
 *
 * La observacion: la transicion solo tiene que ser visible para las palabras
 * empujadas DESPUES de ella.  Eso es ORDEN, no sincronizacion.  Asi que en vez
 * de drenar, se encola {posicion_del_ring, data} y el consumidor la aplica
 * justo al llegar a esa posicion.
 *
 * SPSC: el productor solo escribe s_gp1q_w, el consumidor solo s_gp1q_r.
 * Si la cola se llena, fallback conservador: drenar como antes. */
#define GP1Q_SIZE 64                      /* potencia de 2; ~3/frame, sobra */
#define GP1Q_MASK (GP1Q_SIZE - 1u)

static volatile uint32_t s_gp1q_pos[GP1Q_SIZE];
static volatile uint32_t s_gp1q_data[GP1Q_SIZE];
static volatile uint32_t s_gp1q_w = 0;    /* solo escribe el hilo emu */
static volatile uint32_t s_gp1q_r = 0;    /* solo escribe el hilo GPU */

/* Aplica un GP1 diferido.  Cada comando tiene su "mitad de consumidor": el
 * 0x04 el modo de stream, el 0x05-0x08 la geometria de display.  La mitad del
 * hilo principal ya se aplico en gpuWriteStatus, al encolar. */
static void gp1q_run(uint32_t data)
{
    if (((data >> 24) & 0xFFu) == 0x04u)
        PEOPS_GPUsetStreamMode((unsigned long)data);
    else
        PEOPS_GPUsetDisplayState((unsigned long)data);
}

/* Consumidor: aplica los GP1 cuyo punto de stream ya se ha alcanzado y
 * devuelve cuantas palabras se pueden procesar sin cruzar el siguiente
 * (0xFFFFFFFF = sin limite).  rpos es la posicion absoluta de lectura. */
static uint32_t gp1q_apply(uint32_t rpos)
{
    while (s_gp1q_r != s_gp1q_w) {
        uint32_t idx = s_gp1q_r & GP1Q_MASK;
        /* Acquire: va AQUI, tras ver el indice y ANTES de leer pos/data.
         * El productor publica pos y data y luego w con release; en PPC las
         * cargas se reordenan, asi que sin esta barrera podriamos ver el w
         * nuevo con el pos VIEJO, y pos es lo que decide cuando se aplica el
         * modo y donde se recorta el chunk. */
        __lwsync();
        if ((int32_t)(s_gp1q_pos[idx] - rpos) > 0)
            break;                        /* aun no hemos llegado a su punto */
        gp1q_run(s_gp1q_data[idx]);
        s_gp1q_r++;
    }
    if (s_gp1q_r != s_gp1q_w) {
        __lwsync();
        return s_gp1q_pos[s_gp1q_r & GP1Q_MASK] - rpos;
    }
    return 0xFFFFFFFFu;
}

/* Productor: encola el GP1 para aplicarlo en la posicion actual del ring.
 * Devuelve 0 si la cola esta llena (el llamante debe drenar y aplicarlo el
 * mismo, como antes). */
static int gp1q_push(u32 data)
{
    uint32_t w = s_gp1q_w;
    if ((uint32_t)(w - s_gp1q_r) >= GP1Q_SIZE)
        return 0;
    s_gp1q_pos[w & GP1Q_MASK]  = s_ring_wpos;
    s_gp1q_data[w & GP1Q_MASK] = (uint32_t)data;
    __lwsync();                           /* release: data antes del indice */
    s_gp1q_w = w + 1;
    return 1;
}

/* Descarta los GP1 pendientes: sus posiciones apuntan a un ring que ya no
 * significa nada (reset / carga de juego / load state). */
static void gp1q_reset(void)
{
    s_gp1q_r = s_gp1q_w;
}


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
#if PCSXR_DIAG_INSTRUMENTATION
    int spun = 0;
    LARGE_INTEGER pt0, pt1;
#endif

    /* Esperar espacio.  Capacity - used >= size.  En SPSC con cursores
     * uint32 sin wrap explicito (unsigned arithmetic wrap-safe),
     * used = wpos - rpos.  Spin con yield si no cabe.
     *
     * OJO: este bloqueo NO se contaba en gpu_wait_ticks (que solo mira
     * ring_drain), asi que aparecia disfrazado de "trabajo de CPU" en el
     * exec de [RR-PERF].  diag_push_spin_ticks lo saca a la luz. */
    for (;;) {
        uint32_t rpos = s_ring_rpos;
        uint32_t used = wpos - rpos;
        if (RING_SIZE - used >= size) break;
#if PCSXR_DIAG_INSTRUMENTATION
        if (!spun) { spun = 1; QueryPerformanceCounter(&pt0); }
        s_ring_push_spin_count++;
#endif
        YieldProcessor();
    }
#if PCSXR_DIAG_INSTRUMENTATION
    if (spun) {
        QueryPerformanceCounter(&pt1);
        diag_push_spin_ticks += (uint64_t)(pt1.QuadPart - pt0.QuadPart);
    }
#endif
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
#if PCSXR_DIAG_INSTRUMENTATION
    tr_emu(TR_PUSH, size);
#endif

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
    LARGE_INTEGER t0, t1, stall_ref, now;
    uint32_t spins = 0;
    uint32_t last_rpos;
    static LARGE_INTEGER s_qpf = {0};
    static int64_t       s_stall_ticks = 0;

    /* Si no hay thread (modo NO_THREADING o pre-init/post-shutdown), no
     * hay nada que drenar.  Y el ring nunca se uso. */
    if (s_thread_state != GPU_THREAD_RUNNING)
        return;

    /* Fast path: ring vacio Y sin GP1 diferidos pendientes.  Coste cero comun.
     * Las dos condiciones son necesarias: ver el bucle de abajo. */
    if (s_ring_wpos == s_ring_rpos && s_gp1q_r == s_gp1q_w)
        return;

    /* Umbral de liveness (lazy-init): ~5 s SIN progreso de rpos = el
     * consumidor esta colgado (bucle infinito en un comando GP0; los
     * fallos ya los contiene el SEH del consumidor).  Solo diagnostico:
     * en XDK no hay recuperacion segura (no TerminateThread, no se puede
     * distinguir "lento" de "muerto" ni tomar el relevo sin carrera). */
    if (s_qpf.QuadPart == 0) {
        QueryPerformanceFrequency(&s_qpf);
        s_stall_ticks = s_qpf.QuadPart * 5;
    }

    QueryPerformanceCounter(&t0);
#if PCSXR_DIAG_INSTRUMENTATION
    diag_gpu_drain_waits++;   /* solo el slow path: el ring NO estaba vacio */
    tr_emu(TR_DRAIN0, s_ring_wpos - s_ring_rpos);
#endif
    /* Esperamos DOS cosas, no una:
     *
     *   (a) que el ring quede vacio, y
     *   (b) que el consumidor haya aplicado los GP1 diferidos (gp1q_*).
     *
     * (b) NO se deduce de (a): el consumidor aplica la cola al principio de
     * su vuelta del bucle, o sea en la vuelta SIGUIENTE a la que vacio el
     * ring.  Si solo esperasemos (a), quien llama despues a un GPU_* DIRECTO
     * desde este hilo (gpuReadData / gpuReadDataMem / freeze) leeria un
     * DataWriteMode/DataReadMode viejo y interpretaria mal el stream: eso son
     * las corrupciones de las pantallas de carga de NFS3 y Colin McRae.
     * Upstream tiene la misma regla: gpu_async_sync() de pcsx_rearmed cierra
     * con assert(pos_added == pos_used) Y assert(idle), dos condiciones.
     *
     * El progreso para el watchdog es la pareja (rpos, gp1q_r): con el ring
     * ya vacio, lo que avanza es la cola. */
    last_rpos = s_ring_rpos + s_gp1q_r;
    stall_ref = t0;
    while (s_ring_wpos != s_ring_rpos || s_gp1q_r != s_gp1q_w) {
#if PCSXR_DIAG_INSTRUMENTATION
        s_ring_drain_spin_count++;
#endif
        YieldProcessor();

        /* Chequeo de liveness barato: cada 65536 vueltas.  Si rpos avanza,
         * el consumidor esta vivo (solo lento) -> re-armar.  Si no avanza
         * durante el umbral -> loguear (rate-limited), sin tocar estado. */
        if (((++spins) & 0xFFFFu) == 0u) {
            uint32_t cur = s_ring_rpos + s_gp1q_r;
            if (cur != last_rpos) {
                last_rpos = cur;
                QueryPerformanceCounter(&stall_ref);
            } else {
                QueryPerformanceCounter(&now);
                if ((int64_t)(now.QuadPart - stall_ref.QuadPart) > s_stall_ticks) {
                    static volatile uint32_t s_stall_logs = 0;
                    if ((s_stall_logs++ & 0x7u) == 0u)
                        pcsxr_log(RETRO_LOG_ERROR,
                            "[PCSXR-LR] GPU consumer stalled: rpos frozen >5s (ring=%u/%u). Main sigue en spin.\n",
                            (unsigned)s_ring_wpos, (unsigned)s_ring_rpos);
                    stall_ref = now;   /* re-armar para no re-loguear cada iter */
                }
            }
        }
    }
    /* Acquire: ver psxVuw/state writes hechos por el thread antes de rpos. */
    __lwsync();
    QueryPerformanceCounter(&t1);
    gpu_wait_ticks += (uint64_t)(t1.QuadPart - t0.QuadPart);
#if PCSXR_DIAG_INSTRUMENTATION
    tr_emu(TR_DRAIN1, 0);
#endif
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

    uint32_t idle_iters = 0;   /* backoff del path idle (ver abajo) */

    while (s_thread_state != GPU_THREAD_STOPPING) {
        uint32_t wpos, rpos, used, ridx, chunk;

        wpos = s_ring_wpos;
        /* Acquire: ver el payload publicado antes de wpos. */
        __lwsync();
        rpos = s_ring_rpos;
        used = wpos - rpos;

        /* GP1 diferidos: aplicar los que ya toca y no cruzar el siguiente.
         * Va antes de procesar el chunk porque un cambio de modo pendiente
         * afecta a como se interpretan las palabras que vienen justo despues,
         * incluidas las del frame que estamos cerrando. */
        {
            uint32_t to_gp1 = gp1q_apply(rpos);
            if (used > to_gp1)
                used = to_gp1;
        }

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
            /* Backoff adaptativo.  El hilo es PERSISTENTE (vive entre cargas
             * de juego, ver gpuDmaThreadInit), asi que no puede quemar HW2 al
             * 100% eternamente: en menus / entre juegos no hay trabajo.
             * Spin duro mientras el trabajo fluye (latencia minima, que es
             * todo el punto del solapamiento), y cedemos CPU solo tras un
             * buen rato sin nada que hacer.  En gameplay el ring recibe
             * trabajo constantemente, asi que practicamente nunca se llega
             * al Sleep. */
            if (++idle_iters < 200000u) {
                YieldProcessor();
            } else {
                Sleep(1);
            }
            continue;
        }

#if PCSXR_DIAG_INSTRUMENTATION
        /* Ring tiene trabajo, resetear watchdog. */
        s_idle_iters = 0;
        s_last_cycle = psxRegs.cycle;
#endif

        idle_iters = 0;   /* hay trabajo: volver a spin duro (latencia minima) */

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
#if PCSXR_DIAG_INSTRUMENTATION
        {
            /* [GPU-CHUNK] Instrumentacion temporal: detectar chunks de
             * GPU_writeDataMem que tarden mucho.  Sospechoso para cuelgues
             * tipo TOCA donde main spinea en ring_push 3.29B veces durante
             * un retro_run >1min.  Si un comando concreto del PSX (ej.
             * primera operacion de drawing area / texture upload) tarda
             * 138s, este log lo captura con la primera palabra del data
             * (= comando GP0/GP1) para identificarlo.  Ademas calcula el
             * delta de contadores definidos en xbox_soft/gpu.c
             * (g_xbox_soft_fastpath_words / slow_pixel_words /
             *  primfunc_calls) para saber exactamente donde se gasta el
             * tiempo del chunk: copia rapida de pixels, slow path
             * pixel-a-pixel o rasterizado de polygons. */
            static LARGE_INTEGER s_gpu_chunk_freq = {0};
            static unsigned int s_last_fastpath_words   = 0;
            static unsigned int s_last_slow_pixel_words = 0;
            static unsigned int s_last_primfunc_calls   = 0;
            extern volatile unsigned int g_xbox_soft_fastpath_words;
            extern volatile unsigned int g_xbox_soft_slow_pixel_words;
            extern volatile unsigned int g_xbox_soft_primfunc_calls;
            const uint32_t GPU_CHUNK_WARN_MS = 50;
            LARGE_INTEGER ct0, ct1;
            uint32_t first_word;
            uint64_t chunk_us;
            unsigned int d_fast, d_slow, d_prim;
            unsigned int cur_fast, cur_slow, cur_prim;

            if (s_gpu_chunk_freq.QuadPart == 0)
                QueryPerformanceFrequency(&s_gpu_chunk_freq);
            first_word = s_ring_data[ridx];
            tr_gpu(TR_CHUNK0, (unsigned int)chunk);
            QueryPerformanceCounter(&ct0);
            GPU_writeDataMem(&s_ring_data[ridx], chunk);
            QueryPerformanceCounter(&ct1);
            tr_gpu(TR_CHUNK1, (unsigned int)chunk);
            diag_gpu_thread_busy_ticks += (uint64_t)(ct1.QuadPart - ct0.QuadPart);
            if (retro_run_section == RR_SEC_CPU_EXEC)
                diag_gpu_busy_exec_ticks += (uint64_t)(ct1.QuadPart - ct0.QuadPart);
            chunk_us = (uint64_t)((ct1.QuadPart - ct0.QuadPart)
                                 * 1000000LL / s_gpu_chunk_freq.QuadPart);

            cur_fast = g_xbox_soft_fastpath_words;
            cur_slow = g_xbox_soft_slow_pixel_words;
            cur_prim = g_xbox_soft_primfunc_calls;
            d_fast   = cur_fast - s_last_fastpath_words;
            d_slow   = cur_slow - s_last_slow_pixel_words;
            d_prim   = cur_prim - s_last_primfunc_calls;
            s_last_fastpath_words   = cur_fast;
            s_last_slow_pixel_words = cur_slow;
            s_last_primfunc_calls   = cur_prim;

            if (chunk_us >= GPU_CHUNK_WARN_MS * 1000ULL) {
                static unsigned int s_slow_chunk_count = 0;
                pcsxr_log(RETRO_LOG_DEBUG,
                    "[GPU-CHUNK] slow: words=%u first=0x%08X (gp0_cmd=0x%02X) "
                    "took %u ms | dFast=%u dSlow=%u dPrim=%u\n",
                    (unsigned)chunk,
                    (unsigned)first_word,
                    (unsigned)((first_word >> 24) & 0xFF),
                    (unsigned)(chunk_us / 1000ULL),
                    d_fast, d_slow, d_prim);
                s_slow_chunk_count++;

                /* Cada 16 slow chunks, dump del top-8 comandos GP0 por
                 * ticks acumulados.  Identifica QUE primitiva concreta
                 * domina el tiempo: si hay un solo cmd al 90%, ataque
                 * dirigido.  Si la distribucion es plana, el problema
                 * esta en el dispatcher o el threading. */
                if ((s_slow_chunk_count & 0xF) == 0)
                    gpuDumpCmdHist();
            }
        }
#else
        /* Contencion de fallo del consumidor.  Si un comando GP0 provoca
         * una excepcion (AV, etc.) NO debemos dejar rpos congelado: eso
         * colgaria PARA SIEMPRE al productor en ring_drain/ring_push (el
         * agujero de robustez del diseño previo).  Con SEH (table-based en
         * Xbox360 PPC = coste cero en el camino sin fallo) capturamos, lo
         * logueamos y caemos abajo a avanzar rpos igual, manteniendo el
         * ring fluyendo.  Best-effort: el parser del plugin resincroniza en
         * el siguiente GPU reset (GP1). */
        __try {
            GPU_writeDataMem(&s_ring_data[ridx], chunk);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            static volatile uint32_t s_consumer_faults = 0;
            if ((s_consumer_faults++ & 0x3Fu) == 0u)
                pcsxr_log(RETRO_LOG_ERROR,
                    "[PCSXR-LR] GPU consumer exception in GPU_writeDataMem (chunk=%u words, fault #%u); continuing inline-safe\n",
                    (unsigned)chunk, (unsigned)s_consumer_faults);
        }
#endif

        /* Release: writes del GPU (psxVuw, state) visibles antes de
         * publicar rpos.  El main thread en ring_drain hace lwsync
         * acquire para verlas.  Se ejecuta TAMBIEN tras una excepcion
         * capturada arriba, para que rpos siempre avance. */
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
			/* Re-armar la ventana de "ocupada" del modelo BOUNDED: el
			 * trabajo acaba de entrar, asi que la GPU esta ocupada desde
			 * AHORA (en ciclos emulados) y como maximo durante
			 * GPU_BUSY_MAX_CYCLES.  Equivalente a lo que hace upstream en
			 * gpu_state_change(PGS_PRIMITIVE_START). */
			s_gpu_busy_until = psxRegs.cycle + GPU_BUSY_MAX_CYCLES;
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

#if PCSXR_DIAG_INSTRUMENTATION
/* Contadores GPU port writes definidos en r3000a.c, declarados aqui como
 * extern para incrementar desde gpuWriteData/gpuWriteStatus.  El primer
 * write engatilla un log puntual para marcar el momento en que el BIOS /
 * juego empieza a usar el GPU (util para diagnosticar pantallas negras). */
extern volatile uint32_t diag_gpu_data_writes;
extern volatile uint32_t diag_gpu_status_writes;
extern volatile uint32_t diag_gpu_first_write_seen;
#endif

void gpuWriteData(u32 data)
{
#if PCSXR_DIAG_INSTRUMENTATION
    diag_gpu_data_writes++;
    if (!diag_gpu_first_write_seen) {
        diag_gpu_first_write_seen = 1;
        pcsxr_log(RETRO_LOG_DEBUG,
            "[GPU-IO] FIRST GPU write seen: data=0x%08x\n", (unsigned)data);
    }
#endif
    ring_drain();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_WRITE_DATA);
    GPU_writeData(data);
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);
}

void gpuWriteStatus(u32 data)
{
#if PCSXR_DIAG_INSTRUMENTATION
    diag_gpu_status_writes++;
    if (!diag_gpu_first_write_seen) {
        diag_gpu_first_write_seen = 1;
        pcsxr_log(RETRO_LOG_DEBUG,
            "[GPU-IO] FIRST GPU write seen: status=0x%08x\n", (unsigned)data);
    }
#endif
    /* Status writes (GP1): modo de display, direccion de DMA, reset...
     *
     * Regla: lo que toca estado del CONSUMIDOR no se drena, se DIFIERE en
     * orden de stream (cola gp1q_*); el resto drena.
     *
     *   0x04 (direccion de DMA)      -> mitad diferida: el modo de stream
     *        (DataWriteMode/DataReadMode), que decide si las palabras que
     *        vienen detras son datos de VRAM o comandos.
     *   0x05/0x06/0x07/0x08 (display) -> mitad diferida: la geometria
     *        (PSXDisplay/PreviousPSXDisplay), que el rasterizador lee en cada
     *        primitiva (prim.c: DrawOffset, DisplayPosition/DisplayEnd).
     *
     * En ambos casos la mitad del REGISTRO DE STATUS se aplica aqui, que es su
     * propietario (escribirlo desde el consumidor fue el cuelgue del FMV de
     * Silent Hill).  Ver los comentarios de PEOPS_GPUsetStreamMode y
     * PEOPS_GPUsetDisplayState en el plugin.
     *
     * Historia, porque me equivoque dos veces en direcciones opuestas:
     *   - Con LISTA BLANCA (dejar pasar los de display sin ordenar nada) ->
     *     corrupcion en NFS3 y en las cargas de Colin McRae: el hilo principal
     *     reescribia PSXDisplay mientras el consumidor rasterizaba leyendolo.
     *   - DRENANDO todos -> correcto pero caro: [TRACE] en carrera de F1'99
     *     mostro un drain de 10,4 ms dentro de un frame de 24, esperando a que
     *     el rasterizador vaciase 13.471 palabras.  ~30 de cada 60 frames
     *     fuera de presupuesto.
     * Diferir da las dos cosas, y es lo que hace upstream: gpulib no drena en
     * el 0x05/0x08, encola un FAKECMD_SCREEN_CHANGE en su propio ring
     * (gpu_async_notify_screen_change) y el renderer lo aplica al llegar.
     *
     * Sigue DRENANDO todo lo demas, y en particular 0x10-0x1F (info): los
     * sub-comandos 0x02-0x06 devuelven lGPUInfoVals[INFO_TW/DRAWSTART/DRAWEND/
     * DRAWOFF], que escribe prim.c desde los handlers de GP0 0xE1-0xE5, o sea
     * en el CONSUMIDOR.  Sin drain, el main saca una foto vieja a lGPUdataRet.
     * Upstream trata ese estado con su propio sync dedicado
     * (gpu_async_sync_ecmds / renderer_sync_ecmds), lo que confirma que
     * necesita barrera.  Nunca lo hemos visto bloquear en el histograma. */
    {
        const u32 cmd = (data >> 24) & 0xFFu;
        /* Hay algo por delante contra lo que ordenar?  Si el ring esta vacio y
         * no queda ningun GP1 diferido, el consumidor esta quiescente (publica
         * rpos DESPUES de rasterizar, con release), asi que no hay nada que
         * ordenar: aplicamos las dos mitades aqui mismo y listo.
         *
         * Esto NO es cosmetico.  Diferir con el ring vacio cuesta la latencia
         * de DESPERTAR al consumidor, que en las pantallas de carga esta en
         * Sleep(1) porque el juego casi no dibuja.  Medido en la carga de NFS3:
         * gpu_wait 80 ms/frame con gpu_thr=0,00 y drains == 04-diferidos
         * exactamente.  Con el ring lleno (el menu, el caso que motivo la
         * cola) sigue difiriendo y sigue ganando los 8 ms.
         *
         * Upstream tiene el mismo atajo, con esas palabras:
         * gpu_async_notify_screen_change() aplica en linea si
         * `idle && pos_added == pos_used`. */
        const int pending = (s_ring_wpos != s_ring_rpos) ||
                            (s_gp1q_r != s_gp1q_w);
        /* Diferible?  El 0x04 siempre; los de display solo si el plugin no va
         * a presentar desde dentro (frameskip/fast-forward, ver
         * PEOPS_GPUdisplayDeferrable). */
        const int can_defer = (cmd == 0x04u) ||
                              ((cmd >= 0x05u && cmd <= 0x08u) &&
                               PEOPS_GPUdisplayDeferrable());
        int deferred = 0;
        int need_drain = 1;

        /* Diferir exige ademas que HAYA consumidor: si nadie drena el ring
         * (modo sin threading, o pre-init / post-shutdown) la mitad diferida
         * no se aplicaria nunca. */
        if (can_defer && pending &&
            s_thread_state == GPU_THREAD_RUNNING &&
            gp1q_push(data)) {
            /* Mitad del hilo principal, AHORA (el orden entre las dos mitades
             * no importa: no comparten variables, por eso se pudieron partir). */
            if (cmd == 0x04u)
                PEOPS_GPUsetDMABits((unsigned long)data);
            else
                PEOPS_GPUsetDisplayStatusBits((unsigned long)data);
            need_drain = 0;
            deferred   = 1;
#if PCSXR_DIAG_INSTRUMENTATION
            diag_gp1_04_defer++;
#endif
        }

        if (need_drain) {
#if PCSXR_DIAG_INSTRUMENTATION
            /* Contar solo los drains que de verdad BLOQUEAN (ring no vacio):
             * son los que cuestan los ~8 ms. */
            if (s_ring_wpos != s_ring_rpos)
                diag_gp1_drain_cmd[cmd & 31u]++;
#endif
            ring_drain();
        }

        if (deferred)
            return;            /* el consumidor hara la parte del stream */
    }
    DIAG_SET_PLUGIN_CALL(GPU_CALL_WRITE_STATUS);
    GPU_writeStatus(data);
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);
}


#if PCSXR_DIAG_INSTRUMENTATION
/* Top-8 comandos GP0 por tiempo acumulado en el rasterizador.
 *
 * Estaba EMPOTRADO en el camino de "chunk lento" (umbral 50 ms, y solo 1 de
 * cada 16), asi que en F1'99 no salia nunca: sus chunks son de ~11 ms.  Y es
 * justo el dato que hace falta cuando el techo es el rasterizador, no la
 * sincronizacion.  Extraido a funcion para poder volcarlo por ventana.
 *
 * Requiere PCSXR_DIAG_PRIMFUNC_TIMING=1 en el plugin; con 0 los ticks son
 * cero y solo sale total_calls. */
void gpuDumpCmdHist(void)
{
    extern volatile unsigned int g_xbox_soft_primfunc_cmd_calls[256];
    extern volatile uint64_t     g_xbox_soft_primfunc_cmd_ticks[256];
    extern volatile uint64_t     g_xbox_soft_qpc_freq;
    /* Selection-sort top-8 sobre indices [0..255] por ticks. */
    int top_idx[8];
    uint64_t top_ticks[8];
    uint64_t total_ticks = 0;
    unsigned int total_calls = 0;
    int k, j, b;
    for (k = 0; k < 8; k++) { top_idx[k] = -1; top_ticks[k] = 0; }
    for (k = 0; k < 256; k++) {
        uint64_t t = g_xbox_soft_primfunc_cmd_ticks[k];
        total_ticks += t;
        total_calls += g_xbox_soft_primfunc_cmd_calls[k];
        /* insertion en top-8 */
        for (j = 0; j < 8; j++) {
            if (t > top_ticks[j]) {
                for (b = 7; b > j; b--) {
    top_ticks[b] = top_ticks[b-1];
    top_idx[b]   = top_idx[b-1];
                }
                top_ticks[j] = t;
                top_idx[j]   = k;
                break;
            }
        }
    }
    /* Si el plugin se compilo con PCSXR_DIAG_PRIMFUNC_TIMING=0
     * los ticks son 0 y solo sale total_calls: es lo esperado,
     * no un bug (esa sonda mete 2 QPC por primitiva en el hilo
     * consumidor y falsea justo lo que se quiere medir). */
    pcsxr_log(RETRO_LOG_DEBUG,
        "[CMD-HIST] total_calls=%u total_ms=%u (top-8 by ticks):\n",
        total_calls,
        (unsigned)(g_xbox_soft_qpc_freq
                   ? (unsigned)((total_ticks * 1000ULL) / g_xbox_soft_qpc_freq)
                   : 0u));
    for (k = 0; k < 8; k++) {
        unsigned int cmd, calls;
        uint64_t ms, pct;
        if (top_idx[k] < 0 || top_ticks[k] == 0) break;
        cmd   = (unsigned int)top_idx[k];
        calls = g_xbox_soft_primfunc_cmd_calls[cmd];
        ms    = g_xbox_soft_qpc_freq
                ? (top_ticks[k] * 1000ULL) / g_xbox_soft_qpc_freq
                : 0;
        pct   = total_ticks ? (top_ticks[k] * 100ULL) / total_ticks : 0;
        pcsxr_log(RETRO_LOG_DEBUG,
            "[CMD-HIST]   cmd=0x%02X calls=%u total=%u ms (%u%%) avg=%u us/call\n",
            cmd, calls,
            (unsigned)ms, (unsigned)pct,
            calls ? (unsigned)((ms * 1000ULL) / calls) : 0u);
    }
}
#endif /* PCSXR_DIAG_INSTRUMENTATION */

void gpuUpdateLace(void)
{
#if PCSXR_DIAG_INSTRUMENTATION
    /* Marca de frontera de frame.  El argumento son las palabras pendientes
     * en el ring al llegar al vblank. */
    tr_emu(TR_LACE, s_ring_wpos - s_ring_rpos);
    if (s_tr_on && ++s_tr_laces >= 3) tr_dump();
    {   /* MAXIMO de la ventana: el snapshot del ultimo vblank puede no ser
         * representativo (el juego no envia lo mismo en todos los frames). */
        u32 pend_now = s_ring_wpos - s_ring_rpos;
        if (pend_now > diag_lace_pend_words) diag_lace_pend_words = pend_now;
    }
#endif
#if PCSXR_DIAG_INSTRUMENTATION
    /* Clasificacion del vblank.  Solo quedan `free` (ring vacio) y `disp_alt`
     * (cambios de origen de display): ambos se leen del lado del emulador y son
     * fiables.
     *
     * El test de solape area-de-dibujo / rectangulo visible que habia aqui
     * ESTABA MAL Y NO SE PUEDE ARREGLAR desde este hilo: leia drawX/Y/W/H, que
     * los pone el CONSUMIDOR, y en el vblank el consumidor lleva 14.000 palabras
     * de retraso -- justo el caso que interesaba medir.  Salia (0,0)-(0,0)
     * siempre.  Para responder esa pregunta hace falta el historial de areas
     * anotado por el PRODUCTOR al empujar, como el draw_areas[] de upstream, y
     * eso exige un walker de longitudes GP0 en el lado del emulador. */
    {
        static unsigned long s_prev_disp_origin = ~0UL;
        unsigned long        origin = PEOPS_GPUdiagDisplayOrigin();

        if (s_ring_wpos == s_ring_rpos) diag_scanout_free++;

        if (s_prev_disp_origin != ~0UL && origin != s_prev_disp_origin)
            diag_disp_alt++;
        s_prev_disp_origin = origin;
    }
#endif
    /* VBlank: BlitScreen32 (dentro de GPU_updateLace) lee psxVuw que el
     * thread modifica.  Drain primero para garantizar que el frame esta
     * completo antes de presentarlo.
     *
     * Este drain protege TRES cosas, no solo los pixeles:
     *   1. psxVuw es un bufer unico: el consumidor escribe, este hilo lo lee
     *      en BlitScreen16/32.
     *   2. La geometria del blit -- PSXDisplay.DisplayMode/DisplayPosition y
     *      PreviousPSXDisplay.Range (draw_ok.c) -- la escribe el CONSUMIDOR,
     *      via la cola gp1q.  El drain espera tambien a la cola.
     *   3. bDoVSyncUpdate: lo pone el consumidor (~30 sitios de prim.c) y lo
     *      lee PEOPS_GPUupdateLace para decidir SI PRESENTA.  Su visibilidad
     *      depende del __lwsync acquire de ring_drain.
     *
     * NO es barato, al contrario de lo que decia este comentario antes: en
     * carrera de F1'99 son 4-6 ms de cada frame de 16, con 13.000-18.000
     * palabras pendientes al llegar aqui.  En NFS3 en cambio son 1-2 ms.
     *
     * La traza [TRACE] mide de donde sale ese coste, y NO es que falte
     * capacidad -- es puramente el sitio donde esta la barrera.  F1'99 corre
     * su logica a 30 Hz y alterna dos tipos de frame:
     *
     *   ligero: 11,5 ms de CPU y el ring VACIO al llegar al vblank; el
     *           rasterizador esta parado todo ese rato.
     *   pesado: 9,8 ms de CPU y AL FINAL empuja las ~14.700 palabras de la
     *           lista entera; el vblank cae 0,9 ms despues del push, asi que
     *           el drain se come los 15 ms de rasterizado de golpe.
     *
     * O sea: hay 11,5 ms de rasterizador ocioso justo DESPUES del frame que
     * necesita 15 ms de rasterizado.  Solapar contra esa ventana es lo que
     * gana la espera parcial; el techo no es el trabajo (CPU ~12 ms/frame,
     * rasterizador ~5 ms/frame de media, ninguno pasa de 16,67) sino esta
     * serializacion.
     *
     * Recortarlo exige la espera PARCIAL de upstream (`calc_scanout_wait`) y
     * resolver los puntos 2 y 3; la fase 1 de arriba mide si compensa. */
    ring_drain();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_UPDATE_LACE);
    GPU_updateLace();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);
}

/* Descarta el estado diferido (reset / carga de juego / load state): los GP1
 * en cola apuntan a posiciones de un ring que ya no significa nada, y
 * aplicarlos luego pondria un modo de stream arbitrario. */
void gpuDiscardDeferred(void)
{
    /* Drenar ANTES de tocar la cola: gp1q_reset escribe s_gp1q_r, que es del
     * CONSUMIDOR, y esto se llama con el hilo vivo (retro_unserialize).  Tras
     * el drain la cola ya esta vacia y el reset solo deja el estado explicito. */
    ring_drain();
    gp1q_reset();
}

/* Lectura del registro de STATUS del GPU (0x1814).  NO BLOQUEANTE.
 *
 * Modelo (el de pcsx_rearmed/gpulib: su hilo de render, gpu_async.c, no
 * contiene ni una referencia a `status`):
 *
 *  1. El registro de status es propiedad del HILO PRINCIPAL.  El consumidor
 *     ya no lo toca (se quitaron GPUIsBusy/GPUIsIdle de
 *     PEOPS_GPUwriteDataMem; ver la nota [THREADING] alli).
 *
 *  2. "GPU ocupada" se DERIVA de la ocupacion del ring: mientras queden
 *     comandos sin consumir, la GPU no ha terminado -> IDLE y
 *     READYFORCOMMANDS a 0.  Es exacto, no una aproximacion, y no requiere
 *     esperar a nadie: se conserva el solapamiento CPU/GPU.
 *
 *  3. Barrera ACQUIRE (__lwsync) SIEMPRE antes de leer.  Empareja con el
 *     release que el consumidor hace al publicar s_ring_rpos, de modo que
 *     todo lo que el consumidor haya completado (incluido el bit
 *     GPUSTATUS_READYFORVRAM que publica primStoreImage) es visible aqui.
 *     Este era el hueco real de la version anterior: la barrera solo estaba
 *     en el camino de "ring vacio", asi que en el camino rapido las
 *     escrituras del consumidor podian no verse nunca -- y de ahi venia la
 *     necesidad de drenar (con su coste: BR2 a 40fps) o el contador de spin
 *     heuristico.  Con la barrera en su sitio, ambos sobran.
 *
 * La sincronizacion real de las transferencias de VRAM sigue donde debe
 * estar, igual que upstream: gpuReadData (0x1810) y gpuReadDataMem (DMA
 * vram2mem) drenan el ring antes de leer los datos. */
u32 gpuReadStatus(void)
{
    u32 r;
    const int pending = (s_thread_state == GPU_THREAD_RUNNING &&
                         s_ring_wpos != s_ring_rpos);
    int busy;

    /* Ver el comentario de g_gpu_busy_model arriba: de que derivamos
     * "ocupada" decide si la velocidad del hilo GPU del host se filtra al
     * presupuesto de ciclos de la CPU emulada o no. */
    switch (g_gpu_busy_model) {
        case GPU_BUSY_MODEL_NEVER:
            busy = 0;
            break;
        case GPU_BUSY_MODEL_BOUNDED:
            /* Resta con signo = wrap-safe frente al desborde de
             * psxRegs.cycle (mismo patron que schedule_timeslice). */
            busy = pending && ((s32)(s_gpu_busy_until - psxRegs.cycle) >= 0);
            break;
        default:
            busy = pending;
            break;
    }

#if PCSXR_DIAG_INSTRUMENTATION
    /* Contabilidad: cuantos ciclos emulados se come la espera.  Un
     * "episodio" es un tramo continuo de lecturas con busy=1; se cierra en
     * la primera lectura que ya ve la GPU libre. */
    diag_gpu_status_reads++;
    if (busy) {
        diag_gpu_status_busy++;
        if (!s_busy_episode_active) {
            s_busy_episode_active = 1;
            s_busy_episode_start  = psxRegs.cycle;
            diag_gpu_busy_episodes++;
        }
    } else if (s_busy_episode_active) {
        s_busy_episode_active = 0;
        diag_gpu_busy_cycles += psxRegs.cycle - s_busy_episode_start;
    }
#endif /* PCSXR_DIAG_INSTRUMENTATION */

    /* Acquire: ver todo lo que el consumidor ya publico. */
    __lwsync();

    DIAG_SET_PLUGIN_CALL(GPU_CALL_READ_STATUS);
    r = GPU_readStatus();
    DIAG_SET_PLUGIN_CALL(GPU_CALL_NONE);

    if (busy)
        r &= ~(GPUSTATUS_IDLE | GPUSTATUS_READYFORCOMMANDS);

    return r;
}

/* Punto de sincronizacion publico: espera a que el hilo consumidor vacie
 * el ring.  Para callers en otras TUs (p.ej. misc.c savestate) que hacen
 * accesos directos a GPU_* y necesitan que el trabajo encolado este
 * completo antes.  ring_drain es static; esto lo expone. */
void gpuSync(void)
{
    ring_drain();
}

/* ===========================================================================
 * Lifecycle del GPU helper thread.
 * =========================================================================== */

/* Si un shutdown no consigue JOINear al consumidor, el hilo viejo sigue
 * vivo.  En ese caso NO podemos crear otro: dos consumidores sobre el mismo
 * ring se pisan el cursor rpos (corrupcion + rpos saltando).  Marcamos el
 * subsistema como no disponible y corremos inline el resto de la sesion:
 * perder el hilo GPU es infinitamente preferible a dos consumidores. */
static int s_thread_unavailable = 0;

/* ===========================================================================
 * MODELO DE CICLO DE VIDA: HILO PERSISTENTE
 *
 * El hilo consumidor se crea UNA SOLA VEZ y sobrevive a las cargas/descargas
 * de juego.  Init/Shutdown solo lo APARCAN (cambian s_thread_state), no lo
 * crean ni lo destruyen.
 *
 * Por que: crear y destruir el hilo en cada carga colgaba la consola de
 * forma intermitente tras varias cargas, dentro de `CreateThread`, que no
 * retornaba (de ahi que el ultimo log fuese "gpuDmaThreadInit" y no saliera
 * ningun mensaje posterior: no habia ningun sitio donde loguear).  Es un
 * problema conocido y documentado en este repo: ver el comentario de
 * pcsxr_sthread_create en libpcsxcore/pcsxr-threads.c ("HW0 ... puede colgar
 * el CreateThread del hilo de GPU (gpuDmaThreadInit) de forma
 * intermitente").  Un hilo persistente elimina la causa de raiz: cero
 * CreateThread/CloseHandle repetidos => ni fuga de handles, ni agotamiento,
 * ni cuelgue intermitente de creacion.
 *
 * El consumidor no guarda estado del juego: solo vacia el ring.  Entre
 * juegos el ring esta vacio y el hilo entra en su path idle con backoff
 * (Sleep tras un rato sin trabajo), asi que no quema HW2 en los menus.
 *
 * s_thread_state:
 *   RUNNING  = los productores usan el ring (chain_enqueue -> ring_push)
 *   STOPPED  = productores INLINE; el consumidor sigue vivo pero idle
 *   STOPPING = orden de salida definitiva (solo en gpuDmaThreadDestroy)
 * =========================================================================== */

/* El hilo existe y esta en su bucle (independiente de RUNNING/STOPPED). */
static int s_thread_created = 0;

void gpuDmaThreadShutdown(void)
{
	/* NO destruimos el hilo: solo lo aparcamos.  Drenado best-effort
	 * ACOTADO (antes era un spin sin limite: si el consumidor no consumia,
	 * no salia nunca).  En un unload los draws pendientes son irrelevantes,
	 * el juego se esta yendo. */
	if (s_thread_state == GPU_THREAD_RUNNING) {
		DWORD t0 = GetTickCount();
		while (s_ring_wpos != s_ring_rpos) {
			if ((GetTickCount() - t0) > 250u) {
				pcsxr_log(RETRO_LOG_DEBUG,
					"[PCSXR-LR] GPU shutdown: ring no vacio tras 250ms (%u/%u), continuando\n",
					(unsigned)s_ring_wpos, (unsigned)s_ring_rpos);
				break;
			}
			YieldProcessor();
		}
		__lwsync();
	}

	/* Aparcar: los productores pasan a inline; el consumidor queda idle. */
	s_thread_state = GPU_THREAD_STOPPED;
}

void gpuDmaThreadInit(void)
{
    /* NO tocamos los cursores si el hilo ya existe.  Son contadores libres:
     * `wpos == rpos` significa "vacio" sea cual sea su valor, y el shutdown
     * anterior ya drenó.  Resetearlos a 0 con el consumidor vivo seria una
     * CARRERA: el consumidor podria haber leido el wpos viejo (p.ej. 5000)
     * justo antes del reset y leer luego rpos = 0, calculando
     * used = 5000 -> procesaria basura del ring de la sesion anterior.
     * El cursor es propiedad exclusiva de su dueño (SPSC). */
	if (!g_pcsxr_threading_enabled || s_thread_unavailable){
		/* Modo single-thread: chain_enqueue / ring_drain van directos.
		 * Si el hilo ya existe se queda aparcado e idle, sin molestar. */
		s_thread_state = GPU_THREAD_STOPPED;
		return;
	}

	if (s_thread_created) {
		/* REUTILIZAR el hilo existente: nada de CreateThread (su cuelgue
		 * intermitente era la causa del hang tras varias cargas). */
		__lwsync();   /* release: estado consistente antes de reactivar */
		s_thread_state = GPU_THREAD_RUNNING;
		return;
	}

	/* Primera vez en la sesion: crear el hilo (una sola vez).  Aqui SI es
	 * seguro poner los cursores a cero: todavia no hay consumidor. */
	s_ring_wpos = 0;
	s_ring_rpos = 0;
	s_thread_state = GPU_THREAD_RUNNING;

	s_thread_handle = CreateThread(NULL, 0,
								   (LPTHREAD_START_ROUTINE)gpu_thread_proc,
								   NULL, CREATE_SUSPENDED, NULL);
	if (!s_thread_handle) {
		/* CreateThread fallo (heap exhausted, kernel handles, etc.).
		 * Caer a modo single-thread runtime: chain_enqueue / ring_drain
		 * detectan state != RUNNING y van direct. */
		pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] WARNING: CreateThread for GPU helper failed, falling back to inline mode\n");
		s_thread_state      = GPU_THREAD_STOPPED;
		s_thread_unavailable = 1;
		return;
	}

	SetThreadPriority(s_thread_handle, THREAD_PRIORITY_NORMAL);
	XSetThreadProcessor(s_thread_handle, 2);
	ResumeThread(s_thread_handle);
	s_thread_created = 1;
	pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] GPU helper thread creado (persistente, HW2)\n");
}

/* Teardown REAL del hilo.  Solo para el cierre del core (retro_deinit), no
 * entre juegos.  Si el join falla dejamos el hilo vivo y marcamos el
 * subsistema como no disponible: nunca crear un segundo consumidor sobre el
 * mismo ring (dos consumidores se pisan rpos). */
void gpuDmaThreadDestroy(void)
{
	DWORD wait_result;

	if (!s_thread_created || s_thread_handle == NULL) {
		s_thread_state   = GPU_THREAD_STOPPED;
		s_thread_created = 0;
		return;
	}

	s_thread_state = GPU_THREAD_STOPPING;
	wait_result = WaitForSingleObject(s_thread_handle, 5000);
	if (wait_result == WAIT_TIMEOUT) {
		pcsxr_log(RETRO_LOG_ERROR,
			"[PCSXR-LR] ERROR: GPU helper thread no salio en 5s; se deja vivo y se deshabilita el hilo GPU\n");
		s_thread_unavailable = 1;
		s_thread_handle      = NULL;   /* handle fugado a proposito: el hilo lo usa */
		s_thread_state       = GPU_THREAD_STOPPED;
		s_thread_created     = 0;
		return;
	}

	CloseHandle(s_thread_handle);
	s_thread_handle  = NULL;
	s_thread_state   = GPU_THREAD_STOPPED;
	s_thread_created = 0;
}

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

			/* [XBOX360] size/4, NO size.  upstream pcsx_rearmed (psxdma.c,
			 * casos 0x01000200 y 0x01000201) programa la complecion en
			 * `words / 4`; el pcsxr clasico ponia `size` y ese es el valor
			 * que heredamos -- el comentario de arriba es identico en los dos
			 * arboles, upstream cambio el valor y dejo el comentario.
			 * Importa porque CHCR bit 24 (DMA2 activa) es el gate del bucle
			 * de drenado de la cola de callbacks del BIOS (0x800175C8: sale
			 * si la DMA sigue activa).  Con `size` la DMA figura ocupada 4x
			 * mas tiempo y el drenado saca muchas menos entradas por pasada. */
			GPUDMA_INT(size / 4);
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


			/* [XBOX360] size/4, NO size.  upstream pcsx_rearmed (psxdma.c,
			 * casos 0x01000200 y 0x01000201) programa la complecion en
			 * `words / 4`; el pcsxr clasico ponia `size` y ese es el valor
			 * que heredamos -- el comentario de arriba es identico en los dos
			 * arboles, upstream cambio el valor y dejo el comentario.
			 * Importa porque CHCR bit 24 (DMA2 activa) es el gate del bucle
			 * de drenado de la cola de callbacks del BIOS (0x800175C8: sale
			 * si la DMA sigue activa).  Con `size` la DMA figura ocupada 4x
			 * mas tiempo y el drenado saca muchas menos entradas por pasada. */
			GPUDMA_INT(size / 4);
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

