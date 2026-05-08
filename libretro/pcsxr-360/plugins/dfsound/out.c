/* Output-driver glue for the cycle-driven SPU port.
 *
 * The SPU plugin keeps an `out_current` pointer; pcsx_rearmed picks
 * one of OSS/ALSA/SDL/Pulse/none at SetupSound() time.  In our build
 * (Xbox 360 + libretro frontend) there is a single driver wired to
 * SoundFeedStreamData (libretro_core.cpp), which writes to the SPSC
 * audio ring drained by retro_run via audio_batch_cb. */

#include <stdio.h>
#include <stdlib.h>
#include "out.h"

/* SoundFeedStreamData and pcsxr_audio_ring_reset are C-linked,
 * defined in libretro_core.cpp.  pcsxr_audio_ring_reset zeroes the
 * SPSC audio ring's read/write positions; the legacy SetupSound the
 * frontend used to define is now this plus driver selection (below). */
extern void SoundFeedStreamData(unsigned char *pSound, long lBytes);
extern void pcsxr_audio_ring_reset(void);

static int  libretro_init(void)   { pcsxr_audio_ring_reset(); return 0; }
static void libretro_finish(void) { }
static int  libretro_busy(void)   { return 0; }
static void libretro_feed(void *data, int bytes) {
	SoundFeedStreamData((unsigned char *)data, (long)bytes);
}

static struct out_driver out_libretro = {
	"libretro", libretro_init, libretro_finish, libretro_busy, libretro_feed,
};

struct out_driver *out_current;

void SetupSound(void) {
	if (out_libretro.init() == 0)
		out_current = &out_libretro;
}
