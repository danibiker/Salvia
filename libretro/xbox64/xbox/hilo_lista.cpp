/*
 * xbox/hilo_lista.cpp  --  la display list en paralelo con la CPU, en HW4.
 * El porque, el diagrama y lo que viene del hilo antiguo, en hilo_lista.h.
 *
 * Solo existe con el grabador en su valor 3 (xenon/d3d9_grabador.h): este
 * hilo GRABA ordenes D3D, no las ejecuta.  Con otro valor queda inactivo.
 *
 * C++03 (VS2010).
 */

#include <xtl.h>
#include <ppcintrinsics.h>      /* __lwsync, __mftb: modelo de memoria debil */
#include <stdio.h>
#include <string.h>

#include "hilo_lista.h"
#include "../source/xenon64_diag.h"   /* XBOX64_TRAZAS */
#include "../xenon/d3d9_grabador.h"
#include "../xenon/gx_d3d9.h"
/* SP_DMEM, sp_register, vi_register, MI_register, MI_INTR_DP.  Trae sus
 * propias guardas `extern "C"`. */
#include "../source/gc_memory/memory.h"

extern "C" {
#include "plataforma.h"          /* xbox64_log_line */
void check_interupt(void);       /* source/r4300/interupt.c */
/* [Killer Instinct] Para `hilo_lista_dp_vence` (el DP espera al SP de su
 * lista).  Se declara aqui y no con interupt.h: esa cabecera define `Count`
 * como macro, y el XDK lo usa como identificador.  El tipo es el de
 * interupt.h:77. */
unsigned long *get_event(int type);
}
#define HL_EV_SP_INT 0x080

#if D3D_GRABADOR >= 3

/* ====================================================================== */
/* Parametros                                                             */
/* ====================================================================== */

/* HW4: nucleo 2 (IO, ociosa en partida, junto al audio de SDL que ya duerme
 * con Sleep(1)).  El usuario lo probo en HW3 -- hermano SMT del emulador en
 * HW2 -- y en HW4 va mejor (log 27): el emulador y la lista dejan de
 * repartirse un nucleo.  El consumidor de `retro_run` sigue en HW1. */
#define HL_HW          4
#define HL_PILA        (1024u * 1024u)   /* el plugin corria en los 512 KB
                                          * del hilo principal */

/* Escenas de DMEM en vuelo.  Con MAX_SWAP 2 el emulador va como mucho ~3 VI
 * por delante, y un VI trae una o dos listas. */
#define HL_ESCENAS     8u
/* Ordenes en vuelo: ~5-8 por VI (lista, updateScreen, fin de VI, SWAP,
 * cambios de VI). */
#define HL_RANURAS     256u
/* Resultado de DP por lista, indexado por numero de lista. */
#define HL_DP_ANILLO   256u

/* Las marcas de frame, que no son del plugin: van despues del enum publico. */
#define HL_FBEGIN      100
#define HL_FEND        101
#define HL_SWAPBEGIN   102

/* ====================================================================== */
/* Estado.  Cada variable tiene UN escritor; se anota cual.               */
/* ====================================================================== */

static HANDLE s_hilo     = NULL;
static DWORD  s_hiloId   = 0;
static HANDLE s_evTrabajo = NULL;   /* emulador -> hilo: hay ordenes     */
static HANDLE s_evHecho   = NULL;   /* hilo -> emulador: una orden menos */
static volatile int s_salir = 0;
static int s_activo = 0;            /* HW1, al cargar: el modo de la partida */

struct HlOrden
{
    unsigned int  que;
    unsigned int  esc;               /* escena, para HL_DLIST */
    unsigned int  dl;                /* HL_DLIST: direccion de la lista (OSTask) */
    unsigned int  firma;             /* HL_DLIST: firma de su cabeza al despachar */
    unsigned long vi[HL_VI_N];       /* la foto del VI */
};
static HlOrden               s_cola[HL_RANURAS];
static volatile unsigned int s_colaW = 0;   /* emulador */
static volatile unsigned int s_colaR = 0;   /* hilo     */

/* ★ LA SOMBRA DE LA DMEM Y SU DIRECCION NO SE MUEVE: `gfx_info.DMEM` apunta
 * aqui y el plugin se queda con el puntero en `initiateGFX`
 * (glN64.cpp:298).  El hilo vuelca la escena de cada lista justo antes de
 * ejecutarla.  Alineadas a linea de cache del Xenon. */
static __declspec(align(128)) unsigned char s_dmem[HILO_LISTA_DMEM_BYTES];
static __declspec(align(128)) unsigned char s_escena[HL_ESCENAS][HILO_LISTA_DMEM_BYTES];

/* Lo que el plugin ve como registros.  Solo las toca el hilo (y el plugin
 * desde el hilo). */
static unsigned long s_vi[HL_VI_N];
static unsigned long s_mi = 0;
static unsigned long s_sp = 0;

/* Listas: despachadas (emulador), ejecutadas (hilo), cosechadas (emulador).
 * Contadores libres, se comparan por resta. */
static volatile unsigned int s_dlW  = 0;
static volatile unsigned int s_dlR  = 0;
static unsigned int          s_resR = 0;
/* La orden que el hilo esta ejecutando (0 = ninguna).  Solo para el vigia de
 * esperas largas: dice DONDE se ha quedado. */
static volatile unsigned int s_ordenQue = 0;

static unsigned long          s_sem[HL_ESCENAS];     /* emulador: la semilla del SP */
static volatile unsigned long s_spOut[HL_ESCENAS];   /* hilo: el SP al acabar       */
static volatile unsigned char s_dpHecho[HL_DP_ANILLO]; /* hilo: llego a su fullsync */

/* La deuda de DP: la siguiente lista cuyo DP_INT falta por entregar.  La
 * deuda es `s_dlW - s_dpIdx`.  Solo el emulador. */
static unsigned int s_dpIdx = 0;
static int          s_tareaConLista = 0;   /* emulador: la tarea actual despacho */
static int          s_primera = 1;         /* emulador: la primera, sincrona     */

/* El DP de la lista de RDP sincrona. */
static volatile unsigned long s_rdpMi = 0;

/* El SP al acabar (opcion).  `s_spAlAcabar` lo pone HW1 al cargar; el resto
 * es del emulador.  `s_spHasta` = cuantas listas tienen que estar hechas
 * cuando venza el SP_INT pendiente; `s_spPend` = hay uno. */
static int          s_spAlAcabar = 0;
static int          s_tareaSp = 0;
static unsigned int s_spHasta = 0;
static int          s_spPend = 0;
/* [log 35] El YIELD.  `s_spCedida` = el SP_INT pendiente se entrego como
 * "cedido" (SIG1) y la lista `s_spCedHasta` sigue en el hilo: cuando el juego
 * relance la tarea grafica con OS_TASK_YIELDED, se vuelve a esperar a ELLA en
 * vez de recorrer la lista otra vez.  Solo el emulador. */
static int          s_spCedida = 0;
static unsigned int s_spCedHasta = 0;
static unsigned int s_stCede = 0, s_stReanuda = 0, s_stReanudaSin = 0;
/* [log 8] Cesiones con la lista ya acabada, y DP retenidos por una cesion.
 * Ver `hilo_lista_sp_ceder` y `hilo_lista_dp_vence`.  Solo el emulador. */
static unsigned int s_stCedeHecha = 0, s_stCedeTarde = 0, s_stDpRet = 0;
static unsigned int s_stDpSp = 0;   /* [log 9] DP retenidos tras el SP de su lista */
static int          s_dpRetenido = 0;

/* ★ [Cruis'n USA] LA FOTO DE LA RDRAM (opcion `xbox64_lista_sp` = "foto").
 * Con "al acabar" el SP_INT espera a la lista entera (15-50 ms en HW4) y el
 * juego, que va a 30, pierde su cadencia: log 12, `img` 18-29 por segundo en
 * carrera.  Con "foto" el hilo COPIA la RDRAM entera al empezar la lista, el
 * SP_INT sale en cuanto acaba la copia, y el plugin dibuja desde la copia: lo
 * que el juego reescriba tras su SP_INT ya no le llega a la lista.
 *   s_foto    la copia; se reserva la primera vez que se pide, para siempre
 *   s_fotoR   listas ya fotografiadas (hilo).  El SP_INT de la lista N vence
 *             con `s_fotoR >= N`, como el de "al acabar" con `s_dlR`.
 * Lo que el plugin ESCRIBA en la RDRAM durante la lista va a la copia y el
 * juego no lo ve: solo vale para juegos que no lo necesiten. */
static unsigned char         *s_foto = NULL;
static volatile unsigned int  s_fotoR = 0;
/* ★ LA COPIA ES `XMemCpy` (xbox.h:875), MEDIDO (log 21, Cruis'n con glN64,
 * 137 s con los tres metodos por turnos en las mismas escenas):
 *     memcpy            copia 5,6 ms (max 7,0)   lista detras 16,0 ms
 *     XMemCpy           copia 3,5 ms (max 5,0)   lista detras 16,3 ms
 *     XMemCpyStreaming  copia 4,6 ms (max 6,0)   lista detras 16,2 ms
 * La lista que dibuja desde la copia no cambia con el metodo (ruido), asi que
 * gana la copia mas rapida.  XMemCpy128 no se probo: exige origen y destino
 * alineados a 128 y `rdram` lo esta a 32. */
static volatile unsigned int  s_stFotoN = 0, s_stFotoTb = 0, s_stFotoMax = 0;
static unsigned int           s_stDpAntes = 0;   /* emu: DP dado con la lista a medias */

/* El plazo del DP.  `s_ultimoTb` lo escribe el hilo (lo que costo la ultima
 * lista); `s_porUs` el emulador (Count por us de emulacion, medido). */
static volatile unsigned int s_ultimoTb = 0;
static unsigned int          s_porUs = 0;
static unsigned int          s_esperaViTb = 0;   /* emulador: espera de este VI */

/* -- Medidas ------------------------------------------------------------ */
/* Se declaran siempre (algunas van por puntero a `esperar_un_poco`), pero
 * con XBOX64_TRAZAS a 0 nadie las escribe ni las lee. */
static volatile unsigned int s_stHw = 0;
static volatile unsigned int s_stListas = 0, s_stListaTb = 0, s_stListaMax = 0;
static volatile unsigned int s_stOrdenes = 0;
static unsigned int s_stDpTb = 0, s_stDpN = 0;         /* esperas en el DP_INT */
static unsigned int s_stColaTb = 0, s_stColaN = 0;     /* cola/escenas llenas  */
static unsigned int s_stVacTb = 0, s_stVacN = 0;       /* vaciados             */
static unsigned int s_stDeudaMax = 0, s_stDeudaDesb = 0;
static unsigned int s_stSinDp = 0, s_stSpCamb = 0, s_stPlazoSum = 0, s_stPlazoN = 0;
static volatile unsigned int s_stInfloop = 0, s_stExcep = 0, s_stExcepCod = 0;
static volatile unsigned int s_stPisadas = 0, s_stFirmadas = 0;  /* hilo  */
static unsigned int s_stSpTb = 0, s_stSpN = 0;                    /* emu   */
static unsigned int s_stSpEspTb = 0, s_stSpEspN = 0;  /* emu: SP_INT esperando a su lista */
static unsigned int s_stSpTope = 0;   /* SP_INT "al acabar" recortado a HL_SP_TOPE_DIV */
/* Tope del SP_INT con "al acabar": vi_delay / esto (2 = medio VI, 1 = un VI).
 * Ver hilo_lista_sp_plazo.  [2026-10-16] A 1 a peticion del usuario, para
 * comparar la fluidez con el 2 (log 3: 26 img/s en partida con el 2). */
#define HL_SP_TOPE_DIV 1u

/* ★ LA FIRMA DE LA CABEZA DE LA LISTA: 64 palabras desde su direccion en
 * RDRAM.  Se toma al despachar (emulador) y se repite justo antes de
 * recorrerla (este hilo).  Distinta = el juego la ESCRIBIO entre medias, o
 * sea que la reutilizo tras el SP_INT adelantado.  Es la medida de la
 * hipotesis de los poligonos que faltan: con `pisadas=0` la causa es otra. */
#if XBOX64_TRAZAS   /* sonda: solo la lee [LISTA2] */
static unsigned int firma_dl(unsigned int dir)
{
    const unsigned long *p;
    unsigned int i, f = 0;
    dir &= 0x007FFFF8u;
    if (dir == 0 || dir + 256u > (unsigned int)sizeof(rdram))
        return 0;
    p = (const unsigned long *)((const unsigned char *)rdram + dir);
    for (i = 0; i < 64u; i++)
        f = ((f << 5) | (f >> 27)) ^ (unsigned int)p[i];
    return f ? f : 1u;
}
#endif /* XBOX64_TRAZAS */

/* ★ TRAZAS DE PASO, una vez por partida cada una (log 26: el arranque se
 * colgo sin decir donde).  Bit a bit, para no repetirlas:
 *   1 primera orden en el hilo      2 primera lista despachada (emulador)
 *   4 primera lista hecha (hilo)    8 primer SWAP grabado (hilo)
 *  16 primer DP_INT vencido (emulador)
 * Si el log se corta, la ultima que salga dice en que tramo se quedo. */
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

/* ====================================================================== */
/* Utilidades                                                             */
/* ====================================================================== */

int hilo_lista_soy_yo(void)
{
    return (s_hiloId != 0) && (GetCurrentThreadId() == s_hiloId);
}

int hilo_lista_activo(void) { return s_activo; }

unsigned char *hilo_lista_dmem(void) { return s_dmem; }
unsigned long *hilo_lista_mi(void)   { return &s_mi; }
unsigned long *hilo_lista_sp(void)   { return &s_sp; }
unsigned long *hilo_lista_vi(int i)
{
    return (i >= 0 && i < HL_VI_N) ? &s_vi[i] : &s_vi[0];
}

void hilo_lista_check_interupt(void)
{
    /* Dentro del hilo, el resultado vuelve por las ventanillas y lo aplica
     * el emulador.  Llamar al nucleo desde aqui seria tocar su estado desde
     * otro hilo. */
    if (hilo_lista_soy_yo())
        return;
    check_interupt();
}

static void foto_vi(unsigned long *v)
{
    v[HL_VI_STATUS]  = vi_register.vi_status;
    v[HL_VI_ORIGIN]  = vi_register.vi_origin;
    v[HL_VI_WIDTH]   = vi_register.vi_width;
    v[HL_VI_INTR]    = vi_register.vi_v_intr;
    v[HL_VI_CURRENT] = vi_register.vi_current;
    v[HL_VI_TIMING]  = vi_register.vi_burst;
    v[HL_VI_V_SYNC]  = vi_register.vi_v_sync;
    v[HL_VI_H_SYNC]  = vi_register.vi_h_sync;
    v[HL_VI_LEAP]    = vi_register.vi_leap;
    v[HL_VI_H_START] = vi_register.vi_h_start;
    v[HL_VI_V_START] = vi_register.vi_v_start;
    v[HL_VI_V_BURST] = vi_register.vi_v_burst;
    v[HL_VI_X_SCALE] = vi_register.vi_x_scale;
    v[HL_VI_Y_SCALE] = vi_register.vi_y_scale;
}

/* ====================================================================== */
/* El hilo                                                                */
/* ====================================================================== */

/* ★ `__try` y no por adorno: si el plugin revienta aqui dentro y el hilo
 * muere, el emulador se queda esperando en su valla para siempre, sin traza.
 * Se cuenta y se publica el codigo de la primera en `[LISTA2]`: un `excep`
 * distinto de 0 es un fallo del plugin, no algo que se pueda ignorar. */
static void ejecutar_plugin(int que)
{
    __try
    {
        xbox64_gfx_real(que);
    }
#if XBOX64_TRAZAS
    __except ((s_stExcep == 0 ? (s_stExcepCod = GetExceptionCode()) : 0),
              EXCEPTION_EXECUTE_HANDLER)
    {
        s_stExcep = s_stExcep + 1;
    }
#else
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        /* el hilo sigue vivo; sin trazas no se cuenta */
    }
#endif
}

static void ejecutar(const HlOrden *o)
{
    memcpy(s_vi, o->vi, sizeof(s_vi));

    switch (o->que)
    {
    case HL_DLIST:
    {
        const unsigned long sem = s_sem[o->esc];
        /* El modo se lee UNA vez: HW1 solo lo cambia con este hilo ocioso. */
        const int foto = (s_spAlAcabar == 2 && s_foto != NULL);
        unsigned long long t0;
        unsigned int dt, dtFoto = 0;

        memcpy(s_dmem, s_escena[o->esc], HILO_LISTA_DMEM_BYTES);
        s_sp = sem;
        s_mi = 0;

        /* La sonda `pisadas`: ver `firma_dl`. */
#if XBOX64_TRAZAS
        if (o->firma)
        {
            s_stFirmadas = s_stFirmadas + 1;
            if (firma_dl(o->dl) != o->firma)
                s_stPisadas = s_stPisadas + 1;
        }
#endif

        /* La foto: ver `s_foto`.  Despues de la firma, que mira la RDRAM
         * viva.  `s_fotoR` se publica con la copia ya entera. */
        if (foto)
        {
            t0 = __mftb();
            XMemCpy(s_foto, rdram, sizeof(rdram));
            dtFoto = (unsigned int)(__mftb() - t0);
            __lwsync();
            s_fotoR = s_dlR + 1;
#if XBOX64_TRAZAS
            s_stFotoN = s_stFotoN + 1;
            s_stFotoTb = s_stFotoTb + dtFoto;
            if (dtFoto > s_stFotoMax) s_stFotoMax = dtFoto;
#endif
            xbox64_gfx_rdram_base(s_foto);
        }

        t0 = __mftb();
        ejecutar_plugin(HL_DLIST);
        dt = (unsigned int)(__mftb() - t0);

        /* La base vuelve a la viva: updateScreen, showCFB y la lista de RDP
         * leen lo que el juego tiene AHORA. */
        if (foto)
            xbox64_gfx_rdram_base((unsigned char *)rdram);

#if XBOX64_TRAZAS
        s_stListas = s_stListas + 1;
        s_stListaTb = s_stListaTb + dt;
        if (dt > s_stListaMax) s_stListaMax = dt;
#endif
        /* El plazo del DP (funcional: `dt`/`dtFoto` se miden siempre).
         * Con foto el DP solo espera a la copia (ver
         * `hilo_lista_dp_vence`), asi que se predice con lo que costo ELLA:
         * predecir la lista entera lo haria llegar ~1,5 VI tarde en tiempo
         * emulado aunque ya no bloquee. */
        s_ultimoTb = foto ? dtFoto : dt;

        /* ★ El `infloop` de glN64 (RSP.cpp:463) y de Rice
         * (RSP_Parser.cpp:1072) BORRA TASKDONE/HALT/BROKE para que el nucleo
         * reencole la lista.  En paralelo el nucleo ya paso por ese punto:
         * se cuenta.  Si sale distinto de 0 en un juego, ese juego no puede
         * ir en paralelo tal cual. */
#if XBOX64_TRAZAS
        if ((sem & 0x203) && !(s_sp & 0x203))
            s_stInfloop = s_stInfloop + 1;
#endif

        s_spOut[o->esc] = s_sp;
        s_dpHecho[s_dlR & (HL_DP_ANILLO - 1u)] = (s_mi & MI_INTR_DP) ? 1 : 0;
        /* Sin foto, una lista acabada cuenta como fotografiada: si la opcion
         * pasa a "foto" con un SP_INT pendiente, vence al acabar su lista en
         * vez de esperar una copia que ya no se va a hacer. */
        if (!foto)
            s_fotoR = s_dlR + 1;
        __lwsync();
        s_dlR = s_dlR + 1;
        traza(4, "[LISTA] traza: primera lista hecha en el hilo");
        break;
    }

    case HL_RDPLIST:
        s_mi = 0;
        ejecutar_plugin(HL_RDPLIST);
        s_rdpMi = s_mi;
        break;

    case HL_FBEGIN:
        gxd3d_frame_begin();
        break;

    case HL_FEND:
        gxd3d_frame_end();
        break;

    case HL_SWAPBEGIN:
        d3dq_mete_swap();
        traza(8, "[LISTA] traza: primer SWAP grabado en el hilo");
        gxd3d_frame_begin();
        break;

    default:
        ejecutar_plugin((int)o->que);
        break;
    }
}

static DWORD WINAPI hilo_proc(LPVOID)
{
    for (;;)
    {
        WaitForSingleObject(s_evTrabajo, 100);
        if (s_salir)
            break;

#if XBOX64_TRAZAS
        s_stHw = (unsigned int)GetCurrentProcessorNumber();
#endif

        /* Se comprueba la CONDICION, no se cuentan senales: los eventos son
         * auto-reset y dos avisos seguidos pueden quedarse en uno. */
        while (s_colaR != s_colaW)
        {
            __lwsync();              /* la orden entera antes de leerla */
            traza(1, "[LISTA] traza: primera orden recibida en el hilo");
#if XBOX64_TRAZAS
            s_ordenQue = s_cola[s_colaR & (HL_RANURAS - 1u)].que;
#endif
            ejecutar(&s_cola[s_colaR & (HL_RANURAS - 1u)]);
#if XBOX64_TRAZAS
            s_ordenQue = 0;
            s_stOrdenes = s_stOrdenes + 1;
#endif
            __lwsync();              /* su trabajo visible antes del cursor */
            s_colaR = s_colaR + 1;
            SetEvent(s_evHecho);
        }
    }
    return 0;
}

/* ====================================================================== */
/* El lado del emulador (HW2)                                             */
/* ====================================================================== */

/* ★ [revision 2026-10-01] EL VIGIA DE ESPERAS LARGAS.  Todas las esperas
 * del emulador a este hilo pasan por `esperar_un_poco`, y ninguna tiene
 * tope: si este hilo se atasca (un bucle dentro del plugin, una orden
 * sincrona que nadie atiende), la consola se queda colgada SIN decir donde
 * -- como en los logs 26 y 36.  El vigia no cambia nada: si la cola no
 * avanza en 2 s seguidos de espera, escribe UNA linea con el estado, y otra
 * cuando se resuelve.  Una espera larga legitima (el menu de Salvia abierto
 * mientras este hilo espera al buzon) sale como las dos lineas; un cuelgue,
 * solo como la primera. */
#if XBOX64_TRAZAS   /* el vigia solo escribe en el log */
#define HL_VIGIA_TB   (2ULL * 50000000ULL)   /* 2 s de time base (50 MHz) */
#define HL_VIGIA_HUECO (1000000ULL)          /* 20 ms: otra espera distinta */
static unsigned int       s_vigColaR = 0;
static unsigned long long s_vigT0 = 0, s_vigUlt = 0;
static int                s_vigAvisado = 0;

static void vigia_lista(void)
{
    const unsigned long long ahora = __mftb();
    const int nueva = (ahora - s_vigUlt) > HL_VIGIA_HUECO;
    s_vigUlt = ahora;
    if (nueva || s_colaR != s_vigColaR)
    {
        if (s_vigAvisado)
            xbox64_log_line("[LISTA] espera larga resuelta: la cola vuelve a avanzar");
        s_vigColaR   = s_colaR;
        s_vigT0      = ahora;
        s_vigAvisado = 0;
        return;
    }
    if (!s_vigAvisado && (ahora - s_vigT0) > HL_VIGIA_TB)
    {
        s_vigAvisado = 1;
        hilo_lista_traza_estado("el emulador lleva 2 s esperando al hilo de la lista");
    }
}
#endif /* XBOX64_TRAZAS */

void hilo_lista_traza_estado(const char *por)
{
#if !XBOX64_TRAZAS
    (void)por;
#else
    char l[256];
    _snprintf(l, sizeof(l),
              "[LISTA] ★ ESPERA LARGA (%s) | ordenes R/W=%u/%u en curso=%u | "
              "listas R/W=%u/%u dp=%u | buzon=%d anillo=%u",
              por, s_colaR, s_colaW, s_ordenQue, s_dlR, s_dlW, s_dpIdx,
              d3dq_buzon_ocupado(), (unsigned int)(g_d3dqCabeza - g_d3dqCola));
    l[sizeof(l) - 1] = 0;
    xbox64_log_line(l);
#endif /* XBOX64_TRAZAS */
}

/* Una espera del emulador al hilo.  Se acumula en el tramo que la pidio y en
 * la del VI, que se descuenta al medir `Count` por us (ver el plazo).
 * `s_esperaViTb` es FUNCIONAL (plazo del DP); `*tb` es solo estadistica. */
static void esperar_un_poco(unsigned int *tb)
{
    const unsigned long long t0 = __mftb();
#if XBOX64_TRAZAS
    vigia_lista();
#endif
    WaitForSingleObject(s_evHecho, 2);
    {
        const unsigned int d = (unsigned int)(__mftb() - t0);
#if XBOX64_TRAZAS
        *tb += d;
#else
        (void)tb;
#endif
        s_esperaViTb += d;
    }
}

static void encolar_orden(unsigned int que, unsigned int esc,
                          unsigned int dl, unsigned int firma)
{
    HlOrden *o;

    if (s_colaW - s_colaR >= HL_RANURAS)
    {
#if XBOX64_TRAZAS
        s_stColaN++;
#endif
        while (s_colaW - s_colaR >= HL_RANURAS)
            esperar_un_poco(&s_stColaTb);
    }
    o = &s_cola[s_colaW & (HL_RANURAS - 1u)];
    o->que = que;
    o->esc = esc;
    o->dl = dl;
    o->firma = firma;
    foto_vi(o->vi);
    __lwsync();                  /* la orden entera antes que el cursor */
    s_colaW = s_colaW + 1;
    SetEvent(s_evTrabajo);
}

/* ★ APLICAR EL SP QUE DEVOLVIO EL PLUGIN, SOLO LOS BITS QUE CAMBIO.  Entre el
 * despacho y la cosecha corren tareas de audio por el mismo `do_SP_task` y
 * dejan `sp_status` como debe estar; copiar la palabra entera lo pisaba y
 * colgaba el apreton de manos del SP (log 4 del hilo antiguo). */
static void cosechar(void)
{
    while (s_resR != s_dlR)
    {
        const unsigned int esc = s_resR & (HL_ESCENAS - 1u);
        unsigned long cambiados;
        __lwsync();
        cambiados = s_spOut[esc] ^ s_sem[esc];
        if (cambiados)
        {
#if XBOX64_TRAZAS
            s_stSpCamb++;
#endif
            sp_register.sp_status_reg =
                (sp_register.sp_status_reg & ~cambiados) |
                (s_spOut[esc] & cambiados);
        }
        s_resR = s_resR + 1;
    }
}

int hilo_lista_encolar(int que)
{
    /* ★ Solo mientras el emulador corre en su hilo (`g_d3dqVivo`).  Fuera
     * de eso -- la carga, una pausa (savestate, opciones), la descarga -- el
     * que llama es el hilo del frontend con este hilo vacio, y el plugin va
     * en linea como en la etapa 4: si se encolara, este hilo grabaria
     * ordenes D3D a la vez que el frontend, y la cola es de UN productor. */
    if (!s_activo || !s_hilo || !g_d3dqVivo || hilo_lista_soy_yo())
        return 0;

    switch (que)
    {
    case HL_DLIST:
    {
        unsigned int esc;

        cosechar();
        /* La escena que toca tiene que estar ya volcada por el hilo. */
        if (s_dlW - s_dlR >= HL_ESCENAS)
        {
#if XBOX64_TRAZAS
            s_stColaN++;
#endif
            while (s_dlW - s_dlR >= HL_ESCENAS)
                esperar_un_poco(&s_stColaTb);
            cosechar();
        }
        /* La deuda no puede esperarse aqui: la saldan los DP_INT, que los
         * procesa este mismo hilo.  Se cuenta si se desborda el anillo de
         * resultados (tiene que salir 0). */
#if XBOX64_TRAZAS
        if (s_dlW - s_dpIdx >= HL_DP_ANILLO)
            s_stDeudaDesb++;
#endif

        traza(2, "[LISTA] traza: primera lista despachada (el emulador espera a que acabe)");
        esc = s_dlW & (HL_ESCENAS - 1u);
        memcpy(s_escena[esc], SP_DMEM, HILO_LISTA_DMEM_BYTES);
        s_sem[esc] = sp_register.sp_status_reg;
        s_dlW = s_dlW + 1;
        s_tareaConLista = 1;
        s_tareaSp = 1;
#if XBOX64_TRAZAS
        if (s_dlW - s_dpIdx > s_stDeudaMax) s_stDeudaMax = s_dlW - s_dpIdx;

        {
            /* La lista esta en `data_ptr` del OSTask (DMEM 0xFF0). */
            const unsigned int dl = (unsigned int)SP_DMEM[0xFF0 / 4];
            encolar_orden(HL_DLIST, esc, dl, firma_dl(dl));
        }
#else
        /* Sin la sonda `pisadas` no hace falta ni la direccion ni la firma. */
        encolar_orden(HL_DLIST, esc, 0, 0);
#endif

        /* La primera de la partida, sincrona: la deteccion del ucode y su
         * `GBI_Init` se hacen antes de que el juego siga. */
        if (s_primera)
        {
            s_primera = 0;
            while (s_dlR != s_dlW)
                esperar_un_poco(&s_stColaTb);
            cosechar();
        }
        return 1;
    }

    case HL_RDPLIST:
        /* SINCRONA: el juego espera su DP en el acto, como con el plugin en
         * linea (que levanta MI_INTR_DP y llama a CheckInterrupts). */
        encolar_orden(HL_RDPLIST, 0, 0, 0);
        while (s_colaR != s_colaW)
            esperar_un_poco(&s_stColaTb);
        __lwsync();
        cosechar();
        if (s_rdpMi & MI_INTR_DP)
        {
            MI_register.mi_intr_reg |= MI_INTR_DP;
            check_interupt();
        }
        return 1;

    default:
        encolar_orden((unsigned int)que, 0, 0, 0);
        return 1;
    }
}

void hilo_lista_frame_begin(void) { if (s_activo) encolar_orden(HL_FBEGIN, 0, 0, 0); }
void hilo_lista_frame_end(void)   { if (s_activo) encolar_orden(HL_FEND, 0, 0, 0); }
void hilo_lista_swap_begin(void)  { if (s_activo) encolar_orden(HL_SWAPBEGIN, 0, 0, 0); }

/* ====================================================================== */
/* El SP al acabar (opcion `xbox64_lista_sp`).  Ver hilo_lista.h.         */
/* ====================================================================== */

/* ★ [Cruis'n USA, log 20] Lo que pide la OPCION y lo que pide el JUEGO, por
 * separado, como la velocidad de CPU de Conker (xbox/plataforma.c): el modo
 * del juego (`hilo_lista_sp_db`, puesto por CRC en gamehacks.c en cada carga)
 * solo manda si la opcion sigue en su valor por defecto, "desactivado".  Lo
 * que elija el usuario -- tambien desde el .opt por juego de Salvia -- gana
 * siempre.  Se guardan los dos porque la opcion se reaplica en cada cambio de
 * opciones a mitad de partida. */
static int s_spOpt = 0;
static int s_spDb  = 0;

static void sp_aplicar(void)
{
    int si = (s_spOpt == 0 && s_spDb) ? s_spDb : s_spOpt;

    /* 2 = "foto": la copia se reserva aqui (HW1, este hilo ocioso) la primera
     * vez, y se queda para siempre, como los hilos.  Sin memoria, "al acabar":
     * que el juego siga bien dibujado aunque vaya a tirones. */
    if (si == 2 && s_foto == NULL)
    {
        s_foto = (unsigned char *)VirtualAlloc(NULL, sizeof(rdram),
                     MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
        if (s_foto == NULL)
            s_foto = (unsigned char *)VirtualAlloc(NULL, sizeof(rdram),
                         MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#if XBOX64_TRAZAS
        xbox64_log_line(s_foto ? "[LISTA] foto de la RDRAM: copia reservada"
                               : "[LISTA] foto de la RDRAM: SIN MEMORIA, va como 'activado'");
#endif
    }
    if (si == 2 && s_foto == NULL)
        si = 1;
    s_spAlAcabar = (si == 2) ? 2 : (si ? 1 : 0);
    s_tareaSp = 0;
    s_spPend = 0;
    s_spCedida = 0;
}

void hilo_lista_sp_al_acabar(int si)
{
    s_spOpt = (si == 2) ? 2 : (si ? 1 : 0);
    sp_aplicar();
}

void hilo_lista_sp_db(int modo)
{
#if XBOX64_TRAZAS
    char l[128];
#endif
    s_spDb = (modo == 1 || modo == 2) ? modo : 0;
    sp_aplicar();
#if XBOX64_TRAZAS
    if (s_spDb)
    {
        _snprintf(l, sizeof(l),
                  "[xbox64] por juego: lista en paralelo, esperar al RSP = %s%s",
                  s_spDb == 2 ? "foto" : "activado",
                  s_spOpt == 0 ? ""
                      : " IGNORADO (la opcion no esta en su valor por defecto)");
        l[sizeof(l) - 1] = 0;
        xbox64_log_line(l);
    }
#endif
}

/* `do_SP_task`, al programar el SP_INT de la tarea grafica.  Con la opcion
 * se apunta a que lista hay que esperar; el retraso inicial es el de
 * siempre, y el manejador lo va reprogramando hasta que la lista acaba.
 *
 * ★ [log 31] LA PRIMERA VERSION USABA EL PLAZO PREDICHO DEL DP, y sobraba
 * siempre: `sp=al acabar 0 (0)`, la valla no espero ni una vez.  Con listas
 * de 22-29 ms en Cruis'n USA el SP_INT llegaba ~1,7 VI tarde, el juego no
 * podia lanzar su tarea de AUDIO hasta tener el RSP libre, y la musica
 * repetia notas.  Ahora llega en cuanto la lista acaba, y nunca antes.
 *
 * ★★ [Blast Corps, logs A/B/C y 17] CON "AL ACABAR", EL PLAZO ES EL DE LA
 * LISTA, COMO EL DEL DP.  Con los 1000 de siempre el SP_INT vencia enseguida,
 * la lista seguia en HW4 y el manejador lo reprogramaba cada 1/64 de VI: el
 * emulador, que recorre el VI en ~4 ms, gastaba esos reintentos en tiempo
 * EMULADO mientras la lista tardaba ~9 ms REALES, y el SP_INT caia en el VI
 * siguiente.  Medido en C: 60 VI/s pero 15-19 imagenes/s con el emulador al
 * 24 por ciento (A: 25-28; B: ~30, con errores).  Ahora el SP_INT se programa
 * con lo que costo la lista anterior (`plazo_lista`, el mismo calculo que el
 * DP) y, si al vencer la lista aun no ha acabado, el emulador la ESPERA
 * (`hilo_lista_sp_vence`).  Log 17: Blast Corps a 30 imagenes/s.
 *
 * Probado y DESCARTADO (log 18): esperar solo con la CPU ociosa (mismo `pc`
 * en dos reintentos).  Cruis'n USA mejoraba algo, pero Blast Corps bajaba a
 * 20.  Cruis'n va en "foto" por CRC (gamehacks.c), que le va mejor que esto.
 * Ojo, sin explicar: en el log 17 Blast Corps dejo de lanzar tareas de audio
 * a mitad de partida (`[RSPT] audio=0 (0/s)`); en el 18, no. */
static unsigned plazo_lista(void);

unsigned hilo_lista_sp_plazo(unsigned normal)
{
    const int hubo = s_tareaSp;
    s_tareaSp = 0;
    /* Una tarea grafica NUEVA: si habia una cedida sin relanzar, el juego la
     * dio por terminada (el SP_INT cedido le sirvio de "hecho"). */
    s_spCedida = 0;
    if (!s_activo || !s_spAlAcabar || !hubo)
        return normal;
    s_spHasta = s_dlW;
    s_spPend = 1;
    if (s_spAlAcabar == 1)
    {
        unsigned p = plazo_lista();
        /* ★★ [Blast Corps, el audio se para para siempre] EL SP_INT, COMO MUCHO
         * vi_delay / HL_SP_TOPE_DIV DE TIEMPO EMULADO (resuelto con medio VI;
         * en prueba con uno).  `plazo_lista` sale del coste REAL de la
         * lista anterior y se acota a HL_PLAZO_VI (3) frames, que es lo medido
         * para el DP.  Medido ("Logs Salvia (1)", 2026-10-16): una lista de
         * 159 ms al empezar el nivel (con una compilacion de shader de 93 ms
         * dentro) deja el plazo en el tope, el juego ve tareas graficas de 3
         * VI y, con 60 listas/s, el RSP siempre ocupado.  Blast Corps no cede
         * (`cede=0`): espera a que acabe la grafica para lanzar el audio, el
         * AI se vacia y su hilo de audio queda esperando una interrupcion de
         * fin de DMA que ya no llega (sonda [AI], retirada: escr=0, ai_st=0,
         * sin AI_INT).  ★RESUELTO ("Logs Salvia (3)": 420 s, cero sin audio).
         * En la consola una tarea grafica son unos ms.  Con el tope, si la
         * lista no ha acabado al vencer el emulador la sigue ESPERANDO en
         * tiempo real (`hilo_lista_sp_vence`), que es lo que ya hace el modo
         * "al acabar"; lo que no puede es pasarle al juego frames de RSP
         * ocupado.  El DP conserva su tope de 3 VI (no bloquea el audio). */
        const unsigned tope = (unsigned)(vi_register.vi_delay / HL_SP_TOPE_DIV);
        if (tope && p > tope)
        {
            p = tope;
#if XBOX64_TRAZAS
            s_stSpTope++;
#endif
        }
        return (p > normal) ? p : normal;
    }
    return normal;
}

/* ★★ [log 35] EL YIELD DEL RSP, que es como la consola deja pasar el audio.
 *
 * Medido en el log 35 (Cruis'n USA, "esperar al RSP"): listas de 15-30 ms,
 * el SP_INT llega ~1,8 VI tarde de media (`reintentos` ~3000/s, 64 por VI) y
 * la musica repite notas.  El planificador de libultra (sched.c) no lanza la
 * tarea de AUDIO con una grafica en marcha: le pide que CEDA
 * (`osSpTaskYield`, escribe SP_SET_SIG0) y espera el SP_INT; el microcodigo
 * guarda su estado, pone SIG1 (SP_STATUS_YIELDED) y para.  Con SIG1,
 * `osSpTaskYielded` marca la tarea OS_TASK_YIELDED, corre el audio y despues
 * RELANZA la grafica, que `osSpTaskLoad` sube a la DMEM con ese flag puesto
 * (copia el OSTask antes de borrarlo del original).
 *
 * Sin esto, el SIG0 se apuntaba y nadie contestaba: el audio esperaba a que
 * acabara la lista entera.  Ahora:
 *   ceder     update_SP, al poner SIG0: si la lista de la tarea grafica
 *             sigue en el hilo, el SP_INT pendiente se entrega YA como
 *             cedido.  Si ya acabo, nada: sale como "hecho", que es lo que
 *             hace la consola cuando la tarea termina antes de ceder.
 *   reanudar  do_SP_task, tarea grafica con OS_TASK_YIELDED: no se recorre
 *             la lista otra vez; se vuelve a esperar a la MISMA.
 * `cede`, `reanuda` y `sin` en [LISTA2].  `sin` = relanzada con el flag sin
 * haberla cedido nosotros: tiene que salir 0. */
/* ★★ [Killer Instinct, log 8] SE CEDE SIEMPRE QUE EL SP_INT NO SE HAYA
 * ENTREGADO, aunque la lista ya haya acabado en este hilo.
 *
 * Antes, con la lista ya hecha se contestaba "terminada": el SP_INT salia
 * con SIG0 puesto y sin SIG1.  La traza del log 8 lo pillo en el cuelgue:
 * `L` (lista 412) -> `s` x64 -> la lista acaba (W/R 412/412) pero el SP_INT
 * espera a su reintento de 1/64 de VI -> en ese hueco el juego pide ceder
 * (`W 0x400`) -> `Y a=0` -> `S 0x2C3` -> audio, y KI no vuelve a lanzar una
 * tarea grafica.  Las cuatro cesiones buenas de esa partida fueron con la
 * lista a medias.
 *
 * Para el juego la tarea SEGUIA en marcha: no habia recibido su SP_INT, y por
 * eso pidio ceder.  En la consola el microcodigo que aun no ha avisado de su
 * fin ve SIG0 y cede; "acabo con SIG0 puesto" es una carrera de un instante
 * que aqui abre nuestra espera.  Asi que la respuesta es la de la consola:
 * cedida.  Lo que eso implica para el DP esta en `hilo_lista_dp_vence`.
 *
 * La excepcion: si el DP de esa lista YA se entrego, para el juego el RDP la
 * termino entera, y una tarea terminada no puede ceder.  Entonces se contesta
 * "terminada" como antes y se cuenta en `cedetarde` ([LISTA2]); la traza lo
 * marca `Y a=0`.  [log 9] Ese caso FUE el cuelgue siguiente (`tarde 1`), y su
 * causa es que el DP salia antes que el SP de su lista; desde entonces el DP
 * espera al SP (`hilo_lista_dp_vence`) y `tarde` tiene que salir 0.
 * Devuelve 1 si la lista seguia en el hilo y 2 si ya habia acabado (traza). */
int hilo_lista_sp_ceder(void)
{
    int r;
    if (!s_activo || !s_spPend)
        return 0;
    __lwsync();
    if ((int)(s_dpIdx - s_spHasta) >= 0)
    {
#if XBOX64_TRAZAS
        s_stCedeTarde++;
#endif
        return 0;                       /* su DP ya salio: sale como hecho */
    }
    r = ((int)(s_dlR - s_spHasta) >= 0) ? 2 : 1;
    s_spPend     = 0;
    s_spCedida   = 1;
    s_spCedHasta = s_spHasta;
#if XBOX64_TRAZAS
    s_stCede++;
    if (r == 2) s_stCedeHecha++;
#endif
    return r;
}

int hilo_lista_sp_reanudar(void)
{
    if (!s_spCedida)
    {
#if XBOX64_TRAZAS
        if (s_activo) s_stReanudaSin++;
#endif
        return 0;
    }
    s_spCedida = 0;
    s_spHasta  = s_spCedHasta;
    s_spPend   = 1;
#if XBOX64_TRAZAS
    s_stReanuda++;
#endif
    return 1;
}

/* El manejador de SP_INT.  Sin lista pendiente, 1 (como siempre).  Con ella:
 * si ya acabo, cosechar y 1.  Si no: con "al acabar" el emulador ESPERA a la
 * lista aqui (su plazo ya era lo que se esperaba que tardase, ver
 * `hilo_lista_sp_plazo`) y entrega; con "foto", 0 y el manejador lo
 * reprograma sin bloquear, esperando a que este FOTOGRAFIADA (`s_fotoR`). */
int hilo_lista_sp_vence(void)
{
    if (!s_activo || !s_spPend)
        return 1;
    __lwsync();
    if ((int)(((s_spAlAcabar == 2) ? s_fotoR : s_dlR) - s_spHasta) < 0)
    {
        if (s_spAlAcabar != 1)
        {
#if XBOX64_TRAZAS
            s_stSpN++;                  /* reintentos */
#endif
            return 0;
        }
#if XBOX64_TRAZAS
        s_stSpEspN++;
#endif
        while ((int)(s_dlR - s_spHasta) < 0)
            esperar_un_poco(&s_stSpEspTb);
        __lwsync();
    }
    s_spPend = 0;
    cosechar();
    return 1;
}

/* Cada cuanto se vuelve a mirar: 1/64 de VI (~0,26 ms emulados). */
unsigned hilo_lista_sp_reintento(void)
{
    const unsigned long vd = vi_register.vi_delay;
    const unsigned r = (unsigned)(vd / 64u);
    return (r > 1000u) ? r : 1000u;
}

void hilo_lista_vaciar(void)
{
    if (!s_activo || !s_hilo || hilo_lista_soy_yo())
        return;
    if (s_colaR != s_colaW)
    {
#if XBOX64_TRAZAS
        s_stVacN++;
#endif
        while (s_colaR != s_colaW)
            esperar_un_poco(&s_stVacTb);
    }
    __lwsync();
    cosechar();
}

/* ★ `Count` POR MICROSEGUNDO DE EMULACION, MEDIDO.  No `vi_delay / 16667`:
 * los 16,7 ms son el ritmo de fuera, y el hilo recorre un VI en su `trab`.
 * Descontando la espera al hilo de la lista: durante ella el reloj corre y
 * `Count` no, y contarla encogeria el plazo y realimentaria la espera (nota
 * de `gfx_hilo_plazo_dp` en el hilo antiguo). */
void hilo_lista_anota_vi(unsigned int us)
{
    const unsigned int esp = s_esperaViTb / 50u;
    const unsigned long vd = vi_register.vi_delay;
    s_esperaViTb = 0;
    if (!s_activo || vd == 0 || us <= esp + 100u)
        return;
    {
        const unsigned int por = (unsigned int)(vd / (us - esp));
        s_porUs = s_porUs ? (s_porUs * 7u + por) / 8u : por;
    }
}

int hilo_lista_dp_programar(int hay_dp)
{
    if (!s_activo || !s_tareaConLista)
        return 0;
    s_tareaConLista = 0;
    return hay_dp ? 0 : 1;
}

/* El plazo del DP_INT: lo que costo la lista ANTERIOR, en Count.  Acotado
 * por abajo a los 4000 de siempre y por arriba a HL_PLAZO_VI frames.
 *
 * ★ EL TOPE ERA UN FRAME Y ESO ERA LA VALLA (log 27, GoldenEye): las listas
 * tardan ~2 VI de emulacion (25 ms en HW4 contra 11,6 ms que tarda el
 * emulador en un VI), `plazo med` salia clavado en el tope (811092 =
 * `vi_delay`) y el emulador esperaba `dp=240-320 ms` de cada segundo.  El
 * plazo ya es lo MEDIDO: el tope solo evita un valor absurdo.  Bajarlo a
 * medio frame fue una regresion medida en el hilo antiguo (log 10); subirlo
 * no lo midio nadie, y esta es esa medida. */
#define HL_PLAZO_VI 3u
unsigned hilo_lista_plazo_dp(void)
{
    unsigned int plazo;

    if (vi_register.vi_delay == 0)
        return 4000;
    /* [log 8] Con el DP retenido por una cesion no se predice nada: se mira
     * al ritmo del SP_INT (1/64 de VI), que es cuando el juego puede haber
     * relanzado la tarea. */
    if (s_dpRetenido)
        return hilo_lista_sp_reintento();
    plazo = plazo_lista();
#if XBOX64_TRAZAS
    s_stPlazoSum += plazo;
    s_stPlazoN++;
#endif
    return plazo;
}

/* Lo que costo la ultima lista (tiempo REAL de HW4), pasado a Count.  Lo usan
 * el DP (`hilo_lista_plazo_dp`) y, con "al acabar", el SP_INT
 * (`hilo_lista_sp_plazo`). */
static unsigned plazo_lista(void)
{
    const unsigned long vd = vi_register.vi_delay;
    unsigned int por, plazo;

    if (vd == 0)
        return 4000;
    /* ★★ COUNT POR us: EL MENOR ENTRE EL LIBRE Y EL ACOMPASADO (log 28).
     *
     * El plazo pasa el coste de la lista (tiempo REAL de HW4) a tiempo
     * EMULADO.  Con la velocidad LIBRE del emulador (`s_porUs`, medida sin
     * sus esperas) eso esta mal cuando el emulador va mas rapido que el
     * tiempo real: en Mario 64 emula un VI en 4,3 ms, asi que los 8 ms de su
     * lista salian ~2 VI emulados.  El DP le llegaba al juego dos VI tarde y
     * pasaba de 30 a 20 imagenes por segundo -- y como Mario cuenta su logica
     * por frame, el juego entero iba a 2/3 de velocidad -- con el emulador y
     * la lista ociosos (`esp=0`, `dp=0`, `img=20`).  El tope de 1 VI lo
     * tapaba; al subirlo a 3 salio a la luz.
     *
     * El emulador acaba ACOMPASADO a 60 VI/s, asi que el tiempo del juego
     * tiene que seguir al reloj real: `vi_delay / 16667` Count por us.  La
     * velocidad libre solo se usa si es MENOR (el emulador por detras del
     * tiempo real): entonces el DP no puede llegar antes de que la lista
     * acabe en su tiempo emulado.  Si el emulador alcanza el plazo antes de
     * que la lista acabe, espera: tiempo que le sobraba igualmente. */
    {
        const unsigned int ritmo = (unsigned int)(vd / 16667u);
        por = s_porUs ? s_porUs : ritmo;
        if (ritmo && por > ritmo) por = ritmo;
    }
    if (por == 0) por = 1;
    plazo = (s_ultimoTb / 50u) * por;
    if (plazo < 4000u) plazo = 4000u;
    if (plazo > (unsigned int)vd * HL_PLAZO_VI)
        plazo = (unsigned int)vd * HL_PLAZO_VI;
    return plazo;
}

int hilo_lista_dp_vence(void)
{
    int dar;

    if (!s_activo || s_dpIdx == s_dlW)
        return 1;                       /* sin deuda: como siempre */

    /* ★ [Killer Instinct, log 8] EL DP DE UNA TAREA CEDIDA, RETENIDO HASTA
     * QUE SE RELANZA.  En la consola una tarea cedida no ha llegado a su
     * fullsync: su DP sale cuando el juego la relanza y termina.
     *
     * Historia: en el log 7 esta retencion estuvo puesta por otra hipotesis
     * y no actuo ni una vez (`dpret=0`), porque las cesiones de entonces eran
     * con la lista a medias y su DP vencia ya relanzada; se quito.  Vuelve
     * porque ahora se cede tambien con la lista ACABADA (ver
     * `hilo_lista_sp_ceder`), y en ese caso el DP SI vence antes del
     * relanzamiento: en la traza del log 8, `D` justo despues de `Y`.
     *
     * Devuelve 0 sin saldarlo y el manejador lo reprograma con el ritmo
     * corto (`plazo_dp`).  Se suelta en cuanto la tarea se relanza
     * (`reanudar`) o el juego lanza otra grafica (`sp_plazo`).  Los DP de
     * listas anteriores no se tocan.  `dpret` en [LISTA2]. */
    if (s_spCedida && s_dpIdx + 1u == s_spCedHasta)
    {
        s_dpRetenido = 1;
#if XBOX64_TRAZAS
        s_stDpRet++;
#endif
        return 0;
    }
    /* ★★ [Killer Instinct, log 9] EL DP NO SALE ANTES QUE EL SP DE SU LISTA.
     *
     * En la consola el RSP acaba de alimentar la lista ANTES de que el RDP la
     * termine: el SP_INT de una tarea grafica llega antes que su DP.  Aqui era
     * al reves: este manejador espera BLOQUEADO a la lista y entrega en
     * cuanto acaba, y el SP_INT solo la mira cada 1/64 de VI
     * (`sp_reintento`).  La traza del log 9 cogio el cuelgue en ese hueco:
     * `s` x64 -> `D` (DP de la lista 520) -> el juego pide ceder (`W 0x400`)
     * -> con su DP ya fuera la tarea no puede ceder (`Y a=0`, `tarde 1`) ->
     * `S 0x2C3` (SIG0 sin SIG1) -> audio y ni una grafica mas.  Las tres
     * cesiones `hecha` de esa partida, con el DP aun dentro, fueron bien.
     *
     * Asi que mientras el SP_INT de ESTA lista no se haya entregado
     * (`s_spPend` y es la que espera), el DP se retiene como el de una
     * cedida: 0 sin saldarlo y reintento corto.  Sale en el reintento
     * siguiente al SP_INT.  Solo con "esperar al RSP" (sin la opcion
     * `s_spPend` nunca se pone).  `dpsp` en [LISTA2].  Y solo si HAY un
     * SP_INT en la cola: con "interrupt on break" apagado `do_SP_task` no lo
     * programa, nadie borraria `s_spPend` y el DP no saldria nunca. */
    if (s_spPend && s_dpIdx + 1u == s_spHasta && get_event(HL_EV_SP_INT))
    {
        s_dpRetenido = 1;
#if XBOX64_TRAZAS
        s_stDpSp++;
#endif
        return 0;
    }
    s_dpRetenido = 0;

    /* ★ [Cruis'n USA, log 19] CON "FOTO" EL DP NO ESPERA A LA LISTA, SOLO A SU
     * FOTO.  Con la foto el SP_INT llega ~5 ms despues del despacho, el juego
     * pide su DP enseguida y aqui se quedaba BLOQUEADO hasta que la lista
     * acababa en HW4 (glN64: 15-26 ms + la copia): `dp` de 52 ms/s de media
     * y hasta 191, frames del emulador al 160-215 %.  En el log 12, sin foto,
     * esa espera era 0.
     *
     * Con la foto, lo que el juego haga tras su DP ya no le llega a la lista
     * (dibuja desde la copia), y el orden con el cambio de pantalla lo guarda
     * la cola de este hilo, que es FIFO.  Es el "en el acto" de mupen, donde
     * el DP sale en cuanto la lista se ha leido.  Lo unico que hay que
     * esperar es la copia, para que el juego no pueda pisar nada antes.
     * Si la lista no ha acabado no se sabe si llego a su fullsync: se da por
     * hecho (`sindp` ha salido 0 en todos los logs) y se cuenta en `dpantes`
     * ([LISTA3]). */
    if (s_spAlAcabar == 2)
    {
        if ((int)(s_fotoR - s_dpIdx) <= 0)
        {
#if XBOX64_TRAZAS
            s_stDpN++;
#endif
            while ((int)(s_fotoR - s_dpIdx) <= 0)
                esperar_un_poco(&s_stDpTb);
        }
        __lwsync();
        cosechar();
        if ((int)(s_dlR - s_dpIdx) > 0)
        {
            __lwsync();          /* `s_dlR` antes que el `s_dpHecho` que cubre */
            dar = s_dpHecho[s_dpIdx & (HL_DP_ANILLO - 1u)] ? 1 : 0;
        }
        else
        {
            dar = 1;
#if XBOX64_TRAZAS
            s_stDpAntes++;
#endif
        }
        s_dpIdx = s_dpIdx + 1;
        traza(16, "[LISTA] traza: primer DP_INT entregado (foto)");
#if XBOX64_TRAZAS
        if (!dar) s_stSinDp++;
#endif
        return dar;
    }

    if (s_dlR == s_dpIdx)
    {
#if XBOX64_TRAZAS
        s_stDpN++;
#endif
        while (s_dlR == s_dpIdx)
            esperar_un_poco(&s_stDpTb);
    }
    __lwsync();
    cosechar();

    dar = s_dpHecho[s_dpIdx & (HL_DP_ANILLO - 1u)] ? 1 : 0;
    s_dpIdx = s_dpIdx + 1;
    traza(16, dar ? "[LISTA] traza: primer DP_INT entregado"
                  : "[LISTA] traza: primer DP_INT SIN fullsync (no se entrega)");
#if XBOX64_TRAZAS
    if (!dar) s_stSinDp++;
#endif
    return dar;
}

int hilo_lista_dp_quedan(void)
{
    return (s_activo && s_dpIdx != s_dlW) ? 1 : 0;
}

void hilo_lista_olvidar_dp(void)
{
    s_dpIdx = s_dlW;
    s_tareaConLista = 0;
    /* [revision 2026-10-01] Y el yield: una tarea cedida en ESTA sesion no
     * es la del estado cargado.  Si el estado relanza una tarea cedida, con
     * `s_spCedida` a 0 se ejecuta entera (redibuja y da su DP); a 1 se
     * saltaria su lista y su DP, y el juego esperaria un DP que no llega. */
    s_spCedida = 0;
    s_spPend   = 0;
    s_dpRetenido = 0;
}

/* ====================================================================== */
/* Ciclo de vida (HW1)                                                    */
/* ====================================================================== */

int hilo_lista_crear(void)
{
    if (s_hilo)
        return 1;

    s_evTrabajo = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_evHecho   = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!s_evTrabajo || !s_evHecho)
        goto mal;

    s_salir = 0;
    s_colaW = s_colaR = 0;

    /* [log 36] Una traza por llamada al sistema; una vez por proceso. */
#if XBOX64_TRAZAS
    xbox64_log_line("[LISTA] traza: crear: CreateThread...");
#endif
    s_hilo = CreateThread(NULL, HL_PILA, hilo_proc, NULL,
                          CREATE_SUSPENDED, &s_hiloId);
    if (!s_hilo)
        goto mal;

#if XBOX64_TRAZAS
    xbox64_log_line("[LISTA] traza: crear: prioridad y HW...");
#endif
    SetThreadPriority(s_hilo, THREAD_PRIORITY_NORMAL);
    XSetThreadProcessor(s_hilo, HL_HW);
#if XBOX64_TRAZAS
    xbox64_log_line("[LISTA] traza: crear: ResumeThread...");
#endif
    ResumeThread(s_hilo);
    return 1;

mal:
    if (s_evTrabajo) { CloseHandle(s_evTrabajo); s_evTrabajo = NULL; }
    if (s_evHecho)   { CloseHandle(s_evHecho);   s_evHecho   = NULL; }
    s_hiloId = 0;
    return 0;
}

void hilo_lista_destruir(void)
{
    if (!s_hilo)
        return;
    s_activo = 0;
    s_salir = 1;
    __lwsync();
    SetEvent(s_evTrabajo);
    WaitForSingleObject(s_hilo, 2000);
    CloseHandle(s_hilo);      s_hilo = NULL;  s_hiloId = 0;
    CloseHandle(s_evTrabajo); s_evTrabajo = NULL;
    CloseHandle(s_evHecho);   s_evHecho   = NULL;
}

/* Entre partidas: el hilo esta ocioso (el emulador se paro y vacio).
 *
 * ★ El hilo se CREA AQUI, la primera vez que una partida lo pide, y no en
 * `retro_init`: con la opcion apagada no se crea nunca, y asi el A/B apagado
 * no ejecuta ni una linea de la etapa 5.  Se crea una sola vez por vida del
 * core (no por carga: crear hilos por carga colgaba `CreateThread` en
 * pcsxr-360). */
void hilo_lista_configurar(int si)
{
    if (si && !s_hilo)
    {
#if XBOX64_TRAZAS
        xbox64_log_line("[LISTA] traza: creando el hilo (HW4)...");
#endif
        hilo_lista_crear();
#if XBOX64_TRAZAS
        xbox64_log_line(s_hilo ? "[LISTA] traza: hilo creado"
                               : "[LISTA] traza: CreateThread fallo");
#endif
    }
    s_activo = (si && s_hilo) ? 1 : 0;
#if XBOX64_TRAZAS
    s_trazas = 0;
#endif
    s_colaW = s_colaR = 0;
    s_dlW = s_dlR = 0;
    s_fotoR = 0;
    s_resR = 0;
    s_dpIdx = 0;
    s_tareaConLista = 0;
    s_primera = 1;
    s_porUs = 0;
    s_ultimoTb = 0;
    s_esperaViTb = 0;
    s_dpRetenido = 0;
    s_mi = 0;
    s_sp = 0;
    memset(s_vi, 0, sizeof(s_vi));
    memset(s_dmem, 0, sizeof(s_dmem));
}

/* ====================================================================== */
/* El informe                                                             */
/* ====================================================================== */

/* ★ Como se lee:
 *   1. `listas` es el CONTADOR DE PASO: tiene que salir ~20-30/s en juego.
 *      Con 0 no esta entrando nada y el resto mide el vacio.  `hw` = 3.
 *   2. `espera emu` es lo que el emulador sigue esperando a este hilo.  `dp`
 *      alto = el plazo del DP se queda corto (o el hilo va por detras);
 *      `cola` = cola o escenas llenas.  Lo bueno es que salgan ~0.
 *   3. `sindp`, `infloop`, `excep` y `desb` tienen que salir 0.  `spcamb`
 *      puede no ser 0 (el plugin toca el SP), y se aplica igual.
 * Tiempos en us por segundo. */
void hilo_lista_informe(unsigned frames)
{
#if !XBOX64_TRAZAS
    (void)frames;   /* version publica: sin informe ni medidas */
#else
    char l[320];
    (void)frames;

    if (!s_activo)
        return;

    _snprintf(l, sizeof(l),
              "[LISTA] hw=%u listas=%u (med %u us, max %u) ordenes=%u | "
              "espera emu: dp=%u (%u) cola=%u (%u) vaciar=%u | deuda max=%u",
              s_stHw, s_stListas,
              s_stListas ? (s_stListaTb / 50u) / s_stListas : 0u,
              s_stListaMax / 50u, s_stOrdenes,
              s_stDpTb / 50u, s_stDpN, s_stColaTb / 50u, s_stColaN,
              s_stVacTb / 50u, s_stDeudaMax);
    l[sizeof(l) - 1] = 0;
    xbox64_log_line(l);

    _snprintf(l, sizeof(l),
              "[LISTA2] plazo med=%u count/us=%u | pisadas=%u/%u sp=%s reintentos=%u"
              " spespera=%u us (%u) sptope=%u"
              " cede=%u (hecha %u, tarde %u) reanuda=%u (sin %u) dpret=%u dpsp=%u | "
              "sindp=%u infloop=%u spcamb=%u desb=%u excep=%u (cod %08X)",
              s_stPlazoN ? s_stPlazoSum / s_stPlazoN : 0u, s_porUs,
              s_stPisadas, s_stFirmadas,
              s_spAlAcabar == 2 ? "foto" : (s_spAlAcabar ? "al acabar" : "en el acto"),
              s_stSpN, s_stSpEspTb / 50u, s_stSpEspN, s_stSpTope,
              s_stCede, s_stCedeHecha, s_stCedeTarde,
              s_stReanuda, s_stReanudaSin, s_stDpRet, s_stDpSp,
              s_stSinDp, s_stInfloop, s_stSpCamb, s_stDeudaDesb,
              s_stExcep, s_stExcepCod);
    l[sizeof(l) - 1] = 0;
    xbox64_log_line(l);

    /* [Cruis'n USA] La foto: `copias` es el contador de paso (tiene que salir
     * igual que `listas`); el coste, en us por copia. */
    if (s_spAlAcabar == 2)
    {
        _snprintf(l, sizeof(l),
                  "[LISTA3] foto de la RDRAM: copias=%u de %u KB (med %u us, max %u)"
                  " | dpantes=%u",
                  s_stFotoN, (unsigned int)(sizeof(rdram) / 1024u),
                  s_stFotoN ? (s_stFotoTb / 50u) / s_stFotoN : 0u,
                  s_stFotoMax / 50u, s_stDpAntes);
        l[sizeof(l) - 1] = 0;
        xbox64_log_line(l);
    }
    s_stFotoN = s_stFotoTb = s_stFotoMax = 0;
    s_stDpAntes = 0;

    s_stListas = s_stListaTb = s_stListaMax = 0;
    s_stOrdenes = 0;
    s_stDpTb = s_stDpN = 0;
    s_stColaTb = s_stColaN = 0;
    s_stVacTb = s_stVacN = 0;
    s_stDeudaMax = 0;
    s_stSinDp = s_stSpCamb = 0;
    s_stPlazoSum = s_stPlazoN = 0;
    s_stInfloop = 0;
    s_stPisadas = s_stFirmadas = 0;
    s_stSpTb = s_stSpN = 0;
    s_stSpEspTb = s_stSpEspN = 0;
    s_stSpTope = 0;
    s_stCede = s_stReanuda = 0;
    s_stCedeHecha = s_stCedeTarde = s_stDpRet = s_stDpSp = 0;
    /* `excep`, su codigo, `desb` y `sin` NO se ponen a cero: si pasan una vez,
     * tienen que seguir viendose. */
#endif /* XBOX64_TRAZAS */
}

#else  /* D3D_GRABADOR < 3: sin cola de ordenes este hilo no puede grabar. */

int  hilo_lista_crear(void)                 { return 0; }
void hilo_lista_destruir(void)              { }
void hilo_lista_configurar(int si)          { (void)si; }
int  hilo_lista_activo(void)                { return 0; }
int  hilo_lista_soy_yo(void)                { return 0; }
static unsigned char s_dmemNulo[HILO_LISTA_DMEM_BYTES];
static unsigned long s_regNulo[HL_VI_N + 2];
unsigned char *hilo_lista_dmem(void)        { return s_dmemNulo; }
unsigned long *hilo_lista_mi(void)          { return &s_regNulo[HL_VI_N]; }
unsigned long *hilo_lista_sp(void)          { return &s_regNulo[HL_VI_N + 1]; }
unsigned long *hilo_lista_vi(int i)         { (void)i; return &s_regNulo[0]; }
void hilo_lista_check_interupt(void)        { check_interupt(); }
int  hilo_lista_encolar(int que)            { (void)que; return 0; }
void hilo_lista_frame_begin(void)           { }
void hilo_lista_frame_end(void)             { }
void hilo_lista_swap_begin(void)            { }
void hilo_lista_vaciar(void)                { }
void hilo_lista_anota_vi(unsigned int us)   { (void)us; }
int  hilo_lista_dp_programar(int hay_dp)    { (void)hay_dp; return 0; }
unsigned hilo_lista_plazo_dp(void)          { return 4000; }
int  hilo_lista_dp_vence(void)              { return 1; }
int  hilo_lista_dp_quedan(void)             { return 0; }
void hilo_lista_olvidar_dp(void)            { }
void hilo_lista_sp_db(int modo)             { (void)modo; }
void hilo_lista_informe(unsigned frames)    { (void)frames; }
void hilo_lista_sp_al_acabar(int si)        { (void)si; }
unsigned hilo_lista_sp_plazo(unsigned normal) { return normal; }
int  hilo_lista_sp_vence(void)              { return 1; }
unsigned hilo_lista_sp_reintento(void)      { return 1000; }
int  hilo_lista_sp_ceder(void)              { return 0; }
int  hilo_lista_sp_reanudar(void)           { return 0; }
void hilo_lista_traza_estado(const char *por) { (void)por; }

#endif /* D3D_GRABADOR >= 3 */
