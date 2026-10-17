/*
 * xbox/audio_n64.h  --  el plugin de audio, hablando con libretro.
 *
 * Misma frontera C/C++ que plataforma.h e input_n64.h: este fichero es C
 * y lo incluye el wrapper (C++) dentro de un `extern "C"`.
 *
 * C89.
 */

#ifndef XBOX64_AUDIO_N64_H
#define XBOX64_AUDIO_N64_H

/* --- lo que el wrapper llama --------------------------------------- */

/* Una vez por frame, DESPUES de emular.  Vacia el anillo hacia el
 * frontend.  Devuelve cuantos frames estereo entrego. */
unsigned xbox64_audio_drenar(void);

/* La frecuencia que el invitado ha pedido, en Hz.  Antes de que el juego
 * escriba el registro DACRATE devuelve el valor por defecto. */
unsigned xbox64_audio_frecuencia(void);

/* ★ 1 si la frecuencia ha CAMBIADO desde la ultima vez que se pregunto.
 * Deja la marca a cero.  El wrapper lo usa para avisar al frontend con
 * RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, que no se puede llamar desde
 * dentro de la emulacion. */
int xbox64_audio_frecuencia_cambio(void);

/* Tira lo que haya en el anillo.  Al cargar una ROM o un savestate. */
void xbox64_audio_reset(void);

/* La core option del sonido.  Escribe `audioEnabled`, que es lo que mira
 * `aiLenChanged`. */
void xbox64_audio_habilitar(int si);

/* --- lo unico que el plugin necesita del wrapper -------------------- */

/* Lo DEFINE `libretro_core.cpp`.  Entrega `frames` frames estereo de 16
 * bits con signo, entrelazados L,R.  Ver la nota del formato en el .c. */
void xbox64_retro_audio_batch(const short *datos, unsigned frames);

#endif /* XBOX64_AUDIO_N64_H */
