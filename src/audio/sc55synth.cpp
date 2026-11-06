/* Roland SC-55 en un hilo propio para el sintetizador MIDI del frontend.
 * Ver sc55synth.h.  La logica de anillos, remuestreo y relleno es la de
 * libretro/px68k/sc55_libretro.c, que ya se probo a tiempo real en la 360. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/logger.h>

#include "sc55synth.h"
#include "sc55_frontend.h"

#if defined(_XBOX)
	#include <xtl.h>
	#include <ppcintrinsics.h>
	/* lwsync: las escrituras anteriores se ven antes que el indice publicado
	 * (release) y las lecturas posteriores no se adelantan a el (acquire). */
	#define SC55_BARRIER() __lwsync()
	#define SC55_WIN32_THREADS 1
	/* Hilo hardware de la 360 para la unidad*/
	// El hilo 2 lo usa dosbox para obtener la maxima velocidad, y no tenemos otro tan rapido.
	// Por tanto solo lo usamos en el caso de x68000. Para el resto de casos nos conformamos con el 4
	//const int SALVIA_SC55_XBOX_HWTHREAD = (std::strcmp(EMU_LIB_NAME, "px68k") == 0) ? 2 : 4;
	const int SALVIA_SC55_XBOX_HWTHREAD = 2;

#elif defined(_WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#define SC55_BARRIER() MemoryBarrier()
	#define SC55_WIN32_THREADS 1
#else
	#include <pthread.h>
	#define SC55_BARRIER() __sync_synchronize()
#endif



#define SC55_STR2(x) #x
#define SC55_STR(x) SC55_STR2(x)

static inline size_t loadAcquire(const volatile size_t& v) { size_t x = v; SC55_BARRIER(); return x; }
static inline void   storeRelease(volatile size_t& v, size_t x) { SC55_BARRIER(); v = x; }

/* ------------------------------------------------------------------------- */
/* Hilo y evento                                                             */
/* ------------------------------------------------------------------------- */

#ifdef SC55_WIN32_THREADS
struct Sc55SynthThread {
	static DWORD WINAPI proc(LPVOID arg) { ((Sc55Synth*)arg)->workerLoop(); return 0; }
};
static void* wakeCreate()           { return (void*)CreateEvent(NULL, FALSE, FALSE, NULL); }
static void  wakeDestroy(void* e)   { CloseHandle((HANDLE)e); }
static void  wakeSignal(void* e)    { SetEvent((HANDLE)e); }
static void  wakeWait(void* e)      { WaitForSingleObject((HANDLE)e, INFINITE); }
static void* threadStart(Sc55Synth* s)
{
	HANDLE h = CreateThread(NULL, 256 * 1024, Sc55SynthThread::proc, s, CREATE_SUSPENDED, NULL);
	if (!h) return NULL;
#ifdef _XBOX
	XSetThreadProcessor(h, SALVIA_SC55_XBOX_HWTHREAD);
#endif
	SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL);
	ResumeThread(h);
	return (void*)h;
}
static void threadJoin(void* t) { WaitForSingleObject((HANDLE)t, INFINITE); CloseHandle((HANDLE)t); }
#else
/* Solo para compilar y probar fuera de Windows/Xbox. */
struct Sc55Wake { pthread_mutex_t m; pthread_cond_t c; int set; };
struct Sc55SynthThread {
	static void* proc(void* arg) { ((Sc55Synth*)arg)->workerLoop(); return NULL; }
};
static void* wakeCreate() { Sc55Wake* w = new Sc55Wake; pthread_mutex_init(&w->m, NULL); pthread_cond_init(&w->c, NULL); w->set = 0; return w; }
static void  wakeDestroy(void* e) { Sc55Wake* w = (Sc55Wake*)e; pthread_cond_destroy(&w->c); pthread_mutex_destroy(&w->m); delete w; }
static void  wakeSignal(void* e) { Sc55Wake* w = (Sc55Wake*)e; pthread_mutex_lock(&w->m); w->set = 1; pthread_cond_signal(&w->c); pthread_mutex_unlock(&w->m); }
static void  wakeWait(void* e) { Sc55Wake* w = (Sc55Wake*)e; pthread_mutex_lock(&w->m); while (!w->set) pthread_cond_wait(&w->c, &w->m); w->set = 0; pthread_mutex_unlock(&w->m); }
static void* threadStart(Sc55Synth* s) { pthread_t* t = new pthread_t; if (pthread_create(t, NULL, Sc55SynthThread::proc, s)) { delete t; return NULL; } return t; }
static void  threadJoin(void* t) { pthread_join(*(pthread_t*)t, NULL); delete (pthread_t*)t; }
#endif

/* ------------------------------------------------------------------------- */
/* ROMs                                                                      */
/* ------------------------------------------------------------------------- */

static const char* const SC55_MK2_NAMES[SC55_ROM_COUNT] =
	{ "rom1.bin", "rom2.bin", "rom_sm.bin", "waverom1.bin", "waverom2.bin", NULL };
static const char* const SC55_MK1_NAMES[SC55_ROM_COUNT] =
	{ "sc55_rom1.bin", "sc55_rom2.bin", NULL, "sc55_waverom1.bin", "sc55_waverom2.bin", "sc55_waverom3.bin" };

#if defined(_WIN32) || defined(_XBOX)
static const char SC55_SEP = '\\';
#else
static const char SC55_SEP = '/';
#endif

static std::string joinPath(const std::string& dir, const char* name)
{
	std::string p(dir);
	if (!p.empty() && p[p.size() - 1] != '\\' && p[p.size() - 1] != '/') p += SC55_SEP;
	return p.append(name);
}

static bool fileExists(const std::string& path)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	fclose(f);
	return true;
}

static bool haveSet(const std::string& dir, const char* const names[SC55_ROM_COUNT])
{
	for (int slot = 0; slot < SC55_ROM_COUNT; slot++)
		if (names[slot] && !fileExists(joinPath(dir, names[slot])))
			return false;
	return true;
}

bool Sc55Synth::findRomSet(const std::string& systemDir, std::string& outDir, bool& outMk2)
{
	if (systemDir.empty()) return false;
	static const char* const subdirs[] = {
		"", "sc55", "SC-55", "roland", "nuked", "prboom",
		"prboom/sc55", "prboom/SC-55", "prboom/roland", "prboom/nuked"
	};
	for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); i++)
	{
		std::string dir(systemDir);
		if (subdirs[i][0])
		{
			std::string sub(subdirs[i]);
			for (size_t c = 0; c < sub.size(); c++) if (sub[c] == '/') sub[c] = SC55_SEP;
			dir = joinPath(systemDir, sub.c_str());
		}
		if (haveSet(dir, SC55_MK2_NAMES)) { outDir = dir; outMk2 = true;  return true; }
		if (haveSet(dir, SC55_MK1_NAMES)) { outDir = dir; outMk2 = false; return true; }
	}
	return false;
}

static bool loadSlot(sc55_t* unit, int slot, const std::string& path)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	long size = -1;
	if (fseek(f, 0, SEEK_END) == 0) size = ftell(f);
	if (size <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return false; }
	unsigned char* buf = (unsigned char*)malloc((size_t)size);
	if (!buf) { fclose(f); return false; }
	const bool readOk = (fread(buf, 1, (size_t)size, f) == (size_t)size);
	fclose(f);
	const bool ok = readOk && sc55_load_rom(unit, slot, buf, (size_t)size) != 0;
	free(buf);
	return ok;
}

bool Sc55Synth::loadRoms(const std::string& dir, bool mk2)
{
	const char* const* names = mk2 ? SC55_MK2_NAMES : SC55_MK1_NAMES;
	sc55_t* unit = sc55_new(mk2 ? SC55_MODEL_MK2 : SC55_MODEL_MK1);
	if (!unit) {
		LOG_ERROR("MIDI SC-55: sin memoria para la unidad");
		return false;
	}
	for (int slot = 0; slot < SC55_ROM_COUNT; slot++)
	{
		if (!names[slot]) continue;
		const std::string path = joinPath(dir, names[slot]);
		if (!loadSlot(unit, slot, path)) {
			LOG_ERROR("MIDI SC-55: ROM ausente o invalida: %s", path.c_str());
			sc55_free(unit);
			return false;
		}
	}
	sc55_reset(unit);
	m_unit = unit;
	m_nativeRate = sc55_rate(unit);
	return true;
}

/* ------------------------------------------------------------------------- */
/* Ciclo de vida (hilo principal)                                            */
/* ------------------------------------------------------------------------- */

Sc55Synth::Sc55Synth()
	: m_unit(0), m_nativeRate(66207), m_outRate(44100), m_volumePct(100),
	  m_native(0), m_nativeCount(0), m_nativePhase(0.0), m_ratio(66207.0 / 44100.0), m_ratioRate(0),
	  m_au(0), m_auW(0), m_auR(0), m_midi(0), m_midiW(0), m_midiR(0), m_quit(0),
	  m_overflowReported(false), m_underrunReported(false), m_framesMixed(0),
	  m_thread(0), m_wake(0), m_dcR(0.9995f), m_dcInL(0), m_dcInR(0), m_dcOutL(0), m_dcOutR(0),
	  m_booted(0), m_bootMs(0), m_bootReported(false)
{
}

Sc55Synth::~Sc55Synth()
{
	close();
}

bool Sc55Synth::open(const std::string& romDir, int outRate)
{
	close();

	bool mk2 = haveSet(romDir, SC55_MK2_NAMES);
	if (!mk2 && !haveSet(romDir, SC55_MK1_NAMES)) {
		LOG_ERROR("MIDI SC-55: no hay un juego de ROMs completo en %s", romDir.c_str());
		return false;
	}

	m_native = (int32_t*)malloc(NATIVE_MAX * 2 * sizeof(int32_t));
	m_au     = (int32_t*)malloc(AU_RING * 2 * sizeof(int32_t));
	m_midi   = (uint8_t*)malloc(MIDI_RING);
	if (!m_native || !m_au || !m_midi || !loadRoms(romDir, mk2)) {
		close();
		return false;
	}

	m_outRate     = (outRate > 0) ? outRate : 44100;
	m_ratioRate   = 0;   /* el hilo calcula el ratio en su primera pasada */
	m_nativeCount = 0;
	m_nativePhase = 0.0;
	m_auW = m_auR = 0;
	m_midiW = m_midiR = 0;
	m_quit = 0;
	m_overflowReported = m_underrunReported = false;
	m_framesMixed = 0;
	m_dcInL = m_dcInR = m_dcOutL = m_dcOutR = 0.0f;
	m_booted = 0;
	m_bootReported = false;

	m_wake = wakeCreate();
	m_thread = m_wake ? threadStart(this) : NULL;
	if (!m_thread) {
		LOG_ERROR("MIDI SC-55: no se ha podido crear el hilo de la unidad");
		close();
		return false;
	}

	/* Fuera de la llamada: LOG_INFO es una macro y un #ifdef entre sus
	 * argumentos no es valido (MSVC lo rechaza). */
#ifdef _XBOX
	const char* threadInfo = " en el hilo hardware " SC55_STR(SALVIA_SC55_XBOX_HWTHREAD);
#else
	const char* threadInfo = "";
#endif
	LOG_INFO("MIDI SC-55: %s cargado de %s (%u Hz nativos -> %d Hz), hilo propio%s",
		mk2 ? "SC-55mkII" : "SC-55", romDir.c_str(), m_nativeRate, (int)m_outRate, threadInfo);
	return true;
}

void Sc55Synth::close()
{
	if (m_thread) {
		m_quit = 1;
		SC55_BARRIER();
		wakeSignal(m_wake);
		threadJoin(m_thread);
		m_thread = 0;
	}
	if (m_wake) { wakeDestroy(m_wake); m_wake = 0; }
	if (m_unit) { sc55_free(m_unit); m_unit = 0; }
	free(m_native); m_native = 0;
	free(m_au);     m_au = 0;
	free(m_midi);   m_midi = 0;
	m_nativeCount = 0;
	m_nativePhase = 0.0;
}

void Sc55Synth::setOutputRate(int rate)
{
	if (rate > 0) m_outRate = rate;   /* el hilo rehace el ratio en su siguiente pasada */
}

void Sc55Synth::setVolumePercent(int pct)
{
	if (pct < 0)   pct = 0;
	if (pct > 100) pct = 100;
	m_volumePct = pct;
}

/* ------------------------------------------------------------------------- */
/* Hilo de trabajo                                                           */
/* ------------------------------------------------------------------------- */

/* Pasa a la unidad los bytes MIDI encolados, tantos como admita su cola serie.
 * El resto espera a la siguiente pasada en vez de pisar entrada sin leer. */
void Sc55Synth::workerFeedMidi()
{
	size_t r = m_midiR;
	const size_t w = loadAcquire(m_midiW);
	size_t n = w - r;
	if (!n) return;
	const size_t room = sc55_midi_room(m_unit);
	if (n > room) n = room;
	while (n) {
		const size_t pos = r & (MIDI_RING - 1);
		size_t chunk = MIDI_RING - pos;
		if (chunk > n) chunk = n;
		sc55_midi(m_unit, m_midi + pos, chunk);
		r += chunk;
		n -= chunk;
	}
	storeRelease(m_midiR, r);
}

/* Deja una muestra nativa a la izquierda del punto de interpolacion. */
void Sc55Synth::workerDropNative()
{
	size_t drop = (size_t)m_nativePhase;
	if (drop >= m_nativeCount) drop = m_nativeCount ? m_nativeCount - 1 : 0;
	if (drop) {
		memmove(m_native, m_native + drop * 2, (m_nativeCount - drop) * 2 * sizeof(int32_t));
		m_nativeCount -= drop;
		m_nativePhase -= (double)drop;
	}
}

/* Ejecuta la unidad y anade hasta `frames` frames, a la tasa de salida, al
 * anillo de audio. */
size_t Sc55Synth::workerRender(size_t frames)
{
	if (!frames) return 0;

	const int rate = m_outRate;
	if (rate != m_ratioRate) {
		m_ratio = (double)m_nativeRate / (double)rate;
		m_dcR = 1.0f - (2.0f * 3.14159265f * 3.0f / (float)rate);   /* corte en ~3 Hz */
		m_ratioRate = rate;
	}

	const size_t w = m_auW;
	workerDropNative();
	const double lastNeeded = m_nativePhase + (double)(frames - 1) * m_ratio + 1.0;
	size_t need = (lastNeeded >= 0.0) ? (size_t)lastNeeded + 1 : 2;
	if (need > NATIVE_MAX) need = NATIVE_MAX;
	if (need > m_nativeCount) {
		const size_t add = need - m_nativeCount;
		if (sc55_run(m_unit, m_native + m_nativeCount * 2, add) == add)
			m_nativeCount += add;
	}

	size_t i;
	for (i = 0; i < frames; i++) {
		const size_t idx = (size_t)m_nativePhase;
		if (idx + 1 >= m_nativeCount) break;
		const double frac = m_nativePhase - (double)idx;
		const int32_t* a = m_native + idx * 2;
		const int32_t l = (int32_t)((double)a[0] * (1.0 - frac) + (double)a[2] * frac);
		const int32_t r = (int32_t)((double)a[1] * (1.0 - frac) + (double)a[3] * frac);
		int32_t* dst = m_au + ((w + i) & (AU_RING - 1)) * 2;
		/* Fondo de escala +-2^29: /16384 lo deja en el dominio de 16 bits.
		 * Bloqueador de DC (paso alto de un polo, ~3 Hz): la salida del chip
		 * lleva un desplazamiento constante de ~1024 que en el SC-55 real
		 * quitan los condensadores de salida. */
		const float xl = (float)l * (1.0f / 16384.0f), xr = (float)r * (1.0f / 16384.0f);
		m_dcOutL = xl - m_dcInL + m_dcR * m_dcOutL; m_dcInL = xl;
		m_dcOutR = xr - m_dcInR + m_dcR * m_dcOutR; m_dcInR = xr;
		dst[0] = (int32_t)m_dcOutL;
		dst[1] = (int32_t)m_dcOutR;
		m_nativePhase += m_ratio;
	}
	workerDropNative();
	storeRelease(m_auW, w + i);
	return i;
}

/* Produce hasta que haya `target` frames esperando a mix(). */
void Sc55Synth::workerFill(size_t target)
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

/* El firmware tarda en arrancar (~1,6 s de tiempo emulado en el SC-55): hasta
 * entonces no atiende el MIDI y lo que llega se pierde, y los juegos mandan el
 * reset GS y los cambios de programa nada mas empezar.  Asi que, antes de nada,
 * la unidad se ejecuta sola (sin pasarle MIDI ni producir audio) hasta que el
 * DAC empieza a sacar senal, mas un margen.  Los bytes que el core mande
 * mientras tanto esperan en la cola MIDI.  DOSBox Pure hace lo mismo con su
 * propio SC-55 (MIDI_Reset + ciclos de arranque). */
void Sc55Synth::workerBoot()
{
	const size_t chunk = NATIVE_MAX;
	const size_t maxFrames = (size_t)m_nativeRate * 10;        /* tope: 10 s */
	const size_t extraFrames = (size_t)m_nativeRate / 4;       /* margen: 0,25 s */
	size_t done = 0, readyAt = 0;
	while (!m_quit && done < maxFrames) {
		const size_t got = sc55_run(m_unit, m_native, chunk);
		if (!got) break;
		if (!readyAt) {
			for (size_t i = 0; i < got * 2; i++)
				if (m_native[i]) { readyAt = done + i / 2 + 1; break; }
		}
		done += got;
		if (readyAt && done >= readyAt + extraFrames) break;
	}
	m_bootMs = (unsigned)((double)done * 1000.0 / (double)m_nativeRate);
	m_booted = 1;
}

void Sc55Synth::workerLoop()
{
	workerBoot();
	while (!m_quit) {
		workerFill(LEAD);
		/* Evento auto-reset: un aviso que llega mientras se trabaja no se pierde. */
		wakeWait(m_wake);
	}
}

/* ------------------------------------------------------------------------- */
/* Hilo de emulacion                                                         */
/* ------------------------------------------------------------------------- */

void Sc55Synth::writeByte(uint8_t b)
{
	if (!m_unit) return;
	const size_t w = m_midiW;
	if (w - loadAcquire(m_midiR) >= MIDI_RING) {
		if (!m_overflowReported)
			LOG_WARN("MIDI SC-55: cola MIDI llena, se descartan bytes");
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

void Sc55Synth::mix(int16_t* stereo, int frames)
{
	if (!m_unit || !stereo || frames <= 0) return;

	const int vol = m_volumePct;
	const size_t r = m_auR;
	const size_t avail = loadAcquire(m_auW) - r;
	const size_t n = ((size_t)frames < avail) ? (size_t)frames : avail;
	for (size_t i = 0; i < n; i++) {
		const int32_t* src = m_au + ((r + i) & (AU_RING - 1)) * 2;
		stereo[i * 2]     = clamp16((int32_t)stereo[i * 2]     + src[0] * vol / 100);
		stereo[i * 2 + 1] = clamp16((int32_t)stereo[i * 2 + 1] + src[1] * vol / 100);
	}
	storeRelease(m_auR, r + n);
	wakeSignal(m_wake);

	if (!m_bootReported && m_booted) {
		LOG_INFO("MIDI SC-55: firmware arrancado (%u ms de tiempo emulado)", m_bootMs);
		m_bootReported = true;
		m_framesMixed = 0;   /* el aviso de huecos cuenta desde aqui */
	}

	/* Lo que falte se queda en silencio: el emulador nunca espera a la unidad.
	 * Se ignoran los primeros segundos, mientras arranca el firmware y el hilo
	 * llena su margen. */
	if (n < (size_t)frames && m_bootReported && m_framesMixed > (size_t)m_outRate * 2 && !m_underrunReported) {
		LOG_WARN("MIDI SC-55: la unidad va mas lenta que el tiempo real (huecos en la musica)");
		m_underrunReported = true;
	}
	m_framesMixed += (size_t)frames;
}

void Sc55Synth::panic()
{
	if (!m_unit) return;
	/* Por la cola MIDI, como cualquier otro mensaje: la unidad la consume el
	 * hilo de trabajo. */
	/* Sin Reset All Controllers: tambien reiniciaria la expresion, la
	 * modulacion y el pitch bend de la cancion, que no los va a repetir. */
	for (uint8_t ch = 0; ch < 16; ch++) {
		writeByte((uint8_t)(0xB0 | ch)); writeByte(64);  writeByte(0);   /* suelta el sustain */
		writeByte((uint8_t)(0xB0 | ch)); writeByte(123); writeByte(0);   /* All Notes Off */
		writeByte((uint8_t)(0xB0 | ch)); writeByte(120); writeByte(0);   /* All Sound Off */
	}
	/* El audio ya preparado es de antes del corte: se descarta (m_auR es de
	 * este lado). */
	storeRelease(m_auR, loadAcquire(m_auW));
	wakeSignal(m_wake);
}
