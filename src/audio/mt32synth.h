#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>

#include "rawmidisynth.h"

/* Roland MT-32 / CM-32L para el sintetizador MIDI del frontend, con Munt (el
 * mismo que usa el core px68k, ver mt32_frontend.h).
 *
 * Igual que Sc55Synth: el sintetizador vive en un HILO PROPIO para no comerse
 * el presupuesto de frame del core.
 *
 *   - El hilo de emulacion encola los bytes MIDI en un anillo SPSC y nunca
 *     toca el sintetizador (writeByte).
 *   - El hilo de trabajo pasa esos bytes al analizador MIDI de Munt (que hace
 *     la trama: running status, SysEx...), renderiza, convierte de los 32 kHz
 *     del MT-32 a la tasa del core con el conversor de Munt, y deja LEAD
 *     frames preparados en un segundo anillo SPSC.
 *   - mix() solo copia lo que hay listo y despierta al hilo; nunca espera.
 *
 * Sin candados: los indices de los anillos se publican con barreras.  El hilo
 * se bloquea en un evento mientras su anillo esta lleno.
 *
 * DONDE CORRE CADA COSA (el mismo contrato que MidiSynth)
 *   open / close / findRomSet               -> hilo PRINCIPAL, emulacion parada
 *   writeByte / mix / panic / setOutputRate -> hilo de EMULACION
 *   setVolumePercent                        -> cualquiera (un int)
 */

struct Mt32SynthMunt;   /* objetos de Munt, solo en mt32synth.cpp */

class Mt32Synth : public RawMidiSynth {
public:
	enum Model { MODEL_MT32 = 0, MODEL_CM32L = 1 };

	Mt32Synth();
	~Mt32Synth();

	/* Busca el par de ROMs (control + PCM) del modelo en el directorio system
	 * y en sus subcarpetas habituales (mt32, cm32l, cm64, roland, munt,
	 * prboom).  Mismos nombres que el core px68k:
	 *   MT-32:  MT32_CONTROL.ROM  + MT32_PCM.ROM
	 *   CM-32L: CM32L_CONTROL.ROM + CM32L_PCM.ROM
	 * (tambien en minusculas, con extension .BIN o, el CM-32L, CM-32L_...).
	 * Solo ROMs completas: los volcados partidos en varios ficheros no. */
	static bool findRomSet(const std::string& systemDir, Model model, std::string& outDir);

	/* Carga las ROMs de romDir (las de findRomSet) y arranca el hilo. */
	bool open(const std::string& romDir, Model model, int outRate);
	void close();
	bool isOpen() const { return m_munt != 0; }

	void setOutputRate(int rate);
	void setVolumePercent(int pct);   /* 0..100 */
	void writeByte(uint8_t b);
	void mix(int16_t* stereo, int frames);
	void panic();

private:
	enum {
		AU_RING   = 8192,   /* frames de salida, potencia de dos */
		LEAD      = 2048,   /* frames listos (~46 ms a 44,1 kHz) */
		BLOCK     = 256,    /* frames de salida por pasada del hilo */
		MIDI_RING = 32768   /* bytes, potencia de dos */
	};

	Mt32SynthMunt* m_munt;
	Model          m_model;
	volatile int   m_outRate;      /* lo escribe el hilo de emulacion */
	volatile int   m_volumePct;
	int            m_srcRate;      /* hilo de trabajo: tasa del conversor actual */

	/* ---- compartido ---- */
	int16_t*        m_au;          /* AU_RING * 2, sin volumen */
	volatile size_t m_auW;         /* hilo de trabajo */
	volatile size_t m_auR;         /* hilo de emulacion */
	uint8_t*        m_midi;        /* MIDI_RING */
	volatile size_t m_midiW;       /* hilo de emulacion */
	volatile size_t m_midiR;       /* hilo de trabajo */
	volatile int    m_quit;

	/* ---- lado del hilo de emulacion ---- */
	bool          m_overflowReported;
	bool          m_underrunReported;
	size_t        m_framesMixed;

	void*         m_thread;        /* HANDLE */
	void*         m_wake;          /* HANDLE, evento auto-reset */

	int16_t       m_block[BLOCK * 2];   /* hilo de trabajo */

	void workerFeedMidi();
	size_t workerRender(size_t frames);
	void workerFill(size_t target);
	void workerLoop();
	friend struct Mt32SynthThread;   /* el procedimiento del hilo (WINAPI), en el .cpp */

	Mt32Synth(const Mt32Synth&);
	Mt32Synth& operator=(const Mt32Synth&);
};
