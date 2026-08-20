/***************************************************************************
    Load OutRun ROM Set.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <boolean.h>

#include "romloader.h"

static const bool VERBOSE = true;

typedef struct Roms
{
    RomLoader rom0;
    RomLoader rom1;
    RomLoader tiles;
    RomLoader sprites;
    RomLoader road;
    RomLoader z80;
    RomLoader pcm;
    RomLoader j_rom0;
    RomLoader j_rom1;
    RomLoader* rom0p;
    RomLoader* rom1p;
    int jap_rom_status;
} Roms;

extern Roms roms;

void Roms_ctor(Roms* self);

bool Roms_load_revb_roms(Roms* self, bool);

bool Roms_load_japanese_roms(Roms* self);

int Roms_load_pcm_rom(Roms* self, bool);

bool Roms_load_ym_data(Roms* self, const char* filename);

#ifdef __cplusplus
}
#endif
