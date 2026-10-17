/*
 * gln64_config.cpp  --  la configuracion por defecto de glN64_GX.
 *
 * ★★ POR QUE EXISTE ESTE FICHERO, Y POR QUE ES SU PROPIA UNIDAD
 * -------------------------------------------------------------
 * `Config_LoadConfig()` es de `Config_linux.cpp`, que esta EXCLUIDO del build
 * por ser GTK.  El primer apano fue un stub vacio en `gln64_glue.cpp` con el
 * comentario "los valores por defecto los fija gln64_set_options()", y era
 * FALSO: `gln64_set_options()` solo toca dos banderas de core option.  Todo
 * lo demas se quedaba en el cero del `.bss`, y el que mas dolia era este:
 *
 *     cache.maxBytes = 0
 *       -> while (cache.cachedBytes > cache.maxBytes) TextureCache_RemoveBottom()
 *       -> CADA textura se desaloja nada mas entrar
 *
 * Medido: `texconv=193772 texfree=193770` con 521.361 draws -- una conversion
 * completa (crear textura D3D, bloquear, destilar, soltar) cada 2,7 draws,
 * cero reutilizacion.  De ahi el "se oye mas lento".
 *
 * No va en `gln64_glue.cpp` porque para escribir en `OGL` y `cache` hay que
 * incluir `OpenGL.h` y `Textures.h`, y esos arrastran `gl.h`/`glext.h`, que
 * chocan con el `<xtl.h>` del glue (APIENTRY, WINGDIAPI y companyia).  Este
 * fichero se compila EXACTAMENTE como una unidad del plugin -- con
 * `__GX__;__LINUX__` y sin `<xtl.h>`-- y no hace nada mas.
 *
 * Todo lo de aqui es NUESTRO; el arbol copiado no se toca.
 */

/* ★ `<gccore.h>` PRIMERO, igual que en toda unidad del plugin
 * (Textures.cpp:13, glN64.cpp:23).  De ahi salen `u8`/`u16`/`u32`/`f32`: el
 * `Types.h` del arbol copiado los define pero el fichero ENTERO vive bajo
 * `#ifndef __GX__` (Types.h:12), asi que en esta rama no aporta nada.
 * Olvidarlo da decenas de C2146/C4430 en GBI.h, que es justo la firma que
 * tuvo el primer build de todo este port. */
#include <gccore.h>
#include <ogc/lwp_heap.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "../source/glN64_GX/OpenGL.h"
#include "../source/glN64_GX/Textures.h"
#include "../source/glN64_GX/Config.h"
#include "../source/glN64_GX/N64.h"    /* RDRAMSize */
#include "../source/glN64_GX/RSP.h"    /* RSP: estado que vuelca el vigilante */
#include "../source/glN64_GX/VI.h"     /* VI.width/height: contexto de [CIMG-GX] */
#include "../source/glN64_GX/gDP.h"    /* gDP.combine: el volcado de [TEVHLSL] */
#include "../source/glN64_GX/gSP.h"    /* gSP.changed */
#include "../source/xenon64_diag.h"    /* XBOX64_TRAZAS */

extern "C" void DebugOutputString(const char *s);

/* [Doom 64, Conker] Para el modo de marcado `volcado` (gx_d3d9_tev.cpp): el
 * combinador del N64 que esta puesto y si es de uno o de dos ciclos.  Solo
 * lee. */
extern "C" void gln64_mux_actual(unsigned int *m0, unsigned int *m1, unsigned int *ciclo)
{
	if (m0)    *m0    = gDP.combine.muxs0;
	if (m1)    *m1    = gDP.combine.muxs1;
	if (ciclo) *ciclo = gDP.otherMode.cycleType;
}

/* ★ LA DEFINICION DE `config`, QUE TAMBIEN VIVE EN EL FICHERO EXCLUIDO.
 *
 * Wii64 1b5041a introduce el `struct Config` de GLideN64 (Config.h) con la
 * mascara `generalEmulation.hacks`, y la define -- una linea, `Config
 * config;` -- en `Config_linux.cpp:33`, que es justo el fichero GTK que este
 * proyecto EXCLUYE del build.  Es el mismo tropiezo que ya costo tres builds
 * con `Config_LoadConfig`: al excluir un fichero hay que portar su lista
 * ENTERA, no solo la funcion que se echa en falta.
 *
 * Quien la rellena es `_RSP_SetGameHacks` (RSP.cpp:459-501), por nombre de
 * ROM, desde `RSP_Init`.
 * Enlace C++, igual que en upstream. */
Config config;

/* ★ Contadores de los dos subcomandos de `gSPModifyVertex` que colocan un
 * vertice directamente en pantalla.  La explicacion larga y como leerlos
 * esta en GLN64_MODIFYVTX_SCREEN (xenon/gln64_prof.h); salen en [GLN64MV].
 *
 * `extern "C"` obligatorio: quien los imprime es `profile.c`, que compila
 * como C y los declara sin decorar (ver reference_xdk_vcxproj_compileas). */
extern "C" volatile unsigned int gln64_dbg_modvtx_xy = 0;
extern "C" volatile unsigned int gln64_dbg_modvtx_z  = 0;

/* [Mario Tennis, marco del medidor] Buffers NUEVOS cargados desde su RDRAM
 * (FrameBuffer_GX_CargaNuevoDesdeRDRAM, GLideN64 8b359761): cuantos se miraron
 * y cuantos pixeles se pintaron (los que tenian algo).  Es el coste del cambio
 * en TODOS los juegos; salen en `[xbox64] cpu->gpu`. */
extern "C" volatile unsigned int gln64_dbg_fbrdram    = 0;
extern "C" volatile unsigned int gln64_dbg_fbrdram_px = 0;

/* Geometria con el color image apuntando al depth image.  Ver GLN64_CI_ES_ZI
 * en xenon/gln64_prof.h; salen en [GLN64ZI]. */
extern "C" volatile unsigned int gln64_dbg_cizi_tri  = 0;
extern "C" volatile unsigned int gln64_dbg_cizi_rect = 0;

/* ★★ Triangulos TIRADOS por tener un vertice con w <= 0, o sea detras del
 * ojo (OpenGL.cpp, dentro de OGL_AddTriangle), y triangulos emitidos por el
 * camino de la matriz combinada de W.
 *
 * Son la pareja que contesta la observacion clave del usuario: en el REPLAY
 * de Pilotwings la sombra sale bien en el suelo y JUGANDO no.  Misma
 * geometria y misma display list con otra camara, asi que lo que cambia
 * tiene que depender de la camara -- y este descarte lo hace.
 *
 * Y para Pilotwings la rama esta viva SIEMPRE: la tabla de microcodigos
 * (GBI.cpp:91) le pone `NoN = FALSE`, asi que no entra en el camino de
 * "sin recorte cercano" y cualquier triangulo con un vertice detras del ojo
 * se pierde ENTERO en vez de recortarse. */
extern "C" volatile unsigned int gln64_dbg_wcull = 0;
extern "C" volatile unsigned int gln64_dbg_combw = 0;
/* Triangulos emitidos SIN la matriz combinada de W, o sea los que pagan el
 * divide de perspectiva por vertice en la CPU.  Se lee en pareja con `combw`:
 * la proporcion es lo que decide si merece la pena ensanchar el test de
 * `_gSPUpdateCombW` (gSP.cpp:130). */
extern "C" volatile unsigned int gln64_dbg_nocombw = 0;


/* ===========================================================================
 * ★★ MODO DE MARCADO DE SHADERS (opcion de core `xbox64_marcatex`)
 * ===========================================================================
 * Sustituye el pixel shader por uno de diagnostico, o corta el real por una
 * etapa concreta, para ver DONDE se pierde el color.  Los modos y el porque
 * de cada uno estan en xenon/gx_d3d9_tev.cpp, que es donde viven.
 *
 * Esta variable es lo UNICO que el marcado necesita fuera del shim: todo lo
 * demas se resuelve con estado que el shim ya tiene.  Se hizo asi a proposito
 * para no dejar ganchos dentro del arbol copiado de Wii64 y poder seguir
 * trayendo codigo nuevo de ese proyecto sin conflictos.
 *
 * Herramienta que resolvio el escenario negro de 007 TWINE: la escalera de
 * cortes por etapa demostro que el combinador estaba bien de principio a fin,
 * y `sinniebla` senalo la niebla, que se aplica DESPUES y por eso ninguna
 * sonda de las entradas podia verla. */
extern "C" volatile int          gln64_marcaluz    = 0;

/* [Xbox360] La opcion de core `xbox64_fb_bilineal`: como se filtran las
 * TEXTURAS DE FRAMEBUFFER que se pintan en triangulos (Textures.cpp,
 * TextureCache_ActivateTexture).  0 = auto: bilineal solo en los juegos con
 * hack_fbTextureOffset (Conker y Mario Tennis, como GLideN64, que lleva ese
 * hack para corregir justo ese muestreo); 1 = bilineal siempre que el juego
 * no pida POINT (GLideN64); 2 = nunca (Wii64, GX_NEAR).  El texto de Pokemon
 * Stadium sale de un buffer pintado como textura a escala 1:1, y en bilineal
 * se emborronaba. */
extern "C" volatile int          gln64_fb_bilineal = 0;


/* ===========================================================================
 * ★★ `IsUsedAsDI` DE RICE, PORTADO -- Y POR QUE LA PRIMERA VERSION FALLO
 * ===========================================================================
 * La primera sonda pregunto `gDP.colorImage.address == gDP.depthImageAddress`
 * y salio CERO en Pilotwings (log 16).  Estaba mal planteada: el test de Rice
 * (`RSP_Parser.cpp:2180`) NO es una igualdad simultanea, es HISTORICO --
 * recuerda las DOS ultimas direcciones de depth image y acepta la anterior si
 * es reciente:
 *
 *     bool IsUsedAsDI(uint32 addr) {
 *         if (addr == g_ZI_saves[0].CI_Info.dwAddr) return true;
 *         else if (addr == g_ZI_saves[1].CI_Info.dwAddr &&
 *                  status.gDlistCount - g_ZI_saves[1].updateAtFrame < 10 &&
 *                  g_ZI_saves[1].CI_Info.dwAddr != 0) return true;
 *         return false;
 *     }
 *
 * La diferencia importa justo en el patron que se busca: el juego apunta el
 * COLOR image a una direccion que FUE el depth image, dibuja, y lo devuelve.
 * En ese momento `depthImageAddress` ya no vale eso, asi que la igualdad
 * instantanea no lo ve y la historica si.
 *
 * `RSP.DList` hace el papel de `status.gDlistCount` (RSP.cpp lo incrementa
 * una vez por display list, igual que Rice).
 *
 * Esto NO cambia el comportamiento por si solo: quien decide es el toggle
 * GLN64_CI_ES_ZI. */
static unsigned int s_ziAddr[2]  = { 0, 0 };   /* [0] actual, [1] anterior */
static unsigned int s_ziDList[2] = { 0, 0 };

extern "C" void gln64_zi_nota(unsigned int addr, unsigned int dlist)
{
	if (addr == s_ziAddr[0])
	{
		s_ziDList[0] = dlist;
		return;
	}

	s_ziAddr[1]  = s_ziAddr[0];
	s_ziDList[1] = s_ziDList[0];
	s_ziAddr[0]  = addr;
	s_ziDList[0] = dlist;
}

extern "C" int gln64_ci_es_zi(unsigned int ciAddr)
{
	if (ciAddr == 0)
		return 0;

	if (ciAddr == s_ziAddr[0])
		return 1;

	/* La resta va SIN SIGNO a proposito, como en Rice: si el contador de
	 * display lists girase, la diferencia sale enorme y la ventana se cierra
	 * sola en vez de aceptar cualquier cosa. */
	if (ciAddr == s_ziAddr[1] && s_ziAddr[1] != 0 &&
	    (unsigned int)(s_ziDList[0] - s_ziDList[1]) < 10u)
		return 1;

	return 0;
}

/* ===========================================================================
 * Censo de comandos de display list SIN IMPLEMENTAR
 * ===========================================================================
 * `GBI_Init` apunta los 256 huecos a `GBI_Unknown` y cada microcodigo rellena
 * solo los suyos, asi que todo lo que caiga ahi es una orden del juego que se
 * PIERDE EN SILENCIO (sin DEBUG no se imprime nada).
 *
 * Es una red ancha a proposito: tras dos hipotesis muertas de una en una
 * (gSPModifyVertex y el color image = depth image), interesa mas preguntar
 * "que le estamos ignorando a este juego" que seguir apostando.  El coste
 * es un incremento en un camino que hoy no hace nada.
 *
 * Se guarda la cuenta POR OPCODE, no un total: el opcode es lo que identifica
 * el comando y lo que permite ir a buscarlo a la referencia. */
static unsigned int s_gbiUnk[256];

extern "C" void gln64_gbi_unknown(unsigned int op)
{
#if XBOX64_TRAZAS   /* censo: solo lo lee el log */
	s_gbiUnk[op & 0xFF]++;
#else
	(void)op;
#endif
}

extern "C" unsigned int gln64_gbi_unknown_get(unsigned int op)
{
	return s_gbiUnk[op & 0xFF];
}

/* Las direcciones de depth image DISTINTAS que ha visto el juego, para poder
 * leer el reparto de buffers en vez de deducirlo. */
extern "C" void gln64_zi_get(unsigned int *a0, unsigned int *a1)
{
	if (a0) *a0 = s_ziAddr[0];
	if (a1) *a1 = s_ziAddr[1];
}

/* ===========================================================================
 * ★★ CENSO DE MODOS DE RENDER -- LA HERRAMIENTA PARA "APARECE DE PRONTO"
 * ===========================================================================
 * Situacion en Pilotwings tras tres medidas: no hay comandos sin implementar
 * ([GLN64GBI] vacio), no se dibuja al z-buffer ([GLN64ZI] a cero con el test
 * de Rice ya bien portado) y no se usa gSPModifyVertex ([GLN64MV] vacio).  La
 * sombra sale por el CAMINO NORMAL.  Asi que la pregunta ya no es "que nos
 * falta" sino "cual de los dibujos es".
 *
 * Y el usuario ha dado el dato que lo convierte en medible: el juego arranca
 * SIN la sombra y en un momento dado aparece.  Eso es una transicion, y una
 * transicion se puede cazar mirando QUE CONFIGURACION NUEVA aparece justo
 * entonces.
 *
 * La clave del censo es `gDP.otherMode.l >> 16`, o sea lo que el juego fija
 * con `gDPSetRenderMode`: ahi viven AA_EN, Z_CMP, Z_UPD, IM_RD, CLR_ON_CVG,
 * CVG_DST, ZMODE, CVG_X_ALPHA, ALPHA_CVG_SEL, FORCE_BL y los dos muxes del
 * blender.  Es el mismo numero sobre el que conmuta `_legacyBlending`
 * (OpenGL.cpp:956), asi que un valor de aqui se busca directo en esa tabla y
 * en la de GLideN64.
 *
 * Se imprime el DELTA de cada segundo, no el acumulado: lo que hay que
 * comparar son fases de la misma partida (mar / tierra / replay) y un
 * acumulado no deja verlas.
 *
 * Sin topes de "los primeros N" ([[feedback_sondas_filtrar_por_defecto]]): se
 * guardan hasta GLN64_RM_MAX distintos y, si se llenara, hay un contador de
 * desbordes para que no mienta en silencio.  Un juego usa del orden de una
 * docena.
 *
 * ★★ Y ADEMAS DEL MODO, DONDE.  El defecto es de POSICION -- la sombra se
 * pega al objeto en vez de quedarse en el suelo--, asi que el modo por si
 * solo no basta: hace falta ver a que altura de la pantalla dibuja cada uno.
 * Con eso, el modo de la sombra se identifica sin ambiguedad:
 *
 *   volando sobre el MAR  -> su caja baja, pegada al horizonte/agua
 *   volando sobre TIERRA  -> la misma caja se sube hasta el ala delta
 *
 * Es el mismo modo cambiando de sitio, y eso lo dice el log solo.  Se guarda
 * en NDC (y/w), que es lo que se acaba emitiendo: -1 abajo, +1 arriba.
 *
 * La caja se REINICIA en cada lectura, o sea que cada linea del log es de SU
 * segundo.  Si fuera acumulada acabaria siendo la pantalla entera para todo
 * el mundo y no distinguiria nada. */
#define GLN64_RM_MAX 48

static struct
{
	unsigned int rm, flags, tri, rect;
	float y0, y1;   /* caja en NDC del segundo en curso */
} s_rm[GLN64_RM_MAX];
static unsigned int s_rmNum  = 0;
static unsigned int s_rmOvf  = 0;

extern "C" void gln64_rm_nota(unsigned int rm, unsigned int flags,
                              int esRect, float y, float w)
{
#if XBOX64_TRAZAS   /* censo por dibujo: solo lo lee el log */
	unsigned int i;

	/* ★ LA CAJA SOLO CUENTA LO QUE ESTA EN PANTALLA.
	 *
	 * La primera version metia TODO vertice, y el resultado fue inservible
	 * para el modo del terreno: salian cajas como y[-35592, 2014].  No era un
	 * defecto del juego, es que un vertice con w minusculo (practicamente en
	 * el plano del ojo, detras del recorte) da un y/w enorme y se come el
	 * minimo y el maximo.  Filtrando por w>0 y |y/w|<=2 la caja dice lo que
	 * tiene que decir: donde cae ESE dibujo en la pantalla. */
	const int tieneY = (!esRect && w > 0.0f && y / w >= -2.0f && y / w <= 2.0f);
	const float ndcY = tieneY ? (y / w) : 0.0f;

	for (i = 0; i < s_rmNum; i++)
	{
		if (s_rm[i].rm == rm && s_rm[i].flags == flags)
		{
			if (esRect) s_rm[i].rect++; else s_rm[i].tri++;
			if (tieneY)
			{
				if (ndcY < s_rm[i].y0) s_rm[i].y0 = ndcY;
				if (ndcY > s_rm[i].y1) s_rm[i].y1 = ndcY;
			}
			return;
		}
	}

	if (s_rmNum >= GLN64_RM_MAX)
	{
		s_rmOvf++;
		return;
	}

	s_rm[s_rmNum].rm    = rm;
	s_rm[s_rmNum].flags = flags;
	s_rm[s_rmNum].tri   = esRect ? 0u : 1u;
	s_rm[s_rmNum].rect  = esRect ? 1u : 0u;
	s_rm[s_rmNum].y0    = tieneY ? ndcY :  9999.0f;
	s_rm[s_rmNum].y1    = tieneY ? ndcY : -9999.0f;
	s_rmNum++;
#else
	(void)rm; (void)flags; (void)esRect; (void)y; (void)w;
#endif
}

extern "C" unsigned int gln64_rm_num(void) { return s_rmNum; }
extern "C" unsigned int gln64_rm_ovf(void) { return s_rmOvf; }

/* ===========================================================================
 * ★★ CENSO DE LA PUERTA DEL MIP (puntos K y M)
 * ===========================================================================
 * SINTOMA MEDIDO: en Super Mario 64, cargando el juego EL PRIMERO se ven los
 * quesitos del marcador POWER; cargando otro juego antes, el aro se ve y los
 * quesitos NO.  Mismo defecto que arreglaron los puntos K y M, reaparecido
 * solo en la segunda carga.
 *
 * Y el diferencial de los dos logs dice algo importante: **el conjunto de
 * modos de render es IDENTICO**.  El juego emite los mismos dibujos con el
 * mismo estado del blender y de profundidad.  Lo que cambia tiene que ser la
 * TEXTURA, y la decision que la elige es la puerta de `TextureCache_Update`
 * (Textures.cpp): cadena de mips o carga normal.
 *
 * ★ Y esa puerta el censo `[GLN64RM]` NO PUEDE VERLA: mira
 * `gDP.otherMode.l`, y `textureLOD`/`textureDetail` estan en `otherMode.H`.
 * Llevaba tres sondas midiendo el lado equivocado para este defecto.
 *
 * La clave junta los cuatro valores que deciden, mas la unidad de textura.
 * Si entre las dos cargas cambia `level` o `tile`, ahi esta la respuesta; si
 * no cambia nada, la puerta queda descartada y el problema es de los texels.
 */
#define GLN64_TEX_MAX 24

static struct { unsigned int clave, n; } s_tex[GLN64_TEX_MAX];
static unsigned int s_texNum = 0;
static unsigned int s_texOvf = 0;

extern "C" void gln64_tex_nota(unsigned int lod, unsigned int detail,
                               unsigned int level, unsigned int tile,
                               unsigned int unidad)
{
#if XBOX64_TRAZAS   /* censo: solo lo lee el log */
	const unsigned int clave = ((lod    & 0xF) << 20)
	                         | ((detail & 0xF) << 16)
	                         | ((level  & 0xFF) << 8)
	                         | ((tile   & 0xF) << 4)
	                         |  (unidad & 0xF);
	unsigned int i;

	for (i = 0; i < s_texNum; i++)
		if (s_tex[i].clave == clave) { s_tex[i].n++; return; }

	if (s_texNum >= GLN64_TEX_MAX) { s_texOvf++; return; }

	s_tex[s_texNum].clave = clave;
	s_tex[s_texNum].n     = 1;
	s_texNum++;
#else
	(void)lod; (void)detail; (void)level; (void)tile; (void)unidad;
#endif
}

extern "C" unsigned int gln64_tex_num(void) { return s_texNum; }
extern "C" unsigned int gln64_tex_ovf(void) { return s_texOvf; }

extern "C" void gln64_tex_get(unsigned int i, unsigned int *lod,
                              unsigned int *detail, unsigned int *level,
                              unsigned int *tile, unsigned int *unidad,
                              unsigned int *n)
{
	if (i >= s_texNum) return;
	if (lod)    *lod    = (s_tex[i].clave >> 20) & 0xF;
	if (detail) *detail = (s_tex[i].clave >> 16) & 0xF;
	if (level)  *level  = (s_tex[i].clave >>  8) & 0xFF;
	if (tile)   *tile   = (s_tex[i].clave >>  4) & 0xF;
	if (unidad) *unidad =  s_tex[i].clave        & 0xF;
	if (n)      *n      =  s_tex[i].n;
}

/* ===========================================================================
 * ★★ CENSO DEL SITIO DEL PUNTO K  (`gSPUpdateTextureTiles`)
 * ===========================================================================
 * Gemela de la de arriba pero en OTRO INSTANTE, y esa es toda la gracia.
 *
 * `[GLN64TEX]` lee los bits de othermode dentro de `TextureCache_Update`; el
 * desplazamiento de tile del punto K se decide en `gSPUpdateTextureTiles`,
 * que corre cuando cambia el estado de textura.  Los bits LOD/DETAIL son
 * PEGAJOSOS -- por eso existe el punto K --, asi que leerlos en el momento
 * que no es no prueba nada.
 *
 * MEDIDO en la carga limpia de Mario 64 (marcador OK): `[GLN64TEX]` da UNA
 * sola combinacion, `lod0/det0/lvl0/tile0`, y la puerta del mip no se toma
 * ni una vez.  Con eso, el punto M es codigo muerto en ese juego y el
 * desplazamiento del punto K seria un no-op.  O sea que si el defecto de la
 * segunda carga esta en esta familia, tiene que aparecer AQUI.
 *
 * `rep` es `needReplaceTex1ByTex0()`, que decide el tile de la unidad 1 y es
 * el hallazgo 18 de AUDITORIA_GLIDEN64.txt: Wii64 le quita la opcion que en
 * la referencia lo mantiene APAGADO por defecto. */
#define GLN64_UTT_MAX 24

static struct { unsigned int clave, n; } s_utt[GLN64_UTT_MAX];
static unsigned int s_uttNum = 0;
static unsigned int s_uttOvf = 0;

extern "C" void gln64_utt_nota(unsigned int lod, unsigned int detail,
                               unsigned int level, unsigned int tile,
                               unsigned int rep)
{
#if XBOX64_TRAZAS   /* censo: solo lo lee el log */
	const unsigned int clave = ((lod    & 0xF) << 20)
	                         | ((detail & 0xF) << 16)
	                         | ((level  & 0xFF) << 8)
	                         | ((tile   & 0xF) << 4)
	                         |  (rep    & 0xF);
	unsigned int i;

	for (i = 0; i < s_uttNum; i++)
		if (s_utt[i].clave == clave) { s_utt[i].n++; return; }

	if (s_uttNum >= GLN64_UTT_MAX) { s_uttOvf++; return; }

	s_utt[s_uttNum].clave = clave;
	s_utt[s_uttNum].n     = 1;
	s_uttNum++;
#else
	(void)lod; (void)detail; (void)level; (void)tile; (void)rep;
#endif
}

/* ===========================================================================
 * ★★ ESTADO DEL MONTON DE TEXTURAS -- "algo no se libera bien"
 * ===========================================================================
 * Lo propuso el usuario y encaja con todo lo medido hasta ahora: en Mario 64
 * los quesitos del marcador salen en carga limpia y no salen si antes se
 * cargo otro juego, y los modos de render, los tiles y la puerta del mip son
 * IDENTICOS en los dos casos.  Una textura que NO SE PUEDE RESERVAR explica
 * "falta ese dibujo y nada mas" sin que cambie ninguna otra cosa.
 *
 * Y hay precedente exacto en este arbol: los personajes negros de Paper Mario
 * eran `__lwp_heap_allocate` devolviendo NULL por falta de sitio.
 *
 * ★ Lo que YA esta medido y por tanto NO hay que volver a suponer: la cache
 * de glN64 SI se vacia al cambiar de ROM.  En el log 19 `texfree` paso de 0 a
 * 61 justo en el cambio, que son exactamente los 61 `texconv` de Smash Bros,
 * y esos 61 los vio nuestro `lwp_heap_free_hook`.  O sea que las entradas de
 * la cache se liberan.
 *
 * Lo que NO estaba medido es si el MONTON queda realmente vacio.  El heap
 * (`GXtexCache`) sobrevive a la carga -- `TextureCache_Init` solo lo crea
 * `if (!GXtexCache)` --, asi que cualquier reserva que no pase por la lista
 * de la cache se queda dentro para siempre y va comiendo sitio en cada
 * juego.  Eso es exactamente "algo no se libera bien", y esto lo mide en vez
 * de discutirlo:
 *
 *   free_size que BAJA de una carga a la siguiente -> hay fuga, y con cuanto
 *   lwp_heap_fallos > 0                            -> ya no cabe: causa directa
 */
/* `GXtexCache` la DEFINE Textures.cpp sin `extern "C"`, o sea con enlace
 * C++; se repite aqui porque el montaje del monton (mas abajo en este mismo
 * fichero) la declara igual y esta funcion va antes. */
extern heap_cntrl *GXtexCache;

extern "C" void gln64_heap_info(unsigned int *libre, unsigned int *usado,
                                unsigned int *bloquesLibres,
                                unsigned int *fallos, unsigned int *reservas)
{
	heap_iblock info;

	if (libre)         *libre = 0;
	if (usado)         *usado = 0;
	if (bloquesLibres) *bloquesLibres = 0;
	if (fallos)        *fallos   = lwp_heap_fallos;
	if (reservas)      *reservas = lwp_heap_reservas;

	if (!GXtexCache)
		return;

	memset( &info, 0, sizeof(info) );
	__lwp_heap_getinfo( GXtexCache, &info );

	if (libre)         *libre = info.free_size;
	if (usado)         *usado = info.used_size;
	if (bloquesLibres) *bloquesLibres = info.free_blocks;
}

extern "C" unsigned int gln64_utt_num(void) { return s_uttNum; }
extern "C" unsigned int gln64_utt_ovf(void) { return s_uttOvf; }

extern "C" void gln64_utt_get(unsigned int i, unsigned int *lod,
                              unsigned int *detail, unsigned int *level,
                              unsigned int *tile, unsigned int *rep,
                              unsigned int *n)
{
	if (i >= s_uttNum) return;
	if (lod)    *lod    = (s_utt[i].clave >> 20) & 0xF;
	if (detail) *detail = (s_utt[i].clave >> 16) & 0xF;
	if (level)  *level  = (s_utt[i].clave >>  8) & 0xFF;
	if (tile)   *tile   = (s_utt[i].clave >>  4) & 0xF;
	if (rep)    *rep    =  s_utt[i].clave        & 0xF;
	if (n)      *n      =  s_utt[i].n;
}

/* OJO: reinicia la caja.  Ver arriba -- cada linea del log tiene que ser de
 * su segundo para que mar y tierra se puedan comparar. */
extern "C" void gln64_rm_get(unsigned int i, unsigned int *rm,
                             unsigned int *flags,
                             unsigned int *tri, unsigned int *rect,
                             float *y0, float *y1)
{
	if (i >= s_rmNum) return;
	if (rm)    *rm    = s_rm[i].rm;
	if (flags) *flags = s_rm[i].flags;
	if (tri)   *tri   = s_rm[i].tri;
	if (rect)  *rect  = s_rm[i].rect;
	if (y0)    *y0    = s_rm[i].y0;
	if (y1)    *y1    = s_rm[i].y1;

	s_rm[i].y0 =  9999.0f;
	s_rm[i].y1 = -9999.0f;
}

/* ===========================================================================
 * ★★ EL TAMANO DE LA RDRAM: 4 MB CABLEADOS DONDE HAY 8
 * ===========================================================================
 * `RSP_Init()` (RSP.cpp:474) decide `RDRAMSize` EN TIEMPO DE COMPILACION:
 *
 *     # ifdef __GX__
 *     #  ifdef USE_EXPANSION
 *          RDRAMSize = 1024 * 1024 * 8;
 *     #  else
 *          RDRAMSize = 1024 * 1024 * 4;
 *
 * y `USE_EXPANSION` NO esta definido en ningun sitio de este proyecto.  O sea
 * que glN64 cree que la RDRAM son 4 MB mientras el core corre con el
 * Expansion Pak puesto (8 MB, `xenon64_expansion_pak`).  Todos los guardas
 * `(address + N) > RDRAMSize` de gSP.cpp y gDP.cpp -- hay 20 y pico --
 * rechazan entonces cualquier direccion de la mitad alta, incluido el propio
 * `RSP.PC` del bucle de la display list.
 *
 * Es el MISMO fallo de forma que `gfx_set_window` y que el propio
 * `Config_LoadConfig` de este fichero: un valor que en Wii64 fija la
 * compilacion del frontend y que al portar se quedo sin portar.
 *
 * ★ Y NO se arregla definiendo `USE_EXPANSION`: eso cablearia 8 MB tambien
 * cuando el usuario apaga el Expansion Pak, y entonces glN64 aceptaria
 * direcciones fuera del buffer que el core ha reservado.  El valor tiene que
 * venir del core, que es quien lo sabe.
 *
 * ★★ Y NO ES SOLO UNA COTA, ES UNA MASCARA.  Aqui es donde se vuelve un
 * CUELGUE y no una pantalla negra (RSP.h:57):
 *
 *     #define RSP_SegmentToPhysical( segaddr ) \
 *         ((gSP.segment[...] + (segaddr & 0x00FFFFFF)) & (RDRAMSize - 1))
 *
 * Con 4 MB eso es `& 0x3FFFFF`: toda direccion de la mitad alta se PLIEGA a
 * la mitad baja en vez de rechazarse.  Majora's Mask tiene sus display lists
 * por encima de 4 MB, asi que `gSPDisplayList` saltaba a direcciones
 * minusculas y el parser recorria basura -- NOOPs-- sin fin.  Medido con el
 * vigilante: `PC=00000138 cmd=00 PCi=2 RDRAMSize=4096KB`.  Y por eso el
 * `break` por cota del bucle no salvaba nada: el enmascarado impide que el
 * PC llegue nunca a pasarse.
 *
 * ORDEN, que es lo delicado: `RSP_Init()` corre DENTRO de `gfx.romOpen()`,
 * asi que esto hay que aplicarlo DESPUES (lo hace `main.cpp`), que es cuando
 * ya nadie lo va a pisar.
 *
 * ★ DE DONDE SALE EL TAMANO.  El primer intento leia
 * `g_dev.rdram.dram_size`, y ese campo NO LO FIJA NADIE en este arbol: el
 * struct RDRAM del RCP esta a medio migrar, igual que la mascara del MI
 * (ver memory.c:2992).  Valia 0, la guarda de abajo salia temprano y el
 * arreglo no hacia nada -- ni siquiera dejaba su linea en el log.  La fuente
 * de verdad aqui es la opcion `DisableExtraMem`, que es la que consulta
 * `dma.c:417` para decirle al juego cuanta RAM hay. */

/* -- Vigilante del bucle de la display list.  Ver gln64_prof.h ------------- */
#if GLN64_WATCHDOG
extern "C" unsigned int gln64_wd_n = 0;

extern "C" int gln64_wd_trip(unsigned int pc, unsigned int cmd)
{
#if XBOX64_TRAZAS
	static int avisado = 0;
	char ob[160];

	if (!avisado)
	{
		avisado = 1;
		sprintf(ob, "[GLN64WD] lista desbocada: >%u comandos | PC=%08X cmd=%02X"
		            " PCi=%u RDRAMSize=%uKB halt=%u",
		        GLN64_WD_LIMIT, pc, cmd & 0xFF, RSP.PCi,
		        RDRAMSize / 1024, RSP.halt);
		DebugOutputString(ob);
	}
#else
	(void)pc; (void)cmd;
#endif
	return 1;
}
#endif

/* ★ SONDAS RETIRADAS (2026-09-22), una vez cerrado el framebuffer de
 * Pokemon Stadium.  Estaban aqui y ya no hacen falta:
 *
 *   [CIMG-GX]   los color image distintos que usa el juego
 *   [FBTEX-GX]  los tamaños de cada framebuffer-como-textura
 *   [FBLOAD-GX] las cargas de textura DESVIADAS al framebuffer
 *   [FBRECH-GX] las RECHAZADAS, con el motivo (fue la que dio la causa raiz)
 *   [FBACT-GX]  escalas y desplazamientos al activar la textura
 *   [FBRECT-GX] donde acaba en pantalla cada dibujo de framebuffer
 *
 * Lo que queda de todo aquello son los DOS arreglos, sin toggle y
 * documentados donde viven: `gln64_cimg_escala` aqui abajo, y en el arbol
 * copiado el guarda estrechado de `_gDPCheckForFrameBufferTexture` y el
 * desplazamiento en dos ejes de `FrameBuffer_ActivateBufferTexture`.
 *
 * ★ Y LA LECCION DE LAS SONDAS, que costo cuatro builds: los topes de
 * "primeras N distintas" me recortaron la evidencia TRES veces seguidas
 * (8, 48, 96), y siempre justo en la escena que habia que diagnosticar.
 * La regla es filtrar por el DEFECTO, no por un tope.  Si vuelven, que
 * vuelvan con la clave puesta en lo que se busca. */


/* -- GLN64_ESCALA_POR_CI: la escala tiene que CABER en el EFB -------------
 *
 * ★★ EL DEFECTO, MEDIDO.  `OGL_UpdateScale()` saca la escala SOLO del VI
 * (OpenGL.cpp:328): `GXscaleX = GXwidth / VI.width`.  Con VI=320 y un render
 * target de 640 eso da 2, y vale mientras el color image sea tan ancho como
 * el VI.  Pokemon Stadium dibuja sus paneles en color image MAS ANCHOS:
 *
 *   [FBTEX-GX] n64=538x100 real=640x200 esperado=1076x200
 *   [FBTEX-GX] n64=640x480 real=640x480 esperado=1280x960
 *
 * Un buffer de 640 px de N64 a escala 2 necesitaria 1280 px de EFB y solo hay
 * 640, asi que FrameBuffer.cpp:307 RECORTA el tamaño de la textura.  Dos
 * consecuencias, y las dos se ven en pantalla:
 *
 *   1. al DIBUJAR, solo entra la mitad izquierda del panel;
 *   2. al MUESTREAR, `scaleS = GXscaleX / realWidth` sigue usando el 2, o sea
 *      que esa mitad se estira sobre el quad entero.
 *
 * Resultado: contenido al doble y recortado.  Es el sintoma exacto de los
 * cuadros de texto que no caben.
 *
 * ★ EL ARREGLO: que la escala del color image ACTUAL sea la que cabe, CADA
 * EJE POR SU CUENTA y con el mismo denominador que la ortho de los rects
 * (gln64_rect_ancho/alto).  Ese invariante -- `escala = EFB / denominador de
 * la ortho` -- es lo que hace que el contenido dibujado y la region
 * capturada coincidan, y tiene que cumplirse en los dos ejes.
 *
 * Solo puede BAJAR de la escala del VI, nunca subir, asi que para todo lo que
 * ya funcionaba (color image tan ancho como el VI o menos) el valor no cambia
 * ni un bit y el binario se comporta igual.
 *
 * ★★ ESTO ES LO QUE HACE GLideN64 BIEN Y glN64 NO: alli cada framebuffer
 * lleva su propia escala.  Aqui se consigue el mismo efecto sin tocar la
 * estructura, porque el color image activo es uno solo en cada momento.
 *
 * A 0 vuelve el comportamiento de antes, para el A/B. */
#define GLN64_ESCALA_POR_CI 1

/* -- El ancho/alto en el que viven los RECTS ------------------------------
 *
 * `OGL_DrawRect` y `OGL_DrawTexturedRect` montan su ortho sobre
 * `VI.width x VI.height` (OpenGL.cpp:1560 y :1793):
 *
 *     guOrtho(proj, 0, VI.height, 0, VI.width, 1, -1);
 *
 * o sea que dan por hecho que el color image mide lo mismo que el VI.  Es
 * cierto para el buffer principal y falso para un panel auxiliar mas ancho:
 * Pokemon Stadium dibuja la barra de aviso en un color image de 538 px con
 * el VI a 320, asi que un rect en x=538 sale a NDC 538/320 = 1,68 y se va por
 * la derecha en esa proporcion.  Es la barra roja cuyo texto no cabe.
 *
 * La regla es la MISMA que usa gln64_cimg_escala() para la escala -- el mayor
 * entre el VI y el color image --, y por eso las dos son coherentes: el rect
 * se mapea sobre el mismo ancho con el que se dimensiona la textura.  Para
 * todo color image tan ancho como el VI o mas estrecho, el valor no cambia y
 * el comportamiento es el de siempre. */
/* ★ [Lode Runner 3-D] UN COLOR IMAGE MAS ANCHO QUE EL VI NO SIEMPRE ES UN
 * PANEL.  Lode Runner pinta en un framebuffer de 640 de zancada con dos
 * imagenes de 320 lado a lado (ci 0036A000 / 0036A280) y el VI muestra 320
 * columnas: el color image mide 640 y es el que se ESCANEA.  La regla de los
 * paneles lo trataba como uno -- ortho sobre 640 y escala 640/640 = 1 -- y
 * el juego entero quedaba en la mitad izquierda.  El juego solo pinta en sus
 * 320 columnas (recorte x <= 320), asi que con la escala del VI cabe.
 *
 * El criterio: el ancho del color image es la ZANCADA que recorre el VI
 * (VI_WIDTH) -> es el framebuffer que se muestra, y manda el VI, como en
 * GLideN64, donde la escala de todos los buffers sale del VI.  Los paneles de
 * Pokemon Stadium (538 y 640 con VI_WIDTH = 320) no cumplen eso y siguen con
 * su regla.  Con el color image no mas ancho que el VI no cambia nada. */
static int gln64_ci_es_el_escaneado(void)
{
	return gDP.colorImage.width == (u32)(*REG.VI_WIDTH & 0xFFF);   /* _SHIFTR(...,0,12) */
}

extern "C" float gln64_rect_ancho(void)
{
#if GLN64_ESCALA_POR_CI
	if (gDP.colorImage.width > VI.width && !gln64_ci_es_el_escaneado())
		return (float)gDP.colorImage.width;
#endif
	return (float)VI.width;
}

extern "C" float gln64_rect_alto(void)
{
#if GLN64_ESCALA_POR_CI
	/* La altura del color image la DEDUCE glN64 segun el juego dibuja, asi
	 * que puede ir por detras; se usa solo si ya es mayor que la del VI. */
	if (gDP.colorImage.height > VI.height && !gln64_ci_es_el_escaneado())
		return (float)gDP.colorImage.height;
#endif
	return (float)VI.height;
}

/* ★★ LIMITACION CONOCIDA, y dejarla escrita es mejor que fingir que no esta.
 *
 * Esta funcion corre UNA VEZ, al cambiar de color image, pero
 * `gDP.colorImage.height` NO es definitiva en ese momento: glN64 la deduce
 * segun el juego dibuja (`MAX(height, lry)` en gDPFillRectangle).  Si crece
 * por encima de VI.height DESPUES de esta llamada, `gln64_rect_alto()`
 * devolvera un denominador mayor que el que se uso para `GXscaleY` y el
 * invariante se rompe hasta el siguiente cambio de color image.
 *
 * En la practica converge en una pasada -- el buffer se vuelve a seleccionar
 * cada frame y entonces la altura ya es la buena --, que es por lo que los
 * dos buffers de 640x480 de Pokemon Stadium acaban con esc=1.000 correcto.
 * Pero el PRIMER frame tras estrenar un buffer alto puede salir mal.
 *
 * No se arregla aqui porque haria falta recalcular la escala cada vez que la
 * altura crece, y eso es un cambio en gDPFillRectangle -- mas superficie en
 * el arbol copiado -- que hoy no pide ningun defecto medido. */
extern "C" void gln64_cimg_escala(unsigned int width)
{
#if GLN64_ESCALA_POR_CI
	float sx, sy;
	unsigned int vw;

	/* Sonda: empieza a contar las coordenadas del buffer que ENTRA.  Va lo
	 * primero, antes de cualquier `return`, o el contador se quedaria con lo
	 * del buffer anterior.  Lo imprime [FBTEX-GX] al guardarlo. */
	gxd3d_ci_actual(width);

	if (!width || !VI.width || !VI.height || !OGL.GXwidth || !OGL.GXheight)
		return;

	/* ★★ CADA EJE POR SU CUENTA, Y EL CLAMP "UNIFORME" QUE HABIA AQUI ERA UN
	 * ERROR.  La primera version hacia `if (sy > sx) sy = sx;` con el
	 * argumento de "no deformar", y ese argumento es falso: la textura se
	 * muestrea con scaleS y scaleT INDEPENDIENTES, asi que lo que se estira
	 * al dibujar se des-estira al leer y no se deforma nada.
	 *
	 * Lo que si rompia era la coherencia con la ortho de los rects.  El
	 * invariante que tiene que cumplirse en los DOS ejes es
	 *
	 *     escala = tamaño del EFB / denominador de la ortho
	 *
	 * y la ortho usa `max(VI, color image)` (gln64_rect_ancho/alto).  En el
	 * ancho salia bien -- 640/538 con ortho sobre 538 --, pero al forzar
	 * `sy = sx` el alto quedaba con escala 1,19 y ortho sobre 240: un rect en
	 * y=100 caia en la fila 200 del EFB y la captura solo se llevaba hasta la
	 * 118.  Ese es el "falta redimensionado en altura" de la barra inferior.
	 *
	 * Con cada eje por su cuenta los dos cumplen el invariante por
	 * construccion y no hay nada que cuadrar a mano. */
	/* ★ Los DOS denominadores salen de gln64_rect_ancho/alto, que es lo que
	 * usa la ortho.  El ancho se calculaba aparte con el parametro `width` y
	 * daba lo mismo -- esta funcion se llama despues de actualizar
	 * gDP.colorImage --, pero tener dos fuentes para el mismo numero es como
	 * se desincronizan luego.  Una sola. */
	(void)vw;
	sx = (float)OGL.GXwidth  / gln64_rect_ancho();
	sy = (float)OGL.GXheight / gln64_rect_alto();

	if (OGL.GXscaleX == sx && OGL.GXscaleY == sy)
		return;

	OGL.GXscaleX = sx;
	OGL.GXscaleY = sy;

	/* El viewport y el recorte se derivan de la escala, asi que hay que
	 * pedir que se vuelvan a emitir; si no, el cambio no llega hasta que el
	 * juego los toque por su cuenta. */
	gSP.changed |= CHANGED_VIEWPORT;
	gDP.changed |= CHANGED_SCISSOR;
#else
	(void)width;
#endif
}


/* -- PROBADO Y RETIRADO: el marcador de validez del framebuffer -----------
 *
 * glN64 escribe un MARCADOR en RDRAM al guardar un framebuffer -su propia
 * direccion de arranque, FrameBuffer.cpp:267 y :407- y en el glN64 de PC ese
 * marcador SE LEE en `FrameBuffer_FindBuffer` para saber si el juego ha
 * pisado el buffer.  Wii64 se dejo por el camino su unico lector, asi que
 * aqui se escribe en dos sitios y no se lee en ninguno.
 *
 * Se porto ese lector para explicar la estela de la intro de GoldenEye
 * (repintar una textura vieja encima de un buffer ya limpiado).  DOS
 * resultados, los dos medidos:
 *
 *   1. NO era eso.  `fbvieja=0` en todo el log de GoldenEye: el marcador
 *      sobrevivia, o sea que el juego no estaba pisando el buffer.  La estela
 *      era el quad de borrado de glN64, que no llegaba a pintar -- arreglado
 *      con el Clear() nativo de D3D, ver gxd3d_clear_efb en gx_d3d9.cpp.
 *   2. ★ Y HACIA DANO.  Puesto tambien en `FindBuffer` -que es el camino de
 *      framebuffer-COMO-TEXTURA, no el de restaurar-, en Zelda OoT el
 *      marcador SI se rompe: FindBuffer devolvia NULL y se perdian el fondo
 *      del menu de pausa y el muñeco de Link, y los menus de Pokemon Stadium
 *      salian con basura.
 *
 * Retirado entero.  Si algun dia hace falta, el sitio es FindBuffer y la
 * condicion es `*(u32*)&RDRAM[startAddress] == startAddress`, pero hara falta
 * un defecto MEDIDO que lo pida: hoy no lo hay y el precio esta comprobado. */

/* ★★ HISTORIA DE UNA SONDA MAL PLANTEADA, dos veces, y vale la pena tenerla
 * escrita porque la clase se repite.
 *
 * Habia dos contadores, `efbclear` y `zclear`, que contaban `GX_SetZMode`
 * con `GX_GEQUAL` para medir los borrados de pantalla.  Fallaron por los dos
 * extremos posibles:
 *
 *   1. De MAS: ahi caian dos cosas distintas -- el quad de borrado y la
 *      funcion de profundidad NORMAL del juego, que en GoldenEye tambien es
 *      GEQUAL --, asi que un valor alto no demostraba nada.
 *   2. De MENOS: Wii64 1b5041a paso el borrado a `GX_ALWAYS` y desde
 *      entonces los dos valian 0 SIEMPRE, sin avisar.  Un 0 parece "no pasa
 *      nada" cuando lo que pasa es que la sonda ya no mira donde toca.
 *
 * Retirados.  Los de ahora (`clrcol`/`clrz`/`clrrgb`) cuentan DENTRO de
 * `gxd3d_clear_efb`, o sea en el sitio del borrado y no en un rasgo
 * indirecto suyo.  Esa es la regla. */

/* ★ [Pilotwings, log 2] LAS SONDAS SE REINICIAN CON CADA JUEGO.  Sus listas
 * de "ya visto" y sus topes eran `static` dentro de cada funcion: duraban la
 * sesion entera.  En el log de Pilotwings [CIMG-GX] empezaba en #37 y
 * [FILL-GX] no escribio NI UNA linea -- no porque el juego no rellenase,
 * sino porque los juegos anteriores ya habian gastado sus 48 huecos.  Un
 * cero que parecia "no pasa" y era "la sonda ya no mira".  Ahora viven aqui
 * y `gln64_sondas_reinicia` las vacia desde RSP_Init, al cargar cada ROM. */
#if XBOX64_TRAZAS   /* sondas: solo con trazas; en la publica, vacias */
static unsigned int s_cimgVistos[64];  static int s_cimgN = 0;
static unsigned int s_restVistos[32];  static int s_restN = 0;
static int s_rdramN = 0;
static unsigned int s_fillVistos[48];  static int s_fillN = 0;

extern "C" void gln64_sondas_reinicia(void)
{
	s_cimgN = s_restN = s_rdramN = s_fillN = 0;
}

/* ★ [CIMG-GX]: los color image distintos.  Ver la nota en gln64_prof.h.
 * Una linea por direccion nueva; tope generoso porque el caso que se busca
 * es "cuantos hay", y recortar la lista seria recortar la respuesta. */
extern "C" void gln64_cimg_nota(unsigned int addr, unsigned int width, unsigned int size)
{
	unsigned int *vistos = s_cimgVistos;
	int &n = s_cimgN;
	char ob[192];
	int i;

	for (i = 0; i < n; i++)
		if (vistos[i] == addr)
			return;
	if (n >= 64)
		return;
	vistos[n++] = addr;

	sprintf(ob, "[CIMG-GX] #%d addr=%08X w=%u size=%u | VI=%ux%u GX=%ux%u"
	            " esc=%.3f,%.3f | zi=%08X%s",
	        n, addr, width, size,
	        (unsigned int)VI.width, (unsigned int)VI.height,
	        (unsigned int)OGL.GXwidth, (unsigned int)OGL.GXheight,
	        OGL.GXscaleX, OGL.GXscaleY,
	        gDP.depthImageAddress,
	        (addr == gDP.depthImageAddress) ? " ★ci==zi" : "");
	DebugOutputString(ob);
}

/* ★ [REST-GX]: si al cambiar de color image se encuentra el buffer que hay
 * que repintar en el EFB.  Ver la nota en xenon/gln64_prof.h. */
extern "C" void gln64_restore_nota(unsigned int addr, unsigned int width,
                                   unsigned int size, int encontrado)
{
	unsigned int *vistos = s_restVistos;
	int &n = s_restN;
	char ob[160];
	unsigned int huella = addr ^ (width << 20) ^ (size << 28)
	                      ^ (encontrado ? 0x80000000u : 0u);
	int i;

	for (i = 0; i < n; i++)
		if (vistos[i] == huella)
			return;
	if (n >= 32)
		return;
	vistos[n++] = huella;

	sprintf(ob, "[REST-GX] #%d addr=%08X w=%u size=%u | %s",
	        n, addr, width, size,
	        encontrado ? "repinta el buffer"
	                   : "★ NO LO ENCUENTRA (el EFB se queda con lo anterior)");
	DebugOutputString(ob);
}

/* ★★ [RDRAM-GX]: QUE HAY EN RDRAM en el buffer al que se cambia.
 *
 * Es la ultima pregunta abierta de la estela de Killer Instinct.  Lo demas
 * ya esta medido: doble buffer de 640x480 ([CIMG-GX]), sin depth image,
 * `gDPFillRectangle` nunca ([FILL-GX] = 0) y el guardar/repintar FUNCIONA
 * ([REST-GX]).  Con la tuberia sana y nada que limpie, solo queda que el
 * juego limpie el framebuffer POR CPU escribiendo RDRAM -- algo que el
 * plugin no ve nunca, porque solo mira los comandos del RDP.
 *
 * Esto lo prueba o lo tira: se muestrean palabras de RDRAM repartidas por
 * el buffer justo cuando se va a repintar.
 *
 *   todas iguales (00000000 o un color plano)  -> la CPU lo ha limpiado y
 *       repintar la textura guardada es justo lo que crea la estela.
 *   contenido variado                          -> la CPU no lo limpia y la
 *       hipotesis se cae; habria que mirar otra cosa.
 *
 * Se muestrea en DIAGONAL, no las primeras palabras seguidas: la esquina
 * superior izquierda de una pantalla de creditos es negra en los dos casos
 * y no distinguiria nada. */
extern "C" void gln64_rdram_nota(unsigned int addr, unsigned int width,
                                 unsigned int height, unsigned int size)
{
	int &n = s_rdramN;
	char ob[224];
	char trozo[16];
	const u32 stride = width << size >> 1;
	u32 v[6];
	int i, iguales = 1;

	if (n >= 8 || stride == 0)
		return;

	for (i = 0; i < 6; i++)
	{
		const u32 fila = (height * i) / 6;
		const u32 col  = ((width  * i) / 6) << size >> 1;
		const u32 off  = fila * stride + (col & ~3u);

		v[i] = (addr + off + 3 < RDRAMSize) ? *(u32*)&RDRAM[addr + off] : 0;
		if (v[i] != v[0])
			iguales = 0;
	}

	n++;
	sprintf(ob, "[RDRAM-GX] #%d addr=%08X %ux%u |", n, addr, width, height);
	for (i = 0; i < 6; i++)
	{
		sprintf(trozo, " %08X", v[i]);
		strcat(ob, trozo);
	}
	strcat(ob, iguales ? "  ★ TODAS IGUALES (la CPU lo ha limpiado)"
	                   : "  contenido variado");
	DebugOutputString(ob);
}

/* ★ [FILL-GX]: que rellena el juego.  Ver la nota en xenon/gln64_prof.h.
 * Deduplica por la COMBINACION entera (geometria + modo + color + las dos
 * direcciones + rama), no por un tope: un relleno por frame repetido sale
 * una sola vez, y cualquier variacion sale aparte, que es lo que interesa. */
extern "C" void gln64_fill_nota(int ulx, int uly, int lrx, int lry,
                                unsigned int cycleType, unsigned int fillColor,
                                unsigned int colorAddr, unsigned int depthAddr,
                                unsigned int rama)
{
	unsigned int *vistos = s_fillVistos;
	int &n = s_fillN;
	static const char *nombreRama[4] = { "?", "borra Z", "borra COLOR", "rect normal" };
	char ob[240];
	unsigned int huella;
	int i;

	huella = ((unsigned int)ulx << 1) ^ ((unsigned int)uly << 7)
	         ^ ((unsigned int)lrx << 13) ^ ((unsigned int)lry << 19)
	         ^ (cycleType << 26) ^ fillColor ^ colorAddr ^ (depthAddr << 3)
	         ^ (rama << 29);

	for (i = 0; i < n; i++)
		if (vistos[i] == huella)
			return;
	if (n >= 48)
		return;
	vistos[n++] = huella;

	sprintf(ob, "[FILL-GX] #%d %d,%d-%d,%d (VI %ux%u) | ciclo=%u fill=%08X"
	            " DCC=%08X%s | ci=%08X zi=%08X%s | %s",
	        n, ulx, uly, lrx, lry,
	        (unsigned int)VI.width, (unsigned int)VI.height,
	        cycleType, fillColor, DepthClearColor,
	        (fillColor == DepthClearColor) ? " ★fill==DCC" : "",
	        colorAddr, depthAddr,
	        (colorAddr == depthAddr) ? " ★ci==zi" : "",
	        nombreRama[rama & 3]);
	DebugOutputString(ob);
}
#else  /* !XBOX64_TRAZAS: las sondas existen para enlazar, sin cuerpo */
extern "C" void gln64_sondas_reinicia(void) { }
extern "C" void gln64_cimg_nota(unsigned int addr, unsigned int width, unsigned int size)
{
	(void)addr; (void)width; (void)size;
}
extern "C" void gln64_restore_nota(unsigned int addr, unsigned int width,
                                   unsigned int size, int encontrado)
{
	(void)addr; (void)width; (void)size; (void)encontrado;
}
extern "C" void gln64_rdram_nota(unsigned int addr, unsigned int width,
                                 unsigned int height, unsigned int size)
{
	(void)addr; (void)width; (void)height; (void)size;
}
extern "C" void gln64_fill_nota(int ulx, int uly, int lrx, int lry,
                                unsigned int cycleType, unsigned int fillColor,
                                unsigned int colorAddr, unsigned int depthAddr,
                                unsigned int rama)
{
	(void)ulx; (void)uly; (void)lrx; (void)lry; (void)cycleType;
	(void)fillColor; (void)colorAddr; (void)depthAddr; (void)rama;
}
#endif /* XBOX64_TRAZAS */

extern "C" void gln64_apply_rdram_size(unsigned int bytes)
{
#if XBOX64_TRAZAS
	char ob[96];
#endif

	/* Potencia de dos obligatoria: RSP_SegmentToPhysical la usa como mascara
	 * (`& (RDRAMSize - 1)`), asi que un valor raro corrompe TODA direccion
	 * segmentada.  Si llega algo que no cuadra, mejor dejar lo de RSP_Init. */
	if (bytes == 0 || (bytes & (bytes - 1)) != 0)
	{
#if XBOX64_TRAZAS
		sprintf(ob, "[GLN64] RDRAMSize: valor invalido %u, dejo %u KB",
		        bytes, RDRAMSize / 1024);
		DebugOutputString(ob);
#endif
		return;
	}

	RDRAMSize = bytes;

#if XBOX64_TRAZAS
	sprintf(ob, "[GLN64] RDRAMSize = %u KB (del core)", bytes / 1024);
	DebugOutputString(ob);
#endif
}

/* Las dos banderas que si vienen de core options.  Las define
 * `gln64_glue.cpp`; se declaran igual que en `OpenGL.cpp:73-74` -- a pelo,
 * o sea con enlace C++.  (No estan en ninguna cabecera: cada consumidor
 * repite el `extern`.) */
/* [xbox64] `extern "C"`: mismo caso que glN64_GX/OpenGL.cpp:73.  Las
 * define gln64_glue.cpp con enlace C, porque incluye wii64config.h. */
extern "C" char glN64_useFrameBufferTextures;
extern "C" char glN64_use2xSaiTextures;

/* ------------------------------------------------------------------ */
/* ★★ EL MONTON DE TEXTURAS: 2,4 MB ES EL PRESUPUESTO DE LA GAMECUBE   */
/* ------------------------------------------------------------------ */
/* `GX_TEXTURE_CACHE_SIZE` (Textures.h:89-98) sale de un arbol de `#ifdef`
 * por consola: 8 MB con HW_RVL (Wii), 1 MB con USE_EXPANSION, y **0x264000
 * = 2,4 MB** en el caso que nos toca, que es el de la GameCube pelada.
 * Nadie lo adapto a esta maquina.
 *
 * ★ Y LA REFERENCIA QUE FUNCIONA ESTA EN ESTE MISMO ARBOL: Rice corre con
 * `g_maxTextureMemUsage = 32 MB` (TextureManager.cpp:162, la rama `#else`
 * -- la de `__GX__` no se compila, `__GX__` solo esta en los per-fichero de
 * glN64).  Trece veces mas.
 *
 * MEDIDO en Paper Mario, escena de los cuatro personajes (log 30):
 *
 *     [GLN64C] cache nodos=15 ... 2430KB        <- clavado en el techo
 *     [GLN64] texconv=3022->4748  texfree=3011->4738   (crecen JUNTOS)
 *     [GLN64P3] tcupd=240ms/s  tcload=477ms/s
 *     [GLN64T] tex=323ms/s (conv=317ms/s)
 *     [PERF] 29vi
 *
 * Son sprites 2D grandes: 2430 KB en 15 texturas = 162 KB de media.  No
 * caben cuatro personajes en 2,4 MB, asi que cada textura se desaloja y se
 * reconvierte dentro del mismo frame.  Eso explica los dos sintomas a la vez:
 *
 *   - la caida a <40 fps  = las 477 ms/s de reconversion
 *   - los personajes NEGROS = `__lwp_heap_allocate` devuelve NULL cuando ya
 *     no queda nada que desalojar, y Textures.cpp:1104-1109 deja
 *     `GXtexture = NULL` y `textureBytes = 0` sin decir nada.  Es el "negro
 *     silencioso" que ya avisaba el comentario de `cache.maxBytes`, pero
 *     alcanzado por el otro lado.
 *
 * COMO SE SUBE SIN TOCAR EL ARBOL COPIADO: `TextureCache_Init` solo monta el
 * monton `if (!GXtexCache)` (Textures.cpp:652).  Montandolo nosotros antes
 * -- y `Config_LoadConfig` corre antes, desde `InitiateGFX` (glN64.cpp:260),
 * mientras que `TextureCache_Init` cuelga de `OGL_Start` (OpenGL.cpp:542)--
 * el plugin se encuentra el suyo hecho y no reserva nada.  Cero divergencia.
 *
 * Se prueba en escalera y se registra cual entro: una reserva contigua de 32
 * MB puede fallar, y si fallara en silencio el monton quedaria sobre un
 * puntero NULL.  `GXtexCache` la DEFINE `Textures.cpp:53` sin `extern "C"`,
 * o sea con enlace C++; aqui solo se declara (ver la nota de gln64_glue.cpp
 * sobre el LNK4006 de duplicarla). */
extern heap_cntrl *GXtexCache;

static unsigned int s_texHeapBytes = 0;

static void gln64_tex_heap_setup(void)
{
	static const unsigned int tam[] = {
		32u * 1024u * 1024u,
		16u * 1024u * 1024u,
		 8u * 1024u * 1024u,
		 4u * 1024u * 1024u
	};
#if XBOX64_TRAZAS
	char ob[144];
#endif
	heap_cntrl *ctrl;
	int i;

	/* One-shot seguro, por la misma razon que el `if(!GXtexCache)` del
	 * plugin: `TextureCache_Destroy` vacia la cache entera entre ROMs y
	 * devuelve cada textura al monton.  Ver gln64_glue.cpp:121-126. */
	if (GXtexCache)
		return;

	/* ★ El descriptor PRIMERO y fuera del bucle, a proposito: `memalign` es
	 * una MACRO a `_aligned_malloc` (gccore.h:243), y un `free()` sobre eso
	 * mezcla asignadores.  Asi la arena nunca hay que deshacerla -- igual
	 * que upstream, que tampoco la libera nunca. */
	ctrl = (heap_cntrl *)malloc(sizeof(heap_cntrl));
	if (ctrl)
	{
		for (i = 0; i < 4; i++)
		{
			void *arena = memalign(32, tam[i]);
			if (!arena)
				continue;

			__lwp_heap_init(ctrl, arena, tam[i], 32);
			/* Solo ahora: si fallaramos, el plugin tiene que seguir viendo
			 * NULL y montar el suyo. */
			GXtexCache     = ctrl;
			s_texHeapBytes = tam[i];

#if XBOX64_TRAZAS
			sprintf(ob, "[GLN64] monton de texturas = %u MB"
			            " (el de la GameCube eran %u KB)",
			        tam[i] / (1024u * 1024u),
			        (unsigned int)GX_TEXTURE_CACHE_SIZE / 1024u);
			DebugOutputString(ob);
#endif
			return;
		}
		free(ctrl);
	}

	/* Sin memoria: se deja que lo monte el plugin con su 2,4 MB.  Sigue
	 * funcionando, solo que con el desalojo en cadena de siempre. */
#if XBOX64_TRAZAS
	sprintf(ob, "[GLN64] monton de texturas: SIN MEMORIA, se queda el del"
	            " plugin (%u KB) -- habra reconversion en cadena",
	        (unsigned int)GX_TEXTURE_CACHE_SIZE / 1024u);
	DebugOutputString(ob);
#endif
}

/* Copia literal de la rama `__GX__` de `Config_linux.cpp:439-450`.  Si algun
 * dia se rebasa contra upstream, esa es la referencia a comparar -- y hay que
 * comparar la LISTA ENTERA, no solo los campos que hoy nos duelen: el fallo
 * de origen fue justamente portar una parte y dar por hecho el resto.
 *
 * ★ DIVERGENCIA DELIBERADA: upstream tiene un guarda `static int loaded` que
 * hace que solo surta efecto la PRIMERA llamada (Config_linux.cpp:409-417).
 * Aqui se omite, y a proposito: en este arbol la unica llamada viva sale de
 * `InitiateGFX` (glN64.cpp:259), o sea una por ROM abierta, y dos de los
 * valores vienen de core options que el usuario puede haber cambiado entre
 * ROM y ROM.  Con el guarda se quedarian congelados los de la primera
 * partida.  Reejecutar es inocuo: son asignaciones de constantes y
 * `TextureCache_Init` las lee despues. */
void Config_LoadConfig()
{
	OGL.fullscreenWidth     = 640;
	OGL.fullscreenHeight    = 480;
	OGL.windowedWidth       = 640;
	OGL.windowedHeight      = 480;
	OGL.forceBilinear       = glN64_use2xSaiTextures;
	OGL.enable2xSaI         = glN64_use2xSaiTextures;
	OGL.fog                 = 1;
	OGL.textureBitDepth     = 1;   /* normal (16 y 32 bits) */
	OGL.frameBufferTextures = glN64_useFrameBufferTextures;
	OGL.usePolygonStipple   = 0;

	/* ★ Tiene que valer lo MISMO que el monton que hay montado.  Subirlo sin
	 * subir el monton solo consigue que las reservas fallen y que
	 * `GXtexture` quede NULL, que es un negro silencioso.  Van juntos o no
	 * van, y por eso el monton se monta AQUI MISMO, en la linea de arriba:
	 * asi no hay forma de que uno cambie sin el otro. */
	gln64_tex_heap_setup();
	cache.maxBytes          = s_texHeapBytes ? s_texHeapBytes
	                                         : GX_TEXTURE_CACHE_SIZE;
}

/* En Wii64 abre el dialogo GTK.  Aqui la configuracion son core options. */
void Config_DoConfig()
{
}

/* ------------------------------------------------------------------ */
/* ★ Mirilla a la cache de texturas del plugin, SIN TOCARLO            */
/* ------------------------------------------------------------------ */
/* `TextureCache_Update` busca recorriendo una lista enlazada desde
 * `cache.top` y comparando 12 campos por nodo (Textures.cpp:2130-2162).  El
 * coste de esa busqueda depende de CUANTOS nodos hay, y ese numero esta en
 * un global del plugin: basta leerlo.
 *
 * Se hace desde aqui porque esta unidad ya incluye `Textures.h` -- el glue
 * no puede, sus cabeceras chocan con `<xtl.h>`-- y asi el arbol copiado se
 * queda intacto.
 *
 * Lectura: con `numCached` nodos, un ACIERTO recorre del orden de la mitad.
 * Multiplicado por las ~13.000 consultas/s que mide `[GLN64T] tex n=`, dice
 * si el recorrido es el coste o si lo es el CRC (`[GLN64H]`). */
extern "C" void gln64_texcache_stats(unsigned int *numCached,
                                     unsigned int *hits,
                                     unsigned int *misses,
                                     unsigned int *bytes,
                                     unsigned int *maxBytes)
{
	if (numCached) *numCached = (unsigned int)cache.numCached;
	if (hits)      *hits      = (unsigned int)cache.hits;
	if (misses)    *misses    = (unsigned int)cache.misses;
	if (bytes)     *bytes     = (unsigned int)cache.cachedBytes;
	/* ★ El TECHO, al lado de lo ocupado.  Sin el no se puede distinguir
	 * "la cache va holgada" de "esta clavada arriba y reconvirtiendo en
	 * cadena", que es lo que costo la escena de los cuatro personajes de
	 * Paper Mario.  Se lee junto a texconv/texfree de la linea [GLN64]. */
	if (maxBytes)  *maxBytes  = (unsigned int)cache.maxBytes;
}
