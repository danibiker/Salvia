/*
 * ogc/lwp_heap.h  --  shim.
 *
 * glN64_GX incluye <ogc/lwp_heap.h> en FrameBuffer.cpp, Textures.cpp y VI.cpp
 * y solo usa TRES funciones: __lwp_heap_init, __lwp_heap_allocate y
 * __lwp_heap_free.
 *
 * Este arbol YA tiene ese monton portado -- `source/libogc_lwp_heap.h` y su
 * `.c`, que usa el RecompCache del dynarec-- asi que el shim es un reenvio.
 */

#ifndef _XENON_GXSHIM_OGC_LWP_HEAP_H_
#define _XENON_GXSHIM_OGC_LWP_HEAP_H_

#include "../../../source/libogc_lwp_heap.h"

#endif
