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

/* Per-game fix toggles exposed to the frontend as core variables. They
 * live in different compilation units (xbox_soft, dfsound, libpcsxcore)
 * and are applied together by check_game_fixes(). */
extern int      darkforcesfix;       /* xbox_soft/cfg.c */
extern uint32_t dwActFixes;          /* xbox_soft GPU fixes bitmask */
extern int      iUseFixes;           /* xbox_soft gate for dwActFixes */
extern BOOL     tombraider2fix;      /* dfsound/cfg.c */
extern BOOL     crashteamracingfix;  /* dfsound/cfg.c */
extern BOOL     frontmission3fix;    /* libpcsxcore/psxinterpreter.c */
extern int      soul_reaver_quad_fix;/* libpcsxcore/gpu.c — Soul Reaver collapsed-quad workaround */

/* Runtime selector for the new SwanStation-derived SW renderer that
 * lives alongside PEOPS in the xbox_soft plugin. Defined in
 * plugins/gpu_duck/gpu_duck_driver.cpp, read by the GP0 dispatch
 * selector in plugins/xbox_soft/gpu.c. */
extern int      duck_gpu_enabled;
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

static void disk_register_interface(void);

/* ======================================================================
 * LIBRETRO CALLBACK SETTERS
 * ====================================================================== */

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    struct retro_variable variables[] = {
        { "pcsxr360_pixel_format",       "Pixel Format; RGB565|XRGB8888" },
        { "pcsxr360_fix_parasite_eve2",  "Game Fix: Parasite Eve 2 (counter); disabled|enabled" },
        { "pcsxr360_fix_dark_forces",    "Game Fix: Dark Forces / Duke Nukem (GPU); disabled|enabled" },
        { "pcsxr360_fix_ignore_brightness", "GPU Fix: Ignore black brightness; disabled|enabled" },
        { "pcsxr360_fix_lazy_update",    "GPU Fix: Lazy screen update; disabled|enabled" },
        { "pcsxr360_fix_quads_to_tris",  "GPU Fix: Draw quads with triangles; disabled|enabled" },
        { "pcsxr360_fix_soul_reaver_quads", "Game Fix: Soul Reaver collapsed soul quads; disabled|enabled" },
        { "pcsxr360_fix_front_mission3", "Game Fix: Front Mission 3 (CPU); disabled|enabled" },
        { "pcsxr360_fix_tomb_raider2",   "Game Fix: Tomb Raider 2 (SPU); disabled|enabled" },
        { "pcsxr360_fix_crash_t_racing", "Game Fix: Crash Team Racing (SPU); disabled|enabled" },
        { "pcsxr360_slow_boot",          "Slow Boot (show BIOS intro); disabled|enabled" },
        { "pcsxr360_gpu_renderer",       "GPU Renderer (restart core to apply); xbox_soft|gpu_duck" },
        { NULL, NULL }
    };
    cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

    /* Publish the disk-control interface so the frontend can call
     * set_initial_image() before retro_load_game (used for M3U resume). */
    disk_register_interface();
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
 * GAME FIXES
 *
 * Per-game compatibility toggles surfaced as libretro core variables.
 * Each option maps onto one or more globals in xbox_soft / dfsound /
 * libpcsxcore. Safe to call repeatedly (init + hot-reload via
 * RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE).
 * ====================================================================== */

static bool read_bool_var(const char *key, bool defval) {
    struct retro_variable var = { key, NULL };
    if (!environ_cb || !environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value)
        return defval;
    return strcmp(var.value, "enabled") == 0;
}

static void check_game_fixes(void) {
    /* Parasite Eve 2 — root-counter timing fix (psxcounters.c). */
    Config.RCntFix = read_bool_var("pcsxr360_fix_parasite_eve2", false) ? 1 : 0;

    /* Front Mission 3 — MFC2 branch-delay quirk (psxinterpreter.c). */
    frontmission3fix = read_bool_var("pcsxr360_fix_front_mission3", false) ? 1 : 0;

    /* Soul Reaver — rewrite collapsed 0x2E QuadSemiTex primitives in flight
     * (libpcsxcore/gpu.c). The CPU/GTE emulation produces souls with all
     * four vertices identical (zero-area quad); this expands them to a
     * fixed-size sprite around the centre. */
    soul_reaver_quad_fix = read_bool_var("pcsxr360_fix_soul_reaver_quads", false) ? 1 : 0;

    /* Tomb Raider 2 — SPU voice-silence handling (dfsound/spu.c). */
    tombraider2fix = read_bool_var("pcsxr360_fix_tomb_raider2", false) ? 1 : 0;

    /* Crash Team Racing — SPU IRQ mixer polling (dfsound/spu.c). Was a
     * compile-time #ifdef; now a runtime flag. */
    crashteamracingfix = read_bool_var("pcsxr360_fix_crash_t_racing", false) ? 1 : 0;

    /* BIOS Slow Boot — skips ra-shortcut in misc.c so intros play. */
    Config.SlowBoot = read_bool_var("pcsxr360_slow_boot", false) ? 1 : 0;

    /* PEOPS GPU fix bitmask (xbox_soft). Each bit gates a different
     * workaround in prim.c / gpu.c / soft.c. We OR-compose them and
     * gate the whole thing on iUseFixes (see xbox_soft/cfg.c:77). */
    uint32_t gpu_fixes = 0;
    darkforcesfix = read_bool_var("pcsxr360_fix_dark_forces", false) ? 1 : 0;
    if (darkforcesfix)                                                 gpu_fixes |= 0x100; /* Dark Forces / Duke Nukem: repeated flat-tex triangles */
    if (read_bool_var("pcsxr360_fix_ignore_brightness", false))        gpu_fixes |= 0x004; /* Ignore black brightness colour */
    if (read_bool_var("pcsxr360_fix_lazy_update", false))              gpu_fixes |= 0x040; /* Lazy screen update (Soul Reaver souls, LoK) */
    if (read_bool_var("pcsxr360_fix_quads_to_tris", false))            gpu_fixes |= 0x200; /* Draw quads with triangles (geometry) */
    dwActFixes = gpu_fixes;
    iUseFixes  = gpu_fixes ? 1 : 0;

    /* GPU renderer selector. Only read at startup (before SysInit /
     * PEOPS_GPUinit) — swapping primTables mid-game would leave the
     * driver state desynced, so the option label warns the user to
     * restart. Default: PEOPS software (xbox_soft). */
    {
        struct retro_variable var = { "pcsxr360_gpu_renderer", NULL };
        if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
            duck_gpu_enabled = (strcmp(var.value, "gpu_duck") == 0) ? 1 : 0;
        else
            duck_gpu_enabled = 0;
    }
}

/* ======================================================================
 * USER NOTIFICATIONS
 *
 * Bridge for core code (libpcsxcore) to surface user-visible messages via
 * the libretro frontend. Used e.g. from LoadSBI() to warn about missing
 * libcrypt .sbi files. Safe to call before environ_cb is installed
 * (becomes a no-op in that case).
 * ====================================================================== */

extern "C" void pcsxr_lr_notify_user(const char *msg, unsigned frames) {
    if (!environ_cb || !msg) return;
    struct retro_message rmsg;
    rmsg.msg    = msg;
    rmsg.frames = frames ? frames : 600; /* ~10 s @ 60 fps */
    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &rmsg);
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
    info->valid_extensions = "bin|cue|img|mdf|pbp|cbn|iso|chd|m3u";
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
            "[PCSXR] %s", msg);
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

    // Patches dir: <system>\patches\psx  (must end with separator because
    // the core concatenates <PatchesDir><file> without adding one).
    if (system_dir) {
        /* XDK CRT has _snprintf, not C99 snprintf. */
        _snprintf(Config.PatchesDir, sizeof(Config.PatchesDir),
                  "%s\\patches\\psx\\", system_dir);
        Config.PatchesDir[sizeof(Config.PatchesDir) - 1] = '\0';
    } else {
        Config.PatchesDir[0] = '\0';
    }

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

    /* Re-apply game fixes right before SysInit: Config.SlowBoot must be
     * set before the BIOS shortcut runs, and the GPU/SPU globals must
     * match the user's choice for this boot. */
    check_game_fixes();

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
 * DISK CONTROL (multi-disc M3U support)
 *
 * Exposes the libretro disk-control (+ EXT) interface so the frontend can
 * swap discs at runtime on multi-disc PSX games (MGS, FF7, FF8, FF9, etc.).
 *
 * Swap cycle:
 *   1. Frontend -> set_eject_state(true)
 *      We call SetCdOpenCaseTime(-1) (shell permanently open) and close
 *      the backing ISO (CDR_close).  The PSX BIOS/CD driver fires the lid
 *      interrupt -> games show "Please insert disc N".
 *   2. Frontend -> set_image_index(n)    (only valid while ejected)
 *      We record the new index.
 *   3. Frontend -> set_eject_state(false)
 *      We SetIsoFile(images[n]) + CDR_open(), then SetCdOpenCaseTime
 *      (now+2) so the lid closes shortly; game reads the new TOC and
 *      resumes.
 * ====================================================================== */

#define DISK_MAX_IMAGES  16
#define DISK_PATH_MAX    1024
#define DISK_LABEL_MAX   256

static char     disk_images[DISK_MAX_IMAGES][DISK_PATH_MAX];
static char     disk_labels[DISK_MAX_IMAGES][DISK_LABEL_MAX];
static unsigned disk_count        = 0;
static unsigned disk_current      = 0;
static bool     disk_ejected      = false;
static unsigned disk_initial_idx  = 0;
static char     disk_initial_path[DISK_PATH_MAX];

/* Derive a short label (basename without extension) from a path. */
static void disk_derive_label(char *out, const char *path, size_t outlen) {
    if (!out || outlen == 0) return;
    out[0] = '\0';
    if (!path) return;

    const char *base = path;
    for (const char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\' || *p == ':')
            base = p + 1;
    }

    size_t i = 0;
    while (base[i] && i + 1 < outlen && base[i] != '.') {
        out[i] = base[i];
        ++i;
    }
    /* If we stopped at an extension dot, check whether it's really the ext
     * (last dot).  If there's another dot later, keep going.  Simpler: we
     * accept first-dot truncation; file names like "Game Disc 1.bin" work. */
    out[i] = '\0';
}

static bool disk_path_is_absolute(const char *p) {
    if (!p || !p[0]) return false;
    /* Xbox 360 absolute: "hdd1:\...", "game:\...", "d:\..." — colon present */
    for (const char *q = p; *q; ++q) {
        if (*q == ':') return true;
        if (*q == '/' || *q == '\\') return (q == p); /* leading slash = absolute */
    }
    return false;
}

/* Directory (with trailing separator) part of a path, written into `out`. */
static void disk_dirname(char *out, size_t outlen, const char *path) {
    size_t lastsep = 0, i;
    out[0] = '\0';
    if (!path) return;
    for (i = 0; path[i]; ++i) {
        if (path[i] == '/' || path[i] == '\\' || path[i] == ':')
            lastsep = i + 1;
    }
    if (lastsep == 0) return;
    if (lastsep >= outlen) lastsep = outlen - 1;
    memcpy(out, path, lastsep);
    out[lastsep] = '\0';
}

/* Parse an M3U file and populate disk_images / disk_labels.
 * Returns the number of entries added (0 on failure). */
static unsigned disk_parse_m3u(const char *m3u_path) {
    FILE *f = fopen(m3u_path, "rb");
    if (!f) return 0;

    char dir[DISK_PATH_MAX];
    disk_dirname(dir, sizeof(dir), m3u_path);

    disk_count = 0;

    char line[DISK_PATH_MAX];
    while (disk_count < DISK_MAX_IMAGES && fgets(line, sizeof(line), f)) {
        /* Strip CR/LF and trailing whitespace */
        size_t len = strlen(line);
        while (len > 0 &&
               (line[len - 1] == '\n' || line[len - 1] == '\r' ||
                line[len - 1] == ' '  || line[len - 1] == '\t')) {
            line[--len] = '\0';
        }
        /* Strip leading whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') ++p;
        /* Skip empty / comments / playlist directives */
        if (*p == '\0' || *p == '#') continue;

        char full[DISK_PATH_MAX];
        if (disk_path_is_absolute(p)) {
            strncpy(full, p, sizeof(full) - 1);
            full[sizeof(full) - 1] = '\0';
        } else {
            /* XDK CRT has _snprintf, not C99 snprintf — just build it
             * manually with bounds checks. */
            size_t dl = strlen(dir);
            size_t pl = strlen(p);
            if (dl >= sizeof(full)) dl = sizeof(full) - 1;
            memcpy(full, dir, dl);
            if (dl + pl >= sizeof(full)) pl = sizeof(full) - 1 - dl;
            memcpy(full + dl, p, pl);
            full[dl + pl] = '\0';
        }

        strncpy(disk_images[disk_count], full, DISK_PATH_MAX - 1);
        disk_images[disk_count][DISK_PATH_MAX - 1] = '\0';
        disk_derive_label(disk_labels[disk_count], full, DISK_LABEL_MAX);
        disk_count++;
    }

    fclose(f);
    return disk_count;
}

/* Case-insensitive suffix check. */
static bool disk_path_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t ls = strlen(s), lx = strlen(suffix);
    if (lx > ls) return false;
    for (size_t i = 0; i < lx; ++i) {
        char a = s[ls - lx + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return false;
    }
    return true;
}

/* ---------- libretro disk control callbacks ---------- */

static bool dc_set_eject_state(bool ejected) {
    if (ejected == disk_ejected)
        return true;

    if (ejected) {
        /* Open the shell permanently until we insert */
        SetCdOpenCaseTime((s64)-1);
        if (CDR_close) CDR_close();
    } else {
        /* Swap the backing file, then close the shell shortly */
        if (disk_current < disk_count && disk_images[disk_current][0]) {
            SetIsoFile(disk_images[disk_current]);
            if (CDR_open) CDR_open();
        }
        /* +2 s keeps the lid-open window long enough for the BIOS to see
         * both states; games treat the close transition as a fresh TOC. */
        SetCdOpenCaseTime((s64)time(NULL) + 2);
    }

    disk_ejected = ejected;
    return true;
}

static bool dc_get_eject_state(void) {
    return disk_ejected;
}

static unsigned dc_get_image_index(void) {
    return disk_current;
}

static bool dc_set_image_index(unsigned index) {
    /* Spec: only valid while ejected.  Allow index >= count ("no disc"). */
    if (!disk_ejected) return false;
    disk_current = index;
    return true;
}

static unsigned dc_get_num_images(void) {
    return disk_count;
}

static bool dc_replace_image_index(unsigned index, const struct retro_game_info *info) {
    if (index >= disk_count) return false;

    if (info == NULL) {
        /* Remove entry `index`, shifting the rest down */
        for (unsigned i = index; i + 1 < disk_count; ++i) {
            memcpy(disk_images[i], disk_images[i + 1], sizeof(disk_images[0]));
            memcpy(disk_labels[i], disk_labels[i + 1], sizeof(disk_labels[0]));
        }
        disk_count--;
        if (disk_current >= disk_count && disk_count > 0)
            disk_current = disk_count - 1;
        return true;
    }
    if (!info->path) return false;

    strncpy(disk_images[index], info->path, DISK_PATH_MAX - 1);
    disk_images[index][DISK_PATH_MAX - 1] = '\0';
    disk_derive_label(disk_labels[index], info->path, DISK_LABEL_MAX);
    return true;
}

static bool dc_add_image_index(void) {
    if (disk_count >= DISK_MAX_IMAGES) return false;
    disk_images[disk_count][0] = '\0';
    disk_labels[disk_count][0] = '\0';
    disk_count++;
    return true;
}

static bool dc_set_initial_image(unsigned index, const char *path) {
    if (!path) return false;
    disk_initial_idx = index;
    strncpy(disk_initial_path, path, sizeof(disk_initial_path) - 1);
    disk_initial_path[sizeof(disk_initial_path) - 1] = '\0';
    return true;
}

static bool dc_get_image_path(unsigned index, char *path, size_t len) {
    if (!path || len == 0 || index >= disk_count) return false;
    strncpy(path, disk_images[index], len - 1);
    path[len - 1] = '\0';
    return true;
}

static bool dc_get_image_label(unsigned index, char *label, size_t len) {
    if (!label || len == 0 || index >= disk_count) return false;
    strncpy(label, disk_labels[index], len - 1);
    label[len - 1] = '\0';
    return true;
}

static void disk_register_interface(void) {
    if (!environ_cb) return;

    static const struct retro_disk_control_ext_callback dc_ext = {
        dc_set_eject_state,
        dc_get_eject_state,
        dc_get_image_index,
        dc_set_image_index,
        dc_get_num_images,
        dc_replace_image_index,
        dc_add_image_index,
        dc_set_initial_image,
        dc_get_image_path,
        dc_get_image_label
    };
    static const struct retro_disk_control_callback dc_basic = {
        dc_set_eject_state,
        dc_get_eject_state,
        dc_get_image_index,
        dc_set_image_index,
        dc_get_num_images,
        dc_replace_image_index,
        dc_add_image_index
    };

    if (!environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, (void*)&dc_ext))
        environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, (void*)&dc_basic);
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
    check_game_fixes();
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

    /* ---- Disk list population --------------------------------------- */
    disk_count   = 0;
    disk_current = 0;
    disk_ejected = false;

    if (disk_path_ends_with(game->path, ".m3u")) {
        if (disk_parse_m3u(game->path) == 0) {
            OutputDebugStringA("[PCSXR-LR] M3U parse failed or empty\n");
            return false;
        }
        /* Honor frontend's set_initial_image only if its cached path
         * matches the game we were actually asked to load.  Otherwise
         * the user edited the M3U and a wrong index would boot the
         * wrong disc — spec says fall back to 0 in that case. */
        if (disk_initial_idx < disk_count &&
            disk_initial_path[0] &&
            strcmp(disk_initial_path, game->path) == 0) {
            disk_current = disk_initial_idx;
        }
    } else {
        /* Single disc image — build a one-entry list. */
        strncpy(disk_images[0], game->path, DISK_PATH_MAX - 1);
        disk_images[0][DISK_PATH_MAX - 1] = '\0';
        disk_derive_label(disk_labels[0], game->path, DISK_LABEL_MAX);
        disk_count   = 1;
        disk_current = 0;
    }

    /* Seed the CDR with whatever image we'll boot from. */
    strncpy(game_path_store, disk_images[disk_current], sizeof(game_path_store) - 1);
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
            check_game_fixes();
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

/*
 * Savestate buffer sizing (worst-case, uncompressed):
 *   header (37) + thumb 128*96*3 (36864)
 *   + psxM_2 2 MiB (0x00200000)
 *   + psxR_2 512 KiB (0x00080000)
 *   + psxH_2 64 KiB  (0x00010000)
 *   + psxRegs (~8.5 KiB — ICache included)
 *   + GPUFreeze_t (1 MiB VRAM + header)
 *   + SPU size(4) + SPUFreeze_t (cSPURam 0x80000 + cSPUPort + xa) + SPUOSSFreeze_t (s_chan[MAXCHAN])
 *   + sio/cdr/hw/rcnt/mdec (< 32 KiB)
 * Sum ~4.3 MiB.  8 MiB gives comfortable headroom for any future struct
 * growth (cdr, SPU channel counts, GPU plugin versions).  The frontend
 * handles persistence — no disk I/O from the library.
 */
#define SAVESTATE_MAX_SIZE (8 * 1024 * 1024)

size_t retro_serialize_size(void) {
    return SAVESTATE_MAX_SIZE;
}

bool retro_serialize(void *data, size_t size) {
    if (!data || size == 0) return false;
    size_t used = 0;
    if (SaveStateMem(data, size, &used) != 0) return false;
    /* Zero the tail so hashes/compression of the returned buffer are
     * deterministic — not required by libretro but cheap and useful. */
    if (used < size)
        memset((uint8_t*)data + used, 0, size - used);
    return true;
}

bool retro_unserialize(const void *data, size_t size) {
    if (!data || size == 0) return false;
    return (LoadStateMem(data, size) == 0);
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
