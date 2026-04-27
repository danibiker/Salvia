/*
 * gpu_duck_driver.h
 *
 * Bridge between the PSX GP0 command stream (as parsed by xbox_soft's
 * gpu.c accumulator) and the SwanStation-derived SW rasteriser
 * (GPU_SW_Backend). Mirrors xbox_soft's primTableJ / primTableCX but
 * every handler translates gpuDataM[] into a GPUBackendDraw*Command
 * and dispatches directly on the backend.
 *
 * What this module does:
 *   - Owns a GPU_SW_Backend instance whose VRAM pointer is aimed at
 *     the existing psxVuw buffer (so host-side display code keeps
 *     reading the same framebuffer it always has — no data copy on
 *     the hot path).
 *   - Keeps the PSX draw-mode / palette / texture-window / drawing-
 *     area / drawing-offset registers needed to populate the backend
 *     command structs. These are duplicated here rather than shared
 *     with xbox_soft globals to keep the two renderers independent.
 *   - Exposes a C entry surface (duck_*) that gpu.c's front-end calls
 *     when the duck renderer is selected.
 *
 * What this module does NOT do:
 *   - Parse the GP0 accumulator itself. That stays in gpu.c. We only
 *     swap out the primTableJ it dispatches into.
 *   - Handle GP1 (status) writes — front-end still owns GPUSTAT, the
 *     driver only needs to see SetDrawingArea / DrawingOffset / DMA
 *     direction via GP1 reset paths.
 *   - Drive the display frontend. Display stays with xb_video, which
 *     reads psxVuw as before.
 */

#pragma once

#include "gpu_duck_compat.h"
#include "gpu_duck_sw_backend.h"

#ifdef __cplusplus

class GPU_Duck_Driver
{
public:
  GPU_Duck_Driver();
  ~GPU_Duck_Driver();

  /* Initialise the driver on top of an externally-owned VRAM buffer.
   * `psx_vram` must point to a VRAM_WIDTH*VRAM_HEIGHT u16 array (the
   * PSX emulator's own framebuffer). The caller retains ownership. */
  bool Initialize(u16* psx_vram);
  void Shutdown();
  void Reset();

  /* --- Register setters, called by the 0xE1..0xE6 handlers. ----- */
  void SetTexturePage(u32 gdata);      /* GP0(E1) or poly-word-2 texpage */
  void SetTextureWindow(u32 gdata);    /* GP0(E2) */
  void SetDrawingAreaTL(u32 gdata);    /* GP0(E3) */
  void SetDrawingAreaBR(u32 gdata);    /* GP0(E4) */
  void SetDrawingOffset(u32 gdata);    /* GP0(E5) */
  void SetMaskBitSetting(u32 gdata);   /* GP0(E6) */

  /* --- Primitive emitters. Parameters are PSX-format raw words. -- */
  void EmitFillVRAM(u32 color, u16 x, u16 y, u16 w, u16 h);
  void EmitCopyVRAM(u16 src_x, u16 src_y, u16 dst_x, u16 dst_y, u16 w, u16 h);
  void EmitUpdateVRAM(u16 x, u16 y, u16 w, u16 h, const u16* pixels);

  /* Polygon emitters. Vertex arrays come packed as (x,y) pairs in
   * PSX signed-11 coords, optional per-vertex RGB and UV. */
  struct DuckVertex
  {
    s32 x, y;
    u32 color;    /* RGB24 as 0x00BBGGRR */
    u16 texcoord; /* (v<<8)|u, ignored if no texture */
  };

  void EmitPolygon(bool textured, bool shaded, bool quad,
                   bool raw_texture, bool transparent,
                   const DuckVertex* verts, u32 texpage_word, u32 palette_word);

  /* Rectangle emitter — also used for sprites. */
  void EmitRectangle(s32 x, s32 y, u16 w, u16 h, u32 color, u16 texcoord,
                     u32 palette_word, bool textured, bool raw_texture, bool transparent);

  /* Line emitter (F2/FEx/G2/GEx after the front-end has unpacked the
   * polyline terminator). Count is 2 or more vertices. */
  void EmitLine(bool shaded, bool transparent, const DuckVertex* verts, u32 count);

  /* Called once per frame by the display adapter to ensure the VRAM
   * in GPU_SW_Backend reflects the most recent draws. Currently a
   * no-op because we share a pointer with psxVuw; kept as a seam in
   * case we later separate the buffers for PPC big-endian fixup. */
  void FlushForDisplay();

  GPU_SW_Backend* GetBackend() { return m_backend; }

  /* Accessors used by per-opcode handlers to cook raw vertex coords
   * (sign-extend 11-bit, add drawing offset). */
  s32 GetDrawOffsetX() const { return m_draw_offset_x; }
  s32 GetDrawOffsetY() const { return m_draw_offset_y; }

  /* Interlace-state update hook called from the duck_set_interlaced
   * C entry point. */
  void SetInterlaced(bool enabled, u8 active_line_lsb)
  {
    m_interlaced_rendering = enabled;
    m_active_line_lsb = static_cast<u8>(active_line_lsb & 1);
  }

private:
  /* Push the current m_draw_area_* values into the backend's drawing
   * rectangle. Clamps to VRAM bounds first. */
  void PushDrawingArea();

  /* Produce a GPUBackendCommandParameters reflecting current state
   * (mask AND/OR, interlace flag). */
  GPUBackendCommandParameters BuildParams() const;

  /* Fill in the common GPUBackendDrawCommand header from current
   * driver state. */
  void FillDrawCommand(GPUBackendDrawCommand* cmd) const;

  GPU_SW_Backend* m_backend;

  /* Scratch space for variable-sized command structs (polygons have
   * trailing vertex arrays, lines can have many). Sized for the worst
   * case: 256-vertex polyline. */
  u8 m_cmd_scratch[sizeof(GPUBackendDrawLineCommand) + 256 * sizeof(GPUBackendDrawLineCommand::Vertex)];

  /* PSX state mirror. Stored in the same encoding the backend expects
   * (so BitField unions can be memcpy'd directly into cmd fields). */
  GPUDrawModeReg       m_draw_mode;
  GPUTexturePaletteReg m_palette;
  GPUTextureWindow     m_texture_window;

  /* Drawing area in absolute VRAM coords, already clamped.   */
  u32 m_draw_area_left;
  u32 m_draw_area_top;
  u32 m_draw_area_right;
  u32 m_draw_area_bottom;

  /* Drawing offset to add to incoming vertex x/y (signed 11-bit). */
  s32 m_draw_offset_x;
  s32 m_draw_offset_y;

  /* Mask bit state from GP0(E6). */
  bool m_set_mask_while_drawing;
  bool m_check_mask_before_draw;

  /* Interlaced rendering flag + active line LSB (driven by GP1 +
   * gpu.c status). Populated by duck_setInterlaced(). */
  bool m_interlaced_rendering;
  u8   m_active_line_lsb;
};

/* Singleton access for the C entry-point shims. */
GPU_Duck_Driver* duck_get_driver();

#endif /* __cplusplus */

/* ==================================================================
 * C entry points called from gpu.c / draw.c when the duck renderer is
 * the active backend. Mirrors a subset of the PEOPS_GPU* surface.
 * ================================================================ */

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise / teardown. Must be called by the surrounding plugin
 * before any other duck_* entry point. Pass the PSX VRAM pointer
 * (psxVuw in xbox_soft). */
int  duck_init(unsigned short* psx_vram);
void duck_shutdown(void);
void duck_reset(void);

/* Primitive dispatch table. Mirrors xbox_soft's primTableJ — same
 * 256-entry shape. Entries for unhandled opcodes point to duck_primNI
 * (no-op). gpu.c picks this table instead of primTableJ when the
 * libretro option selects the duck renderer.
 *
 * Handlers expect the fully-accumulated gpuDataM buffer as raw bytes
 * just like primTableJ. */
extern void (*const duck_primTable[256])(unsigned char* baseAddr);

/* Interlace-state update; gpu.c calls this when GP1(08h) changes the
 * display mode or when active-field toggles. */
void duck_set_interlaced(int enabled, int active_line_lsb);

#ifdef __cplusplus
}
#endif
