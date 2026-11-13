/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Direct3D 9 port of renderers/gl.c. The structure follows gl.c function by
 * function so both can be compared side by side; the differences are:
 *
 * - Scanline 0 is at the top of every render target (GL draws upside down).
 * - Per-scanline arrays that gl.c passes as uniform arrays (BG offsets, affine
 *   parameters, window edges) and the backdrop "framebuffer" live in one
 *   float texture (lineData), because ps_3_0 can't index constants.
 * - gl.c discards sprite pixels on scanlines without sprite cycles left with
 *   a uniform array; here the sprite is drawn once per run of scanlines with
 *   cycles left, using the scissor rectangle. Same result.
 * - The OBJ depth (priority / 16) comes from the vertex shader.
 * - Dynamic textures are re-uploaded whole with D3DLOCK_DISCARD, so the CPU
 *   never waits for the GPU.
 * - Integer render targets are A8R8G8B8 holding n / 255.
 * - Text BGs are drawn at 1x (bgLowres): they look exactly the same as at
 *   the internal resolution, which only makes a difference for affine and
 *   bitmap BGs, sprites and windows.
 *
 * Xbox 360 (D3D9_EDRAM): the GPU can only draw into EDRAM surfaces, and the
 * only way out of the EDRAM is Resolve() into a texture, which stalls the
 * GPU. So all the layers stay in the EDRAM while the frame is drawn (seven
 * colour surfaces and a depth one) and are resolved to their textures once,
 * before the composition. When they don't fit (3x and up) the frame is
 * drawn in horizontal bands of `bandRows` scanlines: the surfaces are one
 * band high and are resolved each time the scanlines move to the next band.
 * The scanlines come in order, so each band is entered once per frame.
 * (_edramEnterBand / _edramLeaveBand; targetTop is the first scanline of the
 * band, which the vertex shader and the scissor subtract.) Each band has its
 * own layer textures, so that Resolve always copies a rectangle at (0, 0) to
 * (0, 0); the composition then reads them band by band into one surface
 * as big as the output, resolved whole. Nothing of ours
 * has to survive in the EDRAM between two retro_run, so the surfaces go
 * behind the frontend's back buffer or, if they don't fit there, from tile
 * 0. CPU textures are linear, in rings guarded with fences (LockRect doesn't
 * wait for the GPU there), and only the rows a batch reads are copied into
 * them (all of them before the composition). The frontend's device state is
 * saved and restored by hand (no state blocks).
 *
 * Written in C89 with the C interface of d3d9.h, like the rest of the
 * project, so it builds with VS2010 and the Xbox 360 XDK. */
#include <mgba/internal/gba/renderers/d3d9.h>

#ifdef BUILD_D3D9

#include <mgba/core/core.h>
#include <mgba/core/cache-set.h>
#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/renderers/cache-set.h>
#include <mgba/internal/gba/renderers/common.h>
#include <mgba/internal/gba/video.h>
#include <mgba-util/memory.h>

#include <limits.h>
#include <math.h>
#include <stdarg.h>

#include <stdio.h>

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#else
#include <d3d9.h>
#endif
#include <d3dx9shader.h>

#ifdef _XBOX
#define D3D9_EDRAM 1
#endif

/* Xbox 360: linear textures, which the CPU can write row by row. (Tiled
 * ones filled with XGTileSurface cost 16 ms of CPU per frame in F-Zero, and
 * the GPU is idle half of the frame at 3x anyway) */
#ifdef _XBOX
#define D3D9_FMT_VRAM D3DFMT_LIN_A8L8
#define D3D9_FMT_PALETTE D3DFMT_LIN_R5G6B5
#define D3D9_FMT_LINEDATA D3DFMT_LIN_A32B32G32R32F
#else
#define D3D9_FMT_VRAM D3DFMT_A8L8
#define D3D9_FMT_PALETTE D3DFMT_R5G6B5
#define D3D9_FMT_LINEDATA D3DFMT_A32B32G32R32F
#endif

#ifdef D3D9_EDRAM
/* CPU-written textures: a ring, so that a LockRect doesn't overwrite texels
 * the GPU hasn't read yet (fences say when a texture is free again). Games
 * with raster effects upload palette and per-line data once per batch of
 * scanlines (up to 160 per frame), so those rings are long enough for the
 * CPU not to wait for the GPU in a normal frame */
#define D3D9_RING 64
#define D3D9_RING_VRAM 8
#define D3D9_RING_PALETTE 32
#define D3D9_RING_LINEDATA 64
#define D3D9_CPU_USAGE 0
#define D3D9_CPU_LOCK 0
#define D3D9_EDRAM_TILES 2048
#else
#define D3D9_RING 1
#define D3D9_RING_VRAM 1
#define D3D9_RING_PALETTE 1
#define D3D9_RING_LINEDATA 1
#define D3D9_CPU_USAGE D3DUSAGE_DYNAMIC
#define D3D9_CPU_LOCK D3DLOCK_DISCARD
#endif

#if defined(_WIN32) || defined(_XBOX)
#define D3D9_PATH_SEP "\\"
#else
#define D3D9_PATH_SEP "/"
#endif

#define D3D9_MAGIC 0x39643364

/* lineData columns (see d3d9-shaders.h) */
#define GBA_D3D9_LD_OFFSET 0
#define GBA_D3D9_LD_AFFINE 4
#define GBA_D3D9_LD_WIN0 6
#define GBA_D3D9_LD_WIN1 7
#define GBA_D3D9_LD_BACKDROP 8
#define GBA_D3D9_LD_BGFLAGS 9
#define GBA_D3D9_LD_WIDTH 16

#include "d3d9-shaders.h"

#define D3D9_MAX_SCALE 8

/* Pixel shader constant registers (see d3d9-shaders.h) */
#define PS_BG 4
#define PS_OBJ 8
#define PS_WIN 15
#define PS_FIN 20
#define PS_LAST PS_FIN
#define S_MAX 10

enum {
	D3D9_PS_MODE0_16 = 0,
	D3D9_PS_MODE0_256,
	D3D9_PS_MODE2,
	D3D9_PS_MODE4,
	D3D9_PS_MODE35,
	D3D9_PS_OBJ_16,
	D3D9_PS_OBJ_256,
	D3D9_PS_OBJWIN_16,
	D3D9_PS_OBJWIN_256,
	D3D9_PS_OBJ_PRIORITY,
	D3D9_PS_WINDOW,
	D3D9_PS_FINALIZE,
	D3D9_PS_MAX
};

enum {
	D3D9_RT_OBJ_COLOR = 0,
	D3D9_RT_OBJ_FLAGS,
	D3D9_RT_WINDOW,
	D3D9_RT_OUTPUT,
	D3D9_RT_BG0,
	D3D9_RT_MAX = D3D9_RT_BG0 + 4
};

/* A CPU-written texture (or a ring of them, see D3D9_RING) */
struct GBAVideoD3D9Upload {
	IDirect3DTexture9* tex[D3D9_RING];
	DWORD fence[D3D9_RING];
	bool fenced[D3D9_RING];
	int current;
	int count;
	DWORD stage;
};

struct GBAVideoD3D9Affine {
	int16_t dx;
	int16_t dmx;
	int16_t dy;
	int16_t dmy;
	int32_t sx;
	int32_t sy;
};

struct GBAVideoD3D9Background {
	unsigned index;
	int enabledAtY;
	unsigned priority;
	uint32_t charBase;
	uint32_t oldCharBase;
	int mosaic;
	int multipalette;
	uint32_t screenBase;
	int overflow;
	int size;
	int target1;
	int target2;
	uint16_t x;
	uint16_t y;
	int32_t refx;
	int32_t refy;
	int32_t offsetX;
	int32_t offsetY;

	struct GBAVideoD3D9Affine affine;
};

struct GBAVideoD3D9WindowN {
	struct GBAVideoWindowRegion h;
	struct GBAVideoWindowRegion v;
	GBAWindowControl control;
	int16_t offsetX;
	int16_t offsetY;
};

struct GBAVideoD3D9Renderer {
	struct GBAVideoRenderer d;

	IDirect3DDevice9* dev;
	IDirect3DVertexShader9* vs;
	IDirect3DVertexDeclaration9* decl;
	IDirect3DPixelShader9* ps[D3D9_PS_MAX];
	/* PC: render-target textures (rtSurf is their level 0).
	 * EDRAM: plain textures that Resolve writes; rtSurf is unused */
	IDirect3DTexture9* rtTex[D3D9_RT_MAX];
	IDirect3DSurface9* rtSurf[D3D9_RT_MAX];
	IDirect3DSurface9* depth;
	/* BGs drawn at 1x (bgLowres): a text BG looks exactly the same as at
	 * the internal resolution, and costs scale * scale times less. PC: the
	 * render targets; EDRAM: unused (see edramLow and bandTexLow) */
	IDirect3DTexture9* rtTexLow[4];
	IDirect3DSurface9* rtSurfLow[4];
	/* decided before each frame: BGs that weren't drawn with an affine or
	 * bitmap shader in the previous one (bgHires) */
	bool bgLowres[4];
	bool bgHires[4];
	struct GBAVideoD3D9Upload vramTex;
	struct GBAVideoD3D9Upload paletteTex;
	struct GBAVideoD3D9Upload lineTex;
	char* shaderCache;

	/* device state between the first draw of a frame and finishFrame */
	bool begun;
	IDirect3DSurface9* savedRT;
	IDirect3DSurface9* savedDS;
	IDirect3DSurface9* currentRT[2];
	IDirect3DSurface9* currentDS;
	IDirect3DPixelShader9* currentPS;
	/* scale of the bound render target: renderer->scale, or 1 for a BG
	 * drawn at 1x */
	int targetScale;
	/* scanlines covered by the bound render target: all of them on PC, one
	 * band on Xbox 360 */
	int targetTop;
	int targetRows;
#ifdef D3D9_EDRAM
	/* one per layer except the output (see _targetSurface), bandRows
	 * scanlines high */
	IDirect3DSurface9* edram[D3D9_RT_MAX - 1];
	/* the whole output, drawn after every layer has been resolved */
	IDirect3DSurface9* edramOut;
	/* what Resolve writes: one texture per layer and band (rtTex is only
	 * used for the output), so that Resolve always copies to the same
	 * coordinates it reads from */
	IDirect3DTexture9* bandTex[GBA_VIDEO_VERTICAL_PIXELS / 8][D3D9_RT_MAX];
	/* the same for BGs drawn at 1x */
	IDirect3DSurface9* edramLow[4];
	IDirect3DTexture9* bandTexLow[GBA_VIDEO_VERTICAL_PIXELS / 8][4];
	unsigned edramBase;
	unsigned edramOutBase;
	int bandRows;
	int currentBand;
	/* the frontend's state (no state blocks on Xbox 360) */
	DWORD savedRS[32];
	DWORD savedSamp[S_MAX][5];
	IDirect3DBaseTexture9* savedTex[S_MAX];
	IDirect3DVertexShader9* savedVS;
	IDirect3DPixelShader9* savedPS;
	IDirect3DVertexDeclaration9* savedDecl;
	D3DVIEWPORT9 savedViewport;
	RECT savedScissor;
	float savedVSConst[4][4];
	float savedPSConst[PS_LAST + 1 - PS_BG][4];
#else
	IDirect3DStateBlock9* stateBlock;
#endif

	mColor* temporaryBuffer;

#ifdef _XBOX
	/* the frontend's GPR allocation (see _begin): flags, VS, PS */
	DWORD savedGPR[3];
#endif

	struct GBAVideoD3D9Background bg[4];

	int oamMax;
	bool oamDirty;
	struct GBAVideoRendererSprite sprites[128];
	int16_t objOffsetX;
	int16_t objOffsetY;

	uint16_t shadowPalette[GBA_VIDEO_VERTICAL_PIXELS][512];
	int nextPalette;
	int paletteDirtyScanlines;
	bool paletteDirty;

	unsigned vramDirty;

	uint16_t shadowRegs[0x30];
	uint64_t regsDirty;

	float lineData[GBA_VIDEO_VERTICAL_PIXELS][GBA_D3D9_LD_WIDTH][4];

	GBARegisterDISPCNT dispcnt;

	unsigned target1Obj;
	unsigned target1Bd;
	unsigned target2Obj;
	unsigned target2Bd;
	enum GBAVideoBlendEffect blendEffect;
	uint16_t blda;
	uint16_t bldb;
	uint16_t bldy;

	GBAMosaicControl mosaic;

	struct GBAVideoD3D9WindowN winN[2];
	int winNHistory[2][GBA_VIDEO_VERTICAL_PIXELS * 4];
	int spriteCycles[GBA_VIDEO_VERTICAL_PIXELS];
	float circle[2][4];

	GBAWindowControl winout;
	GBAWindowControl objwin;

	int firstAffine;
	int firstY;
	int lastY;
	/* the batch _drawScanlines was called with */
	int batchFirstY;
	int batchLastY;

	int scale;
};

/* Why the last GBAVideoD3D9RendererCreate/SetScale failed: every failure,
 * one per line */
static char _lastError[4096];

static void _fail(const char* format, ...) {
	char line[1024];
	size_t used = strlen(_lastError);
	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);
	mLOG(GBA_VIDEO, ERROR, "D3D9: %s", line);
	if (used + 2 < sizeof(_lastError)) {
		snprintf(&_lastError[used], sizeof(_lastError) - used, "%s%s", used ? "\n" : "", line);
	}
}

const char* GBAVideoD3D9RendererGetLastError(void) {
	return _lastError;
}

static const int _mosaicTable[17] = { 0, 4096, 2048, 1366, 1024, 820, 683, 586, 512, 456, 410, 373, 342, 316, 293, 274, 256 };

static void GBAVideoD3D9RendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoD3D9RendererDeinit(struct GBAVideoRenderer* renderer);
static void GBAVideoD3D9RendererReset(struct GBAVideoRenderer* renderer);
static uint32_t GBAVideoD3D9RendererId(const struct GBAVideoRenderer* renderer);
static bool GBAVideoD3D9RendererLoadState(struct GBAVideoRenderer* renderer, const void* state, size_t size);
static void GBAVideoD3D9RendererSaveState(struct GBAVideoRenderer* renderer, void** state, size_t* size);
static void GBAVideoD3D9RendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address);
static void GBAVideoD3D9RendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoD3D9RendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static uint16_t GBAVideoD3D9RendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoD3D9RendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoD3D9RendererFinishFrame(struct GBAVideoRenderer* renderer);
static void GBAVideoD3D9RendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBAVideoD3D9RendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels);

static void GBAVideoD3D9RendererUpdateDISPCNT(struct GBAVideoD3D9Renderer* renderer);
static void GBAVideoD3D9RendererWriteBGCNT(struct GBAVideoD3D9Background* bg, uint16_t value);
static void GBAVideoD3D9RendererWriteBGX_LO(struct GBAVideoD3D9Background* bg, uint16_t value);
static void GBAVideoD3D9RendererWriteBGX_HI(struct GBAVideoD3D9Background* bg, uint16_t value);
static void GBAVideoD3D9RendererWriteBGY_LO(struct GBAVideoD3D9Background* bg, uint16_t value);
static void GBAVideoD3D9RendererWriteBGY_HI(struct GBAVideoD3D9Background* bg, uint16_t value);
static void GBAVideoD3D9RendererWriteBLDCNT(struct GBAVideoD3D9Renderer* renderer, uint16_t value);

static void GBAVideoD3D9RendererDrawSprite(struct GBAVideoD3D9Renderer* renderer, struct GBAObj* sprite, int y, int spriteY);
static void GBAVideoD3D9RendererDrawBackgroundMode0(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y);
static void GBAVideoD3D9RendererDrawBackgroundMode2(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y);
static void GBAVideoD3D9RendererDrawBackgroundBitmap(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y, int shader, int charBase, int width, int height);
static void GBAVideoD3D9RendererDrawWindow(struct GBAVideoD3D9Renderer* renderer, int y);

static void _cleanRegister(struct GBAVideoD3D9Renderer* renderer, int address, uint16_t value);
static void _drawScanlines(struct GBAVideoD3D9Renderer* renderer, int y);
static void _finalizeLayers(struct GBAVideoD3D9Renderer* renderer);
#ifdef D3D9_EDRAM
static void _setBand(struct GBAVideoD3D9Renderer* renderer, int band);
#endif

/* ------------------------------------------------------------------------
 * Device helpers
 * ------------------------------------------------------------------------ */

/* With the interface's own Release macro: Xbox 360 D3D objects aren't COM
 * (no lpVtbl), but the IDirect3D*9_Release macros exist on both */
#define SAFE_RELEASE(TYPE, X) do { if (X) { TYPE##_Release(X); (X) = NULL; } } while (0)

static int _rtWidth(const struct GBAVideoD3D9Renderer* renderer) {
	return GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale;
}

static int _rtHeight(const struct GBAVideoD3D9Renderer* renderer) {
	return GBA_VIDEO_VERTICAL_PIXELS * renderer->scale;
}

/* Scale of a layer this frame */
static int _layerScale(const struct GBAVideoD3D9Renderer* renderer, int rt) {
	if (rt >= D3D9_RT_BG0 && renderer->bgLowres[rt - D3D9_RT_BG0]) {
		return 1;
	}
	return renderer->scale;
}

static void _setSampler(IDirect3DDevice9* dev, DWORD stage) {
	IDirect3DDevice9_SetSamplerState(dev, stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_SetSamplerState(dev, stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	IDirect3DDevice9_SetSamplerState(dev, stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	IDirect3DDevice9_SetSamplerState(dev, stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	IDirect3DDevice9_SetSamplerState(dev, stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
#ifndef _XBOX
	IDirect3DDevice9_SetSamplerState(dev, stage, D3DSAMP_MAXMIPLEVEL, 0);
	IDirect3DDevice9_SetSamplerState(dev, stage, D3DSAMP_SRGBTEXTURE, FALSE);
#endif
}

#ifdef D3D9_EDRAM
/* Render states this renderer changes, saved and restored by hand */
static const int _savedStates[] = {
	D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ZFUNC, D3DRS_STENCILENABLE, D3DRS_STENCILFAIL,
	D3DRS_STENCILZFAIL, D3DRS_STENCILPASS, D3DRS_STENCILFUNC, D3DRS_STENCILREF, D3DRS_STENCILMASK,
	D3DRS_STENCILWRITEMASK, D3DRS_CULLMODE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE,
	D3DRS_SCISSORTESTENABLE, D3DRS_COLORWRITEENABLE,
#ifdef _XBOX
	D3DRS_VIEWPORTENABLE, D3DRS_HIZENABLE, D3DRS_HIZWRITEENABLE,
#endif
	-1
};
static const int _savedSamplerStates[5] = { D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV };

static void _saveFrontendState(struct GBAVideoD3D9Renderer* renderer) {
	IDirect3DDevice9* dev = renderer->dev;
	int i, j;
	for (i = 0; _savedStates[i] >= 0; ++i) {
		IDirect3DDevice9_GetRenderState(dev, (D3DRENDERSTATETYPE) _savedStates[i], &renderer->savedRS[i]);
	}
	for (i = 0; i < S_MAX; ++i) {
		for (j = 0; j < 5; ++j) {
			IDirect3DDevice9_GetSamplerState(dev, i, (D3DSAMPLERSTATETYPE) _savedSamplerStates[j], &renderer->savedSamp[i][j]);
		}
		renderer->savedTex[i] = NULL;
		IDirect3DDevice9_GetTexture(dev, i, &renderer->savedTex[i]);
	}
	renderer->savedVS = NULL;
	renderer->savedPS = NULL;
	renderer->savedDecl = NULL;
	IDirect3DDevice9_GetVertexShader(dev, &renderer->savedVS);
	IDirect3DDevice9_GetPixelShader(dev, &renderer->savedPS);
	IDirect3DDevice9_GetVertexDeclaration(dev, &renderer->savedDecl);
	IDirect3DDevice9_GetViewport(dev, &renderer->savedViewport);
	IDirect3DDevice9_GetScissorRect(dev, &renderer->savedScissor);
	IDirect3DDevice9_GetVertexShaderConstantF(dev, 0, renderer->savedVSConst[0], 4);
	IDirect3DDevice9_GetPixelShaderConstantF(dev, PS_BG, renderer->savedPSConst[0], PS_LAST + 1 - PS_BG);
}

static void _restoreFrontendState(struct GBAVideoD3D9Renderer* renderer) {
	IDirect3DDevice9* dev = renderer->dev;
	int i, j;
	for (i = 0; _savedStates[i] >= 0; ++i) {
		IDirect3DDevice9_SetRenderState(dev, (D3DRENDERSTATETYPE) _savedStates[i], renderer->savedRS[i]);
	}
	for (i = 0; i < S_MAX; ++i) {
		for (j = 0; j < 5; ++j) {
			IDirect3DDevice9_SetSamplerState(dev, i, (D3DSAMPLERSTATETYPE) _savedSamplerStates[j], renderer->savedSamp[i][j]);
		}
		IDirect3DDevice9_SetTexture(dev, i, renderer->savedTex[i]);
		SAFE_RELEASE(IDirect3DBaseTexture9, renderer->savedTex[i]);
	}
	IDirect3DDevice9_SetVertexShader(dev, renderer->savedVS);
	IDirect3DDevice9_SetPixelShader(dev, renderer->savedPS);
	IDirect3DDevice9_SetVertexDeclaration(dev, renderer->savedDecl);
	SAFE_RELEASE(IDirect3DVertexShader9, renderer->savedVS);
	SAFE_RELEASE(IDirect3DPixelShader9, renderer->savedPS);
	SAFE_RELEASE(IDirect3DVertexDeclaration9, renderer->savedDecl);
	IDirect3DDevice9_SetViewport(dev, &renderer->savedViewport);
	IDirect3DDevice9_SetScissorRect(dev, &renderer->savedScissor);
	IDirect3DDevice9_SetVertexShaderConstantF(dev, 0, renderer->savedVSConst[0], 4);
	IDirect3DDevice9_SetPixelShaderConstantF(dev, PS_BG, renderer->savedPSConst[0], PS_LAST + 1 - PS_BG);
}
#endif

/* Saves what the frontend had and sets up the device for this renderer. The
 * frontend's state is restored by _end() at the end of the frame. */
static void _begin(struct GBAVideoD3D9Renderer* renderer) {
	IDirect3DDevice9* dev = renderer->dev;
	DWORD i;
	if (renderer->begun) {
		return;
	}
	renderer->begun = true;
#ifdef _XBOX
	/* The pixel shaders are the GPU's work here; with more GPRs it can keep
	 * more of them in flight while texture fetches wait (default: 64/64) */
	IDirect3DDevice9_GetShaderGPRAllocation(dev, &renderer->savedGPR[0], &renderer->savedGPR[1], &renderer->savedGPR[2]);
	IDirect3DDevice9_SetShaderGPRAllocation(dev, 0, 16, 112);
#endif
#ifdef D3D9_EDRAM
	_saveFrontendState(renderer);
#else
	IDirect3DStateBlock9_Capture(renderer->stateBlock);
#endif
	renderer->savedRT = NULL;
	renderer->savedDS = NULL;
	if (FAILED(IDirect3DDevice9_GetRenderTarget(dev, 0, &renderer->savedRT))) {
		renderer->savedRT = NULL;
	}
	if (FAILED(IDirect3DDevice9_GetDepthStencilSurface(dev, &renderer->savedDS))) {
		renderer->savedDS = NULL;
	}
	renderer->currentRT[0] = NULL;
	renderer->currentRT[1] = NULL;
	renderer->currentPS = NULL;
#ifdef D3D9_EDRAM
	renderer->currentBand = -1;
	_setBand(renderer, 0);
#else
	renderer->targetTop = 0;
	renderer->targetRows = GBA_VIDEO_VERTICAL_PIXELS;
#endif

	IDirect3DDevice9_SetDepthStencilSurface(dev, renderer->depth);
	renderer->currentDS = renderer->depth;
	renderer->targetScale = renderer->scale;
	IDirect3DDevice9_SetVertexShader(dev, renderer->vs);
	IDirect3DDevice9_SetVertexDeclaration(dev, renderer->decl);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, D3DZB_FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ZFUNC, D3DCMP_LESS);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILREF, 1);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILMASK, 1);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILWRITEMASK, 1);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, TRUE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xF);
#ifdef _XBOX
	/* The frontend draws pre-transformed quads with it off. Hi-Z off: the
	 * depth test of the sprites has to be exact */
	IDirect3DDevice9_SetRenderState(dev, D3DRS_VIEWPORTENABLE, TRUE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_HIZENABLE, D3DHIZ_DISABLE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_HIZWRITEENABLE, D3DHIZ_DISABLE);
#else
	IDirect3DDevice9_SetRenderState(dev, D3DRS_FILLMODE, D3DFILL_SOLID);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_TWOSIDEDSTENCILMODE, FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE1, 0xF);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_CLIPPING, TRUE);
#endif
	for (i = 0; i < S_MAX; ++i) {
		_setSampler(dev, i);
	}
	IDirect3DDevice9_SetTexture(dev, renderer->vramTex.stage, (IDirect3DBaseTexture9*) renderer->vramTex.tex[renderer->vramTex.current]);
	IDirect3DDevice9_SetTexture(dev, renderer->paletteTex.stage, (IDirect3DBaseTexture9*) renderer->paletteTex.tex[renderer->paletteTex.current]);
	IDirect3DDevice9_SetTexture(dev, renderer->lineTex.stage, (IDirect3DBaseTexture9*) renderer->lineTex.tex[renderer->lineTex.current]);
	for (i = 3; i < S_MAX; ++i) {
		IDirect3DDevice9_SetTexture(dev, i, NULL);
	}
	/* (the frontend's own drawing doesn't count) */
}

static void _end(struct GBAVideoD3D9Renderer* renderer) {
	IDirect3DDevice9* dev = renderer->dev;
	if (!renderer->begun) {
		return;
	}
	renderer->begun = false;
#ifdef _XBOX
	/* (it has to be the default again before the frontend's Present) */
	if (renderer->savedGPR[1] == 64 && renderer->savedGPR[2] == 64) {
		IDirect3DDevice9_SetShaderGPRAllocation(dev, 0, 0, 0);
	} else {
		IDirect3DDevice9_SetShaderGPRAllocation(dev, renderer->savedGPR[0], renderer->savedGPR[1], renderer->savedGPR[2]);
	}
#endif
	IDirect3DDevice9_SetRenderTarget(dev, 1, NULL);
	if (renderer->savedRT) {
		IDirect3DDevice9_SetRenderTarget(dev, 0, renderer->savedRT);
	}
	IDirect3DDevice9_SetDepthStencilSurface(dev, renderer->savedDS);
	SAFE_RELEASE(IDirect3DSurface9, renderer->savedRT);
	SAFE_RELEASE(IDirect3DSurface9, renderer->savedDS);
	/* after SetRenderTarget, which resets the viewport */
#ifdef D3D9_EDRAM
	_restoreFrontendState(renderer);
#else
	IDirect3DStateBlock9_Apply(renderer->stateBlock);
#endif
}

static IDirect3DSurface9* _targetSurface(struct GBAVideoD3D9Renderer* renderer, int rt) {
#ifdef D3D9_EDRAM
	if (rt == D3D9_RT_OUTPUT) {
		return renderer->edramOut;
	}
	if (_layerScale(renderer, rt) != renderer->scale) {
		return renderer->edramLow[rt - D3D9_RT_BG0];
	}
	return renderer->edram[rt < D3D9_RT_OUTPUT ? rt : rt - 1];
#else
	if (_layerScale(renderer, rt) != renderer->scale) {
		return renderer->rtSurfLow[rt - D3D9_RT_BG0];
	}
	return renderer->rtSurf[rt];
#endif
}

static void _setTargets(struct GBAVideoD3D9Renderer* renderer, int rt0, int rt1) {
	IDirect3DSurface9* s0 = _targetSurface(renderer, rt0);
	IDirect3DSurface9* s1 = rt1 >= 0 ? _targetSurface(renderer, rt1) : NULL;
	if (renderer->currentRT[1] != s1 && !s1) {
		IDirect3DDevice9_SetRenderTarget(renderer->dev, 1, NULL);
		renderer->currentRT[1] = NULL;
	}
	if (renderer->currentRT[0] != s0) {
		D3DVIEWPORT9 vp;
		IDirect3DSurface9* ds;
		IDirect3DDevice9_SetRenderTarget(renderer->dev, 0, s0);
		renderer->currentRT[0] = s0;
		renderer->targetScale = _layerScale(renderer, rt0);
		/* Depth only where it's the same size (on Xbox 360 the colour and
		 * depth surfaces share the EDRAM pitch): nothing but the sprites
		 * use it */
		ds = (rt0 == D3D9_RT_OUTPUT || renderer->targetScale != renderer->scale) ? NULL : renderer->depth;
		if (renderer->currentDS != ds) {
			IDirect3DDevice9_SetDepthStencilSurface(renderer->dev, ds);
			renderer->currentDS = ds;
		}
		vp.X = 0;
		vp.Y = 0;
		vp.Width = GBA_VIDEO_HORIZONTAL_PIXELS * renderer->targetScale;
		vp.Height = renderer->targetRows * renderer->targetScale;
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
		IDirect3DDevice9_SetViewport(renderer->dev, &vp);
	}
	if (renderer->currentRT[1] != s1) {
		IDirect3DDevice9_SetRenderTarget(renderer->dev, 1, s1);
		renderer->currentRT[1] = s1;
	}
}

static void _setPS(struct GBAVideoD3D9Renderer* renderer, int shader) {
	if (renderer->currentPS != renderer->ps[shader]) {
		IDirect3DDevice9_SetPixelShader(renderer->dev, renderer->ps[shader]);
		renderer->currentPS = renderer->ps[shader];
	}
}

static void _psConst(struct GBAVideoD3D9Renderer* renderer, UINT reg, float x, float y, float z, float w) {
	float v[4];
	v[0] = x;
	v[1] = y;
	v[2] = z;
	v[3] = w;
	IDirect3DDevice9_SetPixelShaderConstantF(renderer->dev, reg, v, 1);
}

/* Scissor in GBA pixels (whole-screen scanlines); clipped to the bound
 * render target and moved to its coordinates */
static bool _scissor(struct GBAVideoD3D9Renderer* renderer, int x0, int y0, int x1, int y1) {
	RECT r;
	if (x0 < 0) {
		x0 = 0;
	}
	if (y0 < renderer->targetTop) {
		y0 = renderer->targetTop;
	}
	if (x1 > GBA_VIDEO_HORIZONTAL_PIXELS) {
		x1 = GBA_VIDEO_HORIZONTAL_PIXELS;
	}
	if (y1 > renderer->targetTop + renderer->targetRows) {
		y1 = renderer->targetTop + renderer->targetRows;
	}
	if (x1 < x0) {
		x1 = x0;
	}
	if (y1 < y0) {
		y1 = y0;
	}
	r.left = x0 * renderer->targetScale;
	r.top = (y0 - renderer->targetTop) * renderer->targetScale;
	r.right = x1 * renderer->targetScale;
	r.bottom = (y1 - renderer->targetTop) * renderer->targetScale;
	IDirect3DDevice9_SetScissorRect(renderer->dev, &r);
	return r.right > r.left && r.bottom > r.top;
}

/* Clears the current render target(s) inside a rectangle (GBA pixels) */
static void _clear(struct GBAVideoD3D9Renderer* renderer, DWORD flags, D3DCOLOR color, int x0, int y0, int x1, int y1) {
	D3DRECT rect;
	RECT scissor;
	if (!_scissor(renderer, x0, y0, x1, y1)) {
		return;
	}
	IDirect3DDevice9_GetScissorRect(renderer->dev, &scissor);
	rect.x1 = scissor.left;
	rect.y1 = scissor.top;
	rect.x2 = scissor.right;
	rect.y2 = scissor.bottom;
	IDirect3DDevice9_Clear(renderer->dev, 1, &rect, flags, color, 1.0f, 0);
}

static D3DCOLOR _flagsColor(int x, int y, int z, int w) {
	return D3DCOLOR_ARGB(w & 0xFF, x & 0xFF, y & 0xFF, z & 0xFF);
}

/* Draws the unit quad at rect (GBA pixels) with texCoord going from
 * (tx, ty) to (tx + tw, ty + th) */
static void _drawQuad(struct GBAVideoD3D9Renderer* renderer, float x, float y, float w, float h, float tx, float ty, float tw, float th, float z) {
	static const float quad[12] = {
		0, 0, 0,
		1, 0, 0,
		0, 1, 0,
		1, 1, 0
	};
	float c[16];
	c[0] = x;
	c[1] = y - renderer->targetTop;
	c[2] = w;
	c[3] = h;
	c[4] = tx;
	c[5] = ty;
	c[6] = tw;
	c[7] = th;
	c[8] = 2.0f / GBA_VIDEO_HORIZONTAL_PIXELS;
	c[9] = 2.0f / renderer->targetRows;
	/* D3D9 pixel centres are at integer coordinates (also on Xbox 360, where
	 * D3DRS_HALFPIXELOFFSET is left alone): half a pixel up and left */
	c[10] = -1.0f / (GBA_VIDEO_HORIZONTAL_PIXELS * renderer->targetScale);
	c[11] = 1.0f / (renderer->targetRows * renderer->targetScale);
	c[12] = z;
	c[13] = 0;
	c[14] = 0;
	c[15] = 0;
	IDirect3DDevice9_SetVertexShaderConstantF(renderer->dev, 0, c, 4);
	IDirect3DDevice9_DrawPrimitiveUP(renderer->dev, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(float) * 3);
}

/* Starts writing a CPU texture: the next one of the ring, once the GPU is
 * done with it */
static void* _uploadBegin(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Upload* upload, int* pitch) {
	D3DLOCKED_RECT lock;
#ifdef D3D9_EDRAM
	/* (GetCurrentFence: the fence of everything sent so far, without the
	 * kick-off that InsertFence does, which costs CPU) */
	upload->fence[upload->current] = IDirect3DDevice9_GetCurrentFence(renderer->dev);
	upload->fenced[upload->current] = true;
	upload->current = (upload->current + 1) % upload->count;
	if (upload->fenced[upload->current] && IDirect3DDevice9_IsFencePending(renderer->dev, upload->fence[upload->current])) {
		IDirect3DDevice9_BlockOnFence(renderer->dev, upload->fence[upload->current]);
	}
#else
	UNUSED(renderer);
#endif
	if (FAILED(IDirect3DTexture9_LockRect(upload->tex[upload->current], 0, &lock, NULL, D3D9_CPU_LOCK))) {
		return NULL;
	}
	*pitch = lock.Pitch;
	return lock.pBits;
}

static void _uploadEnd(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Upload* upload) {
	IDirect3DTexture9_UnlockRect(upload->tex[upload->current], 0);
	if (renderer->begun) {
		IDirect3DDevice9_SetTexture(renderer->dev, upload->stage, (IDirect3DBaseTexture9*) upload->tex[upload->current]);
	}
}

/* Rows first..last of `src` (width x height texels of `texel` bytes, rows
 * `srcPitch` apart) into the next texture of the ring */
static void _uploadRows(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Upload* upload, const void* src, int srcPitch, int width, int height, int texel, int first, int last) {
	int pitch;
	uint8_t* bits = _uploadBegin(renderer, upload, &pitch);
	if (!bits) {
		return;
	}
#ifndef D3D9_EDRAM
	/* the whole texture is discarded */
	first = 0;
	last = height - 1;
#endif
	if (first < 0) {
		first = 0;
	}
	if (last >= height) {
		last = height - 1;
	}
	for (; first <= last; ++first) {
		memcpy(bits + pitch * first, (const uint8_t*) src + srcPitch * first, width * texel);
	}
	_uploadEnd(renderer, upload);
}

static void _uploadVRAM(struct GBAVideoD3D9Renderer* renderer) {
#ifdef __BIG_ENDIAN__
	/* VRAM is kept little endian; the texture wants native halfwords */
	int pitch;
	int y;
	uint8_t* bits = _uploadBegin(renderer, &renderer->vramTex, &pitch);
	if (!bits) {
		return;
	}
	for (y = 0; y < 192; ++y) {
		const uint16_t* src = &renderer->d.vram[256 * y];
		uint16_t* dst = (uint16_t*) (bits + pitch * y);
		int x;
		for (x = 0; x < 256; ++x) {
			dst[x] = (uint16_t) ((src[x] >> 8) | (src[x] << 8));
		}
	}
	_uploadEnd(renderer, &renderer->vramTex);
#else
	/* A8L8: L is the low byte, A the high one */
	_uploadRows(renderer, &renderer->vramTex, renderer->d.vram, 512, 256, 192, 2, 0, 191);
#endif
}

/* Rows first..last of the per-scanline palette. On PC the whole texture is
 * discarded, so all of it; on Xbox 360 only the rows the next draws read,
 * except before the composition (which reads all of them) */
static void _uploadPalette(struct GBAVideoD3D9Renderer* renderer, int first, int last) {
	_uploadRows(renderer, &renderer->paletteTex, renderer->shadowPalette, sizeof(renderer->shadowPalette[0]), 512, GBA_VIDEO_VERTICAL_PIXELS, 2, first, last);
}

static void _uploadLineData(struct GBAVideoD3D9Renderer* renderer, int first, int last) {
	_uploadRows(renderer, &renderer->lineTex, renderer->lineData, sizeof(renderer->lineData[0]), GBA_D3D9_LD_WIDTH, GBA_VIDEO_VERTICAL_PIXELS, 16, first, last);
}

static void _setLine(struct GBAVideoD3D9Renderer* renderer, int y, int column, float x, float yy, float z, float w) {
	float* v = renderer->lineData[y][column];
	v[0] = x;
	v[1] = yy;
	v[2] = z;
	v[3] = w;
}

#ifdef D3D9_EDRAM
static int _bandCount(const struct GBAVideoD3D9Renderer* renderer) {
	return (GBA_VIDEO_VERTICAL_PIXELS + renderer->bandRows - 1) / renderer->bandRows;
}

static void _setBand(struct GBAVideoD3D9Renderer* renderer, int band) {
	renderer->targetTop = band * renderer->bandRows;
	renderer->targetRows = renderer->bandRows;
	/* the viewport depends on targetRows */
	renderer->currentRT[0] = NULL;
	renderer->currentRT[1] = NULL;
}

/* For the output, which is drawn whole (without depth, see _setTargets) */
static void _setFull(struct GBAVideoD3D9Renderer* renderer) {
	renderer->targetTop = 0;
	renderer->targetRows = GBA_VIDEO_VERTICAL_PIXELS;
	renderer->currentRT[0] = NULL;
	renderer->currentRT[1] = NULL;
}

/* Bound render target `index` (0 or 1) -> its texture: for a layer, the
 * rows of the current band into the band's texture; for the output, all of
 * it. Always from (0, 0) to (0, 0) (8-aligned: bandRows * scale is a
 * multiple of 8) */
static void _edramResolve(struct GBAVideoD3D9Renderer* renderer, int index, int layer) {
	D3DRECT rect;
	D3DPOINT point;
	RECT scissor;
	IDirect3DTexture9* texture;
	HRESULT hr;
	rect.x1 = 0;
	rect.y1 = 0;
	rect.x2 = GBA_VIDEO_HORIZONTAL_PIXELS * renderer->targetScale;
	if (layer == D3D9_RT_OUTPUT) {
		rect.y2 = _rtHeight(renderer);
		texture = renderer->rtTex[D3D9_RT_OUTPUT];
	} else {
		int rows = GBA_VIDEO_VERTICAL_PIXELS - renderer->targetTop;
		if (rows > renderer->targetRows) {
			rows = renderer->targetRows;
		}
		rect.y2 = rows * renderer->targetScale;
		if (_layerScale(renderer, layer) != renderer->scale) {
			texture = renderer->bandTexLow[renderer->currentBand][layer - D3D9_RT_BG0];
		} else {
			texture = renderer->bandTex[renderer->currentBand][layer];
		}
	}
	point.x = 0;
	point.y = 0;
	/* nothing of it left out by the scissor of the last draw */
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = rect.x2;
	scissor.bottom = rect.y2;
	IDirect3DDevice9_SetScissorRect(renderer->dev, &scissor);
	hr = IDirect3DDevice9_Resolve(renderer->dev, index ? D3DRESOLVE_RENDERTARGET1 : D3DRESOLVE_RENDERTARGET0, &rect,
	                              (IDirect3DBaseTexture9*) texture, &point, 0, 0, NULL, 0.0f, 0, NULL);
	if (FAILED(hr)) {
		mLOG(GBA_VIDEO, ERROR, "D3D9: Resolve failed (0x%08lX)", (unsigned long) hr);
	}
}

/* Resolves every layer of the band being drawn */
static void _edramLeaveBand(struct GBAVideoD3D9Renderer* renderer) {
	int i;
	if (renderer->currentBand < 0) {
		return;
	}
	IDirect3DDevice9_SetRenderState(renderer->dev, D3DRS_ZENABLE, D3DZB_FALSE);
	IDirect3DDevice9_SetRenderState(renderer->dev, D3DRS_STENCILENABLE, FALSE);
	for (i = 0; i < D3D9_RT_MAX; ++i) {
		if (i == D3D9_RT_OUTPUT) {
			continue;
		}
		_setTargets(renderer, i, -1);
		_edramResolve(renderer, 0, i);
	}
	renderer->currentBand = -1;
}

/* Moves to the band of scanline y: resolves the previous one and starts the
 * new one cleared, like gl.c clears its layers at scanline 0 */
static void _edramEnterBand(struct GBAVideoD3D9Renderer* renderer, int y) {
	int band = y / renderer->bandRows;
	int i;
	if (band == renderer->currentBand) {
		return;
	}
	_edramLeaveBand(renderer);
	renderer->currentBand = band;
	_setBand(renderer, band);
	_setTargets(renderer, D3D9_RT_OBJ_COLOR, D3D9_RT_OBJ_FLAGS);
	_clear(renderer, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	for (i = 0; i < 4; ++i) {
		_setTargets(renderer, D3D9_RT_BG0 + i, -1);
		_clear(renderer, D3DCLEAR_TARGET, 0, 0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	}
}

/* EDRAM size of a 32-bit surface, in 5 KB tiles of 80x16 pixels */
static unsigned _edramTiles(unsigned width, unsigned height, DWORD multisample) {
#ifdef _XBOX
	return XGSurfaceSize(width, height, D3DFMT_A8R8G8B8, (D3DMULTISAMPLE_TYPE) multisample);
#else
	unsigned samples = multisample ? multisample : 1;
	return ((width + 79) / 80) * ((height + 15) / 16) * samples;
#endif
}

static HRESULT _createEdramSurface(struct GBAVideoD3D9Renderer* renderer, bool depth, UINT width, UINT height, unsigned base, IDirect3DSurface9** surface) {
#ifdef _XBOX
	D3DSURFACE_PARAMETERS params;
	memset(&params, 0, sizeof(params));
	params.Base = base;
	if (depth) {
		return IDirect3DDevice9_CreateDepthStencilSurface(renderer->dev, width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, surface, &params);
	}
	return IDirect3DDevice9_CreateRenderTarget(renderer->dev, width, height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, surface, &params);
#else
	UNUSED(base);
	if (depth) {
		return IDirect3DDevice9_CreateDepthStencilSurface(renderer->dev, width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, surface, NULL);
	}
	return IDirect3DDevice9_CreateRenderTarget(renderer->dev, width, height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, surface, NULL);
#endif
}
#endif

/* ------------------------------------------------------------------------
 * Resources
 * ------------------------------------------------------------------------ */

static void _releaseTargets(struct GBAVideoD3D9Renderer* renderer) {
	int i;
	for (i = 0; i < D3D9_RT_MAX; ++i) {
		SAFE_RELEASE(IDirect3DSurface9, renderer->rtSurf[i]);
		SAFE_RELEASE(IDirect3DTexture9, renderer->rtTex[i]);
	}
	SAFE_RELEASE(IDirect3DSurface9, renderer->depth);
	for (i = 0; i < 4; ++i) {
		SAFE_RELEASE(IDirect3DSurface9, renderer->rtSurfLow[i]);
		SAFE_RELEASE(IDirect3DTexture9, renderer->rtTexLow[i]);
		renderer->bgLowres[i] = false;
		renderer->bgHires[i] = false;
	}
#ifdef D3D9_EDRAM
	for (i = 0; i < D3D9_RT_MAX - 1; ++i) {
		SAFE_RELEASE(IDirect3DSurface9, renderer->edram[i]);
	}
	SAFE_RELEASE(IDirect3DSurface9, renderer->edramOut);
	{
		int band;
		for (band = 0; band < GBA_VIDEO_VERTICAL_PIXELS / 8; ++band) {
			for (i = 0; i < D3D9_RT_MAX; ++i) {
				SAFE_RELEASE(IDirect3DTexture9, renderer->bandTex[band][i]);
			}
			for (i = 0; i < 4; ++i) {
				SAFE_RELEASE(IDirect3DTexture9, renderer->bandTexLow[band][i]);
			}
		}
		for (i = 0; i < 4; ++i) {
			SAFE_RELEASE(IDirect3DSurface9, renderer->edramLow[i]);
		}
	}
#endif
}

static bool _createTargets(struct GBAVideoD3D9Renderer* renderer) {
	int i;
	HRESULT hr;
#ifdef D3D9_EDRAM
	unsigned frontendTiles = 0;
	unsigned tiles = 0;
	int surfaces = D3D9_RT_MAX; /* 7 colour + depth */
	unsigned outTiles;
	unsigned lowTiles;
	int bands;
	int band;
	int base0Rows = 0;
	IDirect3DSurface9* backBuffer = NULL;
	if (SUCCEEDED(IDirect3DDevice9_GetBackBuffer(renderer->dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) && backBuffer) {
		D3DSURFACE_DESC desc;
		if (SUCCEEDED(IDirect3DSurface9_GetDesc(backBuffer, &desc))) {
			frontendTiles = _edramTiles(desc.Width, desc.Height, desc.MultiSampleType);
		}
		SAFE_RELEASE(IDirect3DSurface9, backBuffer);
	}
	/* As few bands as possible (of a multiple of 8 scanlines, so that the
	 * resolves are 8-aligned), behind the frontend's back buffer if there's
	 * room. If not, from the start of the EDRAM: the frontend only draws
	 * between two retro_run, and nothing of ours has to stay in the EDRAM
	 * after finishFrame */
	renderer->bandRows = 0;
	for (bands = 1; bands <= GBA_VIDEO_VERTICAL_PIXELS / 8; ++bands) {
		int rows = ((GBA_VIDEO_VERTICAL_PIXELS + bands - 1) / bands + 7) & ~7;
		/* plus the four 1x BGs */
		unsigned t = surfaces * _edramTiles(_rtWidth(renderer), rows * renderer->scale, 0) + 4 * _edramTiles(GBA_VIDEO_HORIZONTAL_PIXELS, rows, 0);
		if (!base0Rows && t <= D3D9_EDRAM_TILES) {
			base0Rows = rows;
		}
		if (frontendTiles + t <= D3D9_EDRAM_TILES) {
			renderer->bandRows = rows;
			break;
		}
	}
	if (!base0Rows) {
		_fail("%ix%i doesn't fit in the EDRAM", _rtWidth(renderer), _rtHeight(renderer));
		return false;
	}
	if (renderer->bandRows && renderer->bandRows * 2 >= base0Rows) {
		/* behind the frontend, with at most twice as many bands */
		renderer->edramBase = frontendTiles;
	} else {
		renderer->bandRows = base0Rows;
		renderer->edramBase = 0;
	}
	tiles = _edramTiles(_rtWidth(renderer), renderer->bandRows * renderer->scale, 0);
	renderer->currentBand = -1;
	for (i = 0; i < D3D9_RT_MAX - 1; ++i) {
		hr = _createEdramSurface(renderer, false, _rtWidth(renderer), renderer->bandRows * renderer->scale, renderer->edramBase + i * tiles, &renderer->edram[i]);
		if (FAILED(hr)) {
			_fail("can't create a %ix%i EDRAM surface at tile %u (0x%08lX)", _rtWidth(renderer), renderer->bandRows * renderer->scale, renderer->edramBase + i * tiles, (unsigned long) hr);
			_releaseTargets(renderer);
			return false;
		}
	}
	hr = _createEdramSurface(renderer, true, _rtWidth(renderer), renderer->bandRows * renderer->scale, renderer->edramBase + (D3D9_RT_MAX - 1) * tiles, &renderer->depth);
	if (FAILED(hr)) {
		_fail("can't create the depth/stencil surface (0x%08lX)", (unsigned long) hr);
		_releaseTargets(renderer);
		return false;
	}
	lowTiles = _edramTiles(GBA_VIDEO_HORIZONTAL_PIXELS, renderer->bandRows, 0);
	for (i = 0; i < 4; ++i) {
		hr = _createEdramSurface(renderer, false, GBA_VIDEO_HORIZONTAL_PIXELS, renderer->bandRows, renderer->edramBase + surfaces * tiles + i * lowTiles, &renderer->edramLow[i]);
		if (FAILED(hr)) {
			_fail("can't create a %ix%i EDRAM surface (0x%08lX)", GBA_VIDEO_HORIZONTAL_PIXELS, renderer->bandRows, (unsigned long) hr);
			_releaseTargets(renderer);
			return false;
		}
	}
	/* The output is drawn whole once the layers are in their textures, over
	 * the layers' EDRAM if it fits there */
	outTiles = _edramTiles(_rtWidth(renderer), _rtHeight(renderer), 0);
	renderer->edramOutBase = renderer->edramBase + outTiles <= D3D9_EDRAM_TILES ? renderer->edramBase : 0;
	hr = _createEdramSurface(renderer, false, _rtWidth(renderer), _rtHeight(renderer), renderer->edramOutBase, &renderer->edramOut);
	if (FAILED(hr)) {
		_fail("can't create a %ix%i EDRAM surface at tile %u (0x%08lX)", _rtWidth(renderer), _rtHeight(renderer), renderer->edramOutBase, (unsigned long) hr);
		_releaseTargets(renderer);
		return false;
	}
	mLOG(GBA_VIDEO, INFO, "D3D9: %ix%i, %i band(s), EDRAM %u..%u (frontend %u)", _rtWidth(renderer), _rtHeight(renderer), _bandCount(renderer), renderer->edramBase, renderer->edramBase + surfaces * tiles + 4 * lowTiles, frontendTiles);
	/* Resolve targets (tiled, which is what Resolve writes): the output, and
	 * one per layer and band */
	hr = IDirect3DDevice9_CreateTexture(renderer->dev, _rtWidth(renderer), _rtHeight(renderer), 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &renderer->rtTex[D3D9_RT_OUTPUT], NULL);
	for (band = 0; band < _bandCount(renderer) && SUCCEEDED(hr); ++band) {
		for (i = 0; i < D3D9_RT_MAX && SUCCEEDED(hr); ++i) {
			if (i != D3D9_RT_OUTPUT) {
				hr = IDirect3DDevice9_CreateTexture(renderer->dev, _rtWidth(renderer), renderer->bandRows * renderer->scale, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &renderer->bandTex[band][i], NULL);
			}
		}
		for (i = 0; i < 4 && SUCCEEDED(hr); ++i) {
			hr = IDirect3DDevice9_CreateTexture(renderer->dev, GBA_VIDEO_HORIZONTAL_PIXELS, renderer->bandRows, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &renderer->bandTexLow[band][i], NULL);
		}
	}
	if (FAILED(hr)) {
		_fail("can't create the %ix%i textures (0x%08lX)", _rtWidth(renderer), _rtHeight(renderer), (unsigned long) hr);
		_releaseTargets(renderer);
		return false;
	}
	/* Start with a black output: the frontend may show it before the first
	 * frame is drawn */
	{
		bool begun = renderer->begun;
		_begin(renderer);
		_setFull(renderer);
		_setTargets(renderer, D3D9_RT_OUTPUT, -1);
		_clear(renderer, D3DCLEAR_TARGET, 0, 0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
		_edramResolve(renderer, 0, D3D9_RT_OUTPUT);
		if (!begun) {
			_end(renderer);
		}
	}
#else
	for (i = 0; i < D3D9_RT_MAX; ++i) {
		hr = IDirect3DDevice9_CreateTexture(renderer->dev, _rtWidth(renderer), _rtHeight(renderer), 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &renderer->rtTex[i], NULL);
		if (FAILED(hr)) {
			_fail("can't create a %ix%i render target (0x%08lX)", _rtWidth(renderer), _rtHeight(renderer), (unsigned long) hr);
			_releaseTargets(renderer);
			return false;
		}
		IDirect3DTexture9_GetSurfaceLevel(renderer->rtTex[i], 0, &renderer->rtSurf[i]);
	}
	for (i = 0; i < 4; ++i) {
		hr = IDirect3DDevice9_CreateTexture(renderer->dev, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &renderer->rtTexLow[i], NULL);
		if (FAILED(hr)) {
			_fail("can't create a %ix%i render target (0x%08lX)", GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS, (unsigned long) hr);
			_releaseTargets(renderer);
			return false;
		}
		IDirect3DTexture9_GetSurfaceLevel(renderer->rtTexLow[i], 0, &renderer->rtSurfLow[i]);
	}
	hr = IDirect3DDevice9_CreateDepthStencilSurface(renderer->dev, _rtWidth(renderer), _rtHeight(renderer), D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &renderer->depth, NULL);
	if (FAILED(hr)) {
		_fail("can't create the depth/stencil surface (0x%08lX)", (unsigned long) hr);
		_releaseTargets(renderer);
		return false;
	}
#endif
	return true;
}

/* Creates the render targets at the given scale or, if they don't fit, at
 * the largest one that does */
static bool _createTargetsUpTo(struct GBAVideoD3D9Renderer* renderer, int scale) {
	for (; scale >= 1; --scale) {
		renderer->scale = scale;
		if (_createTargets(renderer)) {
			return true;
		}
	}
	return false;
}

/* Compiled shaders are kept in the frontend's system directory: compiling
 * them takes a while, on Xbox 360 above all */
static uint32_t _hash(uint32_t hash, const char* string) {
	if (!string) {
		string = "-";
	}
	for (; *string; ++string) {
		hash = (hash ^ (uint8_t) *string) * 16777619U;
	}
	return (hash ^ 0xFF) * 16777619U;
}

static FILE* _openShaderCache(struct GBAVideoD3D9Renderer* renderer, uint32_t hash, const char* mode) {
	char path[1024];
	if (!renderer->shaderCache) {
		return NULL;
	}
	snprintf(path, sizeof(path), "%s" D3D9_PATH_SEP "mgba_d3d9_%08lx.bin", renderer->shaderCache, (unsigned long) hash);
	return fopen(path, mode);
}

static bool _createShader(struct GBAVideoD3D9Renderer* renderer, const char* profile, const DWORD* code, void** out) {
	HRESULT hr;
	if (profile[0] == 'v') {
		hr = IDirect3DDevice9_CreateVertexShader(renderer->dev, code, (IDirect3DVertexShader9**) out);
	} else {
		hr = IDirect3DDevice9_CreatePixelShader(renderer->dev, code, (IDirect3DPixelShader9**) out);
	}
	return !!SUCCEEDED(hr);
}

/* One attempt at a shader: from the cache, or compiled (and then cached).
 * A compile that failed is remembered in the cache too (an empty file), so
 * fallbacks don't make every start slower. On failure `error` gets why. */
static bool _compileWith(struct GBAVideoD3D9Renderer* renderer, const char* entry, const char* profile, const char* define, const char* define2, DWORD flags, void** out, char* error, size_t errorSize) {
	D3DXMACRO macros[3];
	LPD3DXBUFFER code = NULL;
	LPD3DXBUFFER errors = NULL;
	HRESULT hr;
	uint32_t hash = 2166136261U;
	char flagsText[16];
	FILE* cache;
	bool ok;

	snprintf(flagsText, sizeof(flagsText), "%lx", (unsigned long) flags);
	hash = _hash(hash, _d3d9Shaders);
	hash = _hash(hash, entry);
	hash = _hash(hash, profile);
	hash = _hash(hash, define);
	hash = _hash(hash, define2);
	if (flags) {
		hash = _hash(hash, flagsText);
	}
#ifdef _XBOX
	hash = _hash(hash, "xbox360");
#else
	hash = _hash(hash, "win32");
#endif
	error[0] = '\0';
	cache = _openShaderCache(renderer, hash, "rb");
	if (cache) {
		long size = -1;
		void* data = NULL;
		ok = false;
		if (fseek(cache, 0, SEEK_END) == 0 && (size = ftell(cache)) > 0 && fseek(cache, 0, SEEK_SET) == 0) {
			data = malloc(size);
			if (data && fread(data, 1, size, cache) == (size_t) size) {
				ok = _createShader(renderer, profile, (const DWORD*) data, out);
			}
		}
		free(data);
		fclose(cache);
		if (ok) {
			return true;
		}
		if (size == 0) {
			snprintf(error, errorSize, "%s %s%s%s flags %s: failed before (cached)", profile, entry, define ? " " : "", define ? define : "", flagsText);
			return false;
		}
	}

	macros[0].Name = define;
	macros[0].Definition = "1";
	macros[1].Name = define2;
	macros[1].Definition = "1";
	macros[2].Name = NULL;
	macros[2].Definition = NULL;
	if (!define2) {
		macros[1].Name = NULL;
		macros[1].Definition = NULL;
	}
	hr = D3DXCompileShader(_d3d9Shaders, (UINT) strlen(_d3d9Shaders), define ? macros : &macros[2], NULL, entry, profile, flags, &code, &errors, NULL);
	if (FAILED(hr) || !code) {
		/* (warnings come in the same buffer when it works: ignored) */
		snprintf(error, errorSize, "compiling %s %s%s%s flags %s (0x%08lX): %s", profile, entry, define ? " " : "", define ? define : "", flagsText, (unsigned long) hr,
		         errors ? (const char*) errors->lpVtbl->GetBufferPointer(errors) : "?");
		if (errors) {
			errors->lpVtbl->Release(errors);
		}
		if (code) {
			code->lpVtbl->Release(code);
		}
		cache = _openShaderCache(renderer, hash, "wb");
		if (cache) {
			fclose(cache);
		}
		return false;
	}
	if (errors) {
		errors->lpVtbl->Release(errors);
	}
	ok = _createShader(renderer, profile, (const DWORD*) code->lpVtbl->GetBufferPointer(code), out);
	if (!ok) {
		snprintf(error, errorSize, "creating %s %s%s%s flags %s", profile, entry, define ? " " : "", define ? define : "", flagsText);
	} else {
		cache = _openShaderCache(renderer, hash, "wb");
		if (cache) {
			fwrite(code->lpVtbl->GetBufferPointer(code), 1, code->lpVtbl->GetBufferSize(code), cache);
			fclose(cache);
		}
	}
	code->lpVtbl->Release(code);
	return ok;
}

static bool _compile(struct GBAVideoD3D9Renderer* renderer, const char* entry, const char* profile, const char* define, const char* define2, void** out) {
	char error[1024];
#ifdef D3DXSHADER_OPTIMIZATION_LEVEL3
	/* the GPU is what limits the higher scales: the most optimized code if
	 * the compiler manages it */
	if (_compileWith(renderer, entry, profile, define, define2, D3DXSHADER_OPTIMIZATION_LEVEL3, out, error, sizeof(error))) {
		return true;
	}
#endif
	if (_compileWith(renderer, entry, profile, define, define2, 0, out, error, sizeof(error))) {
		return true;
	}
	_fail("%s", error);
	return false;
}

/* The window shader makes the Xbox 360 microcode compiler crash (X4800,
 * "internal error"). Several ways of writing it (d3d9-shaders.h, WINDOW_*),
 * each with several compiler settings; the first one that works is used */
static bool _compileWindow(struct GBAVideoD3D9Renderer* renderer, void** out) {
	static const char* const variants[] = { NULL, "WINDOW_B", "WINDOW_C", "WINDOW_D" };
	DWORD flags[5];
	int nFlags = 0;
	char error[1024];
	char errors[3][1024];
	int v, f;

	flags[nFlags++] = 0;
#ifdef D3DXSHADER_SKIPOPTIMIZATION
	flags[nFlags++] = D3DXSHADER_SKIPOPTIMIZATION;
#endif
#ifdef D3DXSHADER_AVOID_FLOW_CONTROL
	flags[nFlags++] = D3DXSHADER_AVOID_FLOW_CONTROL;
#endif
#ifdef D3DXSHADER_PREFER_FLOW_CONTROL
	flags[nFlags++] = D3DXSHADER_PREFER_FLOW_CONTROL;
#endif
#ifdef D3DXSHADER_OPTIMIZATION_LEVEL1
	flags[nFlags++] = D3DXSHADER_OPTIMIZATION_LEVEL1;
#endif
	memset(errors, 0, sizeof(errors));
	for (v = 0; v < (int) (sizeof(variants) / sizeof(*variants)); ++v) {
		for (f = 0; f < nFlags; ++f) {
			if (_compileWith(renderer, "psWindow", "ps_3_0", variants[v], NULL, flags[f], out, error, sizeof(error))) {
				if (v || f) {
					mLOG(GBA_VIDEO, WARN, "D3D9: psWindow %s, flags %lx", variants[v] ? variants[v] : "A", (unsigned long) flags[f]);
				}
				return true;
			}
			/* keep a few of the reasons, different ones */
			if (!errors[0][0]) {
				strncpy(errors[0], error, sizeof(errors[0]) - 1);
			} else if (strcmp(errors[0], error) && !errors[1][0]) {
				strncpy(errors[1], error, sizeof(errors[1]) - 1);
			}
			strncpy(errors[2], error, sizeof(errors[2]) - 1);
		}
	}
	_fail("%s", errors[0]);
	if (errors[1][0]) {
		_fail("%s", errors[1]);
	}
	_fail("%s", errors[2]);
	return false;
}

static bool _createShaders(struct GBAVideoD3D9Renderer* renderer) {
	bool ok = true;
	/* all of them, so that every failure gets reported at once */
	ok = _compile(renderer, "vsMain", "vs_3_0", NULL, NULL, (void**) &renderer->vs) && ok;
	ok = _compile(renderer, "psMode0", "ps_3_0", NULL, NULL, (void**) &renderer->ps[D3D9_PS_MODE0_16]) && ok;
	ok = _compile(renderer, "psMode0", "ps_3_0", "TILE256", NULL, (void**) &renderer->ps[D3D9_PS_MODE0_256]) && ok;
	ok = _compile(renderer, "psMode2", "ps_3_0", NULL, NULL, (void**) &renderer->ps[D3D9_PS_MODE2]) && ok;
	ok = _compile(renderer, "psMode4", "ps_3_0", NULL, NULL, (void**) &renderer->ps[D3D9_PS_MODE4]) && ok;
	ok = _compile(renderer, "psMode35", "ps_3_0", NULL, NULL, (void**) &renderer->ps[D3D9_PS_MODE35]) && ok;
	ok = _compile(renderer, "psObj", "ps_3_0", NULL, NULL, (void**) &renderer->ps[D3D9_PS_OBJ_16]) && ok;
	ok = _compile(renderer, "psObj", "ps_3_0", "TILE256", NULL, (void**) &renderer->ps[D3D9_PS_OBJ_256]) && ok;
	ok = _compile(renderer, "psObj", "ps_3_0", "OBJWIN", NULL, (void**) &renderer->ps[D3D9_PS_OBJWIN_16]) && ok;
	ok = _compile(renderer, "psObj", "ps_3_0", "OBJWIN", "TILE256", (void**) &renderer->ps[D3D9_PS_OBJWIN_256]) && ok;
	ok = _compile(renderer, "psObjPriority", "ps_3_0", NULL, NULL, (void**) &renderer->ps[D3D9_PS_OBJ_PRIORITY]) && ok;
	if (!_compileWindow(renderer, (void**) &renderer->ps[D3D9_PS_WINDOW])) {
		/* Not fatal: WIN0/WIN1 are left out (see DrawWindow), the rest works.
		 * GetLastError says why */
		mLOG(GBA_VIDEO, WARN, "D3D9: no window shader, WIN0/WIN1 ignored");
	}
	ok = _compile(renderer, "psFinalize", "ps_3_0", NULL, NULL, (void**) &renderer->ps[D3D9_PS_FINALIZE]) && ok;
	return ok;
}

static void _releaseUpload(struct GBAVideoD3D9Upload* upload) {
	int i;
	for (i = 0; i < D3D9_RING; ++i) {
		SAFE_RELEASE(IDirect3DTexture9, upload->tex[i]);
	}
}

static HRESULT _createUpload(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Upload* upload, DWORD stage, int count, UINT width, UINT height, D3DFORMAT format) {
	HRESULT hr = D3D_OK;
	int i;
	upload->stage = stage;
	upload->current = 0;
	upload->count = count;
	for (i = 0; i < count && SUCCEEDED(hr); ++i) {
		upload->fenced[i] = false;
		hr = IDirect3DDevice9_CreateTexture(renderer->dev, width, height, 1, D3D9_CPU_USAGE, format, D3DPOOL_DEFAULT, &upload->tex[i], NULL);
	}
	return hr;
}

static bool _isUpload(const struct GBAVideoD3D9Upload* upload, IDirect3DBaseTexture9* texture) {
	int i;
	for (i = 0; i < upload->count; ++i) {
		if ((IDirect3DBaseTexture9*) upload->tex[i] == texture) {
			return true;
		}
	}
	return false;
}

static bool _isOurTexture(const struct GBAVideoD3D9Renderer* renderer, IDirect3DBaseTexture9* texture) {
	int i;
	for (i = 0; i < D3D9_RT_MAX; ++i) {
		if ((IDirect3DBaseTexture9*) renderer->rtTex[i] == texture) {
			return true;
		}
	}
	for (i = 0; i < 4; ++i) {
		if ((IDirect3DBaseTexture9*) renderer->rtTexLow[i] == texture) {
			return true;
		}
	}
#ifdef D3D9_EDRAM
	{
		int band;
		for (band = 0; band < GBA_VIDEO_VERTICAL_PIXELS / 8; ++band) {
			for (i = 0; i < D3D9_RT_MAX; ++i) {
				if ((IDirect3DBaseTexture9*) renderer->bandTex[band][i] == texture) {
					return true;
				}
			}
			for (i = 0; i < 4; ++i) {
				if ((IDirect3DBaseTexture9*) renderer->bandTexLow[band][i] == texture) {
					return true;
				}
			}
		}
	}
#endif
	return _isUpload(&renderer->vramTex, texture) || _isUpload(&renderer->paletteTex, texture) || _isUpload(&renderer->lineTex, texture);
}

/* Before releasing textures: nothing may still point at them. The frontend
 * leaves the output set on its sampler after drawing it, and on Xbox 360 the
 * device doesn't hold a reference to what's set on it (a released texture
 * left there is a dangling pointer, and the next GetTexture/SetTexture
 * crashes), nor does it wait for the GPU before freeing the memory */
static void _unbindTextures(struct GBAVideoD3D9Renderer* renderer) {
	DWORD i;
	if (!renderer->dev) {
		return;
	}
	for (i = 0; i < 16; ++i) {
		IDirect3DBaseTexture9* texture = NULL;
		bool ours;
		if (FAILED(IDirect3DDevice9_GetTexture(renderer->dev, i, &texture)) || !texture) {
			continue;
		}
		ours = _isOurTexture(renderer, texture);
		SAFE_RELEASE(IDirect3DBaseTexture9, texture);
		if (ours) {
			IDirect3DDevice9_SetTexture(renderer->dev, i, NULL);
		}
	}
#ifdef D3D9_EDRAM
	{
		DWORD fence = IDirect3DDevice9_InsertFence(renderer->dev);
		IDirect3DDevice9_BlockOnFence(renderer->dev, fence);
	}
#endif
}

static void _releaseAll(struct GBAVideoD3D9Renderer* renderer) {
	int i;
	_unbindTextures(renderer);
	_releaseTargets(renderer);
	SAFE_RELEASE(IDirect3DVertexShader9, renderer->vs);
	SAFE_RELEASE(IDirect3DVertexDeclaration9, renderer->decl);
	for (i = 0; i < D3D9_PS_MAX; ++i) {
		SAFE_RELEASE(IDirect3DPixelShader9, renderer->ps[i]);
	}
	_releaseUpload(&renderer->vramTex);
	_releaseUpload(&renderer->paletteTex);
	_releaseUpload(&renderer->lineTex);
#ifndef D3D9_EDRAM
	SAFE_RELEASE(IDirect3DStateBlock9, renderer->stateBlock);
#endif
	SAFE_RELEASE(IDirect3DDevice9, renderer->dev);
	free(renderer->shaderCache);
	renderer->shaderCache = NULL;
}

struct GBAVideoD3D9Renderer* GBAVideoD3D9RendererCreate(void* device, int scale, const char* shaderCacheDir) {
	static const D3DVERTEXELEMENT9 declaration[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		D3DDECL_END()
	};
	struct GBAVideoD3D9Renderer* renderer;
	IDirect3DDevice9* dev = (IDirect3DDevice9*) device;
	HRESULT hr;
	int i;
	_lastError[0] = '\0';
	if (!dev) {
		_fail("no device");
		return NULL;
	}
#ifndef _XBOX
	{
		D3DCAPS9 caps;
		if (SUCCEEDED(IDirect3DDevice9_GetDeviceCaps(dev, &caps))) {
			if (caps.PixelShaderVersion < D3DPS_VERSION(3, 0) || caps.VertexShaderVersion < D3DVS_VERSION(3, 0) || caps.NumSimultaneousRTs < 2) {
				_fail("the device needs shader model 3 and 2 render targets");
				return NULL;
			}
		}
	}
#endif
	if (scale < 1) {
		scale = 1;
	}
	if (scale > D3D9_MAX_SCALE) {
		scale = D3D9_MAX_SCALE;
	}

	renderer = calloc(1, sizeof(*renderer));
	if (!renderer) {
		return NULL;
	}
	renderer->dev = dev;
	IDirect3DDevice9_AddRef(dev);
	renderer->scale = scale;
	if (shaderCacheDir && shaderCacheDir[0]) {
		renderer->shaderCache = strdup(shaderCacheDir);
	}

	renderer->d.init = GBAVideoD3D9RendererInit;
	renderer->d.reset = GBAVideoD3D9RendererReset;
	renderer->d.deinit = GBAVideoD3D9RendererDeinit;
	renderer->d.rendererId = GBAVideoD3D9RendererId;
	renderer->d.loadState = GBAVideoD3D9RendererLoadState;
	renderer->d.saveState = GBAVideoD3D9RendererSaveState;
	renderer->d.writeVideoRegister = GBAVideoD3D9RendererWriteVideoRegister;
	renderer->d.writeVRAM = GBAVideoD3D9RendererWriteVRAM;
	renderer->d.writeOAM = GBAVideoD3D9RendererWriteOAM;
	renderer->d.writePalette = GBAVideoD3D9RendererWritePalette;
	renderer->d.drawScanline = GBAVideoD3D9RendererDrawScanline;
	renderer->d.finishFrame = GBAVideoD3D9RendererFinishFrame;
	renderer->d.getPixels = GBAVideoD3D9RendererGetPixels;
	renderer->d.putPixels = GBAVideoD3D9RendererPutPixels;
	for (i = 0; i < 128; ++i) {
		renderer->d.highlightOBJ[i] = false;
	}
	renderer->d.highlightColor = M_COLOR_WHITE;
	renderer->d.highlightAmount = 0;

	hr = _createUpload(renderer, &renderer->vramTex, 0, D3D9_RING_VRAM, 256, 192, D3D9_FMT_VRAM);
	if (FAILED(hr)) {
		_fail("can't create the VRAM texture (0x%08lX)", (unsigned long) hr);
	}
	if (SUCCEEDED(hr)) {
		hr = _createUpload(renderer, &renderer->paletteTex, 1, D3D9_RING_PALETTE, 512, GBA_VIDEO_VERTICAL_PIXELS, D3D9_FMT_PALETTE);
		if (FAILED(hr)) {
			_fail("can't create the palette texture (0x%08lX)", (unsigned long) hr);
		}
	}
	if (SUCCEEDED(hr)) {
		hr = _createUpload(renderer, &renderer->lineTex, 2, D3D9_RING_LINEDATA, GBA_D3D9_LD_WIDTH, GBA_VIDEO_VERTICAL_PIXELS, D3D9_FMT_LINEDATA);
		if (FAILED(hr)) {
			_fail("can't create the per-line data texture (0x%08lX)", (unsigned long) hr);
		}
	}
	if (SUCCEEDED(hr)) {
		hr = IDirect3DDevice9_CreateVertexDeclaration(dev, declaration, &renderer->decl);
		if (FAILED(hr)) {
			_fail("can't create the vertex declaration (0x%08lX)", (unsigned long) hr);
		}
	}
#ifndef D3D9_EDRAM
	if (SUCCEEDED(hr)) {
		hr = IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &renderer->stateBlock);
		if (FAILED(hr)) {
			_fail("can't create the state block (0x%08lX)", (unsigned long) hr);
		}
	}
#endif
	if (FAILED(hr)) {
		_releaseAll(renderer);
		free(renderer);
		return NULL;
	}
	/* The shaders first: the EDRAM path draws when it creates the targets */
	if (!_createShaders(renderer) || !_createTargetsUpTo(renderer, scale)) {
		_releaseAll(renderer);
		free(renderer);
		return NULL;
	}
	for (i = 0; i < 4; ++i) {
		renderer->bg[i].index = i;
	}
	return renderer;
}

void GBAVideoD3D9RendererDestroy(struct GBAVideoD3D9Renderer* renderer) {
	if (!renderer) {
		return;
	}
	_end(renderer);
	if (renderer->temporaryBuffer) {
		free(renderer->temporaryBuffer);
		renderer->temporaryBuffer = NULL;
	}
	_releaseAll(renderer);
	free(renderer);
}

struct GBAVideoRenderer* GBAVideoD3D9RendererGetBase(struct GBAVideoD3D9Renderer* renderer) {
	return &renderer->d;
}

int GBAVideoD3D9RendererGetScale(const struct GBAVideoD3D9Renderer* renderer) {
	return renderer->scale;
}

bool GBAVideoD3D9RendererSetScale(struct GBAVideoD3D9Renderer* renderer, int scale) {
	int oldScale = renderer->scale;
	if (scale < 1) {
		scale = 1;
	}
	if (scale > D3D9_MAX_SCALE) {
		scale = D3D9_MAX_SCALE;
	}
	if (scale == renderer->scale && renderer->rtTex[D3D9_RT_OUTPUT]) {
		return true;
	}
	_lastError[0] = '\0';
	_end(renderer);
	if (renderer->temporaryBuffer) {
		free(renderer->temporaryBuffer);
		renderer->temporaryBuffer = NULL;
	}
	_unbindTextures(renderer);
	_releaseTargets(renderer);
	if (!_createTargetsUpTo(renderer, scale)) {
		if (!_createTargetsUpTo(renderer, oldScale)) {
			return false;
		}
	}
	renderer->paletteDirty = true;
	return renderer->scale == scale;
}

void* GBAVideoD3D9RendererGetTexture(struct GBAVideoD3D9Renderer* renderer, unsigned* width, unsigned* height) {
	if (width) {
		*width = _rtWidth(renderer);
	}
	if (height) {
		*height = _rtHeight(renderer);
	}
	return renderer->rtTex[D3D9_RT_OUTPUT];
}

/* ------------------------------------------------------------------------
 * GBAVideoRenderer
 * ------------------------------------------------------------------------ */

static void GBAVideoD3D9RendererInit(struct GBAVideoRenderer* renderer) {
	/* All the resources are created along with the renderer */
	GBAVideoD3D9RendererReset(renderer);
}

static void GBAVideoD3D9RendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoD3D9Renderer* d3dRenderer = (struct GBAVideoD3D9Renderer*) renderer;
	_end(d3dRenderer);
}

static void GBAVideoD3D9RendererReset(struct GBAVideoRenderer* renderer) {
	struct GBAVideoD3D9Renderer* d3dRenderer = (struct GBAVideoD3D9Renderer*) renderer;
	int i;

	d3dRenderer->oamDirty = true;
	d3dRenderer->paletteDirty = true;
	d3dRenderer->vramDirty = 0xFFFFFF;
	d3dRenderer->firstAffine = -1;
	d3dRenderer->firstY = -1;
	d3dRenderer->lastY = -1;
	d3dRenderer->dispcnt = 0x0080;
	d3dRenderer->mosaic = 0;
	d3dRenderer->nextPalette = 0;
	d3dRenderer->paletteDirtyScanlines = GBA_VIDEO_VERTICAL_PIXELS;
	memset(d3dRenderer->shadowRegs, 0, sizeof(d3dRenderer->shadowRegs));
	d3dRenderer->shadowRegs[GBA_REG(DISPCNT)] = d3dRenderer->dispcnt;
	d3dRenderer->regsDirty = 0xFFFFFFFFFFFEULL;

	d3dRenderer->objOffsetX = 0;
	d3dRenderer->objOffsetY = 0;

	for (i = 0; i < 4; ++i) {
		struct GBAVideoD3D9Background* bg = &d3dRenderer->bg[i];
		bg->index = i;
		bg->enabledAtY = INT_MAX;
		bg->priority = 0;
		bg->charBase = 0;
		bg->mosaic = 0;
		bg->multipalette = 0;
		bg->screenBase = 0;
		bg->overflow = 0;
		bg->size = 0;
		bg->target1 = 0;
		bg->target2 = 0;
		bg->x = 0;
		bg->y = 0;
		bg->refx = 0;
		bg->refy = 0;
		bg->affine.dx = 256;
		bg->affine.dmx = 0;
		bg->affine.dy = 0;
		bg->affine.dmy = 256;
		bg->affine.sx = 0;
		bg->affine.sy = 0;
		bg->offsetX = 0;
		bg->offsetY = 0;
	}

	d3dRenderer->winN[0].offsetX = 0;
	d3dRenderer->winN[0].offsetY = 0;
	d3dRenderer->winN[1].offsetX = 0;
	d3dRenderer->winN[1].offsetY = 0;

	/* (after loading a savestate, the palette comes from here, not from
	 * WritePalette: palette RAM is kept little endian, hence LOAD_16) */
	for (i = 0; i < 512; ++i) {
		uint16_t color;
		int r, g, b;
		LOAD_16(color, i * 2, d3dRenderer->d.palette);
		r = M_R5(color);
		g = M_G5(color) << 1;
		b = M_B5(color);
		g |= g >> 5;
		d3dRenderer->shadowPalette[0][i] = (r << 11) | (g << 5) | b;
	}
}

static uint32_t GBAVideoD3D9RendererId(const struct GBAVideoRenderer* renderer) {
	UNUSED(renderer);
	return D3D9_MAGIC;
}

static bool GBAVideoD3D9RendererLoadState(struct GBAVideoRenderer* renderer, const void* state, size_t size) {
	UNUSED(renderer);
	UNUSED(state);
	UNUSED(size);
	return false;
}

static void GBAVideoD3D9RendererSaveState(struct GBAVideoRenderer* renderer, void** state, size_t* size) {
	UNUSED(renderer);
	*state = NULL;
	*size = 0;
}

static void GBAVideoD3D9RendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	struct GBAVideoD3D9Renderer* d3dRenderer = (struct GBAVideoD3D9Renderer*) renderer;
	if (renderer->cache) {
		mCacheSetWriteVRAM(renderer->cache, address);
	}
	d3dRenderer->vramDirty |= 1 << (address >> 12);
}

static void GBAVideoD3D9RendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	struct GBAVideoD3D9Renderer* d3dRenderer = (struct GBAVideoD3D9Renderer*) renderer;
	UNUSED(oam);
	d3dRenderer->oamDirty = true;
}

static void GBAVideoD3D9RendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoD3D9Renderer* d3dRenderer = (struct GBAVideoD3D9Renderer*) renderer;
	int r, g, b;
	if (renderer->cache) {
		mCacheSetWritePalette(renderer->cache, address >> 1, mColorFrom555(value));
	}
	d3dRenderer->paletteDirty = true;
	r = M_R5(value);
	g = M_G5(value) << 1;
	g |= g >> 5;
	b = M_B5(value);
	d3dRenderer->paletteDirtyScanlines = GBA_VIDEO_VERTICAL_PIXELS;
	d3dRenderer->shadowPalette[d3dRenderer->nextPalette][address >> 1] = (r << 11) | (g << 5) | b;
}

static void _writeWindowH(struct GBAVideoD3D9WindowN* win, uint16_t value) {
	win->h.end = value;
	win->h.start = value >> 8;
	if (win->h.start > GBA_VIDEO_HORIZONTAL_PIXELS && win->h.start > win->h.end) {
		win->h.start = 0;
	}
	if (win->h.end > GBA_VIDEO_HORIZONTAL_PIXELS) {
		win->h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
		if (win->h.start > GBA_VIDEO_HORIZONTAL_PIXELS) {
			win->h.start = GBA_VIDEO_HORIZONTAL_PIXELS;
		}
	}
}

static void _writeWindowV(struct GBAVideoD3D9WindowN* win, uint16_t value) {
	win->v.end = value;
	win->v.start = value >> 8;
	if (win->v.start > GBA_VIDEO_VERTICAL_PIXELS && win->v.start > win->v.end) {
		win->v.start = 0;
	}
	if (win->v.end > GBA_VIDEO_VERTICAL_PIXELS) {
		win->v.end = GBA_VIDEO_VERTICAL_PIXELS;
		if (win->v.start > GBA_VIDEO_VERTICAL_PIXELS) {
			win->v.start = GBA_VIDEO_VERTICAL_PIXELS;
		}
	}
}

static uint16_t GBAVideoD3D9RendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoD3D9Renderer* d3dRenderer = (struct GBAVideoD3D9Renderer*) renderer;
	bool dirty = false;
	if (renderer->cache) {
		GBAVideoCacheWriteVideoRegister(renderer->cache, address, value);
	}

	switch (address) {
	case GBA_REG_DISPCNT:
		value &= 0xFFF7;
		dirty = true;
		break;
	case GBA_REG_BG0CNT:
	case GBA_REG_BG1CNT:
		value &= 0xDFFF;
		dirty = true;
		break;
	case GBA_REG_BG0HOFS:
		value &= 0x01FF;
		d3dRenderer->bg[0].x = value;
		break;
	case GBA_REG_BG0VOFS:
		value &= 0x01FF;
		d3dRenderer->bg[0].y = value;
		break;
	case GBA_REG_BG1HOFS:
		value &= 0x01FF;
		d3dRenderer->bg[1].x = value;
		break;
	case GBA_REG_BG1VOFS:
		value &= 0x01FF;
		d3dRenderer->bg[1].y = value;
		break;
	case GBA_REG_BG2HOFS:
		value &= 0x01FF;
		d3dRenderer->bg[2].x = value;
		break;
	case GBA_REG_BG2VOFS:
		value &= 0x01FF;
		d3dRenderer->bg[2].y = value;
		break;
	case GBA_REG_BG3HOFS:
		value &= 0x01FF;
		d3dRenderer->bg[3].x = value;
		break;
	case GBA_REG_BG3VOFS:
		value &= 0x01FF;
		d3dRenderer->bg[3].y = value;
		break;
	case GBA_REG_BG2PA:
		d3dRenderer->bg[2].affine.dx = value;
		break;
	case GBA_REG_BG2PB:
		d3dRenderer->bg[2].affine.dmx = value;
		break;
	case GBA_REG_BG2PC:
		d3dRenderer->bg[2].affine.dy = value;
		break;
	case GBA_REG_BG2PD:
		d3dRenderer->bg[2].affine.dmy = value;
		break;
	case GBA_REG_BG2X_LO:
		GBAVideoD3D9RendererWriteBGX_LO(&d3dRenderer->bg[2], value);
		break;
	case GBA_REG_BG2X_HI:
		GBAVideoD3D9RendererWriteBGX_HI(&d3dRenderer->bg[2], value);
		break;
	case GBA_REG_BG2Y_LO:
		GBAVideoD3D9RendererWriteBGY_LO(&d3dRenderer->bg[2], value);
		break;
	case GBA_REG_BG2Y_HI:
		GBAVideoD3D9RendererWriteBGY_HI(&d3dRenderer->bg[2], value);
		break;
	case GBA_REG_BG3PA:
		d3dRenderer->bg[3].affine.dx = value;
		break;
	case GBA_REG_BG3PB:
		d3dRenderer->bg[3].affine.dmx = value;
		break;
	case GBA_REG_BG3PC:
		d3dRenderer->bg[3].affine.dy = value;
		break;
	case GBA_REG_BG3PD:
		d3dRenderer->bg[3].affine.dmy = value;
		break;
	case GBA_REG_BG3X_LO:
		GBAVideoD3D9RendererWriteBGX_LO(&d3dRenderer->bg[3], value);
		break;
	case GBA_REG_BG3X_HI:
		GBAVideoD3D9RendererWriteBGX_HI(&d3dRenderer->bg[3], value);
		break;
	case GBA_REG_BG3Y_LO:
		GBAVideoD3D9RendererWriteBGY_LO(&d3dRenderer->bg[3], value);
		break;
	case GBA_REG_BG3Y_HI:
		GBAVideoD3D9RendererWriteBGY_HI(&d3dRenderer->bg[3], value);
		break;
	case GBA_REG_BLDALPHA:
		value &= 0x1F1F;
		dirty = true;
		break;
	case GBA_REG_BLDY:
		value &= 0x1F;
		if (value > 0x10) {
			value = 0x10;
		}
		dirty = true;
		break;
	case GBA_REG_WIN0H:
		_writeWindowH(&d3dRenderer->winN[0], value);
		break;
	case GBA_REG_WIN1H:
		_writeWindowH(&d3dRenderer->winN[1], value);
		break;
	case GBA_REG_WIN0V:
		_writeWindowV(&d3dRenderer->winN[0], value);
		break;
	case GBA_REG_WIN1V:
		_writeWindowV(&d3dRenderer->winN[1], value);
		break;
	case GBA_REG_WININ:
	case GBA_REG_WINOUT:
		value &= 0x3F3F;
		dirty = true;
		break;
	default:
		dirty = true;
		break;
	}
	if (d3dRenderer->shadowRegs[address >> 1] == value) {
		dirty = false;
	} else {
		d3dRenderer->shadowRegs[address >> 1] = value;
	}
	if (dirty) {
		d3dRenderer->regsDirty |= 1ULL << (address >> 1);
	}
	return value;
}

static void _cleanRegister(struct GBAVideoD3D9Renderer* renderer, int address, uint16_t value) {
	switch (address) {
	case GBA_REG_DISPCNT:
		renderer->dispcnt = value;
		GBAVideoD3D9RendererUpdateDISPCNT(renderer);
		break;
	case GBA_REG_BG0CNT:
		GBAVideoD3D9RendererWriteBGCNT(&renderer->bg[0], value);
		break;
	case GBA_REG_BG1CNT:
		GBAVideoD3D9RendererWriteBGCNT(&renderer->bg[1], value);
		break;
	case GBA_REG_BG2CNT:
		GBAVideoD3D9RendererWriteBGCNT(&renderer->bg[2], value);
		break;
	case GBA_REG_BG3CNT:
		GBAVideoD3D9RendererWriteBGCNT(&renderer->bg[3], value);
		break;
	case GBA_REG_BLDCNT:
		GBAVideoD3D9RendererWriteBLDCNT(renderer, value);
		break;
	case GBA_REG_BLDALPHA:
		renderer->blda = value & 0x1F;
		if (renderer->blda > 0x10) {
			renderer->blda = 0x10;
		}
		renderer->bldb = (value >> 8) & 0x1F;
		if (renderer->bldb > 0x10) {
			renderer->bldb = 0x10;
		}
		break;
	case GBA_REG_BLDY:
		renderer->bldy = value;
		break;
	case GBA_REG_WININ:
		renderer->winN[0].control = value;
		renderer->winN[1].control = value >> 8;
		break;
	case GBA_REG_WINOUT:
		renderer->winout = value;
		renderer->objwin = value >> 8;
		break;
	case GBA_REG_MOSAIC:
		renderer->mosaic = value;
		break;
	default:
		break;
	}
}

static bool _dirtyMode0(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y) {
	unsigned screenBase, screenMask, charBase, charMask;
	if (y < background->enabledAtY) {
		return false;
	}
	screenBase = background->screenBase >> 11; // Lops off one extra bit
	screenMask = (7 << screenBase) & 0xFFFF; // Technically overzealous
	if (renderer->vramDirty & screenMask) {
		return true;
	}
	charBase = background->charBase >> 11;
	charMask = (0xFFFF << charBase) & 0xFFFF;
	if (renderer->vramDirty & charMask) {
		return true;
	}
	return false;
}

static bool _dirtyMode2(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y) {
	unsigned screenBase, screenMask, charBase, charMask;
	if (y < background->enabledAtY) {
		return false;
	}
	screenBase = background->screenBase >> 11; // Lops off one extra bit
	screenMask = (0xF << screenBase) & 0xFFFF;
	if (renderer->vramDirty & screenMask) {
		return true;
	}
	charBase = background->charBase >> 11;
	charMask = (0x3FFF << charBase) & 0xFFFF;
	if (renderer->vramDirty & charMask) {
		return true;
	}
	return false;
}

static bool _dirtyMode3(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y) {
	if (y < background->enabledAtY) {
		return false;
	}
	if (renderer->vramDirty & 0xFFFFF) {
		return true;
	}
	return false;
}

static bool _dirtyMode45(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y) {
	int start, mask;
	if (y < background->enabledAtY) {
		return false;
	}
	start = GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt) ? 5 : 0;
	mask = 0x3FF << start;
	if (renderer->vramDirty & mask) {
		return true;
	}
	return false;
}

static bool _needsVramUpload(struct GBAVideoD3D9Renderer* renderer, int y) {
	bool dirty = false;
	if (!renderer->vramDirty) {
		return false;
	}
	if (y == 0) {
		return true;
	}

	if (GBARegisterDISPCNTIsObjEnable(renderer->dispcnt) && renderer->vramDirty & 0xFF0000) {
		return true;
	}

	switch (GBARegisterDISPCNTGetMode(renderer->dispcnt)) {
	case 0:
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[0], y);
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[1], y);
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[2], y);
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[3], y);
		break;
	case 1:
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[0], y);
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[1], y);
		dirty = dirty || _dirtyMode2(renderer, &renderer->bg[2], y);
		break;
	case 2:
		dirty = dirty || _dirtyMode2(renderer, &renderer->bg[2], y);
		dirty = dirty || _dirtyMode2(renderer, &renderer->bg[3], y);
		break;
	case 3:
		dirty = _dirtyMode3(renderer, &renderer->bg[2], y);
		break;
	case 4:
		dirty = _dirtyMode45(renderer, &renderer->bg[2], y);
		break;
	case 5:
		dirty = _dirtyMode45(renderer, &renderer->bg[2], y);
		break;
	}
	return dirty;
}

static void GBAVideoD3D9RendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoD3D9Renderer* d3dRenderer = (struct GBAVideoD3D9Renderer*) renderer;
	int i;
	int oldPalette;

	_begin(d3dRenderer);

	if (GBARegisterDISPCNTGetMode(d3dRenderer->dispcnt) != 0) {
		if (d3dRenderer->firstAffine < 0) {
			d3dRenderer->firstAffine = y;
		}
	} else {
		d3dRenderer->firstAffine = -1;
	}

	if (_needsVramUpload(d3dRenderer, y) || d3dRenderer->oamDirty || d3dRenderer->regsDirty) {
		if (d3dRenderer->firstY >= 0) {
			_drawScanlines(d3dRenderer, y - 1);
			d3dRenderer->firstY = y;
		}
	}
	if (d3dRenderer->firstY < 0) {
		d3dRenderer->firstY = y;
	}
	d3dRenderer->lastY = y;

	for (i = 0; i < 0x30; ++i) {
		if (!(d3dRenderer->regsDirty & (1ULL << i))) {
			continue;
		}
		_cleanRegister(d3dRenderer, i << 1, d3dRenderer->shadowRegs[i]);
	}
	d3dRenderer->regsDirty = 0;

	for (i = 0; i < 2; ++i) {
		struct GBAVideoD3D9WindowN* win = &d3dRenderer->winN[i];
		int* history = &d3dRenderer->winNHistory[i][y * 4];
		history[0] = win->h.start + win->offsetX;
		history[1] = win->h.end + win->offsetX;
		history[2] = win->v.start + win->offsetY;
		history[3] = win->v.end + win->offsetY;
		_setLine(d3dRenderer, y, GBA_D3D9_LD_WIN0 + i, (float) history[0], (float) history[1], (float) history[2], (float) history[3]);
	}

	for (i = 0; i < 4; ++i) {
		struct GBAVideoD3D9Background* bg = &d3dRenderer->bg[i];
		_setLine(d3dRenderer, y, GBA_D3D9_LD_OFFSET + i, (float) ((bg->x - bg->offsetX) & 0x1FF), (float) ((bg->y - bg->offsetY) & 0x1FF), 0, 0);
	}
	for (i = 2; i < 4; ++i) {
		struct GBAVideoD3D9Background* bg = &d3dRenderer->bg[i];
		_setLine(d3dRenderer, y, GBA_D3D9_LD_AFFINE + i - 2, (float) bg->affine.dx, (float) bg->affine.dy, (float) bg->affine.sx, (float) bg->affine.sy);
	}

	oldPalette = d3dRenderer->nextPalette;
	d3dRenderer->nextPalette = y + 1;
	if (d3dRenderer->nextPalette >= GBA_VIDEO_VERTICAL_PIXELS) {
		d3dRenderer->nextPalette = 0;
	}
	if (d3dRenderer->paletteDirty) {
		memcpy(d3dRenderer->shadowPalette[d3dRenderer->nextPalette], d3dRenderer->shadowPalette[oldPalette], sizeof(d3dRenderer->shadowPalette[0]));
		if (d3dRenderer->paletteDirtyScanlines > 0) {
			--d3dRenderer->paletteDirtyScanlines;
		}
		if (!d3dRenderer->paletteDirtyScanlines) {
			d3dRenderer->paletteDirty = false;
			_uploadPalette(d3dRenderer, 0, GBA_VIDEO_VERTICAL_PIXELS - 1);
		}
	}

	if (_needsVramUpload(d3dRenderer, y)) {
		_uploadVRAM(d3dRenderer);
		d3dRenderer->vramDirty = 0;
	}

	if (d3dRenderer->oamDirty) {
		d3dRenderer->oamMax = GBAVideoRendererCleanOAM(d3dRenderer->d.oam->obj, d3dRenderer->sprites, 0);
		d3dRenderer->oamDirty = false;
	}

	if (y == 0) {
		int spriteCyclesRemaining;
#ifndef D3D9_EDRAM
		/* gl.c only clears the OBJ colour (plus depth and stencil); the OBJ
		 * flags are only read where there is OBJ colour. (EDRAM: every batch
		 * clears its own rows of every layer instead) */
		_setTargets(d3dRenderer, D3D9_RT_OBJ_COLOR, -1);
		_clear(d3dRenderer, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
		for (i = 0; i < 4; ++i) {
			_setTargets(d3dRenderer, D3D9_RT_BG0 + i, -1);
			_clear(d3dRenderer, D3DCLEAR_TARGET, 0, 0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
		}
#endif

		spriteCyclesRemaining = GBARegisterDISPCNTIsHblankIntervalFree(d3dRenderer->dispcnt) ? OBJ_HBLANK_FREE_LENGTH : OBJ_LENGTH;
		for (i = 0; i < GBA_VIDEO_VERTICAL_PIXELS; ++i) {
			d3dRenderer->spriteCycles[i] = spriteCyclesRemaining;
		}
	}

	if (GBARegisterDISPCNTGetMode(d3dRenderer->dispcnt) != 0) {
		if (d3dRenderer->bg[2].enabledAtY <= y) {
			d3dRenderer->bg[2].affine.sx += d3dRenderer->bg[2].affine.dmx;
			d3dRenderer->bg[2].affine.sy += d3dRenderer->bg[2].affine.dmy;
		}
		if (d3dRenderer->bg[3].enabledAtY <= y) {
			d3dRenderer->bg[3].affine.sx += d3dRenderer->bg[3].affine.dmx;
			d3dRenderer->bg[3].affine.sy += d3dRenderer->bg[3].affine.dmy;
		}
	}
}

static void _drawBackground(struct GBAVideoD3D9Renderer* renderer, int index, int y) {
	struct GBAVideoD3D9Background* bg = &renderer->bg[index];
	if (bg->enabledAtY > y) {
		return;
	}
	switch (GBARegisterDISPCNTGetMode(renderer->dispcnt)) {
	case 0:
		GBAVideoD3D9RendererDrawBackgroundMode0(renderer, bg, y);
		break;
	case 1:
		if (index < 2) {
			GBAVideoD3D9RendererDrawBackgroundMode0(renderer, bg, y);
		} else if (index == 2) {
			GBAVideoD3D9RendererDrawBackgroundMode2(renderer, bg, y);
		}
		break;
	case 2:
		if (index >= 2) {
			GBAVideoD3D9RendererDrawBackgroundMode2(renderer, bg, y);
		}
		break;
	case 3:
		if (index == 2) {
			GBAVideoD3D9RendererDrawBackgroundBitmap(renderer, bg, y, D3D9_PS_MODE35, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
		}
		break;
	case 4:
		if (index == 2) {
			GBAVideoD3D9RendererDrawBackgroundBitmap(renderer, bg, y, D3D9_PS_MODE4, GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt) ? 0xA000 : 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
		}
		break;
	case 5:
		if (index == 2) {
			GBAVideoD3D9RendererDrawBackgroundBitmap(renderer, bg, y, D3D9_PS_MODE35, GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt) ? 0x5000 : 0, 160, 128);
		}
		break;
	}
}

/* Draws scanlines firstY..y (on Xbox 360, all in the current band) */
static void _drawBatch(struct GBAVideoD3D9Renderer* renderer, int y) {
	int i, j;

	/* The "backdrop framebuffer" of gl.c */
	for (j = renderer->firstY; j <= y; ++j) {
		_setLine(renderer, j, GBA_D3D9_LD_BACKDROP, 32, (float) (renderer->target1Bd | (renderer->target2Bd * 2) | (renderer->blendEffect * 4)), renderer->blda, 0);
		for (i = 0; i < 4; ++i) {
			_setLine(renderer, j, GBA_D3D9_LD_BGFLAGS + i, (float) renderer->bg[i].priority,
			         (float) (renderer->bg[i].target1 | (renderer->bg[i].target2 << 1) | (renderer->blendEffect << 2)),
			         renderer->blda, 0);
		}
	}
	/* the rows these draws read: the batch, the scanline before it and the
	 * 3 after it (affine interpolation) */
	_uploadLineData(renderer, renderer->firstY - 3, y + 3);

	if (renderer->paletteDirty) {
		_uploadPalette(renderer, renderer->firstY, y);
	}

	GBAVideoD3D9RendererDrawWindow(renderer, y);
	if (GBARegisterDISPCNTIsObjEnable(renderer->dispcnt) && !renderer->d.disableOBJ) {
		for (i = 0; i < renderer->oamMax; ++i) {
			struct GBAVideoRendererSprite* sprite = &renderer->sprites[i];
			int startY, endY;
			if ((y < sprite->y && (sprite->endY - 256 < 0 || renderer->firstY >= sprite->endY - 256)) || renderer->firstY >= sprite->endY) {
				continue;
			}

			GBAVideoD3D9RendererDrawSprite(renderer, &sprite->obj, y, sprite->y);

			startY = sprite->y;
			endY = sprite->endY;

			if (endY >= 256) {
				startY -= 256;
				endY -= 256;
			}
			if (startY < renderer->firstY) {
				startY = renderer->firstY;
			}
			if (endY > y) {
				endY = y;
			}
			for (j = startY; j <= endY; ++j) {
				renderer->spriteCycles[j] -= sprite->cycles;
			}
		}
		IDirect3DDevice9_SetRenderState(renderer->dev, D3DRS_ZENABLE, D3DZB_FALSE);
		IDirect3DDevice9_SetRenderState(renderer->dev, D3DRS_ZWRITEENABLE, FALSE);
		IDirect3DDevice9_SetRenderState(renderer->dev, D3DRS_STENCILENABLE, FALSE);
	}


	for (i = 0; i < 4; ++i) {
		_drawBackground(renderer, i, y);
	}
}

static void _drawScanlines(struct GBAVideoD3D9Renderer* renderer, int y) {
	renderer->batchFirstY = renderer->firstY;
	renderer->batchLastY = y;
#ifdef D3D9_EDRAM
	/* one batch per band crossed */
	while (renderer->firstY <= y) {
		int last = (renderer->firstY / renderer->bandRows + 1) * renderer->bandRows - 1;
		if (last > y) {
			last = y;
		}
		_edramEnterBand(renderer, renderer->firstY);
		_drawBatch(renderer, last);
		renderer->firstY = last + 1;
	}
#else
	_drawBatch(renderer, y);
#endif
	renderer->firstY = -1;
}

static void GBAVideoD3D9RendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoD3D9Renderer* d3dRenderer = (struct GBAVideoD3D9Renderer*) renderer;
	_begin(d3dRenderer);
	if (d3dRenderer->firstY >= 0) {
		_drawScanlines(d3dRenderer, GBA_VIDEO_VERTICAL_PIXELS - 1);
	}
	_finalizeLayers(d3dRenderer);
	_end(d3dRenderer);
	d3dRenderer->firstAffine = -1;
	d3dRenderer->firstY = -1;
	d3dRenderer->lastY = -1;
	if (d3dRenderer->bg[0].enabledAtY < INT_MAX) {
		d3dRenderer->bg[0].enabledAtY = 0;
	}
	if (d3dRenderer->bg[1].enabledAtY < INT_MAX) {
		d3dRenderer->bg[1].enabledAtY = 0;
	}
	if (d3dRenderer->bg[2].enabledAtY < INT_MAX) {
		d3dRenderer->bg[2].enabledAtY = 0;
	}
	if (d3dRenderer->bg[3].enabledAtY < INT_MAX) {
		d3dRenderer->bg[3].enabledAtY = 0;
	}
	d3dRenderer->bg[2].affine.sx = d3dRenderer->bg[2].refx;
	d3dRenderer->bg[2].affine.sy = d3dRenderer->bg[2].refy;
	d3dRenderer->bg[3].affine.sx = d3dRenderer->bg[3].refx;
	d3dRenderer->bg[3].affine.sy = d3dRenderer->bg[3].refy;
	/* the BGs of the next frame: at 1x those that only had text this one */
	{
		int i;
		for (i = 0; i < 4; ++i) {
			d3dRenderer->bgLowres[i] = d3dRenderer->scale > 1 && !d3dRenderer->bgHires[i];
			d3dRenderer->bgHires[i] = false;
		}
	}
}

/* Slow (waits for the GPU); only for screenshots */
static void GBAVideoD3D9RendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct GBAVideoD3D9Renderer* d3dRenderer = (struct GBAVideoD3D9Renderer*) renderer;
	int width = _rtWidth(d3dRenderer);
	int height = _rtHeight(d3dRenderer);
	IDirect3DSurface9* sysmem = NULL;
	D3DLOCKED_RECT lock;
	int x, y;
	*stride = width;
	*pixels = NULL;
	if (!d3dRenderer->temporaryBuffer) {
		d3dRenderer->temporaryBuffer = calloc((size_t) width * height, sizeof(mColor));
		if (!d3dRenderer->temporaryBuffer) {
			return;
		}
	}
	*pixels = d3dRenderer->temporaryBuffer;
#ifndef _XBOX
	if (FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(d3dRenderer->dev, width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sysmem, NULL))) {
		return;
	}
	if (d3dRenderer->rtSurf[D3D9_RT_OUTPUT] && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(d3dRenderer->dev, d3dRenderer->rtSurf[D3D9_RT_OUTPUT], sysmem)) &&
	    SUCCEEDED(IDirect3DSurface9_LockRect(sysmem, &lock, NULL, D3DLOCK_READONLY))) {
		for (y = 0; y < height; ++y) {
			const uint32_t* src = (const uint32_t*) ((const uint8_t*) lock.pBits + lock.Pitch * y);
			mColor* dst = &d3dRenderer->temporaryBuffer[width * y];
			for (x = 0; x < width; ++x) {
				/* 0xAARRGGBB -> 0x00BBGGRR */
				uint32_t c = src[x];
				c = ((c >> 16) & 0xFF) | (c & 0xFF00) | ((c & 0xFF) << 16);
				dst[x] = M_RGB8_TO_NATIVE(c);
			}
		}
		IDirect3DSurface9_UnlockRect(sysmem);
	}
	SAFE_RELEASE(IDirect3DSurface9, sysmem);
#else
	UNUSED(sysmem);
	UNUSED(lock);
	UNUSED(x);
	UNUSED(y);
#endif
}

static void GBAVideoD3D9RendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels) {
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
}

static void _enableBg(struct GBAVideoD3D9Renderer* renderer, int bg, bool active) {
	int wasActive = renderer->bg[bg].enabledAtY;
	if (!active) {
		renderer->bg[bg].enabledAtY = INT_MAX;
	} else if (wasActive == INT_MAX && active) {
		if (renderer->lastY < 0) {
			renderer->bg[bg].enabledAtY = 0;
		} else if (GBARegisterDISPCNTGetMode(renderer->dispcnt) > 2) {
			renderer->bg[bg].enabledAtY = renderer->lastY + 2;
		} else {
			renderer->bg[bg].enabledAtY = renderer->lastY + 3;
		}
	}
}

static void GBAVideoD3D9RendererUpdateDISPCNT(struct GBAVideoD3D9Renderer* renderer) {
	_enableBg(renderer, 0, !!GBARegisterDISPCNTGetBg0Enable(renderer->dispcnt));
	_enableBg(renderer, 1, !!GBARegisterDISPCNTGetBg1Enable(renderer->dispcnt));
	_enableBg(renderer, 2, !!GBARegisterDISPCNTGetBg2Enable(renderer->dispcnt));
	_enableBg(renderer, 3, !!GBARegisterDISPCNTGetBg3Enable(renderer->dispcnt));
}

static void GBAVideoD3D9RendererWriteBGCNT(struct GBAVideoD3D9Background* bg, uint16_t value) {
	bg->priority = GBARegisterBGCNTGetPriority(value);
	bg->oldCharBase = bg->charBase;
	bg->charBase = GBARegisterBGCNTGetCharBase(value) << 13;
	bg->mosaic = GBARegisterBGCNTGetMosaic(value);
	bg->multipalette = GBARegisterBGCNTGet256Color(value);
	bg->screenBase = GBARegisterBGCNTGetScreenBase(value) << 10;
	bg->overflow = GBARegisterBGCNTGetOverflow(value);
	bg->size = GBARegisterBGCNTGetSize(value);
}

static void GBAVideoD3D9RendererWriteBGX_LO(struct GBAVideoD3D9Background* bg, uint16_t value) {
	bg->refx = (bg->refx & 0xFFFF0000) | value;
	bg->affine.sx = bg->refx;
}

static void GBAVideoD3D9RendererWriteBGX_HI(struct GBAVideoD3D9Background* bg, uint16_t value) {
	bg->refx = (bg->refx & 0x0000FFFF) | (value << 16);
	bg->refx <<= 4;
	bg->refx >>= 4;
	bg->affine.sx = bg->refx;
}

static void GBAVideoD3D9RendererWriteBGY_LO(struct GBAVideoD3D9Background* bg, uint16_t value) {
	bg->refy = (bg->refy & 0xFFFF0000) | value;
	bg->affine.sy = bg->refy;
}

static void GBAVideoD3D9RendererWriteBGY_HI(struct GBAVideoD3D9Background* bg, uint16_t value) {
	bg->refy = (bg->refy & 0x0000FFFF) | (value << 16);
	bg->refy <<= 4;
	bg->refy >>= 4;
	bg->affine.sy = bg->refy;
}

static void GBAVideoD3D9RendererWriteBLDCNT(struct GBAVideoD3D9Renderer* renderer, uint16_t value) {
	renderer->bg[0].target1 = GBARegisterBLDCNTGetTarget1Bg0(value);
	renderer->bg[1].target1 = GBARegisterBLDCNTGetTarget1Bg1(value);
	renderer->bg[2].target1 = GBARegisterBLDCNTGetTarget1Bg2(value);
	renderer->bg[3].target1 = GBARegisterBLDCNTGetTarget1Bg3(value);
	renderer->bg[0].target2 = GBARegisterBLDCNTGetTarget2Bg0(value);
	renderer->bg[1].target2 = GBARegisterBLDCNTGetTarget2Bg1(value);
	renderer->bg[2].target2 = GBARegisterBLDCNTGetTarget2Bg2(value);
	renderer->bg[3].target2 = GBARegisterBLDCNTGetTarget2Bg3(value);

	renderer->blendEffect = GBARegisterBLDCNTGetEffect(value);
	renderer->target1Obj = GBARegisterBLDCNTGetTarget1Obj(value);
	renderer->target1Bd = GBARegisterBLDCNTGetTarget1Bd(value);
	renderer->target2Obj = GBARegisterBLDCNTGetTarget2Obj(value);
	renderer->target2Bd = GBARegisterBLDCNTGetTarget2Bd(value);
}

static void _finalizeLayers(struct GBAVideoD3D9Renderer* renderer) {
	IDirect3DDevice9* dev = renderer->dev;
	int i;
	int band;
	int bands = 1;
	IDirect3DTexture9** layers = renderer->rtTex;
	int texRows = GBA_VIDEO_VERTICAL_PIXELS;
#ifdef D3D9_EDRAM
	_edramLeaveBand(renderer);
	/* the composition reads every row */
	_uploadLineData(renderer, 0, GBA_VIDEO_VERTICAL_PIXELS - 1);
	_uploadPalette(renderer, 0, GBA_VIDEO_VERTICAL_PIXELS - 1);
	_setFull(renderer);
	bands = _bandCount(renderer);
	texRows = renderer->bandRows;
#endif
	_setTargets(renderer, D3D9_RT_OUTPUT, -1);
	if (GBARegisterDISPCNTIsForcedBlank(renderer->dispcnt)) {
		_clear(renderer, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0xFF, 0xFF, 0xFF, 0xFF), 0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	} else {
		_setPS(renderer, D3D9_PS_FINALIZE);
		/* band by band on Xbox 360: each one has its own layer textures */
		for (band = 0; band < bands; ++band) {
			int top = band * texRows;
			int rows = GBA_VIDEO_VERTICAL_PIXELS - top;
			if (rows > texRows) {
				rows = texRows;
			}
#ifdef D3D9_EDRAM
			layers = renderer->bandTex[band];
#endif
			for (i = 0; i < 4; ++i) {
				IDirect3DTexture9* bg = layers[D3D9_RT_BG0 + i];
				if (renderer->bgLowres[i]) {
#ifdef D3D9_EDRAM
					bg = renderer->bandTexLow[band][i];
#else
					bg = renderer->rtTexLow[i];
#endif
				}
				/* (the shader reads them with the same coordinates: point
				 * sampling at 1x gives the same texel) */
				IDirect3DDevice9_SetTexture(dev, 3 + i, (IDirect3DBaseTexture9*) bg);
			}
			IDirect3DDevice9_SetTexture(dev, 7, (IDirect3DBaseTexture9*) layers[D3D9_RT_OBJ_COLOR]);
			IDirect3DDevice9_SetTexture(dev, 8, (IDirect3DBaseTexture9*) layers[D3D9_RT_OBJ_FLAGS]);
			IDirect3DDevice9_SetTexture(dev, 9, (IDirect3DBaseTexture9*) layers[D3D9_RT_WINDOW]);
			_psConst(renderer, PS_FIN, (float) renderer->scale, 1.0f / _rtWidth(renderer), 1.0f / (texRows * renderer->scale), (float) (top * renderer->scale));
			_scissor(renderer, 0, top, GBA_VIDEO_HORIZONTAL_PIXELS, top + rows);
			_drawQuad(renderer, 0, (float) top, GBA_VIDEO_HORIZONTAL_PIXELS, (float) rows, 0, (float) top, GBA_VIDEO_HORIZONTAL_PIXELS, (float) rows, 0);
		}
		for (i = 3; i < 10; ++i) {
			IDirect3DDevice9_SetTexture(dev, i, NULL);
		}
	}
#ifdef D3D9_EDRAM
	_edramResolve(renderer, 0, D3D9_RT_OUTPUT);
#endif
}

static void GBAVideoD3D9RendererDrawSprite(struct GBAVideoD3D9Renderer* renderer, struct GBAObj* sprite, int y, int spriteY) {
	IDirect3DDevice9* dev = renderer->dev;
	int width = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][0];
	int height = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][1];
	int32_t x = (uint32_t) GBAObjAttributesBGetX(sprite->b) << 23;
	int align;
	unsigned charBase;
	unsigned tile = 0;
	int stride;
	int totalWidth, totalHeight;
	bool objwin;
	bool semitransparent;
	float priority;
	float depth;
	float targets;
	int is256;
	int row, runStart, firstRow, lastRow;

	x >>= 23;
	x += renderer->objOffsetX;

	if (GBARegisterDISPCNTGetMode(renderer->dispcnt) >= 3 && GBAObjAttributesCGetTile(sprite->c) < 512) {
		return;
	}

	align = GBAObjAttributesAIs256Color(sprite->a) && !GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt);
	charBase = (BASE_TILE >> 1) + (GBAObjAttributesCGetTile(sprite->c) & ~align) * 0x10;
	if (!GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt)) {
		if (GBAObjAttributesAIs256Color(sprite->a)) {
			tile = (charBase >> 5) & 0xF;
		} else {
			tile = (charBase >> 4) & 0x1F;
		}
		charBase &= ~0x1FF;
	}
	stride = GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? (width >> 3) : (0x20 >> GBAObjAttributesAGet256Color(sprite->a));

	totalWidth = width;
	totalHeight = height;
	if (GBAObjAttributesAIsTransformed(sprite->a) && GBAObjAttributesAIsDoubleSize(sprite->a)) {
		totalWidth <<= 1;
		totalHeight <<= 1;
	}

	spriteY += renderer->objOffsetY;
	if (spriteY + totalHeight >= 256) {
		spriteY -= 256;
	}

	if (x + totalWidth <= 0 || x >= GBA_VIDEO_HORIZONTAL_PIXELS) {
		// These sprites aren't displayed but affect cycle counting
		return;
	}

	objwin = GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_OBJWIN;
	semitransparent = GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT;
	if (objwin && !GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt)) {
		return;
	}
	is256 = GBAObjAttributesAGet256Color(sprite->a) ? 1 : 0;
	priority = (float) GBAObjAttributesCGetPriority(sprite->c);
	depth = priority / 16.f;
	targets = (float) ((renderer->target1Obj || semitransparent) | (renderer->target2Obj * 2) | (renderer->blendEffect * 4));

	_psConst(renderer, PS_OBJ + 0, (float) charBase, (float) tile, (float) stride, (float) GBAObjAttributesCGetPalette(sprite->c));
	_psConst(renderer, PS_OBJ + 1, priority, targets, renderer->blda, semitransparent ? 1.f : 0.f);
	if (GBAObjAttributesAIsTransformed(sprite->a)) {
		struct GBAOAMMatrix mat;
		LOAD_16(mat.a, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].a);
		LOAD_16(mat.b, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].b);
		LOAD_16(mat.c, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].c);
		LOAD_16(mat.d, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].d);
		_psConst(renderer, PS_OBJ + 2, mat.a / 256.f, mat.b / 256.f, mat.c / 256.f, mat.d / 256.f);
	} else {
		_psConst(renderer, PS_OBJ + 2, GBAObjAttributesBIsHFlip(sprite->b) ? -1.f : 1.f, 0, 0, GBAObjAttributesBIsVFlip(sprite->b) ? -1.f : 1.f);
	}
	_psConst(renderer, PS_OBJ + 3, (float) width, (float) height, (float) totalWidth, (float) totalHeight);
	if (GBAObjAttributesAIsMosaic(sprite->a) && !objwin) {
		int mosaicH = GBAMosaicControlGetObjH(renderer->mosaic) + 1;
		int mosaicV = GBAMosaicControlGetObjV(renderer->mosaic) + 1;
		_psConst(renderer, PS_OBJ + 6, (float) _mosaicTable[mosaicH], (float) _mosaicTable[mosaicV], 0, 0);
		if (GBAObjAttributesBIsHFlip(sprite->b)) {
			mosaicH = -mosaicH;
		}
		_psConst(renderer, PS_OBJ + 5, (float) mosaicH, (float) mosaicV, (float) x, (float) spriteY);
	} else {
		_psConst(renderer, PS_OBJ + 5, 0, 0, (float) x, (float) spriteY);
		_psConst(renderer, PS_OBJ + 6, 0, 0, 0, 0);
	}

	if (objwin) {
		// OBJWIN writes do not affect pixel priority
		IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
		IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, D3DZB_FALSE);
		IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
		_psConst(renderer, PS_OBJ + 4, (float) (renderer->objwin & 0x3F), renderer->bldb, renderer->bldy, 0);
		_setTargets(renderer, D3D9_RT_WINDOW, -1);
		_setPS(renderer, is256 ? D3D9_PS_OBJWIN_256 : D3D9_PS_OBJWIN_16);
	} else {
		IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, TRUE);
		IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, D3DZB_TRUE);
		IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, TRUE);
		IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		_psConst(renderer, PS_OBJ + 4, 0, 0, 0, 0);
		_setTargets(renderer, D3D9_RT_OBJ_COLOR, D3D9_RT_OBJ_FLAGS);
		_setPS(renderer, is256 ? D3D9_PS_OBJ_256 : D3D9_PS_OBJ_16);
	}

	/* gl.c discards the pixels of scanlines whose sprite cycles ran out;
	 * here those scanlines are left out of the scissor */
	firstRow = renderer->firstY;
	if (firstRow < spriteY) {
		firstRow = spriteY;
	}
	if (firstRow < 0) {
		firstRow = 0;
	}
	lastRow = spriteY + totalHeight - 1;
	if (lastRow > y) {
		lastRow = y;
	}
	runStart = -1;
	for (row = firstRow; row <= lastRow + 1; ++row) {
		if (row <= lastRow && renderer->spriteCycles[row] > 0) {
			if (runStart < 0) {
				runStart = row;
			}
			continue;
		}
		if (runStart >= 0) {
			_scissor(renderer, x, runStart, x + totalWidth, row);
			_drawQuad(renderer, (float) x, (float) spriteY, (float) totalWidth, (float) totalHeight, 0, 0, (float) totalWidth, (float) totalHeight, depth);
			runStart = -1;
		}
	}

	if (!objwin) {
		// Update the pixel priority for already-written pixels
		IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFUNC, D3DCMP_EQUAL);
		_setTargets(renderer, D3D9_RT_OBJ_FLAGS, -1);
		_setPS(renderer, D3D9_PS_OBJ_PRIORITY);
		_scissor(renderer, x, renderer->firstY, x + totalWidth, y + 1);
		_drawQuad(renderer, (float) x, (float) spriteY, (float) totalWidth, (float) totalHeight, 0, 0, (float) totalWidth, (float) totalHeight, depth);
	}
}

static void _prepareBackground(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int shader) {
	_setTargets(renderer, D3D9_RT_BG0 + background->index, -1);
	_setPS(renderer, shader);
	if (background->mosaic) {
		int h = GBAMosaicControlGetBgH(renderer->mosaic) + 1;
		int v = GBAMosaicControlGetBgV(renderer->mosaic) + 1;
		_psConst(renderer, PS_BG + 1, (float) h, (float) v, (float) _mosaicTable[h], (float) _mosaicTable[v]);
	} else {
		_psConst(renderer, PS_BG + 1, 0, 0, 0, 0);
	}
}

static void _drawBackgroundRows(struct GBAVideoD3D9Renderer* renderer, int firstY, int y) {
	_scissor(renderer, 0, firstY, GBA_VIDEO_HORIZONTAL_PIXELS, y + 1);
	_drawQuad(renderer, 0, (float) firstY, GBA_VIDEO_HORIZONTAL_PIXELS, (float) (y - firstY + 1), 0, (float) firstY, GBA_VIDEO_HORIZONTAL_PIXELS, (float) (y - firstY + 1), 0);
}

static void GBAVideoD3D9RendererDrawBackgroundMode0(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y) {
	int firstY = renderer->firstY;
	if (firstY < background->enabledAtY) {
		firstY = background->enabledAtY;
	}
	_prepareBackground(renderer, background, background->multipalette ? D3D9_PS_MODE0_256 : D3D9_PS_MODE0_16);
	_psConst(renderer, PS_BG + 0, (float) background->screenBase, (float) background->charBase, (float) background->size, (float) (GBA_D3D9_LD_OFFSET + background->index));
	_drawBackgroundRows(renderer, firstY, y);
}

static int _prepareTransform(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y, int shader) {
	int firstY = renderer->firstY;
	if (firstY < background->enabledAtY) {
		firstY = background->enabledAtY;
	}
	/* affine/bitmap: drawn at the internal resolution from the next frame on */
	renderer->bgHires[background->index] = true;
	_prepareBackground(renderer, background, shader);
	_psConst(renderer, PS_BG + 2, (float) renderer->firstAffine, (float) y, (float) (GBA_D3D9_LD_AFFINE + background->index - 2), (float) (1 << background->size));
	return firstY;
}

static void GBAVideoD3D9RendererDrawBackgroundMode2(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y) {
	int firstY = _prepareTransform(renderer, background, y, D3D9_PS_MODE2);
	_psConst(renderer, PS_BG + 0, (float) background->screenBase, (float) background->charBase, (float) background->size, background->overflow ? 1.f : 0.f);
	_psConst(renderer, PS_BG + 3, (float) background->oldCharBase, (float) firstY, (float) (0x8000 << background->size), 1.f / (0x8000 << background->size));
	_drawBackgroundRows(renderer, firstY, y);
	background->oldCharBase = background->charBase;
}

static void GBAVideoD3D9RendererDrawBackgroundBitmap(struct GBAVideoD3D9Renderer* renderer, struct GBAVideoD3D9Background* background, int y, int shader, int charBase, int width, int height) {
	int firstY = _prepareTransform(renderer, background, y, shader);
	_psConst(renderer, PS_BG + 0, (float) charBase, (float) width, (float) height, 0);
	_drawBackgroundRows(renderer, firstY, y);
}

static void _detectCircle(struct GBAVideoD3D9Renderer* renderer, int y, int window) {
	int lastStart = 0;
	int lastEnd = 0;

	int startX = 0;
	int endX = 0;

	int firstY = -1;
	float centerX = -1;
	float centerY = -1;
	float radius = 0;
	bool invalid = false;

	int i;
	for (i = renderer->batchFirstY; i <= y; ++i) {
		int startY, endY, startDiff, endDiff;
		lastStart = startX;
		lastEnd = endX;
		startX = renderer->winNHistory[window][i * 4];
		endX = renderer->winNHistory[window][i * 4 + 1];
		startY = renderer->winNHistory[window][i * 4 + 2];
		endY = renderer->winNHistory[window][i * 4 + 3];

		if (startX == endX || i < startY || i >= endY) {
			if (firstY >= 0) {
				// The bottom edge of the circle
				centerY = (firstY + i) / 2.f;
				firstY = -1;
			}
			continue;
		}
		if (lastEnd - lastStart <= 0) {
			continue;
		}

		// The previous segment was non-zero
		if (startX >= GBA_VIDEO_HORIZONTAL_PIXELS) {
			invalid = true;
			break;
		}

		startDiff = lastStart - startX;
		endDiff = endX - lastEnd;
		// Make sure the slopes match, otherwise this isn't a circle
		if (startDiff - endDiff < -1 || startDiff - endDiff > 1) {
			invalid = true;
			break;
		}

		if (startX < lastStart) {
			centerX = (startX + endX) / 2.f;
			if (radius > 0) {
				// We found two separate shapes, which the interpolation can't handle
				invalid = true;
				break;
			}
		} else if (startX > lastStart && radius <= 0) {
			radius = (lastEnd - lastStart) / 2.f;
		}

		if (firstY < 0 && i - 1 >= startY && i - 1 < endY) {
			firstY = i - 1;
		}
	}

	if (radius <= 0) {
		invalid = true;
	}
	if (centerX < 0) {
		invalid = true;
	}
	if (centerY < 0) {
		invalid = true;
	}

	// Check validity
	for (i = renderer->batchFirstY; i <= y && !invalid; ++i) {
		int sX = renderer->winNHistory[window][i * 4];
		int eX = renderer->winNHistory[window][i * 4 + 1];
		int sY = renderer->winNHistory[window][i * 4 + 2];
		int eY = renderer->winNHistory[window][i * 4 + 3];

		bool xActive = sX < eX;
		bool yActive = i >= sY && i < eY;

		if (xActive && yActive) {
			float cosine, sine;
			// Real window would be active, make sure simulated window would too
			if (centerY - i > radius) {
				// y is above the radius
				invalid = true;
				break;
			}
			if (i - centerY > radius) {
				// y is below the radius
				invalid = true;
				break;
			}

			cosine = (float) fabs(i - centerY);
			sine = (float) sqrt(radius * radius - cosine * cosine);
			if (fabs(centerX - sine - sX) <= 1 && fabs(centerX + sine - eX) <= 1) {
				continue;
			}

			if (radius >= cosine + 1) {
				sine = (float) sqrt(radius * radius - (cosine + 1) * (cosine + 1));
				if (fabs(centerX - sine - sX) <= 1 && fabs(centerX + sine - eX) <= 1) {
					continue;
				}
			}
			// y is active on the wrong parts of the scanline
			invalid = true;
		} else if (centerY - i < radius && i - centerY < radius) {
			// Real window would be inactive, make sure simulated window would too
			invalid = true;
		}
	}

	renderer->circle[window][3] = 0;
	if (invalid) {
		renderer->circle[window][0] = 0;
		renderer->circle[window][1] = 0;
		renderer->circle[window][2] = 0;
	} else {
		renderer->circle[window][0] = centerX;
		renderer->circle[window][1] = centerY;
		renderer->circle[window][2] = radius - 0.499f;
	}
}

static void GBAVideoD3D9RendererDrawWindow(struct GBAVideoD3D9Renderer* renderer, int y) {
	_setTargets(renderer, D3D9_RT_WINDOW, -1);
	switch (renderer->dispcnt & 0xE000) {
	case 0x0000:
		// No windows are enabled
		_clear(renderer, D3DCLEAR_TARGET, _flagsColor(((renderer->dispcnt >> 8) & 0x1F) | 0x20, renderer->bldb, renderer->bldy, 0), 0, renderer->firstY, GBA_VIDEO_HORIZONTAL_PIXELS, y + 1);
		break;
	case 0x8000:
		// Only OBJWIN is enabled
		_clear(renderer, D3DCLEAR_TARGET, _flagsColor(renderer->winout, renderer->bldb, renderer->bldy, 0), 0, renderer->firstY, GBA_VIDEO_HORIZONTAL_PIXELS, y + 1);
		break;
	default:
		if (!renderer->ps[D3D9_PS_WINDOW]) {
			/* the shader didn't compile (Xbox 360): everything outside WIN0/WIN1 */
			_clear(renderer, D3DCLEAR_TARGET, _flagsColor(renderer->winout, renderer->bldb, renderer->bldy, 0), 0, renderer->firstY, GBA_VIDEO_HORIZONTAL_PIXELS, y + 1);
			break;
		}
		_setPS(renderer, D3D9_PS_WINDOW);
		_psConst(renderer, PS_WIN + 0, (renderer->dispcnt & 0x2000) ? 1.f : 0.f, (renderer->dispcnt & 0x4000) ? 1.f : 0.f, renderer->scale < 2 ? 1.f : 0.f, 0);
		_psConst(renderer, PS_WIN + 1, renderer->winN[0].control, renderer->winN[1].control, renderer->winout, 0);
		_psConst(renderer, PS_WIN + 2, renderer->bldb, renderer->bldy, 0, 0);
		/* over the whole batch, even when it's drawn in two bands */
		_detectCircle(renderer, renderer->batchLastY, 0);
		_detectCircle(renderer, renderer->batchLastY, 1);
		IDirect3DDevice9_SetPixelShaderConstantF(renderer->dev, PS_WIN + 3, renderer->circle[0], 2);
		_drawBackgroundRows(renderer, renderer->firstY, y);
		break;
	}
}

#endif
