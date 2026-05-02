/*
 * libretro_core.cpp - Libretro core implementation for PCSXR-360
 *
 * Execution model (libretro-native):
 *
 *   retro_run()
 *     poll input -> frame_done = 0 -> psxCpu->Execute() -> video_cb / audio_batch_cb
 *
 * The CPU PSX runs synchronously inside retro_run on the frontend thread.
 * EmuUpdate() (called from psxRcntUpdate at VBlank) sets the global
 * `frame_done` flag, and the interpreter / PPC dynarec execute loops
 * exit cleanly so we can deliver one frame per retro_run call.
 *
 * Real parallelism comes from two dedicated Xbox 360 threads with core
 * affinity:
 *   - GPU helper thread (libpcsxcore/gpu.c, core 4): consumer of an SPSC
 *     ring de 128K u32; lo llena chain_enqueue (CPU emulada via
 *     gpuDmaChain) y lo drena gpu_thread_proc invocando GPU_writeDataMem.
 *     Sincronizacion main↔helper mediante ring_drain() en cada acceso
 *     direct desde main.
 *   - SPU MAIN thread (plugins/dfsound/spu.c, core 3): produces audio
 *     samples into audio_buf[] which retro_run drains via audio_batch_cb.
 *
 * Legacy Sys-functions and PluginTable shims used by the rest of the core
 * are kept at the bottom of this file (formerly in 360/Xdk/pcsxr/xb_sys.c).
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
#include "sio.h"
#include "spu.h"
#include "../../plugins/xbox_soft/peops_prof.h"

/* xbPlugins.h declares the static plugin function symbols (PEOPS_*,
 * PAD__*, etc.) without extern-C guards, so it MUST be included inside
 * this extern "C" block.  Otherwise the C++ compiler name-mangles the
 * declarations and the PluginTable initialiser further down references
 * symbols the linker cannot find (the implementations live in C TUs). */
#include "xbPlugins.h"

/* Per-game fix toggles exposed to the frontend as core variables. They
 * live in different compilation units (xbox_soft, dfsound, libpcsxcore)
 * and are applied together by check_game_fixes(). */
extern int      darkforcesfix;       /* xbox_soft/cfg.c */
extern uint32_t dwActFixes;          /* xbox_soft GPU fixes bitmask */
extern int      iUseFixes;           /* xbox_soft gate for dwActFixes */
extern BOOL     tombraider2fix;      /* dfsound/cfg.c */
extern BOOL     crashteamracingfix;  /* dfsound/cfg.c */
extern BOOL     frontmission3fix;    /* libpcsxcore/psxinterpreter.c */
//extern int      collapsed_quad_fix;  /* libpcsxcore/gpu.c — Soul Reaver collapsed-quad workaround */
//extern int      dload_enabled;       /* libpcsxcore/ppc/pR3000A.c — R3000A load-delay-slot peephole */
//extern void     psxRec_setLoadDelay(int enabled); /* toggles dload_enabled + invalidates rec cache */

/* Auto-frameskip — toggleable a runtime via core option.  Cuando esta a 1
 * retro_run mide el tiempo del frame y decide si skipear el render del
 * siguiente, con dos guardas:
 *
 *  1. Cap de 1 skip consecutivo (no encadenamos dos skips).  Evita
 *     stuttering visible.
 *  2. Solo se skipea si el exceso sobre el budget proviene del GPU
 *     thread (gpu_wait grande).  Si el cuello es el dynarec, skipear
 *     no da speedup (primTableSkip solo afecta a la rasterizacion) y
 *     solo introduce el flicker de stipple sin ganancia.  La heuristica
 *     compara gpu_wait_ticks contra el exceso del budget.
 *
 * Estado:
 *  - g_auto_frameskip      : 0/1 desde la core option, leido en check_game_fixes
 *  - s_frame_budget_ticks  : QPC ticks correspondientes a (1 frame + 5% margin),
 *                            re-calculado lazy-init la primera vez con Config.PsxType
 *  - s_skipping_this_frame : si bSkipNextFrame se seteo en la iter anterior,
 *                            esta iter esta saltando el render. Usado para:
 *                              (a) decidir video_cb(NULL) vs video_cb(pPsxScreen)
 *                              (b) cap "no dos skips consecutivos" */
static int           g_auto_frameskip = 0;
static int64_t       s_frame_budget_ticks = 0;
static int           s_skipping_this_frame = 0;
extern "C" void GPU_setSkipNextFrame(int skip);          /* xbox_soft/gpu.c */
extern void pcsxr_log(enum retro_log_level level, const char *format, ...);

/* gpu_wait_ticks viene declarado por gpu.h dentro del extern "C" block. */

/* Runtime selector for the new SwanStation-derived SW renderer that
 * lives alongside PEOPS in the xbox_soft plugin. Defined in
 * plugins/gpu_duck/gpu_duck_driver.cpp, read by the GP0 dispatch
 * selector in plugins/xbox_soft/gpu.c. */
extern int      duck_gpu_enabled;

}

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

/* ===== Controller types (matching pcsx-rearmed convention) ===== */
#define RETRO_DEVICE_PSE_STANDARD  RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_PSE_ANALOG    RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG,  0)
#define RETRO_DEVICE_PSE_DUALSHOCK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG,  1)

/* Per-port PSX controller type (PSE_PAD_TYPE_* values from psemu_plugin_defs.h).
 * Updated by retro_set_controller_port_device; read by PSXInput via libretro_get_pad_type(). */
static int in_type[2];

/* ===== Emulator lifecycle state =====
 * emu_running becomes true after retro_load_game completes successfully and
 * stays true until retro_unload_game.  retro_run gates work on this flag. */
static bool emu_running = false;

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

/* Forward declaration: defined further down alongside the
 * retro_get_memory_* implementations.  Called from retro_load_game
 * after emu_setup() so the descriptors capture live psxM_2/psxH_2/
 * psxR_2 pointers. */
static void set_retro_memmap(void);

/* ======================================================================
 * LIBRETRO CALLBACK SETTERS
 * ====================================================================== */

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    struct retro_variable variables[] = {
        { "pcsxr360_pixel_format",       "Pixel Format; RGB565|XRGB8888" },
        { "pcsxr360_fix_parasite_eve2",  "Game Fix: Parasite Eve 2 (counter); disabled|enabled" },
        { "pcsxr360_fix_dark_forces",    "Game Fix: Dark Forces / Duke Nukem (GPU); disabled|enabled" },
        { "pcsxr360_fix_front_mission3", "Game Fix: Front Mission 3 (CPU); disabled|enabled" },
        { "pcsxr360_fix_tomb_raider2",   "Game Fix: Tomb Raider 2 (SPU); disabled|enabled" },
        { "pcsxr360_fix_crash_t_racing", "Game Fix: Crash Team Racing (SPU); disabled|enabled" },
        { "pcsxr360_fix_ignore_brightness", "GPU Fix: Ignore black brightness; disabled|enabled" },
        { "pcsxr360_fix_lazy_update",    "GPU Fix: Lazy screen update; disabled|enabled" },
        { "pcsxr360_fix_quads_to_tris",  "GPU Fix: Draw quads with triangles; disabled|enabled" },
        //{ "pcsxr360_fix_collapsed_quads", "GPU Fix: Collapsed quads; disabled|enabled" },
        //{ "pcsxr360_load_delay",         "CPU Fix: R3000A load-delay slots (Soul Calibur); enabled|disabled" },
		{ "pcsxr360_slow_boot",          "Slow Boot (show BIOS intro); disabled|enabled" },
        { "pcsxr360_gpu_renderer",       "GPU Renderer (restart core to apply); xbox_soft|gpu_duck" },
        { "pcsxr360_auto_frameskip",     "Auto frameskip (skip render on overload); disabled|enabled" },
        { NULL, NULL }
    };
    cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

    /* Declare supported PSX controller types per port so the frontend can
     * expose a type-selector UI (matching pcsx-rearmed's approach). */
    {
        static const struct retro_controller_description pads[] = {
            { "standard",  RETRO_DEVICE_JOYPAD         },
            { "dualshock", RETRO_DEVICE_PSE_DUALSHOCK  },
            { "analog",    RETRO_DEVICE_PSE_ANALOG      },
            { NULL, 0 }
        };
        static const struct retro_controller_info ports[] = {
            { pads, 3 },
            { pads, 3 },
            { NULL, 0 }
        };
        cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
    }

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
    //collapsed_quad_fix = read_bool_var("pcsxr360_fix_collapsed_quads", false) ? 1 : 0;

    /* R3000A load-delay slot emulation in the PowerPC dynarec. Default
     * ENABLED — Soul Calibur (and a handful of other tight games) rely
     * on the 1-cycle load delay. Peephole keeps the cost negligible
     * (only loads with a true read-in-N+1 hazard pay the deferral).
     * Toggling at runtime invalidates the recompiler cache via
     * psxRec_setLoadDelay; CPU/PSX state is preserved. */
    //psxRec_setLoadDelay(read_bool_var("pcsxr360_load_delay", true) ? 1 : 0);

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
    if (read_bool_var("pcsxr360_fix_lazy_update", false))              gpu_fixes |= 0x040; /* Lazy screen update */
    if (read_bool_var("pcsxr360_fix_quads_to_tris", false))            gpu_fixes |= 0x200; /* Draw quads with triangles (geometry) */
    dwActFixes = gpu_fixes;
    iUseFixes  = gpu_fixes ? 1 : 0;

    /* Auto-frameskip: si el frame anterior se pasó del budget (con un
     * pequeño margen), saltar el render del siguiente.  Se toggle-a a
     * runtime — la lógica vive en retro_run.  Por defecto OFF para no
     * perturbar juegos que ya van fluidos. */
    g_auto_frameskip = read_bool_var("pcsxr360_auto_frameskip", false) ? 1 : 0;

    /* (GPU renderer selector intentionally NOT re-applied here — see
     * check_gpu_renderer_initial_only() and the comment on it. This
     * function is hot-reloaded on RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,
     * but flipping duck_gpu_enabled mid-session without calling
     * duck_init/duck_shutdown leaves s_driver NULL while the dispatch
     * table points at duck handlers, leading to a crash on the very
     * next GP0 packet — exactly the symptom users hit by changing the
     * renderer "in caliente" through the libretro options menu. The
     * variable is sampled once at startup by the initial-only helper
     * below; subsequent changes take effect only after a core restart,
     * matching the option label "(restart core to apply)". */
}

/* Snapshot the GPU renderer choice ONCE at core init, before
 * SysInit / PEOPS_GPUinit creates the duck driver in xbox_soft/gpu.c.
 * Splitting this out of check_game_fixes() prevents the hot-swap crash
 * (s_driver NULL while duck_primTable is in use). */
static void check_gpu_renderer_initial_only(void) {
    struct retro_variable var = { "pcsxr360_gpu_renderer", NULL };
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        duck_gpu_enabled = (strcmp(var.value, "gpu_duck") == 0) ? 1 : 0;
    else
        duck_gpu_enabled = 0;
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
    if (port >= 2) return;
    switch (device) {
        case RETRO_DEVICE_JOYPAD:
        case RETRO_DEVICE_PSE_STANDARD:
            in_type[port] = PSE_PAD_TYPE_STANDARD;   break;
        case RETRO_DEVICE_PSE_DUALSHOCK:
            in_type[port] = PSE_PAD_TYPE_ANALOGPAD;  break;
        case RETRO_DEVICE_PSE_ANALOG:
            in_type[port] = PSE_PAD_TYPE_ANALOGJOY;  break;
        default:
            in_type[port] = PSE_PAD_TYPE_STANDARD;   break;
    }
}

/* Bridge for PSXInput.cpp (C++ code, XInput path) to query the per-port
 * controller type selected by the frontend via retro_set_controller_port_device. */
extern "C" int libretro_get_pad_type(int port) {
    return (port >= 0 && port < 2) ? in_type[port] : PSE_PAD_TYPE_STANDARD;
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
 * DISPLAY SIZE (called from GPU plugin when resolution changes)
 * ====================================================================== */

extern "C" void libretro_update_display_size(int w, int h) {
    display_width  = w;
    display_height = h;
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
   char msg[128];
   va_list ap;

   msg[0] = '\0';

   if (string_is_empty(format))
      return;

   va_start(ap, format);
   /* _vsnprintf en MSVC2010 (no hay vsnprintf C99); -1 al length para
    * dejar sitio al \0 final. */
   _vsnprintf(msg, sizeof(msg) - 1, format, ap);
   msg[sizeof(msg) - 1] = '\0';
   va_end(ap);

   if (pcsxr_log_cb) {
      pcsxr_log_cb(level, "[pcsxr] %s", msg);
   } else {
      /* Fallback cuando el frontend no expone log interface (o aun no
       * se obtuvo).  En Xbox 360 stdout/stderr no van a ningun sitio
       * visible — usamos OutputDebugStringA, que el debugger XDK captura
       * y se muestra en la consola de Visual Studio. */
      char prefixed[576];
      _snprintf(prefixed, sizeof(prefixed) - 1, "[PCSXR][%d] %s",
                (int)level, msg);
      prefixed[sizeof(prefixed) - 1] = '\0';
      OutputDebugStringA(prefixed);
   }
}

/* ======================================================================
 * EMULATOR SETUP
 *
 * Brings the PSX core up to a state where psxCpu->Execute() can be called
 * one-frame-at-a-time from retro_run.  Mirrors what EmuFiberProc used to
 * do in the standalone fiber model — no fibers, no thread spawning here.
 * ====================================================================== */

static int emu_setup(void) {
    int ret;

    pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] emu_setup: start\n");

    XMemSet(&Config, 0, sizeof(PcsxConfig));

    Config.PsxOut  = 1;
    Config.HLE     = 0;
    Config.Xa      = 0;
    Config.Cdda    = 0;
    Config.PsxAuto = 1;
    Config.CpuBias = 2;
    Config.Cpu     = CPU_DYNAREC;

    const char *system_dir = NULL;
    if (!environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) || !system_dir) {
        pcsxr_log(RETRO_LOG_WARN, "No system directory defined, unable to look for '%s'.\n", "SCPH1001.BIN");
    } else {
		pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] system_dir: %s\n", system_dir);
	}

    strcpy(Config.Bios, "SCPH1001.BIN");
    strcpy(Config.BiosDir, system_dir ? system_dir : "");

    /* Patches dir: <system>\patches\psx  (must end with separator because
     * the core concatenates <PatchesDir><file> without adding one). */
    if (system_dir) {
        /* XDK CRT has _snprintf, not C99 snprintf. */
        _snprintf(Config.PatchesDir, sizeof(Config.PatchesDir),
                  "%s\\patches\\psx\\", system_dir);
        Config.PatchesDir[sizeof(Config.PatchesDir) - 1] = '\0';
    } else {
        Config.PatchesDir[0] = '\0';
    }

    const char *save_dir = NULL;
    if (!environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) || !save_dir) {
        pcsxr_log(RETRO_LOG_WARN, "No save directory defined, unable to look for '%s'.\n", "Memcard1.mcd");
    }

    if (save_dir) {
        strcpy(Config.Mcd1, save_dir);
        strcat(Config.Mcd1, "\\");
        strcat(Config.Mcd1, "Memcard1.mcd");
        strcpy(Config.Mcd2, save_dir);
        strcat(Config.Mcd2, "\\");
        strcat(Config.Mcd2, "Memcard2.mcd");
    } else {
        Config.Mcd1[0] = '\0';
        Config.Mcd2[0] = '\0';
    }

	pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] cdrIsoInit\n");
    cdrIsoInit();
	pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] SetIsoFile\n");
    SetIsoFile(game_path_store);
	pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] gpuDmaThreadInit\n");
    gpuDmaThreadInit();

    /* Re-apply game fixes right before EmuInit: Config.SlowBoot must be
     * set before the BIOS shortcut runs, and the GPU/SPU globals must
     * match the user's choice for this boot. */
	pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] check_game_fixes\n");
    check_game_fixes();

    /* Sample the GPU renderer choice ONCE for this boot, before
     * PEOPS_GPUinit (called by EmuInit/LoadPlugins) decides whether to
     * call duck_init().  This is intentionally outside check_game_fixes
     * because that runs on every libretro variable-update notification,
     * and flipping duck_gpu_enabled mid-session would dispatch to the
     * duck primTable while s_driver is still NULL. */
	pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] check_gpu_renderer_initial_only\n");
    check_gpu_renderer_initial_only();

    pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling EmuInit...\n");
    if (EmuInit() == -1) {
        pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] EmuInit FAILED\n");
        return -1;
    }

	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling LoadPlugins...\n");
    if (LoadPlugins() == -1) {
        pcsxr_log(RETRO_LOG_ERROR, "LoadPlugins failed\n");
        return -1;
    }
    LoadMcds(Config.Mcd1, Config.Mcd2);
    pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] EmuInit OK\n");

    /* gpuThreadEnable() era no-op desde el rediseño de threading
     * (libpcsxcore/gpu.c, mayo 2026) — el lifecycle del helper thread
     * lo gestiona Init/Shutdown.  Ya no la llamamos. */
    GPU_clearDynarec(clearDynarec);

	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling CDR_open...\n");
    ret = CDR_open();
    if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] CDR_open FAILED\n"); return -1; }
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling GPU_open...\n");
    ret = GPU_open(NULL);
    if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] GPU_open FAILED\n"); return -1; }
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling SPU_open...\n");
    ret = SPU_open(NULL);
    if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] SPU_open FAILED\n"); return -1; }
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling SPU_registerCallback...\n");
    SPU_registerCallback(SPUirq);
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling PAD1_open...\n");
    ret = PAD1_open(NULL);
    if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PAD1_open FAILED\n"); return -1; }
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling PAD2_open...\n");
    ret = PAD2_open(NULL);
    if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PAD2_open FAILED\n"); return -1; }
    pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PADs OK\n");
	
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling CheckCdrom...\n");
    CheckCdrom();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling EmuReset...\n");
    EmuReset();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling LoadCdrom...\n");
    LoadCdrom();
    pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] LoadCdrom done\n");

    Config.CpuRunning = 1;
    return 0;
}

/* Inverse of emu_setup: closes plugins and shuts down the GPU helper
 * thread.  Mirrors the legacy SysClose() but invoked directly. */
static void emu_teardown(void) {
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] CpuRunning = 0\n");
    Config.CpuRunning = 0;
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] gpuDmaThreadShutdown\n");
    gpuDmaThreadShutdown();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PAD2_close\n");
    PAD2_close();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PAD1_close\n");
    PAD1_close();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] CDR_close\n");
    CDR_close();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] GPU_close\n");
    GPU_close();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] SPU_close\n");
    SPU_close();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] EmuShutdown\n");
    EmuShutdown();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] ReleasePlugins\n");
    ReleasePlugins();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] end emu_teardown\n");
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
 * PERFORMANCE METRICS
 *
 * Per-frame timing instrumentation, dumped once per second via
 * pcsxr_log(RETRO_LOG_DEBUG, (visible in DebugView or attached debug console).
 * Lets you read off where each retro_run frame spends its budget so you
 * can tell whether the CPU PSX, the video callback, the audio drain or
 * the frontend itself is the bottleneck — useful before deciding whether
 * to move psxCpu->Execute() into its own thread.
 *
 * Output line (one per second):
 *   [PERF] 60fr 999ms | exec=14.20 vid=1.10 aud=0.30 sync=0.20 gap=0.60 | fps=60.0 budget=16.40 | aud_buf min=8000 avg=8500 max=8820
 *
 *   exec    = mean ms inside psxCpu->Execute() per frame
 *   vid     = mean ms inside video_cb per frame
 *   aud     = mean ms draining the SPU ring + audio_batch_cb per frame
 *   sync    = mean ms in poll_input + hot-reload checks per frame
 *   gap     = mean ms between retro_run end and next retro_run start
 *             (== frontend-side cost: present, OSD, shaders, audio mix)
 *   fps     = effective frame rate over the 60-frame window
 *   budget  = mean total ms per frame (exec+vid+aud+sync+gap)
 *   aud_buf = stereo-int16 samples queued in the SPU ring at start of
 *             the drain (target ~ frame's worth = 1470 NTSC / 1764 PAL;
 *             min == 0 means the SPU thread underran, audio crackles).
 *
 * The whole block is gated by PCSXR_PERF_ENABLED (defined in
 * plugins/xbox_soft/peops_prof.h, default 0).  When disabled the storage,
 * helpers, and dump are all elided — retro_run does not call QPC, does
 * not accumulate, does not format strings, and does not emit
 * pcsxr_log(RETRO_LOG_DEBUG,.  Zero runtime cost.
 * ====================================================================== */

#if PCSXR_PERF_ENABLED

#define PERF_WINDOW_FRAMES 60

static LARGE_INTEGER perf_freq;
static LARGE_INTEGER perf_window_start;
static LARGE_INTEGER perf_last_frame_end;
static uint64_t      perf_acc_exec, perf_acc_vid, perf_acc_aud, perf_acc_sync, perf_acc_gap;
static uint64_t      perf_acc_gpu_wait;   /* subset of perf_acc_exec: time CPU stalled in WaitForGpuThread */
static uint32_t      perf_aud_min, perf_aud_max;
static uint64_t      perf_aud_sum;
static uint32_t      perf_frame_count;
static uint32_t      perf_core_mask;      /* bitfield of cores retro_run was observed on across the window */

static __inline uint64_t perf_us(LARGE_INTEGER a, LARGE_INTEGER b) {
    /* (delta * 1e6) / freq, in 64-bit ints to avoid overflow at large deltas. */
    return (uint64_t)((b.QuadPart - a.QuadPart) * 1000000ll / perf_freq.QuadPart);
}

static void perf_init(void) {
    QueryPerformanceFrequency(&perf_freq);
    perf_acc_exec = perf_acc_vid = perf_acc_aud = perf_acc_sync = perf_acc_gap = 0;
    perf_acc_gpu_wait = 0;
    perf_aud_sum = 0;
    perf_aud_min = 0xFFFFFFFFu;
    perf_aud_max = 0;
    perf_frame_count = 0;
    perf_core_mask = 0;
    perf_last_frame_end.QuadPart = 0;
    perf_window_start.QuadPart   = 0;
    /* Discard PEOPS bucket counters accumulated during boot/BIOS so the
     * first [PERF/GPU] line reflects gameplay only. */
    for (int i = 0; i < PEOPS_PROF_COUNT; ++i) {
        peops_prof_calls[i] = 0;
        peops_prof_ticks[i] = 0;
    }
}

static void perf_dump(LARGE_INTEGER t_window_end) {
    if (perf_frame_count == 0) return;

    uint64_t total_us = perf_us(perf_window_start, t_window_end);
    uint64_t fc       = perf_frame_count;
    /* Means as hundredths of a millisecond (so we can print N.NN). */
    uint64_t e  = (perf_acc_exec     * 100 / fc) / 1000;
    uint64_t gw = (perf_acc_gpu_wait * 100 / fc) / 1000;  /* subset of e */
    uint64_t v  = (perf_acc_vid      * 100 / fc) / 1000;
    uint64_t a  = (perf_acc_aud      * 100 / fc) / 1000;
    uint64_t s  = (perf_acc_sync     * 100 / fc) / 1000;
    uint64_t g  = (perf_acc_gap      * 100 / fc) / 1000;
    uint64_t budget = e + v + a + s + g;
    /* Effective fps with one decimal; guard against zero-window edge case. */
    uint64_t fps_x10 = total_us ? (fc * 10ull * 1000000ull / total_us) : 0;
    uint64_t aud_avg = perf_aud_sum / fc;

    char buf[384];
    _snprintf(buf, sizeof(buf),
        "[PERF] %ufr %ums | exec=%u.%02u (gpu_wait=%u.%02u) vid=%u.%02u aud=%u.%02u sync=%u.%02u gap=%u.%02u | fps=%u.%u budget=%u.%02u | aud_buf min=%u avg=%u max=%u | cores=0x%02X\n",
        (unsigned)fc, (unsigned)(total_us / 1000),
        (unsigned)(e  / 100), (unsigned)(e  % 100),
        (unsigned)(gw / 100), (unsigned)(gw % 100),
        (unsigned)(v  / 100), (unsigned)(v  % 100),
        (unsigned)(a  / 100), (unsigned)(a  % 100),
        (unsigned)(s  / 100), (unsigned)(s  % 100),
        (unsigned)(g  / 100), (unsigned)(g  % 100),
        (unsigned)(fps_x10 / 10), (unsigned)(fps_x10 % 10),
        (unsigned)(budget / 100), (unsigned)(budget % 100),
        (unsigned)perf_aud_min, (unsigned)aud_avg, (unsigned)perf_aud_max,
        (unsigned)perf_core_mask);
    buf[sizeof(buf) - 1] = '\0';
    pcsxr_log(RETRO_LOG_DEBUG,buf);

    /* Per-bucket PEOPS rasteriser breakdown.  Counters live in
     * plugins/xbox_soft/peops_prof.c and are written by the GPU helper
     * thread inside PEOPS_GPUwriteDataMem.  We exchange-and-zero them
     * here so each [PERF/GPU] line shows just the window's deltas. */
    {
        char gbuf[768];
        int  pos = 0;
        uint64_t total_ticks = 0;

        pos += _snprintf(gbuf + pos, sizeof(gbuf) - pos, "[PERF/GPU] ");
        for (int i = 0; i < PEOPS_PROF_COUNT; ++i) {
            /* "Read and reset".  Producer is the GPU thread, single-writer;
             * we accept the rare race on a 64-bit store — alignment makes
             * the read non-tearing on PPC. */
            uint64_t calls = peops_prof_calls[i];
            uint64_t ticks = peops_prof_ticks[i];
            peops_prof_calls[i] = 0;
            peops_prof_ticks[i] = 0;

            if (calls == 0) continue;

            /* Mean ticks per frame -> microseconds per frame. */
            uint64_t us_per_frame = (ticks * 1000000ull / perf_freq.QuadPart) / fc;
            /* Hundredths of a ms for "%u.%02u" formatting. */
            uint64_t ms_x100 = us_per_frame / 10;
            /* Calls per frame, rounded. */
            uint64_t cpf = (calls + fc / 2) / fc;

            total_ticks += ticks;

            pos += _snprintf(gbuf + pos, sizeof(gbuf) - pos,
                             "%s=%u(%u.%02u) ",
                             peops_prof_labels[i],
                             (unsigned)cpf,
                             (unsigned)(ms_x100 / 100),
                             (unsigned)(ms_x100 % 100));
            if (pos >= (int)sizeof(gbuf) - 32) break;
        }
        if (total_ticks > 0) {
            uint64_t total_us_per_frame = (total_ticks * 1000000ull / perf_freq.QuadPart) / fc;
            uint64_t total_ms_x100 = total_us_per_frame / 10;
            _snprintf(gbuf + pos, sizeof(gbuf) - pos,
                      "| total=%u.%02u ms\n",
                      (unsigned)(total_ms_x100 / 100),
                      (unsigned)(total_ms_x100 % 100));
            gbuf[sizeof(gbuf) - 1] = '\0';
            pcsxr_log(RETRO_LOG_DEBUG,gbuf);
        }
    }

    perf_acc_exec = perf_acc_vid = perf_acc_aud = perf_acc_sync = perf_acc_gap = 0;
    perf_acc_gpu_wait = 0;
    perf_aud_sum  = 0;
    perf_aud_min  = 0xFFFFFFFFu;
    perf_aud_max  = 0;
    perf_frame_count = 0;
    perf_core_mask = 0;
}

#else  /* !PCSXR_PERF_ENABLED */

/* Stub perf_init so existing call sites in retro_init / retro_load_game
 * can be left untouched — the compiler will inline this empty body and
 * elide the call.  retro_run is wrapped at the call sites so it doesn't
 * need a stub for perf_dump or perf_us. */
static __inline void perf_init(void) {}

#endif /* PCSXR_PERF_ENABLED */

/* ======================================================================
 * RETRO CORE API
 * ====================================================================== */

void retro_init(void) {

	/* Obtener el log callback del frontend si lo soporta.  Sin esto,
     * pcsxr_log_cb queda NULL y todos los pcsxr_log() del core caen al
     * fallback (OutputDebugStringA) — funcional pero los mensajes no
     * llegan al log oficial del frontend.  El environ ya esta valido
     * en este punto (retro_set_environment se llamo antes que retro_init). */
    {
        struct retro_log_callback log_cb;
        if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_cb) && log_cb.log) {
            pcsxr_log_set_cb(log_cb.log);
        }
    }

	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] retro_init\n");
    emu_running = false;
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PSE_PAD_TYPE_STANDARD 0\n");
    in_type[0]  = PSE_PAD_TYPE_STANDARD;
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PSE_PAD_TYPE_STANDARD 1\n");
    in_type[1]  = PSE_PAD_TYPE_STANDARD;
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] perf_init\n");
    perf_init();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] check_pixel_format\n");
    check_pixel_format();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] check_game_fixes\n");
    check_game_fixes();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] retro_init finished\n");
}

void retro_deinit(void) {
    /* Nothing persistent across runs — emu_teardown is called from
     * retro_unload_game, which the frontend invokes before retro_deinit. */
}

bool retro_load_game(const struct retro_game_info *game) {
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] retro_load_game\n");

    if (!game || !game->path){
		pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] retro_load_game no game selected\n");
        return false;
	}

    /* Defensiva: si por cualquier motivo el frontend nos llama a load
     * sin haber pasado por unload (o si una carga previa fallo a media
     * ruta sin limpiar bien), hacemos teardown implicito antes de
     * volver a allocar.  Sin esto, cargar varios juegos consecutivamente
     * acumula:
     *   - Threads (GPU helper, SPU MAIN) sin parar.
     *   - Buffers SPU (~1.2 MB), psxVSecure (~2-3 MB) duplicados.
     *   - File handles del ISO previo (cdHandle/subHandle) abiertos.
     * Eventualmente se agota el heap o un CreateThread/malloc devuelve
     * NULL y emu_setup falla silenciosamente -> Salvia se queda
     * mostrando "Cargando..." indefinidamente. */
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] checking emu_running\n");
    if (emu_running) {
        pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] retro_load_game: emu_running=true, doing implicit teardown\n");
        emu_running = false;
        emu_teardown();
    }

    /* ---- Disk list population --------------------------------------- */
    disk_count   = 0;
    disk_current = 0;
    disk_ejected = false;

	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] checking m3u\n");
    if (disk_path_ends_with(game->path, ".m3u")) {
        if (disk_parse_m3u(game->path) == 0) {
            pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] M3U parse failed or empty\n");
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

	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] checking game_path_store\n");
    /* Seed the CDR with whatever image we'll boot from. */
    strncpy(game_path_store, disk_images[disk_current], sizeof(game_path_store) - 1);
    game_path_store[sizeof(game_path_store) - 1] = '\0';

    /* Threading layout note: Bloody Roar 2 [PERF] traces showed that
     * playing with retro_run/GPU/SPU thread affinity does NOT raise the
     * fps ceiling.  The bottleneck in heavy battles is the PEOPS soft
     * rasteriser (consumes ~20 ms per frame's worth of GP0 commands),
     * and any rebalancing of cores just shifts time between the CPU
     * (exec) and the wait (gpu_wait) buckets — the sum stays the same.
     * Real gains would need a faster rasteriser (VMX, batching) or a
     * faster dynarec.  We keep the scheduler's default placement. */

	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] calling emu_setup\n");
    if (emu_setup() != 0) {
        pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] emu_setup FAILED\n");
        emu_teardown();
        return false;
    }

    /* Publish PSX memory descriptors (main RAM, scratchpad, BIOS) to
     * the frontend.  Done after EmuInit so psxM_2 / psxH_2 / psxR_2
     * are all live.  Required for RetroAchievements to see the full
     * PSX bus address space — without it the frontend skips the
     * scratchpad region with "WRAM buffer too small". */
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] calling set_retro_memmap\n");
    set_retro_memmap();

    /* Discard any timings accumulated during boot/BIOS so the [PERF]
     * report only reflects steady-state gameplay frames. */
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] calling perf_init\n");
    perf_init();

    emu_running = true;
    pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] retro_load_game: init complete, ready\n");
    return true;
}

void retro_unload_game(void) {
    if (!emu_running)
        return;

    emu_running = false;
    emu_teardown();
}

void retro_run(void) {
    if (!emu_running) {
        if (video_cb)
            video_cb(NULL, display_width, display_height, 0);
        return;
    }

#if PCSXR_PERF_ENABLED
    LARGE_INTEGER t_start, t_after_sync, t_after_exec, t_after_vid, t_end;
    QueryPerformanceCounter(&t_start);

    /* gap = time the frontend spent between the previous retro_run's
     * return and our entry now (present, OSD, shaders, audio mixer...). */
    if (perf_last_frame_end.QuadPart != 0)
        perf_acc_gap += perf_us(perf_last_frame_end, t_start);
    if (perf_frame_count == 0)
        perf_window_start = t_start;

    /* Record which logical core retro_run is currently scheduled on
     * (bitfield across the [PERF] window) so we can verify the
     * XSetThreadProcessor pin from retro_load_game took effect. */
    {
        DWORD core = GetCurrentProcessorNumber();
        if (core < 32) perf_core_mask |= (1u << core);
    }
#endif

    /* Hot-reload of frontend variables */
    {
        bool updated = false;
        if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
            check_pixel_format();
            check_game_fixes();
        }
    }

    /* Auto-frameskip: capturar si esta iter esta skipeando (decisicion tomada
     * en la iter anterior).  bSkipNextFrame se seteo abajo en la iter
     * pasada via GPU_setSkipNextFrame, asi que primTableSkip ya esta activo
     * para cuando psxCpu->Execute() arranque y se procesen los GP0. */
    int was_skipping_this_frame = s_skipping_this_frame;
    LARGE_INTEGER t_af_start;
    if (g_auto_frameskip) {
        QueryPerformanceCounter(&t_af_start);
        /* Lazy-init del budget la primera vez (depende de Config.PsxType
         * que retro_load_game ya tiene fijado).  +5% margen para no
         * toggle-spam en frames ligeramente sobre budget. */
        if (s_frame_budget_ticks == 0) {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            uint64_t budget_us = (Config.PsxType == PSX_TYPE_PAL) ? 20000 : 16667;
            budget_us += budget_us / 20;   /* +5% */
            s_frame_budget_ticks = (int64_t)((budget_us * (uint64_t)freq.QuadPart) / 1000000ull);
        }
    } else if (was_skipping_this_frame) {
        /* Auto-frameskip se acaba de desactivar; limpiar el flag para que
         * el GPU thread vuelva a render normal en proximas iters. */
        GPU_setSkipNextFrame(0);
        s_skipping_this_frame = 0;
        was_skipping_this_frame = 0;
    }

    poll_libretro_input();
#if PCSXR_PERF_ENABLED
    QueryPerformanceCounter(&t_after_sync);
    perf_acc_sync += perf_us(t_start, t_after_sync);
#endif

    /* Run the emulator for one frame.  The CPU loop (interpreter or
     * dynarec) returns when EmuUpdate sets frame_done at VBlank.
     *
     * gpu_wait_ticks es un contador en libpcsxcore/gpu.c que acumula
     * los QueryPerformanceCounter ticks que la CPU emulada paso bloqueada
     * dentro de WaitForGpuThread esperando a que el GPU helper thread
     * (core 4) vacie su ring.  Lo reseteamos a 0 SIEMPRE (no solo bajo
     * PCSXR_PERF_ENABLED) porque el auto-frameskip lo lee abajo para
     * decidir si el cuello del frame es el GPU thread (skipear ayuda)
     * o el dynarec (skipear no ayuda y solo causa flicker). */
    gpu_wait_ticks = 0;
    frame_done = 0;
    psxCpu->Execute();
#if PCSXR_PERF_ENABLED
    QueryPerformanceCounter(&t_after_exec);
    perf_acc_exec     += perf_us(t_after_sync, t_after_exec);
    perf_acc_gpu_wait += (uint64_t)(gpu_wait_ticks * 1000000ll / perf_freq.QuadPart);
#endif

    /* Auto-frameskip: decision basada en (a) el exceso sobre el budget
     * y (b) la fraccion de ese exceso que se debe a esperas al GPU.
     *
     * Reglas:
     *  - Cap consecutivo: si esta iter ya estaba skipeando, NO encadenar
     *    otro skip (evita stuttering visible).
     *  - Solo skipear si el exceso sobre el budget proviene mayoritaria-
     *    mente del GPU thread.  Skipear ataca el GPU (primTableSkip salta
     *    rasterizacion + gate de BlitScreen32) pero NO el dynarec PPC ni
     *    el GTE en C — esos siguen corriendo igual.  Si el cuello es el
     *    dynarec, skipear seria perdida (flicker de stipple) sin ganancia
     *    (el frame seguiria siendo igual de lento).
     *
     *  Heuristica concreta: skip si gpu_wait >= 50% del exceso sobre
     *  budget.  El 50% es conservador — mejor no skipear que skipear sin
     *  ganancia clara.  Tras el skip, gpu_wait baja a casi 0 en ese frame,
     *  asi que el ahorro real es ~gpu_wait del frame anterior. */
    if (g_auto_frameskip) {
        LARGE_INTEGER t_af_end;
        QueryPerformanceCounter(&t_af_end);
        int64_t elapsed   = t_af_end.QuadPart - t_af_start.QuadPart;
        int64_t over      = elapsed - s_frame_budget_ticks;
        int64_t gpu_wait  = (int64_t)gpu_wait_ticks;

        int next_skip = 0;
        if (!was_skipping_this_frame && over > 0) {
            /* gpu_wait_ticks acumula sobre el frame entero (incluye el
             * wait que disparo el VBlank al final).  Si gpu_wait >= 50%
             * del exceso, asumimos que skipear va a recuperar la mayor
             * parte de ese tiempo.  Sino, el cuello es el dynarec y
             * skipear no ayudaria. */
            if (gpu_wait * 2 >= over)
                next_skip = 1;
        }
        if (next_skip != s_skipping_this_frame)
            GPU_setSkipNextFrame(next_skip);
        s_skipping_this_frame = next_skip;
    }

    /* Send video frame to frontend.
     *  - Frame skipeado: video_cb(NULL) -> el frontend duplica el ultimo,
     *    nos ahorramos el envio.
     *  - Frame normal: video_cb(pPsxScreen) tal cual. */
    if (was_skipping_this_frame) {
        if (video_cb)
            video_cb(NULL, display_width, display_height, 0);
    } else if (video_cb && pPsxScreen) {
        video_cb(pPsxScreen, display_width, display_height, g_pPitch);
    }
#if PCSXR_PERF_ENABLED
    QueryPerformanceCounter(&t_after_vid);
    perf_acc_vid += perf_us(t_after_exec, t_after_vid);
#endif

    /* Drain audio ring (lock-free SPSC consumer).  Cap drain to ~1 frame's
     * worth of audio; leftover stays in the ring for the next frame. */
    audio_drain_count = 0;
    {
        uint32_t max_drain = (Config.PsxType == PSX_TYPE_PAL) ? 1764 : 1470;
        if (max_drain > AUDIO_DRAIN_MAX)
            max_drain = AUDIO_DRAIN_MAX;

        uint32_t wpos = audio_wpos;            /* acquire: read producer's pos */
        __lwsync();                            /* payload reads must come after */
        uint32_t rpos  = audio_rpos;
        uint32_t avail = wpos - rpos;
        uint32_t take  = (avail > max_drain) ? max_drain : avail;

#if PCSXR_PERF_ENABLED
        /* Track how full the SPU ring was when we drained it: avg & extremes
         * tell whether the SPU thread is keeping up (target ~ max_drain). */
        if (avail < perf_aud_min) perf_aud_min = avail;
        if (avail > perf_aud_max) perf_aud_max = avail;
        perf_aud_sum += avail;
#endif

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
        long frames = audio_drain_count / 2;   /* stereo: 2 samples per frame */
        if (frames > 0)
            audio_batch_cb(audio_drain_buf, (size_t)frames);
    }

#if PCSXR_PERF_ENABLED
    QueryPerformanceCounter(&t_end);
    perf_acc_aud      += perf_us(t_after_vid, t_end);
    perf_last_frame_end = t_end;
    perf_frame_count++;

    if (perf_frame_count >= PERF_WINDOW_FRAMES)
        perf_dump(t_end);
#endif
}

void retro_reset(void) {
    EmuReset();
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
 *
 * Two libretro memory regions are exposed:
 *
 *   RETRO_MEMORY_SYSTEM_RAM (0x200000 / 2 MiB)
 *     The PSX main RAM (psxM_2).  Used by RetroAchievements and the
 *     frontend's cheat / state inspectors.  See also the descriptors
 *     published in set_retro_memmap() below — those expose the rest
 *     of the PSX bus address space (scratchpad at 0x1F800000, BIOS
 *     ROM at 0x1FC00000).  Without those descriptors the frontend
 *     falls back to assuming WRAM and scratchpad are contiguous in
 *     the SYSTEM_RAM buffer, which hits the
 *
 *         "WRAM buffer too small: need 2098176, have 2097152"
 *
 *     warning in rcheevos and disables the scratchpad portion of
 *     the PSX memory map.
 *
 *   RETRO_MEMORY_SAVE_RAM (MCD_SIZE / 128 KiB)
 *     Memory-card slot 1 (Mcd1Data).  Frontends that auto-snapshot
 *     SAVE_RAM (e.g. RetroArch's "Save SaveRAM" feature, the
 *     achievement runtime's per-game save state) use this rather
 *     than reaching into the PSX bus.  Mirrors pcsx_rearmed's
 *     handling.
 * ====================================================================== */

void *retro_get_memory_data(unsigned id) {
    switch (id) {
        case RETRO_MEMORY_SYSTEM_RAM:
            return psxM_2;
        case RETRO_MEMORY_SAVE_RAM:
            return Mcd1Data;
        default:
            return NULL;
    }
}

size_t retro_get_memory_size(unsigned id) {
    switch (id) {
        case RETRO_MEMORY_SYSTEM_RAM:
            return 0x200000;
        case RETRO_MEMORY_SAVE_RAM:
            return MCD_SIZE;
        default:
            return 0;
    }
}

/* Publish memory descriptors so the frontend can see the scratchpad
 * and BIOS ROM in addition to the main RAM exposed via
 * RETRO_MEMORY_SYSTEM_RAM.  Pattern mirrors pcsx_rearmed's
 * set_retro_memmap.
 *
 * Each descriptor entry is { flags, ptr, offset, start, select, disconnect, len }:
 *   flags       - RETRO_MEMDESC_SYSTEM_RAM marks the region as RAM
 *                 the frontend may snapshot / inspect.
 *   ptr         - host pointer to the buffer.
 *   offset      - byte offset into ptr where the region starts (0).
 *   start       - address at which the region appears.
 *   select      - address mask used to match addresses to this
 *                 descriptor.  Zero means "simple range match"
 *                 (start..start+len).
 *   disconnect  - bits to clear from a matched address before
 *                 indexing into the buffer (0 — len is power of 2).
 *   len         - region length in bytes.
 *
 * Three regions:
 *   1. Main RAM      psxM_2  @ 0x00000000   (0x200000 = 2 MiB)
 *   2. Scratchpad    psxH_2  @ 0x00200000   (0x000400 = 1 KiB)
 *   3. BIOS ROM      psxR_2  @ 0x1FC00000   (0x080000 = 512 KiB)
 *
 * NOTE on the scratchpad address: rcheevos uses a flat virtual
 * address space when describing PSX memory (consoleinfo.c puts WRAM
 * at 0x000000-0x1FFFFF and scratchpad at 0x200000-0x2003FF).  The
 * Salvia frontend's build_memory_map matches descriptor->start
 * against rcheevos' virtual start_address (not the real bus address
 * 0x1F800000).  So we publish the scratchpad descriptor at virtual
 * 0x00200000 to align with that lookup convention.
 *
 * Other libretro frontends (RetroArch) interpret descriptor->start
 * as the emulated bus address (where pcsx_rearmed publishes
 * scratchpad at 0x1F800000).  This pcsxr-360 fork is built
 * specifically for Salvia, so we adopt Salvia's convention.  If
 * porting to a frontend that expects bus addresses, change the
 * scratchpad descriptor below to start=0x1F800000 + select=0x7ffffc00.
 *
 * BIOS at 0x1FC00000 is published in PSX bus form for completeness;
 * rcheevos doesn't query a BIOS region for PSX so the address
 * convention here doesn't matter for achievements.
 *
 * Called from retro_load_game after EmuInit so all three pointers
 * are valid.  Re-calling on subsequent loads is harmless — pointers
 * don't move once allocated. */
static void set_retro_memmap(void) {
    if (!environ_cb) return;

    static struct retro_memory_descriptor descs[3];
    struct retro_memory_map retromap;

    /* Main RAM: virtual addr 0x00000000 = bus addr 0x00000000 (same
     * for PSX WRAM, no namespace conflict).  Salvia maps WRAM via
     * the SYSTEM_RAM buffer directly so it never reaches this
     * descriptor in practice — kept for frontends that prefer
     * descriptors. */
    descs[0].flags      = RETRO_MEMDESC_SYSTEM_RAM;
    descs[0].ptr        = psxM_2;
    descs[0].offset     = 0;
    descs[0].start      = 0x00000000;
    descs[0].select     = 0;
    descs[0].disconnect = 0;
    descs[0].len        = 0x200000;
    descs[0].addrspace  = NULL;

    /* Scratchpad: published at rcheevos virtual address 0x00200000
     * (the address that comes out of consoleinfo.c's PSX memory map
     * for "Scratchpad RAM").  select=0 + len=0x400 makes Salvia's
     * simple range-match resolve it to psxH_2[0..0x3FF].
     *
     * psxH_2 is 64 KiB total: bytes [0..0x3FF] are scratchpad, the
     * rest is hardware registers — we deliberately don't expose the
     * HW regs as RAM (they have side effects on read). */
    descs[1].flags      = RETRO_MEMDESC_SYSTEM_RAM;
    descs[1].ptr        = psxH_2;
    descs[1].offset     = 0;
    descs[1].start      = 0x00200000;
    descs[1].select     = 0;
    descs[1].disconnect = 0;
    descs[1].len        = 0x000400;
    descs[1].addrspace  = NULL;

    /* BIOS ROM at PSX bus address 0x1FC00000.  Frontends that read
     * descriptors for cheats or save-state augmentation may use it;
     * rcheevos doesn't have a BIOS region for PSX. */
    descs[2].flags      = RETRO_MEMDESC_SYSTEM_RAM;
    descs[2].ptr        = psxR_2;
    descs[2].offset     = 0;
    descs[2].start      = 0x1FC00000;
    descs[2].select     = 0x5ff80000;
    descs[2].disconnect = 0;
    descs[2].len        = 0x080000;
    descs[2].addrspace  = NULL;

    retromap.descriptors     = descs;
    retromap.num_descriptors = sizeof(descs) / sizeof(descs[0]);

    environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &retromap);
}

/* ======================================================================
 * LEGACY Sys* / PluginTable SHIMS
 *
 * The PCSX-R core (libpcsxcore, plugins/) was originally built around the
 * standalone-app model: dynamic plugin .dll loading and a Sys* abstraction
 * over the host environment.  In a libretro core none of that adds value
 * — every plugin is statically linked, and there is no host UI to return
 * to.  We keep just enough of those symbols here so the unmodified core
 * keeps compiling and linking.
 *
 * Equivalent code used to live in 360/Xdk/pcsxr/xb_sys.c, which has been
 * removed.  Migrating it here keeps the libretro entry point and its
 * static deps in a single translation unit.
 * ====================================================================== */

extern "C" {

/* ---- Logging ---- */
void SysPrintf(const char *fmt, ...) {
    char msg[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(msg, sizeof(msg), fmt, args);
    msg[sizeof(msg) - 1] = '\0';
    va_end(args);
    pcsxr_log(RETRO_LOG_DEBUG,msg);
}

void SysMessage(const char *fmt, ...) {
    char msg[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(msg, sizeof(msg), fmt, args);
    msg[sizeof(msg) - 1] = '\0';
    va_end(args);
    pcsxr_log(RETRO_LOG_DEBUG,msg);
}

/* ---- Static plugin "loader" ----
 *
 * libpcsxcore/plugins.c calls SysLoadLibrary("GPU") / SysLoadSym(drv, "GPUinit")
 * to populate function-pointer tables.  We map "library names" to indexes
 * into a static plugins[] table and resolve symbols by name lookup.  No
 * actual DLL loading happens.
 */
PluginTable plugins[] = {
    PLUGIN_SLOT_0,
    PLUGIN_SLOT_1,
    PLUGIN_SLOT_2,
    PLUGIN_SLOT_3,
    PLUGIN_SLOT_4,
    PLUGIN_SLOT_5,
    PLUGIN_SLOT_6,
    PLUGIN_SLOT_7
};

void *SysLoadLibrary(const char *lib) {
    for (int i = 0; i < NUM_PLUGINS; i++) {
        if (plugins[i].lib != NULL && !strcmp(lib, plugins[i].lib))
            return (void*)i;
    }
    return NULL;
}

void *SysLoadSym(void *lib, const char *sym) {
    PluginTable *plugin = plugins + (int)lib;
    for (int i = 0; i < plugin->numSyms; i++) {
        if (plugin->syms[i].sym && !strcmp(sym, plugin->syms[i].sym))
            return plugin->syms[i].pntr;
    }
    return NULL;
}

const char *SysLibError(void) { return NULL; }
void SysCloseLibrary(void *lib) { (void)lib; }

/* ---- Lifecycle / GUI hooks ----
 *
 * SysReset, SysUpdate, SysClose, SysRunGui and ClosePlugins are still
 * referenced by libpcsxcore (debug.c, misc.c, sio.c) and the PPC
 * dynarec (recError).  In libretro the frontend owns the lifecycle:
 *   - SysReset    -> EmuReset (used by recError after a recompiler fault).
 *   - SysUpdate   -> no-op   (frame_done is set directly by EmuUpdate).
 *   - SysClose    -> no-op   (retro_unload_game already calls emu_teardown).
 *   - SysRunGui   -> no-op   (no GUI to return to).
 *   - ClosePlugins-> no-op   (plugin shutdown lives in emu_teardown). */
void SysReset(void) { EmuReset(); }
void SysUpdate(void)    { /* no-op in libretro */ }
void SysClose(void)     { /* no-op in libretro */ }
void SysRunGui(void)    { /* no GUI to return to in libretro */ }
void ClosePlugins(void) { /* handled by emu_teardown() */ }

} /* extern "C" */
