/* Minimal headless libretro frontend used to compare two builds of the core.
 * Prints a hash of every video frame / audio batch so that an original
 * (C99) build and the converted (C89/MSVC2010) build can be diffed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include "libretro.h"

static uint64_t vhash = 1469598103934665603ULL, ahash = 1469598103934665603ULL;
static uint64_t fvhash;
static unsigned frame = 0;
static int verbose = 0;
static const char* sysdir = NULL;

static uint64_t fnv(uint64_t h, const void* p, size_t n) {
	const uint8_t* b = (const uint8_t*) p;
	size_t i;
	for (i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ULL; }
	return h;
}

static void video_cb(const void* data, unsigned w, unsigned h, size_t pitch) {
	unsigned y;
	fvhash = 1469598103934665603ULL;
	if (!data) return;
	for (y = 0; y < h; ++y) fvhash = fnv(fvhash, (const uint8_t*) data + y * pitch, w * 2);
	vhash = fnv(vhash, &fvhash, sizeof(fvhash));
}
static void audio_cb(int16_t l, int16_t r) { int16_t s[2] = { l, r }; ahash = fnv(ahash, s, 4); }
static size_t audio_batch_cb(const int16_t* d, size_t f) { ahash = fnv(ahash, d, f * 4); return f; }
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
	/* deterministic pseudo-input: press buttons on a schedule */
	unsigned phase = frame % 120;
	if (port || device != RETRO_DEVICE_JOYPAD) return 0;
	if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
		int16_t m = 0;
		unsigned b;
		for (b = 0; b < 16; ++b) if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, b)) m |= 1 << b;
		return m;
	}
	switch (id) {
	case RETRO_DEVICE_ID_JOYPAD_START: return phase >= 30 && phase < 34;
	case RETRO_DEVICE_ID_JOYPAD_A: return phase >= 60 && phase < 64;
	case RETRO_DEVICE_ID_JOYPAD_RIGHT: return phase >= 80 && phase < 90;
	case RETRO_DEVICE_ID_JOYPAD_DOWN: return phase >= 90 && phase < 100;
	case RETRO_DEVICE_ID_JOYPAD_B: return phase >= 100 && phase < 104;
	}
	return 0;
}
static void log_cb(enum retro_log_level level, const char* fmt, ...) {
	va_list ap;
	if (!verbose) return;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
static bool env_cb(unsigned cmd, void* data) {
	switch (cmd) {
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		return *(enum retro_pixel_format*) data == RETRO_PIXEL_FORMAT_RGB565;
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		((struct retro_log_callback*) data)->log = log_cb;
		return true;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		*(const char**) data = sysdir;
		return sysdir != NULL;
	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
		return true;
	case RETRO_ENVIRONMENT_GET_VARIABLE:
		return false;
	default:
		return false;
	}
}

static unsigned char* readfile(const char* p, size_t* n) {
	FILE* f = fopen(p, "rb");
	unsigned char* b;
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
	b = malloc(*n);
	if (fread(b, 1, *n, f) != *n) { free(b); fclose(f); return NULL; }
	fclose(f);
	return b;
}

int main(int argc, char** argv) {
	struct retro_game_info gi;
	struct retro_system_av_info av;
	size_t n = 0, ss;
	unsigned frames = argc > 2 ? atoi(argv[2]) : 600, i;
	unsigned char* rom;
	void* state;
	uint64_t h1, h2;
	if (argc < 2) { fprintf(stderr, "usage: harness rom [frames]\n"); return 1; }
	verbose = getenv("VERBOSE") != NULL;
	rom = readfile(argv[1], &n);
	if (!rom) { fprintf(stderr, "cannot read rom\n"); return 1; }
	retro_set_environment(env_cb);
	retro_set_video_refresh(video_cb);
	retro_set_audio_sample(audio_cb);
	retro_set_audio_sample_batch(audio_batch_cb);
	retro_set_input_poll(input_poll_cb);
	retro_set_input_state(input_state_cb);
	retro_init();
	memset(&gi, 0, sizeof(gi));
	gi.path = argv[1]; gi.data = rom; gi.size = n;
	if (!retro_load_game(&gi)) { fprintf(stderr, "load failed\n"); return 1; }
	retro_get_system_av_info(&av);
	printf("av %ux%u fps=%.4f rate=%.1f\n", av.geometry.base_width, av.geometry.base_height, av.timing.fps, av.timing.sample_rate);
	for (frame = 0; frame < frames; ++frame) {
		retro_run();
		if (frame % 100 == 99) printf("frame %u v=%016llx a=%016llx\n", frame + 1, (unsigned long long) vhash, (unsigned long long) ahash);
	}
	/* savestate round trip */
	ss = retro_serialize_size();
	state = malloc(ss);
	if (!retro_serialize(state, ss)) printf("serialize failed\n");
	for (i = 0; i < 60; ++i, ++frame) retro_run();
	h1 = fvhash;
	if (!retro_unserialize(state, ss)) printf("unserialize failed\n");
	frame -= 60;
	for (i = 0; i < 60; ++i, ++frame) retro_run();
	h2 = fvhash;
	printf("state size=%u roundtrip=%s last=%016llx\n", (unsigned) ss, h1 == h2 ? "OK" : "MISMATCH", (unsigned long long) h2);
	printf("final v=%016llx a=%016llx\n", (unsigned long long) vhash, (unsigned long long) ahash);
	if (getenv("DUMP")) {
		/* dump last frame as PPM-ish raw for visual check */
	}
	retro_unload_game();
	retro_deinit();
	free(state);
	free(rom);
	return 0;
}
