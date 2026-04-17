#pragma once

#include <stdint.h>
#include <string.h> // memcpy, memset

#ifdef _XBOX
#define BUFF_SIZE 16384
#include <xtl.h>
#else
#define BUFF_SIZE 8192
#include <windows.h>
#endif

class AudioBuffer {
private:
    int16_t buffer[BUFF_SIZE];
    volatile long head;
    volatile long tail;
    HANDLE hSpaceEvent;  // Señalizado cuando Read() libera espacio

    static const size_t capacity = BUFF_SIZE;

    size_t used(long h, long t) const {
        return (h >= t) ? (size_t)(h - t) : capacity - (size_t)(t - h);
    }

    size_t copyIn(size_t pos, const int16_t* src, size_t count) {
        size_t first = capacity - pos;
        if (first >= count) {
            memcpy(buffer + pos, src, count * sizeof(int16_t));
        } else {
            memcpy(buffer + pos, src, first * sizeof(int16_t));
            memcpy(buffer, src + first, (count - first) * sizeof(int16_t));
        }
        return (pos + count) % capacity;
    }

    size_t copyOut(size_t pos, int16_t* dst, size_t count) {
        size_t first = capacity - pos;
        if (first >= count) {
            memcpy(dst, buffer + pos, count * sizeof(int16_t));
        } else {
            memcpy(dst, buffer + pos, first * sizeof(int16_t));
            memcpy(dst + first, buffer, (count - first) * sizeof(int16_t));
        }
        return (pos + count) % capacity;
    }

public:
    AudioBuffer() : head(0), tail(0) {
        memset(buffer, 0, sizeof(buffer));
        // Auto-reset: vuelve a no-señalizado tras despertar un hilo
        hSpaceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    ~AudioBuffer() {
        if (hSpaceEvent) CloseHandle(hSpaceEvent);
    }

    // Escritura no bloqueante: descarta si no hay espacio suficiente
    void Write(const int16_t* samples, size_t count) {
        long h = head;
        size_t free_space = capacity - used(h, tail) - 1;

        if (count > free_space) count = free_space;
        if (count == 0) return;

        head = (long)copyIn((size_t)h, samples, count);
    }

    // Escritura bloqueante: duerme el hilo hasta que Read() libere espacio
    void WriteBlocking(const int16_t* samples, size_t count) {
        while (capacity - used(head, tail) - 1 < count) {
            WaitForSingleObject(hSpaceEvent, 2);
        }

        head = (long)copyIn((size_t)head, samples, count);
    }

    // Lectura desde el callback de SDL: rellena con silencio si hay underrun
    void Read(int16_t* stream, size_t count) {
        size_t t = (size_t)tail;
        size_t avail = used(head, (long)t);
        size_t to_copy = (avail < count) ? avail : count;

        if (to_copy > 0) {
            t = copyOut(t, stream, to_copy);
        }

        if (to_copy < count) {
            memset(stream + to_copy, 0, (count - to_copy) * sizeof(int16_t));
        }

        tail = (long)t;

        // Despertar al productor si estaba dormido en WriteBlocking
        SetEvent(hSpaceEvent);
    }

    // Nivel actual de llenado (muestras ocupadas). Thread-safe snapshot.
    size_t getUsed() const { return used(head, tail); }
    size_t getCapacity() const { return capacity; }
};
