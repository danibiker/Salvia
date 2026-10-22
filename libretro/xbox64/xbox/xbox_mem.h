/*
 * xbox/xbox_mem.h  --  el mapa de memoria de Wii64, en Xbox 360.
 *
 * ★ EL HALLAZGO QUE MOTIVA ESTE FICHERO.
 *
 * Wii64 no RESERVA los buferes del emulador: los COLOCA en direcciones
 * fijas de la MEM2 de la Wii (gc_memory/MEM2.h, desde 0x90000000) o de la
 * ARAM de la GameCube (gc_memory/ARAM.h, desde 0x7F000000).  El patron es
 * siempre el mismo:
 *
 *     static unsigned char *const sram = (unsigned char*)(SRAM_LO);
 *
 * o sea un puntero CONSTANTE inicializado con una direccion literal.  En
 * Xbox 360 esas direcciones no existen y escribir ahi es corrupcion pura.
 *
 * Y es un modo de fallo silencioso de los malos: como `#ifdef HW_RVL` es
 * falso, los seis ficheros de abajo caen en la rama `#else` e incluyen
 * ARAM.h, que compila perfectamente y les da punteros a 0x7F00xxxx.
 *
 * LA LISTA ENTERA, censada sobre lo que entra en el build:
 *
 *   macro                           quien                         tamanyo
 *   INVCODE_LO                      r4300/Invalid_Code.c:11         1 MB
 *   TLBLUT_LO                       gc_memory/tlb.c:46,47           8 MB
 *   SRAM_LO                         gc_memory/dma.c:62             32 KB
 *   FLASHRAM_LO                     gc_memory/flashram.c:47       128 KB
 *   MEMPACK_LO                      gc_memory/pif.c:58            128 KB
 *   BLOCKS_LO                       r4300/r4300.c:64,67             4 MB  (fase 3)
 *   RECOMPMETA_LO                   Recomp-Cache-Heap.c:357         8 MB  (fase 3)
 *   RECOMP_CACHE_HEAP_SCRATCH_LO    Recomp-Cache-Heap.c:58         64 KB  (fase 3)
 *   TEXCACHE_LO / TEXCACHE_SIZE     glN64_GX/Textures.cpp:687      15 MB  (fase 2)
 *                                   glN64_GX/VI.cpp:459
 *   ROMCACHE_LO / ROMCACHE_SIZE     main/ROM-Cache.c  -> resuelto en su
 *                                   propia rama [Xbox360], con malloc
 *
 * ★ POR QUE ARRAYS Y NO malloc.  Porque los usuarios escriben
 * `unsigned char *const sram = (unsigned char*)(SRAM_LO);` a nivel de
 * fichero, y en C89 el inicializador de un objeto estatico tiene que ser
 * una CONSTANTE de direccion.  El nombre de un array lo es; una variable
 * puntero rellenada en tiempo de ejecucion no.  Con malloc habria que
 * tocar los seis ficheros; con arrays, ninguno.
 *
 * ★ POR QUE SE ENGANCHA DESDE MEM2.h Y ARAM.h Y NO DESDE CADA .c.  Los
 * seis usuarios eligen cabecera con `#ifdef HW_RVL` ... salvo `tlb.c:45`,
 * que incluye MEM2.h A PELO bajo `#ifndef USE_TLB_CACHE`.  Con las dos
 * cabeceras reenviando aqui, los seis quedan cubiertos sin editar ninguno,
 * y da igual cual de las dos les toque.
 *
 * ★ TIENE QUE SER VALIDO EN C.
 */

#ifndef _XBOX64_MEM_H_
#define _XBOX64_MEM_H_

#include "xbox64_fase.h"

#ifndef MB
#define MB (1024*1024)
#endif
#ifndef KB
#define KB (1024)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Los tamanyos son los de MEM2.h, no los de ARAM.h.                   */
/* ------------------------------------------------------------------ */
/* MEM2.h es el mapa del build de Wii, que es el que seguimos.  ARAM.h
 * recorta varias cosas para caber en 16 MB (RECOMPMETA 2 MB en vez de 4,
 * por ejemplo); aqui no hay motivo para recortar. */

#define INVCODE_SIZE   (1*MB)
#define TLBLUT_SIZE    (8*MB)
#define SRAM_SIZE      (32*KB)
#define FLASHRAM_SIZE  (128*KB)
#define MEMPACK_SIZE   (128*KB)
#define BLOCKS_SIZE    (4*MB)
/* [Perfect Dark] 8 MB, no 4: crece con la arena de codigo (RECOMP_CACHE_SIZE,
 * de 7 a 16 MB).  Cada funcion recompilada pide aqui `num_instrs * 4` bytes
 * (MetaCache_Alloc); con 7 MB de codigo no fallaba nunca (log 12: 0 desalojos
 * con origen=4), pero con el doble de codigo vivo seria el siguiente tope. */
#define RECOMPMETA_SIZE (8*MB)
#define RECOMP_CACHE_HEAP_SCRATCH_SIZE (64*KB)
#define TEXCACHE_SIZE  (15*MB)

#define EEPROM_SIZE    (2*KB)

extern unsigned char xbox64_mem_invcode  [INVCODE_SIZE];
extern unsigned char xbox64_mem_tlblut   [TLBLUT_SIZE];

/* ====================================================================== */
/* ★★ LOS CUATRO GUARDADOS, EN UN SOLO BLOQUE CONTIGUO                    */
/* ====================================================================== */
/* Antes eran tres arrays sueltos (sram, flashram, mempack) mas un
 * `static unsigned char eeprom[0x800]` dentro de pif.c.  Ahora son los
 * cuatro campos de UNA estructura, y el motivo es concreto:
 *
 * libretro entrega la memoria de guardado al frontend como UN puntero y
 * UN tamanyo (`retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)`).  El
 * frontend lee y escribe ESE bloque cuando quiere -- periodicamente y al
 * salir -- sin volver a preguntar.  Para eso tiene que ser contiguo y
 * tiene que estar quieto: nada de copiar a un bufer intermedio, porque
 * entonces habria dos verdades y la del frontend seria la vieja.
 *
 * ★ EL ORDEN NO ES ARBITRARIO: es el de mupen64plus-next
 * (eeprom, mempack, sram, flashram), asi que un `.srm` generado aqui se
 * puede llevar a ese core y al reves.  Cambiar el orden no rompe nada
 * hoy, y rompe eso.
 *
 * ★ LOS TAMANYOS TAMPOCO SE TOCAN: son los que el N64 define, no una
 * eleccion nuestra.  El eeprom son 2 KB porque es el tope (4 Kbit o 16
 * Kbit segun el cartucho, y `ROM_SETTINGS.eeprom_16kb` decide cual se le
 * ensenya al juego); se guarda el bufer entero de todas formas. */
struct xbox64_guardado
{
	unsigned char eeprom  [EEPROM_SIZE];
	unsigned char mempack [MEMPACK_SIZE];
	unsigned char sram    [SRAM_SIZE];
	unsigned char flashram[FLASHRAM_SIZE];
};

extern struct xbox64_guardado xbox64_mem_guardado;

#define INVCODE_LO   (xbox64_mem_invcode)
#define TLBLUT_LO    (xbox64_mem_tlblut)
#define EEPROM_LO    (xbox64_mem_guardado.eeprom)
#define SRAM_LO      (xbox64_mem_guardado.sram)
#define FLASHRAM_LO  (xbox64_mem_guardado.flashram)
#define MEMPACK_LO   (xbox64_mem_guardado.mempack)

/* ------------------------------------------------------------------ */
/* La tabla de bloques                                                 */
/* ------------------------------------------------------------------ */
/* ★ VA CON PPC_DYNAREC, NO CON XBOX64_DYNAREC, Y NO ES LO MISMO.
 *
 * `r4300.c:64` define `blocks` como `(PowerPC_block**)(BLOCKS_LO)` bajo
 * `#ifdef PPC_DYNAREC`, y PPC_DYNAREC esta definido SIEMPRE en este
 * proyecto -- no porque haya dynarec, sino porque la rama `#else` de
 * Wii64 no existe: usa el tipo `precomp_block`, que en este arbol no se
 * declara en ningun sitio (Wii64 borro el recompilador x86 de mupen64 y
 * con el su typedef).  Ver el comentario de XBOX64_DYNAREC en
 * xbox64_fase.h.
 *
 * O sea: la tabla hace falta desde la fase 1 aunque el dynarec no se
 * ejecute.  `init_blocks()` la recorre entera, y solo se llama desde la
 * rama de `go()` que con dynacore==2 no se toma -- pero el simbolo tiene
 * que existir.
 *
 * 4 MB / sizeof(void*) = 1M entradas, que es justo el rango de
 * `blocks[address>>12]`.  Recortarlo hace que el indice se salga sin
 * aviso. */
extern unsigned char xbox64_mem_blocks[BLOCKS_SIZE];
#define BLOCKS_LO (xbox64_mem_blocks)

/* ------------------------------------------------------------------ */
/* Fase 3: la cache del recompilador                                   */
/* ------------------------------------------------------------------ */
/* Estos dos SI van con XBOX64_DYNAREC: los usa solo
 * `Recomp-Cache-Heap.c` (:58 y :357), que no entra en el build hasta la
 * fase 3.  Son 4 MB que en la fase 1 no toca nadie, y tenerlos fuera hace
 * que un uso prematuro salga como LNK2001 en vez de como BSS silencioso. */
#if XBOX64_DYNAREC
extern unsigned char xbox64_mem_recompmeta[RECOMPMETA_SIZE];
extern unsigned char xbox64_mem_rcscratch [RECOMP_CACHE_HEAP_SCRATCH_SIZE];

#define RECOMPMETA_LO                (xbox64_mem_recompmeta)
#define RECOMP_CACHE_HEAP_SCRATCH_LO (xbox64_mem_rcscratch)

/* ------------------------------------------------------------------ */
/* ★★ LA ARENA DEL CODIGO RECOMPILADO.  AQUI Y NO EN EL MONTON.        */
/* ------------------------------------------------------------------ */
/* Wii64 la pide con `memalign(32, RECOMP_CACHE_SIZE)`
 * (Recomp-Cache-Heap.c:368).  En Xbox 360 ESO SE PUEDE LEER PERO NO SE
 * PUEDE EJECUTAR, y esta medido:
 *
 *   [DYNA] 1/2 code=4331DA40                          <- de _aligned_malloc
 *   [DYNA] 2/2 code[0..3]= 93FC015C 38A00000 ...      <- SE LEE, y son
 *                                                        instrucciones PPC
 *                                                        validas
 *   Access violation reading location 0x4331DA40      <- al saltar ahi
 *
 * O sea que ni el recompilador ni el trampolin estaban mal: el bufer
 * sencillamente no es ejecutable.  Fijarse en el rango: `code` cae en
 * 0x43xxxxxx mientras `r4300` y `rdram`, que son datos de la IMAGEN, estan
 * en 0x84/0x85xxxxxx.  El monton de la CRT sirve las reservas grandes
 * desde otro sitio.
 *
 * ★ LA REFERENCIA QUE FUNCIONA EN ESTE MISMO XDK:
 *   `libretro/picodrive/cpu/drc/cmn.c:26`
 *       __declspec(align(128)) u8 tcache_default[DRC_TCACHE_SIZE];
 * un array estatico, o sea DENTRO de la imagen.  Su DRC de SH2 ejecuta
 * desde ahi.
 *
 * ★ Y ES ADEMAS LO QUE YA HACE SU HERMANO: dos lineas mas abajo, Wii64
 * monta `meta_cache` sobre `(void*)RECOMPMETA_LO`, que aqui ya es un array
 * estatico.  El cambio pone las dos mitades de la cache en el mismo sitio.
 *
 * ★★ CUIDADO SI ALGUN DIA SE CAMBIA EL TAMANYO: quien manda es ESTA
 * definicion.  `Recomp-Cache.h` la respeta con un `#ifndef`, de modo que
 * el array y el tope que usa `release()` no pueden separarse.  Si se
 * separasen, `release()` creeria tener mas sitio del que hay y escribiria
 * fuera del array -- corrupcion silenciosa de lo que haya detras.
 *
 * ★ [Perfect Dark, parones al disparar a un enemigo] 16 MB, NO 7.  Medido con
 * la sonda `origen=` de `[DYNA] release` (log 12): 2313 de 2327 desalojos
 * son de la reserva de CODIGO, casi todos con la arena a 5-6,5 MB de 7 -- no
 * esta llena: esta FRAGMENTADA --, y cada reserva fallida desaloja de media 4
 * veces (hasta 19), o sea hasta 1,7 MB de funciones sueltas que luego se
 * recompilan.  En el log 11 eso fue 100-217 desalojos/s y 3-14 img/s.
 * Wii64 en Wii usa 9 MB con glN64 (Recomp-Cache.h); alli el limite es la
 * memoria de la consola, aqui no.  El tope de verdad es el `bl` hasta la
 * isla de trampolines, que esta al PRINCIPIO de la arena: +-32 MB.  Lo
 * comprueba el typedef de abajo.  Confirmado (log 13): 0 desalojos y sin
 * parones al disparar. */
#define RECOMP_CACHE_SIZE (16*MB)

/* ★ LA ISLA DE TRAMPOLINES VA **ENCIMA** DE LOS 7 MB, NO DENTRO, y esto es
 * el arreglo de un defecto mio.
 *
 * La primera version le daba al monton `RECOMP_CACHE_SIZE - isla` y dejaba
 * que `release()` y `RecompCache_Alloc` siguieran comparando `cacheSize`
 * contra `RECOMP_CACHE_SIZE` entero.  O sea que la contabilidad creia
 * tener 1 KB mas de los que habia: el asignador podia agotarse ANTES de
 * que la cuenta llegara al tope.  Eso no corrompe -- el codigo reintenta
 * con `release()` -- pero mete una discrepancia gratis justo en la ruta de
 * desalojo, que es la menos probada.  Con la isla fuera del presupuesto,
 * el monton recibe EXACTAMENTE `RECOMP_CACHE_SIZE` y la cuenta cuadra.
 *
 * ★ Tiene que valer lo mismo que `DYNA_STUB_BYTES` (ppc/Recompile.h).  No
 * se puede incluir esa cabecera desde aqui (seria circular), asi que
 * Recomp-Cache-Heap.c -- que ve las dos -- lo comprueba EN COMPILACION. */
#define RECOMP_CACHE_ISLA 1024

/* El `bl` de PowerPC alcanza +-32 MB (24 bits x4): todo bloque de la arena
 * tiene que llegar a la isla de su principio.  Si se pasa, no compila. */
typedef char xbox64_arena_cabe_en_un_bl[((RECOMP_CACHE_SIZE + RECOMP_CACHE_ISLA) < (32*MB)) ? 1 : -1];

extern unsigned char xbox64_mem_recompcache[RECOMP_CACHE_SIZE + RECOMP_CACHE_ISLA];
#define RECOMP_CACHE_LO (xbox64_mem_recompcache)
#endif

/* ------------------------------------------------------------------ */
/* Fase 2: la cache de texturas de glN64_GX                            */
/* ------------------------------------------------------------------ */
#if !XBOX64_GFX_NULO
extern unsigned char xbox64_mem_texcache[TEXCACHE_SIZE];
#define TEXCACHE_LO (xbox64_mem_texcache)

/* [Wii64 443f68b] La copia de profundidad de glN64_GX (DepthCopy.cpp): el
 * bufer donde `GX_CopyTex(GX_TF_Z16)` deja la profundidad del EFB a 320x240
 * y la tabla de 64K entradas que la convierte.  Mismos tamanos que en
 * MEM2.h de upstream; aqui son arrays, como el resto de este fichero.
 * [Xbox360] [Star Wars Episode I Racer, logs (32)] A 320x480: en carrera el
 * VI pasa a 320x480 (entrelazado) y con 320x240 DepthCopy_Active la apagaba
 * (medido con una sonda ya retirada): el sol se veia a traves de las
 * paredes; confirmado arreglado por el usuario.  Con el EFB
 * a 640 de ancho (escala horizontal 2, la unica que admite la copia) el VI no
 * pasa de 320, asi que 480 filas es el maximo; 150 KB mas de .bss. */
#define DEPTHCOPY_BUF_SIZE (320*480*2)
#define DEPTHCOPY_LUT_SIZE (65536*2)
#define DEPTHCOPY_SIZE     (DEPTHCOPY_BUF_SIZE + DEPTHCOPY_LUT_SIZE)
extern unsigned char xbox64_mem_dcbuf[DEPTHCOPY_BUF_SIZE];
extern unsigned char xbox64_mem_dclut[DEPTHCOPY_LUT_SIZE];
#define DEPTHCOPY_BUF_LO (xbox64_mem_dcbuf)
#define DEPTHCOPY_LUT_LO (xbox64_mem_dclut)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _XBOX64_MEM_H_ */
