/*
 * gxshim/sys/time.h  --  gettimeofday para Rice_GX.
 *
 * ★★ ESTE SI SE EJECUTA, Y ES LA DIFERENCIA CON dirent.h.
 *
 * `Rice_GX/RSP_Parser.cpp` lo llama en DOS sitios y los dos estan a
 * profundidad cero de `#if`, o sea fuera de cualquier guarda de __GX__:
 *
 *     RSP_Parser.cpp:977-979   DLParser_Process      (una vez por display
 *                                                     list)
 *     RSP_Parser.cpp:1797-1799 RDP_DLParser_Process
 *
 * Las dos hacen lo mismo: `status.gRDPTime = tv.tv_usec`.  O sea que de todo
 * <sys/time.h> se usa UN campo, y como marca de tiempo relativa.
 *
 * ★ POR ESO NO SE DEVUELVE LA HORA DEL DIA, Y ES DELIBERADO.  `tv_sec` sale
 * del mismo contador monotono que `tv_usec`, no del reloj de pared: lo que
 * Rice necesita es que dos lecturas seguidas se puedan restar, y un reloj de
 * pared que el sistema ajuste hacia atras daria diferencias negativas.  Si
 * algun dia alguien necesita la fecha de verdad aqui, esto NO le sirve.
 *
 * ★ Y `tv_usec` tiene que ser MICROSEGUNDOS DENTRO DEL SEGUNDO (0..999999),
 * no microsegundos desde el arranque.  Es lo que dice POSIX y es lo que hace
 * que `gRDPTime` quepa en su campo.  Con el valor absoluto, el `long`
 * desbordaria en 36 minutos.
 *
 * ★ LA IMPLEMENTACION VA EN xbox/compat_posix.c, no aqui como `static
 * __inline`: comparte el `QueryPerformanceFrequency` cacheado con
 * `gettick()` y `ticks_to_microsecs()`, que ya viven alli.  Dos copias del
 * cacheo serian dos verdades sobre la misma frecuencia.
 *
 * ★ NOTA PARA LA FASE DE MEDIDAS.  `r4300/profile.c` se dejo FUERA del
 * proyecto justamente porque incluia <sys/time.h> en su linea 31, fuera de
 * la guarda de PROFILE (ver la cabecera de xbox64.vcxproj).  Con este
 * fichero ese motivo ya no existe; siguen en pie los otros (PROFILE apagado
 * y que no aporta nada al build actual).
 */

#ifndef _XBOX64_GXSHIM_SYS_TIME_H_
#define _XBOX64_GXSHIM_SYS_TIME_H_

#include <xtl.h>

/* ★★ AQUI NO SE DECLARA `struct timeval`, Y ESA AUSENCIA ES EL ARREGLO.
 *
 * La trae el XDK, en `winsockx.h:197`, y `<xtl.h>` la arrastra siempre.  La
 * primera version la declaraba aqui detras de `#ifndef _TIMEVAL_DEFINED`
 * -- el guarda que usa el <winsock.h> de escritorio-- y el XDK NO usa ese
 * guarda, asi que salieron las dos:
 *
 *     gxshim\sys/time.h(52): error C2011: 'timeval' : 'struct' type
 *         redefinition
 *       winsockx.h(197) : see declaration of 'timeval'
 *
 * ★ Y EL SINTOMA INTERESANTE NO FUE ESE, sino el de despues: tras el C2011
 * el compilador deja el tag INUTILIZABLE, y RSP_Parser.cpp salio con
 *
 *     (977): error C2079: 'tv' uses undefined struct 'timeval'
 *     (978): cannot convert parameter 1 from 'int *' to 'timeval *'
 *
 * o sea "no existe" justo despues de decir "ya existe".  Son el mismo fallo
 * visto dos veces, no dos fallos; buscar por que `timeval` estaba a la vez
 * definida y sin definir es perder una tarde. */

#ifdef __cplusplus
extern "C" {
#endif

/* El segundo argumento es la zona horaria de POSIX.  Rice pasa 0 en los dos
 * sitios; se acepta y se ignora, que es lo que hacen todas las libc
 * modernas.  `void*` y no `struct timezone*` para no declarar un tipo que
 * nadie usa. */
int gettimeofday(struct timeval *tv, void *tz);

#ifdef __cplusplus
}
#endif

#endif /* _XBOX64_GXSHIM_SYS_TIME_H_ */
