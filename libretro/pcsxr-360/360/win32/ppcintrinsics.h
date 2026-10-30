/*
 * 360/win32/ppcintrinsics.h  --  los intrinsecos de PowerPC del XDK, en x86.
 *
 * [Windows, 2026-10-30] Solo la ve la build Win32 (ver xtl.h de esta carpeta).
 * La incluye xtl.h, igual que en la Xbox, y tambien se puede incluir sola
 * (plugins/gpu_duck/gpu_duck_gpu_backend.h lo hace, detras de _XBOX).
 *
 * Equivalencias:
 *   __lwsync()          barrera "ligera" del PPC (orden lectura-lectura y
 *                       escritura-escritura).  x86 ya lo garantiza (TSO): basta
 *                       con que el COMPILADOR no reordene.  Es lo que usan el
 *                       ring del hilo de GPU (libpcsxcore/gpu.c) para publicar
 *                       datos antes del indice.
 *   __sync()            barrera completa: MemoryBarrier().
 *   __dcbt / __dcbst /  pistas y vaciados de la cache de datos del PPC.  En
 *   __dcbf / __dcbz128  x86 la cache es coherente: nada que hacer (__dcbz128
 *                       pone a cero 128 bytes, eso si hay que hacerlo).
 *   _CountLeadingZeros  ceros a la izquierda de 32 bits (0 -> 32).
 *
 * Lo que NO esta, a proposito: los load/store con bytes al reves
 * (__loadshortbytereverse y familia).  Ver la nota de xtl.h.
 *
 * ★ VALIDA EN C Y EN C++.
 */

#ifndef _PCSXR360_WIN32_PPCINTRINSICS_H_
#define _PCSXR360_WIN32_PPCINTRINSICS_H_

#if defined(_XBOX)
#error "360/win32/ppcintrinsics.h es solo para la build de Windows"
#endif

#include <windows.h>   /* MemoryBarrier */
#include <intrin.h>
#include <string.h>

#pragma intrinsic(_ReadWriteBarrier, _BitScanReverse)

#define __lwsync()      _ReadWriteBarrier()
#define __sync()        MemoryBarrier()
#define __isync()       ((void)0)
#define __eieio()       _ReadWriteBarrier()

#define __dcbt(off, base)     ((void)(off), (void)(base))
#define __dcbst(off, base)    ((void)(off), (void)(base))
#define __dcbf(off, base)     ((void)(off), (void)(base))
#define __dcbz128(off, base)  memset((char *)(base) + (off), 0, 128)

static __inline unsigned int _CountLeadingZeros(unsigned long x)
{
	unsigned long bit;
	if (!_BitScanReverse(&bit, x))
		return 32;
	return 31 - (unsigned int)bit;
}

#endif /* _PCSXR360_WIN32_PPCINTRINSICS_H_ */
