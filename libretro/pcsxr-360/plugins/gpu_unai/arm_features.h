/* arm_features.h — Xbox 360 PPC32 stub.
 *
 * Replacement for pcsx_rearmed/include/arm_features.h.  Upstream
 * detects ARM architecture revision (HAVE_ARMV5/6/7/8, HAVE_NEON32) and
 * defines GCC-asm helpers (FUNCTION/ESYM/etc.).  None of that applies
 * to Xbox 360 PowerPC: this stub leaves all HAVE_ARM* undefined so
 * unai falls back to its generic C paths, and the asm helpers are not
 * needed because no .S file is being compiled in this port.
 *
 * Included by gpu_inner.h before any ARM-specific include guard.
 */
#ifndef __ARM_FEATURES_H__
#define __ARM_FEATURES_H__

/* Intentionally empty — leaving HAVE_ARMV* undefined disables all
 * ARM-specific asm paths in gpu_inner.h, gpu_inner_blend.h, and
 * gpu_inner_light.h.  Generic C versions (gpuBlendingGeneric,
 * gpuLightingTXTGeneric, etc.) are selected via the `#ifndef
 * gpuBlending` blocks in those headers. */

#endif /* __ARM_FEATURES_H__ */
