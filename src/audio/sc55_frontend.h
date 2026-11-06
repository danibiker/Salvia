#ifndef SALVIA_SC55_FRONTEND_H
#define SALVIA_SC55_FRONTEND_H

/* API de libretro/px68k/sc55.h con nombres propios del frontend (ver
 * sc55_frontend.c: evita el choque con el sc55.c que enlaza el core px68k). */
#define sc55_new       salvia_sc55_new
#define sc55_free      salvia_sc55_free
#define sc55_load_rom  salvia_sc55_load_rom
#define sc55_reset     salvia_sc55_reset
#define sc55_midi      salvia_sc55_midi
#define sc55_midi_room salvia_sc55_midi_room
#define sc55_voices    salvia_sc55_voices
#define sc55_rate      salvia_sc55_rate
#define sc55_run       salvia_sc55_run

#ifdef __cplusplus
extern "C" {
#endif
#include "../../libretro/px68k/sc55.h"
#ifdef __cplusplus
}
#endif

#endif
