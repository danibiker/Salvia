/*
 * gfx_hilo.h  --  la display list en el nucleo 1, con UN solo invariante.
 *
 * ============================================================================
 * ★★★ EL INVARIANTE.  SI SE ROMPE, EL DISENYO ESTA MAL; NO SE LE PONE UNA
 *     GUARDA ENCIMA.
 * ============================================================================
 *
 *     EL HILO GRAFICO NO TOCA JAMAS ESTADO DEL EMULADOR.
 *
 * Lee una INSTANTANEA de la DMEM, ejecuta el plugin contra dos VENTANILLAS
 * privadas (`gfxh_mi_priv`, `gfxh_sp_priv`) y deja ahi el resultado.  Quien
 * aplica ese resultado a los registros de verdad es el hilo de emulacion, en
 * el mismo sitio y el mismo orden que hoy.
 *
 * ============================================================================
 * ★★ POR QUE LA VERSION ANTERIOR ERA UNA NYAPA, Y QUE SE TIRO
 * ============================================================================
 * La primera version AFIRMABA `MI_INTR_DP` en el despacho -- le decia al
 * invitado "el RDP ya termino de leer tu lista" cuando el hilo llevaba cero
 * comandos recorridos.  Era una apuesta, y cada vez que fallaba se le ponia
 * un detector encima: filtro de ucodes, pestillo, dos motivos de pestillo,
 * firma de la cabecera de la lista.  Cinco guardas para una decision mala.
 *
 * Y la apuesta no puede ganarse, porque hay un hecho estructural:
 * `do_SP_task` lee `MI_INTR_DP` en la instruccion SIGUIENTE al despacho.
 *
 *   - Si se honra el DP a +4000 `Count`, el hilo tiene 32 us.  CERO solape.
 *   - Si no se honra, se esta mintiendo, y la mentira hay que detectarla.
 *
 * No hay tercera opcion.  Asi que aqui se elige la unica que da solape y no
 * miente: **el DP se programa en el despacho, desde contexto valido, con un
 * plazo REALISTA**, y el manejador de `DP_INT` hace la valla.
 *
 * ★ Y eso NO es "falsear el temporizado".  Los 4000 `Count` son una ficcion
 * de mupen: en una N64 de verdad el RDP tarda del orden de un frame.  El
 * plazo que se usa aqui sale de lo que costo la lista ANTERIOR, o sea que se
 * ajusta solo y no inventa una constante.
 *
 * ============================================================================
 * ★★ EL LIMITE QUE NO SE PUEDE SALTAR: EL DEVICE ES DEL FRONTEND
 * ============================================================================
 * Salvia dibuja su quad y su menu con el MISMO IDirect3DDevice9 entre dos
 * `retro_run`, y en Xbox 360 no hay dos devices por proceso ni
 * D3DCREATE_MULTITHREADED.  El hilo tiene que haber terminado antes de que
 * `retro_run` devuelva.  Por eso `updateScreen` va tambien a la cola: la
 * global `gxd3d_wg_cursor` (xenon/gx_d3d9.cpp) la comparten
 * `_gDPWriteRDRAM`, dentro de la lista, y `VI_GX_renderCpuFramebuffer`,
 * dentro de `updateScreen`.
 *
 * ============================================================================
 * ★ LOS NUMEROS QUE JUSTIFICAN TODO ESTO (paso 0 y logs 4-6)
 * ============================================================================
 *   ventana de solape medida   OoT 10,9 ms   MM 13,1 ms   (min ~= med, plana)
 *   tarea grafica              OoT 13,4 ms   MM 21,5 ms
 *   con el hilo, medido        trabajo 2.126 us  ->  espera 7 us
 *
 * Objetivo: que `img` de Majora's Mask pase de 15 a 20.
 */

#ifndef _XENON_GFX_HILO_H_
#define _XENON_GFX_HILO_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================= ciclo de vida === */

/* Crea el hilo.  Idempotente.  Devuelve 1 si quedo en marcha.
 *
 * ★ API de Win32 a pelo (`CreateThread` + `XSetThreadProcessor` +
 * `ResumeThread`), como pcsxr-360.  NUNCA `SDL_CreateThread`: el SDL de este
 * arbol reparte hilos hardware con un CONTADOR GLOBAL
 * (libs/libSDLx360/.../SDL_systhread.c:38), o sea por orden de llegada.
 *
 * ★★ Y OJO CON ESO, QUE ES UN PROBLEMA REAL AUNQUE NOSOTROS NO USEMOS SDL:
 * ese contador empieza en 0, asi que el tercer hilo SDL que se cree aterriza
 * en el HW2 -- el nuestro --, y el segundo en el HW1, que es el de
 * emulacion.  Por eso `gfx_hilo_hw()` publica en que hilo hardware estamos
 * CORRIENDO de verdad, en vez de fiarnos de lo que pedimos. */
int  gfx_hilo_init(void);
void gfx_hilo_shutdown(void);

/* Al cargar una ROM.  NO recrea el hilo: es persistente a proposito -- en
 * pcsxr-360, crear y destruir por carga colgaba la consola dentro de
 * `CreateThread` de forma intermitente. */
void gfx_hilo_rom_nueva(void);

/* ★ ¿Me ejecuto DENTRO del hilo grafico?  Lo necesita el envoltorio de
 * `CheckInterrupts` en xbox/plataforma.c: el plugin llama de vuelta al
 * nucleo desde dentro de la lista, y esa puerta hay que cerrarla. */
int  gfx_hilo_soy_yo(void);

/* En que hilo hardware corre el consumidor, leido en ejecucion.  0 = aun no
 * ha corrido.  Ver la nota de `gfx_hilo_init`. */
unsigned gfx_hilo_hw(void);

/* Los ejecutores se INYECTAN desde xbox/plataforma.c, que es donde los
 * simbolos del plugin ya estan resueltos.  El vcxproj del XDK tiene
 * `CompileAs=Default`: los `.c` van como C y los `.cpp` como C++, y apostar
 * aqui sobre el decorado del simbolo seria un LNK2001 tardio. */
void gfx_hilo_set_ejecutores(void (*dlist)(void), void (*updscr)(void));

/* ============================================================ la sombra === */

/* ★★ LA INSTANTANEA DE LA DMEM, Y SON LOS 4 KB ENTEROS.
 *
 * `gfx_info.DMEM` apunta AQUI de forma permanente, porque el plugin copia
 * ese puntero una sola vez en `initiateGFX`.  El productor la rellena antes
 * de CADA lista, vaya al hilo o no, asi que los dos caminos leen lo mismo.
 *
 * ★ 4096 bytes y no los 64 del OSTask: el arbol hermano lo intento con 64
 * apoyandose en un censo de lecturas, y el censo valia para F3DEX2 y era
 * falso para el resto -- ZSortBOSS usa DMEM de 0x000 a 0xC20.  No estrechaba
 * la frontera, la falseaba (Xenon64/xenon/gfx_thread.cpp:212-229). */
#define GFX_HILO_DMEM_BYTES 4096
unsigned char *gfx_hilo_dmem(void);

/* ====================================================== las ventanillas === */

/* ★★ EL REGISTRO DE RESULTADO, y es todo lo que cruza de vuelta.
 *
 * `gfx_info.MI_INTR_REG` y `gfx_info.SP_STATUS_REG` apuntan aqui de forma
 * permanente.  El plugin escribe en ellas creyendo que son los registros del
 * emulador; el hilo de emulacion las lee al unir y aplica lo que proceda.
 *
 * ★ De `sp` se aplican SOLO LOS BITS QUE EL PLUGIN CAMBIO, comparando con la
 * semilla.  Copiar la palabra entera fue un fallo que costo un build (log 4):
 * entre el despacho y el join pasan hasta 13 ms, y por ahi corren tareas de
 * audio que dejan `sp_status` como debe estar.  Escribir encima la foto
 * previa lo borraba y colgaba el apreton de manos del SP. */
extern volatile unsigned int gfxh_mi_priv;
extern volatile unsigned int gfxh_sp_priv;

/* ============================================================== la cola === */

/* Encolan.  Devuelven 1 si se encolo (el llamante NO debe hacer el trabajo) y
 * 0 si hay que hacerlo en linea: sin hilo, opcion apagada, ucode no apto, o
 * llamado DESDE el propio hilo.
 *
 * ★ `dp_libre` = 0 si ya hay un `DP_INT` en la cola de eventos.  Es una
 * PRECONDICION, no una guarda de prudencia: `add_interupt_event_count`
 * (source/r4300/interupt.c) tiene el `if (get_event(type)) { }` VACIO de
 * upstream, asi que un segundo evento del mismo tipo deja uno con el `count`
 * atrasado y `next_interrupt` se queda en el pasado.  El detalle medido, en
 * la implementacion. */
int  gfx_hilo_encolar_dlist(int dp_libre);
int  gfx_hilo_encolar_updscr(void);

/* Espera a que la cola quede vacia y APLICA el registro de resultado.
 *
 * ★ Ojo: el join se hace aunque la cola ya estuviera vacia.  Que el hilo haya
 * llegado a tiempo ahorra la ESPERA, no el trabajo de aplicar el resultado.
 * (Un `return` temprano ahi fue un fallo real: en los frames en que el hilo
 * llegaba a tiempo, el resultado no se aplicaba nunca.) */
void gfx_hilo_vallar(void);

/* Aplica el registro de resultado de las listas que YA han terminado, sin
 * esperar a ninguna.
 *
 * ★ Es la mitad util de la valla.  Antes las dos cosas eran inseparables
 * -- cosechar exigia esperar --, y de ahi salia el bloqueo de 10,5 ms por
 * lista que se midio en Majora's Mask (log 11, `espera=3508` a 20/s).  Ver
 * la nota de `s_escena` en gfx_hilo.cpp. */
void gfx_hilo_cosechar(void);

/* ======================================================== el plazo del DP=== */

/* ★★★ ¿HAY UNA LISTA EN EL HILO A LA QUE LE FALTA PROGRAMAR SU `DP_INT`?
 *
 * Lo consulta `do_SP_task` (source/gc_memory/memory.c) justo donde hoy mira
 * `MI_INTR_DP`.  Devuelve 1 UNA SOLA VEZ por lista y se desarma: el objetivo
 * es que el bloque de programacion del DP corra exactamente igual que en el
 * camino sincrono -- mismo `remove_event`, mismo `check_interupt`, mismo
 * `update_count()` en el mismo sitio -- y solo cambie el plazo. */
int  gfx_hilo_tomar_pendiente(void);

/* El plazo, en unidades de `Count`, para el `add_interupt_event(DP_INT, ...)`
 * de esa lista.
 *
 * ★ Sale de lo que costo la lista ANTERIOR, no de una constante inventada:
 * el RDP emulado "tarda" lo que de verdad nos cuesta dibujar.  Se acota por
 * abajo a los 4000 del original (nunca antes de lo que el juego espera hoy)
 * y por arriba a un frame de `Count` (`vi_delay`), que es lo que tarda un RDP
 * real como mucho. */
unsigned gfx_hilo_plazo_dp(void);

/* ★★ `Count` por microsegundo de tiempo REAL DE EMULACION, medido.
 *
 * Lo escribe `resumen_por_segundo` (360/Xdk/xbox64/libretro_core.cpp) una
 * vez por segundo y lo lee `gfx_hilo_plazo_dp`.  A 0 significa "todavia no
 * hay medida" y el plazo cae al calculo antiguo.
 *
 * ★ NO es `vi_delay / 16667`.  Los 16.667 us son el ritmo que impone el
 * frontend POR FUERA; dentro de `retro_run` el hilo de emulacion recorre un
 * periodo de VI en mucho menos, y usar el numero de fuera daba un plazo 3x
 * corto.  El detalle esta junto al uso, en gfx_hilo.cpp. */
extern volatile unsigned int gfxh_count_por_us;

/* =========================================================== contadores === */

/* `espera` es el numero que dice si esto sirve: si sale casi igual que
 * `trabajo`, no hay solape.  `incompl` son listas que no llegaron a su
 * fullsync (`gDPFullSync` en glN64, `DLParser_RDPFullSync` en Rice) -- tiene
 * que salir 0; si no, ese ucode no puede ir al hilo y hay que excluirlo en
 * `xbox64_gfx_ucode_apto_para_hilo` (xbox/gfx_dispatch.c).
 *
 * ★★ Y ANTES QUE NINGUNO DE LOS DOS, MIRAR EL NUMERO DE COMANDOS: es el que
 * dice si el hilo esta recibiendo listas SIQUIERA.  Hay un `UPDSCR` por VI,
 * o sea 60/s pase lo que pase; si el contador sale clavado en 60 es que NO
 * ENTRA NI UNA `DLIST` y todo lo demas de esta linea mide el vacio.  Asi se
 * descubrio que Rice llevaba toda su vida corriendo en serie. */
extern volatile unsigned int gfxh_work_tb, gfxh_work_n, gfxh_work_max;
extern volatile unsigned int gfxh_wait_tb, gfxh_wait_n;

/* ★ SONDAS DEL DOBLE BUFER (linea `[GFXTH]`, campo `db:`).  El orden en que
 * hay que leerlas, que no es el orden en que salen:
 *
 *   1. `vuelo2` -- veces que se llegaron a tener DOS listas en vuelo.  Es el
 *      CONTADOR DE PASO: si sale 0, la profundidad 2 no se esta usando y
 *      cualquier cambio en `espera` viene de otro sitio.  Mirarlo primero.
 *   2. `cola2`  -- vallas que SI tuvieron que esperar, por las dos escenas
 *      ocupadas.  Es el resto irreducible con 2 escenas.
 *   3. `spcamb` -- ★★ LA QUE FALSA EL DISENO.  Con profundidad 2, la semilla
 *      de la lista N+1 se toma de un `sp_register` al que puede faltarle el
 *      resultado de la N.  El argumento para que eso sea inocuo es que los
 *      ucodes con yield no llegan al hilo y el resto de plugins no tocan el
 *      SP dentro de la lista, o sea `cambiados == 0` siempre.  Si `spcamb`
 *      sale distinto de cero el argumento es falso y hay que volver a
 *      profundidad 1 (poner ESCENAS a 1 en gfx_hilo.cpp). */
extern volatile unsigned int gfxh_vuelo2, gfxh_cola2, gfxh_sp_cambios;

/* ★★ LAS OTRAS DOS VALLAS.  `gfxh_wait_*` ya no es "la espera": es solo la
 * de `do_SP_task`, el `DP_INT` y los savestates.  Las otras dos van aparte
 * porque atacarlas es atacar cosas distintas:
 *
 *   `wmarco` -- fin de `retro_run`, 60/s.  Es el TECHO DEL SOLAPE: mientras
 *      exista, una lista no puede repartirse entre dos marcos del frontend,
 *      haya el doble bufer de escena que haya.  Si el doble bufer funciona
 *      y la espera simplemente se muda aqui, ESTE es el numero que sube, y
 *      el siguiente paso es el doble bufer del destino de render.
 *   `wesc`   -- las dos escenas ocupadas.  Resto irreducible con ESCENAS=2.
 */
extern volatile unsigned int gfxh_wmarco_tb, gfxh_wmarco_n;
extern volatile unsigned int gfxh_wesc_tb,   gfxh_wesc_n;

/* Igual que `gfx_hilo_vallar`, pero contabilizada aparte.  La llama SOLO el
 * fin de `retro_run`. */
void gfx_hilo_vallar_marco(void);

unsigned gfx_hilo_incompletas(void);

/* Listas que NO se encolaron porque ya habia un `DP_INT` en la cola.  Cero
 * es lo normal; si sube, el plazo del DP es demasiado largo para el ritmo de
 * ese juego. */
unsigned gfx_hilo_sin_dp(void);

#ifdef __cplusplus
}
#endif

#endif /* _XENON_GFX_HILO_H_ */
