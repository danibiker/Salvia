/* Entrega de video del core al frontend Salvia.  El porque completo esta en
 * video_handoff.h; aqui solo esta el como. */

#include <stdio.h>
#include <string.h>
#include <xtl.h>
#include <xgraphics.h>          /* XGSurfaceSize */
#include <ppcintrinsics.h>      /* __lwsync: modelo de memoria debil del PPC */

#include "video_handoff.h"
#include "d3d9_grabador.h"      /* d3dq_sinc: ver las tres fronteras de abajo */
#include "../source/xenon64_diag.h"   /* XBOX64_TRAZAS */

extern "C" void DebugOutputString(const char *msg);

/* -- VIDEO_EDRAM_MANUAL ------------------------------------------------------
 *
 * A 1 se coloca a mano el Z del core detras de su color en la EDRAM, con
 * D3DSURFACE_PARAMETERS::Base y el tamano que reporta XGSurfaceSize.  Es lo
 * que hace falta cuando uno crea la pareja color+Z por su cuenta: el XDK
 * asigna Base 0 por omision a AMBAS superficies, y entonces el Z pisa el
 * color.  En el standalone no habia que hacerlo porque la pareja la creaba el
 * propio CreateDevice (EnableAutoDepthStencil) y el driver la repartia.
 *
 * A 0 se pasa NULL en los dos, o sea se deja elegir al driver.  Es el control
 * del experimento: si la imagen sale bien con uno y rota con el otro, el fallo
 * esta aqui y no en el resto de la cadena.  Un cambio de una linea lo decide,
 * en vez de razonar a ciegas sobre la EDRAM.
 *
 * SINTOMA ESPERADO SI LA COLOCACION ESTA MAL: imagen con bandas, o el color
 * mezclado con el patron del Z-buffer.  NO negra: si no hubiera imagen el
 * problema seria otro.
 *
 * ★★ AQUI PONIA QUE COMPARTIR BASE CON EL BACK BUFFER DEL FRONTEND ERA
 * "INOCUO POR EL ORDEN: el core resuelve antes de retornar y el frontend
 * borra antes de dibujar".  ES FALSO y costo varias sesiones de bisect.
 *
 * Lo delato el OVERLAY DE FPS DEL FRONTEND apareciendo DENTRO de la imagen
 * del juego: dos superficies no pueden ensenar lo mismo salvo que sean la
 * misma memoria.  El frontend dibuja su quad, su menu y su overlay entre dos
 * retro_run, encima de nuestro color Y de nuestra profundidad.
 *
 * El reparto correcto -- las nuestras DETRAS del back buffer, con su tamano
 * consultado y no supuesto-- esta abajo, en xenon64_video_init. */
#define VIDEO_EDRAM_MANUAL 1

static IDirect3DDevice9  *s_dev      = NULL;

static IDirect3DSurface9 *s_rt       = NULL;   /* render target del core   */
static IDirect3DSurface9 *s_ds       = NULL;   /* depth-stencil del core   */
static IDirect3DTexture9 *s_tex      = NULL;   /* la que ve el frontend    */
static IDirect3DSurface9 *s_backbuf  = NULL;   /* back buffer del frontend */

static int s_w = 0, s_h = 0;

/* >>> AVISO DE FRAME NUEVO, ENTRE DOS HILOS.
 *
 * Con el traspaso ASINCRONO, quien resuelve es el HILO GRAFICO (HW2) y quien
 * lo consulta es el de emulacion desde retro_run (HW1).  Antes esto era un
 * `int s_frame_ready` pelado con un lee-y-limpia, y tenia dos defectos:
 *
 *  1. Sin `volatile` ni barrera, en el modelo de memoria debil del PPC el
 *     consumidor podia no ver nunca la escritura del productor.
 *  2. El lee-y-limpia es un READ-MODIFY-WRITE: si el productor pone el aviso
 *     entre la lectura y el borrado, ESE AVISO SE PIERDE.
 *
 * Funcionaba solo porque retro_run llama a gfx_thread_sync() ANTES de
 * consultar, o sea que de hecho no hay escritor concurrente en ese instante.
 * Depender de eso es precisamente el tipo de suposicion implicita que ya ha
 * fallado varias veces en este port.
 *
 * Ahora son dos contadores de secuencia con UN SOLO ESCRITOR CADA UNO, que es
 * el patron que ya usa gfx_thread.cpp con s_submit_seq/s_done_seq:
 *   - s_resolve_seq  lo escribe SOLO quien resuelve
 *   - s_visto_seq    lo escribe SOLO retro_run
 *
 * Precision que importa, porque la imprecision es lo que causo el fallo de
 * antes: "quien resuelve" NO es siempre el mismo hilo.  UpdateFrame corre en
 * el hilo grafico cuando el encolado va bien, y en el de EMULACION por el
 * camino de respaldo (`if(!gfx_thread_submit(...)) UpdateScreenStep2();` de
 * Video.cpp).  Pero nunca los dos A LA VEZ: el respaldo solo se toma cuando el
 * hilo no esta arrancado, y retro_run drena con gfx_thread_sync() antes de
 * tocar D3D.  O sea un solo escritor EN CADA INSTANTE, que es lo que el
 * incremento necesita; y ademas el contador solo CRECE, asi que ni siquiera
 * importa cual de los dos lo hizo.
 * Sin read-modify-write, sin atomicos, y no se puede perder un aviso.  La
 * comparacion va con resta CON SIGNO para que sea inmune al desbordamiento,
 * igual que el `(int)(done_seq - my_seq) >= 0` de memory.c. */
static volatile unsigned int s_resolve_seq = 0;
static unsigned int          s_visto_seq   = 0;

#if XBOX64_TRAZAS
static void vh_log(const char *msg)
{
    DebugOutputString(msg);
}
#endif

extern "C" void xenon64_video_set_device(void *pDevice)
{
    s_dev = (IDirect3DDevice9 *)pDevice;
}

extern "C" void *xenon64_video_get_device(void)
{
    return (void *)s_dev;
}

extern "C" int xenon64_video_init(int width, int height)
{
    HRESULT hr;
#if XBOX64_TRAZAS
    char dbg[192];
#endif

    if (!s_dev || width <= 0 || height <= 0)
        return 0;

    if (s_tex && width == s_w && height == s_h)
        return 1;                      /* ya esta, y al mismo tamano */

    xenon64_video_shutdown();

    /* El back buffer del frontend se cachea una vez: es a donde hay que
     * devolver el render target al terminar cada frame.  GetBackBuffer
     * incrementa el refcount, asi que lleva su Release en shutdown. */
    hr = s_dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &s_backbuf);
    if (FAILED(hr) || !s_backbuf)
    {
#if XBOX64_TRAZAS
        sprintf(dbg, "[VH] GetBackBuffer fallo hr=0x%08X", (unsigned int)hr);
        vh_log(dbg);
#endif
        return 0;
    }

#if VIDEO_EDRAM_MANUAL
    {
        D3DSURFACE_PARAMETERS sp;
        UINT colorTiles;
        UINT fbTiles = 0;
        UINT baseColor, baseZ;

        /* ★★ DETRAS DEL BACK BUFFER DEL FRONTEND, NO ENCIMA.
         *
         * Aqui arriba ponia que compartir base con el back buffer del
         * frontend "es inocuo por el orden: el core resuelve antes de
         * retornar y el frontend borra antes de dibujar".  ES FALSO, y lo
         * delato un sintoma que no deja lugar a dudas: el OVERLAY DE FPS DEL
         * FRONTEND se veia DENTRO de la imagen del juego.  Dos superficies
         * distintas no pueden ensenar lo mismo salvo que sean la misma
         * memoria.
         *
         * La cuenta lo confirma.  El back buffer del frontend lo coloca el
         * XDK en base 0, y a 1280x720 son 1280*720/1280 = 720 tiles, o sea
         * 0..719.  Nosotros poniamos color en 0..239 y Z en 240..479: los dos
         * DENTRO de esa region.  El frontend dibuja su quad, su menu y su
         * overlay entre dos retro_run, encima de nuestro color Y de nuestra
         * profundidad.
         *
         * Con la profundidad corrompida por lo que pinte el frontend, la
         * geometria se rechaza a trozos -- y que trozos depende de lo que
         * hubiera en pantalla, asi que cambian entre frames.  Eso son las
         * tiras que parpadean.  Y explica por que apagar el recorte las
         * "arreglaba": sin recorte el core repinta toda la pantalla y tapa lo
         * que el frontend dejo.  Enmascarar, no arreglar.
         *
         * Asi que las nuestras van DETRAS.  El tamano del back buffer se
         * consulta, no se supone: el usuario puede cambiar la resolucion del
         * frontend. */
        {
            D3DSURFACE_DESC fbd;
            if (s_backbuf && SUCCEEDED(s_backbuf->GetDesc(&fbd)))
                fbTiles = XGSurfaceSize(fbd.Width, fbd.Height, fbd.Format,
                                        fbd.MultiSampleType);
        }

        colorTiles = XGSurfaceSize(width, height, D3DFMT_A8R8G8B8,
                                   D3DMULTISAMPLE_NONE);

        baseColor = fbTiles;
        baseZ     = fbTiles + colorTiles;

        /* La EDRAM son 2048 tiles (10 MB a 5120 bytes).  Si no cabe -- 1080p
         * son 1620 tiles y no quedan 480 libres-- se avisa y se vuelve al
         * reparto de antes, que es incorrecto pero conocido: mejor eso que
         * una base fuera de rango. */
        if (baseZ + colorTiles > 2048)
        {
#if XBOX64_TRAZAS
            sprintf(dbg, "[VH] EDRAM: no caben detras del frontend (%u+%u+%u>2048),"
                         " vuelvo a base 0 -- puede haber tiras",
                    (unsigned int)fbTiles, (unsigned int)colorTiles,
                    (unsigned int)colorTiles);
            vh_log(dbg);
#endif
            baseColor = 0;
            baseZ     = colorTiles;
        }

        memset(&sp, 0, sizeof(sp));
        sp.Base = baseColor;
        hr = s_dev->CreateRenderTarget(width, height, D3DFMT_A8R8G8B8,
                                       D3DMULTISAMPLE_NONE, 0, FALSE,
                                       &s_rt, &sp);
        if (FAILED(hr) || !s_rt)
        {
#if XBOX64_TRAZAS
            sprintf(dbg, "[VH] CreateRenderTarget %dx%d fallo hr=0x%08X",
                    width, height, (unsigned int)hr);
            vh_log(dbg);
#endif
            xenon64_video_shutdown();
            return 0;
        }

        /* El Z va DETRAS del color, y los dos detras del frontend.
         *
         * ★ MEDIDO: XGSurfaceSize devuelve TILES de EDRAM, no bytes, y
         * D3DSURFACE_PARAMETERS::Base tambien va en tiles -asi que la cuenta
         * es correcta, solo lo era el nombre-.  Para 640x480 en 32 bpp da
         * exactamente 240: el tile de la 360 son 80x16 pixeles = 1280, y
         * 640*480/1280 = 240. */
        memset(&sp, 0, sizeof(sp));
        sp.Base = baseZ;
        hr = s_dev->CreateDepthStencilSurface(width, height, D3DFMT_D24S8,
                                              D3DMULTISAMPLE_NONE, 0, TRUE,
                                              &s_ds, &sp);
#if XBOX64_TRAZAS
        sprintf(dbg, "[VH] EDRAM: frontend=%u tiles | core color base=%u (%u tiles)"
                     " Z base=%u | total %u de 2048",
                (unsigned int)fbTiles, (unsigned int)baseColor,
                (unsigned int)colorTiles, (unsigned int)baseZ,
                (unsigned int)(baseZ + colorTiles));
        vh_log(dbg);
#endif
    }
#else
    hr = s_dev->CreateRenderTarget(width, height, D3DFMT_A8R8G8B8,
                                   D3DMULTISAMPLE_NONE, 0, FALSE,
                                   &s_rt, NULL);
    if (FAILED(hr) || !s_rt)
    {
#if XBOX64_TRAZAS
        sprintf(dbg, "[VH] CreateRenderTarget %dx%d fallo hr=0x%08X",
                width, height, (unsigned int)hr);
        vh_log(dbg);
#endif
        xenon64_video_shutdown();
        return 0;
    }
    hr = s_dev->CreateDepthStencilSurface(width, height, D3DFMT_D24S8,
                                          D3DMULTISAMPLE_NONE, 0, TRUE,
                                          &s_ds, NULL);
#endif

    if (FAILED(hr) || !s_ds)
    {
#if XBOX64_TRAZAS
        sprintf(dbg, "[VH] CreateDepthStencilSurface %dx%d fallo hr=0x%08X",
                width, height, (unsigned int)hr);
        vh_log(dbg);
#endif
        xenon64_video_shutdown();
        return 0;
    }

    /* La textura de entrega.  Formato TILEADO a proposito (sin LIN_), que es
     * lo que Resolve escribe de todas formas; es el mismo CreateTexture que
     * usa Fbg_GpuCreate (d3d9_backend.cpp:6178), o sea un destino de Resolve
     * ya probado en este arbol. */
    hr = s_dev->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8,
                              D3DPOOL_DEFAULT, &s_tex, NULL);
    if (FAILED(hr) || !s_tex)
    {
#if XBOX64_TRAZAS
        sprintf(dbg, "[VH] CreateTexture %dx%d fallo hr=0x%08X",
                width, height, (unsigned int)hr);
        vh_log(dbg);
#endif
        xenon64_video_shutdown();
        return 0;
    }

    s_w = width;
    s_h = height;
    s_resolve_seq = 0;
    s_visto_seq   = 0;

    /* ★ ARRANCAR EN NEGRO, Y NO ES COSMETICO.
     *
     * `CreateRenderTarget` y `CreateTexture` no garantizan el contenido: lo
     * que haya en esa EDRAM y en esa memoria de GPU es lo que dejo quien la
     * uso antes.  El frontend liga `s_tex` en su sampler y la muestrea desde
     * el PRIMER frame, o sea ANTES de que el juego haya dibujado nada -- en
     * Paper Mario eso es casi medio segundo-- asi que ensena basura.
     *
     * Se borra el render target y se resuelve a la textura de entrega una
     * sola vez, aqui, que es donde las dos acaban de existir.  Cuesta un
     * Clear y un Resolve por ROM abierta.
     *
     * El render target del frontend se devuelve al salir, como hace
     * `xenon64_video_frame_end`: si lo dejasemos apuntando al nuestro, su
     * Clear + quad + Present del siguiente frame caeria dentro de nuestra
     * textura (trampa nº3 de video_handoff.h). */
    {
        D3DRECT src;
        s_dev->SetRenderTarget(0, s_rt);
        s_dev->SetDepthStencilSurface(s_ds);
        s_dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                     D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

        /* RECT explicito por la misma razon que en el resolve de entrega:
         * con NULL el destino se queda SIN TOCAR si los tamanos no casan. */
        src.x1 = 0;  src.y1 = 0;
        src.x2 = s_w; src.y2 = s_h;
        s_dev->Resolve(D3DRESOLVE_RENDERTARGET0, &src, s_tex,
                       NULL, 0, 0, NULL, 0.0f, 0, NULL);

        if (s_backbuf)
        {
            s_dev->SetRenderTarget(0, s_backbuf);
            s_dev->SetDepthStencilSurface(NULL);
        }
    }

#if XBOX64_TRAZAS
    sprintf(dbg, "[VH] entrega por textura lista: %dx%d A8R8G8B8 tileada (borrada a negro)", s_w, s_h);
    vh_log(dbg);
#endif
    return 1;
}

extern "C" void xenon64_video_shutdown(void)
{
    /* Antes de soltar nada, devolver el render target del frontend: si el
     * device se queda apuntando a una superficie liberada, el primer Clear
     * del frontend cae sobre memoria muerta. */
    if (s_dev && s_backbuf)
    {
        s_dev->SetRenderTarget(0, s_backbuf);
        s_dev->SetDepthStencilSurface(NULL);
    }

    if (s_tex)     { s_tex->Release();     s_tex = NULL; }
    if (s_ds)      { s_ds->Release();      s_ds = NULL; }
    if (s_rt)      { s_rt->Release();      s_rt = NULL; }
    if (s_backbuf) { s_backbuf->Release(); s_backbuf = NULL; }

    s_w = 0;
    s_h = 0;
    s_resolve_seq = 0;
    s_visto_seq   = 0;
}

extern "C" void xenon64_video_frame_begin(void)
{
    if (!s_dev || !s_rt || !s_ds)
        return;

    /* Simetrico al de frame_end: aqui tampoco puede quedar nada del core en
     * el anillo cuando se cambia el destino de render a mano. */
    d3dq_sinc();

    s_dev->SetRenderTarget(0, s_rt);
    s_dev->SetDepthStencilSurface(s_ds);

    /* El frontend ha dibujado su quad y su overlay desde que retornamos, y ha
     * dejado SU pipeline puesto.  Ver el comentario largo de la funcion. */
    xenon64_core_state_apply();

    /* SetRenderTarget puede dejar el viewport al tamano del nuevo destino,
     * asi que se vuelve a poner el del N64 explicitamente.  Es barato y quita
     * una dependencia de comportamiento del driver. */
    {
        D3DVIEWPORT9 vp;
        memset(&vp, 0, sizeof(vp));
        vp.X = 0;
        vp.Y = 0;
        vp.Width  = s_w;
        vp.Height = s_h;
        vp.MinZ = 0.0f;
        vp.MaxZ = 1.0f;
        s_dev->SetViewport(&vp);
    }
}

extern "C" void xenon64_video_frame_end(void)
{
    if (!s_dev)
        return;

    /* >>> ANTES DE DEVOLVER EL RENDER TARGET, VACIAR LO DEL CORE.
     *
     * Este fichero habla con `s_dev` DIRECTAMENTE, no por el grabador, asi
     * que sin esto se cambia el destino de render por debajo de los
     * DrawPrimitive que siguen en el anillo: se ejecutan despues y pintan
     * sobre el BACK BUFFER en vez de sobre el render target del core.  Ver
     * d3dq_sinc en d3d9_grabador.h. */
    d3dq_sinc();

    if (s_backbuf)
    {
        s_dev->SetRenderTarget(0, s_backbuf);
        s_dev->SetDepthStencilSurface(NULL);
    }
}

extern "C" int xenon64_video_resolve(void)
{
    HRESULT hr;
    D3DRECT src;

    if (!s_dev || !s_tex || s_w <= 0 || s_h <= 0)
        return 0;

    /* >>> EL MAS IMPORTANTE DE LOS TRES -- Y DESDE LA ETAPA 3 YA NO VA AQUI.
     *
     * El Resolve de abajo va directo a `s_dev` y es lo que ENTREGA el frame
     * al frontend (`++s_resolve_seq`).  Si quedan DrawPrimitive del core en
     * el anillo, este Resolve captura un frame a medio pintar y el siguiente
     * los ejecuta encima: eso es el parpadeo.
     *
     * ★★ [etapa 3] Aqui habia un `d3dq_sinc()` que garantizaba eso, y con el
     * grabador a 2 se convierte en un FALLO: esta funcion la ejecuta ahora el
     * consumidor, como la orden `Entrega`, A MITAD DE UN `d3dq_drena`.  Un
     * vaciado dentro de otro ejecutaria las ordenes que vienen detras y
     * luego el de fuera avanzaria `g_d3dqCola` una vez mas: se saltaria una
     * orden o leeria una ranura vacia.
     *
     * Y ya no hace falta: la garantia la da ahora la propia cola.  `Entrega`
     * es una orden mas, asi que cuando el consumidor llega a ella todos los
     * dibujos anteriores ya se han ejecutado, por orden.  Con el grabador a
     * 0 o a 1 el `d3dq_sinc()` era un no-op de todas formas. */

    /* RECT DE ORIGEN EXPLICITO, no NULL: con NULL el destino se queda SIN
     * TOCAR cuando el render target y la textura no miden lo mismo.  Esta
     * medido en este arbol con FBG_DUMP; ver Fbg_GpuResolve. */
    src.x1 = 0;
    src.y1 = 0;
    src.x2 = s_w;
    src.y2 = s_h;

    hr = s_dev->Resolve(D3DRESOLVE_RENDERTARGET0, &src, s_tex,
                        NULL, 0, 0, NULL, 0.0f, 0, NULL);
    if (FAILED(hr))
    {
#if XBOX64_TRAZAS
        static int quejas = 0;
        if (quejas < 8)
        {
            char dbg[160];
            ++quejas;
            sprintf(dbg, "[VH] Resolve fallo hr=0x%08X %dx%d",
                    (unsigned int)hr, s_w, s_h);
            vh_log(dbg);
        }
#endif
        return 0;
    }

    /* El Resolve ya esta encolado en el device; el __lwsync ordena eso -y el
     * resto de lo que haya escrito este hilo- ANTES de anunciar el frame, para
     * que el consumidor no pueda ver el aviso y no lo anunciado. */
    __lwsync();
    ++s_resolve_seq;
    return 1;
}

extern "C" void *xenon64_video_texture(void) { return (void *)s_tex; }
extern "C" int   xenon64_video_width(void)   { return s_w; }
extern "C" int   xenon64_video_height(void)  { return s_h; }

/* ★★★ CONTABILIDAD DE IMAGENES PERDIDAS.  Ver la nota de
 * `xenon64_video_take_frame_ready`, aqui debajo. */
/* ★ `extern "C"` OBLIGATORIO: video_handoff.h las declara DENTRO de su
 * bloque `extern "C"`, y una definicion con enlace C++ pediria el simbolo
 * decorado.  Es el mismo LNK2001 que ya costo un build con
 * `g_maxTextureMemUsage`. */
extern "C" {
volatile unsigned int xenon64_vh_resuelve = 0;   /* resolves anunciados     */
volatile unsigned int xenon64_vh_nuevas   = 0;   /* tomas con imagen nueva  */
volatile unsigned int xenon64_vh_repite   = 0;   /* tomas sin nada nuevo    */
volatile unsigned int xenon64_vh_perdidas = 0;   /* ★ producidas y tiradas  */
}

extern "C" int xenon64_video_take_frame_ready(void)
{
    unsigned int cur = s_resolve_seq;   /* una sola lectura del compartido */
    int           av;

    __lwsync();                         /* la lectura antes de fiarse de ella */

    av = (int)(cur - s_visto_seq);

    if (av <= 0)
    {
#if XBOX64_TRAZAS   /* contadores: solo los lee el log */
        xenon64_vh_repite++;
#endif
        return 0;                       /* nada nuevo: el frontend repite */
    }

    /* ★★ ESTA LINEA TIRA IMAGENES, Y HASTA AHORA NADIE LO CONTABA.
     *
     * `s_visto_seq = cur` salta al ULTIMO resolve, no avanza de uno en uno.
     * Es lo correcto -- los resolves comparten render target, asi que el
     * segundo ya piso al primero y no hay nada que rescatar --, pero
     * significa que cada vez que se anuncian DOS resolves entre dos tomas,
     * una imagen producida no llega a la pantalla.
     *
     * ★ Y ES EL CANDIDATO A LOS MICROCORTES, porque no lo ve ningun otro
     * contador: el frame no se pasa de presupuesto, el frontend no marca
     * `late` y `vbl/frame` sale clavado en 1,0000.  Lo unico que lo delata
     * hoy es `img` por debajo de 30, y eso no distingue "el juego no
     * dibujo" de "lo dibujo y lo tiramos".
     *
     * MEDIDO antes de esta sonda (log 3, Mario 64 con Rice): `img<30` en 40
     * segundos de 94, pero `run_max` solo pasa de 16.667 us en 17.  O sea
     * que en la mayoria de los casos se pierde la imagen SIN que el frame
     * se haya pasado de tiempo.
     *
     * `perdidas` cuenta exactamente cuantas se tiran.  Si sale 0 y `img`
     * sigue por debajo de 30, el que no dibuja es el juego y esta via queda
     * descartada -- que tambien es una respuesta. */
#if XBOX64_TRAZAS   /* contadores: solo los lee el log */
    if (av > 1)
        xenon64_vh_perdidas += (unsigned int)(av - 1);

    xenon64_vh_nuevas++;
    xenon64_vh_resuelve = cur;
#endif

    s_visto_seq = cur;                  /* solo lo escribe este hilo */
    return 1;
}
