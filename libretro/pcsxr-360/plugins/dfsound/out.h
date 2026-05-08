#ifndef __P_OUT_H__
#define __P_OUT_H__

/* Output-driver abstraction (port from pcsx_rearmed).
 *
 * The cycle-driven SPU pushes mixed samples through `out_current->feed`.
 * In pcsxr-360 / Xbox 360 there is exactly one driver: a thin wrapper
 * around the SPSC ring in libretro_core.cpp (audio_buf), drained by
 * retro_run via audio_batch_cb.
 *
 * `init` returns 0 on success.
 * `finish` is called from SPUclose.
 * `busy` is used by spu_config.iTempo (disabled in this port).
 * `feed(data, bytes)` writes interleaved 16-bit stereo PCM to the ring.
 */
struct out_driver {
	const char *name;
	int (*init)(void);
	void (*finish)(void);
	int (*busy)(void);
	void (*feed)(void *data, int bytes);
};

extern struct out_driver *out_current;

void SetupSound(void);

#endif /* __P_OUT_H__ */
