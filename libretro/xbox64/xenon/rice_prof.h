/*
 * xenon/rice_prof.h  --  sondas de perfilado DENTRO de Rice_GX.
 *
 * ★★ POR QUE AMBITOS MANUALES Y NO UN PERFILADOR AUTOMATICO.
 * ----------------------------------------------------------
 * Se intentaron los dos caminos automaticos y los dos estan cerrados en una
 * consola retail sin enlace xbdm.  El censo completo del XDK y la cadena de
 * evidencia estan en xenon/perfil_cap.h; el resumen:
 *
 *   PIX          `pix.h` solo trae anotaciones; la captura la arranca PIX
 *                desde el PC por xbdm.
 *   XTrace       "native (on the devkit) trace recording", lo dice el
 *                propio tracerecording.h.
 *   muestreo     `GetThreadContext` solo existe en xbdm.h / xbcrash.h, asi
 *                que no hay forma de leer el PC de otro hilo.
 *   /callcap     el compilador llama al gancho en la ENTRADA de la funcion,
 *                con sus argumentos todavia en r3-r12, y un gancho en C los
 *                destruye.  Costo una violacion de acceso en el dynarec.
 *
 * Asi que se vuelve a lo que YA FUNCIONA en este arbol: el mismo esquema de
 * `xenon/gln64_prof.h`, que lleva instrumentando glN64 desde su port.
 *
 * ★ Y NO ES "ELEGIR SOSPECHOSOS", que era la objecion justa.  Los ambitos
 * DESCUENTAN a sus hijos, asi que los cubos son disjuntos y **su suma es el
 * trabajo de CPU de Rice**.  El cubo padre (`flush`) recoge todo lo que no
 * cae en ningun hijo, o sea que nada puede esconderse: si el tiempo esta en
 * el prologo por draw que vive en `Render.cpp` -- que NO esta instrumentado
 * -- aparecera como tiempo PROPIO de `flush`, y eso mismo dice donde mirar
 * despues.  Es una particion, no una seleccion.
 *
 * ★ COMO LLEGA A LOS FICHEROS DE RICE.  Igual que el de glN64: lo arrastra
 * `xenon/gxshim/gccore.h`, que incluyen todas las unidades del arbol
 * copiado.  Asi las lineas que se anaden alla son UNA por funcion y no
 * declaran nada.  Con `RICE_PROF` a 0 son macro vacia y el binario vuelve a
 * ser identico.
 *
 * ★ A diferencia del de glN64 -- que cuelga de `XBOX64_SONDAS`, hoy a 0 --
 * este nace ENCENDIDO: es la medida que estamos buscando.  Los cubos son
 * por draw o por lista (miles por segundo, no por vertice), asi que medirlos
 * no cambia lo que se mide.
 *
 * ASCII, LF.
 */

#ifndef _XENON_RICE_PROF_H_
#define _XENON_RICE_PROF_H_

/* Version publica: cuelga de XBOX64_SONDAS (antes era 1 fijo).  Con las
 * sondas apagadas no queda ni un __mftb por draw ni el informe. */
#include "../source/xenon64_diag.h"
#define RICE_PROF XBOX64_SONDAS

/* El informe se declara siempre: libretro_core.cpp lo llama sin #if. */
#ifdef __cplusplus
extern "C" {
#endif
void rice_prof_informe(unsigned frames);
#ifdef __cplusplus
}
#endif

#if RICE_PROF

/* `__mftb`.  Va DENTRO del `#if`, igual que en gln64_prof.h: con el
 * perfilador apagado este fichero no debe arrastrar nada. */
#include <ppcintrinsics.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ★ Un par (tb, n) por cubo.  `tb` es tiempo PROPIO en ticks del time base;
 * `n`, cuantas veces se entro.  El par importa: un cubo caro con `n` enorme
 * es "barata pero llamada un millon de veces" y se arregla de otra forma que
 * un cubo caro con `n` pequeno.
 *
 * Los diez salen del camino caliente TRAZADO, no de la intuicion:
 *
 *   flush     OGLRender.cpp:876   RenderFlushTris: el draw entero (PADRE)
 *   zbias      "         :412     ApplyZBias
 *   vport      "         :1612    glViewportWrapper (proyeccion + viewport)
 *   ldtex      "         :1733    GXloadTextures
 *   verts      "         :964     el bucle de vertices, GX_Begin..GX_End
 *   pvtx      RenderBase.cpp      ProcessVertexData* (por comando VTX)
 *   mtx        "        :2459     UpdateCombinedMatrix
 *   lod        "        :816      ComputeLOD
 *   tcache    TextureManager.cpp:454   GetTxtrCacheEntry
 *   crc        "                  CalculateRDRAMCRC
 */
/* ★★ LOS ACUMULADORES SON DE 64 BITS, Y NO ES PRECAUCION: LA PRIMERA
 * VERSION SE SATURO.
 *
 * Con `unsigned int`, `ricep_pvtx_tb` salia clavado cerca de 0xFFFFFFFF en
 * TODAS las muestras -- `pvtx=1431631 us/frame`, o sea 1,4 segundos por
 * frame, imposible.  El propio informe lo delataba: `SUMA=649` era
 * coherente porque sumar 2^32-74296 equivale a restar 74296.
 *
 * Un contador de 32 bits en ticks del time base se llena en 86 segundos y
 * cualquier muestra corrupta lo tira al otro lado del cero.  Con 64 bits
 * eso no puede pasar y deja de haber una explicacion que descartar. */
extern volatile unsigned long long ricep_flush_tb;   extern volatile unsigned int ricep_flush_n;
extern volatile unsigned long long ricep_zbias_tb;   extern volatile unsigned int ricep_zbias_n;
extern volatile unsigned long long ricep_vport_tb;   extern volatile unsigned int ricep_vport_n;
extern volatile unsigned long long ricep_ldtex_tb;   extern volatile unsigned int ricep_ldtex_n;
extern volatile unsigned long long ricep_verts_tb;   extern volatile unsigned int ricep_verts_n;
extern volatile unsigned long long ricep_pvtx_tb;    extern volatile unsigned int ricep_pvtx_n;
extern volatile unsigned long long ricep_mtx_tb;     extern volatile unsigned int ricep_mtx_n;
extern volatile unsigned long long ricep_lod_tb;     extern volatile unsigned int ricep_lod_n;
extern volatile unsigned long long ricep_tcache_tb;  extern volatile unsigned int ricep_tcache_n;
extern volatile unsigned long long ricep_crc_tb;     extern volatile unsigned int ricep_crc_n;

/* ★★ SEGUNDA TANDA (2026-09-29), PARA CERRAR EL 45 % QUE NO SE VEIA.
 *
 * Con los diez primeros cubos, `SUMA` cubria el 71 % del tiempo grafico de
 * Mario 64 y solo el 55 % del de San Francisco Rush.  Ese resto no estaba
 * en ningun sitio: era, literalmente, "codigo de Rice que no miro".
 *
 * ★ `dlist` ES EL PADRE DE TODO Y ESA ES LA GRACIA.  `DLParser_Process` es
 * el bucle que despacha los comandos de la display list, asi que su tiempo
 * PROPIO -- descontados todos los demas cubos, que cuelgan de el-- ES
 * exactamente ese resto.  A partir de aqui nada puede esconderse: o cae en
 * un cubo con nombre o cae en `dlist`, y `dlist` grande significa "el coste
 * esta en el despacho de comandos", que tambien es una respuesta.
 *
 *   dlist   RSP_Parser.cpp:935   DLParser_Process   (PADRE de todo)
 *   pretex  RDP_Texture.h:1323   PrepareTextures    (padre de tcache/crc)
 *   endupd  OGLTexture.cpp:109   COGLTexture::EndUpdate  (+ DCFlushRange)
 *   combl   Render.cpp:307       CRender::SetCombinerAndBlender
 *   omodes  Render.cpp:1893      CRender::InitOtherModes
 *
 * ★ `endupd` esta en la lista por una sospecha CONCRETA: hace
 * `DCFlushRange` sobre los texels, y en esta consola eso no es un no-op
 * sino un bucle real de `__dcbst` linea a linea mas un `__sync`
 * (xenon/gxshim/gccore.h).  Y no tiene consumidor: el shim vuelve a leer
 * ese buffer por `LockRect`.  Si `endupd` sale gordo, ya sabemos que
 * quitar. */
extern volatile unsigned long long ricep_dlist_tb;   extern volatile unsigned int ricep_dlist_n;
extern volatile unsigned long long ricep_pretex_tb;  extern volatile unsigned int ricep_pretex_n;
extern volatile unsigned long long ricep_endupd_tb;  extern volatile unsigned int ricep_endupd_n;
extern volatile unsigned long long ricep_combl_tb;   extern volatile unsigned int ricep_combl_n;
extern volatile unsigned long long ricep_omodes_tb;  extern volatile unsigned int ricep_omodes_n;

/* Acumulador de los ambitos anidados; lo usa RiceProfScope para descontar el
 * tiempo de los hijos y quedarse con el PROPIO. */
extern volatile unsigned long long ricep_child_tb;

/* ★ PROFUNDIDAD DE ANIDAMIENTO, y arregla un defecto REAL del esquema de
 * `gln64_prof.h` del que se copio.
 *
 * Alli el destructor hace siempre `child_tb = savedChild + total`.  Para un
 * ambito de primer nivel `savedChild` es lo que dejo el ANTERIOR de primer
 * nivel, asi que `child_tb` crece sin tope durante toda la sesion.  No
 * corrompe los cubos mientras el anidamiento este equilibrado, pero es una
 * bomba de relojeria: en cuanto un destructor se salta (una excepcion
 * estructurada que `catch(...)` se traga sin desenrollar, y Rice tiene uno
 * en RSP_Parser.cpp), las cuentas dejan de cuadrar sin avisar.
 *
 * Con el contador, el ambito mas externo deja `child_tb` en cero y el
 * arrastre no existe. */
extern volatile int ricep_prof_nivel;

/* Muestras descartadas por absurdas (ver RICEP_TOPE).  ★ Va en el informe:
 * si sale 0 y un cubo sigue siendo enorme, el numero es REAL y hay que
 * creerselo; si sale distinto de 0, el instrumento sigue roto y no se
 * interpreta nada.  Es la diferencia entre una medida y una anecdota. */
extern volatile unsigned int ricep_raros;

/* Tope de cordura: 50 millones de ticks ~= 1 segundo.  Ninguna de las diez
 * funciones instrumentadas puede tardar eso; lo que pase de ahi es un
 * ambito desemparejado, no una medida. */
#define RICEP_TOPE  50000000ULL

/* Una vez por segundo, desde `resumen_por_segundo`.  `frames` es para dar
 * us/frame; reinicia los cubos. */
void rice_prof_informe(unsigned frames);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

struct RiceProfScope
{
    volatile unsigned long long *tb;
    volatile unsigned int       *n;
    unsigned long long           t0;
    unsigned long long           savedChild;
    RiceProfScope(volatile unsigned long long *tb_, volatile unsigned int *n_)
        : tb(tb_), n(n_), t0(__mftb()), savedChild(ricep_child_tb)
    {
        ricep_child_tb = 0;
        ricep_prof_nivel++;
    }
    ~RiceProfScope()
    {
        unsigned long long total = __mftb() - t0;
        unsigned long long hijos = ricep_child_tb;

        /* ★ Muestra absurda: se TIRA y se CUENTA, no se acumula.  Un ambito
         * desemparejado envenena el cubo para siempre; contarlo aparte deja
         * el resto de la medida utilizable y dice cuanto desconfiar. */
        if (total > RICEP_TOPE)
        {
            ricep_raros++;
            total = 0;
            hijos = 0;
        }

        *tb += (total > hijos) ? (total - hijos) : 0;   /* propio */
        (*n)++;

        /* El mas externo no arrastra nada al siguiente.  Ver la nota de
         * `ricep_prof_nivel`. */
        if (--ricep_prof_nivel <= 0)
        {
            ricep_prof_nivel = 0;
            ricep_child_tb   = 0;
        }
        else
        {
            ricep_child_tb = savedChild + total;        /* al padre, el total */
        }
    }
};

#define RICE_PROF_SCOPE(bucket) \
    RiceProfScope _riceprof_(&ricep_##bucket##_tb, &ricep_##bucket##_n)

#else  /* !__cplusplus */
#define RICE_PROF_SCOPE(bucket)  ((void)0)
#endif

#else  /* !RICE_PROF */
#define RICE_PROF_SCOPE(bucket)  ((void)0)
#endif /* RICE_PROF */

#endif /* _XENON_RICE_PROF_H_ */
