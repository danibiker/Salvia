#pragma once

#include <const/constant.h>
#include <stdint.h>

//Only to set the number of frames to count on a buffer
#define FPS_AVG_COUNT 20
#define FPS_DESIRED 60
const uint32_t TIME_AVG_COUNT = FPS_AVG_COUNT * 1000; 

static const char *FPS_FORMAT = "FPS: %.1f";
static const char *CPU_FORMAT = "CPU: %.0f%%";

class Sync {
public:
    Sync(int);
    ~Sync(){};

    // Ordenados por tamaño para mejor alineamiento en PowerPC
    double lastWorkEnd;
	float fps;
    float frameDelay;    // 16.6f para 60fps
    float utilization;
    float g_actualFps;
    
    uint32_t g_lastFrameTick;
    uint32_t g_totalTimeSum; // Nueva: suma acumulada de los 10 frames
    int g_frameTimeIndex;
    int g_sync_last;
    
    uint32_t g_frameTimes[FPS_AVG_COUNT];
    
    char fpsText[64]; // Alineado a 16 o 32 bytes (mejor para la cache)
    char cpuText[64];

    void init_fps_counter(float);
    void initAverages(uint32_t);
    void update_fps_counter(bool, uint32_t);
    void limit_fps(double&);
};