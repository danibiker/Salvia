/**
 * Wii64 - Recomp-Cache.h
 * Copyright (C) 2008, 2009, 2010 Mike Slegeir
 * 
 * Interface for allocating/freeing blocks of recompiled code
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: tehpola@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#ifndef RECOMP_CACHE_H
#define RECOMP_CACHE_H

/* [Xbox360] ★ El `#ifndef` deja que xbox/xbox_mem.h mande.  Alli la arena
 * del codigo es un ARRAY ESTATICO -- en esta consola lo que devuelve
 * `memalign` no se puede ejecutar-- y el tamanyo del array tiene que ser
 * EXACTAMENTE el mismo que el tope que usa `release()`.  Con las dos
 * definiciones sueltas podrian separarse y `release()` escribiria fuera
 * del array sin que nada avisara. */
#if defined(_XBOX)
/* Trae RECOMP_CACHE_SIZE y RECOMP_CACHE_LO.  Mismo reenvio y misma ruta que
 * usa gc_memory/MEM2.h:41. */
#include "../../xbox/xbox_mem.h"
#endif

#ifndef RECOMP_CACHE_SIZE
#ifdef HW_RVL
#ifdef GLN64_GX
#define RECOMP_CACHE_SIZE ((9*1024*1024) + (512*1024))	// glN64 Wii
#else
#define RECOMP_CACHE_SIZE (9*1024*1024)		// Rice Wii
#endif
#else
# ifdef USE_EXPANSION
#  ifdef GLN64_GX
#   define RECOMP_CACHE_SIZE (7*1024*1024) //7 MB for GC with expansion pak + glN64
#  else
#   define RECOMP_CACHE_SIZE (6*1024*1024) //6 MB for GC with expansion pak + Rice
#  endif
# else
#  define RECOMP_CACHE_SIZE (8*1024*1024) //8 MB for GC
# endif
#endif
#endif /* [Xbox360] !RECOMP_CACHE_SIZE */
#include "ppc/Recompile.h"

/* [Xbox360] `RecompCache_Init` la llama `r4300.c:205` (C) y la define
 * Recomp-Cache-Heap.c, que se compila como C++.  Ver la nota de
 * ppc/Recompile.h sobre por que esto va simbolo a simbolo: `release`, aqui
 * abajo, es justo el nombre que no conviene soltar al espacio global. */
#ifdef __cplusplus
extern "C" {
#endif
void RecompCache_Init(void);
#ifdef __cplusplus
}
#endif
// Allocate and free memory to be used for recompiled code
//   Any memory allocated this way can be freed at any time
//   you must check invalid_code before you can access it
void RecompCache_Alloc(unsigned int size, unsigned int address, PowerPC_func* func);
void RecompCache_Realloc(PowerPC_func* func, unsigned int new_size);
void RecompCache_Free(unsigned int addr);
/* [Xbox360] ★ [Castlevania, variantes de TLB] LIBERAR POR PUNTERO, NO POR
 * DIRECCION.  Con variantes, dos funciones de la misma pagina virtual (de
 * paginas fisicas distintas) pueden empezar en la MISMA direccion, y
 * RecompCache_Free(addr) liberaria la primera que encuentre: la de otra
 * variante.  Donde se tiene la funcion, se libera esa. */
void RecompCache_FreeFunc(PowerPC_func* func);
/* [Xbox360] Deshace los enlaces que ENTRAN en `func` (los saltos de otras
 * funciones vuelven al despachador), sin liberarla y sin tocar los que salen
 * de ella.  Es lo que se hace al apartar una variante: ningun salto enlazado
 * puede llegar a su codigo mientras la pagina virtual apunte a otra fisica.
 * Los que salen se quedan: si su destino se libera, el propio desenlace del
 * destino parchea este codigo, este o no apartado. */
void RecompCache_UnlinkIn(PowerPC_func* func);
void release(int minNeeded);
// Update the LRU info of the indicated block
//   (call when the block is accessed)
void RecompCache_Update(PowerPC_func* func);
void RecompCache_Link(PowerPC_func* src_func, PowerPC_instr* src_instr,
                      PowerPC_func* dst_func, PowerPC_instr* dst_instr);

// Allocate memory from the meta cache
//   This will free from both the recomp and meta caches if capacity is hit
void* MetaCache_Alloc(unsigned int num_bytes);
// Free data from the meta cache
void MetaCache_Free(void* ptr);

#endif
