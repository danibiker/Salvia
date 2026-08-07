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

#ifndef __MEM__
#define __MEM__

#include <stdint.h>
#include <boolean.h>

#define ROM_START    0x200000
#define ROM_END		0x3FFFFF

#define HIROM_START	0x800000
#define HIROM_END    0x9FFFFF

#define BIOS_START	0xFF0000
#define BIOS_END     0xFFFFFF

#ifdef __cplusplus
extern "C" {
#endif

void reset_memory(void);

extern bool memory_unlock_flash_write;
extern bool memory_flash_command;

extern bool FlashStatusEnable;
extern uint8_t COMMStatus;

uint8_t  loadB(uint32_t address);
uint16_t loadW(uint32_t address);
uint32_t loadL(uint32_t address);

void storeB(uint32_t address, uint8_t data);
void storeW(uint32_t address, uint16_t data);
void storeL(uint32_t address, uint32_t data);

void SetFRM(void);
void RecacheFRM(void);

#ifdef __cplusplus
}
#endif

#endif
