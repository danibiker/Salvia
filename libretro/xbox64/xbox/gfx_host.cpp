/*
 * xbox/gfx_host.cpp  --  la adopcion del device D3D9 del frontend.
 *
 * ★★ POR QUE HACE FALTA ESTE FICHERO, QUE NO ESTABA EN EL PLAN.
 *
 * El shim GX->D3D9 de glN64 (`xenon/gx_d3d9.cpp` y companyia) NO crea ni
 * adopta el device: lo usa a traves de `g_pd3dDevice`, y da por hecho que
 * alguien lo ha dejado puesto.  En el arbol del que viene el shim, ese
 * alguien es `xenon/d3d9_backend.cpp:528 y :1405` -- ★ EL SHIM DE RICE, que
 * en este proyecto NO se copia (decision de la fase 0: un solo plugin).
 *
 * O sea que al quedarnos con glN64 heredamos una dependencia sobre el
 * fichero que deliberadamente dejamos fuera.  Censado el resto por si habia
 * mas: de lo que aportaba d3d9_backend.cpp, el camino de glN64 solo usa
 * `g_pd3dDevice`.  `d3d_real`, `EnsureGraphicsDevice`, `d3dq_hilo_init` y
 * TODO el modulo FBG (`Fbg_*`) tienen cero referencias -- lo que confirma
 * la medida de la fase 0 de que el framebuffer de Rice no entra aqui.
 *
 * Asi que este fichero es pequenyo a proposito: las tres cosas que faltaban
 * y ni una mas.
 *
 * C++03 (VS2010): `d3d9_grabador.h` declara una clase.
 */

#include "../xenon/d3d9_grabador.h"
#include "../xenon/video_handoff.h"
#include "../xenon/gx_d3d9.h"
/* gln64_backend_init / _shutdown, gln64_set_options, gln64_apply_rdram_size
 * y gxd3d_tev_flush_cache.  ★ De este fichero solo se usa eso: las
 * `gfx_plugin_select/selected/name` son del selector de DOS plugins del
 * arbol de origen, aqui no las llama nadie y no se definen. */
#include "../xenon/gfx_plugin_sel.h"
/* El hilo grafico: se arranca al final de `xbox64_gfx_init` y se para al
 * principio de `xbox64_gfx_shutdown`. */

extern "C" {
#include "plataforma.h"
#include "../source/xenon64_diag.h"   /* XBOX64_TRAZAS */
}

/* ====================================================================== */
/* 1. El device                                                           */
/* ====================================================================== */

#if D3D_GRABADOR
/* ★ Instancia ESTATICA y no reservada dinamicamente, igual que en el arbol
 * de origen (d3d9_backend.cpp:526): vive lo que el proceso y asi no hay que
 * ordenar su destruccion contra la del device del frontend. */
static D3DGrabador s_grabador;
#endif

/* El simbolo que declara `d3d9_grabador.h:615` y que usan gx_d3d9.cpp,
 * gx_d3d9_int.h y gx_d3d9_tev.cpp. */
D3DDispositivo * g_pd3dDevice = NULL;

/* El device de VERDAD, sin pasar por el grabador.  Declarado en
 * d3d9_grabador.h desde siempre y sin definir en este arbol hasta la etapa
 * 3, porque nadie lo usaba.  Ahora lo usa el lado del consumidor del shim
 * (`gxd3d_consumidor_empieza`), que tiene que hablar con el device y no con
 * la cola.  Igual que en el origen (Xenon64/xenon/d3d9_backend.cpp:530). */
IDirect3DDevice9 * d3d_real(void)
{
#if D3D_GRABADOR
    return s_grabador.real;
#else
    return g_pd3dDevice;
#endif
}

/* ====================================================================== */
/* 1bis. Lo que quedaba del grabador                                      */
/* ====================================================================== */
/* ★ `d3d9_grabador.h` DECLARA la API del anillo pero quien la IMPLEMENTA
 * es `d3d9_backend.cpp`, el shim de Rice.  Con `D3D_GRABADOR 0` (ver el
 * porque en d3d9_grabador.h) desaparecen `d3dq_mete`, `d3dq_payload`,
 * `d3dq_drena` y el ejecutor de 31 comandos.
 *
 * Sobrevive `d3dq_soltar`, porque `gx_d3d9.cpp` la llama en OCHO sitios
 * SIN guarda de grabador (:2221, :2293, :2309, :2841, :2882, :3286 y
 * :3297).  Con el anillo apagado es exactamente un Release. */

/* ★★ EL PARAMETRO ES `D3DResource *` Y NO `void *`, Y ESO ES EL ARREGLO
 * DE UN CUELGUE QUE YA SE PAGO EN EL ARBOL DE ORIGEN.
 *
 * En Xbox 360 los recursos de D3D NO SON COM: `D3DResource` es un struct
 * llano con `Release()` NO virtual y sin `IUnknown` (d3d9.h:935).  Un
 * `((IUnknown*)p)->Release()` despacharia por una vtable que no existe:
 * leeria el primer campo del struct como puntero de vtable y saltaria a
 * la nada.  Con el tipo correcto, la llamada es directa. */
/* ★ [2026-09-30] `d3dq_soltar` SE HA MUDADO A xenon/d3dq.cpp, junto con el
 * resto del grabador (etapa 2 del renderizado con hilo).  Con el anillo
 * apagado o en pasarela sigue siendo exactamente un `Release()`; con los
 * valores 2 y 3 encola la destruccion en orden con los dibujos, y eso tiene
 * que vivir junto al anillo.
 *
 * Lo que se aprendio aqui sigue valiendo alli: va SIN `extern "C"`, porque
 * d3d9_grabador.h la declara con enlace C++ -- el LNK pedia el simbolo
 * DECORADO (`?d3dq_soltar@@YAXPAUD3DResource@@@Z`) -- y el parametro es
 * `D3DResource *` porque en 360 los recursos no son COM. */

/* La llama `video_handoff.cpp:349` en cada frame_begin.
 *
 * ★ AQUI ES UN NO-OP, Y ESTA COMPROBADO QUE PUEDE SERLO.  En el arbol de
 * origen reestablecia dos cosas que el frontend pisa entre pasadas: la
 * declaracion de vertices DE RICE (`g_pVertexDecl`) y
 * `D3DRS_VIEWPORTENABLE`.  Las dos son especificas de aquel plugin:
 *
 *   - la declaracion de vertices de Rice no existe en este proyecto; el
 *     shim de glN64 crea y liga la SUYA en gln64_backend_init.
 *   - `VIEWPORTENABLE` ya lo pone el propio shim en `gx_d3d9.cpp:1448`,
 *     y su comentario dice literalmente que se repite alli "para que el
 *     backend sea autosuficiente y no dependa de que el otro plugin le
 *     prepare el terreno".
 *
 * ★ NO SE BORRA LA FUNCION: si algun dia el sintoma es "textura NEGRA con
 * el juego corriendo, dibujando y sonando", este es el primer sitio donde
 * mirar -- es la firma de que nadie reestablece el pipeline, y ya costo
 * una sesion entera en el port de Rice. */
extern "C" void xenon64_core_state_apply(void)
{
}

/* ====================================================================== */
/* 2. DebugOutputString                                                   */
/* ====================================================================== */
/* La usan cinco ficheros del shim.  En el arbol de origen vive en
 * `source/api/callbacks.c`, que es de mupen64plus y aqui no existe.
 *
 * Se encamina al log del frontend por la MISMA via que todo lo demas
 * (xbox64_log_line), y no por OutputDebugStringA directamente: si hubiera
 * dos caminos de log, el orden de las lineas dejaria de ser fiable y
 * comparar dos capturas -- que es como se trabaja en este arbol -- dejaria
 * de valer. */
extern "C" void DebugOutputString(const char *msg)
{
#if XBOX64_TRAZAS   /* version publica: vacia (source/xenon64_diag.h) */
    xbox64_log_line(msg ? msg : "");
#else
    (void)msg;
#endif
}

/* ====================================================================== */
/* 3. Arranque y parada del video                                         */
/* ====================================================================== */

/*
 * ★ EL ORDEN IMPORTA Y ESTA MEDIDO EN EL ARBOL DE ORIGEN:
 *
 *   1. xenon64_video_set_device()  guarda el device que dio el frontend.
 *   2. g_pd3dDevice                pasa a apuntar al grabador, que envuelve
 *                                  ese device.  A partir de aqui el shim
 *                                  puede llamar a D3D.
 *   3. xenon64_video_init(w,h)     crea el render target, el depth-stencil
 *                                  y la textura de entrega.  ★ El device
 *                                  del frontend NO trae depth-stencil
 *                                  (EnableAutoDepthStencil esta comentado
 *                                  en SDL_xboxvideo.c), asi que el core se
 *                                  crea el suyo.
 *   4. gln64_backend_init(w,h)     crea los vertex buffers y la declaracion
 *                                  de vertices del shim.  ★ TIENE que ir
 *                                  DESPUES de que g_pd3dDevice apunte a
 *                                  algo: si no, el primer GX_Begin bloquea
 *                                  un vertex buffer NULL y revienta.
 *
 * Y todo esto ANTES de abrir la ROM, porque `InitiateGFX` ya toca el
 * backend.
 */
extern "C" int xbox64_gfx_init(void *device_frontend, int width, int height)
{
    if (!device_frontend)
    {
#if XBOX64_TRAZAS
        xbox64_log_line("[xbox64] el frontend no ha dado device D3D9");
#endif
        return 0;
    }

    xenon64_video_set_device(device_frontend);

#if D3D_GRABADOR
    /* [etapa 4] El anillo y el pool se reservan aqui, ANTES de que se grabe
     * la primera orden.  Idempotente: reserva la primera vez y en las
     * siguientes cargas solo reinicia los cursores. */
    if (!d3dq_init())
    {
#if XBOX64_TRAZAS
        xbox64_log_line("[xbox64] sin memoria para la cola de ordenes D3D");
#endif
        return 0;
    }
    s_grabador.real = (IDirect3DDevice9 *)xenon64_video_get_device();
    /* La cola ejecuta contra este device.  ANTES de que `g_pd3dDevice`
     * apunte al grabador: la primera orden que se grabe ya tiene que tener
     * contra quien ejecutarse. */
    d3dq_set_real(s_grabador.real);
    /* [etapa 3] Sombra de estado vacia: lo que hubiera apuntado de una ROM
     * anterior se refiere a objetos que ya no existen. */
    d3dq_sombra_reset();
    g_pd3dDevice    = s_grabador.real ? &s_grabador : NULL;
#else
    g_pd3dDevice    = (IDirect3DDevice9 *)xenon64_video_get_device();
#endif

    if (!g_pd3dDevice)
    {
#if XBOX64_TRAZAS
        xbox64_log_line("[xbox64] la adopcion del device fallo");
#endif
        return 0;
    }

    if (!xenon64_video_init(width, height))
    {
#if XBOX64_TRAZAS
        xbox64_log_line("[xbox64] xenon64_video_init fallo");
#endif
        return 0;
    }

    if (!gln64_backend_init(width, height))
    {
#if XBOX64_TRAZAS
        xbox64_log_line("[xbox64] gln64_backend_init fallo");
#endif
        return 0;
    }

    return 1;
}

extern "C" void xbox64_gfx_shutdown(void)
{
    gln64_backend_shutdown();
    xenon64_video_shutdown();
    g_pd3dDevice = NULL;
#if D3D_GRABADOR
    s_grabador.real = NULL;
    d3dq_set_real(NULL);
#endif
}
