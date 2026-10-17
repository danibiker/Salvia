#ifndef __WINTYPES_H__
#define __WINTYPES_H__

/* [Xbox360] ★★ ESTE FICHERO CHOCABA DE FRENTE CON <xtl.h>.
 *
 * Declaraba a mano los tipos de Windows para plataformas que no los tienen.
 * En Xbox 360 los trae el XDK, y varios NO COINCIDEN:
 *
 *     DWORD    aqui `unsigned int`, en el XDK `unsigned long`   -> C2371
 *     BOOLEAN  aqui `int`,          en el XDK `unsigned char`   -> C2371
 *     HWND     aqui `int`,          en el XDK `struct HWND__*`  -> C2371
 *     RECT     aqui `int`,          en el XDK una struct        -> C2371
 *     byte     aqui typedef,        en el XDK XObjBase.h:22     -> C2371 en C
 *
 * `unsigned int` y `unsigned long` miden los dos 32 bits en esta consola,
 * asi que el choque de DWORD no cambiaria ningun tamanyo -- pero para el
 * compilador son tipos distintos y no deja pasar la redeclaracion.
 *
 * Se envuelven en `#ifndef _XTL_`, que es el mismo patron que usa
 * `source/winlnxdefs.h` en este arbol: si el XDK ya los ha traido, no se
 * tocan.  Fuera de Xbox 360 el fichero se comporta igual que antes.
 *
 * Los `#define` de convenciones de llamada SI se conservan: `CALL` de
 * Rsp_#1.1.h:30 es `_cdecl`, y `EXPORT` es `__declspec(dllexport)`, que en
 * una libreria estatica no queremos que exporte nada.  Neutralizarlos ya no
 * es peligroso porque xbox/xbox64_pre.h mete el XDK entero antes de la
 * primera linea de cada unidad.
 */
#include <sys/types.h>

#ifndef _XTL_
typedef int HWND;
typedef int HINSTANCE;
typedef void* LPVOID;
#endif

#define __declspec(dllexport)
#define __cdecl
#define _cdecl
#ifndef _XTL_
#define WINAPI
#endif

#ifndef _XTL_
typedef unsigned int		DWORD;
typedef unsigned short		WORD;
typedef unsigned char			BYTE, byte;
typedef int			BOOL, BOOLEAN;
#define __int8                  char
#define __int16                 short
#define __int32	                int
#define __int64                 long long
#else
/* [Xbox360] ★★ AQUI DWORD TIENE QUE SER `unsigned int`, Y NO ES UN CAPRICHO.
 *
 * El XDK lo declara `unsigned long`.  Si rsp_hle usa ese, `RSP_INFO` (que
 * esta definida en Rsp_#1.1.h con campos `DWORD*`) pasa a tener punteros a
 * `unsigned long`, y `hle_init` (hle.h:31-49, upstream) espera
 * `unsigned int*` en SUS DIECINUEVE parametros de registro.  El compilador
 * solo se queja del quinto, pero fallan los diecinueve.
 *
 * ★ Y ASI ES COMO ESTA EN LA REFERENCIA.  En Wii64, `main/winlnxdefs.h:35`
 * declara `DWORD` como `unsigned long` y `rsp_hle/wintypes.h:15` como
 * `unsigned int`: LOS DOS LADOS YA DIVERGEN, y funciona porque en PPC de 32
 * bits los dos miden lo mismo.  `RSP_INFO` se pasa por valor entre un lado
 * y otro y el reparto binario coincide.  Reproducirlo es lo fiel;
 * unificarlo seria el cambio.
 *
 * Tiene que ser una MACRO y no un typedef, porque el typedef del XDK ya
 * existe y redeclararlo con otro tipo base es C2371.  Y es seguro que sea
 * macro por lo mismo de siempre: xbox/xbox64_pre.h ya metio el XDK entero
 * antes de la primera linea, asi que esto no puede expandirse dentro de
 * ninguna cabecera de Microsoft.
 *
 * El alcance queda confinado: `wintypes.h` solo lo incluyen ficheros de
 * rsp_hle, y del build solo plugin.c.
 *
 * Los demas tipos del bloque de arriba SI coinciden con los del XDK
 * (WORD, BYTE, BOOL) o no se usan en rsp_hle (BOOLEAN), asi que basta con
 * dejarlos al XDK. */
#define DWORD unsigned int
#endif

/** HRESULT stuff **/
#ifndef _XTL_
typedef long				HRESULT;
#define S_OK				((HRESULT)0L)
#define E_NOTIMPL		0x80004001L
#endif

#ifndef FALSE
# define FALSE (0)
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

#ifndef _XTL_
typedef int HMENU;
typedef int RECT;
#endif

/* [Xbox360] ★ PAINTSTRUCT SE QUEDA FUERA DE LA GUARDA.
 *
 * Comprobado uno a uno cual de estos tipos trae de verdad el XDK: HWND
 * (windef.h:195), HINSTANCE (:261), HMENU (:258) y RECT (:300) SI; y
 * PAINTSTRUCT NO -- es de GDI/user32, que Xbox 360 no tiene.  Envolverlo
 * con los demas lo dejo sin declarar y `Rsp_#1.1.h:75` dio
 * `C2061: syntax error : identifier 'PAINTSTRUCT'`.
 *
 * Lo usa una sola linea, el puntero a funcion `PaintBPPanel` de la
 * interfaz de depuracion de zilmar, que nadie implementa. */
typedef int PAINTSTRUCT;

#endif /* __WINTYPES_H__ */
