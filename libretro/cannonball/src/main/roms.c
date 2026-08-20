/***************************************************************************
    Load OutRun ROM Set.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "roms.h"

Roms roms;

void Roms_ctor(Roms* self)
{
    self->jap_rom_status = -1;
    self->rom0p = NULL;
    self->rom1p = NULL;
}


/* Expanded example: rom0.load_auto("epr-10380b.133", 0x00000, 0x10000, 0x1f6cadad, INTERLEAVE2); */

bool Roms_load_revb_roms(Roms* self, bool fixed_rom)
{
    /* If incremented, a rom has failed to load. */
    int status = 0;

    /* Load Master CPU ROMs */
    RomLoader_init(&(self->rom0), 0x40000);
    status += RomLoader_load_auto(&(self->rom0), "epr-10380b.133", 0x00000, 0x10000, 0x1f6cadad, INTERLEAVE2, VERBOSE);
    status += RomLoader_load_auto(&(self->rom0), "epr-10382b.118", 0x00001, 0x10000, 0xc4c3fa1a, INTERLEAVE2, VERBOSE);
    status += RomLoader_load_auto(&(self->rom0), "epr-10381b.132", 0x20000, 0x10000, 0xbe8c412b, INTERLEAVE2, VERBOSE);
    status += RomLoader_load_auto(&(self->rom0), "epr-10383b.117", 0x20001, 0x10000, 0x10a2014a, INTERLEAVE2, VERBOSE);

    /* Load Slave CPU ROMs */
    RomLoader_init(&(self->rom1), 0x40000);
    status += RomLoader_load_auto(&(self->rom1), "epr-10327a.76", 0x00000, 0x10000, 0xe28a5baf, INTERLEAVE2, VERBOSE);
    status += RomLoader_load_auto(&(self->rom1), "epr-10329a.58", 0x00001, 0x10000, 0xda131c81, INTERLEAVE2, VERBOSE);
    status += RomLoader_load_auto(&(self->rom1), "epr-10328a.75", 0x20000, 0x10000, 0xd5ec5e5d, INTERLEAVE2, VERBOSE);
    status += RomLoader_load_auto(&(self->rom1), "epr-10330a.57", 0x20001, 0x10000, 0xba9ec82a, INTERLEAVE2, VERBOSE);

    /* Load Non-Interleaved Tile ROMs */
    RomLoader_init(&(self->tiles), 0x30000);
    status += RomLoader_load_auto(&(self->tiles), "opr-10268.99", 0x00000, 0x08000, 0x95344b04, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->tiles), "opr-10232.102", 0x08000, 0x08000, 0x776ba1eb, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->tiles), "opr-10267.100", 0x10000, 0x08000, 0xa85bb823, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->tiles), "opr-10231.103", 0x18000, 0x08000, 0x8908bcbf, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->tiles), "opr-10266.101", 0x20000, 0x08000, 0x9f6f1a74, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->tiles), "opr-10230.104", 0x28000, 0x08000, 0x686f5e50, NORMAL, VERBOSE);

    /* Load Non-Interleaved Road ROMs (2 identical roms, 1 for each road) */
    RomLoader_init(&(self->road), 0x10000);
    status += RomLoader_load_auto(&(self->road), "opr-10185.11", 0x000000, 0x08000, 0x22794426, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->road), "opr-10186.47", 0x008000, 0x08000, 0x22794426, NORMAL, VERBOSE);

    /* Load Interleaved Sprite ROMs */
    RomLoader_init(&(self->sprites), 0x100000);
    status += RomLoader_load_auto(&(self->sprites), "mpr-10371.9", 0x000000, 0x20000, 0x7cc86208, INTERLEAVE4, VERBOSE);
    status += RomLoader_load_auto(&(self->sprites), "mpr-10373.10", 0x000001, 0x20000, 0xb0d26ac9, INTERLEAVE4, VERBOSE);
    status += RomLoader_load_auto(&(self->sprites), "mpr-10375.11", 0x000002, 0x20000, 0x59b60bd7, INTERLEAVE4, VERBOSE);
    status += RomLoader_load_auto(&(self->sprites), "mpr-10377.12", 0x000003, 0x20000, 0x17a1b04a, INTERLEAVE4, VERBOSE);
    status += RomLoader_load_auto(&(self->sprites), "mpr-10372.13", 0x080000, 0x20000, 0xb557078c, INTERLEAVE4, VERBOSE);
    status += RomLoader_load_auto(&(self->sprites), "mpr-10374.14", 0x080001, 0x20000, 0x8051e517, INTERLEAVE4, VERBOSE);
    status += RomLoader_load_auto(&(self->sprites), "mpr-10376.15", 0x080002, 0x20000, 0xf3b8f318, INTERLEAVE4, VERBOSE);
    status += RomLoader_load_auto(&(self->sprites), "mpr-10378.16", 0x080003, 0x20000, 0xa1062984, INTERLEAVE4, VERBOSE);

    /* Load Z80 Sound ROM */
    /* Note: This is a deliberate decision to double the Z80 ROM Space to accomodate extra FM based music */
    RomLoader_init(&(self->z80), 0x10000);
    status += RomLoader_load_auto(&(self->z80), "epr-10187.88", 0x0000, 0x08000, 0xa10abaa9, NORMAL, VERBOSE);

    /* Load Sega PCM Chip Samples */
    RomLoader_init(&(self->pcm), 0x60000);
    status += RomLoader_load_auto(&(self->pcm), "opr-10193.66", 0x00000, 0x08000, 0xbcd10dde, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->pcm), "opr-10192.67", 0x10000, 0x08000, 0x770f1270, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->pcm), "opr-10191.68", 0x20000, 0x08000, 0x20a284ab, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->pcm), "opr-10190.69", 0x30000, 0x08000, 0x7cab70e2, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->pcm), "opr-10189.70", 0x40000, 0x08000, 0x01366b54, NORMAL, VERBOSE);
    status += RomLoader_load_auto(&(self->pcm), "opr-10188.71", 0x50000, 0x08000, 0xbad30ad9, NORMAL, VERBOSE);
    status += Roms_load_pcm_rom(self, fixed_rom);

    /* If status has been incremented, a rom has failed to load. */
    return status == 0;
}

bool Roms_load_japanese_roms(Roms* self)
{
    /* Only attempt to initalize the arrays once. */
    if (self->jap_rom_status == -1)
    {
        RomLoader_init(&(self->j_rom0), 0x40000);
        RomLoader_init(&(self->j_rom1), 0x40000);
    }

    /* If incremented, a rom has failed to load. */
    self->jap_rom_status = 0;

    /* Load Master CPU ROMs      */
    self->jap_rom_status += RomLoader_load_auto(&(self->j_rom0), "epr-10380.133", 0x00000, 0x10000, 0xe339e87a, INTERLEAVE2, VERBOSE);
    self->jap_rom_status += RomLoader_load_auto(&(self->j_rom0), "epr-10382.118", 0x00001, 0x10000, 0x65248dd5, INTERLEAVE2, VERBOSE);
    self->jap_rom_status += RomLoader_load_auto(&(self->j_rom0), "epr-10381.132", 0x20000, 0x10000, 0xbe8c412b, INTERLEAVE2, VERBOSE);
    self->jap_rom_status += RomLoader_load_auto(&(self->j_rom0), "epr-10383.117", 0x20001, 0x10000, 0xdcc586e7, INTERLEAVE2, VERBOSE);

    /* Load Slave CPU ROMs         */
    self->jap_rom_status += RomLoader_load_auto(&(self->j_rom1), "epr-10327.76", 0x00000, 0x10000, 0xda99d855, INTERLEAVE2, VERBOSE);
    self->jap_rom_status += RomLoader_load_auto(&(self->j_rom1), "epr-10329.58", 0x00001, 0x10000, 0xfe0fa5e2, INTERLEAVE2, VERBOSE);
    self->jap_rom_status += RomLoader_load_auto(&(self->j_rom1), "epr-10328.75", 0x20000, 0x10000, 0x3c0e9a7f, INTERLEAVE2, VERBOSE);
    self->jap_rom_status += RomLoader_load_auto(&(self->j_rom1), "epr-10330.57", 0x20001, 0x10000, 0x59786e99, INTERLEAVE2, VERBOSE);
    /* If status has been incremented, a rom has failed to load. */
    return self->jap_rom_status == 0;
}

int Roms_load_pcm_rom(Roms* self, bool fixed_rom)
{
    int status = 0;
    if (fixed_rom)
    {
        status = RomLoader_load_auto(&(self->pcm), "opr-10188.71f", 0x50000, 0x08000, 0x37598616, NORMAL, false);
        if (status == 1)
            status = RomLoader_load_auto(&(self->pcm), "opr-10188.71f", 0x50000, 0x08000, 0xC2DE09B2, NORMAL, VERBOSE);
    }
    else
    {
        status = RomLoader_load_auto(&(self->pcm), "opr-10188.71", 0x50000, 0x08000, 0xbad30ad9, NORMAL, VERBOSE);
    }

    /*return RomLoader_load_auto(&(pcm), fixed_rom ? "opr-10188.71f" : "opr-10188.71", 0x50000, 0x08000, fixed_rom ? 0x37598616 : 0xbad30ad9, NORMAL) == 0; */
    return status;
}

bool Roms_load_ym_data(Roms* self, const char* filename)
{
    RomLoader data;
    if (RomLoader_load_binary(&(data), filename) == 0)
    {
        if (data.length < 0x8000)
        {
            memcpy(self->z80.rom + 0x8000, data.rom, data.length);
            RomLoader_unload(&(data));
            return true;
        }
        else
        {
            fprintf(stderr, "YM Data is too large: %s\n", filename);
        }
    }
    return false;
}