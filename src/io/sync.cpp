#include "sync.h"
#include <SDL.h>

Sync::Sync(){
	g_frameTimes[FPS_AVG_COUNT];
	g_frameTimeIndex = 0;
	g_lastFrameTick = 0;
	g_actualFps = 0.0f;
	sprintf(fpsText, "%.0f", g_actualFps);
	g_sync = SYNC_NONE;
	g_sync_last = SYNC_NONE;
	fps = FPS_DESIRED;
	frameDelay = 1000 / (double)fps; // Aprox 16ms
}

void Sync::initAverages(uint32_t avg){
	memset(g_frameTimes, avg, sizeof g_frameTimes);
}

void Sync::init_fps_counter(double gameFps){
	if (gameFps > 0){
		frameDelay = 1000.0 / gameFps;
		initAverages((uint32_t)frameDelay);
		fps = gameFps;
	} else {
		initAverages((uint32_t)frameDelay);
	}
}

void Sync::update_fps_counter() {
	uint32_t currentTick = SDL_GetTicks();
    
	// Calculamos cuánto tiempo ha pasado realmente desde el frame anterior
	uint32_t frameTime = currentTick - g_lastFrameTick;
	g_lastFrameTick = currentTick;

	// Guardamos el tiempo de este frame en el buffer circular
	g_frameTimes[g_frameTimeIndex] = frameTime;
	g_frameTimeIndex = (g_frameTimeIndex + 1) % FPS_AVG_COUNT;

	// Sumamos todos los tiempos almacenados
	uint32_t totalTime = 0;
	for (int i = 0; i < FPS_AVG_COUNT; i++) {
		totalTime += g_frameTimes[i];
	}

	// Calculamos la media (evitando división por cero)
	if (totalTime > 0) {
		// FPS = 1000ms / promedio_de_frame_en_ms
		// Es lo mismo que: (1000 * cantidad_de_frames) / tiempo_total
		g_actualFps = (1000.0f * FPS_AVG_COUNT) / totalTime;
		sprintf(fpsText, "%.0f", Constant::round(g_actualFps));
	}
}

void Sync::limit_fps(double& nextFrameTime){
	// 2. LIMITADOR DE ALTA PRECISIÓN (Time-Stretching prevention)
	double currentTime = (double)SDL_GetTicks();
	if (currentTime < nextFrameTime) {
		// Si falta mucho tiempo, soltamos el CPU un poco
		if (nextFrameTime - currentTime > 2.0) {
			SDL_Delay((uint32_t)(nextFrameTime - currentTime - 2.0));
		}

		// ESPERA ACTIVA (Busy Wait) para la precisión final
		// Esto garantiza que salimos del bucle en el microsegundo exacto
		while ((double)SDL_GetTicks() < nextFrameTime) {
			// Espera pura
		}
	} else {
		// Si el frame ha tardado más de la cuenta (caída de FPS), 
		// reseteamos el tiempo objetivo para no intentar "recuperar" frames
		// y evitar que el emulador se acelere de golpe.
		nextFrameTime = (double)SDL_GetTicks();
	}
}