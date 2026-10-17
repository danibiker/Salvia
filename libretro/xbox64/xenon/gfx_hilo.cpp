/*
 * gfx_hilo.cpp  --  implementacion.  El invariante y el porque, en gfx_hilo.h.
 */

#include <xtl.h>
#include <string.h>
#include <ppcintrinsics.h>      /* __lwsync, __mftb: modelo de memoria debil */

#include "gfx_hilo.h"
#include "perfil_cap.h"
/* Trae `sp_register` y `vi_register`.  Tiene sus propias guardas `extern "C"`,
 * asi que va fuera de las mias. */
#include "../source/gc_memory/memory.h"

extern "C" {
int  xbox64_gfx_hilo_opcion(void);          /* xbox/plataforma.c  */
/* ★ POR EL DESPACHADOR, NO POR glN64.  Preguntar aqui
 * `gln64_ucode_apto_para_hilo()` a secas dejaba a Rice fuera del hilo en
 * TODAS las listas: su `gln64_ucode_tipo` no lo escribe nadie y se queda en
 * -1.  El porque completo esta en xbox/gfx_dispatch.c. */
int  xbox64_gfx_ucode_apto_para_hilo(void); /* xbox/gfx_dispatch.c */
}

/* Inyectados por xbox/plataforma.c; ver la nota en la cabecera. */
static void (*s_ej_dlist)(void)  = NULL;
static void (*s_ej_updscr)(void) = NULL;

void gfx_hilo_set_ejecutores(void (*dlist)(void), void (*updscr)(void))
{
    s_ej_dlist  = dlist;
    s_ej_updscr = updscr;
}

/* ====================================================================== */
/* Estado                                                                 */
/* ====================================================================== */

/* Alineada a linea de cache del Xenon: el memcpy del productor y las
 * lecturas del consumidor no comparten linea con nada mas.
 *
 * ★★★ ESTA ES LA SOMBRA VIVA Y SU DIRECCION NO PUEDE MOVERSE.
 *
 * `gfx_info.DMEM` apunta aqui, y el plugin SE QUEDA CON EL PUNTERO en su
 * `initiateGFX` -- igual que hace con el SP en
 * source/glN64_GX/glN64.cpp:298.  O sea que el doble bufer NO puede ser
 * "dos sombras y alternar la direccion": el plugin seguiria leyendo la
 * primera y no se enteraria nunca. */
static __declspec(align(128)) unsigned char s_dmem[GFX_HILO_DMEM_BYTES];

/* ====================================================================== *
 * ★★★ LAS ESCENAS: el doble bufer, puesto donde SI se puede.
 *
 * EL DEFECTO QUE ARREGLAN, MEDIDO.  El productor escribia antes en `s_dmem`
 * directamente, y por eso tenia que vallar primero: si habia una lista en
 * vuelo estaba leyendo esa misma memoria (la nota larga de
 * xbox/plataforma.c lo decia).  En Majora's Mask, log 11:
 *
 *     [GFXTH] trabajo=4870 (80/s) espera=3508 (20/s)
 *
 * `espera` sale de `gfxh_wait_tb`, que SOLO se incrementa aqui abajo en
 * `gfx_hilo_vallar`, o sea en el hilo de EMULACION.  3508 us/frame x 60 / 20
 * = 10,5 ms de hilo de emulacion parado en seco por cada lista, una por
 * lista sin excepcion -- mientras el hilo grafico trabajaba 292 ms de cada
 * 1000 y estaba ocioso los otros 708.
 *
 * COMO.  El productor escribe en una escena, que no la lee nadie, y es el
 * CONSUMIDOR quien la vuelca a `s_dmem` justo antes de ejecutar su lista.
 * La direccion que cacheo el plugin no se mueve y el productor no espera.
 * Cuesta un memcpy de 4 KB por lista en el nucleo 1: a 20 listas/s, 80 KB/s.
 *
 * Con dos escenas la profundidad es 2 y solo se valla cuando las dos estan
 * ocupadas.  Mas escenas no compran nada: la precondicion del `DP_INT` de
 * `gfx_hilo_encolar_dlist` ya manda sincrona la lista que llegue con un DP
 * en la cola, asi que 2 es el techo real.
 * ====================================================================== */
#define ESCENAS      2
#define ESCENA_MASK  (ESCENAS - 1)
static __declspec(align(128)) unsigned char s_escena[ESCENAS][GFX_HILO_DMEM_BYTES];

/* Las dos ventanillas.  Ver "el registro de resultado" en la cabecera. */
volatile unsigned int gfxh_mi_priv = 0;
volatile unsigned int gfxh_sp_priv = 0;

#define CMD_DLIST   1
#define CMD_UPDSCR  2

/* Ocho ranuras, potencia de dos.  Solo puede haber UNA lista en vuelo, asi
 * que sobran; el tamano esta para que el enmascarado sea un AND. */
#define RANURAS      8
#define RANURA_MASK  (RANURAS - 1)

static volatile unsigned char s_cmd[RANURAS];
static volatile unsigned int  s_cola_w = 0;   /* solo lo escribe el productor */
static volatile unsigned int  s_cola_r = 0;   /* solo lo escribe el consumidor */

static HANDLE s_hilo    = NULL;
static DWORD  s_hiloId  = 0;
static HANDLE s_ev_work = NULL;
static HANDLE s_ev_done = NULL;
static volatile int s_salir = 0;
static volatile unsigned s_hw = 0;            /* HW thread real del consumidor */

/* ★★ EL REGISTRO DE RESULTADO, AHORA POR ESCENA.
 *
 * Antes era UNA copia, porque solo podia haber una lista en vuelo.  Con
 * profundidad 2 eso seria corrupcion silenciosa: el productor escribia
 * `gfxh_sp_priv = semilla` al despachar, y con la lista anterior todavia
 * corriendo eso le cambiaba el registro a mitad de recorrido -- la misma
 * familia de fallo que el comentario del OSTask.
 *
 * Por eso la semilla la pone el CONSUMIDOR (no el productor) justo antes de
 * ejecutar, y recoge el resultado justo despues.  `gfxh_sp_priv` y
 * `gfxh_mi_priv` siguen siendo una sola palabra cada una porque el plugin
 * cacheo SUS punteros; lo que se duplica es la entrada y la salida.
 *
 * Tres cursores, cada uno con UN solo escritor:
 *   s_dl_w   productor  -- listas despachadas
 *   s_dl_r   consumidor -- listas ejecutadas
 *   s_res_r  productor  -- resultados ya aplicados a sp_register
 */
static volatile unsigned int s_dl_w  = 0;
static volatile unsigned int s_dl_r  = 0;
static unsigned int          s_res_r = 0;

static unsigned int          s_sem[ESCENAS];     /* la escribe el productor  */
static volatile unsigned int s_sp_out[ESCENAS];  /* las escribe el consumidor*/
static volatile unsigned int s_mi_out[ESCENAS];

static int          s_dp_pendiente   = 0;     /* falta programar su DP_INT */
/* Coste de la lista anterior, del que sale el plazo del DP.  `volatile`
 * porque lo escribe el consumidor y lo lee el productor; no hace falta mas
 * que eso -- una palabra alineada, y un valor rancio solo da un plazo algo
 * peor, nunca una incoherencia. */
static volatile unsigned int s_ultimo_tb = 0;
static volatile unsigned s_incompletas = 0;
static volatile unsigned s_sin_dp      = 0;   /* ver la precondicion del DP */

static int s_primera = 1;   /* la primera lista de la sesion va sincrona */

volatile unsigned int gfxh_work_tb = 0, gfxh_work_n = 0, gfxh_work_max = 0;

/* `Count` por microsegundo de tiempo REAL de emulacion.  Lo mide y lo
 * publica `resumen_por_segundo`; 0 = todavia sin medida.  Ver gfx_hilo.h. */
volatile unsigned int gfxh_count_por_us = 0;
volatile unsigned int gfxh_wait_tb = 0, gfxh_wait_n = 0;

/* ★ SONDAS DEL DOBLE BUFER.  Contador de paso ANTES que el valor, como
 * siempre: `vuelo2` dice cuantas veces se llego a tener DOS listas en vuelo
 * -- si sale 0, la profundidad 2 no se esta usando y cualquier mejora de
 * `espera` viene de otro sitio.  `cola2` cuenta las vallas que SI tuvieron
 * que esperar por escenas llenas: es el resto irreducible.
 *
 * `spcamb` es la que falsa el diseno.  Con profundidad 2 la semilla de la
 * lista N+1 se toma de un `sp_register` al que todavia puede faltarle el
 * resultado de la N.  El argumento para que eso sea inocuo es que un ucode
 * con yield no llega al hilo (`xbox64_gfx_ucode_apto_para_hilo`) y que el
 * resto de plugins no tocan el SP dentro de la lista, o sea `cambiados==0`
 * SIEMPRE.  Si `spcamb` sale distinto de cero, el argumento es falso y hay
 * que volver a profundidad 1. */
volatile unsigned int gfxh_vuelo2 = 0, gfxh_cola2 = 0, gfxh_sp_cambios = 0;

/* Las otras dos vallas, separadas del par historico.  Ver `vallar_en`. */
volatile unsigned int gfxh_wmarco_tb = 0, gfxh_wmarco_n = 0;  /* fin de marco */
volatile unsigned int gfxh_wesc_tb   = 0, gfxh_wesc_n   = 0;  /* escenas llenas */

/* ====================================================================== */
/* Utilidades                                                             */
/* ====================================================================== */

int gfx_hilo_soy_yo(void)
{
    return (s_hiloId != 0) && (GetCurrentThreadId() == s_hiloId);
}

unsigned char *gfx_hilo_dmem(void)   { return s_dmem; }
unsigned gfx_hilo_hw(void)           { return s_hw; }
unsigned gfx_hilo_incompletas(void)  { return s_incompletas; }
unsigned gfx_hilo_sin_dp(void)       { return s_sin_dp; }

static int cola_vacia(void) { return s_cola_r == s_cola_w; }

static int hilo_habilitado(void)
{
    return (s_hilo != NULL) && !gfx_hilo_soy_yo() && xbox64_gfx_hilo_opcion();
}

/* ====================================================================== */
/* El consumidor                                                          */
/* ====================================================================== */

static void ejecutar(unsigned char cmd)
{
    /* ★ `__try/__except` y no por adorno: si una excepcion mata este hilo, el
     * emulador se queda dentro de `gfx_hilo_vallar` esperando una bandera que
     * ya no baja nadie -- cuelgue mudo, sin traza y sin pista. */
    __try
    {
        if (cmd == CMD_DLIST)
        {
            if (s_ej_dlist) s_ej_dlist();
        }
        else if (cmd == CMD_UPDSCR)
        {
            if (s_ej_updscr) s_ej_updscr();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        /* Se traga y se sigue: mejor un frame roto que la consola colgada. */
    }
}

static DWORD WINAPI hilo_proc(LPVOID)
{
    /* ★ Se registra ESTE hilo como el unico que contabiliza `/callcap`.
     * Tiene que ser aqui dentro y no en `gfx_hilo_init`: lo que necesita el
     * perfilador es el id del hilo que EJECUTA, no el del que lo creo.  Ver
     * xenon/perfil_cap.h. */
    perfil_cap_marcar_hilo();

    for (;;)
    {
        WaitForSingleObject(s_ev_work, 100);

        if (s_salir)
            break;

        /* El HW thread REAL, no el que pedimos.  Ver la nota de
         * `gfx_hilo_init`: el contador global de SDL puede haberle dado el
         * HW2 a otro. */
        s_hw = (unsigned)GetCurrentProcessorNumber();

        /* ★ El bucle comprueba la CONDICION, no cuenta senales.  Los eventos
         * son auto-reset: si llegan dos avisos antes de despertar se pierde
         * uno, y contarlos dejaria trabajo sin drenar para siempre. */
        while (s_cola_r != s_cola_w)
        {
            const unsigned idx = s_cola_r & RANURA_MASK;
            const unsigned char cmd = s_cmd[idx];
            const unsigned esc = s_dl_r & ESCENA_MASK;
            unsigned long long t0;

            /* ★★ EL VUELCO DE LA ESCENA, Y LA SIEMBRA DE LA VENTANILLA.
             *
             * Las dos cosas van AQUI y no en el productor, y ese es todo el
             * cambio: mientras esto no ocurre, `s_dmem` sigue siendo de la
             * lista anterior y el productor puede escribir su escena sin
             * pisar a nadie.  Ver la nota de `s_escena`. */
            if (cmd == CMD_DLIST)
            {
                memcpy(s_dmem, s_escena[esc], GFX_HILO_DMEM_BYTES);
                gfxh_sp_priv = s_sem[esc];
                gfxh_mi_priv = 0;
            }

            t0 = __mftb();
            ejecutar(cmd);

            {
                const unsigned int d = (unsigned int)(__mftb() - t0);
                gfxh_work_tb += d;
                if (d > gfxh_work_max) gfxh_work_max = d;
                if (cmd == CMD_DLIST) s_ultimo_tb = d;
            }
            gfxh_work_n++;

            /* El resultado, a su escena.  El avance de `s_dl_r` va detras de
             * la barrera de abajo, que es la que lo publica todo junto. */
            if (cmd == CMD_DLIST)
            {
                s_sp_out[esc] = gfxh_sp_priv;
                s_mi_out[esc] = gfxh_mi_priv;
                __lwsync();
                s_dl_r = s_dl_r + 1;
            }

            /* Publicar el avance DESPUES de que el trabajo sea visible. */
            __lwsync();
            s_cola_r = s_cola_r + 1;
            SetEvent(s_ev_done);
        }
    }
    return 0;
}

/* ====================================================================== */
/* El productor                                                           */
/* ====================================================================== */

static void encolar(unsigned char cmd)
{
    const unsigned idx = s_cola_w & RANURA_MASK;
    s_cmd[idx] = cmd;

    /* El comando tiene que ser visible ANTES que el cursor. */
    __lwsync();
    s_cola_w = s_cola_w + 1;
    SetEvent(s_ev_work);
}

/* =================================================================== *
 * ★★★ APLICAR EL REGISTRO DE RESULTADO.  Es el UNICO sitio en el que algo
 * que produjo el hilo llega a los registros del emulador.
 *
 * SIN BLOQUEAR, y eso es lo nuevo: antes esto vivia dentro de la valla, asi
 * que cosechar y esperar eran la misma cosa.  Ahora se cosecha lo que ya
 * esta hecho y se sigue.
 * =================================================================== */
void gfx_hilo_cosechar(void)
{
    if (!s_hilo || gfx_hilo_soy_yo())
        return;

    while (s_res_r != s_dl_r)
    {
        const unsigned esc = s_res_r & ESCENA_MASK;

        /* Lo que el consumidor escribio ya es visible: la barrera la puso el
         * antes de mover `s_dl_r`, y `s_dl_r` se acaba de leer. */
        __lwsync();

        /* ★ SOLO LOS BITS QUE EL PLUGIN CAMBIO.
         *
         * Copiar la palabra entera fue un fallo que costo un build (log 4):
         * entre el despacho y la cosecha pasan hasta 13 ms de emulacion, y
         * por ahi corren tareas de audio que pasan por el mismo
         * `do_SP_task` y dejan `sp_status` como debe estar.  La foto previa
         * lo borraba y el apreton de manos del SP se quedaba colgado -- el
         * audio seguia a 60/s y el invitado dejaba de mandar listas para
         * siempre.
         *
         * Con la mascara de cambios, lo que el hilo de emulacion hizo entre
         * medias se respeta, y lo que el plugin decidio tambien. */
        {
            const unsigned int cambiados = s_sp_out[esc] ^ s_sem[esc];
            if (cambiados)
            {
                gfxh_sp_cambios++;
                sp_register.sp_status_reg =
                    (sp_register.sp_status_reg & ~cambiados) |
                    (s_sp_out[esc] & cambiados);
            }
        }

        /* El `MI_INTR_DP` NO se entrega aqui: lo entrega el evento `DP_INT`
         * que `do_SP_task` programo al despachar.  Esto solo cuenta las
         * listas que no llegaron a `gDPFullSync`, que tienen que ser cero. */
        if (!(s_mi_out[esc] & MI_INTR_DP))
            s_incompletas++;

        s_res_r = s_res_r + 1;
    }
}

/* ★★★ LA VALLA, CON EL SITIO QUE LA PIDE.
 *
 * Contar todas las esperas en el mismo par de contadores era suficiente
 * mientras solo habia una valla que importase.  Ya no: al quitar la del
 * despacho queda la del FIN DE MARCO (360/Xdk/xbox64/libretro_core.cpp,
 * justo antes de `gxd3d_frame_end`), que drena la cola una vez por
 * `retro_run` y cuyo propio comentario dice que es "el techo del solape".
 * Con listas de 14-23 ms y un marco de 16,7 ms, esa valla puede absorber
 * entera la espera que se acaba de quitar -- y con un solo contador el log
 * diria "no ha mejorado nada" sin decir por que.
 *
 * Tres sitios, tres pares.  El que crezca es el que hay que atacar. */
static void vallar_en(volatile unsigned int *tb, volatile unsigned int *n)
{
    if (!s_hilo || gfx_hilo_soy_yo())
        return;

    if (!cola_vacia())
    {
        const unsigned long long t0 = __mftb();
        while (!cola_vacia())
            WaitForSingleObject(s_ev_done, 100);
        *tb += (unsigned int)(__mftb() - t0);
        *n  += 1;
    }
    /* Lo que el hilo escribio ya es visible: la barrera la puso el antes de
     * mover su cursor. */
    __lwsync();

    /* La cola esta vacia, asi que esto cosecha TODAS las listas que hubiera
     * en vuelo -- con profundidad 2 pueden ser dos, y van en orden. */
    gfx_hilo_cosechar();
}

/* Todos los sitios de siempre: `do_SP_task`, el `DP_INT`, los savestates y
 * el descargado de la ROM. */
void gfx_hilo_vallar(void)
{
    vallar_en(&gfxh_wait_tb, &gfxh_wait_n);
}

/* ★ El fin de `retro_run`, y SOLO ese.  Se separa porque es el techo del
 * solape: mientras esta valla exista, una lista no puede repartirse entre
 * dos marcos del frontend por mucho doble bufer de escena que haya. */
void gfx_hilo_vallar_marco(void)
{
    vallar_en(&gfxh_wmarco_tb, &gfxh_wmarco_n);
}

int gfx_hilo_encolar_dlist(int dp_libre)
{
    if (!hilo_habilitado())
        return 0;

    /* ★★★ PRECONDICION: NO PUEDE HABER YA UN `DP_INT` EN LA COLA.
     *
     * Y no es prudencia: `add_interupt_event_count` (source/r4300/interupt.c)
     * tiene esto, literalmente, de upstream:
     *
     *     if (get_event(type)) {
     *         // two events of same type in queue
     *     }
     *
     * Un `if` VACIO.  Detecta el duplicado y no hace nada con el.  Con el
     * `+4000` original eso no importaba -- el DP vencia en ~85 us, mucho
     * antes de la lista siguiente --, pero al alargar el plazo se abre la
     * ventana, y dos eventos del mismo tipo dejan uno con el `count`
     * atrasado.  A partir de ahi `r4300.next_interrupt` se queda en el
     * pasado y `go()` vuelve al instante.
     *
     * MEDIDO (log 9, Conker): 21 segundos con `emu=52 us`, `vueltas=60`,
     * `viupd=60` -- el VI puntual -- y `keys=0`.  El invitado ejecutaba unas
     * 3.200 instrucciones por frame en vez de 390.000.  Se recuperaba solo
     * cuando `Count` alcanzaba el evento rancio.
     *
     * Es la misma familia que este arbol ya tiene anotada dos veces:
     * [[project_xenon64_centinela_next_interupt]] y
     * [[project_xenon64_giro_count_next_vi]].
     *
     * Si pasa, la lista va SINCRONA: el camino de siempre programa su DP a
     * +4000 y no hay duplicado posible.  `sindp` dice cuantas veces. */
    if (!dp_libre)
    {
        s_sin_dp++;
        return 0;
    }

    /* La primera lista de la sesion, sincrona: la deteccion de ucode de
     * `hle_execute` y el `GBI_Init` que la acompana se hacen en serie. */
    if (s_primera)
    {
        s_primera = 0;
        return 0;
    }

    /* Los ucodes que hacen yield: ver gln64_glue.cpp.  Es una comprobacion
     * de capacidad, no un parche -- y el registro de resultado hace que, si
     * alguno se cuela, su reencolado se aplique igual, solo que tarde. */
    if (!xbox64_gfx_ucode_apto_para_hilo())
        return 0;

    /* ★★★ AQUI ESTABA LA VALLA Y AQUI ES DONDE SE QUITA.
     *
     * Decia: "Profundidad 1: el invitado no manda otra lista hasta recibir
     * su SP_INT, y dos recorriendo RDRAM viva no compran nada."  La primera
     * mitad dejo de ser cierta el dia que se ADELANTO el SP_INT para que el
     * invitado siguiera corriendo mientras el nucleo 1 dibujaba: desde
     * entonces manda la siguiente lista enseguida y se la encontraba
     * esperando aqui 10,5 ms.  La segunda mitad tampoco: no son "dos
     * recorriendo RDRAM", sigue habiendo un solo lector.
     *
     * Cosechar ANTES de tomar la semilla y no despues: si no, la semilla de
     * esta lista saldria de un `sp_register` al que todavia le falta
     * aplicar el resultado de la anterior. */
    gfx_hilo_cosechar();

    /* La unica espera que queda, y por la unica razon real que habia:
     * escribir una escena que el consumidor todavia no ha volcado. */
    if ((unsigned int)(s_dl_w - s_dl_r) >= ESCENAS)
    {
        gfxh_cola2++;
        vallar_en(&gfxh_wesc_tb, &gfxh_wesc_n);
    }
    else if (s_dl_w != s_dl_r)
        gfxh_vuelo2++;

    {
        const unsigned esc = s_dl_w & ESCENA_MASK;
        memcpy(s_escena[esc], SP_DMEM, GFX_HILO_DMEM_BYTES);
        s_sem[esc] = sp_register.sp_status_reg;
    }
    s_dl_w = s_dl_w + 1;

    s_dp_pendiente = 1;
    encolar(CMD_DLIST);
    return 1;
}

int gfx_hilo_encolar_updscr(void)
{
    if (!hilo_habilitado())
        return 0;

    encolar(CMD_UPDSCR);
    return 1;
}

/* ====================================================================== */
/* El plazo del DP                                                        */
/* ====================================================================== */

int gfx_hilo_tomar_pendiente(void)
{
    const int p = s_dp_pendiente;
    s_dp_pendiente = 0;
    return p;
}

unsigned gfx_hilo_plazo_dp(void)
{
    unsigned long vd = vi_register.vi_delay;
    unsigned int  us, plazo, por_us;

    if (vd == 0)
        return 4000;            /* aun sin VI programado: como el original */

    /* ★★★ CUANTO AVANZA `Count` POR MICROSEGUNDO DE RELOJ REAL.
     *
     * Esto decia `vd / 16667`, o sea "un frame de Count entre los 16.667 us
     * que dura un frame".  Y ESO ES FALSO DENTRO DE `retro_run`: los 16.667
     * us son el ritmo que impone el FRONTEND desde fuera, pero el hilo de
     * emulacion recorre un periodo de VI en `emu` microsegundos -- unos
     * 5.500 en Mario 64 -- y el resto del frame esta dormido.
     *
     * Con el divisor 3x mas grande de la cuenta, `por_us` salia 3x mas
     * pequenyo y el plazo 3x mas corto: el `DP_INT` vencia mucho antes de
     * que la lista acabara y la valla se comia la diferencia.
     *
     * MEDIDO en Majora's Mask: la ventana de solape es de 10 ms
     * (`[SOLAPE] med=10006`) y la lista cuesta 6,4 ms -- CABE ENTERA -- y
     * aun asi `[GFXTH] espera` era de 3,6 ms por lista.
     *
     * ★ EL FACTOR SE MIDE, NO SE SUPONE: lo publica `resumen_por_segundo`
     * a partir del `emu` real DESCONTADA LA ESPERA.  Descontarla es
     * imprescindible: durante la valla el reloj corre y `Count` no, asi que
     * incluirla inflaria el divisor, encogeria el plazo y realimentaria
     * justo el problema que se intenta arreglar.
     *
     * Si todavia no hay medida (primer segundo), se usa el valor de antes:
     * es conservador -- plazo corto, alguna espera-- y nunca incorrecto,
     * porque quien garantiza la correccion es la valla, no el plazo. */
    por_us = gfxh_count_por_us;
    if (por_us == 0)
        por_us = (unsigned int)(vd / 16667u);
    if (por_us == 0)
        por_us = 1;

    /* El timebase del Xenon va a ~49,875 MHz: ticks / 50 = microsegundos. */
    us = s_ultimo_tb / 50u;

    plazo = us * por_us;

    /* Nunca ANTES de lo que el juego espera hoy, nunca DESPUES de un frame:
     * un RDP real no tarda mas que eso.
     *
     * ★★ EL TOPE ES UN FRAME, Y BAJARLO A MEDIO SALIO CARO.  Lo probe como
     * cinturon contra el `DP_INT` duplicado y fue una regresion limpia
     * (log 10, Majora's Mask, misma escena):
     *
     *              tope = vi_delay      tope = vi_delay/2
     *   emu           7.651 us             13.663 us
     *   espera        5.290                 5.250    <- IDENTICA
     *   entrega          46                    72    <- no se movio de tramo
     *   dt            16,63                16,6-22,17
     *
     * La espera no cambio, o sea que los 6 ms de mas NO son el hilo
     * esperando: es el invitado, al que adelantarle el DP medio frame le
     * descoloca el reparto de su propio trabajo.  No tengo el mecanismo
     * exacto, pero la relacion causa-efecto es limpia: una sola variable.
     *
     * Y el cinturon sobraba: contra el duplicado esta la PRECONDICION de
     * `gfx_hilo_encolar_dlist`, que comprueba la condicion de verdad
     * (`get_event(DP_INT)`) en vez de intentar hacerla improbable. */
    if (plazo < 4000u)  plazo = 4000u;
    if (plazo > (unsigned int)vd) plazo = (unsigned int)vd;
    return plazo;
}

/* ====================================================================== */
/* Ciclo de vida                                                          */
/* ====================================================================== */

int gfx_hilo_init(void)
{
    if (s_hilo)
        return 1;                   /* idempotente */

    s_salir  = 0;
    s_cola_w = s_cola_r = 0;

    s_ev_work = CreateEvent(NULL, FALSE, FALSE, NULL);   /* auto-reset */
    s_ev_done = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!s_ev_work || !s_ev_done)
    {
        if (s_ev_work) { CloseHandle(s_ev_work); s_ev_work = NULL; }
        if (s_ev_done) { CloseHandle(s_ev_done); s_ev_done = NULL; }
        return 0;
    }

    /* Pila de 256 KB: la referencia usaba 128 con Rice, y `TextureCache_Load`
     * de glN64 no esta auditado. */
    s_hilo = CreateThread(NULL, 256 * 1024, hilo_proc, NULL,
                          CREATE_SUSPENDED, &s_hiloId);
    if (!s_hilo)
    {
        CloseHandle(s_ev_work); s_ev_work = NULL;
        CloseHandle(s_ev_done); s_ev_done = NULL;
        s_hiloId = 0;
        return 0;
    }

    /* HW2 = nucleo 1, que Salvia deja libre (su bucle va a CPU_THREAD=1).
     * HW0 y HW3 estan vetados: hermanos SMT del emulador y de este mismo.
     * Si sale otro numero en `gfx_hilo_hw()`, alguien nos lo quito. */
    SetThreadPriority(s_hilo, THREAD_PRIORITY_NORMAL);
    XSetThreadProcessor(s_hilo, 2);
    ResumeThread(s_hilo);
    return 1;
}

void gfx_hilo_shutdown(void)
{
    if (!s_hilo)
        return;

    gfx_hilo_vallar();

    s_salir = 1;
    __lwsync();
    SetEvent(s_ev_work);
    WaitForSingleObject(s_hilo, 2000);

    CloseHandle(s_hilo);    s_hilo = NULL;  s_hiloId = 0;
    CloseHandle(s_ev_work); s_ev_work = NULL;
    CloseHandle(s_ev_done); s_ev_done = NULL;
}

void gfx_hilo_rom_nueva(void)
{
    gfx_hilo_vallar();
    s_primera        = 1;
    s_dp_pendiente   = 0;
    s_ultimo_tb      = 0;

    /* Los tres cursores a la vez.  `gfx_hilo_vallar` de arriba ya dejo la
     * cola vacia y todo cosechado, asi que aqui no se pierde nada. */
    s_dl_w = s_dl_r = 0;
    s_res_r = 0;
}
