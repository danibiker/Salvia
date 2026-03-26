#include "sync.h"
#include <SDL.h>

#ifdef _XBOX
#include <xtl.h>
#elif defined(WIN)
#include <xmmintrin.h>
#endif

Sync::Sync(int syncMode){
	g_frameTimeIndex = 0;
	g_lastFrameTick = 0;
	g_actualFps = FPS_DESIRED;
	fps = FPS_DESIRED;
	utilization = 0.0;
	lastWorkEnd = 0.0;
	memset(fpsText, '\0', 50 * sizeof(char));
	sprintf(fpsText, FPS_FORMAT, g_actualFps);
	sprintf(cpuText, CPU_FORMAT, utilization);
	g_sync_last = syncMode;
	frameDelay = 1000.0 / (double)fps; // Aprox 16ms
}

void Sync::initAverages(uint32_t avg){
	//memset(g_frameTimes, avg, sizeof g_frameTimes);
	for(int i = 0; i < FPS_AVG_COUNT; i++) {
        g_frameTimes[i] = avg;
    }
}

void Sync::init_fps_counter(double gameFps){
	if (gameFps > 0){
        this->fps = gameFps;
        // Mantenemos el frameDelay como double para el limitador de alta precisiÃ³n
        this->frameDelay = 1000.0 / gameFps; 
        
        // Para los promedios (que parecen usar enteros), usamos el redondeo mas cercano
        initAverages((uint32_t)(frameDelay + 0.5));
    } else {
        // Fallback por si gameFps es invalido
        initAverages((uint32_t)frameDelay);
    }
}

void Sync::update_fps_counter(bool updateFpsOverlay) {
	uint32_t currentTick = SDL_GetTicks();
    if (g_lastFrameTick == 0) g_lastFrameTick = currentTick; // Inicializacion en el primer uso
    
	// Calculamos cuanto tiempo ha pasado realmente desde el frame anterior
	uint32_t frameTime = currentTick - g_lastFrameTick;
	g_lastFrameTick = currentTick;

	// Guardamos el tiempo de este frame en el buffer circular
	g_frameTimes[g_frameTimeIndex] = frameTime;
	g_frameTimeIndex = (g_frameTimeIndex + 1) % FPS_AVG_COUNT;
	
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
			g_actualFps = (1000.0f * (double)FPS_AVG_COUNT) / (double)totalTime;
			_snprintf(fpsText, sizeof(fpsText), FPS_FORMAT, g_actualFps);
			_snprintf(cpuText, sizeof(cpuText), CPU_FORMAT, utilization);
			
		}
	}
}

void Sync::limit_fps(double& nextFrameTime) {
    double currentTime = Constant::getTicks();
    double diffTime = nextFrameTime - currentTime;

    // --- Métricas de utilización basadas en tiempo real de trabajo ---
    // workTime = tiempo que la CPU estuvo ocupada procesando el frame
    //          = desde que terminó el sleep del frame anterior hasta ahora
    if (lastWorkEnd > 0.0) {
        double workTime = currentTime - lastWorkEnd;
        double currentUtilization = (workTime / this->frameDelay) * 100.0;

        // Clamp: 0% = idle total, 100% = justo al límite, >100% = no llega a tiempo
        if (currentUtilization < 0.0) currentUtilization = 0.0;
        if (currentUtilization > 200.0) currentUtilization = 200.0;

        // Suavizado EMA (alpha=0.1 para ~10 frames de convergencia)
        //this->utilization = (this->utilization * 0.9) + (currentUtilization * 0.1);
		this->utilization = currentUtilization;
    }

    if (diffTime > 0) {
        // Dormir el hilo si sobra tiempo suficiente (ahorro de CPU)
        // SDL_Delay es seguro aquí porque el Busy Wait corregirá su imprecisión
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

    // Registramos el instante en que terminamos de esperar (= inicio del trabajo del proximo frame)
    lastWorkEnd = Constant::getTicks();

    // El siguiente frame se calcula sobre el objetivo ideal
    nextFrameTime += frameDelay;
}