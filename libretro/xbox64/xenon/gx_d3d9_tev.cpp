/*
 * gx_d3d9_tev.cpp  --  el combinador: TEV de GameCube -> HLSL ps_3_0.
 *
 * ★★ POR QUE ESTA VIA ES BARATA EN GPU, Y HAY QUE CONSERVARLO
 * ------------------------------------------------------------
 * Rice genera UN PIXEL SHADER POR MUX del RDP (d3d9_backend.cpp:6947,
 * `m_vCompiledShaders` sin tope ni purga).  GLideN64 hace lo mismo y ademas
 * sus shaders piden 8-14 muestreos y 150-500 instrucciones por pixel.
 *
 * El TEV es un combinador POR ETAPAS: un numero pequeño de etapas fijas con
 * entradas seleccionables.  Eso se traduce a POCOS shaders parametrizados,
 * y la mayor parte de la variacion viaja en CONSTANTES, no en codigo.  Es la
 * ventaja estructural de portar glN64_GX y la razon de no aplanarla.
 *
 * Aun asi se genera HLSL por configuracion (no un uber-shader con ramas):
 * en ps_3_0 una rama dinamica cuesta, y Xenos tiene sitio de sobra --
 * `GPU_INSTRUCTIONS 4096` y 256 constantes de pixel shader
 * (d3d9gpu.h:176-209), no los 512/224 del SM3.0 minimo de PC.
 *
 * SEMANTICA DEL TEV QUE SE EMULA (por etapa, color y alfa por separado):
 *
 *     raw = d  ±  ( (1-c)*a + c*b )      (+ = GX_TEV_ADD, - = GX_TEV_SUB)
 *     out = clamp( (raw + bias) * scale, 0, 1 )      si clamp
 *
 *     bias : GX_TB_ZERO 0 | GX_TB_ADDHALF +0.5 | GX_TB_SUBHALF -0.5
 *     scale: GX_CS_SCALE_1 1 | _2 2 | _4 4 | GX_CS_DIVIDE_2 0.5
 *
 * El resultado va al registro `tevregid` (PREV, REG0, REG1, REG2), que la
 * etapa siguiente puede leer.
 */

#include <xtl.h>
#include <d3dx9.h>
#include <ppcintrinsics.h>      /* __mftb: se cronometra la compilacion */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "gx_d3d9_int.h"
#include "../source/xenon64_diag.h"

extern "C" volatile unsigned int gxd3d_dbg_shgen    = 0;
extern "C" volatile unsigned int gxd3d_dbg_shhit    = 0;
extern "C" volatile unsigned int gxd3d_dbg_shfail   = 0;
extern "C" volatile unsigned int gxd3d_dbg_shcompus = 0;
extern "C" volatile unsigned int gxd3d_dbg_shovf    = 0;

GXD3DTevState g_gxTev;

/* ------------------------------------------------------------------ */
/* Diagnostico                                                         */
/* ------------------------------------------------------------------ */
/* Volcado del HLSL generado.  Es el equivalente de LOG_GEN_HLSL de
 * d3d9_backend.cpp, y NO es opcional tenerlo desde el primer dia: el unico
 * fallo que resolvio el combinador de Rice fue la biseccion del shader, y
 * las seis hipotesis falsas de antes salieron de razonar sobre estado global
 * en vez de mirar el texto generado (ver project_xenon64_texturas_d3d9). */
/* >>> USADO Y APAGADO (2026-09-22), para los modelos blancos de Pokemon
 * Stadium.  Lo que contesto, y sirve para no volver a mirar aqui:
 *
 *   - Los 42 shaders de una partida son estructuralmente SANOS.  El tipico
 *     es `cprev = tex2D(uTex0, uv0)` y ya.
 *   - Casi todos acaban con una etapa `tm2` que muestrea `uTex2` y NO USA el
 *     resultado (`cres = cprev.rgb + cmix`).  Es una lectura muerta que
 *     cuesta un fetch por pixel, no un fallo -- pero si alguna vez hace
 *     falta rascar relleno de pixel shader, ahi hay uno gratis.
 *   - `rasc` (el color de vertice) se declara y en muchos shaders no se usa,
 *     que es lo que el mux del N64 pide en esos casos. */
/* >>> USADO Y APAGADO otra vez (2026-09-27), para el escenario negro de 007
 * TWINE.  Lo que contesto: el HLSL generado para el combinador sospechoso era
 * CORRECTO -- traduccion exacta del mux que pide el juego--, y eso fue lo que
 * mando a mirar lo que pasa DESPUES del combinador, donde estaba la niebla.
 * Se deja el codigo: leer el texto generado es la unica forma de auditar el
 * traductor, y se enciende con este define. */
#define TEV_LOG_HLSL   (XENON64_DIAG && 0)

/* Biseccion, igual que PS_BISECT:
 *   0 = normal
 *   1 = devolver solo la textura 0 cruda
 *   2 = devolver solo el color del vertice
 *   3 = devolver el alfa en gris
 * Con esto se parte "no se ve bien" en "el insumo esta mal" contra "el
 * combinador esta mal", que es la pregunta que de verdad hace avanzar. */
#define TEV_BISECT     0

extern "C" void DebugOutputString(const char *s);
/* [Doom 64, Conker] El combinador del N64 de glN64 para el volcado
 * (xenon/gln64_config.cpp).  Ver GLN64_MARCA_VOLCADO. */
extern "C" void gln64_mux_actual(unsigned int *m0, unsigned int *m1, unsigned int *ciclo);

/* ------------------------------------------------------------------ */
/* Cache de shaders                                                    */
/* ------------------------------------------------------------------ */
/* La clave es la configuracion de etapas que afecta al CODIGO generado.  Lo
 * que solo cambia valores (reg[], konst[], refs del alpha compare, niebla)
 * viaja en constantes y NO entra en la clave: esa es justo la propiedad que
 * hace que salgan pocos shaders. */
struct GXD3DTevKey
{
    u8 numStages;
    /* ★ CORTE DE DIAGNOSTICO: 0 normal, N = "emitir solo las N primeras
     * etapas y devolver el registro destino de la ultima".
     *
     * Va EN LA CLAVE aunque `numStages` tambien se trunque, y no es
     * redundante: cortar un shader de 5 etapas a 5 daria exactamente la misma
     * clave que el normal, y se serviria el shader cacheado equivocado.  Con
     * este byte los dos no pueden colisionar nunca. */
    u8 corte;
    u8 numTexGens;
    struct {
        u8 texcoord, color;
        u8 texmap;
        u8 cin[4], ain[4];
        u8 cop, cbias, cscale, cclamp, creg;
        u8 aop, abias, ascale, aclamp, areg;
        u8 kcsel, kasel;
    } st[GXD3D_MAX_STAGES];
    u8 acComp0, acOp, acComp1;   /* las REFS van en constante, la FUNCION no */
    u8 ztOp;
    u8 fogType;
    u8 texMono[2];               /* GXD3D_SetCopyMono: 0, 1 = .rrrr, 2 = y alfa 0.825 */
    u8 blFog;                    /* GXD3D_SetBlenderNiebla */
    u8 fogInv;                   /* GXD3D_SetNieblaInversa */
    u8 zClamp;                   /* GXD3D_SetDepthClamp */
    u8 rtz;                      /* GXD3D_SetRenderToDepth: 0 a 5 */
    u8 decN64;                   /* GXD3D_SetDecalN64 */
    u8 nieblaZ;                  /* GXD3D_SetNieblaZ */
    u8 atlas;                   /* GXD3D_SetLodAtlas, solo con LOD_FRACTION;
                                  * 2 = el de RESPALDO mientras el de atlas se
                                  * compila (ver TEV_ASYNC) */
};

struct GXD3DShaderEntry
{
    GXD3DTevKey key;
    IDirect3DPixelShader9 *ps;
    int trabajo;                 /* -1, o la ranura de TEV_ASYNC que lo compila */
};

#define TEV_CACHE_MAX 256
static GXD3DShaderEntry s_cache[TEV_CACHE_MAX];
static int s_cacheCount = 0;
static int s_dirty = 1;
static IDirect3DPixelShader9 *s_curPS = NULL;

/* ★★ ¿SIGUE LIGADO EN EL DEVICE EL SHADER QUE CREEMOS?  UNA VEZ POR PASADA,
 * NO UNA POR LOTE.
 *
 * `gxd3d_tev_apply()` reafirmaba `SetPixelShader(s_curPS)` en CADA lote en el
 * que el combinador no habia cambiado.  Con 5.981 lotes/s en OoT eso son
 * ~6.000 `SetPixelShader` por segundo para no cambiar nada.
 *
 * La razon de que estuviera era buena: el device esta COMPARTIDO con el
 * frontend, que liga el suyo para dibujar su quad y su menu.  Pero solo puede
 * hacerlo ENTRE pasadas, nunca a mitad -- y ese mismo razonamiento ya esta
 * aplicado en este shim al vertex shader, la declaracion y el stream source
 * (`s_bindValid`, gx_d3d9.cpp:244) y al espejo de render states
 * (`gxd3d_state_invalidate`).  El pixel shader era la unica pieza a la que no
 * se le habia aplicado: una inconsistencia del propio fichero.
 *
 * ★ Y NO BASTA CON `s_dirty`.  `gxd3d_frame_begin` llama a `gxd3d_tev_dirty()`,
 * asi que el primer lote de la pasada SI entra por el camino largo -- pero
 * alli el `SetPixelShader` esta guardado por `if (ps != s_curPS)`, y si el
 * juego arranca la pasada con el mismo combinador con el que la termino, `ps`
 * ES `s_curPS` y no se reafirmaria nada.  Por eso hace falta esta bandera
 * aparte y no vale reutilizar la suciedad. */
static int s_psBindValid = 0;

void gxd3d_tev_invalidar_ligado(void) { s_psBindValid = 0; }

/* ★ La clave del shader que HAY LIGADO ahora mismo.  Sin esto no se puede
 * contestar la unica pregunta que importa para fusionar lotes: "¿aplicar
 * ahora cambiaria algo?".  Ver gxd3d_tev_difiere(). */
static GXD3DTevKey s_curKey;
static int         s_curKeyValid = 0;
/* La entrada de la cache cuyo shader de atlas se esta compilando mientras
 * dibuja el de respaldo, o -1.  Ver TEV_ASYNC. */
static int         s_curPend = -1;

/* ★ SHADER DE EMERGENCIA.  Es el equivalente del `g_pPS_FB` de Rice, y no es
 * un lujo: sin el, un fallo de compilacion deja `SetPixelShader(NULL)`, y en
 * Xenos -- que no tiene funcion fija-- el draw no produce NADA.  O sea que el
 * sintoma de un generador roto es PANTALLA NEGRA, que no dice nada.
 *
 * Con el, lo que se ve es la textura por el color del vertice: mal, pero
 * RECONOCIBLE, y el juego sigue siendo jugable mientras se depura.  El
 * contador `shfail` dice cuantas veces se ha caido aqui. */
static IDirect3DPixelShader9 *s_psRescate = NULL;

/* ===========================================================================
 * ★★ VACIAR LA CACHE AL CAMBIAR DE ROM
 * ===========================================================================
 * Esta cache es LA UNICA pieza de la tuberia grafica que sobrevive a una
 * carga de juego.  Todo lo demas se destruye y se reconstruye:
 * `RomClosed` -> `OGL_Stop()` -> `Combiner_Destroy`, `FrameBuffer_Destroy`,
 * `TextureCache_Destroy`.  La nuestra no se entera, y se ve en el log:
 * `shgen` va 20 -> 40 -> 60 a lo largo de tres cargas sin reiniciarse nunca.
 *
 * Contexto de por que se toca esto y no otra cosa.  Sintoma: en Super Mario
 * 64 los quesitos del marcador POWER se ven si el juego se carga EL PRIMERO
 * y no se ven si antes se cargo otro.  Descartado CON DATOS, en este orden:
 *
 *   - gSPModifyVertex en coordenadas de pantalla  ([GLN64MV] nunca sale)
 *   - dibujar al z-buffer                         ([GLN64ZI] a cero)
 *   - comandos de display list sin implementar    ([GLN64GBI] vacio)
 *   - recorte por w<=0                            (10 en toda la sesion)
 *   - el desplazamiento de tile del punto K       (=DESPL nunca, lod/det=0)
 *   - la cadena de mips del punto M               (=MIP nunca)
 *   - needReplaceTex1ByTex0                       (rep0 siempre)
 *   - scissor degenerado                          (degen=18 tambien cuando VA BIEN)
 *   - fuga o fallo del monton de texturas         (mismo `usado`, FALLOS=0)
 *   - el juego dibuja otra cosa                   (censo de modos IDENTICO
 *                                                  entre las dos sesiones)
 *
 * Con todo eso fuera, el dibujo ocurre con el mismo estado y las mismas
 * texturas, asi que lo que queda es con que SHADER se pinta.  Y esta cache es
 * lo unico que cruza la frontera de la carga.
 *
 * ★ NO ES UNA CERTEZA, es la ultima pieza en pie.  Pero quitar la asimetria
 * -- que una cache sobreviva mientras las otras tres se tiran-- es correcto
 * por si mismo, cueste lo que cueste el diagnostico: 60 shaders se
 * regeneran en unos pocos frames y `shgen` vuelve a contar desde cero, que
 * de paso hace los logs comparables entre cargas.
 *
 * ★★ MEDIDO: NO ERA.  Con el vaciado, Mario 64 genera los MISMOS 20 shaders
 * que generaba sin el, o sea que la cache nunca le estaba sirviendo entradas
 * del juego anterior: la clave ya discriminaba bien.  Vuelve a 0 con el
 * resto de la caza (ver GLN64_CAZA_MARIO64 en xenon/gln64_prof.h).
 *
 * Se deja escrito porque la asimetria es real -- esta cache sigue siendo la
 * unica pieza que sobrevive a una carga-- y porque el vaciado esta
 * comprobado inocuo; pero encenderlo sin un defecto que lo pida seria
 * cambiar comportamiento sin motivo. */
#define GLN64_FLUSH_SHADERS_AL_CARGAR 0

extern "C" void gxd3d_tev_flush_cache(void)
{
#if GLN64_FLUSH_SHADERS_AL_CARGAR
    int i;

    for (i = 0; i < s_cacheCount; i++)
    {
        if (s_cache[i].ps)
            s_cache[i].ps->Release();
        s_cache[i].ps = NULL;
    }
    s_cacheCount = 0;

    /* El shader LIGADO tambien se va: su objeto acaba de soltarse, asi que
     * dejar el puntero puesto seria justo el use-after-free que este vaciado
     * pretende evitar.  `s_dirty` obliga a resolver de nuevo en el primer
     * dibujo del juego nuevo. */
    s_curPS       = NULL;
    s_curKeyValid = 0;
    s_dirty       = 1;
#if D3D_GRABADOR
    /* [etapa 3] Y la sombra del consumidor, por la misma razon: guarda el
     * ultimo pixel shader ligado, y el siguiente `d3dq_bind()` lo volveria a
     * ligar despues de soltado.  Ver D3DQ_BIND en d3d9_grabador.h. */
    d3dq_sombra_reset();
#endif
#endif
}

static void gxd3d_build_rescue(void)
{
    static const char *src =
        "sampler2D uTex0 : register(s0);\n"
        "struct PS_IN { float2 uv0:TEXCOORD0; float2 uv1:TEXCOORD1;\n"
        "               float2 uv2:TEXCOORD2; float4 col:COLOR0;\n"
        "               float4 pos:TEXCOORD3; };\n"
        "float4 main(PS_IN IN) : COLOR { return tex2D(uTex0, IN.uv0) * IN.col; }\n";
    LPD3DXBUFFER code = NULL, err = NULL;
    if (s_psRescate) return;
    if (SUCCEEDED(D3DXCompileShader(src, (UINT)strlen(src), NULL, NULL,
                                    "main", "ps_3_0", 0, &code, &err, NULL)))
    {
        g_pd3dDevice->CreatePixelShader((const DWORD *)code->GetBufferPointer(),
                                        &s_psRescate);
        code->Release();
    }
#if XBOX64_TRAZAS
    else if (err)
    {
        DebugOutputString("[TEV] no se pudo compilar ni el shader de rescate");
    }
#endif
    if (err) err->Release();
}

/* ===========================================================================
 * ★ LOS SHADERS CON ATLAS SE COMPILAN EN OTRO HILO
 * ===========================================================================
 * Medido (Logs Salvia (30), Conker; parones de 300-950 ms en un frame): un
 * shader normal compila en 5-10 ms y uno con atlas (k->atlas, las dos
 * atlasNivel en linea) en ~300 ms.  Antes del atlas (logs 13 y 14) ningun
 * segundo pasaba de 52 ms de compilacion; desde el log 16, si.
 *
 * Ahora el TEXTO se genera aqui (es barato), D3DXCompileShader corre en un
 * hilo propio y, mientras tanto, se dibuja con el shader de RESPALDO: la
 * misma clave con atlas = 2, que lee el tile 0 en los dos texmap (la LOD sin
 * mezcla de niveles).  No el de atlas = 0: con el atlas armado, la unidad 1
 * tiene el atlas y no el tile 1.  CreatePixelShader se queda en el hilo que
 * dibuja: con la lista en paralelo el device es el grabador, de un solo
 * productor.
 *
 * El hilo se crea la primera vez que hace falta y vive lo que el proceso
 * (crear hilos por carga colgaba `CreateThread` en pcsxr-360).
 *
 * ★ HW 5, NO HW 3.  Medido (Logs Salvia (31)): en HW 3 cada shader de atlas
 * tardaba 0,5-1,7 s (en el lote, en HW 4, ~300 ms) y el emulador bajaba
 * mientras: HW 3 es el hermano SMT de HW 2, el emulador, que va saturado
 * (`[HILO] trab=` 16-17 ms/VI).  El core 2 es el menos cargado: HW 4 la
 * lista (~40 por ciento en Conker; ociosa sin lista en paralelo) y HW 5 el
 * audio de SDL, que ya duerme con Sleep(1) en XboxDX_PlayAudio.  Prioridad
 * LOWEST para que el hilo de audio, al despertar, le quite HW 5 en el acto. */
#define TEV_ASYNC        1
#define TEV_ASYNC_HW     5
#define TEV_ASYNC_MAX    16

struct TevTrabajo
{
    char *src;                   /* el HLSL; lo suelta quien recoge */
    LPD3DXBUFFER code;
    HRESULT hr;
    unsigned int us;
    unsigned int respaldos;      /* busquedas servidas con el respaldo */
    volatile LONG estado;        /* 0 libre, 1 encolado, 2 compilado */
};
static TevTrabajo s_trab[TEV_ASYNC_MAX];
static HANDLE s_trabHilo = NULL, s_trabEv = NULL;

static DWORD WINAPI tev_trabajador(LPVOID p)
{
    int i;
    (void)p;
    for (;;)
    {
        WaitForSingleObject(s_trabEv, INFINITE);
        for (i = 0; i < TEV_ASYNC_MAX; i++)
        {
            TevTrabajo *t = &s_trab[i];
#if XBOX64_TRAZAS   /* `us` solo lo lee la linea [TEVASYNC] */
            unsigned int t0, t1;
#endif
            if (t->estado != 1) continue;
            __lwsync();              /* el texto entero antes de leerlo */
            t->code = NULL;
#if XBOX64_TRAZAS
            t0 = (unsigned int)__mftb();
#endif
            t->hr = D3DXCompileShader(t->src, (UINT)strlen(t->src), NULL, NULL,
                                      "main", "ps_3_0", 0, &t->code, NULL, NULL);
#if XBOX64_TRAZAS
            t1 = (unsigned int)__mftb();
            t->us = (t1 - t0) / 50;
#endif
            __lwsync();              /* el resultado antes que el estado */
            t->estado = 2;
        }
    }
    return 0;
}

/* Encola el texto `src` (pasa a ser del trabajo).  -1 si no hay hilo o
 * ranura: entonces el que llama compila en el acto, como antes. */
static int tev_async_encolar(char *src)
{
    int i;
    if (!s_trabHilo)
    {
        s_trabEv = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!s_trabEv) return -1;
        s_trabHilo = CreateThread(NULL, 256 * 1024, tev_trabajador, NULL,
                                  CREATE_SUSPENDED, NULL);
        if (!s_trabHilo) { CloseHandle(s_trabEv); s_trabEv = NULL; return -1; }
        SetThreadPriority(s_trabHilo, THREAD_PRIORITY_LOWEST);
        XSetThreadProcessor(s_trabHilo, TEV_ASYNC_HW);
        ResumeThread(s_trabHilo);
#if XBOX64_TRAZAS
        DebugOutputString("[TEVASYNC] hilo de compilacion creado (HW 5, LOWEST)");
#endif
    }
    for (i = 0; i < TEV_ASYNC_MAX; i++)
        if (s_trab[i].estado == 0)
        {
            s_trab[i].src = src;
            s_trab[i].code = NULL;
            s_trab[i].respaldos = 0;
            __lwsync();
            s_trab[i].estado = 1;
            SetEvent(s_trabEv);
            return i;
        }
    return -1;
}

/* Si el trabajo `j` ya compilo, crea el shader (en ESTE hilo), libera la
 * ranura y lo devuelve; el de rescate si fallo.  NULL si sigue compilando. */
static IDirect3DPixelShader9 *tev_async_recoger(int j)
{
    TevTrabajo *t = &s_trab[j];
    IDirect3DPixelShader9 *ps = NULL;
#if XBOX64_TRAZAS
    char m[160];
    if (t->estado != 2) { t->respaldos++; return NULL; }
#else
    if (t->estado != 2) return NULL;
#endif
    __lwsync();
    if (SUCCEEDED(t->hr) && t->code)
    {
        g_pd3dDevice->CreatePixelShader((const DWORD *)t->code->GetBufferPointer(), &ps);
#if XBOX64_TRAZAS
        gxd3d_dbg_shgen++;
#endif
    }
#if XBOX64_TRAZAS
    else
        gxd3d_dbg_shfail++;
#endif
    if (t->code) t->code->Release();
    free(t->src);
#if XBOX64_TRAZAS
    _snprintf(m, sizeof(m), "[TEVASYNC] atlas compilado: us=%u hr=0x%08X respaldos=%u",
              t->us, (unsigned)t->hr, t->respaldos);
    m[sizeof(m) - 1] = 0;
    DebugOutputString(m);
#endif
    t->src = NULL;
    t->code = NULL;
    __lwsync();
    t->estado = 0;
    return ps ? ps : s_psRescate;
}

/* Al soltar la cache: esperar a lo que este compilando y tirarlo. */
static void tev_async_vaciar(void)
{
    int i;
    for (i = 0; i < TEV_ASYNC_MAX; i++)
    {
        TevTrabajo *t = &s_trab[i];
        while (t->estado == 1) Sleep(1);
        if (t->estado == 2)
        {
            __lwsync();
            if (t->code) t->code->Release();
            free(t->src);
            t->src = NULL;
            t->code = NULL;
            t->estado = 0;
        }
    }
}

/* ===========================================================================
 * ★★★ EL SHADER DEL MARCADO POR VIA: SACA UNA CONSTANTE Y NO MUESTREA NADA
 * ===========================================================================
 * `return c[TEVC_MARCA]` y se acabo.  Es "el valor fijo de iluminacion": el
 * pixel no depende ni de la textura, ni del color de vertice, ni del
 * combinador, asi que una textura negra NO puede tragarse el marcador.  Esa
 * es toda la razon de que el marcado sea un shader y no un tinte del color de
 * vertice -- el color de vertice MULTIPLICA, y `rojo * 0` sigue siendo negro,
 * que es exactamente el caso que se quiere mirar.
 *
 * El registro es `c[TEVC_TOTAL]`, o sea el siguiente al ultimo que usa el
 * generador normal: asi el envio del marcado no pisa las constantes del TEV
 * ni depende de `s_constDirty`. */

/* ★ EL REPARTO DE CONSTANTES, AQUI ARRIBA Y NO JUNTO AL GENERADOR.
 *
 * Estaba mas abajo, y `gxd3d_build_marca` -- que esta unas lineas por debajo
 * de esta nota-- imprime `TEVC_MARCA` dentro del texto de su shader.  Una
 * macro solo tiene que estar definida antes de donde se USA, y ahi se usa:
 * con el bloque abajo, `TEVC_TOTAL` llegaba sin definir al `sprintf`.
 *
 * ★ Cabe de sobra: el pixel shader de Xenos tiene 256 constantes float
 * (GPU_D3D_PIXEL_CONSTANTF_COUNT, d3d9gpu.h:203).  Pero OJO con el frontend:
 * los shaders de video de Salvia usan c1 para `textureDims`, y Rice ya se lo
 * piso una vez.  Aqui no hay conflicto porque el frontend reestablece su
 * etapa entera en XBOX_SetupQuadState antes de dibujar su quad
 * (ver project_xenon64_libretro_port). */
#define TEVC_REG     0
#define TEVC_KONST   4
#define TEVC_ACREF   8
#define TEVC_FOGCOL  9
#define TEVC_FOGPAR 10
#define TEVC_ZTEX   11
#define TEVC_BLFOG  12   /* GXD3D_SetBlenderNiebla: rgb = color, a = A_FOG */
#define TEVC_TOTAL  13

#define TEVC_MARCA TEVC_TOTAL

static IDirect3DPixelShader9 *s_psMarca = NULL;
static int                    s_marcaAnt = 0;

/* Paleta del marcado por combinador.  Ni blanco ni negro: el blanco significa
 * "sin marcar" y el negro es el defecto tipico que se investiga, asi que
 * ninguno de los dos puede significar tambien "esta ranura". */
#define GXD3D_MARCA_COLORES 8
static const unsigned int s_marcaColor[GXD3D_MARCA_COLORES] =
{
    0xFF0000u, 0x00FF00u, 0x0000FFu, 0xFFFF00u,
    0x00FFFFu, 0xFF00FFu, 0xFF8000u, 0x808080u
};

/* ★ Tabla de claves vistas, para dar UNA ranura de color estable a cada
 * combinador distinto.
 *
 * No vale el indice de `s_cache`: en modo `combinador` el camino normal no se
 * recorre -- se liga el shader plano y se sale --, asi que la cache no se
 * llena nunca y todo saldria del mismo color.  Esta tabla se llena desde el
 * propio modo.
 *
 * No se reinicia: el color de un combinador es el mismo toda la sesion, que es
 * lo que permite comparar dos fotos de la tele.  Por encima de la capacidad
 * devuelve la ultima ranura -- un color repetido, no un desbordamiento. */
static GXD3DTevKey s_marcaKeys[GXD3D_MARCA_COLORES];
static int         s_marcaKeyN = 0;

static int gxd3d_marca_ranura(const GXD3DTevKey *k)
{
    int i;
    for (i = 0; i < s_marcaKeyN; i++)
        if (memcmp(&s_marcaKeys[i], k, sizeof(*k)) == 0) return i;
    if (s_marcaKeyN < GXD3D_MARCA_COLORES)
    {
        s_marcaKeys[s_marcaKeyN] = *k;
        return s_marcaKeyN++;
    }
    return GXD3D_MARCA_COLORES - 1;
}

/* ★★★ MODO 3: VER SHADE TAL CUAL, SIN TEXTURA NI COMBINADOR.
 *
 * Saca el color de vertice interpolado y nada mas.  Es la medida que faltaba:
 * `[GXRASC]` informa el MAXIMO por canal, asi que su `rgba=255,255,255,255`
 * solo dice que ALGUN vertice fue blanco -- y en el mismo log `[GLN64LUZ]`
 * trae `cero=10110`, o sea que otros llegan negros.  Un maximo no puede
 * separar "esta pared llega negra" de "hay una pared blanca en otro sitio".
 *
 * Con esto se ve directamente: si las paredes salen NEGRAS aqui, SHADE es el
 * culpable y los combinadores 1 y 3 (los unicos que multiplican por RASC) son
 * el mecanismo.  Si salen blancas, SHADE esta bien y hay que mirar la
 * traduccion del combinador. */
static IDirect3DPixelShader9 *s_psShade = NULL;

/* ★★★ MODO 4: VER LO QUE EL SAMPLER LEE DE VERDAD.
 *
 * `return tex2D(uTex0, IN.uv0)`, sin combinador, sin SHADE y sin `clip`.
 *
 * ★ POR QUE NO BASTABA CON LOS BMP.  El volcado de L3+R3 escribe `tmp`, que
 * es el buffer LINEAL DE CPU recien destilado, y salio sano (90 de 91 con
 * contenido, 82 opacas del todo).  Pero entre `tmp` y el sampler quedan la
 * copia fila a fila al `LockRect` y el formato/tileado de la textura: el BMP
 * demuestra que la CONVERSION es correcta, no que la GPU lea eso.  Esta es la
 * unica sonda que mira el otro lado de esa frontera.
 *
 * Lo que lo hizo necesario: en modo `combinador` la geometria negra salio
 * ROJA a pantalla completa, con el mismo estado de profundidad, mezcla y
 * recorte.  O sea que esos dibujos SI llegan al framebuffer y SI escriben, y
 * lo unico que cambia entre "rojo" y "negro" es el pixel shader. */
static IDirect3DPixelShader9 *s_psTex = NULL;

static void gxd3d_build_tex(void)
{
    static const char *src =
        "sampler2D uTex0 : register(s0);\n"
        "struct PS_IN { float2 uv0:TEXCOORD0; float2 uv1:TEXCOORD1;\n"
        "               float2 uv2:TEXCOORD2; float4 col:COLOR0;\n"
        "               float4 pos:TEXCOORD3; };\n"
        "float4 main(PS_IN IN) : COLOR\n"
        "{ return float4(tex2D(uTex0, IN.uv0).rgb, 1.0f); }\n";
    LPD3DXBUFFER code = NULL, err = NULL;
    if (s_psTex) return;
    if (SUCCEEDED(D3DXCompileShader(src, (UINT)strlen(src), NULL, NULL,
                                    "main", "ps_3_0", 0, &code, &err, NULL)))
    {
        g_pd3dDevice->CreatePixelShader((const DWORD *)code->GetBufferPointer(),
                                        &s_psTex);
        code->Release();
    }
#if XBOX64_TRAZAS
    else
    {
        DebugOutputString("[MARCA] no se pudo compilar el shader de TEXTURA");
    }
#endif
    if (err) err->Release();
}

/* [Mario Tennis, bob-ombs y estela] MODO 21: el ALFA de lo que lee el sampler,
 * en gris y opaco (negro = alfa 0, blanco = alfa 1).  El modo 4 tira el alfa,
 * y aqui la pregunta es justo esa: si el fondo de un framebuffer auxiliar
 * usado como textura llega transparente o no. */
static IDirect3DPixelShader9 *s_psTexAlfa = NULL;
static u8 s_texEsFb[2] = { 0, 0 };   /* gxd3d_tev_tex_esfb */

static void gxd3d_build_texalfa(void)
{
    static const char *src =
        "sampler2D uTex0 : register(s0);\n"
        "struct PS_IN { float2 uv0:TEXCOORD0; float2 uv1:TEXCOORD1;\n"
        "               float2 uv2:TEXCOORD2; float4 col:COLOR0;\n"
        "               float4 pos:TEXCOORD3; };\n"
        "float4 main(PS_IN IN) : COLOR\n"
        "{ float a = tex2D(uTex0, IN.uv0).a; return float4(a, a, a, 1.0f); }\n";
    LPD3DXBUFFER code = NULL, err = NULL;
    if (s_psTexAlfa) return;
    if (SUCCEEDED(D3DXCompileShader(src, (UINT)strlen(src), NULL, NULL,
                                    "main", "ps_3_0", 0, &code, &err, NULL)))
    {
        g_pd3dDevice->CreatePixelShader((const DWORD *)code->GetBufferPointer(),
                                        &s_psTexAlfa);
        code->Release();
    }
#if XBOX64_TRAZAS
    else
    {
        DebugOutputString("[MARCA] no se pudo compilar el shader de ALFA DE TEXTURA");
    }
#endif
    if (err) err->Release();
}

static void gxd3d_build_shade(void)
{
    static const char *src =
        "struct PS_IN { float2 uv0:TEXCOORD0; float2 uv1:TEXCOORD1;\n"
        "               float2 uv2:TEXCOORD2; float4 col:COLOR0;\n"
        "               float4 pos:TEXCOORD3; };\n"
        "float4 main(PS_IN IN) : COLOR { return float4(IN.col.rgb, 1.0f); }\n";
    LPD3DXBUFFER code = NULL, err = NULL;
    if (s_psShade) return;
    if (SUCCEEDED(D3DXCompileShader(src, (UINT)strlen(src), NULL, NULL,
                                    "main", "ps_3_0", 0, &code, &err, NULL)))
    {
        g_pd3dDevice->CreatePixelShader((const DWORD *)code->GetBufferPointer(),
                                        &s_psShade);
        code->Release();
    }
#if XBOX64_TRAZAS
    else
    {
        DebugOutputString("[MARCA] no se pudo compilar el shader de SHADE");
    }
#endif
    if (err) err->Release();
}

static void gxd3d_build_marca(void)
{
    /* El numero de registro se IMPRIME desde `TEVC_MARCA` en vez de escribirlo
     * a mano: si algun dia crece `TEVC_TOTAL`, el shader lo sigue solo.  Un
     * `register(c12)` literal se quedaria apuntando al hueco de otro. */
    char src[256];
    LPD3DXBUFFER code = NULL, err = NULL;
    if (s_psMarca) return;
    sprintf(src,
            "float4 cMarca : register(c%d);\n"
            "float4 main() : COLOR { return float4(cMarca.rgb, 1.0f); }\n",
            TEVC_MARCA);
    if (SUCCEEDED(D3DXCompileShader(src, (UINT)strlen(src), NULL, NULL,
                                    "main", "ps_3_0", 0, &code, &err, NULL)))
    {
        g_pd3dDevice->CreatePixelShader((const DWORD *)code->GetBufferPointer(),
                                        &s_psMarca);
        code->Release();
    }
#if XBOX64_TRAZAS
    else
    {
        DebugOutputString("[MARCA] no se pudo compilar el shader del marcado");
    }
#endif
    if (err) err->Release();
}

/* ★ Suciedad de las CONSTANTES, aparte de la del shader.
 *
 * Antes se subian los 12 float4 en CADA draw, con 32 divisiones por 255 de
 * propina.  Con 18.324 draws/s medidos en Conker eso es medio millon de
 * divisiones por segundo y 18.324 `SetPixelShaderConstantF` que casi siempre
 * mandan lo mismo.  Los colores del TEV cambian mucho menos que los draws:
 * `GX_SetTevColor`/`GX_SetTevKColor` y compania marcan esto, y solo entonces
 * se sube. */
static int s_constDirty = 1;

/* GXD3D_SetRuido: uRuido (c15) = (semilla, 1/escX, 1/escY, 0).  Viaja con las
 * demas constantes del TEV, cuando estan sucias. */
static float s_ruido[4] = { 0.0f, 1.0f, 1.0f, 0.0f };

/* GXD3D_SetLod: uLod0 (c16) = (kS, kT, minLod, maxTile) y uLod1 (c17) =
 * (detalle, activo, 0, 0).  Viajan igual que s_ruido. */
static float s_lod[8] = { 1.0f, 1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f, 0.0f };

/* GXD3D_SetLodAtlas: uAtlas0 (c18) = (1/atlasW, 1/atlasH, realW1, realH1) y
 * uAtlas1 (c19) = (bilineal, nivMax, clampS, clampT).  `s_atlasOn` entra en la
 * clave (k->atlas) de los shaders con LOD_FRACTION. */
static float s_atlas[8] = { 1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 0.0f, 0.0f };
static u8    s_atlasOn  = 0;

void gxd3d_tev_dirty(void)       { s_dirty = 1; s_constDirty = 1; }
void gxd3d_tev_const_dirty(void) { s_constDirty = 1; }

static void gxd3d_build_key(GXD3DTevKey *k)
{
    int i;
    memset(k, 0, sizeof(*k));
    k->numStages  = g_gxTev.numStages ? g_gxTev.numStages : 1;
    k->numTexGens = g_gxTev.numTexGens;
    if (k->numStages > GXD3D_MAX_STAGES) k->numStages = GXD3D_MAX_STAGES;
    for (i = 0; i < k->numStages; i++)
    {
        const GXD3DTevStage *s = &g_gxTev.stage[i];
        k->st[i].texcoord = s->texcoord;
        k->st[i].color    = s->color;
        k->st[i].texmap   = (u8)s->texmap;
        memcpy(k->st[i].cin, s->cin, 4);
        memcpy(k->st[i].ain, s->ain, 4);
        k->st[i].cop = s->cop; k->st[i].cbias = s->cbias;
        k->st[i].cscale = s->cscale; k->st[i].cclamp = s->cclamp; k->st[i].creg = s->creg;
        k->st[i].aop = s->aop; k->st[i].abias = s->abias;
        k->st[i].ascale = s->ascale; k->st[i].aclamp = s->aclamp; k->st[i].areg = s->areg;
        k->st[i].kcsel = s->kcsel; k->st[i].kasel = s->kasel;
    }
    k->acComp0 = g_gxTev.acComp0;
    k->acOp    = g_gxTev.acOp;
    k->acComp1 = g_gxTev.acComp1;
    /* ★★★ ESTAS DOS, AQUI ARRIBA Y NO AL FINAL, Y NO ES COSMETICA.
     *
     * Estaban DESPUES de los bloques de modo de diagnostico que hay abajo, y
     * eso PISABA lo que esos bloques ponen: el corte por etapas escribia
     * `k->fogType = GX_FOG_NONE` y esta linea se lo devolvia a su valor real
     * dos lineas mas tarde.  O sea que el corte nunca llego a correr sin
     * niebla aunque el codigo dijera que si.
     *
     * Regla: primero se copia el estado, DESPUES se aplican los overrides.
     * Al reves, un override es una linea muerta que no avisa. */
    k->ztOp    = g_gxTev.ztOp;
    k->fogType = g_gxTev.fogType;
    k->texMono[0] = g_gxTev.texMono[0] ? (g_gxTev.monoAlfaFija ? 2 : 1) : 0;
    k->texMono[1] = g_gxTev.texMono[1] ? (g_gxTev.monoAlfaFija ? 2 : 1) : 0;
    k->blFog      = g_gxTev.blFog;
    /* Solo cambia el codigo si hay niebla: sin ella no entra en la clave. */
    k->fogInv     = (k->fogType != GX_FOG_NONE) ? g_gxTev.fogInv : 0;
    k->zClamp     = g_gxTev.zClamp;
    k->rtz        = g_gxTev.rtz;
    k->decN64     = g_gxTev.decN64;
    k->nieblaZ    = g_gxTev.nieblaZ;
    /* El atlas solo cambia el codigo de los shaders que leen LOD_FRACTION: en
     * los demas no entra en la clave, para no duplicarlos. */
    if (s_atlasOn)
    {
        int e;
        for (i = 0; i < k->numStages && !k->atlas; i++)
            for (e = 0; e < 4; e++)
                if (k->st[i].cin[e] == (u8)GXD3D_CC_LODFRAC || k->st[i].ain[e] == (u8)GXD3D_CA_LODFRAC)
                    k->atlas = 1;
    }

    /* ★★★ MODO 5 DEL MARCADO: EL SHADER REAL, SIN EL `clip`.
     *
     * Los otros modos sustituyen el pixel shader entero, asi que prueban cada
     * entrada POR SEPARADO -- y las cuatro han salido bien: la textura se ve
     * completa en modo `textura`, SHADE sale blanco en `sombreado`, PRIM vale
     * 200,200,200 y ENV es cero (el sumando, inocuo).  Con esas entradas el
     * producto NO PUEDE ser negro, asi que falta algo que solo existe en el
     * shader real, y el `clip` es lo unico de esa lista.
     *
     * ★ Y se hace FORZANDO LA CLAVE, no parcheando el texto: `acComp0` ya
     * forma parte de `GXD3DTevKey`, asi que poner ALWAYS aqui genera y cachea
     * un shader DISTINTO con todo lo demas identico -- mismas etapas, mismas
     * constantes, mismo todo.  El camino normal no se toca y no hay que
     * vaciar la cache al encender o apagar el modo. */
    if (gln64_marcaluz == GLN64_MARCA_SINCLIP)
    {
        k->acComp0 = GX_ALWAYS;
        k->acComp1 = GX_ALWAYS;
    }

    /* ★★★ EL SHADER REAL SIN NIEBLA.
     *
     * `[GXFOG] on=150 off=270 col=000000 ini=0.1 fin=1.0`: la niebla esta
     * ENCENDIDA y su color es NEGRO.  Y el generador la aplica sobre `z/w`:
     *
     *     ff = saturate((z/w - ini) / (fin - ini));
     *     outc.rgb = lerp(outc.rgb, uFogColor.rgb, ff);
     *
     * `z/w` en D3D no es lineal -- se pega a 1 en cuanto te alejas un poco del
     * plano cercano--, asi que con el rango 0,1..1,0 leido en ESE espacio `ff`
     * vale casi 1 para casi todo y `lerp(color, NEGRO, 1)` es negro.
     *
     * ★ Y explica por que ninguna sonda lo vio: los modos que SUSTITUYEN el
     * pixel shader (`textura`, `sombreado`, `combinador`) no emiten niebla, y
     * por eso enseñaban la escena entera y sana; el shader generado si la
     * emite.  Las entradas estaban todas bien porque el defecto no esta en
     * ellas, esta DESPUES del combinador.
     *
     * Como `fogType` ya forma parte de la clave, ponerlo a NONE aqui genera un
     * shader aparte y el camino normal no se toca. */
    if (gln64_marcaluz == GLN64_MARCA_SINNIEBLA)
        k->fogType = GX_FOG_NONE;

    /* ★★★ EL CORTE POR ETAPAS: EL SHADER REAL, PARADO DONDE SE QUIERA.
     *
     * Es la forma sistematica de la pregunta "¿en que paso se va a negro?".
     * Los modos que sustituyen el shader entero prueban cada entrada FUERA de
     * su sitio; esto ejecuta el combinador de verdad, con sus constantes de
     * verdad, y solo lo para antes.
     *
     * Para el combinador negro de 007 (shader #14) la escalera queda asi:
     *
     *   etapa1 -> creg0 = SHADE
     *   etapa2 -> creg0 = SHADE * TEXEL0
     *   etapa3 -> creg0 = SHADE * TEXEL0 * PRIM
     *   etapa4 -> cprev = eso + ENV
     *   etapa5 -> el resultado final
     *
     * El primer corte que salga negro senala la etapa culpable, y sus cuatro
     * entradas ya estan impresas en `[GLN64TEV]`.
     *
     * Se quita tambien el descarte de alfa: un corte intermedio deja un alfa
     * que no es el final, y no tiene sentido filtrar por el. */
    k->corte = 0;

    /* [Doom 64, Conker] El volcado: el shader NORMAL entero, pero con un
     * centinela en la clave para que se genere aparte y se vuelque al log.
     * Ver GLN64_MARCA_VOLCADO en gln64_prof.h. */
    if (gln64_marcaluz == GLN64_MARCA_VOLCADO)
    {
        k->corte = 0xFCu;
        return;
    }

    /* ★★★ CORTE DE CERO ETAPAS: EL PREAMBULO DEL SHADER GENERADO, PERO
     * DEVOLVIENDO EL COLOR DE VERTICE Y NADA MAS.
     *
     * Es la biseccion del ultimo hueco.  Estas dos cosas dan resultados
     * distintos y son la MISMA cuenta:
     *
     *   `sombreado` (s_psShade)  `return float4(IN.col.rgb, 1)`   -> se ve
     *   `etapa1` en el mux negro `outc = saturate(rasc.rgb)`       -> NEGRO
     *
     * y ya esta medido que no es el color de vertice (media=134 de 255), ni
     * que el corte no llegue (`co=5` en etapa5), ni la cuenta (simulada en
     * CPU: daria 134).  Lo unico que queda distinto entre los dos shaders es
     * el PREAMBULO: `s_psShade` no declara ni samplers ni constantes, el
     * generado declara tres samplers y seis bloques.  Y la frontera VS<->PS
     * de Xenos es sensible a eso -- repatchea el VS para casarlo con el PS
     * (ver la nota de `s_vsSource` en gx_d3d9.cpp).
     *
     * Con esto: si `etapa0` se VE, el preambulo es inocente y el defecto esta
     * en el codigo de las etapas.  Si sale NEGRO con `s_psShade` viendose, el
     * defecto es el reparto de interpoladores y no el combinador. */
    /* ★★★ PINTAR EL FACTOR DE NIEBLA Y LA PROFUNDIDAD.
     *
     * `sinniebla` deja la escena perfecta, asi que el defecto es la niebla;
     * pero corregir `fz` a `2*z/w - 1` NO lo arregla, o sea que el mapeo
     * sigue mal.  Y ya he fallado varias veces deduciendo en que espacio
     * esta cada cosa, asi que aqui se MIRA:
     *
     *   `niebla` -> saca `ff` en gris.  Blanco entero = el factor vale 1 en
     *               toda la escena y el rango no sirve; degradado = casi.
     *               Negro = a esa geometria no se le aplica niebla.
     *   `prof`   -> saca `z/w` en tres escalas a la vez, porque en
     *               perspectiva casi todo se apelotona junto a 1 y un gris
     *               plano no distinguiria 0,9 de 0,999:
     *                 rojo  = z/w tal cual
     *                 verde = z/w reescalado a [0,90 .. 1]
     *                 azul  = z/w reescalado a [0,99 .. 1]
     *               El color que salga degradado dice DONDE vive la escena, y
     *               de ahi sale el mapeo correcto para `ini/fin`.
     *
     * Van por la CLAVE (centinelas en `corte`) para que se cacheen aparte y
     * no toquen el camino normal.  Y sin `clip`, que si no la comparacion de
     * alfa se comeria parte de la imagen justo cuando quiero verla entera. */
    if (gln64_marcaluz == GLN64_MARCA_NIEBLA ||
        gln64_marcaluz == GLN64_MARCA_PROF)
    {
        k->corte   = (gln64_marcaluz == GLN64_MARCA_NIEBLA) ? 0xFEu : 0xFDu;
        k->acComp0 = GX_ALWAYS;
        k->acComp1 = GX_ALWAYS;
        return;
    }

    if (gln64_marcaluz == GLN64_MARCA_ETAPA0)
    {
        k->corte     = 0xFFu;   /* centinela: cero etapas */
        k->numStages = 0;
        k->acComp0   = GX_ALWAYS;
        k->acComp1   = GX_ALWAYS;
        k->fogType   = GX_FOG_NONE;
        return;
    }

    if (gln64_marcaluz >= GLN64_MARCA_ETAPA1 &&
        gln64_marcaluz <= GLN64_MARCA_ETAPA5)
    {
        u8 n = (u8)(gln64_marcaluz - GLN64_MARCA_ETAPA1 + 1);
        if (n > k->numStages) n = k->numStages;
        if (n > 0)
        {
            k->corte      = n;
            k->numStages  = n;
            k->acComp0    = GX_ALWAYS;
            k->acComp1    = GX_ALWAYS;
            /* ★ Y LA NIEBLA FUERA, que la primera version no lo hacia y por
             * eso la sonda mintio: la niebla se aplica DESPUES del
             * combinador, asi que contaminaba todos los cortes por igual y
             * `etapa1` salia negro aunque su cuenta sea identica a la del
             * modo `sombreado`, que si se veia.  Un corte tiene que medir el
             * combinador y NADA MAS. */
            k->fogType    = GX_FOG_NONE;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Mapa de registros de constante del pixel shader                     */
/* ------------------------------------------------------------------ */
/*   c0..c3   reg[0..3]  (PREV, REG0, REG1, REG2) -- valores iniciales
 *   c4..c7   konst[0..3]
 *   c8       (ref0, ref1, 0, 0) del alpha compare
 *   c9       color de niebla
 *   c10      (startZ, endZ, nearZ, farZ) de la niebla
 *   c11      (zBias, 0, 0, 0) de la z texture
 *
 * ★ Cabe de sobra: el pixel shader de Xenos tiene 256 constantes float
 * (GPU_D3D_PIXEL_CONSTANTF_COUNT, d3d9gpu.h:203).  Pero OJO con el frontend:
 * los shaders de video de Salvia usan c1 para `textureDims`, y Rice ya se lo
 * piso una vez.  Aqui no hay conflicto porque el frontend reestablece su
 * etapa entera en XBOX_SetupQuadState antes de dibujar su quad
 * (ver project_xenon64_libretro_port). */
/* ★ El reparto de registros VIVE ARRIBA, junto al shader de rescate, no aqui:
 * `gxd3d_build_marca` imprime `TEVC_MARCA` (= `TEVC_TOTAL`) dentro del texto
 * de su shader, y esta mucho antes que este punto.  Un `#define` solo tiene
 * que existir antes de donde se USA, y ahi se usa. */

/* ------------------------------------------------------------------ */
/* Generacion del HLSL                                                 */
/* ------------------------------------------------------------------ */
/* Entradas de COLOR del TEV -> expresion HLSL float3. */
static const char *tev_cc(u8 sel)
{
    switch (sel)
    {
        case GX_CC_CPREV: return "cprev.rgb";
        case GX_CC_APREV: return "cprev.aaa";
        case GX_CC_C0:    return "creg0.rgb";
        case GX_CC_A0:    return "creg0.aaa";
        case GX_CC_C1:    return "creg1.rgb";
        case GX_CC_A1:    return "creg1.aaa";
        case GX_CC_C2:    return "creg2.rgb";
        case GX_CC_A2:    return "creg2.aaa";
        case GX_CC_TEXC:  return "texc.rgb";
        case GX_CC_TEXA:  return "texc.aaa";
        case GX_CC_RASC:  return "rasc.rgb";
        case GX_CC_RASA:  return "rasc.aaa";
        case GX_CC_ONE:   return "float3(1,1,1)";
        case GX_CC_HALF:  return "float3(0.5,0.5,0.5)";
        case GX_CC_KONST: return "konst.rgb";
        case GXD3D_CC_RUIDO: return "float3(ruido,ruido,ruido)";   /* GXD3D_SetRuido */
        case GXD3D_CC_LODFRAC: return "float3(lodFrac,lodFrac,lodFrac)";   /* GXD3D_SetLod */
        default:          return "float3(0,0,0)";   /* GX_CC_ZERO */
    }
}

/* Entradas de ALFA del TEV -> expresion HLSL float. */
static const char *tev_ca(u8 sel)
{
    switch (sel)
    {
        case GX_CA_APREV: return "cprev.a";
        case GX_CA_A0:    return "creg0.a";
        case GX_CA_A1:    return "creg1.a";
        case GX_CA_A2:    return "creg2.a";
        case GX_CA_TEXA:  return "texc.a";
        case GX_CA_RASA:  return "rasc.a";
        case GX_CA_KONST: return "konst.a";
        case GXD3D_CA_RUIDO: return "ruido";        /* GXD3D_SetRuido */
        case GXD3D_CA_LODFRAC: return "lodFrac";    /* GXD3D_SetLod */
        default:          return "0.0";             /* GX_CA_ZERO */
    }
}

static const char *tev_bias(u8 b)
{
    if (b == GX_TB_ADDHALF) return " + 0.5";
    if (b == GX_TB_SUBHALF) return " - 0.5";
    return "";
}

static float tev_scale(u8 s)
{
    switch (s)
    {
        case GX_CS_SCALE_2:  return 2.0f;
        case GX_CS_SCALE_4:  return 4.0f;
        case GX_CS_DIVIDE_2: return 0.5f;
        default:             return 1.0f;
    }
}

static const char *tev_regname_c(u8 r)
{
    switch (r) { case GX_TEVREG0: return "creg0"; case GX_TEVREG1: return "creg1";
                 case GX_TEVREG2: return "creg2"; default: return "cprev"; }
}

/* La seleccion de konst para COLOR.  Solo se implementan los selectores que
 * glN64_GX usa de verdad (K0..K3 y sus alfas replicados, mas la constante
 * 1.0); el resto cae al 1.0 y se cuenta, para que no se pierda en silencio. */
static void tev_kcsel(char *out, size_t n, u8 sel)
{
    switch (sel)
    {
        case GX_TEV_KCSEL_K0:   _snprintf(out, n, "uKonst[0].rgb"); break;
        case GX_TEV_KCSEL_K1:   _snprintf(out, n, "uKonst[1].rgb"); break;
        case GX_TEV_KCSEL_K2:   _snprintf(out, n, "uKonst[2].rgb"); break;
        case GX_TEV_KCSEL_K3:   _snprintf(out, n, "uKonst[3].rgb"); break;
        case GX_TEV_KCSEL_K0_A: _snprintf(out, n, "uKonst[0].aaa"); break;
        case GX_TEV_KCSEL_K1_A: _snprintf(out, n, "uKonst[1].aaa"); break;
        case GX_TEV_KCSEL_K2_A: _snprintf(out, n, "uKonst[2].aaa"); break;
        case GX_TEV_KCSEL_K3_A: _snprintf(out, n, "uKonst[3].aaa"); break;
        default:                _snprintf(out, n, "float3(1,1,1)"); break;
    }
}

static void tev_kasel(char *out, size_t n, u8 sel)
{
    switch (sel)
    {
        case GX_TEV_KASEL_K0_A: _snprintf(out, n, "uKonst[0].a"); break;
        case GX_TEV_KASEL_K1_A: _snprintf(out, n, "uKonst[1].a"); break;
        case GX_TEV_KASEL_K2_A: _snprintf(out, n, "uKonst[2].a"); break;
        case GX_TEV_KASEL_K3_A: _snprintf(out, n, "uKonst[3].a"); break;
        default:                _snprintf(out, n, "1.0");         break;
    }
}

/* Comparacion del alpha compare -> operador HLSL.  Devuelve la condicion de
 * que el pixel PASE. */
static const char *tev_cmp(u8 comp, const char *lhs, const char *rhs, char *buf, size_t n)
{
    const char *op;
    switch (comp)
    {
        case GX_NEVER:  _snprintf(buf, n, "false"); return buf;
        case GX_LESS:   op = "<";  break;
        case GX_EQUAL:  op = "=="; break;
        case GX_LEQUAL: op = "<="; break;
        case GX_GREATER:op = ">";  break;
        case GX_NEQUAL: op = "!="; break;
        case GX_GEQUAL: op = ">="; break;
        default:        _snprintf(buf, n, "true");  return buf;
    }
    _snprintf(buf, n, "(%s %s %s)", lhs, op, rhs);
    return buf;
}

#define HLSL_MAX 16384

/* `soloTexto` != NULL: no compila; deja ahi el HLSL (malloc, para TEV_ASYNC)
 * y devuelve NULL. */
static IDirect3DPixelShader9 *gxd3d_generate(const GXD3DTevKey *k, char **soloTexto)
{
    char *src = (char *)malloc(HLSL_MAX);
    char line[1024];
    char kbuf[64], kbuf2[64], cmp0[128], cmp1[128];
    int i;
    int usaRuido = 0;   /* alguna etapa es la entrada NOISE (GXD3D_SetRuido) */
    int usaLod = 0;     /* alguna etapa es la entrada LOD_FRACTION (GXD3D_SetLod) */
    size_t len = 0;
    IDirect3DPixelShader9 *ps = NULL;
    LPD3DXBUFFER code = NULL, err = NULL;

    if (!src) return NULL;
    for (i = 0; i < k->numStages; i++)
    {
        int e;
        for (e = 0; e < 4; e++)
            if (k->st[i].cin[e] == (u8)GXD3D_CC_RUIDO || k->st[i].ain[e] == (u8)GXD3D_CA_RUIDO)
                usaRuido = 1;
        for (e = 0; e < 4; e++)
            if (k->st[i].cin[e] == (u8)GXD3D_CC_LODFRAC || k->st[i].ain[e] == (u8)GXD3D_CA_LODFRAC)
                usaLod = 1;
    }
    src[0] = 0;

    #define EMIT(s) do { size_t _l = strlen(s); \
        if (len + _l < HLSL_MAX - 1) { memcpy(src + len, (s), _l); len += _l; src[len] = 0; } } while (0)

    EMIT("sampler2D uTex0 : register(s0);\n"
         "sampler2D uTex1 : register(s1);\n"
         "sampler2D uTex2 : register(s2);\n"
         "float4 uReg[4]   : register(c0);\n"
         "float4 uKonst[4] : register(c4);\n"
         "float4 uAcRef    : register(c8);\n"
         "float4 uFogColor : register(c9);\n"
         "float4 uFogPar   : register(c10);\n"
         "float4 uZTex     : register(c11);\n"
         "float4 uBlFog    : register(c12);\n"
         "struct PS_IN {\n"
         "  float2 uv0 : TEXCOORD0;\n"
         "  float2 uv1 : TEXCOORD1;\n"
         "  float2 uv2 : TEXCOORD2;\n"
         "  float4 col : COLOR0;\n"
         "  float4 pos : TEXCOORD3;\n"   /* posicion de recorte, para la z       */
         "  float  fogc : TEXCOORD4;\n"  /* coeficiente de niebla, 0..255        */
         "};\n");

    /* GXD3D_SetRenderToDepth modo 2 y GXD3D_SetDecalN64: la copia de Z y su
     * tamano.  Solo en estos shaders, para no tocar el preambulo de los
     * demas. */
    if ((k->rtz >= 2 && k->rtz <= 4) || k->decN64 || k->nieblaZ)
    {
        _snprintf(line, sizeof(line),
                  "sampler2D uZCopia : register(s%d);\n"
                  "float4 uZCopiaInv : register(c%d);\n",
                  GXD3D_ZCOPIA_SAMPLER, GXD3D_ZCOPIA_CONST);
        line[sizeof(line) - 1] = 0;
        EMIT(line);
    }
    /* [Zelda MM] GXD3D_SetRenderToDepth modo 4: la copia del Z actual. */
    if (k->rtz == 4)
    {
        _snprintf(line, sizeof(line), "sampler2D uZActual : register(s%d);\n",
                  GXD3D_ZACTUAL_SAMPLER);
        line[sizeof(line) - 1] = 0;
        EMIT(line);
    }
    /* GXD3D_SetNieblaZ: la curva de la niebla, 256x1, y el paso al Z del N64. */
    if (k->nieblaZ)
    {
        _snprintf(line, sizeof(line), "sampler2D uNieblaZ : register(s%d);\n"
                                      "float4 uNieblaZPar : register(c%d);\n",
                  GXD3D_NIEBLAZ_SAMPLER, GXD3D_NIEBLAZ_CONST);
        line[sizeof(line) - 1] = 0;
        EMIT(line);
    }
    /* [Resident Evil 2] GXD3D_SetRenderToDepth modo 5: el paso del Z del N64 al
     * del EFB.  Comparte registro con uNieblaZPar (Beetle): los dos no van
     * nunca en el mismo shader (ver el final). */
    else if (k->rtz == 5)
    {
        _snprintf(line, sizeof(line), "float4 uRe2Z : register(c%d);\n",
                  GXD3D_RE2Z_CONST);
        line[sizeof(line) - 1] = 0;
        EMIT(line);
    }

    /* La entrada NOISE: su constante, solo en estos shaders. */
    if (usaRuido)
    {
        _snprintf(line, sizeof(line), "float4 uRuido : register(c%d);\n",
                  GXD3D_RUIDO_CONST);
        line[sizeof(line) - 1] = 0;
        EMIT(line);
    }

    /* La entrada LOD_FRACTION: sus dos constantes, solo en estos shaders. */
    if (usaLod)
    {
        _snprintf(line, sizeof(line), "float4 uLod0 : register(c%d);\n"
                                      "float4 uLod1 : register(c%d);\n",
                  GXD3D_LOD_CONST, GXD3D_LOD_CONST + 1);
        line[sizeof(line) - 1] = 0;
        EMIT(line);
    }

    /* [Xbox360] ★ EL ATLAS DE MIPMAP: TextureMipMap1 de GLideN64
     * (glsl_CombinerProgramBuilderAccurate.cpp:933-975).  El formato lo
     * escribe TextureCache_LoadAtlas (glN64_GX/Textures.cpp): 256 de ancho, la
     * cabecera del nivel n en el texel n (R,G = desplazamiento, B = log2 del
     * ancho y del alto, A = ratioS y ratioT) y los niveles en fila.
     * Como GLideN64: las cuatro esquinas se calculan en el NIVEL 0 (el tile
     * 1, IN.uv1), se les da la vuelta o el clamp ahi, y se escalan al nivel n
     * con lod_scale = 2^(ratio0 - ratioN).  El filtro, el normal de 4
     * muestras (GLideN64 tiene tambien el de 3 puntos). */
    if (usaLod && k->atlas == 1)
    {
        _snprintf(line, sizeof(line), "float4 uAtlas0 : register(c%d);\n"
                                      "float4 uAtlas1 : register(c%d);\n",
                  GXD3D_ATLAS_CONST, GXD3D_ATLAS_CONST + 1);
        line[sizeof(line) - 1] = 0;
        EMIT(line);
        EMIT("float4 atlasTexel(float off) {\n"
             "  float y = floor(off / 256.0);\n"
             "  float x = off - 256.0 * y;\n"
             "  return tex2Dlod(uTex1, float4((x + 0.5) * uAtlas0.x, (y + 0.5) * uAtlas0.y, 0, 0));\n"
             "}\n"
             "float4 atlasCab(float n) {\n"
             "  return floor(tex2Dlod(uTex1, float4((n + 0.5) * uAtlas0.x, 0.5 * uAtlas0.y, 0, 0)) * 255.0 + 0.5);\n"
             "}\n"
             "float2 atlasNib(float b) {\n"
             "  float h = floor(b / 16.0);\n"
             "  return float2(h, b - 16.0 * h);\n"
             "}\n"
             "float2 atlasEnvuelve(float2 p, float2 dim) {\n"
             "  return lerp(p - dim * floor(p / dim), clamp(p, 0.0, dim - 1.0), uAtlas1.zw);\n"
             "}\n"
             "float4 atlasNivel(float2 uv, float n) {\n"
             "  float4 c0 = atlasCab(0.0);\n"
             "  float4 cn = atlasCab(n);\n"
             "  float2 esc = exp2(atlasNib(c0.a) - atlasNib(cn.a));\n"
             "  float off = cn.r + cn.g * 256.0;\n"
             "  float2 dimN = exp2(atlasNib(cn.b));\n"
             "  float2 dim0 = uAtlas0.zw;\n"
             "  float2 tc = uv * dim0 - 0.5 * uAtlas1.x;\n"
             "  float2 b = floor(tc);\n"
             "  float2 f = (tc - b) * uAtlas1.x;\n"
             "  float2 p0 = atlasEnvuelve(b, dim0);\n"
             "  float2 p1 = atlasEnvuelve(b + 1.0, dim0);\n"
             "  float2 q0 = min(floor(p0 * esc), dimN - 1.0);\n"
             "  float2 q1 = min(floor(p1 * esc), dimN - 1.0);\n"
             "  float4 c00 = atlasTexel(off + q0.y * dimN.x + q0.x);\n"
             "  float4 c10 = atlasTexel(off + q0.y * dimN.x + q1.x);\n"
             "  float4 c01 = atlasTexel(off + q1.y * dimN.x + q0.x);\n"
             "  float4 c11 = atlasTexel(off + q1.y * dimN.x + q1.x);\n"
             "  return lerp(lerp(c00, c10, f.x), lerp(c01, c11, f.x), f.y);\n"
             "}\n");
    }

    /* La firma: con salida de profundidad o sin ella, y con VPOS si se lee la
     * copia de Z o hay ruido. */
    {
        const int conVpos = ((k->rtz >= 2 && k->rtz <= 4) || k->decN64 || k->nieblaZ || usaRuido) ? 1 : 0;
        const int conProf = (k->ztOp == GX_ZT_REPLACE || k->zClamp || k->rtz) ? 1 : 0;
        if (conProf)
            EMIT("struct PS_OUT { float4 c : COLOR; float d : DEPTH; };\n");
        EMIT(conProf ? "PS_OUT main(PS_IN IN" : "float4 main(PS_IN IN");
        if (conVpos)
            EMIT(", float2 vpos : VPOS");
        EMIT(conProf ? ") {\n  PS_OUT o;\n" : ") : COLOR {\n");
    }

    EMIT("  float4 cprev = uReg[0];\n"
         "  float4 creg0 = uReg[1];\n"
         "  float4 creg1 = uReg[2];\n"
         "  float4 creg2 = uReg[3];\n"
         "  float4 rasc  = IN.col;\n");

    /* NOISE como GLideN64: `0.5 + 0.5*snoise()` (glsl_CombinerProgramBuilder.cpp:41
     * y :67), con snoise() = hash13 de (semilla, pixel del N64)
     * (glsl_CombinerProgramBuilderCommon.cpp:1011-1019).  Su `gl_FragCoord /
     * uScreenScale` es aqui `vpos * (1/esc)`. */
    if (usaRuido)
        EMIT("  float ruido;\n"
             "  {\n"
             "    float2 rc = floor(vpos * uRuido.yz);\n"
             "    float3 p3 = frac(float3(uRuido.x, rc.x, rc.y) * 0.1031);\n"
             "    p3 += dot(p3, p3.zyx + 31.32);\n"
             "    ruido = 0.5 + 0.5 * frac((p3.x + p3.y) * p3.z);\n"
             "  }\n");

    /* [Xbox360] ★ LOD_FRACTION COMO GLideN64: la funcion mipmap() de
     * glsl_CombinerProgramBuilderAccurate.cpp, rama sin atlas (`uNoAtlasTex`),
     * en su version ACTUAL (la LOD con dFdx/dFdy entro en 407edd8e).  `IN.uv0`
     * va normalizada y con el shift del tile: uLod0.xy la
     * pasa a texels del N64 por pixel del N64 (su vLodTexCoord * uScreenScale).
     *   lod       = max(|ddx|, |ddy|) en texels por pixel del N64
     *   lod_clamp = clamp(lod, min_lod, 2^max_tile - 1/32)
     *   lod_tile  = clamp(int(log2(lod_clamp)), 0, max_tile)
     *   lod_frac  = lod_clamp / 2^lod_tile; si sharpen o lod_clamp >= 1,
     *               clamp(lod_frac - 1, -1, 1)
     * `lodMismo`: con el LOD activo, modo normal y lod_clamp < 1, GLideN64 lee
     * el tile 0 en los DOS texel (tile0 = tile1 = 0); aqui la etapa que lee el
     * texmap 1 lee el 0.  Con mas de dos niveles GLideN64 usa un atlas: si el
     * plugin lo ha armado (k->atlas, GXD3D_SetLodAtlas) se eligen los tiles
     * como alli; si no, se quedan los tiles 0 y 1 (los mas detallados).
     * Confirmado (Logs Salvia (14), sonda [LODSH] retirada): el cuadro de
     * Peach a Bowser de SM64 ya se transforma (3 etapas, maxTile 1, k 64). */
    if (usaLod && k->atlas == 1)
        EMIT("  float4 lodT0, lodT1;\n");
    if (usaLod)
        EMIT("  float lodFrac;\n"
             "  bool lodMismo;\n"
             "  {\n"
             "    float2 ldx = abs(ddx(IN.uv0)) * uLod0.xy;\n"
             "    float2 ldy = abs(ddy(IN.uv0)) * uLod0.xy;\n"
             "    float lod = max(max(ldx.x, ldx.y), max(ldy.x, ldy.y));\n"
             "    float maxTile = min((uLod1.x != 2.0) ? 7.0 : 6.0, uLod0.w);\n"
             "    float minLod = (uLod1.x != 0.0) ? uLod0.z : 1.0;\n"
             "    float maxLod = exp2(maxTile) - 1.0 / 32.0;\n"
             "    float lodClamp = min(max(lod, minLod), maxLod);\n"
             "    float lodTile = min(max(trunc(log2(lodClamp)), 0.0), maxTile);\n"
             "    lodFrac = lodClamp / exp2(lodTile);\n"
             "    if (uLod1.x == 1.0 || lodClamp >= 1.0)\n"
             "      lodFrac = clamp(lodFrac - 1.0, -1.0, 1.0);\n"
             "    lodMismo = (uLod1.y != 0.0) && (lodClamp < 1.0) && (uLod1.x == 0.0);\n");
    /* [Xbox360] Con atlas, la eleccion de tiles ENTERA de mipmap(): el tile 0
     * es uTex0 y el tile n >= 1, el nivel n - 1 del atlas (TextureMipMap1).
     * Las etapas que leen el texmap 0 y el 1 toman lodT0 y lodT1, que son el
     * readtex0 y readtex1 de GLideN64. */
    if (usaLod && k->atlas == 1)
        EMIT("    float t0 = 0.0, t1 = 1.0;\n"
             "    float4 lt0 = tex2D(uTex0, IN.uv0);\n"
             "    if (uLod1.y != 0.0) {\n"
             "      if (lodClamp < 1.0 && uLod1.x == 0.0) { t0 = 0.0; t1 = 0.0; }\n"
             "      else if (lodClamp >= 1.0 && uLod1.x == 2.0) { t0 = lodTile + 1.0; t1 = lodTile + 2.0; }\n"
             "      else { t0 = lodTile; t1 = lodTile + 1.0; }\n"
             "    }\n"
             "    float4 la0 = atlasNivel(IN.uv1, min(max(t0 - 1.0, 0.0), uAtlas1.y));\n"
             "    float4 la1 = atlasNivel(IN.uv1, min(max(t1 - 1.0, 0.0), uAtlas1.y));\n"
             "    lodT0 = (t0 == 0.0) ? lt0 : la0;\n"
             "    lodT1 = (t1 == 0.0) ? lt0 : la1;\n");
    if (usaLod)
        EMIT("  }\n");

#if TEV_BISECT == 1
    /* ★ HONESTO CON LOS DRAWS SIN TEXTURA.
     *
     * La primera version hacia `tex2D(uTex0, IN.uv0)` SIEMPRE, tambien en los
     * draws cuya etapa 0 no tiene texmap -- y esos muestrean el sampler que
     * dejo el draw ANTERIOR, o sea un color arbitrario y estable.  En la
     * captura de SM64 eso salio como rectangulos azules grandes repartidos de
     * arriba abajo, y me los llevo a buscar un defecto de profundidad que
     * quiza no existe: eran artefacto de la propia sonda.
     *
     * Ahora esos draws salen MAGENTA, que no aparece en SM64 y no se puede
     * confundir con contenido real.  Si los rectangulos siguen siendo azules,
     * son geometria texturizada de verdad y el defecto esta en otro sitio;
     * si salen magenta, eran esto. */
    if (k->st[0].texmap == (u8)GX_TEXMAP_NULL || k->numTexGens == 0)
        EMIT("  return float4(1,0,1,1);\n}\n");
    else
        EMIT("  return tex2D(uTex0, IN.uv0);\n}\n");
    goto compile;
#elif TEV_BISECT == 2
    EMIT("  return IN.col;\n}\n");
    goto compile;
#elif TEV_BISECT == 3
    EMIT("  float4 t = tex2D(uTex0, IN.uv0); return float4(t.a,t.a,t.a,1);\n}\n");
    goto compile;
#elif TEV_BISECT == 4
    /* ★ Las UV0 como color, con `frac` para que se vea el CICLO.
     *
     * Contesta de un vistazo la pregunta que las cuatro hipotesis anteriores
     * no contestaron: si una superficie sale con un degradado suave rojo/verde
     * es que sus UV estan bien y el defecto es la textura o el combinador; si
     * sale con el degradado repetido muchas veces, las UV vienen sin
     * normalizar; y si sale de un color plano, las UV son constantes (que es
     * lo que se espera si `combiner.usesT0` es falso y el plugin emite (0,0)
     * en OpenGL.cpp:1406-1409). */
    EMIT("  return float4(frac(IN.uv0.x), frac(IN.uv0.y), 0, 1);\n}\n");
    goto compile;
#endif

    for (i = 0; i < k->numStages; i++)
    {
        const char *a, *b, *c, *d;
        float cs, as;

        /* ★ CADA ETAPA EN SU PROPIO AMBITO.
         *
         * Sin las llaves, las etapas declaran `texc`, `konst`, `cmix`,
         * `cres`, `amix` y `ares` en el MISMO ambito y a partir de la
         * segunda el compilador rechaza el shader entero:
         *
         *     error X3003: redefinition of 'texc'
         *
         * Y el sintoma era PANTALLA NEGRA con el emulador dibujando: el
         * shader salia NULL, `SetPixelShader(NULL)` y en Xenos -- que no
         * tiene funcion fija-- el draw no produce nada.  Se veia en
         * `[GLN64] shgen=1 shfail=9`: solo compilaba la configuracion de UNA
         * etapa.
         *
         * Los registros del TEV (`cprev`, `creg0..2`) se declaran FUERA, asi
         * que lo que una etapa escribe lo sigue viendo la siguiente, que es
         * justo la semantica del TEV. */
        _snprintf(line, sizeof(line), "  // --- etapa %d ---\n  {\n", i);
        EMIT(line);

        /* Muestreo de la textura de esta etapa. */
        if (k->st[i].texmap == (u8)GX_TEXMAP_NULL || k->numTexGens == 0)
            EMIT("  float4 texc = float4(1,1,1,1);\n");
        else
        {
            /* ★ AQUI SE PROBO `hack_fbTextureOffset` DE GLideN64 -- restar un
             * texel a S y T al muestrear una textura de framebuffer con
             * filtro distinto de POINT-- y NO TUVO NINGUN EFECTO sobre los
             * dientes de sierra del fondo de pausa.  Revertido.
             *
             * Explicacion probable de por que no cambio nada: solo se aplica
             * si el filtro no es POINT, y estas texturas pueden estar
             * ligandose con POINT -- en cuyo caso GLideN64 tampoco lo
             * aplicaria y los dientes de sierra tienen otra causa.  Si se
             * retoma, lo primero es medir con que filtro se ligan. */
            const char *uv = (k->st[i].texcoord == GX_TEXCOORD1) ? "IN.uv1"
                           : (k->st[i].texcoord == 2)            ? "IN.uv2"
                                                                 : "IN.uv0";
            /* [Xbox360] Con LOD_FRACTION, el texmap 1 es el tile 0 cuando
             * GLideN64 pondria tile1 = 0 (`lodMismo`, ver arriba). */
            if (usaLod && k->atlas == 1 && (k->st[i].texmap & 3) <= 1)
                _snprintf(line, sizeof(line), "  float4 texc = lodT%d;\n",
                          (int)(k->st[i].texmap & 3));
            /* El respaldo de TEV_ASYNC: el tile 0 en los dos (la unidad 1
             * tiene el atlas, no el tile 1). */
            else if (usaLod && k->atlas == 2 && (k->st[i].texmap & 3) <= 1)
                _snprintf(line, sizeof(line), "  float4 texc = tex2D(uTex0, IN.uv0);\n");
            else if (usaLod && (k->st[i].texmap & 3) == 1)
                _snprintf(line, sizeof(line),
                          "  float4 texc = lodMismo ? tex2D(uTex0, IN.uv0) : tex2D(uTex1, %s);\n", uv);
            else
                _snprintf(line, sizeof(line), "  float4 texc = tex2D(uTex%d, %s);\n",
                          (int)(k->st[i].texmap & 3), uv);
            EMIT(line);

            /* Framebuffer de 8 bits: un solo canal, en los cuatro (GLideN64
             * readTex: `if (fbMonochrome == 1) texColor = vec4(texColor.r)`,
             * `if (fbFixedAlpha == 1) texColor.a = 0.825`). */
            if (k->st[i].texmap < 2 && k->texMono[k->st[i].texmap] != 0)
            {
                EMIT("  texc = texc.rrrr;\n");
                if (k->texMono[k->st[i].texmap] == 2)
                    EMIT("  texc.a = 0.825;\n");
            }
        }

        /* konst de esta etapa */
        tev_kcsel(kbuf,  sizeof(kbuf),  k->st[i].kcsel);
        tev_kasel(kbuf2, sizeof(kbuf2), k->st[i].kasel);
        _snprintf(line, sizeof(line), "  float4 konst = float4(%s, %s);\n", kbuf, kbuf2);
        EMIT(line);

        /* --- canal de COLOR --- */
        a = tev_cc(k->st[i].cin[0]); b = tev_cc(k->st[i].cin[1]);
        c = tev_cc(k->st[i].cin[2]); d = tev_cc(k->st[i].cin[3]);
        cs = tev_scale(k->st[i].cscale);
        _snprintf(line, sizeof(line),
            "  float3 cmix = lerp(%s, %s, %s);\n", a, b, c);
        EMIT(line);
        _snprintf(line, sizeof(line),
            "  float3 cres = ((%s %s cmix)%s) * %.1f;\n",
            d, (k->st[i].cop == GX_TEV_SUB) ? "-" : "+",
            tev_bias(k->st[i].cbias), cs);
        EMIT(line);
        /* GXD3D_TEV_WRAP_*: el envuelto de GLideN64 (gccore.h).  `mod` de GLSL
         * es x - y*floor(x/y), no el fmod de HLSL (que trunca hacia cero). */
        if (k->st[i].cclamp == GXD3D_TEV_WRAP_C)
            EMIT("  cres = (cres + 1.01) - 2.02 * floor((cres + 1.01) / 2.02) - 1.01;\n");
        else if (k->st[i].cclamp == GXD3D_TEV_WRAP_ABD)
            EMIT("  cres = (cres + 0.51) - 2.02 * floor((cres + 0.51) / 2.02) - 0.51;\n");
        else if (k->st[i].cclamp) EMIT("  cres = saturate(cres);\n");

        /* --- canal de ALFA --- */
        a = tev_ca(k->st[i].ain[0]); b = tev_ca(k->st[i].ain[1]);
        c = tev_ca(k->st[i].ain[2]); d = tev_ca(k->st[i].ain[3]);
        as = tev_scale(k->st[i].ascale);
        _snprintf(line, sizeof(line),
            "  float amix = lerp(%s, %s, %s);\n", a, b, c);
        EMIT(line);
        _snprintf(line, sizeof(line),
            "  float ares = ((%s %s amix)%s) * %.1f;\n",
            d, (k->st[i].aop == GX_TEV_SUB) ? "-" : "+",
            tev_bias(k->st[i].abias), as);
        EMIT(line);
        if (k->st[i].aclamp == GXD3D_TEV_WRAP_C)
            EMIT("  ares = (ares + 1.01) - 2.02 * floor((ares + 1.01) / 2.02) - 1.01;\n");
        else if (k->st[i].aclamp == GXD3D_TEV_WRAP_ABD)
            EMIT("  ares = (ares + 0.51) - 2.02 * floor((ares + 0.51) / 2.02) - 0.51;\n");
        else if (k->st[i].aclamp) EMIT("  ares = saturate(ares);\n");

        /* Escritura al registro destino.  OJO: color y alfa pueden ir a
         * registros DISTINTOS, por eso se escriben por separado. */
        _snprintf(line, sizeof(line), "  %s.rgb = cres;\n", tev_regname_c(k->st[i].creg));
        EMIT(line);
        _snprintf(line, sizeof(line), "  %s.a = ares;\n",   tev_regname_c(k->st[i].areg));
        EMIT(line);
        EMIT("  }\n");   /* cierra el ambito de la etapa */
    }

    /* ★ Con corte, la salida es el registro DESTINO de la ultima etapa
     * emitida, no `cprev`.  Es imprescindible: en el combinador negro de 007
     * las tres primeras etapas escriben en `creg0` y `cprev` no se toca hasta
     * la cuarta, asi que devolver `cprev` en un corte temprano mostraria el
     * valor INICIAL del registro y no el resultado de la etapa.  Seria una
     * sonda que mide otra cosa y lo parece todo menos eso. */
    if (k->corte == 0xFFu)
    {
        /* Cero etapas: el color de vertice tal cual, con el preambulo entero
         * del shader generado.  Ver la nota de `GLN64_MARCA_ETAPA0`. */
        EMIT("  float4 outc = float4(rasc.rgb, 1.0);\n");
    }
    else if (k->corte && k->corte != 0xFCu && k->numStages > 0)
    {
        _snprintf(line, sizeof(line), "  float4 outc = %s;\n",
                  tev_regname_c(k->st[k->numStages - 1].creg));
        EMIT(line);
        EMIT("  outc.a = 1.0;\n");
    }
    else
        EMIT("  float4 outc = cprev;\n");

    /* --- alpha compare ---
     * GX hace DOS comparaciones combinadas con AND/OR/XOR/XNOR; D3D9 solo
     * tiene una, asi que se resuelve aqui con `clip()`.
     * ★ `clip()` mata el early-Z de Xenos, que es justo lo que GX pedia con
     * GX_SetZCompLoc(GX_FALSE).  O sea que el comportamiento sale por
     * construccion y no hace falta un render state equivalente. */
    if (!(k->acComp0 == GX_ALWAYS && k->acComp1 == GX_ALWAYS))
    {
        const char *join;
        tev_cmp(k->acComp0, "outc.a", "uAcRef.x", cmp0, sizeof(cmp0));
        tev_cmp(k->acComp1, "outc.a", "uAcRef.y", cmp1, sizeof(cmp1));
        switch (k->acOp)
        {
            case GX_AOP_OR:   join = "||"; break;
            case GX_AOP_XOR:  join = "!="; break;
            case GX_AOP_XNOR: join = "=="; break;
            default:          join = "&&"; break;   /* GX_AOP_AND */
        }
        _snprintf(line, sizeof(line),
            "  clip( (%s %s %s) ? 1.0 : -1.0 );\n", cmp0, join, cmp1);
        EMIT(line);
    }

    /* --- niebla ---
     * ★ El factor va POR PIXEL desde z/w, no por vertice.  Calcularlo por
     * vertice fue lo que dejo la niebla mal en el port de Rice hasta que se
     * corrigio siguiendo a glN64_GX (ver project_xenon64_niebla). */
    if (k->fogType != GX_FOG_NONE)
    {
        /* ★★★ `2*z/w - 1`, Y ESE `-1` ES EL ARREGLO DE 007 TWINE.
         *
         * `uFogPar.x/.y` vienen de glN64_GX (gSP.cpp:1792):
         *
         *     GXfogStartZ = -(fog.offset / fog.multiplier)
         *     GXfogEndZ   = (256 - fog.offset) / fog.multiplier
         *
         * que es la z donde el coeficiente de niebla del N64 vale 0 y 256.
         * Esa z es la NDC DE OPENGL, que va de -1 (cerca) a +1 (lejos).  En
         * D3D `z/w` va de 0 a 1.  Compararlas directamente mezcla dos
         * espacios: con `ini=0.1` la niebla arrancaba en `z/w=0.1` en vez de
         * en 0.55, y como `z/w` se satura hacia 1 en cuanto te alejas un poco
         * del plano cercano, el factor salia ~1 en TODA la habitacion.  Con
         * el color de niebla a negro (medido: `[GXFOG] col=000000`), eso es
         * `lerp(color, NEGRO, 1)`: negro.
         *
         * ★ Y por eso ninguna sonda de las entradas lo vio: la niebla se
         * aplica DESPUES del combinador, y los modos de marcado que
         * sustituyen el pixel shader entero no la emiten.  Textura, SHADE,
         * PRIM y ENV estaban todos bien porque el defecto nunca estuvo ahi.
         *
         * La palanca `xbox64_marcatex=sinniebla` genera este mismo shader sin
         * el bloque: es el A/B que decide si esto era. */
        /* ★★★ EL COEFICIENTE VIENE CALCULADO POR VERTICE.  NO SE RECALCULA.
         *
         * Antes esto reconstruia la niebla por pixel a partir de
         * `IN.pos.z / IN.pos.w`, o sea del espacio de recorte de D3D, y lo
         * comparaba contra `startz`/`endz`.  Es una reconstruccion, y estaba
         * mal por construccion: `startz`/`endz` salen de despejar el
         * coeficiente del N64 `coef = z*multiplier + offset` (gSP.cpp:1810)
         * con la `z/w` DEL PLUGIN, que no es la de D3D.  Sintoma: escenas
         * interiores enteras a negro, y ningun ajuste del rango lo arreglaba
         * -- porque el error no estaba en el rango sino en la magnitud que se
         * comparaba contra el.
         *
         * ★ LA REFERENCIA.  El glN64 original no reconstruye nada: calcula el
         * coeficiente POR VERTICE (OpenGL.cpp:1309)
         *
         *     fog = max(0, z/w * fog.multiplier + fog.offset)
         *
         * y lo interpola.  glN64_GX se salta ese bloque porque delega en el
         * hardware de GX (`//Fog is taken care of in hardware with GX`), y el
         * shim no tiene ese hardware: de ahi la reconstruccion.  Ahora se
         * hace lo que hace el original y el numero viaja en el vertice
         * (`GXD3DVertex::fog` -> TEXCOORD4).
         *
         * Asi no hay conversion de espacios ni constante que afinar: cero
         * numeros magicos y vale para cualquier juego.
         *
         * ★ Por que costo diez hipotesis: la niebla se aplica DESPUES del
         * combinador, y los modos de marcado que SUSTITUYEN el pixel shader
         * entero (`textura`, `sombreado`, `combinador`) NO la emiten.  Por eso
         * cada entrada medida por separado salia sana -- lo estaba-- y el
         * defecto nunca estuvo en ninguna de ellas.
         *
         * Comprobacion: `marcatex=sinniebla` deja la escena PERFECTA, y
         * `[GLN64MUX] ROJO ... niebla: veces=180` con todos los demas
         * combinadores a cero. */
        /* GXD3D_SetNieblaInversa: el blender `CLR_IN * A_SHADE + CLR_FOG *
         * (1 - A_SHADE)` pesa la niebla con 1 - A (Nightmare Creatures). */
        if (k->fogInv)
            EMIT("  float ff = 1.0 - saturate(IN.fogc * (1.0 / 255.0));\n");
        else
            EMIT("  float ff = saturate(IN.fogc * (1.0 / 255.0));\n");
        EMIT("  outc.rgb = lerp(outc.rgb, uFogColor.rgb, ff);\n");
    }

    /* El blender del N64 `CLR_IN * A_FOG + CLR_FOG * (1 - A_FOG)` (ver
     * GXD3D_SetBlenderNiebla en gccore.h).  Detras del alpha compare, que en
     * la N64 va antes que el blender; el alfa no lo toca. */
    /* ★ [F-Zero X] Modo 2: `CLR_FOG * A_SHADE + CLR_IN * (1 - A)`, con el peso
     * en el ALFA DEL VERTICE (`rasc.a`, sin pasar por el combinador): donde
     * F3DFLX2 deja el reflejo de los vehiculos.  Ver OGL_UpdateStates. */
    if (k->blFog == 2)
        EMIT("  outc.rgb = lerp(outc.rgb, uBlFog.rgb, saturate(rasc.a));\n");
    else if (k->blFog)
        EMIT("  outc.rgb = lerp(uBlFog.rgb, outc.rgb, uBlFog.a);\n");

    /* ★ Los dos visores, DESPUES de la niebla: `ff` solo existe si el bloque
     * de arriba se emitio, y en los combinadores que no llevan niebla hay que
     * sacar negro en vez de referenciar una variable que no esta declarada
     * (seria un fallo de compilacion, o sea pantalla negra, o sea la peor
     * senal posible: identica al defecto que se investiga). */
    if (k->corte == 0xFEu)          /* factor de niebla en gris */
    {
        if (k->fogType != GX_FOG_NONE)
            EMIT("  outc = float4(ff, ff, ff, 1.0);\n");
        else
            EMIT("  outc = float4(0.0, 0.0, 0.0, 1.0);\n");
    }
    else if (k->corte == 0xFDu)     /* profundidad en tres escalas */
    {
        EMIT("  float dd = IN.pos.z / max(IN.pos.w, 1e-6);\n"
             "  outc = float4(saturate(dd),\n"
             "                saturate((dd - 0.90) * 10.0),\n"
             "                saturate((dd - 0.99) * 100.0), 1.0);\n");
    }

    /* ★★ [Pilotwings, sombras] DIBUJAR SOBRE EL DEPTH IMAGE: LA PROFUNDIDAD ES
     * EL ROJO DEL COLOR.  GLideN64 ccff245f "Implement render to depth buffer.
     * Fixed shadows in Pilot Wings", hoy `ShaderFragmentRenderTarget`
     * (glsl_CombinerProgramBuilderCommon.cpp:973-981):
     *
     *     uRenderTarget 1:  fragDepth = fragColor.r
     *     uRenderTarget 2:  if (fragDepth >= texelFetch(uDepthTex, coord).r)
     *                           discard;
     *                       fragDepth = fragColor.r
     *
     * En el N64 el juego pone el color image = depth image y dibuja con Z_CMP:
     * el poligono se prueba con SU z contra el Z buffer, pero lo que se guarda
     * en el Z buffer es su COLOR.  Asi deja la silueta de la sombra, y luego un
     * poligono grande en el color buffer solo pasa la prueba dentro de ella.
     * D3D no puede probar una profundidad y escribir otra en una pasada (oDepth
     * vale para las dos cosas), asi que, como la referencia, la prueba va
     * aqui contra una COPIA del Z (uDepthTex = copyDepthBufferTexture) y el
     * device prueba con ALWAYS.
     *
     * La copia es una textura D24S8 resuelta (gx_d3d9.cpp, gxd3d_zcopia).  La
     * Z sale en VERDE: D3DFMT_D24S8 es GPUTEXTUREFORMAT_24_8 con
     * GPUSWIZZLE_OOGR (d3d9types.h:2019), r = X y g = Y, y en estos formatos
     * X es el campo de los bits BAJOS (comparese R5G6B5 = 5_6_5 con ORGB, rojo
     * = Z = bits altos): en 24_8, X = stencil e Y = la Z de 24 bits.  Encaja
     * con lo medido en la copia por CPU: el D24S8 de Xenos tiene la Z en los
     * 24 bits altos (`raw esq=FFFFFE00`).  SIN MEDIR en el shader: si las
     * sombras salen siempre o nunca, lo primero es cambiar `.g` por `.r`.
     *
     * `floor(vpos) + 0.5` y no `vpos + 0.5`: no depende de si VPOS llega con
     * el medio pixel sumado o sin el, y la copia se muestrea en POINT. */

    /* ★ [Beetle Adventure Racing, la niebla] GXD3D_SetNieblaZ: el
     * ShadowMapFragmentShader de GLideN64 (glsl_SpecialShadersFactory.cpp:
     * 87-131), sin N64DepthCompare:
     *
     *     iZ = bufZ > 0.999 ? 262143 : int(floor(bufZ * 262143.0));
     *     iN64z = zlut[iZ];  n64z = clamp(iN64z / 65532.0, 0, 1);
     *     index = min(255, int(n64z * 255.0));
     *     fragColor = vec4(uFogColor.rgb, float(tlut[index] >> 8) / 255.0);
     *
     * ★ UNA DIFERENCIA, Y ES A PROPOSITO: el Z de 18 bits NO es `bufZ *
     * 262143`.  En GLideN64 el Z del GPU ya es el del N64 normalizado; aqui
     * el del EFB va referido al viewport (gSP.viewport.nearz/farz), y el paso
     * que ya funciona es el de DepthCopy.cpp (_dcBuildLut, el de los soles de
     * 1080 y Episode I Racer): iZ = (z - nearz) * 261632 / (farz - nearz),
     * entre 0 y 261632, y SOLO el valor del borrado (z24 FFFFFE) sale como la
     * palabra de borrado del Z (uNieblaZPar = near, escala, umbral, borrado;
     * ver GXD3D_SetNieblaZ: el cubo de 256 de DepthCopy volvia cielo, sin
     * niebla, el fondo del escenario, log 34).
     * La zlut (DepthBuffer.cpp:353-367; DepthCopy.cpp _dcEncode, la misma) se
     * calcula aqui: exponente = unos seguidos desde el bit 17 (tope 7),
     * mantisa = (iZ >> (6 - min(6, exponente))) & 0x7FF, y el valor
     * ((exp << 11) | mant) << 2.  Los unos seguidos se cuentan contra los
     * siete umbrales 2^18 - 2^(18-k): un valor tiene k unos delante si y solo
     * si pasa el umbral k, y los umbrales crecen, asi que basta sumar los que
     * pasa.  Las divisiones por potencias de dos con +0.5, para que un exp2
     * con error no baje un entero.  La Z, en `.g` como el decal (D24S8 =
     * 24_8 + OOGR).  La curva (ARGB = color de niebla y TLUT >> 8) la sube y
     * la liga gx_d3d9.cpp, un texel por indice.  Sustituye a todo lo de
     * arriba: es el color del pixel, como el shader de GLideN64. */
    if (k->nieblaZ)
        EMIT("  {\n"
             "    float zNz = tex2D(uZCopia, (floor(vpos) + 0.5) * uZCopiaInv.xy).g;\n"
             "    float iZ = clamp(floor((zNz - uNieblaZPar.x) * uNieblaZPar.y), 0.0, 261632.0);\n"
             "    float eNz = step(131072.0, iZ) + step(196608.0, iZ) + step(229376.0, iZ)\n"
             "              + step(245760.0, iZ) + step(253952.0, iZ) + step(258048.0, iZ)\n"
             "              + step(260096.0, iZ);\n"
             "    float qNz = floor((iZ + 0.5) / exp2(6.0 - min(6.0, eNz)));\n"
             "    float mNz = qNz - 2048.0 * floor((qNz + 0.5) / 2048.0);\n"
             "    float wNz = (zNz >= uNieblaZPar.z) ? uNieblaZPar.w : (eNz * 2048.0 + mNz) * 4.0;\n"
             "    float iNz = min(255.0, floor(saturate(wNz / 65532.0) * 255.0));\n"
             "    outc = tex2D(uNieblaZ, float2((iNz + 0.5) / 256.0, 0.5));\n"
             "  }\n");

    /* ★★ [ISS64, sombras] EL MODO DECAL DEL N64, COMO GLideN64 CON
     * N64DepthCompare (ShaderN64DepthCompare, glsl_CombinerProgramBuilder
     * Common.cpp:1237-1273, `case 3`):
     *
     *     dz = 4.0*fwidth(curZ);  dzMin = min(dz, depthDeltaZ.r);
     *     bFarther = (curZ + dzMin) >= bufZ;  bNearer = (curZ - dzMin) <= bufZ;
     *     bMax = bufZ == 1.0;
     *     decal:  bRes = bFarther && bNearer && !bMax;
     *
     * O sea: el pixel pasa solo si su z esta A LA PAR de la del Z buffer,
     * por los DOS lados.  El juego deja la silueta de la sombra en el Z con
     * poligonos coplanarios al de la sombra, y el poligono final, en decal,
     * solo pasa dentro de ella.  glN64 hacia el decal con offset de poligono y
     * LEQUAL, que solo mira un lado: pasaba en TODO el suelo que hay detras,
     * y salia el poligono entero (el rectangulo).
     *
     * Dos diferencias, las dos por no tener la imagen de profundidad del N64
     * (GLideN64 la lee y la escribe por pixel con imageLoad/imageStore, que
     * D3D9 no tiene):
     *   - sin el deltaZ GUARDADO: `dzMin` es el dz propio.  Es la cota de
     *     arriba de `min(dz, dzBuf)`: algo mas permisivo, nunca menos;
     *   - suelo de cuantizacion: la copia es de 24 bits y la z propia es un
     *     float interpolado, asi que dz no baja de 4 pasos de 24 bits (en un
     *     poligono de frente fwidth es casi 0 y la propia copia ya difiere).
     * `bMax` con margen por lo mismo: el borrado se lee como FFFFFE (medido en
     * la copia por CPU: `raw esq=FFFFFE00`), no como 1.0 exacto. */
    if (k->decN64)
    {
        if (k->ztOp == GX_ZT_REPLACE)
            EMIT("  float zDec = saturate(uZTex.x);\n");
        else
            EMIT("  float zDec = saturate(IN.pos.z / IN.pos.w);\n");
        EMIT("  float zBufDec = tex2D(uZCopia, (floor(vpos) + 0.5) * uZCopiaInv.xy).g;\n"
             "  float dzDec = max(4.0 * fwidth(zDec), 4.0 / 16777215.0);\n"
             "  clip((zDec + dzDec >= zBufDec && zDec - dzDec <= zBufDec &&\n"
             "        zBufDec < 1.0 - 4.0 / 16777215.0) ? 1.0 : -1.0);\n");
    }

    if (k->rtz)
    {
        if (k->rtz == 2)
        {
            if (k->ztOp == GX_ZT_REPLACE)
                EMIT("  float zPropia = saturate(uZTex.x);\n");
            else
                EMIT("  float zPropia = saturate(IN.pos.z / IN.pos.w);\n");
            EMIT("  float zCopia = tex2D(uZCopia, (floor(vpos) + 0.5) * uZCopiaInv.xy).g;\n"
                 "  clip((zPropia < zCopia) ? 1.0 : -1.0);\n");
        }
        /* ★ [Mario Golf] Modo 3: la COPIA de un Z guardado al Z, pixel a pixel
         * (GLideN64 7dca7f8f: el texrect lee la textura de PROFUNDIDAD del
         * buffer de origen y fragDepth = fragColor.r).  La Z, en `.g` como en
         * el modo 2 (D24S8 = 24_8 + OOGR, ya medido en Pilotwings). */
        /* ★ [Zelda MM, Lente de la Verdad] Modo 4: el `uRenderTarget 4` de
         * GLideN64 5347689b (glsl_CombinerProgramBuilderCommon.cpp, ZeldaMM):
         *     if (texelFetch(uDepthTex, coord).r > 0.0) discard;
         *     fragDepth = fragColor.r;
         * con uDepthTex = copia del Z actual y el color = el Z del fondo: el
         * Z guardado vuelve solo donde el actual vale 0. */
        if (k->rtz == 4)
            EMIT("  clip((tex2D(uZActual, (floor(vpos) + 0.5) * uZCopiaInv.xy).g > 0.0) ? -1.0 : 1.0);\n"
                 "  o.c = outc;\n"
                 "  o.d = tex2D(uZCopia, (floor(vpos) + 0.5) * uZCopiaInv.xy).g;\n"
                 "  return o;\n}\n");
        else if (k->rtz == 3)
            EMIT("  o.c = outc;\n"
                 "  o.d = tex2D(uZCopia, (floor(vpos) + 0.5) * uZCopiaInv.xy).g;\n"
                 "  return o;\n}\n");
        /* ★ [Resident Evil 2] Modo 5: la profundidad es la PALABRA DE Z DEL N64
         * del texel (GLideN64 5e51b386: textura de Z con el texel crudo,
         * `fragDepth = fragColor.r`).  La trae GXGetRE2Z_RGBA8 (glN64_GX/
         * Textures.cpp): R = byte alto, G = byte bajo.  Alli la geometria
         * escribe su Z YA en palabras del N64 (writeDepth con la zlut); aqui la
         * geometria se queda como esta y es la palabra la que pasa al Z del
         * EFB, con la inversa del paso de DepthCopy.cpp (_dcBuildLut):
         *     z18 = (mantisa << max(6 - exp, 0)) + 2^18 - 2^(18 - exp)
         *     (la tabla de descompresion del RDP: 0, 20000, 30000 ... 3F800)
         *     z = nearz + z18 * (farz - nearz) / 261632
         * uRe2Z = (nearz, (farz - nearz) / 261632).  Las dos comparaciones dan lo
         * mismo: las dos funciones crecen.  Con la niebla por Z en el mismo
         * shader (no pasa: es solo de Beetle) se queda en el modo 1. */
        else if (k->rtz == 5 && !k->nieblaZ)
            EMIT("  {\n"
                 "    float zHi = floor(outc.r * 255.0 + 0.5);\n"
                 "    float zLo = floor(outc.g * 255.0 + 0.5);\n"
                 "    float zE  = floor(zHi / 32.0);\n"
                 "    float zM  = floor((zHi * 256.0 + zLo - zE * 8192.0) / 4.0);\n"
                 "    float z18 = zM * exp2(max(6.0 - zE, 0.0)) + 262144.0 - exp2(18.0 - zE);\n"
                 "    o.c = outc;\n"
                 "    o.d = saturate(uRe2Z.x + z18 * uRe2Z.y);\n"
                 "  }\n"
                 "  return o;\n}\n");
        else
            EMIT("  o.c = outc;\n  o.d = saturate(outc.r);\n  return o;\n}\n");
    }
    else if (k->ztOp == GX_ZT_REPLACE)
        EMIT("  o.c = outc;\n  o.d = saturate(uZTex.x);\n  return o;\n}\n");
    else if (k->zClamp)
        /* GXD3D_SetDepthClamp: la profundidad que D3D habria interpolado
         * (`cpos` ya va mapeada al rango del viewport en el vertex shader),
         * sujeta a [0,1] por pixel, como GL_DEPTH_CLAMP.  El Z-texture de
         * arriba manda si los dos estan puestos: es una profundidad fija. */
        EMIT("  o.c = outc;\n  o.d = saturate(IN.pos.z / IN.pos.w);\n  return o;\n}\n");
    else
        EMIT("  return outc;\n}\n");

#if TEV_BISECT != 0
compile:
#endif

    /* [Doom 64, Conker] En tiempo de ejecucion con el modo `volcado` (el
     * centinela 0xFC), ademas del interruptor de compilacion.  Solo con
     * trazas: sin ellas el modo `volcado` dibuja igual y no vuelca nada. */
#if XBOX64_TRAZAS
    if (TEV_LOG_HLSL || k->corte == 0xFCu)
    {
        /* ★ SIN TOPE, y no es un descuido: este bloque solo se ejecuta al
         * GENERAR, o sea una vez por clave distinta, y eso son 19 en una
         * partida entera (`shgen=19`).  Un tope de "los primeros N" habria
         * dejado fuera justo los shaders raros, que son los que interesan
         * ([[feedback_sondas_filtrar_por_defecto]]).  El techo real ya lo
         * pone TEV_CACHE_MAX. */
        const char *p = src; int ln = 1; char hb[600];
        /* El resumen por etapa (`stN tc/tm reg`) en la cabecera hace falta
         * para navegar las decenas de shaders sin leerlos todos. */
        {
            char det[320]; int dp = 0, si;
            det[0] = 0;
            for (si = 0; si < k->numStages && dp + 24 < (int)sizeof(det); si++)
            {
                int n = _snprintf(det + dp, sizeof(det) - dp,
                                  " st%d tc%d/tm%d reg%d",
                                  si, (int)k->st[si].texcoord,
                                  (int)k->st[si].texmap, (int)k->st[si].creg);
                if (n <= 0) break;
                dp += n;
            }
            _snprintf(hb, sizeof(hb),
                      "[TEVHLSL] ---- shader #%u etapas=%d texgens=%d |%s",
                      (unsigned)gxd3d_dbg_shgen, (int)k->numStages,
                      (int)k->numTexGens, det);
        }
        DebugOutputString(hb);
        /* [Doom 64, Conker] Los VALORES en el momento de generarlo: los
         * registros (c0 PREV, c1 = PRIM, c2 = ENV en glN64), las konst, y el
         * combinador del N64 que lo produjo (solo tiene sentido con glN64:
         * con Rice es el ultimo que vio glN64). */
        {
            unsigned int m0 = 0, m1 = 0, ciclo = 0;
            gln64_mux_actual(&m0, &m1, &ciclo);
            _snprintf(hb, sizeof(hb),
                      "[TEVHLSL] mux glN64=%08X:%08X ciclo=%u | reg %02X%02X%02X%02X "
                      "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X | konst "
                      "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                      m0, m1, ciclo,
                      g_gxTev.reg[0].r, g_gxTev.reg[0].g, g_gxTev.reg[0].b, g_gxTev.reg[0].a,
                      g_gxTev.reg[1].r, g_gxTev.reg[1].g, g_gxTev.reg[1].b, g_gxTev.reg[1].a,
                      g_gxTev.reg[2].r, g_gxTev.reg[2].g, g_gxTev.reg[2].b, g_gxTev.reg[2].a,
                      g_gxTev.reg[3].r, g_gxTev.reg[3].g, g_gxTev.reg[3].b, g_gxTev.reg[3].a,
                      g_gxTev.konst[0].r, g_gxTev.konst[0].g, g_gxTev.konst[0].b, g_gxTev.konst[0].a,
                      g_gxTev.konst[1].r, g_gxTev.konst[1].g, g_gxTev.konst[1].b, g_gxTev.konst[1].a,
                      g_gxTev.konst[2].r, g_gxTev.konst[2].g, g_gxTev.konst[2].b, g_gxTev.konst[2].a,
                      g_gxTev.konst[3].r, g_gxTev.konst[3].g, g_gxTev.konst[3].b, g_gxTev.konst[3].a);
            hb[sizeof(hb) - 1] = 0;
            DebugOutputString(hb);
        }
        while (*p)
        {
            const char *e = strchr(p, '\n');
            int l = e ? (int)(e - p) : (int)strlen(p);
            char lb[520];
            if (l > 500) l = 500;
            memcpy(lb, p, l); lb[l] = 0;
            _snprintf(hb, sizeof(hb), "[TEVHLSL] %3d: %s", ln++, lb);
            DebugOutputString(hb);
            if (!e) break;
            p = e + 1;
        }
    }
#endif /* XBOX64_TRAZAS ([TEVHLSL]) */

    if (soloTexto)
    {
        *soloTexto = src;
        return NULL;
    }

    {
        /* ★ Se cronometra la compilacion.  En Rice esto NO se midio nunca --
         * ni una sonda cubre su GenerateShader, y el cubo `comb` que parece
         * cubrirlo cae FUERA de la ruta (Combiner.cpp:392 vs :405).  Aqui se
         * mide desde el principio porque es la cifra que dice si hace falta
         * cache en disco. */
#if XBOX64_TRAZAS   /* cronometro de [GXD3D] compilar= */
        unsigned int t0 = (unsigned int)__mftb();
#endif
        HRESULT hr = D3DXCompileShader(src, (UINT)strlen(src), NULL, NULL,
                                       "main", "ps_3_0", 0, &code, &err, NULL);
#if XBOX64_TRAZAS
        unsigned int t1 = (unsigned int)__mftb();
        /* El time base de Xenon va a 49,875 MHz: ticks/49,875 = us. */
        gxd3d_dbg_shcompus += (t1 - t0) / 50;
#endif

        if (FAILED(hr))
        {
#if XBOX64_TRAZAS
            gxd3d_dbg_shfail++;
            if (gxd3d_dbg_shfail <= 8)
            {
                char msg[512];
                _snprintf(msg, sizeof(msg), "[TEV] fallo de compilacion hr=0x%08X : %s",
                          (unsigned)hr, err ? (const char *)err->GetBufferPointer() : "(sin mensaje)");
                DebugOutputString(msg);
            }
#endif
            if (err)  err->Release();
            if (code) code->Release();
            free(src);
            return NULL;
        }
    }

    g_pd3dDevice->CreatePixelShader((const DWORD *)code->GetBufferPointer(), &ps);
    code->Release();
    if (err) err->Release();
    free(src);
#if XBOX64_TRAZAS
    gxd3d_dbg_shgen++;
#endif
    return ps;

    #undef EMIT
}

/* ------------------------------------------------------------------ */
/* Aplicacion                                                          */
/* ------------------------------------------------------------------ */
/* ★★ ¿APLICAR AHORA CAMBIARIA ALGO?  Es la pregunta que decide si un lote se
 * puede seguir extendiendo, y NO se puede contestar con el simple `s_dirty`.
 *
 * glN64 vuelve a fijar el combinador entero antes de casi cada lote, casi
 * siempre con los mismos valores (el mismo patron que ya obligo a comparar
 * las matrices en GX_LoadPosMtxImm).  Mirar `s_dirty` a secas diria "cambia"
 * en cada draw y no se fusionaria nada; y NO mirarlo -- que es lo que habia--
 * fusiona lotes con combinadores DISTINTOS, que es un fallo de dibujo.
 *
 * Asi que se resuelve la clave y se compara con la del shader que hay
 * ligado.  De paso, cuando coinciden se limpia `s_dirty` y el apply de
 * despues se ahorra la busqueda en la cache. */
/* El shader de RESPALDO de una clave con atlas (TEV_ASYNC): la misma clave
 * con atlas = 2, en la cache como uno mas y compilado en el acto (sin las
 * atlasNivel es de los baratos). */
static IDirect3DPixelShader9 *tev_respaldo(const GXD3DTevKey *k)
{
    GXD3DTevKey r = *k;
    IDirect3DPixelShader9 *ps;
    int i;

    r.atlas = 2;
    for (i = 0; i < s_cacheCount; i++)
        if (memcmp(&s_cache[i].key, &r, sizeof(r)) == 0)
            return s_cache[i].ps;
    if (s_cacheCount >= TEV_CACHE_MAX)
        return s_psRescate;
    ps = gxd3d_generate(&r, NULL);
    if (!ps) ps = s_psRescate;
    s_cache[s_cacheCount].key     = r;
    s_cache[s_cacheCount].ps      = ps;
    s_cache[s_cacheCount].trabajo = -1;
    s_cacheCount++;
    return ps;
}

int gxd3d_tev_difiere(void)
{
    GXD3DTevKey key;

    /* Cambio de modo de marcado (los repintados internos de FrameBuffer.cpp lo
     * apagan): lote nuevo, o el quad se fundiria con el lote marcado. */
    if (gln64_marcaluz != s_marcaAnt) return 1;
    if (s_constDirty)   return 1;   /* las constantes van al mismo draw */
    /* Se esta dibujando con el respaldo y el de atlas ya compilo: lote nuevo
     * y resolver otra vez, o se quedaria el respaldo hasta el proximo cambio
     * de combinador. */
    if (s_curPend >= 0 && s_trab[s_cache[s_curPend].trabajo].estado == 2)
    {
        s_dirty = 1;
        return 1;
    }
    if (!s_dirty)       return 0;
    if (!s_curKeyValid) return 1;

    gxd3d_build_key(&key);
    if (memcmp(&key, &s_curKey, sizeof(key)) == 0)
    {
        s_dirty = 0;                /* era una reescritura con los mismos valores */
        return 0;
    }
    return 1;
}

void gxd3d_tev_apply(void)
{
    GXD3DTevKey key;
    int i;
    float consts[TEVC_TOTAL * 4];

    /* ★ El marcado por via, delante de todo.  La transicion encendido<->apagado
     * ensucia el shader y la clave a mano: al volver al modo normal la clave
     * ligada seguiria pareciendo valida y no se reafirmaria ningun shader. */
    if (gln64_marcaluz != s_marcaAnt)
    {
        s_marcaAnt    = gln64_marcaluz;
        s_dirty       = 1;
        s_constDirty  = 1;
        s_curKeyValid = 0;
        s_psBindValid = 0;
    }
    if (s_marcaAnt == GLN64_MARCA_TEX)
    {
        /* La textura que haya ligada en la unidad 0, sin tocar nada mas.  El
         * atado lo hace `GX_LoadTexObj` antes de abrir el lote, asi que aqui
         * ya esta puesta. */
        if (!s_psTex) gxd3d_build_tex();
        if (s_psTex && (s_curPS != s_psTex || !s_psBindValid))
        {
            g_pd3dDevice->SetPixelShader(s_psTex);
            s_curPS       = s_psTex;
            s_psBindValid = 1;
        }
        return;
    }
    /* Log 10: pintando el alfa de TODAS las texturas la pantalla salia blanca
     * (casi todas son opacas) y no se veia nada.  Ahora solo los dibujos cuya
     * unidad 0 es un framebuffer resuelto; el resto va por el camino normal.
     * Al alternar, el camino normal tiene que volver a ligar su shader y sus
     * constantes: de ahi el `s_dirty`/`s_curKeyValid`/`s_constDirty`. */
    if (s_marcaAnt == GLN64_MARCA_TEXALFA && s_texEsFb[0])
    {
        if (!s_psTexAlfa) gxd3d_build_texalfa();
        if (s_psTexAlfa && (s_curPS != s_psTexAlfa || !s_psBindValid))
        {
            g_pd3dDevice->SetPixelShader(s_psTexAlfa);
            s_curPS       = s_psTexAlfa;
            s_psBindValid = 1;
        }
        s_dirty       = 1;
        s_constDirty  = 1;
        s_curKeyValid = 0;
        return;
    }

    if (s_marcaAnt == GLN64_MARCA_SHADE)
    {
        /* Ni constante ni paleta: el color de vertice interpolado.  No hay
         * nada que subir, asi que se liga y se sale. */
        if (!s_psShade) gxd3d_build_shade();
        if (s_psShade && (s_curPS != s_psShade || !s_psBindValid))
        {
            g_pd3dDevice->SetPixelShader(s_psShade);
            s_curPS       = s_psShade;
            s_psBindValid = 1;
        }
        return;
    }

    /* ★ El unico modo que pinta color plano.  `sinclip` y los cortes NO pueden
     * entrar aqui: su gracia es ejecutar el shader REAL, asi que caen por el
     * camino largo de abajo como si el marcado estuviera apagado. */
    if (s_marcaAnt == GLN64_MARCA_MUX)
    {
        float m[4];
        unsigned int rgb;

        /* ★★ UN COLOR POR COMBINADOR, Y LA RANURA SALE DEL PROPIO SHIM.
         *
         * La clave del TEV identifica el combinador igual de bien que el mux
         * del N64 -- es lo que se deriva de el --, y se construye aqui mismo.
         * Antes esto venia de una tabla publicada desde `Set_TEV_combine`, o
         * sea un gancho dentro de un fichero de Wii64; asi el arbol copiado
         * queda INTACTO, que es lo que permite traer codigo nuevo de ese
         * proyecto sin conflictos. */
        {
            GXD3DTevKey kc;
            gxd3d_build_key(&kc);
            rgb = s_marcaColor[gxd3d_marca_ranura(&kc)];
        }
        m[0] = (float)((rgb >> 16) & 0xFF) / 255.0f;
        m[1] = (float)((rgb >>  8) & 0xFF) / 255.0f;
        m[2] = (float)( rgb        & 0xFF) / 255.0f;
        m[3] = 1.0f;

        if (!s_psMarca) gxd3d_build_marca();
        if (s_psMarca && (s_curPS != s_psMarca || !s_psBindValid))
        {
            g_pd3dDevice->SetPixelShader(s_psMarca);
            s_curPS       = s_psMarca;
            s_psBindValid = 1;
        }
        /* Sin comparar con el valor anterior: el color cambia con la textura
         * ligada, o sea casi en cada lote, y la comparacion costaria mas que
         * el envio.  Es un modo de diagnostico. */
        g_pd3dDevice->SetPixelShaderConstantF(TEVC_MARCA, m, 1);
        return;
    }

    /* TEV_ASYNC: el de atlas que faltaba ya compilo. */
    if (s_curPend >= 0 && s_trab[s_cache[s_curPend].trabajo].estado == 2)
        s_dirty = 1;

    if (s_dirty)
    {
        IDirect3DPixelShader9 *ps = NULL;
        gxd3d_build_key(&key);
        s_curPend = -1;

        /* Busqueda lineal, como la de Rice.  Con un combinador POR ETAPAS el
         * numero de entradas se queda en decenas, no en cientos: si algun dia
         * `gxd3d_dbg_shgen` se dispara, eso mismo seria la senal de que la
         * clave lleva algo que deberia ser una constante. */
        for (i = 0; i < s_cacheCount; i++)
        {
            if (memcmp(&s_cache[i].key, &key, sizeof(key)) == 0)
            {
                ps = s_cache[i].ps;
#if XBOX64_TRAZAS
                gxd3d_dbg_shhit++;
#endif
                break;
            }
        }
        if (i == s_cacheCount)
        {
            /* ★★ LA CACHE LLENA NO PUEDE GENERAR: SERIA UNA FUGA POR DRAW.
             *
             * La primera version hacia `ps = gxd3d_generate(&key);` y solo
             * lo guardaba `if (s_cacheCount < TEV_CACHE_MAX)`.  Con la cache
             * llena eso crea un IDirect3DPixelShader9 NUEVO en cada draw y no
             * lo guarda ni lo suelta: con miles de draws por segundo se come
             * la memoria en segundos y acaba en violacion de acceso.
             *
             * Ahora: si esta llena NO se genera.  Se cuenta en `shovf` y se
             * reutiliza el shader que hubiera puesto.  El dibujo sale mal,
             * pero mal y VISIBLE, no un cuelgue diferido; y el contador dice
             * exactamente que hay que subir TEV_CACHE_MAX.
             *
             * `shovf` > 0 sostenido tambien significaria que la clave lleva
             * algo que deberia viajar en una CONSTANTE: con un combinador por
             * etapas las claves distintas se cuentan por decenas. */
            if (s_cacheCount < TEV_CACHE_MAX)
            {
                int j = -1;
#if TEV_ASYNC
                /* Los de atlas, al hilo de compilacion (ver TEV_ASYNC). */
                if (key.atlas == 1)
                {
                    char *txt = NULL;
                    gxd3d_generate(&key, &txt);
                    if (txt)
                    {
                        j = tev_async_encolar(txt);
                        if (j < 0) free(txt);
                    }
                }
#endif
                ps = (j < 0) ? gxd3d_generate(&key, NULL) : NULL;
                /* Si la generacion fallo, cachear el de RESCATE y no NULL:
                 * asi el draw sale (mal, pero visible) en vez de no salir, y
                 * no se reintenta la compilacion en cada frame. */
                if (!ps && j < 0) ps = s_psRescate;
                s_cache[s_cacheCount].key     = key;
                s_cache[s_cacheCount].ps      = ps;
                s_cache[s_cacheCount].trabajo = j;
                s_cacheCount++;
            }
            else
            {
#if XBOX64_TRAZAS
                gxd3d_dbg_shovf++;
#endif
                ps = s_curPS;
            }
        }
#if TEV_ASYNC
        /* Su shader de atlas sigue en el hilo: si ya compilo se crea aqui; si
         * no, se dibuja con el respaldo y se vuelve a mirar en cada lote. */
        if (i < s_cacheCount && s_cache[i].trabajo >= 0)
        {
            ps = tev_async_recoger(s_cache[i].trabajo);
            if (ps)
            {
                s_cache[i].ps      = ps;
                s_cache[i].trabajo = -1;
            }
            else
            {
                ps = tev_respaldo(&key);
                s_curPend = i;
            }
        }
#endif
        if (ps != s_curPS || !s_psBindValid)
        {
            g_pd3dDevice->SetPixelShader(ps);
            s_curPS       = ps;
            s_psBindValid = 1;
        }
        s_curKey      = key;      /* lo que hay ligado, para gxd3d_tev_difiere */
        s_curKeyValid = 1;
        s_dirty = 0;
    }
    else if (s_curPS && !s_psBindValid)
    {
        /* ★ Reafirmar el shader aunque no haya cambiado: el device esta
         * COMPARTIDO con el frontend, que liga el suyo para dibujar su quad
         * y su menu entre frames.  "Ligar una vez" dejo de ser valido en
         * cuanto el core dejo de ser dueno exclusivo del device
         * (ver project_xenon64_libretro_port).
         *
         * ★ Pero UNA VEZ POR PASADA, no una por lote: la bandera la baja
         * `gxd3d_tev_invalidar_ligado()` desde `gxd3d_frame_begin`, que es el
         * unico instante en el que el frontend ha podido robarlo.  Ver la
         * nota larga de `s_psBindValid`. */
        g_pd3dDevice->SetPixelShader(s_curPS);
        s_psBindValid = 1;
    }

    /* Constantes: solo cuando han cambiado.  Ver `s_constDirty`. */
    if (!s_constDirty) return;
    s_constDirty = 0;

    for (i = 0; i < 4; i++)
    {
        consts[(TEVC_REG + i) * 4 + 0] = g_gxTev.reg[i].r / 255.0f;
        consts[(TEVC_REG + i) * 4 + 1] = g_gxTev.reg[i].g / 255.0f;
        consts[(TEVC_REG + i) * 4 + 2] = g_gxTev.reg[i].b / 255.0f;
        consts[(TEVC_REG + i) * 4 + 3] = g_gxTev.reg[i].a / 255.0f;

        consts[(TEVC_KONST + i) * 4 + 0] = g_gxTev.konst[i].r / 255.0f;
        consts[(TEVC_KONST + i) * 4 + 1] = g_gxTev.konst[i].g / 255.0f;
        consts[(TEVC_KONST + i) * 4 + 2] = g_gxTev.konst[i].b / 255.0f;
        consts[(TEVC_KONST + i) * 4 + 3] = g_gxTev.konst[i].a / 255.0f;
    }
    consts[TEVC_ACREF * 4 + 0] = g_gxTev.acRef0 / 255.0f;
    consts[TEVC_ACREF * 4 + 1] = g_gxTev.acRef1 / 255.0f;
    consts[TEVC_ACREF * 4 + 2] = 0.0f;
    consts[TEVC_ACREF * 4 + 3] = 0.0f;

    consts[TEVC_FOGCOL * 4 + 0] = g_gxTev.fogColor.r / 255.0f;
    consts[TEVC_FOGCOL * 4 + 1] = g_gxTev.fogColor.g / 255.0f;
    consts[TEVC_FOGCOL * 4 + 2] = g_gxTev.fogColor.b / 255.0f;
    consts[TEVC_FOGCOL * 4 + 3] = g_gxTev.fogColor.a / 255.0f;

    consts[TEVC_FOGPAR * 4 + 0] = g_gxTev.fogStartZ;
    consts[TEVC_FOGPAR * 4 + 1] = g_gxTev.fogEndZ;
    consts[TEVC_FOGPAR * 4 + 2] = g_gxTev.fogNearZ;
    consts[TEVC_FOGPAR * 4 + 3] = g_gxTev.fogFarZ;

    /* [Xbox360] ★★★ SONDA TEMPORAL: LA NIEBLA, QUE NUNCA SE HA MEDIDO.
     *
     * El shader hace
     *     ff   = saturate((fz - uFogPar.x) / max(uFogPar.y - uFogPar.x, 1e-6))
     *     outc = lerp(outc, uFogColor, ff)
     *
     * Si `start >= end` -- o los dos a cero --, `ff` satura a 1 en TODOS los
     * pixeles y el color final pasa a ser ENTERAMENTE el de niebla.  Con
     * niebla negra, pantalla negra, sin que textura, paleta, vertices ni
     * combinador tengan nada malo.  Y eso es exactamente el cuadro de 007
     * TWINE, donde ya esta descartado todo lo demas con datos.
     *
     * El suelo, que se ve bien, seria geometria dibujada SIN niebla: por eso
     * se cuentan las dos ramas por separado. */
#if XBOX64_TRAZAS   /* [GXFOG] */
    if (g_gxTev.fogType != GX_FOG_NONE) gxd3d_dbg_fogon++;
    else                                gxd3d_dbg_fogoff++;
    gxd3d_dbg_fogcol = ((unsigned)g_gxTev.fogColor.r << 16)
                     | ((unsigned)g_gxTev.fogColor.g <<  8)
                     |  (unsigned)g_gxTev.fogColor.b;
    gxd3d_dbg_fogini = g_gxTev.fogStartZ;
    gxd3d_dbg_fogfin = g_gxTev.fogEndZ;
#endif

    consts[TEVC_ZTEX * 4 + 0] = (float)g_gxTev.ztBias / (float)GX_MAX_Z24;
    consts[TEVC_ZTEX * 4 + 1] = 0.0f;
    consts[TEVC_ZTEX * 4 + 2] = 0.0f;
    consts[TEVC_ZTEX * 4 + 3] = 0.0f;

    consts[TEVC_BLFOG * 4 + 0] = g_gxTev.blFogColor.r / 255.0f;
    consts[TEVC_BLFOG * 4 + 1] = g_gxTev.blFogColor.g / 255.0f;
    consts[TEVC_BLFOG * 4 + 2] = g_gxTev.blFogColor.b / 255.0f;
    consts[TEVC_BLFOG * 4 + 3] = g_gxTev.blFogColor.a / 255.0f;

    g_pd3dDevice->SetPixelShaderConstantF(0, consts, TEVC_TOTAL);
    g_pd3dDevice->SetPixelShaderConstantF(GXD3D_RUIDO_CONST, s_ruido, 1);
    g_pd3dDevice->SetPixelShaderConstantF(GXD3D_LOD_CONST, s_lod, 2);
    g_pd3dDevice->SetPixelShaderConstantF(GXD3D_ATLAS_CONST, s_atlas, 2);
}

/* ------------------------------------------------------------------ */
/* Las 16 funciones GX del TEV: solo acumulan estado                   */
/* ------------------------------------------------------------------ */
static __inline GXD3DTevStage *stg(u8 s)
{
    if (s >= GXD3D_MAX_STAGES) s = GXD3D_MAX_STAGES - 1;
    return &g_gxTev.stage[s];
}

/* ★★ COMPARAR ANTES DE ENSUCIAR, COMO `rsSet`.
 *
 * glN64_GX vuelve a fijar el combinador ENTERO antes de casi cada lote, casi
 * siempre con los mismos valores (`Combiner_SetCombineStates`).  Marcar sucio
 * a ciegas tenia dos costes:
 *
 *   - `gxd3d_tev_apply()` reconstruia la clave y recorria la cache de
 *     shaders en cada draw;
 *   - y al meter el combinador en la condicion de fusion, `gxd3d_tev_difiere()`
 *     tenia que reconstruir la clave otra vez para poder contestar.  Medido:
 *     `[GLN64T] draw` paso de 0,24 a 5,01 ms/s en una escena ligera y de
 *     0,75 a 22,01 en una pesada.
 *
 * Con el early-out, `s_dirty` solo se levanta cuando algo cambia DE VERDAD,
 * asi que `difiere()` sale por `if (!s_dirty) return 0;` y no cuesta nada.
 * Es exactamente lo que ya se hizo con las matrices en `GX_LoadPosMtxImm`. */
void GX_SetTevOrder(u8 tevstage, u8 texcoord, u32 texmap, u8 color)
{
    GXD3DTevStage *s = stg(tevstage);
    if (s->texcoord == texcoord && s->texmap == texmap && s->color == color)
        return;
    s->texcoord = texcoord; s->texmap = texmap; s->color = color;
    gxd3d_tev_dirty();
}

void GX_SetTevColorIn(u8 tevstage, u8 a, u8 b, u8 c, u8 d)
{
    GXD3DTevStage *s = stg(tevstage);
    if (s->cin[0] == a && s->cin[1] == b && s->cin[2] == c && s->cin[3] == d)
        return;
    s->cin[0] = a; s->cin[1] = b; s->cin[2] = c; s->cin[3] = d;
    gxd3d_tev_dirty();
}

void GX_SetTevAlphaIn(u8 tevstage, u8 a, u8 b, u8 c, u8 d)
{
    GXD3DTevStage *s = stg(tevstage);
    if (s->ain[0] == a && s->ain[1] == b && s->ain[2] == c && s->ain[3] == d)
        return;
    s->ain[0] = a; s->ain[1] = b; s->ain[2] = c; s->ain[3] = d;
    gxd3d_tev_dirty();
}

void GX_SetTevColorOp(u8 tevstage, u8 tevop, u8 tevbias, u8 tevscale, u8 clamp, u8 tevregid)
{
    GXD3DTevStage *s = stg(tevstage);
    if (s->cop == tevop && s->cbias == tevbias && s->cscale == tevscale &&
        s->cclamp == clamp && s->creg == tevregid)
        return;
    s->cop = tevop; s->cbias = tevbias; s->cscale = tevscale;
    s->cclamp = clamp; s->creg = tevregid;
    gxd3d_tev_dirty();
}

void GX_SetTevAlphaOp(u8 tevstage, u8 tevop, u8 tevbias, u8 tevscale, u8 clamp, u8 tevregid)
{
    GXD3DTevStage *s = stg(tevstage);
    if (s->aop == tevop && s->abias == tevbias && s->ascale == tevscale &&
        s->aclamp == clamp && s->areg == tevregid)
        return;
    s->aop = tevop; s->abias = tevbias; s->ascale = tevscale;
    s->aclamp = clamp; s->areg = tevregid;
    gxd3d_tev_dirty();
}

/* Los colores NO ensucian el shader: viajan en constantes.  Es justo lo que
 * hace que salgan pocos shaders. */
void GX_SetTevColor(u8 tev_regid, GXColor color)
{
    /* No ensucia el SHADER (el color viaja en constante, no en la clave),
     * pero si las CONSTANTES -- y esas SI rompen la fusion, porque van en el
     * mismo SetPixelShaderConstantF que el draw.  Comparar tambien aqui. */
    GXColor *p;
    if (tev_regid >= 4) return;
    p = &g_gxTev.reg[tev_regid];
    if (p->r == color.r && p->g == color.g && p->b == color.b && p->a == color.a)
        return;
    *p = color;
    gxd3d_tev_const_dirty();
}

void GX_SetTevKColor(u8 tev_kcregid, GXColor color)
{
    GXColor *p;
    if (tev_kcregid >= 4) return;
    p = &g_gxTev.konst[tev_kcregid];
    if (p->r == color.r && p->g == color.g && p->b == color.b && p->a == color.a)
        return;
    *p = color;
    gxd3d_tev_const_dirty();
}

void GX_SetTevKColorSel(u8 tevstage, u8 sel)
{
    GXD3DTevStage *s = stg(tevstage);
    if (s->kcsel == sel) return;
    s->kcsel = sel; gxd3d_tev_dirty();
}

void GX_SetTevKAlphaSel(u8 tevstage, u8 sel)
{
    GXD3DTevStage *s = stg(tevstage);
    if (s->kasel == sel) return;
    s->kasel = sel; gxd3d_tev_dirty();
}

void GX_SetTevSwapMode(u8 tevstage, u8 ras_sel, u8 tex_sel)
{
    /* Las swap tables permutan canales de RAS y TEX.  glN64_GX solo usa
     * GX_TEV_SWAP0 en los dos (TEV_combiner.cpp:290, :375), que es la
     * identidad, asi que no hay nada que emular.  Si algun dia llega otra,
     * se veria aqui. */
    (void)tevstage;
    if (ras_sel != GX_TEV_SWAP0 || tex_sel != GX_TEV_SWAP0)
    {
#if XENON64_DIAG
        static int avisado = 0;
        if (!avisado) { avisado = 1;
            DebugOutputString("[TEV] swap mode != SWAP0 sin implementar"); }
#endif
    }
}

/* Ver GXD3D_SetCopyMono en gxshim/gccore.h. */
void gxd3d_tev_tex_mono(u8 mapid, u8 mono)
{
    if (mapid >= 2) return;
    if (g_gxTev.texMono[mapid] == mono) return;
    g_gxTev.texMono[mapid] = mono;
    s_dirty = 1;     /* solo la clave: no toca constantes, no rompe el lote */
}

/* Ver gx_d3d9_int.h.  Solo cambia algo con el marcado `texalfa` puesto; el
 * cambio de textura que lo provoca ya vacia el lote en GX_LoadTexObj. */
void gxd3d_tev_tex_esfb(u8 mapid, u8 esfb)
{
    if (mapid >= 2) return;
    esfb = esfb ? 1 : 0;
    if (s_texEsFb[mapid] == esfb) return;
    s_texEsFb[mapid] = esfb;
    if (s_marcaAnt == GLN64_MARCA_TEXALFA)
    {
        s_dirty       = 1;
        s_curKeyValid = 0;
    }
}

void gxd3d_tev_zclamp(u8 on)
{
    on = on ? 1 : 0;
    if (g_gxTev.zClamp == on) return;
    g_gxTev.zClamp = on;
    s_dirty = 1;     /* solo la clave */
}

void gxd3d_tev_rtz(u8 modo)
{
    if (modo > 5) modo = 5;
    if (g_gxTev.rtz == modo) return;
    g_gxTev.rtz = modo;
    s_dirty = 1;     /* solo la clave */
}

void gxd3d_tev_decn64(u8 on)
{
    on = on ? 1 : 0;
    if (g_gxTev.decN64 == on) return;
    g_gxTev.decN64 = on;
    s_dirty = 1;     /* solo la clave */
}

void gxd3d_tev_nieblaz(u8 on)
{
    on = on ? 1 : 0;
    if (g_gxTev.nieblaZ == on) return;
    g_gxTev.nieblaZ = on;
    s_dirty = 1;     /* solo la clave */
}

void GXD3D_SetBlenderNiebla(u8 on, GXColor fog)
{
    if (on > 2) on = 1;   /* 2 = [F-Zero X] el peso es el alfa del vertice */
    if (g_gxTev.blFog != on)
    {
        g_gxTev.blFog = on;
        s_dirty = 1;     /* solo la clave */
    }
    /* El color solo importa encendido: apagado no se sube nada nuevo y no se
     * rompe el lote por una niebla que nadie usa. */
    if (on && (g_gxTev.blFogColor.r != fog.r || g_gxTev.blFogColor.g != fog.g ||
               g_gxTev.blFogColor.b != fog.b || g_gxTev.blFogColor.a != fog.a))
    {
        g_gxTev.blFogColor = fog;
        s_constDirty = 1;
    }
}

void GXD3D_SetNieblaInversa(u8 on)
{
    on = on ? 1 : 0;
    if (g_gxTev.fogInv == on) return;
    g_gxTev.fogInv = on;
    s_dirty = 1;     /* solo la clave */
}

void GXD3D_SetRuido(f32 semilla, f32 escX, f32 escY)
{
    const float ix = (escX > 0.0f) ? 1.0f / escX : 1.0f;
    const float iy = (escY > 0.0f) ? 1.0f / escY : 1.0f;
    if (s_ruido[0] == semilla && s_ruido[1] == ix && s_ruido[2] == iy)
        return;
    s_ruido[0] = semilla;
    s_ruido[1] = ix;
    s_ruido[2] = iy;
    s_constDirty = 1;
}

void GXD3D_SetLod(f32 kS, f32 kT, f32 minLod, f32 maxTile, f32 detalle, f32 activo)
{
    if (s_lod[0] == kS && s_lod[1] == kT && s_lod[2] == minLod && s_lod[3] == maxTile &&
        s_lod[4] == detalle && s_lod[5] == activo)
        return;
    s_lod[0] = kS;
    s_lod[1] = kT;
    s_lod[2] = minLod;
    s_lod[3] = maxTile;
    s_lod[4] = detalle;
    s_lod[5] = activo;
    s_constDirty = 1;
}

void GXD3D_SetLodAtlas(int activo, f32 atlasW, f32 atlasH, f32 realW1, f32 realH1,
                       f32 bilineal, f32 nivMax, f32 clampS, f32 clampT)
{
    float v[8];
    const u8 on = (activo && atlasW > 0.0f && atlasH > 0.0f) ? 1 : 0;
    if (on != s_atlasOn)
    {
        s_atlasOn = on;
        s_dirty = 1;     /* la clave de los shaders con LOD_FRACTION cambia */
    }
    if (!on)
        return;
    v[0] = 1.0f / atlasW;  v[1] = 1.0f / atlasH;  v[2] = realW1;  v[3] = realH1;
    v[4] = bilineal;       v[5] = nivMax;         v[6] = clampS;  v[7] = clampT;
    if (memcmp(v, s_atlas, sizeof(v)) == 0)
        return;
    memcpy(s_atlas, v, sizeof(v));
    s_constDirty = 1;
}

void GXD3D_SetMonoAlfaFija(u8 on)
{
    on = on ? 1 : 0;
    if (g_gxTev.monoAlfaFija == on) return;
    g_gxTev.monoAlfaFija = on;
    s_dirty = 1;     /* sin copia mono ligada la clave no cambia: no hay corte */
}

void GX_SetNumTevStages(u8 num)
{ if (g_gxTev.numStages  == num)      return; g_gxTev.numStages  = num;      gxd3d_tev_dirty(); }
void GX_SetNumChans(u8 num)
{ if (g_gxTev.numChans   == num)      return; g_gxTev.numChans   = num;      gxd3d_tev_dirty(); }
void GX_SetNumTexGens(u32 nr)
{ if (g_gxTev.numTexGens == (u8)nr)   return; g_gxTev.numTexGens = (u8)nr;   gxd3d_tev_dirty(); }

void GX_SetTexCoordGen(u16 texcoord, u32 tgen_typ, u32 tgen_src, u32 mtxsrc)
{
    /* glN64_GX solo lo llama una vez, con GX_TG_MTX2x4 / GX_TG_TEX0 /
     * GX_IDENTITY (OpenGL.cpp:577): las UV llegan listas del vertice y no
     * hay transformacion que aplicar. */
    (void)texcoord; (void)tgen_typ; (void)tgen_src; (void)mtxsrc;
}

void GX_SetZTexture(u8 op, u8 fmt, u32 bias)
{
    if (g_gxTev.ztOp == op && g_gxTev.ztFmt == fmt && g_gxTev.ztBias == bias)
        return;
    g_gxTev.ztOp = op; g_gxTev.ztFmt = fmt; g_gxTev.ztBias = bias;
    gxd3d_tev_dirty();
}

/* Atajo de libogc: configura una etapa con un modo predefinido.  Se replica
 * su implementacion exacta para no cambiar el comportamiento. */
void GX_SetTevOp(u8 tevstage, u8 mode)
{
    u8 defcolor = GX_CC_RASC, defalpha = GX_CA_RASA;
    if (tevstage != GX_TEVSTAGE0) { defcolor = GX_CC_CPREV; defalpha = GX_CA_APREV; }
    switch (mode)
    {
        case GX_MODULATE:
            GX_SetTevColorIn(tevstage, GX_CC_ZERO, GX_CC_TEXC, defcolor, GX_CC_ZERO);
            GX_SetTevAlphaIn(tevstage, GX_CA_ZERO, GX_CA_TEXA, defalpha, GX_CA_ZERO);
            break;
        case GX_DECAL:
            GX_SetTevColorIn(tevstage, defcolor, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
            GX_SetTevAlphaIn(tevstage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, defalpha);
            break;
        case GX_BLEND:
            GX_SetTevColorIn(tevstage, defcolor, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
            GX_SetTevAlphaIn(tevstage, GX_CA_ZERO, GX_CA_TEXA, defalpha, GX_CA_ZERO);
            break;
        case GX_REPLACE:
            GX_SetTevColorIn(tevstage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
            GX_SetTevAlphaIn(tevstage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
            break;
        case GX_PASSCLR:
            GX_SetTevColorIn(tevstage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, defcolor);
            GX_SetTevAlphaIn(tevstage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, defalpha);
            break;
        default: break;
    }
    GX_SetTevColorOp(tevstage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GX_SetTevAlphaOp(tevstage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

/* ------------------------------------------------------------------ */
/* Ciclo de vida                                                       */
/* ------------------------------------------------------------------ */
void gxd3d_tev_init(void)
{
    int i;
    memset(&g_gxTev, 0, sizeof(g_gxTev));
    for (i = 0; i < 4; i++)
    {
        g_gxTev.reg[i].r = g_gxTev.reg[i].g = g_gxTev.reg[i].b = 0;
        g_gxTev.reg[i].a = 255;
        g_gxTev.konst[i].r = g_gxTev.konst[i].g = g_gxTev.konst[i].b = 255;
        g_gxTev.konst[i].a = 255;
    }
    g_gxTev.numStages  = 1;
    g_gxTev.numTexGens = 1;
    g_gxTev.acComp0 = GX_ALWAYS;
    g_gxTev.acComp1 = GX_ALWAYS;
    g_gxTev.acOp    = GX_AOP_AND;
    g_gxTev.fogType = GX_FOG_NONE;
    s_cacheCount = 0;
    s_curPend = -1;
    s_curPS = NULL;
    s_curKeyValid = 0;   /* no hay nada ligado: difiere() no puede comparar */
    s_psBindValid = 0;   /* ...y por tanto tampoco esta ligado en el device */
    s_dirty = 1;
    gxd3d_build_rescue();
}

void gxd3d_tev_shutdown(void)
{
    int i;
    /* Lo que este en el hilo de compilacion se espera y se tira; sus
     * entradas tienen `ps` NULL. */
    tev_async_vaciar();
    s_curPend = -1;
    for (i = 0; i < s_cacheCount; i++)
    {
        /* Ojo: los fallidos apuntan al shader de RESCATE, que es compartido.
         * Soltarlo aqui lo liberaria tantas veces como entradas fallidas
         * haya.  Se salta y se suelta una sola vez mas abajo. */
        if (s_cache[i].ps && s_cache[i].ps != s_psRescate)
            s_cache[i].ps->Release();
        s_cache[i].ps = NULL;
    }
    s_cacheCount = 0;
    s_curPS = NULL;
    s_curKeyValid = 0;
    s_psBindValid = 0;
    if (s_psRescate) { s_psRescate->Release(); s_psRescate = NULL; }
    if (s_psMarca)   { s_psMarca->Release();   s_psMarca   = NULL; }
    if (s_psShade)   { s_psShade->Release();   s_psShade   = NULL; }
    if (s_psTex)     { s_psTex->Release();     s_psTex     = NULL; }
    if (s_psTexAlfa) { s_psTexAlfa->Release(); s_psTexAlfa = NULL; }
}
