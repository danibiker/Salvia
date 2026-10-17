/*
 * xbox/hilo_lista.h  --  la display list en paralelo con la CPU, en HW3.
 *
 * ★★★ ETAPA 5 DEL RENDERIZADO CON HILO.
 *
 * POR QUE (log 25, GoldenEye con glN64).  Con el modelo de mupen la CPU
 * emulada y el recorrido de la lista van EN SERIE en el hilo del emulador:
 * 12 ms de CPU + 7,6 ms de lista = 20,6 ms por VI, contra 16,7 de
 * presupuesto.  El consumidor de `retro_run` solo gasta 1 ms y el resto
 * espera.  Ningun reparto arregla una media que no cabe: hay que quitar la
 * lista del camino serie.  En una N64 el RSP corre en paralelo con la CPU;
 * aqui lo hace HW3.
 *
 *   HW2 emulador              HW3 este hilo                HW1 retro_run
 *   ------------              -------------                -------------
 *   do_SP_task: foto de       plugin: recorre la lista,    ejecuta ordenes
 *   DMEM y VI, encola, sigue  graba ordenes D3D            D3D hasta SWAP
 *   VI: encola updateScreen   updateScreen
 *   fin de VI: encola FIN     frame_end / SWAP / begin
 *   DP_INT: valla a SU lista
 *
 * LO QUE VIENE DEL HILO ANTIGUO (xenon/gfx_hilo.cpp, fuera del proyecto) y
 * se conserva porque ya estaba medido:
 *   - la SOMBRA de la DMEM con direccion FIJA: el plugin cachea el puntero
 *     en `initiateGFX`, asi que se vuelca una ESCENA a la sombra antes de
 *     cada lista, no se alterna la direccion;
 *   - las VENTANILLAS privadas del MI y del SP, con la cosecha POR MASCARA
 *     del SP (copiar la palabra entera colgaba el apreton de manos: log 4);
 *   - el DP_INT programado en el despacho con un PLAZO sacado de lo que
 *     costo la lista anterior, y la VALLA en su manejador.
 *
 * LO NUEVO, y el porque:
 *   - ESTE hilo es el UNICO que graba ordenes D3D: listas, updateScreen y
 *     tambien las marcas de frame del shim y el SWAP.  La cola de d3dq es de
 *     un productor; el emulador ya no graba nada.  Por eso incluso la lista
 *     que tiene que ir sincrona (la primera) va AQUI y el emulador espera.
 *   - La FOTO de los registros del VI en cada orden: el hilo no lee nada
 *     vivo del emulador salvo la RDRAM.
 *   - La DEUDA de DP: un DP_INT por lista, en orden, y nunca dos eventos
 *     DP_INT en la cola (el `if (get_event(type)) {}` vacio de upstream en
 *     `add_interupt_event_count` deja el segundo con el count atrasado:
 *     medido en el log 9 del hilo antiguo).  Si al despachar ya hay uno, el
 *     manejador reprograma el siguiente al vencer.
 *   - Ya no hay VALLA DE FIN DE MARCO, que era el techo del hilo antiguo:
 *     `retro_run` no tiene que esperar a la lista para devolver el device,
 *     porque la lista solo graba.
 *
 * Solo corre con las dos core options (`xbox64_gfx_thread` y
 * `xbox64_lista_paralelo`) y se decide AL CARGAR, antes de `initiateGFX`.
 * Apagada, todo es la etapa 4.
 */

#ifndef _XBOX_HILO_LISTA_H_
#define _XBOX_HILO_LISTA_H_

#ifdef __cplusplus
extern "C" {
#endif

/* --- Ciclo de vida (HW1) ------------------------------------------------ */

/* El hilo, una vez por vida del core (retro_init / retro_deinit). */
int  hilo_lista_crear(void);
void hilo_lista_destruir(void);

/* El modo de ESTA partida.  Se llama en `retro_load_game` ANTES de cargar la
 * ROM: `gfx_info_init` (xbox/plataforma.c) pregunta `hilo_lista_activo()`
 * para decidir a que apuntan DMEM, MI, SP y VI del plugin. */
void hilo_lista_configurar(int si);
int  hilo_lista_activo(void);

/* 1 si quien llama se ejecuta dentro de este hilo. */
int  hilo_lista_soy_yo(void);

/* --- Lo que el plugin ve (gfx_info, solo con el modo activo) ------------ */

#define HILO_LISTA_DMEM_BYTES 4096
unsigned char *hilo_lista_dmem(void);
unsigned long *hilo_lista_mi(void);
unsigned long *hilo_lista_sp(void);

/* Los 14 registros del VI que lee el plugin, en este orden. */
enum
{
    HL_VI_STATUS = 0, HL_VI_ORIGIN, HL_VI_WIDTH, HL_VI_INTR, HL_VI_CURRENT,
    HL_VI_TIMING, HL_VI_V_SYNC, HL_VI_H_SYNC, HL_VI_LEAP, HL_VI_H_START,
    HL_VI_V_START, HL_VI_V_BURST, HL_VI_X_SCALE, HL_VI_Y_SCALE, HL_VI_N
};
unsigned long *hilo_lista_vi(int indice);

/* `gfx_info.CheckInterrupts` con el modo activo: dentro de este hilo no hace
 * nada (el resultado vuelve por las ventanillas); fuera, `check_interupt`. */
void hilo_lista_check_interupt(void);

/* --- Entradas del plugin (xbox/gfx_dispatch.c, hilo del emulador) ------- */

enum
{
    HL_DLIST = 1,     /* asincrona (la primera de la partida, sincrona)  */
    HL_UPDSCR,        /* asincrona                                        */
    HL_VISTATUS,      /* asincrona                                        */
    HL_VIWIDTH,       /* asincrona                                        */
    HL_SHOWCFB,       /* asincrona                                        */
    HL_RDPLIST        /* SINCRONA: su DP se pasa al MI al volver          */
};

/* Encola la entrada.  Devuelve 1 si la tomo (el llamante NO la ejecuta) y 0
 * si hay que ejecutarla en linea: modo apagado o llamada desde este hilo. */
int  hilo_lista_encolar(int que);

/* Las ejecuta HW3.  Las define xbox/gfx_dispatch.c (el despacho de verdad a
 * glN64 o Rice). */
void xbox64_gfx_real(int que);
/* La base de la RDRAM que ve el plugin activo (xbox/gfx_dispatch.c).  Solo
 * desde este hilo, para la "foto" (ver `hilo_lista_sp_al_acabar`). */
void xbox64_gfx_rdram_base(unsigned char *base);

/* --- Las marcas de frame (xbox/hilo_emu.cpp, hilo del emulador) ---------- */

void hilo_lista_frame_begin(void);   /* al arrancar: gxd3d_frame_begin     */
void hilo_lista_frame_end(void);     /* fin de VI:   gxd3d_frame_end       */
void hilo_lista_swap_begin(void);    /* SWAP + gxd3d_frame_begin           */

/* Espera a que este hilo lo haya hecho TODO y cosecha.  En el punto seguro
 * del emulador (pausa, parada): despues, nadie graba. */
void hilo_lista_vaciar(void);

/* El trabajo de un VI del emulador, en us, para el plazo del DP. */
void hilo_lista_anota_vi(unsigned int us);

/* --- El DP (source/gc_memory/memory.c y source/r4300/interupt.c) -------- */

/* `do_SP_task`, tras la tarea grafica.  1 = hay que programar un DP_INT a
 * `hilo_lista_plazo_dp()`.  `hay_dp` = ya hay un DP_INT en la cola: entonces
 * 0, y lo reprogramara el manejador (ver `hilo_lista_dp_quedan`). */
int      hilo_lista_dp_programar(int hay_dp);
unsigned hilo_lista_plazo_dp(void);

/* El manejador de DP_INT (y el descongelado del DP).  Espera a que acabe la
 * lista mas antigua con el DP pendiente y cosecha.  Devuelve 1 si esa lista
 * llego a su fullsync (hay que levantar MI_INTR_DP) o si no habia deuda
 * (comportamiento de siempre). */
int  hilo_lista_dp_vence(void);
/* 1 si quedan listas con su DP pendiente: hay que programar el siguiente. */
int  hilo_lista_dp_quedan(void);

/* Tras cargar un savestate (emulador aparcado, hilo vacio): la deuda de DP
 * de antes ya no significa nada. */
void hilo_lista_olvidar_dp(void);

/* --- El SP (opcion `xbox64_lista_sp`) ------------------------------------
 *
 * ★ POR QUE (log 29, Cruis'n USA: poligonos que no se dibujan).  El HLE
 * levanta el SP_INT de la tarea grafica al volver del despacho, o sea
 * ANTES de que este hilo haya leido la lista.  Un juego que reutiliza su
 * bufer de display list o de vertices en cuanto recibe ese SP_INT escribe
 * encima de lo que la lista todavia tiene que leer.  La sonda `pisadas`
 * (firma de la cabeza de la lista al despachar y al recorrerla) lo mide.
 *
 * Con la opcion, el SP_INT de una tarea grafica se programa con el plazo del
 * DP y su manejador espera a que ESA lista acabe: el juego no recibe "RSP
 * terminado" hasta que lo esta.  Cuesta paralelismo en el juego que la
 * active; por eso es opcion y no va por defecto.
 *
 * ★ [Cruis'n USA] Con el valor 2 ("foto") el SP_INT no espera a la lista
 * sino a que este hilo COPIE la RDRAM al empezarla; el plugin dibuja desde la
 * copia (ver `s_foto` en hilo_lista.cpp).
 *
 *   al_acabar  HW1, al cargar y en caliente: 0 en el acto, 1 al acabar, 2 foto
 *   plazo      do_SP_task: el retraso del SP_INT (`normal` si no aplica)
 *   vence      el manejador de SP_INT: la valla */
void     hilo_lista_sp_al_acabar(int si);
/* [Cruis'n USA] El modo que pide el JUEGO (0 nada, 1 al acabar, 2 foto),
 * puesto por CRC en `GameSpecificHackSetup` (source/main/gamehacks.c) en cada
 * carga.  Solo manda si la opcion sigue en "desactivado". */
void     hilo_lista_sp_db(int modo);
unsigned hilo_lista_sp_plazo(unsigned normal);
/* 1 = entregar el SP_INT ya; 0 = la lista aun no acabo: el manejador lo
 * reprograma a `hilo_lista_sp_reintento()` y vuelve, SIN bloquear.  Con "al
 * acabar" no devuelve 0: espera a la lista y devuelve 1. */
int      hilo_lista_sp_vence(void);
unsigned hilo_lista_sp_reintento(void);
/* [log 35] El YIELD (ver hilo_lista.cpp).  `ceder`: update_SP, al poner
 * SIG0; 1 = entregar ya el SP_INT pendiente como cedido (SIG1).
 * `reanudar`: do_SP_task, tarea grafica con OS_TASK_YIELDED; 1 = no recorrer
 * la lista, esperar otra vez a la cedida. */
int      hilo_lista_sp_ceder(void);
int      hilo_lista_sp_reanudar(void);

/* --- Informe ----------------------------------------------------------- */
void hilo_lista_informe(unsigned frames);
/* [revision 2026-10-01] Una linea con el estado del hilo y de la cola, para
 * el vigia de esperas largas (aqui y en xbox/hilo_emu.cpp).  `por` dice quien
 * espera.  No cambia nada. */
void hilo_lista_traza_estado(const char *por);

#ifdef __cplusplus
}
#endif

#endif /* _XBOX_HILO_LISTA_H_ */
