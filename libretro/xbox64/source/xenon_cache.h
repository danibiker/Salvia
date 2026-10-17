/**
 * Xenon64 - xenon_cache.h
 *
 * Invalidacion de I-cache / D-cache para codigo auto-modificable en la CPU
 * Xenon (Xbox 360), compilando con el XDK / Visual Studio 2010.
 *
 * El arbol original llamaba a memdcbf()/memicbi() de libxenon, que no existen
 * bajo el XDK.  Aqui se reimplementa la secuencia correcta de PowerISA Book II
 * 1.4 (+ erratas de Xenon), la misma que ya usan dosbox-pure
 * (src/cpu/core_dynrec/risc_ppc.h) y picodrive (cpu/drc/emit_ppc.c) en este
 * mismo repositorio:
 *
 *   1. dcbst sobre cada linea modificada   (volcar D-cache a memoria)
 *   2. sync                                (esperar a que el volcado termine)
 *   3. icbi  sobre cada linea modificada   (invalidar I-cache; ya ve los datos)
 *   4. sync                                (esperar a que terminen los icbi)
 *   5. isync                               (descartar el prefetch del pipeline)
 *
 * El segundo sync (paso 4) NO es opcional en Xenon: al ser in-order con modelo
 * de memoria debil, sin el, el isync adelanta a los icbi y quedan lineas
 * rancias en la I-cache.  Funciona la primera vez y falla al regenerar codigo
 * sobre la misma linea (justo lo que hace un dynarec al invalidar bloques).
 *
 * El XDK expone __dcbst y __emit como intrinsecos (ppcintrinsics.h) pero NO
 * expone icbi ni isync, de ahi la funcion naked y el opcode crudo.
 *
 * Linea de cache de Xenon (L1i/L1d): 128 bytes.
 */

#ifndef XENON_CACHE_H
#define XENON_CACHE_H

#if defined(_XBOX) && defined(_MSC_VER)
/* fuera del extern "C": ppcintrinsics.h arrastra vectorintrinsics.h, que trae
 * typedefs propios y ya lleva su propia guarda de enlace */
#include <ppcintrinsics.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_XBOX) && defined(_MSC_VER)

#define XENON_CACHE_LINE 128

/* El XDK no da icbi como intrinseco: funcion naked con una sola instruccion.
 * Vive en .text (siempre ejecutable), asi que no hay problema de arranque
 * (no hace falta invalidar la I-cache para poder ejecutar el invalidador).
 * Convenio: r3 = offset, r4 = base  ->  icbi r3, r4
 * Mismo patron que pcsxr-360 (libpcsxcore/ppc/pR3000A.c:30). */
static void __declspec(naked) xenon_icbi_one(int offset, const void* base)
{
	__asm {
		icbi	r3, r4
		blr
	}
}

static __inline void xenon_icache_invalidate(void* startaddr, unsigned int len)
{
	char* p;
	char* end;

	if (len == 0) return;

	end = (char*)startaddr + len;
	/* alinear el inicio a linea de cache para no dejar bytes rancios delante */
	p = (char*)((unsigned int)startaddr & ~(XENON_CACHE_LINE - 1));

	/* Paso 1: dcbst en todas las lineas */
	while (p < end) {
		__dcbst(0, (const void*)p);
		p += XENON_CACHE_LINE;
	}
	/* Paso 2: sync */
	__emit(0x7C0004AC);

	/* Paso 3: icbi en todas las lineas */
	p = (char*)((unsigned int)startaddr & ~(XENON_CACHE_LINE - 1));
	while (p < end) {
		xenon_icbi_one(0, (const void*)p);
		p += XENON_CACHE_LINE;
	}
	/* Paso 4: sync */
	__emit(0x7C0004AC);
	/* Paso 5: isync */
	__emit(0x4C00012C);
}

#elif defined(__GNUC__)

#define XENON_CACHE_LINE 128

static __inline void xenon_icache_invalidate(void* startaddr, unsigned int len)
{
	char* p;
	char* end;

	if (len == 0) return;

	end = (char*)startaddr + len;
	p = (char*)((unsigned int)startaddr & ~(XENON_CACHE_LINE - 1));

	for (; p < end; p += XENON_CACHE_LINE)
		__asm__ volatile("dcbst 0,%0" :: "r"(p));
	__asm__ volatile("sync");

	p = (char*)((unsigned int)startaddr & ~(XENON_CACHE_LINE - 1));
	for (; p < end; p += XENON_CACHE_LINE)
		__asm__ volatile("icbi 0,%0" :: "r"(p));
	__asm__ volatile("sync\n\t isync");
}

#else
#error "No se sabe como invalidar la I-cache con este compilador"
#endif

#ifdef __cplusplus
}
#endif

#endif /* XENON_CACHE_H */
