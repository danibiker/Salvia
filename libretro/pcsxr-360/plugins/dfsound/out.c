/* Output-driver glue for the cycle-driven SPU port.
 *
 * The SPU plugin keeps an `out_current` pointer; pcsx_rearmed picks
 * one of OSS/ALSA/SDL/Pulse/none at SetupSound() time.  In our build
 * (Xbox 360 + libretro frontend) there is a single driver wired to
 * SoundFeedStreamData (libretro_core.cpp), que pasa los batches de
 * muestras directamente a audio_batch_cb (sin ring intermedio: el
 * frontend tiene su propio buffer). */

#include <stdio.h>
#include <stdlib.h>
#include "out.h"

/* SoundFeedStreamData esta definida en libretro_core.cpp con C linkage. */
extern void SoundFeedStreamData(unsigned char *pSound, long lBytes);

static int  libretro_init(void)   { return 0; }   /* nada que resetear */
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
