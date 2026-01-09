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
    // SDL_LockAudio no es necesario si head/tail son atómicos, 
    // pero protege contra condiciones de carrera en arquitecturas antiguas.
    for (std::size_t i = 0; i < count; i++) {
        buffer[head] = samples[i];
        head = (head + 1) % capacity;
    }
}

/*void AudioBuffer::WriteBlocking(const int16_t* samples, std::size_t count) {
    // Espera activa con cesión de tiempo (Yield)
    while (GetFreeSpace() < count) {
        // SDL_Delay(0) cede el resto del quantum de tiempo del hilo sin 
        // forzar una espera de 1ms o más, mucho más preciso que SDL_Delay(1).
        SDL_Delay(0); 
    }
    Write(samples, count);
}*/

void AudioBuffer::WriteBlocking(const int16_t* samples, std::size_t count) {
    const uint32_t MAX_WAIT_MS = 100; // Máximo tiempo de espera (100ms)
    uint32_t startTick = SDL_GetTicks();
    
    // Bucle de espera con seguridad
    while (get_free_space() < count) {
        // Si el audio no ha avanzado en 100ms, algo va mal
        if (SDL_GetTicks() - startTick > MAX_WAIT_MS) {
            // OPCIONAL: Registrar error o resetear buffer
            // head = tail; // Podrías vaciar el buffer para intentar recuperarlo
            return; // Salimos para no congelar el emulador
        }

        SDL_Delay(0); // Cedemos tiempo al sistema
    }

    // Si salimos del bucle a tiempo, escribimos normalmente
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