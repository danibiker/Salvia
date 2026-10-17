/**
 * glN64_GX - OpenGL.h
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009, 2010 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifndef OPENGL_H
#define OPENGL_H

#ifndef __LINUX__
# include <windows.h>
# include "wglext.h"
# include <GL/gl.h>
# include "glext.h"
#else
# include "../main/winlnxdefs.h"
#define GL_GLEXT_PROTOTYPES
#define __WIN32__
#include "gl.h" 
#include "glext.h"
#undef __WIN32__
//# include <GL/gl.h>
//# include <GL/glext.h>
# ifndef __GX__
# include "SDL.h"
# endif // !_GX__
#endif // __LINUX__

#include "glATI.h"
#include "gSP.h"

// Depth guard band
/* ★★ DIVERGENCIA vs Wii64 1b5041a: LA BANDA DE GUARDA VA A 1, O SEA APAGADA.
 *
 * Upstream la introdujo en esta puesta al dia: comprime la z proyectada a
 * 1/4 (`GXprojZScale` pasa de 0.5 a 0.125) y lo compensa ESTIRANDO el
 * viewport, con `GXviewportNearZ` = farz - 4*(farz-nearz), que para el
 * 0..1 de siempre da un near de **-3**.
 *
 * Eso solo funciona si el viewport puede tener un near negativo.  EN D3D NO
 * PUEDE: `D3DVIEWPORT9::MinZ` esta definido en [0,1].  O sea que aqui nos
 * quedamos con la compresion y sin la compensacion, y eso se paga dos veces:
 *
 *  1. ★ LA NIEBLA, que es el defecto que se vio.  El factor de niebla se
 *     calcula en el shader como `ff = saturate((z/w - start) / (end-start))`
 *     con start=-1 y end=+1 (OGL_GXinitDlist), o sea una rampa pensada para
 *     z_ndc en [-1,0].  Con la banda, z_ndc queda en [-0.25,0] y entonces
 *     `ff` arranca en 0.375 EN VEZ DE 0: todos los pixeles salen con un 37%
 *     de niebla de suelo.  Medido como sintoma en dos juegos a la vez y con
 *     colores distintos -- GoldenEye azulado, Zelda oscuro dentro del Arbol
 *     Deku y con la niebla espesisima fuera --, que es la firma de una causa
 *     global, no de un juego.
 *  2. La profundidad entera se mete en el ultimo cuarto del z-buffer
 *     ([0.75,1] tras el `clip.z += clip.w` del vertex shader), o sea ~2 bits
 *     menos de precision y mas z-fighting.
 *
 * Con la banda a 1 los tres valores vuelven EXACTAMENTE a lo que eran antes
 * del rebase (0.5 / -0.5, y `GXviewportNearZ(n,f)` = n), que es la version
 * que funcionaba.  No se pierde nada que este backend pudiera aprovechar.
 *
 * A 4.0 vuelve el comportamiento de upstream, para el A/B. */
#define GLN64_ZGUARD 1.0

#define GXprojZGuard		 GLN64_ZGUARD
#define GXprojZScale		( 0.5 / GXprojZGuard)  //0.25 //0.5
#define GXprojZOffset		(-0.5 / GXprojZGuard) //-0.5

#define GXviewportNearZ(_n,_f)	((_f) - GXprojZGuard * ((_f) - (_n)))
#define GXpolyOffsetFactor	 5.0e-4 //Tweaked for co-planar polygons. Interestingly, Z resolution should be 5.96e-8.

struct GLVertex
{
	float x, y, z, w;
	struct
	{
		float r, g, b, a;
	} color, secondaryColor;
	float s0, t0, s1, t1;
	float fog;
};

struct GLInfo
{
#ifndef __GX__
#ifndef __LINUX__
	HGLRC	hRC, hPbufferRC;
	HDC		hDC, hPbufferDC;
	HWND	hWnd;
	HPBUFFERARB	hPbuffer;
#else
	SDL_Surface *hScreen;
#endif // __LINUX__
#endif // !__GX__

	DWORD	fullscreenWidth, fullscreenHeight, fullscreenBits, fullscreenRefresh;
	DWORD	width, height, windowedWidth, windowedHeight, heightOffset;

	BOOL	fullscreen, forceBilinear, fog;

	float	scaleX, scaleY;

	BOOL	ATI_texture_env_combine3;	// Radeon
	BOOL	ATIX_texture_env_route;		// Radeon

	BOOL	ARB_multitexture;			// TNT, GeForce, Rage 128, Radeon
	BOOL	ARB_texture_env_combine;	// GeForce, Rage 128, Radeon
	BOOL	ARB_texture_env_crossbar;	// Radeon (GeForce supports it, but doesn't report it)

	BOOL	EXT_fog_coord;				// TNT, GeForce, Rage 128, Radeon
	BOOL	EXT_texture_env_combine;	// TNT, GeForce, Rage 128, Radeon
	BOOL	EXT_secondary_color;		// GeForce, Radeon

	BOOL	NV_texture_env_combine4;	// TNT, GeForce
	BOOL	NV_register_combiners;		// GeForce
	BOOL	ARB_buffer_region;
	BOOL	ARB_pbuffer;
	BOOL	ARB_render_texture;
	BOOL	ARB_pixel_format;

	int		maxTextureUnits;			// TNT = 2, GeForce = 2-4, Rage 128 = 2, Radeon = 3-6
	int		maxGeneralCombiners;

	BOOL	enable2xSaI;
	BOOL	frameBufferTextures;
	int		textureBitDepth;
	float	originAdjust;

	GLVertex vertices[256];
	BYTE	triangles[80][3];
	BYTE	numTriangles;
	BYTE	numVertices;
#ifndef __LINUX__
	HWND	hFullscreenWnd;
#endif
	BOOL	usePolygonStipple;
	GLubyte	stipplePattern[32][8][128];
	BYTE	lastStipple;

	BYTE	combiner;

#ifdef __GX__	//Variables specific to GX
	u32		GXorigX, GXorigY;
	u32		GXwidth, GXheight;
	float	GXscaleX, GXscaleY;
	Mtx44	GXcombW;
	Mtx44	GXprojIdent;
	Mtx44	GXprojTemp;
	Mtx		GXmodelViewIdent;
	BOOL	GXuseCombW;
	BOOL	GXcombWok;
	BOOL	GXupdateMtx;
	int		GXnumVtxMP;
	bool	GXuseAlphaCompare;
	float	GXfogStartZ;
	float	GXfogEndZ;
	GXColor	GXfogColor;
	GXColor GXclearColor;
	u8		GXfogType;
	u8*		GXclearBufferTex;
	bool	GXupdateFog;
	bool	GXpolyOffset;
	bool	GXrenderTexRect;
	bool	GXforceClampS0;
	bool	GXforceClampT0;
	bool	GXforceClampS1;
	bool	GXforceClampT1;
	bool	GXuseMinMagNearest;
	bool	GXclearColorBuffer;
	bool	GXclearDepthBuffer;
#endif
};

extern GLInfo OGL;

struct GLcolor
{
	float r, g, b, a;
};

#ifndef __LINUX__
extern PFNGLCOMBINERPARAMETERFVNVPROC glCombinerParameterfvNV;
extern PFNGLCOMBINERPARAMETERFNVPROC glCombinerParameterfNV;
extern PFNGLCOMBINERPARAMETERIVNVPROC glCombinerParameterivNV;
extern PFNGLCOMBINERPARAMETERINVPROC glCombinerParameteriNV;
extern PFNGLCOMBINERINPUTNVPROC glCombinerInputNV;
extern PFNGLCOMBINEROUTPUTNVPROC glCombinerOutputNV;
extern PFNGLFINALCOMBINERINPUTNVPROC glFinalCombinerInputNV;
extern PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC glGetCombinerInputParameterfvNV;
extern PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC glGetCombinerInputParameterivNV;
extern PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC glGetCombinerOutputParameterfvNV;
extern PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC glGetCombinerOutputParameterivNV;
extern PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC glGetFinalCombinerInputParameterfvNV;
extern PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC glGetFinalCombinerInputParameterivNV;

extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;

extern PFNGLSECONDARYCOLOR3FVEXTPROC glSecondaryColor3fvEXT;

extern PFNGLSECONDARYCOLOR3BEXTPROC glSecondaryColor3bEXT;
extern PFNGLSECONDARYCOLOR3BVEXTPROC glSecondaryColor3bvEXT;
extern PFNGLSECONDARYCOLOR3DEXTPROC glSecondaryColor3dEXT;
extern PFNGLSECONDARYCOLOR3DVEXTPROC glSecondaryColor3dvEXT;
extern PFNGLSECONDARYCOLOR3FEXTPROC glSecondaryColor3fEXT;
extern PFNGLSECONDARYCOLOR3FVEXTPROC glSecondaryColor3fvEXT;
extern PFNGLSECONDARYCOLOR3IEXTPROC glSecondaryColor3iEXT;
extern PFNGLSECONDARYCOLOR3IVEXTPROC glSecondaryColor3ivEXT;
extern PFNGLSECONDARYCOLOR3SEXTPROC	glSecondaryColor3sEXT;
extern PFNGLSECONDARYCOLOR3SVEXTPROC glSecondaryColor3svEXT;
extern PFNGLSECONDARYCOLOR3UBEXTPROC glSecondaryColor3ubEXT;
extern PFNGLSECONDARYCOLOR3UBVEXTPROC glSecondaryColor3ubvEXT;
extern PFNGLSECONDARYCOLOR3UIEXTPROC glSecondaryColor3uiEXT;
extern PFNGLSECONDARYCOLOR3UIVEXTPROC glSecondaryColor3uivEXT;
extern PFNGLSECONDARYCOLOR3USEXTPROC glSecondaryColor3usEXT;
extern PFNGLSECONDARYCOLOR3USVEXTPROC glSecondaryColor3usvEXT;
extern PFNGLSECONDARYCOLORPOINTEREXTPROC glSecondaryColorPointerEXT;

extern PFNWGLCREATEBUFFERREGIONARBPROC wglCreateBufferRegionARB;
extern PFNWGLDELETEBUFFERREGIONARBPROC wglDeleteBufferRegionARB;
extern PFNWGLSAVEBUFFERREGIONARBPROC wglSaveBufferRegionARB;
extern PFNWGLRESTOREBUFFERREGIONARBPROC wglRestoreBufferRegionARB;

extern PFNWGLCREATEPBUFFERARBPROC wglCreatePbufferARB;
extern PFNWGLGETPBUFFERDCARBPROC wglGetPbufferDCARB;
extern PFNWGLRELEASEPBUFFERDCARBPROC wglReleasePbufferDCARB;
extern PFNWGLDESTROYPBUFFERARBPROC wglDestroyPbufferARB;
extern PFNWGLQUERYPBUFFERARBPROC wglQueryPbufferARB;

extern PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB;
extern PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB;
extern PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
#endif // !__LINUX__

bool OGL_Start();
void OGL_Stop();
/* [Xbox360] `primShade`: el "prim shading" de GLideN64 (GraphicsDrawer.cpp:
 * 59-68) -- sin G_LIGHTING ni G_SHADE el color de vertice es gDP.primColor.
 * Solo lo pide gSPTriangle; el resto de llamadores (sprites S2DEX) siguen
 * igual, como en GLideN64, que los dibuja por otro camino.  Ver la nota en
 * OGL_AddTriangle. */
void OGL_AddTriangle( SPVertex *vertices, int v0, int v1, int v2, bool primShade = false );
void OGL_DrawTriangles();
void OGL_DrawLine( SPVertex *vertices, int v0, int v1, float width );
void OGL_DrawRect( int ulx, int uly, int lrx, int lry, float *color );
void OGL_DrawDepthFill( int ulx, int uly, int lrx, int lry, float depth );
// [Xbox360] Screen-space triangles (N64 pixel coordinates), drawn as the strip
// (v[i], v[i+1], v[i+2]) GLideN64's drawScreenSpaceTriangle gets from LLETriangle.
void OGL_DrawScreenSpaceTriangles( SPVertex *vertices, u32 count, BOOL textured );
void OGL_DrawTexturedRect( float ulx, float uly, float lrx, float lry, float uls, float ult, float lrs, float lrt, bool flip, const float *colorOverride = NULL );
void OGL_UpdateScale();
void OGL_UpdateStates();
void OGL_UpdateCullFace();
void OGL_UpdateViewport();
void OGL_ClearDepthBuffer();
void OGL_ClearColorBuffer( float *color );
void OGL_ResizeWindow();
void OGL_SaveScreenshot();
#ifdef __LINUX__
void OGL_SwapBuffers();
#endif // __LINUX__
void OGL_ReadScreen( void **dest, long *width, long *height );
#ifdef __GX__
void OGL_GXinitDlist();
void OGL_GXclearEFB();
void OGL_ApplyPendingClears();
#endif // __GX__

#endif
