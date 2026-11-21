#include "sync.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <utils/logger.h>
#include <io/presentclock.h>

#ifdef _XBOX
#include <xtl.h>
#elif defined(WIN)
#include <windows.h>
#include <xmmintrin.h>
/* SDL_XBOX_SetPresentInterval / SDL_XBOX_SetAdaptiveVSync: en Windows los da
 * la capa D3D9 propia, con los mismos nombres que en Xbox (no-ops). */
#include <video/win_d3d9.h>
#endif

#include <audio/audiorate.h>

/* Maxima diferencia (relativa) entre el ritmo del core y un numero entero de
 * vblanks para que el juego se ate al panel.  El audio se corrige con el mismo
 * factor, asi que el cambio de tono es como mucho este: 1% (un sexto de
 * semitono). */
#define MAX_TIMING_SKEW         0.01

/* Ocupacion del anillo de audio (%) por debajo de la cual un core que hace
 * frameskip por audio corre sin esperas hasta recuperarse.  Por encima del
 * aviso de "underrun probable" (25%) y por debajo del centro del DRC (50%). */
#define CATCHUP_PCT             40

/* Presentaciones seguidas que se pueden omitir por frames duplicados, para que
 * el overlay (FPS, mensajes, mira) no se congele si un core manda muchos. */
#define MAX_DUPED_SIN_PRESENTAR 5

/* Tope de cada espera de vblank (ms): si el reloj de presentacion deja de
 * latir (cambio de modo de video) se sigue en vez de colgar el frontend. */
#define VBLANK_ESPERA_MAX_MS    100

/* Re-ancla del planificador para acotar la aritmetica de frame * r. */
#define FRAMES_REANCLA          65536u

Sync::Sync(const int* modoCfg, AudioRateControl* rate)
{
	modoConfig  = modoCfg;
	modo        = modoCfg ? *modoCfg : SYNC_TO_VIDEO;
	coreFps     = FPS_DESIRED;
	ffActivo    = false;
	ffMult      = 1.0f;
	audioRate   = rate;

	audioCb            = NULL;
	catchUp            = false;
	coreDuplicado      = false;
	duplicadosSeguidos = 0;

	configurado = false;
	cfgEnJuego  = false;
	conRitmo    = true;
	porVblanks  = false;
	refresco    = 0.0;
	r           = 1.0;
	periodoMs   = 1000.0 / FPS_DESIRED;
	intervalo   = -1;     /* -1 = aun sin aplicar: el primer configure lo fija */
	adaptativo  = -1;
	skew        = 1.0;

	anclado    = false;
	frame      = 0;
	vblAncla   = 0;
	cpuAncla   = 0.0;
	finDeGrupo = true;

	/* Contadores para el overlay */
	fps         = FPS_DESIRED;
	g_actualFps = FPS_DESIRED;
	utilization = 0.0f;
	memset(fpsText, 0, sizeof(fpsText));
	memset(cpuText, 0, sizeof(cpuText));
	sprintf(fpsText, FPS_FORMAT, g_actualFps);
	sprintf(cpuText, CPU_FORMAT, utilization);
	g_lastFrameTick  = 0;
	g_frameTimeIndex = 0;
	for (int i = 0; i < FPS_AVG_COUNT; i++) g_frameTimes[i] = 1000 / FPS_DESIRED;
	cpu_prev_us     = 0;
	wall_prev_ms    = 0.0;
	cpu_prev_valid  = false;
	cpu_util_index  = 0;
	cpu_util_filled = 0;
	for (int i = 0; i < FPS_AVG_COUNT; i++) cpu_util_buffer[i] = 0.0f;

#ifdef SYNC_STATS
	statIni = 0.0;
	statFrames = statPresents = statTarde = statVblIni = 0;
	statEsperaMs = 0.0;
#endif
}

/* ===========================================================================
 * Configuracion
 * =========================================================================== */

void Sync::setCoreFps(double fpsCore) {
	if (fpsCore > 0.0) {
		coreFps = fpsCore;
		fps     = (float)fpsCore;
		const uint32_t ms = (uint32_t)(1000.0 / fpsCore + 0.5);
		for (int i = 0; i < FPS_AVG_COUNT; i++) g_frameTimes[i] = ms;
	}
	configurado = false;
}

void Sync::setFastForward(bool activo, float mult) {
	ffActivo    = activo;
	ffMult      = (mult > 0.0f) ? mult : 1.0f;
	configurado = false;
}

void Sync::setAudioStatusCallback(retro_audio_buffer_status_callback_t cb) {
	audioCb = cb;
	if (!cb) catchUp = false;
}

void Sync::resetGame() {
	audioCb            = NULL;
	catchUp            = false;
	coreDuplicado      = false;
	duplicadosSeguidos = 0;
	ffActivo           = false;
	ffMult             = 1.0f;
	configurado        = false;
}

/* Tras un Reset del device D3D (p. ej. al cambiar el refresco del panel): el
 * Reset devuelve el intervalo de presentacion y el present inmediato a los
 * valores de D3D_PP, y configure() solo los aplica cuando cambian.  Se olvida
 * lo aplicado para que se reapliquen, y se reconfigura para releer el
 * refresco. */
void Sync::invalidateDeviceState() {
	intervalo   = -1;
	adaptativo  = -1;
	configurado = false;
}

/* Recalcula la configuracion si ha cambiado el modo pedido, el fps del core,
 * el avance rapido o si se entra o se sale del juego. */
void Sync::actualizaConfig(bool enJuego) {
	if (modoConfig && *modoConfig != modo) {
		modo        = *modoConfig;
		configurado = false;
	}
	/* El contador de vblanks aun no latia cuando se configuro (arranque): en
	 * cuanto lo haga se pasa del reloj de CPU al de vblanks.  En Windows
	 * pc_vblanks() es siempre 0, asi que esto no se dispara nunca. */
	if (configurado && conRitmo && !porVblanks && refresco > 0.0 && pc_vblanks() != 0)
		configurado = false;
	if (!configurado || enJuego != cfgEnJuego)
		configure(enJuego);
}

/* === configure ===============================================================
 * Decide el reloj del frame (ver la tabla de sync.h) y aplica lo que depende de
 * el: el vsync del Present, el present inmediato del modo adaptativo y el
 * factor de tiempo del audio.  Solo corre cuando algo cambia. */
void Sync::configure(bool enJuego) {
	configurado = true;
	cfgEnJuego  = enJuego;

	const float  hz       = pc_refresh_hz();
	refresco              = (hz > 0.0f) ? (double)hz : 0.0;
	const int    m       = enJuego ? modo : SYNC_TO_VIDEO;   /* los menus, siempre por video */
	const bool   ff       = enJuego && ffActivo;

	double fpsEfec = enJuego ? coreFps : ((refresco > 0.0) ? refresco : (double)FPS_DESIRED);
	if (ff) fpsEfec *= ffMult;
	if (fpsEfec <= 0.0) fpsEfec = FPS_DESIRED;

	/* Audio y Ninguna no esperan: el reloj es el audio o no hay reloj. */
	conRitmo  = !enJuego || ff || m == SYNC_TO_VIDEO || m == SYNC_TO_VIDEO_ADAPT;
	periodoMs = 1000.0 / fpsEfec;
	r         = ((refresco > 0.0) ? refresco : (double)FPS_DESIRED) / fpsEfec;

	/* Atar el juego al panel si el ritmo esta a menos de MAX_TIMING_SKEW de un
	 * numero entero de vblanks.  skew = (refresco / n) / fpsCore = r / n. */
	double skewNuevo = 1.0;
	if (!ff) {
		const double n = floor(r + 0.5);
		if (n >= 1.0 && fabs(r - n) <= n * MAX_TIMING_SKEW) {
			if (enJuego && refresco > 0.0) skewNuevo = r / n;
			r = n;
		}
	}

	porVblanks = conRitmo && refresco > 0.0 && pc_vblanks() != 0 && r >= 0.05 && r <= 8.0;

	/* El factor de tiempo solo vale cuando el juego va al ritmo de los vblanks;
	 * con el reloj de CPU el juego corre a su fps exacto. */
	if (!porVblanks || !conRitmo) skewNuevo = 1.0;

	const int iv = conRitmo ? 1 : 0;
	const int ad = (enJuego && !ff && m == SYNC_TO_VIDEO_ADAPT) ? 1 : 0;
	if (iv != intervalo)  { SDL_XBOX_SetPresentInterval(iv); intervalo  = iv; }
	if (ad != adaptativo) { SDL_XBOX_SetAdaptiveVSync(ad);   adaptativo = ad; }
	if (audioRate && skewNuevo != skew) {
		audioRate->setTimingSkew(skewNuevo);
		skew = skewNuevo;
	}

	reancla();

	LOG_DEBUG("Sync: %s modo=%d ff=%d fps=%.4f panel=%.3f r=%.4f reloj=%s vsync=%d adapt=%d skew=%.5f\n",
		enJuego ? "juego" : "menu", m, ff ? 1 : 0, fpsEfec, refresco, r,
		!conRitmo ? "ninguno" : (porVblanks ? "vblanks" : "cpu"), iv, ad, skew);
}

void Sync::reancla() {
	anclado = false;
	frame   = 0;
}

/* ===========================================================================
 * Por frame
 * =========================================================================== */

/* Antes de retro_run: el estado del anillo de audio, para el frameskip. */
void Sync::beginGameFrame(size_t audioUsado, size_t audioCapacidad) {
	const unsigned ocup = audioCapacidad ? (unsigned)(audioUsado * 100 / audioCapacidad) : 0;

	if (audioCb)
		audioCb(true, ocup, audioUsado < audioCapacidad / 4);

	catchUp = (audioCb != NULL) && audioCapacidad != 0 && ocup < CATCHUP_PCT;
}

void Sync::onVideoRefresh(bool duplicado) {
	coreDuplicado = duplicado;
}

/* Con r < 1 (avance rapido) varios frames comparten vblank: solo el ultimo de
 * cada grupo se presenta y espera.  Frame i -> vblank floor(i * r). */
bool Sync::calculaFinDeGrupo() const {
	if (!conRitmo || r >= 1.0 || (cfgEnJuego && catchUp))
		return true;
	const unsigned int i = anclado ? frame + 1 : 1;
	return floor((double)(i + 1) * r) > floor((double)i * r);
}

bool Sync::shouldPresent(bool enJuego) {
	actualizaConfig(enJuego);

	bool presenta = true;

	/* Frame que el core no ha dibujado (frameskip): no hay nada nuevo que
	 * presentar, y presentarlo haria esperar al vblank a un frame que el core
	 * se salto justo para ganar tiempo. */
	if (enJuego && coreDuplicado && duplicadosSeguidos < MAX_DUPED_SIN_PRESENTAR) {
		presenta = false;
		duplicadosSeguidos++;
	} else {
		duplicadosSeguidos = 0;
	}
	coreDuplicado = false;

	finDeGrupo = calculaFinDeGrupo();
	if (!finDeGrupo)
		presenta = false;

#ifdef SYNC_STATS
	if (presenta) statPresents++;
#endif
	return presenta;
}

/* === endFrame ================================================================
 * La UNICA espera del frame.  Se llama despues del Present (si lo hubo), asi
 * que la latencia de entrada es la minima: el frame ya esta en cola y aqui se
 * espera al vblank en el que se va a mostrar. */
void Sync::endFrame(bool enJuego, GameTicks& gameTicks) {
	const double ahora = Constant::getTicks();
	gameTicks.ticks++;
	gameTicks.dt       = (float)(ahora - gameTicks.lastTime);
	gameTicks.lastTime = ahora;

	actualizaConfig(enJuego);

#ifdef SYNC_STATS
	if (statIni == 0.0) { statIni = ahora; statVblIni = pc_vblanks(); }
	statFrames++;
	if (ahora - statIni >= 2000.0) {
		const unsigned vbl = pc_vblanks() - statVblIni;
		LOG_INFO("[SYNC] %u frames %u presents tarde=%u vbl/present=%.3f espera=%.2f ms/frame reloj=%s r=%.4f catchup=%d\n",
			statFrames, statPresents, statTarde,
			statPresents ? (double)vbl / (double)statPresents : 0.0,
			statEsperaMs / (double)statFrames,
			!conRitmo ? "ninguno" : (porVblanks ? "vblanks" : "cpu"), r, catchUp ? 1 : 0);
		statIni = ahora; statVblIni = pc_vblanks();
		statFrames = statPresents = statTarde = 0;
		statEsperaMs = 0.0;
	}
#endif

	/* Sin reloj (Audio, Ninguna) o recuperando con frameskip: no se espera, y el
	 * planificador se queda pegado al presente para no arrastrar deuda. */
	if (!conRitmo || (enJuego && catchUp)) {
		reancla();
		return;
	}

#ifdef SYNC_STATS
	const double t0 = Constant::getTicks();
#endif
	if (porVblanks) esperaVblank();
	else            esperaCpu();
#ifdef SYNC_STATS
	statEsperaMs += Constant::getTicks() - t0;
#endif
}

/* Pacer por vblanks: espera BLOQUEANTE al evento del vblank, sin girar. */
void Sync::esperaVblank() {
	const unsigned int ahora = pc_vblanks();

	if (!anclado) {
		vblAncla = ahora;
		frame    = 0;
		anclado  = true;
	}
	++frame;

	/* Frame intermedio de un grupo (avance rapido): ni se espera. */
	if (!finDeGrupo)
		return;

	const unsigned int objetivo = vblAncla + (unsigned int)floor((double)frame * r);

	/* Llega tarde: no se persigue la deuda (eso es el efecto camara rapida),
	 * se re-ancla en el presente.  Comparacion con signo por si el contador
	 * gira. */
	if ((int)(objetivo - ahora) < 0) {
		vblAncla = ahora;
		frame    = 0;
#ifdef SYNC_STATS
		statTarde++;
#endif
		return;
	}

	pc_wait_vblank(objetivo, VBLANK_ESPERA_MAX_MS);

	if (frame >= FRAMES_REANCLA) {
		vblAncla += (unsigned int)floor((double)frame * r);
		frame     = 0;
	}
}

/* Limitador por reloj de CPU (Windows, o si no hay contador de vblanks):
 * duerme el grueso con SDL_Delay y clava el instante con una espera activa
 * corta. */
void Sync::esperaCpu() {
	const double ahora = Constant::getTicks();

	if (!anclado) {
		cpuAncla = ahora;
		frame    = 0;
		anclado  = true;
	}
	++frame;

	const double limite = cpuAncla + (double)frame * periodoMs;

	/* Mas de un frame tarde (carga, savestate): se re-ancla. */
	if (limite < ahora - periodoMs) {
		cpuAncla = ahora;
		frame    = 0;
#ifdef SYNC_STATS
		statTarde++;
#endif
		return;
	}

	if (limite > ahora) {
		const double resto = limite - ahora;
		if (resto > 2.0)
			SDL_Delay((Uint32)(resto - 1.5));

		#if defined(_XBOX) || defined(WIN)
			const long long limiteTk = (long long)(limite * Constant::getTickFreqMs());
			while (Constant::getRawTicks() < limiteTk) {
				#ifdef _XBOX
					YieldProcessor();
				#else
					_mm_pause();
				#endif
			}
		#else
			while (Constant::getTicks() < limite) { }
		#endif
	}

	if (frame >= FRAMES_REANCLA) {
		cpuAncla += (double)frame * periodoMs;
		frame     = 0;
	}
}

/* ===========================================================================
 * Contadores para el overlay
 * =========================================================================== */

void Sync::update_fps_counter(bool updateFpsOverlay, uint32_t currentTick) {
	if (g_lastFrameTick == 0) g_lastFrameTick = currentTick;

	// Tiempo real desde el frame anterior, al buffer circular
	uint32_t frameTime = currentTick - g_lastFrameTick;
	g_lastFrameTick = currentTick;

	g_frameTimes[g_frameTimeIndex] = frameTime;
	g_frameTimeIndex++;
	if (g_frameTimeIndex == FPS_AVG_COUNT) g_frameTimeIndex = 0;

	if (updateFpsOverlay){
		uint32_t totalTime = 0;
		for (int i = 0; i < FPS_AVG_COUNT; i++) {
			totalTime += g_frameTimes[i];
		}
		// FPS = (1000 * frames) / tiempo_total
		if (totalTime > 0) {
			g_actualFps = TIME_AVG_COUNT / (float)totalTime;
		}
	}
}

/* === sample_cpu_utilization ==================================================
 * Porcentaje del tiempo real que el hilo principal estuvo ejecutando en la CPU
 * (GetThreadTimes: user + kernel).  Las esperas bloqueantes (vblank, audio,
 * SDL_Delay) no cuentan; la espera activa si.  Ventana movil de FPS_AVG_COUNT
 * muestras, como el contador de FPS. */
void Sync::sample_cpu_utilization() {
#if defined(_XBOX) || defined(WIN)
	FILETIME creation_ft, exit_ft, kernel_ft, user_ft;
	if (!GetThreadTimes(GetCurrentThread(), &creation_ft, &exit_ft, &kernel_ft, &user_ft)) {
		return;
	}

	ULARGE_INTEGER k_ui, u_ui;
	k_ui.LowPart  = kernel_ft.dwLowDateTime;
	k_ui.HighPart = kernel_ft.dwHighDateTime;
	u_ui.LowPart  = user_ft.dwLowDateTime;
	u_ui.HighPart = user_ft.dwHighDateTime;
	const uint64_t cpu_now_us  = (k_ui.QuadPart + u_ui.QuadPart) / 10ULL;
	const double   wall_now_ms = Constant::getTicks();

	if (cpu_prev_valid) {
		const uint64_t delta_cpu_us  = (cpu_now_us > cpu_prev_us) ? (cpu_now_us - cpu_prev_us) : 0;
		const double   delta_wall_us = (wall_now_ms - wall_prev_ms) * 1000.0;

		if (delta_wall_us >= 1000.0) {  // minimo 1 ms para evitar ruido
			float pct = (float)(((double)delta_cpu_us / delta_wall_us) * 100.0);
			if (pct < 0.0f)   pct = 0.0f;
			if (pct > 200.0f) pct = 200.0f;

			cpu_util_buffer[cpu_util_index] = pct;
			cpu_util_index++;
			if (cpu_util_index == FPS_AVG_COUNT) cpu_util_index = 0;
			if (cpu_util_filled < FPS_AVG_COUNT) cpu_util_filled++;

			float sum = 0.0f;
			for (int i = 0; i < cpu_util_filled; i++) sum += cpu_util_buffer[i];
			utilization = sum / (float)cpu_util_filled;
			if (utilization > 100.0f) utilization = 100.0f;
		}
	}

	cpu_prev_us    = cpu_now_us;
	wall_prev_ms   = wall_now_ms;
	cpu_prev_valid = true;
#endif
}
