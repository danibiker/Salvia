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

#ifndef __ROM__
#define __ROM__

#ifdef __cplusplus
extern "C" {
#endif

#include <boolean.h>
#include <stddef.h>

typedef struct 
{
	uint8_t* data;		/* Pointer to the ROM data */
	uint8_t *orig_data;	/* Original data (without flash writes 
                        during emulation; necessary for save states) */

	uint32_t length;    /* Length of the rom */

	uint8_t name[16];	/* NULL-terminated string, holding the Game name */
} RomInfo;

typedef struct
{
#ifdef _WIN32
#pragma pack(push, 1)
#endif
	uint8_t		licence[28];		// 0x00 - 0x1B
	uint32_t	startPC;			// 0x1C - 0x1F
	uint16_t	catalog;			// 0x20 - 0x21
	uint8_t		subCatalog;			// 0x22
	uint8_t		mode;				// 0x23
	uint8_t		name[12];			// 0x24 - 0x2F

	uint32_t	reserved1;			// 0x30 - 0x33
	uint32_t	reserved2;			// 0x34 - 0x37
	uint32_t	reserved3;			// 0x38 - 0x3B
	uint32_t	reserved4;			// 0x3C - 0x3F
#ifdef _WIN32
#pragma pack(pop)
} RomHeader;
#else
} __attribute__((__packed__)) RomHeader;
#endif

extern RomInfo ngpc_rom;

extern RomHeader *rom_header;

/*! Call this function when a rom has just been loaded, it will perform
	the system independent actions required. */
void rom_loaded(uint8_t *buf, size_t len);

/*!	Tidy up the rom and free the resources used. */
void rom_unload(bool is_persistent);

#ifdef __cplusplus
}
#endif

#endif
