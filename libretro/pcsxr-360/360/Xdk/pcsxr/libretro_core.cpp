/*
 * libretro_core.cpp - Libretro core implementation for PCSXR-360
 *
 * Single-threaded model using Win32 Fibers (cooperative coroutines):
 *
 *   retro_run()  <--SwitchToFiber-->  EmuFiberProc
 *       (main fiber)                   (emu fiber)
 *
 * retro_run() switches to the emulator fiber, which runs until VBlank
 * (SysUpdate -> libretro_frame_sync), then switches back.  All libretro
 * callbacks (video_cb, audio_batch_cb) are called from retro_run() on the
 * frontend thread.
 *
 * The SPU runs on its own dedicated thread (iUseTimer=0, core 4) for
 * performance.  Audio samples are sent directly to the frontend via
 * audio_batch_cb from SoundFeedStreamData (no intermediate buffering).
 *
 * The PPC dynarec and all core emulation code remain untouched.
 */

#include <xtl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "libretro.h"

extern "C" {
#include "psxcommon.h"
#include "r3000a.h"
#include "cdriso.h"
#include "gpu.h"
#include "plugins.h"
#include "misc.h"
#include "psxmem.h"
}

#include "xbPlugins.h"

/* ===== CRT stub ===== */
extern "C" int __cdecl _chvalidator(int c, int mask)
{
    extern const unsigned short *_pctype;
    return (_pctype[(unsigned char)c] & mask);
}

/* ===== Libretro callbacks ===== */
static retro_environment_t        environ_cb;
static retro_video_refresh_t      video_cb;
static retro_audio_sample_t       audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t         input_poll_cb;
static retro_input_state_t        input_state_cb;

/* ===== Fiber handles ===== */
static LPVOID fiber_main = NULL;   /* retro_run context (frontend thread) */
static LPVOID fiber_emu  = NULL;   /* emulator context */
static bool   emu_running = false;
static bool   emu_thread_exited = false;
static bool   emu_initialized = false;

/* ===== Video state ===== */
extern "C" unsigned char *pPsxScreen;
extern "C" unsigned int   g_pPitch;
extern "C" int            g_useRGB565;
static int display_width  = 320;
static int display_height = 240;
static unsigned current_pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;

/* ===== Audio ring buffer (SPSC lock-free) =====
 *
 * Single producer (SPU thread via SoundFeedStreamData) and single
 * consumer (emu fiber via retro_run drain).  We use a power-of-two
 * ring indexed by unsigned 32-bit positions; count = wpos - rpos
 * (wrap-safe as long as count <= AUDIO_BUF_SAMPLES).
 *
 * Synchronization on PPC (weak memory model):
 *   Producer: write payload -> __lwsync() -> publish wpos
 *   Consumer: read wpos    -> __lwsync() -> read payload
 *             read payload -> __lwsync() -> publish rpos
 *
 * No critical section, no memmove.
 */
#define AUDIO_BUF_SAMPLES  32768
#define AUDIO_BUF_MASK     (AUDIO_BUF_SAMPLES - 1)
static __declspec(align(128)) int16_t  audio_buf[AUDIO_BUF_SAMPLES];
static __declspec(align(128)) volatile uint32_t audio_wpos = 0;  /* producer */
static __declspec(align(128)) volatile uint32_t audio_rpos = 0;  /* consumer */

/* Secondary buffer for drain: max 1 frame of audio + margin */
#define AUDIO_DRAIN_MAX    4096
static int16_t audio_drain_buf[AUDIO_DRAIN_MAX];
static long    audio_drain_count = 0;

/* ===== Input state ===== */
static uint16_t libretro_pad_state[2];
static uint8_t  libretro_analog[2][4];   /* [port][lx, ly, rx, ry] */

/* ===== Game path storage ===== */
static char game_path_store[1024];

#define PATH_MAX_LENGTH 4096

/* ======================================================================
 * LIBRETRO CALLBACK SETTERS
 * ====================================================================== */

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    struct retro_variable variables[] = {
        { "pcsxr360_pixel_format", "Pixel Format; RGB565|XRGB8888" },
        { NULL, NULL }
    };
    cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}

void retro_set_video_refresh(retro_video_refresh_t cb)      { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)        { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb)            { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb)          { input_state_cb = cb; }

/* ======================================================================
 * PIXEL FORMAT
 * ====================================================================== */

static void check_pixel_format(void) {
    struct retro_variable var = { "pcsxr360_pixel_format", NULL };

    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "RGB565") == 0) {
            current_pixel_format = RETRO_PIXEL_FORMAT_RGB565;
            g_useRGB565 = 1;
        } else {
            current_pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
            g_useRGB565 = 0;
        }
    }

    if (environ_cb)
        environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &current_pixel_format);
}

/* ======================================================================
 * SYSTEM INFO
 * ====================================================================== */

unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name     = "PCSXR-360";
    info->library_version  = "2.1.1";
    info->valid_extensions = "bin|cue|img|mdf|pbp|cbn|iso";
    info->need_fullpath    = true;
    info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    info->geometry.base_width   = 320;
    info->geometry.base_height  = 240;
    info->geometry.max_width    = 1024;
    info->geometry.max_height   = 512;
    info->geometry.aspect_ratio = 4.0f / 3.0f;

    info->timing.fps         = (Config.PsxType == PSX_TYPE_PAL) ? 50.0 : 60.0;
    info->timing.sample_rate = 44100.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
    (void)port;
    (void)device;
}

/* ======================================================================
 * AUDIO CAPTURE - Overrides audio.lib's XAudio2 output
 * ====================================================================== */

extern "C" void SetupSound(void) {
    audio_wpos = 0;
    audio_rpos = 0;
}

extern "C" void RemoveSound(void) {
}

extern "C" unsigned long SoundGetBytesBuffered(void) {
    /* Count via unsigned subtraction is wrap-safe for SPSC rings. */
    uint32_t count = audio_wpos - audio_rpos;
    return (unsigned long)(count * sizeof(int16_t));
}

extern "C" void SoundFeedStreamData(unsigned char *pSound, long lBytes) {
    if (!pSound || lBytes <= 0)
        return;

    uint32_t samples = (uint32_t)(lBytes / (long)sizeof(int16_t));

    uint32_t wpos   = audio_wpos;              /* own variable, plain read */
    uint32_t rpos   = audio_rpos;              /* other side's pos (acquire not needed: worst case we see fewer free slots) */
    uint32_t free_s = AUDIO_BUF_SAMPLES - (wpos - rpos);
    if (samples > free_s)
        samples = free_s;
    if (samples == 0)
        return;

    uint32_t w = wpos & AUDIO_BUF_MASK;
    uint32_t first = AUDIO_BUF_SAMPLES - w;
    if (first > samples) first = samples;

    memcpy(&audio_buf[w], pSound, first * sizeof(int16_t));
    if (samples > first) {
        memcpy(&audio_buf[0],
               pSound + first * sizeof(int16_t),
               (samples - first) * sizeof(int16_t));
    }

    __lwsync();                                /* release: payload before wpos */
    audio_wpos = wpos + samples;
}

/* ======================================================================
 * INPUT ROUTING
 * ====================================================================== */

static const struct { int retro_id; int psx_bit; } button_map[] = {
    { RETRO_DEVICE_ID_JOYPAD_B,      14 }, /* Cross */
    { RETRO_DEVICE_ID_JOYPAD_A,      13 }, /* Circle */
    { RETRO_DEVICE_ID_JOYPAD_Y,      15 }, /* Square */
    { RETRO_DEVICE_ID_JOYPAD_X,      12 }, /* Triangle */
    { RETRO_DEVICE_ID_JOYPAD_SELECT,  0 }, /* Select */
    { RETRO_DEVICE_ID_JOYPAD_START,   3 }, /* Start */
    { RETRO_DEVICE_ID_JOYPAD_UP,      4 }, /* D-Up */
    { RETRO_DEVICE_ID_JOYPAD_RIGHT,   5 }, /* D-Right */
    { RETRO_DEVICE_ID_JOYPAD_DOWN,    6 }, /* D-Down */
    { RETRO_DEVICE_ID_JOYPAD_LEFT,    7 }, /* D-Left */
    { RETRO_DEVICE_ID_JOYPAD_L,      10 }, /* L1 */
    { RETRO_DEVICE_ID_JOYPAD_R,      11 }, /* R1 */
    { RETRO_DEVICE_ID_JOYPAD_L2,      8 }, /* L2 */
    { RETRO_DEVICE_ID_JOYPAD_R2,      9 }, /* R2 */
    { RETRO_DEVICE_ID_JOYPAD_L3,      1 }, /* L3 */
    { RETRO_DEVICE_ID_JOYPAD_R3,      2 }, /* R3 */
};
#define BUTTON_MAP_SIZE (sizeof(button_map) / sizeof(button_map[0]))

static void poll_libretro_input(void) {
    if (!input_poll_cb || !input_state_cb)
        return;

    input_poll_cb();

    for (int port = 0; port < 2; port++) {
        uint16_t buttons = 0xFFFF;

        for (unsigned i = 0; i < BUTTON_MAP_SIZE; i++) {
            if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, button_map[i].retro_id))
                buttons &= ~(1 << button_map[i].psx_bit);
        }

        libretro_pad_state[port] = buttons;

        int16_t lx = input_state_cb(port, RETRO_DEVICE_ANALOG,
                                     RETRO_DEVICE_INDEX_ANALOG_LEFT,
                                     RETRO_DEVICE_ID_ANALOG_X);
        int16_t ly = input_state_cb(port, RETRO_DEVICE_ANALOG,
                                     RETRO_DEVICE_INDEX_ANALOG_LEFT,
                                     RETRO_DEVICE_ID_ANALOG_Y);
        int16_t rx = input_state_cb(port, RETRO_DEVICE_ANALOG,
                                     RETRO_DEVICE_INDEX_ANALOG_RIGHT,
                                     RETRO_DEVICE_ID_ANALOG_X);
        int16_t ry = input_state_cb(port, RETRO_DEVICE_ANALOG,
                                     RETRO_DEVICE_INDEX_ANALOG_RIGHT,
                                     RETRO_DEVICE_ID_ANALOG_Y);

        libretro_analog[port][0] = (uint8_t)((lx / 256) + 128);
        libretro_analog[port][1] = (uint8_t)((ly / 256) + 128);
        libretro_analog[port][2] = (uint8_t)((rx / 256) + 128);
        libretro_analog[port][3] = (uint8_t)((ry / 256) + 128);
    }
}

extern "C" void libretro_get_pad_state(int port, uint16_t *buttons,
                                        uint8_t *lx, uint8_t *ly,
                                        uint8_t *rx, uint8_t *ry) {
    *buttons = libretro_pad_state[port];
    *lx = libretro_analog[port][0];
    *ly = libretro_analog[port][1];
    *rx = libretro_analog[port][2];
    *ry = libretro_analog[port][3];
}

/* ======================================================================
 * FRAME SYNC - Fiber yield point
 * ====================================================================== */

extern "C" void libretro_update_display_size(int w, int h) {
    display_width  = w;
    display_height = h;
}

extern "C" void libretro_frame_sync(void) {
    if (!emu_running || !fiber_main)
        return;

    SwitchToFiber(fiber_main);
}

static retro_log_printf_t pcsxr_log_cb = NULL;

void pcsxr_log_set_cb(retro_log_printf_t log_cb)
{
   pcsxr_log_cb = log_cb;
}

static bool string_is_empty(const char *data)
{
   return !data || (*data == '\0');
}

void pcsxr_log(enum retro_log_level level, const char *format, ...)
{
   char msg[512];
   va_list ap;

   msg[0] = '\0';

   if (string_is_empty(format))
      return;

   va_start(ap, format);
   vsprintf(msg, format, ap);
   va_end(ap);

   if (pcsxr_log_cb)
      pcsxr_log_cb(level, "[pcsxr] %s", msg);
   else
      fprintf((level == RETRO_LOG_ERROR) ? stderr : stdout,
            "[Gambatte] %s", msg);
}

/* ======================================================================
 * EMULATOR FIBER
 * ====================================================================== */

static void CALLBACK EmuFiberProc(LPVOID param) {
    int ret, res;

    OutputDebugStringA("[PCSXR-LR] EmuFiberProc: start\n");

    XMemSet(&Config, 0, sizeof(PcsxConfig));

    Config.PsxOut  = 1;
    Config.HLE     = 0;
    Config.Xa      = 0;
    Config.Cdda    = 0;
    Config.PsxAuto = 1;
    Config.CpuBias = 2;
    Config.Cpu     = CPU_DYNAREC;

	
	const char *system_dir = NULL;
	if (!environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) ||
       !system_dir)
	{
      pcsxr_log(RETRO_LOG_WARN, "No system directory defined, unable to look for '%s'.\n", "SCPH1001.BIN");
	}

    strcpy(Config.Bios, "SCPH1001.BIN");
    strcpy(Config.BiosDir, system_dir);

	char full_path[PATH_MAX_LENGTH];

	
	const char *save_dir = NULL;
	if (!environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) ||
       !save_dir)
	{
      pcsxr_log(RETRO_LOG_WARN, "No save directory defined, unable to look for '%s'.\n", "Memcard1.mcd");
	}
	strcpy(Config.Mcd1, save_dir);
	strcat(Config.Mcd1, "\\");
	strcat(Config.Mcd1, "Memcard1.mcd");

    strcpy(Config.Mcd2, save_dir);
	strcat(Config.Mcd2, "\\");
	strcat(Config.Mcd2, "Memcard2.mcd");

    cdrIsoInit();
    SetIsoFile(game_path_store);
    gpuDmaThreadInit();

    OutputDebugStringA("[PCSXR-LR] Calling SysInit...\n");
    if (SysInit() == -1) {
        OutputDebugStringA("[PCSXR-LR] SysInit FAILED\n");
        emu_thread_exited = true;
        SwitchToFiber(fiber_main);
        return;
    }
    OutputDebugStringA("[PCSXR-LR] SysInit OK\n");

    gpuThreadEnable(1);
    GPU_clearDynarec(clearDynarec);

    ret = CDR_open();
    if (ret < 0) { OutputDebugStringA("[PCSXR-LR] CDR_open FAILED\n"); emu_thread_exited = true; SwitchToFiber(fiber_main); return; }
    OutputDebugStringA("[PCSXR-LR] CDR_open OK\n");

    ret = GPU_open(NULL);
    if (ret < 0) { OutputDebugStringA("[PCSXR-LR] GPU_open FAILED\n"); emu_thread_exited = true; SwitchToFiber(fiber_main); return; }
    OutputDebugStringA("[PCSXR-LR] GPU_open OK\n");

    ret = SPU_open(NULL);
    if (ret < 0) { OutputDebugStringA("[PCSXR-LR] SPU_open FAILED\n"); emu_thread_exited = true; SwitchToFiber(fiber_main); return; }
    OutputDebugStringA("[PCSXR-LR] SPU_open OK\n");

    SPU_registerCallback(SPUirq);

    ret = PAD1_open(NULL);
    if (ret < 0) { OutputDebugStringA("[PCSXR-LR] PAD1_open FAILED\n"); emu_thread_exited = true; SwitchToFiber(fiber_main); return; }
    ret = PAD2_open(NULL);
    if (ret < 0) { OutputDebugStringA("[PCSXR-LR] PAD2_open FAILED\n"); emu_thread_exited = true; SwitchToFiber(fiber_main); return; }
    OutputDebugStringA("[PCSXR-LR] PADs OK\n");

    res = CheckCdrom();
    OutputDebugStringA("[PCSXR-LR] CheckCdrom done\n");
    SysReset();
    res = LoadCdrom();
    OutputDebugStringA("[PCSXR-LR] LoadCdrom done\n");

    emu_initialized = true;

    SwitchToFiber(fiber_main);

    OutputDebugStringA("[PCSXR-LR] Entering psxCpu->Execute()\n");
    Config.CpuRunning = 1;
    psxCpu->Execute();

    OutputDebugStringA("[PCSXR-LR] Execute returned\n");
    emu_thread_exited = true;
    SwitchToFiber(fiber_main);
}

/* ======================================================================
 * RETRO CORE API
 * ====================================================================== */

void retro_init(void) {
    emu_running       = false;
    emu_thread_exited = false;
    emu_initialized   = false;
    fiber_main        = NULL;
    fiber_emu         = NULL;
    check_pixel_format();
}

void retro_deinit(void) {
    if (fiber_emu) {
        DeleteFiber(fiber_emu);
        fiber_emu = NULL;
    }
    if (fiber_main) {
        ConvertFiberToThread();
        fiber_main = NULL;
    }
}

bool retro_load_game(const struct retro_game_info *game) {
    if (!game || !game->path)
        return false;

    strncpy(game_path_store, game->path, sizeof(game_path_store) - 1);
    game_path_store[sizeof(game_path_store) - 1] = '\0';

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	XSetThreadProcessor(GetCurrentThread(), 1);

    fiber_main = ConvertThreadToFiber(NULL);
    if (!fiber_main) {
        OutputDebugStringA("[PCSXR-LR] ConvertThreadToFiber FAILED\n");
        return false;
    }

    fiber_emu = CreateFiber(1024 * 1024, EmuFiberProc, NULL);
    if (!fiber_emu) {
        OutputDebugStringA("[PCSXR-LR] CreateFiber FAILED\n");
        ConvertFiberToThread();
        fiber_main = NULL;
        return false;
    }

    emu_running       = true;
    emu_thread_exited = false;
    emu_initialized   = false;

    SwitchToFiber(fiber_emu);

    if (emu_thread_exited) {
        OutputDebugStringA("[PCSXR-LR] Emu init failed inside fiber\n");
        DeleteFiber(fiber_emu);
        fiber_emu = NULL;
        ConvertFiberToThread();
        fiber_main = NULL;
        emu_running = false;
        return false;
    }

    OutputDebugStringA("[PCSXR-LR] retro_load_game: init complete, ready\n");
    return true;
}

void retro_unload_game(void) {
    if (!emu_running)
        return;

    Config.CpuRunning = 0;
    emu_running = false;

    if (fiber_emu && !emu_thread_exited) {
        SwitchToFiber(fiber_emu);
    }

    SysClose();

    if (fiber_emu) {
        DeleteFiber(fiber_emu);
        fiber_emu = NULL;
    }
}

void retro_run(void) {
    if (emu_thread_exited) {
        static bool reported = false;
        if (!reported) {
            OutputDebugStringA("[PCSXR-LR] retro_run: emu exited, sending black frames\n");
            reported = true;
        }
        if (video_cb)
            video_cb(NULL, display_width, display_height, 0);
        return;
    }

    /* 0. Check for hot-changed variables */
    {
        bool updated = false;
        if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
            check_pixel_format();
        }
    }

    /* 1. Poll input */
    poll_libretro_input();

    /* 4. Run the emulator for one frame*/
    SwitchToFiber(fiber_emu);

    /* 5. Send video frame to frontend.
     *    Always send pPsxScreen — on skipped frames it still contains
     *    the last rendered frame, which is fine (implicit dupe). */
    if (video_cb && pPsxScreen) {
        video_cb(pPsxScreen, display_width, display_height, g_pPitch);
    }

    /* 6. Drain audio ring (lock-free SPSC consumer).
     *    Cap the drain to ~1 frame's worth of audio; leftover stays
     *    in the ring for the next frame without any memmove. */
    audio_drain_count = 0;
    {
        //Target samples per frame: 44100Hz * 2ch / fps
        uint32_t max_drain = (Config.PsxType == PSX_TYPE_PAL) ? 1764 : 1470;
        if (max_drain > AUDIO_DRAIN_MAX)
            max_drain = AUDIO_DRAIN_MAX;

        uint32_t wpos = audio_wpos;            /* acquire: read producer's pos */
        __lwsync();                            /* payload reads must come after */
        uint32_t rpos  = audio_rpos;
        uint32_t avail = wpos - rpos;
        uint32_t take  = (avail > max_drain) ? max_drain : avail;

        if (take > 0) {
            uint32_t r = rpos & AUDIO_BUF_MASK;
            uint32_t first = AUDIO_BUF_SAMPLES - r;
            if (first > take) first = take;

            memcpy(audio_drain_buf, &audio_buf[r], first * sizeof(int16_t));
            if (take > first) {
                memcpy(audio_drain_buf + first, &audio_buf[0],
                       (take - first) * sizeof(int16_t));
            }
            audio_drain_count = (long)take;

            __lwsync();                        /* payload read before publishing rpos */
            audio_rpos = rpos + take;
        }
    }
    if (audio_drain_count > 0 && audio_batch_cb) {
		// stereo: 2 samples per frame
        long frames = audio_drain_count / 2;
        if (frames > 0)
            audio_batch_cb(audio_drain_buf, (size_t)frames);
    }
}

void retro_reset(void) {
    SysReset();
}

unsigned retro_get_region(void) {
    return (Config.PsxType == PSX_TYPE_PAL) ? RETRO_REGION_PAL : RETRO_REGION_NTSC;
}

/* ======================================================================
 * SAVE STATES
 * ====================================================================== */

#define SAVESTATE_MAX_SIZE (4 * 1024 * 1024)

size_t retro_serialize_size(void) {
    return SAVESTATE_MAX_SIZE;
}

bool retro_serialize(void *data, size_t size) {
    const char *tmpfile = "game:\\libretro_state.tmp";

    if (SaveState(tmpfile) != 0)
        return false;

    FILE *f = fopen(tmpfile, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if ((size_t)fsize > size) {
        fclose(f);
        return false;
    }

    memset(data, 0, size);
    fread(data, 1, fsize, f);
    fclose(f);

    remove(tmpfile);
    return true;
}

bool retro_unserialize(const void *data, size_t size) {
    const char *tmpfile = "game:\\libretro_state.tmp";

    FILE *f = fopen(tmpfile, "wb");
    if (!f) return false;

    fwrite(data, 1, size, f);
    fclose(f);

    int result = LoadState(tmpfile);
    remove(tmpfile);

    return (result == 0);
}

/* ======================================================================
 * MEMORY ACCESS
 * ====================================================================== */

void *retro_get_memory_data(unsigned id) {
    switch (id) {
        case RETRO_MEMORY_SYSTEM_RAM:
            return psxM_2;
        default:
            return NULL;
    }
}

size_t retro_get_memory_size(unsigned id) {
    switch (id) {
        case RETRO_MEMORY_SYSTEM_RAM:
            return 0x200000;
        default:
            return 0;
    }
}
