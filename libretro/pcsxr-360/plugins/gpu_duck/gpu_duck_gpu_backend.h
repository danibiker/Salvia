/*
 * gpu_duck_gpu_backend.h
 *
 * Minimal VS2010-compatible replacement for SwanStation's gpu_backend.h.
 *
 * Upstream GPUBackend owns a command FIFO, a worker thread, and a lock
 * pair for async command submission. On Xbox 360 / XDK we have no
 * `<atomic>`, `<thread>`, `<mutex>`, or `<condition_variable>` at all,
 * and we don't need any of it — pcsxr-360 already dispatches GPU work
 * synchronously from the emulation thread. So we strip the backend
 * down to:
 *   - The virtual draw/VRAM dispatch surface that GPU_SW_Backend
 *     overrides.
 *   - An m_vram_ptr + m_drawing_area pair (the only pieces of the
 *     base class that GPU_SW_Backend actually reads).
 *   - The lifecycle virtuals (Initialize / Reset / Shutdown).
 *
 * The bridge layer (peops-GP0-to-backend translator) calls the
 * Draw/Fill/Update/Copy methods directly on the concrete backend
 * instance; nothing ever goes through a FIFO.
 */
#pragma once

#include "gpu_duck_compat.h"
#include "gpu_duck_gpu_types.h"

/* ------------------------------------------------------------------
 * VRAM byte-order helper.
 *
 * The PSX stores VRAM as little-endian 16-bit values. pcsxr-360 shares
 * its VRAM buffer (psxVuw) with the xbox_soft plugin, which accesses
 * it via PUTLE16 / GETLE16 — i.e. always through byteswap on a BE
 * host. That means the bytes sitting in memory are always in PSX
 * (little-endian) order, regardless of host endianness.
 *
 * The SwanStation-derived rasterizer was written for LE hosts, where
 * `*ptr` on a u16 slot gives the PSX-native value directly. On a BE
 * host (Xbox 360 PPC) the same read produces a byteswapped value, so
 * every u16 read/write has to go through VRAMSwap to convert between
 * the two views. VRAMSwap is a no-op on LE and a byteswap on BE.
 *
 * Invariants:
 *   - Memory (`m_vram_ptr[i]` as raw bytes) is always in PSX LE order.
 *   - After VRAMSwap the value is in host-native 5551: bit 15 = mask,
 *     bits 10-14 = blue, bits 5-9 = green, bits 0-4 = red.
 *   - The rasterizer does all bit math on host-native values and
 *     swaps back on store.
 * ---------------------------------------------------------------- */
/* DUCK_BIG_ENDIAN is defined in gpu_duck_compat.h (hoisted there so
 * gpu_types.h can use it too). Kept as DUCK_VRAM_BIG_ENDIAN here as a
 * local alias purely for documentation — it marks the spots that care
 * about VRAM byte ordering specifically, rather than generic host
 * endianness. */
#define DUCK_VRAM_BIG_ENDIAN DUCK_BIG_ENDIAN

ALWAYS_INLINE static u16 VRAMSwap(u16 v)
{
#if DUCK_VRAM_BIG_ENDIAN
  /* Manual shift. On PPC, MSVC compiles this to a single `rlwinm` +
   * `rlwimi` pair (1-2 cycles), same as the intrinsic path. Keeps us
   * independent of `<stdlib.h>` being included for _byteswap_ushort,
   * which the XDK headers do not always drag in for us. */
  return static_cast<u16>(((v & 0x00FFu) << 8) | ((v & 0xFF00u) >> 8));
#else
  return v;
#endif
}

class GPUBackend
{
public:
  GPUBackend() : m_vram_ptr(0) {}
  virtual ~GPUBackend() {}

  ALWAYS_INLINE u16* GetVRAM() const { return m_vram_ptr; }

  virtual bool Initialize(bool force_thread) { UNREFERENCED_VARIABLE(force_thread); return true; }
  virtual void UpdateSettings() {}
  virtual void Reset(bool clear_vram) { UNREFERENCED_VARIABLE(clear_vram); }
  virtual void Shutdown() {}

  /* Drawing area is set by the front-end before each primitive burst.
   * The software rasteriser reads it to scissor out-of-bounds pixels. */
  void SetDrawingArea(const Common::Rectangle<u32>& area) { m_drawing_area = area; }
  const Common::Rectangle<u32>& GetDrawingArea() const { return m_drawing_area; }

protected:
  virtual void FillVRAM(u32 x, u32 y, u32 width, u32 height, u32 color,
                        GPUBackendCommandParameters params) = 0;
  virtual void UpdateVRAM(u32 x, u32 y, u32 width, u32 height, const void* data,
                          GPUBackendCommandParameters params) = 0;
  virtual void CopyVRAM(u32 src_x, u32 src_y, u32 dst_x, u32 dst_y, u32 width, u32 height,
                        GPUBackendCommandParameters params) = 0;
  virtual void DrawPolygon(const GPUBackendDrawPolygonCommand* cmd) = 0;
  virtual void DrawRectangle(const GPUBackendDrawRectangleCommand* cmd) = 0;
  virtual void DrawLine(const GPUBackendDrawLineCommand* cmd) = 0;
  virtual void FlushRender() = 0;

  /* Friend declaration so the bridge / driver can dispatch directly. */
  friend class GPU_Duck_Driver;

  u16* m_vram_ptr;
  Common::Rectangle<u32> m_drawing_area;
};
