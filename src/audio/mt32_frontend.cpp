/* Munt (Roland MT-32 / CM-32L) para el sintetizador MIDI del frontend, con
 * los espacios de nombres renombrados (ver mt32_frontend.h).
 *
 * Munt se compila en dos unidades y no en una: varios de sus .cpp definen
 * constantes y funciones auxiliares static con el mismo nombre
 * (MIDDLE_CUTOFF_VALUE, exp9, resAmpDecayFactors, produceDistortedSample,
 * envLogarithmicTime...), y juntos en la misma unidad chocan.
 *   mt32_frontend.cpp   -> todo Munt salvo lo de abajo
 *   mt32_frontend2.cpp  -> srctools (conversor de tasa), LA32WaveGenerator,
 *                          Synth y TVF
 * Ojo con el orden: varios fuentes llevan "using namespace" a nivel de
 * fichero, y en una unidad compartida se acumulan.  Con MT32Emu y SRCTools a
 * la vez, MSVC da FloatSample por ambiguo (los dos son typedef de float) en
 * los fuentes de srctools.  Por eso srctools va al principio de
 * mt32_frontend2.cpp, antes de cualquier "using namespace MT32Emu".
 * Esta es la primera. */
#include "mt32_frontend.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4244 4100 4127 4512 4505 4310 4305 4706 4389 4018 4996 4800 4355)
#endif
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/Analog.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/BReverbModel.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/Display.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/File.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/FileStream.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/LA32FloatWaveGenerator.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/LA32Ramp.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/MidiStreamParser.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/Part.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/Partial.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/PartialManager.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/Poly.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/ROMInfo.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/SampleRateConverter.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/TVA.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/TVP.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/Tables.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/sha1/sha1.cpp"
#include "../../libretro/px68k/midi/mt32emu/munt/mt32emu/src/srchelper/InternalResampler.cpp"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
