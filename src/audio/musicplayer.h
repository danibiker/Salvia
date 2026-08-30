#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <audio/audiobuffer.h>
#include <audio/audiorate.h>
#include <const/constant.h>    /* IO_THREAD, Constant::setup_and_run_thread */
#include <utils/logger.h>

extern "C" {
#include <formats/rmp3.h>
}

/* ==========================================================================
 * MUSICA DE MENU (mp3, por streaming)
 *
 * Suena mientras el estado NO es partida en marcha ni overlay sobre ella (el
 * predicado vive en salvia.cpp: musicWantedFor).  Como las dos fuentes nunca
 * suenan a la vez, el callback de SDL solo elige de donde lee; no hay mezclador.
 *
 * STREAMING y no fichero entero en RAM: cargar varios MB al arrancar retrasa el
 * inicio y, ahora que la musica tambien suena en el menu de pausa, habria que
 * mantenerlos ocupados durante toda la partida.  Con la API rmp3_stream_* el
 * coste en memoria es una ventana de entrada mas un frame decodificado --
 * decenas de KB en vez de megas -- y el arranque no lee nada mas que la primera
 * ventana.
 *
 * Reparto de trabajo en TRES hilos:
 *
 *   update()    hilo PROPIO pineado a IO_THREAD (HW4): lee del disco,
 *               decodifica y remuestrea al anillo
 *   readInto()  hilo de AUDIO (HW5, el de SDL): solo lee del anillo
 *   setWanted() hilo PRINCIPAL: dice si toca musica segun el estado
 *
 * El decodificado tuvo su propio hilo desde el momento en que se vio que
 * corriendo en el hilo principal la musica se cortaba en cuanto el menu tardaba
 * en listar ficheros: el escaneo de directorios bloquea ese hilo y el anillo se
 * vaciaba.  En HW4 el planificador sigue dando rodajas aunque el principal este
 * ocupado, asi que el anillo se rellena igual.  HW4 es donde ya viven los demas
 * trabajos de E/S del frontend (descargas, carga de assets).
 *
 * El anillo sigue siendo SPSC: UN productor (el hilo de musica, antes el
 * principal) y UN consumidor (el de audio).  AudioBuffer esta disenado para eso.
 *
 * El anillo es un miembro fijo y NO se libera nunca, igual que el g_audioBuffer
 * del core.  Lo unico que stop() suelta son el decodificador y el FILE*, y de
 * eso solo se ocupa el hilo principal, asi que liberar no puede competir con el
 * callback y no hace falta ningun flag tipo `audio_closing`.
 *
 * De momento solo mp3.  Anadir ogg/wav es traer rvorbis.c / rwav.c a
 * libs/libretro-common y anadir una rama en load() (ver README-salvia.txt).
 * ========================================================================== */

/* Ventana de entrada del decodificador.  Un mp3 a 128 kbps consume ~16 KB/s, o
 * sea que con esto se hace aproximadamente una lectura de disco por segundo. */
#define MUSIC_IN_WINDOW 16384

/* Frames decodificados por pasada.  1152 y no una potencia de dos: es lo que
 * ocupa un frame MPEG Layer II/III (RMP3_MAX_SAMPLES_PER_FRAME es 1152*2, o sea
 * 1152 por canal).  Con menos capacidad el decodificador no tendria sitio para
 * soltar un frame entero. */
#define MUSIC_DECODE_FRAMES 1152

/* Por debajo de este llenado del anillo, update() decodifica mas. */
#define MUSIC_REFILL_BELOW (BUFF_SIZE / 2)

/* Tope de vueltas por llamada a update().  Evita que un fichero corrupto que
 * nunca produzca muestras se lleve el frame entero girando. */
#define MUSIC_MAX_STEPS 64

/* Periodo de sondeo del hilo MIENTRAS SUENA.  El anillo se mantiene entre media
 * y toda su capacidad, o sea entre 85 y 170 ms a 48 kHz: despertar cada 10 ms
 * deja margen de sobra.
 *
 * Cuando NO suena no hay periodo: el hilo se queda bloqueado en el evento y no
 * consume nada, que es la diferencia con el Sleep fijo que habia antes. */
#define MUSIC_THREAD_POLL_MS 10

/* Duracion de las rampas de entrada y salida. */
#define MUSIC_FADE_MS 1000

/* Ganancia de la rampa, en 1.20.  Se lleva con 20 bits y no con 15 para que el
 * paso por frame salga entero con precision: a 48 kHz y 250 ms son 12.000
 * frames, o sea paso 87 (1048576/12000), que da 12.052 frames = 251 ms.  Con 15
 * bits el paso saldria 2,7 -> 2, y la rampa duraria 341 ms en vez de 250.
 *
 * Al aplicarla se baja a 15 bits (>>5): asi el producto muestra*ganancia es como
 * mucho 32767*32768 = 1.073.741.824 y cabe en int32 con holgura. */
#define MUSIC_GAIN_BITS 20
#define MUSIC_GAIN_ONE  (1 << MUSIC_GAIN_BITS)

/* Declaracion adelantada: la instancia la define y reserva salvia.cpp, pero el
 * menu (gestormenus.cpp) necesita llegar a ella para aplicar el volumen en
 * caliente, y salvia.h no se incluye desde ningun otro sitio. */
class MusicPlayer;
extern MusicPlayer* g_music;

class MusicPlayer {
private:
    AudioBuffer      ring;        /* miembro fijo: nunca se libera */
    AudioRateControl rate;

    rmp3_stream_t*   stream;
    FILE*            file;
    bool             active;
    unsigned         srcChannels;
    unsigned         srcRate;

    /* Hilo de decodificacion (IO_THREAD) y su estado.
     *
     * `cs` protege TODO lo del decodificador (stream, file, inBuf, inFilled,
     * active): sin ella, un stop() desde el hilo principal podria liberar el
     * rmp3_stream_t mientras el hilo de musica esta dentro de update().
     * CRITICAL_SECTION es recursiva para el mismo hilo, que es lo que permite
     * que update() llame a stop() en la rama de error sin bloquearse consigo
     * mismo.
     *
     * `threadRun` y `wantPlay` son volatile y se leen/escriben sin el lock: son
     * banderas de un solo escritor y leerlas rancias como mucho cuesta una
     * pasada de mas o de menos. */
    HANDLE           hThread;
    /* Evento de despertar (auto-reset).  Sustituye al Sleep fijo: mientras no
     * toca musica el hilo espera INFINITE sobre el y no consume ni una rodaja de
     * HW4; al volver al menu, o al salir, se le da un toque y despierta al
     * instante en vez de esperar al siguiente tick. */
    HANDLE           hWake;
    volatile bool    threadRun;
    volatile bool    wantPlay;
    CRITICAL_SECTION cs;

    /* Rampa de volumen.  `gain` lo mueve SOLO el hilo de audio dentro de
     * readInto(), asi que la rampa es exacta muestra a muestra; `gainTarget` lo
     * fija el hilo principal en setWanted().  Un solo escritor cada uno y ambos
     * de 32 bits alineados: no hacen falta candados.
     *
     * gainStep se calcula en load(), cuando ya se conoce la tasa del
     * dispositivo. */
    volatile int32_t gain;
    volatile int32_t gainTarget;
    int32_t          gainStep;

    /* Volumen elegido por el usuario, 0..100.  NO se aplica como una segunda
     * multiplicacion por muestra: se pliega en el destino de la rampa, asi que
     * sigue habiendo un solo producto por muestra y, de regalo, cambiar el
     * volumen desde el menu se oye como un deslizamiento y no como un salto. */
    int volumePct;

    int32_t fullGain() const {
        return (int32_t)(((int64_t)MUSIC_GAIN_ONE * volumePct) / 100);
    }

    /* Scratch.  Miembros y no locales de update(): en la 360 son 24 KB que no
     * queremos en la pila del bucle principal. */
    uint8_t  inBuf[MUSIC_IN_WINDOW];
    size_t   inFilled;
    int16_t  decodeBuf[MUSIC_DECODE_FRAMES * 2];
    int16_t  stereoBuf[MUSIC_DECODE_FRAMES * 2];

    void release() {
        if (stream) {
            rmp3_stream_free(stream);
            stream = NULL;
        }
        if (file) {
            fclose(file);
            file = NULL;
        }
        inFilled = 0;
    }

    /* Rellena la ventana de entrada desde el fichero.  Devuelve los bytes
     * leidos; 0 significa fin de fichero. */
    size_t fillInput() {
        size_t space, got;
        if (!file) return 0;
        space = MUSIC_IN_WINDOW - inFilled;
        if (space == 0) return 0;
        got = fread(inBuf + inFilled, 1, space, file);
        inFilled += got;
        return got;
    }

    /* Vuelve al principio del fichero.  Es musica de menu: se repite. */
    bool rewindStream() {
        if (!file || !stream) return false;
        if (fseek(file, 0, SEEK_SET) != 0) return false;
        rmp3_stream_reset(stream);
        inFilled = 0;
        return true;
    }

    /* Manda al anillo lo que acabe de salir del decodificador, duplicando a
     * estereo si el fichero es mono: toda la cadena (anillo, resampler y
     * dispositivo) es estereo intercalado. */
    void pushDecoded(size_t frames) {
        const int16_t* src = decodeBuf;
        if (frames == 0) return;

        if (srcChannels == 1) {
            for (size_t i = 0; i < frames; i++) {
                const int16_t s = decodeBuf[i];
                stereoBuf[i * 2]     = s;
                stereoBuf[i * 2 + 1] = s;
            }
            src = stereoBuf;
        }

        /* Sin bloquear: si el anillo se llenara se descarta el sobrante, que es
         * lo correcto aqui.  Bloquear colgaria el bucle de menu. */
        rate.processAndWrite(ring, src, frames, false);
    }

    /* Cuerpo del hilo de musica.  Solo decodifica cuando toca y cuando el anillo
     * va bajo; el resto del tiempo duerme, asi que en HW4 no molesta a los demas
     * trabajos de E/S. */
    static DWORD WINAPI threadProc(LPVOID param) {
        MusicPlayer* self = (MusicPlayer*)param;
        while (self->threadRun) {
            /* Se sigue decodificando durante el fade-out: si se parase en cuanto
             * wantPlay baja, el anillo se agotaria a mitad de la rampa y el
             * fundido acabaria en el corte que precisamente viene a evitar. */
            const bool playing = self->wantPlay || !self->isSilent();

            if (playing) {
                EnterCriticalSection(&self->cs);
                if (self->active) self->update();
                LeaveCriticalSection(&self->cs);
            }

            /* Sondeo mientras suena; espera indefinida cuando no.
             *
             * No hay despertar perdido en el cambio de false a true: el evento
             * es AUTO-RESET, asi que si setWanted() lo senala justo despues de
             * leer `playing` aqui arriba, se queda senalado y este Wait vuelve
             * de inmediato en vez de dormirse para siempre. */
            WaitForSingleObject(self->hWake,
                playing ? MUSIC_THREAD_POLL_MS : INFINITE);
        }
        return 0;
    }

    void startThread() {
        if (hThread) return;
        threadRun = true;
        hThread = CreateThread(NULL, 0, threadProc, this, CREATE_SUSPENDED, NULL);
        if (!hThread) {
            /* Sin hilo no hay musica: es preferible eso a decodificar en el hilo
             * principal, que es justo el problema que este hilo viene a resolver. */
            LOG_INFO("Musica: no se pudo crear el hilo de decodificacion\n");
            threadRun = false;
            return;
        }
        /* autoClose=false: el handle se conserva para poder esperarlo al salir. */
        Constant::setup_and_run_thread(hThread, IO_THREAD, false);
    }

    void stopThread() {
        if (!hThread) return;
        threadRun = false;   /* sin el lock: si no, el wait de abajo se abrazaria */
        /* Toque para sacarlo del WaitForSingleObject: sin esto, un hilo parado
         * en INFINITE (que es el caso si estabamos en partida) no saldria nunca
         * y el wait de abajo se comeria los 2 s enteros. */
        if (hWake) SetEvent(hWake);
        WaitForSingleObject(hThread, 2000);
        CloseHandle(hThread);
        hThread = NULL;
    }

public:
    MusicPlayer() {
        stream      = NULL;
        file        = NULL;
        active      = false;
        srcChannels = 0;
        srcRate     = 0;
        inFilled    = 0;
        hThread     = NULL;
        threadRun   = false;
        wantPlay    = false;
        gain        = 0;
        gainTarget  = 0;
        gainStep    = 1;
        volumePct   = 100;
        /* Auto-reset y sin senalar: ver el comentario de hWake. */
        hWake       = CreateEvent(NULL, FALSE, FALSE, NULL);
        InitializeCriticalSection(&cs);
    }

    ~MusicPlayer() {
        stopThread();          /* primero el hilo, luego se puede liberar */
        release();
        if (hWake) {
            CloseHandle(hWake);
            hWake = NULL;
        }
        DeleteCriticalSection(&cs);
    }

    bool isActive() const { return active; }

    /* Lo llama el hilo PRINCIPAL una vez por frame con el resultado de
     * musicWantedFor(estado).  No decodifica: solo abre o cierra el grifo del
     * hilo de musica, asi que un menu atascado no puede pararlo.
     *
     * Solo se toca el evento en el FLANCO a true: llamarlo 60 veces por segundo
     * con el mismo valor no debe convertirse en 60 llamadas al kernel.  Para el
     * flanco contrario no hace falta aviso: el hilo lo ve en su siguiente
     * sondeo, como mucho 10 ms despues, y ahi ya se bloquea. */
    void setWanted(bool wanted) {
        if (wantPlay == wanted) return;
        wantPlay   = wanted;
        gainTarget = wanted ? fullGain() : 0;
        if (wanted && hWake) SetEvent(hWake);
    }

    /* Volumen 0..100.  Se puede llamar en caliente desde el menu: la rampa se
     * encarga de llegar al nivel nuevo sin escalon. */
    void setVolume(int pct) {
        if (pct < 0)   pct = 0;
        if (pct > 100) pct = 100;
        if (volumePct == pct) return;
        volumePct = pct;

        if (wantPlay) {
            gainTarget = fullGain();
            /* Al subir desde 0 el hilo esta dormido en INFINITE (con volumen 0
             * isSilent() es cierto y deja de decodificar), asi que hay que
             * darle un toque o la musica no volveria hasta el siguiente cambio
             * de estado. */
            if (gainTarget > 0 && hWake) SetEvent(hWake);
        }
    }

    int getVolume() const { return volumePct; }

    /* Cierto cuando la rampa ya esta abajo del todo y no hay intencion de
     * subirla.  Lo consulta el callback: mientras sea falso hay que SEGUIR
     * leyendo musica aunque el estado ya no la pida, porque si no el fade-out no
     * llegaria a oirse -- la fuente cambiaria de golpe antes de que la rampa
     * bajase. */
    bool isSilent() const { return gain == 0 && gainTarget == 0; }

    /* Abre el fichero y deja el decodificador listo.  deviceRate es la tasa REAL
     * a la que quedo abierto el dispositivo (g_audio_device_rate), no la pedida. */
    bool load(const std::string& path, int deviceRate) {
        int      steps;
        unsigned ch = 0, hz = 0;

        stop();

        /* Todo el montaje del decodificador va bajo el lock: el hilo de musica
         * puede estar despierto de una carga anterior. */
        EnterCriticalSection(&cs);

        file = fopen(path.c_str(), "rb");
        if (!file) {
            LOG_INFO("Musica: no se pudo abrir '%s'; el menu se queda en silencio\n",
                path.c_str());
            LeaveCriticalSection(&cs);
            return false;
        }

        stream = rmp3_stream_new();
        if (!stream) {
            LOG_INFO("Musica: sin memoria para el decodificador\n");
            release();
            LeaveCriticalSection(&cs);
            return false;
        }

        /* Averiguar tasa y canales SIN decodificar: con la salida a NULL el
         * stream solo localiza frames, y rmp3_stream_info ya responde en cuanto
         * encuentra el primero.  Luego se rebobina y se decodifica desde el
         * principio, asi no se pierde el primer frame. */
        rmp3_stream_set_out_s16(stream, NULL, 0);
        inFilled = 0;
        fillInput();

        for (steps = 0; steps < MUSIC_MAX_STEPS; steps++) {
            size_t read = 0, wrote = 0;
            int    r;

            rmp3_stream_set_in(stream, inBuf, inFilled);
            r = rmp3_stream_process(stream, &read, &wrote);

            if (read > 0 && read <= inFilled) {
                memmove(inBuf, inBuf + read, inFilled - read);
                inFilled -= read;
            }

            if (rmp3_stream_info(stream, &ch, &hz) && hz > 0) break;

            if (r == RMP3_STREAM_NEED_IN) {
                if (fillInput() == 0 && feof(file)) rmp3_stream_set_eof(stream);
            } else if (r != RMP3_STREAM_OK) {
                break;
            }
        }

        if (hz == 0 || ch == 0) {
            LOG_INFO("Musica: '%s' no parece un mp3 valido\n", path.c_str());
            release();
            LeaveCriticalSection(&cs);
            return false;
        }

        srcChannels = ch;
        srcRate     = hz;

        if (!rewindStream()) {
            LOG_INFO("Musica: '%s' no se puede rebobinar\n", path.c_str());
            release();
            LeaveCriticalSection(&cs);
            return false;
        }
        rmp3_stream_set_out_s16(stream, decodeBuf, MUSIC_DECODE_FRAMES);

        ring.Clear();
        rate.init(BUFF_SIZE);
        rate.setRates((double)srcRate, (double)deviceRate);

        /* Paso de la rampa para los MUSIC_FADE_MS pedidos a la tasa REAL del
         * dispositivo.  Suelo de 1: con un paso 0 la rampa no avanzaria nunca. */
        {
            const int fadeFrames = (deviceRate > 0)
                ? (int)(((int64_t)MUSIC_FADE_MS * deviceRate) / 1000) : 1;
            gainStep = (fadeFrames > 0) ? (MUSIC_GAIN_ONE / fadeFrames) : MUSIC_GAIN_ONE;
            if (gainStep < 1) gainStep = 1;
        }

        /* Arranca abajo para que la primera reproduccion tambien entre con
         * fundido.  El destino se re-sincroniza con la intencion actual: si no,
         * un load() posterior a un stop() se quedaria mudo, porque setWanted()
         * corta en seco cuando el valor no cambia y no volveria a fijarlo. */
        gain       = 0;
        gainTarget = wantPlay ? fullGain() : 0;

        active = true;
        LOG_INFO("Musica: '%s' %u Hz %u canales -> dispositivo %d Hz (ratio %.4f), streaming\n",
            path.c_str(), srcRate, srcChannels, deviceRate, rate.getBaseRatio());

        /* Adelantar trabajo: si el anillo llega vacio al primer callback se oye
         * el fade de underrun de AudioBuffer antes que la musica. */
        update();

        LeaveCriticalSection(&cs);

        /* El hilo va DESPUES de soltar el lock: arrancarlo dentro seria pedirle
         * que espere a que terminemos. */
        startThread();
        return true;
    }

    void stop() {
        EnterCriticalSection(&cs);
        active = false;
        release();
        ring.Clear();
        rate.reset();
        srcChannels = 0;
        srcRate     = 0;
        /* Corte seco: stop() es el desmontaje (cierre de la app o recarga), no
         * una pausa.  El fundido de verdad es el de setWanted(false), que baja
         * la rampa ANTES de que nadie llame aqui. */
        gain       = 0;
        gainTarget = 0;
        LeaveCriticalSection(&cs);
    }

    /* Lo llama el HILO DE MUSICA (threadProc), siempre con `cs` tomada.  No
     * llamarlo desde el hilo principal: el motivo de que exista el hilo es
     * precisamente que ahi se quedaba parado cuando el menu listaba ficheros.
     *
     * Rellena el anillo si va bajo, con el bucle canonico de la API de streaming
     * de rmp3 (ver el comentario de rmp3.h): presentar la ventana, procesar,
     * deslizar por los bytes consumidos y reponer cuando pide mas entrada. */
    void update() {
        int steps;

        if (!active || !stream || !file) return;

        for (steps = 0; steps < MUSIC_MAX_STEPS; steps++) {
            size_t read = 0, wrote = 0;
            int    r;

            if (ring.getUsed() >= (size_t)MUSIC_REFILL_BELOW) break;

            /* La salida se fija en cada pasada: asi `wrote` siempre cuenta desde
             * el principio de decodeBuf y no hay que llevar un cursor. */
            rmp3_stream_set_out_s16(stream, decodeBuf, MUSIC_DECODE_FRAMES);
            rmp3_stream_set_in(stream, inBuf, inFilled);

            r = rmp3_stream_process(stream, &read, &wrote);

            /* La entrada se CONSUME, no se presta: lo que no se leyo hay que
             * volver a presentarlo, de ahi el deslizamiento de la ventana. */
            if (read > 0 && read <= inFilled) {
                memmove(inBuf, inBuf + read, inFilled - read);
                inFilled -= read;
            }

            if (wrote > 0) {
                pushDecoded(wrote);
            }

            if (r == RMP3_STREAM_NEED_IN) {
                if (fillInput() == 0 && feof(file)) {
                    /* No hay mas bytes: hay que decirlo para que decodifique la
                     * cola, porque un frame incompleto es indistinguible de
                     * basura y el decodificador se quedaria esperando ventana. */
                    rmp3_stream_set_eof(stream);
                }
            } else if (r == RMP3_STREAM_END) {
                if (!rewindStream()) {
                    LOG_INFO("Musica: no se pudo rebobinar; se detiene\n");
                    stop();
                    return;
                }
                rmp3_stream_set_out_s16(stream, decodeBuf, MUSIC_DECODE_FRAMES);
                fillInput();
            } else if (r == RMP3_STREAM_ERROR) {
                LOG_INFO("Musica: error de decodificacion; se detiene\n");
                stop();
                return;
            }
        }
    }

    /* HILO DE AUDIO.  Lee del anillo y le aplica la rampa de volumen; jamas
     * decodifica ni toca punteros que el hilo principal pueda estar liberando.
     *
     * La rampa se hace AQUI y no en el productor a proposito: es el unico sitio
     * donde el avance es exactamente un frame por frame reproducido, asi que la
     * duracion del fundido es la real y no depende de lo lleno que este el
     * anillo ni de cada cuanto despierte el hilo de decodificacion. */
    void readInto(int16_t* stream_out, size_t count) {
        const size_t  frames = count / 2;
        const int32_t target = gainTarget;
        int32_t       g      = gain;
        size_t        i;

        ring.Read(stream_out, count);

        /* Atajo del caso normal: ya esta arriba del todo, no hay nada que
         * escalar y el bucle de abajo seria multiplicar por 1 cientos de veces. */
        if (g == MUSIC_GAIN_ONE && target == MUSIC_GAIN_ONE) return;

        for (i = 0; i < frames; i++) {
            if (g < target) {
                g += gainStep;
                if (g > target) g = target;
            } else if (g > target) {
                g -= gainStep;
                if (g < target) g = target;
            }

            /* >>5 baja la ganancia de 1.20 a 1.15; ver el comentario de
             * MUSIC_GAIN_BITS para por que el producto cabe en int32. */
            {
                const int32_t gq = g >> 5;
                stream_out[i * 2]     = (int16_t)(((int32_t)stream_out[i * 2]     * gq) >> 15);
                stream_out[i * 2 + 1] = (int16_t)(((int32_t)stream_out[i * 2 + 1] * gq) >> 15);
            }
        }

        gain = g;
    }
};
