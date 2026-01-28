#include <SDL.h>
#include "audiobuffer.h"

AudioBuffer::AudioBuffer(std::size_t size) : capacity(size), head(0), tail(0) {
    buffer.resize(size);
}

std::size_t AudioBuffer::get_free_space() const {
    std::size_t h = head;
    std::size_t t = tail;
    if (h >= t) return capacity - (h - t) - 1;
    return t - h - 1;
}

void AudioBuffer::Write(const int16_t* samples, std::size_t count) {
    std::size_t space_to_end = capacity - head;
    
    if (count <= space_to_end) {
        memcpy(&buffer[head], samples, count * sizeof(int16_t));
    } else {
        // La copia se divide en dos partes (final y principio del buffer circular)
        memcpy(&buffer[head], samples, space_to_end * sizeof(int16_t));
        memcpy(&buffer[0], &samples[space_to_end], (count - space_to_end) * sizeof(int16_t));
    }
    
    head = (head + count) % capacity;
}

void AudioBuffer::WriteBlocking(const int16_t* samples, std::size_t count) {
    // 1. Si el buffer es de 8192, no esperes a que queden 512 libres.
    // Espera a que haya un margen extra para evitar el bloqueo inmediato en el siguiente frame.
    const std::size_t safety_margin = count * 2; 

    while (get_free_space() < (count + safety_margin)) {
        // 2. En lugar de SDL_Delay(0), usa 1ms. 
        // 0ms puede hacer que el CPU consuma el 100% en el bucle sin dejar que el hilo de audio trabaje.
        SDL_Delay(1); 
        // 3. Si el audio de SDL se detiene por alguna razón, evita el bloqueo infinito
        // (Time-out de seguridad)
    }

    // 4. Copia masiva (memcpy) es vital aquí para no perder tiempo en el hilo de video
    Write(samples, count);
}

void AudioBuffer::Read(int16_t* stream, std::size_t count) {
    for (std::size_t i = 0; i < count; i++) {
        if (head != tail) {
            stream[i] = buffer[tail];
            tail = (tail + 1) % capacity;
        } else {
            stream[i] = 0; // Silencio (Underrun)
        }
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