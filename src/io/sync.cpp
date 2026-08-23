#include "sync.h"
#include <SDL.h>
#include <math.h>
#include <stdarg.h>
#include <utils/logger.h>

#ifdef _XBOX
#include <xtl.h>
#elif defined(WIN)
#include <windows.h>
#include <xmmintrin.h>
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
	reset_pace(Constant::getTicks());
	reset_pace_stats();
}

/* === reset_pace ==============================================================
 * Re-ancla el planificador de deadlines en `now`.  Deliberadamente NO toca la
 * ventana de estadisticas: un core que no llega a velocidad plena re-ancla
 * cada pocos frames, y si eso limpiase los contadores nunca llegariamos a
 * completar una ventana - justo cuando mas interesa medir. */
void Sync::reset_pace(double now) {
	paceAnchor  = now;
	paceFrame   = 0;
}

/* === reset_pace_stats ========================================================
 * Limpia solo los acumuladores de la ventana de estadisticas.  El estado del
 * margen adaptativo NO se toca: es una propiedad del reloj de la plataforma, no
 * de la ventana ni del core. */
void Sync::reset_pace_stats() {
	flipSum          = 0.0;
	flipMax          = 0.0;
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
        // wall en ms, cpu en us → convertir wall a us.
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

            // Display cap a 100% — valores >100 confunden al usuario
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
    if (durationMs > 1.0) flipBlockedCount++;
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
    }
#endif

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

	if (syncType != SYNC_TO_VIDEO){
		// Sin limitador: mantenemos el planificador pegado al presente para que
		// al volver a SYNC_TO_VIDEO no haya un salto ni una racha de frames sin
		// pacing recuperando una deuda inventada.
		paceAnchor    = currentTime;
		paceFrame     = 0;
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

    /* Deuda mayor que un frame (hitch de carga, savestate, stream de CD):
     * re-anclamos en vez de correr una racha de frames sin pacing para
     * recuperar el tiempo perdido - eso es el efecto "camara rapida", y con el
     * umbral anterior (-100 ms) podian ser hasta 6 frames seguidos. */
    if (diff < -frameTarget) {
        paceLate++;
        reset_pace(currentTime);
        deadline = currentTime;
        diff     = 0.0;
    } else if (diff < -1.0) {
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
