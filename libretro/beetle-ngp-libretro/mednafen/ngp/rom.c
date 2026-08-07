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

#include <stdlib.h>
#include <string.h>

#include "neopop.h"
#include "flash.h"
#include "interrupt.h"
#include "rom.h"
#include "../mednafen-types.h"

#ifdef MSB_FIRST
#define HTOLE16(l)      ((((l)>>8) & 0xff) | (((l)<<8) & 0xff00))
#else
#define HTOLE16(l) (l)
#endif

#define MATCH_CATALOG(c, s)	(rom_header->catalog == HTOLE16(c) \
				 && rom_header->subCatalog == (s))

RomInfo ngpc_rom;
RomHeader* rom_header = NULL;

static void rom_hack(void)
{
   /*=============================
    * SPECIFIC ROM HACKS !
    *=============================
    */

   /* "Neo-Neo! V1.0 (PD)" */
   if (MATCH_CATALOG(0, 16))
      ngpc_rom.data[0x23] = 0x10;	/* Fix ROM header */

   /* "Cool Cool Jam SAMPLE (U)" */
   if (MATCH_CATALOG(4660, 161))
      ngpc_rom.data[0x23] = 0x10;	/* Fix ROM header */

   /* "Dokodemo Mahjong (J)" */
   if (MATCH_CATALOG(51, 33))
      ngpc_rom.data[0x23] = 0x00;	/* Fix ROM header */
}

void rom_loaded(uint8_t *buf, size_t len)
{
   int i;

   ngpc_rom.data = (uint8 *)malloc(ngpc_rom.length);
   memcpy(ngpc_rom.data, buf, len);

   /* Extract the header */
   rom_header = (RomHeader*)(ngpc_rom.data);

   /* ROM Name */
   for(i = 0; i < 12; i++)
   {
      ngpc_rom.name[i] = ' ';
      if (rom_header->name[i] >= 32 && rom_header->name[i] < 128)
         ngpc_rom.name[i] = rom_header->name[i];
   }
   ngpc_rom.name[i] = 0;

   /* Apply a hack if required! */
   rom_hack();	

   flash_read();
}

void rom_unload(bool is_persistent)
{
   if (ngpc_rom.data)
   {
      int i;

      flash_commit();

      free(ngpc_rom.data);
      ngpc_rom.data = NULL;
      ngpc_rom.length = 0;
      rom_header = 0;

      for (i = 0; i < 16; i++)
         ngpc_rom.name[i] = 0;
   }		

   if(ngpc_rom.orig_data)
   {
      if (!is_persistent)
         free(ngpc_rom.orig_data);
      ngpc_rom.orig_data = NULL;
   }
}
