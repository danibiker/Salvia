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

/* Provided by libretro_core.cpp: returns the latest polled input state
 * (buttons bitmask + analog axes) that already respects libretro remapping. */
extern "C" void libretro_get_pad_state(int port, uint16_t *buttons,
                                        uint8_t *lx, uint8_t *ly,
                                        uint8_t *rx, uint8_t *ry);

/* Provided by libretro_core.cpp: deltas del raton de PSX ya acotados al byte
 * con signo del protocolo.  0,0 en puertos que no sean raton. */
extern "C" void libretro_get_mouse_state(int port, signed char *dx, signed char *dy);

/* Provided by libretro_core.cpp: posicion absoluta del lightgun ya normalizada
 * a 0..1023, o PSXGUN_OFFSCREEN si no apunta a la tele. */
extern "C" void libretro_get_gun_state(int port, int *absx, int *absy);

void PSxInputReadPort(PadDataS* pad, int port){
	uint16_t pad_status = 0xFFFF;
	uint8_t lx = 128, ly = 128, rx = 128, ry = 128;

	libretro_get_pad_state(port, &pad_status, &lx, &ly, &rx, &ry);

	pad->leftJoyX  = lx;
	pad->leftJoyY  = ly;
	pad->rightJoyX = rx;
	pad->rightJoyY = ry;
	pad->controllerType = libretro_get_pad_type(port);
	pad->buttonStatus   = pad_status;

	/* Raton de PSX: el case PSE_PAD_TYPE_MOUSE de _PADpoll (plugins.c) lee
	 * moveX/moveY.  Son DELTAS con signo, no coordenadas, y libretro_get_mouse_state
	 * ya los entrega acotados al byte que manda el protocolo. */
	if (pad->controllerType == PSE_PAD_TYPE_MOUSE) {
		signed char dx = 0, dy = 0;
		libretro_get_mouse_state(port, &dx, &dy);
		pad->moveX = (unsigned char)dx;
		pad->moveY = (unsigned char)dy;
	} else {
		pad->moveX = 0;
		pad->moveY = 0;
	}

	/* Lightgun: el case PSE_PAD_TYPE_GUNCON de _PADpoll (plugins.c) convierte
	 * estos 0..1023 en posicion de BARRIDO, para lo que necesita la geometria
	 * de display (PEOPS_GPUgetScreenInfo).  Fuera del GunCon se deja el
	 * centinela de "no apunta a la tele" en vez de 0,0. */
	if (pad->controllerType == PSE_PAD_TYPE_GUNCON) {
		int absx = PSXGUN_OFFSCREEN, absy = PSXGUN_OFFSCREEN;
		libretro_get_gun_state(port, &absx, &absy);
		pad->absoluteX = absx;
		pad->absoluteY = absy;
	} else {
		pad->absoluteX = PSXGUN_OFFSCREEN;
		pad->absoluteY = PSXGUN_OFFSCREEN;
	}
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