/*
 * xbox/xbox64_pre.h  --  cabecera FORZADA, la primera de cada unidad.
 *
 * Entra por <ForcedIncludeFiles> del vcxproj, o sea antes de la primera
 * linea de cada .c y de cada .cpp.  No la incluye nadie a mano.
 *
 * ★★ EL PROBLEMA QUE RESUELVE, MEDIDO.
 *
 * `source/r4300/macros.h` define los registros COP0 del R4300 como macros
 * con nombres completamente genericos:
 *
 *     #define Index    r4300.reg_cop0[0]     macros.h:60
 *     #define Context  r4300.reg_cop0[4]     macros.h:64
 *     #define Count    r4300.reg_cop0[9]     macros.h:68
 *     #define Compare  r4300.reg_cop0[11]    macros.h:70
 *     #define Status   r4300.reg_cop0[12]    macros.h:71
 *     #define Config   r4300.reg_cop0[16]    macros.h:75
 *
 * Esos seis nombres aparecen 98 veces COMO IDENTIFICADOR en las cabeceras
 * del XDK que este proyecto usa:
 *
 *     d3d9.h           46      xbox.h            4
 *     d3d9gpu.h        13      winnt.h           6
 *     xcompress.h       8      ppcintrinsics.h   1
 *
 * Si `macros.h` se expande ANTES, el XDK se rompe con errores que no
 * apuntan a nada:
 *
 *     d3d9gpu.h:2737   DWORD Count : 3;
 *       -> DWORD r4300.reg_cop0[9] : 3;
 *       -> error C2143: syntax error : missing ';' before '.'
 *     xbox.h:175       __in XAUDIOSPEAKERCONFIG Config
 *       -> ... r4300.reg_cop0[16]
 *       -> error C2143: syntax error : missing ')' before '.'
 *
 * ★ POR QUE NO PASA EN Xenon64 con el MISMO macros.h.  Porque alli las
 * cabeceras del XDK no entran NUNCA en las unidades que incluyen macros.h:
 * su `winlnxdefs.h` (el que trae <xtl.h>) solo lo incluyen ficheros C++ del
 * shim.  En este arbol si entran, porque `main/plugin.h` -- que lo incluyen
 * interupt.c, pif.c y memory.c -- arrastra winlnxdefs.h.  Cambiar eso seria
 * pelearse con el arbol de Wii64; forzar el orden es una linea.
 *
 * ★ Y POR QUE UNA CABECERA FORZADA Y NO REORDENAR LOS INCLUDES.  Porque el
 * orden correcto habria que mantenerlo en las 39 unidades, para siempre, y
 * una sola equivocada da un error a 2000 lineas de distancia en un fichero
 * de Microsoft.  Con esto la regla es estructural: cuando el codigo de
 * Wii64 empieza a leerse, el XDK ya esta entero.
 *
 * ★★ REGLA PARA LAS FASES SIGUIENTES: toda cabecera NUEVA del XDK que el
 * proyecto empiece a usar -- <d3dx9.h> y <xgraphics.h> en la fase 2 -- hay
 * que anyadirla AQUI, no en el fichero que la necesita.  Si se incluye
 * suelta desde un .cpp que ya vio macros.h, vuelve exactamente el mismo
 * fallo.
 *
 * ★ TIENE QUE SER VALIDO EN C Y EN C++: la fuerzan las 39 unidades.
 */

#ifndef _XBOX64_PRE_H_
#define _XBOX64_PRE_H_

/* ------------------------------------------------------------------ */
/* ★ SONDA DE VERSION                                                  */
/* ------------------------------------------------------------------ */
/* Sale UNA VEZ por unidad de compilacion en la salida del build.
 *
 * Existe porque este fichero se edita en una maquina y se compila en otra,
 * y hubo una ronda en la que el log de errores salio IDENTICO al anterior:
 * no habia forma de distinguir "el arreglo no funciona" de "el arreglo no
 * ha llegado".  Con esto, la primera linea del log lo dice.
 *
 * ★ COMO SE USA: descomentar la linea, SUBIR EL NUMERO de version, y
 * mirar la primera linea del log.  Si el mensaje no sale, el arbol que se
 * compila no es el que se edito y no hay nada que buscar en el codigo.
 *
 * Se deja comentada porque sale UNA VEZ POR UNIDAD -- unas 45 lineas por
 * build -- y los logs hay que leerlos y pegarlos.  Cumplio su funcion el
 * 2026-09-23: confirmo que v2 llegaba, y con ella el shim de stdbool. */
/* #pragma message("[xbox64] xbox64_pre.h v3 : xtl+ppcintrinsics+float+time+stdbool, inline/isnan/localtime_r") */

/* Arrastra xbox.h, d3d9.h -> d3d9gpu.h, winnt.h, winbase.h, xcompress.h. */
#include <xtl.h>

/* NO la arrastra xtl.h, y la usan xenon/gxshim/ogc/machine/processor.h
 * (para __lwbrx / __lhbrx, que necesita main/rom_gc.c:83) y varios ficheros
 * del shim grafico.  Tiene una de las seis colisiones. */
#include <ppcintrinsics.h>

/* [Fase 2] El shim GX->D3D9.  ★ VAN AQUI Y NO EN gx_d3d9.cpp, que es donde
 * se necesitan: si se incluyen sueltas desde un .cpp que ya vio macros.h,
 * vuelve exactamente la colision de las seis macros COP0 -- `d3d9.h` solo
 * tiene 46 apariciones de `Count`, `Index` y companyia. */
#include <d3dx9.h>
#include <xgraphics.h>

/* Los dos que necesitan las macros de compatibilidad de mas abajo.  ★ Van
 * AQUI, con el resto de includes, y no pegados a la macro que los usa: este
 * fichero le impone al arbol la regla de "todas las cabeceras antes que
 * ninguna macro", y seria absurdo que el propio fichero la rompiera.  Con
 * `#define inline` vacio activo, cualquier cabecera posterior que use
 * `inline` se rompe. */
#include <float.h>   /* _isnan      */
#include <time.h>    /* localtime_s */
#include <string.h>  /* _stricmp / _strnicmp, para strcasecmp de mas abajo */

/* [Fase 3] `offsetof`.  El dynarec la usa 60 veces para emitir los accesos
 * a la struct `R4300` (`MIPS-to-PPC.c`, `Register-Cache.c`, `Recompile.c`),
 * y NINGUNO de esos ficheros incluye <stddef.h>: en devkitPPC se la
 * arrastraba alguna cabecera de libogc.
 *
 * ★ EL SINTOMA NO DICE "falta offsetof", DICE DOS COSAS A LA VEZ:
 *     error C3861: 'offsetof': identifier not found
 *     error C2275: 'R4300' : illegal use of this type as an expression
 * La segunda es la que despista: al no conocer `offsetof`, el compilador
 * lee `offsetof(R4300, gpr)` como una llamada a funcion y se queja del
 * nombre de tipo que hay dentro.  Fueron ~80 de los primeros 100 errores.
 *
 * ★ Y VA AQUI, NO EN LOS TRES FICHEROS: es la regla de este fichero.  Es
 * ademas el mismo error de metodo que ya costo una ronda con <stdbool.h>,
 * cuando reproduje la cadena de includes de libogc fichero a fichero y
 * flashram.c se quedo fuera. */
#include <stddef.h>  /* offsetof */

/* ★ El shim de <stdbool.h> va DESPUES de <xtl.h> a proposito: define
 * `bool` como macro, y una macro antes del XDK es exactamente la clase de
 * veneno que este fichero existe para evitar.  El porque de que haga falta
 * esta en el bloque `bool` de mas abajo. */
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* [Fase 6] Los dos plugins graficos en el mismo binario               */
/* ------------------------------------------------------------------ */
/* ★★ ES LA ULTIMA CABECERA, Y TIENE QUE SERLO.
 *
 * glN64_GX y Rice_GX definen los mismos ~31 simbolos (los 18 puntos de
 * entrada del plugin, nueve funciones de la interfaz con el anfitrion Wii64
 * y cuatro globales).  En Wii64 nunca chocan porque son dos binarios
 * distintos -- `Makefile.glN64_wii` contra `Makefile.Rice_wii`--; aqui van
 * los dos en el mismo core, con una opcion en el menu, asi que hay que
 * separarlos.  Se hace prefijando con el preprocesador, sin tocar ni una
 * linea de los dos arboles copiados.  El detalle, la lista medida y sobre
 * todo LO QUE NO ESTA EN ELLA, en las dos cabeceras.
 *
 * ★ VA AQUI, DESPUES DE TODAS LAS CABECERAS, POR LA REGLA DE ESTE FICHERO.
 * Una de las macros es `VI`, un token de dos letras: si entrase antes que
 * <xtl.h> seria exactamente el mismo veneno que las seis macros COP0 que
 * este fichero existe para evitar.  Al llegar aqui, el XDK ya esta entero.
 *
 * ★ Y LA DISCRIMINACION ES POR MACRO, NO POR <ForcedIncludeFiles> POR
 * FICHERO.  `__GX__` ya lo llevan los 38 .cpp de glN64 desde la fase 2;
 * basta con anyadir `XBOX64_PLUGIN_RICE` a los 29 de Rice, que son lineas
 * nuevas.  Asi las 38 de glN64 no se tocan: cero ediciones mecanicas, cero
 * rutas que escribir mal.
 *
 * ★★ LOS 29, SIN EXCEPCIONES, aunque uno de ellos
 * (TextureArchive/ArchiveReader.cpp) no defina ningun simbolo que choque.
 * Se intento dejarlo sin defines y esta mal: comparte `Archive.h` con
 * Rice_GX/TextureFilters.cpp, y esa cabecera declara `TextureCRC64` con
 * `__attribute__((packed))`.  Con el define en una unidad y no en la otra,
 * las dos ven la MISMA struct con distinto tamanyo.  El entorno de macros
 * tiene que ser identico en las unidades que comparten cabeceras.
 *
 * ★ Las unidades SIN `__GX__` -- el nucleo, el shim D3D y sobre todo
 * `xbox/gfx_dispatch.c`-- no ven ningun renombrado, que es justo lo que hace
 * falta: el despachador se queda con los nombres sin prefijo, que son los
 * que el nucleo enlaza. */
#if defined(__GX__)
#  if defined(XBOX64_PLUGIN_RICE)

/* ★★ EL ORDEN DE ESTAS TRES LINEAS ES EL ARREGLO, no un detalle de estilo.
 *
 * 1) PATH_MAX PRIMERO, porque `source/winlnxdefs.h:83` hace
 *    `#define MAX_PATH PATH_MAX`.  Si PATH_MAX no existe cuando eso se
 *    expande, el error sale en `Video.h(135)`, una linea que solo dice
 *    `char screenCaptureFilename[MAX_PATH];` y no nombra PATH_MAX.
 *
 * 2) `source/winlnxdefs.h` DESPUES.  No hace falta por lo que trae, sino
 *    por lo que IMPIDE: se apropia del include guard `WINLNXDEFS_H`, que es
 *    el MISMO que usa `Rice_GX/winlnxdefs.h`, y asi la copia de Rice se
 *    salta entera.  Es el mismo truco ya documentado en
 *    `source/main/winlnxdefs.h` para glN64.
 *
 *    Hace falta porque la de Rice redeclara BOOL, DWORD, CHAR, TCHAR,
 *    ULONG, LONG, VOID, HWND, HBITMAP y RECT con otros tipos base: catorce
 *    C2371 contra `windef.h` y `winnt.h` del XDK.  La de `source/` es un
 *    SUPERCONJUNTO de la de Rice ya adaptado (mismos COORDRECT,
 *    BITMAPINFOHEADER, RGBQUAD, BITMAPINFO, IMAGE_FILEFORMAT,
 *    BITMAPFILEHEADER, BI_RGB y BLEND), pero con todo lo que ya da el XDK
 *    detras de `#ifndef _XTL_` y con `#pragma pack` en vez de
 *    `__attribute__((packed))`.  O sea que Rice no pierde nada.
 *
 * 3) El renombrado AL FINAL, que es puro texto y no depende de nadie. */
#    include <sys/syslimits.h>
#    include "../source/winlnxdefs.h"
#    include "../xenon/rice_rename.h"

/* ★ `__attribute__` a vacio.  Quedan DOS sitios en el arbol de Rice que no
 * cubre el cambio de winlnxdefs, los dos en `RSP_Parser.h:518`
 * (`RDP_BlenderSetting`) y su gemelo de mas abajo: uniones de campos de bits
 * marcadas `__attribute__((packed))`.  MSVC no conoce la palabra y el error
 * no dice "no conozco __attribute__", dice cuatro cosas seguidas empezando
 * por `C2065: 'packed' : undeclared identifier`.
 *
 * ★ SE PIERDE EL EMPAQUETADO, Y HAY QUE SABERLO.  Con GCC la union ocupa 2
 * bytes; aqui ocupara 4, porque los campos de bits se declaran sobre
 * `unsigned int`.  Es inofensivo MIENTRAS solo se lean campos por nombre,
 * que es lo unico que hace Rice (`RSP_Parser.cpp:1278-1281` asigna
 * `bl.blender` y lee `bl.c1_m1a` y companyia).  Si algun dia alguien hace
 * `sizeof` de esto, o lo vuelca a fichero, o lo mapea sobre la RDRAM, el
 * tamanyo sera OTRO y no habra ningun aviso.
 *
 * El ORDEN de los campos si esta bien: Rice ya tiene su `#ifdef _BIG_ENDIAN`
 * (RSP_Parser.h:505) y `_BIG_ENDIAN=1` lo define el vcxproj para todo el
 * proyecto. */
#    define __attribute__(x)

/* ★★ `CHAR` VUELVE A SER `unsigned char`, QUE ES LO QUE EL CODIGO ESPERA.
 *
 * El XDK lo define como `char` (`winnt.h:239`); Wii64 lo definia como
 * `unsigned char` (`Rice_GX/winlnxdefs.h:30`), y Rice esta ESCRITO para esa
 * segunda version: castea a `(char*)` justo donde necesita un char
 * -- `sprintf((char*)szCRC,...)` en Config.cpp:1978, `strcpy((char*)...)` en
 * 1982, `strcasecmp((char*)szCRC,...)` en 1994, `DLParser_IdentifyUcode(...,
 * (char*)str)` en RSP_Parser.cpp:867-- y lo pasa CRUDO donde quiere el sin
 * signo: `unsigned char *p = str;` (847) y
 * `DLParser_IdentifyUcodeFromString(str)` (883), cuya firma es
 * `const unsigned char *`.
 *
 * O sea que los casts del arbol solo cuadran con la definicion de Wii64.
 * Dejarlo como `char` y arreglar los dos sitios que fallan seria tocar el
 * arbol copiado para que deje de ser coherente consigo mismo.
 *
 * ★ `TCHAR` NO SE TOCA, aunque Wii64 tambien lo hacia `unsigned char`.  El
 * de MSVC (`char`) compila sus nueve usos sin una sola queja: el build lo
 * demostro, con Config.cpp llegando hasta la linea 1994.  Cambiarlo seria
 * anyadir riesgo sin ninguna medida que lo pida. */
#    define CHAR unsigned char

#  else
#    include "../xenon/gln64_rename.h"
#  endif
#endif

/* ====================================================================== */
/* A partir de aqui, MACROS.  Ninguna cabecera mas.                       */
/* ====================================================================== */

/* ------------------------------------------------------------------ */
/* bool: MSVC 10 no trae <stdbool.h> para C                            */
/* ------------------------------------------------------------------ */
/* ★ AQUI Y NO EN LA CADENA DE INCLUDES DE CADA FICHERO.
 *
 * `gc_memory/{pif,dma,flashram}.c` escriben `bool x = false;` sin incluir
 * nada: en devkitPPC les llegaba arrastrado por el <gctypes.h> de libogc.
 * La primera version reprodujo esa cadena en gxshim/gctypes.h... y
 * `flashram.c` NO INCLUYE winlnxdefs.h, asi que a el no le llegaba y
 * reventaba con `C2065: 'false' : undeclared identifier`.
 *
 * Reproducir cadenas de includes ajenas fichero a fichero es adivinar.
 * Desde aqui llega a los 39, y da igual que manyana alguien quite un
 * include.  El shim y el porque del `unsigned char` estan en
 * xbox/compat/stdbool.h.
 *
 * El `#include <stdbool.h>` esta arriba, con el resto de cabeceras. */

/* ------------------------------------------------------------------ */
/* inline: no es palabra clave del compilador de C de MSVC 10          */
/* ------------------------------------------------------------------ */
/* Sitios en el build: gc_memory/memory.c:1861, gc_memory/pif.c:91,
 * gc_memory/TLB-Cache.h:46-50 (`unsigned int inline f(...)`, con el
 * `inline` DETRAS del tipo) y r4300/r4300.{c,h}, que ya llevan su parche
 * explicito.
 *
 * ★ SE DEFINE A VACIO, NO A `__inline`, Y LA DIFERENCIA IMPORTA.
 *
 * Wii64 compila con `-fgnu89-inline` (Makefile.base): en esa semantica
 * `inline` SIEMPRE emite la funcion con enlace externo, o sea que se
 * comporta como una funcion normal y la palabra clave es solo una
 * sugerencia.  Una macro vacia reproduce eso exactamente.
 *
 * `__inline` de MSVC no: sigue la regla de C++, en la que la funcion puede
 * no emitirse.  Para una `inline` no-static llamada desde otra unidad eso
 * es un LNK2019 que aparece lejos de aqui.  (Hoy no hay ninguna --
 * `update_vi_current` es local a memory.c y `byte2bcd` es static -- pero
 * la macro vacia es correcta pase lo que pase.)
 *
 * ★ Solo para C.  En C++ `inline` es palabra clave con significado propio,
 * y ademas los 14 ficheros de rsp_hle se compilan asi. */
#if !defined(__cplusplus)
#define inline
#endif

/* ------------------------------------------------------------------ */
/* isnan: C99, y MSVC 10 no la tiene                                   */
/* ------------------------------------------------------------------ */
/* `r4300/pure_interp.c` la usa 30 veces, en las comparaciones de coma
 * flotante del COP1 (C.EQ.S, C.LT.D y companyia): el R4300 tiene un
 * resultado distinto para las comparaciones "ordenadas" y las
 * "desordenadas", y la diferencia es exactamente si algun operando es NaN.
 * O sea que no es cosmetica -- sin ella, esas 30 comparaciones darian el
 * resultado equivocado con NaN.
 *
 * MSVC 10 ofrece `_isnan(double)` en <float.h>.  Es el mismo caso que el
 * documentado en este repo como firma de que falta CompileAs=Default
 * (C3861 en _isnan / strcasecmp).
 *
 * ★ Va aqui y no en pure_interp.c por lo mismo que todo lo demas de este
 * fichero: para que el orden respecto al XDK no dependa de nadie.  Y con
 * `#ifndef` por si algun dia el XDK la trae.
 *
 * El cast a double es exacto para float: no pierde nada. */
#ifndef isnan
#define isnan(x) (_isnan((double)(x)))
#endif

/* ------------------------------------------------------------------ */
/* localtime_r: POSIX, y MSVC 10 tiene la suya con otro nombre         */
/* ------------------------------------------------------------------ */
/* La usa `gc_memory/pif.c:211`, para el reloj del cartucho de 64DD que
 * responde por el bus del PIF.
 *
 * ★ Y LOS ARGUMENTOS VAN AL REVES:
 *     POSIX  localtime_r(const time_t *t, struct tm *out)
 *     MSVC   localtime_s(struct tm *out, const time_t *t)
 * Cambiarlos de sitio no da error de compilacion -- los dos son punteros --
 * sino una fecha basura.  Por eso la macro los cruza explicitamente.
 *
 * El valor de retorno difiere (`struct tm*` contra `errno_t`), pero el
 * unico sitio que la llama lo ignora. */
#ifndef localtime_r
#define localtime_r(t, out) localtime_s((out), (t))
#endif

/* ------------------------------------------------------------------ */
/* strcasecmp / strncasecmp: POSIX, y MSVC las tiene con otro nombre   */
/* ------------------------------------------------------------------ */
/* [Fase 6] Las usa `Rice_GX/Config.cpp` 46 veces (el parser del .ini por
 * juego: comparaciones de clave sin distinguir mayusculas) y
 * `Rice_GX/RSP_Parser.cpp` dos, en el reconocimiento del ucode por su cadena.
 *
 * ★ EL SINTOMA ES LA FIRMA DE OTRO PROBLEMA CONOCIDO.  `C3861: 'strcasecmp':
 * identifier not found` es exactamente lo que sale cuando a un .c del XDK le
 * falta `CompileAs=Default` -- ya esta documentado en este repo--, asi que lo
 * primero que uno mira es el vcxproj.  Aqui NO es eso: son .cpp y compilan
 * bien; simplemente MSVC llama a estas funciones `_stricmp` y `_strnicmp`.
 *
 * ★ Van aqui y no solo en la rama de Rice a proposito: no son especificas de
 * ningun plugin, son el mismo hueco de POSIX que `isnan` y `localtime_r` de
 * arriba, y con `#ifndef` no molestan a nadie.  <string.h> esta arriba, con
 * el resto de cabeceras. */
#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

#endif /* _XBOX64_PRE_H_ */
