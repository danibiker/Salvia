/*
 * cdriso_async.c
 *
 * Implementation of the cdriso async prefetch layer.  See header for
 * the architecture diagram and rationale.
 *
 * IMPORTANT: this file is NOT compiled standalone.  It's `#include`d
 * near the bottom of cdriso.c so the worker can reach the file-static
 * state (cdHandle, ti[], ti_handle[], ti_fileLBAoffset[], numtracks,
 * isMode1ISO, subChanMixed, subChanRaw, subHandle, chdFile, readchdsector,
 * DecodeRawSubData, MODE1_DATA_SIZE, cdbuffer, subbuffer, ...).  Pulling
 * those out into a shared header would touch cdriso.c more than
 * necessary and risks regression -- keeping them as TU-locals and
 * including this file gives us the same encapsulation with less
 * surface area.
 *
 * Build note: do NOT add this file to pcsxr.vcxproj's compile list.
 * It compiles as part of cdriso.c via the #include at the bottom of
 * that file.  If added as a standalone TU, you'll get "undeclared
 * identifier" errors for cdriso.c's statics AND duplicate-symbol link
 * errors for the cdra_* public API.
 *
 * Worker thread placement: Xbox 360 XCPU exposes 6 hardware threads:
 *   core 0 -> HW 0, 1 (Xbox kernel reserves part of HW 0)
 *   core 1 -> HW 2, 3
 *   core 2 -> HW 4, 5
 *
 * Pcsx-r 360's main emulator + dynarec runs on core 0/1.  We pin the
 * prefetch worker to HW thread 4 (core 2 primary) via
 * XSetThreadProcessor() so it doesn't compete for execution units
 * with the emulator hot path.  If XSetThreadProcessor fails (e.g.,
 * dev kit running with reduced cores), the worker falls back to
 * whatever core the scheduler picks -- still gets parallelism, just
 * may cause occasional jitter on the main thread.
 */

#ifndef CDRISO_ASYNC_C_INCLUDED
#define CDRISO_ASYNC_C_INCLUDED

#include "cdriso_async.h"

/* === Configuration === */

/* === Ring slots (mitigation B) ===
 *
 * Bumped from 8 to 32 slots = ~78 KB ring.  Rationale:
 *
 * At 2x CD speed, a sector is ~6.7 ms of emulated time.  With 8 slots
 * we cover 53 ms of read-ahead.  If a stall hits during that window,
 * the game stalls.  With 32 slots we cover 215 ms of read-ahead.
 *
 * Disk stalls observed in the field were 30-330 ms (occasionally
 * higher).  A 215 ms ring absorbs most stalls cleanly; the game keeps
 * consuming hits from the ring while the worker's slow read blocks.
 *
 * Memory cost: 32 * (2352 + 96 + ~8) = ~78 KB.  Negligible.
 *
 * Increasing further (64+) gives diminishing returns: the worker's
 * fread throughput on USB is finite, so once the ring is fuller than
 * the worker can refill in one round trip, extra slots stay idle. */
#define CDRA_RING_SLOTS 32

/* Xbox 360 hardware thread index for the worker.  HW thread 4 = core 2
 * primary, away from the main emulator threads. */
#define CDRA_WORKER_HW_THREAD 4

/* === LRU block cache (mitigation C) ===
 *
 * Second-level cache that sits between the small ring buffer (used for
 * "next N sectors" prefetch) and the OS fread.  Holds large contiguous
 * blocks of the .bin file in RAM so that random-access reads (game
 * seeks back to previously-touched regions) become memcpy operations
 * instead of OS round-trips.
 *
 * Sizing:
 *   CDRA_LRU_BLOCK_SIZE   = 32 KB per block (about 14 PSX sectors)
 *   CDRA_LRU_NUM_BLOCKS   = 512 blocks
 *   Total RAM             = 16 MB
 *
 * Earlier iteration used 256 KB blocks but field testing showed that
 * each cache miss then did a 256 KB fread.  On Xbox 360 USB, larger
 * single reads have a higher chance of hitting the kernel I/O jitter
 * (firmware background work, USB stack queue contention, etc.) than
 * small reads.  A 256 KB miss could take 300+ ms while many small
 * sequential 32 KB reads each take 10-30 ms.  Net result: slow_reads
 * count actually went UP with the bigger blocks.
 *
 * 32 KB strikes a balance: still 14x larger than a single sector read
 * (2352 bytes), so we amortize OS overhead across multiple sectors,
 * but each miss transfers an order of magnitude less data than the
 * 256 KB variant.  More cache slots (512 vs 64) means hit rate stays
 * comparable for sequential access -- only purely random scatter
 * loses slightly. */
#define CDRA_LRU_BLOCK_SIZE   (32 * 1024)
#define CDRA_LRU_NUM_BLOCKS   512
#define CDRA_LRU_TOTAL_BYTES  ((size_t)CDRA_LRU_BLOCK_SIZE * CDRA_LRU_NUM_BLOCKS)

/* === State === */

typedef struct {
  volatile int  lba;                            /* sector LBA, or -1 if empty */
  int           has_sub;                        /* 1 if `sub` field was populated */
  unsigned char cd[CD_FRAMESIZE_RAW];           /* 2352 bytes */
  unsigned char sub[SUB_FRAMESIZE];             /* 96 bytes */
} cdra_slot_t;

static cdra_slot_t       cdra_ring[CDRA_RING_SLOTS];
static CRITICAL_SECTION  cdra_ring_cs;     /* protects ring + slot fields */
static CRITICAL_SECTION  cdra_io_cs;       /* serializes file I/O between main+worker */
static HANDLE            cdra_wakeup_evt   = NULL;
static HANDLE            cdra_worker_h     = NULL;
static volatile LONG     cdra_worker_exit  = 0;
static volatile LONG     cdra_anchor       = -1;  /* prefetch starts at this LBA */
static volatile LONG     cdra_generation   = 0;   /* bumped on each main-thread read */
static int               cdra_enabled      = 1;
static int               cdra_initialized  = 0;

/* Visible to host log via pcsxr_log.  Used by cdra_init/cdra_worker_proc
 * to confirm successful startup; counts let main thread spot whether the
 * worker is actually running and making progress. */
extern void pcsxr_log(int level, const char *format, ...);
static volatile LONG     cdra_worker_entered_count = 0;
static volatile LONG     cdra_worker_wake_count    = 0;
static volatile LONG     cdra_read_call_count      = 0;
static volatile LONG     cdra_hit_count            = 0;
static volatile LONG     cdra_miss_count           = 0;

/* === I/O timing instrumentation ===
 *
 * Track wall-clock time spent inside synchronous backend reads so we
 * can correlate gameplay stalls with disk latency events.  All numbers
 * are wall-clock ms (not PSX cycles).  Updated from BOTH main and
 * worker threads via interlocked ops; readers see eventually-consistent
 * values which is fine for diagnostics. */
/* Lowered from 30 ms to 15 ms.  At 15 ms a read is already eating ~1
 * PSX video frame's worth of wall clock.  We're trying to correlate
 * disk stalls with audio crackling -- the crackles likely come from
 * many sub-30ms but >5ms stalls compounding SPU sample-rate jitter.
 * 15 ms catches those without log spam from healthy 3-10 ms reads. */
#define CDRA_SLOW_READ_THRESHOLD_MS  15
#define CDRA_STALL_BURST_THRESHOLD   8   /* consecutive misses to log */

static LARGE_INTEGER     cdra_qpc_freq;          /* set in cdra_init */
static volatile LONG     cdra_slow_read_count     = 0;
static volatile LONG     cdra_slow_read_total_ms  = 0;
static volatile LONG     cdra_max_single_read_ms  = 0;
static volatile LONG     cdra_miss_streak_current = 0;
static volatile LONG     cdra_miss_streak_max     = 0;
static volatile LONG     cdra_worker_slow_iter_count = 0;  /* worker iter >100ms */
static volatile LONG     cdra_worker_max_iter_ms     = 0;

/* === LRU block cache state === */
typedef struct {
  FILE         *handle;        /* owning file handle, NULL = empty slot */
  long          base_offset;   /* file offset of block start (aligned) */
  unsigned int  lru_clock;     /* updated on each hit */
  size_t        valid_bytes;   /* how many bytes in `data` are valid */
  unsigned char *data;         /* pointer into cdra_lru_pool */
} cdra_lru_meta_t;

static cdra_lru_meta_t   cdra_lru_meta[CDRA_LRU_NUM_BLOCKS];
static unsigned char    *cdra_lru_pool       = NULL;
static unsigned int      cdra_lru_clock      = 0;
static CRITICAL_SECTION  cdra_lru_cs;
static int               cdra_lru_initialized = 0;
static volatile LONG     cdra_lru_hits        = 0;
static volatile LONG     cdra_lru_misses      = 0;

/* Convert QPC delta to ms.  freq is ticks/sec, delta is in ticks. */
static unsigned int cdra_qpc_delta_ms(LARGE_INTEGER t0, LARGE_INTEGER t1)
{
  if (cdra_qpc_freq.QuadPart == 0) return 0;
  return (unsigned int)(((t1.QuadPart - t0.QuadPart) * 1000ULL)
                        / cdra_qpc_freq.QuadPart);
}

/* === Thread CPU time helper ===
 *
 * GetThreadTimes returns kernel+user time consumed by the thread.  By
 * sampling before/after a blocking call we can distinguish:
 *
 *   wall - cpu == 0          : thread was running all the time
 *                              (busy-wait or CPU-heavy work)
 *   wall - cpu == wall       : thread was blocked / waiting / preempted
 *                              the whole time (I/O wait, mutex, etc.)
 *   wall - cpu == some_value : partial wait
 *
 * The FILETIME unit is 100-ns intervals; we convert to ms.  On Xbox 360
 * the kernel tracks thread times at scheduler granularity (~1 ms). */
static unsigned int cdra_filetime_delta_ms(FILETIME a, FILETIME b)
{
  ULARGE_INTEGER ua, ub;
  ua.LowPart  = a.dwLowDateTime;
  ua.HighPart = a.dwHighDateTime;
  ub.LowPart  = b.dwLowDateTime;
  ub.HighPart = b.dwHighDateTime;
  if (ub.QuadPart < ua.QuadPart) return 0;
  /* 100 ns units -> ms = / 10000 */
  return (unsigned int)((ub.QuadPart - ua.QuadPart) / 10000ULL);
}

/* Sample current thread's CPU time (kernel + user).  On Xbox 360 the
 * GetThreadTimes API is available via the XDK runtime. */
static void cdra_sample_thread_cpu(FILETIME *user, FILETIME *kernel)
{
  FILETIME ct, et;  /* creation and exit time, unused */
  if (!GetThreadTimes(GetCurrentThread(), &ct, &et, kernel, user)) {
    kernel->dwLowDateTime  = 0;
    kernel->dwHighDateTime = 0;
    user->dwLowDateTime    = 0;
    user->dwHighDateTime   = 0;
  }
}

/* === Forward decls for things defined in cdriso.c ===
 * These are all TU-locals at the point this file is included, so the
 * decls here are just documentation.  Real definitions live above the
 * include site in cdriso.c. */
/* extern static unsigned char cdbuffer[CD_FRAMESIZE_RAW]; */
/* extern static unsigned char subbuffer[SUB_FRAMESIZE]; */
/* extern static FILE *cdHandle; */
/* extern static FILE *subHandle; */
/* extern static FILE *ti_handle[MAXTRACKS]; */
/* extern static unsigned int ti_fileLBAoffset[MAXTRACKS]; */
/* extern static boolean subChanMixed, subChanRaw, isMode1ISO; */
/* extern static int numtracks; */
/* extern static struct trackinfo ti[MAXTRACKS]; */
/* extern static chd_file *chdFile;  (USE_CHD only) */
/* static int readchdsector(int lba, unsigned char *buf);  (USE_CHD only) */
/* static void DecodeRawSubData(void); */

/* === LRU block cache implementation ===
 *
 * cdra_lru_fread() is a drop-in replacement for `fread` that consults
 * a 16 MB LRU cache of 256 KB blocks before touching the OS.  Same
 * semantics: reads `count` bytes from current file position into `buf`,
 * advances the file position, returns bytes actually read.
 *
 * The cache covers spatial locality past the small prefetch ring: if
 * the game seeks back to a region read in the last few minutes, it's
 * still in RAM.
 *
 * Caller must hold cdra_io_cs (because we may issue an actual fread on
 * miss, which shares the underlying file handle). */
static void cdra_lru_init(void)
{
  int i;
  if (cdra_lru_initialized) return;

  InitializeCriticalSection(&cdra_lru_cs);

  cdra_lru_pool = (unsigned char *)malloc(CDRA_LRU_TOTAL_BYTES);
  if (!cdra_lru_pool) {
    pcsxr_log(2 /* WARN */,
              "[cdra-lru] malloc(%u) failed; LRU cache disabled\n",
              (unsigned)CDRA_LRU_TOTAL_BYTES);
    DeleteCriticalSection(&cdra_lru_cs);
    return;
  }

  for (i = 0; i < CDRA_LRU_NUM_BLOCKS; i++) {
    cdra_lru_meta[i].handle       = NULL;
    cdra_lru_meta[i].base_offset  = -1;
    cdra_lru_meta[i].lru_clock    = 0;
    cdra_lru_meta[i].valid_bytes  = 0;
    cdra_lru_meta[i].data         = cdra_lru_pool + (size_t)i * CDRA_LRU_BLOCK_SIZE;
  }
  cdra_lru_clock = 0;
  cdra_lru_initialized = 1;

  pcsxr_log(0 /* INFO */,
            "[cdra-lru] cache ready: %d blocks x %d KB = %d MB\n",
            CDRA_LRU_NUM_BLOCKS,
            CDRA_LRU_BLOCK_SIZE / 1024,
            (int)(CDRA_LRU_TOTAL_BYTES / (1024 * 1024)));
}

static void cdra_lru_shutdown(void)
{
  if (!cdra_lru_initialized) return;
  if (cdra_lru_pool) {
    free(cdra_lru_pool);
    cdra_lru_pool = NULL;
  }
  DeleteCriticalSection(&cdra_lru_cs);
  cdra_lru_initialized = 0;
}

/* Drop all cached blocks.  Called from cdra_invalidate() on disc swap. */
static void cdra_lru_invalidate_all(void)
{
  int i;
  if (!cdra_lru_initialized) return;
  EnterCriticalSection(&cdra_lru_cs);
  for (i = 0; i < CDRA_LRU_NUM_BLOCKS; i++) {
    cdra_lru_meta[i].handle      = NULL;
    cdra_lru_meta[i].base_offset = -1;
    cdra_lru_meta[i].valid_bytes = 0;
  }
  cdra_lru_clock = 0;
  LeaveCriticalSection(&cdra_lru_cs);
}

/* fread() wrapper.  `h` is the file handle (passed explicitly because
 * the cache keys on it -- we want separate cache slots for cdHandle vs
 * subHandle vs ti_handle[k]).  `file_offset` is the absolute offset in
 * the file where the read begins; caller must compute it (we don't
 * call ftell to avoid an extra CRT call).  Returns bytes read; if less
 * than `count`, the file has fewer bytes left from that offset (EOF). */
static size_t cdra_lru_fread(void *buf,
                             size_t count,
                             FILE *h,
                             long file_offset)
{
  long block_base;
  long off_in_block;
  size_t copy_now;
  size_t copied = 0;
  int    i, slot;

  if (!cdra_lru_initialized || h == NULL || count == 0) {
    /* Fallback: plain fread on the underlying file.  Caller already
     * holds io_cs so this is safe. */
    if (h && count) {
      fseek(h, file_offset, SEEK_SET);
      return fread(buf, 1, count, h);
    }
    return 0;
  }

  /* Reads may span block boundaries; handle each block separately. */
  while (copied < count) {
    block_base   = ((file_offset + (long)copied) / CDRA_LRU_BLOCK_SIZE) * CDRA_LRU_BLOCK_SIZE;
    off_in_block = (file_offset + (long)copied) - block_base;
    copy_now     = CDRA_LRU_BLOCK_SIZE - (size_t)off_in_block;
    if (copy_now > count - copied) copy_now = count - copied;

    /* Lookup in cache. */
    EnterCriticalSection(&cdra_lru_cs);
    slot = -1;
    for (i = 0; i < CDRA_LRU_NUM_BLOCKS; i++) {
      if (cdra_lru_meta[i].handle == h &&
          cdra_lru_meta[i].base_offset == block_base) {
        slot = i;
        cdra_lru_meta[i].lru_clock = ++cdra_lru_clock;
        break;
      }
    }

    if (slot >= 0) {
      /* HIT -- copy from cache.  Bound by valid_bytes in case the
       * block was short (EOF). */
      size_t available = (cdra_lru_meta[slot].valid_bytes > (size_t)off_in_block)
                       ? cdra_lru_meta[slot].valid_bytes - (size_t)off_in_block
                       : 0;
      if (available < copy_now) copy_now = available;
      memcpy((unsigned char *)buf + copied,
             cdra_lru_meta[slot].data + off_in_block,
             copy_now);
      LeaveCriticalSection(&cdra_lru_cs);
      InterlockedIncrement(&cdra_lru_hits);
      copied += copy_now;
      if (copy_now == 0) break;  /* EOF in this block */
      continue;
    }

    /* MISS -- find LRU slot to evict.  Empty slots (handle==NULL) win
     * automatically because they have lru_clock=0 (smallest). */
    {
      unsigned int lru_min = cdra_lru_meta[0].lru_clock;
      int evict = 0;
      for (i = 1; i < CDRA_LRU_NUM_BLOCKS; i++) {
        if (cdra_lru_meta[i].lru_clock < lru_min) {
          lru_min = cdra_lru_meta[i].lru_clock;
          evict   = i;
        }
      }
      slot = evict;
      /* Mark invalid while we fill it -- if another thread looks up the
       * same block, they'll miss and queue another read, which is wasteful
       * but correct.  Worth refining only if profiling shows contention. */
      cdra_lru_meta[slot].handle      = NULL;
      cdra_lru_meta[slot].base_offset = -1;
    }
    LeaveCriticalSection(&cdra_lru_cs);
    InterlockedIncrement(&cdra_lru_misses);

    /* Issue the actual fread.  Reads a full block into the slot's data
     * buffer.  May return less than CDRA_LRU_BLOCK_SIZE near EOF. */
    {
      size_t got;
      fseek(h, block_base, SEEK_SET);
      got = fread(cdra_lru_meta[slot].data, 1, CDRA_LRU_BLOCK_SIZE, h);

      EnterCriticalSection(&cdra_lru_cs);
      cdra_lru_meta[slot].handle      = h;
      cdra_lru_meta[slot].base_offset = block_base;
      cdra_lru_meta[slot].valid_bytes = got;
      cdra_lru_meta[slot].lru_clock   = ++cdra_lru_clock;

      /* Copy the slice that the caller wants. */
      {
        size_t available = (got > (size_t)off_in_block)
                         ? got - (size_t)off_in_block
                         : 0;
        if (available < copy_now) copy_now = available;
        memcpy((unsigned char *)buf + copied,
               cdra_lru_meta[slot].data + off_in_block,
               copy_now);
      }
      LeaveCriticalSection(&cdra_lru_cs);

      copied += copy_now;
      if (got < CDRA_LRU_BLOCK_SIZE || copy_now == 0) break;  /* short read -- EOF */
    }
  }
  return copied;
}

/* === cdra_read_sync_internal ===
 *
 * Reads sector `lba` from the underlying disc into caller-provided
 * buffers.  Same logic as the old ISOreadTrack body but parameterised
 * so it can target either the global cdbuffer/subbuffer (main thread
 * cache miss) or a ring slot's buffers (worker thread prefetch).
 *
 * Caller MUST hold cdra_io_cs.  We don't take it here because the main
 * thread may want to hold it across the read AND the subsequent
 * `ring slot consumed` bookkeeping under cdra_ring_cs (lock order:
 * io_cs first, then ring_cs).
 *
 * Returns 0 on success, -1 on error.  Sets *out_has_sub to 1 iff the
 * sub buffer was populated. */
static int cdra_read_sync_internal(int lba,
                                   unsigned char *out_cd,
                                   unsigned char *out_sub,
                                   int *out_has_sub)
{
  FILE         *h;
  unsigned int  fileLBAoff;
  long          offset_lba;
  int           k;
  LARGE_INTEGER it0, it1;
  unsigned int  inner_ms;
  int           rc_inner = 0;
  int           path_id = 0;   /* 0=chd, 1=bin-mixed, 2=bin-mode1iso,
                                  3=bin-2352, 4=bin+subhandle */

  *out_has_sub = 0;
  QueryPerformanceCounter(&it0);

#ifdef USE_CHD
  if (chdFile != NULL) {
    /* CHD reader; goes through readchdsector which uses the HunkCache.
     * HunkCache access is serialized here via cdra_io_cs being held by
     * the caller.  No separate HunkCache mutex needed. */
    path_id = 0;
    rc_inner = (readchdsector(lba, out_cd) != 0) ? -1 : 0;
    goto sync_done;
  }
#endif

  if (cdHandle == NULL)
    return -1;

  /* Pick the file handle that owns this LBA (multi-FILE cue support). */
  h          = cdHandle;
  fileLBAoff = 0;

  if (numtracks > 0) {
    for (k = numtracks; k >= 1; k--) {
      int track_start_lba = (int)msf2sec(ti[k].start) - 2 * 75;
      if (track_start_lba <= lba) {
        if (ti_handle[k] != NULL) h = ti_handle[k];
        fileLBAoff = ti_fileLBAoffset[k];
        break;
      }
    }
  }

  offset_lba = (long)lba - (long)fileLBAoff;
  if (offset_lba < 0) offset_lba = 0;

  if (subChanMixed) {
    LARGE_INTEGER tr0, tr1, tr2;
    unsigned int  read_ms, read2_ms;
    long          file_off = offset_lba * (CD_FRAMESIZE_RAW + SUB_FRAMESIZE);
    path_id = 1;
    /* mitigation C: cd + sub are contiguous; cache spans both. */
    QueryPerformanceCounter(&tr0);
    cdra_lru_fread(out_cd,  CD_FRAMESIZE_RAW, h, file_off);
    QueryPerformanceCounter(&tr1);
    cdra_lru_fread(out_sub, SUB_FRAMESIZE,    h, file_off + CD_FRAMESIZE_RAW);
    QueryPerformanceCounter(&tr2);
    read_ms  = cdra_qpc_delta_ms(tr0, tr1);
    read2_ms = cdra_qpc_delta_ms(tr1, tr2);
    if (read_ms + read2_ms >= CDRA_SLOW_READ_THRESHOLD_MS) {
      pcsxr_log(2 /* WARN */,
                "[cdra]     fine: lru_fread_cd=%u ms lru_fread_sub=%u ms "
                "(subChanMixed, lba=%d)\n",
                read_ms, read2_ms, lba);
    }
    if (subChanRaw) {
      /* DecodeRawSubData reads from the global subbuffer.  If we just
       * wrote to a caller-provided out_sub that's NOT the global, we
       * need to either (a) decode in place using out_sub, or (b) only
       * support subChanRaw decoding when reading into the global.
       *
       * For now: copy into global subbuffer, decode, copy back.  This
       * happens rarely (only some .toc/.cue setups expose raw subq)
       * so the extra memcpy is negligible.  The caller must hold io_cs
       * which prevents concurrent access to subbuffer.  Note: the
       * main thread's "cdbuffer/subbuffer" globals are the target for
       * cache-miss reads, in which case out_sub == subbuffer and the
       * copies are no-ops. */
      if (out_sub != subbuffer) memcpy(subbuffer, out_sub, SUB_FRAMESIZE);
      DecodeRawSubData();
      if (out_sub != subbuffer) memcpy(out_sub, subbuffer, SUB_FRAMESIZE);
    }
    *out_has_sub = 1;
  }
  else {
    if (isMode1ISO) {
      int total = lba + 2 * 75;
      LARGE_INTEGER tr0, tr1;
      unsigned int  read_ms;
      path_id = 2;
      QueryPerformanceCounter(&tr0);
      cdra_lru_fread(out_cd + 12, MODE1_DATA_SIZE, h,
                     offset_lba * MODE1_DATA_SIZE);  /* mitigation C */
      QueryPerformanceCounter(&tr1);
      read_ms = cdra_qpc_delta_ms(tr0, tr1);
      if (read_ms >= CDRA_SLOW_READ_THRESHOLD_MS) {
        pcsxr_log(2 /* WARN */,
                  "[cdra]     fine: lru_fread=%u ms (Mode1ISO, lba=%d)\n",
                  read_ms, lba);
      }
      memset(out_cd, 0, 12);
      /* Fake Mode 2 header: MSF in BCD + mode byte = 1 */
      out_cd[0] = itob((unsigned char)(total / 75 / 60));
      out_cd[1] = itob((unsigned char)((total / 75) % 60));
      out_cd[2] = itob((unsigned char)(total % 75));
      out_cd[3] = 1;
    }
    else {
      /* === BIN/CUE raw 2352 path -- the most common case.
       * Goes through the LRU block cache (mitigation C) for
       * hit-on-revisit acceleration.  The cache also smooths over
       * occasional kernel-level read stalls by amortizing them across
       * a full 256 KB block. */
      LARGE_INTEGER tr0, tr1;
      unsigned int  read_ms;
      long          file_off = offset_lba * CD_FRAMESIZE_RAW;
      path_id = 3;
      QueryPerformanceCounter(&tr0);
      cdra_lru_fread(out_cd, CD_FRAMESIZE_RAW, h, file_off);
      QueryPerformanceCounter(&tr1);
      read_ms = cdra_qpc_delta_ms(tr0, tr1);
      if (read_ms >= CDRA_SLOW_READ_THRESHOLD_MS) {
        pcsxr_log(2 /* WARN */,
                  "[cdra]     fine: lru_fread=%u ms "
                  "(2352-raw, lba=%d, fpos=%ld bytes / %ld MB)\n",
                  read_ms, lba, file_off, file_off / (1024 * 1024));
      }
    }

    if (subHandle != NULL) {
      LARGE_INTEGER tr0, tr1;
      unsigned int  read_ms;
      path_id = 4;
      QueryPerformanceCounter(&tr0);
      cdra_lru_fread(out_sub, SUB_FRAMESIZE, subHandle,
                     (long)lba * SUB_FRAMESIZE);  /* mitigation C */
      QueryPerformanceCounter(&tr1);
      read_ms = cdra_qpc_delta_ms(tr0, tr1);
      if (read_ms >= CDRA_SLOW_READ_THRESHOLD_MS) {
        pcsxr_log(2 /* WARN */,
                  "[cdra]     fine: subHandle lru_fread=%u ms "
                  "(lba=%d)\n",
                  read_ms, lba);
      }
      if (subChanRaw) {
        if (out_sub != subbuffer) memcpy(subbuffer, out_sub, SUB_FRAMESIZE);
        DecodeRawSubData();
        if (out_sub != subbuffer) memcpy(out_sub, subbuffer, SUB_FRAMESIZE);
      }
      *out_has_sub = 1;
    }
  }

sync_done:
  /* === Inner I/O timing ===
   * Measures the actual file/CHD operation, no locks.  If this is
   * slow, the disk subsystem is the bottleneck.  If the OUTER timer
   * (in cdra_read miss path or worker_proc) is much higher than this
   * inner one, the difference is lock contention.  Logs every slow
   * inner I/O with the path identifier so we can spot which code
   * branch (CHD / BIN-mixed / Mode1ISO / 2352-raw / +subhandle) is
   * causing the latency.
   *
   * path_id:
   *   0 = CHD readchdsector (with HunkCache)
   *   1 = BIN/CUE with subChanMixed (cd+sub in same file)
   *   2 = BIN/CUE Mode1ISO (2048-byte sectors + fake header)
   *   3 = BIN/CUE raw 2352
   *   4 = BIN/CUE 2352 + separate .sub file (fseek/fread twice) */
  QueryPerformanceCounter(&it1);
  inner_ms = cdra_qpc_delta_ms(it0, it1);
  if (inner_ms >= CDRA_SLOW_READ_THRESHOLD_MS) {
    pcsxr_log(2 /* WARN */,
              "[cdra]   inner I/O slow: lba=%d %u ms path=%d "
              "(disk-level latency, no lock wait)\n",
              lba, inner_ms, path_id);
  }
  return rc_inner;
}

/* === Worker thread ===
 *
 * Wakes on event, snapshots (generation, anchor), then iterates 0..N-1
 * issuing reads for anchor+i.  Each iteration:
 *   1. Recheck generation -- if main started a new chain, bail.
 *   2. Check ring for existing slot with this LBA -- skip if found.
 *   3. Read into tmp buffer (under io_cs).
 *   4. Recheck generation -- if main changed mind during the read, drop.
 *   5. Store into ring (under ring_cs): empty slot if any, else evict
 *      the slot whose LBA is furthest from `target` (the "least useful"
 *      cached sector relative to the current read frontier).
 */
static DWORD WINAPI cdra_worker_proc(LPVOID arg)
{
  unsigned char tmp_cd[CD_FRAMESIZE_RAW];
  unsigned char tmp_sub[SUB_FRAMESIZE];

  (void)arg;

  /* Bump the entered counter so main thread can confirm we ran. */
  InterlockedIncrement(&cdra_worker_entered_count);

#if defined(_XBOX)
  /* Pin worker to core 2 (HW thread 4).  Return value ignored: on
   * failure the thread runs on whatever core the scheduler picks --
   * still gets parallelism. */
  XSetThreadProcessor(GetCurrentThread(), CDRA_WORKER_HW_THREAD);
#endif

  while (!cdra_worker_exit) {
    LONG local_gen;
    LONG local_anchor;
    int  i;

    /* Block until main signals a new prefetch chain (or shutdown). */
    WaitForSingleObject(cdra_wakeup_evt, INFINITE);
    if (cdra_worker_exit) break;
    InterlockedIncrement(&cdra_worker_wake_count);

    /* Snapshot current chain parameters.  Race-tolerant: if main
     * changes them while we're working, we'll detect via generation
     * mismatch and start over. */
    local_gen    = cdra_generation;
    local_anchor = cdra_anchor;
    if (local_anchor < 0) continue;

    for (i = 0; i < CDRA_RING_SLOTS; i++) {
      int target_lba;
      int has_sub = 0;
      int rc;
      int j;
      int found;
      int slot;

      if (cdra_worker_exit) goto done;
      if (cdra_generation != local_gen) break;     /* main started new chain */
      if (!cdra_enabled)               break;      /* runtime disabled */

      target_lba = (int)local_anchor + i;

      /* Skip if already cached. */
      EnterCriticalSection(&cdra_ring_cs);
      found = 0;
      for (j = 0; j < CDRA_RING_SLOTS; j++) {
        if (cdra_ring[j].lba == target_lba) { found = 1; break; }
      }
      LeaveCriticalSection(&cdra_ring_cs);
      if (found) continue;

      /* Do the actual read.  CHD hunk decompression / fread happens
       * here.  Time it to spot worker preemption (if our wall-clock
       * jumped despite no actual I/O work) vs slow disk (genuine
       * decompress/fread latency). */
      {
        LARGE_INTEGER wt0, wt1;
        unsigned int  wms;

        QueryPerformanceCounter(&wt0);
        EnterCriticalSection(&cdra_io_cs);
        rc = cdra_read_sync_internal(target_lba, tmp_cd, tmp_sub, &has_sub);
        LeaveCriticalSection(&cdra_io_cs);
        QueryPerformanceCounter(&wt1);

        wms = cdra_qpc_delta_ms(wt0, wt1);
        if (wms > (unsigned int)cdra_worker_max_iter_ms)
          InterlockedExchange(&cdra_worker_max_iter_ms, (LONG)wms);
        if (wms >= 100) {
          /* 100 ms in a worker iteration is suspicious -- normal fread
           * on a BIN/CUE is <5 ms, CHD decompression 1-3 ms.  Anything
           * over 100 ms suggests Xbox kernel preempting the core. */
          InterlockedIncrement(&cdra_worker_slow_iter_count);
          pcsxr_log(2 /* WARN */,
                    "[cdra] WORKER slow iter: lba=%d %u ms (rc=%d) -- "
                    "preemption or slow disk?\n",
                    target_lba, wms, rc);
        }
      }
      if (rc != 0) continue;

      /* Drop the result if main has moved on (different generation). */
      if (cdra_generation != local_gen) break;

      /* Find a slot to put this sector in.  Preference order:
       *   1. Any slot with this exact LBA (idempotent overwrite).
       *   2. Any empty slot.
       *   3. The slot whose LBA is furthest from `target_lba`
       *      (i.e., least useful for the current read frontier). */
      EnterCriticalSection(&cdra_ring_cs);
      slot = -1;
      for (j = 0; j < CDRA_RING_SLOTS; j++) {
        if (cdra_ring[j].lba == target_lba) { slot = j; break; }
      }
      if (slot < 0) {
        for (j = 0; j < CDRA_RING_SLOTS; j++) {
          if (cdra_ring[j].lba == -1) { slot = j; break; }
        }
      }
      if (slot < 0) {
        int worst_dist = -1;
        for (j = 0; j < CDRA_RING_SLOTS; j++) {
          int dist = cdra_ring[j].lba - target_lba;
          if (dist < 0) dist = -dist;
          if (dist > worst_dist) { worst_dist = dist; slot = j; }
        }
      }
      if (slot >= 0) {
        cdra_ring[slot].lba     = target_lba;
        cdra_ring[slot].has_sub = has_sub;
        memcpy(cdra_ring[slot].cd,  tmp_cd,  CD_FRAMESIZE_RAW);
        if (has_sub) memcpy(cdra_ring[slot].sub, tmp_sub, SUB_FRAMESIZE);
      }
      LeaveCriticalSection(&cdra_ring_cs);
    }
  }
done:
  return 0;
}

/* === Public API === */

void cdra_init(void)
{
  int i;
  DWORD thread_id;

  if (cdra_initialized) {
    pcsxr_log(0 /* INFO */, "[cdra] init: already initialized, skipping\n");
    return;
  }

  pcsxr_log(0 /* INFO */, "[cdra] init: starting\n");

  /* Capture the high-resolution counter frequency once so we can
   * convert ticks->ms for slow-read diagnostics.  On Xbox 360 the
   * QPC frequency is constant for the lifetime of the system. */
  QueryPerformanceFrequency(&cdra_qpc_freq);

  InitializeCriticalSection(&cdra_ring_cs);
  InitializeCriticalSection(&cdra_io_cs);

  for (i = 0; i < CDRA_RING_SLOTS; i++) {
    cdra_ring[i].lba     = -1;
    cdra_ring[i].has_sub = 0;
  }

  cdra_worker_exit = 0;
  cdra_anchor      = -1;
  cdra_generation  = 0;

  /* Auto-reset event: SetEvent wakes one waiter, then resets.  If main
   * signals multiple times before worker wakes, they collapse into one
   * wakeup -- but that's fine because the worker reads `cdra_anchor`
   * (the latest value) each iteration. */
  cdra_wakeup_evt = CreateEvent(NULL,
                                FALSE,  /* bManualReset = FALSE */
                                FALSE,  /* bInitialState = unsignaled */
                                NULL);
  if (cdra_wakeup_evt == NULL) {
    pcsxr_log(2 /* WARN */, "[cdra] init: CreateEvent FAILED (gle=%lu)\n",
              (unsigned long)GetLastError());
    DeleteCriticalSection(&cdra_ring_cs);
    DeleteCriticalSection(&cdra_io_cs);
    return;
  }

  cdra_worker_h = CreateThread(NULL,
                               64 * 1024,         /* small stack: worker is shallow */
                               cdra_worker_proc,
                               NULL,
                               0,                 /* run immediately */
                               &thread_id);
  if (cdra_worker_h == NULL) {
    pcsxr_log(2 /* WARN */, "[cdra] init: CreateThread FAILED (gle=%lu)\n",
              (unsigned long)GetLastError());
    CloseHandle(cdra_wakeup_evt);
    cdra_wakeup_evt = NULL;
    DeleteCriticalSection(&cdra_ring_cs);
    DeleteCriticalSection(&cdra_io_cs);
    return;
  }

  cdra_initialized = 1;
  pcsxr_log(0 /* INFO */, "[cdra] init: ok, worker thread id=%lu\n",
            (unsigned long)thread_id);

  /* Stand up the LRU block cache too (mitigation C).  Lives across
   * disc swaps; cdra_invalidate() drops contents but keeps the
   * allocation. */
  cdra_lru_init();
}

void cdra_shutdown(void)
{
  if (!cdra_initialized) return;

  /* Signal worker to exit, wake it up so it sees the flag, join. */
  cdra_worker_exit = 1;
  SetEvent(cdra_wakeup_evt);
  WaitForSingleObject(cdra_worker_h, INFINITE);
  CloseHandle(cdra_worker_h);
  cdra_worker_h = NULL;

  CloseHandle(cdra_wakeup_evt);
  cdra_wakeup_evt = NULL;

  DeleteCriticalSection(&cdra_ring_cs);
  DeleteCriticalSection(&cdra_io_cs);

  /* Tear down LRU block cache too (mitigation C).  Releases the 16 MB
   * pool back to the heap. */
  cdra_lru_shutdown();

  cdra_initialized = 0;
}

void cdra_set_enabled(int enabled)
{
  int new_state = enabled ? 1 : 0;
  if (new_state == cdra_enabled) return;  /* no-op, avoid log spam */
  cdra_enabled = new_state;
  if (!cdra_enabled) cdra_invalidate();
  pcsxr_log(0 /* INFO */, "[cdra] set_enabled -> %s (initialized=%d)\n",
            cdra_enabled ? "ON" : "OFF", cdra_initialized);
}

int cdra_is_enabled(void)
{
  return cdra_enabled;
}

void cdra_invalidate(void)
{
  int i;
  if (!cdra_initialized) return;
  EnterCriticalSection(&cdra_ring_cs);
  for (i = 0; i < CDRA_RING_SLOTS; i++)
    cdra_ring[i].lba = -1;
  LeaveCriticalSection(&cdra_ring_cs);
  /* Bump generation so any in-flight worker iteration aborts. */
  InterlockedIncrement(&cdra_generation);

  /* Drop LRU cache contents too -- on disc swap the old blocks point
   * at the previous disc image's data (via stale FILE*). */
  cdra_lru_invalidate_all();
}

int cdra_read(int lba)
{
  int has_sub = 0;
  int rc;
  int i;
  LONG call_count;

  call_count = InterlockedIncrement(&cdra_read_call_count);

  /* Periodic status log so we can verify the worker is actually doing
   * work and spot slow-I/O patterns.  Threshold picked so the line
   * shows up every ~14 seconds of gameplay (PSX issues ~75 reads/sec). */
  if ((call_count & 0x3FF) == 0) {
    pcsxr_log(0 /* INFO */,
              "[cdra] reads=%ld hits=%ld misses=%ld | "
              "lru_hits=%ld lru_misses=%ld | "
              "slow_reads=%ld total_slow_ms=%ld max_read_ms=%ld | "
              "miss_streak_max=%ld | "
              "worker_iter_slow=%ld worker_max_iter_ms=%ld worker_wakes=%ld | "
              "enabled=%d init=%d\n",
              (long)cdra_read_call_count, (long)cdra_hit_count,
              (long)cdra_miss_count,
              (long)cdra_lru_hits, (long)cdra_lru_misses,
              (long)cdra_slow_read_count, (long)cdra_slow_read_total_ms,
              (long)cdra_max_single_read_ms,
              (long)cdra_miss_streak_max,
              (long)cdra_worker_slow_iter_count, (long)cdra_worker_max_iter_ms,
              (long)cdra_worker_wake_count,
              cdra_enabled, cdra_initialized);
  }

  /* Disabled path: bypass ring entirely.  Same code as pre-async.
   * Still instrument so the user can see how bad sync I/O is even
   * without the ring helping.  When disabled there's no worker so no
   * lock contention either; the time is pure disk latency OR thread
   * preemption / CPU-bound work outside this function. */
  if (!cdra_enabled || !cdra_initialized) {
    LARGE_INTEGER t0, t1;
    FILETIME user0, kern0, user1, kern1;
    unsigned int ms, cpu_ms;
    int rc_dis;

    cdra_sample_thread_cpu(&user0, &kern0);
    QueryPerformanceCounter(&t0);
    rc_dis = cdra_read_sync_internal(lba, cdbuffer, subbuffer, &has_sub);
    QueryPerformanceCounter(&t1);
    cdra_sample_thread_cpu(&user1, &kern1);

    ms = cdra_qpc_delta_ms(t0, t1);
    cpu_ms = cdra_filetime_delta_ms(user0, user1)
           + cdra_filetime_delta_ms(kern0, kern1);

    if (ms > (unsigned int)cdra_max_single_read_ms)
      InterlockedExchange(&cdra_max_single_read_ms, (LONG)ms);
    if (ms >= CDRA_SLOW_READ_THRESHOLD_MS) {
      const char *kind;
      InterlockedIncrement(&cdra_slow_read_count);
      InterlockedExchangeAdd(&cdra_slow_read_total_ms, (LONG)ms);
      if (cpu_ms * 10 < ms)         kind = "BLOCKED/PREEMPTED";
      else if (cpu_ms * 5 > ms * 4) kind = "CPU-BOUND";
      else                          kind = "MIXED";
      pcsxr_log(2 /* WARN */,
                "[cdra] SLOW direct read (prefetch off): lba=%d wall=%u ms "
                "cpu=%u ms (%s) rc=%d has_sub=%d\n",
                lba, ms, cpu_ms, kind, rc_dis, has_sub);
    }
    return rc_dis;
  }

  /* Ring lookup.  Hold ring_cs only for the lookup + copy out.  Don't
   * call I/O code under ring_cs. */
  EnterCriticalSection(&cdra_ring_cs);
  for (i = 0; i < CDRA_RING_SLOTS; i++) {
    if (cdra_ring[i].lba == lba) {
      memcpy(cdbuffer, cdra_ring[i].cd, CD_FRAMESIZE_RAW);
      if (cdra_ring[i].has_sub)
        memcpy(subbuffer, cdra_ring[i].sub, SUB_FRAMESIZE);
      cdra_ring[i].lba = -1;            /* mark consumed */
      LeaveCriticalSection(&cdra_ring_cs);
      InterlockedIncrement(&cdra_hit_count);
      /* Reset miss streak counter on hit. */
      InterlockedExchange(&cdra_miss_streak_current, 0);

      /* Signal worker to keep prefetching from lba+1. */
      InterlockedExchange(&cdra_anchor, (LONG)(lba + 1));
      InterlockedIncrement(&cdra_generation);
      SetEvent(cdra_wakeup_evt);
      return 0;
    }
  }
  LeaveCriticalSection(&cdra_ring_cs);
  InterlockedIncrement(&cdra_miss_count);

  /* Track consecutive miss streaks.  A long streak means the worker
   * couldn't keep up (random seeks, worker preemption, or huge hunk
   * decompression chain).  Log when we cross the threshold. */
  {
    LONG streak = InterlockedIncrement(&cdra_miss_streak_current);
    if (streak > cdra_miss_streak_max)
      InterlockedExchange(&cdra_miss_streak_max, streak);
    if (streak == CDRA_STALL_BURST_THRESHOLD) {
      pcsxr_log(2 /* WARN */,
                "[cdra] miss streak hit %ld consecutive (lba=%d, worker may be "
                "preempted or seek pattern too random)\n",
                (long)streak, lba);
    }
  }

  /* Miss: do the read synchronously on the main thread (caller is
   * waiting on this byte anyway).  Hold io_cs to serialize with the
   * worker.  Time both wall clock AND thread CPU time so we can tell
   * whether long stalls were I/O waits (cpu << wall) or CPU-bound work
   * (cpu ~ wall). */
  {
    LARGE_INTEGER t0, t1;
    FILETIME user0, kern0, user1, kern1;
    unsigned int ms, cpu_ms;

    cdra_sample_thread_cpu(&user0, &kern0);
    QueryPerformanceCounter(&t0);
    EnterCriticalSection(&cdra_io_cs);
    rc = cdra_read_sync_internal(lba, cdbuffer, subbuffer, &has_sub);
    LeaveCriticalSection(&cdra_io_cs);
    QueryPerformanceCounter(&t1);
    cdra_sample_thread_cpu(&user1, &kern1);

    ms = cdra_qpc_delta_ms(t0, t1);
    cpu_ms = cdra_filetime_delta_ms(user0, user1)
           + cdra_filetime_delta_ms(kern0, kern1);

    if (ms > (unsigned int)cdra_max_single_read_ms)
      InterlockedExchange(&cdra_max_single_read_ms, (LONG)ms);

    if (ms >= CDRA_SLOW_READ_THRESHOLD_MS) {
      const char *kind;
      InterlockedIncrement(&cdra_slow_read_count);
      InterlockedExchangeAdd(&cdra_slow_read_total_ms, (LONG)ms);

      /* Classify what kind of stall this was based on CPU vs wall.
       * Threshold: if CPU < 10% of wall, the thread was mostly waiting
       * (I/O, mutex, scheduler-preempted).  If CPU > 80% of wall, the
       * thread was actively running -- possibly a JIT compile or other
       * CPU-bound work happened during the read.  Otherwise mixed. */
      if (cpu_ms * 10 < ms)
        kind = "BLOCKED/PREEMPTED";      /* thread not running */
      else if (cpu_ms * 5 > ms * 4)
        kind = "CPU-BOUND";              /* thread actively running */
      else
        kind = "MIXED";
      pcsxr_log(2 /* WARN */,
                "[cdra] SLOW sync read: lba=%d wall=%u ms cpu=%u ms (%s) "
                "rc=%d has_sub=%d\n",
                lba, ms, cpu_ms, kind, rc, has_sub);
    }
  }

  if (rc == 0) {
    /* Bump generation BEFORE updating anchor so a worker that's mid-
     * iteration bails before reading the new anchor.  The order
     * doesn't strictly matter for correctness (the loop rechecks both
     * each iteration) but generation-first slightly reduces wasted
     * worker work on the old chain. */
    InterlockedIncrement(&cdra_generation);
    InterlockedExchange(&cdra_anchor, (LONG)(lba + 1));
    SetEvent(cdra_wakeup_evt);
  }
  return rc;
}

#endif /* CDRISO_ASYNC_C_INCLUDED */
