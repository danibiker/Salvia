#include <SDL.h>
#include "audiobuffer.h"

#ifdef _XBOX
#include <xtl.h>
#elif defined WIN
#include <windows.h> // Necesario para Interlocked functions
#endif



AudioBuffer::AudioBuffer(std::size_t size) : capacity(size), head(0), tail(0) {
    buffer.resize(size);
}

std::size_t AudioBuffer::get_free_space() const {
    // Leemos de forma segura usando Interlocked para evitar lecturas parciales
    long h = InterlockedExchangeAdd((volatile long*)&head, 0);
    long t = InterlockedExchangeAdd((volatile long*)&tail, 0);
    
    if (h >= t) return capacity - (h - t) - 1;
    return t - h - 1;
}

void AudioBuffer::Write(const int16_t* samples, std::size_t count) {
    long h = head; // Copia local
    std::size_t free = get_free_space();
    if (count > free) count = free; 

    std::size_t space_to_end = capacity - h;
    if (count <= space_to_end) {
        memcpy(&buffer[h], samples, count * sizeof(int16_t));
    } else {
        memcpy(&buffer[h], samples, space_to_end * sizeof(int16_t));
        memcpy(&buffer[0], &samples[space_to_end], (count - space_to_end) * sizeof(int16_t));
    }

	// CRÍTICO PARA XBOX 360 (PowerPC):
    // Obliga a que el memcpy termine antes de mover el puntero 'head'
	#ifdef _XBOX
    MemoryBarrier(); 
	#endif

    // Barrera de memoria implícita en funciones Interlocked de Windows
    // Actualizamos 'head' de forma atómica
    InterlockedExchange((volatile long*)&head, (h + count) % capacity);
}
// LATENCIA DESEADA: 
// Si queremos unos 20ms de latencia, el buffer no debería tener 
// más de ~2000 muestras acumuladas.
const std::size_t target_latency = 2048; 

void AudioBuffer::WriteBlocking(const int16_t* samples, std::size_t count) {
    // Bloqueamos si al añadir 'count' superaríamos el target de latencia
    while (get_readable_samples() + count > target_latency) {
        // En Xbox 360 XDK, Sleep(1) es correcto para ceder tiempo
        Sleep(1); 
    }
    Write(samples, count);
}

void AudioBuffer::Read(int16_t* stream, std::size_t count) {
    long h = InterlockedExchangeAdd((volatile long*)&head, 0);
    long t = tail; // Copia local
    
    std::size_t available = (h >= t) ? (h - t) : (capacity - t + h);
    std::size_t to_read = (count < available) ? count : available;

    if (to_read > 0) {
        std::size_t first_part = (to_read < (capacity - t)) ? to_read : (capacity - t);
        memcpy(stream, &buffer[t], first_part * sizeof(int16_t));

        if (first_part < to_read) {
            std::size_t second_part = to_read - first_part;
            memcpy(stream + first_part, &buffer[0], second_part * sizeof(int16_t));
        }
        
        InterlockedExchange((volatile long*)&tail, (t + to_read) % capacity);
    }

    if (to_read < count) {
        memset(stream + to_read, 0, (count - to_read) * sizeof(int16_t));
    }
}

std::size_t AudioBuffer::get_readable_samples() const {
    std::size_t h = head;
    std::size_t t = tail;

    if (h >= t) {
        return h - t;
    } else {
        return capacity - t + h;
    }
}