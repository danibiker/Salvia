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

#include "PlayStation.h"
#include "ConfigDialog.h"
#include "Input.h"

_emuStuff emuStuff;
PlayStationDevice * controller[2] = {NULL, NULL};

char settingsDirectory[1024] = {0}; // for PCSX2

u32 bufferCount = 0;
u8 curPort = 0, curSlot = 1;
u8 multitap = 0;

////////////////////////////////////////////////////////////////////////
// PPDK developer must change libraryName field and can change revision and build
////////////////////////////////////////////////////////////////////////

const u32 revision = 2;
const u32 build    = 0;

const u32 versionPS1 = (0x01 << 16) | (revision << 8) | build;
const u32 versionPS2 = (0x02 << 16) | (revision << 8) | build;

char libraryName[]      = "Pokopom XInput Pad Plugin"; // rewrite your plug-in name
char PluginAuthor[]     = "KrossX"; // rewrite your name

////////////////////////////////////////////////////////////////////////
// stuff to make this a true PDK module
////////////////////////////////////////////////////////////////////////

DllExport char* CALLBACK POKOPOM_PSEgetLibName()
{
	isPSemulator = true;
	return libraryName;
}

////////////////////////////////////////////////////////////////////////
// Init/shutdown, will be called just once on emu start/close
////////////////////////////////////////////////////////////////////////

DllExport s32 CALLBACK POKOPOM_PADinit(s32 flags) // PAD INIT
{
	FileIO::INI_LoadSettings();
	Debug("Pokopom -> PADinit [%X]\n", flags);

	for(int pad = 0; pad < 2; pad++)
	{		
		switch(multitap)
		{
		case 0: 
			controller[pad] = new DualShock(settings[pad]); 
			break;

		case 1: 
			if(pad == 0) controller[pad] = new MultiTap(settings);
			else controller[pad] = new DualShock(settings[pad]);
			break;

		case 2: 
			if(pad == 0) controller[pad] = new DualShock(settings[pad]);
			else controller[pad] = new MultiTap(settings);
			break;
		}

		if(controller[pad])
			controller[pad]->SetPort((u8)pad);
		else 
			return emupro::ERR_FATAL;
	}

	return emupro::INIT_ERR_SUCCESS;
}

DllExport void CALLBACK POKOPOM_PADshutdown() // PAD SHUTDOWN
{
	DebugFunc();

	delete controller[0];
	delete controller[1];

	controller[0] = controller[1] = NULL;
}

////////////////////////////////////////////////////////////////////////
// Open/close will be called when a games starts/stops
////////////////////////////////////////////////////////////////////////

DllExport s32 CALLBACK POKOPOM_PADopen(void* pDisplay) // PAD OPEN
{
	DebugFunc();
	Input::Pause(false);

	return emupro::pad::ERR_SUCCESS;
}

DllExport s32 CALLBACK POKOPOM_PADclose() // PAD CLOSE
{
	DebugFunc();
	Input::Pause(true);

	return emupro::pad::ERR_SUCCESS;
}

////////////////////////////////////////////////////////////////////////
// call config dialog
////////////////////////////////////////////////////////////////////////

DllExport s32 CALLBACK POKOPOM_PADconfigure()
{
	isPSemulator = true;

	FileIO::INI_LoadSettings();

	return emupro::pad::ERR_SUCCESS;
}

////////////////////////////////////////////////////////////////////////
// show about dialog
////////////////////////////////////////////////////////////////////////

DllExport void CALLBACK POKOPOM_PADabout()
{
}

////////////////////////////////////////////////////////////////////////
// test... well, we are ever fine ;)
////////////////////////////////////////////////////////////////////////

DllExport s32 CALLBACK POKOPOM_PADtest()
{
	return emupro::pad::ERR_SUCCESS;
}

////////////////////////////////////////////////////////////////////////
// tell the controller's port which can be used
////////////////////////////////////////////////////////////////////////

DllExport s32 CALLBACK POKOPOM_PADquery()
{
	DebugFunc();
	return emupro::pad::USE_PORT1 | emupro::pad::USE_PORT2;
}

////////////////////////////////////////////////////////////////////////
// tell the input of pad
// this function should be replaced with PADstartPoll and PADpoll
////////////////////////////////////////////////////////////////////////

s32 FASTCALL POKOPOM_PADreadPort(s32 port, emupro::pad::DataS* ppds)
{
	Debug("Pokopom -> PADreadPort [%X]\n", port);

	controller[port]->command(0, 0x01);
	u8 cType = controller[port]->command(1, 0x42);
	ppds->controllerType = cType >> 4;

	controller[port]->command(2, 0x00);
	ppds->buttonStatus = controller[port]->command(3, 0x00) | (controller[port]->command(4, 0x00) << 8);

	cType = cType & 0xF;

	if(cType > 2)
	{
		ppds->rightJoyX = ppds->moveX = controller[port]->command(5, 0x00);
		ppds->rightJoyY = ppds->moveY = controller[port]->command(6, 0x00);

		if (cType >= 0x03)
		{
			ppds->leftJoyX = controller[port]->command(7, 0x00);
			ppds->leftJoyY = controller[port]->command(8, 0x00);
		}
	}

	return emupro::pad::ERR_SUCCESS;
}

DllExport s32 CALLBACK POKOPOM_PADreadPort1(emupro::pad::DataS* ppds)
{
	return POKOPOM_PADreadPort(0, ppds);
}

DllExport s32 CALLBACK POKOPOM_PADreadPort2(emupro::pad::DataS* ppds)
{
	return POKOPOM_PADreadPort(1, ppds);
}

////////////////////////////////////////////////////////////////////////
// input and output of pad
////////////////////////////////////////////////////////////////////////

DllExport u8 CALLBACK POKOPOM_PADstartPoll(s32 port)
{
	curPort = SwapPortsEnabled ? (u8)(port - 1) ^ SwapPorts() : (u8)(port - 1);
	bufferCount = 0;
	
	u8 data = controller[curPort]->command(bufferCount, curSlot);

	//if(curPort == 0) printf("\n[%02d] [%02X|%02X]\n", bufferCount, curSlot, data);
	Debug("\n[%02d|%02d] [%02X|%02X]\n", bufferCount, curPort, curSlot, data);

	return data;
}

DllExport u8 CALLBACK POKOPOM_PADpoll(u8 data)
{
	bufferCount++;

	u8 doto = controller[curPort]->command(bufferCount, data);

	//if(curPort == 0) printf("[%02d] [%02X|%02X]\n", bufferCount, data, doto);
	Debug("[%02d|%02d] [%02X|%02X]\n", bufferCount, curPort, data, doto);

	return doto;
}

////////////////////////////////////////////////////////////////////////
// other stuff
////////////////////////////////////////////////////////////////////////


DllExport u32 CALLBACK POKOPOM_PADfreeze(s32 mode, freezeData *data)
{
	Debug("Pokopom -> PADfreeze [%X]\n", mode);
	if(!data) return (u32)emupro::ERR_FATAL;

	switch(mode)
	{
	case emupro::Savestate::LOAD:
		{
			PlayStationDeviceState *state = (PlayStationDeviceState *)data->data;

			if(memcmp(state[0].libraryName, libraryName, 25) == 0 &&
				state[0].version == ((revision << 8) | build))
			{
				controller[0]->LoadState(state[0]);
				controller[1]->LoadState(state[1]);
			}
			else
			{
				printf("Pokopom -> Wrong savestate data to load.");
			}

		} break;

	case emupro::Savestate::SAVE:
		{
			PlayStationDeviceState state[2];

			memset(state, 0, sizeof(state));

			memcpy(state[0].libraryName, libraryName, 25);
			memcpy(state[1].libraryName, libraryName, 25);

			state[0].version = state[1].version = (revision << 8) | build;

			controller[0]->SaveState(state[0]);
			controller[1]->SaveState(state[1]);
			memcpy(data->data, state, sizeof(state));
		} break;

	case emupro::Savestate::QUERY_SIZE:
		{
			data->size = sizeof(PlayStationDeviceState) * 2;
		} break;
	}

	return emupro::ERR_SUCCESS;
}

DllExport keyEvent* CALLBACK POKOPOM_PADkeyEvent()
{
	DebugFunc();
	static keyEvent pochy;

	if(!isPs2Emulator)
		KeyboardCheck();

	if(!keyEventList.empty())
	{
		pochy = keyEventList.front();
		keyEventList.pop_back();
		return &pochy;
	}

	return NULL;
}

DllExport void CALLBACK POKOPOM_PADupdate(s32 port)
{
	Debug("Pokopom -> PADupdate [%X]\n", port);
}

DllExport s32 CALLBACK POKOPOM_PADKeypressed(void)
{
	Debug("Pokopom -> POKOPOM_PADKeypressed [%X]\n", port);
	return 0;
}


/**
*/
typedef s32 (CALLBACK* PADconfigure)(void);
typedef void (CALLBACK* PADabout)(void);
typedef s32 (CALLBACK* PADinit)(s32);
typedef void (CALLBACK* PADshutdown)(void);
typedef s32 (CALLBACK* PADtest)(void);
typedef s32 (CALLBACK* PADclose)(void);
typedef s32 (CALLBACK* PADquery)(void);
typedef s32 (CALLBACK* PADreadPort1)(emupro::pad::DataS*);
typedef s32 (CALLBACK* PADreadPort2)(emupro::pad::DataS*);
typedef s32 (CALLBACK* PADkeypressed)(void);
typedef u8 (CALLBACK* PADstartPoll)(int);
typedef u8 (CALLBACK* PADpoll)(unsigned char);
typedef void (CALLBACK* PADsetSensitive)(int);
typedef void (CALLBACK* PADregisterVibration)(void (CALLBACK *callback)(uint32_t, uint32_t));
typedef void (CALLBACK* PADregisterCursor)(void (CALLBACK *callback)(int, int, int));
typedef s32 (CALLBACK* PADopen)(void*);

extern "C" {	
	extern PADconfigure        PAD1_configure;
	extern PADabout            PAD1_about;
	extern PADinit             PAD1_init;
	extern PADshutdown         PAD1_shutdown;
	extern PADtest             PAD1_test;
	extern PADopen             PAD1_open;
	extern PADclose            PAD1_close;
	extern PADquery            PAD1_query;
	extern PADreadPort1        PAD1_readPort1;
	extern PADkeypressed       PAD1_keypressed;
	extern PADstartPoll        PAD1_startPoll;
	extern PADpoll             PAD1_poll;
	extern PADsetSensitive     PAD1_setSensitive;
	extern PADregisterVibration PAD1_registerVibration;
	extern PADregisterCursor   PAD1_registerCursor;
	extern PADconfigure        PAD2_configure;
	extern PADabout            PAD2_about;
	extern PADinit             PAD2_init;
	extern PADshutdown         PAD2_shutdown;
	extern PADtest             PAD2_test;
	extern PADopen             PAD2_open;
	extern PADclose            PAD2_close;
	extern PADquery            PAD2_query;
	extern PADreadPort2        PAD2_readPort2;
	extern PADkeypressed       PAD2_keypressed;
	extern PADstartPoll        PAD2_startPoll;
	extern PADpoll             PAD2_poll;
	extern PADsetSensitive     PAD2_setSensitive;
	extern PADregisterVibration PAD2_registerVibration;
	extern PADregisterCursor   PAD2_registerCursor;
}

static s32 CALLBACK PAD__query(void) { return 3; }
static s32 CALLBACK PAD__keypressed() { return 0; }
static void CALLBACK PAD__registerVibration(void (CALLBACK *callback)(uint32_t, uint32_t)) {}
static void CALLBACK PAD__registerCursor(void (CALLBACK *callback)(int, int, int)) {}

DllExport void CALLBACK POKOPOM_Init() {

	PAD1_configure =	PAD2_configure =	POKOPOM_PADconfigure;
	PAD1_about =		PAD2_about =		POKOPOM_PADabout;
	PAD1_init =			PAD2_init =			POKOPOM_PADinit;
	PAD1_shutdown =		PAD2_shutdown =		POKOPOM_PADshutdown;
	PAD1_test =			PAD2_test =			POKOPOM_PADtest;
	PAD1_open =			PAD2_open =			POKOPOM_PADopen;
	PAD1_close =		PAD2_close =		POKOPOM_PADclose;
	PAD1_query =		PAD2_query =		POKOPOM_PADquery;
	PAD1_readPort1	= POKOPOM_PADreadPort1;
	PAD2_readPort2	= POKOPOM_PADreadPort2;

	PAD1_keypressed =	PAD2_keypressed =	POKOPOM_PADKeypressed;
	PAD1_startPoll	=	PAD2_startPoll	=	POKOPOM_PADstartPoll;
	PAD1_poll		=	PAD2_poll		=	POKOPOM_PADpoll;

	PAD1_query		=	PAD2_query		= PAD__query;
	PAD1_keypressed	=	PAD2_keypressed	= PAD__keypressed;
	PAD1_registerVibration	=	PAD2_registerVibration	= PAD__registerVibration;
	PAD1_registerCursor		=	PAD2_registerCursor		= PAD__registerCursor;
}