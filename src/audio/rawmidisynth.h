#pragma once

#include <stdint.h>

/* Hilo hardware de la Xbox 360 para el hilo de trabajo de estos
 * sintetizadores, con todos los cores: el 2 (nucleo fisico 1, con el 3
 * libre).  Ver el reparto en const/constant.h. */
static inline int rawMidiSynthXboxHwThread()
{
	return 2;
}

/* Sintetizador MIDI que recibe el stream MIDI CRUDO del core y lo procesa en
 * un hilo propio: el Roland SC-55 (Sc55Synth) y el MT-32 / CM-32L (Mt32Synth).
 * MidiSynth guarda uno de estos en lugar del TinySoundFont y le pasa todo tal
 * cual, sin su maquina de trama ni la traduccion MT-32 -> GM.
 *
 * Mismo reparto de hilos que MidiSynth:
 *   writeByte / mix / panic / setOutputRate -> hilo de EMULACION
 *   setVolumePercent                        -> cualquiera (un int)
 *   abrir y destruir                        -> hilo PRINCIPAL, emulacion parada */
class RawMidiSynth {
public:
	virtual ~RawMidiSynth() {}

	virtual void setOutputRate(int rate) = 0;
	virtual void setVolumePercent(int pct) = 0;   /* 0..100 */

	virtual void writeByte(uint8_t b) = 0;

	/* Suma el sintetizador sobre un buffer estereo entrelazado (pares L/R) ya
	 * lleno con el audio del core, saturando a 16 bits. */
	virtual void mix(int16_t* stereo, int frames) = 0;

	/* Silencia todo y tira el audio ya preparado (fast-forward, cerrar). */
	virtual void panic() = 0;
};
