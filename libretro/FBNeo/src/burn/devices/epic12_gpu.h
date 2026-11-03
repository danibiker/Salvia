// CV1000 GPU command batching. CPU VRAM remains authoritative between lists.
// Included by epic12.cpp after the original software rasterizers.
#ifndef FBNEO_EPIC12_GPU_H
#define FBNEO_EPIC12_GPU_H

struct Epic12GpuCommand {
	int sx, sy, x, y, w, h;
	int flipx, flipy, transparent, blend, dmode, sa, da;
	clr_t tint;
};

#define EPIC12_GPU_BINS 1
#define EPIC12_VRAM_PAGES 2176
#define EPIC12_SNAPSHOT_Y 4096
#define EPIC12_SNAPSHOT_MAX_Y 4352
static Epic12GpuCommand epic12_gpu_commands[EPIC12_GPU_BINS][4096];
static int epic12_gpu_count[EPIC12_GPU_BINS];
static unsigned epic12_gpu_pixels[EPIC12_GPU_BINS];
static bool epic12_gpu_source_pages[EPIC12_GPU_BINS][EPIC12_VRAM_PAGES];
static bool epic12_gpu_dest_pages[EPIC12_GPU_BINS][EPIC12_VRAM_PAGES];
static unsigned epic12_gpu_source_count[EPIC12_GPU_BINS];
static rectangle epic12_gpu_bounds[EPIC12_GPU_BINS];
static rectangle epic12_gpu_source_bounds[EPIC12_GPU_BINS];
static bool epic12_gpu_enabled;

// Batches below this many pixels are rasterized on the CPU instead of being
// transferred to the Xenos. The split point was chosen against ddpdfk-like
// traffic; epic12.cpp's gfx_exec() re-reads it from the DIPs each list so a
// console A/B can move it without a rebuild.
static unsigned epic12_gpu_threshold = 512;
static bool epic12_gpu_force_bringup = false;

enum Epic12GpuFlushReason {
	EPIC12_GPU_FLUSH_END_LIST = 0,
	EPIC12_GPU_FLUSH_SOURCE_PAGES,
	EPIC12_GPU_FLUSH_REGION,
	EPIC12_GPU_FLUSH_SOURCE_DEPENDENCY,
	EPIC12_GPU_FLUSH_COMMAND_LIMIT,
	EPIC12_GPU_FLUSH_CPU_UPLOAD,
	EPIC12_GPU_FLUSH_CPU_DRAW,
	EPIC12_GPU_FLUSH_OTHER,
	EPIC12_GPU_FLUSH_REASON_COUNT
};
static inline bool epic12_gpu_intersects(int x, int y, int w, int h, const rectangle &r)
{
	return x <= r.max_x && x + w > r.min_x && y <= r.max_y && y + h > r.min_y;
}

static inline void epic12_gpu_add_initial_pages(int b, int px0, int px1, int py0, int py1)
{
	if (py0 == py1 && px0 == px1) {
		const int page = (py0 << 6) | px0;
		epic12_gpu_source_pages[b][page] = true;
		epic12_gpu_source_count[b] = 1;
	} else {
		for (int py = py0; py <= py1; ++py) {
			const int row = py << 6;
			for (int px = px0; px <= px1; ++px) {
				const int page = row | px;
				if (!epic12_gpu_source_pages[b][page]) {
					epic12_gpu_source_pages[b][page] = true;
					++epic12_gpu_source_count[b];
				}
			}
		}
	}
}


static void epic12_gpu_replay(const Epic12GpuCommand &c)
{
	rectangle clip(c.x, c.x + c.w - 1, c.y, c.y + c.h - 1);
	// Normalized sources name their top-left, including after clipping.
#define GPU_CPU_ARGS &clip,m_bitmaps,c.sx,c.sy,c.x,c.y,c.w,c.h,c.flipy,c.sa,c.da,&c.tint
	if (!c.blend && c.tint.r == 32 && c.tint.g == 32 && c.tint.b == 32) {
		if (c.flipx) {
			if (c.transparent) draw_sprite_f1_ti0_tr1_simple(GPU_CPU_ARGS);
			else draw_sprite_f1_ti0_tr0_simple(GPU_CPU_ARGS);
		} else {
			if (c.transparent) draw_sprite_f0_ti0_tr1_simple(GPU_CPU_ARGS);
			else draw_sprite_f0_ti0_tr0_simple(GPU_CPU_ARGS);
		}
		return;
	}
	if (c.blend && c.dmode == 0) {
		epic12_draw_fixed(c.flipx,c.transparent,GPU_CPU_ARGS);
		return;
	}
#define GPU_CPU_CASE(f,t) \
	if (!c.blend) draw_sprite_f##f##_ti1_tr##t##_plain(GPU_CPU_ARGS); \
	else if (c.dmode == 1) draw_sprite_f##f##_ti1_tr##t##_s0_d1(GPU_CPU_ARGS); \
	else if (c.dmode == 7) draw_sprite_f##f##_ti1_tr##t##_s0_d7(GPU_CPU_ARGS); \
	else if (c.dmode == 3) draw_sprite_f##f##_ti1_tr##t##_s0_d3(GPU_CPU_ARGS); \
	else draw_sprite_f##f##_ti1_tr##t##_s0_d4(GPU_CPU_ARGS)
	if (c.flipx) { if (c.transparent) { GPU_CPU_CASE(1,1); } else { GPU_CPU_CASE(1,0); } }
	else { if (c.transparent) { GPU_CPU_CASE(0,1); } else { GPU_CPU_CASE(0,0); } }
#undef GPU_CPU_CASE
#undef GPU_CPU_ARGS
}

#include "epic12_gpu_batch.h"
#include "epic12_gpu_region.h"

static void epic12_gpu_snapshot_reset();
static void epic12_gpu_staging_reset();

#ifdef EPIC12_GPU_TEST
#include "epic12_gpu_mock.h"
#else
#include "epic12_gpu_xbox.h"
#endif

static int s_snap_page_x = 0;
static int s_snap_page_y = 0;
static int s_snap_page_u = 0;
static int s_snap_page_v = 0;
static int s_snap_page_row_h = 0;
static int s_snap_col256_u = 0;
static int s_snap_max_page_x = 0;
static int s_snap_max_page_y = 0;
static bool epic12_gpu_staging_pages[EPIC12_VRAM_PAGES];
static unsigned epic12_gpu_staging_version[EPIC12_VRAM_PAGES];

struct Epic12SnapshotEntry {
	int sx, sy, w, h;
	int snap_x, snap_y;
	unsigned version;
};
static Epic12SnapshotEntry s_snap_cache[256];

static inline void epic12_gpu_staging_reset()
{
	memset(epic12_gpu_staging_pages, 0, sizeof(epic12_gpu_staging_pages));
}

static void epic12_gpu_snapshot_reset()
{
	if (s_snap_max_page_x > 0 || s_snap_max_page_y > 0 || s_snap_page_u > 0 || s_snap_page_v > 0 || s_snap_col256_u > 0) {
		int max_px = s_snap_max_page_x;
		int max_py = s_snap_max_page_y;
		int x1 = ((max_px + 1) << 7) - 1;
		if (x1 > 8191) x1 = 8191;
		int y1 = EPIC12_SNAPSHOT_Y + ((max_py + 1) << 7) - 1;
		if (y1 > EPIC12_SNAPSHOT_MAX_Y - 1) y1 = EPIC12_SNAPSHOT_MAX_Y - 1;
		rectangle snap_r(0, x1, EPIC12_SNAPSHOT_Y, y1);
		epic12_gpu_invalidate(snap_r);
	}
	s_snap_page_x = 0;
	s_snap_page_y = 0;
	s_snap_page_u = 0;
	s_snap_page_v = 0;
	s_snap_page_row_h = 0;
	s_snap_col256_u = 0;
	s_snap_max_page_x = 0;
	s_snap_max_page_y = 0;
	memset(s_snap_cache, 0, sizeof(s_snap_cache));
}

static inline bool epic12_gpu_snapshot_alloc(int w, int h, int &out_x, int &out_y)
{
	if (w > 512 || h > 256) return false;

	if (w <= 128 && h <= 128) {
		const int aligned_w = (w + 15) & ~15;
		const int aligned_h = (h + 3) & ~3;

		if (s_snap_col256_u > 0) {
			++s_snap_page_x;
			s_snap_col256_u = 0;
		}

		if (s_snap_page_u + aligned_w > 128) {
			s_snap_page_u = 0;
			s_snap_page_v += s_snap_page_row_h;
			s_snap_page_row_h = 0;
		}
		if (s_snap_page_v + aligned_h > 128) {
			s_snap_page_u = 0;
			s_snap_page_v = 0;
			s_snap_page_row_h = 0;
			++s_snap_page_x;
			if (s_snap_page_x >= 64) {
				s_snap_page_x = 0;
				++s_snap_page_y;
			}
		}
		if (s_snap_page_y >= 2) return false;

		out_x = (s_snap_page_x << 7) + s_snap_page_u;
		out_y = EPIC12_SNAPSHOT_Y + (s_snap_page_y << 7) + s_snap_page_v;

		s_snap_page_u += aligned_w;
		if (aligned_h > s_snap_page_row_h) s_snap_page_row_h = aligned_h;

		if (s_snap_page_x > s_snap_max_page_x) s_snap_max_page_x = s_snap_page_x;
		if (s_snap_page_y > s_snap_max_page_y) s_snap_max_page_y = s_snap_page_y;
		return true;
	}

	// Tall or large staging sprite (w > 128 or h > 128, up to 512x256).
	// If coming from 128x128 cell packing, advance to next column.
	if (s_snap_page_u > 0 || s_snap_page_v > 0 || s_snap_page_y > 0) {
		++s_snap_page_x;
		s_snap_page_u = 0;
		s_snap_page_v = 0;
		s_snap_page_row_h = 0;
		s_snap_page_y = 0;
		s_snap_col256_u = 0;
	}

	if (w <= 128) {
		// Tall sprite (w <= 128, 128 < h <= 256, e.g. 32x224 water slices):
		// Shelf pack horizontally within 128-wide column across full 256 height
		const int aligned_w = (w + 15) & ~15;
		if (s_snap_col256_u + aligned_w > 128) {
			++s_snap_page_x;
			s_snap_col256_u = 0;
		}
		if (s_snap_page_x >= 64) return false;

		out_x = (s_snap_page_x << 7) + s_snap_col256_u;
		out_y = EPIC12_SNAPSHOT_Y;
		s_snap_col256_u += aligned_w;

		if (s_snap_page_x > s_snap_max_page_x) s_snap_max_page_x = s_snap_page_x;
		if (1 > s_snap_max_page_y) s_snap_max_page_y = 1;
		return true;
	}

	// Wide sprite (w > 128): allocate dedicated page-aligned column(s)
	if (s_snap_col256_u > 0) {
		++s_snap_page_x;
		s_snap_col256_u = 0;
	}

	const int cols = (w + 127) >> 7;
	if (s_snap_page_x + cols > 64) return false;

	out_x = s_snap_page_x << 7;
	out_y = EPIC12_SNAPSHOT_Y;

	const int last_x = s_snap_page_x + cols - 1;
	if (last_x > s_snap_max_page_x) s_snap_max_page_x = last_x;
	const int last_y = (h > 128) ? 1 : 0;
	if (last_y > s_snap_max_page_y) s_snap_max_page_y = last_y;

	s_snap_page_x += cols;
	return true;
}

static void epic12_gpu_flush_bin(int b, Epic12GpuFlushReason reason = EPIC12_GPU_FLUSH_OTHER)
{
	if (!epic12_gpu_count[b]) return;
	(void)reason; // [Salvia] solo alimentaba contadores de diagnostico
	bool rendered=false;
	bool routeCpu = epic12_gpu_pixels[b] < epic12_gpu_threshold;
#ifdef _XBOX
	if (routeCpu && epic12_gpu_force_bringup && !epic12_xenos_validated && !epic12_xenos_failed) routeCpu=false;
#endif
	if (!routeCpu && epic12_gpu_render(epic12_gpu_commands[b], epic12_gpu_count[b], epic12_gpu_bounds[b])) {
		rendered=true;
	}
	if (!rendered) {
		UINT64 delay = epic12_device_blit_delay;
		for (int i = 0; i < epic12_gpu_count[b]; ++i) epic12_gpu_replay(epic12_gpu_commands[b][i]);
		epic12_device_blit_delay = delay;
	}
	epic12_gpu_invalidate(epic12_gpu_bounds[b]);
	epic12_gpu_snapshot_reset();
	epic12_gpu_count[b] = 0;
	epic12_gpu_pixels[b] = 0;
	epic12_gpu_source_count[b] = 0;
	memset(epic12_gpu_source_pages[b],0,sizeof(epic12_gpu_source_pages[b]));
	memset(epic12_gpu_dest_pages[b],0,sizeof(epic12_gpu_dest_pages[b]));
}

static void epic12_gpu_retire_bin0(Epic12GpuFlushReason reason)
{
	epic12_gpu_flush_bin(0, reason);
}

static void epic12_gpu_flush(Epic12GpuFlushReason reason = EPIC12_GPU_FLUSH_OTHER)
{
	epic12_gpu_flush_bin(0, reason);
	if (reason == EPIC12_GPU_FLUSH_END_LIST || reason == EPIC12_GPU_FLUSH_OTHER) {
		epic12_gpu_snapshot_reset();
		epic12_gpu_staging_reset();
	}
}


static inline int epic12_gpu_can_fit(int b, const rectangle &dest, int sx, int sy, int w, int h,
	int px0, int px1, int py0, int py1, unsigned &newPages, int *newPageList, rectangle &merged)
{
	if (!epic12_gpu_count[b]) {
		merged = dest;
		newPages = 0;
		return 0;
	}
	merged = epic12_gpu_bounds[b];
	if (dest.min_x >= merged.min_x && dest.max_x <= merged.max_x &&
	    dest.min_y >= merged.min_y && dest.max_y <= merged.max_y) {
		if (epic12_gpu_count[b] >= 4096) return 1;
	} else {
		if (dest.min_x < merged.min_x) merged.min_x = dest.min_x;
		if (dest.min_y < merged.min_y) merged.min_y = dest.min_y;
		if (dest.max_x > merged.max_x) merged.max_x = dest.max_x;
		if (dest.max_y > merged.max_y) merged.max_y = dest.max_y;

		if (epic12_gpu_count[b] >= 4096) return 1;
		if (merged.max_x - merged.min_x >= 512 || merged.max_y - merged.min_y >= 512)
			return 1;
	}

	bool page_overlap = false;
	if (py0 < 4) {
		if (py0 == py1 && px0 == px1) {
			page_overlap = epic12_gpu_dest_pages[b][(py0 << 6) | px0];
		} else {
			const int max_py = py1 < 3 ? py1 : 3;
			for (int py = py0; py <= max_py; ++py) {
				const int row = py << 6;
				for (int px = px0; px <= px1; ++px) {
					if (epic12_gpu_dest_pages[b][row | px]) {
						page_overlap = true;
						break;
					}
				}
				if (page_overlap) break;
			}
		}
		if (page_overlap && epic12_gpu_intersects(sx, sy, w, h, epic12_gpu_bounds[b])) {
			return 2;
		}
	}

	newPages = 0;
	if (py0 == py1 && px0 == px1) {
		const int page = (py0 << 6) | px0;
		if (!epic12_gpu_source_pages[b][page]) {
			newPageList[0] = page;
			newPages = 1;
		}
	} else {
		for (int py = py0; py <= py1; ++py) {
			const int row = py << 6;
			for (int px = px0; px <= px1; ++px) {
				const int page = row | px;
				if (!epic12_gpu_source_pages[b][page]) {
					if (newPages < 128) {
						newPageList[newPages++] = page;
					} else {
						return 3;
					}
				}
			}
		}
	}
	if (epic12_gpu_source_count[b] + newPages > epic12_gpu_batch_page_capacity())
		return 3;

	return 0;
}

static inline void epic12_gpu_add_to_bin(int b, const rectangle &merged, unsigned newPages, const int *newPageList,
	int px0, int px1, int py0, int py1,
	int sx, int sy, int x, int y, int w, int h, int flipx, int flipy, int transparent,
	int blend, int dmode, int s_alpha, int d_alpha, const clr_t *tint_clr)
{
	const int dpy0 = y >> 7, dpy1 = (y + h - 1) >> 7;
	const int dpx0 = x >> 7, dpx1 = (x + w - 1) >> 7;
	if (dpy0 == dpy1 && dpx0 == dpx1) {
		epic12_gpu_dest_pages[b][(dpy0 << 6) | dpx0] = true;
	} else {
		for (int dpy = dpy0; dpy <= dpy1; ++dpy) {
			const int row = dpy << 6;
			for (int dpx = dpx0; dpx <= dpx1; ++dpx) {
				epic12_gpu_dest_pages[b][row | dpx] = true;
			}
		}
	}
	if (!epic12_gpu_count[b]) {
		epic12_gpu_bounds[b] = merged;
		epic12_gpu_source_bounds[b] = rectangle(sx, sx + w - 1, sy, sy + h - 1);
		epic12_gpu_add_initial_pages(b, px0, px1, py0, py1);
	} else {
		epic12_gpu_bounds[b] = merged;
		if (sx < epic12_gpu_source_bounds[b].min_x) epic12_gpu_source_bounds[b].min_x = sx;
		if (sy < epic12_gpu_source_bounds[b].min_y) epic12_gpu_source_bounds[b].min_y = sy;
		if (sx + w - 1 > epic12_gpu_source_bounds[b].max_x) epic12_gpu_source_bounds[b].max_x = sx + w - 1;
		if (sy + h - 1 > epic12_gpu_source_bounds[b].max_y) epic12_gpu_source_bounds[b].max_y = sy + h - 1;
		for (unsigned i = 0; i < newPages; ++i) {
			epic12_gpu_source_pages[b][newPageList[i]] = true;
		}
		epic12_gpu_source_count[b] += newPages;
	}
	Epic12GpuCommand &cmd = epic12_gpu_commands[b][epic12_gpu_count[b]++];
	cmd.sx = sx; cmd.sy = sy; cmd.x = x; cmd.y = y; cmd.w = w; cmd.h = h;
	cmd.flipx = !!flipx; cmd.flipy = !!flipy; cmd.transparent = !!transparent;
	cmd.blend = !!blend; cmd.dmode = dmode; cmd.sa = s_alpha; cmd.da = d_alpha; cmd.tint = *tint_clr;
	const unsigned pixels = (unsigned)(w * h);
	epic12_gpu_pixels[b] += pixels;
	epic12_device_blit_delay += pixels;
}

// Precomputed universal tint tables (64 tints x 32 channel values):
// ARGB32 format: R is at [23:19], G is at [15:11], B is at [7:3].
static UINT32 epic12_gpu_tint_r_lut[64][32];
static UINT32 epic12_gpu_tint_g_lut[64][32];
static UINT32 epic12_gpu_tint_b_lut[64][32];
static bool epic12_gpu_tint_lut_ready = false;

static void epic12_gpu_init_tint_luts()
{
	for (int tint = 0; tint < 64; ++tint) {
		for (int i = 0; i < 32; ++i) {
			epic12_gpu_tint_r_lut[tint][i] = (UINT32)epic12_device_colrtable[i][tint] << 19;
			epic12_gpu_tint_g_lut[tint][i] = (UINT32)epic12_device_colrtable[i][tint] << 11;
			epic12_gpu_tint_b_lut[tint][i] = (UINT32)epic12_device_colrtable[i][tint] << 3;
		}
	}
	epic12_gpu_tint_lut_ready = true;
}

static inline void epic12_staging_rasterize_row(UINT32 *d, const UINT32 *s, int w,
	int flipx, int transparent, bool is_identity_tint,
	const UINT32 *lut_r, const UINT32 *lut_g, const UINT32 *lut_b)
{
	if (transparent) {
		if (is_identity_tint) {
			if (!flipx) {
				int col = 0;
				for (; col <= w - 4; col += 4) {
					UINT32 p0 = s[col + 0];
					UINT32 p1 = s[col + 1];
					UINT32 p2 = s[col + 2];
					UINT32 p3 = s[col + 3];
					UINT32 any_p = (p0 | p1 | p2 | p3) & 0x20000000;
					if (any_p) {
						UINT32 all_p = (p0 & p1 & p2 & p3) & 0x20000000;
						if (all_p) {
							d[col + 0] = p0;
							d[col + 1] = p1;
							d[col + 2] = p2;
							d[col + 3] = p3;
						} else {
							if (p0 & 0x20000000) d[col + 0] = p0;
							if (p1 & 0x20000000) d[col + 1] = p1;
							if (p2 & 0x20000000) d[col + 2] = p2;
							if (p3 & 0x20000000) d[col + 3] = p3;
						}
					}
				}
				for (; col < w; ++col) {
					UINT32 p = s[col];
					if (p & 0x20000000) d[col] = p;
				}
			} else {
				int col = 0;
				for (; col <= w - 4; col += 4) {
					UINT32 p0 = s[-col - 0];
					UINT32 p1 = s[-col - 1];
					UINT32 p2 = s[-col - 2];
					UINT32 p3 = s[-col - 3];
					UINT32 any_p = (p0 | p1 | p2 | p3) & 0x20000000;
					if (any_p) {
						UINT32 all_p = (p0 & p1 & p2 & p3) & 0x20000000;
						if (all_p) {
							d[col + 0] = p0;
							d[col + 1] = p1;
							d[col + 2] = p2;
							d[col + 3] = p3;
						} else {
							if (p0 & 0x20000000) d[col + 0] = p0;
							if (p1 & 0x20000000) d[col + 1] = p1;
							if (p2 & 0x20000000) d[col + 2] = p2;
							if (p3 & 0x20000000) d[col + 3] = p3;
						}
					}
				}
				for (; col < w; ++col) {
					UINT32 p = s[-col];
					if (p & 0x20000000) d[col] = p;
				}
			}
		} else {
			// tinted + transparent
			if (!flipx) {
				int col = 0;
				for (; col <= w - 4; col += 4) {
					UINT32 p0 = s[col + 0];
					UINT32 p1 = s[col + 1];
					UINT32 p2 = s[col + 2];
					UINT32 p3 = s[col + 3];
					UINT32 any_p = (p0 | p1 | p2 | p3) & 0x20000000;
					if (any_p) {
						if (p0 & 0x20000000) d[col + 0] = lut_r[(p0 >> 19) & 0x1f] | lut_g[(p0 >> 11) & 0x1f] | lut_b[(p0 >> 3) & 0x1f] | 0x20000000;
						if (p1 & 0x20000000) d[col + 1] = lut_r[(p1 >> 19) & 0x1f] | lut_g[(p1 >> 11) & 0x1f] | lut_b[(p1 >> 3) & 0x1f] | 0x20000000;
						if (p2 & 0x20000000) d[col + 2] = lut_r[(p2 >> 19) & 0x1f] | lut_g[(p2 >> 11) & 0x1f] | lut_b[(p2 >> 3) & 0x1f] | 0x20000000;
						if (p3 & 0x20000000) d[col + 3] = lut_r[(p3 >> 19) & 0x1f] | lut_g[(p3 >> 11) & 0x1f] | lut_b[(p3 >> 3) & 0x1f] | 0x20000000;
					}
				}
				for (; col < w; ++col) {
					UINT32 p = s[col];
					if (p & 0x20000000) d[col] = lut_r[(p >> 19) & 0x1f] | lut_g[(p >> 11) & 0x1f] | lut_b[(p >> 3) & 0x1f] | 0x20000000;
				}
			} else {
				int col = 0;
				for (; col <= w - 4; col += 4) {
					UINT32 p0 = s[-col - 0];
					UINT32 p1 = s[-col - 1];
					UINT32 p2 = s[-col - 2];
					UINT32 p3 = s[-col - 3];
					UINT32 any_p = (p0 | p1 | p2 | p3) & 0x20000000;
					if (any_p) {
						if (p0 & 0x20000000) d[col + 0] = lut_r[(p0 >> 19) & 0x1f] | lut_g[(p0 >> 11) & 0x1f] | lut_b[(p0 >> 3) & 0x1f] | 0x20000000;
						if (p1 & 0x20000000) d[col + 1] = lut_r[(p1 >> 19) & 0x1f] | lut_g[(p1 >> 11) & 0x1f] | lut_b[(p1 >> 3) & 0x1f] | 0x20000000;
						if (p2 & 0x20000000) d[col + 2] = lut_r[(p2 >> 19) & 0x1f] | lut_g[(p2 >> 11) & 0x1f] | lut_b[(p2 >> 3) & 0x1f] | 0x20000000;
						if (p3 & 0x20000000) d[col + 3] = lut_r[(p3 >> 19) & 0x1f] | lut_g[(p3 >> 11) & 0x1f] | lut_b[(p3 >> 3) & 0x1f] | 0x20000000;
					}
				}
				for (; col < w; ++col) {
					UINT32 p = s[-col];
					if (p & 0x20000000) d[col] = lut_r[(p >> 19) & 0x1f] | lut_g[(p >> 11) & 0x1f] | lut_b[(p >> 3) & 0x1f] | 0x20000000;
				}
			}
		}
	} else {
		// !transparent (opaque copy)
		if (is_identity_tint) {
			if (!flipx) {
				if ((((uintptr_t)d | (uintptr_t)s) & 7) == 0) {
					if (w == 16) {
						const UINT64 *s64 = (const UINT64*)s;
						UINT64 *d64 = (UINT64*)d;
						d64[0] = s64[0]; d64[1] = s64[1]; d64[2] = s64[2]; d64[3] = s64[3];
						d64[4] = s64[4]; d64[5] = s64[5]; d64[6] = s64[6]; d64[7] = s64[7];
					} else if (w == 8) {
						const UINT64 *s64 = (const UINT64*)s;
						UINT64 *d64 = (UINT64*)d;
						d64[0] = s64[0]; d64[1] = s64[1]; d64[2] = s64[2]; d64[3] = s64[3];
					} else if (w == 4) {
						const UINT64 *s64 = (const UINT64*)s;
						UINT64 *d64 = (UINT64*)d;
						d64[0] = s64[0]; d64[1] = s64[1];
					} else {
						const UINT64 *s64 = (const UINT64*)s;
						UINT64 *d64 = (UINT64*)d;
						int col = 0;
						const int count64 = w >> 1;
						for (; col < count64; ++col) d64[col] = s64[col];
						if (w & 1) d[w - 1] = s[w - 1];
					}
				} else {
					for (int col = 0; col < w; ++col) d[col] = s[col];
				}
			} else {
				for (int col = 0; col < w; ++col) d[col] = s[-col];
			}
		} else {
			if (!flipx) {
				int col = 0;
				for (; col <= w - 4; col += 4) {
					UINT32 p0 = s[col + 0], p1 = s[col + 1], p2 = s[col + 2], p3 = s[col + 3];
					d[col + 0] = lut_r[(p0 >> 19) & 0x1f] | lut_g[(p0 >> 11) & 0x1f] | lut_b[(p0 >> 3) & 0x1f] | (p0 & 0x20000000);
					d[col + 1] = lut_r[(p1 >> 19) & 0x1f] | lut_g[(p1 >> 11) & 0x1f] | lut_b[(p1 >> 3) & 0x1f] | (p1 & 0x20000000);
					d[col + 2] = lut_r[(p2 >> 19) & 0x1f] | lut_g[(p2 >> 11) & 0x1f] | lut_b[(p2 >> 3) & 0x1f] | (p2 & 0x20000000);
					d[col + 3] = lut_r[(p3 >> 19) & 0x1f] | lut_g[(p3 >> 11) & 0x1f] | lut_b[(p3 >> 3) & 0x1f] | (p3 & 0x20000000);
				}
				for (; col < w; ++col) {
					UINT32 p = s[col];
					d[col] = lut_r[(p >> 19) & 0x1f] | lut_g[(p >> 11) & 0x1f] | lut_b[(p >> 3) & 0x1f] | (p & 0x20000000);
				}
			} else {
				for (int col = 0; col < w; ++col) {
					UINT32 p = s[-col];
					d[col] = lut_r[(p >> 19) & 0x1f] | lut_g[(p >> 11) & 0x1f] | lut_b[(p >> 3) & 0x1f] | (p & 0x20000000);
				}
			}
		}
	}
}

static inline void epic12_staging_write_trans4(UINT32 *d, const UINT32 *s)
{
	UINT32 p0 = s[0], p1 = s[1], p2 = s[2], p3 = s[3];
	UINT32 any_p = (p0 | p1 | p2 | p3) & 0x20000000;
	if (any_p) {
		UINT32 all_p = (p0 & p1 & p2 & p3) & 0x20000000;
		if (all_p && ((((uintptr_t)d | (uintptr_t)s) & 7) == 0)) {
			const UINT64 *s64 = (const UINT64*)s;
			UINT64 *d64 = (UINT64*)d;
			d64[0] = s64[0];
			d64[1] = s64[1];
		} else {
			if (p0 & 0x20000000) d[0] = p0;
			if (p1 & 0x20000000) d[1] = p1;
			if (p2 & 0x20000000) d[2] = p2;
			if (p3 & 0x20000000) d[3] = p3;
		}
	}
}

static inline UINT32 epic12_tint63_pixel(UINT32 p)
{
	const UINT32 hi = p & 0x00808080;
	const UINT32 sat = (hi << 1) - (hi >> 4);
	const UINT32 doubled = (p << 1) & 0x00f8f8f8;
	return (p & 0x20000000) | doubled | sat;
}

static inline UINT64 epic12_tint63_2pix(UINT64 s)
{
	const UINT64 hi = s & 0x0080808000808080ULL;
	const UINT64 sat = (hi << 1) - (hi >> 4);
	const UINT64 doubled = (s << 1) & 0x00f8f8f800f8f8f8ULL;
	return (s & 0x2000000020000000ULL) | doubled | sat;
}

static inline void epic12_staging_write_tint63_trans4(UINT32 *d, const UINT32 *s)
{
	UINT32 p0 = s[0], p1 = s[1], p2 = s[2], p3 = s[3];
	UINT32 any_p = (p0 | p1 | p2 | p3) & 0x20000000;
	if (any_p) {
		UINT32 all_p = (p0 & p1 & p2 & p3) & 0x20000000;
		if (all_p && ((((uintptr_t)d | (uintptr_t)s) & 7) == 0)) {
			const UINT64 *s64 = (const UINT64*)s;
			UINT64 *d64 = (UINT64*)d;
			d64[0] = epic12_tint63_2pix(s64[0]);
			d64[1] = epic12_tint63_2pix(s64[1]);
		} else {
			if (p0 & 0x20000000) d[0] = epic12_tint63_pixel(p0);
			if (p1 & 0x20000000) d[1] = epic12_tint63_pixel(p1);
			if (p2 & 0x20000000) d[2] = epic12_tint63_pixel(p2);
			if (p3 & 0x20000000) d[3] = epic12_tint63_pixel(p3);
		}
	}
}

static inline void epic12_staging_write_tint4(UINT32 *d, const UINT32 *s,
	const UINT32 *lut_r, const UINT32 *lut_g, const UINT32 *lut_b)
{
	UINT32 p0 = s[0], p1 = s[1], p2 = s[2], p3 = s[3];
	d[0] = lut_r[(p0 >> 19) & 0x1f] | lut_g[(p0 >> 11) & 0x1f] | lut_b[(p0 >> 3) & 0x1f] | (p0 & 0x20000000);
	d[1] = lut_r[(p1 >> 19) & 0x1f] | lut_g[(p1 >> 11) & 0x1f] | lut_b[(p1 >> 3) & 0x1f] | (p1 & 0x20000000);
	d[2] = lut_r[(p2 >> 19) & 0x1f] | lut_g[(p2 >> 11) & 0x1f] | lut_b[(p2 >> 3) & 0x1f] | (p2 & 0x20000000);
	d[3] = lut_r[(p3 >> 19) & 0x1f] | lut_g[(p3 >> 11) & 0x1f] | lut_b[(p3 >> 3) & 0x1f] | (p3 & 0x20000000);
}

static inline void epic12_staging_write_tint_trans4(UINT32 *d, const UINT32 *s,
	const UINT32 *lut_r, const UINT32 *lut_g, const UINT32 *lut_b)
{
	UINT32 p0 = s[0], p1 = s[1], p2 = s[2], p3 = s[3];
	UINT32 any_p = (p0 | p1 | p2 | p3) & 0x20000000;
	if (any_p) {
		if (p0 & 0x20000000) d[0] = lut_r[(p0 >> 19) & 0x1f] | lut_g[(p0 >> 11) & 0x1f] | lut_b[(p0 >> 3) & 0x1f] | 0x20000000;
		if (p1 & 0x20000000) d[1] = lut_r[(p1 >> 19) & 0x1f] | lut_g[(p1 >> 11) & 0x1f] | lut_b[(p1 >> 3) & 0x1f] | 0x20000000;
		if (p2 & 0x20000000) d[2] = lut_r[(p2 >> 19) & 0x1f] | lut_g[(p2 >> 11) & 0x1f] | lut_b[(p2 >> 3) & 0x1f] | 0x20000000;
		if (p3 & 0x20000000) d[3] = lut_r[(p3 >> 19) & 0x1f] | lut_g[(p3 >> 11) & 0x1f] | lut_b[(p3 >> 3) & 0x1f] | 0x20000000;
	}
}

static inline void epic12_staging_rasterize_blit(UINT32 *d_row, const UINT32 *s_row, int dimx, int dimy, int s_step,
	int flipx, int transparent, int tint_mode,
	const UINT32 *lut_r, const UINT32 *lut_g, const UINT32 *lut_b)
{
	if (!transparent && tint_mode == 0 && !flipx) {
		if ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0) {
			if (dimx == 8) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					d64[0] = w0; d64[1] = w1; d64[2] = w2; d64[3] = w3;
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			if (dimx == 16) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 w4 = s64[4], w5 = s64[5], w6 = s64[6], w7 = s64[7];
					d64[0] = w0; d64[1] = w1; d64[2] = w2; d64[3] = w3;
					d64[4] = w4; d64[5] = w5; d64[6] = w6; d64[7] = w7;
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			if (dimx == 4) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1];
					d64[0] = w0; d64[1] = w1;
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			const int count64 = dimx >> 1;
			const bool odd = (dimx & 1) != 0;
			for (int r = 0; r < dimy; ++r) {
				const UINT64 *s64 = (const UINT64*)s_row;
				UINT64 *d64 = (UINT64*)d_row;
				for (int col = 0; col < count64; ++col) d64[col] = s64[col];
				if (odd) d_row[dimx - 1] = s_row[dimx - 1];
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		for (int r = 0; r < dimy; ++r) {
			for (int col = 0; col < dimx; ++col) d_row[col] = s_row[col];
			d_row += 8192;
			s_row += s_step;
		}
		return;
	}

	if (!transparent && tint_mode == 1 && !flipx) {
		if ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0) {
			if (dimx == 16) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 w4 = s64[4], w5 = s64[5], w6 = s64[6], w7 = s64[7];
					d64[0] = epic12_tint63_2pix(w0);
					d64[1] = epic12_tint63_2pix(w1);
					d64[2] = epic12_tint63_2pix(w2);
					d64[3] = epic12_tint63_2pix(w3);
					d64[4] = epic12_tint63_2pix(w4);
					d64[5] = epic12_tint63_2pix(w5);
					d64[6] = epic12_tint63_2pix(w6);
					d64[7] = epic12_tint63_2pix(w7);
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			if (dimx == 8) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					d64[0] = epic12_tint63_2pix(w0);
					d64[1] = epic12_tint63_2pix(w1);
					d64[2] = epic12_tint63_2pix(w2);
					d64[3] = epic12_tint63_2pix(w3);
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			if (dimx == 4) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1];
					d64[0] = epic12_tint63_2pix(w0);
					d64[1] = epic12_tint63_2pix(w1);
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			const int count64 = dimx >> 1;
			const bool odd = (dimx & 1) != 0;
			for (int r = 0; r < dimy; ++r) {
				const UINT64 *s64 = (const UINT64*)s_row;
				UINT64 *d64 = (UINT64*)d_row;
				int col = 0;
				for (; col <= count64 - 4; col += 4) {
					UINT64 w0 = s64[col + 0], w1 = s64[col + 1], w2 = s64[col + 2], w3 = s64[col + 3];
					d64[col + 0] = epic12_tint63_2pix(w0);
					d64[col + 1] = epic12_tint63_2pix(w1);
					d64[col + 2] = epic12_tint63_2pix(w2);
					d64[col + 3] = epic12_tint63_2pix(w3);
				}
				for (; col < count64; ++col) {
					d64[col] = epic12_tint63_2pix(s64[col]);
				}
				if (odd) d_row[dimx - 1] = epic12_tint63_pixel(s_row[dimx - 1]);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		for (int r = 0; r < dimy; ++r) {
			for (int col = 0; col < dimx; ++col) d_row[col] = epic12_tint63_pixel(s_row[col]);
			d_row += 8192;
			s_row += s_step;
		}
		return;
	}

	if (!transparent && tint_mode == 2 && !flipx) {
		if ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0) {
			if (dimx == 8) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					d64[0] = w0 & 0x2000000020000000ULL;
					d64[1] = w1 & 0x2000000020000000ULL;
					d64[2] = w2 & 0x2000000020000000ULL;
					d64[3] = w3 & 0x2000000020000000ULL;
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			if (dimx == 16) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 w4 = s64[4], w5 = s64[5], w6 = s64[6], w7 = s64[7];
					d64[0] = w0 & 0x2000000020000000ULL;
					d64[1] = w1 & 0x2000000020000000ULL;
					d64[2] = w2 & 0x2000000020000000ULL;
					d64[3] = w3 & 0x2000000020000000ULL;
					d64[4] = w4 & 0x2000000020000000ULL;
					d64[5] = w5 & 0x2000000020000000ULL;
					d64[6] = w6 & 0x2000000020000000ULL;
					d64[7] = w7 & 0x2000000020000000ULL;
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			const int count64 = dimx >> 1;
			const bool odd = (dimx & 1) != 0;
			for (int r = 0; r < dimy; ++r) {
				const UINT64 *s64 = (const UINT64*)s_row;
				UINT64 *d64 = (UINT64*)d_row;
				int col = 0;
				for (; col <= count64 - 4; col += 4) {
					UINT64 w0 = s64[col + 0], w1 = s64[col + 1], w2 = s64[col + 2], w3 = s64[col + 3];
					d64[col + 0] = w0 & 0x2000000020000000ULL;
					d64[col + 1] = w1 & 0x2000000020000000ULL;
					d64[col + 2] = w2 & 0x2000000020000000ULL;
					d64[col + 3] = w3 & 0x2000000020000000ULL;
				}
				for (; col < count64; ++col) {
					d64[col] = s64[col] & 0x2000000020000000ULL;
				}
				if (odd) d_row[dimx - 1] = s_row[dimx - 1] & 0x20000000;
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		for (int r = 0; r < dimy; ++r) {
			for (int col = 0; col < dimx; ++col) d_row[col] = s_row[col] & 0x20000000;
			d_row += 8192;
			s_row += s_step;
		}
		return;
	}

	if (transparent && tint_mode == 0 && !flipx) {
		if (dimx == 16) {
			if ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 w4 = s64[4], w5 = s64[5], w6 = s64[6], w7 = s64[7];
					UINT64 all_bits = w0 & w1 & w2 & w3 & w4 & w5 & w6 & w7;
					if ((all_bits & 0x2000000020000000ULL) == 0x2000000020000000ULL) {
						d64[0] = w0; d64[1] = w1; d64[2] = w2; d64[3] = w3;
						d64[4] = w4; d64[5] = w5; d64[6] = w6; d64[7] = w7;
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					UINT64 any_bits = w0 | w1 | w2 | w3 | w4 | w5 | w6 | w7;
					if ((any_bits & 0x2000000020000000ULL) == 0) {
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					epic12_staging_write_trans4(d_row, s_row);
					epic12_staging_write_trans4(d_row + 4, s_row + 4);
					epic12_staging_write_trans4(d_row + 8, s_row + 8);
					epic12_staging_write_trans4(d_row + 12, s_row + 12);
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_trans4(d_row, s_row);
				epic12_staging_write_trans4(d_row + 4, s_row + 4);
				epic12_staging_write_trans4(d_row + 8, s_row + 8);
				epic12_staging_write_trans4(d_row + 12, s_row + 12);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		if (dimx == 8) {
			if ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 all_bits = w0 & w1 & w2 & w3;
					if ((all_bits & 0x2000000020000000ULL) == 0x2000000020000000ULL) {
						d64[0] = w0; d64[1] = w1; d64[2] = w2; d64[3] = w3;
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					UINT64 any_bits = w0 | w1 | w2 | w3;
					if ((any_bits & 0x2000000020000000ULL) == 0) {
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					epic12_staging_write_trans4(d_row, s_row);
					epic12_staging_write_trans4(d_row + 4, s_row + 4);
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_trans4(d_row, s_row);
				epic12_staging_write_trans4(d_row + 4, s_row + 4);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		if (dimx == 4) {
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_trans4(d_row, s_row);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		for (int r = 0; r < dimy; ++r) {
			int col = 0;
			for (; col <= dimx - 4; col += 4) {
				epic12_staging_write_trans4(d_row + col, s_row + col);
			}
			for (; col < dimx; ++col) {
				UINT32 p = s_row[col];
				if (p & 0x20000000) d_row[col] = p;
			}
			d_row += 8192;
			s_row += s_step;
		}
		return;
	}

	if (transparent && tint_mode == 1 && !flipx) {
		if (dimx == 16) {
			if ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 w4 = s64[4], w5 = s64[5], w6 = s64[6], w7 = s64[7];
					UINT64 all_bits = w0 & w1 & w2 & w3 & w4 & w5 & w6 & w7;
					if ((all_bits & 0x2000000020000000ULL) == 0x2000000020000000ULL) {
						d64[0] = epic12_tint63_2pix(w0); d64[1] = epic12_tint63_2pix(w1);
						d64[2] = epic12_tint63_2pix(w2); d64[3] = epic12_tint63_2pix(w3);
						d64[4] = epic12_tint63_2pix(w4); d64[5] = epic12_tint63_2pix(w5);
						d64[6] = epic12_tint63_2pix(w6); d64[7] = epic12_tint63_2pix(w7);
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					UINT64 any_bits = w0 | w1 | w2 | w3 | w4 | w5 | w6 | w7;
					if ((any_bits & 0x2000000020000000ULL) == 0) {
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					epic12_staging_write_tint63_trans4(d_row, s_row);
					epic12_staging_write_tint63_trans4(d_row + 4, s_row + 4);
					epic12_staging_write_tint63_trans4(d_row + 8, s_row + 8);
					epic12_staging_write_tint63_trans4(d_row + 12, s_row + 12);
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_tint63_trans4(d_row, s_row);
				epic12_staging_write_tint63_trans4(d_row + 4, s_row + 4);
				epic12_staging_write_tint63_trans4(d_row + 8, s_row + 8);
				epic12_staging_write_tint63_trans4(d_row + 12, s_row + 12);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		if (dimx == 8) {
			if ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 all_bits = w0 & w1 & w2 & w3;
					if ((all_bits & 0x2000000020000000ULL) == 0x2000000020000000ULL) {
						d64[0] = epic12_tint63_2pix(w0); d64[1] = epic12_tint63_2pix(w1);
						d64[2] = epic12_tint63_2pix(w2); d64[3] = epic12_tint63_2pix(w3);
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					UINT64 any_bits = w0 | w1 | w2 | w3;
					if ((any_bits & 0x2000000020000000ULL) == 0) {
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					epic12_staging_write_tint63_trans4(d_row, s_row);
					epic12_staging_write_tint63_trans4(d_row + 4, s_row + 4);
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_tint63_trans4(d_row, s_row);
				epic12_staging_write_tint63_trans4(d_row + 4, s_row + 4);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		if (dimx == 4) {
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_tint63_trans4(d_row, s_row);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		for (int r = 0; r < dimy; ++r) {
			int col = 0;
			for (; col <= dimx - 4; col += 4) {
				epic12_staging_write_tint63_trans4(d_row + col, s_row + col);
			}
			for (; col < dimx; ++col) {
				UINT32 p = s_row[col];
				if (p & 0x20000000) d_row[col] = epic12_tint63_pixel(p);
			}
			d_row += 8192;
			s_row += s_step;
		}
		return;
	}

	if (transparent && tint_mode == 2 && !flipx) {
		if ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0) {
			if (dimx == 16) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 w4 = s64[4], w5 = s64[5], w6 = s64[6], w7 = s64[7];
					UINT64 all_bits = w0 & w1 & w2 & w3 & w4 & w5 & w6 & w7;
					if ((all_bits & 0x2000000020000000ULL) == 0x2000000020000000ULL) {
						d64[0] = 0x2000000020000000ULL; d64[1] = 0x2000000020000000ULL;
						d64[2] = 0x2000000020000000ULL; d64[3] = 0x2000000020000000ULL;
						d64[4] = 0x2000000020000000ULL; d64[5] = 0x2000000020000000ULL;
						d64[6] = 0x2000000020000000ULL; d64[7] = 0x2000000020000000ULL;
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					UINT64 any_bits = w0 | w1 | w2 | w3 | w4 | w5 | w6 | w7;
					if ((any_bits & 0x2000000020000000ULL) == 0) {
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					for (int col = 0; col < 16; ++col) {
						if (s_row[col] & 0x20000000) d_row[col] = 0x20000000;
					}
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
			if (dimx == 8) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 *d64 = (UINT64*)d_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 all_bits = w0 & w1 & w2 & w3;
					if ((all_bits & 0x2000000020000000ULL) == 0x2000000020000000ULL) {
						d64[0] = 0x2000000020000000ULL; d64[1] = 0x2000000020000000ULL;
						d64[2] = 0x2000000020000000ULL; d64[3] = 0x2000000020000000ULL;
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					UINT64 any_bits = w0 | w1 | w2 | w3;
					if ((any_bits & 0x2000000020000000ULL) == 0) {
						d_row += 8192;
						s_row += s_step;
						continue;
					}
					for (int col = 0; col < 8; ++col) {
						if (s_row[col] & 0x20000000) d_row[col] = 0x20000000;
					}
					d_row += 8192;
					s_row += s_step;
				}
				return;
			}
		}
		for (int r = 0; r < dimy; ++r) {
			for (int col = 0; col < dimx; ++col) {
				if (s_row[col] & 0x20000000) d_row[col] = 0x20000000;
			}
			d_row += 8192;
			s_row += s_step;
		}
		return;
	}

	if (!transparent && tint_mode == 3 && !flipx) {
		if (dimx == 16) {
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_tint4(d_row, s_row, lut_r, lut_g, lut_b);
				epic12_staging_write_tint4(d_row + 4, s_row + 4, lut_r, lut_g, lut_b);
				epic12_staging_write_tint4(d_row + 8, s_row + 8, lut_r, lut_g, lut_b);
				epic12_staging_write_tint4(d_row + 12, s_row + 12, lut_r, lut_g, lut_b);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		if (dimx == 8) {
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_tint4(d_row, s_row, lut_r, lut_g, lut_b);
				epic12_staging_write_tint4(d_row + 4, s_row + 4, lut_r, lut_g, lut_b);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		if (dimx == 4) {
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_tint4(d_row, s_row, lut_r, lut_g, lut_b);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
	}

	if (transparent && tint_mode == 3 && !flipx) {
		if (dimx == 16) {
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_tint_trans4(d_row, s_row, lut_r, lut_g, lut_b);
				epic12_staging_write_tint_trans4(d_row + 4, s_row + 4, lut_r, lut_g, lut_b);
				epic12_staging_write_tint_trans4(d_row + 8, s_row + 8, lut_r, lut_g, lut_b);
				epic12_staging_write_tint_trans4(d_row + 12, s_row + 12, lut_r, lut_g, lut_b);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		if (dimx == 8) {
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_tint_trans4(d_row, s_row, lut_r, lut_g, lut_b);
				epic12_staging_write_tint_trans4(d_row + 4, s_row + 4, lut_r, lut_g, lut_b);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
		if (dimx == 4) {
			for (int r = 0; r < dimy; ++r) {
				epic12_staging_write_tint_trans4(d_row, s_row, lut_r, lut_g, lut_b);
				d_row += 8192;
				s_row += s_step;
			}
			return;
		}
	}

	for (int r = 0; r < dimy; ++r) {
		epic12_staging_rasterize_row(d_row, s_row, dimx, flipx, transparent, tint_mode == 0, lut_r, lut_g, lut_b);
		d_row += 8192;
		s_row += s_step;
	}
}


// Direct fast path for off-screen staging blits (x >= 768, non-blended):
// Avoids 17-argument function spills, LHS hazards, dynamic stack table generation,
// and CRT memcpy overhead on in-order PowerPC cores.
static inline bool epic12_gpu_staging_blit(int x, int y, int sx, int sy, int w, int h,
	int flipx, int flipy, int transparent, int tint_r_idx, int tint_g_idx, int tint_b_idx)
{
	if (!epic12_gpu_tint_lut_ready) epic12_gpu_init_tint_luts();

	const int dpy0 = y >> 7, dpy1 = (y + h - 1) >> 7;
	const int dpx0 = x >> 7, dpx1 = (x + w - 1) >> 7;
	if (dpy0 == dpy1 && dpx0 == dpx1) {
		const int p = (dpy0 << 6) | dpx0;
		epic12_gpu_staging_pages[p] = true;
		++epic12_gpu_staging_version[p];
	} else {
		for (int py = dpy0; py <= dpy1; ++py) {
			const int row = py << 6;
			for (int px = dpx0; px <= dpx1; ++px) {
				const int p = row | px;
				epic12_gpu_staging_pages[p] = true;
				++epic12_gpu_staging_version[p];
			}
		}
	}

	// Fallback WAR check: in case any queued GPU command samples directly from
	// this destination rectangle (e.g. if snapshot allocation had failed).
	if (epic12_gpu_count[0]) {
		bool war_overlap = false;
		if (dpy0 == dpy1 && dpx0 == dpx1) {
			war_overlap = epic12_gpu_source_pages[0][(dpy0 << 6) | dpx0];
		} else {
			for (int py = dpy0; py <= dpy1 && !war_overlap; ++py) {
				const int row = py << 6;
				for (int px = dpx0; px <= dpx1; ++px) {
					if (epic12_gpu_source_pages[0][row | px]) {
						war_overlap = true;
						break;
					}
				}
			}
		}
		if (war_overlap) {
			epic12_gpu_flush_bin(0, EPIC12_GPU_FLUSH_SOURCE_DEPENDENCY);
		}
	}

	rectangle r_inv(x, x + w - 1, y, y + h - 1);
	epic12_gpu_invalidate(r_inv);

	int tint_mode = 3;
	if (tint_r_idx == 32 && tint_g_idx == 32 && tint_b_idx == 32) {
		tint_mode = 0;
	} else if (tint_r_idx == 63 && tint_g_idx == 63 && tint_b_idx == 63) {
		tint_mode = 1;
	} else if (tint_r_idx == 0 && tint_g_idx == 0 && tint_b_idx == 0) {
		tint_mode = 2;
	}

	const UINT32 *lut_r = (tint_mode <= 2) ? NULL : epic12_gpu_tint_r_lut[tint_r_idx & 63];
	const UINT32 *lut_g = (tint_mode <= 2) ? NULL : epic12_gpu_tint_g_lut[tint_g_idx & 63];
	const UINT32 *lut_b = (tint_mode <= 2) ? NULL : epic12_gpu_tint_b_lut[tint_b_idx & 63];

	const int cur_y_start = flipy ? (h - 1) : 0;
	const int s_step = flipy ? -8192 : 8192;
	UINT32 *d = m_bitmaps + ((size_t)y) * 8192 + x;
	const int s_x_start = flipx ? (sx + w - 1) : sx;
	const UINT32 *s = m_bitmaps + ((size_t)((sy + cur_y_start) & 4095)) * 8192 + s_x_start;

	epic12_staging_rasterize_blit(d, s, w, h, s_step, flipx, transparent, tint_mode, lut_r, lut_g, lut_b);
	epic12_device_blit_delay += (unsigned)(w * h);
	return true;
}

// Direct stream execution for contiguous off-screen staging blits (x >= 768, non-blended).
// Streams through consecutive staging commands in m_ram16_copy in a single tight loop,
// eliminating function dispatch, stack saves, and WAR checks for thousands of blits per frame.
static inline int epic12_gpu_staging_stream(UINT32 start_addr)
{
	if (!epic12_gpu_tint_lut_ready) epic12_gpu_init_tint_luts();

	const UINT32 max_ram = (UINT32)m_main_rammask + 1;
	UINT32 cur_addr = start_addr;
	int fused_count = 0;

	int inv_min_x = 8192, inv_min_y = 4096;
	int inv_max_x = -1, inv_max_y = -1;

	while (1) {
		const UINT32 ram_offset = cur_addr & m_main_rammask;
		if (ram_offset + 20 > max_ram) break;

		const UINT16 *p = &m_ram16_copy[ram_offset >> 1];
		const UINT16 attr = p[0];

		// Must be a draw command (0x1000) and not blended
		if ((attr & 0xf000) != 0x1000 || (attr & 0x0200)) break;

		const UINT16 dst_x_start = p[4];
		// Must be offscreen staging destination (>= 768)
		if (dst_x_start < 768) break;

		const UINT16 dst_y_start = p[5];
		const UINT16 w_raw = p[6];
		const UINT16 h_raw = p[7];
		const int dimx = (w_raw & 0x1fff) + 1;
		const int dimy = (h_raw & 0x0fff) + 1;
		const int x = (INT16)dst_x_start;
		const int y = (INT16)dst_y_start;

		const int sx = p[2] & 0x1fff;
		const int sy = p[3] & 0x0fff;

		// Boundary sanity check
		if (sx + dimx > 8192 || sy + dimy > 4096 || x + dimx > 8192 || y + dimy > 4096 || dimx > 512 || dimy > 512) {
			break;
		}

		// Fast clip rejection
		if (x > m_clip.max_x || y > m_clip.max_y || x + dimx - 1 < m_clip.min_x || y + dimy - 1 < m_clip.min_y) {
			cur_addr += 20;
			++fused_count;
			continue;
		}

		const int flipx = attr & 0x0800;
		const int flipy = attr & 0x0400;
		const int trans = attr & 0x0100;
		const int tint_r_idx = (p[8] & 0x00ff) >> 2;
		const int tint_g_idx = ((p[9] >> 8) & 0xff) >> 2;
		const int tint_b_idx = (p[9] & 0xff) >> 2;

		int total_dimx = dimx;
		int step_count = 1;
		bool repeated_src = false;

		// Peek forward to fuse adjacent horizontal strips
		while (1) {
			const UINT32 next_ram = (cur_addr + step_count * 20) & m_main_rammask;
			if (next_ram + 20 > max_ram) break;

			const UINT16 *np = &m_ram16_copy[next_ram >> 1];
			const UINT16 nattr = np[0];
			if ((nattr & 0xf000) != 0x1000 || (nattr & 0x0200)) break;

			const int nx = (INT16)np[4];
			const int ny = (INT16)np[5];
			const int ndimx = (np[6] & 0x1fff) + 1;
			const int ndimy = (np[7] & 0x0fff) + 1;
			const int nsx = np[2] & 0x1fff;
			const int nsy = np[3] & 0x0fff;

			if (ny != y || ndimy != dimy || nx != x + total_dimx) break;
			if ((nattr & 0x0d00) != (attr & 0x0d00)) break;
			if (np[8] != p[8] || np[9] != p[9]) break;

			if (!repeated_src && step_count == 1 && nsx == sx && nsy == sy && ndimx == dimx) {
				repeated_src = true;
			} else if (repeated_src) {
				if (nsx != sx || nsy != sy || ndimx != dimx) break;
			} else {
				if (nsx != sx + total_dimx || nsy != sy) break;
			}

			if (x + total_dimx + ndimx > 8192) break;
			if (!repeated_src && (sx + total_dimx + ndimx > 8192)) break;

			total_dimx += ndimx;
			++step_count;
		}

		// Check WAR hazard against queued GPU commands in bin 0
		const int dpy0 = y >> 7, dpy1 = (y + dimy - 1) >> 7;
		const int dpx0 = x >> 7, dpx1 = (x + total_dimx - 1) >> 7;
		if (epic12_gpu_count[0]) {
			bool war_overlap = false;
			if (dpy0 == dpy1 && dpx0 == dpx1) {
				war_overlap = epic12_gpu_source_pages[0][(dpy0 << 6) | dpx0];
			} else {
				for (int py = dpy0; py <= dpy1 && !war_overlap; ++py) {
					const int row = py << 6;
					for (int px = dpx0; px <= dpx1; ++px) {
						if (epic12_gpu_source_pages[0][row | px]) {
							war_overlap = true;
							break;
						}
					}
				}
			}
			if (war_overlap) {
				if (inv_max_x >= inv_min_x) {
					rectangle r_inv(inv_min_x, inv_max_x, inv_min_y, inv_max_y);
					epic12_gpu_invalidate(r_inv);
					inv_min_x = 8192; inv_min_y = 4096;
					inv_max_x = -1; inv_max_y = -1;
				}
				epic12_gpu_flush_bin(0, EPIC12_GPU_FLUSH_SOURCE_DEPENDENCY);
			}
		}

		// Mark staging pages
		if (dpy0 == dpy1 && dpx0 == dpx1) {
			const int p_idx = (dpy0 << 6) | dpx0;
			epic12_gpu_staging_pages[p_idx] = true;
			++epic12_gpu_staging_version[p_idx];
		} else {
			for (int py = dpy0; py <= dpy1; ++py) {
				const int row = py << 6;
				for (int px = dpx0; px <= dpx1; ++px) {
					const int p_idx = row | px;
					epic12_gpu_staging_pages[p_idx] = true;
					++epic12_gpu_staging_version[p_idx];
				}
			}
		}

		if (x < inv_min_x) inv_min_x = x;
		if (y < inv_min_y) inv_min_y = y;
		if (x + total_dimx - 1 > inv_max_x) inv_max_x = x + total_dimx - 1;
		if (y + dimy - 1 > inv_max_y) inv_max_y = y + dimy - 1;


		int tint_mode = 3;
		if (tint_r_idx == 32 && tint_g_idx == 32 && tint_b_idx == 32) {
			tint_mode = 0;
		} else if (tint_r_idx == 63 && tint_g_idx == 63 && tint_b_idx == 63) {
			tint_mode = 1;
		} else if (tint_r_idx == 0 && tint_g_idx == 0 && tint_b_idx == 0) {
			tint_mode = 2;
		}

		const UINT32 *lut_r = (tint_mode <= 2) ? NULL : epic12_gpu_tint_r_lut[tint_r_idx & 63];
		const UINT32 *lut_g = (tint_mode <= 2) ? NULL : epic12_gpu_tint_g_lut[tint_g_idx & 63];
		const UINT32 *lut_b = (tint_mode <= 2) ? NULL : epic12_gpu_tint_b_lut[tint_b_idx & 63];

		UINT32 *d_row = m_bitmaps + ((size_t)y) * 8192 + x;
		const int s_x_start = flipx ? (sx + dimx - 1) : sx;
		const int s_y_start = flipy ? (sy + dimy - 1) : sy;
		const int s_step = flipy ? -8192 : 8192;
		const UINT32 *s_row = m_bitmaps + ((size_t)(s_y_start & 4095)) * 8192 + s_x_start;

		if (step_count > 1 && repeated_src) {
			if (!trans && tint_mode == 1 && !flipx && dimx == 16 && ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0)) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 w0 = epic12_tint63_2pix(s64[0]);
					UINT64 w1 = epic12_tint63_2pix(s64[1]);
					UINT64 w2 = epic12_tint63_2pix(s64[2]);
					UINT64 w3 = epic12_tint63_2pix(s64[3]);
					UINT64 w4 = epic12_tint63_2pix(s64[4]);
					UINT64 w5 = epic12_tint63_2pix(s64[5]);
					UINT64 w6 = epic12_tint63_2pix(s64[6]);
					UINT64 w7 = epic12_tint63_2pix(s64[7]);
					UINT64 *d64 = (UINT64*)d_row;
					for (int k = 0; k < step_count; ++k) {
						d64[8 * k + 0] = w0;
						d64[8 * k + 1] = w1;
						d64[8 * k + 2] = w2;
						d64[8 * k + 3] = w3;
						d64[8 * k + 4] = w4;
						d64[8 * k + 5] = w5;
						d64[8 * k + 6] = w6;
						d64[8 * k + 7] = w7;
					}
					d_row += 8192;
					s_row += s_step;
				}
			} else if (!trans && tint_mode == 0 && !flipx && dimx == 16 && ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0)) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 w0 = s64[0], w1 = s64[1], w2 = s64[2], w3 = s64[3];
					UINT64 w4 = s64[4], w5 = s64[5], w6 = s64[6], w7 = s64[7];
					UINT64 *d64 = (UINT64*)d_row;
					for (int k = 0; k < step_count; ++k) {
						d64[8 * k + 0] = w0;
						d64[8 * k + 1] = w1;
						d64[8 * k + 2] = w2;
						d64[8 * k + 3] = w3;
						d64[8 * k + 4] = w4;
						d64[8 * k + 5] = w5;
						d64[8 * k + 6] = w6;
						d64[8 * k + 7] = w7;
					}
					d_row += 8192;
					s_row += s_step;
				}
			} else if (!trans && tint_mode == 2 && !flipx && dimx == 8 && ((((uintptr_t)d_row | (uintptr_t)s_row) & 7) == 0)) {
				for (int r = 0; r < dimy; ++r) {
					const UINT64 *s64 = (const UINT64*)s_row;
					UINT64 w0 = s64[0] & 0x2000000020000000ULL;
					UINT64 w1 = s64[1] & 0x2000000020000000ULL;
					UINT64 w2 = s64[2] & 0x2000000020000000ULL;
					UINT64 w3 = s64[3] & 0x2000000020000000ULL;
					UINT64 *d64 = (UINT64*)d_row;
					for (int k = 0; k < step_count; ++k) {
						d64[4 * k + 0] = w0;
						d64[4 * k + 1] = w1;
						d64[4 * k + 2] = w2;
						d64[4 * k + 3] = w3;
					}
					d_row += 8192;
					s_row += s_step;
				}
			} else {
				for (int k = 0; k < step_count; ++k) {
					UINT32 *d_k = m_bitmaps + ((size_t)y) * 8192 + (x + k * dimx);
					epic12_staging_rasterize_blit(d_k, s_row, dimx, dimy, s_step, flipx, trans, tint_mode, lut_r, lut_g, lut_b);
				}
			}
		} else {
			epic12_staging_rasterize_blit(d_row, s_row, total_dimx, dimy, s_step, flipx, trans, tint_mode, lut_r, lut_g, lut_b);
		}

		epic12_device_blit_delay += (unsigned)(total_dimx * dimy);
		cur_addr += step_count * 20;
		fused_count += step_count;
	}

	if (inv_max_x >= inv_min_x) {
		rectangle r_inv(inv_min_x, inv_max_x, inv_min_y, inv_max_y);
		epic12_gpu_invalidate(r_inv);
	}

	return fused_count;
}

// Return true only when the draw was queued, or is an original no-op.
static bool epic12_gpu_submit(int flipx, int transparent, int blend, int smode, int dmode, BLIT_PARAMS)
{
	if (!epic12_gpu_enabled) return false;
#ifndef EPIC12_GPU_TEST
	const int norm_dm = (dmode == 4 || dmode == 7 || dmode == 3) ? 0 : dmode;
	if (blend && norm_dm != 0 && !(norm_dm == 1 && s_alpha == 0)) {
		return false;
	}
	if (blend && ((dimy <= 2 && dimx >= 120) || (dimx <= 2 && dimy >= 120) ||
	             (dmode == 4 && tint_clr->r == 0 && tint_clr->g == 0 && tint_clr->b == 0))) {
		return false;
	}
#endif
	if (blend && (smode != 0 || (dmode != 0 && dmode != 1 && dmode != 4 && dmode != 7 && dmode != 3))) {
		return false;
	}
	// Original rasterizers reject horizontal wrap before clipping.
	if (src_x + dimx > 8192) return true;
	int x, y, w, h, sx, sy;
	if (dst_x_start >= clip->min_x && dst_y_start >= clip->min_y &&
	    dst_x_start + dimx - 1 <= clip->max_x && dst_y_start + dimy - 1 <= clip->max_y) {
		x = dst_x_start;
		y = dst_y_start;
		w = dimx;
		h = dimy;
		sx = src_x;
		sy = (src_y) & 4095;
	} else {
		int x0 = dst_x_start < clip->min_x ? clip->min_x - dst_x_start : 0;
		int y0 = dst_y_start < clip->min_y ? clip->min_y - dst_y_start : 0;
		int x1 = dimx, y1 = dimy;
		if (dst_x_start + x1 - 1 > clip->max_x) x1 = clip->max_x - dst_x_start + 1;
		if (dst_y_start + y1 - 1 > clip->max_y) y1 = clip->max_y - dst_y_start + 1;
		if (x1 <= x0 || y1 <= y0) {
			return true;
		}
		w = x1 - x0;
		h = y1 - y0;
		x = dst_x_start + x0;
		y = dst_y_start + y0;
		sx = src_x + (flipx ? dimx - x1 : x0);
		sy = (src_y + (flipy ? dimy - y1 : y0)) & 4095;
	}
	if (w > 512 || h > 512) {
		return false;
	}
	if (x < 0 || y < 0 || x + w > 8192 || y + h > 4096) {
		return false;
	}
	if (sy + h > 4096) {
		return false;
	}
	const rectangle dest(x, x + w - 1, y, y + h - 1);
	// In-place copies depend on the original per-pixel write order.
	if (epic12_gpu_intersects(sx, sy, w, h, dest)) {
		return false;
	}
	// Direct CPU blit for off-screen staging buffers (x >= 768):
	// Cave CV1000 games use off-screen VRAM (Zones 1 & 2, x >= 768) as scratchpads
	// to composite multi-part sprites with a few tiny draws (<8,192 px) before
	// blitting them onto the visible display (Buffer 0: x < 320, Buffer 1: 384 <= x < 704).
	// Keeping x < 768 ensures both on-screen double buffers remain fully on the GPU.
	if (x >= 768) {
		if (!blend) {
			return epic12_gpu_staging_blit(x, y, sx, sy, w, h,
				flipx, flipy, transparent, tint_clr->r, tint_clr->g, tint_clr->b);
		}

		// General fallback for complex off-screen staging blits (e.g. blend != 0)
		Epic12GpuCommand c;
		c.sx = sx; c.sy = sy; c.x = x; c.y = y; c.w = w; c.h = h;
		c.flipx = !!flipx; c.flipy = !!flipy; c.transparent = !!transparent;
		c.blend = !!blend; c.dmode = dmode; c.sa = s_alpha; c.da = d_alpha; c.tint = *tint_clr;
		epic12_gpu_replay(c);
		return true;
	}

	unsigned np = 0;
	int npl[128];
	rectangle m;
	int draw_sx = sx;
	int draw_sy = sy;
	bool is_staging = false;
	bool reused = false;
	unsigned ver = 0;
	// Background raster scanlines (w > 128 && h <= 4) tile off-screen VRAM without overwriting
	// in the same frame; snapshotting them scatters a few source pages across dozens of snapshot
	// pages, thrashing the atlas and page limit. All other staging sprites (including multi-part
	// strips with w <= 128 and tall boss segments packed into dedicated columns) are snapshotted
	// to eliminate WAR stalls.
	if (sx >= 768 && sy < 4096 && w <= 512 && h <= 256 && !(w > 128 && h <= 4)) {
		const int spx0 = sx >> 7, spx1 = (sx + w - 1) >> 7;
		const int spy0 = sy >> 7, spy1 = (sy + h - 1) >> 7;
		if (spx0 == spx1 && spy0 == spy1) {
			is_staging = epic12_gpu_staging_pages[(spy0 << 6) | spx0];
		} else {
			for (int py = spy0; py <= spy1 && !is_staging; ++py) {
				const int row = py << 6;
				for (int px = spx0; px <= spx1; ++px) {
					if (epic12_gpu_staging_pages[row | px]) {
						is_staging = true;
						break;
					}
				}
			}
		}

		if (is_staging) {
			if (spx0 == spx1 && spy0 == spy1) {
				ver = epic12_gpu_staging_version[(spy0 << 6) | spx0];
			} else {
				for (int py = spy0; py <= spy1; ++py) {
					const int row = py << 6;
					for (int px = spx0; px <= spx1; ++px) {
						ver += epic12_gpu_staging_version[row | px];
					}
				}
			}
			const unsigned h_idx = (((unsigned)sx * 31) ^ ((unsigned)sy * 17) ^ ((unsigned)w * 7) ^ (unsigned)h) & 255;
			if (s_snap_cache[h_idx].sx == sx && s_snap_cache[h_idx].sy == sy &&
				s_snap_cache[h_idx].w == w && s_snap_cache[h_idx].h == h &&
				s_snap_cache[h_idx].version == ver) {
				draw_sx = s_snap_cache[h_idx].snap_x;
				draw_sy = s_snap_cache[h_idx].snap_y;
				reused = true;
			} else {
				int snap_x, snap_y;
				if (epic12_gpu_snapshot_alloc(w, h, snap_x, snap_y)) {
					draw_sx = snap_x;
					draw_sy = snap_y;
					s_snap_cache[h_idx].sx = sx;
					s_snap_cache[h_idx].sy = sy;
					s_snap_cache[h_idx].w = w;
					s_snap_cache[h_idx].h = h;
					s_snap_cache[h_idx].snap_x = snap_x;
					s_snap_cache[h_idx].snap_y = snap_y;
					s_snap_cache[h_idx].version = ver;
				} else {
					epic12_gpu_retire_bin0(EPIC12_GPU_FLUSH_SOURCE_PAGES);
					if (epic12_gpu_snapshot_alloc(w, h, snap_x, snap_y)) {
						draw_sx = snap_x;
						draw_sy = snap_y;
						s_snap_cache[h_idx].sx = sx;
						s_snap_cache[h_idx].sy = sy;
						s_snap_cache[h_idx].w = w;
						s_snap_cache[h_idx].h = h;
						s_snap_cache[h_idx].snap_x = snap_x;
						s_snap_cache[h_idx].snap_y = snap_y;
						s_snap_cache[h_idx].version = ver;
					} else {
						draw_sx = sx;
						draw_sy = sy;
					}
				}
			}
		}
	}

	int py0 = draw_sy >> 7, py1 = (draw_sy + h - 1) >> 7;
	int px0 = draw_sx >> 7, px1 = (draw_sx + w - 1) >> 7;

	int fit = epic12_gpu_can_fit(0, dest, draw_sx, draw_sy, w, h, px0, px1, py0, py1, np, npl, m);
	if (fit != 0) {
		Epic12GpuFlushReason reason0 = (fit == 2) ? EPIC12_GPU_FLUSH_SOURCE_DEPENDENCY :
			((fit == 3) ? EPIC12_GPU_FLUSH_SOURCE_PAGES : EPIC12_GPU_FLUSH_REGION);
		epic12_gpu_retire_bin0(reason0);
		if (is_staging && !reused) {
			int snap_x, snap_y;
			if (epic12_gpu_snapshot_alloc(w, h, snap_x, snap_y)) {
				draw_sx = snap_x;
				draw_sy = snap_y;
				const unsigned h_idx = (((unsigned)sx * 31) ^ ((unsigned)sy * 17) ^ ((unsigned)w * 7) ^ (unsigned)h) & 255;
				s_snap_cache[h_idx].sx = sx;
				s_snap_cache[h_idx].sy = sy;
				s_snap_cache[h_idx].w = w;
				s_snap_cache[h_idx].h = h;
				s_snap_cache[h_idx].snap_x = snap_x;
				s_snap_cache[h_idx].snap_y = snap_y;
				s_snap_cache[h_idx].version = ver;
			} else {
				draw_sx = sx;
				draw_sy = sy;
			}
		}
		py0 = draw_sy >> 7; py1 = (draw_sy + h - 1) >> 7;
		px0 = draw_sx >> 7; px1 = (draw_sx + w - 1) >> 7;
		fit = epic12_gpu_can_fit(0, dest, draw_sx, draw_sy, w, h, px0, px1, py0, py1, np, npl, m);
	}
	if (fit == 0) {
		if (is_staging && !reused && draw_sy >= EPIC12_SNAPSHOT_Y) {
			epic12_gpu_copy_rows(
				m_bitmaps + (size_t)draw_sy * 8192 + draw_sx, 8192 * sizeof(UINT32),
				m_bitmaps + (size_t)sy * 8192 + sx, 8192 * sizeof(UINT32),
				w * sizeof(UINT32), h);
		}
		epic12_gpu_add_to_bin(0, m, np, npl, px0, px1, py0, py1,
			draw_sx, draw_sy, x, y, w, h, flipx, flipy, transparent, blend, dmode, s_alpha, d_alpha, tint_clr);
		return true;
	}
	return false;
}

static void epic12_gpu_cpu_write(int x, int y, int w, int h,
	Epic12GpuFlushReason reason = EPIC12_GPU_FLUSH_OTHER)
{
	// Conservative invalidation also handles a wrapped source/large upload.
	// The upload loop can spill across a VRAM row; a geometric rectangle
	// alone would miss the beginning of the next row in that case.
	if (x < 0 || y < 0 || x+w > 8192 || y+h > 4096) {
		epic12_gpu_flush(reason);
		epic12_gpu_reset();
		return;
	}
	if (reason == EPIC12_GPU_FLUSH_CPU_UPLOAD) {
		bool conflict = false;
		if (epic12_gpu_count[0]) {
			if (epic12_gpu_intersects(x, y, w, h, epic12_gpu_bounds[0])) {
				conflict = true;
			} else {
				const int py0 = y >> 7, py1 = (y + h - 1) >> 7;
				const int px0 = x >> 7, px1 = (x + w - 1) >> 7;
				for (int py = py0; py <= py1 && !conflict; ++py) {
					const int row = py << 6;
					for (int px = px0; px <= px1; ++px) {
						if (epic12_gpu_source_pages[0][row | px]) {
							conflict = true;
							break;
						}
					}
				}
			}
		}
		if (conflict) {
			epic12_gpu_flush(reason);
		}
	} else {
		epic12_gpu_flush(reason);
	}
	if (x >= 768) {
		const int dpy0 = y >> 7, dpy1 = (y + h - 1) >> 7;
		const int dpx0 = x >> 7, dpx1 = (x + w - 1) >> 7;
		for (int py = dpy0; py <= dpy1; ++py) {
			const int row = py << 6;
			for (int px = dpx0; px <= dpx1; ++px) {
				epic12_gpu_staging_pages[row | px] = true;
			}
		}
	}
	rectangle r(x,x+w-1,y,y+h-1);
	epic12_gpu_invalidate(r);
}
// Software sprite rasterizers clip destination writes; uploads can spill rows
// and must keep the conservative cpu_write path above. Always drain queued
// draws, including for an empty rectangle, preserving the existing ordering.
static void epic12_gpu_cpu_draw(int x, int y, int w, int h, const rectangle &clip)
{
	int x0=x>clip.min_x?x:clip.min_x;
	int y0=y>clip.min_y?y:clip.min_y;
	int x1=x+w-1<clip.max_x?x+w-1:clip.max_x;
	int y1=y+h-1<clip.max_y?y+h-1:clip.max_y;
	if (x1<x0 || y1<y0) {
		return;
	}
	epic12_gpu_cpu_write(x0,y0,x1-x0+1,y1-y0+1,EPIC12_GPU_FLUSH_CPU_DRAW);
}
#endif
