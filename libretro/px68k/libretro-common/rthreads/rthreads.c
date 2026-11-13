/*
 * Platform threading primitives for the PX68K vendored libretro-common subset.
 * API follows libretro-common rthreads (sthread/slock/scond).
 */
#include <stdlib.h>
#include <string.h>
#include <rthreads/rthreads.h>

#if defined(_XBOX)
/* Xbox 360.  The XDK has no CONDITION_VARIABLE (a Vista API), so the _WIN32
 * branch below does not build.  This one uses the kernel objects the XDK
 * does have: CRITICAL_SECTION for slock, and for scond a semaphore with a
 * waiter count kept under the caller's lock.
 *
 * That scond is exact when, as everywhere in PX68K, scond_signal and
 * scond_broadcast are called with the lock held and every scond_wait sits
 * in a loop on its condition: a waiter is counted before it releases the
 * lock, so no signal is lost, and a spurious wake-up only repeats a test.
 *
 * Threads are pinned to one hardware thread.  Salvia runs its main thread
 * on 0 and the emulation on 1, both on core 0; 2 is the first hardware
 * thread of core 1, so the MIDI synth gets a physical core to itself.  On
 * 4 the SC-55 measured ~8% slower: core 2 is shared with SDL's audio
 * thread, which libSDLx360 pins to 5. */
#include <xtl.h>
#ifndef PX68K_RTHREADS_XBOX_HWTHREAD
#define PX68K_RTHREADS_XBOX_HWTHREAD 2
#endif
#ifndef PX68K_RTHREADS_XBOX_STACK
#define PX68K_RTHREADS_XBOX_STACK (256 * 1024)
#endif
struct sthread { HANDLE handle; };
struct slock { CRITICAL_SECTION mutex; };
struct scond { HANDLE sema; LONG waiters; };
struct thread_start { void (*fn)(void *); void *arg; };
static DWORD WINAPI rthreads_thread_start(LPVOID opaque)
{
   struct thread_start *start = (struct thread_start*)opaque;
   void (*fn)(void*) = start->fn;
   void *arg = start->arg;
   free(start);
   fn(arg);
   return 0;
}
sthread_t *sthread_create(void (*fn)(void *), void *arg)
{
   sthread_t *thread;
   struct thread_start *start;
   if (!fn) return NULL;
   thread = (sthread_t*)calloc(1, sizeof(*thread));
   start = (struct thread_start*)malloc(sizeof(*start));
   if (!thread || !start) { free(thread); free(start); return NULL; }
   start->fn = fn; start->arg = arg;
   thread->handle = CreateThread(NULL, PX68K_RTHREADS_XBOX_STACK,
         rthreads_thread_start, start, CREATE_SUSPENDED, NULL);
   if (!thread->handle) { free(start); free(thread); return NULL; }
   XSetThreadProcessor(thread->handle, PX68K_RTHREADS_XBOX_HWTHREAD);
   SetThreadPriority(thread->handle, THREAD_PRIORITY_ABOVE_NORMAL);
   ResumeThread(thread->handle);
   return thread;
}
void sthread_join(sthread_t *thread)
{
   if (!thread) return;
   WaitForSingleObject(thread->handle, INFINITE);
   CloseHandle(thread->handle);
   free(thread);
}
slock_t *slock_new(void)
{
   slock_t *lock = (slock_t*)malloc(sizeof(*lock));
   if (lock) InitializeCriticalSection(&lock->mutex);
   return lock;
}
void slock_free(slock_t *lock) { if (lock) { DeleteCriticalSection(&lock->mutex); free(lock); } }
void slock_lock(slock_t *lock) { EnterCriticalSection(&lock->mutex); }
void slock_unlock(slock_t *lock) { LeaveCriticalSection(&lock->mutex); }
scond_t *scond_new(void)
{
   scond_t *cond = (scond_t*)malloc(sizeof(*cond));
   if (!cond) return NULL;
   cond->waiters = 0;
   cond->sema = CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
   if (!cond->sema) { free(cond); return NULL; }
   return cond;
}
void scond_free(scond_t *cond) { if (cond) { CloseHandle(cond->sema); free(cond); } }
void scond_wait(scond_t *cond, slock_t *lock)
{
   cond->waiters++;                       /* under the caller's lock */
   LeaveCriticalSection(&lock->mutex);
   WaitForSingleObject(cond->sema, INFINITE);
   EnterCriticalSection(&lock->mutex);
}
void scond_signal(scond_t *cond)
{
   if (cond->waiters > 0)
   {
      cond->waiters--;
      ReleaseSemaphore(cond->sema, 1, NULL);
   }
}
void scond_broadcast(scond_t *cond)
{
   if (cond->waiters > 0)
   {
      LONG n = cond->waiters;
      cond->waiters = 0;
      ReleaseSemaphore(cond->sema, n, NULL);
   }
}

#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
struct sthread { HANDLE handle; };
struct slock { CRITICAL_SECTION mutex; };
struct scond { CONDITION_VARIABLE cond; };
struct thread_start { void (*fn)(void *); void *arg; };
static DWORD WINAPI rthreads_thread_start(LPVOID opaque)
{
   struct thread_start *start = (struct thread_start*)opaque;
   void (*fn)(void*) = start->fn;
   void *arg = start->arg;
   free(start);
   fn(arg);
   return 0;
}
sthread_t *sthread_create(void (*fn)(void *), void *arg)
{
   sthread_t *thread;
   struct thread_start *start;
   if (!fn) return NULL;
   thread = (sthread_t*)calloc(1, sizeof(*thread));
   start = (struct thread_start*)malloc(sizeof(*start));
   if (!thread || !start) { free(thread); free(start); return NULL; }
   start->fn = fn; start->arg = arg;
   thread->handle = CreateThread(NULL, 0, rthreads_thread_start, start, 0, NULL);
   if (!thread->handle) { free(start); free(thread); return NULL; }
   return thread;
}
void sthread_join(sthread_t *thread)
{
   if (!thread) return;
   WaitForSingleObject(thread->handle, INFINITE);
   CloseHandle(thread->handle);
   free(thread);
}
slock_t *slock_new(void)
{
   slock_t *lock = (slock_t*)malloc(sizeof(*lock));
   if (lock) InitializeCriticalSection(&lock->mutex);
   return lock;
}
void slock_free(slock_t *lock) { if (lock) { DeleteCriticalSection(&lock->mutex); free(lock); } }
void slock_lock(slock_t *lock) { EnterCriticalSection(&lock->mutex); }
void slock_unlock(slock_t *lock) { LeaveCriticalSection(&lock->mutex); }
scond_t *scond_new(void)
{
   scond_t *cond = (scond_t*)malloc(sizeof(*cond));
   if (cond) InitializeConditionVariable(&cond->cond);
   return cond;
}
void scond_free(scond_t *cond) { free(cond); }
void scond_wait(scond_t *cond, slock_t *lock) { SleepConditionVariableCS(&cond->cond, &lock->mutex, INFINITE); }
void scond_signal(scond_t *cond) { WakeConditionVariable(&cond->cond); }
void scond_broadcast(scond_t *cond) { WakeAllConditionVariable(&cond->cond); }

#elif defined(__SWITCH__)
#include <switch.h>
struct sthread { Thread thread; };
struct slock { Mutex mutex; };
struct scond { CondVar cond; };
struct thread_start { void (*fn)(void *); void *arg; };
static void rthreads_thread_start(void *opaque)
{
   struct thread_start *start = (struct thread_start*)opaque;
   void (*fn)(void*) = start->fn;
   void *arg = start->arg;
   free(start);
   fn(arg);
}
sthread_t *sthread_create(void (*fn)(void *), void *arg)
{
   sthread_t *thread;
   struct thread_start *start;
   if (!fn) return NULL;
   thread = (sthread_t*)calloc(1, sizeof(*thread));
   start = (struct thread_start*)malloc(sizeof(*start));
   if (!thread || !start) { free(thread); free(start); return NULL; }
   start->fn = fn; start->arg = arg;
   if (R_FAILED(threadCreate(&thread->thread, rthreads_thread_start, start,
         NULL, 128 * 1024, 0x2B, -2)))
   { free(start); free(thread); return NULL; }
   threadStart(&thread->thread);
   return thread;
}
void sthread_join(sthread_t *thread)
{
   if (!thread) return;
   threadWaitForExit(&thread->thread);
   threadClose(&thread->thread);
   free(thread);
}
slock_t *slock_new(void)
{
   slock_t *lock = (slock_t*)malloc(sizeof(*lock));
   if (lock) mutexInit(&lock->mutex);
   return lock;
}
void slock_free(slock_t *lock) { free(lock); }
void slock_lock(slock_t *lock) { mutexLock(&lock->mutex); }
void slock_unlock(slock_t *lock) { mutexUnlock(&lock->mutex); }
scond_t *scond_new(void)
{
   scond_t *cond = (scond_t*)malloc(sizeof(*cond));
   if (cond) condvarInit(&cond->cond);
   return cond;
}
void scond_free(scond_t *cond) { free(cond); }
void scond_wait(scond_t *cond, slock_t *lock) { condvarWait(&cond->cond, &lock->mutex); }
void scond_signal(scond_t *cond) { condvarWakeOne(&cond->cond); }
void scond_broadcast(scond_t *cond) { condvarWakeAll(&cond->cond); }

#else
#include <pthread.h>
struct sthread { pthread_t thread; };
struct slock { pthread_mutex_t mutex; };
struct scond { pthread_cond_t cond; };
struct thread_start { void (*fn)(void *); void *arg; };
static void *rthreads_thread_start(void *opaque)
{
   struct thread_start *start = (struct thread_start*)opaque;
   void (*fn)(void*) = start->fn;
   void *arg = start->arg;
   free(start);
   fn(arg);
   return NULL;
}
sthread_t *sthread_create(void (*fn)(void *), void *arg)
{
   sthread_t *thread;
   struct thread_start *start;
   if (!fn) return NULL;
   thread = (sthread_t*)calloc(1, sizeof(*thread));
   start = (struct thread_start*)malloc(sizeof(*start));
   if (!thread || !start) { free(thread); free(start); return NULL; }
   start->fn = fn; start->arg = arg;
   if (pthread_create(&thread->thread, NULL, rthreads_thread_start, start) != 0)
   { free(start); free(thread); return NULL; }
   return thread;
}
void sthread_join(sthread_t *thread)
{
   if (!thread) return;
   pthread_join(thread->thread, NULL);
   free(thread);
}
slock_t *slock_new(void)
{
   slock_t *lock = (slock_t*)malloc(sizeof(*lock));
   if (lock && pthread_mutex_init(&lock->mutex, NULL) != 0) { free(lock); return NULL; }
   return lock;
}
void slock_free(slock_t *lock) { if (lock) { pthread_mutex_destroy(&lock->mutex); free(lock); } }
void slock_lock(slock_t *lock) { pthread_mutex_lock(&lock->mutex); }
void slock_unlock(slock_t *lock) { pthread_mutex_unlock(&lock->mutex); }
scond_t *scond_new(void)
{
   scond_t *cond = (scond_t*)malloc(sizeof(*cond));
   if (cond && pthread_cond_init(&cond->cond, NULL) != 0) { free(cond); return NULL; }
   return cond;
}
void scond_free(scond_t *cond) { if (cond) { pthread_cond_destroy(&cond->cond); free(cond); } }
void scond_wait(scond_t *cond, slock_t *lock) { pthread_cond_wait(&cond->cond, &lock->mutex); }
void scond_signal(scond_t *cond) { pthread_cond_signal(&cond->cond); }
void scond_broadcast(scond_t *cond) { pthread_cond_broadcast(&cond->cond); }
#endif
