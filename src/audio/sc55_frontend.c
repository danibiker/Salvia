/* Roland SC-55 (Nuked-SC55) para el sintetizador MIDI del frontend.
 *
 * No hay copia del emulador: se compila el MISMO libretro/px68k/sc55.c que usa
 * el core px68k.  Pero Salvia enlaza un core distinto en cada configuracion y,
 * en la de px68k, el core trae su propio sc55.c: dos definiciones de
 * sc55_new/sc55_run/... y el enlazador se queja.  Por eso aqui se renombran las
 * nueve funciones publicas antes de incluir el fuente; todo lo demas en sc55.c
 * es static y no choca.  sc55_frontend.h hace el mismo renombrado para quien
 * las llama (sc55synth.cpp).
 *
 * Se compila como C (CompileAsC en el vcxproj): el fuente es C89 y como C++ no
 * compila (declara por adelantado arrays static de punteros a funcion). */
#include "sc55_frontend.h"
#include "../../libretro/px68k/sc55.c"
