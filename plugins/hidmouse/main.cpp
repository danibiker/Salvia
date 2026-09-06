/*
    hidmouse.xex — plugin residente de DashLaunch para Xbox 360 (RGH/JTAG).

    Lee un raton USB HID (boot protocol) hookeando la pila USB del kernel y PUBLICA
    los deltas crudos (accumX/Y/wheel + botones) en la struct global g_hidMouseShared.
    Salvia (o cualquier titulo) LEE esa struct de la memoria del plugin y empuja los
    eventos a SDL. El plugin es residente (cargado por DashLaunch al arranque, nunca
    se descarga), asi que el raton sobrevive a los XLaunchNewImage de Salvia: sin
    crash y sin replug.

    VARIOS RATONES: se reclaman hasta HIDMOUSE_MAX_DEVICES y cada uno publica sus
    propios acumuladores en g_hidMouseShared.dev[i] (bloque de extension), ademas de
    seguir sumando al agregado del bloque base -- que es lo que quiere el cursor del
    menu, y lo unico que entienden los consumidores ya compilados.

    Codigo de lectura HID portado del port de Salvia (SDL_xboxhidmouse.cpp), que a su
    vez viene de x360remap/hiddriver360 (EinTim23). Aqui se descarta TODO lo de
    convertir el raton en mando (XAM/XInput), mapping, calibracion y Studio: el plugin
    solo lee y publica deltas.

    Requiere: consola CFW (RGH/JTAG); build de dashboard 17559 (retail) / 17489 (devkit).
    Fuera de esos builds no instala (fail-safe).
*/

#include <xtl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Detours.h"
#include "HidMouseShared.h"

/* --- Definicion de los miembros estaticos del Detour --- */
BYTE   Detour::TrampolineBuffer[200 * 20] = {};
SIZE_T Detour::TrampolineSize = 0;

#ifndef NT_ERROR
#define NT_ERROR(Status) ((((ULONG)(Status)) >> 30) == 3)
#endif

/* --- Depuracion opcional (OutputDebugStringA, visible por XBDM) --- */
#define HIDMOUSE_PLUGIN_DEBUG 0
#if HIDMOUSE_PLUGIN_DEBUG
#include <stdio.h>
#include <stdarg.h>
static void PlgDbg(const char* fmt, ...) {
	char buf[256];
	va_list ap; va_start(ap, fmt);
	_vsnprintf(buf, sizeof(buf) - 1, fmt, ap); buf[sizeof(buf) - 1] = 0;
	va_end(ap);
	OutputDebugStringA(buf);
}
#define PLGDBG(fmt, ...) PlgDbg("[hidmouse] " fmt "\n", __VA_ARGS__)
#define PLGDBG0(msg)     OutputDebugStringA("[hidmouse] " msg "\n")
#else
#define PLGDBG(fmt, ...) ((void)0)
#define PLGDBG0(msg)     ((void)0)
#endif

/* ============ Struct compartida (el canal plugin -> Salvia) ============
 * Es un GLOBAL del plugin. No hace falta su VA: el consumidor escanea la imagen
 * del plugin (base fija 0x81F00000) buscando la firma magic+version, asi que
 * puede moverse libremente al recompilar. Ver HidMouseShared.h. */
HidMouseShared g_hidMouseShared = { 0 };

/* ===================== Estructuras del stack USB (verbatim) ============ */

struct usb_device_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdUSB;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer;
	uint8_t  iProduct;
	uint8_t  iSerialNumber;
	uint8_t  bNumConfigurations;
};

struct usb_interface_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
};

struct usb_endpoint_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bEndpointAddress;
	uint8_t  bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t  bInterval;
};

struct UsbTrb {
	DWORD endpoint;
	DWORD callback;
	DWORD savedEndpoint;
	BYTE  padding[4];
	BYTE  flags;
	BYTE  controllerIndex;
	BYTE  pad2;
	BYTE  endpointIndex;
	void* buffer;
	DWORD length;
};

struct UsbPacket {
	BYTE  bmRequestType;
	BYTE  bRequest;
	WORD  wValue;
	WORD  wIndex;
	WORD  wLength;
};

struct UsbControlTrb {
	UsbTrb    trb;
	BYTE      pad[4];
	UsbPacket packet;
};

struct deviceHandle;
struct __declspec(align(2)) HidControllerExtension
{
	deviceHandle* deviceHandle;
	UsbTrb interruptTrb;
	BYTE gap20[4];
	UsbControlTrb controlTrb;
	BYTE gap4C[4];
	DWORD cleanupHandler;
	BYTE gap54[24];
	DWORD queue;
	BYTE alwaysOne;
	BYTE alwaysOneTwo;
	BYTE unknownFlag;
	BYTE alwaysZero;
	BYTE cleanupDone;
	BYTE initTransferPending;
	BYTE alwaysZeroTwo;
	unsigned __int8 deviceType;
	BYTE alwaysZeroThree;
	BYTE alwaysZeroFour;
};

struct deviceHandle {
	HidControllerExtension* driver;
};

#pragma pack(push, 1)
struct BootMouseReport {
	uint8_t buttons;
	int8_t  x;
	int8_t  y;
	int8_t  wheel;
};
#pragma pack(pop)

typedef struct _XBOX_KRNL_VERSION {
	WORD Major;
	WORD Minor;
	WORD Build;
	WORD Qfe;
} XBOX_KRNL_VERSION, *PXBOX_KRNL_VERSION;

#define USB_ENDPOINT_TYPE_INTERRUPT 0x03
#define USB_DIRECTION_IN            1
#define USB_HID_PROTOCOL_MOUSE      2

/* Ventana de arranque: no reclamar dispositivos durante los primeros segundos
 * (reclamar durante la enumeracion de boot puede congelar la consola, ver
 * x360remap). Un dispositivo conectado al arrancar se captura al reconectarlo. */
#define HIDMOUSE_BOOT_BLACKOUT_MS 15000

/* Emitir SET_PROTOCOL(boot) al reclamar (algunos ratones lo necesitan). */
#define HIDMOUSE_SEND_SET_PROTOCOL 1

/* Pausa de asentamiento en el reclamo (evita una carrera en la enumeracion USB
 * que reiniciaba la consola sin el retardo de los logs). No se puede Sleep en el
 * hook (posible DISPATCH_LEVEL); busy-wait. 0 lo desactiva. */
#define HIDMOUSE_CLAIM_SETTLE_SPINS 3000000

/* ===================== Punteros a funciones del kernel ================= */

typedef usb_device_descriptor*    (*usb_device_descriptor_func_t)(deviceHandle*);
typedef usb_interface_descriptor* (*usb_interface_descriptor_func_t)(deviceHandle*);
typedef usb_endpoint_descriptor*  (*usb_endpoint_descriptor_func_t)(deviceHandle*, int index, int transfertype, int direction);
typedef int  (*usb_add_device_complete_func_t)(deviceHandle*, int status_code);
typedef LONG (*usb_open_default_endpoint_func_t)(deviceHandle*, DWORD* endpoint);
typedef LONG (*usb_open_endpoint_func_t)(deviceHandle*, int transfertype, int endpointAddress, int maxPacketLength, int interval, DWORD* endpoint);
typedef int  (*usb_queue_async_transfer_func_t)(deviceHandle*, void* endpoint);

typedef int  (*hid_add_device_fn)(deviceHandle*);
typedef int  (*hid_remove_device_fn)(deviceHandle*);

static usb_device_descriptor_func_t      UsbdGetDeviceDescriptor    = 0;
static usb_interface_descriptor_func_t   UsbdGetInterfaceDescriptor = 0;   /* hardcoded por build */
static usb_endpoint_descriptor_func_t    UsbdGetEndpointDescriptor  = 0;
static usb_add_device_complete_func_t    UsbdAddDeviceComplete      = 0;
static usb_open_default_endpoint_func_t  UsbdOpenDefaultEndpoint    = 0;
static usb_open_endpoint_func_t          UsbdOpenEndpoint           = 0;
static usb_queue_async_transfer_func_t   UsbdQueueAsyncTransfer     = 0;

static void* g_hidAddDeviceAddr    = 0;   /* hardcoded por build */
static void* g_hidRemoveDeviceAddr = 0;   /* hardcoded por build */

static Detour g_hidAddDetour((void*)0, (const void*)0);
static Detour g_hidRemoveDetour((void*)0, (const void*)0);
static bool   g_installed = false;

/* ===================== Estado del/los raton(es) ======================= */

struct MouseSlot {
	deviceHandle*           dev;
	HidControllerExtension* drv;
	void*                   reportData;
	uint16_t                reportSize;
};
static MouseSlot g_mice[HIDMOUSE_MAX_DEVICES] = { {0} };

/* Recalcula lo que depende del CONJUNTO de ratones: cuantos hay conectados y el
 * estado de botones del raton virtual agregado.
 *
 * El agregado hace el OR de los botones de todos los ratones en vez de quedarse
 * con el ultimo report, que es lo que hacia antes: con dos ratones, el ultimo en
 * reportar ponia a cero el boton que el otro tenia pulsado. Con un solo raton el
 * OR es exactamente el valor de ese raton, asi que no cambia nada. */
static void RefreshAggregate(void)
{
	uint32_t btn = 0;
	uint32_t n   = 0;
	for (int i = 0; i < HIDMOUSE_MAX_DEVICES; i++) {
		if (!g_hidMouseShared.dev[i].connected)
			continue;
		btn |= g_hidMouseShared.dev[i].buttons;
		n++;
	}
	g_hidMouseShared.numDevices = n;
	g_hidMouseShared.buttons    = btn;
}

/* ===================== Direcciones por build ========================== */

struct KernelBuildAddrs {
	WORD  build;
	DWORD usbdGetInterfaceDescriptor;
	DWORD hidAddDevice;
	DWORD hidRemoveDevice;
};
static const KernelBuildAddrs g_builds[] = {
	/* Retail 17559.  IfaceDesc    HidAdd      HidRemove */
	{ 17559, 0x800D8500, 0x800E4D68, 0x800E4D28 },
	/* Devkit 17489. */
	{ 17489, 0x8010D2D0, 0x8011AE38, 0x8011ADF8 },
	/* TODO otras builds: rellenar con las direcciones de ESE kernel. */
};
static const int g_buildCount = sizeof(g_builds) / sizeof(g_builds[0]);

/* ===================== Helpers ======================================== */

static uint16_t swap16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }

static int32_t NoopComplete(DWORD /*trb*/, int32_t /*status*/) { return 0; }

/* Resolucion de exports del kernel por ORDINAL.
 *
 * Usamos GetModuleHandleA + GetProcAddress(handle, (LPCSTR)ordinal), que funciona
 * desde el titulo (probado en 17559). x360remap, en cambio, usa
 * XexGetProcedureAddress (via xkelib) en contexto de plugin.
 *
 * SI CON DEBUG (HIDMOUSE_PLUGIN_DEBUG) VES "no pude leer XboxKrnlVersion" o "fallo
 * resolviendo ordinales", GetProcAddress no va en este contexto: enlaza xkelib y
 * cambia KrnlProc por `XexGetProcedureAddress(k, ordinal, &ptr)`. */
static FARPROC KrnlProc(HMODULE k, int ordinal)
{
	return GetProcAddress(k, (LPCSTR)(DWORD_PTR)ordinal);
}

static WORD ReadKernelBuild(HMODULE k)
{
	DWORD p = (DWORD)KrnlProc(k, 344);   /* XboxKrnlVersion (export de datos) */
	if (p < 0x80000000)
		return 0;

	PXBOX_KRNL_VERSION a = (PXBOX_KRNL_VERSION)p;
	DWORD pp = *(volatile DWORD*)p;
	PXBOX_KRNL_VERSION b = (pp >= 0x80000000) ? (PXBOX_KRNL_VERSION)pp : 0;

	if (a->Major == 2 && a->Build >= 1000)
		return a->Build;
	if (b && b->Major == 2 && b->Build >= 1000)
		return b->Build;
	return 0;
}

#if HIDMOUSE_CLAIM_SETTLE_SPINS > 0
static void HidSettle(void) { for (volatile int s = 0; s < HIDMOUSE_CLAIM_SETTLE_SPINS; ++s) { } }
#else
static void HidSettle(void) { }
#endif

static void SendControlRequest(
	deviceHandle* dev, UsbControlTrb* controlTrb,
	uint8_t bmRequestType, uint8_t bRequest,
	uint16_t wValue, uint16_t wIndex, uint16_t wLength,
	void* data, DWORD completionCallback)
{
	controlTrb->packet.bmRequestType = bmRequestType;
	controlTrb->packet.bRequest = bRequest;
	controlTrb->packet.wValue = swap16(wValue);
	controlTrb->packet.wIndex = swap16(wIndex);
	controlTrb->packet.wLength = swap16(wLength);
	controlTrb->trb.buffer = data;
	controlTrb->trb.length = wLength;
	controlTrb->trb.flags = 1;
	controlTrb->trb.callback = completionCallback;
	controlTrb->trb.savedEndpoint = controlTrb->trb.endpoint;
	UsbdQueueAsyncTransfer(dev, controlTrb);
}

/* ===================== Callback de interrupcion (publica) ============== */

static int MouseInterruptHandler(DWORD trbParam, int32_t /*a2*/)
{
	HidControllerExtension* drv = (HidControllerExtension*)(trbParam - 4);

	if (!drv || !drv->deviceHandle || !drv->deviceHandle->driver ||
		drv->deviceHandle->driver->cleanupDone)
		return 0;

	int index = -1;
	for (int i = 0; i < HIDMOUSE_MAX_DEVICES; i++) {
		if (g_mice[i].drv == drv) { index = i; break; }
	}
	if (index < 0)
		return UsbdQueueAsyncTransfer(drv->deviceHandle, &drv->interruptTrb);

	const BootMouseReport* r = (const BootMouseReport*)drv->interruptTrb.buffer;
	HidMouseDevice* d = &g_hidMouseShared.dev[index];

	/* 1) Slot de ESTE raton (bloque de extension): es lo que permite dos punteros
	 *    independientes, p.ej. dos lightgun. */
	d->accumX += r->x;
	d->accumY += r->y;
	if (d->hasWheel)
		d->accumW += r->wheel;
	d->buttons = r->buttons;

	/* 2) Raton virtual agregado (bloque base): la suma de todos. Es lo que mueve el
	 *    cursor del menu y lo unico que ve un consumidor que no conozca la
	 *    extension. De los botones se encarga RefreshAggregate. */
	g_hidMouseShared.accumX += r->x;
	g_hidMouseShared.accumY += r->y;
	if (d->hasWheel)
		g_hidMouseShared.accumW += r->wheel;
	RefreshAggregate();

	/* Los seq van al final y detras de una barrera, para que el consumidor pueda
	 * usarlos como "todo lo de antes ya es visible" y no leer un estado a medias. */
	__lwsync();
	d->seq++;
	g_hidMouseShared.seq++;

	return UsbdQueueAsyncTransfer(drv->deviceHandle, &drv->interruptTrb);
}

/* ===================== Hooks HidAddDevice / HidRemoveDevice ============ */

static int MouseHidAddDeviceHook(deviceHandle* dev)
{
	usb_device_descriptor*    dd = UsbdGetDeviceDescriptor(dev);
	usb_interface_descriptor* id = UsbdGetInterfaceDescriptor(dev);

	/* Reclamar SOLO ratones boot-protocol; el resto pasa al handler original. */
	if (!dd || !id ||
		id->bInterfaceClass != 0x03 ||
		id->bInterfaceProtocol != USB_HID_PROTOCOL_MOUSE) {
		return g_hidAddDetour.GetOriginal<hid_add_device_fn>()(dev);
	}

	/* Ventana de arranque: no reclamar durante la enumeracion de boot. */
	if (GetTickCount() < HIDMOUSE_BOOT_BLACKOUT_MS) {
		PLGDBG0("boot blackout: defer, replug tras el arranque");
		return g_hidAddDetour.GetOriginal<hid_add_device_fn>()(dev);
	}

	HidSettle();

	int index = -1;
	for (int i = 0; i < HIDMOUSE_MAX_DEVICES; i++) {
		if (!g_mice[i].drv) { index = i; break; }
	}
	if (index < 0)
		return g_hidAddDetour.GetOriginal<hid_add_device_fn>()(dev);

	usb_endpoint_descriptor* ep = UsbdGetEndpointDescriptor(
		dev, 0, USB_ENDPOINT_TYPE_INTERRUPT, USB_DIRECTION_IN);
	if (!ep)
		return g_hidAddDetour.GetOriginal<hid_add_device_fn>()(dev);

	HidControllerExtension* drv = new HidControllerExtension();
	memset(drv, 0, sizeof(HidControllerExtension));
	drv->deviceType = 0;
	drv->deviceHandle = dev;
	drv->interruptTrb.flags = 1;
	dev->driver = drv;

	UsbdAddDeviceComplete(dev, 0);

	LONG st = UsbdOpenDefaultEndpoint(dev, (DWORD*)&drv->controlTrb);
	if (NT_ERROR(st)) {
		dev->driver = 0;
		delete drv;
		return (int)st;
	}

	uint16_t pktSize = swap16(ep->wMaxPacketSize) & 0x7FF;

#if HIDMOUSE_SEND_SET_PROTOCOL
	SendControlRequest(dev, &drv->controlTrb,
		0x21 /* Host->Device, Class, Interface */, 0x0B /* SET_PROTOCOL */,
		0 /* boot */, id->bInterfaceNumber, 0, 0, (DWORD)NoopComplete);
#endif

	st = UsbdOpenEndpoint(dev, 3, ep->bEndpointAddress, pktSize,
		ep->bInterval, (DWORD*)&drv->interruptTrb);
	if (NT_ERROR(st)) {
		dev->driver = 0;
		delete drv;
		return (int)st;
	}

	void* rd = malloc(pktSize * 2);
	memset(rd, 0, pktSize * 2);

	drv->interruptTrb.savedEndpoint = drv->interruptTrb.endpoint;
	drv->interruptTrb.length   = pktSize;
	drv->interruptTrb.callback = (DWORD)MouseInterruptHandler;
	drv->interruptTrb.buffer   = rd;

	g_mice[index].dev        = dev;
	g_mice[index].drv        = drv;
	g_mice[index].reportData = rd;
	g_mice[index].reportSize = pktSize;

	/* Publica el slot en la struct compartida. Los acumuladores NO se ponen a cero:
	 * son cumulativos y el consumidor saca el delta por diferencia, asi que
	 * resetearlos aqui le meteria un salto enorme a quien ya estuviera cebado.
	 * Dejandolos donde estan, el delta es 0 hasta que el raton nuevo se mueva. */
	{
		HidMouseDevice* d = &g_hidMouseShared.dev[index];
		d->buttons  = 0;
		d->hasWheel = (pktSize >= 4) ? 1u : 0u;
		/* idVendor/idProduct vienen little-endian en el descriptor, igual que
		 * wMaxPacketSize. */
		d->vid      = (uint32_t)swap16(dd->idVendor);
		d->pid      = (uint32_t)swap16(dd->idProduct);
		d->reserved = 0;
		__lwsync();          /* metadatos visibles ANTES de anunciar el slot */
		d->connected = 1;
	}
	RefreshAggregate();

	PLGDBG("RATON RECLAMADO slot=%d pktSize=%d", index, (int)pktSize);

	HidSettle();

	return UsbdQueueAsyncTransfer(dev, &drv->interruptTrb);
}

static int MouseHidRemoveDeviceHook(deviceHandle* dev)
{
	int index = -1;
	for (int i = 0; i < HIDMOUSE_MAX_DEVICES; i++) {
		if (g_mice[i].dev == dev) { index = i; break; }
	}
	if (index < 0)
		return g_hidRemoveDetour.GetOriginal<hid_remove_device_fn>()(dev);

	if (dev->driver && !dev->driver->cleanupDone) {
		dev->driver->cleanupDone = 1;

		void* rd = g_mice[index].reportData;
		HidControllerExtension* drv = g_mice[index].drv;

		g_mice[index].dev = 0;
		g_mice[index].drv = 0;
		g_mice[index].reportData = 0;
		g_mice[index].reportSize = 0;

		/* Baja el slot ANTES de recalcular, para que sus botones dejen de contar en
		 * el OR del agregado. Antes esto solo se limpiaba cuando no quedaba NINGUN
		 * raton, asi que desconectar uno con el boton pulsado lo dejaba pegado en el
		 * agregado hasta que el otro reportase. Los acumuladores se quedan como
		 * estan (cumulativos, ver el reclamo). */
		g_hidMouseShared.dev[index].connected = 0;
		g_hidMouseShared.dev[index].buttons   = 0;
		g_hidMouseShared.dev[index].hasWheel  = 0;
		g_hidMouseShared.dev[index].vid       = 0;
		g_hidMouseShared.dev[index].pid       = 0;
		RefreshAggregate();

		delete drv;
		dev->driver = 0;
		free(rd);
	}
	return 0;
}

/* ===================== Resolucion de kernel + gate por build =========== */

static bool ResolveKernelPointers(void)
{
	HMODULE k = GetModuleHandleA("xboxkrnl.exe");
	if (!k)
		return false;

	WORD build = ReadKernelBuild(k);
	if (build == 0) {
		PLGDBG0("ABORT: no pude leer XboxKrnlVersion");
		return false;
	}

	const KernelBuildAddrs* ba = 0;
	for (int bi = 0; bi < g_buildCount; bi++) {
		if (g_builds[bi].build == build) { ba = &g_builds[bi]; break; }
	}
	if (!ba) {
		PLGDBG("ABORT: build %u sin direcciones", (unsigned)build);
		return false;
	}

	UsbdGetDeviceDescriptor   = (usb_device_descriptor_func_t)     KrnlProc(k, 759);
	UsbdGetEndpointDescriptor = (usb_endpoint_descriptor_func_t)   KrnlProc(k, 744);
	UsbdAddDeviceComplete     = (usb_add_device_complete_func_t)   KrnlProc(k, 740);
	UsbdOpenDefaultEndpoint   = (usb_open_default_endpoint_func_t) KrnlProc(k, 746);
	UsbdOpenEndpoint          = (usb_open_endpoint_func_t)         KrnlProc(k, 747);
	UsbdQueueAsyncTransfer    = (usb_queue_async_transfer_func_t)  KrnlProc(k, 748);

	if (!UsbdGetDeviceDescriptor || !UsbdGetEndpointDescriptor || !UsbdAddDeviceComplete ||
		!UsbdOpenDefaultEndpoint || !UsbdOpenEndpoint || !UsbdQueueAsyncTransfer) {
		PLGDBG0("ABORT: fallo resolviendo ordinales Usbd*");
		return false;
	}

	UsbdGetInterfaceDescriptor = (usb_interface_descriptor_func_t)ba->usbdGetInterfaceDescriptor;
	g_hidAddDeviceAddr         = (void*)ba->hidAddDevice;
	g_hidRemoveDeviceAddr      = (void*)ba->hidRemoveDevice;

	PLGDBG("build %u OK", (unsigned)build);
	return true;
}

static bool InstallHooks(void)
{
	if (g_installed)
		return true;

	if (!ResolveKernelPointers())
		return false;

	g_hidAddDetour    = Detour(g_hidAddDeviceAddr,    (const void*)MouseHidAddDeviceHook);
	g_hidRemoveDetour = Detour(g_hidRemoveDeviceAddr, (const void*)MouseHidRemoveDeviceHook);

	if (!g_hidAddDetour.Install())
		return false;
	if (!g_hidRemoveDetour.Install()) {
		g_hidAddDetour.Remove();
		return false;
	}

	g_installed = true;
	PLGDBG0("hooks instalados (residente)");
	return true;
}

/* ===================== Punto de entrada del plugin ==================== */

BOOL APIENTRY DllMain(HANDLE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH) {
		/* Inicializa el canal compartido ANTES de instalar (Salvia puede estar
		 * leyendo el magic). */
		g_hidMouseShared.magic   = 0;   /* aun no listo */
		g_hidMouseShared.version = HIDMOUSE_SHARED_VERSION;
		g_hidMouseShared.seq     = 0;
		g_hidMouseShared.accumX  = 0;
		g_hidMouseShared.accumY  = 0;
		g_hidMouseShared.accumW  = 0;
		g_hidMouseShared.buttons = 0;

		/* Bloque de extension (deltas por raton). Va detras del base y se anuncia con
		 * su propia firma: un consumidor antiguo lee el agregado y ni se entera de que
		 * esto existe. */
		g_hidMouseShared.extMagic   = HIDMOUSE_EXT_MAGIC;
		g_hidMouseShared.extVersion = HIDMOUSE_EXT_VERSION;
		g_hidMouseShared.maxDevices = HIDMOUSE_MAX_DEVICES;
		g_hidMouseShared.numDevices = 0;
		memset((void*)g_hidMouseShared.dev, 0, sizeof(g_hidMouseShared.dev));

		if (InstallHooks()) {
			__lwsync();   /* todo lo de arriba visible antes de anunciar el canal */
			g_hidMouseShared.magic = HIDMOUSE_SHARED_MAGIC;   /* listo: Salvia puede leer */
			PLGDBG0("plugin activo");
		} else {
			PLGDBG0("plugin NO instalado (build/CFW?)");
		}
	}
	/* No manejamos DLL_PROCESS_DETACH: el plugin es residente y no se descarga. */
	return TRUE;
}
