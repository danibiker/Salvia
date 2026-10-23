/*
 * xbox/plataforma.c  --  lo que hacia main/main_gc-menu2.cpp.
 *
 * Wii64 no tiene "un nucleo" y "una aplicacion" separados: main_gc-menu2.cpp
 * es a la vez el menu, el bucle principal Y el duenyo de casi todo el estado
 * global del emulador.  Al quitarlo hay que reponer su lista ENTERA, que es
 * lo que hay aqui, en el mismo orden que en el original y con las lineas
 * originales anotadas para poder contrastar:
 *
 *   1. Los 23 globales de configuracion  (main_gc-menu2.cpp:100-130)
 *      CON SUS VALORES POR DEFECTO       (main_gc-menu2.cpp:292-342)
 *   2. CONTROL Controls[4]               (main_gc-menu2.cpp:93)
 *   3. Los tres punteros fB*             (main_gc-menu2.cpp:181-183)
 *   4. Los cuatro *_INFO y sus init      (main_gc-menu2.cpp:506-602)
 *   5. loadROM()                         (main_gc-menu2.cpp:415)
 *   6. hasLoadedROM                      (main_gc-menu2.cpp:412)
 *
 * ★ LOS VALORES POR DEFECTO NO SON CERO.  Un `memset` o unas variables sin
 * inicializar dan un emulador que arranca y se porta mal en silencio:
 * audioEnabled=0, dynacore=0 (interprete lento), count_per_op=0 (el Count no
 * avanza NUNCA y no hay interrupciones).  Estan copiados uno a uno.
 *
 * LO QUE NO SE REPONE, Y POR QUE:
 *   - menu, video_mode_init, Initialise, ShutdownWii, main: los sustituye el
 *     frontend.
 *   - readConfig/writeConfig/setOption/handleConfigPair y la tabla OPTIONS:
 *     la configuracion viene de las core options de libretro (fase 5).  La
 *     tabla OPTIONS del original ademas usa `((char*)&dynacore)+3` para
 *     coger el byte bajo de un `unsigned long`, que es un truco de
 *     big-endian de 32 bits; no se copia.
 *   - gfx_set_fb / gfx_set_window: son del plugin grafico (glN64.cpp:66).
 *     Fase 2.
 *
 * C89: declaraciones al principio del bloque que toca.
 */

#include <stdio.h>
#include <string.h>

#include "../source/main/winlnxdefs.h"
#include "../source/main/plugin.h"
#include "../source/main/rom.h"
#include "../source/main/ROM-Cache.h"
#include "../source/main/gamehacks.h"       /* xbox64_gamehacks_habilitar */
#include "../source/main/wii64config.h"      /* arrastra timers.h */
#include "../source/gc_memory/memory.h"
#include "../source/gc_memory/pif.h"
#include "../source/gc_memory/flashram.h"
#include "../source/gc_memory/Saves.h"
#include "../source/r4300/r4300.h"
#include "../source/r4300/interupt.h"
#include "../source/fileBrowser/fileBrowser.h"
#include "../source/gui/DEBUG.h"
#include "../source/xenon64_diag.h"       /* XBOX64_TRAZAS */

#include "plataforma.h"
#include "filebrowser_xbox.h"
#include "xbox64_fase.h"
#include "hilo_lista.h"                      /* [etapa 5] gfx_info en paralelo */
#if XBOX64_DYNAREC
/* La autoprueba del recompilador; definida con enlace C en ppc/Wrappers.c
 * (declarada alli en Wrappers.h).  Se declara aqui y no se incluye
 * Wrappers.h: esa cabecera define macros de nombre generico (`Index`,
 * `Context`, `Status`, `Count`...) que no tienen por que entrar en este
 * fichero. */
void xbox64_autoprueba_dynarec(void);
#endif
#if !XBOX64_GFX_NULO
/* gln64_set_options y gln64_apply_rdram_size.  Es valido en C. */
#include "../xenon/gfx_plugin_sel.h"
#endif

/* ====================================================================== */
/* 1. LOS GLOBALES DE CONFIGURACION                                       */
/*    Declarados en source/main/wii64config.h; los definia                */
/*    main_gc-menu2.cpp:100-130.                                          */
/* ====================================================================== */

/* ★ SOLO LOS QUE ALGUIEN LEE.  wii64config.h declara 23 `extern`, pero un
 * `extern` sin definicion no es un error mientras nadie lo referencie, y
 * definir catorce `char` que no lee nadie es ruido que luego hay que
 * auditar.  Censado sobre el codigo que ENTRA en el build:
 *
 *   showFPSonScreen  printToScreen  screenMode                glN64_GX/VI.cpp
 *   renderCpuFramebuffer  nativeOutput                        glN64_GX/VI.cpp
 *   glN64_useFrameBufferTextures  glN64_use2xSaiTextures      Config_linux.cpp
 *                                                             OpenGL.cpp
 *   enablegDPUpdateColorImage       gDP.cpp  Y  main/rom_gc.c:183 (lo ESCRIBE
 *                                   un gamehack por ROM, asi que hace falta
 *                                   ya en la fase 1)
 *   saveStateDevice                 main/savestates_gc.c
 *   audioEnabled  autoSave          los usa esta capa
 *
 * NO se definen, y al que los pida le saldra un LNK2001 legible en vez de un
 * cero silencioso: miniMenuActive, menuActive, saveEnabled, creditsScrolling,
 * nativeSaveDevice, videoMode, padAutoAssign, padType[], padAssign[],
 * pakMode[], loadButtonSlot, padNeedScan, wpadNeedScan, drcNeedScan,
 * shutdown.  La lista completa con sus valores por defecto esta en
 * FASE0.txt seccion 5.1c.
 *
 * printToSD tampoco: lo DEFINE gui/DEBUG.c:16, no lo declara.  Lo repone
 * nuestro xbox/debug_libretro.c. */

char audioEnabled;
char saveStateDevice;
char autoSave;

/* ★★ [FASE 2] OCHO GLOBALES QUE ANTES SE DEFINIAN AQUI Y AHORA NO.
 *
 * Con el plugin grafico dentro, el lado C++ los define POR SU CUENTA:
 *
 *     showFPSonScreen  printToScreen  renderCpuFramebuffer   glN64_GX/VI.cpp
 *     enablegDPUpdateColorImage                              glN64_GX/gDP.cpp
 *     screenMode  nativeOutput                               xenon/gln64_glue.cpp
 *     glN64_useFrameBufferTextures  glN64_use2xSaiTextures   xenon/gln64_glue.cpp
 *
 * Mientras `wii64config.h` no llevaba `extern "C"`, esas definiciones y
 * las de aqui eran SIMBOLOS DISTINTOS -- dos variables con el mismo
 * nombre -- y el enlazador no decia nada.  Lo que yo inicializaba aqui no
 * lo leia nadie: el plugin usaba su copia.
 *
 * Ya con enlace C unificado serian duplicados de verdad (LNK2005), asi
 * que se quedan en UN solo sitio, y el sitio es el que el plugin ya
 * tenia.  ★ Los valores por defecto COINCIDEN con los que ponia esta
 * capa: VI.cpp y gDP.cpp los dejan a 0 (HIDE/DISABLE) y gln64_glue.cpp
 * pone SCREENMODE_4x3 y NATIVEOUT_DISABLE.  O sea que no cambia el
 * comportamiento, solo desaparecen los fantasmas.
 *
 * Los de glN64 se fijan de verdad por `gln64_set_options()`, que escribe
 * en los del glue; ver xbox64_load_rom. */

/* ====================================================================== */
/* 2. DATOS DE LOS PLUGINS                                                */
/* ====================================================================== */

CONTROL Controls[4];                 /* main_gc-menu2.cpp:93  */

/* ★ LAS CORE OPTIONS, GUARDADAS HASTA QUE HACEN FALTA.
 *
 * El frontend las entrega cuando quiere -- normalmente en `retro_run`,
 * antes de cargar nada -- y varias de ellas solo se pueden APLICAR en
 * momentos concretos (las de glN64, antes de `InitiateGFX`).  Guardarlas
 * aqui y aplicarlas en su sitio es lo que evita el "la opcion no hace
 * nada hasta que reinicias".
 *
 * Los valores iniciales son los que tenia el port cableados antes de que
 * hubiera opciones, para que no cambie el comportamiento de nadie que no
 * las toque. */
static int s_opt_framebuffer = 0;
static int s_opt_sai2x       = 0;
static int s_opt_dynarec     = 1;
static int s_opt_pak         = PLUGIN_MEMPAK;
static int s_opt_count_per_op = COUNT_PER_OP_2;

/* ★ [Conker, logs 13-16] LA VELOCIDAD DE CPU POR JUEGO.  0 = el juego no
 * pide nada.  La pone `GameSpecificHackSetup` (gamehacks.c) por CRC al leer
 * la ROM, y manda SOLO si la opcion sigue en su valor por defecto ("2
 * (Wii64)"): un 1 o un 3 elegido por el usuario -- tambien desde el .opt por
 * juego de Salvia -- gana siempre.  No hay SET_VARIABLE en libretro, asi que
 * es la misma regla que la base de datos de pcsxr-360.  Se guarda aqui, y no
 * se escribe solo en `count_per_op`, porque `xbox64_opciones` lo reescribe en
 * cada cambio de opciones a mitad de partida. */
static int s_db_count_per_op = 0;

static unsigned long cpo_efectivo(void)
{
	if (s_db_count_per_op && s_opt_count_per_op == COUNT_PER_OP_2)
		return (unsigned long)s_db_count_per_op;
	return (unsigned long)s_opt_count_per_op;
}

void xbox64_db_count_per_op(int cpo)
{
#if XBOX64_TRAZAS
	char l[128];
#endif
	s_db_count_per_op = (cpo >= COUNT_PER_OP_1 && cpo <= COUNT_PER_OP_3) ? cpo : 0;
	count_per_op = cpo_efectivo();
#if XBOX64_TRAZAS
	if (s_db_count_per_op)
	{
		_snprintf(l, sizeof(l),
		          "[xbox64] por juego: velocidad de CPU %d%s",
		          s_db_count_per_op,
		          (s_opt_count_per_op == COUNT_PER_OP_2) ? ""
		              : " IGNORADA (la opcion no esta en su valor por defecto)");
		l[sizeof(l) - 1] = 0;
		xbox64_log_line(l);
	}
#endif
}

/* ★ [Jet Force Gemini] LA EMULACION DE FRAMEBUFFER POR JUEGO.
 *
 * JFG pinta la silueta de la sombra de los personajes en un color image
 * auxiliar.  Sin texturas de framebuffer glN64 no guarda ni repinta los
 * buffers, asi que ese dibujo cae en el EFB en (0,0) y se queda en la imagen:
 * la silueta en la esquina superior izquierda y parpadeos a negro.  Con la
 * opcion encendida va bien (comprobado por el usuario).  GLideN64 tampoco lo
 * evita sin emulacion de framebuffer: con `enable = 0` lo dibuja todo en un
 * unico buffer de pantalla (FrameBuffer.cpp:781, GraphicsDrawer.cpp:851), y
 * a JFG le da trato por juego en su ini.
 *
 * La base de datos solo puede ENCENDERLA: el valor efectivo es
 * `s_opt_framebuffer || s_db_framebuffer`.  Desde que la opcion va activada
 * por defecto (2026-10-13) solo cambia algo si el usuario la apaga.  Se aplica en
 * `carga_tras_fijar_rom`, que es despues de `rom_read` (donde corre
 * GameSpecificHackSetup) y antes de `InitiateGFX`. */
static int s_db_framebuffer = 0;

void xbox64_db_framebuffer(int on)
{
	s_db_framebuffer = on ? 1 : 0;
#if XBOX64_TRAZAS
	if (s_db_framebuffer)
		xbox64_log_line(s_opt_framebuffer
		                ? "[xbox64] por juego: emulacion de framebuffer (ya estaba activada)"
		                : "[xbox64] por juego: emulacion de framebuffer ACTIVADA");
#endif
}

/* ★★★ LA OPCION DEL RENDERIZADO CON HILO (`xbox64_gfx_thread`).
 *
 * El hilo grafico que habia aqui -- la display list recorrida en HW2, con
 * sombra de DMEM, ventanillas privadas del MI/SP y DP_INT diferido -- SE HA
 * QUITADO ENTERO (2026-09-30).  No podia repartir la carga: `retro_run`
 * tenia que esperar la lista entera antes de devolver el device, y una
 * lista de 20-30 ms no cabe en un marco de 16,7.  Perfil medido en Rush:
 * `[UTIL run] 173 51 179 51`.
 *
 * Se sustituye por el modelo de mupen64plus-libretro-nx
 * (`EnableThreadedRenderer`): el EMULADOR entero en su propio hilo, la API
 * grafica convertida en una cola de comandos, y `retro_run` -- que corre en
 * el hilo dueno del device -- ejecutandolos hasta cada swap.  Se hace en
 * cuatro etapas medibles; el plan esta en el historial de la sesion.
 *
 * ★ EN LA ETAPA 1 ESTA OPCION NO MANDA SOBRE NADA.  Se conserva el valor
 * porque la etapa 4 lo reutiliza, y porque quitarlo cambiaria las opciones
 * guardadas del usuario.  Encendida y apagada tienen que dar exactamente lo
 * mismo: ese es el control de la etapa. */
static int s_opt_gfx_hilo = 1;

int xbox64_gfx_hilo_opcion(void) { return s_opt_gfx_hilo; }

/* [Wii64 443f68b] Aqui estuvo la opcion `xbox64_depth_rdram` ("Borrado de
 * profundidad en RDRAM"), nuestro arreglo de la camara de Donkey Kong 64.  Se
 * quito al portar ese commit, que trae su propia escritura del borrado en la
 * RDRAM (`_gDPFillRDRAM` y DepthCopy.cpp en source/glN64_GX). */

/* ★ AQUI ESTUVO `xbox64_framebuffer_opcion()`, para que Rice pudiera leer
 * la misma opcion que glN64 recibe por `gln64_set_options`.  Se quito: con
 * Rice_GX sin soporte de emulacion de framebuffer (lo dice su autor) la
 * opcion no mandaria sobre nada, y un accesor que nadie usa es peor que el
 * hueco que documenta.  El detalle esta en Rice_GX/Config.cpp, en
 * `GenerateFrameBufferOptions`. */

void xbox64_opciones(int framebuffer, int sai2x, int dynarec, int pak, int cpo,
                     int gfx_hilo, int gamehacks)
{
	int i;

	/* ★ En caliente y sin estado a medias: `timers.c` consulta el puntero
	 * del parche una vez por frame, asi que apagarlo deja de aplicarlo a
	 * partir del frame siguiente y ya esta.  Lo que ya escribio se queda,
	 * que es lo correcto: el parche solo pone valores, no los deshace. */
	xbox64_gamehacks_habilitar(gamehacks);

	s_opt_framebuffer = framebuffer ? 1 : 0;
	s_opt_sai2x       = sai2x ? 1 : 0;
	s_opt_dynarec     = dynarec ? 1 : 0;
	s_opt_pak         = pak;
	s_opt_gfx_hilo    = gfx_hilo ? 1 : 0;

	if (cpo >= COUNT_PER_OP_1 && cpo <= COUNT_PER_OP_3)
		s_opt_count_per_op = cpo;

	/* ★ EN CALIENTE SI, PERO CON UN MATIZ: `genUpdateCount` empotra el
	 * desplazamiento EN EL CODIGO GENERADO (srwi 2 / srwi 1 / mulli), asi
	 * que los bloques ya compilados siguen con el valor viejo.  El cambio
	 * se completa segun se recompila.  Para una medida limpia, cambiar la
	 * opcion y recargar el juego.  [Conker] Con el ajuste por juego si la
	 * opcion sigue por defecto: ver `cpo_efectivo`. */
	count_per_op = cpo_efectivo();

	/* ★ `dynacore` SI se puede tocar en caliente y el resto no.
	 *
	 * `r4300.c` lo mira en cada vuelta del bucle para decidir si
	 * interpreta o salta al codigo recompilado, asi que cambiarlo entre
	 * frames es legal.  Lo que NO seria legal es cambiarlo a mitad de un
	 * bloque recompilado, y eso no puede pasar: esto se llama desde
	 * `retro_run`, que solo corre entre frames.
	 *
	 * ★ POR NOMBRE Y NO POR NUMERO: el enum tiene TRES valores
	 * (wii64config.h:120-122) y el 0 no es "interprete", es
	 * DYNACORE_INTERPRETER, que es otro camino distinto -- el interprete
	 * que SI usa la maquinaria de bloques.  La pareja que tiene sentido
	 * ofrecer es dynarec / interprete puro, que ademas es el A/B con el
	 * que se depuro la fase 3. */
	dynacore = s_opt_dynarec ? DYNACORE_DYNAREC : DYNACORE_PURE_INTERP;

	/* El pak de los cuatro mandos.  Tambien vale en caliente: `pif.c` lo
	 * consulta en cada comando del PIF. */
	for (i = 0; i < 4; i++)
		Controls[i].Plugin = s_opt_pak;
}

static GFX_INFO     gfx_info;        /* main_gc-menu2.cpp:95  */
static AUDIO_INFO   audio_info;      /* ★ en el original NO es static, porque
                                      * gc_audio/audio.c lo leia por `extern`.
                                      * Ese fichero no entra, y nadie mas del
                                      * build lo referencia (censado), asi que
                                      * aqui si puede ser static. */
static CONTROL_INFO control_info;
static RSP_INFO     rsp_info;

/* main_gc-menu2.cpp:181-183.  ★ A NULL, no a funciones tontas.
 * gc_memory/memory.c los usa SIEMPRE tras comprobarlos
 * (memory.c:602, :652, :653), asi que NULL es el valor correcto y significa
 * "el plugin grafico no ofrece la extension de framebuffer".  La version
 * Linux (main/plugin.c:115) los pone a stubs porque alli se resuelven por
 * dlsym; aqui no. */
void (*fBRead)(DWORD addr) = NULL;
void (*fBWrite)(DWORD addr, DWORD size) = NULL;
void (*fBGetFrameBufferInfo)(void *p) = NULL;

/* main_gc-menu2.cpp:412.  Lo leen ROM-Cache.c:69 y
 * fileBrowser/fileBrowser-libfat.c:38. */
BOOL hasLoadedROM = FALSE;

/* ====================================================================== */
/* 3. ESTADO DE ESTA CAPA                                                 */
/* ====================================================================== */

/* El fichero de ROM que se le pasa a Wii64.  Vive aqui porque ROM-Cache
 * guarda el puntero y lo usa despues de la carga. */
static fileBrowser_file s_rom_file;
static fileBrowser_file s_save_dir;

/* ★ La bandera de fin de frame, separada de r4300.stop.
 * Ver el comentario largo de xbox64_run_frame en plataforma.h. */
static volatile int s_frame_hecho;
static int          s_apagar;

/* Cuantas vueltas de go() se han dado desde la ultima consulta.  Sano = una
 * por frame; mas alto significa que go() vuelve sin llegar a VI_INT.  Lo
 * lee el resumen por segundo del wrapper. */
static unsigned     s_vueltas;

/* ====================================================================== */
/* 4. LOS CUATRO *_INFO                                                   */
/*    Portados campo a campo de main_gc-menu2.cpp:506-602.  Si falta un   */
/*    campo el plugin escribe en NULL y el fallo sale lejos del origen.   */
/* ====================================================================== */

/* ★★★ [Xbox360] AQUI HABIA TRES FUNCIONES PUENTE DEL HILO GRAFICO, Y SE
 * HAN QUITADO (2026-09-30): `xbox64_gfx_check_interupt`, `xbox64_gfx_dlist`
 * y `xbox64_gfx_updatescreen`.  Existian para que la display list pudiera
 * recorrerse en otro nucleo: una sombra de la DMEM, dos ventanillas
 * privadas para el MI y el SP, y un puente que pasaba los bits al registro
 * de verdad.
 *
 * Con el hilo fuera, el cableado vuelve a ser el de Wii64
 * (main_gc-menu2.cpp:506-540 y :596), y eso ARREGLA un fallo que tambien
 * tenia el camino sin hilo: glN64 ESCRIBE en la DMEM (S2DEX en
 * gSP.cpp:2013-2127, F5Rogue en RSP.cpp:380-382, ZSortBOSS).  Con la sombra
 * esas escrituras no llegaban nunca a `SP_DMEM`: la lista siguiente las
 * pisaba con su memcpy y tampoco entraban en el savestate, que guarda
 * `SP_DMEM`. */

static void gfx_info_init(void)      /* main_gc-menu2.cpp:506 */
{
	gfx_info.MemoryBswaped = TRUE;
	gfx_info.HEADER = (BYTE*)&ROM_HEADER;
	gfx_info.RDRAM = (BYTE*)rdram;
	/* ★ La DMEM VIVA, como en Wii64.  Ver la nota de arriba sobre las
	 * escrituras de glN64 en la DMEM que la sombra perdia. */
	gfx_info.DMEM = (BYTE*)SP_DMEM;
	gfx_info.IMEM = (BYTE*)SP_IMEM;
	gfx_info.MI_INTR_REG = &(MI_register.mi_intr_reg);
	gfx_info.DPC_START_REG = &(dpc_register.dpc_start);
	gfx_info.DPC_END_REG = &(dpc_register.dpc_end);
	gfx_info.DPC_CURRENT_REG = &(dpc_register.dpc_current);
	gfx_info.DPC_STATUS_REG = &(dpc_register.dpc_status);
	gfx_info.DPC_CLOCK_REG = &(dpc_register.dpc_clock);
	gfx_info.DPC_BUFBUSY_REG = &(dpc_register.dpc_bufbusy);
	gfx_info.DPC_PIPEBUSY_REG = &(dpc_register.dpc_pipebusy);
	gfx_info.DPC_TMEM_REG = &(dpc_register.dpc_tmem);
	gfx_info.VI_STATUS_REG = &(vi_register.vi_status);
	gfx_info.VI_ORIGIN_REG = &(vi_register.vi_origin);
	gfx_info.VI_WIDTH_REG = &(vi_register.vi_width);
	gfx_info.VI_INTR_REG = &(vi_register.vi_v_intr);
	gfx_info.VI_V_CURRENT_LINE_REG = &(vi_register.vi_current);
	gfx_info.VI_TIMING_REG = &(vi_register.vi_burst);
	gfx_info.VI_V_SYNC_REG = &(vi_register.vi_v_sync);
	gfx_info.VI_H_SYNC_REG = &(vi_register.vi_h_sync);
	gfx_info.VI_LEAP_REG = &(vi_register.vi_leap);
	gfx_info.VI_H_START_REG = &(vi_register.vi_h_start);
	gfx_info.VI_V_START_REG = &(vi_register.vi_v_start);
	gfx_info.VI_V_BURST_REG = &(vi_register.vi_v_burst);
	gfx_info.VI_X_SCALE_REG = &(vi_register.vi_x_scale);
	gfx_info.VI_Y_SCALE_REG = &(vi_register.vi_y_scale);
	gfx_info.SP_STATUS_REG = &(sp_register.sp_status_reg);
	gfx_info.CheckInterrupts = check_interupt;

	/* ★★ [etapa 5] CON LA LISTA EN PARALELO, EL PLUGIN NO VE NADA VIVO DEL
	 * EMULADOR SALVO LA RDRAM.  Lo decide `hilo_lista_configurar`, que
	 * `retro_load_game` llama ANTES de cargar la ROM: el plugin se queda con
	 * estos punteros en `initiateGFX`, justo aqui debajo.
	 *
	 *   DMEM  -> la sombra, que el hilo rellena con la foto de cada lista
	 *   MI/SP -> ventanillas: el emulador aplica el resultado al cosechar
	 *   VI    -> la foto que viaja con cada orden
	 *   CheckInterrupts -> no-op dentro del hilo de la lista
	 *
	 * ★ El precio conocido, el mismo que el hilo antiguo: lo que glN64
	 * ESCRIBE en la DMEM (S2DEX, F5Rogue, ZSortBOSS) se queda en la sombra y
	 * no llega a `SP_DMEM`.  Solo con el modo activo; apagado, todo es la DMEM
	 * viva de Wii64 (la nota de arriba). */
	if (hilo_lista_activo())
	{
		gfx_info.DMEM            = (BYTE*)hilo_lista_dmem();
		gfx_info.MI_INTR_REG     = (DWORD*)hilo_lista_mi();
		gfx_info.SP_STATUS_REG   = (DWORD*)hilo_lista_sp();
		gfx_info.VI_STATUS_REG   = (DWORD*)hilo_lista_vi(HL_VI_STATUS);
		gfx_info.VI_ORIGIN_REG   = (DWORD*)hilo_lista_vi(HL_VI_ORIGIN);
		gfx_info.VI_WIDTH_REG    = (DWORD*)hilo_lista_vi(HL_VI_WIDTH);
		gfx_info.VI_INTR_REG     = (DWORD*)hilo_lista_vi(HL_VI_INTR);
		gfx_info.VI_V_CURRENT_LINE_REG = (DWORD*)hilo_lista_vi(HL_VI_CURRENT);
		gfx_info.VI_TIMING_REG   = (DWORD*)hilo_lista_vi(HL_VI_TIMING);
		gfx_info.VI_V_SYNC_REG   = (DWORD*)hilo_lista_vi(HL_VI_V_SYNC);
		gfx_info.VI_H_SYNC_REG   = (DWORD*)hilo_lista_vi(HL_VI_H_SYNC);
		gfx_info.VI_LEAP_REG     = (DWORD*)hilo_lista_vi(HL_VI_LEAP);
		gfx_info.VI_H_START_REG  = (DWORD*)hilo_lista_vi(HL_VI_H_START);
		gfx_info.VI_V_START_REG  = (DWORD*)hilo_lista_vi(HL_VI_V_START);
		gfx_info.VI_V_BURST_REG  = (DWORD*)hilo_lista_vi(HL_VI_V_BURST);
		gfx_info.VI_X_SCALE_REG  = (DWORD*)hilo_lista_vi(HL_VI_X_SCALE);
		gfx_info.VI_Y_SCALE_REG  = (DWORD*)hilo_lista_vi(HL_VI_Y_SCALE);
		gfx_info.CheckInterrupts = hilo_lista_check_interupt;
	}

	initiateGFX(gfx_info);
}

static void audio_info_init(void)    /* main_gc-menu2.cpp:540 */
{
	audio_info.MemoryBswaped = TRUE;
	audio_info.HEADER = (BYTE*)&ROM_HEADER;
	audio_info.RDRAM = (BYTE*)rdram;
	audio_info.DMEM = (BYTE*)SP_DMEM;
	audio_info.IMEM = (BYTE*)SP_IMEM;
	audio_info.MI_INTR_REG = &(MI_register.mi_intr_reg);
	audio_info.AI_DRAM_ADDR_REG = &(ai_register.ai_dram_addr);
	audio_info.AI_LEN_REG = &(ai_register.ai_len);
	audio_info.AI_CONTROL_REG = &(ai_register.ai_control);
	audio_info.AI_STATUS_REG = &(ai_register.ai_status);
	audio_info.AI_DACRATE_REG = &(ai_register.ai_dacrate);
	audio_info.AI_BITRATE_REG = &(ai_register.ai_bitrate);
	audio_info.CheckInterrupts = check_interupt;
	initiateAudio(audio_info);
}

void control_info_init(void)         /* main_gc-menu2.cpp:557 */
{
	int i;

	control_info.MemoryBswaped = TRUE;
	control_info.HEADER = (BYTE*)&ROM_HEADER;
	control_info.Controls = Controls;
	/* ★★ FASE 4: LOS CUATRO ENCHUFADOS, Y EL ORIGINAL NO LO HACIA ASI.
	 *
	 * `main_gc-menu2.cpp:563` los dejaba a FALSE porque en Wii64 el MENU
	 * decidia despues cuantos mandos habia.  Aqui ese menu no existe: lo
	 * decide el frontend con `retro_set_controller_port_device`, y un
	 * frontend puede no llamarlo nunca.  Con FALSE por defecto, el
	 * resultado de no llamarlo es un emulador que arranca y no responde
	 * -- exactamente la clase de fallo silencioso que el comentario de la
	 * cabecera de este fichero avisa de no repetir.
	 *
	 * Asi que el estado de partida es el de cualquier otro emulador de
	 * N64: cuatro mandos conectados.  `xbox64_input_set_device` los
	 * desenchufa uno a uno si el frontend lo pide.
	 *
	 * ★ PLUGIN_MEMPAK Y NO PLUGIN_NONE: el Controller Pak lo resuelve
	 * `pif.c` el solo contra el array `mempack[]` (pif.c:376-398) y
	 * `format_mempacks()` ya se llama al cargar la ROM, asi que sale
	 * gratis.  Con PLUGIN_NONE, los juegos que piden pak avisan de que no
	 * hay.  Esto quiere acabar siendo una core option. */
	for (i = 0; i < 4; i++)
	{
		Controls[i].Present = TRUE;
		Controls[i].RawData = FALSE;
		Controls[i].Plugin  = PLUGIN_MEMPAK;
	}
	initiateControllers(control_info);
}

static void rsp_info_init(void)      /* main_gc-menu2.cpp:571 */
{
	static int cycle_count;

	rsp_info.MemoryBswaped = TRUE;
	rsp_info.RDRAM = (BYTE*)rdram;
	rsp_info.DMEM = (BYTE*)SP_DMEM;
	rsp_info.IMEM = (BYTE*)SP_IMEM;
	rsp_info.MI_INTR_REG = &MI_register.mi_intr_reg;
	rsp_info.SP_MEM_ADDR_REG = &sp_register.sp_mem_addr_reg;
	rsp_info.SP_DRAM_ADDR_REG = &sp_register.sp_dram_addr_reg;
	rsp_info.SP_RD_LEN_REG = &sp_register.sp_rd_len_reg;
	rsp_info.SP_WR_LEN_REG = &sp_register.sp_wr_len_reg;
	rsp_info.SP_STATUS_REG = &sp_register.sp_status_reg;
	rsp_info.SP_DMA_FULL_REG = &sp_register.sp_dma_full_reg;
	rsp_info.SP_DMA_BUSY_REG = &sp_register.sp_dma_busy_reg;
	rsp_info.SP_PC_REG = &rsp_register.rsp_pc;
	rsp_info.SP_SEMAPHORE_REG = &sp_register.sp_semaphore_reg;
	rsp_info.DPC_START_REG = &dpc_register.dpc_start;
	rsp_info.DPC_END_REG = &dpc_register.dpc_end;
	rsp_info.DPC_CURRENT_REG = &dpc_register.dpc_current;
	rsp_info.DPC_STATUS_REG = &dpc_register.dpc_status;
	rsp_info.DPC_CLOCK_REG = &dpc_register.dpc_clock;
	rsp_info.DPC_BUFBUSY_REG = &dpc_register.dpc_bufbusy;
	rsp_info.DPC_PIPEBUSY_REG = &dpc_register.dpc_pipebusy;
	rsp_info.DPC_TMEM_REG = &dpc_register.dpc_tmem;
	rsp_info.CheckInterrupts = check_interupt;
	rsp_info.ProcessDlistList = processDList;     /* main_gc-menu2.cpp:596 */
	rsp_info.ProcessAlistList = processAList;
	rsp_info.ProcessRdpList = processRDPList;
	rsp_info.ShowCFB = showCFB;
	initiateRSP(rsp_info, (DWORD*)&cycle_count);
}

/* ====================================================================== */
/* 5. CICLO DE VIDA                                                       */
/* ====================================================================== */

/* Las cuatro banderas de "esta partida se ha tocado", que xbox64_unload_rom
 * limpia igual que lo hacia loadROM.  ★ No hay cabecera que las declare: el
 * original las pone a mano justo aqui (main_gc-menu2.cpp:408-411) y se
 * definen repartidas por el arbol, en gc_memory/pif.c:62-63 (eeprom y
 * mempak), dma.c:64 (sram) y flashram.c:49.  Se copia el mismo bloque.
 *
 * El tipo es `bool`, que aqui es `unsigned char` (xbox/compat/stdbool.h) --
 * el mismo que ven los tres .c que las definen, porque a todos les llega
 * por xbox/xbox64_pre.h. */
extern bool eepromWritten;
extern bool mempakWritten;
extern bool sramWritten;
extern bool flashramWritten;

/* -------------------------------------------------------------------- */
/* [Fase 6] El directorio `system` de libretro, para Rice_GX             */
/* -------------------------------------------------------------------- */
/* El porque de que esto exista esta en plataforma.h.  Aqui, lo que no se ve
 * desde la declaracion:
 *
 * ★ SE GUARDA UNA COPIA, no el puntero del frontend.  Salvia devuelve la
 * direccion de un buffer suyo (`dirSystem`, src/salvia.cpp:270-275) que
 * reescribe en CADA consulta desde un std::string.  Guardarse el puntero
 * funcionaria hoy y seria una bomba de relojeria.
 *
 * ★ Y SE NORMALIZA A BARRA INVERTIDA.  El XDK no convierte '/' en '\', asi
 * que una ruta con barras normales no abre ningun fichero y el fallo se ve
 * como "el .ini no esta" en vez de como una ruta mal escrita.  Ya mordio en
 * este repo. */
static char s_system_dir[260] = "";

void xbox64_set_system_dir(const char *dir)
{
	size_t n;
	size_t i;

	s_system_dir[0] = '\0';
	if (!dir || !dir[0])
		return;

	strncpy(s_system_dir, dir, sizeof(s_system_dir) - 2);
	s_system_dir[sizeof(s_system_dir) - 2] = '\0';

	for (i = 0; s_system_dir[i]; i++)
		if (s_system_dir[i] == '/')
			s_system_dir[i] = '\\';

	/* Salvia lo entrega SIN separador final (src/io/cfgloader.cpp:102), y
	 * Rice hace `strcat(dir, "RiceVideoLinux.ini")` a pelo. */
	n = strlen(s_system_dir);
	if (n && s_system_dir[n - 1] != '\\')
	{
		s_system_dir[n]     = '\\';
		s_system_dir[n + 1] = '\0';
	}
}

void xbox64_rice_config_dir(char *out)
{
	if (!out)
		return;
	strcpy(out, s_system_dir);
}

void xbox64_init(void)
{
	/* --- Los valores por defecto, de main_gc-menu2.cpp:292-342 ---------
	 * Uno a uno y en el mismo orden.  Donde el original elegia por
	 * plataforma (#ifdef HW_RVL) se anota cual se ha tomado y por que. */

	audioEnabled     = AUDIO_ENABLE;
	printToSD        = SDLOG_DISABLE;
	/* showFPSonScreen y printToScreen los define y deja a 0 glN64_GX/VI.cpp
	 * (FPS_HIDE / DEBUG_HIDE), que es lo que queremos: aqui no hay OSD de
	 * Wii64 donde escribir y el log va por retro_log. */
	Timers.limitVIs  = LIMITVIS_NONE;     /* ★ el original pone
	                                       * LIMITVIS_WAIT_FOR_VI.  En
	                                       * libretro el ritmo lo lleva el
	                                       * frontend: si el core tambien
	                                       * espera, se frena dos veces.
	                                       * Ver project_salvia_framepacing */
	saveStateDevice  = SAVESTATEDEVICE_SD;
	autoSave         = AUTOSAVE_ENABLE;

	/* ★ EL INTERRUPTOR QUE DE VERDAD DECIDE SI CORRE EL DYNAREC.  Ni
	 * PPC_DYNAREC ni XBOX64_DYNAREC lo hacen: los consulta el compilador y
	 * el enlazador.  Esto lo consulta `go()` en r4300.c:196 en tiempo de
	 * ejecucion.
	 *
	 * ★ Y ES EL A/B DE LA FASE 3.  Poniendo DYNACORE_PURE_INTERP se vuelve
	 * EXACTAMENTE al binario medido de la fase 2 (17-18 fps en Mario 64 y
	 * en Donkey Kong 64) con el dynarec enlazado pero sin ejecutarse.  Si
	 * el dynarec no mueve ese numero, lo primero que hay que mirar es esta
	 * linea, no el dynarec. */
#if XBOX64_DYNAREC
	dynacore         = DYNACORE_DYNAREC;     /* lo que pone el original */
#else
	dynacore         = DYNACORE_PURE_INTERP; /* fase 1 y 2 */
#endif
	/* ★★ `count_per_op` ES EL RELOJ DE LA CPU EMULADA, Y POR ESO SALE DE
	 * UNA OPCION.
	 *
	 * Es cuantas cuentas de `Count` suma cada instruccion del invitado
	 * (`genUpdateCount`, MIPS-to-PPC.c:320-341).  Y el evento del VI se
	 * programa a `Count + vi_delay`, asi que determina CUANTAS
	 * INSTRUCCIONES ejecuta el juego entre dos retrazos:
	 *
	 *     count_per_op = 1  ->  vi_delay    instrucciones por frame
	 *     count_per_op = 2  ->  vi_delay/2  instrucciones por frame
	 *     count_per_op = 3  ->  vi_delay/3
	 *
	 * ★ O sea que NO es un ajuste de precision: es el acelerador.  Un
	 * juego que decide su ritmo por si mismo -- renderiza y espera al
	 * siguiente VI -- baja de 60 a 30 a 20 fps si no le da tiempo, y con
	 * `2` se le esta dando la mitad de CPU que al hardware real.
	 *
	 * ★ EL VALOR DE PARTIDA SIGUE SIENDO EL DEL ORIGINAL (la rama HW_RVL
	 * de main_gc-menu2.cpp:317 pone 2; la de GameCube, 3), asi que quien
	 * no toque la opcion tiene el comportamiento de antes.  Lo que se
	 * anyade es poder bajarlo, que es lo que el hardware real haria: el
	 * R4300 emite hasta una instruccion por ciclo y `Count` avanza a la
	 * mitad del reloj, o sea que lo fiel estaria por DEBAJO de 1.
	 *
	 * ★ SUBIRLO TAMBIEN SIRVE: `gamehacks.c:362` pone 3 en varios juegos
	 * (Wetrix y companyia) porque con menos se rompen.  Esa tabla por CRC
	 * sigue mandando y pisa esto -- no se toca. */
	count_per_op     = (unsigned long)s_opt_count_per_op;

	/* screenMode, nativeOutput, renderCpuFramebuffer,
	 * enablegDPUpdateColorImage y los dos glN64_* ya no se asignan aqui:
	 * los define el lado C++ con estos mismos valores.  Ver el bloque de
	 * los globales, arriba.  Los dos de glN64 se fijan en la carga con
	 * gln64_set_options(). */

	/* Los 11 punteros de fileBrowser.  Antes que nada: la carga de ROM los
	 * usa sin comprobarlos. */
	xbox64_fb_install();

	/* main_gc-menu2.cpp:615.  Va aqui, una sola vez, no por ROM. */
	control_info_init();

	s_frame_hecho = 0;
	s_apagar      = 0;
}

void xbox64_deinit(void)
{
	xbox64_unload_rom();
}

/* loadROM(), de main_gc-menu2.cpp:415, portado entero.
 *
 * ★ DOS ENTRADAS, UN CUERPO.  La ROM puede llegar como ruta o ya en RAM
 * (`need_fullpath = false`), y lo unico que cambia entre las dos es la
 * llamada que rellena `s_rom_file`.  El resto -- que es todo -- es identico,
 * asi que vive una sola vez aqui; duplicar la funcion seria garantizar que
 * dentro de tres cambios las dos copias divergen. */
static int carga_tras_fijar_rom(void)
{
	int ret;

	xbox64_fb_set_save_dir(&s_save_dir);
	saveFile_dir = &s_save_dir;

	format_mempacks();
	reset_flashram();
	init_eeprom();
	hasLoadedROM = TRUE;

	/* El original hace aqui TLBCache_reset() o tlb_mem2_init() segun
	 * USE_TLB_CACHE / HW_RVL.  Ninguno de los dos aplica: la cache de TLB
	 * en ARAM es de GameCube (gc_memory/TLB-Cache.c no entra ni en el
	 * build de Wii) y tlb_mem2_init reparte la MEM2 de la Wii. */

	ret = rom_read(&s_rom_file);
	if (ret)
	{
		hasLoadedROM = FALSE;
		return ret;
	}

	init_memory();

	/* gfx_set_fb / gfx_set_window: fase 2. */

#if !XBOX64_GFX_NULO
	/* ★ ANTES de InitiateGFX: `Config_LoadConfig` copia esto a
	 * `OGL.frameBufferTextures` desde dentro de InitiateGFX, asi que
	 * fijarlo despues no tendria efecto hasta la siguiente carga. */
	/* ★ FASE 4: YA SALEN DE UNA CORE OPTION.  Antes estaban cableados a
	 * (0, 0) -- lo que ponia main_gc-menu2.cpp:336-337 -- con una nota que
	 * decia "fase 5".  Van por aqui y no por asignacion directa porque las
	 * variables viven del lado C++, en xenon/gln64_glue.cpp. */
	/* [Jet Force Gemini] `s_db_framebuffer` lo acaba de poner rom_read por
	 * CRC; ver su nota. */
	gln64_set_options(s_opt_framebuffer || s_db_framebuffer, s_opt_sai2x);
#endif

	gfx_info_init();
	audio_info_init();
	rsp_info_init();

	romOpen_gfx();
	romOpen_audio();
	romOpen_input();

#if !XBOX64_GFX_NULO
	/* ★★ EL TAMANYO DE LA RDRAM, Y VA AQUI POR UNA RAZON DE ORDEN.
	 *
	 * glN64 cablea 4 MB en tiempo de compilacion y con el Expansion Pak
	 * son 8 -- y este build define USE_EXPANSION, igual que el de Wii.
	 *
	 * ★ No es solo una cota: `RSP_SegmentToPhysical` la usa de MASCARA, o
	 * sea que quedarse corto PLIEGA las direcciones altas sobre las bajas
	 * y el parser de display lists se desboca.
	 *
	 * ★ Y DESPUES de romOpen_gfx(), no antes: `RSP_Init()` corre ahi
	 * dentro y lo pisaria. */
	gln64_apply_rdram_size(sizeof(rdram));
#endif

	cpu_init();

#if XBOX64_DYNAREC
	/* [2026-10-01] La autoprueba del recompilador contra el interprete
	 * (ppc/Wrappers.c): aqui, con la CPU recien iniciada y ANTES del primer
	 * `go()`, que de todas formas reinicia la cache y los bloques.  Una vez
	 * por proceso, y solo si el nucleo elegido es el recompilador. */
	if (dynacore == DYNACORE_DYNAREC)
		xbox64_autoprueba_dynarec();
#endif

	/* ★★ FASE 4: EL CORE YA NO CARGA NI GUARDA LOS FICHEROS DE PARTIDA.
	 *
	 * Aqui estaban `saveFile_init` + los cuatro `loadXxx`, y en
	 * `xbox64_unload_rom` los cuatro `saveXxx`.  Ahora los cuatro bufers
	 * viven pegados en `xbox64_mem_guardado` y se entregan al frontend
	 * con `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)`.
	 *
	 * ★ SE QUITAN, NO SE DEJAN "POR SI ACASO": con las dos rutas vivas
	 * habria DOS verdades sobre la misma partida.  Al arrancar ganaria la
	 * que cargue la ultima y al salir la que escriba la ultima, y eso no
	 * da un error -- da partidas que retroceden.
	 *
	 * ★ LO QUE SI SE QUEDA es la inicializacion a cero, unas lineas mas
	 * arriba (`format_mempacks`, `reset_flashram`, `init_eeprom`), y el
	 * ORDEN importa: corren DENTRO de `retro_load_game`, y el frontend
	 * rellena la SRAM DESPUES de que esa funcion vuelva.  Si se hicieran
	 * al reves, el core borraria la partida recien cargada. */

	s_frame_hecho = 0;
	s_apagar      = 0;
	return 0;
}

int xbox64_load_rom(const char *ruta)
{
	if (hasLoadedROM)
		xbox64_unload_rom();

	if (!xbox64_fb_set_rom(&s_rom_file, ruta))
	{
#if XBOX64_TRAZAS
		DEBUG_print("xbox64: no se pudo abrir la ROM", DBG_USBGECKO);
#endif
		return ROM_CACHE_ERROR_READ;
	}
	return carga_tras_fijar_rom();
}

int xbox64_load_rom_mem(const void *datos, unsigned int tam,
                        const char *nombre)
{
	if (hasLoadedROM)
		xbox64_unload_rom();

	if (!xbox64_fb_set_rom_mem(&s_rom_file, datos, tam, nombre))
	{
#if XBOX64_TRAZAS
		DEBUG_print("xbox64: el frontend no dio la ROM en memoria",
		            DBG_USBGECKO);
#endif
		return ROM_CACHE_ERROR_READ;
	}
	return carga_tras_fijar_rom();
}

void xbox64_unload_rom(void)
{
	if (!hasLoadedROM)
		return;

	/* ★ AQUI ESTABAN LOS CUATRO `saveXxx`.  Ver la nota de
	 * `xbox64_load_rom`: la partida la persiste el frontend.
	 *
	 * ★ Y ESTO ARREGLA LA DEBILIDAD QUE TENIAN: solo se escribia al
	 * descargar la ROM, asi que un apagon o una caida se llevaba la
	 * partida por delante.  El frontend la escribe periodicamente. */

	cpu_deinit();
	eepromWritten   = FALSE;
	mempakWritten   = FALSE;
	sramWritten     = FALSE;
	flashramWritten = FALSE;
	romClosed_RSP();
	romClosed_input();
	romClosed_audio();
	romClosed_gfx();
#if !XBOX64_GFX_NULO
	/* ★ Vacia la cache de pixel shaders del shim.  Era la UNICA pieza de
	 * la tuberia grafica que sobrevivia a una carga de juego: `romClosed`
	 * acaba en `OGL_Stop()`, que destruye combinador, framebuffer y
	 * texturas, y la cache del shim no se enteraba.  El sintoma es que el
	 * contador `shgen` va 20 -> 40 -> 60 en tres cargas sin reiniciarse. */
	gxd3d_tev_flush_cache();
#endif
	closeDLL_RSP();
	closeDLL_input();
	closeDLL_audio();
	closeDLL_gfx();
	ROMCache_deinit();
	free_memory();

	/* [Xbox360] ★★ Y EL FICHERO DE LA ROM, QUE NO LO CERRABA NADIE.
	 *
	 * `ROMCache_deinit()` solo libera `ROMBase`.  El descriptor lo abre
	 * `xbox64_fb_set_rom` y `romFile_deinit` NO se llamaba en ningun camino
	 * de exito, asi que se quedaba abierto TODA la sesion y sobrevivia a la
	 * descarga.
	 *
	 * Con libretro eso rompe la carga SIGUIENTE: la ROM llega en un temporal
	 * del frontend (`game:\tmp\extractedRom.z64`) y, con el fichero abierto,
	 * el XDK no deja sobreescribirlo al extraer el .zip del juego nuevo.
	 *
	 * ★ El cierre de verdad ocurre antes, al terminar `ROMCache_load` (ver la
	 * nota alli): asi el temporal queda libre YA DURANTE la partida, no solo
	 * al salir.  Esta llamada es la RED: cubre los caminos en los que
	 * `xbox64_fb_set_rom` abrio y la carga fallo despues, y hace cierto el
	 * contrato de que descargar libera todo.  Es idempotente. */
	if (romFile_deinit)
		romFile_deinit(&s_rom_file);

	hasLoadedROM = FALSE;
}

void xbox64_run_frame(void)
{
	int vueltas;

	if (!hasLoadedROM)
		return;

	s_frame_hecho = 0;

	/* ★ EL TOPE NO ES UNA "SEGURIDAD" DECORATIVA.  Si el invitado deja de
	 * generar interrupciones de VI -- porque se colgo, o porque todavia no
	 * las ha programado en el arranque -- `go()` no volveria nunca y el
	 * frontend se quedaria sin responder, sin log y sin forma de salir.
	 * Con el tope, retro_run devuelve, el log lo dice, y el fallo es
	 * legible.  Cada vuelta es un `go()` que solo puede acabar en
	 * `r4300.stop`, o sea en fin de frame o en apagado. */
	for (vueltas = 0; vueltas < 16 && !s_frame_hecho && !s_apagar; vueltas++)
	{
		go();
#if XBOX64_TRAZAS
		s_vueltas++;
#endif
	}

#if XBOX64_TRAZAS
	if (!s_frame_hecho && !s_apagar)
		DEBUG_print("xbox64: 16 vueltas sin VI_INT", DBG_USBGECKO);
#endif
}

unsigned xbox64_vueltas_y_limpia(void)
{
	unsigned v = s_vueltas;
	s_vueltas = 0;
	return v;
}

/* Rango de PC visto en el limite de frame.  Ver xbox64_pc_rango. */
static unsigned s_pc_min = 0xFFFFFFFF;
static unsigned s_pc_max = 0;

void xbox64_frame_boundary(void)
{
	/* ★ EL PC SE MUESTREA AQUI, Y EL SITIO ES LO QUE LO HACE UTIL.
	 *
	 * La primera version lo leia desde el wrapper, despues de
	 * xbox64_run_frame, y salia SIEMPRE 80000180 -- con el juego
	 * corriendo o parado.  El motivo: en `gen_interupt` la interrupcion de
	 * VI se ENTREGA en `chk_status(1)` (interupt.c:399), que llama a
	 * `exception_general()` y pone el pc en el vector de excepcion.  Eso
	 * pasa DESPUES de este gancho.
	 *
	 * O sea que justo aqui `r4300.pc` es todavia la del codigo del
	 * invitado que se acaba de interrumpir.  Con el juego ejecutando
	 * codigo variado, el rango se mueve; en un bucle cerrado, min y max
	 * se quedan clavados y ademas juntos. */
#if XBOX64_TRAZAS
	unsigned pc = (unsigned)r4300.pc;
	if (pc < s_pc_min) s_pc_min = pc;
	if (pc > s_pc_max) s_pc_max = pc;
#endif

	s_frame_hecho = 1;
	r4300.stop    = 1;
}

void xbox64_pc_rango(unsigned *min, unsigned *max)
{
	*min = s_pc_min;
	*max = s_pc_max;
	s_pc_min = 0xFFFFFFFF;
	s_pc_max = 0;
}

#if XBOX64_TRAZAS
/* ★ HERRAMIENTA FIJA (con trazas): LOS HILOS DE libultra.  Para cuando un
 * juego deja de mandar listas con el audio y el VI sanos (nacio con Resident
 * Evil 2: dlist=0, dp=0, pi=0) y hay que saber que espera.  Busca en la RDRAM
 * las OSThread encadenadas por `tlnext` y vuelca cada una: estado (1 parado,
 * 2 listo, 4 corriendo, 8 esperando), flags (2 = FAULT), prioridad, pc y ra
 * de su contexto, y la cola en la que espera con sus mensajes (validCount /
 * msgCount).  Disposicion (reference_n64_bucle_ocioso_no_es_cuelgue): next
 * +0x00, priority +0x04, queue +0x08, tlnext +0x0C, state +0x10, flags +0x12,
 * id +0x14, contexto en +0x20 (31 u64: sp +0xF4, ra +0x104; sr/pc/cause/
 * badvaddr en +0x118/+0x11C/+0x120/+0x124).  Sin direcciones fijas: se
 * buscan candidatas con todo plausible y se queda la cadena mas larga. */
#define HL_MEMW   (0x800000u / 4u)
#define HL_MAXC   32u

static unsigned hl_w(unsigned dir)          /* palabra N64 en una direccion KSEG0 */
{
	const unsigned f = dir & 0x7FFFFFu;
	return (f / 4u < HL_MEMW) ? (unsigned)rdram[f / 4u] : 0u;
}

static unsigned hl_h(unsigned dir)          /* media palabra */
{
	const unsigned w = hl_w(dir & ~3u);
	return (dir & 2u) ? (w & 0xFFFFu) : (w >> 16);
}

static int hl_puntero(unsigned p)           /* 0 o KSEG0 dentro de 8 MB, alineado a 8 */
{
	return p == 0u || ((p & 0xFF800007u) == 0x80000000u);
}

static int hl_es_hilo(unsigned t)
{
	const unsigned st = hl_h(t + 0x10u), fl = hl_h(t + 0x12u);
	const unsigned pri = hl_w(t + 0x04u), pc = hl_w(t + 0x11Cu);
	if ((t & 0xFF800007u) != 0x80000000u || (t & 0x7FFFFFu) + 0x128u > 0x800000u)
		return 0;
	if (st != 1u && st != 2u && st != 4u && st != 8u) return 0;
	if (fl > 3u || pri > 255u || hl_w(t + 0x14u) > 0xFFFFu) return 0;
	if (!hl_puntero(hl_w(t)) || !hl_puntero(hl_w(t + 0x0Cu))) return 0;
	if (hl_w(t + 0x08u) != 0u && (hl_w(t + 0x08u) & 0xFF800003u) != 0x80000000u) return 0;
	if ((pc & 0xFF800003u) != 0x80000000u) return 0;
	return 1;
}

/* [log 10] ★ La lista de libultra NO acaba en 0: acaba en `__osThreadTail`,
 * un hilo falso {next = NULL, priority = -1}.  Pidiendo un 0 al final la
 * cadena salia de 1 con 14 candidatas. */
static int hl_es_cola_final(unsigned t)
{
	return t == 0u || ((t & 0xFF800003u) == 0x80000000u && hl_w(t + 0x04u) == 0xFFFFFFFFu);
}

static unsigned hl_cadena(unsigned t)       /* longitud de la cadena por tlnext */
{
	unsigned n = 0;
	while (t && n < HL_MAXC && hl_es_hilo(t)) { n++; t = hl_w(t + 0x0Cu); }
	return hl_es_cola_final(t) ? n : 0u;
}

static void hl_vuelca_uno(unsigned t, char *l, unsigned tam);

void xbox64_hilos_volcado(void)
{
	unsigned o, mejor = 0, mejorN = 0, t, n, cand = 0;
	char l[160];

	for (o = 0; o + 0x128u <= 0x800000u; o += 8u)
	{
		const unsigned d = 0x80000000u | o;
		unsigned c;
		if (!hl_es_hilo(d))
			continue;
		cand++;
		c = hl_cadena(d);
		if (c > mejorN) { mejorN = c; mejor = d; }
	}
	_snprintf(l, sizeof(l), "[HILOS] candidatas=%u cadena=%u desde %08X | pc=%08X",
	          cand, mejorN, mejor, (unsigned)r4300.pc);
	l[sizeof(l) - 1] = 0;
	xbox64_log_line(l);

	for (t = mejor, n = 0; t && n < mejorN; n++, t = hl_w(t + 0x0Cu))
		hl_vuelca_uno(t, l, sizeof(l));

	/* Si la cadena no salio, todas las candidatas (como mucho 24): mejor
	 * ver de mas que quedarse otra vez sin datos. */
	if (mejorN < 3u)
	{
		n = 0;
		for (o = 0; o + 0x128u <= 0x800000u && n < 24u; o += 8u)
		{
			const unsigned d = 0x80000000u | o;
			if (d == mejor || !hl_es_hilo(d))
				continue;
			hl_vuelca_uno(d, l, sizeof(l));
			n++;
		}
	}

	/* [log 11] QUIEN DEBERIA ESCRIBIR EN CADA COLA.  Para cada hilo que espera
	 * en la cadena: la cola por dentro (mtqueue, fullqueue, validCount, first,
	 * msgCount, msg) y las palabras de la RDRAM que la apuntan, con su vecina
	 * de detras.  Una de ellas sera la tabla de eventos de libultra
	 * (__osEventStateTab: 15 pares {cola, mensaje}, el VI en el indice 7 y el
	 * PI en el 8), el planificador o el OSIoMesg de un DMA. */
	for (t = mejor, n = 0; t && n < mejorN; n++, t = hl_w(t + 0x0Cu))
	{
		const unsigned q = hl_w(t + 0x08u);
		unsigned r = 0;
		if (hl_h(t + 0x10u) != 8u || !q)
			continue;
		_snprintf(l, sizeof(l), "[HILOS] cola %08X (hilo %08X): %08X %08X %u %u %u %08X",
		          q, t, hl_w(q), hl_w(q + 4u), hl_w(q + 8u), hl_w(q + 12u),
		          hl_w(q + 16u), hl_w(q + 20u));
		l[sizeof(l) - 1] = 0;
		xbox64_log_line(l);
		/* Una linea por referencia (el embudo del log corta a 127): la
		 * direccion y las palabras de -8 a +8 a su alrededor. */
		for (o = 8u; o < 0x800000u - 8u && r < 8u; o += 4u)
		{
			const unsigned d = 0x80000000u | o;
			if (hl_w(d) != q || d == t + 0x08u)
				continue;
			_snprintf(l, sizeof(l), "[HILOS]   ref %08X: %08X %08X [%08X] %08X %08X %08X %08X",
			          d, hl_w(d - 8u), hl_w(d - 4u), hl_w(d), hl_w(d + 4u), hl_w(d + 8u),
			          hl_w(d + 12u), hl_w(d + 16u));
			l[sizeof(l) - 1] = 0;
			xbox64_log_line(l);
			r++;
		}
	}

	/* La tabla de eventos: el gestor del VI (prioridad 254, el que libultra
	 * crea con osCreateViManager) registra SU cola en el indice 7.  Se busca
	 * esa cola en la RDRAM y se vuelcan los 15 pares desde el indice 0
	 * (colas; los mensajes no hacen falta para saber quien es quien). */
	for (t = mejor, n = 0; t && n < mejorN; n++, t = hl_w(t + 0x0Cu))
	{
		const unsigned q = hl_w(t + 0x08u);
		unsigned vistos = 0;   /* C89: declarada al principio del bloque */
		if (hl_w(t + 0x04u) != 254u || !q)
			continue;
		for (o = 0x38u; o < 0x800000u - 0x40u && vistos < 3u; o += 4u)
		{
			const unsigned d = 0x80000000u | o, base = d - 0x38u;
			unsigned k, p;
			if (hl_w(d) != q || d == t + 0x08u)
				continue;
			vistos++;
			p = (unsigned)_snprintf(l, sizeof(l), "[HILOS] eventos? %08X 0-7:", base);
			for (k = 0; k < 8u && p < sizeof(l); k++)
				p += (unsigned)_snprintf(l + p, sizeof(l) - p, " %08X", hl_w(base + 8u * k));
			l[sizeof(l) - 1] = 0;
			xbox64_log_line(l);
			p = (unsigned)_snprintf(l, sizeof(l), "[HILOS] eventos? %08X 8-14:", base);
			for (k = 8; k < 15u && p < sizeof(l); k++)
				p += (unsigned)_snprintf(l + p, sizeof(l) - p, " %08X", hl_w(base + 8u * k));
			l[sizeof(l) - 1] = 0;
			xbox64_log_line(l);
		}
		break;
	}

	/* [log 12] Los ultimos DMA del PI, para cruzarlos con los OSIoMesg que
	 * salen arriba en `ref` (hdr 000B0000, cola, dram, dev, size). */
	xbox64_pidma_volcado();
	xbox64_sptask_volcado();

	/* Y el estado de las interrupciones en ese momento (MI intr/mask, SP, DP). */
	xbox64_traza_cpu();
}

static void hl_vuelca_uno(unsigned t, char *l, unsigned tam)
{
	const unsigned q = hl_w(t + 0x08u);
	_snprintf(l, tam,
	          "[HILOS] %08X id=%u pri=%u st=%u fl=%u pc=%08X ra=%08X sp=%08X cola=%08X v=%u/%u",
	          t, hl_w(t + 0x14u), hl_w(t + 0x04u), hl_h(t + 0x10u), hl_h(t + 0x12u),
	          hl_w(t + 0x11Cu), hl_w(t + 0x104u), hl_w(t + 0xF4u), q,
	          q ? hl_w(q + 0x08u) : 0u, q ? hl_w(q + 0x10u) : 0u);
	l[tam - 1] = 0;
	xbox64_log_line(l);
	/* Los que esperan: su pila, donde esta la direccion de vuelta del
	 * codigo del JUEGO que llamo a osRecvMesg (el ra de arriba es de
	 * dentro de libultra).  Ojo: `cola` apunta al campo de la cola en el
	 * que espera, +0 si es al recibir (v = validCount/msgCount) y +4 si
	 * es al enviar (entonces v sale desplazado). */
	if (hl_h(t + 0x10u) == 8u)
	{
		const unsigned sp = hl_w(t + 0xF4u);
		unsigned k, p;
		unsigned fila;
		/* [log 11] Tres filas de 10: la vuelta al codigo del juego que llamo
		 * puede estar mas abajo que el marco de osRecvMesg. */
		for (fila = 0; fila < 3u; fila++)
		{
			p = (unsigned)_snprintf(l, tam, "[HILOS]   pila %08X:", sp + 0x10u + 40u * fila);
			for (k = 0; k < 10u && p < tam; k++)
				p += (unsigned)_snprintf(l + p, tam - p, " %08X",
				                         hl_w(sp + 0x10u + 40u * fila + 4u * k));
			l[tam - 1] = 0;
			xbox64_log_line(l);
		}
	}
}
#endif

int xbox64_quiere_apagar(void)
{
	return s_apagar;
}

/* [log 39] Ver la declaracion en plataforma.h.  BattleTanx se quedo sin su
 * primer VI y el vigia solo podia decir "el emulador no termina el VI". */
void xbox64_traza_cpu(void)
{
	/* ★ Solo campos sueltos: NADA de `get_event`, que recorre la lista de
	 * eventos mientras el emulador la modifica en su hilo. */
#if XBOX64_TRAZAS
	char l[256];
	_snprintf(l, sizeof(l),
	          "[CPU] pc=%08X Count=%08X proxima_int=%08X Status=%08X Cause=%08X"
	          " | MI intr=%02X mask=%02X | SP=%08X DP=%08X | vi_delay=%u"
	          " | frame_hecho=%d vueltas=%u",
	          (unsigned)r4300.pc, (unsigned)r4300.reg_cop0[9],
	          (unsigned)r4300.next_interrupt,
	          (unsigned)r4300.reg_cop0[12], (unsigned)r4300.reg_cop0[13],
	          (unsigned)MI_register.mi_intr_reg, (unsigned)MI_register.mi_intr_mask_reg,
	          (unsigned)sp_register.sp_status_reg, (unsigned)dpc_register.dpc_status,
	          (unsigned)vi_register.vi_delay, s_frame_hecho, s_vueltas);
	l[sizeof(l) - 1] = 0;
	xbox64_log_line(l);
#endif
}

/* stop_it(), de main_gc-menu2.cpp:603.  Lo llama quien quiera apagar de
 * verdad: a diferencia de xbox64_frame_boundary, esto NO se reanuda. */
void stop_it(void)
{
	s_apagar   = 1;
	r4300.stop = 1;
}
