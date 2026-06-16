#ifndef __THREAD_H__
#define __THREAD_H__

/* On Windows, retro_miscellaneous.h pulls in <windows.h> which already
 * defines THREAD_PRIORITY_HIGHEST / NORMAL / LOWEST.  Redefining them
 * unconditionally would emit -Wmacro-redefined.  Guard each macro so
 * winbase.h's values win when it ran first.  This is safe: only Vita's
 * _thread_map_priority() switch actually reads these (and Vita SDK
 * headers don't define them); the non-Vita thread_run() ignores the
 * priority argument entirely, and thread_set_priority() is a no-op on
 * everything except Vita, so the literal numeric values are irrelevant
 * off-Vita. */
#ifndef THREAD_PRIORITY_HIGHEST
#define THREAD_PRIORITY_HIGHEST 	1
#endif
#ifndef THREAD_PRIORITY_HIGH
#define THREAD_PRIORITY_HIGH 		2
#endif
#ifndef THREAD_PRIORITY_NORMAL
#define THREAD_PRIORITY_NORMAL 		3
#endif
#ifndef THREAD_PRIORITY_LOW
#define THREAD_PRIORITY_LOW 		4
#endif
#ifndef THREAD_PRIORITY_LOWEST
#define THREAD_PRIORITY_LOWEST 		5
#endif

#include <stdint.h>

#if VITA
#include <psp2/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if VITA
typedef SceUID thread_t;
#else
typedef void* thread_t;
#endif

#ifdef THREADED_RENDERER
typedef void(*threadfunc_t)(void*);

thread_t thread_get();
thread_t thread_run(threadfunc_t func, void* p, int priority);
void thread_sleep(int ms);
void thread_set_priority(thread_t id, int priority);
#endif

#ifdef __cplusplus
}
#endif

#endif

