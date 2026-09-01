#ifndef __GPU_H__
#define __GPU_H__

#include "../plugins/xbox_soft/peops_prof.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ===========================================================================
 * Diagnostic instrumentation switch.
 * ===========================================================================
 *
 * Cuando =1, compila una capa de telemetria que corre en el GPU helper
 * thread y loguea (via pcsxr_log) en que parte del main thread esta
 * atascado el emulador cuando deja de avanzar el cycle counter.  Es la
 * misma sonda que usamos para diagnosticar GTA (CDDA deadlock), NFS3
 * (cache visibility) y multi-game-load (thread leak).
 *
 * Cuando =0 (default), todas las macros DIAG_SET_* expanden a (void)0,
 * el watchdog del consumer se compila fuera, y las variables globales
 * de tracking no se declaran.  Coste runtime cero, igual que si no
 * existiera la instrumentacion.
 *
 * Para activarlo: cambiar 0 -> 1 abajo, recompilar.  O pasar
 *   /D PCSXR_DIAG_INSTRUMENTATION=1
 * en la commandline del compilador para habilitarlo sin tocar el
 * fichero (util para builds A/B con un toggle de proyecto).
 *
 * Lo que captura cuando esta ON:
 *
 *   retro_run_section          fase actual de retro_run en main
 *                              (RR_SEC_ENTRY, RR_SEC_CPU_EXEC,
 *                              RR_SEC_VIDEO_CB, etc.)
 *   s_gpu_plugin_call          si main esta dentro de un wrapper
 *                              GPU (writeData, readStatus, etc.)
 *   s_psxhw_active             si main esta dentro de un dispatcher
 *                              de hw register access (0x1f80xxxx)
 *   s_psx_irq_handler          si main esta dentro de un IRQ
 *                              handler (cdrInterrupt, gpuInterrupt,
 *                              spuInterrupt, etc.)
 *
 * Mapa de hw_active a subsistema PSX (cuando irq=-1):
 *   0x1040          SIO (controllers)
 *   0x1070          IRQ status/mask
 *   0x1080-0x10F8   DMA channels
 *   0x1100-0x1128   Root counters
 *   0x1800-0x1803   CD-ROM
 *   0x1820-0x1824   MDEC
 *   0x1c00-0x1fff   SPU
 *
 * El log del watchdog (en gpu.c) cubre los 4 trackers + cycles
 * anchor + spin counts del ring drain/push, asi que basta con leer
 * el output [WD] para localizar el cuelgue.
 */
#ifndef PCSXR_DIAG_INSTRUMENTATION
#define PCSXR_DIAG_INSTRUMENTATION 0
#endif

/* Constantes que viajan como argumento a las macros DIAG_SET_*.
 * Las dejamos definidas siempre (independiente de PCSXR_DIAG_INSTRUMENTATION)
 * para que los call-sites compilen sin envolver cada uno en `#if`.
 * Cuando la instrumentacion esta off, la macro descarta el argumento
 * y el compilador elide cualquier load/operacion sobre estas constantes. */

/* === Section tracker (libretro_core.cpp -> retro_run) === */
#define RR_SEC_OUT_OF_RUN     0   /* fuera de retro_run, en frontend (Salvia) */
#define RR_SEC_ENTRY          1   /* arrancando retro_run */
#define RR_SEC_INPUT_POLL     2   /* poll_libretro_input */
#define RR_SEC_CPU_EXEC       3   /* dentro de psxCpu->Execute (dynarec) */
#define RR_SEC_AUTOSKIP       4   /* decidiendo auto-frameskip */
#define RR_SEC_VIDEO_CB       5   /* en video_cb (Salvia/D3D present) */
/* IDs 6 y 7 (audio drain / audio cb) eliminados: el audio ahora se
 * pasa directamente al frontend desde SoundFeedStreamData (SPU
 * cycle-driven), sin drenado ni audio_buf intermedio. */
#define RR_SEC_PERF_DUMP      8   /* cerrando ventana [PERF] */

/* === Hardware register access tracker (psxhw.c dispatchers) ===
 * Bits 0-15: direccion (0x1f80xxxx).  Bit 16: 1=write, 0=read.
 * 0 = idle. */
#define PSXHW_WRITE_FLAG      0x10000

/* === Interrupt handler tracker (r3000a.c psxBranchTest) ===
 * -1 = idle.  Otros valores son PSXINT_* del enum en r3000a.h. */
#define PSX_IRQ_NONE          -1

/* === GPU plugin call tracker (gpu.c wrappers) === */
#define GPU_CALL_NONE             0
#define GPU_CALL_WRITE_DATA       1
#define GPU_CALL_WRITE_DATA_MEM   2
#define GPU_CALL_READ_DATA        3
#define GPU_CALL_READ_STATUS      4
#define GPU_CALL_WRITE_STATUS     5
#define GPU_CALL_UPDATE_LACE      6
#define GPU_CALL_READ_DATA_MEM    7
#define GPU_CALL_FREEZE           8
#define GPU_CALL_THREAD_PROC      9   /* dentro del consumer (gpu_thread_proc) */


#if PCSXR_DIAG_INSTRUMENTATION

/* Variables globales que los trackers manipulan via las macros
 * DIAG_SET_*.  Solo declaradas/definidas con instrumentacion ON
 * para no consumir memoria/cache cuando esta desactivada. */
extern volatile int      retro_run_section;
extern volatile uint32_t s_psxhw_active;
extern volatile int      s_psx_irq_handler;
extern volatile int      s_gpu_plugin_call;

/* Macros que los call-sites usan.  Cuando PCSXR_DIAG_INSTRUMENTATION=1
 * estos expanden a una asignacion volatile.  Cuando =0 a (void)0. */
#define DIAG_SET_RR_SEC(v)        do { retro_run_section = (v);   } while (0)
#define DIAG_SET_HW_ACTIVE(v)     do { s_psxhw_active    = (v);   } while (0)
#define DIAG_SET_IRQ_HANDLER(v)   do { s_psx_irq_handler = (v);   } while (0)
#define DIAG_SET_PLUGIN_CALL(v)   do { s_gpu_plugin_call = (v);   } while (0)

#else /* PCSXR_DIAG_INSTRUMENTATION == 0 */

#define DIAG_SET_RR_SEC(v)        ((void)0)
#define DIAG_SET_HW_ACTIVE(v)     ((void)0)
#define DIAG_SET_IRQ_HANDLER(v)   ((void)0)
#define DIAG_SET_PLUGIN_CALL(v)   ((void)0)

#endif /* PCSXR_DIAG_INSTRUMENTATION */


	/* GPU helper thread lifecycle.  Llamados desde libretro_core.cpp
	 * en emu_setup / emu_teardown.  Idempotentes y safe contra calls
	 * repetidos.  Implementacion en gpu.c, sub-seccion "GPU THREADING
	 * SUBSYSTEM". */
	void gpuDmaThreadInit(void);
	void gpuDmaThreadShutdown(void);   /* aparca el hilo (entre juegos) */
	void gpuDmaThreadDestroy(void);    /* teardown REAL: solo en retro_deinit */

	/* Per-frame counter de QueryPerformanceCounter ticks que la CPU
	 * emulada paso bloqueada en ring_drain esperando al GPU helper
	 * thread.  retro_run lo resetea a 0 antes de psxCpu->Execute() y
	 * lo lee despues para:
	 *   (a) Auto-frameskip: skipear solo si gpu_wait domina el exceso.
	 *   (b) Dump [PERF]: desglose CPU vs GPU del exec time. */
	extern volatile uint64_t gpu_wait_ticks;

	/* Modelo de "GPU ocupada" para los bits IDLE/READYFORCOMMANDS de
	 * GPUSTAT.  0 = RING (historico), 1 = BOUNDED (acotado en ciclos
	 * emulados, como el gpuIdleAfter de upstream), 2 = NEVER (siempre
	 * idle, como el GPUreadStatus de gpulib).  Lo fija
	 * check_gpu_busy_model() en libretro_core.cpp; la razon de que sea
	 * configurable esta en el bloque de comentario de gpu.c. */
	extern int g_gpu_busy_model;

	/* Descarta el estado diferido (GP1 0x04 en cola).  Se llama en reset /
	 * carga de juego / load state: las posiciones encoladas apuntan a un ring
	 * que ya no significa nada. */
	void gpuDiscardDeferred(void);

#if PCSXR_DIAG_INSTRUMENTATION
	/* Traza temporal one-shot (ver el bloque en gpu.c).  retro_run la arma en
	 * el primer frame fuera de presupuesto y marca su entrada/salida. */
	void diag_trace_arm(void);
	void diag_trace_mark(int kind);
#define DIAG_TR_RUN_IN  1
#define DIAG_TR_RUN_OUT 2
#endif

	/* Coste medido de la espera del juego a la GPU, en CICLOS EMULADOS
	 * (no en tiempo de host).  Lo vuelca [GPU-BUSY] en r3000a.c y los
	 * resetea alli en cada ventana. */
#if PCSXR_DIAG_INSTRUMENTATION
	extern volatile u32 diag_gpu_busy_cycles;
	extern volatile u32 diag_gpu_busy_episodes;
	extern volatile u32 diag_gpu_status_reads;
	extern volatile u32 diag_gpu_status_busy;
	/* Tiempo real de rasterizado del hilo consumidor y nº de bloqueos del
	 * productor.  Los lee [RR-PERF] en libretro_core.cpp. */
	extern volatile uint64_t diag_gpu_thread_busy_ticks;
	extern volatile u32      diag_gpu_drain_waits;
	/* Sondas del diagnostico de solapamiento (ver el bloque de comentario en
	 * gpu.c): busy del consumidor SOLO mientras el emu esta en CPU_EXEC,
	 * tiempo que el emu pasa bloqueado en ring_push (que no entraba en
	 * gpu_wait), y palabras pendientes en el ring al llegar al vblank. */
	extern volatile uint64_t diag_gpu_busy_exec_ticks;
	extern volatile uint64_t diag_push_spin_ticks;
	extern volatile u32      diag_lace_pend_words;
	/* Histograma de drains BLOQUEANTES por comando GP1 (ver gpu.c). */
	extern volatile u32      diag_gp1_drain_cmd[32];
	extern volatile u32      diag_gp1_04_defer;
	/* Fase 1: clasificacion del vblank (ver gpu.c).  Deciden si compensa
	 * portar la espera parcial `calc_scanout_wait` de upstream. */
	extern volatile u32      diag_scanout_free;
	extern volatile u32      diag_disp_alt;
	/* Top-8 comandos GP0 por tiempo de rasterizado (ver gpu.c).  Volcarlo por
	 * ventana: su disparador original (chunk > 50 ms) no salta en F1'99. */
	void gpuDumpCmdHist(void);
	/* Geometria de display + area de dibujo, para el volcado [DISP].  Vive en
	 * el plugin (plugins/xbox_soft/gpu.c) y se compila SIEMPRE, igual que
	 * PEOPS_GPUdiagDisplayOrigin: asi el core y el plugin no tienen que
	 * coincidir en el valor del flag para que enlace.  Se declara aqui, y no
	 * solo en externals.h, porque quien la llama es libretro_core.cpp, que no
	 * incluye las cabeceras del plugin.  Solo es de fiar con el ring drenado
	 * (ver el comentario de la funcion). */
	/* Cuenta de pixeles no negros en el rectangulo visible: decide entre
	 * "el rasterizador no escribe donde toca" y "escribe bien". */
	void PEOPS_GPUdiagVramStats(unsigned int *nonzero, unsigned int *total);
	unsigned long PEOPS_GPUdiagDisplayRect(unsigned long *mode,
	                                       unsigned long *draw,
	                                       int *disabled);
#endif

	/* Geometria de display para el GunCon, calculada y propiedad del HILO
	 * PRINCIPAL (plugins/xbox_soft/gpu.c).  Equivalente de GPUgetScreenInfo()
	 * de upstream.  Se declara aqui porque la llaman libpcsxcore/plugins.c (C)
	 * y libretro_core.cpp (C++), que no incluyen las cabeceras del plugin. */
	void PEOPS_GPUgetScreenInfo(int *y, int *base_vres);
	void PEOPS_GPUgetHRange(int *x0, int *span);
	void PEOPS_GPUresetScreenInfo(void);

    void gpuWriteData(u32 data);
	u32  gpuReadData(void);	

/////////////////////////////////////////////updatelace
	void gpuUpdateLace();
/////////////////////////////////////////////

////////////////////////////////////////////try again
void gpuWriteStatus(u32 data);
u32  gpuReadStatus(void);   /* NO bloqueante; usar SIEMPRE en vez del plugin
                             * GPU_readStatus crudo: deriva "GPU ocupada" del
                             * ring y aplica la barrera acquire. */
/////////////////////////////////////////

/* Sincroniza (drena el ring) para accesos directos a GPU_* desde otras TUs. */
void gpuSync(void);
	void gpuWriteDataMem(uint32_t *, int);
	void gpuReadDataMem(uint32_t *, int);

	void psxDma2(u32 madr, u32 bcr, u32 chcr);

#define gpuInterrupt() \
HW_DMA2_CHCR_2 &= SWAP32(~0x01000000); \
DMA_INTERRUPT_2(2);

void CALLBACK GPUbusy( int ticks );

#ifdef __cplusplus
}
#endif

#endif
