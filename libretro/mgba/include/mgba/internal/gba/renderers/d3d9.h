/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_D3D9_H
#define VIDEO_D3D9_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef BUILD_D3D9

struct mCore;
struct GBAVideoRenderer;

/* Direct3D 9 port of the OpenGL renderer (renderers/gl.c), for frontends
 * that hand the core their IDirect3DDevice9 (Salvia). Same algorithm and the
 * same shaders, rewritten in HLSL for vs_3_0/ps_3_0: the only integer-free
 * shader model D3D9 has, so all bit twiddling is done with exact float math.
 *
 * The renderer draws into its own texture (A8R8G8B8, 240*scale x 160*scale,
 * scanline 0 at the top; a render target on PC, a Resolve target on Xbox
 * 360) and leaves every device
 * state, render target and depth surface as it found them at the end of
 * each frame.
 *
 * Everything here must be called from the thread that owns the device. */
struct GBAVideoD3D9Renderer;

/* Creates all the D3D resources and compiles the shaders. Returns NULL if
 * the device can't run it (needs ps_3_0 and 2 simultaneous render
 * targets) or if anything fails; the caller then keeps the software
 * renderer. `device` is an IDirect3DDevice9*. Compiled shaders are cached in
 * `shaderCacheDir` (mgba_d3d9_*.bin) when it isn't NULL. On Xbox 360 the
 * frame is drawn in horizontal bands when the layers don't fit in the EDRAM
 * at once. */
struct GBAVideoD3D9Renderer* GBAVideoD3D9RendererCreate(void* device, int scale, const char* shaderCacheDir);
void GBAVideoD3D9RendererDestroy(struct GBAVideoD3D9Renderer* renderer);

/* Why the last Create (or a SetScale) failed, one reason per line ("" if
 * nothing failed) */
const char* GBAVideoD3D9RendererGetLastError(void);

struct GBAVideoRenderer* GBAVideoD3D9RendererGetBase(struct GBAVideoD3D9Renderer* renderer);

/* 1..8. Recreates the render targets; call it between frames. Returns
 * false if nothing could be created at that scale (the renderer then keeps
 * a lower one, see GetScale). */
bool GBAVideoD3D9RendererSetScale(struct GBAVideoD3D9Renderer* renderer, int scale);
int GBAVideoD3D9RendererGetScale(const struct GBAVideoD3D9Renderer* renderer);

/* The output texture (IDirect3DTexture9*: a render target on PC, a plain
 * tiled texture written by Resolve on Xbox 360). It
 * changes when the scale does. */
void* GBAVideoD3D9RendererGetTexture(struct GBAVideoD3D9Renderer* renderer, unsigned* width, unsigned* height);

/* Hooks the renderer into a GBA core (src/gba/core.c). With a renderer set
 * and the "hwaccelVideo" config value true, the core uses it instead of the
 * software renderer after the next reset or reloadConfigOption("hwaccelVideo").
 * "videoScale" sets the scale. Pass NULL to unhook it (before destroying it). */
void GBACoreSetD3D9Renderer(struct mCore* core, struct GBAVideoD3D9Renderer* renderer);
/* True when the core is currently drawing with it. */
bool GBACoreIsD3D9RendererActive(const struct mCore* core);

#endif

CXX_GUARD_END

#endif
