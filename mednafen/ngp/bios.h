//---------------------------------------------------------------------------
// NEOPOP : Emulator as in Dreamland
//
// Copyright (c) 2001-2002 by neopop_uk
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version. See also the license.txt file for
//	additional informations.
//---------------------------------------------------------------------------

#ifndef __BIOS__
#define __BIOS__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t ngpc_bios[0x10000];

void iBIOSHLE(void);

/* Fill the bios rom area with a bios. call once at program start */
int bios_install(void);

void biosDecode(int function);
void BIOSHLE_Reset(void);
int BIOSHLE_StateAction(void *data, int load, int data_only);

#ifdef __cplusplus
}
#endif

#endif

