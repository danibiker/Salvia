/* Libretro integration glue for the Nuked-SC55 emulator.
 *
 * The SC-55 emulator runs the unit's own firmware (H8 MCU + sub-MCU + PCM
 * chip), so every output sample is expensive.  Running it inline from
 * retro_run() made the whole X68000 emulation drop far below full speed.
 *
 * The unit therefore lives on its own worker thread:
 *   - The emulation thread queues MIDI bytes into a single-producer /
 *     single-consumer ring (midi_ring) and never touches the unit.
 *   - The worker hands those bytes to the unit, runs it, resamples to
 *     44.1 kHz and keeps PX_SC55_LEAD frames ready in a second SPSC ring
 *     (au_ring).
 *   - px68k_sc55_mix() only copies what is ready and wakes the worker.  It
 *     never waits: if the SC-55 cannot keep up, the music gets gaps but the
 *     emulator stays at full speed.
 * Both rings use retro_atomic indices; nothing takes a lock.  The worker
 * blocks on an auto-reset event while its ring is full, so it costs nothing
 * while the emulator is paused, and the thread only exists while the SC-55
 * is enabled.
 *
 * Without Win32/XDK threads (or with PX68K_SC55_NO_THREAD) the same code
 * runs inline from px68k_sc55_mix(), as before. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <libretro.h>

#if (defined(_WIN32) || defined(_XBOX)) && !defined(PX68K_SC55_NO_THREAD)
#include <retro_atomic.h>   /* brings <windows.h> / <xtl.h> on MSVC */
#if defined(RETRO_ATOMIC_LOCK_FREE)
#define PX_SC55_THREADED 1
#endif
#endif

#include "sc55.h"
#include "sc55_libretro.h"

#include <compat/msvc.h>

extern retro_log_printf_t log_cb;   /* libretro.c */

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#ifdef _WIN32
#define PXSLASH '\\'
#else
#define PXSLASH '/'
#endif

#ifdef PX_SC55_THREADED
typedef retro_atomic_size_t px_size_t;
typedef retro_atomic_int_t  px_int_t;
#define PX_LOAD_SIZE(p)     retro_atomic_load_acquire_size(p)
#define PX_STORE_SIZE(p, v) retro_atomic_store_release_size(p, v)
#define PX_LOAD_INT(p)      retro_atomic_load_acquire_int(p)
#define PX_STORE_INT(p, v)  retro_atomic_store_release_int(p, v)
#else
typedef size_t px_size_t;
typedef int    px_int_t;
#define PX_LOAD_SIZE(p)     (*(p))
#define PX_STORE_SIZE(p, v) (*(p) = (v))
#define PX_LOAD_INT(p)      (*(p))
#define PX_STORE_INT(p, v)  (*(p) = (v))
#endif

/* Xbox 360 hardware thread for the worker.  Salvia's main thread is on 0
 * and the emulation (CPU_THREAD) on 1, i.e. both on core 0; 2 is the first
 * hardware thread of core 1, so the SC-55 gets a physical core to itself.
 * Measured with 24 voices: ~95% of real time on 2, ~103% on 4 (core 2 is
 * shared with SDL's audio thread, which libSDLx360 pins to 5). */
#ifndef PX68K_SC55_XBOX_HWTHREAD
#define PX68K_SC55_XBOX_HWTHREAD 2
#endif

#define PX_SC55_OUT_RATE   44100
#define PX_SC55_AU_RING    8192    /* output frames, power of two */
#define PX_SC55_LEAD       2048    /* frames kept ready (~46 ms at 44.1 kHz) */
#define PX_SC55_BLOCK      256     /* output frames per worker pass */
#define PX_SC55_MIDI_RING  32768   /* bytes, power of two */
#define PX_SC55_NATIVE_MAX 4096    /* native frames buffered for the resampler */

static sc55_t *unit;
static int active_enabled;
static char saved_system[PATH_MAX];
static char saved_content[PATH_MAX];
static unsigned native_rate = 66207;
static int volume_percent = 100;          /* caller side only */

/* ---- worker side ---------------------------------------------------- */
static int32_t native_audio[PX_SC55_NATIVE_MAX * 2];
static size_t native_count;
static double native_phase;
static double native_ratio = 66207.0 / 44100.0;

/* ---- shared between the two sides ----------------------------------- */
static int32_t       au_ring[PX_SC55_AU_RING * 2]; /* pre-volume, 16-bit domain */
static px_size_t     au_w;                 /* worker */
static px_size_t     au_r;                 /* caller */
static unsigned char midi_ring[PX_SC55_MIDI_RING];
static px_size_t     midi_w;               /* caller */
static px_size_t     midi_r;               /* worker */
static px_int_t      worker_quit;

/* ---- caller side ---------------------------------------------------- */
static int    worker_running;
static int    midi_overflow_reported;
static int    underrun_reported;
static size_t frames_mixed;

#ifdef PX_SC55_THREADED
static HANDLE worker_thread;
static HANDLE worker_wake;
#endif

static void sc55_log(enum retro_log_level level, const char *fmt, ...)
{
   char buf[512];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   buf[sizeof(buf) - 1] = '\0';
   {
      /* Some frontends run the text through printf again: no '%' in it. */
      char *c;
      for (c = buf; *c; c++)
         if (*c == '%') *c = ' ';
   }
   if (log_cb) log_cb(level, "[PX68K SC-55] %s\n", buf);
   else fprintf(stderr, "[PX68K SC-55] %s\n", buf);
}

static void copy_path(char *out, size_t cap, const char *in)
{
   if (!in) in = "";
   snprintf(out, cap, "%s", in);
}

static int load_file_slot(sc55_t *dev, int slot, const char *path)
{
   FILE *f = fopen(path, "rb");
   long size;
   unsigned char *buf;
   int ok;
   if (!f) return 0;
   if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET) != 0)
   { fclose(f); return 0; }
   buf = (unsigned char*)malloc((size_t)size);
   if (!buf) { fclose(f); return 0; }
   if (fread(buf, 1, (size_t)size, f) != (size_t)size)
   { free(buf); fclose(f); return 0; }
   fclose(f);
   ok = sc55_load_rom(dev, slot, buf, (size_t)size);
   free(buf);
   return ok;
}

static int try_named_set(int model, const char *const names[SC55_ROM_COUNT],
      const char *const *dirs, size_t ndirs)
{
   sc55_t *dev = sc55_new(model);
   int slot;
   size_t d;
   char path[PATH_MAX * 2];
   if (!dev) return 0;
   for (slot = 0; slot < SC55_ROM_COUNT; slot++)
   {
      int loaded = 0;
      if (!names[slot]) continue;
      for (d = 0; d < ndirs && !loaded; d++)
      {
         if (!dirs[d] || !dirs[d][0]) continue;
         snprintf(path, sizeof(path), "%s%c%s", dirs[d], PXSLASH, names[slot]);
         loaded = load_file_slot(dev, slot, path);
      }
      if (!loaded) { sc55_free(dev); return 0; }
   }
   sc55_reset(dev);
   unit = dev;
   native_rate = sc55_rate(unit);
   native_ratio = (double)native_rate / (double)PX_SC55_OUT_RATE;
   return 1;
}

static int load_unit(void)
{
   char sys_prboom[PATH_MAX], content_sc55[PATH_MAX], sys_sc55[PATH_MAX];
   char sys_sc_55[PATH_MAX], sys_roland[PATH_MAX], sys_nuked[PATH_MAX];
   char pr_sc55[PATH_MAX], pr_sc_55[PATH_MAX], pr_roland[PATH_MAX], pr_nuked[PATH_MAX];
   const char *dirs[16];
   size_t n = 0;
   static const char *const mk2[SC55_ROM_COUNT] =
      { "rom1.bin", "rom2.bin", "rom_sm.bin", "waverom1.bin", "waverom2.bin", NULL };
   static const char *const mk1[SC55_ROM_COUNT] =
      { "sc55_rom1.bin", "sc55_rom2.bin", NULL, "sc55_waverom1.bin", "sc55_waverom2.bin", "sc55_waverom3.bin" };
   snprintf(sys_prboom, sizeof(sys_prboom), "%s%cprboom", saved_system, PXSLASH);
   snprintf(content_sc55, sizeof(content_sc55), "%s%csc55", saved_content, PXSLASH);
   snprintf(sys_sc55, sizeof(sys_sc55), "%s%csc55", saved_system, PXSLASH);
   snprintf(sys_sc_55, sizeof(sys_sc_55), "%s%csc-55", saved_system, PXSLASH);
   snprintf(sys_roland, sizeof(sys_roland), "%s%croland", saved_system, PXSLASH);
   snprintf(sys_nuked, sizeof(sys_nuked), "%s%cnuked", saved_system, PXSLASH);
   snprintf(pr_sc55, sizeof(pr_sc55), "%s%csc55", sys_prboom, PXSLASH);
   snprintf(pr_sc_55, sizeof(pr_sc_55), "%s%csc-55", sys_prboom, PXSLASH);
   snprintf(pr_roland, sizeof(pr_roland), "%s%croland", sys_prboom, PXSLASH);
   snprintf(pr_nuked, sizeof(pr_nuked), "%s%cnuked", sys_prboom, PXSLASH);
   if (saved_content[0]) dirs[n++] = saved_content;
   if (saved_system[0]) dirs[n++] = sys_prboom;
   if (saved_system[0]) dirs[n++] = saved_system;
   if (saved_content[0]) dirs[n++] = content_sc55;
   if (saved_system[0]) dirs[n++] = sys_sc55;
   if (saved_system[0]) dirs[n++] = sys_sc_55;
   if (saved_system[0]) dirs[n++] = sys_roland;
   if (saved_system[0]) dirs[n++] = sys_nuked;
   if (saved_system[0]) dirs[n++] = pr_sc55;
   if (saved_system[0]) dirs[n++] = pr_sc_55;
   if (saved_system[0]) dirs[n++] = pr_roland;
   if (saved_system[0]) dirs[n++] = pr_nuked;
   if (try_named_set(SC55_MODEL_MK2, mk2, dirs, n)) return 1;
   if (try_named_set(SC55_MODEL_MK1, mk1, dirs, n)) return 1;
   return 0;
}

/* ---- worker ---------------------------------------------------------- */

static void rings_reset(void)
{
   native_count = 0;
   native_phase = 0.0;
   PX_STORE_SIZE(&au_w, 0);
   PX_STORE_SIZE(&au_r, 0);
   PX_STORE_SIZE(&midi_w, 0);
   PX_STORE_SIZE(&midi_r, 0);
   PX_STORE_INT(&worker_quit, 0);
   midi_overflow_reported = 0;
   underrun_reported = 0;
   frames_mixed = 0;
}

/* Hand queued MIDI bytes to the unit, as many as its serial queue takes.
 * The rest waits for the next pass instead of overwriting unread input. */
static void worker_feed_midi(void)
{
   size_t r = PX_LOAD_SIZE(&midi_r);
   size_t w = PX_LOAD_SIZE(&midi_w);
   size_t n = w - r;
   size_t room;
   if (!n) return;
   room = sc55_midi_room(unit);
   if (n > room) n = room;
   while (n)
   {
      size_t pos = r & (PX_SC55_MIDI_RING - 1);
      size_t chunk = PX_SC55_MIDI_RING - pos;
      if (chunk > n) chunk = n;
      sc55_midi(unit, midi_ring + pos, chunk);
      r += chunk;
      n -= chunk;
   }
   PX_STORE_SIZE(&midi_r, r);
}

/* Keep one native sample on the left of the interpolation point. */
static void worker_drop_native(void)
{
   size_t drop = (size_t)native_phase;
   if (drop >= native_count) drop = native_count ? native_count - 1 : 0;
   if (drop)
   {
      memmove(native_audio, native_audio + drop * 2, (native_count - drop) * 2 * sizeof(int32_t));
      native_count -= drop;
      native_phase -= (double)drop;
   }
}

/* Run the unit and append up to `frames` 44.1 kHz frames to au_ring. */
static size_t worker_render(size_t frames)
{
   size_t w = PX_LOAD_SIZE(&au_w);
   size_t i;
   size_t need;
   double last_needed;
   if (!frames) return 0;
   worker_drop_native();
   last_needed = native_phase + (double)(frames - 1) * native_ratio + 1.0;
   need = last_needed >= 0.0 ? (size_t)last_needed + 1 : 2;
   if (need > PX_SC55_NATIVE_MAX) need = PX_SC55_NATIVE_MAX;
   if (need > native_count)
   {
      size_t add = need - native_count;
      if (sc55_run(unit, native_audio + native_count * 2, add) == add)
         native_count += add;
   }
   for (i = 0; i < frames; i++)
   {
      size_t idx = (size_t)native_phase;
      double frac = native_phase - (double)idx;
      int32_t l, r;
      int32_t *dst;
      if (idx + 1 >= native_count) break;
      l = (int32_t)((double)native_audio[idx * 2] * (1.0 - frac) + (double)native_audio[(idx + 1) * 2] * frac);
      r = (int32_t)((double)native_audio[idx * 2 + 1] * (1.0 - frac) + (double)native_audio[(idx + 1) * 2 + 1] * frac);
      dst = au_ring + ((w + i) & (PX_SC55_AU_RING - 1)) * 2;
      /* Full scale is +-2^29: >>14 puts it in the 16-bit mixer domain. */
      dst[0] = l >> 14;
      dst[1] = r >> 14;
      native_phase += native_ratio;
   }
   worker_drop_native();
   PX_STORE_SIZE(&au_w, w + i);
   return i;
}

/* Produce until `target` frames are waiting for the mixer. */
static void worker_fill(size_t target)
{
   for (;;)
   {
      size_t fill = PX_LOAD_SIZE(&au_w) - PX_LOAD_SIZE(&au_r);
      size_t want;
      worker_feed_midi();
      if (fill >= target) break;
      if (PX_LOAD_INT(&worker_quit)) break;
      want = target - fill;
      if (want > PX_SC55_BLOCK) want = PX_SC55_BLOCK;
      if (!worker_render(want)) break;
   }
}

#ifdef PX_SC55_THREADED
static DWORD WINAPI worker_main(LPVOID arg)
{
   (void)arg;
   while (!PX_LOAD_INT(&worker_quit))
   {
      worker_fill(PX_SC55_LEAD);
      /* Auto-reset event: a wake sent while we were busy is not lost. */
      WaitForSingleObject(worker_wake, INFINITE);
   }
   return 0;
}
#endif

static int worker_start(void)
{
#ifdef PX_SC55_THREADED
   worker_wake = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!worker_wake) return 0;
   worker_thread = CreateThread(NULL, 256 * 1024, worker_main, NULL, CREATE_SUSPENDED, NULL);
   if (!worker_thread)
   {
      CloseHandle(worker_wake);
      worker_wake = NULL;
      return 0;
   }
#ifdef _XBOX
   XSetThreadProcessor(worker_thread, PX68K_SC55_XBOX_HWTHREAD);
#endif
   SetThreadPriority(worker_thread, THREAD_PRIORITY_ABOVE_NORMAL);
   ResumeThread(worker_thread);
   return 1;
#else
   return 0;
#endif
}

static void worker_stop(void)
{
#ifdef PX_SC55_THREADED
   if (worker_thread)
   {
      PX_STORE_INT(&worker_quit, 1);
      SetEvent(worker_wake);
      WaitForSingleObject(worker_thread, INFINITE);
      CloseHandle(worker_thread);
      worker_thread = NULL;
   }
   if (worker_wake)
   {
      CloseHandle(worker_wake);
      worker_wake = NULL;
   }
#endif
   worker_running = 0;
}

/* ---- public API (emulation thread) ------------------------------------ */

void px68k_sc55_shutdown(void)
{
   worker_stop();
   if (unit) sc55_free(unit);
   unit = NULL;
   active_enabled = 0;
   native_count = 0;
   native_phase = 0.0;
}

void px68k_sc55_configure(int enabled, const char *system_dir, const char *content_dir)
{
   char sys[PATH_MAX], content[PATH_MAX];
   copy_path(sys, sizeof(sys), system_dir);
   copy_path(content, sizeof(content), content_dir);
   enabled = enabled != 0;
   if (!enabled)
   {
      if (unit || active_enabled) px68k_sc55_shutdown();
      saved_system[0] = saved_content[0] = '\0';
      return;
   }
   if (unit && active_enabled && !strcmp(saved_system, sys) && !strcmp(saved_content, content))
      return;
   px68k_sc55_shutdown();
   copy_path(saved_system, sizeof(saved_system), sys);
   copy_path(saved_content, sizeof(saved_content), content);
   if (load_unit())
   {
      rings_reset();
      worker_running = worker_start();
      if (worker_running)
#ifdef _XBOX
         sc55_log(RETRO_LOG_INFO, "unit loaded (%u Hz), worker thread on hw thread %d",
               native_rate, PX68K_SC55_XBOX_HWTHREAD);
#else
         sc55_log(RETRO_LOG_INFO, "unit loaded (%u Hz), worker thread started", native_rate);
#endif
      else
         sc55_log(RETRO_LOG_WARN, "unit loaded (%u Hz), running INLINE (no worker thread)", native_rate);
      active_enabled = 1;
   }
   else sc55_log(RETRO_LOG_ERROR, "ROM set not found. Expected PrBoom-compatible filenames in the System Directory, System/prboom, or SC-55 subfolders.");
}

int px68k_sc55_enabled(void) { return active_enabled && unit != NULL; }

void px68k_sc55_set_volume(int value)
{
   if (value < 0) value = 0;
   if (value > 200) value = 200;
   volume_percent = value;
}

/* Queue a whole message or nothing: half a SysEx is worse than none. */
static void midi_queue(const uint8_t *data, size_t len)
{
   size_t w = PX_LOAD_SIZE(&midi_w);
   size_t r = PX_LOAD_SIZE(&midi_r);
   size_t i;
   if (len > PX_SC55_MIDI_RING - (w - r))
   {
      if (!midi_overflow_reported)
         sc55_log(RETRO_LOG_WARN, "MIDI queue full, dropping messages");
      midi_overflow_reported = 1;
      return;
   }
   for (i = 0; i < len; i++)
      midi_ring[(w + i) & (PX_SC55_MIDI_RING - 1)] = data[i];
   PX_STORE_SIZE(&midi_w, w + len);
}

void px68k_sc55_midi_short(size_t msg)
{
   uint8_t bytes[3];
   size_t len = 3;
   if (!px68k_sc55_enabled()) return;
   bytes[0] = (uint8_t)(msg & 0xff);
   bytes[1] = (uint8_t)((msg >> 8) & 0xff);
   bytes[2] = (uint8_t)((msg >> 16) & 0xff);
   if ((bytes[0] & 0xf0) == 0xc0 || (bytes[0] & 0xf0) == 0xd0) len = 2;
   midi_queue(bytes, len);
}

void px68k_sc55_midi_long(const uint8_t *data, size_t len)
{
   if (px68k_sc55_enabled() && data && len) midi_queue(data, len);
}

static int16_t clamp16(int32_t x)
{
   if (x > 32767) return 32767;
   if (x < -32768) return -32768;
   return (int16_t)x;
}

void px68k_sc55_mix(int16_t *stereo, size_t frames)
{
   size_t r, avail, n, i;
   int vol = volume_percent;
   if (!px68k_sc55_enabled() || !stereo || !frames) return;

   if (!worker_running)
      worker_fill(frames);          /* inline fallback: old behaviour */

   r = PX_LOAD_SIZE(&au_r);
   avail = PX_LOAD_SIZE(&au_w) - r;
   n = frames < avail ? frames : avail;
   for (i = 0; i < n; i++)
   {
      const int32_t *src = au_ring + ((r + i) & (PX_SC55_AU_RING - 1)) * 2;
      int32_t l = (int32_t)(((int64_t)src[0] * vol) / 100);
      int32_t rr = (int32_t)(((int64_t)src[1] * vol) / 100);
      stereo[i * 2] = clamp16((int32_t)stereo[i * 2] + l);
      stereo[i * 2 + 1] = clamp16((int32_t)stereo[i * 2 + 1] + rr);
   }
   PX_STORE_SIZE(&au_r, r + n);

#ifdef PX_SC55_THREADED
   if (worker_running)
      SetEvent(worker_wake);
#endif

   /* Missing frames are left silent; the emulator never waits for the
    * synth.  Ignore the first seconds, while the firmware boots and the
    * worker fills its lead. */
   if (n < frames && frames_mixed > (size_t)PX_SC55_OUT_RATE * 5 && !underrun_reported)
   {
      sc55_log(RETRO_LOG_WARN, "synth is slower than real time on this machine (audio underrun)");
      underrun_reported = 1;
   }
   frames_mixed += frames;
}
