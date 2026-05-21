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

/* === True Color (24-bit shadow framebuffer) ===
 *
 * Buffer paralelo BGR888 al psxVuw que mantiene 8-bit por canal en los
 * pixels pintados por gpu_duck (sin la cuantizacion a 5-bit + dither
 * Bayer del path normal).  Tamano: 1024*512*4 = 2 MB.  Layout: 0x00RRGGBB
 * (R en byte 2, G en byte 1, B en byte 0).
 *
 * - `g_psxVuw24`: puntero al buffer.  NULL si true-color esta off.
 * - `g_pcsxr_true_color_active`: flag de gate.  1 = activo.
 * - `duck_true_color_set_active`: alloca o libera el buffer.  Idempotente.
 *   Llamarlo desde libretro_core.cpp segun pcsxr360_true_color libretro
 *   option y la combinacion renderer/pixel_format activa.
 *
 * El display path (BlitScreen32 en xbox_soft/draw_ok.c) consulta la flag
 * y, si esta ON, lee del shadow con resync defensivo por pixel
 * (quantize(shadow) != vram -> external write -> reexpand from vram). */
extern unsigned int* g_psxVuw24;
extern int  g_pcsxr_true_color_active;
void duck_true_color_set_active(int active);

#ifdef __cplusplus
}
#endif

#endif /* GPU_DUCK_C_API_H */
