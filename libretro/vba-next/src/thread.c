/* Expose POSIX 199309 (struct timespec, nanosleep) needed by the non-vita
 * branch's thread_sleep_ms below.  Must be defined before any header is
 * included -- glibc only honors feature-test macros that are set before its
 * <features.h> first runs. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include <stdint.h>
#include <retro_miscellaneous.h>
#include "thread.h"

#ifdef THREADED_RENDERER

#if VITA
#include <psp2/kernel/threadmgr.h>

static int _thread_func(SceSize args, void* p)
{
   void** argp       = (void**)(p);
   threadfunc_t func = (threadfunc_t)(argp[0]);
   (*func)(argp[1]);
   return sceKernelExitDeleteThread(0);
}

static int _thread_map_priority(int priority)
{
   switch(priority)
   {
      case THREAD_PRIORITY_LOWEST:
         return 0x10000102;
      case THREAD_PRIORITY_LOW:
         return 0x10000101;
      case THREAD_PRIORITY_NORMAL:
      case THREAD_PRIORITY_HIGH:
      case THREAD_PRIORITY_HIGHEST:
      default:
         break;
   }		
   return 0x10000100;
}

thread_t thread_run(threadfunc_t func, void* p, int priority)
{
   SceUID thid;
   void* argp[2];
   argp[0] = (void*)(func);
   argp[1] = p;

   thid = sceKernelCreateThread("my_thread", (SceKernelThreadEntry)_thread_func, _thread_map_priority(priority), 0x10000, 0, 0, NULL);
   if (thid >= 0)
	   sceKernelStartThread(thid, sizeof(argp), &argp);

   return thid;
}
	
thread_t thread_get(void) { return sceKernelGetThreadId(); }	
void thread_sleep(int ms) { sceKernelDelayThread(ms * 1000); } /* retro_sleep
causes crash */
void thread_set_priority(thread_t id, int priority) { sceKernelChangeThreadPriority(id, 0xFF & _thread_map_priority(priority)); }

#else /* non-vita */

#include <stdlib.h>
#include <rthreads/rthreads.h>

/* Local ms-resolution sleep. The vendored libretro-common subtree in this
 * repo does not ship retro_timers.h, so retro_sleep() is unresolved at link
 * time when USE_THREADED_RENDERER is set. Inline a small replacement here
 * rather than vendoring an extra header just for one call site.  POSIX
 * struct timespec is gated on _POSIX_C_SOURCE which we set at the top of
 * this file. */
#if defined(_WIN32)
#include <windows.h>
#define thread_sleep_ms(ms) Sleep((DWORD)(ms))
#else
#include <time.h>
static INLINE void thread_sleep_ms(int ms)
{
   struct timespec tv;
   tv.tv_sec  =  ms / 1000;
   tv.tv_nsec = (ms % 1000) * 1000000L;
   nanosleep(&tv, NULL);
}
#endif

/* Trampoline arg holds the user function pointer in a properly-typed slot.
 * Earlier this code stuffed the function pointer into a void*[2] and cast
 * back, which is widely accepted but is technically UB in ISO C (function
 * pointers are not convertible to/from data pointers) and trips MSVC
 * warnings C4054 / C4055.  The struct keeps the function pointer typed
 * throughout and is heap-owned so it outlives this stack frame. */
struct _thread_run_args
{
   threadfunc_t func;
   void* user_arg;
};

static void _thread_func(void* p)
{
   struct _thread_run_args *args = (struct _thread_run_args*)p;
   threadfunc_t func             = args->func;
   void*        user_arg         = args->user_arg;
   free(args); /* free the heap-allocated arg pair before the user fn takes over */
   (*func)(user_arg);
}

thread_t thread_run(threadfunc_t func, void* p, int priority)
{
   sthread_t *thid = NULL;
   /* args must outlive this stack frame: the new thread reads it asynchronously.
    * heap-allocate, transfer ownership to _thread_func which frees it. */
   struct _thread_run_args *args =
      (struct _thread_run_args*)malloc(sizeof(*args));
   if (!args)
      return NULL;
   args->func     = func;
   args->user_arg = p;

   thid = sthread_create(_thread_func, args);
   if (!thid) {
      free(args);
      return NULL;
   }
   sthread_detach(thid);

   return thid;	
}

thread_t thread_get(void) { return 0; }
void thread_sleep(int ms) { thread_sleep_ms(ms); }
void thread_set_priority(thread_t id, int priority) { }

#endif

#endif /* THREADED_RENDERER */
