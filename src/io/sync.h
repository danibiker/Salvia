#pragma once

#include <const/constant.h>
#include <stdint.h>

//Only to set the number of frames to count on a buffer
#define FPS_AVG_COUNT 120
#define FPS_DESIRED 60

static const char *FPS_FORMAT = "FPS: %.1f";
static const char *CPU_FORMAT = "CPU: %.0f%%";

class Sync {
	public:
		Sync(int);
		~Sync(){};
		char fpsText[50];
		char cpuText[50];
		double fps;
		double frameDelay; // Aprox 16ms
		int g_sync_last;
		double utilization;

		void init_fps_counter(double);
		void initAverages(uint32_t);
		void update_fps_counter(bool);
		void limit_fps(double&);
	private:
		//This are only for the fps counter stuff used in update_fps_counter and initAverages
		int g_frameTimeIndex;
		uint32_t g_lastFrameTick;
		double g_actualFps;
		uint32_t g_frameTimes[FPS_AVG_COUNT];
		double lastWorkEnd; // Instante en que limit_fps terminó de esperar (fin del sleep/busy-wait)
};