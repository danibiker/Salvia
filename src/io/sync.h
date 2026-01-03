#pragma once

#include "..\const\constant.h"
#include <stdint.h>

enum {
	SYNC_TO_AUDIO = 0,
	SYNC_TO_VIDEO,
	SYNC_NONE
};

//Only to set the number of frames to count on a buffer
#define FPS_AVG_COUNT 60
#define FPS_DESIRED 60

class Sync {
	public:
		Sync();
		~Sync(){};
		char fpsText[50];
		double fps;
		double frameDelay; // Aprox 16ms
		int g_sync;
		int g_sync_last;

		void init_fps_counter(double);
		void initAverages(uint32_t);
		void update_fps_counter();
		void limit_fps(double&);
	private:
		//This are only for the fps counter stuff used in update_fps_counter and initAverages
		int g_frameTimeIndex;
		uint32_t g_lastFrameTick;
		float g_actualFps;
		uint32_t g_frameTimes[FPS_AVG_COUNT];
};