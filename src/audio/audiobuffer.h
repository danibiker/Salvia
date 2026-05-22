#pragma once

#include <stdint.h>
#include <string.h> // memcpy, memset

#ifdef _XBOX
#define BUFF_SIZE 16384
#include <xtl.h>
#else
#define BUFF_SIZE 16384
#include <windows.h>
#endif

class AudioBuffer {
private:
    int16_t buffer[BUFF_SIZE];
    volatile long head;
    volatile long tail;
    HANDLE hSpaceEvent;  // Señalizado cuando Read() libera espacio

    // Contadores de telemetria.  volatile LONG permite incremento via
    // InterlockedExchangeAdd desde el thread productor (libretro audio
    // callback) y lectura desde el thread principal sin race condition.
    // dropsTotal: total muestras descartadas por overflow del buffer.
    //             Incrementa cada vez que Write() no-bloqueante no tiene
    //             espacio suficiente.  Cada drop = discontinuidad =
    //             potencial click audible.  Si crece consistentemente,
    //             indica que el DRC no esta drenando el buffer a tiempo
    //             (kp insuficiente, productor mas rapido que consumidor,
    //             etc.).
    // underrunsTotal: total veces que Read() tuvo que rellenar con
    //             silencio porque el buffer no tenia bastantes muestras.
    //             Cada underrun = silencio momentaneo + posible click al
    //             reanudarse.  Indica el productor mas lento que el
    //             consumidor (CPU stall, CD slow read, etc.).
    volatile long dropsTotal;
    volatile long underrunsTotal;
    // Ultima muestra entregada al output, usada para fade-out suave
    // hacia silencio cuando hay underrun.  Sin fade, una transicion
    // brusca de N (ej +20000) a 0 es una funcion-step audible.
    int16_t lastOutL;
    int16_t lastOutR;

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
    AudioBuffer() : head(0), tail(0), dropsTotal(0), underrunsTotal(0),
                    lastOutL(0), lastOutR(0) {
        memset(buffer, 0, sizeof(buffer));
        // Auto-reset: vuelve a no-señalizado tras despertar un hilo
        hSpaceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    ~AudioBuffer() {
        if (hSpaceEvent) CloseHandle(hSpaceEvent);
    }

    // Escritura no bloqueante: descarta si no hay espacio suficiente.
    // Devuelve el numero de muestras escritas realmente (puede ser < count
    // si hubo overflow).  Las muestras descartadas se contabilizan en
    // dropsTotal.
    size_t Write(const int16_t* samples, size_t count) {
        long h = head;
        size_t free_space = capacity - used(h, tail) - 1;
        size_t to_write = (count > free_space) ? free_space : count;

        if (to_write < count) {
            InterlockedExchangeAdd(&dropsTotal, (LONG)(count - to_write));
        }
        if (to_write == 0) return 0;

        head = (long)copyIn((size_t)h, samples, to_write);
        return to_write;
    }

    // Escritura bloqueante: duerme el hilo hasta que Read() libere espacio.
    //
    // Robusta a count > capacity-1: el ring solo puede tener capacity-1
    // muestras libres como maximo (1 slot sentinel para distinguir
    // empty/full), asi que una espera por count > capacity-1 seria un
    // deadlock matematico (la condicion nunca se cumple).  Para soportar
    // bursts grandes (p.ej. la primera rafaga de audio tras boot del PSX,
    // que puede acumular ~93 ms = ~8200 int16_t antes de devolver control)
    // troceamos el write en chunks de hasta capacity-1 y esperamos espacio
    // entre chunks.  El consumer (SDL callback) drena progresivamente,
    // SetEvent(hSpaceEvent) en cada Read despierta esta espera.
    void WriteBlocking(const int16_t* samples, size_t count) {
        const size_t max_chunk = capacity - 1;

        while (count > 0) {
            size_t want = (count > max_chunk) ? max_chunk : count;

            // Esperar a que haya hueco para 'want' muestras.  Como want
            // <= capacity-1, esta condicion siempre puede cumplirse en
            // algun momento (no deadlock).
            while (capacity - used(head, tail) - 1 < want) {
                WaitForSingleObject(hSpaceEvent, 2);
            }

            head = (long)copyIn((size_t)head, samples, want);
            samples += want;
            count   -= want;
        }
    }

    // Lectura desde el callback de SDL: rellena con silencio si hay underrun,
    // pero con fade-out suave desde la ultima muestra entregada para evitar
    // el click audible que produce una transicion-step de N a 0.
    //
    // `count` es el numero de int16_t totales (stereo intercalado), no frames.
    // Debe ser par para que la separacion L/R cuadre.  Si llega impar (no deberia,
    // SDL siempre pide multiplos de 2), el ultimo elemento se trata como L.
    void Read(int16_t* stream, size_t count) {
        size_t t = (size_t)tail;
        size_t avail = used(head, (long)t);
        size_t to_copy = (avail < count) ? avail : count;

        if (to_copy > 0) {
            t = copyOut(t, stream, to_copy);

            // Memorizar la ultima muestra entregada como punto de partida
            // para el fade-out de un futuro underrun.  Si to_copy es impar
            // (no deberia pasar) la R se queda en su valor anterior.
            if (to_copy >= 2) {
                lastOutL = stream[to_copy - 2];
                lastOutR = stream[to_copy - 1];
            } else if (to_copy == 1) {
                lastOutL = stream[0];
            }
        }

        if (to_copy < count) {
            // Underrun.  En lugar de un memset(0) brusco que produce step
            // function audible cuando la ultima muestra era != 0, generamos
            // un fade-out lineal de las primeras FADE_SAMPLES muestras desde
            // (lastOutL, lastOutR) hacia (0, 0), y luego silencio puro.
            //
            // FADE_SAMPLES = 64 frames stereo = 128 muestras = ~1.5 ms a 44.1 kHz.
            // Suficientemente largo para ser inaudible como fade pero corto
            // como para no enmascarar samples reales que lleguen mientras
            // todavia procesamos el underrun.
            const size_t FADE_FRAMES   = 64;
            const size_t FADE_SAMPLES  = FADE_FRAMES * 2;
            size_t missing = count - to_copy;
            int16_t *missingStart = stream + to_copy;

            // Numero de muestras a fade-fillear (puede ser menor que
            // FADE_SAMPLES si el underrun es muy pequeno).
            size_t fadeNow = (missing < FADE_SAMPLES) ? missing : FADE_SAMPLES;

            // Fade-out frame por frame.  Cada frame contiene L y R.
            for (size_t f = 0; f < fadeNow / 2; f++) {
                // Coeficiente que va de 1.0 a 0.0 linealmente
                int num = (int)(FADE_FRAMES - 1 - f);
                int den = (int)FADE_FRAMES;
                missingStart[f * 2]     = (int16_t)((int)lastOutL * num / den);
                missingStart[f * 2 + 1] = (int16_t)((int)lastOutR * num / den);
            }

            // Resto del missing: silencio puro.
            if (missing > fadeNow) {
                memset(missingStart + fadeNow, 0,
                       (missing - fadeNow) * sizeof(int16_t));
            }

            // Tras un underrun la "ultima muestra" efectiva queda a 0,
            // para que el proximo Read recobre desde silencio limpio
            // (sin doble fade en cascada).
            lastOutL = 0;
            lastOutR = 0;

            InterlockedIncrement(&underrunsTotal);
        }

        tail = (long)t;

        // Despertar al productor si estaba dormido en WriteBlocking
        SetEvent(hSpaceEvent);
    }

    // === Telemetria ===
    // Snapshot de contadores acumulados.  No reseteables (siempre suman
    // desde el ultimo Clear() o construccion).  Util para correlacionar
    // con eventos del CD y diagnosticar fuente de crujidos:
    //   - dropsTotal subiendo: productor mas rapido que consumidor,
    //     DRC no esta drenando lo suficiente (kp bajo o saturado).
    //   - underrunsTotal subiendo: productor mas lento, hay stalls en
    //     el emulador (CD slow read, GPU bloqueando, etc.).
    long getDropsTotal()     const { return dropsTotal; }
    long getUnderrunsTotal() const { return underrunsTotal; }

    // Nivel actual de llenado (muestras ocupadas). Thread-safe snapshot.
    size_t getUsed() const { return used(head, tail); }
    size_t getCapacity() const { return capacity; }

    // Vacia el buffer.  Llamado entre cargas de juego para que el
    // siguiente juego no escuche residuos de audio del anterior.
    // Importante: solo seguro mientras el callback de SDL este pausado
    // (SDL_PauseAudio(1)) — sino tail se modifica concurrentemente.
    //
    // Tambien resetea los contadores de telemetria y el estado del
    // fade-out, para que el nuevo juego empiece con metricas limpias y
    // sin arrastrar la ultima muestra del juego anterior (que sonaria
    // como un mini-click al primer underrun del nuevo juego).
    void Clear() {
        head = 0;
        tail = 0;
        dropsTotal = 0;
        underrunsTotal = 0;
        lastOutL = 0;
        lastOutR = 0;
        memset(buffer, 0, sizeof(buffer));
    }
};
