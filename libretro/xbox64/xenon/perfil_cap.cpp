/*
 * xenon/perfil_cap.cpp  --  los dos ganchos de /callcap y su contabilidad.
 *
 * La justificacion entera (por que no un muestreador de PC, que ofrece y que
 * no ofrece el XDK en una consola sin devkit) esta en perfil_cap.h.  Aqui
 * solo la mecanica.
 */

#include <xtl.h>
#include <string.h>
#include <stdio.h>
#include <ppcintrinsics.h>      /* __mftb */
#include <rtcapi.h>             /* _ReturnAddress (rtcapi.h:147) */

#pragma intrinsic(_ReturnAddress)

#include "perfil_cap.h"

extern "C" void DebugOutputString(const char *s);

#if XBOX64_PERFIL_CAP

/* Cada cuantos segundos se vuelca.  Un segundo da pocas muestras y el
 * ranking baila; cuatro lo estabiliza y no ahoga el log. */
#define PERFIL_CAP_SEGUNDOS  4

/* Funciones distintas que caben.  Rice entero son unos cientos. */
#define PC_BITS   11
#define PC_TAM    (1u << PC_BITS)
#define PC_MASK   (PC_TAM - 1u)

/* Profundidad de la pila en sombra.  El parser de display lists de Rice
 * anida poco; 96 va sobrado y el desbordamiento se CUENTA en vez de
 * corromper memoria. */
#define PC_PILA   96

typedef struct {
    unsigned int  fn;        /* direccion de la funcion; 0 = ranura libre */
    unsigned int  llamadas;
    unsigned int  tb_total;  /* con hijos            */
    unsigned int  tb_propio; /* sin hijos            */
} pc_entrada;

static pc_entrada s_tabla[PC_TAM];
static unsigned   s_usadas   = 0;
static unsigned   s_colisiones = 0;   /* no cupo: la tabla se queda corta */

/* Pila en sombra.  `hijos` acumula lo que gastaron los descendientes del
 * marco, que es lo que hay que restar para el tiempo propio. */
typedef struct {
    unsigned int fn;
    unsigned int t0;
    unsigned int hijos;
} pc_marco;

static pc_marco     s_pila[PC_PILA];
static int          s_prof      = 0;
static unsigned     s_desborde  = 0;   /* entradas por encima de PC_PILA   */
static unsigned     s_huerfanas = 0;   /* salidas sin entrada              */

/* ★ EL FILTRO POR HILO.  Ver la nota de la cabecera: la pila en sombra
 * tiene UN escritor.  Mismo criterio que `gfx_hilo_soy_yo`. */
static volatile DWORD s_tid = 0;

/* ★★ CONTADOR DE PASO, Y VA ANTES DEL FILTRO DE HILO.
 *
 * La primera version solo contaba funciones DESPUES de comprobar el hilo, y
 * eso dejo `fn=0` sin interpretacion posible: valia igual para "el
 * compilador no esta emitiendo las llamadas" que para "se llaman desde otro
 * hilo".  Son dos causas con arreglos opuestos -- una es del proyecto y la
 * otra del filtro -- y perdimos un build entero por no poder distinguirlas.
 *
 *   bruto == 0            -> `/callcap` NO esta emitiendo nada.  El problema
 *                            esta en el vcxproj o en el compilador (mirar si
 *                            el build cantó D9002 "ignoring unknown option").
 *   bruto > 0 y fn == 0   -> las llamadas llegan, pero en un hilo que no es
 *                            el que se registro.  El problema es el filtro.
 *
 * Es exactamente [[feedback_sondas_camino_muerto]], y me lo salte. */
static volatile unsigned s_bruto = 0;

static unsigned s_seg = 0;

/* ★ Una direccion conocida para casar el `.map` con la ejecucion: sale en el
 * informe y su pareja esta en el mapa.  MEDIDO en el build del 2026-09-29:
 * las dos coinciden, o sea que NO hay rebase y las direcciones del mapa se
 * pueden usar tal cual.
 *
 * ★★ EL CUERPO NO PUEDE ESTAR VACIO.  La primera version era `{ }` y el
 * enlazador la PLEGO con todas las demas funciones triviales del binario
 * (COMDAT folding: en el mapa salen `write_nothingb`, `ApplyRDPScissor`,
 * `CaptureScreen`... todas en 826c4910, marcadas con la `i` de identical).
 * Un ancla que apunta a media docena de sitios no ancla nada.  Con un
 * contador propio el cuerpo es unico y no se puede plegar. */
static volatile unsigned int s_ancla_marca = 0;
static void pc_ancla(void) { s_ancla_marca += 0x5A171A; }

void perfil_cap_init(void)
{
    memset(s_tabla, 0, sizeof(s_tabla));
    s_usadas = s_colisiones = 0;
    s_prof = 0;
    s_desborde = s_huerfanas = 0;
    s_seg = 0;
}

void perfil_cap_marcar_hilo(void)
{
    s_tid = GetCurrentThreadId();
}

/* Hash abierto con sondeo lineal.  La direccion de una funcion esta alineada
 * a 4, asi que los dos bits bajos no llevan informacion: se descartan antes
 * de mezclar o la mitad de la tabla quedaria vacia. */
static pc_entrada *pc_busca(unsigned int fn)
{
    unsigned h = ((fn >> 2) * 2654435761u) >> (32 - PC_BITS);
    unsigned i;

    for (i = 0; i < 16; i++)
    {
        pc_entrada *e = &s_tabla[(h + i) & PC_MASK];
        if (e->fn == fn)
            return e;
        if (e->fn == 0)
        {
            e->fn = fn;
            s_usadas++;
            return e;
        }
    }
    s_colisiones++;
    return NULL;
}

/* ====================================================================== */
/* Los dos ganchos                                                        */
/* ====================================================================== */
/* ★★ LOS NOMBRES ESTAN MEDIDOS, NO SUPUESTOS.  El primer intento los llamo
 * `_CAP_Start_Profiling` / `_CAP_End_Profiling` y el enlazador contesto:
 *
 *     unresolved external symbol __CAP_Enter_Function
 *     unresolved external symbol __CAP_Exit_Function
 *
 * Y el `.map` confirma las dos cosas que hacian falta saber:
 *   - son DOS subrayados de verdad, no uno mas el adorno del compilador:
 *     `perfil_cap_init` sale en el mapa como `perfil_cap_init`, o sea que
 *     este toolchain NO antepone subrayado a los simbolos `extern "C"`.
 *   - los define **xbdm**:
 *         __CAP_Enter_Function  8271e304  xbdm:xbdm.xex@21256.0+1861.0
 *     Por eso enlazar `xbdm.lib` "arreglaba" el enlace: satisfacia las
 *     llamadas con los ganchos de Microsoft, que alimentan la captura de
 *     PIX -- inutil sin devkit -- y dejaban los mios sin llamar. El sintoma
 *     fue `[CAP] fn=0 propio=0us` con `hilo` distinto de cero: el hilo se
 *     registro y no entro ni una funcion.
 *
 * ★ ASI QUE HAY QUE QUITAR `xbdm.lib` DEL ENLACE.  Con estos dos definidos
 * aqui ya no hace falta, y si se deja puesto sale un LNK2005 por duplicado.
 *
 * ★★ Y LA CLAVE SALE DE `_ReturnAddress()`, NO DE UN ARGUMENTO.  La firma
 * que usa el compilador no esta documentada en ninguna cabecera del XDK
 * (los ganchos los pone el usuario) y declararla mal daria basura en
 * silencio.  La direccion de retorno es un identificador por funcion igual
 * de bueno: apunta DENTRO de la funcion instrumentada, justo detras de la
 * llamada al gancho, asi que es constante para cada una.  Al resolver
 * contra el `.map` hay que quedarse con el simbolo ANTERIOR mas cercano.
 *
 * ★ Declararlos sin parametros es seguro aunque el compilador pase alguno:
 * en PowerPC los argumentos van en registros y no hay limpieza de pila. */
extern "C" void __CAP_Enter_Function(void)
{
    unsigned int clave = (unsigned int)(size_t)_ReturnAddress();
    pc_marco *m;

    s_bruto++;               /* ★ ANTES del filtro.  Ver la nota de s_bruto. */

    if (GetCurrentThreadId() != s_tid)
        return;

    if (s_prof >= PC_PILA)
    {
        s_desborde++;
        s_prof++;              /* se sigue contando para casar la salida */
        return;
    }

    m = &s_pila[s_prof++];
    m->fn    = clave;
    m->hijos = 0;
    /* El reloj SE TOMA EL ULTIMO: todo lo de arriba es coste del gancho y
     * no del codigo medido. */
    m->t0    = (unsigned int)__mftb();
}

/* ★ Aqui NO se usa `_ReturnAddress()` como clave: daria la direccion de
 * detras de la llamada de SALIDA, que es otra distinta de la de entrada.  La
 * clave correcta -- la que se apunto al entrar -- la lleva la pila en
 * sombra, que es justo para lo que esta. */
extern "C" void __CAP_Exit_Function(void)
{
    unsigned int t1 = (unsigned int)__mftb();   /* lo PRIMERO, por lo mismo */
    pc_marco    *m;
    pc_entrada  *e;
    unsigned int total;

    if (GetCurrentThreadId() != s_tid)
        return;

    if (s_prof <= 0)
    {
        /* Puede pasar legitimamente una vez: si el hilo se marco a mitad de
         * una funcion instrumentada, su salida no tiene entrada. */
        s_huerfanas++;
        return;
    }

    s_prof--;
    if (s_prof >= PC_PILA)
        return;                /* era un marco que no cupo */

    m     = &s_pila[s_prof];
    total = t1 - m->t0;

    e = pc_busca(m->fn);
    if (e)
    {
        e->llamadas++;
        e->tb_total  += total;
        e->tb_propio += (total > m->hijos) ? (total - m->hijos) : 0;
    }

    /* Al padre le toca cargar con lo que gasto este marco. */
    if (s_prof > 0 && s_prof - 1 < PC_PILA)
        s_pila[s_prof - 1].hijos += total;
}

/* ====================================================================== */
/* Informe                                                                */
/* ====================================================================== */
/* El time base del Xenon va a 49,875 MHz: ticks/50 ~= us.  Misma conversion
 * que el resto del arbol. */
#define PC_US(tb)  ((tb) / 50u)

/* ★ ESTO CORRE EN EL HILO DE EMULACION Y LA TABLA LA ESCRIBE EL GRAFICO.
 * Es una carrera, y es DELIBERADA: no hay punteros de por medio, lo peor que
 * puede pasar es que una entrada se lea a medio escribir o que el reinicio se
 * lleve por delante unas pocas muestras de esa ventana.  Poner un cerrojo
 * costaria dos operaciones atomicas en CADA entrada y salida de funcion, que
 * es justo lo que este perfilador no puede permitirse: falsearia lo que mide.
 * Un numero con ruido del 0,1 por mil vale; un perfilador que cambia el
 * reparto que esta midiendo, no. */
void perfil_cap_informe(void)
{
    char linea[192];
    unsigned int i, k;
    unsigned int prop_tot = 0;

    if (++s_seg < PERFIL_CAP_SEGUNDOS)
        return;
    s_seg = 0;

    for (i = 0; i < PC_TAM; i++)
        if (s_tabla[i].fn)
            prop_tot += s_tabla[i].tb_propio;

    /* ★ LA CABECERA VA PRIMERO Y LLEVA LOS CONTADORES DE SALUD.  `hilo=0`
     * significa que nadie registro el hilo grafico y TODO lo de abajo es el
     * vacio; `desb` y `huer` distintos de cero dicen que la pila en sombra
     * no cuadra y los tiempos propios no valen.  Sin esta linea, una tabla
     * de ceros no distingue "no cuesta nada" de "no paso nadie". */
    _snprintf(linea, sizeof(linea),
              "[CAP] %us: bruto=%u fn=%u propio=%uus | hilo=%u desb=%u"
              " huer=%u col=%u | ancla=%08X",
              (unsigned)PERFIL_CAP_SEGUNDOS, s_bruto, s_usadas,
              PC_US(prop_tot), (unsigned)s_tid, s_desborde, s_huerfanas,
              s_colisiones, (unsigned int)(size_t)&pc_ancla);
    DebugOutputString(linea);
    s_bruto = 0;

    /* Las 16 mas caras por tiempo PROPIO.  Seleccion parcial: 16 pasadas
     * sobre 2048 ranuras una vez cada cuatro segundos no se nota. */
    for (k = 0; k < 16; k++)
    {
        unsigned int mejor = 0, mx = 0;
        int hay = 0;

        for (i = 0; i < PC_TAM; i++)
        {
            if (!s_tabla[i].fn || s_tabla[i].tb_propio == 0)
                continue;
            if (!hay || s_tabla[i].tb_propio > mx)
            {
                mx = s_tabla[i].tb_propio; mejor = i; hay = 1;
            }
        }
        if (!hay)
            break;

        {
            pc_entrada *e = &s_tabla[mejor];
            /* ★ SIN `%` LITERAL: no sobrevive al volcado del log (medido en
             * gx_d3d9.cpp).  Se dan los us y ya se divide fuera. */
            _snprintf(linea, sizeof(linea),
                      "[CAP] %2u %08X prop=%uus tot=%uus n=%u",
                      k, e->fn, PC_US(e->tb_propio), PC_US(e->tb_total),
                      e->llamadas);
            DebugOutputString(linea);
            e->tb_propio = 0;   /* sacada de la seleccion de esta ronda */
        }
    }

    /* Ventana nueva: se reinicia todo menos las claves, para que el ranking
     * sea comparable entre plugins y entre escenas. */
    for (i = 0; i < PC_TAM; i++)
    {
        s_tabla[i].tb_propio = 0;
        s_tabla[i].tb_total  = 0;
        s_tabla[i].llamadas  = 0;
    }
    s_desborde = s_huerfanas = s_colisiones = 0;
}

#else  /* !XBOX64_PERFIL_CAP */

void perfil_cap_init(void)        { }
void perfil_cap_marcar_hilo(void) { }
void perfil_cap_informe(void)     { }

/* ★★ LOS GANCHOS SE SIGUEN DEFINIENDO, Y VACIOS.
 *
 * Dos razones, y las dos importan:
 *
 *   1. Si en algun sitio queda `/callcap` puesto, el enlace sigue saliendo.
 *      Sin esto volveria el LNK2001, o peor: alguien enlazaria `xbdm.lib`
 *      para taparlo y estariamos otra vez donde empezamos.
 *
 *   2. Y vacias son SEGURAS.  Una funcion C sin cuerpo compila a un `blr`
 *      pelado: no llama a nadie y no toca ni un registro volatil, asi que
 *      los argumentos de la funcion instrumentada llegan intactos.  Lo que
 *      reventaba era el TRABAJO de dentro del gancho, no el gancho.
 *      Ver la nota larga de perfil_cap.h. */
extern "C" void __CAP_Enter_Function(void) { }
extern "C" void __CAP_Exit_Function(void)  { }

#endif /* XBOX64_PERFIL_CAP */
