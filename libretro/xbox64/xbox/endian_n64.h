/*
 * xbox/endian_n64.h  --  el orden de bytes del anfitrion, en UN sitio.
 *
 * [Windows, 2026-10-26] Wii64 solo ha corrido en big-endian (Wii, Xbox 360) y
 * da el orden de bytes por supuesto en muchos sitios sin decirlo.  La build de
 * Windows es little-endian, y los arreglos de ese orden usan estas macros en
 * vez de reescribir cada uno su propio `#ifdef`.
 *
 * ★★ EN LA XBOX NO CUESTAN NADA: con `_BIG_ENDIAN` (lo define el vcxproj de
 * la Xbox) los xor valen 0 y las conversiones son la identidad, asi que el
 * preprocesador deja el MISMO codigo que habia antes de tocar nada.  En PC el
 * xor es una constante y la conversion un `bswap`.
 *
 * EL MODELO DE MEMORIA EN LITTLE-ENDIAN (el de mupen64, memory.h): la RDRAM,
 * la DMEM/IMEM del RSP y la ROM estan en palabras de 32 bits del orden del
 * anfitrion.  La palabra se lee tal cual; el byte N64 de la direccion `a` esta
 * en `a ^ 3` y la media palabra en `a ^ 2` (en big-endian, en `a`).  Es lo
 * mismo que `S8` / `S16` de gc_memory/memory.h, pero con nombre propio: Rice y
 * glN64 tienen sus propios `S16` con OTRO significado (indice de u16, no
 * direccion de byte), y mezclarlos es justo el fallo que hay que evitar.
 *
 * Los buferes que estan en el orden de bytes del N64 (la TMEM de glN64, la PIF
 * RAM, los ficheros del juego) se leen en unidades de 16/32 bits con
 * N64_BE16 / N64_BE32: big-endian a valor.
 *
 * ★ VALIDA EN C Y EN C++; solo macros.  La incluye xbox64_pre.h.
 */

#ifndef _XBOX64_ENDIAN_N64_H_
#define _XBOX64_ENDIAN_N64_H_

#ifdef _BIG_ENDIAN

#define N64_BYTE_XOR     0   /* direccion de un byte en RDRAM/DMEM/ROM    */
#define N64_HALF_XOR     0   /* direccion (en bytes) de una media palabra */
#define N64_HALFIDX_XOR  0   /* indice de un u16 en un array de u16       */
#define N64_BE16(v)      (v) /* u16 de un bufer en orden N64 -> su valor  */
#define N64_BE32(v)      (v) /* u32 de un bufer en orden N64 -> su valor  */

#else  /* little-endian: Windows x86 */

#include <stdlib.h>          /* _byteswap_ushort / _byteswap_ulong */

#define N64_BYTE_XOR     3
#define N64_HALF_XOR     2
#define N64_HALFIDX_XOR  1
#define N64_BE16(v)      ((unsigned short)_byteswap_ushort((unsigned short)(v)))
#define N64_BE32(v)      ((unsigned int)_byteswap_ulong((unsigned long)(v)))

#endif

#endif /* _XBOX64_ENDIAN_N64_H_ */
