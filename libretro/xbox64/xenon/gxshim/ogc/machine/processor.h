/*
 * ogc/machine/processor.h  --  shim.
 *
 * glN64_GX lo incluye SOLO desde Textures.cpp y solo usa `_sync()` y
 * `DCFlushRange()`.
 *
 * ★ Pero esas dos NO viven aqui: viven en `gccore.h`.  La razon es que
 * `FrameBuffer.cpp:259` y `:399` llaman a `DCInvalidateRange` y ese fichero
 * NO incluye processor.h -- solo gccore.h.  En libogc pasa lo mismo: las
 * funciones de cache las trae `<ogc/cache.h>`, que arrastra gccore.h.
 *
 * Tenerlas aqui fue mi primera version y dejo a FrameBuffer.cpp sin ellas
 * (`error C3861: 'DCInvalidateRange': identifier not found`).  Asi que este
 * fichero pasa a ser un reenvio, y queda una sola definicion.
 */

/*
 * [Xbox360, xbox64] ★ AMPLIADO RESPECTO AL SHIM HEREDADO.
 *
 * Al portar Wii64 entero aparecio un segundo usuario que el arbol anterior
 * no tenia: `source/main/rom_gc.c:35`, que ademas es .c.  Eso obliga a dos
 * cosas que el reenvio de arriba no cumplia:
 *
 *   1. NO PUEDE INCLUIR gccore.h INCONDICIONALMENTE.  gccore.h tiene
 *      construcciones de C++ (los `operator=` de las estructuras GX, ver
 *      gccore.h:765) y rom_gc.c compila como C.  Asi que el reenvio se
 *      condiciona a __cplusplus, que es exactamente el caso de los dos
 *      usuarios de glN64 (Textures.cpp y, por la via de gccore.h,
 *      FrameBuffer.cpp).  Su comportamiento no cambia.
 *
 *   2. HAY QUE DAR __lwbrx Y __lhbrx.  rom_gc.c:83 y :86 las usan para el
 *      byteswap de la ROM.  En libogc son asm en linea (`lwbrx`/`lhbrx`);
 *      el XDK trae las mismas instrucciones como intrinsecos.
 *
 *      ★ OJO AL ORDEN DE LOS ARGUMENTOS, QUE ESTA INVERTIDO:
 *          libogc  __lwbrx(base, index)
 *          XDK     __loadwordbytereverse(offset, base)
 *      Confundirlos no da error de compilacion -- los dos son enteros y
 *      punteros que se convierten -- sino una ROM leida desde una
 *      direccion absurda.
 */

#ifndef _XENON_GXSHIM_OGC_MACHINE_PROCESSOR_H_
#define _XENON_GXSHIM_OGC_MACHINE_PROCESSOR_H_

#ifdef __cplusplus
#include <gccore.h>
#endif

#include <ppcintrinsics.h>

/* libogc: ogc/machine/processor.h.  Cargas de 32 y 16 bits con los bytes
 * invertidos, desde `base + index`. */
#define __lwbrx(base, index) __loadwordbytereverse((index), (base))
#define __lhbrx(base, index) __loadshortbytereverse((index), (base))

#endif
