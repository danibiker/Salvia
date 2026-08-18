/* [XBOX360] Primitivas de hilo (sthread/slock/scond) sobre Win32/XDK para el
 * cdrom-async.c de pcsx_rearmed (USE_ASYNC_CDROM). El worker de prefetch de CD
 * es el unico consumidor: un solo hilo, un solo lock y un condvar de un unico
 * waiter, por lo que un evento AUTO-RESET basta como variable de condicion
 * (recuerda una senal, sin lost-wakeups). */

#include <stdlib.h>
#include "pcsxr-threads.h"

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

int pcsxr_sthread_core_count = 0;

void pcsxr_sthread_init(void)
{
	/* no-op en el 360: el pin de afinidad se hace en pcsxr_sthread_create */
}

struct sthread { HANDLE h; void (*fn)(void*); };
struct slock  { CRITICAL_SECTION cs; };
struct scond  { HANDLE ev; };   /* evento auto-reset */

static DWORD WINAPI sthread_trampoline(LPVOID p)
{
	struct sthread *t = (struct sthread *)p;
	t->fn(NULL);
	return 0;
}

struct sthread *pcsxr_sthread_create(void (*thread_func)(void*),
	enum pcsxr_thread_type type)
{
	struct sthread *t = (struct sthread *)malloc(sizeof(*t));
	if (!t)
		return NULL;
	t->fn = thread_func;
	t->h = CreateThread(NULL, 0, sthread_trampoline, t, CREATE_SUSPENDED, NULL);
	if (!t->h) {
		free(t);
		return NULL;
	}
#ifdef _XBOX
	/* Pinear el worker de CD a HW4 (core 2). Mapa real de HW threads en
	 * gameplay:
	 *   HW0 core0 = kernel (reservado parcial) + hermano SMT de la emulacion
	 *   HW1 core0 = emulacion (CPU_THREAD)  -> NUNCA
	 *   HW2 core1 = hilo GPU = BUSY-SPIN 100% (gpu.c gpu_thread_proc, YieldProcessor) -> NUNCA
	 *   HW3 core1 = hermano SMT del spinner de GPU -> muerto de hambre, NO
	 *   HW4 core2 = IO_THREAD, OCIOSO en juego (carga/HTTP solo en menu) -> AQUI
	 *   HW5 core2 = audio, Sleep(1) casi ocioso -> reservado al audio
	 * HW0 es el peor sitio: roba unidades de ejecucion a la emulacion (mismo
	 * core0) y esta reservado al kernel -> puede colgar el CreateThread del hilo
	 * de GPU (gpuDmaThreadInit) de forma intermitente. HW3 se ahoga por el
	 * spinner de GPU en HW2. El core 2 (HW4/HW5) es el unico libre en gameplay
	 * (audio ya NO hace busy-wait, cede con Sleep(1)), asi que el worker de CD
	 * vuelve a HW4, como el prefetcher viejo. */
	if (type == PCSXRT_CDR)
		XSetThreadProcessor(t->h, 4);
#else
	(void)type;
#endif
	ResumeThread(t->h);
	return t;
}

void sthread_join(struct sthread *thread)
{
	if (!thread)
		return;
	if (thread->h) {
		WaitForSingleObject(thread->h, INFINITE);
		CloseHandle(thread->h);
	}
	free(thread);
}

struct slock *slock_new(void)
{
	struct slock *l = (struct slock *)malloc(sizeof(*l));
	if (l)
		InitializeCriticalSection(&l->cs);
	return l;
}

void slock_free(struct slock *lock)
{
	if (!lock)
		return;
	DeleteCriticalSection(&lock->cs);
	free(lock);
}

void slock_lock(struct slock *lock)
{
	if (lock)
		EnterCriticalSection(&lock->cs);
}

void slock_unlock(struct slock *lock)
{
	if (lock)
		LeaveCriticalSection(&lock->cs);
}

struct scond *scond_new(void)
{
	struct scond *c = (struct scond *)malloc(sizeof(*c));
	if (c) {
		c->ev = CreateEvent(NULL, FALSE /*auto-reset*/, FALSE, NULL);
		if (!c->ev) {
			free(c);
			c = NULL;
		}
	}
	return c;
}

void scond_free(struct scond *cond)
{
	if (!cond)
		return;
	if (cond->ev)
		CloseHandle(cond->ev);
	free(cond);
}

void scond_wait(struct scond *cond, struct slock *lock)
{
	/* Soltar el lock, esperar la senal, re-adquirir. El caller re-comprueba
	 * su predicado tras volver (tolerante a wakeups espurios). */
	if (lock)
		LeaveCriticalSection(&lock->cs);
	if (cond && cond->ev)
		WaitForSingleObject(cond->ev, INFINITE);
	if (lock)
		EnterCriticalSection(&lock->cs);
}

void scond_signal(struct scond *cond)
{
	if (cond && cond->ev)
		SetEvent(cond->ev);
}
