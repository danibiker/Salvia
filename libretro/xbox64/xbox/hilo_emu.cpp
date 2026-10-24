/*
 * xbox/hilo_emu.cpp  --  el emulador en su propio hilo.  El porque, el
 * diagrama y las tres desviaciones de mupen estan en hilo_emu.h.
 *
 * Solo existe con el grabador en su valor 3 (xenon/d3d9_grabador.h): sin la
 * cola de ordenes no hay forma de que otro hilo dibuje.  Con cualquier otro
 * valor, `hilo_emu_crear` devuelve 0 y el core juega sin hilo.
 *
 * C++03 (VS2010).
 */

#include <xtl.h>
#include <ppcintrinsics.h>      /* __lwsync, __mftb: modelo de memoria debil */
#include <stdio.h>
#include <string.h>
#include <float.h>              /* _controlfp_s: la sonda del estado de FP */

#include "hilo_emu.h"
#include "../source/xenon64_diag.h"   /* XBOX64_TRAZAS */
#include "hilo_lista.h"          /* [etapa 5] la lista en paralelo, en HW3 */
#include "../xenon/d3d9_grabador.h"
#include "../xenon/gx_d3d9.h"
#include "../xenon/video_handoff.h"

extern "C" {
#include "plataforma.h"
}

#if D3D_GRABADOR >= 3

/* ====================================================================== */
/* Parametros                                                             */
/* ====================================================================== */

/* Swaps pendientes antes de frenar al emulador: el de mupen,
 * `FunctionWrapper::MAX_SWAP` (ThreadedOpenGl/opengl_Wrapper.h:57).  Con 2,
 * el emulador puede ir hasta ~3 frames por delante de lo que se presenta.
 * Ese colchon es el que reparte la carga; mupen avisa de que sube el input
 * lag (libretro_core_options.h:181). */
#define HILO_MAX_SWAP  2

/* [Banjo-Kazooie, logs (2) 2026-10-24] Con la copia del color SINCRONA
 * (gxd3d_ccopia_sinc_activa) el colchon es lo que esperaba cada copia: 3
 * frames sin presentar = 49 ms por copia, 15 img/s.  Sin colchon el emulador
 * no empieza un VI hasta que se presento el anterior, y `retro_run` esta
 * consumiendo ESE frame cuando la lista pide la copia.  Para quitarlo: esta
 * linea y el `maxSwap` del ritmo. */
#define HILO_MAX_SWAP_CCOPIA  0

/* ★ 1 MB de pila.  La emulacion corria hasta ahora en el hilo principal de
 * Salvia, que tiene 512 KB (`StackCommitSize` 524288, Salvia.vcxproj); los
 * dos plugins, el dynarec y el interprete van por aqui.  El doble, para no
 * descubrir el limite con un cuelgue. */
#define HILO_PILA      (1024u * 1024u)

/* HW2 = nucleo 1, hilo 0.  El hilo principal de Salvia -- `retro_run`, el
 * consumidor -- va en HW1 (`CPU_THREAD 1`, src/const/constant.h:29), o sea
 * en el nucleo 0: productor y consumidor en nucleos DISTINTOS.  HW4 y HW5
 * son de Salvia (IO y audio SDL).  Es el HW que usaba el hilo grafico
 * antiguo.  `[HILO] hw=` lo comprueba en ejecucion. */
#define HILO_HW        2

/* ====================================================================== */
/* Estado.  Cada variable tiene UN escritor; se anota cual.               */
/* ====================================================================== */

static HANDLE s_hilo     = NULL;
static DWORD  s_hiloId   = 0;
static HANDLE s_evOrden  = NULL;   /* control -> emulador: cambio de peticion */
static HANDLE s_evEstado = NULL;   /* emulador -> control: cambio de estado   */
static HANDLE s_evSwap   = NULL;   /* consumidor -> emulador: un swap menos   */

static volatile int  s_salir        = 0;  /* control:   terminar el hilo        */
static volatile int  s_quiereCorrer = 0;  /* control:   emular                  */
static volatile int  s_pausa        = 0;  /* control:   aparcar en el fin de VI */
static volatile int  s_corriendo    = 0;  /* emulador:  dentro del bucle        */
static volatile int  s_aparcado     = 0;  /* emulador:  parado en punto seguro  */

/* Swaps encolados y todavia no "presentados": el `m_swapBuffersQueued` de
 * mupen.  Lo suben el emulador y lo bajan el consumidor: por eso va con
 * Interlocked*.  (En mupen es un `int` sin atomico modificado por los dos
 * lados; aqui no se copia eso.) */
static volatile LONG s_swaps = 0;

/* El consumidor ya ejecuto un SWAP y tiene que restarlo AL EMPEZAR el
 * `retro_run` siguiente -- el `m_swapBuffersCallback` que en mupen corre al
 * reanudar el cothread.  Solo lo toca HW1. */
static int s_restar = 0;

/* -- Medidas.  Las escribe un hilo y las lee y pone a cero el informe: una
 *    cuenta perdida en el reinicio no importa. ------------------------- */
#if XBOX64_TRAZAS   /* solo las lee el informe ([HILO], [HILO2], [UTIL emu]) */
static volatile unsigned int s_stHw       = 0;  /* HW del emulador          */
static volatile unsigned int s_stVi       = 0;  /* VI emulados              */
static volatile unsigned int s_stTrabTb   = 0;  /* trabajo del emulador     */
static volatile unsigned int s_stTrabMax  = 0;
static volatile unsigned int s_stRitmoTb  = 0;  /* espera por MAX_SWAP      */
static volatile unsigned int s_stRitmoN   = 0;
static volatile unsigned int s_stEspTb    = 0;  /* retro_run esperando SWAP */
static volatile unsigned int s_stEspMax   = 0;
static volatile unsigned int s_stConsTb   = 0;  /* retro_run ejecutando     */
static volatile unsigned int s_stConsMax  = 0;
static volatile unsigned int s_stSwapMax  = 0;  /* pico de swaps pendientes */

/* [UTIL emu]: el trabajo de CADA VI, en % de 16,667 ms.  Anillo de un
 * escritor (el emulador) y un lector (el informe). */
#define UEMU_N  256u
static unsigned short        s_uEmu[UEMU_N];
static volatile unsigned int s_uEmuW = 0;
static unsigned int          s_uEmuR = 0;
#endif /* XBOX64_TRAZAS */

/* ★ LA SONDA DEL ENTORNO DEL HILO (log 21: Majora con glN64 cae al segundo
 * de arrancar DENTRO DEL DYNAREC, en `RecompCache_Free` con un puntero basura,
 * y en el log 18 -- sin hilo -- corrio 112 s).
 *
 * El emulador hace exactamente lo mismo en su hilo que en el del frontend,
 * salvo lo que depende del ENTORNO del hilo, y la primera candidata es la
 * palabra de control de coma flotante (redondeo, desnormales, mascaras de
 * excepcion): un hilo nuevo nace con la del sistema, no con la que el hilo
 * principal tenga puesta.  Si salen distintas, el juego calcula otra cosa en
 * coma flotante y puede tomar caminos que sin hilo no toma.
 *
 * Solo lee (`_controlfp_s` con mascara 0) y se imprime una vez por partida,
 * en `hilo_emu_arrancar`.  Iguales = esta via queda descartada. */
#if XBOX64_TRAZAS
static volatile unsigned int s_cwHilo = 0;
#endif

/* ★ TRAZAS DE PASO, una vez por partida (log 26: el arranque se colgo sin
 * decir donde).  1 = el emulador arranco, 2 = primer VI hecho, 4 = primer
 * SWAP consumido en `retro_run`, 8 = primer `retro_run` que vuelve. */
#if XBOX64_TRAZAS
static volatile unsigned int s_trazas = 0;

static void traza(unsigned int bit, const char *txt)
{
    if (s_trazas & bit)
        return;
    s_trazas |= bit;
    xbox64_log_line(txt);
}
#else
#define traza(bit, txt) ((void)0)   /* version publica: ni la llamada */
#endif

/* ★ [revision 2026-10-01] EL VIGIA DE ESPERAS LARGAS DE HW1.  Las esperas de
 * `retro_run` al emulador (el SWAP, la parada, la pausa, el arranque) no
 * tienen tope, y si el emulador o el hilo de la lista se atascan la consola
 * se queda colgada sin decir donde.  Esto no cambia ninguna espera: a los 2 s
 * escribe UNA linea con el estado de los dos hilos, y otra si se resuelve.
 * La espera del RITMO (MAX_SWAP) NO lleva vigia a proposito: con el menu de
 * Salvia abierto `retro_run` no se llama y es normal que dure lo que sea. */
#if XBOX64_TRAZAS   /* vigia y medidas: solo escriben en el log */
#define HILO_VIGIA_TB (2ULL * 50000000ULL)   /* 2 s de time base (50 MHz) */

/* [log 39] Se REPITE cada 2 s (hasta 10 veces) con el estado de la CPU
 * emulada: en BattleTanx la primera linea dijo "el emulador no acaba su
 * primer VI" y nada mas.  Dos muestras seguidas del PC distinguen un juego
 * que espera algo en un bucle de un hilo parado en codigo nuestro. */
#define HILO_VIGIA_MAX 10

static void vigia(unsigned long long t0, int *avisado, const char *donde)
{
    char l[224];
    if (*avisado >= HILO_VIGIA_MAX ||
        (__mftb() - t0) < HILO_VIGIA_TB * (unsigned long long)(*avisado + 1))
        return;
    *avisado = *avisado + 1;
    _snprintf(l, sizeof(l),
              "[HILO] ★ ESPERA LARGA: %s lleva %d s | corriendo=%d aparcado=%d "
              "pausa=%d quiere=%d swaps=%d",
              donde, 2 * *avisado, s_corriendo, s_aparcado, s_pausa,
              s_quiereCorrer, (int)s_swaps);
    l[sizeof(l) - 1] = 0;
    xbox64_log_line(l);
    hilo_lista_traza_estado(donde);
    xbox64_traza_cpu();
}

static void vigia_fin(int avisado, const char *donde)
{
    char l[160];
    if (!avisado)
        return;
    _snprintf(l, sizeof(l), "[HILO] espera larga resuelta: %s", donde);
    l[sizeof(l) - 1] = 0;
    xbox64_log_line(l);
}

static void anota_trabajo(unsigned long long dt)
{
    const unsigned int t = (unsigned int)dt;
    const unsigned long long us = dt / 50ULL;     /* time base a 50 MHz */
    s_stVi++;
    s_stTrabTb += t;
    if (t > s_stTrabMax) s_stTrabMax = t;
    s_uEmu[s_uEmuW & (UEMU_N - 1u)] =
        (unsigned short)((us * 100ULL + 16666ULL) / 16667ULL);
    __lwsync();
    s_uEmuW = s_uEmuW + 1u;
}
#endif /* XBOX64_TRAZAS */

static void marca_aparcado(int si)
{
    __lwsync();
    s_aparcado = si;
    __lwsync();
    SetEvent(s_evEstado);
}

/* ====================================================================== */
/* El hilo del emulador                                                   */
/* ====================================================================== */

/* ★ EL PUNTO SEGURO: el fin de un VI, con el lote del shim ya emitido
 * (`gxd3d_frame_end`).  Es el mismo sitio donde hasta la etapa 3 volvia
 * `retro_run`, asi que todo lo que el frontend hacia "entre dos
 * retro_run" -- savestates, opciones -- sigue ocurriendo exactamente aqui. */
static void punto_seguro(void)
{
    /* El juego pidio apagar: dejar de producir.  `retro_run` lo vera y se
     * lo dira al frontend; este descargara y `hilo_emu_parar` nos soltara. */
    if (xbox64_quiere_apagar())
    {
        /* [etapa 5] Aparcado = NADIE graba: antes, que el hilo de la lista
         * acabe lo que tiene (el consumidor de `retro_run` sigue bombeando
         * mientras tanto, asi que no se atasca). */
        hilo_lista_vaciar();
        marca_aparcado(1);
        while (s_quiereCorrer && !s_salir)
            WaitForSingleObject(s_evOrden, 100);
        marca_aparcado(0);
        return;
    }

    if (s_pausa)
    {
        hilo_lista_vaciar();     /* [etapa 5] ver arriba */
        marca_aparcado(1);
        while (s_pausa && s_quiereCorrer && !s_salir)
            WaitForSingleObject(s_evOrden, 100);
        marca_aparcado(0);
    }
}

/* [arranque en frio, 2026-10-13] SONDA TEMPORAL: 1 en cuanto el hilo nuevo
 * ejecuta su primera linea.  Sin escribir en el log desde aqui: el hilo
 * principal la lee y la escribe (ver hilo_emu_crear).  Retirar al cerrar. */
#if XBOX64_TRAZAS
static volatile LONG s_hiloVivo = 0;
#endif

static DWORD WINAPI hilo_proc(LPVOID)
{
#if XBOX64_TRAZAS
    InterlockedExchange(&s_hiloVivo, 1);
#endif
    for (;;)
    {
        /* Ocioso: entre juegos, o con el modo hilo apagado. */
        while (!s_quiereCorrer && !s_salir)
            WaitForSingleObject(s_evOrden, 100);
        if (s_salir)
            break;

        /* La palabra de control de coma flotante CON LA QUE ARRANCA este
         * hilo.  Ver `s_cwHilo`. */
#if XBOX64_TRAZAS
        {
            unsigned int cw = 0;
            _controlfp_s(&cw, 0, 0);
            s_cwHilo = cw;
        }
#endif

        __lwsync();
        s_corriendo = 1;
        __lwsync();
        SetEvent(s_evEstado);

        /* [etapa 5] Con la lista en paralelo, las marcas de frame del shim y
         * el SWAP las graba el hilo de la lista (el unico productor de la
         * cola D3D), en orden con las listas: aqui solo se encolan.  El
         * `frame_begin` de las vueltas siguientes va con el SWAP. */
        traza(1, "[HILO] traza: el emulador arranco en su hilo");
        if (hilo_lista_activo())
            hilo_lista_frame_begin();

        while (s_quiereCorrer && !s_salir)
        {
            {
                const unsigned long long t0 = __mftb();
                unsigned long long dt;
                if (hilo_lista_activo())
                {
                    xbox64_run_frame();
                    hilo_lista_frame_end();
                }
                else
                {
                    gxd3d_frame_begin();
                    xbox64_run_frame();
                    gxd3d_frame_end();
                }
                dt = __mftb() - t0;
                traza(2, "[HILO] traza: primer VI emulado");
#if XBOX64_TRAZAS
                anota_trabajo(dt);
#endif
                /* Funcional: alimenta el Count/us del plazo del DP. */
                hilo_lista_anota_vi((unsigned int)(dt / 50ULL));
#if XBOX64_TRAZAS
                s_stHw = (unsigned int)GetCurrentProcessorNumber();
#endif
            }

            punto_seguro();
            if (!s_quiereCorrer || s_salir)
                break;

            /* ★★ EL RITMO: `WaitForSwapBuffersQueued` de mupen
             * (opengl_Wrapper.cpp:1587-1595).  Con mas de MAX_SWAP frames
             * sin presentar, esperar a que `retro_run` consuma uno.  Es lo
             * UNICO que frena al emulador: el VI no, igual que en mupen
             * (`retro_return` es un no-op en el modo con hilo). */
            const LONG maxSwap = gxd3d_ccopia_sinc_activa() ? HILO_MAX_SWAP_CCOPIA
                                                            : HILO_MAX_SWAP;
            if (s_swaps > maxSwap)
            {
#if XBOX64_TRAZAS
                const unsigned long long tr = __mftb();
#endif
                while (s_swaps > maxSwap && s_quiereCorrer && !s_salir)
                {
                    if (s_pausa)
                        punto_seguro();
                    else
                        WaitForSingleObject(s_evSwap, 5);
                }
#if XBOX64_TRAZAS
                s_stRitmoTb += (unsigned int)(__mftb() - tr);
                s_stRitmoN++;
#endif
            }
            if (!s_quiereCorrer || s_salir)
                break;

#if XBOX64_TRAZAS
            {
                const LONG n = InterlockedIncrement(&s_swaps);
                if ((unsigned int)n > s_stSwapMax) s_stSwapMax = (unsigned int)n;
            }
#else
            InterlockedIncrement(&s_swaps);
#endif
            if (hilo_lista_activo())
                hilo_lista_swap_begin();
            else
                d3dq_mete_swap();
        }

        /* [etapa 5] Parado = nadie graba: que el hilo de la lista acabe lo
         * suyo ANTES de decirlo.  `hilo_emu_parar` bombea hasta ver
         * `s_corriendo` a 0 y despues pasa la cola al modo sin hilo. */
        hilo_lista_vaciar();

        __lwsync();
        s_corriendo = 0;
        __lwsync();
        SetEvent(s_evEstado);
    }
    return 0;
}

/* ====================================================================== */
/* El consumidor (HW1)                                                    */
/* ====================================================================== */

static void restar_un_swap(void)
{
    InterlockedDecrement(&s_swaps);
    SetEvent(s_evSwap);
}

/* El emulador no va a mandar nada mas: parado, aparcado en un punto seguro,
 * o el juego pidio apagar.  Se lee ANTES de volver a mirar el anillo, con
 * la barrera en medio: el emulador publica sus ordenes y DESPUES su estado,
 * asi que visto el estado, lo que haya publicado antes ya es visible. */
static int emulador_quieto(void)
{
    const int q = (!s_corriendo || s_aparcado) ? 1 : 0;
    __lwsync();
    return q;
}

void hilo_emu_consumir_frame(void)
{
#if XBOX64_TRAZAS
    const unsigned long long t0 = __mftb();
    unsigned long long tesp = 0;
    int avisado = 0;             /* vigia de esperas largas */
#endif
    int r;

    /* El swap del frame que se presento en el `retro_run` anterior: ahora
     * que el frontend ya lo presento, deja de contar.  Ver `s_restar`. */
    if (s_restar)
    {
        restar_un_swap();
        s_restar = 0;
    }

    /* [etapa 5] El almacen de texturas: el dueno del device crea por
     * adelantado las que el juego va a pedir, para que el productor no
     * espere en el buzon.  Ver `d3dq_almacen_toma`.  [Logs Salvia (15)]
     * 64: el almacen ahora es de 24 por clase (d3dq.cpp). */
    d3dq_almacen_repone(64u);

    /* Cuanto va por delante el emulador, en ordenes: es `maxcola` en la
     * linea [D3DQ3].  Con el modo hilo `d3dq_drena` no corre, asi que el
     * pico se toma aqui, al empezar cada pasada. */
#if XBOX64_TRAZAS
    {
        const unsigned int pend = g_d3dqCabeza - g_d3dqCola;
        if (pend > d3dq_dbg_maxcola) d3dq_dbg_maxcola = pend;
    }
#endif

    for (;;)
    {
        d3dq_atiende_buzon();
        r = d3dq_consume_una();
        if (r == 2)
        {
            s_restar = 1;
            traza(4, "[HILO] traza: primer SWAP consumido en retro_run");
            break;
        }
        if (r == 1)
            continue;

        /* Anillo vacio.  Si el emulador no va a mandar este SWAP, no se
         * espera: se ejecuta lo que pudo publicar entre la lectura del
         * anillo y la de su estado, y fuera.  Sin SWAP no hay nada que
         * restar. */
        if (emulador_quieto())
        {
            while ((r = d3dq_consume_una()) == 1)
                ;
            if (r == 2)
                s_restar = 1;
            break;
        }

        {
#if XBOX64_TRAZAS
            const unsigned long long tw = __mftb();
#endif
            /* [log 31] Tiempo muerto del dueno del device: reponer el almacen
             * de texturas aqui tambien, poco cada vez. */
            d3dq_almacen_repone(16u);
            d3dq_espera_trabajo(5);
#if XBOX64_TRAZAS
            tesp += __mftb() - tw;
            vigia(t0, &avisado, "retro_run esperando el SWAP");
#endif
        }
    }
#if XBOX64_TRAZAS
    vigia_fin(avisado, "retro_run esperando el SWAP");

    {
        const unsigned int esp  = (unsigned int)tesp;
        const unsigned int cons = (unsigned int)((__mftb() - t0) - tesp);
        s_stEspTb += esp;
        if (esp > s_stEspMax) s_stEspMax = esp;
        s_stConsTb += cons;
        if (cons > s_stConsMax) s_stConsMax = cons;
    }
#endif
    traza(8, "[HILO] traza: primer retro_run con hilo vuelve al frontend");
}

/* ★ EL BOMBEO: consumir mientras se espera a que el emulador llegue a un
 * estado.  Hace falta porque el emulador puede estar esperando AL
 * CONSUMIDOR -- el ritmo de MAX_SWAP, una orden sincrona, el pool o el
 * anillo llenos --, y el consumidor es este mismo hilo.  Es el bucle de
 * `co_switch(game_thread)` de `retro_serialize` y `retro_unload_game` en
 * mupen (libretro.c:2008-2041, 2141-2192).
 *
 * Se ejecuta como una pasada normal -- render target del core, vertex
 * buffer, bind --, porque las ordenes que se consumen aqui DIBUJAN.  Los
 * SWAPs se restan en el acto: no hay frame que presentar.
 *
 * Al salir el anillo queda VACIO: el emulador ya no graba. */
#define BOMBEO_PAUSA  1
#define BOMBEO_PARADA 2

static int bombeo_listo(int modo)
{
    int listo;
    if (modo == BOMBEO_PARADA)
        listo = !s_corriendo;
    else
        listo = (s_aparcado || !s_corriendo);
    __lwsync();
    return listo;
}

static void bombear(int modo)
{
    int r;
#if XBOX64_TRAZAS
    const unsigned long long t0 = __mftb();
    int avisado = 0;             /* vigia de esperas largas */
    const char *donde = (modo == BOMBEO_PARADA) ? "la parada del emulador"
                                                : "la pausa del emulador";
#endif

    if (s_restar)
    {
        restar_un_swap();
        s_restar = 0;
    }

    xenon64_video_frame_begin();
    gxd3d_consumidor_empieza();
#if D3DQ_BIND
    d3dq_bind();
#endif

    for (;;)
    {
        d3dq_atiende_buzon();
        r = d3dq_consume_una();
        if (r == 2) { restar_un_swap(); continue; }
        if (r == 1) continue;
        if (bombeo_listo(modo))
            break;
        d3dq_espera_trabajo(2);
#if XBOX64_TRAZAS
        vigia(t0, &avisado, donde);
#endif
    }
#if XBOX64_TRAZAS
    vigia_fin(avisado, donde);
#endif

    /* Lo que quede: el emulador esta parado o aparcado y ya no graba. */
    while ((r = d3dq_consume_una()) != 0)
        if (r == 2) restar_un_swap();
    d3dq_atiende_buzon();

    gxd3d_consumidor_acaba();
    xenon64_video_frame_end();
}

/* ====================================================================== */
/* Control (HW1)                                                          */
/* ====================================================================== */

int hilo_emu_crear(void)
{
    /* [log 36] Aparcado por un `retro_deinit` anterior: se reutiliza.  Ver
     * la nota de `retro_deinit` en libretro_core.cpp. */
    if (s_hilo)
    {
#if XBOX64_TRAZAS
        xbox64_log_line("[HILO] traza: reutilizando el hilo del emulador (aparcado)");
#endif
        return 1;
    }

    /* [log 36] Una traza por llamada al sistema: el log se corto aqui dentro
     * sin decir en cual.  Salen una vez por proceso. */
#if XBOX64_TRAZAS
    xbox64_log_line("[HILO] traza: crear: eventos...");
#endif
    s_evOrden  = CreateEvent(NULL, FALSE, FALSE, NULL);   /* auto-reset */
    s_evEstado = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_evSwap   = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!s_evOrden || !s_evEstado || !s_evSwap)
        goto mal;

    s_salir = 0;
    s_quiereCorrer = 0;
    s_pausa = 0;
    s_corriendo = 0;
    s_aparcado = 0;
    s_swaps = 0;

#if XBOX64_TRAZAS
    xbox64_log_line("[HILO] traza: crear: CreateThread...");
#endif
    s_hilo = CreateThread(NULL, HILO_PILA, hilo_proc, NULL,
                          CREATE_SUSPENDED, &s_hiloId);
    if (!s_hilo)
        goto mal;

#if XBOX64_TRAZAS
    xbox64_log_line("[HILO] traza: crear: prioridad y HW...");
#endif
    SetThreadPriority(s_hilo, THREAD_PRIORITY_NORMAL);
    XSetThreadProcessor(s_hilo, HILO_HW);
#if XBOX64_TRAZAS
    xbox64_log_line("[HILO] traza: crear: ResumeThread...");
#endif
    ResumeThread(s_hilo);
    /* [arranque en frio, 2026-10-13] SONDA TEMPORAL: el cuelgue aleatorio del
     * primer juego tras arrancar Salvia corta el log justo despues de la
     * linea de arriba (Logs Salvia (15)).  Si esta sale, ResumeThread volvio
     * y el cuelgue esta despues; si no sale, se queda DENTRO.  `vivo` = el
     * hilo nuevo ya ejecuto su primera linea (puede salir 0 aunque vaya bien:
     * es una carrera; solo informa).  Retirar al cerrar. */
#if XBOX64_TRAZAS
    xbox64_log_line(s_hiloVivo ? "[HILO] traza: crear: ResumeThread hecho (vivo=1)"
                               : "[HILO] traza: crear: ResumeThread hecho (vivo=0)");
#endif
    return 1;

mal:
    if (s_evOrden)  { CloseHandle(s_evOrden);  s_evOrden  = NULL; }
    if (s_evEstado) { CloseHandle(s_evEstado); s_evEstado = NULL; }
    if (s_evSwap)   { CloseHandle(s_evSwap);   s_evSwap   = NULL; }
    return 0;
}

void hilo_emu_destruir(void)
{
    if (!s_hilo)
        return;

    hilo_emu_parar();

    s_salir = 1;
    __lwsync();
    SetEvent(s_evOrden);
    SetEvent(s_evSwap);
    WaitForSingleObject(s_hilo, 2000);

    CloseHandle(s_hilo);     s_hilo = NULL;  s_hiloId = 0;
    CloseHandle(s_evOrden);  s_evOrden  = NULL;
    CloseHandle(s_evEstado); s_evEstado = NULL;
    CloseHandle(s_evSwap);   s_evSwap   = NULL;
}

int hilo_emu_corriendo(void)
{
    return (s_quiereCorrer || s_corriendo) ? 1 : 0;
}

void hilo_emu_arrancar(void)
{
    if (!s_hilo || s_quiereCorrer)
        return;

    s_swaps    = 0;
    s_restar   = 0;
    s_pausa    = 0;
    s_aparcado = 0;
#if XBOX64_TRAZAS
    s_trazas   = 0;
    xbox64_log_line("[HILO] traza: arrancando el emulador...");
#endif

    /* ★ ANTES de soltar al emulador: la primera orden que grabe ya tiene
     * que ir al anillo, no ejecutarse en su hilo. */
    d3dq_modo_hilo(1);

    __lwsync();
    s_quiereCorrer = 1;
    SetEvent(s_evOrden);
    {
#if XBOX64_TRAZAS
        const unsigned long long t0 = __mftb();
        int avisado = 0;         /* vigia de esperas largas */
#endif
        while (!s_corriendo)
        {
            WaitForSingleObject(s_evEstado, 10);
#if XBOX64_TRAZAS
            vigia(t0, &avisado, "el arranque del emulador");
#endif
        }
#if XBOX64_TRAZAS
        vigia_fin(avisado, "el arranque del emulador");
#endif
    }

    /* La sonda del entorno: ver `s_cwHilo`. */
#if XBOX64_TRAZAS
    {
        char l[128];
        unsigned int cw = 0;
        __lwsync();
        _controlfp_s(&cw, 0, 0);
        _snprintf(l, sizeof(l),
                  "[HILO] control FP: principal=%08X hilo=%08X  %s",
                  cw, s_cwHilo, (cw == s_cwHilo) ? "IGUALES" : "★ DISTINTOS");
        l[sizeof(l) - 1] = 0;
        xbox64_log_line(l);
    }
#endif
}

void hilo_emu_parar(void)
{
    if (!s_hilo || (!s_quiereCorrer && !s_corriendo))
        return;

    s_quiereCorrer = 0;
    __lwsync();
    SetEvent(s_evOrden);
    SetEvent(s_evSwap);

    bombear(BOMBEO_PARADA);

    /* El emulador ya no graba: a partir de aqui, lo que se grabe (el cierre
     * del shim, la carga siguiente) lo vacia su propio hilo. */
    d3dq_modo_hilo(0);
    d3dq_drena();

    s_swaps    = 0;
    s_restar   = 0;
    s_pausa    = 0;
    s_aparcado = 0;
}

/* ★ Durante la pausa la cola vuelve al MODO SIN HILO (`g_d3dqVivo` a 0).
 * Lo que se haga con el emulador aparcado corre en ESTE hilo, y si grabase
 * una orden sincrona con el modo hilo puesto la mandaria al buzon y se
 * quedaria esperando a un consumidor que es el mismo: cuelgue.  Con 0 se
 * vacia en el acto, como en la etapa 3.  El anillo esta vacio (el bombeo lo
 * deja asi), asi que el orden no cambia. */
void hilo_emu_pausar(void)
{
    if (!s_corriendo)
        return;
    s_pausa = 1;
    __lwsync();
    SetEvent(s_evSwap);
    SetEvent(s_evOrden);
    bombear(BOMBEO_PAUSA);
    d3dq_modo_hilo(0);
}

void hilo_emu_reanudar(void)
{
    if (!s_pausa)
        return;
    d3dq_modo_hilo(1);
    __lwsync();
    s_pausa = 0;
    __lwsync();
    SetEvent(s_evOrden);
}

/* ====================================================================== */
/* El informe                                                             */
/* ====================================================================== */

/* ★ Como se lee, y el orden importa:
 *
 *   1. `vi` es el CONTADOR DE PASO.  Tiene que salir ~60: si sale 0, el
 *      hilo no esta emulando y el resto de la linea mide el vacio.  Y `hw`
 *      tiene que salir 2 (si no, `XSetThreadProcessor` no se aplico).
 *   2. `esp` (retro_run esperando el SWAP).  Es LA medida del reparto: si el
 *      emulador va por delante, `retro_run` encuentra el frame hecho y esto
 *      es ~0.  Si es alto, el emulador va por detras -- la media de trabajo
 *      por VI no cabe en 16,7 ms -- y ningun colchon lo arregla.
 *   3. `trab` (emulador por VI, media y maximo).  El maximo puede pasar de
 *      16,7 ms sin que se note, siempre que la media no.
 *   4. `ritmo` (emulador frenado por MAX_SWAP).  Alto es BUENO: significa
 *      que va sobrado y tiene que esperar.
 *   5. `sinc` (creaciones y su espera).  El punto debil conocido del modelo.
 *   6. `perdidos` tiene que salir ~0 y `siestas` no llegar a los miles:
 *      ver `d3dq_espera_trabajo`.
 *
 * Tiempos en us POR FRAME del frontend, salvo `trab` (por VI) y los
 * maximos, que son del peor caso suelto. */
void hilo_emu_informe(unsigned frames)
{
#if !XBOX64_TRAZAS
    (void)frames;   /* version publica: sin informe ni medidas */
#else
    char l[192];
    const unsigned nf = frames ? frames : 1u;
    const unsigned vi = s_stVi ? s_stVi : 1u;

    if (!s_corriendo && s_stVi == 0)
        return;

    _snprintf(l, sizeof(l),
              "[HILO] hw=%u vi=%u | esp=%u (max %u) cons=%u (max %u) | "
              "trab=%u/vi (max %u)",
              s_stHw, s_stVi,
              (s_stEspTb / 50u) / nf, s_stEspMax / 50u,
              (s_stConsTb / 50u) / nf, s_stConsMax / 50u,
              (s_stTrabTb / 50u) / vi, s_stTrabMax / 50u);
    l[sizeof(l) - 1] = 0;
    xbox64_log_line(l);

    _snprintf(l, sizeof(l),
              "[HILO2] ritmo=%u (%u/s) swaps max=%u | sinc=%u (%u us) "
              "alm=%u/%u (+%u) | pool llenos=%u siestas=%u perdidos=%u",
              (s_stRitmoTb / 50u) / nf, s_stRitmoN,
              s_stSwapMax,
              d3dq_dbg_sinc_n, d3dq_dbg_sinc_tb / 50u,
              d3dq_dbg_alm_toma, d3dq_dbg_alm_toma + d3dq_dbg_alm_falla,
              d3dq_dbg_alm_crea,
              d3dq_dbg_poolllenos, d3dq_dbg_siestas, d3dq_dbg_perdidos);
    l[sizeof(l) - 1] = 0;
    xbox64_log_line(l);

    /* [UTIL emu]: el trabajo de cada VI, como `[UTIL run]`.  Solo los
     * ultimos 64, que es lo que cabe en un segundo. */
    {
        unsigned int w = s_uEmuW, i, j, p;
        __lwsync();
        if (w - s_uEmuR > 64u)
            s_uEmuR = w - 64u;
        for (i = s_uEmuR; i != w; )
        {
            p = (unsigned)_snprintf(l, sizeof(l), "[UTIL emu] %2u:",
                                    i - s_uEmuR);
            for (j = 0; i != w && j < 20u; j++, i++)
                p += (unsigned)_snprintf(l + p, sizeof(l) - p, " %3u",
                                         (unsigned)s_uEmu[i & (UEMU_N - 1u)]);
            l[sizeof(l) - 1] = 0;
            xbox64_log_line(l);
        }
        s_uEmuR = w;
    }

    s_stVi = s_stTrabTb = s_stTrabMax = 0;
    s_stRitmoTb = s_stRitmoN = 0;
    s_stEspTb = s_stEspMax = 0;
    s_stConsTb = s_stConsMax = 0;
    s_stSwapMax = 0;
    d3dq_dbg_sinc_n = d3dq_dbg_sinc_tb = 0;
    d3dq_dbg_poolllenos = 0;
    d3dq_dbg_alm_toma = d3dq_dbg_alm_falla = d3dq_dbg_alm_crea = 0;
    d3dq_dbg_siestas = 0;
    d3dq_dbg_perdidos = 0;
#endif /* XBOX64_TRAZAS */
}

#else  /* D3D_GRABADOR < 3: sin cola no hay hilo.  El core juega sin el. */

int  hilo_emu_crear(void)               { return 0; }
void hilo_emu_destruir(void)            { }
void hilo_emu_arrancar(void)            { }
void hilo_emu_parar(void)               { }
void hilo_emu_pausar(void)              { }
void hilo_emu_reanudar(void)            { }
int  hilo_emu_corriendo(void)           { return 0; }
void hilo_emu_consumir_frame(void)      { }
void hilo_emu_informe(unsigned frames)  { (void)frames; }

#endif /* D3D_GRABADOR >= 3 */
