/*
  libretro wrapper for the 3DOplay / libFreeDO core (the Xbox 360 build with
  DSP threading and the shift/mask CEL engine).

  This replaces the original 3dox frontend (3dox.cpp / GameConsole.cpp /
  DxSound.cpp / ConsoleInput.cpp / *GameSource*) with the standard libretro
  callbacks (video, audio, input, savestate, sram). The emulation core in
  libFreeDO/ is used unmodified so its known-good graphics/threading behaviour
  is preserved.

  Interface reminder (see libFreeDO/3doplay.h):
    _freedo_Interface(FDP_*, datum)     - we call into the core
    _ext_Interface(EXT_*, datum)        - the core calls back into us

  Threading: libFreeDO spawns its own DSP thread inside _3do_Init() when built
  with THREADING (Xbox 360). It calls EXT_PUSH_SAMPLE one sample at a time from
  that thread; we deposit them in a small ring and hand them to audio_batch_cb
  from retro_run (see the audio bridge comment). Delivering from retro_run is
  what lets Salvia's audio sync throttle the emulation.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#include <process.h>   /* _beginthreadex para el hilo prefetcher del CD */
#endif

#include "libretro.h"

#include "libFreeDO/3doplay.h"   /* VDLFrame, FDP_*, EXT_*, _freedo_Interface */

/* The core's DSP thread only emits audio while this is true. It used to live
   in DxSound.cpp (part of the old frontend we are replacing), so define it
   here and leave it enabled. */
UINT8 bAudOkay = 1;

/* Madam.cpp's CEL worker thread self-terminates (_endthread) when this is 0.
   It was the old frontend's run flag (3dox.cpp). Keep it set so the CEL thread
   stays alive for the life of the core. */
int running = 1;

/* --------------------------------------------------------------------- */
/* 3DO memory sizes (see libFreeDO/arm.cpp)                              */
/* --------------------------------------------------------------------- */
#define THREEDO_ROM_SIZE    (2 * 1024 * 1024)   /* pRom = ROMSIZE*2 (bios+font) */
#define THREEDO_NVRAM_SIZE  (32 * 1024)         /* NVRAMSIZE = 65536>>1          */
#define THREEDO_SECTOR_SIZE 2048

#define VIDEO_WIDTH   320
#define VIDEO_HEIGHT  240
#define AUDIO_RATE    44100

/* --------------------------------------------------------------------- */
/* libretro callbacks                                                    */
/* --------------------------------------------------------------------- */
static retro_environment_t      environ_cb;
static retro_video_refresh_t    video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t       input_poll_cb;
static retro_input_state_t      input_state_cb;
static retro_log_printf_t       log_cb;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   {
      char buf[512];
      va_list ap;
      va_start(ap, fmt);
      vsprintf(buf, fmt, ap);
      va_end(ap);
      OutputDebugStringA(buf);
   }
}

/* --------------------------------------------------------------------- */
/* Core-owned buffers                                                    */
/* --------------------------------------------------------------------- */
static VDLFrame     *g_frame        = NULL;    /* target of FDP_DO_*_MT       */
static unsigned char *g_bios        = NULL;    /* THREEDO_ROM_SIZE            */
static unsigned int  g_bios_len     = 0;
static unsigned int *g_video        = NULL;    /* VIDEO_WIDTH*VIDEO_HEIGHT (32bpp max) */
static bool          g_use_565      = false;   /* RGB565 vs XRGB8888 output    */

/* Disc image. Formats:
     .iso        -> cooked 2048-byte sectors (data offset 0)
     .bin        -> raw 2352/2448-byte sectors (Mode1 data@16, Mode2 data@24)
     .cue        -> parsed to locate its .bin, then treated as raw
     .chd        -> handled by disc_chd.c (bundled deps/libchdr)
   The core always asks for the 2048-byte user data of a sector (EXT_READ2048);
   for iso/bin we keep the on-disc sector size + data offset and translate. */
static FILE         *g_disc          = NULL;
static unsigned int  g_disc_sectors  = 0;
static unsigned int  g_cur_sector    = 0;
static unsigned int  g_sector_size   = THREEDO_SECTOR_SIZE; /* 2048 / 2352 / 2448 */
static unsigned int  g_sector_dataoff= 0;                   /* 0 / 16 / 24         */

/* Read-ahead cache for the raw (.iso/.bin/.cue) disc path. FMV streams sequential
   sectors from a slow USB file; one fread per sector tanks the framerate (we saw
   CD up to ~285 ms/s on the ARM thread). On a miss we do ONE big fread of up to
   CD_CACHE_SECTORS sectors and then serve subsequent sequential reads from RAM.
   (.chd already caches hunks inside chd_stream, so this only covers the FILE*
   path.) g_cd_cache holds full raw sectors; user data is extracted at read time. */
#define CD_CACHE_SECTORS 64
#define CD_CACHE_MIN      8    /* prefetch en un salto ALEATORIO: pequeño, para no
                                  amplificar x64 cada lectura dispersa (cargas/seeks
                                  disparaban picos de cientos de ms que estancaban el
                                  ARM y desincronizaban el audio). El secuencial sube
                                  a CD_CACHE_SECTORS en cuanto se detecta streaming. */
static unsigned char g_cd_cache[CD_CACHE_SECTORS * 2448];
static int           g_cd_cache_start = -1;   /* first cached sector (-1 = empty) */
static int           g_cd_cache_count = 0;    /* sectors currently cached         */
static char          g_bin_path[1024];                      /* scratch for .cue    */

/* .chd backend (disc_chd.c, C — keeps libchdr's headers out of this C++ file). */
   int  cd3do_chd_open(const char *path);   /* -> sector count, or <=0 on error */
   long cd3do_chd_read(unsigned lba, void *buf, unsigned bufsize);
   void cd3do_chd_close(void);
static bool          g_chd_open      = false;

/* Log hook so the C chd backend can report through the libretro logger. */
void cd3do_log(const char *s)
{
   if (log_cb) log_cb(RETRO_LOG_INFO, "[3dox] %s\n", s);
}

/* ===================== CD ASINCRONO (prefetch en HW3) =====================
   El core lee sectores SINCRONO en el hilo del ARM (Iso.cpp GetDataFifo ->
   _3do_Read2048 -> EXT_READ2048 -> fread del USB). En rafagas eso estanca el
   ARM cientos de ms -> caida de fps -> el DSP se queda sin CPU -> el audio
   salta. Aqui un hilo PREFETCHER (HW3: core1 ocioso; es I/O-bound, esta
   BLOQUEADO en el fread casi siempre) lee por delante de g_cur_sector en un
   ring circular; EXT_READ2048 sirve desde RAM (hit) sin bloquear. En un salto
   aleatorio (miss) cae a lectura sincrona (fallback). Cubre iso/bin y chd.
   Dos locks: g_cd_ring_cs (breve, slots del ring) y g_cd_io_cs (fread/chd_read
   y el open/close del disco). El camino HIT solo toca ring_cs -> nunca bloquea
   en I/O. Toggle: 3dox_cd_async (por defecto ON; OFF = ruta sincrona adaptativa). */
#define CD_RING_SECTORS 256   /* sectores por delante (256 * 2048 = 512 KB) */
#define CD_PF_CHUNK      64   /* sectores por fread del prefetcher (fichero) */
static unsigned char     g_cd_ring[CD_RING_SECTORS][THREEDO_SECTOR_SIZE];
static int               g_cd_ring_sec[CD_RING_SECTORS];  /* sector en cada slot, -1 vacio */
static CRITICAL_SECTION  g_cd_ring_cs;
static CRITICAL_SECTION  g_cd_io_cs;
static HANDLE            g_cd_ev      = NULL;   /* despierta al prefetcher (auto-reset) */
static HANDLE            g_cd_thread  = NULL;
static volatile long     g_cd_async   = 1;      /* toggle: 1 = prefetcher activo */
static volatile long     g_cd_run     = 1;      /* 0 = terminar el hilo          */
static volatile long     g_cd_target  = 0;      /* sector que el ARM quiere ahora */
static bool              g_cd_ready   = false;  /* CS/evento/hilo ya creados      */

/* Lee UN sector (2048 de datos de usuario) del backend real (chd o fichero).
   LENTO (USB): bajo g_cd_io_cs (serializa con el open del disco). Lo llaman el
   prefetcher (HW3) y el fallback sincrono (ARM, solo en miss). */
static bool cd_read_userdata(unsigned int S, void *dst)
{
   bool ok = false;
   if (S >= g_disc_sectors) { memset(dst, 0, THREEDO_SECTOR_SIZE); return false; }
   EnterCriticalSection(&g_cd_io_cs);
   if (g_chd_open)
      ok = (cd3do_chd_read(S, dst, THREEDO_SECTOR_SIZE) == THREEDO_SECTOR_SIZE);
   else if (g_disc && fseek(g_disc, (long)S * g_sector_size, SEEK_SET) == 0)
   {
      unsigned char raw[2448];
      if (fread(raw, 1, g_sector_size, g_disc) == g_sector_size)
      {
         memcpy(dst, raw + g_sector_dataoff, THREEDO_SECTOR_SIZE);
         ok = true;
      }
   }
   LeaveCriticalSection(&g_cd_io_cs);
   if (!ok) memset(dst, 0, THREEDO_SECTOR_SIZE);
   return ok;
}

/* Hilo prefetcher: mantiene lleno [target, target+RING) por delante del ARM. */
static unsigned int __stdcall cd_prefetch_thread(void *unused)
{
   static unsigned char staging[CD_PF_CHUNK * 2448];   /* BSS, no stack */
   unsigned int fill = 0;
   (void)unused;
   XSetThreadProcessor(GetCurrentThread(), 3);   /* HW3 (core1 ocioso); I/O-bound */
   while (g_cd_run)
   {
      WaitForSingleObject(g_cd_ev, 15);   /* despierta en OnSector, o cada 15 ms */
      if (!g_cd_async) continue;
      for (;;)
      {
         unsigned int t = (unsigned int)g_cd_target;
         unsigned int n, i;
         if (!g_cd_async) break;                                        /* pausado (open/close de disco) */
         if (fill < t || fill > t + CD_RING_SECTORS) fill = t;          /* resync (salto/inicio) */
         if (!g_cd_run || fill >= t + CD_RING_SECTORS || fill >= g_disc_sectors) break;

         n = (t + CD_RING_SECTORS) - fill;         /* huecos hasta llenar la ventana */
         if (n > CD_PF_CHUNK)              n = CD_PF_CHUNK;
         if (fill + n > g_disc_sectors)    n = g_disc_sectors - fill;
         if (n == 0) break;

         if (g_chd_open)
         {
            /* chd: por-sector (su cache de hunks hace barato el secuencial). */
            unsigned char one[THREEDO_SECTOR_SIZE];
            for (i = 0; i < n && g_cd_run; i++)
               if (cd_read_userdata(fill + i, one))
               {
                  int slot = (int)((fill + i) % CD_RING_SECTORS);
                  EnterCriticalSection(&g_cd_ring_cs);
                  g_cd_ring_sec[slot] = (int)(fill + i);
                  memcpy(g_cd_ring[slot], one, THREEDO_SECTOR_SIZE);
                  LeaveCriticalSection(&g_cd_ring_cs);
               }
         }
         else if (g_disc)
         {
            /* fichero: UN fread de n sectores crudos -> extrae y reparte. */
            size_t got = 0;
            EnterCriticalSection(&g_cd_io_cs);
            if (fseek(g_disc, (long)fill * g_sector_size, SEEK_SET) == 0)
               got = fread(staging, g_sector_size, n, g_disc);
            LeaveCriticalSection(&g_cd_io_cs);
            EnterCriticalSection(&g_cd_ring_cs);
            for (i = 0; i < got; i++)
            {
               int slot = (int)((fill + i) % CD_RING_SECTORS);
               g_cd_ring_sec[slot] = (int)(fill + i);
               memcpy(g_cd_ring[slot], staging + i * g_sector_size + g_sector_dataoff,
                      THREEDO_SECTOR_SIZE);
            }
            LeaveCriticalSection(&g_cd_ring_cs);
            if (got == 0) break;
            n = (unsigned int)got;
         }
         else break;   /* sin disco */

         fill += n;
      }
   }
   return 0;
}

static void cd_async_reset(void)   /* invalida el ring (nuevo disco / seek grande) */
{
   int i;
   if (!g_cd_ready) return;
   EnterCriticalSection(&g_cd_ring_cs);
   for (i = 0; i < CD_RING_SECTORS; i++) g_cd_ring_sec[i] = -1;
   LeaveCriticalSection(&g_cd_ring_cs);
}

static void cd_async_start(void)   /* crea locks/evento/hilo UNA vez */
{
   int i;
   if (g_cd_ready) return;
   InitializeCriticalSection(&g_cd_ring_cs);
   InitializeCriticalSection(&g_cd_io_cs);
   for (i = 0; i < CD_RING_SECTORS; i++) g_cd_ring_sec[i] = -1;
   g_cd_ev     = CreateEvent(NULL, 0, 0, NULL);   /* auto-reset */
   g_cd_run    = 1;
   g_cd_thread = (HANDLE)_beginthreadex(NULL, 0, cd_prefetch_thread, NULL, 0, NULL);
   g_cd_ready  = true;
}

/* NVRAM lives inside the core; we expose the pointer as SRAM */
static void         *g_nvram_ptr    = NULL;
static bool          g_nvram_checked = false;  /* format-if-blank done for this load? */

/* Perf instrumentation: ARM frame time (this/main thread) vs CEL blitter time
   (worker thread, measured in Madam.cpp). Logs once/sec so we can see whether
   the CPU (ARM) has headroom and how slow the CEL really is. */
extern "C" void      _madam_GetCelStats(long *us, long *renders, long *triggers);
extern "C" void      _dsp_GetStats(long *us, long *samples, long *peak);
static double        g_arm_us_accum = 0.0;
static int           g_arm_frames   = 0;
static double        g_cd_us_accum  = 0.0;   /* tiempo en lecturas de sector (hilo ARM) */
static int           g_cd_reads     = 0;
static unsigned int  g_perf_last_ms = 0;
static LARGE_INTEGER g_perf_qpf     = {0};

/* --------------------------------------------------------------------- */
/* Audio bridge (DSP thread -> retro_run).                                */
/*                                                                        */
/* The DSP runs on its own thread (HW5) and produces one stereo sample per */
/* EXT_PUSH_SAMPLE. But Salvia applies its audio sync (WriteBlocking       */
/* backpressure / rate control) to whichever thread calls audio_batch_cb,  */
/* and that MUST be the retro_run thread — otherwise the emulation is never */
/* throttled and runs away (which also overflowed the DSP semaphore ->     */
/* STATUS_SEMAPHORE_LIMIT_EXCEEDED). So the DSP thread only deposits samples */
/* into this small bounded ring, and retro_run drains it to audio_batch_cb */
/* once per frame. A threaded DSP genuinely needs this bridge; the         */
/* "no buffer" model only works when the DSP runs inline on the emulation  */
/* thread (as in pcsxr). The counting semaphore in _3do_sys.cpp guarantees */
/* the DSP thread is asked for every sample, so the ring gets the full     */
/* ~735 frames per video frame.                                           */
/*                                                                        */
/* Channels are split BY VALUE (mask/shift): endian-neutral, each 16-bit   */
/* sample exact on LE and BE, left = low 16 (IMem[0x3FE]).                 */
/* --------------------------------------------------------------------- */
#define AUDIO_RING_FRAMES 8192
static int16_t           g_audio_ring[AUDIO_RING_FRAMES * 2];
static volatile unsigned g_audio_wr = 0;   /* written by DSP thread   */
static volatile unsigned g_audio_rd = 0;   /* read by retro_run       */
static CRITICAL_SECTION  g_audio_cs;
static bool              g_audio_cs_ready = false;

static void audio_push_sample(unsigned int packed)   /* DSP thread */
{
   if (!g_audio_cs_ready)   /* core torn down; DSP thread can't be joined */
      return;
   EnterCriticalSection(&g_audio_cs);
   {
      unsigned next = (g_audio_wr + 1) % AUDIO_RING_FRAMES;
      if (next != g_audio_rd)              /* drop rather than block the DSP */
      {
         g_audio_ring[g_audio_wr * 2 + 0] = (int16_t)(packed & 0xFFFF);          /* left  */
         g_audio_ring[g_audio_wr * 2 + 1] = (int16_t)((packed >> 16) & 0xFFFF);  /* right */
         g_audio_wr = next;
      }
   }
   LeaveCriticalSection(&g_audio_cs);
}

static void audio_drain(void)   /* retro_run thread */
{
   static int16_t batch[AUDIO_RING_FRAMES * 2];
   unsigned n = 0;

   if (!g_audio_cs_ready)
      return;

   EnterCriticalSection(&g_audio_cs);
   while (g_audio_rd != g_audio_wr && n < AUDIO_RING_FRAMES)
   {
      batch[n * 2 + 0] = g_audio_ring[g_audio_rd * 2 + 0];
      batch[n * 2 + 1] = g_audio_ring[g_audio_rd * 2 + 1];
      g_audio_rd = (g_audio_rd + 1) % AUDIO_RING_FRAMES;
      n++;
   }
   LeaveCriticalSection(&g_audio_cs);

   if (n && audio_batch_cb)
      audio_batch_cb(batch, n);   /* single delivery, on the retro_run thread */
}

/* Empty the ring (used on game change / reset so a new game does not start by
   playing the tail of the previous one). Safe only when no frame is running
   (the DSP thread is then idle and not writing). */
static void audio_ring_reset(void)
{
   if (!g_audio_cs_ready)
      return;
   EnterCriticalSection(&g_audio_cs);
   g_audio_wr = g_audio_rd = 0;
   LeaveCriticalSection(&g_audio_cs);
}

/* --------------------------------------------------------------------- */
/* Input -> 3DO PBUS (one joypad; layout matches ConsoleInput.cpp)       */
/* --------------------------------------------------------------------- */
static unsigned char g_pbus[16];

static int pad(unsigned id)
{
   if (!input_state_cb)
      return 0;
   return input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id) ? 1 : 0;
}

static unsigned char pbus_low_byte(void)
{
   unsigned char v = 0;
   if (pad(RETRO_DEVICE_ID_JOYPAD_L))      v |= 0x04;
   if (pad(RETRO_DEVICE_ID_JOYPAD_R))      v |= 0x08;
   if (pad(RETRO_DEVICE_ID_JOYPAD_SELECT)) v |= 0x10;  /* X / stop   */
   if (pad(RETRO_DEVICE_ID_JOYPAD_START))  v |= 0x20;  /* P / play   */
   if (pad(RETRO_DEVICE_ID_JOYPAD_X))      v |= 0x40;  /* C          */
   if (pad(RETRO_DEVICE_ID_JOYPAD_A))      v |= 0x80;  /* B          */
   return v;
}

static unsigned char pbus_high_byte(void)
{
   unsigned char v = 0;
   if (pad(RETRO_DEVICE_ID_JOYPAD_B))     v |= 0x01;   /* A          */
   if (pad(RETRO_DEVICE_ID_JOYPAD_LEFT))  v |= 0x02;
   if (pad(RETRO_DEVICE_ID_JOYPAD_RIGHT)) v |= 0x04;
   if (pad(RETRO_DEVICE_ID_JOYPAD_UP))    v |= 0x08;
   if (pad(RETRO_DEVICE_ID_JOYPAD_DOWN))  v |= 0x10;
   v |= 0x80;                                          /* present    */
   return v;
}

static void pbus_build(void)
{
   unsigned char lo = pbus_low_byte();
   unsigned char hi = pbus_high_byte();

   memset(g_pbus, 0, sizeof(g_pbus));
   g_pbus[0x0] = 0x00;
   g_pbus[0x1] = 0x48;
   g_pbus[0x5] = hi;   /* device 0 high byte (A / dpad / present) */
   g_pbus[0x6] = lo;   /* device 0 low  byte (B C P X L R)        */
   g_pbus[0xC] = 0x00;
   g_pbus[0xD] = 0x80;
}

/* --------------------------------------------------------------------- */
/* The callback the core uses to reach us                                */
/* --------------------------------------------------------------------- */
static void *ext_interface(int procedure, void *datum)
{
   switch (procedure)
   {
      case EXT_READ_ROMS:
         /* Copy the loaded BIOS into the core's 2 MB ROM buffer. */
         if (g_bios && datum)
            memcpy(datum, g_bios, THREEDO_ROM_SIZE);
         break;

      case EXT_READ_NVRAM:
         /* NVRAM is loaded straight into the core's buffer via SRAM API,
            so nothing to do here (kept for completeness). */
         break;

      case EXT_WRITE_NVRAM:
         break;

      case EXT_SWAPFRAME:
         return (void*)g_frame;

      case EXT_PUSH_SAMPLE:
         audio_push_sample((unsigned int)(uintptr_t)datum);
         break;

      case EXT_GET_PBUSLEN:
         return (void*)(uintptr_t)sizeof(g_pbus);

      case EXT_GETP_PBUSDATA:
         return (void*)g_pbus;

      case EXT_DEBUG_PRINT:
         if (datum)
            OutputDebugStringA((const char*)datum);
         break;

      case EXT_FRAMETRIGGER_MT:
         /* Core reached line 1: produce the display frame for what MADAM has
            drawn into VRAM (runs the VDLP over all 263 lines). */
         _freedo_Interface(FDP_DO_FRAME_MT, (void*)g_frame);
         break;

      case EXT_READ2048:
         if (datum)
         {
            bool ok = false;
            LARGE_INTEGER _c0, _c1;
            if (!g_perf_qpf.QuadPart) QueryPerformanceFrequency(&g_perf_qpf);
            QueryPerformanceCounter(&_c0);
            if (g_cur_sector < g_disc_sectors)
            {
               if (g_cd_async && g_cd_ready)
               {
                  /* Sirve desde el ring que llena el prefetcher (HW3): hit = sin
                     bloquear en I/O; miss (salto aleatorio) = lectura sincrona. */
                  unsigned int S = g_cur_sector;
                  int slot = (int)(S % CD_RING_SECTORS);
                  EnterCriticalSection(&g_cd_ring_cs);
                  if (g_cd_ring_sec[slot] == (int)S)
                  {
                     memcpy(datum, g_cd_ring[slot], THREEDO_SECTOR_SIZE);
                     ok = true;
                  }
                  LeaveCriticalSection(&g_cd_ring_cs);
                  if (!ok)
                  {
                     /* Miss: el prefetcher no habia llegado. En vez de 1 sector,
                        rellena un LOTE secuencial en el ring con UN solo fread
                        (fichero): colapsa una rafaga de N misses -cada uno ~2.8ms
                        de USB- en ~1 lectura; la siguiente rafaga ya acierta.
                        Solo el ARM ejecuta EXT_READ2048 -> staging propio, sin
                        carrera con el del prefetcher (HW3). chd: 1 sector (su
                        cache de hunks ya abarata el secuencial). */
                     if (g_disc)
                     {
                        static unsigned char arm_staging[CD_PF_CHUNK * 2448];
                        unsigned int n = CD_PF_CHUNK, i;
                        size_t got = 0;
                        if (S + n > g_disc_sectors) n = g_disc_sectors - S;
                        EnterCriticalSection(&g_cd_io_cs);
                        if (fseek(g_disc, (long)S * g_sector_size, SEEK_SET) == 0)
                           got = fread(arm_staging, g_sector_size, n, g_disc);
                        LeaveCriticalSection(&g_cd_io_cs);
                        if (got > 0)
                        {
                           EnterCriticalSection(&g_cd_ring_cs);
                           for (i = 0; i < got; i++)
                           {
                              int sl = (int)((S + i) % CD_RING_SECTORS);
                              g_cd_ring_sec[sl] = (int)(S + i);
                              memcpy(g_cd_ring[sl],
                                     arm_staging + i * g_sector_size + g_sector_dataoff,
                                     THREEDO_SECTOR_SIZE);
                           }
                           LeaveCriticalSection(&g_cd_ring_cs);
                           memcpy(datum, arm_staging + g_sector_dataoff,
                                  THREEDO_SECTOR_SIZE);
                           ok = true;
                        }
                        else
                           memset(datum, 0, THREEDO_SECTOR_SIZE);
                     }
                     else
                     {
                        ok = cd_read_userdata(S, datum);   /* chd: 1 sector */
                        if (ok)
                        {
                           EnterCriticalSection(&g_cd_ring_cs);
                           g_cd_ring_sec[slot] = (int)S;
                           memcpy(g_cd_ring[slot], datum, THREEDO_SECTOR_SIZE);
                           LeaveCriticalSection(&g_cd_ring_cs);
                        }
                     }
                  }
               }
               else if (g_chd_open)
               {
                  ok = (cd3do_chd_read(g_cur_sector, datum, THREEDO_SECTOR_SIZE)
                        == THREEDO_SECTOR_SIZE);
               }
               else if (g_disc)
               {
                  int S = (int)g_cur_sector;

                  /* Miss? refill the cache with ONE big fread of up to
                     CD_CACHE_SECTORS sequential (full, raw) sectors. */
                  if (S < g_cd_cache_start || S >= g_cd_cache_start + g_cd_cache_count)
                  {
                     /* Prefetch GRANDE solo si esta lectura continua el bloque
                        cacheado (streaming secuencial: FMV/musica). En un salto
                        aleatorio (cargas), leer poco -> evita amplificar x64 cada
                        lectura dispersa. Un run contiguo (un fichero de nivel) sube
                        a 64 tras la 1a lectura, porque S pasa a == start+count. */
                     int want = (S == g_cd_cache_start + g_cd_cache_count)
                                ? CD_CACHE_SECTORS : CD_CACHE_MIN;
                     if ((unsigned)S + (unsigned)want > g_disc_sectors)
                        want = (int)g_disc_sectors - S;
                     if (want > 0 &&
                         fseek(g_disc, (long)S * g_sector_size, SEEK_SET) == 0)
                     {
                        size_t got = fread(g_cd_cache, g_sector_size, (size_t)want, g_disc);
                        g_cd_cache_start = S;
                        g_cd_cache_count = (int)got;
                     }
                     else
                        g_cd_cache_count = 0;
                  }

                  /* Serve the 2048 bytes of user data from the cached sector
                     (skipping any sync/header via the data offset). */
                  if (S >= g_cd_cache_start && S < g_cd_cache_start + g_cd_cache_count)
                  {
                     memcpy(datum,
                            g_cd_cache + (size_t)(S - g_cd_cache_start) * g_sector_size
                                       + g_sector_dataoff,
                            THREEDO_SECTOR_SIZE);
                     ok = true;
                  }
               }
            }
            if (!ok)
               memset(datum, 0, THREEDO_SECTOR_SIZE);
            QueryPerformanceCounter(&_c1);
            g_cd_us_accum += (double)(_c1.QuadPart - _c0.QuadPart) * 1000000.0 / (double)g_perf_qpf.QuadPart;
            g_cd_reads++;
         }
         break;

      case EXT_GET_DISC_SIZE:
         return (void*)(uintptr_t)g_disc_sectors;

      case EXT_ON_SECTOR:
         g_cur_sector = (unsigned int)(uintptr_t)datum;
         if (g_cd_async && g_cd_ready)
         {
            g_cd_target = (long)g_cur_sector;   /* mueve la ventana de prefetch */
            SetEvent(g_cd_ev);
         }
         break;

      default:
         break;
   }
   return NULL;
}

/* --------------------------------------------------------------------- */
/* VDLFrame -> framebuffer (see 3dox.cpp Console_FrameDone).              */
/* The 3DO CLUT gives 8 bits per channel, so XRGB8888 is lossless and     */
/* RGB565 is a straight down-shift. The chosen format is decided at load   */
/* time from the core option (see retro_load_game).                       */
/* --------------------------------------------------------------------- */
/* Ported from opera (opera_vdlp.c vdlp_render_pixel_*):
   - pixel 0 -> se muestra el color de BACKGROUND del VDL (antes se pintaba CLUT[0]).
   - si el bit clut_bypass (display-ctrl bit 25) esta activo y el pixel tiene el
     bit 15, el valor es color DIRECTO 5-5-5 (fixed CLUT), no un indice al CLUT.
   xBACKGROUND ya viene como 0x00RRGGBB (8-8-8, ver vdlp.cpp), xOUTCONTROLL es la
   palabra de control cruda. Todo shift/mask -> BE-safe. */
static void render_frame(void)
{
   int line, pix;

   if (g_use_565)
   {
      unsigned short *dst = (unsigned short*)g_video;
      for (line = 0; line < VIDEO_HEIGHT; line++)
      {
         VDLLine        *lp  = (VDLLine*)&g_frame->lines[line];
         unsigned short *sp  = lp->line;
         unsigned int    bg  = lp->xBACKGROUND;                /* 0x00RRGGBB */
         int          bypass = (lp->xOUTCONTROLL >> 25) & 1;   /* clut_bypass */
         unsigned short bg565 = (unsigned short)(((((bg>>16)&0xFF)>>3)<<11) |
                                                 ((((bg>> 8)&0xFF)>>2)<< 5) |
                                                 ( ((bg     )&0xFF)>>3));
         for (pix = 0; pix < VIDEO_WIDTH; pix++)
         {
            unsigned short p = *sp++;
            int red, green, blue;
            if (p == 0) { *dst++ = bg565; continue; }
            if (bypass && (p & 0x8000))
            {
               red   = (p >> 10) & 0x1F; red   = (red   << 3) | (red   >> 2);
               green = (p >>  5) & 0x1F; green = (green << 3) | (green >> 2);
               blue  =  p        & 0x1F; blue  = (blue  << 3) | (blue  >> 2);
            }
            else
            {
               blue  = lp->xCLUTB[(p >>  0) & 0x1F];
               green = lp->xCLUTG[(p >>  5) & 0x1F];
               red   = lp->xCLUTR[(p >> 10) & 0x1F];
            }
            *dst++ = (unsigned short)(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
         }
      }
   }
   else
   {
      unsigned int *dst = g_video;
      for (line = 0; line < VIDEO_HEIGHT; line++)
      {
         VDLLine        *lp  = (VDLLine*)&g_frame->lines[line];
         unsigned short *sp  = lp->line;
         unsigned int    bg  = lp->xBACKGROUND;                /* ya 0x00RRGGBB */
         int          bypass = (lp->xOUTCONTROLL >> 25) & 1;
         for (pix = 0; pix < VIDEO_WIDTH; pix++)
         {
            unsigned short p = *sp++;
            int red, green, blue;
            if (p == 0) { *dst++ = bg; continue; }
            if (bypass && (p & 0x8000))
            {
               red   = (p >> 10) & 0x1F; red   = (red   << 3) | (red   >> 2);
               green = (p >>  5) & 0x1F; green = (green << 3) | (green >> 2);
               blue  =  p        & 0x1F; blue  = (blue  << 3) | (blue  >> 2);
            }
            else
            {
               blue  = lp->xCLUTB[(p >>  0) & 0x1F];
               green = lp->xCLUTG[(p >>  5) & 0x1F];
               red   = lp->xCLUTR[(p >> 10) & 0x1F];
            }
            *dst++ = (red << 16) | (green << 8) | blue;
         }
      }
   }
}

/* --------------------------------------------------------------------- */
/* BIOS / disc loading                                                   */
/* --------------------------------------------------------------------- */
/* Returns the BIOS filename chosen in the '3dox_bios' core option.
   Defaults to panafz1.bin (the previous hardcoded behaviour). The value points
   into frontend-owned memory, valid until the next GET_VARIABLE; used at once. */
static const char *read_bios_option(void)
{
   struct retro_variable var;
   var.key   = "3dox_bios";
   var.value = NULL;
   if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) &&
       var.value && var.value[0])
      return var.value;
   return "panafz1.bin";
}

static int load_bios(void)
{
   const char *sysdir   = NULL;
   const char *biosname = read_bios_option();
   char path[1024];
   FILE *f;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sysdir) || !sysdir)
      return 0;

   sprintf(path, "%s%c%s", sysdir, '\\', biosname);
   f = fopen(path, "rb");
   if (!f)
   {
      sprintf(path, "%s%c%s", sysdir, '/', biosname);
      f = fopen(path, "rb");
   }
   if (!f)
   {
      log_cb(RETRO_LOG_ERROR, "[3dox]: BIOS '%s' not found in %s\n", biosname, sysdir);
      return 0;
   }

   memset(g_bios, 0, THREEDO_ROM_SIZE);
   g_bios_len = (unsigned int)fread(g_bios, 1, THREEDO_ROM_SIZE, f);
   fclose(f);
   log_cb(RETRO_LOG_INFO, "[3dox]: loaded BIOS %s (%u bytes)\n", path, g_bios_len);
   return (g_bios_len > 0);
}

/* Case-insensitive test for a trailing extension (e.g. ".cue"). */
static bool has_ext(const char *path, const char *ext)
{
   size_t lp = strlen(path), le = strlen(ext), i;
   if (lp < le)
      return false;
   for (i = 0; i < le; i++)
   {
      char a = path[lp - le + i], b = ext[i];
      if (a >= 'A' && a <= 'Z') a += 32;
      if (b >= 'A' && b <= 'Z') b += 32;
      if (a != b)
         return false;
   }
   return true;
}

/* Read the first FILE "..." entry of a .cue and resolve it relative to the
   cue's directory into g_bin_path. Returns g_bin_path or NULL. */
static const char *cue_resolve_bin(const char *cue_path)
{
   FILE *cf = fopen(cue_path, "r");
   char line[1024];
   if (!cf)
      return NULL;

   while (fgets(line, sizeof(line), cf))
   {
      const char *p = strstr(line, "FILE");
      if (!p) p = strstr(line, "file");
      if (p)
      {
         const char *q1 = strchr(p, '"');
         const char *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
         if (q1 && q2)
         {
            const char *sep  = strrchr(cue_path, '\\');
            const char *sep2 = strrchr(cue_path, '/');
            size_t dirlen, namelen;
            if (sep2 && sep2 > sep) sep = sep2;
            dirlen  = sep ? (size_t)(sep - cue_path + 1) : 0;
            namelen = (size_t)(q2 - (q1 + 1));
            if (dirlen + namelen + 1 > sizeof(g_bin_path))
               break;
            memcpy(g_bin_path, cue_path, dirlen);
            memcpy(g_bin_path + dirlen, q1 + 1, namelen);
            g_bin_path[dirlen + namelen] = '\0';
            fclose(cf);
            return g_bin_path;
         }
      }
   }
   fclose(cf);
   return NULL;
}

/* Distinguish a cooked 2048 image (.iso) from a raw CD image (.bin: 2352/2448
   with a 12-byte sync mark) and pick the user-data offset from the sector mode.
   Sets g_sector_size / g_sector_dataoff. */
static void detect_sector_layout(FILE *f)
{
   static const unsigned char sync[12] =
      { 0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00 };
   unsigned char hdr[16];
   long bytes;

   g_sector_size    = THREEDO_SECTOR_SIZE;  /* cooked .iso: 2048, data at 0 */
   g_sector_dataoff = 0;

   fseek(f, 0, SEEK_SET);
   if (fread(hdr, 1, 16, f) == 16 && memcmp(hdr, sync, 12) == 0)
   {
      fseek(f, 0, SEEK_END);
      bytes = ftell(f);
      if ((bytes % 2448) == 0 && (bytes % 2352) != 0)
         g_sector_size = 2448;   /* raw + 96-byte subchannel */
      else
         g_sector_size = 2352;
      g_sector_dataoff = (hdr[15] == 2) ? 24 : 16;   /* Mode2 @24, Mode1 @16 */
   }
   fseek(f, 0, SEEK_SET);
}

static void open_disc(const char *path)
{
   const char *open_path = path;
   long bytes;

   g_disc = NULL;
   g_disc_sectors = 0;
   g_cur_sector = 0;
   g_sector_size = THREEDO_SECTOR_SIZE;
   g_sector_dataoff = 0;
   g_chd_open = false;
   g_cd_cache_start = -1;   /* invalidate the read-ahead cache for the new disc */
   g_cd_cache_count = 0;
   if (g_cd_ready) InterlockedExchange(&g_cd_async, 0);   /* pausa el prefetcher durante el (re)open */

   /* Diagnostic: shows exactly what the frontend handed us. If a .chd boots to
      BIOS, check whether this line shows the .chd path (frontend passed it, so
      the chd open failed) or "(null)"/something else (frontend never passed it,
      i.e. an extensions/config issue on the Salvia side). */
   log_cb(RETRO_LOG_INFO, "[3dox]: open_disc path='%s'\n", path ? path : "(null)");

   if (!path || !path[0])
      return;   /* BIOS-only mode */

   if (has_ext(path, ".chd"))
   {
      int n;
      if (g_cd_ready) EnterCriticalSection(&g_cd_io_cs);
      n = cd3do_chd_open(path);
      if (n > 0)
      {
         g_chd_open = true;
         g_disc_sectors = (unsigned int)n;
      }
      if (g_cd_ready) { LeaveCriticalSection(&g_cd_io_cs); cd_async_reset(); }
      if (n > 0)
         log_cb(RETRO_LOG_INFO, "[3dox]: disc %s (chd, %u sectors)\n",
                path, g_disc_sectors);
      else
         log_cb(RETRO_LOG_ERROR,
                "[3dox]: cannot open chd %s (err %d: -1 chd_open, -2 header,"
                " -3 oom, -4 no tracks)\n", path, n);
      return;
   }

   if (has_ext(path, ".cue"))
   {
      open_path = cue_resolve_bin(path);
      if (!open_path)
      {
         log_cb(RETRO_LOG_ERROR, "[3dox]: cannot parse cue %s\n", path);
         return;
      }
   }

   if (g_cd_ready) EnterCriticalSection(&g_cd_io_cs);
   g_disc = fopen(open_path, "rb");
   if (!g_disc)
   {
      if (g_cd_ready) LeaveCriticalSection(&g_cd_io_cs);
      log_cb(RETRO_LOG_ERROR, "[3dox]: cannot open disc %s\n", open_path);
      return;
   }

   detect_sector_layout(g_disc);

   fseek(g_disc, 0, SEEK_END);
   bytes = ftell(g_disc);
   fseek(g_disc, 0, SEEK_SET);
   g_disc_sectors = (unsigned int)(bytes / g_sector_size);
   if (g_cd_ready) { LeaveCriticalSection(&g_cd_io_cs); cd_async_reset(); }
   log_cb(RETRO_LOG_INFO,
          "[3dox]: disc %s (%u sectors, %u B/sector, data@%u)\n",
          open_path, g_disc_sectors, g_sector_size, g_sector_dataoff);
}

/* --------------------------------------------------------------------- */
/* Core options                                                          */
/* --------------------------------------------------------------------- */
static const struct retro_variable g_vars[] =
{
   /* First listed value is the default (legacy variables API). */
   { "3dox_pixel_format", "Video pixel format (needs reload); XRGB8888|RGB565" },
   { "3dox_swi_hle", "SWI HLE math accel (may boost 3D games); disabled|enabled" },
   { "3dox_cd_async", "CD async prefetch (menos cortes de audio en cargas); enabled|disabled" },
   { "3dox_bios", "BIOS (needs reload); panafz1.bin|panafz1j.bin|panafz1j-norsa.bin|panafz10.bin|panafz10-norsa.bin|panafz10e-anvil.bin|panafz10e-anvil-norsa.bin|goldstar.bin|sanyotry.bin|3do_arcade_saot.bin" },
   { NULL, NULL }
};

/* Reads the pixel-format option into g_use_565. Called at load time. */
static void read_pixel_format_option(void)
{
   struct retro_variable var;
   var.key   = "3dox_pixel_format";
   var.value = NULL;
   g_use_565 = false;
   if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      g_use_565 = (strcmp(var.value, "RGB565") == 0);
}

/* Reads the SWI-HLE option and pushes it into the core. Default OFF. Safe to
   call after FDP_INIT (the toggle is a plain global that survives INIT/DESTROY). */
static void read_swi_hle_option(void)
{
   struct retro_variable var;
   int on = 0;
   var.key   = "3dox_swi_hle";
   var.value = NULL;
   if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      on = (strcmp(var.value, "enabled") == 0);
   _freedo_Interface(FDP_SET_SWI_HLE, (void*)on);
}

/* Prefetcher async del CD (hilo en HW3). ON por defecto; OFF = ruta sincrona. */
static void read_cd_async_option(void)
{
   struct retro_variable var;
   long on = 1;
   var.key   = "3dox_cd_async";
   var.value = NULL;
   if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      on = (strcmp(var.value, "disabled") != 0);
   InterlockedExchange(&g_cd_async, on);
   if (on && g_cd_ready) SetEvent(g_cd_ev);   /* despierta al prefetcher ya */
}

/* --------------------------------------------------------------------- */
/* libretro API                                                          */
/* --------------------------------------------------------------------- */
void retro_set_environment(retro_environment_t cb)
{
   bool no_game = true;
   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)g_vars);
}

void retro_set_video_refresh(retro_video_refresh_t cb)       { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)         { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb)             { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb)           { input_state_cb = cb; }

void retro_init(void)
{
   struct retro_log_callback logging;
   if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging) && logging.log)
      log_cb = logging.log;
   else
      log_cb = fallback_log;

   g_frame = (VDLFrame*)malloc(sizeof(VDLFrame));
   g_bios  = (unsigned char*)malloc(THREEDO_ROM_SIZE);
   g_video = (unsigned int*)malloc(VIDEO_WIDTH * VIDEO_HEIGHT * sizeof(unsigned int));
   if (g_frame) memset(g_frame, 0, sizeof(VDLFrame));

   InitializeCriticalSection(&g_audio_cs);
   g_audio_cs_ready = true;
   g_audio_wr = g_audio_rd = 0;
}

void retro_deinit(void)
{
   free(g_frame); g_frame = NULL;
   free(g_bios);  g_bios  = NULL;
   free(g_video); g_video = NULL;
   if (g_audio_cs_ready) { DeleteCriticalSection(&g_audio_cs); g_audio_cs_ready = false; }
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "3dox (libFreeDO)";
   info->library_version  = "1.7.3";
   info->valid_extensions = "iso|bin|cue|chd";
   info->need_fullpath    = true;      /* we open the disc ourselves */
   info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps          = 60.0;
   info->timing.sample_rate  = (double)AUDIO_RATE;
   info->geometry.base_width  = VIDEO_WIDTH;
   info->geometry.base_height = VIDEO_HEIGHT;
   info->geometry.max_width   = VIDEO_WIDTH;
   info->geometry.max_height  = VIDEO_HEIGHT;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port; (void)device;
}

bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt;

   read_pixel_format_option();
   fmt = g_use_565 ? RETRO_PIXEL_FORMAT_RGB565 : RETRO_PIXEL_FORMAT_XRGB8888;

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      /* Requested format rejected by the frontend: fall back to the other. */
      if (g_use_565)
      {
         log_cb(RETRO_LOG_WARN, "[3dox]: RGB565 rejected, falling back to XRGB8888\n");
         g_use_565 = false;
         fmt = RETRO_PIXEL_FORMAT_XRGB8888;
      }
      else
      {
         log_cb(RETRO_LOG_WARN, "[3dox]: XRGB8888 rejected, trying RGB565\n");
         g_use_565 = true;
         fmt = RETRO_PIXEL_FORMAT_RGB565;
      }
      if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      {
         log_cb(RETRO_LOG_ERROR, "[3dox]: no supported pixel format\n");
         return false;
      }
   }
   log_cb(RETRO_LOG_INFO, "[3dox]: pixel format = %s\n", g_use_565 ? "RGB565" : "XRGB8888");

   if (!load_bios())
      return false;

   open_disc(game ? game->path : NULL);
   cd_async_start();   /* crea el hilo prefetcher del CD (una sola vez) */

   /* FDP_INIT runs _3do_Init(): it calls EXT_READ_ROMS and (with THREADING)
      spawns the DSP thread, so audio + bios must be ready before this. */
   _freedo_Interface(FDP_INIT, (void*)ext_interface);

   /* CRITICAL for correct audio speed. _3do_Frame() executes 12,500,000/60 ARM
      cycles per frame (the real 3DO ARM60 runs at 12.5 MHz), but libFreeDO's
      default ARM_CLOCK is 10,000,000 (quarz.cpp). The DSP sample rate is derived
      from ARM_CLOCK, so with the wrong clock the core emits
      208333*44100/10000000 = ~919 samples/frame instead of 44100/60 = 735 ->
      at a true 60fps the audio runs ~25% too fast and overflows the buffer.
      (That is why the audio only sounded right when the framerate dropped to
      ~48fps: 48*919 ~= 44100.) The original 3dox frontend set 12.5 MHz via this
      same call; the libretro port must too. ARM_CLOCK is read live and survives
      _qrz_Init and savestate loads, so setting it once here is enough. */
   _freedo_Interface(FDP_SET_ARMCLOCK, (void*)12500000);
   read_swi_hle_option();   /* aplica la opcion SWI HLE (por defecto OFF) */
   read_cd_async_option();  /* activa/desactiva el prefetcher async del CD (por defecto ON) */

   g_nvram_ptr = _freedo_Interface(FDP_GETP_NVRAM, NULL);
   g_nvram_checked = false;   /* validate/format this game's NVRAM on the first frame */
   audio_ring_reset();   /* don't carry audio from a previously loaded game */
   return true;
}

bool retro_load_game_special(unsigned t, const struct retro_game_info *i, size_t n)
{
   (void)t; (void)i; (void)n;
   return false;
}

void retro_unload_game(void)
{
   _freedo_Interface(FDP_DESTROY, NULL);
   /* Detiene el prefetcher y drena cualquier lectura en vuelo antes de cerrar el
      disco (evita use-after-free del FILE chd en el hilo de HW3). */
   if (g_cd_ready) { InterlockedExchange(&g_cd_async, 0); EnterCriticalSection(&g_cd_io_cs); }
   if (g_disc) { fclose(g_disc); g_disc = NULL; }
   if (g_chd_open) { cd3do_chd_close(); g_chd_open = false; }
   if (g_cd_ready) { LeaveCriticalSection(&g_cd_io_cs); cd_async_reset(); }
   g_nvram_ptr = NULL;
}

/* --------------------------------------------------------------------- */
/* NVRAM formatting                                                       */
/* The 3DO expects its 32KB NVRAM to hold a formatted "LinkedMem" volume; a
   blank (all-zero) NVRAM makes games report "nvram full: Configuration not
   saved" (e.g. The Need for Speed). The core only zeroes it (EXT_READ_NVRAM is
   a no-op here; the frontend loads any saved .srm into g_nvram_ptr via the SRAM
   API AFTER retro_load_game), so we validate/format on the first retro_run: an
   already-valid NVRAM (a real save) is kept untouched, a blank/invalid one is
   formatted. Mimics opera_nvram_init / the official 3DO "format" tool. Xbox360
   is big-endian, so uint32 fields are stored native = big-endian (as the 3DO
   reads them; nv_be32 keeps it correct on a hypothetical LE build too). */

static unsigned int nv_be32(unsigned int v)
{
#if defined(MSB_FIRST) || defined(__BIG_ENDIAN__) || defined(_XBOX) || defined(BIG_ENDIAN)
   return v;                       /* PPC/Xbox360: native store is big-endian */
#else
   return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
#endif
}

#pragma pack(push,1)
typedef struct {
   unsigned char  dl_RecordType;
   unsigned char  dl_VolumeSyncBytes[5];
   unsigned char  dl_VolumeStructureVersion;
   unsigned char  dl_VolumeFlags;
   unsigned char  dl_VolumeCommentary[32];
   unsigned char  dl_VolumeIdentifier[32];
   unsigned int   dl_VolumeUniqueIdentifier;
   unsigned int   dl_VolumeBlockSize;
   unsigned int   dl_VolumeBlockCount;
   unsigned int   dl_RootUniqueIdentifier;
   unsigned int   dl_RootDirectoryBlockCount;
   unsigned int   dl_RootDirectoryBlockSize;
   unsigned int   dl_RootDirectoryLastAvatarIndex;
   unsigned int   dl_RootDirectoryAvatarList[8];
} NvDiscLabel;      /* 132 bytes, dense */
typedef struct {
   unsigned int fingerprint;
   unsigned int flinkoffset;
   unsigned int blinkoffset;
   unsigned int blockcount;
   unsigned int headerblockcount;
} NvLinkedMemBlock; /* 20 bytes */
#pragma pack(pop)

static bool nvram_is_formatted(const unsigned char *b)
{
   int i;
   if (!b)          return true;    /* nothing to format */
   if (b[0] != 1)   return false;   /* dl_RecordType                          */
   if (b[6] != 2)   return false;   /* dl_VolumeStructureVersion (LINKED_MEM)  */
   for (i = 0; i < 5; i++)
      if (b[1 + i] != 'Z') return false;   /* dl_VolumeSyncBytes             */
   return true;
}

static void nvram_format(unsigned char *buf, int size)
{
   NvDiscLabel      *dl     = (NvDiscLabel*)buf;
   NvLinkedMemBlock *anchor = (NvLinkedMemBlock*)&dl[1];
   NvLinkedMemBlock *freeb  = &anchor[1];

   memset(buf, 0, size);

   dl->dl_RecordType = 1;
   memset(dl->dl_VolumeSyncBytes, 'Z', 5);
   dl->dl_VolumeStructureVersion = 2;                 /* LINKED_MEM */
   dl->dl_VolumeFlags = 0;
   strncpy((char*)dl->dl_VolumeCommentary, "3dox formatted", 32);
   strncpy((char*)dl->dl_VolumeIdentifier, "nvram", 32);
   dl->dl_VolumeUniqueIdentifier       = nv_be32((unsigned int)-1);
   dl->dl_VolumeBlockSize              = nv_be32(1);
   dl->dl_VolumeBlockCount             = nv_be32((unsigned int)size);
   dl->dl_RootUniqueIdentifier         = nv_be32((unsigned int)-2);
   dl->dl_RootDirectoryBlockCount      = 0;
   dl->dl_RootDirectoryBlockSize       = nv_be32(1);
   dl->dl_RootDirectoryLastAvatarIndex = 0;
   dl->dl_RootDirectoryAvatarList[0]   = nv_be32((unsigned int)sizeof(NvDiscLabel));

   anchor->fingerprint      = nv_be32(0x855A02B6u);   /* ANCHORBLOCK */
   anchor->flinkoffset      = nv_be32((unsigned int)(sizeof(NvDiscLabel) + sizeof(NvLinkedMemBlock)));
   anchor->blinkoffset      = nv_be32((unsigned int)(sizeof(NvDiscLabel) + sizeof(NvLinkedMemBlock)));
   anchor->blockcount       = nv_be32((unsigned int)sizeof(NvLinkedMemBlock));
   anchor->headerblockcount = nv_be32((unsigned int)sizeof(NvLinkedMemBlock));

   freeb->fingerprint      = nv_be32(0x7AA565BDu);    /* FREEBLOCK */
   freeb->flinkoffset      = nv_be32((unsigned int)sizeof(NvDiscLabel));
   freeb->blinkoffset      = nv_be32((unsigned int)sizeof(NvDiscLabel));
   freeb->blockcount       = nv_be32((unsigned int)(size - (int)sizeof(NvDiscLabel) - (int)sizeof(NvLinkedMemBlock)));
   freeb->headerblockcount = nv_be32((unsigned int)sizeof(NvLinkedMemBlock));

   if (log_cb) log_cb(RETRO_LOG_INFO, "[3dox]: NVRAM was blank -> formatted\n");
}

void retro_run(void)
{
   if (input_poll_cb)
      input_poll_cb();
   pbus_build();

   /* First frame after the frontend has (maybe) loaded the .srm into the NVRAM:
      format it if it isn't a valid 3DO volume. Keeps real saves intact. */
   if (!g_nvram_checked)
   {
      g_nvram_checked = true;
      if (!nvram_is_formatted((const unsigned char*)g_nvram_ptr))
         nvram_format((unsigned char*)g_nvram_ptr, THREEDO_NVRAM_SIZE);
   }

   /* Runs ARM + MADAM (CEL) for one frame. At line 1 the core calls back
      EXT_FRAMETRIGGER_MT, where we run the VDLP into g_frame. */
   {
      LARGE_INTEGER _a0, _a1;
      if (!g_perf_qpf.QuadPart) QueryPerformanceFrequency(&g_perf_qpf);
      QueryPerformanceCounter(&_a0);
      _freedo_Interface(FDP_DO_EXECFRAME_MT, (void*)g_frame);
      QueryPerformanceCounter(&_a1);
      g_arm_us_accum += (double)(_a1.QuadPart - _a0.QuadPart) * 1000000.0 / (double)g_perf_qpf.QuadPart;
      g_arm_frames++;

      {
         unsigned int now = GetTickCount();
         if (g_perf_last_ms == 0) g_perf_last_ms = now;
         if (now - g_perf_last_ms >= 1000)
         {
            long   cel_us = 0, cel_n = 0, cel_trig = 0;
            long   dsp_us = 0, dsp_n = 0, dsp_peak = 0;
            double arm_ms, cel_ms;
            _madam_GetCelStats(&cel_us, &cel_n, &cel_trig);
            _dsp_GetStats(&dsp_us, &dsp_n, &dsp_peak);
            arm_ms = (g_arm_frames > 0) ? (g_arm_us_accum / g_arm_frames / 1000.0) : 0.0;
            cel_ms = (cel_n > 0)        ? ((double)cel_us / (double)cel_n / 1000.0) : 0.0;
            if (log_cb)
            {
               log_cb(RETRO_LOG_INFO,
                      "[3dox] perf: ARM %.2f ms/frame (%d loops/s, presup 16.67) | "
                      "CEL %.2f ms/render, %ld renders/s, %ld triggers/s\n",
                      arm_ms, g_arm_frames, cel_ms, cel_n, cel_trig);
               log_cb(RETRO_LOG_INFO,
                      "[3dox] perf: DSP %ld smp/s (obj 44100), %.1f ms/s busy, backlog pico %ld | "
                      "CD %d lecturas/s, %.2f ms/s\n",
                      dsp_n, dsp_us / 1000.0, dsp_peak, g_cd_reads, g_cd_us_accum / 1000.0);
            }
            g_arm_us_accum = 0.0;
            g_arm_frames   = 0;
            g_cd_us_accum  = 0.0;
            g_cd_reads     = 0;
            g_perf_last_ms = now;
         }
      }
   }

   render_frame();
   if (video_cb)
   {
      size_t pitch = (size_t)VIDEO_WIDTH * (g_use_565 ? sizeof(unsigned short)
                                                      : sizeof(unsigned int));
      video_cb(g_video, VIDEO_WIDTH, VIDEO_HEIGHT, pitch);
   }

   /* Deliver the audio the DSP thread produced this frame. Doing it here (the
      retro_run thread) is what lets Salvia's audio sync throttle the emulation
      correctly; see the audio bridge comment above. */
   audio_drain();
}

/* --------------------------------------------------------------------- */
/* Save states                                                           */
/* --------------------------------------------------------------------- */
size_t retro_serialize_size(void)
{
   return (size_t)(uintptr_t)_freedo_Interface(FDP_GET_SAVE_SIZE, NULL);
}

bool retro_serialize(void *data, size_t size)
{
   if (size < retro_serialize_size())
      return false;
   _freedo_Interface(FDP_DO_SAVE, data);
   return true;
}

bool retro_unserialize(const void *data, size_t size)
{
   (void)size;
   return _freedo_Interface(FDP_DO_LOAD, (void*)data) != NULL;
}

/* --------------------------------------------------------------------- */
/* SRAM (3DO NVRAM)                                                      */
/* --------------------------------------------------------------------- */
void *retro_get_memory_data(unsigned id)
{
   if (id == RETRO_MEMORY_SAVE_RAM)
      return g_nvram_ptr;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   if (id == RETRO_MEMORY_SAVE_RAM)
      return THREEDO_NVRAM_SIZE;
   return 0;
}

void retro_reset(void)
{
   /* Reboot the 3DO: tear the core down and re-init it. The DSP/CEL worker
      threads are created only once (see _3do_Init / _madam_Init), so this no
      longer spawns duplicates; _arm_Destroy/_arm_Init free and re-allocate the
      3DO memory in balance, and both worker threads are idle here (no frame is
      running). The disc stays open, so the BIOS reboots and reads the same game.
      NVRAM must be preserved by hand: EXT_READ/WRITE_NVRAM are no-ops here (the
      frontend owns NVRAM via the SRAM API), so _arm_Init would otherwise hand
      back a zeroed buffer and wipe the user's saves on every reset. */
   unsigned char nvbak[THREEDO_NVRAM_SIZE];
   int have_nv = (g_nvram_ptr != NULL);
   if (have_nv) memcpy(nvbak, g_nvram_ptr, THREEDO_NVRAM_SIZE);

   _freedo_Interface(FDP_DESTROY, NULL);
   _freedo_Interface(FDP_INIT, (void*)ext_interface);
   _freedo_Interface(FDP_SET_ARMCLOCK, (void*)12500000);
   read_swi_hle_option();   /* aplica la opcion SWI HLE (por defecto OFF) */
   g_nvram_ptr = _freedo_Interface(FDP_GETP_NVRAM, NULL);

   if (have_nv && g_nvram_ptr)
      memcpy(g_nvram_ptr, nvbak, THREEDO_NVRAM_SIZE);   /* restore NVRAM across the reboot */
   g_nvram_checked = false;   /* re-validate/format after the reboot */
   audio_ring_reset();
}

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
