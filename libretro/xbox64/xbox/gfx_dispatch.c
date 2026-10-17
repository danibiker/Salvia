/*
 * xbox/gfx_dispatch.c  --  elige plugin grafico en ejecucion: glN64 o Rice.
 *
 * ★★ EL PORQUE, Y EN QUE SE DIFERENCIA DE WII64.
 *
 * Wii64 no elige: compila `Makefile.glN64_wii` o `Makefile.Rice_wii`, dos
 * binarios, y cada plugin se apropia de los nombres que llama el nucleo
 * (`ProcessDList` -> `processDList`, via su `GFXPlugin.h`).  Los dos plugins
 * no llegan a verse nunca.
 *
 * Aqui los queremos LOS DOS en el mismo core, con una opcion en el menu.  Por
 * eso los puntos de entrada de cada uno van prefijados con una cabecera
 * forzada -- `xenon/gln64_rename.h` y `xenon/rice_rename.h`, aplicadas desde
 * el vcxproj-- y este fichero se queda con los nombres sin prefijo, que son
 * los que el nucleo enlaza.  Ni una linea de los dos arboles copiados cambia.
 *
 * ★ LA ELECCION NO ES EN CALIENTE, Y NO ES UNA LIMITACION QUE SE PUEDA QUITAR
 * CON MAS CODIGO.  Cambiar de plugin significa tirar la cache de texturas, el
 * combinador, el estado del shim y el framebuffer, y reconstruirlos desde una
 * display list que ya ha pasado.  `xbox64_gfx_plugin_set` solo tiene efecto
 * antes de `initiateGFX`, o sea al cargar la ROM; despues se ignora a
 * proposito.  Aceptarlo a mitad de partida daria una pantalla rota y un
 * informe de fallo que nadie sabria atribuir.
 *
 * ★ Y EL DESPACHO ES POR `if`, NO POR TABLA DE PUNTEROS.  Son diez funciones
 * llamadas como mucho una vez por display list (`processDList`) o por frame
 * (`updateScreen`); una prediccion de salto perfecta cuesta menos que una
 * carga indirecta, y se lee mejor.  `processRDPList` es la mas frecuente y
 * sigue siendo del orden de miles por segundo, no de cientos de miles.
 */

#include "plataforma.h"
#include "hilo_lista.h"          /* [etapa 5] la lista en paralelo, en HW3 */
#include "../source/main/winlnxdefs.h"
#include "../source/main/plugin.h"

/* Los dos juegos de puntos de entrada, ya prefijados por las cabeceras
 * forzadas.  Se declaran aqui porque ningun .h del arbol copiado los conoce
 * con el prefijo -- el prefijo lo pone el preprocesador al compilar ESOS
 * ficheros, no al compilar este. */
extern BOOL GlN64_initiateGFX(GFX_INFO Gfx_Info);
extern void GlN64_processDList(void);
extern void GlN64_processRDPList(void);
extern void GlN64_updateScreen(void);
extern void GlN64_romOpen_gfx(void);
extern void GlN64_romClosed_gfx(void);
extern void GlN64_closeDLL_gfx(void);
extern void GlN64_showCFB(void);
extern void GlN64_viStatusChanged(void);
extern void GlN64_viWidthChanged(void);

extern BOOL Rice_initiateGFX(GFX_INFO Gfx_Info);
extern void Rice_processDList(void);
extern void Rice_processRDPList(void);
extern void Rice_updateScreen(void);
extern void Rice_romOpen_gfx(void);
extern void Rice_romClosed_gfx(void);
extern void Rice_closeDLL_gfx(void);
extern void Rice_showCFB(void);
extern void Rice_viStatusChanged(void);
extern void Rice_viWidthChanged(void);

/* ★ El que se PIDE y el que esta PUESTO, separados a proposito.
 *
 * `s_pedido` lo escribe la opcion de core en cualquier momento; `s_activo` se
 * congela en `initiateGFX` y es el unico que consultan las diez funciones de
 * abajo.  Sin esa separacion, cambiar la opcion a mitad de partida mandaria
 * el siguiente `processDList` a un plugin que nunca vio un `initiateGFX`. */
static int s_pedido = XBOX64_GFX_GLN64;
static int s_activo = XBOX64_GFX_GLN64;

void xbox64_gfx_plugin_set(int cual)
{
	s_pedido = (cual == XBOX64_GFX_RICE) ? XBOX64_GFX_RICE : XBOX64_GFX_GLN64;
}

int xbox64_gfx_plugin_activo(void)
{
	return s_activo;
}

/* [Xbox360] Aqui estaba `xbox64_gfx_ucode_apto_para_hilo`, la pregunta de
 * si la display list del plugin activo podia irse al hilo grafico antiguo.
 * Se quito con ese hilo (2026-09-30).  [etapa 5] La lista vuelve a ir en
 * paralelo (xbox/hilo_lista.cpp) pero SIN filtro: el caso que lo justificaba
 * -- el `infloop` que borra TASKDONE para que el nucleo reencole -- se CUENTA
 * (`infloop` en `[LISTA2]`) en vez de adivinarse por ucode.  Si sale en
 * algun juego, se decide con ese dato. */

/* -- Los nombres que enlaza el nucleo ------------------------------------ */

BOOL initiateGFX(GFX_INFO Gfx_Info)
{
	/* El unico sitio donde la eleccion se hace efectiva. */
	s_activo = s_pedido;

	/* ★ El monton de texturas de Rice, AQUI Y NO EN OTRO SITIO.
	 *
	 * Tiene que ir despues de congelar `s_activo` -- para no tocarlo cuando
	 * el plugin elegido es glN64 -- y antes de que se reserve la primera
	 * textura.  Este es el unico punto que cumple las dos.  glN64 hace lo
	 * equivalente desde `Config_LoadConfig`, dentro de su `initiateGFX`. */
	if (s_activo == XBOX64_GFX_RICE)
		xbox64_rice_tex_heap_setup();

	return (s_activo == XBOX64_GFX_RICE) ? Rice_initiateGFX(Gfx_Info)
	                                     : GlN64_initiateGFX(Gfx_Info);
}

/* ★★ [etapa 5] EL CORTE DE LA LISTA EN PARALELO ESTA AQUI, Y SOLO AQUI.
 *
 * Todas las llamadas del nucleo al plugin durante la partida pasan por estas
 * seis funciones (censado: memory.c, interupt.c y rsp_hle; `main/plugin.c`
 * es codigo muerto).  Con `hilo_lista_activo()`, cada una se ENCOLA al hilo
 * de la lista (xbox/hilo_lista.cpp), que es quien llama de verdad al plugin
 * con `xbox64_gfx_real`.  Asi el plugin corre SIEMPRE en ese hilo -- nunca
 * en dos a la vez -- y ese hilo es el unico que graba ordenes D3D.
 *
 * `hilo_lista_encolar` devuelve 0 con el modo apagado o si la llamada sale
 * del propio hilo de la lista: entonces se ejecuta en linea, como siempre.
 * Las de carga y descarga (initiateGFX, romOpen, romClosed, closeDLL) no se
 * encolan: corren en el hilo del frontend con el emulador parado. */
void xbox64_gfx_real(int que)
{
	const int rice = (s_activo == XBOX64_GFX_RICE);
	switch (que)
	{
	case HL_DLIST:    if (rice) Rice_processDList();     else GlN64_processDList();     break;
	case HL_UPDSCR:   if (rice) Rice_updateScreen();     else GlN64_updateScreen();     break;
	case HL_VISTATUS: if (rice) Rice_viStatusChanged();  else GlN64_viStatusChanged();  break;
	case HL_VIWIDTH:  if (rice) Rice_viWidthChanged();   else GlN64_viWidthChanged();   break;
	case HL_SHOWCFB:  if (rice) Rice_showCFB();          else GlN64_showCFB();          break;
	case HL_RDPLIST:  if (rice) Rice_processRDPList();   else GlN64_processRDPList();   break;
	default: break;
	}
}

/* [Cruis'n USA] La base de la RDRAM que ve el plugin activo.  La llama SOLO el
 * hilo de la lista, alrededor de una lista con "foto" (xbox/hilo_lista.cpp),
 * y siempre la devuelve a `rdram` al acabar. */
extern void xbox64_gln64_rdram_base(unsigned char *base);
extern void xbox64_rice_rdram_base(unsigned char *base);

void xbox64_gfx_rdram_base(unsigned char *base)
{
	if (s_activo == XBOX64_GFX_RICE) xbox64_rice_rdram_base(base);
	else                             xbox64_gln64_rdram_base(base);
}

void processDList(void)
{
	if (hilo_lista_encolar(HL_DLIST)) return;
	xbox64_gfx_real(HL_DLIST);
}

void processRDPList(void)
{
	if (hilo_lista_encolar(HL_RDPLIST)) return;
	xbox64_gfx_real(HL_RDPLIST);
}

void updateScreen(void)
{
	if (hilo_lista_encolar(HL_UPDSCR)) return;
	xbox64_gfx_real(HL_UPDSCR);
}

void romOpen_gfx(void)
{
	if (s_activo == XBOX64_GFX_RICE) Rice_romOpen_gfx();
	else                             GlN64_romOpen_gfx();
}

/* ★ Los tres de cierre usan `s_activo`, NO `s_pedido`: hay que cerrar el que
 * se abrio.  Si el usuario cambio la opcion durante la partida, `s_pedido` ya
 * apunta al otro y cerrar por ahi dejaria el plugin vivo con sus texturas
 * colgando -- y destruiria las de uno que nunca se inicializo. */
void romClosed_gfx(void)
{
	if (s_activo == XBOX64_GFX_RICE) Rice_romClosed_gfx();
	else                             GlN64_romClosed_gfx();
}

void closeDLL_gfx(void)
{
	if (s_activo == XBOX64_GFX_RICE) Rice_closeDLL_gfx();
	else                             GlN64_closeDLL_gfx();
}

void showCFB(void)
{
	if (hilo_lista_encolar(HL_SHOWCFB)) return;
	xbox64_gfx_real(HL_SHOWCFB);
}

void viStatusChanged(void)
{
	if (hilo_lista_encolar(HL_VISTATUS)) return;
	xbox64_gfx_real(HL_VISTATUS);
}

void viWidthChanged(void)
{
	if (hilo_lista_encolar(HL_VIWIDTH)) return;
	xbox64_gfx_real(HL_VIWIDTH);
}
