/* Roland MT-32 / CM-32L (Munt) en un hilo propio para el sintetizador MIDI del
 * frontend.  Ver mt32synth.h.  Los anillos, el hilo y el relleno son los de
 * sc55synth.cpp; lo que cambia es la unidad (Munt en lugar de Nuked-SC55). */

/* Munt primero: mt32_frontend.h renombra sus espacios de nombres y tiene que
 * verse antes que cualquier otra cabecera que los mencione. */
#include "mt32_frontend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/logger.h>

#include "mt32synth.h"

#ifndef NOMINMAX
#define NOMINMAX   /* que windows.h / xtl.h no definan min y max */
#endif

#if defined(_XBOX)
	#include <xtl.h>
	#include <ppcintrinsics.h>
	/* lwsync: las escrituras anteriores se ven antes que el indice publicado
	 * (release) y las lecturas posteriores no se adelantan a el (acquire). */
	#define MT32_BARRIER() __lwsync()
	#define MT32_WIN32_THREADS 1
	/* Hilo hardware: el mismo que el SC-55, rawMidiSynthXboxHwThread() (ver
	 * rawmidisynth.h).  Solo hay un sintetizador abierto a la vez. */
#elif defined(_WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#define MT32_BARRIER() MemoryBarrier()
	#define MT32_WIN32_THREADS 1
#else
	#include <pthread.h>
	#define MT32_BARRIER() __sync_synchronize()
#endif

static inline size_t loadAcquire(const volatile size_t& v) { size_t x = v; MT32_BARRIER(); return x; }
static inline void   storeRelease(volatile size_t& v, size_t x) { MT32_BARRIER(); v = x; }

/* ------------------------------------------------------------------------- */
/* Objetos de Munt                                                           */
/* ------------------------------------------------------------------------- */

struct Mt32SynthMunt {
	MT32Emu::FileStream                 ctlFile;
	MT32Emu::FileStream                 pcmFile;
	const MT32Emu::ROMImage*            ctlRom;
	const MT32Emu::ROMImage*            pcmRom;
	MT32Emu::Synth*                     synth;
	MT32Emu::SampleRateConverter*       src;
	MT32Emu::DefaultMidiStreamParser*   parser;

	Mt32SynthMunt() : ctlRom(0), pcmRom(0), synth(0), src(0), parser(0) {}
	~Mt32SynthMunt()
	{
		delete parser;
		delete src;
		if (synth) { synth->close(); delete synth; }
		/* Solo despues de borrar el Synth que las usa (ver ROMInfo.h) */
		if (ctlRom) MT32Emu::ROMImage::freeROMImage(ctlRom);
		if (pcmRom) MT32Emu::ROMImage::freeROMImage(pcmRom);
	}
};

/* ------------------------------------------------------------------------- */
/* Hilo y evento (los de sc55synth.cpp)                                      */
/* ------------------------------------------------------------------------- */

#ifdef MT32_WIN32_THREADS
struct Mt32SynthThread {
	static DWORD WINAPI proc(LPVOID arg) { ((Mt32Synth*)arg)->workerLoop(); return 0; }
};
static void* wakeCreate()           { return (void*)CreateEvent(NULL, FALSE, FALSE, NULL); }
static void  wakeDestroy(void* e)   { CloseHandle((HANDLE)e); }
static void  wakeSignal(void* e)    { SetEvent((HANDLE)e); }
static void  wakeWait(void* e)      { WaitForSingleObject((HANDLE)e, INFINITE); }
static void* threadStart(Mt32Synth* s)
{
	HANDLE h = CreateThread(NULL, 256 * 1024, Mt32SynthThread::proc, s, CREATE_SUSPENDED, NULL);
	if (!h) return NULL;
#ifdef _XBOX
	XSetThreadProcessor(h, rawMidiSynthXboxHwThread());
#endif
	SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL);
	ResumeThread(h);
	return (void*)h;
}
static void threadJoin(void* t) { WaitForSingleObject((HANDLE)t, INFINITE); CloseHandle((HANDLE)t); }
#else
/* Solo para compilar y probar fuera de Windows/Xbox. */
struct Mt32Wake { pthread_mutex_t m; pthread_cond_t c; int set; };
struct Mt32SynthThread {
	static void* proc(void* arg) { ((Mt32Synth*)arg)->workerLoop(); return NULL; }
};
static void* wakeCreate() { Mt32Wake* w = new Mt32Wake; pthread_mutex_init(&w->m, NULL); pthread_cond_init(&w->c, NULL); w->set = 0; return w; }
static void  wakeDestroy(void* e) { Mt32Wake* w = (Mt32Wake*)e; pthread_cond_destroy(&w->c); pthread_mutex_destroy(&w->m); delete w; }
static void  wakeSignal(void* e) { Mt32Wake* w = (Mt32Wake*)e; pthread_mutex_lock(&w->m); w->set = 1; pthread_cond_signal(&w->c); pthread_mutex_unlock(&w->m); }
static void  wakeWait(void* e) { Mt32Wake* w = (Mt32Wake*)e; pthread_mutex_lock(&w->m); while (!w->set) pthread_cond_wait(&w->c, &w->m); w->set = 0; pthread_mutex_unlock(&w->m); }
static void* threadStart(Mt32Synth* s) { pthread_t* t = new pthread_t; if (pthread_create(t, NULL, Mt32SynthThread::proc, s)) { delete t; return NULL; } return t; }
static void  threadJoin(void* t) { pthread_join(*(pthread_t*)t, NULL); delete (pthread_t*)t; }
#endif

/* ------------------------------------------------------------------------- */
/* ROMs                                                                      */
/* ------------------------------------------------------------------------- */

static const char* const MT32_CONTROL_NAMES[]  = { "MT32_CONTROL.ROM", "mt32_control.rom", "MT32_CONTROL.BIN", NULL };
static const char* const MT32_PCM_NAMES[]      = { "MT32_PCM.ROM", "mt32_pcm.rom", "MT32_PCM.BIN", NULL };
static const char* const CM32L_CONTROL_NAMES[] = { "CM32L_CONTROL.ROM", "cm32l_control.rom", "CM-32L_CONTROL.ROM", "CM32L_CONTROL.BIN", NULL };
static const char* const CM32L_PCM_NAMES[]     = { "CM32L_PCM.ROM", "cm32l_pcm.rom", "CM-32L_PCM.ROM", "CM32L_PCM.BIN", NULL };

#if defined(_WIN32) || defined(_XBOX)
static const char MT32_SEP = '\\';
#else
static const char MT32_SEP = '/';
#endif

static std::string joinPath(const std::string& dir, const char* name)
{
	std::string p(dir);
	if (!p.empty() && p[p.size() - 1] != '\\' && p[p.size() - 1] != '/') p += MT32_SEP;
	return p.append(name);
}

static bool fileExists(const std::string& path)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	fclose(f);
	return true;
}

/* El primero de `names` que exista en dir, o cadena vacia. */
static std::string findFile(const std::string& dir, const char* const* names)
{
	for (size_t i = 0; names[i]; i++) {
		const std::string p = joinPath(dir, names[i]);
		if (fileExists(p)) return p;
	}
	return std::string();
}

static const char* modelName(Mt32Synth::Model model)
{
	return model == Mt32Synth::MODEL_CM32L ? "CM-32L" : "MT-32";
}

bool Mt32Synth::findRomSet(const std::string& systemDir, Model model, std::string& outDir)
{
	if (systemDir.empty()) return false;
	const char* const* ctl = (model == MODEL_CM32L) ? CM32L_CONTROL_NAMES : MT32_CONTROL_NAMES;
	const char* const* pcm = (model == MODEL_CM32L) ? CM32L_PCM_NAMES : MT32_PCM_NAMES;
	static const char* const subdirs[] = { "", "mt32", "cm32l", "cm64", "roland", "munt", "prboom" };
	for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); i++) {
		const std::string dir = subdirs[i][0] ? joinPath(systemDir, subdirs[i]) : systemDir;
		if (!findFile(dir, ctl).empty() && !findFile(dir, pcm).empty()) {
			outDir = dir;
			return true;
		}
	}
	return false;
}

/* Abre `path` como ROM completa del tipo pedido. */
static const MT32Emu::ROMImage* loadRom(MT32Emu::FileStream& file, const std::string& path,
                                        MT32Emu::ROMInfo::Type type, const char* model)
{
	if (!file.open(path.c_str())) {
		LOG_ERROR("MIDI %s: no se puede abrir la ROM %s", model, path.c_str());
		return NULL;
	}
	const MT32Emu::ROMImage* rom = MT32Emu::ROMImage::makeROMImage(&file);
	const MT32Emu::ROMInfo* info = rom ? rom->getROMInfo() : NULL;
	if (!info || info->type != type || info->pairType != MT32Emu::ROMInfo::Full) {
		LOG_ERROR("MIDI %s: ROM no reconocida, o partida en varios ficheros: %s", model, path.c_str());
		if (rom) MT32Emu::ROMImage::freeROMImage(rom);
		return NULL;
	}
	LOG_INFO("MIDI %s: ROM %s (%s)", model, path.c_str(), info->description);
	return rom;
}

/* ------------------------------------------------------------------------- */
/* Ciclo de vida (hilo principal)                                            */
/* ------------------------------------------------------------------------- */

Mt32Synth::Mt32Synth()
	: m_munt(0), m_model(MODEL_MT32), m_outRate(44100), m_volumePct(100), m_srcRate(0),
	  m_au(0), m_auW(0), m_auR(0), m_midi(0), m_midiW(0), m_midiR(0), m_quit(0),
	  m_overflowReported(false), m_underrunReported(false), m_framesMixed(0),
	  m_thread(0), m_wake(0)
{
}

Mt32Synth::~Mt32Synth()
{
	close();
}

bool Mt32Synth::open(const std::string& romDir, Model model, int outRate)
{
	close();
	m_model = model;
	const char* name = modelName(model);

	const std::string ctlPath = findFile(romDir, model == MODEL_CM32L ? CM32L_CONTROL_NAMES : MT32_CONTROL_NAMES);
	const std::string pcmPath = findFile(romDir, model == MODEL_CM32L ? CM32L_PCM_NAMES : MT32_PCM_NAMES);
	if (ctlPath.empty() || pcmPath.empty()) {
		LOG_ERROR("MIDI %s: no hay un par de ROMs (control + PCM) en %s", name, romDir.c_str());
		return false;
	}

	m_outRate = (outRate > 0) ? outRate : 44100;
	m_au   = (int16_t*)malloc(AU_RING * 2 * sizeof(int16_t));
	m_midi = (uint8_t*)malloc(MIDI_RING);
	m_munt = new Mt32SynthMunt();
	if (!m_au || !m_midi) { close(); return false; }

	Mt32SynthMunt& u = *m_munt;
	u.ctlRom = loadRom(u.ctlFile, ctlPath, MT32Emu::ROMInfo::Control, name);
	u.pcmRom = u.ctlRom ? loadRom(u.pcmFile, pcmPath, MT32Emu::ROMInfo::PCM, name) : NULL;
	if (!u.ctlRom || !u.pcmRom) { close(); return false; }

	/* La configuracion del core (mt32emu_libretro.cpp): 32 parciales, salida
	 * analogica COARSE (32 kHz, la mas barata) y conversor GOOD.  La cola de
	 * eventos es mayor que la del core (1024) porque al reabrir a media
	 * partida MidiSynth le reenvia de golpe el estado de la cancion, que puede
	 * pasar de mil mensajes, y lo que no cabe en la cola Munt lo descarta. */
	u.synth = new MT32Emu::Synth();
	u.synth->setMIDIEventQueueSize(4096);
	u.synth->configureMIDIEventQueueSysexStorage(32768);
	u.synth->preallocateReverbMemory(true);
	if (!u.synth->open(*u.ctlRom, *u.pcmRom, MT32Emu::AnalogOutputMode_COARSE)) {
		LOG_ERROR("MIDI %s: Munt no ha podido abrir el sintetizador", name);
		close();
		return false;
	}
	u.parser = new MT32Emu::DefaultMidiStreamParser(*u.synth);
	/* El conversor de tasa lo crea el hilo en su primera pasada (m_srcRate) */
	m_srcRate = 0;

	m_auW = m_auR = 0;
	m_midiW = m_midiR = 0;
	m_quit = 0;
	m_overflowReported = m_underrunReported = false;
	m_framesMixed = 0;

	m_wake = wakeCreate();
	m_thread = m_wake ? threadStart(this) : NULL;
	if (!m_thread) {
		LOG_ERROR("MIDI %s: no se ha podido crear el hilo del sintetizador", name);
		close();
		return false;
	}

#ifdef _XBOX
	LOG_INFO("MIDI %s: cargado de %s (-> %d Hz), hilo propio en el hilo hardware %d",
	         name, romDir.c_str(), (int)m_outRate, rawMidiSynthXboxHwThread());
#else
	LOG_INFO("MIDI %s: cargado de %s (-> %d Hz), hilo propio", name, romDir.c_str(), (int)m_outRate);
#endif
	return true;
}

void Mt32Synth::close()
{
	if (m_thread) {
		m_quit = 1;
		MT32_BARRIER();
		wakeSignal(m_wake);
		threadJoin(m_thread);
		m_thread = 0;
	}
	if (m_wake) { wakeDestroy(m_wake); m_wake = 0; }
	delete m_munt; m_munt = 0;
	free(m_au);   m_au = 0;
	free(m_midi); m_midi = 0;
	m_srcRate = 0;
}

void Mt32Synth::setOutputRate(int rate)
{
	if (rate > 0) m_outRate = rate;   /* el hilo rehace el conversor en su siguiente pasada */
}

void Mt32Synth::setVolumePercent(int pct)
{
	if (pct < 0)   pct = 0;
	if (pct > 100) pct = 100;
	m_volumePct = pct;
}

/* ------------------------------------------------------------------------- */
/* Hilo de trabajo                                                           */
/* ------------------------------------------------------------------------- */

/* Pasa al analizador de Munt los bytes MIDI encolados.  Los mensajes entran en
 * la cola de eventos del sintetizador con la posicion actual de render, asi
 * que suenan al principio del siguiente bloque. */
void Mt32Synth::workerFeedMidi()
{
	size_t r = m_midiR;
	const size_t w = loadAcquire(m_midiW);
	size_t n = w - r;
	if (!n) return;
	while (n) {
		const size_t pos = r & (MIDI_RING - 1);
		size_t chunk = MIDI_RING - pos;
		if (chunk > n) chunk = n;
		m_munt->parser->parseStream(m_midi + pos, (MT32Emu::Bit32u)chunk);
		r += chunk;
		n -= chunk;
	}
	storeRelease(m_midiR, r);
}

/* Renderiza hasta `frames` frames, a la tasa de salida, en el anillo de audio. */
size_t Mt32Synth::workerRender(size_t frames)
{
	if (!frames) return 0;
	if (frames > BLOCK) frames = BLOCK;

	const int rate = m_outRate;
	if (rate != m_srcRate || !m_munt->src) {
		delete m_munt->src;
		m_munt->src = new MT32Emu::SampleRateConverter(*m_munt->synth, (double)rate,
		                                               MT32Emu::SamplerateConversionQuality_GOOD);
		m_srcRate = rate;
	}
	m_munt->src->getOutputSamples(m_block, (unsigned int)frames);

	const size_t w = m_auW;
	for (size_t i = 0; i < frames; i++) {
		int16_t* dst = m_au + ((w + i) & (AU_RING - 1)) * 2;
		dst[0] = m_block[i * 2];
		dst[1] = m_block[i * 2 + 1];
	}
	storeRelease(m_auW, w + frames);
	return frames;
}

/* Produce hasta que haya `target` frames esperando a mix(). */
void Mt32Synth::workerFill(size_t target)
{
	for (;;) {
		const size_t fill = m_auW - loadAcquire(m_auR);
		workerFeedMidi();
		if (fill >= target) break;
		if (m_quit) break;
		size_t want = target - fill;
		if (want > BLOCK) want = BLOCK;
		if (!workerRender(want)) break;
	}
}

void Mt32Synth::workerLoop()
{
	while (!m_quit) {
		workerFill(LEAD);
		/* Evento auto-reset: un aviso que llega mientras se trabaja no se pierde. */
		wakeWait(m_wake);
	}
}

/* ------------------------------------------------------------------------- */
/* Hilo de emulacion                                                         */
/* ------------------------------------------------------------------------- */

void Mt32Synth::writeByte(uint8_t b)
{
	if (!m_munt) return;
	const size_t w = m_midiW;
	if (w - loadAcquire(m_midiR) >= MIDI_RING) {
		if (!m_overflowReported)
			LOG_WARN("MIDI %s: cola MIDI llena, se descartan bytes", modelName(m_model));
		m_overflowReported = true;
		return;
	}
	m_midi[w & (MIDI_RING - 1)] = b;
	storeRelease(m_midiW, w + 1);
}

static inline int16_t clamp16(int32_t x)
{
	if (x > 32767)  return 32767;
	if (x < -32768) return -32768;
	return (int16_t)x;
}

void Mt32Synth::mix(int16_t* stereo, int frames)
{
	if (!m_munt || !stereo || frames <= 0) return;

	const int vol = m_volumePct;
	const size_t r = m_auR;
	const size_t avail = loadAcquire(m_auW) - r;
	const size_t n = ((size_t)frames < avail) ? (size_t)frames : avail;
	for (size_t i = 0; i < n; i++) {
		const int16_t* src = m_au + ((r + i) & (AU_RING - 1)) * 2;
		stereo[i * 2]     = clamp16((int32_t)stereo[i * 2]     + (int32_t)src[0] * vol / 100);
		stereo[i * 2 + 1] = clamp16((int32_t)stereo[i * 2 + 1] + (int32_t)src[1] * vol / 100);
	}
	storeRelease(m_auR, r + n);
	wakeSignal(m_wake);

	/* Lo que falte se queda en silencio: el emulador nunca espera al
	 * sintetizador.  Se ignoran los primeros segundos, mientras el hilo llena
	 * su margen. */
	if (n < (size_t)frames && m_framesMixed > (size_t)m_outRate * 2 && !m_underrunReported) {
		LOG_WARN("MIDI %s: el sintetizador va mas lento que el tiempo real (huecos en la musica)", modelName(m_model));
		m_underrunReported = true;
	}
	m_framesMixed += (size_t)frames;
}

void Mt32Synth::panic()
{
	if (!m_munt) return;
	/* Por la cola MIDI, como cualquier otro mensaje.  Sin Reset All
	 * Controllers: reiniciaria la expresion y el pitch bend de la cancion. */
	for (uint8_t ch = 0; ch < 16; ch++) {
		writeByte((uint8_t)(0xB0 | ch)); writeByte(64);  writeByte(0);   /* suelta el sustain */
		writeByte((uint8_t)(0xB0 | ch)); writeByte(123); writeByte(0);   /* All Notes Off */
	}
	/* El audio ya preparado es de antes del corte: se descarta */
	storeRelease(m_auR, loadAcquire(m_auW));
	wakeSignal(m_wake);
}
