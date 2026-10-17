#ifndef __LWP_HEAP_INL__
#define __LWP_HEAP_INL__
/* [Xbox360, xbox64] ★ AQUI YA NO SE DEFINE `bool`.
 *
 * Este fichero traia su propio `typedef int bool;`, que era correcto cuando
 * era el UNICO sitio del arbol que lo definia.  Al portar Wii64 dejo de
 * serlo: `gc_memory/{pif,dma,flashram}.c` escriben `bool x = false;` sin
 * incluir nada -- en devkitPPC les llegaba por el <gctypes.h> de libogc --
 * y `rsp_hle/alist.h:25` pide <stdbool.h> a pelo.  Con dos definiciones, la
 * segunda se expandia sobre la primera:
 *
 *     typedef int bool;   +   #define bool unsigned char
 *     -> typedef int unsigned char;
 *     -> error C2632: 'int' followed by 'char' is illegal
 *
 * La fuente unica pasa a ser xbox/compat/stdbool.h.
 *
 * ★ Y CAMBIA EL TIPO BASE: de `int` a `unsigned char`.  El razonamiento
 * original (un bool de 1 byte trunca si alguien le asigna una mascara)
 * sigue siendo cierto y esta anotado en stdbool.h, pero pesa mas el
 * contrario: `bool` aparece en estructuras de rsp_hle que ve codigo C
 * mientras el wrapper es C++, y el `bool` de C++ en MSVC ocupa 1 byte.  Con
 * `int` los dos lados verian tamanyos distintos y el desajuste seria
 * silencioso. */
#include <stdbool.h>

static __inline heap_block* __lwp_heap_head(heap_cntrl *theheap)
{
	return (heap_block*)&theheap->start;
}

static __inline heap_block* __lwp_heap_tail(heap_cntrl *heap)
{
	return (heap_block*)&heap->final;
}

static __inline heap_block* __lwp_heap_prevblock(heap_block *block)
{
	return (heap_block*)((char*)block - (block->back_flag&~HEAP_BLOCK_USED));
}

static __inline heap_block* __lwp_heap_nextblock(heap_block *block)
{
	return (heap_block*)((char*)block + (block->front_flag&~HEAP_BLOCK_USED));
}

static __inline heap_block* __lwp_heap_blockat(heap_block *block,u32 offset)
{
	return (heap_block*)((char*)block + offset);
}

static __inline heap_block* __lwp_heap_usrblockat(void *ptr)
{
	u32 offset = *(((u32*)ptr)-1);
	/* [Xbox360] cast explicito: Recomp-Cache.c incluye esto como C++, donde
	 * no hay conversion implicita de void* a heap_block*. */
	return __lwp_heap_blockat((heap_block*)ptr,-offset+-HEAP_BLOCK_USED_OVERHEAD);
}

static __inline bool __lwp_heap_prev_blockfree(heap_block *block)
{
	return !(block->back_flag&HEAP_BLOCK_USED);
}

static __inline bool __lwp_heap_blockfree(heap_block *block)
{
	return !(block->front_flag&HEAP_BLOCK_USED);
}

static __inline bool __lwp_heap_blockused(heap_block *block)
{
	return (block->front_flag&HEAP_BLOCK_USED);
}

static __inline u32 __lwp_heap_blocksize(heap_block *block)
{
	return (block->front_flag&~HEAP_BLOCK_USED);
}

static __inline void* __lwp_heap_startuser(heap_block *block)
{
	return (void*)&block->next;
}

static __inline bool __lwp_heap_blockin(heap_cntrl *heap,heap_block *block)
{
	return ((u32)block>=(u32)heap->start && (u32)block<=(u32)heap->final);
}

static __inline bool __lwp_heap_pgsize_valid(u32 pgsize)
{
	return (pgsize!=0 && ((pgsize%PPC_ALIGNMENT)==0));
}

static __inline u32 __lwp_heap_buildflag(u32 size,u32 flag)
{
	return (size|flag);
}

#endif
