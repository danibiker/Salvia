/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* HLSL (vs_3_0 / ps_3_0) version of the GLSL shaders in renderers/gl.c.
 * Included only by renderers/d3d9.c.
 *
 * Shader model 3 has no integers, so every int of the GLSL code is a float
 * holding an exact integer (exact up to 2^24, far more than any value used
 * here) and the bit operations are done with power-of-two divisions:
 *   x >> n        floor(x / 2^n)            (arithmetic shift, also for x < 0)
 *   x & (2^n - 1) band(x, 2^n)              (two's-complement result, x < 0 ok)
 *   (x >> n) & 1  bitAt(x, 2^n)
 *   int(f)        trunc(f)                  (GLSL truncates towards zero)
 * Divisions by powers of two are exact in binary floating point.
 *
 * Textures (all point-sampled with tex2Dlod at texel centres):
 *   s0 vram      A8L8 256x192: one 16-bit VRAM halfword per texel (L = low)
 *   s1 palette   R5G6B5 512x160: the palette as it was at each scanline
 *   s2 lineData  A32B32G32R32F 16x160: per-scanline values, columns LD_*
 *   s3..s6 the BG layers, s7 OBJ colour, s8 OBJ flags, s9 window (finalize)
 * Integer flags stored in A8R8G8B8 render targets are written as n/255 and
 * read back with round(v * 255). */

#define LD_STR2(X) #X
#define LD_STR(X) LD_STR2(X)

static const char* const _d3d9Shaders =
	"sampler2D vram : register(s0);\n"
	"sampler2D palette : register(s1);\n"
	"sampler2D lineData : register(s2);\n"
	"sampler2D layer0 : register(s3);\n"
	"sampler2D layer1 : register(s4);\n"
	"sampler2D layer2 : register(s5);\n"
	"sampler2D layer3 : register(s6);\n"
	"sampler2D objColor : register(s7);\n"
	"sampler2D objFlags : register(s8);\n"
	"sampler2D windowTex : register(s9);\n"

	/* ---- vertex shader ------------------------------------------------ */
	/* The quad is (0,0)-(1,1). vsRect is where it goes, in GBA pixels of the
	 * render target (240x160 whatever the scale); vsTex is the texCoord at
	 * its corners (GBA pixels too, like GL's texCoord). vsNdc.xy converts
	 * GBA pixels to clip space, vsNdc.zw is D3D9's half-pixel correction
	 * (zero on Xbox 360 with D3DRS_HALFPIXELOFFSET). vsDepth.x is the depth
	 * (OBJ priority / 16). */
	"float4 vsRect : register(c0);\n"
	"float4 vsTex : register(c1);\n"
	"float4 vsNdc : register(c2);\n"
	"float4 vsDepth : register(c3);\n"
	"struct VSOut { float4 pos : POSITION; float2 tc : TEXCOORD0; };\n"
	"VSOut vsMain(float4 p : POSITION) {\n"
	"	VSOut o;\n"
	"	float2 px = vsRect.xy + p.xy * vsRect.zw;\n"
	"	o.pos = float4(px.x * vsNdc.x - 1.0 + vsNdc.z, 1.0 - px.y * vsNdc.y + vsNdc.w, vsDepth.x, 1.0);\n"
	"	o.tc = vsTex.xy + p.xy * vsTex.zw;\n"
	"	return o;\n"
	"}\n"

	/* ---- helpers ------------------------------------------------------ */
	"float band(float x, float p) { return x - floor(x / p) * p; }\n"
	"float2 band2(float2 x, float2 p) { return x - floor(x / p) * p; }\n"
	"float bitAt(float x, float p) { return band(floor(x / p), 2.0); }\n"
	/* MOSAIC(LHS, RHS) of gl.c with the table entry passed in */
	"float mos(float x, float n, float t) { return floor(x * t / 4096.0) * n; }\n"
	/* VRAM halfword at a halfword address; objMask applies gl.c's
	 * VRAM_MASK (row & 191) used by sprites */
	"float vfetch(float address, float objMask) {\n"
	"	float row = floor(address / 256.0);\n"
	"	float col = address - row * 256.0;\n"
	"	row -= 64.0 * bitAt(row, 64.0) * objMask;\n"
	/* past the end of VRAM (BG char bases up to 0xDFE0): 0, like GL's
	 * out-of-range texelFetch on the drivers mGBA is tested with */
	"	if (row >= 192.0) { return 0.0; }\n"
	"	float4 t = tex2Dlod(vram, float4((col + 0.5) / 256.0, (row + 0.5) / 192.0, 0, 0));\n"
	"	return round(t.r * 255.0) + round(t.a * 255.0) * 256.0;\n"
	"}\n"
	"float4 pfetch(float entry, float row) {\n"
	"	return float4(tex2Dlod(palette, float4((entry + 0.5) / 512.0, (row + 0.5) / 160.0, 0, 0)).rgb, 1.0);\n"
	"}\n"
	"float4 ld(float col, float row) {\n"
	"	return tex2Dlod(lineData, float4((col + 0.5) / 16.0, (row + 0.5) / 160.0, 0, 0));\n"
	"}\n"
	"float4 rtFetch(sampler2D s, float2 uv) { return round(tex2Dlod(s, float4(uv, 0, 0)) * 255.0); }\n"

	/* renderTile() of gl.c: _renderTile16 / _renderTile256 */
	"#ifdef TILE256\n"
	"float renderTile(float charBase, float tile, float paletteId, float2 lc, float objMask) {\n"
	"	float address = charBase + tile * 32.0 + floor(lc.x / 2.0) + lc.y * 4.0;\n"
	"	float halfrow = vfetch(address, objMask);\n"
	"	float entry = band(lc.x, 2.0) > 0.5 ? floor(halfrow / 256.0) : band(halfrow, 256.0);\n"
	"	clip(entry - 0.5);\n"
	"	return entry;\n"
	"}\n"
	"float maskTile(float tile) { return band(tile, 16.0); }\n"
	"#else\n"
	"float renderTile(float charBase, float tile, float paletteId, float2 lc, float objMask) {\n"
	"	float address = charBase + tile * 16.0 + floor(lc.x / 4.0) + lc.y * 2.0;\n"
	"	float halfrow = vfetch(address, objMask);\n"
	/* halfrow >> (4 * (x & 3)), multiplying by exact powers of two (a
	 * division by a variable would go through an approximate rcp) */
	"	float n = band(lc.x, 4.0);\n"
	"	float m = n < 0.5 ? 1.0 : (n < 1.5 ? 0.0625 : (n < 2.5 ? 0.00390625 : 0.000244140625));\n"
	"	float entry = band(floor(halfrow * m), 16.0);\n"
	"	clip(entry - 0.5);\n"
	"	return paletteId * 16.0 + entry;\n"
	"}\n"
	"float maskTile(float tile) { return band(tile, 32.0); }\n"
	"#endif\n"

	/* ---- backgrounds -------------------------------------------------- */
	/* bgParams: mode 0  screenBase, charBase, size, lineData column of the offsets
	 *           mode 2  screenBase, charBase, size, overflow
	 *           mode 3-5 charBase, size.x, size.y, -
	 * bgMosaic: h, v, mosaicTable[h], mosaicTable[v] (h, v <= 1: off)
	 * bgAffine: range.x (first affine line), range.y, lineData column, 1 << size
	 * bgOldCharBase: oldCharBase, firstY, 0x8000 << size, 1 / (0x8000 << size)
 * (different variables never share a constant register) */
	"float4 bgParams : register(c4);\n"
	"float4 bgMosaic : register(c5);\n"
	"float4 bgAffine : register(c6);\n"
	"float4 bgOldCharBase : register(c7);\n"

	"float4 psMode0(float2 tc : TEXCOORD0) : COLOR0 {\n"
	"	float row = floor(tc.y);\n"
	"	float2 coord = floor(tc);\n"
	"	if (bgMosaic.x > 1.0) { coord.x = mos(coord.x, bgMosaic.x, bgMosaic.z); }\n"
	"	if (bgMosaic.y > 1.0) { coord.y = mos(coord.y, bgMosaic.y, bgMosaic.w); }\n"
	"	coord += ld(bgParams.w, row).xy;\n"
	"	float sizeX = band(bgParams.z, 2.0);\n"
	"	float sizeY = bitAt(bgParams.z, 2.0);\n"
	"	coord = band2(coord, float2(512.0, 512.0));\n"
	"	if (sizeX < 0.5) { coord.x = band(coord.x, 256.0); }\n"
	"	if (sizeY < 0.5) { coord.y = band(coord.y, 256.0); }\n"
	"	float2 wrap = floor(coord / 256.0);\n"
	"	coord -= wrap * 256.0;\n"
	"	coord.y += wrap.x * 256.0 + wrap.y * 256.0 * (sizeX + sizeY);\n"
	"	float map = vfetch(bgParams.x + floor(coord.x / 8.0) + floor(coord.y / 8.0) * 32.0, 0.0);\n"
	"	float2 lc = band2(coord, float2(8.0, 8.0));\n"
	"	if (bitAt(map, 1024.0) > 0.5) { lc.x = 7.0 - lc.x; }\n"
	"	if (bitAt(map, 2048.0) > 0.5) { lc.y = 7.0 - lc.y; }\n"
	"	float entry = renderTile(bgParams.y, band(map, 1024.0), band(floor(map / 4096.0), 16.0), lc, 0.0);\n"
	"	return pfetch(entry, row);\n"
	"}\n"

	/* _interpolate of gl.c */
	"float2 bezier(float2 a0, float2 a1, float2 a2, float2 a3, float x) {\n"
	"	float x1m = 1.0 - x;\n"
	"	return x1m * x1m * x1m * a0 + 3.0 * x1m * x1m * x * a1 + 3.0 * x1m * x * x * a2 + x * x * x * a3;\n"
	"}\n"
	"float2 affineInterpolate(float2 tc) {\n"
	"	float2 incoord = tc;\n"
	"	if (bgMosaic.x > 1.0) { incoord.x = mos(floor(incoord.x), bgMosaic.x, bgMosaic.z); }\n"
	"	if (bgMosaic.y > 1.0) { incoord.y = mos(floor(incoord.y), bgMosaic.y, bgMosaic.w); }\n"
	"	float iy = floor(incoord.y);\n"
	"	float start = max(bgAffine.x, iy - 3.0);\n"
	"	float4 t0 = ld(bgAffine.z, start);\n"
	"	float4 t1 = ld(bgAffine.z, start + 1.0);\n"
	"	float4 t2 = ld(bgAffine.z, start + 2.0);\n"
	"	float4 t3 = ld(bgAffine.z, start + 3.0);\n"
	"	float y = frac(incoord.y);\n"
	"	float s = 2.0 / 3.0;\n"
	"	if (iy - bgAffine.x < 4.0) {\n"
	"		y = incoord.y - bgAffine.x;\n"
	"		s -= 1.0;\n"
	"	}\n"
	"	float lin = s + y / 3.0;\n"
	"	float2 mixedTransform = bezier(t0.xy, t1.xy, t2.xy, t3.xy, lin);\n"
	"	float2 mixedOffset = bezier(t0.zw, t1.zw, t2.zw, t3.zw, lin);\n"
	"	return trunc(mixedTransform * incoord.x + mixedOffset);\n"
	"}\n"

	"float4 psMode2(float2 tc : TEXCOORD0) : COLOR0 {\n"
	"	float2 coord = affineInterpolate(tc);\n"
	"	float sizeAdjusted = bgOldCharBase.z;\n"
	"	if (bgParams.w > 0.5) {\n"
	"		coord -= floor(coord * bgOldCharBase.w) * sizeAdjusted;\n"
	"	} else if (coord.x < 0.0 || coord.y < 0.0 || coord.x >= sizeAdjusted || coord.y >= sizeAdjusted) {\n"
	"		clip(-1.0);\n"
	"	}\n"
	"	float map = floor(coord.x / 2048.0) + floor(coord.y / 2048.0) * 16.0 * bgAffine.w;\n"
	"	float twomaps = vfetch(bgParams.x + floor(map / 2.0), 0.0);\n"
	"	float tile = band(map, 2.0) > 0.5 ? floor(twomaps / 256.0) : band(twomaps, 256.0);\n"
	"	float newCharBase = bgParams.y;\n"
	"	if (newCharBase != bgOldCharBase.x) {\n"
	"		float y = floor(tc.y);\n"
	"		if (y == bgOldCharBase.y && floor(ld(bgAffine.z, y - 1.0).w / 2048.0) == floor(coord.y / 2048.0)) {\n"
	"			newCharBase = bgOldCharBase.x;\n"
	"		}\n"
	"	}\n"
	"	float address = newCharBase + tile * 32.0 + band(floor(coord.x / 512.0), 4.0) + band(floor(coord.y / 256.0), 8.0) * 4.0;\n"
	"	float halfrow = vfetch(address, 0.0);\n"
	"	float entry = bitAt(coord.x, 256.0) > 0.5 ? floor(halfrow / 256.0) : band(halfrow, 256.0);\n"
	"	clip(entry - 0.5);\n"
	"	return pfetch(entry, floor(tc.y));\n"
	"}\n"

	"float4 psMode35(float2 tc : TEXCOORD0) : COLOR0 {\n"
	"	float2 coord = affineInterpolate(tc);\n"
	"	if (coord.x < 0.0 || coord.x >= bgParams.y * 256.0 || coord.y < 0.0 || coord.y >= bgParams.z * 256.0) {\n"
	"		clip(-1.0);\n"
	"	}\n"
	"	float entry = vfetch(bgParams.x + floor(coord.x / 256.0) + floor(coord.y / 256.0) * bgParams.y, 0.0);\n"
	"	return float4(band(entry, 32.0), band(floor(entry / 32.0), 32.0), band(floor(entry / 1024.0), 32.0), 31.0) / 31.0;\n"
	"}\n"

	"float4 psMode4(float2 tc : TEXCOORD0) : COLOR0 {\n"
	"	float2 coord = affineInterpolate(tc);\n"
	"	if (coord.x < 0.0 || coord.x >= bgParams.y * 256.0 || coord.y < 0.0 || coord.y >= bgParams.z * 256.0) {\n"
	"		clip(-1.0);\n"
	"	}\n"
	"	float address = bgParams.x + floor(coord.x / 256.0) + floor(coord.y / 256.0) * bgParams.y;\n"
	"	float twoEntries = vfetch(floor(address / 2.0), 0.0);\n"
	"	float entry = band(address, 2.0) > 0.5 ? floor(twoEntries / 256.0) : band(twoEntries, 256.0);\n"
	"	clip(entry - 0.5);\n"
	"	return pfetch(entry, floor(tc.y));\n"
	"}\n"

	/* ---- sprites ------------------------------------------------------ */
	/* objParams: charBase, tile, stride, localPalette
	 * objInflags: priority, blend targets | effect << 2, blda, semitransparent
	 * objTransform: a, b, c, d (coord = [a b; c d] * v)
	 * objDims: width, height, totalWidth, totalHeight
	 * objWin: window flags, bldb, bldy, 0
	 * objMosaic: h (negative: hflip), v, x, y
	 * objMosaicTable: mosaicTable[|h|], mosaicTable[v]
	 * The "cyclesRemaining <= 0" test of gl.c is done with the scissor:
	 * the sprite is drawn once per run of scanlines that still had cycles. */
	"float4 objParams : register(c8);\n"
	"float4 objInflags : register(c9);\n"
	"float4 objTransform : register(c10);\n"
	"float4 objDims : register(c11);\n"
	"float4 objWin : register(c12);\n"
	"float4 objMosaic : register(c13);\n"
	"float4 objMosaicTable : register(c14);\n"
	"struct ObjOut { float4 color : COLOR0; float4 flags : COLOR1; };\n"
	"#ifdef OBJWIN\n"
	"float4 psObj(float2 tc : TEXCOORD0) : COLOR0 {\n"
	"#else\n"
	"ObjOut psObj(float2 tc : TEXCOORD0) {\n"
	"#endif\n"
	"	float2 incoord = tc;\n"
	"	if (objMosaic.x > 1.0) {\n"
	"		float x = floor(incoord.x);\n"
	"		x = mos(objMosaic.z + x, objMosaic.x, objMosaicTable.x) - objMosaic.z;\n"
	"		incoord.x = clamp(x, 0.0, objDims.z - 1.0);\n"
	"	} else if (objMosaic.x < -1.0) {\n"
	"		float x = objDims.z - floor(incoord.x) - 1.0;\n"
	"		x = objDims.z - mos(objMosaic.z + x, -objMosaic.x, objMosaicTable.x) + objMosaic.z - 1.0;\n"
	"		incoord.x = clamp(x, 0.0, objDims.z - 1.0);\n"
	"	}\n"
	"	if (objMosaic.y > 1.0) {\n"
	"		float y = floor(incoord.y);\n"
	"		y = mos(objMosaic.w + y, objMosaic.y, objMosaicTable.y) - objMosaic.w;\n"
	"		incoord.y = clamp(y, 0.0, objDims.w - 1.0);\n"
	"	}\n"
	"	float2 d = incoord - objDims.zw / 2.0;\n"
	"	float2 coord = trunc(float2(objTransform.x * d.x + objTransform.y * d.y, objTransform.z * d.x + objTransform.w * d.y) + objDims.xy / 2.0);\n"
	"	if (coord.x < 0.0 || coord.y < 0.0 || coord.x >= objDims.x || coord.y >= objDims.y) {\n"
	"		clip(-1.0);\n"
	"	}\n"
	"	float tile = maskTile(floor(coord.x / 8.0) + objParams.y) + floor(coord.y / 8.0) * objParams.z;\n"
	"	float entry = renderTile(objParams.x, tile, objParams.w, band2(coord, float2(8.0, 8.0)), 1.0);\n"
	"#ifdef OBJWIN\n"
	"	return objWin / 255.0;\n"
	"#else\n"
	"	ObjOut o;\n"
	"	o.color = pfetch(entry + 256.0, floor(tc.y) + objMosaic.w);\n"
	"	o.flags = objInflags / 255.0;\n"
	"	return o;\n"
	"#endif\n"
	"}\n"

	"float4 psObjPriority(float2 tc : TEXCOORD0) : COLOR0 {\n"
	"	return objInflags / 255.0;\n"
	"}\n"

	/* ---- windows ------------------------------------------------------ */
	/* winEnable: WIN0 on, WIN1 on, disableInterp
	 * winFlags: WININ low, WININ high, WINOUT
	 * winBlend: bldb, bldy
	 * winCircle0/1: centre x, y, radius (radius <= 0: no circle) */
	"float4 winEnable : register(c15);\n"
	"float4 winFlags : register(c16);\n"
	"float4 winBlend : register(c17);\n"
	"float4 winCircle0 : register(c18);\n"
	"float4 winCircle1 : register(c19);\n"
	/* The Xbox 360 microcode compiler crashes (X4800, "internal error") on
	 * this shader, so there are several ways of writing it and d3d9.c uses
	 * the first one that compiles:
	 *   (none)    gl.c's logic, branch-free (1.0 = true, 0.0 = false)
	 *   WINDOW_B  the same with step()/dot() instead of compares/distance()
	 *   WINDOW_C  no interpolation between scanlines and no circles: exact
	 *             GBA windows, only less smooth at high scales
	 *   WINDOW_D  WINDOW_C written inline, without helper functions
	 * crop: inside the window rectangle; a start past the end wraps around */
	"#if defined(WINDOW_D)\n"
	"float4 psWindow(float2 tc : TEXCOORD0) : COLOR0 {\n"
	"	float row = floor(tc.y);\n"
	"	float4 w0 = tex2Dlod(lineData, float4((" LD_STR(GBA_D3D9_LD_WIN0) " + 0.5) / 16.0, (row + 0.5) / 160.0, 0, 0));\n"
	"	float4 w1 = tex2Dlod(lineData, float4((" LD_STR(GBA_D3D9_LD_WIN1) " + 0.5) / 16.0, (row + 0.5) / 160.0, 0, 0));\n"
	"	float4 c0 = step(float4(w0.x, tc.x, w0.z, tc.y), float4(tc.x, w0.y, tc.y, w0.w));\n"
	"	float4 c1 = step(float4(w1.x, tc.x, w1.z, tc.y), float4(tc.x, w1.y, tc.y, w1.w));\n"
	/* c.x: x >= start, c.y: end >= x... see WINDOW_C for the readable form */
	"	float4 o0 = 1.0 - c0;\n"
	"	float4 o1 = 1.0 - c1;\n"
	"	o0.y = step(w0.y, tc.x);\n"
	"	o0.w = step(w0.w, tc.y);\n"
	"	o1.y = step(w1.y, tc.x);\n"
	"	o1.w = step(w1.w, tc.y);\n"
	"	float wv0 = step(w0.w + 0.5, w0.z);\n"
	"	float wh0 = step(w0.y + 0.5, w0.x);\n"
	"	float wv1 = step(w1.w + 0.5, w1.z);\n"
	"	float wh1 = step(w1.y + 0.5, w1.x);\n"
	"	float in0 = (1.0 - (wv0 * o0.z * o0.w + (1.0 - wv0) * max(o0.z, o0.w))) * (1.0 - (wh0 * o0.x * o0.y + (1.0 - wh0) * max(o0.x, o0.y)));\n"
	"	float in1 = (1.0 - (wv1 * o1.z * o1.w + (1.0 - wv1) * max(o1.z, o1.w))) * (1.0 - (wh1 * o1.x * o1.y + (1.0 - wh1) * max(o1.x, o1.y)));\n"
	"	in0 *= winEnable.x;\n"
	"	in1 *= (1.0 - in0) * winEnable.y;\n"
	"	return float4(in0 * winFlags.x + in1 * winFlags.y + (1.0 - in0 - in1) * winFlags.z, winBlend.x, winBlend.y, 0.0) / 255.0;\n"
	"}\n"
	"#else\n"
	"#if defined(WINDOW_B) || defined(WINDOW_C)\n"
	"float lt(float a, float b) { return 1.0 - step(b, a); }\n"
	"float ge(float a, float b) { return step(b, a); }\n"
	"float crop(float4 p, float2 tc) {\n"
	"	float cx = lt(tc.x, p.x);\n"
	"	float cy = ge(tc.x, p.y);\n"
	"	float cz = lt(tc.y, p.z);\n"
	"	float cw = ge(tc.y, p.w);\n"
	"	float wrapV = lt(p.w, p.z);\n"
	"	float wrapH = lt(p.y, p.x);\n"
	"	float vOut = wrapV * cz * cw + (1.0 - wrapV) * max(cz, cw);\n"
	"	float hOut = wrapH * cx * cy + (1.0 - wrapH) * max(cx, cy);\n"
	"	return (1.0 - vOut) * (1.0 - hOut);\n"
	"}\n"
	"#else\n"
	"float crop(float4 p, float2 tc) {\n"
	"	float cx = tc.x < p.x ? 1.0 : 0.0;\n"
	"	float cy = tc.x >= p.y ? 1.0 : 0.0;\n"
	"	float cz = tc.y < p.z ? 1.0 : 0.0;\n"
	"	float cw = tc.y >= p.w ? 1.0 : 0.0;\n"
	"	float vOut = p.z > p.w ? cz * cw : max(cz, cw);\n"
	"	float hOut = p.x > p.y ? cx * cy : max(cx, cy);\n"
	"	return (1.0 - vOut) * (1.0 - hOut);\n"
	"}\n"
	"#endif\n"
	"#if defined(WINDOW_C)\n"
	"float winTest(float4 circle, float4 top, float4 bottom, float2 tc) {\n"
	"	return crop(top, tc);\n"
	"}\n"
	"#elif defined(WINDOW_B)\n"
	"float winTest(float4 circle, float4 top, float4 bottom, float2 tc) {\n"
	"	float4 d = top - bottom;\n"
	"	float keepTop = max(winEnable.z, 1.0 - step(dot(d, d), 1600.0));\n"
	"	float2 mixed = bottom.xy + (top.xy - bottom.xy) * frac(tc.y);\n"
	"	float4 p = float4(top.xy * keepTop + mixed * (1.0 - keepTop), top.zw);\n"
	"	float2 c = circle.xy - tc;\n"
	"	float inCircle = step(dot(c, c), circle.z * circle.z);\n"
	"	float hasCircle = 1.0 - step(circle.z, 0.0);\n"
	"	return hasCircle * inCircle + (1.0 - hasCircle) * crop(p, tc);\n"
	"}\n"
	"#else\n"
	"float4 winInterpolate(float4 top, float4 bottom, float2 tc) {\n"
	"	float keepTop = max(winEnable.z, distance(top, bottom) > 40.0 ? 1.0 : 0.0);\n"
	"	float4 mixed = float4(lerp(bottom.xy, top.xy, frac(tc.y)), top.zw);\n"
	"	return lerp(mixed, top, keepTop);\n"
	"}\n"
	"float winTest(float4 circle, float4 top, float4 bottom, float2 tc) {\n"
	"	float inCircle = distance(circle.xy, tc) <= circle.z ? 1.0 : 0.0;\n"
	"	float inRect = crop(winInterpolate(top, bottom, tc), tc);\n"
	"	return circle.z > 0.0 ? inCircle : inRect;\n"
	"}\n"
	"#endif\n"
	"float4 psWindow(float2 tc : TEXCOORD0) : COLOR0 {\n"
	"	float top = floor(tc.y);\n"
	"	float bottom = max(top - 1.0, 0.0);\n"
	"	float in0 = winEnable.x * winTest(winCircle0, ld(" LD_STR(GBA_D3D9_LD_WIN0) ", top), ld(" LD_STR(GBA_D3D9_LD_WIN0) ", bottom), tc);\n"
	"	float in1 = (1.0 - in0) * winEnable.y * winTest(winCircle1, ld(" LD_STR(GBA_D3D9_LD_WIN1) ", top), ld(" LD_STR(GBA_D3D9_LD_WIN1) ", bottom), tc);\n"
	"	float flags = in0 * winFlags.x + in1 * winFlags.y + (1.0 - in0 - in1) * winFlags.z;\n"
	"	return float4(flags, winBlend.x, winBlend.y, 0.0) / 255.0;\n"
	"}\n"
	"#endif\n"

	/* ---- composition -------------------------------------------------- */
	/* finScale: scale, 1 / width and 1 / height of the layer textures (in
	 * pixels), first row of the layer textures (in pixels: on Xbox 360 each
	 * band of scanlines has its own) */
	"float4 finScale : register(c20);\n"
	"void composite(float4 pixel, float4 flags, inout float4 topPixel, inout float4 topFlags, inout float4 bottomPixel, inout float4 bottomFlags) {\n"
	"	if (flags.x >= topFlags.x) {\n"
	"		if (flags.x < bottomFlags.x) {\n"
	"			bottomFlags = flags;\n"
	"			bottomPixel = pixel;\n"
	"		}\n"
	"	} else {\n"
	"		bottomFlags = topFlags;\n"
	"		topFlags = flags;\n"
	"		bottomPixel = topPixel;\n"
	"		topPixel = pixel;\n"
	"	}\n"
	"}\n"
	"float4 psFinalize(float2 tc : TEXCOORD0) : COLOR0 {\n"
	"	float row = floor(tc.y);\n"
	"	float4 topPixel = pfetch(0.0, row);\n"
	"	float4 bottomPixel = topPixel;\n"
	"	float4 topFlags = round(ld(" LD_STR(GBA_D3D9_LD_BACKDROP) ", row));\n"
	"	float4 bottomFlags = topFlags;\n"
	"	float2 uv = (floor(tc * finScale.x) - float2(0.0, finScale.w) + 0.5) * finScale.yz;\n"
	"	float4 windowFlags = rtFetch(windowTex, uv);\n"
	"	float layerWindow = windowFlags.x;\n"
	"	float4 pix;\n"
	"	if (bitAt(layerWindow, 16.0) > 0.5) {\n"
	"		pix = tex2Dlod(objColor, float4(uv, 0, 0));\n"
	"		if (pix.a != 0.0) { composite(pix, rtFetch(objFlags, uv), topPixel, topFlags, bottomPixel, bottomFlags); }\n"
	"	}\n"
	"	if (bitAt(layerWindow, 1.0) > 0.5) {\n"
	"		pix = tex2Dlod(layer0, float4(uv, 0, 0));\n"
	"		if (pix.a != 0.0) { composite(pix, round(ld(" LD_STR(GBA_D3D9_LD_BGFLAGS) ", row)), topPixel, topFlags, bottomPixel, bottomFlags); }\n"
	"	}\n"
	"	if (bitAt(layerWindow, 2.0) > 0.5) {\n"
	"		pix = tex2Dlod(layer1, float4(uv, 0, 0));\n"
	"		if (pix.a != 0.0) { composite(pix, round(ld(" LD_STR(GBA_D3D9_LD_BGFLAGS) " + 1.0, row)), topPixel, topFlags, bottomPixel, bottomFlags); }\n"
	"	}\n"
	"	if (bitAt(layerWindow, 4.0) > 0.5) {\n"
	"		pix = tex2Dlod(layer2, float4(uv, 0, 0));\n"
	"		if (pix.a != 0.0) { composite(pix, round(ld(" LD_STR(GBA_D3D9_LD_BGFLAGS) " + 2.0, row)), topPixel, topFlags, bottomPixel, bottomFlags); }\n"
	"	}\n"
	"	if (bitAt(layerWindow, 8.0) > 0.5) {\n"
	"		pix = tex2Dlod(layer3, float4(uv, 0, 0));\n"
	"		if (pix.a != 0.0) { composite(pix, round(ld(" LD_STR(GBA_D3D9_LD_BGFLAGS) " + 3.0, row)), topPixel, topFlags, bottomPixel, bottomFlags); }\n"
	"	}\n"
	"	if (bitAt(layerWindow, 32.0) < 0.5) {\n"
	"		topFlags.y -= band(topFlags.y, 2.0);\n"
	"	}\n"
	/* (flags.y & 13): bit 0 = 1st target, bits 2-3 = effect */
	"	float t1 = band(topFlags.y, 2.0);\n"
	"	float effect = floor(topFlags.y / 4.0);\n"
	"	if (((t1 > 0.5 && effect == 1.0) || topFlags.w > 0.0) && bitAt(bottomFlags.y, 2.0) > 0.5) {\n"
	"		topPixel.rgb *= topFlags.z / 16.0;\n"
	"		topPixel.rgb += bottomPixel.rgb * windowFlags.y / 16.0;\n"
	"	} else if (topFlags.w == 0.0 && t1 > 0.5) {\n"
	"		if (effect == 2.0) {\n"
	"			topPixel.rgb += (1.0 - topPixel.rgb) * windowFlags.z / 16.0;\n"
	"		} else if (effect == 3.0) {\n"
	"			topPixel.rgb -= topPixel.rgb * windowFlags.z / 16.0;\n"
	"		}\n"
	"	}\n"
	"	return float4(topPixel.rgb, 1.0);\n"
	"}\n";
