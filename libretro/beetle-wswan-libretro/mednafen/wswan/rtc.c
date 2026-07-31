/******************************************************************************/
/* Mednafen - Multi-system Emulator                                           */
/******************************************************************************/
/* rtc.c - WonderSwan RTC Emulation
**  Copyright (C) 2014-2020 Mednafen Team
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
 RTC utilizing games:
	Dicing Knight
	Dokodemo Hamster 3
	Inuyasha - Kagome no Sengoku Nikki
*/

#include <time.h>
#include <string.h>

#include "wswan.h"

#include "../state.h"
#include "../state_inline.h"

static uint32 ClockCycleCounter;

static uint8 Command;
static uint8 CommandBuffer[7];
static uint8 CommandIndex;
static uint8 CommandCount;

/* All fields are BCD. mon and mday are 1-based, as on the hardware. */
typedef struct
{
   uint8 sec;
   uint8 min;
   uint8 hour;
   uint8 wday;
   uint8 mday;
   uint8 mon;
   uint8 year;
} GenericRTC;

static GenericRTC RTC;

#define mBCD8(v) ((uint8)((((v) / 10) << 4) | ((v) % 10)))

/* Increment a BCD value, rolling over to reset_val when thresh is
 * reached. Returns true on rollover. */
static bool BCDInc(uint8 *V, uint8 thresh, uint8 reset_val)
{
   *V = ((*V + 1) & 0x0F) | (*V & 0xF0);
   if((*V & 0x0F) >= 0x0A)
   {
      *V &= 0xF0;
      *V += 0x10;

      if((*V & 0xF0) >= 0xA0)
         *V &= 0x0F;
   }

   if(*V >= thresh)
   {
      *V = reset_val;
      return true;
   }

   return false;
}

static void RTC_TickSecond(void)
{
   if(BCDInc(&RTC.sec, 0x60, 0x00))
   {
      if(BCDInc(&RTC.min, 0x60, 0x00))
      {
         if(BCDInc(&RTC.hour, 0x24, 0x00))
         {
            uint8 mday_thresh = 0x32;

            if(RTC.mon == 0x02)
            {
               mday_thresh = 0x29;

               if(((RTC.year & 0x0F) % 4) == ((RTC.year & 0x10) ? 0x02 : 0x00))
                  mday_thresh = 0x30;
            }
            else if(RTC.mon == 0x04 || RTC.mon == 0x06 || RTC.mon == 0x09 || RTC.mon == 0x11)
               mday_thresh = 0x31;

            BCDInc(&RTC.wday, 0x07, 0x00);

            if(BCDInc(&RTC.mday, mday_thresh, 0x01))
            {
               if(BCDInc(&RTC.mon, 0x13, 0x01))
                  BCDInc(&RTC.year, 0xA0, 0x00);
            }
         }
      }
   }
}

void WSwan_RTCWrite(uint32 A, uint8 V)
{
   if(A == 0xCA)
   {
      Command = V & 0x1F;

      if(Command == 0x15)
      {
         CommandBuffer[0] = RTC.year;
         CommandBuffer[1] = RTC.mon;
         CommandBuffer[2] = RTC.mday;
         CommandBuffer[3] = RTC.wday;
         CommandBuffer[4] = RTC.hour;
         CommandBuffer[5] = RTC.min;
         CommandBuffer[6] = RTC.sec;

         CommandIndex = 0;
         CommandCount = 7;
      }
      else if(Command == 0x14)
      {
         CommandIndex = 0;
         CommandCount = 7;
      }
   }
   else if(A == 0xCB)
   {
      if(Command == 0x14)
      {
         if(CommandIndex < CommandCount)
            CommandBuffer[CommandIndex++] = V;
      }
   }
}

uint8 WSwan_RTCRead(uint32 A)
{
   uint8 ret = 0;

   if(A == 0xCA)
      ret = Command | 0x80;
   else if(A == 0xCB)
   {
      ret = 0x80;

      if(Command == 0x15)
      {
         if(CommandIndex < CommandCount)
            ret = CommandBuffer[CommandIndex++];
      }
   }

   return(ret);
}

void WSwan_RTCClock(uint32 cycles)
{
   ClockCycleCounter += cycles;

   while(ClockCycleCounter >= 3072000)
   {
      ClockCycleCounter -= 3072000;
      RTC_TickSecond();
   }
}

/* Called once at game load; seeds the emulated RTC from the host
 * clock. Everything afterwards advances only by emulated cycles,
 * so runahead/netplay/movies stay deterministic within a session. */
void WSwan_RTCInit(void)
{
   time_t long_time = time(NULL);
   struct tm *toom  = localtime(&long_time);

   RTC.sec  = 0x00;
   RTC.min  = 0x00;
   RTC.hour = 0x00;
   RTC.wday = 0x00;
   RTC.mday = 0x01;
   RTC.mon  = 0x01;
   RTC.year = 0x00;

   if(toom)
   {
      RTC.sec  = mBCD8(toom->tm_sec >= 60 ? 59 : toom->tm_sec);
      RTC.min  = mBCD8(toom->tm_min);
      RTC.hour = mBCD8(toom->tm_hour);
      RTC.wday = mBCD8(toom->tm_wday);
      RTC.mday = mBCD8(toom->tm_mday);
      RTC.mon  = mBCD8(toom->tm_mon + 1);
      RTC.year = mBCD8(toom->tm_year % 100);
   }

   ClockCycleCounter = 0;
}

void WSwan_RTCReset(void)
{
   Command = 0x00;
   memset(CommandBuffer, 0, sizeof(CommandBuffer));
   CommandIndex = 0;
   CommandCount = 0;
}

int WSwan_RTCStateAction(StateMem *sm, int load, int data_only)
{
   SFORMAT StateRegs[] =
   {
      SFVARN(RTC.sec, "RTC.sec"),
      SFVARN(RTC.min, "RTC.min"),
      SFVARN(RTC.hour, "RTC.hour"),
      SFVARN(RTC.wday, "RTC.wday"),
      SFVARN(RTC.mday, "RTC.mday"),
      SFVARN(RTC.mon, "RTC.mon"),
      SFVARN(RTC.year, "RTC.year"),

      SFVARN(ClockCycleCounter, "ClockCycleCounter"),

      SFVARN(Command, "Command"),
      SFARRAYN(CommandBuffer, sizeof(CommandBuffer), "CommandBuffer"),
      SFVARN(CommandCount, "CommandCount"),
      SFVARN(CommandIndex, "CommandIndex"),
      { 0, 0, 0, 0 }
   };

   if(!MDFNSS_StateAction(sm, load, data_only, StateRegs, "RTC", false))
      return 0;

   if(load)
   {
      if(CommandCount > sizeof(CommandBuffer))
         CommandCount = sizeof(CommandBuffer);
      if(CommandIndex > CommandCount)
         CommandIndex = CommandCount;
   }

   return 1;
}
