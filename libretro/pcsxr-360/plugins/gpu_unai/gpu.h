/***************************************************************************
*   Copyright (C) 2010 PCSX4ALL Team                                      *
*   Copyright (C) 2010 Unai                                               *
*   Copyright (C) 2016 Senquack (dansilsby <AT> gmail <DOT> com)          *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
***************************************************************************/

#ifndef GPU_UNAI_GPU_H
#define GPU_UNAI_GPU_H

/* Port note (Xbox 360 / pcsxr-360):
 *
 * Upstream has TWO `gpu.h` files: this one (private to gpu_unai) and a
 * sibling in `../gpulib/gpu.h` that holds the gpulib state + global
 * `struct gpu`.  In the pcsxr-360 port we DO NOT use gpulib; the
 * driver bridge in gpu_unai_driver.cpp wires unai directly to the
 * pcsxr-360 GP0 stream, and `gpu_unai.vram` is set to point at our
 * shared `psxVuw` buffer.  The gpulib `struct gpu` and its globals
 * (gpu.state, gpu.cmd_lengths, etc.) are reimplemented in the bridge
 * where needed and skipped where not.
 *
 * The only thing this header exposes is gpu_unai_config_t — the user-
 * facing config struct that lets the frontend toggle lighting,
 * dithering, blending, etc.  Nothing in here references gpulib.
 */

#include <stdint.h>

struct gpu_unai_config_t {
	uint8_t pixel_skip:1;     /* If 1, allows skipping rendering pixels that
	                             would not be visible when a high horizontal
	                             resolution PS1 video mode is set.
	                             Only applies to devices with low resolutions
	                             like 320x240. Should not be used if a
	                             down-scaling framebuffer blitter is in use.
	                             Can cause gfx artifacts if game reads VRAM
	                             to do framebuffer effects. */

	uint8_t ilace_force:3;    /* Option to force skipping rendering of lines,
	                             for very slow platforms. Value will be
	                             assigned to 'ilace_mask' in gpu_unai struct.
	                             Normally 0. Value '1' will skip rendering
	                             odd lines. */

	uint8_t lighting:1;
	uint8_t fast_lighting:1;
	uint8_t blending:1;
	uint8_t dithering:1;
	uint8_t force_dithering:1;

	/* old_renderer is the pre-2016 (pre-senquack) standalone gpu_unai.
	 * The pcsxr-360 port does NOT include the old renderer (we'd have
	 * to bring in `old/`, which doubles the codebase for marginal
	 * benefit on a system fast enough for the modern path).  The flag
	 * is retained for ABI compatibility but `old_renderer = 1` causes
	 * the driver to fall back to the modern path with a log warning. */
	uint8_t old_renderer:1;
};

/* Filled in by the frontend's check_game_fixes / check_gpu_renderer
 * code paths in libretro_core.cpp.  Read by renderer_init() and copied
 * into gpu_unai.config.  See gpu_unai_driver.cpp for hot-reload
 * semantics.
 *
 * C linkage so the symbol matches the definition in
 * gpu_unai_driver.cpp (which is wrapped in `extern "C" {}`).  Without
 * the linkage qualifier MSVC mangles the name and the linker can't
 * resolve it from libretro_core.cpp's `extern "C"` declaration. */
#ifdef __cplusplus
extern "C" {
#endif
extern struct gpu_unai_config_t gpu_unai_config_ext;
#ifdef __cplusplus
}
#endif

#endif /* GPU_UNAI_GPU_H */
