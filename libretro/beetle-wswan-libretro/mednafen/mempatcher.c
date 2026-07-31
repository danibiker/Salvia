/* Mednafen memory patcher, reduced to the surface the libretro
 * core actually uses: page-mapped RAM registration and periodic
 * 'R'-type (replace) cheat application fed by retro_cheat_set().
 *
 * Dropped relative to the mednafen original: read-substitution
 * patches ('S'/'C' subcheat machinery - nothing in a libretro
 * build installs read patches), cheat conditions, Game Genie /
 * PAR decoders, and the list/get/set/toggle/delete management
 * API (the frontend owns the cheat list and only ever adds or
 * flushes). Cheat names are accepted but not stored.
 */

#include <stdlib.h>
#include <string.h>

#include "mednafen-types.h"
#include "mempatcher.h"
#include "mempatcher-driver.h"
#include "settings.h"

typedef struct
{
   uint32 addr;
   uint64 val;
   uint32 length;
   uint8 bigendian;
   uint8 type;
} CHEATF;

static uint8 **RAMPtrs      = NULL;
static uint32 PageSize      = 0;
static uint32 NumPages      = 0;
static bool CheatsActive    = true;

static CHEATF *cheats       = NULL;
static uint32 cheat_count   = 0;
static uint32 cheat_alloc   = 0;

bool MDFNMP_Init(uint32 ps, uint32 numpages)
{
   PageSize = ps;
   NumPages = numpages;

   RAMPtrs = (uint8 **)calloc(numpages, sizeof(uint8 *));
   if (!RAMPtrs)
      return 0;

   CheatsActive = MDFN_GetSettingB("cheats");
   return 1;
}

void MDFNMP_Kill(void)
{
   if (RAMPtrs)
   {
      free(RAMPtrs);
      RAMPtrs = NULL;
   }
   if (cheats)
   {
      free(cheats);
      cheats      = NULL;
      cheat_count = 0;
      cheat_alloc = 0;
   }
}

void MDFNMP_AddRAM(uint32 size, uint32 A, uint8 *RAM)
{
   uint32 AB = A / PageSize;
   uint32 x;

   /* Round the page count up: with the 16KB cheat page size, a
    * region smaller than one page (e.g. the 8KB SRAM used by save
    * type 0x01 carts) previously mapped zero pages, making it
    * invisible to the cheat engine. Same bug exists in upstream
    * mednafen. Callers are responsible for keeping cheat
    * addresses within the real region size; the libretro cheat
    * entry point validates against SRAMSize before adding a
    * code. */
   size = (size + PageSize - 1) / PageSize;

   for (x = 0; x < size; x++)
   {
      if (AB + x >= NumPages)
         break;
      RAMPtrs[AB + x] = RAM;
      if (RAM) /* Don't increment the RAM pointer if we're passed a NULL pointer */
         RAM += PageSize;
   }
}

int MDFNI_AddCheat(const char *name, uint32 addr, uint64 val, uint64 compare, char type, unsigned int length, bool bigendian)
{
   CHEATF *slot;

   (void)name;
   (void)compare;

   if (cheat_count == cheat_alloc)
   {
      uint32 new_alloc = cheat_alloc ? (cheat_alloc << 1) : 8;
      CHEATF *new_buf  = (CHEATF *)realloc(cheats, new_alloc * sizeof(CHEATF));

      if (!new_buf)
         return 0;

      cheats      = new_buf;
      cheat_alloc = new_alloc;
   }

   slot            = &cheats[cheat_count];
   slot->addr      = addr;
   slot->val       = val;
   slot->length    = length;
   slot->bigendian = bigendian ? 1 : 0;
   slot->type      = (uint8)type;
   cheat_count++;

   return 1;
}

void MDFN_FlushGameCheats(int nosave)
{
   (void)nosave;
   cheat_count = 0;
}

void MDFNMP_ApplyPeriodicCheats(void)
{
   uint32 i;

   if (!CheatsActive)
      return;

   for (i = 0; i < cheat_count; i++)
   {
      const CHEATF *ch = &cheats[i];
      uint32 x;

      if (ch->type != 'R')
         continue;

      for (x = 0; x < ch->length; x++)
      {
         uint32 page = ((ch->addr + x) / PageSize) % NumPages;

         if (RAMPtrs[page])
         {
            uint64 tmpval = ch->val;

            if (ch->bigendian)
               tmpval >>= (ch->length - 1 - x) * 8;
            else
               tmpval >>= x * 8;

            RAMPtrs[page][(ch->addr + x) % PageSize] = (uint8)tmpval;
         }
      }
   }
}
