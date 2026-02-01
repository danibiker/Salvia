#pragma once

#include <vector>
#include <stdint.h>
#include <cstddef>

class AudioBuffer {
private:
    std::vector<int16_t> buffer;
    // volatile evita que el compilador de VS2010 optimice estas variables en registros
    volatile long head; 
    volatile long tail; 
    std::size_t capacity;

public:
	enum { AUDIO_BUFFER_SIZE = 8192 }; 
    AudioBuffer(std::size_t size = AUDIO_BUFFER_SIZE);
    void Write(const int16_t* samples, std::size_t count);
    void WriteBlocking(const int16_t* samples, std::size_t count);
    void Read(int16_t* stream, std::size_t count);
	std::size_t get_free_space() const;
	std::size_t get_readable_samples() const;
};