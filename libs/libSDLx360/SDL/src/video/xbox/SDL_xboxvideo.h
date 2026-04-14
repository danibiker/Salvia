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
 "@(#) $Id: SDL_xboxvideo.h,v 1.1 2003/07/18 15:19:33 lantus Exp $";
#endif

#ifndef _SDL_nullvideo_h
#define _SDL_nullvideo_h

#include <xtl.h>
#include <d3dx9.h>
#include <XGraphics.h>
#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_mutex.h"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_VideoDevice *this

/* Private display data */

typedef struct _VERTEX {
		float x, y, z;
		float rhw;
		float tx, ty;		 
} VERTEX; //our custom vertex with a constuctor for easier assignment


VERTEX triangleStripVertices[4];					//the 4 vertices that make up our display rectangle

struct SDL_PrivateVideoData {
	LPDIRECT3DTEXTURE9 SDL_primary;
};

LPDIRECT3D9 D3D;
LPDIRECT3DDEVICE9 D3D_Device;
D3DPRESENT_PARAMETERS D3D_PP;
LPDIRECT3DVERTEXBUFFER9 SDL_vertexbuffer;
PALETTEENTRY SDL_colors[256];
LPDIRECT3DVERTEXBUFFER9 vertexBuffer;
LPDIRECT3DVERTEXDECLARATION9 vertexDeclaration;
D3DVertexShader* g_pGradientVertexShader;
D3DVertexDeclaration* g_pGradientVertexDecl;
D3DPixelShader* g_pPixelShader;
LPD3DXBUFFER ppShader;
LPD3DXBUFFER pPixelShaderCode;
LPD3DXBUFFER pPixelErrorMsg;


SDL_Overlay *XBOX_CreateYUVOverlay(_THIS, int width, int height, Uint32 format, SDL_Surface *display);
int XBOX_DisplayYUVOverlay(_THIS, SDL_Overlay *overlay, SDL_Rect *src, SDL_Rect *dst);
int XBOX_LockYUVOverlay(_THIS, SDL_Overlay *overlay);
void XBOX_UnlockYUVOverlay(_THIS, SDL_Overlay *overlay);
void XBOX_FreeYUVOverlay(_THIS, SDL_Overlay *overlay);

/* Update display quad to maintain the given aspect ratio (width/height).
   E.g. 4.0f/3.0f for 4:3, 16.0f/9.0f for 16:9.
   Can be called at any time without recreating the texture or surface. */
void SDL_XBOX_SetDisplaySize(float aspect_ratio);

/* Set display mode: 1 = scale to fill screen (default), 0 = pixel perfect size.
   In pixel perfect mode, the quad is tex_size * effect_scale, centered on screen.
   E.g. 320x240 with Scale2x = 640x480 centered. */
void SDL_XBOX_SetDisplayFullscreen(int fullscreen);

/* Get the overlay surface (1280x720, 32bpp ARGB).
   The overlay is drawn on top of the game quad with alpha blending.
   Write pixels with alpha=0x00 for transparent, alpha=0xFF for opaque.
   Created lazily on first call. */
SDL_Surface* SDL_XBOX_GetOverlay(void);

/* Enable (1) or disable (0) overlay rendering. */
void SDL_XBOX_SetOverlayEnabled(int enabled);

#endif /* _SDL_nullvideo_h */

 