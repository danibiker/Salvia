#include <xtl.h>
extern "C"{
#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "PSEmu_Plugin_Defs.h"
#include "PSXInput.h"
}

#define PSX_BUTTON_TRIANGLE ~(1 << 12)
#define PSX_BUTTON_SQUARE 	~(1 << 15)
#define PSX_BUTTON_CROSS	~(1 << 14)
#define PSX_BUTTON_CIRCLE	~(1 << 13)
#define PSX_BUTTON_L2		~(1 << 8)
#define PSX_BUTTON_R2		~(1 << 9)
#define PSX_BUTTON_L1		~(1 << 10)
#define PSX_BUTTON_R1		~(1 << 11)
#define PSX_BUTTON_SELECT	~(1 << 0)
#define PSX_BUTTON_START	~(1 << 3)
#define PSX_BUTTON_DUP		~(1 << 4)
#define PSX_BUTTON_DRIGHT	~(1 << 5)
#define PSX_BUTTON_DDOWN	~(1 << 6)
#define PSX_BUTTON_DLEFT	~(1 << 7)

void PSxInputReadPort(PadDataS* pad, int port){

	unsigned short pad_status = 0xFFFF;
	int ls_x,ls_y,rs_x,rs_y;

	XINPUT_STATE InputState;
	DWORD XInputErr=XInputGetState( port, &InputState );

	if(	XInputErr == ERROR_SUCCESS) {

		//Action
		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_A )
			pad_status &= PSX_BUTTON_CROSS;

		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_B )
			pad_status &= PSX_BUTTON_CIRCLE;

		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_X )
			pad_status &= PSX_BUTTON_SQUARE;

		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_Y )
			pad_status &= PSX_BUTTON_TRIANGLE;

		//back & start
		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK )
			pad_status &= PSX_BUTTON_SELECT;

		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_START )
			pad_status &= PSX_BUTTON_START;

		//selection
		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT )
			pad_status &= PSX_BUTTON_DLEFT;

		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
			pad_status &= PSX_BUTTON_DRIGHT;

		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN )
			pad_status &= PSX_BUTTON_DDOWN;

		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP )
			pad_status &= PSX_BUTTON_DUP;

		//L/R
		//SHOULDER LB/RB
		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
			pad_status &= PSX_BUTTON_L1;

		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
			pad_status &= PSX_BUTTON_R1;

		//TRIGGER RT/RB
		if(InputState.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD )
			pad_status &= PSX_BUTTON_L2;

		if(InputState.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD )
			pad_status &= PSX_BUTTON_R2;

		//Analog	
		ls_x= (int)(float(InputState.Gamepad.sThumbLX/0x500)*256)+128;
		ls_y= (int)(float(InputState.Gamepad.sThumbLY/0x500)*256)+128;

		rs_x= (int)(float(InputState.Gamepad.sThumbRX/0x500)*256)+128;
		rs_y= (int)(float(InputState.Gamepad.sThumbRY/0x500)*256)+128;

		pad->leftJoyX = ls_x;
		pad->leftJoyY = ls_y;

		pad->rightJoyX = rs_x;
		pad->rightJoyY = rs_x;
	}

	pad->controllerType = PSE_PAD_TYPE_STANDARD; 	// Standard Pad
	pad->buttonStatus = pad_status;					//Copy Buttons
};