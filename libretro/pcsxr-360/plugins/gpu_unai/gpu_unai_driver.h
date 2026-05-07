/*
 * gpu_unai_driver.h
 *
 * Bridge between pcsxr-360's GP0 dispatch surface (xbox_soft/gpu.c +
 * primTableJ) and the gpu_unai software rasteriser ported from PCSX-
 * ReARMed.
 *
 * Mirrors the pattern used by gpu_duck_driver:
 *   - xbox_soft/gpu.c picks `unai_primTable` instead of `primTableJ`
 *     when the libretro option `pcsxr360_gpu_renderer = gpu_unai`
 *     is selected, then dispatches per-command from there.
 *   - VRAM pointer in `gpu_unai.vram` is set to share `psxVuw`, so
 *     xb_video keeps reading the same buffer.  No copy on the hot
 *     path.
 *
 * What this module does:
 *   - Initialises gpu_unai's state (LUTs, dithering, drawing area).
 *   - Owns the global `gpu_unai` instance (defined in the .cpp).
 *   - Translates incoming PSX-format raw command words into the
 *     packet layout gpu_unai's draw functions expect.
 *   - Calls into gpu_command.h / gpu_raster_polygon.h /
 *     gpu_raster_sprite.h / gpu_raster_line.h / gpu_raster_image.h.
 *
 * What this module does NOT do:
 *   - Parse the GP0 accumulator itself — xbox_soft/gpu.c stays
 *     responsible for that.
 *   - Handle GP1 (status writes) — front-end still owns GPUSTAT.
 *   - Drive the display — xb_video reads psxVuw as before.
 */
#pragma once

#include "gpu_unai_compat.h"
#include "gpu_unai.h"

/* ==================================================================
 * C entry points called from xbox_soft/gpu.c when the unai renderer
 * is the active backend.  Lifetime parallels duck_init / duck_shutdown
 * in gpu_duck_driver.h.
 * ================================================================ */

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise / teardown.  Pass the PSX VRAM pointer (psxVuw in
 * xbox_soft).  Idempotent; safe to call multiple times. */
int  unai_init(unsigned short* psx_vram);
void unai_shutdown(void);
void unai_reset(void);

/* Renderer-active flag.  When non-zero, xbox_soft/gpu.c routes its
 * primFunc dispatch through `unai_primTable` instead of `primTableJ`.
 * Set by libretro_core.cpp when the user selects gpu_unai. */
extern int unai_gpu_enabled;

/* Primitive dispatch table — same shape and contract as primTableJ /
 * duck_primTable.  Entries for unhandled opcodes point to a no-op. */
extern void (*const unai_primTable[256])(unsigned char* baseAddr);

/* Optional hooks for runtime config refresh (lighting / blending /
 * dithering toggles).  Frontend calls this when the libretro
 * variables change.  The values come from the global
 * gpu_unai_config_ext. */
void unai_apply_config(void);

#ifdef __cplusplus
}
#endif
