#pragma once

#include <vector>
#include <stdint.h>

class AudioBuffer {
private:
    std::vector<int16_t> buffer;
    std::size_t head; // Donde escribe el Core
    std::size_t tail; // Donde lee SDL
    std::size_t capacity;

public:
    AudioBuffer(std::size_t size = 8192);
    void Write(const int16_t*, std::size_t);
	void WriteBlocking(const int16_t*, std::size_t) ;
    void Read(int16_t*, std::size_t);
};