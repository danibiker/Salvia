/*
 * gccore.h  --  SHIM de la API GX de libogc sobre el D3D9 del XDK de Xbox 360.
 *
 * ESTE FICHERO NO ES DE libogc.  Es NUESTRO.  Declara exactamente el
 * subconjunto de la API GX que usa `source/glN64_GX/`, para que ese arbol
 * compile BYTE A BYTE IDENTICO al de Wii64 y toda la adaptacion viva aqui.
 * Es la misma disciplina del port de device/rcp/ (ver
 * project_xenon64_port_rcp): los copiados no se tocan, la adaptacion es
 * nuestra.
 *
 * POR QUE ESTE CORTE Y NO OTRO
 * ----------------------------
 * glN64_GX no tiene abstraccion de backend: el port a GameCube se hizo con
 * ~400 `#ifdef __GX__` repartidos por OpenGL.cpp (121), Textures.cpp (90) y
 * FrameBuffer.cpp (61).  Pero el CENSO dice que toda esa maquinaria se apoya
 * en solo **59 funciones GX distintas**, y que los ficheros que llevan la
 * logica del N64 casi no las tocan:
 *
 *     OpenGL.cpp      172 llamadas GX_*()
 *     VI.cpp           80
 *     FrameBuffer.cpp  50
 *     TEV_combiner.cpp 18
 *     Textures.cpp      8
 *     gSP.cpp           3
 *     gDP RDP RSP GBI N64 CRC Combiner DepthBuffer   CERO
 *
 * O sea que interponerse EN LA API GX sale mas barato que reescribir el
 * renderer, y ademas deja intacto el arbol entero en vez de forkear 6.203
 * lineas (OpenGL + Textures + FrameBuffer + TEV_combiner).
 *
 * Y la rama `__GX__` es la que conviene seguir por una razon medida en este
 * mismo arbol: GameCube/Wii dibujan sobre un framebuffer EMPOTRADO (EFB) y
 * copian a textura con `GX_CopyTex`, que es TOP-DOWN -- exactamente la forma
 * del EDRAM de Xenos y su `Resolve()`, y al reves que el `glCopyTexImage2D`
 * de OpenGL, que es bottom-up.  Ver project_xenon64_framebuffer_gln64.
 *
 * NOTA SOBRE LOS VALORES DE LAS CONSTANTES
 * ----------------------------------------
 * Verificado con un barrido: **el plugin NO hace aritmetica con ninguna
 * constante GX_*** (cero ocurrencias de `GX_TEVSTAGE0 + i`, `GX_TEXMAP0 + n`
 * y similares).  Por tanto los valores de aqui solo tienen que ser DISTINTOS
 * dentro de su familia; no tienen que coincidir con los de libogc.  Aun asi
 * se han conservado los valores reales de libogc donde se conocian, para que
 * un volcado sea legible contra la documentacion de GX.
 *
 * Toolchain: VS2010 / XDK.  C++03.  Nada de enum class, constexpr ni <cstdint>.
 */

#ifndef _XENON_GXSHIM_GCCORE_H_
#define _XENON_GXSHIM_GCCORE_H_

#include "../../source/winlnxdefs.h"   /* BOOL/DWORD y companyia del arbol */

/* ------------------------------------------------------------------ */
/* ★ LOS TIPOS DE libogc.  AQUI, Y NO EN OTRO SITIO.                   */
/* ------------------------------------------------------------------ */
/* `u8`/`u16`/`u32`/`f32`... los define `gctypes.h` de libogc, que en Wii
 * entra arrastrado por `<gccore.h>`.  Este shim SUSTITUYE a gccore.h, asi
 * que le toca aportarlos.
 *
 * La prueba de que este es su sitio esta en el propio arbol copiado:
 * `glN64_GX/Types.h` define exactamente estos typedefs... pero el fichero
 * ENTERO vive bajo `#ifndef __GX__` (Types.h:12).  O sea que en el build de
 * GameCube/Wii -- el que seguimos-- Types.h no aporta NADA y los tipos salen
 * de gccore.h.  La primera version de este shim incluyo `winlnxdefs.h`
 * dando por hecho que los traia, y no los trae: ~200 errores C2146/C4430,
 * todos la misma causa.
 *
 * ★ Y LOS TIPOS BASE NO SON ARBITRARIOS.  Se eligen para que COINCIDAN con
 * los de las otras tres cabeceras del arbol que declaran los mismos nombres:
 *
 *     source/xetypes.h               u32 = uint32_t   (= unsigned int)
 *     source/Rice_GX_Xenos/typedefs.h  u32 = unsigned int
 *     source/mupen64plus-rsp-hle/hle.h u32 = unsigned int
 *
 * Hoy ninguna unidad de compilacion incluye dos de ellas a la vez
 * (comprobado), pero si algun dia pasa, una redefinicion IDENTICA de typedef
 * es legal en C++ y una divergente es `error C2371`.  Ojo: la rama muerta de
 * `Types.h` usa `unsigned long` para u32; no se copia eso a proposito, que
 * seria justo el C2371. */
#ifndef _GCTYPES_H_
#define _GCTYPES_H_

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

typedef signed char         s8;
typedef short               s16;
typedef int                 s32;
typedef long long           s64;

typedef volatile unsigned char       vu8;
typedef volatile unsigned short      vu16;
typedef volatile unsigned int        vu32;
typedef volatile unsigned long long  vu64;

typedef volatile signed char         vs8;
typedef volatile short               vs16;
typedef volatile int                 vs32;
typedef volatile long long           vs64;

typedef float               f32;
typedef double              f64;

#endif /* _GCTYPES_H_ */

/* ★ Los mutex de LWP, arrastrados como hace libogc (gccore.h -> ogc/mutex.h).
 *
 * Va AQUI, justo detras de los tipos, y no mas abajo: `Rice_GX/CritSect.h`
 * declara un miembro `mutex_t` y bajo `__GX__` no incluye NADA, asi que el
 * tipo tiene que estar puesto antes de que ese fichero se compile.  Es copia
 * verbatim de Wii64 y no se toca. */
#include "ogc/mutex.h"

/* TRUE/FALSE/NULL: en Wii tambien salen por esta via.  `Types.h` los pone
 * con la misma guarda `#ifndef`, y tampoco se compila bajo __GX__. */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL  0
#endif

/* ------------------------------------------------------------------ */
/* Macros de utilidad de libogc (gctypes.h / gcutil.h)                 */
/* ------------------------------------------------------------------ */
/* `ATTRIBUTE_ALIGN` es la unica de alineacion que usa el plugin
 * (N64.cpp:24, `u64 TMEM[512] ATTRIBUTE_ALIGN(32);`).  En GCC es
 * `__attribute__((aligned(n)))` y va DETRAS del declarador; en MSVC es
 * `__declspec(align(n))` y va DELANTE...  pero MSVC tambien lo acepta detras
 * del declarador en una definicion de variable, que es el unico uso aqui. */
#ifndef ATTRIBUTE_ALIGN
#  if defined(_MSC_VER)
#    define ATTRIBUTE_ALIGN(n) __declspec(align(n))
#  else
#    define ATTRIBUTE_ALIGN(n) __attribute__((aligned(n)))
#  endif
#endif

#ifndef ATTRIBUTE_PACKED
#  define ATTRIBUTE_PACKED
#endif

/* MIN/MAX: libogc las da en gctypes.h.  El plugin las usa en RSP.cpp:302,
 * gDP.cpp (9 sitios), F5Rogue.cpp:1159 y ZSortBOSS.cpp:576. */
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* ------------------------------------------------------------------ */
/* Cache de datos (en libogc: <ogc/cache.h>, arrastrado por gccore.h)  */
/* ------------------------------------------------------------------ */
/* ★ VIVEN AQUI Y NO EN `ogc/machine/processor.h` a proposito: `Textures.cpp`
 * es el unico que incluye processor.h, pero `FrameBuffer.cpp:259` y `:399`
 * tambien llaman a `DCInvalidateRange`.  En libogc las trae gccore.h via
 * <ogc/cache.h>, asi que este es su sitio de verdad. */
#if defined(_XBOX) && defined(_MSC_VER)
#include "../../source/xenon_cache.h"   /* XENON_CACHE_LINE, __dcbst, __sync */

#ifndef _sync
#define _sync() __sync()
#endif

/* Volcar a memoria las lineas de D-cache del rango, para que la GPU las vea.
 * `dcbst` (store) y no `dcbf` (flush): no hace falta expulsar la linea. */
static __inline void DCFlushRange(void *startaddr, unsigned int len)
{
    unsigned char *p, *end;
    if (len == 0) return;
    p   = (unsigned char *)((unsigned int)startaddr & ~(XENON_CACHE_LINE - 1));
    end = (unsigned char *)startaddr + len;
    for (; p < end; p += XENON_CACHE_LINE)
        __dcbst(0, p);
    __sync();
}

static __inline void DCStoreRange(void *startaddr, unsigned int len)
{
    DCFlushRange(startaddr, len);
}

static __inline void DCInvalidateRange(void *startaddr, unsigned int len)
{
    DCFlushRange(startaddr, len);
}

/* ★ DCZeroRange NO se implementa con `dcbz`, y esto importa.
 *
 * `dcbz` pone a cero UNA LINEA DE CACHE ENTERA.  En Gekko/Broadway la linea
 * son 32 bytes y el plugin cuenta con eso; en Xenon son **128**
 * (XENON_CACHE_LINE).  Un `dcbz` sobre un bloque de 32 bytes borraria 96
 * bytes de memoria AJENA -- corrupcion silenciosa y dependiente del layout,
 * de lo peor de diagnosticar.
 *
 * Como aqui solo se busca dejar el rango a cero, un memset hace el trabajo
 * sin suponer nada del tamano de linea. */
static __inline void DCZeroRange(void *startaddr, unsigned int len)
{
    unsigned char *p = (unsigned char *)startaddr;
    unsigned int i;
    for (i = 0; i < len; i++) p[i] = 0;
    DCFlushRange(startaddr, len);
}

/* ------------------------------------------------------------------ */
/* Cache de INSTRUCCIONES.  La pide la fase 3, no el plugin grafico    */
/* ------------------------------------------------------------------ */
/* El dynarec la llama SIEMPRE justo detras de DCFlushRange, en las mismas
 * cuatro parejas:
 *
 *     ppc/Recompile.c:295-296          el codigo recien emitido
 *     Recomp-Cache-Heap.c:141-142      un salto reparado (11 instrucciones)
 *     Recomp-Cache-Heap.c:340-341      idem
 *
 * ★ NO ES UN `icbi` SUELTO: la secuencia correcta en Xenon son CINCO pasos
 * (dcbst / sync / icbi / sync / isync) y el segundo `sync` no es opcional --
 * sin el, el `isync` adelanta a los `icbi` y quedan lineas rancias en la
 * I-cache.  Falla justo al REGENERAR codigo sobre una linea ya usada, que
 * es lo que hace un dynarec al invalidar un bloque.  El porque completo, y
 * de donde sale la secuencia, estan en `source/xenon_cache.h`.
 *
 * ★ QUE EL `dcbst` SE REPITA NO ES UN FALLO.  `xenon_icache_invalidate` ya
 * hace su propio paso 1, asi que el `DCFlushRange` que Wii64 pone delante
 * queda redundante.  Se deja: quitarlo seria editar cuatro sitios del
 * codigo de la referencia para ahorrar un recorrido de unas pocas lineas de
 * cache, y la regla es no tocar lo que la referencia tiene. */
static __inline void ICInvalidateRange(void *startaddr, unsigned int len)
{
    xenon_icache_invalidate(startaddr, len);
}

/* ------------------------------------------------------------------ */
/* ★★ dcbz/dcbf POR BLOQUE DE 32 BYTES: NO-OPS, Y HAY DOS RAZONES      */
/* ------------------------------------------------------------------ */
/* `Textures.cpp` envuelve cada bloque de textura de 32 bytes en
 *
 *     __asm__ volatile("dcbz %y0" : "=Z"(*dest) :: "memory");   // antes
 *     ...escribe los 32 bytes...
 *     __asm__ volatile("dcbf %y0" : "=Z"(*dest) :: "memory");   // despues
 *
 * (16 sitios).  Son inline asm de GCC, que el XDK no acepta, asi que hay que
 * hacer algo con ellos.  Y la respuesta correcta es NO IMPLEMENTARLOS:
 *
 * 1. ★ `dcbz` SERIA PELIGROSO.  Pone a cero UNA LINEA DE CACHE ENTERA.  En
 *    Gekko/Broadway la linea son 32 bytes y encaja con el bloque; en Xenon
 *    son **128**.  Un dcbz "sobre 32 bytes" borraria 96 bytes de memoria
 *    AJENA: corrupcion silenciosa, dependiente del layout y de las que
 *    aparecen semanas despues.  Aqui es puramente una optimizacion (evita el
 *    read-for-ownership de una linea que se va a sobrescribir entera), asi
 *    que quitarla no cambia ningun resultado.
 *
 * 2. `dcbf` es coherencia CPU->GPU, y en este backend NO HACE FALTA: los
 *    texels en formato GX no los lee nunca la GPU.  Los lee la CPU, en
 *    `GX_LoadTexObj` (xenon/gx_d3d9.cpp), que los destila a una textura de
 *    D3D.  O sea que el dato va CPU -> CPU -> textura, y ahi la cache es
 *    coherente por construccion.  Donde SI hace falta volcar -- la textura
 *    de D3D-- ya esta el DCFlushRange de arriba.
 *
 * Si algun dia el backend pasara a darle esos bytes directamente a la GPU,
 * ESTA es la nota que hay que releer. */
#define GX_DCBZ32(p)  ((void)0)
#define GX_DCBF32(p)  ((void)0)

/* `memalign` es de GNU; MSVC tiene `_aligned_malloc`, CON LOS ARGUMENTOS AL
 * REVES.  El plugin solo lo usa para darle memoria al monton de texturas
 * (Textures.cpp:658, VI.cpp:448) y NUNCA la libera, asi que no hay que
 * preocuparse del `_aligned_free`.
 *
 * ★ OJO AL LLAMARLA DESDE CODIGO NUESTRO: al ser macro, el puntero que
 * devuelve es de `_aligned_malloc` y hay que soltarlo con `_aligned_free`,
 * NUNCA con `free`.  Lo mas sano es no soltarlo, como hace el plugin.
 *
 * El sitio de `Textures.cpp:658` NO se ejecuta en nuestro build:
 * `gln64_config.cpp` monta `GXtexCache` antes -- con una arena mucho mayor,
 * porque `GX_TEXTURE_CACHE_SIZE` es el presupuesto de la GameCube; la nota
 * larga esta alli.  Pero tiene que compilar igual. */
#ifndef memalign
#define memalign(align, size) _aligned_malloc((size), (align))
#endif

/* `lrint` es de C99 y MSVC 2010 no la trae.  VI.cpp:72 y :80. */
#ifndef lrint
#define lrint(x) ((long)((x) >= 0.0 ? ((x) + 0.5) : ((x) - 0.5)))
#endif

/* (`crc32` se declara mas abajo, DENTRO del bloque `extern "C"`.) */

#else  /* build que no es Xbox 360 */
#ifndef _sync
#define _sync() ((void)0)
#endif
static __inline void DCFlushRange(void *a, unsigned int n) { (void)a; (void)n; }
static __inline void DCStoreRange(void *a, unsigned int n) { (void)a; (void)n; }
static __inline void DCInvalidateRange(void *a, unsigned int n) { (void)a; (void)n; }
static __inline void DCZeroRange(void *a, unsigned int n) { (void)a; (void)n; }
static __inline void ICInvalidateRange(void *a, unsigned int n) { (void)a; (void)n; }
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -- tipos de libogc propios de GX ---------------------------------------- */

/* GXColor: cuatro bytes. */
typedef struct _gx_color {
    u8 r, g, b, a;
} GXColor;

/* ★ LA UNICA DIVERGENCIA DE SINTAXIS DEL ARBOL COPIADO.
 *
 * glN64_GX construye colores con LITERALES COMPUESTOS de C99,
 * `(GXColor){0,0,0,255}`, que GCC acepta en C++ como extension y MSVC NO.
 * No hay forma de hacerlos validos desde la cabecera: `(T){...}` es un cast
 * aplicado a una lista de inicializacion, y eso no existe en C++03.
 *
 * En glN64_GX son NUEVE sitios y los nueve tienen EXACTAMENTE el mismo valor
 * (negro opaco), asi que la divergencia se reduce a un reemplazo textual
 * uniforme por `GXCOL_BLACK`.  Estan listados en
 * `xenon/gxshim/DIVERGENCIAS.txt` para que el diff contra Wii64 siga siendo
 * auditable.
 *
 * ★ [Fase 6] Rice_GX trae SEIS mas, en dos ficheros, y NO son todos iguales:
 * cuatro negros opacos, uno negro TRANSPARENTE (OGLRender.cpp:344, el color
 * de borrado) y uno azul opaco (OGLRender.cpp:960, que ademas es una
 * inicializacion y no una asignacion, asi que ahi bastan las llaves sin el
 * cast y no hace falta constante ninguna).  Estan en
 * `xenon/gxshim/DIVERGENCIAS_RICE.txt`.
 *
 * ★ Leer el valor ANTES de reemplazar, no dar por hecho que es negro: el de
 * borrado con alfa 0 y el negro opaco se escriben casi igual y se sustituyen
 * por constantes distintas. */
extern const GXColor GXCOL_BLACK;
extern const GXColor GXCOL_TRANSP;   /* {0,0,0,0} -- negro CON ALFA CERO */

/* Matrices de libogc: filas de 4 floats.  Mtx es 3x4 (modelview, la ultima
 * fila es implicita), Mtx44 es 4x4 (proyeccion).  El plugin las rellena a
 * mano campo a campo, asi que el layout tiene que ser este exactamente. */
typedef f32 Mtx[3][4];
typedef f32 Mtx44[4][4];

typedef struct _gx_vec { f32 x, y, z; } guVector;

/* Objeto de textura.  En libogc son 32 bytes opacos; aqui guardamos lo que
 * el backend D3D9 necesita.  El plugin solo lo pasa por puntero a
 * GX_InitTexObj / GX_InitTexObjLOD / GX_LoadTexObj y lo tiene EMPOTRADO por
 * valor dentro de CachedTexture (Textures.h:31), asi que el tamano importa
 * para el layout de esa struct, pero nadie lo serializa. */
typedef struct _gx_texobj {
    void *data;            /* puntero a los texels en formato GX          */
    u16   width, height;   /* dimensiones en texels                        */
    u8    format;          /* GX_TF_*                                      */
    u8    wrap_s, wrap_t;  /* GX_CLAMP / GX_REPEAT / GX_MIRROR             */
    u8    mipmap;
    u8    minfilt, magfilt;
    u8    biasclamp, edgelod, maxaniso;
    f32   minlod, maxlod, lodbias;
    /* ★ NO hay puntero a la textura de D3D aqui, a proposito.  Esta struct
     * va empotrada en `CachedTexture` y NACE SIN INICIALIZAR
     * (`TextureCache_AddTop` solo pone `GXtexture = NULL`,
     * Textures.cpp:903), asi que cualquier campo nuestro seria basura al
     * leerlo.  Ademas el plugin llama a `GX_InitTexObj` en cada ACTIVACION,
     * no al crear, con lo que un backend guardado aqui se perdia (y se
     * filtraba) 3554 veces en tres segundos.
     * La textura de D3D vive en el registro con LRU de gx_d3d9.cpp. */
} GXTexObj;

/* Modo de video.  Solo se leen unos pocos campos (aa, sample_pattern,
 * vfilter, fbWidth, efbHeight, xfbHeight, viWidth, viHeight). */
typedef struct _gx_rmodeobj {
    u32 viTVMode;
    u16 fbWidth, efbHeight, xfbHeight;
    u16 viXOrigin, viYOrigin, viWidth, viHeight;
    u32 xfbMode;
    u8  field_rendering, aa;
    u8  sample_pattern[12][2];
    u8  vfilter[7];
} GXRModeObj;

typedef void (*GXDrawSyncCallback)(u16 token);

/* -- constantes ----------------------------------------------------------- */
/* Booleanos y habilitacion */
#define GX_FALSE                 0
#define GX_TRUE                  1
#define GX_DISABLE               0
#define GX_ENABLE                1

/* [Xbox360] Valores EXTRA del `clamp` de GX_SetTevColorOp/GX_SetTevAlphaOp
 * (no existen en GX): en vez de saturar, ENVOLVER como el "sign-extend" del
 * N64 que emula GLideN64 al final del primer ciclo (WRAP(x, low, high) =
 * mod(x - low, high - low) + low, glsl_CombinerProgramBuilderCommon.cpp:
 * 445/456 y 477/488).  _C: el resultado entra como C del segundo ciclo,
 * [-1.01, 1.01).  _ABD: entra como A, B o D, [-0.51, 1.51).  Solo los pone
 * TEV_combiner.cpp. */
#define GXD3D_TEV_WRAP_C         2
#define GXD3D_TEV_WRAP_ABD       3

/* Primitivas (valores reales de GX) */
#define GX_QUADS                 0x80
#define GX_TRIANGLES             0x90
#define GX_LINES                 0xA8

/* Formatos de vertice */
#define GX_VTXFMT0               0
#define GX_VTXFMT1               1

/* Atributos de vertice */
#define GX_VA_PTNMTXIDX          0
#define GX_VA_TEX0MTXIDX         1
#define GX_VA_TEX1MTXIDX         2
#define GX_VA_TEX2MTXIDX         3
#define GX_VA_POS                9
#define GX_VA_NRM                10
#define GX_VA_CLR0               11
#define GX_VA_TEX0               13
#define GX_VA_TEX1               14
#define GX_VA_TEX2               15

/* Tipo de descriptor de atributo */
#define GX_NONE                  0
#define GX_DIRECT                1

/* Formato de componentes */
#define GX_POS_XY                0
#define GX_POS_XYZ               1
#define GX_NRM_XYZ               0
#define GX_CLR_RGBA              1
#define GX_TEX_ST                1
#define GX_RGBA8                 5
#define GX_F32                   4

/* Matrices */
#define GX_PNMTX0                0
#define GX_PNMTX1                3
#define GX_PNMTX2                6
#define GX_TEXMTX0               30
#define GX_IDENTITY              60
#define GX_PERSPECTIVE           0
#define GX_ORTHOGRAPHIC          1

/* Culling */
#define GX_CULL_NONE             0
#define GX_CULL_FRONT            1
#define GX_CULL_BACK             2
#define GX_CULL_ALL              3

/* Comparaciones (Z y alpha) */
#define GX_NEVER                 0
#define GX_LESS                  1
#define GX_EQUAL                 2
#define GX_LEQUAL                3
#define GX_GREATER               4
#define GX_NEQUAL                5
#define GX_GEQUAL                6
#define GX_ALWAYS                7

/* Operacion logica de los dos tests de alpha */
#define GX_AOP_AND               0
#define GX_AOP_OR                1
#define GX_AOP_XOR               2
#define GX_AOP_XNOR              3

/* Mezcla */
#define GX_BM_NONE               0
#define GX_BM_BLEND              1
#define GX_BM_LOGIC              2
#define GX_BM_SUBTRACT           3
#define GX_BL_ZERO               0
#define GX_BL_ONE                1
#define GX_BL_SRCCLR             2
#define GX_BL_INVSRCCLR          3
#define GX_BL_SRCALPHA           4
#define GX_BL_INVSRCALPHA        5
#define GX_BL_DSTALPHA           6
#define GX_BL_INVDSTALPHA        7
/* ★★ [Fase 6] NO SON CONSTANTES NUEVAS: SON ALIAS DE LAS DOS DE ARRIBA, y
 * asi estan en libogc.  El campo del factor tiene 3 bits en el hardware y el
 * valor 2 significa "el OTRO color", asi que depende de la ranura:
 *
 *     GX_SetBlendMode(tipo, src_fact, dst_fact, op)
 *        src_fact == 2  ->  color de DESTINO   (por eso se escribe DSTCLR)
 *        dst_fact == 2  ->  color de ORIGEN    (por eso se escribe SRCCLR)
 *
 * Los pide `Rice_GX/TEVBlender.cpp:41-42`, en la tabla `GX_BlendFuncMaps`,
 * que se indexa con el mismo array para las DOS ranuras (lineas 74-75).
 *
 * ★ Y POR ESO `gx_d3d9.cpp` NECESITA DOS TABLAS DE TRADUCCION, no una.  El
 * shim tenia `s_blendMap[factor & 7]` para las dos, lo que da SRCCOLOR
 * tambien en la ranura de origen: correcto para dst, mal para src.  glN64
 * nunca pasa 2 ni 3 en la ranura de origen, asi que llevaba latente desde el
 * principio.  El sintoma habria sido un color mezclado mal, sin ningun
 * error. */
#define GX_BL_DSTCLR             GX_BL_SRCCLR
#define GX_BL_INVDSTCLR          GX_BL_INVSRCCLR
#define GX_LO_CLEAR              0
#define GX_LO_COPY               3
#define GX_LO_SET                15

/* Recorte */
#define GX_CLIP_ENABLE           0
#define GX_CLIP_DISABLE          1

/* Niebla */
#define GX_FOG_NONE              0
#define GX_FOG_ORTHO_LIN         10

/* Anchura de linea */
#define GX_TO_ZERO               0
#define GX_TO_SIXTEENTH          1

/* Canales de color */
#define GX_COLOR0                0
#define GX_COLOR0A0              4
/* [Fase 6] Lo pide Rice en dos `GX_SetTevOrder` (TEVCombiner.cpp:180 y
 * TextureManager.cpp:1077).  Significa "esta etapa no toma color de vertice;
 * el rasterizado vale CERO", y es distinto de GX_COLORNULL, que es "no hay
 * canal".
 *
 * ★ HOY NO CAMBIA EL SHADER QUE SE EMITE, y conviene saberlo antes de mirar
 * una pantalla rara: `GX_SetTevOrder` guarda el canal en la clave
 * (gx_d3d9_tev.cpp:471) pero el generador de HLSL no lo lee todavia.  O sea
 * que una etapa con GX_COLORZERO produce su propio shader, distinto del de
 * GX_COLOR0A0, pero con el mismo codigo dentro.  Es una simplificacion
 * heredada de glN64, que nunca usa este valor; si Rice sale con color de mas
 * en alguna etapa, este es el primer sitio donde mirar. */
#define GX_COLORZERO             6
#define GX_COLORNULL             255

/* Coordenadas de textura y mapas */
#define GX_TEXCOORD0             0
#define GX_TEXCOORD1             1
#define GX_TEXCOORD2             2
#define GX_TEXCOORDNULL          255
#define GX_TEXMAP0               0
#define GX_TEXMAP1               1
#define GX_TEXMAP2               2
#define GX_TEXMAP3               3
#define GX_TEXMAP_NULL           255

/* ★ `GX_MAX_TEXTURES` y `GX_TEXTURE_CACHE_SIZE` NO SE DEFINEN AQUI, y es
 * importante: NO son de libogc, las define el propio plugin en
 * `Textures.h:89-99` segun la maquina (8 MB en Wii, 2,4 MB en GameCube) y
 * `GX_MAX_TEXTURES` sale de una cuenta, `((400*1024)/sizeof(CachedTexture))`.
 *
 * Mi primera version las metio aqui con valores inventados -- 1 MB y **8**
 * texturas-- porque el censo de simbolos `GX_*` las capturo por su USO y el
 * filtro de "¿las define el plugin?" no las vio: usan `# define` con espacio
 * e indentado, y el patron solo miraba `#define` a principio de linea.
 *
 * El dano habria sido silencioso: gana el define que llegue el segundo (aqui
 * el del plugin, porque Textures.h incluye gccore.h antes), asi que el build
 * habria pasado... y en otro orden de includes la cache de texturas se habria
 * quedado en OCHO entradas.  La leccion es la de siempre: antes de shimear un
 * simbolo, comprobar que no lo define ya quien lo usa. */

/* Generacion de coordenadas */
#define GX_TG_MTX3x4             0
#define GX_TG_MTX2x4             1
#define GX_MTX3x4                0
#define GX_MTX2x4                1
#define GX_TG_POS                0
#define GX_TG_NRM                1
#define GX_TG_TEX0               4
#define GX_TG_TEX1               5

/* Direccionamiento y filtrado */
#define GX_CLAMP                 0
#define GX_REPEAT                1
#define GX_MIRROR                2
#define GX_NEAR                  0
#define GX_LINEAR                1
#define GX_NEAR_MIP_NEAR         2
#define GX_LIN_MIP_NEAR          3
#define GX_NEAR_MIP_LIN          4
#define GX_LIN_MIP_LIN           5
#define GX_ANISO_1               0
#define GX_ANISO_2               1
#define GX_ANISO_4               2

/* ★ Modo de copia del EFB.  Entro con la puesta al dia a Wii64 1b5041a, que
 * cambio el ultimo argumento de las tres `GX_SetTexCopyDst` de FrameBuffer.cpp
 * de `GX_FALSE` a `GX_COPY_PROGRESSIVE`.
 *
 * OJO: ese argumento se llama `mipmap` en la firma de libogc
 * (`GX_SetTexCopyDst(u16 wd, u16 ht, u32 fmt, u8 mipmap)`), asi que el cambio
 * de upstream es cosmetico -- las dos constantes valen 0.  Aqui da igual
 * ademas por partida doble: nuestro `GX_SetTexCopyDst` ignora el parametro
 * (`(void)mipmap`), porque el Resolve de Xenos no tiene ese concepto. */
#define GX_COPY_PROGRESSIVE      0
#define GX_COPY_INTLC_EVEN       2
#define GX_COPY_INTLC_ODD        3

/* Formatos de textura de GameCube (importan: definen el empaquetado que
 * Textures.cpp produce y que el shim tiene que desempaquetar) */
#define GX_TF_I4                 0x0
#define GX_TF_I8                 0x1
#define GX_TF_IA4                0x2
#define GX_TF_IA8                0x3
#define GX_TF_RGB565             0x4
#define GX_TF_RGB5A3             0x5
#define GX_TF_RGBA8              0x6
#define GX_TF_CI4                0x8
#define GX_TF_CI8                0x9
#define GX_TF_Z8                 0x11
#define GX_TF_Z16                0x13
#define GX_TF_Z24X8              0x16
#define GX_TF_A8                 0x27

/* Z texture */
#define GX_ZT_DISABLE            0
#define GX_ZT_ADD                1
#define GX_ZT_REPLACE            2
#define GX_MAX_Z24               0x00FFFFFF

/* -- TEV ------------------------------------------------------------------ */
/* Etapas */
#define GX_TEVSTAGE0             0
#define GX_TEVSTAGE1             1
#define GX_TEVSTAGE2             2
#define GX_TEVSTAGE3             3
#define GX_MAX_TEVSTAGE          16

/* Registros de salida / entrada del TEV */
#define GX_TEVPREV               0
#define GX_TEVREG0               1
#define GX_TEVREG1               2
#define GX_TEVREG2               3

/* Constantes de color (konst) */
#define GX_KCOLOR0               0
#define GX_KCOLOR1               1
#define GX_KCOLOR2               2
#define GX_KCOLOR3               3

/* Entradas del combinador de COLOR (valores reales de GX) */
#define GX_CC_CPREV              0
#define GX_CC_APREV              1
#define GX_CC_C0                 2
#define GX_CC_A0                 3
#define GX_CC_C1                 4
#define GX_CC_A1                 5
#define GX_CC_C2                 6
#define GX_CC_A2                 7
#define GX_CC_TEXC               8
#define GX_CC_TEXA               9
#define GX_CC_RASC               10
#define GX_CC_RASA               11
#define GX_CC_ONE                12
#define GX_CC_HALF               13
#define GX_CC_KONST              14
#define GX_CC_ZERO               15

/* Entradas del combinador de ALFA */
#define GX_CA_APREV              0
#define GX_CA_A0                 1
#define GX_CA_A1                 2
#define GX_CA_A2                 3
#define GX_CA_TEXA               4
#define GX_CA_RASA               5
#define GX_CA_KONST              6
#define GX_CA_ZERO               7

/* [Xbox360] ★ EXTENSION DEL SHIM, NO ES GX: la entrada NOISE del combinador
 * como una entrada mas del TEV (ver GXD3D_SetRuido).  En GX solo podria ser
 * una textura -- y una etapa tiene UNA --, asi que con NOISE y TEXEL0 en la
 * misma etapa NOISE leia TEXEL0 (Super Smash Bros., log 29). */
#define GXD3D_CC_RUIDO           16
#define GXD3D_CA_RUIDO           8
/* ★ EXTENSION DEL SHIM.  La entrada LOD_FRACTION del combinador del N64, como
 * GLideN64 (la funcion mipmap() de glsl_CombinerProgramBuilderAccurate.cpp,
 * rama sin atlas): la calcula el pixel shader por pixel con ddx/ddy.  Ver
 * GXD3D_SetLod. */
#define GXD3D_CC_LODFRAC         17
#define GXD3D_CA_LODFRAC         9

/* Operacion, bias, escala */
#define GX_TEV_ADD               0
#define GX_TEV_SUB               1
#define GX_TB_ZERO               0
#define GX_TB_ADDHALF            1
#define GX_TB_SUBHALF            2
#define GX_CS_SCALE_1            0
#define GX_CS_SCALE_2            1
#define GX_CS_SCALE_4            2
#define GX_CS_DIVIDE_2           3

/* Modos predefinidos de GX_SetTevOp.  Valores reales de libogc.
 *
 * ★ OJO con `GX_BLEND`: en libogc es a la vez el nombre de un op del TEV
 * (valor 2) y se confunde facil con `GX_BM_BLEND` (valor 1), que es el modo
 * de MEZCLA de GX_SetBlendMode.  Comprobado que glN64_GX solo menciona
 * `GX_BLEND` en un COMENTARIO (Combiner.cpp... concretamente
 * TEV_combiner.cpp:204, "TevOps are: GX_MODULATE, GX_DECAL, GX_BLEND,
 * GX_REPLACE, GX_PASSCLR") y nunca como valor, asi que la ambiguedad no
 * muerde; aun asi se define con el valor correcto de libogc y no como alias
 * de GX_BM_BLEND, que era la trampa. */
#define GX_MODULATE              0
#define GX_DECAL                 1
#define GX_BLEND                 2
#define GX_REPLACE               3
#define GX_PASSCLR               4

/* Seleccion de konst para color */
#define GX_TEV_KCSEL_1           0x00
#define GX_TEV_KCSEL_7_8         0x01
#define GX_TEV_KCSEL_3_4         0x02
#define GX_TEV_KCSEL_5_8         0x03
#define GX_TEV_KCSEL_1_2         0x04
#define GX_TEV_KCSEL_K0          0x0C
#define GX_TEV_KCSEL_K1          0x0D
#define GX_TEV_KCSEL_K2          0x0E
#define GX_TEV_KCSEL_K3          0x0F
#define GX_TEV_KCSEL_K0_R        0x10
#define GX_TEV_KCSEL_K0_A        0x1C
#define GX_TEV_KCSEL_K1_A        0x1D
#define GX_TEV_KCSEL_K2_A        0x1E
#define GX_TEV_KCSEL_K3_A        0x1F

/* Seleccion de konst para alfa */
#define GX_TEV_KASEL_1           0x00
#define GX_TEV_KASEL_K0_A        0x1C
#define GX_TEV_KASEL_K1_A        0x1D
#define GX_TEV_KASEL_K2_A        0x1E
#define GX_TEV_KASEL_K3_A        0x1F

/* Swap tables */
#define GX_TEV_SWAP0             0
#define GX_TEV_SWAP1             1
#define GX_TEV_SWAP2             2
#define GX_TEV_SWAP3             3

/* -- geometria en modo inmediato ------------------------------------------ */
/* El plugin usa el patron clasico GX_Begin / atributos / GX_End.  El shim lo
 * acumula en el vertex buffer de D3D9 (el mismo anillo VB_COUNT que ya usa
 * d3d9_backend.cpp) y emite un DrawPrimitive en GX_End. */
void GX_Begin(u8 primitive, u8 vtxfmt, u16 nverts);
void GX_End(void);
void GX_Position3f32(f32 x, f32 y, f32 z);
void GX_Position2f32(f32 x, f32 y);
void GX_Color4u8(u8 r, u8 g, u8 b, u8 a);
void GX_TexCoord2f32(f32 s, f32 t);

/* Descriptores de vertice.  En nuestro backend el layout es FIJO, asi que
 * estas dos son casi no-op: solo anotan que atributos vienen para que el
 * shim sepa cuantos floats consumir por vertice. */
void GX_ClearVtxDesc(void);
void GX_SetVtxDesc(u8 attr, u8 type);
void GX_SetVtxAttrFmt(u8 vtxfmt, u32 vtxattr, u32 comptype, u32 compsize, u32 frac);

/* -- estado del rasterizador ---------------------------------------------- */
void GX_SetCullMode(u8 mode);
void GX_SetZMode(u8 enable, u8 func, u8 update_enable);
void GX_SetZCompLoc(u8 before_tex);
void GX_SetBlendMode(u8 type, u8 src_fact, u8 dst_fact, u8 op);
void GX_SetAlphaCompare(u8 comp0, u8 ref0, u8 aop, u8 comp1, u8 ref1);
void GX_SetAlphaUpdate(u8 enable);
void GX_SetColorUpdate(u8 enable);
void GX_SetDstAlpha(u8 enable, u8 a);
void GX_SetScissor(u32 xOrigin, u32 yOrigin, u32 wd, u32 ht);
void GX_SetViewport(f32 xOrig, f32 yOrig, f32 wd, f32 ht, f32 nearZ, f32 farZ);
void GX_SetClipMode(u8 mode);
void GX_SetCoPlanar(u8 enable);
void GX_SetLineWidth(u8 width, u8 fmt);
void GX_SetFog(u8 type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor col);

/* -- matrices -------------------------------------------------------------- */
void GX_LoadPosMtxImm(Mtx mt, u32 pnidx);
void GX_LoadProjectionMtx(Mtx44 mt, u8 type);
void GX_LoadTexMtxImm(Mtx mt, u32 texidx, u8 type);

/* -- TEV: aqui vive el combinador ------------------------------------------ */
/* Estas 16 funciones NO tocan la GPU: acumulan la descripcion de las etapas
 * del TEV en el estado del shim.  El pixel shader HLSL se genera/selecciona
 * en el GX_End, a partir de esa descripcion.  Es el equivalente exacto de lo
 * que hace TEV_combiner.cpp contra el TEV de GameCube, y la razon de que
 * esta via sea barata en GPU: son POCOS shaders parametrizados, no uno por
 * mux del RDP como hace Rice. */
void GX_SetTevOrder(u8 tevstage, u8 texcoord, u32 texmap, u8 color);
void GX_SetTevColorIn(u8 tevstage, u8 a, u8 b, u8 c, u8 d);
void GX_SetTevAlphaIn(u8 tevstage, u8 a, u8 b, u8 c, u8 d);
void GX_SetTevColorOp(u8 tevstage, u8 tevop, u8 tevbias, u8 tevscale, u8 clamp, u8 tevregid);
void GX_SetTevAlphaOp(u8 tevstage, u8 tevop, u8 tevbias, u8 tevscale, u8 clamp, u8 tevregid);
void GX_SetTevColor(u8 tev_regid, GXColor color);
void GX_SetTevKColor(u8 tev_kcregid, GXColor color);
void GX_SetTevKColorSel(u8 tevstage, u8 sel);
void GX_SetTevKAlphaSel(u8 tevstage, u8 sel);
void GX_SetTevSwapMode(u8 tevstage, u8 ras_sel, u8 tex_sel);
void GX_SetTevOp(u8 tevstage, u8 mode);
void GX_SetNumTevStages(u8 num);
void GX_SetNumChans(u8 num);
void GX_SetNumTexGens(u32 nr);
void GX_SetTexCoordGen(u16 texcoord, u32 tgen_typ, u32 tgen_src, u32 mtxsrc);
void GX_SetZTexture(u8 op, u8 fmt, u32 bias);
/* ★ EXTENSION DEL SHIM, NO ES GX.  Como lee GLideN64 un framebuffer de 8 bits
 * usado como textura (`UFrameBufferInfo`, uFbMonochrome/uFbFixedAlpha): el
 * buffer guarda UN canal, y el texel sale de ese canal en los cuatro
 * (`.rrrr`); con IMAGE_READ apagado, ademas, alfa = 0.825.
 *   GXD3D_SetCopyMono: marca la copia `dest` (la de un GX_CopyTex ya hecho)
 *                      como monocroma o no.  Viaja con la textura: al ligarla
 *                      en GX_TEXMAP0/1 se aplica, y al ligar otra se apaga.
 *   GXD3D_SetMonoAlfaFija: el IMAGE_READ apagado del dibujo en curso.
 * Las dos van en la clave del shader. */
void GXD3D_SetCopyMono(void *dest, u8 mono);
void GXD3D_SetMonoAlfaFija(u8 on);
/* ★ EXTENSION DEL SHIM, NO ES GX.  El blender del N64
 *     CLR_IN * A_FOG + CLR_FOG * (1 - A_FOG)
 * que los factores de mezcla no pueden expresar (lleva un termino constante):
 * se aplica al final del pixel shader, como hace GLideN64 sin
 * `enableLegacyBlending`.  `fog` = color de niebla, con A_FOG en `.a`.
 * Con `.a` = 0 sale `fog` tal cual (blender sin FORCE_BL cuyo primer mux es
 * el color de mezcla o el de niebla).  `on` = 2 [F-Zero X]:
 * CLR_FOG * A_SHADE + CLR_IN * (1 - A), el peso es el alfa del vertice. */
void GXD3D_SetBlenderNiebla(u8 on, GXColor fog);
/* ★ EXTENSION DEL SHIM, NO ES GX.  La niebla de GX_SetFog con el peso
 * invertido: `lerp(color, niebla, 1 - f)` en vez de `lerp(color, niebla, f)`,
 * para el blender del N64 `CLR_IN * A_SHADE + CLR_FOG * (1 - A_SHADE)`, donde
 * A_SHADE = 1 es SIN niebla (Nightmare Creatures, blender 0x0AF0). */
void GXD3D_SetNieblaInversa(u8 on);
/* ★ EXTENSION DEL SHIM, NO ES GX.  La entrada NOISE del combinador, como
 * GLideN64 (snoise(), glsl_CombinerProgramBuilderCommon.cpp:1004-1022): las
 * entradas GXD3D_CC_RUIDO / GXD3D_CA_RUIDO del TEV valen
 * 0.5 + 0.5 * hash13(semilla, pixel del N64).  `semilla` cambia
 * cada frame (GLideN64: contador de swaps & 0xFF); escX/escY = pixeles del
 * EFB por pixel del N64 (su uScreenScale). */
void GXD3D_SetRuido(f32 semilla, f32 escX, f32 escY);
/* ★ EXTENSION DEL SHIM, NO ES GX.  Los datos de la entrada LOD_FRACTION
 * (GXD3D_CC_LODFRAC / GXD3D_CA_LODFRAC), los uniformes de mipmap() de GLideN64:
 *   kS, kT    paso de IN.uv0 a texels del N64 por pixel del N64:
 *             uScreenScale / (shiftScale * scale) de la textura del tile 0;
 *   minLod    uMinLod = min_level de SetPrimColor (entre 32);
 *   maxTile   uMaxTile = gSP.texture.level;
 *   detalle   uTextureDetail = otherMode.textureDetail (0 clamp, 1 sharpen,
 *             2 detail);
 *   activo    uEnableLod = otherMode.textureLOD == G_TL_LOD.
 * Solo dos tiles (el 0 y el 1, los dos texmap): es la rama `uNoAtlasTex` de
 * GLideN64, que es la del cuadro de Peach/Bowser de Super Mario 64. */
void GXD3D_SetLod(f32 kS, f32 kT, f32 minLod, f32 maxTile, f32 detalle, f32 activo);
/* ★ EXTENSION DEL SHIM, NO ES GX.  El ATLAS de mipmap de GLideN64
 * (_loadAccurate + TextureMipMap1): con `activo`, el texmap 1 no es una
 * textura sino un RGBA8 de 256 x atlasH con una cabecera de 8 texels (uno por
 * nivel) y los niveles en fila detras.  Lo arma TextureCache_LoadAtlas
 * (glN64_GX/Textures.cpp), que documenta el formato.
 *   atlasW/H        tamano del atlas (atlasW = 256);
 *   realW1/realH1   tamano del nivel 0 (el tile 1), el de las UV de IN.uv1;
 *   bilineal        el filtro del juego (otherMode.textureFilter != POINT);
 *   nivMax          ultimo nivel del atlas (niveles - 1);
 *   clampS/clampT   clamp del nivel 0: si no, la coordenada da la vuelta.
 * Solo cuenta en shaders con LOD_FRACTION (GXD3D_CC_LODFRAC). */
void GXD3D_SetLodAtlas(int activo, f32 atlasW, f32 atlasH, f32 realW1, f32 realH1,
                       f32 bilineal, f32 nivMax, f32 clampS, f32 clampT);
/* [Xbox360] Destila texels GX (tileados) a ARGB lineal; `w` y `h` son los
 * rellenados al bloque (GXrealWidth/GXrealHeight) y `dst` mide w*h u32. */
void GXD3D_DecodificaLineal(const void *gx, u8 fmt, int w, int h, u32 *dst);
/* ★ EXTENSION DEL SHIM, NO ES GX.  GL_DEPTH_CLAMP de GLideN64
 * (`ClampMode::NoNearPlaneClipping`) para los microcodigos NoN: sin recorte
 * contra los planos cercano y lejano, con la profundidad sujeta a [0,1] por
 * pixel.  Cuesta la escritura de profundidad en el pixel shader, asi que el
 * plugin solo lo pide para los triangulos que lo necesitan.  Vacia el lote
 * pendiente al cambiar. */
void GXD3D_SetDepthClamp(u8 on);
/* ★ EXTENSION DEL SHIM, NO ES GX.  Dibujar con el color image = depth image,
 * como GLideN64 (ccff245f "Implement render to depth buffer. Fixed shadows in
 * Pilot Wings"; hoy `uRenderTarget`): el pixel shader escribe como
 * profundidad el ROJO del color.  `modo` 0 = normal; 1 = solo eso; 2 = ademas
 * el pixel se descarta si su propia z no es menor que la de una COPIA del Z
 * buffer, que el shim hace en el primer dibujo del modo 2 tras
 * GXD3D_InvalidaCopiaZ.  El plugin pone ademas el test del device en ALWAYS
 * con escritura.  Vacia el lote pendiente al cambiar.  3 [Mario Golf] = el
 * Z guardado de GXD3D_UsaZGuardada, pixel a pixel; 4 [Zelda MM, Lente de la
 * Verdad] = lo mismo solo donde el Z actual vale 0 (GLideN64 5347689b).
 * 5 [Resident Evil 2] = la profundidad es la palabra de Z del N64 del texel
 * (R = byte alto, G = byte bajo; ver GXGetRE2Z_RGBA8 en glN64_GX/Textures.cpp)
 * pasada al Z del EFB con el rango de GXD3D_SetProfundidadRE2. */
void GXD3D_SetRenderToDepth(u8 modo);
/* ★ EXTENSION DEL SHIM, NO ES GX.  [Resident Evil 2] El rango del modo 5 de
 * GXD3D_SetRenderToDepth: el Z de 18 bits del N64, 0..261632, va a
 * `nearz`..`farz`, la inversa del paso de DepthCopy.cpp.  Vacia el lote
 * pendiente si cambia en el modo 5. */
void GXD3D_SetProfundidadRE2(f32 nearz, f32 farz);
/* La copia de Z de GXD3D_SetRenderToDepth deja de valer (el plugin la tira
 * al cambiar de color image, como GLideN64 al volver a enganchar el depth
 * buffer: `m_copied = false`). */
void GXD3D_InvalidaCopiaZ(void);
/* ★ EXTENSION DEL SHIM, NO ES GX.  [Mario Golf] Z guardados por direccion
 * (GLideN64 tiene un Z por buffer; aqui solo el de la EDRAM).  GuardaZ copia
 * el Z actual como "Z de `dir`" (sin esperas); UsaZGuardada elige el de `dir`
 * para el modo 3 de GXD3D_SetRenderToDepth (escribe ese Z pixel a pixel) y
 * devuelve 0 si no hay; ZEscrituras cuenta los lotes que han escrito Z, para
 * saber si merece la pena guardar. */
void GXD3D_GuardaZ(u32 dir);
int  GXD3D_UsaZGuardada(u32 dir);
unsigned int GXD3D_ZEscrituras(void);
/* ★ EXTENSION DEL SHIM, NO ES GX.  El modo decal del N64 (ZMODE_DEC) como
 * GLideN64 con N64DepthCompare: el pixel pasa solo si su z esta a la par de
 * la del Z buffer, dentro de 4*fwidth(z), por los dos lados.  Se prueba en el
 * pixel shader contra una copia de Z que el shim rehace si el Z cambio desde
 * la anterior.  El plugin pone el test del device en ALWAYS y quita el offset
 * de poligono.  Vacia el lote pendiente al cambiar. */
void GXD3D_SetDecalN64(u8 on);
/* ★ EXTENSION DEL SHIM, NO ES GX.  [Beetle Adventure Racing] La niebla por Z
 * de GLideN64 (texturedRectShadowMap / ShadowMapShader): mientras esta
 * puesta, cada pixel sale del color de `color` con alfa = `alfa[i]`, donde i
 * es el indice de 8 bits del Z del N64 que hay en ESE pixel (la copia de Z,
 * rehecha si el Z cambio, pasada al formato del N64 como DepthCopy.cpp: el
 * rango `nearz`..`farz` del viewport a 0..261632, y solo en el valor del
 * borrado del Z la palabra de borrado `borrado`).  `alfa` son las 256 entradas de la TLUT >> 8.  El
 * plugin pone el blend SRCALPHA/INVSRCALPHA.  Vacia el lote pendiente.
 * Devuelve 0 si no se pudo (sin la textura de la curva): entonces el dibujo
 * es el normal. */
int  GXD3D_SetNieblaZ(u8 on, const u8 *alfa, GXColor color,
                      f32 nearz, f32 farz, u16 borrado);
/* ★ EXTENSION DEL SHIM, NO ES GX.  La copia del color a la RDRAM de GLideN64
 * (copyToRDRAM), sin esperar a la GPU: Pide resuelve el rect del EFB y
 * apunta adonde va (`dest`, en la RDRAM, `n64w` x `n64h` en RGBA5551; `dir`
 * es su direccion N64); Entrega escribe la copia terminada mas nueva (de 1-3
 * frames antes) y devuelve 1 con su direccion y sus bytes, o 0 si no habia
 * ninguna.  Ver "LA COPIA DEL COLOR A LA RDRAM" en xenon/gx_d3d9.cpp. */
void GXD3D_PideColorARDRAM(u16 *dest, u32 dir, u16 x, u16 y, u16 w, u16 h,
                           u16 n64w, u16 n64h);
int  GXD3D_EntregaColorARDRAM(u32 *dir, u32 *bytes);
/* [Paper Mario] GLideN64 1f4d04f4 (hack_paper_mario_subscreen): en la
 * conversion de esa copia, azul += 4 si 0 < azul <= 0xFB.  1 = activo. */
void GXD3D_ColorARDRAMAzulPM(int activo);
/* [Banjo-Kazooie] La copia del color SINCRONA (GLideN64 ctSync): pide como
 * GXD3D_PideColorARDRAM y espera a la GPU; devuelve 1 con los bytes escritos
 * en `dest` (de ESTE frame), o 0 si no llego a tiempo (entonces queda pedida
 * y la escribe la entrega de siempre).  Interruptor GXD3D_CCOPIA_SINC en
 * xenon/gx_d3d9.cpp. */
int  GXD3D_CopiaColorARDRAMSincrona(u16 *dest, u32 dir, u16 x, u16 y, u16 w, u16 h,
                                    u16 n64w, u16 n64h, u32 *bytes);

/* -- texturas -------------------------------------------------------------- */
/* [Xbox360] Los texels de `data` han cambiado: el proximo GX_LoadTexObj con
 * ese puntero los vuelve a subir sobre la MISMA textura de D3D en vez de
 * crear otra (crear es sincrono).  Para buferes que el plugin reutiliza con
 * contenido nuevo.  Ver GXD3D_TexturaCambiada en gx_d3d9.cpp. */
void GXD3D_TexturaCambiada(void *data);
void GX_InitTexObj(GXTexObj *obj, void *img_ptr, u16 wd, u16 ht, u8 fmt,
                   u8 wrap_s, u8 wrap_t, u8 mipmap);
void GX_InitTexObjLOD(GXTexObj *obj, u8 minfilt, u8 magfilt, f32 minlod,
                      f32 maxlod, f32 lodbias, u8 biasclamp, u8 edgelod,
                      u8 maxaniso);
/* ★ Las usa Rice_GX, no glN64_GX: cambia filtro y repeticion sobre un
 * `GXTexObj` ya inicializado en vez de rehacerlo.  Ver gx_d3d9.cpp. */
void GX_InitTexObjWrapMode(GXTexObj *obj, u8 wrap_s, u8 wrap_t);
void GX_InitTexObjFilterMode(GXTexObj *obj, u8 minfilt, u8 magfilt);
void GX_LoadTexObj(GXTexObj *obj, u8 mapid);
void GX_InvalidateTexAll(void);

/* -- copia del framebuffer empotrado --------------------------------------- */
/* GX_CopyTex es el `Resolve()` de Xenos: EFB -> textura, top-down.
 * GX_CopyDisp es EFB -> framebuffer externo, o sea nuestro
 * xenon64_video_resolve(). */
void GX_SetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht);
void GX_SetTexCopyDst(u16 wd, u16 ht, u32 fmt, u8 mipmap);
void GX_SetCopyClear(GXColor color, u32 zvalue);
void GX_SetCopyFilter(u8 aa, u8 sample_pattern[12][2], u8 vf, u8 vfilter[7]);
void GX_CopyTex(void *dest, u8 clear);
void GX_CopyDisp(void *dest, u8 clear);
/* [Xbox360] GX_CopyDisp que borra el color y CONSERVA el Z (ver gx_d3d9.cpp). */
void GXD3D_CopyDispConservaZ(void *dest);
void GX_PixModeSync(void);

/* -- sincronizacion --------------------------------------------------------- */
void GX_DrawDone(void);
void GX_SetDrawSync(u16 token);
void GX_SetDrawSyncCallback(GXDrawSyncCallback cb);

/* -- lectura del buffer de profundidad del EFB ----------------------------- */
/* gDP.cpp la usa para el hack de Perfect Dark / Turok
 * (hack_rectDepthBufferCopyPD), que lee el z-buffer para decidir si las
 * coronas de luz estan tapadas.  En Xenos no hay lectura por pixel: lee de la
 * ULTIMA copia terminada de la Z del EFB, de 1-3 frames antes, que pide
 * GXD3D_PreparaPeekZ (un Resolve por frame, sin esperas).  Sin copia todavia,
 * el plano lejano.  Ver "GX_PeekZ" en gx_d3d9.cpp.  x, y, w, h: rectangulo del
 * EFB a copiar (el del frame del N64). */
void GXD3D_PreparaPeekZ(u16 x, u16 y, u16 w, u16 h);
void GX_PeekZ(u16 x, u16 y, u32 *z);

/* ★★ EL WRITE GATHER PIPE, Y POR QUE NO ES UN `u32 *`
 * -----------------------------------------------------
 * En la Wii el WGP es el camino rapido CPU->GPU: una UNION en una direccion
 * FIJA (0xCC008000) donde cada escritura a un miembro EMPUJA al FIFO y la
 * direccion no se mueve.  De ahi que libogc lo use como `wgPipe->U32 = x`
 * ocho veces seguidas y salgan 32 bytes consecutivos.
 *
 * Aqui no hay FIFO: `GX_RedirectWriteGatherPipe` apunta a memoria normal y
 * hay que AVANZAR en cada escritura.  La primera version declaraba
 * `extern u32 *wgPipe` y hacia que el unico consumidor -- el bucle del XFB de
 * VI.cpp, que ya era divergencia nuestra-- avanzara el puntero a mano.
 *
 * Con la puesta al dia a Wii64 1b5041a aparecio un SEGUNDO consumidor,
 * `_gDPWriteRDRAM` (gDP.cpp), que usa la sintaxis de libogc de verdad
 * (`wgPipe->F64` y `wgPipe->U32`) y NO avanza nada.  Con un `u32 *` eso no
 * compila, y "arreglarlo" tocando gDP.cpp seria una divergencia mas en codigo
 * copiado.
 *
 * Asi que el shim pasa a emular la union de verdad: un objeto cuyos miembros
 * son proxies con `operator=`, y el cursor de escritura vive en gx_d3d9.cpp.
 * Cada `wgPipe->XXX = v` escribe en el cursor y lo avanza por el tamano del
 * miembro, que es exactamente lo que el WGP hace.  Con esto los DOS
 * consumidores usan la misma sintaxis que upstream.
 *
 * Fuera de un Redirect el cursor es NULL y las escrituras se tiran, igual que
 * escribir al FIFO sin haberlo redirigido no toca la memoria de nadie. */
void GX_RedirectWriteGatherPipe(void *ptr);
void GX_RestoreWriteGatherPipe(void);

#ifdef __cplusplus
#include <string.h>   /* memcpy, para los operator= de abajo */

/* El cursor lo define gx_d3d9.cpp y lo mueven `GX_RedirectWriteGatherPipe` /
 * `GX_RestoreWriteGatherPipe`.  Esta expuesto para que los `operator=` sean
 * EN LINEA: `_gDPWriteRDRAM` mete por aqui decenas de miles de palabras por
 * frame en Majora, y ya hemos pagado una vez el precio de traducir una
 * llamada GX gratis por una cara (ver GX_InvalidateTexAll, 432 ms/s). */
extern unsigned char *gxd3d_wg_cursor;

/* `memcpy` y no una asignacion directa: por aqui pasan BYTES CRUDOS (F64
 * lleva 8 bytes de RDRAM disfrazados de double) y el destino no tiene por
 * que estar alineado al tamano del miembro. */
#define GXD3D_WG_DEF(nombre, tipo, miembro)                      \
    struct nombre {                                              \
        void operator=(tipo v) {                                 \
            if (!gxd3d_wg_cursor) return;                        \
            memcpy(gxd3d_wg_cursor, &v, sizeof(tipo));           \
            gxd3d_wg_cursor += sizeof(tipo);                     \
        }                                                        \
    } miembro

/* ★ FALTAN `S8` y `S16` A PROPOSITO, aunque la union de libogc los tiene.
 * En este arbol los dos son MACROS -- `source/memory/memory.h:247`,
 * `mupen64plus-rsp-hle/hle.h:34`, `Rice_GX_Xenos/typedefs.h:32` --, asi que
 * declararlos aqui seria poner una mina: basta con que alguien incluya uno de
 * esos antes que este fichero para que el miembro se convierta en `} 3;` y el
 * error salga en gccore.h, sin relacion visible con la causa.
 * Si algun dia upstream los usa, el error sera "no es miembro de GXWGPipe" y
 * apuntara a esta nota, que es infinitamente mejor. */
struct GXWGPipe
{
    GXD3D_WG_DEF(PU8,  u8,  U8);
    GXD3D_WG_DEF(PU16, u16, U16);
    GXD3D_WG_DEF(PS32, s32, S32);
    GXD3D_WG_DEF(PU32, u32, U32);
    GXD3D_WG_DEF(PS64, s64, S64);
    GXD3D_WG_DEF(PU64, u64, U64);
    GXD3D_WG_DEF(PF32, f32, F32);
    GXD3D_WG_DEF(PF64, f64, F64);
};

extern GXWGPipe *wgPipe;
#endif /* __cplusplus */

/* -- utilidades de matrices de libogc (gu*) --------------------------------- */
void guMtxIdentity(Mtx mt);
/* ★★ [Fase 6] COPIA 3x4, NO 4x4, Y ESO NO ES UN DESCUIDO: es lo que hace la
 * de libogc, y el shim tiene que reproducirla.
 *
 * Rice la llama DOS veces con matrices `Mtx44` (`OGLRender.cpp:1661` y
 * `:1674`, sobre gGX.GXcombW / GXprojIdent / GXprojTemp, declaradas Mtx44 en
 * RenderBase.h:248-251).  Compila sin un aviso porque `f32[3][4]` y
 * `f32[4][4]` decaen los dos a `f32(*)[4]`... y copia SOLO 12 de los 16
 * floats: la ULTIMA FILA de la matriz destino se queda con lo que tuviera.
 *
 * ★ ES UN FALLO DE Wii64, NO NUESTRO, Y NO SE ARREGLA AQUI.  Copiar 16 seria
 * "mejorar" la referencia y quedarnos sin saber si un defecto visual viene
 * de eso o de otra cosa.  Si algun dia la proyeccion de Rice sale rara en el
 * offset de poligono (que es justo para lo que se usa), ESTE comentario es
 * la pista; el arreglo, si se hace, se mide y se anota como divergencia. */
void guMtxCopy(Mtx src, Mtx dst);
void guMtx44Identity(Mtx44 mt);
void guMtx44Inverse(Mtx44 src, Mtx44 inv);
void guOrtho(Mtx44 mt, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
f32  guVecDotProduct(guVector *a, guVector *b);

/* ★ DOS argumentos, no uno.  El plugin la llama siempre con el MISMO puntero
 * repetido -- `guVecNormalize((guVector*)&v, (guVector*)&v)` en gSP.cpp:215,
 * :608, :675 y F3DFLX2.cpp:56 -- o sea normalizar en sitio.  La firma de un
 * argumento (que es la del libogc actual) da
 * `error C2660: function does not take 2 arguments`. */
void guVecNormalize(guVector *src, guVector *dst);

/* -- API de video de libogc (en libogc: <ogc/video.h>) --------------------- */
/* VI.cpp:599-606 las usa para presentar el XFB y engancharse al retrazo.  En
 * este core nada de eso es del plugin: el frame se entrega por textura al
 * frontend (env 30003) y el ritmo lo lleva Salvia.  Son no-ops. */
void VIDEO_SetPreRetraceCallback(void (*cb)(u32 retraceCnt));
void VIDEO_SetNextFramebuffer(void *fb);
void VIDEO_Flush(void);
void VIDEO_WaitVSync(void);

/* -- crc32 --------------------------------------------------------------- */
/* GBI.cpp la usa para identificar el microcodigo (GBI.cpp:425), y el VALOR
 * importa: se compara contra la tabla de microcodigos conocidos
 * (`uc_crc == specialMicrocodes[i].crc`, GBI.cpp:433).  Con otro polinomio no
 * casaria ninguno y el plugin caeria al reconocimiento por texto -- sin error
 * visible, solo juegos concretos dibujando mal.
 *
 * ★ NO la implementamos: la trae **zlib.lib**, que ya esta en la linea de
 * enlace del frontend (`COMMON_LIBS_XBOX_RELEASE`).  Una implementacion
 * propia daba `LNK2005: crc32 already defined in zlib.lib(crc32.obj)`, y
 * ademas la de zlib es la canonica por definicion.  Aqui solo se declara,
 * porque el arbol copiado ya no incluye <zlib.h> (ver DIVERGENCIAS.txt 9).
 *
 * Va DENTRO del `extern "C"` para casar con zlib, que es C. */
unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned int len);

#ifdef __cplusplus
}
#endif

/* ★ La sonda TEMPORAL de perfilado dentro de glN64 (xenon/gln64_prof.h).
 *
 * Se arrastra desde aqui a proposito: TODAS las unidades de `glN64_GX/`
 * incluyen `<gccore.h>`, asi que las lineas que hay que anadir al arbol
 * copiado se quedan en UNA por funcion instrumentada, sin ningun `#include`.
 * Con `GLN64_PROF` a 0 esas lineas son macro vacia y el binario vuelve a
 * ser identico. */
#include "../gln64_prof.h"

/* ★ Y el mismo esquema para Rice_GX (xenon/rice_prof.h), por la misma via y
 * por la misma razon: las unidades de `Rice_GX/` tambien incluyen
 * `<gccore.h>`, asi que las lineas que se anaden a ese arbol copiado se
 * quedan en UNA por funcion instrumentada, sin ningun `#include`.
 *
 * Son dos ficheros y no uno porque los cubos son distintos y porque los
 * interruptores tienen que poder ir por separado: el de glN64 cuelga de
 * `XBOX64_SONDAS` (hoy a 0) y el de Rice nace encendido. */
#include "../rice_prof.h"

/* ------------------------------------------------------------------------ */
/* ★ LO QUE SE INVESTIGO AQUI Y SE QUITO (siluetas negras de Paper Mario)    */
/* ------------------------------------------------------------------------ */
/* En esta zona vivieron dos cambios de comportamiento que se probaron contra
 * las siluetas negras de los personajes de Paper Mario y **ninguno era**.
 * Se han quitado enteros; queda la nota para no volver a pagarlos:
 *
 *  1. Exigir que la anchura de la imagen de textura casara con la del
 *     framebuffer antes de desviar la carga (la regla de GLideN64
 *     `src/gDP.cpp:421`, que en Rice SI arreglo las setas de Conker).
 *     MEDIDO: `pasa=490 filtra=478035` -- no filtraba casos raros, apagaba
 *     el desvio entero.  Y aun apagada, los personajes seguian negros.
 *
 *  2. Aplicar `needReplaceTex1ByTex0()` tambien al ACTIVAR la textura, que
 *     es donde la tienen las dos copias de GLideN64 (`Textures.cpp:2079`) y
 *     donde glN64_GX no la tiene -- solo en los dos caminos de texrect.
 *     Aplicado: sin ningun efecto, o sea que la condicion da falso para
 *     esos draws.
 *
 * Lo que SI quedo establecido y no hay que volver a medir esta en
 * [[project_xenon64_papermario_siluetas]]. */

#endif /* _XENON_GXSHIM_GCCORE_H_ */
