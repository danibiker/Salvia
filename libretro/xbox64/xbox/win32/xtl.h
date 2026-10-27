/*
 * xbox/win32/xtl.h  --  la cabecera raiz del XDK, en Windows.
 *
 * [Windows, 2026-10-26] Solo la ve la build Win32: su carpeta va la PRIMERA en
 * el include path de esa configuracion (xbox64.vcxproj), asi que los ficheros
 * que hacen `#include <xtl.h>` aterrizan aqui sin tocarlos.  En Xbox 360 esta
 * carpeta no esta en el include path y entra la del XDK.
 *
 * Trae lo que en la consola trae <xtl.h> y el arbol usa: Win32 y Direct3D 9.
 * Y lo que el XDK tiene de mas y el arbol nombra:
 *
 *   - `_XTL_`: varias cabeceras del arbol (source/winlnxdefs.h,
 *     rsp_hle/wintypes.h) redeclaran tipos de Windows detras de
 *     `#ifndef _XTL_`.  Aqui los da <windows.h>, como en la consola los da
 *     el XDK: mismo caso, misma guarda.
 *   - Los nombres de los tipos de Direct3D de la Xbox (`D3DTexture`,
 *     `D3DSurface`...), que en PC son las interfaces `IDirect3D*9`.
 *   - Las extensiones de la Xbox que el shim nombra (`D3DPT_QUADLIST`,
 *     `D3DRS_VIEWPORTENABLE`, `D3DRS_HIZ*`, `D3DFMT_LIN_*`, `D3DRESOLVE_*` y
 *     sus structs), con LOS MISMOS VALORES que en el XDK
 *     (C:\develop\proyectos\personal\xbox\d3d9types.h).  Ninguna existe en
 *     el D3D9 de PC: el consumidor de la cola (xenon/d3dq.cpp) las traduce o
 *     se las salta en Windows.
 *   - XSetThreadProcessor / XPhysicalAlloc / XPhysicalFree / XMemCpy.
 *
 * ★ VALIDA EN C Y EN C++: la fuerza xbox64_pre.h en todas las unidades.
 */

#ifndef _XBOX64_WIN32_XTL_H_
#define _XBOX64_WIN32_XTL_H_

#if defined(_XBOX)
#error "xbox/win32/xtl.h es solo para la build de Windows"
#endif

#include <windows.h>
/* [logs (22)] Si el <windows.h> que entra no es el del SDK, lo siguiente son
 * cientos de C2061 en rpcasync.h ('HANDLE', 'HWND') y ninguno apunta aqui.
 * El del SDK define `_WINDOWS_`.  El sospechoso es xenon\gxshim\windows.h, el
 * reenvio a <xtl.h> de la Xbox: se movio a xenon\gxshim\xbox\ (solo en el
 * include path de Xbox) y una copia vieja en su sitio de antes lo tapa. */
#ifndef _WINDOWS_
#error "xbox/win32/xtl.h: <windows.h> no es el del SDK. Borra xenon/gxshim/windows.h (ahora vive en xenon/gxshim/xbox/)"
#endif
#include <d3d9.h>
#include <string.h>
#include <malloc.h>   /* _aligned_malloc: el memalign de gxshim/gccore.h */

#ifndef _XTL_
#define _XTL_ 1
#endif

/* ------------------------------------------------------------------ */
/* Los nombres de los tipos de Direct3D de la Xbox 360                 */
/* ------------------------------------------------------------------ */
typedef IDirect3DDevice9             D3DDevice;
typedef IDirect3DResource9           D3DResource;
typedef IDirect3DBaseTexture9        D3DBaseTexture;
typedef IDirect3DTexture9            D3DTexture;
typedef IDirect3DSurface9            D3DSurface;
typedef IDirect3DVertexBuffer9       D3DVertexBuffer;
typedef IDirect3DIndexBuffer9        D3DIndexBuffer;
typedef IDirect3DVertexDeclaration9  D3DVertexDeclaration;
typedef IDirect3DVertexShader9       D3DVertexShader;
typedef IDirect3DPixelShader9        D3DPixelShader;
typedef IDirect3DQuery9              D3DQuery;

/* Los metodos del device que en la Xbox devuelven void.  El XDK los declara
 * con estas dos macros, y con su rama de PC (d3d9.h:54-59 del XDK) son estas. */
#ifndef D3DVOID
#define D3DVOID        HRESULT
#define D3DVOIDRETURN  return S_OK
#endif

#ifndef D3DPOINT_DEFINED
typedef struct _D3DPOINT
{
	LONG x;
	LONG y;
} D3DPOINT;
#define D3DPOINT_DEFINED
#endif

/* ------------------------------------------------------------------ */
/* Extensiones de la Xbox 360, con los valores del XDK                 */
/* ------------------------------------------------------------------ */
#define D3DPT_RECTLIST         ((D3DPRIMITIVETYPE)8)
#define D3DPT_QUADLIST         ((D3DPRIMITIVETYPE)13)

#define D3DRS_VIEWPORTENABLE   ((D3DRENDERSTATETYPE)304)
#define D3DRS_HIZENABLE        ((D3DRENDERSTATETYPE)384)
#define D3DRS_HIZWRITEENABLE   ((D3DRENDERSTATETYPE)388)

/* El primer render state que el D3D9 de PC no tiene (D3DRS_BLENDOPALPHA =
 * 209 es el ultimo).  El consumidor no le pasa al device nada de aqui en
 * adelante. */
#define XBOX64_D3DRS_SOLO_XBOX 300

typedef enum _D3DHIZENABLEMODE
{
	D3DHIZ_DISABLE     = 0,
	D3DHIZ_ENABLE      = 1,
	D3DHIZ_AUTOMATIC   = 2,
	D3DHIZ_FORCE_DWORD = 0x7fffffff
} D3DHIZENABLEMODE;

typedef enum _D3DHIZFUNC
{
	D3DHIZFUNC_DEFAULT     = 0,
	D3DHIZFUNC_FORCE_DWORD = 0x7fffffff
} D3DHIZFUNC;

/* En PC no hay formatos tileados: el "lineal" es el normal. */
#define D3DFMT_LIN_A8R8G8B8    D3DFMT_A8R8G8B8
#define D3DFMT_LIN_X8R8G8B8    D3DFMT_X8R8G8B8
#define D3DFMT_LIN_R5G6B5      D3DFMT_R5G6B5
#define D3DFMT_LIN_A4R4G4B4    D3DFMT_A4R4G4B4
#define D3DFMT_LIN_A1R5G5B5    D3DFMT_A1R5G5B5
#define D3DFMT_LIN_L8          D3DFMT_L8
#define D3DFMT_LIN_A8          D3DFMT_A8
#define D3DFMT_LIN_A8L8        D3DFMT_A8L8

#define D3DRESOLVE_RENDERTARGET0        0x0000
#define D3DRESOLVE_RENDERTARGET1        0x0001
#define D3DRESOLVE_RENDERTARGET2        0x0002
#define D3DRESOLVE_RENDERTARGET3        0x0003
#define D3DRESOLVE_DEPTHSTENCIL         0x0004
#define D3DRESOLVE_ALLFRAGMENTS         0x0000
#define D3DRESOLVE_FRAGMENT0            0x0010
#define D3DRESOLVE_CLEARRENDERTARGET    0x0100
#define D3DRESOLVE_CLEARDEPTHSTENCIL    0x0200

typedef struct _D3DSURFACE_PARAMETERS
{
	DWORD      Base;
	DWORD      HierarchicalZBase;
	INT        ColorExpBias;
	D3DHIZFUNC HiZFunc;
} D3DSURFACE_PARAMETERS;

typedef struct _D3DRESOLVE_PARAMETERS
{
	D3DFORMAT ColorFormat;
	INT       ColorExpBias;
	D3DFORMAT DepthFormat;
} D3DRESOLVE_PARAMETERS;

typedef struct _D3DVECTOR4
{
	float x, y, z, w;
} D3DVECTOR4;

/* ------------------------------------------------------------------ */
/* Las funciones de sistema de la Xbox 360 que usa el arbol            */
/* ------------------------------------------------------------------ */

/* Hilo -> uno de los seis hilos hardware de Xenon.  En PC el planificador
 * de Windows decide; no se fija nada. */
static __inline DWORD XSetThreadProcessor(HANDLE hThread, DWORD dwHardwareThread)
{
	(void)hThread;
	(void)dwHardwareThread;
	return 0;
}

/* Memoria fisica contigua (texturas propias del shim).  En PC basta con
 * memoria virtual normal: la GPU no la lee directamente. */
static __inline LPVOID XPhysicalAlloc(SIZE_T dwSize, ULONG_PTR ulPhysicalAddress,
                                      ULONG_PTR ulAlignment, DWORD flProtect)
{
	(void)ulPhysicalAddress;
	(void)ulAlignment;
	(void)flProtect;
	return VirtualAlloc( NULL, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
}

static __inline VOID XPhysicalFree(LPVOID lpAddress)
{
	VirtualFree( lpAddress, 0, MEM_RELEASE );
}

/* La copia rapida del XDK (xbox.h): la foto de la RDRAM (xbox/hilo_lista.cpp). */
static __inline void *XMemCpy(void *dest, const void *src, size_t count)
{
	return memcpy( dest, src, count );
}

#endif /* _XBOX64_WIN32_XTL_H_ */
