/* Mednafen - Multi-system Emulator
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include <vector>

#include "mednafen-types.h"

#include "mempatcher.h"

#ifdef _WIN32
#include "msvc_compat.h"
#endif

#include "settings.h"

static uint8 **RAMPtrs = NULL;
static uint32 PageSize;
static uint32 NumPages;

typedef struct __CHEATF
{
   char *name;
   char *conditions;

   uint32 addr;
   uint64 val;
   uint64 compare;

   unsigned int length;
   bool bigendian;
   unsigned int icount; // Instance count
   char type;   /* 'R' for replace, 'S' for substitute(GG), 'C' for substitute with compare */
   int status;
} CHEATF;

static std::vector<CHEATF> cheats;
static bool CheatsActive = true;

static std::vector<SUBCHEAT> SubCheats[8];
static void RebuildSubCheats(void)
{
 std::vector<CHEATF>::iterator chit;

 for(int x = 0; x < 8; x++)
  SubCheats[x].clear();

 if(!CheatsActive) return;

 for(chit = cheats.begin(); chit != cheats.end(); chit++)
 {
  if(chit->status && chit->type != 'R')
  {
   for(unsigned int x = 0; x < chit->length; x++)
   {
    SUBCHEAT tmpsub;
    unsigned int shiftie;

    if(chit->bigendian)
     shiftie = (chit->length - 1 - x) * 8;
    else
     shiftie = x * 8;
    
    tmpsub.addr = chit->addr + x;
    tmpsub.value = (chit->val >> shiftie) & 0xFF;
    if(chit->type == 'C')
     tmpsub.compare = (chit->compare >> shiftie) & 0xFF;
    else
     tmpsub.compare = -1;
    SubCheats[(chit->addr + x) & 0x7].push_back(tmpsub);
   }
  }
 }
}

bool MDFNMP_Init(uint32 ps, uint32 numpages)
{
 PageSize = ps;
 NumPages = numpages;

 RAMPtrs = (uint8 **)calloc(numpages, sizeof(uint8 *));

 CheatsActive = MDFN_GetSettingB("cheats");
 return(1);
}

void MDFNMP_Kill(void)
{
   if(RAMPtrs)
   {
      free(RAMPtrs);
      RAMPtrs = NULL;
   }
}


void MDFNMP_AddRAM(uint32 size, uint32 A, uint8 *RAM)
{
 uint32 AB = A / PageSize;
 
 size /= PageSize;

 for(unsigned int x = 0; x < size; x++)
 {
  RAMPtrs[AB + x] = RAM;
  if(RAM) // Don't increment the RAM pointer if we're passed a NULL pointer
   RAM += PageSize;
 }
}

/* This function doesn't allocate any memory for "name" */
static int AddCheatEntry(char *name, char *conditions, uint32 addr, uint64 val, uint64 compare, int status, char type, unsigned int length, bool bigendian)
{
 CHEATF temp;

 memset(&temp, 0, sizeof(CHEATF));

 temp.name=name;
 temp.conditions = conditions;
 temp.addr=addr;
 temp.val=val;
 temp.status=status;
 temp.compare=compare;
 temp.length = length;
 temp.bigendian = bigendian;
 temp.type=type;

 cheats.push_back(temp);
 return(1);
}

void MDFN_LoadGameCheats(void *override_ptr)
{
 RebuildSubCheats();
}

void MDFN_FlushGameCheats(int nosave)
{
   std::vector<CHEATF>::iterator chit;

   for(chit = cheats.begin(); chit != cheats.end(); chit++)
   {
      free(chit->name);
      if(chit->conditions)
         free(chit->conditions);
   }
   cheats.clear();

   RebuildSubCheats();
}

int MDFNI_AddCheat(const char *name, uint32 addr, uint64 val, uint64 compare, char type, unsigned int length, bool bigendian)
{
 char *t;

 if(!(t = strdup(name)))
 {
    /* Error allocating memory for cheat data. */
    return(0);
 }

 if(!AddCheatEntry(t, NULL, addr,val,compare,1,type, length, bigendian))
 {
  free(t);
  return(0);
 }

 RebuildSubCheats();

 return(1);
}


static bool TestConditions(const char *string)
{
 char address[64];
 char operation[64];
 char value[64];
 char endian;
 unsigned int bytelen;
 bool passed = 1;

 while(sscanf(string, "%u %c %63s %63s %63s", &bytelen, &endian, address, operation, value) == 5 && passed)
 {
  uint64 v_value;
  uint64 value_at_address;


  if(value[0] == '0' && value[1] == 'x')
   v_value = strtoull(value + 2, NULL, 16);
  else
   v_value = strtoull(value, NULL, 0);

  value_at_address = 0;

  if(!strcmp(operation, ">="))
  {
   if(!(value_at_address >= v_value))
    passed = 0;
  }
  else if(!strcmp(operation, "<="))
  {
   if(!(value_at_address <= v_value))
    passed = 0;
  }
  else if(!strcmp(operation, ">"))
  {
   if(!(value_at_address > v_value))
    passed = 0;
  }
  else if(!strcmp(operation, "<"))
  {
   if(!(value_at_address < v_value))
    passed = 0;
  }
  else if(!strcmp(operation, "==")) 
  {
   if(!(value_at_address == v_value))
    passed = 0;
  }
  else if(!strcmp(operation, "!="))
  {
   if(!(value_at_address != v_value))
    passed = 0;
  }
  else if(!strcmp(operation, "&"))
  {
   if(!(value_at_address & v_value))
    passed = 0;
  }
  else if(!strcmp(operation, "!&"))
  {
   if(value_at_address & v_value)
    passed = 0;
  }
  else if(!strcmp(operation, "^"))
  {
   if(!(value_at_address ^ v_value))
    passed = 0;
  }
  else if(!strcmp(operation, "!^"))
  {
   if(value_at_address ^ v_value)
    passed = 0;
  }
  else if(!strcmp(operation, "|"))
  {
   if(!(value_at_address | v_value))
    passed = 0;
  }
  else if(!strcmp(operation, "!|"))
  {
   if(value_at_address | v_value)
    passed = 0;
  }
  string = strchr(string, ',');
  if(!string)
   break;
  string++;
 }

 return(passed);
}

void MDFNMP_ApplyPeriodicCheats(void)
{
 std::vector<CHEATF>::iterator chit;


 if(!CheatsActive)
  return;

 for(chit = cheats.begin(); chit != cheats.end(); chit++)
 {
  if(chit->status && chit->type == 'R')
  {
   if(!chit->conditions || TestConditions(chit->conditions))
    for(unsigned int x = 0; x < chit->length; x++)
    {
     uint32 page = ((chit->addr + x) / PageSize) % NumPages;
     if(RAMPtrs[page])
     {
      uint64 tmpval = chit->val;

      if(chit->bigendian)
       tmpval >>= (chit->length - 1 - x) * 8;
      else
       tmpval >>= x * 8;

      RAMPtrs[page][(chit->addr + x) % PageSize] = tmpval;
     }
   }
  }
 }
}
