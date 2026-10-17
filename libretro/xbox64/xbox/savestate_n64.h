/*
 * xbox/savestate_n64.h  --  estados guardados, en memoria.
 *
 * Misma frontera C/C++ que plataforma.h.  C89.
 */

#ifndef XBOX64_SAVESTATE_N64_H
#define XBOX64_SAVESTATE_N64_H

#include <stddef.h>

/* Tamanyo del bufer que hay que reservar.  CONSTANTE durante toda la
 * sesion: libretro llama a `retro_serialize_size` una vez y se lo cree. */
size_t xbox64_state_tam(void);

/* 1 si fue bien, 0 si no.  `tam` es el tamanyo del bufer que da el
 * frontend, y puede ser MAYOR que el que hace falta. */
int xbox64_state_guardar(void *destino, size_t tam);
int xbox64_state_cargar(const void *origen, size_t tam);

#endif /* XBOX64_SAVESTATE_N64_H */
