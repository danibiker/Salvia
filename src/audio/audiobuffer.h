#pragma once

#include <vector>
#include <stdint.h>
#include <string.h> // memcpy


#ifdef _XBOX
#define BUFF_SIZE 16384
#include <xtl.h>
#else
#define BUFF_SIZE 8192
#include <windows.h>
#endif

#define FADE_SAMPLES 32

class AudioBuffer {
private:
    std::vector<int16_t> buffer;
    volatile long head;
    volatile long tail;
    std::size_t capacity;

    // Últimas muestras L/R para fade-out suave en underrun
    int16_t last_sample_L;
    int16_t last_sample_R;
    int fade_pos; // Posición actual en el fade (0 = no fading, >0 = en progreso)

public:

	// Helper para leer de forma segura en VS2010
    long getHead() { return InterlockedExchangeAdd(&head, 0); }
    long getTail() { return InterlockedExchangeAdd(&tail, 0); }

    AudioBuffer(std::size_t size = BUFF_SIZE);
    void Write(const int16_t*, std::size_t);
	void WriteBlocking(const int16_t*, std::size_t) ;
    void Read(int16_t*, std::size_t);
};