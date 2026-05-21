#include "sync.h"
#include <SDL.h>

#ifdef _XBOX
#include <xtl.h>
#elif defined(WIN)
#include <windows.h>
#include <xmmintrin.h>
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
        // Mantenemos el frameDelay como double para el limitador de alta precisión
        this->frameDelay = 1000.0f / gameFps; 
        
        // Para los promedios (que parecen usar enteros), usamos el redondeo mas cercano
        initAverages((uint32_t)(frameDelay + 0.5));
    } else {
        // Fallback por si gameFps es invalido
        initAverages((uint32_t)frameDelay);
    }
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

void Sync::limit_fps(double& nextFrameTime) {
    double currentTime = Constant::getTicks();
    float diffTime = (float)(nextFrameTime - currentTime);

    // Nota: la metrica de utilizacion CPU se calcula ahora en
    // sample_cpu_utilization() via GetThreadTimes, llamada cada frame
    // desde GameMenu::updateFps().  Esto distingue trabajo real del
    // CPU de espera bloqueante (la metrica wall-time anterior no lo
    // hacia) y funciona en TODOS los modos de sync, no solo
    // SYNC_TO_VIDEO (limit_fps solo se llama con SYNC_TO_VIDEO).

    if (diffTime > 0) {
        // Dormir el hilo si sobra tiempo suficiente (ahorro de CPU)
        // SDL_Delay es seguro aqu� porque el Busy Wait corregir� su imprecisi�n
        if (diffTime > 4.0) {
            SDL_Delay((uint32_t)(diffTime - 2.0));
        }
        // ESPERA ACTIVA: Clava el microsegundo exacto
        while (Constant::getTicks() < nextFrameTime) {
			#ifdef _XBOX
				YieldProcessor();
			#elif defined(WIN)
				_mm_pause(); // Optimiza el bucle de espera en CPUs x86/x64
			#endif
        }
    } else if (diffTime < -100.0) {
        // Si hay un lag masivo, reseteamos para evitar el efecto "camara rapida"
        nextFrameTime = currentTime;
    }

    // El siguiente frame se calcula sobre el objetivo ideal
    nextFrameTime += frameDelay;
}
