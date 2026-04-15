/*
 * video_stub.cpp - Minimal video stubs for libretro adaptation
 *
 * Replaces gui_video.cpp and video.cpp. Provides the extern "C" functions
 * that the GPU plugin (video.lib) expects, but without any D3D/XUI code.
 * A libretro frontend will replace these with proper libretro callbacks.
 */

#include <xtl.h>
#include <stdio.h>
#include <string.h>

#define PSX_WIDTH  1024
#define PSX_HEIGHT 512

/* Framebuffer that the GPU plugin writes into.
 * Sized for the largest format (XRGB8888 = 4 bytes/pixel). */
static __declspec(align(128)) unsigned char psxScreenBuffer[PSX_WIDTH * PSX_HEIGHT * 4];

extern "C" unsigned char *pPsxScreen = psxScreenBuffer;
extern "C" unsigned int   g_pPitch   = PSX_WIDTH * 4;

/* Pixel format flag: 0 = XRGB8888 (32bpp), 1 = RGB565 (16bpp).
 * Read by the GPU plugin (draw_ok.c) in DoBufferSwap(). */
extern "C" int g_useRGB565 = 0;

/* Current display resolution reported by the GPU plugin */
static int currentWidth  = PSX_WIDTH;
static int currentHeight = PSX_HEIGHT;

/*
 * VideoInit - Called by the GPU plugin to initialize the video output.
 * Returns 0 (S_OK) on success.
 */
extern "C" unsigned int VideoInit()
{
    memset(psxScreenBuffer, 0, sizeof(psxScreenBuffer));
    pPsxScreen = psxScreenBuffer;
    g_pPitch   = g_useRGB565 ? (PSX_WIDTH * 2) : (PSX_WIDTH * 4);
    return 0; /* S_OK */
}

/*
 * UpdateScrenRes - Called by the GPU plugin when the PSX display
 * resolution changes. A libretro core would update its geometry here.
 */
extern "C" void libretro_update_display_size(int w, int h);

extern "C" void UpdateScrenRes(int x, int y)
{
    currentWidth  = x;
    currentHeight = y;
	/* Update pitch to match the actual display width so the frontend 
	 * can memcpy the framebuffer in one shot instead of row-by-row. */
	g_pPitch   = g_useRGB565 ? (x * 2) : (x * 4);

#ifdef LIBRETRO
    libretro_update_display_size(x, y);
#endif
}

/*
 * DisplayUpdate - Called by the GPU plugin to present a completed frame.
 * A libretro core would call retro_video_refresh_t here.
 */
extern "C" void DisplayUpdate()
{
    /* stub - libretro frontend will handle frame presentation */
}

/*
 * DrawPcsxSurface - Was used by the XUI GUI to blit the PSX framebuffer.
 * Stub for compatibility.
 */
void DrawPcsxSurface()
{
    /* stub */
}

/*
 * VideoPresent - Was the D3D swap chain present call.
 * Stub for compatibility.
 */
void VideoPresent()
{
    /* stub */
}

/*
 * InitD3D - Was the D3D9 device initialization.
 * Stub - no longer needed without GUI.
 */
void InitD3D()
{
    /* stub - video initialization now happens in VideoInit() */
}

/*
 * PcsxSetD3D - Was used to pass the D3D device to the GPU plugin.
 * No longer needed.
 */
void PcsxSetD3D(void* device)
{
    (void)device;
}

/*
 * LoadShaderFromFile - Was used to load D3D shaders.
 * Stub for compatibility.
 */
void LoadShaderFromFile(const char* filename)
{
    (void)filename;
}

/* Accessors for libretro integration */
extern "C" unsigned char* GetPsxFramebuffer()
{
    return pPsxScreen;
}

extern "C" unsigned int GetPsxPitch()
{
    return g_pPitch;
}

extern "C" void GetPsxResolution(int* w, int* h)
{
    *w = currentWidth;
    *h = currentHeight;
}
