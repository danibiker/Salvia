#pragma once

#include <stdint.h>
#include <string.h>
#include <audio/audiobuffer.h>

// Maximo de frames estereo que puede producir el resampler por llamada.
// 2048 frames * 2 canales * 2 bytes = 8KB en stack — sobrado para cualquier core.
#define DRC_MAX_FRAMES 2048

class AudioRateControl {
private:
    // Estado del controlador
    double smoothedFill;    // nivel de llenado filtrado (0.0 a 1.0)
    double ratio;           // ratio de resampling actual (cercano a 1.0)
    int    warmup;          // frames restantes de warmup (sin DRC)

    // Estado del resampler (continuidad entre batches)
    int16_t lastL;
    int16_t lastR;
    bool    hasLast;

    // Configuracion (fijada en init)
    size_t bufCapacity;

    // Constantes de tuning
    static const int WARMUP_FRAMES = 30;

    // --- Controlador proporcional con EMA ---
    void updateRatio(size_t currentUsed) {
        // Fraccion de llenado actual
        double fill = (double)currentUsed / (double)bufCapacity;

        // Filtro EMA (alpha = 0.1, ~10 frames de constante de tiempo)
        const double alpha = 0.1;
        smoothedFill = smoothedFill * (1.0 - alpha) + fill * alpha;

        // Error: positivo si el buffer esta mas lleno de lo deseado.
        // Rango: -0.5 (buffer vacio) .. +0.5 (buffer lleno).
        double error = smoothedFill - 0.5;

        // Ajuste proporcional.  kp escogido para que un error maximo
        // (|0.5|) produzca el clamp maximo (|0.02| = 2%), es decir:
        //   kp = 0.02 / 0.5 = 0.04
        // El kp anterior (0.005) era 8x demasiado pequeno: incluso con
        // el buffer 100% lleno, adj apenas alcanzaba 0.25%, que es
        // insuficiente para drenar el buffer durante el catch-up tras
        // stalls del CD.  Resultado: buffer se llenaba, Write() no-
        // bloqueante descartaba muestras silenciosamente, cada drop
        // era una discontinuidad audible (crujido continuo).
        double adj = error * 0.04;

        // Clamp a +-2% maximo (limite de pitch shift imperceptible).
        if (adj >  0.02) adj =  0.02;
        if (adj < -0.02) adj = -0.02;

        // ratio > 1.0: avanzamos mas rapido en el input -> producimos menos output -> buffer baja
        // ratio < 1.0: avanzamos mas lento -> producimos mas output -> buffer sube
        ratio = 1.0 + adj;
    }

    // --- Resampler lineal estereo ---
    // Devuelve el numero de frames de salida escritos en dst.
    size_t resample(const int16_t* src, size_t srcFrames, int16_t* dst, size_t dstMaxFrames) {
        if (srcFrames == 0) return 0;

        // Numero estimado de frames de salida
        size_t outFrames = (size_t)((double)srcFrames / ratio + 0.5);
        if (outFrames > dstMaxFrames) outFrames = dstMaxFrames;
        if (outFrames == 0) outFrames = 1;

        // Paso fraccional en el espacio de entrada por cada frame de salida
        double step = (double)srcFrames / (double)outFrames;
        double pos = 0.0;

        for (size_t i = 0; i < outFrames; i++) {
            size_t idx = (size_t)pos;
            double frac = pos - (double)idx;

            // Muestra izquierda (s0) y derecha (s1) para interpolar
            int16_t s0L, s0R, s1L, s1R;

            if (idx < srcFrames) {
                s0L = src[idx * 2];
                s0R = src[idx * 2 + 1];
            } else {
                s0L = src[(srcFrames - 1) * 2];
                s0R = src[(srcFrames - 1) * 2 + 1];
            }

            if (idx + 1 < srcFrames) {
                s1L = src[(idx + 1) * 2];
                s1R = src[(idx + 1) * 2 + 1];
            } else {
                s1L = s0L;
                s1R = s0R;
            }

            // Interpolacion lineal.
            //
            // Aritmetica en int (32-bit) para evitar UB del estandar C
            // al convertir doubles fuera de rango a int16_t.  El delta
            // (s1L - s0L) puede ser hasta 65535; multiplicado por frac
            // (0..1) puede dar 0..65535, que no cabe en int16_t.  Hacer
            // el calculo en int y clampar antes del cast final garantiza
            // que el cast a int16_t sea siempre dentro de rango.
            //
            // En la practica en PPC del 360 con MSVC el wraparound de
            // 2's complement daba el resultado correcto por casualidad
            // matematica, pero es UB segun el estandar y puede generar
            // artefactos en optimizaciones agresivas o compiladores
            // distintos.
            int interpL = s0L + (int)((double)(s1L - s0L) * frac);
            int interpR = s0R + (int)((double)(s1R - s0R) * frac);
            if (interpL >  32767) interpL =  32767;
            if (interpL < -32768) interpL = -32768;
            if (interpR >  32767) interpR =  32767;
            if (interpR < -32768) interpR = -32768;
            dst[i * 2]     = (int16_t)interpL;
            dst[i * 2 + 1] = (int16_t)interpR;

            pos += step;
        }

        // Guardar ultima muestra del input para continuidad en el siguiente batch
        lastL = src[(srcFrames - 1) * 2];
        lastR = src[(srcFrames - 1) * 2 + 1];
        hasLast = true;

        return outFrames;
    }

public:
    AudioRateControl() {
        smoothedFill = 0.5;
        ratio = 1.0;
        warmup = 0;
        lastL = 0;
        lastR = 0;
        hasLast = false;
        bufCapacity = 8192;
    }

    void init(size_t capacity) {
        bufCapacity = capacity;
        smoothedFill = 0.5;
        ratio = 1.0;
        warmup = WARMUP_FRAMES;
        lastL = 0;
        lastR = 0;
        hasLast = false;
    }

    void reset() {
        smoothedFill = 0.5;
        ratio = 1.0;
        warmup = 0;
        lastL = 0;
        lastR = 0;
        hasLast = false;
    }

    // Punto de entrada principal.
    // Calcula el ratio, resamplea y escribe al buffer.
    size_t processAndWrite(AudioBuffer& buffer, const int16_t* data, size_t frames, bool blocking) {
        if (frames == 0) return 0;

        // Durante el warmup, escribir directo sin DRC
        if (warmup > 0) {
            warmup--;
            if (blocking)
                buffer.WriteBlocking(data, frames * 2);
            else
                buffer.Write(data, frames * 2);
            return frames;
        }

        // Actualizar ratio segun nivel del buffer
        updateRatio(buffer.getUsed());

        // Resamplear en buffer temporal en stack
        int16_t tmpBuf[DRC_MAX_FRAMES * 2];
        size_t maxOut = DRC_MAX_FRAMES;
        if (maxOut > frames + frames / 10 + 16)
            maxOut = frames + frames / 10 + 16; // no sobredimensionar

        size_t outFrames = resample(data, frames, tmpBuf, maxOut);

        // Escribir al ring buffer
        if (blocking)
            buffer.WriteBlocking(tmpBuf, outFrames * 2);
        else
            buffer.Write(tmpBuf, outFrames * 2);

        return frames;
    }

    // Accessors para debug/overlay
    double getCurrentRatio() const { return ratio; }
    double getSmoothedFill() const { return smoothedFill; }
};
