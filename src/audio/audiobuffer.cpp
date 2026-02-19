#include <SDL.h>
#include "audiobuffer.h"

AudioBuffer::AudioBuffer(std::size_t size) : capacity(size) {
    buffer.resize(size);
	head = 0;
	tail = 0;
}

void AudioBuffer::Write(const int16_t* samples, std::size_t count) {
    // 1. Calculamos el nuevo índice localmente
    std::size_t next_head = head; 

    for (std::size_t i = 0; i < count; i++) {
        buffer[next_head] = samples[i];
        next_head = (next_head + 1) % capacity;
        
        // 2. IMPORTANTE: Evitar que el Write sobrepase al Tail (overflow)
        // Si el buffer se llena en modo no-bloqueante, dejamos de escribir.
        if (next_head == tail) break; 
    }

    // 3. ATÓMICO: Actualizamos el head real de una sola vez.
    // InterlockedExchange asegura que el hilo de SDL vea el nuevo valor inmediatamente.
    InterlockedExchange((long*)&head, (long)next_head);
}

void AudioBuffer::Read(int16_t* stream, std::size_t count) {
    for (std::size_t i = 0; i < count; i++) {
        long h = getHead();
        if (h != tail) {
            stream[i] = buffer[tail];
            tail = (tail + 1) % capacity;
        } else {
            // Si el buffer se vacía (underrun), aplicamos un pequeño fundido 
            // a cero o simplemente silencio para evitar el "pop" electrónico
            stream[i] = 0; 
        }
    }
}

void AudioBuffer::WriteBlocking(const int16_t* samples, std::size_t count) {
    while (true) {
        long h = getHead();
        long t = getTail();
        std::size_t occupied = (h >= t) ? (h - t) : (capacity - (t - h));
        std::size_t free_space = capacity - occupied - 1;

        if (free_space >= count) break;

        // En lugar de 1ms, cedemos el paso muy brevemente
        #if defined(WIN)
            _mm_pause(); 
            Sleep(0); // Cede el turno a otros hilos sin dormir 1ms completo
        #else
            SDL_Delay(0);
        #endif
    }

    for (std::size_t i = 0; i < count; i++) {
        buffer[head] = samples[i];
        head = (head + 1) % capacity;
    }
}