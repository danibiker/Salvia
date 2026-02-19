#pragma once

#include <vector>
#include <stdint.h>

#ifdef WIN
#define BUFF_SIZE 8192
#include <windows.h>
#else
#define BUFF_SIZE 8192
#include <xtl.h>
#endif

class AudioBuffer {
private:
    std::vector<int16_t> buffer;
    volatile long head; 
    volatile long tail;
    std::size_t capacity;

public:

	// Helper para leer de forma segura en VS2010
    long getHead() { return InterlockedExchangeAdd(&head, 0); }
    long getTail() { return InterlockedExchangeAdd(&tail, 0); }

    AudioBuffer(std::size_t size = BUFF_SIZE);
    void Write(const int16_t*, std::size_t);
	void WriteBlocking(const int16_t*, std::size_t) ;
    void Read(int16_t*, std::size_t);
};