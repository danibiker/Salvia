#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>

/* Roland SC-55 para el sintetizador MIDI del frontend (Nuked-SC55: ejecuta el
 * firmware real de la unidad, H8 + sub-MCU + chip PCM).  Adaptado de
 * libretro/px68k/sc55_libretro.c.
 *
 * Cada muestra es cara: renderizarlo dentro del hilo de emulacion del core
 * (como hace TinySoundFont en MidiSynth) se comeria el presupuesto de frame.
 * Por eso la unidad vive en un HILO PROPIO, como en px68k, donde se midio que
 * va a tiempo real con un nucleo fisico para ella:
 *
 *   - El hilo de emulacion encola los bytes MIDI en un anillo SPSC y nunca
 *     toca la unidad (writeByte).
 *   - El hilo de trabajo pasa esos bytes a la unidad, la ejecuta, remuestrea
 *     de su tasa nativa (66207 Hz mkII / 64000 Hz mk1) a la del core y deja
 *     LEAD frames preparados en un segundo anillo SPSC.
 *   - mix() solo copia lo que hay listo y despierta al hilo; nunca espera.
 *     Si la unidad no da abasto la musica tiene huecos, pero el emulador no
 *     pierde velocidad.
 *
 * Sin candados: los indices de los anillos se publican con barreras.  El hilo
 * se bloquea en un evento mientras su anillo esta lleno, asi que no gasta nada
 * con el juego en pausa o en el menu (no se llama a mix()).
 *
 * DONDE CORRE CADA COSA (el mismo contrato que MidiSynth)
 *   open / close / findRomSet              -> hilo PRINCIPAL, emulacion parada
 *   writeByte / mix / panic / setOutputRate -> hilo de EMULACION
 *   setVolumePercent                        -> cualquiera (un int)
 */

struct sc55;

class Sc55Synth {
public:
	Sc55Synth();
	~Sc55Synth();

	/* Busca un juego de ROMs completo en el directorio system y en sus
	 * subcarpetas habituales (sc55, SC-55, roland, nuked, prboom...).  Mismos
	 * nombres que px68k/prboom/DOSBox Pure:
	 *   SC-55mkII: rom1.bin rom2.bin rom_sm.bin waverom1.bin waverom2.bin
	 *   SC-55:     sc55_rom1.bin sc55_rom2.bin sc55_waverom1.bin
	 *              sc55_waverom2.bin sc55_waverom3.bin */
	static bool findRomSet(const std::string& systemDir, std::string& outDir, bool& outMk2);

	/* Carga las ROMs de romDir (las de findRomSet) y arranca el hilo. */
	bool open(const std::string& romDir, int outRate);
	void close();
	bool isOpen() const { return m_unit != 0; }

	void setOutputRate(int rate);
	void setVolumePercent(int pct);   /* 0..100 */

	void writeByte(uint8_t b);

	/* Suma la unidad sobre un buffer estereo entrelazado (pares L/R) ya lleno
	 * con el audio del core, saturando a 16 bits. */
	void mix(int16_t* stereo, int frames);

	/* Silencia todo (sustain, All Notes Off y All Sound Off en los 16 canales) y tira
	 * el audio ya preparado.  Al entrar en fast-forward y al cerrar el juego. */
	void panic();

private:
	enum {
		AU_RING    = 8192,   /* frames de salida, potencia de dos */
		LEAD       = 2048,   /* frames listos (~46 ms a 44,1 kHz) */
		BLOCK      = 256,    /* frames de salida por pasada del hilo */
		MIDI_RING  = 32768,  /* bytes, potencia de dos */
		NATIVE_MAX = 4096    /* frames nativos guardados para el remuestreo */
	};

	struct sc55*  m_unit;
	unsigned      m_nativeRate;
	volatile int  m_outRate;      /* lo escribe el hilo de emulacion */
	volatile int  m_volumePct;

	/* ---- lado del hilo de trabajo ---- */
	int32_t*      m_native;       /* NATIVE_MAX * 2 */
	size_t        m_nativeCount;
	double        m_nativePhase;
	double        m_ratio;
	int           m_ratioRate;    /* m_outRate con el que se calculo m_ratio */

	/* ---- compartido ---- */
	int32_t*        m_au;         /* AU_RING * 2, dominio de 16 bits, sin volumen */
	volatile size_t m_auW;        /* hilo de trabajo */
	volatile size_t m_auR;        /* hilo de emulacion */
	uint8_t*        m_midi;       /* MIDI_RING */
	volatile size_t m_midiW;      /* hilo de emulacion */
	volatile size_t m_midiR;      /* hilo de trabajo */
	volatile int    m_quit;

	/* ---- lado del hilo de emulacion ---- */
	bool          m_overflowReported;
	bool          m_underrunReported;
	size_t        m_framesMixed;

	void*         m_thread;       /* HANDLE */
	void*         m_wake;         /* HANDLE, evento auto-reset */

	/* Bloqueador de DC (hilo de trabajo) */
	float         m_dcR, m_dcInL, m_dcInR, m_dcOutL, m_dcOutR;

	/* Arranque del firmware: lo hace el hilo de trabajo antes de atender MIDI */
	volatile int  m_booted;
	unsigned      m_bootMs;
	bool          m_bootReported; /* hilo de emulacion */

	bool loadRoms(const std::string& dir, bool mk2);
	void workerFeedMidi();
	void workerDropNative();
	size_t workerRender(size_t frames);
	void workerFill(size_t target);
	void workerBoot();
	void workerLoop();
	friend struct Sc55SynthThread;   /* el procedimiento del hilo (WINAPI), en el .cpp */

	Sc55Synth(const Sc55Synth&);
	Sc55Synth& operator=(const Sc55Synth&);
};
