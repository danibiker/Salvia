#ifndef __P_SPU_CONFIG_H__
#define __P_SPU_CONFIG_H__

/* SPU runtime configuration (port from pcsx_rearmed).
 *
 * iVolume:           master volume (0-1024+, 1024 = 100 %).
 * iXAPitch:          unused on this port (legacy).
 * iUseReverb:        0 disables reverb, 1 enables.
 * iUseInterpolation: 0 = none, 1 = linear, 2 = gauss, 3 = cubic.
 * iTempo:            unused on this port (rate-control hack for slow
 *                    output drivers; libretro uses RetroArch's audio
 *                    sync, not this).
 * iUseThread:        worker-thread mixing.  Disabled in this port —
 *                    the cycle-driven model already runs the SPU
 *                    inline with the CPU emulator. */
typedef struct
{
 int        iVolume;
 int        iXAPitch;
 int        iUseReverb;
 int        iUseInterpolation;
 int        iTempo;
 int        iUseThread;
} SPUConfig;

extern SPUConfig spu_config;

#endif /* __P_SPU_CONFIG_H__ */
