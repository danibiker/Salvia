#ifndef SALVIA_MT32_FRONTEND_H
#define SALVIA_MT32_FRONTEND_H

/* Munt (Roland MT-32 / CM-32L) para el sintetizador MIDI del frontend.
 *
 * Como con el SC-55 (sc55_frontend.h), no hay copia del emulador: se compilan
 * los MISMOS fuentes de libretro/px68k/midi/mt32emu/munt que usa el core
 * px68k.  En la configuracion de px68k el core ya trae su Munt, asi que aqui se
 * renombran los tres espacios de nombres de Munt para que sus simbolos no
 * choquen al enlazar.  No se compila c_interface (la API en C, que son funciones
 * extern "C" sin espacio de nombres): el frontend usa la API en C++.
 *
 * Lo incluyen mt32_frontend.cpp y mt32_frontend2.cpp (los fuentes de Munt) y
 * mt32synth.cpp (quien los usa); nadie mas, para que las macros no se
 * escapen. */
#define MT32Emu  SalviaMT32Emu
#define SRCTools SalviaSRCTools
#define sha1     salvia_sha1

/* Lo mismo que define el core en su vcxproj */
#ifndef MT32EMU_WITH_INTERNAL_RESAMPLER
#define MT32EMU_WITH_INTERNAL_RESAMPLER 1
#endif
#ifndef MT32EMU_FASTER_MATH
#define MT32EMU_FASTER_MATH 1
#endif

#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/Synth.h"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/File.h"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/FileStream.h"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/ROMInfo.h"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/MidiStreamParser.h"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/SampleRateConverter.h"

#endif
