/*
 * xbox/win32/ppcintrinsics.h  --  los intrinsecos de PowerPC del XDK, en x86.
 *
 * [Windows, 2026-10-26] Solo la ve la build Win32: su carpeta va la PRIMERA en
 * el include path de esa configuracion (xbox64.vcxproj), asi que los ~20
 * ficheros que hacen `#include <ppcintrinsics.h>` aterrizan aqui sin tocarlos.
 * En Xbox 360 esta carpeta no esta en el include path y entra la del XDK.
 *
 * Equivalencias:
 *   __mftb()            el time base del Xenon, 49.875.000 ticks por segundo:
 *                       QueryPerformanceCounter escalado a esa frecuencia
 *                       (XBOX64_TB_HZ).  [2026-10-27] Era __rdtsc(), a la
 *                       frecuencia de la CPU: todo lo que divide entre 50 para
 *                       tener microsegundos ([UTIL run/gfx/emu], compilar=,
 *                       las esperas de d3dq en ticks) salia ~60 veces grande
 *                       (logs (12): `[UTIL run]` 2600-3900 por frame, y los
 *                       unsigned short de [UTIL] se pasaban de 65535).  Quien
 *                       lo pasa a us con QueryPerformanceFrequency usa en PC
 *                       XBOX64_TB_HZ (en la Xbox las dos frecuencias son la
 *                       misma).
 *   __lwsync()          barrera de memoria "ligera" del PPC.  x86 ya ordena
 *                       las lecturas entre si y las escrituras entre si (TSO):
 *                       basta con que el COMPILADOR no reordene.
 *   __sync()            barrera completa: MemoryBarrier().
 *   __dcbt/__dcbf/      pistas y vaciados de la cache de datos del PPC.  En
 *   __dcbst             x86 la cache es coherente: nada que hacer.
 *   __loadwordbytereverse / __loadshortbytereverse
 *                       lectura con los bytes al reves (los usa
 *                       gxshim/ogc/machine/processor.h para __lwbrx/__lhbrx).
 *   _CountLeadingZeros  ceros a la izquierda de 32 bits (0 -> 32).
 *   __frsqrte           estimacion de 1/sqrt: aqui la exacta.
 *
 * ★ VALIDA EN C Y EN C++: la fuerza xbox64_pre.h en todas las unidades.
 */

#ifndef _XBOX64_WIN32_PPCINTRINSICS_H_
#define _XBOX64_WIN32_PPCINTRINSICS_H_

#if defined(_XBOX)
#error "xbox/win32/ppcintrinsics.h es solo para la build de Windows"
#endif

#include <windows.h>   /* MemoryBarrier */
#include <intrin.h>
#include <stdlib.h>    /* _byteswap_ulong / _byteswap_ushort */
#include <math.h>      /* sqrt */

#pragma intrinsic(_ReadWriteBarrier, _BitScanReverse)

/* La frecuencia del time base del Xenon (ver arriba). */
#define XBOX64_TB_HZ    49875000LL

static __inline unsigned __int64 xbox64_pc_mftb(void)
{
	static LONGLONG s_frec = 0;
	LARGE_INTEGER t;
	if (s_frec == 0)
	{
		LARGE_INTEGER f;
		s_frec = (QueryPerformanceFrequency( &f ) && f.QuadPart) ? f.QuadPart : XBOX64_TB_HZ;
	}
	QueryPerformanceCounter( &t );
	/* En dos partes para no desbordar: cuenta * 49,875e6 no cabe en 64 bits
	 * pasados unos dias, y el resto por 49,875e6 si (frecuencia < 1e11). */
	return (unsigned __int64)(t.QuadPart / s_frec) * (unsigned __int64)XBOX64_TB_HZ +
	       (unsigned __int64)(((t.QuadPart % s_frec) * XBOX64_TB_HZ) / s_frec);
}

#define __mftb()        xbox64_pc_mftb()
#define __lwsync()      _ReadWriteBarrier()
#define __sync()        MemoryBarrier()
#define __isync()       ((void)0)
#define __eieio()       _ReadWriteBarrier()

#define __dcbt(off, base)    ((void)(off), (void)(base))
#define __dcbf(off, base)    ((void)(off), (void)(base))
#define __dcbst(off, base)   ((void)(off), (void)(base))

static __inline unsigned int __loadwordbytereverse(int offset, const void *base)
{
	return _byteswap_ulong( *(const unsigned long *)((const char *)base + offset) );
}

static __inline unsigned short __loadshortbytereverse(int offset, const void *base)
{
	return _byteswap_ushort( *(const unsigned short *)((const char *)base + offset) );
}

static __inline unsigned int _CountLeadingZeros(unsigned long x)
{
	unsigned long bit;
	if (!_BitScanReverse( &bit, x ))
		return 32;
	return 31 - (unsigned int)bit;
}

static __inline double __frsqrte(double x)
{
	return 1.0 / sqrt( x );
}

#endif /* _XBOX64_WIN32_PPCINTRINSICS_H_ */
