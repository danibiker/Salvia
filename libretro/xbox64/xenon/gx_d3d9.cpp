/*
 * gx_d3d9.cpp  --  implementacion de la API GX de libogc sobre el D3D9 del
 *                  XDK de Xbox 360.
 *
 * Es el backend del plugin glN64_GX en Xenon64.  Ver la explicacion del
 * corte en xenon/gxshim/gccore.h y las divergencias del arbol copiado en
 * xenon/gxshim/DIVERGENCIAS.txt.
 *
 * Contenido de este fichero: estado del rasterizador, matrices, geometria en
 * modo inmediato, texturas y la copia del framebuffer empotrado.
 * El combinador (las 16 funciones GX_SetTev*) vive en gx_d3d9_tev.cpp.
 *
 * TRES COSAS DEL ARBOL QUE HAY QUE RESPETAR, Y QUE YA COSTARON UN BUILD CADA
 * UNA CUANDO SE PORTO RICE:
 *
 *  1. El device es del FRONTEND y esta COMPARTIDO (core + video_handoff +
 *     el menu/overlay de Salvia).  "Fijar el estado una vez al inicializar"
 *     no vale: cada pasada tiene que establecer el estado del que depende.
 *     Ver project_xenon64_libretro_port.
 *  2. Los recursos se mutan y se destruyen A TRAVES DEL OBJETO
 *     (`tex->LockRect`, `tex->Release`), no del device, asi que el
 *     interpositor `d3d9_grabador.h` no los ve: hace falta `d3dq_sinc()`
 *     delante de cada acceso directo y `d3dq_soltar()` para los Release.
 *  3. `D3DBLEND` del XDK es 0-BASED (ZERO=0, ONE=1, SRCALPHA=6,
 *     INVSRCALPHA=7, DESTALPHA=10), NO como el D3D9 de PC.  Medido, ver
 *     project_xenon64_texturas_d3d9.
 */

#include <xtl.h>
#include <d3dx9.h>
#include <xgraphics.h>        /* XGAddress2DTiledOffset (en linea): copia de Z */
#include <ppcintrinsics.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>   /* _aligned_malloc: la arena del monton de Rice */
#include <math.h>
#include "gx_d3d9_int.h"
/* ★ gx_d3d9.h declara gxd3d_init/shutdown/frame_begin/frame_end dentro de un
 * `extern "C"`.  Hay que incluirlo AQUI para que las definiciones de abajo
 * hereden ese enlace; sin el salen con enlace C++ y quien las llama
 * (gln64_glue.cpp, que si incluye el header) no las encuentra:
 *     LNK2001: unresolved external symbol gxd3d_init     <- SIN decorar */
#include "gx_d3d9.h"
#include "video_handoff.h"
#include "../source/xenon64_diag.h"
#include "../source/libogc_lwp_heap.h"   /* lwp_heap_free_hook */
/* [Fase 6] `xbox64_gfx_plugin_activo` y XBOX64_GFX_RICE.  Los usa
 * `GX_InitTexObj`: los dos plugins le dan a esa llamada significados
 * distintos y el shim tiene que saber cual esta puesto.  La cabecera lleva
 * su propio `extern "C"`. */
#include "../xbox/plataforma.h"

extern "C" void DebugOutputString(const char *s);

/* [Kirby 64] El modo de marcado `sinz` (la definicion y el porque estan en
 * xenon/gln64_prof.h; la variable la define gln64_config.cpp).  Se declara
 * suelta para no traer aqui la cabecera entera de las sondas de glN64. */
extern "C" volatile int gln64_marcaluz;
#ifndef GLN64_MARCA_SINZ
#define GLN64_MARCA_SINZ 17
#endif

/* ★ El monton de texturas del plugin: `Textures.cpp:53` lo define y
 * `FrameBuffer.cpp:45` / `VI.cpp:416` lo declaran asi, A PELO.  O sea ENLACE
 * C++, y hay que declararlo FUERA de cualquier `extern "C"` o el enlazador
 * pide el nombre sin decorar (la regla larga esta en gln64_glue.cpp).
 *
 * Solo se usa para comparar punteros en el gancho de liberacion. */
extern heap_cntrl *GXtexCache;

/* ★★ [Fase 6] Y EL DE Rice, QUE ES OTRO MONTON DISTINTO.
 *
 * Rice define el suyo en `TextureManager.cpp:37` y `rice_rename.h` lo
 * prefija, porque los dos plugins declaran una global con el mismo nombre.
 * Este fichero no lleva `__GX__`, asi que no ve el renombrado y la
 * comparacion de abajo miraba SIEMPRE el de glN64 -- que con Rice activo
 * vale NULL, porque glN64 no ha arrancado.
 *
 * ★ EL FALLO NO HABRIA DADO LA CARA COMO UN FALLO.  `gxd3d_tex_free_hook`
 * se habria salido por el `return` en TODAS las liberaciones, o sea que el
 * registro de texturas y el de resueltas nunca se enterarian de que una
 * direccion se ha liberado.  El monton REUTILIZA direcciones, asi que el
 * sintoma seria una textura servida en el sitio de otra, de vez en cuando,
 * y un `texconv` que no sube al cambiar de escenario (ver la nota de
 * arriba: es el sintoma que ya costo encontrar una vez). */
extern heap_cntrl *Rice_GXtexCache;

/* ★★ Y EL LIMITE DE DESALOJO DE RICE, QUE ES OTRA VARIABLE.
 *
 * `Rice_GX/TextureManager.cpp:546` desaloja mientras
 * `GXnumTexBytes + hueco > g_maxTextureMemUsage`, asi que agrandar el monton
 * sin subir esto no serviria de nada: seguiria desalojando al llegar al MB.
 *
 * ★★ SE DECLARA `DWORD`, Y ESO IMPORTA: ES UN SIMBOLO DE C++, DECORADO.
 *
 * `Rice_GX/TextureManager.cpp` la define como `DWORD` y no esta dentro de
 * ningun `extern "C"`, asi que el enlazador pide el nombre decorado CON EL
 * TIPO.  `unsigned long` decora `K` y `unsigned int` decora `I`: si los dos
 * lados no usan el mismo, sale un LNK2001 que menciona el tipo.
 *
 * ★ Y el `DWORD` que ve Rice es el del XDK, `unsigned long`, aunque
 * `source/winlnxdefs.h:35` parezca decir `unsigned int`: esa linea esta
 * DENTRO de un `#ifndef _XTL_` que empieza en :32, y justo antes, en :31,
 * el propio fichero hace `#include <xtl.h>`.  O sea que el bloque entero de
 * typedefs de repuesto NO ENTRA NUNCA en este build.  Leer la linea sin
 * mirar su guarda ya costo un enlace fallido.
 *
 * No esta en `rice_rename.h` porque glN64 no tiene ninguna global con ese
 * nombre. */
extern DWORD g_maxTextureMemUsage;

/* ★★★ EL MONTON DE TEXTURAS DE RICE ERA DE 1 MB; EL DE glN64, DE 32.
 *
 * `GX_TEXTURE_CACHE_SIZE` (TextureManager.cpp:32) se resuelve sin `HW_RVL` y
 * con `USE_EXPANSION` -- que el vcxproj define en las cuatro
 * configuraciones -- a **1 MB**, que es el valor de una GameCube con
 * Expansion Pak.  glN64 no lo sufre porque `gln64_tex_heap_setup`
 * (gln64_config.cpp) le monta 32 MB antes de arrancar... y esa funcion
 * cuelga de `Config_LoadConfig`, que solo invoca `GlN64_initiateGFX`: **con
 * Rice no corre jamas**.
 *
 * Es el CUARTO caso de la misma familia (`gfx_set_window`, `GXtexCache`,
 * `gln64_ucode_apto_para_hilo`): la adaptacion a Xbox 360 se hizo del lado
 * de glN64 y Rice se quedo con el numero de 2001.
 *
 * Efecto en cadena, con el camino en el arbol: monton lleno -> desalojo de
 * 32 KB en 32 KB (`g_amountToFree`) -> fallo de CRC -> TextureManager.cpp:776
 * llama a `GX_DrawDone()` + `GX_InvalidateTexAll()` -> vaciado del lote y los
 * cuatro samplers a NULL -> y reconversion de los texels en el siguiente
 * `GX_LoadTexObj`.
 *
 * ★ POR QUE NO SE PUEDE USAR EL TRUCO DE glN64, QUE ES LO PRIMERO QUE PROBE.
 * Alli el monton se pre-monta antes de que el plugin llegue a su
 * `if (!GXtexCache)`.  Aqui no hay hueco: quien monta el de Rice es el
 * CONSTRUCTOR de `CTextureManager`, y `gTextureManager` es un objeto GLOBAL
 * (TextureManager.cpp:40), o sea que corre en la inicializacion estatica,
 * antes que cualquier codigo nuestro.  Asi que se SUSTITUYE despues.
 *
 * ★ Y sustituir es seguro SOLO porque todavia no hay ni una textura dentro:
 * esto se llama desde `initiateGFX` (xbox/gfx_dispatch.c) y las texturas se
 * reservan al procesar display lists, mucho despues.  `gGX.GXnumTexBytes`
 * vale 0 en ese instante.  Si algun dia se moviera la llamada a un punto
 * posterior, cada textura viva se quedaria apuntando a una arena huerfana.
 *
 * ★ La arena de 1 MB se pierde, una vez por sesion.  No se libera a
 * proposito: el puntero que haria falta esta dentro del `heap_cntrl` y salio
 * de `memalign`, que en este arbol es `_aligned_malloc` (gccore.h:286);
 * deducirlo de la estructura interna del monton para pasarlo a
 * `_aligned_free` es justo la clase de apano que rompe en silencio.  Un MB
 * en una consola de 512, y upstream tampoco libera la suya nunca.
 *
 * ★ QUEDA PENDIENTE, y se anota aqui para que no se pierda:
 * `m_numOfCachedTxtrList` son 131 cubos de hash sin `HW_RVL`
 * (TextureManager.cpp:63), contra 809 en Wii.  Con 32 MB caben muchas mas
 * texturas en las mismas 131 cadenas.  No se toca porque tambien se fija en
 * el constructor y esta en el arbol copiado. */
extern "C" void xbox64_rice_tex_heap_setup(void)
{
    static const unsigned int tam[4] = {
        32u * 1024u * 1024u,
        16u * 1024u * 1024u,
         8u * 1024u * 1024u,
         4u * 1024u * 1024u
    };
    static int  s_hecho = 0;
    heap_cntrl *ctrl;
#if XBOX64_TRAZAS
    char        ob[176];
#endif
    int         i;

    if (s_hecho)
        return;
    s_hecho = 1;

    /* El descriptor por `malloc` y fuera del bucle, igual que en
     * `gln64_tex_heap_setup`: la arena va por `_aligned_malloc` y mezclar
     * asignadores al deshacerla seria un fallo diferido. */
    ctrl = (heap_cntrl *)malloc(sizeof(heap_cntrl));
    if (ctrl)
    {
        for (i = 0; i < 4; i++)
        {
            void *arena = _aligned_malloc(tam[i], 32);
            if (!arena)
                continue;

            __lwp_heap_init(ctrl, arena, tam[i], 32);
            /* Solo ahora, y los dos JUNTOS: el monton sin el limite no sirve
             * de nada, y el limite sin el monton haria que
             * `__lwp_heap_allocate` devolviera NULL -- que en Rice es una
             * textura negra sin ningun aviso. */
            Rice_GXtexCache      = ctrl;
            g_maxTextureMemUsage = tam[i];

#if XBOX64_TRAZAS
            sprintf(ob, "[RICE] monton de texturas = %u MB"
                        " (el del plugin eran %u KB)",
                    tam[i] / (1024u * 1024u), 1024u);
            DebugOutputString(ob);
#endif
            return;
        }
        free(ctrl);
    }

    /* Sin memoria: se queda el del plugin y se DICE.  Que el sintoma sea
     * "va lento" sin una linea que lo explique es lo que costo encontrarlo
     * la primera vez. */
#if XBOX64_TRAZAS
    DebugOutputString("[RICE] monton de texturas: SIN MEMORIA, se queda el"
                      " del plugin (1024 KB) -- habra reconversion en cadena");
#endif
}

/* Definidas mas abajo; se declaran aqui porque GX_SetCopyClear y GX_CopyTex
 * las usan antes de su definicion. */
/* ------------------------------------------------------------------ */
/* ★★ LOTE DIFERIDO: FUSIONAR DRAW CALLS                               */
/* ------------------------------------------------------------------ */
/* MEDIDO antes de escribir esto ([GLN64D]): de los 70,62 ms/s que cuesta la
 * cola de `GX_End`, `bind`=0,17, `mtx`=0,72, `tev`=13,44 y **`prim`=55,69**.
 * O sea 8,0 us por `DrawPrimitive`, con 6.938 al segundo -- cuando en Xenos
 * un draw call deberia costar 1-2 us.
 *
 * La causa es estructural: en GX un `GX_Begin`/`GX_End` es una escritura al
 * FIFO y el plugin hace cientos de lotes diminutos por frame (12-18
 * vertices, 4-6 triangulos).  Traducir cada uno a un draw call de D3D9 es
 * el peaje.
 *
 * LA IDEA: no dibujar en `GX_End`.  Como los vertices ya se escriben
 * SEGUIDOS en el mismo buffer, dos lotes consecutivos con el mismo estado y
 * la misma primitiva son un solo rango: basta con extenderlo.  Se vacia
 * cuando cambia algo que afecte al dibujo.
 *
 * ★ PRERREQUISITO, y sin el no fusiona nada: los setters tienen que
 * COMPARAR antes de ensuciar.  `OGL_DrawTriangles` recarga las dos matrices
 * antes de cada lote y `TextureCache_ActivateTexture` religa la textura en
 * cada draw, casi siempre con el mismo valor.  Por eso `GX_LoadPosMtxImm`,
 * `GX_LoadProjectionMtx` y `GX_LoadTexObj` llevan `memcmp`/comparacion. */
static D3DPRIMITIVETYPE s_pendPrim   = D3DPT_TRIANGLELIST;
static int              s_pendStart  = 0;
static int              s_pendCount  = 0;   /* vertices acumulados */
static int              s_pendActive = 0;

extern "C" volatile unsigned int gxsh_merged = 0;  /* lotes fusionados */

static void gxd3d_flush_pending(void);

static void gxd3d_set_clear_impl(GXColor c, u32 z);
static void gxd3d_resolve_to_gx_buffer(void *dest, u16 sl, u16 st, u16 sw, u16 sh,
                                       u16 dw, u16 dh, u32 fmt);

/* ------------------------------------------------------------------ */
/* Contadores                                                          */
/* ------------------------------------------------------------------ */
/* ==========================================================================
 *  ★★★ VOLCADO DE TEXTURAS A BMP, ARMADO CON L3+R3.
 *
 *  Portado de Xenon64 (xenon/fb_gln64.cpp:509-610 y d3d9_backend.cpp:1791),
 *  donde ya existia y funcionaba.  Aqui hacia falta para 007 TWINE: medido
 *  que las paredes, la mesa y los personajes salen negros con la PALETA
 *  perfecta (`rgbmax=7FFF negras=0`), los indices sanos (`idxcero` 0,5%) y el
 *  color de vertice en BLANCO MAXIMO (`conrasc rgba=255,255,255,255`).  Con
 *  `TEXEL0 * SHADE` y SHADE blanco, el resultado ES la textura -- asi que hay
 *  que VERLAS para saber cual sale negra y en que se diferencia de las
 *  baldosas del suelo, que son lo unico que se ve.
 *
 *  ★ SE VUELCA EN LA CONVERSION, NO EN EL ATADO.  El buffer `tmp` que sale de
 *  `gxd3d_untile_*` es ARGB LINEAL en memoria de CPU, justo antes de subirlo.
 *  Volcar desde la textura de D3D obligaria a un `LockRect` sobre una textura
 *  tileada, que en esta consola es una trampa conocida
 *  ([[reference_xbox360_lockrect_no_tilea]]).
 *
 *  El nombre lleva todo lo necesario para identificarla sin cruzar dos
 *  volcados: indice, formato GX, tamanyo, puntero de origen en el heap de
 *  texturas y el mux del combinador activo.
 * ========================================================================== */
extern "C" { volatile int g_texbmpArm = 0; }

static int         s_tbmpActivo = 0;
static const void *s_tbmpVistas[256];
static int         s_tbmpN      = 0;
static const char *s_tbmpDir    = 0;
static int         s_tbmpBuscado = 0;
/* Contadores de PASO del volcado.  Un "0 texturas volcadas" puede significar
 * dos cosas opuestas -- "no hay texturas" o "por aqui no pasa nadie"-- y la
 * primera version no sabia distinguirlas: el volcado colgaba de la rama de
 * CONVERSION, que solo corre cuando la textura FALLA en el registro del shim,
 * y en regimen estable no falla ninguna.  Con estos dos se lee directo. */
static unsigned int s_tbmpAtados  = 0;   /* GX_LoadTexObj atendidos           */
static unsigned int s_tbmpConvs   = 0;   /* de esos, re-destilados            */
static unsigned int s_tbmpFallos  = 0;   /* BMP que no se pudieron escribir   */

/* ★ La cache de texturas DEL SHIM, con palanca.
 *
 * Es el registro `s_texReg` (puntero+w+h+formato -> IDirect3DTexture9*).  Dos
 * usos, y por eso la condicion es un OR y no una opcion suelta:
 *
 *   1. Diagnostico: si una textura se ve mal y apagando esto se arregla, el
 *      defecto es que se estaba sirviendo una convertida OBSOLETA (el monton
 *      del plugin reutiliza direcciones; hay un gancho en el free para eso,
 *      pero no cubre una reescritura EN SITIO de los mismos texels).
 *   2. El volcado a BMP: mientras esta armado hay que forzar el fallo para
 *      que la rama de conversion vuelva a correr y produzca `tmp`.
 *
 * Apagada cuesta una conversion por atado (~12.700/s): es una herramienta de
 * medida, no un modo de juego. */
static int          s_texCacheOn = 1;
extern "C" void gxd3d_texcache_habilitar(int on) { s_texCacheOn = on ? 1 : 0; }

/* Volcado TEXBMP: herramienta de diagnostico, solo con trazas.  Sin ellas
 * `g_texbmpArm` existe (L3+R3 lo arma) pero nadie lo atiende. */
#if XBOX64_TRAZAS
/* Para que el plugin escriba, en el MISMO frame del volcado, el detalle de
 * cada textura con el puntero que lleva el nombre del BMP ([TEXPAR*] y los
 * .bin, glN64_GX/Textures.cpp _texSondaPar). */
extern "C" int gxd3d_tbmp_activo(void) { return s_tbmpActivo; }

static void tbmp_pon32(unsigned char *p, unsigned int v)
{   /* BMP es LITTLE-ENDIAN y la consola BIG-ENDIAN: byte a byte, a mano. */
    p[0] = (unsigned char)( v        & 0xFF);
    p[1] = (unsigned char)((v >>  8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}
static void tbmp_pon16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)( v       & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static const char *tbmp_dir(void)
{
    /* `game:\` el primero: es la raiz del juego y es donde se mira. */
    static const char *cand[] = { "game:\\", "devkit:\\", "cache:\\", "D:\\" };
    int  i;
    char ruta[128];

    if (s_tbmpBuscado) return s_tbmpDir;
    s_tbmpBuscado = 1;

    for (i = 0; i < (int)(sizeof(cand) / sizeof(cand[0])); i++)
    {
        FILE *f;
        sprintf(ruta, "%stexbmp.tmp", cand[i]);
        f = fopen(ruta, "wb");
        if (f)
        {
            char aviso[192];
            fwrite("TEX", 1, 3, f);
            fclose(f);
            s_tbmpDir = cand[i];
            sprintf(aviso, "[TEXBMP] escribiendo los BMP en %s", s_tbmpDir);
            DebugOutputString(aviso);
            return s_tbmpDir;
        }
    }
    DebugOutputString("[TEXBMP] NINGUNA ruta escribible; no habra BMP."
                      "  Probadas: game:\\ devkit:\\ cache:\\ D:\\");
    return 0;
}

/* El mismo directorio para los .bin del detalle del volcado ([TEXPAR*] del
 * plugin: TMEM y RDRAM de cada textura). */
extern "C" const char *gxd3d_tbmp_dir(void) { return tbmp_dir(); }

/* BMP de 32 bits TOP-DOWN (biHeight negativo), sin comprimir.  Cada pixel
 * sale al fichero como B,G,R,A. */
static int tbmp_fichero(const char *nombre, const void *argb,
                        int w, int h, int pitch)
{
    const char   *dir = tbmp_dir();
    unsigned char cab[54];
    unsigned char fila[1024 * 4];
    char          ruta[256];
    FILE         *f;
    int           x, y;

    if (!dir || !argb || w <= 0 || h <= 0 || w > 1024) return 0;

    sprintf(ruta, "%s%s.bmp", dir, nombre);
    f = fopen(ruta, "wb");
    if (!f) return 0;

    memset(cab, 0, sizeof(cab));
    cab[0] = 'B'; cab[1] = 'M';
    tbmp_pon32(cab +  2, (unsigned int)(54 + w * h * 4));
    tbmp_pon32(cab + 10, 54u);
    tbmp_pon32(cab + 14, 40u);
    tbmp_pon32(cab + 18, (unsigned int)w);
    tbmp_pon32(cab + 22, (unsigned int)(-h));
    tbmp_pon16(cab + 26, 1u);
    tbmp_pon16(cab + 28, 32u);
    tbmp_pon32(cab + 34, (unsigned int)(w * h * 4));
    fwrite(cab, 1, sizeof(cab), f);

    for (y = 0; y < h; y++)
    {
        const unsigned int *src = (const unsigned int *)
            ((const unsigned char *)argb + (size_t)y * (size_t)pitch);
        for (x = 0; x < w; x++)
        {
            unsigned int p = src[x];
            fila[x * 4 + 0] = (unsigned char)( p        & 0xFF);  /* B */
            fila[x * 4 + 1] = (unsigned char)((p >>  8) & 0xFF);  /* G */
            fila[x * 4 + 2] = (unsigned char)((p >> 16) & 0xFF);  /* R */
            fila[x * 4 + 3] = (unsigned char)((p >> 24) & 0xFF);  /* A */
        }
        fwrite(fila, 1, (size_t)w * 4u, f);
    }
    fclose(f);
    {
        char aviso[256];
        sprintf(aviso, "[TEXBMP] %s  %dx%d", ruta, w, h);
        DebugOutputString(aviso);
    }
    return 1;
}
#endif /* XBOX64_TRAZAS (TEXBMP) */

extern "C" volatile unsigned int gxd3d_dbg_draws   = 0;
extern "C" volatile unsigned int gxd3d_dbg_verts   = 0;
extern "C" volatile unsigned int gxd3d_dbg_texconv = 0;
extern "C" volatile unsigned int gxd3d_dbg_resolve = 0;

/* ★ Rects degenerados de viewport/scissor.  Esta sonda existe porque su
 * ausencia costo una sesion: los clamp que GX_SetScissor/GX_SetViewport
 * necesitan para que D3D no rechace el rect convierten un 0x0 en un 1x1, y
 * eso CALLA el defecto en vez de gritarlo.  Con OGL.GXwidth a cero la escena
 * entera se dibujaba en el pixel (0,0) y el log decia que todo iba bien
 * (draws, verts y shaders sanos).  Si este contador no es 0, el rect viene
 * degenerado de arriba y no hay que buscar en el combinador. */
extern "C" volatile unsigned int gxd3d_dbg_degen   = 0;

/* ★ La niebla.  `[GXFOG]` fue lo que cerro el caso del escenario negro de 007
 * TWINE: decia `on=180 col=000000 ini=0.1 fin=1.0` mientras el resto de la
 * cadena estaba sana.  Se deja porque la niebla no tiene otra medida. */
extern "C" volatile unsigned int gxd3d_dbg_fogon   = 0;
extern "C" volatile unsigned int gxd3d_dbg_fogoff  = 0;
extern "C" volatile unsigned int gxd3d_dbg_fogcol  = 0;
extern "C" volatile float        gxd3d_dbg_fogini  = 0.0f;
extern "C" volatile float        gxd3d_dbg_fogfin  = 0.0f;

/* ★ Vertices DESCARTADOS por buffer lleno.  Los dos sitios que escriben al
 * vertex buffer llevan un `if (s_vbUsed < GXD3D_MAX_VERTS)` que, al llenarse,
 * se come el vertice SIN DECIR NADA.  Con la cuenta estimada (~3.500
 * vertices/frame contra 49.152 de capacidad) no deberia pasar nunca, pero es
 * un camino de fallo silencioso y las bandas de Paper Mario son justamente
 * geometria que falta: se mide en vez de suponerse.  Si sale != 0, ahi esta. */
extern "C" volatile unsigned int gxd3d_dbg_vbdrop  = 0;

/* ★ Draws emitidos con Z-TEXTURE en REPLACE, o sea con el pixel shader
 * escribiendo la profundidad.  Es el camino de `G_ZS_PRIM` (profundidad de
 * primitiva), que glN64 enciende en OpenGL.cpp:801 y que es justo lo que usa
 * un fondo 2D dibujado por tiras.  Si las tiras que faltan en Paper Mario son
 * de este camino, `ztex` sera grande; si sale 0, el camino no esta en juego y
 * hay que mirar a otro sitio.  Sin el, distinguirlo cuesta un build. */
extern "C" volatile unsigned int gxd3d_dbg_ztex    = 0;

/* ★ Draws emitidos con un recorte que NO cubre el render target entero, y el
 * area del rect mas pequeno que se ha usado para dibujar.
 *
 * El bisect apunta al recorte, y la sonda [SCIS-GX] enseño por que puede ser:
 * el plugin pide rects pequenos en la esquina (260x108, 248x30, 176x32...)
 * para las pasadas a buffers fuera de pantalla -- bocadillos, marcadores--, y
 * como aqui NO hay render-to-texture (`resolve=0`), esas pasadas caen en el
 * render target PRINCIPAL y su recorte se traga la escena.
 *
 * `scisclip` grande = confirmado.  `scisclip` ~0 = la teoria se cae y el
 * recorte solo estorba de otra forma. */
extern "C" volatile unsigned int gxd3d_dbg_scisclip = 0;
/* ★★ ERA UN CONTADOR MUERTO: se calculaba y no lo imprimia nadie, y ademas
 * no se reiniciaba nunca.  Se arregla porque es justo lo que hace falta para
 * el cuadro negro de Majora's Mask: si el recorte mas pequenyo del segundo
 * mide media pantalla, el cuadrante deja de ser un misterio. */
extern "C" volatile unsigned int gxd3d_dbg_scismin  = 0xFFFFFFFFu;
extern "C" volatile unsigned int gxd3d_dbg_scisminw = 0;
extern "C" volatile unsigned int gxd3d_dbg_scisminh = 0;

/* ★ Los borrados del EFB, contados DONDE SE HACEN (`gxd3d_clear_efb`).
 *
 * Por que importa: si el z-buffer no se limpia, la misma geometria a la
 * misma profundidad pasa un frame y falla el siguiente -- parpadeo a 30 Hz,
 * peor donde hay muchas capas 2D solapadas.  `clrz` cerca de 0 con el juego
 * a 60 display lists/s seria exactamente eso.  Y `clrrgb` guarda el ULTIMO
 * color de borrado, que es lo primero que hay que mirar cuando aparece una
 * capa de color sobre la escena.
 *
 * ★★ AQUI HABIA `zclear`/`efbclear`, Y EL REBASE LOS MATO SIN AVISAR.
 * Contaban `GX_SetZMode(GX_GEQUAL)` porque esa ERA la funcion de Z del quad
 * de borrado; Wii64 1b5041a la cambio a `GX_ALWAYS` y desde entonces valian
 * 0 SIEMPRE.  Un 0 parece "no pasa nada" cuando lo que pasa es que la sonda
 * ya no mira donde toca: por eso se cuenta en el sitio del borrado y no en
 * un rasgo indirecto suyo. */
extern "C" volatile unsigned int gxd3d_dbg_clrcol   = 0;
extern "C" volatile unsigned int gxd3d_dbg_clrz     = 0;
extern "C" volatile unsigned int gxd3d_dbg_clrrgb   = 0;   /* ultimo color, 0x00RRGGBB */
/* Viewports con un rango de Z fuera del [0,1] que admite D3D.  Ver
 * GX_SetViewport y la nota de GLN64_ZGUARD en glN64_GX/OpenGL.h. */
extern "C" volatile unsigned int gxd3d_dbg_vpz      = 0;

/* ★ Por que Zelda OoT se hunde con la emulacion de framebuffer encendida.
 * Hay DOS costes candidatos y estos contadores los separan:
 *
 *   rtthit  activaciones de textura que SI encuentran su imagen ya resuelta
 *           en el registro de `GX_CopyTex`.  Es el camino barato.  Si sale
 *           ~0 mientras `resolve` es alto, cada carga de framebuffer-como-
 *           textura se esta yendo por el camino NORMAL, o sea destilando
 *           320x240 texels POR CPU en cada activacion.  Ese seria el cuello.
 *
 *   rtt_tb  tiempo dentro de `gxd3d_resolve_to_gx_buffer`, que incluye el
 *           `d3dq_sinc()` -- un drenaje de la cola de D3D.  Si el cuello es
 *           este, es sincronizacion con la GPU y no trabajo de CPU. */
extern "C" volatile unsigned int gxd3d_dbg_rtthit   = 0;
extern "C" volatile unsigned int gxsh_rtt_tb        = 0;
extern "C" volatile unsigned int gxsh_rtt_n         = 0;

/* Entradas del registro de texturas tiradas porque el plugin libero su buffer.
 * Se lee EN PAREJA con `texconv`: si `texconv` apenas sube y esto es 0, el
 * registro esta sirviendo texturas de direcciones ya recicladas. */
extern "C" volatile unsigned int gxd3d_dbg_texfree = 0;
/* ★ [Fase 6] Cuantas veces se LLAMA a `GX_InitTexObj` y cuantas de esas
 * INVALIDAN de verdad.  Existen porque afirme sin medirlo que con Rice esa
 * llamada solo ocurre al reconstruir una textura; si el ritmo se parece al
 * de `draws`, la afirmacion es falsa y la invalidacion de ahi hay que
 * quitarla. `texfree` de arriba ya cuenta las bajas por liberacion del
 * monton, asi que las dos vias quedan separadas en el log. */
extern "C" volatile unsigned int gxd3d_dbg_initobj = 0;
extern "C" volatile unsigned int gxd3d_dbg_texinval = 0;
/* [Cruis'n USA] Las lapidas del registro de texturas: ver gxd3d_tex_compacta. */
static unsigned int s_texLapidas = 0;

/* ★ Entradas VIVAS del registro de texturas resueltas que el LRU ha tirado.
 * Cada una es una textura de framebuffer que a partir de ese momento se
 * dibujaria NEGRA.  Tiene que ser 0; si no, GXD3D_RTT_MAX se queda corto.
 * Ver la nota larga de GXD3D_RTT_MAX. */
extern "C" volatile unsigned int gxd3d_dbg_rttdesal = 0;

/* ★ Draws CON y SIN textura ligada en el sampler 0.
 *
 * Esta es la medida que tenia que haber hecho hace cuatro hipotesis. El
 * reparto observado es: lo que dibuja `OGL_DrawTexturedRect` -- que liga la
 * textura EL MISMO (OpenGL.cpp:1796-1831)-- sale texturizado, y lo que
 * dibuja `OGL_DrawTriangles` -- que depende de
 * `TextureCache_ActivateTexture`-- sale plano. Estos dos contadores dicen si
 * eso es porque la textura no llega a ligarse, o porque llega y el problema
 * esta en las UV o en el shader. */
/* Draws con textura ligada que el shader NO muestrea (numTexGens == 0): el
 * generador les pone texc = blanco.  Ver el sitio donde se cuenta. */
extern "C" volatile unsigned int gxd3d_dbg_texnogen  = 0;

/* ★★ ACTIVACIONES DE FRAMEBUFFER-COMO-TEXTURA QUE NO ESTAN RESUELTAS.
 *
 * El mecanismo lo describe profile.c junto a `rttdesal`: una textura de
 * framebuffer cuyo GX_LoadTexObj no encuentra su entrada en el registro se
 * va por el camino NORMAL y destila el buffer del plugin -- memoria que
 * NADIE ha escrito, porque nosotros guardamos la imagen en una textura de
 * D3D y no en texels en formato GX.  El resultado es negro o basura.
 *
 * `rttdesal` solo cubre UNA causa de ese fallo: que el LRU haya tirado la
 * entrada.  Sale 0 en Pokemon Stadium, asi que si el sintoma esta ahi, la
 * entrada nunca llego a existir -- p.ej. el buffer no se resolvio jamas
 * porque `gDP.colorImage.changed` era FALSE cuando se salio de el.
 *
 * Esto cuenta el fallo COMPLETO, sea cual sea la causa: lo llama
 * FrameBuffer_ActivateBufferTexture, o sea exactamente una vez por
 * activacion de framebuffer-como-textura, y comprueba si hay imagen.
 *
 * Los dos sintomas abiertos encajan con esto: los rectangulos negros del HUD
 * de combate y los retratos de Pokemon en blanco (los once buffers de
 * 128x96 de la rejilla). */
extern "C" volatile unsigned int gxd3d_dbg_fbmiss = 0;
extern "C" volatile unsigned int gxd3d_dbg_fbact  = 0;

extern "C" volatile unsigned int gxd3d_dbg_drawtex   = 0;
extern "C" volatile unsigned int gxd3d_dbg_drawnotex = 0;

/* Espejo de lo ultimo ligado en cada sampler: NULL o no. */
static int s_texBound[4] = { 0, 0, 0, 0 };

/* Espejo COMPLETO del sampler: textura y los cuatro filtros/wrap.  Se
 * invalida con el resto del estado en `gxd3d_frame_begin`, porque el device
 * esta compartido con el frontend. */
struct GXD3DSampMirror
{
    IDirect3DTexture9 *tex;
    DWORD au, av, mn, mg;
    int   valid;
};
static GXD3DSampMirror s_samp[4];

/* ¿Siguen puestos NUESTROS vertex shader, declaracion y stream source?  Se
 * invalida una vez por pasada en `gxd3d_frame_begin`, porque el frontend
 * comparte el device y liga los suyos ENTRE frames -- nunca a mitad. */
static int s_bindValid = 0;

/* ------------------------------------------------------------------ */
/* ★ RELOJES DEL SHIM, POR PARTES                                      */
/* ------------------------------------------------------------------ */
/* Los cubos `[GFXP]` del perfilador instrumentan RICE, no este backend, asi
 * que con glN64 solo se veia el total (`[PERF] gfx`) y habia que deducir el
 * reparto.  Estos cubos lo miden.
 *
 * ★ Y el que de verdad contesta la pregunta es el RESTO: `gfx` menos la suma
 * de estos es el trabajo de CPU DEL PROPIO PLUGIN (T&L en gSP.cpp, la logica
 * del combinador, la cache de texturas).  Eso no se puede medir desde el
 * shim sin tocar glN64 -- y no se toca-- pero por diferencia si se acota.
 *
 * Granularidad elegida para que la sonda no falsee la medida: `lote` cubre
 * de GX_Begin a GX_End entero (2 lecturas del timebase por lote, ~7.000/s,
 * despreciable).  Poner relojes en GX_Position/Color/TexCoord seria 2
 * lecturas por ATRIBUTO -- del orden de un millon por segundo-- y mediria
 * sobre todo la propia sonda. */
extern "C" volatile unsigned int gxsh_lote_tb  = 0;  /* GX_Begin..GX_End    */
extern "C" volatile unsigned int gxsh_lote_n   = 0;
extern "C" volatile unsigned int gxsh_draw_tb  = 0;  /* cola de GX_End      */
extern "C" volatile unsigned int gxsh_tex_tb   = 0;  /* GX_LoadTexObj       */
extern "C" volatile unsigned int gxsh_tex_n    = 0;
extern "C" volatile unsigned int gxsh_conv_tb  = 0;  /* destilar + subir     */
extern "C" volatile unsigned int gxsh_state_tb = 0;  /* SetRenderState y cia */

/* ★ La cola de GX_End, partida.  Salio 65,28 ms/s para 7.016 lotes = 9,3 us
 * por lote, y eso es demasiado para lo que hace.  Descartado ya por calculo
 * que sea la busqueda lineal en la cache de shaders (42 entradas x 3.463
 * consultas/s ~ 18 MB/s de memcmp, unos pocos ms).  Estos cuatro lo dicen
 * en vez de deducirlo. */
/* ★★ EL `Lock` DEL VERTEX BUFFER, APARTE.
 *
 * Es el unico sitio del camino de frame donde el driver puede PARAR a la CPU
 * esperando a la GPU, y hasta ahora caia en tierra de nadie: lo llama
 * `gxd3d_frame_begin`, que corre ANTES de que el wrapper arranque el
 * cronometro de `emu`.
 *
 * Y tiene precedente EN ESTE MISMO FICHERO (ver GXD3D_VB_COUNT): *"con UN
 * solo buffer el Lock cuesta 5,41 ms/frame porque espera a que la GPU suelte
 * el de la ultima vez; con tres, 0,00"*.  Esa medida se tomo con la GPU
 * holgada.  Ahora sabemos que va al limite -- bajar el relleno a la cuarta
 * parte tira `prim` de 25.037 a 970 ns -- y con la GPU mas de dos frames por
 * detras, tres buffers dejan de bastar y el Lock vuelve a esperar.
 *
 * Si `lock` sale en milisegundos, la respuesta es subir GXD3D_VB_COUNT. */
extern "C" volatile unsigned int gxsh_lock_tb = 0;
extern "C" volatile unsigned int gxsh_lock_n  = 0;

extern "C" volatile unsigned int gxsh_d_bind_tb = 0;  /* VS + decl + stream  */
extern "C" volatile unsigned int gxsh_d_mtx_tb  = 0;  /* subir matrices      */
extern "C" volatile unsigned int gxsh_d_tev_tb  = 0;  /* gxd3d_tev_apply     */
extern "C" volatile unsigned int gxsh_d_prim_tb = 0;  /* DrawPrimitive       */

/* ★ El coste del CRC de la cache de texturas del plugin.  La sonda vive en
 * `source/main/xxhash.c` (fuera del arbol de glN64), pero la DEFINICION
 * tiene que estar aqui: ese fichero se compila DOS VECES, porque
 * `glN64_GX/CRC.cpp` lo incluye entero via `XXH_PRIVATE_API`.  Definirlo
 * alli daba LNK4006 y dejaba en manos del enlazador cual de las dos copias
 * se incrementaba. */
extern "C" volatile unsigned int gxsh_hash_tb = 0;
extern "C" volatile unsigned int gxsh_hash_n  = 0;
extern "C" volatile unsigned int gxsh_hash_by = 0;

static unsigned long long s_loteT0 = 0;

/* ★ EL MAXIMO DE |UV| EN TEX0, SEPARADO POR PRIMITIVA.
 *
 * `TEV_BISECT 4` (las UV como color) mostro que los rects del HUD salen con
 * UN degradado suave -- UV en [0,1], correctas-- y que la geometria de
 * triangulos sale con el degradado repetido MUCHAS veces.  La prueba de que
 * eso es un defecto y no un tileado legitimo son los ARBOLES: un billboard
 * pequeno no puede repetir su textura, y repite.
 *
 * Lo que falta es el FACTOR, porque el factor identifica la division que se
 * ha perdido (32 = el S10.5 del N64, 64, 256, 65536...).  De ahi estos dos:
 * si `uvtri` sale ~32 y `uvrect` ~1, el defecto esta en el camino de
 * triangulos y el factor es el S10.5.
 *
 * Se guardan como entero (parte entera del maximo) porque los contadores del
 * perfilador son `unsigned int`. */
extern "C" volatile unsigned int gxd3d_dbg_uvtri  = 0;
extern "C" volatile unsigned int gxd3d_dbg_uvrect = 0;

/* ★★ A 0 POR RENDIMIENTO, Y ESA ES LA LECCION.
 *
 * Esta sonda resolvio el bug de las UV (ver abajo), pero vive dentro de
 * `GX_TexCoord2f32`, o sea **por vertice**: con 125.000 vertices/s son
 * 125.000 abs + comparaciones + cinco ramas por segundo en el camino mas
 * caliente del backend.  Medido despues: `lote` = 116ms/s de los cuales
 * solo 41 son el draw, o sea ~75ms/s en emitir vertices -- unos 380 ciclos
 * por llamada de atributo, que es muchisimo.
 *
 * Una sonda de diagnostico en un camino caliente hay que APAGARLA cuando ha
 * contestado su pregunta.  A 1 vuelve, y la linea [GLN64UV] con ella. */
#define GXD3D_UV_STATS 0

/* ===========================================================================
 * GXD3D_DIAG_SIN_RECORTE / GXD3D_DIAG_SIN_ZTEST -- segundo paso del bisect
 * ===========================================================================
 * TEMPORALES, como GXD3D_DIAG_FONDO (ver gxd3d_frame_begin).  Van aqui
 * arriba y no junto a su comentario hermano porque se usan en GX_SetZMode y
 * GX_SetScissor, que estan antes.
 *
 * `GXD3D_DIAG_FONDO` ya contesto la primera pregunta de las bandas de Paper
 * Mario: salen MAGENTA, o sea que son AGUJEROS -- ahi no dibuja nadie.  Eso
 * mata de paso la hipotesis del z-fighting con la que empece: las bandas
 * "negras" del principio eran el render target sin borrar, no algo pintado
 * encima.
 *
 * La segunda pregunta es POR QUE falta esa geometria, y solo hay tres
 * opciones.  Estas dos banderas eliminan DOS en un solo build:
 *
 *   se rellenan las tiras -> era el RECORTE o el TEST DE PROFUNDIDAD, y el
 *                            build siguiente separa cual dejando una sola a 1
 *   siguen los agujeros   -> las tiras NO SE EMITEN: el defecto esta aguas
 *                            arriba (plugin o camino de framebuffer) y el
 *                            shim queda descartado
 *
 * Mientras esten a 1 la imagen se vera mal por otro lado -- sin recorte hay
 * sobredibujo y sin test de Z se pierde el orden --, y da igual: lo unico
 * que hay que mirar es si las tiras se rellenan.
 *
 * -- BITACORA ---------------------------------------------------------------
 * PASADA 1 (las dos a 1): LAS TIRAS SE RELLENAN.  O sea que esos dibujos SI
 * se emiten y algo los rechazaba.  Falta separar cual de los dos.
 *
 * PASADA 2 (GXD3D_LOTES=0): las tiras SIGUEN -> la fusion de lotes queda
 * EXONERADA.  Y con ella caen tres hipotesis mas, todas con dato: `ztex=0`
 * (el camino de Z-texture / G_ZS_PRIM no se usa NI UNA VEZ en Paper Mario),
 * `vbdrop=0` (el vertex buffer nunca desborda) y `degen=0` (ni scissor ni
 * viewport degenerados).  Lo unico en pie sigue siendo el par
 * recorte/profundidad de la pasada 1.
 *
 * PASADA 3 (solo el test de Z fuera): las tiras SIGUEN, y lo unico que
 * cambia es que los personajes de dentro de la casa se ven por encima de
 * ella -- o sea que el toggle hizo su trabajo y la profundidad NO es.
 *
 * ★★ CONCLUSION: ES EL RECORTE.  Las tiras se rellenan con (scissor OFF,
 * ztest OFF) y vuelven con (scissor ON, ztest OFF), asi que lo que las quita
 * es el scissor.  Mi inferencia desde la imagen -- "el primer plano
 * sobrevive en esas filas, luego el rechazo es por primitiva" -- era FALSA:
 * lo que se veia entre tiras no era primer plano superviviente.
 *
 * Y con `degen=0` en el log, el rect nunca es degenerado: es un rect VALIDO
 * pero EQUIVOCADO.  De ahi la sonda [SCIS-GX] de GX_SetScissor.
 *
 * Todo a 0 = binario normal. */
/* ★ PASADA 4 (solo el recorte fuera) confirmo el mecanismo con numeros:
 * `scisclip=447771` de `draws=1518111` -- el 29,5% de los dibujos salen con
 * la pantalla recortada-- y `scismin=4752` (132x36), que es uno de los rects
 * de bocadillo que lista [SCIS-GX].
 *
 * Los dos vuelven a 0: el arreglo va en `gxd3d_frame_begin`, que arranca la
 * pasada SIN recortar en vez de heredar lo que dejara el frontend. */
#define GXD3D_DIAG_SIN_RECORTE 0
#define GXD3D_DIAG_SIN_ZTEST   0

/* [SCIS-GX]: volcar los rects de recorte DISTINTOS que pide el plugin, y los
 * que el clamp cambia.  Ver GX_SetScissor.  Temporal.
 * A 0: el recorte ya esta descartado como causa de las tiras (lo era el
 * solape de EDRAM con el back buffer del frontend), y la lista ya se leyo. */
#define GXD3D_DIAG_SCIS 0

/* Borrar la profundidad al empezar cada pasada.
 *
 * ★ PROBADO Y A 0: el parpadeo no cambio.  Y la medida explico por que la
 * hipotesis estaba mal planteada: `zclear` y `efbclear` salen los dos a ~30/s
 * con 60 display lists/s, o sea que el plugin SI borra -- lo que pasa es que
 * el juego solo limpia en frames alternos, que es lo que hace quien alterna
 * entre DOS color image en RDRAM.  Ese es el hilo bueno ([CIMG-GX]).
 *
 * Se deja a 0 para no arrastrar un cambio de comportamiento que no arregla
 * nada mientras se diagnostica.  Si al final hay que emular el doble buffer,
 * esto vuelve a hacer falta. */
#define GXD3D_ZCLEAR_POR_FRAME 0

/* ===========================================================================
 * ★★ GXD3D_LOTES -- apagar la FUSION DE LOTES, que es invencion MIA
 * ===========================================================================
 * En GX de verdad no existe nada de esto: `GX_Begin`/`GX_End` escriben en la
 * FIFO por el write-gather-pipe y son casi gratis, asi que el plugin no tiene
 * que agrupar nada.  El lote diferido (`s_pend*`) lo anadi yo para que D3D9
 * no pagara un `DrawPrimitive` por primitiva.
 *
 * Y es la pieza que mas defectos ha dado del port entero: CUATRO, todos de la
 * misma forma -- estado que cambia mientras hay un lote acumulado, asi que la
 * geometria se dibuja con el estado del SIGUIENTE (matrices, combinador,
 * mascara de color, sesgo/texturas).  No hay referencia contra la que
 * contrastarla porque upstream no la tiene.
 *
 * Y lo que compra esta MEDIDO y es poco: `[GLN64D] tev+prim` ~38 ms/s de 518,
 * un 7% del hilo grafico en el mejor caso.
 *
 * A 0, cada lote se dibuja en el acto: desaparece la clase entera de fallos.
 * Sirve de las dos cosas a la vez --
 *
 *   se arreglan las tiras -> quedaba un sitio que cambia estado sin vaciar,
 *                            y entonces la pregunta es CUAL (o se deja a 0 y
 *                            se cobra el 7%, que puede ser buen negocio)
 *   siguen las tiras      -> la fusion queda EXONERADA y el defecto es el
 *                            test de profundidad, que es donde apuntaba la
 *                            pasada anterior
 *
 * A 1 vuelve la fusion.
 *
 * >>> A 1 OTRA VEZ (2026-09-28), como A/B de rendimiento en San Francisco
 * 2049.  El motivo es que el perfil de ESE juego no se parece al que dejo
 * esto a 0:
 *
 *     lotes=25389/s draws=25389/s verts=535656/s
 *
 * o sea 564 `DrawPrimitive` por frame con 21 vertices (7 triangulos) cada
 * uno, y el hilo grafico es el cuello (`espera` 9-13 ms de los 16,6).  La
 * medida de arriba -- 7% en el mejor caso -- se tomo en un juego que no
 * emitia lotes asi, asi que no vale para decidir aqui.
 *
 * ★ LO QUE HAY QUE MIRAR AL PROBARLO, y no es el rendimiento: la clase de
 * fallo que dejo esto a 0 son TROZOS QUE DESAPARECEN por cambiar estado sin
 * vaciar el lote ([[reference_gxd3d_estado_sin_vaciar_lote]], cuatro veces
 * ya).  Si reaparecen tiras o faltan trozos de geometria, esto vuelve a 0 y
 * el 7% no vale lo que cuesta. */
#define GXD3D_LOTES 1

/* ★ Y UN HISTOGRAMA, porque el MAXIMO fue el estadistico equivocado.
 *
 * La primera medida dio `triangulos=1806052 rects=1`.  El control (rects)
 * salio perfecto, pero 1,8 millones no identifica ninguna division perdida:
 * lo domina un vertice degenerado.  Lo que decide lo que se VE es el valor
 * TIPICO, y para eso hace falta la distribucion.
 *
 * Cubos sobre |UV0| de los vertices de TRIANGULOS:
 *   uv0_2   <= 2     lo normal (una textura sin repetir, o casi)
 *   uv2_8   <= 8
 *   uv8_32  <= 32    ★ si aqui se acumula todo, el factor es el S10.5
 *   uv32_1k <= 1024
 *   uvbig   > 1024   basura
 */
extern "C" volatile unsigned int gxd3d_dbg_uv0_2   = 0;
extern "C" volatile unsigned int gxd3d_dbg_uv2_8   = 0;
extern "C" volatile unsigned int gxd3d_dbg_uv8_32  = 0;
extern "C" volatile unsigned int gxd3d_dbg_uv32_1k = 0;
extern "C" volatile unsigned int gxd3d_dbg_uvbig   = 0;

/* La unica constante GXColor que el arbol copiado necesita (ver
 * DIVERGENCIAS.txt punto 1). */
const GXColor GXCOL_BLACK = { 0, 0, 0, 255 };
/* [Fase 6] El color de borrado de Rice (OGLRender.cpp:344).  ★ Es negro con
 * ALFA CERO, no el GXCOL_BLACK de arriba: se parecen al leerlos y no hacen
 * lo mismo. */
const GXColor GXCOL_TRANSP = { 0, 0, 0, 0 };

/* ------------------------------------------------------------------ */
/* Vertex buffer en anillo                                             */
/* ------------------------------------------------------------------ */
/* Se copia el diseno de d3d9_backend.cpp, que esta MEDIDO: con un solo
 * buffer el Lock cuesta 5,41 ms/frame porque espera a que la GPU suelte el
 * de la ultima vez; con tres, 0,00.  Es la trampa del "primer D3D que
 * bloquea se come la espera", que en este arbol ha aparecido siete veces. */
#define GXD3D_VB_COUNT        3
#define GXD3D_MAX_VERTS   (16384*3)

static IDirect3DVertexBuffer9 *s_vb[GXD3D_VB_COUNT] = { NULL, NULL, NULL };
static IDirect3DVertexDeclaration9 *s_decl = NULL;
static IDirect3DVertexShader9 *s_vs = NULL;

/* ------------------------------------------------------------------ */
/* ★★ EL VERTEX SHADER.  FALTABA, Y ERA UN CUELGUE                     */
/* ------------------------------------------------------------------ */
/* Sin `SetVertexShader()` el device se queda con el que dejo Rice o el
 * frontend, cuyas SALIDAS no casan con las entradas de nuestro pixel shader
 * (TEXCOORD0/1/2 + COLOR0 + TEXCOORD3).  Xenos intenta entonces parchear el
 * VS para que case con el PS y revienta dentro del propio driver:
 *
 *     memcpy()
 *     D3D::VertexShaderRedirectOutput()
 *     D3D::PatchVertexShaderToMatchPixelShader()
 *     D3DDevice_DrawVertices()  <-  GX_End()  <-  OGL_DrawTriangles()
 *
 * QUE HACE, Y POR QUE ES TAN CORTO: glN64_GX ya hizo el T&L EN CPU.  Lo que
 * llega en el vertice esta practicamente en espacio de recorte
 * (OpenGL.cpp:1388-1398): en el camino de perspectiva manda `(x, y, -w)` y
 * carga `GXcombW`; en el otro manda `(x/w, y/w, z/w)` ya dividido y con la
 * z recortada a [-1,1], y carga `GXprojIdent`.  O sea que aqui solo hay que
 * aplicar las dos matrices y repartir atributos.
 *
 * ★ CONVENIO DE LAS MATRICES, para no volver a dudar: se suben por FILAS con
 * SetVertexShaderConstantF, y el HLSL de D3D9 empaqueta las matrices en
 * registros COLUMN-major por omision.  Esa transposicion cancela el `mul`,
 * asi que `mul(v, m)` calcula `m * v` con la matriz TAL COMO SE ESCRIBIO --
 * que es el convenio de vector columna de GX (`clip = P * MV * obj`).  Es la
 * misma nota que hubo que dejar en el port de Rice. */
static const char *s_vsSource =
    "float4x4 uProj : register(c0);\n"
    "float4x4 uMV   : register(c4);\n"
    "float4   uHalfPx : register(c8);\n"   /* (1/vpW, 1/vpH, 0, 0) */
    "float4   uVpFix  : register(c9);\n"   /* (sx, sy, ox, oy): GX_SetViewport */
    "float4   uZRange : register(c10);\n"  /* (MinZ, MaxZ-MinZ, 0, 0): GX_SetViewport */
    /* >>> HUBO AQUI un `uFogMO` para calcular el coeficiente de niebla en la
     * GPU desde `clip.z/clip.w`.  NO VALE, y la razon esta en el lote
     * (OpenGL.cpp:1477): en el camino `combW` el plugin escribe
     * `dst[i].z = -v->w` y TIRA la z real -- la profundidad la reconstruye la
     * matriz--, asi que `clip.z/clip.w` no puede recuperar el `z/w` del
     * plugin.  Sintoma medido: 007 (camino ya dividido) salia bien y OoT
     * (camino `combW`) se quedaba sin niebla.  El coeficiente tiene que
     * calcularse donde `v->z` y `v->w` siguen existiendo: en la CPU. */
    /* ★ `pos` se declara `float4` y el vertex buffer solo trae XYZ: la
     * declaracion es `D3DDECLTYPE_FLOAT3` y D3D expande a (x,y,z,1).  Antes el
     * vertice llevaba una `w` que valia SIEMPRE 1.0f, o sea 4 bytes por
     * vertice de nada.
     *
     * ★ Y NO HAY `uv2` DE ENTRADA: valia siempre (0,0) en los tres sitios del
     * plugin que declaran un tercer juego de coordenadas.  El pixel shader
     * SIGUE recibiendolo (se escribe aqui como literal), asi que el generador
     * de HLSL no se entera.  Ver la nota larga de xenon/gxd3d_lote.h. */
    "struct VS_IN {\n"
    "  float4 pos : POSITION;\n"
    "  float2 uv0 : TEXCOORD0;\n"
    "  float2 uv1 : TEXCOORD1;\n"
    "  float4 col : COLOR0;\n"
    "  float  fog : TEXCOORD2;\n"
    "};\n"
    "struct VS_OUT {\n"
    "  float4 pos  : POSITION;\n"
    "  float2 uv0  : TEXCOORD0;\n"
    "  float2 uv1  : TEXCOORD1;\n"
    "  float2 uv2  : TEXCOORD2;\n"
    "  float4 col  : COLOR0;\n"
    "  float4 cpos : TEXCOORD3;\n"   /* posicion de recorte, para la z        */
    "  float  fogc : TEXCOORD4;\n"   /* coeficiente de niebla, 0..255         */
    "};\n"
    "VS_OUT main(VS_IN In) {\n"
    "  VS_OUT o;\n"
    "  float4 eye  = mul(In.pos, uMV);\n"
    "  float4 clip = mul(eye,    uProj);\n"
    /* ★ GX normaliza la profundidad a [-1,0] y D3D la quiere en [0,1].
     * `z += w` es exactamente ese desplazamiento hecho antes de la division
     * por w.  Sin esto TODA la escena cae fuera del rango de profundidad y
     * no se dibuja nada -- el mismo tipo de fallo que el de la fila z de las
     * matrices de Rice, que metio el cursor de SM64 detras de la cara.
     * A 0 se queda el convenio de GX, para poder bisecar. */
    "#if GXD3D_VS_ZFIX\n"
    "  clip.z += clip.w;\n"
    "#endif\n"
    /* [Kirby 64] Modo de marcado `sinz` (uHalfPx.z = 1): la z sujeta a
     * [0, w] para que ni el plano cercano ni el lejano recorten nada.  Fuera
     * de ese modo uHalfPx.z vale 0 y no cambia nada.  Ver GLN64_MARCA_SINZ. */
    "  if (uHalfPx.z > 0.5) clip.z = clamp(clip.z, 0.0, max(clip.w, 0.0));\n"
    /* ★★ LA REGLA DEL MEDIO PIXEL DE D3D9, QUE GX NO TIENE.
     *
     * D3D9 muestrea el centro del texel en (0.5, 0.5) mientras que la
     * posicion de vertice (0,0) es la ESQUINA del pixel.  Al mapear una
     * textura 1:1 con la pantalla eso desplaza medio pixel.  GX no lo tiene,
     * asi que el codigo copiado no lo compensa.
     *
     * Normalmente medio pixel es cosmetico, pero aqui NO, porque el
     * guardar/restaurar del framebuffer es un BUCLE DE REALIMENTACION:
     * restaurar -> dibujar -> `GX_CopyTex` -> restaurar...  Medio pixel de
     * error con filtrado bilineal en cada vuelta se acumula, y eso explica
     * los tres sintomas a la vez:
     *
     *   - el fondo de Paper Mario moviendose un pixel en Y (y a veces en X)
     *   - el EMBORRONAMIENTO progresivo del logo (el "fundido")
     *   - los dientes de sierra en las ~40 tiras del fondo de pausa de OoT
     *
     * La correccion estandar: correr la posicion medio pixel.  En NDC medio
     * pixel son 1/ancho y 1/alto del viewport, y la Y va invertida.
     * `uHalfPx` = (1/vpW, 1/vpH, 0, 0), lo pone GX_SetViewport. */
    /* ★★ [log 60] EL VIEWPORT QUE NO CABE EN EL RENDER TARGET.  En la N64 un
     * viewport mayor que la pantalla o con origen negativo es legal: el
     * recorte limita lo que se pinta.  D3D9 no lo puede expresar (X/Y son
     * DWORD y el rect tiene que caber), asi que GX_SetViewport le da a D3D el
     * trozo que SI cabe y aqui se corrige la diferencia en espacio de
     * recorte: ndc' = ndc * s + o.  Fuera de esos casos uVpFix = (1,1,0,0) y
     * no cambia nada.  Va ANTES del medio pixel, que es del viewport real. */
    "  clip.xy = clip.xy * uVpFix.xy + uVpFix.zw * clip.w;\n"
    "#if GXD3D_VS_HALFPIXEL\n"
    "  clip.x -= clip.w * uHalfPx.x;\n"
    "  clip.y += clip.w * uHalfPx.y;\n"
    "#endif\n"
    "  o.pos  = clip;\n"
    "  o.cpos = clip;\n"
    /* ★ [Conker, la N de la intro; MM, la mascara] LA PROFUNDIDAD SUJETA DE
     * GLideN64 (`ClampMode::NoNearPlaneClipping` = GL_DEPTH_CLAMP) PARA LOS
     * MICROCODIGOS NoN, que en la N64 no recortan contra el plano cercano.
     * D3D9 no tiene ese interruptor, asi que se hace en dos mitades:
     *   - aqui, la z que ve el RECORTADOR se sube a >= 0: D3D ya no corta por
     *     el plano CERCANO.  El LEJANO se sigue recortando (z <= w), que es lo
     *     que hace el `gl_ClipDistance[0] = w - z` de GLideN64
     *     (glsl_CombinerProgramBuilderCommon.cpp:162).  [Una primera version
     *     sujetaba a [0, w] y quitaba los dos planos: se dibujaba lo que hay
     *     mas alla del lejano.]  El plugin NO lo pide para triangulos con un
     *     vertice detras del ojo (w <= 0): con ellos salian huecos (A/B,
     *     logs 38-39).  Ver OGL_AddTriangle;
     *   - la z real viaja en `cpos`, ya mapeada al rango del viewport, y el
     *     pixel shader escribe `saturate(cpos.z / cpos.w)`: la misma
     *     profundidad que D3D habria interpolado, sujeta por pixel.
     * Bandera en uHalfPx.w (GXD3D_SetDepthClamp). */
    "  if (uHalfPx.w > 0.5) {\n"
    "    o.cpos.z = uZRange.x * clip.w + clip.z * uZRange.y;\n"
    "    o.pos.z  = max(clip.z, 0.0);\n"
    "  }\n"
    "  o.uv0  = In.uv0;\n"
    "  o.uv1  = In.uv1;\n"
    "  o.uv2  = float2(0,0);\n"
    "  o.fogc = In.fog;\n"
    "  o.col  = In.col;\n"
    "  return o;\n"
    "}\n";
/* ★★★ [etapa 3] EL VERTEX BUFFER YA NO ES DEL PRODUCTOR.
 *
 * Hasta la etapa 2 el shim escribia los vertices DIRECTAMENTE en un vertex
 * buffer bloqueado (`s_vbData` apuntaba dentro de el) y lanzaba
 * `DrawPrimitive` sobre ese buffer.  En la etapa 4 el shim corre en el hilo
 * del EMULADOR, que no es el dueno del device: no puede bloquear, rotar ni
 * soltar un vertex buffer.
 *
 * Ahora son dos piezas:
 *
 *   PRODUCTOR  `s_vbData` apunta a `s_staging`, memoria de CPU corriente.
 *              TODO el codigo que escribe vertices -- `GX_Position*`, el
 *              reparto de quads, `gxd3d_lote_abrir` de Rice y glN64 -- sigue
 *              EXACTAMENTE igual: solo cambia a donde apunta el puntero.  Al
 *              vaciar el lote se emite `DibujaVerts` y los vertices se
 *              COPIAN al pool de la cola.
 *   CONSUMIDOR `s_vb[]`, `s_vbIndex` y el bloqueo (`s_cvData`/`s_cvUsed`).
 *              `gxd3d_consumidor_dibuja` copia los vertices a SU buffer y
 *              lanza el mismo `DrawPrimitive` que antes.  La rotacion de 3
 *              buffers por pasada, medida (5,41 ms/frame con uno, 0,00 con
 *              tres), se conserva tal cual en este lado.
 *
 * Es el camino de vertices de cliente de GLideN64 (`glDrawArrays` sin VBO,
 * ThreadedOpenGl/opengl_Wrapper.cpp:361-374).  Coste: una copia de vertices
 * mas, que sale en `[D3DQ] bytes`. */
static int   s_vbIndex   = GXD3D_VB_COUNT - 1;   /* CONSUMIDOR */
static GXD3DVertex *s_vbData = NULL;   /* PRODUCTOR: base del bufer de preparacion */
static int   s_vbUsed    = 0;          /* PRODUCTOR: vertices ya escritos           */
static GXD3DVertex  s_staging[GXD3D_MAX_VERTS];  /* PRODUCTOR */

static GXD3DVertex *s_cvData    = NULL;   /* CONSUMIDOR: base del VB bloqueado */
static int          s_cvUsed    = 0;      /* CONSUMIDOR: vertices ya escritos  */
static int          s_cvStream  = 0;      /* CONSUMIDOR: stream source ligado  */

/* ★ Los desplazamientos tienen que casar EXACTAMENTE con `struct GXD3DVertex`
 * (xenon/gxd3d_lote.h): x,y,z | s0,t0 | s1,t1 | color | fog.
 *
 * ★★ EL TAMANO IMPORTA, Y HAY UNA MEDIDA DETRAS.  Al anyadir el quinto
 * elemento `vert` subio de 4.018 a 11.294 us/frame, pero el culpable NO era
 * solo el tamano: en el mismo cambio entraron una division en coma flotante y
 * cuatro accesos `volatile` de una sonda, POR VERTICE, en el bucle mas
 * caliente del plugin (medio millon de vertices por segundo).  Con la sonda
 * fuera y la division limitada a la geometria que de verdad lleva niebla
 * (`on=1186` contra `off=2603` en OoT), el coste vuelve a ser el de antes.
 * Si algun dia `vert` se dispara otra vez, mirar primero lo que se hace POR
 * VERTICE y solo despues el tamano. */
static const D3DVERTEXELEMENT9 s_vertexDecl[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT2,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 20, D3DDECLTYPE_FLOAT2,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
    { 0, 28, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    { 0, 32, D3DDECLTYPE_FLOAT1,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
    D3DDECL_END()
};

/* ------------------------------------------------------------------ */
/* Estado del lote en curso (GX_Begin .. GX_End)                       */
/* ------------------------------------------------------------------ */
static int  s_inBegin      = 0;
static u8   s_primType     = GX_TRIANGLES;
static int  s_batchStart   = 0;   /* indice del primer vertice del lote   */
static int  s_batchCount   = 0;   /* vertices escritos en este lote       */
static int  s_curAttr      = 0;   /* que atributo toca escribir           */

/* Vertice en construccion.  GX escribe los atributos en orden fijo
 * (posicion, color, tex0, tex1, tex2) y cierra el vertice cuando ha visto
 * tantos atributos como declara el descriptor. */
static GXD3DVertex s_v;
static int s_haveTex[3] = { 0, 0, 0 };
static int s_wantTex    = 0;   /* cuantos juegos de UV trae cada vertice */

/* ★★ UN ATRIBUTO QUE NO ESTA EN EL DESCRIPTOR NO VALE CERO.
 *
 * Aqui se reiniciaba el vertice con `memset` a 0, asi que cuando el
 * descriptor NO declara `GX_VA_CLR0` el color quedaba en 0x00000000 -- negro
 * transparente.  En GX eso es FALSO: si el atributo no viene por vertice, el
 * color lo pone la configuracion del canal, que es blanco opaco.
 *
 * Y hay exactamente un sitio del plugin que omite el color:
 * `FrameBuffer_RestoreBuffer` (FrameBuffer.cpp:640-647), el quad que vuelve a
 * meter en el destino la captura del buffer al que se conmuta.  Con el color
 * a cero ese quad no pinta nada, la restauracion no ocurre, y como en GX (y
 * aqui) el destino de dibujo es UNO SOLO, los frames de los tres buffers del
 * juego se acumulan encima.  Eso es el FUNDIDO del logo de Nintendo y, muy
 * probablemente, las tiras del fondo de pausa.
 *
 * ★ Y contesta por que la referencia no necesita varios render targets: en
 * Wii tampoco los hay -- se dibuja siempre al EFB-- y el truco es justo este
 * par guardar/restaurar (`GX_CopyTex` + este quad).  El mecanismo estaba
 * copiado y funcionando; lo que fallaba era el valor por defecto del color. */
static void gxd3d_vertex_reset(void)
{
    memset(&s_v, 0, sizeof(s_v));
    s_v.color = 0xFFFFFFFFu;   /* blanco opaco: lo pisa GX_Color4u8 si viene */
}

/* Quads partidos en dos triangulos.  La explicacion larga esta encima de
 * `gxd3d_vertex_maybe_flush`; aqui arriba solo porque `GX_Begin` necesita el
 * define para reservar sitio y el acumulador para reiniciarlo.
 *
 * ★ A 0 PARA PROBAR LA DIAGONAL DEL QUAD A PANTALLA COMPLETA.
 *
 * Con la emulacion de framebuffer encendida, la pantalla entera pasa a ser un
 * quad texturizado, y ahi se nota un DESFASE entre sus dos mitades por la
 * diagonal.  No puede ser el quad del frontend: Salvia dibuja el juego como
 * UN triangulo a pantalla completa, que no tiene diagonal.
 *
 * El reparto de aqui -- (0,1,2) + (0,2,3), compartiendo los vertices 0 y 2
 * byte a byte-- no deberia introducir ningun desfase, y cuando se probo
 * contra D3DPT_QUADLIST el resultado fue IDENTICO.  Pero aquella prueba fue
 * con escenas 3D, no con un quad a pantalla completa texturizado, que es
 * donde un reparto distinto de la diagonal SI se ve.
 *
 * A 0 se usa D3DPT_QUADLIST y lo reparte el hardware.  Si la diagonal
 * desaparece, era esto; si sigue, el desfase viene de las UV o del filtrado y
 * el reparto queda descartado.
 *
 * ★★ Y VUELVE A 1, PORQUE EL REPARTO DEL HARDWARE NO ES EL DE GX.
 *
 * Con la emulacion de framebuffer encendida y el color de vertice ya
 * arreglado, el quad a pantalla completa de `FrameBuffer_RestoreBuffer` sale
 * con LA MITAD IZQUIERDA BIEN Y LA DERECHA CON EL FUNDIDO VIEJO.  Los cuatro
 * vertices son identicos y comparten la textura, asi que si una mitad sale
 * bien y la otra no, las dos mitades NO estan recibiendo los mismos
 * vertices: el reparto del hardware usa otra diagonal que la de GX.
 *
 * Y estaba avisado aqui mismo: el XDK declara `D3DPT_QUADLIST = 13` SIN
 * DOCUMENTAR el orden de vertices.  GX reparte en abanico -- (0,1,2) +
 * (0,2,3) --; QUADLIST reparte como tira, o sea (0,1,2) + (1,3,2).  El
 * primer triangulo coincide en los dos y el segundo no: exactamente el
 * sintoma.
 *
 * Asi que se parte a mano, que es el unico reparto que sabemos correcto.  La
 * prueba de que "daba identico" se hizo con escenas 3D, donde un quad mal
 * repartido casi no se ve; en un quad a pantalla completa texturizado se ve
 * entero. */
#define GXD3D_QUADS_AS_TRIS 1
#if GXD3D_QUADS_AS_TRIS
static GXD3DVertex s_quad[4];
static int         s_quadFill = 0;
#endif
static int s_wantColor  = 0;

/* ------------------------------------------------------------------ */
/* Espejo del estado de render                                         */
/* ------------------------------------------------------------------ */
/* ★ Este espejo TIENE QUE CADUCAR una vez por display list.  El frontend
 * pone en el device compartido exactamente estos estados (ALPHABLENDENABLE,
 * SRCBLEND, DESTBLEND, CULLMODE, ZENABLE, SCISSORTESTENABLE) al dibujar su
 * quad y su menu, y el espejo no se entera.  Es el corolario que quedo
 * documentado al hacer el anillo de ordenes de D3D
 * (ver project_xenon64_gfx_thread, fase 2, punto 3). */
struct GXD3DRenderState
{
    u32 cullMode;
    u32 zEnable, zFunc, zWrite;
    u32 blendEnable, srcBlend, dstBlend;
    u32 alphaTestEnable, alphaFunc, alphaRef;
    u32 colorWrite;
    u32 depthBias;      /* patron de bits del float: ver GX_SetCoPlanar */
    u32 scissorEnable;
    RECT scissor;
    D3DVIEWPORT9 viewport;
    int valid;
};
static GXD3DRenderState s_rs;



void gxd3d_state_invalidate(void)
{
    memset(&s_rs, 0, sizeof(s_rs));
    s_rs.valid = 0;

    /* ★ MISMA CLASE QUE EL COLOR DE VERTICE: el cero del memset NO es el
     * valor por defecto.
     *
     * `GX_SetColorUpdate` y `GX_SetAlphaUpdate` son dos booleanos
     * INDEPENDIENTES en GX, los dos activos por defecto, y aqui se combinan
     * en una sola mascara con OR.  Partiendo de 0, un
     * `GX_SetColorUpdate(GX_ENABLE)` suelto deja la mascara en RGB **sin
     * alfa**, o sea que apaga la escritura de alfa sin que nadie lo pida.
     *
     * Hoy `OGL_GXinitDlist` (OpenGL.cpp:2221-2222) llama a los dos al
     * empezar cada display list, asi que no se nota; pero el valor de
     * partida tiene que ser el de GX, no el del memset. */
    s_rs.colorWrite = D3DCOLORWRITEENABLE_RED   | D3DCOLORWRITEENABLE_GREEN
                    | D3DCOLORWRITEENABLE_BLUE  | D3DCOLORWRITEENABLE_ALPHA;
}

/* Traduccion de comparaciones GX -> D3D9.  El enum D3DCMP del XDK esta
 * MEDIDO (NEVER=0 LESS=1 EQUAL=2 LESSEQUAL=3 GREATER=4 NOTEQUAL=5
 * GREATEREQUAL=6 ALWAYS=7) y coincide con el orden de GX, pero se escribe la
 * tabla explicita en vez de confiar en la coincidencia: deducir enums del
 * XDK en vez de medirlos ya costo dos turnos en el port de Rice. */
static const DWORD s_cmpMap[8] = {
    D3DCMP_NEVER, D3DCMP_LESS, D3DCMP_EQUAL, D3DCMP_LESSEQUAL,
    D3DCMP_GREATER, D3DCMP_NOTEQUAL, D3DCMP_GREATEREQUAL, D3DCMP_ALWAYS
};

/* GX_BL_* -> D3DBLEND.  OJO al enum 0-based del XDK.
 *
 * ★★ [Fase 6] DOS TABLAS, Y LA DIFERENCIA ESTA SOLO EN LOS INDICES 2 Y 3.
 *
 * En GX el campo del factor tiene 3 bits y el valor 2 quiere decir "el OTRO
 * color", asi que su significado depende de la RANURA.  Por eso libogc
 * define `GX_BL_DSTCLR` como alias de `GX_BL_SRCCLR`: se escribe DSTCLR en
 * la ranura de origen y SRCCLR en la de destino, y los dos valen 2.
 *
 *     ranura ORIGEN   2 -> color de DESTINO,  3 -> su complemento
 *     ranura DESTINO  2 -> color de ORIGEN,   3 -> su complemento
 *
 * ★ HABIA UNA SOLA TABLA -- la de destino -- usada para las dos ranuras.  No
 * daba ningun fallo porque glN64 nunca pasa 2 ni 3 en la de origen.  Rice si
 * (`Rice_GX/TEVBlender.cpp:41-42`, BLEND_DESTCOLOR y BLEND_INVDESTCOLOR; su
 * tabla `GX_BlendFuncMaps` se indexa igual para las dos, lineas 74-75).  Con
 * una sola tabla saldria `src*src` donde toca `src*dst`: un color mezclado
 * mal, sin error ni aviso de ninguna clase. */
static const DWORD s_blendMapSrc[8] = {
    D3DBLEND_ZERO,          /* GX_BL_ZERO                           */
    D3DBLEND_ONE,           /* GX_BL_ONE                            */
    D3DBLEND_DESTCOLOR,     /* GX_BL_DSTCLR    (== GX_BL_SRCCLR)    */
    D3DBLEND_INVDESTCOLOR,  /* GX_BL_INVDSTCLR (== GX_BL_INVSRCCLR) */
    D3DBLEND_SRCALPHA,      /* GX_BL_SRCALPHA                       */
    D3DBLEND_INVSRCALPHA,   /* GX_BL_INVSRCALPHA                    */
    D3DBLEND_DESTALPHA,     /* GX_BL_DSTALPHA                       */
    D3DBLEND_INVDESTALPHA   /* GX_BL_INVDSTALPHA                    */
};

static const DWORD s_blendMapDst[8] = {
    D3DBLEND_ZERO,          /* GX_BL_ZERO        */
    D3DBLEND_ONE,           /* GX_BL_ONE         */
    D3DBLEND_SRCCOLOR,      /* GX_BL_SRCCLR      */
    D3DBLEND_INVSRCCOLOR,   /* GX_BL_INVSRCCLR   */
    D3DBLEND_SRCALPHA,      /* GX_BL_SRCALPHA    */
    D3DBLEND_INVSRCALPHA,   /* GX_BL_INVSRCALPHA */
    D3DBLEND_DESTALPHA,     /* GX_BL_DSTALPHA    */
    D3DBLEND_INVDESTALPHA   /* GX_BL_INVDSTALPHA */
};

/* ★ Punto CENTRAL de todos los render states, y por eso el sitio correcto
 * para vaciar el lote pendiente: si el estado cambia de verdad, lo ya
 * acumulado tiene que dibujarse ANTES con el estado viejo.  Ponerlo aqui y
 * no en cada `GX_Set*` es lo que hace que no se pueda olvidar ninguno. */
static __inline void rsSet(D3DRENDERSTATETYPE st, DWORD v, u32 *mirror)
{
    if (s_rs.valid && *mirror == v) return;
    gxd3d_flush_pending();
    g_pd3dDevice->SetRenderState(st, v);
    *mirror = v;
}

/* ------------------------------------------------------------------ */
/* Matrices                                                            */
/* ------------------------------------------------------------------ */
/* El vertex shader recibe proyeccion (c0..c3) y modelview (c4..c6) como
 * constantes.  En GX la proyeccion se carga con GX_LoadProjectionMtx, que
 * distingue PERSPECTIVE de ORTHOGRAPHIC porque la matriz de GX omite la
 * fila w; hay que reconstruirla.
 *
 * ★ Y hay que absorber la diferencia de convenio de profundidad: en GL el z
 * normalizado va a [-1,1] y en D3D ya es la profundidad en [0,1].  Ese
 * `/2 + 0.5` va en la matriz.  Ponerlo mal fue lo que metio el cursor de la
 * mano de SM64 detras de la cara cuando se porto Rice
 * (ver project_xenon64_texturas_d3d9). */
/* ★ IDENTIDAD, NO CERO.  En GX la memoria de matrices (PNMTX0 y la
 * proyeccion) arranca en IDENTIDAD, asi que un dibujo que ocurra antes de
 * cargar la suya sale sin transformar.  Como `static` sin inicializar esto
 * valdria CERO, y una matriz cero colapsa toda la geometria al origen: la
 * escena entera en un punto, que es el sintoma que ya costo una sesion con
 * `OGL.GXwidth` sin inicializar.
 *
 * Hoy todos los caminos vivos de glN64 cargan las dos antes de dibujar, asi
 * que esto no arregla nada visible -- es cerrar la clase de fallo, no un
 * caso.  Cuesta cero. */
static float s_proj[4][4] = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
static float s_modelview[4][4] = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
static int   s_mtxDirty = 1;

/* ★★ [Fase 6] EL BANCO DE MATRICES DE POSICION, Y POR QUE NO LO HABIA.
 *
 * GX tiene diez ranuras (GX_PNMTX0..9) y el vertice elige la suya con el
 * atributo GX_VA_PTNMTXIDX.  glN64_GX usa SOLO la 0 -- carga y dibuja, carga
 * y dibuja -- asi que el shim guardaba una unica `s_modelview` y
 * `GX_LoadPosMtxImm` IGNORABA el argumento `pnidx`.  Estaba escrito en su
 * comentario: "solo se usa GX_PNMTX0 en el camino vivo".
 *
 * Rice_GX usa DOS, y con matrices DISTINTAS:
 *
 *     OGLGraphicsContext.cpp:389  GX_LoadPosMtxImm(GXmodelView2D,  GX_PNMTX2)
 *                           :396  GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX2)
 *     OGLRender.cpp:192           GX_LoadPosMtxImm(GXmodelViewIdent, GX_PNMTX0)
 *                   :714,796,844,925,1120  ... GX_PNMTX0
 *
 * o sea la 2D/OSD por un lado y la del juego por otro.  Colapsandolas en una,
 * gana la ULTIMA que se cargue y toda la geometria sale transformada con la
 * matriz del otro camino.
 *
 * ★ SON CUATRO RANURAS Y NO DIEZ a proposito: Rice usa la 0 y la 2, glN64 la
 * 0.  Si algun ucode pidiera la 4, `slot >= GXD3D_POSMTX_N` lo lleva a la 0,
 * que es lo que pasaba ANTES con todas.  Ampliarlo es cambiar un numero.
 *
 * ★ `s_modelview` sigue significando "la que esta puesta en la GPU", que es
 * lo que mira `s_mtxDirty` y lo que sube `SetVertexShaderConstantF`.  El
 * banco es de donde se copia.  Asi la comparacion "no ha cambiado, no
 * ensucies" -- que es la que sostiene la fusion de lotes -- sigue valiendo
 * igual. */
#define GXD3D_POSMTX_N 4
static float s_posMtxBank[GXD3D_POSMTX_N][4][4];
static int   s_posMtxSel = 0;   /* la ranura que selecciona el descriptor */

/* ★ Medio pixel en NDC: el viewport entero mide 2 unidades, asi que un pixel
 * son 2/ancho y medio pixel 1/ancho.  Lo consume el vertex shader en c8 bajo
 * GXD3D_VS_HALFPIXEL (la nota larga esta junto a `s_vsSource`).  Lo recalcula
 * GX_SetViewport, que es el unico sitio que conoce las dimensiones reales. */
static float s_halfPx[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
static int   s_halfPxDirty = 1;
/* [log 60] c9 del vertex shader: (sx, sy, ox, oy) que lleva el viewport
 * PEDIDO al que se le da a D3D (ver GX_SetViewport y `uVpFix`).  Viaja con
 * la bandera del medio pixel: los dos dependen del viewport y los dos los
 * pisa el frontend (`s_halfPxDirty = 1` en cada pasada). */
static float s_vpFix[4]    = { 1.0f, 1.0f, 0.0f, 0.0f };
/* c10 del vertex shader: (MinZ, MaxZ - MinZ, 0, 0) del viewport que se le da a
 * D3D.  Lo usa la profundidad sujeta de GXD3D_SetDepthClamp para escribir en
 * el pixel shader EXACTAMENTE la profundidad que D3D habria interpolado.
 * Viaja con c8 y c9 (`s_halfPxDirty`). */
static float s_zRange[4]   = { 0.0f, 1.0f, 0.0f, 0.0f };
/* Cuantos viewports no cabian en el render target (por segundo, en
 * [GXD3D2]) y los DISTINTOS que se pidieron asi: el cielo y el mar de
 * Cruis'n salen en sitios diferentes, o sea que cada uno trae el suyo. */
#define GXD3D_VPFUERA_MAX 6
static unsigned int s_vpFuera = 0;
static unsigned int s_vpFueraDist = 0;          /* distintos; pasado de GXD3D_VPFUERA_MAX
                                                 * cuenta DE MAS (no se recuerdan) */
static float s_vpFueraPedido[GXD3D_VPFUERA_MAX][4];


static void gxd3d_upload_matrices(void)
{
    /* Bandera propia: el medio pixel cambia con el VIEWPORT, no con las
     * matrices, y las dos cosas se mueven por su cuenta. */
    if (s_halfPxDirty)
    {
        g_pd3dDevice->SetVertexShaderConstantF(8, s_halfPx, 1);
        g_pd3dDevice->SetVertexShaderConstantF(9, s_vpFix, 1);   /* [log 60] */
        g_pd3dDevice->SetVertexShaderConstantF(10, s_zRange, 1); /* GXD3D_SetDepthClamp */
        s_halfPxDirty = 0;
    }
    if (!s_mtxDirty) return;
    g_pd3dDevice->SetVertexShaderConstantF(0, &s_proj[0][0], 4);
    g_pd3dDevice->SetVertexShaderConstantF(4, &s_modelview[0][0], 4);
    s_mtxDirty = 0;
}

void GX_LoadProjectionMtx(Mtx44 mt, u8 type)
{
    /* ★ Se construye en un temporal y se compara: ver la nota de
     * `GX_LoadPosMtxImm`.  El plugin la recarga antes de cada lote. */
    float prev[4][4];
    int i, j;
    memcpy(prev, s_proj, sizeof(prev));

    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            s_proj[i][j] = mt[i][j];

    if (type == GX_PERSPECTIVE)
    {
        /* GX guarda la proyeccion perspectiva con la fila 3 implicita
         * (0,0,-1,0): w_clip = -z_eye.  D3D la necesita explicita. */
        s_proj[3][0] = 0.0f;
        s_proj[3][1] = 0.0f;
        s_proj[3][2] = -1.0f;
        s_proj[3][3] = 0.0f;
    }
    else
    {
        /* Ortografica: w_clip = 1. */
        s_proj[3][0] = 0.0f;
        s_proj[3][1] = 0.0f;
        s_proj[3][2] = 0.0f;
        s_proj[3][3] = 1.0f;
    }

    /* GX normaliza z a [-1,0] (near=-1... far=0 con su convenio) y D3D pide
     * [0,1].  glN64_GX ya compensa parte con GXprojZScale/GXprojZOffset
     * (OpenGL.h:38-39, 0.5 y -0.5), asi que aqui NO se vuelve a escalar:
     * duplicarlo aplastaria toda la profundidad en una banda estrecha, que
     * es exactamente el defecto que tuvo Rice con su NEAR_PLANE/FAR_PLANE
     * inventados. */
    if (memcmp(prev, s_proj, sizeof(prev)) == 0) return;
    gxd3d_flush_pending();
    s_mtxDirty = 1;
}

/* Pone en la GPU la ranura seleccionada.  Mantiene la regla de oro del shim:
 * comparar antes de ensuciar.  Un cambio de matriz obliga a vaciar el lote
 * pendiente, y marcarlo sucio a ciegas rompe la fusion de lotes (ver la nota
 * de GX_LoadPosMtxImm). */
static void gxd3d_posmtx_bind(void)
{
    if (memcmp(s_posMtxBank[s_posMtxSel], s_modelview, sizeof(s_modelview)) == 0)
        return;
    memcpy(s_modelview, s_posMtxBank[s_posMtxSel], sizeof(s_modelview));
    gxd3d_flush_pending();
    s_mtxDirty = 1;
}

void GX_LoadPosMtxImm(Mtx mt, u32 pnidx)
{
    /* ★ COMPARAR ANTES DE ENSUCIAR.  `OGL_DrawTriangles` recarga las dos
     * matrices ANTES DE CADA LOTE (OpenGL.cpp:1332-1334), casi siempre con
     * el mismo valor.  Marcar sucio a ciegas no solo sube 8 float4 al
     * command buffer por draw: ademas ROMPE LA FUSION DE LOTES, porque un
     * cambio de matriz obliga a vaciar el lote pendiente. */
    unsigned slot = (unsigned)pnidx / 3;   /* GX_PNMTXn = n*3 */
    int i, j;

    if (slot >= GXD3D_POSMTX_N) slot = 0;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            s_posMtxBank[slot][i][j] = mt[i][j];
    s_posMtxBank[slot][3][0] = 0.0f; s_posMtxBank[slot][3][1] = 0.0f;
    s_posMtxBank[slot][3][2] = 0.0f; s_posMtxBank[slot][3][3] = 1.0f;

    /* Cargar una ranura que no esta seleccionada NO toca la GPU ni rompe el
     * lote: es justo lo que hace Rice al preparar la matriz 2D mientras
     * dibuja con la del juego. */
    if ((int)slot == s_posMtxSel)
        gxd3d_posmtx_bind();
}

void GX_LoadTexMtxImm(Mtx mt, u32 texidx, u8 type)
{
    /* glN64_GX solo carga GX_IDENTITY en la matriz de textura por el camino
     * vivo (OpenGL.cpp:577 usa GX_IDENTITY en GX_SetTexCoordGen), asi que
     * las UV llegan ya listas y no hay nada que transformar. */
    (void)mt; (void)texidx; (void)type;
}

/* ------------------------------------------------------------------ */
/* Estado del rasterizador                                             */
/* ------------------------------------------------------------------ */
void GX_SetCullMode(u8 mode)
{
    DWORD d;
    switch (mode)
    {
        case GX_CULL_FRONT: d = D3DCULL_CW;  break;
        case GX_CULL_BACK:  d = D3DCULL_CCW; break;
        default:            d = D3DCULL_NONE; break;
    }
    /* [Kirby 64] Modo de marcado `sinz`: sin culling.  Ver GLN64_MARCA_SINZ. */
    if (gln64_marcaluz == GLN64_MARCA_SINZ) d = D3DCULL_NONE;
    rsSet(D3DRS_CULLMODE, d, &s_rs.cullMode);
}

void GX_SetZMode(u8 enable, u8 func, u8 update_enable)
{
#if GXD3D_DIAG_SIN_ZTEST
    /* La escritura de Z se deja como esta: lo que se quiere descartar es que
     * las tiras se pierdan por el TEST, no cambiar lo que se escribe. */
    rsSet(D3DRS_ZENABLE,      D3DZB_FALSE,                       &s_rs.zEnable);
#else
    /* [Kirby 64] Modo de marcado `sinz`: sin test de Z (la escritura se deja
     * como pide el juego).  Ver GLN64_MARCA_SINZ. */
    if (gln64_marcaluz == GLN64_MARCA_SINZ) enable = 0;
    rsSet(D3DRS_ZENABLE,      enable ? D3DZB_TRUE : D3DZB_FALSE, &s_rs.zEnable);
#endif
    rsSet(D3DRS_ZFUNC,        s_cmpMap[func & 7],                &s_rs.zFunc);
    rsSet(D3DRS_ZWRITEENABLE, update_enable ? TRUE : FALSE,      &s_rs.zWrite);
}

void GX_SetZCompLoc(u8 before_tex)
{
    /* En GX esto elige si el test de Z va ANTES o DESPUES del texturizado.
     * "Despues" hace falta cuando el shader escribe la profundidad (z
     * texture) o cuando el alpha test descarta pixeles: si el Z se resolviera
     * antes, se escribiria profundidad de pixeles que luego se descartan.
     *
     * En D3D9 no hay interruptor equivalente; lo que hay es early-Z, que el
     * hardware apaga solo en cuanto el pixel shader escribe DEPTH o hace
     * clip().  Nuestro generador de HLSL ya emite `clip()` para el alpha
     * test y `DEPTH` para la z texture, asi que el efecto se consigue por
     * construccion.  Se anota el valor por si el generador lo necesita. */
    (void)before_tex;
}

void GX_SetBlendMode(u8 type, u8 src_fact, u8 dst_fact, u8 op)
{
    (void)op;   /* GX_LO_* solo aplica al modo LOGIC, que el plugin no usa */

    /* El termino de niebla de GXD3D_SetBlenderNiebla es parte del blender:
     * cualquier GX_SetBlendMode lo apaga, y quien lo quiera lo vuelve a pedir
     * detras (OGL_UpdateStates).  Asi los dibujos inmediatos de VI.cpp y
     * FrameBuffer.cpp, que no pasan por OGL_UpdateStates, no lo heredan. */
    if (g_gxTev.blFog)
        GXD3D_SetBlenderNiebla(0, g_gxTev.blFogColor);

    if (type == GX_BM_BLEND)
    {
        rsSet(D3DRS_ALPHABLENDENABLE, TRUE, &s_rs.blendEnable);
        rsSet(D3DRS_SRCBLEND,  s_blendMapSrc[src_fact & 7], &s_rs.srcBlend);
        rsSet(D3DRS_DESTBLEND, s_blendMapDst[dst_fact & 7], &s_rs.dstBlend);
    }
    else
    {
        rsSet(D3DRS_ALPHABLENDENABLE, FALSE, &s_rs.blendEnable);
    }
}

void GX_SetAlphaCompare(u8 comp0, u8 ref0, u8 aop, u8 comp1, u8 ref1)
{
    /* GX hace DOS comparaciones combinadas con AND/OR/XOR/XNOR.  D3D9 solo
     * tiene una.  El caso general se resuelve en el pixel shader con
     * `clip()`, que es lo que hace el generador; aqui solo se guarda el
     * estado y se apaga el alpha test de funcion fija para no aplicar el
     * descarte dos veces. */
    rsSet(D3DRS_ALPHATESTENABLE, FALSE, &s_rs.alphaTestEnable);

    /* Comparar antes de ensuciar, como en los demas setters del TEV: sin
     * esto cada reescritura con los mismos valores rompe la fusion de lotes
     * y obliga a reconstruir la clave del shader. */
    if (g_gxTev.acComp0 == comp0 && g_gxTev.acRef0 == ref0 &&
        g_gxTev.acOp    == aop   &&
        g_gxTev.acComp1 == comp1 && g_gxTev.acRef1 == ref1)
        return;
    g_gxTev.acComp0 = comp0; g_gxTev.acRef0 = ref0;
    g_gxTev.acOp    = aop;
    g_gxTev.acComp1 = comp1; g_gxTev.acRef1 = ref1;
    gxd3d_tev_dirty();
}

/* ★★ POR `rsSet`, NO POR `SetRenderState` A PELO.  ESTO ERAN LAS BANDAS
 * NEGRAS PARPADEANTES DE PAPER MARIO.
 *
 * Las dos cambiaban D3DRS_COLORWRITEENABLE directamente, saltandose el unico
 * sitio que vacia el lote pendiente.  Consecuencia: la geometria YA
 * ACUMULADA se dibujaba con la mascara NUEVA.
 *
 * Y el caso que lo destapa es el borrado de profundidad.  `OGL_GXclearEFB`
 * (OpenGL.cpp:2232-2235) borra dibujando un quad a pantalla completa, y
 * cuando solo hay que borrar la Z lo unico que impide que ademas pinte color
 * es `GX_SetColorUpdate(GX_DISABLE)`.  Sin el vaciado, ese DISABLE alcanzaba
 * al lote pendiente y lo hacia desaparecer -- en NEGRO, y en el trozo de
 * pantalla que hubiera caido dentro de ese lote, que cambia de un frame a
 * otro.  De ahi las bandas y el parpadeo.
 *
 * Paper Mario lo sufre mas que nadie porque es 2D por capas y borra
 * profundidad constantemente.
 *
 * Misma familia que los otros tres de este fichero: cambiar estado sin
 * vaciar lo que ya estaba acumulado. */
void GX_SetAlphaUpdate(u8 enable)
{
    DWORD mask = s_rs.colorWrite;
    if (enable) mask |=  D3DCOLORWRITEENABLE_ALPHA;
    else        mask &= ~D3DCOLORWRITEENABLE_ALPHA;
    rsSet(D3DRS_COLORWRITEENABLE, mask, &s_rs.colorWrite);
}

void GX_SetColorUpdate(u8 enable)
{
    DWORD rgb = D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
    DWORD mask = s_rs.colorWrite;
    if (enable) mask |=  rgb;
    else        mask &= ~rgb;
    rsSet(D3DRS_COLORWRITEENABLE, mask, &s_rs.colorWrite);
}

void GX_SetDstAlpha(u8 enable, u8 a)
{
    /* Forzar un valor constante en el alfa del destino.  Xenos no lo tiene
     * como render state; se conseguiria escribiendolo desde el shader.  El
     * plugin solo lo llama con GX_DISABLE (tres sitios), asi que no hace
     * falta mas que registrar el caso. */
    (void)a;
    if (enable)
    {
#if XENON64_DIAG
        static int avisado = 0;
        if (!avisado) { avisado = 1;
            DebugOutputString("[GXD3D] GX_SetDstAlpha(ENABLE) sin implementar"); }
#endif
    }
}

void GX_SetScissor(u32 xOrigin, u32 yOrigin, u32 wd, u32 ht)
{
    RECT r;

    /* -- [SCIS-GX] sonda TEMPORAL: los rects de recorte que pide el plugin --
     *
     * El bisect ya dijo que las tiras de Paper Mario las quita EL RECORTE, y
     * `degen=0` dice que el rect nunca es degenerado: son rects VALIDOS pero
     * EQUIVOCADOS.  Asi que hay que ver cuales son.
     *
     * Se imprimen los DISTINTOS, no los N primeros: un tope por cuenta se
     * comeria justo el que interesa si aparece tarde.  En una escena hay un
     * puñado de rects distintos, no cientos.
     *
     * Se imprime lo que PIDE el plugin y lo que queda TRAS EL CLAMP, porque
     * si el clamp esta recortando de mas, la diferencia entre las dos lineas
     * es la respuesta. */
#if GXD3D_DIAG_SCIS
    {
        static unsigned long long vistos[24];
        static int nVistos = 0;
        unsigned long long clave = ((unsigned long long)(xOrigin & 0xFFFF) << 48)
                                 | ((unsigned long long)(yOrigin & 0xFFFF) << 32)
                                 | ((unsigned long long)(wd      & 0xFFFF) << 16)
                                 |  (unsigned long long)(ht      & 0xFFFF);
        int k, visto = 0;
        for (k = 0; k < nVistos; k++) if (vistos[k] == clave) { visto = 1; break; }
        if (!visto && nVistos < 24)
        {
            char ob[160];
            vistos[nVistos++] = clave;
            sprintf(ob, "[SCIS-GX] pide x=%u y=%u w=%u h=%u -> (%u,%u)-(%u,%u) | rt=%ux%u",
                    xOrigin, yOrigin, wd, ht,
                    xOrigin, yOrigin, xOrigin + wd, yOrigin + ht,
                    (unsigned)xenon64_video_width(), (unsigned)xenon64_video_height());
            DebugOutputString(ob);
        }
    }
#endif

    r.left   = (LONG)xOrigin;
    r.top    = (LONG)yOrigin;
    r.right  = (LONG)(xOrigin + wd);
    r.bottom = (LONG)(yOrigin + ht);

    /* ★ CLAMP obligatorio, que upstream no necesita: D3D es mas estricto
     * que GX_SetScissor y si el rect se sale del render target o queda
     * left>right, SetScissorRect FALLA y no cambia nada -- o sea que se
     * vuelve al comportamiento sin recorte EN SILENCIO.  Mismo arreglo que
     * hubo que hacer en Rice (SCISSOR_TEST). */
    if (r.left   < 0) r.left   = 0;
    if (r.top    < 0) r.top    = 0;
    if (r.right  > (LONG)xenon64_video_width())  r.right  = (LONG)xenon64_video_width();
    if (r.bottom > (LONG)xenon64_video_height()) r.bottom = (LONG)xenon64_video_height();
#if XBOX64_TRAZAS
    if (r.right  <= r.left) { r.right  = r.left + 1; gxd3d_dbg_degen++; }
    if (r.bottom <= r.top)  { r.bottom = r.top  + 1; gxd3d_dbg_degen++; }
#else
    if (r.right  <= r.left) r.right  = r.left + 1;
    if (r.bottom <= r.top)  r.bottom = r.top  + 1;
#endif

    /* ★ EL RECT DEGENERADO, CON NOMBRE Y APELLIDOS.
     *
     * `degen` solo dice CUANTOS, y un contador a 18 no se puede perseguir.
     * Un scissor vacio lo convierte el clamp en 1x1, o sea que todo lo que se
     * dibuje con el activo queda INVISIBLE -- y callarlo es exactamente lo
     * que costo una sesion la vez de `OGL.GXwidth`
     * ([[reference_gln64_gx_pantalla_negra_gxwidth]]).
     *
     * Se imprimen los DISTINTOS y sin tope de "los primeros N": el filtro ya
     * es el defecto mismo ([[feedback_sondas_filtrar_por_defecto]]), y por
     * definicion son pocos -- si fueran muchos, el contador no se quedaria
     * clavado. */
#if GLN64_CAZA_MARIO64
    {
        static unsigned long long s_degVistos[16];
        static int                s_degNum = 0;
        if (r.right == r.left + 1 || r.bottom == r.top + 1)
        {
            const unsigned long long clave =
                  ((unsigned long long)(unsigned short)(short)xOrigin << 48)
                | ((unsigned long long)(unsigned short)(short)yOrigin << 32)
                | ((unsigned long long)(unsigned short)(short)wd      << 16)
                |  (unsigned long long)(unsigned short)(short)ht;
            int k, visto = 0;
            for (k = 0; k < s_degNum; k++)
                if (s_degVistos[k] == clave) { visto = 1; break; }
            if (!visto && s_degNum < 16)
            {
                char ob[160];
                s_degVistos[s_degNum++] = clave;
                sprintf(ob, "[DEGEN] scissor vacio: pedido x=%d y=%d w=%d h=%d"
                            " -> queda %dx%d en (%d,%d)",
                        (int)xOrigin, (int)yOrigin, (int)wd, (int)ht,
                        (int)(r.right - r.left), (int)(r.bottom - r.top),
                        (int)r.left, (int)r.top);
                DebugOutputString(ob);
            }
        }
    }
#endif /* GLN64_CAZA_MARIO64 */

#if GXD3D_DIAG_SCIS
    /* Y el rect REAL que se le da a D3D, cuando el clamp lo ha cambiado.
     * Si esta linea aparece mucho, el clamp es el sospechoso. */
    if (r.left != (LONG)xOrigin || r.top != (LONG)yOrigin ||
        r.right != (LONG)(xOrigin + wd) || r.bottom != (LONG)(yOrigin + ht))
    {
        static unsigned long long vistosC[24];
        static int nC = 0;
        unsigned long long clave = ((unsigned long long)(r.left  & 0xFFFF) << 48)
                                 | ((unsigned long long)(r.top   & 0xFFFF) << 32)
                                 | ((unsigned long long)(r.right & 0xFFFF) << 16)
                                 |  (unsigned long long)(r.bottom & 0xFFFF);
        int k, visto = 0;
        for (k = 0; k < nC; k++) if (vistosC[k] == clave) { visto = 1; break; }
        if (!visto && nC < 24)
        {
            char ob[160];
            vistosC[nC++] = clave;
            sprintf(ob, "[SCIS-GX] CLAMP (%u,%u)-(%u,%u) -> (%ld,%ld)-(%ld,%ld)",
                    xOrigin, yOrigin, xOrigin + wd, yOrigin + ht,
                    r.left, r.top, r.right, r.bottom);
            DebugOutputString(ob);
        }
    }
#endif

    if (!s_rs.valid || memcmp(&r, &s_rs.scissor, sizeof(RECT)) != 0)
    {
        gxd3d_flush_pending();
        g_pd3dDevice->SetScissorRect(&r);
        s_rs.scissor = r;
    }
    /* ★ En D3D9 el rect NO se respeta sin esto.  El equivalente en GX es el
     * primer parametro de Xe_SetScissor, y perderlo al traducir es lo que
     * dejo a Rice renderizando sin scissor desde siempre. */
#if GXD3D_DIAG_SIN_RECORTE
    rsSet(D3DRS_SCISSORTESTENABLE, FALSE, &s_rs.scissorEnable);
#else
    rsSet(D3DRS_SCISSORTESTENABLE, TRUE, &s_rs.scissorEnable);
#endif
}

void GX_SetViewport(f32 xOrig, f32 yOrig, f32 wd, f32 ht, f32 nearZ, f32 farZ)
{
    D3DVIEWPORT9 vp;
    float fix[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
    const float rtW = (float)xenon64_video_width();
    const float rtH = (float)xenon64_video_height();
    /* ★★ [Fighting Force 64: ni escenario ni personajes, faltan cosas del
     * menu] EL VIEWPORT CON ESCALA NEGATIVA ES UN ESPEJO, NO UNO VACIO.
     * El juego pone vscale < 0 (glN64: x = vtrans - vscale = borde DERECHO y
     * ancho = 2*vscale < 0; log 43: `pedido 640,480` y todo `degen`).  En la
     * N64 eso refleja la imagen; GLideN64 hace el viewport en el shader con
     * su signo y su cull va por el giro EN PANTALLA (4049f7d1, "Invisible
     * textures in Fighting Force 64", quito la dependencia de vscale[0] > 0).
     * Aqui: el mismo rect con ancho positivo (origen x + ancho) y la X de
     * recorte con el signo cambiado en `uVpFix` (fix[0] = -fix[0]); igual en
     * Y.  D3D descarta caras despues del espejo, como la N64: el cull sale
     * solo.  ★RESUELTO (logs 44-45): Fighting Force pide 0,0 640x480 con
     * espejo en X y en Y, y el escenario y los personajes ya salen (sonda
     * [GXVP] retirada). */
    float espX = 1.0f, espY = 1.0f;
    if (wd < 0.0f) { xOrig += wd; wd = -wd; espX = -1.0f; }
    if (ht < 0.0f) { yOrig += ht; ht = -ht; espY = -1.0f; }
#if XBOX64_TRAZAS
    if (wd < 1 || ht < 1) gxd3d_dbg_degen++;
#endif
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;

    /* ★★ [log 60] EL VIEWPORT QUE NO CABE (Cruis'n USA).  El juego pide para
     * su cielo, su mar y su HUD un viewport de 2050x2050 con origen negativo
     * (`vp=0,0 2050x2050` en [GXD3D2], y Rice lo pasa tal cual desde el
     * gSPViewport, OGLRender.cpp:1808).  En la N64 es legal; aqui se
     * recortaba el origen a 0 y D3D metia el resto en el render target: todo
     * lo dibujado con el salia ENCOGIDO a ~1/3 (640/2050) y desplazado.  Pasa
     * con Rice y con glN64: es del shim.
     *
     * Se le da a D3D la interseccion con el render target, y el vertex
     * shader (`uVpFix`, c9) aplica la escala y el desplazamiento que llevan
     * del pedido al recortado.  Con x' = vp.X + (n'+1)/2*vp.Width igual a
     * x = X + (n+1)/2*W sale n' = n*W/vp.Width + (2*(X-vp.X)+W)/vp.Width - 1,
     * y en Y, que va hacia abajo en pantalla y hacia arriba en NDC,
     * n' = n*H/vp.Height + 1 - (2*(Y-vp.Y)+H)/vp.Height.
     *
     * Si cabe, el camino de siempre al pie de la letra y la correccion en
     * identidad: el resto de juegos no cambia en nada. */
    if (xOrig >= 0.0f && yOrig >= 0.0f &&
        xOrig + wd <= rtW && yOrig + ht <= rtH)
    {
        vp.X      = (DWORD)xOrig;
        vp.Y      = (DWORD)yOrig;
        vp.Width  = (DWORD)wd;
        vp.Height = (DWORD)ht;
    }
    else
    {
        float x0 = (xOrig < 0.0f) ? 0.0f : xOrig;
        float y0 = (yOrig < 0.0f) ? 0.0f : yOrig;
        float x1 = (xOrig + wd > rtW) ? rtW : xOrig + wd;
        float y1 = (yOrig + ht > rtH) ? rtH : yOrig + ht;
        if (x0 > rtW - 1.0f) x0 = rtW - 1.0f;   /* fuera del todo: 1 pixel, */
        if (y0 > rtH - 1.0f) y0 = rtH - 1.0f;   /* y la geometria cae fuera  */
        vp.X      = (DWORD)x0;
        vp.Y      = (DWORD)y0;
        vp.Width  = (x1 - (float)vp.X >= 1.0f) ? (DWORD)(x1 - (float)vp.X) : 1;
        vp.Height = (y1 - (float)vp.Y >= 1.0f) ? (DWORD)(y1 - (float)vp.Y) : 1;
        fix[0] = wd / (float)vp.Width;
        fix[1] = ht / (float)vp.Height;
        fix[2] = (2.0f * (xOrig - (float)vp.X) + wd) / (float)vp.Width - 1.0f;
        fix[3] = 1.0f - (2.0f * (yOrig - (float)vp.Y) + ht) / (float)vp.Height;
#if XBOX64_TRAZAS   /* tabla de vpfuera: solo la lee [GXD3D2] */
        s_vpFuera++;
        {
            const float pedido[4] = { xOrig, yOrig, wd, ht };
            unsigned int k, n = (s_vpFueraDist < GXD3D_VPFUERA_MAX) ? s_vpFueraDist : GXD3D_VPFUERA_MAX;
            for (k = 0; k < n; k++)
                if (memcmp(s_vpFueraPedido[k], pedido, sizeof(pedido)) == 0) break;
            if (k == n)
            {
                if (s_vpFueraDist < GXD3D_VPFUERA_MAX)
                    memcpy(s_vpFueraPedido[s_vpFueraDist], pedido, sizeof(pedido));
                s_vpFueraDist++;   /* sin lista llena, sigue contando distintos */
            }
        }
#endif
    }
    /* El espejo de arriba: ndc' = (-ndc) * s + o. */
    fix[0] *= espX;
    fix[1] *= espY;
    if (memcmp(fix, s_vpFix, sizeof(fix)) != 0)
    {
        /* Lo ya encolado se dibujo con la correccion vieja. */
        gxd3d_flush_pending();
        memcpy(s_vpFix, fix, sizeof(fix));
        s_halfPxDirty = 1;
    }
    /* ★ `D3DVIEWPORT9::MinZ`/`MaxZ` estan definidos en [0,1].  GX no: su
     * viewport admite un near NEGATIVO, y de hecho la banda de guarda que
     * Wii64 1b5041a metio en `GXviewportNearZ` pide un -3.  Aqui eso no se
     * puede expresar, asi que se acota Y SE CUENTA: antes se pasaba tal cual
     * y un viewport invalido no da error, simplemente deja de hacer lo que
     * crees.  Si `vpz` sale distinto de 0, alguien esta pidiendo un rango de
     * profundidad que este backend no puede dar -- ver la nota de
     * GLN64_ZGUARD en source/glN64_GX/OpenGL.h. */
#if XBOX64_TRAZAS
    if (nearZ < 0.0f || nearZ > 1.0f || farZ < 0.0f || farZ > 1.0f)
        gxd3d_dbg_vpz++;
#endif
    vp.MinZ   = (nearZ < 0.0f) ? 0.0f : ((nearZ > 1.0f) ? 1.0f : nearZ);
    vp.MaxZ   = (farZ  < 0.0f) ? 0.0f : ((farZ  > 1.0f) ? 1.0f : farZ);
    if (!s_rs.valid || memcmp(&vp, &s_rs.viewport, sizeof(vp)) != 0)
    {
        gxd3d_flush_pending();
        g_pd3dDevice->SetViewport(&vp);
        s_rs.viewport = vp;
    }
    if (s_zRange[0] != vp.MinZ || s_zRange[1] != vp.MaxZ - vp.MinZ)
    {
        gxd3d_flush_pending();   /* lo encolado, con el rango viejo */
        s_zRange[0] = vp.MinZ;
        s_zRange[1] = vp.MaxZ - vp.MinZ;
        s_halfPxDirty = 1;
    }

    /* ★ El medio pixel depende del TAMANO del viewport, asi que se recalcula
     * aqui aunque el rect no haya cambiado de sitio.  Fuera del `if` a
     * proposito: `s_rs.valid` se pone a 0 por otras razones y la constante
     * tiene que sobrevivir a eso. */
    {
        float hx = 1.0f / (float)vp.Width;
        float hy = 1.0f / (float)vp.Height;
        /* [Kirby 64] El modo de marcado `sinz` viaja en uHalfPx.z. */
        float zc = (gln64_marcaluz == GLN64_MARCA_SINZ) ? 1.0f : 0.0f;
        if (s_halfPx[0] != hx || s_halfPx[1] != hy || s_halfPx[2] != zc)
        {
            if (s_halfPx[2] != zc)
                gxd3d_flush_pending();   /* lo encolado, con el valor viejo */
            s_halfPx[0] = hx;
            s_halfPx[1] = hy;
            s_halfPx[2] = zc;
            /* s_halfPx[3] es la bandera de GXD3D_SetDepthClamp: no se toca. */
            s_halfPxDirty = 1;
        }
    }
}

void GX_SetClipMode(u8 mode)
{
    /* ★ `D3DRS_CLIPPING` NO EXISTE EN XBOX 360.  Esta en la lista explicita
     * de render states no soportados, `d3d9types.h:635-638`:
     *
     *     // The following render states are not supported on Xbox 360:
     *     //  D3DRS_SRGBWRITEENABLE / D3DRS_SHADEMODE / D3DRS_CLIPPING
     *
     * Xenos recorta contra el frustum siempre y no hay interruptor.  Eso
     * resulta ser lo correcto aqui: el plugin solo pide `GX_CLIP_ENABLE`
     * (OpenGL.cpp:2196, VI.cpp:410) y la unica llamada con `GX_CLIP_DISABLE`
     * esta COMENTADA (OpenGL.cpp:2197).  O sea que el no-op coincide con el
     * comportamiento pedido, no lo aproxima.
     *
     * Si algun dia hiciera falta desactivarlo de verdad, la via en Xenos es
     * el guard band, no un render state.
     *
     * (Mi fallo al escribirlo: mirar la lista de lo que la consola NO tiene
     * ANTES de elegir una API, no despues del build.  Vale para d3d9.h:
     * 2063-2081 -- metodos-- y para esta -- render states.) */
    (void)mode;
}

void GX_SetCoPlanar(u8 enable)
{
    /* Sesgo de profundidad para poligonos coplanares (decals).  glN64_GX usa
     * GXpolyOffsetFactor = 5.0e-4 (OpenGL.h:40).
     *
     * ★ POR `rsSet`, igual que GX_SetColorUpdate.  Antes escribia
     * D3DRS_DEPTHBIAS a pelo: sin espejo (se reprogramaba en cada llamada) y
     * sin vaciar, asi que el lote pendiente se dibujaba con el sesgo del
     * SIGUIENTE.  En un juego de capas coplanares apiladas -- Paper Mario es
     * el caso extremo-- eso es exactamente lo que decide quien gana el
     * z-fight, y cambia segun donde caiga la frontera del lote. */
    float bias = enable ? -5.0e-4f : 0.0f;
    rsSet(D3DRS_DEPTHBIAS, *(DWORD*)&bias, &s_rs.depthBias);
}

void GX_SetLineWidth(u8 width, u8 fmt)
{
    /* Xenos no tiene ancho de linea programable por render state; las lineas
     * gruesas habria que emitirlas como quads.  El plugin solo lo usa en
     * OGL_DrawLine, que es marginal. */
    (void)width; (void)fmt;
}

void GX_SetFog(u8 type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor col)
{
    /* Comparar antes de ensuciar: ver GX_SetTevOrder y companeros. */
    if (g_gxTev.fogType   == type   && g_gxTev.fogStartZ == startz &&
        g_gxTev.fogEndZ   == endz   && g_gxTev.fogNearZ  == nearz  &&
        g_gxTev.fogFarZ   == farz   &&
        g_gxTev.fogColor.r == col.r && g_gxTev.fogColor.g == col.g &&
        g_gxTev.fogColor.b == col.b && g_gxTev.fogColor.a == col.a)
        return;
    g_gxTev.fogType   = type;
    g_gxTev.fogStartZ = startz;
    g_gxTev.fogEndZ   = endz;
    g_gxTev.fogNearZ  = nearz;
    g_gxTev.fogFarZ   = farz;
    g_gxTev.fogColor  = col;

    gxd3d_tev_dirty();
}

/* ------------------------------------------------------------------ */
/* Descriptores de vertice                                             */
/* ------------------------------------------------------------------ */
void GX_ClearVtxDesc(void)
{
    s_wantTex   = 0;
    s_wantColor = 0;
    s_haveTex[0] = s_haveTex[1] = s_haveTex[2] = 0;
}

void GX_SetVtxDesc(u8 attr, u8 type)
{
    /* ★★ ESTE CASO VA ANTES DEL `GX_NONE`, Y NO ES UN CAPRICHO DE ORDEN.
     *
     * Para GX_VA_PTNMTXIDX el segundo argumento NO es GX_DIRECT/GX_NONE: es
     * el INDICE DE MATRIZ, y `GX_PNMTX0` vale 0, exactamente igual que
     * `GX_NONE` (gccore.h:419 y :432).  Con el descarte delante, la linea
     * `GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0)` -- que Rice hace en cinco
     * sitios de OGLRender.cpp para volver a la matriz del juego -- se
     * tragaria entera y el shim se quedaria en la ranura 2D.
     *
     * Hoy daria igual porque el `default:` tampoco hacia nada, pero en
     * cuanto el caso existe el orden pasa a importar. */
    if (attr == GX_VA_PTNMTXIDX)
    {
        unsigned slot = (unsigned)type / 3;
        if (slot >= GXD3D_POSMTX_N) slot = 0;
        if ((int)slot != s_posMtxSel)
        {
            s_posMtxSel = (int)slot;
            gxd3d_posmtx_bind();
        }
        return;
    }

    if (type == GX_NONE) return;
    switch (attr)
    {
        case GX_VA_CLR0: s_wantColor = 1; break;
        case GX_VA_TEX0: s_haveTex[0] = 1; break;
        case GX_VA_TEX1: s_haveTex[1] = 1; break;
        case GX_VA_TEX2: s_haveTex[2] = 1; break;
        default: break;   /* los *MTXIDX y POS son fijos en nuestro layout */
    }
    s_wantTex = s_haveTex[0] + s_haveTex[1] + s_haveTex[2];
}

void GX_SetVtxAttrFmt(u8 vtxfmt, u32 vtxattr, u32 comptype, u32 compsize, u32 frac)
{
    /* El layout de vertice del backend es FIJO (s_vertexDecl) y coincide con
     * lo que glN64_GX declara siempre: POS XYZ f32, CLR0 RGBA8, TEX0/1/2 ST
     * f32.  Si algun dia pidiera otro formato habria que atenderlo aqui. */
    (void)vtxfmt; (void)vtxattr; (void)comptype; (void)compsize; (void)frac;
}

/* ------------------------------------------------------------------ */
/* Geometria en modo inmediato                                         */
/* ------------------------------------------------------------------ */
/* [etapa 3] PRODUCTOR: volver al principio del bufer de preparacion.
 *
 * Conserva el nombre y el contrato de antes -- "vaciar lo pendiente y
 * empezar un buffer nuevo" -- para que ninguno de sus llamantes cambie.  Lo
 * que hacia con el vertex buffer de verdad (Lock, rotacion, stream source)
 * vive ahora en `gxd3d_consumidor_*`.
 *
 * ★ `gxd3d_flush_pending()` PRIMERO, igual que antes: el lote pendiente
 * apunta dentro del bufer, y hay que emitirlo -- o sea copiarlo al pool --
 * antes de volver a escribir encima. */
static void gxd3d_lock_next_vb(void)
{
    gxd3d_flush_pending();
    s_vbData = s_staging;
    s_vbUsed = 0;
}

/* ====================================================================== *
 * [etapa 3] EL VERTEX BUFFER DEL CONSUMIDOR.
 *
 * Lo que antes hacia `gxd3d_lock_next_vb` con el buffer de verdad, movido
 * sin cambios de fondo.  Solo lo tocan el consumidor (`d3dq_ejecuta`, o el
 * shim directamente con el grabador a 0) y el arranque/cierre.
 *
 * ★ Sin `d3dq_sinc()`: esto se ejecuta A MITAD DE UN VACIADO, y un vaciado
 * dentro de otro avanzaria la cola dos veces por la misma orden.  Tampoco
 * hace falta: con el grabador puesto, cuando el consumidor llega aqui todo
 * lo anterior ya se ha ejecutado, por orden. */
static void gxd3d_consumidor_rota_vb(IDirect3DDevice9 *dev)
{
    void *p = NULL;
    (void)dev;
    if (s_cvData)
    {
        s_vb[s_vbIndex]->Unlock();
        s_cvData = NULL;
    }
    s_vbIndex = (s_vbIndex + 1) % GXD3D_VB_COUNT;
    if (!s_vb[s_vbIndex])
        return;          /* `gxd3d_init` no llego a crearlos: nada que bloquear */
    {
#if XBOX64_TRAZAS   /* cronometro del Lock: solo lo lee [GXSH] */
        const unsigned long long tL = __mftb();
        s_vb[s_vbIndex]->Lock(0, GXD3D_MAX_VERTS * sizeof(GXD3DVertex), &p, 0);
        gxsh_lock_tb += (unsigned int)(__mftb() - tL);
        gxsh_lock_n++;
#else
        s_vb[s_vbIndex]->Lock(0, GXD3D_MAX_VERTS * sizeof(GXD3DVertex), &p, 0);
#endif
    }
    s_cvData = (GXD3DVertex *)p;
    s_cvUsed = 0;
    /* ★ OBLIGATORIO al cambiar de buffer: sin esto la GPU seguiria leyendo
     * del buffer ANTERIOR. */
    s_cvStream = 0;
}

void gxd3d_consumidor_dibuja(IDirect3DDevice9 *dev, unsigned int prim,
                             unsigned int nprim, const void *verts,
                             unsigned int nverts, unsigned int stride)
{
    if (!dev || !verts || nverts == 0 || nprim == 0) return;
    if (stride != sizeof(GXD3DVertex) || nverts > GXD3D_MAX_VERTS)
    {
        /* No puede pasar: el unico productor es `gxd3d_flush_pending`, con
         * lotes de `s_staging`.  Si pasa, se cuenta en vez de corromper. */
#if XBOX64_TRAZAS
        gxd3d_dbg_vbdrop += nverts;
#endif
        return;
    }

    if (!s_cvData || s_cvUsed + (int)nverts > GXD3D_MAX_VERTS)
        gxd3d_consumidor_rota_vb(dev);
    if (!s_cvData) return;

    memcpy(&s_cvData[s_cvUsed], verts, (size_t)nverts * sizeof(GXD3DVertex));

    if (!s_cvStream)
    {
        dev->SetStreamSource(0, s_vb[s_vbIndex], 0, sizeof(GXD3DVertex));
        s_cvStream = 1;
    }
    dev->DrawPrimitive((D3DPRIMITIVETYPE)prim, (UINT)s_cvUsed, (UINT)nprim);
    s_cvUsed += (int)nverts;
}

/* Al EMPEZAR cada pasada, con el render target del core ya ligado.  Rota el
 * buffer -- el que se uso hace dos pasadas es el que la GPU ya solto -- y da
 * el stream source por perdido: el frontend liga el suyo entre pasadas. */
void gxd3d_consumidor_empieza(void)
{
    IDirect3DDevice9 *dev = d3d_real();
    if (!dev) return;
    gxd3d_consumidor_rota_vb(dev);
}

/* Al ACABAR la pasada, antes de devolver el device.  Dejar el buffer
 * bloqueado mientras el frontend dibuja su quad es pedir un conflicto, y el
 * Lock de la pasada siguiente se comeria la espera. */
void gxd3d_consumidor_acaba(void)
{
    if (s_cvData)
    {
        s_vb[s_vbIndex]->Unlock();
        s_cvData = NULL;
    }
    s_cvStream = 0;
}

/* ===========================================================================
 * GXD3D_DIAG_FONDO -- sonda TEMPORAL para las bandas negras de Paper Mario
 * ===========================================================================
 * Llevo tres hipotesis falsadas razonando desde el codigo (la familia de
 * `guOrtho`, la del vaciado del lote, el recorte) y ninguna movio la aguja.
 * Esto deja de adivinar y parte el problema en dos con una sola lectura.
 *
 * Borra el render target a MAGENTA al empezar la pasada.  El juego repinta
 * el frame entero, asi que en condiciones normales no se ve nada:
 *
 *   bandas MAGENTA  -> son AGUJEROS.  Ahi no dibuja nadie, y la pregunta
 *                      pasa a ser por que falta geometria en tiras.  Mirar
 *                      recorte, viewport y rechazo de primitivas.
 *   bandas NEGRAS   -> alguien dibuja negro ENCIMA.  La pregunta pasa a ser
 *                      quien, y el siguiente paso es un bisect de draws.
 *   PANTALLA magenta-> el juego no repinta el fondo y depende de que el
 *                      framebuffer persista entre frames, que en Paper Mario
 *                      es plausible.  Tambien es un dato.
 *
 * ★ Magenta y no negro a proposito: el negro es el color del defecto, asi
 * que un fondo negro no distinguiria nada -- el mismo error que ya cometi
 * mapeando el "resto" a blanco en una paleta de diagnostico.
 *
 * ★★ CONTESTADA Y APAGADA.  Las bandas salieron MAGENTA -> son AGUJEROS.
 * Eso mato la hipotesis del z-fighting con la que empece (las bandas
 * "negras" del principio eran el render target sin borrar, no algo pintado
 * encima) y mando el diagnostico a GXD3D_DIAG_SIN_ZTEST, arriba.
 *
 * Se apaga: a 60 Hz esto es un parpadeo a pantalla completa, molesto y sin
 * nada mas que aportar.  El caso que de verdad hacia falta cubrir -- que la
 * pantalla arranque limpia en vez de con basura de GPU -- lo resuelve ahora
 * el borrado a NEGRO de `xenon64_video_init` (video_handoff.cpp), que corre
 * una sola vez por ROM.
 *
 * A 0 el binario es el de siempre. */
#define GXD3D_DIAG_FONDO 0

/* ★★★ EL ESPEJO DE ESTADO NO SERVIA PARA NADA, Y ESO ERA EL COSTE DE RICE.
 *
 * `s_rs.valid` se declaraba (:948), se ponia a 0 (:957) y se LEIA en tres
 * sitios -- `rsSet` (:1031), `GX_SetScissor` (:1474) y `GX_SetViewport`
 * (:1510) --, pero **no habia ni una asignacion a 1 en todo el arbol**.  O
 * sea que las tres comparaciones "¿ya esta puesto?" eran siempre falsas:
 *
 *   - `rsSet` vaciaba el lote y llamaba a `SetRenderState` SIEMPRE, aunque
 *     el valor fuera identico al que ya tenia el device.
 *   - `GX_SetScissor` y `GX_SetViewport`, igual.
 *
 * ★ POR QUE A glN64 NO LE DOLIA Y A RICE SI.  glN64 mete todo su estado en
 * `OGL_UpdateStates`, detras de `if (gSP.changed || gDP.changed)`
 * (OpenGL.cpp:1288): con el estado quieto no emite ni un `rsSet`.  Rice
 * llama a `CRender::SetCombinerAndBlender()` SIN GUARDA DE CAMBIO una vez
 * por lote de triangulos (Render.cpp:307), y ademas `ApplyZBias`
 * (OGLRender.cpp:424-433) pone `gGX.GXupdateMtx = true` en las DOS ramas, lo
 * que fuerza un `GX_SetViewport` en cada `RenderFlushTris`.
 *
 * Cuenta por draw de Rice: GX_SetZMode x2 (3 `rsSet` cada uno) +
 * GX_SetBlendMode (3) + GX_SetAlphaCompare (1) + GX_SetViewport (1) +
 * GX_SetScissor (1-2) = ~11 `SetRenderState` REDUNDANTES por draw.  MEDIDO:
 * 5.000-7.400 draws/s, o sea 55.000-77.000 llamadas a D3D por segundo que no
 * cambian nada, y 28-36 us por draw cuando un `DrawPrimitive` de 11 vertices
 * cuesta 1-3.
 *
 * ★★ Y ADEMAS MATABA LA FUSION DE LOTES.  El primer vaciado cae ANTES del
 * `GX_Begin`, asi que al llegar a `GX_End` (:2410) `s_pendActive` ya vale 0
 * y la condicion de fusion no puede cumplirse nunca.  En el log eso salia
 * como `lotes == draws` EXACTAMENTE, en las 116 muestras de 116.
 *
 * ★ POR QUE NO BASTA CON PONER `valid = 1` EN `gxd3d_state_invalidate`, que
 * es la tentacion.  `valid` significa "el espejo es copia fiel del device",
 * y despues del `memset` no lo es: entre dos pasadas el frontend dibuja su
 * quad y su menu y deja en el device SUS estados.  Afirmar el espejo sin
 * programar nada haria que los primeros draws de la pasada heredaran ese
 * estado ajeno -- que es la familia de fallo que este fichero ya documenta
 * dos veces (el recorte de aqui abajo y el `VIEWPORTENABLE`).
 *
 * Asi que se programa TODO lo que el espejo dice, y solo entonces se afirma.
 * Cuesta 11 llamadas por pasada (60/s) y ahorra decenas de miles.
 *
 * ★ El recorte NO entra aqui: lo programa el bloque de `gxd3d_frame_begin`,
 * que ya escribia su espejo, y esta funcion se llama despues.
 *
 * ★ `s_rs.alphaFunc` y `s_rs.alphaRef` estan declarados y NO los usa nadie
 * (censado: no aparecen fuera de la struct).  No se programan. */
static void gxd3d_state_afirmar(void)
{
    D3DVIEWPORT9 vp;

    /* Los valores de partida son los de GX al abrir una lista, no los del
     * `memset`: ver la nota de `colorWrite` en `gxd3d_state_invalidate`. */
    s_rs.cullMode        = D3DCULL_NONE;
    s_rs.zEnable         = D3DZB_FALSE;
    s_rs.zFunc           = D3DCMP_LESSEQUAL;
    s_rs.zWrite          = FALSE;
    s_rs.blendEnable     = FALSE;
    s_rs.srcBlend        = D3DBLEND_SRCALPHA;
    s_rs.dstBlend        = D3DBLEND_INVSRCALPHA;
    s_rs.alphaTestEnable = FALSE;
    s_rs.depthBias       = 0;      /* patron de bits de 0.0f */
    /* `colorWrite` ya lo dejo `gxd3d_state_invalidate` en ARGB. */

    g_pd3dDevice->SetRenderState(D3DRS_CULLMODE,          s_rs.cullMode);
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE,           s_rs.zEnable);
    g_pd3dDevice->SetRenderState(D3DRS_ZFUNC,             s_rs.zFunc);
    g_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,      s_rs.zWrite);
    g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,  s_rs.blendEnable);
    g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND,          s_rs.srcBlend);
    g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND,         s_rs.dstBlend);
    g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE,   s_rs.alphaTestEnable);
    g_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,  s_rs.colorWrite);
    g_pd3dDevice->SetRenderState(D3DRS_DEPTHBIAS,         s_rs.depthBias);

    /* ★ EL VIEWPORT TAMBIEN, y antes no se programaba en toda la pasada.
     *
     * Sin esto el espejo diria 0x0 (del memset) y el device tendria lo que
     * dejara el frontend; con `valid` a 1 eso seria una mentira.  El estado
     * neutro es el render target entero, mismo criterio que el recorte.
     *
     * No toca `s_halfPx`: su bloque en `GX_SetViewport` esta FUERA del `if`
     * a proposito (ver la nota de :1517-1520), asi que el plugin lo
     * recalcula igual aunque el rect ya coincida. */
    vp.X      = 0;
    vp.Y      = 0;
    vp.Width  = (DWORD)xenon64_video_width();
    vp.Height = (DWORD)xenon64_video_height();
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    g_pd3dDevice->SetViewport(&vp);
    s_rs.viewport = vp;
    /* [log 60] El viewport neutro cabe: correccion en identidad. */
    s_vpFix[0] = 1.0f; s_vpFix[1] = 1.0f; s_vpFix[2] = 0.0f; s_vpFix[3] = 0.0f;
    s_halfPxDirty = 1;

    /* Y AHORA si: el espejo es copia fiel del device.
     *
     * ★★ A 0 SE VUELVE AL COMPORTAMIENTO ANTERIOR en un caracter: cada
     * `rsSet`, `GX_SetScissor` y `GX_SetViewport` reprograma siempre.  Es
     * caro -- era el 60 % del coste por draw de Rice-- pero es INMUNE a que
     * alguien cambie el estado del device por detras del espejo, y por eso
     * es la primera palanca que hay que mover si aparece un fallo de dibujo
     * que antes no estaba. */
#define GXD3D_ESPEJO_VALIDO 1
    s_rs.valid = GXD3D_ESPEJO_VALIDO;
}

/* [etapa 3] Si la pasada que empieza es la PRIMERA desde `gxd3d_init`.  Con
 * D3DQ_BIND a 1 es la unica en la que el productor pone su estado a neutro
 * y declara el espejo valido; ver la nota dentro de gxd3d_frame_begin. */
static int s_primeraPasada = 1;

void gxd3d_frame_begin(void)
{
    /* ★ El volcado de texturas dura UN frame: se arma con L3+R3 desde
     * libretro_core.cpp y se consume aqui.  La lista de vistas se reinicia
     * para que cada disparo saque el juego de texturas de ESE frame. */
#if XBOX64_TRAZAS   /* TEXBMP: diagnostico */
    if (g_texbmpArm)
    {
        g_texbmpArm  = 0;
        s_tbmpActivo = 1;
        s_tbmpN      = 0;
        s_tbmpAtados = 0;
        s_tbmpConvs  = 0;
        s_tbmpFallos = 0;
        DebugOutputString("[TEXBMP] volcando las texturas de este frame");
        tbmp_dir();   /* resuelve y REGISTRA la ruta ya, aunque luego no se
                       * vuelque nada: separa "no hay texturas" de "no hay
                       * donde escribir". */
    }
    else if (s_tbmpActivo)
    {
        char aviso[192];
        /* Los dos contadores de paso van DELANTE del resultado a proposito:
         * atados=0 dice "no paso nadie por el atado" y convs=0 con atados>0
         * dice "todas venian de la cache".  Sin ellos, `volcadas=0` no
         * distingue el defecto del camino muerto. */
        sprintf(aviso, "[TEXBMP] hecho: atados=%u convs=%u -> %d volcadas"
                       " (fallos=%u, dir=%s)",
                s_tbmpAtados, s_tbmpConvs, s_tbmpN, s_tbmpFallos,
                s_tbmpDir ? s_tbmpDir : "(sin directorio escribible)");
        DebugOutputString(aviso);
        s_tbmpActivo = 0;
    }
#endif /* XBOX64_TRAZAS (TEXBMP) */

#if GXD3D_DIAG_FONDO
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET,
                        D3DCOLOR_XRGB(255, 0, 255), 1.0f, 0);
#endif

    /* [etapa 3] El bufer de preparacion de vertices vuelve al principio.  Ya
     * no hay vertex buffer que soltar ni que bloquear aqui: eso lo hace el
     * consumidor (`gxd3d_consumidor_empieza`).  Ver la nota de `s_staging`. */
    gxd3d_lock_next_vb();

    /* ★★★ [etapa 3] CON D3DQ_BIND, TODO LO QUE SIGUE CORRE UNA SOLA VEZ.
     *
     * Este bloque existia porque el device es del FRONTEND: entre dos
     * `retro_run` Salvia dibuja con el y deja SU estado, asi que en cada
     * pasada el shim se ponia a neutro, lo volvia a emitir y declaraba su
     * espejo valido.  Funcionaba porque productor y `retro_run` iban
     * sincronizados.
     *
     * En la etapa 4 no lo van, y la cesion del marco ya demostro lo que pasa:
     * resetear a neutro por debajo de una lista a medias le quita el estado
     * que ya habia puesto, y la lista no lo vuelve a emitir (glN64 solo
     * reemite con sus banderas `CHANGED_*`, que son por lista).  Faltaban
     * trozos de geometria.
     *
     * Con D3DQ_BIND a 1 lo arregla el CONSUMIDOR, como `glsm BIND` en mupen:
     * `d3dq_bind()` reaplica al entrar en `retro_run` todo lo que se ejecuto.
     * El espejo del productor pasa a ser lo que en Wii siempre fue -- estado
     * que persiste entre listas -- y este bloque solo hace falta la primera
     * vez, para partir de un estado conocido.
     *
     * Con D3DQ_BIND a 0, el comportamiento de siempre: todo, en cada pasada.
     * Es el aislador si la imagen sale mal. */
#if D3DQ_BIND
    if (s_primeraPasada)
    {
        s_primeraPasada = 0;
#endif

    /* ★ Invalidar el espejo del estado de render UNA VEZ POR PASADA no es
     * opcional: el device esta COMPARTIDO y el frontend pone en el
     * exactamente los estados que cacheamos (ALPHABLENDENABLE, SRCBLEND,
     * DESTBLEND, CULLMODE, ZENABLE, SCISSORTESTENABLE) al dibujar su quad y
     * su menu entre frames.  Sin esto, el espejo cree que ya estan puestos y
     * no los reprograma.  [etapa 3: con D3DQ_BIND lo cubre `d3dq_bind()`.] */
    gxd3d_state_invalidate();

    /* ★★ Y EL RECTANGULO DE RECORTE, QUE INVALIDAR NO BASTA PARA ARREGLAR.
     *
     * Invalidar el espejo hace que la PROXIMA llamada reprograme.  El
     * problema es que puede no haber proxima llamada: glN64 solo reprograma
     * el recorte cuando SU bandera `CHANGED_SCISSOR` esta puesta
     * (OpenGL.cpp:814), y esa bandera es por DISPLAY LIST.  Si el juego no lo
     * cambia, glN64 da por hecho que el que dejo sigue vigente -- un supuesto
     * correcto en Wii, donde nadie mas toca el device.
     *
     * Aqui el device esta COMPARTIDO: entre dos `retro_run` el frontend
     * dibuja su quad a pantalla completa y su menu, y puede dejar otro rect.
     * Los primeros dibujos de la pasada salen entonces recortados por algo
     * ajeno al juego -- y como lo que sobrevive depende de en que punto de la
     * lista caiga, salen AGUJEROS que cambian de sitio entre frames.
     *
     * Medido en Paper Mario: `scisclip=447771` de `draws=1518111`, o sea el
     * 29,5% de los dibujos con la pantalla recortada, y `scismin=4752`
     * (132x36), que es uno de los rects de bocadillo.
     *
     * Asi que la pasada arranca SIN recortar, que es el estado neutro: si el
     * juego quiere recortar, `OGL_UpdateStates` lo pondra.  Mismo criterio y
     * mismo sitio que el `VIEWPORTENABLE` de aqui abajo. */
    {
        RECT full;
        full.left   = 0;
        full.top    = 0;
        full.right  = (LONG)xenon64_video_width();
        full.bottom = (LONG)xenon64_video_height();
        g_pd3dDevice->SetScissorRect(&full);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        s_rs.scissor       = full;
        s_rs.scissorEnable = FALSE;
    }

#if GXD3D_ZCLEAR_POR_FRAME
    /* ★★ ARREGLO CANDIDATO del parpadeo, a la espera de lo que diga `zclear`.
     *
     * Si el plugin no reconoce como borrado de z-buffer lo que hace el juego,
     * la profundidad del frame anterior sobrevive: la misma geometria a la
     * misma z pasa un frame y falla el siguiente, y eso es un parpadeo a
     * 30 Hz peor cuanto mas capas 2D solapadas haya -- que es lo que se ve
     * dentro de la casa en Paper Mario.
     *
     * Borrar la profundidad al empezar la pasada es lo que hace el RDP con su
     * z-buffer en cada frame, asi que es lo conservador.  NO se toca el color:
     * ahi el juego SI puede depender de que persista.
     *
     * A 0 si resulta que `zclear` ya sale a 60/s -- entonces el plugin si
     * borra y el parpadeo es otra cosa. */
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
#endif

    /* [etapa 3] Aqui se soltaba el vertex buffer y se bloqueaba el siguiente
     * del anillo.  Ahora el productor no tiene vertex buffer: el reinicio de
     * su bufer de preparacion va al principio de la funcion, y la rotacion
     * del de verdad la hace el consumidor. */

    /* ★★ `VIEWPORTENABLE` A TRUE, Y NO ES COSMETICO.
     *
     * Nuestro vertex shader emite coordenadas de RECORTE.  El frontend pone
     * este render state a FALSE porque su quad va pretransformado, y si se
     * queda asi el shader se queda sin la transformacion de viewport y LA
     * ESCENA ENTERA CAE EN UN PUNTO: textura resuelta NEGRA con el juego
     * corriendo, dibujando y sonando.
     *
     * Eso ya paso EXACTAMENTE igual al portar Rice a libretro, y costo una
     * sesion entera porque el sintoma -- todo negro, tambien el menu-- no
     * apunta al estado del device (project_xenon64_libretro_port, "TODO
     * NEGRO = NADIE RE-ESTABLECE SU PIPELINE").
     *
     * `xenon64_core_state_apply()` ya lo hace para Rice y se llama desde
     * `xenon64_video_frame_begin()`, asi que hoy esta cubierto por ahi; se
     * repite aqui para que el backend sea autosuficiente y no dependa de que
     * el otro plugin le prepare el terreno. */
    g_pd3dDevice->SetRenderState(D3DRS_VIEWPORTENABLE, TRUE);

    /* ★★★ Y AQUI SE AFIRMA EL ESPEJO DE ESTADO.  Va DESPUES del recorte y
     * del `VIEWPORTENABLE` de arriba: hasta este punto el device todavia
     * lleva cosas del frontend, y `valid = 1` solo puede decirse cuando ya
     * no queda ninguna.  El porque largo esta en la funcion. */
    gxd3d_state_afirmar();

    /* ★ Aqui, y solo aqui, se da por perdido lo que ligamos: entre pasadas
     * ha corrido el frontend con SU vertex shader, SU declaracion y SU
     * stream source.  Dentro de la pasada nadie nos los toca, asi que
     * `GX_End` ya no los reafirma por draw (ver la nota de alli).  Mismo
     * criterio que el `CaducarEspejo()` de Rice. */
    s_bindValid = 0;
    /* El frontend tambien liga SUS texturas y samplers entre pasadas. */
    memset(s_samp, 0, sizeof(s_samp));

    s_mtxDirty    = 1;
    s_halfPxDirty = 1;   /* c8 tambien es nuestro y el frontend lo pisa */
    gxd3d_tev_dirty();
    /* ★ Y el pixel shader, que el frontend tambien liga entre pasadas.  Este
     * es el UNICO instante en que puede habernoslo robado, asi que reafirmarlo
     * aqui basta y `gxd3d_tev_apply` deja de hacerlo en cada lote (~6.000/s).
     * Ver la nota de `s_psBindValid` en xenon/gx_d3d9_tev.cpp. */
    gxd3d_tev_invalidar_ligado();

#if D3DQ_BIND
    }   /* fin del bloque de la primera pasada */
#endif
}

void gxd3d_frame_end(void)
{
    /* ★ LO PRIMERO: lo que quede pendiente hay que emitirlo antes de
     * devolver el control.  [etapa 3] Ya no hay vertex buffer que soltar
     * aqui: el lote se copia al pool y el buffer de verdad lo suelta el
     * consumidor (`gxd3d_consumidor_acaba`). */
    gxd3d_flush_pending();
}

/* ------------------------------------------------------------------ */
/* ★★ DIBUJAR SOBRE EL DEPTH IMAGE (GLideN64 ccff245f, Pilotwings)     */
/* ------------------------------------------------------------------ */
/* Ver GXD3D_SetRenderToDepth en gccore.h y la nota del final del pixel
 * shader en gx_d3d9_tev.cpp.  Aqui esta la mitad del device:
 *
 *   - la COPIA DE Z (`gxd3d_zcopia`), el `copyDepthBufferTexture` de
 *     GLideN64 (DepthBuffer.cpp:307-315): un `Resolve` de la profundidad a
 *     una textura D24S8, UNA vez por fase -- GLideN64 la hace en el primer
 *     dibujo con Z_CMP y no la repite (`m_copied`) hasta que se vuelve a
 *     enganchar el depth buffer; aqui el plugin la invalida al cambiar de
 *     color image (GXD3D_InvalidaCopiaZ) --.  Sin esperas: el Resolve es una
 *     orden mas de la cola, detras de los dibujos que la preceden.
 *   - la Z JERARQUICA.  El shader escribe una profundidad que no es la
 *     rasterizada y que puede ser MAS LEJANA que la que habia (ZFUNC
 *     ALWAYS), asi que la Hi-Z dejaria de ser conservadora y descartaria
 *     geometria buena a trozos.  Se apaga en cuanto se dibuja asi y no se
 *     vuelve a encender hasta el siguiente borrado de Z, que la reinicia
 *     (`s_hizSucio`).  GXD3D_SetDepthClamp la apaga por lo mismo mientras
 *     dura, y las dos pasan por `gxd3d_hiz_aplica`. */
static u8  s_rtzModo   = 0;        /* GXD3D_SetRenderToDepth vigente        */
static int s_hizSucio  = 0;        /* Hi-Z apagada hasta el proximo borrado */
static IDirect3DTexture9 *s_zcTex = NULL;   /* la copia de Z               */
static u16 s_zcW = 0, s_zcH = 0;
static int s_zcValida  = 0;        /* hecha para la fase actual             */

/* Medidas, por segundo, en [RTZ] (gxd3d_informe).  Contadores de paso
 * delante del valor: `l1`/`l2` dicen si hubo dibujos de cada modo, y solo
 * entonces significan algo `copias` y `fallos`. */
static unsigned int s_stRtz1 = 0, s_stRtz2 = 0, s_stZcCopias = 0, s_stZcFallos = 0;

/* ★ [ISS64] El decal del N64 por shader (GXD3D_SetDecalN64): lee la MISMA
 * copia de Z, pero la necesita AL DIA -- los poligonos de la silueta acaban
 * de escribir el Z justo antes --, asi que se rehace en cada lote decal si
 * desde la ultima copia algun lote escribio profundidad o se borro el Z
 * (`s_zCambio`). */
static u8  s_decN64  = 0;
static int s_zCambio = 1;
static unsigned int s_stDec = 0;

/* ★ [Beetle Adventure Racing] La niebla por Z (GXD3D_SetNieblaZ): lee la
 * MISMA copia de Z, al dia como el decal, y la curva de 256x1 en ARGB
 * (color de niebla y alfa por indice del Z del N64).  Dos texturas para la
 * curva, alternas: si cambia, la nueva se sube en la otra y la que pueda
 * estar leyendo todavia la GPU no se toca. */
static u8  s_nieblaZ = 0;
static IDirect3DTexture9 *s_nzCurva[2] = { NULL, NULL };
static int s_nzActual = -1;              /* la curva vigente, -1 = ninguna */
static u32 s_nzDatos[256];
static float s_nzPar[4];                 /* uNieblaZPar (GXD3D_NIEBLAZ_CONST) */

static void gxd3d_hiz_aplica(void)
{
    const int off = (g_gxTev.zClamp || s_rtzModo || s_hizSucio) ? 1 : 0;
    g_pd3dDevice->SetRenderState(D3DRS_HIZENABLE,
                                 off ? D3DHIZ_DISABLE : D3DHIZ_AUTOMATIC);
    g_pd3dDevice->SetRenderState(D3DRS_HIZWRITEENABLE,
                                 off ? D3DHIZ_DISABLE : D3DHIZ_AUTOMATIC);
}

/* Lo llaman los Clear() con D3DCLEAR_ZBUFFER, ANTES del Clear: con la Hi-Z
 * encendida el borrado la reinicia.  Si en ese momento sigue haciendo falta
 * apagada (se esta dibujando sobre el depth image o con depth clamp), se
 * queda sucia y lo arreglara el borrado siguiente. */
static void gxd3d_hiz_borrado_z(void)
{
    s_zCambio = 1;   /* la copia de Z del decal ya no vale */
    if (s_hizSucio && !s_rtzModo && !g_gxTev.zClamp)
    {
        s_hizSucio = 0;
        gxd3d_hiz_aplica();
    }
}

static void gxd3d_zcopia(void)
{
    const UINT w = (UINT)xenon64_video_width();
    const UINT h = (UINT)xenon64_video_height();
    D3DRECT r;

    if (!w || !h)
        return;
    if (s_zcTex && (s_zcW != (u16)w || s_zcH != (u16)h))
    {
        d3dq_soltar((D3DResource *)s_zcTex);
        s_zcTex = NULL;
    }
    if (!s_zcTex)
    {
        /* Tileada, como todo destino de un Resolve
         * (reference_xbox360_resolve_siempre_tilea). */
        if (FAILED(g_pd3dDevice->CreateTexture(w, h, 1, 0, D3DFMT_D24S8,
                                               D3DPOOL_DEFAULT, &s_zcTex, NULL)))
        {
            s_zcTex = NULL;
#if XBOX64_TRAZAS
            s_stZcFallos++;
#endif
            return;
        }
        s_zcW = (u16)w;
        s_zcH = (u16)h;
    }
    r.x1 = 0;  r.y1 = 0;
    r.x2 = (LONG)w;  r.y2 = (LONG)h;
    g_pd3dDevice->Resolve(D3DRESOLVE_DEPTHSTENCIL, &r, (D3DBaseTexture *)s_zcTex,
                          NULL, 0, 0, NULL, 0.0f, 0, NULL);
    s_zcValida = 1;
    s_zCambio  = 0;
#if XBOX64_TRAZAS
    s_stZcCopias++;
#endif
}

/* Al ARRANCAR cada lote del modo 2: la copia en su sampler y su tamano en su
 * constante.  Se reafirma en cada lote y no se espeja: son pocos (solo los
 * dibujos sobre el depth image) y asi nada de lo que toque el sampler 3 entre
 * medias -- GX_InvalidateTexAll, el frontend entre pasadas -- lo puede dejar
 * sin copia. */
static void gxd3d_zcopia_liga_tex(IDirect3DTexture9 *tex, u16 tw, u16 th)
{
    float inv[4];
    const DWORD s = GXD3D_ZCOPIA_SAMPLER;

    g_pd3dDevice->SetTexture(s, (D3DBaseTexture *)tex);
    g_pd3dDevice->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    g_pd3dDevice->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    g_pd3dDevice->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    g_pd3dDevice->SetSamplerState(s, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP);
    g_pd3dDevice->SetSamplerState(s, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP);
    /* El espejo del sampler 3 ya no dice la verdad: que no se fie de el. */
    s_samp[s].valid = 0;
    s_texBound[s]   = (tex != NULL);

    inv[0] = tw ? 1.0f / (float)tw : 0.0f;
    inv[1] = th ? 1.0f / (float)th : 0.0f;
    inv[2] = 0.0f;
    inv[3] = 0.0f;
    g_pd3dDevice->SetPixelShaderConstantF(GXD3D_ZCOPIA_CONST, inv, 1);
}

static void gxd3d_zcopia_liga(void)
{
    gxd3d_zcopia_liga_tex(s_zcTex, s_zcW, s_zcH);
}

/* ★★ [Mario Golf] LOS Z GUARDADOS POR DIRECCION (GLideN64 7dca7f8f).
 *
 * GLideN64 tiene un Z por buffer; con la camara quieta Mario Golf no vuelve a
 * dibujar el campo y COPIA con texrects el Z de su depth buffer temporal al
 * principal, y GLideN64 sirve para esos texrects la textura de PROFUNDIDAD
 * del buffer de origen (FrameBuffer::getTexture, `getDepthTexture`).  Aqui
 * hay un solo Z, el de la EDRAM, asi que el del temporal se GUARDA al dejar de
 * ser el depth image (GXD3D_GuardaZ, un Resolve sin esperas) y el texrect
 * de la copia lo escribe pixel a pixel (modo 3 de GXD3D_SetRenderToDepth).
 * Medido con [MGOLF] (log 5): zi 0400 -> 002EA000 (se dibuja la escena),
 * 002EA000 -> 0400, y 38 texrects COPY de 304x6 sobre ci = zi = 0400 que leen
 * 002EA000 con `st` = su propia posicion. */
#define GXD3D_ZG_MAX 4
struct GXD3DZGuardada
{
    u32 dir;                  /* direccion N64 del depth buffer, 0 = libre */
    IDirect3DTexture9 *tex;   /* D24S8 del tamano del render target        */
    u16 w, h;
    u32 uso;                  /* para el LRU                                */
};
static GXD3DZGuardada s_zg[GXD3D_ZG_MAX];
static u32 s_zgUso = 0;
static int s_zgActual = -1;          /* la que liga el modo 3              */
static u32 s_zEscrituras = 0;        /* lotes que han escrito Z            */
static unsigned int s_stZgGuardadas = 0, s_stZgLotes = 0, s_stZgFallos = 0;

unsigned int GXD3D_ZEscrituras(void)
{
    return s_zEscrituras;
}

void GXD3D_GuardaZ(u32 dir)
{
    const UINT w = (UINT)xenon64_video_width();
    const UINT h = (UINT)xenon64_video_height();
    GXD3DZGuardada *e = NULL;
    D3DRECT r;
    int i;

    if (!dir || !w || !h)
        return;
    for (i = 0; i < GXD3D_ZG_MAX; i++)
        if (s_zg[i].dir == dir) { e = &s_zg[i]; break; }
    if (!e)
    {
        e = &s_zg[0];
        for (i = 1; i < GXD3D_ZG_MAX; i++)
            if (s_zg[i].dir == 0 || s_zg[i].uso < e->uso)
            {
                e = &s_zg[i];
                if (s_zg[i].dir == 0)
                    break;
            }
    }
    if (e->tex && (e->w != (u16)w || e->h != (u16)h))
    {
        d3dq_soltar((D3DResource *)e->tex);
        e->tex = NULL;
    }
    if (!e->tex)
    {
        if (FAILED(g_pd3dDevice->CreateTexture(w, h, 1, 0, D3DFMT_D24S8,
                                               D3DPOOL_DEFAULT, &e->tex, NULL)))
        {
            e->tex = NULL;
            e->dir = 0;
#if XBOX64_TRAZAS
            s_stZgFallos++;
#endif
            return;
        }
        e->w = (u16)w;
        e->h = (u16)h;
    }
    gxd3d_flush_pending();   /* lo dibujado con ese Z tiene que entrar */
    r.x1 = 0;  r.y1 = 0;
    r.x2 = (LONG)w;  r.y2 = (LONG)h;
    g_pd3dDevice->Resolve(D3DRESOLVE_DEPTHSTENCIL, &r, (D3DBaseTexture *)e->tex,
                          NULL, 0, 0, NULL, 0.0f, 0, NULL);
    e->dir = dir;
    e->uso = ++s_zgUso;
#if XBOX64_TRAZAS
    s_stZgGuardadas++;
#endif
}

int GXD3D_UsaZGuardada(u32 dir)
{
    int i;
    for (i = 0; i < GXD3D_ZG_MAX; i++)
        if (s_zg[i].dir == dir && s_zg[i].tex)
        {
            s_zgActual = i;
            s_zg[i].uso = ++s_zgUso;
            return 1;
        }
    return 0;
}

/* [Resident Evil 2] uRe2Z del modo 5 (GXD3D_RE2Z_CONST): ver
 * GXD3D_SetProfundidadRE2 en gccore.h. */
static float s_re2Par[4] = { 0.0f, 1.0f / 261632.0f, 0.0f, 0.0f };

void GXD3D_SetProfundidadRE2(f32 nearz, f32 farz)
{
    const float esc = (farz - nearz) / 261632.0f;
    if (s_re2Par[0] == nearz && s_re2Par[1] == esc)
        return;
    if (s_rtzModo == 5)
        gxd3d_flush_pending();   /* lo pendiente, con los suyos */
    s_re2Par[0] = nearz;
    s_re2Par[1] = esc;
}

void GXD3D_SetRenderToDepth(u8 modo)
{
    if (modo > 5) modo = 5;   /* 3 = copiar el Z guardado (GXD3D_UsaZGuardada);
                               * 4 = [Zelda MM] idem solo donde el Z actual es 0;
                               * 5 = [Resident Evil 2] la palabra de Z del texel */
    /* El modo 2 sin copia valida tambien entra: el plugin pudo invalidarla
     * sin pasar por otro modo. */
    if (modo == s_rtzModo && !(modo == 2 && !s_zcValida))
        return;
    gxd3d_flush_pending();   /* lo pendiente se dibuja con su modo */
    if (modo == 2 && !s_zcValida)
        gxd3d_zcopia();
    s_rtzModo = modo;
    if (modo)
        s_hizSucio = 1;
    gxd3d_tev_rtz(modo);
    gxd3d_hiz_aplica();
}

void GXD3D_InvalidaCopiaZ(void)
{
    s_zcValida = 0;
}

void GXD3D_SetDecalN64(u8 on)
{
    on = on ? 1 : 0;
    if (on == s_decN64)
        return;
    gxd3d_flush_pending();   /* lo pendiente se dibuja con su modo */
    s_decN64 = on;
    gxd3d_tev_decn64(on);
}

int GXD3D_SetNieblaZ(u8 on, const u8 *alfa, GXColor color,
                     f32 nearz, f32 farz, u16 borrado)
{
    on = (on && alfa) ? 1 : 0;
    if (!on)
    {
        if (!s_nieblaZ)
            return 0;
        gxd3d_flush_pending();   /* el texrect de la niebla, con su modo */
        s_nieblaZ = 0;
        gxd3d_tev_nieblaz(0);
        return 0;
    }

    gxd3d_flush_pending();       /* lo pendiente se dibuja con su modo */
    {
        const u32 rgb = ((u32)color.r << 16) | ((u32)color.g << 8) | (u32)color.b;
        u32 curva[256];
        int i, cambia = (s_nzActual < 0) ? 1 : 0;
        for (i = 0; i < 256; i++)
        {
            curva[i] = ((u32)alfa[i] << 24) | rgb;
            if (curva[i] != s_nzDatos[i])
                cambia = 1;
        }
        if (cambia)
        {
            const int k = (s_nzActual + 1) & 1;   /* -1 -> 0 */
            /* Lineal: la sube LockRect, como las texturas de GX_LoadTexObj
             * (reference_xbox360_lockrect_no_tilea). */
            if (!s_nzCurva[k] &&
                FAILED(g_pd3dDevice->CreateTexture(256, 1, 1, 0, D3DFMT_LIN_A8R8G8B8,
                                                   D3DPOOL_DEFAULT, &s_nzCurva[k], NULL)))
            {
                s_nzCurva[k] = NULL;
                if (s_nieblaZ)
                {
                    s_nieblaZ = 0;
                    gxd3d_tev_nieblaz(0);
                }
                return 0;
            }
            memcpy(s_nzDatos, curva, sizeof(s_nzDatos));
            /* La orden copia los texels al pool: `s_nzDatos` se puede volver
             * a escribir en cuanto vuelva. */
#if D3D_GRABADOR
            g_pd3dDevice->SubeTextura(s_nzCurva[k], s_nzDatos, 256, 1, 256, GX_CLAMP);
#else
            gxd3d_consumidor_sube_tex(s_nzCurva[k], s_nzDatos, 256, 1, 256, GX_CLAMP);
#endif
            s_nzActual = k;
        }
    }
    /* El paso del Z del EFB al del N64, el de DepthCopy.cpp (_dcBuildLut):
     * (z - nearz) * 261632 / (farz - nearz).
     * ★★ [log 34] Y LA PALABRA DE BORRADO SOLO PARA EL VALOR DEL BORRADO, NO
     * PARA EL CUBO DE 256 z24 DE DepthCopy.  La curva de la carrera vale FA
     * en el indice 247 (el tope de la geometria, 261632) y 00 en el 255 (el
     * de FFFC, el cielo: sin niebla).  Con el cubo, la geometria del fondo
     * del escenario -- que en perspectiva cae en las ultimas z24 -- salia
     * como cielo: alfa 0, sin niebla, con un corte a distancia fija (el
     * "cubo" que vio el usuario, "solo en ocasiones").  En el N64 la
     * geometria no pasa de 0x3FE00 y nunca se confunde con el borrado.  El
     * borrado del shim es Clear(Z = 1.0) y el relleno de OGL_DrawDepthFill
     * escribe 0xFFFFFF/0x1000000: los dos quedan en z24 FFFFFE (medido en
     * la copia por CPU, `raw esq=FFFFFE00`), asi que el umbral va entre
     * FFFFFD y FFFFFE, sea cual sea farz.  (GLideN64 manda al 255 todo
     * bufZ > 0.999, peor aun: el blog de 2014 ya veia "polygons on the
     * left" mal sin saber por que.) */
    {
        const f32 rango = (farz != nearz) ? (farz - nearz) : 1.0f;
        s_nzPar[0] = nearz;
        s_nzPar[1] = 261632.0f / rango;
        s_nzPar[2] = (float)((16777214.0 - 0.5) / 16777215.0);
        s_nzPar[3] = (float)borrado;
    }
    s_nieblaZ = 1;
    gxd3d_tev_nieblaz(1);
    return 1;
}

/* Al ARRANCAR cada lote, despues de ligar su estado: la copia de Z al dia
 * para un lote decal, y la cuenta de si este lote cambia el Z.  El lote
 * anterior ya esta vaciado, asi que el Resolve va detras de el en la cola. */
static void gxd3d_zcopia_lote(void)
{
    if (s_decN64)
    {
        if (s_zCambio || !s_zcTex)
            gxd3d_zcopia();
        gxd3d_zcopia_liga();
#if XBOX64_TRAZAS
        s_stDec++;
#endif
    }
    /* [Beetle] La niebla por Z: la copia al dia, como el decal, y la curva en
     * su sampler.  Se reafirma en cada lote, como la copia: el sampler 4 no
     * tiene espejo y nadie mas lo toca. */
    if (s_nieblaZ && s_nzActual >= 0)
    {
        const DWORD s = GXD3D_NIEBLAZ_SAMPLER;
        if (s_zCambio || !s_zcTex)
            gxd3d_zcopia();
        gxd3d_zcopia_liga();
        g_pd3dDevice->SetTexture(s, (D3DBaseTexture *)s_nzCurva[s_nzActual]);
        g_pd3dDevice->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        g_pd3dDevice->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        g_pd3dDevice->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        g_pd3dDevice->SetSamplerState(s, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP);
        g_pd3dDevice->SetSamplerState(s, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP);
        g_pd3dDevice->SetPixelShaderConstantF(GXD3D_NIEBLAZ_CONST, s_nzPar, 1);
    }
    /* [Mario Golf] El modo 3: el Z guardado que eligio GXD3D_UsaZGuardada.
     * [Zelda MM, Lente de la Verdad] El modo 4 tambien, y ademas una copia del
     * Z ACTUAL en GXD3D_ZACTUAL_SAMPLER (resuelta aqui, antes de que este
     * lote escriba nada): el shader descarta donde vale mas de 0.  Mismo
     * tamano que la guardada (las dos son del render target), asi que vale
     * la constante uZCopiaInv que pone gxd3d_zcopia_liga_tex. */
    if ((s_rtzModo == 3 || s_rtzModo == 4) && s_zgActual >= 0)
    {
        const GXD3DZGuardada *e = &s_zg[s_zgActual];
        if (s_rtzModo == 4)
        {
            const DWORD s = GXD3D_ZACTUAL_SAMPLER;
            gxd3d_zcopia();
            g_pd3dDevice->SetTexture(s, (D3DBaseTexture *)s_zcTex);
            g_pd3dDevice->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            g_pd3dDevice->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            g_pd3dDevice->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            g_pd3dDevice->SetSamplerState(s, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP);
            g_pd3dDevice->SetSamplerState(s, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP);
        }
        gxd3d_zcopia_liga_tex(e->tex, e->w, e->h);
#if XBOX64_TRAZAS
        s_stZgLotes++;
#endif
    }
    /* [Resident Evil 2] El modo 5: su constante, en cada lote como la de la
     * niebla (comparten registro; ver GXD3D_RE2Z_CONST). */
    if (s_rtzModo == 5)
        g_pd3dDevice->SetPixelShaderConstantF(GXD3D_RE2Z_CONST, s_re2Par, 1);
    if (s_rs.zEnable == D3DZB_TRUE && s_rs.zWrite)
    {
        s_zCambio = 1;
        s_zEscrituras++;   /* [Mario Golf] ver GXD3D_ZEscrituras */
    }
}

/* gxd3d_shutdown: la copia se suelta y el modo vuelve a 0. */
static void gxd3d_rtz_shutdown(void)
{
    /* [Zelda MM] El modo 4 deja la copia del Z ligada en el sampler 5. */
    g_pd3dDevice->SetTexture(GXD3D_ZACTUAL_SAMPLER, NULL);
    if (s_zcTex)
    {
        d3dq_soltar((D3DResource *)s_zcTex);
        s_zcTex = NULL;
    }
    s_zcW = s_zcH = 0;
    s_zcValida = 0;
    s_rtzModo  = 0;
    s_hizSucio = 0;
    gxd3d_tev_rtz(0);
    s_decN64   = 0;
    s_zCambio  = 1;
    gxd3d_tev_decn64(0);
    /* [Beetle] La niebla por Z: el sampler desligado antes de soltar la
     * curva, o el device la muestrearia ya liberada. */
    s_nieblaZ = 0;
    gxd3d_tev_nieblaz(0);
    g_pd3dDevice->SetTexture(GXD3D_NIEBLAZ_SAMPLER, NULL);
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            if (s_nzCurva[i])
                d3dq_soltar((D3DResource *)s_nzCurva[i]);
            s_nzCurva[i] = NULL;
        }
        s_nzActual = -1;
        memset(s_nzDatos, 0, sizeof(s_nzDatos));
    }
    {
        int i;
        for (i = 0; i < GXD3D_ZG_MAX; i++)
        {
            if (s_zg[i].tex)
                d3dq_soltar((D3DResource *)s_zg[i].tex);
            s_zg[i].tex = NULL;
            s_zg[i].dir = 0;
            s_zg[i].w = s_zg[i].h = 0;
        }
        s_zgActual = -1;
    }
    gxd3d_hiz_aplica();   /* la Hi-Z, como la dejaria el depth clamp */
}

/* ------------------------------------------------------------------ */
/* ★★ EL INFORME DEL SHIM                                              */
/* ------------------------------------------------------------------ */
/* Los contadores de arriba llevaban todo el port acumulando sin que nadie
 * los leyera (la parte que los volcaba se quedo en Xenon64).  Esto es solo
 * el volcado: no mide nada nuevo.
 *
 * ★ LAS TRES PREGUNTAS QUE CONTESTA, y como se leen:
 *
 *  1. ¿Donde se va la cola de `GX_End`?  `bind/mtx/tev/prim` la parten.  Con
 *     `SetPixelShader` ya fuera del camino por lote, si `tev` sigue grande el
 *     culpable es la busqueda en la cache de shaders o las constantes.
 *
 *  2. ¿Bloquea el `DrawPrimitive`?  `ns/draw: prim` se compara DIRECTAMENTE
 *     con los 740 ns que mide Rice en esta misma consola sin vsync
 *     (d3d9_backend.cpp:1476).  Si sale muy por encima, el tiempo no es
 *     trabajo de CPU: es contrapresion de la GPU, y el arreglo esta al otro
 *     lado del `Present`.
 *
 *  3. ¿Se fusionan los lotes?  `fusion%`.  Si cae justo cuando aparecen
 *     `rect` intercalados, la geometria y los quads se estan pisando y el
 *     numero de draws se dispara sin que crezca el de triangulos.
 *
 * `vert` no es un reloj: es `lote - draw`, o sea lo que queda del tramo
 * `GX_Begin..GX_End` una vez descontada su cola.  Poner un reloj por
 * atributo mediria sobre todo la propia sonda (ver la nota de `gxsh_lote_tb`).
 *
 * La frecuencia se PREGUNTA, no se cablea: mismo motivo que en
 * xbox/compat_posix.c:15-19. */
void gxd3d_informe(unsigned frames)
{
#if XBOX64_TRAZAS   /* version publica: informe vacio, la firma se queda */
#if !XBOX64_SONDAS
    /* ★★ [Fase 6] UNA LINEA, Y SOLO CONTADORES DE PASO.
     *
     * El maestro apagado (source/xenon64_diag.h) callaba las 6 lineas del
     * informe.  Pero los contadores SIGUEN SUMANDO -- son incrementos
     * sueltos por lote y por draw, no instrumentacion cara -- asi que
     * callarlos del todo era tirar informacion gratis.
     *
     * Y hacia falta: con Rice en negro, la primera pregunta no es "cuanto
     * cuesta" sino "PASA ALGUIEN POR AQUI?".  Sin esta linea hay que elegir
     * entre no saber nada o encender SONDAS entero, que mete 6 lineas por
     * segundo, sondas por triangulo y un coste que cambia lo que se mide.
     *
     * ★ ES LA REGLA DE ESTE ARBOL: el contador de PASO antes que el valor.
     * Un `draws=0` distingue "el plugin no dibuja" de "dibuja y no se ve",
     * que son dos investigaciones que no se parecen en nada.  Un cero de
     * tiempo no distingue eso.
     *
     * Cuesta un sprintf por segundo.  Se queda. */
    {
        /* 320 y no 192: la primera linea mide 230 en su peor caso y la
         * segunda 179, los dos ESTIMADOS por mi.  Un margen de 13 bytes
         * sobre una estimacion propia no es un margen. */
        char l[320];
        const unsigned d = gxd3d_dbg_draws, v = gxd3d_dbg_verts;
        const unsigned lo = gxsh_lote_n,    sg = gxd3d_dbg_shgen;

        if (frames == 0) frames = 1;
        sprintf(l, "[GXD3D] draws=%u/s verts=%u/s lotes=%u/s shaders=%u "
                   "(compilar=%u us, fallos=%u) | por frame: draws=%u verts=%u "
                   "| initobj=%u inval=%u libera=%u",
                d, v, lo, sg, gxd3d_dbg_shcompus, gxd3d_dbg_shfail,
                d / frames, v / frames,
                gxd3d_dbg_initobj, gxd3d_dbg_texinval, gxd3d_dbg_texfree);
        DebugOutputString(l);

        /* ★★ `compilar` ES EL SOSPECHOSO DEL TIRON, Y EL CRONOMETRO YA
         * ESTABA PUESTO: `gx_d3d9_tev.cpp:1154-1159` mide cada
         * `D3DXCompileShader` y su comentario dice que es "la cifra que dice
         * si hace falta cache en disco".  Solo que no se publicaba con el
         * maestro apagado, asi que la medida existia y no la veia nadie.
         *
         * El shim compila HLSL EN EJECUCION cada vez que aparece una
         * combinacion TEV nueva.  En el log de las 16:0x cada pico de `emu`
         * (18-44 ms) cae en el mismo segundo en que sube `shaders`, y 18
         * shaders nuevos coinciden con el pico de 44 ms.  Si `compilar` se
         * parece a ese pico, el tiron es esto y la salida es cachear el
         * bytecode; si no, la correlacion era casualidad y hay que buscar en
         * la CPU emulada. */
        gxd3d_dbg_shcompus = 0;

        /* ★ `inval` es el numero que decide si la invalidacion de
         * `GX_InitTexObj` esta de mas.  Si se parece a `draws`, Rice la
         * llama al ENLAZAR y no solo al reconstruir: entonces hay que sacar
         * la invalidacion de ahi y buscar otra senyal.  Si se queda en unas
         * pocas por segundo y sube de golpe al cambiar de nivel, esta en su
         * sitio. */
        gxd3d_dbg_initobj  = 0;
        gxd3d_dbg_texinval = 0;
        gxd3d_dbg_texfree  = 0;

        /* ★★ LA SEGUNDA LINEA ES LA DEL "DIBUJA Y NO SE VE".
         *
         * Con la primera ya sabemos que la geometria sale (draws>0).  Estos
         * son los cuatro sospechosos de que salga y no aparezca, y los
         * cuatro los mantiene ya el shim sin coste:
         *
         *   degen     rects de viewport/scissor con ancho o alto < 1.
         *   scisclip  draws emitidos con el recorte dejando area cero.
         *   vpz       near/far fuera de [0,1]: D3D los acota y el rango de
         *             profundidad deja de ser el que el plugin cree.
         *   scismin   el recorte MAS PEQUENYO del segundo, con sus
         *             dimensiones.  Ver la nota de al lado del sprintf.
         *
         * ★★ `maxxy` SALIO DE ESTA LINEA, y conviene saber por que antes de
         * echarlo de menos: lo alimentaba `GX_Position3f32`, o sea el modo
         * inmediato, y desde que Rice pasa los triangulos por el camino de
         * lote (igual que glN64) solo veia ya los rects.  Un "el mayor x,y
         * de la escena" que no incluye la geometria no dice lo que promete,
         * y la linea no da para los dos campos (el embudo del log corta a
         * 127 caracteres).
         *
         * La global sigue existiendo y actualizandose por el camino
         * inmediato: si algun dia hace falta de nuevo -- la pregunta que
         * contestaba, "en que espacio manda el plugin los vertices", sigue
         * siendo buena-- hay que volver a alimentarla TAMBIEN desde el
         * camino de lote o volvera a mentir. */
        /* ★ `scismin` es el recorte MAS PEQUENYO del segundo, con sus
         * dimensiones.  El `scis=` de al lado es el ULTIMO estado, que casi
         * siempre sale a pantalla completa y por eso no dice nada de lo que
         * paso durante el frame. */
        sprintf(l, "[GXD3D2] degen=%u scisclip=%u vpz=%u | scismin=%ux%u | "
                   "vp=%u,%u %ux%u z=%.2f..%.2f | scis=%d,%d-%d,%d on=%u",
                gxd3d_dbg_degen, gxd3d_dbg_scisclip, gxd3d_dbg_vpz,
                gxd3d_dbg_scisminw, gxd3d_dbg_scisminh,
                (unsigned)s_rs.viewport.X,     (unsigned)s_rs.viewport.Y,
                (unsigned)s_rs.viewport.Width, (unsigned)s_rs.viewport.Height,
                (double)s_rs.viewport.MinZ,    (double)s_rs.viewport.MaxZ,
                (int)s_rs.scissor.left,  (int)s_rs.scissor.top,
                (int)s_rs.scissor.right, (int)s_rs.scissor.bottom,
                (unsigned)s_rs.scissorEnable);
        DebugOutputString(l);

        /* [log 60] Solo si hubo: viewports que no cabian en el render target y
         * se corrigieron en el vertex shader, con el ultimo que se pidio. */
        if (s_vpFuera)
        {
            unsigned int k, n = (s_vpFueraDist < GXD3D_VPFUERA_MAX) ? s_vpFueraDist : GXD3D_VPFUERA_MAX;
            sprintf(l, "[GXD3D2] vpfuera=%u distintos=%u", s_vpFuera, s_vpFueraDist);
            DebugOutputString(l);
            for (k = 0; k < n; k++)
            {
                sprintf(l, "[GXD3D2]   pedido %d,%d %dx%d", (int)s_vpFueraPedido[k][0],
                        (int)s_vpFueraPedido[k][1], (int)s_vpFueraPedido[k][2],
                        (int)s_vpFueraPedido[k][3]);
                DebugOutputString(l);
            }
            s_vpFuera = 0;
            s_vpFueraDist = 0;
        }

        /* Solo si hubo: lotes dibujados sobre el depth image (GLideN64
         * ccff245f).  `l1` = profundidad = rojo; `l2` = ademas con Z_CMP contra
         * la copia de Z; `copias` = Resolve de Z hechos; `fallos` = no se pudo
         * crear la textura de la copia. */
        if (s_stRtz1 || s_stRtz2 || s_stDec || s_stZcFallos ||
            s_stZgGuardadas || s_stZgLotes || s_stZgFallos)
        {
            /* `dec` = lotes con el decal del N64 por shader (ISS64).  `zg` =
             * [Mario Golf] Z guardados por direccion (`guarda`) y lotes que los
             * copian al Z (`l3`). */
            sprintf(l, "[RTZ] lotes l1=%u l2=%u dec=%u | copias=%u fallos=%u | "
                       "zg guarda=%u l3=%u fallos=%u | hiz %s",
                    s_stRtz1, s_stRtz2, s_stDec, s_stZcCopias, s_stZcFallos,
                    s_stZgGuardadas, s_stZgLotes, s_stZgFallos,
                    s_hizSucio ? "apagada" : "normal");
            DebugOutputString(l);
            s_stRtz1 = s_stRtz2 = s_stDec = s_stZcCopias = 0;
            s_stZgGuardadas = s_stZgLotes = 0;
        }

        gxd3d_dbg_degen    = 0;
        gxd3d_dbg_scisclip = 0;
        gxd3d_dbg_vpz      = 0;
        /* ★ Y el minimo TAMBIEN se reinicia, que es lo que le faltaba: sin
         * esto seria el minimo historico desde el arranque y no diria nada
         * de la escena que se esta mirando. */
        gxd3d_dbg_scismin  = 0xFFFFFFFFu;
        gxd3d_dbg_scisminw = 0;
        gxd3d_dbg_scisminh = 0;

        /* ★ `draws`, `verts` y `lote_n` NO se reinician aqui: los reinicia el
         * bloque `[GXSH]` de abajo, que ahora corre siempre.  Estuvieron un
         * tiempo duplicados en los dos sitios, y eso es peor que faltar. */
    }
#endif /* !XBOX64_SONDAS */

    /* ★★★ EL REPARTO POR FASE, FUERA DEL INTERRUPTOR MAESTRO.
     *
     * Estos contadores SE ACUMULAN SIEMPRE -- son `+=` sueltos por lote y
     * por draw, repartidos por todo el fichero --, pero su volcado vivia
     * bajo `#if XBOX64_SONDAS`, que vale 0.  O sea que habia un perfilador
     * completo midiendo y tirando sus numeros a la basura, exactamente la
     * misma historia que ya paso con los once cubos de `gln64_prof.h`.
     *
     * Y es la medida que hacia falta: "los draws cuestan 28-36 us" no dice
     * si se van en estado, en texturas, en el combinador o en el
     * `DrawPrimitive`, y cada uno de esos cuatro tiene un arreglo distinto.
     *
     * Mismo criterio que la linea `[GXD3D]` de arriba: son contadores POR
     * LOTE (~5.000/s), no por vertice ni por triangulo, asi que medirlos no
     * cambia lo que se mide. */
    {
    static LONGLONG frec = 0;
    char linea[320];
    unsigned lote_us, draw_us, vert_us;

    /* ★ Se fotografian los contadores ANTES de formatear.  Son `volatile`, o
     * sea que cada mencion es una lectura de memoria: leerlos dos veces (una
     * para la cuenta y otra para el divisor) daria una razon calculada con
     * dos valores distintos.  Aqui no hay concurrencia -- esto corre en el
     * hilo de emulacion, que es el que los incrementa-- pero una razon que
     * mezcla dos instantes es un numero equivocado sin aviso, y este informe
     * existe justo para no tener que dudar de sus numeros. */
    const unsigned f_lote_tb = gxsh_lote_tb,   f_lote_n = gxsh_lote_n;
    const unsigned f_draw_tb = gxsh_draw_tb;
    const unsigned f_bind_tb = gxsh_d_bind_tb, f_mtx_tb = gxsh_d_mtx_tb;
    const unsigned f_tev_tb  = gxsh_d_tev_tb,  f_prim_tb = gxsh_d_prim_tb;
    const unsigned f_tex_tb  = gxsh_tex_tb,    f_conv_tb = gxsh_conv_tb;
    const unsigned f_est_tb  = gxsh_state_tb;
    const unsigned f_lock_tb = gxsh_lock_tb,  f_lock_n = gxsh_lock_n;
    const unsigned f_merged  = gxsh_merged;
    const unsigned f_draws   = gxd3d_dbg_draws, f_verts = gxd3d_dbg_verts;
    const unsigned f_vbdrop  = gxd3d_dbg_vbdrop;
    const unsigned f_shgen   = gxd3d_dbg_shgen, f_shhit = gxd3d_dbg_shhit;
    const unsigned f_shovf   = gxd3d_dbg_shovf;

    if (frames == 0) frames = 1;
    if (frec == 0)
    {
        LARGE_INTEGER f;
        if (QueryPerformanceFrequency(&f) && f.QuadPart) frec = f.QuadPart;
        else                                             frec = 49875000;
    }

    /* us por frame.  tb como mucho ~50M ticks: 50M * 1e6 = 5e13, cabe. */
    #define GXSH_USF(tb)   ((unsigned)((((LONGLONG)(tb) * 1000000LL) / frec) / frames))
    /* ns por evento.  50M * 1e9 = 5e16, tambien cabe en 64 bits con signo. */
    #define GXSH_NS(tb,n)  ((unsigned)((n) ? ((((LONGLONG)(tb) * 1000000000LL) / frec) / (LONGLONG)(n)) : 0))

    lote_us = GXSH_USF(f_lote_tb);
    draw_us = GXSH_USF(f_draw_tb);
    vert_us = (lote_us > draw_us) ? (lote_us - draw_us) : 0;

    /* ★ CUATRO LINEAS CORTAS Y NO DOS LARGAS: el embudo del log corta a 127
     * caracteres (ver `xbox64_log_line`).  Ahora trocea en vez de tirar, pero
     * una linea que cabe entera se lee mucho mejor que una partida. */
    sprintf(linea,
            "[GXSH] ns/lote: lote=%u draw=%u tev=%u | ns/draw: prim=%u (Rice 740)",
            GXSH_NS(f_lote_tb, f_lote_n),
            GXSH_NS(f_draw_tb, f_lote_n),
            GXSH_NS(f_tev_tb,  f_lote_n),
            GXSH_NS(f_prim_tb, f_draws));
    DebugOutputString(linea);

    sprintf(linea,
            "[GXSH] us/f: lote=%u vert=%u draw=%u | bind=%u mtx=%u tev=%u prim=%u",
            lote_us, vert_us, draw_us,
            GXSH_USF(f_bind_tb), GXSH_USF(f_mtx_tb),
            GXSH_USF(f_tev_tb),  GXSH_USF(f_prim_tb));
    DebugOutputString(linea);

    sprintf(linea,
            "[GXSH] us/f: tex=%u conv=%u estado=%u | lock=%u (%u/s, %u ns)",
            GXSH_USF(f_tex_tb), GXSH_USF(f_conv_tb), GXSH_USF(f_est_tb),
            GXSH_USF(f_lock_tb), f_lock_n, GXSH_NS(f_lock_tb, f_lock_n));
    DebugOutputString(linea);

    /* ★ SIN `%%`: el `%` literal NO SOBREVIVE al volcado.  Medido en el log
     * 36: `fusion=%u%% verts=` salio como `fusion=0verts=`, o sea que algo
     * aguas abajo de `OutputDebugStringA` vuelve a pasar la cadena por un
     * printf y se come el `% `.  Se da la fusion como fraccion y listo. */
    sprintf(linea,
            "[GXSH] lotes=%u/s draws=%u/s fusion=%u de %u verts=%u/s "
            "vbdrop=%u | sh gen=%u hit=%u ovf=%u",
            f_lote_n, f_draws, f_merged, f_lote_n,
            f_verts, f_vbdrop,
            f_shgen, f_shhit, f_shovf);
    DebugOutputString(linea);

    /* ★ La niebla.  Ver la nota en gx_d3d9_tev.cpp. */
    sprintf(linea, "[GXFOG] on=%u off=%u col=%06X ini=%.1f fin=%.1f",
            gxd3d_dbg_fogon, gxd3d_dbg_fogoff, gxd3d_dbg_fogcol,
            (double)gxd3d_dbg_fogini, (double)gxd3d_dbg_fogfin);
    DebugOutputString(linea);
    gxd3d_dbg_fogon = gxd3d_dbg_fogoff = 0;

    #undef GXSH_USF
    #undef GXSH_NS

    /* Se reinicia todo lo que es POR SEGUNDO.  `shgen` no: es el numero de
     * shaders que hay en la cache, y ahi vale como absoluto. */
    gxsh_lote_tb    = 0;  gxsh_lote_n   = 0;
    gxsh_draw_tb    = 0;
    gxsh_tex_tb     = 0;  gxsh_tex_n    = 0;
    gxsh_conv_tb    = 0;  gxsh_state_tb = 0;
    gxsh_lock_tb    = 0;  gxsh_lock_n   = 0;
    gxsh_d_bind_tb  = 0;  gxsh_d_mtx_tb = 0;
    gxsh_d_tev_tb   = 0;  gxsh_d_prim_tb = 0;
    gxsh_merged     = 0;
    gxd3d_dbg_draws = 0;  gxd3d_dbg_verts = 0;  gxd3d_dbg_vbdrop = 0;
    gxd3d_dbg_shhit = 0;  gxd3d_dbg_shovf = 0;
    }
#else
    (void)frames;
#endif /* XBOX64_TRAZAS */
}

void GX_Begin(u8 primitive, u8 vtxfmt, u16 nverts)
{
    (void)vtxfmt;
    if (!s_vbData) gxd3d_lock_next_vb();

    /* Si el lote no cabe, cerrar el buffer y pasar al siguiente del anillo.
     * ★ NUNCA exit(): en consola es un reinicio.  Si ni siquiera cabe en un
     * buffer entero se descarta el lote y se cuenta, que es lo que hace hoy
     * d3d9_backend.cpp tras el susto del `exit(1)` de nextVertex(). */
    /* ★ Un quad ocupa SEIS ranuras, no cuatro: se parten en dos triangulos
     * al emitirlos.  Reservar por `nverts` a secas dejaria el lote corto y
     * se perderian los ultimos quads en silencio. */
    {
        int need = nverts;
#if GXD3D_QUADS_AS_TRIS
        if (primitive == GX_QUADS) need = (nverts / 4) * 6;
#endif
        if (s_vbUsed + need > GXD3D_MAX_VERTS)
        {
            /* [etapa 3] Antes: soltar el vertex buffer y bloquear el
             * siguiente.  Ahora el productor solo vacia lo pendiente y
             * vuelve al principio de su bufer de preparacion. */
            gxd3d_lock_next_vb();
            if (need > GXD3D_MAX_VERTS)
            {
                s_inBegin = 0;
                return;
            }
        }
    }

#if XBOX64_TRAZAS
    s_loteT0     = __mftb();
#endif
    s_inBegin    = 1;
    s_primType   = primitive;
    s_batchStart = s_vbUsed;
    s_batchCount = 0;
    s_curAttr    = 0;
#if GXD3D_QUADS_AS_TRIS
    /* Sin esto, un lote que se cortara a mitad de quad contaminaria el
     * siguiente con vertices del anterior. */
    s_quadFill   = 0;
#endif
    gxd3d_vertex_reset();
}

/* ★★ LOS QUADS SE PARTEN EN DOS TRIANGULOS AQUI, Y NO SE MANDAN COMO
 * `D3DPT_QUADLIST`.
 *
 * La primera version usaba QUADLIST razonando que Xenos lo tiene nativo y que
 * partir el quad interpola las UV con dos ecuaciones y mete un texel de
 * desfase en la diagonal.  El desfase es real -- se midio en fb_gln64 con las
 * tiras del fondo de pausa de OoT-- pero **el orden de vertices que espera
 * QUADLIST no esta documentado en el d3d9types.h del XDK** (solo aparece el
 * valor, `D3DPT_QUADLIST = 13`).  O sea que eso era una SUPOSICION, no la
 * referencia ([[feedback_referencia_que_funciona]]).
 *
 * Y el reparto de primitivas del plugin senala justo ahi:
 *
 *     GX_TRIANGLES  OGL_DrawTriangles (la geometria 3D)      -> se ve BIEN
 *     GX_QUADS      OGL_DrawRect (fillrect del N64, a
 *                   pantalla completa), OGL_DrawTexturedRect,
 *                   los quads de FrameBuffer y los de VI      -> corrupto
 *
 * Todo lo que va en espacio de pantalla es quad, y es justo la "capa a
 * pantalla completa encima de la escena" que se ve.  Dos triangulos
 * (0,1,2 + 0,2,3) es lo que GX_QUADS significa y no se puede malinterpretar.
 *
 * A 0 vuelve a QUADLIST para comparar.  (El define y el acumulador estan
 * declarados mas arriba, junto al resto del estado del vertice, porque
 * `GX_Begin` tambien los necesita.) */

/* Cierra el vertice en curso cuando ya se han visto todos sus atributos. */
static __inline void gxd3d_vertex_maybe_flush(void)
{
    int want = 1 + s_wantColor + s_wantTex;   /* posicion + color + UVs */
    if (s_curAttr < want) return;

#if GXD3D_QUADS_AS_TRIS
    if (s_primType == GX_QUADS)
    {
        /* Se acumulan los cuatro y se emiten SEIS.  El cuarto vertice de GX
         * cierra el poligono, asi que el reparto es (0,1,2) y (0,2,3): la
         * misma diagonal para los dos triangulos y el mismo sentido de giro
         * que traia el quad, que es lo que el culling espera. */
        s_quad[s_quadFill++] = s_v;
        if (s_quadFill == 4)
        {
            s_quadFill = 0;
            if (s_vbData && s_vbUsed + 6 <= GXD3D_MAX_VERTS)
            {
                s_vbData[s_vbUsed++] = s_quad[0];
                s_vbData[s_vbUsed++] = s_quad[1];
                s_vbData[s_vbUsed++] = s_quad[2];
                s_vbData[s_vbUsed++] = s_quad[0];
                s_vbData[s_vbUsed++] = s_quad[2];
                s_vbData[s_vbUsed++] = s_quad[3];
                s_batchCount += 6;
            }
#if XBOX64_TRAZAS
            else gxd3d_dbg_vbdrop += 6;   /* buffer lleno: quad perdido */
#endif
        }
    }
    else
#endif
    if (s_vbData && s_vbUsed < GXD3D_MAX_VERTS)
    {
        s_vbData[s_vbUsed++] = s_v;
        s_batchCount++;
    }
#if XBOX64_TRAZAS
    else gxd3d_dbg_vbdrop++;              /* buffer lleno: vertice perdido */
#endif
    s_curAttr = 0;
    gxd3d_vertex_reset();
}

/* -- ★ SONDA: hasta donde llegan las coordenadas dentro de un color image --
 *
 * La pregunta que contesta, y que no se puede contestar razonando: cuando el
 * juego dibuja en un buffer auxiliar de 128 px de ancho, ¿emite coordenadas
 * de 0..128 (las del panel) o de 0..320 (las de pantalla)?
 *
 * Si son de pantalla, el contenido es ~2,5 veces mas grande que el panel y
 * solo entra el 40% -- que es exactamente lo que se ve en los cuadros de
 * texto de Pokemon Stadium.  Si son del panel, la cadena esta bien y el
 * defecto esta en otro sitio.
 *
 * Aqui llegan TAL CUAL las que empuja el plugin (espacio N64): la ortho y el
 * viewport los aplica el vertex shader despues.  Lo pone en marcha
 * gxd3d_ci_actual(), desde el cambio de color image. */
extern "C" volatile float        gxd3d_ci_maxx = 0.0f;
extern "C" volatile float        gxd3d_ci_maxy = 0.0f;
extern "C" volatile unsigned int gxd3d_ci_w    = 0;

extern "C" void gxd3d_ci_actual(unsigned int width)
{
#if XBOX64_TRAZAS
    gxd3d_ci_w    = width;
    gxd3d_ci_maxx = 0.0f;
    gxd3d_ci_maxy = 0.0f;
#else
    (void)width;
#endif
}

void GX_Position3f32(f32 x, f32 y, f32 z)
{
    if (!s_inBegin) return;
#if XBOX64_TRAZAS   /* sonda por vertice */
    if (x > gxd3d_ci_maxx) gxd3d_ci_maxx = x;
    if (y > gxd3d_ci_maxy) gxd3d_ci_maxy = y;
#endif
    s_v.x = x; s_v.y = y; s_v.z = z;
    s_curAttr++;
    gxd3d_vertex_maybe_flush();
}

void GX_Position2f32(f32 x, f32 y)
{
    if (!s_inBegin) return;
    s_v.x = x; s_v.y = y; s_v.z = 0.0f;
    s_curAttr++;
    gxd3d_vertex_maybe_flush();
}

void GX_Color4u8(u8 r, u8 g, u8 b, u8 a)
{
    if (!s_inBegin) return;
    s_v.color = GXD3D_PACK_RGBA(r, g, b, a);
    s_curAttr++;
    gxd3d_vertex_maybe_flush();
}

void GX_TexCoord2f32(f32 s, f32 t)
{
    static int which = 0;
    if (!s_inBegin) return;
    /* Las UV llegan en orden TEX0, TEX1, TEX2 dentro del mismo vertice. */
    if (s_curAttr == 1 + s_wantColor)
    {
        s_v.s0 = s; s_v.t0 = t; which = 0;
#if GXD3D_UV_STATS
        /* Solo TEX0: es el que lleva la textura de la superficie. */
        {
            f32 m = (s < 0 ? -s : s);
            f32 n = (t < 0 ? -t : t);
            unsigned int v;
            if (n > m) m = n;
            v = (unsigned int)m;
            if (s_primType == GX_TRIANGLES)
            {
                if (v > gxd3d_dbg_uvtri) gxd3d_dbg_uvtri = v;
                if      (m <=    2.0f) gxd3d_dbg_uv0_2++;
                else if (m <=    8.0f) gxd3d_dbg_uv2_8++;
                else if (m <=   32.0f) gxd3d_dbg_uv8_32++;
                else if (m <= 1024.0f) gxd3d_dbg_uv32_1k++;
                else                   gxd3d_dbg_uvbig++;
            }
            else { if (v > gxd3d_dbg_uvrect) gxd3d_dbg_uvrect = v; }
        }
#endif
    }
    else if (s_curAttr == 2 + s_wantColor)     { s_v.s1 = s; s_v.t1 = t; which = 1; }
    else
    {
        /* ★ EL TERCER JUEGO DE COORDENADAS SE CONSUME Y SE TIRA.
         *
         * El vertice ya no lleva `s2/t2` (ver xenon/gxd3d_lote.h): valia
         * SIEMPRE (0,0) en los tres unicos sitios que declaran TEX2 --
         * OGL_DrawTriangles, OGL_DrawTexturedRect y ZSortBOSS --, y el pixel
         * shader lo sigue recibiendo porque lo escribe el vertex shader como
         * literal.
         *
         * Hay que CONSUMIR el atributo igual (el `s_curAttr++` de abajo) o el
         * recuento de `gxd3d_vertex_maybe_flush` se desalinea y los vertices
         * se cierran donde no toca. */
        which = 2;
    }
    (void)which;
    s_curAttr++;
    gxd3d_vertex_maybe_flush();
}

void GX_End(void)
{
    D3DPRIMITIVETYPE pt;
    UINT nprim;

    if (!s_inBegin) return;
    s_inBegin = 0;
    if (s_batchCount == 0) return;

    switch (s_primType)
    {
        case GX_TRIANGLES:
            pt = D3DPT_TRIANGLELIST;
            nprim = s_batchCount / 3;
            break;
        case GX_QUADS:
#if GXD3D_QUADS_AS_TRIS
            /* Ya vienen partidos en dos triangulos desde
             * gxd3d_vertex_maybe_flush; ver la nota larga de alli. */
            pt = D3DPT_TRIANGLELIST;
            nprim = s_batchCount / 3;
#else
            pt = D3DPT_QUADLIST;
            nprim = s_batchCount / 4;
#endif
            break;
        case GX_LINES:
            pt = D3DPT_LINELIST;
            nprim = s_batchCount / 2;
            break;
        default:
            return;
    }
    if (nprim == 0) return;

    /* ★★ NI Unlock NI Lock AQUI.  SE DIBUJA DESDE EL BUFFER BLOQUEADO.
     *
     * La primera version hacia `d3dq_sinc(); Unlock(); ... DrawPrimitive();
     * Lock();` en CADA lote, con el razonamiento de que "el buffer tiene que
     * estar desbloqueado para dibujar".  En este hardware eso es FALSO, y la
     * referencia que funciona lo demuestra: `d3d9_backend.cpp` hace `Lock`
     * UNA VEZ por display list (linea 1188, junto al BeginScene) y `Unlock`
     * una vez al final del frame (linea 2164), y dibuja miles de veces con
     * el buffer bloqueado.  Su propio comentario lo dice: *"20 veces por
     * segundo contra 4.491 dibujos, asi que no rompe el batching"*.
     *
     * Medido en Conker/OoT con la version de antes: 18.324 draws/s, o sea
     * **18.324 pares Unlock+Lock por segundo contra los ~20 de Rice**, con
     * `[PERF] gfx=557ms/s` y `gfxCPU=16,89ms` de un presupuesto de 16,67.
     *
     * Es seguro porque la escritura es SOLO HACIA DELANTE dentro del frame
     * (`s_vbUsed` nunca retrocede) y el anillo de 3 buffers avanza por
     * FRAME: la GPU nunca lee la region que se esta escribiendo.  Es la
     * misma disciplina de Rice. */

    /* ★ Estado que antes se reafirmaba en CADA draw y ahora solo cuando
     * cambia.  Reafirmarlo por draw no era paranoia gratuita -- el device
     * esta COMPARTIDO con el frontend-- pero el frontend solo puede
     * robarlo ENTRE frames, asi que basta con reafirmar una vez por pasada
     * (lo hace `gxd3d_frame_begin`).  Es lo mismo que hace Rice con
     * `CaducarEspejo()`, "una vez por display list". */
    {
#if XBOX64_TRAZAS
        const unsigned long long tDraw = __mftb();
#endif

        /* ★ Si el lote pendiente es del mismo tipo, CONTIGUO en el buffer y
         * con el MISMO COMBINADOR, se extiende en vez de dibujar.
         *
         * Del estado que se fija con setters (render states, matrices,
         * texturas, scissor, viewport) no hace falta preguntar: todos vacian
         * el lote antes de mutar.  El combinador NO lo hacia -- sus setters
         * solo marcan sucio-- y por eso se fusionaban lotes con shaders
         * distintos.  `gxd3d_tev_difiere()` resuelve la clave y la compara
         * con la del shader ligado, de modo que las reescrituras con los
         * mismos valores -- que glN64 hace antes de casi cada lote-- siguen
         * fusionando. */
        (void)nprim;
        if (GXD3D_LOTES && s_pendActive && s_pendPrim == pt &&
            s_pendStart + s_pendCount == s_batchStart &&
            !gxd3d_tev_difiere())
        {
            s_pendCount += s_batchCount;
#if XBOX64_TRAZAS
            gxsh_merged++;
#endif
        }
        else
        {
#if XBOX64_TRAZAS
            unsigned long long tq;
#endif

            /* Primero el lote viejo, con el estado que todavia esta ligado
             * (que es el suyo: ver la nota de gxd3d_flush_pending). */
            gxd3d_flush_pending();

            /* Y ahora se liga el del lote nuevo. */
#if XBOX64_TRAZAS   /* cronometros de [GXSH] */
            tq = __mftb();
            gxd3d_upload_matrices();
            gxsh_d_mtx_tb += (unsigned int)(__mftb() - tq); tq = __mftb();
            gxd3d_tev_apply();
            gxsh_d_tev_tb += (unsigned int)(__mftb() - tq);
#else
            gxd3d_upload_matrices();
            gxd3d_tev_apply();
#endif

            /* Dibujo sobre el depth image: ver GXD3D_SetRenderToDepth. */
            if (s_rtzModo == 2)
            {
                gxd3d_zcopia_liga();
#if XBOX64_TRAZAS
                s_stRtz2++;
#endif
            }
#if XBOX64_TRAZAS
            else if (s_rtzModo == 1)
                s_stRtz1++;
#endif
            /* El decal del N64 por shader: ver GXD3D_SetDecalN64. */
            gxd3d_zcopia_lote();

            s_pendPrim   = pt;
            s_pendStart  = s_batchStart;
            s_pendCount  = s_batchCount;
            s_pendActive = 1;
        }

#if !GXD3D_LOTES
        /* Sin fusion: se dibuja en el acto, con el estado que se acaba de
         * ligar arriba.  Asi ningun setter puede alcanzar a un lote a medias,
         * que es la clase de fallo que este toggle esta descartando. */
        gxd3d_flush_pending();
#endif

#if XBOX64_TRAZAS   /* censo y cronometros por lote: solo los lee el log */
        gxd3d_dbg_verts += s_batchCount;
        if (s_texBound[0]) gxd3d_dbg_drawtex++; else gxd3d_dbg_drawnotex++;

        /* ★ DRAWS CON TEXTURA LIGADA QUE EL SHADER IGNORA.
         *
         * El generador de HLSL sustituye el muestreo por BLANCO cuando la
         * etapa no tiene texmap o cuando no hay texgens:
         *
         *     if (texmap == GX_TEXMAP_NULL || numTexGens == 0)
         *         texc = float4(1,1,1,1);        (gx_d3d9_tev.cpp:441)
         *
         * O sea que un draw puede tener su textura perfectamente ligada en el
         * sampler -- y contar como `contex` -- y aun asi salir BLANCO porque
         * el shader ni la toca.  `sintex` no lo ve: mide el sampler, no el
         * shader.
         *
         * Es el sospechoso de los modelos blancos de Pokemon Stadium en
         * combate.  Si `texnogen` es grande ahi, el defecto esta en quien
         * deja `numTexGens` a 0 y no en la textura ni en el combinador. */
        if (s_texBound[0] && g_gxTev.numTexGens == 0)
            gxd3d_dbg_texnogen++;

        gxsh_draw_tb += (unsigned int)(__mftb() - tDraw);
        gxsh_lote_tb += (unsigned int)(__mftb() - s_loteT0);
        gxsh_lote_n++;
#endif /* XBOX64_TRAZAS */
    }
}

/* ------------------------------------------------------------------ */
/* ★★ LOTE ENTERO: los vertices los escribe el PLUGIN                  */
/* ------------------------------------------------------------------ */
/* El porque completo (y las medidas) estan en xenon/gxd3d_lote.h.  Aqui solo
 * la mecanica, que se apoya ENTERA en `GX_Begin` y `GX_End`:
 *
 *   - `abrir`  = la reserva de `GX_Begin` (rotacion del anillo, tope del
 *                buffer, reinicio de contadores, reloj del lote) y devolver
 *                donde escribir.
 *   - `cerrar` = decirle a `GX_End` cuantos vertices hay y dejar que haga lo
 *                de siempre: fusionar con el lote pendiente o ligar estado y
 *                dibujar.
 *
 * ★ Ni un punto de vaciado nuevo, ni uno menos.  Es un cambio de TRANSPORTE:
 * los mismos vertices, en el mismo orden, en el mismo sitio del mismo vertex
 * buffer, con el mismo estado.  Esa es la unica forma de tocar esta pieza sin
 * repetir los cuatro defectos que ya ha dado
 * ([[reference_gxd3d_estado_sin_vaciar_lote]]). */
GXD3DVertex *gxd3d_lote_abrir(int nverts)
{
    if (nverts <= 0) return NULL;

    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, (u16)nverts);

    /* `GX_Begin` se rinde (dejando `s_inBegin = 0`) cuando el lote no cabe ni
     * en un vertex buffer entero.  Con 255 vertices como tope del plugin y
     * 49.152 de capacidad no puede pasar, pero es un camino de perdida
     * SILENCIOSA de geometria y por eso se cuenta igual que los otros dos. */
    if (!s_inBegin || !s_vbData)
    {
        s_inBegin = 0;
#if XBOX64_TRAZAS
        gxd3d_dbg_vbdrop += (unsigned int)nverts;
#endif
        return NULL;
    }
    return &s_vbData[s_batchStart];
}

void gxd3d_lote_cerrar(int nverts)
{
    if (!s_inBegin) return;
    if (nverts > 0)
    {
        s_vbUsed     += nverts;
        s_batchCount += nverts;
    }
    GX_End();
}

/* Emite de verdad el lote acumulado.  TODO lo que cambie el estado de
 * dibujo tiene que llamar a esto ANTES de cambiarlo. */
/* ★★ EL BORRADO DE PANTALLA, CON EL Clear() NATIVO DE D3D.
 *
 * POR QUE.  glN64 borra la pantalla dibujando un quad a pantalla completa
 * (OGL_GXclearEFB, OpenGL.cpp:2229) porque en GX no hay otra cosa.  En D3D SI
 * la hay, y traducir el modismo a `Clear()` es la traduccion CORRECTA, no un
 * atajo: la funcion ya desactiva el recorte y cubre el render target entero,
 * que es exactamente lo que `Clear(NULL)` hace.
 *
 * EL SINTOMA que lo destapo: en la intro de GoldenEye la pantalla no se
 * borraba y el logo y los creditos se quedaban pegados frame tras frame.
 *
 * ★ Y POR QUE NO SE SIGUIO BUSCANDO EL FALLO DEL QUAD.  Porque se descarto
 * con numeros todo lo que se podia mirar sin reescribirlo, y salio sano:
 *
 *   - no era restaurar un framebuffer viejo: el contador de restauraciones
 *     saltadas por marcador roto salia 0
 *   - el borrado SI se reconoce (el rect casa con VI.width x VI.height) y SI
 *     se ejecuta, las dos cosas a 60/s
 *   - no era el color: forzando el quad a magenta no aparecia magenta
 *   - el estado D3D del draw estaba bien: nprim=2 verts=6, mascara de color
 *     completa, sin mezcla, viewport 0,0 640x480 z=0..1, y el test de
 *     profundidad en GEQUAL sin escritura
 *   - el vertice se compone bien (posicion + color, cuatro acumulados y seis
 *     emitidos) y GXmodelViewIdent se inicializa en cada display list
 *
 * Lo que quedaba por mirar -- el lote diferido, el shader generado y los
 * datos ya en el vertex buffer-- son tres sitios mas, y el Clear() nativo no
 * depende de ninguno.  Comprobado en GoldenEye, Paper Mario, Zelda OoT y
 * Pokemon Stadium.
 *
 * ★ EL VACIADO VA PRIMERO Y NO ES OPCIONAL: el lote pendiente es geometria
 * ANTERIOR en el tiempo al borrado, asi que tiene que dibujarse antes o el
 * Clear se la come. */
extern "C" void gxd3d_clear_efb(int color, int depth, u8 r, u8 g, u8 b)
{
    DWORD flags = 0;
    if (color) flags |= D3DCLEAR_TARGET;
    if (depth) flags |= D3DCLEAR_ZBUFFER;
    if (!flags || !g_pd3dDevice) return;

    /* Los contadores buenos del borrado.  `clrrgb` guarda el ULTIMO color,
     * que es lo que hay que mirar cuando aparece una capa de color sobre la
     * escena: si la capa es azulada y aqui sale un azul, ya no hay que
     * buscar mas lejos. */
#if XBOX64_TRAZAS
    if (color) { gxd3d_dbg_clrcol++;
                 gxd3d_dbg_clrrgb = ((unsigned int)r << 16) |
                                    ((unsigned int)g << 8)  | (unsigned int)b; }
    if (depth) gxd3d_dbg_clrz++;
#endif

    gxd3d_flush_pending();

    if (depth)
        gxd3d_hiz_borrado_z();   /* ver GXD3D_SetRenderToDepth */
    g_pd3dDevice->Clear(0, NULL, flags, D3DCOLOR_ARGB(255, r, g, b), 1.0f, 0);
}

static void gxd3d_flush_pending(void)
{
    UINT nprim;
#if XBOX64_TRAZAS
    unsigned long long tq;
#endif

    if (!s_pendActive || s_pendCount == 0) { s_pendActive = 0; s_pendCount = 0; return; }

    switch (s_pendPrim)
    {
        case D3DPT_TRIANGLELIST: nprim = (UINT)(s_pendCount / 3); break;
        case D3DPT_LINELIST:     nprim = (UINT)(s_pendCount / 2); break;
        case D3DPT_QUADLIST:     nprim = (UINT)(s_pendCount / 4); break;
        default:                 nprim = 0;                       break;
    }
    if (nprim == 0) { s_pendActive = 0; s_pendCount = 0; return; }

#if XBOX64_TRAZAS
    tq = __mftb();
#endif
    if (!s_bindValid)
    {
        g_pd3dDevice->SetVertexShader(s_vs);
        g_pd3dDevice->SetVertexDeclaration(s_decl);
        /* [etapa 3] Sin `SetStreamSource`: el vertex buffer es del
         * consumidor y lo liga el (`gxd3d_consumidor_dibuja`). */
        s_bindValid = 1;
    }
#if XBOX64_TRAZAS
    gxsh_d_bind_tb += (unsigned int)(__mftb() - tq); tq = __mftb();
#endif

    /* ★★ AQUI YA NO SE APLICA ESTADO.  ESTA FUNCION DIBUJA Y NADA MAS.
     *
     * Antes llamaba a `gxd3d_upload_matrices()` y `gxd3d_tev_apply()` justo
     * antes del DrawPrimitive, y eso era un fallo de dibujo latente: el lote
     * pendiente se dibujaba con el estado VIGENTE EN EL MOMENTO DEL VACIADO,
     * no con el suyo.
     *
     *   - matrices: `GX_LoadPosMtxImm` hace `memcpy(s_modelview, nm)` y SOLO
     *     DESPUES llama al vaciado, asi que el lote viejo se subia con la
     *     matriz NUEVA.
     *   - combinador: ningun `GX_SetTev*` vaciaba nada, asi que el lote se
     *     dibujaba con el shader de quien viniera detras.
     *
     * No daba la cara porque un cambio de combinador casi siempre viene con
     * un cambio de textura o de mezcla, y esos SI vaciaban antes de mutar.
     *
     * Ahora el estado se aplica al ARRANCAR un lote (ver el camino de abajo),
     * de modo que lo que hay ligado es por construccion lo del lote
     * pendiente y vaciar desde cualquier sitio es siempre correcto. */
#if XBOX64_TRAZAS
    if (g_gxTev.ztOp == GX_ZT_REPLACE) gxd3d_dbg_ztex++;
#endif

    /* ¿Este draw sale con la pantalla recortada?  Ver gxd3d_dbg_scisclip.
     * Solo con trazas: es censo.  (El aislador GXD3D_DIAG_SALTA_SCIS de
     * dentro, que esta a 0, tambien necesita XBOX64_TRAZAS.) */
#if XBOX64_TRAZAS
    {
        unsigned int aw = (unsigned int)(s_rs.scissor.right  - s_rs.scissor.left);
        unsigned int ah = (unsigned int)(s_rs.scissor.bottom - s_rs.scissor.top);
        unsigned int a  = aw * ah;
        if (s_rs.scissor.left   > 0 || s_rs.scissor.top    > 0 ||
            aw < (unsigned int)xenon64_video_width() ||
            ah < (unsigned int)xenon64_video_height())
        {
            gxd3d_dbg_scisclip++;
            /* ★ Se guardan las DIMENSIONES del mas pequenyo, no solo su
             * area: "el recorte minimo mide 320x240" es una respuesta y
             * "mide 76800" hay que factorizarlo a mano. */
            if (a < gxd3d_dbg_scismin)
            {
                gxd3d_dbg_scismin  = a;
                gxd3d_dbg_scisminw = aw;
                gxd3d_dbg_scisminh = ah;
            }

            /* ★★★ AISLADOR: SALTARSE LOS DRAWS RECORTADOS A MEDIA PANTALLA.
             *
             * Los modos de `marcatex` sustituyen el shader de TODOS los
             * draws, asi que sirven para preguntar "¿que sale de este
             * combinador?" pero NO para aislar una region: con ellos la
             * pantalla entera se vuelve blanca o negra y el cuadrante no se
             * distingue del resto.  Eso ya costo dos pruebas que no
             * contestaban lo que yo dije que contestaban.
             *
             * Esto es lo contrario: no toca el shader, no toca el color, y
             * solo quita de en medio los draws cuyo RECORTE es de media
             * pantalla o menos -- que es la unica propiedad MEDIDA que
             * tenemos del cuadro negro (`scismin=320x240`, 15 muestras de
             * 18 en Majora's Mask).
             *
             *   el cuadro negro DESAPARECE -> lo dibuja uno de esos draws.
             *       Son el paso de render-to-texture, y el siguiente sitio
             *       donde mirar es por que su buffer sale vacio.
             *   el cuadro negro SIGUE      -> no lo dibuja ninguno.  El
             *       recorte queda descartado de verdad esta vez, y hay que
             *       buscar un quad a pantalla completa que salga negro.
             *
             * ★ Se salta el dibujo pero NO la contabilidad del lote: el
             * `s_pendActive`/`s_pendCount` se limpian igual mas abajo, o el
             * siguiente lote heredaria vertices de este.
             *
             * ★★★ NO ES SEGURA: A 1 REVIENTA, Y CONTESTO ANTES DE HACERLO.
             *
             * MEDIDO (2026-09-29): con esto a 1 el cuadro negro de Majora's
             * Mask DESAPARECE -- que era la pregunta, y la respuesta vale--
             * pero al rato salta una violacion de acceso en el FRONTEND,
             * dentro de `SDL_BlitSurface` del overlay de FPS
             * (`GameMenu::updateFps`, `Blit1to4`).  Pasa con los dos
             * plugins, asi que no es del camino de Rice.
             *
             * No se ha encontrado el mecanismo y NO se va a buscar: es una
             * sonda de usar y tirar.  Lo que hay que saber es que saltarse
             * el `DrawPrimitive` a mitad de la tuberia deja al shim en un
             * estado que el resto no espera, y esta es la QUINTA vez que
             * este fichero castiga tocar el lote por su cuenta
             * ([[reference_gxd3d_estado_sin_vaciar_lote]]).
             *
             * Si hace falta volver a mirar los recortes, usar
             * `GXD3D_DIAG_SCIS` (arriba), que solo IMPRIME y no puede
             * romper nada.
             *
             * A 0 el binario es el de siempre.  DEJARLO A 0. */
#define GXD3D_DIAG_SALTA_SCIS 0
#if GXD3D_DIAG_SALTA_SCIS
            if (aw * 2 <= (unsigned int)xenon64_video_width() &&
                ah * 2 <= (unsigned int)xenon64_video_height())
            {
                s_pendActive = 0;
                s_pendCount  = 0;
                return;
            }
#endif
        }
    }
#endif /* XBOX64_TRAZAS (scisclip) */

    /* [etapa 3] Los vertices del lote se copian al pool y el consumidor los
     * dibuja desde SU vertex buffer con el mismo `DrawPrimitive` de antes.
     * Con el grabador a 0 no hay pool: se llama al consumidor en el acto,
     * que es exactamente la misma ejecucion. */
#if D3D_GRABADOR
    g_pd3dDevice->DibujaVerts((unsigned int)s_pendPrim, (unsigned int)nprim,
                              &s_vbData[s_pendStart], (unsigned int)s_pendCount,
                              (unsigned int)sizeof(GXD3DVertex));
#else
    gxd3d_consumidor_dibuja(g_pd3dDevice, (unsigned int)s_pendPrim,
                            (unsigned int)nprim, &s_vbData[s_pendStart],
                            (unsigned int)s_pendCount,
                            (unsigned int)sizeof(GXD3DVertex));
#endif
#if XBOX64_TRAZAS
    gxsh_d_prim_tb += (unsigned int)(__mftb() - tq);

    gxd3d_dbg_draws++;
#endif
    s_pendActive = 0;
    s_pendCount  = 0;
}

/* ------------------------------------------------------------------ */
/* Texturas                                                            */
/* ------------------------------------------------------------------ */
/* glN64_GX entrega los texels ya EMPAQUETADOS en formato de GameCube, que es
 * tileado: bloques de 4x4 (16 y 32 bpp), 8x4 (8 bpp) u 8x8 (4 bpp), en orden
 * fila-mayor dentro del bloque y bloque-mayor dentro de la imagen.  Xenos
 * tiene SU PROPIO tileado, distinto, asi que hay que destilar a lineal y
 * dejar que D3D lo re-tile al escribir.
 *
 * Es el mismo trabajo que ya se hizo al reves en fb_gln64.cpp, donde la
 * formula de destileado de Xenos se dedujo del dato y no de la documentacion
 * (la que "recordaba" estaba mal).  Aqui la de GameCube si esta bien
 * documentada y es mecanica. */

static void gxd3d_untile_i4(const u8 *src, u32 *dst, int w, int h)
{
    int bx, by, y, x;
    for (by = 0; by < h; by += 8)
        for (bx = 0; bx < w; bx += 8)
            for (y = 0; y < 8; y++)
                for (x = 0; x < 8; x += 2)
                {
                    u8 b = *src++;
                    int i0 = (b >> 4) & 0xF, i1 = b & 0xF;
                    u8 c0 = (u8)(i0 * 17), c1 = (u8)(i1 * 17);
                    if (by + y < h)
                    {
                        if (bx + x     < w) dst[(by+y)*w + bx+x]     = (c0<<24)|(c0<<16)|(c0<<8)|c0;
                        if (bx + x + 1 < w) dst[(by+y)*w + bx+x+1]   = (c1<<24)|(c1<<16)|(c1<<8)|c1;
                    }
                }
}

static void gxd3d_untile_ia4(const u8 *src, u32 *dst, int w, int h)
{
    int bx, by, y, x;
    for (by = 0; by < h; by += 4)
        for (bx = 0; bx < w; bx += 8)
            for (y = 0; y < 4; y++)
                for (x = 0; x < 8; x++)
                {
                    u8 b = *src++;
                    u8 a = (u8)(((b >> 4) & 0xF) * 17);
                    u8 i = (u8)((b & 0xF) * 17);
                    if (by + y < h && bx + x < w)
                        dst[(by+y)*w + bx+x] = ((u32)a<<24)|((u32)i<<16)|((u32)i<<8)|i;
                }
}

static void gxd3d_untile_ia8(const u8 *src, u32 *dst, int w, int h)
{
    int bx, by, y, x;
    for (by = 0; by < h; by += 4)
        for (bx = 0; bx < w; bx += 4)
            for (y = 0; y < 4; y++)
                for (x = 0; x < 4; x++)
                {
                    /* GameCube es big-endian igual que nosotros: el halfword
                     * se lee tal cual, sin swap. */
                    u16 v = (u16)((src[0] << 8) | src[1]); src += 2;
                    u8 a = (u8)(v >> 8), i = (u8)(v & 0xFF);
                    if (by + y < h && bx + x < w)
                        dst[(by+y)*w + bx+x] = ((u32)a<<24)|((u32)i<<16)|((u32)i<<8)|i;
                }
}

static void gxd3d_untile_rgb565(const u8 *src, u32 *dst, int w, int h)
{
    int bx, by, y, x;
    for (by = 0; by < h; by += 4)
        for (bx = 0; bx < w; bx += 4)
            for (y = 0; y < 4; y++)
                for (x = 0; x < 4; x++)
                {
                    u16 v = (u16)((src[0] << 8) | src[1]); src += 2;
                    u8 r = (u8)(((v >> 11) & 0x1F) * 255 / 31);
                    u8 g = (u8)(((v >>  5) & 0x3F) * 255 / 63);
                    u8 b = (u8)(( v        & 0x1F) * 255 / 31);
                    if (by + y < h && bx + x < w)
                        dst[(by+y)*w + bx+x] = 0xFF000000u|((u32)r<<16)|((u32)g<<8)|b;
                }
}

static void gxd3d_untile_rgb5a3(const u8 *src, u32 *dst, int w, int h)
{
    int bx, by, y, x;
    for (by = 0; by < h; by += 4)
        for (bx = 0; bx < w; bx += 4)
            for (y = 0; y < 4; y++)
                for (x = 0; x < 4; x++)
                {
                    u16 v = (u16)((src[0] << 8) | src[1]); src += 2;
                    u8 r, g, b, a;
                    if (v & 0x8000)
                    {   /* 0RRRRRGGGGGBBBBB, alfa opaco */
                        a = 255;
                        r = (u8)(((v >> 10) & 0x1F) * 255 / 31);
                        g = (u8)(((v >>  5) & 0x1F) * 255 / 31);
                        b = (u8)(( v        & 0x1F) * 255 / 31);
                    }
                    else
                    {   /* 0AAARRRRGGGGBBBB */
                        a = (u8)(((v >> 12) & 0x7) * 255 / 7);
                        r = (u8)(((v >>  8) & 0xF) * 17);
                        g = (u8)(((v >>  4) & 0xF) * 17);
                        b = (u8)(( v        & 0xF) * 17);
                    }
                    if (by + y < h && bx + x < w)
                        dst[(by+y)*w + bx+x] = ((u32)a<<24)|((u32)r<<16)|((u32)g<<8)|b;
                }
}

static void gxd3d_untile_rgba8(const u8 *src, u32 *dst, int w, int h)
{
    /* RGBA8 de GameCube: bloques de 4x4 pero PARTIDOS en dos mitades de 32
     * bytes: primero los pares AR de los 16 texels, luego los GB.  Es el
     * unico formato con esa rareza y el que mas se equivoca al portar. */
    int bx, by, y, x;
    for (by = 0; by < h; by += 4)
        for (bx = 0; bx < w; bx += 4)
        {
            const u8 *ar = src;
            const u8 *gb = src + 32;
            for (y = 0; y < 4; y++)
                for (x = 0; x < 4; x++)
                {
                    u8 a = *ar++, r = *ar++;
                    u8 g = *gb++, b = *gb++;
                    if (by + y < h && bx + x < w)
                        dst[(by+y)*w + bx+x] = ((u32)a<<24)|((u32)r<<16)|((u32)g<<8)|b;
                }
            src += 64;
        }
}

/* [Xbox360] Atlas de mipmap (Textures.cpp, TextureCache_LoadAtlas): el plugin
 * necesita los texels de cada nivel en ARGB lineal para juntarlos en el atlas.
 * `w` y `h` son los RELLENADOS al bloque (GXrealWidth/GXrealHeight), y `dst`
 * mide w*h.  Mismos decodificadores que la subida de GX_LoadTexObj. */
void GXD3D_DecodificaLineal(const void *gx, u8 fmt, int w, int h, u32 *dst)
{
    const u8 *src = (const u8 *)gx;
    switch (fmt)
    {
        case GX_TF_I4:     gxd3d_untile_i4(src, dst, w, h);     break;
        case GX_TF_IA4:    gxd3d_untile_ia4(src, dst, w, h);    break;
        case GX_TF_IA8:    gxd3d_untile_ia8(src, dst, w, h);    break;
        case GX_TF_RGB565: gxd3d_untile_rgb565(src, dst, w, h); break;
        case GX_TF_RGB5A3: gxd3d_untile_rgb5a3(src, dst, w, h); break;
        case GX_TF_RGBA8:  gxd3d_untile_rgba8(src, dst, w, h);  break;
        default:           memset(dst, 0, (size_t)w * h * 4);   break;
    }
}

/* Cuantos bytes ocupa una textura GX de wxh en ese formato. */
static u32 gxd3d_tex_bytes(int w, int h, u8 fmt)
{
    int bw, bh, bpp;
    switch (fmt)
    {
        case GX_TF_I4:     bw = 8; bh = 8; bpp = 4;  break;
        case GX_TF_I8:     bw = 8; bh = 4; bpp = 8;  break;
        case GX_TF_IA4:    bw = 8; bh = 4; bpp = 8;  break;
        case GX_TF_IA8:    bw = 4; bh = 4; bpp = 16; break;
        case GX_TF_RGB565: bw = 4; bh = 4; bpp = 16; break;
        case GX_TF_RGB5A3: bw = 4; bh = 4; bpp = 16; break;
        case GX_TF_RGBA8:  bw = 4; bh = 4; bpp = 32; break;
        default:           bw = 4; bh = 4; bpp = 16; break;
    }
    {
        int cw = (w + bw - 1) / bw;
        int ch = (h + bh - 1) / bh;
        return (u32)cw * ch * bw * bh * bpp / 8;
    }
}

/* ------------------------------------------------------------------ */
/* ★★ REGISTRO DE TEXTURAS DE D3D, CON LRU ACOTADO                     */
/* ------------------------------------------------------------------ */
/* Y esto NO es una optimizacion: es la correccion de un CUELGUE.
 *
 * `GX_InitTexObj` NO lo llama el plugin al crear la textura, sino en CADA
 * ACTIVACION, desde `TextureCache_ActivateTexture` (Textures.cpp:1792,
 * :1799, :1804, :1818).  Mi primera version hacia `obj->backend = NULL`
 * ahi, asi que cada activacion abandonaba la textura de D3D anterior sin
 * soltarla y creaba otra.  Con `[GFXP] tex n=3554` en TRES SEGUNDOS eso son
 * 3554 texturas filtradas: la memoria se agota en la ventana siguiente y
 * sale un `Access violation reading location 0xFFFFFFFF`.
 *
 * Y guardar el backend DENTRO del GXTexObj tampoco vale, por dos razones
 * que estan en el codigo del plugin:
 *
 *  1. El `GXTexObj` va EMPOTRADO en `CachedTexture` (Textures.h:31) y nace
 *     SIN INICIALIZAR: `TextureCache_AddTop` solo pone `GXtexture = NULL`
 *     (Textures.cpp:903).  O sea que `obj->backend` seria basura y
 *     comprobarlo seria leer un puntero al azar.
 *  2. El plugin destruye texturas en `Textures.cpp:786` y `:842`
 *     (`__lwp_heap_free`) sin avisar a nadie, asi que no hay gancho donde
 *     soltar la de D3D.
 *
 * Por eso el registro es NUESTRO y va por LRU: nunca hay mas de
 * GXD3D_TEX_MAX texturas vivas, se desaloja la mas antigua y ahi SI se
 * libera.  Si una desalojada se vuelve a pedir, se re-destila -- cuesta,
 * pero es correcto y esta acotado.
 *
 * La clave incluye formato y dimensiones ademas del puntero: el monton del
 * plugin reutiliza direcciones, y sin eso se serviria la textura de otra. */
/* ★★ 2048 ENTRADAS, Y ES UNA CIFRA MEDIDA, NO UN NUMERO REDONDO.
 *
 * Con 256 hubo un bajon reproducible en Zelda -- camara cerca de Link a
 * caballo en Hyrule Field-- que el log explica sin ambiguedad:
 *
 *     normal   tex= 26,63ms  conv=  0,14   texconv=39922
 *     bajon    tex=483,22ms  conv=450,32   (+31.000 conversiones)
 *     y el frame 16,66 -> 27,77 con el hilo grafico en 20,6ms
 *
 * La causa: **mi registro era MAS PEQUENO que la cache del plugin**, que
 * llega a 538 nodos (medido en `[GLN64C]`).  En cuanto la escena usa mas de
 * 256 texturas distintas, mi LRU desaloja entradas que el plugin sigue
 * dando por cacheadas, y cada draw vuelve a convertir: crear textura D3D,
 * bloquear, destilar, soltar.  El techo del plugin es
 * `GX_MAX_TEXTURES = (400*1024)/sizeof(CachedTexture)`, del orden del
 * millar, asi que 2048 lo cubre con margen.  Cada entrada son ~20 bytes:
 * 40 KB, nada.
 *
 * ★ Y subir el tamano OBLIGABA a arreglar la busqueda: era un barrido
 * lineal de todo el array en cada `GX_LoadTexObj`, o sea ~12.000 veces por
 * segundo.  A 2048 eso seria 8 veces peor que el problema que arregla.  Por
 * eso ahora hay una tabla hash de direccionamiento abierto sobre el puntero
 * a los texels, que es unico por textura viva.
 *
 * ★★ [Cruis'n USA, 2a fase, logs (8), 2026-10-21] 4096, Y LAS LAPIDAS SE
 * LIMPIAN.  Medido con [GLN64TF]: la cache del plugin llega a su techo,
 * `nodos=2133` (GX_MAX_TEXTURES), con 300-900 texturas nuevas por segundo.
 * Dos fallos de este registro, que se suman:
 *   1. Con 2048 entradas y el desalojo al 75 % (1536 vivas), el registro
 *      tiraba texturas que el plugin seguia teniendo en su cache: cada uso
 *      volvia a convertirla (texconv ~520/s con 380 cargas/s) y cada alta
 *      hacia el barrido de 2048 buscando la mas antigua.
 *   2. Cada baja deja una LAPIDA y nunca se limpiaban (solo al reiniciar):
 *      con tanta rotacion no quedaba ningun hueco virgen, y cada busqueda
 *      que falla -- cada textura nueva, cada `__lwp_heap_free` -- recorria
 *      las 2048.  `tcupd` propio: 5,4-6,1 ms por VI (1,2 en la 1a fase).
 * Ahora 4096 (el 75 % son 3072, por encima del techo del plugin) y, si
 * vivas + lapidas pasan del 75 %, se reconstruye la tabla sin lapidas
 * (gxd3d_tex_compacta), que es O(4096) de vez en cuando. */
#define GXD3D_TEX_MAX   4096
#define GXD3D_TEX_MASK  (GXD3D_TEX_MAX - 1)

struct GXD3DTexEntry
{
    void              *data;      /* puntero a los texels en formato GX */
    u16                w, h;
    u8                 fmt;
    IDirect3DTexture9 *tex;
    u32                serial;    /* para el LRU */
    u8                 sucia;     /* GXD3D_TexturaCambiada: resubir, no crear */
};
static GXD3DTexEntry s_texReg[GXD3D_TEX_MAX];
static u32           s_texSerial = 0;
static int           s_texCount  = 0;   /* entradas ocupadas */

/* Los punteros del monton del plugin estan alineados a 32, asi que los cinco
 * bits bajos son siempre cero: hay que mezclarlos o todo cae en el mismo
 * cubo. */
static __inline unsigned int gxd3d_tex_hash(void *data)
{
    unsigned int h = (unsigned int)(size_t)data;
    h ^= h >> 13;
    h *= 0x9E3779B1u;
    h ^= h >> 15;
    return h & GXD3D_TEX_MASK;
}

static GXD3DTexEntry *gxd3d_tex_find(void *data, u16 w, u16 h, u8 fmt)
{
    unsigned int i = gxd3d_tex_hash(data);
    int probes;
    /* Direccionamiento abierto lineal.  Se para en el primer hueco VACIO
     * (tex == NULL y data == NULL): las bajas dejan lapida (data != NULL con
     * tex NULL) para no cortar cadenas de sondeo. */
    for (probes = 0; probes < GXD3D_TEX_MAX; probes++)
    {
        GXD3DTexEntry *e = &s_texReg[i];
        if (!e->tex && !e->data) return NULL;      /* hueco virgen: no esta */
        if (e->tex && e->data == data &&
            e->w == w && e->h == h && e->fmt == fmt)
            return e;
        i = (i + 1) & GXD3D_TEX_MASK;
    }
    return NULL;
}

/* [Cruis'n USA] La tabla otra vez sin lapidas: las vivas, cada una en su
 * cadena de sondeo desde cero.  Solo la toca el hilo que dibuja (el gancho
 * de liberacion, GX_LoadTexObj), y nadie guarda punteros a entradas entre
 * llamadas, asi que se puede mover todo. */
static GXD3DTexEntry s_texTmp[GXD3D_TEX_MAX];
static void gxd3d_tex_compacta(void)
{
    int k, n = 0;
    for (k = 0; k < GXD3D_TEX_MAX; k++)
        if (s_texReg[k].tex)
            s_texTmp[n++] = s_texReg[k];
    memset(s_texReg, 0, sizeof(s_texReg));
    for (k = 0; k < n; k++)
    {
        unsigned int i = gxd3d_tex_hash(s_texTmp[k].data);
        while (s_texReg[i].tex)
            i = (i + 1) & GXD3D_TEX_MASK;
        s_texReg[i] = s_texTmp[k];
    }
    s_texLapidas = 0;
}

/* Hueco para una entrada nueva.  Si la tabla se llena demasiado, desaloja la
 * mas antigua para que el sondeo no degenere. */
static GXD3DTexEntry *gxd3d_tex_slot(void *data)
{
    unsigned int i;
    int probes;

    /* [Cruis'n USA] Con demasiadas lapidas, fuera: ver GXD3D_TEX_MAX. */
    if (s_texLapidas && ((unsigned int)s_texCount + s_texLapidas) * 4u >= GXD3D_TEX_MAX * 3u)
        gxd3d_tex_compacta();

    /* ★ Por encima del 75% de ocupacion el sondeo lineal se degrada mucho.
     * Se hace sitio desalojando la MAS ANTIGUA, que es lo que hacia el LRU
     * de antes -- pero ahora solo ocurre con >1500 texturas vivas, no con
     * 256. */
    if (s_texCount * 4 >= GXD3D_TEX_MAX * 3)
    {
        int k, lru = -1;
        u32 oldest = 0xFFFFFFFFu;
        for (k = 0; k < GXD3D_TEX_MAX; k++)
            if (s_texReg[k].tex && s_texReg[k].serial < oldest)
            { oldest = s_texReg[k].serial; lru = k; }
        if (lru >= 0)
        {
            /* ★ Castear a D3DResource*, NUNCA a IUnknown*: en Xbox 360 el
             * D3D9 no es COM y `((IUnknown*)p)->Release()` compila y salta a
             * 0x00000000 (reference_xbox360_d3d9_metodos_ausentes). */
            d3dq_soltar((D3DResource *)s_texReg[lru].tex);
            s_texReg[lru].tex = NULL;      /* queda lapida: data != NULL */
            s_texCount--;
            s_texLapidas++;
        }
    }

    i = gxd3d_tex_hash(data);
    for (probes = 0; probes < GXD3D_TEX_MAX; probes++)
    {
        GXD3DTexEntry *e = &s_texReg[i];
        if (!e->tex)                               /* hueco virgen o lapida */
        {
            if (e->data && s_texLapidas)
                s_texLapidas--;                    /* se reusa una lapida */
            s_texCount++;
            return e;
        }
        i = (i + 1) & GXD3D_TEX_MASK;
    }
    return &s_texReg[gxd3d_tex_hash(data)];   /* no deberia pasar */
}

/* ★ [Perfect Dark, la lista esperando ~9 ms en cada frame] LOS TEXELS DE ESTE
 * PUNTERO HAN CAMBIADO: el proximo GX_LoadTexObj los vuelve a subir SOBRE LA
 * MISMA textura de D3D, sin crearla.  Crear es SINCRONO (el productor espera al
 * consumidor, hasta un frame).  Medido con [CREATEX] (log 14): 22-40 texturas
 * de 320x240 creadas por segundo, todas de FrameBuffer_GX_CargaNuevoDesdeRDRAM,
 * que pedia, subia y liberaba un bufer en cada lista -- con lo que el registro,
 * que va por puntero, no acertaba nunca.  Ahora ese bufer es fijo y avisa con
 * esto.  Sigue la cadena de sondeo de gxd3d_tex_find. */
void GXD3D_TexturaCambiada(void *data)
{
    unsigned int i = gxd3d_tex_hash(data);
    int probes;
    for (probes = 0; probes < GXD3D_TEX_MAX; probes++)
    {
        GXD3DTexEntry *e = &s_texReg[i];
        if (!e->tex && !e->data) return;   /* hueco virgen: fin de la cadena */
        if (e->tex && e->data == data)
            e->sucia = 1;
        i = (i + 1) & GXD3D_TEX_MASK;
    }
}

/* ★★ EL MONTON DEL PLUGIN REUTILIZA DIRECCIONES, Y SIN ESTO SE SERVIA LA
 * TEXTURA DE OTRA COSA.
 *
 * La clave del registro es el puntero a los texels, y `TextureCache_Remove` /
 * `TextureCache_RemoveBottom` (Textures.cpp:786, :842) sueltan ese buffer con
 * `__lwp_heap_free` al desalojar.  La siguiente textura que entre puede caer
 * en la MISMA direccion con el mismo ancho, alto y formato: la clave casa y
 * se dibuja con los texels viejos.
 *
 * Y el plugin no ofrece ningun otro aviso: `GX_InvalidateTexAll` solo sale de
 * FrameBuffer.cpp y VI.cpp (nunca del desalojo) y `DCFlushRange` solo cubre
 * el camino de 2xSaI (Textures.cpp:1261, :1663), no la carga normal.  Por eso
 * el gancho va en el monton, que es fichero NUESTRO.
 *
 * Sintoma medido: `texconv` clavado en 15 con 931.172 draws.  Un contador que
 * no sube cuando el juego cambia de escenario es la firma de esto. */
/* Definida junto a `s_rtt`, mas abajo: aqui solo se declara. */
static void gxd3d_rtt_olvida(void *ptr);
/* Definida justo debajo del gancho, por el mismo motivo. */
static void gxd3d_tex_olvida(void *ptr);

static void gxd3d_tex_free_hook(heap_cntrl *theheap, void *ptr)
{
    /* O(1) para todo lo que no sea el monton de texturas del plugin: por
     * aqui pasa tambien el RecompCache del dynarec, que libera en caliente.
     *
     * [Fase 6] Se comparan LOS DOS montones en vez de preguntar cual esta
     * activo: son dos punteros distintos y no nulos a la vez (cada plugin
     * crea el suyo al arrancar), asi que la comparacion ya distingue sola.
     * Ademas esto corre en el camino caliente de liberacion y una llamada a
     * `xbox64_gfx_plugin_activo()` aqui seria una indireccion por cada
     * `free` del dynarec. */
    if (theheap != GXtexCache && theheap != Rice_GXtexCache) return;

    /* ★★ Y EL REGISTRO DE RESUELTAS TAMBIEN, QUE FALTABA.
     *
     * Es la MISMA clase de fallo que el de abajo -- la clave es un puntero
     * del monton del plugin y el monton REUTILIZA direcciones-- pero ahi
     * quedo sin cubrir: `s_rtt` no tenia ningun camino de borrado, solo el
     * desalojo por LRU.  Consecuencia: el plugin quita un framebuffer
     * (`FrameBuffer_Remove` -> `__lwp_heap_free`), la direccion se recicla
     * para una textura normal, y `gxd3d_find_resolved` ACIERTA y sirve la
     * imagen vieja del framebuffer.  Es el bug de las setas de Conker otra
     * vez, en el otro plugin.
     *
     * ★ Y es lo que hace que el tope de la tabla sea secundario: la
     * referencia (GLideN64) no tiene tope pero SI borra -- `m_list.erase`
     * en seis sitios, con la vida del FBO atada al objeto `FrameBuffer`.
     * Con el borrado puesto, la tabla deja de llenarse de cadaveres y el
     * contador `rttdesal` se convierte en la prueba de que 64 sobran. */
    gxd3d_rtt_olvida(ptr);

    gxd3d_tex_olvida(ptr);
}

/* ★★ [Fase 6] SACADA DEL GANCHO DE LIBERACION PARA PODER LLAMARLA TAMBIEN
 * DESDE `GX_InitTexObj`.  El porque esta alli; aqui solo el mecanismo.
 *
 * ★ Sondeo hash, no barrido: por aqui pasa cada `__lwp_heap_free` del monton
 * de texturas.  Y al dar de baja se deja LAPIDA (`data` se conserva, `tex` a
 * NULL) para no cortar cadenas de sondeo de otras entradas que colisionaron
 * en el mismo cubo. */
static void gxd3d_tex_olvida(void *ptr)
{
    {
        unsigned int k = gxd3d_tex_hash(ptr);
        int probes;
        for (probes = 0; probes < GXD3D_TEX_MAX; probes++)
        {
            GXD3DTexEntry *e = &s_texReg[k];
            if (!e->tex && !e->data) break;        /* hueco virgen: no esta */
            if (e->tex && e->data == ptr)
            {
                d3dq_soltar((D3DResource *)e->tex);
                e->tex = NULL;                      /* lapida */
                s_texCount--;
                s_texLapidas++;
#if XBOX64_TRAZAS
                gxd3d_dbg_texfree++;
#endif
                break;
            }
            k = (k + 1) & GXD3D_TEX_MASK;
        }
    }
}

static void gxd3d_tex_reset(void)
{
    int i;
    for (i = 0; i < GXD3D_TEX_MAX; i++)
    {
        if (s_texReg[i].tex) d3dq_soltar((D3DResource *)s_texReg[i].tex);
        s_texReg[i].tex = NULL;
        s_texReg[i].data = NULL;   /* aqui SI se limpian las lapidas */
    }
    s_texSerial = 0;
    s_texCount  = 0;
    s_texLapidas = 0;
}

/* ★ D3D acolcha el pitch de las texturas `LIN_` a 256 bytes, y EL SAMPLER LEE
 * EL RELLENO -- las dos mitades estan MEDIDAS en Rice, no supuestas:
 *
 *     [TEXPAD] 32x32 creada 32x32 pitch=256 ancho*4=128 sobran=32 texels
 *
 * y una sonda que pintaba ese relleno de magenta lo hacia aparecer en
 * pantalla.  Solo ocurre con anchos menores de 64; a 64 texels los 256 bytes
 * caen justos.  El arreglo es el de upstream (`handle_small_surface`,
 * xenos_backend.cpp:1427): replicar la fila por todo el pitch para que
 * cualquier lectura del relleno caiga en una copia valida y en fase.
 *
 * Solo horizontal, a proposito: el relleno medido es de pitch, y escribir mas
 * alla de la altura pedida seria salirse del buffer porque D3D no expone la
 * altura realmente asignada.  Es un gemelo de
 * `replicateIntoPitchPadding` (d3d9_backend.cpp:6254); no se reutiliza
 * aquella porque es `static` en el backend de Rice y este fichero no debe
 * depender de el. */
/* ★ Y EL RELLENO SE LLENA SEGUN EL MODO DE DIRECCIONAMIENTO, NO SIEMPRE
 * IGUAL.  Repetir la fila desde x=0 -- que es lo que hace upstream y lo que
 * hacia esto-- es lo correcto para REPEAT: el texel que sigue al ultimo es el
 * primero, asi que un tap filtrado en el borde mezcla con el color que toca.
 *
 * Para CLAMP es justo lo contrario: ahi el texel que sigue al ultimo TIENE
 * que ser el ultimo otra vez.  Repitiendo la fila, un tap en el borde derecho
 * mezcla el ultimo texel con el PRIMERO, o sea que el color del borde
 * izquierdo se cuela por el derecho.
 *
 * Y para MIRROR es otra cosa distinta: ahi la fila VUELVE sobre si misma, o
 * sea ultimo, penultimo, ...  Repetir la fila tambien la rompe.
 *
 * SINTOMA: en Mario 64, con la cabeza grande del arranque, la pupila DERECHA
 * sale rosada.  ★ Y LA ASIMETRIA ES EL DATO QUE SEÑALA A ESTO: la cara es
 * simetrica y el juego dibuja LA MISMA textura del ojo espejada para el otro
 * lado, asi que los dos ojos comparten textura pero NO modo de
 * direccionamiento.  Cualquier defecto que dependa del modo -- y este lo
 * hace-- estropea uno y deja el otro bien.  Lo que se colaba por el borde es
 * el color del lado opuesto de la textura, que en la cara de Mario es piel:
 * rosa.
 *
 * ★★ DESCARTADO POR EL CAMINO, que era la otra sospecha razonable (el mismo
 * fallo que dejo "todo rojo" y la piel rosada hace unas sesiones): NO es el
 * orden de canales.  Los tres caminos de color estan comprobados y son
 * coherentes -- el color de VERTICE se empaqueta 0xAABBGGRR
 * (GXD3D_PACK_RGBA), las TEXTURAS se escriben 0xAARRGGBB, que es lo que pide
 * una superficie A8R8G8B8, y las constantes del TEV viajan componente a
 * componente.  Ademas glN64_GX solo usa GX_Color4u8: no hay una segunda
 * entrada de color sin convertir.
 *
 * Solo pasa con anchos menores de 64 texels, que son los que dejan relleno:
 * D3D acolcha el pitch de las `LIN_` a 256 bytes y a 64 texels caen justos.
 * Que el sampler LEE ese relleno esta medido en Rice, no supuesto -- ver la
 * nota de arriba. */
#define GXD3D_PAD_SEGUN_WRAP 1

static void gxd3d_replicate_pitch_padding(void *bits, int pitchBytes,
                                          int wTex, int hTex, u8 wrap_s)
{
    int wPitch = pitchBytes / 4;   /* el pitch, en texels */
    int y, xp;

    if (!bits || wTex <= 0 || pitchBytes <= 0 || wPitch <= wTex)
        return;

    for (y = 0; y < hTex; ++y)
    {
        u32 *row = (u32 *)((u8 *)bits + (size_t)y * pitchBytes);

#if GXD3D_PAD_SEGUN_WRAP
        if (wrap_s == GX_MIRROR)
        {
            /* MIRROR: la fila VUELVE sobre si misma.  El texel que sigue al
             * ultimo es el ultimo otra vez, luego el penultimo, y asi; cada
             * 2*wTex el patron se repite.  Es la formula estandar del
             * espejado y es lo que hace que el borde no salte de color. */
            int periodo = wTex * 2;
            for (xp = wTex; xp < wPitch; ++xp)
            {
                int p = xp % periodo;
                row[xp] = row[(p < wTex) ? p : (periodo - 1 - p)];
            }
            continue;
        }
        if (wrap_s != GX_REPEAT)
        {
            /* CLAMP: prolongar el ultimo texel. */
            u32 last = row[wTex - 1];
            for (xp = wTex; xp < wPitch; ++xp)
                row[xp] = last;
            continue;
        }
#else
        (void)wrap_s;
#endif

        /* REPEAT: repetir la fila, en fase. */
        for (xp = wTex; xp < wPitch; xp += wTex)
        {
            int n = wTex;
            if (xp + n > wPitch)
                n = wPitch - xp;         /* la ultima copia va cortada */
            memcpy(&row[xp], row, (size_t)n * 4);
        }
    }
}

/* ★★ [etapa 3] LA SUBIDA DE TEXELS, EN EL LADO DEL CONSUMIDOR.
 *
 * Es el trozo de `GX_LoadTexObj` que tocaba la textura por CPU -- `LockRect`,
 * copia fila a fila con el pitch del lock, relleno del borde para el
 * wrap, `UnlockRect` --, movido tal cual.  El productor sigue destilando
 * los texels (`gxd3d_untile_*`, que es CPU pura y no toca D3D) y los manda
 * por la orden `SubeTextura`, copiados al pool.  Es lo que hace GLideN64
 * con `glTexSubImage2D` (ThreadedOpenGl/opengl_Wrapper.cpp:342-359).
 *
 * `texels` es ARGB lineal de `sw` texels de ancho; se copian las `h` filas
 * de `w` texels.  Sin `d3dq_sinc()`: ver la regla de oro de
 * d3d9_grabador.h para las funciones del consumidor. */
void gxd3d_consumidor_sube_tex(IDirect3DTexture9 *tex, const void *texels,
                               int w, int h, int sw, int wrap_s)
{
    D3DLOCKED_RECT lr;
    const u32 *tmp = (const u32 *)texels;

    if (!tex || !tmp || w <= 0 || h <= 0 || sw < w) return;
    if (SUCCEEDED(tex->LockRect(0, &lr, NULL, 0)))
    {
        int y;
        for (y = 0; y < h; y++)
            memcpy((u8 *)lr.pBits + (size_t)y * lr.Pitch,
                   tmp + (size_t)y * sw, (size_t)w * 4);
        gxd3d_replicate_pitch_padding(lr.pBits, (int)lr.Pitch, w, h,
                                      (u8)wrap_s);
        tex->UnlockRect(0);
    }
}

void GX_InitTexObj(GXTexObj *obj, void *img_ptr, u16 wd, u16 ht, u8 fmt,
                   u8 wrap_s, u8 wrap_t, u8 mipmap)
{
    if (!obj) return;

    /* ★★ [Fase 6] CON RICE, ESTA LLAMADA SIGNIFICA "ACABO DE REESCRIBIR ESTOS
     * TEXELS", Y HAY QUE TIRAR LA TEXTURA DE D3D QUE TENIAMOS EN LA CACHE.
     *
     * Los dos plugins llaman a `GX_InitTexObj` con contratos OPUESTOS, y
     * esta comprobado leyendo TODOS sus sitios de llamada:
     *
     *   Rice   UNO solo sobre texturas del juego: `COGLTexture::EndUpdate`
     *          (OGLTexture.cpp:131), justo detras del `DCFlushRange` que
     *          cierra la escritura de los texels.  Al DIBUJAR no pasa por
     *          aqui: usa `GX_LoadTexObj` (OGLRender.cpp:1735-1736,
     *          TEVCombiner.cpp:359, TextureManager.cpp:1074).
     *
     *   glN64  en cada ACTIVACION (OpenGL.cpp:2008-2037, Textures.cpp:2059).
     *          Es el caso que ya costo una fuga de 3554 texturas en tres
     *          segundos y que documenta la nota de mas abajo.
     *
     * ★ POR QUE HACIA FALTA.  El registro se indexa por (puntero, w, h,
     * formato) y no valida el contenido.  Eso basta para glN64, cuyo cache
     * libera por el monton -- y el gancho de liberacion lo cubre --.  Pero
     * el `CTextureManager` de Rice RECICLA sus `CTexture`: reescribe texels
     * nuevos en la MISMA reserva, sin liberarla.  Misma clave, contenido
     * distinto: el shim servia la textura vieja para siempre.  Es lo que se
     * veia como suelo y montanyas con cuadros negros, y lo que desaparecia
     * al apagar `xbox64_texcache`.
     *
     * ★ Y NO MATA LA CACHE DE RICE.  `EndUpdate` solo corre cuando Rice
     * reconstruye una textura de verdad, o sea en un fallo de SU cache por
     * CRC, no al dibujar.  Por eso apagar la opcion cuesta rendimiento y
     * esto no: se invalida lo que ha cambiado y nada mas.
     *
     * ★ EL CORTE VA POR PLUGIN PORQUE ES UNA DIFERENCIA DE CONTRATO, no una
     * propiedad que hoy coincida: con glN64 invalidar aqui seria apagarle la
     * cache entera, que es exactamente lo que hace `texcache=0`. */
#if XBOX64_TRAZAS
    gxd3d_dbg_initobj++;
#endif
    if (img_ptr && xbox64_gfx_plugin_activo() == XBOX64_GFX_RICE)
    {
#if XBOX64_TRAZAS
        gxd3d_dbg_texinval++;
#endif
        gxd3d_tex_olvida(img_ptr);
    }

    obj->data    = img_ptr;
    obj->width   = wd;
    obj->height  = ht;
    obj->format  = fmt;
    obj->wrap_s  = wrap_s;
    obj->wrap_t  = wrap_t;
    obj->mipmap  = mipmap;
    /* Valores por defecto de GX si no se llama a GX_InitTexObjLOD. */
    obj->minfilt = GX_LINEAR;
    obj->magfilt = GX_LINEAR;
    obj->minlod = 0.0f; obj->maxlod = 0.0f; obj->lodbias = 0.0f;
    obj->biasclamp = GX_FALSE; obj->edgelod = GX_FALSE; obj->maxaniso = GX_ANISO_1;
    /* ★ NO se toca ningun backend aqui.  La textura de D3D vive en el
     * registro de arriba, no en esta struct.  Ver la explicacion larga. */
}

void GX_InitTexObjLOD(GXTexObj *obj, u8 minfilt, u8 magfilt, f32 minlod,
                      f32 maxlod, f32 lodbias, u8 biasclamp, u8 edgelod,
                      u8 maxaniso)
{
    if (!obj) return;
    obj->minfilt = minfilt; obj->magfilt = magfilt;
    obj->minlod = minlod;   obj->maxlod = maxlod;  obj->lodbias = lodbias;
    obj->biasclamp = biasclamp; obj->edgelod = edgelod; obj->maxaniso = maxaniso;
}

/* ★ Las dos que pide Rice_GX y glN64_GX no usaba.
 *
 * Rice no vuelve a llamar a `GX_InitTexObj` para cambiar filtro o repeticion:
 * modifica el `GXTexObj` ya inicializado (OGLExtRender.cpp:151,350 y
 * OGLRender.cpp:269).  Como aqui el objeto es nuestro y el estado real se
 * aplica en `GX_LoadTexObj` -- que lee estos campos--, basta con guardarlos.
 *
 * No hay que invalidar nada: el registro de texturas del shim se indexa por
 * `data`/tamano/formato, no por filtro ni repeticion, asi que cambiar estos
 * no obliga a re-destilar.  Lo que SI hacen es romper el espejo de sampler
 * (`s_samp`), y de eso ya se encarga `GX_LoadTexObj` comparando. */
void GX_InitTexObjWrapMode(GXTexObj *obj, u8 wrap_s, u8 wrap_t)
{
    if (!obj) return;
    obj->wrap_s = wrap_s;
    obj->wrap_t = wrap_t;
}

void GX_InitTexObjFilterMode(GXTexObj *obj, u8 minfilt, u8 magfilt)
{
    if (!obj) return;
    obj->minfilt = minfilt;
    obj->magfilt = magfilt;
}

static DWORD gxd3d_addr(u8 wrap)
{
    switch (wrap)
    {
        case GX_REPEAT: return D3DTADDRESS_WRAP;
        case GX_MIRROR: return D3DTADDRESS_MIRROR;
        default:        return D3DTADDRESS_CLAMP;
    }
}

static u8 gxd3d_resolved_mono(void *key);   /* mas abajo, con s_rtt */

/* Ver GXD3D_SetDepthClamp en gccore.h y la nota del vertex shader.  Lo
 * encolado se vacia ANTES de cambiar: el lote pendiente se dibuja con la
 * bandera que tenia. */
void GXD3D_SetDepthClamp(u8 on)
{
    const float f = on ? 1.0f : 0.0f;
    if (s_halfPx[3] == f) return;
    gxd3d_flush_pending();
    s_halfPx[3]   = f;
    s_halfPxDirty = 1;
    gxd3d_tev_zclamp(on ? 1 : 0);
    /* (Las dos lineas de Hi-Z de abajo pasan ahora por `gxd3d_hiz_aplica`,
     * que tambien mira el dibujo sobre el depth image; ver
     * GXD3D_SetRenderToDepth.  Lo que dice la nota no cambia.) */

    /* ★ LA Z JERARQUICA, APAGADA MIENTRAS EL SHADER ESCRIBE LA PROFUNDIDAD.
     *
     * La Hi-Z de Xenos se prueba y se actualiza con la z RASTERIZADA, y aqui
     * esa z es la del recortador (subida a >= 0), no la que escribe el pixel
     * shader.  Con la Hi-Z encendida esos bloques quedaban marcados "mas
     * cerca" de lo que estan y la geometria posterior se descartaba a
     * trozos -- huecos en el escenario con el color de fondo (sintoma visto
     * por el usuario en la primera version) --.  Apagada para estos lotes:
     * con el test LEQUAL solo escriben profundidades mas cercanas que las
     * que ya habia, asi que la Hi-Z anterior sigue siendo conservadora.  Al
     * cerrarlos vuelve el valor por defecto, AUTOMATIC. */
    gxd3d_hiz_aplica();
}

void GX_LoadTexObj(GXTexObj *obj, u8 mapid)
{
    IDirect3DTexture9 *tex;
    int resubir = 0;   /* GXD3D_TexturaCambiada: subir sobre la que ya hay */
#if XBOX64_TRAZAS   /* cronometro de [GXSH] */
    const unsigned long long tTex = __mftb();
    struct GxshTexClock {
        unsigned long long t0;
        GxshTexClock(unsigned long long t) : t0(t) { }
        ~GxshTexClock() { gxsh_tex_tb += (unsigned int)(__mftb() - t0); gxsh_tex_n++; }
    } _clk(tTex);   /* hay varios `return` por el camino; asi se cierran todos */
#endif

    /* ★★ LAS TRES SALIDAS TEMPRANAS DE AQUI ABAJO CAMBIABAN LA TEXTURA
     * LIGADA SIN VACIAR EL LOTE PENDIENTE.
     *
     * Solo el camino normal (mas abajo, "si cambia de verdad") llamaba a
     * `gxd3d_flush_pending()`.  Por las otras tres el lote ya acumulado se
     * dibujaba con la textura del SIGUIENTE -- y dos de ellas ligan NULL, o
     * sea que ese lote se dibujaba muestreando nada: NEGRO.
     *
     * Misma familia que GX_SetColorUpdate y GX_SetCoPlanar, y el mismo
     * sintoma: trozos de escena en negro que cambian de sitio entre frames,
     * segun donde caiga la frontera del lote.
     *
     * Se vacia ANTES de tocar el sampler, no despues, que es todo el
     * asunto. */
    if (!obj || !obj->data || obj->width == 0 || obj->height == 0)
    {
        gxd3d_flush_pending();
        g_pd3dDevice->SetTexture(mapid, NULL);
        if (mapid < 4) s_texBound[mapid] = 0;
        gxd3d_tev_tex_mono(mapid, 0);
        gxd3d_tev_tex_esfb(mapid, 0);   /* marcado `texalfa` */
        return;
    }

    /* ★ PRIMERO: ¿es un buffer que salio de un GX_CopyTex?  Si lo es, la
     * imagen esta en la textura resuelta y NO en la RAM del plugin -- ahi no
     * hay mas que basura, porque nosotros no escribimos texels en formato
     * GX.  Sin esta comprobacion el render-to-texture sale en negro o con
     * ruido, que es justo el sintoma que costo varias sesiones en fb_gln64
     * hasta entender que "el buffer que se GUARDA no es el que se LEE". */
    {
        extern IDirect3DTexture9 *gxd3d_find_resolved(void *key);
        IDirect3DTexture9 *rt = gxd3d_find_resolved(obj->data);
        if (rt)
        {
#if XBOX64_TRAZAS
            gxd3d_dbg_rtthit++;      /* camino barato: ya estaba resuelta */
#endif
            gxd3d_flush_pending();   /* el lote pendiente usa la textura VIEJA */
            g_pd3dDevice->SetTexture(mapid, rt);
            if (mapid < 4) s_texBound[mapid] = 1;
            g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_ADDRESSU, gxd3d_addr(obj->wrap_s));
            g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_ADDRESSV, gxd3d_addr(obj->wrap_t));
            g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_MINFILTER,
                (obj->minfilt == GX_NEAR) ? D3DTEXF_POINT : D3DTEXF_LINEAR);
            g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_MAGFILTER,
                (obj->magfilt == GX_NEAR) ? D3DTEXF_POINT : D3DTEXF_LINEAR);
            g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            gxd3d_tev_tex_mono(mapid, gxd3d_resolved_mono(obj->data));
            gxd3d_tev_tex_esfb(mapid, 1);   /* marcado `texalfa` */
            return;
        }
    }
    gxd3d_tev_tex_mono(mapid, 0);   /* textura normal: nunca monocroma */
    gxd3d_tev_tex_esfb(mapid, 0);   /* marcado `texalfa` */

    /* ★ El registro con LRU, no `obj->backend`.  Ver la explicacion larga
     * encima de GX_InitTexObj: guardar la textura en el GXTexObj filtraba
     * una por ACTIVACION (3554 en tres segundos) y ademas esa struct nace
     * sin inicializar. */
    {
        GXD3DTexEntry *hit = gxd3d_tex_find(obj->data, obj->width, obj->height,
                                            obj->format);
#if XBOX64_TRAZAS
        s_tbmpAtados++;
#endif

        /* Cache apagada, o volcado armado: se tira la convertida y se vuelve
         * a destilar.  Se reusa LA MISMA ranura (lapida: `tex` a NULL con
         * `data` puesto) en vez de dejarla y pedir otra, o el registro
         * creceria hasta el LRU en un par de frames.  `d3dq_soltar` encola el
         * Release, asi que es seguro aunque la GPU siga leyendola. */
        if (hit && (!s_texCacheOn || s_tbmpActivo))
        {
            d3dq_soltar((D3DResource *)hit->tex);
            hit->tex = NULL;
            s_texCount--;
            s_texLapidas++;
            hit = NULL;
        }

        if (hit)
        {
            hit->serial = ++s_texSerial;
            tex = hit->tex;
            /* GXD3D_TexturaCambiada: misma textura, texels nuevos.  Lo que
             * este pendiente de dibujar con ella usa los viejos: se dibuja
             * antes de que la subida entre en la cola. */
            if (hit->sucia)
            {
                hit->sucia = 0;
                resubir = 1;
                gxd3d_flush_pending();
            }
        }
        else
        {
            tex = NULL;
        }
    }

    if (tex == NULL || resubir)
    {
        /* ★★ `D3DFMT_LIN_`, Y NO ES OPCIONAL.
         *
         * La primera version creaba `D3DFMT_A8R8G8B8` con el razonamiento de
         * que "nadie la lee por CPU, solo la muestrea el shader, asi que
         * tileada es lo correcto".  Es FALSO: `LockRect` sobre una textura
         * tileada devuelve la memoria TILEADA EN CRUDO -- D3D no tilea al
         * escribir-- asi que volcar filas lineales ahi deja al sampler
         * leyendo el swizzle de Xenos sobre datos que no lo llevan.
         *
         * No habia que deducirlo: el camino que SI funciona lo dice con
         * todas las letras en d3d9_backend.cpp:5173-5177, *"Sin esto,
         * LockRect+memcpy lineal produce el clasico resultado en
         * 'bloques'/pixelado"*.  Y el sintoma medido fue exactamente ese:
         * bandas horizontales de bloques rectangulares sobre una geometria
         * por lo demas correcta ([[feedback_referencia_que_funciona]]). */
        if (!resubir &&
            FAILED(g_pd3dDevice->CreateTexture(obj->width, obj->height, 1, 0,
                                               D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT,
                                               &tex, NULL)))
        {
            gxd3d_flush_pending();   /* idem: vaciar ANTES de desligar */
            g_pd3dDevice->SetTexture(mapid, NULL);
            if (mapid < 4) s_texBound[mapid] = 0;
            return;
        }

        {
            /* [etapa 3] Aqui se hacia `d3dq_sinc()` + `LockRect` y se copiaban
             * los texels directamente a la textura.  El productor no puede
             * tocar un recurso de D3D en la etapa 4, asi que destila los
             * texels como siempre y los manda con la orden `SubeTextura`; el
             * LockRect lo hace el consumidor (`gxd3d_consumidor_sube_tex`). */
            {
                int w = obj->width, h = obj->height;

                /* ★★ EL BUFFER NO MIDE w x h: MIDE LO RELLENADO AL BLOQUE.
                 *
                 * `TextureCache_ActivateTexture` pasa a GX_InitTexObj
                 * `realWidth`/`realHeight` (Textures.cpp:1796), pero los
                 * texels los escribio a `GXrealWidth`/`GXrealHeight`, que son
                 * esas mismas redondeadas HACIA ARRIBA al tamano de bloque
                 * (Textures.cpp:1064-1084):
                 *
                 *     blockWidth = (GXsize == 1) ? 8 : 4;   blockHeight = 4;
                 *     textureBytes = GXrealWidth * GXrealHeight * GXsize;
                 *
                 * En hardware GX eso da igual, porque el direccionamiento
                 * tileado ya cuenta por bloques enteros.  Aqui NO: si destilo
                 * usando `w` como bloques-por-fila y el dato tiene mas, cada
                 * fila de bloques se desplaza un poco mas que la anterior.
                 * Sintoma: bandas horizontales de bloques sobre una geometria
                 * correcta.
                 *
                 * Los unicos formatos vivos son IA4 (GXsize 1 -> bloque 8x4),
                 * IA8 y RGB5A3 (2 -> 4x4) y RGBA8 (4 -> 4x4); la tabla
                 * `imageFormat` (Textures.cpp:381) no produce ningun otro. */
                int blockW = (obj->format == GX_TF_IA4 ||
                              obj->format == GX_TF_I4) ? 8 : 4;
                int sw = (w % blockW) ? w + blockW - (w % blockW) : w;
                int sh = (h % 4)      ? h + 4      - (h % 4)      : h;

                /* Se destila a un intermedio y se copia fila a fila: el
                 * intermedio es `sw` de ancho y el destino `w`, y ademas el
                 * pitch del lock trae su propio acolchado. */
                u32 *tmp = (u32 *)malloc((size_t)sw * sh * 4);
                if (tmp)
                {
                    const u8 *src = (const u8 *)obj->data;
                    switch (obj->format)
                    {
                        case GX_TF_I4:     gxd3d_untile_i4(src, tmp, sw, sh);     break;
                        case GX_TF_IA4:    gxd3d_untile_ia4(src, tmp, sw, sh);    break;
                        case GX_TF_IA8:    gxd3d_untile_ia8(src, tmp, sw, sh);    break;
                        case GX_TF_RGB565: gxd3d_untile_rgb565(src, tmp, sw, sh); break;
                        case GX_TF_RGB5A3: gxd3d_untile_rgb5a3(src, tmp, sw, sh); break;
                        case GX_TF_RGBA8:  gxd3d_untile_rgba8(src, tmp, sw, sh);  break;
                        /* ★ Salida silenciosa a negro transparente.  Los
                         * formatos que produce la tabla `imageFormat` del
                         * plugin (Textures.cpp:381) estan todos arriba --
                         * medido con un contador temporal: nunca se dispara. */
                        default:           memset(tmp, 0, (size_t)sw * sh * 4);   break;
                    }
                    /* La copia a la textura, con su pitch y el relleno del
                     * borde, ya no se hace aqui: va por la orden.  `tmp` se
                     * copia al pool dentro, asi que se puede liberar en
                     * cuanto vuelva. */
#if D3D_GRABADOR
                    g_pd3dDevice->SubeTextura(tex, tmp, w, h, sw, (int)obj->wrap_s);
#else
                    gxd3d_consumidor_sube_tex(tex, tmp, w, h, sw, (int)obj->wrap_s);
#endif

                    /* ★★★ EL VOLCADO, AQUI Y NO EN EL ATADO: `tmp` es ARGB
                     * lineal en memoria de CPU.  Ver la nota de arriba. */
#if XBOX64_TRAZAS   /* TEXBMP */
                    if (s_tbmpActivo && s_tbmpN < (int)(sizeof(s_tbmpVistas) /
                                                        sizeof(s_tbmpVistas[0])))
                    {
                        int k, visto = 0;
                        for (k = 0; k < s_tbmpN; k++)
                            if (s_tbmpVistas[k] == obj->data) { visto = 1; break; }
                        if (!visto)
                        {
                            char nom[192];
                            s_tbmpVistas[s_tbmpN] = obj->data;
                            sprintf(nom, "tex_%02d_f%d_%ux%u_d%08X",
                                    s_tbmpN, (int)obj->format,
                                    (unsigned)w, (unsigned)h,
                                    (unsigned)(size_t)obj->data);
                            s_tbmpN++;
                            if (!tbmp_fichero(nom, tmp, w, h, (int)(sw * 4)))
                                s_tbmpFallos++;
                        }
                    }
#endif /* XBOX64_TRAZAS (TEXBMP) */

                    free(tmp);
                }
#if XBOX64_TRAZAS
                gxd3d_dbg_texconv++;
                s_tbmpConvs++;
                /* Dentro de `gxsh_tex`, pero separado: dice si el coste de
                 * texturas es CONVERTIR o solo ligar. */
                gxsh_conv_tb += (unsigned int)(__mftb() - tTex);
#endif
            }
        }

        /* Al registro, desalojando la mas antigua si hace falta.  Una
         * resubida ya esta en el. */
        if (!resubir)
        {
            GXD3DTexEntry *slot = gxd3d_tex_slot(obj->data);
            slot->data   = obj->data;
            slot->w      = obj->width;
            slot->h      = obj->height;
            slot->fmt    = obj->format;
            slot->tex    = tex;
            slot->serial = ++s_texSerial;
            slot->sucia  = 0;
        }
    }

    /* ★★ ESPEJO POR SAMPLER, Y ADEMAS ES PRERREQUISITO DE LA FUSION.
     *
     * `TextureCache_ActivateTexture` llega aqui en CADA draw (~12.700/s
     * medidos) y casi siempre con la misma textura y los mismos filtros:
     * eran ~76.000 llamadas al device por segundo repitiendo el valor.  Y
     * peor: sin comparar, cada draw contaria como cambio de estado y
     * romperia el lote pendiente, o sea que no se fusionaria nada. */
    {
#if XBOX64_TRAZAS
        const unsigned long long tBind = __mftb();
#endif
        const DWORD au = gxd3d_addr(obj->wrap_s);
        const DWORD av = gxd3d_addr(obj->wrap_t);
        const DWORD mn = (obj->minfilt == GX_NEAR) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        const DWORD mg = (obj->magfilt == GX_NEAR) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        GXD3DSampMirror *sm = (mapid < 4) ? &s_samp[mapid] : NULL;

        if (sm && sm->valid && sm->tex == tex &&
            sm->au == au && sm->av == av && sm->mn == mn && sm->mg == mg)
        {
#if XBOX64_TRAZAS
            gxsh_state_tb += (unsigned int)(__mftb() - tBind);
#endif
            return;   /* nada que cambiar: el lote pendiente sigue vivo */
        }

        gxd3d_flush_pending();

        g_pd3dDevice->SetTexture(mapid, tex);
        if (mapid < 4) s_texBound[mapid] = (tex != NULL);
        g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_ADDRESSU, au);
        g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_ADDRESSV, av);
        g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_MINFILTER, mn);
        g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_MAGFILTER, mg);
        g_pd3dDevice->SetSamplerState(mapid, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        if (sm)
        {
            sm->tex = tex; sm->au = au; sm->av = av;
            sm->mn = mn;   sm->mg = mg; sm->valid = 1;
        }
#if XBOX64_TRAZAS
        gxsh_state_tb += (unsigned int)(__mftb() - tBind);
#endif
    }
}

/* ★★ TIRAR EL REGISTRO ENTERO AQUI CUESTA 432 ms/s EN ZELDA OoT.
 *
 * En GX de verdad `GX_InvalidateTexAll` invalida la CACHE DE TEXTURAS de la
 * GPU (la TMEM): los datos siguen en RAM y la llamada es casi gratis.  Aqui
 * se tradujo por "suelta todas las texturas D3D", que obliga a destilar el
 * catalogo entero por CPU otra vez.  Es la misma clase de error que traducir
 * un `GX_Begin` -- gratis en la FIFO-- por algo caro.
 *
 * Y se nota porque `GX_InvalidateTexAll` SOLO sale de FrameBuffer.cpp y
 * VI.cpp: con la emulacion de framebuffer apagada casi no se llama, y con
 * ella encendida se llama en cada operacion.  Medido en OoT, misma escena:
 *
 *              frame      [GLN64T] conv     texconv     GFXTH trabajo
 *   apagada    16,66 ms     0,00 ms/s        542 (plano)   158 ms/s
 *   encendida  31-33 ms   432,69 ms/s      2.425/s         632 ms/s
 *
 * y `rtt=0,13 ms` descarta que el coste sea el `Resolve` o su drenaje.
 *
 * ★ Y EL MOTIVO QUE JUSTIFICABA TIRARLAS YA NO APLICA.  Decia: "el plugin
 * esta a punto de reutilizar las mismas direcciones de su monton para OTRAS
 * texturas, y nuestra clave incluye el puntero".  Cierto, pero de eso se
 * encarga ya `gxd3d_tex_free_hook`, el gancho sobre `__lwp_heap_free`: da de
 * baja la entrada EN EL MOMENTO en que el plugin libera el buffer, asi que
 * cuando la direccion se reutiliza nuestra entrada vieja ya no existe.  El
 * barrido de aqui es redundante con el gancho.
 *
 * Lo que SI hay que seguir haciendo es DESLIGAR los samplers: si el device se
 * queda una textura ligada y luego se libera, la muestrea en el Present
 * siguiente (trampa nº2 de project_xenon64_libretro_port).
 *
 * A 1 vuelve el barrido, por si aparece el clasico "sale la textura de otra
 * cosa" -- eso significaria que hay un camino que recicla direcciones SIN
 * pasar por `__lwp_heap_free`, y entonces el arreglo es taparlo ahi, no
 * volver a pagar 432 ms/s. */
#define GXD3D_INVAL_TIRA_TODO 0

void GX_InvalidateTexAll(void)
{
    int i;

    gxd3d_flush_pending();   /* lo pendiente lee estas texturas */

    for (i = 0; i < 4; i++)
    {
        g_pd3dDevice->SetTexture(i, NULL);
        s_texBound[i] = 0;
    }

    /* ★★ Y EL ESPEJO DE SAMPLERS, QUE FALTABA -- ES UN DIBUJO SIN TEXTURA.
     *
     * `s_texBound` no es lo que consulta la salida temprana de
     * `GX_LoadTexObj`: esa mira `s_samp[mapid]` (:3594-3596).  Al desligar
     * aqui sin tocar el espejo, el device se queda con NULL en los cuatro
     * samplers y `s_samp` sigue afirmando que la textura esta puesta, asi
     * que el siguiente `GX_LoadTexObj` de ESA MISMA textura se cree servido
     * y no reprograma nada.  El draw sale sin textura y no hay ni un error.
     *
     * ★ Con glN64 casi no se nota: solo llega aqui desde FrameBuffer.cpp y
     * VI.cpp.  Rice llega en pleno juego, desde `TextureManager.cpp:776-777`,
     * en CADA fallo de CRC de una entrada cacheada.
     *
     * Las otras dos limpiezas de `s_samp` estan en `gxd3d_frame_begin` (por
     * el frontend) y en `gxd3d_init`; esta es la tercera y por el mismo
     * motivo: lo que hay en el device dejo de ser lo que dice el espejo. */
    memset(s_samp, 0, sizeof(s_samp));

#if GXD3D_INVAL_TIRA_TODO
    gxd3d_tex_reset();
#endif
}

/* ------------------------------------------------------------------ */
/* Copia del framebuffer empotrado                                     */
/* ------------------------------------------------------------------ */
static u16 s_copySrcL, s_copySrcT, s_copySrcW, s_copySrcH;
static u16 s_copyDstW, s_copyDstH;
static u32 s_copyDstFmt;

void GX_SetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht)
{
    s_copySrcL = left; s_copySrcT = top; s_copySrcW = wd; s_copySrcH = ht;
}

void GX_SetTexCopyDst(u16 wd, u16 ht, u32 fmt, u8 mipmap)
{
    (void)mipmap;
    s_copyDstW = wd; s_copyDstH = ht; s_copyDstFmt = fmt;
}

static GXColor s_clearColor = { 0, 0, 0, 255 };
static u32     s_clearZ     = 0x00FFFFFF;

static void gxd3d_set_clear_impl(GXColor c, u32 z) { s_clearColor = c; s_clearZ = z; }

void GX_SetCopyClear(GXColor color, u32 zvalue)
{
    /* Color y Z con los que se limpia el EFB despues de copiar.  Se guarda y
     * lo aplica GX_CopyTex/GX_CopyDisp cuando le pasan clear=GX_TRUE. */
    gxd3d_set_clear_impl(color, zvalue);
}

void GX_SetCopyFilter(u8 aa, u8 sample_pattern[12][2], u8 vf, u8 vfilter[7])
{
    /* Filtro de deflicker de la copia EFB->XFB.  En Xenos el equivalente es
     * el resolve con MSAA, y la 360 no lo necesita (sale por HDMI/componentes
     * progresivo).  Se ignora a proposito. */
    (void)aa; (void)sample_pattern; (void)vf; (void)vfilter;
}

/* ------------------------------------------------------------------ */
/* Registro de texturas RESUELTAS (render-to-texture)                  */
/* ------------------------------------------------------------------ */
/* ★ LA PIEZA QUE HACE QUE ESTO FUNCIONE, Y NO ES OBVIA.
 *
 * `GX_CopyTex(dest, ...)` en GameCube escribe en `dest` los texels YA en
 * formato GX.  glN64_GX despues le pasa ESE MISMO puntero a `GX_InitTexObj`
 * y lo muestrea como una textura normal (FrameBuffer.cpp).
 *
 * Nosotros no escribimos texels: resolvemos la EDRAM a una textura de D3D.
 * Asi que hay que recordar la asociacion `dest -> IDirect3DTexture9*` para
 * que `GX_LoadTexObj` sirva la textura resuelta en vez de intentar
 * destilar el buffer de RAM, que no contiene nada.
 *
 * Es el equivalente de lo que `fb_gln64.cpp` ya hace para Rice, solo que
 * alli la asociacion va por direccion de RDRAM y aqui por el puntero del
 * propio buffer del plugin.
 *
 * ★★ PERDER UNA ENTRADA DE ESTA TABLA ES UN NEGRO SILENCIOSO.
 *
 * Si el LRU desaloja una entrada viva, `gxd3d_find_resolved` falla y
 * `GX_LoadTexObj` se va al camino NORMAL: destilar el buffer del plugin,
 * **que nosotros no hemos escrito nunca**.  Antes eso daba basura reciclada
 * del monton de 2,4 MB; con el monton de 32 MB recien reservado da CEROS, o
 * sea negro.  Por eso hay contador (`rttdesal`): un negro que no avisa es lo
 * que mas caro ha salido en este port.
 *
 * ★ Y 16 era un numero de la Wii, no nuestro.  La referencia -- GLideN64,
 * `FrameBuffer.h:189-190`-- guarda los framebuffers en un `std::list` SIN
 * TOPE, y Paper Mario declara al menos seis color image distintos mas los
 * auxiliares ([CIMG-GX]).  Aqui una entrada son 4 bytes de descriptor mas la
 * textura de D3D, que existiria igual: subirlo no cuesta memoria nueva
 * apreciable y quita una causa de negro. */
#define GXD3D_RTT_MAX 64

struct GXD3DResolved
{
    void *key;                     /* el `dest` que paso GX_CopyTex */
    IDirect3DTexture9 *tex;
    u16 w, h;
    u32 serial;
    u8  mono;                      /* GXD3D_SetCopyMono: framebuffer de 8 bits */
};
static GXD3DResolved s_rtt[GXD3D_RTT_MAX];
static u32 s_rttSerial = 0;
static int s_rttVivas  = 0;

/* Da de baja la entrada cuyo `dest` acaba de liberar el plugin.  La llama
 * `gxd3d_tex_free_hook`; el porque largo esta alli.
 *
 * Barrido lineal y no hash: son 64 entradas y esto solo corre en los
 * `__lwp_heap_free` del monton de texturas, que son raros -- y con
 * `s_rttVivas` a 0 ni eso.  A diferencia de `s_texReg` aqui NO hace falta
 * lapida: la busqueda es lineal, no una cadena de sondeo, asi que un hueco
 * no corta nada. */
static void gxd3d_rtt_olvida(void *ptr)
{
    int i;
    if (!s_rttVivas) return;
    for (i = 0; i < GXD3D_RTT_MAX; i++)
    {
        if (s_rtt[i].key != ptr) continue;
        if (s_rtt[i].tex) d3dq_soltar((D3DResource *)s_rtt[i].tex);
        s_rtt[i].key = NULL;
        s_rtt[i].tex = NULL;
        s_rtt[i].w   = 0;
        s_rtt[i].h   = 0;
        s_rtt[i].mono = 0;
        s_rttVivas--;
        return;
    }
}

/* Ver gxshim/gccore.h.  Se llama DESPUES del GX_CopyTex: la entrada ya
 * existe, y si no (copia fallida) no hay nada que marcar. */
void GXD3D_SetCopyMono(void *dest, u8 mono)
{
    int i;
    if (!dest) return;
    for (i = 0; i < GXD3D_RTT_MAX; i++)
        if (s_rtt[i].key == dest) { s_rtt[i].mono = mono ? 1 : 0; return; }
}

static u8 gxd3d_resolved_mono(void *key)
{
    int i;
    for (i = 0; i < GXD3D_RTT_MAX; i++)
        if (s_rtt[i].key == key) return s_rtt[i].mono;
    return 0;
}

IDirect3DTexture9 *gxd3d_find_resolved(void *key)
{
    int i;
    if (!key) return NULL;
    for (i = 0; i < GXD3D_RTT_MAX; i++)
        if (s_rtt[i].key == key) return s_rtt[i].tex;
    return NULL;
}

static GXD3DResolved *gxd3d_rtt_slot(void *key, u16 w, u16 h)
{
    int i, lru = 0;
    u32 oldest = 0xFFFFFFFFu;
    for (i = 0; i < GXD3D_RTT_MAX; i++)
        if (s_rtt[i].key == key) return &s_rtt[i];
    for (i = 0; i < GXD3D_RTT_MAX; i++)
    {
        if (s_rtt[i].key == NULL) { lru = i; break; }
        if (s_rtt[i].serial < oldest) { oldest = s_rtt[i].serial; lru = i; }
    }
    /* ★ Se le quita la entrada a OTRO `dest` que todavia estaba vivo: a
     * partir de aqui su `GX_LoadTexObj` se ira al camino normal y sacara
     * negro.  Ver la nota de GXD3D_RTT_MAX. */
#if XBOX64_TRAZAS
    if (s_rtt[lru].key && s_rtt[lru].key != key)
        gxd3d_dbg_rttdesal++;
    else if (!s_rtt[lru].key)
        s_rttVivas++;
#else
    if (!s_rtt[lru].key)       /* mismo efecto sin el contador */
        s_rttVivas++;
#endif

    /* Reciclar: si el tamano no coincide hay que recrear la textura. */
    if (s_rtt[lru].tex && (s_rtt[lru].w != w || s_rtt[lru].h != h))
    {
        d3dq_soltar((D3DResource *)s_rtt[lru].tex);
        s_rtt[lru].tex = NULL;
    }
    if (s_rtt[lru].key != key)
        s_rtt[lru].mono = 0;       /* la marca era del `dest` anterior */
    s_rtt[lru].key = key;
    s_rtt[lru].w = w;
    s_rtt[lru].h = h;
    return &s_rtt[lru];
}

static void gxd3d_resolve_to_gx_buffer(void *dest, u16 sl, u16 st, u16 sw, u16 sh,
                                       u16 dw, u16 dh, u32 fmt)
{
    GXD3DResolved *slot;
    D3DRECT r;
#if XBOX64_TRAZAS
    const unsigned long long tRtt = __mftb();
#endif

    (void)fmt;   /* el formato GX del destino da igual: lo muestrea el shader */
    if (!dest || dw == 0 || dh == 0) return;

    slot = gxd3d_rtt_slot(dest, dw, dh);
    if (!slot->tex)
    {
        /* ★ TILEADA a proposito (D3DFMT_A8R8G8B8, no LIN_): `Resolve` de
         * Xenos escribe SIEMPRE tileado aunque el destino se declare
         * LIN_*, y declararlo lineal solo consigue que el muestreador lo
         * lea mal.  Medido, ver reference_xbox360_resolve_siempre_tilea. */
        if (FAILED(g_pd3dDevice->CreateTexture(dw, dh, 1, 0, D3DFMT_A8R8G8B8,
                                               D3DPOOL_DEFAULT, &slot->tex, NULL)))
        {
            slot->tex = NULL;
            return;
        }
    }
    slot->serial = ++s_rttSerial;

    /* ★ RECT DE ORIGEN EXPLICITO, NUNCA NULL: con NULL el destino se queda
     * SIN TOCAR si el render target y la textura no miden lo mismo -- y para
     * un color image estrecho nunca miden lo mismo.  Ese fallo exacto dejo
     * en negro el muneco de Link del menu de OoT y costo varias sesiones
     * (ver project_xenon64_framebuffer_gln64, "resolvia el render target
     * ENTERO, no la region del CI"). */
    r.x1 = sl;  r.y1 = st;
    r.x2 = sl + (sw ? sw : dw);
    r.y2 = st + (sh ? sh : dh);

    /* [etapa 3] Aqui habia un `d3dq_sinc()`.  Sobra: el `Resolve` va por el
     * grabador, o sea que es una orden mas y se ejecuta DESPUES de todos los
     * dibujos que la preceden, que es justo lo que se queria garantizar.  Y
     * en la etapa 4 no podria estar: el productor no es quien vacia. */
    g_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, &r, slot->tex,
                          NULL, 0, 0, NULL, 0.0f, 0, NULL);

#if XBOX64_TRAZAS
    gxsh_rtt_tb += (unsigned int)(__mftb() - tRtt);
    gxsh_rtt_n++;
#endif
}

/* Lo llama FrameBuffer_ActivateBufferTexture (glN64_GX/FrameBuffer.cpp) con
 * el buffer de texels del framebuffer que va a activar.  Ver la nota de
 * gxd3d_dbg_fbmiss.  Sonda pura: sin trazas, vacia. */
extern "C" void gxd3d_fb_activa(void *gxtex)
{
#if XBOX64_TRAZAS
    ++gxd3d_dbg_fbact;
    if (!gxtex || !gxd3d_find_resolved(gxtex))
        ++gxd3d_dbg_fbmiss;
#else
    (void)gxtex;
#endif
}

/* ------------------------------------------------------------------ */
/* ★★ LA COPIA DE PROFUNDIDAD DEL EFB (Wii64 443f68b, DepthCopy.cpp)    */
/* ------------------------------------------------------------------ */
/* En la Wii, `GX_CopyTex` con formato GX_TF_Z16 escribe en `dest` la
 * profundidad del EFB reducida a la mitad (filtro de caja), en el formato de
 * textura de GX: bloques de 4x4 texels de 16 bits, y cada texel es
 * `z24 >> 8` con sus dos bytes cambiados (lo dice DepthCopy.cpp).  glN64 lo
 * lee POR CPU al empezar la lista siguiente, despues de `GX_DrawDone()`, y lo
 * convierte a la profundidad del N64 en la RDRAM: el sol y los brillos de
 * OoT y Majora, y la camara y el sol de Donkey Kong 64.
 *
 * Aqui la profundidad vive en la EDRAM de Xenos, y la Wii no se puede
 * imitar al pie de la letra: alli `GX_DrawDone` espera a una GPU que lleva
 * desde el final de la lista anterior con la copia hecha; aqui las ordenes
 * las ejecuta `retro_run` hasta un frame despues.
 *
 * ★★ [log 22, DK64] LA PRIMERA VERSION ESPERABA COMO LA WII, Y NO SIRVE:
 *     `GX_DrawDone` esperaba al consumidor, y el consumidor a la GPU
 *     (`BlockUntilIdle`, hasta 16 ms) y luego leia la textura de D3D, que
 *     esta en paginas de 16 MB SIN vista cacheada (`sincache` en todas):
 *     11,5 ms por copia.  El plugin esperaba 25-30 ms en cada lista y el
 *     juego bajo de 60 a 20 imagenes por segundo.
 *     Lo que si confirmo: el D24S8 de Xenos tiene la Z en los 24 bits altos
 *     (`raw esq=FFFFFE00` tras el borrado, centro F8D0B300...).
 *
 * Ahora, SIN NINGUNA ESPERA:
 *   1. productor (`gxd3d_copia_z`): `Resolve(D3DRESOLVE_DEPTHSTENCIL)` a uno
 *      de tres HUECOS por turno.  Cada hueco es una textura D24S8 con
 *      cabecera propia (`XGSetTextureHeader`) sobre memoria fisica CACHEADA
 *      de paginas de 64 KB (`XPhysicalAlloc`), que la CPU lee deprisa.  Detras
 *      va la orden QOP_MarcaProfundidad.
 *   2. consumidor: esa orden pone una FENCE (`gxd3d_consumidor_marca_z`), y
 *      en cada SWAP mira, sin bloquear, que fences ya han pasado
 *      (`gxd3d_consumidor_revisa_z`).
 *   3. productor, en `GX_DrawDone` (`gxd3d_entrega_z`): si hay una copia
 *      terminada mas nueva que la ultima entregada, la convierte a `dest` en
 *      el formato EXACTO de GX (bloques de 4x4, `z24 >> 8` con los bytes
 *      cambiados, media de 2x2).  DepthCopy.cpp no cambia.
 * El precio: la profundidad que ve el juego tiene 1-3 frames en vez de 1
 * (`retraso` en [ZCOPIA], en copias).  Si no hay ninguna nueva, `dest` se
 * queda con la anterior (`sindato`).
 *
 * La memoria cacheada no es coherente con lo que escribe la GPU: antes de
 * leer un hueco se tiran sus lineas con `__dcbf` (la CPU nunca las escribe:
 * estan limpias y dcbf solo las invalida).
 *
 * Solo el caso que usa DepthCopy.cpp: ancho de destino multiplo de 4 y mitad
 * exacta del origen.  En vertical, el doble exacto es la caja de GX; [1080
 * Snowboarding] 238 filas del N64 ocupan 480 del EFB (escala 2,017) y el alto
 * no es multiplo de 4: se toma el par de filas mas cercano al centro de cada
 * fila y el ultimo bloque de 4x4 va a medias (DepthCopy.cpp no lee de mas).
 * Lo demas se cuenta en `mal` y no se copia. */
#define GXD3D_Z_HUECOS 3u
#define GXD3D_Z_CAP    (1280u * 1024u)   /* 640x480x4 = 1.228.800 cabe */
/* [Mario Kart 64, Banjo-Kazooie] La copia del COLOR a la RDRAM usa la misma
 * maquinaria (hueco, fence, entrega sin esperas) en los huecos que siguen a los
 * de Z: GXD3D_Z_HUECOS .. GXD3D_HUECOS-1.  Ver "LA COPIA DEL COLOR A LA RDRAM"
 * mas abajo.  Las dos colas llevan su propia secuencia; un hueco solo es de una. */
#define GXD3D_C_HUECOS 3u
#define GXD3D_HUECOS   (GXD3D_Z_HUECOS + GXD3D_C_HUECOS)

struct GXD3DZHueco
{
    D3DTexture   hdr;        /* cabecera propia, apunta a `mem`              */
    u8          *mem;        /* texels: memoria fisica cacheada, 64 KB pags   */
    u16          w, h;       /* lo resuelto (el doble que la copia)           */
    u16          ox, oy;     /* esquina del EFB resuelta (GX_PeekZ)           */
    volatile u32 pedida;     /* productor: ultima copia resuelta aqui         */
    DWORD        fence;      /* consumidor                                    */
    u32          fenceSeq;   /* consumidor: a que copia pertenece `fence`     */
    volatile u32 lista;      /* consumidor: copia cuya fence ya paso          */
};
static GXD3DZHueco s_zHueco[GXD3D_HUECOS];
static int  s_zMemOk     = 0;   /* 0 sin reservar, 1 ok, -1 sin memoria */
static u32  s_zSeq       = 0;   /* productor: ultima copia pedida       */
static u32  s_zEntregada = 0;   /* productor: ultima copia convertida   */
static void *s_zDest     = NULL;
static u16  s_zDw = 0, s_zDh = 0;

/* Medidas, por segundo (las escribe el productor salvo `marcas`). */
static volatile u32 s_stZPedidas = 0, s_stZEntregadas = 0, s_stZSinDato = 0;
static volatile u32 s_stZRetraso = 0, s_stZConvTb = 0, s_stZMarcas = 0;
static volatile u32 s_stZMal = 0, s_stZSinMem = 0;
static volatile u32 s_stZRawEsq = 0, s_stZRawCen = 0;
static volatile u32 s_stZMin = 0xFFFFFFFFu, s_stZMax = 0;

/* ★ [Banjo-Kazooie, 2026-10-24] INTERRUPTOR DE LA COPIA DEL COLOR SINCRONA
 * (GLideN64 ctSync).  A 0 desaparece todo su codigo y
 * GXD3D_CopiaColorARDRAMSincrona se queda en pedir la copia de siempre (1-3
 * frames tarde).  Ver "LA COPIA DEL COLOR SINCRONA", tras
 * GXD3D_EntregaColorARDRAM.  Para quitarla del todo: este bloque, el `if` de
 * gxd3d_consumidor_marca_z, esa funcion, su linea en [CCOPIA], su
 * declaracion en gccore.h y hack_copyToRDRAMSync (Config.h, RSP.cpp, gDP.cpp). */
#define GXD3D_CCOPIA_SINC    1
#define GXD3D_CCOPIA_SINC_MS 100u   /* tope de espera; pasado, la de siempre */
#if GXD3D_CCOPIA_SINC
/* Huecos de color cuya fence el consumidor espera EN EL ACTO al marcarla.  Lo
 * pone el productor antes de encolar la marca (la barrera de d3dq_mete). */
static volatile u8 s_cSincHueco[GXD3D_HUECOS];
static HANDLE      s_cSincEv = NULL;   /* el consumidor avisa: hueco listo */
#endif

/* Consumidor: la fence de la copia `seq`, recien resuelta en el hueco `k`. */
void gxd3d_consumidor_marca_z(IDirect3DDevice9 *dev, unsigned int k, unsigned int seq)
{
    if (dev && k < GXD3D_HUECOS)   /* los de Z y los de color */
    {
        s_zHueco[k].fence    = dev->InsertFence();
        s_zHueco[k].fenceSeq = seq;
#if GXD3D_CCOPIA_SINC
        /* La copia sincrona: el productor esta parado esperandola.  Aqui
         * mismo, sin esperar al SWAP (que no llegara: el productor no lo
         * manda hasta que se le suelte). */
        if (s_cSincHueco[k])
        {
            dev->BlockOnFence(s_zHueco[k].fence);
            __lwsync();
            s_zHueco[k].lista = seq;
            if (s_cSincEv)
                SetEvent(s_cSincEv);
        }
#endif
#if XBOX64_TRAZAS
        if (k < GXD3D_Z_HUECOS)
            s_stZMarcas++;
#endif
    }
}

/* Consumidor, en cada SWAP (y sin hilo, al final de cada d3dq_drena): que
 * copias ha terminado ya la GPU.  No bloquea. */
void gxd3d_consumidor_revisa_z(IDirect3DDevice9 *dev)
{
    unsigned int k;
    if (!dev)
        return;
    for (k = 0; k < GXD3D_HUECOS; k++)   /* los de Z y los de color */
    {
        GXD3DZHueco *h = &s_zHueco[k];
        if (h->fenceSeq && h->lista != h->fenceSeq && !dev->IsFencePending(h->fence))
        {
            __lwsync();
            h->lista = h->fenceSeq;
        }
    }
}

/* ★★ [logs 23-25] EL RELOJ DE `conv` ES QueryPerformanceCounter, NO __mftb.
 *
 * `conv` salia 2^32/n.  La sonda [ZCOPIA2] del log 25 lo resolvio: cada
 * medida daba 0 o -1 tick, con el acumulador limpio (no lo pisaba nadie).  O
 * sea que las DOS lecturas del reloj acababan juntas, sin el bucle en medio:
 * el compilador trata `__mftb` como algo sin efectos y lo mueve -- y meterlo
 * en una funcion `noinline` (log 24) no basto, porque al ser `static` ve
 * dentro y sabe que no toca memoria.
 * QueryPerformanceCounter es una llamada al sistema: el compilador no sabe
 * que hace, y no puede mover a traves de ella el bucle, que escribe en
 * memoria.  Cuenta en ticks de time base (`QueryPerformanceFrequency`),
 * igual que `__mftb`: 50 por us. */
#if XBOX64_TRAZAS   /* solo lo usan las medidas de [ZCOPIA]/[CCOPIA] */
static unsigned long long gxd3d_reloj(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (unsigned long long)t.QuadPart;
}
#endif

/* ★ [Excitebike 64, 2026-10-23] LA ESCALA HORIZONTAL LIBRE.  VI de 512 en un
 * EFB de 640 (escala 1,25): no hay caja de 2x2 que valga, y una direccion
 * tileada por pixel cuesta ~9 ms por copia (log 26).  Se toma la muestra mas
 * cercana al CENTRO de cada pixel del N64 (en vertical, como el par de 1080)
 * y su direccion sale de dos tablas:
 *
 *     XGAddress2DTiledOffset(sx, sy) = fila[y] + col[(sy >> 3) & 1][x]
 *     fila[y] = XG(0, sy),   col[b][x] = XG(sx, 8b) - XG(0, 8b)
 *
 * Exacto, de la formula de xgraphics.h:536-560 con 4 bytes por texel: en
 * `Offset` x solo pone los bits {2,3,5} y la fila {4,6,7,8} (Micro mezcla
 * (x & 7) << 2 con (y & 6) << 4, bits distintos), y `Macro` es multiplo de
 * 512, asi que se suma sin acarreos y el barajado final es lineal; la fila
 * solo entra en la parte de x por `((y & 8) >> 2) + (x >> 3)`, de ahi las dos
 * tablas por el bit 3.  Una vez por tamano (sw/sh/dw/dh).  Solo el productor
 * (gxd3d_entrega_z). */
#define GXD3D_ZT_MAXW 1280u
#define GXD3D_ZT_MAXH 1024u
static u32 s_ztCol[2][GXD3D_ZT_MAXW];
static u32 s_ztFila[GXD3D_ZT_MAXH];
static u8  s_ztFilaB[GXD3D_ZT_MAXH];
static unsigned int s_ztSw = 0, s_ztSh = 0, s_ztDw = 0, s_ztDh = 0;

static void gxd3d_zt_prepara(unsigned int sw, unsigned int sh, unsigned int dw, unsigned int dh)
{
    unsigned int x, y, b;

    if (sw == s_ztSw && sh == s_ztSh && dw == s_ztDw && dh == s_ztDh)
        return;
    s_ztSw = sw;  s_ztSh = sh;  s_ztDw = dw;  s_ztDh = dh;

    for (y = 0; y < dh; y++)
    {
        const unsigned int sy = ((2u * y + 1u) * sh) / (2u * dh);
        s_ztFila[y]  = XGAddress2DTiledOffset(0, sy, sw, 4);
        s_ztFilaB[y] = (u8)((sy >> 3) & 1u);
    }
    for (b = 0; b < 2u; b++)
    {
        const u32 cero = XGAddress2DTiledOffset(0, 8u * b, sw, 4);
        for (x = 0; x < dw; x++)
        {
            const unsigned int sx = ((2u * x + 1u) * sw) / (2u * dw);
            s_ztCol[b][x] = XGAddress2DTiledOffset(sx, 8u * b, sw, 4) - cero;
        }
    }
}

/* Productor: convierte el hueco `h` (ya terminado) a `dest` en formato GX. */
static void gxd3d_convierte_z(GXD3DZHueco *h, u16 *out, unsigned int dw, unsigned int dh)
{
#if XBOX64_TRAZAS   /* reloj y min/max: solo los lee [ZCOPIA] */
    const unsigned long long t0 = gxd3d_reloj();
    u32 zmin = 0xFFFFFFFFu, zmax = 0;
#endif
    const unsigned int sw = h->w, sh = h->h;
    const unsigned int aw = (sw + 31u) & ~31u;   /* tiles de 32x32 */
    const unsigned int ah = (sh + 31u) & ~31u;
    const unsigned int bytes = aw * ah * 4u;
    const u32 *t = (const u32 *)h->mem;
    unsigned int off, x, y;

    for (off = 0; off < bytes; off += 128u)
        __dcbf((int)off, h->mem);
    __sync();

#if XBOX64_TRAZAS
    s_stZRawEsq = t[XGAddress2DTiledOffset(0, 0, sw, 4)];
    s_stZRawCen = t[XGAddress2DTiledOffset(sw / 2u, sh / 2u, sw, 4)];
#endif

    /* ★★ [log 26] UNA DIRECCION TILEADA POR CADA 4 PIXELES, NO POR PIXEL.
     * Con una por pixel `conv` salio en ~9 ms por copia, dentro de la lista:
     * el emulador esperaba al DP 250-400 ms de cada segundo.
     *
     * De la formula de XGAddress2DTiledOffset (xgraphics.h) con 4 bytes por
     * texel, para un grupo de 8 texels de una fila alineado a 8 (x0 = 8g):
     *   - la base es la de x0; los ocho estan en base + {0,1,2,3,8,9,10,11}:
     *     `x & 3` suma 4 bytes y `x & 4` suma 32, los dos dentro de
     *     `Offset & 63` y sin acarreo (lo maximo ahi es 12+16+32 = 60);
     *   - la fila siguiente (y par -> y+1) suma 4 texels: es el `(y & 1) << 4`
     *     y no toca ningun otro termino.
     *   - `x >> 3` (el macro y el `& 3` del entrelazado) es el mismo en
     *     todo el grupo.
     * Cada pixel de salida `i` del grupo es el 2x2 de los texels 2i y 2i+1:
     * desplazamientos `kA[i]` = {0,2,8,10}, mas +1, +4 y +5.
     * Solo con el doble exacto en horizontal; si no, las tablas de abajo. */
    if (sw == 2u * dw)
    {
        static const unsigned int kA[4] = { 0u, 2u, 8u, 10u };
        const unsigned int grupos = dw >> 2;   /* dw es multiplo de 4 */
        for (y = 0; y < dh; y++)
        {
            u16 *fila = out + (((y >> 2) * grupos) << 4) + ((y & 3u) << 2);
            /* La fila PAR del hueco cuyo par (sy, sy+1) cubre el centro de la
             * fila `y`.  Con el doble exacto (sh = 2*dh) sale 2y, la caja de
             * GX de siempre; con otra escala (1080: 480/238), la mas cercana.
             * Par por el `(y & 1) << 4` de los desplazamientos de abajo. */
            unsigned int sy = (((2u * y + 1u) * sh) / (2u * dh)) & ~1u;
            if (sy + 1u >= sh)
                sy = (sh - 2u) & ~1u;
            for (x = 0; x < grupos; x++)
            {
                const unsigned int base = XGAddress2DTiledOffset(x * 8u, sy, sw, 4);
                u16 *bloque = fila + (x << 4);
                unsigned int i;
                for (i = 0; i < 4u; i++)
                {
                    const unsigned int o = base + kA[i];
                    /* El filtro de caja de la copia de GX: la media de 2x2. */
                    const u32 z = ((t[o] >> 8) + (t[o + 1u] >> 8) +
                                   (t[o + 4u] >> 8) + (t[o + 5u] >> 8) + 2u) >> 2;
#if XBOX64_TRAZAS
                    if (z < zmin) zmin = z;
                    if (z > zmax) zmax = z;
#endif
                    /* Bloques de 4x4, y el texel es z24 >> 8 con los bytes
                     * cambiados: byte bajo = bits 23..16, alto = 15..8. */
                    bloque[i] = (u16)((((z >> 8) & 0xFFu) << 8) | ((z >> 16) & 0xFFu));
                }
            }
        }
    }
    else
    {
        /* [Excitebike 64] Escala horizontal libre: la muestra mas cercana al
         * centro, por las tablas de gxd3d_zt_prepara.  El mismo formato de
         * salida (bloques de 4x4, z24 >> 8 con los bytes cambiados). */
        const unsigned int grupos = dw >> 2;   /* dw es multiplo de 4 */
        gxd3d_zt_prepara(sw, sh, dw, dh);
        for (y = 0; y < dh; y++)
        {
            u16 *fila = out + (((y >> 2) * grupos) << 4) + ((y & 3u) << 2);
            const u32 *t0 = t + s_ztFila[y];
            const u32 *col = s_ztCol[s_ztFilaB[y]];
            for (x = 0; x < dw; x++)
            {
                const u32 z = t0[col[x]] >> 8;
#if XBOX64_TRAZAS
                if (z < zmin) zmin = z;
                if (z > zmax) zmax = z;
#endif
                fila[((x >> 2) << 4) + (x & 3u)] =
                    (u16)((((z >> 8) & 0xFFu) << 8) | ((z >> 16) & 0xFFu));
            }
        }
    }

#if XBOX64_TRAZAS
    s_stZMin = (zmin < s_stZMin) ? zmin : s_stZMin;
    s_stZMax = (zmax > s_stZMax) ? zmax : s_stZMax;
    s_stZConvTb += (u32)(gxd3d_reloj() - t0);
#endif
}

static void gxd3d_copia_z(void *dest, u16 sl, u16 st, u16 sw, u16 sh, u16 dw, u16 dh)
{
    D3DRECT r;

    /* [1080] El alto ya no tiene que ser multiplo de 4 ni la mitad exacta:
     * ver la nota de arriba.  `sh >= 2` para el par de filas.
     * [Excitebike 64] Ni el ancho el doble exacto: sin el, las tablas de
     * gxd3d_zt_prepara, que tienen su tope. */
    if (!dest || !dw || !dh || (dw & 3u) ||
        sw < dw || sh < 2u || sh < dh ||
        (sw != (u16)(dw * 2u) && (dw > GXD3D_ZT_MAXW || dh > GXD3D_ZT_MAXH)))
    {
#if XBOX64_TRAZAS
        s_stZMal++;
#endif
        return;
    }

    /* Los tres huecos, una vez por vida del core: nunca se liberan, porque
     * la GPU puede tener un resolve en vuelo hacia cualquiera de ellos. */
    if (s_zMemOk == 0)
    {
        unsigned int k;
        s_zMemOk = 1;
        for (k = 0; k < GXD3D_Z_HUECOS; k++)
        {
            s_zHueco[k].mem = (u8 *)XPhysicalAlloc(GXD3D_Z_CAP, MAXULONG_PTR, 0,
                                                   PAGE_READWRITE | MEM_LARGE_PAGES);
            if (!s_zHueco[k].mem)
                s_zMemOk = -1;
        }
    }
    if (s_zMemOk < 0)
    {
#if XBOX64_TRAZAS
        s_stZSinMem++;
#endif
        return;
    }

    {
        const unsigned int k = s_zSeq % GXD3D_Z_HUECOS;
        GXD3DZHueco *h = &s_zHueco[k];

        if (h->w != sw || h->h != sh)
        {
            UINT base = 0, mip = 0;
            XGSetTextureHeader(sw, sh, 1, 0, D3DFMT_D24S8, (D3DPOOL)0, 0,
                               XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, &h->hdr, &base, &mip);
            if (base > GXD3D_Z_CAP)
            {
                h->w = h->h = 0;
#if XBOX64_TRAZAS
                s_stZMal++;
#endif
                return;
            }
            XGOffsetResourceAddress(&h->hdr, h->mem);
            h->w = sw;
            h->h = sh;
        }

        s_zSeq = s_zSeq + 1;
        h->pedida = s_zSeq;   /* hasta su fence, el hueco no se lee */
        s_zDest = dest;
        s_zDw = dw;
        s_zDh = dh;
#if XBOX64_TRAZAS
        s_stZPedidas++;
#endif

        r.x1 = sl;       r.y1 = st;
        r.x2 = sl + sw;  r.y2 = st + sh;
        g_pd3dDevice->Resolve(D3DRESOLVE_DEPTHSTENCIL, &r, (D3DBaseTexture *)&h->hdr,
                              NULL, 0, 0, NULL, 0.0f, 0, NULL);
#if D3D_GRABADOR
        g_pd3dDevice->MarcaProfundidad(k, s_zSeq);
#else
        gxd3d_consumidor_marca_z(g_pd3dDevice, k, s_zSeq);
#endif
    }
}

/* Productor, en `GX_DrawDone`: la copia terminada mas nueva, a `dest`.  NO
 * ESPERA: si no hay ninguna nueva, `dest` se queda como estaba (`sindato`).
 * Un hueco se lee solo si su fence ya paso para la ULTIMA copia resuelta en
 * el (`lista == pedida`); mientras se lee no se puede resolver otra encima,
 * porque las dos cosas las hace este mismo hilo. */
static void gxd3d_entrega_z(void)
{
    GXD3DZHueco *mejor = NULL;
    unsigned int k;

    if (s_zSeq == s_zEntregada || !s_zDest)
        return;
#if !D3D_GRABADOR
    /* Sin grabador no hay SWAP en la cola que lo revise: aqui mismo. */
    gxd3d_consumidor_revisa_z(g_pd3dDevice);
#endif
    for (k = 0; k < GXD3D_Z_HUECOS; k++)
    {
        GXD3DZHueco *h = &s_zHueco[k];
        const u32 lista = h->lista;
        if (lista && lista == h->pedida && (int)(lista - s_zEntregada) > 0 &&
            (!mejor || (int)(lista - mejor->lista) > 0))
            mejor = h;
    }
    if (!mejor)
    {
#if XBOX64_TRAZAS
        s_stZSinDato++;
#endif
        return;
    }
    __lwsync();   /* la fence antes que los texels */
    /* [Excitebike 64] Lo mismo que admite gxd3d_copia_z: el doble exacto en
     * horizontal, o cualquier ancho >= el de destino dentro de las tablas. */
    if (mejor->w >= s_zDw && mejor->h >= 2u && mejor->h >= s_zDh &&
        (mejor->w == (u16)(s_zDw * 2u) || (s_zDw <= GXD3D_ZT_MAXW && s_zDh <= GXD3D_ZT_MAXH)))
    {
        gxd3d_convierte_z(mejor, (u16 *)s_zDest, s_zDw, s_zDh);
#if XBOX64_TRAZAS
        s_stZEntregadas++;
        s_stZRetraso += s_zSeq - mejor->lista;
#endif
    }
#if XBOX64_TRAZAS
    else
        s_stZMal++;
#endif
    s_zEntregada = mejor->lista;
}

/* ------------------------------------------------------------------ */
/* ★★ LA COPIA DEL COLOR A LA RDRAM (GLideN64 copyToRDRAM)              */
/* ------------------------------------------------------------------ */
/* [Mario Kart 64, Banjo-Kazooie] GLideN64 "Frame buffer emulation part II"
 * (blog, 2014), "Emulation of CPU based frame buffer effects": la CPU LEE el
 * color buffer de la RDRAM -- Mario Kart copia un trozo para la pantalla de
 * TV del circuito; Banjo, la imagen para las piezas del puzzle --, asi que el
 * plugin tiene que volcar a la RDRAM lo que dibujo en la GPU.  GLideN64 lo hace
 * en gDPFullSync (gDP.cpp:1091-1095) con el buffer principal, y su
 * GLideN64.custom.ini lo pone por juego: [MARIOKART64] copyToRDRAM=2 (doble
 * buffer: llega un frame tarde) y [BANJO-KAZOOIE] copyToRDRAM=1 (e8a5e829,
 * "Fix Banjo-Kazooie puzzle transitions").
 *
 * Aqui no se puede esperar a la GPU (las ordenes las ejecuta `retro_run` hasta
 * un frame despues), asi que va como la copia de Z de arriba, SIN ESPERAS:
 *   1. productor (GXD3D_PideColorARDRAM, en el fullsync): Resolve del EFB a un
 *      hueco A8R8G8B8 en memoria fisica cacheada y la fence de siempre
 *      (QOP_MarcaProfundidad sirve igual: indexa el hueco);
 *   2. consumidor: fence y revision en el SWAP, las mismas funciones;
 *   3. productor (GXD3D_EntregaColorARDRAM, en el fullsync siguiente): la
 *      copia terminada mas nueva se reduce a la resolucion del N64 leyendo del
 *      hueco tileado el pixel central de cada celda (sin destilarlo: ver la
 *      nota del log 3 en la funcion) y se escribe en la RDRAM en RGBA5551 en
 *      la direccion de SU buffer.
 * La imagen que ve el juego es de 1-3 frames antes: el doble buffer de Mario
 * Kart ya es 1; para el puzzle de Banjo es la escena de un instante antes.
 *
 * SIN MEDIR: el orden de canales al leer el A8R8G8B8 por CPU.  Se lee como
 * u32 0xAARRGGBB, que es lo que da el D24S8 por CPU en la copia de Z (Z en los
 * 24 bits altos, medido).  Si la TV o el puzzle salen con los colores
 * cambiados, es esto. */
struct GXD3DCopiaColor
{
    u16 *dest;          /* en la RDRAM real (no en la foto de la lista)     */
    u32  dir;           /* su direccion N64, para devolverla al plugin       */
    u16  n64w, n64h;
};
static GXD3DCopiaColor s_cInfo[GXD3D_C_HUECOS];
static int  s_cMemOk = 0;          /* 0 sin reservar, 1 ok, -1 sin memoria */
static u32  s_cSeq = 0, s_cEntregada = 0;
static volatile u32 s_stCPedidas = 0, s_stCEntregadas = 0, s_stCSinDato = 0;
static volatile u32 s_stCRetraso = 0, s_stCConvTb = 0, s_stCMal = 0, s_stCSinMem = 0;
/* [logs 2-3] El reparto de `conv`: vaciado de cache y muestreo (lectura de
 * los texels tileados + conversion a RGBA5551). */
static volatile u32 s_stCFlushTb = 0, s_stCRedTb = 0;
static u16 s_cSx[1280];             /* columna del EFB de cada columna N64  */
/* [Paper Mario] GLideN64 1f4d04f4, `hack_paper_mario_subscreen`
 * (ColorBufferToRDRAM::_RGBAtoRGBA16): "small increase of Blue component of
 * the color buffer does the trick" para el retraso del subscreen.  Lo pone el
 * plugin antes de cada peticion (GXD3D_ColorARDRAMAzulPM). */
static int s_cAzulPM = 0;

void GXD3D_ColorARDRAMAzulPM(int activo)
{
    s_cAzulPM = activo ? 1 : 0;
}

void GXD3D_PideColorARDRAM(u16 *dest, u32 dir, u16 x, u16 y, u16 w, u16 h,
                           u16 n64w, u16 n64h)
{
    const u32 rtW = (u32)xenon64_video_width();
    const u32 rtH = (u32)xenon64_video_height();
    D3DRECT r;

    if (!dest || !w || !h || !n64w || !n64h || (u32)x + w > rtW || (u32)y + h > rtH)
    {
#if XBOX64_TRAZAS
        s_stCMal++;
#endif
        return;
    }

    gxd3d_flush_pending();   /* lo dibujado tiene que entrar en el resolve */

    /* Los tres huecos, una vez por vida del core, como los de Z. */
    if (s_cMemOk == 0)
    {
        unsigned int k;
        s_cMemOk = 1;
        for (k = GXD3D_Z_HUECOS; k < GXD3D_HUECOS; k++)
        {
            s_zHueco[k].mem = (u8 *)XPhysicalAlloc(GXD3D_Z_CAP, MAXULONG_PTR, 0,
                                                   PAGE_READWRITE | MEM_LARGE_PAGES);
            if (!s_zHueco[k].mem)
                s_cMemOk = -1;
        }
    }
    if (s_cMemOk < 0)
    {
#if XBOX64_TRAZAS
        s_stCSinMem++;
#endif
        return;
    }

    {
        const unsigned int i = s_cSeq % GXD3D_C_HUECOS;
        const unsigned int k = GXD3D_Z_HUECOS + i;
        GXD3DZHueco *hu = &s_zHueco[k];

        if (hu->w != w || hu->h != h)
        {
            UINT base = 0, mip = 0;
            XGSetTextureHeader(w, h, 1, 0, D3DFMT_A8R8G8B8, (D3DPOOL)0, 0,
                               XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, &hu->hdr, &base, &mip);
            if (base > GXD3D_Z_CAP)
            {
                hu->w = hu->h = 0;
#if XBOX64_TRAZAS
                s_stCMal++;
#endif
                return;
            }
            XGOffsetResourceAddress(&hu->hdr, hu->mem);
            hu->w = w;
            hu->h = h;
        }

        s_cSeq = s_cSeq + 1;
        hu->pedida = s_cSeq;   /* hasta su fence, el hueco no se lee */
        s_cInfo[i].dest = dest;
        s_cInfo[i].dir  = dir;
        s_cInfo[i].n64w = n64w;
        s_cInfo[i].n64h = n64h;
#if XBOX64_TRAZAS
        s_stCPedidas++;
#endif

        r.x1 = x;      r.y1 = y;
        r.x2 = x + w;  r.y2 = y + h;
        g_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, &r, (D3DBaseTexture *)&hu->hdr,
                              NULL, 0, 0, NULL, 0.0f, 0, NULL);
#if D3D_GRABADOR
        g_pd3dDevice->MarcaProfundidad(k, s_cSeq);
#else
        gxd3d_consumidor_marca_z(g_pd3dDevice, k, s_cSeq);
#endif
    }
}

static int gxd3d_entrega_color_hueco(GXD3DZHueco *mejor, unsigned int iMejor,
                                     u32 *dir, u32 *bytes);

int GXD3D_EntregaColorARDRAM(u32 *dir, u32 *bytes)
{
    GXD3DZHueco *mejor = NULL;
    unsigned int k, iMejor = 0;

    if (s_cSeq == s_cEntregada)
        return 0;
#if !D3D_GRABADOR
    gxd3d_consumidor_revisa_z(g_pd3dDevice);
#endif
    for (k = GXD3D_Z_HUECOS; k < GXD3D_HUECOS; k++)
    {
        GXD3DZHueco *hu = &s_zHueco[k];
        const u32 lista = hu->lista;
        if (lista && lista == hu->pedida && (int)(lista - s_cEntregada) > 0 &&
            (!mejor || (int)(lista - mejor->lista) > 0))
        {
            mejor  = hu;
            iMejor = k - GXD3D_Z_HUECOS;
        }
    }
    if (!mejor)
    {
#if XBOX64_TRAZAS
        s_stCSinDato++;
#endif
        return 0;
    }
    return gxd3d_entrega_color_hueco(mejor, iMejor, dir, bytes);
}

/* La copia del hueco `mejor` (su fence ya paso) a la RDRAM, en la direccion
 * de SU buffer.  La usan la entrega de siempre y la sincrona de Banjo
 * (GXD3D_CopiaColorARDRAMSincrona). */
static int gxd3d_entrega_color_hueco(GXD3DZHueco *mejor, unsigned int iMejor,
                                     u32 *dir, u32 *bytes)
{
    __lwsync();   /* la fence antes que los texels */

    {
#if XBOX64_TRAZAS   /* relojes de [CCOPIA] */
        const unsigned long long t0 = gxd3d_reloj();
#endif
        const GXD3DCopiaColor *ci = &s_cInfo[iMejor];
        const u32 w = mejor->w, h = mejor->h;
        const u32 aw = (w + 31u) & ~31u, ah = (h + 31u) & ~31u;
        const u32 bytesHueco = aw * ah * 4u;
        u32 off, ox, oy;

#if XBOX64_TRAZAS
        unsigned long long t1;
#endif
        const u32 *t = (const u32 *)mejor->mem;

        if (ci->n64w > (u32)(sizeof(s_cSx) / sizeof(s_cSx[0])))
        {
#if XBOX64_TRAZAS
            s_stCMal++;
#endif
            s_cEntregada = mejor->lista;
            return 0;
        }

        /* Memoria cacheada que escribio la GPU: se tiran sus lineas antes de
         * leer (la CPU nunca las escribe; ver gxd3d_convierte_z). */
        for (off = 0; off < bytesHueco; off += 128u)
            __dcbf((int)off, mejor->mem);
        __sync();
#if XBOX64_TRAZAS
        t1 = gxd3d_reloj();
#endif

        /* ★ [log 3] SIN DESTILAR: SE LEEN DEL HUECO TILEADO SOLO LOS TEXELS
         * MUESTREADOS, con UNA direccion tileada por cada 8 columnas.
         *
         * Medido: con XGUntileSurface (log 2, el hueco entero) conv era 10,8
         * ms por copia; destilando solo las filas muestreadas (log 3), 4,1-5,2
         * ms de destilado, unos 14 ns por texel, para luego usar uno de cada
         * dos.  Es el mismo truco de gxd3d_convierte_z (log 26, ya validado):
         * con texels de 4 bytes, los 8 de una fila alineados a 8 estan en
         * `base + {0,1,2,3,8,9,10,11}`, y `base` es la de la columna x0 = 8g
         * de ESA fila.  Las columnas muestreadas crecen, asi que la base solo
         * se recalcula al cambiar de grupo. */
        {
            static const u32 kGrupo[8] = { 0u, 1u, 2u, 3u, 8u, 9u, 10u, 11u };
            const int azulPM = s_cAzulPM;
            for (ox = 0; ox < ci->n64w; ox++)
                s_cSx[ox] = (u16)(((2u * ox + 1u) * w) / (2u * ci->n64w));
            for (oy = 0; oy < ci->n64h; oy++)
            {
                const u32 sy = ((2u * oy + 1u) * h) / (2u * ci->n64h);
                u16 *dst = ci->dest + oy * ci->n64w;
                u32 grupoAnt = 0xFFFFFFFFu, base = 0;
                for (ox = 0; ox < ci->n64w; ox++)
                {
                    const u32 sx = s_cSx[ox];
                    const u32 g  = sx >> 3;
                    u32 v;
                    if (g != grupoAnt)
                    {
                        base = XGAddress2DTiledOffset(g << 3, sy, w, 4);
                        grupoAnt = g;
                    }
                    v = t[base + kGrupo[sx & 7u]];
                    {
                        /* GLideN64 1f4d04f4: con el hack, azul += 4 si
                         * 0 < azul <= 0xFB (antes de quedarse en 5 bits). */
                        u32 b = v & 0xFFu;
                        if (azulPM && b > 0x00u && b <= 0xFBu)
                            b += 0x04u;
                        dst[ox] = (u16)((((v >> 16) & 0xF8u) << 8) |   /* R -> 15..11 */
                                        (((v >>  8) & 0xF8u) << 3) |   /* G -> 10..6  */
                                        ((b & 0xF8u) >> 2) |           /* B ->  5..1  */
                                        1u);
                    }
                }
            }
        }
#if XBOX64_TRAZAS
        s_stCFlushTb += (u32)(t1 - t0);
        s_stCRedTb   += (u32)(gxd3d_reloj() - t1);
#endif

        s_cEntregada = mejor->lista;
#if XBOX64_TRAZAS
        s_stCEntregadas++;
        s_stCRetraso += s_cSeq - mejor->lista;
        s_stCConvTb  += (u32)(gxd3d_reloj() - t0);
#endif

        *dir   = ci->dir;
        *bytes = (u32)ci->n64w * ci->n64h * 2u;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* ★ LA COPIA DEL COLOR SINCRONA (GLideN64 ctSync, Banjo-Kazooie)       */
/* ------------------------------------------------------------------ */
/* [Banjo-Kazooie, 2026-10-24] GLideN64 e8a5e829 "Fix Banjo-Kazooie puzzle
 * transitions": [BANJO-KAZOOIE] copyToRDRAM=1 = ctSync, no el doble buffer de
 * su defecto.  Tras la carga (2 s sin listas) el juego pinta UN frame de la
 * escena nueva y la CPU lo lee de la RDRAM nada mas recibir el DP para hacer
 * las piezas: la copia de 1-3 frames tarde le daba lo de antes de la carga, y
 * las piezas se montaban negras.
 *
 * Igual que la de siempre (mismo hueco, misma marca), pero el productor se
 * queda esperando a que el consumidor (`retro_run`) ejecute el resolve y su
 * marca; la marca, con `s_cSincHueco`, espera a la GPU en el acto
 * (gxd3d_consumidor_marca_z).  Luego se convierte aqui mismo, en el buffer de
 * ESTE frame, antes del DP_INT.  Coste: el emulador no puede ir frames por
 * delante de la presentacion.  Si en GXD3D_CCOPIA_SINC_MS no llega, se vuelve
 * sin ella: queda pedida y la entrega de siempre la escribe despues.
 * Devuelve 1 con los bytes escritos (en `dir`), 0 si no se escribio nada.
 * Medido (logs (4)): en partida 30 img/s y ~11 ms de espera por copia. */

/* ★ [logs (2), 2026-10-24] SIN COLCHON DE FRAMES.  Con el de siempre
 * (HILO_MAX_SWAP 2) el emulador llega al fullsync con 3 frames sin presentar,
 * y la espera era la de presentarlos: 49 ms por copia, 15 img/s.  Con esto a
 * 1, xbox/hilo_emu.cpp usa HILO_MAX_SWAP_CCOPIA (0): el emulador no empieza un
 * VI hasta que se presento el anterior, asi que `retro_run` ya esta
 * dentro del frame cuando llega la copia y la espera es la de la GPU.  Lo
 * pone la primera copia sincrona; lo quita el cierre de la partida. */
#if GXD3D_CCOPIA_SINC
static volatile int s_cSincUsada = 0;
#endif

int gxd3d_ccopia_sinc_activa(void)
{
#if GXD3D_CCOPIA_SINC
    return s_cSincUsada;
#else
    return 0;
#endif
}

int GXD3D_CopiaColorARDRAMSincrona(u16 *dest, u32 dir, u16 x, u16 y, u16 w, u16 h,
                                   u16 n64w, u16 n64h, u32 *bytes)
{
#if GXD3D_CCOPIA_SINC
    const unsigned int i = s_cSeq % GXD3D_C_HUECOS;   /* el que usara la peticion */
    const unsigned int k = GXD3D_Z_HUECOS + i;
    const u32 antes = s_cSeq;
    GXD3DZHueco *hu = &s_zHueco[k];
    u32 seq, dirHecha = 0;
    DWORD t0;

    if (!s_cSincEv)
        s_cSincEv = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_cSincUsada = 1;
    s_cSincHueco[k] = 1;
    __lwsync();
#endif

    GXD3D_PideColorARDRAM(dest, dir, x, y, w, h, n64w, n64h);

#if GXD3D_CCOPIA_SINC
    if (s_cSeq == antes)
        return 0;   /* no se pidio (rect fuera, sin memoria...) */
    seq = s_cSeq;
#if D3D_GRABADOR >= 2
    /* Sin hilo, el consumidor es este mismo hilo: como d3dq_sincrona. */
    if (!g_d3dqVivo)
        d3dq_drena();
#endif
    t0 = GetTickCount();
    while (hu->lista != seq)
    {
        if (GetTickCount() - t0 > GXD3D_CCOPIA_SINC_MS)
            return 0;
        if (s_cSincEv)
            WaitForSingleObject(s_cSincEv, 2);
        else
            Sleep(1);
        __lwsync();
    }
    if (hu->pedida != seq)
        return 0;   /* no deberia: el hueco ya es de otra peticion */
    return gxd3d_entrega_color_hueco(hu, i, &dirHecha, bytes);
#else
    (void)bytes;
    return 0;
#endif
}

#if XBOX64_TRAZAS   /* informes [CCOPIA]/[ZCOPIA]/[PEEKZ]: solo con trazas */
static void gxd3d_ccopia_informe(void)
{
    char l[256];
    if (s_cSeq == 0 && s_stCMal == 0 && s_stCSinMem == 0)
        return;
    {
        const u32 n = s_stCEntregadas ? s_stCEntregadas : 1u;
        _snprintf(l, sizeof(l),
                  "[CCOPIA] pedidas=%u entregadas=%u sindato=%u | retraso med %u.%02u copias"
                  " | conv %u us (cache %u muestreo %u) | mal=%u sinmem=%u",
                  s_stCPedidas, s_stCEntregadas, s_stCSinDato,
                  s_stCEntregadas ? s_stCRetraso / s_stCEntregadas : 0u,
                  s_stCEntregadas ? (s_stCRetraso * 100u / s_stCEntregadas) % 100u : 0u,
                  s_stCEntregadas ? (s_stCConvTb / 50u) / n : 0u,
                  (s_stCFlushTb / 50u) / n, (s_stCRedTb / 50u) / n,
                  s_stCMal, s_stCSinMem);
    }
    l[sizeof(l) - 1] = 0;
    DebugOutputString(l);
    s_stCPedidas = s_stCEntregadas = s_stCSinDato = 0;
    s_stCRetraso = s_stCConvTb = 0;
    s_stCFlushTb = s_stCRedTb = 0;
}

static void gxd3d_peekz_informe(void);   /* [Perfect Dark] GX_PeekZ, mas abajo */
#endif /* XBOX64_TRAZAS */

extern "C" void gxd3d_zcopia_informe(void)
{
#if XBOX64_TRAZAS
    char l[256];
    gxd3d_ccopia_informe();   /* [Mario Kart 64, Banjo] la del color */
    gxd3d_peekz_informe();
    if (s_zSeq == 0 && s_stZMal == 0 && s_stZSinMem == 0)
        return;
    _snprintf(l, sizeof(l),
              "[ZCOPIA] pedidas=%u marcas=%u entregadas=%u sindato=%u | retraso med %u.%02u "
              "copias | conv %u us | mal=%u sinmem=%u | raw esq=%08X cen=%08X "
              "z24 %06X..%06X",
              s_stZPedidas, s_stZMarcas, s_stZEntregadas, s_stZSinDato,
              s_stZEntregadas ? s_stZRetraso / s_stZEntregadas : 0u,
              s_stZEntregadas ? (s_stZRetraso * 100u / s_stZEntregadas) % 100u : 0u,
              s_stZEntregadas ? (s_stZConvTb / 50u) / s_stZEntregadas : 0u,
              s_stZMal, s_stZSinMem, s_stZRawEsq, s_stZRawCen,
              (s_stZMin == 0xFFFFFFFFu) ? 0u : s_stZMin, s_stZMax);
    l[sizeof(l) - 1] = 0;
    DebugOutputString(l);
    s_stZPedidas = s_stZMarcas = s_stZEntregadas = s_stZSinDato = 0;
    s_stZRetraso = s_stZConvTb = 0;
    s_stZMin = 0xFFFFFFFFu;
    s_stZMax = 0;
    /* `mal` y `sinmem` no se ponen a cero: si pasan una vez, tienen que
     * seguir viendose. */
#endif /* XBOX64_TRAZAS */
}

void GX_CopyTex(void *dest, u8 clear)
{
    gxd3d_flush_pending();   /* idem: se resuelve lo dibujado */
    /* [Wii64 443f68b] La copia de PROFUNDIDAD: ver "LA COPIA DE PROFUNDIDAD
     * DEL EFB" justo encima.  No pasa por el registro de texturas resueltas:
     * `dest` se lee por CPU, no se muestrea. */
    if (s_copyDstFmt == GX_TF_Z16)
    {
        gxd3d_copia_z(dest, s_copySrcL, s_copySrcT, s_copySrcW, s_copySrcH,
                      s_copyDstW, s_copyDstH);
        if (clear)
            gxd3d_hiz_borrado_z();
        if (clear)
            g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                D3DCOLOR_ARGB(s_clearColor.a, s_clearColor.r,
                                              s_clearColor.g, s_clearColor.b),
                                1.0f, 0);
        return;
    }
    /* ★ EL EQUIVALENTE EXACTO: GX_CopyTex copia del framebuffer EMPOTRADO a
     * una textura, top-down.  En Xenos eso es `Resolve()` desde la EDRAM, que
     * tambien es top-down.  Esa equivalencia es la razon de seguir la rama
     * __GX__ y no la de OpenGL, cuyo glCopyTexImage2D es bottom-up
     * (ver project_xenon64_framebuffer_gln64).
     *
     * `dest` es el buffer en formato GX donde glN64_GX espera los texels.
     * Nosotros resolvemos a una textura de D3D y anotamos la asociacion; la
     * conversion a formato GX solo haria falta si el plugin leyera esos
     * bytes por CPU, y no lo hace: se los vuelve a dar a GX_InitTexObj. */
    gxd3d_resolve_to_gx_buffer(dest, s_copySrcL, s_copySrcT, s_copySrcW, s_copySrcH,
                               s_copyDstW, s_copyDstH, s_copyDstFmt);
#if XBOX64_TRAZAS
    gxd3d_dbg_resolve++;
#endif
    if (clear)
        gxd3d_hiz_borrado_z();
    if (clear)
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                            D3DCOLOR_ARGB(s_clearColor.a, s_clearColor.r,
                                          s_clearColor.g, s_clearColor.b),
                            1.0f, 0);
}

void GX_CopyDisp(void *dest, u8 clear)
{
    gxd3d_flush_pending();   /* la escena tiene que estar completa antes de resolver */
    /* EFB -> framebuffer externo == entregar el frame.  En este core eso lo
     * hace el frontend por textura (env 30003), asi que se delega en
     * xenon64_video_resolve(), que ya resuelve con rect EXPLICITO -- con
     * NULL el destino se queda sin tocar si RT y textura no miden lo mismo,
     * medido con FBG_DUMP. */
    (void)dest;
    /* [etapa 3] Por la cola: `xenon64_video_resolve()` habla con el device
     * de verdad (`s_dev`), asi que tiene que ejecutarla el consumidor, en su
     * sitio entre los dibujos -- despues de todos los de este frame. */
#if D3D_GRABADOR
    g_pd3dDevice->Entrega();
#else
    xenon64_video_resolve();
#endif
    if (clear)
        gxd3d_hiz_borrado_z();
    if (clear)
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                            D3DCOLOR_ARGB(s_clearColor.a, s_clearColor.r,
                                          s_clearColor.g, s_clearColor.b),
                            1.0f, 0);
}

/* [Xbox360] [Quake II, objetos a traves de las paredes] GX_CopyDisp con
 * borrado de COLOR pero conservando el Z.  El borrado de las dos cosas en la
 * copia es del hardware de GameCube, no de glN64 (SwapBuffers de PC no toca
 * el Z) ni de GLideN64 (cada buffer en su FBO; el swap no borra nada).  En el
 * N64 el Z solo lo borra el juego, con un relleno sobre la depth image.
 * Hace falta cuando el VI cae entre dos listas del mismo frame (Quake II:
 * escenario en la primera, objetos en la segunda): la segunda comparaba
 * contra un Z vacio.  Lo usa VI_UpdateScreen en la rama con emulacion de
 * framebuffer, que repone el color con FrameBuffer_RestoreBuffer.  (MEDIDO,
 * Quake II: hasta 82 de cada 256 swaps caian a mitad de frame.)  La rama sin
 * emulacion de framebuffer no borra nada: GX_CopyDisp(.., GX_FALSE). */
void GXD3D_CopyDispConservaZ(void *dest)
{
    gxd3d_flush_pending();
    (void)dest;
#if D3D_GRABADOR
    g_pd3dDevice->Entrega();
#else
    xenon64_video_resolve();
#endif
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET,
                        D3DCOLOR_ARGB(s_clearColor.a, s_clearColor.r,
                                      s_clearColor.g, s_clearColor.b),
                        1.0f, 0);
}

void GX_PixModeSync(void) { }

/* ------------------------------------------------------------------ */
/* Sincronizacion                                                      */
/* ------------------------------------------------------------------ */
static GXDrawSyncCallback s_drawSyncCb = NULL;
static u16 s_drawSyncToken = 0;

void GX_DrawDone(void)
{
    gxd3d_flush_pending();
    /* [Wii64 443f68b] DepthCopy.cpp lee `dcBuf` justo despues: se le deja
     * la copia de profundidad terminada mas nueva, SIN ESPERAR (ver "LA
     * COPIA DE PROFUNDIDAD DEL EFB").  Sin copias, no cuesta nada. */
    gxd3d_entrega_z();
    /* En GX espera a que la GPU vacie la FIFO.  Aqui el orden ya lo garantiza
     * el interpositor.
     *
     * [etapa 3] Aqui habia un `d3dq_sinc()` -- "drenar es lo unico honesto".
     * Ya no hace falta y en la etapa 4 no podria estar, porque el productor
     * no es quien vacia.  Lo que protegia -- que la memoria que el plugin
     * libera despues (Textures.cpp:839, Rice TextureManager.cpp:777) no la
     * lea nadie -- ya lo garantiza el propio diseno: los texels se COPIAN al
     * pool al convertirlos, asi que ni la GPU ni el consumidor leen nunca la
     * memoria del plugin, y la destruccion de las texturas de D3D es una
     * orden mas (`d3dq_soltar`), en orden con los dibujos que la usan. */
}

void GX_SetDrawSync(u16 token)
{
    s_drawSyncToken = token;
    if (s_drawSyncCb) s_drawSyncCb(token);
}

void GX_SetDrawSyncCallback(GXDrawSyncCallback cb) { s_drawSyncCb = cb; }

/* ★★ GX_PeekZ: UN RESOLVE DE PROFUNDIDAD POR FRAME, LEIDO 1-3 FRAMES TARDE.
 *
 * gDP.cpp la usa desde `_gDPPeekDepth`, dentro del hack
 * `hack_rectDepthBufferCopyPD` (Perfect Dark y Turok): el juego copia por el
 * RDP la Z bajo cada corona de luz y la CPU decide si la corona se ve.
 *
 * Antes era un stub que devolvia el plano lejano, y las coronas de Perfect
 * Dark NO SALEN NUNCA con eso.  El codigo descompilado del juego
 * (n64decomp/perfect_dark, src/game/artifact.c) lo explica: los puntos son
 * las ESQUINAS de la caja de cada lampara y la corona cuenta solo si
 *     |actualdepth - expecteddepth| <= tolerance
 * o sea, la Z leida tiene que ser LA DE LA LAMPARA.  Ningun valor fijo vale;
 * medido con la sonda [PDCORONA] (logs 4-7): F800, FFFC y el buffer entero
 * bien escrito, y ni un halo.
 *
 * En la Wii el EFB se lee por pixel.  Aqui no se puede esperar a la GPU (log
 * 22 de DK64: 11,5 ms por copia esperando), asi que va como la copia de Z de
 * DepthCopy, SIN ESPERAS y con sus mismos huecos y fences:
 *   1. productor, al primer texrect de coronas del frame
 *      (`GXD3D_PreparaPeekZ`): elige como fuente la copia TERMINADA mas
 *      nueva, tira sus lineas de cache si es nueva, y pide un Resolve de la Z
 *      del EFB al hueco siguiente (si no es la fuente);
 *   2. consumidor: fence y revision en el SWAP, lo de siempre;
 *   3. productor (`GX_PeekZ`): lee el pixel de la fuente, tileado, Z en los 24
 *      bits altos del D24S8 (medido en la copia de Z).
 * El juego ya cuenta con la Z del frame anterior (artifact.c: "the checks are
 * done by reading from the previous frame's z-buffer"); aqui es de 1-3.
 * Sin fuente todavia, el plano lejano, como antes. */
static GXD3DZHueco *s_pzFuente    = NULL;   /* de donde lee GX_PeekZ       */
static u32          s_pzFuenteSeq = 0;      /* que copia tiene la fuente    */
static volatile u32 s_stPzPedidas = 0, s_stPzNuevas = 0, s_stPzLecturas = 0;
static volatile u32 s_stPzSinFuente = 0, s_stPzFuera = 0, s_stPzOcupados = 0;

void GXD3D_PreparaPeekZ(u16 x, u16 y, u16 w, u16 h)
{
    const u32 rtW = (u32)xenon64_video_width();
    const u32 rtH = (u32)xenon64_video_height();
    GXD3DZHueco *mejor = NULL;
    unsigned int k;

    if (!w || !h || (u32)x + w > rtW || (u32)y + h > rtH)
    {
#if XBOX64_TRAZAS
        s_stZMal++;
#endif
        return;
    }

    /* 1. La fuente: la copia terminada mas nueva. */
#if !D3D_GRABADOR
    gxd3d_consumidor_revisa_z(g_pd3dDevice);
#endif
    for (k = 0; k < GXD3D_Z_HUECOS; k++)
    {
        GXD3DZHueco *hz = &s_zHueco[k];
        const u32 lista = hz->lista;
        if (lista && lista == hz->pedida && (!mejor || (int)(lista - mejor->lista) > 0))
            mejor = hz;
    }
    if (mejor && (mejor != s_pzFuente || mejor->lista != s_pzFuenteSeq))
    {
        const unsigned int aw = ((unsigned int)mejor->w + 31u) & ~31u;
        const unsigned int ah = ((unsigned int)mejor->h + 31u) & ~31u;
        const unsigned int bytes = aw * ah * 4u;
        unsigned int off;
        __lwsync();   /* la fence antes que los texels */
        for (off = 0; off < bytes; off += 128u)
            __dcbf((int)off, mejor->mem);
        __sync();
        s_pzFuente    = mejor;
        s_pzFuenteSeq = mejor->lista;
#if XBOX64_TRAZAS
        s_stPzNuevas++;
#endif
    }

    /* 2. La copia de este frame, a un hueco LIBRE: ni la fuente (la GPU la
     * pisaria mientras se lee) ni uno con una copia en vuelo.
     *
     * NO AL QUE TOCA POR TURNO, como DepthCopy: con el hilo grafico el
     * productor va hasta 3 frames por delante del consumidor ([HILO2] swaps
     * max=3) y pide una por frame, asi que cuando la fence de un hueco pasa
     * ya puede haber otra pedida encima (`pedida` > `lista`) y ninguno
     * quedaria listo.  (OJO: lo que median los logs 8-10 -- fuentes=0 -- era
     * otra cosa: sin hilo nadie revisaba las fences; ver d3dq_drena.) */
    gxd3d_flush_pending();   /* lo dibujado tiene que entrar en el resolve */
    if (s_zMemOk == 0)
    {
        s_zMemOk = 1;
        for (k = 0; k < GXD3D_Z_HUECOS; k++)
        {
            s_zHueco[k].mem = (u8 *)XPhysicalAlloc(GXD3D_Z_CAP, MAXULONG_PTR, 0,
                                                   PAGE_READWRITE | MEM_LARGE_PAGES);
            if (!s_zHueco[k].mem)
                s_zMemOk = -1;
        }
    }
    if (s_zMemOk < 0)
    {
#if XBOX64_TRAZAS
        s_stZSinMem++;
#endif
        return;
    }
    {
        unsigned int kk = GXD3D_Z_HUECOS;
        GXD3DZHueco *hz;
        D3DRECT r;

        for (k = 0; k < GXD3D_Z_HUECOS; k++)
        {
            const GXD3DZHueco *c = &s_zHueco[k];
            if (c != s_pzFuente && (c->pedida == 0 || c->lista == c->pedida))
            {
                kk = k;
                break;
            }
        }
        if (kk == GXD3D_Z_HUECOS)
        {
#if XBOX64_TRAZAS
            s_stPzOcupados++;
#endif
            return;
        }
        hz = &s_zHueco[kk];
        if (hz->w != w || hz->h != h)
        {
            UINT base = 0, mip = 0;
            XGSetTextureHeader(w, h, 1, 0, D3DFMT_D24S8, (D3DPOOL)0, 0,
                               XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, &hz->hdr, &base, &mip);
            if (base > GXD3D_Z_CAP)
            {
                hz->w = hz->h = 0;
#if XBOX64_TRAZAS
                s_stZMal++;
#endif
                return;
            }
            XGOffsetResourceAddress(&hz->hdr, hz->mem);
            hz->w = w;
            hz->h = h;
        }
        hz->ox = x;
        hz->oy = y;

        s_zSeq = s_zSeq + 1;
        hz->pedida = s_zSeq;   /* hasta su fence, el hueco no se lee */
#if XBOX64_TRAZAS
        s_stPzPedidas++;
#endif

        r.x1 = x;      r.y1 = y;
        r.x2 = x + w;  r.y2 = y + h;
        g_pd3dDevice->Resolve(D3DRESOLVE_DEPTHSTENCIL, &r, (D3DBaseTexture *)&hz->hdr,
                              NULL, 0, 0, NULL, 0.0f, 0, NULL);
#if D3D_GRABADOR
        g_pd3dDevice->MarcaProfundidad(kk, s_zSeq);
#else
        gxd3d_consumidor_marca_z(g_pd3dDevice, kk, s_zSeq);
#endif
    }
}

void GX_PeekZ(u16 x, u16 y, u32 *z)
{
    const GXD3DZHueco *f = s_pzFuente;
    if (!z)
        return;
    *z = 0x00FFFFFF;   /* el plano lejano, sin fuente */
    if (!f || !f->mem || f->lista != s_pzFuenteSeq)
    {
#if XBOX64_TRAZAS
        s_stPzSinFuente++;
#endif
        return;
    }
    if (x < f->ox || y < f->oy || x >= f->ox + f->w || y >= f->oy + f->h)
    {
#if XBOX64_TRAZAS
        s_stPzFuera++;
#endif
        return;
    }
    {
        const u32 *t = (const u32 *)f->mem;
        *z = t[XGAddress2DTiledOffset((UINT)(x - f->ox), (UINT)(y - f->oy), f->w, 4)] >> 8;
    }
#if XBOX64_TRAZAS
    s_stPzLecturas++;
#endif
}

/* Una linea por segundo (con la de [ZCOPIA]) si hubo algo: cuantas copias se
 * pidieron, cuantas fuentes nuevas se tomaron, cuantas lecturas se sirvieron
 * de verdad y cuantas salieron lejanas por no tener fuente o caer fuera. */
#if XBOX64_TRAZAS
static void gxd3d_peekz_informe(void)
{
    char l[256];
    if (s_stPzPedidas == 0 && s_stPzLecturas == 0 && s_stPzSinFuente == 0)
        return;
    _snprintf(l, sizeof(l),
              "[PEEKZ] pedidas=%u ocupados=%u fuentes=%u lecturas=%u sinfuente=%u fuera=%u | fuente %ux%u en %u,%u",
              s_stPzPedidas, s_stPzOcupados, s_stPzNuevas, s_stPzLecturas, s_stPzSinFuente, s_stPzFuera,
              s_pzFuente ? (unsigned)s_pzFuente->w : 0u, s_pzFuente ? (unsigned)s_pzFuente->h : 0u,
              s_pzFuente ? (unsigned)s_pzFuente->ox : 0u, s_pzFuente ? (unsigned)s_pzFuente->oy : 0u);
    l[sizeof(l) - 1] = 0;
    DebugOutputString(l);
    s_stPzPedidas = s_stPzNuevas = s_stPzLecturas = s_stPzSinFuente = s_stPzFuera = 0;
    s_stPzOcupados = 0;
}
#endif /* XBOX64_TRAZAS */

/* El "write gather pipe" de la Wii es el camino rapido CPU->GPU: una UNION en
 * una direccion FIJA donde cada escritura a un miembro EMPUJA al FIFO.  En
 * Xenon no existe, asi que el destino es memoria normal y el cursor tiene que
 * AVANZAR en cada escritura.  La explicacion completa -- y por que ya no vale
 * declararlo `u32 *`-- esta en gxshim/gccore.h.
 *
 * Dos consumidores: el bucle del XFB de VI.cpp y `_gDPWriteRDRAM` (gDP.cpp,
 * llegado con Wii64 1b5041a).  Los dos usan la sintaxis de libogc. */
/* El cursor de escritura.  Lo leen y lo avanzan los `operator=` EN LINEA de
 * `GXWGPipe` (gxshim/gccore.h); aqui solo lo mueven las dos funciones de
 * redireccion.  Fuera de un Redirect vale NULL y las escrituras se tiran:
 * en la Wii, escribir al FIFO sin haberlo redirigido tampoco toca la memoria
 * de nadie. */
unsigned char *gxd3d_wg_cursor = NULL;
static void   *s_wgp           = NULL;

/* El objeto no lleva datos: existe solo para que `wgPipe->XXX` tenga a que
 * apuntar.  Todo el estado es el cursor de arriba. */
static GXWGPipe s_wgPipeObj;
GXWGPipe *wgPipe = &s_wgPipeObj;

void GX_RedirectWriteGatherPipe(void *ptr) { s_wgp = ptr; gxd3d_wg_cursor = (unsigned char *)ptr; }
void GX_RestoreWriteGatherPipe(void)       { s_wgp = NULL; gxd3d_wg_cursor = NULL; }
void *gxd3d_wgp(void)                      { return s_wgp; }

/* ------------------------------------------------------------------ */
/* Utilidades de matrices de libogc                                    */
/* ------------------------------------------------------------------ */
void guMtxIdentity(Mtx mt)
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            mt[i][j] = (i == j) ? 1.0f : 0.0f;
}

/* ★★ [Fase 6] 3x4, NO 4x4.  Es lo que copia la de libogc y el shim tiene que
 * reproducirlo, aunque los dos unicos llamantes le pasen matrices de 4x4.
 *
 * Rice la usa en `OGLRender.cpp:1661` y `:1674` sobre gGX.GXcombW,
 * gGX.GXprojIdent y gGX.GXprojTemp, que son `Mtx44` (RenderBase.h:248-251).
 * No da ni un aviso al compilar porque `f32[3][4]` y `f32[4][4]` decaen los
 * dos al MISMO tipo, `f32(*)[4]`; el compilador no puede verlo.  Resultado:
 * la ultima fila del destino se queda con lo que tuviera de antes.
 *
 * ★ ES UN FALLO DE Wii64 Y SE REPRODUCE A PROPOSITO.  Copiar 16 floats seria
 * "mejorar" la referencia, y el dia que la proyeccion de Rice salga rara no
 * sabriamos si es por eso o por otra cosa.  Se usa justo para el offset de
 * poligono (`GXprojTemp[2][2] += GXpolyOffsetFactor`), asi que si aparece un
 * z-fighting extranyo en Rice, empezar por aqui: cambiar el 3 por un 4 y
 * medir. */
void guMtxCopy(Mtx src, Mtx dst)
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            dst[i][j] = src[i][j];
}

void guMtx44Identity(Mtx44 mt)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            mt[i][j] = (i == j) ? 1.0f : 0.0f;
}

void guMtx44Inverse(Mtx44 src, Mtx44 inv)
{
    /* Inversa general 4x4 por Gauss-Jordan.  glN64_GX la usa una sola vez
     * (para deshacer la proyeccion), asi que no merece optimizacion. */
    float a[4][8];
    int i, j, k;
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++) a[i][j] = src[i][j];
        for (j = 0; j < 4; j++) a[i][4 + j] = (i == j) ? 1.0f : 0.0f;
    }
    for (i = 0; i < 4; i++)
    {
        int piv = i;
        float d;
        for (k = i + 1; k < 4; k++)
            if (a[k][i] * (a[k][i] < 0 ? -1.0f : 1.0f) >
                a[piv][i] * (a[piv][i] < 0 ? -1.0f : 1.0f)) piv = k;
        if (piv != i) for (j = 0; j < 8; j++) { float t = a[i][j]; a[i][j] = a[piv][j]; a[piv][j] = t; }
        d = a[i][i];
        if (d == 0.0f) { guMtx44Identity(inv); return; }
        for (j = 0; j < 8; j++) a[i][j] /= d;
        for (k = 0; k < 4; k++)
        {
            if (k == i) continue;
            d = a[k][i];
            for (j = 0; j < 8; j++) a[k][j] -= d * a[i][j];
        }
    }
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            inv[i][j] = a[i][4 + j];
}

void guOrtho(Mtx44 mt, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f)
{
    guMtx44Identity(mt);
    mt[0][0] = 2.0f / (r - l);
    mt[0][3] = -(r + l) / (r - l);
    mt[1][1] = 2.0f / (t - b);
    mt[1][3] = -(t + b) / (t - b);

    /* ★★ LA FILA Z: `-f`, NO `-n`.  UN CARACTER, Y ERA EL BANDEADO.
     *
     * En GX la profundidad normalizada va a **[-1, 0]**, con near = -1 y
     * far = 0.  No hay que creerselo: lo demuestra el `GXprojIdent` que el
     * propio plugin construye A MANO (OpenGL.cpp:2195-2196,
     * `[2][2]=0.5`, `[2][3]=-0.5`), que con vertices z en [-1,1] mapea
     * -1 -> -1 y +1 -> 0.  Esa es la referencia que FUNCIONA.
     *
     * De ahi sale la fila: z=n -> -1 y z=f -> 0, o sea
     *     mt[2][2] = -1/(f-n)      mt[2][3] = -f/(f-n)
     *
     * Yo habia puesto `-n/(f-n)`.  Con la llamada real del plugin
     * (`guOrtho(..., 1.0f, -1.0f)`, con n y f INVERTIDOS a proposito) eso
     * cambia el signo del termino independiente:
     *
     *     correcto   z_ndc = 0.5z - 0.5   ->  z=0 da -0.5 (en medio)
     *     el mio     z_ndc = 0.5z + 0.5   ->  z=0 da +0.5, FUERA de [-1,0]
     *
     * y tras el `clip.z += clip.w` del vertex shader eso es 1.5, que el
     * viewport recorta a 1.0.  O sea: **todos los rects en espacio de
     * pantalla -- cuadros de dialogo, HUD, los fillrect del N64 -- acababan
     * pegados al plano lejano**, peleandose entre si y contra la escena 3D.
     * Sintoma medido: bandas horizontales regulares alternando los colores
     * de las dos superficies que se solapan, y una escena LIMPIA en los
     * fotogramas donde no habia ningun rect (solo la geometria 3D, que usa
     * GXcombW/GXprojIdent y no pasa por aqui).
     *
     * Los dos sitios con `n=0, f=1` (VI.cpp:552, FrameBuffer.cpp:494) tenian
     * el mismo defecto: daban 0 en vez de -1. */
    mt[2][2] = -1.0f / (f - n);
    mt[2][3] = -f / (f - n);
}

f32 guVecDotProduct(guVector *a, guVector *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

void guVecNormalize(guVector *src, guVector *dst)
{
    /* Dos argumentos porque asi la llama el plugin, siempre con el mismo
     * puntero repetido (gSP.cpp:215, :608, :675; F3DFLX2.cpp:56).
     *
     * ★ Raiz reciproca: en Xenon se usa __frsqrte + un paso de Newton, NO el
     * truco de bits de Quake III -- ese pasa por memoria (Load-Hit-Store)
     * porque PPC no mueve GPR<->FPR.  Ver reference_xenon_frsqrte_vs_quake3.
     * Aqui se deja la forma portable porque guVecNormalize se llama poquisimo
     * (iluminacion, una vez por luz y vertice como mucho) y el arbol ya tiene
     * xenon/ppcfast.h si algun dia sale en el perfil. */
    f32 d;
    if (!src || !dst) return;
    d = src->x * src->x + src->y * src->y + src->z * src->z;
    if (d > 0.0f)
    {
        f32 s = 1.0f / (f32)sqrt((double)d);
        dst->x = src->x * s;
        dst->y = src->y * s;
        dst->z = src->z * s;
    }
    else if (dst != src)
    {
        *dst = *src;
    }
}

/* -- API de video de libogc: no-ops ---------------------------------------- */
/* El frame se entrega por textura al frontend (env 30003) y el ritmo lo
 * lleva Salvia (Sync::limit_fps), asi que ni presentamos XFB ni nos
 * enganchamos al retrazo. */
void VIDEO_SetPreRetraceCallback(void (*cb)(u32 retraceCnt)) { (void)cb; }
void VIDEO_SetNextFramebuffer(void *fb) { (void)fb; }
void VIDEO_Flush(void) { }
void VIDEO_WaitVSync(void) { }

/* ------------------------------------------------------------------ */
/* Ciclo de vida                                                       */
/* ------------------------------------------------------------------ */
int gxd3d_init(void)
{
    int i;
    if (!g_pd3dDevice) return 0;

    for (i = 0; i < GXD3D_VB_COUNT; i++)
    {
        if (FAILED(g_pd3dDevice->CreateVertexBuffer(
                GXD3D_MAX_VERTS * sizeof(GXD3DVertex),
                D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &s_vb[i], NULL)))
            return 0;
    }
    g_pd3dDevice->CreateVertexDeclaration(s_vertexDecl, &s_decl);

    /* El vertex shader.  Se compila en ejecucion como el de Rice
     * (d3d9_backend.cpp:1654); el perfil lo promociona el XDK a vs_3_0. */
    {
        D3DXMACRO defs[3];
        LPD3DXBUFFER code = NULL, err = NULL;
        defs[0].Name = "GXD3D_VS_ZFIX";
        defs[0].Definition = "1";   /* a "0" para bisecar el convenio de z */
        /* ★ La regla del medio pixel de D3D9.  A "0" se revierte entero y el
         * shader vuelve a ser el de antes.  Ver la nota junto a `uHalfPx`. */
        defs[1].Name = "GXD3D_VS_HALFPIXEL";
        defs[1].Definition = "1";
        defs[2].Name = NULL;
        defs[2].Definition = NULL;
        if (SUCCEEDED(D3DXCompileShader(s_vsSource, (UINT)strlen(s_vsSource),
                                        defs, NULL, "main", "vs_3_0", 0,
                                        &code, &err, NULL)))
        {
            g_pd3dDevice->CreateVertexShader((const DWORD *)code->GetBufferPointer(), &s_vs);
            code->Release();
        }
#if XBOX64_TRAZAS
        else
        {
            char msg[512];
            _snprintf(msg, sizeof(msg), "[GXD3D] el vertex shader NO compila: %s",
                      err ? (const char *)err->GetBufferPointer() : "(sin mensaje)");
            DebugOutputString(msg);
        }
#endif
        if (err) err->Release();
        if (!s_vs) return 0;   /* sin VS no se puede dibujar: fallar pronto */
    }

    s_vbIndex = GXD3D_VB_COUNT - 1;
    s_vbData  = NULL;
    /* [etapa 3] El lado del consumidor del vertex buffer, y la primera
     * pasada otra vez: la ROM nueva parte de un estado conocido. */
    s_cvData   = NULL;
    s_cvUsed   = 0;
    s_cvStream = 0;
    s_primeraPasada = 1;
    s_pendActive = 0;  s_pendCount = 0;
    memset(s_samp, 0, sizeof(s_samp));
    memset(s_rtt, 0, sizeof(s_rtt));
    s_rttVivas = 0;          /* va con el memset o la cuenta se descuadra */
    memset(s_texReg, 0, sizeof(s_texReg));
    s_texSerial = 0;
    s_texLapidas = 0;
    gxd3d_state_invalidate();
    guMtx44Identity(s_proj);
    guMtx44Identity(s_modelview);
    /* [Fase 6] El banco entero a identidad y la seleccion a la ranura 0.  Sin
     * esto, una ranura que el plugin no haya cargado todavia devolveria ceros
     * -- una matriz que colapsa toda la geometria a un punto -- y al recargar
     * juego el shim se quedaria con la seleccion del anterior. */
    {
        int k;
        for (k = 0; k < GXD3D_POSMTX_N; k++)
            guMtx44Identity(s_posMtxBank[k]);
    }
    s_posMtxSel   = 0;
    s_mtxDirty    = 1;
    s_halfPx[0]   = 0.0f;  s_halfPx[1] = 0.0f;
    s_halfPx[2]   = 0.0f;  s_halfPx[3] = 0.0f;
    s_vpFix[0]    = 1.0f;  s_vpFix[1]  = 1.0f;   /* [log 60] identidad */
    s_vpFix[2]    = 0.0f;  s_vpFix[3]  = 0.0f;
    s_halfPxDirty = 1;

    /* El aviso de liberacion del monton de texturas del plugin.  Ver la nota
     * larga sobre `gxd3d_tex_free_hook`: sin esto el registro sirve texturas
     * de direcciones ya recicladas. */
    lwp_heap_free_hook = gxd3d_tex_free_hook;

    gxd3d_tev_init();
    return 1;
}

void gxd3d_shutdown(void)
{
    int i;
    /* ★ LO PRIMERO: el gancho apunta a codigo y a un registro de este
     * fichero.  Si se deja puesto, el RecompCache del dynarec -- que sigue
     * vivo y sigue liberando-- entra aqui despues de soltar las texturas. */
    lwp_heap_free_hook = NULL;

    /* ★ [revision 2026-10-01] RETIRAR DEL DEVICE LO QUE SE SUELTA DIRECTAMENTE
     * aqui abajo: los pixel shaders de la cache (`gxd3d_tev_shutdown`), la
     * declaracion, el vertex shader y los vertex buffers.  Se soltaban
     * ESTANDO LIGADOS, y el siguiente `Set*` del frontend tocaria el objeto
     * saliente ya liberado: con las texturas eso esta MEDIDO (log 20,
     * `SetTexture` escribiendo en la liberada) y se arreglo retirandolas
     * antes; con estos no se habia hecho.  Es el patron de `dx9Exit` de
     * fbanext (src/intf/video/xbox/vid_d3d9.cpp:142-144), que va en esta
     * misma consola.  Son ordenes: el `d3dq_drena` de justo debajo las
     * ejecuta antes de soltar nada. */
    g_pd3dDevice->SetPixelShader(NULL);
    g_pd3dDevice->SetVertexShader(NULL);
    g_pd3dDevice->SetVertexDeclaration(NULL);
    g_pd3dDevice->SetStreamSource(0, NULL, 0, 0);

#if D3D_GRABADOR >= 2
    /* [etapa 3] Ejecutar lo que quede en la cola ANTES de soltar nada:
     * justo debajo se sueltan objetos DIRECTAMENTE -- los pixel shaders de
     * la cache, la declaracion, el vertex shader --, y una orden pendiente
     * que los referenciase leeria memoria liberada. */
    d3dq_drena();
#endif
    gxd3d_tev_shutdown();
    /* [etapa 3] El vertex buffer bloqueado es ahora el del consumidor; el
     * del productor es memoria de CPU y no hay nada que soltar. */
    gxd3d_consumidor_acaba();
    s_vbData = NULL;
    for (i = 0; i < GXD3D_VB_COUNT; i++)
        if (s_vb[i]) { d3dq_soltar((D3DResource *)s_vb[i]); s_vb[i] = NULL; }
    if (s_decl) { s_decl->Release(); s_decl = NULL; }
    if (s_vs)   { s_vs->Release();   s_vs   = NULL; }

    /* ★ Las texturas resueltas TAMBIEN hay que soltarlas aqui, y ademas hay
     * que RETIRARLAS del device antes: si el frontend sigue con una ligada
     * en un sampler la muestrea en el Present siguiente.  Es la trampa nº2
     * del port de Rice a libretro (project_xenon64_libretro_port). */
    for (i = 0; i < 4; i++)
        g_pd3dDevice->SetTexture(i, NULL);
    for (i = 0; i < GXD3D_RTT_MAX; i++)
        if (s_rtt[i].tex) { d3dq_soltar((D3DResource *)s_rtt[i].tex); s_rtt[i].tex = NULL; }
    memset(s_rtt, 0, sizeof(s_rtt));
    s_rttVivas = 0;          /* idem */
    /* [Wii64 443f68b] La copia de profundidad: los huecos se quedan (son de
     * la vida del core y puede haber un resolve en vuelo), pero lo que haya
     * dentro ya no se entrega a la partida siguiente. */
    s_zEntregada = s_zSeq;
    s_zDest = NULL;
    /* [Mario Kart 64, Banjo] Idem con la copia del color: lo que quede en los
     * huecos no se escribe en la RDRAM de la partida siguiente. */
    s_cEntregada = s_cSeq;
#if GXD3D_CCOPIA_SINC
    /* Y la partida siguiente empieza sin copias sincronas. */
    memset((void *)s_cSincHueco, 0, sizeof(s_cSincHueco));
    s_cSincUsada = 0;
#endif
    /* La copia de Z del dibujo sobre el depth image (ya fuera del sampler 3:
     * el bucle de SetTexture(NULL) de arriba la saco). */
    gxd3d_rtz_shutdown();
    gxd3d_tex_reset();

#if D3D_GRABADOR >= 2
    /* [etapa 3] Y otra vez al final: los `d3dq_soltar` y los SetTexture(NULL)
     * de arriba son ORDENES, y tienen que ejecutarse antes de que el device
     * se devuelva al frontend. */
    d3dq_drena();
#endif
#if D3D_GRABADOR
    /* La sombra del consumidor apunta a objetos que acaban de morir. */
    d3dq_sombra_reset();
#endif
}
