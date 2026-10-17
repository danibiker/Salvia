/*
 * ogc/mutex.h  --  los mutex de libogc (LWP), sobre secciones criticas de Win32.
 *
 * Lo pide Rice_GX y no glN64_GX: `Rice_GX/CritSect.h` envuelve `LWP_Mutex*`
 * en una clase `CCritSect` que usa el gestor de texturas.  El fichero de Rice
 * no se toca -- es copia verbatim de Wii64 --, asi que la compatibilidad
 * tiene que salir de aqui.
 *
 * ★★ Y SON MUTEX DE VERDAD, NO UN NO-OP.
 *
 * La tentacion es dejarlos vacios "porque emulamos en un hilo".  Es falso en
 * este arbol: con `gfxh=1` la display list la procesa un HILO APARTE en el
 * nucleo 1 (xenon/gfx_hilo.cpp), y el gestor de texturas de Rice es
 * exactamente el tipo de estructura que ese hilo toca mientras el de
 * emulacion puede estar mirandola.  Un no-op aqui daria corrupcion
 * intermitente y dificil de atribuir, que es la peor clase de fallo.
 *
 * ★ `CRITICAL_SECTION` y no un `HANDLE` de mutex con nombre: es reentrante
 * por el mismo hilo (como LWP) y no cruza el limite del proceso, que es lo
 * unico que hace falta.  Ademas no entra en el nucleo cuando no hay
 * contienda, y aqui casi nunca la hay.
 *
 * Convenio de retorno de libogc: 0 = bien, negativo = error.  Rice no mira el
 * valor en ningun sitio, pero se respeta para que no sorprenda si algun dia
 * lo mira.
 */

#ifndef _XENON_GXSHIM_OGC_MUTEX_H_
#define _XENON_GXSHIM_OGC_MUTEX_H_

#include <xtl.h>
#include <stdlib.h>   /* malloc/free */
#include "../gctypes.h"

typedef CRITICAL_SECTION *mutex_t;

#define LWP_MUTEX_NULL ((mutex_t)0)

#ifdef __cplusplus
extern "C" {
#endif

/* `use_recursive` se ignora: `CRITICAL_SECTION` YA es recursiva, y ese es el
 * caso que Rice usa (`GX_FALSE`, o sea no recursiva).  Dar de mas aqui es
 * seguro; dar de menos seria un interbloqueo.
 *
 * ★ El parametro va como `int` y no como `bool`: esta cabecera cuelga de
 * `gccore.h`, que tambien entra desde ficheros C, y en MSVC < 1800 `bool` no
 * existe en C salvo que alguien lo haya definido (ver
 * [[reference_msvc_bool_truncation]]).  `GX_FALSE` convierte a `int` sin
 * ruido desde los dos lenguajes. */
static __inline s32 LWP_MutexInit(mutex_t *mutex, int use_recursive)
{
    CRITICAL_SECTION *cs;
    (void)use_recursive;
    if (!mutex) return -1;
    cs = (CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
    if (!cs) { *mutex = LWP_MUTEX_NULL; return -1; }
    InitializeCriticalSection(cs);
    *mutex = cs;
    return 0;
}

static __inline s32 LWP_MutexDestroy(mutex_t mutex)
{
    if (!mutex) return -1;
    DeleteCriticalSection(mutex);
    free(mutex);
    return 0;
}

/* ★ Tolerantes con el mutex nulo a proposito.  Si `LWP_MutexInit` fallo, el
 * camino correcto es seguir sin exclusion -- degradado-- y no reventar en un
 * `EnterCriticalSection(NULL)`, que en Xbox 360 es un salto a 0. */
static __inline s32 LWP_MutexLock(mutex_t mutex)
{
    if (!mutex) return -1;
    EnterCriticalSection(mutex);
    return 0;
}

static __inline s32 LWP_MutexUnlock(mutex_t mutex)
{
    if (!mutex) return -1;
    LeaveCriticalSection(mutex);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _XENON_GXSHIM_OGC_MUTEX_H_ */
