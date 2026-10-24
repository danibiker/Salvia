/**
 * glN64_GX - OpenGL.cpp
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009, 2010 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifdef __GX__
#include <gccore.h>
extern "C" {
#include "../main/gamehacks.h"
}
#include "../gui/DEBUG.h"
#include <math.h>
#include "3DMath.h"
#include <malloc.h>
#endif // __GX__

#ifndef __LINUX__
# include <windows.h>
# include <GL/gl.h>
# include "glext.h"
#else // !__LINUX__
# include "../main/winlnxdefs.h"
#define GL_GLEXT_PROTOTYPES
#define __WIN32__
#include "gl.h" 
#include "glext.h"
#undef __WIN32__
//# include <GL/gl.h>
//# include <GL/glext.h>
#ifndef __GX__
# include "SDL.h"
#endif // !__GX__
# include <string.h>
# include <time.h>
# include <stdlib.h>

# ifndef min
#  define min(a,b) ((a) < (b) ? (a) : (b))
# endif
# ifndef max
#  define max(a,b) ((a) > (b) ? (a) : (b))
# endif
# define timeGetTime() time(NULL)

#endif // __LINUX__
#include <math.h>
#include <stdio.h>
#include "../xenon64_diag.h"   /* XBOX64_TRAZAS */
#include "glN64.h"
#include "OpenGL.h"
#include "Types.h"
#include "N64.h"
#include "gSP.h"
#include "gDP.h"
#include "Textures.h"
#include "Combiner.h"
#include "Config.h"	// [Xbox360] hack_n64DepthCompareDec (ISS64)
#include "VI.h"
#include "DepthBuffer.h"	// [Xbox360] OGL_DrawScreenSpaceTriangles
#include <string.h>

#ifndef GL_BGR
#define GL_BGR GL_BGR_EXT
#endif

#ifdef DEBUGON
extern "C" { void _break(); }
#endif

#ifdef __GX__
/* [Xbox360] `extern "C"`: las define xenon/gln64_glue.cpp, que incluye
 * `main/wii64config.h` -- y esa cabecera ya lleva enlace C.  Aqui se
 * declaran A MANO, sin pasar por ella, asi que hay que decirlo o se busca
 * el simbolo decorado y no aparece (LNK2001). */
extern "C" char glN64_useFrameBufferTextures;
extern "C" char glN64_use2xSaiTextures;

/* [Xbox360] El camino de LOTE ENTERO del backend D3D9: `OGL_DrawTriangles`
 * escribe sus vertices directamente sobre el vertex buffer bloqueado en vez
 * de empujarlos de cinco en cinco por el modo inmediato de GX.  La cabecera
 * no depende de nada y trae la definicion del vertice, que es UNICA para el
 * plugin y el shim. */
#include "../../xenon/gxd3d_lote.h"
#endif // __GX__

GLInfo OGL;

extern GXRModeObj *vmode, *rmode;

#ifndef __LINUX__
// NV_register_combiners functions
PFNGLCOMBINERPARAMETERFVNVPROC glCombinerParameterfvNV;
PFNGLCOMBINERPARAMETERFNVPROC glCombinerParameterfNV;
PFNGLCOMBINERPARAMETERIVNVPROC glCombinerParameterivNV;
PFNGLCOMBINERPARAMETERINVPROC glCombinerParameteriNV;
PFNGLCOMBINERINPUTNVPROC glCombinerInputNV;
PFNGLCOMBINEROUTPUTNVPROC glCombinerOutputNV;
PFNGLFINALCOMBINERINPUTNVPROC glFinalCombinerInputNV;
PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC glGetCombinerInputParameterfvNV;
PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC glGetCombinerInputParameterivNV;
PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC glGetCombinerOutputParameterfvNV;
PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC glGetCombinerOutputParameterivNV;
PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC glGetFinalCombinerInputParameterfvNV;
PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC glGetFinalCombinerInputParameterivNV;

// ARB_multitexture functions
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;

// EXT_fog_coord functions
PFNGLFOGCOORDFEXTPROC glFogCoordfEXT;
PFNGLFOGCOORDFVEXTPROC glFogCoordfvEXT;
PFNGLFOGCOORDDEXTPROC glFogCoorddEXT;
PFNGLFOGCOORDDVEXTPROC glFogCoorddvEXT;
PFNGLFOGCOORDPOINTEREXTPROC glFogCoordPointerEXT;

// EXT_secondary_color functions
PFNGLSECONDARYCOLOR3BEXTPROC glSecondaryColor3bEXT;
PFNGLSECONDARYCOLOR3BVEXTPROC glSecondaryColor3bvEXT;
PFNGLSECONDARYCOLOR3DEXTPROC glSecondaryColor3dEXT;
PFNGLSECONDARYCOLOR3DVEXTPROC glSecondaryColor3dvEXT;
PFNGLSECONDARYCOLOR3FEXTPROC glSecondaryColor3fEXT;
PFNGLSECONDARYCOLOR3FVEXTPROC glSecondaryColor3fvEXT;
PFNGLSECONDARYCOLOR3IEXTPROC glSecondaryColor3iEXT;
PFNGLSECONDARYCOLOR3IVEXTPROC glSecondaryColor3ivEXT;
PFNGLSECONDARYCOLOR3SEXTPROC glSecondaryColor3sEXT;
PFNGLSECONDARYCOLOR3SVEXTPROC glSecondaryColor3svEXT;
PFNGLSECONDARYCOLOR3UBEXTPROC glSecondaryColor3ubEXT;
PFNGLSECONDARYCOLOR3UBVEXTPROC glSecondaryColor3ubvEXT;
PFNGLSECONDARYCOLOR3UIEXTPROC glSecondaryColor3uiEXT;
PFNGLSECONDARYCOLOR3UIVEXTPROC glSecondaryColor3uivEXT;
PFNGLSECONDARYCOLOR3USEXTPROC glSecondaryColor3usEXT;
PFNGLSECONDARYCOLOR3USVEXTPROC glSecondaryColor3usvEXT;
PFNGLSECONDARYCOLORPOINTEREXTPROC glSecondaryColorPointerEXT;
#endif // !__LINUX__

BOOL isExtensionSupported( const char *extension )
{
#ifndef __GX__
	const GLubyte *extensions = NULL;
	const GLubyte *start;
	GLubyte *where, *terminator;

	where = (GLubyte *) strchr(extension, ' ');
	if (where || *extension == '\0')
		return 0;

	extensions = glGetString(GL_EXTENSIONS);

	start = extensions;
	for (;;)
	{
		where = (GLubyte *) strstr((const char *) start, extension);
		if (!where)
			break;

		terminator = where + strlen(extension);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return TRUE;

		start = terminator;
	}
#endif // !__GX__
	return FALSE;
}

void OGL_InitExtensions()
{
	if ((OGL.NV_register_combiners = isExtensionSupported( "GL_NV_register_combiners" )))
	{
#ifndef __LINUX__
		glCombinerParameterfvNV = (PFNGLCOMBINERPARAMETERFVNVPROC)wglGetProcAddress( "glCombinerParameterfvNV" );
		glCombinerParameterfNV = (PFNGLCOMBINERPARAMETERFNVPROC)wglGetProcAddress( "glCombinerParameterfNV" );
		glCombinerParameterivNV = (PFNGLCOMBINERPARAMETERIVNVPROC)wglGetProcAddress( "glCombinerParameterivNV" );
		glCombinerParameteriNV = (PFNGLCOMBINERPARAMETERINVPROC)wglGetProcAddress( "glCombinerParameteriNV" );
		glCombinerInputNV = (PFNGLCOMBINERINPUTNVPROC)wglGetProcAddress( "glCombinerInputNV" );
		glCombinerOutputNV = (PFNGLCOMBINEROUTPUTNVPROC)wglGetProcAddress( "glCombinerOutputNV" );
		glFinalCombinerInputNV = (PFNGLFINALCOMBINERINPUTNVPROC)wglGetProcAddress( "glFinalCombinerInputNV" );
		glGetCombinerInputParameterfvNV = (PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC)wglGetProcAddress( "glGetCombinerInputParameterfvNV" );
		glGetCombinerInputParameterivNV = (PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC)wglGetProcAddress( "glGetCombinerInputParameterivNV" );
		glGetCombinerOutputParameterfvNV = (PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC)wglGetProcAddress( "glGetCombinerOutputParameterfvNV" );
		glGetCombinerOutputParameterivNV = (PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC)wglGetProcAddress( "glGetCombinerOutputParameterivNV" );
		glGetFinalCombinerInputParameterfvNV = (PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC)wglGetProcAddress( "glGetFinalCombinerInputParameterfvNV" );
		glGetFinalCombinerInputParameterivNV = (PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC)wglGetProcAddress( "glGetFinalCombinerInputParameterivNV" );
#endif // !__LINUX__
#ifndef __GX__ 
		glGetIntegerv( GL_MAX_GENERAL_COMBINERS_NV, &OGL.maxGeneralCombiners );
#endif // !__GX__
	}

	if ((OGL.ARB_multitexture = isExtensionSupported( "GL_ARB_multitexture" )))
	{
#ifndef __LINUX__
		glActiveTextureARB			= (PFNGLACTIVETEXTUREARBPROC)wglGetProcAddress( "glActiveTextureARB" );
		glClientActiveTextureARB	= (PFNGLCLIENTACTIVETEXTUREARBPROC)wglGetProcAddress( "glClientActiveTextureARB" );
		glMultiTexCoord2fARB		= (PFNGLMULTITEXCOORD2FARBPROC)wglGetProcAddress( "glMultiTexCoord2fARB" );
#endif // !__LINUX__
#ifndef __GX__
		glGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &OGL.maxTextureUnits );
		OGL.maxTextureUnits = min( 8, OGL.maxTextureUnits ); // The plugin only supports 8, and 4 is really enough
#endif // !__GX__
	}
#ifdef __GX__
	OGL.maxTextureUnits = 8;
	OGL.ARB_multitexture = TRUE;
#endif // __GX__


	if ((OGL.EXT_fog_coord = isExtensionSupported( "GL_EXT_fog_coord" )))
	{
#ifndef __LINUX__
		glFogCoordfEXT = (PFNGLFOGCOORDFEXTPROC)wglGetProcAddress( "glFogCoordfEXT" );
		glFogCoordfvEXT = (PFNGLFOGCOORDFVEXTPROC)wglGetProcAddress( "glFogCoordfvEXT" );
		glFogCoorddEXT = (PFNGLFOGCOORDDEXTPROC)wglGetProcAddress( "glFogCoorddEXT" );
		glFogCoorddvEXT = (PFNGLFOGCOORDDVEXTPROC)wglGetProcAddress( "glFogCoorddvEXT" );
		glFogCoordPointerEXT = (PFNGLFOGCOORDPOINTEREXTPROC)wglGetProcAddress( "glFogCoordPointerEXT" );
#endif // !__LINUX__
	}

	if ((OGL.EXT_secondary_color = isExtensionSupported( "GL_EXT_secondary_color" )))
	{
#ifndef __LINUX__
		glSecondaryColor3bEXT = (PFNGLSECONDARYCOLOR3BEXTPROC)wglGetProcAddress( "glSecondaryColor3bEXT" );
		glSecondaryColor3bvEXT = (PFNGLSECONDARYCOLOR3BVEXTPROC)wglGetProcAddress( "glSecondaryColor3bvEXT" );
		glSecondaryColor3dEXT = (PFNGLSECONDARYCOLOR3DEXTPROC)wglGetProcAddress( "glSecondaryColor3dEXT" );
		glSecondaryColor3dvEXT = (PFNGLSECONDARYCOLOR3DVEXTPROC)wglGetProcAddress( "glSecondaryColor3dvEXT" );
		glSecondaryColor3fEXT = (PFNGLSECONDARYCOLOR3FEXTPROC)wglGetProcAddress( "glSecondaryColor3fEXT" );
		glSecondaryColor3fvEXT = (PFNGLSECONDARYCOLOR3FVEXTPROC)wglGetProcAddress( "glSecondaryColor3fvEXT" );
		glSecondaryColor3iEXT = (PFNGLSECONDARYCOLOR3IEXTPROC)wglGetProcAddress( "glSecondaryColor3iEXT" );
		glSecondaryColor3ivEXT = (PFNGLSECONDARYCOLOR3IVEXTPROC)wglGetProcAddress( "glSecondaryColor3ivEXT" );
		glSecondaryColor3sEXT = (PFNGLSECONDARYCOLOR3SEXTPROC)wglGetProcAddress( "glSecondaryColor3sEXT" );
		glSecondaryColor3svEXT = (PFNGLSECONDARYCOLOR3SVEXTPROC)wglGetProcAddress( "glSecondaryColor3svEXT" );
		glSecondaryColor3ubEXT = (PFNGLSECONDARYCOLOR3UBEXTPROC)wglGetProcAddress( "glSecondaryColor3ubEXT" );
		glSecondaryColor3ubvEXT = (PFNGLSECONDARYCOLOR3UBVEXTPROC)wglGetProcAddress( "glSecondaryColor3ubvEXT" );
		glSecondaryColor3uiEXT = (PFNGLSECONDARYCOLOR3UIEXTPROC)wglGetProcAddress( "glSecondaryColor3uiEXT" );
		glSecondaryColor3uivEXT = (PFNGLSECONDARYCOLOR3UIVEXTPROC)wglGetProcAddress( "glSecondaryColor3uivEXT" );
		glSecondaryColor3usEXT = (PFNGLSECONDARYCOLOR3USEXTPROC)wglGetProcAddress( "glSecondaryColor3usEXT" );
		glSecondaryColor3usvEXT = (PFNGLSECONDARYCOLOR3USVEXTPROC)wglGetProcAddress( "glSecondaryColor3usvEXT" );
		glSecondaryColorPointerEXT = (PFNGLSECONDARYCOLORPOINTEREXTPROC)wglGetProcAddress( "glSecondaryColorPointerEXT" );
#endif // !__LINUX__
	}

	OGL.ARB_texture_env_combine = isExtensionSupported( "GL_ARB_texture_env_combine" );
	OGL.ARB_texture_env_crossbar = isExtensionSupported( "GL_ARB_texture_env_crossbar" );
	OGL.EXT_texture_env_combine = isExtensionSupported( "GL_EXT_texture_env_combine" );
	OGL.ATI_texture_env_combine3 = isExtensionSupported( "GL_ATI_texture_env_combine3" );
	OGL.ATIX_texture_env_route = isExtensionSupported( "GL_ATIX_texture_env_route" );
	OGL.NV_texture_env_combine4 = isExtensionSupported( "GL_NV_texture_env_combine4" );;
}

void OGL_InitStates()
{
#ifndef __GX__
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	glVertexPointer( 4, GL_FLOAT, sizeof( GLVertex ), &OGL.vertices[0].x );
	glEnableClientState( GL_VERTEX_ARRAY );

	glColorPointer( 4, GL_FLOAT, sizeof( GLVertex ), &OGL.vertices[0].color.r );
	glEnableClientState( GL_COLOR_ARRAY );

	if (OGL.EXT_secondary_color)
	{
		glSecondaryColorPointerEXT( 3, GL_FLOAT, sizeof( GLVertex ), &OGL.vertices[0].secondaryColor.r );
		glEnableClientState( GL_SECONDARY_COLOR_ARRAY_EXT );
	}

	if (OGL.ARB_multitexture)
	{
		glClientActiveTextureARB( GL_TEXTURE0_ARB );
		glTexCoordPointer( 2, GL_FLOAT, sizeof( GLVertex ), &OGL.vertices[0].s0 );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );

		glClientActiveTextureARB( GL_TEXTURE1_ARB );
		glTexCoordPointer( 2, GL_FLOAT, sizeof( GLVertex ), &OGL.vertices[0].s1 );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	}
	else
	{
		glTexCoordPointer( 2, GL_FLOAT, sizeof( GLVertex ), &OGL.vertices[0].s0 );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	if (OGL.EXT_fog_coord)
	{
		glFogi( GL_FOG_COORDINATE_SOURCE_EXT, GL_FOG_COORDINATE_EXT );

		glFogi( GL_FOG_MODE, GL_LINEAR );
		glFogf( GL_FOG_START, 0.0f );
		glFogf( GL_FOG_END, 255.0f );

		glFogCoordPointerEXT( GL_FLOAT, sizeof( GLVertex ), &OGL.vertices[0].fog );
		glEnableClientState( GL_FOG_COORDINATE_ARRAY_EXT );
	}

	glPolygonOffset( -3.0f, -3.0f );

	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );
#else // !__GX__
	// TODO: Init GX variables here...
#endif // __GX__

	srand( timeGetTime() );

	for (int i = 0; i < 32; i++)
	{
		for (int j = 0; j < 8; j++)
			for (int k = 0; k < 128; k++)
				OGL.stipplePattern[i][j][k] =((i > (rand() >> 10)) << 7) |
											((i > (rand() >> 10)) << 6) |
											((i > (rand() >> 10)) << 5) |
											((i > (rand() >> 10)) << 4) |
											((i > (rand() >> 10)) << 3) |
											((i > (rand() >> 10)) << 2) |
											((i > (rand() >> 10)) << 1) |
											((i > (rand() >> 10)) << 0);
	}

#ifndef __LINUX__
	SwapBuffers( wglGetCurrentDC() );
#else
	OGL_SwapBuffers();
#endif
}

void OGL_UpdateScale()
{
	OGL.scaleX = OGL.width / (float)VI.width;
	OGL.scaleY = OGL.height / (float)VI.height;
#ifdef __GX__
	OGL.GXscaleX = (float)OGL.GXwidth / (float)VI.width;
	OGL.GXscaleY = (float)OGL.GXheight / (float)VI.height;
#endif //__GX__
}

void OGL_ResizeWindow()
{
#ifndef __LINUX__
	RECT	windowRect, statusRect, toolRect;

	if (OGL.fullscreen)
	{
		OGL.width = OGL.fullscreenWidth;
		OGL.height = OGL.fullscreenHeight;
		OGL.heightOffset = 0;

		SetWindowPos( hWnd, NULL, 0, 0,	OGL.width, OGL.height, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	}
	else
	{
		OGL.width = OGL.windowedWidth;
		OGL.height = OGL.windowedHeight;

		GetClientRect( hWnd, &windowRect );
		GetWindowRect( hStatusBar, &statusRect );

		if (hToolBar)
			GetWindowRect( hToolBar, &toolRect );
		else
			toolRect.bottom = toolRect.top = 0;

		OGL.heightOffset = (statusRect.bottom - statusRect.top);
		windowRect.right = windowRect.left + OGL.windowedWidth - 1;
		windowRect.bottom = windowRect.top + OGL.windowedHeight - 1 + OGL.heightOffset;

		AdjustWindowRect( &windowRect, GetWindowLong( hWnd, GWL_STYLE ), GetMenu( hWnd ) != NULL );

		SetWindowPos( hWnd, NULL, 0, 0,	windowRect.right - windowRect.left + 1,
						windowRect.bottom - windowRect.top + 1 + toolRect.bottom - toolRect.top + 1, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE );
	}
#else // !__LINUX__
#endif // __LINUX__
	// This is mainly initializing OGL.heightOffset because I don't think it's inited otherwise.
	OGL.fullscreen = true;
	OGL.fullscreenWidth = 640;
	OGL.fullscreenHeight = 480;
	OGL.windowedWidth = 640;
	OGL.windowedHeight = 480;
	OGL.heightOffset = 0;
#ifdef __GX__

#endif // __GX__
}

bool OGL_Start()
{
#ifndef __GX__
#ifndef __LINUX__
	int		pixelFormat;

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),    // size of this pfd
		1,                                // version number
		PFD_DRAW_TO_WINDOW |              // support window
		PFD_SUPPORT_OPENGL |              // support OpenGL
		PFD_DOUBLEBUFFER,                 // double buffered
		PFD_TYPE_RGBA,                    // RGBA type
		32,								  // color depth
		0, 0, 0, 0, 0, 0,                 // color bits ignored
		0,                                // no alpha buffer
		0,                                // shift bit ignored
		0,                                // no accumulation buffer
		0, 0, 0, 0,                       // accum bits ignored
		32,								  // z-buffer
		0,                                // no stencil buffer
		0,                                // no auxiliary buffer
		PFD_MAIN_PLANE,                   // main layer
		0,                                // reserved
		0, 0, 0                           // layer masks ignored
	};

	if ((OGL.hDC = GetDC( hWnd )) == NULL)
	{
		MessageBox( hWnd, "Error while getting a device context!", pluginName, MB_ICONERROR | MB_OK );
		return FALSE;
	}

	if ((pixelFormat = ChoosePixelFormat( OGL.hDC, &pfd )) == 0)
	{
		MessageBox( hWnd, "Unable to find a suitable pixel format!", pluginName, MB_ICONERROR | MB_OK );
		OGL_Stop();
		return FALSE;
	}

	if ((SetPixelFormat( OGL.hDC, pixelFormat, &pfd )) == FALSE)
	{
		MessageBox( hWnd, "Error while setting pixel format!", pluginName, MB_ICONERROR | MB_OK );
		OGL_Stop();
		return FALSE;
	}

	if ((OGL.hRC = wglCreateContext( OGL.hDC )) == NULL)
	{
		MessageBox( hWnd, "Error while creating OpenGL context!", pluginName, MB_ICONERROR | MB_OK );
		OGL_Stop();
		return FALSE;
	}

	if ((wglMakeCurrent( OGL.hDC, OGL.hRC )) == FALSE)
	{
		MessageBox( hWnd, "Error while making OpenGL context current!", pluginName, MB_ICONERROR | MB_OK );
		OGL_Stop();
		return FALSE;
	}
#else // !__LINUX__
	// init sdl & gl
	const SDL_VideoInfo *videoInfo;
	Uint32 videoFlags = 0;

	if (OGL.fullscreen)
	{
		OGL.width = OGL.fullscreenWidth;
		OGL.height = OGL.fullscreenHeight;
	}
	else
	{
		OGL.width = OGL.windowedWidth;
		OGL.height = OGL.windowedHeight;
	}


	/* Initialize SDL */
	printf( "[glN64]: (II) Initializing SDL video subsystem...\n" );
	if (SDL_InitSubSystem( SDL_INIT_VIDEO ) == -1)
	{
		printf( "[glN64]: (EE) Error initializing SDL video subsystem: %s\n", SDL_GetError() );
		return FALSE;
	}

	/* Video Info */
	printf( "[glN64]: (II) Getting video info...\n" );
	if (!(videoInfo = SDL_GetVideoInfo()))
	{
		printf( "[glN64]: (EE) Video query failed: %s\n", SDL_GetError() );
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
		return FALSE;
	}

	/* Set the video mode */
	videoFlags |= SDL_OPENGL | SDL_GL_DOUBLEBUFFER | SDL_HWPALETTE;

	if (videoInfo->hw_available)
		videoFlags |= SDL_HWSURFACE;
	else
		videoFlags |= SDL_SWSURFACE;

	if (videoInfo->blit_hw)
		videoFlags |= SDL_HWACCEL;

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
/*	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );*/
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );	// 32 bit z-buffer

	printf( "[glN64]: (II) Setting video mode %dx%d...\n", (int)OGL.width, (int)OGL.height );
	if (!(OGL.hScreen = SDL_SetVideoMode( OGL.width, OGL.height, 0, videoFlags )))
	{
		printf( "[glN64]: (EE) Error setting videomode %dx%d: %s\n", (int)OGL.width, (int)OGL.height, SDL_GetError() );
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
		return FALSE;
	}

	SDL_WM_SetCaption( pluginName, pluginName );
#endif // __LINUX__
#else // !__GX__
	//Set 'window height' to efb dimensions
	OGL.width = rmode->fbWidth;
	OGL.height = rmode->efbHeight;

	//Reset gDP and combiner colors between ROMs. This fixes fillColor when changing ROMs.
	combiner.vertex.color = ZERO;
	combiner.vertex.secondaryColor = ZERO;
	combiner.vertex.alpha = ZERO;
	gDP.primColor.m = 0;
	gDP.primColor.r = 0.0f;
	gDP.primColor.g = 0.0f;
	gDP.primColor.b = 0.0f;
	gDP.primColor.a = 0.0f;
	gDP.primColor.l = 0.0f;
	gDP.envColor.r = 0.0f;
	gDP.envColor.g = 0.0f;
	gDP.envColor.b = 0.0f;
	gDP.envColor.a = 0.0f;
	gDP.fogColor.r = 0.0f;
	gDP.fogColor.g = 0.0f;
	gDP.fogColor.b = 0.0f;
	gDP.fogColor.a = 0.0f;
	gDP.blendColor.r = 0.0f;
	gDP.blendColor.g = 0.0f;
	gDP.blendColor.b = 0.0f;
	gDP.blendColor.a = 0.0f;
	gDP.fillColor.r = 0.0f;
	gDP.fillColor.g = 0.0f;
	gDP.fillColor.b = 0.0f;
	gDP.fillColor.a = 0.0f;
	gDP.fillColor.z = 0.0f;
	gDP.fillColor.dz = 0.0f;
	gDP.primDepth.z = 0.0f;
	gDP.primDepth.deltaZ = 0.0f;
#endif // __GX__
	OGL_InitExtensions();
	OGL_InitStates();

	TextureCache_Init();
	FrameBuffer_Init();
	Combiner_Init();

	gSP.changed = gDP.changed = 0xFFFFFFFF;
	OGL_UpdateScale();

	return TRUE;
}

void OGL_Stop()
{
	Combiner_Destroy();
	FrameBuffer_Destroy();
	TextureCache_Destroy();

#ifndef __GX__
#ifndef __LINUX__
	wglMakeCurrent( NULL, NULL );

	if (OGL.hRC)
	{
		wglDeleteContext( OGL.hRC );
		OGL.hRC = NULL;
	}

	if (OGL.hDC)
	{
		ReleaseDC( hWnd, OGL.hDC );
		OGL.hDC = NULL;
	}
#else // !__LINUX__
	SDL_QuitSubSystem( SDL_INIT_VIDEO );
	OGL.hScreen = NULL;
#endif // __LINUX__
#endif // !__GX__
}

void OGL_UpdateCullFace()
{
#ifndef __GX__
	if (gSP.geometryMode & G_CULL_BOTH)
	{
		glEnable( GL_CULL_FACE );

		if (gSP.geometryMode & G_CULL_BACK)
			glCullFace( GL_BACK );
		else
			glCullFace( GL_FRONT );
	}
	else
		glDisable( GL_CULL_FACE );
#else // !__GX__
	if (gSP.geometryMode & G_CULL_BOTH)
	{
		if (gSP.geometryMode & G_CULL_BACK)
			GX_SetCullMode (GX_CULL_FRONT);	// GC vertex winding is backwards.
		else
			GX_SetCullMode (GX_CULL_BACK);	// GC vertex winding is backwards.
	}
	else
		GX_SetCullMode (GX_CULL_NONE);
#endif // __GX__
}

void OGL_UpdateViewport()
{
#ifndef __GX__
	glViewport( (int)(gSP.viewport.x * OGL.scaleX), (int)((VI.height - (gSP.viewport.y + gSP.viewport.height)) * OGL.scaleY + OGL.heightOffset),
	            (int)(gSP.viewport.width * OGL.scaleX), (int)(gSP.viewport.height * OGL.scaleY) );
	glDepthRange( 0.0f, 1.0f );//gSP.viewport.nearz, gSP.viewport.farz );
#else // !__GX__
	const f32 GXfarZ = OGL.GXpolyOffset ? (gSP.viewport.farz - GXpolyOffsetFactor) : gSP.viewport.farz;
	const f32 GXnearZ = GXviewportNearZ(gSP.viewport.nearz, GXfarZ);
	GX_SetViewport((f32) (OGL.GXorigX + gSP.viewport.x * OGL.GXscaleX),(f32) (OGL.GXorigY + gSP.viewport.y * OGL.GXscaleY),
		(f32) (gSP.viewport.width * OGL.GXscaleX),(f32) (gSP.viewport.height * OGL.GXscaleY), GXnearZ, GXfarZ);
#endif // __GX__
}

void OGL_UpdateDepthUpdate()
{
#ifndef __GX__
	if (gDP.otherMode.depthUpdate)
		glDepthMask( TRUE );
	else
		glDepthMask( FALSE );
#else // !__GX__
	//This should now be taken care of in OGL_UpdateStates()
#endif // __GX__
}

#ifdef __GX__
/* [Xbox360] ★★ [Nightmare Creatures, escenario negro] LA NIEBLA LA DECIDE EL
 * BLENDER, como GLideN64 sin `enableLegacyBlending` (su defecto).
 *
 * G_FOG solo hace que el RSP escriba el coeficiente en el alfa del vertice
 * (GLideN64: `vShadeColor.a = fp`, glsl_CombinerProgramBuilderCommon.cpp); el
 * color se mezcla con el de niebla SOLO si la formula del blender lo pide.
 * glN64 lo mezclaba siempre como `lerp(color, niebla, A)`, que es el
 * ShaderLegacyBlender de GLideN64.  ★RESUELTO (log 38; controles 007, Conker,
 * Doom 64 y OoT bien).  Medido (Logs Salvia (37), sonda [NIEBLA] retirada):
 *   0AF0 = CLR_IN * A_SHADE + CLR_FOG * (1 - A): el peso va AL REVES (A = 1
 *          es sin niebla; mul=-5120 off=5120 satura lo CERCANO a 255) y se
 *          pintaba negro justo lo cercano.
 *   0050 = CLR_IN * A_IN + CLR_MEM * (1 - A): sin niebla en el blender.
 * Devuelve 0 = sin niebla de color, 1 = `lerp(c, niebla, A_SHADE)` (lo de
 * siempre), 2 = `lerp(c, niebla, 1 - A_SHADE)`.  Si el blender usa CLR_FOG
 * con otro peso (A_FOG...), se queda el 1 de antes.
 *
 * Los muxes se leen de los bits crudos (GBL_c1 = bits 31,27,23,19 del
 * othermode L, GBL_c2 = 29,25,21,17), no de los bitfields: P/M 3 = CLR_FOG,
 * A 2 = A_SHADE, B 0 = 1 - A.  En ciclo doble la niebla va en el primer ciclo;
 * si no esta ahi se mira el segundo. */
static int OGL_GX_NieblaDelBlender()
{
	const u32 bl = gDP.otherMode.l >> 16;
	int c;
	for (c = 0; c < 2; c++)
	{
		const u32 sh = (c == 0) ? 2 : 0;
		const u32 m1a = (bl >> (12 + sh)) & 3;
		const u32 m1b = (bl >> ( 8 + sh)) & 3;
		const u32 m2a = (bl >> ( 4 + sh)) & 3;
		const u32 m2b = (bl >> ( 0 + sh)) & 3;
		if (m1a == 3 || m2a == 3)
		{
			if (m2a == 3 && m1a != 3 && m1b == 2 && m2b == 0)
				return 2;
			return 1;
		}
		if (gDP.otherMode.cycleType != G_CYC_2CYCLE)
			break;
	}
	return 0;
}
#endif // __GX__

/* [Xbox360] [Resident Evil 2] La esquina inferior derecha de la tijera.
 * GLideN64 5e51b386, GraphicsDrawer::updateScissor:
 *     if (u32(SX1) == 512 && (config.generalEmulation.hacks & hack_RE2) != 0) {
 *         SX1 = f32(*REG.VI_WIDTH);
 *         SY1 *= 512.0f / SX1;
 *     }
 * Los buffers de 512 de ancho son del ancho del VI con mas filas (ver
 * _gSPFondoRE2 en gSP.cpp).  Solo en las tijeras de GX, como alli en la de GL:
 * gDP.scissor no se toca. */
static void _oglTijeraLR( f32 *lrx, f32 *lry )
{
	*lrx = gDP.scissor.lrx;
	*lry = gDP.scissor.lry;
	if ((config.generalEmulation.hacks & hack_RE2) != 0 && (u32)*lrx == 512 && *REG.VI_WIDTH != 0)
	{
		*lrx = (f32)*REG.VI_WIDTH;
		*lry *= 512.0f / *lrx;
	}
}

void OGL_UpdateStates()
{
	GLN64_PROF_SCOPE(updst);   /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
#ifdef __GX__
	/* [Xbox360] [Paper Mario] La carga desde la RDRAM de un buffer nuevo, si
	 * quedo aplazada: ANTES de fijar ningun estado, porque dibuja con el suyo
	 * y marca gSP/gDP.changed.  Ver FrameBuffer_GX_CargaPendiente. */
	FrameBuffer_GX_CargaPendiente();
#endif // __GX__
#ifdef __GX__
	if (OGL.GXclearColorBuffer || OGL.GXclearDepthBuffer)
		OGL_GXclearEFB();
#endif //__GX__

	if (gSP.changed & CHANGED_GEOMETRYMODE)
	{
		OGL_UpdateCullFace();
#ifndef __GX__
		if ((gSP.geometryMode & G_FOG) && OGL.EXT_fog_coord && OGL.fog)
			glEnable( GL_FOG );
		else
			glDisable( GL_FOG );
#else // !__GX__
		if ((gSP.geometryMode & G_FOG) && OGL.fog)
			OGL.GXfogType = GX_FOG_ORTHO_LIN;
		else
			OGL.GXfogType = GX_FOG_NONE;
		OGL.GXupdateFog = true;
#endif // __GX__

		gSP.changed &= ~CHANGED_GEOMETRYMODE;
	}

#ifndef __GX__
	if (gSP.geometryMode & G_ZBUFFER)
		glEnable( GL_DEPTH_TEST );
	else
		glDisable( GL_DEPTH_TEST );

	if (gDP.changed & CHANGED_RENDERMODE)
	{
		if (gDP.otherMode.depthCompare)
			glDepthFunc( GL_LEQUAL );
		else
			glDepthFunc( GL_ALWAYS );

		OGL_UpdateDepthUpdate();

		if (gDP.otherMode.depthMode == ZMODE_DEC)
			glEnable( GL_POLYGON_OFFSET_FILL );
		else
		{
//			glPolygonOffset( -3.0f, -3.0f );
			glDisable( GL_POLYGON_OFFSET_FILL );
		}
	}
#else // !__GX__
	//Zbuffer settings
	static u8 GXenableZmode, GXZfunc = GX_ALWAYS, GXZupdate = GX_FALSE;
	if (((gSP.geometryMode & G_ZBUFFER) || (gDP.otherMode.depthSource == G_ZS_PRIM)) &&
	    gDP.otherMode.cycleType <= G_CYC_2CYCLE)
		GXenableZmode = GX_ENABLE;
	else
		GXenableZmode = GX_DISABLE;

	if (gDP.changed & CHANGED_RENDERMODE)
	{
		if (gDP.otherMode.depthCompare)
//			glDepthFunc( GL_LEQUAL );
			GXZfunc = GX_LEQUAL;
		else
//			glDepthFunc( GL_ALWAYS );
			GXZfunc = GX_ALWAYS;

//		OGL_UpdateDepthUpdate();
		if (gDP.otherMode.depthUpdate)
//			glDepthMask( TRUE );
			GXZupdate = GX_TRUE;
		else
//			glDepthMask( FALSE );
			GXZupdate = GX_FALSE;

		/* [Xbox360] Con el decal del N64 por shader (abajo) no hay offset de
		 * poligono: GLideN64 con N64DepthCompare tampoco lo pone
		 * (_updateDepthCompare apaga el test y el offset entero). */
		const BOOL decConOffset = (gDP.otherMode.depthMode == ZMODE_DEC &&
		                           (config.generalEmulation.hacks & hack_n64DepthCompareDec) == 0) ? TRUE : FALSE;
		if (decConOffset && !OGL.GXpolyOffset)
		{
			OGL.GXpolyOffset = true;
			OGL.GXupdateMtx = true;
		}
		else if (!decConOffset && OGL.GXpolyOffset)
		{
			OGL.GXpolyOffset = false;
			OGL.GXupdateMtx = true;
		}
	}
	/* [Xbox360] ★★ [ISS64, sombras] EL MODO DECAL DEL N64, POR SHADER, solo en
	 * los juegos con frameBufferEmulation\N64DepthCompare=1 en
	 * GLideN64.custom.ini (GLideN64 2cbf2b52 "Fix ISS64 shadows";
	 * hack_n64DepthCompareDec en RSP.cpp).
	 *
	 * El juego deja la silueta de la sombra en el Z con poligonos invisibles
	 * coplanarios, y luego pinta UN poligono solido en decal que solo debe
	 * pasar dentro de la silueta.  El decal del N64 pasa si la z esta a la par
	 * de la del buffer por los dos lados; el offset + LEQUAL de glN64 solo mira
	 * uno y dejaba pasar todo el suelo de detras: el rectangulo.
	 *
	 * GLideN64 con N64DepthCompare hace TODA la prueba de Z en el shader con
	 * su propia imagen de profundidad (imageLoad/imageStore), que D3D9 no
	 * tiene.  De ella aqui solo el decal, que solo LEE el Z: contra una copia
	 * que el shim rehace cuando el Z cambia (GXD3D_SetDecalN64).  Los demas
	 * modos siguen con la prueba del device, que es lo que GLideN64 hace sin
	 * N64DepthCompare.  Condiciones de UDepthInfo: Z activa (G_ZBUFFER o PRIM,
	 * ciclo 1/2, ya en GXenableZmode) y Z_CMP. */
	{
		const BOOL decalN64 = ((config.generalEmulation.hacks & hack_n64DepthCompareDec) != 0 &&
		                       gDP.otherMode.depthMode == ZMODE_DEC &&
		                       gDP.otherMode.depthCompare != 0 &&
		                       GXenableZmode == GX_ENABLE) ? TRUE : FALSE;
		GX_SetZMode(GXenableZmode, decalN64 ? (u8)GX_ALWAYS : GXZfunc, GXZupdate);
		GXD3D_SetDecalN64( decalN64 );
	}
#endif // __GX__

#ifndef __GX__
	if ((gDP.changed & CHANGED_ALPHACOMPARE) || (gDP.changed & CHANGED_RENDERMODE))
	{
		// Enable alpha test for threshold mode
		if ((gDP.otherMode.alphaCompare == G_AC_THRESHOLD) && !(gDP.otherMode.alphaCvgSel))
		{
			glEnable( GL_ALPHA_TEST );

			glAlphaFunc( (gDP.blendColor.a > 0.0f) ? GL_GEQUAL : GL_GREATER, gDP.blendColor.a );
		}
		// Used in TEX_EDGE and similar render modes
		else if (gDP.otherMode.cvgXAlpha)
		{
			glEnable( GL_ALPHA_TEST );

			// Arbitrary number -- gives nice results though
			glAlphaFunc( GL_GEQUAL, 0.5f );
		}
		else
			glDisable( GL_ALPHA_TEST );

		if (OGL.usePolygonStipple && (gDP.otherMode.alphaCompare == G_AC_DITHER) && !(gDP.otherMode.alphaCvgSel))
			glEnable( GL_POLYGON_STIPPLE );
		else
			glDisable( GL_POLYGON_STIPPLE );
	}
#else // !__GX__
	//GX alpha compare update

	/* [Xbox360] [Mario Tennis] CHANGED_CYCLETYPE tambien: el relleno (FILL) apaga
	 * el alpha test, y entrar o salir de FILL no siempre toca el modo de render. */
	if ((gDP.changed & CHANGED_ALPHACOMPARE) || (gDP.changed & CHANGED_RENDERMODE) ||
	    (gDP.changed & CHANGED_CYCLETYPE))
	{
		u8 GXalphaFunc, GXalphaRef;
//		u8 GXZcompLoc = GX_FALSE; //do Z compare after texturing (GX_FALSE)
		OGL.GXuseAlphaCompare = true;
		/* [Xbox360] ★ [Mario Tennis, bob-ombs] Sin alpha test en FILL, como
		 * GLideN64 (UAlphaTestInfo, glsl_CombinerProgramUniformFactoryCommon.cpp:
		 * 445: `cycleType == G_CYC_FILL` -> uEnableAlphaTest 0).  Un relleno de
		 * alfa 0 con un umbral puesto se descartaba entero y no borraba nada;
		 * ver la nota del FILL en la mezcla, mas abajo. */
		if (gDP.otherMode.cycleType == G_CYC_FILL)
		{
			GXalphaFunc = GX_ALWAYS;
			GXalphaRef = (u8) 0;
			OGL.GXuseAlphaCompare = false;
		}
		// Enable alpha test for threshold mode
		else if ((gDP.otherMode.alphaCompare == G_AC_THRESHOLD) && !(gDP.otherMode.alphaCvgSel))
		{
//			glEnable( GL_ALPHA_TEST );
//			glAlphaFunc( (gDP.blendColor.a > 0.0f) ? GL_GEQUAL : GL_GREATER, gDP.blendColor.a );
			GXalphaFunc = (gDP.blendColor.a > 0.0f) ? GX_GEQUAL : GX_GREATER;
			GXalphaRef = GXcastf32u8(gDP.blendColor.a);
		}
		// Used in TEX_EDGE and similar render modes
		else if (gDP.otherMode.cvgXAlpha)
		{
//			glEnable( GL_ALPHA_TEST );
			// Arbitrary number -- gives nice results though
//			glAlphaFunc( GL_GEQUAL, 0.5f );
			/* [Xbox360] ★ [Hexen, las manos/arma no salian] 0.125 COMO GLideN64, no
			 * el 0.5 "arbitrario" de glN64: GLideN64 descarta con cvgXAlpha solo
			 * si `alpha < 0.125` (glsl_CombinerProgramBuilder.cpp:294/308).
			 * Medido con una sonda temporal (log 3): las manos se dibujaban (con
			 * marcatex=sinclip salian) y el test las tiraba enteras; casi todas
			 * las combinaciones en que glN64 difiere de GLideN64 son esta rama,
			 * omL 0F0A7008/0C193078, y una lleva alfa TEXEL0.a*ENV.a.  Con este
			 * umbral las manos salen (confirmado por el usuario). */
			GXalphaFunc = GX_GEQUAL;
			GXalphaRef = (u8) 32;
		}
		else
		{
//			glDisable( GL_ALPHA_TEST );
			GXalphaFunc = GX_ALWAYS;
			GXalphaRef = (u8) 0;
//			GXZcompLoc = GX_TRUE; //do Z compare before texturing (GX_TRUE)
			OGL.GXuseAlphaCompare = false;
		}
		//TODO: Add more code to implement the following line.
//		GX_SetZCompLoc(GXZcompLoc);
		GX_SetAlphaCompare(GXalphaFunc,GXalphaRef,GX_AOP_AND,GX_ALWAYS,0);

#ifdef GLN64_SDLOG
		sprintf(txtbuffer,"SetAlphaCompare: GXalphaFunc %d, GXalphaRef %d, GXZcompLoc %d\n", GXalphaFunc, GXalphaRef, GXZcompLoc);
		DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
#endif // GLN64_SDLOG

//TODO: Implement PolygonStipple?
/*		if (OGL.usePolygonStipple && (gDP.otherMode.alphaCompare == G_AC_DITHER) && !(gDP.otherMode.alphaCvgSel))
			glEnable( GL_POLYGON_STIPPLE );
		else
			glDisable( GL_POLYGON_STIPPLE );*/

		if (gDP.otherMode.depthSource == G_ZS_PRIM)
			GX_SetZTexture(GX_ZT_REPLACE,GX_TF_Z24X8,gDP.primDepth.z*GX_MAX_Z24);
		else
			GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z8,0);
	}
#endif // __GX__

#ifndef __GX__
	if (gDP.changed & CHANGED_SCISSOR)
	{
		glScissor( (int)(gDP.scissor.ulx * OGL.scaleX), (int)((VI.height - gDP.scissor.lry) * OGL.scaleY + OGL.heightOffset),
			(int)((gDP.scissor.lrx - gDP.scissor.ulx) * OGL.scaleX), (int)((gDP.scissor.lry - gDP.scissor.uly) * OGL.scaleY) );
	}
#else // !__GX__
	if (gDP.changed & CHANGED_SCISSOR)
	{
		f32 tijX, tijY;
		_oglTijeraLR( &tijX, &tijY );
		float ulx = max(OGL.GXorigX + gDP.scissor.ulx * OGL.GXscaleX, 0);
		float uly = max(OGL.GXorigY + gDP.scissor.uly * OGL.GXscaleY, 0);
		float lrx = max(OGL.GXorigX + min(tijX * OGL.GXscaleX,OGL.GXwidth), 0);
		float lry = max(OGL.GXorigY + min(tijY * OGL.GXscaleY,OGL.GXheight), 0);
		GX_SetScissor((u32) ulx,(u32) uly,(u32) (lrx - ulx),(u32) (lry - uly));
	}
#endif // __GX__

	if (gSP.changed & CHANGED_VIEWPORT)
	{
		OGL_UpdateViewport();
	}

	if ((gDP.changed & CHANGED_COMBINE) || (gDP.changed & CHANGED_CYCLETYPE))
	{
		if (gDP.otherMode.cycleType == G_CYC_COPY)
			Combiner_SetCombine( EncodeCombineMode( 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0 ) );
		/* [Xbox360] Alfa SHADE, no 1: GLideN64 (Combiner.cpp:311-312) rellena
		 * con el alfa del fill color (bit 0 en 16 bits, gDP.cpp:268-281), que
		 * OGL_DrawRect pone en el color de vertice.  Con alfa 1 el borrado
		 * 0x0000 de los aux de 64x64 de Mario Tennis (bob-ombs) los dejaba
		 * OPACOS y el pegado con alpha test sacaba el aux entero en negro. */
		else if (gDP.otherMode.cycleType == G_CYC_FILL)
			Combiner_SetCombine( EncodeCombineMode( 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE ) );
		else
			Combiner_SetCombine( gDP.combine.mux );

		// S.C.A.R.S red track hack/fix
		if(GetGameSpecificHack() == &hack_scars && gDP.combine.mux == 0x00127E61F0FFF83EULL)
			Combiner_SetCombine( EncodeCombineMode( TEXEL0, 0, SHADE, NOISE, 0, 0, 0, SHADE, 0, 0, 0, COMBINED, 0, 0, 0, 1 ) );
	}

	if (gDP.changed & CHANGED_COMBINE_COLORS)
	{
		Combiner_UpdateCombineColors();
	}

#ifdef __GX__
	/* [Xbox360] Atlas de mipmap (TextureCache_LoadAtlas): que la unidad 1 sea
	 * un atlas o una textura normal depende del combinador (usesLOD) y del
	 * modo LOD, y sus niveles de textureDetail y gSP.texture.level, no solo
	 * del tile.  Si algo de eso cambia sin que cambie la textura, hay que
	 * volver a buscarla. */
	{
		static u32 atlasPedidoAntes = 0;
		const u32 atlasPedido = (combiner.usesLOD && (gDP.otherMode.textureLOD == G_TL_LOD))
			? (1 + ((u32)gDP.otherMode.textureDetail << 4) + (((u32)gSP.texture.level & 0xFF) << 8)) : 0;
		if (atlasPedido != atlasPedidoAntes)
		{
			atlasPedidoAntes = atlasPedido;
			gSP.changed |= CHANGED_TEXTURE;
		}
	}
#endif // __GX__

	if ((gSP.changed & CHANGED_TEXTURE) || (gDP.changed & CHANGED_TILE) || (gDP.changed & CHANGED_TMEM))
	{
		Combiner_BeginTextureUpdate();
		if (combiner.usesT0)
		{
			TextureCache_Update( 0 );

			gSP.changed &= ~CHANGED_TEXTURE;
			gDP.changed &= ~CHANGED_TILE;
			gDP.changed &= ~CHANGED_TMEM;
		}
		else
		{
			TextureCache_ActivateDummy( 0 );
		}

		if (combiner.usesT1)
		{
			TextureCache_Update( 1 );

			gSP.changed &= ~CHANGED_TEXTURE;
			gDP.changed &= ~CHANGED_TILE;
			gDP.changed &= ~CHANGED_TMEM;
		}
		else
		{
			TextureCache_ActivateDummy( 1 );
		}
		Combiner_EndTextureUpdate();
	}

#ifdef __GX__
	/* [Xbox360] GLideN64 UFrameBufferInfo: una textura de framebuffer de 8
	 * bits se lee con alfa fijo 0.825 si IMAGE_READ esta apagado.  Solo
	 * cuenta con una copia monocroma ligada (GXD3D_SetCopyMono, que pone
	 * FrameBuffer.cpp al copiar); con cualquier otra textura no hace nada. */
	GXD3D_SetMonoAlfaFija( gDP.otherMode.imageRead ? 0 : 1 );

	/* [Xbox360] ★ [Super Mario 64, cuadro de Peach a Bowser] LOS UNIFORMES DE LA
	 * ENTRADA LOD_FRACTION, como GLideN64 (UTextureEngine / UMipmap de
	 * glsl_CombinerProgramUniformFactoryAccurate.cpp:103-119):
	 *   uMinLod        = gDP.primColor.m (5 bits, entre 32; RDP_SetPrimColor)
	 *   uEnableLod     = textureLOD == G_TL_LOD
	 *   uTextureDetail = textureDetail
	 *   uMaxTile       = gSP.texture.level, con su "hack" de tope 1 si la
	 *                    textura del tile 1 no tiene niveles (max_level == 0)
	 *   uScreenScale   = la escala del buffer actual (OGL.GXscale)
	 * y el paso de la UV del shim (normalizada y con el shift del tile) a la
	 * vLodTexCoord de GLideN64 (texels sin shift): dividir por shiftScale*scale
	 * del tile 0.  El shim solo lo sube si cambia; solo lo leen los shaders
	 * que usan LOD_FRACTION. */
	{
		f32 kS = 1.0f, kT = 1.0f;
		u32 maxTile = gSP.texture.level;
		if (combiner.usesT0 && cache.current[0] != NULL)
		{
			const f32 ds = cache.current[0]->shiftScaleS * cache.current[0]->scaleS;
			const f32 dt = cache.current[0]->shiftScaleT * cache.current[0]->scaleT;
			kS = (ds != 0.0f) ? OGL.GXscaleX / ds : 0.0f;
			kT = (dt != 0.0f) ? OGL.GXscaleY / dt : 0.0f;
		}
		/* El atlas cuenta como textura con niveles (GLideN64 le da max_level). */
		const CachedTexture *t1 = combiner.usesT1 ? cache.current[1] : NULL;
		const BOOL atlas = (t1 != NULL && t1->atlasNiveles > 0) ? TRUE : FALSE;
		if (cache.current[1] != NULL && cache.current[1]->max_level == 0 && !atlas && maxTile > 1)
			maxTile = 1;
		GXD3D_SetLod( kS, kT, (f32)gDP.primColor.m / 32.0f, (f32)maxTile,
		              (f32)gDP.otherMode.textureDetail,
		              (gDP.otherMode.textureLOD == G_TL_LOD) ? 1.0f : 0.0f );
		/* [Xbox360] El atlas de mipmap de la unidad 1 (TextureCache_LoadAtlas):
		 * su tamano, el del nivel 0 (para pasar IN.uv1 a texels), el filtro y
		 * el clamp del nivel 0.  Sin atlas, el shim usa la rama de dos tiles. */
		if (atlas)
			GXD3D_SetLodAtlas( 1, (f32)t1->atlasW, (f32)t1->atlasH,
			                   (f32)t1->realWidth, (f32)t1->realHeight,
			                   ((gDP.otherMode.textureFilter != G_TF_POINT) || OGL.forceBilinear) ? 1.0f : 0.0f,
			                   (f32)(t1->atlasNiveles - 1),
			                   t1->clampS ? 1.0f : 0.0f, t1->clampT ? 1.0f : 0.0f );
		else
			GXD3D_SetLodAtlas( 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
	}
#endif // __GX__

#ifndef __GX__
	if ((gDP.changed & CHANGED_FOGCOLOR) && OGL.fog)
		glFogfv( GL_FOG_COLOR, &gDP.fogColor.r );
#else // !__GX__
	if ((gDP.changed & CHANGED_FOGCOLOR) && OGL.fog) 
	{
		OGL.GXfogColor.r = GXcastf32u8(gDP.fogColor.r);
		OGL.GXfogColor.g = GXcastf32u8(gDP.fogColor.g);
		OGL.GXfogColor.b = GXcastf32u8(gDP.fogColor.b);
		OGL.GXfogColor.a = GXcastf32u8(gDP.fogColor.a);
		OGL.GXupdateFog = true;
	}

	/* [Xbox360] [Nightmare Creatures] La niebla segun el blender: ver
	 * OGL_GX_NieblaDelBlender.  Cada vez, porque depende del geometry mode
	 * (G_FOG) y del render mode, que cambian por separado. */
	{
		const int modo = ((gSP.geometryMode & G_FOG) && OGL.fog)
		               ? OGL_GX_NieblaDelBlender() : 0;
		const u8 tipo = (modo != 0) ? GX_FOG_ORTHO_LIN : GX_FOG_NONE;
		if (OGL.GXfogType != tipo)
		{
			OGL.GXfogType = tipo;
			OGL.GXupdateFog = true;
		}
		GXD3D_SetNieblaInversa( (modo == 2) ? 1 : 0 );
	}

	if(OGL.GXupdateFog)
	{
		GX_SetFog(OGL.GXfogType,OGL.GXfogStartZ,OGL.GXfogEndZ,-1.0,1.0,OGL.GXfogColor);
		OGL.GXupdateFog = false;
		if(OGL.GXfogType == GX_FOG_ORTHO_LIN)
		{
#ifdef SHOW_DEBUG
			sprintf(txtbuffer,"SetFog: StartZ %f, EndZ %f, color (%d,%d,%d,%d), fo %f, fm %f", OGL.GXfogStartZ, OGL.GXfogEndZ, OGL.GXfogColor.r, OGL.GXfogColor.g, OGL.GXfogColor.b, OGL.GXfogColor.a, (float)gSP.fog.offset, (float)gSP.fog.multiplier);
			DEBUG_print(txtbuffer,DBG_RSPINFO1);
#endif
		}
	}
#endif // __GX__

#ifndef __GX__
	if ((gDP.changed & CHANGED_RENDERMODE) || (gDP.changed & CHANGED_CYCLETYPE))
	{
		if ((gDP.otherMode.forceBlender) &&
			(gDP.otherMode.cycleType != G_CYC_COPY) &&
			(gDP.otherMode.cycleType != G_CYC_FILL) &&
			!(gDP.otherMode.alphaCvgSel))
		{
 			glEnable( GL_BLEND );

			switch (gDP.otherMode.l >> 16)
			{
				case 0x0448: // Add
				case 0x055A:
					glBlendFunc( GL_ONE, GL_ONE );
					break;
				case 0x0C08: // 1080 Sky
				case 0x0F0A: // Used LOTS of places
					glBlendFunc( GL_ONE, GL_ZERO );
					break;
				case 0xC810: // Blends fog
				case 0xC811: // Blends fog
				case 0x0C18: // Standard interpolated blend
				case 0x0C19: // Used for antialiasing
				case 0x0050: // Standard interpolated blend
				case 0x0055: // Used for antialiasing
					glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
					break;
				case 0x0FA5: // Seems to be doing just blend color - maybe combiner can be used for this?
				case 0x5055: // Used in Paper Mario intro, I'm not sure if this is right...
					glBlendFunc( GL_ZERO, GL_ONE );
					break;
				default:
					glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
					break;
			}
		}
		else
			glDisable( GL_BLEND );

		if (gDP.otherMode.cycleType == G_CYC_FILL)
		{
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glEnable( GL_BLEND );
		}
	}
#else // !__GX__
	static u8 GXblenddstfactor, GXblendsrcfactor, GXblendmode;
	static u8 GXblendFog = 0;
	/* [Xbox360] [F-Zero X] Que color pone el termino del blender del shim cuando
	 * GXblendFog no lo pide (0): 2 = el color de mezcla, 3 = el de niebla, los
	 * dos con peso 0 (el color tal cual: blender sin FORCE_BL). */
	static u8 GXblendFogSrc = 0;

	if ((gDP.changed & CHANGED_RENDERMODE) || (gDP.changed & CHANGED_CYCLETYPE))
	{

		// Ported from GLideN64's GraphicsDrawer::_legacyBlending()
		const unsigned int blendmode = gDP.otherMode.l >> 16;

		/* [Xbox360] [Conker] ★ EL BLENDER 0x07C2 EXACTO, NO EL DE LA TABLA.
		 *
		 * GLideN64 lo documenta en su tabla legacy (GraphicsDrawer.cpp:292):
		 *     ciclo 1: clr_in * a_fog + clr_fog * (1-a)
		 *     ciclo 2: clr_in * 0 + clr_in * 1
		 * y la tabla lo deja en ONE/ZERO, o sea `clr_in`.  Pero GLideN64 usa
		 * la tabla solo con `enableLegacyBlending`, que por defecto esta a 0
		 * (Config.cpp:56): normalmente emula el blender en el shader.
		 *
		 * Medido en Conker (logs 34-36, sonda [SIL8T]): la silueta de la
		 * sombra se pinta con este modo, ciclo doble, niebla NEGRA y A_FOG =
		 * 0.004..0.008.  En la N64 sale `clr_in*0.006 + 0`: una mancha negra
		 * uniforme.  Con ONE/ZERO salia `clr_in` = SHADE*TEXEL0, el modelo en
		 * grises: partes del cuerpo mas oscuras y parpadeo al animarse.
		 *
		 * Solo el caso medido (ciclo doble), que es el que las dos lineas de
		 * arriba describen.  Los factores de GX siguen siendo ONE/ZERO; el
		 * termino de niebla lo pone el shim al final del pixel shader. */
		GXblendFog = 0;
		GXblendFogSrc = 0;

		/* [Xbox360] ★★ [ISS64, silueta de la sombra en verde] BLENDER FORZADO
		 * QUE SOLO DEJA LA MEMORIA: INVISIBLE, como el camino por defecto de
		 * GLideN64 (_ordinaryBlending, GraphicsDrawer.cpp:443-538).
		 *
		 * Medido con la sonda [ISSDEC] (log 37, ya retirada; confirmado en el
		 * log siguiente: `mezcla=1 0/1`): la silueta se pinta con
		 * `omL FF5A6379` -- FORCE_BL + ALPHA_CVG_SEL, blender FF5A = ciclo 1
		 * `FOG * 0 + MEM * 1` -- y color PRIM verde (00FF0080).  En la consola
		 * no pinta nada: solo deja su z para el decal.  glN64 la mandaba a la
		 * rama de abajo (ALPHA_CVG_SEL sin los tres bits de 0x7000), que es la
		 * de `_legacyBlending` y dice "sin mezcla": opaca, VERDE.  GLideN64 por
		 * defecto no usa esa tabla: emula el blender en el shader (el primer
		 * termino sale 0 porque m1b = 0) y deja factores de memoria ONE/ONE;
		 * con el origen a 0 eso es ZERO/ONE.
		 *
		 * Solo el subcaso en que el shader de GLideN64 sacaria el origen a 0 sin
		 * mirar nada mas -- m1b == 0 (el factor del primer mux es CERO) -- y el
		 * segundo mux es la memoria (m2a == MEM).  El factor de destino es el de
		 * su `memFactorSource 1`: m2b 1-A (con A = 0) y ONE -> ONE, A_MEM ->
		 * DST_ALPHA, 0 -> ZERO.  En dos ciclos manda el segundo, como alli. */
		BOOL soloMemoria = FALSE;
		u8   dstMemoria  = GX_BL_ONE;
		if (gDP.otherMode.forceBlender != 0 && gDP.otherMode.cycleType < G_CYC_COPY)
		{
			const BOOL dos = (gDP.otherMode.cycleType == G_CYC_2CYCLE) ? TRUE : FALSE;
			const u32 m1b = dos ? gDP.otherMode.c2_m1b : gDP.otherMode.c1_m1b;
			const u32 m2a = dos ? gDP.otherMode.c2_m2a : gDP.otherMode.c1_m2a;
			const u32 m2b = dos ? gDP.otherMode.c2_m2b : gDP.otherMode.c1_m2b;
			if (m1b == 3 && m2a == 1)
			{
				soloMemoria = TRUE;
				if (m2b == 1)
					dstMemoria = GX_BL_DSTALPHA;
				else if (m2b == 3)
					dstMemoria = GX_BL_ZERO;
				else
					dstMemoria = GX_BL_ONE;
			}
		}

		if (soloMemoria)
		{
			GXblendmode = GX_BM_BLEND;
			GXblendsrcfactor = GX_BL_ZERO;
			GXblenddstfactor = dstMemoria;
		}
		// 0x7000 = CVG_X_ALPHA|ALPHA_CVG_SEL|FORCE_BL
		else if (gDP.otherMode.alphaCvgSel != 0 && (gDP.otherMode.l & 0x7000) != 0x7000)
		{
			switch (blendmode)
			{
				case 0x4055: // Mario Golf
				case 0x5055: // Paper Mario intro clr_mem * a_in + clr_mem * a_mem
					GXblendmode = GX_BM_BLEND;
					GXblendsrcfactor = GX_BL_ZERO;
					GXblenddstfactor = GX_BL_ONE;
					break;
				default:
					GXblendmode = GX_BM_NONE;
					GXblendsrcfactor = GX_BL_ONE;
					GXblenddstfactor = GX_BL_ZERO;
					break;
			}
		}
		else if ((gDP.otherMode.forceBlender) && (gDP.otherMode.cycleType < G_CYC_COPY))
		{
			GXblendmode = GX_BM_BLEND;

			switch (blendmode)
			{
				// Mace objects
				case 0x0382:
				// 1080 Sky
				case 0x0091:
				// Used LOTS of places
				case 0x0C08:
				//DK64 blue prints
				case 0x0F0A:
				// Bomberman 2 special blend mode
				case 0x0302:
				//Sin and Punishment
				case 0xA500:
				// Battlezone
				case 0xCB02:
				// Conker BFD
				case 0xC800:
				case 0x07C2:
				case 0x00C0:
				//ISS64
				case 0xC302:
				// Donald Duck
				case 0xC702:
					GXblendsrcfactor = GX_BL_ONE;
					GXblenddstfactor = GX_BL_ZERO;
					break;

				case 0x55f0:
					// Bust-A-Move 3 DX
					GXblendsrcfactor = GX_BL_ONE;
					GXblenddstfactor = GX_BL_SRCALPHA;
					break;

				case 0x0F1A:
					if (gDP.otherMode.cycleType == G_CYC_1CYCLE) {
						GXblendsrcfactor = GX_BL_ONE;
						GXblenddstfactor = GX_BL_ZERO;
					} else {
						GXblendsrcfactor = GX_BL_ZERO;
						GXblenddstfactor = GX_BL_ONE;
					}
					break;

				// Space Invaders
				case 0x0448: // Add
				case 0x055A:
					GXblendsrcfactor = GX_BL_ONE;
					GXblenddstfactor = GX_BL_ONE;
					break;

				case 0xc712: // Pokemon Stadium?
				case 0xAF50: // LOT in Zelda: MM
				case 0x0F5A: // LOT in Zelda: MM
				case 0x0FA5: // Seems to be doing just blend color - maybe combiner can be used for this?
				case 0x5055: // Used in Paper Mario intro, I'm not sure if this is right...
					GXblendsrcfactor = GX_BL_ZERO;
					GXblenddstfactor = GX_BL_ONE;
					break;

				case 0x5F50: //clr_mem * 0 + clr_mem * (1-a)
					GXblendsrcfactor = GX_BL_ZERO;
					GXblenddstfactor = GX_BL_INVSRCALPHA;
					break;

				case 0xF550: //clr_fog * a_fog + clr_mem * (1-a)
				case 0x0150: // spiderman
				case 0x0D18: //clr_in * a_fog + clr_mem * (1-a)
					GXblendsrcfactor = GX_BL_SRCALPHA;
					GXblenddstfactor = GX_BL_INVSRCALPHA;
					break;

				case 0x0550: // bomberman 64 title screen dim layer
					GXblendsrcfactor = GX_BL_ONE;
					GXblenddstfactor = GX_BL_ONE;
					break;

				case 0xC912: //40 winks, clr_in * a_fog + clr_mem * 1
					GXblendsrcfactor = GX_BL_SRCALPHA;
					GXblenddstfactor = GX_BL_ONE;
					break;

				case 0x0040: // Fzero
				case 0xC810: // Blends fog
				case 0x0C18: // Standard interpolated blend
				case 0x0050: // Standard interpolated blend
				case 0x0051: // Standard interpolated blend
				case 0x0055: // Used for antialiasing
					GXblendsrcfactor = GX_BL_SRCALPHA;
					GXblenddstfactor = GX_BL_INVSRCALPHA;
					break;

				case 0x0C19: // Used for antialiasing
				case 0xC811: // Blends fog
					GXblendsrcfactor = GX_BL_SRCALPHA;
					GXblenddstfactor = GX_BL_DSTALPHA;
					break;

				case 0x5000: // V8 explosions
					GXblendsrcfactor = GX_BL_INVSRCALPHA;
					GXblenddstfactor = GX_BL_SRCALPHA;
					break;

				case 0xFA00: // Bomberman second attack
					GXblendsrcfactor = GX_BL_ONE;
					GXblenddstfactor = GX_BL_ZERO;
					break;

				default:
					GXblendsrcfactor = GX_BL_SRCALPHA;
					GXblenddstfactor = GX_BL_INVSRCALPHA;
					break;
			}

			/* [Xbox360] [Conker] Ver la nota de GXblendFog arriba. */
			if (blendmode == 0x07C2 && gDP.otherMode.cycleType == G_CYC_2CYCLE)
				GXblendFog = 1;
		}
		/* [Xbox360] ★★ [Mario Tennis, iconos de la seleccion] BLENDER SIN FORZAR:
		 * EL PRIMER MUX, COMO EL CAMINO POR DEFECTO DE GLideN64.
		 *
		 * Aqui estaba la rama CLR_ON_CVG de `_legacyBlending` ("just use second
		 * mux": memoria si c1_m2a == 1).  GLideN64 solo usa esa tabla con
		 * `enableLegacyBlending`, apagado por defecto; su camino normal es el
		 * blender en el shader, que sin FORCE_BL saca `MUXPM(m1a)` con factor de
		 * destino 0 -- el PRIMER mux, sin mirar CLR_ON_CVG --.  Lo mismo en tabla
		 * es `_ordinaryBlending` (GraphicsDrawer.cpp:539, "Just use first mux of
		 * blender"), que es lo que se porta.
		 *
		 * Medido con [OMLCOMP] (log 5): los iconos de personajes llegan con
		 * `omL 005510C4` (CLR_ON_CVG + CVG_X_ALPHA, blender 0055 sin forzar):
		 * m1a = IN, m2a = MEM.  Con la rama vieja salia BLEND ZERO/ONE -- no se
		 * pintaba nada --.  Salio al arreglar F3D(EX2)_SetOtherMode_L: antes esa
		 * escritura entera se perdia y los iconos usaban el modo anterior. */
		else if ((gDP.otherMode.forceBlender == 0) && (gDP.otherMode.cycleType < G_CYC_COPY))
		{
			/* [Xbox360] ★ [F-Zero X, el borde del modo ataque] Y EL PRIMER MUX
			 * PUEDE SER EL COLOR DE MEZCLA O EL DE NIEBLA, no solo la memoria o
			 * la entrada.  GLideN64 d4dbe7c0 "Correct blending when FORCE_BL flag
			 * is off" (#639; blog 2017-09-11 "F-Zero fixes"): sin FORCE_BL el
			 * blender no se salta, su salida es el primer argumento,
			 * `clampedColor.rgb = muxPM[uBlendMux1[0]].rgb` (ShaderBlender1/2),
			 * y el borde lo pinta con el BLEND COLOR (rojo), ignorando el
			 * combinador.  muxPM = {entrada, memoria, color de mezcla, niebla}.
			 * ★ [log 39] En CICLO DOBLE el primer ciclo se mezcla SIEMPRE
			 * (GLideN64 UBlendMode2Cycle: `forceBlend1 = 1`; angrylion calcula
			 * la ecuacion del ciclo 0 sin mirar blend_en) y solo el segundo
			 * obedece a FORCE_BL: su primer mux 0 es el RESULTADO MEZCLADO del
			 * primer ciclo (`muxPM[0] = srcColor1`), no el primer argumento de
			 * este.  Es el caso de los coches de F-Zero X en carrera (H 00182C30,
			 * L C8000230): tomar la P del primer ciclo los dejaba del color de
			 * la niebla, rosas.  El color de mezcla o de niebla lo pone
			 * el shim en el pixel con el termino del blender (GXD3D_SetBlenderNiebla,
			 * peso 0: el color tal cual); el alfa sigue siendo el del combinador. */
			/* Los muxes de los bits crudos, como OGL_GX_NieblaDelBlender:
			 * m1a de GBL_c1 = bits 15-14 de L >> 16, el de GBL_c2 = 13-12. */
			const u32 blMux = gDP.otherMode.l >> 16;
			const u32 c1m1a = (blMux >> 14) & 3;
			const u32 c2m1a = (blMux >> 12) & 3;
			/* En ciclo doble, c2_m1a == 0 = la mezcla del primer ciclo, que aqui
			 * es la entrada (con el termino de niebla de abajo si lo pide). */
			const u32 primero = (gDP.otherMode.cycleType == G_CYC_2CYCLE) ? c2m1a : c1m1a;
			if (primero == 2 || primero == 3)
				GXblendFogSrc = (u8)primero;
			bool useMemColor = (primero == 1) ? true : false;
			if (useMemColor) {
				GXblendmode = GX_BM_BLEND;
				GXblendsrcfactor = GX_BL_ZERO;
				GXblenddstfactor = GX_BL_ONE;
			} else {
				GXblendmode = GX_BM_NONE;
				GXblendsrcfactor = GX_BL_ONE;
				GXblenddstfactor = GX_BL_ZERO;
			}
		}
		else {
			GXblendmode = GX_BM_NONE;
			GXblendsrcfactor = GX_BL_ONE;
			GXblenddstfactor = GX_BL_ZERO;
		}

		/* [Xbox360] ★★ [Mario Tennis, bob-ombs] EL RELLENO NO MEZCLA, como GLideN64
		 * (GraphicsDrawer.cpp `_ordinaryBlending`/`_dualSourceBlending`: con
		 * cycleType >= G_CYC_COPY la mezcla se apaga) y como la consola: en modo
		 * FILL el RDP escribe el color de relleno tal cual.  glN64 ponia aqui
		 * SRCALPHA/INVSRCALPHA, asi que un relleno de color transparente
		 * (0x0000, el borrado normal de un buffer auxiliar) no escribia nada y el
		 * buffer se quedaba con lo que hubiera en el EFB.  Medido con [AUXDIB]
		 * (log 18): las bob-ombs del marcador son modelos 3D dibujados en buffers
		 * auxiliares de 64x64 que se borran con un FILL y luego se pegan con
		 * alpha test; sin borrar, el fondo quedaba opaco -- el cuadrado negro --. */
		if (gDP.otherMode.cycleType == G_CYC_FILL)
		{
			GXblendmode = GX_BM_NONE;
			GXblendsrcfactor = GX_BL_ONE;
			GXblenddstfactor = GX_BL_ZERO;
		}

	}

	GX_SetBlendMode(GXblendmode, GXblendsrcfactor, GXblenddstfactor, GX_LO_CLEAR);
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	GX_SetDstAlpha(GX_DISABLE, 0xFF);

	/* [Xbox360] [Conker] El termino de niebla del blender 0x07C2.  Fuera del
	 * bloque de CHANGED_RENDERMODE porque el color de niebla cambia sin que
	 * cambie el modo; el shim solo ensucia si algo cambia de verdad. */
	{
		GXColor fogBl;
		u8 modoBl = GXblendFog;
		fogBl.r = GXcastf32u8(gDP.fogColor.r);
		fogBl.g = GXcastf32u8(gDP.fogColor.g);
		fogBl.b = GXcastf32u8(gDP.fogColor.b);
		fogBl.a = GXcastf32u8(gDP.fogColor.a);
		/* [Xbox360] [F-Zero X] Blender sin FORCE_BL cuyo primer mux es el color
		 * de mezcla (2) o el de niebla (3): ese color tal cual (peso 0).  Ver
		 * GXblendFogSrc. */
		if (modoBl == 0 && GXblendFogSrc != 0)
		{
			if (GXblendFogSrc == 2)
			{
				fogBl.r = GXcastf32u8(gDP.blendColor.r);
				fogBl.g = GXcastf32u8(gDP.blendColor.g);
				fogBl.b = GXcastf32u8(gDP.blendColor.b);
			}
			fogBl.a = 0;
			modoBl = 1;
		}
		/* [Xbox360] ★ [F-Zero X, los reflejos de los vehiculos] "Reflections
		 * on vehicles in F-Zero are not reflections but fog!" (blog de GLideN64,
		 * 2017-09-11).  F3DFLX2 deja el reflejo en el ALFA DEL VERTICE (la tabla
		 * de la DMAIO, ver gSPProcessVertex; GLideN64 47d718f2) y apaga el
		 * calculo de la niebla, y el blender de ciclo 1 es
		 * `CLR_FOG * A_SHADE + CLR_IN * (1 - A)`: mezcla el color de niebla con
		 * el peso del reflejo.  GLideN64 lo saca porque emula el blender en el
		 * shader; aqui la niebla del blender solo existia con G_FOG y pesaba con
		 * el coeficiente propio (`fogc`), asi que sin G_FOG el reflejo se perdia.
		 * Modo 2 del shim: lerp(color, niebla, alfa del vertice).  Solo con
		 * F3DFLX2 y sin G_FOG (con G_FOG manda la niebla de siempre), como el
		 * caso por microcodigo que GLideN64 tuvo antes (ef36b702). */
		/* Con FORCE_BL, o en ciclo doble, donde el primer ciclo se mezcla
		 * siempre ([log 39]: los coches en carrera, sin FORCE_BL). */
		else if (modoBl == 0 && (gSP.geometryMode & G_FOG) == 0 &&
		         GBI.current != NULL && GBI.current->type == F3DFLX2 &&
		         (gDP.otherMode.forceBlender != 0 || gDP.otherMode.cycleType == G_CYC_2CYCLE) &&
		         gDP.otherMode.cycleType < G_CYC_COPY &&
		         ((gDP.otherMode.l >> 16) & 0xCCCCu) == 0xC800u)   // GBL_c1: FOG, A_SHADE, IN, 1-A
		{
			modoBl = 2;
		}
		GXD3D_SetBlenderNiebla( modoBl, fogBl );
	}

#ifdef GLN64_SDLOG
	sprintf(txtbuffer,"SetBlendMode: GXblendmode %d, SrcFactor %d, DestFactor %d\n", GXblendmode, GXblendsrcfactor, GXblenddstfactor);
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
#endif // GLN64_SDLOG
#endif // __GX__

	gDP.changed &= CHANGED_TILE | CHANGED_TMEM;
	gSP.changed &= CHANGED_TEXTURE | CHANGED_MATRIX;

#ifdef __GX__
	/* [Xbox360] ★★ [Pilotwings, sombras] DIBUJAR SOBRE EL DEPTH IMAGE, COMO
	 * GLideN64: ccff245f "Implement render to depth buffer. Fixed shadows in
	 * Pilot Wings", hoy GraphicsDrawer::_updateStates (GraphicsDrawer.cpp:794-
	 * 822) y URenderTarget (glsl_CombinerProgramUniformFactoryCommon.cpp:616):
	 *
	 *   - color image == depth image: el shader escribe profundidad = rojo del
	 *     color, con el test en ALWAYS y escritura (setDepthCopyParameters);
	 *   - y si ademas isDepthCompareEnabled() (gDP.cpp:39: ciclo 1/2, Z_CMP y
	 *     G_ZBUFFER o depth source PRIM), el pixel se prueba con SU z contra
	 *     una copia del Z (copyDepthBufferTexture): uRenderTarget 2;
	 *   - sin framebuffer actual (aqui: sin emulacion de FB) no se escribe
	 *     color (setBlending(ZERO, ONE)).  Con FB el color cae en el EFB y el
	 *     buffer principal se repone al volver a el (gDPSetColorImage).
	 *
	 * Es la mitad de la sombra de Pilotwings: la silueta se deja en el Z
	 * buffer, y luego un poligono grande en el color buffer solo pasa la
	 * prueba de Z dentro de ella.  Sin esto glN64 pintaba la silueta como
	 * color y el poligono grande salia entero: los "poligonos negros enormes"
	 * del articulo de GLideN64 (que Wii64 tapaba con el parche de
	 * gamehacks.c).
	 *
	 * El FILL no pasa por aqui: el relleno sobre el depth image ya tiene su
	 * camino (OGL_DrawDepthFill, gDPFillRectangle).  El COPY tampoco, aunque
	 * GLideN64 si lo incluye: no lo pide ningun juego medido y aqui hay copias
	 * de profundidad propias de Wii64 (DepthCopy.cpp) que no conviene pisar.
	 *
	 * Va AL FINAL y fuerza CHANGED_RENDERMODE (como setDepthCopyParameters):
	 * el siguiente dibujo vuelve a pasar por aqui y, si ya no es el depth
	 * image, recompone el modo de Z normal de arriba. */
	/* [Xbox360] ★ [Zelda MM, Lente de la Verdad] GLideN64 5347689b
	 * (GraphicsDrawer::_updateStates, m_depthCopyMode): la pasada eBgCopy de
	 * gSPBgRectCopy escribe el Z guardado del fondo (gln64_lotFondo) solo donde
	 * el Z actual vale 0 -- render to depth modo 4 --, en ALWAYS con escritura
	 * y SIN color (el dibujo normal de despues lo pinta); y en esa pasada normal
	 * (eCopyDone) no hay render to depth aunque el color image sea el depth
	 * image.  El Z guardado se vuelve a elegir aqui: lo que se haya dibujado
	 * entre medias pudo elegir otro (Mario Golf, _gDPCopiaDeZGuardada). */
	if (gln64_lotModo == GLN64_LOT_DEVUELVE && GXD3D_UsaZGuardada( gln64_lotFondo ))
	{
		GXD3D_SetRenderToDepth( 4 );
		GXD3D_SetDecalN64( 0 );
		GX_SetZMode( GX_ENABLE, GX_ALWAYS, GX_TRUE );
		GX_SetColorUpdate( GX_DISABLE );
		GX_SetAlphaUpdate( GX_DISABLE );
		gDP.changed |= CHANGED_RENDERMODE;
	}
	else if (gln64_lotModo != GLN64_LOT_NADA)
		GXD3D_SetRenderToDepth( 0 );
	else if (gDP.colorImage.address == gDP.depthImageAddress && gln64_zCopiaDesde != 0)
	{
		/* [Xbox360] ★★ [Mario Golf] Modo 3: el texrect copia al Z un Z guardado
		 * (GLideN64 7dca7f8f; ver _gDPCopiaDeZGuardada en gDP.cpp).  En COPY,
		 * que el render to depth de arriba no cubre.  Z activa en ALWAYS con
		 * escritura y SIN color: es una copia de profundidad, y en el EFB unico
		 * el color de la tira caeria encima de la imagen. */
		GXD3D_SetRenderToDepth( 3 );
		GXD3D_SetDecalN64( 0 );
		GX_SetZMode( GX_ENABLE, GX_ALWAYS, GX_TRUE );
		GX_SetColorUpdate( GX_DISABLE );
		GX_SetAlphaUpdate( GX_DISABLE );
		gDP.changed |= CHANGED_RENDERMODE;
	}
	/* [Xbox360] ★ [Resident Evil 2] LOS FONDOS DE PROFUNDIDAD.  El juego pinta
	 * el Z pre-renderizado de cada sala con el color image = depth image, y en
	 * GLideN64 (5e51b386, hack_LoadDepthTextures) esa textura es la Z cruda y
	 * su render to depth la escribe (fragDepth = fragColor.r), en COPY tambien.
	 * Modo 5: la palabra del texel (GXGetRE2Z_RGBA8) pasada al Z del EFB con
	 * el rango del viewport, el mismo de DepthCopy.cpp.  Sin color, como el
	 * modo 3: en el EFB unico caeria encima de la imagen.  Con Z_CMP en 1/2
	 * ciclos sigue el modo 2 de abajo (GLideN64 alli: uRenderTarget 2). */
	else if ((config.generalEmulation.hacks & hack_RE2) != 0 &&
	         gDP.colorImage.address == gDP.depthImageAddress &&
	         (gDP.otherMode.cycleType == G_CYC_COPY ||
	          (gDP.otherMode.cycleType <= G_CYC_2CYCLE &&
	           !(gDP.otherMode.depthCompare != 0 &&
	             ((gSP.geometryMode & G_ZBUFFER) || gDP.otherMode.depthSource == G_ZS_PRIM)))))
	{
		GXD3D_SetProfundidadRE2( gSP.viewport.nearz, gSP.viewport.farz );
		GXD3D_SetRenderToDepth( 5 );
		GXD3D_SetDecalN64( 0 );
		GX_SetZMode( GX_ENABLE, GX_ALWAYS, GX_TRUE );
		GX_SetColorUpdate( GX_DISABLE );
		GX_SetAlphaUpdate( GX_DISABLE );
		gDP.changed |= CHANGED_RENDERMODE;
	}
	else if (gDP.colorImage.address == gDP.depthImageAddress &&
	    gDP.otherMode.cycleType <= G_CYC_2CYCLE)
	{
		const BOOL conZcmp = (gDP.otherMode.depthCompare != 0 &&
		                      ((gSP.geometryMode & G_ZBUFFER) ||
		                       gDP.otherMode.depthSource == G_ZS_PRIM)) ? TRUE : FALSE;
		GXD3D_SetRenderToDepth( conZcmp ? 2 : 1 );
		GXD3D_SetDecalN64( 0 );   /* manda el dibujo sobre el depth image */
		GX_SetZMode( GX_ENABLE, GX_ALWAYS, GX_TRUE );
		if (!OGL.frameBufferTextures)
		{
			GX_SetColorUpdate( GX_DISABLE );
			GX_SetAlphaUpdate( GX_DISABLE );
		}
		gDP.changed |= CHANGED_RENDERMODE;
	}
	else
		GXD3D_SetRenderToDepth( 0 );

	/* [Xbox360] [Beetle Adventure Racing, la niebla] El texrect de la niebla
	 * por Z (gln64_nieblaZ, ver _gDPTexrectNieblaZ en gDP.cpp): el shader lo
	 * pone el shim, y aqui el blend de GLideN64 ShadowMapShader::activate,
	 * glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ("DepthFogCombiner does
	 * not support shader blending": setBlendMode(true), el blend clasico), sin
	 * alpha compare: su shader no descarta.  CHANGED_RENDERMODE para que el
	 * siguiente dibujo recomponga el suyo. */
	if (gln64_nieblaZ)
	{
		GX_SetBlendMode( GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR );
		GX_SetAlphaCompare( GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0 );
		gDP.changed |= CHANGED_RENDERMODE;
	}
#endif // __GX__
}

#ifdef __GX__
// [Xbox360] The pending triangles need GXD3D_SetDepthClamp; see OGL_AddTriangle.
static BOOL s_gxZClamp = FALSE;
#endif // __GX__

/* [Xbox360] ★ [Mario Tennis, marcador: la franja de abajo corrida a la
 * izquierda y tapando "LUIGI"] LAS TEXTURAS DE FRAMEBUFFER, CON EL MISMO
 * MEDIO TEXEL QUE LAS NORMALES, en los triangulos.  glN64 centra las texturas
 * normales con `offsetS/T = 0.5` (TextureCache_Update) -- el N64 muestrea en
 * la esquina del texel y la GPU en el centro -- y a las de framebuffer se lo
 * olvido: su offset es solo el origen de la carga (FrameBuffer_ActivateBuffer
 * Texture), medio texel corrido.  GLideN64 no lleva ese medio texel en ningun
 * caso (su motor de texturas del shader imita el muestreo del N64) y resta 1
 * a las de framebuffer con hack_fbTextureOffset (Conker y Mario Tennis, sus
 * #519 y #2112).  Con esto la diferencia entre una textura normal y una de
 * framebuffer queda en la de GLideN64: el -1 del hack, nada mas.
 * MEDIDO por el usuario (2026-10-24) con una opcion temporal de tres valores:
 * el marcador es un panel 3D en tiras de su aux 0013BA50, las de arriba
 * servidas del framebuffer y la de abajo (filas 36-46, fuera de lo dibujado)
 * de la RDRAM; sin el medio texel se desfasaban 1.5 texels.  Con +0.5 queda
 * aceptable (1 texel, como GLideN64); +0.5 y sin el -1 lo cuadraba del todo
 * pero rompia la sombra de las bob-ombs (linea negra) y el borde del
 * marcador derecho.  Solo triangulos: los texrects de framebuffer van en
 * punto y sus copias 1:1 ya caen en el centro. */
static f32 _oglFbCentroExtra( const CachedTexture *c )
{
	return (c != NULL && c->frameBufferTexture) ? 0.5f : 0.0f;
}

void OGL_AddTriangle( SPVertex *vertices, int v0, int v1, int v2, bool primShade )
{
	GLN64_PROF_SCOPE(addtri);  /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	int v[] = { v0, v1, v2 };

	/* [Xbox360] El juego mandando geometria al BUFFER DE PROFUNDIDAD.  Ver la
	 * nota larga de GLN64_CI_ES_ZI en xenon/gln64_prof.h.  A 0 esto solo
	 * cuenta y el binario se comporta igual que antes. */
	/* ★★ TRAS EL MAESTRO, Y NO ES COSMETICA: ESTO CORRE POR TRIANGULO.
	 *
	 * Solo el `return` estaba tras un define; LA LLAMADA no.  Medido en San
	 * Francisco 2049: `OGL_AddTriangle` se invoca 160.962 veces por segundo y
	 * cuesta 0,81 us cada vez -- unos 2.600 ciclos para una funcion que
	 * calcula UVs y descarta.  Con dos sondas de una investigacion APARCADA
	 * (la sombra de Pilotwings) colgando de ese camino, ese numero no mide el
	 * emulador: mide las sondas.
	 *
	 * Y el hilo grafico es justo el cuello de ese juego (`espera` = 13-15 ms
	 * de los 16,6 disponibles), asi que esto sale del presupuesto que falta. */
#if XBOX64_SONDAS
	if (gln64_ci_es_zi( gDP.colorImage.address ))
	{
		gln64_dbg_cizi_tri++;
#if GLN64_CI_ES_ZI
		return;
#endif
	}
#endif

	/* [Xbox360] Censo de modos de render, con la ALTURA en pantalla a la que
	 * dibuja cada uno.  Ver gln64_rm_nota en xenon/gln64_config.cpp: es lo
	 * que identifica la sombra sin adivinar, porque el modo culpable es el
	 * que cambia de sitio entre volar sobre el mar y volar sobre tierra.
	 *
	 * Se toma el vertice 0 y no los tres: para una caja que solo tiene que
	 * decir "arriba o abajo" sobra, y ahorra cuatro divisiones por triangulo
	 * en un camino que se recorre cientos de miles de veces por segundo. */
	/* ★ LOS BITS DE ABAJO TAMBIEN, Y POR CAMPO CON NOMBRE.
	 *
	 * La primera version uso solo `otherMode.l >> 16`, que son los muxes del
	 * BLENDER, y en Pilotwings entero salieron CUATRO valores: no separan la
	 * sombra de nada.  Lo que distingue una sombra vive en los bits de abajo
	 * -- ZMODE (3 = DECAL), Z_CMP, Z_UPD, CVG_X_ALPHA, ALPHA_CVG_SEL --, asi
	 * que van en la clave.
	 *
	 * Se leen por CAMPO CON NOMBRE y no desplazando bits a mano: gDP.h define
	 * la struct con DOS ordenes distintos segun el endianness (gDP.h:165 y
	 * :246), asi que un `>> 10` escrito a ojo diria una cosa distinta de la
	 * que usa el propio plugin ([[reference_bitfield_endianness_be]]). */
	/* ★★ Y esta TAMBIEN por triangulo, y es la cara: `gln64_rm_nota` hace DOS
	 * divisiones en coma flotante y una busqueda LINEAL sobre una tabla, cada
	 * vez.  Ver la nota de arriba. */
#if XBOX64_SONDAS
	gln64_rm_nota( (unsigned int)(gDP.otherMode.l >> 16),
	               (unsigned int)( (gDP.otherMode.depthMode & 3)
	                             | (gDP.otherMode.depthCompare ? 0x04u : 0u)
	                             | (gDP.otherMode.depthUpdate  ? 0x08u : 0u)
	                             | (gDP.otherMode.alphaCvgSel  ? 0x10u : 0u)
	                             | (gDP.otherMode.cvgXAlpha    ? 0x20u : 0u)
	                             | (gDP.otherMode.forceBlender ? 0x40u : 0u)
	                             | (gDP.otherMode.clearOnCvg   ? 0x80u : 0u) ),
	               0, vertices[v0].y, vertices[v0].w );
#endif

#ifdef __GX__

	/* [Xbox360] ★ [Conker, la N de la intro; MM, la mascara que se acerca]
	 * Los microcodigos NoN no recortan contra el plano cercano.  GLideN64 lo
	 * emula con `ClampMode::NoNearPlaneClipping` = GL_DEPTH_CLAMP: el triangulo
	 * se dibuja ENTERO y la profundidad se sujeta por pixel.  glN64 solo lo
	 * imitaba cuando los TRES vertices quedan por delante del plano cercano
	 * (`tooNear == 3`: va por el camino sin combW, con la z sujeta en la CPU).
	 * Con uno o dos se quedaba en combW y D3D cortaba el trozo de delante: un
	 * pedazo del poligono transparente justo cuando la camara se le pega.
	 * Esos triangulos piden ahora la profundidad sujeta del shim
	 * (GXD3D_SetDepthClamp).  El plano LEJANO se sigue recortando, como en
	 * GLideN64 (su `gl_ClipDistance[0] = w - z`).
	 *
	 * ★ SOLO SI NINGUN VERTICE ESTA DETRAS DEL OJO (w <= 0).  Medido con un A/B
	 * (logs 38-39): pidiendola tambien para esos, aparecian huecos en el
	 * escenario de escenas concretas; sin ellos, ningun hueco y la N entera, y
	 * `tri` 200-3300/s con `detras = 0` en la escena.  Esos triangulos siguen
	 * como antes: los recorta D3D en el plano cercano. */
	BOOL zClamp = FALSE;

	if (OGL.GXcombWok && GBI_IsNoN())
	{
		// Without this, every non-Z-buffered draw in a NoN game loses
		// perspective-correct texturing (OoT market area).
		const BOOL zSupplied = (gSP.geometryMode & G_ZBUFFER) ? TRUE : FALSE;
		u32 onNear = 0, tooNear = 0, behind = 0;
		for (int c = 0; c < 3; c++)
		{
			const SPVertex &pv = vertices[v[c]];
			if (zSupplied && pv.w > 0.0f && (pv.z + pv.w) == 0.0f)
				onNear++;

			if (pv.w <= 0.0f)
				behind++;
			else if ((-OGL.GXcombW[2][2] + (OGL.GXcombW[2][3] / pv.w)) < -1.0f)
				tooNear++;
		}

		const BOOL useCombW =
			(onNear == 3 || (behind == 0 && tooNear == 3)) ? FALSE : TRUE;
		if (useCombW != OGL.GXuseCombW)
		{
			if (OGL.numTriangles)
				OGL_DrawTriangles();
			OGL.GXuseCombW = useCombW;
			OGL.GXupdateMtx = TRUE;
		}

		zClamp = (useCombW && tooNear != 0 && behind == 0) ? TRUE : FALSE;
	}

	if (zClamp != s_gxZClamp)
	{
		if (OGL.numTriangles)
			OGL_DrawTriangles();
		s_gxZClamp = zClamp;
	}

	if (!OGL.GXuseCombW && !(OGL.GXcombWok && GBI_IsNoN()))
	{
		for (int c = 0; c < 3; c++)
			if (vertices[v[c]].w <= 0.0f)
			{
				/* [Xbox360] ★ DESCARTE QUE DEPENDE DE LA CAMARA, y por eso
				 * se cuenta.  Pilotwings NO es NoN (GBI.cpp:91: el segundo
				 * campo de la tabla es NoN y vale FALSE), asi que esta rama
				 * SIEMPRE esta viva para ese juego: cualquier triangulo con
				 * un vertice detras del ojo se tira ENTERO.
				 *
				 * El usuario ha visto que en el REPLAY la sombra sale bien en
				 * el suelo y jugando no.  Misma geometria, misma display
				 * list, camara distinta -> un rechazo que depende de la
				 * camara es exactamente la forma que tiene ese sintoma.  Si
				 * este contador se dispara jugando y no en el replay, es
				 * aqui.  Ver gln64_dbg_wcull en xenon/gln64_config.cpp. */
#if XBOX64_TRAZAS
				gln64_dbg_wcull++;
#endif
				return;
			}
	}

	/* Triangulos emitidos con la matriz combinada de W (el camino que evita
	 * el recorte cercano).  Se lee EN PAREJA con wcull: si jugando estamos
	 * fuera de combW y en el replay dentro, la diferencia esta explicada. */
#if XBOX64_TRAZAS   /* contadores por triangulo: solo los lee el log */
	if (OGL.GXuseCombW)
		gln64_dbg_combw++;
	else
		/* ★ Y EL COMPLEMENTO, que es el que cuesta dinero.  Sin combW el
		 * lote divide POR VERTICE en la CPU (`invW`, OpenGL.cpp:1496), asi
		 * que este contador es directamente "cuanta geometria paga el divide
		 * de perspectiva a mano".  Un solo contador no sirve: `combw=1400`
		 * puede ser el 100% o el 5% segun lo que haya al lado. */
		gln64_dbg_nocombw++;
#endif

#endif // __GX__

	if (gSP.changed || gDP.changed)
		OGL_UpdateStates();

//	Playing around with lod fraction junk...
//	float ds = max( max( fabs( vertices[v0].s - vertices[v1].s ), fabs( vertices[v0].s - vertices[v2].s ) ), fabs( vertices[v1].s - vertices[v2].s ) ) * cache.current[0]->shiftScaleS * gSP.texture.scales;
//	float dx = max( max( fabs( vertices[v0].x / vertices[v0].w - vertices[v1].x / vertices[v1].w ), fabs( vertices[v0].x / vertices[v0].w - vertices[v2].x / vertices[v2].w ) ), fabs( vertices[v1].x / vertices[v1].w - vertices[v2].x / vertices[v2].w ) ) * gSP.viewport.vscale[0];
//	float lod = ds / dx;
//	float lod_fraction = min( 1.0f, max( 0.0f, lod - 1.0f ) / max( 1.0f, gSP.texture.level ) );


	/* ★★ LOS COEFICIENTES DEL UV, FUERA DEL BUCLE DE VERTICES.
	 *
	 * Todas las variantes de abajo son la misma forma afin:
	 *
	 *     s0 = (s * shiftScaleS * texPerspScaleS - fuls + offsetS) * scaleS
	 *        = s * (shiftScaleS * texPerspScaleS * scaleS)
	 *          + (offsetS - fuls) * scaleS
	 *
	 * y los dos coeficientes son POR DIBUJO: salen de `cache.current[]` y del
	 * tile, que no cambian dentro del lote -- un cambio de textura pasa por
	 * `OGL_UpdateStates`, que vacia el lote antes.
	 *
	 * ★ POR QUE NO LO HACE EL COMPILADOR.  Dentro del bucle hay escrituras a
	 * `OGL.vertices[...]`, y sin reglas de aliasing estrictas el compilador
	 * tiene que suponer que esas escrituras pueden tocar `cache.current[0]` o
	 * `gSP.textureTile[0]`.  Asi que releia los punteros y sus campos EN CADA
	 * VERTICE: no son las multiplicaciones lo que cuesta (~10 flops por
	 * vertice, 0,04 ms/frame a 590.000 vertices/s), son los accesos.
	 *
	 * ★ Y POR ESO NO SE MANDA A LA GPU.  Con los coeficientes ya en registros
	 * lo que queda por vertice es un `mad` por coordenada; moverlo al vertex
	 * shader ahorraria esos 0,04 ms y a cambio habria que meter una matriz de
	 * textura que el camino inmediato (VI.cpp, FrameBuffer.cpp, que mandan UV
	 * ya finales) tendria que aprender a desactivar.  Mal cambio. */
	const f32 texPerspHalf = (GBI_IsTexturePersp() && gDP.otherMode.texturePersp == 0)
	                         ? 0.5f : 1.0f;
	const f32 texPerspScaleS = gSP.texture.scales * texPerspHalf;
	const f32 texPerspScaleT = gSP.texture.scalet * texPerspHalf;

	f32 uvA0s = 0.0f, uvB0s = 0.0f, uvA0t = 0.0f, uvB0t = 0.0f;
	f32 uvA1s = 0.0f, uvB1s = 0.0f, uvA1t = 0.0f, uvB1t = 0.0f;

	if (combiner.usesT0)
	{
		const CachedTexture *c = cache.current[0];
		const gDPTile       *t = gSP.textureTile[0];
		/* El `fmod` solo aparece en el camino de framebuffer-como-textura, y
		 * su argumento tambien es por dibujo: sigue siendo una constante. */
		const f32 fs = (c->frameBufferTexture && t->masks)
		             ? (f32)fmod( t->fuls, 1 << t->masks ) : t->fuls;
		const f32 ft = (c->frameBufferTexture && t->maskt)
		             ? (f32)fmod( t->fult, 1 << t->maskt ) : t->fult;
		const f32 extra = _oglFbCentroExtra( c );
		uvA0s = c->shiftScaleS * texPerspScaleS * c->scaleS;
		uvB0s = (c->offsetS + extra - fs) * c->scaleS;
		uvA0t = c->shiftScaleT * texPerspScaleT * c->scaleT;
		uvB0t = (c->offsetT + extra - ft) * c->scaleT;
	}
	if (combiner.usesT1)
	{
		const CachedTexture *c = cache.current[1];
		const gDPTile       *t = gSP.textureTile[1];
		const f32 extra = _oglFbCentroExtra( c );
		uvA1s = c->shiftScaleS * texPerspScaleS * c->scaleS;
		uvB1s = (c->offsetS + extra - t->fuls) * c->scaleS;
		uvA1t = c->shiftScaleT * texPerspScaleT * c->scaleT;
		uvB1t = (c->offsetT + extra - t->fult) * c->scaleT;
	}

	for (int i = 0; i < 3; i++)
	{
		OGL.vertices[OGL.numVertices].x = vertices[v[i]].x;
		OGL.vertices[OGL.numVertices].y = vertices[v[i]].y;
#ifndef __GX__
		OGL.vertices[OGL.numVertices].z = gDP.otherMode.depthSource == G_ZS_PRIM ? gDP.primDepth.z * vertices[v[i]].w : vertices[v[i]].z;
#else // !__GX__
		//TODO: primDepthZ should now be handled with a Ztex. Verify this.
		//Note: Could also handle primDepthZ by manipulating the Projection matrix
//		OGL.vertices[OGL.numVertices].z = (gDP.otherMode.depthSource == G_ZS_PRIM) && !(OGL.GXuseProj) ? gDP.primDepth.z * vertices[v[i]].w : vertices[v[i]].z;
		OGL.vertices[OGL.numVertices].z = vertices[v[i]].z;
#endif // __GX__
		OGL.vertices[OGL.numVertices].w = vertices[v[i]].w;

		/* [Xbox360] ★ [Pilotwings 64] EL "PRIM SHADING" DE LA REFERENCIA.
		 *
		 * Sin G_LIGHTING y sin G_SHADE el RSP no calcula sombreado: el color
		 * que ve el combinador es el PRIMITIVO.  GLideN64 lo hace en
		 * GraphicsDrawer::addTriangle (GraphicsDrawer.cpp:59-68); glN64 solo
		 * en el microcodigo F5Rogue, y en el camino normal se quedaba con el
		 * color que traia el vertice.  Pilotwings dibuja asi el fondo de toda
		 * la pantalla, por detras del viewport de 620x448: con el marcado,
		 * textura NEGRA, sombreado BLANCO y en pantalla un marco blanco; en la
		 * consola sale negro.  Solo con `primShade` (lo pide gSPTriangle). */
		if (primShade)
		{
			OGL.vertices[OGL.numVertices].color.r = gDP.primColor.r;
			OGL.vertices[OGL.numVertices].color.g = gDP.primColor.g;
			OGL.vertices[OGL.numVertices].color.b = gDP.primColor.b;
			OGL.vertices[OGL.numVertices].color.a = gDP.primColor.a;
		}
		else
		{
			OGL.vertices[OGL.numVertices].color.r = vertices[v[i]].r;
			OGL.vertices[OGL.numVertices].color.g = vertices[v[i]].g;
			OGL.vertices[OGL.numVertices].color.b = vertices[v[i]].b;
			OGL.vertices[OGL.numVertices].color.a = vertices[v[i]].a;
		}
		SetConstant( OGL.vertices[OGL.numVertices].color, combiner.vertex.color, combiner.vertex.alpha );
		//SetConstant( OGL.vertices[OGL.numVertices].secondaryColor, combiner.vertex.secondaryColor, ONE );

		if (OGL.EXT_secondary_color)
		{
			OGL.vertices[OGL.numVertices].secondaryColor.r = 0.0f;//lod_fraction; //vertices[v[i]].r;
			OGL.vertices[OGL.numVertices].secondaryColor.g = 0.0f;//lod_fraction; //vertices[v[i]].g;
			OGL.vertices[OGL.numVertices].secondaryColor.b = 0.0f;//lod_fraction; //vertices[v[i]].b;
			OGL.vertices[OGL.numVertices].secondaryColor.a = 1.0f;
			SetConstant( OGL.vertices[OGL.numVertices].secondaryColor, combiner.vertex.secondaryColor, ONE );
		}

#ifndef __GX__
		if ((gSP.geometryMode & G_FOG) && OGL.EXT_fog_coord && OGL.fog)
		{
			if (vertices[v[i]].z < -vertices[v[i]].w)
				OGL.vertices[OGL.numVertices].fog = max( 0.0f, -(float)gSP.fog.multiplier + (float)gSP.fog.offset );
			else
				OGL.vertices[OGL.numVertices].fog = max( 0.0f, vertices[v[i]].z / vertices[v[i]].w * (float)gSP.fog.multiplier + (float)gSP.fog.offset );
		}
#else //!__GX__
		//Fog is taken care of in hardware with GX.
#endif //__GX__

		/* ★ Dos `mad` por unidad de textura.  Los coeficientes se calcularon
		 * una vez antes del bucle; la nota larga esta alli.
		 *
		 * Las ramas que habia aqui (framebuffer-como-textura y su variante
		 * con `fmod`) NO desaparecen: estan plegadas en `uvB0s`/`uvB0t`, que
		 * es donde vivia su unica diferencia.  Las dos ramas de T1 eran
		 * algebraicamente identicas -- `(o + (s*K - f))*e` y
		 * `(s*K - f + o)*e` --, asi que ahi no habia nada que plegar; y la
		 * condicion miraba `cache.current[0]` teniendo T1 delante, que parece
		 * un desliz de Wii64 pero daba igual por lo mismo.  Se conserva el
		 * comportamiento, no el codigo. */
		if (combiner.usesT0)
		{
			OGL.vertices[OGL.numVertices].s0 = vertices[v[i]].s * uvA0s + uvB0s;
			OGL.vertices[OGL.numVertices].t0 = vertices[v[i]].t * uvA0t + uvB0t;
		}
		if (combiner.usesT1)
		{
			OGL.vertices[OGL.numVertices].s1 = vertices[v[i]].s * uvA1s + uvB1s;
			OGL.vertices[OGL.numVertices].t1 = vertices[v[i]].t * uvA1t + uvB1t;
		}
		OGL.numVertices++;
	}
	OGL.numTriangles++;

	if (OGL.numVertices >= 255)
		OGL_DrawTriangles();
}

#ifdef SHOW_DEBUG
	int CntTriProj, CntTriProjW, CntTriOther, CntTriNear, CntTriPolyOffset;
#endif

void OGL_DrawTriangles()
{
	GLN64_PROF_SCOPE(drawtri); /* [Xbox360] sonda temporal, xenon/gln64_prof.h */

	if (OGL.usePolygonStipple && (gDP.otherMode.alphaCompare == G_AC_DITHER) && !(gDP.otherMode.alphaCvgSel))
	{
		OGL.lastStipple = (OGL.lastStipple + 1) & 0x7;
#ifndef __GX__
		glPolygonStipple( OGL.stipplePattern[(BYTE)(gDP.envColor.a * 255.0f) >> 3][OGL.lastStipple] );
#else // !__GX__
		//TODO: find equivalent
#endif // __GX__
	}

#ifndef __GX__
	glDrawArrays( GL_TRIANGLES, 0, OGL.numVertices );
#else // !__GX__

#ifdef GLN64_SDLOG
	sprintf(txtbuffer,"OGL_DrawTris: numTri %d, numVert %d, useT0 %d, useT1 %d\n", OGL.numTriangles, OGL.numVertices, combiner.usesT0, combiner.usesT1);
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
#endif // GLN64_SDLOG

	//Update MV & P Matrices
	if(OGL.GXupdateMtx)
	{
		OGL_UpdateViewport();
		if(OGL.GXuseCombW)
			GX_LoadProjectionMtx(OGL.GXcombW, GX_PERSPECTIVE);
		else
			GX_LoadProjectionMtx(OGL.GXprojIdent, GX_ORTHOGRAPHIC);
		OGL.GXupdateMtx = false;
	}

	if ((gDP.otherMode.depthSource == G_ZS_PRIM)||OGL.GXuseAlphaCompare)
	{
		GX_SetZCompLoc(GX_FALSE);	// Do Z-compare after texturing. (i.e. use Ztex)
#if XBOX64_TRAZAS   /* contador de SHOW_DEBUG */
		cache.GXZTexPrimCnt++;
#endif
	}
	else
	{
		GX_SetZCompLoc(GX_TRUE);	// Do Z-compare before texturing.
#if XBOX64_TRAZAS   /* contador de SHOW_DEBUG */
		cache.GXnoZTexPrimCnt++;
#endif
	}

	/* [Xbox360] ★ AQUI IBA LA DECLARACION DEL FORMATO DE VERTICE: 13 llamadas
	 * (`GX_ClearVtxDesc` + 7 `GX_SetVtxDesc` + 5 `GX_SetVtxAttrFmt`) ANTES DE
	 * CADA LOTE, para un formato que no cambia nunca.  Con 5.981 lotes/s en
	 * OoT eran 78.000 llamadas por segundo, y el propio upstream lo decia:
	 * *"//TODO: These only need to be set once during init."*
	 *
	 * Ya no hacen falta EN ESTA FUNCION porque este camino no pasa por el modo
	 * inmediato: se escribe el vertice entero de una vez (abajo).
	 *
	 * ★ Y NO SE SUBEN A `OGL_Start`, que era la idea obvia y es FALSA: en el
	 * plugin hay NUEVE sitios que declaran formatos DISTINTOS (OGL_DrawRect
	 * con POS+CLR0+TEX2, VI.cpp con POS+TEX0 y sin color, FrameBuffer.cpp con
	 * POS_XY...).  El descriptor es estado global del shim; fijarlo una vez en
	 * el arranque romperia a los otros ocho.  Cada uno de ellos empieza por
	 * `GX_ClearVtxDesc()` y pone el suyo, asi que quitarlo de aqui es seguro. */

#ifdef SHOW_DEBUG
/*	if (OGL.GXuseCombW) CntTriProjW += OGL.numTriangles;
	else CntTriOther += OGL.numTriangles;
	if (OGL.GXuseCombW && OGL.GXuseProjWnear) CntTriNear += OGL.numTriangles;
	if (OGL.GXpolyOffset) CntTriPolyOffset += OGL.numTriangles;*/
#endif

	/* [Xbox360] ★★ EL LOTE ENTERO DE UNA PASADA, NO CINCO LLAMADAS POR VERTICE.
	 *
	 * El bucle de antes empujaba cada vertice por el modo inmediato de GX:
	 * `GX_Position3f32` + `GX_Color4u8` + TRES `GX_TexCoord2f32`.  En Wii eso
	 * son stores al write-gather pipe y sale gratis; aqui cada una es una
	 * llamada cross-modulo al shim que escribe en una estatica de fichero y la
	 * vuelve a leer acto seguido -- un Load-Hit-Store detras de otro en el
	 * PPE-- para acabar copiando un vertice que YA ESTABA en `OGL.vertices`.
	 *
	 * Ahora el shim presta el sitio del vertex buffer y se rellena aqui.  Son
	 * los MISMOS vertices, en el mismo orden y con el mismo estado: es un
	 * cambio de transporte.  Las medidas y el porque estan en
	 * xenon/gxd3d_lote.h.
	 *
	 * Los `if` de dentro del bucle son invariantes del lote, asi que el
	 * predictor de saltos los acierta siempre; lo caro eran las llamadas, no
	 * las ramas. */
	/* [Xbox360] La profundidad sujeta, solo para este lote (ver s_gxZClamp en
	 * OGL_AddTriangle).  Se apaga al cerrarlo: asi ningun otro dibujo -- VI,
	 * rects, FrameBuffer -- la hereda, y el shim vacia el lote al cambiar. */
	if (s_gxZClamp && OGL.GXuseCombW)
		GXD3D_SetDepthClamp( 1 );
	{
		const int nv = (int)OGL.numVertices;
		GXD3DVertex *dst = gxd3d_lote_abrir( nv );
		if (dst)
		{
			const int useT0 = combiner.usesT0 ? 1 : 0;
			const int useT1 = combiner.usesT1 ? 1 : 0;
			const int combW = OGL.GXuseCombW  ? 1 : 0;
			/* ★★ NIEBLA: el coeficiente se calcula AQUI, POR VERTICE, con la
			 * formula literal del glN64 original (linea 1309 de este fichero,
			 * bajo `#ifndef __GX__` porque glN64_GX delega en el hardware de
			 * GX y el shim de D3D9 no lo tiene).
			 *
			 * ★ Y tiene que ser aqui y no en el vertex shader: en el camino
			 * `combW` el vertice sale con `z = -w` y la z real se pierde, asi
			 * que la GPU no puede reconstruirla.  Se intento y dejo a OoT sin
			 * niebla mientras 007, que va por el otro camino, salia bien.
			 *
			 * La bandera se saca del bucle: la division solo se paga en la
			 * geometria que de verdad lleva niebla, que es la minoria
			 * (`[GXFOG] on=1186 off=2603` en OoT). */
			const int conNiebla = (gSP.geometryMode & G_FOG) ? 1 : 0;
			const f32 fogMul = (f32)gSP.fog.multiplier;
			const f32 fogOff = (f32)gSP.fog.offset;
			/* El camino `combW` NO dividia por w: solo se paga el reciproco si
			 * lo pide el otro camino o la niebla.  Los dos son invariantes del
			 * lote, asi que la rama es predecible. */
			const int necesitaInvW = (!combW) || conNiebla;
			int i;
			for (i = 0; i < nv; i++)
			{
				const GLVertex *v = &OGL.vertices[i];
				const f32 invW = necesitaInvW
				               ? ((v->w != 0) ? 1/v->w : 0.0f)
				               : 0.0f;
				if (combW)
				{
					dst[i].x = v->x;
					dst[i].y = v->y;
					dst[i].z = -v->w;
				}
				else
				{
					// Clamp rather than let GX clip, GLideN64 does GL_DEPTH_CLAMP.
					f32 zClamp = v->z*invW;
					if      (zClamp < -1.0f) zClamp = -1.0f;
					else if (zClamp >  1.0f) zClamp =  1.0f;
					dst[i].x = v->x*invW;
					dst[i].y = v->y*invW;
					dst[i].z = zClamp;
				}
				if (conNiebla)
				{
					/* El caso `z < -w` es el del original: el vertice queda
					 * por delante del plano cercano y se usa el valor del
					 * extremo en vez de dividir por una `w` que no sirve. */
					f32 fg = (v->z < -v->w) ? (-fogMul + fogOff)
					                        : (v->z * invW * fogMul + fogOff);
					dst[i].fog = (fg > 0.0f) ? fg : 0.0f;
				}
				dst[i].color = GXD3D_PACK_RGBA( GXcastf32u8(v->color.r),
				                                GXcastf32u8(v->color.g),
				                                GXcastf32u8(v->color.b),
				                                GXcastf32u8(v->color.a) );

				if (useT0) { dst[i].s0 = v->s0; dst[i].t0 = v->t0; }
				else       { dst[i].s0 = 0.0f;  dst[i].t0 = 0.0f;  }
				if (useT1) { dst[i].s1 = v->s1; dst[i].t1 = v->t1; }
				else       { dst[i].s1 = 0.0f;  dst[i].t1 = 0.0f;  }
#ifdef GLN64_SDLOG
				sprintf(txtbuffer," Vert%d: Pos x = %.2f, y = %.2f, z = %.2f, w = %.2f, RGBA = %08X, VertCol RGBA = %.2f, %.2f, %.2f, %.2f\n", i, v->x/v->w, v->y/v->w, v->z/v->w, v->w, (unsigned int)dst[i].color, v->color.r, v->color.g, v->color.b, v->color.a);
				DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
#endif // GLN64_SDLOG
			}
			gxd3d_lote_cerrar( nv );
		}
	}
	GXD3D_SetDepthClamp( 0 );
#endif // __GX__
	OGL.numTriangles = OGL.numVertices = 0;
}

void OGL_DrawLine( SPVertex *vertices, int v0, int v1, float width )
{
	int v[] = { v0, v1 };

	GLcolor color;

	if (gSP.changed || gDP.changed)
		OGL_UpdateStates();

#ifndef __GX__
	glLineWidth( width * OGL.scaleX );

	glBegin( GL_LINES );
		for (int i = 0; i < 2; i++)
		{
			color.r = vertices[v[i]].r;
			color.g = vertices[v[i]].g;
			color.b = vertices[v[i]].b;
			color.a = vertices[v[i]].a;
			SetConstant( color, combiner.vertex.color, combiner.vertex.alpha );
			glColor4fv( &color.r );

			if (OGL.EXT_secondary_color)
			{
				color.r = vertices[v[i]].r;
				color.g = vertices[v[i]].g;
				color.b = vertices[v[i]].b;
				color.a = vertices[v[i]].a;
				SetConstant( color, combiner.vertex.secondaryColor, combiner.vertex.alpha );
				glSecondaryColor3fvEXT( &color.r );
			}

			glVertex4f( vertices[v[i]].x, vertices[v[i]].y, vertices[v[i]].z, vertices[v[i]].w );
		}
	glEnd();
#else // !__GX__		//TODO: Implement secondary color.
//	GX_SetLineWidth( width * OGL.scaleX, GX_TO_ZERO );
	GX_SetLineWidth( width * OGL.GXscaleX * 6, GX_TO_ZERO );

	GXColor GXcol;
	float invW;

	//Update MV & P Matrices
	if(OGL.GXupdateMtx)
	{
		OGL_UpdateViewport();
		if(OGL.GXuseCombW)
			GX_LoadProjectionMtx(OGL.GXcombW, GX_PERSPECTIVE);
		else
			GX_LoadProjectionMtx(OGL.GXprojIdent, GX_ORTHOGRAPHIC);
		OGL.GXupdateMtx = false;
	}

	if ((gDP.otherMode.depthSource == G_ZS_PRIM)||OGL.GXuseAlphaCompare)
	{
		GX_SetZCompLoc(GX_FALSE);	// Do Z-compare after texturing. (i.e. use Ztex)
#if XBOX64_TRAZAS   /* contador de SHOW_DEBUG */
		cache.GXZTexPrimCnt++;
#endif
	}
	else
	{
		GX_SetZCompLoc(GX_TRUE);	// Do Z-compare before texturing.
#if XBOX64_TRAZAS   /* contador de SHOW_DEBUG */
		cache.GXnoZTexPrimCnt++;
#endif
	}

	//set vertex description here
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX2MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX2, GX_DIRECT);
	//set vertex attribute formats here
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX2, GX_TEX_ST, GX_F32, 0);
	/* [Xbox360] ★ [Mario Golf, la rejilla de nivelacion] LA POSICION SALE DE
	 * `vertices[v[i]]`, NO DE `OGL.vertices[i]`.  `OGL.vertices` es el bufer de
	 * TRIANGULOS: la linea salia con las posiciones de los dos primeros
	 * vertices del ultimo lote de triangulos -- una linea suelta que se movia
	 * con la camara, y ninguna rejilla.  Lo metio Wii64 e7f8747 ("Matrix
	 * cleanup") al copiar el bloque de OGL_DrawTriangles; antes leia
	 * `vertices[v[i]]` como el camino GL de arriba y como el color de aqui
	 * abajo.  OGL_AddTriangle copia x, y, z, w tal cual, asi que es la misma
	 * transformacion. */
	GX_Begin(GX_LINES, GX_VTXFMT0, 2);
		for (int i = 0; i < 2; i++)
		{
			const SPVertex *sv = &vertices[v[i]];
			if(OGL.GXuseCombW)
				GX_Position3f32( sv->x, sv->y, -sv->w );
			else
			{
				invW = (sv->w != 0) ? 1/sv->w : 0.0f;
				// Same GL_DEPTH_CLAMP as abovev
				f32 zClamp = sv->z*invW;
				if      (zClamp < -1.0f) zClamp = -1.0f;
				else if (zClamp >  1.0f) zClamp =  1.0f;
				GX_Position3f32( sv->x*invW, sv->y*invW, zClamp );
			}
			color.r = vertices[v[i]].r;
			color.g = vertices[v[i]].g;
			color.b = vertices[v[i]].b;
			color.a = vertices[v[i]].a;
			SetConstant( color, combiner.vertex.color, combiner.vertex.alpha );
			GXcol.r = GXcastf32u8(color.r);
			GXcol.g = GXcastf32u8(color.g);
			GXcol.b = GXcastf32u8(color.b);
			GXcol.a = GXcastf32u8(color.a);
			GX_Color4u8(GXcol.r, GXcol.g, GXcol.b, GXcol.a); 
			GX_TexCoord2f32(0.0f,0.0f);
		}
	GX_End();
#endif // __GX__
}

#ifdef __GX__
/* [Xbox360] [Killer Instinct, cielo de los combates] Triangulos en coordenadas de
 * PANTALLA del N64, para los triangulos de bajo nivel del RDP (gDPLLETriangle).
 * Es el `drawScreenSpaceTriangle` de GLideN64 sobre el camino que ya funciona en
 * glN64 para lo mismo, ZSortBOSS_DrawScreenSpaceTriangle (ZSortBOSS.cpp):
 * misma conversion de coordenadas de textura, mismo recorte, misma proyeccion
 * (x,y ya en ndc * w y profundidad fija), y la tira (v[i], v[i+1], v[i+2]) que
 * LLETriangle::draw le da a GLideN64.  El paso de pixeles a ndc va sobre el
 * ancho/alto del COLOR IMAGE (gln64_rect_ancho/alto), como OGL_DrawRect.
 *
 * Marcado de diagnostico `lle` (GLN64_MARCA_LLE): sin textura y con el
 * combinador SHADE, todos de MAGENTA, para ver que zona cubren. */
void OGL_DrawScreenSpaceTriangles( SPVertex *vertices, u32 count, BOOL textured )
{
	struct { f32 s, t, s1, t1; } tc[8];
	GXColor col[8];
	u32 i;

	if (count < 3)
		return;
	if (count > 8)
		count = 8;

	if (OGL.numTriangles)
		OGL_DrawTriangles();

	OGL_UpdateStates();

	const BOOL marcar = (gln64_marcaluz == GLN64_MARCA_LLE) ? TRUE : FALSE;
	if (marcar)
	{
		Combiner_SetCombine( EncodeCombineMode( 0, 0, 0, SHADE, 0, 0, 0, 1, 0, 0, 0, SHADE, 0, 0, 0, 1 ) );
		textured = FALSE;
	}

	// The same scissor as ZSortBOSS_DrawScreenSpaceTriangle, with std::max/min spelled out.
	f32 tijX, tijY;
	_oglTijeraLR( &tijX, &tijY );	// [Xbox360] [Resident Evil 2]
	float ulx1 = OGL.GXorigX + gDP.scissor.ulx * OGL.GXscaleX;
	float uly1 = OGL.GXorigY + gDP.scissor.uly * OGL.GXscaleY;
	float lrx1 = tijX * OGL.GXscaleX;
	float lry1 = tijY * OGL.GXscaleY;
	if (lrx1 > (float)OGL.GXwidth)  lrx1 = (float)OGL.GXwidth;
	if (lry1 > (float)OGL.GXheight) lry1 = (float)OGL.GXheight;
	lrx1 += OGL.GXorigX;
	lry1 += OGL.GXorigY;
	if (ulx1 < 0.0f) ulx1 = 0.0f;
	if (uly1 < 0.0f) uly1 = 0.0f;
	if (lrx1 < 0.0f) lrx1 = 0.0f;
	if (lry1 < 0.0f) lry1 = 0.0f;
	GX_SetScissor( (u32)ulx1, (u32)uly1, (u32)(lrx1 - ulx1), (u32)(lry1 - uly1) );
	GX_SetCullMode( GX_CULL_NONE );
	GX_SetViewport( (f32)OGL.GXorigX, (f32)OGL.GXorigY, (f32)OGL.GXwidth, (f32)OGL.GXheight, 0.0f, 1.0f );

	Mtx44 proj;
	memset( proj, 0, sizeof(proj) );
	proj[0][0] = 1.0f;	// clip.x = x  (x is already ndc * w)
	proj[1][1] = 1.0f;	// clip.y = y
	proj[2][2] = 0.5f;
	proj[2][3] = 0.0f;
	GX_LoadProjectionMtx( proj, GX_PERSPECTIVE );
	GX_LoadPosMtxImm( OGL.GXmodelViewIdent, GX_PNMTX0 );

	if ((gDP.otherMode.depthSource == G_ZS_PRIM) || OGL.GXuseAlphaCompare)
		GX_SetZCompLoc( GX_FALSE );
	else
		GX_SetZCompLoc( GX_TRUE );

	for (i = 0; i < count; ++i)
	{
		tc[i].s = tc[i].s1 = vertices[i].s;
		tc[i].t = tc[i].t1 = vertices[i].t;
		if (marcar)
		{
			col[i].r = 255;  col[i].g = 0;  col[i].b = 255;  col[i].a = 255;
		}
		else
		{
			col[i].r = GXcastf32u8( vertices[i].r );
			col[i].g = GXcastf32u8( vertices[i].g );
			col[i].b = GXcastf32u8( vertices[i].b );
			col[i].a = GXcastf32u8( vertices[i].a );
		}
	}

	if (textured && combiner.usesT0 && cache.current[0] != NULL)
	{
		const bool fbTex0 = (cache.current[0]->frameBufferTexture != 0);
		for (i = 0; i < count; ++i)
		{
			if (fbTex0)
			{
				if (gSP.textureTile[0]->masks)
					tc[i].s = (cache.current[0]->offsetS + (tc[i].s * cache.current[0]->shiftScaleS * gSP.texture.scales - fmod( gSP.textureTile[0]->fuls, 1 << gSP.textureTile[0]->masks ))) * cache.current[0]->scaleS;
				else
					tc[i].s = (cache.current[0]->offsetS + (tc[i].s * cache.current[0]->shiftScaleS * gSP.texture.scales - gSP.textureTile[0]->fuls)) * cache.current[0]->scaleS;

				if (gSP.textureTile[0]->maskt)
					tc[i].t = (cache.current[0]->offsetT + (tc[i].t * cache.current[0]->shiftScaleT * gSP.texture.scalet - fmod( gSP.textureTile[0]->fult, 1 << gSP.textureTile[0]->maskt ))) * cache.current[0]->scaleT;
				else
					tc[i].t = (cache.current[0]->offsetT + (tc[i].t * cache.current[0]->shiftScaleT * gSP.texture.scalet - gSP.textureTile[0]->fult)) * cache.current[0]->scaleT;
			}
			else
			{
				tc[i].s = (tc[i].s * cache.current[0]->shiftScaleS * gSP.texture.scales - gSP.textureTile[0]->fuls + cache.current[0]->offsetS) * cache.current[0]->scaleS;
				tc[i].t = (tc[i].t * cache.current[0]->shiftScaleT * gSP.texture.scalet - gSP.textureTile[0]->fult + cache.current[0]->offsetT) * cache.current[0]->scaleT;
			}
		}
	}

	bool useTex1 = false;
	if (textured && combiner.usesT1 && OGL.ARB_multitexture && cache.current[1] != NULL)
	{
		useTex1 = true;
		const bool fbTex1 = (cache.current[1]->frameBufferTexture != 0);
		for (i = 0; i < count; ++i)
		{
			if (fbTex1)
			{
				tc[i].s1 = (cache.current[1]->offsetS + (tc[i].s1 * cache.current[1]->shiftScaleS * gSP.texture.scales - gSP.textureTile[1]->fuls)) * cache.current[1]->scaleS;
				tc[i].t1 = (cache.current[1]->offsetT + (tc[i].t1 * cache.current[1]->shiftScaleT * gSP.texture.scalet - gSP.textureTile[1]->fult)) * cache.current[1]->scaleT;
			}
			else
			{
				tc[i].s1 = (tc[i].s1 * cache.current[1]->shiftScaleS * gSP.texture.scales - gSP.textureTile[1]->fuls + cache.current[1]->offsetS) * cache.current[1]->scaleS;
				tc[i].t1 = (tc[i].t1 * cache.current[1]->shiftScaleT * gSP.texture.scalet - gSP.textureTile[1]->fult + cache.current[1]->offsetT) * cache.current[1]->scaleT;
			}
		}
	}

	GX_ClearVtxDesc();
	GX_SetVtxDesc( GX_VA_PTNMTXIDX, GX_PNMTX0 );
	GX_SetVtxDesc( GX_VA_TEX0MTXIDX, GX_TEXMTX0 );
	GX_SetVtxDesc( GX_VA_TEX1MTXIDX, GX_TEXMTX0 );
	GX_SetVtxDesc( GX_VA_TEX2MTXIDX, GX_TEXMTX0 );
	GX_SetVtxDesc( GX_VA_POS, GX_DIRECT );
	GX_SetVtxDesc( GX_VA_CLR0, GX_DIRECT );
	GX_SetVtxDesc( GX_VA_TEX0, GX_DIRECT );
	GX_SetVtxDesc( GX_VA_TEX1, GX_DIRECT );
	GX_SetVtxDesc( GX_VA_TEX2, GX_DIRECT );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0 );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0 );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0 );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0 );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_TEX2, GX_TEX_ST, GX_F32, 0 );

	const f32 ndcScaleX = 2.0f / gln64_rect_ancho();
	const f32 ndcScaleY = 2.0f / gln64_rect_alto();
	GX_Begin( GX_TRIANGLES, GX_VTXFMT0, (u16)((count - 2) * 3) );
	for (i = 0; i + 2 < count; ++i)
	{
		u32 j;
		for (j = 0; j < 3; ++j)
		{
			const u32 idx = i + j;
			const f32 vw = (vertices[idx].w > 0.0f) ? vertices[idx].w : 1.0f;
			const f32 ndcX = vertices[idx].x * ndcScaleX - 1.0f;
			const f32 ndcY = 1.0f - vertices[idx].y * ndcScaleY;
			GX_Position3f32( ndcX * vw, ndcY * vw, -vw );
			GX_Color4u8( col[idx].r, col[idx].g, col[idx].b, col[idx].a );
			GX_TexCoord2f32( tc[idx].s, tc[idx].t );
			if (useTex1)
				GX_TexCoord2f32( tc[idx].s1, tc[idx].t1 );
			else
				GX_TexCoord2f32( 0.0f, 0.0f );
			GX_TexCoord2f32( 0.0f, 0.0f );
		}
	}
	GX_End();

	OGL.GXupdateMtx = true;
	gDP.changed |= CHANGED_SCISSOR; // Restore scissor in OGL_UpdateStates() before drawing next geometry.
	if (marcar)
		gDP.changed |= CHANGED_COMBINE;
	OGL_UpdateCullFace();
	OGL_UpdateViewport();

	if (depthBuffer.current) depthBuffer.current->cleared = FALSE;

	gDPBufferChanged( gDP.scissor.lry );
}
#endif // __GX__

void OGL_DrawRect( int ulx, int uly, int lrx, int lry, float *color )
{
	GLN64_PROF_SCOPE(rect);    /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	OGL_UpdateStates();

#ifndef __GX__
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_CULL_FACE );
	glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
	glOrtho( 0, VI.width, VI.height, 0, 1.0f, -1.0f );
	glViewport( 0, OGL.heightOffset, OGL.width, OGL.height );
	glDepthRange( 0.0f, 1.0f );

	glColor4f( color[0], color[1], color[2], color[3] );

	glBegin( GL_QUADS );
		glVertex4f( ulx, uly, (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz, 1.0f );
		glVertex4f( lrx, uly, (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz, 1.0f );
		glVertex4f( lrx, lry, (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz, 1.0f );
		glVertex4f( ulx, lry, (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz, 1.0f );
	glEnd();

	glLoadIdentity();
	OGL_UpdateCullFace();
	OGL_UpdateViewport();
	glEnable( GL_SCISSOR_TEST );
#else // !__GX__
	/* [Xbox360] ★★ [Star Fox 64, estela de estrellas en los bordes] EL SCISSOR
	 * DEL JUEGO TAMBIEN RECORTA LOS RELLENOS.  glN64 lo quitaba aqui ("Disable
	 * Scissor", y glDisable(GL_SCISSOR_TEST) en la rama de PC); el RDP recorta
	 * con el todas las primitivas, y GLideN64 tambien (GraphicsDrawer::drawRect
	 * pasa por _updateStates -> updateScissor, y fillRDRAM lo clampa).  Los
	 * texrect de aqui ya lo aplicaban (OGL_DrawTexturedRect).  MEDIDO (log 17,
	 * [FILL-GX]): Star Fox pinta las estrellas con rellenos de 1x1 con el
	 * scissor en 8,8-312,232 y solo borra 8,8-312,233; las que caian en el
	 * marco (174,2 / 5,154 ...) se pintaban y nadie las borraba.
	 *
	 * ★ Scissor DEGENERADO (ancho o alto 0): sin recortar, como antes.  gDP no
	 * se inicializa y vale 0,0-0,0 hasta el primer SetScissor; ese cero no es
	 * un estado del RDP.  Misma cuenta que OGL_UpdateStates. */
	if (gDP.scissor.lrx > gDP.scissor.ulx && gDP.scissor.lry > gDP.scissor.uly)
	{
		f32 tijX, tijY;
		_oglTijeraLR( &tijX, &tijY );	// [Xbox360] [Resident Evil 2]
		float ulx1 = max(OGL.GXorigX + gDP.scissor.ulx * OGL.GXscaleX, 0);
		float uly1 = max(OGL.GXorigY + gDP.scissor.uly * OGL.GXscaleY, 0);
		float lrx1 = max(OGL.GXorigX + min(tijX * OGL.GXscaleX,OGL.GXwidth), 0);
		float lry1 = max(OGL.GXorigY + min(tijY * OGL.GXscaleY,OGL.GXheight), 0);
		GX_SetScissor((u32) ulx1,(u32) uly1,(u32) (lrx1 - ulx1),(u32) (lry1 - uly1));
	}
	else
		GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);	//Disable Scissor
	GX_SetCullMode (GX_CULL_NONE);
	Mtx44 GXprojection;
	/* [Xbox360] Sobre el ancho del COLOR IMAGE, no sobre el del VI: un panel
	 * auxiliar mas ancho que el VI se salia por la derecha en esa misma
	 * proporcion.  Ver gln64_rect_ancho en xenon/gln64_config.cpp. */
	guOrtho(GXprojection, 0, gln64_rect_alto(), 0, gln64_rect_ancho(), 1.0f, -1.0f);
	GX_LoadProjectionMtx(GXprojection, GX_ORTHOGRAPHIC);
	GX_LoadPosMtxImm(OGL.GXmodelViewIdent,GX_PNMTX0);

	GX_SetViewport((f32) OGL.GXorigX,(f32) OGL.GXorigY,(f32) OGL.GXwidth,(f32) OGL.GXheight, 0.0f, 1.0f);

	GXColor GXcol;
	GXcol.r = GXcastf32u8(color[0]);
	GXcol.g = GXcastf32u8(color[1]);
	GXcol.b = GXcastf32u8(color[2]);
	GXcol.a = GXcastf32u8(color[3]);

	if ((gDP.otherMode.depthSource == G_ZS_PRIM)||OGL.GXuseAlphaCompare)
	{
		GX_SetZCompLoc(GX_FALSE);	// Do Z-compare after texturing. (i.e. use Ztex)
#if XBOX64_TRAZAS   /* contador de SHOW_DEBUG */
		cache.GXZTexPrimCnt++;
#endif
	}
	else
	{
		GX_SetZCompLoc(GX_TRUE);	// Do Z-compare before texturing.
#if XBOX64_TRAZAS   /* contador de SHOW_DEBUG */
		cache.GXnoZTexPrimCnt++;
#endif
	}

	//set vertex description here
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX2MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX2, GX_DIRECT);

	//set vertex attribute formats here
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX2, GX_TEX_ST, GX_F32, 0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);	//TODO: This may fail for G_ZS_PRIM... log and fix, maybe with guOrtho
//		GX_Position3f32( ulx, uly, (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz );
		GX_Position3f32( ulx, uly, gSP.viewport.nearz );
		GX_Color4u8(GXcol.r, GXcol.g, GXcol.b, GXcol.a); 
		GX_TexCoord2f32(0.0f,0.0f);
//		GX_Position3f32( lrx, uly, (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz );
		GX_Position3f32( lrx, uly, gSP.viewport.nearz );
		GX_Color4u8(GXcol.r, GXcol.g, GXcol.b, GXcol.a); 
		GX_TexCoord2f32(0.0f,0.0f);
//		GX_Position3f32( lrx, lry, (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz );
		GX_Position3f32( lrx, lry, gSP.viewport.nearz );
		GX_Color4u8(GXcol.r, GXcol.g, GXcol.b, GXcol.a); 
		GX_TexCoord2f32(0.0f,0.0f);
//		GX_Position3f32( ulx, lry, (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz );
		GX_Position3f32( ulx, lry, gSP.viewport.nearz );
		GX_Color4u8(GXcol.r, GXcol.g, GXcol.b, GXcol.a); 
		GX_TexCoord2f32(0.0f,0.0f);
	GX_End();

	OGL.GXupdateMtx = true;

	OGL_UpdateCullFace();
	OGL_UpdateViewport();
	gDP.changed |= CHANGED_SCISSOR;	//Restore scissor in OGL_UpdateStates() before drawing next geometry.
#endif // __GX__
}

/* [Xbox360] ★★ [Mario Tennis, pantalla VS] DIBUJAR EN EL DEPTH IMAGE ESCRIBE
 * PROFUNDIDAD, como GLideN64 con `enableFragmentDepthWrite` (su valor por
 * defecto, Config.cpp:66): si el color image es el depth image, prueba de
 * profundidad ALWAYS con escritura (GraphicsDrawer.cpp:684-689, 794-821) y el
 * shader hace `gl_FragDepth = fragColor.r` (uRenderTarget 1,
 * glsl_CombinerProgramBuilderCommon.cpp:959-970).  El buffer de profundidad es
 * el mismo para todos los color image, asi que eso es lo que ve el 3D de
 * despues.
 *
 * La VS lo usa de mascara: pinta una escalera de rellenos 0x0000 sobre el depth
 * image (z = 0, lo mas cercano) a la izquierda de la diagonal, y la vista del
 * jugador, dibujada despues con prueba de profundidad, solo entra a la
 * derecha; a la izquierda queda el rival.  glN64 lo pintaba solo como color y
 * el jugador tapaba la pantalla entera.
 *
 * Solo profundidad: el color lo sigue pintando OGL_DrawRect, como antes.  Solo
 * el relleno (color constante): un texrect sobre el depth image pondria la
 * profundidad por pixel desde la textura y eso pide un pixel shader con
 * salida de profundidad que el shim no tiene.  La z, con la misma convencion
 * que el borrado del EFB (OGL_GXclearEFB: orto 0..1, z = -profundidad). */
void OGL_DrawDepthFill( int ulx, int uly, int lrx, int lry, float depth )
{
#ifdef __GX__
	if (OGL.numTriangles)
		OGL_DrawTriangles();
	OGL_UpdateStates();

	if (depth < 0.0f) depth = 0.0f;
	if (depth > 1.0f) depth = 1.0f;
	const f32 z = -depth * ((f32) 0xFFFFFF / 0x1000000);

	GX_SetColorUpdate(GX_DISABLE);
	GX_SetAlphaUpdate(GX_DISABLE);
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_TRUE);

	GX_SetNumChans(1);
	GX_SetNumTexGens(0);
	GX_SetNumTevStages(1);
	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXMAP_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z8,0);
	GX_SetZCompLoc(GX_TRUE);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	GX_SetFog(GX_FOG_NONE,0.1,1.0,0.0,1.0,GXCOL_BLACK);   /* [Xbox360] literal compuesto */
	GX_SetViewport((f32) OGL.GXorigX,(f32) OGL.GXorigY,(f32) OGL.GXwidth,(f32) OGL.GXheight, 0.0f, 1.0f);
	GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);	//Disable Scissor
	GX_SetCullMode (GX_CULL_NONE);
	Mtx44 GXprojection;
	/* Las coordenadas N64 del rect, sobre el color image, como OGL_DrawRect;
	 * la z, como OGL_GXclearEFB. */
	guOrtho(GXprojection, 0, gln64_rect_alto(), 0, gln64_rect_ancho(), 0.0f, 1.0f);
	GX_LoadProjectionMtx(GXprojection, GX_ORTHOGRAPHIC);
	GX_LoadPosMtxImm(OGL.GXmodelViewIdent,GX_PNMTX0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32((f32) ulx, (f32) uly, z);
		GX_Color4u8(0, 0, 0, 0);
		GX_Position3f32((f32) lrx, (f32) uly, z);
		GX_Color4u8(0, 0, 0, 0);
		GX_Position3f32((f32) lrx, (f32) lry, z);
		GX_Color4u8(0, 0, 0, 0);
		GX_Position3f32((f32) ulx, (f32) lry, z);
		GX_Color4u8(0, 0, 0, 0);
	GX_End();

	/* Lo mismo que deja OGL_GXclearEFB al salir. */
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	OGL.GXupdateMtx = true;
	OGL.GXupdateFog = true;
	OGL_UpdateCullFace();
	OGL_UpdateViewport();
	gDP.changed |= CHANGED_SCISSOR | CHANGED_COMBINE | CHANGED_RENDERMODE;
#endif // __GX__
}

void OGL_DrawTexturedRect( float ulx, float uly, float lrx, float lry, float uls, float ult, float lrs, float lrt, bool flip, const float *colorOverride )
{
	GLN64_PROF_SCOPE(rect);    /* [Xbox360] sonda temporal, xenon/gln64_prof.h */

	/* [Xbox360] Gemela de la de OGL_AddTriangle: los texrects tambien pueden
	 * ir al buffer de profundidad.  Se cuentan APARTE porque saber cual de
	 * los dos caminos usa el juego cambia donde hay que mirar despues. */
	/* Mismas dos sondas que en `OGL_AddTriangle`, tras el maestro por la misma
	 * razon.  Aqui pesan menos (1.650 rects/s contra 160.962 triangulos), pero
	 * dejarlas fuera seria dejar media palanca puesta. */
#if XBOX64_SONDAS
	if (gln64_ci_es_zi( gDP.colorImage.address ))
	{
		gln64_dbg_cizi_rect++;
#if GLN64_CI_ES_ZI
		return;
#endif
	}

	gln64_rm_nota( (unsigned int)(gDP.otherMode.l >> 16),
	               (unsigned int)( (gDP.otherMode.depthMode & 3)
	                             | (gDP.otherMode.depthCompare ? 0x04u : 0u)
	                             | (gDP.otherMode.depthUpdate  ? 0x08u : 0u)
	                             | (gDP.otherMode.alphaCvgSel  ? 0x10u : 0u)
	                             | (gDP.otherMode.cvgXAlpha    ? 0x20u : 0u)
	                             | (gDP.otherMode.forceBlender ? 0x40u : 0u)
	                             | (gDP.otherMode.clearOnCvg   ? 0x80u : 0u) ),
	               1, 0.0f, 0.0f );
#endif

#ifdef __GX__
	OGL.GXrenderTexRect = true;
#endif //__GX__
	/* [Xbox360] ★ [Pilotwings 64] EL SHADE DE UN TEXRECT ES 0, COMO EN LA
	 * REFERENCIA.  Un texrect no tiene sombreado: GLideN64 lo dibuja con
	 * `gDP.rectColor = gDPInfo::Color()` -- (0,0,0,0), gDP.cpp:906 -- y su
	 * vertex shader de rectangulos usa ese uRectColor como SHADE.  glN64 ponia
	 * aqui {1,1,1,0}: con un combinador que es SOLO SHADE, el rect salia
	 * BLANCO.  MEDIDO con [MARCO-GX]: Pilotwings tapa el margen de la pantalla
	 * con texrects asi (`R caja=0,222-319,240 mux=00FFFFFF:FFFE7838`, d0 =
	 * SHADE y Ad0 = SHADE) -- el marco blanco; en la consola es negro.  El alfa
	 * ya era 0 y no cambia. */
	GLVertex rect[2] =
	{	//TODO: This may fail for G_ZS_PRIM... log and fix, maybe with guOrtho
		{ ulx, uly, gDP.otherMode.depthSource == G_ZS_PRIM ? gDP.primDepth.z : gSP.viewport.nearz, 1.0f, { /*gDP.blendColor.r, gDP.blendColor.g, gDP.blendColor.b, gDP.blendColor.a */0.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, uls, ult, uls, ult, 0.0f },
		{ lrx, lry, gDP.otherMode.depthSource == G_ZS_PRIM ? gDP.primDepth.z : gSP.viewport.nearz, 1.0f, { /*gDP.blendColor.r, gDP.blendColor.g, gDP.blendColor.b, gDP.blendColor.a*/0.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, lrs, lrt, lrs, lrt, 0.0f },
	};

	OGL_UpdateStates();

#ifdef __GX__
	// If we have fog applied, we're going to possibly darken the texture here.
	// Link's subscreen picture was being made way too dark here on GX.
	// The PC code path doesn't need to do this because with GL_FOG_COORDINATE_SOURCE_EXT
	// that means "no fog" regardless of what the scene left
	// enabled and on GX we don't have per-vertex fog, so set to nothing here.
	GX_SetFog(GX_FOG_NONE, 0.1f, 1.0f, 0.0f, 1.0f, GXCOL_BLACK);   /* [Xbox360] literal compuesto */
#endif // __GX__

#ifndef __GX__
	glDisable( GL_CULL_FACE );
	glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
	glOrtho( 0, VI.width, VI.height, 0, 1.0f, -1.0f );
	glViewport( 0, OGL.heightOffset, OGL.width, OGL.height );
#else // !__GX__
	//Note: Scissoring may need to be reworked here
	f32 tijX, tijY;
	_oglTijeraLR( &tijX, &tijY );	// [Xbox360] [Resident Evil 2]
	float ulx1 = max(OGL.GXorigX + gDP.scissor.ulx * OGL.GXscaleX, 0);
	float uly1 = max(OGL.GXorigY + gDP.scissor.uly * OGL.GXscaleY, 0);
	float lrx1 = max(OGL.GXorigX + min(tijX * OGL.GXscaleX,OGL.GXwidth), 0);
	float lry1 = max(OGL.GXorigY + min(tijY * OGL.GXscaleY,OGL.GXheight), 0);
	GX_SetScissor((u32) ulx1,(u32) uly1,(u32) (lrx1 - ulx1),(u32) (lry1 - uly1));
	GX_SetCullMode (GX_CULL_NONE);
	Mtx44 GXprojection;
	/* [Xbox360] Gemela de la de OGL_DrawRect: sobre el ancho del COLOR
	 * IMAGE.  Ver gln64_rect_ancho en xenon/gln64_config.cpp. */
	guOrtho(GXprojection, 0, gln64_rect_alto(), 0, gln64_rect_ancho(), 1.0f, -1.0f);
	GX_LoadProjectionMtx(GXprojection, GX_ORTHOGRAPHIC);
	GX_LoadPosMtxImm(OGL.GXmodelViewIdent,GX_PNMTX0);

	GX_SetViewport((f32) OGL.GXorigX,(f32) OGL.GXorigY,(f32) OGL.GXwidth,(f32) OGL.GXheight, 0.0f, 1.0f);
#endif // __GX__

	if (combiner.usesT0)
	{
		const f32 edgeS0 = gDP.texRect.dsdx * cache.current[0]->shiftScaleS;
		const f32 edgeT0 = gDP.texRect.dtdy * cache.current[0]->shiftScaleT;
		rect[0].s0 = rect[0].s0 * cache.current[0]->shiftScaleS - gSP.textureTile[0]->fuls;
		rect[0].t0 = rect[0].t0 * cache.current[0]->shiftScaleT - gSP.textureTile[0]->fult;
		rect[1].s0 = rect[1].s0 * cache.current[0]->shiftScaleS + edgeS0 - gSP.textureTile[0]->fuls;
		rect[1].t0 = rect[1].t0 * cache.current[0]->shiftScaleT + edgeT0 - gSP.textureTile[0]->fult;
		if ((cache.current[0]->maskS) && (fmod( rect[0].s0, cache.current[0]->width ) == 0.0f) && !(cache.current[0]->mirrorS))
		{
			rect[1].s0 -= rect[0].s0;
			rect[0].s0 = 0.0f;
		}

		if ((cache.current[0]->maskT) && (fmod( rect[0].t0, cache.current[0]->height ) == 0.0f) && !(cache.current[0]->mirrorT))
		{
			rect[1].t0 -= rect[0].t0;
			rect[0].t0 = 0.0f;
		}

		if (cache.current[0]->frameBufferTexture)
		{
#ifndef __GX__
			rect[0].s0 = cache.current[0]->offsetS + rect[0].s0;
			rect[0].t0 = cache.current[0]->offsetT - rect[0].t0;
			rect[1].s0 = cache.current[0]->offsetS + rect[1].s0;
			rect[1].t0 = cache.current[0]->offsetT - rect[1].t0;
#else //!__GX__
			rect[0].s0 = cache.current[0]->offsetS + rect[0].s0;
			rect[0].t0 = cache.current[0]->offsetT + rect[0].t0;
			rect[1].s0 = cache.current[0]->offsetS + rect[1].s0;
			rect[1].t0 = cache.current[0]->offsetT + rect[1].t0;
#endif //__GX__
		}

#ifndef __GX__	
		if (OGL.ARB_multitexture)
			glActiveTextureARB( GL_TEXTURE0_ARB );

		if ((rect[0].s0 >= 0.0f) && (rect[1].s0 <= cache.current[0]->width))
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		//glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		//glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

		if ((rect[0].t0 >= 0.0f) && (rect[1].t0 <= cache.current[0]->height))
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
#else // !__GX__
		if ((rect[0].s0 >= 0.0f) && (rect[1].s0 <= cache.current[0]->width))
			OGL.GXforceClampS0 = true;
		if ((rect[0].t0 >= 0.0f) && (rect[1].t0 <= cache.current[0]->height))
			OGL.GXforceClampT0 = true;
#endif // __GX__

//		GLint height;

//		glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height );

		rect[0].s0 *= cache.current[0]->scaleS;
		rect[0].t0 *= cache.current[0]->scaleT;
		rect[1].s0 *= cache.current[0]->scaleS;
		rect[1].t0 *= cache.current[0]->scaleT;
	}

	if (combiner.usesT1 && OGL.ARB_multitexture)
	{
		rect[0].s1 = rect[0].s1 * cache.current[1]->shiftScaleS - gSP.textureTile[1]->fuls;
		rect[0].t1 = rect[0].t1 * cache.current[1]->shiftScaleT - gSP.textureTile[1]->fult;
		const f32 edgeS1 = gDP.texRect.dsdx * cache.current[1]->shiftScaleS;
		const f32 edgeT1 = gDP.texRect.dtdy * cache.current[1]->shiftScaleT;
		rect[1].s1 = rect[1].s1 * cache.current[1]->shiftScaleS + edgeS1 - gSP.textureTile[1]->fuls;
		rect[1].t1 = rect[1].t1 * cache.current[1]->shiftScaleT + edgeT1 - gSP.textureTile[1]->fult;

		if ((cache.current[1]->maskS) && (fmod( rect[0].s1, cache.current[1]->width ) == 0.0f) && !(cache.current[1]->mirrorS))
		{
			rect[1].s1 -= rect[0].s1;
			rect[0].s1 = 0.0f;
		}

		if ((cache.current[1]->maskT) && (fmod( rect[0].t1, cache.current[1]->height ) == 0.0f) && !(cache.current[1]->mirrorT))
		{
			rect[1].t1 -= rect[0].t1;
			rect[0].t1 = 0.0f;
		}

		if (cache.current[1]->frameBufferTexture)
		{
#ifndef __GX__
			rect[0].s1 = cache.current[1]->offsetS + rect[0].s1;
			rect[0].t1 = cache.current[1]->offsetT - rect[0].t1;
			rect[1].s1 = cache.current[1]->offsetS + rect[1].s1;
			rect[1].t1 = cache.current[1]->offsetT - rect[1].t1;
#else //!__GX__
			rect[0].s1 = cache.current[1]->offsetS + rect[0].s1;
			rect[0].t1 = cache.current[1]->offsetT + rect[0].t1;
			rect[1].s1 = cache.current[1]->offsetS + rect[1].s1;
			rect[1].t1 = cache.current[1]->offsetT + rect[1].t1;
#endif //__GX__
		}

#ifndef __GX__	
		glActiveTextureARB( GL_TEXTURE1_ARB );

		if ((rect[0].s1 == 0.0f) && (rect[1].s1 <= cache.current[1]->width))
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );

		if ((rect[0].t1 == 0.0f) && (rect[1].t1 <= cache.current[1]->height))
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
#else // !__GX__
		if ((rect[0].s1 == 0.0f) && (rect[1].s1 <= cache.current[1]->width))
			OGL.GXforceClampS1 = true;
		if ((rect[0].t1 == 0.0f) && (rect[1].t1 <= cache.current[1]->height))
			OGL.GXforceClampT1 = true;
#endif // __GX__

		rect[0].s1 *= cache.current[1]->scaleS;
		rect[0].t1 *= cache.current[1]->scaleT;
		rect[1].s1 *= cache.current[1]->scaleS;
		rect[1].t1 *= cache.current[1]->scaleT;
	}

#ifndef __GX__	
	if ((gDP.otherMode.cycleType == G_CYC_COPY) && !OGL.forceBilinear)
	{
		if (OGL.ARB_multitexture)
			glActiveTextureARB( GL_TEXTURE0_ARB );

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
#else // !__GX__
	//TODO: Set LOD texture filter modes here.
	/* [Xbox360] ★★ [Quest 64, de cuatro "1" iguales del HUD solo el primero sin
	 * borde magenta] EL FILTRO POINT SE DECIDE AQUI EN CADA TEXRECT, con la
	 * misma condicion que TextureCache_ActivateTexture (Textures.cpp).  Hasta
	 * ahora solo lo ponia esa funcion, que solo corre cuando CAMBIA la textura,
	 * y el final de este dibujo lo vuelve a poner a false: el primer texrect con
	 * una textura salia con GX_NEAR y los siguientes con la MISMA textura con
	 * GX_LINEAR.  MEDIDO con [TXRSM] (Logs Salvia (6)): h=00008CF0 (POINT) con
	 * near=0; con esto near=1 y el HUD sale bien (log 7, sondas retiradas).
	 * Viene de Wii64.  Y se calcula ENTERO desde false: lo que hubiera
	 * dejado la activacion de la textura no vale si el modo cambio despues. */
	OGL.GXuseMinMagNearest = false;
	if (((gDP.otherMode.cycleType == G_CYC_COPY) ||
	     !((gDP.otherMode.textureFilter == G_TF_BILERP) || (gDP.otherMode.textureFilter == G_TF_AVERAGE))) &&
	    !OGL.forceBilinear)
		OGL.GXuseMinMagNearest = true;
	/* [Xbox360] ★ [Quest 64, ribete magenta en cada letra] UN TEXRECT FILTRADO
	 * A ESCALA 1:1 Y ALINEADO AL TEXEL ES, EN EL N64, UN MUESTREO SIN MEZCLA.
	 * El RDP muestrea en la esquina superior izquierda del pixel: con s,t
	 * enteros y dsdx = dtdy = 1 la fraccion del bilineal es 0 y sale el texel
	 * tal cual, igual que POINT.  Aqui se dibuja a 2x: cada pixel del N64 son
	 * cuatro, sus muestras caen a 1/4 y 3/4 del texel y el bilineal mezcla un
	 * 25 de cada vecino.  MEDIDO con [TXRQ] (Logs Salvia, sonda retirada): el
	 * texto es 1 ciclo, BILERP, CI4/CI8 con TLUT RGBA16 cuyo transparente es
	 * MAGENTA, alpha compare THRESHOLD con blend a = 1/255 y blender IN*0+IN*1
	 * (sin mezcla): el cuarto de texel transparente pasa la prueba y se pinta
	 * opaco con su magenta.  Es lo que GLideN64 resuelve dibujando los texrect a
	 * resolucion nativa (graphics2D.enableNativeResTexrects; en su ini para
	 * Mario Kart 64 y 40 Winks).  Decidido por el usuario: global.  Solo si los
	 * tiles usados no tienen shift (shiftScale 1); los framebuffer ya van con
	 * GX_NEAR abajo. */
	else if (gDP.otherMode.textureFilter != G_TF_POINT && !OGL.forceBilinear &&
	         gDP.texRect.dsdx == 1.0f && gDP.texRect.dtdy == 1.0f &&
	         uls == floorf( uls ) && ult == floorf( ult ) &&
	         (!combiner.usesT0 || (cache.current[0]->shiftScaleS == 1.0f && cache.current[0]->shiftScaleT == 1.0f)) &&
	         (!combiner.usesT1 || (cache.current[1]->shiftScaleS == 1.0f && cache.current[1]->shiftScaleT == 1.0f)))
		OGL.GXuseMinMagNearest = true;

	if (combiner.usesT0 && cache.current[0]->GXtexture != NULL)
	{
		if (cache.enable2xSaI && !cache.current[0]->frameBufferTexture)
			GX_InitTexObj(&cache.current[0]->GXtex, cache.current[0]->GXtexture, (u16) cache.current[0]->realWidth << 1, 
				(u16) cache.current[0]->realHeight << 1, cache.current[0]->GXtexfmt, 
				(cache.current[0]->clampS || OGL.GXforceClampS0) ? GX_CLAMP : GX_REPEAT, 
				(cache.current[0]->clampT || OGL.GXforceClampT0) ? GX_CLAMP : GX_REPEAT, GX_FALSE); 
		else
			/* [Xbox360] ★ [Killer Instinct, retrato del lobo en tiras rotas,
			 * 2026-10-24] El espejo por la GPU (GXespejoS/T, ver
			 * TextureCache_Update) TAMBIEN aqui: esta llamada rehace el objeto de
			 * los texrect y ponia GX_REPEAT, y una textura del tamano de la
			 * mascara se repetia SIN reflejar.  Las tiras de KI muestrean s
			 * 212..256 (segunda mitad del periodo de 128) y salian de la mitad
			 * equivocada; las de Worms no salen de la primera y no se veia. */
			GX_InitTexObj(&cache.current[0]->GXtex, cache.current[0]->GXtexture, (u16) cache.current[0]->realWidth,
				(u16) cache.current[0]->realHeight, cache.current[0]->GXtexfmt,
				(cache.current[0]->clampS || OGL.GXforceClampS0) ? GX_CLAMP : (cache.current[0]->GXespejoS ? GX_MIRROR : GX_REPEAT),
				(cache.current[0]->clampT || OGL.GXforceClampT0) ? GX_CLAMP : (cache.current[0]->GXespejoT ? GX_MIRROR : GX_REPEAT),
				(cache.current[0]->max_level > 0) ? GX_TRUE : GX_FALSE);
		if (cache.current[0]->frameBufferTexture || OGL.GXuseMinMagNearest)
			GX_InitTexObjLOD(&cache.current[0]->GXtex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
		else if (cache.current[0]->max_level > 0)
			GX_InitTexObjLOD(&cache.current[0]->GXtex, GX_LIN_MIP_LIN, GX_LINEAR,
			                 0.0f, (f32)cache.current[0]->max_level, 0.0f,
			                 GX_TRUE, GX_TRUE, GX_ANISO_4);
		else
			GX_InitTexObjLOD(&cache.current[0]->GXtex, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_TRUE, GX_TRUE, GX_ANISO_4);
		GX_LoadTexObj(&cache.current[0]->GXtex, GX_TEXMAP0); // t = 0 is GX_TEXMAP0 and t = 1 is GX_TEXMAP1
	}

	if (combiner.usesT1 && OGL.ARB_multitexture && cache.current[1]->GXtexture != NULL)
	{
		if (cache.enable2xSaI && !cache.current[1]->frameBufferTexture)
			GX_InitTexObj(&cache.current[1]->GXtex, cache.current[1]->GXtexture, (u16) cache.current[1]->realWidth << 1, 
				(u16) cache.current[1]->realHeight << 1, cache.current[1]->GXtexfmt, 
				(cache.current[1]->clampS || OGL.GXforceClampS1) ? GX_CLAMP : GX_REPEAT, 
				(cache.current[1]->clampT || OGL.GXforceClampT1) ? GX_CLAMP : GX_REPEAT, GX_FALSE); 
		else
			/* [Xbox360] [Killer Instinct] Espejo por la GPU: ver la unidad 0. */
			GX_InitTexObj(&cache.current[1]->GXtex, cache.current[1]->GXtexture, (u16) cache.current[1]->realWidth,
				(u16) cache.current[1]->realHeight, cache.current[1]->GXtexfmt,
				(cache.current[1]->clampS || OGL.GXforceClampS1) ? GX_CLAMP : (cache.current[1]->GXespejoS ? GX_MIRROR : GX_REPEAT),
				(cache.current[1]->clampT || OGL.GXforceClampT1) ? GX_CLAMP : (cache.current[1]->GXespejoT ? GX_MIRROR : GX_REPEAT),
				(cache.current[1]->max_level > 0) ? GX_TRUE : GX_FALSE);
		if (cache.current[1]->frameBufferTexture || OGL.GXuseMinMagNearest)
			GX_InitTexObjLOD(&cache.current[1]->GXtex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
		else if (cache.current[1]->max_level > 0)
			GX_InitTexObjLOD(&cache.current[1]->GXtex, GX_LIN_MIP_LIN, GX_LINEAR,
			                 0.0f, (f32)cache.current[1]->max_level, 0.0f,
			                 GX_TRUE, GX_TRUE, GX_ANISO_4);
		else
			GX_InitTexObjLOD(&cache.current[1]->GXtex, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_TRUE, GX_TRUE, GX_ANISO_4);
		if (cache.current[1]->atlasNiveles)
			TextureCache_InitAtlasTexObj( cache.current[1] );	// [Xbox360] el atlas lo filtra el shader
		GX_LoadTexObj(&cache.current[1]->GXtex, GX_TEXMAP1); // t = 0 is GX_TEXMAP0 and t = 1 is GX_TEXMAP1
	}

	OGL.GXforceClampS0 = false;
	OGL.GXforceClampT0 = false;
	OGL.GXforceClampS1 = false;
	OGL.GXforceClampT1 = false;
	OGL.GXuseMinMagNearest = false;
#endif // __GX__

    if (colorOverride)
	{
		rect[0].color.r = colorOverride[0];
		rect[0].color.g = colorOverride[1];
		rect[0].color.b = colorOverride[2];
		rect[0].color.a = colorOverride[3];
	}
	else
	{
		SetConstant( rect[0].color, combiner.vertex.color, combiner.vertex.alpha );
		if(GetGameSpecificHack() == &hack_dukenukem) {
			/* [Xbox360] asignacion con llaves (C++11): campo a campo para MSVC2010. */
			rect[0].color.r = 0.0f;
			rect[0].color.g = 0.0f;
			rect[0].color.b = 0.0f;
			rect[0].color.a = 0.0f;
		}
	}

	if (OGL.EXT_secondary_color)
		SetConstant( rect[0].secondaryColor, combiner.vertex.secondaryColor, combiner.vertex.alpha );

#ifndef __GX__
	glBegin( GL_QUADS );
		glColor4f( rect[0].color.r, rect[0].color.g, rect[0].color.b, rect[0].color.a );
		if (OGL.EXT_secondary_color)
			glSecondaryColor3fEXT( rect[0].secondaryColor.r, rect[0].secondaryColor.g, rect[0].secondaryColor.b );

		if (OGL.ARB_multitexture)
		{
			glMultiTexCoord2fARB( GL_TEXTURE0_ARB, rect[0].s0, rect[0].t0 );
			glMultiTexCoord2fARB( GL_TEXTURE1_ARB, rect[0].s1, rect[0].t1 );
			glVertex4f( rect[0].x, rect[0].y, rect[0].z, 1.0f );

			glMultiTexCoord2fARB( GL_TEXTURE0_ARB, rect[1].s0, rect[0].t0 );
			glMultiTexCoord2fARB( GL_TEXTURE1_ARB, rect[1].s1, rect[0].t1 );
			glVertex4f( rect[1].x, rect[0].y, rect[0].z, 1.0f );

			glMultiTexCoord2fARB( GL_TEXTURE0_ARB, rect[1].s0, rect[1].t0 );
			glMultiTexCoord2fARB( GL_TEXTURE1_ARB, rect[1].s1, rect[1].t1 );
			glVertex4f( rect[1].x, rect[1].y, rect[0].z, 1.0f );

			glMultiTexCoord2fARB( GL_TEXTURE0_ARB, rect[0].s0, rect[1].t0 );
			glMultiTexCoord2fARB( GL_TEXTURE1_ARB, rect[0].s1, rect[1].t1 );
			glVertex4f( rect[0].x, rect[1].y, rect[0].z, 1.0f );
		}
		else
		{
			glTexCoord2f( rect[0].s0, rect[0].t0 );
			glVertex4f( rect[0].x, rect[0].y, rect[0].z, 1.0f );

			if (flip)
				glTexCoord2f( rect[1].s0, rect[0].t0 );
			else
				glTexCoord2f( rect[0].s0, rect[1].t0 );

			glVertex4f( rect[1].x, rect[0].y, rect[0].z, 1.0f );

			glTexCoord2f( rect[1].s0, rect[1].t0 );
			glVertex4f( rect[1].x, rect[1].y, rect[0].z, 1.0f );

			if (flip)
				glTexCoord2f( rect[1].s0, rect[0].t0 );
			else
				glTexCoord2f( rect[1].s0, rect[0].t0 );
			glVertex4f( rect[0].x, rect[1].y, rect[0].z, 1.0f );
		}
	glEnd();
#else // !__GX__
	GXColor GXcol;

	GXcol.r = GXcastf32u8(rect[0].color.r);
	GXcol.g = GXcastf32u8(rect[0].color.g);
	GXcol.b = GXcastf32u8(rect[0].color.b);
	GXcol.a = GXcastf32u8(rect[0].color.a);

	if ((gDP.otherMode.depthSource == G_ZS_PRIM)||OGL.GXuseAlphaCompare)
	{
		GX_SetZCompLoc(GX_FALSE);	// Do Z-compare after texturing. (i.e. use Ztex)
#if XBOX64_TRAZAS   /* contador de SHOW_DEBUG */
		cache.GXZTexPrimCnt++;
#endif
	}
	else
	{
		GX_SetZCompLoc(GX_TRUE);	// Do Z-compare before texturing.
#if XBOX64_TRAZAS   /* contador de SHOW_DEBUG */
		cache.GXnoZTexPrimCnt++;
#endif
	}

	//set vertex description here
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_TEX1MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_TEX2MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX1, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX2, GX_DIRECT);
	//set vertex attribute formats here
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX2, GX_TEX_ST, GX_F32, 0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32( rect[0].x, rect[0].y, rect[0].z );
		GX_Color4u8( GXcol.r, GXcol.g, GXcol.b, GXcol.a ); 
		GX_TexCoord2f32( rect[0].s0, rect[0].t0 );
		GX_TexCoord2f32( rect[0].s1, rect[0].t1 );
		GX_TexCoord2f32( 0.0, 0.0 );
		GX_Position3f32( rect[1].x, rect[0].y, rect[0].z );
		GX_Color4u8(GXcol.r, GXcol.g, GXcol.b, GXcol.a); 
		if (flip)
		{
			GX_TexCoord2f32( rect[0].s0, rect[1].t0 );
			GX_TexCoord2f32( rect[0].s1, rect[1].t1 );
		}
		else	
		{
			GX_TexCoord2f32( rect[1].s0, rect[0].t0 );
			GX_TexCoord2f32( rect[1].s1, rect[0].t1 );
		}
		GX_TexCoord2f32( 0.0, 0.0 );
		GX_Position3f32( rect[1].x, rect[1].y, rect[0].z );
		GX_Color4u8( GXcol.r, GXcol.g, GXcol.b, GXcol.a ); 
		GX_TexCoord2f32( rect[1].s0, rect[1].t0 );
		GX_TexCoord2f32( rect[1].s1, rect[1].t1 );
		GX_TexCoord2f32( 0.0, 0.0 );
		GX_Position3f32( rect[0].x, rect[1].y, rect[0].z );
		GX_Color4u8(GXcol.r, GXcol.g, GXcol.b, GXcol.a); 
		if (flip)
		{
			GX_TexCoord2f32( rect[1].s0, rect[0].t0 );
			GX_TexCoord2f32( rect[1].s1, rect[0].t1 );
		}
		else
		{
			GX_TexCoord2f32( rect[0].s0, rect[1].t0 );
			GX_TexCoord2f32( rect[0].s1, rect[1].t1 );
		}
		GX_TexCoord2f32( 0.0, 0.0 );
	GX_End();
#endif // __GX__

#ifndef __GX__
	glLoadIdentity();
#else // !__GX__
	OGL.GXrenderTexRect = false;
	// Put back whatever fog the scene had configured.
	GX_SetFog(OGL.GXfogType, OGL.GXfogStartZ, OGL.GXfogEndZ, -1.0f, 1.0f, OGL.GXfogColor);
	OGL.GXupdateMtx = true;
	gDP.changed |= CHANGED_SCISSOR;	//Restore scissor in OGL_UpdateStates() before drawing next geometry.
#endif // __GX__
	OGL_UpdateCullFace();
	OGL_UpdateViewport();
}

void OGL_ClearDepthBuffer()
{
#ifndef __GX__
	glDisable( GL_SCISSOR_TEST );

	OGL_UpdateStates();
	glDepthMask( TRUE );
	glClear( GL_DEPTH_BUFFER_BIT );

	OGL_UpdateDepthUpdate();

	glEnable( GL_SCISSOR_TEST );
#else // !__GX__
	//Note: OGL_UpdateDepthUpdate() should not need to be called b/c DepthMask is set in OGL_UpdateStates()
	OGL.GXclearDepthBuffer = true;
	gDP.changed |= CHANGED_RENDERMODE;
#endif // __GX__
}

void OGL_ClearColorBuffer( float *color )
{
#ifndef __GX__
	glDisable( GL_SCISSOR_TEST );

	glClearColor( color[0], color[1], color[2], color[3] );
	glClear( GL_COLOR_BUFFER_BIT );

	glEnable( GL_SCISSOR_TEST );
#else // !__GX__
	OGL.GXclearColor.r = GXcastf32u8(color[0]);
	OGL.GXclearColor.g = GXcastf32u8(color[1]);
	OGL.GXclearColor.b = GXcastf32u8(color[2]);
	OGL.GXclearColor.a = GXcastf32u8(color[3]);

	OGL.GXclearColorBuffer = true;
	gDP.changed |= CHANGED_RENDERMODE;
	/* [Paper Mario] Un borrado del color tapa la carga aplazada entera. */
	FrameBuffer_GX_CancelaCargaPendiente();
#endif // __GX__
}

void OGL_SaveScreenshot()
{
#ifndef __LINUX__
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;
	HANDLE hBitmapFile;

	char *pixelData = (char*)malloc( OGL.width * OGL.height * 3 );

	glReadBuffer( GL_FRONT );
	glReadPixels( 0, OGL.heightOffset, OGL.width, OGL.height, GL_BGR_EXT, GL_UNSIGNED_BYTE, pixelData );
	glReadBuffer( GL_BACK );

	infoHeader.biSize = sizeof( BITMAPINFOHEADER );
	infoHeader.biWidth = OGL.width;
	infoHeader.biHeight = OGL.height;
	infoHeader.biPlanes = 1;
	infoHeader.biBitCount = 24;
	infoHeader.biCompression = BI_RGB;
	infoHeader.biSizeImage = OGL.width * OGL.height * 3;
	infoHeader.biXPelsPerMeter = 0;
	infoHeader.biYPelsPerMeter = 0;
	infoHeader.biClrUsed = 0;
	infoHeader.biClrImportant = 0;

	fileHeader.bfType = 19778;
	fileHeader.bfSize = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER ) + infoHeader.biSizeImage;
	fileHeader.bfReserved1 = fileHeader.bfReserved2 = 0;
	fileHeader.bfOffBits = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER );

	char filename[256];

	CreateDirectory( screenDirectory, NULL );

	int i = 0;
	do
	{
		sprintf( filename, "%sscreen%02i.bmp", screenDirectory, i );
		i++;

		if (i > 99)
			return;

		hBitmapFile = CreateFile( filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL );
	}
	while (hBitmapFile == INVALID_HANDLE_VALUE);
	
	DWORD written;

	WriteFile( hBitmapFile, &fileHeader, sizeof( BITMAPFILEHEADER ), &written, NULL );
    WriteFile( hBitmapFile, &infoHeader, sizeof( BITMAPINFOHEADER ), &written, NULL );
    WriteFile( hBitmapFile, pixelData, infoHeader.biSizeImage, &written, NULL );

 	CloseHandle( hBitmapFile );
	free( pixelData );
#else // !__LINUX__
#endif // __LINUX__
}

#ifdef __LINUX__
void
OGL_SwapBuffers()
{
#ifndef __GX__
	static int frames[5] = { 0, 0, 0, 0, 0 };
	static int framesIndex = 0;
	static Uint32 lastTicks = 0;
	Uint32 ticks = SDL_GetTicks();

	frames[framesIndex]++;
	if (ticks >= (lastTicks + 1000))
	{
		char caption[500];
		float fps = 0.0;
		for (int i = 0; i < 5; i++)
			fps += frames[i];
		fps /= 5.0;
		snprintf( caption, 500, "%s - %.2f fps", pluginName, fps );
		SDL_WM_SetCaption( caption, pluginName );
		framesIndex = (framesIndex + 1) % 5;
		frames[framesIndex] = 0;
		lastTicks = ticks;
	}

	SDL_GL_SwapBuffers();
#endif // !__GX__
}

void OGL_ReadScreen( void **dest, long *width, long *height )
{
#ifndef __GX__
	*width = OGL.width;
	*height = OGL.height;

	*dest = malloc( OGL.height * OGL.width * 3 );
	if (*dest == 0)
		return;

	GLint oldMode;
	glGetIntegerv( GL_READ_BUFFER, &oldMode );
	glReadBuffer( GL_FRONT );
//	glReadBuffer( GL_BACK );
	glReadPixels( 0, 0, OGL.width, OGL.height,
	              GL_BGR, GL_UNSIGNED_BYTE, *dest );
	glReadBuffer( oldMode );
#endif // !__GX__ Note: This is a VCR function.
}

#endif // __LINUX__

#ifdef __GX__
void OGL_GXinitDlist()
{
//	if(VI.copy_fb)
//		VIDEO_WaitVSync();

	OGL.frameBufferTextures = glN64_useFrameBufferTextures;
	OGL.enable2xSaI = glN64_use2xSaiTextures;
	OGL.forceBilinear = glN64_use2xSaiTextures;

	// init Ztexture, AlphaCompare, and Texture Clamping
	GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z8,0);
	OGL.GXuseAlphaCompare = false;
	GX_SetZCompLoc(GX_TRUE);	// Do Z-compare before texturing.
	OGL.GXrenderTexRect = false;
	OGL.GXforceClampS0 = false;
	OGL.GXforceClampT0 = false;
	OGL.GXforceClampS1 = false;
	OGL.GXforceClampT1 = false;
	OGL.GXuseMinMagNearest = false;

	// init fog
	OGL.GXfogStartZ = -1.0f;
	OGL.GXfogEndZ = 1.0f;
	OGL.GXfogColor = GXCOL_BLACK;   /* [Xbox360] literal compuesto */
	OGL.GXfogType = GX_FOG_NONE;
	GX_SetFog(OGL.GXfogType,OGL.GXfogStartZ,OGL.GXfogEndZ,-1.0,1.0,OGL.GXfogColor);
	OGL.GXupdateFog = false;

	//Reset Modelview matrix
	guMtxIdentity(OGL.GXmodelViewIdent);
	GX_LoadPosMtxImm(OGL.GXmodelViewIdent,GX_PNMTX0);
	GX_LoadTexMtxImm(OGL.GXmodelViewIdent,GX_TEXMTX0,GX_MTX2x4);

	//Reset projection matrix
	guMtx44Identity(OGL.GXcombW);
	guMtx44Identity(OGL.GXprojIdent);
	//N64 Z clip space is backwards, so mult z components by -1
	//N64 Z [-1,1] whereas GC Z [-1,0], so mult by 0.5 and shift by -0.5
	OGL.GXcombW[2][2] = 0.0f;
	OGL.GXcombW[2][3] = -0.5f;
	OGL.GXcombW[3][2] = -1.0f;
	OGL.GXcombW[3][3] = 0.0f;
	OGL.GXprojIdent[2][2] = GXprojZScale; //0.5;
	OGL.GXprojIdent[2][3] = GXprojZOffset; //-0.5;
	GX_LoadProjectionMtx(OGL.GXprojIdent, GX_ORTHOGRAPHIC);
	OGL.GXpolyOffset = false;

	OGL.GXnumVtxMP = 0;
	cache.GXprimDepthCnt = cache.GXZTexPrimCnt = cache.GXnoZTexPrimCnt = 0;

	//Not sure if this is needed.  Clipping is a slow process...
	//Note: gx.h has GX_CLIP_ENABLE and GX_CLIP_DISABLE backwards!!
	GX_SetClipMode(GX_CLIP_ENABLE);
//	GX_SetClipMode(GX_CLIP_DISABLE);

	//These are here temporarily until combining/blending is sorted out...
	//Set PassColor TEV mode
	GX_SetNumChans (1);
	GX_SetNumTexGens (0);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	//Set CopyModeDraw blend modes here
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR); //Fix src alpha
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	GX_SetDstAlpha(GX_DISABLE, 0xFF);
//	OGL.GXclearColorBuffer = false;
//	OGL.GXclearDepthBuffer = false;
	OGL.GXclearColor = GXCOL_BLACK;   /* [Xbox360] literal compuesto */

	/* [Xbox360] ★★ [Quake II: modelos que parpadean y se vuelven negros] LO
	 * QUE ESTA FUNCION DEJA CAMBIADO EN GX, MARCADO PARA QUE SE REPONGA: el
	 * TEV de la etapa 0 en PASSCLR, NumTexGens 0, la mezcla y el alpha test.
	 * Antes lo tapaba el reinicio de otherMode que venia detras en
	 * RSP_ProcessDList: sus gDPSet* marcaban exactamente estas cuatro.  Pero
	 * con hack_doNotResetOtherModeH/L (los Quake) no corria, y el primer
	 * dibujo de cada lista sin cambio de mux se quedaba con la etapa 0 en
	 * PASSCLR: color de vertice en vez de PRIM.  MEDIDO (sonda [Q2TEV]):
	 * 38910 de 94208 lotes del mux de los modelos con la etapa 0 en
	 * 0F0F0F0A (RASC) en vez de 0F0F0F04 (C1).  Y al pasar el reinicio al
	 * swap (RSP_ReiniciaOtherMode) ya no corria para NINGUN juego al empezar
	 * cada lista; marcando aqui las mismas banderas todos quedan como antes. */
	gDP.changed |= CHANGED_RENDERMODE | CHANGED_ALPHACOMPARE | CHANGED_CYCLETYPE;
	gSP.changed |= CHANGED_TEXTURE;
}

void OGL_ApplyPendingClears()
{
	if (OGL.GXclearColorBuffer || OGL.GXclearDepthBuffer)
		OGL_GXclearEFB();
}

void OGL_GXclearEFB()
{
	/* [Xbox360] [Pilotwings] El quad de borrado no puede salir con el dibujo
	 * sobre el depth image puesto (escribiria su color como profundidad): se
	 * llama al principio de OGL_UpdateStates, antes de que el final de esa
	 * funcion decida el modo del siguiente dibujo. */
	GXD3D_SetRenderToDepth( 0 );
	GXD3D_SetDecalN64( 0 );   /* [ISS64] idem con el decal por shader */
#if GLN64_CLEAR_D3D
	/* [Xbox360] EL BORRADO DE PANTALLA, CON EL Clear() NATIVO DE D3D.
	 *
	 * Va ANTES de todo lo demas y el resto de la funcion se deja correr tal
	 * cual: el quad de abajo pinta lo mismo, asi que no estorba, y dejarlo
	 * mantiene intactas la contabilidad de banderas de upstream y el A/B del
	 * toggle.  El porque -y lo que costo llegar aqui- esta en gxd3d_clear_efb
	 * (xenon/gx_d3d9.cpp).
	 *
	 * ★ NOTA DE LA PUESTA AL DIA A Wii64 1b5041a: upstream ha arreglado la
	 * CAUSA RAIZ en las dos lineas de aqui debajo -- antes las dos ramas del
	 * `if` ponian `GX_SetZMode(..., GX_GEQUAL, ...)` y el quad de borrado lo
	 * rechazaba la propia prueba de profundidad en los juegos que usan GEQUAL
	 * como funcion normal (GoldenEye); ahora ponen `GX_ALWAYS`.  O sea que el
	 * camino del quad deberia bastar por si solo.  Se deja este bloque porque
	 * es la solucion COMPROBADA en hardware, y el toggle existe justo para
	 * medir si ya sobra: con GLN64_CLEAR_D3D a 0 el binario usa solo el quad
	 * de upstream. */
	gxd3d_clear_efb(OGL.GXclearColorBuffer ? 1 : 0,
	                OGL.GXclearDepthBuffer ? 1 : 0,
	                OGL.GXclearColor.r, OGL.GXclearColor.g, OGL.GXclearColor.b);
#endif

	//Note: EFB is RGB8, so no need to clear alpha
	if(OGL.GXclearColorBuffer)	GX_SetColorUpdate(GX_ENABLE);
	else						GX_SetColorUpdate(GX_DISABLE);

	if(OGL.GXclearDepthBuffer)	GX_SetZMode(GX_ENABLE,GX_ALWAYS,GX_TRUE);
	else						GX_SetZMode(GX_ENABLE,GX_ALWAYS,GX_FALSE);

	GX_SetNumChans(1);
	GX_SetNumTexGens(0);
	GX_SetNumTevStages(1);
	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXMAP_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR); 
	GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z8,0);
	GX_SetZCompLoc(GX_TRUE);	// Do Z-compare before texturing.
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	GX_SetFog(GX_FOG_NONE,0.1,1.0,0.0,1.0,GXCOL_BLACK);   /* [Xbox360] literal compuesto */
	GX_SetViewport((f32) OGL.GXorigX,(f32) OGL.GXorigY,(f32) OGL.GXwidth,(f32) OGL.GXheight, 0.0f, 1.0f);
	GX_SetScissor((u32) 0,(u32) 0,(u32) OGL.width,(u32) OGL.height);	//Disable Scissor
//	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetCullMode (GX_CULL_NONE);
	Mtx44 GXprojection;
	guOrtho(GXprojection, 0, OGL.height, 0, OGL.width, 0.0f, 1.0f);
	GX_LoadProjectionMtx(GXprojection, GX_ORTHOGRAPHIC); 
	GX_LoadPosMtxImm(OGL.GXmodelViewIdent,GX_PNMTX0);

	//set vertex description here
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	f32 ZmaxDepth = (f32) -0xFFFFFF/0x1000000;
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(0.0f, 0.0f, ZmaxDepth);
		GX_Color4u8(OGL.GXclearColor.r, OGL.GXclearColor.g, OGL.GXclearColor.b, OGL.GXclearColor.a); 
		GX_Position3f32((f32) OGL.width, 0.0f, ZmaxDepth);
		GX_Color4u8(OGL.GXclearColor.r, OGL.GXclearColor.g, OGL.GXclearColor.b, OGL.GXclearColor.a); 
		GX_Position3f32((f32) OGL.width,(f32) OGL.height, ZmaxDepth);
		GX_Color4u8(OGL.GXclearColor.r, OGL.GXclearColor.g, OGL.GXclearColor.b, OGL.GXclearColor.a); 
		GX_Position3f32(0.0f,(f32) OGL.height, ZmaxDepth);
		GX_Color4u8(OGL.GXclearColor.r, OGL.GXclearColor.g, OGL.GXclearColor.b, OGL.GXclearColor.a); 
	GX_End();

	OGL.GXclearColorBuffer = false;
	OGL.GXclearDepthBuffer = false;
	OGL.GXupdateMtx = true;
	OGL.GXupdateFog = true;
	GX_SetColorUpdate(GX_ENABLE);
	OGL_UpdateCullFace();
	OGL_UpdateViewport();
	gDP.changed |= CHANGED_SCISSOR | CHANGED_COMBINE | CHANGED_RENDERMODE;	//Restore scissor in OGL_UpdateStates() before drawing next geometry.

/*	OGL.GXclearBufferTex = (u8*) memalign(32,640*480/2);
	GX_SetCopyClear (OGL.GXclearColor, 0xFFFFFF);
	if(OGL.GXclearColorBuffer)	GX_SetColorUpdate(GX_ENABLE);
	else						GX_SetColorUpdate(GX_DISABLE);
	if(OGL.GXclearDepthBuffer)	GX_SetZMode(GX_ENABLE,GX_ALWAYS,GX_TRUE);
	else						GX_SetZMode(GX_ENABLE,GX_ALWAYS,GX_FALSE);
	GX_SetTexCopySrc(0, 0, 640, 480);
	GX_SetTexCopyDst(640, 480, GX_TF_I4, GX_FALSE);
	GX_CopyTex(OGL.GXclearBufferTex, GX_TRUE);
	GX_PixModeSync();
//	GX_DrawDone();
	GX_SetColorUpdate(GX_ENABLE);
	free(OGL.GXclearBufferTex);
	gDP.changed |= CHANGED_RENDERMODE;*/
}
#endif // __GX__
