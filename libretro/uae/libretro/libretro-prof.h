/*
 * Instrumentacion por subsistema, al estilo de la que usamos en pcsxr-360.
 *
 * Reparte el tiempo de cada frame entre los subsistemas de UAE para ver donde
 * se va, en vez de adivinarlo. Mide tiempo EXCLUSIVO: una region anidada le
 * descuenta su tiempo a la que la contiene, asi que los buckets suman casi
 * todo el frame y lo que falta queda como residuo.
 *
 * COSTE: dos lecturas de contador y unas 10 instrucciones por region. En la
 * 360 el contador es __mftb(), una sola instruccion sin acceso a memoria.
 * Aun asi, la regla es instrumentar solo puntos que corran del orden de
 * CIENTOS o POCOS MILES de veces por frame. Nunca algo que se ejecute por
 * cada ciclo de reloj emulado -- decide_line, decide_fetch, decide_blitter y
 * companyia van a 227*312 = ~70.000 veces por frame; instrumentarlas
 * falsearia la medida mas que informar. Ese trabajo cae a proposito en el
 * residuo, y si el residuo domina, eso ES el resultado.
 *
 * No se convierte a milisegundos: el contador barato no tiene una frecuencia
 * que podamos dar por sabida sin calibrar. Se reportan PORCENTAJES del frame
 * (que es lo que interesa) y aparte el tiempo real del frame medido con
 * retro_ticks(), que si esta en microsegundos.
 *
 * Para desactivarlo del todo, comenta PROF_ENABLED: los macros se quedan en
 * nada y no queda ni una instruccion.
 *
 * QUE NUCLEOS DE CPU ESTAN INSTRUMENTADOS: solo los genericos,
 * m68k_run_2_000 (68000/68010) y m68k_run_2_020, que son los que se usan con
 * puae_cpu_compatibility = "normal" -- el valor por defecto fuera de x86_64.
 * Con "compatible" (prefetch, m68k_run_1) o cualquiera de los "exact" el
 * tiempo de la CPU y del chipset NO se separa y cae entero en SIN MARCAR.
 * Si ves SIN MARCAR muy alto, comprueba antes de nada en que modo estas.
 */

#ifndef LIBRETRO_PROF_H
#define LIBRETRO_PROF_H

#define PROF_ENABLED 1

/* Cada bucket es un subsistema. El orden es el del informe. */
enum
{
   PROF_CPU = 0,      /* interprete 68k: dispatch + ejecucion             */
   PROF_CHIPSET,      /* do_cycles(): copper/bitplanes/sprites por ciclo  */
   PROF_HSYNC,        /* hsync_handler: lo que no cae en los tres de abajo */
   PROF_HSYNC_PRE,    /* hsync_handler_pre, sin los dos de abajo          */
   PROF_FINISHDEC,    /* finish_partial_decision, sin los cinco de abajo   */
   PROF_SYNCCOPPER,   /* sync_copper                                       */
   PROF_DEC_HDIW,     /* decide_hdiw                                       */
   PROF_DEC_LINE,     /* decide_line                                       */
   PROF_DEC_FETCH,    /* decide_fetch_safe                                 */
   PROF_DEC_SPRITES,  /* decide_sprites                                    */
   PROF_DEVHSYNC,     /* devices_hsync: disco+audio+cia+blitter+serie      */
   PROF_HSYNC_POST,   /* hsync_handler_post                               */
   PROF_DRAWLINES,    /* draw_lines: bucle de lineas de la rodaja         */
   PROF_RENDER,       /* render_screen: composicion de la pantalla        */
   PROF_VSYNC,        /* vsync_handler: fin de frame                      */
   PROF_DRAW_LINE,    /* drawing.c: rasterizado de una linea              */
   PROF_DRAW_END,     /* finish_drawing_frame + entrega del buffer        */
   PROF_BLITTER,      /* blitter_handler (el evento, no los ciclos)       */
   PROF_COPPER,       /* do_copper                                        */
   PROF_AUDIO,        /* audio.c: Paula + remuestreo                      */
   PROF_DISK,         /* disk.c: emulacion de disquetera                  */
   PROF_CIA,          /* CIA_handler                                      */
   PROF_FILESYS,      /* filesys/hardfile desde el hilo de emulacion      */
   PROF_GLUE_VIDEO,   /* video_cb                                         */
   PROF_GLUE_AUDIO,   /* audio_batch_cb                                   */
   PROF_GLUE_INPUT,   /* input_poll_cb + retro_poll_event                 */
   PROF_GLUE_MISC,    /* update_audiovideo, statusbar, vkbd, mensajes     */
   /* Region EXTERIOR, abierta al entrar en retro_run() y cerrada al salir.
    * Su tiempo exclusivo es todo lo que pasa dentro del frame y no cae en
    * ninguno de los buckets de arriba: el trabajo por ciclo del chipset que
    * a proposito no instrumentamos, mas cualquier cosa que se nos escape.
    * Al estar medida con el mismo contador, los porcentajes cuadran sin
    * mezclar relojes. */
   PROF_UNMARKED,
   PROF_NBUCKETS
};

/* Contadores sueltos, para distinguir "es caro" de "se llama mucho". */
enum
{
   PROFC_INSN = 0,    /* instrucciones 68k ejecutadas    */
   PROFC_LINES,       /* lineas rasterizadas             */
   PROFC_BLITS,       /* eventos de blitter              */
   PROFC_SAMPLES,     /* muestras de audio entregadas    */
   PROFC_NCOUNTERS
};

#include "sysconfig.h"
#include "sysdeps.h"

/* Fuera del #ifdef a proposito: retro_run() declara una variable de este tipo
 * y tiene que compilar igual con la instrumentacion apagada. */
typedef uae_u64 prof_t;

#ifdef PROF_ENABLED

/* --- lectura del contador barato --- */
/* PROF_TICKS_PER_SEC a 0 significa "no sabemos la frecuencia": entonces no se
 * reportan milisegundos, solo porcentajes. NO usar retro_ticks() para esto:
 * cuando el frontend no registra perf_cb devuelve retro_now, que es el reloj
 * EMULADO y avanza 20000 us clavados por frame -- daba 50.0 fps siempre. */
#if defined(_XBOX)
/* Time base del Xenon: una sola instruccion, sin tocar memoria. El intrinseco
 * lo declara PPCIntrinsics.h, y las fuentes de UAE no incluyen xtl.h, asi que
 * hay que traerlo aqui explicitamente. Corre a 49,875 MHz, fijo por hardware
 * e independiente de la frecuencia del core. */
#include <PPCIntrinsics.h>
#define PROF_TICK() ((prof_t)__mftb())
#define PROF_TICKS_PER_SEC 49875000.0
#elif defined(_MSC_VER)
#include <intrin.h>
#define PROF_TICK() ((prof_t)__rdtsc())
#define PROF_TICKS_PER_SEC 0.0 /* el TSC habria que calibrarlo */
#else
extern long retro_ticks(void);
#define PROF_TICK() ((prof_t)retro_ticks())
#define PROF_TICKS_PER_SEC 1000000.0
#endif

#define PROF_STACK_MAX 16

extern prof_t prof_acc[PROF_NBUCKETS];
extern prof_t prof_counter[PROFC_NCOUNTERS];
extern prof_t prof_last;
extern int prof_stack[PROF_STACK_MAX];
extern int prof_sp;
extern int prof_on;

/* Abre una region. Lo acumulado desde la ultima marca se le apunta a la
 * region que estuviera abierta, que es lo que hace que el tiempo sea
 * exclusivo y no inclusivo. */
STATIC_INLINE void prof_enter(int id)
{
   prof_t now;

   if (!prof_on || prof_sp >= PROF_STACK_MAX)
      return;
   now = PROF_TICK();
   if (prof_sp > 0)
      prof_acc[prof_stack[prof_sp - 1]] += now - prof_last;
   prof_stack[prof_sp++] = id;
   prof_last = now;
}

STATIC_INLINE void prof_leave(void)
{
   prof_t now;

   if (!prof_on || prof_sp <= 0)
      return;
   now = PROF_TICK();
   prof_acc[prof_stack[--prof_sp]] += now - prof_last;
   prof_last = now;
}

#define PROF_ENTER(id)   prof_enter(id)
#define PROF_LEAVE()     prof_leave()
#define PROF_COUNT(c, n) do { if (prof_on) prof_counter[c] += (prof_t)(n); } while (0)

/* Lo llama retro_run() una vez por frame, con el frame medido en TICKS del
 * mismo contador que los buckets. Vuelca el informe cada PROF_REPORT_FRAMES. */
extern void prof_frame(prof_t frame_ticks);

#else /* !PROF_ENABLED */

#define PROF_ENTER(id)      do { } while (0)
#define PROF_LEAVE()        do { } while (0)
#define PROF_COUNT(c, n)    do { } while (0)
#define prof_frame(usec)    do { } while (0)

#endif /* PROF_ENABLED */

#endif /* LIBRETRO_PROF_H */
