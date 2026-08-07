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

#ifndef __FLASH__
#define __FLASH__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void flash_read(void);

uint8_t *make_flash_commit(int32_t *length);

/* Marks flash blocks for saving. */
void flash_write(uint32_t start_address, uint16_t length);
void flash_optimise_blocks(void);

/* Stores the flash data */
void flash_commit(void);

int FLASH_StateAction(void *data, int load, int data_only);

void do_flash_read(uint8_t *flashdata);

#ifdef __cplusplus
}
#endif

#endif
