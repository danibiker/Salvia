#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>
#include "midireplay.h"

/* Sintetizador General MIDI del frontend, sobre TinySoundFont.
 *
 * Por que hace falta: los cores de libretro no sintetizan MIDI.  Los que tienen
 * musica MIDI (px68k con la placa CZ-6BM1 del X68000, dosbox-pure, prboom) piden
 * RETRO_ENVIRONMENT_GET_MIDI_INTERFACE y escupen bytes MIDI crudos esperando que
 * el frontend los convierta en sonido.  Si nadie contesta, la musica se pierde
 * en silencio -- y en px68k es peor que no tener nada, porque el juego detecta
 * la placa, elige la ruta MIDI y se queda mudo en vez de usar su FM interno.
 *
 * DONDE CORRE CADA COSA
 *
 *   open / close / setSampleRate      -> hilo PRINCIPAL, con la emulacion parada
 *   writeByte / flush / render / panic -> hilo de EMULACION (dentro de retro_run)
 *
 * No hay candados, y por eso el reparto de arriba es un contrato, no una
 * sugerencia: cambiar de SoundFont con una partida en marcha seria una carrera
 * de verdad (render() estaria usando el tsf* que close() libera).  El menu solo
 * se alcanza con la emulacion parada, asi que se cumple.
 *
 * Hay una excepcion controlada: la carga perezosa abre el banco desde el
 * handler de GET_MIDI_INTERFACE, y algun core (dosbox-pure) pide ese interface
 * ya dentro de retro_run.  No es una carrera: en ese caso quien abre es el
 * MISMO hilo que luego llama a render(), asi que las dos cosas van en
 * secuencia.
 *
 * POR QUE SE RENDERIZA EN EL HILO DE EMULACION
 *
 * render() se llama desde retro_audio_sample_batch, es decir, con el audio que
 * acaba de producir el core y a la tasa del core.  Asi el sintetizador avanza
 * exactamente con el tiempo emulado: la pausa, el menu y los cambios de
 * velocidad salen gratis y sin desincronizar, sin hilo propio ni anillo extra.
 * El precio es que la CPU del synth sale del presupuesto de frame del core, de
 * ahi el tope de voces (mas bajo en la 360).
 */

struct tsf; /* tsf.h solo se incluye en midisynth.cpp */
class Sc55Synth;

/* Ruta especial de open() para el Roland SC-55 (Nuked-SC55) en lugar de un
 * SoundFont: "sc55:" seguido del directorio con las ROMs (ver sc55synth.h).
 * Lo construye applyMidiSoundfont cuando se elige la entrada "Roland SC-55" de
 * la lista. */
#define MIDISYNTH_SC55_PREFIX "sc55:"

/* Definida en salvia.cpp.  Resuelve que SoundFont toca segun la configuracion y
 * lo aplica.  Se declara aqui, como applyMenuMusic en musicplayer.h, porque la
 * llaman los callbacks del menu y salvia.h no se incluye desde ningun otro
 * sitio.
 *
 * CARGA PEREZOSA: un banco ocupa en RAM el DOBLE de lo que pesa el fichero
 * (TSF expande las muestras a float), y hay bancos de 50 MB por ahi.  Reservar
 * eso para un core que no va a mandar un solo byte MIDI no tiene sentido, y
 * menos en la 360.  Por eso, con loadNow a false, esto solo AJUSTA el
 * sintetizador si ya estaba cargado; abrirlo se deja para el momento en que un
 * core pide de verdad el interface MIDI (loadNow = true desde el handler de
 * RETRO_ENVIRONMENT_GET_MIDI_INTERFACE). */
void applyMidiSoundfont(bool loadNow = false);

class MidiSynth {
public:
	MidiSynth();
	~MidiSynth();

	/* ---- hilo principal ---- */

	/* Carga el .sf2 entero en memoria y lo abre.  coreSampleRate es la tasa a la
	 * que se le pedira render() (la del core, no la del dispositivo).
	 * Con MIDISYNTH_SC55_PREFIX + directorio de ROMs abre en su lugar el
	 * Roland SC-55, que corre en un hilo propio (ver sc55synth.h): writeByte,
	 * render y panic se le pasan tal cual y no hay traduccion MT-32. */
	bool open(const std::string& sf2Path, int coreSampleRate);
	void close();
	bool isLoaded() const { return m_tsf != 0 || m_sc55 != 0; }

	/* Re-aplica la tasa cuando el core la cambia (SET_SYSTEM_AV_INFO).  Es
	 * seguro a media reproduccion: solo reajusta la salida y las voces vivas se
	 * reafinan en el siguiente bloque. */
	void setSampleRate(int coreSampleRate);

	void setVolumePercent(int pct);   /* 0..100, aplicado como ganancia global */
	int  getVolumePercent() const { return m_volumePct; }

	/* Que modulo cree el juego que tiene delante.  Con AUTO se deduce del SysEx
	 * de reset que manda el core (ver handleSysex): px68k envia el de MT-32
	 * cuando px68k_midi_output_type = LA, y con eso esa opcion pasa a tener
	 * efecto de verdad sin que el frontend sepa nada de ningun core.  Los otros
	 * dos valores son para juegos que no mandan reset. */
	enum ModuleMode { MODULE_AUTO = 0, MODULE_GM = 1, MODULE_MT32 = 2 };
	void setModuleMode(int mode);
	int  getModuleMode() const { return m_moduleOption; }

	const std::string& getPath() const { return m_path; }
	bool hasDrumBank() const { return m_hasDrumBank; }

	/* ---- hilo de emulacion ---- */

	void writeByte(uint8_t b);        /* retro_midi_interface::write */
	void flush() {}                   /* retro_midi_interface::flush: no hay cola */

	/* Suma el sintetizador sobre un buffer estereo entrelazado ya lleno con el
	 * audio del core.  frames = pares L/R, no muestras sueltas. */
	void render(int16_t* interleavedStereo, int frames);

	/* true en cuanto el core ha mandado algun byte MIDI: sirve para que la ruta
	 * de audio se salte todo el trabajo en los juegos que no usan MIDI. */
	bool isActive() const { return (m_tsf != 0 || m_sc55 != 0) && m_sawData; }

	/* Corta todo el sonido y reinicia el estado de trama.  Al cerrar un juego o
	 * al entrar en fast-forward, o quedan notas colgadas. */
	void panic();

	/* Olvida el estado de la cancion que se guarda para reabrir el SC-55 a
	 * media partida (ver m_sc55Song).  Al cerrar el juego: el siguiente no
	 * tiene que heredarlo. */
	void resetSongState() { m_sc55Song.clear(); }

private:
	enum { MIDI_CHANNELS = 16, SYSEX_MAX = 256, DRUM_CHANNEL = 9 };

	/* Lo que la cancion ha elegido para cada canal.  Se conserva a proposito
	 * entre instancias del sintetizador: al cambiar de SoundFont en caliente, la
	 * instancia nueva no va a recibir otra vez los program change ni los
	 * controladores de la cancion en curso, asi que hay que reaplicarselos o se
	 * queda tocandolo todo con piano y a volumen por defecto. */
	struct ChannelState {
		uint8_t  program;      /* ultimo program change */
		uint8_t  isDrum;       /* 1 en el canal 10 por GM; los SysEx GS pueden moverlo */
		uint8_t  volume;       /* CC7  */
		uint8_t  expression;   /* CC11 */
		uint8_t  pan;          /* CC10 */
		uint16_t pitchwheel;   /* 0..16383, centro 8192 */
	};

	tsf*         m_tsf;
	Sc55Synth*   m_sc55;          /* en lugar de m_tsf con el Roland SC-55 */
	/* Estado de la cancion para el SC-55, que recibe el stream crudo y no pasa
	 * por ChannelState.  Sobrevive a close()/open() a proposito: apagar y
	 * encender el sintetizador desde el menu a media partida crea una unidad
	 * nueva, y open() se lo reenvia para que siga con los mismos instrumentos. */
	MidiReplay   m_sc55Song;
	std::string  m_path;
	int          m_sampleRate;
	int          m_volumePct;
	bool         m_hasDrumBank;   /* el banco trae bank 128 (percusion) */
	bool         m_sawData;
	bool         m_haveState;     /* la cancion ya ha configurado algun canal */
	int          m_moduleOption;  /* lo que ha pedido el usuario (ModuleMode) */
	bool         m_mt32;          /* modo efectivo: traducir programas de MT-32 */
	ChannelState m_ch[MIDI_CHANNELS];

	/* Estado de la maquina de trama del stream de bytes.  uint8_t en todos
	 * lados a proposito: con `char` pelado la signedness en PPC es cosa del
	 * compilador y `b & 0x80` o `b >= 0xF8` se invierten en silencio. */
	uint8_t  m_running;        /* ultimo status de canal visto, 0 = ninguno */
	uint8_t  m_status;         /* status del mensaje que se esta montando */
	uint8_t  m_data[2];
	uint8_t  m_dataLen;
	uint8_t  m_dataNeed;
	uint8_t  m_inSysex;
	uint8_t  m_sysexOverflow;
	uint16_t m_sysexLen;
	uint8_t  m_sysex[SYSEX_MAX];   /* sin el 0xF0 inicial ni el 0xF7 final */

	/* Contadores de diagnostico (ver MIDI_SYNTH_DEBUG en midisynth.cpp). */
	uint32_t m_dbgBytes;
	uint32_t m_dbgNotes;

	void resetFraming();
	void resetChannels(bool hardReset);
	void reapplyChannels();       /* vuelca m_ch[] al sintetizador */
	void applyChannelMessage(uint8_t status, uint8_t d0, uint8_t d1);
	void applyProgram(int ch, uint8_t program);
	void setMt32Active(bool on);  /* cambia el modo efectivo y lo deja en el log */
	void handleSysex();

	MidiSynth(const MidiSynth&);
	MidiSynth& operator=(const MidiSynth&);
};
