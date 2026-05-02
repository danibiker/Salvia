/*
 * peops_prof.h - per-bucket profiling for the PEOPS soft rasteriser.
 *
 * The dispatcher in gpu.c (PEOPS_GPUwriteDataMem) routes every GP0 opcode
 * through primFunc[gpuCommand].  We bracket that single call site with
 * QueryPerformanceCounter so the cost of each rasteriser entry point is
 * attributed to a coarse-grained bucket — chosen to match the families
 * we'd actually optimise (flat vs gouraud, by texture mode 4/8/15-bit,
 * sprites, lines, transfers).
 *
 * The buckets line up with the [PERF] dump emitted from libretro_core.cpp
 * once per ~60-frame window, alongside the existing exec/gpu_wait totals.
 *
 * Counters are written from the GPU helper thread (core 4) and read from
 * retro_run on the frontend thread.  We accept the rare race on a 64-bit
 * write — increments are statistical, not financial; PPC keeps aligned
 * 8-byte stores atomic.  Reset is "read and zero" from retro_run.
 */

#ifndef PEOPS_PROF_H
#define PEOPS_PROF_H

#include <stdint.h>

/* ---- Compile-time switch -------------------------------------------------
 * PCSXR_PERF_ENABLED gates ALL profiling: per-bucket PEOPS counters, the
 * GPU-wait sonda in WaitForGpuThread, and the [PERF]/[PERF/GPU] dump emitted
 * from libretro_core.cpp.  When 0 (default) the compiler eliminates every
 * accumulator, every QueryPerformanceCounter call, every OutputDebugStringA
 * — zero runtime cost.  Flip to 1 from the build configuration to bring
 * the instrumentation back for a debugging session.
 *
 * The flag lives in this header (not its own file) because it's the lowest
 * point in the include graph that every instrumentation TU pulls in:
 * libretro_core.cpp, xbox_soft/gpu.c, peops_prof.c and libpcsxcore/gpu.c
 * all include peops_prof.h, so a single #ifndef/#define keeps the value
 * consistent everywhere with no risk of ODR drift. */
#ifndef PCSXR_PERF_ENABLED
#define PCSXR_PERF_ENABLED 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* Polygons: flat / gouraud, with or without texture, by texture mode. */
    PEOPS_PROF_pF   = 0,   /* flat, untextured (cmds 0x20-0x23, 0x28-0x2B) */
    PEOPS_PROF_pG,         /* gouraud, untextured (0x30-0x33, 0x38-0x3B) */
    PEOPS_PROF_pFT4,       /* flat textured, 4-bit CLUT  (0x24-0x27/0x2C-0x2F + GlobalTextTP=0) */
    PEOPS_PROF_pFT8,       /* flat textured, 8-bit CLUT  (... + TP=1) */
    PEOPS_PROF_pFTD,       /* flat textured, 15-bit direct (... + TP=2) */
    PEOPS_PROF_pGT4,       /* gouraud textured, 4-bit CLUT  (0x34-0x37/0x3C-0x3F + TP=0) */
    PEOPS_PROF_pGT8,       /* gouraud textured, 8-bit CLUT  (... + TP=1) */
    PEOPS_PROF_pGTD,       /* gouraud textured, 15-bit direct (... + TP=2) */

    /* Other primitives. */
    PEOPS_PROF_lin,        /* lines (0x40-0x5F) */
    PEOPS_PROF_spr,        /* sprites/tiles (0x60-0x7F) */

    /* Block ops. */
    PEOPS_PROF_fill,       /* block fill (0x02) */
    PEOPS_PROF_xfer,       /* move/load/store image (0x80/0xA0/0xC0) */

    /* Catch-all for register writes (0xE0-0xE6) and unrecognised commands. */
    PEOPS_PROF_misc,

    PEOPS_PROF_COUNT
};

#if PCSXR_PERF_ENABLED

/* Counters incremented from the GPU helper thread.  Aligned to 8 bytes so
 * 64-bit stores are atomic on PPC.  Volatile for ordering; the producer
 * issues an __lwsync() before publishing in the dispatcher path. */
extern __declspec(align(8)) volatile uint64_t peops_prof_calls[PEOPS_PROF_COUNT];
extern __declspec(align(8)) volatile uint64_t peops_prof_ticks[PEOPS_PROF_COUNT];

/* GlobalTextTP from prim.c: 0 = 4-bit CLUT, 1 = 8-bit CLUT, 2 = 15-bit.
 * Re-declared here with the same type as externals.h (int32_t) so the
 * inline classifier below compiles in any translation unit that includes
 * peops_prof.h, even those that don't pull in externals.h. */
extern int32_t GlobalTextTP;

/* Wrap QueryPerformanceCounter in helpers so callers (e.g. xbox_soft/gpu.c)
 * don't need to drag in <xtl.h> just to bracket a single call.  Definitions
 * are in peops_prof.c. */
uint64_t peops_prof_qpc_now(void);
void     peops_prof_qpc_account(int bucket, uint64_t t0_ticks);

/* Classify a GP0 opcode (top byte of the command word) into a bucket.
 * Inlined into the dispatcher so the cost is just a couple of branches. */
static __inline int peops_prof_classify(unsigned char cmd)
{
    /* Polygons live in 0x20-0x3F.  Bit layout:
     *   bit 4 (0x10) -> gouraud (vs flat)
     *   bit 2 (0x04) -> textured (vs solid)
     *   (bit 3 (0x08) -> 4 vs 3 vertices, but we lump them together) */
    if (cmd >= 0x20 && cmd <= 0x3F) {
        int gouraud  = (cmd & 0x10) != 0;
        int textured = (cmd & 0x04) != 0;
        if (!textured) {
            return gouraud ? PEOPS_PROF_pG : PEOPS_PROF_pF;
        } else {
            int tp = (int)(GlobalTextTP & 3);
            if (tp > 2) tp = 2;   /* TP=3 collapses to 15-bit per UpdateGlobalTP */
            if (gouraud) {
                return PEOPS_PROF_pGT4 + tp;
            } else {
                return PEOPS_PROF_pFT4 + tp;
            }
        }
    }

    /* Lines 0x40-0x5F, sprites/tiles 0x60-0x7F. */
    if (cmd >= 0x40 && cmd <= 0x5F) return PEOPS_PROF_lin;
    if (cmd >= 0x60 && cmd <= 0x7F) return PEOPS_PROF_spr;

    /* Transfer: move-image (0x80), load-image (0xA0), store-image (0xC0). */
    if (cmd == 0x80 || cmd == 0xA0 || cmd == 0xC0) return PEOPS_PROF_xfer;

    /* Block fill is 0x02. */
    if (cmd == 0x02) return PEOPS_PROF_fill;

    /* Register writes and unrecognised commands. */
    return PEOPS_PROF_misc;
}

/* Short labels matching the order of the enum.  Used by the [PERF] dump in
 * libretro_core.cpp to print a one-line summary. */
extern const char *const peops_prof_labels[PEOPS_PROF_COUNT];

#endif /* PCSXR_PERF_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* PEOPS_PROF_H */
