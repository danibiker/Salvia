/*
 * cdriso_async.h
 *
 * Async prefetch layer for the CD ISO reader.  Wraps the synchronous
 * I/O in cdriso.c with a worker thread + ring buffer so that the next
 * N sectors after the most-recently-read one are pre-decompressed (CHD)
 * or pre-read (BIN/CUE) on a separate core, hiding the latency from
 * the emulator's main thread.
 *
 * Architecture:
 *
 *   Emu thread (cdrom.c)
 *      |
 *      v  CDR_readTrack(time)
 *   ISOreadTrack(time) in cdriso.c
 *      |
 *      v  cdra_read(lba)            <-- this header
 *   ring buffer hit?  yes -> memcpy from ring slot to global cdbuffer
 *                     no  -> grab io_cs, do sync file read, signal worker
 *
 *   Worker thread (core 2 = HW thread 4)
 *      |
 *      v  WaitForSingleObject(wakeup_event)
 *   loop i = 0..7
 *      target_lba = prefetch_anchor + i
 *      EnterCriticalSection(io_cs)
 *      cdra_read_sync_internal(target_lba, slot.cd, slot.sub)
 *      LeaveCriticalSection(io_cs)
 *      stash in ring[next free]
 *
 * On Xbox 360 we use Win32-style sync primitives provided by the XDK
 * (CRITICAL_SECTION, CreateEvent, CreateThread, XSetThreadProcessor).
 *
 * Thread safety: the CHD HunkCache in cdriso.c is reached only via
 * cdra_read_sync_internal(), which is always called inside io_cs.
 * That serializes HunkCache access between main and worker.  No
 * change to the HunkCache itself.
 *
 * Disable path: when prefetch is off (libretro option), cdra_read()
 * just does a sync read directly with no thread interaction.  Same
 * code path as before this layer existed.
 *
 * Build note: cdriso_async.c is NOT compiled as a standalone TU.  It
 * is `#include`d at the end of cdriso.c so the worker can reach the
 * file-static state in cdriso.c (cdHandle, ti[], readchdsector, ...).
 * Do NOT add cdriso_async.c to pcsxr.vcxproj's compile list, or you'll
 * get "duplicate symbol" link errors AND the standalone compile of
 * cdriso_async.c will fail with "undeclared identifier" because the
 * cdriso.c statics aren't visible from outside its TU.
 */

#ifndef __CDRISO_ASYNC_H__
#define __CDRISO_ASYNC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Lifecycle.  cdra_init() is idempotent: safe to call multiple times
 * (no-op after first).  cdra_shutdown() joins the worker thread, frees
 * the ring buffer, destroys sync primitives.  Call cdra_shutdown()
 * BEFORE closing the cdHandle/chdFile in ISOclose so the worker can't
 * read a half-closed file. */
void cdra_init(void);
void cdra_shutdown(void);

/* libretro option hook.  Toggling at runtime is supported: when
 * disabled, cdra_read() falls back to plain sync I/O; the worker
 * thread stays alive (waiting on event that never fires) until
 * cdra_shutdown(). */
void cdra_set_enabled(int enabled);
int  cdra_is_enabled(void);

/* Main thread read entry.  Writes the sector at `lba` into the global
 * cdbuffer / subbuffer of cdriso.c (so existing ISOgetBuffer /
 * ISOgetBufferSub callers see the same data).  Returns 0 on success,
 * -1 on error.  Triggers a prefetch chain for lba+1..lba+RING_SLOTS
 * before returning. */
int  cdra_read(int lba);

/* Invalidate the entire ring.  Call on lid swap, savestate restore,
 * or any other event that means "the disc contents we cached may be
 * stale".  Doesn't stop the worker; next read will repopulate. */
void cdra_invalidate(void);

#ifdef __cplusplus
}
#endif

#endif /* __CDRISO_ASYNC_H__ */
