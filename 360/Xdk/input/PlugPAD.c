/*
	Basic Analog PAD plugin for PCSX Gamecube
	by emu_kidid based on the DC/MacOSX HID plugin

	TODO: Rumble?
*/

#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "plugins.h"
#include "PsxCommon.h"
#include "PSEmu_Plugin_Defs.h"
#include "PSXInput.h"

/* Button Bits */
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

#define PSX_CONTROLLER_TYPE_STANDARD 0
#define PSX_CONTROLLER_TYPE_ANALOG 1

int controllerType = PSX_CONTROLLER_TYPE_STANDARD; // 0 = standard, 1 = analog (analog fails on old games)

long  PadFlags = 0;

long PAD__init(long flags) {
	PadFlags |= flags;
	return PSE_PAD_ERR_SUCCESS;
}

long PAD__shutdown(void) {
	return PSE_PAD_ERR_SUCCESS;
}

long PAD__open(void)
{
	return PSE_PAD_ERR_SUCCESS;
}

long PAD__close(void) {
	return PSE_PAD_ERR_SUCCESS;
}

long PAD__readPort1(PadDataS* pad) {
	PSxInputReadPort(pad, 0);
	return PSE_PAD_ERR_SUCCESS;
}

long PAD__readPort2(PadDataS* pad) {
	PSxInputReadPort(pad, 1);
	return PSE_PAD_ERR_SUCCESS;
}
