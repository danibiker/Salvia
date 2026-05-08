#ifndef __P_OUT_H__
#define __P_OUT_H__

/* Output-driver abstraction (port from pcsx_rearmed).
 *
 * The cycle-driven SPU pushes mixed samples through `out_current->feed`.
 * En pcsxr-360 / Xbox 360 hay exactamente un driver: un thin wrapper
 * sobre SoundFeedStreamData (libretro_core.cpp), que reenvia el batch
 * directamente a audio_batch_cb del frontend.  El frontend mantiene su
 * propio buffer de audio.
 *
 * `init` returns 0 on success.
 * `finish` is called from SPUclose.
 * `busy` is used by spu_config.iTempo (disabled in this port).
 * `feed(data, bytes)` reenvia interleaved 16-bit stereo PCM al frontend.
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
