#ifndef __WSWAN_MEMORY_H
#define __WSWAN_MEMORY_H

#include "../state.h"

enum
{
   MEMORY_GSREG_ROMBBSLCT = 0,
   MEMORY_GSREG_BNK1SLCT,
   MEMORY_GSREG_BNK2SLCT,
   MEMORY_GSREG_BNK3SLCT
};


#ifdef __cplusplus
extern "C" {
#endif

extern uint8 wsRAM[65536];
extern uint8 wsEEPROM[2048];
extern uint8 *wsCartROM;
extern uint32 eeprom_size;
extern uint8 *wsSRAM;
extern uint32 wsRAMSize;

void WSwan_MemoryInit(bool lang, bool IsWSC, uint32 ssize, bool IsWW);
void WSwan_MemoryKill(void);

uint8 WSwan_readmem20(uint32);
void WSwan_writemem20(uint32 address,uint8 data);

void WSwan_writeport(uint32 IOPort, uint8 V);
uint8 WSwan_readport(uint32 number);

/* WonderWitch variants: identical to the above plus the flash
 * window in bank 1 and the flash lock register at port 0xCE. */
uint8 WSwan_readmem20_WW(uint32);
void WSwan_writemem20_WW(uint32 address,uint8 data);
void WSwan_writeport_WW(uint32 IOPort, uint8 V);
uint8 WSwan_readport_WW(uint32 number);

/* WonderWitch only: contiguous [SRAM | flash] block exposed to
 * the frontend as libretro save RAM. NULL for normal carts. */
uint8 *WSwan_GetWWSaveBlock(uint32 *size);

void WSwan_MemoryReset(void);
int WSwan_MemoryStateAction(StateMem *sm, int load, int data_only);

void WSwan_CheckSoundDMA(void);

uint32 WSwan_MemoryGetRegister(const unsigned int id, char *special, const uint32 special_len);
void WSwan_MemorySetRegister(const unsigned int id, uint32 value);

#ifdef __cplusplus
}
#endif

#endif
