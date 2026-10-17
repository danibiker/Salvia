/*
 * xbox/plugins_fase1.c  --  los puntos de entrada de los plugins, vacios.
 *
 * Wii64 no carga plugins: los ENLAZA, y cada plugin renombra sus funciones
 * exportadas a los nombres en minuscula que llama el nucleo
 * (glN64_GX/GFXPlugin.h:29 `#define UpdateScreen updateScreen`).
 * `main/plugin.c`, que es la version con dlsym, NO esta en el build de Wii64
 * -- es codigo muerto.
 *
 * O sea que los simbolos tienen que existir de verdad en el enlace.  Este
 * fichero los aporta para la fase 1, en la que no hay ni plugin grafico ni
 * audio ni input.
 *
 * ★ LA LISTA NO ES "TODO plugin.h": es EXACTAMENTE lo que referencia el
 * codigo que entra en el build.  Censado con grep sobre r4300/, gc_memory/,
 * rsp_hle/ y los .c de main/ que se compilan:
 *
 *     del nucleo hacia el plugin grafico
 *       updateScreen       interupt.c:360, memory.c:820
 *       processDList       memory.c:626 (comentada) y rsp_info_init
 *       processRDPList     memory.c:1562,1617,1656,1683
 *       viStatusChanged    memory.c:1911,1965,2006,2039
 *       viWidthChanged     memory.c:1919,1978,2017,2048
 *       showCFB            rsp_info_init
 *     del nucleo hacia el audio
 *       aiLenChanged       memory.c:2167
 *       aiDacrateChanged   memory.c:2214 y 7 mas
 *       processAList       rsp_info_init
 *       pauseAudio         ROM-Cache.c:67
 *       resumeAudio        ROM-Cache.c:68
 *     del nucleo hacia el input
 *       getKeys            pif.c
 *       readController     pif.c
 *       controllerCommand  pif.c
 *     de la carga de ROM (los llama xbox/plataforma.c)
 *       initiateGFX  initiateAudio  initiateControllers
 *       romOpen_*  romClosed_*  closeDLL_*
 *     de libgui/
 *       LoadingBar_showBar  ROM-Cache.c:232,267 y savestates_gc.c x12
 *
 * NO estan, a proposito, y si algun dia hacen falta saldra un LNK2001
 * legible: changeWindow, readScreen, aiReadLength, keyDown, keyUp,
 * captureScreen, drawScreen, moveScreen, getDllInfo_*, dllConfig_*,
 * dllTest_*, dllAbout_*.  Ninguno se referencia hoy.
 *
 * Los del RSP (doRspCycles, initiateRSP, romClosed_RSP, closeDLL_RSP) NO
 * estan aqui: los aporta rsp_hle/plugin.c, que SI se compila.
 *
 * C89.
 */

#include <stdio.h>

#include "../source/main/winlnxdefs.h"
#include "../source/main/plugin.h"
#include "../source/gc_memory/memory.h"    /* MI_register     */
#include "../source/r4300/interupt.h"      /* check_interupt  */
#include "xbox64_fase.h"
#include "plataforma.h"

/* ====================================================================== */
/* ★★ CONTADORES: LA MEDIDA QUE DECIDE SI LA FASE 2 EMPIEZA LIMPIA        */
/* ====================================================================== */
/* Sin plugin grafico la pantalla esta negra, y negro significa dos cosas
 * incompatibles: "el emulador va bien y no hay quien dibuje" o "el juego
 * esta atascado".  Las dos sondas anteriores no las distinguian:
 *
 *   `pc`     salia siempre 80000180 porque se muestreaba justo DESPUES de
 *            VI_INT, que es cuando el R4300 acaba de saltar al vector de
 *            excepcion general.
 *   `Count`  sale siempre el MISMO numero -- 48665520 -- porque el VI se
 *            programa EN UNIDADES DE Count (interupt.c: `next_vi = ... +
 *            vi_register.vi_delay`).  Ese delta es constante por
 *            construccion, ejecute el invitado lo que ejecute.
 *
 * Estos si discriminan, porque cuentan cosas que SOLO ocurren si el juego
 * progresa:
 *
 *   dlist    el juego ha montado una tarea grafica y el RSP la ha
 *            reconocido.  ★ Si esto sube, el emulador esta bien y lo unico
 *            que falta es el plugin: la fase 2 empieza limpia.
 *   alist    idem para audio.  Mario 64 arranca el audio muy pronto.
 *   rdp      el juego escribe en los registros del RDP.
 *   vista    el juego reprograma el VI (VI_STATUS o VI_WIDTH).  Si esto
 *            sube, tambien esta tocando VI_ORIGIN, y entonces que
 *            vi_origin no cambie ES un fallo nuestro.
 *   keys     el juego lee los mandos: llego al bucle principal.
 *
 * Con todos a cero, el invitado no ha pasado del arranque. */
unsigned xbox64_n_dlist = 0;
unsigned xbox64_n_alist = 0;
unsigned xbox64_n_rdp   = 0;
unsigned xbox64_n_vista = 0;
unsigned xbox64_n_keys  = 0;

/* ====================================================================== */
/* GRAFICOS                                                               */
/* ====================================================================== */
#if XBOX64_GFX_NULO

/* ★ updateScreen NO senyala el fin de frame.
 *
 * Podria: es la llamada de "hay imagen nueva".  Pero en la fase 2 este
 * fichero desaparece y quien define updateScreen es glN64_GX, asi que el
 * limite de frame se quedaria sin duenyo justo cuando empieza a importar.
 * El gancho vive en `r4300/interupt.c`, en VI_INT, que es donde esta el
 * ritmo de verdad y donde sigue estando en la fase 2. */
void updateScreen(void)   { }

/* ★★ NO ES UN NO-OP: SENYALA EL FIN DEL RDP.
 *
 * MEDIDO ANTES DE ESCRIBIRLO.  Con esta funcion vacia, Super Mario 64
 * arranca, programa el VI tres veces, ejecuta codigo variado
 * (pc=80246DD8..80317088), monta UNA display list... y a partir de ahi se
 * queda clavado: pc=80246DD8..80246DD8 y todos los contadores a cero,
 * indefinidamente.
 *
 * Esa es la firma exacta de un juego esperando a que el RDP termine.  El
 * `gDPFullSync` del final de la lista es quien pide la interrupcion, y en
 * mupen64 la senyala EL PLUGIN GRAFICO, no el nucleo:
 *
 *     glN64_GX/gDP.cpp:1505    *REG.MI_INTR |= MI_INTR_DP;
 *     glN64_GX/gDP.cpp:1507    CheckInterrupts();
 *
 * Sin plugin no la senyala nadie y el juego espera para siempre.
 *
 * ★ Se reproduce eso y solo eso: el bit MI_INTR_DP y la llamada a
 * check_interupt, que es justo lo que el nucleo le pasa al plugin en
 * `gfx_info.CheckInterrupts` (xbox/plataforma.c, gfx_info_init).  Ni un
 * `if` de mas.  La constante se toma del nucleo (gc_memory/memory.h:181)
 * y no del plugin (glN64_GX/N64.h:15), que valen lo mismo pero aqui
 * estamos del lado del nucleo.
 *
 * ★★ Y ES UNA SONDA, NO UN ARREGLO.  Comprueba la hipotesis "el nucleo
 * esta sano y lo unico que falta es dibujar": si con esto el juego sigue
 * avanzando -- mas dlists, keys, vi_origin moviendose -- queda probado.
 * En la fase 2 este fichero desaparece y quien lo senyala es glN64_GX, en
 * el sitio correcto: al final de la lista de verdad, no al principio. */
void processDList(void)
{
	xbox64_n_dlist++;

	MI_register.mi_intr_reg |= MI_INTR_DP;   /* gc_memory/memory.h:181 */
	check_interupt();
}
void processRDPList(void) { xbox64_n_rdp++;   }
void viStatusChanged(void){ xbox64_n_vista++; }
void viWidthChanged(void) { xbox64_n_vista++; }
void showCFB(void)        { }

BOOL initiateGFX(GFX_INFO Gfx_Info) { (void)Gfx_Info; return TRUE; }
void romOpen_gfx(void)   { }
void romClosed_gfx(void) { }
void closeDLL_gfx(void)  { }

#endif /* XBOX64_GFX_NULO */

/* ====================================================================== */
/* AUDIO                                                                  */
/* ====================================================================== */
/* ★ FASE 4: YA NO ESTAN AQUI.  Los nueve puntos de entrada del plugin de
 * audio -- aiDacrateChanged, aiLenChanged, processAList, initiateAudio,
 * romOpen_audio, romClosed_audio, closeDLL_audio, pauseAudio y
 * resumeAudio -- se han ido ENTEROS a `xbox/audio_n64.c`.
 *
 * Los NUEVE de una vez, igual que con los mandos: repartirlos entre dos
 * ficheros es como se acaba con medio plugin implementado y medio vacio. */

/* ====================================================================== */
/* MANDOS                                                                 */
/* ====================================================================== */
/* ★ FASE 4: YA NO ESTAN AQUI.  Los siete puntos de entrada del plugin de
 * mandos -- getKeys, readController, controllerCommand,
 * initiateControllers, romOpen_input, romClosed_input, closeDLL_input --
 * se han ido ENTEROS a `xbox/input_n64.c`, que es donde se implementan de
 * verdad.
 *
 * Se movieron los SIETE de una vez, no los que hacian falta: dejar medio
 * juego de puntos de entrada aqui y medio alli es como se llega a un
 * `Config_LoadConfig` vacio que cuesta tres compilaciones.  Si se busca
 * uno, esta en input_n64.c y en ningun otro sitio. */

/* ====================================================================== */
/* main/savestates_gc.c                                                   */
/* ====================================================================== */
/* ★ ESTE SE ME ESCAPO EN EL CENSO, Y LA LECCION ES EL METODO.
 *
 * Al dejar savestates_gc.c fuera del build busque quien llamaba a
 * `savestates_save`, `savestates_load`, `savestates_job` y
 * `savestates_select_slot` -- nombres que me invente -- y no salio nada.
 * El fichero define NUEVE funciones, y la que usa el nucleo es otra:
 * `r4300/interupt.c:345`.  Censar por los nombres que uno imagina no vale;
 * hay que censar por lo que el fichero EXPORTA.
 *
 * Lista completa de savestates_gc.c, por si vuelve a hacer falta:
 *   savestates_select_slot  savestates_get_filename  savestates_check_valid
 *   savestates_exists  savestates_queue_load  savestates_queued_load
 *   savestates_save  savestates_load_header  savestates_load
 * De las nueve, SOLO savestates_queued_load se referencia desde el build.
 *
 * Devolver 0 es la respuesta correcta en la fase 1, no una mentira: la
 * usa `if(!interrupt_unsafe_state && savestates_queued_load())` para saber
 * si hay un estado pendiente de cargar en este frame.  Como nadie puede
 * encolar uno todavia (savestates_queue_load no existe), la respuesta
 * verdadera es que no hay ninguno.
 *
 * ★ La firma va sin `void` a proposito: savestates.h:50 la declara
 * `int savestates_queued_load();` y en C las dos formas tienen que casar. */
int savestates_queued_load()
{
	return 0;
}

/* ====================================================================== */
/* libgui/                                                                */
/* ====================================================================== */
/* libgui/LoadingBar.cpp.  ★ Se declara EN LINEA en los dos sitios que la
 * usan (ROM-Cache.c:58 y savestates_gc.c:62), no por cabecera, asi que la
 * firma tiene que coincidir exactamente o el enlace la busca decorada de
 * otra forma. */
void LoadingBar_showBar(float percent, const char *string)
{
	(void)percent; (void)string;
}
