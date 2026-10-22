#pragma once

#include <const/constant.h>
#include <stdint.h>

#include <beans/structures.h>

//Only to set the number of frames to count on a buffer
#define FPS_AVG_COUNT 20
#define FPS_DESIRED 60
const uint32_t TIME_AVG_COUNT = FPS_AVG_COUNT * 1000;

/* Ventana de las estadisticas de framepacing: 120 presentaciones ~= 2 s. */
#define PACE_WINDOW 120

/* Margen que el limitador reserva para la espera activa (ms).  INIT es el valor
 * de arranque, conservador (el que era constante antes de medirlo); FLOOR es el
 * suelo al que converge en una plataforma con reloj de 1 ms. */
#define SLEEP_MARGIN_INIT  2.00
#define SLEEP_MARGIN_FLOOR 0.25

/* --- Vsync adaptativo (ver el bloque largo en los miembros de Sync) ---------
 *
 * TARDE_MS es la misma tolerancia de 1 ms que ya usa `paceLate`, para que las
 * dos metricas cuenten lo mismo y no puedan discrepar.
 *
 * LENTO/RAPIDO son la banda muerta sobre el periodo CONSEGUIDO (`dtAvg`, media
 * movil de present-to-present): por encima de target+LENTO se apaga el vsync,
 * por debajo de target+RAPIDO se enciende, y en medio no se toca.
 *
 * ** LA SENAL ES EL PERIODO, NO EL SIGNO DE `diff` **, y esto costo dos
 * iteraciones.  Medido (log 21): `late=40-80 de 120` con `dt=16.67` EXACTO, o
 * sea a pleno rendimiento.  El limitador apunta AL deadline, asi que caer unas
 * decimas antes o despues es una moneda al aire y `diff > 0` es un SORTEO cuando
 * el core si llega.  Resultado: 91 conmutaciones en un solo log.  No se
 * arreglaba con rachas de consecutivos -se quedaba inerte, ~7 s para reaccionar-
 * ni con un integrador saturante -seguia aleteando-: el defecto estaba en la
 * SENAL, no en el filtro.
 *
 * `dtAvg` separa sin solaparse en ese mismo log: 16,63-16,81 ms en los tramos
 * buenos contra 18,06-20,32 en los malos, asi que 1,00 y 0,40 caen holgados a
 * cada lado.  El `flip` lo corrobora: 0,70-0,81 ms de media con `blk=2-40/120`
 * frente a 1,85-4,37 con `blk=59-92/120` y picos de 13-14 ms.  O sea que con el
 * vsync ENCENDIDO y bien acompasado el Present cuesta 0,7 ms; solo duele cuando
 * ya vamos descolgados, y por eso el criterio tiene que ser "llegamos o no" y no
 * "vamos unas decimas por delante".
 *
 * Los umbrales del PRIMER diseno (rachas de 60/120 frames, holgura de 7 ms)
 * existian porque conmutar costaba un Reset del device (SDL_XBOX_SetVSync) y
 * habia que evitar la oscilacion a toda costa.  Eso murio midiendo: el Reset
 * invalida los recursos D3DPOOL_DEFAULT que el core libretro tiene en el device
 * compartido -Xenon64 dibuja a una textura D3D- y la pantalla se queda NEGRA.
 * Con D3DRS_PRESENTINTERVAL no hay Reset y conmutar es barato.
 *
 * Los umbrales salen de los tres regimenes MEDIDOS, y separan los tres:
 *   log 21 bueno   dt 16,63-16,81  ->  < 16,87  ->  ON
 *   log 21 malo    dt 18,06-20,32  ->  > 17,17  ->  OFF
 *   log 22         dt 17,73        ->  > 17,17  ->  OFF
 * (con LENTO=1,00 el ultimo caso quedaba a 0,06 ms del umbral: demasiado justo)
 *
 * GUARDA es el tiempo minimo de permanencia, y sin el las dos reglas se PELEAN.
 * Medido en el log 22: `camb=2071`, unas 20 conmutaciones por segundo. Con el
 * core a 57 fps `limit_fps` re-ancla cada ~3 frames, asi que el escape
 * inmediato salta cada ~3 frames y fuerza OFF mientras el regimen lo vuelve a
 * encender. Ademas cada conmutacion CAMBIA lo que se esta midiendo -apagar el
 * vsync acelera al core-, asi que hay que dejar que la media se asiente.
 *
 * ** HUBO un tercer disparador por "anillo de audio seco" (AUDIO_SECO/AUDIO_OK,
 * calibrados dos veces: 25/40 y luego 12/22) y esta RETIRADO tras medirlo en
 * asincrono.  Era palanca equivocada -media el deficit de audio del CORE y
 * reaccionaba con un ajuste de VIDEO- y aleteaba, porque el anillo vive justo
 * encima de sus umbrales.  El detalle, en `update_vsync_adaptativo`.
 *
 * ** Y OJO CON LA SALUD DEL PACING: la metrica buena NO es `late`.  Contra los
 * contadores de vblank del hardware, en el mismo log: `vbl/frame` 1,002-1,010
 * -o sea 0,2-1,0% de frames que se comen un vblank extra- frente a `late`
 * 40-75 de 120, que son 33-63%.  **El instrumento de deadlines es ~70 veces mas
 * pesimista que la pantalla.**  `late` mide distancia a un deadline de CPU;
 * `vbl/frame - 1,0` es la fraccion exacta de frames que resbalan de verdad.
 * Ver la linea `Sync: pantalla` de salvia.cpp. */
#define VSYNC_TARDE_MS         1.00
#define VSYNC_LENTO_MS         0.50
#define VSYNC_RAPIDO_MS        0.20
#define VSYNC_GUARDA_MS        500.0

/* Palanca del pacer por vblanks (Fase 1).  A 0 se usa el camino de reloj de CPU
 * en las dos plataformas, que es el pacer anterior: sirve de A/B limpio, y es
 * lo que corre en Windows de todas formas porque alli no hay reloj de
 * presentacion. */
#define VBL_PACER              1

/* Palanca del vsync adaptativo POR SOFTWARE (Fase 2).  A 0 -el valor bueno- lo
 * decide el hardware con `D3DRS_PRESENTIMMEDIATETHRESHOLD`, fijado una vez al
 * crear el device.  A 1 vuelve la maquina de estados de abajo, que existe solo
 * para poder comparar. */
#define VSYNC_ADAPTATIVO       0

/* Rejilla del tick del scheduler (ver Sync::note_grid_wake). */
#define GRID_TICK_MS    1.00   /* resolucion pedida con timeBeginPeriod(1) */
#define GRID_EPS_MS     0.30   /* colchon para no pasarse del deadline */
/* Tolerancia de congruencia.  Medido: el despertar cae en la rejilla MAS una
 * latencia variable de ~0.13 ms (el tiempo que tarda el scheduler en devolver el
 * hilo).  Se sabe porque el margen adaptativo llegaba a 1.38 ms, y con una
 * rejilla pura el exceso sobre lo pedido esta acotado por el tick (<1 ms).
 * Con 0.15 esa latencia rompia la racha de aciertos y no se enganchaba nunca.
 * Es tambien el filtro contra enganchar una rejilla que no existe: un despertar
 * aleatorio cae dentro de la tolerancia con probabilidad 2*TOL/TICK, asi que con
 * 0.30 y 12 aciertos seguidos la probabilidad de enganchar por casualidad es
 * ~0.2% por intento, y un enganche falso se deshace en 2 frames (el chequeo de
 * la prediccion) a costa de un frame unas decimas tarde. */
#define GRID_TOL_MS     0.30   /* tolerancia de congruencia con la rejilla */
#define GRID_LOCK_HITS  12     /* aciertos seguidos para fiarse de la rejilla */
#define GRID_MISS_MAX   2      /* fallos seguidos para desengancharse */

/* Latencia de despertar: el timer dispara EN la rejilla de ticks, pero el hilo
 * no vuelve hasta unas decimas despues (lo que tarda el scheduler en darle la
 * CPU).  O sea que la rejilla de DESPERTARES que medimos esta desplazada esa
 * latencia respecto a la rejilla de TICKS contra la que SDL_Delay arma el timer,
 * y la peticion hay que calcularla contra la segunda.
 *
 * Medido en Salvia: ~0.09 ms (el margen adaptativo llegaba a 1.34, o sea un
 * exceso de 1.09 = un tick + 0.09).  Se usa 0.20 con holgura: pasarse un poco
 * solo cuesta despertar un tick antes de vez en cuando (un milisegundo mas de
 * espera activa en esos frames), mientras quedarse corto manda el despertar al
 * tick SIGUIENTE, ya pasado el deadline. */
#define GRID_LAT_MS     0.20

/* ===== FASE 4: QUIEN FRENO EL FRAME =========================================
 *
 * El problema que resuelve esto no es de medida, es de diseno: hay CUATRO
 * mecanismos capaces de regular la velocidad de emulacion -el limitador, el
 * DRC del remuestreador, la escritura bloqueante de audio y el Present- y
 * ninguno sabe de los otros.  RetroArch lo dice en su propio codigo:
 *
 *   "Any combination can be live on the same frame, which is why VSync
 *    together with audio_sync stutters - two hardware clocks holding the same
 *    loop"
 *   "redundant double-pacing: two clocks holding one loop drift against each
 *    other and the slower one wins"
 *
 * Con la mascara, "quien freno" deja de deducirse de `late`/`blk`/`flip` y se
 * DECLARA.  La regla de lectura es una sola y es verificable en el log:
 * **en regimen tiene que haber exactamente UN bit levantado**.  Dos a la vez es
 * el defecto que se esta eliminando.
 *
 * `pace_fuentes()` es una funcion pura a proposito: entra el estado, sale la
 * mascara, sin leer nada global.  Asi se puede razonar sobre ella -y probarla-
 * sin montar un frame entero. */
#define PACE_NADA      0x00
#define PACE_VBLANK    0x01   /* el pacer espero a un vblank del hardware */
#define PACE_TIMER     0x02   /* el limitador por reloj de CPU durmio o giro */
#define PACE_AUDIO     0x04   /* la escritura de audio bloqueo (SYNC_TO_AUDIO) */
#define PACE_PRESENT   0x08   /* el Present bloqueo al vblank (>2 ms) */

static inline unsigned pace_fuentes(bool esperoVblank, bool durmioTimer,
                                    bool bloqueoAudio, bool bloqueoPresent)
{
    unsigned m = PACE_NADA;
    if (esperoVblank)   m |= PACE_VBLANK;
    if (durmioTimer)    m |= PACE_TIMER;
    if (bloqueoAudio)   m |= PACE_AUDIO;
    if (bloqueoPresent) m |= PACE_PRESENT;
    return m;
}

/* Cuantos bits hay levantados.  >1 es el sintoma de doble pacing. */
static inline int pace_cuantos(unsigned m)
{
    int n = 0;
    while (m) { n += (int)(m & 1u); m >>= 1; }
    return n;
}

static const char *FPS_FORMAT = "FPS: %.1f";
static const char *CPU_FORMAT = "CPU: %.0f%%";

class Sync {
public:
    Sync(int);
    ~Sync(){};

    // Ordenados por tamano para mejor alineamiento en PowerPC
	float fps;
    float frameDelay;    // 16.6f para 60fps
    float utilization;
    float g_actualFps;

    uint32_t g_lastFrameTick;
    uint32_t g_totalTimeSum; // Nueva: suma acumulada de los 10 frames
    int g_frameTimeIndex;
    int g_sync_last;

    uint32_t g_frameTimes[FPS_AVG_COUNT];

    char fpsText[64]; // Alineado a 16 o 32 bytes (mejor para la cache)
    char cpuText[64];

    // === CPU utilization via GetThreadTimes ===
    // Mide el % real de tiempo que el thread principal estuvo ejecutando
    // en el CPU (user + kernel) frente al wall time.  Distingue trabajo
    // real (busy-wait, dynarec) de espera bloqueante (SDL_Delay,
    // WriteBlocking, WaitForSingleObject), que es lo que la metrica
    // anterior basada en workTime/frameDelay no separaba.
    uint64_t cpu_prev_us;        // tiempo CPU del thread acumulado en ultimo sample (us)
    double   wall_prev_ms;       // wall time del ultimo sample (ms)
    bool     cpu_prev_valid;     // false hasta el primer sample real
    float    cpu_util_buffer[FPS_AVG_COUNT];  // ventana movil para suavizar
    int      cpu_util_index;     // indice circular en cpu_util_buffer
    int      cpu_util_filled;    // cuantos slots tienen muestra valida (cap FPS_AVG_COUNT)

    // === Framepacing =========================================================
    // El planificador de deadlines trabaja sobre un ancla + contador de frames
    // (ancla + n*frameTarget) en lugar de acumular sumas, para que no se
    // acumule error de redondeo ni el sesgo del `float` frameDelay.
    double   frameTarget;      // periodo objetivo en ms (double, sin sesgo)
    double   paceAnchor;       // instante base del planificador (ms)
    double   lastPresent;      // timestamp del ultimo Present (ms)
    uint32_t paceFrame;        // frames emitidos desde paceAnchor

    // === Margen de sleep ADAPTATIVO (Fase 2) =================================
    // SDL_Delay solo garantiza resolucion de milisegundo, asi que el limitador
    // duerme el grueso y clava el instante con una espera activa corta.  El
    // margen que se reserva para esa espera activa era una constante de 2 ms;
    // ahora se MIDE el error real del sleep y el margen se ajusta a el.
    //
    // Medido en Salvia (Xbox 360 y Windows): Sleep(8) tarda 8.00 ms con ~30 us
    // de error, asi que el margen converge a las decimas y la espera activa
    // baja de ~2.9 ms a ~0.7 ms por frame (el resto es el residuo fraccionario
    // que SDL_Delay no puede dormir).  Si el reloj de la plataforma fuese peor,
    // el margen crece solo y el pacing sigue exacto a cambio de mas CPU, en
    // lugar de llegar tarde a cada frame como pasaba con el margen fijo.
    double   sleepOverAvg;     // EMA del exceso del sleep sobre lo pedido (ms)
    double   sleepOverDev;     // EMA de la desviacion absoluta del exceso (ms)
    double   sleepMargin;      // margen reservado a la espera activa (ms)

    // === Rejilla del tick del scheduler ======================================
    // SDL_Delay no despierta cuando se le pide, sino en el siguiente punto de la
    // rejilla del timer del sistema (1 ms con timeBeginPeriod(1) activo).  Sin
    // conocer esa rejilla hay que reservar un tick ENTERO de margen, porque el
    // despertar puede caer en cualquier punto de [pedido, pedido+tick).
    //
    // Conociendo la FASE de la rejilla se puede pedir el sueno que despierta
    // justo en el ultimo punto ANTES del deadline, y entonces la espera activa
    // solo cubre el hueco rejilla->deadline: 0.5 ms de media en vez de 1.5 ms
    // medidos con el margen estadistico.
    //
    // La fase se mide en cada despertar (cada despertar ES un punto de la
    // rejilla) y el modelo se valida por congruencia.  Si los despertares dejan
    // de caer en la rejilla -- reloj mas basto, o al contrario un sueno exacto
    // sin rejilla -- se desengancha y vuelve al margen estadistico, que es
    // correcto en los dos casos aunque cueste mas CPU.
    double   tickGridMs;       // periodo de la rejilla (ms)
    double   gridPhase;        // fase de la rejilla en el reloj de getTicks (ms)
    int      gridGood;         // aciertos de congruencia consecutivos
    int      gridMiss;         // fallos consecutivos
    bool     gridValid;        // true = el sueno se alinea a la rejilla

    // Duracion del flip (instrumentacion): dice si el Present bloquea hasta el
    // vblank o si la cola de flips del driver le deja adelantarse un frame.
    double   flipSum;
    double   flipMax;
    double   flipLast;           // duracion del ULTIMO flip (la mascara de pacing)
    int      flipCount;
    int      flipBlockedCount;   // flips que tardaron mas de 2 ms

    // Coste del limitador en la ventana (ms acumulados)
    double   sleepSum;
    double   spinSum;

    // Estadisticas de la ventana en curso (present-to-present, en ms)
    double   paceSum;
    double   paceSumSq;
    double   paceMin;
    double   paceMax;
    double   paceLogLast;
    double   paceSummaryLast;  // resumen [PACE] que SI sale sin FRAMEPACE_LOG
    int      paceCount;        // presentaciones validas en la ventana
    int      paceDrops;        // presentaciones que ocuparon >=2 periodos
    int      paceLate;         // frames que llegaron tarde al deadline

    /* --- Vsync adaptativo ---------------------------------------------------
     *
     * POR QUE.  El Present se hace ANTES del limitador (para latencia minima),
     * asi que cuando el core no llega a tiempo el limitador ya no espera -su
     * `diff` es negativo y sale sin dormir- y el bloqueo al vblank se lo come
     * entero el Present.  Medido en Salvia y anotado en note_flip(): 0,86 ms
     * cuando el limitador ya ha esperado, 11,7 ms si se le deja bloquear, y ese
     * bloqueo es BUSY-WAIT, CPU al 100%.
     *
     * Eso convierte un core lento en un core mas lento, porque el Present y el
     * core comparten hilo (HW0).  Medido con Xenon64/Majora's Mask en
     * sincrono: `[CORE] dentro=696ms fuera=304ms`, o sea 6,6 ms de cada frame
     * de 21,75 quemados fuera del core, con el emulador ya al 77% del tiempo
     * real (46 VI/s de 60) y el audio reportando 75%.
     *
     * Solo se pisa con cores que YA no llegan: con uno rapido el limitador
     * espera primero y el Present vuelve en 0,86 ms.  Asi que el arreglo es
     * apagar el vsync cuando se confirma que no llegamos, y volver a
     * encenderlo solo si hay holgura de verdad.
     *
     * ** COMO SE CONMUTA, y esto costo una pantalla negra.  NO con
     * `SDL_XBOX_SetVSync`: esa hace `IDirect3DDevice9_Reset`, y un Reset
     * invalida todos los recursos D3DPOOL_DEFAULT del device.  El device es
     * UNO y compartido con el core libretro: Xenon64 renderiza a una textura
     * D3D creada ahi, no se entera del Reset y se queda con punteros muertos.
     * Sintoma medido: pantalla completamente negra en cuanto conmuta.
     *
     * La via buena es `SDL_XBOX_SetPresentInterval`, que en Xbox 360 usa el
     * render state D3DRS_PRESENTINTERVAL (d3d9types.h:624) y no toca el device.
     * Al ser gratis, la decision pasa a ser POR FRAME y el signo de `diff` la
     * resuelve entero, sin umbrales ni anti-oscilacion. */
    bool     vsyncOn;          // espejo del estado real del device
    bool     vsyncPermitido;   // false = manda otro (avance rapido)
    double   dtAvg;            // media movil del periodo conseguido (ms): la SENAL
    double   vsyncGuarda;      // instante hasta el que no se reevalua (ms)

    /* Contadores de vblank del HARDWARE, sobre la MISMA ventana que `paceLate`,
     * para que el contraste se lea en la misma linea.  `vblSum/paceCount` es la
     * fraccion real de frames que resbalan un vblank; `paceLate` es distancia a
     * un deadline de CPU y sale ~70x mas pesimista. */
    unsigned int vblPrev;      // ultimo contador visto (0 = sin muestra aun)
    unsigned int vblSum;       // vblanks transcurridos en la ventana
    unsigned int vblMissPrev;
    unsigned int vblMissSum;

    /* --- Pacer por vblanks (Fase 1) -----------------------------------------
     * El objetivo del frame `n` es el vblank `vblAnchor + round(n * vblPorFrame)`.
     * `vblAnchor == 0` significa "sin anclar todavia". */
    unsigned int vblAnchor;
    unsigned int vblFrame;

    /* Modo de sync del frame anterior (-1 = ninguno aun).  Al cambiar se
     * re-anclan los dos planificadores y se pone el present inmediato del
     * "Video adaptativo" (SDL_XBOX_SetAdaptiveVSync): solo en
     * SYNC_TO_VIDEO_ADAPT.  "Video" (SYNC_TO_VIDEO) es el de antes de
     * 9f9f64bc: limitador por reloj de CPU y vsync sin present inmediato. */
    int    syncTypeAnt;

    bool   vblPacerActivo();   // hay reloj de presentacion Y ratio utilizable
    double vblPorFrame();      // refresco del panel / fps del core

    /* --- Fase 4: quien freno ------------------------------------------------
     * `paceMask` es la del ultimo frame; `paceDoble` cuenta los frames de la
     * ventana con MAS DE UNA fuente activa, que es el defecto a erradicar. */
    unsigned paceMask;
    int      paceDoble;
    unsigned paceMaskAcum;     // OR de toda la ventana: que fuentes se han visto

    void anota_pace(bool esperoVblank, bool durmioTimer, bool modoAudio);
    double   audioOcup;        // EMA de la ocupacion del anillo (%); <0 = sin dato
    int      vsyncCambios;     // conmutaciones, para el log
    double   vsyncLogLast;     // ultima traza, para no inundar el log

    void set_vsync_permitido(bool permitido);  // lo llama el avance rapido
    void update_vsync_adaptativo(double diff);
    void note_audio_ocupacion(unsigned pct);   // lo llama updateGame cada frame

    void init_fps_counter(float);
    void initAverages(uint32_t);
    void update_fps_counter(bool, uint32_t);
    void sample_cpu_utilization();  // llamado por updateFps cada frame
    double limit_fps(double&, int syncType, GameTicks &gameTicks);

    // Framepacing
    void note_flip(double durationMs);    // duracion del flip que acaba de salir
    void update_sleep_margin(double over); // aprende el error real de SDL_Delay
    void note_grid_wake(double actual, double predicted); // sigue la rejilla
    void note_present(double now);        // llamar justo DESPUES del flip
    void reset_pace(double now);          // re-ancla el planificador
    void reset_pace_stats();              // limpia la ventana de estadisticas
};