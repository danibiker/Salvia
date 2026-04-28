/*
 * gpu_duck_driver.cpp
 *
 * Bridge between xbox_soft's GP0 accumulator (gpu.c + primTableCX packet
 * lengths) and the SwanStation-derived software rasteriser.
 *
 * Every entry in duck_primTable[256] decodes its PSX-format packet out
 * of the `baseAddr` byte buffer, populates a GPUBackendDraw{Polygon,
 * Rectangle,Line}Command in the driver's scratch space, and dispatches
 * it synchronously on the backend.
 *
 * Nothing in this file owns a thread or a FIFO — the whole pipeline is
 * synchronous. VRAM is shared by pointer with xbox_soft's psxVuw, so
 * the display adapter keeps reading the same buffer.
 *
 * Endianness: the GP0 accumulator stores packet words in PSX/LE order.
 * On Xbox 360 (PPC, BE) we byteswap on read. A single pair of local
 * helpers (read_le_u32 / read_le_s16) hides the platform check so the
 * per-opcode handlers stay simple.
 */

#include "gpu_duck_driver.h"

#include <cstring>
#include <cstdlib>

/* Reuse xbox_soft's state-setup for VRAM transfer opcodes. These set
 * the VRAMWrite / VRAMRead globals and flip DataWriteMode /
 * DataReadMode to DR_VRAMTRANSFER; the actual pixel bytes are streamed
 * in afterwards by PEOPS_GPUwriteDataMem / GPUreadDataMem through the
 * STARTVRAM loop in xbox_soft/gpu.c, which writes directly into
 * psxVuw. Because our backend's VRAM pointer IS psxVuw (zero-copy),
 * we have nothing further to do in the duck handler — trying to read
 * the pixel payload out of the GP0 accumulator is wrong (the header
 * is only 3 words; the payload arrives on the data channel, not in
 * the command packet). */
extern "C" void primLoadImage(unsigned char* baseAddr);
extern "C" void primStoreImage(unsigned char* baseAddr);

/* ------------------------------------------------------------------
 * LE read helpers. Kept local so this TU doesn't pull in xbox_soft's
 * swap.h (which drags psxcommon.h).
 * ---------------------------------------------------------------- */
#if defined(_XBOX) || (defined(_MSC_VER) && defined(_M_PPC))
  /* Xbox 360 XDK: PPC, big-endian, MSVC intrinsics available. */
  #define DUCK_BYTESWAP_U32(x) _byteswap_ulong((unsigned long)(x))
  #define DUCK_BYTESWAP_U16(x) _byteswap_ushort((unsigned short)(x))
  #define DUCK_HOST_BIG_ENDIAN 1
#elif defined(__BIG_ENDIAN__) || defined(__PPC__)
  #define DUCK_BYTESWAP_U32(x) \
    ((((x) & 0xFFu) << 24) | (((x) & 0xFF00u) << 8) | (((x) & 0xFF0000u) >> 8) | (((x) >> 24) & 0xFFu))
  #define DUCK_BYTESWAP_U16(x) \
    ((u16)((((x) & 0xFFu) << 8) | (((x) >> 8) & 0xFFu)))
  #define DUCK_HOST_BIG_ENDIAN 1
#else
  #define DUCK_HOST_BIG_ENDIAN 0
#endif

static ALWAYS_INLINE u32 read_le_u32(const unsigned char* p)
{
  u32 v;
  std::memcpy(&v, p, sizeof(v));
#if DUCK_HOST_BIG_ENDIAN
  return DUCK_BYTESWAP_U32(v);
#else
  return v;
#endif
}

static ALWAYS_INLINE s16 read_le_s16(const unsigned char* p)
{
  u16 v;
  std::memcpy(&v, p, sizeof(v));
#if DUCK_HOST_BIG_ENDIAN
  return static_cast<s16>(DUCK_BYTESWAP_U16(v));
#else
  return static_cast<s16>(v);
#endif
}

static ALWAYS_INLINE u16 read_le_u16(const unsigned char* p)
{
  u16 v;
  std::memcpy(&v, p, sizeof(v));
#if DUCK_HOST_BIG_ENDIAN
  return DUCK_BYTESWAP_U16(v);
#else
  return v;
#endif
}

/* Word-indexed view of baseAddr. */
static ALWAYS_INLINE u32 gp0_word(const unsigned char* base, u32 word_index)
{
  return read_le_u32(base + word_index * 4);
}

/* Short-indexed view of baseAddr. */
static ALWAYS_INLINE s16 gp0_short(const unsigned char* base, u32 short_index)
{
  return read_le_s16(base + short_index * 2);
}

/* Sign-extend an 11-bit PSX vertex coordinate held in an s32. */
static ALWAYS_INLINE s32 sx11(s32 v)
{
  return SignExtendN<11, s32>(v);
}

/* ------------------------------------------------------------------
 * Driver singleton.
 * ---------------------------------------------------------------- */
static GPU_Duck_Driver* s_driver = 0;

GPU_Duck_Driver* duck_get_driver()
{
  return s_driver;
}

/* ------------------------------------------------------------------
 * Construction / lifecycle.
 * ---------------------------------------------------------------- */
GPU_Duck_Driver::GPU_Duck_Driver()
  : m_backend(0),
    m_draw_area_left(0), m_draw_area_top(0),
    m_draw_area_right(VRAM_WIDTH - 1), m_draw_area_bottom(VRAM_HEIGHT - 1),
    m_draw_offset_x(0), m_draw_offset_y(0),
    m_set_mask_while_drawing(false), m_check_mask_before_draw(false),
    m_interlaced_rendering(false), m_active_line_lsb(0)
{
  m_draw_mode.bits = 0;
  m_palette.bits = 0;
  /* Default texture window is "pass-through": and-mask all-ones, or-mask zero. */
  m_texture_window.and_x = 0xFF;
  m_texture_window.and_y = 0xFF;
  m_texture_window.or_x  = 0;
  m_texture_window.or_y  = 0;
  std::memset(m_cmd_scratch, 0, sizeof(m_cmd_scratch));
}

GPU_Duck_Driver::~GPU_Duck_Driver()
{
  Shutdown();
}

bool GPU_Duck_Driver::Initialize(u16* psx_vram)
{
  if (m_backend)
    return true;

  m_backend = new GPU_SW_Backend(psx_vram);
  if (!m_backend->Initialize(false))
  {
    delete m_backend;
    m_backend = 0;
    return false;
  }

  /* Seed the backend's drawing area with a full-VRAM default so any
   * primitive issued before the first E3/E4 pair lands somewhere
   * visible (matches DuckStation cold-start behaviour). */
  m_backend->SetDrawingArea(Common::Rectangle<u32>(0, 0, VRAM_WIDTH - 1, VRAM_HEIGHT - 1));

  s_driver = this;
  return true;
}

void GPU_Duck_Driver::Shutdown()
{
  if (m_backend)
  {
    m_backend->Shutdown();
    delete m_backend;
    m_backend = 0;
  }
  if (s_driver == this)
    s_driver = 0;
}

void GPU_Duck_Driver::Reset()
{
  if (m_backend)
  {
    m_backend->Reset(true);
    m_backend->SetDrawingArea(Common::Rectangle<u32>(0, 0, VRAM_WIDTH - 1, VRAM_HEIGHT - 1));
  }

  m_draw_mode.bits = 0;
  m_palette.bits = 0;
  m_texture_window.and_x = 0xFF;
  m_texture_window.and_y = 0xFF;
  m_texture_window.or_x  = 0;
  m_texture_window.or_y  = 0;

  m_draw_area_left = 0;
  m_draw_area_top  = 0;
  m_draw_area_right  = VRAM_WIDTH - 1;
  m_draw_area_bottom = VRAM_HEIGHT - 1;

  m_draw_offset_x = 0;
  m_draw_offset_y = 0;

  m_set_mask_while_drawing = false;
  m_check_mask_before_draw = false;

  m_interlaced_rendering = false;
  m_active_line_lsb = 0;
}

/* ------------------------------------------------------------------
 * GP0(E1..E6) state register setters.
 * ---------------------------------------------------------------- */
void GPU_Duck_Driver::SetTexturePage(u32 gdata)
{
  /* Only the low 13 bits define the draw-mode register; the upper
   * bits are either opcode or reserved. */
  m_draw_mode.bits = static_cast<u16>(gdata & GPUDrawModeReg::MASK);
}

void GPU_Duck_Driver::SetTextureWindow(u32 gdata)
{
  const u8 mask_x   = static_cast<u8>(gdata & 0x1Fu);
  const u8 mask_y   = static_cast<u8>((gdata >> 5) & 0x1Fu);
  const u8 offset_x = static_cast<u8>((gdata >> 10) & 0x1Fu);
  const u8 offset_y = static_cast<u8>((gdata >> 15) & 0x1Fu);

  /* Stored pre-computed in the texture-sampling-friendly form:
   *   final_u = (raw_u & and_x) | or_x
   *   final_v = (raw_v & and_y) | or_y  */
  m_texture_window.and_x = static_cast<u8>(~(mask_x * 8u));
  m_texture_window.and_y = static_cast<u8>(~(mask_y * 8u));
  m_texture_window.or_x  = static_cast<u8>((offset_x & mask_x) * 8u);
  m_texture_window.or_y  = static_cast<u8>((offset_y & mask_y) * 8u);
}

void GPU_Duck_Driver::SetDrawingAreaTL(u32 gdata)
{
  m_draw_area_left = gdata & 0x3FFu;
  m_draw_area_top  = (gdata >> 10) & 0x1FFu;
  PushDrawingArea();
}

void GPU_Duck_Driver::SetDrawingAreaBR(u32 gdata)
{
  m_draw_area_right  = gdata & 0x3FFu;
  m_draw_area_bottom = (gdata >> 10) & 0x1FFu;
  PushDrawingArea();
}

void GPU_Duck_Driver::SetDrawingOffset(u32 gdata)
{
  m_draw_offset_x = SignExtendN<11, s32>(static_cast<s32>(gdata & 0x7FFu));
  m_draw_offset_y = SignExtendN<11, s32>(static_cast<s32>((gdata >> 11) & 0x7FFu));
}

void GPU_Duck_Driver::SetMaskBitSetting(u32 gdata)
{
  m_set_mask_while_drawing = (gdata & 0x01u) != 0u;
  m_check_mask_before_draw = (gdata & 0x02u) != 0u;
}

/* ------------------------------------------------------------------
 * Helpers used by every draw emitter.
 * ---------------------------------------------------------------- */
void GPU_Duck_Driver::PushDrawingArea()
{
  if (!m_backend)
    return;
  /* Clamp to VRAM bounds just in case; the rasteriser treats these as
   * inclusive-inclusive. */
  u32 l = m_draw_area_left;
  u32 t = m_draw_area_top;
  u32 r = m_draw_area_right;
  u32 b = m_draw_area_bottom;
  if (l > VRAM_WIDTH  - 1) l = VRAM_WIDTH  - 1;
  if (r > VRAM_WIDTH  - 1) r = VRAM_WIDTH  - 1;
  if (t > VRAM_HEIGHT - 1) t = VRAM_HEIGHT - 1;
  if (b > VRAM_HEIGHT - 1) b = VRAM_HEIGHT - 1;
  m_backend->SetDrawingArea(Common::Rectangle<u32>(l, t, r, b));
}

GPUBackendCommandParameters GPU_Duck_Driver::BuildParams() const
{
  GPUBackendCommandParameters p;
  p.bits = 0;
  if (m_interlaced_rendering)
    p.interlaced_rendering = true;
  p.active_line_lsb = m_active_line_lsb;
  if (m_set_mask_while_drawing)
    p.set_mask_while_drawing = true;
  if (m_check_mask_before_draw)
    p.check_mask_before_draw = true;
  return p;
}

void GPU_Duck_Driver::FillDrawCommand(GPUBackendDrawCommand* cmd) const
{
  cmd->draw_mode.bits = m_draw_mode.bits;
  cmd->palette.bits   = m_palette.bits;
  cmd->window         = m_texture_window;
  cmd->params         = BuildParams();
}

/* ------------------------------------------------------------------
 * VRAM ops.
 * ---------------------------------------------------------------- */
void GPU_Duck_Driver::EmitFillVRAM(u32 color, u16 x, u16 y, u16 w, u16 h)
{
  if (!m_backend || w == 0 || h == 0)
    return;
  /* FillVRAM is unaffected by drawing-area / mask state in PSX
   * semantics — it writes the 15-bit colour directly. Pass empty
   * params so the rasteriser uses its unmasked path. */
  GPUBackendCommandParameters params;
  params.bits = 0;
  m_backend->FillVRAM(x, y, w, h, color, params);
}

void GPU_Duck_Driver::EmitCopyVRAM(u16 sx, u16 sy, u16 dx, u16 dy, u16 w, u16 h)
{
  if (!m_backend || w == 0 || h == 0)
    return;
  m_backend->CopyVRAM(sx, sy, dx, dy, w, h, BuildParams());
}

void GPU_Duck_Driver::EmitUpdateVRAM(u16 x, u16 y, u16 w, u16 h, const u16* pixels)
{
  if (!m_backend || w == 0 || h == 0 || pixels == 0)
    return;
  m_backend->UpdateVRAM(x, y, w, h, pixels, BuildParams());
}

/* ------------------------------------------------------------------
 * Polygon / rectangle / line emitters.
 *
 * These take parameters already unpacked from the PSX packet by the
 * per-opcode handler and fill a GPUBackendDraw*Command in m_cmd_scratch.
 * ---------------------------------------------------------------- */
void GPU_Duck_Driver::EmitPolygon(bool textured, bool shaded, bool quad,
                                  bool raw_texture, bool transparent,
                                  const DuckVertex* verts, u32 texpage_word, u32 palette_word)
{
  if (!m_backend)
    return;

  /* Textured polygons carry their own texpage in the second colour
   * word (or later for gouraud); adopt it as the current draw-mode so
   * the rasteriser samples from the right page. Upstream caches this
   * on GPUBackendDrawCommand::draw_mode per-draw, which is exactly
   * what FillDrawCommand copies. */
  if (textured)
  {
    const u16 tp = static_cast<u16>(texpage_word & GPUDrawModeReg::POLYGON_TEXPAGE_MASK);
    /* Keep the non-texpage bits (dither_enable, draw_to_displayed_field,
     * x/y flip) from the current register; replace the bottom 11 bits
     * from the polygon-embedded texpage. */
    m_draw_mode.bits = static_cast<u16>(
      (m_draw_mode.bits & ~GPUDrawModeReg::POLYGON_TEXPAGE_MASK) |
      (tp & GPUDrawModeReg::POLYGON_TEXPAGE_MASK));
    m_palette.bits = static_cast<u16>(palette_word & GPUTexturePaletteReg::MASK);
  }

  GPUBackendDrawPolygonCommand* cmd =
    reinterpret_cast<GPUBackendDrawPolygonCommand*>(m_cmd_scratch);

  const u16 num_verts = quad ? 4 : 3;

  cmd->type = GPUBackendCommandType::DrawPolygon;
  cmd->size = sizeof(GPUBackendDrawPolygonCommand) + num_verts * sizeof(GPUBackendDrawPolygonCommand::Vertex);
  FillDrawCommand(cmd);

  /* Synthesize the rc word the backend reads. Bit layout matches the
   * PSX GP0 polygon command byte exactly. */
  u32 rc_bits = 0;
  rc_bits |= (static_cast<u32>(GPUPrimitive::Polygon) << 29);
  if (shaded)        rc_bits |= (1u << 28);
  if (quad)          rc_bits |= (1u << 27);
  if (textured)      rc_bits |= (1u << 26);
  if (transparent)   rc_bits |= (1u << 25);
  if (raw_texture)   rc_bits |= (1u << 24);
  /* For flat polygons (shaded==false) the backend reads the vertex-0
   * colour from rc.color_for_first_vertex (bits 0-23). */
  rc_bits |= (verts[0].color & 0x00FFFFFFu);
  cmd->rc.bits = rc_bits;

  cmd->num_vertices = num_verts;
  for (u16 i = 0; i < num_verts; ++i)
  {
    GPUBackendDrawPolygonCommand::Vertex* out = &cmd->vertices[i];
    out->x        = verts[i].x;
    out->y        = verts[i].y;
    out->color    = verts[i].color & 0x00FFFFFFu;
    out->texcoord = verts[i].texcoord;
  }

  m_backend->DrawPolygon(cmd);
}

void GPU_Duck_Driver::EmitRectangle(s32 x, s32 y, u16 w, u16 h, u32 color, u16 texcoord,
                                    u32 palette_word, bool textured, bool raw_texture,
                                    bool transparent)
{
  if (!m_backend || w == 0 || h == 0)
    return;

  if (textured)
    m_palette.bits = static_cast<u16>(palette_word & GPUTexturePaletteReg::MASK);

  GPUBackendDrawRectangleCommand* cmd =
    reinterpret_cast<GPUBackendDrawRectangleCommand*>(m_cmd_scratch);

  cmd->type = GPUBackendCommandType::DrawRectangle;
  cmd->size = sizeof(GPUBackendDrawRectangleCommand);
  FillDrawCommand(cmd);

  u32 rc_bits = 0;
  rc_bits |= (static_cast<u32>(GPUPrimitive::Rectangle) << 29);
  if (textured)    rc_bits |= (1u << 26);
  if (transparent) rc_bits |= (1u << 25);
  if (raw_texture) rc_bits |= (1u << 24);
  rc_bits |= (color & 0x00FFFFFFu);
  cmd->rc.bits = rc_bits;

  cmd->x        = x;
  cmd->y        = y;
  cmd->width    = w;
  cmd->height   = h;
  cmd->texcoord = texcoord;
  cmd->color    = color & 0x00FFFFFFu;

  m_backend->DrawRectangle(cmd);
}

void GPU_Duck_Driver::EmitLine(bool shaded, bool transparent, const DuckVertex* verts, u32 count)
{
  if (!m_backend || count < 2)
    return;

  /* Emit each segment as an independent 2-vertex line command. The
   * rasteriser's line routine is specialised for 2-vertex input; we
   * don't need to push the whole polyline at once. */
  for (u32 seg = 0; seg + 1 < count; ++seg)
  {
    GPUBackendDrawLineCommand* cmd =
      reinterpret_cast<GPUBackendDrawLineCommand*>(m_cmd_scratch);

    cmd->type = GPUBackendCommandType::DrawLine;
    cmd->size = sizeof(GPUBackendDrawLineCommand) + 2 * sizeof(GPUBackendDrawLineCommand::Vertex);
    FillDrawCommand(cmd);

    u32 rc_bits = 0;
    rc_bits |= (static_cast<u32>(GPUPrimitive::Line) << 29);
    if (shaded)      rc_bits |= (1u << 28);
    if (transparent) rc_bits |= (1u << 25);
    rc_bits |= (verts[seg].color & 0x00FFFFFFu);
    cmd->rc.bits = rc_bits;

    cmd->num_vertices = 2;
    cmd->vertices[0].x = verts[seg].x;
    cmd->vertices[0].y = verts[seg].y;
    cmd->vertices[0].color = verts[seg].color & 0x00FFFFFFu;
    cmd->vertices[1].x = verts[seg + 1].x;
    cmd->vertices[1].y = verts[seg + 1].y;
    cmd->vertices[1].color = verts[seg + 1].color & 0x00FFFFFFu;

    m_backend->DrawLine(cmd);
  }
}

void GPU_Duck_Driver::FlushForDisplay()
{
  /* No-op while VRAM is shared with psxVuw. Kept as a seam so the
   * display adapter can call it unconditionally. */
}

/* ==================================================================
 * Per-opcode GP0 handlers.
 * These are what duck_primTable[256] dispatches to. They decode the
 * PSX packet out of `baseAddr` and call the driver emitters.
 * ================================================================ */

namespace /* anonymous — one TU, no linkage */
{

/* Convenience: apply offset + 11-bit sign-extension to a raw (x,y).
 * NULL-guard the driver lookup for the same reason as the state-command
 * handlers above (duck_get_driver may return NULL during the early-init
 * / hot-swap window). Falling back to offset (0,0) keeps the geometry
 * mathematically valid; nothing is drawn that frame anyway because the
 * drawing helpers further down also early-out on a missing backend. */
static ALWAYS_INLINE void cook_xy(s16 raw_x, s16 raw_y, s32& out_x, s32& out_y)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  s32 ox = drv ? drv->GetDrawOffsetX() : 0;
  s32 oy = drv ? drv->GetDrawOffsetY() : 0;
  out_x = sx11(static_cast<s32>(raw_x)) + ox;
  out_y = sx11(static_cast<s32>(raw_y)) + oy;
}

/* --- 0x02 FillVRAM ----------------------------------------------- */
static void duck_primBlkFill(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 color = gp0_word(baseAddr, 0) & 0x00FFFFFFu;
  u16 x = static_cast<u16>(gp0_short(baseAddr, 2) & 0x3F0);  /* 16-pixel aligned */
  u16 y = static_cast<u16>(gp0_short(baseAddr, 3) & 0x1FF);
  u16 w = static_cast<u16>(((static_cast<u16>(gp0_short(baseAddr, 4)) & 0x3FFu) + 0x0Fu) & ~0x0Fu);
  u16 h = static_cast<u16>(static_cast<u16>(gp0_short(baseAddr, 5)) & 0x1FFu);
  drv->EmitFillVRAM(color, x, y, w, h);
}

/* --- 0x80 MoveImage (VRAM->VRAM copy) ---------------------------- */
static void duck_primMoveImage(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u16 sx = static_cast<u16>(gp0_short(baseAddr, 2) & 0x3FF);
  const u16 sy = static_cast<u16>(gp0_short(baseAddr, 3) & 0x1FF);
  const u16 dx = static_cast<u16>(gp0_short(baseAddr, 4) & 0x3FF);
  const u16 dy = static_cast<u16>(gp0_short(baseAddr, 5) & 0x1FF);
  u16 w = static_cast<u16>(gp0_short(baseAddr, 6));
  u16 h = static_cast<u16>(gp0_short(baseAddr, 7));
  /* PSX wraparound: 0 width/height means 0x400 / 0x200. */
  if (w == 0) w = 0x400;
  if (h == 0) h = 0x200;
  drv->EmitCopyVRAM(sx, sy, dx, dy, w, h);
}

/* --- 0xA0 LoadImage (CPU->VRAM upload) ----------------------------
 * Delegate to xbox_soft. Its primLoadImage sets VRAMWrite.{x,y,Width,
 * Height,ImagePtr,RowsRemaining,ColsRemaining} and flips DataWriteMode
 * to DR_VRAMTRANSFER. The pixel stream then flows through
 * PEOPS_GPUwriteDataMem's STARTVRAM loop, which PUTLE16's each u16
 * into psxVuw. Our backend's m_vram_ptr IS psxVuw (zero-copy), so by
 * the time the next draw happens the texels are already in place.
 *
 * Previous implementation tried to read w*h u16s from baseAddr+3*4,
 * but the GP0 accumulator only holds the 3-word header — the payload
 * arrives separately on the data channel. Reading past the header
 * ran off the end of gpuDataM (1 KB) and caused an access violation
 * for any non-trivial upload. */
static void duck_primLoadImage(unsigned char* baseAddr)
{
  primLoadImage(baseAddr);
}

/* --- 0xC0 StoreImage (VRAM->CPU readback) -------------------------
 * Same story: the xbox_soft handler sets VRAMRead state; the actual
 * readback flows through PEOPS_GPUreadDataMem. We don't need to do
 * anything extra since our backend's VRAM aliases psxVuw. */
static void duck_primStoreImage(unsigned char* baseAddr)
{
  primStoreImage(baseAddr);
}

/* --- 0xE1..E6 state commands -------------------------------------
 *
 * NULL-guard `duck_get_driver()` here. The dispatch tables in
 * xbox_soft/gpu.c select duck_primTable based on `duck_gpu_enabled`,
 * and there is a window — most visibly when a libretro frontend
 * notifies a variable update mid-session, or at very early init
 * before duck_init() has run — where `duck_gpu_enabled == 1` but
 * `s_driver == NULL`. Without the guard, the next GP0 0xE1..E6
 * packet dereferences NULL and the emulator crashes (this is the
 * exact stack the user saw on duck_cmdTexturePage). Falling through
 * silently for one frame is correct and harmless: the renderer
 * choice is supposed to be sampled only at startup (the option
 * label says "restart core to apply"), so any apparent in-flight
 * change is a transient that the next retro_run cycle resolves. */
static void duck_cmdTexturePage  (unsigned char* baseAddr) { GPU_Duck_Driver* drv = duck_get_driver(); if (drv) drv->SetTexturePage   (gp0_word(baseAddr, 0)); }
static void duck_cmdTextureWindow(unsigned char* baseAddr) { GPU_Duck_Driver* drv = duck_get_driver(); if (drv) drv->SetTextureWindow (gp0_word(baseAddr, 0)); }
static void duck_cmdDrawAreaTL   (unsigned char* baseAddr) { GPU_Duck_Driver* drv = duck_get_driver(); if (drv) drv->SetDrawingAreaTL (gp0_word(baseAddr, 0)); }
static void duck_cmdDrawAreaBR   (unsigned char* baseAddr) { GPU_Duck_Driver* drv = duck_get_driver(); if (drv) drv->SetDrawingAreaBR (gp0_word(baseAddr, 0)); }
static void duck_cmdDrawOffset   (unsigned char* baseAddr) { GPU_Duck_Driver* drv = duck_get_driver(); if (drv) drv->SetDrawingOffset (gp0_word(baseAddr, 0)); }
static void duck_cmdSTP          (unsigned char* baseAddr) { GPU_Duck_Driver* drv = duck_get_driver(); if (drv) drv->SetMaskBitSetting(gp0_word(baseAddr, 0)); }

/* --- Polygon helpers: extract opcode attribute flags. ------------ */
static ALWAYS_INLINE bool is_raw_texture (u32 w) { return (w & (1u << 24)) != 0; }
static ALWAYS_INLINE bool is_transparent (u32 w) { return (w & (1u << 25)) != 0; }
static ALWAYS_INLINE bool is_shade_texture(u32 w) { return (w & (1u << 24)) == 0; }

/* --- 0x20..0x23 PolyF3 ------------------------------------------- */
static void duck_primPolyF3(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const u32 col = cmd & 0x00FFFFFFu;

  GPU_Duck_Driver::DuckVertex v[3];
  for (int i = 0; i < 3; ++i)
  {
    cook_xy(gp0_short(baseAddr, 2 + i * 2), gp0_short(baseAddr, 3 + i * 2), v[i].x, v[i].y);
    v[i].color = col;
    v[i].texcoord = 0;
  }
  drv->EmitPolygon(false, false, false, false, is_transparent(cmd), v, 0, 0);
}

/* --- 0x28..0x2B PolyF4 ------------------------------------------- */
static void duck_primPolyF4(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const u32 col = cmd & 0x00FFFFFFu;

  GPU_Duck_Driver::DuckVertex v[4];
  for (int i = 0; i < 4; ++i)
  {
    cook_xy(gp0_short(baseAddr, 2 + i * 2), gp0_short(baseAddr, 3 + i * 2), v[i].x, v[i].y);
    v[i].color = col;
    v[i].texcoord = 0;
  }
  drv->EmitPolygon(false, false, true, false, is_transparent(cmd), v, 0, 0);
}

/* --- 0x24..0x27 PolyFT3 ------------------------------------------ */
static void duck_primPolyFT3(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  /* For textured polys with shade-blend ON, upstream forces a neutral
   * 0x808080 vertex colour so the modulation is 1.0 — otherwise it's
   * the raw command colour. */
  const bool raw = is_raw_texture(cmd);
  const u32 col = raw ? 0x00808080u : (cmd & 0x00FFFFFFu);

  GPU_Duck_Driver::DuckVertex v[3];
  /* v0: pos word 1, uv word 2 lo */
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), v[0].x, v[0].y);
  v[0].color = col;
  v[0].texcoord = read_le_u16(baseAddr + 2 * 4);
  const u32 palette = gp0_word(baseAddr, 2) >> 16;

  /* v1: pos word 3, uv word 4 lo, texpage word 4 hi */
  cook_xy(gp0_short(baseAddr, 6), gp0_short(baseAddr, 7), v[1].x, v[1].y);
  v[1].color = col;
  v[1].texcoord = read_le_u16(baseAddr + 4 * 4);
  const u32 texpage = gp0_word(baseAddr, 4) >> 16;

  /* v2: pos word 5, uv word 6 lo */
  cook_xy(gp0_short(baseAddr, 10), gp0_short(baseAddr, 11), v[2].x, v[2].y);
  v[2].color = col;
  v[2].texcoord = read_le_u16(baseAddr + 6 * 4);

  drv->EmitPolygon(true, false, false, raw, is_transparent(cmd), v, texpage, palette);
}

/* --- 0x2C..0x2F PolyFT4 ------------------------------------------ */
static void duck_primPolyFT4(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const bool raw = is_raw_texture(cmd);
  const u32 col = raw ? 0x00808080u : (cmd & 0x00FFFFFFu);

  GPU_Duck_Driver::DuckVertex v[4];
  cook_xy(gp0_short(baseAddr, 2),  gp0_short(baseAddr, 3),  v[0].x, v[0].y);
  v[0].color = col; v[0].texcoord = read_le_u16(baseAddr + 2 * 4);
  const u32 palette = gp0_word(baseAddr, 2) >> 16;

  cook_xy(gp0_short(baseAddr, 6),  gp0_short(baseAddr, 7),  v[1].x, v[1].y);
  v[1].color = col; v[1].texcoord = read_le_u16(baseAddr + 4 * 4);
  const u32 texpage = gp0_word(baseAddr, 4) >> 16;

  cook_xy(gp0_short(baseAddr, 10), gp0_short(baseAddr, 11), v[2].x, v[2].y);
  v[2].color = col; v[2].texcoord = read_le_u16(baseAddr + 6 * 4);

  cook_xy(gp0_short(baseAddr, 14), gp0_short(baseAddr, 15), v[3].x, v[3].y);
  v[3].color = col; v[3].texcoord = read_le_u16(baseAddr + 8 * 4);

  drv->EmitPolygon(true, false, true, raw, is_transparent(cmd), v, texpage, palette);
}

/* --- 0x30..0x33 PolyG3 ------------------------------------------- */
static void duck_primPolyG3(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);

  GPU_Duck_Driver::DuckVertex v[3];
  /* c0, v0 | c1, v1 | c2, v2 — three (colour, vertex) pairs. */
  v[0].color = cmd & 0x00FFFFFFu;
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), v[0].x, v[0].y);
  v[0].texcoord = 0;

  v[1].color = gp0_word(baseAddr, 2) & 0x00FFFFFFu;
  cook_xy(gp0_short(baseAddr, 6), gp0_short(baseAddr, 7), v[1].x, v[1].y);
  v[1].texcoord = 0;

  v[2].color = gp0_word(baseAddr, 4) & 0x00FFFFFFu;
  cook_xy(gp0_short(baseAddr, 10), gp0_short(baseAddr, 11), v[2].x, v[2].y);
  v[2].texcoord = 0;

  drv->EmitPolygon(false, true, false, false, is_transparent(cmd), v, 0, 0);
}

/* --- 0x38..0x3B PolyG4 ------------------------------------------- */
static void duck_primPolyG4(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);

  GPU_Duck_Driver::DuckVertex v[4];
  v[0].color = cmd & 0x00FFFFFFu;
  cook_xy(gp0_short(baseAddr, 2),  gp0_short(baseAddr, 3),  v[0].x, v[0].y);
  v[0].texcoord = 0;

  v[1].color = gp0_word(baseAddr, 2) & 0x00FFFFFFu;
  cook_xy(gp0_short(baseAddr, 6),  gp0_short(baseAddr, 7),  v[1].x, v[1].y);
  v[1].texcoord = 0;

  v[2].color = gp0_word(baseAddr, 4) & 0x00FFFFFFu;
  cook_xy(gp0_short(baseAddr, 10), gp0_short(baseAddr, 11), v[2].x, v[2].y);
  v[2].texcoord = 0;

  v[3].color = gp0_word(baseAddr, 6) & 0x00FFFFFFu;
  cook_xy(gp0_short(baseAddr, 14), gp0_short(baseAddr, 15), v[3].x, v[3].y);
  v[3].texcoord = 0;

  drv->EmitPolygon(false, true, true, false, is_transparent(cmd), v, 0, 0);
}

/* --- 0x34..0x37 PolyGT3 ------------------------------------------ */
static void duck_primPolyGT3(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const bool raw = is_raw_texture(cmd);

  /* Upstream: when shade-texture is enabled (raw==false), colours are
   * modulated; when raw, force neutral so texels pass through. */
  const u32 col0_force = raw ? 0x00808080u : 0;

  GPU_Duck_Driver::DuckVertex v[3];
  v[0].color = raw ? col0_force : (cmd & 0x00FFFFFFu);
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), v[0].x, v[0].y);
  v[0].texcoord = read_le_u16(baseAddr + 2 * 4);
  const u32 palette = gp0_word(baseAddr, 2) >> 16;

  v[1].color = raw ? col0_force : (gp0_word(baseAddr, 3) & 0x00FFFFFFu);
  cook_xy(gp0_short(baseAddr, 8), gp0_short(baseAddr, 9), v[1].x, v[1].y);
  v[1].texcoord = read_le_u16(baseAddr + 5 * 4);
  const u32 texpage = gp0_word(baseAddr, 5) >> 16;

  v[2].color = raw ? col0_force : (gp0_word(baseAddr, 6) & 0x00FFFFFFu);
  cook_xy(gp0_short(baseAddr, 14), gp0_short(baseAddr, 15), v[2].x, v[2].y);
  v[2].texcoord = read_le_u16(baseAddr + 8 * 4);

  drv->EmitPolygon(true, true, false, raw, is_transparent(cmd), v, texpage, palette);
}

/* --- 0x3C..0x3F PolyGT4 ------------------------------------------ */
static void duck_primPolyGT4(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const bool raw = is_raw_texture(cmd);
  const u32 col0_force = raw ? 0x00808080u : 0;

  GPU_Duck_Driver::DuckVertex v[4];
  v[0].color = raw ? col0_force : (cmd & 0x00FFFFFFu);
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), v[0].x, v[0].y);
  v[0].texcoord = read_le_u16(baseAddr + 2 * 4);
  const u32 palette = gp0_word(baseAddr, 2) >> 16;

  v[1].color = raw ? col0_force : (gp0_word(baseAddr, 3) & 0x00FFFFFFu);
  cook_xy(gp0_short(baseAddr, 8), gp0_short(baseAddr, 9), v[1].x, v[1].y);
  v[1].texcoord = read_le_u16(baseAddr + 5 * 4);
  const u32 texpage = gp0_word(baseAddr, 5) >> 16;

  v[2].color = raw ? col0_force : (gp0_word(baseAddr, 6) & 0x00FFFFFFu);
  cook_xy(gp0_short(baseAddr, 14), gp0_short(baseAddr, 15), v[2].x, v[2].y);
  v[2].texcoord = read_le_u16(baseAddr + 8 * 4);

  v[3].color = raw ? col0_force : (gp0_word(baseAddr, 9) & 0x00FFFFFFu);
  cook_xy(gp0_short(baseAddr, 20), gp0_short(baseAddr, 21), v[3].x, v[3].y);
  v[3].texcoord = read_le_u16(baseAddr + 11 * 4);

  drv->EmitPolygon(true, true, true, raw, is_transparent(cmd), v, texpage, palette);
}

/* --- 0x40..0x43 LineF2 (single flat segment) --------------------- */
static void duck_primLineF2(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const u32 col = cmd & 0x00FFFFFFu;

  GPU_Duck_Driver::DuckVertex v[2];
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), v[0].x, v[0].y);
  cook_xy(gp0_short(baseAddr, 4), gp0_short(baseAddr, 5), v[1].x, v[1].y);
  v[0].color = v[1].color = col;
  v[0].texcoord = v[1].texcoord = 0;

  drv->EmitLine(false, is_transparent(cmd), v, 2);
}

/* --- 0x50..0x53 LineG2 (single gouraud segment) ------------------ */
static void duck_primLineG2(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);

  GPU_Duck_Driver::DuckVertex v[2];
  v[0].color = cmd & 0x00FFFFFFu;
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), v[0].x, v[0].y);
  v[0].texcoord = 0;

  v[1].color = gp0_word(baseAddr, 2) & 0x00FFFFFFu;
  cook_xy(gp0_short(baseAddr, 6), gp0_short(baseAddr, 7), v[1].x, v[1].y);
  v[1].texcoord = 0;

  drv->EmitLine(true, is_transparent(cmd), v, 2);
}

/* --- 0x48..0x4F LineFEx (flat polyline) -------------------------- */
static void duck_primLineFEx(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const u32 col = cmd & 0x00FFFFFFu;

  /* Accumulate up to 255 vertices, terminated by the PSX 0x55555555
   * polyline terminator (any word matching pattern 0x5xxx5xxx with the
   * top nibbles both 5). gpu.c's accumulator will have already stopped
   * at it; we just need to find where to stop reading. */
  GPU_Duck_Driver::DuckVertex v[256];
  u32 count = 0;
  /* First vertex is word 1. */
  for (u32 w = 1; w < 255 && count < 256; ++w)
  {
    const u32 word = gp0_word(baseAddr, w);
    if ((word & 0xF000F000u) == 0x50005000u && w >= 2)
      break;
    cook_xy(static_cast<s16>(word & 0xFFFFu), static_cast<s16>((word >> 16) & 0xFFFFu),
            v[count].x, v[count].y);
    v[count].color = col;
    v[count].texcoord = 0;
    ++count;
  }

  if (count >= 2)
    drv->EmitLine(false, is_transparent(cmd), v, count);
}

/* --- 0x58..0x5F LineGEx (gouraud polyline) ----------------------- */
static void duck_primLineGEx(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);

  GPU_Duck_Driver::DuckVertex v[256];
  u32 count = 0;
  /* Word 0: cmd+color0. Vertex 0 at word 1. Then (color,vertex) pairs.
   * Terminator is a colour-slot word with 0x5xxx5xxx bit pattern. */
  u32 col = cmd & 0x00FFFFFFu;
  for (u32 w = 1; w < 255 && count < 256; )
  {
    const u32 vword = gp0_word(baseAddr, w);
    cook_xy(static_cast<s16>(vword & 0xFFFFu), static_cast<s16>((vword >> 16) & 0xFFFFu),
            v[count].x, v[count].y);
    v[count].color = col;
    v[count].texcoord = 0;
    ++count;
    ++w;

    if (w >= 255)
      break;
    const u32 nword = gp0_word(baseAddr, w);
    if ((nword & 0xF000F000u) == 0x50005000u && count >= 2)
      break;
    col = nword & 0x00FFFFFFu;
    ++w;
  }

  if (count >= 2)
    drv->EmitLine(true, is_transparent(cmd), v, count);
}

/* --- 0x60..0x63 TileS (variable-size untextured rect) ------------ */
static void duck_primTileS(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  s32 x, y;
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), x, y);
  const u16 w = static_cast<u16>(static_cast<u16>(gp0_short(baseAddr, 4)) & 0x3FFu);
  const u16 h = static_cast<u16>(static_cast<u16>(gp0_short(baseAddr, 5)) & 0x1FFu);
  drv->EmitRectangle(x, y, w, h, cmd & 0x00FFFFFFu, 0, 0, false, false, is_transparent(cmd));
}

/* --- 0x68..0x6B Tile1 (1x1) -------------------------------------- */
static void duck_primTile1(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  s32 x, y;
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), x, y);
  drv->EmitRectangle(x, y, 1, 1, cmd & 0x00FFFFFFu, 0, 0, false, false, is_transparent(cmd));
}

/* --- 0x70..0x73 Tile8 (8x8) -------------------------------------- */
static void duck_primTile8(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  s32 x, y;
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), x, y);
  drv->EmitRectangle(x, y, 8, 8, cmd & 0x00FFFFFFu, 0, 0, false, false, is_transparent(cmd));
}

/* --- 0x78..0x7B Tile16 (16x16) ----------------------------------- */
static void duck_primTile16(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  s32 x, y;
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), x, y);
  drv->EmitRectangle(x, y, 16, 16, cmd & 0x00FFFFFFu, 0, 0, false, false, is_transparent(cmd));
}

/* --- 0x64..0x67 SprtS (variable-size textured sprite) ------------ */
static void duck_primSprtS(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const bool raw = is_raw_texture(cmd);
  const u32 col = raw ? 0x00808080u : (cmd & 0x00FFFFFFu);

  s32 x, y;
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), x, y);
  const u16 uv = read_le_u16(baseAddr + 2 * 4);
  const u32 palette = gp0_word(baseAddr, 2) >> 16;
  const u16 w = static_cast<u16>(static_cast<u16>(gp0_short(baseAddr, 6)) & 0x3FFu);
  const u16 h = static_cast<u16>(static_cast<u16>(gp0_short(baseAddr, 7)) & 0x1FFu);
  drv->EmitRectangle(x, y, w, h, col, uv, palette, true, raw, is_transparent(cmd));
}

/* --- 0x74..0x77 Sprt8 (8x8 textured) ----------------------------- */
static void duck_primSprt8(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const bool raw = is_raw_texture(cmd);
  const u32 col = raw ? 0x00808080u : (cmd & 0x00FFFFFFu);

  s32 x, y;
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), x, y);
  const u16 uv = read_le_u16(baseAddr + 2 * 4);
  const u32 palette = gp0_word(baseAddr, 2) >> 16;
  drv->EmitRectangle(x, y, 8, 8, col, uv, palette, true, raw, is_transparent(cmd));
}

/* --- 0x7C..0x7F Sprt16 (16x16 textured) -------------------------- */
static void duck_primSprt16(unsigned char* baseAddr)
{
  GPU_Duck_Driver* drv = duck_get_driver();
  if (!drv) return;
  const u32 cmd = gp0_word(baseAddr, 0);
  const bool raw = is_raw_texture(cmd);
  const u32 col = raw ? 0x00808080u : (cmd & 0x00FFFFFFu);

  s32 x, y;
  cook_xy(gp0_short(baseAddr, 2), gp0_short(baseAddr, 3), x, y);
  const u16 uv = read_le_u16(baseAddr + 2 * 4);
  const u32 palette = gp0_word(baseAddr, 2) >> 16;
  drv->EmitRectangle(x, y, 16, 16, col, uv, palette, true, raw, is_transparent(cmd));
}

/* --- Unhandled opcode (the vast majority of the 256 slots). ------ */
static void duck_primNI(unsigned char* /*baseAddr*/)
{
}

} /* anonymous namespace */

/* ==================================================================
 * 256-entry dispatch table. Every four-opcode block in the PSX GP0
 * spec shares a single handler — the low two bits of the command byte
 * are per-primitive flags (semi-transparent / raw-texture) that the
 * handler re-extracts from the full command word.
 * ================================================================ */
extern "C" void (*const duck_primTable[256])(unsigned char*) =
{
  /* 0x00 */ duck_primNI, duck_primNI, duck_primBlkFill, duck_primNI,
  /* 0x04 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x08 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x0C */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0x10 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x14 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x18 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x1C */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0x20 */ duck_primPolyF3,  duck_primPolyF3,  duck_primPolyF3,  duck_primPolyF3,
  /* 0x24 */ duck_primPolyFT3, duck_primPolyFT3, duck_primPolyFT3, duck_primPolyFT3,
  /* 0x28 */ duck_primPolyF4,  duck_primPolyF4,  duck_primPolyF4,  duck_primPolyF4,
  /* 0x2C */ duck_primPolyFT4, duck_primPolyFT4, duck_primPolyFT4, duck_primPolyFT4,

  /* 0x30 */ duck_primPolyG3,  duck_primPolyG3,  duck_primPolyG3,  duck_primPolyG3,
  /* 0x34 */ duck_primPolyGT3, duck_primPolyGT3, duck_primPolyGT3, duck_primPolyGT3,
  /* 0x38 */ duck_primPolyG4,  duck_primPolyG4,  duck_primPolyG4,  duck_primPolyG4,
  /* 0x3C */ duck_primPolyGT4, duck_primPolyGT4, duck_primPolyGT4, duck_primPolyGT4,

  /* 0x40 */ duck_primLineF2,  duck_primLineF2,  duck_primLineF2,  duck_primLineF2,
  /* 0x44 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x48 */ duck_primLineFEx, duck_primLineFEx, duck_primLineFEx, duck_primLineFEx,
  /* 0x4C */ duck_primLineFEx, duck_primLineFEx, duck_primLineFEx, duck_primLineFEx,

  /* 0x50 */ duck_primLineG2,  duck_primLineG2,  duck_primLineG2,  duck_primLineG2,
  /* 0x54 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x58 */ duck_primLineGEx, duck_primLineGEx, duck_primLineGEx, duck_primLineGEx,
  /* 0x5C */ duck_primLineGEx, duck_primLineGEx, duck_primLineGEx, duck_primLineGEx,

  /* 0x60 */ duck_primTileS,  duck_primTileS,  duck_primTileS,  duck_primTileS,
  /* 0x64 */ duck_primSprtS,  duck_primSprtS,  duck_primSprtS,  duck_primSprtS,
  /* 0x68 */ duck_primTile1,  duck_primTile1,  duck_primTile1,  duck_primTile1,
  /* 0x6C */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0x70 */ duck_primTile8,  duck_primTile8,  duck_primTile8,  duck_primTile8,
  /* 0x74 */ duck_primSprt8,  duck_primSprt8,  duck_primSprt8,  duck_primSprt8,
  /* 0x78 */ duck_primTile16, duck_primTile16, duck_primTile16, duck_primTile16,
  /* 0x7C */ duck_primSprt16, duck_primSprt16, duck_primSprt16, duck_primSprt16,

  /* 0x80 */ duck_primMoveImage, duck_primNI, duck_primNI, duck_primNI,
  /* 0x84 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x88 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x8C */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0x90 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x94 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x98 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0x9C */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0xA0 */ duck_primLoadImage, duck_primNI, duck_primNI, duck_primNI,
  /* 0xA4 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xA8 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xAC */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0xB0 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xB4 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xB8 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xBC */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0xC0 */ duck_primStoreImage, duck_primNI, duck_primNI, duck_primNI,
  /* 0xC4 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xC8 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xCC */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0xD0 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xD4 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xD8 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xDC */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0xE0 */ duck_primNI, duck_cmdTexturePage, duck_cmdTextureWindow, duck_cmdDrawAreaTL,
  /* 0xE4 */ duck_cmdDrawAreaBR, duck_cmdDrawOffset, duck_cmdSTP, duck_primNI,
  /* 0xE8 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xEC */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,

  /* 0xF0 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xF4 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xF8 */ duck_primNI, duck_primNI, duck_primNI, duck_primNI,
  /* 0xFC */ duck_primNI, duck_primNI, duck_primNI, duck_primNI
};

/* ==================================================================
 * C entry points.
 * ================================================================ */
extern "C" {

/* Runtime selector read by xbox_soft's gpu.c. Default off so nothing
 * changes in builds that haven't been explicitly opted in. Toggled by
 * the pcsxr360_gpu_plugin libretro option. */
int duck_gpu_enabled = 0;

int duck_init(unsigned short* psx_vram)
{
  if (s_driver)
    return 1;
  GPU_Duck_Driver* d = new GPU_Duck_Driver();
  if (!d->Initialize(psx_vram))
  {
    delete d;
    return 0;
  }
  /* s_driver is set by Initialize on success. */
  return 1;
}

void duck_shutdown(void)
{
  if (s_driver)
  {
    GPU_Duck_Driver* d = s_driver;
    d->Shutdown();
    delete d;
    /* Shutdown clears s_driver. */
  }
}

void duck_reset(void)
{
  if (s_driver)
    s_driver->Reset();
}

void duck_set_interlaced(int enabled, int active_line_lsb)
{
  if (s_driver)
    s_driver->SetInterlaced(enabled != 0, static_cast<u8>(active_line_lsb & 1));
}

} /* extern "C" */
