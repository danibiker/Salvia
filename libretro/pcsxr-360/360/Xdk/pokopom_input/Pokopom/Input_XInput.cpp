/*  Pokopom - Input Plugin for PSX/PS2 Emulators
 *  Copyright (C) 2012  KrossX
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "General.h"
#include "Input.h"
#include "Input_Shared.h"

#define XINPUT_GAMEPAD_GUIDE 0x400

XINPUT_STATE state[4];

namespace Input
{

////////////////////////////////////////////////////////////////////////
// General
////////////////////////////////////////////////////////////////////////

bool FASTCALL Recheck(u8 port)
{
	if(settings[port].disabled) return false;

	DWORD result = XInputGetState(port, &state[port]);
	
	return (result == ERROR_SUCCESS);
}

void FASTCALL Pause(bool pewpew) 
{ 
	if(pewpew) StopRumbleAll();
}

void StopRumbleAll()
{
	StopRumble(0); StopRumble(1);
	StopRumble(2); StopRumble(3);
}

void FASTCALL StopRumble(u8 port)
{
	XINPUT_VIBRATION vib;

	vib.wLeftMotorSpeed = 0;
	vib.wRightMotorSpeed = 0;

	XInputSetState(port, &vib);
}

bool FASTCALL CheckAnalogToggle(u8 port)
{
	const bool pad = !!(state[port].Gamepad.wButtons & XINPUT_GAMEPAD_GUIDE);

	return pad;
}

void FASTCALL SetAnalogLed(u8 port, bool digital)
{
}

bool FASTCALL InputGetState(_Pad& pad, _Settings &set)
{
	const int xport = set.xinputPort;
	DWORD result = XInputGetState(xport, &state[xport]);

	if(result == ERROR_SUCCESS)
	{
		pad.buttons[X360_DUP] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
		pad.buttons[X360_DDOWN] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) >> 1;
		pad.buttons[X360_DLEFT] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) >> 2;
		pad.buttons[X360_DRIGHT] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) >> 3;

		pad.buttons[X360_START] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_START) >> 4;
		pad.buttons[X360_BACK] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_BACK) >> 5;

		pad.buttons[X360_LS] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) >> 6;
		pad.buttons[X360_RS] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) >> 7;
		pad.buttons[X360_LB] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) >> 8;
		pad.buttons[X360_RB] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) >> 9;

		pad.buttons[X360_BIGX] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_GUIDE) >> 10;

		pad.buttons[X360_A] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_A) >> 12;
		pad.buttons[X360_B] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_B) >> 13;
		pad.buttons[X360_X] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_X) >> 14;
		pad.buttons[X360_Y] = (state[xport].Gamepad.wButtons & XINPUT_GAMEPAD_Y) >> 15;

		pad.analog[X360_STICKLX] = state[xport].Gamepad.sThumbLX;
		pad.analog[X360_STICKLY] = state[xport].Gamepad.sThumbLY;
		pad.analog[X360_STICKRX] = state[xport].Gamepad.sThumbRX;
		pad.analog[X360_STICKRY] = state[xport].Gamepad.sThumbRY;

		pad.analog[X360_TRIGGERL] = state[xport].Gamepad.bLeftTrigger;
		pad.analog[X360_TRIGGERR] = state[xport].Gamepad.bRightTrigger;

		pad.stickL.X = pad.analog[X360_STICKLX];
		pad.stickL.Y = pad.analog[X360_STICKLY];
		pad.stickR.X = pad.analog[X360_STICKRX];
		pad.stickR.Y = pad.analog[X360_STICKRY];

		set.axisValue[GP_AXIS_LY] = pad.analog[X360_STICKLY] * (set.axisInverted[GP_AXIS_LY] ? -1 : 1);
		set.axisValue[GP_AXIS_LX] = pad.analog[X360_STICKLX] * (set.axisInverted[GP_AXIS_LX] ? -1 : 1);
		set.axisValue[GP_AXIS_RY] = pad.analog[X360_STICKRY] * (set.axisInverted[GP_AXIS_RY] ? -1 : 1);
		set.axisValue[GP_AXIS_RX] = pad.analog[X360_STICKRX] * (set.axisInverted[GP_AXIS_RX] ? -1 : 1);

		pad.modL.X = set.axisValue[set.axisRemap[GP_AXIS_LX]];
		pad.modL.Y = set.axisValue[set.axisRemap[GP_AXIS_LY]];
		pad.modR.X = set.axisValue[set.axisRemap[GP_AXIS_RX]];
		pad.modR.Y = set.axisValue[set.axisRemap[GP_AXIS_RY]];

		GetRadius(pad.stickL); GetRadius(pad.stickR);
		GetRadius(pad.modL); GetRadius(pad.modR);
	}
	
	return result == ERROR_SUCCESS;
};

////////////////////////////////////////////////////////////////////////
// DualShock
////////////////////////////////////////////////////////////////////////

void FASTCALL DualshockRumble(u8 smalldata, u8 bigdata, _Settings &set, bool &gamepadPlugged)
{
	if(!gamepadPlugged) return;
	
	//Debug("Vibrate! [%X] [%X]\n", smalldata, bigdata);
	const u8 xport = set.xinputPort;

	static XINPUT_VIBRATION vib[4];
	static DWORD timerS[4], timerB[4];

	if(smalldata)
	{
		vib[xport].wRightMotorSpeed = Clamp(0xFFFF * set.rumble);
		timerS[xport] = GetTickCount();
	}
	else if (vib[xport].wRightMotorSpeed && GetTickCount() - timerS[xport] > 150)
	{
		vib[xport].wRightMotorSpeed = 0;
	}

	/*
	3.637978807091713*^-11 +
	156.82454281087692 * x + -1.258165252213538 *  x^2 +
	0.006474549734772402 * x^3;
	*/

	if(bigdata)
	{
		f64 broom = 0.006474549734772402 * pow(bigdata, 3.0) -
			1.258165252213538 *  pow(bigdata, 2.0) +
			156.82454281087692 * bigdata +
			3.637978807091713e-11;


		/*
		u32 broom = bigdata;

		if(bigdata <= 0x2C) broom *= 0x72;
		else if(bigdata <= 0x53) broom = 0x13C7 + bigdata * 0x24;
		else broom *= 0x205;
		*/

		vib[xport].wLeftMotorSpeed = Clamp(broom * set.rumble);
		timerB[xport] = GetTickCount();
	}
	else if (vib[xport].wLeftMotorSpeed && GetTickCount() - timerB[xport] > 150)
	{
		vib[xport].wLeftMotorSpeed = 0;
	}

	/*

	vib.wRightMotorSpeed = smalldata == 0? 0 : 0xFFFF;
	vib.wLeftMotorSpeed = bigdata * 0x101;

	vib.wRightMotorSpeed = Clamp(vib.wRightMotorSpeed * settings.rumble);
	vib.wLeftMotorSpeed = Clamp(vib.wLeftMotorSpeed * settings.rumble);
	*/

	if( XInputSetState(xport, &vib[xport]) != ERROR_SUCCESS )
		gamepadPlugged = false;
}

bool FASTCALL DualshockPressure(u8 * bufferOut, u32 mask, _Settings &set, bool &gamepadPlugged)
{
	return false;
}

}