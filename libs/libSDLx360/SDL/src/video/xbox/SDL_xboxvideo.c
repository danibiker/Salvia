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

 // Declaración global (debe ser un puntero a punteros)
IDirect3DPixelShader9** pixelShaders = NULL;

int have_vertexbuffer=0;
int have_direct3dtexture=0;

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
    "   float4 Position : POSITION;                            \n" // En vs_3_0 POSITION es válido
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
    " sampler2D detail : register(s0);             " // Definición explícita de sampler2D
    "                                              "
    " float4 main( PS_IN In ) : COLOR0             " // ps_3_0 usa COLOR0 como salida
    " {                                            "
    "     return tex2D( detail, In.TexCoord );     "
    " }     ";
*/

const char* g_strPixelShaderProgram =
    " float fFilterType : register(c0);            " // El parámetro vive aquí
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
	return &vid_modes;
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

SDL_Surface *XBOX_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{

	void *pLockedVertexBuffer;
	float tX = 1;
	float tY = 1;
  
	VERTEX a;
	VERTEX b;
	VERTEX c;
	VERTEX d;

	int pixel_mode,pitch;
	Uint32 Rmask, Gmask, Bmask;

	HRESULT ret;

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

	//if (have_vertexbuffer) {
	//	D3DResource_Release((D3DResource*)SDL_vertexbuffer);
	//}

    initShaders();
   	// Create pixel shader.
	g_pPixelShader = pixelShaders[0];
	XBOX_SetVideoFilter(0);
	 
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
 
	// 3. Cambio en la compilación del Vertex Shader: vs_2_0 -> vs_3_0
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
 	 
// 4. Corrección de llamadas a la API (Usar los métodos del Device correctamente)
    IDirect3DDevice9_CreateVertexBuffer(D3D_Device, sizeof(triangleStripVertices), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &vertexBuffer, NULL );

    IDirect3DDevice9_SetVertexShader(D3D_Device, g_pGradientVertexShader );
    IDirect3DDevice9_SetPixelShader(D3D_Device, g_pPixelShader);
    IDirect3DDevice9_SetVertexDeclaration(D3D_Device, g_pGradientVertexDecl);

	D3DDevice_SetRenderState(D3D_Device, D3DRS_VIEWPORTENABLE, FALSE);

	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
 
  
	IDirect3DVertexBuffer9_Lock(vertexBuffer, 0, 0, (BYTE **)&pLockedVertexBuffer, 0L );	 

	a.x = (float)0 - 0.5f;
	a.y = (float)D3D_PP.BackBufferHeight - 0.5f;
	a.z = 0;
	a.rhw = 1;
	a.tx = 0;
	a.ty = tY;

	b.x = (float)0 - 0.5f;
	b.y = (float)0 - 0.5f;
	b.z = 0;
	b.rhw = 1;
	b.tx = 0;
	b.ty = 0;

	c.x = (float)D3D_PP.BackBufferWidth - 0.5f;
	c.y = (float)D3D_PP.BackBufferHeight - 0.5f;
	c.z = 0;
	c.rhw = 1;
	c.tx = tX;
	c.ty = tY;

	d.x = (float)D3D_PP.BackBufferWidth - 0.5f;
	d.y = (float)0 - 0.5f;
	d.z = 0;
	d.rhw = 1;
	d.tx = tX;
	d.ty = 0;


	triangleStripVertices[0] = a;
	triangleStripVertices[1] = b;
	triangleStripVertices[2] = c;
	triangleStripVertices[3] = d;
	
	memcpy(pLockedVertexBuffer,triangleStripVertices,sizeof(triangleStripVertices));
	IDirect3DVertexBuffer9_Unlock(vertexBuffer);
	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET , 0x00000000, 1.0f, 0L);
	IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)this->hidden->SDL_primary);
	IDirect3DDevice9_SetStreamSource(D3D_Device, 0,vertexBuffer,0,sizeof(VERTEX));		
    	 
	have_vertexbuffer=1;


	/* Set up the new mode framebuffer */
	current->flags = (SDL_FULLSCREEN|SDL_HWSURFACE);

	if (flags & SDL_DOUBLEBUF)
		current->flags |= SDL_DOUBLEBUF;
	if (flags & SDL_HWPALETTE)
		current->flags |= SDL_HWPALETTE;



	current->w = width;
	current->h = height;
	current->pitch = current->w * (bpp / 8);
	current->pixels = NULL;


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
	static int    i = 0;

	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET , 0x00000000, 1.0f, 0L);
	
 
    // Render the image
    IDirect3DDevice9_DrawPrimitive(D3D_Device,  D3DPT_TRIANGLESTRIP, 0, 2 ); 
	IDirect3DDevice9_Present(D3D_Device,NULL,NULL,NULL,NULL);

	return (1);
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
 
	if (!this->hidden->SDL_primary)
		return;

    // Render the image

	IDirect3DDevice9_DrawPrimitive(D3D_Device,  D3DPT_TRIANGLESTRIP, 0, 2 ); 
	IDirect3DDevice9_Present(D3D_Device,NULL,NULL,NULL,NULL);


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
	 if (this->hidden->SDL_primary)
	 {
        IDirect3DDevice9_SetTexture(D3D_Device, 0, NULL);
		IDirect3DDevice9_SetStreamSource(D3D_Device, 0,NULL, 0, 0);	
		IDirect3DTexture9_Release(this->hidden->SDL_primary);
	 }

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

HRESULT CreateShader(const char* source, IDirect3DPixelShader9** target) {
    ID3DXBuffer* pCode = NULL;
    ID3DXBuffer* pError = NULL;
    HRESULT hr;

    hr = D3DXCompileShader(source, (UINT)strlen(source), NULL, NULL, "main", "ps_2_0", 0, &pCode, &pError, NULL);
    
    if (FAILED(hr)) {
        if (pError) OutputDebugString((char*)pError->lpVtbl->GetBufferPointer(pError));
        return hr;
    }

    hr = IDirect3DDevice9_CreatePixelShader(D3D_Device, (DWORD*)pCode->lpVtbl->GetBufferPointer(pCode), target);
    pCode->lpVtbl->Release(pCode);
    return hr;
}

void initShaders() {
    // Reservamos espacio para 3 PUNTEROS
    pixelShaders = (IDirect3DPixelShader9**) malloc(3 * sizeof(IDirect3DPixelShader9*));

    // Compilar efectos
    CreateShader(g_strShaderNormalSource, &pixelShaders[0]);
    CreateShader(g_strShaderGraySource,   &pixelShaders[1]);
    CreateShader(g_strShaderSepiaSource,  &pixelShaders[2]);
}

void destroyShaders() {
	int i;
    if (pixelShaders == NULL) return;
    for (i = 0; i < 3; i++) {
        if (pixelShaders[i] != NULL) {
            // Opción A: Casting a IUnknown (La más robusta en el SDK de Xbox)
            ((IUnknown*)pixelShaders[i])->lpVtbl->Release((IUnknown*)pixelShaders[i]);
            
            // Opción B: Si estás en C++, simplemente:
            // pixelShaders[i]->Release();
            
            pixelShaders[i] = NULL;
        }
    }
    free(pixelShaders);
    pixelShaders = NULL;
}

void XBOX_SelectEffect(int effectID) {
    if (!D3D_Device || !pixelShaders) return;
    
    // Pasamos directamente el puntero almacenado en el array
    IDirect3DDevice9_SetPixelShader(D3D_Device, pixelShaders[effectID]);
}