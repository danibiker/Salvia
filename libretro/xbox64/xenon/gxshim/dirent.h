/*
 * gxshim/dirent.h  --  para que dos ficheros de Rice compilen.  NADA MAS.
 *
 * ★★ NO LEE DIRECTORIOS, Y NO ES UN TODO-POR-HACER.
 *
 * `Rice_GX/Video.cpp:20` y `Rice_GX/TextureFilters.cpp:29` incluyen
 * <dirent.h> SIN guarda, en la cabecera del fichero.  Pero TODOS los sitios
 * que lo usan de verdad estan dentro de `#ifndef __GX__`, o sea en la rama
 * de Linux que aqui no se compila.  Se comprobo uno a uno:
 *
 *     Video.cpp:146-157            recorre el directorio del ejecutable,
 *                                  justo debajo de un
 *                                  readlink("/proc/self/exe")
 *     TextureFilters.cpp:932-936   comprueba si existe la carpeta de
 *                                  texturas de alta resolucion
 *     TextureFilters.cpp:1030-1227 la recorre
 *
 * O sea que lo unico que hace falta es que el `#include` no falle.  El
 * codigo que pediria una implementacion de verdad no llega al compilador.
 *
 * ★ POR ESO NO SE DECLARAN `DIR`, `struct dirent` NI opendir/readdir, Y ESO
 * TAMBIEN ES DELIBERADO.  Si un dia alguien enciende esa rama -- las
 * texturas de alta resolucion son lo mas probable-- el compilador le dira
 * `C2065: 'DIR': undeclared identifier` en la linea exacta, y sabra que
 * tiene que escribir un lector de directorios sobre FindFirstFile.  Con los
 * tipos declarados y sin funciones, el mismo caso saldria como un LNK2019
 * lejos del sitio; y con funciones vacias, como un directorio que parece
 * existir y siempre esta vacio, que es lo peor de los tres.
 *
 * ★ Y VA EN gxshim/ Y NO EN xbox/compat/ porque el que lo pide es el arbol
 * del plugin, no el nucleo; es el mismo criterio que <sys/syslimits.h>.  Los
 * dos directorios estan en AdditionalIncludeDirectories.
 */

#ifndef _XBOX64_GXSHIM_DIRENT_H_
#define _XBOX64_GXSHIM_DIRENT_H_

/* A proposito: sin contenido.  Ver arriba. */

#endif /* _XBOX64_GXSHIM_DIRENT_H_ */
