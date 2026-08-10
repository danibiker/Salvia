/*
 *  Prefs.cpp - Global preferences
 *
 *  Frodo Copyright (C) Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>

#include "sysdeps.h"

#include "Prefs.h"
#include "Display.h"
#include "C64.h"
#include "main.h"

// These are the active preferences
Prefs ThePrefs;

/*
 *  Constructor: Set up preferences with defaults
 */

Prefs::Prefs()
{
   NormalCycles       = 63;
   BadLineCycles      = 23;
   CIACycles          = 63;
   FloppyCycles       = 64;
   SkipFrames         = 1;
   LatencyMin         = 80;
   LatencyMax         = 120;
   LatencyAvg         = 280;
   ScalingNumerator   = 2;
   ScalingDenominator = 2;

   strcpy(DrivePath[0], "64prgs");
   strcpy(DrivePath[1], "");
   strcpy(DrivePath[2], "");
   strcpy(DrivePath[3], "");

   strcpy(ViewPort, "Default");
   strcpy(DisplayMode, "Default");

   SIDType            = SIDTYPE_DIGITAL;
   REUSize            = REU_NONE;
   DisplayType        = DISPTYPE_WINDOW;
   Joystick1Port      = 0;
   Joystick2Port      = 0;

   SpritesOn          = true;
   SpriteCollisions   = true;
   JoystickSwap       = false;
   LimitSpeed         = true;
   FastReset          = false;
   CIAIRQHack         = false;
   MapSlash           = true;
   Emul1541Proc       = false;
   SIDFilters         = true;
   DoubleScan         = true;
   HideCursor         = false;
   DirectSound        = true;	
   ExclusiveSound     = false;
   AutoPause          = false;
   PrefsAtStartup     = false;
   SystemMemory       = false;
   AlwaysCopy         = false;
   SystemKeys         = true;
   ShowLEDs           = true;
}


/*
 *  Check if two Prefs structures are equal
 */

bool Prefs::operator==(const Prefs &rhs) const
{
	return (1
		&& NormalCycles == rhs.NormalCycles
		&& BadLineCycles == rhs.BadLineCycles
		&& CIACycles == rhs.CIACycles
		&& FloppyCycles == rhs.FloppyCycles
		&& SkipFrames == rhs.SkipFrames
		&& LatencyMin == rhs.LatencyMin
		&& LatencyMax == rhs.LatencyMax
		&& LatencyAvg == rhs.LatencyAvg
		&& ScalingNumerator == rhs.ScalingNumerator
		&& ScalingDenominator == rhs.ScalingNumerator
		&& strcmp(DrivePath[0], rhs.DrivePath[0]) == 0
		&& strcmp(DrivePath[1], rhs.DrivePath[1]) == 0
		&& strcmp(DrivePath[2], rhs.DrivePath[2]) == 0
		&& strcmp(DrivePath[3], rhs.DrivePath[3]) == 0
		&& strcmp(ViewPort, rhs.ViewPort) == 0
		&& strcmp(DisplayMode, rhs.DisplayMode) == 0
		&& SIDType == rhs.SIDType
		&& REUSize == rhs.REUSize
		&& DisplayType == rhs.DisplayType
		&& SpritesOn == rhs.SpritesOn
		&& SpriteCollisions == rhs.SpriteCollisions
		&& Joystick1Port == rhs.Joystick1Port
		&& Joystick2Port == rhs.Joystick2Port
		&& JoystickSwap == rhs.JoystickSwap
		&& LimitSpeed == rhs.LimitSpeed
		&& FastReset == rhs.FastReset
		&& CIAIRQHack == rhs.CIAIRQHack
		&& MapSlash == rhs.MapSlash
		&& Emul1541Proc == rhs.Emul1541Proc
		&& SIDFilters == rhs.SIDFilters
		&& DoubleScan == rhs.DoubleScan
		&& HideCursor == rhs.HideCursor
		&& DirectSound == rhs.DirectSound
		&& ExclusiveSound == rhs.ExclusiveSound
		&& AutoPause == rhs.AutoPause
		&& PrefsAtStartup == rhs.PrefsAtStartup
		&& SystemMemory == rhs.SystemMemory
		&& AlwaysCopy == rhs.AlwaysCopy
		&& SystemKeys == rhs.SystemKeys
		&& ShowLEDs == rhs.ShowLEDs
	);
}

bool Prefs::operator!=(const Prefs &rhs) const
{
	return !operator==(rhs);
}

void Prefs::set_drive8(char *filename,int type)
{
   strcpy(DrivePath[0], filename);
}
