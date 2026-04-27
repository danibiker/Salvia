/*
 * gpu_duck_sw_backend.h
 *
 * VS2010-compatible port of SwanStation's gpu_sw_backend.h.
 *
 * Changes from upstream:
 *   - `using X = Y;` aliases rewritten as typedefs.
 *   - `static constexpr DitherLUT ComputeDitherLUT();` was a C++17
 *     constexpr function returning a large std::array; we keep the
 *     same LUT but populate it at runtime in ctor / InitializeLUTs().
 *   - `std::clamp<s32>` calls rewritten as DUCK_CLAMP.
 *   - std::array is VS2010-compatible, kept as-is.
 */
#pragma once

#include "gpu_duck_compat.h"
#include "gpu_duck_gpu_backend.h"
#include "gpu_duck_gpu_types.h"

#include <array>
#include <memory>
#include <vector>

class GPU_Duck_Driver; /* forward — friend below */

class GPU_SW_Backend : public GPUBackend
{
  /* The bridge calls the protected FillVRAM/DrawPolygon/etc. virtuals
   * directly rather than via a command FIFO, so grant it access. The
   * base class already friends GPU_Duck_Driver for the same reason. */
  friend class GPU_Duck_Driver;

public:
  /* Default ctor: owns an internal VRAM buffer. */
  GPU_SW_Backend();

  /* External-VRAM ctor: the backend draws into a buffer owned by
   * someone else (e.g. the surrounding PSX emulator). The caller
   * must guarantee the pointer stays valid for the backend's
   * lifetime and that the buffer is at least VRAM_WIDTH*VRAM_HEIGHT
   * u16s in size. */
  explicit GPU_SW_Backend(u16* external_vram);

  virtual ~GPU_SW_Backend();

  virtual bool Initialize(bool force_thread);
  virtual void Reset(bool clear_vram);

  /* All pixel accessors go through m_vram_ptr (inherited from the
   * base) so they work whether the backing store is `m_vram` or an
   * external buffer.
   *
   * On BE hosts VRAMSwap converts between the PSX-LE byte layout that
   * lives in memory (shared with psxVuw) and the host-native 5551
   * values the rasterizer computes on. On LE hosts it's a no-op.
   * GetPixelPtr is NOT byteswapped — any caller using the raw pointer
   * must swap explicitly. No code currently does; it's only exposed
   * for parity with the upstream surface. */
  ALWAYS_INLINE_RELEASE u16        GetPixel(u32 x, u32 y) const     { return VRAMSwap(m_vram_ptr[VRAM_WIDTH * y + x]); }
  ALWAYS_INLINE_RELEASE const u16* GetPixelPtr(u32 x, u32 y) const  { return &m_vram_ptr[VRAM_WIDTH * y + x]; }
  ALWAYS_INLINE_RELEASE u16*       GetPixelPtr(u32 x, u32 y)        { return &m_vram_ptr[VRAM_WIDTH * y + x]; }
  ALWAYS_INLINE_RELEASE void       SetPixel(u32 x, u32 y, u16 value){ m_vram_ptr[VRAM_WIDTH * y + x] = VRAMSwap(value); }

  /* Actually (31 * 255) >> 4 == 494, but rounded up to the next power
   * of two (512) to simplify addressing. */
  static const u32 DITHER_LUT_SIZE = 512;
  typedef std::array<std::array<std::array<u8, 512>, DITHER_MATRIX_SIZE>, DITHER_MATRIX_SIZE> DitherLUT;

protected:
  union VRAMPixel
  {
    u16 bits;

    BitField<u16, u8,   0,  5> r;
    BitField<u16, u8,   5,  5> g;
    BitField<u16, u8,   10, 5> b;
    BitField<u16, bool, 15, 1> c;

    void Set(u8 r_, u8 g_, u8 b_, bool c_ = false)
    {
      bits = static_cast<u16>(
        (ZeroExtend16(r_)) | (ZeroExtend16(g_) << 5) | (ZeroExtend16(b_) << 10) | (static_cast<u16>(c_) << 15));
    }

    void ClampAndSet(u8 r_, u8 g_, u8 b_, bool c_ = false)
    {
      Set((std::min)(r_, static_cast<u8>(0x1F)),
          (std::min)(g_, static_cast<u8>(0x1F)),
          (std::min)(b_, static_cast<u8>(0x1F)),
          c_);
    }

    void SetRGB24(u32 rgb24, bool c_ = false)
    {
      bits = static_cast<u16>(
        Truncate16(((rgb24 >> 3) & 0x1F) | (((rgb24 >> 11) & 0x1F) << 5) | (((rgb24 >> 19) & 0x1F) << 10)) |
        (static_cast<u16>(c_) << 15));
    }

    void SetRGB24(u8 r8, u8 g8, u8 b8, bool c_ = false)
    {
      bits = static_cast<u16>(
        (ZeroExtend16(r8 >> 3)) | (ZeroExtend16(g8 >> 3) << 5) | (ZeroExtend16(b8 >> 3) << 10) |
        (static_cast<u16>(c_) << 15));
    }

    void SetRGB24Dithered(u32 x, u32 y, u8 r8, u8 g8, u8 b8, bool c_ = false)
    {
      const s32 offset = DITHER_MATRIX[y & 3][x & 3];
      r8 = static_cast<u8>(DUCK_CLAMP(static_cast<s32>(ZeroExtend32(r8)) + offset, 0, 255));
      g8 = static_cast<u8>(DUCK_CLAMP(static_cast<s32>(ZeroExtend32(g8)) + offset, 0, 255));
      b8 = static_cast<u8>(DUCK_CLAMP(static_cast<s32>(ZeroExtend32(b8)) + offset, 0, 255));
      SetRGB24(r8, g8, b8, c_);
    }

    u32 ToRGB24() const
    {
      const u32 r_ = ZeroExtend32(r.GetValue());
      const u32 g_ = ZeroExtend32(g.GetValue());
      const u32 b_ = ZeroExtend32(b.GetValue());
      return ((r_ << 3) | (r_ & 7)) | (((g_ << 3) | (g_ & 7)) << 8) | (((b_ << 3) | (b_ & 7)) << 16);
    }
  };

  /* Tuple-unpack helpers: upstream used std::make_tuple/structured
   * bindings. Ported callers receive values by out-params or by
   * calling the componentwise helpers directly. Keep for reference,
   * unused in the ported .cpp. */
  static void UnpackTexcoord(u16 texcoord, u8& out_u, u8& out_v)
  {
    out_u = static_cast<u8>(texcoord);
    out_v = static_cast<u8>(texcoord >> 8);
  }

  static void UnpackColorRGB24(u32 rgb24, u8& out_r, u8& out_g, u8& out_b)
  {
    out_r = static_cast<u8>(rgb24);
    out_g = static_cast<u8>(rgb24 >> 8);
    out_b = static_cast<u8>(rgb24 >> 16);
  }

  virtual void FillVRAM(u32 x, u32 y, u32 width, u32 height, u32 color, GPUBackendCommandParameters params);
  virtual void UpdateVRAM(u32 x, u32 y, u32 width, u32 height, const void* data, GPUBackendCommandParameters params);
  virtual void CopyVRAM(u32 src_x, u32 src_y, u32 dst_x, u32 dst_y, u32 width, u32 height,
                        GPUBackendCommandParameters params);

  virtual void DrawPolygon(const GPUBackendDrawPolygonCommand* cmd);
  virtual void DrawLine(const GPUBackendDrawLineCommand* cmd);
  virtual void DrawRectangle(const GPUBackendDrawRectangleCommand* cmd);
  virtual void FlushRender();

  /* -- Rasterisation ---------------------------------------------- */

  template<bool texture_enable, bool raw_texture_enable, bool transparency_enable, bool dithering_enable>
  void ShadePixel(const GPUBackendDrawCommand* cmd, u32 x, u32 y, u8 color_r, u8 color_g, u8 color_b, u8 texcoord_x,
                  u8 texcoord_y);

  template<bool texture_enable, bool raw_texture_enable, bool transparency_enable>
  void DrawRectangleTemplated(const GPUBackendDrawRectangleCommand* cmd);

  typedef void (GPU_SW_Backend::*DrawRectangleFunction)(const GPUBackendDrawRectangleCommand* cmd);
  DrawRectangleFunction GetDrawRectangleFunction(bool texture_enable, bool raw_texture_enable,
                                                 bool transparency_enable);

  /* -- Polygon and line rasterisation (Mednafen-derived) ---------- */
  struct i_deltas
  {
    u32 du_dx, dv_dx;
    u32 dr_dx, dg_dx, db_dx;

    u32 du_dy, dv_dy;
    u32 dr_dy, dg_dy, db_dy;
  };

  struct i_group
  {
    u32 u, v;
    u32 r, g, b;
  };

  template<bool shading_enable, bool texture_enable>
  bool CalcIDeltas(i_deltas& idl, const GPUBackendDrawPolygonCommand::Vertex* A,
                   const GPUBackendDrawPolygonCommand::Vertex* B, const GPUBackendDrawPolygonCommand::Vertex* C);

  template<bool shading_enable, bool texture_enable>
  void AddIDeltas_DX(i_group& ig, const i_deltas& idl, u32 count = 1);

  template<bool shading_enable, bool texture_enable>
  void AddIDeltas_DY(i_group& ig, const i_deltas& idl, u32 count = 1);

  template<bool shading_enable, bool texture_enable, bool raw_texture_enable, bool transparency_enable,
           bool dithering_enable>
  void DrawSpan(const GPUBackendDrawPolygonCommand* cmd, s32 y, s32 x_start, s32 x_bound, i_group ig,
                const i_deltas& idl);

  template<bool shading_enable, bool texture_enable, bool raw_texture_enable, bool transparency_enable,
           bool dithering_enable>
  void DrawTriangle(const GPUBackendDrawPolygonCommand* cmd, const GPUBackendDrawPolygonCommand::Vertex* v0,
                    const GPUBackendDrawPolygonCommand::Vertex* v1, const GPUBackendDrawPolygonCommand::Vertex* v2);

  typedef void (GPU_SW_Backend::*DrawTriangleFunction)(const GPUBackendDrawPolygonCommand* cmd,
                                                       const GPUBackendDrawPolygonCommand::Vertex* v0,
                                                       const GPUBackendDrawPolygonCommand::Vertex* v1,
                                                       const GPUBackendDrawPolygonCommand::Vertex* v2);
  DrawTriangleFunction GetDrawTriangleFunction(bool shading_enable, bool texture_enable, bool raw_texture_enable,
                                               bool transparency_enable, bool dithering_enable);

  template<bool shading_enable, bool transparency_enable, bool dithering_enable>
  void DrawLineTemplated(const GPUBackendDrawLineCommand* cmd, const GPUBackendDrawLineCommand::Vertex* p0,
                         const GPUBackendDrawLineCommand::Vertex* p1);

  typedef void (GPU_SW_Backend::*DrawLineFunction)(const GPUBackendDrawLineCommand* cmd,
                                                   const GPUBackendDrawLineCommand::Vertex* p0,
                                                   const GPUBackendDrawLineCommand::Vertex* p1);
  DrawLineFunction GetDrawLineFunction(bool shading_enable, bool transparency_enable, bool dithering_enable);

  std::array<u16, VRAM_WIDTH * VRAM_HEIGHT> m_vram;
};
