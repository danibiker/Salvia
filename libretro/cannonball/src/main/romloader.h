/***************************************************************************
    Binary File Loader.

    Handles loading an individual binary file to memory.
    Supports reading bytes, words and longs from this area of memory.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

enum {NORMAL = 1, INTERLEAVE2 = 2, INTERLEAVE4 = 4};

typedef struct RomLoader
{
    uint8_t* rom;
    uint32_t length;
    bool loaded;
} RomLoader;

void RomLoader_dtor(RomLoader* self);

void RomLoader_init(RomLoader* self, uint32_t length);

int RomLoader_load_auto(RomLoader* self, const char* filename, const int offset, const int length,
                  const uint32_t expected_crc,
                  const uint8_t mode,
                  const bool verbose);

int RomLoader_load_rom(RomLoader* self, const char* filename, const int offset, const int length,
                 const uint32_t expected_crc,
                 const uint8_t mode,
                 const bool verbose);

int RomLoader_load_crc32(RomLoader* self, const char* debug, const int offset, const int length,
                   const uint32_t expected_crc,
                   const uint8_t mode,
                   const bool verbose);

int RomLoader_load_binary(RomLoader* self, const char* filename);
int RomLoader_load_mem(RomLoader* self, const uint8_t* data, uint32_t len);

void RomLoader_unload(RomLoader* self);

static INLINE uint32_t RomLoader_read32_a(RomLoader* self, uint32_t* addr)
{
        uint32_t data = (self->rom[*addr] << 24) | (self->rom[*addr + 1] << 16) |
                        (self->rom[*addr + 2] << 8) | self->rom[*addr + 3];
        *addr += 4;
        return data;
    }
static INLINE uint16_t RomLoader_read16_a(RomLoader* self, uint32_t* addr)
{
        uint16_t data = (self->rom[*addr] << 8) | self->rom[*addr + 1];
        *addr += 2;
        return data;
    }
static INLINE uint8_t RomLoader_read8_a(RomLoader* self, uint32_t* addr)
{
        return self->rom[(*addr)++];
    }
static INLINE uint32_t RomLoader_read32(RomLoader* self, uint32_t addr)
{
        return (self->rom[addr] << 24) | (self->rom[addr + 1] << 16) |
               (self->rom[addr + 2] << 8) | self->rom[addr + 3];
    }
static INLINE uint16_t RomLoader_read16(RomLoader* self, uint32_t addr)
{
        return (self->rom[addr] << 8) | self->rom[addr + 1];
    }
static INLINE uint8_t RomLoader_read8(RomLoader* self, uint32_t addr)
{
        return self->rom[addr];
    }
static INLINE uint16_t RomLoader_read16_16a(RomLoader* self, uint16_t* addr)
{
        uint16_t data = (self->rom[*addr + 1] << 8) | self->rom[*addr];
        *addr += 2;
        return data;
    }
static INLINE uint8_t RomLoader_read8_16a(RomLoader* self, uint16_t* addr)
{
        return self->rom[(*addr)++];
    }
static INLINE uint16_t RomLoader_read16_16(RomLoader* self, uint16_t addr)
{
        return (self->rom[addr + 1] << 8) | self->rom[addr];
    }

#ifdef __cplusplus
}
#endif
