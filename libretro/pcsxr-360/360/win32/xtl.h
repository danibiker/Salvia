/*
 * 360/win32/xtl.h  --  la cabecera raiz del XDK, en Windows.
 *
 * [Windows, 2026-10-30] Solo la ve la build Win32: su carpeta va la PRIMERA en
 * el include path de las configuraciones Win32 de los seis proyectos de
 * pcsxr-360 (core, video, audio, input, cdrom, pcsxr), asi que los ficheros
 * que hacen `#include <xtl.h>` aterrizan aqui sin tocarlos.  En Xbox 360 esta
 * carpeta no esta en el include path y entra la del XDK.
 *
 * Mismo patron que libretro/xbox64/xbox/win32 (port Win32 de xbox64).
 *
 * Trae lo que en la consola trae <xtl.h> y el arbol usa: Win32, XInput y los
 * intrinsecos de PowerPC (ver ppcintrinsics.h de esta carpeta).  Y lo que el
 * XDK tiene de mas y el arbol nombra:
 *
 *   XSetThreadProcessor   fija un hilo a uno de los 6 hilos hardware del
 *                         Xenon.  En PC el planificador del sistema lo hace
 *                         mejor que un numero fijo pensado para la Xbox:
 *                         no-op.
 *   XPhysicalAlloc/Free   memoria fisica contigua de la Xbox.  En PC basta
 *                         VirtualAlloc (alineada a pagina, a cero).  Se
 *                         ignoran el tope de direccion, la alineacion pedida
 *                         (VirtualAlloc ya da 64 KB) y los flags de
 *                         proteccion: MEM_LARGE_PAGES pediria un privilegio
 *                         que el usuario normal no tiene.
 *   XMemSet / XMemCpy     memset / memcpy.
 *
 * ★ No define __loadshortbytereverse ni familia A PROPOSITO: en este arbol
 *   se usan para leer la VRAM de la PSX (little-endian) desde un anfitrion
 *   big-endian.  En x86 lo correcto es una lectura directa, no un volteo.
 *   Si un uso se queda sin su `#if defined(_XBOX)`, mejor un error de
 *   compilacion que colores cambiados en pantalla.
 *
 * ★ VALIDA EN C Y EN C++.
 */

#ifndef _PCSXR360_WIN32_XTL_H_
#define _PCSXR360_WIN32_XTL_H_

#if defined(_XBOX)
#error "360/win32/xtl.h es solo para la build de Windows"
#endif

#include <windows.h>
#ifndef _WINDOWS_
#error "360/win32/xtl.h: <windows.h> no es el del SDK (algo del include path lo tapa)"
#endif
#include <xinput.h>   /* XINPUT_VIBRATION, XInputSetState (input/PSXInput.cpp) */
#include <string.h>
#include <malloc.h>

#include "ppcintrinsics.h"

/* XInput: la importa el core (vibracion), el frontend no la enlaza. */
#if defined(_MSC_VER)
#pragma comment(lib, "xinput.lib")
#endif

#ifndef _XTL_
#define _XTL_ 1
#endif

/* [Windows, 2026-10-30] CALLBACK vacio, como en la Xbox en la practica: en
 * PowerPC no hay __stdcall, y el arbol declara las mismas funciones unas veces
 * con CALLBACK y otras sin (xbPlugins.h con, dfsound/stdafx.h lo vacia,
 * cdriso.c llama a CDR__getStatus sin prototipo).  En x86 eso son
 * convenciones distintas y no enlaza (_PEOPS_SPUinit@0 frente a
 * _PEOPS_SPUinit).  Los typedef de Windows que usan CALLBACK ya se leyeron
 * arriba con su __stdcall y no cambian. */
#undef CALLBACK
#define CALLBACK

#ifdef __cplusplus
extern "C" {
#endif

static __inline DWORD XSetThreadProcessor(HANDLE hThread, DWORD dwHardwareThread)
{
	(void)hThread; (void)dwHardwareThread;
	return 0;
}

static __inline LPVOID XPhysicalAlloc(SIZE_T dwSize, ULONG_PTR ulPhysicalAddress,
                                      ULONG_PTR ulAlignment, DWORD flProtect)
{
	(void)ulPhysicalAddress; (void)ulAlignment; (void)flProtect;
	return VirtualAlloc(NULL, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

static __inline void XPhysicalFree(LPVOID lpAddress)
{
	if (lpAddress)
		VirtualFree(lpAddress, 0, MEM_RELEASE);
}

#ifdef __cplusplus
}
#endif

#define XMemSet(d, c, n)   memset((d), (c), (n))
#define XMemCpy(d, s, n)   memcpy((d), (s), (n))

#endif /* _PCSXR360_WIN32_XTL_H_ */
