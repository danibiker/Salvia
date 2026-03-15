#include <SDL.h>
#include "audiobuffer.h"

AudioBuffer::AudioBuffer(std::size_t size) : capacity(size) {
    buffer.resize(size);
	head = 0;
	tail = 0;
	last_sample_L = 0;
	last_sample_R = 0;
	fade_pos = 0;
}

void AudioBuffer::Write(const int16_t* samples, std::size_t count) {
    long h = getHead();
    long t = getTail();
    std::size_t occupied = (h >= t) ? (h - t) : (capacity - (t - h));
    std::size_t free_space = capacity - occupied - 1;

    // Truncar si no cabe (modo no-bloqueante)
    if (count > free_space) count = free_space;
    if (count == 0) return;

    // Escribir en bloque con memcpy (evita % por cada muestra)
    std::size_t local_head = (std::size_t)h;
    std::size_t first_chunk = capacity - local_head;

    if (first_chunk >= count) {
        memcpy(&buffer[local_head], samples, count * sizeof(int16_t));
    } else {
        memcpy(&buffer[local_head], samples, first_chunk * sizeof(int16_t));
        memcpy(&buffer[0], samples + first_chunk, (count - first_chunk) * sizeof(int16_t));
    }

    local_head = (local_head + count) % capacity;

    // Publicamos el head atómicamente para que SDL lo vea de una sola vez
    InterlockedExchange((long*)&head, (long)local_head);
}

void AudioBuffer::Read(int16_t* stream, std::size_t count) {
    // Snapshot atómico de head una sola vez (no por cada muestra)
    long h = getHead();
    std::size_t local_tail = (std::size_t)tail;
    std::size_t available = (h >= (long)local_tail)
        ? (h - local_tail)
        : (capacity - (local_tail - h));

    // Copiar en bloque las muestras disponibles con memcpy
    std::size_t to_copy = (available < count) ? available : count;

    if (to_copy > 0) {
        std::size_t first_chunk = capacity - local_tail;
        if (first_chunk >= to_copy) {
            // No hay wrap-around, una sola copia
            memcpy(stream, &buffer[local_tail], to_copy * sizeof(int16_t));
        } else {
            // Wrap-around: copiar en dos bloques
            memcpy(stream, &buffer[local_tail], first_chunk * sizeof(int16_t));
            memcpy(stream + first_chunk, &buffer[0], (to_copy - first_chunk) * sizeof(int16_t));
        }

        local_tail = (local_tail + to_copy) % capacity;

        // Guardar las últimas muestras para posible fade-out (estéreo: L, R)
        if (to_copy >= 2) {
            last_sample_L = stream[to_copy - 2];
            last_sample_R = stream[to_copy - 1];
        }
        fade_pos = 0; // Reset fade: tenemos datos reales
    }

    // Si hay underrun, hacer fade-out suave en lugar de cortar a silencio
    if (to_copy < count) {
        std::size_t remaining = count - to_copy;
        // Asegurar que remaining es par (estéreo: siempre pares de muestras)
        remaining &= ~(std::size_t)1;
        int16_t* dest = stream + to_copy;

        for (std::size_t i = 0; i + 1 < remaining; i += 2) {
            if (fade_pos < FADE_SAMPLES) {
                // Rampa lineal de la última muestra real a cero
                int factor = FADE_SAMPLES - fade_pos;
                dest[i]     = (int16_t)((last_sample_L * factor) / FADE_SAMPLES);
                dest[i + 1] = (int16_t)((last_sample_R * factor) / FADE_SAMPLES);
                fade_pos++;
            } else {
                dest[i]     = 0;
                dest[i + 1] = 0;
            }
        }
    }

    // Publicar tail atómicamente para que el hilo escritor lo vea
    InterlockedExchange((long*)&tail, (long)local_tail);
}

void AudioBuffer::WriteBlocking(const int16_t* samples, std::size_t count) {
    while (true) {
        long h = getHead();
        long t = getTail();
        std::size_t occupied = (h >= t) ? (h - t) : (capacity - (t - h));
        std::size_t free_space = capacity - occupied - 1;

        if (free_space >= count) break;

        // Cedemos el paso brevemente
        #if defined(_XBOX)
            // PPC yield hint: mas ligero que SDL_Delay, ~10ns vs ~1ms
            YieldProcessor();
        #elif defined(WIN)
            _mm_pause();
            Sleep(0);
        #else
            SDL_Delay(0);
        #endif
    }

    // Escribir en bloque con memcpy
    std::size_t local_head = (std::size_t)head;
    std::size_t first_chunk = capacity - local_head;

    if (first_chunk >= count) {
        memcpy(&buffer[local_head], samples, count * sizeof(int16_t));
    } else {
        memcpy(&buffer[local_head], samples, first_chunk * sizeof(int16_t));
        memcpy(&buffer[0], samples + first_chunk, (count - first_chunk) * sizeof(int16_t));
    }

    local_head = (local_head + count) % capacity;

    // Publicar head atómicamente
    InterlockedExchange((long*)&head, (long)local_head);
}