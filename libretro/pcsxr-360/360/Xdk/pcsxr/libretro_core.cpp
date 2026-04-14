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
 * performance.  Audio samples are accumulated in a shared buffer protected
 * by a CRITICAL_SECTION and drained by retro_run() each frame.
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

/* ===== Audio buffer (shared between SPU thread and main thread) =====
 *
 * The SPU thread (iUseTimer=0) checks SoundGetBytesBuffered() > TESTSIZE
 * to decide when to sleep.  TESTSIZE = 24192 bytes in the SPU plugin.
 * Our buffer MUST be larger than TESTSIZE/sizeof(int16_t) = 12096 samples
 * so the throttle mechanism actually engages.  We use 32768 samples
 * (~371ms at 44100 Hz stereo) which gives plenty of headroom.
 *
 * retro_run drains a capped amount per frame (~1 frame's worth of audio)
 * and shifts the remainder, keeping the flow smooth to the frontend.
 */
#define AUDIO_BUF_SAMPLES  32768
static int16_t audio_buf[AUDIO_BUF_SAMPLES];
static volatile long audio_buf_pos = 0;
static CRITICAL_SECTION audio_cs;
static bool audio_cs_init = false;

/* Secondary buffer for drain: max 1 frame of audio + margin */
#define AUDIO_DRAIN_MAX    4096
static int16_t audio_drain_buf[AUDIO_DRAIN_MAX];
static long    audio_drain_count = 0;

/* High-resolution timer for auto frameskip */
static LARGE_INTEGER perf_freq;

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
    /* Initialize the critical section for thread-safe audio buffer access */
    if (!audio_cs_init) {
        InitializeCriticalSection(&audio_cs);
        audio_cs_init = true;
    }
    audio_buf_pos = 0;
}

extern "C" void RemoveSound(void) {
    if (audio_cs_init) {
        DeleteCriticalSection(&audio_cs);
        audio_cs_init = false;
    }
}

extern "C" unsigned long SoundGetBytesBuffered(void) {
    /* Return the actual buffered amount so the SPU thread can throttle
     * itself (sleeps when buffer > TESTSIZE).  This prevents runaway
     * audio production while keeping the pipeline fed. */
    long pos = audio_buf_pos;   /* volatile read, good enough for throttle */
    return (unsigned long)(pos * sizeof(int16_t));
}

extern "C" void SoundFeedStreamData(unsigned char *pSound, long lBytes) {
    if (!pSound || lBytes <= 0)
        return;

    long samples = lBytes / (long)sizeof(int16_t);

    EnterCriticalSection(&audio_cs);
    long space = AUDIO_BUF_SAMPLES - audio_buf_pos;
    if (samples > space)
        samples = space;
    if (samples > 0) {
        memcpy(&audio_buf[audio_buf_pos], pSound, samples * sizeof(int16_t));
        audio_buf_pos += samples;
    }
    LeaveCriticalSection(&audio_cs);
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
    QueryPerformanceFrequency(&perf_freq);
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
    /* Safety: clean up audio CS if SPUclose didn't run */
    if (audio_cs_init) {
        DeleteCriticalSection(&audio_cs);
        audio_cs_init = false;
    }
}

bool retro_load_game(const struct retro_game_info *game) {
    if (!game || !game->path)
        return false;

    strncpy(game_path_store, game->path, sizeof(game_path_store) - 1);
    game_path_store[sizeof(game_path_store) - 1] = '\0';

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
            
        }
    }

    /* 1. Poll input */
    poll_libretro_input();

    SwitchToFiber(fiber_emu);

    /* 5. Send video frame to frontend.
     *    Always send pPsxScreen — on skipped frames it still contains
     *    the last rendered frame, which is fine (implicit dupe). */
    if (video_cb && pPsxScreen) {
        video_cb(pPsxScreen, display_width, display_height, g_pPitch);
    }

    /* 6. Drain audio buffer under lock, then send outside lock.
     *    The SPU thread writes concurrently, so we snapshot under CS.
     *    We cap the drain to ~1 frame's worth of audio to keep the
     *    frontend's audio pipeline smooth.  Leftover stays in the
     *    buffer for the next frame (shifted via memmove). */
    audio_drain_count = 0;
    if (audio_cs_init) {
        /* Target samples per frame: 44100Hz * 2ch / fps */
        long max_drain = (Config.PsxType == PSX_TYPE_PAL) ? 1764 : 1470;
        if (max_drain > AUDIO_DRAIN_MAX)
            max_drain = AUDIO_DRAIN_MAX;

        EnterCriticalSection(&audio_cs);
        if (audio_buf_pos > 0) {
            long to_drain = audio_buf_pos;
            if (to_drain > max_drain)
                to_drain = max_drain;

            memcpy(audio_drain_buf, audio_buf, to_drain * sizeof(int16_t));
            audio_drain_count = to_drain;

            /* Shift remaining samples to front of buffer */
            long remaining = audio_buf_pos - to_drain;
            if (remaining > 0)
                memmove(audio_buf, &audio_buf[to_drain], remaining * sizeof(int16_t));
            audio_buf_pos = remaining;
        }
        LeaveCriticalSection(&audio_cs);
    }
    if (audio_drain_count > 0 && audio_batch_cb) {
        long frames = audio_drain_count / 2;  /* stereo: 2 samples per frame */
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
