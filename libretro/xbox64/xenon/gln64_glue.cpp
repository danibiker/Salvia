/*
 * gln64_glue.cpp  --  pegamento del plugin glN64_GX en Xenon64.
 *
 * Define los globales que `source/glN64_GX/` declara `extern` y que en Wii64
 * vivian en ficheros que aqui no se compilan (Config_linux.cpp, main/*,
 * gui/*), mas la configuracion del plugin.
 *
 * Todo lo de aqui es NUESTRO.  El arbol copiado no se toca (ver
 * xenon/gxshim/DIVERGENCIAS.txt).
 *
 * ★★ LA REGLA DE ENLACE DE ESTE FICHERO, Y COSTO UN BUILD ENTERO
 * ---------------------------------------------------------------
 * El enlace de cada simbolo lo fija **LA PRIMERA DECLARACION QUE VE EL
 * COMPILADOR**, y aqui esa declaracion casi siempre esta en el ARBOL
 * COPIADO, que es C++ y NO usa `extern "C"`.  O sea:
 *
 *   - Si el simbolo lo declara `source/glN64_GX/*` a pelo
 *     (`extern float VILimit;`)  ->  enlace C++  ->  definir SIN extern "C".
 *     Firma del fallo: LNK2001 con el nombre DECORADO (`?VILimit@@3MA`).
 *
 *   - Si lo declara una cabecera NUESTRA dentro de `extern "C"`
 *     (timers.h, wii64config.h, gamehacks.h, gx_d3d9.h)  ->  enlace C  ->
 *     definir CON extern "C".
 *     Firma del fallo: LNK2001 con el nombre SIN DECORAR.
 *
 *   - Y si el arbol copiado lo declara el mismo dentro de `extern "C"`
 *     (VI.cpp:89-91, VI.cpp:243-247)  ->  enlace C.
 *
 * Es la misma trampa de [[reference_xdk_vcxproj_compileas]], pero al reves de
 * como la aplique la primera vez: alli el problema era C leyendo C++, aqui es
 * C++ leyendo lo que yo habia marcado como C.  **Mirar como lo declara quien
 * lo USA, no decidirlo aqui.**
 */

#include <xtl.h>
#include <string.h>
#include <stdio.h>
#include "gxshim/gccore.h"
#include "gx_d3d9.h"           /* gxd3d_init/shutdown: declarados extern "C" */
/* ★ HACE FALTA DE VERDAD, no es higiene: este fichero DEFINE los contadores
 * del perfilador y `gln64_prof_informe`, y sin la cabecera `GLN64_PROF` no
 * estaria definida -- el `#if GLN64_PROF` del informe valdria 0 y la
 * funcion compilaria A NADA, sin un solo aviso.  De paso, las
 * declaraciones `extern` de la cabecera quedan comprobadas contra las
 * definiciones de mas abajo, que hasta ahora coincidian por convenio. */
#include "gln64_prof.h"
#include "../source/xenon64_diag.h"
#include "../source/main/timers.h"
#include "../source/main/wii64config.h"
#include "../source/main/gamehacks.h"

extern "C" void DebugOutputString(const char *s);

/* [Xbox360] Aqui estaba el filtro de ucode del hilo grafico antiguo
 * (`gln64_ucode_tipo` + `gln64_ucode_apto_para_hilo`), quitado con el hilo
 * el 2026-09-30.  Su unico escritor era una linea de
 * source/glN64_GX/RSP.cpp, tambien quitada. */

/* ------------------------------------------------------------------ */
/* Contadores de la sonda TEMPORAL dentro de glN64 (gln64_prof.h)      */
/* ------------------------------------------------------------------ */
/* Viven aqui, no en el plugin: las lineas que se anaden alla son una por
 * funcion y no declaran nada.  Ver la cabecera de xenon/gln64_prof.h. */
extern "C" {
volatile unsigned int gln64p_dlist_tb = 0,   gln64p_dlist_n = 0;
volatile unsigned int gln64p_viupd_tb = 0,   gln64p_viupd_n = 0;
volatile unsigned int gln64p_vtx_tb = 0,     gln64p_vtx_n = 0;
volatile unsigned int gln64p_addtri_tb = 0,  gln64p_addtri_n = 0;
volatile unsigned int gln64p_drawtri_tb = 0, gln64p_drawtri_n = 0;
volatile unsigned int gln64p_rect_tb = 0,    gln64p_rect_n = 0;
volatile unsigned int gln64p_ldtile_tb = 0,  gln64p_ldtile_n = 0;
volatile unsigned int gln64p_updst_tb = 0,   gln64p_updst_n = 0;
volatile unsigned int gln64p_tcupd_tb = 0,   gln64p_tcupd_n = 0;
volatile unsigned int gln64p_comb_tb = 0,    gln64p_comb_n = 0;
volatile unsigned int gln64p_tcload_tb = 0,  gln64p_tcload_n = 0;
volatile unsigned int gln64p_tccrc_tb = 0,   gln64p_tccrc_n = 0;
volatile unsigned int gln64p_crcbytes = 0;
/* Consultas por rama de paleta; ver la nota en gln64_prof.h. */
volatile unsigned int gln64p_pal4 = 0, gln64p_pal8 = 0, gln64p_pal16 = 0;
volatile unsigned int gln64p_crccalls = 0, gln64p_crclento = 0;
volatile unsigned int gln64p_vtxn = 0;
/* Paso 0 del hilo grafico; ver la nota en gln64_prof.h. */
volatile unsigned int gln64p_fsync = 0;
volatile unsigned int gln64p_salok = 0, gln64p_salloop = 0;
volatile unsigned int gln64p_saldir = 0, gln64p_salwd = 0;
/* Acumulador de los ambitos anidados; lo usa Gln64ProfScope para descontar
 * el tiempo de los hijos y quedarse con el PROPIO. */
volatile unsigned int gln64p_child_tb = 0;
}

/* ★★ EL INFORME QUE FALTABA, Y ESA ES LA HISTORIA.
 *
 * Los once cubos de arriba se llevaban instrumentando desde el port de
 * glN64 -- 13 `GLN64_PROF_SCOPE` repartidos por el plugin -- y NADIE LOS
 * LEIA.  La parte que los volcaba vivia en la maquinaria de diagnostico de
 * Xenon64 y no se porto con el resto.  O sea que habia un perfilador
 * completo corriendo y tirando sus numeros a la basura.
 *
 * ★ LOS CUBOS SON DISJUNTOS (tiempo PROPIO, sin los hijos instrumentados),
 * asi que su suma es el trabajo de CPU del plugin y se puede comparar con
 * el presupuesto del frame.  El detalle esta en gln64_prof.h.
 *
 * ★ `hash` NO tiene cubo propio a proposito: va DENTRO de `tcupd`.  Si
 * `tcupd` sale grande, la pregunta siguiente es si es el CRC o el recorrido
 * de la lista, y para eso hace falta partirlo.
 *
 * La frecuencia se PREGUNTA, no se cablea: mismo motivo que en
 * xbox/compat_posix.c:15-19. */
extern "C" void gln64_prof_informe(unsigned frames)
{
#if GLN64_PROF
    static LONGLONG frec = 0;
    char linea[256];

    if (frames == 0)
        frames = 1;

    if (frec == 0)
    {
        LARGE_INTEGER f;
        if (QueryPerformanceFrequency(&f) && f.QuadPart)
            frec = f.QuadPart;
        else
            frec = 49875000;      /* respaldo: timebase del Xenon */
    }

    /* ★★ MICROSEGUNDOS POR FRAME, NO MILISEGUNDOS POR SEGUNDO.
     *
     * Es la misma medida dividida entre los frames de la ventana, pero se
     * lee de otra manera: el presupuesto de un frame de N64 son 16.667 us,
     * asi que cada cubo se compara DE UN VISTAZO con el numero que importa.
     * Con ms/s hay que dividir mentalmente entre 60 cada vez, y eso es una
     * invitacion a equivocarse leyendo.
     *
     * En 64 bits: tb * 1.000.000 con ~50M ticks da 5e13, que cabe. */
    #define US(tb) ((unsigned)((((LONGLONG)(tb) * 1000000LL) / frec) / frames))

    sprintf(linea,
            "[GLN64T] dlist=%u viupd=%u | vtx=%u addtri=%u drawtri=%u rect=%u "
            "| ldtile=%u updst=%u comb=%u | tcupd=%u tccrc=%u tcload=%u  (us/frame)",
            US(gln64p_dlist_tb),  US(gln64p_viupd_tb),
            US(gln64p_vtx_tb),    US(gln64p_addtri_tb),
            US(gln64p_drawtri_tb),US(gln64p_rect_tb),
            US(gln64p_ldtile_tb), US(gln64p_updst_tb), US(gln64p_comb_tb),
            US(gln64p_tcupd_tb),  US(gln64p_tccrc_tb), US(gln64p_tcload_tb));
    DebugOutputString(linea);

    /* ★★ LA LINEA QUE DECIDE QUE HAY QUE ARREGLAR.
     *
     *   MB/s razonable (cientos)  -> xxHash va bien y el problema es el
     *                                VOLUMEN: se le dan demasiados bytes.
     *                                Arreglo: hashear menos (la paleta de
     *                                2 KB en cada consulta, una llamada
     *                                por FILA en vez de una por textura).
     *   MB/s ridiculo (decenas)   -> el coste es por LLAMADA, no por byte:
     *                                XXH32 se llama `height` veces y cada
     *                                una paga inicializacion y avalancha.
     *                                Arreglo: una sola llamada.
     *
     * Y `tcupd` ya no incluye el CRC (los ambitos descuentan a los hijos),
     * asi que lo que queda ahi ES el recorrido de la lista. */
    {
        unsigned us  = US(gln64p_tccrc_tb) * frames;      /* us de la ventana */
        unsigned kb  = gln64p_crcbytes / 1024u;
        sprintf(linea,
                "[GLN64H] crc: %u llamadas, %u KB, %u us  -> %u MB/s  | "
                "%u bytes/llamada",
                gln64p_tccrc_n, kb, us,
                us ? (unsigned)(((LONGLONG)gln64p_crcbytes * 1000000LL)
                                / ((LONGLONG)us * 1048576LL)) : 0,
                gln64p_tccrc_n ? gln64p_crcbytes / gln64p_tccrc_n : 0);
        DebugOutputString(linea);
    }

    /* ★ LAS CUENTAS VAN APARTE Y HACEN FALTA: 5000 us en `tcload` significan
     * cosas opuestas segun si es UNA carga de textura carisima (mirar la
     * conversion) o 300 (la cache no retiene: mirar su tamanyo y la clave).
     *
     * ★ ESTAS VAN POR SEGUNDO Y NO POR FRAME, Y ES A PROPOSITO: al
     * dividirlas entre 60 con enteros, `dlist` salia **0** -- 20 listas
     * entre 60 frames-- y con el la informacion.  Un contador que se
     * redondea a cero miente mas que no estar. */
    sprintf(linea,
            "[GLN64N] dlist=%u viupd=%u vtx=%u addtri=%u drawtri=%u rect=%u "
            "ldtile=%u updst=%u comb=%u tcupd=%u tcload=%u  (n/s)",
            gln64p_dlist_n,  gln64p_viupd_n,  gln64p_vtx_n,
            gln64p_addtri_n, gln64p_drawtri_n,gln64p_rect_n,
            gln64p_ldtile_n, gln64p_updst_n,  gln64p_comb_n,
            gln64p_tcupd_n,  gln64p_tcload_n);
    DebugOutputString(linea);

    /* ★ El reparto de la rama de paleta.  `pal16` es la que GLideN64 no
     * tiene: si sale 0 se puede quitar, y si sale alta hay que estudiarla.
     * Ver la nota en gln64_prof.h. */
    sprintf(linea,
            "[GLN64PAL] paleta: 4b=%u 8b=%u 16b=%u | xxh=%u/s (%u B/llam) "
            "porfila=%u  (n/s)",
            gln64p_pal4, gln64p_pal8, gln64p_pal16,
            gln64p_crccalls,
            gln64p_crccalls ? (gln64p_crcbytes / gln64p_crccalls) : 0,
            gln64p_crclento);
    DebugOutputString(linea);

    /* ★ VERTICES TRANSFORMADOS, y el coste POR VERTICE.
     *
     * `OGL.GXnumVtxMP` lo lleva contando `gSPProcessVertex` desde siempre y
     * solo se imprimia dentro de un `SHOW_DEBUG` que este build no compila
     * -- el mismo caso que el resto del perfilador.
     *
     * Es el dato que decide si merece la pena vectorizar el T&L: el cubo
     * `vtx` son 1,91 ms del frame que dibuja, pero sin saber CUANTOS
     * vertices son no se sabe si el coste por vertice es de aritmetica
     * (~100 ciclos, y entonces SIMD no arregla nada) o de acarreo de
     * memoria (miles, y entonces el problema es el formato de los datos).
     *
     * Contador propio y no `OGL.GXnumVtxMP`: `OGL` es del plugin y desde
     * aqui no se ve su tipo sin arrastrar OpenGL.h entero. */
    sprintf(linea, "[GLN64V] vertices=%u/s  (vtx propio: %u us/frame)",
            gln64p_vtxn, US(gln64p_vtx_tb));
    DebugOutputString(linea);

    /* ★★ PASO 0 DEL HILO GRAFICO.  Ver la nota larga en gln64_prof.h.
     *
     * `dlist` es cuantas listas se procesaron; `fsync` cuantas levantaron el
     * DP.  Si no coinciden, afirmar el `DP_INT` al despachar seria mentir, y
     * `loop/dir/wd` dicen por donde. */
    sprintf(linea,
            "[GLN64DP] dlist=%u fsync=%u | salidas: ok=%u loop=%u dir=%u wd=%u",
            gln64p_dlist_n, gln64p_fsync,
            gln64p_salok, gln64p_salloop, gln64p_saldir, gln64p_salwd);
    DebugOutputString(linea);


    #undef US

    gln64p_dlist_tb = gln64p_dlist_n = 0;
    gln64p_viupd_tb = gln64p_viupd_n = 0;
    gln64p_vtx_tb = gln64p_vtx_n = 0;
    gln64p_addtri_tb = gln64p_addtri_n = 0;
    gln64p_drawtri_tb = gln64p_drawtri_n = 0;
    gln64p_rect_tb = gln64p_rect_n = 0;
    gln64p_ldtile_tb = gln64p_ldtile_n = 0;
    gln64p_updst_tb = gln64p_updst_n = 0;
    gln64p_tcupd_tb = gln64p_tcupd_n = 0;
    gln64p_comb_tb = gln64p_comb_n = 0;
    gln64p_tcload_tb = gln64p_tcload_n = 0;
    gln64p_tccrc_tb = gln64p_tccrc_n = 0;
    gln64p_crcbytes = 0;
    gln64p_pal4 = gln64p_pal8 = gln64p_pal16 = 0;
    gln64p_crccalls = gln64p_crclento = 0;
    gln64p_vtxn = 0;
    gln64p_fsync = 0;
    gln64p_salok = gln64p_salloop = gln64p_saldir = gln64p_salwd = 0;
#endif
}

/* ------------------------------------------------------------------ */
/* Grupo 1: los declara el ARBOL COPIADO sin extern "C" -> ENLACE C++  */
/* ------------------------------------------------------------------ */

/* Config_linux.cpp los define en Wii64; aqui salen de las core options.
 * Declarados en OpenGL.cpp:73-74, sin extern "C" (no estan en ninguna
 * cabecera: cada consumidor repite el `extern`). */
/* ★ LO FIJA LA CORE OPTION `xenon64_gln64_framebuffer`, via
 * `gln64_set_options()`, que corre antes de abrir la ROM.  Este valor inicial
 * solo cubre el caso de que nadie llame (standalone).
 *
 * El reparto medido, por si alguien se plantea cablearlo:
 *   A FAVOR    quita el parpadeo de Paper Mario (triple buffer: tres color
 *              image de 320x240x2, ver [CIMG-GX]) y arregla en Zelda OoT el
 *              fondo del menu de pausa y el muñeco de Link.
 *   EN CONTRA  OoT pierde mucho rendimiento (sin cuantificar).
 *
 * ★ Aqui se le culpaba tambien del cuelgue de Mario Kart 64 y era FALSO: eso
 * era el `&& (mi & 0x1)` de memory.c:1847.  MK64 arranca con esto encendido y
 * apagado por igual. */
char glN64_useFrameBufferTextures = 0;
char glN64_use2xSaiTextures       = 0;

/* [xbox64] `VILimit` QUITADA: la define `main/timers.c:16` y la RECALCULA
 * `InitTimer()` (timers.c:47) segun el codigo de region de la ROM -- 50 Hz
 * para PAL, 60 para NTSC.  Dejar aqui una copia fija a 60.0f hacia que el
 * plugin leyera siempre 60 aunque el juego fuera PAL.
 *
 * ★ Y por eso `VI.cpp:290` pasa a declararla `extern "C"`: sin eso busca
 * el simbolo decorado, que ya no existe. */

/* VI.cpp:44, sin extern "C".  Desplazamiento del XFB: en Wii centra la
 * imagen en modo entrelazado; aqui la imagen sale por textura y la coloca
 * el frontend. */
int GX_xfb_offset = 0;

/* OpenGL.h:34 (`extern GXRModeObj *vmode, *rmode;`), sin extern "C".
 * El plugin lee `rmode->aa`, `->sample_pattern` y `->vfilter` en
 * GX_SetCopyFilter -- que nuestro shim ignora-- y las dimensiones. */
static GXRModeObj s_rmode;
GXRModeObj *vmode = &s_rmode;
GXRModeObj *rmode = &s_rmode;

/* ★ `GXtexCache` NO SE DEFINE AQUI.  La define el propio plugin
 * (`Textures.cpp:53`) y ademas se la inicializa solo en `TextureCache_Init`
 * (`Textures.cpp:651-664`): reserva el monton con `GX_TEXTURE_CACHE_SIZE` si
 * lo encuentra a NULL.
 *
 * Mi primera version la duplicaba, con su propio monton de 8 MB:
 *
 *     gln64_glue.obj : warning LNK4006: "GXtexCache" already defined in
 *                      Textures.obj; second definition ignored
 *
 * Y otra vez lo peligroso es que sea un WARNING: el enlazador se queda con
 * una de las dos en silencio, asi que el plugin y el glue podrian haber
 * estado mirando montones distintos.
 *
 * Comprobado ademas que NO hace falta reinicializarlo entre ROMs:
 * `TextureCache_Destroy` vacia la cache entera con
 * `while (cache.bottom) TextureCache_RemoveBottom();`, que libera cada
 * textura del monton.  O sea que el `if(!GXtexCache)` de arriba es un
 * one-shot SEGURO aqui, al contrario que el de `Fbg_Setup` o el de
 * `RecompCache_Init` (project_xenon64_libretro_port). */

/* Contadores de triangulos de la pantalla de debug de Wii64. */
int CntTriProj = 0, CntTriProjW = 0, CntTriOther = 0;
int CntTriNear = 0, CntTriPolyOffset = 0;

/* Config.h los declara sin extern "C", y glN64.cpp los llama.
 * Sustituyen a Config_linux.cpp, que es GTK y no se compila. */
/* ★ `Config_LoadConfig` y `Config_DoConfig` VIVEN EN `gln64_config.cpp`.
 *
 * Aqui habia un stub vacio con el comentario "los valores por defecto los
 * fija gln64_set_options()", y era falso: esa funcion solo toca dos banderas
 * de core option y TODO lo demas se quedaba en el cero del `.bss`.  El que
 * mas dolia era `cache.maxBytes = 0`, que hacia que la cache de texturas del
 * plugin desalojara cada textura nada mas entrar.
 *
 * Estan en otra unidad porque escribir en `OGL` y `cache` obliga a incluir
 * `OpenGL.h`/`Textures.h`, que arrastran `gl.h` y chocan con el `<xtl.h>` de
 * este fichero.  Ver la cabecera de gln64_config.cpp. */

/* ------------------------------------------------------------------ */
/* Grupo 2: declarados dentro de extern "C" -> ENLACE C                */
/* ------------------------------------------------------------------ */

/* Nuestro source/main/timers.h.  Se quedan a cero: quien mide el ritmo en
 * este arbol es el perfilador ([PERF]/[FRAME]/[PEAK]), no el plugin. */
/* [xbox64] `Timers` QUITADA: la define `main/timers.c:15`, que SI esta en
 * este build.  Y no es solo un duplicado de enlace: timers.c es quien la
 * ACTUALIZA (new_frame y new_vi calculan Timers.vis y Timers.fps).  Con la
 * de aqui ganando, el plugin leeria una estructura que nadie toca. */

/* En Wii64 avisa de que una display list no termino, para que el limitador
 * de VI no espere.  Aqui el ritmo lo lleva el frontend (Sync::limit_fps) y
 * el core solo arma `frame_done` en el VI_INT: no hay nada que avisar. */
/* [xbox64] `dlist_incomplete` e `InitTimer` QUITADAS: las define
 * `main/timers.c` (:64 y :47), que SI esta en este build -- el arbol de
 * origen no tenia ese fichero y por eso las ponia aqui vacias.
 *
 * Tenerlas las dos daba `LNK4006: already defined in gln64_glue.obj;
 * second definition ignored`, y lo ignorado era la implementacion DE
 * VERDAD: InitTimer calcula VILimit segun el codigo de region de la ROM
 * (timers.c:20-45) y la llama `rom_gc.c` al cargar.  Con la vacia
 * ganando, el limite de VI se quedaba en su valor por defecto. */


/* Nuestro source/main/wii64config.h.  El aspecto y el escalado los decide el
 * FRONTEND, despues de recibir la textura (env 30003). */
extern "C" char screenMode   = SCREENMODE_4x3;
extern "C" char nativeOutput = NATIVEOUT_DISABLE;

/* VI.cpp:243-247 los declara el mismo dentro de extern "C". */
extern "C" int enableLoadIcon = 0;

extern "C" long long gettime(void)
{
    return (long long)__mftb();          /* time base de 64 bits de Xenon */
}

extern "C" unsigned int diff_sec(long long start, long long end)
{
    long long d = end - start;           /* el time base va a 49,875 MHz */
    if (d < 0) d = 0;
    return (unsigned int)(d / 49875000LL);
}

/* VI.cpp:89-91, tambien dentro de extern "C".  En Wii64 conmutan la salida
 * de video a 240p nativo.  Aqui no aplica: el core entrega su textura y es
 * el frontend quien decide resolucion y escalado -- y Salvia ya tiene modos
 * de escalado entero 1x-5x, que es lo que esto perseguia.
 *
 * `getEfbHeight` devuelve la altura real del render target.  Con >264 y
 * `nativeOutput` en DISABLE, `GX_CheckResChange` (VI.cpp:108) deja
 * `using_240p` en falso y no hace nada mas: el camino queda inerte por
 * construccion, no por casualidad.
 *
 * (Se llamaba `getXfbHeight` hasta Wii64 1b5041a, que la renombro a
 * `getEfbHeight` -- misma semantica, el nombre viejo era el equivocado.)
 *
 * `getVmodeAA` dice si el modo de video de la Wii usa antialiasing, en cuyo
 * caso su z-buffer se guarda en formato Z16-mid y `_gDPPeekDepth` (gDP.cpp)
 * tiene que decodificarlo.  Aqui no hay AA de EFB: devuelve 0 siempre, y asi
 * el valor que da nuestro GX_PeekZ se interpreta como los 24 bits que es. */
extern "C" void switchTo240p(bool is_pal)   { (void)is_pal; }
extern "C" void switchToNormalVideo(void)   { }
extern "C" int  getEfbHeight(void)          { return (int)s_rmode.efbHeight; }
extern "C" int  getVmodeAA(void)            { return 0; }

/* ------------------------------------------------------------------ */
/* [xbox64] LOS HACKS POR JUEGO SE HAN QUITADO DE AQUI.  NO REPONERLOS. */
/* ------------------------------------------------------------------ */
/* Aqui habia una implementacion VACIA de las ocho funciones de
 * `gamehacks.h` -- hack_winback, hack_paperboy, hack_dukenukem,
 * hack_scars, hack_ogrebattle, hack_yoshistory, GetGameSpecificHack y
 * GameSpecificHackSetup -- con la politica de "entrar con todos apagados".
 *
 * ★ En el arbol de origen eso era necesario: alli `source/main/` tiene
 * `gamehacks.h` pero NO `gamehacks.c`, asi que si nadie las definia no
 * enlazaba.
 *
 * ★★ AQUI SOBRA Y ADEMAS ROMPE, POR PARTIDA DOBLE:
 *
 *   1. `main/gamehacks.c` de Wii64 SI esta en el build -- lo lista
 *      Makefile.base -- y define las ocho CON LOS HACKS DE VERDAD,
 *      incluido el corte por CRC de gamehacks.c:369-386.  Tener las dos
 *      es LNK2005.
 *   2. El `gamehacks.h` de Wii64 las declara SIN `extern "C"`, o sea con
 *      enlace C++.  Redefinirlas aqui dentro de `extern "C"` da
 *      `C2732: linkage specification contradicts earlier specification`,
 *      que es el error que aparecio.
 *
 * ★ Y la decision de fondo: la politica de "sin hacks por juego" es del
 * proyecto de origen, no de este.  La premisa de xbox64 es portar Wii64
 * TAL CUAL, y Wii64 lleva esos hacks y los cablea desde
 * `rom_gc.c:GameSpecificHackSetup()` y `timers.c:new_frame()`.  Quitarlos
 * seria justamente "no anyadir lo que la referencia no tiene" al reves:
 * quitar lo que SI tiene. */

/* ------------------------------------------------------------------ */
/* Opciones                                                            */
/* ------------------------------------------------------------------ */
extern "C" void gln64_set_options(int frameBufferTextures, int sai2x)
{
    glN64_useFrameBufferTextures = (char)(frameBufferTextures ? 1 : 0);
    glN64_use2xSaiTextures       = (char)(sai2x ? 1 : 0);
}

/* ------------------------------------------------------------------ */
/* Arranque y parada del backend                                       */
/* ------------------------------------------------------------------ */
/* glN64.cpp:66, sin extern "C" -> enlace C++ (grupo 1).
 *
 * ★★ [Fase 6] Y LA DE Rice TAMBIEN, QUE ES OTRA FUNCION.  `rice_rename.h`
 * prefija la copia de Rice (Video.cpp:102, tambien enlace C++), asi que este
 * fichero -- que no lleva `__GX__` y por tanto no ve ningun renombrado --
 * solo alcanzaba la de glN64.  Con Rice activo, `gGX.GXwidth` se quedaba a
 * CERO y la pantalla en negro; ver la nota de la llamada, mas abajo. */
void gfx_set_window(int x, int y, int width, int height);
void Rice_gfx_set_window(int x, int y, int width, int height);

extern "C" int gln64_backend_init(int width, int height)
{
    /* El "modo de video" que el plugin consulta.  Solo lee `aa`,
     * `sample_pattern` y `vfilter` en GX_SetCopyFilter -- que nuestro shim
     * ignora-- y las dimensiones. */
    memset(&s_rmode, 0, sizeof(s_rmode));
    s_rmode.fbWidth   = (u16)width;
    s_rmode.efbHeight = (u16)height;
    s_rmode.xfbHeight = (u16)height;
    s_rmode.viWidth   = (u16)width;
    s_rmode.viHeight  = (u16)height;
    s_rmode.aa        = 0;

    /* ★★ EL RECTANGULO DE PANTALLA.  ESTA LLAMADA NO ES OPCIONAL.
     *
     * `OGL.GXorigX/GXorigY/GXwidth/GXheight` se asignan en EXACTAMENTE UN
     * sitio de todo el plugin -- `gfx_set_window` (glN64.cpp:68-71)-- y en
     * Wii64 quien la llama es el FRONTEND al arrancar, codigo que aqui no se
     * compila.  `OGL_Start()` no las toca (solo pone OGL.width/height desde
     * `rmode`, OpenGL.cpp:505-506), y el unico llamante que queda dentro del
     * arbol es `GX_CheckResChange()` (VI.cpp:113-122), que solo dispara
     * cuando la resolucion CAMBIA: con 480 de alto fijo su condicion es
     * falsa siempre, asi que no se arregla solo.
     *
     * Sin esto se quedan a CERO, y de ahi sale todo lo demas:
     *   OGL.GXscaleX = GXwidth / VI.width = 0     (OpenGL.cpp:328)
     *   GX_SetViewport(0 + vp.x*0, ..., vp.w*0, vp.h*0)   -> 0x0
     *   GX_SetScissor(0, 0, min(lrx*0, 0), ...)           -> 0x0
     * y nuestros clamp de D3D convierten esos rects degenerados en 1x1 en
     * vez de fallar.  Sintoma medido: el juego corre, suena y dibuja
     * (draws=182925, verts=2808204, shfail=0) pero LA ESCENA ENTERA CAE EN
     * EL PIXEL (0,0) -- pantalla negra con unos pocos pixeles de color.
     *
     * ★ Y el clamp es lo que lo volvio silencioso: sin el, SetScissorRect
     * habria fallado y se veria todo sin recortar, que es un sintoma que SI
     * apunta al scissor.  El clamp sigue haciendo falta (D3D es mas estricto
     * que GX), pero por eso el defecto se busco primero en el combinador.
     *
     * Se llama AQUI porque este init corre antes de `xenon64_rom_open()` ->
     * `InitiateGFX` -> `OGL_Start()`, y `OGL_Start()` termina en
     * `OGL_UpdateScale()` (OpenGL.cpp:547), que recalcula GXscaleX/Y con
     * estos valores ya puestos. */
    /* ★★ [Fase 6] SE LLAMA A LAS DOS, Y ES LO CORRECTO, no pereza.
     *
     * Este init corre ANTES de `initiateGFX`, que es donde
     * `xbox/gfx_dispatch.c` congela que plugin manda.  Aqui todavia no se
     * sabe, asi que preguntar seria leer una eleccion vieja.  Y no hay nada
     * que elegir: cada una escribe SOLO las globales de su propio plugin
     * (`OGL.GX*` contra `gGX.GX*`), asi que avisar al que luego no se use no
     * cuesta nada y quita el problema de orden de raiz.
     *
     * ★ SIN ESTO, RICE SALE EN NEGRO Y LOS CONTADORES DE PASO ESTAN SANOS:
     * dibuja sus ~140 draws por frame, pero `gGX.GXwidth` vale 0, su
     * `GX_SetViewport` pide un rectangulo de 0x0, el shim lo acota a 1x1
     * (gx_d3d9.cpp:1466) y todo el frame cae dentro de un pixel.  Se cazo
     * con `[GXD3D2] degen=9891 ... vp=0,0 1x1`, que es exactamente el mismo
     * sintoma que ya tuvo glN64 con `OGL.GXwidth`. */
    gfx_set_window(0, 0, width, height);
    Rice_gfx_set_window(0, 0, width, height);

    /* El monton de texturas se lo monta el plugin en TextureCache_Init.
     * Ver la nota de GXtexCache arriba. */
    return gxd3d_init();
}

extern "C" void gln64_backend_shutdown(void)
{
    gxd3d_shutdown();
}

/* ------------------------------------------------------------------ */
/* NOTA: `crc32` y `gfx_set_window` NO se definen aqui, y no es un olvido  */
/* ------------------------------------------------------------------ */
/* - `crc32`: la trae **zlib.lib**, que ya esta en la linea de enlace del
 *   FRONTEND (`COMMON_LIBS_XBOX_RELEASE`).  Implementarla aqui daba
 *   `LNK2005: crc32 already defined in zlib.lib(crc32.obj)`.  La declaracion
 *   de gccore.h basta, y ademas asi es el crc32 canonico de verdad -- que es
 *   lo que exige el uso (se compara contra la tabla de microcodigos
 *   conocidos, GBI.cpp:433).
 *
 * - `gfx_set_window`: la define el propio plugin en glN64.cpp:66.  La primera
 *   version de este fichero la duplicaba.  Aqui solo se LLAMA, desde
 *   `gln64_backend_init` -- ver la nota larga de alli, porque que nadie la
 *   llamara era el motivo de la pantalla negra.
 *
 *   Y no hay colision con Rice pese a que `Rice_GX_Xenos/Video.cpp:117`
 *   define una funcion con el mismo nombre: la suya vive bajo `#ifdef __GX__`
 *   (Video.cpp:96) y `__GX__` NO esta en los defines del proyecto, solo en
 *   los per-fichero de los 39 ficheros de glN64.  Lo mismo vale para
 *   `gfx_set_fb`, `VI_GX_setFB` y `VI_GX_PreRetraceCallback`, que un censo
 *   por texto marca como duplicados y el preprocesador descarta. */
