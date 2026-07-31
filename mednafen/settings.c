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

#include <stdint.h>
#include <string.h>
#include "settings.h"

/* Owner profile written into the internal EEPROM at game load;
 * populated from core options / frontend username by the
 * libretro layer before MDFNI_LoadGame(). Defaults match
 * upstream mednafen's setting defaults. */
uint32_t setting_wswan_byear    = 1989;
uint32_t setting_wswan_bmonth   = 6;
uint32_t setting_wswan_bday     = 23;
uint32_t setting_wswan_sex      = 2; /* female */
uint32_t setting_wswan_blood    = 3; /* O */
uint32_t setting_wswan_language = 1; /* english */
char setting_wswan_name[17]     = "Mednafen";

uint64_t MDFN_GetSettingUI(const char *name)
{
   if (!strcmp("wswan.ocmultiplier", name))
      return 1;
   if (!strcmp("wswan.bday", name))
      return setting_wswan_bday;
   if (!strcmp("wswan.bmonth", name))
      return setting_wswan_bmonth;
   if (!strcmp("wswan.byear", name))
      return setting_wswan_byear;
   if (!strcmp("wswan.slstart", name))
      return 4;
   if (!strcmp("wswan.slend", name))
      return 235;

   return 0;
}

int64_t MDFN_GetSettingI(const char *name)
{
   if (!strcmp("wswan.sex", name))
      return (int64_t)setting_wswan_sex;
   if (!strcmp("wswan.blood", name))
      return (int64_t)setting_wswan_blood;
   return 0;
}

bool MDFN_GetSettingB(const char *name)
{
   if (!strcmp("cheats", name))
      return 1;
   if (!strcmp("wswan.forcemono", name))
      return 0;
   if (!strcmp("wswan.language", name))
      return setting_wswan_language != 0;
   if (!strcmp("wswan.correct_aspect", name))
      return 1;
   return 0;
}

const char *MDFN_GetSettingS(const char *name)
{
   if (!strcmp("wswan.name", name))
      return setting_wswan_name;
   return "";
}
