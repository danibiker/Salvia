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

    VARIOS RATONES: el plugin publica ademas un bloque de EXTENSION con los deltas
    de cada raton por separado (XBOX_HIDMouse_DeviceCount/DeviceConnected/
    DrainDevice). Eso NO pasa por SDL: SDL 1.2 tiene un unico cursor global y
    volveria a fusionarlos. Lo consume Salvia directamente (dos punteros
    independientes, p.ej. dos RETRO_DEVICE_LIGHTGUN); por SDL sigue yendo solo el
    raton virtual agregado, que es el que mueve el cursor del menu.
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

/* Bloque de extension (deltas por raton). Va DETRAS del bloque base y tiene su
 * propia firma, precisamente para que la version del bloque base no cambie: el
 * canal se localiza escaneando magic+version, asi que subirla dejaria sin raton
 * -- y en silencio -- a los .xex compilados antes. Un plugin antiguo simplemente
 * no tiene estos campos y la firma no casa. */
#define HIDMOUSE_EXT_MAGIC      0x484D5832u   /* 'HMX2' */
#define HIDMOUSE_EXT_VERSION    2u
#define HIDMOUSE_MAX_DEVICES    4

typedef struct HidMouseDevice {
	volatile int32_t  accumX;     /* cumulativo, por raton */
	volatile int32_t  accumY;
	volatile int32_t  accumW;
	volatile uint32_t buttons;    /* bit0=izq, bit1=der, bit2=medio */
	volatile uint32_t seq;
	volatile uint32_t connected;
	volatile uint32_t vid;
	volatile uint32_t pid;
	volatile uint32_t hasWheel;
	volatile uint32_t reserved;
} HidMouseDevice;

typedef struct HidMouseShared {
	/* --- bloque base (version 1) --- */
	volatile uint32_t magic;
	volatile uint32_t version;
	volatile uint32_t seq;
	volatile int32_t  accumX;   /* cumulativo, SUMA de todos los ratones */
	volatile int32_t  accumY;
	volatile int32_t  accumW;
	volatile uint32_t buttons;  /* bit0=izq, bit1=der, bit2=medio (OR de todos) */
	/* --- extension (version 2): solo valida si extMagic/extVersion casan --- */
	volatile uint32_t extMagic;
	volatile uint32_t extVersion;
	volatile uint32_t maxDevices;
	volatile uint32_t numDevices;
	HidMouseDevice    dev[HIDMOUSE_MAX_DEVICES];
} HidMouseShared;

/* El plugin es un sysdll con base FIJA 0x81F00000 (xex.xml). No hace falta saber
   la VA exacta de g_hidMouseShared (que podria cambiar entre builds del plugin):
   escaneamos la imagen del plugin desde su base buscando la firma magic+version.
   Asi se encuentra este donde este dentro de la imagen, sin .map ni ajustes. */
#define HIDMOUSE_PLUGIN_BASE   0x81F00000u
#define HIDMOUSE_SCAN_BYTES    0x00080000u   /* 512 KB: cota; el SEH corta al final real de la imagen */

static HidMouseShared* g_shared = 0;
static int             g_hasExt = 0;   /* el plugin publica deltas por raton */

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

/* ¿Trae este plugin el bloque de extension? Con un plugin v1 la struct acaba en
 * `buttons` y estos campos caen en lo que haya detras en su .data: memoria
 * MAPEADA (no falla), pero basura -- que la firma de dos palabras descarta. El
 * SEH es por si la struct estuviera justo al final de la imagen. */
static int ProbeExt(HidMouseShared* s)
{
	if (!s)
		return 0;
	__try {
		if (s->extMagic != HIDMOUSE_EXT_MAGIC)     return 0;
		if (s->extVersion != HIDMOUSE_EXT_VERSION) return 0;
		if (s->maxDevices == 0u || s->maxDevices > HIDMOUSE_MAX_DEVICES) return 0;
		return 1;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

extern "C" int XBOX_HIDMouse_Init(void)
{
	g_shared = LocateShared();
	g_hasExt = ProbeExt(g_shared);
	return g_shared ? 1 : 0;
}

extern "C" void XBOX_HIDMouse_Quit(void)
{
	/* Nada que desinstalar: el plugin es residente. */
	g_shared = 0;
	g_hasExt = 0;
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

/* ===================== Canal por raton (no pasa por SDL) ================ */

extern "C" int XBOX_HIDMouse_DeviceCount(void)
{
	uint32_t n;
	if (!g_shared || !g_hasExt)
		return 0;                       /* sin extension: solo el agregado de SDL */
	n = g_shared->maxDevices;
	if (n > HIDMOUSE_MAX_DEVICES)
		n = HIDMOUSE_MAX_DEVICES;
	return (int)n;
}

extern "C" int XBOX_HIDMouse_DeviceConnected(int idx)
{
	if ((unsigned int)idx >= (unsigned int)XBOX_HIDMouse_DeviceCount())
		return 0;
	return g_shared->dev[idx].connected ? 1 : 0;
}

extern "C" int XBOX_HIDMouse_DeviceInfo(int idx, unsigned int* vid, unsigned int* pid)
{
	if ((unsigned int)idx >= (unsigned int)XBOX_HIDMouse_DeviceCount()) {
		if (vid) *vid = 0;
		if (pid) *pid = 0;
		return 0;
	}
	if (vid) *vid = (unsigned int)g_shared->dev[idx].vid;
	if (pid) *pid = (unsigned int)g_shared->dev[idx].pid;
	return 1;
}

extern "C" void XBOX_HIDMouse_DrainDevice(int idx, int* dx, int* dy, int* dwheel,
                                          unsigned int* buttons)
{
	/* Mismo esquema que el agregado pero con un "ultimo leido" por slot. No hace
	 * falta descebar al desconectar: el plugin NO resetea los acumuladores, asi
	 * que al reconectar (o al ocupar otro raton el slot) la diferencia sigue
	 * siendo 0 hasta que ese raton se mueva. */
	static int32_t lastX[HIDMOUSE_MAX_DEVICES] = { 0 };
	static int32_t lastY[HIDMOUSE_MAX_DEVICES] = { 0 };
	static int32_t lastW[HIDMOUSE_MAX_DEVICES] = { 0 };
	static int     primed[HIDMOUSE_MAX_DEVICES] = { 0 };

	int32_t cx, cy, cw, vx, vy, vw;

	if ((unsigned int)idx >= (unsigned int)XBOX_HIDMouse_DeviceCount()) {
		if (dx) *dx = 0;
		if (dy) *dy = 0;
		if (dwheel) *dwheel = 0;
		if (buttons) *buttons = 0;
		return;
	}

	cx = g_shared->dev[idx].accumX;
	cy = g_shared->dev[idx].accumY;
	cw = g_shared->dev[idx].accumW;

	if (!primed[idx]) {
		lastX[idx] = cx; lastY[idx] = cy; lastW[idx] = cw;
		primed[idx] = 1;
	}

	vx = cx - lastX[idx]; lastX[idx] = cx;
	vy = cy - lastY[idx]; lastY[idx] = cy;
	vw = cw - lastW[idx]; lastW[idx] = cw;

	if (dx)      *dx = (int)vx;
	if (dy)      *dy = (int)vy;
	if (dwheel)  *dwheel = (int)vw;
	if (buttons) *buttons = (unsigned int)g_shared->dev[idx].buttons;
}

#endif /* SDL_XBOX_HIDMOUSE */
