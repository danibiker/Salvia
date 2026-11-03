// Included by pgm_draw.cpp. Each worker owns destination rows and a small
// sprite decode buffer. Emulated RAM and the prepared sprite list are read-only
// until all workers have finished the frame.
#include "render_worker.h"

static BurnRenderPool pgm_workers;
static bool pgm_parallel_enabled;
static UINT32 *pgm_mask_prefix;

struct PgmRenderSprite {
	INT32 x, y, width, height, color, flip, priority;
	UINT32 mask, pixels, xzoom, yzoom;
	INT32 xgrow, ygrow;
	UINT8 ycount[33];
};
static PgmRenderSprite pgm_render_sprites[256];
static INT32 pgm_render_count;

struct BURN_RENDER_ALIGN PgmRenderContext {
	UINT16 line[1024];
};
static PgmRenderContext pgm_render_contexts[3];

// Prefix sums cover 64 mask bytes each. They let a worker jump directly to
// its first sprite row without decoding all the preceding invisible rows.
static UINT32 pgm_mask_pixel_count(UINT32 offset, UINT32 bytes)
{
	UINT32 total = 0;
	const UINT32 mask = nPGMSPRMaskMaskLen;
	offset &= mask;
	while (bytes && (offset & 63)) {
		total += sprmsktab[PGMSPRMaskROM[offset] ^ 0xff];
		offset = (offset + 1) & mask;
		bytes--;
	}
	while (bytes >= 64) {
		UINT32 blocks = bytes >> 6;
		const UINT32 available = (mask + 1 - offset) >> 6;
		if (blocks > available) blocks = available;
		total += pgm_mask_prefix[(offset >> 6) + blocks] - pgm_mask_prefix[offset >> 6];
		offset = (offset + (blocks << 6)) & mask;
		bytes -= blocks << 6;
	}
	while (bytes--) {
		total += sprmsktab[PGMSPRMaskROM[offset] ^ 0xff];
		offset = (offset + 1) & mask;
	}
	return total;
}

static INT32 pgm_sprite_y(const PgmRenderSprite &s, INT32 row)
{
	if (!s.yzoom) return s.y + row;
	const INT32 count = (row >> 5) * s.ycount[32] + s.ycount[row & 31];
	return s.y + row + (s.ygrow ? count : -count);
}

static void pgm_prepare_render_sprites()
{
	UINT16 *source = PGMSprBuf;
	UINT16 *finish = source + (OldCodeMode ? 0xa00 : 0x1000) / 2;
	UINT16 *zoomtable = OldCodeMode ? &PGMVidReg[0x1000 / 2] : PGMZoomRAM;
	pgm_render_count = 0;
	while (source < finish) {
		const UINT16 size = BURN_ENDIAN_SWAP_INT16(source[4]);
		if (OldCodeMode ? size == 0 : (size & 0x7fff) == 0) break;
		const UINT16 sx = BURN_ENDIAN_SWAP_INT16(source[0]);
		const UINT16 sy = BURN_ENDIAN_SWAP_INT16(source[1]);
		const UINT16 attr = BURN_ENDIAN_SWAP_INT16(source[2]);
		PgmRenderSprite &s = pgm_render_sprites[pgm_render_count];
		s.x = sx & 0x7ff; if (s.x > 0x3ff) s.x -= 0x800;
		s.y = sy & 0x3ff; if (s.y > 0x1ff) s.y -= 0x400;
		s.width = (size & 0x7e00) >> 5;
		s.height = size & 0x1ff;
		s.color = (attr & 0x1f00) >> 3;
		s.flip = (attr >> 13) & 3;
		s.priority = (attr >> 7) & 1;
		s.xgrow = sx >> 15; s.ygrow = sy >> 15;
		INT32 xz = (sx >> 11) & 15, yz = (sy >> 11) & 15;
		if (s.xgrow) xz = 16 - xz;
		if (s.ygrow) yz = 16 - yz;
		s.xzoom = xz & 16 ? 0 : ((UINT32)(OldCodeMode ? BURN_ENDIAN_SWAP_INT16(zoomtable[xz * 2]) : zoomtable[xz * 2]) << 16)
			| (OldCodeMode ? BURN_ENDIAN_SWAP_INT16(zoomtable[xz * 2 + 1]) : zoomtable[xz * 2 + 1]);
		s.yzoom = yz & 16 ? 0 : ((UINT32)(OldCodeMode ? BURN_ENDIAN_SWAP_INT16(zoomtable[yz * 2]) : zoomtable[yz * 2]) << 16)
			| (OldCodeMode ? BURN_ENDIAN_SWAP_INT16(zoomtable[yz * 2 + 1]) : zoomtable[yz * 2 + 1]);
		s.ycount[0] = 0;
		if (s.yzoom) for (INT32 bit = 0; bit < 32; bit++) s.ycount[bit + 1] = s.ycount[bit] + ((s.yzoom >> bit) & 1);
		UINT32 offset = ((attr & 0x7f) << 16) | BURN_ENDIAN_SWAP_INT16(source[3]);
		if ((nPGMSpriteBufferHack || OldCodeMode) && (attr & 0x8000)) offset += 0x800000;
		offset *= 2;
		const UINT32 mask = nPGMSPRMaskMaskLen;
		s.pixels = ((PGMSPRMaskROM[offset & mask] | (PGMSPRMaskROM[(offset + 1) & mask] << 8)
			| (PGMSPRMaskROM[(offset + 2) & mask] << 16) | ((UINT32)PGMSPRMaskROM[(offset + 3) & mask] << 24)) >> 2) * 3;
		s.mask = offset + 4;
		// A growing pixel can be duplicated at most once.
		if (s.width && s.height && s.x < nScreenWidth && s.x + s.width * (s.xgrow ? 2 : 1) > 0
			&& s.y < nScreenHeight && pgm_sprite_y(s, s.height) > 0) pgm_render_count++;
		source += OldCodeMode ? 5 : 8;
	}
}

static void pgm_draw_render_sprite(const PgmRenderSprite &s, INT32 top, INT32 bottom, UINT16 *line)
{
	if (s.y >= bottom || pgm_sprite_y(s, s.height) <= top) return;
	// Find the source rows that contribute to this band. The zoom mapping is
	// monotonic, including rows which are removed by shrinking.
	INT32 first = top > s.y ? top - s.y : 0;
	INT32 last = bottom - s.y < s.height ? bottom - s.y : s.height;
	if (s.yzoom) {
		INT32 lo = 0, hi = s.height;
		while (lo < hi) {
			const INT32 mid = (lo + hi) / 2;
			if (pgm_sprite_y(s, mid + 1) <= top) lo = mid + 1; else hi = mid;
		}
		first = lo;
		hi = s.height;
		while (lo < hi) {
			const INT32 mid = (lo + hi) / 2;
			if (pgm_sprite_y(s, mid) < bottom) lo = mid + 1; else hi = mid;
		}
		last = lo;
	}
	const INT32 start = s.flip & 2 ? s.height - last : first;
	const INT32 end = s.flip & 2 ? s.height - first : last;
	const INT32 stride = s.width / 8;
	UINT32 maskOffset = s.mask + start * stride;
	UINT32 pixelOffset = s.pixels + pgm_mask_pixel_count(s.mask, start * stride);
	sprite_draw_nozoom_function *draw = nozoom_draw_table[s.flip & 1];

	for (INT32 row = start; row < end; row++) {
		const INT32 logical = s.flip & 2 ? s.height - 1 - row : row;
		const INT32 y = pgm_sprite_y(s, logical);
		const INT32 next = pgm_sprite_y(s, logical + 1);
		if (y == next) {
			pixelOffset += pgm_mask_pixel_count(maskOffset, stride);
			maskOffset += stride;
			continue;
		}
		if (s.xzoom == 0 && s.yzoom == 0) {
			UINT16 *dst = pTempScreen + y * nScreenWidth;
			UINT8 *pri = SpritePrio + y * nScreenWidth;
			for (INT32 x = 0; x < s.width; x += 8, maskOffset++) {
				const UINT8 mask = PGMSPRMaskROM[maskOffset & nPGMSPRMaskMaskLen];
				const INT32 left = s.x + (s.flip & 1 ? s.width - 8 - x : x);
				if (left + 8 <= 0 || left >= nScreenWidth) {
					pixelOffset += sprmsktab[mask ^ 0xff];
					continue;
				}
				UINT8 *pixels = PGMSPRColROM + (pixelOffset & nPGMSPRColMaskLen);
				if (left >= 0 && left + 8 <= nScreenWidth) {
					pixelOffset += draw[mask](dst + left, pri + left, pixels, s.color, s.priority);
				} else {
					for (INT32 bit = 0; bit < 8; bit++) {
						if (mask & (1 << bit)) continue;
						const INT32 dx = left + (s.flip & 1 ? 7 - bit : bit);
						if (dx >= 0 && dx < nScreenWidth) {
							dst[dx] = *pixels | s.color;
							pri[dx] = s.priority;
						}
						pixels++; pixelOffset++;
					}
				}
			}
		} else {
			// Decode only a visible source row. The old path decoded the whole
			// sprite, including rows hidden offscreen, before applying zoom.
			for (INT32 x = 0; x < s.width; x += 8, maskOffset++) {
				pixelOffset += zoom_draw_table[PGMSPRMaskROM[maskOffset & nPGMSPRMaskMaskLen]]
					(line + x, PGMSPRColROM + (pixelOffset & nPGMSPRColMaskLen), s.color);
			}
			for (INT32 dy = y < top ? top : y; dy < next && dy < bottom; dy++) {
				draw_sprite_line(s.width / 16, pTempScreen + dy * nScreenWidth, SpritePrio + dy * nScreenWidth,
					s.xzoom, s.xgrow, 0, s.flip, s.x, s.priority, line);
			}
		}
	}
}

static void pgm_draw_render_layer(bool text, INT32 top, INT32 bottom)
{
	const INT32 size = text ? 8 : 32, shift = text ? 3 : 5;
	const INT32 xmask = text ? 0x1ff : 0x7ff, ymask = text ? 0xff : 0x1ff;
	UINT16 *vram = (UINT16 *)(text ? PGMTxtRAM : PGMBgRAM);
	const INT32 scrollX = OldCodeMode ? (INT16)BURN_ENDIAN_SWAP_INT16(PGMVidReg[(text ? 0x6000 : 0x3000) / 2])
		: text ? pgm_fg_scrollx : pgm_bg_scrollx;
	const INT32 scrollY = OldCodeMode ? (INT16)BURN_ENDIAN_SWAP_INT16(PGMVidReg[(text ? 0x5000 : 0x2000) / 2])
		: text ? pgm_fg_scrolly : pgm_bg_scrolly;
	bool rowscroll = false;
	if (!text) for (INT32 y = 1; y < nScreenHeight; y++) if (PGMRowRAM[y] != PGMRowRAM[0]) { rowscroll = true; break; }
	for (INT32 y = top; y < bottom;) {
		const INT32 sy = (scrollY + y) & ymask;
		INT32 height = rowscroll ? 1 : size - (sy & (size - 1));
		if (height > bottom - y) height = bottom - y;
		const INT32 sx = (scrollX + (text ? 0 : BURN_ENDIAN_SWAP_INT16(PGMRowRAM[rowscroll ? y : 0]))) & xmask;
		for (INT32 x = 0; x < nScreenWidth;) {
			const INT32 sourceX = (sx + x) & xmask;
			INT32 width = size - (sourceX & (size - 1));
			if (width > nScreenWidth - x) width = nScreenWidth - x;
			const INT32 offset = ((sy >> shift) * 64 + (sourceX >> shift)) * 2;
			const INT32 code = BURN_ENDIAN_SWAP_INT16(vram[offset]);
			const INT32 transparent = text ? texttrans[code] : code < nTileMask ? tiletrans[code] : 0;
			if (transparent) {
				const INT32 attr = BURN_ENDIAN_SWAP_INT16(vram[offset + 1]);
				const INT32 color = text ? ((attr & 0x3e) << 3) | 0x800 : ((attr & 0x3e) << 4) | 0x400;
				const INT32 flip = (attr & 0x40 ? size - 1 : 0) | (attr & 0x80 ? (size - 1) << shift : 0);
				const UINT8 *gfx = (text ? PGMTileROM : PGMTileROMExp) + (code << (shift * 2));
				UINT16 *dst = pTransDraw + y * nScreenWidth + x;
				for (INT32 dy = 0; dy < height; dy++, dst += nScreenWidth) {
					const INT32 row = (((sy & (size - 1)) + dy) << shift) + (sourceX & (size - 1));
					for (INT32 dx = 0; dx < width; dx++) {
						const UINT8 pen = gfx[(row + dx) ^ flip];
						if ((transparent & 2) || pen != (text ? 15 : 31)) dst[dx] = color | pen;
					}
				}
			}
			x += width;
		}
		y += height;
	}
}

static void pgm_draw_band(INT32 top, INT32 bottom, INT32 index)
{
	const INT32 first = top * nScreenWidth, last = bottom * nScreenWidth;
	memset(SpritePrio + first, 0xff, last - first);
	for (INT32 i = 0; i < pgm_render_count; i++) pgm_draw_render_sprite(pgm_render_sprites[i], top, bottom, pgm_render_contexts[index].line);
	const UINT16 backdrop = OldCodeMode ? 0x900 : 0x1000;
	for (INT32 i = first; i < last; i++) {
		pTransDraw[i] = (nSpriteEnable & 1) && SpritePrio[i] == 1 ? pTempScreen[i] : backdrop;
	}
	if (nBurnLayer & 1) pgm_draw_render_layer(false, top, bottom);
	if (nBurnLayer & 2) {
		if (OldCodeMode || !(pgm_video_control & 0x2000)) {
			for (INT32 i = first; i < last; i++) if (SpritePrio[i] == 0) pTransDraw[i] = pTempScreen[i];
		}
		if (OldCodeMode || !(pgm_video_control & 0x0800)) pgm_draw_render_layer(true, top, bottom);
	}
	// Palette conversion is part of each job, with the same pitch and byte
	// order as BurnTransferCopy. The workers never call shared tile helpers.
	for (INT32 y = top; y < bottom; y++) {
		const UINT16 *src = pTransDraw + y * nScreenWidth;
		UINT8 *dst = pBurnDraw + y * nBurnPitch;
		switch (nBurnBpp) {
			case 2: for (INT32 x = 0; x < nScreenWidth; x++) ((UINT16 *)dst)[x] = RamCurPal[src[x]]; break;
			case 4: for (INT32 x = 0; x < nScreenWidth; x++) ((UINT32 *)dst)[x] = RamCurPal[src[x]]; break;
			case 3: for (INT32 x = 0; x < nScreenWidth; x++) {
				const UINT32 color = RamCurPal[src[x]];
				dst[x * 3] = color; dst[x * 3 + 1] = color >> 8; dst[x * 3 + 2] = color >> 16;
			} break;
		}
	}
}

static void pgm_render_init()
{
	const char *name = BurnDrvGetTextA(DRV_NAME);
	pgm_parallel_enabled = !strncmp(name, "kov2", 4) || !strncmp(name, "ddp2", 4) || !strncmp(name, "ddp3", 4);
	if (!pgm_parallel_enabled) return;
	const UINT32 bytes = nPGMSPRMaskMaskLen + 1;
	pgm_mask_prefix = (UINT32 *)BurnMalloc(((bytes >> 6) + 1) * sizeof(UINT32));
	if (!pgm_mask_prefix) { pgm_parallel_enabled = false; return; }
	UINT32 total = 0;
	for (UINT32 offset = 0; offset < bytes; offset++) {
		if (!(offset & 63)) pgm_mask_prefix[offset >> 6] = total;
		total += sprmsktab[PGMSPRMaskROM[offset] ^ 0xff];
	}
	pgm_mask_prefix[bytes >> 6] = total;
	pgm_workers.init(pgm_draw_band);
}

static void pgm_render_exit()
{
	pgm_workers.exit();
	BurnFree(pgm_mask_prefix);
	pgm_parallel_enabled = false;
}

static bool pgm_render_frame()
{
	if (!pgm_parallel_enabled || enable_blending || nScreenHeight != 224) return false;
#if defined(DUMP_SPRITE_BITMAPS) || defined(DRAW_SPRITE_NUMBER)
	return false;
#endif
	INT32 left, right, top, bottom;
	GenericTilesGetClip(&left, &right, &top, &bottom);
	if (left || top || right != nScreenWidth || bottom != nScreenHeight) return false;
	pgm_prepare_render_sprites();
	pBurnDrvPalette = RamCurPal;
	pgm_workers.render(nScreenHeight);
	return true;
}
