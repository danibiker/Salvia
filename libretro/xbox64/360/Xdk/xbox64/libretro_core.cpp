/*
 * 360/Xdk/xbox64/libretro_core.cpp  --  el wrapper libretro de xbox64.
 *
 * FASE 1.  Sin plugin grafico, sin dynarec, sin audio y sin mandos: lo unico
 * que se persigue es la puerta de salida de la fase, que es *"si esto no
 * arranca una ROM al interprete, parar"*.
 *
 * ★ COMO SE COMPRUEBA ESA PUERTA SIN VIDEO.  No se comprueba a ojo: una
 * pantalla negra porque el emulador esta parado y una pantalla negra porque
 * todavia no hay plugin grafico se ven EXACTAMENTE igual.  Asi que esta fase
 * trae dos medidas que fallarian si la hipotesis "el invitado esta
 * ejecutando" fuera falsa:
 *
 *   1. SE PINTA EL FRAMEBUFFER DEL VI, leido de la RDRAM del invitado.  No
 *      es un adorno: para que ahi haya algo, el invitado tiene que haber
 *      programado VI_ORIGIN y VI_WIDTH y haber escrito pixeles.  Las
 *      secuencias de arranque de N64 borran el framebuffer por CPU antes de
 *      que entre el RDP, asi que con el interprete solo YA se tiene que ver
 *      algo: el borrado.  Si sale negro Y vi_origin sigue a cero, el
 *      invitado no ha llegado ni a inicializar el VI.
 *
 *   2. UN RESUMEN POR SEGUNDO con VI/s, frames entregados y vi_origin.  Un
 *      VI/s cercano a 60 dice que el reloj del invitado avanza; un vi_origin
 *      que cambia dice que hay doble bufer de verdad.
 *
 * El latido de la fila 0 esta a proposito: distingue "parado" de "corriendo
 * y negro", que es justo la ambiguedad que hunde esta fase.
 *
 * C++03 (VS2010).  La frontera con Wii64, que es C89, esta en
 * xbox/plataforma.h y no se cruza en ningun otro sitio.
 */

#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libretro.h"

extern "C" {
#include "../../../xbox/plataforma.h"
#include "../../../xbox/filebrowser_xbox.h"
#include "../../../xbox/xbox64_fase.h"
#include "../../../xbox/input_n64.h"
#include "../../../xbox/audio_n64.h"
#include "../../../xbox/savestate_n64.h"
#include "../../../xbox/xbox_mem.h"      /* xbox64_mem_guardado */
#include "../../../xbox/trucos_n64.h"    /* retro_cheat_reset / retro_cheat_set */
#include "../../../xenon/video_handoff.h"
}

/* La cola de ordenes D3D: sus contadores, `d3dq_bind` y `d3dq_drena`.
 *
 * ★★ FUERA DEL `extern "C"` DE ARRIBA, Y ESE ERA EL LNK2001.  La cabecera es
 * C++ -- declara la clase del grabador y funciones con enlace C++
 * (`d3dq_drena`, `d3dq_bind`, `d3dq_soltar`) -- y trae SU PROPIO
 * `extern "C"` para lo que lo necesita (los contadores y `d3dq_censo`).
 * Dentro del bloque, esas funciones pasaban a declararse con enlace C y el
 * enlazador las buscaba SIN DECORAR (`d3dq_drena`, no `?d3dq_drena@@YAXXZ`),
 * que no existe.  En la etapa 2 no se vio porque solo se usaban los
 * contadores. */
#include "../../../xenon/d3d9_grabador.h"

/* [etapa 4] El emulador en su propio hilo, al estilo de mupen.  C++, y por
 * lo mismo que la de arriba FUERA del `extern "C"`. */
#include "../../../xbox/hilo_emu.h"
/* [etapa 5] La lista en paralelo, en HW3.  Trae su propio `extern "C"`. */
#include "../../../xbox/hilo_lista.h"

/* gxd3d_frame_begin/end: las fronteras de pasada DEL BACKEND, que son otras
 * que las de `video_handoff`.  La cabecera ya trae su propio `extern "C"`. */
#include "../../../xenon/gx_d3d9.h"

/* El informe del perfilador de glN64.  Trae su propio `extern "C"`. */
#include "../../../xenon/gln64_prof.h"
/* ★ EXPLICITO, aunque `gln64_prof.h` ya lo arrastre: este fichero prueba
 * `#if XBOX64_SONDAS`, y una macro sin definir vale 0 en un `#if` SIN error y
 * SIN aviso.  O sea que si un dia cambia la cadena de includes, las sondas se
 * apagarian solas y el sintoma seria "el log no dice nada". */
#include "../../../source/xenon64_diag.h"

#include "../../../xenon/perfil_cap.h"
#include "../../../xenon/rice_prof.h"

#include <ppcintrinsics.h>   /* __mftb, para el reparto del frame */

/* ====================================================================== */
/* El contrato de video con Salvia                                        */
/* ====================================================================== */
/* Salvia deja que un core dibuje CON EL DEVICE DEL FRONTEND y le entregue
 * una textura, en vez de un bufer de pixeles.  Son dos llamadas propias de
 * este frontend; un core que las pida a otro se queda sin imagen en vez de
 * reventar, porque el `default` del switch devuelve false. */
#define RETRO_ENVIRONMENT_SALVIA_GET_D3D_DEVICE   30002  /* IDirect3DDevice9 ** */
#define RETRO_ENVIRONMENT_SALVIA_SET_D3D_TEXTURE  30003  /* salvia_d3d_texture * */

/* ★ TIENE QUE SER IDENTICA A LA DE src/salvia.cpp:822.  Son tres palabras
 * de 32 bits sin relleno posible en este ABI, asi que basta con no cambiar
 * el orden. */
struct salvia_d3d_texture
{
    void     *texture;   /* IDirect3DTexture9* que el frontend liga en s0 */
    unsigned  width;
    unsigned  height;
};

/* Lo de Wii64 que se lee directamente aqui: los registros del VI y la RDRAM.
 * Son C y estan declarados en cabeceras C. */
extern "C" {
#include "../../../source/main/winlnxdefs.h"
#include "../../../source/gc_memory/memory.h"
#include "../../../source/r4300/r4300.h"
#include "../../../source/r4300/interupt.h"
}

/* ★ El armado del volcado de texturas (L3+R3).  Lo define SIEMPRE
 * xenon/gx_d3d9.cpp, con el volcado encendido o apagado, para que el
 * enlazado no dependa de ningun interruptor. */
extern "C" volatile int g_texbmpArm;
extern "C" void gxd3d_texcache_habilitar(int on);
/* [Wii64 443f68b] `[ZCOPIA]`, la copia de profundidad de DepthCopy.cpp
 * (xenon/gx_d3d9.cpp).  No escribe nada si ningun juego la ha pedido. */
extern "C" void gxd3d_zcopia_informe(void);
/* La velocidad del VI segun el codigo de pais de la cabecera de la ROM: 50 en
 * PAL, 60 en NTSC (source/main/timers.c, de Wii64; no esta en timers.h). */
extern "C" int GetVILimit(void);

/* ====================================================================== */
/* Callbacks del frontend                                                 */
/* ====================================================================== */

static retro_environment_t   environ_cb  = NULL;
static retro_video_refresh_t video_cb    = NULL;
static retro_audio_sample_t  audio_cb    = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
static retro_input_poll_t    input_poll_cb  = NULL;
static retro_input_state_t   input_state_cb = NULL;
static retro_log_printf_t    log_cb      = NULL;

/* ====================================================================== */
/* Geometria de la fase 1                                                 */
/* ====================================================================== */
/* 640x480 XRGB8888.  Es la resolucion nativa maxima del VI de N64; el
 * escalado y el aspecto los hace el frontend. */
#define FB_ANCHO  640
#define FB_ALTO   480

/* ===========================================================================
 * ★★ XBOX64_DIV_RENDER -- LA PRUEBA QUE SEPARA RELLENO DE COSTE POR DRAW
 * ===========================================================================
 * DONDE ESTAMOS.  El log 41 dejo claro que en juego el muro es la GPU y no la
 * CPU: al bajar `tcupd` de 480 a 198 us/frame, `DrawPrimitive` subio de 963 ns
 * a 25.037 ns por draw y **`dt` no se movio** (16,6-17,9 ms, `late` 0-8/120).
 * El tiempo no aumento, cambio de sitio: lo que esperaba en el `Present` del
 * frontend ahora espera dentro del draw.  Quitarle mas trabajo a la CPU no da
 * frames mientras eso siga asi.
 *
 * Con la GPU como muro solo hay dos candidatos, y uno ya esta descartado:
 *
 *   COSTE POR DRAW  -> DESCARTADO.  Encender `GXD3D_LOTES` (fusion de lotes,
 *                      ~6.000 draws/s) no cambia nada, medido.
 *   RELLENO         -> queda este.  Y hay motivo: los logs dicen
 *                      `VI=320x240 GX=640x480 esc=2.000,2.000`, o sea que
 *                      rasterizamos **cuatro veces los pixeles que dibujaba
 *                      la N64**.  Con lo que estos juegos abusan del alfa,
 *                      es el sospechoso de libro.
 *
 * LA PRUEBA.  Este divisor solo afecta al tamano del render target, del
 * depth y de la textura de entrega del core.  El frontend recibe la textura
 * con su tamano real por `SET_D3D_TEXTURE` y la escala con su quad, asi que
 * la imagen sale CORRECTA, solo mas suave.
 *
 *   2  media resolucion: 320x240 = la CUARTA parte de pixeles.
 *   1  como siempre, 640x480.  Es el control.
 *
 * ★★ RESULTADO (log 42), Y POR ESO VUELVE A 1.
 *
 * `prim` se desplomo de **25.037 a 970 ns/draw**: ERA RELLENO, confirmado.
 * Pero NO DA FRAMES.  Con la misma carga pico (60.444 contra 60.194 tri/s):
 *
 *     late medio /120   4,1 -> 3,8
 *     dt medio          17,39 -> 17,33 ms
 *     dt peor           20,06 -> 19,95 ms
 *
 * O sea nada.  Al quitar la espera de GPU el tiempo VUELVE A LA CPU -- el
 * espejo exacto de lo que paso en el log 41, donde quitar 282 us de CPU hizo
 * que `DrawPrimitive` subiera de 963 ns a 25.037.  CPU y GPU estan las dos
 * al borde del presupuesto y SE TURNAN: matar una destapa la otra.
 *
 * Asi que media resolucion cuesta nitidez visible y compra 0,3 frames tarde
 * de cada 120.  No sale a cuenta como ajuste fijo.  Donde SI tendria sentido
 * es como core option, para quien prefiera 320x240 nativo en los juegos mas
 * pesados; eso es una decision de producto, no una optimizacion.
 *
 * No es invencion: Rice tiene este mismo experimento montado en este arbol
 * (`DIAG_HALF_RES`, ../Xenon64/xenon/d3d9_backend.cpp:1454), con la nota
 * *"si ese tramo sube a 60, era relleno"*.
 *
 * ★ La geometria que se declara en `retro_get_system_av_info` se queda en
 * 640x480 A PROPOSITO: es lo que el frontend usa para la relacion de
 * aspecto, y no tiene por que seguir a la resolucion interna. */
#define XBOX64_DIV_RENDER 1

/* ★ El framebuffer por software es SOLO de la fase 1: pintaba lo que el
 * invitado escribiera por CPU, para poder distinguir "corriendo" de
 * "parado" sin plugin grafico.  Con glN64 la imagen viaja como textura de
 * D3D y esto no se usa.  Se conserva guardado -- y no borrado -- porque es
 * la unica forma de volver a ver el framebuffer del VI en crudo si algun
 * dia hay que comparar contra lo que dibuja el plugin. */
#if XBOX64_GFX_NULO
static unsigned *s_fb = NULL;
#endif

/* ====================================================================== */
/* Contadores del resumen por segundo                                     */
/* ====================================================================== */

static unsigned s_frames      = 0;
static unsigned s_frames_prev = 0;

/* [etapa 3] Ordenes y bytes grabados en UN frame: la foto al empezar la
 * pasada y el maximo del segundo.  Con ~3 frames en vuelo (MAX_SWAP de
 * mupen), es lo que dimensiona el anillo y el pool de la etapa 4. */
static unsigned int s_q_ord0 = 0, s_q_byt0 = 0;
static unsigned int s_q_ordmax = 0, s_q_bytmax = 0;
static DWORD    s_t_resumen   = 0;

/* [etapa 4] El renderizado con hilo (xbox/hilo_emu.h).
 *   s_hilo_ok    el hilo del emulador existe: se crea en `retro_init` y vive
 *                lo que el core.
 *   s_modo_hilo  ESTA partida emula en ese hilo.  Se decide AL CARGAR con la
 *                core option `xbox64_gfx_thread`, como `EnableThreadedRenderer`
 *                en mupen (que tambien se lee en `retro_load_game`): cambiar
 *                la opcion a media partida vale para la carga siguiente.
 *                Con 0, `retro_run` es exactamente el de la etapa 3, y es el
 *                control del A/B. */
static int s_hilo_ok   = 0;
static int s_modo_hilo = 0;

/* [etapa 5] La lista en paralelo (xbox/hilo_lista.h).
 *   s_lista_ok   su hilo existe (retro_init).
 *   s_opt_lista  la core option `xbox64_lista_paralelo`, leida en
 *                `aplica_opciones`; manda AL CARGAR, antes de la ROM. */
static int s_lista_ok  = 0;
static int s_opt_lista = 1;
static int s_opt_lista_sp = 0;   /* `xbox64_lista_sp`, ver OPT_LISTA_SP */

/* ★★ LA MEDIDA QUE DECIDE LA FASE 2: cuantos frames ha RESUELTO el shim.
 *
 * Sin ella, una pantalla negra sigue significando dos cosas: "el plugin no
 * dibuja" o "dibuja y la imagen no llega al frontend".  `img` las separa:
 *
 *   img ~= frames   el shim resuelve imagen y la entrega -> si se ve negro,
 *                   el problema esta DENTRO del dibujado.
 *   img = 0         no resuelve nada -> el problema esta en el camino de
 *                   entrega (device, render target, o el plugin ni arranca).
 *
 * Es la misma leccion de la fase 1: una sonda tiene que dar valores
 * distintos segun cual de las dos hipotesis sea cierta. */
static unsigned s_img = 0;

/* ====================================================================== */
/* EL REPARTO DEL FRAME                                                   */
/* ====================================================================== */
/* ★★ MEDIDO, NO DERIVADO POR RESTA.  Es la leccion de Xenon64: alli el
 * cubo `cpu` se calculaba restando y ABSORBIA la espera del frontend, asi
 * que parecia enorme cuando en realidad el emulador estaba ocioso.  Aqui
 * cada tramo se cronometra con `__mftb()` alrededor, y lo que NO se mide
 * se ve como la diferencia con el presupuesto -- explicita, no escondida.
 *
 *   emu      lo que tarda `xbox64_run_frame()`.  ★ INCLUYE EL DIBUJADO:
 *            la lista de pantalla la procesa el RSP DENTRO de la
 *            emulacion, asi que aqui van CPU emulada Y plugin grafico
 *            juntos.  Quien los separa es `[GLN64T]`.
 *   entrega  el traspaso de video y el vaciado de audio.
 *
 * El presupuesto de un frame de N64 son 16,67 ms.  Si `emu+entrega` se
 * queda en 8, hay la mitad libre y el emulador NO es el cuello. */
static unsigned long long s_tb_emu     = 0;
static unsigned long long s_tb_entrega = 0;
/* ★ `entrega` es un RESIDUO: junta tres esperas de naturaleza distinta.  Ver
 * la nota larga junto a `xbox64_audio_drenar()`.  Estos tres las separan:
 *   audio   el frontend REGULA LA VELOCIDAD por la cola de audio
 *   fin     valla del hilo grafico + ultimo vaciado + Resolve  (espera a GPU)
 *   video   la llamada al frontend
 * Son arreglos opuestos, asi que el total no sirve para decidir. */
static unsigned long long s_tb_ent_audio = 0;
static unsigned long long s_tb_ent_fin   = 0;
static unsigned long long s_tb_ent_video = 0;

/* ===========================================================================
 * ★★ XBOX64_PASO0 -- el interruptor que separa "el informe" de "otra cosa"
 * ===========================================================================
 * SINTOMA: con el informe del shim puesto (`[GXSH]` + `max=`), OoT deja de
 * arrancar.  El log 35 se corta detras del `trampolin 7`, ANTES de la primera
 * display list -- no sale ni `[CIMG-GX]` ni la linea `[xbox64] frames=`.
 *
 * ★ Y ESO NO BASTA PARA EXONERAR AL INFORME, aunque lo parezca.  El primer
 * resumen salta al segundo, y en esa ventana el juego puede no haber emitido
 * todavia su primera lista: en el log 34 la emitio antes y en el 35 pudo ser
 * al reves.  O sea que "muere antes de dibujar" y "muere en el informe" son
 * compatibles, y no se pueden separar leyendo.
 *
 * Lo que SI esta descartado con el codigo delante:
 *   - el embudo del log no desborda: `retro_log_printf` (src/salvia.h:402)
 *     usa `_vsnprintf_s(buffer, 128, _TRUNCATE, ...)` y termina ahi.
 *   - los `sprintf` del informe caben de sobra en su buffer.
 *   - el desplazamiento del .text entre los dos builds (+0x4CD0) es el que
 *     justifica el codigo anadido; no hay nada raro en el enlazado.
 *
 *   0  el core vuelve al comportamiento del build del log 34, que funcionaba.
 *      ★ Con UNA diferencia deliberada: el troceado de `xbox64_log_line`, que
 *      no se apaga.  Es un arreglo del embudo del log (se perdian datos) y
 *      ademas conviene que este puesto en esta pasada: si el cuelgue
 *      desaparece con el paso 0 a 0, el troceado queda de paso exonerado.
 *   1  vuelven `[GXSH]` y `max=`.
 *
 * COMO SE LEE LA PASADA:
 *   arranca con 0  -> el informe ES el culpable y se bisecta dentro de el.
 *   sigue colgado  -> el informe queda descartado y el delta que queda es
 *                     NADA de codigo ejecutable, asi que el fallo no es de
 *                     esta ronda: hay que reprobar el binario del log 34
 *                     antes de seguir (podria ser intermitente). */
#define XBOX64_PASO0 1

/* ★★ EL PEOR FRAME, QUE ES EL QUE DECIDE SI HAY 60 fps.  LA MEDIA NO.
 *
 * Con vsync, un frame que se pasa de 16.667 us no cuesta "un poco mas": paga
 * un intervalo ENTERO.  Asi que el ritmo lo marca el PICO y no el promedio, y
 * hasta ahora solo se imprimia el promedio.
 *
 * El caso que lo destapo: en OoT, `emu` medio salia 13,5-14,7 ms -- dentro del
 * presupuesto-- y el frontend iba a 45 fps con `late=40/120`.  La explicacion
 * esta en `img`: el juego emite display list 16 veces por segundo, asi que de
 * 46 `retro_run` hay 16 que hacen TODO el trabajo grafico (~28 ms) y 30 que no
 * hacen ninguno (~6 ms).  El promedio no ve eso; este maximo si.
 *
 * Mide el tramo de EMULACION, no el `retro_run` entero.  `entrega` va aparte y
 * sale plano en 38 us, asi que `max` + 38 es el peor frame con buena
 * aproximacion -- y sin depender de por donde salga `retro_run`. */
static unsigned long long s_tb_emu_max = 0;

/* ===========================================================================
 * ★★ LA CABECERA DE `retro_run`, QUE NO MEDIA NADIE
 * ===========================================================================
 * `emu` arranca su cronometro JUSTO ANTES de `xbox64_run_frame()`, y para
 * entonces `retro_run` ya ha hecho:
 *
 *     input_poll · opciones · frecuencia de audio
 *     xenon64_video_frame_begin()   <- SetRenderTarget al EDRAM del core
 *     gxd3d_frame_begin()           <- Unlock + **Lock** del vertex buffer,
 *                                      SetScissorRect, SetRenderState
 *
 * Nada de eso entraba en `emu` ni en `entrega`: era tiempo INVISIBLE, y
 * cuadra con el hueco que arrastramos entre lo que suman los cubos (~17 ms
 * en el frame que dibuja) y el `emu max` medido (~23 ms).
 *
 * ★ Y hay un sospechoso concreto ahi dentro: el `Lock()` del vertex buffer.
 * El propio shim lo tiene anotado -- *"con UN solo buffer el Lock cuesta
 * 5,41 ms/frame porque espera a que la GPU suelte el de la ultima vez; con
 * tres, 0,00"* (gx_d3d9.cpp:464).  Aquella medida se tomo cuando la GPU iba
 * holgada.  Ahora sabemos que va al limite (bajar el relleno a la cuarta
 * parte tira `prim` de 25.037 a 970 ns), y con la GPU mas de dos frames por
 * detras tres buffers dejan de bastar: el Lock vuelve a esperar.
 *
 * `cab` = `run` - `emu` - `entrega`.  Se calcula por diferencia A PROPOSITO:
 * asi no se puede escapar nada por un tramo que se me olvide cronometrar.
 * Si sale grande, el siguiente paso es el reloj fino de `[GXSH] ini`, que
 * separa el Lock del resto. */
static unsigned long long s_tb_run     = 0;
static unsigned long long s_tb_run_max = 0;

/* ====================================================================== */
/* ★★★ UTILIZACION POR FRAME (no por segundo)                             */
/* ====================================================================== */
/* TODO lo demas de este fichero son medias o maximos POR SEGUNDO, y con eso
 * no se puede ver un microcorte: un tiron es UN frame, y una media de 60 lo
 * disuelve por construccion.  `run` sale 8.312 us de mediana -- la mitad del
 * presupuesto, aparentemente holgadisimo -- mientras `run_max` sale 15.251.
 * Esas dos cifras describen la MISMA cosa y cuentan historias opuestas.
 *
 * Aqui se guarda el dato crudo de cada frame y se vuelca la SECUENCIA, que
 * es lo unico que enseña el patron: si el frame malo es uno de cada dos
 * (juego a 30 fps con la carga sin repartir) o si aparece al azar (otra
 * causa).  Son dos diagnosticos distintos y ninguna media los separa.
 *
 * ★ EN PORCENTAJE DEL PRESUPUESTO, no en microsegundos: tres digitos por
 * frame caben 20 por linea, y el embudo del log corta a 127 caracteres.
 * `100` = el frame entero de 16.667 us.  Un valor >100 es un frame que se
 * paso, y a simple vista se ve cuantos hay y como se agrupan.
 *
 * ★ `gfx` es la parte de ESE frame que se fue en la tarea grafica del RSP
 * (con `gfxh=0` eso es todo Rice).  Se saca por delta del acumulador, asi
 * que no hace falta ningun cronometro nuevo. */
/* `logf` se define mas abajo (es el log de la casa, no DebugOutputString,
 * que en este fichero no esta declarada).  Se adelanta la declaracion en
 * vez de mover setenta lineas. */
static void logf(const char *fmt, ...);

/* Todo el resumen por segundo y sus medidas por frame: solo con trazas. */
#if XBOX64_TRAZAS
#define UTIL_N      64       /* un segundo cabe de sobra (60 frames)      */
#define UTIL_LINEA  20       /* valores por linea, para no partirla       */

static unsigned short s_util_run[UTIL_N];
static unsigned short s_util_gfx[UTIL_N];
static unsigned       s_util_n   = 0;
static unsigned int   s_util_gfx_prev = 0;

static void util_anota(unsigned long long dtr)
{
    unsigned int gfx_ahora, dgfx;
    unsigned long long us;

    if (s_util_n >= UTIL_N)
        return;                      /* el volcado por segundo lo vacia */

    /* Porcentaje del presupuesto, redondeando al alza para que un frame
     * justo por encima no salga como 100 clavado. */
    us = dtr / 50ULL;
    s_util_run[s_util_n] = (unsigned short)((us * 100ULL + 16666ULL) / 16667ULL);

    /* ★ `xbox64_rsp_gfx_tb` lo REINICIA el bloque `[RSPT]` una vez por
     * segundo.  Sin esta guarda, el primer frame de cada segundo restaria
     * contra el acumulado del anterior y saldria un delta enorme: un pico
     * falso, exactamente uno por segundo, que es justo el patron que
     * estamos intentando distinguir.  Con el reinicio detectado, ese frame
     * mide desde cero, que es lo correcto. */
    gfx_ahora = xbox64_rsp_gfx_tb;   /* acumulador; se lee por delta */
    if (gfx_ahora < s_util_gfx_prev)
        s_util_gfx_prev = 0;
    dgfx      = gfx_ahora - s_util_gfx_prev;
    s_util_gfx_prev = gfx_ahora;
    s_util_gfx[s_util_n] = (unsigned short)((((unsigned long long)dgfx / 50ULL)
                                             * 100ULL + 16666ULL) / 16667ULL);
    s_util_n++;
}

/* Vuelca la secuencia y la vacia.  Lo llama `resumen_por_segundo`. */
static void util_informe(void)
{
    char l[160];
    unsigned i, j, p;

    for (i = 0; i < s_util_n; i += UTIL_LINEA)
    {
        p = (unsigned)_snprintf(l, sizeof(l), "[UTIL run] %2u:", i);
        for (j = i; j < i + UTIL_LINEA && j < s_util_n; j++)
            p += (unsigned)_snprintf(l + p, sizeof(l) - p, " %3u", s_util_run[j]);
        l[sizeof(l) - 1] = 0;
        logf("%s", l);
    }
    for (i = 0; i < s_util_n; i += UTIL_LINEA)
    {
        p = (unsigned)_snprintf(l, sizeof(l), "[UTIL gfx] %2u:", i);
        for (j = i; j < i + UTIL_LINEA && j < s_util_n; j++)
            p += (unsigned)_snprintf(l + p, sizeof(l) - p, " %3u", s_util_gfx[j]);
        l[sizeof(l) - 1] = 0;
        logf("%s", l);
    }
    s_util_n = 0;
}

static unsigned tb_a_us_frame(unsigned long long tb, unsigned frames)
{
    static LONGLONG frec = 0;
    if (frec == 0)
    {
        LARGE_INTEGER f;
        if (QueryPerformanceFrequency(&f) && f.QuadPart)
            frec = f.QuadPart;
        else
            frec = 49875000;      /* respaldo: timebase del Xenon */
    }
    if (frames == 0)
        frames = 1;
    return (unsigned)(((tb * 1000000ULL) / (unsigned long long)frec) / frames);
}
#endif /* XBOX64_TRAZAS */

/* ★ DOS SONDAS QUE NO DISTINGUIAN NADA, Y POR QUE.  Anotado porque las dos
 * parecian razonables y las dos costaron un ciclo de prueba en hardware:
 *
 *   `pc` leido DESPUES de xbox64_run_frame -> salia SIEMPRE 80000180, con
 *   el juego corriendo o parado.  En `gen_interupt` la interrupcion de VI
 *   se entrega en `chk_status(1)` (interupt.c:399), que salta al vector de
 *   excepcion general, y eso pasa antes de que run_frame vuelva.  Ahora se
 *   muestrea DENTRO de xbox64_frame_boundary, que es antes de esa entrega.
 *
 *   `Count` -> salia SIEMPRE el mismo numero, 48665520.  No porque el
 *   invitado repita: porque el VI se programa EN UNIDADES DE Count
 *   (interupt.c: `next_vi = ... + vi_register.vi_delay`), asi que el
 *   incremento por frame es constante POR CONSTRUCCION.  Medir el reloj
 *   con el que programas la alarma no dice nada del que corre.
 *
 * La regla, otra vez: una sonda que da el mismo valor cuando la hipotesis
 * es cierta y cuando es falsa no es una sonda.  Lo que se mide ahora son
 * los contadores de plugins_fase1.c, que cuentan cosas que SOLO ocurren si
 * el juego progresa. */

/* ====================================================================== */
/* Log                                                                    */
/* ====================================================================== */

/* ★★ EL EMBUDO DEL LOG TIENE UN TOPE DE 128 BYTES Y NOS LO HEMOS COMIDO TODA
 * LA SESION SIN DARNOS CUENTA.
 *
 * `log_cb` es `retro_log_printf` de Salvia (src/salvia.h:402-408):
 *
 *     const unsigned int MAX_BUFFER = 128;
 *     char buffer[MAX_BUFFER] = {0};
 *     _vsnprintf_s(buffer, MAX_BUFFER, _TRUNCATE, fmt, args);
 *
 * O sea que TODO lo que pase de 127 caracteres se TIRA EN SILENCIO, contando
 * el "\n" que le anadimos aqui.  La prueba estaba delante en cada log:
 *
 *     ...comb=25 | tcupd=577 tccrc=791 tcloa[GLN64H] crc: 6860 llamadas...
 *                                          ^^^^^ cortado a 127 exactos
 *
 * `tcload` no salia NUNCA en las lineas de juego, y yo lo habia dado por
 * "truncado por el visor".  No: se perdia aqui.  Todas las medidas de
 * `[GLN64T]` de esta sesion estan leidas de lineas mutiladas.
 *
 * ★ SE TROCEA, NO SE TRUNCA.  Truncar seria repetir el fallo con otro nombre;
 * lo que hace falta es que no se pierda nada.  Cada trozo sale con un `+` al
 * principio para que se vea que es continuacion y no una linea nueva.
 *
 * ★ Y `linea` puede ser ARBITRARIAMENTE larga (los volcados de HLSL de
 * `[TEVHLSL]` pasan de 500), asi que el bucle no supone nada sobre el
 * tamano de entrada. */
#define XBOX64_LOG_TROZO 118   /* 127 - "\n" - el "+" - margen */

extern "C" void xbox64_log_line(const char *linea)
{
    size_t n;

#if !XBOX64_TRAZAS
    /* Version publica: el sumidero de TODAS las trazas queda vacio
     * (source/xenon64_diag.h).  Los que llaman estan gateados ellos mismos. */
    (void)linea;
    return;
#endif
    if (!linea)
        return;

    n = strlen(linea);

    if (!log_cb)
    {
        OutputDebugStringA(linea);
        return;
    }

    if (n <= XBOX64_LOG_TROZO)
    {
        log_cb(RETRO_LOG_INFO, "%s\n", linea);
        return;
    }

    {
        char trozo[XBOX64_LOG_TROZO + 2];
        size_t i = 0;
        int    primero = 1;
        while (i < n)
        {
            size_t c = n - i;
            if (c > (size_t)XBOX64_LOG_TROZO) c = (size_t)XBOX64_LOG_TROZO;
            memcpy(trozo, linea + i, c);
            trozo[c] = '\0';
            log_cb(RETRO_LOG_INFO, primero ? "%s\n" : "+%s\n", trozo);
            primero = 0;
            i += c;
        }
    }
}

static void logf(const char *fmt, ...)
{
#if XBOX64_TRAZAS   /* version publica: vacia (source/xenon64_diag.h) */
    char    buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    xbox64_log_line(buf);
#else
    (void)fmt;
#endif
}

/* ====================================================================== */
/* El framebuffer del VI                                                  */
/* ====================================================================== */

/*
 * Lee el framebuffer que el invitado tiene programado en el VI y lo pasa a
 * XRGB8888.
 *
 * ★ EL BYTESWAP.  `rdram` es un array de `unsigned long` y el Xenon es
 * big-endian, igual que el R4300: una palabra de 32 bits de la RDRAM se lee
 * tal cual, sin voltear.  Es una de las pocas cosas que este port NO tiene
 * que arreglar, y conviene dejarlo escrito porque la tentacion de meter un
 * swap "por si acaso" es exactamente el fallo que produce colores raros.
 *
 * ★ EL MODO DE 16 BITS DE N64 ES RGBA5551, NO RGB565.  El bit bajo es el
 * alfa (o "coverage"), no el bit bajo del azul.  Tomarlo por 565 da una
 * imagen verdosa y desplazada.
 */
#if XBOX64_GFX_NULO
static void pinta_framebuffer_vi(void)
{
    unsigned       tipo   = (unsigned)(vi_register.vi_status & 3);
    unsigned       origin = (unsigned)(vi_register.vi_origin & 0x00FFFFFF);
    unsigned       ancho  = (unsigned)(vi_register.vi_width & 0xFFF);
    unsigned       alto;
    unsigned       x, y;
    const unsigned char *base;

    memset(s_fb, 0, FB_ANCHO * FB_ALTO * 4);

    /* tipo: 0 = en blanco, 1 = reservado, 2 = 16 bits, 3 = 32 bits */
    if (tipo >= 2 && ancho && ancho <= FB_ANCHO)
    {
        /* El VI no dice la altura: se deduce del escalado vertical.
         * vi_y_scale lleva el factor en 2.10, y 0x200 = 1.0 sobre 240
         * lineas.  Es la cuenta que hacen todos los plugins. */
        alto = (unsigned)(((vi_register.vi_y_scale & 0xFFF) * 240) >> 10);
        if (alto == 0 || alto > FB_ALTO)
            alto = 240;

        base = (const unsigned char *)rdram + origin;

        for (y = 0; y < alto; y++)
        {
            unsigned *dst = s_fb + y * FB_ANCHO;

            if (tipo == 2)
            {
                const unsigned short *src =
                    (const unsigned short *)(base + y * ancho * 2);
                for (x = 0; x < ancho; x++)
                {
                    unsigned short p = src[x];
                    unsigned r = (p >> 11) & 0x1F;
                    unsigned g = (p >>  6) & 0x1F;
                    unsigned b = (p >>  1) & 0x1F;
                    /* 5 bits -> 8 replicando los altos, no desplazando:
                     * 0x1F tiene que dar 0xFF, no 0xF8. */
                    r = (r << 3) | (r >> 2);
                    g = (g << 3) | (g >> 2);
                    b = (b << 3) | (b >> 2);
                    dst[x] = (r << 16) | (g << 8) | b;
                }
            }
            else
            {
                const unsigned *src = (const unsigned *)(base + y * ancho * 4);
                for (x = 0; x < ancho; x++)
                    dst[x] = src[x] >> 8;   /* RGBA8888 -> 0RGB8888 */
            }
        }
    }

    /* El latido.  Ver la cabecera del fichero: sin esto, "parado" y
     * "corriendo con la pantalla en negro" son indistinguibles. */
    {
        unsigned px = s_frames % FB_ANCHO;
        unsigned i;
        for (i = 0; i < 4 && i < FB_ALTO; i++)
            s_fb[i * FB_ANCHO + px] = 0x00FF0000;
    }
}
#endif /* XBOX64_GFX_NULO */

/* El resumen por segundo entero (y los informes de los otros modulos que
 * llama) solo existe con trazas: version publica = ni se compila. */
#if XBOX64_TRAZAS
static void resumen_por_segundo(void)
{
    DWORD ahora = GetTickCount();

    if (s_t_resumen == 0)
    {
        s_t_resumen = ahora;
        return;
    }
    if (ahora - s_t_resumen < 1000)
        return;

    {
        unsigned pcmin, pcmax;
        xbox64_pc_rango(&pcmin, &pcmax);

        /* ★★ LOS CONTADORES PRIMERO: son los unicos que distinguen "el
         * emulador va bien y no hay quien dibuje" de "el juego esta
         * atascado".  Si dlist sube, la fase 2 empieza limpia. */
        /* ★ En la fase 2, `dlist` y `rdp` se quedan a cero: los contaba
         * xbox/plugins_fase1.c, y con el plugin dentro quien define
         * processDList/processRDPList es glN64_GX.  No es un sintoma.  Lo
         * que hay que mirar ahora es `img`. */
        /* ★ `vi_origin` SE LEE DEL REGISTRO, no de una copia.  Antes venia de
         * `s_ultimo_origin`, que solo escribia `pinta_framebuffer_vi()` -- que
         * en la fase 2 esta compilado fuera.  O sea que el campo salia
         * 000000 SIEMPRE y no era un dato: era una sonda muerta, igual que
         * `dlist`, pero sin avisar de que lo era.  Se lee de
         * `vi_register` como los otros dos campos de la misma linea. */
        logf("[xbox64] frames=%u img=%u vueltas=%u | dlist=%u alist=%u "
             "rdp=%u vista=%u keys=%u | pc=%08X..%08X | vi_origin=%06X w=%u st=%04X",
             s_frames - s_frames_prev,
             s_img,
             xbox64_vueltas_y_limpia(),
             xbox64_n_dlist, xbox64_n_alist, xbox64_n_rdp,
             xbox64_n_vista, xbox64_n_keys,
             pcmin, pcmax,
             (unsigned)(vi_register.vi_origin & 0x00FFFFFF),
             (unsigned)(vi_register.vi_width & 0xFFF),
             /* [Doom 64] El registro ENTERO y en hex, no solo el tipo
              * (`& 3`): el bit 3 (0x8) es la gamma del VI, que GLideN64
              * aplica (PostProcessor.cpp:156) y glN64/Rice no. */
             (unsigned)(vi_register.vi_status & 0xFFFF));

        /* ★★★ LA ENTREGA, Y ES LA SONDA DE LOS MICROCORTES.
         *
         * `img` por debajo de 30 no distingue "el juego no dibujo ese frame"
         * de "lo dibujo y lo tiramos".  Estas cuatro cifras si:
         *
         *   nuevas    tomas que se llevaron una imagen nueva
         *   repite    tomas que no encontraron nada (el frontend repite)
         *   ★ perd    imagenes PRODUCIDAS Y TIRADAS: dos resolves entre dos
         *             tomas, y el segundo ya habia pisado al primero
         *   seq       numero de resolve absoluto; su delta por segundo tiene
         *             que salir igual a `nuevas + perd`
         *
         * MEDIDO antes de esta sonda (log 3, Mario 64 con Rice): `img<30` en
         * 40 segundos de 94 pero `run_max` solo pasa de 16.667 us en 17, y
         * `late` se queda en 0,15/120.  O sea que la imagen se pierde SIN
         * que el frame se pase de tiempo y sin que el frontend lo note.
         * Ningun contador de los que hay lo explica; este si.
         *
         * Si `perd` sale 0, esta via queda descartada y el que no dibuja es
         * el juego -- que tambien es una respuesta. */
        logf("[ENTREGA] nuevas=%u repite=%u perd=%u | seq=%u",
             xenon64_vh_nuevas, xenon64_vh_repite,
             xenon64_vh_perdidas, xenon64_vh_resuelve);
        xenon64_vh_nuevas = xenon64_vh_repite = xenon64_vh_perdidas = 0;

        /* ★★ EL PRESUPUESTO, QUE ES LA PREGUNTA DE VERDAD.
         *
         * `libre` NO es un tramo medido: es lo que sobra del presupuesto
         * despues de restar lo que SI se ha medido, y se imprime aparte
         * justo para que no se confunda con una medida.  Si sale grande,
         * el emulador no es el cuello y no hay nada que optimizar aqui;
         * si sale cerca de cero, entonces `[GLN64T]` dice donde. */
        {
            /* ★★ TODO POR FRAME, contra los 16.667 us que dura uno a 60 Hz.
             * Ese numero es la unica referencia que hace falta: si la suma
             * lo pasa, no cabemos; si no, sobra. */
            unsigned nf   = s_frames - s_frames_prev;
            unsigned emu  = tb_a_us_frame(s_tb_emu,     nf);
            unsigned ent  = tb_a_us_frame(s_tb_entrega, nf);
            int      libre = 16667 - (int)(emu + ent);

#if XBOX64_PASO0
            /* ★ `max` ES EL NUMERO QUE DECIDE LOS 60 fps, no `emu`.  Si `max`
             * pasa de 16.667 hay frames que pagan un vblank entero, aunque la
             * media quepa de sobra.  Ver la nota de `s_tb_emu_max`.
             * Un solo frame: se divide entre 1, no entre nf. */
            /* ★ `cab` POR DIFERENCIA: todo lo de `retro_run` que no es ni
             * emulacion ni entrega.  Ver la nota de `s_tb_run`. */
            {
                unsigned run  = tb_a_us_frame(s_tb_run, nf);
                unsigned cab  = (run > emu + ent) ? (run - emu - ent) : 0;
                logf("[xbox64] reparto: emu=%u (max=%u) entrega=%u cab=%u "
                     "run=%u (max=%u) de 16667 us/frame  (libre=%d us, %d%%)",
                     emu, tb_a_us_frame(s_tb_emu_max, 1), ent, cab,
                     run, tb_a_us_frame(s_tb_run_max, 1),
                     libre, (int)((libre * 100) / 16667));
            }
#else
            logf("[xbox64] reparto: emu=%u entrega=%u de 16667 us/frame  "
                 "(libre=%d us, %d%%)  ★ emu INCLUYE el dibujado",
                 emu, ent, libre, (int)((libre * 100) / 16667));
#endif
            /* ★ El reparto de `entrega`, que sin partir no decide nada.  Sale
             * FUERA de `XBOX64_SONDAS` y de `XBOX64_PASO0` a proposito: es
             * parte de la medida de rendimiento, no una sonda de contenido, y
             * cuesta dos `__mftb()` por frame.
             *
             * Los tres tienen que SUMAR `entrega`.  Si no suman, hay un camino
             * que devuelve sin poner las marcas -- y entonces el reparto esta
             * midiendo otra cosa, no el bajon. */
            logf("[xbox64] entrega: audio=%u finframe=%u video=%u (de %u)",
                 tb_a_us_frame(s_tb_ent_audio, nf),
                 tb_a_us_frame(s_tb_ent_fin,   nf),
                 tb_a_us_frame(s_tb_ent_video, nf),
                 ent);

            /* ★ LOS DOS CANDIDATOS DE CPU->GPU, MEDIDOS ANTES DE TOCAR NADA.
             *
             * combw/nocombw: triangulos emitidos CON y SIN la matriz combinada
             *   de W.  Sin ella el lote divide por vertice en la CPU, asi que
             *   `nocombw` alto es geometria pagando a mano un divide que la
             *   GPU haria gratis.  El arreglo seria ensanchar el test de
             *   `_gSPUpdateCombW` (gSP.cpp:130), no mover codigo.
             *
             * conv/texconv: microsegundos y numero de texturas destiladas de
             *   formato GX a lineal.  En regimen vale 0; si en un bajon de los
             *   aislados se dispara, los picos de `emu max` (94-204 ms) son
             *   esto y entonces si tiene sentido pensar en hacerlo en GPU.
             *
             * Los cuatro contadores son por triangulo o por conversion, no por
             * vertice, asi que salen FUERA de `XBOX64_SONDAS`: no se pueden
             * medir con las sondas caras puestas, porque esas cambian justo lo
             * que se quiere medir. */
            /* ★ NO se reinician aqui `gxsh_conv_tb` ni `texconv`: con las
             * sondas encendidas los lee TAMBIEN `gxd3d_informe` mas abajo, y
             * ponerlos a cero ahora le dejaria ceros.  El reinicio va al
             * bloque general del final, que corre despues de los dos. */
            /* [Mario Tennis, pantalla VS] `modvtx` y `wcull` se contaban pero la
             * linea [GLN64MV] que los imprimia ya no existia: "no sale nunca"
             * no probaba nada.  Van aqui, por segundo.  xy = gSPModifyVertex
             * G_MWO_POINT_XYSCREEN (GLN64_MODIFYVTX_SCREEN decide si se
             * honra), z = ZSCREEN, wcull = triangulos tirados por w <= 0. */
            /* `fbrdram` = buffers NUEVOS cargados desde su RDRAM (GLideN64
             * 8b359761): mirados / pixeles pintados.  Coste en todos los juegos. */
            logf("[xbox64] cpu->gpu: combw=%u nocombw=%u | conv=%u us texconv=%u"
                 " | modvtx xy=%u z=%u wcull=%u | fbrdram %u px=%u",
                 gln64_dbg_combw, gln64_dbg_nocombw,
                 tb_a_us_frame(gxsh_conv_tb, nf), gxd3d_dbg_texconv,
                 gln64_dbg_modvtx_xy, gln64_dbg_modvtx_z, gln64_dbg_wcull,
                 gln64_dbg_fbrdram, gln64_dbg_fbrdram_px);
            gln64_dbg_combw = gln64_dbg_nocombw = 0;
            gln64_dbg_modvtx_xy = gln64_dbg_modvtx_z = gln64_dbg_wcull = 0;
            gln64_dbg_fbrdram = gln64_dbg_fbrdram_px = 0;

            /* ★ HERRAMIENTA FIJA (con trazas): LOS HILOS DEL JUEGO CUANDO DEJA DE
             * MANDAR LISTAS.  Tras haber mandado alguna, el TERCER segundo
             * seguido sin tareas graficas, y otra vez cada 20 s mientras siga
             * asi: los OSThread de libultra con su cola, quien la apunta, la
             * tabla de eventos, los ultimos DMA del PI y las ultimas tareas del
             * RSP.  Ver xbox64_hilos_volcado (xbox/plataforma.c).  Nacio con
             * Resident Evil 2 (logs 10-14). */
            {
                static unsigned s_hlConGfx = 0, s_hlSinGfx = 0;
                if (xbox64_rsp_gfx_n != 0)
                {
                    s_hlConGfx = 1;
                    s_hlSinGfx = 0;
                }
                else if (s_hlConGfx)
                {
                    s_hlSinGfx++;
                    if (s_hlSinGfx == 3 || (s_hlSinGfx > 3 && (s_hlSinGfx - 3) % 20 == 0))
                        xbox64_hilos_volcado();
                }
            }

            gln64_prof_informe(nf);
            gxd3d_zcopia_informe();
#if XBOX64_PASO0
            gxd3d_informe(nf);
#endif

/* ★ A partir de aqui, todo lo que sale por segundo es SONDA.  Con
 * `XBOX64_SONDAS` a 0 (source/xenon64_diag.h) no se publica nada de esto y en
 * el log quedan solo `frames=` y `reparto:`, que son la medida de rendimiento
 * de verdad.  Los contadores que lo alimentan tambien se apagan: ver la nota
 * del maestro -- `GLN64_PROF_CNT(vtxn)` corre POR VERTICE. */
#if XBOX64_SONDAS

            /* ★★ PASO 0 DEL HILO GRAFICO: cuanto cuesta cada tarea del RSP.
             *
             * `gfx` tiene que cuadrar con la suma de `[GLN64T]` -- es un
             * control positivo de que el reloj esta bien puesto.  `aud` es
             * el dato NUEVO: el coste de la tarea de audio, que hasta ahora
             * vivia sin separar dentro de "CPU emulada + audio" y es lo que
             * decide si el hilo de audio merece la pena.
             *
             * `hw` es en que hilo hardware corre la emulacion.  Salvia pinea
             * su bucle a CPU_THREAD=1 (src/engine.cpp:49) y deja libre el
             * NUCLEO 1 entero (HW2 y HW3); se imprime para confirmarlo en
             * ejecucion en vez de fiarse del codigo del frontend. */
            /* ★★ EL BLOQUE DE `[RSPT]` TAMBIEN VIVE YA FUERA DEL MAESTRO,
             * detras del `#endif`.  Motivo en su nota. */
#endif /* XBOX64_SONDAS */

            /* ★ El reparto por fase DENTRO de Rice.  Ver xenon/rice_prof.h. */
            rice_prof_informe(nf);

            /* El perfilador por funcion (/callcap) quedo DESCARTADO por
             * medida; esto es un no-op.  Ver xenon/perfil_cap.h. */
            perfil_cap_informe();

            /* ★★★ LA SECUENCIA POR FRAME.  Va ANTES del bloque `[RSPT]`
             * porque ese reinicia `xbox64_rsp_gfx_tb`, que es de donde sale
             * la serie `gfx`; asi el ultimo frame del segundo se vuelca con
             * su delta bueno en vez de con el acumulador ya a cero. */
            util_informe();

            /* [etapa 4] `[HILO]` y `[UTIL emu]`: el reparto visto desde los
             * dos lados.  `[UTIL run]` de arriba es lo que tarda cada
             * `retro_run` (lo que decide), `[UTIL emu]` lo que tarda cada VI
             * en el hilo del emulador (lo que se esta repartiendo).  Sin el
             * modo hilo no imprime nada. */
            hilo_emu_informe(nf);
            /* [etapa 5] `[LISTA]`: la lista en HW3.  Sin el modo, nada. */
            hilo_lista_informe(nf);

            /* ★★★ EL REPARTO DEL TRABAJO DEL RSP, FUERA DEL MAESTRO.
             *
             * Este es EL contador que atribuye tiempo por subsistema, y
             * llevaba todo el port apagado dentro de `#if XBOX64_SONDAS`
             * mientras yo deducia el reparto por RESTA -- que es justo el
             * error que este arbol ya tiene anotado para `[PERF] cpu`: un
             * residuo no es una medida, absorbe todo lo que no se mide.
             *
             *   gfx    el camino `do_SP_task` de la tarea GRAFICA: el plugin
             *          entero recorriendo la lista, medido desde fuera y sin
             *          inferencia.  Es la cifra de "cuanto cuestan los
             *          graficos".
             *   audio  las listas de audio del RSP.
             *   otras  el resto de tareas del RSP.
             *
             * Y `emu - (gfx + audio + otras)` SIGUE siendo un residuo, pero
             * ya es uno pequenyo y con nombre: la ejecucion del r4300.
             *
             * `gfx max` es la PEOR tarea grafica del segundo, entera: es el
             * numero que decide si un frame se pasa de presupuesto, no la
             * media. */
            {
                unsigned gus = tb_a_us_frame(xbox64_rsp_gfx_tb, nf);
                unsigned aus = tb_a_us_frame(xbox64_rsp_aud_tb, nf);
                unsigned ous = tb_a_us_frame(xbox64_rsp_otr_tb, nf);
                unsigned gmx = tb_a_us_frame(xbox64_rsp_gfx_max, 1);
                logf("[RSPT] us/frame: gfx=%u (%u/s, max=%u) audio=%u (%u/s) "
                     "otras=%u (%u/s) | hw=%u",
                     gus, xbox64_rsp_gfx_n, gmx, aus, xbox64_rsp_aud_n,
                     ous, xbox64_rsp_otr_n,
                     (unsigned)GetCurrentProcessorNumber());
                xbox64_rsp_gfx_max = 0;
                xbox64_rsp_gfx_tb = xbox64_rsp_gfx_n = 0;
                xbox64_rsp_aud_tb = xbox64_rsp_aud_n = 0;
                xbox64_rsp_otr_tb = xbox64_rsp_otr_n = 0;
            }

            /* [Xbox360] Aqui estaban `[SOLAPE]`, `[GFXTH]` y `[GFXTH2]`, los
             * informes del hilo grafico antiguo (quitado el 2026-09-30).  La
             * instrumentacion del reparto llega con la etapa 4. */

#if D3D_GRABADOR
            /* ★★ LA COLA DE ORDENES D3D (etapa 2: pasarela).
             *
             * Orden de lectura, y el primero no es negociable:
             *
             *   1. `ordenes` es el CONTADOR DE PASO.  Tiene que salir en los
             *      miles por segundo.  Si sale 0, las llamadas del shim NO
             *      estan pasando por el grabador y todo lo demas de estas
             *      dos lineas mide el vacio.
             *   2. `descon` tiene que ser 0: una orden que el ejecutor no
             *      sabe traducir se descarta, y en pantalla sale como un
             *      dibujo que falta.
             *   3. `gordas`, `llenos` y `pool` solo significan algo a partir
             *      del valor 2; en pasarela tienen que salir a 0.
             *
             * `[D3DQOP]` es el censo de ordenes RARAS (creaciones, resolves,
             * clears): son las que nadie mira y por tanto donde un fallo de
             * codificacion sobrevive. */
            /* ★ [etapa 3] Y la segunda linea, la de la etapa:
             *
             *   binds    -- CONTADOR DE PASO de la sombra.  Con D3DQ_BIND a 1
             *               tiene que salir igual que `frames` (uno por
             *               pasada).  Si sale 0, la sombra no se esta
             *               aplicando y un fallo visual no se le puede
             *               atribuir.
             *   rebinds  -- estados reaplicados por pasada (media).
             *   frame    -- lo que se graba en UN frame, el peor del
             *               segundo: ordenes y KB.  Por 3 (MAX_SWAP de
             *               mupen) es lo que tienen que aguantar el anillo
             *               (16.384 ranuras) y el pool (8 MB) en la etapa 4.
             *   maxcola  -- pico de ordenes pendientes al vaciar.
             *   gordas   -- tiene que ser 0: un payload que no cabe se pasa
             *               por puntero, y el bufer de preparacion se
             *               reutiliza antes de ejecutarse. */
            {
                char censo[256];
                logf("[D3DQ] ordenes=%u descon=%u gordas=%u llenos=%u"
                     " drenajes=%u pool=%u | valor=%d",
                     d3dq_dbg_ordenes, d3dq_dbg_descon, d3dq_dbg_gordas,
                     d3dq_dbg_llenos, d3dq_dbg_drenajes, d3dq_dbg_poolmax,
                     (int)D3D_GRABADOR);
                logf("[D3DQ3] bind=%d binds=%u rebinds=%u | frame: ord max=%u"
                     " KB max=%u | maxcola=%u | KB/s=%u ligsuelta=%u",
                     (int)D3DQ_BIND, d3dq_dbg_binds,
                     d3dq_dbg_binds ? d3dq_dbg_rebinds / d3dq_dbg_binds : 0,
                     s_q_ordmax, s_q_bytmax / 1024u, d3dq_dbg_maxcola,
                     d3dq_dbg_bytes / 1024u, d3dq_dbg_ligsuelta);
                d3dq_dbg_ligsuelta = 0;
                d3dq_censo(censo, (int)sizeof(censo));
                logf("%s", censo);
                d3dq_dbg_ordenes = d3dq_dbg_descon = d3dq_dbg_gordas = 0;
                d3dq_dbg_llenos = d3dq_dbg_drenajes = d3dq_dbg_poolmax = 0;
                d3dq_dbg_binds = d3dq_dbg_rebinds = 0;
                d3dq_dbg_bytes = d3dq_dbg_maxcola = 0;
                s_q_ordmax = s_q_bytmax = 0;
            }
#endif

        }

        s_tb_emu = s_tb_entrega = 0;
        s_tb_ent_audio = s_tb_ent_fin = s_tb_ent_video = 0;
        /* Aqui y no en la linea que los imprime: con las sondas encendidas
         * `gxd3d_informe` tambien los lee, y este bloque corre despues de los
         * dos.  Con las sondas apagadas ese informe sale antes de tocarlos,
         * asi que sin esto crecerian sin fin. */
        gxsh_conv_tb = 0;  gxd3d_dbg_texconv = 0;
        s_tb_emu_max = 0;
        s_tb_run = 0;  s_tb_run_max = 0;
        s_img = 0;
        xbox64_n_dlist = xbox64_n_alist = xbox64_n_rdp = 0;
        xbox64_n_vista = xbox64_n_keys = 0;
    }

    s_frames_prev = s_frames;
    s_t_resumen   = ahora;
}
#endif /* XBOX64_TRAZAS */

/* ====================================================================== */
/* API libretro                                                           */
/* ====================================================================== */

/* Definidas mas abajo, con la tabla de opciones al lado.  La declaracion
 * va aqui porque `retro_set_environment` es la PRIMERA funcion del API y
 * ya las necesita. */
static void publica_opciones(void);
static void aplica_opciones(void);
static void rumble_parar_todo(void);	/* mas abajo, junto a xbox64_rumble */

/* ★ LOS TIPOS DE MANDO POR PUERTO: un mando de N64 o nada.  Con esto el
 * frontend deja elegir en cada puerto si hay mando, y lo comunica con
 * `retro_set_controller_port_device` -> `xbox64_input_set_device`, que con
 * RETRO_DEVICE_NONE pone `Controls[puerto].Present` a FALSE: el PIF le
 * contesta al juego "no hay mando" (pif.c), igual que una consola con un solo
 * mando enchufado.  Antes no se anunciaba ningun tipo y el juego veia siempre
 * los cuatro.
 *
 * ★ EL MANDO VA PRIMERO a proposito: es el indice 0, y el que elige un
 * frontend que no sepa nada mas.  El estado de partida sigue siendo cuatro
 * enchufados (plataforma.c, input_info_init), asi que un frontend que nunca
 * llame a `retro_set_controller_port_device` se porta como antes. */
static const struct retro_controller_description s_tipos_mando[] = {
    { "Mando de Nintendo 64", RETRO_DEVICE_JOYPAD },
    { "Desconectado",         RETRO_DEVICE_NONE },
};

static const struct retro_controller_info s_puertos_mando[] = {
    { s_tipos_mando, 2 },
    { s_tipos_mando, 2 },
    { s_tipos_mando, 2 },
    { s_tipos_mando, 2 },
    { NULL, 0 }
};

void retro_set_environment(retro_environment_t cb)
{
    environ_cb = cb;

    /* ★ AQUI Y NO EN `retro_init`: el frontend llama a esto ANTES de
     * `retro_init`, y necesita la lista para poder pintar el menu de
     * opciones y para tener valores que devolver cuando el core pregunte
     * con GET_VARIABLE.  Publicarlas mas tarde funciona a medias -- el
     * menu sale vacio la primera vez -- y eso es de los fallos que se
     * achacan al frontend. */
    publica_opciones();

    /* Los tipos de mando, por la misma razon: el menu de mandos del frontend
     * se pinta antes de cargar el juego. */
    environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void *)s_puertos_mando);
}
void retro_set_video_refresh(retro_video_refresh_t cb)  { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)    { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb)        { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb)      { input_state_cb = cb; }

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    /* ★ SIN PARENTESIS NI ESPACIOS.  Salvia construye rutas con este
     * nombre, y con "xbox64 (Wii64)" salia en el log
     * `game:\data\states\xbox64 (Wii64)`.  Un directorio con parentesis y
     * espacio en el sistema de ficheros de la consola es pedir un fallo
     * que aparecera al guardar un estado, lejos de aqui. */
    info->library_name     = "xbox64";
    info->library_version  = "0.1-fase1";
    info->valid_extensions = "n64|v64|z64|bin|u1|ndd";
    /* ★★ LA ROM LA RESERVA Y LA LEE EL FRONTEND.
     *
     * Estuvo a `true` toda la fase 4: el core abria la ruta y ROM-Cache leia
     * por trozos de 64 KB.  Funcionaba, pero pagaba dos peajes que no hacian
     * falta -- con un .zip el frontend tenia que DESCOMPRIMIR A UN TEMPORAL
     * (`game:\tmp\extractedRom.z64`) para darnos una ruta, y el core dejaba
     * ese fichero abierto mientras cargaba, que es lo que impedia extraer el
     * siguiente juego encima.
     *
     * Con `false` el frontend entrega la ROM ya en RAM en
     * `retro_game_info.data` y aqui no se abre un solo fichero.  Lo que NO
     * cambia: ROM-Cache sigue igual, porque `filebrowser_xbox.c` le sirve el
     * bufer con los mismos `romFile_*`.  Eso conserva `init_byte_swap` /
     * `byte_swap`, o sea los .v64 y los .n64; cortocircuitar ROM-Cache los
     * habria roto en silencio (la .z64 sale bien y las otras dos no).
     *
     * ★ OJO AL TOPE DEL FRONTEND: `unzipOrLoad` (src/unzip/unziptool.h)
     * rechaza con errorCode -2 lo que pase de MAX_MEM_SIZE, y los cartuchos
     * de 64 MB (Conker, Resident Evil 2) lo pasan. */
    info->need_fullpath    = false;
    info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    memset(info, 0, sizeof(*info));
    info->geometry.base_width   = FB_ANCHO;
    info->geometry.base_height  = FB_ALTO;
    info->geometry.max_width    = FB_ANCHO;
    info->geometry.max_height   = FB_ALTO;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
    /* ★ 50 EN LOS JUEGOS PAL.  Era 60 fijo: con sincronizacion por audio no
     * se notaba (el audio PAL ya sale a su ritmo), pero los modos de video
     * (Sync::limit_fps) pacean a esta cifra, y un juego PAL (1080 o Beetle
     * Adventure Racing europeos) iba un 20 por ciento rapido.  Lo mismo que
     * mupen64plus-libretro (50/60 por el tipo de sistema de la ROM).  El
     * frontend lo pide tras `retro_load_game`, con la cabecera ya leida; sin
     * ROM, `GetVILimit` da 60.  Tambien lo reenvia el cambio de DACRATE (abajo). */
    info->timing.fps            = (double)GetVILimit();

    /* ★ LA FRECUENCIA DE VERDAD, NO UN 44100 DE ADORNO.
     *
     * El N64 no tiene una frecuencia fija: sale de dividir el reloj del
     * VI por el registro DACRATE que escribe el juego, asi que cada
     * cartucho pide la suya (32000, 22050, 44100... y alguno la cambia a
     * mitad de partida).  Declarar 44100 siempre obligaba al frontend a
     * remuestrear con una razon equivocada, que es afinacion incorrecta:
     * el juego suena agudo o grave y no se nota hasta compararlo.
     *
     * Antes de que el juego escriba el DACRATE esto devuelve el valor por
     * defecto; cuando lo escribe, `retro_run` reenvia el av_info con
     * RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO. */
    info->timing.sample_rate    = (double)xbox64_audio_frecuencia();
}

/* ====================================================================== */
/* MANDOS                                                                 */
/* ====================================================================== */
/* ★ EL PUENTE, Y ES LA UNICA LINEA QUE CRUZA.  `input_n64.c` sabe de N64
 * (el mapa de botones, el rango del stick) y no quiere saber de libretro;
 * aqui pasa lo contrario.  Se tocan solo aqui.
 *
 * La declaracion la trae `input_n64.h`, incluido arriba DENTRO del
 * `extern "C"`, asi que esta definicion hereda enlace C.  Sin eso, el
 * enlazador pediria el nombre decorado y nadie lo definiria -- es la
 * frontera que en la fase 3 costo tres oleadas de LNK2001. */
int xbox64_retro_input_state(unsigned puerto, unsigned device,
                             unsigned indice, unsigned id)
{
    if (!input_state_cb)
        return 0;
    return (int)input_state_cb(puerto, device, indice, id);
}

/* ★ El puente del audio, por el mismo motivo y con el mismo enlace C.
 *
 * `audio_batch_cb` puede aceptar MENOS frames de los que se le dan -- lo
 * dice su contrato, devuelve cuantos tomo -- asi que se reintenta hasta
 * colocarlos todos.  Si el frontend deja de tomar (devuelve 0), se corta:
 * insistir seria un bucle infinito dentro de `retro_run`. */
void xbox64_retro_audio_batch(const short *datos, unsigned frames)
{
    if (!audio_batch_cb || !datos)
        return;

    while (frames > 0)
    {
        size_t tomados = audio_batch_cb((const int16_t *)datos, frames);
        if (tomados == 0)
            break;
        if (tomados > frames)
            tomados = frames;
        datos  += tomados * 2;
        frames -= (unsigned)tomados;
    }
}

/* ★ LAS CONSTANTES QUE `input_n64.c` COPIA, COMPROBADAS CONTRA LAS DE
 * VERDAD.  Ese fichero es C89 y no incluye libretro.h a proposito, asi
 * que define a mano los IDs del RetroPad.  Si algun dia libretro.h los
 * cambiara, esto no compila en vez de mover los botones en silencio. */
typedef char xbox64_chk_input[
    (RETRO_DEVICE_ID_JOYPAD_B      ==  0 &&
     RETRO_DEVICE_ID_JOYPAD_Y      ==  1 &&
     RETRO_DEVICE_ID_JOYPAD_START  ==  3 &&
     RETRO_DEVICE_ID_JOYPAD_UP     ==  4 &&
     RETRO_DEVICE_ID_JOYPAD_DOWN   ==  5 &&
     RETRO_DEVICE_ID_JOYPAD_LEFT   ==  6 &&
     RETRO_DEVICE_ID_JOYPAD_RIGHT  ==  7 &&
     RETRO_DEVICE_ID_JOYPAD_A      ==  8 &&
     RETRO_DEVICE_ID_JOYPAD_X      ==  9 &&
     RETRO_DEVICE_ID_JOYPAD_L      == 10 &&
     RETRO_DEVICE_ID_JOYPAD_R      == 11 &&
     RETRO_DEVICE_ID_JOYPAD_L2     == 12 &&
     RETRO_DEVICE_NONE             ==  0 &&
     RETRO_DEVICE_JOYPAD           ==  1 &&
     RETRO_DEVICE_ANALOG           ==  5 &&
     RETRO_DEVICE_INDEX_ANALOG_LEFT  == 0 &&
     RETRO_DEVICE_INDEX_ANALOG_RIGHT == 1 &&
     RETRO_DEVICE_ID_ANALOG_X      ==  0 &&
     RETRO_DEVICE_ID_ANALOG_Y      ==  1) ? 1 : -1];


/* ★ LOS DESCRIPTORES SON PARA EL USUARIO, no para el core: son los
 * nombres que Salvia ensenya en su pantalla de remapeo.  A y B del N64 van
 * en A y B del RetroPad (xbox/input_n64.c), o sea en A y B del mando. */
static const struct retro_input_descriptor s_desc[] = {
#define XBOX64_DESC_PUERTO(P) \
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Cruceta arriba" },   \
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Cruceta abajo" },    \
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Cruceta izquierda" },\
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Cruceta derecha" },  \
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "A" },                \
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },                \
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "Z" },                \
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },                \
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },                \
    { P, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },            \
    { P, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,                        \
         RETRO_DEVICE_ID_ANALOG_X, "Stick X" },                                      \
    { P, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,                        \
         RETRO_DEVICE_ID_ANALOG_Y, "Stick Y" },                                      \
    { P, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,                       \
         RETRO_DEVICE_ID_ANALOG_X, "Botones C X" },                                  \
    { P, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,                       \
         RETRO_DEVICE_ID_ANALOG_Y, "Botones C Y" }
    XBOX64_DESC_PUERTO(0),
    XBOX64_DESC_PUERTO(1),
    XBOX64_DESC_PUERTO(2),
    XBOX64_DESC_PUERTO(3),
#undef XBOX64_DESC_PUERTO
    { 0, 0, 0, 0, NULL }
};

/* ====================================================================== */
/* CORE OPTIONS                                                           */
/* ====================================================================== */
/* ★ V2 Y NO `SET_VARIABLES`: Salvia lo soporta (salvia.cpp:468) y da
 * descripcion larga por opcion, que es donde cabe explicar QUE hace cada
 * una.  Con el formato clasico solo hay una etiqueta y el usuario tiene
 * que adivinar.
 *
 * ★ Y HAY RESERVA: si el frontend rechaza V2 se cae a `SET_VARIABLES`,
 * que entiende cualquiera.  Son diez lineas y evitan que el core salga
 * sin opciones en un frontend que no las soporte -- que es un fallo
 * silencioso, porque el core arranca igual. */
/* ★ QUE PLUGIN GRAFICO.  Los dos estan enlazados (ver xbox/gfx_dispatch.c) y
 * la eleccion se congela en `initiateGFX`, o sea al CARGAR la ROM.  Por eso
 * la descripcion lo dice explicitamente: una opcion que parece no hacer nada
 * hasta que recargas es peor que una opcion que avisa. */
#define OPT_PLUGIN  "xbox64_plugin"
#define OPT_FB      "xbox64_framebuffer"
#define OPT_SAI     "xbox64_2xsai"
#define OPT_CORE    "xbox64_cpu"
#define OPT_PAK     "xbox64_pak"
#define OPT_AUDIO   "xbox64_audio"
#define OPT_CPO     "xbox64_cpu_speed"
/* ★ El renderizado con hilo, al estilo de mupen64plus-libretro-nx.  Desde la
 * etapa 4 manda: la emulacion entera va en su hilo (xbox/hilo_emu.cpp) y
 * `retro_run` ejecuta las ordenes D3D.  Se lee AL CARGAR (ver
 * `s_modo_hilo`).  La clave es la de siempre, para no romper las opciones
 * guardadas.  Ver `s_opt_gfx_hilo` en xbox/plataforma.c. */
#define OPT_GFXH    "xbox64_gfx_thread"
/* [etapa 5] La display list en paralelo con la CPU, en HW3
 * (xbox/hilo_lista.h).  Solo manda con OPT_GFXH activado, y se lee AL
 * CARGAR: decide a que apunta el plugin en `initiateGFX`.  Apagada, la
 * etapa 4 tal cual: es el A/B dentro del mismo binario. */
#define OPT_LISTA   "xbox64_lista_paralelo"
/* [etapa 5] El SP_INT de la lista en paralelo al ACABAR la lista, y no al
 * despacharla.  Para los juegos que reutilizan sus buferes (vertices,
 * matrices) en cuanto el RSP "acaba": Cruis'n USA, logs 29-30, poligonos
 * donde no deben, como destellos.  `pisadas=0` en el log 30 descarta la
 * cabeza de la lista, no los vertices.  Cuesta paralelismo: por defecto
 * apagada.  Se aplica EN CALIENTE.  Ver xbox/hilo_lista.h. */
#define OPT_LISTA_SP "xbox64_lista_sp"
/* [Wii64 443f68b] Aqui estuvo OPT_ZRAM (`xbox64_depth_rdram`, "Borrado de
 * profundidad en RDRAM"), nuestro arreglo de Donkey Kong 64.  Lo sustituye
 * la escritura del borrado que trae ese commit en glN64_GX/gDP.cpp. */

/* ★ Los parches por juego de Wii64 (source/main/gamehacks.c), que `timers.c`
 * reaplica una vez por frame escribiendo en direcciones fijas de la RAM del
 * invitado.  Encendidos por defecto, como siempre han estado.
 *
 * Existe la opcion porque mientras un parche esta puesto TAPA el defecto que
 * arregla, y entonces no se puede ni mirar: el de Pilotwings 64 quitaba la
 * sombra de los aviones, asi que cualquier prueba sobre esa sombra daba igual
 * con el parche activo.  (Ese ya no existe: la sombra se emula, ver
 * gamehacks.c.) */
#define OPT_HACKS   "xbox64_gamehacks"

/* ★ La cache de texturas DEL SHIM (xenon/gx_d3d9.cpp, `s_texReg`), no la del
 * plugin.  Es una PALANCA DE DIAGNOSTICO: apagada, cada atado vuelve a
 * destilar la textura desde los texels del plugin, asi que si un defecto
 * visual desaparece al apagarla, lo que se estaba dibujando era una
 * convertida obsoleta.  Cuesta ~12.700 conversiones por segundo: no es un
 * modo de juego. */
#define OPT_TEXCACHE "xbox64_texcache"

/* ★ Marcado por CAMINO DE TEXTURA.  Pinta cada pixel con un color plano segun
 * la via por la que se destilo su textura, SIN muestrearla: sirve para senalar
 * en pantalla que objetos comparten camino.  La tabla de colores y el porque
 * de que sea un shader (y no un tinte del color de vertice) estan en
 * xenon/gln64_config.cpp. */
#define OPT_MARCATEX "xbox64_marcatex"

/* ★ [Pokemon Stadium, texto borroso] Como se filtran las TEXTURAS DE
 * FRAMEBUFFER en los triangulos.  Tres valores; `auto` deja el bilineal solo
 * en los juegos con hack_fbTextureOffset (Conker y Mario Tennis).  Se aplica
 * EN CALIENTE: el plugin la mira en cada atado.  Ver gln64_fb_bilineal en
 * xenon/gln64_config.cpp. */
#define OPT_FBBIL   "xbox64_fb_bilineal"

/* ★★ LAS OPCIONES EN DOS IDIOMAS: INGLES (`us`) Y ESPANOL (`local`).
 *
 * Se publican con SET_CORE_OPTIONS_V2_INTL (ver publica_opciones): `us` es
 * la tabla en ingles y `local` la espanola, solo si el frontend dice que su
 * idioma es el espanol (GET_LANGUAGE).  Salvia usa `local` si su idioma no
 * es el ingles y, sin ella, la `us` (salvia.cpp, SET_CORE_OPTIONS_V2_INTL).
 *
 * ★ LOS VALORES NO SE TRADUCEN, SOLO SUS ETIQUETAS.  La cadena de valor
 * ("activado", "recompilador", ...) es la que compara el core al leer la
 * opcion (lee_opcion y compania) y Salvia guarda el INDICE; cambiarla
 * romperia la lectura.  Lo que ve el usuario es la etiqueta, que va en las
 * dos tablas (Salvia no completa las etiquetas de la local con la `us`).
 *
 * ★ Las tildes del espanol, en UTF-8 por escapes para que el fuente siga
 * siendo ASCII.  Salvia pasa el texto por Constant::ansiToUtf8, que deja el
 * UTF-8 valido tal cual.  Cada macro es su propio literal, asi que un
 * caracter hexadecimal detras no se lo come el escape. */
#define ES_a "\xC3\xA1"   /* a con tilde */
#define ES_e "\xC3\xA9"   /* e con tilde */
#define ES_i "\xC3\xAD"   /* i con tilde */
#define ES_o "\xC3\xB3"   /* o con tilde */
#define ES_u "\xC3\xBA"   /* u con tilde */
#define ES_n "\xC3\xB1"   /* enye */

/* ★ LAS CATEGORIAS (submenus en Salvia, salvia.cpp + gestormenus.cpp).  Solo
 * cambian la VISTA: el guardado va por clave, asi que las opciones ya
 * guardadas siguen valiendo.  Salvia las ensenya en este orden. */
static const struct retro_core_option_v2_category s_opts_cats_en[] = {
    { "video",       "Video",
      "Graphics plugin, framebuffer and texture filtering." },
    { "sistema",     "System",
      "Emulated CPU, controller, sound and per-game patches." },
    { "rendimiento", "Performance",
      "Graphics thread and parallel display list." },
    { "diagnostico", "Diagnostics",
      "Tools to hunt down bugs; not gameplay modes." },
    { NULL, NULL, NULL }
};

static const struct retro_core_option_v2_category s_opts_cats_es[] = {
    { "video",       "V" ES_i "deo",
      "Plugin gr" ES_a "fico, framebuffer y filtrado de texturas." },
    { "sistema",     "Sistema",
      "CPU emulada, mando, sonido y parches por juego." },
    { "rendimiento", "Rendimiento",
      "Hilo gr" ES_a "fico y lista gr" ES_a "fica en paralelo." },
    { "diagnostico", "Diagn" ES_o "stico",
      "Herramientas para cazar fallos; no son modos de juego." },
    { NULL, NULL, NULL }
};

/* ---- INGLES (us) ------------------------------------------------------ */
static const struct retro_core_option_v2_definition s_opts_v2_en[] = {
    {
        OPT_PLUGIN, "Graphics plugin", NULL,
        "glN64 is the default: more accurate with effects and framebuffer. "
        "Rice is faster in games with a lot of geometry. "
        "APPLIED WHEN THE GAME LOADS: changing it during play does nothing "
        "until you load the game again.", NULL, "video",
        { { "glN64", "glN64" }, { "Rice", "Rice" }, { NULL, NULL } },
        "glN64"
    },
    {
        OPT_FB, "Framebuffer emulation", NULL,
        "Renders to texture so the game can read back what it has drawn. "
        "Paper Mario, mirrors and screen effects need it. "
        "It costs performance; if a game does not use it, it is not needed.",
        NULL, "video",
        { { "desactivado", "Disabled" }, { "activado", "Enabled" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_SAI, "2xSaI texture filter", NULL,
        "Smooths textures when they are loaded. Costs memory and loading time.",
        NULL, "video",
        { { "desactivado", "Disabled" }, { "activado", "Enabled" }, { NULL, NULL } },
        "desactivado"
    },
    {
        OPT_FBBIL, "Framebuffer texture filtering", NULL,
        "How the images the game draws and then uses as textures are smoothed "
        "(shadows, text in some menus). Auto: only in the games that need it "
        "(Conker's shadow, Mario Tennis). Enabled: whenever the game asks for "
        "it, like GLideN64; it may blur text (Pokemon Stadium). Disabled: "
        "never, like Wii64. Only has an effect with framebuffer emulation. "
        "Applied immediately.", NULL, "video",
        { { "auto", "Auto" }, { "activado", "Enabled" }, { "desactivado", "Disabled" },
          { NULL, NULL } },
        "auto"
    },
    {
        OPT_CORE, "CPU core", NULL,
        "The recompiler is three to four times faster. The interpreter is "
        "only useful to compare when a game misbehaves.",
        NULL, "sistema",
        { { "recompilador", "Recompiler" }, { "interprete", "Interpreter" },
          { NULL, NULL } },
        "recompilador"
    },
    {
        OPT_PAK, "Controller accessory", NULL,
        "Controller Pak saves games in the titles that ask for it. "
        "Rumble Pak makes the controller vibrate in the games that use it, "
        "but then there is nowhere to save on the Controller Pak (only one "
        "fits in the console). "
        "None behaves like a bare controller. Applied immediately.",
        NULL, "sistema",
        /* ★ "rumble pak" AL FINAL: Salvia guarda el INDICE del valor, y
         * meterlo en medio cambiaria lo que significa un "ninguno" guardado. */
        { { "controller pak", "Controller Pak" }, { "ninguno", "None" },
          { "rumble pak", "Rumble Pak" }, { NULL, NULL } },
        "controller pak"
    },
    {
        OPT_AUDIO, "Sound", NULL,
        "Turning it off does not speed up emulation: the game still "
        "generates the audio, it just stops being output.", NULL, "sistema",
        { { "activado", "Enabled" }, { "desactivado", "Disabled" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_CPO, "Emulated CPU speed", NULL,
        "How many instructions the game runs between two vertical retraces. "
        "'2' is the Wii64 value and gives it HALF of what the real hardware "
        "does: a game that sets its own pace can get stuck at 20 fps because "
        "of it. '1' is twice as fast and is what to try when a game runs at "
        "fewer fps than it should. '3' makes it slower, and some games need "
        "it to avoid breaking. Some games (Conker, Fighting Force) get '1' "
        "automatically while this is left at '2 (Wii64)'.", NULL, "sistema",
        { { "2 (Wii64)", "2 (Wii64)" }, { "1 (mas rapida)", "1 (faster)" },
          { "3 (mas lenta)", "3 (slower)" }, { NULL, NULL } },
        "2 (Wii64)"
    },
    {
        OPT_GFXH, "Graphics thread", NULL,
        "Emulation runs on its own thread, up to 3 frames ahead, and the "
        "drawing is spread across frames (mupen64plus's threaded renderer). "
        "It adds a little input lag. Applied when the game loads.",
        NULL, "rendimiento",
        { { "activado", "Enabled" }, { "desactivado", "Disabled" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_LISTA, "Parallel display list", NULL,
        "With the graphics thread enabled, walks the display list on another "
        "processor thread, in parallel with the emulated CPU, like the "
        "console's RSP. Helps the games that do not reach 60. Applied when "
        "the game loads.", NULL, "rendimiento",
        { { "activado", "Enabled" }, { "desactivado", "Disabled" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_LISTA_SP, "Parallel list: wait for the RSP", NULL,
        "With the parallel list, tells the game that the RSP has finished "
        "only once the list has really been drawn. Fixes the games that get "
        "stray polygons or flashes (Cruis'n USA) at the cost of some speed. "
        "Snapshot: instead of waiting for the list to be drawn, the thread "
        "copies the game's memory when it starts, signals as soon as the copy "
        "is done and draws from the copy; it may remove stutter, but whatever "
        "the plugin writes to memory does not reach the game. Applied "
        "immediately.", NULL, "rendimiento",
        { { "desactivado", "Disabled" }, { "activado", "Enabled" },
          { "foto", "Snapshot" }, { NULL, NULL } },
        "desactivado"
    },
    {
        OPT_HACKS, "Per-game patches", NULL,
        "The per-game fixes that come with Wii64 (gamehacks.c), applied once "
        "per frame by writing to fixed addresses in the game's memory. For "
        "example, the one that lets you get past the first level of Pokemon "
        "Snap. "
        "Enabled by default. Turn them off to see the bug a patch is hiding, "
        "or if you suspect a patch is causing more problems than it solves.",
        NULL, "sistema",
        { { "activado", "Enabled" }, { "desactivado", "Disabled" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_TEXCACHE, "Converted texture cache", NULL,
        "Keeps every texture already converted to the console's format so "
        "it is not redone on every draw. Turn it off only to diagnose: if a "
        "texture that looks wrong is fixed by turning it off, the bug is that "
        "an old version of that texture was being served. With it off the "
        "game runs VERY slowly.", NULL, "diagnostico",
        { { "activado", "Enabled" }, { "desactivado", "Disabled" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_MARCATEX, "Diagnostic marking", NULL,
        "Replaces the color of each surface to see where it gets lost. "
        "Combiner: one color for each color combiner the game uses. "
        "Shading: paints the vertex color as is, without texture. "
        "Texture: paints what the shader reads from the texture, nothing "
        "else. No clip: draws normally but without discarding pixels by "
        "their transparency. No fog: draws normally but without fog. "
        "Stage 0 to Stage 5: draws with the real combiner but stops it at "
        "that step, to see in which one the color is lost. Fog: paints in "
        "gray how much fog is applied at each spot. Depth: paints the depth "
        "at three scales (red whole range, green the last 10 percent, blue "
        "the last 1). No Z: draws normally but without depth test, face "
        "culling or near/far clipping; if something invisible shows up this "
        "way, depth or culling was losing it. Dump: draws normally and "
        "writes to the log every combiner in use, with its formula and "
        "colors (only in builds with traces). LLE: paints the RDP's "
        "low-level triangles in flat magenta. Texture alpha: paints the "
        "alpha of framebuffer textures in gray (black 0, white 1).",
        NULL, "diagnostico",
        { { "desactivado", "Disabled" }, { "combinador", "Combiner" },
          { "sombreado", "Shading" }, { "textura", "Texture" },
          { "sinclip", "No clip" }, { "sinniebla", "No fog" },
          { "etapa0", "Stage 0" }, { "etapa1", "Stage 1" }, { "etapa2", "Stage 2" },
          { "etapa3", "Stage 3" }, { "etapa4", "Stage 4" }, { "etapa5", "Stage 5" },
          { "niebla", "Fog" }, { "prof", "Depth" }, { "sinz", "No Z" },
          { "volcado", "Dump" }, { "lle", "LLE" }, { "texalfa", "Texture alpha" },
          { NULL, NULL } },
        "desactivado"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL }
};

/* ---- ESPANOL (local) -------------------------------------------------- */
/* ★ Mismas claves, mismos valores y mismo orden que la `us`: solo cambian
 * los textos y las etiquetas.  El valor por defecto lo toma Salvia de la
 * `us` (el de aqui da igual, se deja el mismo). */
static const struct retro_core_option_v2_definition s_opts_v2_es[] = {
    {
        OPT_PLUGIN, "Plugin gr" ES_a "fico", NULL,
        "glN64 es el de siempre: m" ES_a "s fiel en efectos y framebuffer. "
        "Rice es m" ES_a "s r" ES_a "pido en juegos con mucha geometr" ES_i "a. "
        "SE APLICA AL CARGAR EL JUEGO: si lo cambias durante la partida, no "
        "pasa nada hasta que vuelvas a cargar.", NULL, "video",
        { { "glN64", "glN64" }, { "Rice", "Rice" }, { NULL, NULL } },
        "glN64"
    },
    {
        OPT_FB, "Emulaci" ES_o "n de framebuffer", NULL,
        "Dibuja a textura para que el juego pueda leer lo que ha pintado. "
        "La necesitan Paper Mario, los espejos y los efectos de pantalla. "
        "Cuesta rendimiento; si un juego no la usa, sobra.", NULL, "video",
        { { "desactivado", "Desactivado" }, { "activado", "Activado" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_SAI, "Filtro de texturas 2xSaI", NULL,
        "Suaviza las texturas al cargarlas. Cuesta memoria y tiempo de carga.",
        NULL, "video",
        { { "desactivado", "Desactivado" }, { "activado", "Activado" }, { NULL, NULL } },
        "desactivado"
    },
    {
        OPT_FBBIL, "Filtrado de texturas de framebuffer", NULL,
        "C" ES_o "mo se suavizan las im" ES_a "genes que el juego pinta y luego "
        "usa como textura (sombras, texto de algunos men" ES_u "s). Auto: solo "
        "en los juegos que lo necesitan (la sombra de Conker, Mario Tennis). "
        "Activado: siempre que el juego lo pida, como GLideN64; puede "
        "emborronar textos (Pok" ES_e "mon Stadium). Desactivado: nunca, como "
        "Wii64. Solo tiene efecto con la emulaci" ES_o "n de framebuffer. Se "
        "aplica al momento.", NULL, "video",
        { { "auto", "Auto" }, { "activado", "Activado" }, { "desactivado", "Desactivado" },
          { NULL, NULL } },
        "auto"
    },
    {
        OPT_CORE, "N" ES_u "cleo de CPU", NULL,
        "El recompilador es entre tres y cuatro veces m" ES_a "s r" ES_a "pido. "
        "El int" ES_e "rprete solo sirve para comparar cuando un juego se porta "
        "mal.", NULL, "sistema",
        { { "recompilador", "Recompilador" }, { "interprete", "Int" ES_e "rprete" },
          { NULL, NULL } },
        "recompilador"
    },
    {
        OPT_PAK, "Accesorio del mando", NULL,
        "Controller Pak guarda partidas en los juegos que lo piden. "
        "Rumble Pak hace vibrar el mando en los juegos que lo usan, pero "
        "entonces no hay d" ES_o "nde guardar en el Controller Pak (en la "
        "consola solo cabe uno). "
        "Ninguno responde como un mando sin nada. Se aplica al momento.",
        NULL, "sistema",
        { { "controller pak", "Controller Pak" }, { "ninguno", "Ninguno" },
          { "rumble pak", "Rumble Pak" }, { NULL, NULL } },
        "controller pak"
    },
    {
        OPT_AUDIO, "Sonido", NULL,
        "Apagarlo no acelera la emulaci" ES_o "n: el juego genera el audio "
        "igual, solo deja de entregarse.", NULL, "sistema",
        { { "activado", "Activado" }, { "desactivado", "Desactivado" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_CPO, "Velocidad de la CPU emulada", NULL,
        "Cu" ES_a "ntas instrucciones ejecuta el juego entre dos retrazos. "
        "'2' es el valor de Wii64 y le da la MITAD que al hardware real: "
        "un juego que decide su ritmo solo puede quedarse en 20 fps por "
        "eso. '1' es el doble de r" ES_a "pido y es lo que hay que probar si "
        "un juego va a menos fps de los que deber" ES_i "a. '3' lo hace m" ES_a "s "
        "lento, y algunos juegos lo necesitan para no romperse. Algunos "
        "juegos (Conker, Fighting Force) reciben '1' autom" ES_a "ticamente "
        "mientras esta opci" ES_o "n est" ES_e " en '2 (Wii64)'.", NULL, "sistema",
        { { "2 (Wii64)", "2 (Wii64)" }, { "1 (mas rapida)", "1 (m" ES_a "s r" ES_a "pida)" },
          { "3 (mas lenta)", "3 (m" ES_a "s lenta)" }, { NULL, NULL } },
        "2 (Wii64)"
    },
    {
        OPT_GFXH, "Hilo gr" ES_a "fico", NULL,
        "La emulaci" ES_o "n va en su propio hilo, hasta 3 frames por delante, "
        "y el dibujado se reparte entre frames (el renderizado con hilo de "
        "mupen64plus). Sube algo el retardo del mando. Se aplica al cargar "
        "el juego.", NULL, "rendimiento",
        { { "activado", "Activado" }, { "desactivado", "Desactivado" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_LISTA, "Lista gr" ES_a "fica en paralelo", NULL,
        "Con el hilo gr" ES_a "fico activado, recorre la display list en otro "
        "hilo del procesador, en paralelo con la CPU emulada, como el RSP "
        "de la consola. Ayuda en los juegos que no llegan a 60. Se aplica "
        "al cargar el juego.", NULL, "rendimiento",
        { { "activado", "Activado" }, { "desactivado", "Desactivado" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_LISTA_SP, "Lista en paralelo: esperar al RSP", NULL,
        "Con la lista en paralelo, avisa al juego de que el RSP ha terminado "
        "solo cuando la lista est" ES_a " dibujada de verdad. Arregla los "
        "juegos a los que se les cuelan pol" ES_i "gonos o destellos (Cruis'n "
        "USA) a cambio de algo de velocidad. Foto: en vez de esperar a que la "
        "lista se dibuje, el hilo copia la memoria del juego al empezarla, "
        "avisa en cuanto acaba la copia y dibuja desde la copia; puede quitar "
        "los tirones, pero lo que el plugin escriba en la memoria no le llega "
        "al juego. Se aplica al momento.", NULL, "rendimiento",
        { { "desactivado", "Desactivado" }, { "activado", "Activado" },
          { "foto", "Foto" }, { NULL, NULL } },
        "desactivado"
    },
    {
        OPT_HACKS, "Parches por juego", NULL,
        "Los arreglos por juego que trae Wii64 (gamehacks.c), aplicados una "
        "vez por frame escribiendo en direcciones fijas de la memoria del "
        "juego. Por ejemplo, el que deja pasar el primer nivel de Pok" ES_e "mon "
        "Snap. "
        "Encendidos por defecto. Ap" ES_a "galos para ver el defecto que tapa un "
        "parche, o si sospechas que un parche est" ES_a " causando m" ES_a "s "
        "problemas de los que resuelve.", NULL, "sistema",
        { { "activado", "Activado" }, { "desactivado", "Desactivado" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_TEXCACHE, "Cach" ES_e " de texturas convertidas", NULL,
        "Guarda cada textura ya convertida al formato de la consola para no "
        "rehacerla en cada dibujo. Ap" ES_a "gala solo para diagnosticar: si una "
        "textura que se ve mal se arregla al apagarla, el fallo es que se "
        "estaba sirviendo una versi" ES_o "n antigua de esa textura. Apagada el "
        "juego va MUY lento.", NULL, "diagnostico",
        { { "activado", "Activado" }, { "desactivado", "Desactivado" }, { NULL, NULL } },
        "activado"
    },
    {
        OPT_MARCATEX, "Marcado de diagn" ES_o "stico", NULL,
        "Sustituye el color de cada superficie para ver d" ES_o "nde se pierde. "
        "Combinador: un color por cada combinador de color que usa el juego. "
        "Sombreado: pinta el color de v" ES_e "rtice tal cual, sin textura. "
        "Textura: pinta lo que el sombreador lee de la textura, sin nada m" ES_a "s. "
        "Sin clip: dibuja normal pero sin descartar p" ES_i "xeles por su "
        "transparencia. Sin niebla: dibuja normal pero sin aplicar la niebla. "
        "Etapa 0 a Etapa 5: dibuja con el combinador real pero par" ES_a "ndolo "
        "en ese paso, para ver en cu" ES_a "l se pierde el color. Niebla: pinta "
        "en gris cu" ES_a "nta niebla se aplica en cada sitio. Profundidad: "
        "pinta la profundidad en tres escalas (rojo entera, verde el " ES_u "ltimo "
        "10 por ciento, azul el " ES_u "ltimo 1). Sin Z: dibuja normal pero sin "
        "test de profundidad, sin descartar caras y sin recortar por cerca o "
        "lejos; si algo invisible aparece as" ES_i ", lo perd" ES_i "a la "
        "profundidad o el descarte de caras. Volcado: dibuja normal y escribe "
        "en el log cada combinador que se usa, con su f" ES_o "rmula y sus "
        "colores (solo en compilaciones con trazas). LLE: pinta de magenta "
        "plano los tri" ES_a "ngulos de bajo nivel del RDP. Alfa de textura: "
        "pinta en gris el alfa de las texturas de framebuffer (negro 0, "
        "blanco 1).",
        NULL, "diagnostico",
        { { "desactivado", "Desactivado" }, { "combinador", "Combinador" },
          { "sombreado", "Sombreado" }, { "textura", "Textura" },
          { "sinclip", "Sin clip" }, { "sinniebla", "Sin niebla" },
          { "etapa0", "Etapa 0" }, { "etapa1", "Etapa 1" }, { "etapa2", "Etapa 2" },
          { "etapa3", "Etapa 3" }, { "etapa4", "Etapa 4" }, { "etapa5", "Etapa 5" },
          { "niebla", "Niebla" }, { "prof", "Profundidad" }, { "sinz", "Sin Z" },
          { "volcado", "Volcado" }, { "lle", "LLE" }, { "texalfa", "Alfa de textura" },
          { NULL, NULL } },
        "desactivado"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL }
};

static const struct retro_core_options_v2 s_opts_v2_en_tabla = {
    (struct retro_core_option_v2_category *)s_opts_cats_en,
    (struct retro_core_option_v2_definition *)s_opts_v2_en
};

static const struct retro_core_options_v2 s_opts_v2_es_tabla = {
    (struct retro_core_option_v2_category *)s_opts_cats_es,
    (struct retro_core_option_v2_definition *)s_opts_v2_es
};

/* La reserva para un frontend sin V2: en ingles, el idioma comun.  Los
 * valores son los de siempre (son las claves que lee el core). */
static const struct retro_variable s_opts_v1[] = {
    { OPT_PLUGIN, "Graphics plugin (applied when the game loads); glN64|Rice" },
    { OPT_FB,    "Framebuffer emulation; activado|desactivado" },
    { OPT_SAI,   "2xSaI texture filter; desactivado|activado" },
    { OPT_FBBIL, "Framebuffer texture filtering; auto|activado|desactivado" },
    { OPT_CORE,  "CPU core; recompilador|interprete" },
    { OPT_PAK,   "Controller accessory; controller pak|ninguno|rumble pak" },
    { OPT_AUDIO, "Sound; activado|desactivado" },
    { OPT_CPO,   "Emulated CPU speed; 2 (Wii64)|1 (mas rapida)|3 (mas lenta)" },
    { OPT_GFXH,  "Graphics thread; activado|desactivado" },
    { OPT_LISTA, "Parallel display list; activado|desactivado" },
    { OPT_LISTA_SP, "Parallel list: wait for the RSP; desactivado|activado|foto" },
    { OPT_HACKS, "Per-game patches; activado|desactivado" },
    { OPT_TEXCACHE, "Converted texture cache; activado|desactivado" },
    { OPT_MARCATEX, "Diagnostic marking; desactivado|combinador|sombreado|textura|sinclip|sinniebla|etapa0|etapa1|etapa2|etapa3|etapa4|etapa5|niebla|prof|sinz|volcado|lle|texalfa" },
    { NULL, NULL }
};

static void publica_opciones(void)
{
    unsigned version = 0;
    unsigned idioma  = RETRO_LANGUAGE_ENGLISH;
    struct retro_core_options_v2_intl intl;

    /* La tabla local solo si el frontend esta en espanol; si no, NULL y el
     * frontend usa la `us` (ingles). */
    if (!environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &idioma))
        idioma = RETRO_LANGUAGE_ENGLISH;
    intl.us    = (struct retro_core_options_v2 *)&s_opts_v2_en_tabla;
    intl.local = (idioma == RETRO_LANGUAGE_SPANISH)
               ? (struct retro_core_options_v2 *)&s_opts_v2_es_tabla : NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) &&
        version >= 2)
    {
        if (environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL, &intl))
            return;
        /* Un frontend V2 sin la variante INTL: la tabla del idioma, sin mas. */
        if (environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
                       intl.local ? (void *)intl.local : (void *)intl.us))
            return;
    }

#if XBOX64_TRAZAS
    logf("[xbox64] el frontend no acepta core options V2; se usa el formato clasico");
#endif
    environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)s_opts_v1);
}

/* Devuelve 1 si el valor es `si`, 0 si no, y `porDefecto` si el frontend
 * no contesta.  ★ Un `GET_VARIABLE` que falla NO es "desactivado": es
 * "no se sabe", y tratarlo como cero apagaria opciones que el usuario
 * habia encendido. */
static int lee_opcion(const char *clave, const char *si, int porDefecto)
{
    struct retro_variable v;
    v.key   = clave;
    v.value = NULL;

    if (!environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) || !v.value)
        return porDefecto;

    return (strcmp(v.value, si) == 0) ? 1 : 0;
}

/* Lee las cinco y las aplica.  Se llama en `retro_load_game` (antes de
 * cargar, que es cuando las de glN64 tienen efecto) y en `retro_run`
 * cuando el frontend avisa de un cambio. */
static void aplica_opciones(void)
{
    /* ★ Se aplica siempre, pero el despachador solo la mira en `initiateGFX`.
     * Ver xbox/gfx_dispatch.c: no es en caliente a proposito. */
    xbox64_gfx_plugin_set(lee_opcion(OPT_PLUGIN, "Rice", 0)
                          ? XBOX64_GFX_RICE : XBOX64_GFX_GLN64);

    int fb    = lee_opcion(OPT_FB,    "activado",       1);
    int sai   = lee_opcion(OPT_SAI,   "activado",       0);
    int dyna  = lee_opcion(OPT_CORE,  "recompilador",   1);
    /* Tres valores: PLUGIN_MEMPAK = 2 (por defecto), PLUGIN_NONE = 1 y
     * PLUGIN_RUMBLE_PAK = 3 (main/plugin.h:56-58).  Un valor que no se
     * reconozca se queda en el Controller Pak. */
    int pak = 2;
    {
        struct retro_variable v;
        v.key = OPT_PAK; v.value = NULL;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value)
        {
            if      (strcmp(v.value, "ninguno")    == 0) pak = 1;
            else if (strcmp(v.value, "rumble pak") == 0) pak = 3;
        }
    }
    /* Sin Rumble Pak, ningun motor puede quedarse encendido. */
    if (pak != 3)
        rumble_parar_todo();
    int audio = lee_opcion(OPT_AUDIO, "activado",       1);
    /* ★ Por defecto ENCENDIDO.  [etapa 4] Ya no es el hilo grafico antiguo
     * (el de la medida de Majora's Mask, quitado el 2026-09-30) sino el
     * renderizado con hilo de mupen.  NO se aplica en caliente: se guarda
     * aqui y `retro_load_game` lo lee al cargar (`s_modo_hilo`).  Sigue
     * siendo la palanca del A/B. */
    int gfxh  = lee_opcion(OPT_GFXH,  "activado",       1);
    /* [etapa 5] Se guarda y se usa al cargar (ver `s_opt_lista`). */
    s_opt_lista = lee_opcion(OPT_LISTA, "activado", 1);
    /* Tres valores: 0 en el acto, 1 al acabar la lista, 2 al acabar la FOTO
     * de la RDRAM (ver `hilo_lista_sp_al_acabar`).  Un valor que no se
     * reconozca se queda en 0, el de siempre. */
    s_opt_lista_sp = 0;
    {
        struct retro_variable v;
        v.key = OPT_LISTA_SP; v.value = NULL;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value)
        {
            if      (strcmp(v.value, "activado") == 0) s_opt_lista_sp = 1;
            else if (strcmp(v.value, "foto")     == 0) s_opt_lista_sp = 2;
        }
    }
    /* ★ EN CALIENTE (log 30: el usuario la cambio con el juego en marcha y,
     * al leerse solo al cargar, no hizo nada).  Es seguro: en partida esto se
     * llama con el emulador aparcado en el fin de un VI y el hilo de la lista
     * vacio (`hilo_emu_pausar`), asi que no hay ningun SP_INT a medias que
     * dependa de la bandera. */
    hilo_lista_sp_al_acabar(s_opt_lista_sp);
#if XBOX64_TRAZAS
    logf("[xbox64] lista en paralelo: SP_INT %s",
         s_opt_lista_sp == 2 ? "al acabar la FOTO de la RDRAM" :
         s_opt_lista_sp == 1 ? "al ACABAR la lista" : "en el acto (como mupen)");
#endif
    /* ★ Por defecto encendidos, que es como han estado siempre.  Se aplica
     * en caliente: `timers.c` consulta el puntero del parche cada frame. */
    int hacks = lee_opcion(OPT_HACKS, "activado",       1);
    /* En caliente: el shim la consulta en cada atado. */
    int texcache = lee_opcion(OPT_TEXCACHE, "activado", 1);
    /* ★ MUCHOS valores, no dos: se lee a mano igual que OPT_CPO.  Apagado por
     * defecto, y ante una respuesta que no reconozca se queda apagado -- el
     * marcado sustituye el color de TODA la escena, asi que un valor raro no
     * puede acabar en "encendido".  Los numeros son los `GLN64_MARCA_*` de
     * xenon/gln64_prof.h. */
    int marcatex = 0;
    {
        struct retro_variable v;
        v.key = OPT_MARCATEX; v.value = NULL;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value)
        {
            if      (strcmp(v.value, "combinador") == 0) marcatex = 2;
            else if (strcmp(v.value, "sombreado")  == 0) marcatex = 3;
            else if (strcmp(v.value, "textura")    == 0) marcatex = 4;
            else if (strcmp(v.value, "sinclip")    == 0) marcatex = 5;
            else if (strcmp(v.value, "sinniebla")  == 0) marcatex = 13;
            /* Ojo: `etapa0` NO es 8-1, va por su lado (14). */
            else if (strcmp(v.value, "etapa0")     == 0) marcatex = 14;
            else if (strcmp(v.value, "niebla")     == 0) marcatex = 15;
            else if (strcmp(v.value, "prof")       == 0) marcatex = 16;
            else if (strcmp(v.value, "sinz")       == 0) marcatex = 17;
            else if (strcmp(v.value, "volcado")    == 0) marcatex = 19;
            else if (strcmp(v.value, "lle")        == 0) marcatex = 20;
            else if (strcmp(v.value, "texalfa")    == 0) marcatex = 21;
            /* etapa1..etapa5 -> 8..12, que es lo que espera el generador. */
            else if (strncmp(v.value, "etapa", 5)  == 0 &&
                     v.value[5] >= '1' && v.value[5] <= '5')
                marcatex = 8 + (v.value[5] - '1');
            else                                         marcatex = 0;
        }
    }

    /* ★ ESTA NO ES BINARIA: tres valores.  `lee_opcion` solo sabe de dos,
     * asi que se lee a mano contra las dos alternativas al valor por
     * defecto -- y si el frontend no contesta, se queda en 2. */
    int cpo = 2;
    {
        struct retro_variable v;
        v.key = OPT_CPO; v.value = NULL;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value)
        {
            if      (v.value[0] == '1') cpo = 1;
            else if (v.value[0] == '3') cpo = 3;
            else                        cpo = 2;
        }
    }

    /* Tres valores tambien: 0 auto, 1 activado, 2 desactivado.  Un valor que
     * no se reconozca se queda en auto, el de por defecto. */
    int fbbil = 0;
    {
        struct retro_variable v;
        v.key = OPT_FBBIL; v.value = NULL;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value)
        {
            if      (strcmp(v.value, "activado")    == 0) fbbil = 1;
            else if (strcmp(v.value, "desactivado") == 0) fbbil = 2;
        }
    }

    xbox64_opciones(fb, sai, dyna, pak, cpo, gfxh, hacks);
    xbox64_audio_habilitar(audio);
    gxd3d_texcache_habilitar(texcache);
    gln64_marcaluz = marcatex;
    gln64_fb_bilineal = fbbil;

#if XBOX64_TRAZAS
    logf("[xbox64] opciones: fb=%d 2xsai=%d dynarec=%d pak=%d audio=%d cpo=%d "
         "gfxh=%d hacks=%d texcache=%d marcatex=%d fbbil=%d",
         fb, sai, dyna, pak, audio, cpo, gfxh, hacks, texcache, marcatex, fbbil);
#endif
}

/* ★ LA VIBRACION DEL RUMBLE PAK EN EL MANDO DE XBOX 360.  La llama el PIF
 * (source/gc_memory/pif.c, escritura en 0xC000 con PLUGIN_RUMBLE_PAK) desde
 * el hilo del emulador.  Como pcsxr-360 (input/PSXInput.cpp,
 * PSxInputSetVibration): XInputSetState con el numero de PUERTO del core como
 * indice de XInput, y solo cuando el estado cambia -- los juegos repiten la
 * misma orden muchas veces seguidas, y XInputSetState tiene latencia y compite
 * con las lecturas del mando.
 *
 * El motor del N64 es uno solo y de todo o nada: va al grande (izquierdo, baja
 * frecuencia) a tope, con el pequeno a la mitad para que se note el zumbido.
 *
 * -1 = "no se sabe que tiene el mando": obliga a mandar la siguiente orden. */
static volatile int s_rumble[4] = { -1, -1, -1, -1 };

extern "C" void xbox64_rumble(int puerto, int encendido)
{
    XINPUT_VIBRATION vib;
    if (puerto < 0 || puerto > 3)
        return;
    encendido = encendido ? 1 : 0;
    if (s_rumble[puerto] == encendido)
        return;
    s_rumble[puerto] = encendido;
    vib.wLeftMotorSpeed  = encendido ? 0xFFFF : 0;
    vib.wRightMotorSpeed = encendido ? 0x8000 : 0;
    XInputSetState((DWORD)puerto, &vib);
}

/* Apaga los cuatro: al descargar el juego, al apagar el core y al cambiar de
 * accesorio.  Sin esto un mando que vibraba al salir se queda vibrando. */
static void rumble_parar_todo(void)
{
    int i;
    for (i = 0; i < 4; i++)
    {
        s_rumble[i] = -1;
        xbox64_rumble(i, 0);
    }
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    if (port >= 4)
        return;
#if XBOX64_TRAZAS
    logf("[xbox64] mando puerto %u -> device %u%s",
         port, device, (device == RETRO_DEVICE_NONE) ? "  (desenchufado)" : "");
#endif
    xbox64_input_set_device(port, device);
    /* Un puerto que se desconecta no puede quedarse vibrando. */
    if (device == RETRO_DEVICE_NONE)
    {
        s_rumble[port] = -1;
        xbox64_rumble((int)port, 0);
    }
}

void retro_init(void)
{
    struct retro_log_callback logging;

    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;

#if XBOX64_TRAZAS
    logf("[xbox64] retro_init  (GFX_NULO=%d  DYNAREC=%d)",
         XBOX64_GFX_NULO, XBOX64_DYNAREC);
#endif

    /* Perfilador por funcion (/callcap).  Antes de crear el hilo grafico,
     * que es quien se registra como el hilo contabilizado. */
    perfil_cap_init();

    /* [Fase 6] El directorio `system`, que es de donde Rice_GX lee su
     * `RiceVideoLinux.ini` (su base de datos de arreglos por juego) y donde
     * escribe su `RiceVideo.cfg`.  El porque y el formato, en
     * xbox/plataforma.h.
     *
     * ★ SE PIDE AQUI Y NO EN `retro_load_game`, aunque solo lo use Rice al
     * cargar: `environ_cb` ya esta puesto y el valor no cambia en toda la
     * sesion.  Pedirlo en cada carga seria preguntar lo mismo N veces.
     *
     * ★ Y NO ES UN FALLO QUE EL FRONTEND NO LO DE.  `xbox64_set_system_dir`
     * deja la cadena vacia, `fopen` falla y Rice se queda con sus opciones
     * por defecto.  Se traza para que un "Rice se ve distinto" no cueste
     * media hora. */
    {
        const char *sysdir = NULL;

        if (environ_cb &&
            environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sysdir) &&
            sysdir && sysdir[0])
        {
            xbox64_set_system_dir(sysdir);
#if XBOX64_TRAZAS
            logf("[xbox64] system dir: %s", sysdir);
#endif
        }
        else
        {
            xbox64_set_system_dir(NULL);
#if XBOX64_TRAZAS
            logf("[xbox64] system dir: NO HAY "
                 "(Rice usara sus opciones por defecto)");
#endif
        }
    }

#if XBOX64_GFX_NULO
    s_fb = (unsigned *)malloc(FB_ANCHO * FB_ALTO * 4);
    if (s_fb)
        memset(s_fb, 0, FB_ANCHO * FB_ALTO * 4);
#endif

    /* [etapa 5] TRAZAS DE PASO del arranque (log 26: se corto justo despues
     * de `system dir` sin decir donde).  Una linea por hito, una vez. */
#if XBOX64_TRAZAS
    logf("[xbox64] arranque: xbox64_init...");
#endif
    xbox64_init();
#if XBOX64_TRAZAS
    logf("[xbox64] arranque: xbox64_init hecho; creando el hilo del emulador...");
#endif

    /* [etapa 4] El hilo del emulador, UNA vez por vida del core.  Se queda
     * ocioso hasta el primer `retro_run` de una partida con el modo hilo.
     * ★ Persistente a proposito: crear y destruir un hilo por carga colgaba
     * la consola dentro de `CreateThread` de forma intermitente en pcsxr-360
     * (libpcsxcore/gpu.c:1523-1549).  Si no se puede crear, se juega sin
     * hilo: es el camino de la etapa 3, que funciona. */
    s_hilo_ok = hilo_emu_crear();
#if XBOX64_TRAZAS
    logf("[xbox64] hilo del emulador: %s",
         s_hilo_ok ? "creado (HW2, ocioso hasta cargar)"
                   : "NO DISPONIBLE (se juega sin hilo)");
#endif

    /* [etapa 5] El de la lista NO se crea aqui: se crea al cargar, y solo si
     * la opcion `xbox64_lista_paralelo` esta activada (ver
     * `hilo_lista_configurar`).  Asi, con la opcion apagada no corre NI UNA
     * linea de la etapa 5, y ese es el control limpio del A/B. */
    s_lista_ok = 1;
}

void retro_deinit(void)
{
    /* Primero el hilo: puede estar emulando todavia si el frontend no llamo
     * a `retro_unload_game`, y lo que se suelta debajo es suyo.
     *
     * ★★ [log 36] SE APARCA, NO SE DESTRUYE.  Salvia llama a `retro_deinit`
     * y `retro_init` en CADA cambio de juego (logs 34, 35 y 37: un par por
     * carga), asi que destruirlo aqui era crear y destruir los dos hilos por
     * carga -- justo lo que colgaba `CreateThread` en pcsxr-360 y lo que la
     * nota de `retro_init` daba por evitado.  El log 36 se corta dentro de
     * `hilo_emu_crear`.  Aparcados, `hilo_emu_crear` y `hilo_lista_configurar`
     * los reutilizan: un `CreateThread` por hilo y por PROCESO.  Mueren con
     * el proceso cuando Salvia lanza otro ejecutable. */
    hilo_emu_parar();
    s_hilo_ok   = 0;
    s_modo_hilo = 0;
    /* [etapa 5] Despues del emulador: es quien le manda trabajo. */
    hilo_lista_configurar(0);
    s_lista_ok  = 0;

    /* Con el emulador parado: ya nadie puede volver a encender el motor. */
    rumble_parar_todo();

    xbox64_deinit();
#if D3D_GRABADOR
    /* El anillo y el pool (32 MB): se reservan en la primera carga y se
     * reutilizan en las siguientes; aqui se devuelven al sistema. */
    d3dq_fin();
#endif
#if XBOX64_GFX_NULO
    if (s_fb) { free(s_fb); s_fb = NULL; }
#endif
#if XBOX64_TRAZAS
    logf("[xbox64] retro_deinit");
#endif
}

/* [Xbox360] ★ [Gauntlet Legends, logs GL_B/GL_C y 20] JUEGOS QUE NO PUEDEN
 * LLEVAR LA LISTA EN PARALELO.
 *
 * Su display list salta a si misma (gSPBranchList) para dejar al RSP
 * esperando mientras la CPU escribe el trozo siguiente.  El plugin la corta
 * ahi y borra TASKDONE/HALT/BROKE para que `do_SP_task` (memory.c) la
 * relance; con la lista en otro hilo el emulador ya paso por ese punto, el
 * relanzamiento se pierde y el juego no arranca (`infloop=1`, en "en el
 * acto" y en "al acabar").  Es la salida que ya documenta RSP.cpp
 * ("LA SALIDA QUE PROHIBE EL HILO GRAFICO").  Ademas no ganaria nada: cada
 * trozo exige que la lista anterior haya acabado.
 *
 * Se decide aqui, por CRC de la cabecera, porque la lista en paralelo se
 * configura ANTES de cargar la ROM (`gamehacks.c` llega tarde).  Los CRC, de
 * mupen64plus.ini.  La cabecera se pasa a z64 (big-endian) desde .v64/.n64. */
static int rom_sin_lista_paralelo(const struct retro_game_info *game, const char **nombre)
{
    unsigned char h[0x18];
    unsigned int w0, c1, c2, i;

    *nombre = NULL;
    if (game->data && game->size >= sizeof(h))
        memcpy(h, game->data, sizeof(h));
    else if (game->path)
    {
        FILE *f = fopen(game->path, "rb");
        size_t n;
        if (!f)
            return 0;
        n = fread(h, 1, sizeof(h), f);
        fclose(f);
        if (n != sizeof(h))
            return 0;
    }
    else
        return 0;

    w0 = ((unsigned)h[0] << 24) | ((unsigned)h[1] << 16) | ((unsigned)h[2] << 8) | h[3];
    if (w0 == 0x37804012u)                 /* .v64: bytes de 16 bits cambiados */
    {
        for (i = 0; i < sizeof(h); i += 2)
        {
            unsigned char t = h[i]; h[i] = h[i + 1]; h[i + 1] = t;
        }
    }
    else if (w0 == 0x40123780u)            /* .n64: palabras de 32 bits al reves */
    {
        for (i = 0; i < sizeof(h); i += 4)
        {
            unsigned char t0 = h[i], t1 = h[i + 1];
            h[i] = h[i + 3]; h[i + 1] = h[i + 2]; h[i + 2] = t1; h[i + 3] = t0;
        }
    }
    else if (w0 != 0x80371240u)
        return 0;

    c1 = ((unsigned)h[0x10] << 24) | ((unsigned)h[0x11] << 16) | ((unsigned)h[0x12] << 8) | h[0x13];
    c2 = ((unsigned)h[0x14] << 24) | ((unsigned)h[0x15] << 16) | ((unsigned)h[0x16] << 8) | h[0x17];
    if ((c1 == 0x729B5E32u && c2 == 0xB728D980u) ||    /* Gauntlet Legends (U) */
        (c1 == 0xD543BCD6u && c2 == 0x2BA5E256u) ||    /* Gauntlet Legends (E) */
        (c1 == 0x70B0260Eu && c2 == 0x6716D04Cu))      /* Gauntlet Legends (J) */
    {
        *nombre = "Gauntlet Legends";
        return 1;
    }
    return 0;
}

bool retro_load_game(const struct retro_game_info *game)
{
    enum retro_pixel_format pf = RETRO_PIXEL_FORMAT_XRGB8888;
    const char *dir_guardado = NULL;
    int ret;

    /* ★ CON `need_fullpath = false` LA RUTA PUEDE SER NULL.  Salvia solo
     * rellena `path` si extrajo a disco; con un .zip cargado a memoria pasa
     * `NULL` (salvia.h:770-776).  Lo que tiene que haber es `data`. */
    if (!game || (!game->data && !game->path))
        return false;

    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pf))
    {
#if XBOX64_TRAZAS
        logf("[xbox64] el frontend no acepta XRGB8888");
#endif
        return false;
    }

    /* Los nombres de los botones para la pantalla de remapeo de Salvia.
     * No se comprueba el retorno a proposito: un frontend que no los
     * quiera sigue jugando igual, solo ensenya los nombres genericos. */
    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void *)s_desc);

    /* ★ LAS OPCIONES, ANTES DE CARGAR.  Las de glN64 solo tienen efecto
     * si estan puestas cuando corre `InitiateGFX`, y eso pasa DENTRO de
     * `xbox64_load_rom`, unas lineas mas abajo. */
    aplica_opciones();

    /* ★ [etapa 5] LA LISTA EN PARALELO SE DECIDE AQUI, ANTES DE LA ROM:
     * `gfx_info_init` (xbox/plataforma.c) pregunta `hilo_lista_activo()` y
     * el plugin se queda con esos punteros en `initiateGFX`.  Hace falta el
     * hilo del emulador: la lista solo puede ir aparte si alguien consume
     * las ordenes en `retro_run`. */
    {
        int lista = 0;
#if !XBOX64_GFX_NULO
        lista = (s_hilo_ok && s_lista_ok && xbox64_gfx_hilo_opcion() &&
                 s_opt_lista) ? 1 : 0;
        if (lista)
        {
            const char *nombre = NULL;
            if (rom_sin_lista_paralelo(game, &nombre))
            {
                lista = 0;
#if XBOX64_TRAZAS
                logf("[xbox64] por juego: lista en paralelo DESACTIVADA (%s: su lista salta a si misma)",
                     nombre);
#endif
            }
        }
#endif
#if XBOX64_TRAZAS
        if (lista)
            logf("[xbox64] lista en paralelo: pedida; creando/configurando su hilo (HW4)...");
#endif
        hilo_lista_configurar(lista);
#if XBOX64_TRAZAS
        logf("[xbox64] lista en paralelo: %s",
             hilo_lista_activo() ? "SI (HW4)"
                                 : (lista ? "NO (el hilo no se pudo crear)" : "no"));
#endif
    }

    /* ★ ANTES de cargar: fileBrowser construye las rutas de partida con
     * esta raiz, y loadEeprom y companyia corren DENTRO de xbox64_load_rom. */
    if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir_guardado) &&
        dir_guardado && dir_guardado[0])
        xbox64_fb_set_save_root(dir_guardado);

#if !XBOX64_GFX_NULO
    /* -- Video: adoptar el device y arrancar el shim ----------------------
     *
     * ★ TODO ESTO ANTES DE xbox64_load_rom, porque la carga llama a
     * `initiateGFX` y eso ya toca el backend.  El orden de dentro lo
     * explica xbox/gfx_host.cpp. */
    {
        void *dev = NULL;

        if (!environ_cb(RETRO_ENVIRONMENT_SALVIA_GET_D3D_DEVICE, &dev) || !dev)
        {
#if XBOX64_TRAZAS
            logf("[xbox64] el frontend no da device D3D9 (environment %d)",
                 RETRO_ENVIRONMENT_SALVIA_GET_D3D_DEVICE);
#endif
            return false;
        }

        /* ★ El divisor solo entra AQUI: render target, depth y textura de
         * entrega.  Ver XBOX64_DIV_RENDER arriba. */
        if (!xbox64_gfx_init(dev,
                             FB_ANCHO / XBOX64_DIV_RENDER,
                             FB_ALTO  / XBOX64_DIV_RENDER))
            return false;

#if XBOX64_TRAZAS
        logf("[xbox64] video listo: %dx%d  (div=%d, declarado %dx%d)",
             xenon64_video_width(), xenon64_video_height(),
             XBOX64_DIV_RENDER, FB_ANCHO, FB_ALTO);
#endif
    }
#endif

    /* ★ MEMORIA SI LA HAY, RUTA SI NO, Y SE DICE CUAL.
     *
     * El camino normal es el de memoria: `need_fullpath` esta a `false`.  El
     * de ruta se queda porque libretro permite que un frontend pase solo la
     * ruta aunque el core no la pida, y porque un fallo de carga tiene que
     * decir POR CUAL DE LOS DOS entro -- sin esta linea, "la ROM no carga"
     * no distingue "el bufer venia vacio" de "el fichero no se abre". */
    if (game->data && game->size)
    {
#if XBOX64_TRAZAS
        logf("[xbox64] cargando %u bytes desde memoria (%s)",
             (unsigned)game->size, game->path ? game->path : "sin ruta");
#endif
        ret = xbox64_load_rom_mem(game->data, (unsigned)game->size,
                                  game->path);
    }
    else
    {
#if XBOX64_TRAZAS
        logf("[xbox64] cargando por ruta %s", game->path);
#endif
        ret = xbox64_load_rom(game->path);
    }

    if (ret)
    {
#if XBOX64_TRAZAS
        logf("[xbox64] la carga de la ROM fallo: %d", ret);
#endif
        return false;
    }

#if !XBOX64_GFX_NULO
    /* -- Publicar la textura al frontend ----------------------------------
     * Va DESPUES de la carga: xenon64_video_init la creo, pero el frontend
     * la liga en su quad y a partir de aqui todo lo de aguas abajo
     * -aspecto, shaders, escalado entero, overlay del menu- funciona sin
     * enterarse de que la imagen no viene de un bufer de pixeles. */
    {
        struct salvia_d3d_texture t;
        t.texture = xenon64_video_texture();
        t.width   = (unsigned)xenon64_video_width();
        t.height  = (unsigned)xenon64_video_height();

        if (!t.texture ||
            !environ_cb(RETRO_ENVIRONMENT_SALVIA_SET_D3D_TEXTURE, &t))
        {
#if XBOX64_TRAZAS
            logf("[xbox64] el frontend no acepta la textura (environment %d)",
                 RETRO_ENVIRONMENT_SALVIA_SET_D3D_TEXTURE);
#endif
            xbox64_unload_rom();
            return false;
        }
#if XBOX64_TRAZAS
        logf("[xbox64] textura entregada: %ux%u", t.width, t.height);
#endif
    }
#endif

#if XBOX64_TRAZAS
    s_frames      = 0;
    s_frames_prev = 0;
    s_t_resumen   = 0;
#endif

    /* [etapa 4] El modo de ESTA partida.  El hilo no arranca aqui sino en
     * el primer `retro_run`, como el pthread de mupen (libretro.c:2053). */
    s_modo_hilo = 0;
#if !XBOX64_GFX_NULO
    if (s_hilo_ok && xbox64_gfx_hilo_opcion())
        s_modo_hilo = 1;
#endif
#if XBOX64_TRAZAS
    logf("[xbox64] renderizado con hilo: %s",
         s_modo_hilo ? "SI (emulacion en HW2, MAX_SWAP 2)"
                     : (s_hilo_ok ? "no (opcion apagada)" : "no (sin hilo)"));

    logf("[xbox64] ROM cargada.  pc=%08X  count_per_op=%u",
         (unsigned)r4300.pc, (unsigned)count_per_op);
#endif
    return true;
}

bool retro_load_game_special(unsigned type,
                             const struct retro_game_info *info,
                             size_t num)
{
    (void)type; (void)info; (void)num;
    return false;
}

void retro_unload_game(void)
{
#if XBOX64_TRAZAS
    logf("[xbox64] retro_unload_game  (frames=%u)", s_frames);
#endif

    /* [etapa 4] ★ LO PRIMERO: parar al emulador.  Todo lo de abajo -- la
     * ROM, el plugin, el shim -- es estado que el hilo esta usando.  Bombea
     * la cola mientras espera (el emulador puede estar esperando al
     * consumidor) y la deja vacia.  Es el STOP + bombeo + join de mupen
     * (libretro.c:2008-2041), sin el join: el hilo se queda ocioso. */
    hilo_emu_parar();
    s_modo_hilo = 0;
    /* [etapa 5] El emulador vacio el hilo de la lista al pararse.  A partir
     * de aqui el plugin se llama en linea (el cierre de la ROM). */
    hilo_lista_configurar(0);
    /* El mando no puede seguir vibrando fuera del juego. */
    rumble_parar_todo();
#if D3D_GRABADOR >= 3 && !XBOX64_GFX_NULO
    /* [etapa 5] Las texturas del almacen que nadie llego a pedir. */
    d3dq_almacen_vaciar();
#endif

#if D3D_GRABADOR && !XBOX64_GFX_NULO
    /* ★★ Y ANTES de soltar nada, desligar las texturas del device: la
     * descarga libera las del plugin, y el `SetTexture(i, NULL)` de
     * `gxd3d_shutdown` llegaba DESPUES.  Con el hilo, el bombeo de
     * `hilo_emu_parar` las acababa de volver a ligar (log 20: excepcion en
     * `D3DDevice_SetTexture(1, NULL)` al pasar de Rush a OoT).  Ver
     * `d3dq_desliga_texturas` en xenon/d3dq.cpp.  En los dos modos.
     * OJO: la llamada es FUNCIONAL; sin trazas se hace igual, sin el log. */
#if XBOX64_TRAZAS
    logf("[xbox64] descarga: desligadas %u texturas del core antes de soltarlas",
         d3dq_desliga_texturas());
#else
    d3dq_desliga_texturas();
#endif
#endif

    xbox64_unload_rom();

#if !XBOX64_GFX_NULO
    /* ★ RETIRAR LA TEXTURA ANTES DE LIBERARLA.  xbox64_gfx_shutdown ->
     * xenon64_video_shutdown la destruye; si el frontend sigue con ella
     * ligada en su quad, se quedaria muestreando memoria muerta en cuanto
     * vuelva al menu.  `texture = NULL` es como se retira. */
    if (environ_cb)
    {
        struct salvia_d3d_texture t;
        t.texture = NULL;
        t.width   = 0;
        t.height  = 0;
        environ_cb(RETRO_ENVIRONMENT_SALVIA_SET_D3D_TEXTURE, &t);
    }
    xbox64_gfx_shutdown();
#endif
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void retro_reset(void)
{
    /* Fase 5.  Un reset de verdad es descargar y volver a cargar; sin la
     * ruta guardada no se puede hacer aqui todavia, y mentir con un no-op
     * silencioso seria peor que decirlo. */
#if XBOX64_TRAZAS
    logf("[xbox64] retro_reset: todavia no implementado");
#endif
}

void retro_run(void)
{
#if XBOX64_TRAZAS   /* cronometros del reparto: solo los lee el resumen */
    unsigned long long tb_entrega0 = 0;
    /* Marcas intermedias del reparto de `entrega`.  A 0 = "este frame no paso
     * por ahi", y entonces no se reparte nada (ver el cierre, abajo). */
    unsigned long long tb_ent_audio = 0, tb_ent_fin = 0;
    /* ★★ EL RELOJ DE `retro_run` ENTERO.  Ver `s_tb_cab`. */
    const unsigned long long tb_run0 = __mftb();
#endif

    if (input_poll_cb)
        input_poll_cb();

    /* ★ LA FOTO DE LOS MANDOS, AQUI Y UNA SOLA VEZ.
     *
     * Tiene que ir DESPUES de `input_poll_cb()` -- antes leeria el estado
     * del frame anterior -- y ANTES de `xbox64_run_frame()`, porque
     * `getKeys` se llama desde dentro de la emulacion.  Entre los dos no
     * hay nada mas que pueda moverla. */
    xbox64_input_poll();

    /* ★★★ L3 + R3 ARMA EL VOLCADO DE TEXTURAS DEL FRAME SIGUIENTE.
     *
     * Portado de Xenon64 (360/Xdk/xenon64/libretro_core.cpp:673), donde ya
     * existia.  `g_texbmpArm` lo define SIEMPRE xenon/gx_d3d9.cpp, y ese
     * fichero lo consume en `gxd3d_frame_begin`: saca un BMP por cada
     * textura distinta convertida en ese frame, con su formato, tamanyo,
     * puntero de origen y si su combinador usa SHADE.
     *
     * Solo en el flanco: si no, mantener los dos sticks pulsados volcaria en
     * cada frame y llenaria el disco.
     *
     * ★ DEPENDE DEL FRONTEND: esto solo llega si Salvia mapea L3 y R3 a
     * RETRO_DEVICE_ID_JOYPAD_L3/R3 en el puerto 0.  Si no se dispara, es
     * que no llegan, y habria que mirar el mapeo del frontend antes que
     * esto. */
    {
        static int s_l3r3Ant = 0;
        int ahora = (xbox64_retro_input_state(0, RETRO_DEVICE_JOYPAD, 0,
                                              RETRO_DEVICE_ID_JOYPAD_L3) &&
                     xbox64_retro_input_state(0, RETRO_DEVICE_JOYPAD, 0,
                                              RETRO_DEVICE_ID_JOYPAD_R3)) ? 1 : 0;
        if (ahora && !s_l3r3Ant)
        {
            g_texbmpArm = 1;
#if XBOX64_TRAZAS
            logf("[TEXBMP] L3+R3: armado el volcado del frame siguiente");
#endif
        }
        s_l3r3Ant = ahora;
    }

    /* ★ LAS OPCIONES QUE EL USUARIO HAYA TOCADO EN EL MENU.
     *
     * Solo se releen cuando el frontend avisa: preguntar por las cinco en
     * cada frame son cinco llamadas de entorno 60 veces por segundo para
     * que casi siempre contesten lo mismo.
     *
     * ★ Y OJO CON LO QUE SE APLICA EN CALIENTE: `aplica_opciones` cambia
     * `dynacore`, el pak y el sonido, que valen entre frames.  Las dos de
     * glN64 se guardan pero NO tienen efecto hasta la siguiente carga,
     * porque `Config_LoadConfig` las copia desde `InitiateGFX`.  Eso es
     * una limitacion real y esta anotada en FASE4.txt, no un descuido. */
    /* [etapa 4] Con el emulador en su hilo, las opciones tocan estado suyo
     * (el dynarec, el pak, el audio): se le aparca en el fin de VI -- el
     * mismo punto donde hasta la etapa 3 volvia `retro_run` -- y se le suelta
     * despues.  Sin hilo, `pausar` y `reanudar` no hacen nada. */
    {
        bool cambio = false;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &cambio) && cambio)
        {
            if (s_modo_hilo) hilo_emu_pausar();
            aplica_opciones();
            if (s_modo_hilo) hilo_emu_reanudar();
        }
    }

    /* ★ LA FRECUENCIA DE AUDIO, SI EL INVITADO LA HA CAMBIADO.
     *
     * Va AQUI y no en `aiDacrateChanged` porque ese punto de entrada corre
     * DENTRO de la emulacion, desde el manejador de escritura del registro
     * del AI.  Llamar al frontend desde ahi es reentrar en el a mitad de
     * una instruccion del invitado; libretro pide que las llamadas de
     * entorno salgan de `retro_run`, `retro_load_game` o `retro_init`.
     * El plugin solo levanta una marca y se recoge aqui.
     *
     * Casi todos los juegos escriben el DACRATE una vez al arrancar, asi
     * que esto suele dispararse una sola vez por partida. */
    if (xbox64_audio_frecuencia_cambio())
    {
        struct retro_system_av_info av;
        retro_get_system_av_info(&av);
#if XBOX64_TRAZAS
        logf("[xbox64] audio: el juego pide %u Hz", xbox64_audio_frecuencia());
#endif
        environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
    }

#if !XBOX64_GFX_NULO
    /* El render target y el depth del core, en su sitio, antes de que el
     * invitado emita nada. */
    xenon64_video_frame_begin();

    /* ★★ Y LA FRONTERA DEL BACKEND, QUE ES OTRA COSA Y FALTABA.
     *
     * `xenon64_video_frame_begin` pone el render target; `gxd3d_frame_begin`
     * caduca lo que el SHIM cree que tiene ligado.  Entre dos `retro_run` el
     * frontend ha dibujado su quad y su menu CON EL MISMO DEVICE, o sea que
     * ha dejado puestos SU vertex shader, SU declaracion de vertices, SU
     * stream source, sus samplers y los seis render states que el shim
     * cachea.  Sin caducar, `s_bindValid` sigue a 1 desde el primer dibujo
     * de la sesion y el shim NUNCA vuelve a llamar a `SetVertexShader`.
     *
     * Eso no es un fallo cosmetico: el pixel shader SI es el nuestro (lo
     * pone `gxd3d_tev_apply` por su propia suciedad), asi que el draw sale
     * con el VS del frontend contra nuestro PS.  Sus salidas no casan
     * (TEXCOORD0/1/2 + COLOR0 + TEXCOORD3), Xenos intenta parchear el VS
     * para que case y revienta DENTRO del driver -- la pila exacta esta
     * escrita desde hace tiempo en gx_d3d9.cpp:476-486, que es donde se
     * pago este mismo cuelgue en el arbol de origen. */
#if D3D_GRABADOR
    /* ★★★ [etapa 3] EL LADO DEL CONSUMIDOR, ANTES DE GRABAR NADA.
     *
     * El render target del core ya esta ligado (`xenon64_video_frame_begin`
     * de arriba).  Ahora el vertex buffer de verdad -- que desde esta etapa
     * es del consumidor -- y la sombra de estado: `d3dq_bind()` reaplica todo
     * lo que el frontend haya pisado desde el `retro_run` anterior.  Es el
     * `glsm_ctl(GLSM_CTL_STATE_BIND)` de mupen (libretro/libretro.c:2062).
     *
     * Van ANTES de `gxd3d_frame_begin`: con el grabador a 2 hay vaciados a
     * mitad de frame (ordenes sincronas, anillo lleno), y esos dibujos ya
     * tienen que encontrar el buffer bloqueado y el estado repuesto.
     *
     * ★ `gxd3d_consumidor_empieza()` va FUERA del `#if` a proposito: el
     * vertex buffer es del consumidor tambien con el grabador a 0 (el shim
     * llama a `gxd3d_consumidor_dibuja` en el acto), y hay que bloquearlo y
     * soltarlo en cada pasada igual. */
#endif
    gxd3d_consumidor_empieza();
#if D3D_GRABADOR
#if D3DQ_BIND
    d3dq_bind();
#endif
#if XBOX64_TRAZAS
    s_q_ord0 = d3dq_dbg_ordenes;
    s_q_byt0 = d3dq_dbg_bytes;
#endif
#endif
    /* [etapa 4] Con el modo hilo, la frontera del PRODUCTOR la pone su hilo
     * (xbox/hilo_emu.cpp), alrededor de cada VI.  Aqui solo queda la del
     * consumidor, que es la de arriba. */
    if (!s_modo_hilo)
        gxd3d_frame_begin();
#endif

    /* ★★★ [etapa 4] EL TRAMO `emu` CAMBIA DE SIGNIFICADO CON EL MODO HILO.
     *
     * Sin hilo es lo de siempre: `xbox64_run_frame()`, la CPU emulada y el
     * recorrido de la lista.  Con hilo, la emulacion corre en HW2 y aqui
     * dentro esta el CONSUMIDOR: ejecutar las ordenes D3D del frame hasta su
     * SWAP, y esperarlo si el emulador va por detras.  `[HILO]` lo parte en
     * `cons` (ejecutar) y `esp` (esperar), y la emulacion se mide alli, en
     * `trab`, por VI.
     *
     * El hilo se arranca en el PRIMER `retro_run` de la partida, como el
     * pthread de mupen (libretro.c:2053-2060): la pasada del consumidor ya
     * esta preparada, y lo que la carga dejara en el anillo se ejecuta aqui
     * mismo, en orden, antes que lo del emulador. */
    {
#if XBOX64_TRAZAS
        unsigned long long t0 = __mftb();
        unsigned long long dt;
#endif
        if (s_modo_hilo)
        {
            if (!hilo_emu_corriendo())
                hilo_emu_arrancar();
            hilo_emu_consumir_frame();
        }
        else
            xbox64_run_frame();
#if XBOX64_TRAZAS
        dt = __mftb() - t0;
        s_tb_emu += dt;
        if (dt > s_tb_emu_max) s_tb_emu_max = dt;   /* ver la nota de arriba */
#endif
    }
#if XBOX64_TRAZAS
    s_frames++;

    /* Arranca el tramo de ENTREGA; se cierra al final de retro_run. */
    tb_entrega0 = __mftb();
#endif

    /* ★ EL AUDIO SE VACIA AQUI, con el frame ya emulado y ANTES de
     * entregar la imagen.
     *
     * El orden importa poco para el sonido y mucho para el ritmo: el
     * frontend usa la cola de audio para regular la velocidad, asi que
     * cuanto antes la vea, mejor reparte el trabajo del frame.  Se
     * entrega TODO lo que el invitado haya producido; dejar cola aqui es
     * latencia que ya no se recupera. */
    xbox64_audio_drenar();
    /* ★★ `entrega` ES UN RESIDUO: hay que partirlo o no dice nada.
     *
     * Mezcla tres cosas que se paran por motivos distintos -- el drenaje de
     * audio (por el que el frontend REGULA LA VELOCIDAD, ver el comentario de
     * arriba), el cierre de frame del shim (que hace el ultimo vaciado y el
     * Resolve, o sea D3D que puede esperar a la GPU) y la llamada al
     * frontend.  Un solo numero de 8.000 us no distingue "nos estan frenando
     * por el ritmo" de "la GPU va por detras", y son arreglos opuestos.
     *
     * Motivo: en Majora's Mask hay bajones sostenidos a 52-54 fps en los que
     * `emu` NO se mueve (8-10 ms) y todo el exceso esta aqui; arrancan detras
     * de un pico de carga y se deshacen solos en ~30 s. */
#if XBOX64_TRAZAS
    tb_ent_audio = __mftb();
#endif

    /* El PC ya no se muestrea aqui: desde este punto la interrupcion ya se
     * entrego y siempre vale 80000180.  Lo hace xbox64_frame_boundary. */

#if XBOX64_GFX_NULO
    if (s_fb)
    {
        pinta_framebuffer_vi();
        if (video_cb)
            video_cb(s_fb, FB_ANCHO, FB_ALTO, FB_ANCHO * 4);
    }
#else
    /* ★ ANTES de devolverle el device al frontend: `gxd3d_frame_end` dibuja
     * lo que quede en el lote pendiente y DESBLOQUEA el vertex buffer.
     * Dejarlo bloqueado mientras el frontend dibuja su quad es pedir un
     * conflicto, y el Lock de la pasada siguiente se comeria la espera. */
    /* [Xbox360] Aqui estaba la valla del hilo grafico antiguo: esperar a
     * que la lista en vuelo acabara antes de devolver el device.  Era el
     * techo del reparto -- una lista de 20-30 ms no cabe en 16,7 -- y se
     * quito con el hilo (2026-09-30).  Sin hilo, la lista ya termino dentro
     * de `xbox64_run_frame` y no hay nada que esperar. */
    /* [etapa 4] Con el modo hilo, esto y el vaciado de abajo NO van: el
     * `gxd3d_frame_end` lo llama el hilo del emulador al acabar cada VI, y
     * las ordenes del frame ya se ejecutaron en `hilo_emu_consumir_frame`.
     * Un `d3dq_drena` aqui seria un segundo consumidor corriendo por delante
     * del SWAP: se comeria los frames que el emulador lleva de adelanto. */
    if (!s_modo_hilo)
        gxd3d_frame_end();

#if D3D_GRABADOR >= 2
    /* ★★★ [etapa 3] EL VACIADO, AQUI Y SOLO AQUI.
     *
     * Todo lo que el shim grabo durante `xbox64_run_frame` se ejecuta ahora,
     * en orden, contra el device.  Es el `co_switch(game_thread)` de
     * `retro_run` en mupen: el consumidor corre dentro de `retro_run`, que es
     * el hilo dueno del device.  En la etapa 4 esto mismo seguira aqui; lo
     * unico que cambia es QUIEN graba y CUANDO.
     *
     * Tiene que ir ANTES de soltar el vertex buffer y de devolver el render
     * target: los dibujos pendientes leen de uno y pintan en el otro. */
    if (!s_modo_hilo)
        d3dq_drena();
#endif
    /* Fuera del `#if`, por lo mismo que `gxd3d_consumidor_empieza`. */
    gxd3d_consumidor_acaba();
#if D3D_GRABADOR && XBOX64_TRAZAS
    {
        /* Lo que se grabo en ESTE frame, para dimensionar el anillo de la
         * etapa 4, donde pueden acumularse hasta ~3 frames (MAX_SWAP). */
        const unsigned int o = d3dq_dbg_ordenes - s_q_ord0;
        const unsigned int b = d3dq_dbg_bytes   - s_q_byt0;
        if (o > s_q_ordmax) s_q_ordmax = o;
        if (b > s_q_bytmax) s_q_bytmax = b;
    }
#endif

    xenon64_video_frame_end();     /* devuelve el back buffer al frontend */

    /* Marca de fin del tramo de CIERRE DE FRAME: aqui dentro estan la valla
     * del hilo grafico, el ultimo vaciado del lote y el Resolve.  Si el
     * exceso sale en este tramo, es D3D esperando a la GPU. */
#if XBOX64_TRAZAS
    tb_ent_fin = __mftb();
#endif

    /* ★ EL CENTINELA, NO UN PUNTERO.  `RETRO_HW_FRAME_BUFFER_VALID` NO es
     * NULL: el frontend tiene que reconocerlo explicitamente o lo tratara
     * como puntero a pixeles.  Y cuando el core no ha resuelto imagen
     * nueva se manda NULL, que en libretro significa "repite el frame
     * anterior" -- que es lo correcto y no un fallo. */
    {
        int lista = xenon64_video_take_frame_ready();
#if XBOX64_TRAZAS
        if (lista) s_img++;
#endif
        if (video_cb)
            video_cb(lista ? RETRO_HW_FRAME_BUFFER_VALID : NULL,
                     (unsigned)xenon64_video_width(),
                     (unsigned)xenon64_video_height(), 0);
    }
#endif

    /* Cierra el tramo de entrega ANTES del resumen: el resumen imprime y
     * eso es coste de la sonda, no del core.
     *
     * ★ Y REPARTIDO EN TRES.  Las marcas intermedias valen 0 en las ramas que
     * no las ponen (GFX_NULO, o una salida temprana): en ese caso los tres
     * sumandos salen a cero y solo cuenta el total, que es lo que habia antes
     * -- nunca un numero inventado. */
#if XBOX64_TRAZAS
    {
        const unsigned long long t_ent = __mftb();
        s_tb_entrega += t_ent - tb_entrega0;
        if (tb_ent_audio && tb_ent_fin)
        {
            s_tb_ent_audio += tb_ent_audio - tb_entrega0;
            s_tb_ent_fin   += tb_ent_fin   - tb_ent_audio;
            s_tb_ent_video += t_ent        - tb_ent_fin;
        }
    }

    /* ★ Se cierra ANTES del resumen, igual que `entrega`: imprimir es coste
     * de la sonda, no del core. */
    {
        unsigned long long dtr = __mftb() - tb_run0;
        s_tb_run += dtr;
        if (dtr > s_tb_run_max) s_tb_run_max = dtr;

        /* ★★★ Y LA UTILIZACION DE **ESTE** FRAME, QUE ES LO QUE HAY QUE
         * MIRAR.  Ver la nota de `s_util_run`. */
        util_anota(dtr);
    }

    resumen_por_segundo();
#endif /* XBOX64_TRAZAS */

    if (xbox64_quiere_apagar() && environ_cb)
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

/* ---------------------------------------------------------------------- */
/* Fase 5: estados, memoria y trucos                                      */
/* ---------------------------------------------------------------------- */
/* Se dejan devolviendo "no disponible" en vez de fingir que funcionan:
 * savestates_gc.c existe y se compila, pero engancharlo pide decidir el
 * formato y la version, y eso es trabajo de su fase. */

/* ★ EL TAMANYO SE CACHEA: libretro permite llamar a esto cuando quiera,
 * pero el valor tiene que ser ESTABLE durante la sesion -- el frontend
 * reserva una vez.  `xbox64_state_tam()` ya es constante; cachearlo lo
 * deja explicito y ademas evita recorrer los `sizeof` en cada llamada. */
size_t retro_serialize_size(void)
{
    static size_t tam = 0;
    if (tam == 0)
    {
        tam = xbox64_state_tam();
#if XBOX64_TRAZAS
        logf("[xbox64] estado: %u bytes", (unsigned)tam);
#endif
    }
    return tam;
}

/* [etapa 4] ★ Con el modo hilo, el estado se lee y se escribe con el
 * emulador APARCADO en el fin de un VI, que es exactamente el punto donde
 * hasta la etapa 3 caia cualquier llamada entre dos `retro_run`.  Es el
 * `co_switch(game_thread)` de `retro_serialize` en mupen
 * (libretro.c:2141-2192).  Salvia llama a los dos desde su hilo principal
 * (`saveState`/`loadState`, src/io/statesram.h), que es el del consumidor:
 * por eso pueden bombear la cola sin ser un segundo consumidor.
 *
 * Los frames que el emulador llevaba de adelanto se DIBUJAN al aparcar
 * pero no se presentan: al cargar, la imagen salta directamente al estado
 * nuevo. */
bool retro_serialize(void *data, size_t size)
{
    bool ok;
    if (s_modo_hilo) hilo_emu_pausar();
    ok = xbox64_state_guardar(data, size) ? true : false;
    if (s_modo_hilo) hilo_emu_reanudar();
    return ok;
}

bool retro_unserialize(const void *data, size_t size)
{
    bool ok = false;

    if (s_modo_hilo) hilo_emu_pausar();
    if (xbox64_state_cargar(data, size))
    {
        /* ★ EL AUDIO PENDIENTE YA NO VALE: son muestras del momento ANTERIOR
         * a la carga.  Entregarlas seria un chasquido justo al cargar, que
         * ademas parece un fallo del audio y no lo es.  [etapa 4] Con el
         * emulador aparcado, que es quien escribe en el anillo de audio. */
        xbox64_audio_reset();
        /* [etapa 5] Los DP_INT que haya en la cola son los del estado
         * cargado; la deuda de listas de antes ya no les corresponde. */
        hilo_lista_olvidar_dp();
        ok = true;
    }
    if (s_modo_hilo) hilo_emu_reanudar();
    return ok;
}

/* ====================================================================== */
/* LA PARTIDA GUARDADA                                                    */
/* ====================================================================== */
/* ★ SE ENTREGA EL BLOQUE, NO UNA COPIA.  El frontend lee y escribe este
 * puntero cuando quiere -- periodicamente y al salir -- sin volver a
 * preguntar.  Por eso los cuatro guardados del N64 viven pegados en
 * `xbox64_mem_guardado` (xbox/xbox_mem.h) y no en cuatro arrays sueltos:
 * copiar a un bufer intermedio daria dos verdades, y la del frontend
 * seria siempre la vieja.
 *
 * ★ El orden dentro del bloque es el de mupen64plus-next (eeprom,
 * mempack, sram, flashram), asi que un `.srm` es intercambiable con ese
 * core.
 *
 * RETRO_MEMORY_SYSTEM_RAM es la RDRAM, y se entrega tambien: es lo que
 * usan los buscadores de trucos del frontend.  No se persiste -- eso lo
 * decide el frontend por el id -- asi que no hay riesgo de confundirla
 * con la partida. */
void *retro_get_memory_data(unsigned id)
{
    switch (id)
    {
        case RETRO_MEMORY_SAVE_RAM:   return &xbox64_mem_guardado;
        case RETRO_MEMORY_SYSTEM_RAM: return (void *)rdram;
        default:                      return NULL;
    }
}

size_t retro_get_memory_size(unsigned id)
{
    switch (id)
    {
        case RETRO_MEMORY_SAVE_RAM:   return sizeof(xbox64_mem_guardado);
        case RETRO_MEMORY_SYSTEM_RAM: return sizeof(rdram);
        default:                      return 0;
    }
}

/* Trucos GameShark de los .cht de libretro (Salvia los manda desde su menu de
 * trucos).  El motor es el de mupen64plus, en xbox/trucos_n64.c; se aplica en
 * cada VI desde new_vi (timers.c). */
void retro_cheat_reset(void)
{
    xbox64_trucos_borrar();
}
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    xbox64_trucos_poner(index, enabled ? 1 : 0, code);
}
