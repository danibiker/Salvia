/**
 * Wii64 - Recomp-Cache-Heap.c
 * Copyright (C) 2009, 2010 Mike Slegeir
 * 
 * Cache for recompiled code using a heap to maintain LRU order
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

#include <gccore.h>
#include <ogc/lwp_heap.h>
#include <stdlib.h>
#include <malloc.h>
#include "r4300.h"
#include "ppc/Recompile.h"
#include "ppc/Wrappers.h"
#include "Invalid_Code.h"
#include "Recomp-Cache.h"
#include "../gui/DEBUG.h"
#include "../xenon64_diag.h"   /* XBOX64_TRAZAS */

#ifdef HW_RVL
#include "../gc_memory/MEM2.h"
#else
#include "../gc_memory/ARAM.h"
#endif

typedef struct _meta_node {
	unsigned int  addr;
	PowerPC_func* func;
	unsigned int  size;
} CacheMetaNode;

static heap_cntrl* cache = NULL, * meta_cache = NULL;
static int cacheSize = 0;

#define HEAP_CHILD1(i) ((i<<1)+1)
#define HEAP_CHILD2(i) ((i<<1)+2)
#define HEAP_PARENT(i) ((i-1)>>2)

#define INITIAL_HEAP_SIZE (64)
#define MIN_RELEASE_SIZE (512*1024)

#define NODE_HEAP_SIZE   (384 * 1024)
#define MAX_HEAP_ENTRIES (16384)

#if defined(_XBOX)
/* [Xbox360] ★ COMPROBACION EN COMPILACION, no un comentario que pedir que
 * alguien respete: el hueco que `xbox_mem.h` reserva para la isla de
 * trampolines tiene que ser EXACTAMENTE el que la isla ocupa.  Si se
 * separan, o el monton pisa los trampolines o se desperdicia espacio y la
 * contabilidad de `release()` vuelve a mentir.  Este fichero es el unico
 * que ve las dos definiciones.  Si no cuadran: "negative subscript". */
typedef char xbox64_chk_isla[(DYNA_STUB_BYTES == RECOMP_CACHE_ISLA) ? 1 : -1];

/* [Xbox360] ★ SONDA DEL DESENLACE.  Ver la nota larga en `unlink_func`.
 * Callada mientras todo va bien: solo habla cuando el sitio que se va a
 * parchear NO es el que deberia. */
#define XBOX64_SONDA_DESENLACE (XBOX64_TRAZAS && 1)
#endif
#define LRU_SCRATCH   ((CacheMetaNode**)RECOMP_CACHE_HEAP_SCRATCH_LO)

heap_cntrl* node_heap = NULL;
static CacheMetaNode** cacheHeap = NULL;
static unsigned int maxHeapSize = 0;
static unsigned int heapSize = 0;

void release(int minNeeded);

static void heapSwap(int i, int j){
	CacheMetaNode* t = cacheHeap[i];
	cacheHeap[i] = cacheHeap[j];
	cacheHeap[j] = t;
}

static void heapUp(int i){
	// While the given element is out of order
	while(i && cacheHeap[i]->func->lru < cacheHeap[HEAP_PARENT(i)]->func->lru){
		// Swap the child with its parent
		heapSwap(i, HEAP_PARENT(i));
		// Consider its new position
		i = HEAP_PARENT(i);
	}
}

static void heapDown(int i){
	// While the given element is out of order
	while(1){
		unsigned int lru = cacheHeap[i]->func->lru;
		CacheMetaNode* c1 = (HEAP_CHILD1(i) < heapSize) ?
		                     cacheHeap[HEAP_CHILD1(i)] : NULL;
		CacheMetaNode* c2 = (HEAP_CHILD2(i) < heapSize) ?
		                     cacheHeap[HEAP_CHILD2(i)] : NULL;
		// Check against the children, swapping with the min if parent isn't
		if(c1 && lru > c1->func->lru &&
		   (!c2 || c1->func->lru < c2->func->lru)){
			heapSwap(i, HEAP_CHILD1(i));
			i = HEAP_CHILD1(i);
		} else if(c2 && lru > c2->func->lru){
			heapSwap(i, HEAP_CHILD2(i));
			i = HEAP_CHILD2(i);
		} else break;
	}
}

static void heapify(void){
	int i;
	for(i=1; i<heapSize; ++i) heapUp(i);
}

static void heapPush(CacheMetaNode* node)
{
    // If the heap is full, free at least one block
    if (heapSize >= maxHeapSize) {
        // Free something, enough to hold the new thing
        release(node->size);

        if (heapSize >= maxHeapSize) {
            // free at least one LRU block
            release(1);
        }
    }

    /* [Xbox360] ★★ GUARDA DURA.  Si `release()` no consiguio hacer sitio,
     * la linea de abajo escribia PASADO EL ARRAY -- y `cacheHeap` sale de
     * `node_heap`, que es memoria del MONTON DE LA CRT, o sea la vecina de
     * las superficies SDL del frontend.  Un desbordamiento aqui no se
     * manifiesta aqui: se manifiesta como una violacion de acceso dentro
     * de un blit del frontend, que es exactamente el sintoma que estamos
     * persiguiendo en Donkey Kong 64.
     *
     * No se "arregla" descartando el nodo a la ligera: si esto salta, hay
     * un problema real de capacidad y hay que verlo.  Pero perder un nodo
     * de LRU es recuperable; corromper el monton no. */
    if (heapSize >= maxHeapSize) {
#if XBOX64_TRAZAS
        sprintf(txtbuffer, "[DYNA] ★ heapPush SIN SITIO: heapSize=%u max=%u "
                "cacheSize=%u size=%u", heapSize, maxHeapSize,
                (unsigned int)cacheSize, (unsigned int)node->size);
        DEBUG_print(txtbuffer, DBG_USBGECKO);
#endif
        return;
    }

    cacheHeap[heapSize++] = node;
}

static CacheMetaNode* heapPop(void){
	heapSwap(0, --heapSize);
	heapDown(0);
	return cacheHeap[heapSize];
}

/* [Xbox360] ★ TERCERA FUNCION ANIDADA DE GCC, sacada a ambito de fichero.
 * Las otras dos estaban en ppc/Recompile.c.  Extension de GNU C: ni C89, ni
 * C99, ni C++ (`error C2601: local function definitions are illegal`).
 *
 * ★ CAPTURA `func`, el parametro de `unlink_func`, y lo usa SOLO PARA
 * COMPARAR (:159 `(*link)->func == func`).  Al ser de solo lectura pasa por
 * VALOR, al contrario que las tres de `handle_overlap`, que se escribian y
 * tuvieron que ir por puntero.
 *
 * ★ Y AQUI EL ERROR EN CASCADA ENGANYA MAS QUE EN NINGUN SITIO:
 *     error C2275: 'func' : illegal use of this type as an expression
 *     ... see declaration of 'func'  ->  ppc/Recompile.h(49)
 * `func` es TAMBIEN el nombre de una etiqueta de struct (`typedef struct
 * func {...} PowerPC_func`).  Cuando el parser pierde el ambito de la
 * funcion anidada, el identificador `func` deja de ser la variable y pasa a
 * resolverse como el TIPO.  El mensaje apunta a una cabecera que no tiene
 * nada que ver y a una linea correcta.  Se arregla solo al sacar la
 * funcion. */
static void remove_outgoing_links(PowerPC_func_node** node, PowerPC_func* func){
	if(!*node) return;
	if((*node)->left) remove_outgoing_links(&(*node)->left, func);
	if((*node)->right) remove_outgoing_links(&(*node)->right, func);

	// Remove any links this function has which point in the code
	PowerPC_func_link_node** link, ** next;
	for(link = &(*node)->function->links_in; *link != NULL; link = next){
		next = &(*link)->next;
		if((*link)->func == func){
			PowerPC_func_link_node* tmp = (*link)->next;
			MetaCache_Free(*link);
			*link = tmp;
			next = link;
		}
	}
	MetaCache_Free(*node); // Free the PowerPC_func_node*
}

/* [Xbox360] La primera mitad de unlink_func, tal cual, sacada para
 * RecompCache_UnlinkIn (las variantes de TLB, Wrappers.c): los enlaces que
 * ENTRAN.  unlink_func la llama igual que antes. */
static void unlink_in(PowerPC_func* func){
	// Remove any incoming links to this func
	PowerPC_func_link_node* link, * next_link;
	for(link = func->links_in; link != NULL; link = next_link){
		next_link = link->next;

		/* [Xbox360] ★★ SONDA DEL DESENLACE, FILTRADA POR EL DEFECTO.
		 *
		 * Estas cinco lineas de abajo son lo mas peligroso que hace el
		 * dynarec: PARCHEAN CODIGO YA EMITIDO, y encima a ciegas, en
		 * `branch-10` y `branch-9`, once palabras contadas hacia atras
		 * desde el salto.  Si `branch` no apunta donde se cree, esto
		 * escribe tres palabras en medio de otra funcion recompilada --
		 * y eso da a la vez un juego que calcula mal y un monton roto,
		 * que son los dos sintomas abiertos de DK64.
		 *
		 * ★ EL CONTROL POSITIVO ESTA EN `RecompCache_Link` (mas abajo):
		 * al enlazar escribe `__B(*src_instr, ...)`, o sea que mientras
		 * el enlace este sano el opcode primario de `*link->branch`
		 * TIENE que ser 18 (`b`).  Cualquier otra cosa significa que ya
		 * no estamos mirando el salto que se enlazo.
		 *
		 * ★ SIN TOPE Y SIN FILTRO POR FRECUENCIA: en condiciones sanas
		 * esta sonda no imprime NADA en toda la sesion.  Si imprime
		 * miles de lineas, eso ES el hallazgo. */
#if XBOX64_SONDA_DESENLACE
		{
			const char* lo = (const char*)RECOMP_CACHE_LO;
			const char* hi = lo + RECOMP_CACHE_SIZE + RECOMP_CACHE_ISLA;
			const char* pri = (const char*)(link->branch - 10);
			const char* fin = (const char*)(link->branch + 1);
			unsigned int opcode = ((unsigned int)*link->branch) >> 26;
			if(pri < lo || fin > hi || opcode != 18){
				sprintf(txtbuffer,
				        "[DYNA] * DESENLACE FUERA DE SITIO: branch=%08X op=%u "
				        "(esperado 18) func=%08X arena=%08X..%08X",
				        (unsigned int)link->branch, opcode,
				        (unsigned int)func, (unsigned int)lo, (unsigned int)hi);
				DEBUG_print(txtbuffer, DBG_USBGECKO);
			}
		}
#endif
		_ORI(*(link->branch-10), 0, 0, 0);
		_ORI(*(link->branch-9), 0, 0, 0);
		_BLR(*link->branch, 1); // Set the linking branch to blrl
		DCFlushRange(link->branch-10, 11*sizeof(PowerPC_instr));
		ICInvalidateRange(link->branch-10, 11*sizeof(PowerPC_instr));
		
		remove_func(&link->func->links_out, func);
		MetaCache_Free(link);
	}
	func->links_in = NULL;
}

static void unlink_func(PowerPC_func* func){
//	start_section(UNLINK_SECTION);

	unlink_in(func);

	// Remove any references to outgoing links from this func
	remove_outgoing_links(&func->links_out, func);
	func->links_out = NULL;
	
//	end_section(UNLINK_SECTION);
}

static void free_func(PowerPC_func* func, unsigned int addr){
	/* [Xbox360] [Conker, log 14] Que `dynarec()` no enlace desde ella si era
	 * `last_func`: ver `note_freed_func` en ppc/Recompile.c. */
	note_freed_func(func);
	/* [Xbox360] [JR rapido] Ningun JR/JALR puede saltar ya a ella por la tabla
	 * (xb_jr_tabla, ppc/Wrappers.h).  Antes de liberar: lee su rango. */
	dynarec_jr_olvida_func(func);
	// Free the code associated with the func
	__lwp_heap_free(cache, func->code);
	MetaCache_Free(func->code_addr);
	// Remove any holes into this func
	PowerPC_func_hole_node* hole, * next_hole;
	for(hole = func->holes; hole != NULL; hole = next_hole){
		next_hole = hole->next;
		free(hole);
	}

	// Remove any pointers to this code
	// Remove the func from the block.
	/* [Xbox360] [Castlevania, variantes de TLB] Del arbol de SU bloque, que
	 * puede ser una variante apartada y no `blocks[addr>>12]`.  Sin dueno
	 * apuntado, como siempre. */
	if (func->xb_owner)
		remove_func(&func->xb_owner->funcs, func);
	else if (blocks[addr>>12])
		remove_func(&blocks[addr>>12]->funcs, func);
	// Remove func links
	unlink_func(func);

	free(func);
}

static inline void update_lru(PowerPC_func* func){
	func->lru = r4300.nextLRU++;

    // Handle LRU overflow
    if (!r4300.nextLRU || r4300.nextLRU > 0x80000000)
    {
        // Rebuild heap ordering
        heapify();

        // Pop everything into the scratch buffer (ARAM/MEM2)
        int savedSize = heapSize;
        int i = 0;

        while (heapSize > 0)
        {
            CacheMetaNode* n = heapPop();
            LRU_SCRATCH[i] = n;
            n->func->lru = i; // new LRU
            i++;
        }

        // Copy back into the real heap
        for (i = 0; i < savedSize; i++)
            cacheHeap[i] = LRU_SCRATCH[i];

        heapSize = savedSize;
        r4300.nextLRU = savedSize;
    }
}

void release(int minNeeded){
	// Frees alloc'ed blocks so that at least minNeeded bytes are available
	int toFree = minNeeded * 2; // Free 2x if we're here due to meta filling up

	/* [Xbox360] ★ SONDA DE LA RUTA DE DESALOJO.
	 *
	 * Es la ruta que Super Mario 64 probablemente NO toca y Donkey Kong 64
	 * si, porque llena antes los 7 MB -- y esa es justo la diferencia entre
	 * el juego que va y el que revienta.  Tambien es, con diferencia, la
	 * menos probada: libera funciones, deshace enlaces y PARCHEA codigo ya
	 * emitido.
	 *
	 * ★ SIN TOPE, y no hace falta ponerlo: `release` solo deberia correr
	 * cuando la cache se llena.  Si resulta que sale cientos de veces por
	 * segundo, ESO ES EL HALLAZGO y no un problema de la sonda. */
#if XBOX64_TRAZAS
	sprintf(txtbuffer, "[DYNA] release min=%d cacheSize=%d heapSize=%u",
	        minNeeded, cacheSize, heapSize);
	DEBUG_print(txtbuffer, DBG_USBGECKO);
#endif

	if ((minNeeded * 2) + cacheSize > RECOMP_CACHE_SIZE) {
		toFree = minNeeded * 2 > MIN_RELEASE_SIZE ? minNeeded * 2 : MIN_RELEASE_SIZE; // Free more when we've actually hit capacity.
	}
	// Restore the heap properties to pop the LRU
	heapify();
	// Release nodes' memory until we've freed enough
	while(toFree > 0 && cacheSize){
		// Pop the LRU to be freed
		CacheMetaNode* n = heapPop();
		// Free the function it contains
		free_func(n->func, n->addr);
		toFree    -= n->size;
		cacheSize -= n->size;
		// And the cache node itself
		__lwp_heap_free(node_heap, n);
	}
}

void RecompCache_Alloc(unsigned int size, unsigned int address, PowerPC_func* func){
	/* [Xbox360] casts: fichero compilado como C++ */
	CacheMetaNode* newBlock = (CacheMetaNode*)__lwp_heap_allocate(node_heap, sizeof(CacheMetaNode));

	if (!newBlock) {
		release(size);
		newBlock = (CacheMetaNode*)__lwp_heap_allocate(node_heap, sizeof(CacheMetaNode));
	}
	newBlock->addr = address;
	newBlock->size = size;
	newBlock->func = func;

	// Allocate new memory for this code
	void* code = __lwp_heap_allocate(cache, size);
	while(!code){
		release(size);
		code = __lwp_heap_allocate(cache, size);
	}
	int num_instrs = (func->end_addr - func->start_addr) >> 2;
	void* code_addr = MetaCache_Alloc(num_instrs * sizeof(void*));

	cacheSize += size;
	newBlock->func->code = (PowerPC_instr*)code;
	newBlock->func->code_addr = (PowerPC_instr**)code_addr;
	// Add it to the heap
	heapPush(newBlock);
	// Make this function the LRU
	update_lru(func);
}

void RecompCache_Realloc(PowerPC_func* func, unsigned int new_size){
	// There should be no need for the code to be preserved
	__lwp_heap_free(cache, func->code);
	func->code = (PowerPC_instr*)__lwp_heap_allocate(cache, new_size);
	while(!func->code){
		release(new_size);
		func->code = (PowerPC_instr*)__lwp_heap_allocate(cache, new_size);
	}
	
	// Update the size for the cache
	int i;
	for(i=heapSize-1; i>=0; --i){
		if(cacheHeap[i]->func == func){
			cacheSize += new_size - cacheHeap[i]->size;
			cacheHeap[i]->size = new_size;
			break;
		}
	}
	
	// Remove any func links since the code has changed
	unlink_func(func);
}

void RecompCache_Free(unsigned int addr){
	int i;
	CacheMetaNode* n = NULL;
	// Find the corresponding node
	for(i=heapSize-1; i>=0; --i){
		if(cacheHeap[i]->addr == addr){
			n = cacheHeap[i];
			// Remove from the heap
			heapSwap(i, --heapSize);
			// Free n's func
			free_func(n->func, addr);
			cacheSize -= n->size;
			// Free the cache node
			__lwp_heap_free(node_heap, n);
			return;
		}
	}
}

/* [Xbox360] Ver Recomp-Cache.h.  La misma busqueda que RecompCache_Free, por
 * el puntero de la funcion en vez de por su direccion. */
void RecompCache_FreeFunc(PowerPC_func* func){
	int i;
	CacheMetaNode* n = NULL;
	for(i=heapSize-1; i>=0; --i){
		if(cacheHeap[i]->func == func){
			n = cacheHeap[i];
			heapSwap(i, --heapSize);
			free_func(n->func, n->addr);
			cacheSize -= n->size;
			__lwp_heap_free(node_heap, n);
			return;
		}
	}
}

void RecompCache_UnlinkIn(PowerPC_func* func){
	unlink_in(func);
}

void RecompCache_Update(PowerPC_func* func){
	update_lru(func);
}

void RecompCache_Link(PowerPC_func* src_func, PowerPC_instr* src_instr,
                      PowerPC_func* dst_func, PowerPC_instr* dst_instr){
//	start_section(LINK_SECTION);
	
	// Setup book-keeping
	// Create the incoming link info
	PowerPC_func_link_node* fln = (PowerPC_func_link_node*)
		MetaCache_Alloc(sizeof(PowerPC_func_link_node));
	fln->branch = src_instr;
	fln->func = src_func;
	fln->next = dst_func->links_in;
	dst_func->links_in = fln;
	// Create the outgoing link info
	insert_func(&src_func->links_out, dst_func);
	
	// Actually link the funcs
	_LIS(*(src_instr-10), DYNAREG_FUNC, (unsigned int)dst_func>>16);
	_ORI(*(src_instr-9), DYNAREG_FUNC,DYNAREG_FUNC, (unsigned int)dst_func);
	__B(*src_instr, (PowerPC_instr*)dst_instr-src_instr, 0, 0);
	DCFlushRange(src_instr-10, 11*sizeof(PowerPC_instr));
	ICInvalidateRange(src_instr-10, 11*sizeof(PowerPC_instr));
	
//	end_section(LINK_SECTION);
}

void RecompCache_Init(void){
	if(!cache){
		cache = (heap_cntrl*)memalign(32,sizeof(heap_cntrl));
		/* [Xbox360] ★ LA ARENA SALE DE UN ARRAY ESTATICO, NO DEL MONTON.
		 * Lo que devuelve `memalign` en esta consola SE LEE PERO NO SE
		 * EJECUTA -- medido: el primer salto al codigo recompilado murio
		 * con `Access violation` en la direccion exacta que devolvio, y
		 * las cuatro primeras palabras de ahi se habian leido bien y eran
		 * instrucciones PPC validas.  La nota larga y el patron de
		 * picodrive estan en xbox/xbox_mem.h.
		 *
		 * ★ Es ademas LA MISMA FORMA que usa la linea de `meta_cache` aqui
		 * abajo, que ya monta el monton sobre `(void*)RECOMPMETA_LO`. */
#if defined(_XBOX)
		/* ★ El monton arranca DESPUES de la isla de trampolines, que vive
		 * en la cabecera de la arena.  Si empezara en RECOMP_CACHE_LO, el
		 * primer bloque que se compile se escribiria encima de los
		 * trampolines y las llamadas a C saltarian a datos.  El porque de
		 * la isla esta en ppc/Recompile.c, junto a `dyna_get_stub`.
		 *
		 * ★ Y EL TAMANYO ES `RECOMP_CACHE_SIZE` ENTERO, no menos la isla:
		 * el array mide `RECOMP_CACHE_SIZE + RECOMP_CACHE_ISLA` para que el
		 * presupuesto que maneja `release()` cuadre con lo que el monton
		 * tiene de verdad.  La nota esta en xbox/xbox_mem.h. */
		__lwp_heap_init(cache, (void*)((char*)RECOMP_CACHE_LO + DYNA_STUB_BYTES),
		                RECOMP_CACHE_SIZE, 32);
#else
		__lwp_heap_init(cache, memalign(32,RECOMP_CACHE_SIZE),
		                RECOMP_CACHE_SIZE, 32);
#endif
#ifdef SHOW_DEBUG
		DEBUG_registerHeap(cache, "RC");
#endif
	}
	if(!meta_cache){
		meta_cache = (heap_cntrl*)memalign(32,sizeof(heap_cntrl));
		__lwp_heap_init(meta_cache, (void*)RECOMPMETA_LO,
		                RECOMPMETA_SIZE, 32);
#ifdef SHOW_DEBUG
		DEBUG_registerHeap(meta_cache, "M");
#endif
	}
	if (!node_heap) {
        node_heap = (heap_cntrl*)memalign(32, sizeof(heap_cntrl));
        __lwp_heap_init(node_heap,
                        memalign(32, NODE_HEAP_SIZE),
                        NODE_HEAP_SIZE,
                        32);
#ifdef SHOW_DEBUG
		DEBUG_registerHeap(node_heap, "N");
#endif
    }

    if (!cacheHeap) {
        cacheHeap = (CacheMetaNode**)__lwp_heap_allocate(node_heap,
                                        MAX_HEAP_ENTRIES * sizeof(void*));
        maxHeapSize = MAX_HEAP_ENTRIES;
    }

	heapify();
	while (heapSize) {
		CacheMetaNode* n = heapPop();
		if (n) {
			free_func(n->func, n->addr);
			__lwp_heap_free(node_heap, n);
		}
	}
	cacheSize = 0;
}

void* MetaCache_Alloc(unsigned int size){
	void* ptr = __lwp_heap_allocate(meta_cache, size);
	// While there's no room to allocate, call release
	while(!ptr){
		release(size);
		ptr = __lwp_heap_allocate(meta_cache, size);
	}
	
	return ptr;
}

void MetaCache_Free(void* ptr){
	__lwp_heap_free(meta_cache, ptr);
}
