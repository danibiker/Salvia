/*
 * gxshim/sys/syslimits.h  --  PATH_MAX, que en el XDK no esta donde Rice lo
 * busca.
 *
 * QUIEN LO PIDE, Y POR QUE NO SE PUEDE ESQUIVAR.  Tres de los 29 objetos de
 * `Makefile.Rice`, y los tres desde la rama de GameCube/Wii, que es la
 * nuestra:
 *
 *     Rice_GX/Config.cpp:30           #else //!__GX__   -> rama __GX__
 *     Rice_GX/Video.cpp:35            idem
 *     Rice_GX/TextureFilters.cpp:34   idem
 *
 * ★ OJO AL COMENTARIO `#else //!__GX__` DE Rice, QUE SE LEE AL REVES.  Marca
 * el FINAL del bloque !__GX__, o sea que lo que viene DESPUES es la rama de
 * __GX__.  Leyendolo mal se concluye que esta cabecera no hace falta.
 *
 * En devkitPPC esto lo trae newlib.
 *
 * ★★ Y TIENE QUE SER UN LITERAL, NO `_MAX_PATH`.  Esto costo la primera
 * ronda de build: `source/winlnxdefs.h:83` hace
 *
 *     #define MAX_PATH  PATH_MAX
 *     #define _MAX_PATH PATH_MAX
 *
 * o sea que si aqui pusieramos `#define PATH_MAX _MAX_PATH`, la expansion
 * seria PATH_MAX -> _MAX_PATH -> PATH_MAX, y el preprocesador PARA ahi (no
 * reexpande una macro que ya esta en curso).  Lo que le llega al compilador
 * es el identificador `PATH_MAX` pelado:
 *
 *     Video.h(135): error C2065: 'PATH_MAX' : undeclared identifier
 *
 * que apunta a una linea que solo dice `char x[MAX_PATH];` y no menciona
 * PATH_MAX por ningun lado.  Con un literal el ciclo no existe.
 *
 * 260 es `_MAX_PATH` de MSVC y el limite real de la plataforma.  NO se pone
 * el 1024 de newlib: Rice reserva estos buffers EN PILA (`char
 * name[PATH_MAX]` en Config.cpp:325, 336, 493 y tres `TCHAR
 * x[PATH_MAX+1]` seguidos en 1770-1772), y ademas seria mentir sobre lo que
 * el sistema de ficheros admite.
 *
 * ★ Tiene que ser valido en C y en C++.
 */

#ifndef _XBOX64_GXSHIM_SYS_SYSLIMITS_H_
#define _XBOX64_GXSHIM_SYS_SYSLIMITS_H_

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

#endif /* _XBOX64_GXSHIM_SYS_SYSLIMITS_H_ */
