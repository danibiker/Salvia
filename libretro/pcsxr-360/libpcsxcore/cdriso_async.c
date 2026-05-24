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

/* PCSXR_CDRA_DIAG default 0 -- declarado en cdriso_async.h. */

#if PCSXR_CDRA_DIAG
#  define CDRA_LOG(level, ...)     pcsxr_log((level), __VA_ARGS__)
#  define CDRA_STAT_INC(c)         InterlockedIncrement(&(c))
#  define CDRA_STAT_ADD(c, v)      InterlockedExchangeAdd(&(c), (LONG)(v))
#  define CDRA_STAT_EXCH(c, v)     InterlockedExchange(&(c), (LONG)(v))
#else
#  define CDRA_LOG(level, ...)     ((void)0)
#  define CDRA_STAT_INC(c)         ((void)0)
#  define CDRA_STAT_ADD(c, v)      ((void)0)
#  define CDRA_STAT_EXCH(c, v)     ((void)0)
#endif

/* Xbox 360 hardware thread index for the worker.  HW thread 4 = core 2
 * primary, away from the main emulator threads. */
#define CDRA_WORKER_HW_THREAD 4

/* === Lock-free sector pool ===
 *
 * [XBOX360] Rediseno: pool de sectores indexado por (lba mod POOL_SIZE).
 * SIN critical sections en el hot path.  Sincronizacion via seqlock
 * sobre `lba` usando InterlockedExchange (full memory barriers en
 * PowerPC del Xbox 360).
 *
 *  Worker (single producer):
 *    1. InterlockedExchange(slot.lba, -1)   ; invalida slot
 *    2. fread datos en slot.cd / slot.sub   ; sin lock
 *    3. InterlockedExchange(slot.lba, lba)  ; publica
 *
 *  Main (single consumer):
 *    1. InterlockedExchangeAdd(slot.lba,0)  ; lectura atomica con barrera
 *    2. Si == lba_target: memcpy datos      ; sin lock
 *    3. Re-read slot.lba                    ; si cambio = data invalida
 *
 * Colisiones: dos LBAs con (lba % N) iguales comparten slot.  Solo el
 * mas reciente cacheado.  Para gameplay secuencial (que es la mayoria
 * del tiempo) funciona bien.
 *
 * POOL_SIZE 4096 = ~9.6 MB, ventana amplia para absorber outliers USB
 * (>300 ms) y reducir colisiones mod-N en re-lecturas.  Memoria
 * estaticamente alocada en BSS — sin malloc, sin null checks.
 *
 * Soporta dos formatos:
 *  - BIN/CUE/Mode1ISO: worker usa cdHandle_worker (FILE* paralelo)
 *  - CHD: worker usa chdFile_worker (chd_file* paralelo)
 * En ambos casos cada thread tiene su propio handle, sin race ni lock. */
#define CDRA_POOL_SIZE  4096

typedef struct {
  volatile LONG lba;                            /* -1=empty/loading, else LBA */
  volatile LONG has_sub;                        /* 1 if sub valido */
  unsigned char cd[CD_FRAMESIZE_RAW];           /* 2352 bytes */
  unsigned char sub[SUB_FRAMESIZE];             /* 96 bytes */
} cdra_pool_slot_t;

/* Pool estatico en BSS — sin malloc/free, sin NULL checks. */
static cdra_pool_slot_t  cdra_pool[CDRA_POOL_SIZE];
static volatile LONG     cdra_pool_target = -1;    /* worker fills around this LBA */
static int               cdra_pool_active = 0;     /* 0 si CHD/sin worker handle */

/* === Worker control === */
static HANDLE            cdra_wakeup_evt   = NULL;
static HANDLE            cdra_worker_h     = NULL;
static volatile LONG     cdra_worker_exit  = 0;
static int               cdra_enabled      = 1;
static int               cdra_initialized  = 0;

/* [XBOX360] Flag para sincronizacion segura en disc swap.
 *
 * El worker thread lo pone a 1 mientras esta DENTRO de la llamada de
 * I/O (chd_read / fseek+fread), y a 0 entre iteraciones.  ISOclose
 * llama a cdra_wait_worker_idle() para esperar a que sea 0 antes de
 * cerrar los handles, evitando que el chd_close/fclose libere memoria
 * que el worker esta usando activamente. */
static volatile LONG     cdra_worker_busy  = 0;

/* Visible to host log via pcsxr_log. */
extern void pcsxr_log(int level, const char *format, ...);

#if PCSXR_CDRA_DIAG
/* === Diagnostic stats counters (compilados fuera si PCSXR_CDRA_DIAG=0) ===
 * Sin DIAG estas variables no existen y los CDRA_STAT_* macros expanden
 * a (void)0, asi que cero overhead en hot path. */
static volatile LONG     cdra_pool_hits              = 0;
static volatile LONG     cdra_pool_misses            = 0;
static volatile LONG     cdra_pool_fills             = 0;
static volatile LONG     cdra_worker_entered_count   = 0;
static volatile LONG     cdra_worker_wake_count      = 0;
static volatile LONG     cdra_read_call_count        = 0;

#define CDRA_SLOW_READ_THRESHOLD_MS  15
static LARGE_INTEGER     cdra_qpc_freq;             /* set in cdra_init */
static volatile LONG     cdra_slow_read_count        = 0;
static volatile LONG     cdra_slow_read_total_ms     = 0;
static volatile LONG     cdra_max_single_read_ms     = 0;
static volatile LONG     cdra_worker_slow_iter_count = 0;  /* worker iter >100ms */
static volatile LONG     cdra_worker_max_iter_ms     = 0;
#endif /* PCSXR_CDRA_DIAG */

#if PCSXR_CDRA_DIAG
/* Convert QPC delta to ms.  freq is ticks/sec, delta is in ticks. */
static unsigned int cdra_qpc_delta_ms(LARGE_INTEGER t0, LARGE_INTEGER t1)
{
  if (cdra_qpc_freq.QuadPart == 0) return 0;
  return (unsigned int)(((t1.QuadPart - t0.QuadPart) * 1000ULL)
                        / cdra_qpc_freq.QuadPart);
}
#endif

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


/* === cdra_pool_read_sector_direct ===
 *
 * [XBOX360] Lectura DIRECTA de un sector.  Sin LRU, sin locks.  Usado
 * por el worker thread lock-free para llenar el pool.
 *
 * Dos paths segun formato:
 *   - CHD: usa chdFile_worker (chd_file* paralelo abierto en parsechd)
 *     y descomprime un hunk en el buffer worker_hunk_buf, luego extrae
 *     el sector.  Cada chd_file* tiene su propio buffer interno de
 *     descompresion, no race con main thread (que usa chdFile).
 *   - BIN/CUE/Mode1ISO: usa el FILE* `h` (cdHandle_worker) para
 *     fseek + fread directo.
 *
 * Argumentos:
 *   lba             - sector PSX (LBA)
 *   h               - FILE* del worker para BIN/CUE.  Ignorado en CHD.
 *   worker_hunk_buf - buffer de descompresion del worker (>=chdHunkBytes).
 *                     Solo usado en path CHD.  Si NULL y CHD activo, error.
 *   out_cd          - buffer 2352 bytes
 *   out_sub         - buffer 96 bytes (puede ser NULL)
 *   out_has_sub     - se pone a 1 si rellenamos out_sub
 *
 * Returns 0 en exito, -1 en error. */
static int cdra_pool_read_sector_direct(int lba, FILE *h,
                                        unsigned char *worker_hunk_buf,
                                        unsigned char *out_cd,
                                        unsigned char *out_sub,
                                        int *out_has_sub)
{
  unsigned int fileLBAoff = 0;
  long         offset_lba;
  int          k;
  long         file_off;
  size_t       got;

  *out_has_sub = 0;

#ifdef USE_CHD
  if (chdFile_worker != NULL) {
    /* Worker CHD path.  Mismo mapeo lba -> CHD sector que
     * readchdsector pero usando chdFile_worker en vez de chdFile,
     * sin tocar el HunkCache compartido. */
    int chd_sector, hunknum, offset_in_hunk;
    int t;

    if (worker_hunk_buf == NULL) return -1;

    /* Find track that owns this LBA */
    t = numtracks;
    while (t > 1 && lba < chdTrackLBA[t]) t--;
    if (t < 1) t = 1;

    chd_sector = chdTrackCHDSector[t] + (lba - chdTrackLBA[t]);
    if (chd_sector < 0) return -1;

    hunknum        = (chd_sector * CHD_CD_FRAME_SIZE) / chdHunkBytes;
    offset_in_hunk = (chd_sector * CHD_CD_FRAME_SIZE) % chdHunkBytes;

    if (chd_read(chdFile_worker, hunknum, worker_hunk_buf) != CHDERR_NONE)
      return -1;

    memcpy(out_cd, worker_hunk_buf + offset_in_hunk, CD_FRAMESIZE_RAW);
    return 0;
  }
#endif

  /* BIN/CUE/Mode1ISO path. */
  if (h == NULL) return -1;

  /* Multi-track CUE: track 1 usa cdHandle_worker (= h pasado); tracks
   * >= 2 con .bin separado usan ti_handle_worker[k] (paralelo abierto
   * en parsecue).  Si por alguna razon no hay handle worker para ese
   * track (fopen fallo), devolver -1 y dejar que main haga sync. */
  if (numtracks > 0) {
    for (k = numtracks; k >= 1; k--) {
      int track_start_lba = (int)msf2sec(ti[k].start) - 2 * 75;
      if (track_start_lba <= lba) {
        if (ti_handle[k] != NULL && ti_handle[k] != cdHandle) {
          /* Track con FILE* propio (multi-bin cue).  Usar el handle
           * worker paralelo si esta disponible. */
          if (ti_handle_worker[k] == NULL) return -1;
          h = ti_handle_worker[k];
        }
        fileLBAoff = ti_fileLBAoffset[k];
        break;
      }
    }
  }

  offset_lba = (long)lba - (long)fileLBAoff;
  if (offset_lba < 0) offset_lba = 0;

  if (subChanMixed) {
    file_off = offset_lba * (CD_FRAMESIZE_RAW + SUB_FRAMESIZE);
    fseek(h, file_off, SEEK_SET);
    got = fread(out_cd, 1, CD_FRAMESIZE_RAW, h);
    if (got != CD_FRAMESIZE_RAW) return -1;
    if (out_sub) {
      got = fread(out_sub, 1, SUB_FRAMESIZE, h);
      if (got == SUB_FRAMESIZE) *out_has_sub = 1;
    }
  }
  else if (isMode1ISO) {
    file_off = offset_lba * MODE1_DATA_SIZE;
    fseek(h, file_off, SEEK_SET);
    got = fread(out_cd + 12, 1, MODE1_DATA_SIZE, h);
    if (got != MODE1_DATA_SIZE) return -1;
    memset(out_cd, 0, 12);
    out_cd[12 + 3] = 1;
  }
  else {
    file_off = offset_lba * CD_FRAMESIZE_RAW;
    fseek(h, file_off, SEEK_SET);
    got = fread(out_cd, 1, CD_FRAMESIZE_RAW, h);
    if (got != CD_FRAMESIZE_RAW) return -1;
  }

  return 0;
}

/* === cdra_read_sync_internal ===
 *
 * Reads sector `lba` from the underlying disc into caller-provided
 * buffers.  Same logic as the old ISOreadTrack body but parameterised
 * so it can target either the global cdbuffer/subbuffer (main thread
 * cache miss) or a ring slot's buffers (worker thread prefetch).
 *
 * `file_override`: si != NULL, se usa como FILE* primario para BIN/CUE
 * en lugar de cdHandle.  Permite que el worker thread use su propio
 * FILE* (cdHandle_worker) y no colisione con el main thread en el
 * buffer userspace de stdio.  Si es NULL se usa cdHandle como antes.
 * NO afecta al camino CHD (chdFile es shared; el caller debe coger
 * io_cs si necesita serializar HunkCache).
 *
 * Locking: para BIN/CUE con file_override != NULL, NO se necesita
 * io_cs porque cada thread tiene su FILE* (el kernel serializa
 * ReadFile al device).  Para BIN/CUE con file_override == NULL o
 * para CHD, el caller debe coger io_cs (compatibilidad).
 *
 * Returns 0 on success, -1 on error.  Sets *out_has_sub to 1 iff the
 * sub buffer was populated. */
static int cdra_read_sync_internal(int lba,
                                   unsigned char *out_cd,
                                   unsigned char *out_sub,
                                   int *out_has_sub,
                                   FILE *file_override)
{
  FILE         *h;
  unsigned int  fileLBAoff;
  long          offset_lba;
  int           k;
  long          file_off;
  size_t        got;

  *out_has_sub = 0;

#ifdef USE_CHD
  if (chdFile != NULL) {
    /* CHD path: usa el HunkCache de libchdr.  No es thread-safe sin
     * locks; el pool lock-free se desactiva para CHD (cdra_pool_active=0)
     * asi que main thread es el unico que llama aqui. */
    return (readchdsector(lba, out_cd) != 0) ? -1 : 0;
  }
#endif

  if (cdHandle == NULL) return -1;

  /* Multi-FILE cue: track 1 usa cdHandle/override; tracks >= 2 usan
   * ti_handle[k] (compartido entre main y worker -- single-track
   * BIN/CUE como NFS3 no llega aqui). */
  h          = (file_override != NULL) ? file_override : cdHandle;
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
    file_off = offset_lba * (CD_FRAMESIZE_RAW + SUB_FRAMESIZE);
    fseek(h, file_off, SEEK_SET);
    got = fread(out_cd, 1, CD_FRAMESIZE_RAW, h);
    if (got != CD_FRAMESIZE_RAW) return -1;
    got = fread(out_sub, 1, SUB_FRAMESIZE, h);
    if (got != SUB_FRAMESIZE) return -1;
    if (subChanRaw) {
      if (out_sub != subbuffer) memcpy(subbuffer, out_sub, SUB_FRAMESIZE);
      DecodeRawSubData();
      if (out_sub != subbuffer) memcpy(out_sub, subbuffer, SUB_FRAMESIZE);
    }
    *out_has_sub = 1;
  }
  else if (isMode1ISO) {
    int total = lba + 2 * 75;
    file_off = offset_lba * MODE1_DATA_SIZE;
    fseek(h, file_off, SEEK_SET);
    got = fread(out_cd + 12, 1, MODE1_DATA_SIZE, h);
    if (got != MODE1_DATA_SIZE) return -1;
    memset(out_cd, 0, 12);
    out_cd[0] = itob((unsigned char)(total / 75 / 60));
    out_cd[1] = itob((unsigned char)((total / 75) % 60));
    out_cd[2] = itob((unsigned char)(total % 75));
    out_cd[3] = 1;
  }
  else {
    /* BIN/CUE raw 2352 - the most common case */
    file_off = offset_lba * CD_FRAMESIZE_RAW;
    fseek(h, file_off, SEEK_SET);
    got = fread(out_cd, 1, CD_FRAMESIZE_RAW, h);
    if (got != CD_FRAMESIZE_RAW) return -1;
  }

  if (subHandle != NULL && !subChanMixed) {
    fseek(subHandle, (long)lba * SUB_FRAMESIZE, SEEK_SET);
    got = fread(out_sub, 1, SUB_FRAMESIZE, subHandle);
    if (got == SUB_FRAMESIZE) {
      if (subChanRaw) {
        if (out_sub != subbuffer) memcpy(subbuffer, out_sub, SUB_FRAMESIZE);
        DecodeRawSubData();
        if (out_sub != subbuffer) memcpy(out_sub, subbuffer, SUB_FRAMESIZE);
      }
      *out_has_sub = 1;
    }
  }

  return 0;
}

/* === Worker thread (lock-free pool fill) ===
 *
 * [XBOX360] Diseno lock-free.  Llena el cdra_pool con sectores
 * proximos a cdra_pool_target.  Indexado por (lba % POOL_SIZE).
 *
 * Soporta BIN/CUE (via cdHandle_worker, fseek+fread) y CHD (via
 * chdFile_worker, chd_read + descompresion en worker_hunk_buf).  En
 * ambos casos sin locks: cada thread tiene su propio handle/state. */
static DWORD WINAPI cdra_worker_proc(LPVOID arg)
{
  unsigned char *worker_hunk_buf = NULL;   /* CHD: alocado on-demand */
  size_t         worker_hunk_buf_size = 0;

  (void)arg;

  CDRA_STAT_INC(cdra_worker_entered_count);

#if defined(_XBOX)
  XSetThreadProcessor(GetCurrentThread(), CDRA_WORKER_HW_THREAD);
#endif

  while (!cdra_worker_exit) {
    int   is_chd_disc;

    /* Bloquear hasta que main pida prefetch (cdra_pool_target update). */
    WaitForSingleObject(cdra_wakeup_evt, INFINITE);
    if (cdra_worker_exit) break;
    CDRA_STAT_INC(cdra_worker_wake_count);

    if (!cdra_pool_active) continue;

    /* Detectar formato y prerequisitos.  Si CHD: necesita
     * chdFile_worker + worker_hunk_buf.  Si BIN/CUE: necesita
     * cdHandle_worker. */
#ifdef USE_CHD
    is_chd_disc = (chdFile_worker != NULL);
#else
    is_chd_disc = 0;
#endif

    if (is_chd_disc) {
#ifdef USE_CHD
      /* Asegurar hunk buffer alocado y suficientemente grande.
       * chdHunkBytes se setea en parsechd, antes de que el pool quede
       * activo, asi que aqui ya es estable. */
      if (worker_hunk_buf_size < (size_t)chdHunkBytes) {
        free(worker_hunk_buf);
        worker_hunk_buf = (unsigned char *)malloc(chdHunkBytes);
        if (worker_hunk_buf == NULL) {
          worker_hunk_buf_size = 0;
          pcsxr_log(2 /* WARN */,
                    "[cdra-pool] worker malloc(hunk_buf=%u) FAILED, "
                    "CHD prefetch disabled this run\n", chdHunkBytes);
          continue;
        }
        worker_hunk_buf_size = chdHunkBytes;
      }
#endif
    } else {
      if (cdHandle_worker == NULL) continue;  /* sin handle BIN/CUE */
    }

    /* Llenar la ventana del pool repetidamente hasta agotar trabajo.
     * Cada iter re-lee target (puede avanzar mientras leemos). */
    for (;;) {
      LONG target;
      int  fill_lba = -1;
      int  off;
      int  slot;
      int  rc;
      int  has_sub = 0;

      if (cdra_worker_exit) goto done;
      if (!cdra_enabled)    break;

      target = InterlockedExchangeAdd(&cdra_pool_target, 0);
      if (target < 0) break;

      /* Buscar primera LBA en [target, target+POOL_SIZE) que no este
       * ya en el pool.  Indexada por (lba % POOL_SIZE). */
      for (off = 0; off < CDRA_POOL_SIZE; off++) {
        int test_lba   = (int)target + off;
        int test_slot  = test_lba % CDRA_POOL_SIZE;
        LONG stored    = InterlockedExchangeAdd(&cdra_pool[test_slot].lba, 0);
        if (stored != test_lba) {
          fill_lba = test_lba;
          break;
        }
      }

      if (fill_lba < 0) break;  /* Ventana llena, dormir. */

      slot = fill_lba % CDRA_POOL_SIZE;

      /* Seqlock: invalidar antes de escribir. */
      InterlockedExchange(&cdra_pool[slot].lba, -1);
      InterlockedExchange(&cdra_pool[slot].has_sub, 0);

      /* Marcar la zona de I/O para que ISOclose pueda esperarnos.
       * El InterlockedExchange tambien actua como memory barrier. */
      InterlockedExchange(&cdra_worker_busy, 1);
#if PCSXR_CDRA_DIAG
      {
        LARGE_INTEGER wt0, wt1;
        unsigned int  wms;
        QueryPerformanceCounter(&wt0);
        rc = cdra_pool_read_sector_direct(fill_lba,
                                          cdHandle_worker,
                                          worker_hunk_buf,
                                          cdra_pool[slot].cd,
                                          cdra_pool[slot].sub,
                                          &has_sub);
        QueryPerformanceCounter(&wt1);
        wms = cdra_qpc_delta_ms(wt0, wt1);

        if (wms > (unsigned int)cdra_worker_max_iter_ms)
          InterlockedExchange(&cdra_worker_max_iter_ms, (LONG)wms);
        if (wms >= 100) {
          InterlockedIncrement(&cdra_worker_slow_iter_count);
          pcsxr_log(2 /* WARN */,
                    "[cdra-pool] WORKER slow iter: lba=%d %u ms (rc=%d)\n",
                    fill_lba, wms, rc);
        }
      }
#else
      rc = cdra_pool_read_sector_direct(fill_lba,
                                        cdHandle_worker,
                                        worker_hunk_buf,
                                        cdra_pool[slot].cd,
                                        cdra_pool[slot].sub,
                                        &has_sub);
#endif
      InterlockedExchange(&cdra_worker_busy, 0);

      if (rc != 0) {
        /* Error de lectura: dejar slot como -1 (invalido), continuar. */
        continue;
      }

      InterlockedExchange(&cdra_pool[slot].has_sub, has_sub);
      CDRA_STAT_INC(cdra_pool_fills);

      /* Publicar el LBA.  Tras este barrier, main thread puede hit. */
      InterlockedExchange(&cdra_pool[slot].lba, fill_lba);
    }
  }
done:
  if (worker_hunk_buf != NULL) {
    free(worker_hunk_buf);
    worker_hunk_buf = NULL;
  }
  return 0;
}

/* === Public API === */

void cdra_init(void)
{
  int i;
  DWORD thread_id;

  if (cdra_initialized) {
    CDRA_LOG(0 /* INFO */, "[cdra] init: already initialized, skipping\n");
    return;
  }

  CDRA_LOG(0 /* INFO */, "[cdra] init: starting\n");

#if PCSXR_CDRA_DIAG
  /* Capture the high-resolution counter frequency once so we can
   * convert ticks->ms for slow-read diagnostics. */
  QueryPerformanceFrequency(&cdra_qpc_freq);
#endif

  cdra_worker_exit = 0;

  /* Auto-reset event: SetEvent wakes one waiter, then resets. */
  cdra_wakeup_evt = CreateEvent(NULL,
                                FALSE,  /* bManualReset = FALSE */
                                FALSE,  /* bInitialState = unsignaled */
                                NULL);
  if (cdra_wakeup_evt == NULL) {
    pcsxr_log(2 /* WARN */, "[cdra] init: CreateEvent FAILED (gle=%lu)\n",
              (unsigned long)GetLastError());
    return;
  }

  cdra_worker_h = CreateThread(NULL,
                               64 * 1024,
                               cdra_worker_proc,
                               NULL,
                               0,
                               &thread_id);
  if (cdra_worker_h == NULL) {
    pcsxr_log(2 /* WARN */, "[cdra] init: CreateThread FAILED (gle=%lu)\n",
              (unsigned long)GetLastError());
    CloseHandle(cdra_wakeup_evt);
    cdra_wakeup_evt = NULL;
    return;
  }

  cdra_initialized = 1;
  CDRA_LOG(0 /* INFO */, "[cdra] init: ok, worker thread id=%lu\n",
           (unsigned long)thread_id);

  /* Pool estatico — solo inicializar slots a "vacio".  Sin malloc. */
  for (i = 0; i < CDRA_POOL_SIZE; i++) {
    cdra_pool[i].lba     = -1;
    cdra_pool[i].has_sub = 0;
  }
  cdra_pool_active = 1;  /* habilitado por defecto; se desactiva en
                          * cdra_invalidate si CHD/sin worker handle */
  CDRA_LOG(0 /* INFO */,
           "[cdra-pool] static pool ready: %d slots x %d bytes = %d KB\n",
           CDRA_POOL_SIZE,
           (int)sizeof(cdra_pool_slot_t),
           (int)(sizeof(cdra_pool_slot_t) * CDRA_POOL_SIZE / 1024));
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

  /* Pool es estatico — solo marcar inactivo (no hay nada que liberar). */
  cdra_pool_active = 0;

  cdra_initialized = 0;
}

void cdra_set_enabled(int enabled)
{
  int new_state = enabled ? 1 : 0;
  if (new_state == cdra_enabled) return;  /* no-op, avoid log spam */
  cdra_enabled = new_state;
  if (!cdra_enabled) cdra_invalidate();
  CDRA_LOG(0 /* INFO */, "[cdra] set_enabled -> %s (initialized=%d)\n",
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

  /* Invalida el lock-free pool (lo principal en hot path). */
  for (i = 0; i < CDRA_POOL_SIZE; i++) {
    InterlockedExchange(&cdra_pool[i].lba, -1);
    InterlockedExchange(&cdra_pool[i].has_sub, 0);
  }
  InterlockedExchange(&cdra_pool_target, -1);

  /* [XBOX360] Activacion del pool depende del handle del worker:
   *   - BIN/CUE/Mode1ISO: necesita cdHandle_worker (abierto en ISOopen)
   *   - CHD: necesita chdFile_worker (abierto en parsechd)
   * Si ninguno disponible, pool desactivado y main hace todo sync.
   *
   * Excepcion: si hay subHandle separado (.sub file paralelo al .bin,
   * no subChanMixed), DESACTIVAMOS el pool.  Razon: el worker no tiene
   * handle dedicado al .sub asi que no puede leer subchannel sin race
   * con main thread.  Sin sub data correcto, juegos con libcrypt o que
   * dependan de subQ (CdlGetlocP) pueden romperse.  Caso raro, vale
   * sacrificar prestaciones. */
  if (subHandle != NULL && !subChanMixed) {
    cdra_pool_active = 0;
    CDRA_LOG(0 /* INFO */,
             "[cdra-pool] desactivado: subHandle separado, sub no thread-safe\n");
  } else {
#ifdef USE_CHD
    if (chdFile != NULL) {
      cdra_pool_active = (chdFile_worker != NULL) ? 1 : 0;
    } else {
      cdra_pool_active = (cdHandle_worker != NULL) ? 1 : 0;
    }
#else
    cdra_pool_active = (cdHandle_worker != NULL) ? 1 : 0;
#endif
  }
}

/* [XBOX360] Espera a que el worker no este en mid-I/O.  Llamar antes
 * de cerrar handles en disc swap. */
void cdra_wait_worker_idle(void)
{
  int spin;

  if (!cdra_initialized) return;

  /* Desactivar pool para que el worker NO inicie nuevas I/Os mientras
   * esperamos.  cdra_invalidate hace esto pero lo repetimos por
   * defensa (esta funcion debe ser segura llamada sola). */
  cdra_pool_active = 0;
  InterlockedExchange(&cdra_pool_target, -1);

  /* Esperar a que la I/O actual del worker termine.  Maximo ~1 segundo
   * (= max outlier USB observado + margen).  En la practica son <50 ms
   * tipicos cuando el worker esta haciendo prefetch normal. */
  for (spin = 0; spin < 100; spin++) {
    if (InterlockedExchangeAdd(&cdra_worker_busy, 0) == 0) return;
    Sleep(10);
  }
  /* Si llegamos aqui, el worker lleva >1s en I/O — probablemente USB
   * congelado.  Seguimos adelante para no bloquear el cierre del
   * emulador indefinidamente.  Si la I/O completa despues, el fclose
   * habra liberado el handle y el worker crashearia — pero ese caso
   * ya era patologico (USB device error). */
}

/* [XBOX360] Hint del CDC emulator: actualiza el target del pool
 * lock-free y despierta al worker para que prefetchee desde aqui. */
void cdra_set_anchor(int lba)
{
  if (!cdra_initialized) return;
  if (!cdra_enabled)     return;
  if (lba < 0)           return;

  if (cdra_pool_active) {
    LONG cur_target = InterlockedExchangeAdd(&cdra_pool_target, 0);
    if (cur_target != lba) {
      InterlockedExchange(&cdra_pool_target, (LONG)lba);
      SetEvent(cdra_wakeup_evt);
    }
  }
}

/* [XBOX360] Reset de contadores de telemetria al cargar disco.
 * No-op cuando PCSXR_CDRA_DIAG=0 (los contadores ni se incrementan). */
void cdra_reset_stats(void)
{
#if PCSXR_CDRA_DIAG
  InterlockedExchange(&cdra_read_call_count,        0);
  InterlockedExchange(&cdra_pool_hits,              0);
  InterlockedExchange(&cdra_pool_misses,            0);
  InterlockedExchange(&cdra_pool_fills,             0);
  InterlockedExchange(&cdra_slow_read_count,        0);
  InterlockedExchange(&cdra_slow_read_total_ms,     0);
  InterlockedExchange(&cdra_max_single_read_ms,     0);
  InterlockedExchange(&cdra_worker_slow_iter_count, 0);
  InterlockedExchange(&cdra_worker_max_iter_ms,     0);
  InterlockedExchange(&cdra_worker_wake_count,      0);

  pcsxr_log(0 /* INFO */,
            "[cdra] stats reset (new disc/session)\n");
#endif
}

int cdra_read(int lba)
{
  int has_sub = 0;
  int rc;

#if PCSXR_CDRA_DIAG
  {
    LONG call_count = InterlockedIncrement(&cdra_read_call_count);
    /* Periodic status log (cada 1024 reads ~= 13 segundos de gameplay). */
    if ((call_count & 0x3FF) == 0) {
      pcsxr_log(0 /* INFO */,
                "[cdra-pool] reads=%ld pool_hits=%ld pool_misses=%ld fills=%ld | "
                "slow_reads=%ld total_slow_ms=%ld max_read_ms=%ld | "
                "worker_iter_slow=%ld worker_max_iter_ms=%ld worker_wakes=%ld | "
                "enabled=%d init=%d pool_active=%d\n",
                (long)cdra_read_call_count,
                (long)cdra_pool_hits, (long)cdra_pool_misses,
                (long)cdra_pool_fills,
                (long)cdra_slow_read_count, (long)cdra_slow_read_total_ms,
                (long)cdra_max_single_read_ms,
                (long)cdra_worker_slow_iter_count, (long)cdra_worker_max_iter_ms,
                (long)cdra_worker_wake_count,
                cdra_enabled, cdra_initialized, cdra_pool_active);
    }
  }
#endif

  /* Hint al worker: estamos en este LBA, prefetchea adelante.
   * SIN locks, SIN generation.  Worker re-lee target atomicamente.
   * FUNCIONAL — no se wrappa, debe ejecutarse siempre. */
  if (cdra_pool_active) {
    LONG cur_target = InterlockedExchangeAdd(&cdra_pool_target, 0);
    if (cur_target != lba) {
      InterlockedExchange(&cdra_pool_target, (LONG)lba);
      SetEvent(cdra_wakeup_evt);
    }
  }

  /* === FAST PATH: lock-free pool lookup === */
  if (cdra_pool_active) {
    int slot = ((unsigned)lba) % CDRA_POOL_SIZE;
    LONG stored_lba = InterlockedExchangeAdd(&cdra_pool[slot].lba, 0);
    if (stored_lba == lba) {
      LONG stored_sub;
      /* Hit candidate.  Copia datos. */
      memcpy(cdbuffer, cdra_pool[slot].cd, CD_FRAMESIZE_RAW);
      stored_sub = InterlockedExchangeAdd(&cdra_pool[slot].has_sub, 0);
      if (stored_sub) {
        memcpy(subbuffer, cdra_pool[slot].sub, SUB_FRAMESIZE);
      }
      /* Re-check seqlock: si el LBA ha cambiado durante el memcpy,
       * el worker estaba sobreescribiendo este slot -> datos corruptos,
       * caer a sync read. */
      if (InterlockedExchangeAdd(&cdra_pool[slot].lba, 0) == lba) {
        CDRA_STAT_INC(cdra_pool_hits);
        return 0;
      }
    }
    CDRA_STAT_INC(cdra_pool_misses);
  }

  /* === SLOW PATH: sync read from main thread ===
   *
   * Pool miss (o pool desactivado/CHD).  Lectura directa sin locks. */
#if PCSXR_CDRA_DIAG
  {
    LARGE_INTEGER t0, t1;
    unsigned int  ms;

    QueryPerformanceCounter(&t0);
    rc = cdra_read_sync_internal(lba, cdbuffer, subbuffer, &has_sub,
                                 NULL /* usa cdHandle */);
    QueryPerformanceCounter(&t1);

    ms = cdra_qpc_delta_ms(t0, t1);
    if (ms > (unsigned int)cdra_max_single_read_ms)
      InterlockedExchange(&cdra_max_single_read_ms, (LONG)ms);
    if (ms >= CDRA_SLOW_READ_THRESHOLD_MS) {
      InterlockedIncrement(&cdra_slow_read_count);
      InterlockedExchangeAdd(&cdra_slow_read_total_ms, (LONG)ms);
      pcsxr_log(2 /* WARN */,
                "[cdra-pool] SLOW sync read: lba=%d wall=%u ms rc=%d\n",
                lba, ms, rc);
    }
  }
#else
  rc = cdra_read_sync_internal(lba, cdbuffer, subbuffer, &has_sub,
                               NULL /* usa cdHandle */);
#endif

  return rc;
}

#endif /* CDRISO_ASYNC_C_INCLUDED */
