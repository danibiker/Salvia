/*
 * xbox/plataforma.h  --  la frontera entre el wrapper libretro y Wii64.
 *
 * Todo lo que en Wii64 hacia `main/main_gc-menu2.cpp` vive ahora en
 * `plataforma.c`: los globales de configuracion, `Controls[4]`, los cuatro
 * *_INFO que reciben los plugins, y la secuencia de carga y descarga de ROM.
 *
 * El wrapper libretro (360/Xdk/xbox64/libretro_core.cpp) NO toca nada de
 * Wii64 directamente: solo llama a las cinco funciones de aqui.  El motivo es
 * que el wrapper compila como C++ y casi todo Wii64 es C89; cruzar esa
 * frontera en un solo sitio evita los sustos de `extern "C"` sueltos.
 *
 * ★ TIENE QUE SER VALIDO EN C.  Lo incluye `plataforma.c` (que es .c) y
 * `r4300/interupt.c`.  Nada de referencias, `bool` ni declaraciones a mitad
 * de bloque.
 */

#ifndef _XBOX64_PLATAFORMA_H_
#define _XBOX64_PLATAFORMA_H_

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/* Ciclo de vida                                                        */
/* -------------------------------------------------------------------- */

/* Una vez por sesion, desde retro_init.  Fija los valores por defecto de los
 * 23 globales de configuracion y llama a control_info_init(), que es lo que
 * hacia Initialise() en main_gc-menu2.cpp:615. */
void xbox64_init(void);

/* Desde retro_deinit. */
void xbox64_deinit(void);

/* ★ LAS OPCIONES QUE HAY QUE FIJAR **ANTES** DE CARGAR LA ROM.
 *
 * `Config_LoadConfig` copia `frameBufferTextures` a `OGL.frameBufferTextures`
 * desde DENTRO de `InitiateGFX`, que corre en `xbox64_load_rom`.  Fijarlas
 * despues no tendria efecto hasta la siguiente carga -- y eso se ve como
 * "la opcion no hace nada la primera vez", que es de los sintomas mas
 * desagradables de atar.
 *
 *   framebuffer  emulacion de framebuffer de glN64 (render a textura).
 *                La piden Paper Mario, Zelda y compania.
 *   sai2x        filtro 2xSaI de texturas.
 *   dynarec      1 = recompilador, 0 = interprete puro.
 *   pak          PLUGIN_* para los cuatro mandos (main/plugin.h).
 *   cpo          count_per_op, 1..3.  ★ Es el RELOJ de la CPU emulada:
 *                cuantas instrucciones ejecuta el invitado entre dos
 *                retrazos.  Con 2 se le da la MITAD que al hardware
 *                real, y un juego que decide su ritmo por si mismo baja
 *                de 60 a 20 fps por eso.  Ver la nota larga en
 *                plataforma.c.
 *   gfx_hilo     la core option `xbox64_gfx_thread`.  ★ En la etapa 1 del
 *                renderizado con hilo (el hilo grafico antiguo quitado y
 *                todavia sin el nuevo) NO MANDA SOBRE NADA: se guarda para
 *                la etapa 4.  Ver la nota de `s_opt_gfx_hilo` en
 *                plataforma.c.
 *
 * Si no se llama, se quedan los valores por defecto de `xbox64_init`. */
void xbox64_opciones(int framebuffer, int sai2x, int dynarec, int pak, int cpo,
                     int gfx_hilo, int gamehacks);

/* [Conker] Velocidad de CPU que pide el JUEGO (1..3; 0 = nada), puesta por
 * `GameSpecificHackSetup` (source/main/gamehacks.c) al leer la ROM.  Solo
 * manda si la opcion `xbox64_cpu_speed` sigue en su valor por defecto. */
void xbox64_db_count_per_op(int cpo);

/* [Jet Force Gemini] El JUEGO pide emulacion de framebuffer (1) o no (0).
 * La pone `GameSpecificHackSetup` al leer la ROM, que corre antes de
 * `gln64_set_options`.  Solo puede encenderla: con `xbox64_framebuffer`
 * activado (su defecto) no cambia nada. */
void xbox64_db_framebuffer(int on);

/* ★ QUE PLUGIN GRAFICO.  Los dos estan enlazados; ver xbox/gfx_dispatch.c.
 *
 * `xbox64_gfx_plugin_set` se puede llamar en cualquier momento, pero solo
 * tiene efecto en el siguiente `initiateGFX` -- o sea al cargar la ROM.  No
 * es pereza: cambiar de plugin exige tirar la cache de texturas, el
 * combinador y el estado del shim, y reconstruirlos desde una display list
 * que ya paso.  `xbox64_gfx_plugin_activo` devuelve el que esta puesto de
 * verdad, que es lo que hay que mirar para decidir comportamiento. */
#define XBOX64_GFX_GLN64  0
#define XBOX64_GFX_RICE   1
void xbox64_gfx_plugin_set(int cual);
int  xbox64_gfx_plugin_activo(void);

/* Sustituye el monton de texturas de 1 MB que Rice se monta en la
 * inicializacion estatica por uno de 32 MB, y sube con el el limite de
 * desalojo.  Idempotente.
 *
 * ★ TIENE QUE LLAMARSE ANTES DE LA PRIMERA TEXTURA, o sea desde
 * `initiateGFX`: sustituir el monton con texturas dentro las dejaria
 * apuntando a una arena huerfana.  El porque largo esta en xenon/gx_d3d9.cpp,
 * junto a la definicion. */
void xbox64_rice_tex_heap_setup(void);

/* El valor de la core option `xbox64_gfx_thread`.  En la etapa 1 no lo
 * consulta nadie; ver `s_opt_gfx_hilo` en plataforma.c. */
int  xbox64_gfx_hilo_opcion(void);

/* Desde retro_load_game.  Es loadROM() de main_gc-menu2.cpp:415, portado
 * entero: descarga lo anterior, formatea los paks, lee la ROM, init_memory,
 * los *_INFO, romOpen_* y cpu_init.  Devuelve 0 si todo fue bien; si no, el
 * codigo de error de rom_read(). */
int  xbox64_load_rom(const char *ruta);

/* Lo mismo con la ROM YA EN RAM, que es el camino normal desde que
 * `need_fullpath` esta a `false`: el frontend lee el fichero (o lo
 * descomprime) y pasa el bufer en `retro_game_info.data`.
 *
 * ★ El bufer solo es valido durante `retro_load_game` -- Salvia lo libera al
 * volver-- y esta funcion lo vuelca entero a la reserva de ROM-Cache antes
 * de devolver, asi que no hay que conservarlo.  `nombre` es opcional y solo
 * sale en el log.
 *
 * Mismo retorno que xbox64_load_rom. */
int  xbox64_load_rom_mem(const void *datos, unsigned int tam,
                         const char *nombre);

/* Desde retro_unload_game. */
void xbox64_unload_rom(void);

/* Desde retro_run.  Ejecuta hasta el siguiente limite de frame.
 *
 * ★ COMO SE SALE DEL EMULADOR SIN UN HILO.  `go()` (r4300.c:192) es un bucle
 * que no vuelve hasta que `r4300.stop`.  En vez de meter un hilo de
 * emulacion -- que en la fase 1 solo anyadiria una clase entera de fallos
 * nuevos -- se usa que `pure_interpreter()` (pure_interp.c:3308) es
 * REANUDABLE:
 *
 *     void pure_interpreter() {
 *         r4300.stop = 0;
 *         r4300.last_pc = r4300.pc;
 *         while (!r4300.stop) { prefetch(); interp_ops[...](); }
 *     }
 *
 * No tiene estado de entrada: arranca de `r4300.pc`, que es donde lo dejamos.
 * Asi que `xbox64_frame_boundary()` pone `r4300.stop = 1` desde la
 * interrupcion de VI, el bucle sale al terminar la instruccion en curso, y la
 * siguiente llamada continua exactamente donde iba.
 *
 * ★ Y POR ESO HACE FALTA LA BANDERA APARTE.  `r4300.stop` tambien es la
 * senyal de "el usuario ha salido" (stop_it() en main_gc-menu2.cpp:603).  Sin
 * distinguir las dos, un fin de frame pareceria un apagado. */
void xbox64_run_frame(void);

/* -------------------------------------------------------------------- */
/* Limite de frame                                                      */
/* -------------------------------------------------------------------- */

/* La llama `r4300/interupt.c` en VI_INT.  Ver xbox64_run_frame. */
void xbox64_frame_boundary(void);

/* Cuantas vueltas de go() se han dado desde la ultima consulta, y pone el
 * contador a cero.  Sano = una por frame.  Lo usa el resumen por segundo
 * del wrapper para distinguir "el VI llega cuando toca" de "go() vuelve
 * sin haber llegado a VI_INT". */
unsigned xbox64_vueltas_y_limpia(void);

/* El rango de `r4300.pc` visto en el limite de frame durante el ultimo
 * intervalo, y lo reinicia.  ★ Se muestrea DENTRO de
 * xbox64_frame_boundary, que es antes de que la interrupcion se entregue y
 * mueva el pc al vector de excepcion: ahi el valor todavia es del codigo
 * del invitado.  Si min y max estan clavados y juntos, es un bucle. */
void     xbox64_pc_rango(unsigned *min, unsigned *max);

/* Herramienta fija: vuelca los hilos de libultra ([HILOS]) buscandolos en la
 * RDRAM.  Solo existe con XBOX64_TRAZAS (este fichero no incluye
 * xenon64_diag.h: quien la llame, dentro de su `#if`); ver plataforma.c. */
void     xbox64_hilos_volcado(void);
/* Parte del mismo volcado: los ultimos 16 DMA del PI hacia la RDRAM
 * (gc_memory/dma.c) y las ultimas 12 tareas del RSP (gc_memory/memory.c,
 * do_SP_task).  Solo con XBOX64_TRAZAS; los llama xbox64_hilos_volcado. */
void     xbox64_pidma_volcado(void);
void     xbox64_sptask_volcado(void);

/* [log 39] Una linea con el estado de la CPU emulada y de las interrupciones
 * (PC, Count, proxima interrupcion, Status, Cause, MI, SP, DP).  La llama el
 * vigia de esperas largas desde HW1 mientras el emulador corre: lectura sin
 * sincronizar, solo para diagnostico.  Repetida cada 2 s dice si el PC se
 * mueve (el juego espera algo en un bucle) o no (el hilo esta parado en
 * codigo nuestro). */
void     xbox64_traza_cpu(void);

/* ★★ Contadores de xbox/plugins_fase1.c.  Cuentan cosas que SOLO ocurren
 * si el juego progresa, que es lo que ni `pc` ni `Count` distinguian.  El
 * porque de cada uno esta en la cabecera de plugins_fase1.c. */
extern unsigned xbox64_n_dlist;   /* tareas graficas del RSP  */
extern unsigned xbox64_n_alist;   /* tareas de audio          */
extern unsigned xbox64_n_rdp;     /* listas del RDP           */
extern unsigned xbox64_n_vista;   /* reprogramaciones del VI  */
extern unsigned xbox64_n_keys;    /* lecturas de mando        */

/* ★★ Coste de cada tarea del RSP, en ticks del time base.  Los define
 * `gc_memory/memory.c` alrededor de `doRspCycles`, dentro de `do_SP_task`;
 * salen en la linea [RSPT] y alimentan `[UTIL gfx]`.  No dependen de ningun
 * hilo: miden la tarea donde corra.
 *
 * `gfx` es un control positivo -- tiene que cuadrar con la suma de los cubos
 * de `[GLN64T]` --, y `audio` es el dato que no teniamos: hasta ahora vivia
 * sin separar dentro de los ~6,8 ms de "CPU emulada + audio", y es el que
 * decide si vale la pena un hilo de audio. */
extern volatile unsigned int xbox64_rsp_gfx_tb, xbox64_rsp_gfx_n;
/* ★ `max` = la PEOR tarea grafica del segundo, en ticks.  La suma no basta:
 * `gfx` partido por `n` dice 16,8 ms por tarea en OoT, y eso implicaria que
 * 20 frames de cada 61 pasan de 22 ms y se pierde uno de cada tres -- pero
 * el frontend mide `late=0/120` en ese mismo segundo.  O las tareas NO son
 * uniformes (unas pocas gordas y el resto pequenyas) o la suma miente.
 * Este contador lo dice sin ambiguedad. */
extern volatile unsigned int xbox64_rsp_gfx_max;
extern volatile unsigned int xbox64_rsp_aud_tb, xbox64_rsp_aud_n;
extern volatile unsigned int xbox64_rsp_otr_tb, xbox64_rsp_otr_n;

/* Lo consulta el wrapper para saber si el invitado ha pedido apagar de
 * verdad (RETRO_ENVIRONMENT_SHUTDOWN) en vez de haber cerrado un frame. */
int  xbox64_quiere_apagar(void);

/* -------------------------------------------------------------------- */
/* Log                                                                  */
/* -------------------------------------------------------------------- */

/* La UNICA salida de texto del core.  La implementa el wrapper
 * (360/Xdk/xbox64/libretro_core.cpp) sobre RETRO_ENVIRONMENT_GET_LOG_INTERFACE.
 *
 * Esta aqui y no en un log.h propio porque el punto es que haya un solo
 * sitio: `xbox/debug_libretro.c` reimplementa las seis funciones de
 * gui/DEBUG.h y todas acaban aqui.  Si aparece un segundo camino de log, el
 * orden de las lineas deja de ser fiable y comparar dos capturas -- que es
 * como se trabaja en este arbol -- deja de valer. */
void xbox64_log_line(const char *linea);

/* -------------------------------------------------------------------- */
/* Video (fase 2)                                                        */
/* -------------------------------------------------------------------- */

/* Adopta el device D3D9 del frontend y arranca el shim GX->D3D9.  Lo
 * implementa xbox/gfx_host.cpp, donde esta el porque del orden de los
 * cuatro pasos.  Devuelve 0 si algo falla.
 *
 * ★ Hay que llamarla ANTES de abrir la ROM: `InitiateGFX` ya toca el
 * backend. */
int  xbox64_gfx_init(void *device_frontend, int width, int height);
void xbox64_gfx_shutdown(void);

/* -------------------------------------------------------------------- */
/* Directorio `system` de libretro (fase 6)                              */
/* -------------------------------------------------------------------- */

/* Lo pide el wrapper con RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY y lo deja
 * aqui.  Salvia devuelve `<dirActual>\system` SIN separador final
 * (src/io/cfgloader.cpp:102).
 *
 * ★ QUIEN LO NECESITA: Rice_GX.  Su `GetPluginDir` (Video.cpp:180) tiene
 * cableado `sd:/wii64/`, que en Xbox 360 no existe, y de ahi salen las
 * rutas de `RiceVideoLinux.ini` -- su base de datos de arreglos por juego,
 * indexada por CRC de la ROM -- y de `RiceVideo.cfg`.
 *
 * ★ Y EL SINTOMA DE NO TENERLO ERA UN CUELGUE, no una opcion por defecto:
 * `Config.cpp:1791` abre el fichero para escribir y hace `fclose` SIN
 * comprobar NULL, asi que con una ruta invalida la consola se para dentro
 * de `_invalid_parameter_noinfo`.
 *
 * glN64 no la usa: su configuracion la lleva `xenon/gln64_config.cpp`. */
void xbox64_set_system_dir(const char *dir);

/* Devuelve el directorio CON separador final, listo para `strcat` del
 * nombre de fichero, que es como lo usa Rice.  Si el frontend no dio
 * ninguno, devuelve la cadena vacia: entonces `fopen` falla y Rice se queda
 * con sus opciones por defecto, que es degradarse, no romperse.
 *
 * ★ El separador es la BARRA INVERTIDA.  El XDK no convierte '/' en '\',
 * asi que una ruta con barras normales no abre ningun fichero. */
void xbox64_rice_config_dir(char *out);

#ifdef __cplusplus
}
#endif

#endif /* _XBOX64_PLATAFORMA_H_ */
