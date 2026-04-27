/*
 * gpu_duck_c_api.h
 *
 * Pure-C surface that xbox_soft's gpu.c can include without dragging in
 * any of the C++ class / template machinery. Contains exactly the
 * symbols the front-end needs to:
 *
 *   - init / reset / shutdown the duck renderer
 *   - dispatch a GP0 primitive through duck_primTable
 *   - toggle interlaced-display state
 *   - read/write a global flag that picks duck vs primTableJ at runtime
 *
 * The full C++ API (GPU_Duck_Driver class) lives in gpu_duck_driver.h.
 */
#ifndef GPU_DUCK_C_API_H
#define GPU_DUCK_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the duck renderer on top of an existing PSX VRAM buffer
 * (xbox_soft's `psxVuw`). Returns 1 on success, 0 on failure. Must be
 * called from GPUinit before any primitive dispatch. */
int  duck_init(unsigned short* psx_vram);

/* Tear down and free the driver + backend. */
void duck_shutdown(void);

/* Clear VRAM and reset internal state. Called from PSX reset paths. */
void duck_reset(void);

/* 256-entry GP0 primitive dispatch table. Signature matches xbox_soft's
 * primTableJ exactly so gpu.c can swap one pointer. */
extern void (*const duck_primTable[256])(unsigned char* baseAddr);

/* Forward the interlaced-display flag + active-line LSB. */
void duck_set_interlaced(int enabled, int active_line_lsb);

/* Runtime selector. Non-zero = use duck_primTable; zero = use the
 * original xbox_soft primTableJ. Set once by libretro option parsing
 * (before any GP0 traffic) and read in the GP0 accumulator hot path.
 *
 * Defined in gpu_duck_driver.cpp. */
extern int duck_gpu_enabled;

#ifdef __cplusplus
}
#endif

#endif /* GPU_DUCK_C_API_H */
