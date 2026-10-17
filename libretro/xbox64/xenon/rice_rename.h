/*
 * rice_rename.h  --  prefija los simbolos de Rice_GX que chocan con glN64_GX.
 *
 * ★★ POR QUE EXISTE ESTE FICHERO, Y POR QUE NO SE TOCA EL ARBOL COPIADO.
 *
 * Wii64 NO tiene este problema: compila `Makefile.glN64_wii` o
 * `Makefile.Rice_wii`, dos binarios distintos, y los dos plugins no se ven
 * nunca.  Aqui los queremos LOS DOS en el mismo core con una opcion en el
 * menu, asi que sus simbolos comunes chocarian al enlazar.
 *
 * La salida es renombrar SIN EDITAR NI UNA LINEA de `source/Rice_GX/`: esta
 * cabecera se cuela por delante de cada .cpp de Rice y el preprocesador
 * reescribe los nombres.  Los 140 ficheros siguen siendo byte a byte los de
 * Wii64 (verificado con SHA-1 al copiarlos), que es lo unico que permite
 * traer codigo nuevo de ese proyecto sin resolver conflictos a mano.
 *
 * ★★ COMO ENTRA, Y POR QUE NO POR <ForcedIncludeFiles> POR FICHERO.
 *
 * Lo incluye `xbox/xbox64_pre.h`, que ya es la cabecera forzada de TODAS las
 * unidades, discriminando por `XBOX64_PLUGIN_RICE` -- una macro que el
 * vcxproj pone solo en los .cpp de Rice, al lado de `__GX__;__LINUX__`.
 *
 * La alternativa era un `<ForcedIncludeFiles>` por fichero, y se descarto por
 * una razon practica: habria que anyadirlo tambien a las 38 lineas de glN64
 * (que necesitan su gemelo `gln64_rename.h`), repitiendo en cada una la ruta
 * de `xbox64_pre.h` -- porque la metadata por fichero SUSTITUYE a la del
 * ItemDefinitionGroup, no se suma.  38 ediciones mecanicas en las que una
 * ruta mal escrita cuesta un build entero, contra un solo `#if` aqui.
 *
 * ★ EL ORDEN IMPORTA y sale gratis: `xbox64_pre.h` ya ha metido <xtl.h>,
 * <d3dx9.h> y la CRT ANTES de llegar a este bloque, asi que ninguno de estos
 * #define puede envenenar una cabecera de Microsoft.  Es la misma regla que
 * ese fichero le impone al resto del arbol.
 *
 * ★★★ Y SE RENOMBRAN LOS NOMBRES EN MINUSCULA, NO LOS EXPORTADOS.
 *
 * Wii64 no carga plugins: los ENLAZA, y cada plugin mapea sus funciones a los
 * nombres que el nucleo llama de verdad, con su propio `GFXPlugin.h`:
 *
 *     #define ProcessDList    processDList      <- Rice_GX/GFXPlugin.h:21
 *
 * O sea que el simbolo que llega al enlazador es `processDList`, no
 * `ProcessDList`.  La primera version de este fichero renombraba los nombres
 * en MAYUSCULA, y no habria servido de nada: `GFXPlugin.h` se incluye DESPUES
 * de la cabecera forzada y habria redefinido la macro, ganando la suya.  El
 * sintoma habria sido un `LNK2005` por cada punto de entrada, con este
 * fichero aparentemente correcto delante.
 *
 * ★★ LA LISTA ESTA MEDIDA, NO ADIVINADA.
 *
 * Se saco cruzando las definiciones a nivel de fichero de los 29 objetos de
 * `Makefile.Rice` contra las de los 38 de glN64 Y contra las del resto del
 * binario (r4300, gc_memory, rsp_hle, xenon/, xbox/).  Solo esta aqui lo que
 * DEFINEN LOS DOS LADOS.  La primera version la escribi de memoria y tenia
 * cuatro entradas que habrian roto el enlace y le faltaban cuatro que lo
 * habrian roto igual.
 *
 * ★ LO QUE NO ESTA, Y POR QUE -- esto es la mitad del valor de este fichero:
 *
 *   gettime, diff_sec, enableLoadIcon, rmode, Timers, text
 *       Rice solo los DECLARA `extern`; los define el shim
 *       (`xenon/gln64_glue.cpp:334,406`) o el nucleo
 *       (`source/main/timers.c:15`).  Renombrarlos daria un LNK2019 por
 *       `Rice_gettime` y compania, que NADIE define.  Es el error mas facil
 *       de cometer aqui, porque el grep los encuentra en los dos arboles.
 *
 *   ReadScreen
 *       glN64 lo mapea a `readScreen_gfx` en su GFXPlugin.h; Rice no lo mapea
 *       y exporta `ReadScreen`.  Son dos simbolos distintos: no chocan.
 *
 *   GetPluginDir
 *       el de glN64 esta en `Config_linux.cpp`, que esta EXCLUIDO del build
 *       (lo sustituye `xenon/gln64_config.cpp`), y encima es `static inline`.
 *
 *   changeWindow, moveScreen, captureScreen, drawScreen, getDllInfo_gfx,
 *   dllConfig_gfx, dllTest_gfx, dllAbout_gfx
 *       SI chocan y SI estan aqui, pero su gemelo `gln64_rename.h` NO los
 *       lleva: el renombrado es asimetrico a proposito.  Basta con mover un
 *       lado, y nadie en este core los llama.
 *
 * ★ SI EL ENLAZADOR SE QUEJA DE ALGO QUE NO ESTA AQUI, es un simbolo que el
 * censo no cazo (una definicion a varias lineas, o una global declarada en
 * una cabecera): se comprueba que los DOS lados lo definen y se anade a esta
 * lista.  Nunca al arbol copiado.
 */

#ifndef _XENON_RICE_RENAME_H_
#define _XENON_RICE_RENAME_H_

/* -- Los 18 puntos de entrada del plugin ----------------------------------
 * Ya en minuscula, que es como los deja `Rice_GX/GFXPlugin.h`.  Los diez
 * primeros son los que el nucleo llama de verdad y los que despacha
 * `xbox/gfx_dispatch.c`; los ocho ultimos no los llama nadie en este core,
 * pero los definen los dos plugins y por tanto chocan. */
#define initiateGFX              Rice_initiateGFX
#define processDList             Rice_processDList
#define processRDPList           Rice_processRDPList
#define updateScreen             Rice_updateScreen
#define romOpen_gfx              Rice_romOpen_gfx
#define romClosed_gfx            Rice_romClosed_gfx
#define closeDLL_gfx             Rice_closeDLL_gfx
#define showCFB                  Rice_showCFB
#define viStatusChanged          Rice_viStatusChanged
#define viWidthChanged           Rice_viWidthChanged
#define changeWindow             Rice_changeWindow
#define moveScreen               Rice_moveScreen
#define captureScreen            Rice_captureScreen
#define drawScreen               Rice_drawScreen
#define getDllInfo_gfx           Rice_getDllInfo_gfx
#define dllConfig_gfx            Rice_dllConfig_gfx
#define dllTest_gfx              Rice_dllTest_gfx
#define dllAbout_gfx             Rice_dllAbout_gfx

/* -- La interfaz con el anfitrion Wii64 -----------------------------------
 * Los dos plugins traen su propia copia de estas nueve, porque en Wii64 solo
 * hay una en el binario.  Aqui hay dos, y cada una la llama SOLO su propio
 * arbol, asi que prefijar el lado de Rice las separa sin tocar nada mas.
 *
 * ★ `GX_CheckResChange` es el caso a vigilar: ademas de las dos copias de los
 * plugins, `xenon/gln64_glue.cpp` la llama.  Esa llamada se queda apuntando a
 * la de glN64, que es lo correcto mientras el shim sea el de glN64. */
#define VI_GX_DrawSyncCallback   Rice_VI_GX_DrawSyncCallback
#define VI_GX_PreRetraceCallback Rice_VI_GX_PreRetraceCallback
#define VI_GX_setFB              Rice_VI_GX_setFB
#define VI_GX_showDEBUG          Rice_VI_GX_showDEBUG
#define VI_GX_showFPS            Rice_VI_GX_showFPS
#define VI_GX_showLoadIcon       Rice_VI_GX_showLoadIcon
#define GX_CheckResChange        Rice_GX_CheckResChange
#define gfx_set_fb               Rice_gfx_set_fb
#define gfx_set_window           Rice_gfx_set_window

/* -- Dos variables globales -----------------------------------------------
 * Estas se me escaparon en la primera version, y el enlazador las habria
 * cantado como LNK2005 sin decir de donde salian:
 *
 *     VI                glN64_GX/VI.cpp:41   <->  Rice_GX/OGLGraphicsContext.cpp:345
 *     GXtexCache        glN64_GX/Textures.cpp:53 <-> Rice_GX/TextureManager.cpp:37
 *
 * ★★ Y AQUI FALTAN `printToScreen` Y `showFPSonScreen` A PROPOSITO, despues
 * de haberlas puesto y de que costaran un LNK2001.
 *
 * Las definen los dos plugins, si, pero NO son simbolos de plugin: son un
 * ajuste del frontend, declarado en `source/main/wii64config.h:62,69`, que
 * en este port vale siempre 0 y que ya define y deja a cero
 * `glN64_GX/VI.cpp:47-48` (esta documentado en xbox/plataforma.c:546).  Lo
 * leen tambien ficheros C.
 *
 * ★ Y AL RENOMBRARLAS EL FALLO NO FUE UN DUPLICADO, SINO LO CONTRARIO, por
 * una asimetria de ENLACE que no se ve leyendo Rice:
 *
 *     OGLGraphicsContext.cpp:26  incluye ../main/wii64config.h
 *                                -> ve la declaracion dentro de `extern "C"`
 *                                -> pide `Rice_showFPSonScreen` SIN decorar
 *     Video.cpp                  NO lo incluye
 *                                -> define el simbolo con enlace C++
 *
 * Dos simbolos distintos con el mismo nombre en el fuente:
 * `LNK2001: unresolved external symbol Rice_showFPSonScreen`, con el nombre
 * SIN decorar, que es la pista de que lo que se busca tiene enlace C.
 *
 * Sin el renombrado, OGLGraphicsContext se engancha al unico simbolo de
 * enlace C que hay -- el de glN64, que vale 0 -- y Rice deja de dibujar su
 * contador de FPS, que es lo que este port quiere: no hay texto sobre el
 * juego ni debe haberlo (ver source/libgui/IPLFont.h).  La copia de enlace
 * C++ de Video.cpp se queda sin referenciar; no choca con nadie porque su
 * nombre decorado es otro.
 *
 * ★ LA REGLA QUE SE SACA: antes de anyadir un nombre a esta lista, mirar si
 * `wii64config.h` lo declara.  Si lo hace, es del frontend y NO se renombra.
 *
 * ★ `VI` da miedo porque es un token de dos letras, asi que se comprobo antes
 * de ponerlo: en `source/Rice_GX/` no aparece suelto NI UNA vez fuera de su
 * propia definicion y uso en OGLGraphicsContext.cpp.  Las demas apariciones
 * son cadenas de texto y comentarios ("At VI origin update", Config.h:145),
 * donde el preprocesador no entra.  El tipo `VIInfo` es otro token y no le
 * afecta. */
#define VI                       Rice_VI
#define GXtexCache               Rice_GXtexCache

#endif /* _XENON_RICE_RENAME_H_ */
