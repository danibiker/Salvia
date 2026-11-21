#pragma once

#include <const/constant.h>
#include <stdint.h>
#include <stddef.h>

#include <beans/structures.h>
#include <libretro/libretro.h>

class AudioRateControl;

//Only to set the number of frames to count on a buffer
#define FPS_AVG_COUNT 20
#define FPS_DESIRED 60
const uint32_t TIME_AVG_COUNT = FPS_AVG_COUNT * 1000;

static const char *FPS_FORMAT = "FPS: %.1f";
static const char *CPU_FORMAT = "CPU: %.0f%%";

/* ===========================================================================
 * Sync: TODA la sincronizacion del frontend.
 *
 * Regla de oro: UN SOLO RELOJ por modo y UNA SOLA ESPERA por frame, en
 * endFrame().  Nada mas en el bucle principal espera a proposito.
 *
 *   Modo              Reloj                          Vsync del Present  Audio
 *   ----------------  -----------------------------  -----------------  ---------
 *   Video             vblanks (pacer, espera          si                 DRC
 *                     bloqueante al evento del vblank)
 *   Video adaptativo  vblanks                         si + present       DRC
 *                                                     inmediato al 80%
 *   Audio             la escritura de audio           no                 bloqueante
 *                     bloqueante
 *   Ninguna           ninguno                         no                 DRC
 *   Avance rapido     vblanks, varios frames por      si                 descartado
 *                     cada uno que se presenta
 *   Menus             vblanks, un frame por vblank    si                 -
 *
 * PACER POR VBLANKS.  El frame n se presenta en el vblank
 * ancla + floor(n * r), con r = refresco / fps.  Si r esta a menos de
 * MAX_TIMING_SKEW de un entero (60,0988 en 60, 59,73 en 60) se redondea: el
 * juego va al ritmo del panel, sin saltos de un vblank cada pocos cientos de
 * frames, y el remuestreador de audio se corrige con el mismo factor
 * (AudioRateControl::setTimingSkew) para que el DRC no gaste su margen en
 * ello.  Si no (PAL a 50 en 60), r se queda fraccionario y sale la cadencia
 * 5:6.  Con r < 1 (avance rapido) varios frames comparten vblank: solo se
 * presenta y se espera el ultimo de cada grupo.
 *
 * Sin reloj de vblanks (Windows: D3D9 de escritorio no tiene callback de
 * vblank) se usa un limitador por reloj de CPU con el mismo esquema de
 * ancla + n * periodo.
 *
 * FRAMESKIP (cores con SET_AUDIO_BUFFER_STATUS_CALLBACK, p. ej. FBNeo).  El
 * core decide saltar frames mirando la ocupacion del anillo de audio, que se
 * le pasa en beginGameFrame().  Con el anillo por debajo de CATCHUP_PCT no se
 * espera nada: los frames saltados sirven para recuperar el tiempo perdido y
 * volver a llenar el anillo.  Los frames que el core no dibuja (data == NULL)
 * no se presentan, con un tope de MAX_DUPED_SIN_PRESENTAR seguidos.
 *
 * El modo pedido se lee de un int de la configuracion (el que cambian el
 * override del core y el menu) y se aplica en el frame siguiente.
 * =========================================================================== */
class Sync {
public:
    Sync(const int* modoConfig, AudioRateControl* audioRate);
    ~Sync(){};

    /* --- Configuracion ----------------------------------------------------- */
    void setCoreFps(double coreFps);              // al cargar y en SET_SYSTEM_AV_INFO
    void setFastForward(bool activo, float mult); // hotkey de avance rapido
    bool isFastForward() const { return ffActivo; }
    void setAudioStatusCallback(retro_audio_buffer_status_callback_t cb);
    void resetGame();                             // al cerrar el juego
    void invalidateDeviceState();                 // tras un Reset del device D3D

    /* --- Por frame, en este orden ------------------------------------------ */
    void beginGameFrame(size_t audioUsado, size_t audioCapacidad); // antes de retro_run
    void onVideoRefresh(bool duplicado);          // desde retro_video_refresh
    bool shouldPresent(bool enJuego);             // presentar o no este frame
    void endFrame(bool enJuego, GameTicks& gameTicks); // la UNICA espera

    /* --- Audio (los callbacks del core) ------------------------------------ */
    bool audioBlocking() const { return modo == SYNC_TO_AUDIO && !ffActivo; }
    bool audioDiscard() const  { return ffActivo && ffMult > 1.0f; }

    /* --- Contadores para el overlay ---------------------------------------- */
    float fps;
    float utilization;
    float g_actualFps;
    char  fpsText[64];
    char  cpuText[64];
    void update_fps_counter(bool updateFpsOverlay, uint32_t currentTick);
    void sample_cpu_utilization();

private:
    /* Lo que pide el usuario */
    const int*        modoConfig;   // int de la configuracion (puede ser NULL)
    int               modo;         // copia aplicada de *modoConfig
    double            coreFps;
    bool              ffActivo;
    float             ffMult;
    AudioRateControl* audioRate;

    /* Frameskip y frames duplicados */
    retro_audio_buffer_status_callback_t audioCb;
    bool              catchUp;      // anillo bajo con un core que hace frameskip
    bool              coreDuplicado;
    int               duplicadosSeguidos;

    /* Configuracion efectiva (configure) */
    bool              configurado;
    bool              cfgEnJuego;   // para que configuracion se calculo
    bool              conRitmo;     // false = sin espera (Audio, Ninguna, catch-up)
    bool              porVblanks;   // true = reloj de vblanks; false = reloj de CPU
    double            refresco;     // Hz del panel, 0 = desconocido
    double            r;            // vblanks (o refrescos) por frame
    double            periodoMs;    // periodo del frame en el camino de CPU
    int               intervalo;    // vsync del Present aplicado (-1 = sin aplicar)
    int               adaptativo;   // present inmediato aplicado (-1 = sin aplicar)
    double            skew;         // factor de tiempo aplicado al audio

    /* Planificador (ancla + frames) */
    bool              anclado;
    unsigned int      frame;        // frames desde el ancla
    unsigned int      vblAncla;
    double            cpuAncla;
    bool              finDeGrupo;   // este frame es el ultimo de su vblank

    /* Contadores de FPS / CPU */
    uint32_t g_lastFrameTick;
    int      g_frameTimeIndex;
    uint32_t g_frameTimes[FPS_AVG_COUNT];
    uint64_t cpu_prev_us;
    double   wall_prev_ms;
    bool     cpu_prev_valid;
    float    cpu_util_buffer[FPS_AVG_COUNT];
    int      cpu_util_index;
    int      cpu_util_filled;

#ifdef SYNC_STATS
    double   statIni;
    unsigned statFrames, statPresents, statTarde, statVblIni;
    double   statEsperaMs;
#endif

    void actualizaConfig(bool enJuego);
    void configure(bool enJuego);
    void reancla();
    void esperaVblank();
    void esperaCpu();
    bool calculaFinDeGrupo() const;
};
