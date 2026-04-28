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

/* Provided by libretro_core.cpp: returns the PSE_PAD_TYPE_* selected by the
 * frontend via retro_set_controller_port_device for the given port. */
extern "C" int libretro_get_pad_type(int port);

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

	XINPUT_STATE InputState;
	DWORD XInputErr = XInputGetState(port, &InputState);

	if (XInputErr == ERROR_SUCCESS) {

		/* Face buttons */
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_A)             pad_status &= PSX_BUTTON_CROSS;
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_B)             pad_status &= PSX_BUTTON_CIRCLE;
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_X)             pad_status &= PSX_BUTTON_SQUARE;
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_Y)             pad_status &= PSX_BUTTON_TRIANGLE;

		/* Menu buttons */
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)          pad_status &= PSX_BUTTON_SELECT;
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_START)         pad_status &= PSX_BUTTON_START;

		/* D-pad */
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)     pad_status &= PSX_BUTTON_DLEFT;
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)    pad_status &= PSX_BUTTON_DRIGHT;
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)     pad_status &= PSX_BUTTON_DDOWN;
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)       pad_status &= PSX_BUTTON_DUP;

		/* Shoulder buttons */
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) pad_status &= PSX_BUTTON_L1;
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)pad_status &= PSX_BUTTON_R1;

		/* Triggers as digital L2/R2 */
		if (InputState.Gamepad.bLeftTrigger  > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) pad_status &= PSX_BUTTON_L2;
		if (InputState.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) pad_status &= PSX_BUTTON_R2;

		/* Stick clicks (L3 / R3) — present on DualShock, ignored on standard */
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)    pad_status &= ~(1 << 1); /* L3 */
		if (InputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)   pad_status &= ~(1 << 2); /* R3 */

		/* Analog sticks.
		 * XInput range: -32768..32767.  PSX range: 0..255, center=128.
		 * PSX Y convention: 0=up, 255=down  (opposite of XInput where +Y=up).
		 * Formula: (value >> 8) + 128  maps [-32768,32767] -> [0,255] exactly. */
		pad->leftJoyX  = (uint8_t)(((int)InputState.Gamepad.sThumbLX  >> 8) + 128);
		pad->leftJoyY  = (uint8_t)((-(int)InputState.Gamepad.sThumbLY >> 8) + 128);
		pad->rightJoyX = (uint8_t)(((int)InputState.Gamepad.sThumbRX  >> 8) + 128);
		pad->rightJoyY = (uint8_t)((-(int)InputState.Gamepad.sThumbRY >> 8) + 128);
	}

	/* Use the controller type selected by the frontend (Standard / DualShock / Analog).
	 * Default is PSE_PAD_TYPE_STANDARD when the frontend has not called
	 * retro_set_controller_port_device or if XInput read failed. */
	pad->controllerType = libretro_get_pad_type(port);
	pad->buttonStatus   = pad_status;
};