#pragma once

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
    int16_t     buffer[BUFF_SIZE];  // Array estatico: no heap, no constructor
    volatile long head;
    volatile long tail;

    // Últimas muestras L/R para fade-out suave en underrun
    int16_t last_sample_L;
    int16_t last_sample_R;
    int     fade_pos;

    static const size_t capacity = BUFF_SIZE;

public:
    // Helper para leer de forma segura en VS2010
    long getHead() { return InterlockedExchangeAdd(&head, 0); }
    long getTail() { return InterlockedExchangeAdd(&tail, 0); }

    AudioBuffer()
    {
        memset(buffer, 0, sizeof(buffer));
        head          = 0;
        tail          = 0;
        last_sample_L = 0;
        last_sample_R = 0;
        fade_pos      = 0;
    }

    void Write(const int16_t* samples, size_t count)
    {
        long h = getHead();
        long t = getTail();
        size_t occupied   = (h >= t) ? (h - t) : (capacity - (t - h));
        size_t free_space = capacity - occupied - 1;

        if (count > free_space) count = free_space;
        if (count == 0) return;

        size_t local_head  = (size_t)h;
        size_t first_chunk = capacity - local_head;

        if (first_chunk >= count) {
            memcpy(buffer + local_head, samples, count * sizeof(int16_t));
        } else {
            memcpy(buffer + local_head, samples, first_chunk * sizeof(int16_t));
            memcpy(buffer,              samples + first_chunk, (count - first_chunk) * sizeof(int16_t));
        }

        local_head = (local_head + count) % capacity;
        InterlockedExchange((long*)&head, (long)local_head);
    }

    void WriteBlocking(const int16_t* samples, size_t count)
    {
        while (true)
        {
            long h = getHead();
            long t = getTail();
            size_t occupied   = (h >= t) ? (h - t) : (capacity - (t - h));
            size_t free_space = capacity - occupied - 1;

            if (free_space >= count) break;

#if defined(_XBOX)
            YieldProcessor();
#else
            _mm_pause();
            Sleep(0);
#endif
        }

        size_t local_head  = (size_t)head;
        size_t first_chunk = capacity - local_head;

        if (first_chunk >= count) {
            memcpy(buffer + local_head, samples, count * sizeof(int16_t));
        } else {
            memcpy(buffer + local_head, samples, first_chunk * sizeof(int16_t));
            memcpy(buffer,              samples + first_chunk, (count - first_chunk) * sizeof(int16_t));
        }

        local_head = (local_head + count) % capacity;
        InterlockedExchange((long*)&head, (long)local_head);
    }

    void Read(int16_t* stream, size_t count)
    {
        long        h          = getHead();
        size_t local_tail = (size_t)tail;
        size_t available  = (h >= (long)local_tail)
                                   ? (h - local_tail)
                                   : (capacity - (local_tail - h));

        size_t to_copy = (available < count) ? available : count;

        if (to_copy > 0)
        {
            size_t first_chunk = capacity - local_tail;
            if (first_chunk >= to_copy) {
                memcpy(stream, buffer + local_tail, to_copy * sizeof(int16_t));
            } else {
                memcpy(stream,              buffer + local_tail, first_chunk * sizeof(int16_t));
                memcpy(stream + first_chunk, buffer,            (to_copy - first_chunk) * sizeof(int16_t));
            }

            local_tail = (local_tail + to_copy) % capacity;

            if (to_copy >= 2) {
                last_sample_L = stream[to_copy - 2];
                last_sample_R = stream[to_copy - 1];
            }
            fade_pos = 0;
        }

        // Fade-out suave en underrun
        if (to_copy < count)
        {
            size_t remaining = (count - to_copy) & ~(size_t)1; // forzar par
            int16_t*    dest      = stream + to_copy;

            for (size_t i = 0; i + 1 < remaining; i += 2)
            {
                if (fade_pos < FADE_SAMPLES) {
                    int factor  = FADE_SAMPLES - fade_pos;
                    dest[i]     = (int16_t)((last_sample_L * factor) / FADE_SAMPLES);
                    dest[i + 1] = (int16_t)((last_sample_R * factor) / FADE_SAMPLES);
                    fade_pos++;
                } else {
                    dest[i]     = 0;
                    dest[i + 1] = 0;
                }
            }
        }

        InterlockedExchange((long*)&tail, (long)local_tail);
    }
};
