#ifndef XENON64_D3D9_GRABADOR_H
#define XENON64_D3D9_GRABADOR_H

/* ==========================================================================
 * FASE 2: interponerse entre el plugin y el device de D3D.
 *
 * QUE PROBLEMA RESUELVE.  Hoy el hilo grafico recibe la display list y la
 * recorre EL, asi que lee RDRAM del invitado en vivo: los comandos
 * (`RSP_Parser.cpp:1033`), los vertices (`RenderBase.cpp:1494`), las matrices,
 * las texturas y el modulo FBG.  Por eso hay que retenerle avisos al invitado
 * para que no recicle esos buferes mientras se leen.
 *
 * La referencia no hace eso, y sus TRES caminos multihilo comparten una regla:
 * el hilo trabajador JAMAS lee memoria viva del invitado.  GLideN64 difiere
 * solo llamadas a la API grafica con los payloads copiados a un pool anillo
 * (`opengl_Command.cpp:9`); paraLLEl-RDP copia el flujo de comandos dos veces
 * (`rdp.cpp:87` -> `command_ring.cpp:79`); angrylion es fork-join.
 *
 * POR QUE LA FRONTERA ES ESTA.  En Xbox 360 `D3DCREATE_MULTITHREADED` no
 * existe (`d3d9.h:281-287`).  Si hay traspaso EXPLICITO de propiedad
 * (`AcquireThreadOwnership` y companyia, `d3d9.h:1948-1951`), pero eso es un
 * testigo, no un candado compartido: sigue habiendo un solo hilo dueno a la
 * vez, y lo que queremos es que los dos avancen a la vez.  Asi que el hilo de
 * emulacion no puede llamar a D3D y el corte tiene que ser un flujo de
 * comandos.
 *
 * POR QUE UNA CLASE Y NO TOCAR LAS LLAMADAS.  Hay 293 referencias a
 * `g_pd3dDevice` en 5 ficheros.  En 360 los metodos del device NO son
 * virtuales (`d3d9.h:2175`), asi que no hay vtable que enganchar; pero todos
 * los sitios llaman con `->`, asi que basta cambiar de que tipo es el puntero.
 * 293 ediciones se convierten en 4.
 *
 * VALORES DE LA PALANCA:
 *   0 = sin interponer.  El arbol de antes, exacto.  Es el control.
 *   1 = pasarela: se interpone y reenvia en el acto.  VALIDADO en hardware
 *       (log 6: sin [OPST], 333/348 ventanas a 60-61 VI/s, rech=0,
 *       mi0=miCap0=dispar=0 en 352, cero TEXFAIL).
 *   2 = GRABA en el anillo y lo DRENA EL MISMO HILO.  Sin concurrencia
 *       ninguna: lo unico que pone a prueba es si los 31 pares de
 *       codificar/ejecutar son correctos.  Si la imagen es identica a la del
 *       valor 1, la codificacion esta bien.
 *   3 = (siguiente paso, no escrito) el hilo de emulacion recorre la display
 *       list y el trabajador drena el anillo.
 *
 * El 2 existe porque en el 3 se juntan DOS modos de fallo independientes
 * -"codifique mal un comando" y "me equivoque con el hilo"- y separarlos es
 * la diferencia entre un fallo legible y una pantalla negra sin pistas.
 * ========================================================================== */

#include <d3d9.h>

/* >>> ★★ MEDIDO: EL VALOR 3 NO PAGA, Y LA PREMISA DE LA FASE ERA FALSA.
 *
 * Zelda MM, misma escena cargada:
 *
 *                              VI/s     frame      [PERF] gfx en el hilo emu
 *   asincrono (valor 2)        60-61    16,6 ms    ~0  (la DL va al hilo)
 *   VALOR 3                    51-52    19,2-19,6  268-280 ms/s
 *   sincrono puro (sin anillo) 51-53    19,2-19,8  272 ms/s
 *
 * El valor 3 aterriza EXACTAMENTE sobre el sincrono puro.  Por que:
 *
 * 1. Lo unico que se difiere de verdad es `DrawPrimitive`, y en condiciones
 *    de tipo sincrono cuesta 76 ms/s (log sincrono: `[GFX] draw=76.15`).  Los
 *    151 ms/s que yo cite del log ASINCRONO para justificar la fase eran
 *    CONTRAPRESION DE LA GPU absorbida por el hilo, no trabajo de CPU.  Es la
 *    trampa del "primer D3D que bloquea se come la espera", que este arbol
 *    tiene documentada siete veces y en la que volvi a caer.
 * 2. Grabar cuesta: el `flush` del hilo de emulacion pasa de 9 ms/s de CPU en
 *    sincrono (85,22 - 76,15) a 39-63 ms/s en el valor 3 (`flush=39,88-62,83`
 *    con `draw=1,43`).  La codificacion se come casi todo lo que el diferido
 *    ahorra.
 * 3. Y a cambio vuelven al hilo de emulacion los 186 ms/s atados a RDRAM
 *    (recorrido de la DL + `vtx` + `tex`).
 *
 * Neto: el valor 3 deja al hilo de emulacion ~110 ms/s PEOR que el asincrono.
 * Y no es un problema de sincronizacion mal puesta: `[D3DQ] espera=32-51
 * ms/s`, o sea que el productor apenas espera; simplemente hace mas trabajo.
 *
 * NO REHACER ESTE CAMINO sin una medida nueva que desmienta lo de arriba.  La
 * ganancia real de la sesion fue la FASE 1 (GFXTH_SP_PRONTO), que es
 * independiente de todo esto.
 *
 * Los valores 2 y 3 se quedan porque funcionan y estan validados en los tres
 * juegos; lo que no hacen es ir mas rapido. */
/* ★★★ [xbox64] A 1: LA PASARELA.  ETAPA 2 DEL RENDERIZADO CON HILO.
 *
 * Hasta el 2026-09-30 estaba a 0, con la nota de que traer la
 * implementacion "seria portar media infraestructura para no ganar nada"
 * porque no habia hilo grafico que la necesitase.  Eso cambio: se esta
 * construyendo el modelo de mupen64plus-libretro-nx
 * (`EnableThreadedRenderer`), en el que el EMULADOR va en su propio hilo y
 * `retro_run` -- el hilo dueno del device -- ejecuta las ordenes D3D que el
 * emulador graba.  Esa cola es exactamente este grabador.
 *
 * La implementacion esta portada entera, con los tres valores, en
 * xenon/d3dq.cpp (de Xenon64/xenon/d3d9_backend.cpp, shim de DirectX).
 *
 * Las etapas, un build cada una:
 *   0  el control: sin interponer, cada llamada directa al device.  Es la
 *      etapa 1, y volver a ella es cambiar esta linea.
 *   1  etapa 2 (VALIDADA 2026-09-30, log 18: `descon=0` en 466 lineas y
 *      Rush con Rice igual que el valor 0).  Cada orden se codifica y se
 *      ejecuta en el acto.
 *   2  etapa 3 (VALIDADA 2026-09-30, log 19: `binds` = `frames`,
 *      `descon=gordas=vbdrop=0` e imagen correcta con los dos plugins).  Se
 *      graba durante `xbox64_run_frame` y `retro_run` lo vacia al final.
 *   3  ETAPA 4, HOY: los hilos de mupen.  El EMULADOR graba en su propio
 *      hilo (xbox/hilo_emu.cpp) y `retro_run` -- el hilo dueno del device --
 *      ejecuta las ordenes hasta cada SWAP.  OJO: en el arbol de origen el 3
 *      era "un hilo grafico propio drena", que es OTRA cosa.
 *
 *      ★ El 3 incluye el 2 EN TIEMPO DE EJECUCION: con `g_d3dqVivo` a 0 (la
 *      core option `xbox64_gfx_thread` apagada, o fuera de partida) el
 *      productor vacia el mismo, exactamente como el valor 2.  Asi el A/B
 *      contra la etapa 3 es cambiar la opcion, sin recompilar.
 *
 * ★ La nota "VALOR 3 NO PAGA" de arriba sigue en pie como riesgo medido: se
 * contrasta con la opcion apagada, mismo juego y plugin. */
#define D3D_GRABADOR 3

/* ★★★ LA SOMBRA DE ESTADO DEL CONSUMIDOR: el `glsm BIND` de mupen.
 *
 * El device es del FRONTEND: entre dos `retro_run` Salvia dibuja su quad y
 * su menu y lo deja con SU estado.  Hasta ahora eso lo arreglaba el
 * PRODUCTOR: `gxd3d_frame_begin` reseteaba en cada frame el estado del shim
 * a neutro y lo volvia a emitir.  En la etapa 4 el productor ya no va
 * sincronizado con `retro_run`, asi que eso deja de valer -- y la cesion del
 * marco demostro lo que pasa si se intenta: resetear a neutro por debajo de
 * una lista a medias le quita el estado que ya habia puesto.
 *
 * mupen lo resuelve en el CONSUMIDOR: `glsm_ctl(GLSM_CTL_STATE_BIND)` al
 * entrar en `retro_run` reaplica la sombra del estado del core
 * (libretro/libretro.c:2062).  Aqui igual: `d3dq_ejecuta` apunta cada
 * estado que ejecuta y `d3dq_bind()` los reaplica todos al empezar la
 * pasada.  El espejo del productor se declara valido UNA vez y no vuelve a
 * invalidarse, porque quien garantiza que el device coincide con el es el
 * consumidor.
 *
 *   1  sombra del consumidor.  Es el modelo de la etapa 4.
 *   0  el reset por frame del productor, como hasta la etapa 2.  Es el
 *      aislador: si con 1 la imagen sale mal y con 0 bien, el fallo esta en
 *      la sombra y no en el anillo.
 *
 * Solo tiene sentido con el grabador puesto: la sombra vive en el
 * ejecutor. */
#if D3D_GRABADOR
#define D3DQ_BIND 1
#else
#define D3DQ_BIND 0
#endif

#if D3D_GRABADOR

/* -- El anillo -------------------------------------------------------------
 *
 * Ranura de tamano FIJO, que es lo que hace que codificar no pueda
 * descuadrarse: cinco punteros y seis palabras cubren el comando mas gordo
 * (`Resolve`, 5 punteros y 5 escalares) y sobra sitio.  Un formato de tamano
 * variable ahorraria memoria y a cambio meteria aritmetica de offsets en el
 * camino caliente, que es justo donde no quiero un fallo silencioso.
 *
 * Los punteros a DATOS (rects, arrays de constantes, vertices de
 * DrawPrimitiveUP) NO se guardan tal cual: el llamante puede reutilizar su
 * buffer en cuanto vuelve.  Se copian al pool.  Los punteros a OBJETOS de D3D
 * (texturas, shaders, vertex buffers) si se guardan tal cual: son handles con
 * vida propia. */
/* ★ [xbox64] MAS GRANDES QUE EN EL ORIGEN, Y POR UNA RAZON CONCRETA.
 *
 * Alli el pool solo llevaba rects, constantes y los vertices de las fuentes.
 * Aqui lleva ademas TODOS los vertices del shim (`DibujaVerts`) y los texels
 * de cada subida (`SubeTextura`).  El lote mas grande que el shim puede
 * vaciar de golpe es el bufer de preparacion entero: GXD3D_MAX_VERTS (49.152)
 * vertices de 36 bytes, 1,7 MB -- mas que el pool entero de 1 MB del origen.
 * Con eso saldria por `gordas`, que pasa el puntero original, y el bufer de
 * preparacion se reutiliza antes de ejecutarse: vertices corruptos.
 *
 * ★★ [etapa 4] EL TAMANO SALE DE LO MEDIDO EN LA ETAPA 3 (log 19), NO SE
 * ADIVINA.  El peor frame grabo 9.059 ordenes (Rush) y 2,7 MB de datos
 * (glN64).  Con `MAX_SWAP 2` pueden acumularse ~3 frames: ~27.000 ordenes y
 * ~8,3 MB.  Se dimensiona con margen: 65.536 ranuras (~2,4x) y un pool de
 * 32 MB (~4x).  mupen deja crecer el suyo de 10 a 200 MB (RingBufferPool.cpp).
 *
 * ★ Y SE RESERVAN AL CARGAR, NO EN ESTATICO.  Salvia enlaza TODOS sus cores
 * en el mismo ejecutable: 35 MB de BSS aqui los pagaria cualquier otro
 * sistema aunque no se juegue a N64.  `d3dq_init()` los pide con escalera
 * 32 -> 16 -> 8 -> 4 MB, como el monton de texturas de Rice
 * (`xbox64_rice_tex_heap_setup`). */
#define D3DQ_RANURAS   65536u             /* potencia de 2 */
#define D3DQ_MASC      (D3DQ_RANURAS - 1u)
#define D3DQ_POOL_MAX  (32u << 20)        /* el pool que se intenta primero */

enum D3DQOp
{
    QOP_NADA = 0,
    /* `Release` no esta aqui a proposito: no se encola.  Drena lo pendiente y
     * llama al device directamente, porque es una orden del ciclo de vida que
     * siempre ocurre en el hilo de emulacion y con el anillo ya vacio. */
    QOP_Present,
    QOP_CreateTexture,                /* sincrono: devuelve puntero */
    QOP_CreateVertexBuffer,           /* sincrono */
    QOP_CreateDepthStencilSurface,    /* sincrono */
    QOP_CreateVertexShader,           /* sincrono */
    QOP_CreatePixelShader,            /* sincrono */
    QOP_CreateVertexDeclaration,      /* sincrono */
    QOP_GetRenderTarget,              /* sincrono: AddRef, no se puede espejar */
    QOP_GetDepthStencilSurface,       /* sincrono: idem */
    QOP_GetPixelShader,               /* sincrono: idem */
    QOP_SetRenderTarget,
    QOP_SetDepthStencilSurface,
    QOP_Resolve,
    QOP_BeginScene,
    QOP_EndScene,
    QOP_Clear,
    QOP_SetRenderState,
    QOP_SetViewport,
    QOP_SetScissorRect,
    QOP_SetTexture,
    QOP_SetSamplerState,
    QOP_SetVertexShader,
    QOP_SetVertexShaderConstantF,
    QOP_SetPixelShader,
    QOP_SetPixelShaderConstantB,
    QOP_SetPixelShaderConstantF,
    QOP_SetVertexDeclaration,
    QOP_SetStreamSource,
    QOP_DrawPrimitive,
    QOP_DrawPrimitiveUP,
    QOP_GetRenderState,               /* sincrono: solo la PRIMERA lectura */
    QOP_GetScissorRect,               /* sincrono: solo la PRIMERA lectura */
    QOP_SoltarRecurso,                /* Release() EN ORDEN con los dibujos */
    /* [xbox64] Las tres que el productor necesita para no tocar NINGUN
     * recurso de D3D por su cuenta.  Ver `gxd3d_consumidor_*` mas abajo. */
    QOP_DibujaVerts,                  /* vertices en el pool -> VB del consumidor */
    QOP_SubeTextura,                  /* texels en el pool -> LockRect del consumidor */
    QOP_Entrega,                      /* xenon64_video_resolve() en el consumidor */
    QOP_MarcaProfundidad,             /* [DepthCopy] fence tras el resolve de Z */
    /* [xbox64, etapa 4] Fin del frame: el `CoreVideo_GL_SwapBuffers` de mupen
     * (ThreadedOpenGl/opengl_WrappedFunctions.h:5264).  El consumidor de
     * `retro_run` ejecuta hasta aqui y vuelve.  Ejecutado fuera de ese bucle
     * (`d3dq_drena`) no hace nada. */
    QOP_Swap,
    QOP_FIN
};

struct D3DQCmd
{
    unsigned int op;
    unsigned int poolFin;   /* posicion del pool DESPUES de esta orden: es lo que
                             * permite al productor saber que se ha liberado.
                             * [xbox64, etapa 4] Por fin se usa: el consumidor la
                             * publica al ejecutar (`g_d3dqPoolLibre`), y el pool
                             * es un anillo de verdad en vez de "vaciar entero y
                             * volver a 0". */
    void        *p[5];
    unsigned int w[6];
};

/* Estado del anillo.  [xbox64] Definido en xenon/d3dq.cpp (en el arbol de
 * origen vivia en d3d9_backend.cpp) y RESERVADO por `d3dq_init()`. */
extern D3DQCmd              *g_d3dq;
extern unsigned char        *g_d3dqPool;
extern unsigned int          g_d3dqPoolTam;   /* potencia de 2 */
extern volatile unsigned int g_d3dqCabeza;    /* solo lo toca el PRODUCTOR */
extern volatile unsigned int g_d3dqCola;      /* solo lo toca el CONSUMIDOR */
extern unsigned int          g_d3dqPoolCab;   /* solo el productor, monotono */
extern volatile unsigned int g_d3dqPoolLibre; /* solo el consumidor, monotono */

/* [xbox64] Reserva el anillo y el pool (escalera de tamanos) y crea los
 * eventos.  Devuelve 0 si no hay memoria ni para el tamano minimo.  La llama
 * xbox/gfx_host.cpp antes de adoptar el device; `d3dq_fin` los suelta. */
int   d3dq_init(void);
void  d3dq_fin(void);

/* Reserva `n` bytes del pool y copia `src`.  Devuelve el puntero dentro del
 * pool, o el original si `src` es NULL (para que los argumentos opcionales
 * sigan siendo NULL al otro lado). */
void *d3dq_payload(const void *src, unsigned int n);

/* Encola una orden ya rellena.  Bloquea si el anillo esta lleno. */
void  d3dq_mete(const D3DQCmd *c);

/* [xbox64] El device de verdad contra el que se ejecutan las ordenes.  Lo
 * fija xbox/gfx_host.cpp al adoptar el device del frontend y lo pone a NULL
 * al soltarlo.  En el arbol de origen no hacia falta: la cola vivia en el
 * mismo fichero que `s_grabador` y leia `s_grabador.real` directamente. */
void  d3dq_set_real(IDirect3DDevice9 *dev);

/* Ejecuta TODO lo que haya pendiente contra el device de verdad, SWAPs
 * incluidos (que ahi no hacen nada).
 *
 * ★ Solo es valida cuando productor y consumidor son el MISMO hilo: con
 * `g_d3dqVivo` a 0 (modo sin hilo, carga, cierre) o con el hilo del
 * emulador ya parado.  Con el emulador grabando en su hilo, el unico que
 * ejecuta es `retro_run` (`d3dq_consume_una`). */
void  d3dq_drena(void);

/* -- [xbox64] EL MODO 3: EL MODELO DE mupen64plus-libretro-nx ---------------
 *
 * El PRODUCTOR es el hilo del emulador (xbox/hilo_emu.cpp) y el CONSUMIDOR
 * es `retro_run`, en el hilo del frontend, que es el dueno del device.  En
 * el arbol de origen el valor 3 era otra cosa -- un hilo grafico propio que
 * drenaba -- y su API (`d3dq_hilo_init/fin/drena`, `d3dq_espera_vacio`) se
 * ha quitado: esperar a que la cola se vacie ENTERA no vale cuando el
 * consumidor para en cada SWAP, porque el productor esperaria frames.
 *
 * `g_d3dqVivo`: 1 mientras el emulador graba en su hilo.  Con 0 todo se
 * comporta como el valor 2 (el productor vacia el mismo). */
extern volatile unsigned int g_d3dqVivo;
void  d3dq_modo_hilo(int si);

/* Consumidor: ejecuta la SIGUIENTE orden del anillo.
 *   0  no habia ninguna
 *   1  una orden normal
 *   2  un SWAP: fin de frame, `retro_run` tiene que volver */
int   d3dq_consume_una(void);

/* ★ LAS ORDENES SINCRONAS (creacion de texturas y shaders).  Es la cola de
 * PRIORIDAD de mupen (`executePriorityCommand`, opengl_Wrapper.cpp:53-58):
 * no van al anillo -- tendrian que esperar a todo lo que hay delante, frames
 * enteros --, sino a un buzon que el consumidor atiende entre orden y orden.
 * Crear un objeto no depende del orden respecto a los dibujos, asi que
 * adelantarse es correcto.
 *
 * `d3dq_sincrona` la llama el productor y espera; `d3dq_atiende_buzon` el
 * consumidor.  Con `g_d3dqVivo` a 0 la orden va al anillo y se vacia en el
 * acto, como el valor 2. */
void  d3dq_sincrona(const D3DQCmd *c);
int   d3dq_atiende_buzon(void);
/* 1 = hay un productor esperando en el buzon.  Solo para el vigia de
 * esperas largas (xbox/hilo_lista.cpp): no sincroniza nada. */
int   d3dq_buzon_ocupado(void);

/* El productor avisa al consumidor cuando hay trabajo (anillo que deja de
 * estar vacio, buzon lleno); el consumidor duerme aqui como mucho `ms`. */
void  d3dq_espera_trabajo(unsigned int ms);

/* El productor cierra el frame.  Ver QOP_Swap. */
void  d3dq_mete_swap(void);

/* ★★ [etapa 5] EL ALMACEN DE TEXTURAS.  Medido en el log 27 (GoldenEye):
 * `sinc=139 (295248 us)` -- el hilo de la lista pierde ~0,3 s de cada
 * segundo esperando en el buzon a que `retro_run` le cree una textura, porque
 * `retro_run` solo lo atiende mientras corre.  Cada lista pasaba de ~13 a
 * ~25 ms, y eso arrastraba la valla del DP del emulador.
 *
 * El consumidor (el dueno del device) crea POR ADELANTADO unas pocas
 * texturas de cada tamanyo que el juego ya ha pedido, y el productor las toma
 * al instante.  Si no hay del tamanyo pedido, buzon de siempre, y el tamanyo
 * queda apuntado para la proxima reposicion.  Todas las llamadas a D3D siguen
 * en el hilo del device.
 *
 *   toma     productor: 1 si la sirvio el almacen (sin esperar)
 *   repone   consumidor, al empezar cada pasada de `retro_run`
 *   vaciar   al descargar, con el emulador parado: suelta lo no usado */
int   d3dq_almacen_toma(UINT w, UINT h, UINT levels, DWORD usage,
                        D3DFORMAT fmt, D3DPOOL pool, D3DTexture **pp);
void  d3dq_almacen_repone(unsigned int max);   /* crea como mucho `max` */
void  d3dq_almacen_vaciar(void);

/* Contadores de diagnostico: salen en la linea [D3DQ].
 *
 * >>> EL `extern "C"` VA AQUI, EN LA DECLARACION, Y ESO ES EL ARREGLO.
 *
 * Los lee `profile.c`, que se compila como C (el vcxproj pone
 * `CompileAs=Default`, xenon64.vcxproj:129), asi que el simbolo tiene que ser
 * sin decorar.  Ponerlo solo en la DEFINICION de d3d9_backend.cpp no basta:
 * ese fichero incluye esta cabecera antes, la declaracion de aqui fija enlace
 * C++ y la definicion posterior ya no lo puede cambiar -- el simbolo sale
 * decorado igual y el LNK2001 se repite sin que nada en la definicion parezca
 * mal.  La linkage la manda la PRIMERA declaracion que ve el compilador. */
#ifdef __cplusplus
extern "C" {
#endif
extern volatile unsigned int d3dq_dbg_ordenes;   /* ordenes encoladas/s      */
extern volatile unsigned int d3dq_dbg_drenajes;  /* vaciados del anillo/s    */
extern volatile unsigned int d3dq_dbg_llenos;    /* esperas por anillo lleno */
extern volatile unsigned int d3dq_dbg_poolmax;   /* pico de uso del pool     */
extern volatile unsigned int d3dq_dbg_descon;    /* ordenes DESCONOCIDAS     */
extern volatile unsigned int d3dq_dbg_gordas;    /* payload que no cabe      */

/* Censo por orden.  Sirve para la pregunta "que usa GoldenEye que Zelda no":
 * si el fallo resulta ser de CODIFICACION, la sospechosa esta entre las que
 * salgan a cero en Zelda y distintas de cero en GoldenEye.  Indexado por
 * D3DQOp; se publica solo el punado de ordenes raras, que son las que nadie
 * mira y por tanto donde un fallo sobrevive. */
extern volatile unsigned int d3dq_dbg_op[40];
/* Espera del productor al consumidor, en ticks del Time Base.  Ver
 * d3dq_espera_vacio: es el numero que dice si el paso 3 paga. */
extern volatile unsigned int d3dq_dbg_espera_tb;
extern volatile unsigned int d3dq_dbg_espera_n;

/* Formatea en `buf` la linea [D3DQOP] con el censo de ordenes raras y pone
 * a cero ese censo.  Vive en xenon/d3dq.cpp porque los indices salen del
 * enum `D3DQOp`. */
void d3dq_censo(char *buf, int n);

/* [xbox64] Etapa 3.
 *   binds     -- `d3dq_bind()` ejecutados.  Tiene que salir 1 por frame con
 *                D3DQ_BIND a 1: es el contador de paso de la sombra.
 *   rebinds   -- estados reaplicados por esos binds (suma).
 *   bytes     -- bytes copiados al pool.  Con `ordenes`, es la medida que
 *                dimensiona el anillo de la etapa 4.
 *   maxcola   -- pico de ordenes pendientes al empezar un vaciado. */
extern volatile unsigned int d3dq_dbg_binds;
extern volatile unsigned int d3dq_dbg_rebinds;
extern volatile unsigned int d3dq_dbg_bytes;
extern volatile unsigned int d3dq_dbg_maxcola;
/* [xbox64] Etapa 4.
 *   sinc_n / sinc_tb  -- ordenes sincronas (creaciones) y lo que el
 *                        emulador espero por ellas.  Es el punto debil
 *                        conocido de mupen: con `retro_run` fuera del bucle
 *                        de consumo, una creacion espera hasta el frame
 *                        siguiente.
 *   poolllenos        -- veces que el productor espero por pool lleno.
 *   perdidos          -- siestas del consumidor que acabaron por plazo con
 *                        trabajo esperando: avisos perdidos.  Ver
 *                        `d3dq_espera_trabajo`.  Tiene que salir ~0.
 *   siestas           -- veces que el consumidor llego a dormir tras girar.
 *                        Miles por segundo = el giro se queda corto.
 *   ligsuelta         -- texturas soltadas estando todavia ligadas en un
 *                        sampler (segun la sombra).  Solo cuenta.  Ver
 *                        `sombra_olvida` en xenon/d3dq.cpp. */
extern volatile unsigned int d3dq_dbg_sinc_n;
extern volatile unsigned int d3dq_dbg_sinc_tb;
extern volatile unsigned int d3dq_dbg_poolllenos;
extern volatile unsigned int d3dq_dbg_perdidos;
extern volatile unsigned int d3dq_dbg_siestas;
extern volatile unsigned int d3dq_dbg_ligsuelta;
/* [etapa 5] El almacen de texturas: servidas sin esperar, pedidas que no
 * habia (van al buzon), y creadas por adelantado por el consumidor. */
extern volatile unsigned int d3dq_dbg_alm_toma;
extern volatile unsigned int d3dq_dbg_alm_falla;
extern volatile unsigned int d3dq_dbg_alm_crea;
#ifdef __cplusplus
}
#endif

/* ★★ [xbox64] LA SOMBRA DE ESTADO DEL CONSUMIDOR.  Ver D3DQ_BIND arriba.
 *
 * `d3dq_bind()` reaplica al device TODO lo que el consumidor ha ejecutado:
 * render states, samplers, texturas, shaders, declaracion, constantes,
 * viewport y recorte.  Se llama al empezar cada pasada, con el render
 * target del core ya ligado.
 *
 * `d3dq_sombra_reset()` la vacia.  Hay que llamarla siempre que se suelten
 * objetos de D3D SIN pasar por `d3dq_soltar` (la cache de pixel shaders, el
 * cierre del shim): si no, el siguiente bind ligaria un objeto muerto. */
void d3dq_bind(void);
void d3dq_sombra_reset(void);

/* Desliga del device de verdad las texturas de los 4 samplers y las quita de
 * la sombra.  La llama `retro_unload_game` con el emulador parado y ANTES de
 * que la descarga suelte las texturas del plugin: soltar una textura ligada y
 * desligarla despues escribe en memoria liberada (log 20).  Devuelve cuantas
 * habia ligadas.  Ver la nota en xenon/d3dq.cpp. */
unsigned int d3dq_desliga_texturas(void);

class D3DGrabador
{
public:
    /* El device de verdad.  Publico a proposito: el arranque lo asigna y el
     * apagado lo consulta, y esconderlo detras de accesores solo anadiria
     * ruido a un adaptador. */
    IDirect3DDevice9 *real;

    /* Espejo de los estados fijos.  `GetRenderState` son 59 sitios de llamada
     * que devuelven un valor, asi que no se pueden diferir; y solo se leen
     * ONCE estados distintos, todos de mezcla/alfa/z/cull, que nuestro propio
     * codigo escribe.
     *
     * SIEMBRA PEREZOSA, y no un barrido de los 404 al arrancar: en 360 la
     * tabla interna del device esta indexada por `State/4` (`d3d9.h:2175`),
     * o sea que NO todos los indices de 0 a D3DRS_MAX son estados validos, y
     * preguntarlos todos seria pedirle al device cosas que no existen.  Aqui
     * un estado se conoce en cuanto se escribe; el que se lea antes de
     * escribirse se pregunta UNA vez con una orden sincrona y se cachea. */
    DWORD rsEspejo[404];      /* D3DRS_MAX = 404 (d3d9types.h:632) */
    char  rsConocido[404];
    RECT  scissorEspejo;
    char  scissorConocido;

    D3DGrabador() : real(0)
    {
        unsigned int i;
        for (i = 0; i < 404; ++i) { rsEspejo[i] = 0; rsConocido[i] = 0; }
        scissorEspejo.left = scissorEspejo.top = 0;
        scissorEspejo.right = scissorEspejo.bottom = 0;
        scissorConocido = 0;
    }

    /* -- >>> EL ESPEJO CADUCA: EL DEVICE NO ES SOLO NUESTRO ---------------
     *
     * Aqui esta la asimetria con la referencia, y es de fondo: GLideN64 es
     * dueno EXCLUSIVO de su contexto de OpenGL, asi que su cache de estado
     * no puede quedarse rancia.  Nuestro device es del FRONTEND y lo
     * comparte: Salvia dibuja su menu y su overlay con el mismo device y
     * pone sus propios estados, y de eso el espejo no se entera.
     *
     * Con `getRS=0/1` por segundo en el log, la consecuencia es que despues
     * del primer frame TODO se contesta del espejo, para siempre.  Si el
     * frontend cambio `ALPHABLENDENABLE` o `CULLMODE` por su cuenta, el
     * plugin lee lo que el creia y decide no ponerlo, porque cree que ya
     * esta.  Eso sale como mezcla y texturas mal, y explica que el modo 1
     * -- donde `GetRenderState` iba al device de verdad -- fuese bien.
     *
     * Se invalida una vez por display list: barato (los estados que se leen
     * de verdad son ONCE, asi que son ~11 ordenes sincronas por lista) y
     * acota la mentira a cero.  No vale con invalidarlo al arrancar. */
    void CaducarEspejo()
    {
        unsigned int i;
        for (i = 0; i < 404; ++i) rsConocido[i] = 0;
        scissorConocido = 0;
    }

private:
    /* Rellena y encola.  Inline y sin memset del hueco entero: solo se leen
     * los campos que el ejecutor de cada orden mira. */
    static void emite(unsigned int op,
                      void *p0, void *p1, void *p2, void *p3, void *p4,
                      unsigned int w0, unsigned int w1, unsigned int w2,
                      unsigned int w3, unsigned int w4, unsigned int w5)
    {
        D3DQCmd c;
        c.op = op;
        c.p[0] = p0; c.p[1] = p1; c.p[2] = p2; c.p[3] = p3; c.p[4] = p4;
        c.w[0] = w0; c.w[1] = w1; c.w[2] = w2;
        c.w[3] = w3; c.w[4] = w4; c.w[5] = w5;
        d3dq_mete(&c);
    }
    static void emite0(unsigned int op)
    { emite(op, 0,0,0,0,0, 0,0,0,0,0,0); }

    /* Las ordenes que devuelven algo: se encolan y se drena en el acto, que
     * es lo que GLideN64 llama `_synced` (`opengl_Command.cpp:51`).  Al
     * drenar aqui, los punteros a datos del llamante siguen vivos, asi que
     * estas NO necesitan copia al pool. */
    static void emiteSinc(unsigned int op,
                          void *p0, void *p1, void *p2, void *p3, void *p4,
                          unsigned int w0, unsigned int w1, unsigned int w2,
                          unsigned int w3, unsigned int w4, unsigned int w5)
    {
#if D3D_GRABADOR >= 3
        /* [xbox64, etapa 4] Por el BUZON de prioridad, no por el anillo: ver
         * `d3dq_sincrona`.  Con el modo hilo apagado hace lo mismo que el
         * camino de abajo. */
        D3DQCmd c;
        c.op = op;  c.poolFin = 0;
        c.p[0] = p0; c.p[1] = p1; c.p[2] = p2; c.p[3] = p3; c.p[4] = p4;
        c.w[0] = w0; c.w[1] = w1; c.w[2] = w2;
        c.w[3] = w3; c.w[4] = w4; c.w[5] = w5;
        d3dq_sincrona(&c);
#else
        emite(op, p0,p1,p2,p3,p4, w0,w1,w2,w3,w4,w5);
        d3dq_drena();
#endif
    }

    static unsigned int comoW(float f)
    { union { float f; unsigned int u; } x; x.f = f; return x.u; }

public:
    /* ================= ciclo de vida ================= */
    ULONG   Release()
    {
        /* Solo tiene sentido con productor y consumidor en el mismo hilo (el
         * cierre).  El shim no la usa: el device es del frontend. */
        d3dq_drena();
        return real->Release();
    }

    /* ================= presentacion ================== */
    /* Los cuatro argumentos son "Unused" en 360 (`d3d9.h:1861`) y el arbol
     * siempre pasa NULL, asi que no hay payload que copiar. */
    D3DVOID Present(CONST RECT *pSrc, CONST RECT *pDst, void *hWnd, void *pDirty)
    {
        (void)pSrc; (void)pDst; (void)hWnd; (void)pDirty;
        emite0(QOP_Present);
        D3DVOIDRETURN;
    }

    /* ================= creacion (sincronas) ========== */
    HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage,
                          D3DFORMAT Format, D3DPOOL Pool,
                          D3DTexture **ppTexture, HANDLE *pShared)
    {
        HRESULT hr = S_OK;
#if D3D_GRABADOR >= 3
        /* [etapa 5] Del almacen si hay, sin pasar por el buzon.  Ver
         * `d3dq_almacen_toma`. */
        if (g_d3dqVivo && !pShared &&
            d3dq_almacen_toma(Width, Height, Levels, Usage, Format, Pool, ppTexture))
            return S_OK;
#endif
        emiteSinc(QOP_CreateTexture, ppTexture, pShared, &hr, 0, 0,
                  Width, Height, Levels, Usage, (unsigned int)Format, (unsigned int)Pool);
        return hr;
    }
    HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
                               D3DVertexBuffer **ppVB, HANDLE *pShared)
    {
        HRESULT hr = S_OK;
        emiteSinc(QOP_CreateVertexBuffer, ppVB, pShared, &hr, 0, 0,
                  Length, Usage, FVF, (unsigned int)Pool, 0, 0);
        return hr;
    }
    HRESULT CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format,
                                      D3DMULTISAMPLE_TYPE MultiSample, DWORD Quality,
                                      BOOL Discard, D3DSurface **ppSurface,
                                      CONST D3DSURFACE_PARAMETERS *pParameters)
    {
        HRESULT hr = S_OK;
        emiteSinc(QOP_CreateDepthStencilSurface, ppSurface, (void *)pParameters, &hr, 0, 0,
                  Width, Height, (unsigned int)Format, (unsigned int)MultiSample,
                  Quality, (unsigned int)Discard);
        return hr;
    }
    HRESULT CreateVertexShader(CONST DWORD *pFunction, D3DVertexShader **ppShader)
    {
        HRESULT hr = S_OK;
        emiteSinc(QOP_CreateVertexShader, (void *)pFunction, ppShader, &hr, 0, 0,
                  0,0,0,0,0,0);
        return hr;
    }
    HRESULT CreatePixelShader(CONST DWORD *pFunction, D3DPixelShader **ppShader)
    {
        HRESULT hr = S_OK;
        emiteSinc(QOP_CreatePixelShader, (void *)pFunction, ppShader, &hr, 0, 0,
                  0,0,0,0,0,0);
        return hr;
    }
    HRESULT CreateVertexDeclaration(CONST D3DVERTEXELEMENT9 *pElements,
                                    D3DVertexDeclaration **ppDecl)
    {
        HRESULT hr = S_OK;
        emiteSinc(QOP_CreateVertexDeclaration, (void *)pElements, ppDecl, &hr, 0, 0,
                  0,0,0,0,0,0);
        return hr;
    }

    /* ================= lecturas que hacen AddRef ===== *
     * No se pueden espejar: D3D9 incrementa el contador de referencias al
     * devolver el objeto, asi que devolver una copia guardada dejaria el
     * refcount mal y el `Release()` del llamante soltaria de mas. */
    HRESULT GetRenderTarget(DWORD Index, D3DSurface **ppRT)
    {
        HRESULT hr = S_OK;
        emiteSinc(QOP_GetRenderTarget, ppRT, &hr, 0, 0, 0, Index, 0,0,0,0,0);
        return hr;
    }
    HRESULT GetDepthStencilSurface(D3DSurface **ppZ)
    {
        HRESULT hr = S_OK;
        emiteSinc(QOP_GetDepthStencilSurface, ppZ, &hr, 0, 0, 0, 0,0,0,0,0,0);
        return hr;
    }
    D3DVOID GetPixelShader(D3DPixelShader **ppShader)
    {
        emiteSinc(QOP_GetPixelShader, ppShader, 0, 0, 0, 0, 0,0,0,0,0,0);
        D3DVOIDRETURN;
    }

    /* ================= destinos de render ============ */
    D3DVOID SetRenderTarget(DWORD Index, D3DSurface *pRT)
    { emite(QOP_SetRenderTarget, pRT, 0,0,0,0, Index, 0,0,0,0,0); D3DVOIDRETURN; }

    D3DVOID SetDepthStencilSurface(D3DSurface *pZ)
    { emite(QOP_SetDepthStencilSurface, pZ, 0,0,0,0, 0,0,0,0,0,0); D3DVOIDRETURN; }

    D3DVOID Resolve(DWORD Flags, CONST D3DRECT *pSrcRect, D3DBaseTexture *pDstTex,
                    CONST D3DPOINT *pDstPoint, UINT DestLevel, UINT DestSliceOrFace,
                    CONST D3DVECTOR4 *pClearColor, float ClearZ, DWORD ClearStencil,
                    CONST D3DRESOLVE_PARAMETERS *pParameters)
    {
        emite(QOP_Resolve,
              d3dq_payload(pSrcRect,    sizeof(D3DRECT)),
              pDstTex,
              d3dq_payload(pDstPoint,   sizeof(D3DPOINT)),
              d3dq_payload(pClearColor, sizeof(D3DVECTOR4)),
              d3dq_payload(pParameters, sizeof(D3DRESOLVE_PARAMETERS)),
              Flags, DestLevel, DestSliceOrFace, comoW(ClearZ), ClearStencil, 0);
        D3DVOIDRETURN;
    }

    /* ================= escena y limpieza ============= */
    D3DVOID BeginScene() { emite0(QOP_BeginScene); D3DVOIDRETURN; }
    D3DVOID EndScene()   { emite0(QOP_EndScene);   D3DVOIDRETURN; }

    D3DVOID Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags,
                  D3DCOLOR Color, float Z, DWORD Stencil)
    {
        emite(QOP_Clear,
              d3dq_payload(pRects, Count * sizeof(D3DRECT)), 0,0,0,0,
              Count, Flags, Color, comoW(Z), Stencil, 0);
        D3DVOIDRETURN;
    }

    /* ================= estado fijo =================== */
    D3DVOID SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
    {
        if ((unsigned int)State < 404)
        {
            rsEspejo[State]   = Value;
            rsConocido[State] = 1;
        }
        emite(QOP_SetRenderState, 0,0,0,0,0, (unsigned int)State, Value, 0,0,0,0);
        D3DVOIDRETURN;
    }
    /* Del espejo si ya se conoce -- que es el caso en todas las lecturas
     * salvo la primera de cada estado-, y entonces no encola nada ni bloquea.
     * Si no, una orden sincrona: es correcta en los tres modos, al contrario
     * que preguntarle al device desde aqui. */
    D3DVOID GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue)
    {
        unsigned int s = (unsigned int)State;
        DWORD v = 0;

        if (s >= 404)
        {
            if (pValue) *pValue = 0;
            D3DVOIDRETURN;
        }
        if (!rsConocido[s])
        {
            emiteSinc(QOP_GetRenderState, &v, 0, 0, 0, 0, s, 0,0,0,0,0);
            rsEspejo[s]   = v;
            rsConocido[s] = 1;
        }
        if (pValue) *pValue = rsEspejo[s];
        D3DVOIDRETURN;
    }
    D3DVOID SetViewport(CONST D3DVIEWPORT9 *pViewport)
    {
        emite(QOP_SetViewport,
              d3dq_payload(pViewport, sizeof(D3DVIEWPORT9)), 0,0,0,0,
              0,0,0,0,0,0);
        D3DVOIDRETURN;
    }
    D3DVOID SetScissorRect(CONST RECT *pRect)
    {
        if (pRect) { scissorEspejo = *pRect; scissorConocido = 1; }
        emite(QOP_SetScissorRect,
              d3dq_payload(pRect, sizeof(RECT)), 0,0,0,0, 0,0,0,0,0,0);
        D3DVOIDRETURN;
    }
    D3DVOID GetScissorRect(RECT *pRect)
    {
        if (!scissorConocido)
        {
            emiteSinc(QOP_GetScissorRect, &scissorEspejo, 0,0,0,0, 0,0,0,0,0,0);
            scissorConocido = 1;
        }
        if (pRect) *pRect = scissorEspejo;
        D3DVOIDRETURN;
    }

    /* ================= texturas ====================== */
    D3DVOID SetTexture(DWORD Sampler, D3DBaseTexture *pTexture)
    { emite(QOP_SetTexture, pTexture, 0,0,0,0, Sampler, 0,0,0,0,0); D3DVOIDRETURN; }

    D3DVOID SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
    { emite(QOP_SetSamplerState, 0,0,0,0,0, Sampler, (unsigned int)Type, Value, 0,0,0); D3DVOIDRETURN; }

    /* ================= shaders ======================= */
    D3DVOID SetVertexShader(D3DVertexShader *pShader)
    { emite(QOP_SetVertexShader, pShader, 0,0,0,0, 0,0,0,0,0,0); D3DVOIDRETURN; }

    D3DVOID SetVertexShaderConstantF(UINT StartRegister, CONST float *pData, DWORD Vector4fCount)
    {
        emite(QOP_SetVertexShaderConstantF,
              d3dq_payload(pData, Vector4fCount * 4u * sizeof(float)), 0,0,0,0,
              StartRegister, Vector4fCount, 0,0,0,0);
        D3DVOIDRETURN;
    }
    D3DVOID SetPixelShader(D3DPixelShader *pShader)
    { emite(QOP_SetPixelShader, pShader, 0,0,0,0, 0,0,0,0,0,0); D3DVOIDRETURN; }

    D3DVOID SetPixelShaderConstantB(UINT StartRegister, CONST BOOL *pData, UINT BoolCount)
    {
        emite(QOP_SetPixelShaderConstantB,
              d3dq_payload(pData, BoolCount * sizeof(BOOL)), 0,0,0,0,
              StartRegister, BoolCount, 0,0,0,0);
        D3DVOIDRETURN;
    }
    D3DVOID SetPixelShaderConstantF(UINT StartRegister, CONST float *pData, DWORD Vector4fCount)
    {
        emite(QOP_SetPixelShaderConstantF,
              d3dq_payload(pData, Vector4fCount * 4u * sizeof(float)), 0,0,0,0,
              StartRegister, Vector4fCount, 0,0,0,0);
        D3DVOIDRETURN;
    }

    /* ================= geometria ===================== */
    D3DVOID SetVertexDeclaration(D3DVertexDeclaration *pDecl)
    { emite(QOP_SetVertexDeclaration, pDecl, 0,0,0,0, 0,0,0,0,0,0); D3DVOIDRETURN; }

    D3DVOID SetStreamSource(UINT StreamNumber, D3DVertexBuffer *pVB, UINT Offset, UINT Stride)
    { emite(QOP_SetStreamSource, pVB, 0,0,0,0, StreamNumber, Offset, Stride, 0,0,0); D3DVOIDRETURN; }

    D3DVOID DrawPrimitive(D3DPRIMITIVETYPE Type, UINT StartVertex, UINT PrimitiveCount)
    { emite(QOP_DrawPrimitive, 0,0,0,0,0, (unsigned int)Type, StartVertex, PrimitiveCount, 0,0,0); D3DVOIDRETURN; }

    /* El unico dibujo con datos del llamante: hay que copiarlos, porque el
     * que llama reutiliza su array en cuanto volvemos.  `g_fontVerts` de
     * Font.h es exactamente ese caso. */
    D3DVOID DrawPrimitiveUP(D3DPRIMITIVETYPE Type, UINT PrimitiveCount,
                            CONST void *pVertexData, UINT VertexStride)
    {
        unsigned int verts = 0;
        switch ((unsigned int)Type)
        {
        case D3DPT_POINTLIST:     verts = PrimitiveCount;         break;
        case D3DPT_LINELIST:      verts = PrimitiveCount * 2u;    break;
        case D3DPT_LINESTRIP:     verts = PrimitiveCount + 1u;    break;
        case D3DPT_TRIANGLELIST:  verts = PrimitiveCount * 3u;    break;
        case D3DPT_TRIANGLESTRIP: verts = PrimitiveCount + 2u;    break;
        case D3DPT_TRIANGLEFAN:   verts = PrimitiveCount + 2u;    break;
        default:                  verts = PrimitiveCount * 3u;    break;
        }
        emite(QOP_DrawPrimitiveUP,
              d3dq_payload(pVertexData, verts * VertexStride), 0,0,0,0,
              (unsigned int)Type, PrimitiveCount, VertexStride, 0,0,0);
        D3DVOIDRETURN;
    }

    /* ================= [xbox64] las tres de la etapa 3 ==================
     *
     * No son metodos del device: son las tres cosas que el shim hacia
     * tocando RECURSOS de D3D directamente (el vertex buffer bloqueado,
     * `LockRect` y el resolve de entrega), convertidas en ordenes.  Es el
     * mismo trato que GLideN64 da a `glTexSubImage2D` y a `glDrawArrays` con
     * vertices de cliente (ThreadedOpenGl/opengl_Wrapper.cpp:342-374): los
     * datos se COPIAN al pool y el consumidor los aplica.
     *
     * Por eso el productor ya no necesita `d3dq_sinc()` en ningun sitio: en
     * la etapa 4 no podria, porque no es el dueno del device. */

    /* Los vertices van al pool; el consumidor los copia a SU vertex buffer y
     * lanza el `DrawPrimitive` de siempre.  Ver gxd3d_consumidor_dibuja. */
    D3DVOID DibujaVerts(unsigned int prim, unsigned int nprim,
                        const void *verts, unsigned int nverts, unsigned int stride)
    {
        emite(QOP_DibujaVerts,
              d3dq_payload(verts, nverts * stride), 0,0,0,0,
              prim, nprim, nverts, stride, 0, 0);
        D3DVOIDRETURN;
    }

    /* Los texels ya destilados (ARGB lineal, `sw` de ancho) van al pool; el
     * consumidor hace el LockRect, la copia con pitch y el relleno del borde.
     * Solo se copian las `h` filas que se usan.  El puntero a la textura va
     * tal cual: es un objeto con vida propia, igual que en SetTexture. */
    D3DVOID SubeTextura(IDirect3DTexture9 *tex, const void *texels,
                        int w, int h, int sw, int wrap_s)
    {
        emite(QOP_SubeTextura,
              tex, d3dq_payload(texels, (unsigned int)(sw * h * 4)), 0,0,0,
              (unsigned int)w, (unsigned int)h, (unsigned int)sw,
              (unsigned int)wrap_s, 0, 0);
        D3DVOIDRETURN;
    }

    /* El frame esta completo: publicarlo.  Lo ejecuta el consumidor con el
     * render target del core todavia ligado. */
    D3DVOID Entrega()
    {
        emite0(QOP_Entrega);
        D3DVOIDRETURN;
    }

    /* [Wii64 443f68b] La copia de profundidad de DepthCopy.cpp: el consumidor
     * pone una fence detras del resolve de Z al hueco `k` (copia `seq`).  NO
     * espera a nada.  Ver gxd3d_consumidor_marca_z en xenon/gx_d3d9.cpp. */
    D3DVOID MarcaProfundidad(unsigned int k, unsigned int seq)
    {
        emite(QOP_MarcaProfundidad, 0,0,0,0,0, k, seq, 0,0,0,0);
        D3DVOIDRETURN;
    }
};

typedef D3DGrabador D3DDispositivo;

#else   /* D3D_GRABADOR 0: el arbol de antes, sin interponer nada */

typedef IDirect3DDevice9 D3DDispositivo;

#endif

/* El UNICO sitio donde se declara.  Antes estaba repetido en tres cabeceras
 * (`zlx/Draw.h`, `zlx/Font.h`, `xenon/d3d9_backend.h`) con el tipo escrito a
 * mano en cada una, que es como se cuela una discrepancia que el enlazador no
 * ve. */
extern D3DDispositivo * g_pd3dDevice;

/* El device de verdad, para quien lo necesite sin pasar por el grabador. */
IDirect3DDevice9 * d3d_real(void);

/* -- >>> SOLTAR UN RECURSO EN ORDEN CON LOS DIBUJOS ------------------------
 *
 * Equivalente de `GlDeleteTexturesCommand` de GLideN64: la DESTRUCCION de un
 * recurso es una orden mas del flujo, no una llamada aparte.
 *
 * POR QUE HACE FALTA.  `tex->Release()` no pasa por `g_pd3dDevice`, asi que
 * el anillo no lo ve: se ejecutaba en el acto mientras habia DrawPrimitive
 * encolados que todavia referencian esa textura.  Si el refcount llega a
 * cero ahi, el dibujo encolado lee memoria liberada.
 *
 * Y ES EL CAMINO QUE DISTINGUE A LOS JUEGOS, que es lo que lo senala:
 * GoldenEye recicla 34 texturas por segundo (`[TEX] mk=34 orph=34`) y Zelda
 * CERO (`orph=0/0`).  Zelda va bien con el anillo puesto y GoldenEye no.
 *
 * `d3dq_soltar(p)` encola el Release; con el anillo apagado lo hace en el
 * acto, que es el comportamiento de siempre.
 *
 * >>> EL PARAMETRO ES `D3DResource *` Y NO `void *`, Y ESO ES EL ARREGLO DE
 * UN CUELGUE.  La primera version tomaba `void *` y por dentro hacia
 * `((IUnknown *)p)->Release()`.  En Xbox 360 los recursos de D3D **no son
 * COM**: `D3DResource` es un struct llano con `Release()` NO virtual y sin
 * `IUnknown` por ninguna parte (`d3d9.h:935`, y `D3DTexture : D3DBaseTexture
 * : D3DResource`).  Asi que aquel cast despachaba por una vtable que no
 * existe: leia el primer campo del struct (`Common`) como si fuera el
 * puntero de vtable y saltaba a la nada.  Firma en el log:
 *
 *   Access violation reading location 0x0010000B
 *   Unhandled exception at 0x00000000
 *
 * Es el MISMO hecho que hace posible el interpositor por tipo -- que los
 * metodos no sean virtuales -- y aun asi lo contradije al escribir el cast.
 * Con el parametro tipado, un puntero que no sea recurso ya no compila. */
void d3dq_soltar(D3DResource *pRecurso);

/* -- >>> [xbox64] EL LADO DEL CONSUMIDOR DE LAS TRES ORDENES NUEVAS --------
 *
 * Las definen xenon/gx_d3d9.cpp (las dos primeras) y xenon/video_handoff.cpp
 * (`xenon64_video_resolve`).  Las llama `d3dq_ejecuta` con el grabador
 * puesto, y el propio shim directamente con el grabador a 0.
 *
 * ★★ REGLA DE ORO PARA ESTAS FUNCIONES: hablan con `dev`, el device de
 * VERDAD, y NUNCA con `g_pd3dDevice` -- que con el grabador puesto GRABA en
 * vez de ejecutar, y aqui dentro estamos a mitad de un vaciado -- ni llaman
 * a `d3dq_sinc()`/`d3dq_drena()`: un vaciado dentro de otro avanzaria
 * `g_d3dqCola` dos veces por la misma orden. */
void gxd3d_consumidor_dibuja(IDirect3DDevice9 *dev, unsigned int prim,
                             unsigned int nprim, const void *verts,
                             unsigned int nverts, unsigned int stride);
void gxd3d_consumidor_sube_tex(IDirect3DTexture9 *tex, const void *texels,
                               int w, int h, int sw, int wrap_s);
/* [Wii64 443f68b] La copia de profundidad: la fence tras su resolve, y la
 * revision sin bloqueo de que fences han pasado (en cada SWAP). */
void gxd3d_consumidor_marca_z(IDirect3DDevice9 *dev, unsigned int k, unsigned int seq);
void gxd3d_consumidor_revisa_z(IDirect3DDevice9 *dev);

/* -- >>> PUNTO DE SINCRONIZACION: hay que llamarlo ANTES de tocar un recurso
 *    de D3D directamente (`VB->Lock`, `tex->LockRect`, y cualquier otro
 *    acceso que no pase por `g_pd3dDevice`).
 *
 * ESTE ERA EL FALLO DE LA PRIMERA VERSION DEL ANILLO, y conviene que quede
 * escrito porque no es obvio: interponerse en el DEVICE no basta.  En D3D9
 * los recursos no se mutan a traves del device sino a traves del propio
 * objeto, asi que `DrawPrimitive` se encolaba mientras `VB->Lock()` y
 * `tex->LockRect()` escribian EN EL ACTO.  Cuando el dibujo encolado se
 * ejecutaba, leia vertices y pixeles ya sobrescritos.  Sintoma medido:
 * texturas parpadeando y geometria mal, con el anillo dando `descon=0
 * gordas=0` -- o sea con la codificacion perfecta-.
 *
 * GLideN64 no tiene este problema porque en OpenGL TODO pasa por el
 * contexto y su wrapper lo ve todo; en D3D9 hay una segunda puerta.
 *
 * COSTE MEDIDO, y es lo que hace viable el arreglo: el vertex buffer se
 * bloquea 20 veces por segundo (una por display list) para 4.491 dibujos, y
 * las texturas 11.  O sea ~31 drenajes mas sobre los 72 que ya habia, y
 * quedan ~225 dibujos por lote.  No mata el batching.
 *
 * Y en el modo 3 sigue siendo el reparto que queremos: el hilo de emulacion
 * prepara la display list N+1 mientras el trabajador dibuja la N. */
#if D3D_GRABADOR >= 3
/* [xbox64, etapa 4] UN NO-OP, y es lo correcto.  Desde la etapa 3 el shim
 * no tiene ni un `d3dq_sinc()`: ningun recurso se toca por la puerta de
 * atras.  Los unicos que quedan son los de video_handoff.cpp, que corren en
 * el CONSUMIDOR (`retro_run`) -- y ahi un vaciado no puede ir: con el
 * emulador grabando, el consumidor para en cada SWAP, y la cola nunca se
 * queda vacia del todo.  Quien vacia es el bucle de consumo de hilo_emu.cpp,
 * o `d3dq_drena` explicito en el modo sin hilo. */
#define d3dq_sinc()  ((void)0)
#elif D3D_GRABADOR >= 2
#define d3dq_sinc()  d3dq_drena()
#else
#define d3dq_sinc()  ((void)0)
#endif

#endif /* XENON64_D3D9_GRABADOR_H */
