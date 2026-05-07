/*
 * gpu_unai_c_api.h
 *
 * Pure-C surface that xbox_soft/gpu.c includes when the gpu_unai
 * renderer is wired in.  Mirrors gpu_duck_c_api.h exactly so the
 * dispatch logic in gpu.c can pick between PEOPS / gpu_duck /
 * gpu_unai without dragging C++ headers across translation units.
 *
 * The full C++/internal surface lives in gpu_unai_driver.h.
 */
#ifndef GPU_UNAI_C_API_H
#define GPU_UNAI_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the gpu_unai renderer on top of an existing PSX VRAM
 * buffer (xbox_soft's `psxVuw`).  Returns 0 on success.  Must be
 * called from GPUinit before any primitive dispatch. */
int  unai_init(unsigned short* psx_vram);

/* Tear down and free the driver state.  No-op since gpu_unai uses
 * a static state struct, but kept for ABI symmetry with gpu_duck. */
void unai_shutdown(void);

/* Clear internal state and re-init.  Called from PSX reset paths. */
void unai_reset(void);

/* 256-entry GP0 primitive dispatch table.  Signature matches
 * xbox_soft's primTableJ exactly so gpu.c can swap one pointer. */
extern void (*const unai_primTable[256])(unsigned char* baseAddr);

/* Runtime selector.  Non-zero = use unai_primTable; zero = use the
 * original xbox_soft primTableJ (or duck_primTable, depending on
 * duck_gpu_enabled).  Set once by libretro option parsing before any
 * GP0 traffic, read in the GP0 accumulator hot path. */
extern int unai_gpu_enabled;

#ifdef __cplusplus
}
#endif

#endif /* GPU_UNAI_C_API_H */
