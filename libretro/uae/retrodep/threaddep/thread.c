 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Threading support, using pthreads
  *
  * Copyright 1997 Bernd Schmidt
  * Copyright 2004 Richard Drummond
  *
  * This handles initialization when using named semaphores.
  * Idea stolen from SDL.
  */

#include "sysconfig.h"
#include "sysdeps.h"
#include "thread.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif
#include <process.h>

void uae_sem_init (uae_sem_t * event, int manual_reset, int initial_state)
{
   if(*event) {
      if (initial_state)
         SetEvent (*event);
      else
         ResetEvent (*event);
   } else {
      *event = CreateEvent (NULL, manual_reset, initial_state, NULL);
   }
}

void uae_sem_wait (uae_sem_t * event)
{
   WaitForSingleObject (*event, INFINITE);
}

void uae_sem_unpost (uae_sem_t * event)
{
   ResetEvent(*event);
}

void uae_sem_post (uae_sem_t * event)
{
   SetEvent (*event);
}

int uae_sem_trywait_delay(uae_sem_t * event, int millis)
{
   int v = WaitForSingleObject(*event, millis);
   if (v == WAIT_OBJECT_0)
      return 0;
   if (v == WAIT_ABANDONED)
      return -2;
   return -1;
}

int uae_sem_trywait (uae_sem_t * event)
{
   return uae_sem_trywait_delay(event, 0);
}

void uae_sem_destroy (uae_sem_t * event)
{
   if (*event) {
      CloseHandle (*event);
      *event = NULL;
   }
}

uae_thread_id uae_thread_get_id(void)
{
   return (uae_thread_id)UlongToHandle(GetCurrentThreadId());
}

typedef unsigned (__stdcall *BEGINTHREADEX_FUNCPTR)(void *);

struct thparms
{
   void (*f)(void *);
   void *arg;
};

static unsigned __stdcall thread_init (void *f)
{
   struct thparms *thp = (struct thparms*)f;
   void (*fp)(void *) = thp->f;
   void *arg = thp->arg;

   xfree (f);
   fp (arg);
   return 0;
}

void uae_end_thread (uae_thread_id *tid)
{
   if (tid) {
      CloseHandle (*tid);
      *tid = NULL;
   }
}

#ifdef _XBOX

/* Reparto de hardware threads en Xenon.
 *
 * La consola tiene 3 nucleos fisicos con 2 hardware threads cada uno:
 *    core 0 -> HW 0 y 1     core 1 -> HW 2 y 3     core 2 -> HW 4 y 5
 *
 * El hilo principal (68k + chipset, que van acoplados ciclo a ciclo) se
 * queda donde lo arranca el XDK, o sea el HW 0. Un hilo creado con
 * _beginthreadex HEREDA el hardware thread del padre, asi que sin esto
 * todos caian en el 0 compitiendo con la emulacion -- y encima a
 * THREAD_PRIORITY_HIGHEST, que es por encima de ella. Eso no daba
 * paralelismo, daba tirones.
 *
 * OJO: XSetThreadProcessor hay que llamarlo ANTES de que el hilo empiece a
 * correr. Si se llama despues, el hilo ya ha arrancado en el core del padre
 * y solo migra en el siguiente quantum. De ahi el CREATE_SUSPENDED de
 * _beginthreadex y el ResumeThread posterior.
 */
#define UAE_HWTHREAD_FAST 2 /* core 1: la emulacion se bloquea esperandolos */
#define UAE_HWTHREAD_IO   4 /* core 2: almacenamiento, rafagas largas      */
#define UAE_HWTHREAD_BG   5 /* core 2: fondo (red, serie, impresora)       */

static const struct
{
   const TCHAR *name;
   DWORD hw;
} uae_hwthread_map[] =
{
   /* El 68k espera a este de forma sincrona en cada acceso al chipset. */
   { _T("cpu"),               UAE_HWTHREAD_FAST },
   /* Almacenamiento: el Amiga los espera, pero no ciclo a ciclo. */
   { _T("filesys"),           UAE_HWTHREAD_IO },
   { _T("hardfile"),          UAE_HWTHREAD_IO },
   { _T("scsi"),              UAE_HWTHREAD_IO },
   { _T("uaescsi"),           UAE_HWTHREAD_IO },
   { _T("ide"),               UAE_HWTHREAD_IO },
   { _T("akiko"),             UAE_HWTHREAD_IO },
   { _T("cdtv"),              UAE_HWTHREAD_IO },
   { _T("cdtv-cr"),           UAE_HWTHREAD_IO },
   { _T("cdimage_cdda_play"), UAE_HWTHREAD_IO },
   { _T("cdimage_unpack"),    UAE_HWTHREAD_IO },
   { NULL, 0 }
};

/* Lo que no este en la tabla se va al de fondo: si no lo hemos mirado, por
 * definicion no sabemos que este en el camino caliente. */
static DWORD uae_hwthread_for (const TCHAR *name)
{
   int i;

   if (!name)
      return UAE_HWTHREAD_BG;
   for (i = 0; uae_hwthread_map[i].name; i++) {
      if (!_tcscmp (name, uae_hwthread_map[i].name))
         return uae_hwthread_map[i].hw;
   }
   return UAE_HWTHREAD_BG;
}

#endif /* _XBOX */

static int uae_start_thread_on (const TCHAR *name, void (*f)(void *), void *arg, uae_thread_id *tid, int fast)
{
   HANDLE hThread;
   int result = 1;
   unsigned foo;
   unsigned initflag;
   struct thparms *thp;
#ifdef _XBOX
   DWORD hw = fast ? (DWORD)UAE_HWTHREAD_FAST : uae_hwthread_for (name);
   initflag = CREATE_SUSPENDED;
#else
   initflag = 0;
   (void)fast;
#endif

   thp = xmalloc (struct thparms, 1);
   thp->f = f;
   thp->arg = arg;
   hThread = (HANDLE)_beginthreadex (NULL, 0, thread_init, thp, initflag, &foo);
   if (hThread) {
      if (name) {
         /* write_log (_T("Thread '%s' started (%d)\n"), name, hThread); */
            SetThreadPriority (hThread, THREAD_PRIORITY_HIGHEST);
      }
#ifdef _XBOX
      /* Si fallara devuelve (DWORD)-1 y el hilo se queda donde estaba, que
       * es exactamente el comportamiento de antes: no hay que abortar. */
      if (XSetThreadProcessor (hThread, hw) == (DWORD)-1)
         write_log (_T("Thread '%s': XSetThreadProcessor(%d) fallo\n"),
            name ? name : _T("<fast>"), (int)hw);
      else
         write_log (_T("Thread '%s' -> HW thread %d\n"),
            name ? name : _T("<fast>"), (int)hw);
      ResumeThread (hThread);
#endif
   } else {
      result = 0;
      write_log (_T("Thread '%s' failed to start!?\n"), name ? name : _T("<unknown>"));
   }
   if (tid)
      *tid = hThread;
   else
      CloseHandle (hThread);
   return result;
}

int uae_start_thread (const TCHAR *name, void (*f)(void *), void *arg, uae_thread_id *tid)
{
   return uae_start_thread_on (name, f, arg, tid, 0);
}

int uae_start_thread_fast (void (*f)(void *), void *arg, uae_thread_id *tid)
{
   /* Los arranca traps.c y el 68k se queda bloqueado esperandolos, asi que
    * van al core 1 aunque lleguen aqui sin nombre. */
   int v = uae_start_thread_on (NULL, f, arg, tid, 1);
   if (*tid) {
      SetThreadPriority (*tid, THREAD_PRIORITY_HIGHEST);
   }
   return v;
}

DWORD_PTR cpu_affinity = 1, cpu_paffinity = 1;

void uae_set_thread_priority (uae_thread_id *thread, int pri)
{
   if (!SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_HIGHEST))
      SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}

#else /* _WIN32 */

#ifdef WIIU

#include <wiiu_pthread.h>
#include <wiiu/os/semaphore.h>

#else /* WIIU */

#include <pthread.h>
#include <semaphore.h>

#ifdef USE_NAMED_SEMAPHORES
void uae_sem_init (uae_sem_t *sem, int pshared, unsigned int value)
{
    char name[32];
    static int semno = 0;
    int result = 0;

    sprintf (name, "/uaesem-%d-%d", getpid (), semno++);

    if ((sem->sem = sem_open (name, O_CREAT, 0600, value)) != (sem_t *)SEM_FAILED)
       sem_unlink (name);
    else
       sem->sem = 0;
}
#else
void uae_sem_init (uae_sem_t *sem, int pshared, unsigned int value)
{
    if (!sem || (sem && sem->sem))
        return;
    sem->sem = (sem_t*)calloc(1, sizeof(sem_t));
    sem_init (sem->sem, pshared, value);
}
#endif /* USE_NAMED_SEMAPHORES */

#endif
#endif
