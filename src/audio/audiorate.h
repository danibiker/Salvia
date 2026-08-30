#pragma once

#include <stdint.h>
#include <string.h>
#include <audio/audiobuffer.h>

// Maximo de frames estereo que puede producir el resampler por pasada.
// 2048 frames * 2 canales * 2 bytes = 8KB en stack.  Los lotes que darian mas
// salida que esto se procesan por trozos (ver processAndWrite).
#define DRC_MAX_FRAMES 2048

// Punto fijo del resampler: 16 bits de parte fraccionaria.
#define RATE_FRAC_BITS 16
#define RATE_ONE       (1u << RATE_FRAC_BITS)

// Maximo de frames de ENTRADA por pasada del resampler (ver processAndWrite).
#define RATE_MAX_CHUNK_IN 8192

/* ==========================================================================
 * Control de tasa + remuestreo de la tasa del CORE a la del DISPOSITIVO.
 *
 * El dispositivo de audio se abre UNA sola vez al arrancar, a una tasa fija
 * (48000 por defecto), y no se vuelve a abrir: en Xbox 360 el ciclo
 * SDL_OpenAudio/SDL_CloseAudio repetido cuelga libSDLx360.  Asi que es aqui
 * donde se adapta la tasa de cada core a la del dispositivo.
 *
 * Dos factores se multiplican en un unico ratio:
 *
 *   baseRatio = tasaCore / tasaDispositivo   (fijo mientras dure el juego)
 *   1 + adj                                  (DRC: corrige el drift entre el
 *                                             reloj del core y el del device)
 *
 * El bucle por muestra va en punto fijo 16.16 y sin ningun double: en el PPC
 * del 360 la conversion double->int es un round-trip por memoria, y aqui se
 * ejecuta 48.000 veces por segundo.  Los doubles se quedan donde no duelen,
 * una vez por lote, al recalcular el paso.
 * ========================================================================== */
class AudioRateControl {
private:
    // Estado del controlador
    double smoothedFill;    // nivel de llenado filtrado (0.0 a 1.0)
    double baseRatio;       // tasaCore / tasaDispositivo
    double ratio;           // baseRatio * (1 + adj) = el ratio efectivo
    int    warmup;          // lotes restantes de warmup (sin DRC)

    // Estado del resampler
    uint32_t stepFx;        // `ratio` en 16.16: frames de entrada por frame de salida
    uint32_t posFx;         // posicion fraccionaria, PERSISTENTE entre lotes
    int16_t  prevL;         // ultimo frame del lote anterior: es la muestra
    int16_t  prevR;         // izquierda de la primera interpolacion del lote actual
    bool     hasPrev;

    // Configuracion
    size_t bufCapacity;

    // Constantes de tuning
    static const int WARMUP_FRAMES = 30;

    void recalcStep() {
        double r = ratio;
        if (r < 0.05) r = 0.05;           /* topes de cordura: un ratio absurdo */
        if (r > 20.0) r = 20.0;           /* solo podria venir de una tasa mala */
        stepFx = (uint32_t)(r * (double)RATE_ONE + 0.5);
        if (stepFx == 0) stepFx = 1;
    }

    // --- Controlador proporcional con EMA ---
    void updateRatio(size_t currentUsed) {
        double fill = (double)currentUsed / (double)bufCapacity;

        const double alpha = 0.1;
        smoothedFill = smoothedFill * (1.0 - alpha) + fill * alpha;

        double error = smoothedFill - 0.5;

        double adj = error * 0.04;

        if (adj >  0.02) adj =  0.02;
        if (adj < -0.02) adj = -0.02;

        /* El DRC ya no fija el ratio, lo MODULA: el grueso lo pone la relacion
         * entre la tasa del core y la del dispositivo. */
        ratio = baseRatio * (1.0 + adj);
        recalcStep();

#ifdef AUDIO_LOG
        // Trace periodico cada 2s: DRC state
        {
            static DWORD lastDrct = 0;
            DWORD now = GetTickCount();
            if (now - lastDrct >= 2000) {
                lastDrct = now;
                LOG_DEBUG("[DRC] base=%.4f ratio=%.4f fill_raw=%.1f%% fill_smooth=%.1f%% err=%.4f adj=%.4f",
                    baseRatio, ratio, fill * 100.0, smoothedFill * 100.0, error, adj);
            }
        }
#endif
    }

    /* --- Resampler lineal estereo, punto fijo ---
     *
     * Interpola sobre un flujo virtual en el que la muestra -1 es el ultimo
     * frame del lote ANTERIOR (prevL/prevR).  Eso, junto con posFx persistente,
     * es lo que da continuidad en las fronteras de lote: sin ello cada llamada
     * reempezaria en pos 0 e interpolaria el ultimo frame contra si mismo, lo
     * que con ratio 1.0 es inaudible pero con 44100->48000 son clicks.
     *
     * Devuelve frames de SALIDA escritos y, en *consumedIn, frames de ENTRADA
     * consumidos.  Puede consumir menos de srcFrames si se llena dst; el
     * llamante continua con el resto y el estado queda coherente.
     */
    size_t resampleChunk(const int16_t* src, size_t srcFrames,
                         int16_t* dst, size_t dstMaxFrames, size_t* consumedIn) {
        size_t out = 0;

        /* Primer lote de la sesion: sin muestra anterior real, arrancar desde
         * src[0] en vez de desde el 0 con el que nacen prevL/prevR, que meteria
         * una rampa desde silencio en el primer frame. */
        if (!hasPrev && srcFrames > 0) {
            prevL   = src[0];
            prevR   = src[1];
            hasPrev = true;
        }

        while (out < dstMaxFrames) {
            const uint32_t whole = posFx >> RATE_FRAC_BITS;
            if (whole >= srcFrames) break;      /* falta la muestra derecha */

            const int32_t  idx  = (int32_t)whole - 1;   /* -1 = frame del lote anterior */
            const int32_t  l1   = src[whole * 2];
            const int32_t  r1   = src[whole * 2 + 1];
            int32_t        l0, r0;

            if (idx < 0) {
                l0 = prevL;
                r0 = prevR;
            } else {
                l0 = src[idx * 2];
                r0 = src[idx * 2 + 1];
            }

            /* Interpolacion lineal en enteros de 32 bits, sin 64 bits ni float.
             *
             * f se toma con 15 bits (no 16) por el margen: |l1-l0| <= 65535 y
             * f <= 32767, luego |d*f| <= 2.147.385.345, que cabe en int32 con
             * 98.302 de holgura.  Con 16 bits desbordaria.
             *
             * No hace falta clamp: el resultado queda SIEMPRE entre l0 y l1
             * (ambos int16). Para d>0, floor(d*f/32768) esta en [0, d); para
             * d<0, como d es entero y d*f/32768 > d, el floor no baja de d.
             * Por eso aqui no hay las cuatro comparaciones por muestra que
             * llevaba la version en double, que si las necesitaba. */
            const int32_t f = (int32_t)((posFx & (RATE_ONE - 1)) >> 1);

            dst[out * 2]     = (int16_t)(l0 + (((l1 - l0) * f) >> 15));
            dst[out * 2 + 1] = (int16_t)(r0 + (((r1 - r0) * f) >> 15));

            out++;
            posFx += stepFx;
        }

        /* Frames de entrada que ya no hacen falta.  Si salimos por dstMaxFrames,
         * `whole` va por detras de srcFrames y el resto se procesa en la
         * siguiente vuelta del llamante. */
        size_t consumed = posFx >> RATE_FRAC_BITS;
        if (consumed > srcFrames) consumed = srcFrames;

        if (consumed > 0) {
            prevL   = src[(consumed - 1) * 2];
            prevR   = src[(consumed - 1) * 2 + 1];
            hasPrev = true;
            posFx  -= (uint32_t)(consumed << RATE_FRAC_BITS);
        }

        if (consumedIn) *consumedIn = consumed;
        return out;
    }

public:
    AudioRateControl() {
        smoothedFill = 0.5;
        baseRatio = 1.0;
        ratio = 1.0;
        warmup = 0;
        stepFx = RATE_ONE;
        posFx = 0;
        prevL = 0;
        prevR = 0;
        hasPrev = false;
        bufCapacity = 8192;
    }

    void init(size_t capacity) {
        bufCapacity = capacity;
        smoothedFill = 0.5;
        ratio = baseRatio;          /* sin ajuste de DRC todavia */
        warmup = WARMUP_FRAMES;
        posFx = 0;
        prevL = 0;
        prevR = 0;
        hasPrev = false;
        recalcStep();
    }

    void reset() {
        smoothedFill = 0.5;
        ratio = baseRatio;
        warmup = 0;
        posFx = 0;
        prevL = 0;
        prevR = 0;
        hasPrev = false;
        recalcStep();
    }

    /* Tasas de origen (core) y destino (dispositivo abierto).  Se llama al
     * cargar un juego y en RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO; NO implica
     * reabrir nada, que es justo el objetivo. */
    void setRates(double srcRate, double dstRate) {
        if (srcRate <= 0.0 || dstRate <= 0.0) {
            baseRatio = 1.0;
        } else {
            baseRatio = srcRate / dstRate;
        }
        ratio = baseRatio;
        posFx = 0;
        prevL = 0;
        prevR = 0;
        hasPrev = false;
        recalcStep();
    }

    double getBaseRatio() const { return baseRatio; }

    // Punto de entrada principal.
    // Calcula el ratio, remuestrea y escribe al buffer.
    size_t processAndWrite(AudioBuffer& buffer, const int16_t* data, size_t frames, bool blocking) {
        if (frames == 0) return 0;

        /* Durante el warmup no se ajusta el DRC, pero SI se remuestrea: con una
         * tasa de dispositivo fija, escribir crudo sonaria a destiempo. */
        if (warmup > 0) {
            warmup--;
        } else {
            updateRatio(buffer.getUsed());
        }

        int16_t tmpBuf[DRC_MAX_FRAMES * 2];
        size_t  done = 0;

#ifdef AUDIO_LOG
        size_t producedTotal = 0;
#endif

        while (done < frames) {
            /* Tope de entrada por pasada.  Ademas de trocear los lotes que
             * darian mas de DRC_MAX_FRAMES de salida, acota la aritmetica de
             * posFx: dentro de resampleChunk se calcula `consumed << 16` en
             * uint32, que con un lote de mas de 65535 frames desbordaria. Con
             * este tope el margen es de tres ordenes de magnitud. */
            size_t chunkIn = frames - done;
            if (chunkIn > RATE_MAX_CHUNK_IN) chunkIn = RATE_MAX_CHUNK_IN;

            size_t consumed = 0;
            size_t outFrames = resampleChunk(data + done * 2, chunkIn,
                                             tmpBuf, DRC_MAX_FRAMES, &consumed);

            if (outFrames > 0) {
                if (blocking)
                    buffer.WriteBlocking(tmpBuf, outFrames * 2);
                else
                    buffer.Write(tmpBuf, outFrames * 2);
            }
#ifdef AUDIO_LOG
            producedTotal += outFrames;
#endif

            /* Sin entrada consumida no hay forma de avanzar `done`, asi que
             * seguir dando vueltas seria un bucle infinito reescribiendo el
             * mismo trozo.  Con los topes de recalcStep() no deberia ocurrir;
             * el corte esta por seguridad, no por diseno. */
            if (consumed == 0) break;
            done += consumed;
        }

#ifdef AUDIO_LOG
        // Trace periodico cada 2s: resampler stats
        {
            static DWORD lastRsTrace = 0;
            static size_t totalIn = 0, totalOut = 0, batchCount = 0;
            totalIn += frames;
            totalOut += producedTotal;
            batchCount++;
            DWORD now = GetTickCount();
            if (now - lastRsTrace >= 2000) {
                lastRsTrace = now;
                LOG_DEBUG("[DRC] batches=%lu avg_in=%lu avg_out=%lu total_ratio=%.4f (base=%.4f)",
                    (unsigned long)batchCount,
                    (unsigned long)(totalIn / batchCount),
                    (unsigned long)(totalOut / batchCount),
                    totalOut > 0 ? (double)totalIn / (double)totalOut : 0.0,
                    baseRatio);
                totalIn = totalOut = batchCount = 0;
            }
        }
#endif

        return frames;
    }

    // Accessors para debug/overlay
    double getCurrentRatio() const { return ratio; }
    double getSmoothedFill() const { return smoothedFill; }
};
