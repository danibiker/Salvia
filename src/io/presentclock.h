#pragma once

#include <stdint.h>

/* ==========================================================================
 * EL RELOJ DE PRESENTACION
 *
 * POR QUE EXISTE.  Todo el pacing del frontend INFERIA el reloj de la pantalla
 * a partir del reloj de la CPU: medias moviles del periodo entre presents,
 * enganche a la rejilla del timer del scheduler, margen de sleep aprendido, y
 * umbrales para decidir "llegamos o no".  Catorce constantes para adivinar algo
 * que el hardware sabe de primera mano.
 *
 * En Xbox 360 el driver llama a un callback en CADA vblank
 * (`SetVerticalBlankCallback`), asi que hay un CONTADOR de vblanks exacto.  Con
 * el, el pacing deja de ser "espera hasta tal instante de reloj" y pasa a ser
 * "presenta este frame en el vblank N", que es lo que de verdad ocurre.
 *
 * LO QUE ESO ELIMINA.  Si el objetivo es un indice de vblank, no hay rejilla que
 * adivinar ni margen que aprender: la espera termina cuando el contador llega.
 * Se van `gridPhase/gridValid/gridGood/gridMiss` con sus seis constantes,
 * `sleepMargin/sleepOverAvg/sleepOverDev` con sus dos, y la maquina de estados
 * del vsync adaptativo con las suyas.
 *
 * Y HAY UNA GANANCIA QUE NO ES MENOR: la espera pasa de activa a bloqueante.
 * Hoy el limitador gasta 0,5-0,9 ms por frame girando en `YieldProcessor`, y
 * eso es CPU robada al core, que corre en el MISMO hilo.  Esperando en un
 * evento que el callback de vblank pulsa, ese tiempo vuelve a la emulacion.
 *
 * PORTABILIDAD.  D3D9 de escritorio no tiene callback de vblank, asi que en
 * Windows `vblanks()` devuelve 0 y el llamante se queda con el camino de reloj
 * de CPU que ya existia.  La regla de lectura es una sola: **`vblanks() == 0`
 * significa "sin reloj de hardware"**.
 * ========================================================================== */

/* Contador monotono de vblanks desde la creacion del device.  0 = la plataforma
 * no tiene reloj de presentacion y hay que usar el camino de reloj de CPU. */
unsigned int pc_vblanks(void);

/* Swaps que perdieron su vblank (`D3DVBLANK_SWAPMISSED`), acumulado.
 *
 * OJO AL INTERPRETARLO, que esto ya me engano una vez: NO es "llegamos tarde".
 * Cuenta un swap ENCOLADO que no alcanzo su vblank.  Si simplemente presentamos
 * mas tarde de lo que queriamos, no hay swap encolado que perder y esto sale 0.
 * Medido: 0 constante incluso con el 48% de los frames pasados de deadline.  La
 * metrica de "cuantos frames resbalan" es `vblanks` por frame presentado. */
unsigned int pc_swaps_missed(void);

/* Avance del barrido en el frame actual, 0-100.  -1 si no se sabe.
 * Sirve para decidir si merece la pena presentar ya o esperar al siguiente. */
int pc_phase_percent(void);

/* Espera hasta que el contador de vblanks alcance `objetivo`.
 *
 * Devuelve el contador real al salir.  Bloquea en un evento, no gira; `msMax`
 * es el tope de seguridad para que un vblank perdido o un cambio de modo de
 * video no cuelguen el bucle.  Si la plataforma no tiene reloj de hardware
 * devuelve 0 inmediatamente y el llamante debe usar su propio camino. */
unsigned int pc_wait_vblank(unsigned int objetivo, unsigned int msMax);

/* Tasa de refresco del panel en Hz, 0 si no se conoce.  En Xbox 360 sale de
 * `XVIDEO_MODE.RefreshRate`, y se comprobo contando vblanks que el valor es
 * REAL y no nominal (contado 59,95-60,06 frente a 60,0000 declarado). */
float pc_refresh_hz(void);
