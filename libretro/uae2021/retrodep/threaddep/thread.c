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

void uae_sem_post (uae_sem_t * event)
{
   SetEvent (*event);
}

int uae_sem_trywait (uae_sem_t * event)
{
   return WaitForSingleObject (*event, 0) == WAIT_OBJECT_0 ? 0 : -1;
}

void uae_sem_destroy (uae_sem_t * event)
{
   if (*event) {
      CloseHandle (*event);
      *event = NULL;
   }
}

typedef unsigned (__stdcall *BEGINTHREADEX_FUNCPTR)(void *);

struct thparms
{
   void *(*f)(void*);
   void *arg;
};

static unsigned __stdcall thread_init (void *f)
{
   struct thparms *thp = (struct thparms*)f;
   void *(*fp)(void*) = thp->f;
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
 * 3 nucleos fisicos con 2 hardware threads cada uno:
 *    core 0 -> HW 0 y 1     core 1 -> HW 2 y 3     core 2 -> HW 4 y 5
 *
 * Un hilo creado con _beginthreadex HEREDA el hardware thread del padre, asi
 * que sin esto todos caen junto al bucle de emulacion -- y encima a
 * THREAD_PRIORITY_HIGHEST, prioridad por encima de ella. Eso no da
 * paralelismo, da tirones.
 *
 * OJO: XSetThreadProcessor hay que llamarlo ANTES de que el hilo empiece a
 * correr. Si se llama despues, el hilo ya arranco en el core del padre y solo
 * migra en el siguiente quantum. De ahi el CREATE_SUSPENDED y el ResumeThread.
 *
 * Mapa REAL del frontend, copiado de Salvia/src/const/constant.h (no de memoria):
 *   HW 0 -> dashboard de la 360               \ mismo core fisico
 *   HW 1 -> main / Salvia / retro_run         / (CPU_THREAD). Aqui va el 68k.
 *   HW 2 -> hilo auxiliar de la GPU de PSX    \ mismo core fisico. Libre
 *   HW 3 -> ocioso                            / corriendo el core de Amiga.
 *   HW 4 -> IO & HTTP  (IO_THREAD)            \ mismo core fisico
 *   HW 5 -> XAudio + hilo de sonido de SDL    / NO USAR NI COMPARTIR.
 *
 * ★ NO PONER NADA EN EL HW 4. Es donde vive el IO_THREAD de Salvia, o sea el
 * hilo que SIRVE las lecturas de fichero por las que nuestros hilos esperan.
 * Compartir hardware thread con el significa que la E/S solo avanza cuando el
 * que la pidio cede el turno: serializacion autoinfligida, y el sintoma es
 * justo "el threading no hace nada".
 *
 * OJO con el HW 3: Salvia/src/io/video.h:872 lanza los 3 hilos del escalador
 * xBRZ a los HW 1, 3 y 5 y hace WaitForMultipleObjects(INFINITE). Con ese
 * filtro activo el HW 3 tiene rafagas. Si molesta, poner CDDA tambien en el 2.
 */
/* ★ EL 68k NO SE COLOCA DESDE AQUI. Corre dentro de retro_run(), es decir en el
 * hilo principal de Salvia, y Salvia lo clava en HW1
 * (XSetThreadProcessor(currentThread, CPU_THREAD) con CPU_THREAD=1, en
 * engine.cpp:48). Ninguna linea de esta tabla lo mueve.
 *
 * UAE_HWTHREAD_CPU y la fila "cpu" de la tabla son un hueco RESERVADO que hoy
 * es INALCANZABLE, comprobado: no hay ni un llamante de uae_start_thread_fast()
 * (solo su definicion aqui y su declaracion en thread.h) y no existe ningun
 * hilo llamado "cpu". Por eso comparte valor con IO sin que eso sea una
 * colision con el 68k.
 *
 * Si algun dia UAE lanza la CPU en su propio hilo, hay que REHACER el reparto,
 * no solo esta linea: con HW2 y HW3 ocupados no queda ninguno libre (HW0 kernel,
 * HW1 Salvia, HW4 hermano SMT del audio, HW5 audio). Lo natural seria juntar
 * almacenamiento y CDDA en el HW3 y dejarle el HW2 entero a la CPU. */
#define UAE_HWTHREAD_CPU  2 /* hueco reservado, HOY SIN USO (ver arriba)     */
#define UAE_HWTHREAD_IO   2 /* almacenamiento: a rafagas y bloqueante        */
#define UAE_HWTHREAD_CDDA 3 /* audio de CD: tiene que despertar A TIEMPO, asi
                             * que no comparte hardware thread con las
                             * lecturas de 64 sectores del prefetch de akiko  */
#define UAE_HWTHREAD_BG   2 /* trabajo que nadie espera en caliente          */

static const struct
{
   const TCHAR *name;
   DWORD hw;
} uae_hwthread_map[] =
{
   /* El 68k espera a este de forma sincrona. */
   { _T("cpu"),               UAE_HWTHREAD_CPU },
   /* Almacenamiento: el Amiga los espera, pero no ciclo a ciclo. */
   { _T("filesys"),           UAE_HWTHREAD_IO },
   { _T("hardfile"),          UAE_HWTHREAD_IO },
   { _T("scsi"),              UAE_HWTHREAD_IO },
   { _T("uaescsi"),           UAE_HWTHREAD_IO },
   { _T("ide"),               UAE_HWTHREAD_IO },
   { _T("akiko"),             UAE_HWTHREAD_IO },
   { _T("cdtv"),              UAE_HWTHREAD_IO },
   { _T("cdimage_cdda_play"), UAE_HWTHREAD_CDDA },
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

static int uae_start_thread_on (const TCHAR *name, void *(*f)(void *), void *arg, uae_thread_id *tid, int fast)
{
   HANDLE hThread;
   int result = 1;
   unsigned foo;
   unsigned initflag;
   struct thparms *thp;
#ifdef _XBOX
   DWORD hw = fast ? (DWORD)UAE_HWTHREAD_CPU : uae_hwthread_for (name);
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
      /* Si falla devuelve (DWORD)-1 y el hilo se queda donde estaba, que es
       * exactamente el comportamiento anterior: no hay que abortar. */
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

int uae_start_thread (const TCHAR *name, void *(*f)(void *), void *arg, uae_thread_id *tid)
{
   return uae_start_thread_on (name, f, arg, tid, 0);
}

int uae_start_thread_fast (void *(*f)(void *), void *arg, uae_thread_id *tid)
{
   /* Los arranca traps.c y el 68k se queda bloqueado esperandolos, asi que van
    * al core 1 aunque lleguen aqui sin nombre. */
   int v = uae_start_thread_on (NULL, f, arg, tid, 1);
   if (*tid) {
      SetThreadPriority (*tid, THREAD_PRIORITY_HIGHEST);
   }
   return v;
}

DWORD_PTR cpu_affinity = 1, cpu_paffinity = 1;

void uae_set_thread_priority (int pri)
{
#ifndef __LIBRETRO__
   if (!SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_HIGHEST))
      SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif
}

#else /* _WIN32 */

#ifdef WIIU

#include <wiiu_pthread.h>
#include <wiiu/os/semaphore.h>

#else /* WIIU */

#include <pthread.h>
#include <semaphore.h>

#ifdef USE_NAMED_SEMAPHORES
int uae_sem_init (uae_sem_t *sem, int pshared, unsigned int value)
{
    char name[32];
    static int semno = 0;
    int result = 0;

    sprintf (name, "/uaesem-%d-%d", getpid (), semno++);

    if ((sem->sem = sem_open (name, O_CREAT, 0600, value)) != (sem_t *)SEM_FAILED)
		sem_unlink (name);
    else {
		sem->sem = 0;
		result = -1;
    }
    return result;
}
#else
int uae_sem_init (uae_sem_t *sem, int pshared, unsigned int value)
{
	if (!sem || (sem && sem->sem))
		return -1;
	sem->sem = (sem_t*)calloc(1, sizeof(sem_t));
	return sem_init (sem->sem, pshared, value);
}
#endif /* USE_NAMED_SEMAPHORES */

#endif
#endif
