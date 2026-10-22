/*
 * xenon/d3dq.cpp  --  la cola de ordenes de Direct3D (el "grabador").
 *
 * ★★★ POR QUE EXISTE: ETAPA 2 DEL RENDERIZADO CON HILO AL ESTILO DE MUPEN.
 *
 * El modelo que se esta construyendo es el de mupen64plus-libretro-nx con
 * `EnableThreadedRenderer`: el emulador entero en su propio hilo, cada
 * llamada a la API grafica convertida en una ORDEN, y `retro_run` -- que
 * corre en el hilo dueno del device -- ejecutandolas hasta cada swap.
 * GLideN64 lo hace con su `FunctionWrapper` sobre OpenGL
 * (ThreadedOpenGl/opengl_Wrapper.cpp); aqui la API es Direct3D 9 y el
 * interpositor es la clase `D3DGrabador` de d3d9_grabador.h.
 *
 * Este fichero es la IMPLEMENTACION de esa cola: el anillo, el pool de
 * datos, codificar (`d3dq_mete`, `d3dq_payload`), ejecutar (`d3dq_ejecuta`)
 * y vaciar (`d3dq_drena`).  Portado ENTERO -- los caminos de los valores 2
 * y 3 incluidos, aunque hoy no se compilen -- de
 * `Xenon64/xenon/d3d9_backend.cpp:520-1060`, que es un shim de DirectX y
 * por tanto entra en lo que se permite coger de aquel arbol.  Alli estaba
 * validado en hardware en los tres valores (ver la nota "VALOR 3 NO PAGA"
 * de d3d9_grabador.h).
 *
 * ★ EL UNICO CAMBIO DE FONDO respecto al origen: alli el device de verdad
 * se leia de `s_grabador.real`, una estatica del MISMO fichero.  Aqui
 * `s_grabador` vive en xbox/gfx_host.cpp, que es quien adopta el device del
 * frontend, asi que se pasa con `d3dq_set_real()`.
 *
 * ETAPA 2 = VALOR 1, LA PASARELA: cada orden se codifica y se ejecuta EN EL
 * ACTO, en el mismo hilo y en el mismo orden.  No cambia nada del reparto
 * de la carga.  Lo unico que pone a prueba es que los 19 pares de
 * codificar/ejecutar que usa el shim traducen bien: la imagen tiene que
 * salir IDENTICA a la de la etapa 1.
 *
 * C++03 (VS2010): d3d9_grabador.h declara una clase.
 */

#include <xtl.h>
#include <ppcintrinsics.h>      /* __lwsync, __mftb: modelo de memoria debil */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>             /* malloc: el anillo y el pool (d3dq_init) */

#include "d3d9_grabador.h"
#include "video_handoff.h"      /* xenon64_video_resolve: la orden Entrega */
#include "../source/xenon64_diag.h"   /* XBOX64_TRAZAS */

/* ====================================================================== */
/* El device de verdad                                                    */
/* ====================================================================== */

#if D3D_GRABADOR
/* ★ Lo fija xbox/gfx_host.cpp al adoptar el device del frontend y lo borra
 * al soltarlo.  Sin el, `d3dq_mete` y `d3dq_drena` no ejecutan nada: es el
 * caso del apagado, cuando el device ya se devolvio. */
static IDirect3DDevice9 *s_real = NULL;

void d3dq_set_real(IDirect3DDevice9 *dev)
{
    s_real = dev;
}
#endif

/* Los contadores se definen SIEMPRE, tambien con el anillo apagado, para que
 * la linea [D3DQ] no dependa del valor de la palanca: con el anillo apagado
 * salen todos a cero, que es ademas la lectura correcta.
 *
 * >>> `extern "C"` NO ES DECORATIVO: la declaracion de d3d9_grabador.h los
 * pone con enlace C, y la linkage la manda la primera declaracion que ve el
 * compilador.  Ver la nota alli. */
extern "C" {
volatile unsigned int d3dq_dbg_ordenes  = 0;
volatile unsigned int d3dq_dbg_drenajes = 0;
volatile unsigned int d3dq_dbg_llenos   = 0;
volatile unsigned int d3dq_dbg_poolmax  = 0;
volatile unsigned int d3dq_dbg_descon   = 0;
volatile unsigned int d3dq_dbg_gordas   = 0;
volatile unsigned int d3dq_dbg_op[40];
/* Espera del PRODUCTOR al consumidor (valor 3).  Ticks del Time Base. */
volatile unsigned int d3dq_dbg_espera_tb = 0;
volatile unsigned int d3dq_dbg_espera_n  = 0;
/* [xbox64] Etapa 3.  Ver la declaracion en d3d9_grabador.h. */
volatile unsigned int d3dq_dbg_binds   = 0;
volatile unsigned int d3dq_dbg_rebinds = 0;
volatile unsigned int d3dq_dbg_bytes   = 0;
volatile unsigned int d3dq_dbg_maxcola = 0;
/* [xbox64] Etapa 4. */
volatile unsigned int d3dq_dbg_sinc_n     = 0;
volatile unsigned int d3dq_dbg_sinc_tb    = 0;
volatile unsigned int d3dq_dbg_poolllenos = 0;
volatile unsigned int d3dq_dbg_perdidos   = 0;
volatile unsigned int d3dq_dbg_siestas    = 0;
volatile unsigned int d3dq_dbg_ligsuelta  = 0;
volatile unsigned int d3dq_dbg_alm_toma   = 0;
volatile unsigned int d3dq_dbg_alm_falla  = 0;
volatile unsigned int d3dq_dbg_alm_crea   = 0;
}

#if D3D_GRABADOR
/* ====================================================================== *
 * ★★★ LA SOMBRA DE ESTADO DEL CONSUMIDOR (el `glsm BIND` de mupen).
 *
 * Todo lo que `d3dq_ejecuta` pone en el device se apunta aqui, y
 * `d3dq_bind()` lo reaplica entero al empezar cada pasada.  Solo la toca el
 * consumidor -- el que ejecuta --, asi que no necesita ninguna proteccion
 * entre hilos ni en la etapa 4.
 *
 * INDICES.  En el XDK los render states y los sampler states van de 4 en 4
 * (`D3DRS_SCISSORTESTENABLE = 200`, `D3DRS_VIEWPORTENABLE = 304`,
 * `D3DRS_MAX = 404`; `D3DSAMP_ADDRESSU = 0 ... D3DSAMP_MAX = 80`, en
 * d3d9types.h), porque la tabla interna del device esta indexada por
 * `State/4` (ver la nota del espejo de la clase).  Se indexa igual.
 *
 * TAMANOS, sacados del codigo y no supuestos: el shim sube las constantes
 * del vertex shader en c0..c8 (`gxd3d_upload_matrices`) y las del pixel
 * shader en c0..c12 (`TEVC_TOTAL` 12 + `TEVC_MARCA`, gx_d3d9_tev.cpp:292-300),
 * y solo usa los samplers 0..3.
 * ====================================================================== */
#define SOMBRA_RS     (404 / 4)
#define SOMBRA_SAMP   4
#define SOMBRA_STIPOS (80 / 4)
#define SOMBRA_VSC    16
#define SOMBRA_PSC    21     /* c0..c20: c16/c17 = GXD3D_LOD_CONST, c18/c19 = GXD3D_ATLAS_CONST, c20 = GXD3D_NIEBLAZ_CONST (gx_d3d9_int.h) */

static DWORD          s_sRs[SOMBRA_RS];
static unsigned char  s_sRsOk[SOMBRA_RS];
static unsigned short s_sRsLista[SOMBRA_RS];   /* en orden de primera escritura */
static int            s_sRsN = 0;

static D3DBaseTexture *s_sTex[SOMBRA_SAMP];
static unsigned char   s_sTexOk[SOMBRA_SAMP];
static DWORD           s_sSamp[SOMBRA_SAMP][SOMBRA_STIPOS];
static unsigned char   s_sSampOk[SOMBRA_SAMP][SOMBRA_STIPOS];

static D3DVertexShader      *s_sVS   = NULL;  static int s_sVSOk   = 0;
static D3DPixelShader       *s_sPS   = NULL;  static int s_sPSOk   = 0;
static D3DVertexDeclaration *s_sDecl = NULL;  static int s_sDeclOk = 0;

static float         s_sVSC[SOMBRA_VSC][4];
static unsigned char s_sVSCOk[SOMBRA_VSC];
static float         s_sPSC[SOMBRA_PSC][4];
static unsigned char s_sPSCOk[SOMBRA_PSC];

static D3DVIEWPORT9 s_sVp;  static int s_sVpOk = 0;
static RECT         s_sSc;  static int s_sScOk = 0;

void d3dq_sombra_reset(void)
{
    memset(s_sRsOk,   0, sizeof(s_sRsOk));
    s_sRsN = 0;
    memset(s_sTexOk,  0, sizeof(s_sTexOk));
    memset(s_sSampOk, 0, sizeof(s_sSampOk));
    s_sVS = NULL;   s_sVSOk = 0;
    s_sPS = NULL;   s_sPSOk = 0;
    s_sDecl = NULL; s_sDeclOk = 0;
    memset(s_sVSCOk,  0, sizeof(s_sVSCOk));
    memset(s_sPSCOk,  0, sizeof(s_sPSCOk));
    s_sVpOk = 0;
    s_sScOk = 0;
}

static void sombra_rs(DWORD st, DWORD v)
{
    const unsigned int i = (unsigned int)st >> 2;
    if ((unsigned int)st >= 404u || ((unsigned int)st & 3u)) return;
    if (!s_sRsOk[i]) { s_sRsOk[i] = 1; s_sRsLista[s_sRsN++] = (unsigned short)i; }
    s_sRs[i] = v;
}

static void sombra_samp(DWORD s, DWORD tipo, DWORD v)
{
    const unsigned int t = (unsigned int)tipo >> 2;
    if (s >= SOMBRA_SAMP || (unsigned int)tipo >= 80u || ((unsigned int)tipo & 3u)) return;
    s_sSamp[s][t] = v;  s_sSampOk[s][t] = 1;
}

static void sombra_const(float (*dst)[4], unsigned char *ok, unsigned int max,
                         unsigned int ini, const float *src, unsigned int n)
{
    unsigned int k;
    if (!src) return;
    for (k = 0; k < n && ini + k < max; k++)
    {
        dst[ini + k][0] = src[k * 4 + 0];  dst[ini + k][1] = src[k * 4 + 1];
        dst[ini + k][2] = src[k * 4 + 2];  dst[ini + k][3] = src[k * 4 + 3];
        ok[ini + k] = 1;
    }
}

/* ★★★ [log 33] TEXTURAS SOLTADAS ESTANDO LIGADAS: SE APLAZA EL RELEASE.
 *
 * `ligsuelta` era el contador del defecto, y la nota que habia aqui decia
 * "si sale distinto de 0 en partida, esa es la siguiente pieza".  Salio:
 * `ligsuelta=47` en el primer segundo de Majora's Mask.  La cadena:
 *
 *   1. El productor suelta una textura que sigue ligada en un sampler (el
 *      LRU del registro, `gxd3d_tex_olvida`, la cache apagada...).
 *   2. Aqui se liberaba en el acto: la cabecera de D3D vuelve al monton del
 *      proceso, el MISMO del que sale el `malloc` de la CRT.
 *   3. El siguiente `SetTexture` de ese sampler TOCA LA TEXTURA SALIENTE --
 *      medido en el log 20: `D3DDevice_SetTexture(1, NULL)` escribiendo en
 *      la textura ya liberada --, y el `d3dq_bind` del `retro_run`
 *      siguiente tambien.
 *   4. Con el emulador corriendo A LA VEZ, entre 2 y 3 el dynarec puede
 *      haberse quedado ese bloque (`calloc` de un `PowerPC_block`, un
 *      `PowerPC_func`, un hueco): D3D le escribe encima.  Es el cuelgue del
 *      dynarec al arrancar Majora y Conker.  Sin hilo, y en los dos modos en
 *      serie (logs 32 y 33), el emulador esta quieto mientras se ejecutan
 *      las ordenes, nadie recicla el bloque entre 2 y 3, y no cae.
 *
 * ARREGLO: la textura no se libera mientras siga en algun sampler.  Se
 * aplaza hasta que el consumidor ejecute el `SetTexture` que la saca del
 * ultimo (o hasta `d3dq_desliga_texturas`, al descargar).  La sombra la
 * CONSERVA, asi que el device sigue teniendo justo lo que el espejo del
 * productor (`s_samp` en gx_d3d9.cpp) cree que tiene: ni dibujos sin
 * textura, ni una direccion reciclada que el espejo confunda con la vieja,
 * porque mientras esta aplazada la direccion no se puede reciclar.
 *
 * A lo sumo hay SOMBRA_SAMP aplazadas: cada una ocupa al menos un sampler, y
 * religar ese sampler la suelta. */
static D3DBaseTexture *s_aplTex[SOMBRA_SAMP];
static unsigned int    s_aplMasc[SOMBRA_SAMP];   /* samplers donde sigue */

static void aplazadas_saca_de(unsigned int s)
{
    int k;
    for (k = 0; k < SOMBRA_SAMP; k++)
        if (s_aplTex[k] && (s_aplMasc[k] & (1u << s)))
        {
            s_aplMasc[k] &= ~(1u << s);
            if (!s_aplMasc[k])
            {
                ((D3DResource *)s_aplTex[k])->Release();
                s_aplTex[k] = NULL;
            }
        }
}

static void aplazadas_suelta_todas(void)
{
    int k;
    for (k = 0; k < SOMBRA_SAMP; k++)
        if (s_aplTex[k])
        {
            ((D3DResource *)s_aplTex[k])->Release();
            s_aplTex[k]  = NULL;
            s_aplMasc[k] = 0;
        }
}

/* Un recurso que se suelta.  Devuelve la mascara de samplers en los que la
 * sombra lo tiene ligado: si no es 0, quien llama NO debe liberarlo.  Los
 * shaders y la declaracion se olvidan como antes. */
static unsigned int sombra_olvida(void *p)
{
    unsigned int masc = 0;
    int i;
    if (!p) return 0;
    for (i = 0; i < SOMBRA_SAMP; i++)
        if (s_sTexOk[i] && (void *)s_sTex[i] == p)
        {
            masc |= 1u << i;
#if XBOX64_TRAZAS
            ++d3dq_dbg_ligsuelta;
#endif
        }
    if (s_sVSOk   && (void *)s_sVS   == p) { s_sVS   = NULL; s_sVSOk   = 0; }
    if (s_sPSOk   && (void *)s_sPS   == p) { s_sPS   = NULL; s_sPSOk   = 0; }
    if (s_sDeclOk && (void *)s_sDecl == p) { s_sDecl = NULL; s_sDeclOk = 0; }
    return masc;
}

/* La orden QOP_SoltarRecurso. */
static void suelta_recurso(void *p)
{
    unsigned int masc;
    int k;

    if (!p) return;
    masc = sombra_olvida(p);
    if (masc)
    {
        for (k = 0; k < SOMBRA_SAMP; k++)
            if (s_aplTex[k] == (D3DBaseTexture *)p)
                return;                 /* ya aplazada: no soltarla dos veces */
        for (k = 0; k < SOMBRA_SAMP; k++)
            if (!s_aplTex[k])
            {
                s_aplTex[k]  = (D3DBaseTexture *)p;
                s_aplMasc[k] = masc;
                return;
            }
        /* Sin hueco no puede pasar (ver arriba); si pasara, lo de antes. */
    }
    ((D3DResource *)p)->Release();
}

/* ★ EL BIND.  Reaplica la sombra entera.  Solo se llama desde el consumidor
 * -- `retro_run` --, con el render target del core ya ligado.
 *
 * `D3DRS_VIEWPORTENABLE` va dentro de los render states, y es de los que no
 * pueden faltar: el frontend lo pone a FALSE para su quad pretransformado, y
 * con el asi la escena entera cae en un punto (nota de gxd3d_frame_begin). */
void d3dq_bind(void)
{
    IDirect3DDevice9 *dev = s_real;
    unsigned int n = 0;
    int i, t;

    if (!dev) return;

    for (i = 0; i < s_sRsN; i++)
    {
        const unsigned int k = s_sRsLista[i];
        dev->SetRenderState((D3DRENDERSTATETYPE)(k << 2), s_sRs[k]);
        n++;
    }
    if (s_sVSOk)   { dev->SetVertexShader(s_sVS);        n++; }
    if (s_sPSOk)   { dev->SetPixelShader(s_sPS);         n++; }
    if (s_sDeclOk) { dev->SetVertexDeclaration(s_sDecl); n++; }
    for (i = 0; i < SOMBRA_SAMP; i++)
    {
        if (s_sTexOk[i]) { dev->SetTexture((DWORD)i, s_sTex[i]); n++; }
        for (t = 0; t < SOMBRA_STIPOS; t++)
            if (s_sSampOk[i][t])
            {
                dev->SetSamplerState((DWORD)i, (D3DSAMPLERSTATETYPE)(t << 2),
                                     s_sSamp[i][t]);
                n++;
            }
    }
    for (i = 0; i < SOMBRA_VSC; i++)
        if (s_sVSCOk[i]) { dev->SetVertexShaderConstantF((UINT)i, s_sVSC[i], 1); n++; }
    for (i = 0; i < SOMBRA_PSC; i++)
        if (s_sPSCOk[i]) { dev->SetPixelShaderConstantF((UINT)i, s_sPSC[i], 1); n++; }
    if (s_sVpOk) { dev->SetViewport(&s_sVp);    n++; }
    if (s_sScOk) { dev->SetScissorRect(&s_sSc); n++; }

#if XBOX64_TRAZAS   /* `n` solo lo lee el log; sin trazas es local muerta */
    d3dq_dbg_binds++;
    d3dq_dbg_rebinds += n;
#else
    (void)n;
#endif
}

/* ★★ DESLIGAR LAS TEXTURAS DEL DEVICE ANTES DE QUE SE SUELTEN.  La llama
 * `retro_unload_game` con el emulador ya parado y ANTES de descargar la ROM.
 *
 * El fallo que arregla (log 20, al pasar de Rush a OoT): `D3DDevice_SetTexture
 * (Sampler=1, NULL)` escribiendo en 0x4032D788, desde el vaciado del final de
 * `gxd3d_shutdown`.  La cadena:
 *
 *   1. `hilo_emu_parar` bombea la cola, y el bombeo es una pasada normal: su
 *      `d3dq_bind()` vuelve a ligar en los samplers las texturas del core.
 *   2. `xbox64_unload_rom` -> `romClosed_gfx` suelta las texturas del plugin
 *      (`d3dq_soltar`, en orden), y el primer vaciado de `gxd3d_shutdown` las
 *      libera.  Una de ellas sigue LIGADA en el sampler 1.
 *   3. El `SetTexture(i, NULL)` de `gxd3d_shutdown` -- que existe justo para
 *      esto, "retirarlas del device antes" -- llega DESPUES de liberarlas, y
 *      el device toca la textura saliente, ya liberada.
 *
 * Sin el hilo no pasaba (log 19, mismo cambio de juego) porque entre el
 * ultimo `retro_run` y la descarga el menu del frontend ya habia pisado esos
 * samplers; el bombeo de la etapa 4 los vuelve a poner justo antes.
 *
 * Tambien se vacia la sombra: el siguiente bind no debe volver a ligarlas.
 *
 * Devuelve cuantos samplers tenian una textura del core segun la sombra: es
 * la condicion del fallo.  Si sale > 0 y la descarga ya no cae, la cadena de
 * arriba queda confirmada. */
unsigned int d3dq_desliga_texturas(void)
{
    IDirect3DDevice9 *dev = s_real;
    unsigned int n = 0;
    int i;

    if (!dev) return 0;
    for (i = 0; i < SOMBRA_SAMP; i++)
    {
        if (s_sTexOk[i] && s_sTex[i]) n++;
        dev->SetTexture((DWORD)i, NULL);
        s_sTex[i]   = NULL;
        s_sTexOk[i] = 1;
    }
    /* [log 33] Ya fuera de todos los samplers: las aplazadas se liberan. */
    aplazadas_suelta_todas();
    return n;
}
#endif /* D3D_GRABADOR */

#if D3D_GRABADOR
/* ==========================================================================
 * EL ANILLO DE ORDENES DE D3D.  Ver d3d9_grabador.h para el porque.
 *
 * >>> CON EL VALOR 1 NO HAY ANILLO: la orden se EJECUTA en el acto, en el
 * mismo hilo y en orden.  Es la pasarela.  El camino es el mismo -- se
 * rellena una `D3DQCmd` y se despacha por `d3dq_ejecuta` -- para que los
 * tres valores compartan codigo y no haya dos verdades sobre como se
 * traduce cada orden.
 * ========================================================================== */

#if D3D_GRABADOR >= 2
/* [xbox64] Reservados por `d3dq_init()`: ver D3DQ_POOL_MAX en la cabecera.
 *
 * Los CUATRO cursores van libre-corriendo y se enmascaran al indexar: asi
 * "lleno" y "vacio" se distinguen sin desperdiciar una ranura, y la resta
 * aguanta el desbordamiento sin casos especiales.  Cada uno tiene UN solo
 * escritor, que es lo que hace correcta la cola entre dos hilos con solo
 * `__lwsync`:
 *
 *   g_d3dqCabeza     productor   ordenes metidas
 *   g_d3dqCola       consumidor  ordenes ejecutadas
 *   g_d3dqPoolCab    productor   bytes del pool repartidos
 *   g_d3dqPoolLibre  consumidor  bytes del pool ya liberados (el `poolFin`
 *                                de la ultima orden ejecutada) */
D3DQCmd              *g_d3dq          = NULL;
unsigned char        *g_d3dqPool      = NULL;
unsigned int          g_d3dqPoolTam   = 0;
volatile unsigned int g_d3dqCabeza    = 0;
volatile unsigned int g_d3dqCola      = 0;
unsigned int          g_d3dqPoolCab   = 0;
volatile unsigned int g_d3dqPoolLibre = 0;
#endif

#if D3D_GRABADOR >= 3
/* ★ EL INTERRUPTOR DEL MODO 3 EN TIEMPO DE EJECUCION.  1 mientras el
 * emulador graba en su propio hilo (xbox/hilo_emu.cpp); 0 en cualquier otro
 * momento -- la carga y el cierre, que corren en el hilo del frontend, y el
 * modo sin hilo.  Con 0 no hay otro hilo que ejecute, asi que el productor
 * vacia el mismo en todos los sitios donde con 1 esperaria: es exactamente
 * el valor 2. */
volatile unsigned int g_d3dqVivo = 0;

/* "Hay trabajo": el productor lo senala si el consumidor esta DORMIDO
 * (`s_consDuerme`) y al llenar el buzon.  Solo entonces: a decenas de miles
 * de ordenes por segundo, un SetEvent en cada una costaria mas que el
 * trabajo.
 * "Buzon hecho": el consumidor lo senala al terminar una orden sincrona. */
static HANDLE s_evTrabajo = NULL;
static HANDLE s_evBuzon   = NULL;

/* El BUZON de las ordenes sincronas.  Una sola plaza basta: hay un solo
 * productor y se queda esperando en cada sincrona. */
static D3DQCmd               s_buzon;
static volatile unsigned int s_buzonLleno = 0;

/* ★ 1 mientras el consumidor esta (o va a estar) dentro del Wait.  Es lo que
 * decide si el productor tiene que despertarle.
 *
 * La primera version avisaba solo al pasar el anillo de VACIO a NO VACIO, y
 * eso tiene una carrera: el productor lee la cola (1 pendiente, "no estaba
 * vacio"), el consumidor ejecuta esa ultima orden, ve el anillo vacio y se
 * duerme, y el productor publica la suya SIN avisar.  `retro_run` se queda
 * dormido el plazo entero con trabajo delante -- y eso sale en `[UTIL run]`
 * como un frame caro que no lo es.  Con la bandera el consumidor ANUNCIA que
 * se duerme y vuelve a mirar el anillo antes de hacerlo. */
static volatile unsigned int s_consDuerme = 0;

static void d3dq_despierta(void)
{
    if (s_evTrabajo) SetEvent(s_evTrabajo);
}

void d3dq_modo_hilo(int si)
{
    __lwsync();
    g_d3dqVivo = si ? 1u : 0u;
    __lwsync();
}

/* ★ `__sync` Y NO `__lwsync`, y es el unico sitio del anillo donde hace
 * falta: el PPC reordena una ESCRITURA seguida de una LECTURA, y aqui se
 * escribe la bandera y luego se lee la cabeza.  Con `__lwsync` la lectura
 * podria adelantarse a la escritura y el productor no veria la bandera.
 * Cuesta una barrera completa, pero una vez por siesta, no por orden.
 *
 * El productor mira la bandera SIN barrera completa (ver `d3dq_mete`), asi
 * que queda una ventana de nanosegundos en la que un aviso puede perderse.
 * Por eso el plazo, y por eso `perdidos`: CONTADOR DEL DEFECTO, cuenta las
 * siestas que acabaron por plazo con trabajo esperando.  Tiene que salir ~0;
 * si sale alto, esta carrera cuesta frames y hay que cerrarla del todo.
 *
 * ★★ Y ANTES DE DORMIR, GIRAR.  Mientras el emulador recorre una display
 * list las ordenes llegan cada microsegundo o dos, y el consumidor es mas
 * rapido que el: vacia el anillo en cuanto entra algo.  Dormirse cada vez
 * que lo ve vacio seria un SetEvent en el productor y un despertar del
 * nucleo por ORDEN -- miles por frame, del orden de milisegundos tirados en
 * los dos hilos.  La cola de mupen hace lo mismo: el `LightweightSemaphore`
 * de moodycamel gira antes de ir al sistema (GLideN64,
 * ThreadedOpenGl/BlockingReaderWriterQueue).  Se gira D3DQ_GIRO_TB ticks
 * (50 us) y solo se duerme si en ese rato no llega nada: entonces el
 * emulador esta en CPU, no dibujando, y el SetEvent es uno por tramo.
 * `siestas` cuenta las que llegan a dormir: si sale en los miles por
 * segundo, el giro se queda corto. */
#define D3DQ_GIRO_TB  2500u        /* 50 us a 50 MHz */

void d3dq_espera_trabajo(unsigned int ms)
{
    if (!s_evTrabajo)
        return;

    {
        const unsigned long long t0 = __mftb();
        while ((unsigned int)(__mftb() - t0) < D3DQ_GIRO_TB)
        {
            if (g_d3dqCabeza != g_d3dqCola || s_buzonLleno)
                return;
            YieldProcessor();
        }
    }

    s_consDuerme = 1;
    __sync();
    if (g_d3dqCabeza == g_d3dqCola && !s_buzonLleno)
    {
#if XBOX64_TRAZAS
        ++d3dq_dbg_siestas;
        if (WaitForSingleObject(s_evTrabajo, ms) == WAIT_TIMEOUT)
        {
            __lwsync();
            if (g_d3dqCabeza != g_d3dqCola || s_buzonLleno)
                ++d3dq_dbg_perdidos;
        }
#else
        WaitForSingleObject(s_evTrabajo, ms);
#endif
    }
    s_consDuerme = 0;
}
#endif /* D3D_GRABADOR >= 3 */

/* ★ La reserva, con escalera de tamanos.  IDEMPOTENTE: se llama en cada
 * carga de ROM (xbox64_gfx_init) y solo reserva la primera vez; las demas
 * solo reinician los cursores, que en ese momento es seguro porque la cola
 * esta vacia.  Reservar y soltar 32 MB en cada carga fragmentaria la
 * memoria para nada. */
int d3dq_init(void)
{
#if D3D_GRABADOR >= 2
    if (!g_d3dq)
    {
        g_d3dq = (D3DQCmd *)malloc(sizeof(D3DQCmd) * D3DQ_RANURAS);
        if (!g_d3dq)
            return 0;
    }
    if (!g_d3dqPool)
    {
        unsigned int tam = D3DQ_POOL_MAX;
        while (tam >= (4u << 20))
        {
            g_d3dqPool = (unsigned char *)malloc(tam);
            if (g_d3dqPool) { g_d3dqPoolTam = tam; break; }
            tam >>= 1;
        }
        if (!g_d3dqPool)
            return 0;
#if XBOX64_TRAZAS
        {
            char l[96];
            _snprintf(l, sizeof(l), "[D3DQ] anillo=%u ranuras  pool=%u MB",
                      (unsigned)D3DQ_RANURAS, g_d3dqPoolTam >> 20);
            l[sizeof(l) - 1] = 0;
            OutputDebugStringA(l);
        }
#endif
    }
    g_d3dqCabeza = g_d3dqCola = 0;
    g_d3dqPoolCab = 0;
    g_d3dqPoolLibre = 0;
#endif
#if D3D_GRABADOR >= 3
    if (!s_evTrabajo) s_evTrabajo = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!s_evBuzon)   s_evBuzon   = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_buzonLleno = 0;
    g_d3dqVivo = 0;
#endif
    return 1;
}

void d3dq_fin(void)
{
#if D3D_GRABADOR >= 3
    g_d3dqVivo = 0;
    if (s_evTrabajo) { CloseHandle(s_evTrabajo); s_evTrabajo = NULL; }
    if (s_evBuzon)   { CloseHandle(s_evBuzon);   s_evBuzon   = NULL; }
#endif
#if D3D_GRABADOR >= 2
    if (g_d3dq)     { free(g_d3dq);     g_d3dq = NULL; }
    if (g_d3dqPool) { free(g_d3dqPool); g_d3dqPool = NULL; g_d3dqPoolTam = 0; }
#endif
}

#define D3DQ_USADAS()  ((unsigned int)(g_d3dqCabeza - g_d3dqCola))

/* ★★ [etapa 4] EL POOL, COMO ANILLO DE VERDAD.
 *
 * El origen reservaba en linea y, al llegar al final, VACIABA LA COLA ENTERA
 * y volvia a 0.  En el modo 3 eso no vale: el consumidor para en cada SWAP,
 * asi que "cola vacia" puede tardar frames en darse, y el productor se
 * quedaria esperando justo ahi -- a 7 MB/s, cada pocos segundos.
 *
 * Ahora es el `RingBufferPool` de GLideN64 (RingBufferPool.cpp:64-193): el
 * consumidor LIBERA en orden lo que ya ejecuto (`g_d3dqPoolLibre`, el
 * `poolFin` de cada orden) y el productor solo espera si el trozo que
 * necesita sigue ocupado.  Un bloque no se parte entre el final y el
 * principio: si no cabe al final, se salta al principio.
 *
 * Alineado a 8 bytes.  Un bloque de mas de la mitad del pool no se acepta:
 * se pasa el puntero original y se cuenta en `gordas` (en el modo 3 eso es
 * un dato que puede morir antes de ejecutarse; tiene que salir 0). */
void *d3dq_payload(const void *src, unsigned int n)
{
#if D3D_GRABADOR < 2
    /* Pasarela: la orden se ejecuta antes de volver, asi que el buffer del
     * llamante sigue vivo.  Copiarlo seria trabajo puro y duro por nada. */
    (void)n;
    return (void *)src;
#else
    unsigned int n8, pos, salto, necesita, usado;
    unsigned char *dst;
    int esperando = 0;       /* [etapa 4] ya se conto y se aviso esta espera */

    if (!src || n == 0)
        return (void *)src;
    if (!g_d3dqPool)
    {
#if XBOX64_TRAZAS
        ++d3dq_dbg_gordas;
#endif
        return (void *)src;
    }

    n8 = (n + 7u) & ~7u;
    if (n8 > g_d3dqPoolTam / 2u)
    {
#if XBOX64_TRAZAS
        ++d3dq_dbg_gordas;
#endif
        return (void *)src;
    }

    pos      = g_d3dqPoolCab & (g_d3dqPoolTam - 1u);
    salto    = (pos + n8 > g_d3dqPoolTam) ? (g_d3dqPoolTam - pos) : 0u;
    necesita = salto + n8;

    for (;;)
    {
        __lwsync();
        usado = g_d3dqPoolCab - g_d3dqPoolLibre;
        if (g_d3dqPoolTam - usado >= necesita)
            break;
#if D3D_GRABADOR >= 3
        if (g_d3dqVivo)
        {
            /* Esperar a que el consumidor libere.  Se le despierta UNA vez
             * -- un SetEvent por vuelta seria una llamada al sistema en un
             * bucle de giro -- y se cede el nucleo.  Se cuenta una vez por
             * espera, no por vuelta.  En regimen no pasa (ver el
             * dimensionado). */
            if (!esperando)
            {
                esperando = 1;
#if XBOX64_TRAZAS
                ++d3dq_dbg_poolllenos;
#endif
                d3dq_despierta();
            }
            YieldProcessor();
            continue;
        }
#endif
        /* Mismo hilo: vaciar libera todo menos los payloads ya dados a la
         * orden que se esta armando, que son pocos bytes. */
        d3dq_drena();
        __lwsync();
        usado = g_d3dqPoolCab - g_d3dqPoolLibre;
        if (g_d3dqPoolTam - usado < necesita)
        {
#if XBOX64_TRAZAS
            ++d3dq_dbg_gordas;
#endif
            return (void *)src;
        }
        break;
    }

    g_d3dqPoolCab += salto;
    dst = &g_d3dqPool[g_d3dqPoolCab & (g_d3dqPoolTam - 1u)];
    memcpy(dst, src, n);
    g_d3dqPoolCab += n8;
#if XBOX64_TRAZAS
    d3dq_dbg_bytes += n;     /* [xbox64] lo que dimensiona el pool */
    usado = g_d3dqPoolCab - g_d3dqPoolLibre;
    if (usado > d3dq_dbg_poolmax)
        d3dq_dbg_poolmax = usado;
#endif
    return dst;
#endif /* D3D_GRABADOR < 2 */
}

/* Definida mas abajo: con el valor 1 se llama desde `d3dq_mete`. */
static void d3dq_ejecuta(const D3DQCmd *c, IDirect3DDevice9 *dev);

void d3dq_mete(const D3DQCmd *c)
{
#if D3D_GRABADOR < 2
    /* PASARELA: sin anillo y sin pool.  Se ejecuta en el acto, en el mismo
     * hilo y en el mismo orden en que el plugin llamo. */
    {
        IDirect3DDevice9 *dev = s_real;
        if (dev) d3dq_ejecuta(c, dev);
    }
#if XBOX64_TRAZAS
    ++d3dq_dbg_ordenes;
    if (c->op < 40u) ++d3dq_dbg_op[c->op];
#endif
#else
    D3DQCmd *r;

#if D3D_GRABADOR >= 3
    /* Anillo lleno: ESPERAR a que el consumidor libere. */
    if (D3DQ_USADAS() >= D3DQ_RANURAS)
    {
#if XBOX64_TRAZAS
        ++d3dq_dbg_llenos;
#endif
        if (!g_d3dqVivo)
        {
            /* Sin otro hilo consumiendo, no hay a quien esperar: el productor
             * vacia el mismo, como el valor 2. */
            d3dq_drena();
        }
        else
        {
            d3dq_despierta();
            while (D3DQ_USADAS() >= D3DQ_RANURAS)
            {
                YieldProcessor();
                __lwsync();
            }
        }
    }
#else
    while (D3DQ_USADAS() >= D3DQ_RANURAS)
    {
        /* En el modo 2 productor y consumidor son el MISMO hilo, asi que
         * esperar seria un cuelgue: se vacia aqui mismo. */
#if XBOX64_TRAZAS
        ++d3dq_dbg_llenos;
#endif
        d3dq_drena();
    }
#endif

    {
        r  = &g_d3dq[g_d3dqCabeza & D3DQ_MASC];
        *r = *c;
        r->poolFin = g_d3dqPoolCab;

        /* La orden tiene que estar COMPLETA en memoria antes de que el
         * consumidor pueda verla. */
        __lwsync();
        g_d3dqCabeza = g_d3dqCabeza + 1u;
#if D3D_GRABADOR >= 3
        /* Despertar al consumidor si se ha dormido.  Ver `s_consDuerme`:
         * una lectura por orden, y el SetEvent solo cuando hace falta. */
        if (s_consDuerme) d3dq_despierta();
#endif
    }
#if XBOX64_TRAZAS
    ++d3dq_dbg_ordenes;
    if (c->op < 40u) ++d3dq_dbg_op[c->op];
#endif
#endif /* D3D_GRABADOR < 2 */
}

/* La traduccion de UNA orden.  Vive aparte para que la compartan los tres
 * valores de la palanca: con el 1 la llama `d3dq_mete` en el acto y con el 2
 * y el 3 la llama `d3dq_drena` desde el anillo.  Dos copias de este switch
 * serian dos verdades sobre como se ejecuta cada orden. */
static void d3dq_ejecuta(const D3DQCmd *c, IDirect3DDevice9 *dev)
{
    switch (c->op)
    {
    case QOP_Present:
        dev->Present(NULL, NULL, NULL, NULL);
        break;

    case QOP_CreateTexture:
        *(HRESULT *)c->p[2] = dev->CreateTexture(
            c->w[0], c->w[1], c->w[2], c->w[3],
            (D3DFORMAT)c->w[4], (D3DPOOL)c->w[5],
            (D3DTexture **)c->p[0], (HANDLE *)c->p[1]);
        break;

    case QOP_CreateVertexBuffer:
        *(HRESULT *)c->p[2] = dev->CreateVertexBuffer(
            c->w[0], c->w[1], c->w[2], (D3DPOOL)c->w[3],
            (D3DVertexBuffer **)c->p[0], (HANDLE *)c->p[1]);
        break;

    case QOP_CreateDepthStencilSurface:
        *(HRESULT *)c->p[2] = dev->CreateDepthStencilSurface(
            c->w[0], c->w[1], (D3DFORMAT)c->w[2],
            (D3DMULTISAMPLE_TYPE)c->w[3], c->w[4], (BOOL)c->w[5],
            (D3DSurface **)c->p[0],
            (CONST D3DSURFACE_PARAMETERS *)c->p[1]);
        break;

    case QOP_CreateVertexShader:
        *(HRESULT *)c->p[2] = dev->CreateVertexShader(
            (CONST DWORD *)c->p[0], (D3DVertexShader **)c->p[1]);
        break;

    case QOP_CreatePixelShader:
        *(HRESULT *)c->p[2] = dev->CreatePixelShader(
            (CONST DWORD *)c->p[0], (D3DPixelShader **)c->p[1]);
        break;

    case QOP_CreateVertexDeclaration:
        *(HRESULT *)c->p[2] = dev->CreateVertexDeclaration(
            (CONST D3DVERTEXELEMENT9 *)c->p[0],
            (D3DVertexDeclaration **)c->p[1]);
        break;

    case QOP_GetRenderTarget:
        *(HRESULT *)c->p[1] = dev->GetRenderTarget(
            c->w[0], (D3DSurface **)c->p[0]);
        break;

    case QOP_GetDepthStencilSurface:
        *(HRESULT *)c->p[1] = dev->GetDepthStencilSurface(
            (D3DSurface **)c->p[0]);
        break;

    case QOP_GetPixelShader:
        dev->GetPixelShader((D3DPixelShader **)c->p[0]);
        break;

    case QOP_SetRenderTarget:
        dev->SetRenderTarget(c->w[0], (D3DSurface *)c->p[0]);
        break;

    case QOP_SetDepthStencilSurface:
        dev->SetDepthStencilSurface((D3DSurface *)c->p[0]);
        break;

    case QOP_Resolve:
    {
        union { unsigned int u; float f; } z; z.u = c->w[3];
        dev->Resolve(c->w[0],
                     (CONST D3DRECT *)c->p[0],
                     (D3DBaseTexture *)c->p[1],
                     (CONST D3DPOINT *)c->p[2],
                     c->w[1], c->w[2],
                     (CONST D3DVECTOR4 *)c->p[3],
                     z.f, c->w[4],
                     (CONST D3DRESOLVE_PARAMETERS *)c->p[4]);
        break;
    }

    case QOP_BeginScene: dev->BeginScene(); break;
    case QOP_EndScene:   dev->EndScene();   break;

    case QOP_Clear:
    {
        union { unsigned int u; float f; } z; z.u = c->w[3];
        dev->Clear(c->w[0], (CONST D3DRECT *)c->p[0], c->w[1],
                   (D3DCOLOR)c->w[2], z.f, c->w[4]);
        break;
    }

    /* ★ Los que dejan ESTADO en el device se apuntan ademas en la sombra
     * del consumidor, para reaplicarlos en el siguiente `d3dq_bind()`.  Ver
     * D3DQ_BIND en d3d9_grabador.h. */
    case QOP_SetRenderState:
        dev->SetRenderState((D3DRENDERSTATETYPE)c->w[0], c->w[1]);
        sombra_rs(c->w[0], c->w[1]);
        break;

    case QOP_SetViewport:
        dev->SetViewport((CONST D3DVIEWPORT9 *)c->p[0]);
        if (c->p[0]) { s_sVp = *(CONST D3DVIEWPORT9 *)c->p[0]; s_sVpOk = 1; }
        break;

    case QOP_SetScissorRect:
        dev->SetScissorRect((CONST RECT *)c->p[0]);
        if (c->p[0]) { s_sSc = *(CONST RECT *)c->p[0]; s_sScOk = 1; }
        break;

    case QOP_SetTexture:
        dev->SetTexture(c->w[0], (D3DBaseTexture *)c->p[0]);
        if (c->w[0] < SOMBRA_SAMP)
        {
            s_sTex[c->w[0]]   = (D3DBaseTexture *)c->p[0];
            s_sTexOk[c->w[0]] = 1;
            /* [log 33] La saliente ya no esta en este sampler: si estaba
             * aplazada y era el ultimo, AHORA se libera. */
            aplazadas_saca_de(c->w[0]);
        }
        break;

    case QOP_SetSamplerState:
        dev->SetSamplerState(c->w[0], (D3DSAMPLERSTATETYPE)c->w[1], c->w[2]);
        sombra_samp(c->w[0], c->w[1], c->w[2]);
        break;

    case QOP_SetVertexShader:
        dev->SetVertexShader((D3DVertexShader *)c->p[0]);
        s_sVS = (D3DVertexShader *)c->p[0];  s_sVSOk = 1;
        break;

    case QOP_SetVertexShaderConstantF:
        dev->SetVertexShaderConstantF(c->w[0], (CONST float *)c->p[0], c->w[1]);
        sombra_const(s_sVSC, s_sVSCOk, SOMBRA_VSC, c->w[0],
                     (CONST float *)c->p[0], c->w[1]);
        break;

    case QOP_SetPixelShader:
        dev->SetPixelShader((D3DPixelShader *)c->p[0]);
        s_sPS = (D3DPixelShader *)c->p[0];  s_sPSOk = 1;
        break;

    case QOP_SetPixelShaderConstantB:
        dev->SetPixelShaderConstantB(c->w[0], (CONST BOOL *)c->p[0], c->w[1]);
        break;

    case QOP_SetPixelShaderConstantF:
        dev->SetPixelShaderConstantF(c->w[0], (CONST float *)c->p[0], c->w[1]);
        sombra_const(s_sPSC, s_sPSCOk, SOMBRA_PSC, c->w[0],
                     (CONST float *)c->p[0], c->w[1]);
        break;

    case QOP_SetVertexDeclaration:
        dev->SetVertexDeclaration((D3DVertexDeclaration *)c->p[0]);
        s_sDecl = (D3DVertexDeclaration *)c->p[0];  s_sDeclOk = 1;
        break;

    case QOP_SetStreamSource:
        dev->SetStreamSource(c->w[0], (D3DVertexBuffer *)c->p[0],
                             c->w[1], c->w[2]);
        break;

    case QOP_DrawPrimitive:
        dev->DrawPrimitive((D3DPRIMITIVETYPE)c->w[0], c->w[1], c->w[2]);
        break;

    case QOP_DrawPrimitiveUP:
        dev->DrawPrimitiveUP((D3DPRIMITIVETYPE)c->w[0], c->w[1],
                             c->p[0], c->w[2]);
        break;

    /* Las dos lecturas que solo ocurren la PRIMERA vez que se mira un
     * estado: a partir de ahi contesta el espejo. */
    case QOP_GetRenderState:
        dev->GetRenderState((D3DRENDERSTATETYPE)c->w[0], (DWORD *)c->p[0]);
        break;

    case QOP_GetScissorRect:
        dev->GetScissorRect((RECT *)c->p[0]);
        break;

    /* La destruccion, EN ORDEN con los dibujos que aun la referencian.
     * Ver d3dq_soltar en d3d9_grabador.h. */
    case QOP_SoltarRecurso:
        /* `D3DResource`, NO `IUnknown`: en 360 los recursos de D3D no son
         * COM y `Release()` no es virtual (d3d9.h:935).  Castear a IUnknown
         * aqui costo un salto a 0x00000000 en el arbol de origen.
         *
         * ★ Y fuera de la sombra ANTES de soltarlo: si no, el siguiente bind
         * lo volveria a ligar.
         *
         * ★★ [log 33] Salvo una textura todavia ligada: esa se APLAZA hasta
         * que se religue su sampler.  Ver `suelta_recurso`. */
        suelta_recurso(c->p[0]);
        break;

    /* ================= [xbox64] las tres de la etapa 3 ================== */
    case QOP_DibujaVerts:
        gxd3d_consumidor_dibuja(dev, c->w[0], c->w[1], c->p[0], c->w[2], c->w[3]);
        break;

    case QOP_SubeTextura:
        gxd3d_consumidor_sube_tex((IDirect3DTexture9 *)c->p[0], c->p[1],
                                  (int)c->w[0], (int)c->w[1], (int)c->w[2],
                                  (int)c->w[3]);
        break;

    case QOP_Entrega:
        xenon64_video_resolve();
        break;

    /* [Wii64 443f68b] La copia de profundidad: ver gxd3d_consumidor_marca_z. */
    case QOP_MarcaProfundidad:
        gxd3d_consumidor_marca_z(dev, c->w[0], c->w[1]);
        break;

    /* [etapa 4] El fin de frame.  Lo trata `d3dq_consume_una`, que es quien
     * para ahi; cuando llega hasta aqui es porque se esta vaciando todo de
     * golpe (`d3dq_drena`: modo sin hilo, carga, cierre), y no hay nada que
     * hacer.  Salvo mirar, sin bloquear, que copias de profundidad ha
     * terminado ya la GPU (DepthCopy.cpp; el modo sin hilo pasa por aqui). */
    case QOP_Swap:
        gxd3d_consumidor_revisa_z(dev);
        break;

    default:
        /* ★ Una orden que no se sabe ejecutar es un fallo de codificacion,
         * y en silencio se manifestaria como "falta un dibujo" horas
         * despues.  Que salga en el log. */
#if XBOX64_TRAZAS
        ++d3dq_dbg_descon;
#endif
        break;
    }
}

void d3dq_drena(void)
{
#if D3D_GRABADOR >= 2
    IDirect3DDevice9 *dev = s_real;

    if (!dev)
    {
        /* Sin device no hay nada que ejecutar.  Se descarta para no atascar
         * al productor -- que en el valor 2 es este mismo hilo -- y solo se
         * alcanza en el apagado, cuando el device del frontend ya se solto.
         * Si alguna vez sale con el emulador corriendo, aqui se estan
         * perdiendo dibujos en silencio. */
        g_d3dqCola = g_d3dqCabeza;
        g_d3dqPoolLibre = g_d3dqPoolCab;
        return;
    }

    /* [xbox64] El pico de ordenes pendientes: lo que dimensiona el anillo de
     * la etapa 4, donde se acumulan hasta ~3 frames. */
#if XBOX64_TRAZAS
    if (D3DQ_USADAS() > d3dq_dbg_maxcola)
        d3dq_dbg_maxcola = D3DQ_USADAS();
#endif

    while (g_d3dqCola != g_d3dqCabeza)
    {
        const D3DQCmd *c;
        /* Ver la orden entera antes de leerla.  Pareja del __lwsync de
         * d3dq_mete. */
        __lwsync();
        c = &g_d3dq[g_d3dqCola & D3DQ_MASC];
        d3dq_ejecuta(c, dev);
        /* [etapa 4] Sus datos ya no hacen falta: liberarlos.  Ver el pool en
         * anillo, en d3dq_payload. */
        g_d3dqPoolLibre = c->poolFin;
        __lwsync();
        g_d3dqCola = g_d3dqCola + 1u;
    }

    /* ★ [Perfect Dark, log 10] Las copias asincronas (Z y color) se dan por
     * terminadas al revisar sus fences, y eso solo lo hacia el SWAP.  Sin el
     * hilo grafico (xbox64_gfx_thread desactivado) NO HAY NINGUN SWAP en la
     * cola -- solo lo meten hilo_emu.cpp y hilo_lista.cpp --, asi que ninguna
     * copia llegaba nunca a "lista": [PEEKZ] revisiones=0, los tres huecos de
     * Z con su fence puesta y lista=0 para siempre.  Aqui, que en ese modo es
     * el consumidor, tambien.  No bloquea. */
    gxd3d_consumidor_revisa_z(dev);

#if XBOX64_TRAZAS
    ++d3dq_dbg_drenajes;
#endif
#endif /* D3D_GRABADOR >= 2 */
}

#if D3D_GRABADOR >= 3
/* ====================================================================== *
 * ★★★ [etapa 4] EL CONSUMIDOR Y EL BUZON.  Ver el modo 3 en la cabecera.
 * ====================================================================== */

/* Una orden del anillo.  La llama SOLO el consumidor: `retro_run` a traves
 * del bucle de xbox/hilo_emu.cpp.  Es el cuerpo de `commandLoop` de GLideN64
 * (ThreadedOpenGl/opengl_Wrapper.cpp:73-101), de uno en uno para que quien
 * llama pueda parar en el SWAP y atender el buzon entre medias. */
int d3dq_consume_una(void)
{
    const D3DQCmd *c;
    unsigned int op;

    if (g_d3dqCola == g_d3dqCabeza)
        return 0;

    /* Ver la orden entera antes de leerla.  Pareja del __lwsync de
     * d3dq_mete. */
    __lwsync();
    c  = &g_d3dq[g_d3dqCola & D3DQ_MASC];
    op = c->op;
    if (op != QOP_Swap && s_real)
        d3dq_ejecuta(c, s_real);
    else if (s_real)
        /* [Wii64 443f68b] En cada fin de frame: que copias de profundidad ha
         * terminado ya la GPU.  No bloquea (gxd3d_consumidor_revisa_z). */
        gxd3d_consumidor_revisa_z(s_real);
    g_d3dqPoolLibre = c->poolFin;
    __lwsync();
    g_d3dqCola = g_d3dqCola + 1u;

    return (op == QOP_Swap) ? 2 : 1;
}

/* El consumidor atiende la orden sincrona pendiente, si la hay. */
int d3dq_atiende_buzon(void)
{
    if (!s_buzonLleno)
        return 0;
    __lwsync();
    if (s_real)
        d3dq_ejecuta(&s_buzon, s_real);
    /* Lo que la orden escribio (el puntero al objeto creado, el HRESULT)
     * tiene que ser visible ANTES de soltar al productor. */
    __lwsync();
    s_buzonLleno = 0;
    if (s_evBuzon) SetEvent(s_evBuzon);
    return 1;
}

int d3dq_buzon_ocupado(void)
{
    return s_buzonLleno ? 1 : 0;
}

/* El productor manda una orden sincrona y espera.
 *
 * ★ Los punteros de la orden -- `&hr`, `ppTexture` -- son de la PILA del
 * productor, y es correcto: se queda aqui esperando hasta que el
 * consumidor ha escrito en ellos.  Es lo mismo que hace GLideN64 con sus
 * `_synced` (`waitOnCommand`, opengl_Command.cpp:51). */
void d3dq_sincrona(const D3DQCmd *c)
{
#if XBOX64_TRAZAS   /* cronometro y censo: solo los lee el log */
    unsigned long long t0;

    ++d3dq_dbg_sinc_n;
#endif

    if (!g_d3dqVivo)
    {
        /* Sin otro hilo: al anillo y vaciar en el acto, como el valor 2.
         * (`d3dq_mete` ya la cuenta en el censo.) */
        d3dq_mete(c);
        d3dq_drena();
        return;
    }

#if XBOX64_TRAZAS
    if (c->op < 40u) ++d3dq_dbg_op[c->op];
    t0 = __mftb();
#endif
    s_buzon = *c;
    __lwsync();
    s_buzonLleno = 1;
    d3dq_despierta();
    while (s_buzonLleno)
    {
        if (s_evBuzon)
            WaitForSingleObject(s_evBuzon, 2);
        else
            YieldProcessor();
        __lwsync();
    }
    __lwsync();
#if XBOX64_TRAZAS
    d3dq_dbg_sinc_tb += (unsigned int)(__mftb() - t0);
#endif
}

void d3dq_mete_swap(void)
{
    D3DQCmd c;
    c.op = QOP_Swap;  c.poolFin = 0;
    c.p[0] = c.p[1] = c.p[2] = c.p[3] = c.p[4] = 0;
    c.w[0] = c.w[1] = c.w[2] = c.w[3] = c.w[4] = c.w[5] = 0;
    d3dq_mete(&c);
}

/* ====================================================================== *
 * ★★ [etapa 5] EL ALMACEN DE TEXTURAS.  Ver la declaracion en
 * d3d9_grabador.h: el porque, medido en el log 27.
 *
 * Una CLASE por combinacion (ancho, alto, formato, uso, pool) con un nivel.
 * Cada clase es un anillo de UN escritor por cursor: el consumidor mete
 * (`w`), el productor saca (`r`).  Las clases las da de alta el PRODUCTOR
 * cuando pide una que no existe (solo el sube `s_almN`); el consumidor solo
 * lee las ya publicadas.
 *
 * Topes, para que no se coma la memoria: clases de hasta 256x256 texels,
 * ALM_NIVEL texturas por clase y ALM_PRESUPUESTO bytes en total en el
 * almacen.  Cuantas se crean en cada llamada lo decide quien llama.
 *
 * ★ [log 31] LOS TOPES DE LA PRIMERA VERSION SE QUEDABAN CORTOS, MEDIDO.
 * Cruis'n USA crea 700-1250 texturas por segundo; con 8 por pasada el
 * almacen no pasaba de 480/s (`alm=... (+480)`, exacto: 8 x 60) y la lista
 * seguia esperando 0,2-0,4 s de cada segundo en el buzon (`sinc`).  Ahora 32
 * por pasada, 8 por clase, y se repone tambien mientras `retro_run` espera
 * al SWAP, que es tiempo muerto del dueno del device.
 *
 * ★★ [Logs Salvia (15)] Y AUN SE QUEDABA CORTO: cada bajon de Cruis'n
 * coincide con texturas que el almacen no tenia (`alm=399/412`: 13 por el
 * buzon) y cada una cuesta ~9,6 ms del hilo de la lista parado (`sinc=13
 * (125041 us)`, 46 VI/s).  Ahora 24 por clase (rafagas de una misma talla
 * dentro de un frame) y 48 MB: quedan ~250 MB de memoria fisica libres en
 * partida.
 * ====================================================================== */
#define ALM_CLASES      64u
#define ALM_HONDO       32u             /* potencia de 2, > ALM_NIVEL */
#define ALM_NIVEL       24u
#define ALM_MAX_TEXELS  (256u * 256u)
#define ALM_PRESUPUESTO (48u << 20)     /* bytes en el almacen, como mucho */

struct AlmClase
{
    UINT      w, h;
    DWORD     usage;
    D3DFORMAT fmt;
    D3DPOOL   pool;
    D3DTexture           *anillo[ALM_HONDO];
    volatile unsigned int aw;           /* consumidor */
    volatile unsigned int ar;           /* productor  */
};
static AlmClase              s_alm[ALM_CLASES];
static volatile unsigned int s_almN = 0;   /* productor: clases publicadas */
/* Bytes en el almacen = creados (consumidor) - tomados (productor).  Dos
 * contadores de un escritor cada uno; la resta es la ocupacion. */
static volatile unsigned int s_almCreados = 0;
static volatile unsigned int s_almTomados = 0;

int d3dq_almacen_toma(UINT w, UINT h, UINT levels, DWORD usage,
                      D3DFORMAT fmt, D3DPOOL pool, D3DTexture **pp)
{
    unsigned int i;
    const unsigned int n = s_almN;

    if (!pp || levels != 1 || w * h > ALM_MAX_TEXELS)
        return 0;

    for (i = 0; i < n; i++)
    {
        AlmClase *c = &s_alm[i];
        if (c->w != w || c->h != h || c->fmt != fmt ||
            c->usage != usage || c->pool != pool)
            continue;
        if (c->aw != c->ar)
        {
            __lwsync();                /* la textura entera antes de usarla */
            *pp = c->anillo[c->ar & (ALM_HONDO - 1u)];
            __lwsync();
            c->ar = c->ar + 1u;
            s_almTomados = s_almTomados + w * h * 4u;
#if XBOX64_TRAZAS
            ++d3dq_dbg_alm_toma;
#endif
            return 1;
        }
#if XBOX64_TRAZAS
        ++d3dq_dbg_alm_falla;
#endif
        return 0;                       /* la clase existe pero esta vacia */
    }

    /* Clase nueva: se da de alta para la proxima reposicion. */
    if (n < ALM_CLASES)
    {
        AlmClase *c = &s_alm[n];
        c->w = w;  c->h = h;  c->fmt = fmt;  c->usage = usage;  c->pool = pool;
        c->aw = 0; c->ar = 0;
        __lwsync();
        s_almN = n + 1u;
    }
#if XBOX64_TRAZAS
    ++d3dq_dbg_alm_falla;
#endif
    return 0;
}

void d3dq_almacen_repone(unsigned int max)
{
    IDirect3DDevice9 *dev = s_real;
    unsigned int i, creadas = 0;
    const unsigned int n = s_almN;

    if (!dev) return;
    __lwsync();                         /* ver las clases ya publicadas */
    for (i = 0; i < n && creadas < max; i++)
    {
        AlmClase *c = &s_alm[i];
        const unsigned int bytes = c->w * c->h * 4u;
        while ((unsigned int)(c->aw - c->ar) < ALM_NIVEL && creadas < max &&
               (unsigned int)(s_almCreados - s_almTomados) + bytes <= ALM_PRESUPUESTO)
        {
            D3DTexture *t = NULL;
            if (FAILED(dev->CreateTexture(c->w, c->h, 1, c->usage, c->fmt,
                                          c->pool, &t, NULL)) || !t)
                break;
            c->anillo[c->aw & (ALM_HONDO - 1u)] = t;
            __lwsync();                 /* la textura antes que el cursor */
            c->aw = c->aw + 1u;
            s_almCreados = s_almCreados + bytes;
            ++creadas;
#if XBOX64_TRAZAS
            ++d3dq_dbg_alm_crea;
#endif
        }
    }
}

void d3dq_almacen_vaciar(void)
{
    unsigned int i;
    for (i = 0; i < s_almN; i++)
    {
        AlmClase *c = &s_alm[i];
        while (c->ar != c->aw)
        {
            D3DTexture *t = c->anillo[c->ar & (ALM_HONDO - 1u)];
            if (t) ((D3DResource *)t)->Release();
            c->ar = c->ar + 1u;
        }
        c->aw = c->ar = 0;
    }
    s_almN = 0;
    s_almCreados = s_almTomados = 0;
}
#endif /* D3D_GRABADOR >= 3 */
#endif /* D3D_GRABADOR (el bloque entero del grabador, valores 1, 2 y 3) */

/* Ver el bloque largo de d3dq_soltar en d3d9_grabador.h.
 *
 * ★ Estaba en xbox/gfx_host.cpp como un `Release()` a secas, que es
 * exactamente lo que hace aqui con el anillo apagado o en pasarela.  Se
 * mueve para que la version encolada (valores 2 y 3) viva en el mismo sitio
 * que el anillo al que pertenece. */
void d3dq_soltar(D3DResource *pRecurso)
{
    if (!pRecurso)
        return;
#if D3D_GRABADOR >= 2
    {
        D3DQCmd c;
        c.op = QOP_SoltarRecurso;
        c.p[0] = pRecurso; c.p[1] = 0; c.p[2] = 0; c.p[3] = 0; c.p[4] = 0;
        c.w[0] = c.w[1] = c.w[2] = c.w[3] = c.w[4] = c.w[5] = 0;
        d3dq_mete(&c);
    }
#else
    pRecurso->Release();
#endif
}

/* Censo de ordenes RARAS para la linea [D3DQOP].  Los indices salen del enum
 * D3DQOp, y cablearlos a mano en otro fichero es precisamente como se
 * desincronizan sin que nadie se entere.  Se publica solo el punado raro:
 * las comunes ya se ven en `ordenes` y solo taparian la linea. */
extern "C" void d3dq_censo(char *buf, int n)
{
#if D3D_GRABADOR && XBOX64_TRAZAS
    _snprintf(buf, n, "[D3DQOP] crearTex=%u crearPS=%u crearVS=%u crearVB=%u"
                      " crearDecl=%u resolve=%u clear=%u drawUP=%u"
                      " getRS=%u getSC=%u",
        d3dq_dbg_op[QOP_CreateTexture],      d3dq_dbg_op[QOP_CreatePixelShader],
        d3dq_dbg_op[QOP_CreateVertexShader], d3dq_dbg_op[QOP_CreateVertexBuffer],
        d3dq_dbg_op[QOP_CreateVertexDeclaration],
        d3dq_dbg_op[QOP_Resolve],            d3dq_dbg_op[QOP_Clear],
        d3dq_dbg_op[QOP_DrawPrimitiveUP],    d3dq_dbg_op[QOP_GetRenderState],
        d3dq_dbg_op[QOP_GetScissorRect]);
    if (n > 0) buf[n - 1] = 0;
    {
        int i;
        for (i = 0; i < 40; ++i) d3dq_dbg_op[i] = 0;
    }
#else
    if (n > 0) buf[0] = 0;
#endif
}
