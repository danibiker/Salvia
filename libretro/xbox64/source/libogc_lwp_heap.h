#ifndef __LWP_HEAP_H__
#define __LWP_HEAP_H__

#include <xetypes.h>

/* [Xbox360] TRUE/FALSE venian de gctypes.h de libogc, que aqui no existe.
 * Con guarda por si algun dia entra windows.h/xtl.h en la misma unidad. */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define PPC_ALIGNMENT					16

#define HEAP_BLOCK_USED					1
#define HEAP_BLOCK_FREE					0

#define HEAP_DUMMY_FLAG					(0+HEAP_BLOCK_USED)

#define HEAP_OVERHEAD					(sizeof(u32)*2)
#define HEAP_BLOCK_USED_OVERHEAD		(sizeof(void*)*2)
#define HEAP_MIN_SIZE					(HEAP_OVERHEAD+sizeof(heap_block))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _heap_block_st heap_block;
struct _heap_block_st {
	u32 back_flag;
	u32 front_flag;
	heap_block *next;
	heap_block *prev;
};

typedef struct _heap_iblock_st {
	u32 free_blocks;
	u32 free_size;
	u32 used_blocks;
	u32 used_size;
} heap_iblock;

typedef struct _heap_cntrl_st {
	heap_block *start;
	heap_block *final;

	heap_block *first;
	heap_block *perm_null;
	heap_block *last;
	u32 pg_size;
	u32 reserved;
} heap_cntrl;

/* [Xbox360] Aviso de liberacion.  Lo necesita el backend GX->D3D9 de
 * glN64_GX: cachea una textura de D3D por cada buffer de texels del plugin y
 * la clave es el PUNTERO, pero el plugin desaloja con __lwp_heap_free y el
 * monton REUTILIZA la direccion para otra textura -- asi se servia la textura
 * vieja y el contador de conversiones se quedaba clavado.
 *
 * El plugin no da ningun otro momento donde enterarse: no llama a
 * GX_InvalidateTexAll al desalojar (solo desde FrameBuffer.cpp y VI.cpp) y
 * DCFlushRange solo cubre el camino de 2xSaI.
 *
 * El gancho recibe el monton para que el oyente descarte en O(1) los frees
 * que no son suyos: por aqui pasa tambien el RecompCache del dynarec, que
 * libera en caliente. */
extern void (*lwp_heap_free_hook)(heap_cntrl *theheap, void *ptr);

/* [Xbox360] Reservas que NO CABEN (y el total, para leerlas en proporcion).
 * Una que falla deja una textura sin texels, o sea invisible -- fue la causa
 * de los personajes negros de Paper Mario.  Ver la nota en el .c. */
extern u32 lwp_heap_fallos;
extern u32 lwp_heap_reservas;

u32 __lwp_heap_init(heap_cntrl *theheap,void *start_addr,u32 size,u32 pg_size);
void* __lwp_heap_allocate(heap_cntrl *theheap,u32 size);
/* [Xbox360] BOOL venia de gctypes.h de libogc, que aqui no existe. */
int __lwp_heap_free(heap_cntrl *theheap,void *ptr);
u32 __lwp_heap_getinfo(heap_cntrl *theheap,heap_iblock *theinfo);

#include "libogc_lwp_heap.inl"

#ifdef __cplusplus
	}
#endif

#endif
