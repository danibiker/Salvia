// [Salvia] Puente core -> Direct3D para la GPU de CV1000.  Ver retro_core_gpu.h.
#include "retro_core_gpu.h"

#ifdef _XBOX
#include <xtl.h>

static CRITICAL_SECTION retroCoreGpuLock;
static volatile LONG    retroCoreGpuLockReady = 0;
static void            *retroCoreGpuDevice = NULL;
static unsigned         retroCoreGpuGeneration = 0;
static const char    *(*retroCoreGpuMessageSource)(void) = NULL;

static void RetroCoreGpuInitLock(void)
{
	// Se inicializa una vez y no se destruye: el blitter puede tenerlo pedido
	// mientras el driver se cierra, y vive lo mismo que el proceso.
	if (InterlockedCompareExchange(&retroCoreGpuLockReady, 1, 0) == 0) {
		InitializeCriticalSection(&retroCoreGpuLock);
		InterlockedExchange(&retroCoreGpuLockReady, 2);
	}
	while (retroCoreGpuLockReady != 2) Sleep(0);
}

void RetroCoreGpuSetDevice(void *device)
{
	RetroCoreGpuInitLock();
	EnterCriticalSection(&retroCoreGpuLock);
	if (device != retroCoreGpuDevice) {
		retroCoreGpuDevice = device;
		retroCoreGpuGeneration++;
	}
	LeaveCriticalSection(&retroCoreGpuLock);
}

void *RetroCoreGpuAcquire(unsigned *generation)
{
	RetroCoreGpuInitLock();
	EnterCriticalSection(&retroCoreGpuLock);
	if (!retroCoreGpuDevice) {
		LeaveCriticalSection(&retroCoreGpuLock);
		return NULL;
	}
	if (generation) *generation = retroCoreGpuGeneration;
	return retroCoreGpuDevice;
}

void RetroCoreGpuRelease(void)
{
	LeaveCriticalSection(&retroCoreGpuLock);
}

void RetroCoreGpuSetMessageSource(const char *(*source)(void))
{
	retroCoreGpuMessageSource = source;
}

const char *RetroCoreGpuTakeMessage(void)
{
	return retroCoreGpuMessageSource ? retroCoreGpuMessageSource() : NULL;
}
#endif
