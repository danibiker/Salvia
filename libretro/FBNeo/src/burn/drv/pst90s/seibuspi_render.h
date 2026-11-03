// Included by d_seibuspi.cpp. This renderer uses only immutable frame inputs
// and disjoint destination rows; GenericTilemapDraw has shared scratch globals
// and cannot safely be called by two threads.
#include "render_worker.h"

static BurnRenderPool spi_workers;
static bool spi_parallel_enabled;

struct SpiLayerScroll {
	INT32 y;
	INT32 x[512];
	bool perLine;
};
static SpiLayerScroll spi_scroll[4];

static bool spi_prepare_scroll()
{
	UINT16 *crtc = (UINT16 *)DrvCRTCRAM;
	spi_scroll[0].y = spi_scroll[0].x[0] = 0;
	spi_scroll[0].perLine = false;
	for (INT32 layer = 1; layer < 4; layer++) {
		SpiLayerScroll &scroll = spi_scroll[layer];
		scroll.y = BURN_ENDIAN_SWAP_INT16(crtc[(0x22 + (layer - 1) * 4) / 2]) & 0x1ff;
		INT32 x = BURN_ENDIAN_SWAP_INT16(crtc[(0x20 + (layer - 1) * 4) / 2]);
		scroll.perLine = false;
		if (rowscroll_enable) {
			INT16 *rows = (INT16 *)&tilemap_ram[0x200 + (layer - 1) * 0x400];
			for (INT32 row = 0; row < 512; row++) {
				INT32 value = x + BURN_ENDIAN_SWAP_INT16(rows[(0x19 + row) & 0x1ff]);
				// Keep the generic renderer's signed-remainder edge cases.
				if (value < 0) return false;
				scroll.x[row] = value & 0x1ff;
				if (rows[row] != rows[0]) scroll.perLine = true;
			}
		} else {
			scroll.x[0] = x & 0x1ff;
		}
	}
	return true;
}

static void spi_draw_layer(INT32 layer, bool opaque, INT32 top, INT32 bottom)
{
	const SpiLayerScroll &scroll = spi_scroll[layer];
	const GenericTilesGfx &gfx = GenericGfxData[layer ? 1 : 2];
	const INT32 size = layer ? 16 : 8;
	const INT32 shift = layer ? 4 : 3;
	const INT32 ymask = layer ? 0x1ff : 0xff;
	const UINT8 transparent = layer ? 0x3f : 0x1f;
	void (*tileInfo)(INT32, GenericTilemapCallbackStruct *) =
		layer == 0 ? text_map_callback : layer == 1 ? back_map_callback :
		layer == 2 ? midl_map_callback : fore_map_callback;

	for (INT32 y = top; y < bottom;) {
		const INT32 sourceY = (scroll.y + y) & ymask;
		const INT32 row = sourceY >> shift;
		const INT32 tileY = sourceY & (size - 1);
		INT32 height = scroll.perLine ? 1 : size - tileY;
		if (height > bottom - y) height = bottom - y;
		const INT32 scrollX = scroll.x[scroll.perLine ? sourceY : 0];

		for (INT32 x = 0; x < nScreenWidth;) {
			const INT32 sourceX = (scrollX + x) & 0x1ff;
			const INT32 col = sourceX >> shift;
			const INT32 tileX = sourceX & (size - 1);
			INT32 width = size - tileX;
			if (width > nScreenWidth - x) width = nScreenWidth - x;
			GenericTilemapCallbackStruct tile;
			tileInfo(layer ? col * 32 + row : row * 64 + col, &tile);
			const UINT32 code = tile.code % gfx.code_mask;
			const UINT32 color = ((tile.color & gfx.color_mask) << gfx.depth) + gfx.color_offset;
			const UINT8 *src = gfx.gfxbase + (code << (shift * 2)) + tileY * size + tileX;
			UINT32 *dest = bitmap32 + y * nScreenWidth + x;
			for (INT32 dy = 0; dy < height; dy++, src += size, dest += nScreenWidth) {
				for (INT32 dx = 0; dx < width; dx++) {
					const UINT8 pen = src[dx];
					if (opaque || pen != transparent) {
						const UINT32 index = color + pen;
						if (DrvAlphaTable[index]) dest[dx] = alpha_blend(dest[dx], DrvPalette[index]);
						else dest[dx] = DrvPalette[index];
					}
				}
			}
			x += width;
		}
		y += height;
	}
}

static void spi_draw_band(INT32 top, INT32 bottom, INT32)
{
	memset(bitmap32 + top * nScreenWidth, 0, (bottom - top) * nScreenWidth * sizeof(UINT32));
	memset(pPrioDraw + top * nScreenWidth, 0, (bottom - top) * nScreenWidth);
	if (~layer_enable & 1 && nBurnLayer & 1) spi_draw_layer(1, true, top, bottom);
	draw_sprites(0, top, bottom);
	if ((layer_enable & 0x15) == 0 && nSpriteEnable & 1) spi_draw_layer(1, false, top, bottom);
	if (~layer_enable & 4) draw_sprites(1, top, bottom);
	if (~layer_enable & 2 && nBurnLayer & 2) spi_draw_layer(2, false, top, bottom);
	if (layer_enable & 4) draw_sprites(1, top, bottom);
	draw_sprites(2, top, bottom);
	if (~layer_enable & 4 && nBurnLayer & 4) spi_draw_layer(3, false, top, bottom);
	draw_sprites(3, top, bottom);
	if (~layer_enable & 8 && nBurnLayer & 8) spi_draw_layer(0, false, top, bottom);
}

static void spi_render_init()
{
	const char *name = BurnDrvGetTextA(DRV_NAME);
	spi_parallel_enabled = sound_system == 1 &&
		(!strncmp(name, "rdft", 4) || !strncmp(name, "rfjet", 5));
	if (spi_parallel_enabled) spi_workers.init(spi_draw_band);
}

static void spi_render_exit()
{
	spi_workers.exit();
	spi_parallel_enabled = false;
}

static bool spi_render_frame()
{
	if (!spi_parallel_enabled) return false;
	INT32 left, right, top, bottom;
	GenericTilesGetClip(&left, &right, &top, &bottom);
	if (left || top || right != nScreenWidth || bottom != nScreenHeight || !spi_prepare_scroll()) return false;
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}
	layer_enable = BURN_ENDIAN_SWAP_INT16(((UINT16 *)DrvCRTCRAM)[0x1c / 2]);
	prepare_sprites();
	// The CPU/Z80 have finished this frame. No emulated RAM or registers may
	// change until all bands complete, including on reset/load/exit paths.
	spi_workers.render(nScreenHeight);

	pBurnDrvPalette = DrvPalette;
	DrvTransferBitmap32();
	return true;
}
