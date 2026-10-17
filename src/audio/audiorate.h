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

/* Autoridad del control dinamico de ritmo: el ratio se mueve como maximo
 * +-DRC_DELTA respecto al nominal.  0,005 es el `DEFAULT_RATE_CONTROL_DELTA` de
 * RetroArch, y basta porque el grueso de la correccion NO es realimentacion: la
 * pone la alimentacion directa de `setRates` con las tasas MEDIDAS.  El valor
 * anterior, 0,02, era cuatro veces esto y en los extremos se oye el desafine. */
#define DRC_DELTA 0.005

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
    bool   drcPending;      // el frame ha empezado: toca recomputar el ratio

    // Estado del resampler
    uint32_t stepFx;        // `ratio` en 16.16: frames de entrada por frame de salida
    uint32_t posFx;         // posicion fraccionaria, PERSISTENTE entre lotes
    int16_t  prevL;         // ultimo frame del lote anterior: es la muestra
    int16_t  prevR;         // izquierda de la primera interpolacion del lote actual
    bool     hasPrev;

    // Configuracion
    size_t bufCapacity;
    double srcRateDeclared;   // la que declara el core
    double dstRateDeclared;   // la que declaro SDL al abrir el dispositivo
    double dstRateMeasured;   // la CONTADA; 0 = sin medida todavia

    // Constantes de tuning
    static const int WARMUP_FRAMES = 30;

    /* El ratio nominal: tasa del core entre la tasa REAL del dispositivo, con la
     * declarada como respaldo mientras no haya medida.  Ver
     * `setDeviceRateMeasured` para el razonamiento. */
    void recalcBase() {
        const double dst = (dstRateMeasured > 0.0) ? dstRateMeasured : dstRateDeclared;

        if (srcRateDeclared <= 0.0 || dst <= 0.0)
            baseRatio = 1.0;
        else
            baseRatio = srcRateDeclared / dst;
    }

    void recalcStep() {
        double r = ratio;
        if (r < 0.05) r = 0.05;           /* topes de cordura: un ratio absurdo */
        if (r > 20.0) r = 20.0;           /* solo podria venir de una tasa mala */
        stepFx = (uint32_t)(r * (double)RATE_ONE + 0.5);
        if (stepFx == 0) stepFx = 1;
    }

    /* --- Controlador proporcional, sin filtro y con autoridad acotada -------
     *
     * Tres cambios respecto a la version anterior, los tres con el mismo origen:
     * asi es como lo hace RetroArch, y las razones estan documentadas en su
     * codigo.
     *
     * 1) SIN EMA EN EL ERROR.  Antes habia `smoothedFill` con alpha 0,1.  Un
     *    controlador proporcional que actua sobre un INTEGRADOR -y el anillo lo
     *    es- ya forma un lazo de primer orden estable; filtrar el error solo
     *    anade retardo de fase.  RetroArch usa la muestra CRUDA de `write_avail`
     *    y su anillo de 8192 muestras es telemetria, no lazo
     *    (audio_driver.c:1243-1245, dice "for diagnostics" literalmente).
     *
     * 2) AUTORIDAD DE +-0,005 en vez de +-0,02.  El tope viejo era 4x el de
     *    RetroArch (`DEFAULT_RATE_CONTROL_DELTA 0.005f`) y en los extremos
     *    desafina de forma audible.  Con el error normalizado a [-1,+1] el
     *    ratio queda acotado exactamente a [1-delta, 1+delta].
     *
     * 3) EL ERROR SE NORMALIZA.  `(fill - 0.5) / 0.5` en vez de `fill - 0.5`,
     *    para que el delta signifique de verdad "autoridad maxima" y no la
     *    mitad.  Es la forma de `direction = delta_mid / half_size` de
     *    RetroArch.
     *
     * Signo, que conviene tener escrito porque es facil equivocarse: `ratio` son
     * frames de ENTRADA por frame de SALIDA, asi que un ratio MENOR produce MAS
     * salida.  Anillo vaciandose -> fill < 0,5 -> error < 0 -> adj < 0 -> ratio
     * baja -> mas salida -> el anillo se rellena.  Correcto. */
    void updateRatio(size_t currentUsed) {
        double fill = (double)currentUsed / (double)bufCapacity;

        /* Se conserva `smoothedFill` SOLO para el overlay y las trazas: ya no
         * entra en el lazo. */
        smoothedFill = fill;

        double error = (fill - 0.5) / 0.5;      /* normalizado a [-1, +1] */

        double adj = error * DRC_DELTA;

        if (adj >  DRC_DELTA) adj =  DRC_DELTA;
        if (adj < -DRC_DELTA) adj = -DRC_DELTA;

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
        drcPending = false;
        srcRateDeclared = 0.0;
        dstRateDeclared = 0.0;
        dstRateMeasured = 0.0;
    }

    void init(size_t capacity) {
        bufCapacity = capacity;
        smoothedFill = 0.5;
        ratio = baseRatio;          /* sin ajuste de DRC todavia */
        warmup = WARMUP_FRAMES;
        drcPending = false;
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
        srcRateDeclared = srcRate;
        dstRateDeclared = dstRate;
        /* La medida del dispositivo sobrevive a un cambio de core: es una
         * propiedad del cristal del hardware, no del juego. */
        recalcBase();
        ratio = baseRatio;
        posFx = 0;
        prevL = 0;
        prevR = 0;
        hasPrev = false;
        recalcStep();
    }

    double getBaseRatio() const { return baseRatio; }

    /* Lo llama el frontend justo DESPUES de `retro_run`, una vez por frame.  Es
     * lo que ancla la fase del muestreo del DRC: ver el bloque de
     * `processAndWrite`. */
    void frameEnd() { drcPending = true; }

    /* --- ALIMENTACION DIRECTA con la tasa MEDIDA del dispositivo ------------
     *
     * El grueso de la correccion tiene que ser feed-forward, no realimentacion:
     * por eso a RetroArch le basta con +-0,5% de autoridad en el DRC.  Si el
     * ratio nominal esta mal, el controlador vive saturado o sostiene un error
     * de regimen permanente -un P puro no puede anular un offset constante-.
     *
     * OJO CON LA FORMULA DE RETROARCH: la suya lleva un factor
     * `refresco / fpsCore` porque RetroArch esclaviza la emulacion al PANEL
     * (vsync como reloj maestro).  Salvia la esclaviza al limitador A LA TASA
     * DEL CORE, que es lo correcto -un juego PAL debe correr a sus 50 Hz aunque
     * la pantalla vaya a 60-, asi que aqui ese factor NO va: con 60/50 = 1,2
     * aceleraria el audio un 20% en todos los cores PAL.
     *
     * Lo que si corresponde es usar la tasa REAL del dispositivo en vez de la
     * declarada.  Medida: 47992-48006 Hz frente a los 48000 del `obtained` de
     * SDL, o sea +-0,017%; pequeno, pero es correccion gratis y es la unica que
     * no puede hacer el DRC sin gastar autoridad. */
    void setDeviceRateMeasured(double medida) {
        if (medida <= 0.0 || dstRateDeclared <= 0.0)
            return;
        /* Guarda de cordura: una medida que se aparte mas de un 2% de lo
         * declarado es una ventana contaminada (una carga, un savestate), no
         * una propiedad del cristal.  Se ignora. */
        if (medida < dstRateDeclared * 0.98 || medida > dstRateDeclared * 1.02)
            return;

        dstRateMeasured = medida;
        recalcBase();
    }

    // Punto de entrada principal.
    // Calcula el ratio, remuestrea y escribe al buffer.
    size_t processAndWrite(AudioBuffer& buffer, const int16_t* data, size_t frames, bool blocking) {
        if (frames == 0) return 0;

        /* Durante el warmup no se ajusta el DRC, pero SI se remuestrea: con una
         * tasa de dispositivo fija, escribir crudo sonaria a destiempo. */
        if (warmup > 0) {
            warmup--;
        } else if (drcPending) {
            /* UNA SOLA VEZ POR FRAME, Y CON LA FASE ANCLADA.
             *
             * Esto estaba mal y no era obvio: `updateRatio` se llamaba en CADA
             * `processAndWrite`, o sea una vez por LOTE de audio.  Un core que
             * entrega N lotes por frame muestreaba la ocupacion N veces con
             * fase sub-frame distinta, metiendo ruido en un lazo cuya constante
             * de tiempo es de UN FRAME.  RetroArch lo documenta explicitamente
             * (audio_driver.h:757-777) y lo resuelve con este mismo mecanismo:
             * un flag que se arma despues de `retro_run` (`drc_pending`,
             * audio_driver.c:6665-6667) y que hace que el recompute caiga
             * siempre en el primer lote del frame siguiente, o sea siempre en el
             * mismo punto del frame.
             *
             * Entre recomputes se reutiliza el ratio ya calculado, que es lo que
             * hace `cached_rate_adjust` alli. */
            drcPending = false;
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
