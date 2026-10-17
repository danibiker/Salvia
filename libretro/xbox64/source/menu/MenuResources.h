/*
 * menu/MenuResources.h  --  shim para el port de glN64_GX a Xenon64.
 *
 * En Wii64 declara los recursos empotrados del menu (imagenes .png/.tex).
 * glN64_GX lo incluye desde VI.cpp:18 y lo usa SOLO para el icono de carga
 * de `VI_GX_showLoadIcon()`, que vive entero bajo `#ifdef HW_DOL`
 * (VI.cpp:183, 208, 229, 294) -- la rama de GameCube.
 *
 * No definimos HW_DOL, asi que el shim puede estar vacio: existe para que el
 * `#include` resuelva y el fichero copiado no haya que tocarlo.
 *
 * Comprobado: fuera de los bloques HW_DOL, VI.cpp no referencia ningun
 * simbolo de esta cabecera.
 */

#ifndef MENURESOURCES_H
#define MENURESOURCES_H

#endif /* MENURESOURCES_H */
