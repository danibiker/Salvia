/*
 * gln64_prof.h  --  relojes TEMPORALES dentro de glN64_GX.
 *
 * ★ ESTO ES UNA SONDA, NO CODIGO DEFINITIVO.
 *
 * El perfilador ya reparte el tiempo del backend (`[GLN64T]`: geometria,
 * texturas, y el RESTO por diferencia) y el coste del hash (`[GLN64H]`),
 * pero `RESTO` -- el trabajo de CPU del propio plugin-- salio 240 ms/s y no
 * se puede partir desde el shim: esas funciones no pasan por la API GX.
 *
 * Para partirlo hace falta una linea dentro de tres funciones de glN64, y
 * eso rompe la disciplina de "el arbol copiado no se toca".  Por eso:
 *
 *   - `GLN64_PROF` a 0 deja las tres lineas en NADA (macro vacia), asi que
 *     el binario de produccion es identico al de antes.
 *   - las lineas son UNA por funcion, aditivas, y se quitan en cuanto la
 *     medida conteste.
 *   - los contadores viven AQUI, no en el plugin.
 *
 * ★ COBERTURA DE TODO EL FLUJO.  Cada cubo mide **TIEMPO PROPIO**: lo que
 * se gasta dentro de esa funcion SIN contar las funciones instrumentadas
 * que llama (ver `Gln64ProfScope`).  Por eso son disjuntos de verdad y su
 * suma es comparable con `[PERF] gfx`.
 *
 * Los "niveles" de abajo son solo la relacion de llamada, util para leer:
 * NO implican que haya que restar nada, eso ya esta hecho.
 *
 *   NIVEL 1 -- los dos puntos de entrada
 *     dlist    RSP_ProcessDList        la display list entera
 *     viupd    VI_UpdateScreen         la presentacion
 *
 *   NIVEL 2 -- llamados desde dlist
 *     vtx      gSPVertex               el T&L  (la pregunta de VMX128)
 *     addtri   OGL_AddTriangle         UV por triangulo y recortes
 *                                      ★ OJO: llama a OGL_DrawTriangles
 *                                      (:1175, :1298) y a OGL_UpdateStates
 *                                      (:1190).  Por eso hace falta el
 *                                      tiempo propio.
 *     drawtri  OGL_DrawTriangles       emision del lote
 *     rect     OGL_DrawRect + OGL_DrawTexturedRect
 *     ldtile   gDPLoadTile + gDPLoadBlock   copia a TMEM
 *
 *   NIVEL 3 -- llamados desde drawtri/rect/addtri
 *     updst    OGL_UpdateStates
 *     tcupd    TextureCache_Update     CRC + recorrido de lista + activar
 *     comb     Combiner_SetCombineStates
 *
 *   NIVEL 4 -- llamado desde tcupd
 *     tcload   TextureCache_Load       conversion de texels N64 -> GX
 */

#ifndef _XENON_GLN64_PROF_H_
#define _XENON_GLN64_PROF_H_

/* ★ LA PALANCA, LO PRIMERO DEL FICHERO.
 *
 * Estaba mas abajo, detras del bloque `extern "C"` del hilo grafico.  Ahi
 * funcionaba por casualidad -- los `#if GLN64_PROF` de este header quedan por
 * debajo--, pero deja el fichero en un estado en el que cualquier `#if` nuevo
 * escrito por encima valdria 0 en silencio, sin error y sin aviso: los
 * contadores se apagarian solos y el sintoma seria "la sonda no mide nada".
 * Una palanca se declara antes que todo lo que pueda mirarla.
 *
 * ★ Y CUELGA DEL MAESTRO `XBOX64_SONDAS` (source/xenon64_diag.h), para que
 * apagar las sondas sea UN sitio y no una caceria.  Lo caro que mata esto es
 * `GLN64_PROF_CNT(vtxn)`, que esta en `gSPProcessVertex`: un incremento
 * `volatile` POR VERTICE. */
#include "../source/xenon64_diag.h"

/* ★ Cuelga del interruptor maestro, asi que por defecto esta APAGADO y el
 * binario de glN64 es el de siempre.
 *
 * ★★ ESTUVO A 1 UNAS HORAS EL 2026-09-29, y conviene saber por que se
 * volvio atras.  Se encendio para tener el control de la comparacion
 * "por que Rice va peor que glN64": el cubo `vtx` de aqui mide lo mismo
 * que el `pvtx` de `xenon/rice_prof.h` del otro lado.  Se apaga otra vez
 * porque NO es gratis: son 13 ambitos mas un `GLN64_PROF_CNT(vtxn)` que es
 * un incremento `volatile` POR VERTICE, y dejarlo encendido falsea justo lo
 * que mide.
 *
 * Si hay que repetir la comparacion, se enciende, se toman los dos logs de
 * la MISMA escena y se vuelve a apagar -- no se deja puesto. */
#define GLN64_PROF XBOX64_SONDAS

/* ★ Vuelca los once cubos y los pone a cero.  Se llama UNA vez por segundo
 * desde el resumen del wrapper.  La define xenon/gln64_glue.cpp, junto a
 * los contadores, y con GLN64_PROF a 0 el cuerpo queda vacio -- asi que la
 * llamada puede quedarse puesta sin coste. */
#ifdef __cplusplus
extern "C" {
#endif
/* `frames` = cuantos frames cubre la ventana.  Todo se imprime POR FRAME,
 * para poder compararlo con los 16.667 us que dura uno. */
void gln64_prof_informe(unsigned frames);

/* [Xbox360] Aqui se declaraban `gln64_ucode_tipo` y
 * `gln64_ucode_apto_para_hilo`, el filtro de ucode del hilo grafico antiguo.
 * Quitados con el hilo el 2026-09-30.  Con el modelo de mupen la display
 * list se recorre siempre en el hilo del emulador, asi que los ucodes que
 * hacen yield ya no son un caso aparte. */
#ifdef __cplusplus
}
#endif

/* (`GLN64_PROF` se define al principio del fichero.) */

/* ===========================================================================
 * GLN64_WATCHDOG -- convertir el cuelgue en una linea de log
 * ===========================================================================
 * Majora's Mask se queda clavado en su PRIMERA tarea grafica: el log se corta
 * justo detras de `[OSTASK] tipo=1` y no vuelve a salir ni un [PERF] ni un
 * [PACE], o sea que `retro_run` no vuelve.  Pasa con el hilo grafico
 * ENCENDIDO y APAGADO, y con Rice el mismo juego carga, asi que el sospechoso
 * es el bucle de la display list de glN64 (`RSP_ProcessDList`).
 *
 * Esto NO cambia el comportamiento: solo cuenta comandos y, si una sola lista
 * pasa de GLN64_WD_LIMIT, escribe donde estaba y corta el bucle.  Un juego
 * normal no se acerca: las listas gordas andan por decenas de miles.
 *
 * Si la linea [GLN64WD] sale -> el bucle es el sitio, y trae el PC, el
 * comando y el RDRAMSize con el que estaba trabajando.
 * Si NO sale y sigue colgado -> el bucle esta descartado y hay que mirar
 * fuera (el shim, una espera de D3D).  Las dos respuestas sirven. */
#define GLN64_WATCHDOG  1
#define GLN64_WD_LIMIT  2000000u

/* ===========================================================================
 * GLN64_MODIFYVTX_SCREEN -- gSPModifyVertex en coordenadas de pantalla
 * ===========================================================================
 * `gSPModifyVertex` (gSP.cpp) tiene cuatro subcomandos y glN64 implementa
 * DOS: `G_MWO_POINT_RGBA` y `G_MWO_POINT_ST`.  Los otros dos --
 * `G_MWO_POINT_XYSCREEN` y `G_MWO_POINT_ZSCREEN` -- son `case` VACIOS, y el
 * propio upstream los marca `DEBUG_UNHANDLED`.  No es una simplificacion de
 * las que hace Wii64 al portar de GLideN64: es codigo que glN64 nunca
 * termino, y por eso NO sale en AUDITORIA_GLIDEN64.txt (ese mapa cubre lo
 * que Wii64 SI porto).
 *
 * Sirven para que el juego coloque un vertice DIRECTAMENTE en pantalla,
 * saltandose las matrices.  Ignorarlos deja el vertice donde lo puso la
 * transformacion 3D, o sea pegado al objeto.
 *
 * La referencia (GLideN64 src/gSP.cpp:2039-2070) los implementa, y no
 * necesita NADA especifico de ningun juego para ello: en todo GLideN64 la
 * unica mencion a Pilotwings es un comentario de CRC de microcodigo
 * (GBI.cpp:73).
 *
 *   0  como upstream: los dos subcomandos se ignoran.
 *   1  XYSCREEN implementado (ZSCREEN sigue solo contandose).
 *
 * ★ POR QUE SOLO XY DE MOMENTO.  La conversion de XY esta DERIVADA del
 * propio arbol y no supuesta: `OGL_UpdateViewport` monta el viewport con
 * `gSP.viewport.x/width`, y `OGL_DrawRect` -- el camino de pantalla que hoy
 * funciona-- monta su ortho con `guOrtho(m, 0, alto, 0, ancho, 1, -1)`, o
 * sea que NDC +1 es ARRIBA.  Con eso el algebra del viewport se cancela y
 * solo quedan vtrans/vscale.  La de Z NO se puede derivar igual de limpio:
 * depende de si el NDC de profundidad de este backend es [-1,0] o [-1,1], y
 * eso ya se equivoco una vez (la fila z de `guOrtho`, y otra vez el
 * `GXprojZGuard`).  Se hace en una segunda pasada SI el contador dice que
 * hace falta.
 *
 * Los dos contadores salen en la linea [GLN64MV] y se leen ASI:
 *   xy=0 y z=0  -> el juego NO usa este camino.  Hipotesis muerta, y el
 *                  binario se comporta igual que antes.  Una build perdida,
 *                  no una sesion.
 *   xy>0        -> el camino existe y ahora se honra.
 *   xy=0 y z>0  -> hay que hacer la pasada de Z.
 *
 * ★★ RESULTADO EN PILOTWINGS 64 (log 15): la linea [GLN64MV] **NO SALE**, o
 * sea xy=0 y z=0.  El juego NO usa este camino y la hipotesis de la sombra
 * queda MUERTA.  La sonda hizo su trabajo: un build, respuesta cerrada.
 *
 * Por eso el toggle se queda en 0.  El codigo esta escrito y validado contra
 * la referencia, pero NO se ha ejercitado nunca en hardware, asi que dejarlo
 * a 1 seria meter una conversion sin probar en el primer juego que la use,
 * sin saber cual ni cuando.  A 0 el binario es el de upstream exacto y los
 * contadores SIGUEN contando: el dia que [GLN64MV] aparezca en algun juego,
 * se pone a 1 con el dato delante.
 *
 * ★★ ESE DIA HA LLEGADO (Mario Golf, log 13): la "medida" de Pilotwings no
 * valia -- la linea [GLN64MV] ya no se imprimia (ahora va en `[xbox64]
 * cpu->gpu ... modvtx xy=`).  En Mario Golf, con la rejilla de nivelacion,
 * `modvtx xy=12000-22000` por segundo: la rejilla se coloca ENTERA con
 * XYSCREEN y, ignorado, colapsaba en una linea.  A 1, con la conversion
 * corregida para conservar la w (ver gSPModifyVertex). */
#define GLN64_MODIFYVTX_SCREEN  1

/* ===========================================================================
 * GLN64_CI_ES_ZI -- dibujos cuando el color image ES el depth image
 * ===========================================================================
 * SINTOMA: en Pilotwings 64 la sombra del ala delta sale pegada al ala en vez
 * de proyectada en el suelo, y con el jetpack se estira en una tira desde el
 * suelo hasta el personaje.
 *
 * ★ LA PISTA VIENE DE RICE, QUE CORRE EN ESTA MISMA CONSOLA.
 * `Rice_GX_Xenos/Render.cpp:1409` tiene un hack por juego para Pilotwings:
 *
 *     if (IsUsedAsDI(g_CI.dwAddr) && z_cmp + z_upd > 0) {
 *         TRACE0("Warning: using Flushtris to write Zbuffer");
 *         gRSP.numVertices = 0; ... return true;     // DESCARTA el dibujo
 *     }
 *
 * o sea que el juego DIBUJA TRIANGULOS CON EL COLOR IMAGE PUESTO EN EL DEPTH
 * IMAGE, y Rice los tira.  Y esta MEDIDO por el usuario que en Rice la sombra
 * no se ve: cuadra con que los dibujos descartados sean justo esos.
 *
 * Nosotros no hacemos ninguna de las dos cosas, asi que esa geometria se
 * pinta en el EFB visible.  Eso explicaria los dos sintomas a la vez: lo que
 * se ve no es una sombra mal colocada, es la GEOMETRIA que el juego queria
 * mandar al buffer de profundidad, pintada como color y por tanto en la
 * posicion del objeto.
 *
 * La referencia lo resuelve GENERICO, sin nombrar a ningun juego:
 * `isCurrentColorImageDepthImage()` (GLideN64 src/gDP.cpp:33) y, cuando es
 * cierto, el shader escribe gl_FragDepth en vez de color
 * (src/GraphicsDrawer.cpp:794-800).
 *
 *   0  contar nada mas: el binario se comporta EXACTAMENTE como hoy.
 *   1  no dibujar esa geometria = lo que hace Rice.  Si el bicho desaparece,
 *      el mecanismo queda CONFIRMADO y estamos a la par de Rice (sin sombra).
 *
 * ★★ PRIMER INTENTO, FALLIDO Y POR MI CULPA (log 16): la sonda pregunto
 * `colorImage.address == depthImageAddress` y salio CERO.  El test estaba
 * mal planteado, no el mecanismo: el de Rice NO es una igualdad simultanea
 * sino HISTORICO -- las dos ultimas direcciones de depth image, con ventana
 * de 10 display lists (`IsUsedAsDI`, RSP_Parser.cpp:2180).  Y la diferencia
 * cae justo en el patron que se busca: el juego apunta el color image a una
 * direccion que FUE el depth image, dibuja, y lo devuelve.  Ahora la sonda
 * usa `gln64_ci_es_zi()`, que es el port del test de Rice.
 *
 * Por eso vuelve a 0: la medida de antes no descarto nada, asi que toca
 * medir otra vez ANTES de volver a descartar geometria.
 *
 * ★ RIESGO ASUMIDO, y hay que mirarlo: la condicion es GENERICA, no por
 * juego (Rice lo hace solo para Pilotwings, y ademas exige
 * `z_cmp + z_upd > 0`).  Si algun otro juego dibuja legitimamente con el
 * color image puesto en el depth image, aqui perderia esa geometria.  La
 * linea [GLN64ZI] sale en TODOS los juegos, asi que basta mirar si aparece
 * donde no deberia; si molesta, el siguiente paso NO es volver a 0 sino
 * estrechar con la condicion de Rice.
 *
 * ★ Confirmar el mecanismo es el paso 1; que la sombra salga BIEN es el paso
 * 2 y es otro trabajo -- portar la escritura de profundidad de GLideN64
 * (src/GraphicsDrawer.cpp:794-800).  No se mezclan: primero el dato. */
#define GLN64_CI_ES_ZI  0

/* ===========================================================================
 * ★★ GLN64_RESET_RDP_AL_CARGAR -- estado del RDP que sobrevive al cambio de ROM
 * ===========================================================================
 * SINTOMA (medido por el usuario): el marcador POWER de Super Mario 64 sale
 * bien en un arranque limpio, pero si ANTES se ha cargado otro juego, sale
 * mal.  O sea que algo del plugin sobrevive al cambio de ROM.
 *
 * `RSP_Init()` (RSP.cpp) se llama en cada `RomOpen` y reinicia bastante --
 * `gSP` entero por memset, `gDP.otherMode`, `gDP.loadTile`,
 * `gSP.textureTile[]`, los hacks por juego, DepthBuffer, GBI y, via
 * `OGL_Start()`, la cache de texturas.
 *
 * Pero NO toca tres cosas, y las tres son justo las que deciden el tamano de
 * la textura de un texrect -- que es lo que es el marcador POWER:
 *
 *     gDP.tiles[]      los 8 descriptores de tile
 *     gDP.loadInfo[]   que puso cada carga en cada direccion de TMEM
 *     TMEM[]           la memoria de texturas
 *
 * `gDP.loadInfo[]` es el que mas cuadra: `TextureCache_Update` decide por
 * `loadedInfo.loadType == LOADTYPE_TILE` si usa la informacion de la carga o
 * se va por otra rama, y eso solo se aplica a TEXRECTS.  Con datos del juego
 * anterior, el marcador se dimensiona con lo que dejo la ROM de antes.
 *
 * ★ HONESTIDAD SOBRE LA REFERENCIA: esto NO es "portar algo que GLideN64
 * tiene y nosotros no".  GLideN64 tampoco reinicia gDP, asi que el hueco es
 * de upstream y lo hereda Wii64.  Es la misma clase de fallo de RECARGA DE
 * JUEGO que ya ha aparecido en atari800, px68k, Frodo, prboom y nestopia.
 *
 * Lo que SI viene de upstream es la ultima linea: `RSP.cpp:222` hace
 * `gSP.changed = gDP.changed = 0xFFFFFFFF` despues de reinicializar
 * FrameBuffer/TextureCache/Combiner... pero dentro del bloque `RSPTHREAD`,
 * que este build NO compila (RomOpen coge la rama `#else`).  O sea que esa
 * linea es codigo de la referencia que aqui no se ejecuta nunca.
 *
 * ★★ MEDIDO: NO ARREGLA NADA.  Se queda a 0.  Ver el punto P de
 * DIVERGENCIAS.txt y la nota de GLN64_CAZA_MARIO64 aqui abajo. */
#define GLN64_RESET_RDP_AL_CARGAR  0

/* ===========================================================================
 * GLN64_CAZA_MARIO64 -- la instrumentacion de los quesitos del marcador POWER
 * ===========================================================================
 * Defecto ABIERTO: en Super Mario 64 los quesitos del medidor POWER se ven si
 * el juego se carga EL PRIMERO y NO se ven si antes se cargo otro.  El aro si
 * se ve en los dos casos.
 *
 * A 0 (ahora) todas estas sondas compilan fuera: ni llamadas en los caminos
 * calientes ni lineas en el log, que si no ensucian la caza de Pilotwings.  A
 * 1 vuelven enteras.
 *
 * ★ LO QUE YA ESTA DESCARTADO CON DATOS, para no repetirlo:
 *
 *   gSPModifyVertex en pantalla     [GLN64MV] no sale nunca
 *   dibujar al z-buffer             [GLN64ZI] a cero, con el test de Rice
 *   comandos sin implementar        [GLN64GBI] vacio
 *   recorte por w<=0                10 en toda una sesion
 *   desplazamiento de tile (pto K)  =DESPL nunca; lod y det son 0 SIEMPRE
 *   cadena de mips (pto M)          =MIP nunca
 *   needReplaceTex1ByTex0           rep0 siempre
 *   scissor degenerado              degen=18 tambien cuando VA BIEN
 *   fuga del monton de texturas     mismo `usado` tras cada cierre, FALLOS=0
 *   reserva de textura fallida      FALLOS=0 en 124 muestras
 *   246 texturas convertidas        IGUAL en las dos cargas
 *   reset de tiles/loadInfo/TMEM    probado, no cambia nada
 *   cache de shaders rancia         vaciada, genera los mismos 20: no era
 *
 * ★★ Y DOS COSAS QUE HAY QUE SABER ANTES DE SEGUIR:
 *
 *  1. Los puntos K y M NO PUEDEN haber arreglado esto: su codigo no se
 *     ejecuta ni una vez en Mario 64 (lod/det valen 0 siempre).  La
 *     atribucion de aquella sesion es incorrecta y hay que rehacerla.
 *
 *  2. "El censo de modos es identico entre las dos cargas" NO prueba que se
 *     dibujen los quesitos.  El censo AGREGA por modo de render, y los
 *     quesitos son del orden de 8-16 triangulos sobre 900.000: esa
 *     diferencia no se ve ahi.  La pregunta "se emite ese dibujo?" SIGUE SIN
 *     CONTESTAR, y es por donde hay que empezar.
 *
 * Siguiente paso concreto cuando se retome: censar la IDENTIDAD de las
 * texturas creadas (crc, formato, tamano) -- son 246 por sesion-- y
 * diferenciar las dos cargas.  Si los conjuntos son iguales, el problema
 * esta en la emision del dibujo y no en la textura. */
#define GLN64_CAZA_MARIO64 0

/* Las funciones que el arbol copiado llama y que implementamos nosotros.  Se
 * declaran aqui porque `gccore.h` arrastra este fichero, y asi glN64 no gana
 * ni un `#include`.
 *
 * ★ Las SONDAS del framebuffer ([CIMG-GX], [FBTEX-GX], [FBLOAD-GX],
 * [FBRECH-GX], [FBACT-GX], [FBRECT-GX]) y sus toggles se RETIRARON el
 * 2026-09-22, al cerrar el caso de Pokemon Stadium.  Ver la nota en
 * xenon/gln64_config.cpp. */
#ifdef __cplusplus
extern "C" {
#endif
/* ★ NO es una sonda: ajusta OGL.GXscaleX/Y para que el color image ACTUAL
 * quepa en el EFB.  Sin esto, un color image mas ancho que el VI se dibuja a
 * medias y se muestrea estirado -- los cuadros de texto de Pokemon Stadium.
 * El porque completo esta en gln64_config.cpp. */
void gln64_cimg_escala(unsigned int width);

/* El ancho/alto sobre el que se monta la ortho de los rects: el mayor entre
 * el VI y el color image activo.  Ver gln64_config.cpp. */
float gln64_rect_ancho(void);
float gln64_rect_alto(void);


/* Sonda: una activacion de framebuffer-como-textura.  Comprueba si la imagen
 * esta realmente resuelta; si no, ese dibujo saldra negro o con basura.  Ver
 * gxd3d_dbg_fbmiss en xenon/gx_d3d9.cpp. */
void gxd3d_fb_activa(void *gxtex);

/* ★ [FILL-GX]: sonda TEMPORAL de `gDPFillRectangle`.
 *
 * RESULTADO EN KILLER INSTINCT GOLD: **CERO LINEAS**.  El juego no llama a
 * `gDPFillRectangle` ni una vez, asi que no limpia por relleno y todo el
 * camino del punto L no tiene nada que ver con su estela.
 *
 * ★★ Y DE PASO DESHACE UN ERROR MIO DE ATRIBUCION.  Dije que `clrz` habia
 * pasado de 8669 a 0 "por el cambio del punto L".  FALSO: los 8669 salen de
 * una sesion LARGA de juego (2M draws) y los dos logs siguientes son la
 * misma escena corta de intro (58k draws, contadores identicos salvo el
 * ruido de `compus`).  Compare dos logs que no eran comparables.  Y ademas
 * `clrz` de esa sesion no podia venir de `gDPFillRectangle`, que aqui no se
 * llama: viene de `gDPSetDepthImage`.
 *
 * `rama`: 1 = borrado de Z, 2 = borrado de color, 3 = rect normal. */
void gln64_fill_nota(int ulx, int uly, int lrx, int lry,
                     unsigned int cycleType, unsigned int fillColor,
                     unsigned int colorAddr, unsigned int depthAddr,
                     unsigned int rama);

/* ★ [CIMG-GX]: los color image DISTINTOS que fija el juego.  Estaba y la
 * retire al cerrar Pokemon Stadium; vuelve porque la pregunta que contesta
 * es exactamente la que queda abierta en Killer Instinct.
 *
 * Con la estela, un solo render target y `gDPFillRectangle` sin usar, el
 * sospechoso natural es que el juego alterne DOS O MAS buffers en RDRAM y
 * nosotros los metamos en el mismo EFB: cada frame dibuja sobre lo que dejo
 * el anterior.  Es el mismo patron que el parpadeo de Paper Mario, donde
 * salieron TRES color image contiguos.
 *
 * Si sale UNO solo, el doble buffer queda descartado y la estela es que
 * nadie repinta el fondo. */
void gln64_cimg_nota(unsigned int addr, unsigned int width, unsigned int size);

/* [Pilotwings, log 2] Vacia las listas de "ya visto" de [CIMG-GX],
 * [REST-GX], [RDRAM-GX] y [FILL-GX].  La llama RSP_Init al cargar cada ROM:
 * antes duraban la sesion y un juego heredaba los topes gastados del
 * anterior.  Ver gln64_config.cpp. */
void gln64_sondas_reinicia(void);

/* ★ [REST-GX]: la pieza que de verdad decide la estela.
 *
 * MEDIDO en Killer Instinct Gold: DOS color image, 0036A000 y 002D4000,
 * separados por 0x96000 = 640*480*2 EXACTO.  Doble buffer de libro, a
 * 640x480, y `gDPFillRectangle` sin usar ni una vez (`[FILL-GX]` = 0).
 *
 * En la N64 cada buffer tiene su propio contenido.  Aqui hay UN SOLO EFB,
 * asi que al cambiar de color image hay que EMULAR el cambio: repintar en
 * el EFB lo que ese buffer tenia.  De eso se encarga
 * `FrameBuffer_RestoreBuffer`... si encuentra el buffer.
 *
 * Si NO lo encuentra, no hace nada y el EFB se queda con lo del buffer
 * anterior: se dibuja encima, se guarda, se vuelve a dibujar encima.  El
 * contenido se ACUMULA monotonamente, que es exactamente una estela que no
 * para de crecer.
 *
 * `encontrado` = 0 en las lineas que importan seria la confirmacion. */
void gln64_restore_nota(unsigned int addr, unsigned int width,
                        unsigned int size, int encontrado);

/* ★★ [RDRAM-GX]: que hay en RDRAM en el buffer al que se cambia.  Es la
 * ultima pregunta abierta de la estela; ver la nota en gln64_config.cpp. */
void gln64_rdram_nota(unsigned int addr, unsigned int width,
                      unsigned int height, unsigned int size);

/* Contadores de `gSPModifyVertex` en coordenadas de pantalla.  Ver la nota
 * larga de GLN64_MODIFYVTX_SCREEN arriba: son la medida que decide si la
 * hipotesis de la sombra de Pilotwings es siquiera alcanzable. */
extern volatile unsigned int gln64_dbg_modvtx_xy;
extern volatile unsigned int gln64_dbg_modvtx_z;

/* Buffers nuevos cargados desde la RDRAM (GLideN64 8b359761); ver
 * gln64_config.cpp. */
extern volatile unsigned int gln64_dbg_fbrdram;
extern volatile unsigned int gln64_dbg_fbrdram_px;

/* Geometria emitida con el color image APUNTANDO AL DEPTH IMAGE.  Ver la
 * nota de GLN64_CI_ES_ZI arriba: contesta si Pilotwings usa de verdad el
 * camino que Rice descarta con su hack por juego. */
extern volatile unsigned int gln64_dbg_cizi_tri;
extern volatile unsigned int gln64_dbg_cizi_rect;

/* ★★ Triangulos tirados por w <= 0 (vertice detras del ojo) y triangulos
 * emitidos por el camino de la matriz combinada de W.  Dependen de la
 * CAMARA, que es lo que separa el replay de Pilotwings (sombra bien) de la
 * partida (sombra mal).  Ver gln64_config.cpp. */
extern volatile unsigned int gln64_dbg_wcull;
extern volatile unsigned int gln64_dbg_combw;
extern volatile unsigned int gln64_dbg_nocombw;

/* ===========================================================================
 * MARCADO DE SHADERS (opcion de core `xbox64_marcatex`)
 * ===========================================================================
 * Sustituye el pixel shader por uno de diagnostico, o corta el real por una
 * etapa, para ver DONDE se pierde el color.  Todo vive en xenon/gx_d3d9_tev.cpp
 * salvo esta palanca: ni un gancho dentro del arbol copiado de Wii64. */
#define GLN64_MARCA_NO        0
#define GLN64_MARCA_MUX       2   /* un color plano por combinador            */
#define GLN64_MARCA_SHADE     3   /* pinta SHADE (el color de vertice) tal cual*/
#define GLN64_MARCA_TEX       4   /* pinta tex2D(uTex0, uv0) tal cual          */
#define GLN64_MARCA_SINCLIP   5   /* el shader REAL sin el descarte de alfa    */
/* Corte por etapas del shader REAL: lo ejecuta de verdad y lo para en la etapa
 * N, devolviendo el registro DESTINO de esa etapa.  El primer corte que salga
 * mal senala la etapa culpable.  `ETAPA0` da el preambulo del shader generado
 * devolviendo solo el color de vertice, para separar preambulo de etapas. */
#define GLN64_MARCA_ETAPA1    8
#define GLN64_MARCA_ETAPA5   12
#define GLN64_MARCA_SINNIEBLA 13  /* el shader REAL sin la niebla             */
#define GLN64_MARCA_ETAPA0   14
#define GLN64_MARCA_NIEBLA   15   /* pinta el factor de niebla en gris        */
#define GLN64_MARCA_PROF     16   /* pinta z/w en tres escalas a la vez       */
/* [Kirby 64] Dibuja NORMAL pero sin test de Z, sin culling y con la z del
 * vertex shader sujeta a [0, w] (sin recorte por los planos cercano/lejano).
 * Es un AISLADOR: si un objeto invisible aparece asi, lo perdia la
 * profundidad o el culling; si no, no se esta emitiendo.  Lo aplica el shim
 * (xenon/gx_d3d9.cpp: GX_SetZMode, GX_SetCullMode, GX_SetViewport). */
#define GLN64_MARCA_SINZ     17
/* [Doom 64, Conker] Dibuja NORMAL, pero cada shader de TEV que se genera sale
 * al log ([TEVHLSL]) con el combinador del N64 que lo origino y los valores
 * de sus constantes.  Va por la clave (centinela 0xFC en `corte`), asi que
 * al encenderlo TODOS los combinadores en uso se vuelven a generar y se
 * vuelcan, sin vaciar la cache.  Ver gxd3d_build_key en gx_d3d9_tev.cpp. */
#define GLN64_MARCA_VOLCADO  19
/* [Killer Instinct, cielo de los combates] Los triangulos de bajo nivel del RDP
 * (gDPLLETriangle, ordenes 0x08-0x0F de RDP_ProcessRDPList) se pintan de MAGENTA
 * plano, sin textura: asi se ve que zona cubren.  Lo aplica
 * OGL_DrawScreenSpaceTriangles (source/glN64_GX/OpenGL.cpp). */
#define GLN64_MARCA_LLE      20
/* [Mario Tennis, bob-ombs y estela] Pinta el ALFA de la textura de la unidad 0
 * en gris opaco (negro = 0, blanco = 1).  Ver gxd3d_build_texalfa en
 * xenon/gx_d3d9_tev.cpp. */
#define GLN64_MARCA_TEXALFA  21

extern volatile int          gln64_marcaluz;   /* la palanca (opcion de core) */
/* Opcion `xbox64_fb_bilineal`: 0 auto, 1 activado, 2 desactivado.  Ver
 * xenon/gln64_config.cpp y Textures.cpp (TextureCache_ActivateTexture). */
extern volatile int          gln64_fb_bilineal;

/* `cwmax`: el mayor |GXcombW[2][3]| del segundo.  `cwrej`: cuantas veces la
 * prueba de validez de `_gSPUpdateCombW` rechazo la matriz.  Ver la nota
 * larga en gln64_config.cpp. */

/* `IsUsedAsDI` de Rice, portado: historial de las dos ultimas direcciones de
 * depth image.  `gln64_zi_nota` lo alimenta desde gDPSetDepthImage y
 * `gln64_ci_es_zi` hace la pregunta.  gln64_config.cpp explica por que la
 * igualdad instantanea NO vale. */
void gln64_zi_nota(unsigned int addr, unsigned int dlist);
int  gln64_ci_es_zi(unsigned int ciAddr);
void gln64_zi_get(unsigned int *a0, unsigned int *a1);

/* Censo de comandos de display list sin implementar.  `GBI_Unknown` lo
 * alimenta; se lee por opcode. */
void         gln64_gbi_unknown(unsigned int op);
unsigned int gln64_gbi_unknown_get(unsigned int op);

/* ★ Censo de modos de render (`gDP.otherMode.l >> 16`, lo que fija
 * gDPSetRenderMode) con cuenta de triangulos y de rects.  Es la herramienta
 * para identificar un dibujo que APARECE en un momento dado: se imprime
 * acumulado cada segundo y el culpable es el que pasa de 0 a crecer.  La
 * explicacion larga esta en gln64_config.cpp. */
/* ★ Censo de la puerta del mip de `TextureCache_Update` (puntos K y M).
 * OJO: esa puerta depende de `otherMode.H` (textureLOD / textureDetail) y el
 * censo de modos de render mira `otherMode.l`, asi que NO se solapan.  Ver
 * la nota larga en gln64_config.cpp. */
void         gln64_tex_nota(unsigned int lod, unsigned int detail,
                            unsigned int level, unsigned int tile,
                            unsigned int unidad);
unsigned int gln64_tex_num(void);
unsigned int gln64_tex_ovf(void);
void         gln64_tex_get(unsigned int i, unsigned int *lod,
                           unsigned int *detail, unsigned int *level,
                           unsigned int *tile, unsigned int *unidad,
                           unsigned int *n);

/* (`gxd3d_tev_flush_cache` se declara en xenon/gfx_plugin_sel.h, que es el
 * que ve el core: la llama `xenon64_rom_close`.) */

/* ★★ Estado del monton de texturas (`GXtexCache`): espacio libre, usado,
 * bloques libres, y reservas TOTALES y FALLIDAS.  Una reserva que no cabe
 * deja una textura sin texels -- invisible--, que es el precedente de los
 * personajes negros de Paper Mario.  Ver gln64_config.cpp. */
void gln64_heap_info(unsigned int *libre, unsigned int *usado,
                     unsigned int *bloquesLibres,
                     unsigned int *fallos, unsigned int *reservas);

/* ★ Gemela de la anterior pero en el sitio del punto K
 * (`gSPUpdateTextureTiles`), que es OTRO instante.  Ver gln64_config.cpp. */
void         gln64_utt_nota(unsigned int lod, unsigned int detail,
                            unsigned int level, unsigned int tile,
                            unsigned int rep);
unsigned int gln64_utt_num(void);
unsigned int gln64_utt_ovf(void);
void         gln64_utt_get(unsigned int i, unsigned int *lod,
                           unsigned int *detail, unsigned int *level,
                           unsigned int *tile, unsigned int *rep,
                           unsigned int *n);

void         gln64_rm_nota(unsigned int rm, unsigned int flags,
                           int esRect, float y, float w);
unsigned int gln64_rm_num(void);
unsigned int gln64_rm_ovf(void);
/* OJO: `gln64_rm_get` REINICIA la caja y/w que devuelve. */
void         gln64_rm_get(unsigned int i, unsigned int *rm, unsigned int *flags,
                          unsigned int *tri, unsigned int *rect,
                          float *y0, float *y1);

/* Reinicia el maximo de coordenadas al cambiar de color image.  Lo llama
 * `gln64_cimg_escala`; ver GX_Position3f32 en xenon/gx_d3d9.cpp. */
void gxd3d_ci_actual(unsigned int width);
extern volatile float        gxd3d_ci_maxx;
extern volatile float        gxd3d_ci_maxy;
extern volatile unsigned int gxd3d_ci_w;

/* ★ GLN64_CLEAR_D3D: el borrado de pantalla por el Clear() nativo de D3D en
 * vez de por el quad de glN64.  NO es una sonda: es el arreglo, comprobado en
 * GoldenEye, Paper Mario, Zelda OoT y Pokemon Stadium.  El porque y la lista
 * de lo que se descarto con numeros estan en gxd3d_clear_efb
 * (xenon/gx_d3d9.cpp).  A 0 vuelve el camino del quad, para el A/B. */
#define GLN64_CLEAR_D3D 1
void gxd3d_clear_efb(int color, int depth,
                     unsigned char r, unsigned char g, unsigned char b);
#ifdef __cplusplus
}
#endif

#if GLN64_WATCHDOG
#ifdef __cplusplus
extern "C" {
#endif
extern unsigned int gln64_wd_n;
/* Devuelve 1 cuando se ha pasado del tope (y lo anota una sola vez). */
int gln64_wd_trip(unsigned int pc, unsigned int cmd);
#ifdef __cplusplus
}
#endif

#define GLN64_WD_RESET()       (gln64_wd_n = 0)
#define GLN64_WD_TICK(pc,cmd)  ((++gln64_wd_n > GLN64_WD_LIMIT) \
                                ? gln64_wd_trip((unsigned int)(pc), (unsigned int)(cmd)) : 0)
#else
#define GLN64_WD_RESET()       ((void)0)
#define GLN64_WD_TICK(pc,cmd)  (0)
#endif

#if GLN64_PROF

#include <ppcintrinsics.h>

#ifdef __cplusplus
extern "C" {
#endif
extern volatile unsigned int gln64p_dlist_tb,   gln64p_dlist_n;
extern volatile unsigned int gln64p_viupd_tb,   gln64p_viupd_n;
extern volatile unsigned int gln64p_vtx_tb,     gln64p_vtx_n;
extern volatile unsigned int gln64p_addtri_tb,  gln64p_addtri_n;
extern volatile unsigned int gln64p_drawtri_tb, gln64p_drawtri_n;
extern volatile unsigned int gln64p_rect_tb,    gln64p_rect_n;
extern volatile unsigned int gln64p_ldtile_tb,  gln64p_ldtile_n;
extern volatile unsigned int gln64p_updst_tb,   gln64p_updst_n;
extern volatile unsigned int gln64p_tcupd_tb,   gln64p_tcupd_n;
extern volatile unsigned int gln64p_comb_tb,    gln64p_comb_n;
extern volatile unsigned int gln64p_tcload_tb,  gln64p_tcload_n;
/* ★ EL CORTE QUE FALTABA DENTRO DE `tcupd`.
 *
 * `tcupd` mezcla DOS trabajos que se arreglan de formas opuestas:
 *   - calcular el CRC de la textura (TextureCache_CalculateCRC)
 *   - RECORRER la lista de la cache comparando crc + 12 campos
 * Con `tccrc` instrumentado, el tiempo PROPIO de `tcupd` pasa a ser el
 * recorrido y lo demas, asi que la resta la hace el perfilador.
 *
 * `crcbytes` es la pieza que lo cierra: con bytes y tiempo sale MB/s, y
 * eso distingue "xxHash va lento" de "se le estan dando demasiados
 * bytes" -- que son arreglos distintos. */
extern volatile unsigned int gln64p_tccrc_tb,   gln64p_tccrc_n;
extern volatile unsigned int gln64p_crcbytes;
/* ★ Consultas de textura POR RAMA DE PALETA, en `TextureCache_CalculateCRC`.
 * Contestan una pregunta concreta: la rama de `G_IM_SIZ_16b` es de
 * glN64/Wii64 y GLideN64 NO la tiene (solo 4b y 8b).  Si `pal16` sale a cero
 * se puede quitar; si sale alta, hay que estudiarla antes de tocarla
 * ([[reference_wii64_sobra_vs_gliden64]]). */
extern volatile unsigned int gln64p_pal4, gln64p_pal8, gln64p_pal16;
/* ★ Invocaciones REALES de XXH32 dentro de `TextureCache_CalculateCRC`, y
 * cuantas consultas se fueron por el camino de UNA LLAMADA POR FILA.
 *
 * Es lo que convierte "deduzco que el coste es por llamada" en una medida:
 * con `crcbytes / crccalls` sale el tamano medio del lote que se hashea, y
 * `crclento` dice cuantas texturas no pudieron colapsarse en una sola
 * llamada (filas no contiguas o vuelta de la TMEM).  Si `crclento` es alto,
 * la condicion del camino rapido es demasiado estrecha y hay que mirarla. */
extern volatile unsigned int gln64p_crccalls, gln64p_crclento;
/* ★ Vertices que pasan por `gSPProcessVertex` (transformacion 4x4 +
 * iluminacion + niebla + texgen).  `vtx` son 1,91 ms del frame que dibuja,
 * pero sin el numero de vertices no se sabe si el coste POR VERTICE es
 * aritmetica (~100 ciclos: vectorizar no arreglaria nada) o acarreo de
 * memoria (miles: el problema seria el formato de los datos).  Es el dato
 * que decide si VMX128 merece la pena. */
extern volatile unsigned int gln64p_vtxn;
/* ★★ PASO 0 DEL HILO GRAFICO: ¿SE PUEDE AFIRMAR EL `DP_INT` AL DESPACHAR?
 *
 * Para sacar la display list a otro nucleo sin tocar el temporizado hay que
 * levantar `MI_INTR_DP` en el momento del despacho, ANTES de saber si la
 * lista va a terminar.  Es una afirmacion anticipada, y estas cinco cuentas
 * dicen si alguna vez se cobra.
 *
 *   fsync      veces que se alcanzo `gDPFullSync` (gDP.cpp), que es quien
 *              levanta el DP de verdad
 *   salok      listas que salieron por el final normal
 *   salloop    salidas por `RSP.infloop` -- yield.  ★ Estas NO son solo una
 *              mentira sobre el DP: el plugin ademas BORRA
 *              TASKDONE|HALT|BROKE para que el nucleo reencole la tarea
 *              (RSP.cpp), y con hilo ese reencolado se perderia.
 *              Gauntlet Legends, World Driver Championship, Stunt Racer 64.
 *   saldir     salidas por direccion fuera de RDRAM
 *   salwd      salidas por el perro guardian (GLN64_WATCHDOG)
 *
 * LECTURA: si `fsync == salok == dlist` y las otras tres son 0 en los juegos
 * objetivo, la afirmacion anticipada no es una mentira y se puede escribir
 * el hilo.  Cualquier otra cosa obliga al filtro previo por ucode mas el
 * pestillo que vuelve a sincrono. */

extern volatile unsigned int gln64p_fsync;
extern volatile unsigned int gln64p_salok, gln64p_salloop;
extern volatile unsigned int gln64p_saldir, gln64p_salwd;
#ifdef __cplusplus
}
#endif

/* Guarda de ambito: las funciones instrumentadas tienen varios `return`, asi
 * que un par begin/end a mano dejaria caminos sin cerrar.
 * Solo C++: en C la macro queda en nada (ningun .c del arbol se instrumenta). */
#ifdef __cplusplus
/* ★★ TIEMPO PROPIO, NO ACUMULADO.  Y NO ES UN REFINAMIENTO: SIN ESTO LA
 * MEDIDA MIENTE.
 *
 * La primera version sumaba el tiempo total de cada funcion, con una nota
 * que afirmaba que los cubos del nivel 2 eran disjuntos.  **Era falso**:
 * `OGL_AddTriangle` llama a `OGL_DrawTriangles` (OpenGL.cpp:1175 y :1298) y
 * a `OGL_UpdateStates` (:1190).  Resultado: `addtri` salia 143 ms/s y
 * parecia el mayor coste del plugin, cuando casi todo era el tiempo de sus
 * HIJOS contado otra vez.
 *
 * Ahora cada ambito descuenta lo que gastaron los ambitos anidados dentro
 * de el y le pasa su total al padre.  Asi cada cubo es "tiempo DENTRO de
 * esta funcion sin contar las instrumentadas que llama", los cubos vuelven
 * a ser disjuntos de verdad y su suma es comparable con `[PERF] gfx`.
 *
 * Ojo: `hash` (xxhash.c) NO pasa por aqui, asi que sigue estando DENTRO del
 * tiempo propio de `tcupd`; es a proposito, para poder partirlo. */
extern volatile unsigned int gln64p_child_tb;

struct Gln64ProfScope
{
    volatile unsigned int *tb;
    volatile unsigned int *n;
    unsigned long long     t0;
    unsigned int           savedChild;
    Gln64ProfScope(volatile unsigned int *tb_, volatile unsigned int *n_)
        : tb(tb_), n(n_), t0(__mftb()), savedChild(gln64p_child_tb)
    {
        gln64p_child_tb = 0;
    }
    ~Gln64ProfScope()
    {
        unsigned int total = (unsigned int)(__mftb() - t0);
        unsigned int hijos = gln64p_child_tb;
        *tb += (total > hijos) ? (total - hijos) : 0;   /* propio */
        (*n)++;
        gln64p_child_tb = savedChild + total;           /* al padre, el total */
    }
};

#define GLN64_PROF_SCOPE(bucket) \
    Gln64ProfScope _gln64prof_(&gln64p_##bucket##_tb, &gln64p_##bucket##_n)
/* Contador de bytes, para poder sacar MB/s de un cubo. */
#define GLN64_PROF_BYTES(cont, n)  (gln64p_##cont += (unsigned int)(n))
/* Contador de eventos a secas.  ★ Va definido en LAS TRES ramas: olvidar una
 * de las de "no-op" rompe el build con GLN64_PROF a 0, y eso ya paso una vez
 * con GLN64_PROF_BYTES. */
#define GLN64_PROF_CNT(cont)       (gln64p_##cont += 1u)
#else
#define GLN64_PROF_SCOPE(bucket)  ((void)0)
#define GLN64_PROF_BYTES(cont, n) ((void)0)
#define GLN64_PROF_CNT(cont)      ((void)0)
#endif /* __cplusplus */

#else
#define GLN64_PROF_SCOPE(bucket)  ((void)0)
#define GLN64_PROF_BYTES(cont, n) ((void)0)
#define GLN64_PROF_CNT(cont)      ((void)0)
#endif /* GLN64_PROF */

#endif /* _XENON_GLN64_PROF_H_ */
