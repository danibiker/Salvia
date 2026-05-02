/*
 * peops_prof.c - storage and QPC helpers for the PEOPS soft-rasteriser
 * profiling buckets.  See peops_prof.h for the per-bucket model and the
 * inline classifier.
 *
 * Counters live here (not in gpu.c) so the storage is shared with any
 * future helper that needs to read/reset them — currently only the
 * libretro [PERF] dump in libretro_core.cpp.
 *
 * The QPC helpers (peops_prof_qpc_now / peops_prof_qpc_account) live
 * here so the rest of xbox_soft (gpu.c, prim.c, ...) doesn't have to
 * pull in <xtl.h> just to bracket the dispatcher call site.
 */

#include "peops_prof.h"

#if PCSXR_PERF_ENABLED

#include <xtl.h>

__declspec(align(8)) volatile uint64_t peops_prof_calls[PEOPS_PROF_COUNT] = { 0 };
__declspec(align(8)) volatile uint64_t peops_prof_ticks[PEOPS_PROF_COUNT] = { 0 };

uint64_t peops_prof_qpc_now(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)t.QuadPart;
}

void peops_prof_qpc_account(int bucket, uint64_t t0_ticks)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    peops_prof_ticks[bucket] += (uint64_t)t.QuadPart - t0_ticks;
    peops_prof_calls[bucket]++;
}

const char *const peops_prof_labels[PEOPS_PROF_COUNT] = {
    "pF",     /* PEOPS_PROF_pF */
    "pG",     /* PEOPS_PROF_pG */
    "pFT4",   /* PEOPS_PROF_pFT4 */
    "pFT8",   /* PEOPS_PROF_pFT8 */
    "pFTD",   /* PEOPS_PROF_pFTD */
    "pGT4",   /* PEOPS_PROF_pGT4 */
    "pGT8",   /* PEOPS_PROF_pGT8 */
    "pGTD",   /* PEOPS_PROF_pGTD */
    "lin",    /* PEOPS_PROF_lin */
    "spr",    /* PEOPS_PROF_spr */
    "fill",   /* PEOPS_PROF_fill */
    "xfer",   /* PEOPS_PROF_xfer */
    "misc"    /* PEOPS_PROF_misc */
};

#else  /* !PCSXR_PERF_ENABLED */

/* TU intentionally empty when profiling is disabled.  Some compilers warn
 * on a fully empty translation unit; emit a single typedef so the file
 * still produces an object. */
typedef int peops_prof_disabled_marker;

#endif /* PCSXR_PERF_ENABLED */
