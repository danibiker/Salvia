#include "sync.h"
#include <SDL.h>
#include <math.h>
#include <stdarg.h>
#include <utils/logger.h>
#include <io/presentclock.h>   /* el reloj de presentacion: ver Fase 1 */

#ifdef _XBOX
#include <xtl.h>
#elif defined(WIN)
#include <windows.h>
#include <xmmintrin.h>
/* SDL_XBOX_SetPresentInterval / SDL_XBOX_GetVBlankStats: en Windows los da la
 * capa D3D9 propia, con los mismos nombres que en Xbox. */
#include <video/win_d3d9.h>
#endif

/* Volcado de las estadisticas de framepacing.  Ya no es una opcion de menu: se
 * activa definiendo FRAMEPACE_LOG en el proyecto, porque es una herramienta de
 * diagnostico (sirvio para medir el coste del limitador y para descubrir que el
 * despertar de SDL_Delay esta cuantizado al tick), no algo que el usuario tenga
 * que tocar.
 *
 * Sale por OutputDebugStringA ADEMAS del log de ficheros porque Logger::write se
 * compila a un no-op cuando el build no define DEBUG_LOG (es el caso del Release
 * de Xbox 360), y estas medidas hay que poder verlas en una build normal. */
#ifdef FRAMEPACE_LOG
static void pace_trace(const char* fmt, ...) {
	char buf[192];
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[sizeof(buf) - 1] = '\0';
	OutputDebugStringA(buf);
	LOG_INFO("%s", buf);
}
#endif

Sync::Sync(int syncMode){
	g_frameTimeIndex = 0;
	g_lastFrameTick = 0;
	g_actualFps = FPS_DESIRED;
	fps = FPS_DESIRED;
	utilization = 0.0;
	memset(fpsText, '\0', 50 * sizeof(char));
	sprintf(fpsText, FPS_FORMAT, g_actualFps);
	sprintf(cpuText, CPU_FORMAT, utilization);
	g_sync_last = syncMode;
	frameDelay = 1000.0f / (float)fps; // Aprox 16ms

	// Estado del medidor de utilizacion CPU (GetThreadTimes-based).
	cpu_prev_us       = 0;
	wall_prev_ms      = 0.0;
	cpu_prev_valid    = false;
	cpu_util_index    = 0;
	cpu_util_filled   = 0;
	for (int i = 0; i < FPS_AVG_COUNT; i++) cpu_util_buffer[i] = 0.0f;

	// Estado del framepacing.
	frameTarget   = (double)frameDelay;
	lastPresent   = 0.0;
	paceLogLast   = 0.0;
	// El margen del sleep arranca conservador (el valor fijo que tenia antes) y
	// converge en unos pocos frames a lo que de verdad mide la plataforma.
	sleepOverAvg  = 0.0;
	sleepOverDev  = 0.0;
	sleepMargin   = SLEEP_MARGIN_INIT;
	tickGridMs    = GRID_TICK_MS;
	gridPhase     = 0.0;
	gridGood      = 0;
	gridMiss      = 0;
	gridValid     = false;
	/* El device arranca con vsync encendido (SDL_xboxvideo.c lo pone a
	 * D3DPRESENT_INTERVAL_ONE), asi que el espejo arranca igual. */
	vsyncOn         = true;
	vsyncPermitido  = true;
	vsyncCambios    = 0;
	dtAvg           = 0.0;
	vsyncGuarda     = 0.0;
	audioOcup       = -1.0;   /* centinela: "sin muestra aun" */
	vblPrev         = 0;      /* 0 = sin muestra del contador de hardware */
	vblMissPrev     = 0;
	vblAnchor       = 0;      /* 0 = el pacer de vblanks aun sin anclar */
	vblFrame        = 0;
	syncTypeAnt     = -1;     /* el primer limit_fps aplica el modo */
	paceMask        = PACE_NADA;
	flipLast        = 0.0;
	vsyncLogLast    = 0.0;
	paceSummaryLast = 0.0;
	reset_pace(Constant::getTicks());
	reset_pace_stats();
}

/* === set_vsync_permitido =====================================================
 * El avance rapido tambien apaga el vsync, y dos duenos del mismo bit se
 * pelean: el adaptativo lo volveria a encender a mitad de un avance rapido.
 * Asi que el avance rapido pasa por aqui y mientras dure el adaptativo queda
 * inerte, sin perder el estado que tenia. */
void Sync::set_vsync_permitido(bool permitido) {
	vsyncPermitido  = permitido;

	/* Tambien por el render state, no por SDL_XBOX_SetVSync: el avance rapido
	 * sufria el MISMO fallo de pantalla negra con un core que renderiza por
	 * hardware, solo que nadie lo habia atado a esto. */
	if (!permitido) {
		if (vsyncOn) {
			SDL_XBOX_SetPresentInterval(0);
			vsyncOn = false;
			vsyncCambios++;
		}
		return;
	}

	/* Al salir del avance rapido se restaura el vsync y el adaptativo vuelve a
	 * decidir desde cero con el marcador a 0: si el core no llega lo apagara
	 * otra vez, y si llega se queda encendido. */
	if (!vsyncOn) {
		SDL_XBOX_SetPresentInterval(1);
		vsyncOn = true;
		vsyncCambios++;
	}
}

/* === note_audio_ocupacion ====================================================
 * Ocupacion del anillo de audio del frontend, en tanto por ciento, una muestra
 * por frame desde `updateGame`.
 *
 * Se promedia con el mismo peso lento que `dtAvg` (1/64, ~1 s) y por el mismo
 * motivo: el anillo sube y baja con cada lote de audio_batch, asi que la
 * muestra instantanea cruzaria cualquier umbral sin parar.  Lo que interesa es
 * si esta SECO de forma sostenida, no si un frame concreto pillo el valle. */
void Sync::note_audio_ocupacion(unsigned pct) {
	const double v = (double)pct;
	audioOcup = (audioOcup < 0.0) ? v : (audioOcup + (v - audioOcup) / 64.0);
}

/* === update_vsync_adaptativo =================================================
 * Ver el bloque largo de sync.h para el por que.  Aqui solo la mecanica.
 *
 * Lo que se decide: dejar que el Present bloquee al vblank cuesta ~0,7 ms si
 * vamos bien acompasados y hasta 13-14 ms si ya vamos descolgados -y entonces la
 * contrapresion cae ademas en los DrawPrimitive del plugin, DENTRO de
 * retro_run-.  Asi que con el core llegando se deja el vsync puesto (imagen sin
 * tearing, coste despreciable) y en cuanto no llega se quita.
 *
 * Dos entradas, y cada una cubre lo que la otra no:
 *
 *   diff   el instante de ESTE frame, para reaccionar ya a una caida brusca.
 *          Solo se usa el caso inequivoco -mas de un frame entero de deuda-,
 *          porque su signo es ruido (ver el bloque de sync.h).
 *   dtAvg  el periodo CONSEGUIDO, promediado: el regimen, con banda muerta.
 *
 * Es decision por frame porque conmutar es un SetRenderState, no un Reset del
 * device -esa via deja al core con recursos muertos y la pantalla negra-. */
void Sync::update_vsync_adaptativo(double diff) {
#if !VSYNC_ADAPTATIVO
	/* FASE 2: lo hace el HARDWARE.  `D3DRS_PRESENTIMMEDIATETHRESHOLD`, fijado
	 * una vez al crear el device (SDL_xboxvideo.c), convierte el Present en
	 * inmediato solo cuando el barrido ya va pasado del 80% del frame, o sea
	 * solo cuando llegamos tarde.  Un render state sustituye a toda esta
	 * maquina: dos calibraciones de umbrales (25/40 y 12/22), una racha de
	 * consecutivos, un integrador saturante, una banda muerta, una guarda
	 * anti-aleteo de 500 ms y hasta 2071 conmutaciones en un solo log.
	 *
	 * Se conserva el codigo, no se borra, porque es el A/B: con
	 * VSYNC_ADAPTATIVO a 1 vuelve la decision por software y se puede comparar
	 * contra el umbral del hardware. */
	(void)diff;
	return;
#else
	if (!vsyncPermitido) return;

	const double ahora = Constant::getTicks();
	bool quiere = vsyncOn;

	/* (1) ESCAPE INMEDIATO: deuda de mas de un frame ENTERO.  Es el mismo
	 * umbral con el que `limit_fps` re-ancla, o sea "esto no es jitter": un
	 * milisegundo de ruido no puede producirlo.  Es tambien exactamente lo que
	 * pasa al salir de un fundido a negro a una escena cargada -medido en el
	 * log 21: `diff -34.46`, dos frames de deuda, viniendo de `diff +12.44`-, y
	 * lo que hacia que la transicion se notase: por el regimen de abajo habria
	 * que esperar a que la media se moviese.
	 *
	 * ** Y ARMA LA GUARDA, que es lo que faltaba.  Sin ella las dos reglas se
	 * PELEAN: con el core a 57 fps `limit_fps` re-ancla cada ~3 frames, o sea
	 * que este escape salta cada ~3 frames y fuerza OFF, mientras el regimen de
	 * abajo lo vuelve a encender en cuanto la media baja del umbral.  Medido en
	 * el log 22: `camb=2071`, unas 20 conmutaciones por segundo, con la mitad de
	 * los frames bloqueando en el Present (`blk=46-105/120`). */
	if (diff < -frameTarget) {
		quiere      = false;
		vsyncGuarda = ahora + VSYNC_GUARDA_MS;
	}
	/* Dentro de la guarda no se reevalua el regimen: el escape acaba de decir
	 * que no llegamos y la media tarda en enterarse. */
	else if (ahora < vsyncGuarda) {
		/* nada */
	}
	/* (2) REGIMEN: el periodo CONSEGUIDO, con banda muerta.
	 *
	 * ** El criterio NO puede ser el signo de `diff`.  Medido en el log 21:
	 * `late=40-80 de 120` con `dt=16.67` EXACTO, o sea a pleno rendimiento.  El
	 * limitador apunta AL deadline, asi que caer unas decimas antes o despues es
	 * una moneda al aire, y `diff > 0` es un sorteo cuando el core si llega.  Eso
	 * produjo 91 conmutaciones en un solo log, y ni el integrador saturante lo
	 * arregla: la senal misma era ruido.
	 *
	 * `dtAvg` separa sin solaparse (mismo log): 16,63-16,81 en los tramos buenos
	 * contra 18,06-20,32 en los malos.  Y el `flip` lo corrobora: 0,70-0,81 ms
	 * de media con `blk=2-40/120` frente a 1,85-4,37 con `blk=59-92/120` y picos
	 * de 13-14 ms.  O sea que con el vsync ENCENDIDO y bien acompasado el Present
	 * cuesta 0,7 ms; solo duele cuando ya vamos descolgados. */
	else if (dtAvg > 0.0) {
		/* ** RETIRADO el disparador de "anillo de audio seco", y el motivo es una
		 * medida.  La idea era cubrir lo que el periodo no ve: un core que
		 * cumple el periodo de frame y aun asi no llega (Xenon64 asincrono, 60
		 * VI/s con el audio al 70%).  Pero estaba mal de raiz por dos cosas:
		 *
		 * 1) PALANCA EQUIVOCADA.  Mide el deficit de audio del CORE y reacciona
		 *    con un ajuste de VIDEO.  El anillo con Xenon64 asincrono esta a
		 *    0-18% -el DRC saturado contra un core que no produce lo que toca-,
		 *    y apagar el vsync no crea audio.  Eso se arregla en el core, con
		 *    frameskip por SET_AUDIO_BUFFER_STATUS_CALLBACK.
		 * 2) ALETEABA.  `camb` 33->43 en 16 s, porque el anillo vive justo
		 *    encima de los umbrales 12/22 que le puse -y esos umbrales ya iban
		 *    por su segunda calibracion-.
		 *
		 * `note_audio_ocupacion` y el campo `aud=` del log se quedan: como
		 * DIAGNOSTICO son utiles -distinguen "el core no llega" de "el frame no
		 * cuadra", que es justo lo que el periodo no puede ver-. Lo que no hacen
		 * es gobernar el vsync. */
		if (vsyncOn) {
			if (dtAvg > frameTarget + VSYNC_LENTO_MS)  quiere = false;
		} else {
			if (dtAvg < frameTarget + VSYNC_RAPIDO_MS) quiere = true;
		}
	}

	if (quiere == vsyncOn) return;

	SDL_XBOX_SetPresentInterval(quiere ? 1 : 0);
	vsyncOn = quiere;
	vsyncCambios++;
	vsyncGuarda = ahora + VSYNC_GUARDA_MS;

	if ((ahora - vsyncLogLast) >= 2000.0) {
		vsyncLogLast = ahora;
		LOG_INFO("[VSYNC] %s (objetivo %.3f ms, diff %.2f). Cambios: %d\n",
			vsyncOn ? "ON" : "OFF", frameTarget, diff, vsyncCambios);
	}
#endif /* VSYNC_ADAPTATIVO */
}

/* === anota_pace =============================================================
 * Cierra la contabilidad de "quien freno este frame" (Fase 4).
 *
 * El cuarto regulador -el Present- no se pasa como parametro porque no hay que
 * preguntarselo a nadie: ya se mide.  Un Present que bloquea al vblank tarda
 * milisegundos y uno que solo encola vuelve en microsegundos, asi que el propio
 * `flipMax` de este frame lo delata.  Se usa el mismo umbral de 2 ms que
 * `note_flip`. */
void Sync::anota_pace(bool esperoVblank, bool durmioTimer, bool modoAudio) {
	/* `flipLast`, NO `flipMax`: el maximo es de toda la ventana, asi que en
	 * cuanto un frame bloquease marcaria como frenados por el Present todos los
	 * siguientes y la mascara diria siempre 'P'. */
	const bool bloqueoPresent = (flipLast > 2.0);

	paceMask      = pace_fuentes(esperoVblank, durmioTimer, modoAudio, bloqueoPresent);
	paceMaskAcum |= paceMask;

	if (pace_cuantos(paceMask) > 1)
		++paceDoble;
}

/* === vblPorFrame / vblPacerActivo ===========================================
 *
 * Cuantos vblanks del panel dura un frame del core.  Con panel y core a 60 sale
 * 1,0; con un core PAL de 50 Hz en un panel de 60 sale 1,2, que es el valor que
 * se midio en el hardware (`vbl/frame = 1,202`).
 *
 * NO se fuerza a entero a proposito.  Redondear a 1 haria correr el juego PAL a
 * 60 Hz -un 20% rapido y desafinado- y redondear a 2 lo dejaria a 30. El ratio
 * fraccionario es lo que produce el reparto 5:6 correcto. */
double Sync::vblPorFrame() {
	const float hz = pc_refresh_hz();

	if (hz <= 0.0f || frameTarget <= 0.0)
		return 0.0;

	/* frameTarget es el periodo del core en ms, asi que su fps es 1000/target. */
	return (double)hz * frameTarget / 1000.0;
}

bool Sync::vblPacerActivo() {
	double r;

#if !VBL_PACER
	return false;               /* palanca: ver VBL_PACER en sync.h */
#endif
	if (pc_vblanks() == 0)
		return false;           /* plataforma sin reloj de presentacion */

	r = vblPorFrame();

	/* Ratios absurdos -refresco desconocido, un core que declara 500 fps- caen
	 * al camino de reloj de CPU en vez de producir esperas disparatadas. */
	return (r >= 0.2 && r <= 8.0);
}

/* === reset_pace ==============================================================
 * Re-ancla el planificador de deadlines en `now`.  Deliberadamente NO toca la
 * ventana de estadisticas: un core que no llega a velocidad plena re-ancla
 * cada pocos frames, y si eso limpiase los contadores nunca llegariamos a
 * completar una ventana - justo cuando mas interesa medir. */
void Sync::reset_pace(double now) {
	paceAnchor  = now;
	paceFrame   = 0;
	/* El pacer de vblanks se re-ancla con el mismo evento: si no, tras un hitch
	 * seguiria contando frames contra un ancla vieja y todos los objetivos
	 * saldrian en el pasado. */
	vblAnchor   = 0;
	vblFrame    = 0;
}

/* === reset_pace_stats ========================================================
 * Limpia solo los acumuladores de la ventana de estadisticas.  El estado del
 * margen adaptativo NO se toca: es una propiedad del reloj de la plataforma, no
 * de la ventana ni del core. */
void Sync::reset_pace_stats() {
	flipSum          = 0.0;
	flipMax          = 0.0;
	/* `flipLast` NO se limpia: es el estado del ultimo frame, no un acumulado
	 * de la ventana, y la mascara de pacing del primer frame de la ventana
	 * siguiente lo necesita. */
	flipCount        = 0;
	flipBlockedCount = 0;
	sleepSum         = 0.0;
	spinSum          = 0.0;
	paceSum     = 0.0;
	paceSumSq   = 0.0;
	paceMin     = 0.0;
	paceMax     = 0.0;
	paceCount   = 0;
	paceDrops   = 0;
	paceLate    = 0;
	/* Los acumulados de la ventana si, pero NO `vblPrev`/`vblMissPrev`: son el
	 * ultimo valor absoluto visto del contador del hardware y tienen que
	 * sobrevivir entre ventanas, o la primera muestra de cada ventana se
	 * perderia. */
	vblSum      = 0;
	vblMissSum  = 0;
	paceDoble    = 0;
	paceMaskAcum = PACE_NADA;
}

void Sync::initAverages(uint32_t avg){
	//memset(g_frameTimes, avg, sizeof g_frameTimes);
	for(int i = 0; i < FPS_AVG_COUNT; i++) {
        g_frameTimes[i] = avg;
    }
}

void Sync::init_fps_counter(float gameFps){
	if (gameFps > 0){
        this->fps = gameFps;
        // Mantenemos el frameDelay como double para el limitador de alta precision
        this->frameDelay = 1000.0f / gameFps;
        // El limitador de verdad usa frameTarget (double).  frameDelay se
        // conserva en float por compatibilidad con el resto de la clase:
        // redondear el periodo a float mete un sesgo fijo en cada frame, y
        // sumarlo 60 veces por segundo lo convierte en deriva acumulada.
        this->frameTarget = 1000.0 / (double)gameFps;
        
        // Para los promedios (que parecen usar enteros), usamos el redondeo mas cercano
        initAverages((uint32_t)(frameDelay + 0.5));
    } else {
        // Fallback por si gameFps es invalido
        this->frameTarget = (double)frameDelay;
        initAverages((uint32_t)frameDelay);
    }

    // Ritmo nuevo (juego nuevo, fast forward): re-anclamos el planificador.
    reset_pace(Constant::getTicks());
}

void Sync::update_fps_counter(bool updateFpsOverlay, uint32_t currentTick) {
    if (g_lastFrameTick == 0) g_lastFrameTick = currentTick; // Inicializacion en el primer uso
    
	// Calculamos cuanto tiempo ha pasado realmente desde el frame anterior
	uint32_t frameTime = currentTick - g_lastFrameTick;
	g_lastFrameTick = currentTick;

	// Guardamos el tiempo de este frame en el buffer circular
	g_frameTimes[g_frameTimeIndex] = frameTime;
	g_frameTimeIndex++;
	if (g_frameTimeIndex == FPS_AVG_COUNT) g_frameTimeIndex = 0;
	
	if (updateFpsOverlay){
		// Sumamos todos los tiempos almacenados
		uint32_t totalTime = 0;
		for (int i = 0; i < FPS_AVG_COUNT; i++) {
			totalTime += g_frameTimes[i];
		}

		// Calculamos la media (evitando division por cero)
		if (totalTime > 0) {
			// FPS = 1000ms / promedio_de_frame_en_ms
			// Es lo mismo que: (1000 * cantidad_de_frames) / tiempo_total
			g_actualFps = TIME_AVG_COUNT / (float)totalTime;
		}
	}
}

/* === sample_cpu_utilization ==================================================
 *
 * Mide la utilizacion REAL del CPU del thread principal usando
 * GetThreadTimes (Win32/Xbox 360 SDK).  Devuelve el porcentaje del wall
 * time que el thread estaba ejecutandose en CPU (user + kernel mode),
 * frente al tiempo dormido / bloqueado.
 *
 * Diferencias vs la metrica anterior (workTime / frameDelay * 100):
 *
 *   - Anterior: media wall time del trabajo entre dos limit_fps.  No
 *     distinguia trabajo real de espera bloqueante (WriteBlocking,
 *     WaitForSingleObject, SDL_Delay), asi que un frame con 14 ms de
 *     espera audio se contaba igual que uno con 14 ms de CPU activa.
 *
 *   - Nueva: GetThreadTimes solo suma tiempo en estado RUNNING del
 *     thread.  SDL_Delay y WaitForSingleObject ponen el thread en
 *     WAIT (no se cuenta).  El busy-wait al final de limit_fps
 *     (YieldProcessor) SI se cuenta porque el thread sigue running.
 *     Eso es correcto: busy-wait consume CPU; bloquear en evento no.
 *
 * Suavizado: ventana movil de FPS_AVG_COUNT muestras (mismo principio
 * que el FPS counter, para consistencia visual).  La utilizacion
 * promediada queda en this->utilization, lista para el overlay.
 *
 * Se llama desde GameMenu::updateFps() cada frame, en TODOS los modos
 * de sync (la metrica anterior solo se actualizaba en SYNC_TO_VIDEO).
 */
void Sync::sample_cpu_utilization() {
#if defined(_XBOX) || defined(WIN)
    FILETIME creation_ft, exit_ft, kernel_ft, user_ft;
    if (!GetThreadTimes(GetCurrentThread(),
                        &creation_ft, &exit_ft,
                        &kernel_ft, &user_ft)) {
        return;  // GetThreadTimes fallo (raro); preservar ultimo valor
    }

    // Convertir FILETIME (100-ns units) a microsegundos.  Sumamos
    // kernel + user porque ambos cuentan como tiempo CPU del thread.
    ULARGE_INTEGER k_ui, u_ui;
    k_ui.LowPart  = kernel_ft.dwLowDateTime;
    k_ui.HighPart = kernel_ft.dwHighDateTime;
    u_ui.LowPart  = user_ft.dwLowDateTime;
    u_ui.HighPart = user_ft.dwHighDateTime;
    uint64_t cpu_now_us = (k_ui.QuadPart + u_ui.QuadPart) / 10ULL;

    // Wall time del mismo sample.
    double wall_now_ms = Constant::getTicks();

    if (cpu_prev_valid) {
        // Delta CPU vs delta wall desde el sample anterior.
        uint64_t delta_cpu_us  = (cpu_now_us  > cpu_prev_us)
                                 ? (cpu_now_us - cpu_prev_us) : 0;
        double   delta_wall_ms = wall_now_ms - wall_prev_ms;
        // wall en ms, cpu en us -> convertir wall a us.
        double   delta_wall_us = delta_wall_ms * 1000.0;

        if (delta_wall_us >= 1000.0) {  // minimo 1 ms para evitar ruido
            float pct = (float)(((double)delta_cpu_us / delta_wall_us) * 100.0);

            // Clamp a [0, 200].  Por encima de 100% indica que el
            // sample mide CPU de varios HW threads (no esperado en
            // single-thread main, pero defensivo).
            if (pct < 0.0f)   pct = 0.0f;
            if (pct > 200.0f) pct = 200.0f;

            cpu_util_buffer[cpu_util_index] = pct;
            cpu_util_index++;
            if (cpu_util_index == FPS_AVG_COUNT) cpu_util_index = 0;
            if (cpu_util_filled < FPS_AVG_COUNT) cpu_util_filled++;

            // Promedio sobre slots VALIDOS (durante el warm-up usa
            // solo los que se han llenado, evitando arrastrar ceros).
            float sum = 0.0f;
            for (int i = 0; i < cpu_util_filled; i++) sum += cpu_util_buffer[i];
            utilization = sum / (float)cpu_util_filled;

            // Display cap a 100% - valores >100 confunden al usuario
            // (no tenemos varios threads que mostrar como CPU%).
            if (utilization > 100.0f) utilization = 100.0f;
        }
    }

    cpu_prev_us    = cpu_now_us;
    wall_prev_ms   = wall_now_ms;
    cpu_prev_valid = true;
#else
    /* Sin GetThreadTimes (otras plataformas): preservar el valor
     * actual o setear a 0.  No es critico fuera de la build de
     * Xbox 360 / Windows. */
    (void)0;
#endif
}

/* === note_flip ===============================================================
 * Cronometra el flip que acaba de salir.  Instrumentacion: un Present que
 * bloquea hasta el vblank tarda milisegundos, uno que solo encola vuelve en
 * microsegundos.  Sirve para saber si el frame se esta perdiendo tiempo dentro
 * del driver (medido en Salvia: 0.86 ms cuando el limitador ya ha esperado,
 * 11.7 ms si se le deja bloquear, y ese bloqueo es busy-wait: CPU al 100%). */
void Sync::note_flip(double durationMs) {
    if (durationMs < 0.0) durationMs = 0.0;
    flipSum += durationMs;
    if (durationMs > flipMax) flipMax = durationMs;
    /* Umbral a 2 ms y no a 1: con el Present bien acompasado el flip mide
     * 0,89-1,02 ms de forma consistente, asi que el umbral de 1 ms daba
     * `blk=120/120` en frames perfectos y habia dejado de discriminar.  Un
     * Present que de verdad bloquea al vblank mide 8-14 ms (medido), asi que 2
     * ms separa de sobra sin acercarse al ruido. */
    flipLast = durationMs;   /* el de ESTE frame, para la mascara de pacing */
    if (durationMs > 2.0) flipBlockedCount++;
    flipCount++;
}

/* === note_present ============================================================
 *
 * Se llama justo DESPUES del flip, con el reloj de alta resolucion, y produce
 * las estadisticas de framepacing REALES (present-to-present): media, sigma,
 * min/max, presentaciones que ocuparon 2 o mas periodos ("drops"), frames que
 * llegaron tarde al deadline, y el reparto del coste del limitador entre sueno
 * y espera activa.  Es la unica medida que corresponde con lo que ve el
 * usuario; el contador de FPS clasico promedia 20 frames y esconde
 * precisamente el jitter que buscamos.
 */
void Sync::note_present(double now) {
    const double prev = lastPresent;
    lastPresent = now;

    if (prev <= 0.0) return;               // primera presentacion de la sesion
    const double dt = now - prev;
    if (dt <= 0.0 || dt > 500.0) return;   // hitch de carga / savestate: no cuenta

    /* Media movil del periodo CONSEGUIDO, para el vsync adaptativo.  Es la
     * senal, y el filtro va aqui y no en limit_fps porque el descarte del hitch
     * de dos lineas arriba tambien tiene que protegerla.
     *
     * Peso 1/64 -> constante de tiempo de ~64 frames, ~1 s.  Empezo en 1/16 y
     * era DEMASIADO NERVIOSA: medido en el log 22, con `dt=17.73` estable la
     * media se movia entre 16,34 y 17,83 y cruzaba el umbral sin parar, porque
     * el `dt` por frame va a rafagas (unos a 16,67 y otros a 22).  Puede ser
     * lenta sin coste: la reaccion rapida la cubre el escape inmediato de
     * `update_vsync_adaptativo`, que no pasa por aqui. */
    dtAvg = (dtAvg <= 0.0) ? dt : (dtAvg + (dt - dtAvg) / 64.0);

    /* Vblanks del hardware en esta ventana.  Dos lecturas volatiles por frame,
     * coste despreciable, y es la unica metrica de salud del pacing que resiste
     * el contraste: `vblSum/paceCount - 1,0` es la fraccion EXACTA de frames que
     * se comen un vblank extra.  Medido: 0,002-0,010 donde `paceLate` decia
     * 0,33-0,63.  Devuelve 0 en plataformas sin reloj de hardware (Windows), y
     * entonces el campo sale a 0 y hay que mirar `late`. */
    {
        unsigned int vb = 0, miss = 0;
        SDL_XBOX_GetVBlankStats(&vb, &miss);
        if (vb != 0) {
            if (vblPrev != 0) {
                vblSum     += vb - vblPrev;
                vblMissSum += miss - vblMissPrev;
            }
            vblPrev     = vb;
            vblMissPrev = miss;
        }
    }

    paceSum   += dt;
    paceSumSq += dt * dt;
    if (paceCount == 0 || dt < paceMin) paceMin = dt;
    if (dt > paceMax)                   paceMax = dt;
    paceCount++;

    /* "Drop" = la presentacion ocupo 2 o mas periodos objetivo. */
    if (frameTarget > 0.0 && dt > frameTarget * 1.5) paceDrops++;

    if (paceCount < PACE_WINDOW) return;

    /* --- Fin de ventana: volcado de estadisticas --- */

#ifdef FRAMEPACE_LOG
    if ((now - paceLogLast) >= 2000.0) {
        paceLogLast = now;
        const double avg = paceSum / (double)paceCount;
        const double var = (paceSumSq / (double)paceCount) - (avg * avg);
        const double sd  = (var > 0.0) ? sqrt(var) : 0.0;
        pace_trace("[PACE] dt avg=%.3f sd=%.3f min=%.3f max=%.3f target=%.3f\n",
                   avg, sd, paceMin, paceMax, frameTarget);
        pace_trace("[PACE] drops=%d late=%d n=%d sleep=%.2f spin=%.2f\n",
                   paceDrops, paceLate, paceCount,
                   sleepSum / (double)paceCount, spinSum / (double)paceCount);
        pace_trace("[PACE] grid=%d phase=%.3f margin=%.3f\n",
                   gridValid ? 1 : 0, gridPhase, sleepMargin);
        pace_trace("[PACE] flip avg=%.3f max=%.3f blk=%d/%d\n",
                   (flipCount > 0) ? (flipSum / (double)flipCount) : 0.0, flipMax,
                   flipBlockedCount, flipCount);
        pace_trace("[PACE] vsync=%d perm=%d cambios=%d dtAvg=%.2f\n",
                   vsyncOn ? 1 : 0, vsyncPermitido ? 1 : 0, vsyncCambios,
                   dtAvg);
    }
#endif

    /* --- Resumen SIEMPRE, no solo con FRAMEPACE_LOG ---------------------------
     *
     * POR QUE.  Medido con Xenon64/Majora's Mask: en los segundos a 60 VI/s se
     * van 5,7 ms por frame fuera de retro_run y en los de 45 se van 7,8.  Los
     * primeros son SANOS -es el limitador esperando, o sea holgura-; los
     * segundos no, porque con `diff` negativo el limitador no espera.  O sea
     * que `[CORE] fuera` del core NO distingue "el limitador espera" de "el
     * Present bloquea", y sin distinguirlo no se puede saber si queda algo por
     * arreglar aqui o si 45 fps es lo que el core da de si.
     *
     * `flip` lo separa sin ambiguedad: un Present que bloquea al vblank tarda
     * milisegundos y uno que solo encola vuelve en microsegundos (medido: 0,86
     * frente a 11,7).  Y `vs=` dice si el adaptativo estaba actuando en esos
     * segundos, que es la otra mitad de la pregunta.
     *
     * Va por LOG_INFO y no por `pace_trace` porque FRAMEPACE_LOG no esta
     * definido en el vcxproj; una linea cada 2 s no molesta. */
    //if ((now - paceSummaryLast) >= 2000.0) {
    //    paceSummaryLast = now;
    //    LOG_INFO("[PACE] dt=%.2f ema=%.2f target=%.2f vbl/frame=%.4f resbalan=%.2f%% | freno=%c%c%c%c doble=%d (late=%d/%d) | flip avg=%.2f max=%.2f blk=%d/%d | vs=%d camb=%d aud=%.0f | dorm=%.2f giro=%.2f\n",
    //             paceSum / (double)paceCount, dtAvg, frameTarget,
    //             (double)vblSum / (double)paceCount,
    //             (vblSum ? ((double)vblSum / (double)paceCount - 1.0) * 100.0 : 0.0),
    //             /* Quien freno en la ventana.  En regimen debe haber UNA sola
    //              * letra y `doble=0`; dos letras o `doble>0` es el defecto de
    //              * los dos relojes que esta fase existe para erradicar. */
    //             (paceMaskAcum & PACE_VBLANK)  ? 'V' : '-',
    //             (paceMaskAcum & PACE_TIMER)   ? 'T' : '-',
    //             (paceMaskAcum & PACE_AUDIO)   ? 'A' : '-',
    //             (paceMaskAcum & PACE_PRESENT) ? 'P' : '-',
    //             paceDoble,
    //             paceLate, paceCount,
    //             (flipCount > 0) ? (flipSum / (double)flipCount) : 0.0, flipMax,
    //             flipBlockedCount, flipCount,
    //             vsyncOn ? 1 : 0, vsyncCambios, audioOcup,
    //             sleepSum / (double)paceCount, spinSum / (double)paceCount);
    //}

    /* Reiniciamos SOLO los acumuladores de la ventana; el planificador
     * (paceAnchor/paceFrame) no se toca aqui: re-anclarlo cambiaria la fase. */
    reset_pace_stats();
}

/* === note_grid_wake ==========================================================
 *
 * Se llama con cada despertar de SDL_Delay.  Mantiene la fase de la rejilla del
 * timer del scheduler y decide si podemos fiarnos de ella.
 *
 * La comprobacion es de CONGRUENCIA: si los despertares caen de verdad en una
 * rejilla de tickGridMs, la distancia de cada uno al punto de rejilla mas
 * cercano (segun la fase que ya teniamos) es ~0.  Basta eso; no hay que
 * suponer nada sobre el reloj.
 *
 * Los tres mundos posibles quedan cubiertos:
 *   - rejilla de 1 ms (lo esperado con timeBeginPeriod(1)): engancha, y el
 *     limitador puede pedir el sueno alineado.
 *   - sueno exacto sin rejilla: no engancha (los despertares no son
 *     congruentes), pero entonces el exceso del sleep es ~0 y el margen
 *     estadistico converge solo a su suelo.  Igual de barato por otro camino.
 *   - rejilla mas basta (Windows sin resolucion de 1 ms): no engancha y el
 *     margen crece hasta pasar a espera activa pura.  Cuesta CPU, pero el
 *     pacing no se rompe. */
void Sync::note_grid_wake(double actual, double predicted) {
    /* Distancia FIRMADA al punto de rejilla mas cercano, en [-tick/2, tick/2). */
    double off = fmod(actual - gridPhase, tickGridMs);
    if (off < 0.0) off += tickGridMs;
    const double err = (off <= tickGridMs * 0.5) ? off : (off - tickGridMs);

    /* Congruente = el despertar cae en la rejilla.  A tiempo = no se ha pasado
     * del punto que habiamos predicho (solo aplica en modo alineado). */
    const bool congruent = (fabs(err) <= GRID_TOL_MS);
    const bool onTime    = (predicted <= 0.0) || (actual <= predicted + GRID_TOL_MS);

    if (congruent && !onTime) {
        /* Cae en la rejilla pero un tick mas tarde del punto pedido: eso es un
         * error de la PETICION (la latencia de despertar, ver GRID_LAT_MS), no
         * una rejilla que se haya movido.  Ni acierto ni fallo: seguimos la fase
         * y ya esta.  Desengancharse aqui es lo que hacia que en el menu se
         * soltase y volviese a enganchar cada ~14 frames. */
        gridPhase += err * 0.25;
        if (gridPhase < 0.0)         gridPhase += tickGridMs;
        if (gridPhase >= tickGridMs) gridPhase -= tickGridMs;
        return;
    }

    if (congruent) {
        gridMiss = 0;
        if (gridGood < GRID_LOCK_HITS) {
            gridGood++;
            if (gridGood >= GRID_LOCK_HITS && !gridValid) {
                gridValid = true;
#ifdef FRAMEPACE_LOG
                pace_trace("[PACE] rejilla del timer enganchada: tick=%.2f fase=%.3f\n",
                           tickGridMs, gridPhase);
#endif
            }
        }
        /* Seguimiento fino: el reloj de QPC y el tick del scheduler son dominios
         * distintos, asi que la fase deriva despacio y hay que acompanarla. */
        gridPhase += err * 0.25;
        if (gridPhase < 0.0)         gridPhase += tickGridMs;
        if (gridPhase >= tickGridMs) gridPhase -= tickGridMs;
        return;
    }

    gridGood = 0;
    gridMiss++;

    if (gridValid) {
        if (gridMiss < GRID_MISS_MAX) return;   /* un fallo aislado no desengancha */
        gridValid = false;
        /* Volvemos al margen partiendo de un valor conservador: si el primer
         * sleep sin rejilla se pasa, no queremos llegar tarde mientras el
         * ataque rapido lo reajusta. */
        sleepMargin = SLEEP_MARGIN_INIT;
#ifdef FRAMEPACE_LOG
        pace_trace("[PACE] rejilla del timer perdida (err %.3f ms): vuelvo al margen\n", err);
#endif
    }

    /* Re-sembrar la fase con este despertar y volver a contar. */
    double ph = fmod(actual, tickGridMs);
    if (ph < 0.0) ph += tickGridMs;
    gridPhase = ph;
    gridMiss  = 0;
}

/* === update_sleep_margin =====================================================
 *
 * Ajusta el margen que el limitador reserva para la espera activa, a partir del
 * exceso REAL de cada SDL_Delay (`over` = dormido - pedido).
 *
 * Se mantienen dos EMA: la media del exceso y su desviacion absoluta media.  El
 * margen es media + 3 desviaciones + un colchon minimo, de forma que cubra el
 * peor caso razonable del sleep sin reservar de mas.
 *
 * Medido en Salvia: SDL_Delay se pasa ~0.03 ms, asi que el margen converge al
 * suelo (0.25 ms) y la espera activa queda en ~0.7 ms por frame contra los
 * ~2.9 ms del margen fijo anterior.  Y si el reloj fuese malo (Windows sin
 * resolucion de 1 ms: Sleep se puede pasar 7 ms), el margen crece hasta que la
 * peticion de sueno se queda por debajo de 1 ms y el limitador pasa a espera
 * activa pura: cuesta CPU, pero el pacing sigue exacto.  Con el margen fijo,
 * ese caso llegaba tarde a TODOS los frames sin enterarse. */
void Sync::update_sleep_margin(double over) {
    if (over < 0.0) over = 0.0;   /* dormir de menos no gasta margen */

    const double alpha = 0.05;
    sleepOverAvg += (over - sleepOverAvg) * alpha;
    sleepOverDev += (fabs(over - sleepOverAvg) - sleepOverDev) * alpha;

    double margin = sleepOverAvg + 3.0 * sleepOverDev + SLEEP_MARGIN_FLOOR;

    /* Ataque rapido, decaimiento lento: si ESTE sleep se ha pasado mas de lo que
     * el margen cubria, lo subimos ya.  Esperar a que converja la media serian
     * decenas de frames llegando tarde, que es justo lo que hay que evitar en el
     * caso malo (Windows sin resolucion de 1 ms: Sleep se pasa ~7 ms). */
    if (over + SLEEP_MARGIN_FLOOR > margin) margin = over + SLEEP_MARGIN_FLOOR;

    if (margin < SLEEP_MARGIN_FLOOR) margin = SLEEP_MARGIN_FLOOR;
    /* Tope: medio frame.  Mas alla, dormir no aporta nada y es mejor que la
     * peticion caiga por debajo de 1 ms y se resuelva con espera activa. */
    const double cap = (frameTarget > 0.0) ? (frameTarget * 0.5) : 8.0;
    if (margin > cap) margin = cap;
    sleepMargin = margin;
}

/* === limit_fps ===============================================================
 *
 * Planificador de deadlines: el objetivo del frame n es
 *
 *      paceAnchor + n * frameTarget
 *
 * calculado siempre desde el ancla en lugar de acumular sumas.  Asi el ritmo
 * medio es exacto (sin deriva por redondeo) y una espera larga no arrastra su
 * error a los frames siguientes.
 *
 * La espera son dos tramos: SDL_Delay para el grueso (el thread queda en WAIT y
 * no consume CPU) y una espera activa corta para clavar el instante.  Lo que
 * decide el reparto se MIDE, no se supone (medido en Salvia, coste de la espera
 * activa por frame):
 *
 *   2.9 ms  margen fijo de 2 ms + el resto de truncar los ms del sleep
 *   1.5 ms  margen adaptativo (update_sleep_margin): aprende que el despertar
 *           puede irse hasta un tick, asi que reserva un tick
 *   0.6 ms  sueno alineado a la rejilla del timer (note_grid_wake): sabiendo
 *           donde despierta, solo queda el hueco rejilla->deadline
 *
 * El suelo con SDL_Delay son ~0.5 ms: el deadline cae en un punto arbitrario
 * entre dos puntos de la rejilla y ese hueco no se puede dormir.
 *
 * `nextFrameTime` (referencia del caller) se actualiza SIEMPRE, en todos los
 * modos de sync, porque el watcher de cuelgues de salvia.h lo usa de heartbeat:
 * si se queda congelado (como pasaba en SYNC_TO_AUDIO, que salia antes de
 * tocarlo) el watcher da un falso positivo de cuelgue al segundo.
 */
double Sync::limit_fps(double& nextFrameTime, int syncType, GameTicks &gameTicks) {
    const double currentTime = Constant::getTicks();

	gameTicks.ticks++;
	gameTicks.dt = (float)(currentTime - gameTicks.lastTime);
	gameTicks.lastTime = currentTime;

	/* Cambio de modo (menu, override por core al cargar, avance rapido, que
	 * fuerza SYNC_TO_VIDEO y luego restaura): los dos planificadores se
	 * re-anclan, y el present inmediato del "Video adaptativo" solo queda
	 * puesto en ese modo.  Un render state, sin Reset del device. */
	if (syncType != syncTypeAnt) {
		syncTypeAnt = syncType;
		SDL_XBOX_SetAdaptiveVSync(syncType == SYNC_TO_VIDEO_ADAPT ? 1 : 0);
		reset_pace(currentTime);
	}

	if (syncType != SYNC_TO_VIDEO && syncType != SYNC_TO_VIDEO_ADAPT){
		// Sin limitador: mantenemos el planificador pegado al presente para que
		// al volver a SYNC_TO_VIDEO no haya un salto ni una racha de frames sin
		// pacing recuperando una deuda inventada.
		paceAnchor    = currentTime;
		paceFrame     = 0;
		vblAnchor     = 0;        /* re-ancla tambien el pacer de vblanks */
		nextFrameTime = currentTime + frameTarget;
		return currentTime;
	}

	/* ===== PACER POR VBLANKS ================================================
	 *
	 * Cuando la plataforma tiene reloj de presentacion, el objetivo NO es un
	 * instante de reloj de CPU: es el VBLANK en el que toca presentar el frame
	 * `n`.  Con `r = refresco / fpsCore`, eso es `ancla + round(n * r)`.
	 *
	 * Que esto es lo correcto y no una elegancia se ve en el caso PAL, que es el
	 * que mas duele: core a 50 Hz en panel de 60 da `r = 1,2`, y la secuencia
	 * `round(1,2n)` = 0,1,2,4,5,6,7,8,10... produce **exactamente** el patron
	 * 5:6 de 1 y 2 vblanks que se midio en el hardware (`vbl/frame = 1,202`).
	 * O sea que el pacer y la pantalla dejan de ser dos relojes: el pacer
	 * ELIGE el vblank, en vez de esperar un instante y descubrir despues en
	 * cual ha caido.
	 *
	 * Y la espera es BLOQUEANTE (`pc_wait_vblank`), no activa.  El limitador
	 * viejo gastaba 0,5-0,9 ms/frame girando en `YieldProcessor`, y eso es CPU
	 * del mismo hilo que corre el core.
	 *
	 * El camino de reloj de CPU de mas abajo se conserva intacto como respaldo:
	 * es el que corre en Windows, donde `pc_vblanks()` devuelve 0.
	 *
	 * ★ SOLO EN "VIDEO ADAPTATIVO" (SYNC_TO_VIDEO_ADAPT).  Con el umbral de
	 * present inmediato, un core por debajo de 60 (Gambatte ~59,73, Genesis
	 * Plus GX ~59,92) salta un vblank cada pocos cientos de frames y el present
	 * de despues sale rasgado.  "Video" (SYNC_TO_VIDEO) vuelve a ser el de
	 * antes de 9f9f64bc: el camino de reloj de CPU de abajo. */
	if (syncType == SYNC_TO_VIDEO_ADAPT && vblPacerActivo())
	{
		const unsigned int vblAhora = pc_vblanks();
		const double       r        = vblPorFrame();
		unsigned int       objetivo;

		if (vblAnchor == 0) {
			vblAnchor = vblAhora;
			vblFrame  = 0;
		}

		++vblFrame;
		objetivo = vblAnchor + (unsigned int)(vblFrame * r + 0.5);

		/* Si el objetivo ya paso, no se persigue la deuda: se re-ancla.  Es la
		 * misma regla que el camino de reloj -y la que evita el efecto "camara
		 * rapida"-, pero aqui en unidades del hardware y sin ambiguedad: un
		 * frame de retraso es literalmente un vblank de retraso. */
		if ((int)(objetivo - vblAhora) < 0) {
			++paceLate;
			vblAnchor = vblAhora;
			vblFrame  = 0;
			nextFrameTime = currentTime + frameTarget;
			return currentTime;
		}

		/* Tope de seguridad generoso: tres periodos de frame.  Si se agota es
		 * que el reloj de presentacion ha dejado de latir (cambio de modo de
		 * video), y volver sin esperar es mejor que colgar el frontend. */
		{
			const bool espero = ((int)(objetivo - vblAhora) > 0);
			pc_wait_vblank(objetivo, (unsigned int)(frameTarget * 3.0) + 2);
			anota_pace(espero, false, syncType == SYNC_TO_AUDIO);
		}

		/* El planificador de reloj se mantiene pegado al presente mientras manda
		 * el pacer de vblanks.  Si no, al caer a el -cambio de modo de video, o
		 * VBL_PACER a 0- arrancaria con un ancla de hace minutos y una deuda
		 * inventada de miles de frames.  Es la misma razon por la que la salida
		 * temprana de los otros modos de sync tambien lo re-ancla. */
		paceAnchor = currentTime;
		paceFrame  = 0;

		/* Re-ancla cada 4096 frames para acotar la aritmetica: `vblFrame * r`
		 * se convierte a `unsigned`, y con el contador creciendo sin limite el
		 * producto acabaria desbordando.  Se desplaza el ancla los vblanks que
		 * de verdad han pasado, asi que la fase se conserva salvo el redondeo:
		 * menos de un vblank una vez cada ~68 s. */
		if (vblFrame >= 4096) {
			vblAnchor += (unsigned int)(4096.0 * r + 0.5);
			vblFrame  -= 4096;
		}

		nextFrameTime = currentTime + frameTarget;
		return currentTime;
	}

    // Nota: la metrica de utilizacion CPU se calcula en
    // sample_cpu_utilization() via GetThreadTimes, llamada cada frame desde
    // GameMenu::updateFps().  Esto distingue trabajo real del CPU de espera
    // bloqueante (la metrica wall-time anterior no lo hacia) y funciona en
    // TODOS los modos de sync.

    double deadline = paceAnchor + (double)paceFrame * frameTarget;
    double diff     = deadline - currentTime;   // > 0 = vamos adelantados

    /* Antes del re-anclaje de abajo, que pone `diff` a 0 y borraria justo la
     * senal de retraso mas fuerte que hay: "mas de un frame tarde".  El `diff`
     * de aqui ya lleva descontado el Present de este frame, asi que es
     * directamente comparable con lo que cuesta dejarlo bloquear. */
    update_vsync_adaptativo(diff);

    /* Deuda mayor que un frame (hitch de carga, savestate, stream de CD):
     * re-anclamos en vez de correr una racha de frames sin pacing para
     * recuperar el tiempo perdido - eso es el efecto "camara rapida", y con el
     * umbral anterior (-100 ms) podian ser hasta 6 frames seguidos. */
    if (diff < -frameTarget) {
        paceLate++;
        reset_pace(currentTime);
        deadline = currentTime;
        diff     = 0.0;
    } else if (diff < -VSYNC_TARDE_MS) {
        /* 1 ms de tolerancia para que la metrica cuente retrasos de verdad y no
         * el ruido de una plataforma que se pasa unas decimas. */
        paceLate++;
    }

    if (diff > 0.0) {
        /* Tramo dormido.  SDL_Delay solo acepta milisegundos enteros y despierta
         * en un punto de la rejilla del timer, asi que hay dos formas de pedirlo:
         *
         *  - ALINEADO (gridValid): conocemos la fase de la rejilla, asi que
         *    pedimos el sueno que despierta en el ultimo punto ANTES del
         *    deadline.  La espera activa se queda con el hueco rejilla->deadline,
         *    ~0.5 ms de media.  Nota: floor() sirve para elegir la peticion
         *    porque floor(x) esta en (x-1, x] y la rejilla es de 1 ms, asi que el
         *    primer punto de rejilla >= now+floor(x) es exactamente el buscado.
         *
         *  - POR MARGEN: sin rejilla fiable hay que reservar un tick entero
         *    (sleepMargin lo aprende), porque el despertar puede caer en
         *    cualquier punto de [pedido, pedido+tick). */
        double afterSleep = currentTime;
        double predicted  = 0.0;
        uint32_t ms       = 0;

        if (gridValid) {
            const double target = deadline - GRID_EPS_MS;
            double frac = fmod(target - gridPhase, tickGridMs);
            if (frac < 0.0) frac += tickGridMs;
            const double grid    = target - frac;      /* despertar buscado */
            /* La peticion se calcula contra la rejilla de TICKS (grid menos la
             * latencia de despertar): SDL_Delay despierta en el primer tick
             * posterior a now+ms, no en el primer despertar. */
            const double request = grid - GRID_LAT_MS - currentTime;
            if (request >= 1.0) {
                ms        = (uint32_t)request;
                predicted = grid;
            }
            /* Si el hueco no da para un milisegundo se gasta girando.  NO se cae
             * al camino del margen: ese pediria un Sleep(1) que despertaria en el
             * punto de rejilla SIGUIENTE, ya pasado el deadline. */
        } else {
            const double request = diff - sleepMargin;
            if (request >= 1.0) ms = (uint32_t)request;
        }

        if (ms > 0) {
            SDL_Delay(ms);
            afterSleep = Constant::getTicks();
            const double slept = afterSleep - currentTime;
            sleepSum += slept;
            /* El margen solo se entrena cuando es el que manda: en modo alineado
             * el "exceso" es intencionado (pedimos de menos a proposito) y
             * entrenarlo con eso lo dejaria inflado para siempre. */
            if (predicted <= 0.0) update_sleep_margin(slept - (double)ms);
            note_grid_wake(afterSleep, predicted);
        }

        /* ESPERA ACTIVA: clava el instante exacto.  Comparamos el contador
         * crudo contra el deadline ya convertido a ticks para no hacer una
         * division en coma flotante por iteracion del bucle. */
        #if defined(_XBOX) || defined(WIN)
            const long long deadlineTk = (long long)(deadline * Constant::getTickFreqMs());
            while (Constant::getRawTicks() < deadlineTk) {
                #ifdef _XBOX
                    YieldProcessor();
                #else
                    _mm_pause(); // Optimiza el bucle de espera en CPUs x86/x64
                #endif
            }
        #else
            while (Constant::getTicks() < deadline) { }
        #endif

        spinSum += Constant::getTicks() - afterSleep;
    }

    /* Fase 4: por este camino el que frena es el limitador, y solo si de verdad
     * llego a esperar (`diff > 0`).  El Present lo deduce `anota_pace` de la
     * duracion del flip. */
    anota_pace(false, diff > 0.0, syncType == SYNC_TO_AUDIO);

    paceFrame++;

    /* Re-ancla exacta cada 4096 frames: desplazar el ancla un numero entero de
     * periodos no cambia la fase, y evita que paceFrame crezca sin limite. */
    if (paceFrame >= 4096) {
        paceAnchor += (double)paceFrame * frameTarget;
        paceFrame   = 0;
    }

    nextFrameTime = paceAnchor + (double)paceFrame * frameTarget;
	return currentTime;
}
