/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id: SDL_xboxvideo.c,v 1.1 2003/07/18 15:19:33 lantus Exp $";
#endif

/* XBOX SDL video driver implementation; this is just enough to make an
 *  SDL-based application THINK it's got a working video driver, for
 *  applications that call SDL_Init(SDL_INIT_VIDEO) when they don't need it,
 *  and also for use as a collection of stubs when porting SDL to a new
 *  platform for which you haven't yet written a valid video driver.
 *
 * This is also a great way to determine bottlenecks: if you think that SDL
 *  is a performance problem for a given platform, enable this driver, and
 *  then see if your application runs faster without video overhead.
 *
 * Initial work by Ryan C. Gordon (icculus@linuxgames.com). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "XBOX" by Sam Lantinga.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "SDL.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_pixels_c.h"
#include "SDL_events_c.h"

#include "SDL_xboxvideo.h"
#include "SDL_xboxevents_c.h"
#include "SDL_xboxmouse_c.h"
#include "../SDL_yuvfuncs.h"
#include "SDL_xboxshaders.h"
#include "../../../../../src/video/HLSLBackground.h"
/* Tabla de shaders publicada por la capa de aplicacion (assets\shaders).
   Ver src/video/salvia_shader_api.h para el reparto de responsabilidades. */
#include "../../../../../src/video/salvia_shader_api.h"

#define XBOXVID_DRIVER_NAME "XBOX"

/* ─── HLSLBackground: Xbox 360 C implementation ───────────────── */
static unsigned long XBOX_HashShaderSource(const char* str, DWORD flags);
static DWORD* XBOX_LoadCachedShader(unsigned long hash, DWORD* outSize);
static void XBOX_SaveCachedShader(unsigned long hash, const void* bytecode, DWORD size);

static LPDIRECT3DPIXELSHADER9  g_hlslBg_ps[HLSL_BG_COUNT];
static LPDIRECT3DPIXELSHADER9  g_hlslBg_psAlphaFix = NULL;
static LPDIRECT3DVERTEXBUFFER9 g_hlslBg_vb  = NULL;
static LPDIRECT3DVERTEXDECLARATION9 g_hlslBg_vd = NULL;
static int g_hlslBkg_active = 0;

/* Tabla de presets publicada por la app + objetos compilados por preset. */
static const SalviaShaderPreset* g_presets = NULL;
static int g_presetCount = 0;
static IDirect3DPixelShader9** g_compiled = NULL;   /* [preset], NULL = passthrough */
static LPDIRECT3DTEXTURE9** g_lutTex = NULL;        /* [preset][lut] */
static int g_lutsUploaded = 0;

/* Passthrough embebido: la red de seguridad. En Xenon no hay pipeline de
   funcion fija, asi que enganchar un pixel shader NULL es PANTALLA NEGRA.
   Este objeto se compila lo primero y jamas se deja de tener. */
static IDirect3DPixelShader9* g_fallbackPS = NULL;
/* Shader realmente enganchado ahora mismo. El overlay restaura este puntero
   en vez de volver a indexar por g_current_effect. */
static IDirect3DPixelShader9* g_activePS = NULL;

int have_vertexbuffer=0;
int have_direct3dtexture=0;
static float g_display_aspect_ratio = 0.0f; /* 0 = use native pixel ratio */
static int g_display_fullscreen = 1; /* 1 = scale to fill screen, 0 = pixel perfect size */
static int g_display_overflow   = 0; /* 1 = integer scale puede salirse de pantalla */
static int g_display_scale_type = 0; /* 0=reduce,1=increase,2..6=escala fija 1x..5x */
static int g_texture_width = 0;
static int g_texture_height = 0;

/* >>> TEXTURA EXTERNA: cores que renderizan ellos mismos con Direct3D.
 *
 * Con esto puesto, la imagen del juego NO esta en this->hidden->SDL_primary
 * (nuestra textura LIN_ con LockRect permanente) sino en una textura que es
 * del CORE y que vive en la GPU.  Ver el comentario largo de SDL_video.h.
 *
 * Consecuencias en el camino de presentacion, y son las dos que importan:
 *   - NO se hace el par UnlockRect/LockRect: la externa no se bloquea nunca.
 *   - lo que se liga en s0 y lo que se le pasa a XBOX_DrawOverlay para que
 *     lo restaure es la externa.
 *
 * A NULL se vuelve al camino normal sin dejar rastro, que es lo que tiene que
 * pasar al volver al menu o al cargar otro core. */
static LPDIRECT3DTEXTURE9 g_extGameTex = NULL;
static int g_extGameW = 0;
static int g_extGameH = 0;
static int g_screen_rotation = 0; /* 0..3 = 0/90/180/270 deg CCW (libretro convention) */

typedef struct { float x, y, z, rhw; float u, v; } HLSL_BG_VTX;

static const D3DVERTEXELEMENT9 g_hlslBg_decl[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
	D3DDECL_END()
};

static const char* g_strHLSLBgVS =
	" struct VS_IN  { float4 Pos : POSITION; float2 UV : TEXCOORD0; }; \n"
	" struct VS_OUT { float4 Pos : POSITION; float2 UV : TEXCOORD0; }; \n"
	" VS_OUT main(VS_IN In) {                                          \n"
	"     VS_OUT Out; Out.Pos = In.Pos; Out.UV = In.UV; return Out;   \n"
	" }                                                                \n";

void HLSLBackground_init(LPDIRECT3DDEVICE9 dev)
{
	ID3DXBuffer* code = NULL;
	ID3DXBuffer* errs = NULL;
	ID3DXBuffer* vsCode = NULL;
	int i;

	g_strHLSLBackgrounds[0] = g_strHLSLBackground;
	g_strHLSLBackgrounds[1] = g_strHLSLBackgroundEther;
	g_strHLSLBackgrounds[2] = g_strHLSLBackgroundWormholes;
	//g_strHLSLBackgrounds[3] = g_strHLSLBackgroundShootingStars;
	//g_strHLSLBackgrounds[4] = g_strHLSLBackgroundNeonBars;

	if (!dev || g_hlslBg_ps[0]) return;

	/* Compile all background shaders (with disk cache) */
	for (i = 0; i < HLSL_BG_COUNT; i++) {
		unsigned long hash = XBOX_HashShaderSource(g_strHLSLBackgrounds[i], 0);
		DWORD cachedSize = 0;
		DWORD* cachedCode = XBOX_LoadCachedShader(hash, &cachedSize);
		if (cachedCode) {
			IDirect3DDevice9_CreatePixelShader(dev, cachedCode, &g_hlslBg_ps[i]);
			free(cachedCode);
		} else {
			if (SUCCEEDED(D3DXCompileShader(g_strHLSLBackgrounds[i], (UINT)strlen(g_strHLSLBackgrounds[i]),
					NULL, NULL, "main", "ps_3_0",
					0, &code, &errs, NULL))) {
				XBOX_SaveCachedShader(hash,
					code->lpVtbl->GetBufferPointer(code),
					code->lpVtbl->GetBufferSize(code));
				IDirect3DDevice9_CreatePixelShader(dev, (DWORD*)code->lpVtbl->GetBufferPointer(code), &g_hlslBg_ps[i]);
				code->lpVtbl->Release(code);
			}
			if (errs) { errs->lpVtbl->Release(errs); errs = NULL; }
		}
	}

	/* Compilar alpha-fixup shader (con cache) */
	{
		unsigned long hash = XBOX_HashShaderSource(g_strHLSLAlphaFix, 0);
		DWORD cachedSize = 0;
		DWORD* cachedCode = XBOX_LoadCachedShader(hash, &cachedSize);
		if (cachedCode) {
			IDirect3DDevice9_CreatePixelShader(dev, cachedCode, &g_hlslBg_psAlphaFix);
			free(cachedCode);
		} else {
			if (SUCCEEDED(D3DXCompileShader(g_strHLSLAlphaFix, (UINT)strlen(g_strHLSLAlphaFix),
					NULL, NULL, "main", "ps_3_0",
					0, &code, &errs, NULL))) {
				XBOX_SaveCachedShader(hash,
					code->lpVtbl->GetBufferPointer(code),
					code->lpVtbl->GetBufferSize(code));
				IDirect3DDevice9_CreatePixelShader(dev, (DWORD*)code->lpVtbl->GetBufferPointer(code), &g_hlslBg_psAlphaFix);
				code->lpVtbl->Release(code);
			}
			if (errs) { errs->lpVtbl->Release(errs); errs = NULL; }
		}
	}

	if (SUCCEEDED(D3DXCompileShader(g_strHLSLBgVS, (UINT)strlen(g_strHLSLBgVS),
			NULL, NULL, "main", "vs_3_0",
			0, &vsCode, &errs, NULL))) {
		D3DVertexShader* pVS = NULL;
		IDirect3DDevice9_CreateVertexShader(dev, (DWORD*)vsCode->lpVtbl->GetBufferPointer(vsCode), &pVS);
		if (pVS) { IDirect3DDevice9_SetVertexShader(dev, pVS); D3DVertexShader_Release(pVS); }
		vsCode->lpVtbl->Release(vsCode);
	}
	if (errs) { errs->lpVtbl->Release(errs); errs = NULL; }

	IDirect3DDevice9_CreateVertexDeclaration(dev, g_hlslBg_decl, &g_hlslBg_vd);
	IDirect3DDevice9_CreateVertexBuffer(dev, sizeof(HLSL_BG_VTX) * 4, D3DUSAGE_WRITEONLY,
		0, D3DPOOL_DEFAULT, &g_hlslBg_vb, NULL);
}

void HLSLBackground_draw(LPDIRECT3DDEVICE9 dev)
{
	HLSL_BG_VTX* vtx;
	D3DVIEWPORT9 vp;
	float w, h, t;
	float cTime[4], cRes[4];

	if (!dev || !g_hlslBg_ps[0] || !g_hlslBg_vb) return;

	IDirect3DDevice9_GetViewport(dev, &vp);
	w = (float)vp.Width;
	h = (float)vp.Height;

	IDirect3DVertexBuffer9_Lock(g_hlslBg_vb, 0, 0, (BYTE**)&vtx, 0);
	vtx[0].x = -0.5f;    vtx[0].y = h - 0.5f; vtx[0].z = 0; vtx[0].rhw = 1; vtx[0].u = 0; vtx[0].v = 1;
	vtx[1].x = -0.5f;    vtx[1].y = -0.5f;    vtx[1].z = 0; vtx[1].rhw = 1; vtx[1].u = 0; vtx[1].v = 0;
	vtx[2].x = w - 0.5f; vtx[2].y = h - 0.5f; vtx[2].z = 0; vtx[2].rhw = 1; vtx[2].u = 1; vtx[2].v = 1;
	vtx[3].x = w - 0.5f; vtx[3].y = -0.5f;    vtx[3].z = 0; vtx[3].rhw = 1; vtx[3].u = 1; vtx[3].v = 0;
	IDirect3DVertexBuffer9_Unlock(g_hlslBg_vb);

	t = SDL_GetTicks() / 1000.0f;
	cTime[0] = t; cTime[1] = 0; cTime[2] = 0; cTime[3] = 0;
	cRes[0]  = w; cRes[1]  = h; cRes[2]  = 0; cRes[3]  = 0;

	IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
	IDirect3DDevice9_SetTexture(dev, 0, NULL);
	IDirect3DDevice9_SetStreamSource(dev, 0, g_hlslBg_vb, 0, sizeof(HLSL_BG_VTX));
	IDirect3DDevice9_SetVertexDeclaration(dev, g_hlslBg_vd);
	IDirect3DDevice9_SetPixelShaderConstantF(dev, 0, cTime, 1);
	IDirect3DDevice9_SetPixelShaderConstantF(dev, 1, cRes, 1);
	{
		int idx = (g_hlslBkg_active >= 1 && g_hlslBkg_active <= HLSL_BG_COUNT) ? g_hlslBkg_active - 1 : 0;
		IDirect3DDevice9_SetPixelShader(dev, g_hlslBg_ps[idx]);
	}
	IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_DrawPrimitive(dev, D3DPT_TRIANGLESTRIP, 0, 2);

	/* Restaurar c1 = dims de la textura del juego (ver nota en win_d3d9.cpp:
	 * el fondo debe dejar c1 intacto para el shader del juego). Los shaders del
	 * juego leen c1 como textureDims; sin esto, al volver del menu el efecto
	 * muestrea con la resolucion del backbuffer y "desaparece". */
	if (g_texture_width > 0) {
		float cGameDims[4] = { (float)g_texture_width, (float)g_texture_height, 0, 0 };
		IDirect3DDevice9_SetPixelShaderConstantF(dev, 1, cGameDims, 1);
	}
}

void HLSLBackground_shutdown(void)
{
	int i;
	if (g_hlslBg_vb) { IDirect3DVertexBuffer9_Release(g_hlslBg_vb); g_hlslBg_vb = NULL; }
	for (i = 0; i < HLSL_BG_COUNT; i++) {
		if (g_hlslBg_ps[i]) { IDirect3DPixelShader9_Release(g_hlslBg_ps[i]); g_hlslBg_ps[i] = NULL; }
	}
	if (g_hlslBg_psAlphaFix) { IDirect3DPixelShader9_Release(g_hlslBg_psAlphaFix); g_hlslBg_psAlphaFix = NULL; }
	if (g_hlslBg_vd) { IDirect3DVertexDeclaration9_Release(g_hlslBg_vd); g_hlslBg_vd = NULL; }
}

void HLSLBackground_setActive(int active)
{
	g_hlslBkg_active = active;
}

int HLSLBackground_getActive(){
	return g_hlslBkg_active;
}

LPDIRECT3DPIXELSHADER9 HLSLBackground_getAlphaFixShader(void)
{
	return g_hlslBg_psAlphaFix;
}
/* ─── End HLSLBackground ─────────────────────────────────────── */

/* [XBOX360] Critical section que serializa IDirect3DDevice9::Present.
 *
 * El runtime D3D9 de Xenon NO es thread-safe (no existe el flag
 * D3DCREATE_MULTITHREADED).  Salvia tiene un watcher thread
 * (th_printLoading) que puede llamar SDL_Flip durante un hang del main
 * thread, lo que requiere serializar el Present para no corromper el
 * ring buffer del GPU.
 *
 * Coste por Flip: ~us de adquisicion.  Despreciable a 60 fps.
 * Solo aplica al wrap del Present; el rendering normal sigue intacto. */
static CRITICAL_SECTION g_xboxFlipCS;
static int              g_xboxFlipCSInit = 0;

/* D3DRS_PRESENTIMMEDIATETHRESHOLD de fabrica, leido al crear el device: lo que
 * deja la sincronizacion "Video".  Ver SDL_XBOX_SetAdaptiveVSync. */
static DWORD            g_umbralInmediatoFabrica = 0;
static int              g_umbralInmediatoLeido   = 0;

/* --- Reloj de presentacion por HARDWARE ------------------------------------
 *
 * POR QUE EXISTE.  Todo el pacing del frontend inferia el reloj de la pantalla
 * a partir del reloj de la CPU: medias moviles del periodo entre presents,
 * enganche a la rejilla del timer del scheduler, umbrales para decidir si
 * "llegamos o no".  Y el hardware lo sabe de primera mano: este callback lo
 * llama el driver en CADA vblank, y `D3DVBLANK_SWAPMISSED` dice sin ambiguedad
 * que un swap no llego a su vblank.  Esa bandera es literalmente la senal que
 * las EMAs y los umbrales intentaban estimar.
 *
 * Dos usos inmediatos, los dos de MEDIDA y no de control:
 *   1. Contar vblanks contra el reloj de CPU da la tasa de refresco REAL, y con
 *      ella se sabe si `XVIDEO_MODE.RefreshRate` reporta el valor real o el
 *      nominal: un panel a 59,94 que se declara 60,000 es indistinguible sin
 *      contar.
 *   2. Vblanks por frame PRESENTADO. Con un core PAL a 50 Hz en un panel a 60
 *      tiene que salir alternando 1 y 2 en patron 5:6, porque 50 no divide a
 *      60. Si sale constante, el modelo del Present esta mal entendido.
 *
 * CORRE EN CONTEXTO DE DRIVER, asi que aqui NO se hace nada mas que incrementar
 * dos contadores.  Ni logs, ni locks, ni D3D: cualquier bloqueo aqui se paga en
 * la salida de video.  Son `volatile unsigned int` alineados, una escritura de
 * 32 bits es atomica en PPC, y hay un solo escritor. El lector (hilo principal)
 * solo compara y divide, asi que un valor rancio de un vblank no le afecta. */
static volatile unsigned int g_vblankCount  = 0;
static volatile unsigned int g_vblankMissed = 0;
static HANDLE                g_vblankEvent  = NULL;

static void WINAPI XBOX_VBlankCallback(D3DVBLANKDATA *pData)
{
    ++g_vblankCount;
    if (pData && (pData->Flags & D3DVBLANK_SWAPMISSED))
        ++g_vblankMissed;

    /* Pulso para el que espere.  `SetEvent` sobre un evento de auto-reset es lo
     * unico que se hace aqui ademas de los dos contadores: no hay lock que
     * pueda estar tomado por el hilo principal, asi que no hay inversion de
     * prioridad posible.
     *
     * Y el que espera NO se fia del pulso: vuelve a leer el contador en cada
     * vuelta (ver pc_wait_vblank).  Asi un pulso que quede senalado sin nadie
     * escuchando solo cuesta una vuelta de bucle de mas, en vez de devolver un
     * vblank antes de tiempo. */
    if (g_vblankEvent)
        SetEvent(g_vblankEvent);
}

void SDL_XBOX_GetVBlankStats(unsigned int *count, unsigned int *missed)
{
    if (count)  *count  = g_vblankCount;
    if (missed) *missed = g_vblankMissed;
}

/* Espera bloqueante al siguiente vblank.  Devuelve 0 si no se puede esperar
 * (sin evento), para que el llamante caiga a su propio camino. */
int SDL_XBOX_WaitVBlank(unsigned int msMax)
{
    if (!g_vblankEvent)
        return 0;

    WaitForSingleObject(g_vblankEvent, msMax);
    return 1;
}

/* Avance del barrido en el frame actual, 0-100.  -1 si no se sabe.
 * `GetRasterStatus` no bloquea: es una lectura de registro del GPU. */
int SDL_XBOX_GetScanlinePercent(void)
{
    D3DRASTER_STATUS rs;
    int alto;

    if (!D3D_Device)
        return -1;

    IDirect3DDevice9_GetRasterStatus(D3D_Device, 0, &rs);

    if (rs.InVBlank)
        return 100;

    alto = (int)D3D_PP.BackBufferHeight;
    if (alto <= 0)
        return -1;

    return (int)((rs.ScanLine * 100) / (UINT)alto);
}

/* Initialization/Query functions */
static int XBOX_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **XBOX_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *XBOX_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int XBOX_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void XBOX_VideoQuit(_THIS);

/* Hardware surface functions */
static int XBOX_AllocHWSurface(_THIS, SDL_Surface *surface);
static int XBOX_LockHWSurface(_THIS, SDL_Surface *surface);
static void XBOX_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void XBOX_FreeHWSurface(_THIS, SDL_Surface *surface);
static int XBOX_RenderSurface(_THIS, SDL_Surface *surface);
static int XBOX_FillHWRect(_THIS, SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
static int XBOX_CheckHWBlit(_THIS, SDL_Surface *src, SDL_Surface *dst);
static int XBOX_HWAccelBlit(SDL_Surface *src, SDL_Rect *srcrect,SDL_Surface *dst, SDL_Rect *dstrect);
static int XBOX_SetHWAlpha(_THIS, SDL_Surface *surface, Uint8 alpha);
static int XBOX_SetHWColorKey(_THIS, SDL_Surface *surface, Uint32 key);
static int XBOX_SetFlickerFilter(_THIS, SDL_Surface *surface, int filter);
static int XBOX_SetSoftDisplayFilter(_THIS, SDL_Surface *surface, int enabled);


/* The functions used to manipulate software video overlays */
static struct private_yuvhwfuncs XBOX_yuvfuncs = {
	XBOX_LockYUVOverlay,
	XBOX_UnlockYUVOverlay,
	XBOX_DisplayYUVOverlay,
	XBOX_FreeYUVOverlay
};

struct private_yuvhwdata {
	LPDIRECT3DTEXTURE9 surface;
	
	/* These are just so we don't have to allocate them separately */
	Uint16 pitches[3];
	Uint8 *planes[3];
};

/* UVs para el "single fullscreen triangle": 3 vertices por rotacion,
 *   v0 = TL (cubre la esquina TL del rect visible)
 *   v1 = 2x TR (TL + 2*(TR-TL))
 *   v2 = 2x BL (TL + 2*(BL-TL))
 * Las UVs llegan a (0..1) en las cuatro esquinas del rect visible al
 * interpolarse y el resto del triangulo queda fuera del scissor.
 * Identidad (0 deg): TL=(0,0), TR=(1,0), BL=(0,1) -> v0=(0,0), v1=(2,0), v2=(0,2).
 * Para las demas rotaciones aplicamos la misma formula. */
static const float g_uv_rot[4][6] = {
	/* 0:   0 deg  */ { 0.0f, 0.0f,  2.0f, 0.0f,  0.0f, 2.0f },
	/* 1:  90 CCW  */ { 1.0f, 0.0f,  1.0f, 2.0f, -1.0f, 0.0f },
	/* 2: 180      */ { 1.0f, 1.0f, -1.0f, 1.0f,  1.0f,-1.0f },
	/* 3: 270 CCW  */ { 0.0f, 1.0f,  0.0f,-1.0f,  2.0f, 1.0f },
};

/* Rect del area visible (sin las bandas negras de letterbox/pillarbox).
 * Lo usamos como scissor para clipear el sobrepintado del triangulo
 * oversized.  Se actualiza en XBOX_UpdateVertexBuffer. */
static RECT g_visible_rect = { 0, 0, 0, 0 };

/* Overlay system: a 1280x720 ARGB layer drawn on top of the game quad */
static LPDIRECT3DTEXTURE9 g_overlay_texture = NULL;
static LPDIRECT3DVERTEXBUFFER9 g_overlay_vb = NULL;
static SDL_Surface* g_overlay_surface = NULL;
static int g_overlay_enabled = 0;
static int g_overlay_locked = 0;
static int g_current_effect = 0;

/* Overscan del overlay: desplaza los bordes del quad hacia adentro (positivo)
   o hacia afuera (negativo).  Se aplica via SDL_XBOX_SetOverscan. */
static int g_overlay_overscan_x = 0;
static int g_overlay_overscan_y = 0;

/** Especifica la resolucion de pantalla que puede ser configurada*/
static int g_screen_resolution = 0;

/* Filtro de muestreo actual del s0 segun el efecto activo (LINEAR/POINT).
 * Lo mantiene XBOX_SetSampler0Filter cuando XBOX_SelectEffect lo decide.
 * XBOX_DrawOverlay lee este valor para restaurar el filtro despues de
 * forzar LINEAR temporal para dibujar el UI, sin tener que reentrar a
 * XBOX_SelectEffect entero por frame. */
static D3DTEXTUREFILTERTYPE g_current_sampler_filter = D3DTEXF_LINEAR;

static void XBOX_SetSampler0Filter(D3DTEXTUREFILTERTYPE filter)
{
    g_current_sampler_filter = filter;
    IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, filter);
    IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, filter);
}

/* La lista de efectos ya no es fija: la publica la capa de aplicacion desde
   assets\shaders. El indice de efecto es la posicion en esa tabla, y las LUT
   vienen dentro del propio preset (clave `textures =` estilo RetroArch). */

D3DVERTEXELEMENT9 decl[] = 
{
{ 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
D3DDECL_END()
};

//--------------------------------------------------------------------------------------
// Vertex Shader (vs_3_0)
//--------------------------------------------------------------------------------------
static const CHAR* g_strGradientShader =
    "struct VS_IN                                              \n"
    "{                                                         \n"
    "   float4 Position : POSITION;                            \n"
    "   float2 UV       : TEXCOORD0;                           \n"
    "};                                                        \n"
    "                                                          \n"
    "struct VS_OUT                                             \n"
    "{                                                         \n"
    "   float4 Position : POSITION;                            \n" // En vs_3_0 POSITION es valido
    "   float2 UV       : TEXCOORD0;                           \n"
    "};                                                        \n"
    "                                                          \n"
    "VS_OUT GradientVertexShader( VS_IN In )                   \n"
    "{                                                         \n"
    "   VS_OUT Out;                                            \n"
    "   Out.Position = In.Position;                            \n"
    "   Out.UV       = In.UV;                                  \n"
    "   return Out;                                            \n"
    "}                                                         \n";
   
//-------------------------------------------------------------------------------------
// Pixel Shader (ps_3_0)
//-------------------------------------------------------------------------------------
/*const char* g_strPixelShaderProgram =
    " struct PS_IN                                 "
    " {                                            "
    "     float2 TexCoord : TEXCOORD0;             " 
    " };                                           "
    "                                              "
    " sampler2D detail : register(s0);             " 
    "                                              "
    " float4 main( PS_IN In ) : COLOR0             " 
    " {                                            "
    "     return tex2D( detail, In.TexCoord );     "
    " }     ";
*/

const char* g_strPixelShaderProgram =
    " float fFilterType : register(c0);            "
    "                                              "
    " struct PS_IN                                 "
    " {                                            "
    "     float2 TexCoord : TEXCOORD0;             "
    " };                                           "
    "                                              "
    " sampler2D detail : register(s0);             "
    "                                              "
    " float4 main( PS_IN In ) : COLOR0             "
    " {                                            "
    "     float4 color = tex2D( detail, In.TexCoord ); "
    "                                              "
    "     if (fFilterType > 0.5 && fFilterType < 1.5) { " // Tipo 1: Grises
    "         float gray = dot(color.rgb, float3(0.299, 0.587, 0.114)); "
    "         return float4(gray, gray, gray, color.a); "
    "     }                                        "
    "     else if (fFilterType > 1.5) {             " // Tipo 2: Sepia
    "         float r = (color.r * 0.393) + (color.g * 0.769) + (color.b * 0.189); "
    "         float g = (color.r * 0.349) + (color.g * 0.686) + (color.b * 0.168); "
    "         float b = (color.r * 0.272) + (color.g * 0.534) + (color.b * 0.131); "
    "         return float4(r, g, b, color.a);      "
    "     }                                        "
    "                                              "
    "     return color;                            " // Tipo 0: Original
    " }     ";
 
static void XBOX_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

/* XBOX driver bootstrap functions */

static int XBOX_Available(void)
{
	return(1);
}

static void XBOX_DeleteDevice(SDL_VideoDevice *device)
{
	free(device->hidden);
	free(device);
}

static SDL_VideoDevice *XBOX_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			free(device);
		}
		return(0);
	}
	memset(device->hidden, 0, (sizeof *device->hidden));

	/* Set the function pointers */
	device->VideoInit = XBOX_VideoInit;
	device->ListModes = XBOX_ListModes;
	device->SetVideoMode = XBOX_SetVideoMode;
	device->CreateYUVOverlay = XBOX_CreateYUVOverlay;
	device->SetColors = XBOX_SetColors;
	device->UpdateRects = XBOX_UpdateRects;
	device->VideoQuit = XBOX_VideoQuit;
	device->AllocHWSurface = XBOX_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = XBOX_SetHWColorKey;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = XBOX_LockHWSurface;
	device->UnlockHWSurface = XBOX_UnlockHWSurface;
	device->FlipHWSurface = XBOX_RenderSurface;
	device->FreeHWSurface = XBOX_FreeHWSurface;
	device->SetCaption = NULL;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = NULL;
	device->InitOSKeymap = XBOX_InitOSKeymap;
	device->PumpEvents = XBOX_PumpEvents;
	device->free = XBOX_DeleteDevice;

	return device;
}

VideoBootStrap XBOX_bootstrap = {
	XBOXVID_DRIVER_NAME, "XBOX 360 SDL video driver V0.01",
	XBOX_Available, XBOX_CreateDevice
};

const static SDL_Rect
	RECT_1280x720 = {0,0,1280,720},
	RECT_1280x1024 = {0,0,1280,1024},
	RECT_1024x768 = {0,0,1024,768},
	RECT_800x600 = {0,0,800,600},
	RECT_640x480 = {0,0,640,480};

const static SDL_Rect * const vid_modes[] = {
	&RECT_1280x720,
	&RECT_1280x1024,
	&RECT_1024x768,
	&RECT_800x600,
	&RECT_640x480,
	NULL
};

int XBOX_VideoInit(_THIS, SDL_PixelFormat *vformat)
{	 
    // 1. TODAS las declaraciones de variables al principio
    D3DCAPS9 caps;
    HRESULT hr;

#ifdef SDL_XBOX_HIDMOUSE
    /* Instala el lector de raton USB HID nativo (hooks de kernel). Idempotente
     * y fail-safe: si el build de dashboard no esta soportado o la consola no
     * es CFW, no instala nada y el raton queda inactivo. Se hace aqui, al
     * inicializar el video, porque es un buen punto emparejado con VideoQuit. */
    XBOX_HIDMouse_Init();
#endif

    if (!D3D)
        D3D = Direct3DCreate9(D3D_SDK_VERSION);

    // 2. Ahora las instrucciones ejecutables
    if (D3D) {
        IDirect3D9_GetDeviceCaps(D3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
        
        // Comprobar si soporta Shader Model 3.0 (0x0300)
        if (caps.PixelShaderVersion < D3DPS_VERSION(3, 0)) {
            return 0;
        }
    }

    ZeroMemory(&D3D_PP, sizeof(D3D_PP));

	if (vid_modes[g_screen_resolution] != NULL) {
		D3D_PP.BackBufferWidth = vid_modes[g_screen_resolution]->w;
		D3D_PP.BackBufferHeight = vid_modes[g_screen_resolution]->h;
	}

    D3D_PP.BackBufferFormat = D3DFMT_X8R8G8B8;
    
    // En Xbox 360, estas opciones son recomendadas para rendimiento
    //D3D_PP.EnableAutoDepthStencil = TRUE;
    //D3D_PP.AutoDepthStencilFormat = D3DFMT_D24S8;

#ifdef NOVSYNC
    D3D_PP.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    D3D_PP.SwapEffect = D3DSWAPEFFECT_DISCARD;
#else
    D3D_PP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
#endif

    /* -- Banderas de CreateDevice, y por que estas ---------------------------
     *
     * ★ CORRECCION: aqui ponia "Usamos D3DCREATE_HARDWARE_VERTEXPROCESSING
     * para asegurar soporte de VS_3_0".  Es FALSO.  En el d3d9.h del XDK esa
     * bandera vale 0x00000000L y esta marcada "Ignored on Xbox 360", igual
     * que D3DCREATE_PUREDEVICE y D3DCREATE_FPU_PRESERVE.  O sea que hasta hoy
     * el device se creaba SIN NINGUNA BANDERA.
     *
     * [XBOX360] NO existe D3DCREATE_MULTITHREADED en el D3D9 de Xenon.  La
     * sincronizacion del Present entre hilos se hace a mano con g_xboxFlipCS,
     * abajo en XBOX_RenderSurface.  Eso NO cambia: es el motivo de que el
     * device sea uno solo por proceso y de que los cores que dibujan por GPU
     * (Xenon64) tengan que dejarlo sin trabajo en vuelo antes de devolverlo.
     *
     * ★★ PROBADAS Y REVERTIDAS (2026-09-20).  Se midio
     * BUFFER_2_FRAMES | CREATE_THREAD_ON_3 en Majora's Mask con glN64_GX y
     * NO movieron nada: `[GFXTH] trabajo` 518 -> 507-538 ms/s y
     * `[FRAME] bloq` 9,04 -> 8,5-9,0, todo dentro del ruido entre escenas.
     * El device se creo con ellas (no salto el reintento de abajo).
     *
     * Lo que eso DESCARTA, que es lo util: si `BUFFER_2_FRAMES` no baja
     * `trabajo`, ese tiempo no era la CPU esperando con el command buffer
     * lleno -- es trabajo real de CPU emitiendo comandos.  No hay que volver
     * por aqui.
     *
     * Se quedan documentadas y a 0.  Para reactivarlas basta con poner el
     * OR de vuelta en XBOX_D3D_FLAGS.
     *
     *   BUFFER_2_FRAMES     deja a D3D encolar dos frames de comandos.  Si
     *                       parte del coste del hilo grafico de un core es
     *                       la CPU parada con el command buffer lleno, esto
     *                       lo recupera.  Se mira en `[GFXTH] trabajo` de
     *                       Xenon64: tiene que BAJAR.  Si no se mueve, ese
     *                       tiempo era trabajo real y la bandera sobra.
     *
     *   CREATE_THREAD_ON_3  pinea el hilo interno de D3D a HW3.  Pineamos a
     *                       mano todos los hilos nuestros (emulacion HW0,
     *                       grafico HW2, audio HW4, partidas HW5) y este se
     *                       dejaba al azar: si el XDK lo pone en HW0 o HW2
     *                       le esta robando unidades de ejecucion a la
     *                       emulacion o al grafico.  HW3 es el unico libre
     *                       (hermano SMT del grafico, que es peor que un
     *                       nucleo entero pero mejor que pisar a otro).
     *                       Se mira en `[FRAME] emul` y `[PACE] flip`.
     *
     * NO se activa D3DCREATE_ASYNCHRONOUS_SWAPS todavia: cambia el momento en
     * que vuelve el Present, y este frontend tiene su propio framepacing
     * (Sync::limit_fps) calibrado contra el Present actual.  Una cosa cada
     * vez, o el log no dira cual fue.
     *
     * NO se activa D3DCREATE_NO_SHADER_PATCHING: Xenos parchea el vertex
     * shader para casar con el pixel shader ligado, y quitarlo obliga a casar
     * las firmas a mano en TODOS los caminos de dibujo (frontend y cores).
     * No es una bandera suelta, es un contrato. */
#define XBOX_D3D_FLAGS  0
/* Medido sin ganancia:  (D3DCREATE_BUFFER_2_FRAMES | D3DCREATE_CREATE_THREAD_ON_3) */

    if (!D3D_Device) {
        hr = IDirect3D9_CreateDevice(D3D, 0, D3DDEVTYPE_HAL, NULL,
                                     XBOX_D3D_FLAGS,
                                     &D3D_PP, &D3D_Device);

        /* ★ Red de seguridad del experimento.  Si esa combinacion de
         * banderas no le gusta al driver, sin esto el frontend se queda sin
         * video ENTERO y sin pista de por que -- el unico sintoma seria un
         * arranque en negro.  Reintentar con 0 deja el comportamiento de
         * siempre y una linea en el log que lo dice. */
        if (FAILED(hr) && XBOX_D3D_FLAGS != 0) {
            OutputDebugStringA("[XBOXVIDEO] CreateDevice fallo con las banderas"
                               " nuevas; reintento sin ellas\n");
            hr = IDirect3D9_CreateDevice(D3D, 0, D3DDEVTYPE_HAL, NULL,
                                         0,
                                         &D3D_PP, &D3D_Device);
        }
        if (FAILED(hr)) return 0;

        /* Init del lock una sola vez. */
        if (!g_xboxFlipCSInit) {
            InitializeCriticalSection(&g_xboxFlipCS);
            g_xboxFlipCSInit = 1;
        }

        /* Reloj de presentacion por HARDWARE.  Ver el bloque de
         * XBOX_VBlankCallback para el por que; aqui solo se engancha.
         *
         * El evento se crea ANTES de enganchar el callback: si se hiciera al
         * reves, los primeros vblanks llamarian al callback con `g_vblankEvent`
         * a NULL.  No pasaria nada -el callback lo comprueba- pero el orden
         * correcto no cuesta nada. */
        if (!g_vblankEvent)
            g_vblankEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        IDirect3DDevice9_SetVerticalBlankCallback(D3D_Device, XBOX_VBlankCallback);

        /* FASE 2: el Present deja de ser un regulador, y lo decide el HARDWARE.
         *
         * `D3DRS_PRESENTIMMEDIATETHRESHOLD` es el porcentaje de avance del DAC a
         * partir del cual un Present con vsync se convierte en INMEDIATO
         * (d3d9types.h:625).  O sea: si al presentar el barrido ya va por encima
         * de ese punto, no se espera al vblank siguiente, se presenta ya.
         *
         * Eso es tearing SOLO en los frames que ya iban tarde, que es cuando es
         * el mal menor, y sustituye a una maquina de estados entera en software
         * -la del vsync adaptativo, con sus dos calibraciones de umbrales, su
         * integrador, su guarda anti-aleteo y sus 2071 conmutaciones por log-.
         *
         * 80 y no otro numero: por debajo se empieza a rasgar en frames que aun
         * llegaban, y por encima de ~90 el margen que queda es menor que la
         * propia latencia de la llamada y deja de actuar.
         *
         * ★ YA NO SE PONE AQUI PARA TODOS: solo con la sincronizacion "Video
         * adaptativo" (SDL_XBOX_SetAdaptiveVSync, desde Sync::limit_fps).  Puesto
         * siempre rasgaba cores que van sobrados (Gambatte, Genesis Plus GX): con
         * el pacer por vblanks, el present despues de cada vblank saltado sale
         * "tarde".  Aqui se lee el valor de fabrica, que es el que tenia el
         * device antes de 9f9f64bc y el que deja "Video". */
        IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_PRESENTINTERVAL,
            D3DPRESENT_INTERVAL_ONE);
        IDirect3DDevice9_GetRenderState(D3D_Device, D3DRS_PRESENTIMMEDIATETHRESHOLD,
            &g_umbralInmediatoFabrica);
        g_umbralInmediatoLeido = 1;

        HLSLBackground_init(D3D_Device);
    }

    vformat->BitsPerPixel = 32;
    vformat->BytesPerPixel = 4;
    vformat->Amask = 0xFF000000;
    vformat->Rmask = 0x00FF0000;
    vformat->Gmask = 0x0000FF00;
    vformat->Bmask = 0x000000FF;

    return (D3D_Device) ? 1 : 0;
}

SDL_Rect **XBOX_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	/* Return -1 to indicate any resolution is supported.
	   The texture is created at the requested size and scaled to the backbuffer. */
	return (SDL_Rect **)-1;
}

void XBOX_SetVideoFilter(int filterType)
{
    // Creamos un array de 4 floats porque los registros c0-cN son float4
    float values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    
    values[0] = (float)filterType; // El primer componente es nuestro selector

    if (D3D_Device) {
        // Enviamos el valor al registro c0 del Pixel Shader
        IDirect3DDevice9_SetPixelShaderConstantF(D3D_Device, 0, values, 1);
    }
}

/* Returns the pixel scale factor for the current effect.
   Nearest/Bilinear/Scanlines/CRT = 1x, HQ2x = 2x, HQ3x/xBR variants = 3x, HQ4x = 4x.
   xBR shaders usan `fp = frac(uv*texDims)` para decidir el subpixel y son
   scale-agnostic en el calculo per-pixel.  Renderizamos a 3x (la infraestructura
   HQ3x) porque es suficiente para los detalles edge-aware del algoritmo y no
   sobrecarga el fillrate como 5x lo haria. */
static int XBOX_GetEffectScale(void)
{
	switch (g_current_effect) {
//		case 8:  return 2; /* HQ2x */
//		case 9:  return 3; /* HQ3x */
//		case 10: return 4; /* HQ4x */
//		case 11: return 3; /* xBR-lv2-fast */
//		case 12: return 3; /* 5xBR-Hyllian (rendered at 3x via HQ3x infra) */
		default: return 1; /* 0=Nearest, 1=Sharp-Bilinear, 2=Bilinear, 3=LCD-Grid-v2,
		                      4=Scanlines, 5=CRT-Geom, 6=CRT-Lottes, 7=CRT-Easymode */
	}
}

/* Dibuja el quad principal usando el "single fullscreen triangle".  El
 * triangulo de 3 vertices excede el rect visible para evitar la arista
 * compartida del antiguo triangle strip; el scissor lo recorta a la zona
 * sin letterbox/pillarbox.  Se llama desde XBOX_RenderSurface y desde
 * XBOX_UpdateRects (mismos parametros). */
static void XBOX_DrawMainQuad(void)
{
	IDirect3DDevice9_SetScissorRect(D3D_Device, &g_visible_rect);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_SCISSORTESTENABLE, TRUE);
	IDirect3DDevice9_DrawPrimitive(D3D_Device, D3DPT_TRIANGLELIST, 0, 1);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_SCISSORTESTENABLE, FALSE);
}

/* Si g_display_fullscreen == 1 (fullscreen): aspect_ratio define el aspect
 *   del quad mostrado en pantalla (con letterbox/pillarbox).  aspect_ratio<=0
 *   usa el ratio nativo del source (tex_w/tex_h).
 * Si g_display_fullscreen == 0 (pixel-perfect): si aspect_ratio > 0 se aplica
 *   tambien aqui (Y entero crisp, X estirado al ratio elegido).  Si <=0 se
 *   escala por el maximo factor entero en ambos ejes (ratio nativo). */
static void XBOX_UpdateVertexBuffer(int tex_w, int tex_h, float aspect_ratio)
{
	void *pLockedVertexBuffer;
	float display_w, display_h, offset_x, offset_y;
	float bbw = (float)D3D_PP.BackBufferWidth;
	float bbh = (float)D3D_PP.BackBufferHeight;
	float bb_ratio;

	/* Si el contenido se rota 90 o 270, el ancho y alto efectivos se
	   intercambian para todo el calculo de tamano/posicion del quad. La
	   textura en VRAM mantiene sus dimensiones reales: solo las UVs y el
	   rectangulo en pantalla se invierten. */
	if (g_screen_rotation & 1) {
		int tmp = tex_w;
		tex_w = tex_h;
		tex_h = tmp;
	}

	if (g_display_fullscreen) {
		/* Scale to fill screen maintaining aspect ratio */
		float bb_ratio = bbw / bbh;

		if (aspect_ratio <= 0.0f)
			aspect_ratio = (float)tex_w / (float)tex_h;

		if (aspect_ratio > bb_ratio) {
			display_w = bbw;
			display_h = bbw / aspect_ratio;
		} else {
			display_h = bbh;
			display_w = bbh * aspect_ratio;
		}
	} else {
		/* Pixel perfect: exact size based on effect scale factor */
		int scale = XBOX_GetEffectScale();

		/* Modos de escala entera FIJA 1x..5x (g_display_scale_type 2..6): factor fijo,
		   se salta el auto-calculo y puede salirse de pantalla (g_display_overflow ya es true). */
		int fixed = (g_display_scale_type >= 2) ? (g_display_scale_type - 1) : 0;
		if (fixed > 0) scale = fixed;

		if (aspect_ratio > 0.0f && tex_w > 0 && tex_h > 0) {
			/* Aspect ratio definido (4:3, 16:9, etc.): Y entero (crisp scanlines),
			   X estirado para cumplir el ratio elegido. Comportamiento estandar
			   de emuladores: lineas horizontales nitidas, ancho proporcional. */
			int maxscale_h, maxscale_w, maxscale;

			if (fixed == 0 && (scale == 1 || g_display_overflow)) {
				/* Factor 1 = filtros sin escalado propio (nearest, scanlines, crt).
				   Subimos al maximo factor entero que cabe en el backbuffer. */
				maxscale_h = (int)floor(bbh / (float)tex_h);
				maxscale_w = (int)floor(bbw / ((float)tex_h * aspect_ratio));
				maxscale = g_display_overflow ? max(maxscale_h, maxscale_w) : min(maxscale_h, maxscale_w);
				if (maxscale > 1) {
					scale = maxscale;
				}
			}

			display_h = (float)(tex_h * scale);
			/* Forzar entero: aspect_ratio puede ser no entero (4:3 = 1.333...) y
			   un display_w fraccional descuadra el muestreo en Xenos. */
			display_w = (float)floor(display_h * aspect_ratio);
		} else {
			/* RATIO_CORE / aspect_ratio<=0: pixel perfect estricto en ambos ejes. */
			if (fixed == 0 && (scale == 1 || g_display_overflow) && tex_w > 0 && tex_h > 0){
				int maxscale_w = (int)floor(bbw / (float)tex_w);
				int maxscale_h = (int)floor(bbh / (float)tex_h);
				int maxscale = g_display_overflow ? max(maxscale_w, maxscale_h) : min(maxscale_w, maxscale_h);
				if (maxscale > 1){
					scale = maxscale;
				}
			}

			display_w = (float)(tex_w * scale);
			display_h = (float)(tex_h * scale);
		}

		/* Clamp to backbuffer if larger */
		if (!g_display_overflow && (display_w > bbw || display_h > bbh)) {
			float clamp_ratio = aspect_ratio;
			if (clamp_ratio <= 0.0f)
				clamp_ratio = (float)tex_w / (float)tex_h;

			bb_ratio = bbw / bbh;

			if (clamp_ratio > bb_ratio) {
				display_w = bbw;
				display_h = (float)floor(bbw / clamp_ratio);
			} else {
				display_h = bbh;
				display_w = (float)floor(bbh * clamp_ratio);
			}
		}
	}

	/* Centrado entero: con dimensiones impares la mitad daria .5, lo que
	   produce muestreo sub-pixel y blur permanente con LINEAR y "wobble"
	   con POINT.  En Xenos (centro-de-pixel) las posiciones deben ser
	   enteras para mantener pixel-perfect. */
	offset_x = (float)floor((bbw - display_w) * 0.5f);
	offset_y = (float)floor((bbh - display_h) * 0.5f);

	/* Guardar el rect visible para que el scissor recorte el sobrepintado
	   del triangulo oversized.  Coordenadas en pixeles del backbuffer. */
	g_visible_rect.left   = (LONG)offset_x;
	g_visible_rect.top    = (LONG)offset_y;
	g_visible_rect.right  = (LONG)(offset_x + display_w);
	g_visible_rect.bottom = (LONG)(offset_y + display_h);

	IDirect3DVertexBuffer9_Lock(vertexBuffer, 0, 0, (BYTE **)&pLockedVertexBuffer, 0L);

	/* SINGLE FULLSCREEN TRIANGLE: un solo triangulo oversized cubre el
	   rect visible cuando lo recorta el scissor.  Sin arista compartida
	   no hay "seam" diagonal en TL->BR, y se evita el doble shading
	   (helper invocations) de los 2x2 del rasterizador que cruzan la
	   union de dos triangulos.  Misma o menor carga GPU.

	     v0 = TL                   (esquina TL del rect visible)
	     v1 = TL + 2*(TR - TL)     (oversized hacia la derecha)
	     v2 = TL + 2*(BL - TL)     (oversized hacia abajo)

	   Las UVs (en g_uv_rot) siguen la misma formula. */
	{
		const float* uv = g_uv_rot[g_screen_rotation & 3];

		/* v0: TL del rect visible */
		triangleStripVertices[0].x   = offset_x;
		triangleStripVertices[0].y   = offset_y;
		triangleStripVertices[0].z   = 0;
		triangleStripVertices[0].rhw = 1;
		triangleStripVertices[0].tx  = uv[0];
		triangleStripVertices[0].ty  = uv[1];

		/* v1: 2x TR (estira 2x a la derecha desde TL) */
		triangleStripVertices[1].x   = offset_x + 2.0f * display_w;
		triangleStripVertices[1].y   = offset_y;
		triangleStripVertices[1].z   = 0;
		triangleStripVertices[1].rhw = 1;
		triangleStripVertices[1].tx  = uv[2];
		triangleStripVertices[1].ty  = uv[3];

		/* v2: 2x BL (estira 2x hacia abajo desde TL) */
		triangleStripVertices[2].x   = offset_x;
		triangleStripVertices[2].y   = offset_y + 2.0f * display_h;
		triangleStripVertices[2].z   = 0;
		triangleStripVertices[2].rhw = 1;
		triangleStripVertices[2].tx  = uv[4];
		triangleStripVertices[2].ty  = uv[5];
	}

	memcpy(pLockedVertexBuffer, triangleStripVertices, sizeof(triangleStripVertices));
	IDirect3DVertexBuffer9_Unlock(vertexBuffer);
}

void SDL_XBOX_SetDisplaySize(float aspect_ratio)
{
	g_display_aspect_ratio = aspect_ratio;
	XBOX_UpdateVertexBuffer(g_texture_width, g_texture_height, aspect_ratio);
	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
}

void SDL_XBOX_SetDisplayFullscreen(int fullscreen)
{
	g_display_fullscreen = fullscreen;
	XBOX_UpdateVertexBuffer(g_texture_width, g_texture_height, g_display_aspect_ratio);
	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
}

void SDL_XBOX_SetDisplayOverflow(int type)
{
	g_display_scale_type = type;
	g_display_overflow   = (type != 0);   /* reduce(0) recorta a pantalla; increase y 1x-5x pueden salirse */
	XBOX_UpdateVertexBuffer(g_texture_width, g_texture_height, g_display_aspect_ratio);
	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
}

void SDL_XBOX_SetRotation(int rotation)
{
	/* Saneo: solo 0..3 son validos (libretro). Cualquier otra cosa = sin rotacion. */
	g_screen_rotation = (rotation >= 0 && rotation <= 3) ? rotation : 0;
	XBOX_UpdateVertexBuffer(g_texture_width, g_texture_height, g_display_aspect_ratio);
	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
}

/* Definida mas abajo (linea ~2240).  El fichero venia llamandola sin prototipo
 * y apoyandose en la declaracion implicita de C89 -compila porque la firma
 * casa y porque este build tiene los avisos apagados-, pero aqui se declara
 * explicitamente: si algun dia cambia la firma, el error sale en la
 * compilacion y no en forma de pila corrupta. */
void XBOX_SelectEffect(int effectID);

/* >>> Re-liga en s0 la textura del juego que toque.
 *
 * La llaman los sitios que RECONSTRUYEN el estado de D3D y que, antes de
 * existir la textura externa, solo tenian que pensar en SDL_primary: el Reset
 * del device al cambiar el VSync, y XBOX_SetVideoMode al cambiar la
 * resolucion de pantalla.  Las dos cosas las puede hacer el usuario desde el
 * menu CON EL JUEGO CORRIENDO, asi que sin esto la imagen del core se
 * quedaria sustituida por la textura vacia del frontend.
 *
 * Con textura externa manda ella Y SUS DIMENSIONES: g_texture_width/height se
 * restauran desde g_extGameW/H porque esos dos sitios acaban de ponerlas al
 * tamano de SDL_primary, que aqui no es el del juego.  Y hay que pasar por
 * XBOX_SelectEffect, que es quien escribe el uniforme c1 que el shader usa
 * como tamano de textura. */
static void XBOX_RebindGameTexture(SDL_VideoDevice *dev)
{
	if (!D3D_Device) return;

	if (g_extGameTex)
	{
		g_texture_width  = g_extGameW;
		g_texture_height = g_extGameH;
		XBOX_UpdateVertexBuffer(g_extGameW, g_extGameH, g_display_aspect_ratio);
		IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)g_extGameTex);
		XBOX_SelectEffect(g_current_effect);
		return;
	}

	if (dev && dev->hidden && dev->hidden->SDL_primary)
		IDirect3DDevice9_SetTexture(D3D_Device, 0,
			(D3DBaseTexture *)dev->hidden->SDL_primary);
}

void* SDL_XBOX_GetD3DDevice(void)
{
	return (void *)D3D_Device;
}

int SDL_XBOX_SetExternalGameTexture(void *texture, int width, int height)
{
	SDL_VideoDevice *this = current_video;

	if (!D3D_Device) return 0;

	if (!texture || width <= 0 || height <= 0)
	{
		/* Vuelta al camino normal.  Se religa la textura propia para que el
		 * quad no se quede muestreando una superficie que el core va a
		 * liberar. */
		g_extGameTex = NULL;
		g_extGameW   = 0;
		g_extGameH   = 0;
		if (this && this->hidden && this->hidden->SDL_primary)
		{
			IDirect3DDevice9_SetTexture(D3D_Device, 0,
				(D3DBaseTexture *)this->hidden->SDL_primary);
		}
		return 1;
	}

	g_extGameTex     = (LPDIRECT3DTEXTURE9)texture;
	g_extGameW       = width;
	g_extGameH       = height;
	g_texture_width  = width;
	g_texture_height = height;

	/* Lo mismo que hace XBOX_ResizeGameTexture despues de crear la suya, y
	 * por los mismos motivos: el quad hay que recalcularlo (letterbox,
	 * escalado entero) y XBOX_SelectEffect es quien escribe el uniforme c1
	 * con las dimensiones que el shader necesita. */
	XBOX_UpdateVertexBuffer(width, height, g_display_aspect_ratio);
	IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)g_extGameTex);
	XBOX_SelectEffect(g_current_effect);

	return 1;
}

/* -- Capturar la imagen del juego cuando la pinta el CORE ------------------
 *
 * POR QUE HACE FALTA.  Un core que renderiza con D3D (Xenon64) no entrega
 * pixeles: entrega su textura, que el quad liga en s0.  Asi que el frontend
 * NO tiene una copia en CPU de la imagen, y `take_screenshot()` -- que es
 * quien hace la miniatura del savestate-- se queda sin fuente.
 *
 * ★ Y NO SE PUEDE LEER LA TEXTURA DIRECTAMENTE.  En Xbox 360 `Resolve()`
 * escribe SIEMPRE tileado, aunque el destino se declare `D3DFMT_LIN_`, asi
 * que un `LockRect` sobre la textura del core devuelve los texels en orden
 * de bloques: rayas horizontales en vez de imagen.
 *
 * La pieza que lo resuelve es `D3DXLoadSurfaceFromSurface`, que destila (y de
 * paso escala) y que en este arbol ya esta probada -- `fbg_destila()` de
 * Xenon64 la usa para exactamente esto.  El destino es una textura LINEAL
 * propia, que ya se puede bloquear y leer.
 *
 * COSTE.  Leer memoria de GPU por CPU va a unos 20 MB/s, o sea ~60 ms para un
 * frame de 640x480x4.  Eso es inasumible POR FRAME -que es la razon de que el
 * video vaya por textura- pero una miniatura es UNA VEZ, y el savestate que
 * la acompaña son 8,7 MB comprimidos con zlib.  Aqui 60 ms no se notan.
 *
 * Devuelve 1 y rellena `dst` con ARGB de 32 bits, `w`*`h` pixeles densos.
 * Devuelve 0 si no hay textura externa (core normal) o si algo falla, y
 * entonces el llamante simplemente se queda sin miniatura. */
int SDL_XBOX_CaptureGameTexture(void *dst, int w, int h)
{
	static LPDIRECT3DTEXTURE9 s_capTex = NULL;
	static int                s_capW   = 0;
	static int                s_capH   = 0;

	LPDIRECT3DSURFACE9 srcSurf = NULL;
	LPDIRECT3DSURFACE9 dstSurf = NULL;
	D3DLOCKED_RECT     lr;
	int                y;
	HRESULT            hr;

	if (!dst || w <= 0 || h <= 0 || !D3D_Device || !g_extGameTex)
		return 0;

	/* La de destilado, cacheada: una captura no deberia crear una textura
	 * cada vez, y ademas el tamaño casi nunca cambia. */
	if (s_capTex && (s_capW != w || s_capH != h))
	{
		IDirect3DTexture9_Release(s_capTex);
		s_capTex = NULL;
	}
	if (!s_capTex)
	{
		/* LINEAL a proposito: es lo que hace que el LockRect de abajo
		 * devuelva filas y no bloques. */
		hr = IDirect3DDevice9_CreateTexture(D3D_Device, w, h, 1, 0,
		                                    D3DFMT_LIN_A8R8G8B8,
		                                    D3DPOOL_DEFAULT, &s_capTex, NULL);
		if (FAILED(hr) || !s_capTex) { s_capTex = NULL; return 0; }
		s_capW = w;
		s_capH = h;
	}

	if (FAILED(IDirect3DTexture9_GetSurfaceLevel(g_extGameTex, 0, &srcSurf)) || !srcSurf)
		return 0;
	if (FAILED(IDirect3DTexture9_GetSurfaceLevel(s_capTex, 0, &dstSurf)) || !dstSurf)
	{
		IDirect3DSurface9_Release(srcSurf);
		return 0;
	}

	/* Destila y escala en una sola llamada.  Con filtro, que esto es una
	 * miniatura y puede venir de una resolucion mayor. */
	hr = D3DXLoadSurfaceFromSurface(dstSurf, NULL, NULL, srcSurf, NULL, NULL,
	                                D3DX_FILTER_LINEAR, 0);
	IDirect3DSurface9_Release(srcSurf);

	if (FAILED(hr))
	{
		IDirect3DSurface9_Release(dstSurf);
		return 0;
	}

	if (FAILED(IDirect3DSurface9_LockRect(dstSurf, &lr, NULL, D3DLOCK_READONLY)))
	{
		IDirect3DSurface9_Release(dstSurf);
		return 0;
	}

	/* Fila a fila: el pitch de la superficie no tiene por que ser w*4. */
	for (y = 0; y < h; ++y)
	{
		memcpy((unsigned char *)dst + (size_t)y * (size_t)w * 4,
		       (unsigned char *)lr.pBits + (size_t)y * (size_t)lr.Pitch,
		       (size_t)w * 4);
	}

	IDirect3DSurface9_UnlockRect(dstSurf);
	IDirect3DSurface9_Release(dstSurf);
	return 1;
}

void SDL_XBOX_SetScreenResolution(int w, int h) {
	int i;
	if (w <= 0 || h <= 0) {
		/* "Auto": resolucion configurada en el dashboard del sistema. */
		XVIDEO_MODE vm;
		ZeroMemory(&vm, sizeof(vm));
		XGetVideoMode(&vm);
		w = (int)vm.dwDisplayWidth;
		h = (int)vm.dwDisplayHeight;
		/* Cap de rendimiento: maximo 1280x720 (el scaler HW sube a la pantalla). */
		if (w > 1280 && h > 720) { w = 1280; h = 720; }
	}
	for (i = 0; vid_modes[i] != NULL; i++) {
		if (vid_modes[i]->w == w && vid_modes[i]->h == h) {
			g_screen_resolution = i;
			return;
		}
	}
	/* No esta en la allow-list -> 1280x720 (indice 0). */
	g_screen_resolution = 0;
}

void SDL_XBOX_GetScreenResolution(int *w, int *h) {
	const SDL_Rect *m = vid_modes[g_screen_resolution >= 0 ? g_screen_resolution : 0];
	if (w) *w = m->w;
	if (h) *h = m->h;
}

/* Tasa de refresco REAL del panel, en Hz.  0 = desconocida.
 *
 * POR QUE HACE FALTA.  El frontend derivaba su periodo objetivo del fps que
 * DECLARA el core (`av_info.timing.fps`), que para el N64 son ~60,13 Hz, y
 * presentaba contra un panel que puede ir a 59,94 (NTSC) o 50,00 (PAL).  Dos
 * rejillas con periodos distintos derivan una contra otra -a 60,00 frente a
 * 59,94 son ~1 ms cada 60 frames, con un batido de ~17 s-, y esa deriva es lo
 * que se estaba midiendo como "la mitad de los frames llegan tarde" con el
 * emulador funcionando a pleno rendimiento.
 *
 * El dato ya estaba a mano y se tiraba: `XGetVideoMode` se llama en
 * `SDL_XBOX_SetScreenResolution` y solo se usaban `dwDisplayWidth/Height`.
 *
 * Se consulta al vuelo en vez de cachearlo porque el usuario puede cambiar la
 * salida de video desde el panel del sistema, y la llamada es barata (no toca
 * D3D). */
float SDL_XBOX_GetRefreshRate(void) {
	XVIDEO_MODE vm;
	ZeroMemory(&vm, sizeof(vm));
	XGetVideoMode(&vm);
	return vm.RefreshRate;
}

/* Forward decl for XBOX_UpdateOverlayVertices, defined below */
static void XBOX_UpdateOverlayVertices(void);

/* Vsync EN CALIENTE, sin Reset del device.
 *
 * POR QUE NO VALE SDL_XBOX_SetVSync PARA ESTO.  Esa funcion cambia
 * D3D_PP.PresentationInterval y llama a IDirect3DDevice9_Reset, y un Reset
 * invalida TODOS los recursos D3DPOOL_DEFAULT del device.  El device es uno
 * solo y compartido: los cores libretro que renderizan por hardware (Xenon64
 * dibuja a una textura D3D propia) tienen sus recursos ahi y NO se enteran del
 * Reset, asi que se quedan con punteros muertos.  Sintoma medido: pantalla
 * completamente negra en cuanto el vsync conmuta.
 *
 * En Xbox 360 el intervalo de presentacion es ademas un RENDER STATE
 * (D3DRS_PRESENTINTERVAL, d3d9types.h:624), asi que se cambia con
 * SetRenderState y no hace falta Reset ninguno.  Es barato, o sea que se puede
 * llamar cada frame -y conviene: si algun core o plugin pisa el estado, la
 * llamada del frame siguiente lo corrige sola-.
 *
 * Nota: NO se toca D3D_PP.PresentationInterval a proposito.  Ese es el valor
 * base del usuario y es lo que debe quedar si alguien hace un Reset de verdad
 * (cambio de resolucion); el render state se reaplica solo al frame siguiente. */
void SDL_XBOX_SetPresentInterval(int enabled)
{
	if (!D3D_Device) return;

	/* Misma seccion critica que usa SDL_XBOX_SetVSync y el propio flip: D3D9 de
	 * Xbox 360 no es seguro entre hilos, y el hilo grafico del core libretro
	 * emite comandos sobre este mismo device. */
	if (g_xboxFlipCSInit) EnterCriticalSection(&g_xboxFlipCS);

	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_PRESENTINTERVAL,
		enabled ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE);

	if (g_xboxFlipCSInit) LeaveCriticalSection(&g_xboxFlipCS);
}

/* "Video adaptativo" (1) o "Video" normal (0): D3DRS_PRESENTIMMEDIATETHRESHOLD a
 * 80 (ver XBOX_VideoInit: un Present que llega tarde se muestra ya, con
 * tearing, en vez de esperar al vblank siguiente) o al valor de fabrica que
 * se leyo al crear el device.  Mismo render state sin Reset y misma seccion
 * critica que SDL_XBOX_SetPresentInterval.  No toca el intervalo. */
void SDL_XBOX_SetAdaptiveVSync(int enabled)
{
	if (!D3D_Device || !g_umbralInmediatoLeido) return;

	if (g_xboxFlipCSInit) EnterCriticalSection(&g_xboxFlipCS);

	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_PRESENTIMMEDIATETHRESHOLD,
		enabled ? 80 : g_umbralInmediatoFabrica);

	if (g_xboxFlipCSInit) LeaveCriticalSection(&g_xboxFlipCS);
}

void SDL_XBOX_SetVSync(int enabled)
{
	SDL_VideoDevice *this = current_video;
	D3DLOCKED_RECT d3dlr;
	void *pLocked;
	float bbw, bbh;

	if (!D3D_Device) return;

	if (g_xboxFlipCSInit) EnterCriticalSection(&g_xboxFlipCS);

	/* Unlock game texture before Reset */
	if (this && this->hidden && this->hidden->SDL_primary)
		IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);

	/* Unlock overlay texture if locked */
	if (g_overlay_texture && g_overlay_locked) {
		IDirect3DTexture9_UnlockRect(g_overlay_texture, 0);
		g_overlay_locked = 0;
	}

	/* Release HLSL background resources ANTES del Reset.
	 * g_hlslBg_vb es D3DPOOL_DEFAULT y se destruye con el Reset;
	 * sin esto queda como dangling pointer. */
	//HLSLBackground_shutdown();

	/* Release D3DPOOL_DEFAULT resources (lost on Reset) */
	if (vertexBuffer) {
		IDirect3DVertexBuffer9_Release(vertexBuffer);
		vertexBuffer = NULL;
		have_vertexbuffer = 0;
	}
	if (g_overlay_vb) {
		IDirect3DVertexBuffer9_Release(g_overlay_vb);
		g_overlay_vb = NULL;
	}

	/* Modify PresentationInterval */
	D3D_PP.PresentationInterval = enabled ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

	/* Reset device with new presentation parameters */
	IDirect3DDevice9_Reset(D3D_Device, &D3D_PP);

	/* Recreate game vertex buffer (D3DPOOL_DEFAULT fue destruido) */
	IDirect3DDevice9_CreateVertexBuffer(D3D_Device, sizeof(triangleStripVertices),
		D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &vertexBuffer, NULL);
	have_vertexbuffer = 1;
	if (vertexBuffer) {
		IDirect3DVertexBuffer9_Lock(vertexBuffer, 0, 0, (BYTE **)&pLocked, 0L);
		memcpy(pLocked, triangleStripVertices, sizeof(triangleStripVertices));
		IDirect3DVertexBuffer9_Unlock(vertexBuffer);
	}

	/* Re-lock game texture (CPU-cached, sobrevive al Reset) */
	if (this && this->hidden && this->hidden->SDL_primary) {
		IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &d3dlr, NULL, 0);
		if (this->screen) {
			this->screen->pixels = d3dlr.pBits;
			this->screen->pitch = d3dlr.Pitch;
		}
	}

	/* Recrear overlay vertex buffer si hacía falta */
	if (g_overlay_texture) {
		IDirect3DDevice9_CreateVertexBuffer(D3D_Device,
			sizeof(VERTEX[4]), D3DUSAGE_WRITEONLY, 0,
			D3DPOOL_DEFAULT, &g_overlay_vb, NULL);

		if (g_overlay_vb)
			XBOX_UpdateOverlayVertices();

		/* Re-lock overlay texture */
		IDirect3DTexture9_LockRect(g_overlay_texture, 0, &d3dlr, NULL, 0);
		if (g_overlay_surface) {
			g_overlay_surface->pixels = d3dlr.pBits;
			g_overlay_surface->pitch = d3dlr.Pitch;
		}
		g_overlay_locked = 1;
	}

	/* Restaurar estado D3D perdido tras el Reset */
	IDirect3DDevice9_SetVertexShader(D3D_Device, g_pGradientVertexShader);
	IDirect3DDevice9_SetVertexDeclaration(D3D_Device, g_pGradientVertexDecl);
	D3DDevice_SetRenderState(D3D_Device, D3DRS_VIEWPORTENABLE, FALSE);

	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

	XBOX_RebindGameTexture(this);
	if (vertexBuffer)
		IDirect3DDevice9_SetStreamSource(D3D_Device, 0, vertexBuffer, 0, sizeof(VERTEX));

	XBOX_SelectEffect(g_current_effect);

	/* Re-iniciar el HLSL background tras el Reset */
	//HLSLBackground_init(D3D_Device);

	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);

	if (g_xboxFlipCSInit) LeaveCriticalSection(&g_xboxFlipCS);
}

/* ---- Overlay system ---- */

/* Recalcula los 4 vertices del overlay quad aplicando el overscan actual.
   Llamar cuando cambien g_overlay_overscan_x/y, o cuando se cree el VB. */
static void XBOX_UpdateOverlayVertices(void)
{
	VERTEX overlayVerts[4];
	float bbw = (float)D3D_PP.BackBufferWidth;
	float bbh = (float)D3D_PP.BackBufferHeight;
	float ox = (float)g_overlay_overscan_x;
	float oy = (float)g_overlay_overscan_y;
	void *pLocked;

	if (!g_overlay_vb) return;

	overlayVerts[0].x = -0.5f + ox;
	overlayVerts[0].y = bbh - 0.5f - oy;
	overlayVerts[0].z = 0; overlayVerts[0].rhw = 1;
	overlayVerts[0].tx = 0; overlayVerts[0].ty = 1;

	overlayVerts[1].x = -0.5f + ox;
	overlayVerts[1].y = -0.5f + oy;
	overlayVerts[1].z = 0; overlayVerts[1].rhw = 1;
	overlayVerts[1].tx = 0; overlayVerts[1].ty = 0;

	overlayVerts[2].x = bbw - 0.5f - ox;
	overlayVerts[2].y = bbh - 0.5f - oy;
	overlayVerts[2].z = 0; overlayVerts[2].rhw = 1;
	overlayVerts[2].tx = 1; overlayVerts[2].ty = 1;

	overlayVerts[3].x = bbw - 0.5f - ox;
	overlayVerts[3].y = -0.5f + oy;
	overlayVerts[3].z = 0; overlayVerts[3].rhw = 1;
	overlayVerts[3].tx = 1; overlayVerts[3].ty = 0;

	IDirect3DVertexBuffer9_Lock(g_overlay_vb, 0, 0, (BYTE **)&pLocked, 0L);
	memcpy(pLocked, overlayVerts, sizeof(overlayVerts));
	IDirect3DVertexBuffer9_Unlock(g_overlay_vb);
}

static void XBOX_InitOverlay(void)
{
	D3DLOCKED_RECT d3dlr;
	float bbw = (float)D3D_PP.BackBufferWidth;
	float bbh = (float)D3D_PP.BackBufferHeight;

	if (g_overlay_texture) return; /* Already initialized */

	/* Create ARGB texture at backbuffer resolution */
	IDirect3DDevice9_CreateTexture(D3D_Device,
		(int)bbw, (int)bbh, 1, 0,
		D3DFMT_LIN_A8R8G8B8, D3DUSAGE_CPU_CACHED_MEMORY,
		(D3DTexture**)&g_overlay_texture, NULL);

	if (!g_overlay_texture) return;

	/* Create fullscreen vertex buffer for the overlay quad */
	IDirect3DDevice9_CreateVertexBuffer(D3D_Device,
		sizeof(VERTEX[4]), D3DUSAGE_WRITEONLY, 0,
		D3DPOOL_DEFAULT, &g_overlay_vb, NULL);

	if (!g_overlay_vb) {
		IDirect3DTexture9_Release(g_overlay_texture);
		g_overlay_texture = NULL;
		return;
	}

	/* Fill vertex buffer with overscan applied */
	XBOX_UpdateOverlayVertices();

	/* Create SDL_Surface wrapper for the overlay */
	g_overlay_surface = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
		(int)bbw, (int)bbh, 32,
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

	if (!g_overlay_surface) {
		IDirect3DVertexBuffer9_Release(g_overlay_vb);
		IDirect3DTexture9_Release(g_overlay_texture);
		g_overlay_vb = NULL;
		g_overlay_texture = NULL;
		return;
	}

	/* Lock the overlay texture permanently, same pattern as the game texture */
	IDirect3DTexture9_LockRect(g_overlay_texture, 0, &d3dlr, NULL, 0);

	/* Free the pixel buffer allocated by SDL_CreateRGBSurface,
	   then point to texture memory instead */
	if (g_overlay_surface->pixels) {
		free(g_overlay_surface->pixels);
	}
	g_overlay_surface->pixels = d3dlr.pBits;
	g_overlay_surface->pitch = d3dlr.Pitch;
	g_overlay_surface->flags |= SDL_PREALLOC;
	g_overlay_locked = 1;

	/* Clear to fully transparent */
	memset(g_overlay_surface->pixels, 0, d3dlr.Pitch * (int)bbh);
}

static void XBOX_DestroyOverlay(void)
{
	if (g_overlay_surface) {
		g_overlay_surface->pixels = NULL;
		SDL_FreeSurface(g_overlay_surface);
		g_overlay_surface = NULL;
	}
	if (g_overlay_texture) {
		if (g_overlay_locked)
			IDirect3DTexture9_UnlockRect(g_overlay_texture, 0);
		IDirect3DTexture9_Release(g_overlay_texture);
		g_overlay_texture = NULL;
		g_overlay_locked = 0;
	}
	if (g_overlay_vb) {
		IDirect3DVertexBuffer9_Release(g_overlay_vb);
		g_overlay_vb = NULL;
	}
	g_overlay_enabled = 0;
}

/* Draw the overlay quad with alpha blending (called during flip).
   game_texture is the primary D3D texture to restore after drawing.
   useAlphaFix: when true, use the alpha-fixup shader to force all non-black
   pixels opaque (avoids the CPU loop that sets alpha=0xFF per pixel). */
static void XBOX_DrawOverlay(LPDIRECT3DTEXTURE9 game_texture, int useAlphaFix)
{
	D3DLOCKED_RECT d3dlr;

	if (!g_overlay_enabled || !g_overlay_texture || !g_overlay_vb) return;

	/* Unlock overlay texture so GPU can read it */
	IDirect3DTexture9_UnlockRect(g_overlay_texture, 0);

	/* Enable alpha blending */
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_ALPHABLENDENABLE, TRUE);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)g_overlay_texture);
	IDirect3DDevice9_SetStreamSource(D3D_Device, 0, g_overlay_vb, 0, sizeof(VERTEX));
	IDirect3DDevice9_SetPixelShader(D3D_Device,
		(useAlphaFix && g_hlslBg_psAlphaFix) ? g_hlslBg_psAlphaFix : g_fallbackPS);
	IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	IDirect3DDevice9_DrawPrimitive(D3D_Device, D3DPT_TRIANGLESTRIP, 0, 2);

	/* Disable alpha blending */
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_ALPHABLENDENABLE, FALSE);

	/* Restore game texture, vertex buffer and active shader.
	 * NO llamamos a XBOX_SelectEffect aqui: solo necesitamos restaurar lo
	 * que el overlay ha tocado (shader del s0 y filtro MIN/MAG del s0).
	 * Los uniformes (textureDims en c1), la LUT del s1 (HQx/xBR) y los
	 * blend modes los deja como estaban ya que el overlay no los modifica.
	 * IMPORTANTE: esta invariante depende de que HLSLBackground_draw() (que
	 * corre justo ANTES del overlay) restaure c1 tras dibujarse; si no, c1
	 * quedaria con la resolucion del backbuffer y romperia el shader del juego.
	 * Asi nos ahorramos un switch enorme + SetTexture+SetPixelShaderConstantF
	 * por cada frame mientras el menu/HUD este activo. */
	IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)game_texture);
	IDirect3DDevice9_SetStreamSource(D3D_Device, 0, vertexBuffer, 0, sizeof(VERTEX));
	/* g_activePS es el puntero YA validado por XBOX_SelectEffect. Esto corre
	 * en cada frame con el menu/HUD activo, y en Xenon un pixel shader NULL es
	 * pantalla negra, asi que aqui no se indexa nunca por g_current_effect. */
	IDirect3DDevice9_SetPixelShader(D3D_Device, g_activePS ? g_activePS : g_fallbackPS);
	IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, g_current_sampler_filter);
	IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, g_current_sampler_filter);

	/* Re-lock overlay texture so the app can keep drawing to it */
	IDirect3DTexture9_LockRect(g_overlay_texture, 0, &d3dlr, NULL, 0);
	g_overlay_surface->pixels = d3dlr.pBits;
	g_overlay_surface->pitch = d3dlr.Pitch;
}


/* Rectangulo donde se dibuja la imagen del JUEGO, expresado en PIXELES DEL
 * OVERLAY.  Lo necesita quien dibuje sobre el overlay algo que tenga que
 * alinearse con el contenido del juego (la reticula del lightgun).
 *
 * Son dos quads distintos y hay que componer sus geometrias:
 *
 *  - El quad del JUEGO ocupa g_visible_rect en pixeles de backbuffer: el
 *    aspect del core lo deja pillarboxed (4:3 en 16:9 -> 960 de 1280) o no,
 *    segun lo que reporte el core (el hack de widescreen cambia esto).
 *  - El quad del OVERLAY cubre el backbuffer ENTERO menos el overscan, y NO
 *    aplica el aspect.  Asi que el pixel px del overlay aparece en pantalla en
 *    ox + px*(bbw - 2*ox)/bbw, y hay que invertir esa relacion.
 *
 * Sin esto, la reticula se reparte sobre toda la pantalla mientras el disparo
 * se reparte sobre la imagen: coinciden en el centro y se separan hacia los
 * bordes, cada uno hacia su lado. */
void SDL_XBOX_GetGameRectOnOverlay(int *x, int *y, int *w, int *h)
{
	float bbw = (float)D3D_PP.BackBufferWidth;
	float bbh = (float)D3D_PP.BackBufferHeight;
	float ox  = (float)g_overlay_overscan_x;
	float oy  = (float)g_overlay_overscan_y;
	float span_x = bbw - 2.0f * ox;
	float span_y = bbh - 2.0f * oy;
	float left = (float)g_visible_rect.left,  top    = (float)g_visible_rect.top;
	float right= (float)g_visible_rect.right, bottom = (float)g_visible_rect.bottom;

	/* Sin juego cargado el rect esta a cero: devolver el overlay entero. */
	if (right <= left || bottom <= top) {
		*x = 0; *y = 0; *w = (int)bbw; *h = (int)bbh;
		return;
	}

	if (span_x < 1.0f || span_y < 1.0f) {   /* overscan absurdo: sin inset */
		*x = (int)left;  *y = (int)top;
		*w = (int)(right - left);
		*h = (int)(bottom - top);
		return;
	}

	*x = (int)((left   - ox) * bbw / span_x);
	*y = (int)((top    - oy) * bbh / span_y);
	*w = (int)((right  - left) * bbw / span_x);
	*h = (int)((bottom - top ) * bbh / span_y);
}

SDL_Surface* SDL_XBOX_GetOverlay(void)
{
	if (!g_overlay_surface)
		XBOX_InitOverlay();
	return g_overlay_surface;
}

void SDL_XBOX_SetOverlayEnabled(int enabled)
{
	if (enabled && !g_overlay_surface)
		XBOX_InitOverlay();
	g_overlay_enabled = enabled;
}

/* Ajusta el overscan del overlay.
   x,y > 0 desplazan los bordes hacia adentro (reduce area visible).
   x,y < 0 los expanden hacia afuera.  Se puede llamar en cualquier
   momento sin reiniciar el overlay. */
void SDL_XBOX_SetOverscan(int x, int y)
{
	g_overlay_overscan_x = x;
	g_overlay_overscan_y = y;
	XBOX_UpdateOverlayVertices();
}

SDL_Surface *XBOX_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{

	int pixel_mode,pitch;
	Uint32 Rmask, Gmask, Bmask;
	D3DLOCKED_RECT d3dlr;

	HRESULT ret;

	/* Cleanup previous D3D texture if re-setting video mode */
	if (this->hidden->SDL_primary) {
		IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);
		IDirect3DDevice9_SetTexture(D3D_Device, 0, NULL);
		IDirect3DTexture9_Release(this->hidden->SDL_primary);
		this->hidden->SDL_primary = NULL;
		current->pixels = NULL;
	}

	switch(bpp)
	{
		case 8:
			bpp = 16;
			pitch = width*2;
			pixel_mode = D3DFMT_LIN_R5G6B5;
		case 16:
			pitch = width*2;
			Rmask = 0x0000f800;
			Gmask = 0x000007e0;
			Bmask = 0x0000001f;
			pixel_mode = D3DFMT_LIN_R5G6B5;
			break;
		case 24:
			pitch = width*4;
			bpp = 32;
			pixel_mode = D3DFMT_LIN_X8R8G8B8;
		case 32:
			pitch = width*4;
			pixel_mode = D3DFMT_LIN_X8R8G8B8;
			Rmask = 0x00FF0000;
			Gmask = 0x0000FF00;
			Bmask = 0x000000FF;
			break;
		default:
			SDL_SetError("Couldn't find requested mode in list");
			return(NULL);
	}

	/* Allocate the new pixel format for the screen */
	if ( ! SDL_ReallocFormat(current, bpp, Rmask, Gmask, Bmask, 0) ) {
		SDL_SetError("Couldn't allocate new pixel format for requested mode");
		return(NULL);
	}

 
	ret = IDirect3DDevice9_CreateTexture(D3D_Device,width,height,1, 0,pixel_mode, D3DUSAGE_CPU_CACHED_MEMORY, (D3DTexture**)&this->hidden->SDL_primary, NULL);
	have_direct3dtexture=1;

	if (ret != D3D_OK)
	{
		SDL_SetError("Couldn't create Direct3D Texture!");
		return(NULL);
	}

	/* Lock the texture permanently - app draws directly to texture memory.
	   Unlock only briefly during flip for GPU to render, then re-lock. */
	ret = IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &d3dlr, NULL, 0);
	if (ret != D3D_OK) {
		SDL_SetError("Couldn't lock Direct3D Texture!");
		return(NULL);
	}


    initShaders();
   	
	 
	    // Create vertex declaration
    if( NULL == g_pGradientVertexDecl )
    {
        // Nota: Para SM 3.0 es mejor asegurar que POSITION y TEXCOORD coincidan con el struct VS_IN
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        IDirect3DDevice9_CreateVertexDeclaration(D3D_Device, decl, &g_pGradientVertexDecl);
            
    }
 
    if( NULL == g_pGradientVertexShader )
    {
        ID3DXBuffer* pShaderCode;
        if( SUCCEEDED( D3DXCompileShader( g_strGradientShader, (UINT)strlen( g_strGradientShader ),
                                       NULL, NULL, "GradientVertexShader", 
                                       "vs_2_0", 0, 
                                       &pShaderCode, NULL, NULL ) ) )
        {
            IDirect3DDevice9_CreateVertexShader(D3D_Device, (DWORD*)pShaderCode->lpVtbl->GetBufferPointer(pShaderCode), &g_pGradientVertexShader);
        }
    }
 	 
// 4. Correccion de llamadas a la API (Usar los metodos del Device correctamente)
    /* Release previous vertex buffer if re-setting video mode */
    if (vertexBuffer != NULL) {
        IDirect3DVertexBuffer9_Release(vertexBuffer);
        vertexBuffer = NULL;
    }
    IDirect3DDevice9_CreateVertexBuffer(D3D_Device, sizeof(triangleStripVertices), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &vertexBuffer, NULL );

    IDirect3DDevice9_SetVertexShader(D3D_Device, g_pGradientVertexShader );
    IDirect3DDevice9_SetPixelShader(D3D_Device, g_pPixelShader);
    IDirect3DDevice9_SetVertexDeclaration(D3D_Device, g_pGradientVertexDecl);

	D3DDevice_SetRenderState(D3D_Device, D3DRS_VIEWPORTENABLE, FALSE);

	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
 
  
	/* Calculate aspect-ratio-correct quad and update vertex buffer.
	   Use the stored aspect ratio if one was set, otherwise native pixel ratio. */
	XBOX_UpdateVertexBuffer(width, height, g_display_aspect_ratio);

	/* Store texture dimensions for pixel shaders that need them (e.g. Scale2x) */
	g_texture_width = width;
	g_texture_height = height;

	XBOX_RebindGameTexture(this);
	IDirect3DDevice9_SetStreamSource(D3D_Device, 0, vertexBuffer, 0, sizeof(VERTEX));

	/* Aplicar el efecto activo: deja shader + sampler + constants en el
	 * estado correcto desde el primer frame.  El frontend solo invoca
	 * XBOX_SelectEffect cuando el filtro CAMBIA respecto al previo, asi que
	 * si la app arranca con su default y g_current_effect ya estaba a ese
	 * valor (0 = Nearest), no entraria nunca y se quedaria el shader
	 * pasante + sampler LINEAR puestos justo arriba (imagen blurry).
	 * Tiene que llamarse despues de fijar g_texture_width/height — el
	 * shader lee esas dims via SetPixelShaderConstantF. */
	XBOX_SelectEffect(g_current_effect);

	have_vertexbuffer=1;


	/* Set up the new mode framebuffer.
	   SDL_SWSURFACE|SDL_PREALLOC: prevents shadow surface creation and
	   prevents SDL_FreeSurface from calling free() on our texture pointer. */
	current->flags = (SDL_FULLSCREEN|SDL_SWSURFACE|SDL_PREALLOC);

	if (flags & SDL_DOUBLEBUF)
		current->flags |= SDL_DOUBLEBUF;
	if (flags & SDL_HWPALETTE)
		current->flags |= SDL_HWPALETTE;

	current->w = width;
	current->h = height;
	current->pitch = d3dlr.Pitch;
	current->pixels = d3dlr.pBits;

	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
	IDirect3DDevice9_Present(D3D_Device,NULL,NULL,NULL,NULL);

	SDL_XBOX_SetScreenResolution(width, height);
 
	/* We're done */
	return(current);
}

/* =====================================================================
 * XBOX_ResizeGameTexture - Redimensiona la textura del juego en caliente
 * SIN tocar el modo de video ni el backbuffer.
 *
 * A diferencia de SDL_SetVideoMode (que libera y recrea shaders, vertex
 * buffers, declaraciones y hace un Clear+Present forzado produciendo un
 * flash negro), esta funcion solo cambia el tamano de la textura donde
 * el core escribe los frames.  Los shaders, el vertex buffer (quad
 * fullscreen) y el resto del pipeline D3D permanecen intactos.
 *
 * Equivalente funcional a WinD3D9_SetGameMode en la rama Windows.
 * Llamar desde hw_refresh cuando cambia la resolucion del core.
 *
 * Parametros:
 *   width, height  - nueva resolucion nativa del core
 *   bpp            - 16 (RGB565) o 32 (X8R8G8B8)
 * Retorno:
 *   SDL_Surface*   - el surface del gameScreen con pixels apuntando
 *                    a la textura D3D lockeada permanentemente, o NULL
 *                    si falla la creacion de la textura.
 * =================================================================== */
SDL_Surface* XBOX_ResizeGameTexture(int width, int height, int bpp)
{
	SDL_VideoDevice *this = current_video;
	D3DLOCKED_RECT d3dlr;
	int pixel_mode, pitch_bpp;
	Uint32 Rmask, Gmask, Bmask;
	HRESULT ret;

	if (!this || !this->hidden) return NULL;

	switch (bpp) {
		case 8:
		case 16:
			pixel_mode = D3DFMT_LIN_R5G6B5; pitch_bpp = 16;
			Rmask = 0x0000F800; Gmask = 0x000007E0; Bmask = 0x0000001F;
			break;
		case 24:
		case 32:
			pixel_mode = D3DFMT_LIN_X8R8G8B8; pitch_bpp = 32;
			Rmask = 0x00FF0000; Gmask = 0x0000FF00; Bmask = 0x000000FF;
			break;
		default:
			return NULL;
	}

	/* Liberar textura D3D anterior */
	if (this->hidden->SDL_primary) {
		IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);
		IDirect3DDevice9_SetTexture(D3D_Device, 0, NULL);
		IDirect3DTexture9_Release(this->hidden->SDL_primary);
		this->hidden->SDL_primary = NULL;
	}

	/* Crear textura D3D */
	ret = IDirect3DDevice9_CreateTexture(D3D_Device, width, height, 1, 0,
		pixel_mode, D3DUSAGE_CPU_CACHED_MEMORY,
		(D3DTexture**)&this->hidden->SDL_primary, NULL);
	if (ret != D3D_OK) { this->hidden->SDL_primary = NULL; return NULL; }
	have_direct3dtexture = 1;

	/* Lock permanente: core escribe directo a VRAM (zero-copy) */
	ret = IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &d3dlr, NULL, 0);
	if (ret != D3D_OK) {
		IDirect3DTexture9_Release(this->hidden->SDL_primary);
		this->hidden->SDL_primary = NULL;
		return NULL;
	}

	/* Liberar el surface SDL anterior (si existe) y crear uno nuevo
	 * apuntando a la textura D3D lockeada, igual que XBOX_SetVideoMode.
	 * NO reusamos SDL_ReallocFormat: crear surface nuevo evita
	 * problemas de coherencia con el format del overlay. */
	if (this->screen) {
		/* Los pixels apuntan a la textura D3D anterior (ya liberada),
		 * asi que SDL no debe free() el buffer. */
		this->screen->pixels = NULL;
		SDL_FreeSurface(this->screen);
	}
	this->screen = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_PREALLOC,
		width, height, pitch_bpp, Rmask, Gmask, Bmask, 0);
	if (!this->screen) {
		IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);
		IDirect3DTexture9_Release(this->hidden->SDL_primary);
		this->hidden->SDL_primary = NULL;
		return NULL;
	}
	/* Reemplazar el buffer interno por la textura D3D lockeada */
	if (this->screen->pixels) free(this->screen->pixels);
	this->screen->pixels = d3dlr.pBits;
	this->screen->pitch = d3dlr.Pitch;
	this->screen->flags |= SDL_PREALLOC;

	/* Dimensiones globales + quad */
	g_texture_width = width;
	g_texture_height = height;
	XBOX_UpdateVertexBuffer(width, height, g_display_aspect_ratio);

	/* D3D state minimo (equivalente a WinD3D9_SetGameMode) */
	XBOX_RebindGameTexture(this);
	IDirect3DDevice9_SetStreamSource(D3D_Device, 0, vertexBuffer, 0, sizeof(VERTEX));
	XBOX_SelectEffect(g_current_effect);

	/* Sincronizacion GPU: Clear+Present ligero.
	 * XBOX_SetVideoMode termina con Clear+Present para forzar coherencia
	 * de cache.  El Clear pinta el backbuffer a negro temporalmente
	 * (el unico flash que queda, inevitable).  Sin el, la GPU ve datos
	 * stale del overlay aunque la CPU tenga alpha=0xFF. */
	//IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
	IDirect3DDevice9_Present(D3D_Device, NULL, NULL, NULL, NULL);

	return this->screen;
}

/* We don't actually allow hardware surfaces other than the main one */

static int XBOX_AllocHWSurface(_THIS, SDL_Surface *surface)
{

	return(-1);
}
static void XBOX_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

/* >>> ESTABLECER NUESTRO PROPIO PIPELINE ANTES DE DIBUJAR.
 *
 * Hasta ahora este estado se fijaba UNA SOLA VEZ en XBOX_SetVideoMode
 * (:1445-1453) y se daba por sentado que nadie mas tocaba el device.  Eso
 * valia mientras TODOS los cores entregaban un buffer de CPU.
 *
 * Deja de valer con un core que renderiza el mismo con Direct3D (Xenon64):
 * comparte nuestro device y deja puestos SU vertex shader, SU declaracion de
 * vertices, su Z y su viewport.  Con la declaracion de otro ligada, nuestro
 * triangulo a pantalla completa se interpreta con el layout equivocado y se
 * transforma con la matriz del core: **no se ve NI el juego NI el menu**.
 * Sintoma exacto: pantalla negra con el core corriendo y sonando.
 *
 * Son ~8 llamadas por frame, irrelevantes, y para los cores que no tocan D3D
 * fijan los mismos valores que ya tenian: no cambian nada.  Lo que se gana es
 * que el frontend deje de depender de la buena voluntad del core. */
static void XBOX_SetupQuadState(void)
{
	if (!D3D_Device) return;

	/* Pipeline de vertices: nuestro shader, nuestra declaracion y nuestro
	 * vertex buffer (XBOX_DrawMainQuad hace DrawPrimitive a secas y no pone
	 * el stream source). */
	IDirect3DDevice9_SetVertexShader(D3D_Device, g_pGradientVertexShader);
	IDirect3DDevice9_SetVertexDeclaration(D3D_Device, g_pGradientVertexDecl);
	if (vertexBuffer)
		IDirect3DDevice9_SetStreamSource(D3D_Device, 0, vertexBuffer, 0, sizeof(VERTEX));

	/* Nuestro quad va en coordenadas YA transformadas: el viewport no debe
	 * aplicarse.  Un core 3D lo deja a TRUE con su propio viewport. */
	D3DDevice_SetRenderState(D3D_Device, D3DRS_VIEWPORTENABLE, FALSE);

	/* >>> Y EL VIEWPORT AL TAMANO DEL BACK BUFFER, aunque este desactivado.
	 *
	 * No es redundante: HLSLBackground_draw NO usa el viewport para
	 * transformar -sus vertices van en pixeles con rhw=1- sino que lo LEE con
	 * GetViewport() para saber de que tamano hacer su quad
	 * (`w = vp.Width; h = vp.Height`).  Un core 3D deja aqui SU resolucion
	 * interna -640x480 en el caso del N64-, y entonces el fondo animado del
	 * menu se dibuja de ese tamano en una pantalla de 1280x720, o sea en un
	 * cuadrante.  Ese era el sintoma de "el fondo del overlay deja de
	 * funcionar". */
	{
		D3DVIEWPORT9 vp;
		vp.X = 0;
		vp.Y = 0;
		vp.Width  = D3D_PP.BackBufferWidth;
		vp.Height = D3D_PP.BackBufferHeight;
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
		IDirect3DDevice9_SetViewport(D3D_Device, &vp);
	}

	/* Sin profundidad.  Y no es cosmetico: aqui no hay depth-stencil ligado,
	 * y dibujar con ZENABLE puesto y sin superficie de Z hace que el Xenos
	 * descarte el draw entero. */
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_ZENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_ZWRITEENABLE, FALSE);

	/* El scissor lo pone XBOX_DrawMainQuad para su rect visible; aqui se
	 * apaga para que el Clear de abajo borre el back buffer ENTERO y no el
	 * recorte que dejara el core (320x240 del N64). */
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_SCISSORTESTENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_ALPHABLENDENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_ALPHATESTENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_CULLMODE, D3DCULL_NONE);

	/* Shader de pixeles y filtros del s0: los mismos que restaura
	 * XBOX_DrawOverlay al terminar. */
	IDirect3DDevice9_SetPixelShader(D3D_Device, g_activePS ? g_activePS : g_fallbackPS);
	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, g_current_sampler_filter);
	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, g_current_sampler_filter);

	/* >>> Y CON UN CORE QUE COMPARTE EL DEVICE, LA ETAPA DE SHADER ENTERA.
	 *
	 * Lo de arriba no basta porque el core no solo cambia shaders: cambia los
	 * REGISTROS y los SAMPLERS que nuestros presets dan por suyos.
	 *
	 *  - ★ Rice hace `SetPixelShaderConstantF(0, consts, 8)`
	 *    (d3d9_backend.cpp:6157), o sea que escribe c0..c7 con las constantes
	 *    de su combinador, MILES DE VECES POR SEGUNDO.  Y nuestros shaders
	 *    declaran `float2 textureDims : register(c1)`.  Con c1 pisado por el
	 *    1,0 del combinador, HQ2X/xBR calculan sus desplazamientos como
	 *    1/(1,1) = 1,0 en UV y muestrean la esquina opuesta de la textura: el
	 *    efecto "no se aplica".  Es el mismo fallo que ya tuvo HLSLBackground
	 *    pisando c1, y por eso ese modulo lo restaura al terminar.
	 *  - Rice usa hasta 8 unidades de textura, asi que s1..s3 -donde van
	 *    nuestras LUT- quedan con texturas suyas.
	 *
	 * XBOX_SelectEffect es justo la re-establecida completa: shader, c1 desde
	 * g_texture_width/height, filtro y wrap de s0, y desligar s1..sN para
	 * religar las LUT del preset.  Se escribio pensando en correr por CAMBIO
	 * de efecto -su propio comentario lo dice- y esa premisa deja de valer en
	 * cuanto otro cliente toca el device.  Cuesta un switch y unas pocas
	 * llamadas por frame: nada.
	 *
	 * Condicionado a que HAYA textura externa a proposito: para los cores que
	 * entregan un buffer de CPU nadie pisa nada, asi que se deja su camino
	 * exactamente como estaba y este cambio no puede causarles regresion. */
	if (g_extGameTex)
		XBOX_SelectEffect(g_current_effect);
}

static int XBOX_RenderSurface(_THIS, SDL_Surface *surface)
{
	D3DLOCKED_RECT d3dlr;
	int hlslWasActive;

	/* [XBOX360] Serializar todo el bloque de rendering+Present con un
	 * lock para que main thread y watcher thread no corrompan el ring
	 * buffer del GPU si coinciden.  Coste: ~us por frame, despreciable. */
	if (g_xboxFlipCSInit) EnterCriticalSection(&g_xboxFlipCS);

	/* Unlock so the GPU can read the texture for rendering.
	 *
	 * Con textura EXTERNA no hay nada que desbloquear: la imagen la ha dejado
	 * el core en una textura de la GPU y nuestra SDL_primary no participa.
	 * Bloquearla y desbloquearla aqui seria trabajo tirado, y ademas dejaria
	 * `surface->pixels` cambiando de sitio sin que nadie escriba ahi. */
	if (!g_extGameTex)
		IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);

	/* Nuestro pipeline, no el que haya dejado el core.  ANTES del Clear, para
	 * que el scissor apagado le deje borrar el back buffer entero. */
	XBOX_SetupQuadState();

	/* Clear for letterbox/pillarbox bars, then render game quad */
	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
	if (g_extGameTex)
		IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)g_extGameTex);
	XBOX_DrawMainQuad();

	/* Draw HLSL background after game quad (as menu background) */
	if (g_hlslBkg_active) HLSLBackground_draw(D3D_Device);

	/* Draw overlay on top if enabled.
	 * Pass g_hlslBkg_active so DrawOverlay can use the alpha-fixup shader
	 * when the HLSL background is visible.
	 * g_hlslBkg_active es estado RETENIDO (lo fija el frontend en las
	 * transiciones de estado / arranque / callback del menu); ya no se
	 * resetea por-frame aqui. */
	hlslWasActive = g_hlslBkg_active;
	/* La textura que el overlay tiene que RESTAURAR en s0 al terminar es la
	 * que esta alimentando el quad del juego, o sea la externa si la hay. */
	XBOX_DrawOverlay(g_extGameTex ? g_extGameTex : this->hidden->SDL_primary,
	                 hlslWasActive);

	IDirect3DDevice9_Present(D3D_Device, NULL, NULL, NULL, NULL);

	/* Re-lock so the app can keep drawing directly to texture memory */
	if (!g_extGameTex)
	{
		IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &d3dlr, NULL, 0);

		surface->pixels = d3dlr.pBits;
		surface->pitch = d3dlr.Pitch;
	}

	if (g_xboxFlipCSInit) LeaveCriticalSection(&g_xboxFlipCS);

	return 0;
}

static int XBOX_FillHWRect(_THIS, SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color)
{
	HRESULT ret;
	ret = IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, color, 1.0f, 0L);

	if (ret == D3D_OK)
		return (1);
	else
		return (0);
}


static int XBOX_HWAccelBlit(SDL_Surface *src, SDL_Rect *srcrect,
					SDL_Surface *dst, SDL_Rect *dstrect)
{
	return(1);
}

static int XBOX_CheckHWBlit(_THIS, SDL_Surface *src, SDL_Surface *dst)
{
	return(0);
}

/* We need to wait for vertical retrace on page flipped displays */
static int XBOX_LockHWSurface(_THIS, SDL_Surface *surface)
{
	HRESULT ret;
	D3DLOCKED_RECT d3dlr;

	if (!this->hidden->SDL_primary)
		return (-1);

	ret = IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &d3dlr, NULL, 0);

	surface->pitch = d3dlr.Pitch;
	surface->pixels = d3dlr.pBits;

	if (ret == D3D_OK)
		return(0);
	else
		return(-1);
}

static void XBOX_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary,0);

	return;
}

static void XBOX_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
	D3DLOCKED_RECT d3dlr;

	if (!this->hidden->SDL_primary || !have_vertexbuffer)
		return;

	/* [XBOX360] Mismo lock que XBOX_RenderSurface — el watcher podria
	 * llamar SDL_UpdateRect indirectamente, race con main thread. */
	if (g_xboxFlipCSInit) EnterCriticalSection(&g_xboxFlipCS);

	/* Unlock so the GPU can read the texture for rendering.
	 * Con textura externa no participa la nuestra; ver XBOX_RenderSurface. */
	if (!g_extGameTex)
		IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);

	XBOX_SetupQuadState();   /* ver XBOX_RenderSurface */

	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
	if (g_extGameTex)
		IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)g_extGameTex);
	XBOX_DrawMainQuad();
	if (g_hlslBkg_active) HLSLBackground_draw(D3D_Device);
	/* g_hlslBkg_active es estado retenido; ya no se resetea por-frame. */
	XBOX_DrawOverlay(g_extGameTex ? g_extGameTex : this->hidden->SDL_primary, 0);

	IDirect3DDevice9_Present(D3D_Device, NULL, NULL, NULL, NULL);

	/* Re-lock so the app can keep drawing directly to texture memory */
	if (!g_extGameTex)
	{
		IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &d3dlr, NULL, 0);

		this->screen->pixels = d3dlr.pBits;
		this->screen->pitch = d3dlr.Pitch;
	}

	if (g_xboxFlipCSInit) LeaveCriticalSection(&g_xboxFlipCS);
}

int XBOX_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	return(1);
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void XBOX_VideoQuit(_THIS)
{
#ifdef SDL_XBOX_HIDMOUSE
	 /* Retira los hooks de kernel del raton USB HID y libera los ratones aun
	  * conectados. OBLIGATORIO antes de que el .xex se descargue: si no, el
	  * kernel saltaria a memoria liberada al conectar/desconectar cualquier
	  * dispositivo USB. Idempotente. */
	 XBOX_HIDMouse_Quit();
#endif
	 HLSLBackground_shutdown();
	 XBOX_DestroyOverlay();
	 if (this->hidden->SDL_primary)
	 {
		 IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);
		 IDirect3DDevice9_SetTexture(D3D_Device, 0, NULL);
		 IDirect3DDevice9_SetStreamSource(D3D_Device, 0, NULL, 0, 0);
		 IDirect3DTexture9_Release(this->hidden->SDL_primary);
		 this->hidden->SDL_primary = NULL;
	 }

	 if (this->screen)
		 this->screen->pixels = NULL;

	 destroyShaders();
}

static int XBOX_SetHWAlpha(_THIS, SDL_Surface *surface, Uint8 alpha)
{
	return(1);
}

static int XBOX_SetHWColorKey(_THIS, SDL_Surface *surface, Uint32 key)
{
	
	return(0);
}

static int XBOX_SetFlickerFilter(_THIS, SDL_Surface *surface, int filter)
{
 
	return(0);
}

static int XBOX_SetSoftDisplayFilter(_THIS, SDL_Surface *surface, int enabled)
{
 
	return(0);
}


static LPDIRECT3DTEXTURE9 CreateYUVSurface(_THIS, int width, int height, Uint32 format)
{
    LPDIRECT3DTEXTURE9 surface;

	IDirect3DDevice9_CreateTexture(D3D_Device,width,height,1, 0,D3DFMT_LIN_UYVY, D3DUSAGE_CPU_CACHED_MEMORY, (D3DTexture**)&surface, NULL);

	return surface;
}
 
SDL_Overlay *XBOX_CreateYUVOverlay(_THIS, int width, int height, Uint32 format, SDL_Surface *display)
{
	SDL_Overlay *overlay;
	struct private_yuvhwdata *hwdata;

	if (format == SDL_YV12_OVERLAY || format == SDL_IYUV_OVERLAY)
		return NULL;

	/* Create the overlay structure */
	overlay = (SDL_Overlay *)malloc(sizeof *overlay);
	if ( overlay == NULL ) {
		SDL_OutOfMemory();
		return(NULL);
	}
	memset(overlay, 0, (sizeof *overlay));

	/* Fill in the basic members */
	overlay->format = format;
	overlay->w = width;
	overlay->h = height;

	/* Set up the YUV surface function structure */
	overlay->hwfuncs = &XBOX_yuvfuncs;

	/* Create the pixel data and lookup tables */
	hwdata = (struct private_yuvhwdata *)malloc(sizeof *hwdata);
	overlay->hwdata = hwdata;
	if ( hwdata == NULL ) {
		SDL_OutOfMemory();
		SDL_FreeYUVOverlay(overlay);
		return(NULL);
	}
	hwdata->surface = CreateYUVSurface(this, width, height, format);
	if ( hwdata->surface == NULL ) {
		SDL_FreeYUVOverlay(overlay);
		return(NULL);
	}
	overlay->hw_overlay = 1;

	/* Set up the plane pointers */
	overlay->pitches = hwdata->pitches;
	overlay->pixels = hwdata->planes;
	switch (format) {
		case SDL_YV12_OVERLAY:
		case SDL_IYUV_OVERLAY:
		overlay->planes = 3;
		break;
		default:
		overlay->planes = 1;
		break;
	}

	/* We're all done.. */
	return(overlay);
 
}

int XBOX_DisplayYUVOverlay(_THIS, SDL_Overlay *overlay, SDL_Rect *src, SDL_Rect *dst)
{

	// this is slow. need to optimize
	
	LPDIRECT3DTEXTURE9 surface;
	D3DLOCKED_RECT destSurface;
	D3DLOCKED_RECT srcSurface;
	XGTEXTURE_DESC descSrc;
	XGTEXTURE_DESC descDst;
    
	RECT srcrect, dstrect;

	POINT dstPoint =
    {
        0, 0
    };
		
	surface = overlay->hwdata->surface;
	srcrect.top = src->y;
	srcrect.bottom = srcrect.top+src->h;
	srcrect.left = src->x;
	srcrect.right = srcrect.left+src->w;
	dstrect.top = dst->y;
	dstrect.left = dst->x;
	dstrect.bottom = dstrect.top+dst->h;
	dstrect.right = dstrect.left+dst->w;
 
    // Copy tiled texture to a linear texture
   
	IDirect3DTexture9_LockRect(surface, 0, &srcSurface, &srcrect, D3DLOCK_READONLY);
	IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &destSurface, NULL, D3DLOCK_NOOVERWRITE);
     
	XGGetTextureDesc( (D3DBaseTexture *)this->hidden->SDL_primary, 0, &descDst );
	XGGetTextureDesc( (D3DBaseTexture *)surface, 0, &descSrc );
	XGCopySurface( destSurface.pBits, destSurface.Pitch, dst->w, dst->h, descDst.Format, NULL,
                   srcSurface.pBits, srcSurface.Pitch, descSrc.Format, NULL, XGCOMPRESS_YUV_SOURCE , 0 );


    IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);
	IDirect3DTexture9_UnlockRect(surface, 0);

	XBOX_DrawMainQuad();
	IDirect3DDevice9_Present(D3D_Device,NULL,NULL,NULL,NULL);

	return 0;
}
int XBOX_LockYUVOverlay(_THIS, SDL_Overlay *overlay)
{	
 	LPDIRECT3DTEXTURE9 surface;
	D3DLOCKED_RECT rect;
 	
	surface = overlay->hwdata->surface;
		
	IDirect3DTexture9_LockRect(surface, 0, &rect, NULL, 0);

	/* Find the pitch and offset values for the overlay */
	overlay->pitches[0] = (Uint16)rect.Pitch;
	overlay->pixels[0]  = (Uint8 *)rect.pBits;
	switch (overlay->format) {
	    case SDL_YV12_OVERLAY:
	    case SDL_IYUV_OVERLAY:
		/* Add the two extra planes */
        overlay->pitches[0] = overlay->w;
		overlay->pitches[1] = overlay->pitches[0] / 2;
		overlay->pitches[2] = overlay->pitches[0] / 2;

		overlay->pixels[0] = (Uint8 *)rect.pBits;
	        overlay->pixels[1] = overlay->pixels[0] +
		                     overlay->pitches[0] * overlay->h;
	        overlay->pixels[2] = overlay->pixels[1] +
		                     overlay->pitches[1] * overlay->h / 2;

			overlay->planes = 3;
	        break;
	    default:
		/* Only one plane, no worries */
	break;
	}

	return 0;

}

void XBOX_UnlockYUVOverlay(_THIS, SDL_Overlay *overlay)
{

	LPDIRECT3DTEXTURE9 surface;

	surface = overlay->hwdata->surface;
	IDirect3DTexture9_UnlockRect(surface, 0);

}
void XBOX_FreeYUVOverlay(_THIS, SDL_Overlay *overlay)
{

	struct private_yuvhwdata *hwdata;

	hwdata = overlay->hwdata;
	if ( hwdata ) {
		if ( hwdata->surface ) {
			IDirect3DTexture9_Release(hwdata->surface);
		}
		free(hwdata);
		overlay->hwdata = NULL;
	}


}

/* Simple hash for shader cache filenames (DJB2) */
/* Compile flag presets para CreateShader.
 *
 *   PS_FLAGS_DEFAULT: half precision (16-bit). El compilador puede empaquetar 2
 *     half4 en 1 float4, doblando los temp registers efectivos. Suficiente para
 *     la mayoria de shaders 2D (luma/coord/gamma con 10-11 bits de mantisa).
 *   PS_FLAGS_FULL_PRECISION: full float (32-bit). Necesario cuando el shader hace
 *     aritmetica con valores grandes (e.g. fmod(vpos.x, 3) en pantallas 1080p+
 *     pierde precision en half y aparecen bandas verticales por subpixel mal
 *     calculado a partir de vpos.x~256+).
 */
#define PS_FLAGS_DEFAULT         (D3DXSHADER_PARTIALPRECISION | D3DXSHADER_PREFER_FLOW_CONTROL)
#define PS_FLAGS_FULL_PRECISION  (D3DXSHADER_PREFER_FLOW_CONTROL)

/* Mezcla los flags en el hash del source para que el cache distinga compilaciones
 * con flags distintos. Sin esto, dos llamadas con misma source pero distintos
 * flags devolverian el mismo .pso cacheado. */
static unsigned long XBOX_HashShaderSource(const char* str, DWORD flags) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    hash = ((hash << 5) + hash) + (unsigned long)flags;
    return hash;
}

/* Try to load a cached compiled shader from disk.
   Returns the bytecode buffer (caller must free) and sets *outSize.
   Returns NULL if cache miss. */
static DWORD* XBOX_LoadCachedShader(unsigned long hash, DWORD* outSize) {
    char path[256];
    HANDLE hFile;
    DWORD fileSize, bytesRead;
    DWORD* buffer;

    sprintf(path, "game:\\shadercache\\%08lX.pso", hash);
    hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return NULL;

    fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return NULL;
    }

    buffer = (DWORD*)malloc(fileSize);
    if (!buffer) { CloseHandle(hFile); return NULL; }

    if (!ReadFile(hFile, buffer, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
        free(buffer);
        CloseHandle(hFile);
        return NULL;
    }

    CloseHandle(hFile);
    *outSize = fileSize;
    return buffer;
}

/* Save compiled shader bytecode to disk cache. */
static void XBOX_SaveCachedShader(unsigned long hash, const void* bytecode, DWORD size) {
    char path[256];
    HANDLE hFile;
    DWORD bytesWritten;

    /* Ensure cache directory exists */
    CreateDirectory("game:\\shadercache", NULL);

    sprintf(path, "game:\\shadercache\\%08lX.pso", hash);
    hFile = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    WriteFile(hFile, bytecode, size, &bytesWritten, NULL);
    CloseHandle(hFile);
}

HRESULT CreateShader(const char* source, IDirect3DPixelShader9** target, DWORD flags) {
    ID3DXBuffer* pCode = NULL;
    ID3DXBuffer* pError = NULL;
    HRESULT hr;
    unsigned long hash;
    DWORD cachedSize = 0;
    DWORD* cachedCode;

    if (!source || !target) return E_INVALIDARG;

    /* Try loading from cache first (hash incluye los flags) */
    hash = XBOX_HashShaderSource(source, flags);
    cachedCode = XBOX_LoadCachedShader(hash, &cachedSize);
    if (cachedCode) {
        //OutputDebugString("  -> loaded from cache\n");
        hr = IDirect3DDevice9_CreatePixelShader(D3D_Device, cachedCode, target);
        free(cachedCode);
        return hr;
    }

    /* Cache miss: compile from source.
     * Flags pasados por el caller (PS_FLAGS_DEFAULT / PS_FLAGS_FULL_PRECISION):
     *   D3DXSHADER_PARTIALPRECISION - usa half (16-bit). Suficiente para shaders
     *     de color (luma/coord ~11 bits) pero NO para aritmetica con vpos en
     *     resoluciones 1080p+ (perdida de precision en fmod). Para esos casos
     *     se pasa PS_FLAGS_FULL_PRECISION.
     *   D3DXSHADER_PREFER_FLOW_CONTROL - prefiere ramas reales sobre predicacion.
     *     Reduce pressure en shaders con branches dinamicos. */
    //OutputDebugString("  -> compiling from source...\n");
    hr = D3DXCompileShader(source, (UINT)strlen(source), NULL, NULL, "main", "ps_3_0",
                            flags,
                            &pCode, &pError, NULL);

    if (FAILED(hr)) {
        if (pError) {
            /* Habilitado temporalmente para diagnosticar shaders que no compilan
             * - el output va al debugger del XDK (si la consola esta conectada
             * a un kit). Si tu setup es retail/RGH sin debugger, comenta esto
             * de nuevo y dime los sintomas; podemos volcar a fichero tambien. */
            OutputDebugString((char*)pError->lpVtbl->GetBufferPointer(pError));
            pError->lpVtbl->Release(pError);
        }
        *target = NULL;
        return hr;
    }

    /* Save to cache for next time */
    XBOX_SaveCachedShader(hash,
        pCode->lpVtbl->GetBufferPointer(pCode),
        pCode->lpVtbl->GetBufferSize(pCode));

    hr = IDirect3DDevice9_CreatePixelShader(D3D_Device, (DWORD*)pCode->lpVtbl->GetBufferPointer(pCode), target);
    pCode->lpVtbl->Release(pCode);
    if (pError) pError->lpVtbl->Release(pError);
    return hr;
}

/* Sube una LUT ya decodificada a una textura.
   El contrato de salvia_shader_api.h es "words 0xAARRGGBB en endianness
   nativa": en Xenon (big-endian) eso son bytes A,R,G,B en memoria, que es
   exactamente el layout de D3DFMT_LIN_A8R8G8B8 -> memcpy por fila, sin
   reordenar nada. El byte-swap BGRA->ARGB que habia aqui existia solo porque
   los datos venian de un array .h en formato BGRA del extractor .NET. */
static LPDIRECT3DTEXTURE9 XBOX_CreateLUTTexture(const SalviaShaderLut* src)
{
    LPDIRECT3DTEXTURE9 tex = NULL;
    D3DLOCKED_RECT lr;
    HRESULT hr;
    int y;

    if (!src || !src->pixels || src->width <= 0 || src->height <= 0) return NULL;

    hr = IDirect3DDevice9_CreateTexture(D3D_Device, src->width, src->height, 1, 0,
        D3DFMT_LIN_A8R8G8B8, D3DUSAGE_CPU_CACHED_MEMORY, (D3DTexture**)&tex, NULL);
    if (FAILED(hr) || !tex) return NULL;

    hr = IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0);
    if (FAILED(hr)) {
        IDirect3DTexture9_Release(tex);
        return NULL;
    }

    for (y = 0; y < src->height; y++) {
        memcpy((unsigned char*)lr.pBits + y * lr.Pitch,
               src->pixels + (size_t)y * src->pitch,
               (size_t)src->width * 4);
    }

    IDirect3DTexture9_UnlockRect(tex, 0);
    return tex;
}

static DWORD XBOX_MapWrap(SalviaShaderWrap w)
{
    if (w == SALVIA_WRAP_REPEAT) return D3DTADDRESS_WRAP;
    if (w == SALVIA_WRAP_MIRROR) return D3DTADDRESS_MIRROR;
    return D3DTADDRESS_CLAMP;
}

/* =====================================================================
 * API de registro de la tabla de shaders (src/video/salvia_shader_api.h).
 * La capa de aplicacion (shaderpreset.cpp) descubre y parsea assets\shaders
 * y publica el resultado aqui ANTES de SDL_SetVideoMode, porque initShaders()
 * se llama desde dentro de el.
 * =================================================================== */
int SalviaShader_SetTable(const SalviaShaderPreset* presets, int count)
{
    if (!presets || count <= 0) return 0;
    if (count > SALVIA_SHADER_MAX_PRESETS) count = SALVIA_SHADER_MAX_PRESETS;
    g_presets = presets;
    g_presetCount = count;
    return 1;
}

int SalviaShader_GetCount(void)     { return g_presetCount; }
int SalviaShader_LutsUploaded(void) { return g_lutsUploaded; }

void SalviaShader_Release(void)
{
    g_presets = NULL;
    g_presetCount = 0;
}

void initShaders() {
    int i, l;

    if (g_fallbackPS != NULL) return;   /* ya compilados */

    /* El passthrough va PRIMERO y sin condiciones. Es lo que garantiza que ni
       un preset roto ni la ausencia de assets\shaders puedan dejar el pipeline
       con un pixel shader NULL (= pantalla negra en Xenon). */
    CreateShader(g_strShaderNormalSource, &g_fallbackPS, PS_FLAGS_DEFAULT);
    if (g_fallbackPS == NULL) {
        OutputDebugString("ERROR: no se pudo compilar el passthrough integrado\n");
        return;
    }
    g_activePS = g_fallbackPS;
    g_pPixelShader = g_fallbackPS;

    if (g_presetCount <= 0 || g_presets == NULL) {
        OutputDebugString("Shaders: no hay tabla registrada; solo passthrough\n");
        XBOX_SetVideoFilter(0);
        return;
    }

    g_compiled = (IDirect3DPixelShader9**)calloc(g_presetCount, sizeof(IDirect3DPixelShader9*));
    g_lutTex   = (LPDIRECT3DTEXTURE9**)calloc(g_presetCount, sizeof(LPDIRECT3DTEXTURE9*));
    if (g_compiled == NULL || g_lutTex == NULL) return;

    for (i = 0; i < g_presetCount; i++) {
        const SalviaShaderPass* pass = &g_presets[i].passes[g_presets[i].activePass];

        /* Sin source, el preset solo describe estado de sampler (asi se
           representan Nearest y Bilinear): se queda NULL y XBOX_SelectEffect
           engancha el passthrough. */
        if (pass->source != NULL) {
            DWORD flags = (pass->psFlags & SALVIA_PS_FULL_PRECISION)
                        ? PS_FLAGS_FULL_PRECISION : PS_FLAGS_DEFAULT;
            CreateShader(pass->source, &g_compiled[i], flags);
            if (g_compiled[i] == NULL) {
                char msg[160];
                sprintf(msg, "ERROR: el preset '%s' no compila; cae a passthrough\n",
                        g_presets[i].id);
                OutputDebugString(msg);
            }
        }

        g_lutTex[i] = (LPDIRECT3DTEXTURE9*)calloc(SALVIA_SHADER_MAX_LUTS,
                                                  sizeof(LPDIRECT3DTEXTURE9));
        if (g_lutTex[i] == NULL) continue;
        for (l = 0; l < pass->lutCount && l < SALVIA_SHADER_MAX_LUTS; l++)
            g_lutTex[i][l] = XBOX_CreateLUTTexture(&pass->luts[l]);
    }

    g_lutsUploaded = 1;
    XBOX_SetVideoFilter(0);
}

void destroyShaders() {
    int i, l;

    if (g_compiled != NULL) {
        for (i = 0; i < g_presetCount; i++) {
            if (g_compiled[i] != NULL) {
                IDirect3DPixelShader9_Release(g_compiled[i]);
                g_compiled[i] = NULL;
            }
        }
        free(g_compiled);
        g_compiled = NULL;
    }

    if (g_lutTex != NULL) {
        for (i = 0; i < g_presetCount; i++) {
            if (g_lutTex[i] == NULL) continue;
            for (l = 0; l < SALVIA_SHADER_MAX_LUTS; l++) {
                if (g_lutTex[i][l] != NULL) {
                    IDirect3DTexture9_Release(g_lutTex[i][l]);
                    g_lutTex[i][l] = NULL;
                }
            }
            free(g_lutTex[i]);
        }
        free(g_lutTex);
        g_lutTex = NULL;
    }

    if (g_fallbackPS != NULL) {
        IDirect3DPixelShader9_Release(g_fallbackPS);
        g_fallbackPS = NULL;
    }
    g_activePS = NULL;
    g_pPixelShader = NULL;
    g_lutsUploaded = 0;
    SalviaShader_Release();
}

/* Aplica el estado descrito por el preset. Sustituye al switch de 13 casos
   que habia aqui: filtro, wrap y LUT vienen ahora del .hlslp. */
void XBOX_SelectEffect(int effectID) {
    const SalviaShaderPass* pass;
    IDirect3DPixelShader9* ps;
    float dims[4];
    int i;

    if (!D3D_Device || g_fallbackPS == NULL) return;

    if (g_presetCount <= 0 || g_presets == NULL) {
        /* Sin tabla: passthrough puro. Nunca se deja el pipeline sin shader. */
        g_activePS = g_fallbackPS;
        IDirect3DDevice9_SetPixelShader(D3D_Device, g_activePS);
        XBOX_SetSampler0Filter(D3DTEXF_POINT);
        return;
    }

    /* Clamp de rango: la rama Windows ya lo tenia, esta no. */
    if (effectID < 0 || effectID >= g_presetCount) effectID = 0;
    g_current_effect = effectID;

    pass = &g_presets[effectID].passes[g_presets[effectID].activePass];

    /* 1. Shader. Cachear el puntero (en vez de indexar por g_current_effect
          desde el bucle de dibujado) es lo que garantiza que jamas se engancha
          un NULL: un preset que no compilo cae aqui al passthrough. */
    ps = (g_compiled != NULL && g_compiled[effectID] != NULL)
       ? g_compiled[effectID] : g_fallbackPS;
    g_activePS = ps;
    g_pPixelShader = ps;
    IDirect3DDevice9_SetPixelShader(D3D_Device, ps);

    /* 2. c1 = textureDims, SIEMPRE. Antes habia efectos que no lo escribian;
          ponerlo de mas cuesta 4 floats por CAMBIO de efecto (no por frame) y
          elimina toda la casuistica. */
    dims[0] = (float)g_texture_width;
    dims[1] = (float)g_texture_height;
    dims[2] = 0.0f;
    dims[3] = 0.0f;
    IDirect3DDevice9_SetPixelShaderConstantF(D3D_Device, 1, dims, 1);

    /* 3. Sampler s0: filtro y wrap SIEMPRE. Escribir el wrap sin condiciones
          arregla de paso una fuga de estado real que habia antes: los efectos
          que ponian CLAMP explicito (Lottes, Easymode, HQx) nunca lo devolvian
          a su valor, asi que el siguiente efecto heredaba su direccionamiento. */
    XBOX_SetSampler0Filter(pass->filter == SALVIA_FILTER_LINEAR
                           ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_ADDRESSU, XBOX_MapWrap(pass->wrap));
    IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_ADDRESSV, XBOX_MapWrap(pass->wrap));

    /* 4. LUTs: desvincular s1..sN y enganchar las de este preset. */
    for (i = 1; i <= SALVIA_SHADER_MAX_LUTS; i++)
        IDirect3DDevice9_SetTexture(D3D_Device, i, NULL);

    for (i = 0; i < pass->lutCount && i < SALVIA_SHADER_MAX_LUTS; i++) {
        int s = pass->luts[i].sampler;
        LPDIRECT3DTEXTURE9 tex = (g_lutTex != NULL && g_lutTex[effectID] != NULL)
                               ? g_lutTex[effectID][i] : NULL;
        DWORD f = (pass->luts[i].filter == SALVIA_FILTER_LINEAR)
                ? D3DTEXF_LINEAR : D3DTEXF_POINT;
        if (tex == NULL || s < 1 || s > SALVIA_SHADER_MAX_LUTS) continue;
        IDirect3DDevice9_SetTexture(D3D_Device, s, (D3DBaseTexture*)tex);
        IDirect3DDevice9_SetSamplerState(D3D_Device, s, D3DSAMP_MINFILTER, f);
        IDirect3DDevice9_SetSamplerState(D3D_Device, s, D3DSAMP_MAGFILTER, f);
        IDirect3DDevice9_SetSamplerState(D3D_Device, s, D3DSAMP_ADDRESSU,
                                         XBOX_MapWrap(pass->luts[i].wrap));
        IDirect3DDevice9_SetSamplerState(D3D_Device, s, D3DSAMP_ADDRESSV,
                                         XBOX_MapWrap(pass->luts[i].wrap));
    }

    /* Update quad size: in pixel-perfect mode the scale factor may change with the effect */
    if (!g_display_fullscreen && g_texture_width > 0)
        XBOX_UpdateVertexBuffer(g_texture_width, g_texture_height, g_display_aspect_ratio);
}
