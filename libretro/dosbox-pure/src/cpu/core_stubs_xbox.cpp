/*
 *  Xbox 360 build: stubs para los cores que se excluyen del build por
 *  cuestiones de tamano o por errores de compilacion bajo XDK.  El
 *  fichero original es:
 *    - core_prefetch.cpp  (emulacion de prefetch queue 286/386/486)
 *
 *  core_full.cpp YA NO se sustituye: PageFaultCore (paging.cpp) e
 *  IOFaultCore (iohandler.cpp) lo usan para ejecutar instrucciones
 *  anidadas mientras se resuelve un page fault / IO fault, y el core
 *  normal no sirve como sustituto ahi (probablemente porque guarda su
 *  estado de decodificacion en variables estaticas y no es reentrante).
 *  Verificado con una build PPC big-endian en qemu: con el core normal
 *  en PageFaultCore/IOFaultCore la misma imagen de Win95 da el 0E;
 *  con core_full arranca bien.  Con el dynrec, Windows 95 terminaba en
 *  "fatal exception 0E" al arrancar.
 *
 *  Estos cores son referenciados desde:
 *    - cpu.cpp           (seleccion de cpudecoder por config)
 *    - paging.cpp:275    (re-entrada para resolver page faults)
 *    - iohandler.cpp:177 (re-entrada para MMIO)
 *    - core_dynrec.cpp:327 (rama de fallback)
 *
 *  Sin los .cpp originales el linker no encuentra esos simbolos.  Este
 *  fichero los provee redirigiendo todos al core normal (mas rapido que
 *  ambos y siempre presente).  Si el usuario selecciona "full" o
 *  "prefetch" / "386_prefetch" / "486_prefetch" desde las core options,
 *  obtiene el core normal en su lugar (no es un error, solo una
 *  degradacion silenciosa al fallback mas razonable disponible).
 *
 *  Este fichero SOLO debe compilarse en builds Xbox 360 donde
 *  core_prefetch.cpp este excluido del proyecto.
 */

#if defined(_XBOX)

#include "dosbox.h"
#include "cpu.h"

/* Forward declaration del core que SI esta presente en el build. */
extern Bits CPU_Core_Normal_Run(void);

/* ---------- Stubs de core_prefetch.cpp ---------- */

Bits CPU_Core_Prefetch_Run(void)
{
	/* Si el usuario selecciono "386_prefetch" / "486_prefetch" / "auto"
	 * con prefetch, se ejecuta esto.  Caemos al core normal, que es
	 * funcionalmente correcto excepto para los pocos juegos que dependen
	 * de los efectos secundarios de la prefetch queue real (algunas
	 * copy-protections de finales de los 80). */
	return CPU_Core_Normal_Run();
}

Bits CPU_Core_Prefetch_Trap_Run(void)
{
	return CPU_Core_Normal_Run();
}

#endif // _XBOX
