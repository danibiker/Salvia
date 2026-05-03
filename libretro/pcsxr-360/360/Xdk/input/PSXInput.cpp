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
		 *
		 * Conversion gotcha: the formula (value>>8)+128 truncates incorrectly
		 * for value == -32768 on the inverted Y axes.  Negating -32768 yields
		 * +32768 (not representable in int16 but fine in int), which after
		 * >>8 gives 128, and +128 = 256.  Casting to uint8_t truncates to 0
		 * — which the PSX interprets as "stick up at max" instead of "down".
		 * Result: in Ape Escape and other analog-heavy games, pushing the
		 * stick fully down would lock the character into the "up" direction.
		 *
		 * Fix: convert via int math, clamp to [0,255], then cast. */
		{
			int lx = ( (int)InputState.Gamepad.sThumbLX  >> 8) + 128;
			int ly = (-(int)InputState.Gamepad.sThumbLY >> 8) + 128;
			int rx = ( (int)InputState.Gamepad.sThumbRX  >> 8) + 128;
			int ry = (-(int)InputState.Gamepad.sThumbRY >> 8) + 128;
			if (lx < 0) lx = 0; if (lx > 255) lx = 255;
			if (ly < 0) ly = 0; if (ly > 255) ly = 255;
			if (rx < 0) rx = 0; if (rx > 255) rx = 255;
			if (ry < 0) ry = 0; if (ry > 255) ry = 255;
			pad->leftJoyX  = (uint8_t)lx;
			pad->leftJoyY  = (uint8_t)ly;
			pad->rightJoyX = (uint8_t)rx;
			pad->rightJoyY = (uint8_t)ry;
		}
	}

	/* Use the controller type selected by the frontend (Standard / DualShock / Analog).
	 * Default is PSE_PAD_TYPE_STANDARD when the frontend has not called
	 * retro_set_controller_port_device or if XInput read failed. */
	pad->controllerType = libretro_get_pad_type(port);
	pad->buttonStatus   = pad_status;
};

/* ===========================================================================
 *  Vibration / rumble — DualShock command 0x4D path
 * ===========================================================================
 *
 *  Llamado desde plugins.c::_PADpoll cuando el state machine detecta que el
 *  juego envio bytes de rumble durante un poll DualShock.  Hacemos la
 *  traduccion del formato PSX (small=boolean, big=analog 0..255) al formato
 *  XInput (left=low-freq 0..65535, right=high-freq 0..65535).
 *
 *  Cache del ultimo valor enviado: si el juego envia las mismas intensidades
 *  cada frame (caso comun en arcade), evitamos hacer XInputSetState 60 veces
 *  por segundo — que tiene latencia y compite con XInputGetState. */
static unsigned short s_last_left[4]  = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
static unsigned short s_last_right[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };

extern "C" void PSxInputSetVibration(int port, unsigned char smallMotor, unsigned char bigMotor)
{
	if (port < 0 || port > 3) return;

	/* PSX big motor (0..255) -> XInput left (low-freq) escalado a 0..65535.
	 * Multiplicar por 257 mapea exacto: 0->0, 1->257, ..., 255->65535. */
	unsigned short left  = (unsigned short)(bigMotor * 257u);

	/* PSX small motor: documentado como digital pero algunos juegos pasan
	 * cualquier valor != 0.  Lo tratamos como ON/OFF en XInput right.
	 * Si el usuario quiere modulacion proporcional, cambiar aqui. */
	unsigned short right = smallMotor ? 0xFFFF : 0x0000;

	if (left == s_last_left[port] && right == s_last_right[port])
		return;   /* sin cambio, ahorrarse el syscall */

	s_last_left[port]  = left;
	s_last_right[port] = right;

	XINPUT_VIBRATION vib;
	vib.wLeftMotorSpeed  = left;
	vib.wRightMotorSpeed = right;
	XInputSetState((DWORD)port, &vib);
}