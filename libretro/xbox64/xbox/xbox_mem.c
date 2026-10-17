/*
 * xbox/xbox_mem.c  --  las reservas del mapa de memoria.
 *
 * El porque de que sean arrays estaticos y no malloc esta en xbox_mem.h.
 *
 * ALINEACION.  El original coloca cada bloque en una direccion del mapa que
 * es multiplo de 32 KB como poco, asi que todo venia alineado "gratis".
 * Aqui hay que pedirlo:
 *
 *   - 32 bytes es lo que el arbol asume en todas partes (rdram, SP_DMEM,
 *     PIF_RAM y eeprom lo piden explicitamente).
 *   - 128 bytes es la linea de cache del Xenon.  Se usa para los bloques
 *     grandes que se recorren enteros (invcode, tlblut): asi ni el
 *     principio ni el final comparten linea con otra cosa, que es lo que
 *     provoca falso compartido cuando haya hilos.
 *
 * ★ tlb_LUT_r y tlb_LUT_w se colocan en TLBLUT_LO y TLBLUT_LO+0x400000
 * (tlb.c:46-47), o sea DENTRO del mismo bloque de 8 MB, partido en dos.
 * Por eso es UN array de 8 MB y no dos de 4: si fueran dos, nada garantiza
 * que el segundo caiga exactamente 4 MB despues del primero.
 *
 * C89.
 */

#include "xbox_mem.h"

__declspec(align(128)) unsigned char xbox64_mem_invcode  [INVCODE_SIZE];
__declspec(align(128)) unsigned char xbox64_mem_tlblut   [TLBLUT_SIZE];
/* ★ LOS CUATRO GUARDADOS, JUNTOS.  El porque esta en xbox_mem.h: libretro
 * los entrega al frontend como un solo bloque contiguo y quieto.
 *
 * Alineado a 32 como estaban los tres arrays que sustituye, y el primer
 * campo -- eeprom -- es justo el que lo pedia explicitamente. */
__declspec(align(32)) struct xbox64_guardado xbox64_mem_guardado;

/* ★ BLOCKS_LO se castea a `PowerPC_block**` (r4300.c:64): es un array de
 * PUNTEROS de 0x100000 entradas, no de datos.  4 MB / 4 = 1M entradas, que
 * es justo el rango de `blocks[address>>12]`.  Si esto se recorta, el
 * indice se sale sin aviso.
 *
 * Va sin guarda: PPC_DYNAREC esta definido siempre.  Ver xbox_mem.h. */
__declspec(align(128)) unsigned char xbox64_mem_blocks    [BLOCKS_SIZE];

#if XBOX64_DYNAREC
__declspec(align(128)) unsigned char xbox64_mem_recompmeta[RECOMPMETA_SIZE];
__declspec(align(32))  unsigned char xbox64_mem_rcscratch [RECOMP_CACHE_HEAP_SCRATCH_SIZE];

/* ★ LA ARENA DEL CODIGO RECOMPILADO, y por eso esta AQUI y no en el monton:
 * lo que devuelve `memalign` en esta consola se lee pero NO SE EJECUTA
 * (medido; la nota larga esta en xbox_mem.h).  Alineada a 128, que es la
 * linea de cache del Xenon: el codigo emitido se invalida por lineas
 * enteras y empezar a mitad de linea obligaria a tocar la anterior. */
__declspec(align(128)) unsigned char xbox64_mem_recompcache[RECOMP_CACHE_SIZE + RECOMP_CACHE_ISLA];
#endif

#if !XBOX64_GFX_NULO
/* __lwp_heap_init lo inicializa con alineacion 32 (VI.cpp:459), asi que la
 * base tiene que estar al menos a 32. */
__declspec(align(128)) unsigned char xbox64_mem_texcache[TEXCACHE_SIZE];

/* [Wii64 443f68b] DepthCopy.cpp: lo escribe el consumidor D3D
 * (`gxd3d_consumidor_lee_z`) y lo lee el plugin tras `GX_DrawDone`. */
__declspec(align(128)) unsigned char xbox64_mem_dcbuf[DEPTHCOPY_BUF_SIZE];
__declspec(align(128)) unsigned char xbox64_mem_dclut[DEPTHCOPY_LUT_SIZE];
#endif
