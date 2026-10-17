/**
 * glN64_GX - N64.cpp
 * Copyright (C) 2003 Orkin
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 *
**/

#ifdef __GX__
#include <gccore.h>
#endif // __GX__

#ifndef __LINUX__
# include <windows.h>
#else
# include "../main/winlnxdefs.h"
#endif // __LINUX__
#include "N64.h"
#include "Types.h"

u8 *DMEM;
u8 *IMEM;
u64 TMEM[512] ATTRIBUTE_ALIGN(32);
u8 *RDRAM;
u8 *HEADER;
u32 RDRAMSize;

N64Regs REG;

/* [Xbox360] [Cruis'n USA] La lista en paralelo con "foto" lee una COPIA de la
 * RDRAM tomada al empezarla (xbox/hilo_lista.cpp).  El hilo de la lista mueve
 * aqui la base antes de la lista y la devuelve despues; nadie mas llama al
 * plugin mientras tanto.  Ver xbox64_gfx_rdram_base en xbox/gfx_dispatch.c. */
extern "C" void xbox64_gln64_rdram_base( unsigned char *base )
{
	RDRAM = base;
}
