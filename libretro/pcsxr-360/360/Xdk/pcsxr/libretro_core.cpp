/*
 * libretro_core.cpp - Libretro core implementation for PCSXR-360
 *
 * Execution model (libretro-native):
 *
 *   retro_run()
 *     poll input -> frame_done = 0 -> psxCpu->Execute() -> video_cb
 *
 * The CPU PSX runs synchronously inside retro_run on the frontend thread.
 * EmuUpdate() (called from psxRcntUpdate at VBlank) sets the global
 * `frame_done` flag, and the interpreter / PPC dynarec execute loops
 * exit cleanly so we can deliver one frame per retro_run call.
 *
 * Audio path (cycle-driven SPU, no helper thread):
 *   psxRcntUpdate -> SPU_async(cycle, flags=1) -> SoundFeedStreamData ->
 *   audio_batch_cb directamente.  ~12 llamadas por frame NTSC, ~61 frames
 *   estereo por llamada.  El frontend (Salvia) tiene su propio AudioBuffer
 *   de ~93 ms con WriteBlocking + tolerancia a underrun, asi que no
 *   necesitamos un buffer SPSC intermedio en el core.
 *
 * Real parallelism comes from one dedicated Xbox 360 thread with core
 * affinity:
 *   - GPU helper thread (libpcsxcore/gpu.c, core 4): consumer of an SPSC
 *     ring de 128K u32; lo llena chain_enqueue (CPU emulada via
 *     gpuDmaChain) y lo drena gpu_thread_proc invocando GPU_writeDataMem.
 *     Sincronizacion main<->helper mediante ring_drain() en cada acceso
 *     direct desde main.
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
#include "spu.h"  /* SPUirq, SPUschedule (cycle-driven SPU event handlers) */
#include "../../plugins/dfsound/spu_config.h"  /* SPUConfig spu_config */
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
extern int      iUseDither;          /* xbox_soft dither mode (0/1/2) */
extern BOOL     frontmission3fix;    /* libpcsxcore/psxinterpreter.c */

/* Note: tombraider2fix, crashteamracingfix, spuirq, iSPUIRQWait are
 * legacy PEOPS-thread flags. The cycle-driven SPU port (pcsx_rearmed)
 * makes them obsolete:
 *   - Tomb Raider 2 voice silence: handled correctly upstream.
 *   - Crash Team Racing decoded-buffer IRQ: handled correctly upstream.
 *   - SPU IRQ wait: replaced by cycle-correct IRQ delivery via
 *     PSXINT_SPU_IRQ scheduling (no more wait/handshake).
 * The old globals still link (cfg.c stubs them) but nothing reads
 * them inside the new SPU plugin. */
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

/* Toggle global de dithering, leido por los 3 renderers (Unai/Peops/Duck).
 * 1 = respeta el bit de dither que el juego setea en GP1 (comportamiento PSX
 *     original; algunas pantallas se ven con la matriz Bayer 4x4 caracteristica).
 * 0 = fuerza dither off en todas las primitivas (gana fillrate, evita el cuelgue
 *     de Silent Hill por timing GPU desincronizado en el path lento de gpu_unai).
 * Hot-reloadable via check_game_fixes + RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE
 * porque los 3 renderers lo consultan por primitiva, no en init. */
extern "C" int g_pcsxr_dithering = 1;

extern "C" void GPU_setSkipNextFrame(int skip);          /* xbox_soft/gpu.c */
extern void pcsxr_log(enum retro_log_level level, const char *format, ...);

/* gpu_wait_ticks viene declarado por gpu.h dentro del extern "C" block. */

/* Runtime selector for the new SwanStation-derived SW renderer that
 * lives alongside PEOPS in the xbox_soft plugin. Defined in
 * plugins/gpu_duck/gpu_duck_driver.cpp, read by the GP0 dispatch
 * selector in plugins/xbox_soft/gpu.c. */
extern int      duck_gpu_enabled;

/* True Color (24-bit shadow framebuffer) for gpu_duck.  Defined in
 * plugins/gpu_duck/gpu_duck_driver.cpp.  See gpu_duck_c_api.h for the
 * detailed contract.  Called from check_game_fixes() to apply the
 * pcsxr360_true_color libretro option in caliente.  La definicion en
 * gpu_duck_driver.cpp esta dentro de `extern "C" { }`, asi que aqui
 * tenemos que matchearlo para que el linker resuelva el simbolo. */
extern "C" void duck_true_color_set_active(int active);

/* Async CD-ROM prefetch toggle (cdriso_async.c).  Hot-reloadable from
 * the libretro option pcsxr360_cdrom_prefetch.  When OFF, cdra_read()
 * bypasses the ring buffer and goes straight to the sync backend. */
extern "C" void cdra_set_enabled(int enabled);

/* Runtime selector for the gpu_unai SW renderer ported from
 * PCSX-ReARMed.  Defined in plugins/gpu_unai/gpu_unai_driver.cpp.
 * Mutually exclusive with duck_gpu_enabled — the GP0 dispatch
 * selector picks at most one alternate primTable per session. */
extern int      unai_gpu_enabled;
int  unai_init(unsigned short* psx_vram);
void unai_shutdown(void);
void unai_apply_config(void);

/* Mirror the gpu_unai_config_t layout from plugins/gpu_unai/gpu.h.
 * Repeated here so we can populate it from libretro vars without
 * including the gpu_unai header (which would clash with the
 * libpcsxcore gpu.h that this TU already pulls in). */
struct gpu_unai_config_t {
    uint8_t pixel_skip:1;
    uint8_t ilace_force:3;
    uint8_t lighting:1;
    uint8_t fast_lighting:1;
    uint8_t blending:1;
    uint8_t dithering:1;
    uint8_t force_dithering:1;
    uint8_t old_renderer:1;
};
extern struct gpu_unai_config_t gpu_unai_config_ext;

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

/* Modo "BIOS only": el frontend llamo retro_load_game(NULL) para
 * arrancar la consola sin disco.  En este modo:
 *   - emu_setup salta CheckCdrom/LoadCdrom (no hay TOC que leer).
 *   - Force Config.SlowBoot = 1 para que la BIOS pase por su flujo
 *     completo (logo PSX -> shell con MemCard manager + CD player).
 *     Sin SlowBoot, misc.c hace `psxRegs.pc = ra` (shortcut a juego)
 *     y se cuelga porque no hay juego cargado en RAM.
 *   - El disco-virtual queda "ejected": disk_count = 0.
 * El frontend Salvia activa este modo via launchGame("@bios-only"). */
static bool g_boot_bios_only = false;

/* ===== Video state ===== */
extern "C" unsigned char *pPsxScreen;
extern "C" unsigned int   g_pPitch;
extern "C" int            g_useRGB565;
static int display_width  = 320;
static int display_height = 240;
static unsigned current_pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;

/* ===== Audio path =====
 *
 * No mantenemos buffer intermedio en el core.  El SPU (cycle-driven)
 * llama SPU_async(.., flags=1) ~12 veces por frame NTSC (~13 PAL),
 * cada llamada produce ~60 frames estereo.  SoundFeedStreamData los
 * pasa directamente a audio_batch_cb.
 *
 * El frontend (Salvia) mantiene su propio AudioBuffer SPSC con
 * WriteBlocking (~93 ms a 44.1 kHz estereo) que absorbe la jitter y
 * provee backpressure al CPU emulado.  El antiguo ring local
 * audio_buf[] + drenado adaptativo en retro_run quedaron obsoletos
 * cuando se elimino el thread SPU dedicado. */

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
        { "pcsxr360_gpu_renderer",       "GPU Renderer (restart core to apply); Unai|Peops|SwanStation" },
		{ "pcsxr360_threading",          "GPU Thread (restart core to apply); enabled|disabled" },
		{ "pcsxr360_pixel_format",       "Pixel Format; RGB565|XRGB8888" },
        { "pcsxr360_auto_frameskip",     "Auto frameskip (skip render on overload); disabled|enabled" },
        /* Dithering: respeta el bit de dither que el juego setea en GP1.
         * Cuando ON, las primitivas de Gouraud/lighting/blending pasan por el
         * camino con offsets de la matriz Bayer 4x4 y reclamp por canal:
         * imagen mas fiel a la PSX original pero ~15-20% mas fillrate.
         * Cuando OFF, todas las primitivas saltan el dither. Aplica a los 3
         * renderers (Unai, Peops, SwanStation). Cambia en caliente. */
        { "pcsxr360_dithering",          "Dithering (PSX-style color quantization); enabled|disabled" },
        /* True Color (24-bit internal rendering): elimina banding sin dither.
         * Mantiene un framebuffer paralelo 8-bit/canal junto al BGR555 normal.
         * Solo aplica al renderer SwanStation + pixel_format XRGB8888 (los
         * otros renderers ignoran el flag).  Defaults:
         *   - auto: ON si renderer=SwanStation Y pixel_format=XRGB8888
         *   - enabled: forzado ON (si no es SwanStation no surte efecto)
         *   - disabled: forzado OFF
         * Coste: +2MB RAM, ~10-15% mas fillrate por el doble store en
         * ShadePixel. */
        { "pcsxr360_true_color",         "True Color 24-bit (SwanStation + XRGB8888 only); auto|enabled|disabled" },
        /* CD-ROM async prefetch: worker thread pinned to core 2 pre-reads the
         * next 8 sectors after each successful CDR_readTrack into an in-memory
         * ring buffer.  When the game requests one of those sectors, it's
         * served zero-latency from the ring (no chd_read / fread / decompress).
         * Biggest benefit: CHD-compressed games where each cold-cache sector
         * costs 1-5ms of zlib decompression on the PPC -- prefetch hides that
         * behind emulator CPU work.  BIN/CUE gets a small benefit too (disk
         * I/O latency overlap).  Disable if you see crashes or hangs you
         * suspect are CD-related so you can rule it out. */
        { "pcsxr360_cdrom_prefetch",     "CD-ROM async prefetch (core 2 worker); enabled|disabled" },
		{ "pcsxr360_slow_boot",          "Slow Boot (show BIOS intro); disabled|enabled" },
        { "pcsxr360_fix_parasite_eve2",  "Game Fix (PEOPS): Parasite Eve 2 (counter); disabled|enabled" },
        { "pcsxr360_fix_dark_forces",    "Game Fix (PEOPS): Dark Forces / Duke Nukem (GPU); disabled|enabled" },
        { "pcsxr360_fix_front_mission3", "Game Fix (PEOPS): Front Mission 3 (CPU); disabled|enabled" },
        { "pcsxr360_fix_ignore_brightness", "GPU Fix: Ignore black brightness; disabled|enabled" },
        { "pcsxr360_fix_lazy_update",    "GPU Fix: Lazy screen update; disabled|enabled" },
        { "pcsxr360_fix_quads_to_tris",  "GPU Fix: Draw quads with triangles; disabled|enabled" },
        //{ "pcsxr360_load_delay",         "CPU Fix: R3000A load-delay slots (Soul Calibur); enabled|disabled" },
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

    /* Anunciar al frontend que aceptamos retro_load_game(NULL) para
     * arrancar la consola sin disco (shell de la BIOS PSX: gestor de
     * Memory Cards + reproductor CD audio).  Salvia ofrece esta
     * opcion via launchGame("@bios-only"). */
    {
        bool no_game = true;
        cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
    }
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

    /* Tomb Raider 2 / Crash Team Racing / SPU IRQ Wait options were
     * tied to the legacy PEOPS-thread SPU and are gone now: the new
     * cycle-driven SPU plugin (port from pcsx_rearmed) handles voice
     * silence, decoded-buffer IRQs and IRQ delivery correctly out of
     * the box.  See r3000a.h enum (PSXINT_SPU_IRQ / PSXINT_SPU_UPDATE)
     * and libpcsxcore/spu.c for the new IRQ delivery path. */

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

    /* Dithering global (Unai / Peops / SwanStation): propagamos a cada
     * renderer su flag nativo. Los 3 lo consultan POR PRIMITIVA, asi que el
     * cambio es efectivo en el siguiente draw sin reiniciar.
     * Default = enabled (look PSX original). Si el usuario lo apaga, se
     * mejora fillrate y se evitan los cuelgues de Silent Hill en gpu_unai.
     *
     * Mapeo por plugin:
     *   - gpu_unai:  gpu_unai_config_ext.dithering en {0,1} - respeta el bit
     *                de GP1, OR-ado con force_dithering (lo dejamos a 0).
     *   - xbox_soft: iUseDither en {0,1,2} donde 0=off, 1=respeta GP1,
     *                2=force. Usamos {0,1} para mirror el comportamiento.
     *   - gpu_duck:  se hace via la variable global g_pcsxr_dithering que
     *                el sw_backend consulta antes de bifurcar al path con
     *                dither. */
    g_pcsxr_dithering = read_bool_var("pcsxr360_dithering", true) ? 1 : 0;
    gpu_unai_config_ext.dithering = g_pcsxr_dithering;
    iUseDither = g_pcsxr_dithering;

    /* CD-ROM async prefetch (worker thread on core 2, see cdriso_async.c).
     * Toggleable in caliente: cdra_set_enabled() flips a flag that the
     * read path checks on every call.  When OFF, cdra_read() falls
     * through to the sync path with no thread interaction.  Default ON
     * because it's a pure latency-hiding win for CHD games and harmless
     * for BIN/CUE.  Disable only to rule it out when debugging CD-related
     * issues.  (extern "C" decl is at file scope above.) */
    cdra_set_enabled(read_bool_var("pcsxr360_cdrom_prefetch", true) ? 1 : 0);
    /* gpu_unai mantiene DOS structs de config: la externa (ext) que es la
     * que toca el frontend, y la interna `gpu_unai.config` que es la que
     * consulta `DitheringEnabled()` en el inner loop. Hay que llamar a
     * `unai_apply_config()` para propagar ext -> internal o el cambio no
     * surte efecto en caliente. Solo tiene sentido si Unai esta activo;
     * con otro renderer la llamada copia datos a una zona muerta. */
    if (unai_gpu_enabled)
        unai_apply_config();

    /* True Color (gpu_duck only): aloca/libera el shadow buffer paralelo
     * 24-bit segun la libretro option.  Tres modos:
     *   - "auto": ON si renderer=SwanStation Y pixel_format=XRGB8888.  Lo
     *     gating por pixel_format es porque RGB565 (output 16-bit) no puede
     *     mostrar 8-bit/canal, asi que el shadow no aporta nada.
     *   - "enabled": ON siempre (si duck no esta activo, el flag se setea
     *     pero el buffer no se aloca porque duck_true_color_set_active
     *     solo aloca cuando duck esta vivo).
     *   - "disabled": OFF.
     *
     * El flag g_pcsxr_true_color_active y el buffer g_psxVuw24 se gestionan
     * via duck_true_color_set_active() (en gpu_duck_driver.cpp).  Cuando el
     * usuario cambia la option en caliente: si pasa de OFF a ON y duck esta
     * activo, se aloca el shadow y se inicializa expandiendo psxVuw.  Si
     * pasa de ON a OFF, se libera el shadow. */
    {
        struct retro_variable var_tc = { "pcsxr360_true_color", NULL };
        const char* tc_mode = NULL;
        if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_tc) && var_tc.value)
            tc_mode = var_tc.value;

        int want_tc = 0;
        if (tc_mode && strcmp(tc_mode, "enabled") == 0) {
            want_tc = 1;
        } else if (tc_mode && strcmp(tc_mode, "disabled") == 0) {
            want_tc = 0;
        } else {
            /* "auto" (or default): habilitar si SwanStation + XRGB8888.
             *
             * IMPORTANTE: leemos pcsxr360_gpu_renderer DIRECTAMENTE de la
             * core option, NO usamos `duck_gpu_enabled`.  Razon: en el
             * arranque, check_game_fixes() corre ANTES de check_gpu_
             * renderer_initial_only(), asi que duck_gpu_enabled todavia
             * es 0 (su valor de inicializacion estatica) en este punto.
             * Si nos basaramos en el global, el modo auto nunca activaria
             * true-color al boot, solo despues de un toggle manual de la
             * libretro option (que dispara variable-update y la re-evalua
             * cuando ya hay state).  Leyendo la option directamente
             * funciona en todos los timing paths. */
            struct retro_variable var_pf = { "pcsxr360_pixel_format", NULL };
            const char* pf = NULL;
            if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_pf) && var_pf.value)
                pf = var_pf.value;
            const int is_xrgb = (pf && strcmp(pf, "XRGB8888") == 0);

            struct retro_variable var_renderer = { "pcsxr360_gpu_renderer", NULL };
            const char* renderer = NULL;
            if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_renderer) && var_renderer.value)
                renderer = var_renderer.value;
            const int is_swanstation = (renderer && strcmp(renderer, "SwanStation") == 0);

            want_tc = (is_swanstation && is_xrgb) ? 1 : 0;
        }
        duck_true_color_set_active(want_tc);
        pcsxr_log(RETRO_LOG_INFO,
            "[TrueColor] mode=%s -> active=%d\n",
            tc_mode ? tc_mode : "(unset/auto)", want_tc);
    }

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
    /* Three-way switch: xbox_soft (PEOPS, default), gpu_duck (Swan
     * Station port), gpu_unai (PCSX-ReARMed port).  At most one of
     * the alternates is enabled per session; xbox_soft/gpu.c picks
     * its primFunc dispatch table accordingly. */
    duck_gpu_enabled = 0;
    unai_gpu_enabled = 0;

    struct retro_variable var = { "pcsxr360_gpu_renderer", NULL };
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if      (strcmp(var.value, "SwanStation") == 0) duck_gpu_enabled = 1;
        else if (strcmp(var.value, "Unai") == 0) unai_gpu_enabled = 1;
    }

    if (unai_gpu_enabled) {
        /* Populate gpu_unai's frontend-facing config struct from the
         * libretro variables.  Mirrors PCSX-ReARMed's
         * gpu_unai_config_ext that gets read by renderer_init.
         * Nota: dithering NO se setea aqui - check_game_fixes() lo aplica
         * desde la libretro option pcsxr360_dithering en cada init/update. */
        gpu_unai_config_ext.lighting        = 1;
        gpu_unai_config_ext.fast_lighting   = 0;
        gpu_unai_config_ext.blending        = 1;
        gpu_unai_config_ext.force_dithering = 0;
        gpu_unai_config_ext.ilace_force     = 0;
        gpu_unai_config_ext.pixel_skip      = 0;
        gpu_unai_config_ext.old_renderer    = 0;
    }
}

/* Snapshot the helper-threads choice ONCE at core init, before the
 * threads are spun up.  Sets the global g_pcsxr_threading_enabled
 * (declared in psxcommon.h) which is read by:
 *   - libpcsxcore/gpu.c  (gpuDmaThreadInit / gpuDmaThreadShutdown /
 *                          chain_enqueue)
 *   - plugins/dfsound/spu.c (SetupTimer)
 * Must run BEFORE gpuDmaThreadInit and SPU_open in emu_setup, otherwise
 * the threads boot with the previous boot's setting.  Toggling at
 * runtime would require tearing down and recreating the threads
 * mid-session — not worth the complexity for what is essentially a
 * diagnostic switch.  Hence "(restart core to apply)" in the label. */
static void check_threading_initial_only(void) {
    struct retro_variable var = { "pcsxr360_threading", NULL };
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        g_pcsxr_threading_enabled = (strcmp(var.value, "disabled") == 0) ? 0 : 1;
    else
        g_pcsxr_threading_enabled = 1;  /* default: threading on */
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
 * AUDIO CAPTURE - Direct passthrough to libretro audio_batch_cb
 *
 * SetupSound() / RemoveSound() son provistos por el plugin SPU
 * cycle-driven (port de pcsx_rearmed) via su out_driver en
 * plugins/dfsound/out.c.  El plugin invoca SoundFeedStreamData con
 * batches de muestras estereo PCM 16-bit a 44.1 kHz cada vez que
 * SPU_async se llama con flags=1 (psxcounters.c, ~12 veces/frame
 * NTSC, ~13 PAL).
 *
 * SoundFeedStreamData reenvia esos batches directamente al frontend.
 * El frontend (Salvia) tiene un AudioBuffer SPSC propio con
 * WriteBlocking que provee backpressure al CPU emulado y tolera
 * underrun rellenando con silencio.  No hace falta otro ring aqui. */

extern "C" void SoundFeedStreamData(unsigned char *pSound, long lBytes) {
    if (!audio_batch_cb || !pSound || lBytes <= 0)
        return;
    /* lBytes / 4 = numero de frames estereo (2 canales * 2 bytes/sample). */
    audio_batch_cb((const int16_t *)pSound, (size_t)(lBytes >> 2));
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
   char msg[512];
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
    /* En modo BIOS-only seteamos un path dummy para que UsingIso()
     * devuelva true durante LoadPlugins.  Sin esto, LoadPlugins toma
     * la rama LoadCDRplugin(Config.Cdr) que intenta cargar una DLL
     * externa desde "" -> "Could not load CD-ROM plugin" -> falla.
     * Con el dummy, toma la rama LoadCDRplugin(NULL) que reusa el
     * cdrIsoInit interno.  El dummy se limpia despues de LoadPlugins
     * (mas abajo) para que el resto del codigo vea "no disco". */
    if (g_boot_bios_only) {
        SetIsoFile("@bios-only");
    } else {
        SetIsoFile(game_path_store);
    }

    /* Sample the helper-threads choice ONCE for this boot, before
     * gpuDmaThreadInit() (and before SPU_open further down) decide
     * whether to spin up the GPU/SPU helper threads.  Outside
     * check_game_fixes for the same reason as check_gpu_renderer_only:
     * tearing threads down mid-session via the hot-reload path would
     * race with the producers/consumers.  Restart-to-apply by design. */
	pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] check_threading_initial_only\n");
    check_threading_initial_only();

	pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] gpuDmaThreadInit\n");
    gpuDmaThreadInit();

    /* Re-apply game fixes right before EmuInit: Config.SlowBoot must be
     * set before the BIOS shortcut runs, and the GPU/SPU globals must
     * match the user's choice for this boot. */
	pcsxr_log(RETRO_LOG_DEBUG, "[PCSXR-LR] check_game_fixes\n");
    check_game_fixes();

    /* Modo BIOS-only: forzar SlowBoot para evitar el shortcut de
     * misc.c::LoadCdrom (que setea psxRegs.pc=ra y caeria al vacio
     * sin un EXE en RAM).  Se aplica DESPUES de check_game_fixes
     * para tener prioridad sobre la opcion del usuario. */
    if (g_boot_bios_only) {
        Config.SlowBoot = 1;
        pcsxr_log(RETRO_LOG_INFO, "[PCSXR-LR] BIOS-only mode: forcing Config.SlowBoot=1\n");
    }

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

    /* Limpiar el path dummy que pusimos antes de LoadPlugins en BIOS-only.
     * Con IsoFile = "" el resto del flujo (CDR_open saltado, queries de
     * estado del cdrom emulado) ve "no disco insertado" y la BIOS PSX
     * arranca al shell. */
    if (g_boot_bios_only) {
        SetIsoFile(NULL);
    }

    LoadMcds(Config.Mcd1, Config.Mcd2);
    pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] EmuInit OK\n");

    /* gpuThreadEnable() era no-op desde el rediseño de threading
     * (libpcsxcore/gpu.c, mayo 2026) — el lifecycle del helper thread
     * lo gestiona Init/Shutdown.  Ya no la llamamos. */
    GPU_clearDynarec(clearDynarec);

    /* En modo BIOS-only no abrimos imagen: ISOopen haria fopen("",..)
     * y fallaria.  CDR sigue inicializado por LoadPlugins (CDR_init),
     * solo que sin handle: las queries de TOC del firmware emulado
     * devuelven "no disc" y la BIOS pasa al shell.  Para que el
     * frontend pueda meter un disco luego via Disk Control, marcamos
     * disk_ejected = true (ya hecho en retro_load_game).
     *
     * Ademas seteamos cdOpenCaseTime = -1 (lid permanentemente abierta)
     * para que el plugin CDR reporte Status=0x10 desde el primer
     * getStatus.  Sin esto el plugin reporta "closed sin disco" y la
     * BIOS muestra "INSERT PLAYSTATION CD-ROM" pero ignora el cambio
     * a TOC nuevo cuando luego haces swap (no detecta la transicion
     * open->close porque nunca vio "open"). */
    if (g_boot_bios_only) {
        pcsxr_log(RETRO_LOG_INFO, "[PCSXR-LR] BIOS-only mode: skipping CDR_open, setting lid open\n");
        SetCdOpenCaseTime((s64)-1);
    } else {
        pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling CDR_open...\n");
        ret = CDR_open();
        if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] CDR_open FAILED\n"); return -1; }
    }
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling GPU_open...\n");
    ret = GPU_open(NULL);
    if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] GPU_open FAILED\n"); return -1; }
    /* Populate SPUConfig AFTER LoadPlugins (which already ran SPUinit
     * and set iVolume = 768 as the plugin default).  We must NOT
     * write iVolume = 0 here — that's how SPUinit signals "use the
     * plugin's default" the first time around, but afterwards 0 is
     * just zero and do_samples_finish's "muted" path zeroes every
     * mixed sample, producing total silence (this was exactly the
     * "no audio in Crash Bandicoot" symptom).
     *
     * Fields:
     *  - iVolume:           1024 = unity (100 %).  Use the full PSX
     *                       master, since RetroArch already exposes
     *                       its own volume control on top.
     *  - iUseReverb:        1 = enabled (PSX-correct reverb).
     *  - iUseInterpolation: 2 = gauss (good quality, fast on Xenon).
     *  - iTempo:            0 = no rate-stretch hack (RetroArch syncs).
     *  - iUseThread:        0 = run mixing inline with the CPU
     *                       emulator (cycle-driven model).  Worker
     *                       thread is intentionally disabled — it
     *                       trades 1 frame of latency for ~1 ms of
     *                       parallelism, not worth it on this build. */
    {
        spu_config.iVolume = 1024;
        spu_config.iXAPitch = 0;
        spu_config.iUseReverb = 1;
        spu_config.iUseInterpolation = 2;
        spu_config.iTempo = 0;
        spu_config.iUseThread = 0;
    }
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling SPU_open...\n");
    ret = SPU_open(NULL);
    if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] SPU_open FAILED\n"); return -1; }
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling SPU_registerCallback...\n");
    /* Register the IRQ callback (cycles-aware: SPUirq(int cycles_after)).
     * The new plugin schedules a PSXINT_SPU_IRQ event via set_event when
     * cycles_after > 0, so the IRQ bit lands at the right cycle. */
    SPU_registerCallback(SPUirq);
    /* Also register the schedule callback — the SPU plugin uses this
     * to ask the CPU scheduler to re-enter SPUasync at the next
     * predicted IRQ point (PSXINT_SPU_UPDATE event).  This is the
     * core mechanism that replaces the legacy PEOPS-thread wait. */
    if (SPU_registerScheduleCb)
        SPU_registerScheduleCb(SPUschedule);
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling PAD1_open...\n");
    ret = PAD1_open(NULL);
    if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PAD1_open FAILED\n"); return -1; }
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling PAD2_open...\n");
    ret = PAD2_open(NULL);
    if (ret < 0) { pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PAD2_open FAILED\n"); return -1; }
    pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PADs OK\n");
	
    /* Modo BIOS-only: no hay disco que inspeccionar, asi que saltamos
     * CheckCdrom (lee la TOC + SYSTEM.CNF) y LoadCdrom (carga el EXE
     * del juego).  EmuReset si se llama porque deja CPU/SPU/GPU en
     * estado inicial coherente, que es lo que la BIOS PSX espera al
     * arrancar. */
    if (g_boot_bios_only) {
        pcsxr_log(RETRO_LOG_INFO, "[PCSXR-LR] BIOS-only mode: skipping CheckCdrom/LoadCdrom\n");
        EmuReset();
    } else {
        pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling CheckCdrom...\n");
        CheckCdrom();
        pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling EmuReset...\n");
        EmuReset();
        pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] Calling LoadCdrom...\n");
        LoadCdrom();
        pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] LoadCdrom done\n");
    }

    Config.CpuRunning = 1;
    return 0;
}

/* Inverse of emu_setup: closes plugins and shuts down the GPU helper
 * thread.  Mirrors the legacy SysClose() but invoked directly.
 *
 * IMPORTANTE: este teardown puede ejecutarse despues de un emu_setup
 * que fallo a media ruta (e.g. LoadPlugins devuelve -1 porque la BIOS
 * o el plugin CD no estaba donde se esperaba).  En ese caso los punteros
 * de plugin (PAD1_close, CDR_close, etc.) son NULL — Salvia los declara
 * como globales sin inicializacion en libpcsxcore/plugins.c, y son
 * LoadPlugins quien los rellena.  Si invocamos PAD2_close() etc.
 * directamente sin guard, salta access violation 0xC0000005 leyendo
 * desde 0x00000000 (el call goes to NULL function pointer).  Por eso
 * cada close va gateado por un check de NULL. */
static void emu_teardown(void) {
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] CpuRunning = 0\n");
    Config.CpuRunning = 0;
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] gpuDmaThreadShutdown\n");
    gpuDmaThreadShutdown();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PAD2_close\n");
    if (PAD2_close) PAD2_close();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] PAD1_close\n");
    if (PAD1_close) PAD1_close();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] CDR_close\n");
    if (CDR_close)  CDR_close();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] GPU_close\n");
    if (GPU_close)  GPU_close();
	pcsxr_log(RETRO_LOG_DEBUG,"[PCSXR-LR] SPU_close\n");
    if (SPU_close)  SPU_close();
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
    /* No early-return cuando los flags coinciden: en BIOS-only arrancamos
     * con disk_ejected=true y cdOpenCaseTime=-1, y Salvia hara
     * set_eject_state(true) (redundante en flag) seguido de
     * set_image_index + set_eject_state(false).  Si el redundante
     * no-op, el BIOS PSX no ve nada anormal entre ambos calls; pero
     * tampoco hace daño re-llamar a SetCdOpenCaseTime/CDR_close, son
     * idempotentes y aseguran estado limpio para el siguiente swap. */
    if (ejected) {
        /* Open the shell permanently until we insert */
        SetCdOpenCaseTime((s64)-1);
        /* CDR_close solo si previamente abierto: en BIOS-only inicial
         * nunca llamamos CDR_open, asi que cdHandle es NULL.  ISOclose
         * tolera handle NULL (early-return) pero llamarlo en bucle no
         * cuesta y nos protege de estados huerfanos. */
        if (CDR_close && !disk_ejected) CDR_close();
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
 *   [PERF] 60fr 999ms | exec=14.20 vid=1.10 aud=0.30 sync=0.20 gap=0.60 | fps=60.0 budget=16.40 | cores=0x18
 *
 *   exec    = mean ms inside psxCpu->Execute() per frame
 *   vid     = mean ms inside video_cb per frame
 *   aud     = residual ms post-vid pre-perf (audio se entrega ya
 *             dentro de exec via SoundFeedStreamData -> audio_batch_cb,
 *             asi que esta cifra suele ser ~0)
 *   sync    = mean ms in poll_input + hot-reload checks per frame
 *   gap     = mean ms between retro_run end and next retro_run start
 *             (== frontend-side cost: present, OSD, shaders, audio mix)
 *   fps     = effective frame rate over the 60-frame window
 *   budget  = mean total ms per frame (exec+vid+aud+sync+gap)
 *   cores   = bitmask of cores observed running retro_run en la ventana
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
/* perf_aud_min/max/sum eliminados: ya no hay ring de audio interno que
 * monitorizar (audio se pasa directo al frontend desde el SPU
 * cycle-driven). */
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

    char buf[384];
    _snprintf(buf, sizeof(buf),
        "[PERF] %ufr %ums | exec=%u.%02u (gpu_wait=%u.%02u) vid=%u.%02u aud=%u.%02u sync=%u.%02u gap=%u.%02u | fps=%u.%u budget=%u.%02u | cores=0x%02X\n",
        (unsigned)fc, (unsigned)(total_us / 1000),
        (unsigned)(e  / 100), (unsigned)(e  % 100),
        (unsigned)(gw / 100), (unsigned)(gw % 100),
        (unsigned)(v  / 100), (unsigned)(v  % 100),
        (unsigned)(a  / 100), (unsigned)(a  % 100),
        (unsigned)(s  / 100), (unsigned)(s  % 100),
        (unsigned)(g  / 100), (unsigned)(g  % 100),
        (unsigned)(fps_x10 / 10), (unsigned)(fps_x10 % 10),
        (unsigned)(budget / 100), (unsigned)(budget % 100),
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

    /* Modo "BIOS only": el frontend nos llama con game==NULL para
     * arrancar la consola sin disco (shell PSX: MemCard manager + CD
     * audio player).  Lo anunciamos via SET_SUPPORT_NO_GAME en
     * retro_set_environment. */
    g_boot_bios_only = (game == NULL || game->path == NULL || game->path[0] == '\0');
    if (g_boot_bios_only) {
        pcsxr_log(RETRO_LOG_INFO, "[PCSXR-LR] retro_load_game: BIOS-only mode (no disc)\n");
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
    /* En BIOS-only el disco esta "ejected" — no hay nada metido y la
     * BIOS asi lo detecta al inicio.  El usuario podria luego usar
     * Disk Control del frontend para meter un disco si quisiera, pero
     * por defecto arrancamos sin nada. */
    disk_ejected = g_boot_bios_only;

    if (g_boot_bios_only) {
        /* Sin path de disco, no hay TOC ni labels que poblar.
         * game_path_store vacio + SetIsoFile(NULL) en emu_setup
         * dejan UsingIso() == false. */
        game_path_store[0] = '\0';
    } else {
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
    }

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

#if PCSXR_DIAG_INSTRUMENTATION
/* === [RR-PERSEC] timing por seccion (gated PCSXR_DIAG_INSTRUMENTATION) ===
 * Versiones envueltas de DIAG_SET_RR_SEC que ademas acumulan tiempo wall
 * en cada seccion.  Permite que [RR-SLOW] muestre breakdown por seccion
 * en lugar de solo "ultima seccion al salir".  Coste cero en release. */
static LARGE_INTEGER g_rr_sec_last_change = {0};
static int           g_rr_current_section = RR_SEC_OUT_OF_RUN;
static uint64_t      g_rr_sec_us[9] = {0};
static LARGE_INTEGER g_rr_perf_freq = {0};

static inline void rr_account_section(int new_sec) {
    LARGE_INTEGER now;
    uint64_t delta_us;
    if (g_rr_perf_freq.QuadPart == 0) QueryPerformanceFrequency(&g_rr_perf_freq);
    QueryPerformanceCounter(&now);
    if (g_rr_sec_last_change.QuadPart != 0 &&
        g_rr_current_section >= 0 && g_rr_current_section < 9) {
        delta_us = (uint64_t)((now.QuadPart - g_rr_sec_last_change.QuadPart)
                             * 1000000LL / g_rr_perf_freq.QuadPart);
        g_rr_sec_us[g_rr_current_section] += delta_us;
    }
    g_rr_current_section = new_sec;
    g_rr_sec_last_change = now;
    retro_run_section = new_sec;
}

#define RR_SET_SEC(sec) rr_account_section(sec)
#else
/* En release, RR_SET_SEC degenera a DIAG_SET_RR_SEC (que tambien es
 * no-op si PCSXR_DIAG_INSTRUMENTATION=0 en gpu.h).  Cero overhead. */
#define RR_SET_SEC(sec) DIAG_SET_RR_SEC(sec)
#endif

void retro_run(void) {
#if PCSXR_DIAG_INSTRUMENTATION
    /* === [RR-HEARTBEAT] instrumentacion diagnostico ===
     * Gateado tras PCSXR_DIAG_INSTRUMENTATION.  En release nada de
     * esto compila — coste cero. */
    LARGE_INTEGER rr_t_entry;
    /* Resetear contadores por seccion para este frame */
    {
        int i;
        for (i = 0; i < 9; i++) g_rr_sec_us[i] = 0;
    }
    {
        extern LARGE_INTEGER g_rr_last_exit;
        static LARGE_INTEGER s_rr_freq = {0};
        static LARGE_INTEGER s_rr_window_start = {0};
        static uint32_t s_rr_entry_count = 0;
        static uint32_t s_rr_total_calls = 0;
        const uint32_t  RR_LOG_EVERY = 60;
        const uint32_t  RR_GAP_WARN_MS = 100;

        if (s_rr_freq.QuadPart == 0)
            QueryPerformanceFrequency(&s_rr_freq);
        QueryPerformanceCounter(&rr_t_entry);

        if (g_rr_last_exit.QuadPart != 0) {
            uint64_t gap_us = (uint64_t)((rr_t_entry.QuadPart - g_rr_last_exit.QuadPart)
                                        * 1000000LL / s_rr_freq.QuadPart);
            if (gap_us >= RR_GAP_WARN_MS * 1000ULL) {
                pcsxr_log(RETRO_LOG_DEBUG,
                    "[RR-GAP] frontend gap=%u ms (call %u; >%u ms means Salvia held off the next retro_run)\n",
                    (unsigned)(gap_us / 1000ULL),
                    (unsigned)s_rr_total_calls,
                    (unsigned)RR_GAP_WARN_MS);
            }
        }
        s_rr_total_calls++;
        s_rr_entry_count++;

        if (s_rr_window_start.QuadPart == 0)
            s_rr_window_start = rr_t_entry;
        if (s_rr_entry_count >= RR_LOG_EVERY) {
            uint64_t window_us = (uint64_t)((rr_t_entry.QuadPart - s_rr_window_start.QuadPart)
                                           * 1000000LL / s_rr_freq.QuadPart);
            pcsxr_log(RETRO_LOG_DEBUG,
                "[RR-HEARTBEAT] %u entries in %u ms wall (avg %u ms/call) | total=%u\n",
                (unsigned)s_rr_entry_count,
                (unsigned)(window_us / 1000ULL),
                window_us ? (unsigned)(window_us / s_rr_entry_count / 1000ULL) : 0u,
                (unsigned)s_rr_total_calls);
            s_rr_entry_count  = 0;
            s_rr_window_start = rr_t_entry;
        }
    }
#endif /* PCSXR_DIAG_INSTRUMENTATION */

    /* Marca la fase actual de retro_run para el GPU watchdog (cuando
     * PCSXR_DIAG_INSTRUMENTATION=1 en gpu.h).  Si la macro esta ON,
     * RR_SET_SEC acumula tiempo por seccion para [RR-SLOW]; si esta
     * OFF, degenera a DIAG_SET_RR_SEC (tambien no-op). */
    RR_SET_SEC(RR_SEC_ENTRY);

    if (!emu_running) {
        if (video_cb)
            video_cb(NULL, display_width, display_height, 0);
        RR_SET_SEC(RR_SEC_OUT_OF_RUN);
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

    RR_SET_SEC(RR_SEC_INPUT_POLL);
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
    RR_SET_SEC(RR_SEC_CPU_EXEC);
    psxCpu->Execute();
#if PCSXR_PERF_ENABLED
    QueryPerformanceCounter(&t_after_exec);
    perf_acc_exec     += perf_us(t_after_sync, t_after_exec);
    perf_acc_gpu_wait += (uint64_t)(gpu_wait_ticks * 1000000ll / perf_freq.QuadPart);
#endif

    RR_SET_SEC(RR_SEC_AUTOSKIP);

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
    RR_SET_SEC(RR_SEC_VIDEO_CB);
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

    /* Audio: ya no hay drenado aqui.  El SPU cycle-driven invoca
     * audio_batch_cb directamente desde SoundFeedStreamData (~12 veces
     * por frame NTSC).  El frontend (Salvia) tiene su propio buffer
     * con WriteBlocking, que provee backpressure si el dispositivo de
     * salida va lento. */

#if PCSXR_PERF_ENABLED
    RR_SET_SEC(RR_SEC_PERF_DUMP);
    QueryPerformanceCounter(&t_end);
    /* perf_acc_aud queda como diferencia minima (sin drenado real) — la
     * mantenemos por compat con el dump. */
    perf_acc_aud      += perf_us(t_after_vid, t_end);
    perf_last_frame_end = t_end;
    perf_frame_count++;

    if (perf_frame_count >= PERF_WINDOW_FRAMES)
        perf_dump(t_end);
#endif

    /* Mark exit phase para watchdog GPU.  Si DIAG_INSTRUMENTATION
     * esta ON, RR_SET_SEC tambien acumula tiempo para el dump
     * [RR-SLOW] del bloque diagnostico que sigue. */
    RR_SET_SEC(RR_SEC_OUT_OF_RUN);

#if PCSXR_DIAG_INSTRUMENTATION
    /* === [RR-SLOW] desglose por seccion (gated DIAG) === */
    {
        extern LARGE_INTEGER g_rr_last_exit;
        const uint32_t RR_FRAME_WARN_MS = 100;
        const uint32_t RR_PERSEC_REPORT_MS = 20;  /* loguear secciones >=20 ms */
        uint64_t frame_us = 0;
        int i;

        for (i = 0; i < 9; i++) frame_us += g_rr_sec_us[i];

        if (frame_us >= RR_FRAME_WARN_MS * 1000ULL) {
            pcsxr_log(RETRO_LOG_DEBUG,
                "[RR-SLOW] frame_dur=%u ms wall, breakdown by section:\n",
                (unsigned)(frame_us / 1000ULL));
            for (i = 0; i < 9; i++) {
                uint64_t sec_us = g_rr_sec_us[i];
                const char *sec_name;
                if (sec_us < RR_PERSEC_REPORT_MS * 1000ULL) continue;
                switch (i) {
                    case RR_SEC_ENTRY:       sec_name = "ENTRY";       break;
                    case RR_SEC_INPUT_POLL:  sec_name = "INPUT_POLL";  break;
                    case RR_SEC_CPU_EXEC:    sec_name = "CPU_EXEC";    break;
                    case RR_SEC_AUTOSKIP:    sec_name = "AUTOSKIP";    break;
                    case RR_SEC_VIDEO_CB:    sec_name = "VIDEO_CB";    break;
                    case RR_SEC_PERF_DUMP:   sec_name = "PERF_DUMP";   break;
                    case RR_SEC_OUT_OF_RUN:  sec_name = "OUT_OF_RUN";  break;
                    default:                 sec_name = "?";           break;
                }
                pcsxr_log(RETRO_LOG_DEBUG,
                    "[RR-SLOW]   %-12s = %u ms (%u%%)\n",
                    sec_name,
                    (unsigned)(sec_us / 1000ULL),
                    frame_us ? (unsigned)((sec_us * 100ULL) / frame_us) : 0u);
            }
        }
        {
            LARGE_INTEGER rr_t_exit;
            QueryPerformanceCounter(&rr_t_exit);
            g_rr_last_exit = rr_t_exit;
        }
    }
#endif /* PCSXR_DIAG_INSTRUMENTATION */
}

#if PCSXR_DIAG_INSTRUMENTATION
/* [RR-HEARTBEAT] timestamp de salida del ultimo retro_run, leido por
 * la siguiente entrada para calcular el gap del frontend. */
LARGE_INTEGER g_rr_last_exit = {0};
#endif

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
