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

static const char *FPS_FORMAT = "FPS: %.1f";
static const char *CPU_FORMAT = "CPU: %.0f%%";

class Sync {
public:
    Sync(int);
    ~Sync(){};

    // Ordenados por tamaño para mejor alineamiento en PowerPC
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
    int      flipCount;
    int      flipBlockedCount;   // flips que tardaron mas de 1 ms

    // Coste del limitador en la ventana (ms acumulados)
    double   sleepSum;
    double   spinSum;

    // Estadisticas de la ventana en curso (present-to-present, en ms)
    double   paceSum;
    double   paceSumSq;
    double   paceMin;
    double   paceMax;
    double   paceLogLast;
    int      paceCount;        // presentaciones validas en la ventana
    int      paceDrops;        // presentaciones que ocuparon >=2 periodos
    int      paceLate;         // frames que llegaron tarde al deadline

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