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
#include <ctype.h>
#include <errno.h>
#include <vector>

#include "general.h"
#include "mednafen-types.h"
#include "mempatcher.h"
#include "settings.h"

#ifdef _WIN32
#include "compat/msvc.h"
#endif

static uint8 **RAMPtrs = NULL;
static uint32 PageSize;
static uint32 NumPages;

typedef struct
{
 bool excluded;
 uint8 value; 
} CompareStruct;

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
static uint32 resultsbytelen = 1;
static bool resultsbigendian = 0;
static bool CheatsActive = true;

bool SubCheatsOn = 0;
std::vector<SUBCHEAT> SubCheats[8];

static void RebuildSubCheats(void)
{
   std::vector<CHEATF>::iterator chit;
   unsigned x;

   SubCheatsOn = 0;
   for(x = 0; x < 8; x++)
      SubCheats[x].clear();

   if(!CheatsActive)
      return;

   for(chit = cheats.begin(); chit != cheats.end(); chit++)
   {
      if(chit->status && chit->type != 'R')
      {
         for(x = 0; x < chit->length; x++)
         {
            SUBCHEAT tmpsub;
            unsigned int shiftie = x * 8;

            if(chit->bigendian)
               shiftie = (chit->length - 1 - x) * 8;

            tmpsub.addr    = chit->addr + x;
            tmpsub.value   = (chit->val >> shiftie) & 0xFF;
            tmpsub.compare = -1;
            if(chit->type == 'C')
               tmpsub.compare = (chit->compare >> shiftie) & 0xFF;
            SubCheats[(chit->addr + x) & 0x7].push_back(tmpsub);
            SubCheatsOn = 1;
         }
      }
   }
}

extern "C" bool MDFNMP_Init(uint32 ps, uint32 numpages)
{
   PageSize = ps;
   NumPages = numpages;

   RAMPtrs = (uint8 **)calloc(numpages, sizeof(uint8 *));

   CheatsActive = MDFN_GetSettingB("cheats");
   return(1);
}

extern "C" void MDFNMP_Kill(void)
{
   if(RAMPtrs)
   {
      free(RAMPtrs);
      RAMPtrs = NULL;
   }
}

extern "C" void MDFNMP_AddRAM(uint32 size, uint32 A, uint8 *RAM)
{
   unsigned x;
   uint32_t AB = A / PageSize;

   size /= PageSize;

   for(x = 0; x < size; x++)
   {
      RAMPtrs[AB + x] = RAM;
      if(RAM) // Don't increment the RAM pointer if we're passed a NULL pointer
         RAM += PageSize;
   }
}

extern "C" void MDFNMP_InstallReadPatches(void)
{
   unsigned x;
   std::vector<SUBCHEAT>::iterator chit;

   if(!CheatsActive)
      return;
}

void MDFNMP_RemoveReadPatches(void)
{
}

/* This function doesn't allocate any memory for "name" */
static int AddCheatEntry(char *name, char *conditions, uint32 addr, uint64 val, uint64 compare, int status, char type, unsigned int length, bool bigendian)
{
   CHEATF temp = {0};

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

extern "C" void MDFN_LoadGameCheats(void)
{
   RebuildSubCheats();
}

extern "C" void MDFN_FlushGameCheats(void)
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
      return(0);

   if(!AddCheatEntry(t, NULL, addr,val,compare,1,type, length, bigendian))
   {
      free(t);
      return(0);
   }

   MDFNMP_RemoveReadPatches();
   RebuildSubCheats();
   MDFNMP_InstallReadPatches();

   return(1);
}

int MDFNI_DelCheat(uint32 which)
{
   free(cheats[which].name);
   cheats.erase(cheats.begin() + which);

   MDFNMP_RemoveReadPatches();
   RebuildSubCheats();
   MDFNMP_InstallReadPatches();

   return(1);
}

/*
 Condition format(ws = white space):
 
  <variable size><ws><endian><ws><address><ws><operation><ws><value>
	  [,second condition...etc.]

  Value should be unsigned integer, hex(with a 0x prefix) or
  base-10.  

  Operations:
   >=
   <=
   >
   <
   ==
   !=
   &	// Result of AND between two values is nonzero
   !&   // Result of AND between two values is zero
   ^    // same, XOR
   !^
   |	// same, OR
   !|

  Full example:

  2 L 0xADDE == 0xDEAD, 1 L 0xC000 == 0xA0

*/

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

extern "C" void MDFNMP_ApplyPeriodicCheats(void)
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


void MDFNI_ListCheats(int (*callb)(char *name, uint32 a, uint64 v, uint64 compare, int s, char type, unsigned int length, bool bigendian, void *data), void *data)
{
   std::vector<CHEATF>::iterator chit;

   for(chit = cheats.begin(); chit != cheats.end(); chit++)
   {
      if(!callb(chit->name, chit->addr, chit->val, chit->compare, chit->status, chit->type, chit->length, chit->bigendian, data)) break;
   }
}

int MDFNI_GetCheat(uint32 which, char **name, uint32 *a, uint64 *v, uint64 *compare, int *s, char *type, unsigned int *length, bool *bigendian)
{
   CHEATF *next = &cheats[which];

   if(name)
      *name=next->name;
   if(a)
      *a=next->addr; 
   if(v)
      *v=next->val;
   if(s)
      *s=next->status;
   if(compare)
      *compare=next->compare;
   if(type)
      *type=next->type;
   if(length)
      *length = next->length;
   if(bigendian)
      *bigendian = next->bigendian;
   return(1);
}

int MDFNI_DecodePAR(const char *str, uint32 *a, uint8 *v, uint8 *c, char *type)
{
   int boo[4];
   if(strlen(str)!=8) return(0);

   sscanf(str,"%02x%02x%02x%02x",boo,boo+1,boo+2,boo+3);

   *c = 0;

   if(1)
   {
      *a=(boo[3]<<8)|(boo[2]+0x7F);
      *v=0;
   }
   else
   {
      *v=boo[3];
      *a=boo[2]|(boo[1]<<8);
   }

   *type = 'S';
   return(1);
}

/* name can be NULL if the name isn't going to be changed. */
int MDFNI_SetCheat(uint32 which, const char *name, uint32 a, uint64 v, uint64 compare, int s, char type, unsigned int length, bool bigendian)
{
   CHEATF *next = &cheats[which];

   if(name)
   {
      char *t;

      if(!(t=(char *)realloc(next->name,strlen(name)+1)))
         return(0);

      next->name=t;
      strcpy(next->name,name);
   }
   next->addr=a;
   next->val=v;
   next->status=s;
   next->compare=compare;
   next->type=type;
   next->length = length;
   next->bigendian = bigendian;

   RebuildSubCheats();

   return(1);
}

/* Convenience function. */
int MDFNI_ToggleCheat(uint32 which)
{
   cheats[which].status = !cheats[which].status;
   RebuildSubCheats();

   return(cheats[which].status);
}

static void SettingChanged(const char *name)
{
   MDFNMP_RemoveReadPatches();

   CheatsActive = MDFN_GetSettingB("cheats");

   RebuildSubCheats();

   MDFNMP_InstallReadPatches();
}
