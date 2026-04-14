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
#include "hq2x_lut.h"
#include "hq3x_lut.h"
#include "hq4x_lut.h"

#define XBOXVID_DRIVER_NAME "XBOX"
 

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

 // Declaraci�n global (debe ser un puntero a punteros)
IDirect3DPixelShader9** pixelShaders = NULL;

int have_vertexbuffer=0;
int have_direct3dtexture=0;
static float g_display_aspect_ratio = 0.0f; /* 0 = use native pixel ratio */
static int g_display_fullscreen = 1; /* 1 = scale to fill screen, 0 = pixel perfect size */
static int g_texture_width = 0;
static int g_texture_height = 0;

/* Overlay system: a 1280x720 ARGB layer drawn on top of the game quad */
static LPDIRECT3DTEXTURE9 g_overlay_texture = NULL;
static LPDIRECT3DVERTEXBUFFER9 g_overlay_vb = NULL;
static SDL_Surface* g_overlay_surface = NULL;
static int g_overlay_enabled = 0;
static int g_overlay_locked = 0;
static int g_current_effect = 0;

/* Total number of effects: 0=Nearest, 1=Bilinear, 2=Scanlines, 3=CRT, 4=HQ2x, 5=HQ3x, 6=HQ4x */
#define NUM_EFFECTS 7

/* HQ2x/HQ3x/HQ4x LUT textures (created from embedded data) */
static LPDIRECT3DTEXTURE9 g_hq2x_lut = NULL;
static LPDIRECT3DTEXTURE9 g_hq3x_lut = NULL;
static LPDIRECT3DTEXTURE9 g_hq4x_lut = NULL;

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
    "   float4 Position : POSITION;                            \n" // En vs_3_0 POSITION es v�lido
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
    "     float2 TexCoord : TEXCOORD0;             " // TEXCOORD0 para coincidir con VS
    " };                                           "
    "                                              "
    " sampler2D detail : register(s0);             " // Definici�n expl�cita de sampler2D
    "                                              "
    " float4 main( PS_IN In ) : COLOR0             " // ps_3_0 usa COLOR0 como salida
    " {                                            "
    "     return tex2D( detail, In.TexCoord );     "
    " }     ";
*/

const char* g_strPixelShaderProgram =
    " float fFilterType : register(c0);            " // El par�metro vive aqu�
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


int XBOX_VideoInit(_THIS, SDL_PixelFormat *vformat)
{	 
    // 1. TODAS las declaraciones de variables al principio
    D3DCAPS9 caps; 
    HRESULT hr;

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

    D3D_PP.BackBufferWidth = 1280;
    D3D_PP.BackBufferHeight = 720;
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

    if (!D3D_Device) {
        // Usamos D3DCREATE_HARDWARE_VERTEXPROCESSING para asegurar soporte de VS_3_0
        hr = IDirect3D9_CreateDevice(D3D, 0, D3DDEVTYPE_HAL, NULL, 
                                     D3DCREATE_HARDWARE_VERTEXPROCESSING, 
                                     &D3D_PP, &D3D_Device);
        if (FAILED(hr)) return 0;
    }

    vformat->BitsPerPixel = 32;
    vformat->BytesPerPixel = 4;
    vformat->Amask = 0xFF000000;
    vformat->Rmask = 0x00FF0000;
    vformat->Gmask = 0x0000FF00;
    vformat->Bmask = 0x000000FF;

    return (D3D_Device) ? 1 : 0;
}

const static SDL_Rect
	RECT_1280x720 = {0,0,1280,720},
	RECT_800x600 = {0,0,800,600},
	RECT_640x480 = {0,0,640,480},
	RECT_512x384 = {0,0,512,384},
	RECT_400x300 = {0,0,400,300},
	RECT_320x240 = {0,0,320,240},
	RECT_320x200 = {0,0,320,200};
const static SDL_Rect *vid_modes[] = {
	&RECT_1280x720,
	&RECT_800x600,
	&RECT_640x480,
	&RECT_512x384,
	&RECT_400x300,
	&RECT_320x240,
	&RECT_320x200,
	NULL
};


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
   Nearest/Bilinear/Scanlines/CRT = 1x, HQ2x = 2x, HQ3x = 3x, HQ4x = 4x. */
static int XBOX_GetEffectScale(void)
{
	switch (g_current_effect) {
		case 4: return 2; /* HQ2x */
		case 5: return 3; /* HQ3x */
		case 6: return 4; /* HQ4x */
		default: return 1; /* 0=Nearest, 1=Bilinear, 2=Scanlines, 3=CRT */
	}
}

/* aspect_ratio = desired width/height (e.g. 4.0f/3.0f for 4:3, 16.0f/9.0f for 16:9).
   If aspect_ratio <= 0, uses the texture's native pixel ratio (tex_w/tex_h).
   If g_display_fullscreen == 0, shows at pixel-perfect size (tex * effect_scale). */
static void XBOX_UpdateVertexBuffer(int tex_w, int tex_h, float aspect_ratio)
{
	void *pLockedVertexBuffer;
	float display_w, display_h, offset_x, offset_y;
	float bbw = (float)D3D_PP.BackBufferWidth;
	float bbh = (float)D3D_PP.BackBufferHeight;
	float bb_ratio;

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

		//We only scale the factor 1 to the maximum available pixel perfect factor
		//wich affects to nearest neighbour, scanlines and crt filters
		if (scale == 1 && tex_w > 0 && tex_h > 0){
			int maxscale = min(floor(bbw / (float)tex_w), floor(bbh / (float)tex_h));
			if (maxscale > 1){
				scale = maxscale;
			}
		}

		display_w = (float)(tex_w * scale);
		display_h = (float)(tex_h * scale);

		/* Clamp to backbuffer if larger */
		/* Ajuste proporcional si la imagen excede el backbuffer */
		if (display_w > bbw || display_h > bbh) {
			if (aspect_ratio <= 0.0f)
				aspect_ratio = (float)tex_w / (float)tex_h;

			bb_ratio = bbw / bbh;

			if (aspect_ratio > bb_ratio) {
				display_w = bbw;
				display_h = bbw / aspect_ratio;
			} else {
				display_h = bbh;
				display_w = bbh * aspect_ratio;
			}
		}
	}

	offset_x = (bbw - display_w) * 0.5f;
	offset_y = (bbh - display_h) * 0.5f;

	IDirect3DVertexBuffer9_Lock(vertexBuffer, 0, 0, (BYTE **)&pLockedVertexBuffer, 0L);

	/* a=bottom-left, b=top-left, c=bottom-right, d=top-right (triangle strip) */
	triangleStripVertices[0].x = offset_x - 0.5f;
	triangleStripVertices[0].y = offset_y + display_h - 0.5f;
	triangleStripVertices[0].z = 0;
	triangleStripVertices[0].rhw = 1;
	triangleStripVertices[0].tx = 0;
	triangleStripVertices[0].ty = 1;

	triangleStripVertices[1].x = offset_x - 0.5f;
	triangleStripVertices[1].y = offset_y - 0.5f;
	triangleStripVertices[1].z = 0;
	triangleStripVertices[1].rhw = 1;
	triangleStripVertices[1].tx = 0;
	triangleStripVertices[1].ty = 0;

	triangleStripVertices[2].x = offset_x + display_w - 0.5f;
	triangleStripVertices[2].y = offset_y + display_h - 0.5f;
	triangleStripVertices[2].z = 0;
	triangleStripVertices[2].rhw = 1;
	triangleStripVertices[2].tx = 1;
	triangleStripVertices[2].ty = 1;

	triangleStripVertices[3].x = offset_x + display_w - 0.5f;
	triangleStripVertices[3].y = offset_y - 0.5f;
	triangleStripVertices[3].z = 0;
	triangleStripVertices[3].rhw = 1;
	triangleStripVertices[3].tx = 1;
	triangleStripVertices[3].ty = 0;

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

/* ---- Overlay system ---- */

static void XBOX_InitOverlay(void)
{
	VERTEX overlayVerts[4];
	void *pLocked;
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
		sizeof(overlayVerts), D3DUSAGE_WRITEONLY, 0,
		D3DPOOL_DEFAULT, &g_overlay_vb, NULL);

	if (!g_overlay_vb) {
		IDirect3DTexture9_Release(g_overlay_texture);
		g_overlay_texture = NULL;
		return;
	}

	/* Fullscreen quad vertices */
	overlayVerts[0].x = -0.5f;
	overlayVerts[0].y = bbh - 0.5f;
	overlayVerts[0].z = 0; overlayVerts[0].rhw = 1;
	overlayVerts[0].tx = 0; overlayVerts[0].ty = 1;

	overlayVerts[1].x = -0.5f;
	overlayVerts[1].y = -0.5f;
	overlayVerts[1].z = 0; overlayVerts[1].rhw = 1;
	overlayVerts[1].tx = 0; overlayVerts[1].ty = 0;

	overlayVerts[2].x = bbw - 0.5f;
	overlayVerts[2].y = bbh - 0.5f;
	overlayVerts[2].z = 0; overlayVerts[2].rhw = 1;
	overlayVerts[2].tx = 1; overlayVerts[2].ty = 1;

	overlayVerts[3].x = bbw - 0.5f;
	overlayVerts[3].y = -0.5f;
	overlayVerts[3].z = 0; overlayVerts[3].rhw = 1;
	overlayVerts[3].tx = 1; overlayVerts[3].ty = 0;

	IDirect3DVertexBuffer9_Lock(g_overlay_vb, 0, 0, (BYTE **)&pLocked, 0L);
	memcpy(pLocked, overlayVerts, sizeof(overlayVerts));
	IDirect3DVertexBuffer9_Unlock(g_overlay_vb);

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
   game_texture is the primary D3D texture to restore after drawing. */
static void XBOX_DrawOverlay(LPDIRECT3DTEXTURE9 game_texture)
{
	D3DLOCKED_RECT d3dlr;

	if (!g_overlay_enabled || !g_overlay_texture || !g_overlay_vb) return;

	/* Unlock overlay texture so GPU can read it */
	IDirect3DTexture9_UnlockRect(g_overlay_texture, 0);

	/* Enable alpha blending */
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_ALPHABLENDENABLE, TRUE);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	/* Switch to overlay texture and vertex buffer, use normal shader (no Scale2x) */
	IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)g_overlay_texture);
	IDirect3DDevice9_SetStreamSource(D3D_Device, 0, g_overlay_vb, 0, sizeof(VERTEX));
	IDirect3DDevice9_SetPixelShader(D3D_Device, pixelShaders[0]);
	IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	IDirect3DDevice9_DrawPrimitive(D3D_Device, D3DPT_TRIANGLESTRIP, 0, 2);

	/* Disable alpha blending */
	IDirect3DDevice9_SetRenderState(D3D_Device, D3DRS_ALPHABLENDENABLE, FALSE);

	/* Restore game texture, vertex buffer and active shader */
	IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)game_texture);
	IDirect3DDevice9_SetStreamSource(D3D_Device, 0, vertexBuffer, 0, sizeof(VERTEX));
	XBOX_SelectEffect(g_current_effect);

	/* Re-lock overlay texture so the app can keep drawing to it */
	IDirect3DTexture9_LockRect(g_overlay_texture, 0, &d3dlr, NULL, 0);
	g_overlay_surface->pixels = d3dlr.pBits;
	g_overlay_surface->pitch = d3dlr.Pitch;
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
 
	// 3. Cambio en la compilaci�n del Vertex Shader: vs_2_0 -> vs_3_0
    if( NULL == g_pGradientVertexShader )
    {
        ID3DXBuffer* pShaderCode;
        if( SUCCEEDED( D3DXCompileShader( g_strGradientShader, (UINT)strlen( g_strGradientShader ),
                                       NULL, NULL, "GradientVertexShader", 
                                       "vs_2_0", 0, // Cambiado a 3.0
                                       &pShaderCode, NULL, NULL ) ) )
        {
            IDirect3DDevice9_CreateVertexShader(D3D_Device, (DWORD*)pShaderCode->lpVtbl->GetBufferPointer(pShaderCode), &g_pGradientVertexShader);
        }
    }
 	 
// 4. Correcci�n de llamadas a la API (Usar los m�todos del Device correctamente)
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

	IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)this->hidden->SDL_primary);
	IDirect3DDevice9_SetStreamSource(D3D_Device, 0, vertexBuffer, 0, sizeof(VERTEX));

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
 
	/* We're done */
	return(current);
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

static int XBOX_RenderSurface(_THIS, SDL_Surface *surface)
{
	D3DLOCKED_RECT d3dlr;

	/* Unlock so the GPU can read the texture for rendering */
	IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);


	/* Clear for letterbox/pillarbox bars, then render game quad */
	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
	IDirect3DDevice9_DrawPrimitive(D3D_Device, D3DPT_TRIANGLESTRIP, 0, 2);

	/* Draw overlay on top if enabled */
	XBOX_DrawOverlay(this->hidden->SDL_primary);

	IDirect3DDevice9_Present(D3D_Device, NULL, NULL, NULL, NULL);

	/* Re-lock so the app can keep drawing directly to texture memory */
	IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &d3dlr, NULL, 0);

	surface->pixels = d3dlr.pBits;
	surface->pitch = d3dlr.Pitch;

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

	/* Unlock so the GPU can read the texture for rendering */
	IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);


	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
	IDirect3DDevice9_DrawPrimitive(D3D_Device, D3DPT_TRIANGLESTRIP, 0, 2);

	/* Draw overlay on top if enabled */
	XBOX_DrawOverlay(this->hidden->SDL_primary);

	IDirect3DDevice9_Present(D3D_Device, NULL, NULL, NULL, NULL);

	/* Re-lock so the app can keep drawing directly to texture memory */
	IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &d3dlr, NULL, 0);

	this->screen->pixels = d3dlr.pBits;
	this->screen->pitch = d3dlr.Pitch;
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

	IDirect3DDevice9_DrawPrimitive(D3D_Device,  D3DPT_TRIANGLESTRIP, 0, 2 ); 
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
static unsigned long XBOX_HashShaderSource(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
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

HRESULT CreateShader(const char* source, IDirect3DPixelShader9** target) {
    ID3DXBuffer* pCode = NULL;
    ID3DXBuffer* pError = NULL;
    HRESULT hr;
    unsigned long hash;
    DWORD cachedSize = 0;
    DWORD* cachedCode;

    if (!source || !target) return E_INVALIDARG;

    /* Try loading from cache first */
    hash = XBOX_HashShaderSource(source);
    cachedCode = XBOX_LoadCachedShader(hash, &cachedSize);
    if (cachedCode) {
        OutputDebugString("  -> loaded from cache\n");
        hr = IDirect3DDevice9_CreatePixelShader(D3D_Device, cachedCode, target);
        free(cachedCode);
        return hr;
    }

    /* Cache miss: compile from source */
    OutputDebugString("  -> compiling from source...\n");
    hr = D3DXCompileShader(source, (UINT)strlen(source), NULL, NULL, "main", "ps_3_0", 0, &pCode, &pError, NULL);

    if (FAILED(hr)) {
        if (pError) {
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

/* Create a LUT texture from embedded BGRA data (from .NET extraction).
   Xbox 360 is big-endian: D3DFMT_LIN_A8R8G8B8 stores bytes as A,R,G,B.
   Source data from .NET is B,G,R,A per pixel - we need to byte-swap. */
static LPDIRECT3DTEXTURE9 XBOX_CreateLUTTexture(const unsigned char* data, int w, int h)
{
    LPDIRECT3DTEXTURE9 tex = NULL;
    D3DLOCKED_RECT lr;
    HRESULT hr;
    int x, y;

    hr = IDirect3DDevice9_CreateTexture(D3D_Device, w, h, 1, 0,
        D3DFMT_LIN_A8R8G8B8, D3DUSAGE_CPU_CACHED_MEMORY, (D3DTexture**)&tex, NULL);
    if (FAILED(hr) || !tex) return NULL;

    hr = IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0);
    if (FAILED(hr)) {
        IDirect3DTexture9_Release(tex);
        return NULL;
    }

    for (y = 0; y < h; y++) {
        const unsigned char* src_row = data + y * w * 4;
        unsigned char* dst_row = (unsigned char*)lr.pBits + y * lr.Pitch;
        for (x = 0; x < w; x++) {
            /* Source: B,G,R,A (BGRA from .NET) */
            /* Dest:   A,R,G,B (D3DFMT_LIN_A8R8G8B8 big-endian) */
            unsigned char B = src_row[x*4+0];
            unsigned char G = src_row[x*4+1];
            unsigned char R = src_row[x*4+2];
            unsigned char A = src_row[x*4+3];
            dst_row[x*4+0] = A;
            dst_row[x*4+1] = R;
            dst_row[x*4+2] = G;
            dst_row[x*4+3] = B;
        }
    }

    IDirect3DTexture9_UnlockRect(tex, 0);
    return tex;
}

static void XBOX_InitLUTTextures(void)
{
    if (!g_hq2x_lut) {
        g_hq2x_lut = XBOX_CreateLUTTexture(hq2x_lut_data, HQ2X_LUT_WIDTH, HQ2X_LUT_HEIGHT);
        if (g_hq2x_lut)
            OutputDebugString("HQ2x LUT texture created (256x64)\n");
        else
            OutputDebugString("ERROR: Failed to create HQ2x LUT texture\n");
    }
    if (!g_hq3x_lut) {
        g_hq3x_lut = XBOX_CreateLUTTexture(hq3x_lut_data, HQ3X_LUT_WIDTH, HQ3X_LUT_HEIGHT);
        if (g_hq3x_lut)
            OutputDebugString("HQ3x LUT texture created (256x144)\n");
        else
            OutputDebugString("ERROR: Failed to create HQ3x LUT texture\n");
    }
    if (!g_hq4x_lut) {
        g_hq4x_lut = XBOX_CreateLUTTexture(hq4x_lut_data, HQ4X_LUT_WIDTH, HQ4X_LUT_HEIGHT);
        if (g_hq4x_lut)
            OutputDebugString("HQ4x LUT texture created (256x256)\n");
        else
            OutputDebugString("ERROR: Failed to create HQ4x LUT texture\n");
    }
}

static void XBOX_DestroyLUTTextures(void)
{
    if (g_hq2x_lut) { IDirect3DTexture9_Release(g_hq2x_lut); g_hq2x_lut = NULL; }
    if (g_hq3x_lut) { IDirect3DTexture9_Release(g_hq3x_lut); g_hq3x_lut = NULL; }
    if (g_hq4x_lut) { IDirect3DTexture9_Release(g_hq4x_lut); g_hq4x_lut = NULL; }
}

void initShaders() {
    /* Only compile shaders once - avoid leak on repeated SDL_SetVideoMode calls */
    if (pixelShaders != NULL) return;

    /* Create LUT textures for HQ2x/HQ3x/HQ4x */
    XBOX_InitLUTTextures();

    /* 0=Nearest, 1=Bilinear, 2=Scanlines, 3=CRT, 4=HQ2x, 5=HQ3x, 6=HQ4x */
    pixelShaders = (IDirect3DPixelShader9**) calloc(NUM_EFFECTS, sizeof(IDirect3DPixelShader9*));

    OutputDebugString("initShaders: compiling Nearest (Normal)...\n");
	CreateShader(g_strShaderNormalSource,      &pixelShaders[0]);
    OutputDebugString("initShaders: compiling Bilinear (Normal)...\n");
	CreateShader(g_strShaderNormalSource,      &pixelShaders[1]); 
    OutputDebugString("initShaders: compiling Scanlines...\n");
    CreateShader(g_strShaderScanlinesSource,   &pixelShaders[2]);
    OutputDebugString("initShaders: compiling CRT...\n");
    CreateShader(g_strShaderCRTSource,         &pixelShaders[3]);
    OutputDebugString("initShaders: compiling HQ2x...\n");
    CreateShader(g_strShaderHQ2xSource,        &pixelShaders[4]);
    OutputDebugString("initShaders: compiling HQ3x...\n");
    CreateShader(g_strShaderHQ3xSource,        &pixelShaders[5]);
    OutputDebugString("initShaders: compiling HQ4x...\n");
    CreateShader(g_strShaderHQ4xSource,        &pixelShaders[6]);
    OutputDebugString("initShaders: all 7 shaders compiled.\n");
	// Create pixel shader.
	g_pPixelShader = pixelShaders[0];
	XBOX_SetVideoFilter(0);
}

void destroyShaders() {
    int i;
    if (pixelShaders == NULL) return;

    for (i = 0; i < NUM_EFFECTS; i++) {
        if (pixelShaders[i] != NULL) {
            // En el XDK, si es un archivo .c, usa esta l�nea:
            IDirect3DPixelShader9_Release(pixelShaders[i]);
            
            // Si es un archivo .cpp, usa esta:
            // pixelShaders[i]->Release();
            
            pixelShaders[i] = NULL;
        }
    }
    free(pixelShaders);
    pixelShaders = NULL;

    XBOX_DestroyLUTTextures();
}

void XBOX_SelectEffect(int effectID) {
    if (!D3D_Device || !pixelShaders) return;

    g_current_effect = effectID;

    /* Unbind LUT from sampler s1 by default (HQ effects will re-bind it below) */
    IDirect3DDevice9_SetTexture(D3D_Device, 1, NULL);

    switch (effectID) {
    case 0: /* Nearest Neighbor: passthrough + POINT */
        IDirect3DDevice9_SetPixelShader(D3D_Device, pixelShaders[0]);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        break;

    case 1: /* Bilinear: passthrough + LINEAR */
        IDirect3DDevice9_SetPixelShader(D3D_Device, pixelShaders[1]);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        break;

    case 2: /* Scanlines: dedicated shader + POINT + textureDims */
    {
        float dims[4];
        IDirect3DDevice9_SetPixelShader(D3D_Device, pixelShaders[2]);
        dims[0] = (float)g_texture_width; dims[1] = (float)g_texture_height;
        dims[2] = 0.0f; dims[3] = 0.0f;
        IDirect3DDevice9_SetPixelShaderConstantF(D3D_Device, 1, dims, 1);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        break;
    }

    case 3: /* CRT: dedicated shader + LINEAR + textureDims */
    {
        float dims[4];
        IDirect3DDevice9_SetPixelShader(D3D_Device, pixelShaders[3]);
        dims[0] = (float)g_texture_width; dims[1] = (float)g_texture_height;
        dims[2] = 0.0f; dims[3] = 0.0f;
        IDirect3DDevice9_SetPixelShaderConstantF(D3D_Device, 1, dims, 1);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        break;
    }

    case 4: /* HQ2x */
    case 5: /* HQ3x */
    case 6: /* HQ4x */
    {
        if (pixelShaders[effectID] == NULL) {
            /* Fallback to passthrough if shader failed to compile */
            IDirect3DDevice9_SetPixelShader(D3D_Device, pixelShaders[0]);
            IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        } else {
            float dims[4];
            LPDIRECT3DTEXTURE9 lut = (effectID == 4) ? g_hq2x_lut :
                                     (effectID == 5) ? g_hq3x_lut : g_hq4x_lut;
            IDirect3DDevice9_SetPixelShader(D3D_Device, pixelShaders[effectID]);
            dims[0] = (float)g_texture_width; dims[1] = (float)g_texture_height;
            dims[2] = 0.0f; dims[3] = 0.0f;
            IDirect3DDevice9_SetPixelShaderConstantF(D3D_Device, 1, dims, 1);
            IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            /* Bind LUT texture to sampler s1 with POINT filtering */
            if (lut) {
                IDirect3DDevice9_SetTexture(D3D_Device, 1, (D3DBaseTexture*)lut);
                IDirect3DDevice9_SetSamplerState(D3D_Device, 1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                IDirect3DDevice9_SetSamplerState(D3D_Device, 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                IDirect3DDevice9_SetSamplerState(D3D_Device, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                IDirect3DDevice9_SetSamplerState(D3D_Device, 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            }
        }
        break;
    }

    default: /* Unknown: fallback to Nearest Neighbor */
        IDirect3DDevice9_SetPixelShader(D3D_Device, pixelShaders[0]);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        IDirect3DDevice9_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        break;
    }

    /* Update quad size: in pixel-perfect mode the scale factor may change with the effect */
    if (!g_display_fullscreen && g_texture_width > 0)
        XBOX_UpdateVertexBuffer(g_texture_width, g_texture_height, g_display_aspect_ratio);
}