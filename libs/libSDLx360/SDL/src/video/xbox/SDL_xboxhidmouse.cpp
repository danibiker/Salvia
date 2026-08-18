/*
    SDL_xboxhidmouse.cpp — LECTOR del raton USB HID publicado por el plugin
    residente de DashLaunch `hidmouse.xex`.

    ARQUITECTURA (cambiada): antes este fichero hookeaba el kernel el mismo para
    leer el raton, pero como Salvia es "un .xex por core" y relanza con
    XLaunchNewImage (que recicla la memoria del proceso), los hooks/callback/estado
    quedaban colgando -> crash aleatorio y replug en cada core.

    Ahora TODO el trabajo HID lo hace un plugin residente de DashLaunch
    (plugins/hidmouse/, `hidmouse.xex`) que se carga al arranque y NUNCA se
    descarga. Ese plugin lee el raton y ACUMULA los deltas (cumulativos) en una
    struct global. Aqui, desde el titulo Salvia, solo se LEE esa struct de la
    memoria del plugin (en RGH/JTAG un titulo lee memoria de kernel sin problema) y
    se calcula el delta por diferencia. Sin hooks, sin detours, sin persistencia. Si
    el plugin no esta cargado, no-op (fail-safe por `magic`).

    El resto de la cadena no cambia: SDL_xboxmouse.c::XBOX_MouseUpdateHID llama a
    XBOX_HIDMouse_Drain y empuja los eventos a SDL.
*/

#include "SDL_xboxhidmouse.h"

#ifdef SDL_XBOX_HIDMOUSE

#include <xtl.h>
#include <stdint.h>

/* =====================================================================
   Contrato compartido con el plugin.
   MANTENER EN SINCRONIA con plugins/hidmouse/HidMouseShared.h
   ===================================================================== */
#define HIDMOUSE_SHARED_MAGIC   0x484D5345u   /* 'HMSE' */
#define HIDMOUSE_SHARED_VERSION 1u

typedef struct HidMouseShared {
	volatile uint32_t magic;
	volatile uint32_t version;
	volatile uint32_t seq;
	volatile int32_t  accumX;   /* cumulativo */
	volatile int32_t  accumY;   /* cumulativo */
	volatile int32_t  accumW;   /* cumulativo */
	volatile uint32_t buttons;  /* bit0=izq, bit1=der, bit2=medio */
} HidMouseShared;

/* El plugin es un sysdll con base FIJA 0x81F00000 (xex.xml). No hace falta saber
   la VA exacta de g_hidMouseShared (que podria cambiar entre builds del plugin):
   escaneamos la imagen del plugin desde su base buscando la firma magic+version.
   Asi se encuentra este donde este dentro de la imagen, sin .map ni ajustes. */
#define HIDMOUSE_PLUGIN_BASE   0x81F00000u
#define HIDMOUSE_SCAN_BYTES    0x00080000u   /* 512 KB: cota; el SEH corta al final real de la imagen */

static HidMouseShared* g_shared = 0;

/* Escanea la imagen del plugin buscando la struct por su firma (magic seguido de
 * version). SEH: si el plugin NO esta cargado, el rango puede estar sin mapear y
 * la lectura falla al pasar del final de la imagen -> lo tratamos como "sin
 * plugin" (no-op) en vez de crashear. La firma de 2 palabras hace practicamente
 * imposible un falso positivo. */
static HidMouseShared* LocateShared(void)
{
	volatile const uint32_t* p = (const uint32_t*)HIDMOUSE_PLUGIN_BASE;
	const uint32_t words = HIDMOUSE_SCAN_BYTES / 4u;
	uint32_t i;

	__try {
		for (i = 0; i + 1u < words; ++i) {
			if (p[i] == HIDMOUSE_SHARED_MAGIC && p[i + 1u] == HIDMOUSE_SHARED_VERSION)
				return (HidMouseShared*)(void*)&p[i];
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		/* fin de la imagen mapeada sin encontrar la firma -> plugin no cargado */
	}
	return 0;
}

extern "C" int XBOX_HIDMouse_Init(void)
{
	g_shared = LocateShared();
	return g_shared ? 1 : 0;
}

extern "C" void XBOX_HIDMouse_Quit(void)
{
	/* Nada que desinstalar: el plugin es residente. */
	g_shared = 0;
}

extern "C" int XBOX_isHidMousePluginConnected(){
	return g_shared ? 1 : 0;
}

extern "C" void XBOX_HIDMouse_Drain(int* dx, int* dy, int* dwheel, unsigned int* buttons)
{
	/* Estado por-consumidor: ultimo valor cumulativo leido. Cada .xex se "ceba" en
	 * su primera lectura (delta 0) tras arrancar/relanzarse. */
	static int32_t lastX = 0, lastY = 0, lastW = 0;
	static int     primed = 0;

	/* g_shared se fija una sola vez en Init (el plugin se carga al arranque, antes
	 * que Salvia). Aqui NO re-escaneamos por frame: si no hay plugin, no-op barato. */
	HidMouseShared* s = g_shared;
	if (!s) {
		primed = 0;
		if (dx) *dx = 0;
		if (dy) *dy = 0;
		if (dwheel) *dwheel = 0;
		if (buttons) *buttons = 0;
		return;
	}

	int32_t cx = s->accumX;
	int32_t cy = s->accumY;
	int32_t cw = s->accumW;

	if (!primed) {            /* primera lectura: no generar un salto enorme */
		lastX = cx; lastY = cy; lastW = cw;
		primed = 1;
	}

	int32_t vx = cx - lastX; lastX = cx;   /* diff modulo 2^32 (resta con envoltura) */
	int32_t vy = cy - lastY; lastY = cy;
	int32_t vw = cw - lastW; lastW = cw;

	if (dx)      *dx = (int)vx;
	if (dy)      *dy = (int)vy;
	if (dwheel)  *dwheel = (int)vw;
	if (buttons) *buttons = (unsigned int)s->buttons;
}

#endif /* SDL_XBOX_HIDMOUSE */
