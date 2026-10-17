/*
 * gxshim/ogc/lwp_watchdog.h  --  el reloj de libogc, en Xbox 360.
 *
 * QUIEN LO PIDE.  `source/main/timers.c:7`, que SI entra en el build: es
 * quien lleva la cuenta de VI/s y FPS del emulador y a quien llaman
 * `r4300/interupt.c:390` (new_vi) y `gc_memory/memory.c:668` (new_frame).
 *
 * De toda la cabecera de libogc, timers.c usa DOS cosas:
 *
 *     gettick()                 el contador de ticks del bus
 *     ticks_to_microsecs(t)     de ticks a microsegundos
 *
 * (`ticks_to_millisecs` y `diff_ticks` aparecen solo en lineas comentadas.)
 *
 * ★ LA BASE DE TIEMPO DEL XENON.  `mftb` cuenta a 50 MHz en Xbox 360, no a
 * la frecuencia del bus de la Wii.  Pero timers.c no necesita la frecuencia
 * de libogc: solo necesita que `ticks_to_microsecs(gettick())` de
 * MICROSEGUNDOS COHERENTES, porque lo unico que hace con el valor es restar
 * dos lecturas.  Por eso la conversion se hace con la frecuencia real del
 * timebase, leida una vez.
 *
 * ★ POR QUE NO SE USA GetTickCount().  Tiene resolucion de ~15 ms y
 * timers.c compara diferencias de medio segundo con precision de
 * microsegundo (`>= 500000.0`).  Con 15 ms de grano, `Timers.vis` saldria
 * a saltos y seria inservible justo como medida, que es para lo unico que
 * lo queremos en la fase 1.
 *
 * ★ TIENE QUE SER VALIDO EN C.  timers.c es .c y compila como C.
 */

#ifndef _XBOX64_GXSHIM_LWP_WATCHDOG_H_
#define _XBOX64_GXSHIM_LWP_WATCHDOG_H_

#include <xtl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Implementadas en xbox/compat_posix.c.  No se ponen `static __inline` aqui
 * a proposito: timers.c declara `extern unsigned int usleep(...)` por su
 * cuenta (timers.c:60), y una definicion inline en la cabecera chocaria con
 * esa declaracion.  Con funciones de verdad, las dos casan. */
unsigned long long gettick(void);
unsigned long long ticks_to_microsecs(unsigned long long ticks);
unsigned long long ticks_to_millisecs(unsigned long long ticks);

#ifdef __cplusplus
}
#endif

#endif /* _XBOX64_GXSHIM_LWP_WATCHDOG_H_ */
